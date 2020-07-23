#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h> 


#define TIMER_BINGO_CMD_OPEN		_IO(0xEF,0)	//命令从0开始
#define TIMER_BINGO_CMD_CLOSE		_IO(0xEF,1)	
#define TIMER_BINGO_CMD_FREQ		_IOR(0xEF,2,int)

int main(int argc, char *argv[])
{
	int result = 0;
	int value = 0;
	int fd = 0;
	char* filename = NULL;
	char str[100];

	if(argc < 2)
	{
		printf("**APP** : argc num error!!!\r\n");
		return -1;
	}

	filename = argv[1];
	
	fd = open(filename,O_RDWR);
	if(fd < 0)
	{
		printf("**APP** : open %s faild!!!\r\n",filename);
		return -1;
	}

	while(1)
	{
		printf("Input cmd:");
		result = scanf("%d",&value);
		if(result != 1)	//按键不是输入1个参数
		{
			gets(str);	//防止卡死
		}
		if(value == 3)	break;

		switch(value)
		{
			case 0:
				{
					ioctl(fd,TIMER_BINGO_CMD_OPEN);
				}
				break;
			case 1:
				{
					ioctl(fd,TIMER_BINGO_CMD_CLOSE);
				}
				break;
			case 2:
				{
					printf("Input DelayTime(ms):");
					result = scanf("%d",&value);
					if(result != 1)	//按键不是输入1个参数
					{
						gets(str);	//防止卡死
					}
					ioctl(fd,TIMER_BINGO_CMD_FREQ,&value);
				}
				break;
		}
	}


	result = close(fd);
	if(result < 0)
	{
		printf("**APP** : close %s faild!!!\r\n",filename);
		return -1;
	}

	return 0;
}





