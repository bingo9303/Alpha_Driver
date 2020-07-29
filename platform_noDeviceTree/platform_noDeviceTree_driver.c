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





#define LED_MAJOR	201
#define LED_NAME	"platform_noDeviceTree"
#define LED_CNT		1


static void __iomem * VIR_CCM_CCGR1 = NULL;
static void __iomem * VIR_SW_MUX_GPIO1_IO03 = NULL;
static void __iomem * VIR_SW_PAD_GPIO1_IO03 = NULL;
static void __iomem * VIR_GPIO1_DR = NULL;
static void __iomem * VIR_GPIO1_GDIR = NULL;

/* 设备信息结构�?*/
struct ledInfo
{	
	u32 major;
	u32 minor;
	dev_t devid;
	struct class* class;
	struct device* device;
	struct cdev cdev;
};



struct ledInfo ledInfo;


static int led_open(struct inode *inode, struct file *file)
{
	file->private_data = (struct ledInfo*)&ledInfo;
	return 0;
}


static int led_release(struct inode *inode, struct file *file)
{
	printk("**Kernel** : driver release !!!\r\n");
	return 0;
}

static ssize_t led_write(struct file *file, const char __user *data,
			 						size_t len, loff_t *ppos)
{
	unsigned int value = 0;
	char writeData;
	int result = 0;
	
	result = copy_from_user(&writeData, data, len);
	if(result < 0)
	{
		printk("**Kernel** : write faild!!!\r\n");
		return -1;
	}
	
	value =  readl(VIR_GPIO1_DR);

	if(writeData == 0)
		value &= ~(1 << 3);				//点亮
	else 	
		value |= (1 << 3); 				//�?
	writel(value,VIR_GPIO1_DR);

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

static int led_probe(struct platform_device *dev)
{
	int result = 0;
	int i;
	struct resource* deviceResource[5]={NULL};
	int deviceResourceSize[5];
	unsigned int value = 0;
#if 0
	result = register_chrdev(LED_MAJOR, LED_NAME,&led_fops);
#endif


	/* 获取设备资源 */
	for(i=0;i<5;i++)
	{
		deviceResource[i] = platform_get_resource(dev,IORESOURCE_MEM,i);
		if(deviceResource[i] == NULL)
		{
			dev_err(&dev->dev,"No Index %d Mem Resouece\r\n",i);
			return  -ENXIO;
		}
		deviceResourceSize[i] = resource_size(deviceResource[i]);
	}

	ledInfo.major = 0;

	if(ledInfo.major)
	{
		ledInfo.devid = MKDEV(ledInfo.major,0);	//大部分驱动次设备号从0开�?构建设备�?
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
	if(IS_ERR(ledInfo.class))	//判断指针是否错误，用IS_ERR()
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


	VIR_CCM_CCGR1 = ioremap(deviceResource[0]->start,deviceResourceSize[0]);
	VIR_SW_MUX_GPIO1_IO03 = ioremap(deviceResource[1]->start,deviceResourceSize[1]);
	VIR_SW_PAD_GPIO1_IO03 = ioremap(deviceResource[2]->start,deviceResourceSize[2]);
	VIR_GPIO1_DR = ioremap(deviceResource[3]->start,deviceResourceSize[3]);
	VIR_GPIO1_GDIR = ioremap(deviceResource[4]->start,deviceResourceSize[4]);

	value =  readl(VIR_CCM_CCGR1);
	value |= (3 << 26);
	writel(value,VIR_CCM_CCGR1);

	value = 0x05;
	writel(value,VIR_SW_MUX_GPIO1_IO03);

	value = 0x10B0; 
	writel(value,VIR_SW_PAD_GPIO1_IO03);

	value =  readl(VIR_GPIO1_GDIR);	//输出
	value |= (1 << 3);
	writel(value,VIR_GPIO1_GDIR);

	printk("**Kernel** : init led driver succeed.Major:%d Minor:%d\r\n",ledInfo.major,ledInfo.minor);

	return result;


device_faild:
	class_destroy(ledInfo.class);
class_faild:
	cdev_del(&ledInfo.cdev);
init_faild:
	unregister_chrdev_region(ledInfo.devid, LED_CNT);
dev_faild:
	return result;	
}


static int led_remove(struct platform_device *dev)
{
	unsigned int value = 0;

	value =  readl(VIR_GPIO1_DR);
	value |= (1 << 3);				//�?
	writel(value,VIR_GPIO1_DR);

	/* 1,取消地址映射 */
	__arm_iounmap(VIR_CCM_CCGR1);
	__arm_iounmap(VIR_SW_MUX_GPIO1_IO03);
	__arm_iounmap(VIR_SW_PAD_GPIO1_IO03);
	__arm_iounmap(VIR_GPIO1_DR);
	__arm_iounmap(VIR_GPIO1_GDIR);

#if 0
	unregister_chrdev(LED_MAJOR,LED_NAME);
#endif
	/* 1,删除字符设备 */
	cdev_del(&ledInfo.cdev);

	/* 2,注销设备�?*/
	unregister_chrdev_region(ledInfo.devid, LED_CNT);

	/* 3,摧毁设备 */
	device_destroy(ledInfo.class, ledInfo.devid);

	/* 4,摧毁�?*/
	class_destroy(ledInfo.class);
	
	printk("**Kernel** : exti led driver succeed!!!\r\n");

	return 0;
}


static struct platform_driver led_platform_driver = 
{
	.driver = 
	{
		.name = "led-bingo-platform-gpio",
	},
	.probe = led_probe,
	.remove = led_remove,
};


static int __init ledDriver_init(void)
{
	return platform_driver_register(&led_platform_driver);
}

static void __exit ledDriver_exit(void)
{
	platform_driver_unregister(&led_platform_driver);
}



module_init(ledDriver_init);
module_exit(ledDriver_exit);



MODULE_LICENSE("GPL"); 
MODULE_AUTHOR("BINGO");






