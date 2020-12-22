#ifndef _FB_FRAME_H
#define _FB_FRAME_H
typedef struct 
{
	int             fbdev;
    char            *fb_device;
    unsigned int    screensize;
    unsigned int    fb_width;
    unsigned int    fb_height;
    unsigned int    fb_depth;
    unsigned char  *framebuff;
}fb_info_t;

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
}rgb24_frame;

#define    FB_DEV  "/dev/fb0"

unsigned short RGB888toRGB565(unsigned char red, unsigned char green, unsigned char blue);
unsigned int RGB888toRGB32bit(unsigned char red, unsigned char green, unsigned char blue);
int fb_pixel(void *fbmem, int width, int height,int x, int y, unsigned short color);
int fb_pixel_32bit(void *fbmem, int width, int height,int x, int y, unsigned int color);
void init_fb(fb_info_t* dev);
void close_fb(fb_info_t* dev);
void yuyv_to_rgb16_rgb565( unsigned char* yuvBuffer,
                    int length,
                    unsigned char  *framebuf,
                    int width, int height,
                    int x, int y,
                    int displaySizeX);
void yuyv_to_rgb24( unsigned char* yuvBuffer,
                    int length,
                    unsigned char  *framebuf,
                    int width, int height,
                    int x, int y,
                    int displaySizeX);
void yuyv_to_rgb32( unsigned char* yuvBuffer,
                    int length,
                    unsigned char  *framebuf,
                    int width, int height,
                    int x, int y,
                    int displaySizeX);                    

#endif


