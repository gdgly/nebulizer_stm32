#ifndef  NEBULIZER_HEADER
#define  NEBULIZER_HEADER

#define KEY0_FLAG		       0x01
#define KEY1_FLAG		       0x02
#define TIMER_FLAG         0x04
#define RTC_UPDATE_FLAG    0x10
#define KEYWKUP_LOW_FLAG   0x20
#define KEYWKUP_HIGH_FLAG  0x40
#define KEYFLAGS_VALUE	   0x00	

#define NTP_TIMER_FLAG           0x02

#define GPRS_OLD_SEND_FLAG       0x44

#define GPRS_NTPOK_SEND_FLAG     0x55 //must be 0x55
#define GPRS_NTPOK_TRY_SECOND    0x56
#define GPRS_NTPOK_TRY_THIRD     0x57


#define GPRS_NTPFAIL_SEND_FLAG   0x65
#define GPRS_NTPFAIL_TRY_SECOND  0x66
#define GPRS_NTPFAIL_TRY_THIRD   0x67

#define GRPS_SEND_SAVEDATA_FLAG  0x60

#define CCID_OFFSET         5
#define STARTTIME_OFFSET    25
#define ENDTIME_OFFSET      45
#define CURRENT_CELL_OFFSET 65
#define NEXT_CELL_OFFSET    82
#define VERIFY_OFFSET       99

#define TASK_Q_NUM	      8	
#define MODEMTASK_Q_NUM	  10	

void postKeyEvent(u8 keynum);
void postTimerMessage(u8 timer_num);
void ntpProcess(u8 *buf);
void postRTCUpdateMessage(void);
void postGprsSendMessage(u8**);
u8 sim800c_gprs_transparentMode(u8* content, u16 len);
u8 check_mainTaskMsg_queue(void);
u8 check_modemTaskMsg_queue(void);

u8 readFromFlash(u8* pBuf, u32 *infoAddr, u8 *infoContent );
u8 getNumInFlash(u8 *savedNum);
u8 updateFlashInfo(u32 infoAddr, u8 *infoContent);
u8 saveToFlash(u8* pBuf);
void postGprsSendSaveDataMessage(void);
void stopAutoTest(void);
u16 getCountFromFlash(void);
u8 increaseCntAtFlash(void);
#endif
