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
#include <asm/uaccess.h>



#define KEY_BINGO_CNT		1
#define KEY_BINGO_NAME		"key_bingo"



/* 设备信息结构体 */
struct keyInfo
{	
	u32 major;
	u32 minor;
	dev_t devid;
	struct class* class;
	struct device* device;
	struct cdev cdev;
	struct device_node *nd;
    int 	key_gpio;
	atomic_t	atomicValue;
};
struct keyInfo keyInfo;

static int key_bingo_open(struct inode *inode, struct file *file)
{
	file->private_data = (struct keyInfo*)&keyInfo;
	return 0;
}

static int key_bingo_release(struct inode *inode, struct file *file)
{	
	return 0;
}

static ssize_t key_bingo_write(struct file *file, const char __user *data,
			 						size_t len, loff_t *ppos)
{
	return 0;
}

static ssize_t key_bingo_read(struct file *file, char __user *userbuf,
				  size_t count, loff_t *ppos)
{
	struct keyInfo* dev = (struct keyInfo*)file->private_data;
	char value = 1;
	
	atomic_set(&dev->atomicValue,__gpio_get_value(dev->key_gpio));
	value = atomic_read(&dev->atomicValue);
	if(value)
	{
		
	}
	else
	{
		//消抖处理


	}
	
	return copy_to_user(userbuf,&value,1);
}








const struct file_operations key_bingo_fops =
{
	.owner		= THIS_MODULE,
	.open		= key_bingo_open,
	.release	= key_bingo_release,	
	.read 		= key_bingo_read,
	.write		= key_bingo_write,
};


static int __init key_bingo_init(void)
{
	
	int result=0;
	const char * compproStr = NULL;

	
	keyInfo.major = 0;
	if(keyInfo.major)
	{
		keyInfo.devid = MKDEV(keyInfo.major,0);
		result = register_chrdev_region(keyInfo.devid,KEY_BINGO_CNT,KEY_BINGO_NAME);
	}
	else
	{
		result = alloc_chrdev_region(&keyInfo.devid,0,KEY_BINGO_CNT,KEY_BINGO_NAME);
		keyInfo.major = MAJOR(keyInfo.devid);	//获取主设备号
		keyInfo.minor = MINOR(keyInfo.devid);	//获取次设备号
	}

	if(result < 0)
	{
		printk("**Kernel** : region devid faild!!!\r\n");
		goto dev_faild;
	}

	keyInfo.cdev.owner = THIS_MODULE;
	cdev_init(&keyInfo.cdev,&key_bingo_fops);
	result = cdev_add(&keyInfo.cdev,keyInfo.devid,KEY_BINGO_CNT);

	if(result < 0)
	{
		printk("**Kernel** : init led driver faild!!!\r\n");
		goto init_faild;
	}

	keyInfo.class = class_create(THIS_MODULE,KEY_BINGO_NAME);
	if(IS_ERR(keyInfo.class))
	{
		printk("**Kernel** : create class faild!!!\r\n");
		result = PTR_ERR(keyInfo.class);
		goto class_faild;
	}

	
	keyInfo.device = device_create(keyInfo.class,NULL,keyInfo.devid,NULL,KEY_BINGO_NAME);
	if(IS_ERR(keyInfo.device))
	{
		printk("**Kernel** : create device faild!!!\r\n");
		result = PTR_ERR(keyInfo.device);
		goto device_faild;
	}


	

	keyInfo.nd = of_find_node_by_path("/key_bingo");
	if(keyInfo.nd == NULL)
	{
		result = -EINVAL;
		goto find_node_fail;
	}

	result = of_property_read_string(keyInfo.nd,"compatible",&compproStr);
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
	result = of_gpio_named_count(keyInfo.nd,"key-gpios");
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
	keyInfo.key_gpio = of_get_named_gpio(keyInfo.nd,"key-gpios",0);
	if(keyInfo.key_gpio < 0)
	{
		result = -EINVAL;
		goto read_string_fail;
	}
	else 
	{
		printk("**Kernel** : gpio index = %d\r\n",keyInfo.key_gpio);	
	}

	/* 申请GPIO */
	result = gpio_request(keyInfo.key_gpio,"key_bingo-gpio");
	if(result == 0)
	{
		printk("**Kernel** : gpio request succeed!\r\n");
	}
	else
	{
		result = -EINVAL;
		goto goio_request_fail;
	}

	/* 设置输入方向 */

	result = gpio_direction_input(keyInfo.key_gpio);
	if(result == 0)
	{
		printk("**Kernel** : gpio direction input!\r\n");
	}
	else 
	{
		result = -EINVAL;
		goto goio_direction_fail;
	}

	atomic_set(&keyInfo.atomicValue,1);//原子变量赋值1


	
	return 0;
	
goio_direction_fail:
	gpio_free(keyInfo.key_gpio);

goio_request_fail:
read_string_fail:	
find_node_fail:

	device_destroy(keyInfo.class, keyInfo.devid);
device_faild:
	class_destroy(keyInfo.class);
class_faild:
	cdev_del(&keyInfo.cdev);	
init_faild:
	unregister_chrdev_region(keyInfo.devid, KEY_BINGO_CNT);
dev_faild:
	return result;
}





static void __exit key_bingo_exit(void)
{
	/* 1,删除字符设备 */
	cdev_del(&keyInfo.cdev);

	/* 2,注销设备号 */
	unregister_chrdev_region(keyInfo.devid, KEY_BINGO_CNT);

	/* 3,摧毁设备 */
	device_destroy(keyInfo.class, keyInfo.devid);

	/* 4,摧毁类 */
	class_destroy(keyInfo.class);

	gpio_free(keyInfo.key_gpio);
	
	printk("**Kernel** : exti led driver succeed!!!\r\n");

}












module_init(key_bingo_init);
module_exit(key_bingo_exit);



MODULE_LICENSE("GPL"); 
MODULE_AUTHOR("BINGO");






