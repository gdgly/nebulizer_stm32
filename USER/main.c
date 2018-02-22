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
#define MODEM_STK_SIZE 		128
//������ƿ�
OS_TCB ModemTaskTCB;
//�����ջ	
CPU_STK MODEM_TASK_STK[MODEM_STK_SIZE];
void modem_task(void *p_arg);

////////////////////////�¼���־��//////////////////////////////
					
OS_FLAG_GRP	EventFlags;
OS_TMR tmr1;	//����һ����ʱ��
OS_TMR tmr2; 
void tmr1_callback(void *p_tmr,void *p_arg); //��ʱ��1�ص�����
void tmr2_callback(void *p_tmr,void *p_arg); //��ʱ��1�ص�����

OS_MEM INTERNAL_MEM;	
//�洢���д洢������
#define INTERNAL_MEM_NUM		5
//ÿ���洢���С
//����һ��ָ�����ռ��4�ֽ����Կ�Ĵ�Сһ��ҪΪ4�ı���
//���ұ������һ��ָ�����(4�ֽ�)ռ�õĿռ�,����Ļ��洢�鴴�����ɹ�
#define INTERNAL_MEMBLOCK_SIZE	100  
//�洢�����ڴ�أ�ʹ���ڲ�RAM
CPU_INT08U Internal_RamMemp[INTERNAL_MEM_NUM][INTERNAL_MEMBLOCK_SIZE];

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
								 
	OSTmrCreate((OS_TMR		*)&tmr2,		//��ʱ��1
                (CPU_CHAR	*)"tmr2",		//��ʱ������
                (OS_TICK	 )500,			//0ms
                (OS_TICK	 )0,          //200*10=2000ms
                (OS_OPT		 )OS_OPT_TMR_ONE_SHOT, //����ģʽ
                (OS_TMR_CALLBACK_PTR)tmr2_callback,//��ʱ��1�ص�����
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
	p_arg = p_arg;
	
	OSTmrStart(&tmr1,&err);
	
	RTCret = RTC_Init();
	while(1)
	{
		printf("[%d]main task wait ...\r\n", OSTimeGet(&err));
		LED0 = 0;
		flags = OSFlagPend((OS_FLAG_GRP*)&EventFlags,
		       (OS_FLAGS	)KEY1_FLAG,
		     	 (OS_TICK   )0,
				   (OS_OPT	  )OS_OPT_PEND_FLAG_SET_ANY+OS_OPT_PEND_FLAG_CONSUME,
				   (CPU_TS*   )0,
				   (OS_ERR*	  )&err);
		printf("[%d]main task notified with %x\r\n", OSTimeGet(&err), flags);
		
		if(flags & TIMER_FLAG){
			printf("timer event \r\n");
			if(RTCret != 0) {
			  RTCret = RTC_Init();
				printf("rtc init again \r\n");
			}else{
			    //RTC_Get();
				  //printf("%d/%d/%d %d:%d:%d\r\n", calendar.w_year, calendar.w_month,calendar.w_date,calendar.hour,calendar.min,calendar.sec);
			  
			}
		}
		
		//OSTimeDlyHMSM(0,0,1,0,OS_OPT_TIME_PERIODIC,&err);   //��ʱ1s
		
		
	}
}


void modem_task(void *p_arg)
{
	OS_ERR err;
	OS_FLAGS flags;
	CPU_INT08U *buf;
	CPU_INT08U *cellId;
	u8 SignalQ;
	p_arg = p_arg;
	while(checkSIM800HW() !=0)
	{
		OSTimeDlyHMSM(0,0,0,200,OS_OPT_TIME_PERIODIC,&err);
		printf("try SIM800C again \r\n");
	}
	buf=OSMemGet((OS_MEM*)&INTERNAL_MEM,
								      (OS_ERR*)&err);
	while(1){
	  flags = OSFlagPend((OS_FLAG_GRP*)&EventFlags,
		       (OS_FLAGS	)KEY0_FLAG+TIMER_FLAG+NTP_TIMER_FLAG,
		     	 (OS_TICK   )0,
				   (OS_OPT	  )OS_OPT_PEND_FLAG_SET_ANY+OS_OPT_PEND_FLAG_CONSUME,
				   (CPU_TS*   )0,
				   (OS_ERR*	  )&err);
		printf("[%d]modem task notified with %x\r\n", OSTimeGet(&err), flags);
	  if(flags & TIMER_FLAG){
			RTC_Get();
			printf("%d/%d/%d %d:%d:%d\r\n", calendar.w_year, calendar.w_month,calendar.w_date,calendar.hour,calendar.min,calendar.sec);
		  SignalQ = checkGSMSignalQuality(buf);
		  printf("SQ %d \r\n", SignalQ);
		  SignalQ = queryCellId(buf);
			if(fetchNetworkTime(buf) == 0){
			    printf("%d/%d/%d %d:%d:%d\r\n", buf[0], buf[1],buf[2], buf[3], buf[4], buf[5]);
			}
		}else if(flags & KEY0_FLAG){
			
			
			OSTmrStop(&tmr1,OS_OPT_TMR_NONE,0,&err);
			
		  if(ntp_update() == 0) {
			  //memset(buf , 0, INTERNAL_MEMBLOCK_SIZE);
				//delay_ms(1000);
			  OSTmrStart(&tmr2,&err);
				printf("timer2 start %x \r\n", err);
			} else{
				OSTmrStart(&tmr1,&err);
			}
		}else if(flags & NTP_TIMER_FLAG){
			u16 year, hour;
			if(fetchNetworkTime(buf) == 0){
			    printf("%d/%d/%d %d:%d:%d\r\n", buf[0], buf[1],buf[2], buf[3], buf[4], buf[5]);
			    year = 2000 + buf[0];
				  hour = buf[3] + 6;
			    RTC_Set(year, buf[1],buf[2], hour, buf[4], buf[5]);
			}
			OSTmrStart(&tmr1,&err);
		}
  }
	
	OSMemPut((OS_MEM*	)&INTERNAL_MEM,		//�ͷ��ڴ�
							 (void*		)buf,
							 (OS_ERR* 	)&err);
}

void tmr1_callback(void *p_tmr,void *p_arg)
{
	printf("timer 1 tick \r\n");
	postTimerEvent(1);
}

void tmr2_callback(void *p_tmr,void *p_arg)
{
	printf("timer 2 tick \r\n");
	postTimerEvent(2);
}

void postKey0Event(void){
	OS_FLAGS flags_num;
	OS_ERR   err;	
		
	flags_num=OSFlagPost((OS_FLAG_GRP*)&EventFlags,
								 (OS_FLAGS	  )KEY0_FLAG,
								 (OS_OPT	  )OS_OPT_POST_FLAG_SET,
					       (OS_ERR*	  )&err);
	
	printf("[%d]post key0 event 0x%x \r\n", OSTimeGet(&err), flags_num);
}

void postKey1Event(void){
	OS_FLAGS flags_num;
	OS_ERR   err;	
	
	flags_num=OSFlagPost((OS_FLAG_GRP*)&EventFlags,
								 (OS_FLAGS	  )KEY1_FLAG,
								 (OS_OPT	  )OS_OPT_POST_FLAG_SET,
					       (OS_ERR*	  )&err);
	
	printf("[%d]post key1 event 0x%x\r\n", OSTimeGet(&err), flags_num);
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
	   flags_num=OSFlagPost((OS_FLAG_GRP*)&EventFlags,
								 (OS_FLAGS	  )NTP_TIMER_FLAG,
								 (OS_OPT	  )OS_OPT_POST_FLAG_SET,
					       (OS_ERR*	  )&err);
	}
	
	printf("[%d]post timer %d event 0x%x\r\n", OSTimeGet(&err), timer_num, flags_num);
}
