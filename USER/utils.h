#include "sys.h"
#include "rtc.h"

u8 IsLeap(u16 year);
u16 DayInYear(_calendar_obj* pDate);
int DaysBetween2Date(_calendar_obj* pDate1, _calendar_obj* pDate2);
//int diffTime(u16 syear,u8 smon,u8 sday,u8 hour,u8 min,u8 sec);
int diffTimeInSecs(_calendar_obj time1, _calendar_obj time2);
u8 flashInfoXorVerify(u8* buf);
void flashInfoXorFill(u8* buf);
int compareTime(_calendar_obj* pDate1, _calendar_obj* pDate2);
u8  shiftTimeInGprsBuf(u8* buf, int timeDiff);
u8 rock_chr2hex(u8 chr);
u8 rock_hex2chr(u8 hex);
