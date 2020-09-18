#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h> 
#include "ld3320_app.h"
#include "uart_common.h"



#define LD3320_UART_PORT		2


static struct ld3320_app_info	_ld3320_app_info;


struct ld3320_app_info* getLd3320AppInfo(void)
{
	return &_ld3320_app_info;
}



int main(int argc, char *argv[])
{
	int i;
	int result = 0;
	int value = 0;
	char* filename = NULL;
	if(argc < 2)
	{
		printf("**APP** : argc num error!!!\r\n");
		return -1;
	}

	filename = argv[1];
	
	getLd3320AppInfo()->ld3320_fd = open(filename,O_RDWR);	//默认以阻塞方式打开
	if(getLd3320AppInfo()->ld3320_fd < 0)
	{
		printf("**APP** : open %s faild!!!\r\n",filename);
		return -1;
	}

	getLd3320AppInfo()->uart_fd = open_uart_port(LD3320_UART_PORT,OEPN_UART_BLOCK); 
	init_scene();

	while(1)
	{
		read(getLd3320AppInfo()->ld3320_fd, &value, 1);
		switch(getLd3320AppInfo()->sceneIndex)
		{
			case SCENE_GREETING:
				greetingsFunction(value);
				break;
			case SCENE_WHATUP:
				whatUpFunction(value);
				break;
		}
	}


closeApp:
	close_uart_port(getLd3320AppInfo()->uart_fd);  
	result = close(getLd3320AppInfo()->ld3320_fd);
	if(result < 0)
	{
		printf("**APP** : close %s faild!!!\r\n",filename);
		return -1;
	}

	return 0;
}
























