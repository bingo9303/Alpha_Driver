#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h> 
#include <signal.h>

#define KEY_NUM					1


static int fd=0;		//文件描述符


void SIGIO_Sighandler(int signum)
{
	int i;
	int value = 0;

	if(signum == SIGIO)
	{
		read(fd, &value, 1);
		for(i=0;i<KEY_NUM;i++)
		{
			if(value & (1<<i))
			{
				printf("**APP** : signum = %d , key_%d press\r\n",signum,i);
			}
		}

	}
}



int main(int argc, char *argv[])
{
	int i;
	int result = 0;
	void* result_p=NULL;
	
	char* filename = NULL;
	int flag;

	if(argc < 2)
	{
		printf("**APP** : argc num error!!!\r\n");
		return -1;
	}

	filename = argv[1];
	
	fd = open(filename,O_RDWR);	//默认以阻塞方式打开
	if(fd < 0)
	{
		printf("**APP** : open %s faild!!!\r\n",filename);
		return -1;
	}

	result_p = signal(SIGIO, SIGIO_Sighandler);
	if(result_p == SIG_ERR)
	{
		printf("**APP** : signal SIGIO faild!!!\r\n");
		goto closeApp;
	}


	fcntl(fd,F_SETOWN,getpid());	//将当前进程号告诉内核驱动
	flag = fcntl(fd,F_GETFD);		//获取当前进程状态
	fcntl(fd,F_SETFL,flag | FASYNC);//设置进程开启异步通知功能
	
	while(1)
	{
		sleep(2);		//让进程一直处于休眠状态,否则CPU的占用率很高
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



