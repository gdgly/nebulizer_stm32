#ifndef __SIM800C_H__
#define __SIM800C_H__	 
#include "sys.h"
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
//////////////////////////////////////////////////////////////////////////////////	

#define swap16(x) (x&0XFF)<<8|(x&0XFF00)>>8	//�ߵ��ֽڽ����궨��
 
#define SERVER_RES_LEN 27

typedef enum
{ 
  AT_OK = 0,
  AT_FAIL = 0x0001,
	AT_GPRS_SEND_FAIL,
	AT_GPRS_CLOSE_FAIL,
	
	AT_SERVER_RES_TIMEOUT = 0x8001,
	
	AT_SERVER_PREAMBLE_ERR,
	AT_SERVER_IND_NG,
	AT_SERVER_SERIAL_NUM_ERR,
	AT_SERVER_XOR_ERR
	
}SIM800_ERROR;

extern u8 Scan_Wtime;

void sim_send_sms(u8*phonenumber,u8*msg);
void sim_at_response(u8 mode);	
u8* sim800c_check_cmd(u8 *str);
u8 sim800c_send_cmd(u8 *cmd,u8 *ack,u16 waittime);
u8 sim800c_wait_request(u8 *request ,u16 waittime);
u8 sim800c_chr2hex(u8 chr);
u8 sim800c_hex2chr(u8 hex);
void sim800c_unigbk_exchange(u8 *src,u8 *dst,u8 mode);
void sim800c_load_keyboard(u16 x,u16 y,u8 **kbtbl);
void sim800c_key_staset(u16 x,u16 y,u8 keyx,u8 sta);
u8 sim800c_get_keynum(u16 x,u16 y);

u8 sim800c_gsminfo_show(u16 x,u16 y);//��ʾGSMģ����Ϣ
u8 ntp_update(void);               //����ͬ��ʱ��
u8 fetchNetworkTime(u8* pTime);
u8 checkSIM800HW(void);
u8 checkGPRSEnv(void);
u8 queryCSQ(void);
u8 queryCellId(u8* id, u8* neighborId);
u8 getCCID(u8* pCcid);
SIM800_ERROR sim800c_gprs_tcp(u8* content, u16 len);
u8 xorVerify(u8* buf, u8 len);
#endif





