#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <semaphore.h>
#include <fcntl.h>
#include <linux/fb.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include "jpeglib.h"
#include "jerror.h"
#include <net/if.h>
#include <net/if_arp.h>
#include <arpa/inet.h>
#include <errno.h>
#include <sys/ioctl.h>

#define DEBUG_TEST			0
#define SOCKET_MEM_POOL_SIZE		(1024*1024*4)		//申请4M的内存空间
#define SOCKET_CELL_SIZE	4096
#define JPEG_BUFF_SIZE		(1024*500)			//申请500K内存用于存储要显示的jpeg数据

#define CACHE_FRAME_MAX		20
#define CACHE_FRAME_MIN		5

#define FB_START_X			0
#define FB_START_Y			0


#define POS_OFFSET_ADD(startpos,offset,maxSize)	(((startpos+offset) >= maxSize)?((startpos+offset)-maxSize):(startpos+offset))
#define POS_OFFSET_SUB(startpos,offset,maxSize) ((startpos < offset)?(maxSize - (offset-startpos)):(startpos-offset))

#define    FB_DEV  "/dev/fb0"

#define MULTIPLE_NETWOROK       1
#define ETH_NAME    			"wlan0"

char returnFlag = 0;

unsigned char bagHeadInfo[] = {0x3C, 0x42, 0x69, 0x6E, 0x67, 0x6F, 0x53, 0x74, 0x61, 0x72, 0x74, 0xFF, 0xD8};//0xFF 0xD8为jpeg标志位
unsigned char bagTailInfo[] = {0x42, 0x69, 0x6E, 0x67, 0x6F, 0x45, 0x6E, 0x64, 0x3E};

unsigned char* socketMemPool = NULL;
unsigned int write_pos = 0;
unsigned int read_pos = 0;

struct jpegBuffer
{
	unsigned int len;
	unsigned char* buffer;
};

struct jpegBuffer _jpegBuffer[CACHE_FRAME_MAX];
unsigned int cache_write_pos = 0;
unsigned int cache_read_pos = 0;


sem_t sem_recv;
sem_t sem_jpeg;

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
 


unsigned int getVaidRange(unsigned int write,unsigned int read,unsigned int maxSize)
{
	if(write >= read)	return write-read;
	else
	{
		return maxSize-read+write;
	}
}

unsigned int isVaildSocketBag_head(unsigned int readPos)
{
	char i,len;
	unsigned int readpos_temp = readPos;
	len = sizeof(bagHeadInfo);
	for(i=0;i<len;i++)
	{
		if(socketMemPool[readPos] == bagHeadInfo[i])
		{
			readPos++;
			if(readPos >= SOCKET_MEM_POOL_SIZE)	readPos = 0;
		}
		else return 0xFFFFFFFF;
	}
	return readpos_temp;
}

unsigned int isVaildSocketBag_tail(unsigned int readPos)
{
	char i,len;
	unsigned int readpos_temp = readPos;
	len = sizeof(bagTailInfo);
	for(i=0;i<len;i++)
	{
		if(socketMemPool[readPos] == bagTailInfo[i])
		{
			readPos++;
			if(readPos >= SOCKET_MEM_POOL_SIZE)	readPos = 0;
		}
		else return 0xFFFFFFFF;
	}
	return readpos_temp;
}



unsigned int isJpegFlag(unsigned int readPos,char stOrEnd)
{
	char i,len;
	char jpegFlag[2];
	unsigned int readpos_temp = readPos;
	len = 2;

	jpegFlag[0] = 0xFF;
	if(stOrEnd == 0)
	{
		jpegFlag[1] = 0xD8;
	}
	else
	{
		jpegFlag[1] = 0xD9;
	}
	

	for(i=0;i<len;i++)
	{
		if(socketMemPool[readPos] == jpegFlag[i])
		{
			readPos++;
			if(readPos >= SOCKET_MEM_POOL_SIZE)	readPos = 0;
		}
		else return 0xFFFFFFFF;
	}
	return readpos_temp;
}



char isVaildCheckSum(unsigned int start,unsigned int end)
{
	unsigned int i;
	unsigned char checkSum=0;
	
	while(start != end)
	{
		checkSum += socketMemPool[start];
		start++;
		if(start >= SOCKET_MEM_POOL_SIZE)	start = 0;
	}
	if(checkSum == socketMemPool[end])	return 1;
	else								return 0;
}



void frameBuffFifo(unsigned int start,unsigned int end)
{
	unsigned int i=0;
	_jpegBuffer[cache_write_pos].len = 0;
	while(start != end)//checkSum不复制
	{
		_jpegBuffer[cache_write_pos].buffer[i++] = socketMemPool[start++];
		if(start >= SOCKET_MEM_POOL_SIZE)	start = 0;
		_jpegBuffer[cache_write_pos].len++;
	}
	cache_write_pos = POS_OFFSET_ADD(cache_write_pos,1,CACHE_FRAME_MAX);
}


void* socketReceive(void* args)
{
	int sockfd = *((int*)args);
	int numbytes;
	
	
	while(1)
    {
		pthread_mutex_lock(&mutex);
		pthread_mutex_unlock(&mutex);
        numbytes=recv(sockfd,&socketMemPool[write_pos],SOCKET_CELL_SIZE,0); 
      	if(numbytes<0)
      	{
			printf("socket recv error %d",numbytes);
			break;
		}
		else
		{
			write_pos = POS_OFFSET_ADD(write_pos,numbytes,SOCKET_MEM_POOL_SIZE);
		
			if(getVaidRange(write_pos,read_pos,SOCKET_MEM_POOL_SIZE) >= SOCKET_CELL_SIZE)
			{
				
				sem_post(&sem_recv);
			}
		}
		usleep(1);
    }
	returnFlag = 1;
}


void* analyse_socketData(void* args)
{
	unsigned int byteNum=0,i;
	static char flag = 0;	//20201107
	static unsigned int startAddr,endAddr;
#if DEBUG_TEST
	unsigned errorCount[3] = {0};
#endif
	while(1)
	{
		sem_wait(&sem_recv);
		if(returnFlag)	break;
		pthread_mutex_lock(&mutex);
		pthread_mutex_unlock(&mutex);
		byteNum = getVaidRange(write_pos,read_pos,SOCKET_MEM_POOL_SIZE);
		for(i=0;i<byteNum;i++)
		{
			if((isVaildSocketBag_head(read_pos) != 0xFFFFFFFF))
			{
				startAddr = POS_OFFSET_ADD(read_pos,sizeof(bagHeadInfo)-2,SOCKET_MEM_POOL_SIZE);
				flag = 1;
			}
			else if((flag==1) && (isJpegFlag(read_pos,1) != 0xFFFFFFFF))
			{
				flag = 0;
				if(isVaildSocketBag_tail(POS_OFFSET_ADD(read_pos,3,SOCKET_MEM_POOL_SIZE)) != 0xFFFFFFFF)
				{
					endAddr = POS_OFFSET_ADD(read_pos,2,SOCKET_MEM_POOL_SIZE);
					if(isVaildCheckSum(startAddr,endAddr))
					{
						pthread_mutex_lock(&mutex);
						frameBuffFifo(startAddr,endAddr);
						pthread_mutex_unlock(&mutex);
						sem_post(&sem_jpeg);
					}
					else
					{
					#if DEBUG_TEST
						errorCount[1]++;
						printf("debug 1 debug 1 debug 1 errorCount = %d\r\n",errorCount[1]);
					#endif
					}
				}
				else
				{
				#if DEBUG_TEST
					errorCount[0]++;
					printf("debug 0 debug 0 debug 0 errorCount = %d\r\n",errorCount[0]);
				#endif
				}
				
				
			}
			else if((flag==1) && (isJpegFlag(read_pos,0) != 0xFFFFFFFF) && (read_pos != startAddr))
			{
				flag = 0;
			#if DEBUG_TEST
				errorCount[2]++;
				printf("debug 2 debug 2 debug 2 errorCount = %d,startAddr = %d,read_pos = %d\r\n",errorCount[2],startAddr,read_pos);				
			#endif
			}
			read_pos = POS_OFFSET_ADD(read_pos,1,SOCKET_MEM_POOL_SIZE);
		}	
		usleep(1);
	}
}

unsigned short RGB888toRGB565(unsigned char red, unsigned char green, unsigned char blue)
{
    unsigned short  B = (blue >> 3) & 0x001F;
    unsigned short  G = ((green >> 2) << 5) & 0x07E0;
    unsigned short  R = ((red >> 3) << 11) & 0xF800;
    return (unsigned short) (R | G | B);
}

unsigned int RGB888toRGB32bit(unsigned char red, unsigned char green, unsigned char blue)
{
    unsigned int  A = 0xFF<<24;
    unsigned int  R = (red << 16) & 0x00FF0000;
    unsigned int  G = (green<< 8) & 0x0000FF00;
    unsigned int  B = (blue << 0) & 0x000000FF;
    
    
    return (unsigned int) (A | R | G | B);
}


/*
 * open framebuffer device.
 * return positive file descriptor if success,
 * else return -1.
 */

int fb_open(char *fb_device)
{
    int  fd;
    if ((fd = open(fb_device, O_RDWR)) < 0) 
    {
        perror(__func__);
        return (-1);
    }
    return (fd);
}

 

/*
 * get framebuffer's width,height,and depth.
 * return 0 if success, else return -1.
 */

int fb_stat(int fd, int *width, int *height, int *depth)
{
    struct fb_fix_screeninfo fb_finfo;
    struct fb_var_screeninfo fb_vinfo;

    if (ioctl(fd, FBIOGET_FSCREENINFO, &fb_finfo)) 
    {
        perror(__func__);
        return (-1);
    }

    if (ioctl(fd, FBIOGET_VSCREENINFO, &fb_vinfo)) 
    {
        perror(__func__);
        return (-1);
    }

    *width = fb_vinfo.xres;
    *height = fb_vinfo.yres;
    *depth = fb_vinfo.bits_per_pixel;

    return (0);
}

 

/*
 * map shared memory to framebuffer device.
 * return maped memory if success,
 * else return -1, as mmap dose.
 */

void* fb_mmap(int fd, unsigned int screensize)
{
    caddr_t   fbmem;
    if ((fbmem = mmap(0, screensize, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0)) == MAP_FAILED) 
    {
        perror(__func__);
        return (void *) (-1);
    }
    return (fbmem);
}

/*
 * unmap map memory for framebuffer device.
 */

int fb_munmap(void *start, size_t length)
{
    return (munmap(start, length));
}


/*
 * close framebuffer device
 */

int fb_close(int fd)
{
    return (close(fd));
}

/*
 * display a pixel on the framebuffer device.
 * fbmem is the starting memory of framebuffer,
 * width and height are dimension of framebuffer,
 * x and y are the coordinates to display,
 * color is the pixel's color value.
 * return 0 if success, otherwise return -1.
 */

int fb_pixel(void *fbmem, int width, int height,int x, int y, unsigned short color)
{
    if ((x > width) || (y > height))

    return (-1);

    unsigned short *dst = ((unsigned short *) fbmem + y * width + x);
    *dst = color;
    return (0);
}


int fb_pixel_32bit(void *fbmem, int width, int height,int x, int y, unsigned int color)
{
    if ((x > width) || (y > height))

    return (-1);

    unsigned int *dst = ((unsigned int *) fbmem + y * width + x);
    *dst = color;
    return (0);
}
void* jpegFrambuff(void* args)
{
	int res;
	unsigned char  *buffer;
	int             fbdev;
    char           *fb_device;
    unsigned char  *fbmem;
    unsigned int    screensize;
    unsigned int    fb_width;
    unsigned int    fb_height;
    unsigned int    fb_depth;
    unsigned int    x;
    unsigned int    y;

	if ((fb_device = getenv("FRAMEBUFFER")) == NULL)
            fb_device = FB_DEV;
    printf("fb_device: %s\r\n",fb_device);
    fbdev = fb_open(fb_device);
	fb_stat(fbdev, &fb_width, &fb_height, &fb_depth);
	screensize = fb_width * fb_height * fb_depth / 8;
    fbmem = fb_mmap(fbdev, screensize);
	buffer = (unsigned char *) malloc(3840*2160);
	if(buffer == NULL)
	{
		returnFlag = 1;
		printf("malloc libjpeg buffer faild\r\n");
	}

	while(1)
	{
		struct jpeg_decompress_struct cinfo;
    	struct jpeg_error_mgr jerr;
		sem_wait(&sem_jpeg);
		if(returnFlag)	break;
		

		pthread_mutex_lock(&mutex);				//上锁，保证刷屏过程不被抢占

		cinfo.err = jpeg_std_error(&jerr);
    	jpeg_create_decompress(&cinfo);
	//	jpeg_stdio_buffer_src (&cinfo, _jpegBuffer[cache_read_pos].buffer,_jpegBuffer[cache_read_pos].len);
		jpeg_mem_src (&cinfo,_jpegBuffer[cache_read_pos].buffer,_jpegBuffer[cache_read_pos].len);     //使用原版的内存解码接口

		cache_read_pos = POS_OFFSET_ADD(cache_read_pos,1,CACHE_FRAME_MAX);


		if(jpeg_read_header(&cinfo, TRUE) != 1)
		{
			printf("jpeg_read_header error !!!\r\n");
			continue;
		}
		if(jpeg_start_decompress(&cinfo) != 1)
		{
			printf("jpeg_start_decompress error !!!\r\n");
			continue;
		}

		if ((cinfo.output_width > fb_width) || (cinfo.output_height > fb_height)) 
		{
			printf("JPEG bad file,cannot display\r\n");
			pthread_mutex_unlock(&mutex);
			continue;
		}
		y = FB_START_Y;
		//printf("fb_width=%d,fb_height=%d,fb_depth=%d\r\n",fb_width,fb_height,fb_depth);
		//printf("cinfo_width=%d,cinfo_height=%d,fb_components=%d\r\n",cinfo.output_width,cinfo.output_height,cinfo.output_components);
		while (cinfo.output_scanline < cinfo.output_height) 
		{
			jpeg_read_scanlines(&cinfo, &buffer, 1);
			if (fb_depth == 16) 
			{
				unsigned short  color;
				for (x = 0; x < cinfo.output_width; x++) 
				{
					color = RGB888toRGB565(buffer[x * 3],buffer[x * 3 + 1], buffer[x * 3 + 2]);
					fb_pixel(fbmem, fb_width, fb_height, x+FB_START_X, y, color);
				}
			} 
			else if (fb_depth == 24)
			{
				memcpy((unsigned char *) fbmem + y * fb_width * 3,buffer, cinfo.output_width * cinfo.output_components);
			}
			else if (fb_depth == 32)
			{
				unsigned int  color;
				for (x = 0; x < cinfo.output_width; x++) 
				{
					color = RGB888toRGB32bit(buffer[x * 3],buffer[x * 3 + 1], buffer[x * 3 + 2]);
					fb_pixel_32bit(fbmem, fb_width, fb_height, x+FB_START_X, y, color);
				}            
			}
			y++;                                   // next scanline
		}
		jpeg_finish_decompress(&cinfo);
    	jpeg_destroy_decompress(&cinfo);

		pthread_mutex_unlock(&mutex);
		
		//usleep(1);
	}
	free(buffer);
	fb_munmap(fbmem, screensize);
	fb_close(fbdev);
}

/* 关闭自动息屏功能 */
int disableAutoCloseFb(void)
{
	int fd;
	fd = open("/dev/tty1", O_RDWR);
	write(fd, "\033[9;0]", 8);
	close(fd);
	return 0;
}


//argv[1]:IP地址
//argv[2]:端口号
int main(int argc,char *argv[])
{
	int i;
	pthread_t tid_recv,tid_analyse,tid_display;
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
    their_addr.sin_port = htons(atoi(argv[2]));				//要连接的服务器端口号为8000
    their_addr.sin_addr.s_addr=inet_addr(argv[1]);	//要连接的服务器的地址
    bzero(&(their_addr.sin_zero), 8);						//填充0以保持与sockaddr结构的长度相同
	//客户端自身的地址已经分配好，客户端的端口号系统会自动分配，不需要人为去分配

  
    while(connect(sockfd,(struct sockaddr*)&their_addr,sizeof(struct sockaddr)) == -1);//和服务器握手连接
    printf("Get the Server~Cheers!\n");
	
    numbytes = recv(sockfd, buf, 50,0);//F429会在握手成功后发来的第一句打印信息 
    buf[numbytes]='\0';  
    printf("%s",buf);
	disableAutoCloseFb();
	for(i=0;i<CACHE_FRAME_MAX;i++)
	{
		_jpegBuffer[i].buffer = (unsigned char*)malloc(JPEG_BUFF_SIZE);
		if(_jpegBuffer[i].buffer == NULL)
		{
			printf("malloc jpeg buff faild\r\n");
			return -1;
		}
	}
	sem_init(&sem_recv,0,0);
	sem_init(&sem_jpeg,0,0);
	pthread_create(&tid_display,NULL,jpegFrambuff,NULL);//创建一个线程，用以显示画面
	sleep(1);

	pthread_create(&tid_recv,NULL,socketReceive,(void*)&sockfd);//创建一个线程，用以接收数据
	pthread_create(&tid_analyse,NULL,analyse_socketData,NULL);//创建一个线程，用以处理数据
	

	
    pthread_join(tid_recv,NULL);
	sem_post(&sem_recv);		//最后发一次任务调度，结束数据处理线程	
	pthread_join(tid_analyse,NULL);
	sem_post(&sem_jpeg);		//最后发一次任务调度，结束数据处理线程	
	pthread_join(tid_display,NULL);


	for(i=0;i<CACHE_FRAME_MAX;i++)
	{
		free(_jpegBuffer[i].buffer);
	}
	sem_destroy(&sem_recv);
	sem_destroy(&sem_jpeg);
    close(sockfd);
	printf("close main pthread\r\n");
    return 0;
}







