#ifndef  NEBULIZER_HEADER
#define  NEBULIZER_HEADER

#define KEY0_FLAG		    0x01
#define KEY1_FLAG		    0x02
#define TIMER_FLAG      0x04
#define NTP_TIMER_FLAG  0x08
#define RTC_UPDATE_FLAG 0x10
#define KEYFLAGS_VALUE	0x00	


void postKey0Event(void);
void postKey1Event(void);
void postTimerEvent(u8 timer_num);
#endif
