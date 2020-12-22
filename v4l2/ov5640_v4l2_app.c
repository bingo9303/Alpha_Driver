#include <stdio.h>   
#include <stdlib.h>   
#include <string.h>   
#include <assert.h>   
#include <getopt.h>             /* getopt_long() */     
#include <fcntl.h>              /* low-level i/o */   
#include <unistd.h>   
#include <errno.h>   
#include <sys/stat.h>   
#include <sys/types.h>   
#include <sys/time.h>   
#include <sys/mman.h>   
#include <sys/ioctl.h>     
#include <asm/types.h>          /* for videodev2.h */     
#include <linux/videodev2.h>   
#include "fb_frame.h"
#include "debugTime.h"
  
#define CLEAR(x) memset (&(x), 0, sizeof (x))   
  
#define DISPLAY_SIZE_X  640
#define DISPLAY_SIZE_Y  480

typedef enum {  
    IO_METHOD_READ,  
    IO_METHOD_MMAP,  
    IO_METHOD_USERPTR,  
} io_method;  
  
struct buffer {  
        void *                  start;  
        size_t                  length;  
};  
  
static char *           dev_name        = NULL;  
static io_method    io      = IO_METHOD_MMAP;  
static int              fd              = -1;  
struct buffer *         buffers         = NULL;  
static unsigned int     n_buffers       = 0;  
static fb_info_t        fb_info;

  
static void  errno_exit  (const char *s)  
{  
    fprintf (stderr, "%s error %d, %s\n",  
                s, errno, strerror (errno));  
    exit (EXIT_FAILURE);  
}  
  
static int  
xioctl                          (int                    fd,  
                                 int                    request,  
                                 void *                 arg)  
{  
        int r;  
  
        do r = ioctl (fd, request, arg);  
        while (-1 == r && EINTR == errno);  
  
        return r;  
}  
  


static void  process_image(const void *p)  
{  
    struct buffer * buffer = (struct buffer * )p;
    char strbuff[50];

  //  printf("111..%ld\r\n",GetTime_Ms());
   
    if(fb_info.fb_depth == 32)
    {
        yuyv_to_rgb32(  buffer->start,
                        buffer->length,
                        fb_info.framebuff,
                        fb_info.fb_width, 
                        fb_info.fb_height,
                        0, 0,
                        DISPLAY_SIZE_X); 
    }
    else if(fb_info.fb_depth == 24)
    {
        yuyv_to_rgb24(  buffer->start,
                        buffer->length,
                        fb_info.framebuff,
                        fb_info.fb_width, 
                        fb_info.fb_height,
                        0, 0,
                        DISPLAY_SIZE_X); 

    }
    else if(fb_info.fb_depth == 16)
    {
        yuyv_to_rgb16_rgb565(   buffer->start,
                                buffer->length,
                                fb_info.framebuff,
                                fb_info.fb_width, 
                                fb_info.fb_height,
                                0, 0,
                                DISPLAY_SIZE_X);
    }    
 //   printf("222..%ld\r\n",GetTime_Ms());
/*
    fputc ('.', stdout);  
    fflush (stdout);  
    */
}  
  
static int  read_frame(void)  
{  
    struct v4l2_buffer buf;  
    unsigned int i;  
  
    switch (io) 
    {  
        case IO_METHOD_READ:  
            if (-1 == read (fd, buffers[0].start, buffers[0].length)) 
            {  
                switch (errno) 
                {  
                    case EAGAIN:  
                            return 0;  
                    case EIO:  
                        /* Could ignore EIO, see spec. */  
                        /* fall through */  
                    default:  
                        errno_exit ("read");  
                }  
            }  
            process_image (buffers[0].start);  
            break;  
  
        case IO_METHOD_MMAP:  
            CLEAR (buf);  
            buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;  
            buf.memory = V4L2_MEMORY_MMAP;  

            if (-1 == xioctl (fd, VIDIOC_DQBUF, &buf)) 
            {  
                switch (errno) 
                {  
                    case EAGAIN:  
                        return 0;  

                    case EIO:  
                        /* Could ignore EIO, see spec. */  

                        /* fall through */  

                    default:  
                        errno_exit ("VIDIOC_DQBUF");  
                }  
            }  
            assert (buf.index < n_buffers);   
            process_image (&buffers[buf.index]);  
            if (-1 == xioctl (fd, VIDIOC_QBUF, &buf))  
                errno_exit ("VIDIOC_QBUF");  
    
            break;  
        case IO_METHOD_USERPTR:  
            CLEAR (buf);  
            buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;  
            buf.memory = V4L2_MEMORY_USERPTR;  
            if (-1 == xioctl (fd, VIDIOC_DQBUF, &buf)) 
            {  
                switch (errno) 
                {  
                    case EAGAIN:  
                        return 0;  
                    case EIO:  
                        /* Could ignore EIO, see spec. */  
                        /* fall through */  
                    default:  
                        errno_exit ("VIDIOC_DQBUF");  
                }  
            }  
            for (i = 0; i < n_buffers; ++i)  
                if (buf.m.userptr == (unsigned long) buffers[i].start  
                    && buf.length == buffers[i].length)  
                    break;  
            assert (i < n_buffers);  
            process_image ((void *) buf.m.userptr);  
            if (-1 == xioctl (fd, VIDIOC_QBUF, &buf))  
                errno_exit ("VIDIOC_QBUF");  
            break;  
    }  
    return 1;  
}  
  
static void  mainloop(void)  
{  
    unsigned int count;  
    count = 100000;  
  
    while (count-- > 0) 
    {  
        for (;;) 
        {  
            fd_set fds;  
            struct timeval tv;  
            int r;  

            FD_ZERO (&fds);  
            FD_SET (fd, &fds);  

            /* Timeout. */  
            tv.tv_sec = 2;  
            tv.tv_usec = 0;  

            r = select (fd + 1, &fds, NULL, NULL, &tv);  

            if (-1 == r) {  
                if (EINTR == errno)  
                        continue;  
                errno_exit ("select");  
            }  

            if (0 == r) {  
                fprintf (stderr, "select timeout\n");  
                exit (EXIT_FAILURE);  
            }  

            if (read_frame ())  
            {
                break;  
            }
            

            /* EAGAIN - continue select loop. */  
        }  
    }  
}  
  
static void  
stop_capturing                  (void)  
{  
        enum v4l2_buf_type type;  
  
    switch (io) {  
    case IO_METHOD_READ:  
        /* Nothing to do. */  
        break;  
  
    case IO_METHOD_MMAP:  
    case IO_METHOD_USERPTR:  
        type = V4L2_BUF_TYPE_VIDEO_CAPTURE;  
  
        if (-1 == xioctl (fd, VIDIOC_STREAMOFF, &type))  
            errno_exit ("VIDIOC_STREAMOFF");  
  
        break;  
    }  
}  
  
static void  start_capturing(void)  
{  
    unsigned int i;  
    enum v4l2_buf_type type;  
  
    switch (io) 
    {  
        case IO_METHOD_READ:  
            /* Nothing to do. */  
            break;  
    
        case IO_METHOD_MMAP:  
            for (i = 0; i < n_buffers; ++i) 
            {  
                struct v4l2_buffer buf;  

                CLEAR (buf);  

                buf.type        = V4L2_BUF_TYPE_VIDEO_CAPTURE;  
                buf.memory      = V4L2_MEMORY_MMAP;  
                buf.index       = i;  

                if (-1 == xioctl (fd, VIDIOC_QBUF, &buf))  
                            errno_exit ("VIDIOC_QBUF");  
            }  
            type = V4L2_BUF_TYPE_VIDEO_CAPTURE;  
            if (-1 == xioctl (fd, VIDIOC_STREAMON, &type))  
                errno_exit ("VIDIOC_STREAMON");  
            break;  
  
        case IO_METHOD_USERPTR:  
            for (i = 0; i < n_buffers; ++i) 
            {  
                struct v4l2_buffer buf;  
                CLEAR (buf);  
                buf.type        = V4L2_BUF_TYPE_VIDEO_CAPTURE;  
                buf.memory      = V4L2_MEMORY_USERPTR;  
                buf.index       = i;  
                buf.m.userptr   = (unsigned long) buffers[i].start;  
                buf.length      = buffers[i].length;  
    
                if (-1 == xioctl (fd, VIDIOC_QBUF, &buf))  
                    errno_exit ("VIDIOC_QBUF");  
            }  
            type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            if (-1 == xioctl (fd, VIDIOC_STREAMON, &type))  
                errno_exit ("VIDIOC_STREAMON");  
            break;  
    }  
}  
  
static void  
uninit_device                   (void)  
{  
        unsigned int i;  
  
    switch (io) {  
    case IO_METHOD_READ:  
        free (buffers[0].start);  
        break;  
  
    case IO_METHOD_MMAP:  
        for (i = 0; i < n_buffers; ++i)  
            if (-1 == munmap (buffers[i].start, buffers[i].length))  
                errno_exit ("munmap");  
        break;  
  
    case IO_METHOD_USERPTR:  
        for (i = 0; i < n_buffers; ++i)  
            free (buffers[i].start);  
        break;  
    }  
  
    free (buffers);  
}  
  
static void  
init_read           (unsigned int       buffer_size)  
{  
        buffers = calloc (1, sizeof (*buffers));  
  
        if (!buffers) {  
                fprintf (stderr, "Out of memory\n");  
                exit (EXIT_FAILURE);  
        }  
  
    buffers[0].length = buffer_size;  
    buffers[0].start = malloc (buffer_size);  
  
    if (!buffers[0].start) {  
            fprintf (stderr, "Out of memory\n");  
                exit (EXIT_FAILURE);  
    }  
}  
  
static void  init_mmap(void)  
{  
    struct v4l2_requestbuffers req;  
  
    CLEAR (req);  

    req.count               = 4;  
    req.type                = V4L2_BUF_TYPE_VIDEO_CAPTURE;  
    req.memory              = V4L2_MEMORY_MMAP;  
  
    if (-1 == xioctl (fd, VIDIOC_REQBUFS, &req)) 
    {  
        if (EINVAL == errno) 
        {  
            fprintf (stderr, "%s does not support "  
                        "memory mapping\n", dev_name);  
            exit (EXIT_FAILURE);  
        } 
        else 
        {  
            errno_exit ("VIDIOC_REQBUFS");  
        }  
    }  
  
    if (req.count < 2) 
    {  
        fprintf (stderr, "Insufficient buffer memory on %s\n",  
                    dev_name);  
        exit (EXIT_FAILURE);  
    }  
  
    buffers = calloc (req.count, sizeof (*buffers));  
  
    if (!buffers) {  
            fprintf (stderr, "Out of memory\n");  
            exit (EXIT_FAILURE);  
    }  

    for (n_buffers = 0; n_buffers < req.count; ++n_buffers) 
    {  
        struct v4l2_buffer buf;  

        CLEAR (buf);  

        buf.type        = V4L2_BUF_TYPE_VIDEO_CAPTURE;  
        buf.memory      = V4L2_MEMORY_MMAP;  
        buf.index       = n_buffers;  

        if (-1 == xioctl (fd, VIDIOC_QUERYBUF, &buf))  
                errno_exit ("VIDIOC_QUERYBUF");  

        buffers[n_buffers].length = buf.length;  
        buffers[n_buffers].start =  
                mmap (  NULL /* start anywhere */,  
                        buf.length,  
                        PROT_READ | PROT_WRITE /* required */,  
                        MAP_SHARED /* recommended */,  
                        fd, buf.m.offset);  

        if (MAP_FAILED == buffers[n_buffers].start)  
                errno_exit ("mmap");  
    }  
}  
  
static void  
init_userp          (unsigned int       buffer_size)  
{  
    struct v4l2_requestbuffers req;  
  
        CLEAR (req);  
  
        req.count               = 4;  
        req.type                = V4L2_BUF_TYPE_VIDEO_CAPTURE;  
        req.memory              = V4L2_MEMORY_USERPTR;  
  
        if (-1 == xioctl (fd, VIDIOC_REQBUFS, &req)) {  
                if (EINVAL == errno) {  
                        fprintf (stderr, "%s does not support "  
                                 "user pointer i/o\n", dev_name);  
                        exit (EXIT_FAILURE);  
                } else {  
                        errno_exit ("VIDIOC_REQBUFS");  
                }  
        }  
  
        buffers = calloc (4, sizeof (*buffers));  
  
        if (!buffers) {  
                fprintf (stderr, "Out of memory\n");  
                exit (EXIT_FAILURE);  
        }  
  
        for (n_buffers = 0; n_buffers < 4; ++n_buffers) {  
                buffers[n_buffers].length = buffer_size;  
                buffers[n_buffers].start = malloc (buffer_size);  
  
                if (!buffers[n_buffers].start) {  
                fprintf (stderr, "Out of memory\n");  
                    exit (EXIT_FAILURE);  
        }  
        }  
}  
  
static void  init_device(void)  
{  
    struct v4l2_capability cap;  
    struct v4l2_cropcap cropcap;  
    struct v4l2_crop crop;  
    struct v4l2_format fmt; 
    struct v4l2_fmtdesc desc;
  
    unsigned int min;  
    
    if (-1 == xioctl (fd, VIDIOC_QUERYCAP, &cap)) 
    {  
        if (EINVAL == errno) 
        {  
            fprintf (stderr, "%s is no V4L2 device\n",  
                        dev_name);  
            exit (EXIT_FAILURE);  
        } 
        else 
        {  
            errno_exit ("VIDIOC_QUERYCAP");  
        }  
    }  

    printf("Driver Name:%s\nCard Name:%s\nBus info:%s\nDriver Version:%u.%u.%u\n",cap.driver,cap.card,cap.bus_info,(cap.version>>16)&0XFF, (cap.version>>8)&0XFF,cap.version&0XFF);
  
    if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE)) 
    {  
        fprintf (stderr, "%s is no video capture device\n",  
                    dev_name);  
        exit (EXIT_FAILURE);  
    }  
  
    switch (io) 
    {  
        case IO_METHOD_READ:  
            if (!(cap.capabilities & V4L2_CAP_READWRITE)) {  
                fprintf (stderr, "%s does not support read i/o\n",  
                    dev_name);  
                exit (EXIT_FAILURE);  
            }  
    
            break;  
  
        case IO_METHOD_MMAP:  
        case IO_METHOD_USERPTR:  
            if (!(cap.capabilities & V4L2_CAP_STREAMING)) {  
                fprintf (stderr, "%s does not support streaming i/o\n",  
                    dev_name);  
                exit (EXIT_FAILURE);  
            }  
    
            break;  
    }  


    desc.index = 0;
	desc.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	while (ioctl(fd, VIDIOC_ENUM_FMT, &desc) == 0) 
	{
		printf("format %s\n", desc.description);
		desc.index++;
	}
   
    /* Select video input, video standard and tune here. */  
  
    CLEAR (cropcap);  
  
    cropcap.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;    
    if (0 == xioctl (fd, VIDIOC_CROPCAP, &cropcap)) 
    {  
        crop.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;  
        crop.c = cropcap.defrect; /* reset to default */  

        if (-1 == xioctl (fd, VIDIOC_S_CROP, &crop)) 
        {  
            switch (errno) 
            {  
                case EINVAL:  
                    /* Cropping not supported. */  
                    break;  
                default:  
                    /* Errors ignored. */  
                    break;  
            }  
        }  
    } 
    else 
    {      
        /* Errors ignored. */  
    }    
    CLEAR (fmt);  

    fmt.type                = V4L2_BUF_TYPE_VIDEO_CAPTURE;  
    fmt.fmt.pix.width       = DISPLAY_SIZE_X;   
    fmt.fmt.pix.height      = DISPLAY_SIZE_Y;  
    fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;  
    fmt.fmt.pix.field       = V4L2_FIELD_INTERLACED;  

    if (-1 == xioctl (fd, VIDIOC_S_FMT, &fmt))  
            errno_exit ("VIDIOC_S_FMT");  

    /* Note VIDIOC_S_FMT may change width and height. */  
  
    /* Buggy driver paranoia. */  
    min = fmt.fmt.pix.width * 2;  
    if (fmt.fmt.pix.bytesperline < min)  
        fmt.fmt.pix.bytesperline = min;  
    min = fmt.fmt.pix.bytesperline * fmt.fmt.pix.height;  
    if (fmt.fmt.pix.sizeimage < min)  
        fmt.fmt.pix.sizeimage = min;  
  
    switch (io) 
    {  
        case IO_METHOD_READ:  
            init_read (fmt.fmt.pix.sizeimage);  
            break;  
    
        case IO_METHOD_MMAP:  
            init_mmap ();  
            break;  
    
        case IO_METHOD_USERPTR:  
            init_userp (fmt.fmt.pix.sizeimage);  
            break;  
    }  
}  
  
static void  
close_device                    (void)  
{  
        if (-1 == close (fd))  
            errno_exit ("close");  
  
        fd = -1;  
}  
  
static void  open_device(void)  
{  
    struct stat st;   

    if (-1 == stat (dev_name, &st)) 
    {  
        fprintf (stderr, "Cannot identify '%s': %d, %s\n",  
                    dev_name, errno, strerror (errno));  
        exit (EXIT_FAILURE);  
    }  

    if (!S_ISCHR (st.st_mode)) 
    {  
        fprintf (stderr, "%s is no device\n", dev_name);  
        exit (EXIT_FAILURE);  
    }  

    fd = open (dev_name, O_RDWR /* required */ | O_NONBLOCK, 0);  

    if (-1 == fd) 
    {  
        fprintf (stderr, "Cannot open '%s': %d, %s\n",  
                        dev_name, errno, strerror (errno));  
        exit (EXIT_FAILURE);  
    }  
}  

  
static void  usage(FILE *fp,int argc,char **argv)  
{  
    fprintf (fp,  
                "Usage: %s [options]\n\n"  
                "Options:\n"  
                "-d | --device name   Video device name [/dev/video]\n"  
                "-h | --help          Print this message\n"  
                "-m | --mmap          Use memory mapped buffers\n"  
                "-r | --read          Use read() calls\n"  
                "-u | --userp         Use application allocated buffers\n"  
                "",argv[0]);  
}  
  
static const char short_options [] = "d:hmru";  
  
static const struct option  
long_options [] = {  
        { "device",     required_argument,      NULL,           'd' },  
        { "help",       no_argument,            NULL,           'h' },  
        { "mmap",       no_argument,            NULL,           'm' },  
        { "read",       no_argument,            NULL,           'r' },  
        { "userp",      no_argument,            NULL,           'u' },  
        { 0, 0, 0, 0 }  
};  
  

int main(int argc, char *argv[])
{  
    dev_name = "/dev/video";  
    fb_info.fb_device = NULL;
  
    for (;;) 
    {  
        int index;  
        int c;  
            
        c = getopt_long (argc, argv,short_options, long_options,&index);  

        if (-1 == c)  
                break;  

        switch (c) {  
        case 0: /* getopt_long() flag */  
                break;  

        case 'd':  
                dev_name = optarg;  
                break;  

        case 'h':  
                usage (stdout, argc, argv);  
                exit (EXIT_SUCCESS);  

        case 'm':  
                io = IO_METHOD_MMAP;  
                break;  

        case 'r':  
                io = IO_METHOD_READ;  
                break;  

        case 'u':  
                io = IO_METHOD_USERPTR;  
                break;  

        default:  
                usage (stderr, argc, argv);  
                exit (EXIT_FAILURE);  
        }  
    }  

    open_device ();  

    init_device ();  

    init_fb(&fb_info);

    start_capturing ();  

    mainloop ();  

    stop_capturing ();  

    uninit_device ();  

    close_device ();  

    close_fb(&fb_info);

    exit (EXIT_SUCCESS);  

    return 0;  
}




#if 0

#include <stdio.h>
#include <sys/types.h>		/* open() */
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>		/* close() */
#include <string.h>		/* memset()  */
#include <sys/ioctl.h>		/* ioctl() */
#include <sys/mman.h>		/* mmap() */
#include <stdlib.h>		/* malloc() */
#include <linux/videodev2.h>	/* v4l2 */

struct bitmap_fileheader {
	unsigned short	type;
	unsigned int	size;
	unsigned short	reserved1;
	unsigned short	reserved2;
	unsigned int	off_bits;
} __attribute__ ((packed));
 
struct bitmap_infoheader {
	unsigned int	size;
	unsigned int	width;
	unsigned int	height;
	unsigned short	planes;
	unsigned short	bit_count;
	unsigned int	compression;
	unsigned int	size_image;
	unsigned int	xpels_per_meter;
	unsigned int	ypels_per_meter;
	unsigned int	clr_used;
	unsigned int	clr_important;
} __attribute__ ((packed));
 
int saveimage(char *filename, char *p, int width, int height, int bits_per_pixel)
{
	FILE *fp;
	struct bitmap_fileheader fh;
	struct bitmap_infoheader ih;
	int x, y;
 
	fp = fopen(filename, "wb");
	if (fp == NULL) {
		printf("can't open file %s\n", filename);
		return -1;
	}
 
	memset(&fh, 0, sizeof(struct bitmap_fileheader));
	fh.type	= 0x4d42;
	fh.off_bits = sizeof(struct bitmap_fileheader) + sizeof(struct bitmap_infoheader);
	fh.size = fh.off_bits + width * height * (bits_per_pixel / 8);
	fwrite(&fh, 1, sizeof(struct bitmap_fileheader), fp);
 
	memset(&ih, 0, sizeof(struct bitmap_infoheader));
	ih.size = sizeof(struct bitmap_infoheader);
	ih.width = width;
	ih.height = height;
	ih.planes = 1;
	ih.bit_count = bits_per_pixel;
/*	ih.compression = 0;
	ih.size_image = 0;
	ih.xpels_per_meter = 0;
	ih.ypels_per_meter = 0;
	ih.clr_used = 0;
	ih.clr_important = 0;*/
	fwrite(&ih, 1, sizeof(struct bitmap_infoheader), fp);
 
/*	fwrite(p, 1, width * height * (bits_per_pixel / 8), fp);*/
 
	p += width * (bits_per_pixel / 8) * (height - 1);
	for (y = 0; y < height; y++, p -= width * (bits_per_pixel / 8)) {
		fwrite(p, 1, width * (bits_per_pixel / 8), fp);
	}
 
	fclose(fp);
 
	return 0;
}
 
void yuv_2_rgb888(unsigned char y, unsigned char u, unsigned char v,
		unsigned char *r, unsigned char *g, unsigned char *b)
{
	/*
	 * From Wikipedia: en.wikipedia.org/wiki/YUV
	 *
	 * r = y + 1.402 * (v - 128)
	 * g = y - 0.344 * (u - 128) - 0.714 * (v - 128)
	 * b = y + 1.772 * (u - 128)
	 */
	int red, green, blue;
 
	red = y + 1.402 * (v - 128);
	green = y - 0.344 * (u - 128) - 0.714 * (v - 128);
	blue = y + 1.772 * (u - 128);
 
	*r = ((red > 255)) ? 255 : ((red < 0) ? 0 : red);
	*g = ((green > 255)) ? 255 : ((green < 0) ? 0 : green);
	*b = ((blue > 255)) ? 255 : ((blue < 0) ? 0 : blue);
}
 
int capture(char *filename, char *buf, int width, int height)
{
	unsigned char *p = NULL;
	int x, y;
	
	unsigned char y0, u0, y1, v0;
 
	p = (unsigned char *)malloc(width * height * 3);
	if (p == NULL)
		return -1;
 
	for (y = 0; y < height; y++) {
		for (x = 0; x < width*3; x += 6, buf += 4) {
			y0 = buf[0]; u0 = buf[1]; y1 = buf[2]; v0 = buf[3];
 
			yuv_2_rgb888(y0, u0, v0, &p[y*width*3+x+0], &p[y*width*3+x+1], &p[y*width*3+x+2]);
			yuv_2_rgb888(y1, u0, v0, &p[y*width*3+x+3], &p[y*width*3+x+4], &p[y*width*3+x+5]);
		}
	}
 
	saveimage(filename, (char *)p, width, height, 24);
	
	free(p);
 
	return 0;
}
 
struct buffer {
	void    *start;
	int     length;
};
 
#define NR_BUFFER 4
struct buffer buffers[NR_BUFFER];
 
/* V4L2 API */
int v4l2_querycap(int fd)
{
	int ret;
	struct v4l2_capability cap;
 
	ret = ioctl(fd, VIDIOC_QUERYCAP, &cap);
	if (ret < 0) 
	{
		perror("VIDIOC_QUERYCAP failed");
		return ret;
	}
	else
	{
		printf("Driver Name:%s\nCard Name:%s\nBus info:%s\nDriver Version:%u.%u.%u\n",cap.driver,cap.card,cap.bus_info,(cap.version>>16)&0XFF, (cap.version>>8)&0XFF,cap.version&0XFF);
	}
	return 0;
}
 
int v4l2_enum_fmt(int fd, enum v4l2_buf_type type)
{
	struct v4l2_fmtdesc desc;
 
	desc.index = 0;
	desc.type = type;
	while (ioctl(fd, VIDIOC_ENUM_FMT, &desc) == 0) 
	{
		printf("format %s\n", desc.description);
		desc.index++;
	}
 
	return 0;
}
 
int v4l2_g_fmt(int fd, enum v4l2_buf_type type, int *width, int *height, int *bytesperline)
{
	int ret;
	struct v4l2_format fmt;
 
	fmt.type = type;
	ret = ioctl(fd, VIDIOC_G_FMT, &fmt);
	if (ret < 0) {
		perror("VIDIOC_G_FMT failed");
		return ret;
	}
 
	*width = fmt.fmt.pix.width;
	*height = fmt.fmt.pix.height;
	*bytesperline = fmt.fmt.pix.bytesperline;
 
	printf("width %d height %d bytesperline %d\n", *width, *height, *bytesperline);
 
	return 0;
}
 
int v4l2_s_fmt(int fd)
{
	return 0;
}
 
int v4l2_reqbufs(int fd, enum v4l2_buf_type type, int nr_buffer)
{
	int ret;
	struct v4l2_requestbuffers req;
 
	req.count = nr_buffer;
	req.type = type;
	req.memory = V4L2_MEMORY_MMAP;
	ret = ioctl(fd, VIDIOC_REQBUFS, &req);
	if (ret < 0) {
		perror("VIDIOC_REQBUFS failed");
		return ret;
	}
 
	return 0;
}
 
int v4l2_querybuf(int fd, enum v4l2_buf_type type, int nr_buffer, struct buffer *video_buffers)
{
	int ret, i;
	struct v4l2_buffer buf;
 
	for (i = 0; i < nr_buffer; i++) {
		buf.index = i;
		buf.type = type;
		buf.memory = V4L2_MEMORY_MMAP;
		ret = ioctl(fd, VIDIOC_QUERYBUF, &buf);
		if (ret < 0) {
			perror("VIDIOC_QUERYBUF failed");
			return ret;
		}
 
		video_buffers[i].length = buf.length;
		video_buffers[i].start = mmap(0, buf.length, PROT_READ | PROT_WRITE,
						MAP_SHARED, fd, buf.m.offset);
	}
 
	return 0;
}
 
int v4l2_qbuf(int fd, enum v4l2_buf_type type, int index)
{
	int ret;
	struct v4l2_buffer buf;
 
	buf.index = index;
	buf.type =  type;
	buf.memory = V4L2_MEMORY_MMAP;
	ret = ioctl(fd, VIDIOC_QBUF, &buf);
	if (ret < 0) {
		perror("VIDIOC_QBUF failed");
		return ret;
	}
	return 0;
}
 
int v4l2_dqbuf(int fd, enum v4l2_buf_type type)
{
	int ret;
	struct v4l2_buffer buf;
 
	buf.type = type;
	buf.memory = V4L2_MEMORY_MMAP;
	ret = ioctl(fd, VIDIOC_DQBUF, &buf);
	if (ret < 0) {
		perror("VIDIOC_DQBUF failed");
		return ret;
	}
 
	return buf.index;
}
 
int v4l2_streamon(int fd)
{
	int ret;
	enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
 
	ret = ioctl(fd, VIDIOC_STREAMON, &type);
	if (ret < 0) {
		perror("VIDIOC_STREAMON failed");
		return ret;
	}
 
	return 0;
}
 
int v4l2_streamoff(int fd)
{
	int ret;
	enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
 
	ret = ioctl(fd, VIDIOC_STREAMOFF, &type);
	if (ret < 0) {
		perror("VIDIOC_STREAMOFF failed");
		return ret;
	}
 
	return 0;
}
/* V4L2 API end */
 
int dev_open(char *devname)
{
	int fd;
 
	fd = open(devname, O_RDWR);
	if (fd < 0) {
		printf("no such video device!\n");
		return -1;
	}
 
	return fd;
}
 
void dev_close(int fd)
{
	close(fd);
}
 
int dev_init(int fd, int *width, int *height, int *bytesperline)
{
	int ret, i;
	enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
 
	ret = v4l2_querycap(fd);
	if (ret < 0)
		return ret;
	
	v4l2_enum_fmt(fd, type);
 
	ret = v4l2_g_fmt(fd, type, width, height, bytesperline);
	if (ret < 0)
		return ret;
 
	ret = v4l2_reqbufs(fd, type, NR_BUFFER);
	if (ret < 0)
		return ret;
 
	ret = v4l2_querybuf(fd, type, NR_BUFFER, buffers);
	if (ret < 0)
		return ret;
 
	for (i = 0; i < NR_BUFFER; i++) {
		v4l2_qbuf(fd, type, i);
	}
 
	return 0;
}
 
int start_preview(int fd)
{
	return v4l2_streamon(fd);
}
 
int stop_preview(int fd)
{
	return v4l2_streamoff(fd);
}
 
int start_capture(char *filename, int fd, int width, int height, int byterperline)
{
	int index;
	enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
 
	index = v4l2_dqbuf(fd, type);
	if (index < 0)
		return -1;
 
	return capture(filename, buffers[index].start, width, height);
}
 
int main(int argc, char *argv[])
{
  	int fd, ret;
	int width, height, bytesperline;
 
	if (argc < 2) {
		printf("please input a file name!\n");
		return -1;
	}
 
	fd = dev_open(argv[1]);
	if (fd < 0)
		return fd;
 
	ret = dev_init(fd, &width, &height, &bytesperline);
	if (ret < 0)
		return ret;
 
 
	ret = start_preview(fd);
	if (ret < 0)
		return ret;
 
	start_capture(argv[1], fd, width, height, bytesperline);
 
	ret = stop_preview(fd);
	if (ret < 0)
		return ret;
 
	dev_close(fd);
 
	return 0;
}


#endif