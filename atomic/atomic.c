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





#define ATOMIC_CNT	1
#define ATOMIC_NAME	"atomic"

/* 设备信息结构体 */
struct atomicInfo
{	
	u32 major;
	u32 minor;
	dev_t devid;
	struct class* class;
	struct device* device;
	struct cdev cdev;
	struct device_node *nd;
	atomic_t	atomicValue;//原子操作数值

	
};
struct atomicInfo atomicInfo;

static int atomic_open(struct inode *inode, struct file *file)
{
	file->private_data = (struct atomicInfo*)&atomicInfo;

	if(atomic_read(&atomicInfo.atomicValue))
	{
		return -EBUSY;		
	}
	else
	{
		atomic_inc(&atomicInfo.atomicValue);
	}

	
	return 0;
}

static int atomic_release(struct inode *inode, struct file *file)
{
	struct atomicInfo* dev = file->private_data;
	atomic_dec(&dev->atomicValue);
	return 0;
}

static ssize_t atomic_write(struct file *file, const char __user *data,
			 						size_t len, loff_t *ppos)
{
	return 0;
}




const struct file_operations atomic_fops =
{
	.owner		= THIS_MODULE,
	.open		= atomic_open,
	.release	= atomic_release,	
	//.read 	= atomic_read,
	.write		= atomic_write,
};


static int __init atomic_init(void)
{
	int result=0;

	atomicInfo.major = 0;
	if(atomicInfo.major)
	{
		atomicInfo.devid = MKDEV(atomicInfo.major,0);
		result = register_chrdev_region(atomicInfo.devid,ATOMIC_CNT,ATOMIC_NAME);
	}
	else
	{
		result = alloc_chrdev_region(&atomicInfo.devid,0,ATOMIC_CNT,ATOMIC_NAME);
		atomicInfo.major = MAJOR(atomicInfo.devid);	//获取主设备号
		atomicInfo.minor = MINOR(atomicInfo.devid);	//获取次设备号
	}

	if(result < 0)
	{
		printk("**Kernel** : region devid faild!!!\r\n");
		goto dev_faild;
	}


	
	atomicInfo.cdev.owner = THIS_MODULE;
	cdev_init(&atomicInfo.cdev,&atomic_fops);
	result = cdev_add(&atomicInfo.cdev,atomicInfo.devid,ATOMIC_CNT);

	if(result < 0)
	{
		printk("**Kernel** : init led driver faild!!!\r\n");
		goto init_faild;
	}


	
	atomicInfo.class = class_create(THIS_MODULE,ATOMIC_NAME);
	if(IS_ERR(atomicInfo.class))
	{
		printk("**Kernel** : create class faild!!!\r\n");
		result = PTR_ERR(atomicInfo.class);
		goto class_faild;
	}

	atomicInfo.device = device_create(atomicInfo.class,NULL,atomicInfo.devid,NULL,ATOMIC_NAME);
	if(IS_ERR(atomicInfo.device))
	{
		printk("**Kernel** : create device faild!!!\r\n");
		result = PTR_ERR(atomicInfo.device);
		goto device_faild;
	}

	atomic_set(&atomicInfo.atomicValue,0);//原子变量赋值0
	

	return 0;

device_faild:
	class_destroy(atomicInfo.class);
class_faild:
	cdev_del(&atomicInfo.cdev);	
init_faild:
	unregister_chrdev_region(atomicInfo.devid, ATOMIC_CNT);

dev_faild:
	return result;

}





static void __exit atomic_exit(void)
{
	/* 1,删除字符设备 */
	cdev_del(&atomicInfo.cdev);

	/* 2,注销设备号 */
	unregister_chrdev_region(atomicInfo.devid, ATOMIC_CNT);

	/* 3,摧毁设备 */
	device_destroy(atomicInfo.class, atomicInfo.devid);

	/* 4,摧毁类 */
	class_destroy(atomicInfo.class);
	
	printk("**Kernel** : exti led driver succeed!!!\r\n");

}






module_init(atomic_init);
module_exit(atomic_exit);



MODULE_LICENSE("GPL"); 
MODULE_AUTHOR("BINGO");




