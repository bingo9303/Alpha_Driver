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
#include <linux/i2c.h>
#include <linux/spi/spi.h>
#include <linux/of_irq.h>


#define LD3320_CNT			1
#define LD3320_NAME			"bingoLD3320"



#define LD_RST_H()			gpio_set_value(dev->rst_gpio,1)
#define LD_RST_L()			gpio_set_value(dev->rst_gpio,0)
#define LD_CS_L()			gpio_set_value(spi->cs_gpio,0)
#define LD_CS_H()			gpio_set_value(spi->cs_gpio,1)



struct ld3320_info 
{
	int wr_gpio;
	int irq_gpio;
	int rst_gpio;	
	int irqIndex;
	__u32 major;
	__u32 minor;
	dev_t devid;
	struct class* class;
	struct device* device;
	struct cdev cdev;
	void* private_data;
};
static struct ld3320_info ld3320_dev;

static void ld3320_write_regs(struct spi_device *spi,__u8 startReg,__u8* buff,int len)
{
	struct spi_transfer t[2];
	struct spi_message m;
	struct ld3320_info *dev = (struct ld3320_info *)spi->dev.driver_data;
	__u8 flgaBuff[2] = {0x04,startReg};

	gpio_set_value(dev->wr_gpio,0);	//写使能引脚，不是cs引脚

	spi_message_init(&m);
	memset(t, 0, (sizeof t));

	t[0].tx_buf = flgaBuff;
	t[0].len = 2;
	spi_message_add_tail(&t[0], &m);

	t[1].tx_buf = buff;					
	t[1].len = len;
	spi_message_add_tail(&t[1], &m);

	spi_sync(spi, &m);
}



static void ld3320_read_regs(struct spi_device *spi,__u8 startReg,__u8* buff,int len)
{
	struct spi_transfer t[2];
	struct spi_message m;	
	struct ld3320_info *dev = (struct ld3320_info *)spi->dev.driver_data;
	__u8 flgaBuff[2] = {0x05,startReg};
	
	gpio_set_value(dev->wr_gpio,0);	//写使能引脚，不是cs引脚
	spi_message_init(&m);
	memset(t, 0, (sizeof t));

	t[0].tx_buf = flgaBuff;		//双工换回来的rx_buf没有用，NULL也不会出错
	t[0].len = 2;
	spi_message_add_tail(&t[0], &m);

	t[1].rx_buf = buff;			//去置换的tx_buf随意数据，NULL也不会出错
	t[1].len = len;
	spi_message_add_tail(&t[1], &m);

	spi_sync(spi, &m);
}

static void ld3320_write_one_reg(struct spi_device *spi,__u8 reg,__u8 value)
{
	__u8 data = value;
	ld3320_write_regs(spi,reg,&data,1);
}

static __u8 ld3320_read_one_reg(struct spi_device *spi,__u8 reg)
{
	__u8 data = 0;
	ld3320_read_regs(spi,reg,&data,1);
	return data;
}





static void reset_ld3320(struct spi_device *spi)
{
	struct ld3320_info *dev = (struct ld3320_info *)spi->dev.driver_data;

	LD_RST_H();
	msleep(10);
	LD_RST_L();
	msleep(10);
	LD_RST_H();
	msleep(10);
	LD_CS_L();
	msleep(10);
	LD_CS_H();		
	msleep(10);	
#if 1
	printk("reg 0x06 = %02x\r\n",ld3320_read_one_reg(spi,0x06));
	printk("reg 0x06 = %02x\r\n",ld3320_read_one_reg(spi,0x06));
	printk("reg 0x35 = %02x\r\n",ld3320_read_one_reg(spi,0x35));
	printk("reg 0xb3 = %02x\r\n",ld3320_read_one_reg(spi,0xb3));
#endif
	ld3320_read_one_reg(spi,0x6);
	ld3320_write_one_reg(spi,0x35, 0x33);
	ld3320_write_one_reg(spi,0x1b, 0x55);
	ld3320_write_one_reg(spi,0xb3, 0xaa);
	ld3320_read_one_reg(spi,0x35);
	ld3320_read_one_reg(spi,0x1b);
	ld3320_read_one_reg(spi,0xb3);
#if 1
	printk("/*************/\r\n");	
	printk("reg 0x35 = %02x\r\n",ld3320_read_one_reg(spi,0x35));
	printk("reg 0x1b = %02x\r\n",ld3320_read_one_reg(spi,0x1b));
	printk("reg 0xb3 = %02x\r\n",ld3320_read_one_reg(spi,0xb3));
#endif

	LD_RST_H();
	msleep(10);
	LD_RST_L();
	msleep(10);
	LD_RST_H();
	msleep(10);
	LD_CS_L();
	msleep(10);
	LD_CS_H();		
	ld3320_read_one_reg(spi,0x6);
	msleep(10);
	ld3320_read_one_reg(spi,0x35);
	ld3320_read_one_reg(spi,0x1b);
	ld3320_read_one_reg(spi,0xb3);
}



static void init_ld3320(struct spi_device *spi)
{
	reset_ld3320(spi);	
}



static int ld3320_of_get_deviceTree_info(struct spi_device *spi)
{
	int result;
	struct device_node *of_node = spi->dev.of_node;
	struct ld3320_info *dev = (struct ld3320_info *)spi->dev.driver_data;

	dev->irqIndex = irq_of_parse_and_map(of_node, 0);
	if(dev->irqIndex == 0)
	{
		printk("**Kernel** : get deviceTree irq faild!!!\r\n");
		return -1;
	}

	dev->irq_gpio = of_get_named_gpio(of_node,"int-gpios",0);
	if(gpio_is_valid(dev->irq_gpio))
	{
		result = devm_gpio_request_one(&spi->dev,dev->irq_gpio,GPIOF_IN,"bingo_ld3320_int");
		if(result)
		{
			printk("**Kernel** : request irq io faild!!!\r\n");
			return -EINVAL;
		}
	}
	else
	{
		return -EINVAL;
		printk("**Kernel** : get deviceTree irq faild!!!\r\n");
	}
		
	
	dev->rst_gpio = of_get_named_gpio(of_node,"rst-gpios",0);
	if(gpio_is_valid(dev->rst_gpio))
	{
		result = devm_gpio_request_one(&spi->dev,dev->rst_gpio,GPIOF_OUT_INIT_HIGH,"bingo_ld3320_rst");	
		if(result)
		{
			printk("**Kernel** : request reset io faild!!!\r\n");
			return -EINVAL;
		}
	}
	else
	{
		return -EINVAL;
		printk("**Kernel** : get deviceTree rst faild!!!\r\n");
	}

	dev->wr_gpio = of_get_named_gpio(of_node,"wr-gpios",0);
	if(gpio_is_valid(dev->wr_gpio))
	{
		result = devm_gpio_request_one(&spi->dev,dev->wr_gpio,GPIOF_OUT_INIT_HIGH,"bingo_ld3320_wr");	
		if(result)
		{
			printk("**Kernel** : request wr io faild!!!\r\n");
			return -EINVAL;
		}
	}
	else
	{
		return -EINVAL;
		printk("**Kernel** : get deviceTree wr faild!!!\r\n");
	}


	return 0;
}

static irqreturn_t ld3320_irq_handler(int irq, void *dev_id)
{
	return IRQ_HANDLED;
}

static int ld3320_init_irq(struct spi_device *spi)
{
	int result = 0;
	struct ld3320_info *dev = (struct ld3320_info *)spi->dev.driver_data;

	result = devm_request_threaded_irq(	&spi->dev,
										dev->irqIndex,
										NULL,
										ld3320_irq_handler,
										IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
										"bingo_ld3320_rst",
										spi);
	if(result)
	{
		printk("**Kernel** : request irq handler faild!!!\r\n");
		return -EINVAL;
	}
	return 0;
}


static int ld3320_open(struct inode *inode, struct file *file)
{	
	struct spi_device *spi = NULL;
	file->private_data = ld3320_dev.private_data;
	spi = (struct spi_device *)file->private_data;
	init_ld3320(spi);
	return 0;
}

static ssize_t ld3320_read(struct file *file, char __user *userbuf,
				  size_t count, loff_t *ppos)
{
	return 0;
}

static ssize_t ld3320_write(struct file *file, const char __user *data,
			 						size_t len, loff_t *ppos)
{
	return 0;
}

static int ld3320_release(struct inode *inode, struct file *file)
{
	return 0;
}
const struct file_operations ld3320_fops =
{
	.owner		= THIS_MODULE,
	.open		= ld3320_open,
	.release	= ld3320_release,	
	.read 		= ld3320_read,
	.write		= ld3320_write,
};

static int ld3320_alloc_register_chrdev(struct spi_device *spi)
{
	int result = 0;
	struct ld3320_info *dev = (struct ld3320_info *)spi->dev.driver_data;

	result = alloc_chrdev_region(&dev->devid,0,LD3320_CNT,LD3320_NAME);
	dev->major = MAJOR(dev->devid); //获取主设备号
	dev->minor = MINOR(dev->devid); //获取次设备号

	if(result < 0)
	{
		printk("**Kernel** : region devid faild!!!\r\n");
		goto dev_faild;
	}

	dev->cdev.owner = THIS_MODULE;
	cdev_init(&dev->cdev,&ld3320_fops);
	result = cdev_add(&dev->cdev,dev->devid,LD3320_CNT);

	if(result < 0)
	{
		printk("**Kernel** : init ap3216c driver faild!!!\r\n");
		goto init_faild;
	}

	dev->class = class_create(THIS_MODULE,LD3320_NAME);
	if(IS_ERR(dev->class))
	{
		printk("**Kernel** : create class faild!!!\r\n");
		result = PTR_ERR(dev->class);
		goto class_faild;
	}

	
	dev->device = device_create(dev->class,NULL,dev->devid,NULL,LD3320_NAME);
	if(IS_ERR(dev->device))
	{
		printk("**Kernel** : create device faild!!!\r\n");
		result = PTR_ERR(dev->device);
		goto device_faild;
	}
	return 0;


device_faild:
	class_destroy(dev->class);
class_faild:
	cdev_del(&dev->cdev);	
init_faild:
	unregister_chrdev_region(dev->devid, LD3320_CNT);
dev_faild:
	return result;
}


static int ld3320_probe(struct spi_device *spi)
{
	int result = 0;
	
	dev_set_drvdata(&spi->dev, &ld3320_dev);
	ld3320_dev.private_data = spi;

	result = ld3320_of_get_deviceTree_info(spi);
	if(result)	return -EINVAL;

	result = ld3320_alloc_register_chrdev(spi);
	if(result)	return -EINVAL;
	
	spi->mode = SPI_MODE_2; /*MODE0，CPOL=1，CPHA=0 */
	spi_setup(spi);

	init_ld3320(spi);

	result = ld3320_init_irq(spi);
	if(result) return -EINVAL;

	printk("**Kernel** : probe ap3216c driver succeed!!!\r\n");	
	return 0;
}



static int ld3320_remove(struct spi_device *spi)
{
	struct ld3320_info *dev = (struct ld3320_info *)spi->dev.driver_data;
	/* 1,删除字符设备 */
	cdev_del(&dev->cdev);

	/* 2,注销设备号 */
	unregister_chrdev_region(dev->devid, LD3320_CNT);

	/* 3,摧毁设备 */
	device_destroy(dev->class, dev->devid);

	/* 4,摧毁类 */
	class_destroy(dev->class);
	printk("**Kernel** : exti ap3216c driver succeed!!!\r\n");

	return 0;
}





static const struct spi_device_id ld3320_spi_id[] = {
	{ "bingo,ld3320", 0 },
	{ }
};
MODULE_DEVICE_TABLE(spi, ld3320_spi_id);


static const struct of_device_id ld3320_of_match[] = {
       { .compatible = "bingo,ld3320", },
       { }
};

MODULE_DEVICE_TABLE(of, ld3320_of_match);

static struct spi_driver ld3320_driver = {
	.driver = {
		.name	= "spiDriver_ld3320",
		.owner	= THIS_MODULE,
		.of_match_table = ld3320_of_match,
	},
	.probe = ld3320_probe,
	.remove = ld3320_remove,
	.id_table = ld3320_spi_id,
};

static int __init ld3320_init(void)
{
	return spi_register_driver(&ld3320_driver);
}

static void __exit ld3320_exit(void)
{
	spi_unregister_driver(&ld3320_driver);
}



module_init(ld3320_init);
module_exit(ld3320_exit);


MODULE_LICENSE("GPL"); 
MODULE_AUTHOR("BINGO");


