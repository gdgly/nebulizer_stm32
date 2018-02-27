#ifndef  NEBULIZER_HEADER
#define  NEBULIZER_HEADER

#define KEY0_FLAG		      0x01
#define KEY1_FLAG		      0x02
#define TIMER_FLAG        0x04
#define RTC_UPDATE_FLAG   0x10
#define KEYWKUP_LOW_FLAG  0x20
#define KEYWKUP_HIGH_FLAG  0x40
#define KEYFLAGS_VALUE	  0x00	

#define NTP_TIMER_FLAG    0x02
#define GPRS_SEND_FLAG    0x20

#define CCID_OFFSET         5
#define STARTTIME_OFFSET    25
#define ENDTIME_OFFSET      45
#define CURRENT_CELL_OFFSET 65
#define NEXT_CELL_OFFSET    82
#define VERIFY_OFFSET       99


void postKeyEvent(u8 keynum);
void postTimerEvent(u8 timer_num);
void ntpProcess(u8 *buf);
void postRTCUpdateEvent(void);
void postGprsSendEvent(void);
u8 sim800c_gprs_transparentMode(u8* content, u16 len);
#endif
