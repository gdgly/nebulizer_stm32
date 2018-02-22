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
u8 checkGSMSignalQuality(u8 *p);
u8 queryCellId(u8* id);
u8 getCCID(u8* pCcid);
u8 sim800c_gprs_tcp(u8* content);
#endif





