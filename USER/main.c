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
#define MAIN_TASK_PRIO		4
//任务堆栈大小	
#define MAIN_STK_SIZE 		128
//任务控制块
OS_TCB MainTaskTCB;
//任务堆栈	
CPU_STK MAIN_TASK_STK[MAIN_STK_SIZE];
void main_task(void *p_arg);

//任务优先级
#define MODEM_TASK_PRIO		5
//任务堆栈大小	
#define MODEM_STK_SIZE 		256
//任务控制块
OS_TCB ModemTaskTCB;
//任务堆栈	
CPU_STK MODEM_TASK_STK[MODEM_STK_SIZE];
void modem_task(void *p_arg);

////////////////////////事件标志组//////////////////////////////
					
OS_FLAG_GRP	EventFlags;
OS_FLAG_GRP ModemEventFlags;
OS_TMR tmr1;	//定义一个定时器
OS_TMR tmr_ntp; 
void tmr1_callback(void *p_tmr,void *p_arg); //定时器1回调函数
void tmr_ntp_callback(void *p_tmr,void *p_arg); //定时器1回调函数

OS_MEM INTERNAL_MEM;	
//存储区中存储块数量
#define INTERNAL_MEM_NUM		5
//每个存储块大小
//由于一个指针变量占用4字节所以块的大小一定要为4的倍数
//而且必须大于一个指针变量(4字节)占用的空间,否则的话存储块创建不成功
#define INTERNAL_MEMBLOCK_SIZE	100  
//存储区的内存池，使用内部RAM
CPU_INT08U Internal_RamMemp[INTERNAL_MEM_NUM][INTERNAL_MEMBLOCK_SIZE];

u16 useCount = 0;

u8 g_ntpUpdateFlag = 0;

u16 ntp_year;
u8 ntp_month, ntp_day, ntp_hour, ntp_min, ntp_second;

u8 iccidInfo[20] = {0};
u8 startTime[20] = {0};
u8 endTime[20] = {0};

CPU_INT08U *gprs_Buf;

int main(void)
{
	OS_ERR err;
	CPU_SR_ALLOC();
	
	delay_init();       //延时初始化
	NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2); //中断分组配置
	uart_init(115200);    //串口波特率设置
	LED_Init();         //LED初始化
	//KEY_Init();
	EXTIX_Init();
	USART2_Init(115200);
	LED0 = 1;
	LED1 = 1;
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
	OSFlagCreate((OS_FLAG_GRP*)&EventFlags,		//指向事件标志组
                 (CPU_CHAR*	  )"Event Flags",	//名字
                 (OS_FLAGS	  )KEYFLAGS_VALUE,	//事件标志组初始值
                 (OS_ERR*  	  )&err);			//错误码
  
	OSFlagCreate((OS_FLAG_GRP*)&ModemEventFlags,		//指向事件标志组
                 (CPU_CHAR*	  )"Modem Event Flags",	//名字
                 (OS_FLAGS	  )KEYFLAGS_VALUE,	//事件标志组初始值
                 (OS_ERR*  	  )&err);			//错误码
								 
	OSTmrCreate((OS_TMR		*)&tmr_ntp,		//定时器1
                (CPU_CHAR	*)"tmr ntp",		//定时器名字
                (OS_TICK	 )1000,			//1000* 10 =10s
                (OS_TICK	 )0,          //
                (OS_OPT		 )OS_OPT_TMR_ONE_SHOT, //周期模式
                (OS_TMR_CALLBACK_PTR)tmr_ntp_callback,//定时器1回调函数
                (void	    *)0,			//参数为0
                (OS_ERR	    *)&err);		//返回的错误码							 

	OSTmrCreate((OS_TMR		*)&tmr1,		//定时器1
                (CPU_CHAR	*)"tmr1",		//定时器名字
                (OS_TICK	 )0,			//0ms
                (OS_TICK	 )500,          //200*10=2000ms
                (OS_OPT		 )OS_OPT_TMR_PERIODIC, //周期模式
                (OS_TMR_CALLBACK_PTR)tmr1_callback,//定时器1回调函数
                (void	    *)0,			//参数为0
                (OS_ERR	    *)&err);		//返回的错误码

	OSTaskCreate((OS_TCB 	* )&MainTaskTCB,		
				 (CPU_CHAR	* )"main task", 		
                 (OS_TASK_PTR )main_task, 			
                 (void		* )0,					
                 (OS_PRIO	  )MAIN_TASK_PRIO,     
                 (CPU_STK   * )&MAIN_TASK_STK[0],	
                 (CPU_STK_SIZE)MAIN_STK_SIZE/10,	
                 (CPU_STK_SIZE)MAIN_STK_SIZE,		
                 (OS_MSG_QTY  )0,					
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
                 (OS_MSG_QTY  )0,					
                 (OS_TICK	  )0,					
                 (void   	* )0,				
                 (OS_OPT      )OS_OPT_TASK_STK_CHK|OS_OPT_TASK_STK_CLR, 
                 (OS_ERR 	* )&err);	
	
	OS_CRITICAL_EXIT();	//进入临界区
  OSTaskDel((OS_TCB*)0,&err);
}

//main_task
//1.Handle the key interrupt
//2.combine the data in certain format
//3.inform modem task send data
void main_task(void *p_arg)
{
	OS_ERR err;
	OS_FLAGS flags;
	u8 RTCret = 0;
	u8 keyPress = 0;
	CPU_INT08U *buf;
	_calendar_obj start_time;
	_calendar_obj end_time;
	u8 deviceWorking = 0;
	
	p_arg = p_arg;
	
	
	RTCret = RTC_Init();
	
	gprs_Buf=OSMemGet((OS_MEM*)&INTERNAL_MEM, (OS_ERR*)&err);
	
	while(1)
	{
		printf("[%d]main task wait ...\r\n", OSTimeGet(&err));
		LED0 = 0;
		flags = OSFlagPend((OS_FLAG_GRP*)&EventFlags,
		       (OS_FLAGS	)KEYWKUP_HIGH_FLAG + KEYWKUP_LOW_FLAG + RTC_UPDATE_FLAG,
		     	 (OS_TICK   )0,
				   (OS_OPT	  )OS_OPT_PEND_FLAG_SET_ANY+OS_OPT_PEND_FLAG_CONSUME,
				   (CPU_TS*   )0,
				   (OS_ERR*	  )&err);
		printf("[%d]main task notified with %x\r\n", OSTimeGet(&err), flags);
		
		if((flags & KEYWKUP_HIGH_FLAG) && (deviceWorking == 0)){
			deviceWorking = 1;
			memset(gprs_Buf, 0, INTERNAL_MEMBLOCK_SIZE);
			useCount++;
			gprs_Buf[0]=0x55;
			gprs_Buf[1]=0xAA;
			gprs_Buf[2]=0x81;
			gprs_Buf[3]=(useCount & 0xFF00) >>8;
			gprs_Buf[4]=(useCount & 0x00FF);
			memcpy(gprs_Buf+5, iccidInfo, 20);
			printf("gprs buf-1 %s \r\n", gprs_Buf+5);
		  RTC_Get();
			printf("%d/%02d/%02d %02d:%02d:%02d\r\n", calendar.w_year, calendar.w_month,calendar.w_date,calendar.hour,calendar.min,calendar.sec);
			sprintf((char*)(gprs_Buf+25),"%02d/%02d/%02d,%02d/%02d/%02d+00",calendar.w_year>2000?(calendar.w_year-2000):calendar.w_year,
			  calendar.w_month,calendar.w_date,calendar.hour,calendar.min,calendar.sec);
			start_time = calendar;
			printf("gprs buf-2 %s \r\n", gprs_Buf+5);
		}else if((flags & KEYWKUP_LOW_FLAG) && (deviceWorking == 1)) {
			deviceWorking = 0;
		  RTC_Get();
			printf("%d/%d/%d %d:%d:%d\r\n", calendar.w_year, calendar.w_month,calendar.w_date,calendar.hour,calendar.min,calendar.sec);
			sprintf((char*)(gprs_Buf+45),"%02d/%02d/%02d,%02d/%02d/%02d+00",calendar.w_year>2000?(calendar.w_year-2000):calendar.w_year,
			calendar.w_month,calendar.w_date,calendar.hour,calendar.min,calendar.sec);
			end_time = calendar;
			postGprsSendEvent();
		}
		
		if(flags & RTC_UPDATE_FLAG){
			u16 year;
			u8 hour;
			buf=OSMemGet((OS_MEM*)&INTERNAL_MEM, (OS_ERR*)&err);
			if(fetchNetworkTime(buf) == 0){
			  printf("%d/%d/%d %d:%d:%d\r\n", buf[0], buf[1],buf[2], buf[3], buf[4], buf[5]);
			  year = 2000 + buf[0];
				hour = buf[3] + 6;
				hour = hour >= 24 ? (hour - 24):hour;
			  RTC_Set(year, buf[1],buf[2], hour, buf[4], buf[5]);
				printf("ntp set over ok \r\n");
			}else{
				printf("get ntp time fail ,try again \r\n");
			  OSTimeDlyHMSM(0,0,1,0,OS_OPT_TIME_PERIODIC,&err);
				postRTCUpdateEvent();
			}
			
	    OSMemPut((OS_MEM*	)&INTERNAL_MEM, (void*)buf, (OS_ERR*)&err);
		}
		
	}
}


void modem_task(void *p_arg)
{
	OS_ERR err;
	OS_FLAGS flags;
	CPU_INT08U *buf;
	CPU_INT08U *cellId;
	u8 ret, i=0;
	p_arg = p_arg;
	
  buf=OSMemGet((OS_MEM*)&INTERNAL_MEM, (OS_ERR*)&err);
	
	
	while(i < 15){
	  OSTimeDlyHMSM(0,0,1,0,OS_OPT_TIME_PERIODIC,&err);
		printf(" %d secs\r\n", 15-i);
		i++;
	}
	
	while(checkSIM800HW() != 0){
		OSTimeDlyHMSM(0,0,1,0,OS_OPT_TIME_PERIODIC,&err);
		printf("try SIM800C again \r\n");
	}
	
	printf("SIM800C onsite \r\n");
	
	ret = getCCID(iccidInfo);
	printf("ccid %x %x %x %x \r\n", iccidInfo[0], iccidInfo[1], iccidInfo[18], iccidInfo[19]);
	
  while(checkGPRSEnv() != 0){		
    OSTimeDlyHMSM(0,0,3,0,OS_OPT_TIME_PERIODIC,&err);
    printf("try GPRS env again \r\n");
  }
	
  printf("signal ok, registered, gprs attached \r\n");
	
	if(fetchNetworkTime(buf) == 0){
	  printf("%d/%d/%d %d:%d:%d\r\n", buf[0], buf[1],buf[2], buf[3], buf[4], buf[5]);
	}
	
	ntpProcess(buf);
		
	while(1){
		printf("[%d]modem task wait ...\r\n", OSTimeGet(&err));
	  flags = OSFlagPend((OS_FLAG_GRP*)&ModemEventFlags,
		       (OS_FLAGS	)NTP_TIMER_FLAG+GPRS_SEND_FLAG+KEY0_FLAG,
		     	 (OS_TICK   )0,
				   (OS_OPT	  )OS_OPT_PEND_FLAG_SET_ANY+OS_OPT_PEND_FLAG_CONSUME,
				   (CPU_TS*   )0,
				   (OS_ERR*	  )&err);
		printf("[%d]modem task notified with %x\r\n", OSTimeGet(&err), flags);
	  if(flags & NTP_TIMER_FLAG){
       ntpProcess(buf);
		}else if(flags & GPRS_SEND_FLAG){
			//OSTmrStop(&tmr1,OS_OPT_TMR_NONE,0,&err);
		  
			if(queryCellId(gprs_Buf+65, gprs_Buf+82) == 0){
			  //printf("CellId %s \r\n", buf);
				printf("gprs buf from ccid %s \r\n", gprs_Buf+5);
			}else{
			  printf("cellid fail \r\n");
			}
			
			gprs_Buf[99] = xorVerify(gprs_Buf+2, 97);
			
			//sim800c_gprs_transparentMode(gprs_Buf, 100);
			sim800c_gprs_tcp(gprs_Buf, 100);
		}else if(flags & KEY0_FLAG){
		  sim800c_gprs_transparentMode(gprs_Buf, 100);
		}
  }
	
	OSMemPut((OS_MEM*	)&INTERNAL_MEM,		//释放内存
							 (void*		)buf,
							 (OS_ERR* 	)&err);
}

void ntpProcess(u8 * buf){
	OS_ERR err;
	
	printf("ntpProcess ...\r\n");
	
  if(ntp_update() == 0) {
		OSTimeDlyHMSM(0,0,5,0,OS_OPT_TIME_PERIODIC,&err);
		memset(buf, 0, INTERNAL_MEMBLOCK_SIZE);
		if(fetchNetworkTime(buf) == 0){
	    printf("%d/%d/%d %d:%d:%d\r\n", buf[0], buf[1], buf[2], buf[3], buf[4], buf[5]);
			if(buf[0] < 18){
			  printf("ntp time is wrong, try ntp again \r\n");
				OSTmrStart(&tmr_ntp,&err);
			}else{
			  printf("ntp time is ok, signal rtc writing\r\n");
				postRTCUpdateEvent();
			}
		}
	}else{
		printf("ntp process fail, try 10s later /r/n");
	  OSTmrStart(&tmr_ntp,&err);
	}
}


void tmr1_callback(void *p_tmr,void *p_arg)
{
	printf("timer 1 tick \r\n");
	postTimerEvent(1);
}

void tmr_ntp_callback(void *p_tmr,void *p_arg)
{
	printf("timer ntp tick \r\n");
	postTimerEvent(2);
}


void postRTCUpdateEvent(void){
	OS_ERR err;
	OS_FLAGS flags_num;
  flags_num=OSFlagPost((OS_FLAG_GRP*)&EventFlags,
								 (OS_FLAGS	  )RTC_UPDATE_FLAG,
								 (OS_OPT	  )OS_OPT_POST_FLAG_SET,
					       (OS_ERR*	  )&err);
}

void postGprsSendEvent(void){
	OS_ERR err;
	OS_FLAGS flags_num;
  flags_num=OSFlagPost((OS_FLAG_GRP*)&ModemEventFlags,
								 (OS_FLAGS	  )GPRS_SEND_FLAG,
								 (OS_OPT	  )OS_OPT_POST_FLAG_SET,
					       (OS_ERR*	  )&err);
}

void postKeyEvent(u8 keynum){
	OS_FLAGS flags_num;
	OS_ERR   err;	
	
	if(keynum ==0){
		flags_num=OSFlagPost((OS_FLAG_GRP*)&ModemEventFlags,
								 (OS_FLAGS	  )KEY0_FLAG,
								 (OS_OPT	  )OS_OPT_POST_FLAG_SET,
					       (OS_ERR*	  )&err);
	}else if(keynum == 1){
		flags_num=OSFlagPost((OS_FLAG_GRP*)&EventFlags,
								 (OS_FLAGS	  )KEY1_FLAG,
								 (OS_OPT	  )OS_OPT_POST_FLAG_SET,
					       (OS_ERR*	  )&err);
	}else if(keynum == 2){
	  flags_num=OSFlagPost((OS_FLAG_GRP*)&EventFlags,
								 (OS_FLAGS	  )KEYWKUP_LOW_FLAG,
								 (OS_OPT	  )OS_OPT_POST_FLAG_SET,
					       (OS_ERR*	  )&err);
	}else if(keynum == 3){
	  flags_num=OSFlagPost((OS_FLAG_GRP*)&EventFlags,
								 (OS_FLAGS	  )KEYWKUP_HIGH_FLAG,
								 (OS_OPT	  )OS_OPT_POST_FLAG_SET,
					       (OS_ERR*	  )&err);
	}
		
	printf("[%d]post key %d event 0x%x\r\n", keynum, OSTimeGet(&err), flags_num);
}

void postTimerEvent(u8 timer_num){
	OS_FLAGS flags_num;
	OS_ERR   err;	
	
	if(timer_num == 1){
	  flags_num=OSFlagPost((OS_FLAG_GRP*)&EventFlags,
								 (OS_FLAGS	  )TIMER_FLAG,
								 (OS_OPT	  )OS_OPT_POST_FLAG_SET,
					       (OS_ERR*	  )&err);
	}else if(timer_num == 2){
	   flags_num=OSFlagPost((OS_FLAG_GRP*)&ModemEventFlags,
								 (OS_FLAGS	  )NTP_TIMER_FLAG,
								 (OS_OPT	  )OS_OPT_POST_FLAG_SET,
					       (OS_ERR*	  )&err);
	}
	
	printf("[%d]post timer %d event 0x%x\r\n", OSTimeGet(&err), timer_num, flags_num);
}
