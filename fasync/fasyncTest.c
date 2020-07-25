#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/io.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/string.h>
#include <linux/irq.h>
#include <asm/mach/map.h>
#include <asm/uaccess.h>
#include <asm/io.h>
#include <linux/interrupt.h>
#include <linux/wait.h>
#include <linux/ide.h>
#include <linux/poll.h>




#define KEY_INTERRUPT_MAJOR		201
#define KEY_INTERRUPT_NAME		"keyFasyncBingo"
#define KEY_INTERRUPT_CNT		1
#define KEY_NUM					1

enum
{
	KEY_VALUE_PRESS=0x10,
	KEY_VALUE_RAISE=0x20,
	KEY_VALUE_NONE=0xFF,
};

struct keyInfo
{
	
	atomic_t keyValue;
	atomic_t pressFlag;		//是否有按下抬起一个完整过程
	int keyGpio;
	int irqIndex;		//中断号
	char name[20];
	irq_handler_t handlerFunc;//中断处理函数
	struct timer_list timer;
};


/* 设备信息结构体 */
struct keyInterruptInfo
{	
	u32 major;
	u32 minor;
	dev_t devid;
	struct class* class;
	struct device* device;
	struct cdev cdev;
	struct device_node *nd;
	struct keyInfo key[KEY_NUM];
	struct fasync_struct * async_queue;
};
struct keyInterruptInfo keyInterruptInfo;

static int key_interrupt_open(struct inode *inode, struct file *file)
{
	file->private_data = (struct keyInterruptInfo*)&keyInterruptInfo;
	return 0;
}

static ssize_t key_interrupt_write(struct file *file, const char __user *data,
			 						size_t len, loff_t *ppos)
{
	return 0;
}

int hasVaildKey(struct keyInterruptInfo* dev)
{
	int i;
	for(i=0;i<KEY_NUM;i++)
	{
		if(atomic_read(&dev->key[i].pressFlag) == KEY_VALUE_RAISE)	//只要有一个按键输入有效
		{
			return 1;
		}
	}
	return 0;
}

static ssize_t key_interrupt_read(struct file *file, char __user *userbuf,
				  size_t count, loff_t *ppos)
{
	int result=0;
	char value=0;
	int i;
	struct keyInterruptInfo* dev = (struct keyInterruptInfo*)file->private_data;


	for(i=0;i<KEY_NUM;i++)
	{
		if(atomic_read(&dev->key[i].pressFlag) == KEY_VALUE_RAISE)
		{
			value |= (1<<i);
			atomic_set(&dev->key[i].pressFlag,KEY_VALUE_NONE);
		}	
	}

 	result = copy_to_user(userbuf,&value,1);
	
	return result;
}



static int key_interrupt_fasync(int fd, struct file *file, int on)
{
	struct keyInterruptInfo *dev = (struct keyInterruptInfo *)file->private_data;

	if (fasync_helper(fd, file, on, &dev->async_queue) < 0)
		return -EIO;

	return 0;
}


static int key_interrupt_release(struct inode *inode, struct file *file)
{
	return key_interrupt_fasync(-1, file, 0);	//释放async_queue结构体
}


				  

const struct file_operations key_interrupt_fops =
{
	.owner		= THIS_MODULE,
	.open		= key_interrupt_open,
	.read		= key_interrupt_read,
	.write      = key_interrupt_write,
	.fasync		= key_interrupt_fasync,
	.release	= key_interrupt_release,
};

static irqreturn_t irq_handler_func_key_0(int irq, void *dev_id)
{
	struct keyInterruptInfo* dev = (struct keyInterruptInfo*)dev_id;
	atomic_set(&dev->key[0].keyValue,__gpio_get_value(dev->key[0].keyGpio));

	/* 激活定时器进行消抖处理 */
	
	if(atomic_read(&dev->key[0].pressFlag) != KEY_VALUE_RAISE)	//上一次按键按下处理完成后才处理下一次按键
	{
		mod_timer(&dev->key[0].timer, (jiffies + msecs_to_jiffies(20)));
	}	
	return IRQ_HANDLED;
}

static void timerKeyCallFunction_key_0(unsigned long data)
{
	int value;
	struct keyInfo * dev = (struct keyInfo*)data;
	value = __gpio_get_value(dev->keyGpio);
	if(value == atomic_read(&dev->keyValue))/* 如果按键状态还是相同 */
	{
		if(value == 0)
		{
			atomic_set(&dev->keyValue,KEY_VALUE_PRESS);
			atomic_set(&dev->pressFlag,KEY_VALUE_PRESS);
		}
		else if(value == 1)
		{
			atomic_set(&dev->keyValue,KEY_VALUE_RAISE);
			if(atomic_read(&dev->pressFlag) == KEY_VALUE_PRESS)
			{
				atomic_set(&dev->pressFlag,KEY_VALUE_RAISE);
				kill_fasync(&keyInterruptInfo.async_queue, SIGIO, POLL_IN);	//向应用程序发送信号
			}
			else
			{
				atomic_set(&dev->pressFlag,KEY_VALUE_NONE);
			}
		}
		else
		{
			atomic_set(&dev->keyValue,KEY_VALUE_NONE);
			atomic_set(&dev->pressFlag,KEY_VALUE_NONE);
		}
	}
	else
	{
		atomic_set(&dev->keyValue,KEY_VALUE_NONE);
		atomic_set(&dev->pressFlag,KEY_VALUE_NONE);
	}

}

static irq_handler_t irqFuncArray[KEY_NUM] = {irq_handler_func_key_0};
static void (*irqTimerArray[KEY_NUM])(unsigned long) = {timerKeyCallFunction_key_0};



static int key_init_irq_io(struct keyInterruptInfo * dev)
{
	int i;
	int result = 0;
	dev->nd = of_find_node_by_path("/key_bingo");
	if(dev->nd == NULL)
	{
		return -EINVAL;
	}

	for(i=0;i<KEY_NUM;i++)
	{
		dev->key[i].keyGpio = -1;
	}
	

	/* 初始化IO */
	for(i=0;i<KEY_NUM;i++)
	{
		atomic_set(&dev->key[i].keyValue,KEY_VALUE_NONE);
		dev->key[i].keyGpio = of_get_named_gpio(dev->nd,"key-gpios",i);
		if(dev->key[i].keyGpio < 0)		return -EINVAL;
		sprintf(dev->key[i].name,"KEY_%d",i);
		if(gpio_request(dev->key[i].keyGpio,dev->key[i].name) != 0)	return -EINVAL;
		if(gpio_direction_input(dev->key[i].keyGpio) != 0)
		{
			result = -EINVAL;
			goto goio_direction_fail;
		}
	}

	/* 初始化中断 */
	for(i=0;i<KEY_NUM;i++)
	{
		dev->key[i].handlerFunc = irqFuncArray[i];
		dev->key[i].irqIndex = irq_of_parse_and_map(dev->nd, i);
		if(request_irq( dev->key[i].irqIndex, 
					 dev->key[i].handlerFunc, 
					 IRQF_TRIGGER_RISING|IRQF_TRIGGER_FALLING, 
					 dev->key[i].name, 
					 dev) != 0)
		{
			result = -EINVAL;
			goto goio_direction_fail;
		}
					 
		//timerBingoInfo.timerInfo.expires = jiffies + msecs_to_jiffies(timerBingoInfo.delayTime);
		dev->key[i].timer.function = irqTimerArray[i];
		dev->key[i].timer.data = (unsigned long)&dev->key[i];
		init_timer(&dev->key[i].timer);
	}

	return result;
	
goio_direction_fail:
	for(i=0;i<KEY_NUM;i++)
	{
		if(dev->key[i].keyGpio > 0)
			gpio_free(dev->key[i].keyGpio);
	}
	return result;
}

static int __init key_interrupt_init(void)
{
	int result=0;
	keyInterruptInfo.major = 0;
	if(keyInterruptInfo.major)
	{
		keyInterruptInfo.devid = MKDEV(keyInterruptInfo.major,0);
		result = register_chrdev_region(keyInterruptInfo.devid,KEY_INTERRUPT_CNT,KEY_INTERRUPT_NAME);
	}
	else
	{
		result = alloc_chrdev_region(&keyInterruptInfo.devid,0,KEY_INTERRUPT_CNT,KEY_INTERRUPT_NAME);
		keyInterruptInfo.major = MAJOR(keyInterruptInfo.devid);	//获取主设备号
		keyInterruptInfo.minor = MINOR(keyInterruptInfo.devid);	//获取次设备号
	}

	if(result < 0)
	{
		printk("**Kernel** : region devid faild!!!\r\n");
		goto dev_faild;
	}

	keyInterruptInfo.cdev.owner = THIS_MODULE;
	cdev_init(&keyInterruptInfo.cdev,&key_interrupt_fops);
	result = cdev_add(&keyInterruptInfo.cdev,keyInterruptInfo.devid,KEY_INTERRUPT_CNT);

	if(result < 0)
	{
		printk("**Kernel** : init led driver faild!!!\r\n");
		goto init_faild;
	}

	keyInterruptInfo.class = class_create(THIS_MODULE,KEY_INTERRUPT_NAME);
	if(IS_ERR(keyInterruptInfo.class))
	{
		printk("**Kernel** : create class faild!!!\r\n");
		result = PTR_ERR(keyInterruptInfo.class);
		goto class_faild;
	}

	
	keyInterruptInfo.device = device_create(keyInterruptInfo.class,NULL,keyInterruptInfo.devid,NULL,KEY_INTERRUPT_NAME);
	if(IS_ERR(keyInterruptInfo.device))
	{
		printk("**Kernel** : create device faild!!!\r\n");
		result = PTR_ERR(keyInterruptInfo.device);
		goto device_faild;
	}

	result = key_init_irq_io(&keyInterruptInfo);
	if(result < 0)
	{
		goto key_init_faild;
	}

	return 0;

key_init_faild:
	device_destroy(keyInterruptInfo.class, keyInterruptInfo.devid);
device_faild:
	class_destroy(keyInterruptInfo.class);
class_faild:
	cdev_del(&keyInterruptInfo.cdev);	
init_faild:
	unregister_chrdev_region(keyInterruptInfo.devid, KEY_INTERRUPT_CNT);
dev_faild:
	return result;
}



static void __exit key_interrupt_exit(void)
{
	int i;
	/* 1,删除字符设备 */
	cdev_del(&keyInterruptInfo.cdev);

	/* 2,注销设备号 */
	unregister_chrdev_region(keyInterruptInfo.devid, KEY_INTERRUPT_CNT);

	/* 3,摧毁设备 */
	device_destroy(keyInterruptInfo.class, keyInterruptInfo.devid);

	/* 4,摧毁类 */
	class_destroy(keyInterruptInfo.class);

	for(i=0;i<KEY_NUM;i++)
	{
		gpio_free(keyInterruptInfo.key[i].keyGpio);
		free_irq(keyInterruptInfo.key[i].irqIndex,&keyInterruptInfo);
		del_timer_sync(&keyInterruptInfo.key[i].timer);
	}
	printk("**Kernel** : exti led driver succeed!!!\r\n");
}


module_init(key_interrupt_init);
module_exit(key_interrupt_exit);



MODULE_LICENSE("GPL"); 
MODULE_AUTHOR("BINGO");










