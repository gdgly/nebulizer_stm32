#include "sim800c.h"
#include "usart.h"		
#include "delay.h"	
#include "led.h"   	 
#include "key.h"
#include "string.h"    
#include "usart2.h" 


//////////////////////////////////////////////////////////////////////////////////	 
//������ֻ��ѧϰʹ�ã�δ��������ɣ��������������κ���;
//ALIENTEK STM32������
//ATK-SIM800C GSM/GPRSģ������	  
//����ԭ��@ALIENTEK
//������̳:www.openedv.com
//�޸�����:2016/4/1
//�汾��V1.0
//��Ȩ���У�����ؾ���
//Copyright(C) ������������ӿƼ����޹�˾ 2009-2019
//All rights reserved	
//********************************************************************************
//��

u8 Scan_Wtime = 0;//����ɨ����Ҫ��ʱ��
u8 BT_Scan_mode=0;//����ɨ���豸ģʽ��־

//usmart֧�ֲ��� 
//���յ���ATָ��Ӧ�����ݷ��ظ����Դ���
//mode:0,������USART2_RX_STA;
//     1,����USART2_RX_STA;
void sim_at_response(u8 mode)
{
	if(USART2_RX_STA&0X8000)		//���յ�һ��������
	{ 
		USART2_RX_BUF[USART2_RX_STA&0X7FFF]=0;//��ӽ�����
		printf("%s",USART2_RX_BUF);	//���͵�����
		if(mode)USART2_RX_STA=0;
	} 
}
//////////////////////////////////////////////////////////////////////////////////
//ATK-SIM800C �������(���Ų��ԡ����Ų��ԡ�GPRS���ԡ���������)���ô���

//sim800C���������,�����յ���Ӧ��
//str:�ڴ���Ӧ����
//����ֵ:0,û�еõ��ڴ���Ӧ����
//����,�ڴ�Ӧ������λ��(str��λ��)
u8* sim800c_check_cmd(u8 *str)
{
	char *strx=0;
	if(USART2_RX_STA&0X8000)		//���յ�һ��������
	{ 
		USART2_RX_BUF[USART2_RX_STA&0X7FFF]=0;//��ӽ�����
		strx=strstr((const char*)USART2_RX_BUF,(const char*)str);
	} 
	return (u8*)strx;
}
//��sim800C��������
//cmd:���͵������ַ���(����Ҫ��ӻس���),��cmd<0XFF��ʱ��,��������(���緢��0X1A),���ڵ�ʱ�����ַ���.
//ack:�ڴ���Ӧ����,���Ϊ��,���ʾ����Ҫ�ȴ�Ӧ��
//waittime:�ȴ�ʱ��(��λ:10ms)
//����ֵ:0,���ͳɹ�(�õ����ڴ���Ӧ����)
//       1,����ʧ��
u8 sim800c_send_cmd(u8 *cmd,u8 *ack,u16 waittime)
{
	u8 res=0; 
	USART2_RX_STA=0;
	if((u32)cmd<=0XFF) {
		while(DMA1_Channel7->CNDTR!=0);	//�ȴ�ͨ��7�������   
		USART2->DR=(u32)cmd;
	} else {
		u2_printf("%s\r\n",cmd);//��������
	}
		
	if(ack&&waittime)		//��Ҫ�ȴ�Ӧ��
	{ 
	   while(--waittime)	//�ȴ�����ʱ
	   {
		   delay_ms(10);
		   if(USART2_RX_STA&0X8000)//���յ��ڴ���Ӧ����
		   {
			   if(sim800c_check_cmd(ack)){
				   break;//�õ���Ч���� 
				 }else{
				   printf("cmd not match \r\n");
				 }
			   USART2_RX_STA=0;
		   } 
	   }
	   if(waittime==0)
			 res=1; 
	}
	return res;
} 

//����SIM800C�������ݣ���������ģʽ��ʹ�ã�
//request:�ڴ����������ַ���
//waittimg:�ȴ�ʱ��(��λ��10ms)
//����ֵ:0,���ͳɹ�(�õ����ڴ���Ӧ����)
//       1,����ʧ��
u8 sim800c_wait_request(u8 *request ,u16 waittime)
{
	 u8 res = 1;
	 u8 key;
	 if(request && waittime)
	 {
		while(--waittime)
		{   
		   key=KEY_Scan(0);
		   if(key==WKUP_PRES) return 2;//������һ��
		   delay_ms(10);
		   if(USART2_RX_STA &0x8000)//���յ��ڴ���Ӧ����
		   {
			  if(sim800c_check_cmd(request)) break;//�õ���Ч����
			  USART2_RX_STA=0;
		   }
		}
		if(waittime==0)res=0;
	 }
	 return res;
}

//��1���ַ�ת��Ϊ16��������
//chr:�ַ�,0~9/A~F/a~F
//����ֵ:chr��Ӧ��16������ֵ
u8 sim800c_chr2hex(u8 chr)
{
	if(chr>='0'&&chr<='9')return chr-'0';
	if(chr>='A'&&chr<='F')return (chr-'A'+10);
	if(chr>='a'&&chr<='f')return (chr-'a'+10); 
	return 0;
}
//��1��16��������ת��Ϊ�ַ�
//hex:16��������,0~15;
//����ֵ:�ַ�
u8 sim800c_hex2chr(u8 hex)
{
	if(hex<=9)return hex+'0';
	if(hex>=10&&hex<=15)return (hex-10+'A'); 
	return '0';
}
//unicode gbk ת������
//src:�����ַ���
//dst:���(uni2gbkʱΪgbk����,gbk2uniʱ,Ϊunicode�ַ���)
//mode:0,unicode��gbkת��;
//     1,gbk��unicodeת��;
void sim800c_unigbk_exchange(u8 *src,u8 *dst,u8 mode)
{
}
//////////////////////////////////////////////////////////////////////////////////////////
//���Ų��Բ��ִ���

//sim800C���Ų���
//���ڲ���绰�ͽ����绰
//����ֵ:0,����
//����,�������
u8 sim800c_call_test(void)
{
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////// 
//���Ų��Բ��ִ���

//SIM800C�����Ų���
void sim800c_sms_read_test(void)
{ 
  return;
}
//���Զ��ŷ�������(70����[UCS2��ʱ��,1���ַ�/���ֶ���1����])
const u8* sim800c_test_msg="���ã�����һ�����Զ��ţ���ATK-SIM800C GSMģ�鷢�ͣ�ģ�鹺���ַ:http://eboard.taobao.com��лл֧�֣�";
//SIM800C�����Ų��� 
void sim800c_sms_send_test(void)
{

} 

//sim800C���Ų���
//���ڶ����Ż��߷�����
//����ֵ:0,����
//    ����,�������
u8 sim800c_sms_test(void)
{
  return 0;
} 
/////////////////////////////////////////////////////////////////////////////////////////////////////
//GPRS���Բ��ִ���

const u8 *modetbl[2]={"TCP","UDP"};//����ģʽ
//tcp/udp����
//����������,��ά������
//mode:0:TCP����;1,UDP����)
//ipaddr:ip��ַ
//port:�˿� 
void sim800c_tcpudp_test(u8 mode,u8* ipaddr,u8* port)
{ 

}
//gprs����������
void sim800c_gprs_ui(void)
{

} 
//sim800C GPRS����
//���ڲ���TCP/UDP����
//����ֵ:0,����
//����,�������
u8 sim800c_gprs_test(void)
{
  return 0;
}


///////////////////////////////////////////////////////////////////// 
//ATK-SIM800C GSM/GPRS�����Կ��Ʋ���

//GSM��Ϣ��ʾ(�ź�����,��ص���,����ʱ��)
//����ֵ:0,����
//����,�������
u8 sim800c_gsminfo_show(u16 x,u16 y)
{
	u8 *p,*p1,*p2;
	u8 res=0;
	//p=mymalloc(50);//����50���ֽڵ��ڴ�
	
	USART2_RX_STA=0;
	if(sim800c_send_cmd("AT+CPIN?","OK",200))res|=1<<0;	//��ѯSIM���Ƿ���λ 
	USART2_RX_STA=0;  
	if(sim800c_send_cmd("AT+COPS?","OK",200)==0)		//��ѯ��Ӫ������
	{ 
		p1=(u8*)strstr((const char*)(USART2_RX_BUF),"\""); 
		if(p1)//����Ч����
		{
			p2=(u8*)strstr((const char*)(p1+1),"\"");
			p2[0]=0;//���������			
			sprintf((char*)p,"��Ӫ��:%s",p1+1);
			
		} 
		USART2_RX_STA=0;		
	}else res|=1<<1;
	if(sim800c_send_cmd("AT+CSQ","+CSQ:",200)==0)		//��ѯ�ź�����
	{ 
		p1=(u8*)strstr((const char*)(USART2_RX_BUF),":");
		p2=(u8*)strstr((const char*)(p1),",");
		p2[0]=0;//���������
		sprintf((char*)p,"�ź�����:%s",p1+2);
		
		USART2_RX_STA=0;		
	}else res|=1<<2;
	if(sim800c_send_cmd("AT+CBC","+CBC:",200)==0)		//��ѯ��ص���
	{ 
		p1=(u8*)strstr((const char*)(USART2_RX_BUF),",");
		p2=(u8*)strstr((const char*)(p1+1),",");
		p2[0]=0;p2[5]=0;//���������
		sprintf((char*)p,"��ص���:%s%%  %smV",p1+1,p2+1);
		
		USART2_RX_STA=0;		
	}else res|=1<<3; 
	if(sim800c_send_cmd("AT+CCLK?","+CCLK:",200)==0)		//��ѯ��ص���
	{ 
		p1=(u8*)strstr((const char*)(USART2_RX_BUF),"\"");
		p2=(u8*)strstr((const char*)(p1+1),":");
		p2[3]=0;//���������
		sprintf((char*)p,"����ʱ��:%s",p1+1);
		
		USART2_RX_STA=0;		
	}else res|=1<<4; 
	//myfree(p); 
	return res;
} 
//NTP����ͬ��ʱ��
u8 ntp_update(void)
{  
	 u8 ret = 0;
	 printf("start ntp update ... \r\n");
	 if(sim800c_send_cmd("AT+SAPBR=3,1,\"Contype\",\"GPRS\"","OK",200)){
	   printf("ntp fail step 1 \r\n");
		 return 1;
   }else{
	   printf("ntp step 1 ok\r\n");
	 }
	 
	 if(sim800c_send_cmd("AT+SAPBR=3,1,\"APN\",\"CMNET\"","OK",200)){
	   printf("ntp fail step 2 \r\n");
		 return 2;
	 }else{
	   printf("ntp step 2 ok\r\n");
	 }
	 
	 if(sim800c_send_cmd("AT+SAPBR=1,1",0,200)){
	   printf("ntp fail step 3 \r\n");
		 return 3;
	 }else{
	   printf("ntp step 3 ok\r\n");
	 }
   
	 delay_ms(5);
	 
   if(sim800c_send_cmd("AT+CNTPCID=1","OK",400)){
	   printf("ntp fail step 4 \r\n");
		 ret = 4;
		 goto end;
	 }else{
	   printf("ntp step 4 ok\r\n");
	 }
	 //sim800c_send_cmd("AT+CNTP=\"202.120.2.101\",32","OK",200);     //����NTP�������ͱ���ʱ��(32ʱ�� ʱ����׼ȷ)
	 
	 if(sim800c_send_cmd("AT+CNTP=\"133.100.11.8\",8","OK",800)){
	   printf("ntp fail step 5 \r\n");
		 //return 5;
		 ret = 5;
		 goto end;
	 }else{
	   printf("ntp step 5 ok\r\n");
	 }
	 
   if(sim800c_send_cmd("AT+CNTP","+CNTP: 1",800)){
	   printf("ntp fail step 6 \r\n");
		 ret = 6;
		 goto end;
	 }else{
	   printf("ntp step 6 ok\r\n");
	 }
	 
	 if(sim800c_send_cmd("AT+SAPBR=0,1",0,800)){
	   printf("ntp fail step 7 \r\n");
		 return 7;
	 }else{
		 printf("ntp ok, close gprs context \r\n");
	   return 0;
	 }
	 	 
end:	 
	 if(sim800c_send_cmd("AT+SAPBR=0,1",0,200)){
	   printf("ntp close gprs fail\r\n");
	 }else{
	   printf("ntp fail, close gprs context \r\n");
	 }
	 
	 return ret;
}

u8 fetchNetworkTime(u8* pTime){
	u8 *p1, *p2, *p;
	u8 ret = 0, value, i;
	
	p = pTime;
	printf("fetch network time ...\r\n");
  if(sim800c_send_cmd("AT+CCLK?","+CCLK:",200)==0)
	{ 
		p1=(u8*)strstr((const char*)(USART2_RX_BUF),"\"");
		p2=(u8*)strstr((const char*)(p1+1),"+");
		p2[3]=0;//���������
		//sprintf((char*)p,"����ʱ��:%s",p1+1);
		printf("ntp time %s \r\n", (const char*)(p1+1));
		
		//year
		p1++;
		value = sim800c_chr2hex(*p1)*10;
			//printf("%d-%d ", i, value);
		p1++;
		value += sim800c_chr2hex(*p1);
			//printf("%d-%d ;", i, value);
	  *p = value;
		//month
		p1=p1+2;
		p++;
		value = sim800c_chr2hex(*p1)*10;
			//printf("%d-%d ", i, value);
		p1++;
		value += sim800c_chr2hex(*p1);
			//printf("%d-%d ;", i, value);
	  *p = value;
		//day
		p1=p1+2;
		p++;
		value = sim800c_chr2hex(*p1)*10;
			//printf("%d-%d ", i, value);
		p1++;
		value += sim800c_chr2hex(*p1);
			//printf("%d-%d ;", i, value);
	  *p = value;
		//hour
		p1=p1+2;
		p++;
		value = sim800c_chr2hex(*p1)*10;
			//printf("%d-%d ", i, value);
		p1++;
		value += sim800c_chr2hex(*p1);
			//printf("%d-%d ;", i, value);
	  *p = value;
		//min
		p1=p1+2;
		p++;
		value = sim800c_chr2hex(*p1)*10;
			//printf("%d-%d ", i, value);
		p1++;
		value += sim800c_chr2hex(*p1);
			//printf("%d-%d ;", i, value);
	  *p = value;
		//sec
		p1=p1+2;
		p++;
		value = sim800c_chr2hex(*p1)*10;
			//printf("%d-%d ", i, value);
		p1++;
		value += sim800c_chr2hex(*p1);
			//printf("%d-%d ;", i, value);
	  *p = value;
		
		/*
		while(i < 6){
			
		  value = sim800c_chr2hex(*p1)*10;
			//printf("%d-%d ", i, value); if no printf, result is 0 and null;with printf, it is ok, fuck
		  p1++;
		  value += sim800c_chr2hex(*p1);
			//printf("%d-%d ;", i, value);
			*p = value;
			
			p++;
			p1=p1+2;
			i++;
		}
		*/
		
		printf("\r\n");
		USART2_RX_STA=0;		
	} else {
		printf("fail to get CCLK\r\n");
	  ret = 1;
	}
	
  return ret;
}


//sim800C�����Գ���
u8 checkSIM800HW(void)
{
	u8 ret = 1;
	if(sim800c_send_cmd("AT","OK",100))//����Ƿ�Ӧ��ATָ�� 
	{
		printf("at no response \r\n");
	} else {
	  printf("at response ok\r\n");
		if(sim800c_send_cmd("AT+CPIN?","OK",200)){
		  printf("SIM is not present \r\n");
		} else {  
	    printf("SIM is present \r\n");
			ret =0;
			sim800c_send_cmd("ATE0","OK",200);//������
		}
	}
  return ret;	
}

u8 checkGSMSignalQuality(u8* p){
	u8 *p1,*p2;
	u8 value =0;
	
	//AT+CSQ
	//+CSQ: 24,0
  if(sim800c_send_cmd("AT+CSQ","+CSQ:",200)==0)
	{ 
		p1=(u8*)strstr((const char*)(USART2_RX_BUF),":");
		p2=(u8*)strstr((const char*)(p1),",");
		p2[0]=0;//���������
		sprintf((char*)p,"�ź�����:%s",p1+2);
		printf("%s\r\n", p);
		if(*(p1+3) == '\0'){
      value = sim800c_chr2hex(*(p1+2));
		}else if(*(p1+4) == '\0') {
		  value = sim800c_chr2hex(*(p1+2))*10;
			value += sim800c_chr2hex(*(p1+3));
		}else {
		  printf("SQ format is wrong, 2 bit at most \r\n");
		}
    USART2_RX_STA=0;
	}
	
  return value;
}

u8 queryCellId(u8* id){
	u8 ret = 0;
	u8 *p1,*p2;
	//AT+CSQ
	//+CSQ: 24,0
	id = id;
	if(sim800c_send_cmd("AT+CENG=1,1","OK",200)){
		ret = 1;
		printf("ceng mode open fail \r\n");
	} else {
		printf("ceng mode open ok \r\n");
    if(sim800c_send_cmd("AT+CENG?","+CENG:",200)==0){ 
	    //printf("ceng return %s \r\n", (const char*)(USART2_RX_BUF));
		
			p1=(u8*)strstr((const char*)(USART2_RX_BUF),"\"");
			p2=(u8*)strstr((const char*)(p1+1),"\"");
			p2[0]=0;//���������
			printf("cell 0 info %s \r\n", p1+1);
			if(sim800c_send_cmd("AT+CENG=0,1","OK",200) == 0){
			  printf("ceng mode close ok\r\n");
			}else{
			  printf("ceng mode close fail\r\n");
			}
			
      USART2_RX_STA=0;
	  }
  }
  return ret;
}













