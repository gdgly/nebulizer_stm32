#include "exti.h"
#include "led.h"
#include "key.h"
#include "delay.h"
#include "usart.h"
#include "nebulizer.h"
#if SYSTEM_SUPPORT_OS
#include "includes.h"					//os Ê¹ÓÃ	  
#endif
//////////////////////////////////////////////////////////////////////////////////	 
//±¾³ÌĞòÖ»¹©Ñ§Ï°Ê¹ÓÃ£¬Î´¾­×÷ÕßĞí¿É£¬²»µÃÓÃÓÚÆäËüÈÎºÎÓÃÍ¾
//Mini STM32¿ª·¢°å
//Íâ²¿ÖĞ¶Ï Çı¶¯´úÂë			   
//ÕıµãÔ­×Ó@ALIENTEK
//¼¼ÊõÂÛÌ³:www.openedv.com
//ĞŞ¸ÄÈÕÆÚ:2010/12/01  
//°æ±¾£ºV1.0
//°æÈ¨ËùÓĞ£¬µÁ°æ±Ø¾¿¡£
//Copyright(C) ÕıµãÔ­×Ó 2009-2019
//All rights reserved	  
////////////////////////////////////////////////////////////////////////////////// 	  
 
extern OS_FLAG_GRP	EventFlags;
//Íâ²¿ÖĞ¶Ï³õÊ¼»¯º¯Êı
void EXTIX_Init(void)
{
 	  EXTI_InitTypeDef EXTI_InitStructure;
 	  NVIC_InitTypeDef NVIC_InitStructure;

  	RCC_APB2PeriphClockCmd(RCC_APB2Periph_AFIO,ENABLE);//Íâ²¿ÖĞ¶Ï£¬ĞèÒªÊ¹ÄÜAFIOÊ±ÖÓ

	  KEY_Init();//³õÊ¼»¯°´¼ü¶ÔÓ¦ioÄ£Ê½

    //GPIOC.5 ÖĞ¶ÏÏßÒÔ¼°ÖĞ¶Ï³õÊ¼»¯ÅäÖÃ
  	GPIO_EXTILineConfig(GPIO_PortSourceGPIOC,GPIO_PinSource5);

  	EXTI_InitStructure.EXTI_Line=EXTI_Line5;
  	EXTI_InitStructure.EXTI_Mode = EXTI_Mode_Interrupt;	
  	EXTI_InitStructure.EXTI_Trigger = EXTI_Trigger_Falling;//trigger in dual edge
  	EXTI_InitStructure.EXTI_LineCmd = ENABLE;
  	EXTI_Init(&EXTI_InitStructure);	 	//¸ù¾İEXTI_InitStructÖĞÖ¸¶¨µÄ²ÎÊı³õÊ¼»¯ÍâÉèEXTI¼Ä´æÆ÷

    //GPIOA.15	  ÖĞ¶ÏÏßÒÔ¼°ÖĞ¶Ï³õÊ¼»¯ÅäÖÃ
  	GPIO_EXTILineConfig(GPIO_PortSourceGPIOA,GPIO_PinSource15);

  	EXTI_InitStructure.EXTI_Line=EXTI_Line15;
  	EXTI_InitStructure.EXTI_Mode = EXTI_Mode_Interrupt;	
  	EXTI_InitStructure.EXTI_Trigger = EXTI_Trigger_Falling;
  	EXTI_InitStructure.EXTI_LineCmd = ENABLE;
  	EXTI_Init(&EXTI_InitStructure);	  	//¸ù¾İEXTI_InitStructÖĞÖ¸¶¨µÄ²ÎÊı³õÊ¼»¯ÍâÉèEXTI¼Ä´æÆ
		
		    //GPIOA.0	
  	GPIO_EXTILineConfig(GPIO_PortSourceGPIOA,GPIO_PinSource0);

   	EXTI_InitStructure.EXTI_Line=EXTI_Line0;
  	EXTI_InitStructure.EXTI_Mode = EXTI_Mode_Interrupt;	
  	EXTI_InitStructure.EXTI_Trigger = EXTI_Trigger_Rising_Falling;
  	EXTI_InitStructure.EXTI_LineCmd = ENABLE;
  	EXTI_Init(&EXTI_InitStructure);
		
		
		NVIC_InitStructure.NVIC_IRQChannel = EXTI9_5_IRQn;			//Ê¹ÄÜ°´¼üËùÔÚµÄÍâ²¿ÖĞ¶ÏÍ¨µÀ
  	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0x02;	//ÇÀÕ¼ÓÅÏÈ¼¶2£¬ 
  	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0x01;					//×ÓÓÅÏÈ¼¶1
  	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;								//Ê¹ÄÜÍâ²¿ÖĞ¶ÏÍ¨µÀ
  	NVIC_Init(&NVIC_InitStructure); 
 
    NVIC_InitStructure.NVIC_IRQChannel = EXTI15_10_IRQn;			//Ê¹ÄÜ°´¼üËùÔÚµÄÍâ²¿ÖĞ¶ÏÍ¨µÀ
  	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0x02;	//ÇÀÕ¼ÓÅÏÈ¼¶2£¬ 
  	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0x00;					//×ÓÓÅÏÈ¼¶1
  	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;								//Ê¹ÄÜÍâ²¿ÖĞ¶ÏÍ¨µÀ
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
	delay_ms(10);   //Ïû¶¶			 
	if(KEY0 == 0) {
		//LED0 = 0;
		printf("[%d]key0 pressed\r\n", OSTimeGet(&err));
		postKeyEvent(0);
  }
 	EXTI_ClearITPendingBit(EXTI_Line5);    //Çå³ıLINE5ÉÏµÄÖĞ¶Ï±êÖ¾Î»  

	OSIntExit();  											 

}

void EXTI15_10_IRQHandler(void)
{
	OS_ERR err;	
	
	OSIntEnter();
  delay_ms(10);    //Ïû¶¶			 
  if(KEY1==0)	{
		//LED1 = 0;
		printf("[%d]key1 pressed\r\n", OSTimeGet(&err));
		postKeyEvent(1);
	}
	
	EXTI_ClearITPendingBit(EXTI_Line15);  //Çå³ıLINE15ÏßÂ·¹ÒÆğÎ»
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
		postKeyEvent(3);
	}else if(WK_UP==0){
  	printf("[%d]key wakeup released\r\n", OSTimeGet(&err));
		postKeyEvent(2);
	}
	
	EXTI_ClearITPendingBit(EXTI_Line0);
	OSIntExit();
}

