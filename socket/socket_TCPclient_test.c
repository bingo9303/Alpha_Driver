#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include   <net/if.h>
#include   <net/if_arp.h>
#include   <arpa/inet.h>
#include   <errno.h>
#include <sys/ioctl.h>

#define TESTBUFSIZE	100

#define MULTIPLE_NETWOROK       1
#define ETH_NAME    "wlan0"

int main(int argc,char *argv[])
{
    int sockfd,numbytes;
    char buf[TESTBUFSIZE];
    struct sockaddr_in their_addr;
    printf("break!");

	
    while((sockfd = socket(AF_INET,SOCK_STREAM,0)) == -1);//打开socket
    printf("We get the sockfd~\n");

#if (MULTIPLE_NETWOROK)  //多网卡   
    {
        struct ifreq interface;
        char if_addr[16];
        printf("multiple network car  ");
        memset(&interface,0,sizeof(struct ifreq));
        strncpy(interface.ifr_ifrn.ifrn_name, ETH_NAME, sizeof(ETH_NAME));//指定网卡

        /* 读取该网卡的IP地址 */
        ioctl(sockfd, SIOCGIFADDR, &interface);
        strcpy(if_addr, inet_ntoa(((struct sockaddr_in *)&interface.ifr_ifru.ifru_addr)->sin_addr));
        printf("ipaddr : %s\n", if_addr);

        /* socket指定网卡收发数据 */
        if(setsockopt(sockfd, SOL_SOCKET, SO_BINDTODEVICE, (char *)&interface, sizeof(interface))  < 0) 
        {
            printf("SO_BINDTODEVICE failed");
        }
    }
#endif
	
    their_addr.sin_family = AF_INET;						//IPV4的协议族
    their_addr.sin_port = htons(8000);						//要连接的服务器端口号为8000
    their_addr.sin_addr.s_addr=inet_addr("192.168.0.102");	//要连接的服务器的地址
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

