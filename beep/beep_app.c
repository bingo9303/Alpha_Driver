#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define fileName "/dev/beep"

int main(int argc, char *argv[])
{
	int result = 0;
	char value = 0;
	int fd = 0;
	char i;

	if(argc != 1)
	{
		printf("**APP** : argc num error!!!\r\n");
		return -1;
	}

	/* 挂载驱动 */
	system("depmod");
	system("modprobe beep.ko");
		

	fd = open(fileName,O_RDWR);
	if(fd < 0)
	{
		printf("**APP** : open %s faild!!!\r\n",fileName);
		return -1;
	}
	else 
	{
		printf("**APP** : open %s succeed!!!\r\n",fileName);
	}

	for(i=0;i<3;i++)
	{
		value = 0;
		result = write(fd,&value,1);
		if(result < 0)
		{
			printf("**APP** : write %s faild!!!\r\n",fileName);
			return -1;
		}
		usleep(200000);
		value = 1;
		result = write(fd,&value,1);
		if(result < 0)
		{
			printf("**APP** : write %s faild!!!\r\n",fileName);
			return -1;
		}
		usleep(200000);
	}


	result = close(fd);
	if(result < 0)
	{
		printf("**APP** : close %s faild!!!\r\n",fileName);
		return -1;
	}
	else
	{
		printf("**APP** : close %s succeed!!!\r\n",fileName);
	}

	return 0;
}




