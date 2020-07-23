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





#define MUTEX_LOCK_CNT	1
#define MUTEX_LOCK_NAME	"mutex"

/* 设备信息结构体 */
struct mutexLockInfo
{	
	u32 major;
	u32 minor;
	dev_t devid;
	struct class* class;
	struct device* device;
	struct cdev cdev;
	struct device_node *nd;
	char  value;	//受保护的数据
	struct mutex mutex;
};
struct mutexLockInfo mutexLockInfo;

static int mutexLock_open(struct inode *inode, struct file *file)
{
	file->private_data = (struct mutexLockInfo*)&mutexLockInfo;

	mutex_lock(&mutexLockInfo.mutex);	//上锁，获取互斥体
	mutexLockInfo.value++;
	//不马上释放，延迟一会再释放
	
	printk("**Kernel** : get spin lock\r\n");	
	return 0;
}

static int mutexLock_release(struct inode *inode, struct file *file)
{
	struct mutexLockInfo* dev = file->private_data;
	mutex_unlock(&dev->mutex);		
	return 0;
}

static ssize_t mutexLock_write(struct file *file, const char __user *data,
			 						size_t len, loff_t *ppos)
{
	return 0;
}




const struct file_operations sem_fops =
{
	.owner		= THIS_MODULE,
	.open		= mutexLock_open,
	.release	= mutexLock_release,	
	//.read 	= mutexLock_read,
	.write		= mutexLock_write,
};


static int __init mutexLock_init(void)
{
	int result=0;

	mutexLockInfo.major = 0;
	if(mutexLockInfo.major)
	{
		mutexLockInfo.devid = MKDEV(mutexLockInfo.major,0);
		result = register_chrdev_region(mutexLockInfo.devid,MUTEX_LOCK_CNT,MUTEX_LOCK_NAME);
	}
	else
	{
		result = alloc_chrdev_region(&mutexLockInfo.devid,0,MUTEX_LOCK_CNT,MUTEX_LOCK_NAME);
		mutexLockInfo.major = MAJOR(mutexLockInfo.devid);	//获取主设备号
		mutexLockInfo.minor = MINOR(mutexLockInfo.devid);	//获取次设备号
	}

	if(result < 0)
	{
		printk("**Kernel** : region devid faild!!!\r\n");
		goto dev_faild;
	}


	
	mutexLockInfo.cdev.owner = THIS_MODULE;
	cdev_init(&mutexLockInfo.cdev,&sem_fops);
	result = cdev_add(&mutexLockInfo.cdev,mutexLockInfo.devid,MUTEX_LOCK_CNT);

	if(result < 0)
	{
		printk("**Kernel** : init led driver faild!!!\r\n");
		goto init_faild;
	}


	
	mutexLockInfo.class = class_create(THIS_MODULE,MUTEX_LOCK_NAME);
	if(IS_ERR(mutexLockInfo.class))
	{
		printk("**Kernel** : create class faild!!!\r\n");
		result = PTR_ERR(mutexLockInfo.class);
		goto class_faild;
	}

	mutexLockInfo.device = device_create(mutexLockInfo.class,NULL,mutexLockInfo.devid,NULL,MUTEX_LOCK_NAME);
	if(IS_ERR(mutexLockInfo.device))
	{
		printk("**Kernel** : create device faild!!!\r\n");
		result = PTR_ERR(mutexLockInfo.device);
		goto device_faild;
	}


	mutex_init(&mutexLockInfo.mutex);//初始化互斥体

	return 0;

device_faild:
	class_destroy(mutexLockInfo.class);
class_faild:
	cdev_del(&mutexLockInfo.cdev);	
init_faild:
	unregister_chrdev_region(mutexLockInfo.devid, MUTEX_LOCK_CNT);

dev_faild:
	return result;

}





static void __exit mutexLock_exit(void)
{
	/* 1,删除字符设备 */
	cdev_del(&mutexLockInfo.cdev);

	/* 2,注销设备号 */
	unregister_chrdev_region(mutexLockInfo.devid, MUTEX_LOCK_CNT);

	/* 3,摧毁设备 */
	device_destroy(mutexLockInfo.class, mutexLockInfo.devid);

	/* 4,摧毁类 */
	class_destroy(mutexLockInfo.class);
	
	printk("**Kernel** : exti led driver succeed!!!\r\n");

}






module_init(mutexLock_init);
module_exit(mutexLock_exit);



MODULE_LICENSE("GPL"); 
MODULE_AUTHOR("BINGO");




