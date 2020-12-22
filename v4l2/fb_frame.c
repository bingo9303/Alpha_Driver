#include "fb_frame.h"
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <fcntl.h>
#include <linux/fb.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include "debugTime.h"


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

static int fb_open(char *fb_device)
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

static int fb_stat(int fd, int *width, int *height, int *depth)
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

    printf("fb info : width=%d,height=%d,depth=%d\r\n",*width,*height,*depth);


    return (0);
}

 

/*
 * map shared memory to framebuffer device.
 * return maped memory if success,
 * else return -1, as mmap dose.
 */

static void* fb_mmap(int fd, unsigned int screensize)
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

static int fb_munmap(void *start, size_t length)
{
    return (munmap(start, length));
}


/*
 * close framebuffer device
 */

static int fb_close(int fd)
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


/*
*   yuvBuffer:要转换的YUV格式数据起始地址
*   int length:YUV的数据长度
*   rgbBuffer:要存入的RGB格式数据的起始地址
*   framebuf:fb的内存
*   width:fb的X方向的总大小
*   height:fb的Y方向的总大小
*   x:要显示的区域起始X位置
*   y:要显示的区域起始Y位置
*   displaySizeX:YUV数据所显示的图像X方向大小
*/

void yuyv_to_rgb16_rgb565( unsigned char* yuvBuffer,
                    int length,
                    unsigned char  *framebuf,
                    int width, int height,
                    int x, int y,
                    int displaySizeX)
{
	unsigned int i;
    unsigned int x_offset=0;
	unsigned int y_offset=0;
	unsigned char* y0=yuvBuffer+0;
	unsigned char* u0=yuvBuffer+1;
	unsigned char* y1=yuvBuffer+2;
	unsigned char* v0=yuvBuffer+3;
    unsigned short color_16bit;

	//double rt0=0,gt0=0,bt0=0,rt1=0,gt1=0,bt1=0;
    int rt0=0,gt0=0,bt0=0,rt1=0,gt1=0,bt1=0;
    int y_temp,u, v;
	for(i=0;i<=(length/4);i++)
    {
        /*
        bt0=1.164*(*y0-16)+2.017*(*u0-128);
		gt0=1.164*(*y0-16)-0.813*(*v0-128)-0.392*(*u0-128);
		rt0=1.164*(*y0-16)+1.596*(*v0-128);
 
		bt1=1.164*(*y1-16)+2.018*(*u0-128);
		gt1=1.164*(*y1-16)-0.813*(*v0-128)-0.392*(*u0-128);
		rt1=1.164*(*y1-16)+1.596*(*v0-128);
*/
        u = *u0 - 128;
        v = *v0 - 128;  
        y_temp = *y0 << 8;
        rt0 = (y_temp + (359 * v)) >> 8;
        gt0 = (y_temp - (88 * u) - (183 * v)) >> 8;
        bt0 = (y_temp + (454 * u)) >> 8;

        y_temp = *y1 << 8;
        rt1 = (y_temp + (359 * v)) >> 8;
        gt1 = (y_temp - (88 * u) - (183 * v)) >> 8;
        bt1 = (y_temp + (454 * u)) >> 8;

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

        color_16bit = RGB888toRGB565(rt0, gt0, bt0);
        fb_pixel(framebuf, width, height,(x+x_offset+0), (y+y_offset),color_16bit);

        color_16bit = RGB888toRGB565(rt1, gt1, bt1);
        fb_pixel(framebuf,width,height, (x+x_offset+1), (y+y_offset), color_16bit);

        x_offset += 2;

         //1组YUV数据对应2个RGB像素点
		if(((i % (displaySizeX/2)) == 0) && (i != 0))
        {
			x_offset = 0;       //回到X的起始位置
			y_offset += 1;      //换行
		}
		
		yuvBuffer=yuvBuffer+4;

		if(yuvBuffer==NULL)
			break;
 
		y0=yuvBuffer;
		u0=yuvBuffer+1;
		y1=yuvBuffer+2;
		v0=yuvBuffer+3;
	}
}

/*
*   yuvBuffer:要转换的YUV格式数据起始地址
*   int length:YUV的数据长度
*   rgbBuffer:要存入的RGB格式数据的起始地址
*   framebuf:fb的内存
*   width:fb的X方向的总大小
*   height:fb的Y方向的总大小
*   x:要显示的区域起始X位置
*   y:要显示的区域起始Y位置
*   displaySizeX:YUV数据所显示的图像X方向大小
*/

void yuyv_to_rgb24( unsigned char* yuvBuffer,
                    int length,
                    unsigned char  *framebuf,
                    int width, int height,
                    int x, int y,
                    int displaySizeX)
{
	unsigned int i;
    unsigned int x_offset=0;
	unsigned int y_offset=0;
	unsigned char* y0=yuvBuffer+0;
	unsigned char* u0=yuvBuffer+1;
	unsigned char* y1=yuvBuffer+2;
	unsigned char* v0=yuvBuffer+3;

	double rt0=0,gt0=0,bt0=0,rt1=0,gt1=0,bt1=0;
	for(i=0;i<=(length/4);i++)
    {
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

        /* 设备是32bit的，以下代码没有调试过 */

        *(framebuf + (y+y_offset)*width +(x+x_offset+0)) = (unsigned char)bt0;
        *(framebuf + (y+y_offset)*width +(x+x_offset+1)) = (unsigned char)gt0;
        *(framebuf + (y+y_offset)*width +(x+x_offset+2)) = (unsigned char)rt0;

        *(framebuf + (y+y_offset)*width +(x+x_offset+3)) = (unsigned char)bt1;
        *(framebuf + (y+y_offset)*width +(x+x_offset+4)) = (unsigned char)gt1;
        *(framebuf + (y+y_offset)*width +(x+x_offset+5)) = (unsigned char)rt1;
		x_offset += 6;

        //1组YUV数据对应2个RGB像素点
		if(((i % (displaySizeX/2)) == 0) && (i != 0))
        {
			x_offset = 0;       //回到X的起始位置
			y_offset += 1;  //换行
		}
		yuvBuffer=yuvBuffer+4;
		if(yuvBuffer==NULL)
			break;
 
		y0=yuvBuffer;
		u0=yuvBuffer+1;
		y1=yuvBuffer+2;
		v0=yuvBuffer+3;
	}
}
 
void yuyv_to_rgb32( unsigned char* yuvBuffer,
                    int length,
                    unsigned char  *framebuf,
                    int width, int height,
                    int x, int y,
                    int displaySizeX)
{
	unsigned int i;
    unsigned int x_offset=0;
	unsigned int y_offset=0;
    unsigned int color_32bit;
	unsigned char* y0=yuvBuffer+0;
	unsigned char* u0=yuvBuffer+1;
	unsigned char* y1=yuvBuffer+2;
	unsigned char* v0=yuvBuffer+3;
 
    double rt0=0,gt0=0,bt0=0,rt1=0,gt1=0,bt1=0;

     struct timeval tv[5];
    

	for(i=0;i<=(length/4);i++)
    {
        {
         //   gettimeofday(&tv[0],NULL);
        }

		bt0=1.164*(*y0-16)+2.017*(*u0-128);
		gt0=1.164*(*y0-16)-0.813*(*v0-128)-0.392*(*u0-128);
		rt0=1.164*(*y0-16)+1.596*(*v0-128);
 
		bt1=1.164*(*y1-16)+2.017*(*u0-128);
		gt1=1.164*(*y1-16)-0.813*(*v0-128)-0.392*(*u0-128);
		rt1=1.164*(*y1-16)+1.596*(*v0-128);


        {
        //    gettimeofday(&tv[1],NULL);
        }
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

        {
        //    gettimeofday(&tv[2],NULL);
        }
#if 0
        *(framebuf + (y+y_offset)*width +(x+x_offset+0)) = (unsigned char)bt0;
        *(framebuf + (y+y_offset)*width +(x+x_offset+1)) = (unsigned char)gt0;
        *(framebuf + (y+y_offset)*width +(x+x_offset+2)) = (unsigned char)rt0;

        *(framebuf + (y+y_offset)*width +(x+x_offset+4)) = (unsigned char)bt1;
        *(framebuf + (y+y_offset)*width +(x+x_offset+5)) = (unsigned char)gt1;
        *(framebuf + (y+y_offset)*width +(x+x_offset+6)) = (unsigned char)rt1;

        {
        //    gettimeofday(&tv[3],NULL);
        }
        x_offset += 8;
         //1组YUV数据对应2个RGB像素点
		if(((i % (displaySizeX/2)) == 0) && (i != 0))
        {
			x_offset = 0;       //回到X的起始位置
			y_offset += 1;      //换行
		}
#endif
        color_32bit = RGB888toRGB32bit(rt0, gt0, bt0);
        fb_pixel_32bit(framebuf,width,height, (x+x_offset+0), (y+y_offset), color_32bit);

        color_32bit = RGB888toRGB32bit(rt1, gt1, bt1);
        fb_pixel_32bit(framebuf,width,height, (x+x_offset+1), (y+y_offset), color_32bit);

        x_offset += 2;

         //1组YUV数据对应2个RGB像素点
		if(((i % (displaySizeX/2)) == 0) && (i != 0))
        {
			x_offset = 0;       //回到X的起始位置
			y_offset += 1;      //换行
		}
		
		yuvBuffer=yuvBuffer+4;
 
		if(yuvBuffer==NULL)
			break;
 
		y0=yuvBuffer;
		u0=yuvBuffer+1;
		y1=yuvBuffer+2;
		v0=yuvBuffer+3;
        {
        //    gettimeofday(&tv[4],NULL);
            
        //     printf("%ld,%ld,%ld,%ld,%ld\n",tv[0].tv_usec,tv[1].tv_usec,tv[2].tv_usec,tv[3].tv_usec,tv[4].tv_usec);  //微秒
        }
	}
}

void init_fb(fb_info_t* dev)
{   
    if(dev->fb_device == NULL)
    {        
        if ((dev->fb_device = getenv("FRAMEBUFFER")) == NULL)
        {
            dev->fb_device = FB_DEV;    
        }
    }	
    printf("fb_device: %s\r\n",dev->fb_device);
    dev->fbdev = fb_open(dev->fb_device);
	fb_stat(dev->fbdev, &dev->fb_width, &dev->fb_height, &dev->fb_depth);
	dev->screensize = dev->fb_width * dev->fb_height * dev->fb_depth / 8;
    dev->framebuff = fb_mmap(dev->fbdev, dev->screensize);
}

void close_fb(fb_info_t* dev)
{
	fb_munmap(dev->framebuff, dev->screensize);
	fb_close(dev->fbdev);
}