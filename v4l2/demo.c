
#ifndef _V4L2_H
#define _V4L2_H
 
#define DEV_PATH "/dev/video0"
#define BMP      "./image.bmp"
#define YUYV     "./image.yuv"
 
#define TURE 1
#define FALSE 0
#define N_BUFFER 4
struct buffer{
	void* start;
	unsigned int length;
};
 
/*V4L2初始化*/
void v4l2_init();
 
/*V4L2关闭*/
void v4l2_close();
#endif




#include<stdio.h>
#include<sys/types.h>
#include<sys/stat.h>
#include<fcntl.h>
#include<unistd.h>
#include<sys/ioctl.h>
#include<stdlib.h>
#include<sys/mman.h>
#include<linux/types.h>
#include<linux/videodev2.h>
#include<string.h>
#include "V4l2.h"
#include "framebuffer.c"
 
static int fd;
struct buffer* buffers=NULL;
 
void v4l2_init(int width,int height){
	/*打开设备*/
	fd=open(DEV_PATH,O_RDWR);
	if(fd==-1)
		perror("open "),exit(-1);
	/*读取设备信息*/
	struct v4l2_capability cap;
	int res=ioctl(fd, VIDIOC_QUERYCAP, &cap);
	if(res==-1)
		printf("read dev information error\n"),exit(-1);
	else{
     	printf("driver:\t\t%s\n",cap.driver);
     	printf("card:\t\t%s\n",cap.card);
     	printf("bus_info:\t%s\n",cap.bus_info);
     	printf("version:\t%d\n",cap.version);
     	printf("capabilities:\t%x\n",cap.capabilities);
	}
 
	/*查询是否支持视频采集*/
 
	if ((cap.capabilities & V4L2_CAP_VIDEO_CAPTURE) == V4L2_CAP_VIDEO_CAPTURE)
		printf("Device %s: supports capture.\n",DEV_PATH);
	else
		printf("Device not supports capture\n"),exit(-1);
	if ((cap.capabilities & V4L2_CAP_STREAMING) == V4L2_CAP_STREAMING)
		printf("Device %s: supports streaming.\n",DEV_PATH);
	else
		printf("Device not supports streaming\n"),exit(-1);
	
	/*查询设备支持的图片输出格式*/
	struct v4l2_fmtdesc fmtdesc;
	fmtdesc.index=0;
	fmtdesc.type=V4L2_BUF_TYPE_VIDEO_CAPTURE;
	printf("Support format:\n");
	while(ioctl(fd,VIDIOC_ENUM_FMT,&fmtdesc)!=-1){
		printf("%d.%s\n",fmtdesc.index+1,fmtdesc.description);
	}
		fmtdesc.index++;
 
	/*设置帧格式,并检验*/
	struct v4l2_format format;
	format.type=V4L2_BUF_TYPE_VIDEO_CAPTURE;
	format.fmt.pix.pixelformat=V4L2_PIX_FMT_YUYV;
	format.fmt.pix.height=height;
	format.fmt.pix.width=width;
	format.fmt.pix.field=V4L2_FIELD_INTERLACED;
 
	res=ioctl(fd,VIDIOC_S_FMT,&format);
	if(res==-1)
		printf("set up frame foramt failed\n"),exit(-1);
 
	res=ioctl(fd,VIDIOC_G_FMT,&format);
	if(res==-1)
		printf("read frame format failed\n"),exit(-1);
	
	printf("type:\t\t%d\n",format.type);
    
	printf("pix.pixelformat:\t%c%c%c%c\n",format.fmt.pix.pixelformat & 0xFF, (format.fmt.pix.pixelformat >> 8) & 0xFF,(format.fmt.pix.pixelformat >> 16) & 0xFF, (format.fmt.pix.pixelformat >> 24) & 0xFF);
    
	printf("pix.height:\t\t%d\n",format.fmt.pix.height);
    printf("pix.width:\t\t%d\n",format.fmt.pix.width);
    printf("pix.field:\t\t%d\n",format.fmt.pix.field);
 
	/*申请缓冲区*/
	struct v4l2_requestbuffers req;
	struct v4l2_buffer buf;
 
	req.count=N_BUFFER;
	req.type=V4L2_BUF_TYPE_VIDEO_CAPTURE;
	req.memory=V4L2_MEMORY_MMAP;
	res=ioctl(fd,VIDIOC_REQBUFS,&req);
	if(res==-1)
		printf("memory request failed\n"),exit(-1);
	buffers=malloc(req.count*sizeof(struct buffer));
 
	if(!buffers)
		printf("out of memory!\n"),exit(-1);
	
	for(int i=0;i<req.count;++i){
		buf.type=V4L2_BUF_TYPE_VIDEO_CAPTURE;
		buf.memory=V4L2_MEMORY_MMAP;
		buf.index=i;
		res=ioctl(fd,VIDIOC_QUERYBUF,&buf);
		if(res==-1)
			printf("query buffer error\n"),exit(-1);
		buffers[i].length=buf.length;
		buffers[i].start=mmap(NULL,buf.length,PROT_READ|PROT_WRITE,MAP_SHARED,fd,buf.m.offset);
		if (buffers[i].start == MAP_FAILED)
			printf("buffer map error\n"),exit(-1);
	}
	//将所有缓冲区挂入输入队列
	for (int i = 0;i < req.count;++i){
		buf.index =i;
		ioctl(fd, VIDIOC_QBUF, &buf);
	}
	/*开始采集图像*/
	enum v4l2_buf_type type=V4L2_BUF_TYPE_VIDEO_CAPTURE;
	res=ioctl (fd, VIDIOC_STREAMON, &type);
	if(res==-1)
		printf("error to start\n");
	else
		printf("start to act!\n");
}
 
void v4l2_close(void){
	if(fd == -1)
		printf("close failed\n"),exit(-1);
	enum v4l2_buf_type type=V4L2_BUF_TYPE_VIDEO_CAPTURE;
	
	/*关闭视频采集*/
	int res=ioctl(fd,VIDIOC_STREAMOFF,&type);
	if(res==-1)
		printf("close stream failed\n"),exit(-1);
	else
		printf("success to close stream\n");
	/*解除内存映射*/
	for(int i=0;i<N_BUFFER;i++){
		if(munmap(buffers[i].start,buffers[i].length)==(void*)(-1))
			printf("error to munmap\n"),exit(-1);
	}
	printf("success to munmap!\n");
	/*释放内存*/
	free(buffers);
	printf("success to free!\n");
	/*关闭文件描述符*/
	close(fd);
	printf("v4l2 success to close\n");	
}
 
struct buffer* v4l2_get(){//VIDIOC_DQBUF有阻塞功能，无需等待
/*	struct timeval tv;
	fd_set fds;
	FD_ZERO(&fds);
	FD_SET(fd,&fds);
	tv.tv_sec=2;
	tv.tv_usec=0;
	int res=select(fd+1,&fds,NULL,NULL,&tv);
	if(res==-1)
		printf("select error\n"),exit(-1);
	else if(res==0)
		printf("timeout\n"),exit(-1);
*/
	struct v4l2_buffer buf;
	struct buffer* buffer1=NULL;
	memset(&buf,0,sizeof(buf));
	buf.type=V4L2_BUF_TYPE_VIDEO_CAPTURE;
	buf.memory=V4L2_MEMORY_MMAP;
	int res=ioctl(fd,VIDIOC_DQBUF,&buf);
	if(res==-1){
		printf("get error!\n");
		v4l2_close();
		exit(-1);
	}
	buffer1=&buffers[buf.index];
	/*重新放入输入队列*/
	res=ioctl(fd,VIDIOC_QBUF,&buf);
	if(res!=0)
		printf("error to put in input quene\n"),exit(-1);
	return buffer1;
}
 
extern void* framebuffer;
extern int framex;
void yuyv_to_rgb24(unsigned char* yuv,int length,unsigned char *rgb){
	unsigned int i;
	unsigned int i1=0,j1=0;
	unsigned char* y0=yuv+0;
	unsigned char* u0=yuv+1;
	unsigned char* y1=yuv+2;
	unsigned char* v0=yuv+3;
 
	unsigned char* r0=rgb+0;
	unsigned char* g0=rgb+1;
	unsigned char* b0=rgb+2;
	unsigned char* r1=rgb+3;
	unsigned char* g1=rgb+4;
	unsigned char* b1=rgb+5;
	rgb32_frame* frame=(rgb32_frame*)framebuffer;
 
	double rt0=0,gt0=0,bt0=0,rt1=0,gt1=0,bt1=0;
	for(i=0;i<=(length/4);i++){
		bt0=1.164*(*y0-16)+2.017*(*u0-128);
		gt0=1.164*(*y0-16)-0.813*(*v0-128)-0.392*(*u0-128);
		rt0=1.164*(*y0-16)+1.596*(*v0-128);
 
		bt1=1.164*(*y1-16)+2.018*(*u0-128);
		gt1=1.164*(*y1-16)-0.813*(*v0-128)-0.392*(*u0-128);
		rt1=1.164*(*y1-16)+1.596*(*v0-128);
 
		if(rt0>255) rt0=255;
		else if(rt0<0) rt0=0;
	
		if(gt0>255) gt0=255;
		else if(gt0<0) gt0=0;
		
		if(bt0>255) bt0=255;
		else if(bt0<0) bt0=0;
		
		if(rt1>255) rt1=255;
		else if(rt1<0) rt1=0;
		
		if(gt1>255) gt1=255;
		else if(gt1<0) gt1=0;
		
		if(bt1>255) bt1=255;
		else if(bt1<0) bt1=0;
		*r0=(unsigned char)rt0;
		*g0=(unsigned char)gt0;
		*b0=(unsigned char)bt0;
		*r1=(unsigned char)rt1;
		*g1=(unsigned char)gt1;
		*b1=(unsigned char)bt1;
		
		frame[i1+j1].b=*r0;		
		frame[i1+j1].g=*g0;
		frame[i1+j1].r=*b0;
		frame[i1+j1+1].b=*r1;
		frame[i1+j1+1].g=*g1;
		frame[i1+j1+1].r=*b1;
		j1+=2;
		if(i%320==0){
			j1=0;
			i1+=framex;
		}
		yuv=yuv+4;
		rgb=rgb+6;
 
		if(yuv==NULL)
			break;
 
		y0=yuv;
		u0=yuv+1;
		y1=yuv+2;
		v0=yuv+3;
 
		r0=rgb+0;
		g0=rgb+1;
		b0=rgb+2;
		r1=rgb+3;
		g1=rgb+4;
		b1=rgb+5;
	}
}
 
void yuyv_to_rgb32(unsigned char* yuv,int length,unsigned char *rgb){
	unsigned int i;
	unsigned char* y0=yuv+0;
	unsigned char* u0=yuv+1;
	unsigned char* y1=yuv+2;
	unsigned char* v0=yuv+3;
 
	unsigned char* r0=rgb+0;
	unsigned char* g0=rgb+1;
	unsigned char* b0=rgb+2;
	unsigned char* r1=rgb+4;
	unsigned char* g1=rgb+5;
	unsigned char* b1=rgb+6;
 
	double rt0=0,gt0=0,bt0=0,rt1=0,gt1=0,bt1=0;
	for(i=0;i<=(length/4);i++){
		bt0=1.164*(*y0-16)+2.017*(*u0-128);
		gt0=1.164*(*y0-16)-0.813*(*v0-128)-0.392*(*u0-128);
		rt0=1.164*(*y0-16)+1.596*(*v0-128);
 
		bt1=1.164*(*y1-16)+2.017*(*u0-128);
		gt1=1.164*(*y1-16)-0.813*(*v0-128)-0.392*(*u0-128);
		rt1=1.164*(*y1-16)+1.596*(*v0-128);
 
		if(rt0>255) rt0=255;
		else if(rt0<0) rt0=0;
	
		if(gt0>255) gt0=255;
		else if(gt0<0) gt0=0;
		
		if(bt0>255) bt0=255;
		else if(bt0<0) bt0=0;
		
		if(rt1>255) rt1=255;
		else if(rt1<0) rt1=0;
		
		if(gt1>255) gt1=255;
		else if(gt1<0) gt1=0;
		
		if(bt1>255) bt1=255;
		else if(bt1<0) bt1=0;
	
		*r0=(unsigned char)rt0;
		*g0=(unsigned char)gt0;
		*b0=(unsigned char)bt0;
		*r1=(unsigned char)rt1;
		*g1=(unsigned char)gt1;
		*b1=(unsigned char)bt1;
		
		yuv=yuv+4;
		rgb=rgb+8;
 
		if(yuv==NULL)
			break;
 
		y0=yuv;
		u0=yuv+1;
		y1=yuv+2;
		v0=yuv+3;
 
		r0=rgb+0;
		g0=rgb+1;
		b0=rgb+2;
		r1=rgb+4;
		g1=rgb+5;
		b1=rgb+6;
	}
}
 
int main(){
	struct buffer* yuv=NULL;
	v4l2_init(640,480);
	framebuffer_init();
	void * pic=malloc(640*480*3);	
	while(1){
		yuv=v4l2_get();
		yuyv_to_rgb24(yuv->start,yuv->length,(unsigned char*)pic);
	}
	framebuffer_close();
	v4l2_close();
}




#ifndef _FRAMEBUFFER_H
#define _FRAMEBUFFER_H
 
typedef struct{
	unsigned char r;
	unsigned char g;
	unsigned char b;
	unsigned char rgbReserved;
}rgb32;
 
typedef struct{
	unsigned char r;
	unsigned char g;
	unsigned char b;
	unsigned char rgbReserved;
}rgb32_frame;
 
typedef struct{
	unsigned char r;
	unsigned char g;
	unsigned char b;
}rgb24;
 
typedef struct{
	unsigned char r;
	unsigned char g;
	unsigned char b;
}rgb24_frame;
 
void framebuffer_init(void);
 
void framebuffer_write(void* img_buf,unsigned int img_width,unsigned int img_height,unsigned int img_bits);
 
void framebuffer_close(void);
 
 
#endif



#include "framebuffer.h"
#include<unistd.h>
#include<fcntl.h>
#include<linux/fb.h>
#include<linux/kd.h>
#include<sys/mman.h>
#include<sys/ioctl.h>
#include<stdlib.h>
#include<stdio.h>
 
static int frame_fd=-1;
void* framebuffer=NULL;
static int screensize=0;
int framex=0;
 
void framebuffer_init(void){ 
    struct fb_var_screeninfo vinfo;
    struct fb_fix_screeninfo finfo;
    frame_fd = open("/dev/fb0" , O_RDWR);  
    if(frame_fd==-1)  
        perror("open frame buffer fail"),exit(-1);
 
    // Get fixed screen information
    if (ioctl(frame_fd, FBIOGET_FSCREENINFO, &finfo))
        printf("Error reading fixed information.\n"),exit(-1);
   
	 // Get variable screen information
    if (ioctl(frame_fd, FBIOGET_VSCREENINFO, &vinfo)) 
        printf("Error reading variable information.\n"),exit(-1);
    //这里把整个显存一起初始化（xres_virtual 表示显存的x，比实际的xres大,bits_per_pixel位深）
   
	screensize = vinfo.xres_virtual * vinfo.yres_virtual * vinfo.bits_per_pixel / 8;
    framex=vinfo.xres_virtual;
	//获取实际的位色，这里很关键，后面转换和填写的时候需要
    int framebpp = vinfo.bits_per_pixel;
    printf("%dx%d, %d bpp  screensize is %ld\n", vinfo.xres_virtual, vinfo.yres_virtual, vinfo.bits_per_pixel,screensize);
    
    //映射出来，用户直接操作
    framebuffer = mmap(0, screensize, PROT_READ | PROT_WRITE , MAP_SHARED ,frame_fd ,0 );  
    if(framebuffer == (void *)-1)  
        perror("memory map fail"),exit(-1);    
}
 
 
 
//写入framebuffer   img_buf:采集到的图片首地址  width：用户的宽 height：用户的高  bits：图片的位深 
void framebuffer_write(void *img_buf, unsigned int img_width, unsigned int img_height, unsigned int img_bits){   
    int row, column;
    int num = 0;        //img_buf 中的某个像素点元素的下标
    rgb32_frame *rgb32_fbp = (rgb32_frame *)framebuffer;
    rgb24 *rgb24_img_buf = (rgb24 *)img_buf;    
    
    //防止摄像头采集宽高比显存大
    if(screensize < img_width * img_height * img_bits / 8){
        printf("the imgsize is too large\n"),exit(-1);
    }
    /*不同的位深度图片使用不同的显示方案*/
    switch (img_bits){
        case 24:
            for(row = 0; row < img_height; row++){
                    for(column = 0; column < img_width; column++){
                        //由于摄像头分辨率没有帧缓冲大，完成显示后，需要强制换行，帧缓冲是线性的，使用row * vinfo.xres_virtual换行
                        rgb32_fbp[row * framex + column].r = rgb24_img_buf[num].b;
                        rgb32_fbp[row * framex + column].g = rgb24_img_buf[num].g;
                        rgb32_fbp[row * framex + column].b = rgb24_img_buf[num].r;
                        num++;
                    }        
                }    
            break;
        default:
            break;
    }
}
 
void framebuffer_close(void){
    munmap(framebuffer,screensize);  
	if(frame_fd==-1)
		perror("file don't exit\n"),exit(-1);
    close(frame_fd);  
}




