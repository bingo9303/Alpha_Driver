#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#define TESTBUFSIZE	100


int main(int argc,char *argv[])
{
    int sockfd,numbytes;
    char buf[TESTBUFSIZE];
    struct sockaddr_in their_addr;
    printf("break!");

	
    while((sockfd = socket(AF_INET,SOCK_STREAM,0)) == -1);//��socket
    printf("We get the sockfd~\n");

	
    their_addr.sin_family = AF_INET;						//IPV4��Э����
    their_addr.sin_port = htons(8000);						//Ҫ���ӵķ������˿ں�Ϊ8000
    their_addr.sin_addr.s_addr=inet_addr("192.168.0.200");	//Ҫ���ӵķ������ĵ�ַ
    bzero(&(their_addr.sin_zero), 8);						//���0�Ա�����sockaddr�ṹ�ĳ�����ͬ
	//�ͻ�������ĵ�ַ�Ѿ�����ã��ͻ��˵Ķ˿ں�ϵͳ���Զ����䣬����Ҫ��Ϊȥ����


	
    
    while(connect(sockfd,(struct sockaddr*)&their_addr,sizeof(struct sockaddr)) == -1);//�ͷ�������������
    printf("Get the Server~Cheers!\n");
	
    numbytes = recv(sockfd, buf, TESTBUFSIZE,0);//���շ���������Ϣ  
    buf[numbytes]='\0';  
    printf("%s",buf);
    while(1)
    {
        printf("Entersome thing:");
        scanf("%s",buf);
        numbytes = send(sockfd, buf, strlen(buf), 0);
            numbytes=recv(sockfd,buf,TESTBUFSIZE,0);  
        buf[numbytes]='\0'; 
        printf("received:%s\n",buf);  
    }
    close(sockfd);
    return 0;
}

