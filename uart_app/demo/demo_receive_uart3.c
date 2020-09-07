#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h> 
#include "uart_common.h"



int main(int argc, char *argv[])
{
	int fd=-1,result=0;
	int port = atoi(argv[1]);
	int iBaudRate = atoi(argv[2]);
	char readBuff[0xFF];
	int returnCount;
	
	if(argc < 3)
	{
		printf("**APP** : argc num error!!!\r\n");
		return -1;
	}

	if((port < 1) || (port > 8))
	{
		printf("**APP** : uart port argc num error!!!\r\n");
		return -1;
	}

	fd = open_uart_port(port,OEPN_UART_BLOCK);	//以阻塞打开
	if(fd < 0)
	{
		printf("**APP** : open uart port faild!!!\r\n");
		return -1;
	}

	result = set_uart_port(fd,iBaudRate,8,'N',1);
	if(result < 0)
	{
		printf("**APP** : set uart port faild!!!\r\n");
		goto closeApp;
	}
	
	while(1)
	{
		returnCount = 0;	
		returnCount = read_uart_port(fd, readBuff,0xFF);

		//printf("returnCount = %d\r\n",returnCount);
	
		if(returnCount > 0)
		{
			int i;
			for(i=0;i<returnCount;i++)
				printf("%c",readBuff[i]);
			fflush(stdout);	//刷新一下缓冲区 让它马上输出. 不然没有\r\n没有输出到控制台
		}
	}

	
closeApp:
	close_uart_port(fd);

	return 0;	
}






#if 0

int main(int argc, char *argv[])
{
	int fd=-1,result=0;
	fd_set readfds;	//�������ļ���������
	struct timeval timeout; /* ��ʱ�ṹ�� */
	int port = atoi(argv[1]);
	int iBaudRate = atoi(argv[2]);
	char readBuff[0xFF];
	int returnCount;
	
	if(argc < 3)
	{
		printf("**APP** : argc num error!!!\r\n");
		return -1;
	}

	if((port < 1) || (port > 8))
	{
		printf("**APP** : uart port argc num error!!!\r\n");
		return -1;
	}

	fd = open_uart_port(port,OEPN_UART_NOBLOCK);	//�Է�������
	if(fd < 0)
	{
		printf("**APP** : open uart port faild!!!\r\n");
		return -1;
	}

	result = set_uart_port(fd,iBaudRate,8,'N',1);
	if(result < 0)
	{
		printf("**APP** : set uart port faild!!!\r\n");
		return -1;
	}
	
	while(1)
	{
		returnCount = 0;
		timeout.tv_sec = 0;
		timeout.tv_usec = 1000*10;	//10ms
		FD_ZERO(&readfds); 			/* ��� readfds */
    	FD_SET(fd, &readfds); 		/* �� fd ��ӵ� readfds ���� */

		result = select(fd+1,&readfds,NULL,NULL,&timeout);

		switch(result)
		{
			case 0:
				//printf("**APP** : time out\r\n",i);
				break;
			case -1:
				printf("**APP** : select error\r\n");
				break;
			default:
				if(FD_ISSET(fd, &readfds))	//�ж��Ƿ��������������fd���Խ��в���
				{
					returnCount = read(fd, readBuff, 0xFF);
					if(returnCount > 0)
					{
						int i;
						for(i=0;i<returnCount;i++)
							printf("%02X ",readBuff[i]);
					}
				}
				break;
		}
	}
}

#endif




