#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/types.h>
#include <linux/delay.h>
#include <linux/ide.h>
#include <asm/io.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/atomic.h>





#define SPIN_LOCK_CNT	1
#define SPIN_LOCK_NAME	"lock"

/* 设备信息结构体 */
struct spinLockInfo
{	
	u32 major;
	u32 minor;
	dev_t devid;
	struct class* class;
	struct device* device;
	struct cdev cdev;
	struct device_node *nd;
	char  value;	//受保护的数据
	spinlock_t	lock;//自旋锁	
};
struct spinLockInfo spinLockInfo;

static int spinLock_open(struct inode *inode, struct file *file)
{
	unsigned long flags;
	file->private_data = (struct spinLockInfo*)&spinLockInfo;

	//spin_lock(&spinLockInfo.lock);
	spin_lock_irqsave(&spinLockInfo.lock, flags);//获取自旋锁，准备操作受保护的数据
	
	if(spinLockInfo.value)
	{
		spin_unlock_irqrestore(&spinLockInfo.lock, flags);//能跑进这里来，说明也已经获得了自旋锁，退出前需要释放
		return -EBUSY;
	}
	spinLockInfo.value++;
	
	spin_unlock_irqrestore(&spinLockInfo.lock, flags);
	printk("**Kernel** : get spin lock\r\n");	
	return 0;
}

static int spinLock_release(struct inode *inode, struct file *file)
{
	unsigned long flags;
	struct spinLockInfo* dev = file->private_data;
	spin_lock_irqsave(&dev->lock, flags);//获取自旋锁，准备操作受保护的数据
	spinLockInfo.value--;
	spin_unlock_irqrestore(&spinLockInfo.lock, flags);	
	return 0;
}

static ssize_t spinLock_write(struct file *file, const char __user *data,
			 						size_t len, loff_t *ppos)
{
	return 0;
}




const struct file_operations lock_fops =
{
	.owner		= THIS_MODULE,
	.open		= spinLock_open,
	.release	= spinLock_release,	
	//.read 	= spinLock_read,
	.write		= spinLock_write,
};


static int __init spinLock_init(void)
{
	int result=0;

	spinLockInfo.major = 0;
	if(spinLockInfo.major)
	{
		spinLockInfo.devid = MKDEV(spinLockInfo.major,0);
		result = register_chrdev_region(spinLockInfo.devid,SPIN_LOCK_CNT,SPIN_LOCK_NAME);
	}
	else
	{
		result = alloc_chrdev_region(&spinLockInfo.devid,0,SPIN_LOCK_CNT,SPIN_LOCK_NAME);
		spinLockInfo.major = MAJOR(spinLockInfo.devid);	//获取主设备号
		spinLockInfo.minor = MINOR(spinLockInfo.devid);	//获取次设备号
	}

	if(result < 0)
	{
		printk("**Kernel** : region devid faild!!!\r\n");
		goto dev_faild;
	}


	
	spinLockInfo.cdev.owner = THIS_MODULE;
	cdev_init(&spinLockInfo.cdev,&lock_fops);
	result = cdev_add(&spinLockInfo.cdev,spinLockInfo.devid,SPIN_LOCK_CNT);

	if(result < 0)
	{
		printk("**Kernel** : init led driver faild!!!\r\n");
		goto init_faild;
	}


	
	spinLockInfo.class = class_create(THIS_MODULE,SPIN_LOCK_NAME);
	if(IS_ERR(spinLockInfo.class))
	{
		printk("**Kernel** : create class faild!!!\r\n");
		result = PTR_ERR(spinLockInfo.class);
		goto class_faild;
	}

	spinLockInfo.device = device_create(spinLockInfo.class,NULL,spinLockInfo.devid,NULL,SPIN_LOCK_NAME);
	if(IS_ERR(spinLockInfo.device))
	{
		printk("**Kernel** : create device faild!!!\r\n");
		result = PTR_ERR(spinLockInfo.device);
		goto device_faild;
	}

	spin_lock_init(&spinLockInfo.lock);//初始化自旋锁
	spinLockInfo.value = 0;

	return 0;

device_faild:
	class_destroy(spinLockInfo.class);
class_faild:
	cdev_del(&spinLockInfo.cdev);	
init_faild:
	unregister_chrdev_region(spinLockInfo.devid, SPIN_LOCK_CNT);

dev_faild:
	return result;

}





static void __exit spinLock_exit(void)
{
	/* 1,删除字符设备 */
	cdev_del(&spinLockInfo.cdev);

	/* 2,注销设备号 */
	unregister_chrdev_region(spinLockInfo.devid, SPIN_LOCK_CNT);

	/* 3,摧毁设备 */
	device_destroy(spinLockInfo.class, spinLockInfo.devid);

	/* 4,摧毁类 */
	class_destroy(spinLockInfo.class);
	
	printk("**Kernel** : exti led driver succeed!!!\r\n");

}






module_init(spinLock_init);
module_exit(spinLock_exit);



MODULE_LICENSE("GPL"); 
MODULE_AUTHOR("BINGO");




