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
	int fd=-1;
	char* res=NULL;
	char str[0xFF]={0};
	int port = atoi(argv[1]);
	
	
	if(argc < 2)
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

	
	while(1)
	{
		int len;
		res = gets(str);	  //gets读一行，getchar()读一个字符。
		if(res != NULL)
		{
			if(strcmp(str,"return") == 0)	break;
			len = strlen(str);

			write_uart_port(fd,str,(len>sizeof(str))?sizeof(str):len);			
		}

	}

		
closeApp:
	close_uart_port(fd);
	printf("**APP** :close write uart\r\n");

	return 0;	
}



