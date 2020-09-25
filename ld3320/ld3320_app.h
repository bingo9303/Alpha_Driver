#ifndef __LD3320_APP_H
#define __LD3320_APP_H


struct ld3320_app_info
{
	int ld3320_fd;
	int uart_fd;
	int sceneIndex;
	int sceneLen;
	const char** pCurrentStr;
};





enum
{
	SCENE_GREETING,
	SCENE_WHATUP,


};

void init_scene(void);
int greetingsFunction(int cmd);
int whatUpFunction(int cmd);
struct ld3320_app_info* getLd3320AppInfo(void);
void SYN_FrameInfo(int fd,unsigned char Music,char *HZdata);
void recoverScene(void);

#endif



