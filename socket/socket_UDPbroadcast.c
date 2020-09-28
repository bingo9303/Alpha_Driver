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


	/* ���ս��̣����ݽ��յĵ�ַ��Ϣ(�൱�ڷ�����) */
	raddr.sin_family = AF_INET;
	raddr.sin_port = htons(8888);
	raddr.sin_addr.s_addr =htonl(INADDR_ANY);

	/* ���ͽ��̣����ݷ��͵ĵ�ַ��Ϣ(�൱�ڿͻ���) */
	daddr.sin_family = AF_INET;
	daddr.sin_port = htons(1000);
	daddr.sin_addr.s_addr = inet_addr("192.168.0.255");	//��ַ255����192.168.0.xxx�ĵ�ַ��������
 
	udp = socket(AF_INET,SOCK_DGRAM,0);

	/* ���ù㲥���� */
	setsockopt(udp,SOL_SOCKET,SO_BROADCAST,&opt,sizeof opt);
	setsockopt(udp,SOL_SOCKET,SO_REUSEADDR,&opt,sizeof opt);
 
	bind(udp,(struct sockaddr*)&raddr,sizeof(struct sockaddr));
	
	udp_a.udp = udp;
	udp_a.raddr = raddr;
 
	pthread_create(&tid,NULL,thread,(void*)&udp_a);//����һ���̣߳����Խ�������
 
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
�Ѿ���c�ļ��а������̵߳�ͷ�ļ�<pthread.h>�����Ǳ����ʱ��ȴ������pthread_createδ��������á���
ԭ��ʱ��Ϊ pthread�ⲻ��LinuxϵͳĬ�ϵĿ⣬����ʱ��Ҫʹ�ÿ�libpthread.a,
������ʹ��pthread_create�����߳�ʱ���ڱ�����Ҫ��-lpthread����:
gcc createThread.c -lpthread -o createThread. ��������Ժ����ɹ���

*/

