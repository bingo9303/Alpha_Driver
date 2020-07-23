#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>



int main(int argc, char *argv[])
{
	char* fileName = NULL;
	int fd = 0;
	int result = 0;
	char readBuffer[100];
	char writeBuffer[]={"hello world!!!"};

	if(argc != 3)
	{
		printf("**APP** : argc num error!!!\r\n");
		return -1;
	}

	fileName = argv[1];

	fd = open(fileName,O_RDWR);
	if(fd == -1)
	{
		printf("**APP** : open %s file error\r\n",fileName);
		return -1;
	}

	if(atoi(argv[2]) == 1)	//read
	{
		result = read(fd,readBuffer,50);
		if(result == -1)
		{
			printf("**APP** : read %s file error\r\n",fileName);
		}
		else
		{
			printf("**APP** : read %s file data is <%s>\r\n",fileName,readBuffer);
		}
	}
	else if(atoi(argv[2]) == 2)	//write
	{
		result = write(fd,writeBuffer,sizeof(writeBuffer));
		if(result == -1)
		{
			printf("**APP** : write %s file error\r\n",fileName);
		}
		else
		{
			//printf("**APP** : write %s file succeed\r\n",fileName);
		}
	}
	
	result = close(fd);
	if(result == -1)
	{
		printf("**APP** : close %s file error\r\n",fileName);
	}

	return 0;
}






