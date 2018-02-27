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


//UCOSIII���������ȼ��û�������ʹ�ã�ALIENTEK
//����Щ���ȼ��������UCOSIII��5��ϵͳ�ڲ�����
//���ȼ�0���жϷ������������� OS_IntQTask()
//���ȼ�1��ʱ�ӽ������� OS_TickTask()
//���ȼ�2����ʱ���� OS_TmrTask()
//���ȼ�OS_CFG_PRIO_MAX-2��ͳ������ OS_StatTask()
//���ȼ�OS_CFG_PRIO_MAX-1���������� OS_IdleTask()
//����֧�֣�www.openedv.com
//�Ա����̣�http://eboard.taobao.com  
//������������ӿƼ����޹�˾  
//���ߣ�����ԭ�� @ALIENTEK

//�������ȼ�
#define START_TASK_PRIO		3
//�����ջ��С	
#define START_STK_SIZE 		512
//������ƿ�
OS_TCB StartTaskTCB;
//�����ջ	
CPU_STK START_TASK_STK[START_STK_SIZE];
//������
void start_task(void *p_arg);

//�������ȼ�
#define MAIN_TASK_PRIO		4
//�����ջ��С	
#define MAIN_STK_SIZE 		128
//������ƿ�
OS_TCB MainTaskTCB;
//�����ջ	
CPU_STK MAIN_TASK_STK[MAIN_STK_SIZE];
void main_task(void *p_arg);

//�������ȼ�
#define MODEM_TASK_PRIO		5
//�����ջ��С	
#define MODEM_STK_SIZE 		256
//������ƿ�
OS_TCB ModemTaskTCB;
//�����ջ	
CPU_STK MODEM_TASK_STK[MODEM_STK_SIZE];
void modem_task(void *p_arg);

////////////////////////�¼���־��//////////////////////////////
					
OS_FLAG_GRP	EventFlags;
OS_FLAG_GRP ModemEventFlags;
OS_TMR tmr1;	//����һ����ʱ��
OS_TMR tmr_ntp; 
void tmr1_callback(void *p_tmr,void *p_arg); //��ʱ��1�ص�����
void tmr_ntp_callback(void *p_tmr,void *p_arg); //��ʱ��1�ص�����

OS_MEM INTERNAL_MEM;	
//�洢���д洢������
#define INTERNAL_MEM_NUM		5
//ÿ���洢���С
//����һ��ָ�����ռ��4�ֽ����Կ�Ĵ�Сһ��ҪΪ4�ı���
//���ұ������һ��ָ�����(4�ֽ�)ռ�õĿռ�,����Ļ��洢�鴴�����ɹ�
#define INTERNAL_MEMBLOCK_SIZE	100  
//�洢�����ڴ�أ�ʹ���ڲ�RAM
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
	
	delay_init();       //��ʱ��ʼ��
	NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2); //�жϷ�������
	uart_init(115200);    //���ڲ���������
	LED_Init();         //LED��ʼ��
	//KEY_Init();
	EXTIX_Init();
	USART2_Init(115200);
	LED0 = 1;
	LED1 = 1;
	useCount = 0;
	printf("hw init end\r\n");
	
	OSInit(&err);		//��ʼ��UCOSIII
	OS_CRITICAL_ENTER();//�����ٽ���
	
	OSMemCreate((OS_MEM*	)&INTERNAL_MEM,
				(CPU_CHAR*	)"Internal Mem",
				(void*		)&Internal_RamMemp[0][0],
				(OS_MEM_QTY	)INTERNAL_MEM_NUM,
				(OS_MEM_SIZE)INTERNAL_MEMBLOCK_SIZE,
				(OS_ERR*	)&err);
				
	//������ʼ����
	OSTaskCreate((OS_TCB 	* )&StartTaskTCB,		//������ƿ�
				 (CPU_CHAR	* )"start task", 		//��������
                 (OS_TASK_PTR )start_task, 			//������
                 (void		* )0,					//���ݸ��������Ĳ���
                 (OS_PRIO	  )START_TASK_PRIO,     //�������ȼ�
                 (CPU_STK   * )&START_TASK_STK[0],	//�����ջ����ַ
                 (CPU_STK_SIZE)START_STK_SIZE/10,	//�����ջ�����λ
                 (CPU_STK_SIZE)START_STK_SIZE,		//�����ջ��С
                 (OS_MSG_QTY  )0,					//�����ڲ���Ϣ�����ܹ����յ������Ϣ��Ŀ,Ϊ0ʱ��ֹ������Ϣ
                 (OS_TICK	  )0,					//��ʹ��ʱ��Ƭ��תʱ��ʱ��Ƭ���ȣ�Ϊ0ʱΪĬ�ϳ��ȣ�
                 (void   	* )0,					//�û�����Ĵ洢��
                 (OS_OPT      )OS_OPT_TASK_STK_CHK|OS_OPT_TASK_STK_CLR, //����ѡ��
                 (OS_ERR 	* )&err);				//��Ÿú�������ʱ�ķ���ֵ
	OS_CRITICAL_EXIT();	//�˳��ٽ���	 
	OSStart(&err);  //����UCOSIII
	while(1);
}

//��ʼ������
void start_task(void *p_arg)
{
	OS_ERR err;
	CPU_SR_ALLOC();
	p_arg = p_arg;

	g_ntpUpdateFlag = 0;
	
	CPU_Init();
#if OS_CFG_STAT_TASK_EN > 0u
   OSStatTaskCPUUsageInit(&err);  	//ͳ������                
#endif
	
#ifdef CPU_CFG_INT_DIS_MEAS_EN		//���ʹ���˲����жϹر�ʱ��
   CPU_IntDisMeasMaxCurReset();	
#endif
	
#if	OS_CFG_SCHED_ROUND_ROBIN_EN  //��ʹ��ʱ��Ƭ��ת��ʱ��
	 //ʹ��ʱ��Ƭ��ת���ȹ���,ʱ��Ƭ����Ϊ1��ϵͳʱ�ӽ��ģ���1*5=5ms
	OSSchedRoundRobinCfg(DEF_ENABLED,1,&err);  
#endif		
	
	OS_CRITICAL_ENTER();	//�����ٽ���
	OSFlagCreate((OS_FLAG_GRP*)&EventFlags,		//ָ���¼���־��
                 (CPU_CHAR*	  )"Event Flags",	//����
                 (OS_FLAGS	  )KEYFLAGS_VALUE,	//�¼���־���ʼֵ
                 (OS_ERR*  	  )&err);			//������
  
	OSFlagCreate((OS_FLAG_GRP*)&ModemEventFlags,		//ָ���¼���־��
                 (CPU_CHAR*	  )"Modem Event Flags",	//����
                 (OS_FLAGS	  )KEYFLAGS_VALUE,	//�¼���־���ʼֵ
                 (OS_ERR*  	  )&err);			//������
								 
	OSTmrCreate((OS_TMR		*)&tmr_ntp,		//��ʱ��1
                (CPU_CHAR	*)"tmr ntp",		//��ʱ������
                (OS_TICK	 )1000,			//1000* 10 =10s
                (OS_TICK	 )0,          //
                (OS_OPT		 )OS_OPT_TMR_ONE_SHOT, //����ģʽ
                (OS_TMR_CALLBACK_PTR)tmr_ntp_callback,//��ʱ��1�ص�����
                (void	    *)0,			//����Ϊ0
                (OS_ERR	    *)&err);		//���صĴ�����							 

	OSTmrCreate((OS_TMR		*)&tmr1,		//��ʱ��1
                (CPU_CHAR	*)"tmr1",		//��ʱ������
                (OS_TICK	 )0,			//0ms
                (OS_TICK	 )500,          //200*10=2000ms
                (OS_OPT		 )OS_OPT_TMR_PERIODIC, //����ģʽ
                (OS_TMR_CALLBACK_PTR)tmr1_callback,//��ʱ��1�ص�����
                (void	    *)0,			//����Ϊ0
                (OS_ERR	    *)&err);		//���صĴ�����

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
	
	OS_CRITICAL_EXIT();	//�����ٽ���
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
	
	OSMemPut((OS_MEM*	)&INTERNAL_MEM,		//�ͷ��ڴ�
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
