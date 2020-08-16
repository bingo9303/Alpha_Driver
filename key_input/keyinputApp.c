#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h> 
#include <poll.h>
#include <sys/select.h>
#include <sys/time.h>
#include <signal.h>
#include <fcntl.h>
#include <linux/input.h>


#define KEY_NUM					1

int main(int argc, char *argv[])
{
	int i;
	int result = 0;
	int fd = 0;
	char* filename = NULL;
	struct input_event inputInfo;
	
	if(argc < 2)
	{
		printf("**APP** : argc num error!!!\r\n");
		return -1;
	}

	filename = argv[1];
	
	fd = open(filename,O_RDWR);	//默认以阻塞方式打开
	if(fd < 0)
	{
		printf("**APP** : open %s faild!!!\r\n",filename);
		return -1;
	}

	while(1)
	{
		result = read(fd, &inputInfo, sizeof(inputInfo));
		if(result > 0)
		{
			switch(inputInfo.type)
			{
				case EV_KEY:
					if(inputInfo.code < BTN_MISC)
					{
						printf("**APP** : KEY %d %s,%d s %d us \r\n",inputInfo.code,inputInfo.value?"press":"raise",
							inputInfo.time.tv_sec,inputInfo.time.tv_usec);
					}
					else
					{
						printf("**APP** : BUTTON %d %s,%d s %d us \r\n",inputInfo.code,inputInfo.value?"press":"raise",
							inputInfo.time.tv_sec,inputInfo.time.tv_usec);
					}
					break;
				case EV_REL:
					break;
				case EV_ABS:
					break;			
				default:
					break;
			}
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





