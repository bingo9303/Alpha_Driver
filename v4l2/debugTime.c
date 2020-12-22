#include "debugTime.h"
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>


int GetTime_H_M_S(char * psTime) 
{
    time_t nSeconds;
    struct tm * pTM;
    
    time(&nSeconds);
    pTM = localtime(&nSeconds);
    
    /* 系统时间，格式: HHMMSS */
    sprintf(psTime, "%02d:%02d:%02d",pTM->tm_hour, pTM->tm_min, pTM->tm_sec);
           
    return 0;       
}


unsigned int GetTime_Sec(void)
{
    struct timeval tv;
    gettimeofday(&tv,NULL);
    return tv.tv_sec;
}

unsigned int GetTime_Ms(void)
{
    struct timeval tv;
    gettimeofday(&tv,NULL);
    return (tv.tv_sec*1000 + tv.tv_usec/1000);
}

unsigned int GetTime_Usc(void)
{
    struct timeval tv;
    gettimeofday(&tv,NULL);
   // return (tv.tv_sec*1000000 + tv.tv_usec);
   return (tv.tv_usec);
}







