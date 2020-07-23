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

#define LED_MAJOR	201
#define LED_NAME	"led"
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
};
struct ledInfo ledInfo;

static void __iomem * VIR_CCM_CCGR1 = NULL;
static void __iomem * VIR_SW_MUX_GPIO1_IO03 = NULL;
static void __iomem * VIR_SW_PAD_GPIO1_IO03 = NULL;
static void __iomem * VIR_GPIO1_DR = NULL;
static void __iomem * VIR_GPIO1_GDIR = NULL;



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
		value |= (1 << 3); 				//灭
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





static int __init led_init(void)
{
	int result=0;
	struct device_node * bl_nd = NULL;
	const char * compproStr = NULL;
	unsigned int value = 0;
	u32 regData[10];


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

	
	bl_nd = of_find_node_by_path("/led_bingo");
	if(bl_nd == NULL)
	{
		result = -EINVAL;
		goto find_node_fail;
	}


	result = of_property_read_string(bl_nd,"compatible",&compproStr);
	if(result == 0)	//读取成功
	{
		printk("**Kernel** : device tree compatible = %s\r\n",compproStr);	
	}
	else
	{
		result = -EINVAL;
		goto read_string_fail;
	}
	result = of_property_read_string(bl_nd,"status",&compproStr);
	
	if(result == 0)	//读取成功
	{
		printk("**Kernel** : device tree status = %s\r\n",compproStr);	
	}
	else
	{
		result = -EINVAL;
		goto read_string_fail;
	}

	result = of_property_read_u32_array(bl_nd,"reg",regData,10);
	if(result == 0)
	{
		u32 i;
		printk("**Kernel** :reg data is: ");
		for(i=0;i<10;i++)
		{
			printk("0x%x ",*(regData+i));
		}
		printk("\r\n");
	}
	else
	{
		result = -EINVAL;
		goto read_u32_array_fail;
	}

	VIR_CCM_CCGR1 = ioremap(regData[0],4);
	VIR_SW_MUX_GPIO1_IO03 = ioremap(regData[2],4);
	VIR_SW_PAD_GPIO1_IO03 = ioremap(regData[4],4);
	VIR_GPIO1_DR = ioremap(regData[6],4);
	VIR_GPIO1_GDIR = ioremap(regData[8],4);


	
	value =  readl(VIR_CCM_CCGR1);
	value |= (3 << 26);
	writel(value,VIR_CCM_CCGR1);

	value = 0x05;
	writel(value,VIR_SW_MUX_GPIO1_IO03);

	value = 0x10B0; 
	writel(value,VIR_SW_PAD_GPIO1_IO03);

	value =  readl(VIR_GPIO1_GDIR); //输出
	value |= (1 << 3);
	writel(value,VIR_GPIO1_GDIR);

	printk("**Kernel** : init led driver succeed.Major:%d Minor:%d\r\n",ledInfo.major,ledInfo.minor);
	return result;
	
read_u32_array_fail:
read_string_fail:
find_node_fail:

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
	unsigned int value = 0;
	
	value =  readl(VIR_GPIO1_DR);
	value |= (1 << 3);				//灭
	writel(value,VIR_GPIO1_DR);

	 /* 1,取消地址映射 */
	__arm_iounmap(VIR_CCM_CCGR1);
	__arm_iounmap(VIR_SW_MUX_GPIO1_IO03);
	__arm_iounmap(VIR_SW_PAD_GPIO1_IO03);
	__arm_iounmap(VIR_GPIO1_DR);
	__arm_iounmap(VIR_GPIO1_GDIR);

	/* 1,删除字符设备 */
	cdev_del(&ledInfo.cdev);

	/* 2,注销设备号 */
	unregister_chrdev_region(ledInfo.devid, LED_CNT);

	/* 3,摧毁设备 */
	device_destroy(ledInfo.class, ledInfo.devid);

	/* 4,摧毁类 */
	class_destroy(ledInfo.class);
	
	printk("**Kernel** : exti led driver succeed!!!\r\n");
}





module_init(led_init);
module_exit(led_exit);



MODULE_LICENSE("GPL"); 
MODULE_AUTHOR("BINGO");








