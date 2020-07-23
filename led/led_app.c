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
	char value = 0;
	int fd = 0;
	char* filename = NULL;

	if(argc < 3)
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

	if(atoi(argv[2]) == 0)	value = 0;
	else					value = 1;

	result = write(fd,&value,1);
	if(result < 0)
	{
		printf("**APP** : write %s faild!!!\r\n",filename);
		return -1;
	}
	else
	{
		printf("**APP** : write %s succeed!!!\r\n",filename);
	}

	result = close(fd);
	if(result < 0)
	{
		printf("**APP** : close %s faild!!!\r\n",filename);
		return -1;
	}

	return 0;
}




