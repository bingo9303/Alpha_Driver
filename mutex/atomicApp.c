#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char *argv[])
{
	int result = 0;
	int fd = 0;
	char* fileName = NULL;
	char i;
	

	if(argc != 2)
	{
		printf("**APP** : argc num error!!!\r\n");
		return -1;
	}

	fileName = argv[1];
	
	fd = open(fileName,O_RDWR);
	if(fd < 0)
	{
		printf("**APP** : open %s faild!!!\r\n",fileName);
		return -1;
	}
	else
	{
		char j;
		for(j=0;j<5;j++)
		{
			sleep(5);
			printf("**APP** : sleep %d\r\n",j);
		}
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





