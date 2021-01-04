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


typedef enum
{
	FB_16B_RGB565 = 0,
	FB_24B_RGB888,
	FB_32B_RGB8888,
}fb_pixelBit;

#define    FB_DEV  "/dev/fb0"

unsigned short RGB888toRGB565(unsigned char red, unsigned char green, unsigned char blue);
unsigned int RGB888toRGB32bit(unsigned char red, unsigned char green, unsigned char blue);
int fb_pixel(void *fbmem, int width, int height,int x, int y, unsigned short color);
int fb_pixel_32bit(void *fbmem, int width, int height,int x, int y, unsigned int color);
void init_fb(fb_info_t* dev);
void close_fb(fb_info_t* dev);


void mjpeg_to_rgbxxx(  unsigned char* mjpegBuffer,
                        int length,
                        unsigned char  *framebuf,
                        int width, int height,
                        int x, int y,
                        fb_pixelBit rgbFormat);

void yuyv_to_rgbxxx( unsigned char* yuvBuffer,
                    int length,
                    unsigned char  *framebuf,
                    int width, int height,
                    int x, int y,
                    int displaySizeX,
                    fb_pixelBit rgbFormat);

void rgb888_to_rgbxxx(  unsigned char* rgbBuffer,
                        int length,
                        unsigned char  *framebuf,
                        int width, int height,
                        int x, int y,
                        int displaySizeX,
                        fb_pixelBit rgbFormat);              

#endif


