#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h> 


static const char* str_greetings[] = 	//Ĭ���ʺ���
{
	"<001>ni hao",
	"<002>xiao xiao jiao",
	"<003>lai ren a",
	"<004>you ren zai ma",
};




int main(int argc, char *argv[])
{
	int i;
	int result = 0;
	int value = 0;
	int fd = 0;
	char* filename = NULL;
	if(argc < 2)
	{
		printf("**APP** : argc num error!!!\r\n");
		return -1;
	}

	filename = argv[1];
	
	fd = open(filename,O_RDWR);	//Ĭ����������ʽ��
	if(fd < 0)
	{
		printf("**APP** : open %s faild!!!\r\n",filename);
		return -1;
	}

	

	while(1)
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


closeApp:
	result = close(fd);
	if(result < 0)
	{
		printf("**APP** : close %s faild!!!\r\n",filename);
		return -1;
	}

	return 0;
}
























