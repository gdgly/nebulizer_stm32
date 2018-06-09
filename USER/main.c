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
OS_TMR tmr_ntp; 
OS_TMR tmr_loopsend;
void tmr1_callback(void *p_tmr,void *p_arg); //定时器1回调函数
void tmr_ntp_callback(void *p_tmr,void *p_arg); //定时器1回调函数


OS_MUTEX	FLASH_MUTEX;

OS_MEM INTERNAL_MEM;	
//存储区中存储块数量
#define INTERNAL_MEM_NUM		3
//每个存储块大小
//由于一个指针变量占用4字节所以块的大小一定要为4的倍数
//而且必须大于一个指针变量(4字节)占用的空间,否则的话存储块创建不成功
#define INTERNAL_MEMBLOCK_SIZE	100  
//存储区的内存池，使用内部RAM
CPU_INT08U Internal_RamMemp[INTERNAL_MEM_NUM][INTERNAL_MEMBLOCK_SIZE];


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
#define GPRS_MEM_NUM		8
//每个存储块大小
//由于一个指针变量占用4字节所以块的大小一定要为4的倍数
//而且必须大于一个指针变量(4字节)占用的空间,否则的话存储块创建不成功
#define GPRS_MEMBLOCK_SIZE	100  
//存储区的内存池，使用内部RAM
CPU_INT08U Gprs_RamMemp[GPRS_MEM_NUM][GPRS_MEMBLOCK_SIZE];


u16 useCount = 0;
int g_loopSendMsgNum = 0;
u8 g_ntpUpdateFlag = 0;
u8 g_gprsWorkFlag = 0;
int g_diffTime = 0;

u16 ntp_year;
u8 ntp_month, ntp_day, ntp_hour, ntp_min, ntp_second;

u8 iccidInfo[20] = {0};
u8 startTime[20] = {0};
u8 endTime[20] = {0};


u8 g_oldNumInFlash  = 0;


#define INFO_TRUNK_SIZE      64
#define GPRS_SAVE_INFO_START_ADDR  0x08030000
#define GRPS_SAVE_INFO_END_ADDR    0x08030800
#define GRPS_SAVE_START_ADDR       0x08030800
#define GPRS_SAVE_END_ADDR         0x08032800
#define GPRS_SAVE_COUNT_START_ADDR 0x08032800
#define GPRS_SAVE_COUNT_END_ADDR   0x08033000

u8 testFlag = 0;
//Page n : 2K  => Data Info 8 bytes [Index][DataNum][Xor][DataPtr]
//Page n+1 
// Loop use from page 1 to 14
//Page n+4:
//Page n+5: 2K  => Count Info 4 bytes [Index][CountNum]

//Loop use page 0 to save Index info
//Find the biggest Index to get the latest gprs data info, check the DataNum, if DataNum is 0, DataPtr should be the start ptr of next saving




	
//main_task
//1.Handle the key interrupt
//2.combine the data in certain format
//3.inform modem task send data
void main_task(void *p_arg)
{
	OS_ERR err;
	_calendar_obj start_time;
	_calendar_obj end_time;
	u8 deviceWorking = 0;
	u8 *pMsg;
	OS_MSG_SIZE size;

	CPU_INT08U *gprs_Buf;
	p_arg = p_arg;
	
	RTC_Init();
	
	testFlag = 0;
	
	g_loopSendMsgNum = 0;
	
	if(getNumInFlash(&g_oldNumInFlash) == 0){
	  printf(" %d old record in flash \r\n", g_oldNumInFlash);   			 
			
	}else{
		printf(" boot fail to get num in flash\r\n ");   			  
	  g_oldNumInFlash = 0;
	} 
	
	useCount  = getCountFromFlash();
	
	printf(" useCount from flash is %d\r\n ", useCount);   
	
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
						if(g_ntpUpdateFlag == 1){
			        gprs_Buf[0]=GPRS_NTPOK_SEND_FLAG;
						}else {
						  gprs_Buf[0]=GPRS_NTPFAIL_SEND_FLAG;
						}
			      gprs_Buf[1]=0xAA;
			      gprs_Buf[2]=0x81;
			      gprs_Buf[3]=(useCount & 0xFF00) >>8;
			      gprs_Buf[4]=(useCount & 0x00FF);
			      memcpy(gprs_Buf+5, iccidInfo, 20);
			      
		        RTC_Get();
			      printf("%d/%02d/%02d %02d:%02d:%02d\r\n", calendar.w_year, calendar.w_month,calendar.w_date,calendar.hour,calendar.min,calendar.sec);
			      sprintf((char*)(gprs_Buf+25),"%02d/%02d/%02d,%02d:%02d:%02d+00",calendar.w_year>2000?(calendar.w_year-2000):calendar.w_year,
			      calendar.w_month,calendar.w_date,calendar.hour,calendar.min,calendar.sec);
			      start_time = calendar;
						//OSMutexPost(&NTP_MUTEX,OS_OPT_POST_NONE,&err);
			      printf("gprs buf %s \r\n", gprs_Buf+5);
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
			    if(gprs_Buf != 0){
						//OSMutexPend (&NTP_MUTEX,0,OS_OPT_PEND_BLOCKING,0,&err);
						RTC_Get();
			      printf("%d/%d/%d %d:%d:%d\r\n", calendar.w_year, calendar.w_month,calendar.w_date,calendar.hour,calendar.min,calendar.sec);
						
						if(g_ntpUpdateFlag == 1){
						   if(gprs_Buf[0] == GPRS_NTPOK_SEND_FLAG){
							    printf("start and end time is both ntp time \r\n");
							 }else if(gprs_Buf[0] == GPRS_NTPFAIL_SEND_FLAG){
							    printf("start time is not ntp time, end time is \r\n");
								  if(g_diffTime != 0){
										printf("shift time before %d/%02d/%02d %02d:%02d:%02d\r\n", start_time.w_year, start_time.w_month,start_time.w_date,start_time.hour,start_time.min,start_time.sec);
									  RTC_Shift(g_diffTime, &start_time);
										printf("shift time after  %d/%02d/%02d %02d:%02d:%02d\r\n", start_time.w_year, start_time.w_month,start_time.w_date,start_time.hour,start_time.min,start_time.sec);
										sprintf((char*)(gprs_Buf+25),"%02d/%02d/%02d,%02d:%02d:%02d+00",start_time.w_year>2000?(start_time.w_year-2000):start_time.w_year,
			              start_time.w_month,start_time.w_date,start_time.hour,start_time.min,start_time.sec);
									}
									gprs_Buf[0] = GPRS_NTPOK_SEND_FLAG;
							 }
						}
						
					  sprintf((char*)(gprs_Buf+45),"%02d/%02d/%02d,%02d:%02d:%02d+00",calendar.w_year>2000?(calendar.w_year-2000):calendar.w_year,
			      calendar.w_month,calendar.w_date,calendar.hour,calendar.min,calendar.sec);
			      end_time = calendar;
						
						printf("duration is %d \r\n", diffTimeInSecs(start_time, end_time));
						
						//OSMutexPost(&NTP_MUTEX,OS_OPT_POST_NONE,&err);
			      if(g_gprsWorkFlag == 1){
						   postGprsSendMessage(&gprs_Buf);
						}else{
							 sprintf((char*)(gprs_Buf+65),"%03d,%02d,%04d,%04d,",0,0,0,0);
							 sprintf((char*)(gprs_Buf+82),"%03d,%02d,%04d,%04d,",0,0,0,0);
							 gprs_Buf[99] = xorVerify(gprs_Buf+2, 97);
							 OSMutexPend (&FLASH_MUTEX,0,OS_OPT_PEND_BLOCKING,0,&err);
						   saveToFlash(gprs_Buf);
							 OSMemPut((OS_MEM*	)&GPRS_MEM, (void*)gprs_Buf, (OS_ERR*)&err);
							 OSMutexPost(&FLASH_MUTEX,OS_OPT_POST_NONE,&err);
							 
						}
						OSMutexPend (&FLASH_MUTEX,0,OS_OPT_PEND_BLOCKING,0,&err);
						increaseCntAtFlash();
						OSMutexPost(&FLASH_MUTEX,OS_OPT_POST_NONE,&err);
				  }else{
					  printf("key stop ooo GPRS memeory \r\n");
					}
					
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
			case RTC_UPDATE_FLAG:
			{
			  u16 year;
			  u8 hour;
			  CPU_INT08U *buf;
			  buf=OSMemGet((OS_MEM*)&INTERNAL_MEM, (OS_ERR*)&err);
			  if(fetchNetworkTime(buf) == 0){
					_calendar_obj time1;
					_calendar_obj time2;
			    //printf("%d/%d/%d %d:%d:%d\r\n", buf[0], buf[1],buf[2], buf[3], buf[4], buf[5]);
			    year = 2000 + buf[0];
				  hour = buf[3] + 6;
				  hour = hour >= 24 ? (hour - 24):hour;
					
					time2.w_year = year;
	        time2.w_month = buf[1];
	        time2.w_date = buf[2];
	        time2.hour = hour;
	        time2.min = buf[4];
	        time2.sec = buf[5];
					RTC_Get();
	        time1 = calendar;
					
					g_diffTime = diffTimeInSecs(time1, time2);
					
					printf("time diff is %d \r\n", g_diffTime);
			    RTC_Set(year, buf[1],buf[2], hour, buf[4], buf[5]);
				  printf("ntp set over ok \r\n");
					g_ntpUpdateFlag = 1;
					LED1 = 1;
					
					//postKeyEvent(KEY0_FLAG);
					
			  }else{
				  printf("get ntp time fail ,try again \r\n");
			    OSTimeDlyHMSM(0,0,1,0,OS_OPT_TIME_PERIODIC,&err);
				  postRTCUpdateMessage();
			  }
			
	      OSMemPut((OS_MEM*	)&INTERNAL_MEM, (void*)buf, (OS_ERR*)&err);
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
	CPU_INT08U *buf;
	CPU_INT08U *flashTestBuf;
	u8 ret;
	u16 i=0;
	u8 *pMsg;
	OS_MSG_SIZE size;
	
	p_arg = p_arg;
	
  buf=OSMemGet((OS_MEM*)&INTERNAL_MEM, (OS_ERR*)&err);
	
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
	//printf("ccid %x %x %x %x \r\n", iccidInfo[0], iccidInfo[1], iccidInfo[18], iccidInfo[19]);
	
  while(checkGPRSEnv() != 0){		
    OSTimeDlyHMSM(0,0,1,0,OS_OPT_TIME_PERIODIC,&err);
    printf("try GPRS env again \r\n");
  }
	
	SIM800Power = 0;
	
  printf("signal ok, registered, gprs attached \r\n");

//NTP Test, mark off auto , manullay trigger	
		
	//ntpProcess(buf);
	postTimerMessage(2);
  g_gprsWorkFlag = 1;
	
	if(getNumInFlash(&ret) == 0){
	  printf(" %d num in flash after GPRS attach \r\n", ret);   			 
		if(ret > 0){
		  postGprsSendSaveDataMessage();
		}			
	}
				
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
		
		if(*pMsg == GRPS_SEND_SAVEDATA_FLAG){
		  g_loopSendMsgNum--;			
		}
		
		printf("[%d]modem task notified by %x, size %d, msg queue left %d\r\n", OSTimeGet(&err), *pMsg, size, check_modemTaskMsg_queue());
		
		switch(*pMsg){
		  case NTP_TIMER_FLAG:
			{
				ntpProcess(buf);
				OSMemPut((OS_MEM*	)&MESSAGE_MEM, (void*)pMsg, (OS_ERR*)&err);
				break;
			}
			case GPRS_NTPFAIL_SEND_FLAG:
		  {
				printf("@@@@ ntp fail send ++++\r\n");
				//OSTimeDlyHMSM(0,0,10,0,OS_OPT_TIME_PERIODIC,&err);
				if(g_ntpUpdateFlag ==1){
				  shiftTimeInGprsBuf(pMsg, g_diffTime);
					*pMsg = GPRS_NTPOK_SEND_FLAG;
				}
				printf("ntp fail send ---\r\n");
			}
			case GPRS_NTPOK_SEND_FLAG:
			{
			  SIM800_ERROR aterr;
				u8 flag = *pMsg;
			  //OSTmrStop(&tmr1,OS_OPT_TMR_NONE,0,&err);
 				/*
				u32 infoAddr = 0;
				u8 infoContent[8];
				flashTestBuf = OSMemGet((OS_MEM*)&GPRS_MEM, (OS_ERR*)&err);
				
				ret = readFromFlash(flashTestBuf, &infoAddr, infoContent);
				printf("@@@ flash read test return %d, %08x \r\n", ret, infoAddr);
								
				OSMemPut((OS_MEM*	)&GPRS_MEM, (void*)flashTestBuf, (OS_ERR*)&err);
				*/
				
			  if(queryCellId(pMsg+65, pMsg+82) == 0){
			    //printf("CellId %s \r\n", buf);
				  printf("gprs buf from ccid %s \r\n", pMsg+5);
			  }else{
			    printf("cellid fail \r\n");
			  }
			
			  pMsg[99] = xorVerify(pMsg+2, 97);
			
			  //sim800c_gprs_transparentMode(gprs_Buf, 100);
				pMsg[0] = 0x55;
			  aterr = sim800c_gprs_tcp(pMsg, 100);
				if(aterr == AT_OK){
					u8 num = 0;
				  LED0 = 1;
					LED1 = 1;
					OSTimeDlyHMSM(0,0,0,200,OS_OPT_TIME_PERIODIC,&err);
					LED0 = 0;
					LED1 = 0;
					OSTimeDlyHMSM(0,0,0,200,OS_OPT_TIME_PERIODIC,&err);
					LED0 = 1;
					LED1 = 1;
					
					OSMemPut((OS_MEM*	)&GPRS_MEM, (void*)pMsg, (OS_ERR*)&err);
					
					OSMutexPend (&FLASH_MUTEX,0,OS_OPT_PEND_BLOCKING,0,&err);
				  if(getNumInFlash(&num) == 0){
				  }else{
				    num = 0;
				  }
				  OSMutexPost(&FLASH_MUTEX,OS_OPT_POST_NONE,&err);
          if(num > 0) {					
					  postGprsSendSaveDataMessage();
					}
					
				}else{
					printf(" flash open and resend  \r\n");
					//OSTmrStop(&tmr1,OS_OPT_TMR_NONE,0,&err);
					//testFlag = 3;
					if(flag == GPRS_NTPOK_SEND_FLAG){
					  *pMsg = GPRS_NTPOK_TRY_SECOND;
					}else if(flag == GPRS_NTPFAIL_SEND_FLAG){
					  *pMsg = GPRS_NTPFAIL_TRY_SECOND;
					}
					LED0 = 1;
					OSTimeDlyHMSM(0,0,0,200,OS_OPT_TIME_PERIODIC,&err);
					LED0 = 0;
					OSTimeDlyHMSM(0,0,0,200,OS_OPT_TIME_PERIODIC,&err);
					LED0 = 1;
				
					
					/*
					ret = saveToFlash(pMsg);
					if(ret != 0){
					  stopAutoTest();
					}
					
					
					OSMemPut((OS_MEM*	)&GPRS_MEM, (void*)pMsg, (OS_ERR*)&err);
					
					*/
					postGprsSendMessage(&pMsg);
					//OSMemPut((OS_MEM*	)&GPRS_MEM, (void*)pMsg, (OS_ERR*)&err);
				}
				break;
			}
			case GPRS_NTPFAIL_TRY_SECOND:
			{
				if(g_ntpUpdateFlag ==1){
				  shiftTimeInGprsBuf(pMsg, g_diffTime);
					*pMsg = GPRS_NTPOK_TRY_SECOND;
				}
			}
			case GPRS_NTPOK_TRY_SECOND:
			{
				SIM800_ERROR aterr;
				u8 flag = *pMsg;
				printf("gprs second try delay 3s\r\n");
				OSTimeDlyHMSM(0,0,3,0,OS_OPT_TIME_PERIODIC,&err);
				*pMsg = 0x55;
				aterr = sim800c_gprs_tcp(pMsg, 100);
				if(aterr == AT_OK){
				  OSMemPut((OS_MEM*	)&GPRS_MEM, (void*)pMsg, (OS_ERR*)&err);
					//OSTmrStart(&tmr1, &err);
					//testFlag = 0;
					LED0 = 1;
					LED1 = 1;
					OSTimeDlyHMSM(0,0,0,200,OS_OPT_TIME_PERIODIC,&err);
					LED0 = 0;
					LED1 = 0;
					OSTimeDlyHMSM(0,0,0,200,OS_OPT_TIME_PERIODIC,&err);
					LED0 = 1;
					LED1 = 1;
				}else{
					*pMsg = flag;
					OSMutexPend (&FLASH_MUTEX,0,OS_OPT_PEND_BLOCKING,0,&err);
					ret = saveToFlash(pMsg);
					OSMemPut((OS_MEM*	)&GPRS_MEM, (void*)pMsg, (OS_ERR*)&err);
					OSMutexPost(&FLASH_MUTEX,OS_OPT_POST_NONE,&err);
					OSTmrStart(&tmr_loopsend, &err);
					printf("start repeat send save data \r\n");
					LED0 = 1;
				  LED1 = 1;
				}
				
				break;
			}
			
			case GRPS_SEND_SAVEDATA_FLAG:
			{
				SIM800_ERROR aterr;
				u32 infoAddr;
				u8 infoContent[8];
				u8 i = 0, num = 0;
				
				OSMutexPend (&FLASH_MUTEX,0,OS_OPT_PEND_BLOCKING,0,&err);
				if(getNumInFlash(&num) == 0){
				  
				}else{
				   num = 0;
				}
				OSMutexPost(&FLASH_MUTEX,OS_OPT_POST_NONE,&err);			
				printf("total num is %d, old num is %d \r\n", num, g_oldNumInFlash);
				
				if(num > 0) {
				  flashTestBuf = OSMemGet((OS_MEM*)&GPRS_MEM, (OS_ERR*)&err);
					if(flashTestBuf != 0){
         	  OSMutexPend (&FLASH_MUTEX,0,OS_OPT_PEND_BLOCKING,0,&err);
				    ret = readFromFlash(flashTestBuf, &infoAddr, infoContent);
					  OSMutexPost(&FLASH_MUTEX,OS_OPT_POST_NONE,&err);
				    printf("@@@ flash read test return %d , 2 byte %x %x\r\n", ret, *flashTestBuf, *(flashTestBuf+1));
				
				
			      if((*flashTestBuf == GPRS_NTPFAIL_TRY_SECOND) && (g_oldNumInFlash == 0)){
				      printf("correct the time \r\n");
					    shiftTimeInGprsBuf(flashTestBuf, g_diffTime);
			      }
										
				    *flashTestBuf = 0x55;
					
				    if(ret == 0){
				      aterr = sim800c_gprs_tcp(flashTestBuf, 100);
						  printf("tcp send return %04x \r\n", aterr);
					    if(aterr != AT_FAIL && aterr != AT_SERVER_RES_TIMEOUT){
							  OSMutexPend (&FLASH_MUTEX,0,OS_OPT_PEND_BLOCKING,0,&err);
				        updateFlashInfo(infoAddr, infoContent);
							  OSMutexPost(&FLASH_MUTEX,OS_OPT_POST_NONE,&err);
							  LED0 = 1;
					      LED1 = 1;
					      OSTimeDlyHMSM(0,0,0,200,OS_OPT_TIME_PERIODIC,&err);
					      LED0 = 0;
					      LED1 = 0;
					      OSTimeDlyHMSM(0,0,0,200,OS_OPT_TIME_PERIODIC,&err);
					      LED0 = 1;
					      LED1 = 1;
							
							  if(num > 1){
							    postGprsSendSaveDataMessage();
							  }else{
							    printf("it is the last data in flash \r\n");
							  }
							  if(g_oldNumInFlash > 0){
							    g_oldNumInFlash--;
							  }
							
				      }else{
							  LED0 = 1;
					      OSTimeDlyHMSM(0,0,0,200,OS_OPT_TIME_PERIODIC,&err);
					      LED0 = 0;
					      OSTimeDlyHMSM(0,0,0,200,OS_OPT_TIME_PERIODIC,&err);
					      LED0 = 1;
						    OSTmrStart(&tmr_loopsend, &err);
							  printf("send save data fail, 10s try again \r\n");
							  //break;
						  }
				    }
					  
			  
				    OSMemPut((OS_MEM*	)&GPRS_MEM, (void*)flashTestBuf, (OS_ERR*)&err);
				  }else{
					  printf("fail to get buf when readfromflash \r\n");
					}
				}
				OSMemPut((OS_MEM*	)&MESSAGE_MEM, (void*)pMsg, (OS_ERR*)&err);
				break;
			}
			default:{
			   break;
			}
		}
		
  }
	
	OSMemPut((OS_MEM*	)&INTERNAL_MEM,(void*	)buf,(OS_ERR* )&err);
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


u8 increaseCntAtFlash(void)
{
	u16 i= 0,offset,index = 0, countLen = (GPRS_SAVE_COUNT_END_ADDR - GPRS_SAVE_COUNT_START_ADDR);
	u8 counttemp[4];
	u8 flashOpRet = 0;
	u16 countNum = 0;
	u8 datatemp[INFO_TRUNK_SIZE];
	u16 *ptr;
	u8* countSaveAddr;
	u8 countCheckTemp[4];
	CPU_SR_ALLOC();
	
	while(i < countLen){
	  ptr = (u16*)datatemp;
		OS_CRITICAL_ENTER();
		STMFLASH_Read(GPRS_SAVE_COUNT_START_ADDR+i, ptr,INFO_TRUNK_SIZE/2);
		OS_CRITICAL_EXIT();
		//printf("@@@ read info offset %d \r\n", i);
		offset = (u32)ptr - (u32)datatemp;
		while( offset < INFO_TRUNK_SIZE){
			
		  if(*ptr == 0xFFFF){
			  printf("@@@ meet blank bytes %d \r\n", offset);
			  break;
			}else{
			  //printf("@@@ new index %04x\r\n", *ptr);
			}
		
			if(index <= *ptr){
			  index = *ptr;
				countSaveAddr = (u8 *)(GPRS_SAVE_COUNT_START_ADDR + i + offset);
				memcpy(counttemp, (u8 *)ptr, 4);
				//printf("@@@ bigger index %x, ptr %x \r\n", index, (u32)infoSaveAddr);
			}
			
			if(*(ptr+1) == 0xFFFF ){
			  printf("info data corrupt");
			}
			
			ptr += 2;
			offset += 4;
		}
		
		if(*ptr == 0xFFFF){
			printf("@@@ meet blank, end \r\n");
		  break;
		}
		
		if(index == 0xFFFE){
		  break;
		}
		
		i += INFO_TRUNK_SIZE;
	}
	
	if(index == 0){
		//It is the 1st to write a record
		countNum = 0;
		countSaveAddr = (u8 *)GPRS_SAVE_COUNT_START_ADDR;
	}else{
	  countNum = *((u16 *)counttemp + 1);
	}
	
	if(index < 0xFFFE){
      *((u16*)counttemp) = index + 1;
  }else{
			*((u16*)counttemp) = 1;
  }
	
  if(countNum < 0xFFFE){
      *((u16*)counttemp+1) = countNum + 1;
  }else{
			*((u16*)counttemp+1) = 1;
  }
			
  if(index != 0){
    countSaveAddr += 4;
  }else{
		printf("@1st record, not increase \r\n");
	}
			
  if((u32)countSaveAddr >= GPRS_SAVE_COUNT_END_ADDR){
	  countSaveAddr = (u8 *)GPRS_SAVE_COUNT_START_ADDR;
  }
	
	
	i = 3;
	
	while(i > 0){
			  
    if((u32)countSaveAddr == GPRS_SAVE_COUNT_START_ADDR){
      STMFLASH_ErasePage(GPRS_SAVE_COUNT_START_ADDR);
	  }
					
		flashOpRet = STMFLASH_Write((u32)countSaveAddr,(u16*)counttemp,2);
		
		if(flashOpRet == 0){
		  STMFLASH_Read((u32)countSaveAddr, (u16*)countCheckTemp,4);
		  if(countCheckTemp[0] != counttemp[0] && countCheckTemp[2] != counttemp[2]){
			  printf("countCheckTemp-> %02x, %02x, %02x, %02x\r\n",
		    countCheckTemp[0], countCheckTemp[1],countCheckTemp[2], countCheckTemp[3]);
				flashOpRet = 6;
		  }else{
			  printf("count write and read ok\r\n");
			}
      break;
    }else {
      printf("try again \r\n");
    }
				//if(recordNum == 20){
			  //   infoSaveAddr += 8;
			  //}
		i--;
  }
	return flashOpRet;
}

u16 getCountFromFlash(void)
{
	u16 i= 0,offset,index = 0, countLen = (GPRS_SAVE_COUNT_END_ADDR - GPRS_SAVE_COUNT_START_ADDR);
	u8 counttemp[4];
	u16 countNum = 0;
	u8 datatemp[INFO_TRUNK_SIZE];
	u16 *ptr;
	CPU_SR_ALLOC();
	
	while(i < countLen){
	  ptr = (u16*)datatemp;
		OS_CRITICAL_ENTER();
		STMFLASH_Read(GPRS_SAVE_COUNT_START_ADDR+i, ptr,INFO_TRUNK_SIZE/2);
		OS_CRITICAL_EXIT();
		//printf("@@@ read info offset %d \r\n", i);
		offset = (u32)ptr - (u32)datatemp;
		while( offset < INFO_TRUNK_SIZE){
			
		  if(*ptr == 0xFFFF){
			  printf("@@@ meet blank bytes %d \r\n", offset);
			  break;
			}else{
			  //printf("@@@ new index %04x\r\n", *ptr);
			}
		
			if(index <= *ptr){
			  index = *ptr;
				memcpy(counttemp, (u8 *)ptr, 4);
				//printf("@@@ bigger index %x, ptr %x \r\n", index, (u32)infoSaveAddr);
			}
			
			if(*(ptr+1) == 0xFFFF ){
			  printf("info data corrupt");
			}
			
			ptr += 2;
			offset += 4;
		}
		
		if(*ptr == 0xFFFF){
			printf("@@@ meet blank, end \r\n");
		  break;
		}
		
		if(index == 0xFFFE){
		  break;
		}
		
		i += INFO_TRUNK_SIZE;
	}
	
	if(index == 0){
		//It is the 1st to write a record
		countNum = 0;
	}else{
	  countNum = *((u16 *)counttemp + 1);
	}
				
  return countNum;
}



u8 saveToFlash(u8* pBuf)
{
	u8 datatemp[INFO_TRUNK_SIZE];
	u8 infotemp[8];
	u8 infoCheckTemp[8];
	u16 i=0,offset, index = 0;
	u16 *ptr;
	u8* saveAddr;
	u8* infoSaveAddr;
	u8 recordNum = 0;
	u8 flashOpRet = 0;
	u16 infoLen = (GRPS_SAVE_INFO_END_ADDR - GPRS_SAVE_INFO_START_ADDR);
	CPU_SR_ALLOC();
	
	printf("@@@ saveToFlash ... \r\n");
  //Find the biggst index, util meet the 0xFFFF(blank area) or 0xFFFE(max index)
	//If index is 0xFFFE, we must erase whole page, start with 1;
	//If index is 0, only happen no record before or all record is clean when info corrupt is detected
  while(i < infoLen){
	  ptr = (u16*)datatemp;
		OS_CRITICAL_ENTER();
		STMFLASH_Read(GPRS_SAVE_INFO_START_ADDR+i, ptr,INFO_TRUNK_SIZE/2);
		OS_CRITICAL_EXIT();
		//printf("@@@ read info offset %d \r\n", i);
		offset = (u32)ptr - (u32)datatemp;
		while( offset < INFO_TRUNK_SIZE){
			
		  if(*ptr == 0xFFFF){
			  printf("@@@ meet blank bytes %d \r\n", offset);
			  break;
			}else{
			  //printf("@@@ new index %04x\r\n", *ptr);
			}
		
			if(index <= *ptr){
			  index = *ptr;
				infoSaveAddr = (u8 *)(GPRS_SAVE_INFO_START_ADDR + i + offset);
				memcpy(infotemp, (u8 *)ptr, 8);
				//printf("@@@ bigger index %x, ptr %x \r\n", index, (u32)infoSaveAddr);
			}
			
			if((*(ptr+1) == 0xFFFF) || (*(ptr+2) == 0xFFFF) || (*(ptr+3) == 0xFFFF) ){
			  printf("info data corrupt");
			}
			
			ptr += 4;
			offset += 8;
		}
		
		if(*ptr == 0xFFFF){
			printf("@@@ meet blank, end \r\n");
		  break;
		}
		
		if(index == 0xFFFE){
		  break;
		}
		
		i += INFO_TRUNK_SIZE;
	}
	
	printf("@@@ index %d , infoSaveAddr %08x\r\n", index, (u32)infoSaveAddr);
	if(index == 0){
		//It is the 1st to write a record
		infoSaveAddr = (u8 *)GPRS_SAVE_INFO_START_ADDR;
	  saveAddr = (u8 *)GRPS_SAVE_START_ADDR;
		
		recordNum = 0;
	}else{
	  if(flashInfoXorVerify(infotemp) == 0){
		   saveAddr = (u8 *)((infotemp[7] << 24) | (infotemp[6] << 16) | (infotemp[5] << 8) | infotemp[4]);
	     recordNum = infotemp[2];
		}else {
			//To Do
			//erase info page
		  return 1;
		}
	}
	
	printf("@@@ saveAddr %x, num %d \r\n", (u32)saveAddr, recordNum);
	
	if(((u32)saveAddr < GRPS_SAVE_START_ADDR) || ((u32)saveAddr > GPRS_SAVE_END_ADDR) ){
	
	//To Do 
	//erase info page
    return 2;		
	}
	
	if(recordNum > 80){
		//data corrupt
		//To Do
		//erase info page
	  return 3;
	}
	
	if(recordNum == 80){
		 //To do remove the oldest record
	  if(((u32)saveAddr + 100) < GPRS_SAVE_END_ADDR){
		  saveAddr += 100;   
	  }else{
		  saveAddr = (u8*)(GRPS_SAVE_START_ADDR + GPRS_SAVE_END_ADDR - (u32)saveAddr);
	  }
		recordNum--;
		if(g_oldNumInFlash > 0){
		  g_oldNumInFlash--;
		}
	}
	
	{
		u32 writeAddr = (u32)saveAddr + recordNum*100;
		
		OS_CRITICAL_ENTER();
		
		if(((u32)writeAddr+100) < GPRS_SAVE_END_ADDR){
			printf("@@@ writeAddr %x \r\n", writeAddr);
			flashOpRet = STMFLASH_Write((u32)writeAddr,(u16*)pBuf,50);
		}else if((u32)writeAddr < GPRS_SAVE_END_ADDR){
		  u16 left = GPRS_SAVE_END_ADDR - (u32)writeAddr;
			printf("@@@ should not be here !!! \r\n");
			flashOpRet = STMFLASH_Write((u32)writeAddr,(u16*)pBuf,left/2);
			flashOpRet = STMFLASH_Write(GRPS_SAVE_START_ADDR,(u16*)(pBuf+left),(100 - left)/2);
		
		}else if((u32)writeAddr >= GPRS_SAVE_END_ADDR){
			u32 newAddr = (u32)writeAddr - GPRS_SAVE_END_ADDR + GRPS_SAVE_START_ADDR;
			//printf("@@@ should no be here !!!! \r\n");
			flashOpRet = STMFLASH_Write(newAddr,(u16*)pBuf,50);
		}else{
		  printf("!!! can not be here %x \r\n", (u32)writeAddr);
		}
		
    if(index < 0xFFFE){
      *((u16*)infotemp) = index + 1;
    }else{
			*((u16*)infotemp) = 1;
    }
			
	  infotemp[2] = recordNum+1;
		*((u32*)(&infotemp[4])) = (u32)saveAddr;
		
    if(index != 0){
       infoSaveAddr += 8;
		}else{
		   printf("@1st record, not increase \r\n");
		}
			
    if((u32)infoSaveAddr >= GRPS_SAVE_INFO_END_ADDR){
		  infoSaveAddr = (u8 *)GPRS_SAVE_INFO_START_ADDR;
		}
		
		flashInfoXorFill(infotemp);
		
    printf("info addr %08x, data addr %08x , info-> %02x, %02x \r\n ", (u32)infoSaveAddr, (u32)saveAddr, infotemp[0], infotemp[1]);
		printf("info-> %02x, %02x, %02x, %02x, %02x,%02x\r\n", infotemp[2], infotemp[3],infotemp[4], infotemp[5],infotemp[6], infotemp[7]);
		
		
		if(flashOpRet == 0){
		  int j = 3;
			while(j > 0){
			  
			  if((u32)infoSaveAddr == GPRS_SAVE_INFO_START_ADDR){
	        STMFLASH_ErasePage(GPRS_SAVE_INFO_START_ADDR);
	      }
					
		    flashOpRet = STMFLASH_Write((u32)infoSaveAddr,(u16*)infotemp,4);
		
			  if(flashOpRet == 0){
		       STMFLASH_Read((u32)infoSaveAddr, (u16*)infoCheckTemp,4);
				   if(infoCheckTemp[0] != infotemp[0] && infoCheckTemp[3] != infotemp[3]){
						 printf("infoCheckTemp-> %02x, %02x, %02x, %02x, %02x, %02x, %02x, %02x\r\n",
		         infoCheckTemp[0], infoCheckTemp[1],infoCheckTemp[2], infoCheckTemp[3],infoCheckTemp[4], infoCheckTemp[5], infoCheckTemp[6], infoCheckTemp[7]);
				     flashOpRet = 6;
				   }
					 break;
			  }else {
			    printf("try again \r\n");
			  }
				//if(recordNum == 20){
			  //   infoSaveAddr += 8;
			  //}
				j--;
		  }
		}
    /*
		printf("infoCheckTemp-> %02x, %02x, %02x, %02x, %02x, %02x, %02x, %02x\r\n",
		  infoCheckTemp[0], infoCheckTemp[1],infoCheckTemp[2], infoCheckTemp[3],infoCheckTemp[4], infoCheckTemp[5], infoCheckTemp[6], infoCheckTemp[7]);
		
		if(infoCheckTemp[0] != infotemp[0]){
		  STMFLASH_Read((u32)infoSaveAddr-8, (u16*)infoCheckTemp,4);
			printf("infoCheckTemp - 8-> %02x, %02x, %02x, %02x, %02x, %02x, %02x, %02x\r\n",
			  infoCheckTemp[0], infoCheckTemp[1],infoCheckTemp[2], infoCheckTemp[3],infoCheckTemp[4], infoCheckTemp[5], infoCheckTemp[6], infoCheckTemp[7]);
			STMFLASH_Read((u32)infoSaveAddr+8, (u16*)infoCheckTemp,4);
			printf("infoCheckTemp + 8-> %02x, %02x, %02x, %02x, %02x, %02x, %02x, %02x\r\n",
			  infoCheckTemp[0], infoCheckTemp[1],infoCheckTemp[2], infoCheckTemp[3],infoCheckTemp[4], infoCheckTemp[5], infoCheckTemp[6], infoCheckTemp[7]);
		}else{
		  STMFLASH_Read((u32)infoSaveAddr-8, (u16*)infoCheckTemp,4);
			printf("infoCheckTemp - 8-> %02x, %02x, %02x, %02x, %02x, %02x, %02x, %02x\r\n",
			  infoCheckTemp[0], infoCheckTemp[1],infoCheckTemp[2], infoCheckTemp[3],infoCheckTemp[4], infoCheckTemp[5], infoCheckTemp[6], infoCheckTemp[7]);
			STMFLASH_Read((u32)infoSaveAddr+8, (u16*)infoCheckTemp,4);
			printf("infoCheckTemp + 8-> %02x, %02x, %02x, %02x, %02x, %02x, %02x, %02x\r\n",
			  infoCheckTemp[0], infoCheckTemp[1],infoCheckTemp[2], infoCheckTemp[3],infoCheckTemp[4], infoCheckTemp[5], infoCheckTemp[6], infoCheckTemp[7]);
		}
		*/
		OS_CRITICAL_EXIT();
	}
	
	//To Do we'd better read and check
	
	
	return flashOpRet;
}


u8 getNumInFlash(u8 *savedNum) {
  u8 datatemp[INFO_TRUNK_SIZE];
	u8 infotemp[8];
	u16 i=0, offset, index = 0;
	u16 *ptr;
	u8* saveAddr;
	u8* infoSaveAddr;
	u8 recordNum = 0;
	u16 infoLen = (GRPS_SAVE_INFO_END_ADDR - GPRS_SAVE_INFO_START_ADDR);
	CPU_SR_ALLOC();
	
	printf("@@@ getNumInFlash ... \r\n");
	
	//Find the biggst index, util meet the 0xFFFF(blank area) or 0xFFFE(max index)
	//If index is 0xFFFE, we must erase whole page, start with 1;
	//If index is 0, only happen no record before or all record is clean when info corrupt is detected
  while(i < infoLen){
	  ptr = (u16*)datatemp;
		OS_CRITICAL_ENTER();
		STMFLASH_Read(GPRS_SAVE_INFO_START_ADDR+i, ptr,INFO_TRUNK_SIZE/2);
		OS_CRITICAL_EXIT();
		//printf("@@@ read info offset %d \r\n",i);
		offset = (u32)ptr - (u32)datatemp;
		while(offset < INFO_TRUNK_SIZE){
			
		  if(*ptr == 0xFFFF){
				printf("@@@ meet blank, break \r\n");
			  break;
			}
		
			if(index <= *ptr){
			  index = *ptr;
				infoSaveAddr = (u8 *)(GPRS_SAVE_INFO_START_ADDR + i + offset);
				memcpy(infotemp, (u8 *)ptr, 8);
				//printf("@@@ bigger index %x, ptr %x \r\n", index, (u32)infoSaveAddr);
			}
			
			if((*(ptr+1) == 0xFFFF) || (*(ptr+2) == 0xFFFF) || (*(ptr+3) == 0xFFFF) ){
			  printf("@@@ info data corrupt");
			}
			
			ptr += 4;
			offset += 8;
		}
		
		if(*ptr == 0xFFFF){
			printf("meet the end of ");
		  break;
		}
		if(index == 0xFFFE){
		  break;
		}
		
		i += INFO_TRUNK_SIZE;
	}
	
	printf("@@@ index %d infoSaveAddr %08x \r\n", index, (u32)infoSaveAddr);
	if(index == 0){
		//No record is found
		printf("@@@ no record is found \r\n");
	  return 0;
	}else{
	  if(flashInfoXorVerify(infotemp) == 0){
		   saveAddr = (u8 *)((infotemp[7] << 24) | (infotemp[6] << 16) | (infotemp[5] << 8) | infotemp[4]);
	     recordNum = infotemp[2];
			 printf("@@@ addr %x, record num %d \r\n", (u32)saveAddr, recordNum);
			 if(recordNum == 0){
				 printf("@@@ record is empty \r\n");
			   return 0;
			 }
		}else {
			//To Do
			//erase info page
		  return 2;
		}
	}
	
	if(((u32)saveAddr < GRPS_SAVE_START_ADDR) || ((u32)saveAddr >= GPRS_SAVE_END_ADDR) ){
	
	//To Do 
	//erase info page
    return 3;		
	}
	
	if(recordNum > 80){
		//data corrupt
		//To Do
		//erase info page
	  return 4;
	}
	
	*savedNum = recordNum;
	printf("@@@ getFlash ok return %d \r\n", recordNum);
	return 0;
}

//We just read the content, if content send by GRPS ok, then update the gprs info
u8 readFromFlash(u8* pBuf, u32 *infoAddr, u8 *infoContent ){
  u8 datatemp[INFO_TRUNK_SIZE];
	u8 infotemp[8];
	u16 i=0, offset, index = 0;
	u16 *ptr;
	u8* saveAddr;
	u8* infoSaveAddr;
	u8 recordNum = 0;
	u16 infoLen = (GRPS_SAVE_INFO_END_ADDR - GPRS_SAVE_INFO_START_ADDR);
	CPU_SR_ALLOC();
	
	printf("@@@ readFromFlash ... \r\n");
	
	//Find the biggst index, util meet the 0xFFFF(blank area) or 0xFFFE(max index)
	//If index is 0xFFFE, we must erase whole page, start with 1;
	//If index is 0, only happen no record before or all record is clean when info corrupt is detected
  while(i < infoLen){
	  ptr = (u16*)datatemp;
		OS_CRITICAL_ENTER();
		STMFLASH_Read(GPRS_SAVE_INFO_START_ADDR+i, ptr,INFO_TRUNK_SIZE/2);
		OS_CRITICAL_EXIT();
		//printf("@@@ read info offset %d \r\n",i);
		offset = (u32)ptr - (u32)datatemp;
		while(offset < INFO_TRUNK_SIZE){
			
		  if(*ptr == 0xFFFF){
				printf("@@@ meet blank, break \r\n");
			  break;
			}
		
			if(index <= *ptr){
			  index = *ptr;
				infoSaveAddr = (u8 *)(GPRS_SAVE_INFO_START_ADDR + i + offset);
				memcpy(infotemp, (u8 *)ptr, 8);
				//printf("@@@ bigger index %x, ptr %x \r\n", index, (u32)infoSaveAddr);
			}
			
			if((*(ptr+1) == 0xFFFF) || (*(ptr+2) == 0xFFFF) || (*(ptr+3) == 0xFFFF) ){
			  printf("@@@ info data corrupt");
			}
			
			ptr += 4;
			offset += 8;
		}
		
		if(*ptr == 0xFFFF){
			printf("meet the end of ");
		  break;
		}
		if(index == 0xFFFE){
		  break;
		}
		
		i += INFO_TRUNK_SIZE;
	}
	
	printf("@@@ index %d infoSaveAddr %08x \r\n", index, (u32)infoSaveAddr);
	if(index == 0){
		//No record is found
		printf("@@@ no record is found \r\n");
	  return 1;
	}else{
	  if(flashInfoXorVerify(infotemp) == 0){
		   saveAddr = (u8 *)((infotemp[7] << 24) | (infotemp[6] << 16) | (infotemp[5] << 8) | infotemp[4]);
	     recordNum = infotemp[2];
			 printf("@@@ addr %x, record num %d \r\n", (u32)saveAddr, recordNum);
			 if(recordNum == 0){
				 printf("@@@ record is empty \r\n");
			   return 1;
			 }
		}else {
			//To Do
			//erase info page
			printf("**** info xor error\r\n");
		  return 2;
		}
	}
	
	if(((u32)saveAddr < GRPS_SAVE_START_ADDR) || ((u32)saveAddr >= GPRS_SAVE_END_ADDR) ){
	
	//To Do 
	//erase info page
    return 3;		
	}
	
	if(recordNum > 80){
		//data corrupt
		//To Do
		//erase info page
	  return 4;
	}
	
	
	OS_CRITICAL_ENTER();
	if((u32)saveAddr + 100 <= GPRS_SAVE_END_ADDR){
	  STMFLASH_Read((u32)saveAddr, (u16*)pBuf, 50);
	}else{
		u16 left  = (GPRS_SAVE_END_ADDR - (u32)saveAddr)/2;
		STMFLASH_Read((u32)saveAddr, (u16*)pBuf, left);
		
		STMFLASH_Read(GRPS_SAVE_START_ADDR, (u16*)pBuf+left, 50-left);
	}
	OS_CRITICAL_EXIT();
	
	if(index < 0xFFFE){
    *((u16*)infotemp) = index + 1;
  }else{
		*((u16*)infotemp) = 1;
  }
			
	infotemp[2] = recordNum-1;
	
	saveAddr += 100;
	
	if((u32)saveAddr >= GPRS_SAVE_END_ADDR){
	  saveAddr = saveAddr - GPRS_SAVE_END_ADDR + GRPS_SAVE_START_ADDR;
	}
	
  *((u32*)(&infotemp[4])) = (u32)saveAddr;
			
  infoSaveAddr += 8;
			
  if((u32)infoSaveAddr >= GRPS_SAVE_INFO_END_ADDR){
    infoSaveAddr = (u8 *)GPRS_SAVE_INFO_START_ADDR;
	}
		
  flashInfoXorFill(infotemp);
		
  *infoAddr	 = (u32)infoSaveAddr;
	memcpy(infoContent, infotemp, 8);
	//STMFLASH_Write((u32)infoSaveAddr,(u16*)infotemp,4);
		
	printf("@@@ not save info addr %08x, data addr %08x , info-> %02x, %02x \r\n ", (u32)infoSaveAddr, (u32)saveAddr, infotemp[0], infotemp[1]);
	printf("@@@ info-> %02x, %02x, %02x, %02x, %02x,%02x\r\n", infotemp[2], infotemp[3],infotemp[4], infotemp[5],infotemp[6], infotemp[7]);
	
	return 0;
}

u8 updateFlashInfo(u32 infoAddr, u8 *infoContent){
  CPU_SR_ALLOC();
	
	printf("@@@ updateFlashInfo \r\n");
	
	printf("@@@ info addr %08x, info-> %02x, %02x \r\n ", (u32)infoAddr, infoContent[0], infoContent[1]);
	printf("@@@ info-> %02x, %02x, %02x, %02x, %02x,%02x\r\n", infoContent[2], infoContent[3],infoContent[4], infoContent[5],infoContent[6], infoContent[7]);
  OS_CRITICAL_ENTER();
	
	if(infoAddr == GPRS_SAVE_INFO_START_ADDR){
	  STMFLASH_ErasePage(GPRS_SAVE_INFO_START_ADDR);
	}
	
  STMFLASH_Write(infoAddr,(u16*)infoContent,4);
	
  OS_CRITICAL_EXIT();
	
	return 0;
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

void ntpProcess(u8 * buf){
	OS_ERR err;
	u8 i = 3;
	printf("ntpProcess try 3times + 2s to get ntp time...\r\n");
	
  if(ntp_update() == 0) {
		while(i > 0){
		  OSTimeDlyHMSM(0,0,2,0,OS_OPT_TIME_PERIODIC,&err);
				
		  memset(buf, 0, INTERNAL_MEMBLOCK_SIZE);
		
		  if(fetchNetworkTime(buf) == 0){
	      //printf("%02d/02%d/02%d %02d:%02d:%02d\r\n", buf[0], buf[1], buf[2], buf[3], buf[4], buf[5]);
			  if(buf[0] < 18){
			    printf("ntp time is wrong, try ntp again \r\n");
				  OSTmrStart(&tmr_ntp,&err);
			  }else{
			    printf("ntp time is ok, signal rtc writing\r\n");
				  postRTCUpdateMessage();
			  }
				break;
		  }
			i--;
	  }
		
	}else{
		printf("ntp process fail, try 10s later /r/n");
	  OSTmrStart(&tmr_ntp,&err);
	}
}

void tmr1_callback(void *p_tmr,void *p_arg)
{
	printf("timer 1 tick \r\n");
	postTimerMessage(1);
}

void tmr_ntp_callback(void *p_tmr,void *p_arg)
{
	printf("timer ntp tick \r\n");
	postTimerMessage(2);
}


void tmr_loopsend_callback(void *p_tmr,void *p_arg)
{
	printf("timer loopsend tick \r\n");
  postGprsSendSaveDataMessage();
}

void postRTCUpdateMessage(void){
	OS_ERR err;
	u8 *pbuf;
	pbuf = OSMemGet((OS_MEM*)&MESSAGE_MEM, (OS_ERR*)&err);
	
	if(pbuf){
		  *pbuf = RTC_UPDATE_FLAG;
		  OSTaskQPost((OS_TCB*	)&MainTaskTCB,
                    (void*		)pbuf,
                    (OS_MSG_SIZE)MESSAGE_MEMBLOCK_SIZE,
                    (OS_OPT		)OS_OPT_POST_FIFO,
					(OS_ERR*	)&err);
		  if(err != OS_ERR_NONE){
			  OSMemPut((OS_MEM*	)&MESSAGE_MEM, (void*)pbuf, (OS_ERR*)&err);
		  }else{
			  printf("post RTC update message \r\n");
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

void postGprsSendSaveDataMessage(void){
	OS_ERR err;
	u8 *pbuf;
	
	
	g_loopSendMsgNum++;
	
	if(g_loopSendMsgNum > 1){
		 printf(" g_loopSendMsgNum %d > 1, do not post again\r\n", g_loopSendMsgNum);
	   g_loopSendMsgNum--;
		 return;
	}
						  
	
	pbuf = OSMemGet((OS_MEM*)&MESSAGE_MEM, (OS_ERR*)&err);
	
	if(pbuf){
	  *pbuf = GRPS_SEND_SAVEDATA_FLAG;
		OSTaskQPost((OS_TCB*	)&ModemTaskTCB,
                    (void*		)pbuf,
                    (OS_MSG_SIZE)MESSAGE_MEMBLOCK_SIZE,
                    (OS_OPT		)OS_OPT_POST_FIFO,
					(OS_ERR*	)&err);
		if(err != OS_ERR_NONE){
		  OSMemPut((OS_MEM*	)&MESSAGE_MEM, (void*)pbuf, (OS_ERR*)&err);
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
	
	OSMemCreate((OS_MEM*	)&INTERNAL_MEM,
				(CPU_CHAR*	)"Internal Mem",
				(void*		)&Internal_RamMemp[0][0],
				(OS_MEM_QTY	)INTERNAL_MEM_NUM,
				(OS_MEM_SIZE)INTERNAL_MEMBLOCK_SIZE,
				(OS_ERR*	)&err);
	
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

	g_ntpUpdateFlag = 0;
	
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
								 
	OSTmrCreate((OS_TMR		*)&tmr_ntp,		//定时器1
                (CPU_CHAR	*)"tmr ntp",		//定时器名字
                (OS_TICK	 )2000,			//2000* 10 =20s
                (OS_TICK	 )0,          //
                (OS_OPT		 )OS_OPT_TMR_ONE_SHOT,
                (OS_TMR_CALLBACK_PTR)tmr_ntp_callback,//定时器1回调函数
                (void	    *)0,			//参数为0
                (OS_ERR	    *)&err);		//返回的错误码

	OSTmrCreate((OS_TMR		*)&tmr_loopsend,		//
                (CPU_CHAR	*)"time loopsend",
                (OS_TICK	 )1000,			//1000* 10 =10s
                (OS_TICK	 )0,          //
                (OS_OPT		 )OS_OPT_TMR_ONE_SHOT,
                (OS_TMR_CALLBACK_PTR)tmr_loopsend_callback,//定时器1回调函数
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

