#include "led.h"
//#include "key.h"
#include "delay.h"
#include "sys.h"
#include "usart.h"
#include "exti.h"
#include "nebulizer.h"
#include "rtc.h"
#include "sim800c.h"
#include "usart2.h"
#include "stmflash.h"
#include "utils.h"
#include "includes.h"


//UCOSIII中以下优先级用户程序不能使用，ALIENTEK
//将这些优先级分配给了UCOSIII的5个系统内部任务
//优先级0：中断服务服务管理任务 OS_IntQTask()
//优先级1：时钟节拍任务 OS_TickTask()
//优先级2：定时任务 OS_TmrTask()
//优先级OS_CFG_PRIO_MAX-2：统计任务 OS_StatTask()
//优先级OS_CFG_PRIO_MAX-1：空闲任务 OS_IdleTask()
//技术支持：www.openedv.com
//淘宝店铺：http://eboard.taobao.com  
//广州市星翼电子科技有限公司  
//作者：正点原子 @ALIENTEK

//任务优先级
#define START_TASK_PRIO		3
//任务堆栈大小	
#define START_STK_SIZE 		512
//任务控制块
OS_TCB StartTaskTCB;
//任务堆栈	
CPU_STK START_TASK_STK[START_STK_SIZE];
//任务函数
void start_task(void *p_arg);

//任务优先级
#define KEYSCAN_TASK_PRIO		4
//任务堆栈大小	
#define KEYSCAN_STK_SIZE 		128
//任务控制块
OS_TCB KeyScanTaskTCB;
//任务堆栈	
CPU_STK KEYSCAN_TASK_STK[KEYSCAN_STK_SIZE];
//任务函数
void keyscan_task(void *p_arg);

//任务优先级
#define MAIN_TASK_PRIO		5
//任务堆栈大小	
#define MAIN_STK_SIZE 		384
//任务控制块
OS_TCB MainTaskTCB;
//任务堆栈	
CPU_STK MAIN_TASK_STK[MAIN_STK_SIZE];
void main_task(void *p_arg);

//任务优先级
#define MODEM_TASK_PRIO		6
//任务堆栈大小	
#define MODEM_STK_SIZE 		512
//任务控制块
OS_TCB ModemTaskTCB;
//任务堆栈	
CPU_STK MODEM_TASK_STK[MODEM_STK_SIZE];
void modem_task(void *p_arg);

////////////////////////事件标志组//////////////////////////////
OS_FLAG_GRP	KeyEventFlags;
OS_TMR tmr1;	//定义一个定时器
OS_TMR tmr_gprs_resend; 
OS_TMR tmr_heartbeat;
void tmr1_callback(void *p_tmr,void *p_arg); //定时器1回调函数
void tmr_gprs_resend_callback(void *p_tmr,void *p_arg); //定时器1回调函数


OS_MUTEX	FLASH_MUTEX;

OS_MEM MESSAGE_MEM;	
//存储区中存储块数量
#define MESSAGE_MEM_NUM		20
//每个存储块大小
//由于一个指针变量占用4字节所以块的大小一定要为4的倍数
//而且必须大于一个指针变量(4字节)占用的空间,否则的话存储块创建不成功
#define MESSAGE_MEMBLOCK_SIZE	8  
//存储区的内存池，使用内部RAM
CPU_INT08U Message_RamMemp[MESSAGE_MEM_NUM][MESSAGE_MEMBLOCK_SIZE];

OS_MEM GPRS_MEM;	
//存储区中存储块数量
#define GPRS_MEM_NUM		4
//每个存储块大小
//由于一个指针变量占用4字节所以块的大小一定要为4的倍数
//而且必须大于一个指针变量(4字节)占用的空间,否则的话存储块创建不成功
#define GPRS_MEMBLOCK_SIZE	108  
//存储区的内存池，使用内部RAM
CPU_INT08U Gprs_RamMemp[GPRS_MEM_NUM][GPRS_MEMBLOCK_SIZE];


u16 useCount = 0;

u8 g_gprsWorkFlag = 0;


u8 iccidInfo[20] = {0};
u8 imeiInfo[16] = {0};




#define INFO_TRUNK_SIZE      64
#define GPRS_SAVE_INFO_START_ADDR  0x08030000
#define GRPS_SAVE_INFO_END_ADDR    0x08030800
#define GRPS_SAVE_START_ADDR       0x08030800
#define GPRS_SAVE_END_ADDR         0x08032800
#define GPRS_SAVE_COUNT_START_ADDR 0x08032800
#define GPRS_SAVE_COUNT_END_ADDR   0x08033000

u8 testFlag = 0;

	
//main_task
//1.Handle the key interrupt
//2.combine the data in certain format
//3.inform modem task send data
void main_task(void *p_arg)
{
	OS_ERR err;
	u8 deviceWorking = 0;
	u8 *pMsg;
	OS_MSG_SIZE size;

	CPU_INT08U *gprs_Buf;
	p_arg = p_arg;
	
	RTC_Init();
	
	testFlag = 0;
	
	
	while(1)
	{
		printf("[%d]main task wait ...\r\n", OSTimeGet(&err));
		
		/*
		flags = OSFlagPend((OS_FLAG_GRP*)&EventFlags,
		       (OS_FLAGS	)KEYWKUP_HIGH_FLAG + KEYWKUP_LOW_FLAG + RTC_UPDATE_FLAG,
		     	 (OS_TICK   )0,
				   (OS_OPT	  )OS_OPT_PEND_FLAG_SET_ANY+OS_OPT_PEND_FLAG_CONSUME,
				   (CPU_TS*   )0,
				   (OS_ERR*	  )&err);
		printf("[%d]main task notified with %x\r\n", OSTimeGet(&err), flags);
		*/
		pMsg=OSTaskQPend((OS_TICK		)0,
                      (OS_OPT		)OS_OPT_PEND_BLOCKING,
                      (OS_MSG_SIZE*	)&size,
                      (CPU_TS*		)0,
                      (OS_ERR*      )&err );
		
		if(err != OS_ERR_NONE){
			printf("main tsk pend wrong %x \r\n", err);
		  continue;
		}
		
		printf("[%d]main task notified by %x, size %d, msg queue left %d\r\n", OSTimeGet(&err), *pMsg, size, check_mainTaskMsg_queue());
		
		switch(*pMsg){
		  case KEYWKUP_LOW_FLAG:
			{
			  if(deviceWorking == 0){
				  deviceWorking = 1;
					LED0 =0;
					gprs_Buf=OSMemGet((OS_MEM*)&GPRS_MEM, (OS_ERR*)&err);
					if(gprs_Buf != 0){
					  printf("gprs_buf allocate %x \r\n", (u32)gprs_Buf);
			      memset(gprs_Buf, 0, GPRS_MEMBLOCK_SIZE);
			      useCount++;
						//OSMutexPend (&NTP_MUTEX,0,OS_OPT_PEND_BLOCKING,0,&err);
						gprs_Buf[0]=GPRS_BOOTUP_FLAG;
			      gprs_Buf[1]=0xAA;
			      gprs_Buf[2]=0x04;
						gprs_Buf[3]=0x00;
						gprs_Buf[4]=0x64;//100bytes follow
			      gprs_Buf[5]=(useCount & 0xFF00) >>8;
			      gprs_Buf[6]=(useCount & 0x00FF);
						gprs_Buf[7]=0x05; // 2nd generation
						memset(gprs_Buf+8, 0x30, 20);
			      memcpy(gprs_Buf+28, iccidInfo, 20);
			      memcpy(gprs_Buf+48, imeiInfo, 15);
		        gprs_Buf[63] = 0x60; //battery
						
            gprs_Buf[99] = 0x00; 
						gprs_Buf[100] = 0x00;
						gprs_Buf[101] = 0x00;
						gprs_Buf[102] = 0x00;
						gprs_Buf[103] = 0x00;
						//OSMutexPost(&NTP_MUTEX,OS_OPT_POST_NONE,&err);
			      printf("gprs buf %s \r\n", gprs_Buf+8);
						postGprsSendMessage(&gprs_Buf);
				  }else{
						deviceWorking = 0;
						LED0 = 1;
					  printf("key start ooo GPRS memeory \r\n");
					}
				}
				break;
			}
			case KEYWKUP_HIGH_FLAG:
			{
				if(deviceWorking == 1){
				  deviceWorking = 0;
		      LED1 = 0;
					postCloseGprsConnMessage();
  			}
				break;
			}
			case TIMER_FLAG :
			{
				if(testFlag == 0){
				  postKeyEvent(KEYWKUP_LOW_FLAG);
					testFlag = 1;
					OSTmrStart(&tmr1,&err);
				}else if(testFlag == 1){
				  postKeyEvent(KEYWKUP_HIGH_FLAG);
					testFlag = 0;
					OSTmrStart(&tmr1,&err);
					//printf("try once only");
				}
				
				break;
			}
			
			case KEY0_FLAG:
			{
				postKeyEvent(KEYWKUP_LOW_FLAG);
	      OSTmrStart(&tmr1,&err);
				testFlag = 1;
				//postGprsSendSaveDataMessage();
				break;
			}
			case KEY1_FLAG:
			{
				//CPU_INT08U *buf;
				printf("key 1 pressed \r\n");
				
				//buf=OSMemGet((OS_MEM*)&INTERNAL_MEM, (OS_ERR*)&err);
				//if(g_ntpUpdateFlag == 0){
				//   ntpProcess(buf);
				//}
				//OSMemPut((OS_MEM*	)&INTERNAL_MEM, (void*)buf, (OS_ERR*)&err);
				//OSTmrStart(&tmr_loopsend, &err);
				/*
				gprs_Buf=OSMemGet((OS_MEM*)&GPRS_MEM, (OS_ERR*)&err);
				for(i = 0; i < 90 ; i++){
					*gprs_Buf = i;
				  ret = saveToFlash(gprs_Buf);
					if(ret != 0){
					  stopAutoTest();
					}
					OSTimeDlyHMSM(0,0,2,0,OS_OPT_TIME_PERIODIC,&err);
				}
				OSMemPut((OS_MEM*	)&GPRS_MEM, (void*)gprs_Buf, (OS_ERR*)&err);
				*/
				break;
			}
			default:
			{
				printf("what is the fuck %x \r\n", *pMsg);
				break;
			}
		}
		
		OSMemPut((OS_MEM*	)&MESSAGE_MEM, (void*)pMsg, (OS_ERR*)&err);
		
	}
}



void modem_task(void *p_arg)
{
	OS_ERR err;
	u8 ret;
	CPU_INT08U *gprs_hb_Buf;
//	u16 i=0;
	u8 *pMsg;
	OS_MSG_SIZE size;
	
	p_arg = p_arg;

  SIM800Power = 1;	
	
//	OSTimeDlyHMSM(0,0,1,0,OS_OPT_TIME_PERIODIC,&err);
	
//	RTC_Set(2005, 12, 31, 23, 59, 57);
	
	while(checkSIM800HW() != 0){
		OSTimeDlyHMSM(0,0,1,0,OS_OPT_TIME_PERIODIC,&err);
		printf("try SIM800C again \r\n");
	}
			
	LED0 = 1;
	printf("SIM800C onsite \r\n");
	
	ret = getCCID(iccidInfo);
	if(ret != 0){
	  printf("getCCID fail \r\n");
	}
	
	ret = queryImei(imeiInfo);
	if(ret != 0){
	  printf("getImei fail \r\n");
	}
	//printf("ccid %x %x %x %x \r\n", iccidInfo[0], iccidInfo[1], iccidInfo[18], iccidInfo[19]);
	
  while(checkGPRSEnv() != 0){		
    OSTimeDlyHMSM(0,0,1,0,OS_OPT_TIME_PERIODIC,&err);
    printf("try GPRS env again \r\n");
  }
	LED1 = 1;
	SIM800Power = 0;
	
  printf("signal ok, registered, gprs attached \r\n");

  g_gprsWorkFlag = 1;
	

	while(1){
		printf("[%d]modem task wait ...\r\n", OSTimeGet(&err));
		
		pMsg=OSTaskQPend((OS_TICK		)0,
                      (OS_OPT		)OS_OPT_PEND_BLOCKING,
                      (OS_MSG_SIZE*	)&size,
                      (CPU_TS*		)0,
                      (OS_ERR*      )&err );
		
		if(err != OS_ERR_NONE){
			printf("modem tsk pend wrong %x \r\n", err);
		  continue;
		}
		
		printf("[%d]modem task notified by %x, size %d, msg queue left %d\r\n", OSTimeGet(&err), *pMsg, size, check_modemTaskMsg_queue());
		
		switch(*pMsg){
			case GPRS_BOOTUP_FLAG:
			{
			  SIM800_ERROR aterr;
//				u8 flag = *pMsg;
				
			  if(queryCellId(pMsg+65, pMsg+82) == 0){
			    //printf("CellId %s \r\n", buf);
				  printf("gprs buf from ccid %s \r\n", pMsg+8);
			  }else{
			    printf("cellid fail \r\n");
			  }
			
			  pMsg[104] = xorVerify(pMsg+5, 99);
			
			  //sim800c_gprs_transparentMode(gprs_Buf, 100);
			
			  aterr = sim800c_gprs_tcp(pMsg, 105);
				if(aterr == AT_OK){
//					u8 num = 0;
				  LED0 = 1;
					LED1 = 1;
					OSTimeDlyHMSM(0,0,0,200,OS_OPT_TIME_PERIODIC,&err);
					LED0 = 0;
					LED1 = 0;
					OSTimeDlyHMSM(0,0,0,200,OS_OPT_TIME_PERIODIC,&err);
					LED0 = 1;
					LED1 = 1;
					OSTmrStart(&tmr_heartbeat, &err);
					OSMemPut((OS_MEM*	)&GPRS_MEM, (void*)pMsg, (OS_ERR*)&err);
				}else if(aterr == AT_FAIL || aterr == AT_SERVER_RES_TIMEOUT){
					pMsg[0] = GPRS_BOOTUP_TRY_SECOND;
          OSTimeDlyHMSM(0,0,3,0,OS_OPT_TIME_PERIODIC,&err);
					postGprsSendMessage(&pMsg);
				}else{
				  OSTmrStart(&tmr_heartbeat, &err);
					OSMemPut((OS_MEM*	)&GPRS_MEM, (void*)pMsg, (OS_ERR*)&err);
				}
				
				
				break;
			}
			case GPRS_BOOTUP_TRY_SECOND:
			{
				SIM800_ERROR aterr;
				pMsg[0] = 0x55;
				aterr = sim800c_gprs_tcp(pMsg, 105);
				if(aterr == AT_OK){
				  OSTmrStart(&tmr_heartbeat, &err);
					OSMemPut((OS_MEM*	)&GPRS_MEM, (void*)pMsg, (OS_ERR*)&err);
				}else if(aterr == AT_FAIL || aterr == AT_SERVER_RES_TIMEOUT){
					pMsg[0] = GPRS_BOOTUP_TRY_SECOND;
          OSTimeDlyHMSM(0,0,3,0,OS_OPT_TIME_PERIODIC,&err);
					postGprsSendMessage(&pMsg);
				}else{
				  OSTmrStart(&tmr_heartbeat, &err);
					OSMemPut((OS_MEM*	)&GPRS_MEM, (void*)pMsg, (OS_ERR*)&err);
				}
				
				break;
			}
			
			case GPRS_CLOSE_FLAG:{
				OSTmrStop(&tmr_heartbeat,OS_OPT_TMR_NONE,0,&err);
			  closeGrpsConn();
				break;
			}
			case GPRS_HEART_BEAT_FLAG:{
			  gprs_hb_Buf=OSMemGet((OS_MEM*)&GPRS_MEM, (OS_ERR*)&err);
				if(gprs_hb_Buf != 0){
					  printf("gprs_hb_buf allocate %x \r\n", (u32)gprs_hb_Buf);
			      memset(gprs_hb_Buf, 0, GPRS_MEMBLOCK_SIZE);
			      
						gprs_hb_Buf[0] = 0x55;
			      gprs_hb_Buf[1] = 0xAA;
			      gprs_hb_Buf[2] = 0x05;
						gprs_hb_Buf[3] = 0x00;
						gprs_hb_Buf[4] = 0x04;
						gprs_hb_Buf[5] = 0x00;
						gprs_hb_Buf[6] = 0x00;
						gprs_hb_Buf[7] = 0x00;
						gprs_hb_Buf[8] = xorVerify(gprs_hb_Buf+5, 3);
						sim800c_gprs_tcp_sendonly(gprs_hb_Buf, 9);
						OSMemPut((OS_MEM*	)&GPRS_MEM, (void*)gprs_hb_Buf, (OS_ERR*)&err);
						if(err != OS_ERR_NONE){
			        printf("return gprs_hb_Buf fail %d \r\n", err);
		        }
						OSTmrStart(&tmr_heartbeat, &err);
				}
				break;
			}
			default:{
			   break;
			}
		}
		
  }
	
}


void stopAutoTest(void)
{
	OS_ERR err;
  OSTmrStop(&tmr1,OS_OPT_TMR_NONE,0,&err);
	testFlag = 3;
	while(1){
	  LED0 = 1;
		OSTimeDlyHMSM(0,0,1,0,OS_OPT_TIME_PERIODIC,&err);
    LED1 = 1;
	  OSTimeDlyHMSM(0,0,1,0,OS_OPT_TIME_PERIODIC,&err);
		LED0 = 0;
		OSTimeDlyHMSM(0,0,1,0,OS_OPT_TIME_PERIODIC,&err);
		LED1 = 0;
	  OSTimeDlyHMSM(0,0,1,0,OS_OPT_TIME_PERIODIC,&err);
	}
}



u8 check_mainTaskMsg_queue(void)
{
	CPU_SR_ALLOC();
	u8 msgq_remain_size;	//消息队列剩余大小
	OS_CRITICAL_ENTER();	//进入临界段
	msgq_remain_size =MainTaskTCB.MsgQ.NbrEntriesSize-MainTaskTCB.MsgQ.NbrEntries;
	OS_CRITICAL_EXIT();		//退出临界段
	return msgq_remain_size;
}

u8 check_modemTaskMsg_queue(void)
{
	CPU_SR_ALLOC();
	u8 msgq_remain_size;	//消息队列剩余大小
	OS_CRITICAL_ENTER();	//进入临界段
	msgq_remain_size =ModemTaskTCB.MsgQ.NbrEntriesSize-ModemTaskTCB.MsgQ.NbrEntries;
	OS_CRITICAL_EXIT();		//退出临界段
	return msgq_remain_size;
}

void tmr1_callback(void *p_tmr,void *p_arg)
{
	printf("timer 1 tick \r\n");
	postTimerMessage(1);
}

void tmr_gprs_resend_callback(void *p_tmr,void *p_arg)
{
	printf("timer gprs resend tick \r\n");
	//postTimerMessage(2);
}


void tmr_heartbeat_callback(void *p_tmr,void *p_arg)
{
	printf("timer hearbeat tick \r\n");
  postHeartBeatMessage();
}

void postHeartBeatMessage(void){
	OS_ERR err;
	u8 *pBuf;
	
	printf("postHeartBeatMessage \r\n");
	
	pBuf = OSMemGet((OS_MEM*)&MESSAGE_MEM, (OS_ERR*)&err);
	
	if(*pBuf){
	  *pBuf = GPRS_HEART_BEAT_FLAG;
		OSTaskQPost((OS_TCB*	)&ModemTaskTCB,
                    (void*	)pBuf,
                    (OS_MSG_SIZE)MESSAGE_MEMBLOCK_SIZE,
                    (OS_OPT		)OS_OPT_POST_FIFO,
					(OS_ERR*	)&err);
		if(err != OS_ERR_NONE){
			printf("postHeartBeatMessage fail %d \r\n", err);
		  OSMemPut((OS_MEM*	)&MESSAGE_MEM, (void*)(*pBuf), (OS_ERR*)&err);
		}else{
			printf("post postHeartBeat message \r\n");
	  }
	}
}

void postGprsSendMessage(u8 **pBuf){
	OS_ERR err;
	
	//printf("postGprsSendMsg get %x  %x %x\r\n", pBuf, *pBuf, **pBuf);
	
	if(*pBuf){
	  
		OSTaskQPost((OS_TCB*	)&ModemTaskTCB,
                    (void*		)(*pBuf),
                    (OS_MSG_SIZE)GPRS_MEMBLOCK_SIZE,
                    (OS_OPT		)OS_OPT_POST_FIFO,
					(OS_ERR*	)&err);
		if(err != OS_ERR_NONE){
			printf("postGprsSendMessage fail %d \r\n", err);
		  OSMemPut((OS_MEM*	)&GPRS_MEM, (void*)(*pBuf), (OS_ERR*)&err);
		}else{
			printf("post GPRS send message \r\n");
	  }
	}
}


void postKeyEvent(u8 keynum){
	OS_FLAGS flags_num;
	OS_ERR   err;	
	
	if(keynum ==KEY0_FLAG){
		flags_num=OSFlagPost((OS_FLAG_GRP*)&KeyEventFlags,
								 (OS_FLAGS	  )KEY0_FLAG,
								 (OS_OPT	  )OS_OPT_POST_FLAG_SET,
					       (OS_ERR*	  )&err);
	}else if(keynum == KEY1_FLAG){
		flags_num=OSFlagPost((OS_FLAG_GRP*)&KeyEventFlags,
								 (OS_FLAGS	  )KEY1_FLAG,
								 (OS_OPT	  )OS_OPT_POST_FLAG_SET,
					       (OS_ERR*	  )&err);
	}else if(keynum == KEYWKUP_LOW_FLAG){
	  flags_num=OSFlagPost((OS_FLAG_GRP*)&KeyEventFlags,
								 (OS_FLAGS	  )KEYWKUP_LOW_FLAG,
								 (OS_OPT	  )OS_OPT_POST_FLAG_SET,
					       (OS_ERR*	  )&err);
	}else if(keynum == KEYWKUP_HIGH_FLAG){
	  flags_num=OSFlagPost((OS_FLAG_GRP*)&KeyEventFlags,
								 (OS_FLAGS	  )KEYWKUP_HIGH_FLAG,
								 (OS_OPT	  )OS_OPT_POST_FLAG_SET,
					       (OS_ERR*	  )&err);
	}
		
	printf("[%d]post key %d event 0x%x\r\n", keynum, OSTimeGet(&err), flags_num);
}

void postCloseGprsConnMessage(void){
	OS_ERR err;
	u8 *pBuf;
	
	printf("postCloseGprsConnMessage \r\n");
	
	pBuf = OSMemGet((OS_MEM*)&MESSAGE_MEM, (OS_ERR*)&err);
	
	if(*pBuf){
	  *pBuf = GPRS_CLOSE_FLAG;
		OSTaskQPost((OS_TCB*	)&ModemTaskTCB,
                    (void*	)pBuf,
                    (OS_MSG_SIZE)MESSAGE_MEMBLOCK_SIZE,
                    (OS_OPT		)OS_OPT_POST_FIFO,
					(OS_ERR*	)&err);
		if(err != OS_ERR_NONE){
			printf("postCloseGprsConnMessage fail %d \r\n", err);
		  OSMemPut((OS_MEM*	)&MESSAGE_MEM, (void*)(*pBuf), (OS_ERR*)&err);
		}else{
			printf("post CloseGprsConn message \r\n");
	  }
	}
}

void postTimerMessage(u8 timer_num){
	u8 *pbuf;
	OS_ERR err;
	
	pbuf = OSMemGet((OS_MEM*)&MESSAGE_MEM, (OS_ERR*)&err);
		
	if(timer_num == 1){
	  if(pbuf){
		  *pbuf = TIMER_FLAG;
		  OSTaskQPost((OS_TCB*	)&MainTaskTCB,
                    (void*		)pbuf,
                    (OS_MSG_SIZE)MESSAGE_MEMBLOCK_SIZE,
                    (OS_OPT		)OS_OPT_POST_FIFO,
					(OS_ERR*	)&err);
		  if(err != OS_ERR_NONE){
			  OSMemPut((OS_MEM*	)&MESSAGE_MEM, (void*)pbuf, (OS_ERR*)&err);
		  }else{
			  printf("post timer message \r\n");
			}
	  }
	}else if(timer_num == 2){
    if(pbuf){
		  *pbuf = NTP_TIMER_FLAG;
		  OSTaskQPost((OS_TCB*	)&ModemTaskTCB,
                    (void*		)pbuf,
                    (OS_MSG_SIZE)MESSAGE_MEMBLOCK_SIZE,
                    (OS_OPT		)OS_OPT_POST_FIFO,
					(OS_ERR*	)&err);
		  if(err != OS_ERR_NONE){
			  OSMemPut((OS_MEM*	)&MESSAGE_MEM, (void*)pbuf, (OS_ERR*)&err);
		  }else{
			  printf("post ntp timer message \r\n");
			}
    } 
	}
	
	printf("[%d]post timer %d message\r\n", OSTimeGet(&err), timer_num);
}

int main(void)
{
	OS_ERR err;
	CPU_SR_ALLOC();
	
	delay_init();       //延时初始化
	NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2); //中断分组配置
	LED_Init();         //LED初始化
	LED0 = 0;
	LED1 = 0;
	SIM800Power = 0;
	uart_init(115200);    //串口波特率设置
	//KEY_Init();
	EXTIX_Init();
	USART2_Init(115200);
	
	useCount = 0;
	printf("hw init end\r\n");
	
	OSInit(&err);		//初始化UCOSIII
	OS_CRITICAL_ENTER();//进入临界区
	

  OSMemCreate((OS_MEM*	)&MESSAGE_MEM,
				(CPU_CHAR*	)"Message Mem",
				(void*		)&Message_RamMemp[0][0],
				(OS_MEM_QTY	)MESSAGE_MEM_NUM,
				(OS_MEM_SIZE)MESSAGE_MEMBLOCK_SIZE,
				(OS_ERR*	)&err);
				
	OSMemCreate((OS_MEM*	)&GPRS_MEM,
				(CPU_CHAR*	)"Gprs Mem",
				(void*		)&Gprs_RamMemp[0][0],
				(OS_MEM_QTY	)GPRS_MEM_NUM,
				(OS_MEM_SIZE)GPRS_MEMBLOCK_SIZE,
				(OS_ERR*	)&err);
	//创建开始任务
	OSTaskCreate((OS_TCB 	* )&StartTaskTCB,		//任务控制块
				 (CPU_CHAR	* )"start task", 		//任务名字
                 (OS_TASK_PTR )start_task, 			//任务函数
                 (void		* )0,					//传递给任务函数的参数
                 (OS_PRIO	  )START_TASK_PRIO,     //任务优先级
                 (CPU_STK   * )&START_TASK_STK[0],	//任务堆栈基地址
                 (CPU_STK_SIZE)START_STK_SIZE/10,	//任务堆栈深度限位
                 (CPU_STK_SIZE)START_STK_SIZE,		//任务堆栈大小
                 (OS_MSG_QTY  )0,					//任务内部消息队列能够接收的最大消息数目,为0时禁止接收消息
                 (OS_TICK	  )0,					//当使能时间片轮转时的时间片长度，为0时为默认长度，
                 (void   	* )0,					//用户补充的存储区
                 (OS_OPT      )OS_OPT_TASK_STK_CHK|OS_OPT_TASK_STK_CLR, //任务选项
                 (OS_ERR 	* )&err);				//存放该函数错误时的返回值
	OS_CRITICAL_EXIT();	//退出临界区	 
	OSStart(&err);  //开启UCOSIII
	while(1);
}

//开始任务函数
void start_task(void *p_arg)
{
	OS_ERR err;
	CPU_SR_ALLOC();
	p_arg = p_arg;

	CPU_Init();
#if OS_CFG_STAT_TASK_EN > 0u
   OSStatTaskCPUUsageInit(&err);  	//统计任务                
#endif
	
#ifdef CPU_CFG_INT_DIS_MEAS_EN		//如果使能了测量中断关闭时间
   CPU_IntDisMeasMaxCurReset();	
#endif
	
#if	OS_CFG_SCHED_ROUND_ROBIN_EN  //当使用时间片轮转的时候
	 //使能时间片轮转调度功能,时间片长度为1个系统时钟节拍，既1*5=5ms
	OSSchedRoundRobinCfg(DEF_ENABLED,1,&err);  
#endif		
	
	OS_CRITICAL_ENTER();	//进入临界区
	
	OSMutexCreate((OS_MUTEX*	)&FLASH_MUTEX,
				  (CPU_CHAR*	)"FLASH_MUTEX",
                  (OS_ERR*		)&err);
									
	
	OSFlagCreate((OS_FLAG_GRP*)&KeyEventFlags,		//指向事件标志组
                 (CPU_CHAR*	  )"Key Event Flags",	//名字
                 (OS_FLAGS	  )KEYFLAGS_VALUE,	//事件标志组初始值
                 (OS_ERR*  	  )&err);			//错误码
								 
	OSTmrCreate((OS_TMR		*)&tmr_gprs_resend,		//定时器1
                (CPU_CHAR	*)"tmr ntp",		//定时器名字
                (OS_TICK	 )300,			//300* 10 =3s
                (OS_TICK	 )0,          //
                (OS_OPT		 )OS_OPT_TMR_ONE_SHOT,
                (OS_TMR_CALLBACK_PTR)tmr_gprs_resend_callback,//定时器1回调函数
                (void	    *)0,			//参数为0
                (OS_ERR	    *)&err);		//返回的错误码

	OSTmrCreate((OS_TMR		*)&tmr_heartbeat,		//
                (CPU_CHAR	*)"time heart beat",
                (OS_TICK	 )3000,			//3000* 10 =30s
                (OS_TICK	 )0,          //
                (OS_OPT		 )OS_OPT_TMR_ONE_SHOT,
                (OS_TMR_CALLBACK_PTR)tmr_heartbeat_callback,//定时器1回调函数
                (void	    *)0,			//参数为0
                (OS_ERR	    *)&err);		//返回的错误码		
								
/*
	OSTmrCreate((OS_TMR		*)&tmr1,		//定时器1
                (CPU_CHAR	*)"tmr1",		//定时器名字
                (OS_TICK	 )0,			//0ms
                (OS_TICK	 )500,          //200*10=2000ms
                (OS_OPT		 )OS_OPT_TMR_PERIODIC, //周期模式
                (OS_TMR_CALLBACK_PTR)tmr1_callback,//定时器1回调函数
                (void	    *)0,			//参数为0
                (OS_ERR	    *)&err);		//返回的错误码
*/
	OSTmrCreate((OS_TMR		*)&tmr1,		//定时器1
                (CPU_CHAR	*)"tmr1",		//定时器名字
                (OS_TICK	 )6000,			//0ms
                (OS_TICK	 )0,        
                (OS_OPT		 )OS_OPT_TMR_ONE_SHOT,
                (OS_TMR_CALLBACK_PTR)tmr1_callback,//定时器1回调函数
                (void	    *)0,			//参数为0
                (OS_ERR	    *)&err);		//返回的错误码								
								
	OSTaskCreate((OS_TCB 	* )&KeyScanTaskTCB,		
				 (CPU_CHAR	* )"key scan task", 		
                 (OS_TASK_PTR )keyscan_task, 			
                 (void		* )0,					
                 (OS_PRIO	  )KEYSCAN_TASK_PRIO,     
                 (CPU_STK   * )&KEYSCAN_TASK_STK[0],	
                 (CPU_STK_SIZE)KEYSCAN_STK_SIZE/10,	
                 (CPU_STK_SIZE)KEYSCAN_STK_SIZE,		
                 (OS_MSG_QTY  )0,					
                 (OS_TICK	  )0,					
                 (void   	* )0,					
                 (OS_OPT      )OS_OPT_TASK_STK_CHK|OS_OPT_TASK_STK_CLR,
                 (OS_ERR 	* )&err);
								 
	OSTaskCreate((OS_TCB 	* )&MainTaskTCB,		
				 (CPU_CHAR	* )"main task", 		
                 (OS_TASK_PTR )main_task, 			
                 (void		* )0,					
                 (OS_PRIO	  )MAIN_TASK_PRIO,     
                 (CPU_STK   * )&MAIN_TASK_STK[0],	
                 (CPU_STK_SIZE)MAIN_STK_SIZE/10,	
                 (CPU_STK_SIZE)MAIN_STK_SIZE,		
                 (OS_MSG_QTY  )TASK_Q_NUM,					
                 (OS_TICK	  )0,					
                 (void   	* )0,					
                 (OS_OPT      )OS_OPT_TASK_STK_CHK|OS_OPT_TASK_STK_CLR,
                 (OS_ERR 	* )&err);				
				 
	//
	OSTaskCreate((OS_TCB 	* )&ModemTaskTCB,		
				 (CPU_CHAR	* )"modem task", 		
                 (OS_TASK_PTR )modem_task, 			
                 (void		* )0,					
                 (OS_PRIO	  )MODEM_TASK_PRIO,     	
                 (CPU_STK   * )&MODEM_TASK_STK[0],	
                 (CPU_STK_SIZE)MODEM_STK_SIZE/10,	
                 (CPU_STK_SIZE)MODEM_STK_SIZE,		
                 (OS_MSG_QTY  )MODEMTASK_Q_NUM,					
                 (OS_TICK	  )0,					
                 (void   	* )0,				
                 (OS_OPT      )OS_OPT_TASK_STK_CHK|OS_OPT_TASK_STK_CLR, 
                 (OS_ERR 	* )&err);	
	
	OS_CRITICAL_EXIT();	//进入临界区
  OSTaskDel((OS_TCB*)0,&err);
}

void keyscan_task(void *p_arg){
	OS_ERR err;
	OS_FLAGS flags;
	u8 *pbuf;
	p_arg = p_arg;
	
	//OSTmrStart(&tmr1,&err);
	
	while(1){
		printf("keyscan_task wait ... \r\n");
	  flags = OSFlagPend((OS_FLAG_GRP*)&KeyEventFlags,
		       (OS_FLAGS	)KEYWKUP_HIGH_FLAG + KEYWKUP_LOW_FLAG + KEY0_FLAG + KEY1_FLAG,
		     	 (OS_TICK   )0,
				   (OS_OPT	  )OS_OPT_PEND_FLAG_SET_ANY+OS_OPT_PEND_FLAG_CONSUME,
				   (CPU_TS*   )0,
				   (OS_ERR*	  )&err);
	
	  pbuf = OSMemGet((OS_MEM*)&MESSAGE_MEM, (OS_ERR*)&err);
	
	  if(flags & KEYWKUP_HIGH_FLAG){
	    if(pbuf){
		    *pbuf = KEYWKUP_HIGH_FLAG;
		    OSTaskQPost((OS_TCB*	)&MainTaskTCB,
                    (void*		)pbuf,
                    (OS_MSG_SIZE)MESSAGE_MEMBLOCK_SIZE,
                    (OS_OPT		)OS_OPT_POST_FIFO,
					(OS_ERR*	)&err);
		    if(err != OS_ERR_NONE){
			    OSMemPut((OS_MEM*	)&MESSAGE_MEM, (void*)pbuf, (OS_ERR*)&err);
		    }else{
			    printf("post wk up key relese message \r\n");
			  }
	    }			
	  }else if(flags & KEYWKUP_LOW_FLAG){
	    if(pbuf){
		    *pbuf = KEYWKUP_LOW_FLAG;
		    OSTaskQPost((OS_TCB*	)&MainTaskTCB,
                    (void*		)pbuf,
                    (OS_MSG_SIZE)MESSAGE_MEMBLOCK_SIZE,
                    (OS_OPT		)OS_OPT_POST_FIFO,
					(OS_ERR*	)&err);
		    if(err != OS_ERR_NONE){
			    OSMemPut((OS_MEM*	)&MESSAGE_MEM, (void*)pbuf, (OS_ERR*)&err);
		    }else{
			    printf("post wk low key relese message \r\n");
			  }
	    }			
	  }else if(flags & KEY0_FLAG){
	    if(pbuf){
		    *pbuf = KEY0_FLAG;
		    OSTaskQPost((OS_TCB*	)&MainTaskTCB,
                    (void*		)pbuf,
                    (OS_MSG_SIZE)MESSAGE_MEMBLOCK_SIZE,
                    (OS_OPT		)OS_OPT_POST_FIFO,
					(OS_ERR*	)&err);
		    if(err != OS_ERR_NONE){
			    OSMemPut((OS_MEM*	)&MESSAGE_MEM, (void*)pbuf, (OS_ERR*)&err);
		    }else{
			    printf("post key0 key relese message \r\n");
			  }
	    }			
	  }else if(flags & KEY1_FLAG){
	    if(pbuf){
		    *pbuf = KEY1_FLAG;
		    OSTaskQPost((OS_TCB*	)&MainTaskTCB,
                    (void*		)pbuf,
                    (OS_MSG_SIZE)MESSAGE_MEMBLOCK_SIZE,
                    (OS_OPT		)OS_OPT_POST_FIFO,
					(OS_ERR*	)&err);
		    if(err != OS_ERR_NONE){
			    OSMemPut((OS_MEM*	)&MESSAGE_MEM, (void*)pbuf, (OS_ERR*)&err);
		    }else{
			    printf("post key1 relese message \r\n");
			  }
	    }
	  }else{
	    OSMemPut((OS_MEM*	)&MESSAGE_MEM, (void*)pbuf, (OS_ERR*)&err);
	  }
  }
}

