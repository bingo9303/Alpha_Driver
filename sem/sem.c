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





#define SEM_LOCK_CNT	1
#define SEM_LOCK_NAME	"sem"

/* 设备信息结构体 */
struct semLockInfo
{	
	u32 major;
	u32 minor;
	dev_t devid;
	struct class* class;
	struct device* device;
	struct cdev cdev;
	struct device_node *nd;
	char  value;	//受保护的数据
	struct semaphore sem;
};
struct semLockInfo semLockInfo;

static int semLock_open(struct inode *inode, struct file *file)
{
	file->private_data = (struct semLockInfo*)&semLockInfo;

	down(&semLockInfo.sem);//获取信号量
	semLockInfo.value++;
	//不马上释放，延迟一会再释放
	
	printk("**Kernel** : get spin lock\r\n");	
	return 0;
}

static int semLock_release(struct inode *inode, struct file *file)
{
	struct semLockInfo* dev = file->private_data;
	up(&dev->sem);
	return 0;
}

static ssize_t semLock_write(struct file *file, const char __user *data,
			 						size_t len, loff_t *ppos)
{
	return 0;
}




const struct file_operations sem_fops =
{
	.owner		= THIS_MODULE,
	.open		= semLock_open,
	.release	= semLock_release,	
	//.read 	= semLock_read,
	.write		= semLock_write,
};


static int __init semLock_init(void)
{
	int result=0;

	semLockInfo.major = 0;
	if(semLockInfo.major)
	{
		semLockInfo.devid = MKDEV(semLockInfo.major,0);
		result = register_chrdev_region(semLockInfo.devid,SEM_LOCK_CNT,SEM_LOCK_NAME);
	}
	else
	{
		result = alloc_chrdev_region(&semLockInfo.devid,0,SEM_LOCK_CNT,SEM_LOCK_NAME);
		semLockInfo.major = MAJOR(semLockInfo.devid);	//获取主设备号
		semLockInfo.minor = MINOR(semLockInfo.devid);	//获取次设备号
	}

	if(result < 0)
	{
		printk("**Kernel** : region devid faild!!!\r\n");
		goto dev_faild;
	}


	
	semLockInfo.cdev.owner = THIS_MODULE;
	cdev_init(&semLockInfo.cdev,&sem_fops);
	result = cdev_add(&semLockInfo.cdev,semLockInfo.devid,SEM_LOCK_CNT);

	if(result < 0)
	{
		printk("**Kernel** : init led driver faild!!!\r\n");
		goto init_faild;
	}


	
	semLockInfo.class = class_create(THIS_MODULE,SEM_LOCK_NAME);
	if(IS_ERR(semLockInfo.class))
	{
		printk("**Kernel** : create class faild!!!\r\n");
		result = PTR_ERR(semLockInfo.class);
		goto class_faild;
	}

	semLockInfo.device = device_create(semLockInfo.class,NULL,semLockInfo.devid,NULL,SEM_LOCK_NAME);
	if(IS_ERR(semLockInfo.device))
	{
		printk("**Kernel** : create device faild!!!\r\n");
		result = PTR_ERR(semLockInfo.device);
		goto device_faild;
	}

	sema_init(&semLockInfo.sem,1);	//初始化信号量为1
	
	return 0;

device_faild:
	class_destroy(semLockInfo.class);
class_faild:
	cdev_del(&semLockInfo.cdev);	
init_faild:
	unregister_chrdev_region(semLockInfo.devid, SEM_LOCK_CNT);

dev_faild:
	return result;

}





static void __exit semLock_exit(void)
{
	/* 1,删除字符设备 */
	cdev_del(&semLockInfo.cdev);

	/* 2,注销设备号 */
	unregister_chrdev_region(semLockInfo.devid, SEM_LOCK_CNT);

	/* 3,摧毁设备 */
	device_destroy(semLockInfo.class, semLockInfo.devid);

	/* 4,摧毁类 */
	class_destroy(semLockInfo.class);
	
	printk("**Kernel** : exti led driver succeed!!!\r\n");

}






module_init(semLock_init);
module_exit(semLock_exit);



MODULE_LICENSE("GPL"); 
MODULE_AUTHOR("BINGO");




