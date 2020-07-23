#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>



#define DEALY_TIME		500000


int main(int argc, char *argv[])
{
	int result = 0;
	char value = 0;
	int fd = 0;
	char* filename = NULL;

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
		read(fd, &value, 1);
		if(value)
		{

		}
		else
		{
			int count;
			for(count=0;count<DEALY_TIME;count++)
			{
				read(fd, &value, 1);
				if(value)	break;
			}
			if(count >= DEALY_TIME)
			{
				printf("**APP** : key hold !\r\n");
				break;
			}
			else
			{
				printf("**APP** : key press !\r\n");
			}
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




