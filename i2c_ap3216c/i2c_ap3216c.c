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


#define AP3216C_CNT		1
#define AP3216C_NAME	"i2c_bingoAp3216c"

struct ap3216cInfo
{
	u32 major;
	u32 minor;
	dev_t devid;
	struct class* class;
	struct device* device;
	struct cdev cdev;
	struct device_node *nd;
    void* private_data;
};


static struct ap3216cInfo _ap3216cInfo;





static void ap3216c_read_reg(struct i2c_client *client,char reg,char* buf,u32 len)
{


}




static int ap3216c_open(struct inode *inode, struct file *file)
{
	file->private_data = (struct ap3216cInfo*)&_ap3216cInfo;
	return 0;
}


static int ap3216c_release(struct inode *inode, struct file *file)
{
	return 0;
}


static ssize_t ap3216c_read(struct file *file, char __user *userbuf,
				  size_t count, loff_t *ppos)
{
	


}




const struct file_operations ap3216c_fops =
{
	.owner		= THIS_MODULE,
	.open		= ap3216c_open,
	.release	= ap3216c_release,	
	.read 		= ap3216c_read,
	.write		= ap3216c_write,
};


static int ap3216c_probe(struct i2c_client *client,const struct i2c_device_id *id)
{
	int result=0;
	const char * compproStr = NULL;

	_ap3216cInfo.major = 0;
	if(_ap3216cInfo.major)
	{
		_ap3216cInfo.devid = MKDEV(_ap3216cInfo.major,0);
		result = register_chrdev_region(_ap3216cInfo.devid,AP3216C_CNT,AP3216C_NAME);
	}
	else
	{
		result = alloc_chrdev_region(&_ap3216cInfo.devid,0,AP3216C_CNT,AP3216C_NAME);
		_ap3216cInfo.major = MAJOR(_ap3216cInfo.devid);	//获取主设备号
		_ap3216cInfo.minor = MINOR(_ap3216cInfo.devid);	//获取次设备号
	}

	if(result < 0)
	{
		printk("**Kernel** : region devid faild!!!\r\n");
		goto dev_faild;
	}

	_ap3216cInfo.cdev.owner = THIS_MODULE;
	cdev_init(&_ap3216cInfo.cdev,&led_fops);
	result = cdev_add(&_ap3216cInfo.cdev,_ap3216cInfo.devid,AP3216C_CNT);

	if(result < 0)
	{
		printk("**Kernel** : init ap3216c driver faild!!!\r\n");
		goto init_faild;
	}

	_ap3216cInfo.class = class_create(THIS_MODULE,AP3216C_NAME);
	if(IS_ERR(_ap3216cInfo.class))
	{
		printk("**Kernel** : create class faild!!!\r\n");
		result = PTR_ERR(_ap3216cInfo.class);
		goto class_faild;
	}

	
	_ap3216cInfo.device = device_create(_ap3216cInfo.class,NULL,_ap3216cInfo.devid,NULL,AP3216C_NAME);
	if(IS_ERR(_ap3216cInfo.device))
	{
		printk("**Kernel** : create device faild!!!\r\n");
		result = PTR_ERR(_ap3216cInfo.device);
		goto device_faild;
	}

	_ap3216cInfo.private_data = client;	//把私有数据设置为client
	
	return 0;


device_faild:
	class_destroy(_ap3216cInfo.class);
class_faild:
	cdev_del(&_ap3216cInfo.cdev);	
init_faild:
	unregister_chrdev_region(_ap3216cInfo.devid, AP3216C_CNT);
dev_faild:
	return result;
}



static int ap3216c_remove(struct i2c_client *client)
{
	/* 1,删除字符设备 */
	cdev_del(&_ap3216cInfo.cdev);

	/* 2,注销设备号 */
	unregister_chrdev_region(_ap3216cInfo.devid, AP3216C_CNT);

	/* 3,摧毁设备 */
	device_destroy(_ap3216cInfo.class, _ap3216cInfo.devid);

	/* 4,摧毁类 */
	class_destroy(_ap3216cInfo.class);

	printk("**Kernel** : exti ap3216c driver succeed!!!\r\n");

	return 0;
}



static const struct i2c_device_id ap3216c_id[] = {
	{ "bingo,ap3216c", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, ap3216c_id);


static struct i2c_driver ap3216c_driver = {
	.driver = {
		.name	= "i2cDriver_ap3216c",
		.owner	= THIS_MODULE,
	},
	.probe = ap3216c_probe,
	.remove = ap3216c_remove,
	.id_table = ap3216c_id,
};

static int __init ap3216c_init(void)
{
	return i2c_add_driver(&ap3216c_driver);
}

static void __exit ap3216c_exit(void)
{
	i2c_del_driver(&ap3216c_driver);
}





















