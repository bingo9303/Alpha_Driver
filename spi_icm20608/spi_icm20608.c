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



/* ICM20608寄存器 
 *复位后所有寄存器地址都为0，除了
 *Register 107(0X6B) Power Management 1 	= 0x40
 *Register 117(0X75) WHO_AM_I 				= 0xAF或0xAE
 */
/* 陀螺仪和加速度自测(出产时设置，用于与用户的自检输出值比较） */
#define	ICM20_SELF_TEST_X_GYRO		0x00
#define	ICM20_SELF_TEST_Y_GYRO		0x01
#define	ICM20_SELF_TEST_Z_GYRO		0x02
#define	ICM20_SELF_TEST_X_ACCEL		0x0D
#define	ICM20_SELF_TEST_Y_ACCEL		0x0E
#define	ICM20_SELF_TEST_Z_ACCEL		0x0F

/* 陀螺仪静态偏移 */
#define	ICM20_XG_OFFS_USRH			0x13
#define	ICM20_XG_OFFS_USRL			0x14
#define	ICM20_YG_OFFS_USRH			0x15
#define	ICM20_YG_OFFS_USRL			0x16
#define	ICM20_ZG_OFFS_USRH			0x17
#define	ICM20_ZG_OFFS_USRL			0x18

#define	ICM20_SMPLRT_DIV			0x19
#define	ICM20_CONFIG				0x1A
#define	ICM20_GYRO_CONFIG			0x1B
#define	ICM20_ACCEL_CONFIG			0x1C
#define	ICM20_ACCEL_CONFIG2			0x1D
#define	ICM20_LP_MODE_CFG			0x1E
#define	ICM20_ACCEL_WOM_THR			0x1F
#define	ICM20_FIFO_EN				0x23
#define	ICM20_FSYNC_INT				0x36
#define	ICM20_INT_PIN_CFG			0x37
#define	ICM20_INT_ENABLE			0x38
#define	ICM20_INT_STATUS			0x3A

/* 加速度输出 */
#define	ICM20_ACCEL_XOUT_H			0x3B
#define	ICM20_ACCEL_XOUT_L			0x3C
#define	ICM20_ACCEL_YOUT_H			0x3D
#define	ICM20_ACCEL_YOUT_L			0x3E
#define	ICM20_ACCEL_ZOUT_H			0x3F
#define	ICM20_ACCEL_ZOUT_L			0x40

/* 温度输出 */
#define	ICM20_TEMP_OUT_H			0x41
#define	ICM20_TEMP_OUT_L			0x42

/* 陀螺仪输出 */
#define	ICM20_GYRO_XOUT_H			0x43
#define	ICM20_GYRO_XOUT_L			0x44
#define	ICM20_GYRO_YOUT_H			0x45
#define	ICM20_GYRO_YOUT_L			0x46
#define	ICM20_GYRO_ZOUT_H			0x47
#define	ICM20_GYRO_ZOUT_L			0x48

#define	ICM20_SIGNAL_PATH_RESET		0x68
#define	ICM20_ACCEL_INTEL_CTRL 		0x69
#define	ICM20_USER_CTRL				0x6A
#define	ICM20_PWR_MGMT_1			0x6B
#define	ICM20_PWR_MGMT_2			0x6C
#define	ICM20_FIFO_COUNTH			0x72
#define	ICM20_FIFO_COUNTL			0x73
#define	ICM20_FIFO_R_W				0x74
#define	ICM20_WHO_AM_I 				0x75

/* 加速度静态偏移 */
#define	ICM20_XA_OFFSET_H			0x77
#define	ICM20_XA_OFFSET_L			0x78
#define	ICM20_YA_OFFSET_H			0x7A
#define	ICM20_YA_OFFSET_L			0x7B
#define	ICM20_ZA_OFFSET_H			0x7D
#define	ICM20_ZA_OFFSET_L 			0x7E


#define ICM20608_CNT		1
#define ICM20608_NAME		"spi_bingoIcm20608"


#define USE_CS_GPIO_CUSOTM	0	//CS引脚自己主动控制，不靠spi控制器控制


struct icm20608Info
{
	u32 major;
	u32 minor;
	dev_t devid;
	struct class* class;
	struct device* device;
	struct cdev cdev;
	struct device_node *nd;
    void* private_data;
#if USE_CS_GPIO_CUSOTM
	int cs_gpio;
#endif
	signed int gyro_x_adc;		/* 陀螺仪X轴原始值 	 */
	signed int gyro_y_adc;		/* 陀螺仪Y轴原始值		*/
	signed int gyro_z_adc;		/* 陀螺仪Z轴原始值 		*/
	signed int accel_x_adc;		/* 加速度计X轴原始值 	*/
	signed int accel_y_adc;		/* 加速度计Y轴原始值	*/
	signed int accel_z_adc;		/* 加速度计Z轴原始值 	*/
	signed int temp_adc;		/* 温度原始值 			*/
};


static struct icm20608Info _icm20608Info;


static void icm20608_read_regs(struct icm20608Info *dev,__u8 startReg,u8* buf,int len)
{
	struct spi_transfer t[2];
	struct spi_message m;
	struct spi_device *spi = (struct spi_device *)dev->private_data;
	__u8 regAddr = (1<<7) | startReg;
#if USE_CS_GPIO_CUSOTM
	gpio_set_value(dev->cs_gpio,0);	//片选
#endif

	spi_message_init(&m);
	memset(t, 0, (sizeof t));

	t[0].tx_buf = &regAddr;		//双工换回来的rx_buf没有用，NULL也不会出错
	t[0].len = 1;
	spi_message_add_tail(&t[0], &m);

	t[1].rx_buf = buf;			//去置换的tx_buf随意数据，NULL也不会出错
	t[1].len = len;
	spi_message_add_tail(&t[1], &m);

	spi_sync(spi, &m);

#if USE_CS_GPIO_CUSOTM
	gpio_set_value(dev->cs_gpio,1);	//片选
#endif	
}

static void icm20608_write_regs(struct icm20608Info *dev,__u8 startReg,u8* buf,int len)
{
	struct spi_transfer t[2];
	struct spi_message m;
	struct spi_device *spi = (struct spi_device *)dev->private_data;
	__u8 regAddr = startReg&(~(1<<7));

#if USE_CS_GPIO_CUSOTM
	gpio_set_value(dev->cs_gpio,0); //片选
#endif

	spi_message_init(&m);
	memset(t, 0, (sizeof t));

	t[0].tx_buf = &regAddr;
	t[0].len = 1;
	spi_message_add_tail(&t[0], &m);

	t[1].tx_buf = buf;					
	t[1].len = len;
	spi_message_add_tail(&t[1], &m);

	spi_sync(spi, &m);
	
#if USE_CS_GPIO_CUSOTM
		gpio_set_value(dev->cs_gpio,1); //片选
#endif	
}


static __u8 icm20608_read_oneRegs(struct icm20608Info *dev,__u8 reg)
{
	__u8 value;
	icm20608_read_regs(dev,reg,&value,1);
	return value;
}


static void icm20608_write_oneRegs(struct icm20608Info *dev,__u8 reg,__u8 value)
{
	__u8 buf = value;
	icm20608_write_regs(dev,reg,&buf,1);
}



/*---------------------------------------------------------------------------*/


static void init_icm20608(struct icm20608Info *dev)
{
	icm20608_write_oneRegs(dev, ICM20_PWR_MGMT_1, 0x80);
	mdelay(50);
	icm20608_write_oneRegs(dev, ICM20_PWR_MGMT_1, 0x01);
	mdelay(50);

	printk("**Kernel** : ICM20608 ID:0x%02x\r\n",icm20608_read_oneRegs(dev,ICM20_WHO_AM_I));
	
	icm20608_write_oneRegs(dev, ICM20_SMPLRT_DIV, 0x00); 	/* 输出速率是内部采样率					*/
	icm20608_write_oneRegs(dev, ICM20_GYRO_CONFIG, 0x18); 	/* 陀螺仪±2000dps量程 				*/
	icm20608_write_oneRegs(dev, ICM20_ACCEL_CONFIG, 0x18); 	/* 加速度计±16G量程 					*/
	icm20608_write_oneRegs(dev, ICM20_CONFIG, 0x04); 		/* 陀螺仪低通滤波BW=20Hz 				*/
	icm20608_write_oneRegs(dev, ICM20_ACCEL_CONFIG2, 0x04); /* 加速度计低通滤波BW=21.2Hz 			*/
	icm20608_write_oneRegs(dev, ICM20_PWR_MGMT_2, 0x00); 	/* 打开加速度计和陀螺仪所有轴 				*/
	icm20608_write_oneRegs(dev, ICM20_LP_MODE_CFG, 0x00); 	/* 关闭低功耗 						*/
	icm20608_write_oneRegs(dev, ICM20_FIFO_EN, 0x00);		/* 关闭FIFO						*/
}

void icm20608_readdata(struct icm20608Info *dev)
{
	__u8 data[14];
	icm20608_read_regs(dev, ICM20_ACCEL_XOUT_H, data, 14);

	dev->accel_x_adc = (signed short)((data[0] << 8) | data[1]); 
	dev->accel_y_adc = (signed short)((data[2] << 8) | data[3]); 
	dev->accel_z_adc = (signed short)((data[4] << 8) | data[5]); 
	dev->temp_adc    = (signed short)((data[6] << 8) | data[7]); 
	dev->gyro_x_adc  = (signed short)((data[8] << 8) | data[9]); 
	dev->gyro_y_adc  = (signed short)((data[10] << 8) | data[11]);
	dev->gyro_z_adc  = (signed short)((data[12] << 8) | data[13]);
}




static int icm20608_open(struct inode *inode, struct file *file)
{
	file->private_data = (void*)&_icm20608Info;		
	return 0;
}

static ssize_t icm20608_read(struct file *file, char __user *userbuf,
				  size_t count, loff_t *ppos)
{
	unsigned int data[7];
	struct icm20608Info* dev = (struct icm20608Info*)file->private_data;
	icm20608_readdata(dev);
	
	data[0] = dev->gyro_x_adc;
	data[1] = dev->gyro_y_adc;
	data[2] = dev->gyro_z_adc;
	data[3] = dev->accel_x_adc;
	data[4] = dev->accel_y_adc;
	data[5] = dev->accel_z_adc;
	data[6] = dev->temp_adc;
	
	return copy_to_user(userbuf, data, sizeof(data));;
}

static int icm20608_release(struct inode *inode, struct file *file)
{
	return 0;
}



const struct file_operations icm20608_fops =
{
	.owner		= THIS_MODULE,
	.open		= icm20608_open,
	.release	= icm20608_release,	
	.read 		= icm20608_read,
	//.write		= icm20608_write,
};


static int icm20608_probe(struct spi_device *spi)
{
	int result=0;
#if USE_CS_GPIO_CUSOTM
	struct device_node *nd;
#endif

	_icm20608Info.major = 0;
	if(_icm20608Info.major)
	{
		_icm20608Info.devid = MKDEV(_icm20608Info.major,0);
		result = register_chrdev_region(_icm20608Info.devid,ICM20608_CNT,ICM20608_NAME);
	}
	else
	{
		result = alloc_chrdev_region(&_icm20608Info.devid,0,ICM20608_CNT,ICM20608_NAME);
		_icm20608Info.major = MAJOR(_icm20608Info.devid); //获取主设备号
		_icm20608Info.minor = MINOR(_icm20608Info.devid); //获取次设备号
	}

	if(result < 0)
	{
		printk("**Kernel** : region devid faild!!!\r\n");
		goto dev_faild;
	}

	_icm20608Info.cdev.owner = THIS_MODULE;
	cdev_init(&_icm20608Info.cdev,&icm20608_fops);
	result = cdev_add(&_icm20608Info.cdev,_icm20608Info.devid,ICM20608_CNT);

	if(result < 0)
	{
		printk("**Kernel** : init ap3216c driver faild!!!\r\n");
		goto init_faild;
	}

	_icm20608Info.class = class_create(THIS_MODULE,ICM20608_NAME);
	if(IS_ERR(_icm20608Info.class))
	{
		printk("**Kernel** : create class faild!!!\r\n");
		result = PTR_ERR(_icm20608Info.class);
		goto class_faild;
	}

	
	_icm20608Info.device = device_create(_icm20608Info.class,NULL,_icm20608Info.devid,NULL,ICM20608_NAME);
	if(IS_ERR(_icm20608Info.device))
	{
		printk("**Kernel** : create device faild!!!\r\n");
		result = PTR_ERR(_icm20608Info.device);
		goto device_faild;
	}

	_icm20608Info.private_data = spi;

	
#if USE_CS_GPIO_CUSOTM
	nd = of_find_node_by_path("/soc/aips-bus@02000000/spba-bus@02000000/ecspi@02010000");
	if(nd == NULL)
	{
		printk("**Kernel** : 1.get cs-gpio faild\r\n");	
		result = -EINVAL;
		goto find_node_fail;
	}

	/* 获取gpios里面GPIO数量 */
	result = of_gpio_named_count(nd,"cs-gpio-custom");
	if(result < 0)
	{
		printk("**Kernel** : 2.get cs-gpio faild\r\n");
		result = -EINVAL;			
		goto find_node_fail;
	}
	else 
	{
		printk("**Kernel** : gpio count = %d\r\n",result);	
	}


	/* 获取GPIO编号 */
	_icm20608Info.cs_gpio = of_get_named_gpio(nd,"cs-gpio-custom",0);
	if(_icm20608Info.cs_gpio < 0)
	{
		printk("**Kernel** : 3.get cs-gpio faild\r\n");
		result = -EINVAL;
		goto find_node_fail;
	}

	/* 申请GPIO */
	result = gpio_request(_icm20608Info.cs_gpio,"spi_bingo-cs-gpio");

	/* 设置输出方向,默认为低电平*/
	result = gpio_direction_output(_icm20608Info.cs_gpio,1);
#endif
	spi->mode = SPI_MODE_0; /*MODE0，CPOL=0，CPHA=0 */
	spi_setup(spi);

	init_icm20608(&_icm20608Info);

	printk("**Kernel** : probe ap3216c driver succeed!!!\r\n");	
	return 0;

#if USE_CS_GPIO_CUSOTM	
find_node_fail:

	device_destroy(_icm20608Info.class, _icm20608Info.devid);
#endif
device_faild:
	class_destroy(_icm20608Info.class);
class_faild:
	cdev_del(&_icm20608Info.cdev);	
init_faild:
	unregister_chrdev_region(_icm20608Info.devid, ICM20608_CNT);
dev_faild:
	return result;
}




static int icm20608_remove(struct spi_device *spi)
{
#if USE_CS_GPIO_CUSOTM
	gpio_set_value(_icm20608Info.cs_gpio,1);//释放
#endif

	/* 1,删除字符设备 */
	cdev_del(&_icm20608Info.cdev);

	/* 2,注销设备号 */
	unregister_chrdev_region(_icm20608Info.devid, ICM20608_CNT);

	/* 3,摧毁设备 */
	device_destroy(_icm20608Info.class, _icm20608Info.devid);

	/* 4,摧毁类 */
	class_destroy(_icm20608Info.class);
#if USE_CS_GPIO_CUSOTM
	gpio_free(_icm20608Info.cs_gpio);
#endif
	printk("**Kernel** : exti ap3216c driver succeed!!!\r\n");

	return 0;
}




static const struct spi_device_id icm20608_spi_id[] = {
	{ "icm20608", 0 },
	{ }
};
MODULE_DEVICE_TABLE(spi, icm20608_spi_id);


static const struct of_device_id icm20608_of_match[] = {
       { .compatible = "bingo,icm20608", },
       { }
};

MODULE_DEVICE_TABLE(of, icm20608_of_match);

static struct spi_driver icm20608_driver = {
	.driver = {
		.name	= "spiDriver_icm20608",
		.owner	= THIS_MODULE,
		.of_match_table = icm20608_of_match,
	},
	.probe = icm20608_probe,
	.remove = icm20608_remove,
	.id_table = icm20608_spi_id,
};

static int __init icm20608_init(void)
{
	return spi_register_driver(&icm20608_driver);
}

static void __exit icm20608_exit(void)
{
	spi_unregister_driver(&icm20608_driver);
}



module_init(icm20608_init);
module_exit(icm20608_exit);


MODULE_LICENSE("GPL"); 
MODULE_AUTHOR("BINGO");


