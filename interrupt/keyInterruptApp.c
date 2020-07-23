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
	time_t lastTimep;

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
		for(i=0;i<KEY_NUM;i++)
		{
			value = i;	
			write(fd,&value,1);
			read(fd, &value, 1);
			if(value == 0)
			{
				
			}
			else
			{
				time_t currentTimep;
				printf("**APP** : key_%d press\r\n",i);
				time (&currentTimep);
				if((currentTimep - lastTimep) < 1)	goto closeApp;	//1s内连续按两次，就退出
				else		lastTimep = currentTimep;
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





