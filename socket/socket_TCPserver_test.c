#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include   <net/if.h>
#include   <net/if_arp.h>
#include   <arpa/inet.h>
#include   <errno.h>
#include <sys/ioctl.h>
/************************************************************************************************************************
1、int socket(int family,int type,int protocol)
family:
    指定使用的协议簇:AF_INET（IPv4） AF_INET6（IPv6） AF_LOCAL（UNIX协议） AF_ROUTE（路由套接字） AF_KEY（秘钥套接字）
type:
    指定使用的套接字的类型:SOCK_STREAM（字节流套接字） SOCK_DGRAM
protocol:
    如果套接字类型不是原始套接字，那么这个参数就为0
2、int bind(int sockfd, struct sockaddr *myaddr, int addrlen)
sockfd:
    socket函数返回的套接字描述符
myaddr:
    是指向本地IP地址的结构体指针
myaddrlen:
    结构长度
struct sockaddr{
    unsigned short sa_family; //通信协议类型族AF_xx
    char sa_data[14];  //14字节协议地址，包含该socket的IP地址和端口号
};
struct sockaddr_in{
    short int sin_family; //通信协议类型族
    unsigned short int sin_port; //端口号
    struct in_addr sin_addr; //IP地址
    unsigned char si_zero[8];  //填充0以保持与sockaddr结构的长度相同
};
3、int connect(int sockfd,const struct sockaddr *serv_addr,socklen_t addrlen)
sockfd:
    socket函数返回套接字描述符
serv_addr:
    服务器IP地址结构指针
addrlen:
    结构体指针的长度
4、int listen(int sockfd, int backlog)
sockfd:
    socket函数绑定bind后套接字描述符
backlog:
    设置可连接客户端的最大连接个数，当有多个客户端向服务器请求时，收到此值的影响。默认值20
5、int accept(int sockfd,struct sockaddr *cliaddr,socklen_t *addrlen)
sockfd:
    socket函数经过listen后套接字描述符
cliaddr:
    客户端套接字接口地址结构
addrlen:
    客户端地址结构长度
6、int send(int sockfd, const void *msg,int len,int flags)
7、int recv(int sockfd, void *buf,int len,unsigned int flags)
sockfd:
    socket函数的套接字描述符
msg:
    发送数据的指针
buf:
    存放接收数据的缓冲区
len:
    数据的长度，把flags设置为0
*************************************************************************************************************************/
#define TESTBUFSIZE	100
#define MULTIPLE_NETWOROK       1
#define ETH_NAME    "wlan0"


int main(int argc, char *argv[])
{
    int fd, new_fd, struct_len, numbytes,i;
    struct sockaddr_in server_addr;
    struct sockaddr_in client_addr;
    char buff[TESTBUFSIZE];

    server_addr.sin_family = AF_INET;			//IPV4的协议族
    server_addr.sin_port = htons(8000);			//服务器端口号为8000
    server_addr.sin_addr.s_addr = INADDR_ANY;	//服务器接收任何地址的客户端
    bzero(&(server_addr.sin_zero), 8);			//填充0以保持与sockaddr结构的长度相同
    struct_len = sizeof(struct sockaddr_in);

    fd = socket(AF_INET, SOCK_STREAM, 0);		//打开socket


#if (MULTIPLE_NETWOROK)  //多网卡   
    {
        struct ifreq interface;
        char if_addr[16];
        printf("multiple network car  ");
        memset(&interface,0,sizeof(struct ifreq));
        strncpy(interface.ifr_ifrn.ifrn_name, ETH_NAME, sizeof(ETH_NAME));//指定网卡

        /* 读取该网卡的IP地址 */
        ioctl(fd, SIOCGIFADDR, &interface);
        strcpy(if_addr, inet_ntoa(((struct sockaddr_in *)&interface.ifr_ifru.ifru_addr)->sin_addr));
        printf("ipaddr : %s\n", if_addr);

        /* socket指定网卡收发数据 */
        if(setsockopt(fd, SOL_SOCKET, SO_BINDTODEVICE, (char *)&interface, sizeof(interface))  < 0) 
        {
            printf("SO_BINDTODEVICE failed");
        }
    }


#endif
	
    while(bind(fd, (struct sockaddr *)&server_addr, struct_len) == -1);//绑定地址
    printf("Bind Success!\n");
	
    while(listen(fd, 10) == -1);	//监听
    printf("Listening....\n");
    printf("Ready for Accept,Waitting...\n");
	
    new_fd = accept(fd, (struct sockaddr *)&client_addr, &struct_len);//握手进入阻塞
    printf("Get the Client.\n");
	
    numbytes = send(new_fd,"Welcome to my server\n",21,0); //发送数据
    while((numbytes = recv(new_fd, buff, TESTBUFSIZE, 0)) > 0)
    {
        buff[numbytes] = '\0';
        printf("%s\n",buff);
            if(send(new_fd,buff,numbytes,0)<0)  
            {  
                perror("write");  
                return 1;  
            }  
    }
	
    close(new_fd);	//释放TCP连接
    close(fd);		//关闭socket
    return 0;
}
