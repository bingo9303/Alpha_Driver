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


const char* str_greetings[] = 	//Ĭ���ʺ���
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
			printf("���\r\n");
			SYN_FrameInfo(getLd3320AppInfo()->uart_fd,0,"�������");
			break;
		case 2:
			printf("СС��\r\n");
			SYN_FrameInfo(getLd3320AppInfo()->uart_fd,0,"���Ҹ���");
			break;
		case 3:
			printf("���˰�\r\n");
			SYN_FrameInfo(getLd3320AppInfo()->uart_fd,0,"������ʲô�Ը�");
			break;
		case 4:
			printf("��������\r\n");
			SYN_FrameInfo(getLd3320AppInfo()->uart_fd,0,"������ʲô�Ը�");
			break;
		case 5:
			printf("СС��ͬѧ\r\n");
			SYN_FrameInfo(getLd3320AppInfo()->uart_fd,0,"���Ҹ���");
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
			printf("���׸�����\r\n");
			SYN_FrameInfo(getLd3320AppInfo()->uart_fd,0,"����ʲô��");
			break;
		case 2:
			printf("�аְ�\r\n");
			SYN_FrameInfo(getLd3320AppInfo()->uart_fd,0,"�ְ�");
			break;
		case 3:
			printf("���ڼ�����\r\n");
			SYN_FrameInfo(getLd3320AppInfo()->uart_fd,0,"��Ҳ��֪��");
			break;
		default:
			return -1;
	}

	write(getLd3320AppInfo()->ld3320_fd,str_whatUp,sizeof(str_whatUp));
	getLd3320AppInfo()->sceneIndex = SCENE_WHATUP;

	return 0;
}






