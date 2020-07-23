#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h> 

#define KEY_NUM					1



int main(int argc, char *argv[])
{
	int i;
	int result = 0;
	int value = 0;
	int fd = 0;
	char* filename = NULL;

	fd_set readfds;	//读操作文件描述符集
	struct timeval timeout; /* 超时结构体 */
		
	
	if(argc < 2)
	{
		printf("**APP** : argc num error!!!\r\n");
		return -1;
	}

	filename = argv[1];
	
	fd = open(filename,O_RDWR|O_NONBLOCK);	//默认以非阻塞方式打开
	if(fd < 0)
	{
		printf("**APP** : open %s faild!!!\r\n",filename);
		return -1;
	}

		

	while(1)
	{
		timeout.tv_sec = 1;
		timeout.tv_usec = 0;
		FD_ZERO(&readfds); 			/* 清除 readfds */
    	FD_SET(fd, &readfds); 		/* 将 fd 添加到 readfds 里面 */
		
		result = select(fd+1,&readfds,NULL,NULL,&timeout);

		switch(result)
		{
			case 0:
				//printf("**APP** : time out\r\n",i);
				break;
			case -1:
				printf("**APP** : select error\r\n",i);
				break;
			default:
				if(FD_ISSET(fd, &readfds))	//判断是否是描述符集里的fd可以进行操作
				{
					read(fd, &value, 1);
					if(value == 0)
					{
						
					}
					else
					{
						for(i=0;i<KEY_NUM;i++)
						{
							if(value & (1<<i))
							{
								printf("**APP** : key_%d press\r\n",i);
							}
						}
					}
				}
				break;
		}

	}


closeApp:
	result = close(fd);
	if(result < 0)
	{
		printf("**APP** : close %s faild!!!\r\n",filename);
		return -1;
	}

	return 0;
}





