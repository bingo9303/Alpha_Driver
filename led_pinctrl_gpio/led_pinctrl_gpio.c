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


#define LED_MAJOR	201
#define LED_NAME	"ledGpio"
#define LED_CNT		1

/* 设备信息结构体 */
struct ledInfo
{	
	u32 major;
	u32 minor;
	dev_t devid;
	struct class* class;
	struct device* device;
	struct cdev cdev;
	struct device_node *nd;
    int led_gpio;
};
struct ledInfo ledInfo;



static int led_open(struct inode *inode, struct file *file)
{
	file->private_data = (struct ledInfo*)&ledInfo;
	return 0;
}

static int led_release(struct inode *inode, struct file *file)
{
	
	return 0;
}

static ssize_t led_write(struct file *file, const char __user *data,
			 						size_t len, loff_t *ppos)
{
	char writeData;
	int result = 0;
	struct ledInfo* dev = file->private_data;
	
	result = copy_from_user(&writeData, data, len);
	if(result < 0)
	{
		printk("**Kernel** : write faild!!!\r\n");
		return -1;
	}
	
	
	if(writeData == 0)
		gpio_set_value(dev->led_gpio,0);				//点亮
	else 	
		gpio_set_value(dev->led_gpio,1);				//灭

	return 0;
}



const struct file_operations led_fops =
{
	.owner		= THIS_MODULE,
	.open		= led_open,
	.release	= led_release,	
	//.read 	= led_read,
	.write		= led_write,
};



static int __init led_init(void)
{
	int result=0;
	const char * compproStr = NULL;

	ledInfo.major = 0;
	if(ledInfo.major)
	{
		ledInfo.devid = MKDEV(ledInfo.major,0);
		result = register_chrdev_region(ledInfo.devid,LED_CNT,LED_NAME);
	}
	else
	{
		result = alloc_chrdev_region(&ledInfo.devid,0,LED_CNT,LED_NAME);
		ledInfo.major = MAJOR(ledInfo.devid);	//获取主设备号
		ledInfo.minor = MINOR(ledInfo.devid);	//获取次设备号
	}

	if(result < 0)
	{
		printk("**Kernel** : region devid faild!!!\r\n");
		goto dev_faild;
	}

	ledInfo.cdev.owner = THIS_MODULE;
	cdev_init(&ledInfo.cdev,&led_fops);
	result = cdev_add(&ledInfo.cdev,ledInfo.devid,LED_CNT);

	if(result < 0)
	{
		printk("**Kernel** : init led driver faild!!!\r\n");
		goto init_faild;
	}

	ledInfo.class = class_create(THIS_MODULE,LED_NAME);
	if(IS_ERR(ledInfo.class))
	{
		printk("**Kernel** : create class faild!!!\r\n");
		result = PTR_ERR(ledInfo.class);
		goto class_faild;
	}

	
	ledInfo.device = device_create(ledInfo.class,NULL,ledInfo.devid,NULL,LED_NAME);
	if(IS_ERR(ledInfo.device))
	{
		printk("**Kernel** : create device faild!!!\r\n");
		result = PTR_ERR(ledInfo.device);
		goto device_faild;
	}


	ledInfo.nd = of_find_node_by_path("/led_gpio_bingo");
	if(ledInfo.nd == NULL)
	{
		result = -EINVAL;
		goto find_node_fail;
	}

	result = of_property_read_string(ledInfo.nd,"compatible",&compproStr);
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
	result = of_gpio_named_count(ledInfo.nd,"led_bingo-gpios");
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
	ledInfo.led_gpio = of_get_named_gpio(ledInfo.nd,"led_bingo-gpios",0);
	if(ledInfo.led_gpio < 0)
	{
		result = -EINVAL;
		goto read_string_fail;
	}
	else 
	{
		printk("**Kernel** : gpio index = %d\r\n",ledInfo.led_gpio);	
	}

	/* 申请GPIO */
	result = gpio_request(ledInfo.led_gpio,"led_bingo-gpio");
	if(result == 0)
	{
		printk("**Kernel** : gpio request succeed!\r\n");
	}
	else
	{
		result = -EINVAL;
		goto goio_request_fail;
	}

	/* 设置输出方向,默认为低电平，点亮LED */
	result = gpio_direction_output(ledInfo.led_gpio,0);
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
	gpio_free(ledInfo.led_gpio);

goio_request_fail:
read_string_fail:	
find_node_fail:

	device_destroy(ledInfo.class, ledInfo.devid);
device_faild:
	class_destroy(ledInfo.class);
class_faild:
	cdev_del(&ledInfo.cdev);	
init_faild:
	unregister_chrdev_region(ledInfo.devid, LED_CNT);
dev_faild:
	return result;
}



static void __exit led_exit(void)
{

	gpio_set_value(ledInfo.led_gpio,1);//关灯
	

	/* 1,删除字符设备 */
	cdev_del(&ledInfo.cdev);

	/* 2,注销设备号 */
	unregister_chrdev_region(ledInfo.devid, LED_CNT);

	/* 3,摧毁设备 */
	device_destroy(ledInfo.class, ledInfo.devid);

	/* 4,摧毁类 */
	class_destroy(ledInfo.class);

	gpio_free(ledInfo.led_gpio);
	
	printk("**Kernel** : exti led driver succeed!!!\r\n");

}


module_init(led_init);
module_exit(led_exit);



MODULE_LICENSE("GPL"); 
MODULE_AUTHOR("BINGO");





