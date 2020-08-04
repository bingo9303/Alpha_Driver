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

#define MISC_BEEP_MINOR	177
#define MISC_BEEP_NAME	"misc_beep"

/* 设备信息结构体 */
struct beepInfo
{	
	struct device_node *nd;
    int beep_gpio;
};
static struct beepInfo beepInfo;



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

static struct miscdevice miscdev_beep = 
{
	.minor = MISC_BEEP_MINOR,
	.name = MISC_BEEP_NAME,
	.fops = beep_fops,
};





static int beep_probe(struct platform_device *dev)
{
	int result=0;
	const char * compproStr = NULL;

	result = misc_register(&miscdev_beep);
	if(result < 0)
	{
		printk("**Kernel** : misc register faild!!!\r\n");
		goto init_faild;
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
		//printk("**Kernel** : device tree compatible = %s\r\n",compproStr);	
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
		//printk("**Kernel** : gpio count = %d\r\n",result);	
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
		//printk("**Kernel** : gpio index = %d\r\n",beepInfo.beep_gpio);	
	}

	/* 申请GPIO */
	result = gpio_request(beepInfo.beep_gpio,"beep_bingo-gpio");
	if(result == 0)
	{
		//printk("**Kernel** : gpio request succeed!\r\n");
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
		//printk("**Kernel** : gpio direction output!\r\n");
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


init_faild:
	return result;
}


static int beep_remove(struct platform_device *dev)
{
	gpio_set_value(beepInfo.beep_gpio,1);//关蜂鸣器

	misc_deregister(&miscdev_beep);	

	gpio_free(beepInfo.beep_gpio);

	printk("**Kernel** : exti beep driver succeed!!!\r\n");
}



static const struct of_device_id beep_device_id[] = 
{
	[0] = 
		{
			.compatible = "bingo,beep",			
		},
	[1] = 
		{},//最后一个必须全空
};

static struct platform_driver beep_platform_driver = 
{
	.probe = beep_probe,
	.remove = beep_remove,
	.driver = 
		{
			.name = "platform-beep-bingo",
			.of_match_table = beep_device_id,
		},
};


static int __init beepDriver_init(void)
{
	return platform_driver_register(&beep_platform_driver);
}

static void __exit beepDriver_exit(void)
{
	platform_driver_unregister(&beep_platform_driver);
}


module_init(beepDriver_init);
module_exit(beepDriver_exit);



MODULE_LICENSE("GPL"); 
MODULE_AUTHOR("BINGO");





