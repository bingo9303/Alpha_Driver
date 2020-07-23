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




#define TIMER_BINGO_NAME	"timerBingo"
#define TIMER_BINGO_CNT		1

									//0xEF随便找了一个内核中没有用过的类型
#define TIMER_BINGO_CMD_OPEN		_IO(0xEF,0)	//命令从0开始
#define TIMER_BINGO_CMD_CLOSE		_IO(0xEF,1)	
#define TIMER_BINGO_CMD_FREQ		_IOR(0xEF,2,int)



/* 设备信息结构体 */
struct timerBingoInfo
{	
	u32 major;
	u32 minor;
	dev_t devid;
	struct class* class;
	struct device* device;
	struct cdev cdev;
	struct device_node *nd;
	int led_gpio;
	int	delayTime;
	struct timer_list timerInfo;
	int ledStatue;
	spinlock_t	lock;//自旋锁	
};
struct timerBingoInfo timerBingoInfo;


static int timerBingo_open(struct inode *inode, struct file *file)
{
	file->private_data = (struct timerBingoInfo*)&timerBingoInfo;
	return 0;
}

static int timerBingo_release(struct inode *inode, struct file *file)
{
	
	return 0;
}

static ssize_t timerBingo_write(struct file *file, const char __user *data,
			 						size_t len, loff_t *ppos)
{
	return 0;
}

static long timerBingo_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	int result=0;
	unsigned long flags;
	struct timerBingoInfo *dev = (struct timerBingoInfo *)file->private_data;
	
	switch(cmd)
	{
		case TIMER_BINGO_CMD_OPEN:
			{
				mod_timer(&dev->timerInfo, (jiffies + msecs_to_jiffies(dev->delayTime)));
			}
			break;
		case TIMER_BINGO_CMD_CLOSE:
			{
				del_timer(&dev->timerInfo);
			}
			break;
		case TIMER_BINGO_CMD_FREQ:
			{
				spin_lock_irqsave(&dev->lock, flags);
				result = copy_from_user(&dev->delayTime, (int*)arg, sizeof(int));			
				spin_unlock_irqrestore(&dev->lock, flags);
				mod_timer(&dev->timerInfo, (jiffies + msecs_to_jiffies(dev->delayTime)));
			}
			break;
	}
	return result;
}

									

const struct file_operations led_fops =
{
	.owner		= THIS_MODULE,
	.open		= timerBingo_open,
	.release	= timerBingo_release,	
	//.read 	= timerBingo_read,
	.write		= timerBingo_write,
	.unlocked_ioctl = timerBingo_ioctl,
};


static int init_ledBingo(struct timerBingoInfo* dev)
{
	int result;
	dev->nd = of_find_node_by_path("/led_gpio_bingo");
	if(dev->nd == NULL)
	{
		result = -EINVAL;
	}

	/* 获取GPIO编号 */
	dev->led_gpio = of_get_named_gpio(dev->nd,"led_bingo-gpios",0);
	if(dev->led_gpio < 0)
	{
		result = -EINVAL;
	}
	else 
	{
		printk("**Kernel** : gpio index = %d\r\n",dev->led_gpio);	
	}

	/* 申请GPIO */
	result = gpio_request(dev->led_gpio,"led_bingo-gpio");
	if(result == 0)
	{
		printk("**Kernel** : gpio request succeed!\r\n");
	}
	else
	{
		result = -EINVAL;
	}

	/* 设置输出方向,默认为低电平，点亮LED */
	result = gpio_direction_output(dev->led_gpio,dev->ledStatue);
	if(result == 0)
	{
		printk("**Kernel** : gpio direction output!\r\n");
	}
	else 
	{
		gpio_free(dev->led_gpio);
		result = -EINVAL;
	}

	return 0;
}


void timerBingoCallFunction(unsigned long data)
{
	unsigned long flags;
	struct timerBingoInfo* dev = (struct timerBingoInfo*)data;

	spin_lock_irqsave(&dev->lock, flags);//获取自旋锁，准备操作受保护的数据
	
	dev->ledStatue = ~dev->ledStatue;
	
	spin_unlock_irqrestore(&dev->lock, flags);//释放自旋锁

	__gpio_set_value(dev->led_gpio, dev->ledStatue);
	
	//再次激活内核定时器
	mod_timer(&dev->timerInfo, (jiffies + msecs_to_jiffies(dev->delayTime)));
}



static int __init timerBingo_init(void)
{
	int result=0;
	timerBingoInfo.major = 0;
	if(timerBingoInfo.major)
	{
		timerBingoInfo.devid = MKDEV(timerBingoInfo.major,0);
		result = register_chrdev_region(timerBingoInfo.devid,TIMER_BINGO_CNT,TIMER_BINGO_NAME);
	}
	else
	{
		result = alloc_chrdev_region(&timerBingoInfo.devid,0,TIMER_BINGO_CNT,TIMER_BINGO_NAME);
		timerBingoInfo.major = MAJOR(timerBingoInfo.devid);	//获取主设备号
		timerBingoInfo.minor = MINOR(timerBingoInfo.devid);	//获取次设备号
	}

	if(result < 0)
	{
		printk("**Kernel** : region devid faild!!!\r\n");
		goto dev_faild;
	}

	timerBingoInfo.cdev.owner = THIS_MODULE;
	cdev_init(&timerBingoInfo.cdev,&led_fops);
	result = cdev_add(&timerBingoInfo.cdev,timerBingoInfo.devid,TIMER_BINGO_CNT);

	if(result < 0)
	{
		printk("**Kernel** : init led driver faild!!!\r\n");
		goto init_faild;
	}

	timerBingoInfo.class = class_create(THIS_MODULE,TIMER_BINGO_NAME);
	if(IS_ERR(timerBingoInfo.class))
	{
		printk("**Kernel** : create class faild!!!\r\n");
		result = PTR_ERR(timerBingoInfo.class);
		goto class_faild;
	}

	
	timerBingoInfo.device = device_create(timerBingoInfo.class,NULL,timerBingoInfo.devid,NULL,TIMER_BINGO_NAME);
	if(IS_ERR(timerBingoInfo.device))
	{
		printk("**Kernel** : create device faild!!!\r\n");
		result = PTR_ERR(timerBingoInfo.device);
		goto device_faild;
	}

	timerBingoInfo.ledStatue = 0;
	result = init_ledBingo(&timerBingoInfo);
	if(result == 0)
	{
		
	}
	else
	{
		goto gpio_init_faild;
	}

	spin_lock_init(&timerBingoInfo.lock);//初始化自旋锁

	timerBingoInfo.delayTime = 1000;
	timerBingoInfo.timerInfo.expires = jiffies + msecs_to_jiffies(timerBingoInfo.delayTime);
	timerBingoInfo.timerInfo.function = timerBingoCallFunction;
	timerBingoInfo.timerInfo.data = (unsigned long)&timerBingoInfo;
	
	init_timer(&timerBingoInfo.timerInfo);
	add_timer(&timerBingoInfo.timerInfo);
	
	
	return 0;



gpio_init_faild:
	device_destroy(timerBingoInfo.class, timerBingoInfo.devid);
device_faild:
	class_destroy(timerBingoInfo.class);
class_faild:
	cdev_del(&timerBingoInfo.cdev);	
init_faild:
	unregister_chrdev_region(timerBingoInfo.devid, TIMER_BINGO_CNT);
dev_faild:
	return result;
}





static void __exit timerBingo_exit(void)
{

	gpio_set_value(timerBingoInfo.led_gpio,1);//关灯
	

	/* 1,删除字符设备 */
	cdev_del(&timerBingoInfo.cdev);

	/* 2,注销设备号 */
	unregister_chrdev_region(timerBingoInfo.devid, TIMER_BINGO_CNT);

	/* 3,摧毁设备 */
	device_destroy(timerBingoInfo.class, timerBingoInfo.devid);

	/* 4,摧毁类 */
	class_destroy(timerBingoInfo.class);

	gpio_free(timerBingoInfo.led_gpio);

	del_timer(&timerBingoInfo.timerInfo);
	
	printk("**Kernel** : exti led driver succeed!!!\r\n");

}


module_init(timerBingo_init);
module_exit(timerBingo_exit);



MODULE_LICENSE("GPL"); 
MODULE_AUTHOR("BINGO");














