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



/////////////////////////////////////////////////////////////////////////////////////////////////////
//GPRS���Բ��ִ���


//sim800C GPRS����
//���ڲ���TCP/UDP����
//����ֵ:0,����
//����,�������
SIM800_ERROR sim800c_gprs_tcp(u8* content, u16 len)
{
	u8 *p1, *p2;
	//u8 test[4] = {0x55, 0xaa, 0x5a, 0xa5};
	u8 cmd[20] = {0};
	u8 recv[SERVER_RES_LEN+1] = {0};
	u8 i=0,length=0,j=0,recvFlag=0;
	u16 count =0;
	SIM800_ERROR ret = AT_OK;
	printf("sim8ooc_gprs_tcp \r\n");
	
	if(sim800c_send_cmd("AT+CGCLASS=\"B\"","OK",1000)){
		printf("cgclass fail \r\n");
		return AT_FAIL;
  }
	
	if(sim800c_send_cmd("AT+CGDCONT=1,\"IP\",\"CMNET\"","OK",1000)){
		printf("cgdcont fail \r\n");
		return AT_FAIL;
	}
	
	if(sim800c_send_cmd("AT+CGATT?","+CGATT:",500) == 0){
	   p1=(u8*)strstr((const char*)(USART2_RX_BUF),":");
		 p2 = (u8 *)strstr((const char*)(USART2_RX_BUF),"1");
		 if(p2 == 0){
		    printf("gprs is not attach \r\n");
			  if(sim800c_send_cmd("AT+CGATT=1","OK",500)){
					return AT_FAIL;					//����GPRSҵ��
				}else{
				  printf("gprs reconnect ok \r\n");
				}
		 }else{
		   printf("gprs already connect \r\n");
		 }
	}
	
	if(sim800c_send_cmd("AT+CLPORT=\"TCP\",\"2000\"","OK",1000)){
	  printf("clport fail \r\n");
		
    ret = AT_FAIL;
		goto end;
	}
	
		
	if(sim800c_send_cmd("AT+CIPSTART=\"TCP\",\"19m9b15866.iok.la\",\"39084\"","CONNECT OK",3000)){
	//if(sim800c_send_cmd("AT+CIPSTART=\"TCP\",\"huxiweishi.3322.org\",\"5414\"","CONNECT OK",3000)){
	  printf("connect fail \r\n");
    return AT_FAIL;
	}else {
	  printf("connect ok\r\n");
	}
	
	if(sim800c_send_cmd("AT+CIPSTATUS","CONNECT OK",500) == 0){
		printf("CIPSTATUS is connected , right \r\n");
	}else{
		printf("CIPSTATUS is wrong \r\n");
	}
	
	sprintf((char*)cmd,"AT+CIPSEND=%d",len); //fixed length, no need to send 0x1A
	
	if(sim800c_send_cmd(cmd,">",500)==0)		//��������
  { 
 				//printf("CIPSEND DATA:%s\r\n",p1);	 			//�������ݴ�ӡ������
    //u2_printf("%s\r\n","fuck you again");
		
		u2_hexsend(content, len);
		
		delay_ms(50);
		
		/*
		if(sim800c_send_cmd((u8*)0X1A,"SEND OK",1000)==0){
		  printf("send ok\r\n");
		}else{
		  printf("send fail\r\n");
		}
		*/
		
		USART2_RX_STA = 0;
		
    while((i < SERVER_RES_LEN) && (count < 500)){
	    if(USART2_RX_STA & 0x8000){
		    p1 = USART2_RX_BUF;
			  length = USART2_RX_STA&0x7FFF;
				printf("rece %d \r\n", length);
				printf("%s", p1);
			  if(recvFlag == 0){
			    for(j=0; j<length; j++){
			      if(*p1 == 0x55){
						  printf("find 0x55 \r\n");
					    break;
				    }
				    p1++;
			    }
			    if(j!=length){
			      if((length-j) >= SERVER_RES_LEN){
				      memcpy(recv,p1,SERVER_RES_LEN);
						  printf("finish in 1st receive\r\n");
						  recvFlag = 2;
						  USART2_RX_STA = 0;
					    break;
				    }else{
				      memcpy(recv, p1, length-j);
						  recvFlag=1;
						  i = length-j;
						  printf("1 shot len %d, j %d, i %d \r\n", length, j, i);
						  USART2_RX_STA = 0;
				    }
			    }else{
				    printf("0x55 not in this recv \r\n");
					  USART2_RX_STA = 0;
				  }
		    }else{
				  printf("len %d, i %d \r\n", length, i);
				  if(length >= (SERVER_RES_LEN-i)){
			      memcpy(recv+i, p1, SERVER_RES_LEN-i);
					  printf("rece mult finish\r\n");
					  recvFlag = 2;
					  USART2_RX_STA = 0;
					  break;
				  }else{
					  memcpy(recv+i, p1, length);
					  i = length+i;
					  USART2_RX_STA = 0;
				  }
			  }
			
	    }
			
		  delay_ms(10);
		  count++;//we can not wait too long, 3s at most
    }

	  if(recvFlag != 2){
	    ret = AT_SERVER_RES_TIMEOUT;
		  printf("recv not complete and timeout \r\n");
	  }else{
	    if(recv[1] != 0xAA){
	      ret = AT_SERVER_PREAMBLE_ERR;
			  printf("preamble is wrong \r\n");
	    }else{
	      if((recv[3] != content[3]) || (recv[4] != content[4])){
		      ret = AT_SERVER_SERIAL_NUM_ERR;
				  printf("flow index not match %d %d\r\n", recv[3], recv[4]);
		    }else{
		      if(recv[5] != 0x77){
					  printf("server find wrong data %d\r\n", recv[5]);
			      ret = AT_SERVER_IND_NG;
			    }else{
			      u8 value = xorVerify(recv+2, SERVER_RES_LEN-3);
				    if(value != recv[SERVER_RES_LEN-1]){
				      ret = AT_SERVER_XOR_ERR;
						  printf("xor is wrong \r\n");
				    }else{
							u8 time[21];
							memcpy(time, &(recv[6]), 20);
							time[20] = '\0';
				      printf("recv verify pass %s\r\n", time);
				    }
			    }
		    }
	    }
    }
  }else{
    sim800c_send_cmd((u8*)0X1B,0,0);	//ESC,ȡ������ 
		printf("cancel send, send fail \r\n");
		ret = AT_GPRS_SEND_FAIL;
	}

end:
	
  printf("cipclose ...\r\n");	
  if(sim800c_send_cmd("AT+CIPCLOSE=1","CLOSE OK",2000)){
	  printf("cip close fail \r\n");
	}else {
	  printf("cip close ok\r\n");
	}
	
	printf("cipshutdown ...\r\n");
  if(sim800c_send_cmd("AT+CIPSHUT","SHUT OK",1000)){
	  printf("cip shut down fail \r\n");
	}else{
	  printf("cip shut down ok \r\n");
	}
	
	if(sim800c_send_cmd("AT+CIPSTATUS","IP INITIAL",500) == 0){
		delay_ms(50);
	  printf("CIPSTATUS IP INITIAL is ok \r\n");
	}
	
  return ret;
}
 
u16 sim800c_gprs_transparentMode(u8* content, u16 len)
{
	u8 *p1, *p2;
	u8 recv[SERVER_RES_LEN+1] = {0};
	u8 i=0,length=0,j=0,recvFlag=0, count=0;
	u16 ret = 0;
	printf("sim800c_gprs_transparentMode \r\n");
	
	if(sim800c_send_cmd("AT+CGATT?","+CGATT:",500) == 0){
	   p1=(u8*)strstr((const char*)(USART2_RX_BUF),":");
		 p2 = (u8 *)strstr((const char*)(USART2_RX_BUF),"1");
		 if(p2 == 0){
		    printf("gprs is not attach \r\n");
			  if(sim800c_send_cmd("AT+CGATT=1","OK",500)){
					return 3;					//����GPRSҵ��
				}else{
				  printf("gprs reconnect ok \r\n");
				}
		 }else{
		   printf("gprs already connect \r\n");
		 }
	}
	
	if(sim800c_send_cmd("AT+CIPMODE=1","OK",200)){
	  printf("cip mode fail \r\n");
    return 4;		 
	}
	
	if(sim800c_send_cmd("AT+CSTT=\"CMNET\"","OK",1000)){
	  printf("cstt cmnet fail \r\n");
    return 4;		 
	}
	
	if(sim800c_send_cmd("AT+CIICR","OK",1000)){
	  printf("ciicr fail \r\n");
    return 4;		 
	}
	
	if(sim800c_send_cmd("AT+CIFSR",0,100)){
	  printf("ciicr fail \r\n");
    return 4;		 
	}else{
		delay_ms(100);
	  if(USART2_RX_STA & 0x8000){
		  printf("ip %s \r\n", (const char*)(USART2_RX_BUF));
		}
	}
	
	if(sim800c_send_cmd("AT+CIPSTART=\"TCP\",\"19m9b15866.iok.la\",\"39084\"","CONNECT",3000)){
	  printf("connect fail \r\n");
    return 4;		 
	}else {
	  printf("connect ok\r\n");
	}
	
  USART2_RX_STA = 0;
	u2_hexsend(content, len);
	
	while((i < SERVER_RES_LEN) && (count < 1000)){
	  if(USART2_RX_STA & 0x8000){
		  p1 = USART2_RX_BUF;
			length = USART2_RX_STA&0x7FFF;
			if(recvFlag == 0){
			  for(j=0; j<length; j++){
			    if(*p1 == 0x55){
						printf("find 0x55 \r\n");
					  break;
				  }
				  p1++;
			  }
			  if(j!=length){
			    if((length-j) >= SERVER_RES_LEN){
				    memcpy(recv,p1,SERVER_RES_LEN);
						printf("finish in 1st receive\r\n");
						recvFlag = 2;
						USART2_RX_STA = 0;
					  break;
				  }else{
				    memcpy(recv, p1, length-j);
						recvFlag=1;
						i = length-j;
						printf("1 shot len %d, j %d, i %d \r\n", length, j, i);
						USART2_RX_STA = 0;
				  }
			  }else{
				  printf("0x55 not in this recv \r\n");
					USART2_RX_STA = 0;
				}
		  }else{
				printf("len %d, i %d \r\n", length, i);
				if(length >= (SERVER_RES_LEN-i)){
			    memcpy(recv+i, p1, SERVER_RES_LEN-i);
					printf("rece mult finish\r\n");
					recvFlag = 2;
					USART2_RX_STA = 0;
					break;
				}else{
					memcpy(recv+i, p1, length);
					i = length+i;
					USART2_RX_STA = 0;
				}
			}
			
	  }
		delay_ms(10);
		count++;//we can not wait too long, 3s at most
  }
	
	if(recvFlag != 2){
	  ret = 0x50;
		printf("recv not complete and timeout \r\n");
	}else{
	  if(recv[1] != 0xAA){
	    ret = 0x10;
			printf("preamble is wrong \r\n");
	  }else{
	    if((recv[3] != content[3]) || (recv[4] != content[4])){
		    ret = 0x20;
				printf("flow index not match %d %d\r\n", recv[3], recv[4]);
		  }else{
		    if(recv[5] != 0x77){
					printf("server find wrong data %d\r\n", recv[5]);
			    ret = 0x30;
			  }else{
			    u8 value = xorVerify(recv+2, SERVER_RES_LEN-3);
				  if(value != recv[SERVER_RES_LEN-1]){
				    ret = 0x40;
						printf("xor is wrong \r\n");
				  }else{
				    printf("recv verify pass \r\n");
				  }
			  }
		  }
	  }
  }
		
	delay_ms(1000);
	u2_printf("+++");
	delay_ms(1000);
			
  if(sim800c_send_cmd("AT+CIPCLOSE=1","CLOSE OK",500)){
	  printf("cip close fail \r\n");
		return ret|5;
	}else {
	  printf("cip close ok\r\n");
	}
	
  if(sim800c_send_cmd("AT+CIPSHUT","SHUT OK",500)){
	  printf("cip shut down fail \r\n");
		return ret|6;
	}else{
	  printf("cip shut down ok \r\n");
	}
	
  return ret;
}

u8 xorVerify(u8* buf, u8 len){
  int ret = 0,i;
	for(i = 0; i < len ; i++){
	  ret = ret^(*(buf+i));
	}
	printf("xor result %x \r\n", ret);
	return ret;
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
	   //printf("ntp step 1 ok\r\n");
	 }
	 
	 if(sim800c_send_cmd("AT+SAPBR=3,1,\"APN\",\"CMNET\"","OK",200)){
	   printf("ntp fail step 2 \r\n");
		 return 2;
	 }else{
	   //printf("ntp step 2 ok\r\n");
	 }
	 
	 if(sim800c_send_cmd("AT+SAPBR=1,1",0,200)){
	   printf("ntp fail step 3 \r\n");
		 return 3;
	 }else{
	   //printf("ntp step 3 ok\r\n");
	 }
   
	 delay_ms(5);
	 
   if(sim800c_send_cmd("AT+CNTPCID=1","OK",400)){
	   printf("ntp fail step 4 \r\n");
		 ret = 4;
		 goto end;
	 }else{
	   //printf("ntp step 4 ok\r\n");
	 }
	 //sim800c_send_cmd("AT+CNTP=\"202.120.2.101\",32","OK",200);     //����NTP�������ͱ���ʱ��(32ʱ�� ʱ����׼ȷ)
	 
	 if(sim800c_send_cmd("AT+CNTP=\"202.108.6.95\",8","OK",800)){
	   printf("ntp fail step 5 \r\n");
		 //return 5;
		 ret = 5;
		 goto end;
	 }else{
	   printf("ntp step 5 ok��china 202.108.6.95\r\n");
	 }
	 
   if(sim800c_send_cmd("AT+CNTP","+CNTP: 1",800)){
	   printf("ntp fail step 6, try another ntp server \r\n");
		 
		 if(sim800c_send_cmd("AT+CNTP=\"120.25.108.11\",8","OK",800)){
			 ret =6;
			 goto end;
		 }else{
		   if(sim800c_send_cmd("AT+CNTP","+CNTP: 1",800)){
			   ret = 6;
				 if(sim800c_send_cmd("AT+CNTP=\"202.112.29.82\",8","OK",800)){
			     ret =6;
			     goto end;
		     }else{
		       if(sim800c_send_cmd("AT+CNTP","+CNTP: 1",800)){
			       ret = 6;
				     goto end;
			     }else{
					    printf("ntp 3rd try china edu 202.112.29.82 ok \r\n");
					 }
				 }
			 }else{
			   printf("ntp 2nd try 120.25.108.11\r\n");
			 }
		 }
	 }else{
	   //printf("ntp step 6 ok\r\n");
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
	u8 ret = 0, value;
	
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
		
		//printf("\r\n");
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
	printf("checkSIM800HW ..\r\n");
	if(sim800c_send_cmd("AT","OK",100)){
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

u8 checkGPRSEnv(void){
	u8 *p1, *p2;
	u8 ret=1, reg, value;
	
	value = queryCSQ();
	
	if(value < 8){
		printf("signal %d < 8, too weak\r\n", value);
	  return 1;  
	}
	
  if(sim800c_send_cmd("AT+CREG?","+CREG:",100)){
		printf("CREG cmd fail \r\n");
    return 2;
	}
	
  p1=(u8*)strstr((const char*)(USART2_RX_BUF),",");
	reg = sim800c_chr2hex(*(p1+1));
	if((reg != 1)&&(reg != 5)){
		printf("GSM not register %d \r\n", reg);
    return 3;			
  }
 
  if(sim800c_send_cmd("AT+CGATT?","+CGATT:",500)){
		printf("CGATT cmd fail\r\n");
    return 4;
  }

	p1=(u8*)strstr((const char*)(USART2_RX_BUF),":");
	p2 = (u8 *)strstr((const char*)(USART2_RX_BUF),"1");
	if(p2 == 0){
	  printf("gprs is not attach \r\n");
	  if(sim800c_send_cmd("AT+CGATT=1","OK",500)){
		  return 5;					//����GPRSҵ��
	  }else{
			printf("please check GATT again \r\n");
			return 6;
		}
  }else{
		printf("gprs already connect \r\n");
	}
	
	return 0;
}

u8 getCCID(u8* pCcid){
  u8 ret=1, i=0;
	u8 *p1=0;
	
	if(sim800c_send_cmd("AT+CCID=?", "OK", 200) == 0){
		//printf("CCID test cmd ok \r\n");
		if(sim800c_send_cmd("AT+CCID", "OK", 200) == 0) {
			//delay_ms(100);
			p1 = USART2_RX_BUF;
			while(i < 10){
			  if(*p1 < 0x30){
				  p1++;
				}else{
				  break;
				}
				i++;
			}
			if(i<10){
			  p1[20]=0;
			  printf("ccid is %s \r\n", (const char*)p1);
			  /*
			    while(i < 20){
			     pCcid[i] = sim800c_chr2hex(*(p1+i));
				  i++;
			  }
			  */
			  memcpy(pCcid, p1, 20);
			
			  ret = 0;
			}else{
			  printf("no num in first 10 bytes \r\n");
			}				
		}
	}else{
	  printf("CCID test cmd fail /r/n");
	}
	
	return ret;
}

u8 queryCSQ(){
	u8 *p1,*p2;
	u8 value =0;
	
	//AT+CSQ
	//+CSQ: 24,0
	printf("queryCSQ ...\r\n");
  if(sim800c_send_cmd("AT+CSQ","+CSQ:",200)==0)
	{ 
		p1=(u8*)strstr((const char*)(USART2_RX_BUF),":");
		p2=(u8*)strstr((const char*)(p1),",");
		p2[0]=0;//���������
		printf("CSQ %s\r\n", p1);
		if(*(p1+3) == '\0'){
      value = sim800c_chr2hex(*(p1+2));
		}else if(*(p1+4) == '\0') {
		  value = sim800c_chr2hex(*(p1+2))*10;
			value += sim800c_chr2hex(*(p1+3));
		}else {
		  printf("SQ format is wrong, 2 bit at most \r\n");
		}
    USART2_RX_STA=0;
	}else{
	  printf("queryCSQ fail \r\n");
	}
	
  return value;
}

u8 queryCellId(u8* id, u8* neighborId){
	u8 ret = 1;
	u8 *p1;
	//AT+CSQ
	//+CSQ: 24,0
	memset(id, 18, 0);
	memset(neighborId, 18, 0);
	if(sim800c_send_cmd("AT+CENG=1,1","OK",200)){
		printf("ceng mode open fail \r\n");
	} else {
		printf("ceng mode open ok \r\n");
    if(sim800c_send_cmd("AT+CENG?","+CENG:",200)==0){ 
	    //printf("ceng return %s \r\n", (const char*)(USART2_RX_BUF));
		  delay_ms(100);
			p1=(u8*)strstr((const char*)(USART2_RX_BUF),"0,\"");
			//p2=(u8*)strstr((const char*)(p1+3),"\"");
			//p2[0]=0;//���������
			if(p1 != 0){
			  u8 *ptrS, *ptrE;
				//printf("cell 0 info %s \r\n", p1+3);
				ptrS = (u8*)strstr((const char*)(p1+3),",");//bcch,rxl
				ptrS = (u8*)strstr((const char*)(ptrS+1),",");//rxl,rxq
				ptrS = (u8*)strstr((const char*)(ptrS+1),",");//rxq,mcc
				ptrE = ptrS;
				ptrE = (u8*)strstr((const char*)(ptrE+1),",");//mcc,mnc
				ptrE = (u8*)strstr((const char*)(ptrE+1),",");//mnc,bsic
				if((ptrE-ptrS-1) == 6){
				  memcpy(id, ptrS+1, 7); //460,00,
					//printf("id-1 %s \r\n", id);
				}else{
				  printf("mcc,mnc len is wrong %d\r\n", ptrE-ptrS);
				}
				ptrS = ptrE;
				ptrS = (u8*)strstr((const char*)(ptrS+1),",");//bsic,cellid
				ptrE = ptrS;
				ptrE = (u8*)strstr((const char*)(ptrE+1),",");//cellid,rla
				if((ptrE-ptrS-1) == 4){
				  memcpy(id+7, ptrS+1, 5); //62ea,
					//printf("id-2 %s \r\n", id);
				}else{
				  printf("cellid len is wrong %d\r\n", ptrE-ptrS);
				}
				ptrS = ptrE;
				ptrS = (u8*)strstr((const char*)(ptrS+1),",");//rla,txp
				ptrS = (u8*)strstr((const char*)(ptrS+1),",");//txp,lac
				ptrE = ptrS;
				ptrE = (u8*)strstr((const char*)(ptrE+1),",");//lac,TA
				if((ptrE-ptrS-1) == 4){
				  memcpy(id+12, ptrS+1, 5); //11d6,
					//printf("id-3 %s \r\n", id);
				}else{
				  printf("cellid len is wrong %d\r\n", ptrE-ptrS);
				}
				printf("cell 0 parse finish \r\n");
			}else{
			  printf("no cell 0 info");
			}
			
			p1=(u8*)strstr((const char*)(USART2_RX_BUF),"1,\"");
			if(p1 != 0){
			  u8 *ptrS, *ptrE;
				//printf("cell 0 info %s \r\n", p1+3);
				ptrS = (u8*)strstr((const char*)(p1+3),",");//bcch,rxl
				ptrS = (u8*)strstr((const char*)(ptrS+1),",");//rxl,bsic
				ptrS = (u8*)strstr((const char*)(ptrS+1),",");//bsic,cellid
				ptrE = (u8*)strstr((const char*)(ptrS+1),"\"");
				if((ptrE - ptrS -1) == 16){
				  memcpy(neighborId, ptrS+6,7);
					memcpy(neighborId+7, ptrS+1,5);
					memcpy(neighborId+12,ptrS+13,4);
					neighborId[16] = ',';
					//printf("neighborId = %s \r\n", neighborId);
				}else{
				  printf("cell 1 len is wrong %d\r\n", ptrE-ptrS);
					sprintf(neighborId, "%03d,%02d,%04d,%04d,",0,0,0,0);
				}
			}else{
			
			}
			
			ret = 0;
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













