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


#define SOCKET_MEM_POOL_SIZE		(1024*1024*4)		//申请4M的内存空间
#define SOCKET_CELL_SIZE	1024
#define JPEG_BUFF_SIZE		(1024*500)			//申请500K内存用于存储要显示的jpeg数据

#define CACHE_FRAME_MAX		20
#define CACHE_FRAME_MIN		5

#define FB_START_X			600
#define FB_START_Y			0

#define    FB_DEV  "/dev/fb0"

char returnFlag = 0;

unsigned char* socketMemPool = NULL;
unsigned char* jpegBuffer = NULL;
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

unsigned int getVaidFrame(void)
{
	if(cache_write_pos >= cache_read_pos)	return cache_write_pos-cache_read_pos;
	else
	{
		return CACHE_FRAME_MAX-cache_read_pos+cache_write_pos;
	}
}




void* socketReceive(void* args)
{
	int sockfd = *((int*)args);
	int numbytes;
	
	
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
	returnFlag = 1;
	sem_post(&sem_recv);		//最后发一次任务调度，结束数据处理线程	
}


void* analyse_socketData(void* args)
{
	unsigned int byteNum=0,i;
	char flag = 0;
	while(1)
	{
		sem_wait(&sem_recv);
		if(returnFlag)	break;
		byteNum = getVaidRange();
		for(i=0;i<byteNum;i++)
		{
			if(socketMemPool[read_pos] == 0xFF)
			{
				if((flag==0) && (socketMemPool[read_pos+1] == 0xD8))
				{
					cacheFrame_start[cache_write_pos] = read_pos;
					flag = 1;
				//	printf("aaaaaaaa  read_pos=%d\r\n",read_pos);
				}
				else if((flag==1) && (socketMemPool[read_pos+1] == 0xD9))
				{
					cacheFrame_end[cache_write_pos] = read_pos+1;
					flag = 0;
				//	printf("bbbbbbbbbbb  read_pos=%d\r\n",read_pos+1);
					cache_write_pos++;
					if(cache_write_pos >= CACHE_FRAME_MAX)	cache_write_pos=0;
				}
			}
			read_pos++;
			if(read_pos >= SOCKET_MEM_POOL_SIZE)	read_pos = 0;
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
	unsigned int delayTime = 0,frameNum=0,timeNum=0;

	if ((fb_device = getenv("FRAMEBUFFER")) == NULL)
            fb_device = FB_DEV;
    printf("fb_device: %s\r\n",fb_device);
    fbdev = fb_open(fb_device);
	fb_stat(fbdev, &fb_width, &fb_height, &fb_depth);
	screensize = fb_width * fb_height * fb_depth / 8;
    fbmem = fb_mmap(fbdev, screensize);
	jpegBuffer = (unsigned char *) malloc(JPEG_BUFF_SIZE);
	buffer = (unsigned char *) malloc(3840*2160);

	while(1)
	{
		struct jpeg_decompress_struct cinfo;
    	struct jpeg_error_mgr jerr;
		unsigned int startAddr,endAddr,len;
		if(returnFlag)	break;
		frameNum = getVaidFrame();
		if(frameNum == 0)
		{
			delayTime = 1000;	//10ms
			goto sleepProcess;
		}
		startAddr = cacheFrame_start[cache_read_pos];
		endAddr = cacheFrame_end[cache_read_pos];
		if(startAddr <= endAddr)
		{
			len = endAddr - startAddr +1;
			memcpy(jpegBuffer,&socketMemPool[startAddr],len);
		}			
		else
		{
			len = SOCKET_MEM_POOL_SIZE - startAddr + endAddr + 1;
			memcpy(jpegBuffer,&socketMemPool[startAddr],SOCKET_MEM_POOL_SIZE - startAddr);
			memcpy(&jpegBuffer[SOCKET_MEM_POOL_SIZE - startAddr],&socketMemPool[0],endAddr + 1);
		}
			
		
		cache_read_pos++;
		if(cache_read_pos >= CACHE_FRAME_MAX)	cache_read_pos = 0;
		
	//	printf("len=%d timeNum=%d\r\n",len,timeNum++);


		cinfo.err = jpeg_std_error(&jerr);
    	jpeg_create_decompress(&cinfo);
		jpeg_stdio_buffer_src (&cinfo, jpegBuffer, len);
		jpeg_read_header(&cinfo, TRUE);
		jpeg_start_decompress(&cinfo);
		if ((cinfo.output_width > fb_width) || (cinfo.output_height > fb_height)) 
		{
			printf("too large JPEG file,cannot display/n");
			break;
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
		

		continue;

		sleepProcess:
			usleep(delayTime);
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

	sem_init(&sem_recv,0,0);

	pthread_create(&tid_recv,NULL,socketReceive,(void*)&sockfd);//创建一个线程，用以接收数据
	pthread_create(&tid_analyse,NULL,analyse_socketData,NULL);//创建一个线程，用以处理数据
	pthread_create(&tid_display,NULL,jpegFrambuff,NULL);//创建一个线程，用以显示画面

	
    pthread_join(tid_recv,NULL);
	pthread_join(tid_analyse,NULL);
	pthread_join(tid_display,NULL);

	sem_destroy(&sem_recv);
    close(sockfd);
	printf("close main pthread\r\n");
    return 0;
}







