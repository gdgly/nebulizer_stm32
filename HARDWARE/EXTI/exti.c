#include "exti.h"
#include "led.h"
#include "key.h"
#include "delay.h"
#include "usart.h"
#include "nebulizer.h"
#if SYSTEM_SUPPORT_OS
#include "includes.h"					//os 使用	  
#endif
//////////////////////////////////////////////////////////////////////////////////	 
//本程序只供学习使用，未经作者许可，不得用于其它任何用途
//Mini STM32开发板
//外部中断 驱动代码			   
//正点原子@ALIENTEK
//技术论坛:www.openedv.com
//修改日期:2010/12/01  
//版本：V1.0
//版权所有，盗版必究。
//Copyright(C) 正点原子 2009-2019
//All rights reserved	  
////////////////////////////////////////////////////////////////////////////////// 	  
 
extern OS_FLAG_GRP	EventFlags;
//外部中断初始化函数
void EXTIX_Init(void)
{
 	  EXTI_InitTypeDef EXTI_InitStructure;
 	  NVIC_InitTypeDef NVIC_InitStructure;

  	RCC_APB2PeriphClockCmd(RCC_APB2Periph_AFIO,ENABLE);//外部中断，需要使能AFIO时钟

	  KEY_Init();//初始化按键对应io模式

    //GPIOC.5 中断线以及中断初始化配置
  	GPIO_EXTILineConfig(GPIO_PortSourceGPIOC,GPIO_PinSource5);

  	EXTI_InitStructure.EXTI_Line=EXTI_Line5;
  	EXTI_InitStructure.EXTI_Mode = EXTI_Mode_Interrupt;	
  	EXTI_InitStructure.EXTI_Trigger = EXTI_Trigger_Falling;//trigger in dual edge
  	EXTI_InitStructure.EXTI_LineCmd = ENABLE;
  	EXTI_Init(&EXTI_InitStructure);	 	//根据EXTI_InitStruct中指定的参数初始化外设EXTI寄存器

    //GPIOA.15	  中断线以及中断初始化配置
  	GPIO_EXTILineConfig(GPIO_PortSourceGPIOA,GPIO_PinSource15);

  	EXTI_InitStructure.EXTI_Line=EXTI_Line15;
  	EXTI_InitStructure.EXTI_Mode = EXTI_Mode_Interrupt;	
  	EXTI_InitStructure.EXTI_Trigger = EXTI_Trigger_Falling;
  	EXTI_InitStructure.EXTI_LineCmd = ENABLE;
  	EXTI_Init(&EXTI_InitStructure);	  	//根据EXTI_InitStruct中指定的参数初始化外设EXTI寄存?
		
		    //GPIOA.0	
  	GPIO_EXTILineConfig(GPIO_PortSourceGPIOA,GPIO_PinSource0);

   	EXTI_InitStructure.EXTI_Line=EXTI_Line0;
  	EXTI_InitStructure.EXTI_Mode = EXTI_Mode_Interrupt;	
  	EXTI_InitStructure.EXTI_Trigger = EXTI_Trigger_Rising_Falling;
  	EXTI_InitStructure.EXTI_LineCmd = ENABLE;
  	EXTI_Init(&EXTI_InitStructure);
		
		
		NVIC_InitStructure.NVIC_IRQChannel = EXTI9_5_IRQn;			//使能按键所在的外部中断通道
  	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0x02;	//抢占优先级2， 
  	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0x01;					//子优先级1
  	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;								//使能外部中断通道
  	NVIC_Init(&NVIC_InitStructure); 
 
    NVIC_InitStructure.NVIC_IRQChannel = EXTI15_10_IRQn;			//使能按键所在的外部中断通道
  	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0x02;	//抢占优先级2， 
  	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0x00;					//子优先级1
  	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;								//使能外部中断通道
  	NVIC_Init(&NVIC_InitStructure); 
		
  	NVIC_InitStructure.NVIC_IRQChannel = EXTI0_IRQn;			//?????????????
  	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0x02;	//?????2 
  	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0x02;					//????1
  	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;								//????????
  	NVIC_Init(&NVIC_InitStructure);  	  //??NVIC_InitStruct???????????NVIC???
}

 void EXTI9_5_IRQHandler(void)
{
  OS_ERR err;	
	
	OSIntEnter();
	delay_ms(10);   //消抖			 
	if(KEY0 == 0) {
		//LED0 = 0;
		printf("[%d]key0 pressed\r\n", OSTimeGet(&err));
		postKeyEvent(KEY0_FLAG);
  }
 	EXTI_ClearITPendingBit(EXTI_Line5);    //清除LINE5上的中断标志位  

	OSIntExit();  											 

}

void EXTI15_10_IRQHandler(void)
{
	OS_ERR err;	
	
	OSIntEnter();
  delay_ms(10);    //消抖			 
  if(KEY1==0)	{
		//LED1 = 0;
		printf("[%d]key1 pressed\r\n", OSTimeGet(&err));
		postKeyEvent(KEY1_FLAG);
	}
	
	EXTI_ClearITPendingBit(EXTI_Line15);  //清除LINE15线路挂起位
	OSIntExit(); 
}

void EXTI0_IRQHandler(void)
{
	OS_ERR err;	
	
	OSIntEnter();
  delay_ms(10); 
	if(WK_UP==1)
	{	  
		printf("[%d]key wakeup pressed\r\n", OSTimeGet(&err));
		postKeyEvent(KEYWKUP_HIGH_FLAG);
	}else if(WK_UP==0){
  	printf("[%d]key wakeup released\r\n", OSTimeGet(&err));
		postKeyEvent(KEYWKUP_LOW_FLAG);
	}
	
	EXTI_ClearITPendingBit(EXTI_Line0);
	OSIntExit();
}

