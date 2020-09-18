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


const char* str_greetings[] = 	//默认问候语
{
	"<001>ni hao",
	"<002>xiao xiao jiao",
	"<003>lai ren a",
	"<004>you ren zai ma",
	"<005>xiao xiao jiao tong xue",
};


const char* str_whatUp[] = 
{
	"<001>chang shou ge lai ting",
	"<002>jiao ba ba",
	"<003>xian zai ji dian le",
};


int init_scene(void)
{
	getLd3320AppInfo()->sceneIndex = SCENE_GREETING;
	write(getLd3320AppInfo()->ld3320_fd,str_greetings,sizeof(str_greetings));

}


int greetingsFunction(int cmd)
{
	switch(cmd)
	{
		case 1:
			printf("你好\r\n");
			SYN_FrameInfo(getLd3320AppInfo()->uart_fd,0,"你好主人");
			break;
		case 2:
			printf("小小娇\r\n");
			SYN_FrameInfo(getLd3320AppInfo()->uart_fd,0,"找我干嘛");
			break;
		case 3:
			printf("来人啊\r\n");
			SYN_FrameInfo(getLd3320AppInfo()->uart_fd,0,"主人有什么吩咐");
			break;
		case 4:
			printf("有人在吗\r\n");
			SYN_FrameInfo(getLd3320AppInfo()->uart_fd,0,"主人有什么吩咐");
			break;
		case 5:
			printf("小小娇同学\r\n");
			SYN_FrameInfo(getLd3320AppInfo()->uart_fd,0,"找我干嘛");
			break;
		default:
			return -1;
	}

	write(getLd3320AppInfo()->ld3320_fd,str_whatUp,sizeof(str_whatUp));
	getLd3320AppInfo()->sceneIndex = SCENE_WHATUP;

	return 0;
}






int whatUpFunction(int cmd)
{
	switch(cmd)
	{
		case 1:
			printf("唱首歌来听\r\n");
			SYN_FrameInfo(getLd3320AppInfo()->uart_fd,0,"想听什么歌");
			break;
		case 2:
			printf("叫爸爸\r\n");
			SYN_FrameInfo(getLd3320AppInfo()->uart_fd,0,"爸爸");
			break;
		case 3:
			printf("现在几点了\r\n");
			SYN_FrameInfo(getLd3320AppInfo()->uart_fd,0,"我也不知道");
			break;
		default:
			return -1;
	}

	write(getLd3320AppInfo()->ld3320_fd,str_whatUp,sizeof(str_whatUp));
	getLd3320AppInfo()->sceneIndex = SCENE_WHATUP;

	return 0;
}






