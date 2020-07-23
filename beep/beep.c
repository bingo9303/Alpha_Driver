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

#define BEEP_CNT	1
#define BEEP_NAME	"beep"

/* 设备信息结构体 */
struct beepInfo
{	
	u32 major;
	u32 minor;
	dev_t devid;
	struct class* class;
	struct device* device;
	struct cdev cdev;
	struct device_node *nd;
    int beep_gpio;
};
struct beepInfo beepInfo;



static int beep_open(struct inode *inode, struct file *file)
{
	file->private_data = (struct beepInfo*)&beepInfo;
	return 0;
}

static int beep_release(struct inode *inode, struct file *file)
{
	
	return 0;
}

static ssize_t beep_write(struct file *file, const char __user *data,
			 						size_t len, loff_t *ppos)
{
	char writeData;
	int result = 0;
	struct beepInfo* dev = file->private_data;
	
	result = copy_from_user(&writeData, data, len);
	if(result < 0)
	{
		printk("**Kernel** : write faild!!!\r\n");
		return -1;
	}
	
	
	if(writeData == 0)
		gpio_set_value(dev->beep_gpio,0);				//点亮
	else 	
		gpio_set_value(dev->beep_gpio,1);				//灭

	return 0;
}




const struct file_operations beep_fops =
{
	.owner		= THIS_MODULE,
	.open		= beep_open,
	.release	= beep_release,	
	//.read 	= beep_read,
	.write		= beep_write,
};




static int __init beep_init(void)
{
	int result=0;
	const char * compproStr = NULL;

	beepInfo.major = 0;
	if(beepInfo.major)
	{
		beepInfo.devid = MKDEV(beepInfo.major,0);
		result = register_chrdev_region(beepInfo.devid,BEEP_CNT,BEEP_NAME);
	}
	else
	{
		result = alloc_chrdev_region(&beepInfo.devid,0,BEEP_CNT,BEEP_NAME);
		beepInfo.major = MAJOR(beepInfo.devid);	//获取主设备号
		beepInfo.minor = MINOR(beepInfo.devid);	//获取次设备号
	}

	if(result < 0)
	{
		printk("**Kernel** : region devid faild!!!\r\n");
		goto dev_faild;
	}


	
	beepInfo.cdev.owner = THIS_MODULE;
	cdev_init(&beepInfo.cdev,&beep_fops);
	result = cdev_add(&beepInfo.cdev,beepInfo.devid,BEEP_CNT);

	if(result < 0)
	{
		printk("**Kernel** : init led driver faild!!!\r\n");
		goto init_faild;
	}


	
	beepInfo.class = class_create(THIS_MODULE,BEEP_NAME);
	if(IS_ERR(beepInfo.class))
	{
		printk("**Kernel** : create class faild!!!\r\n");
		result = PTR_ERR(beepInfo.class);
		goto class_faild;
	}

	beepInfo.device = device_create(beepInfo.class,NULL,beepInfo.devid,NULL,BEEP_NAME);
	if(IS_ERR(beepInfo.device))
	{
		printk("**Kernel** : create device faild!!!\r\n");
		result = PTR_ERR(beepInfo.device);
		goto device_faild;
	}

	
	beepInfo.nd = of_find_node_by_path("/beep_bingo");
	if(beepInfo.nd == NULL)
	{
		result = -EINVAL;
		goto find_node_fail;
	}

	result = of_property_read_string(beepInfo.nd,"compatible",&compproStr);
	if(result == 0)	//读取成功
	{
		printk("**Kernel** : device tree compatible = %s\r\n",compproStr);	
	}
	else
	{
		result = -EINVAL;
		goto read_string_fail;
	}

	/* 获取gpios里面GPIO数量 */
	result = of_gpio_named_count(beepInfo.nd,"beep-gpios");
	if(result < 0)
	{
		result = -EINVAL;
		goto read_string_fail;
	}
	else 
	{
		printk("**Kernel** : gpio count = %d\r\n",result);	
	}


	/* 获取GPIO编号 */
	beepInfo.beep_gpio = of_get_named_gpio(beepInfo.nd,"beep-gpios",0);
	if(beepInfo.beep_gpio < 0)
	{
		result = -EINVAL;
		goto read_string_fail;
	}
	else 
	{
		printk("**Kernel** : gpio index = %d\r\n",beepInfo.beep_gpio);	
	}

	/* 申请GPIO */
	result = gpio_request(beepInfo.beep_gpio,"beep_bingo-gpio");
	if(result == 0)
	{
		printk("**Kernel** : gpio request succeed!\r\n");
	}
	else
	{
		result = -EINVAL;
		goto goio_request_fail;
	}

	/* 设置输出方向,默认为高电平，关闭蜂鸣器 */
	result = gpio_direction_output(beepInfo.beep_gpio,1);
	if(result == 0)
	{
		printk("**Kernel** : gpio direction output!\r\n");
	}
	else 
	{
		result = -EINVAL;
		goto goio_direction_fail;
	}
	


	return 0;
goio_direction_fail:
	gpio_free(beepInfo.beep_gpio);
goio_request_fail:
read_string_fail:
find_node_fail:


	device_destroy(beepInfo.class, beepInfo.devid);
device_faild:
	class_destroy(beepInfo.class);
class_faild:
	cdev_del(&beepInfo.cdev);	
init_faild:
	unregister_chrdev_region(beepInfo.devid, BEEP_CNT);

dev_faild:
	return result;

}




static void __exit beep_exit(void)
{
	gpio_set_value(beepInfo.beep_gpio,1);//关蜂鸣器
	

	/* 1,删除字符设备 */
	cdev_del(&beepInfo.cdev);

	/* 2,注销设备号 */
	unregister_chrdev_region(beepInfo.devid, BEEP_CNT);

	/* 3,摧毁设备 */
	device_destroy(beepInfo.class, beepInfo.devid);

	/* 4,摧毁类 */
	class_destroy(beepInfo.class);

	gpio_free(beepInfo.beep_gpio);
	
	printk("**Kernel** : exti led driver succeed!!!\r\n");

}












module_init(beep_init);
module_exit(beep_exit);



MODULE_LICENSE("GPL"); 
MODULE_AUTHOR("BINGO");





