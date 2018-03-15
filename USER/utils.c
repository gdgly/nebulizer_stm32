#include "utils.h"
#include "usart.h"


u8 flashInfoXorVerify(u8* buf){
  u8 ret = 0,i;
	for(i = 0; i < 8; i++){
		if(i != 3){
	    ret = ret^(*(buf+i));
		}
	}
	
	ret = ret ^ buf[3];

	printf("flash xor check %x \r\n", ret);
	
	return ret;
}

void flashInfoXorFill(u8* buf){
  u8 ret = 0,i;
	for(i = 0; i < 8; i++){
		if(i != 3){
	    ret = ret^(*(buf+i));
		}
	}
	
	buf[3] = ret;

	printf("flash xor fill %x \r\n", ret);
}

u8 IsLeap(u16 year)  
{  
    return (year % 4 ==0 || year % 400 ==0) && (year % 100 !=0);  
}  

u16 DayInYear(_calendar_obj* pDate)  
{  
    int iRet = pDate->w_date, i;  
    int DAY[12]={31,28,31,30,31,30,31,31,30,31,30,31};  
    if(IsLeap(pDate->w_year))  
        DAY[1] = 29;  
    for(i=0; i < (pDate->w_month - 1); ++i)  
    {  
        iRet += DAY[i];  
    }  
    return iRet;  
}  
  
//return 1 means Date2 is late than Date1
//return 0 means Date1 equals Date2
//return -1 means Date1 is late than Date2

int compareTime(_calendar_obj* pDate1, _calendar_obj* pDate2)
{
	if(pDate1->w_year > pDate2->w_year){
	  return -1;
	}
	
	if(pDate1->w_year < pDate2->w_year){
		return 1;
	}
	
	if(pDate1->w_month > pDate2->w_month){
	  return -1;
	}
	
	if(pDate1->w_month < pDate2->w_month){
		return 1;
	}
	
  if(pDate1->w_date > pDate2->w_date){
	  return -1;
	}
	
	if(pDate1->w_date < pDate2->w_date){
		return 1;
	}
	
  if(pDate1->hour > pDate2->hour){
	  return -1;
	}
	
	if(pDate1->hour < pDate2->hour){
		return 1;
	}
	
	if(pDate1->min > pDate2->min){
	  return -1;
	}
	
	if(pDate1->min < pDate2->min){
		return 1;
	}
	
	if(pDate1->sec > pDate2->sec){
	  return -1;
	}
	
	if(pDate1->sec < pDate2->sec){
		return 1;
	}
	
	return 0;
	
	
}



int DaysBetween2Date(_calendar_obj* pDate1, _calendar_obj* pDate2)  
{  
    
    _calendar_obj *pTmp;  
    int year;
    if(pDate1->w_year == pDate2->w_year && pDate1->w_month == pDate2->w_month)  
    {  
        return abs(pDate1->w_date - pDate2->w_date);  
    }  
    else if(pDate1->w_year == pDate2->w_year) 
    {  
        return abs(DayInYear(pDate1) - DayInYear(pDate2));  
    }  
    else
    {  
        int d1,d2,d3;  
  
        if(pDate1->w_year > pDate2->w_year){  
            pTmp = pDate1;  
            pDate1 = pDate2;  
            pDate2 = pTmp;  
        }  
  
        if(IsLeap(pDate1->w_year))  
            d1 = 366 - DayInYear(pDate1);
        else  
            d1 = 365 - DayInYear(pDate1);  
        d2 = DayInYear(pDate2);
          
        d3 = 0;  
        for( year = pDate1->w_year + 1; year < pDate2->w_year; year++)  
        {  
            if(IsLeap(year))  
                d3 += 366;  
            else  
                d3 += 365;  
        }  
        return d1 + d2 + d3;  
	}
}


u8  shiftTimeInGprsBuf(u8* buf, int timeDiff)
{
	_calendar_obj inputTime1;
	_calendar_obj inputTime2;
	u8* ptr = buf;
	u8 endByte = buf[65];
	
	printf("shiftTimeInGprsBuf +++ %d\r\n", timeDiff);
	printf("buf+25 before %s \r\n", buf+25);
	inputTime1.w_year = rock_chr2hex(ptr[25])*10 + rock_chr2hex(ptr[26]) + 2000;
	inputTime1.w_month = rock_chr2hex(ptr[28])*10 + rock_chr2hex(ptr[29]);
	inputTime1.w_date = rock_chr2hex(ptr[31])*10 + rock_chr2hex(ptr[32]);
	
	inputTime1.hour = rock_chr2hex(ptr[34])*10 + rock_chr2hex(ptr[35]);
	inputTime1.min = rock_chr2hex(ptr[37])*10 + rock_chr2hex(ptr[38]);
	inputTime1.sec = rock_chr2hex(ptr[40])*10 + rock_chr2hex(ptr[41]);
	
	RTC_Shift(timeDiff, &inputTime1);
			
	inputTime2.w_year = rock_chr2hex(ptr[45])*10 + rock_chr2hex(ptr[46]) + 2000;
	inputTime2.w_month = rock_chr2hex(ptr[48])*10 + rock_chr2hex(ptr[49]);
	inputTime2.w_date = rock_chr2hex(ptr[51])*10 + rock_chr2hex(ptr[52]);
	
	inputTime2.hour = rock_chr2hex(ptr[54])*10 + rock_chr2hex(ptr[55]);
	inputTime2.min = rock_chr2hex(ptr[57])*10 + rock_chr2hex(ptr[58]);
	inputTime2.sec = rock_chr2hex(ptr[60])*10 + rock_chr2hex(ptr[61]);
	
	RTC_Shift(timeDiff, &inputTime2);
	

	sprintf((char*)(buf+25),"%02d/%02d/%02d,%02d:%02d:%02d+00",inputTime1.w_year>2000?(inputTime1.w_year-2000):inputTime1.w_year,
			      inputTime1.w_month,inputTime1.w_date,inputTime1.hour,inputTime1.min,inputTime1.sec);
		
	
	sprintf((char*)(buf+45),"%02d/%02d/%02d,%02d:%02d:%02d+00",inputTime2.w_year>2000?(inputTime2.w_year-2000):inputTime2.w_year,
			      inputTime2.w_month,inputTime2.w_date,inputTime2.hour,inputTime2.min,inputTime2.sec);
	
	buf[65] = endByte;
	
	printf("buf+25 after %s \r\n", buf+25);
}

//将1个字符转换为16进制数字
//chr:字符,0~9/A~F/a~F
//返回值:chr对应的16进制数值
u8 rock_chr2hex(u8 chr)
{
	if(chr>='0'&&chr<='9')return chr-'0';
	if(chr>='A'&&chr<='F')return (chr-'A'+10);
	if(chr>='a'&&chr<='f')return (chr-'a'+10); 
	return 0;
}
//将1个16进制数字转换为字符
//hex:16进制数字,0~15;
//返回值:字符
u8 rock_hex2chr(u8 hex)
{
	if(hex<=9)return hex+'0';
	if(hex>=10&&hex<=15)return (hex-10+'A'); 
	return '0';
}

int diffTimeInSecs(_calendar_obj time1, _calendar_obj time2)
{
	int diffDays = 0;
	int ret = 0;
	int lateFlag = 0;
	
	printf("time2 %d/%02d/%02d %02d:%02d:%02d\r\n", time2.w_year, time2.w_month,time2.w_date,time2.hour,time2.min,time2.sec);
	
	printf("time1 %d/%02d/%02d %02d:%02d:%02d\r\n", time1.w_year, time1.w_month,time1.w_date,time1.hour,time1.min,time1.sec);
	
  diffDays = DaysBetween2Date(&time1, &time2);
	
	printf("diffDays %d \r\n", diffDays);
	
	lateFlag = compareTime(&time1, &time2);
	
	printf("lateFlag %d \r\n", lateFlag);
	
	if(diffDays == 0){
	  ret = (time2.hour*3600 + time2.min*60 + time2.sec) - (time1.hour*3600 + time1.min*60 + time1.sec);
	  printf("date is same, sec diff is %d\r\n", ret);	
    if(ret < 10 && ret > -10){
		  ret = 0;
    }
  }else{
		
		if(lateFlag == 1){
	     ret = (24 - time1.hour -1)*3600 + (60 - time1.min -1)*60 + 60 - time1.sec + time2.hour*3600 + time2.min*60 + time2.sec + (diffDays -1)*86400;
		}else if(lateFlag == -1){
			 ret = (24 - time2.hour -1)*3600 + (60 - time2.min -1)*60 + 60 - time2.sec + time1.hour*3600 + time1.min*60 + time1.sec + (diffDays -1)*86400;
			 printf("ret before -1 %d \r\n", ret);
			 ret = -ret;
		}
		
		printf("date is not same, sec diff is %d\r\n", ret);
		if(ret < 10 && ret > -10){
		  ret = 0;
    }
	}
	
	return ret;
}


u8 shiftTime(int diff, _calendar_obj * inputTime)
{
	
}

