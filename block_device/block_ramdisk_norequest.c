#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/ide.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/errno.h>
#include <linux/gpio.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/of_gpio.h>
#include <linux/semaphore.h>
#include <linux/timer.h>
#include <linux/irq.h>
#include <linux/wait.h>
#include <linux/poll.h>
#include <linux/fs.h>
#include <linux/fcntl.h>
#include <linux/platform_device.h>
#include <asm/mach/map.h>
#include <asm/uaccess.h>
#include <asm/io.h>
#include <linux/i2c.h>
#include <linux/spi/spi.h>
#include <linux/of_irq.h>
#include <linux/of.h>
#include <linux/major.h>
#include <linux/vmalloc.h>
#include <linux/blkdev.h>
#include <linux/bitops.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <asm/setup.h>
#include <asm/pgtable.h>
#include <linux/hdreg.h>



#define DISK_RAM_SIZE       (1024*1024*2)       //申请2M内存来模拟磁盘的空间
#define DISK_RAM_NAME       "diskRam_norequest"
#define DISK_RAM_MINOR      3                   //表示3个磁盘分区，不是次设备号为3



struct diskRam_info
{
    int major;
    unsigned char* diskRamBuffAddr;         //申请的内存地址
    spinlock_t lock;            //自旋锁
    struct gendisk *gendisk; /* gendisk */
    struct request_queue *queue;/* 请求队列*/
};




struct diskRam_info diskRam_dev;


#if 0
static void ramdisk_transfer(struct request *req) 
{ 
    unsigned long start = blk_rq_pos(req) << 9; /* blk_rq_pos 获取到的是扇区地址，左移 9 位转换为字节地址 */
    unsigned long len = blk_rq_cur_bytes(req); /* 大小 */

    /* bio 中的数据缓冲区
    * 读：从磁盘读取到的数据存放到 buffer 中
    * 写：buffer 保存这要写入磁盘的数据
    */
    void *buffer = bio_data(req->bio); 

    if(rq_data_dir(req) == READ) /* 读数据 */
        memcpy(buffer, diskRam_dev.diskRamBuffAddr + start, len);
    else if(rq_data_dir(req) == WRITE) /* 写数据 */
        memcpy(diskRam_dev.diskRamBuffAddr + start, buffer, len);
}



static void diskRam_request_fn(struct request_queue *q)
{
    int err = 0;
    struct request *req;

    /* 获取和开启请求队列中的每个请求 */
    req = blk_fetch_request(q);

    while(req != NULL) 
    {
        /* 针对请求做具体的传输处理 */
        ramdisk_transfer(req);

        /* 判断是否为最后一个请求，如果不是的话就获取下一个请求
        * 循环处理完请求队列中的所有请求。
        */
        if (!__blk_end_request_cur(req, err))
            req = blk_fetch_request(q);
    }
}
#endif

static void diskRam_request_fn(struct request_queue *q,struct bio *bio)
{
    int offset;
    struct bio_vec bvec;
    struct bvec_iter iter;
    unsigned long len = 0;
    offset = (bio->bi_iter.bi_sector) << 9; /* 获取设备的偏移地址 */

    /* 处理 bio 中的每个段 */
    bio_for_each_segment(bvec, bio, iter)
    {
        char *ptr = page_address(bvec.bv_page) + bvec.bv_offset;
        len = bvec.bv_len;

        if(bio_data_dir(bio) == READ) /* 读数据 */
            memcpy(ptr, diskRam_dev.diskRamBuffAddr + offset, len);
        else if(bio_data_dir(bio) == WRITE) /* 写数据 */
            memcpy(diskRam_dev.diskRamBuffAddr + offset, ptr, len);
        offset += len;
    }
    set_bit(BIO_UPTODATE, &bio->bi_flags);
    bio_endio(bio, 0);
}


static int diskRam_open(struct block_device * dev, fmode_t mode)
{
    printk("ramdisk open\r\n");
    return 0;
}

static void diskRam_release(struct gendisk * dev, fmode_t mode)
{
    printk("ramdisk release\r\n");
}


/* 获取磁盘信息，这些信息都是假设的 */
static int diskRam_getgeo(struct block_device * dev, struct hd_geometry * geo)
{
    geo->heads = 2; /* 磁头 */
    geo->cylinders = 32; /* 柱面 */
    geo->sectors = DISK_RAM_SIZE / (geo->heads * geo->cylinders *512); /* 磁道上的扇区数量 */
    return 0;
}




static struct block_device_operations diskRam_dev_fops =
{
    .owner		= THIS_MODULE,
    .open       = diskRam_open,
    .release    = diskRam_release,
    .getgeo     = diskRam_getgeo,
};

static int __init diskRam_init(void)
{
    int result=0;
    diskRam_dev.diskRamBuffAddr = kzalloc(DISK_RAM_SIZE, GFP_KERNEL);
    if(diskRam_dev.diskRamBuffAddr == NULL)
    {
        result = -EINVAL;
        goto ram_fail;
    }

    /* 初始化自旋锁 */
    spin_lock_init(&diskRam_dev.lock);

    /* 注册块设备 */
    diskRam_dev.major = register_blkdev(0, DISK_RAM_NAME);
    if(diskRam_dev.major < 0)
    {
        result = -EINVAL;
        goto register_blkdev_fail;

    }
    printk("**Kernel** : diskRam_norequest major : %d\r\n",diskRam_dev.major);


    /* 分配并初始化 gendisk */
    diskRam_dev.gendisk = alloc_disk(DISK_RAM_MINOR);
    if(!diskRam_dev.gendisk) 
    {
        result = -EINVAL;
        goto gendisk_alloc_fail;
    }

#if 0
    /* 分配并初始化请求队列 */
    diskRam_dev.queue = blk_init_queue(diskRam_request_fn,&diskRam_dev.lock);
    if(diskRam_dev.queue == NULL)
    {
         result = -EINVAL;
        goto blk_init_queue_fail;
    }
#endif

    /* 分配请求队列 */
    diskRam_dev.queue = blk_alloc_queue(GFP_KERNEL);
    if(diskRam_dev.queue == NULL)
    {
         result = -EINVAL;
        goto blk_alloc_queue_fail;
    }

    blk_queue_make_request(diskRam_dev.queue, diskRam_request_fn);


    /* 注册gendisk */
    diskRam_dev.gendisk->major = diskRam_dev.major;     /* 主设备号 */
    diskRam_dev.gendisk->first_minor = 0;               /* 起始次设备号 */
    diskRam_dev.gendisk->fops = &diskRam_dev_fops;      /* 操作函数 */
    diskRam_dev.gendisk->private_data = &diskRam_dev;   /* 私有数据 */
    diskRam_dev.gendisk->queue = diskRam_dev.queue;     /* 请求队列 */
    sprintf(diskRam_dev.gendisk->disk_name,"%s",DISK_RAM_NAME);  /* 名字 */

    set_capacity(diskRam_dev.gendisk, DISK_RAM_SIZE/512);    /* 设备容量(单位为扇区)*/
    add_disk(diskRam_dev.gendisk);                          /* 注册gendisk */

    return 0;

blk_alloc_queue_fail:
    put_disk(diskRam_dev.gendisk);
gendisk_alloc_fail:
    unregister_blkdev(diskRam_dev.major,DISK_RAM_NAME);
register_blkdev_fail:    
    kfree(diskRam_dev.diskRamBuffAddr); /* 释放内存 */
ram_fail:
    return result;

}






static void __exit diskRam_exit(void)
{
    /* 释放 gendisk */
    put_disk(diskRam_dev.gendisk);
    del_gendisk(diskRam_dev.gendisk);

    /* 清除请求队列 */
    blk_cleanup_queue(diskRam_dev.queue);

    /* 注销块设备 */
    unregister_blkdev(diskRam_dev.major,DISK_RAM_NAME);

    /* 释放内存 */
    kfree(diskRam_dev.diskRamBuffAddr);
}







module_init(diskRam_init);
module_exit(diskRam_exit);



MODULE_LICENSE("GPL"); 
MODULE_AUTHOR("BINGO");




