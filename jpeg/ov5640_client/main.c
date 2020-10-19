#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#define SOCKET_MEM_POOL_SIZE		(1024*1024*4)		//����4M���ڴ�ռ�
#define SOCKET_CELL_SIZE	1024

#define CACHE_FRAME_MAX		20
#define CACHE_FRAME_MIN		3

unsigned char* socketMemPool = NULL;
unsigned int write_pos = 0;
unsigned int read_pos = 0;


unsigned int cacheFrame_start[CACHE_FRAME_MAX],cacheFrame_end[CACHE_FRAME_MAX];
unsigned int cache_write_pos = 0;
unsigned int cache_read_pos = 0;


sem_t sem_recv;


unsigned int getVaidRange(void)
{
	if(write_pos >= read_pos)	return write_pos-read_pos;
	else
	{
		return SOCKET_MEM_POOL_SIZE-read_pos+write_pos;
	}
}


void* socketReceive(void* args)
{
	int sockfd = *args;
	int numbytes;
	sem_init(&sem_recv,0,0);
	
	while(1)
    {
        numbytes=recv(sockfd,&socketMemPool[write_pos],SOCKET_CELL_SIZE,0);  
      	if(numbytes<0)
      	{
			printf("socket recv error %d",numbytes);
			break;
		}
		else
		{
			write_pos += numbytes;
			if(write_pos >= SOCKET_MEM_POOL_SIZE)	
			{
				write_pos = write_pos-SOCKET_MEM_POOL_SIZE;
			}	
			
			if(getVaidRange() >= SOCKET_CELL_SIZE)
			{
				sem_post(&sem_recv);
			}
		}
		usleep(1);
    }
	sem_destroy(&sem_recv);
}


void* analyse_socketData(void* args)
{
	unsigned int byteNum=0,i;
	while(1)
	{
		sem_wait(&sem_recv);
		byteNum = getVaidRange();
		for(i=0;i<byteNum;i++)
		{
			if(socketMemPool[read_pos] == 0xFF)
			{
				if(socketMemPool[read_pos+1] == 0xD8)
				{
					cacheFrame_start[cache_write_pos] = socketMemPool[read_pos+2];
				}
				else if(socketMemPool[read_pos+1] == 0xD9)
				{
					cacheFrame_end[cache_write_pos] = socketMemPool[read_pos+2];
					cache_write_pos++;
					if(cache_write_pos >= CACHE_FRAME_MAX)	cache_write_pos=0;
				}
			}
			read_pos++;
			if(read_pos >= SOCKET_MEM_POOL_SIZE)	read_pos = 0;
		}		
	}


}






//argv[1]:IP��ַ
//argv[2]:�˿ں�
int main(int argc,char *argv[])
{
	struct udp_args udp_a;
	pthread_t tid_recv;
    int sockfd,numbytes;
	char buf[50];
    struct sockaddr_in their_addr;
   
	if(argc < 3)
	{
		printf("**APP** : argc num error\r\n");
		return -1;
	}
	socketMemPool = malloc(SOCKET_MEM_POOL_SIZE);
	if(socketMemPool == NULL)
	{
		printf("**APP** : malloc socket pool faild\r\n");
		return -1;
	}
	
    while((sockfd = socket(AF_INET,SOCK_STREAM,0)) == -1);//��socket
    printf("We get the sockfd~\n");

	
    their_addr.sin_family = AF_INET;						//IPV4��Э����
    their_addr.sin_port = htons(atoi(argv[2]));				//Ҫ���ӵķ������˿ں�Ϊ8000
    their_addr.sin_addr.s_addr=inet_addr(argv[1]);	//Ҫ���ӵķ������ĵ�ַ
    bzero(&(their_addr.sin_zero), 8);						//���0�Ա�����sockaddr�ṹ�ĳ�����ͬ
	//�ͻ�������ĵ�ַ�Ѿ�����ã��ͻ��˵Ķ˿ں�ϵͳ���Զ����䣬����Ҫ��Ϊȥ����

  
    while(connect(sockfd,(struct sockaddr*)&their_addr,sizeof(struct sockaddr)) == -1);//�ͷ�������������
    printf("Get the Server~Cheers!\n");
	
    numbytes = recv(sockfd, buf, 50,0);//F429�������ֳɹ������ĵ�һ���ӡ��Ϣ 
    buf[numbytes]='\0';  
    printf("%s",buf);

	pthread_create(&tid_recv,NULL,socketReceive,(void*)&sockfd);//����һ���̣߳����Խ�������


	
    pthread_join(tid_recv,NULL);
    close(sockfd);
    return 0;
}







