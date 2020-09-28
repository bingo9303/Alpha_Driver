#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>
#include <dirent.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <signal.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/msg.h>
#include <sys/sem.h>
#include <pthread.h>
#include <semaphore.h>
#include <poll.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <pthread.h>


char wbuf[50];
char rbuf[50];
 
struct udp_args
{
	int udp;
	struct sockaddr_in raddr;
};
 
void* thread(void* args)
{
	char rbuf[50]={0};
	struct udp_args s = *((struct udp_args*)args);
	
	int udp = s.udp;
	struct sockaddr_in raddr = s.raddr;
	int len =sizeof(struct sockaddr);
	while(1)
	{
		printf("receiver data:");
		recvfrom(udp,rbuf,50,0,(struct sockaddr*)&raddr,
			&len);
		printf("%s\n",rbuf);
		bzero(rbuf,50);
	}
}
 
int main()
{
	struct udp_args udp_a;
 
	int udp,size,opt=1;
	struct sockaddr_in daddr;
	struct sockaddr_in raddr;
	pthread_t tid;


	/* 接收进程：数据接收的地址信息(相当于服务器) */
	raddr.sin_family = AF_INET;
	raddr.sin_port = htons(8888);
	raddr.sin_addr.s_addr =htonl(INADDR_ANY);

	/* 发送进程：数据发送的地址信息(相当于客户端) */
	daddr.sin_family = AF_INET;
	daddr.sin_port = htons(1000);
	daddr.sin_addr.s_addr = inet_addr("192.168.0.255");	//地址255，向192.168.0.xxx的地址发送数据
 
	udp = socket(AF_INET,SOCK_DGRAM,0);

	/* 设置广播属性 */
	setsockopt(udp,SOL_SOCKET,SO_BROADCAST,&opt,sizeof opt);
	setsockopt(udp,SOL_SOCKET,SO_REUSEADDR,&opt,sizeof opt);
 
	bind(udp,(struct sockaddr*)&raddr,sizeof(struct sockaddr));
	
	udp_a.udp = udp;
	udp_a.raddr = raddr;
 
	pthread_create(&tid,NULL,thread,(void*)&udp_a);//创建一个线程，用以接收数据
 
	int len = sizeof(struct sockaddr);
	while(1)
	{
		printf("sent data:");
		scanf("%s",wbuf);
		sendto(udp,wbuf,strlen(wbuf),0,
			(struct sockaddr*)&daddr,len);
		bzero(wbuf,50);
	}
}

/*
已经在c文件中包含了线程的头文件<pthread.h>，可是编译的时候却报错“对pthread_create未定义的引用“，
原来时因为 pthread库不是Linux系统默认的库，连接时需要使用库libpthread.a,
所以在使用pthread_create创建线程时，在编译中要加-lpthread参数:
gcc createThread.c -lpthread -o createThread. 加上这个以后编译成功！

*/

