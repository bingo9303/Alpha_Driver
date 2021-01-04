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

struct formatListInfo
{
	unsigned int index;
	char name[16];
};

enum
{
    FORMAT_YUYV = 0,
    FORMAT_RGB888,
    FORMAT_MJPEG,
};

static struct formatListInfo v4l2FormatList[] = {
	{	V4L2_PIX_FMT_YUYV,		"YUYV" },
	{	V4L2_PIX_FMT_RGB24,		"RGB8-8-8"},
	{	V4L2_PIX_FMT_MJPEG,		"MJPEG"},
};


static char videoFormatType = FORMAT_YUYV;

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
  
static int  xioctl(int fd,int  request,void * arg)  
{  
    int r;  
    do r = ioctl (fd, request, arg);  
    while (-1 == r && EINTR == errno);  
    return r;  
}  
  


static void  process_image(const void *p)  
{  
    
    char strbuff[50];
    unsigned char* startAddr;
	unsigned int len;
    //printf("1.%ld\r\n",GetTime_Ms());
  	switch (io) 
  	{
		case IO_METHOD_READ: 
		case IO_METHOD_MMAP:  
			{
				struct buffer * buf = (struct buffer * )p;
				startAddr = buf->start;
				len = buf->length;
			}
			break;
		case IO_METHOD_USERPTR:  
			{
				struct v4l2_buffer * buf = (struct v4l2_buffer *)p;
				startAddr = (unsigned char*)buf->m.userptr;
				len = buf->length;
			}			
			break;
	}


    if(fb_info.fb_depth == 32)
    {
        if(videoFormatType == FORMAT_YUYV)
        {
            yuyv_to_rgbxxx(  startAddr,
                            len,
                            fb_info.framebuff,
                            fb_info.fb_width, 
                            fb_info.fb_height,
                            0, 0,
                            DISPLAY_SIZE_X,
                            FB_32B_RGB8888); 
        }
        else if(videoFormatType == FORMAT_RGB888)
        {
            rgb888_to_rgbxxx(  startAddr,
		                        len,
		                        fb_info.framebuff,
		                        fb_info.fb_width, 
		                        fb_info.fb_height,
		                        0, 0,
		                        DISPLAY_SIZE_X,
		                        FB_32B_RGB8888);
        }
		else if(videoFormatType == FORMAT_MJPEG)
		{
			mjpeg_to_rgbxxx(  startAddr,
                        		len,
                        		fb_info.framebuff,
                        		fb_info.fb_width, 
		                        fb_info.fb_height,
                        		0, 0,
                        		FB_32B_RGB8888);
		}       
    }
    else if(fb_info.fb_depth == 24)
    {
        if(videoFormatType == FORMAT_YUYV)
        {
            yuyv_to_rgbxxx(  startAddr,
                        len,
                        fb_info.framebuff,
                        fb_info.fb_width, 
                        fb_info.fb_height,
                        0, 0,
                        DISPLAY_SIZE_X,
                        FB_24B_RGB888); 
        }
        else if(videoFormatType == FORMAT_RGB888)
        {
            rgb888_to_rgbxxx(  startAddr,
                        len,
                        fb_info.framebuff,
                        fb_info.fb_width, 
                        fb_info.fb_height,
                        0, 0,
                        DISPLAY_SIZE_X,
                        FB_24B_RGB888);
        }
		else if(videoFormatType == FORMAT_MJPEG)
		{
			mjpeg_to_rgbxxx(  startAddr,
                        		len,
                        		fb_info.framebuff,
                        		fb_info.fb_width, 
		                        fb_info.fb_height,
                        		0, 0,
                        		FB_24B_RGB888);
		}      
    }
    else if(fb_info.fb_depth == 16)
    {
        if(videoFormatType == FORMAT_YUYV)
        {
            yuyv_to_rgbxxx(   startAddr,
                                len,
                                fb_info.framebuff,
                                fb_info.fb_width, 
                                fb_info.fb_height,
                                0, 0,
                                DISPLAY_SIZE_X,
                                FB_16B_RGB565);
        }
        else if(videoFormatType == FORMAT_RGB888)
        {
            rgb888_to_rgbxxx(  startAddr,
                        len,
                        fb_info.framebuff,
                        fb_info.fb_width, 
                        fb_info.fb_height,
                        0, 0,
                        DISPLAY_SIZE_X,
                        FB_16B_RGB565);
        }
		else if(videoFormatType == FORMAT_MJPEG)
		{
			mjpeg_to_rgbxxx(  startAddr,
                        		len,
                        		fb_info.framebuff,
                        		fb_info.fb_width, 
		                        fb_info.fb_height,
                        		0, 0,
                        		FB_16B_RGB565);
		}    
    }    
   // printf("2.%ld\r\n",GetTime_Ms());
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
            //process_image (buffers[0].start);  
            process_image (&buffers[0]);  
            break;  
  
        case IO_METHOD_MMAP:  
            CLEAR (buf);  
            buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;  
            buf.memory = V4L2_MEMORY_MMAP;  

			//buf里会被填充当前读取到的是哪个一个内核空间内存的index的缓存帧
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
            {
            	//找出当前读取到的缓存帧属于哪一块用户空间的内存
				if (buf.m.userptr == (unsigned long) buffers[i].start  
                    && buf.length == buffers[i].length)  
				{
					break;  
				}                    
			}
                
            assert (i < n_buffers);  
           // process_image ((void *) buf.m.userptr);  
            process_image (&buf);  
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
  
static void  stop_capturing(void)  
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
  
static void  uninit_device(void)  
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
  
static void  init_read(unsigned int buffer_size)  
{  
    buffers = calloc (1, sizeof (*buffers));  

    if (!buffers) 
	{  
        fprintf (stderr, "Out of memory\n");  
        exit (EXIT_FAILURE);  
    }  
  
    buffers[0].length = buffer_size;  
    buffers[0].start = malloc (buffer_size);  
  
    if (!buffers[0].start) 
	{  
        fprintf (stderr, "Out of memory\n");  
        exit (EXIT_FAILURE);  
    }  
}  
  
static void  init_mmap(void)  
{  
    struct v4l2_requestbuffers req;  
  
    CLEAR (req);  

    req.count               = 4;  //缓冲帧数
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
  
    if (!buffers) 
	{  
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
  
static void  init_userp(unsigned int buffer_size)  
{  
    struct v4l2_requestbuffers req;  
  
    CLEAR (req);  

    req.count               = 4;  //缓冲帧数
    req.type                = V4L2_BUF_TYPE_VIDEO_CAPTURE;  
    req.memory              = V4L2_MEMORY_USERPTR;  

    if (-1 == xioctl (fd, VIDIOC_REQBUFS, &req)) 
	{  
        if (EINVAL == errno) 
		{  
           fprintf (stderr, "%s does not support "  
                         "user pointer i/o\n", dev_name);  
           exit (EXIT_FAILURE);  
        } 
		else 
		{  
           errno_exit ("VIDIOC_REQBUFS");  
        }  
    }  

    buffers = calloc (4, sizeof (*buffers));  

    if (!buffers) 
	{  
        fprintf (stderr, "Out of memory\n");  
        exit (EXIT_FAILURE);  
    }  

    for (n_buffers = 0; n_buffers < 4; ++n_buffers) 
	{  
        buffers[n_buffers].length = buffer_size;  
        buffers[n_buffers].start = malloc (buffer_size);  

        if (!buffers[n_buffers].start) 
		{  
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
            if (!(cap.capabilities & V4L2_CAP_READWRITE)) 
			{  
                fprintf (stderr, "%s does not support read i/o\n",  
                    dev_name);  
                exit (EXIT_FAILURE);  
            }  
            break;  
  
        case IO_METHOD_MMAP:  
        case IO_METHOD_USERPTR:  
            if (!(cap.capabilities & V4L2_CAP_STREAMING)) 
			{  
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
    fmt.fmt.pix.pixelformat = v4l2FormatList[videoFormatType].index;  
    fmt.fmt.pix.field       = V4L2_FIELD_INTERLACED;

    if (-1 == xioctl (fd, VIDIOC_S_FMT, &fmt))  
    {
		errno_exit ("VIDIOC_S_FMT");  
	}
	else
	{
		printf("use format : %s\r\n",v4l2FormatList[videoFormatType].name);
	}
	

    /* Note VIDIOC_S_FMT may change width and height. */  
  
    /* Buggy driver paranoia. */  
	
	//YUYV格式下，4个字节对应2个像素点，1个像素点相当于需要2个字节
	if(videoFormatType == FORMAT_YUYV)
        min = fmt.fmt.pix.width * 2;  		//一行至少需要几个字节
    else if(videoFormatType == FORMAT_RGB888)
        min = fmt.fmt.pix.width * 3;
    else
		goto request_buffers;

        
    if (fmt.fmt.pix.bytesperline < min)  
    {
		fmt.fmt.pix.bytesperline = min;  
	}		

    min = fmt.fmt.pix.bytesperline * fmt.fmt.pix.height;  //一行至少需要几个字节
    if (fmt.fmt.pix.sizeimage < min)  
    {
		fmt.fmt.pix.sizeimage = min;  
	}
        
request_buffers:
	//申请内存
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
  
static void  close_device(void)  
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
                "-f | --format        Input Video Format\n"  
                "",argv[0]);  
}  
  
static const char short_options [] = "d:F:hmru";  
  
static const struct option  
long_options [] = {  
        { "device",     required_argument,      NULL,           'd' },  
        { "help",       no_argument,            NULL,           'h' },  
        { "mmap",       no_argument,            NULL,           'm' },  
        { "read",       no_argument,            NULL,           'r' },  
        { "userp",      no_argument,            NULL,           'u' },  
        { "format",     no_argument,            NULL,           'F' },
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
				if(videoFormatType == FORMAT_MJPEG)
				{
					printf("MJPEG only use mmap\r\n");
					exit (EXIT_FAILURE);
				}
                io = IO_METHOD_READ;  
                break;  

        case 'u':  
				if(videoFormatType == FORMAT_MJPEG)
				{
					printf("MJPEG only use mmap\r\n");
					exit (EXIT_FAILURE);
				}
                io = IO_METHOD_USERPTR;  
                break;  
        case 'F':
              if(!strcmp("YUYV",optarg))
              {
				videoFormatType = FORMAT_YUYV;
              }
              else if(!strcmp("RGB888",optarg))
              {
                videoFormatType = FORMAT_RGB888;
              }
			  else if(!strcmp("MJPEG",optarg))
			  {
				videoFormatType = FORMAT_MJPEG;
				io = IO_METHOD_MMAP;
			  }
              else
              {
                usage (stderr, argc, argv);  
                exit (EXIT_FAILURE);
              }
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
