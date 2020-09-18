#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include "uart_common.h"


int open_uart_port(int port,int flag)
{
	int fd = -1;  
	char devName[20];
	int result=0;

	if((port<0) || (port>7))	return -1;

	
	sprintf(devName,"/dev/ttymxc%d",port);

	fd = open(devName,O_RDWR|O_NOCTTY|O_NDELAY);  //以非阻塞的方式打开
	if(fd < 0)  
	{  
	    printf("**APP** :Can't Open Serial Port %d !!!",port);
	    return (-1);  
	}  

	if(flag == OEPN_UART_BLOCK)
	{
		if(fcntl(fd,F_SETFL,0) < 0)/*设置串口的状态为阻塞状态，用于等待串口数据的读入*/  
		{  
			printf("**APP** :fcntl failed !\n"); 
			result = -1;
			goto open_faild;  
		} 
	}
	/*测试打开的文件描述符是否应用一个终端设备，以进一步确认串口是否正确打开*/  
	if(!isatty(STDIN_FILENO))
	{  
		printf("**APP** :Standard input isn't a terminal device !\n");  
		result = -1;
		goto open_faild;   
	}  
	printf("**APP** :open uart port succeed, file ID= %d !\n",fd);  
	  
	return fd;	

open_faild:
	close(fd);
	return result;
}


int set_uart_port(int fd,int iBaudRate,int iDataSize,char cParity,int iStopBit)  
{
	int iResult = 0;  
    struct termios oldtio,newtio; 

	iResult = tcgetattr(fd,&oldtio);/*保存原先串口配置*/  
	if(iResult)  
    {  
        printf("**APP** :Can't get old terminal description !");  
        return (-1);  
    } 

	
    bzero(&newtio,sizeof(newtio));  
    newtio.c_cflag |= CLOCAL | CREAD;/*设置本地连接和接收使用*/ 

	/*设置输入输出波特率*/  
    switch(iBaudRate)  
    {  
        case 2400:  
            cfsetispeed(&newtio,B2400);  
            cfsetospeed(&newtio,B2400);  
            break;  
        case 4800:  
            cfsetispeed(&newtio,B4800);  
            cfsetospeed(&newtio,B4800);  
            break;    
        case 9600:  
            cfsetispeed(&newtio,B9600);  
            cfsetospeed(&newtio,B9600);  
            break;    
        case 19200:  
            cfsetispeed(&newtio,B19200);  
            cfsetospeed(&newtio,B19200);  
            break;  
        case 38400:  
            cfsetispeed(&newtio,B38400);  
            cfsetospeed(&newtio,B38400);  
            break;    
        case 57600:  
            cfsetispeed(&newtio,B57600);  
            cfsetospeed(&newtio,B57600);  
            break;                                                        
        case 115200:  
	        cfsetispeed(&newtio,B115200);  
	        cfsetospeed(&newtio,B115200);  
	        break;    
        case 460800:  
            cfsetispeed(&newtio,B460800);  
            cfsetospeed(&newtio,B460800);  
            break;                        
        default     :  
            printf("**APP** :Don't exist iBaudRate %d !\n",iBaudRate);  
            return (-1);                                                                      
    }  

	/*设置数据位*/  
    newtio.c_cflag &= (~CSIZE);  
    switch( iDataSize )  
    {  
        case 7:  
            newtio.c_cflag |= CS7;  
            break;  
        case 8:  
            newtio.c_cflag |= CS8;  
            break;  
        default:   
            printf("**APP** :Don't exist iDataSize %d !\n",iDataSize);  
            return (-1);                                  
    }  
      
    /*设置校验位*/  
    switch( cParity )  
    {  
        case 'N':                    /*无校验*/  
            newtio.c_cflag &= (~PARENB);  
            break;  
        case 'O':                    /*奇校验*/  
            newtio.c_cflag |= PARENB;  
            newtio.c_cflag |= PARODD;  
            newtio.c_iflag |= (INPCK | ISTRIP);  
            break;  
        case 'E':                    /*偶校验*/  
            newtio.c_cflag |= PARENB;  
            newtio.c_cflag &= (~PARODD);  
            newtio.c_iflag |= (INPCK | ISTRIP);  
            break;                
        default:  
            printf("**APP** :Don't exist cParity %c !\n",cParity);  
            return (-1);                                  
    }  
      
    /*设置停止位*/  
    switch( iStopBit )  
    {  
        case 1:  
	        newtio.c_cflag &= (~CSTOPB);  
	        break;  
        case 2:  
            newtio.c_cflag |= CSTOPB;  
            break;  
        default:  
            printf("**APP** :Don't exist iStopBit %d !\n",iStopBit);  
            return (-1);                                  
    }  
      
    newtio.c_cc[VTIME] = 0; /*设置等待时间*/  
    newtio.c_cc[VMIN] = 1;  /*设置最小字符*/  
	/* 上面两个条件为非零时才有效，两个都为非零时任意一个条件达到都返回，如果两个条件都为零，则马上返回 */
	
    tcflush(fd,TCIFLUSH);       /*刷新输入队列(TCIOFLUSH为刷新输入输出队列)*/  
    iResult = tcsetattr(fd,TCSANOW,&newtio);    /*激活新的设置使之生效,参数TCSANOW表示更改立即发生*/  
  
    if(iResult)
    {  
        printf("**APP** :Set new terminal description error !");  
        return (-1);  
    }     
      
    printf("**APP** : set_port success !\n");  
      
    return 0;  
}

int read_uart_port(int fd,void *buf,int iByte)  
{  
    int iLen = 0;  
    if(!iByte)  
    {  
        printf("**APP** :Read byte number error !\n");  
        return iLen;  
    }  
      
    iLen = read(fd,buf,iByte);  
      
    return iLen;  
}  
  
int write_uart_port(int fd,void *buf,int iByte)  
{  
    int iLen = 0;  
    if(!iByte)  
    {  
        printf("**APP** :Write byte number error !\n");  
        return iLen;  
    }  
      
    iLen = write(fd,buf,iByte);  
      
    return iLen;  
}  
  
  
int close_uart_port(int fd)  
{  
    int iResult = -1;  
      
    iResult = close(fd);  
      
    return iResult;  
}  

 


