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

	
    while((sockfd = socket(AF_INET,SOCK_STREAM,0)) == -1);//打开socket
    printf("We get the sockfd~\n");

	
    their_addr.sin_family = AF_INET;						//IPV4的协议族
    their_addr.sin_port = htons(8000);						//要连接的服务器端口号为8000
    their_addr.sin_addr.s_addr=inet_addr("192.168.0.200");	//要连接的服务器的地址
    bzero(&(their_addr.sin_zero), 8);						//填充0以保持与sockaddr结构的长度相同
	//客户端自身的地址已经分配好，客户端的端口号系统会自动分配，不需要人为去分配


	
    
    while(connect(sockfd,(struct sockaddr*)&their_addr,sizeof(struct sockaddr)) == -1);//和服务器握手连接
    printf("Get the Server~Cheers!\n");
	
    numbytes = recv(sockfd, buf, TESTBUFSIZE,0);//接收服务器端信息  
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

