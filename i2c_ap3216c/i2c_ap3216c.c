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



#define AP3216C_CNT		1
#define AP3216C_NAME	"i2c_bingoAp3216c"



#define AP3216C_SYSTEMCONG 	0x00 /* 配置寄存器 */
#define AP3216C_INTSTATUS 	0X01 /* 中断状态寄存器  */
#define AP3216C_INTCLEAR 	0X02 /* 中断清除寄存器 */
#define AP3216C_IRDATALOW 	0x0A /* IR 数据低字节 */
#define AP3216C_IRDATAHIGH 	0x0B /* IR 数据高字节 */
#define AP3216C_ALSDATALOW 	0x0C /* ALS 数据低字节 */
#define AP3216C_ALSDATAHIGH 0X0D /* ALS 数据高字节 */
#define AP3216C_PSDATALOW 	0X0E /* PS 数据低字节 */
#define AP3216C_PSDATAHIGH 	0X0F /* PS 数据高字节 */


struct ap3216cInfo
{
	u32 major;
	u32 minor;
	dev_t devid;
	struct class* class;
	struct device* device;
	struct cdev cdev;
	struct device_node *nd;
    void* private_data;
	__u16 IR;
	__u16 ALS;
	__u16 PS;
};


static struct ap3216cInfo _ap3216cInfo;




//读取多个寄存器
static int ap3216c_read_regs(struct i2c_client *client,__u8 reg,__u8* buf,__u8 len)
{
	int result=0;
	struct i2c_client * dev = client;
	struct i2c_msg msg[2];
	__u8 startReg = reg;
	

	//写入读取的寄存器地址
	msg[0].addr = dev->addr;
	msg[0].flags = 0;
	msg[0].buf = &startReg;
	msg[0].len = 1;
	
	//读取寄存器的数值
	msg[1].addr = dev->addr;
	msg[1].flags = I2C_M_RD;
	msg[1].buf = buf;
	msg[1].len = len;
		
	result = i2c_transfer(dev->adapter,msg,2);

	if(result != 2)
	{
		return -1;
	}

	return 0;
}


//写入多个寄存器数值
static int ap3216c_write_regs(struct i2c_client *client,__u8 startReg,__u8* buf,__u8 len)
{
	int result=0;
	struct i2c_client * dev = client;
	struct i2c_msg msg;
	__u8 msgBuff[256];

	msgBuff[0] = startReg;			//第一个字节为要写入的寄存器首地址
	memcpy(&msgBuff[1],buf,len);	//后面的字节才是要依次写入的数据

	msg.addr = dev->addr;
	msg.flags = 0;
	msg.buf = msgBuff;
	msg.len = len+1;			//要加上寄存器首地址那一个字节

	result = i2c_transfer(dev->adapter,&msg,1);

	if(result != 1)
	{
		return -1;
	}

	return 0;
}


//读取单个寄存器
static __u8 ap3216c_read_OneReg(struct i2c_client *client,__u8 reg)
{
	__u8 value;

	ap3216c_read_regs(client,reg,&value,1);
	
	return value;
}


//写入单个寄存器数值
static void ap3216c_write_OneReg(struct i2c_client *client,__u8 reg,__u8 value)
{
	__u8 dat = value;
	ap3216c_write_regs(client,reg,&dat,1);
}


static int ap3216c_open(struct inode *inode, struct file *file)
{
	struct ap3216cInfo* dev ;
	struct i2c_client * client;
	file->private_data = &_ap3216cInfo;
	dev = (struct ap3216cInfo*)file->private_data;
	client = (struct i2c_client *)(dev->private_data);
	

	ap3216c_write_OneReg(client,AP3216C_SYSTEMCONG,0x04);	//复位芯片
	mdelay(50);
	ap3216c_write_OneReg(client,AP3216C_SYSTEMCONG,0x03);	//三种模式全部开启	
	return 0;
}


static int ap3216c_release(struct inode *inode, struct file *file)
{
	return 0;
}


static ssize_t ap3216c_read(struct file *file, char __user *userbuf,
				  size_t count, loff_t *ppos)
{
	u8 i;
	__u8 readBuff[6]={0};
	__u16 returnBuff[3];
	struct ap3216cInfo* dev = (struct ap3216cInfo*)file->private_data;
	//ap3215c不支持连续读多个寄存器，太辣鸡了
	//ap3216c_read_regs((struct i2c_client *)dev->private_data,AP3216C_IRDATALOW,readBuff,sizeof(readBuff));

	for(i=0;i<6;i++)
	{
		readBuff[i] = ap3216c_read_OneReg((struct i2c_client *)dev->private_data,AP3216C_IRDATALOW+i);		
	}

	if((readBuff[0] & (1<<7)) == 0)
	{
		dev->IR = (readBuff[0]&0x03) | ((__u16)readBuff[1]<<2);
	}

	dev->ALS = readBuff[2] | ((__u16)readBuff[3]<<8);

	if((readBuff[4] & (1<<6)) == 0)
	{
		dev->PS = (readBuff[4]&0x0F) | (((__u16)readBuff[5]&0x3F)<<4);
	}

	returnBuff[0] = dev->IR;
	returnBuff[1] = dev->ALS;
	returnBuff[2] = dev->PS;
	
	return copy_to_user(userbuf,returnBuff,sizeof(returnBuff));
}




const struct file_operations ap3216c_fops =
{
	.owner		= THIS_MODULE,
	.open		= ap3216c_open,
	.release	= ap3216c_release,	
	.read 		= ap3216c_read,
	//.write		= ap3216c_write,
};


static int ap3216c_probe(struct i2c_client *client,const struct i2c_device_id *id)
{
	int result=0;

	_ap3216cInfo.major = 0;
	if(_ap3216cInfo.major)
	{
		_ap3216cInfo.devid = MKDEV(_ap3216cInfo.major,0);
		result = register_chrdev_region(_ap3216cInfo.devid,AP3216C_CNT,AP3216C_NAME);
	}
	else
	{
		result = alloc_chrdev_region(&_ap3216cInfo.devid,0,AP3216C_CNT,AP3216C_NAME);
		_ap3216cInfo.major = MAJOR(_ap3216cInfo.devid);	//获取主设备号
		_ap3216cInfo.minor = MINOR(_ap3216cInfo.devid);	//获取次设备号
	}

	if(result < 0)
	{
		printk("**Kernel** : region devid faild!!!\r\n");
		goto dev_faild;
	}

	_ap3216cInfo.cdev.owner = THIS_MODULE;
	cdev_init(&_ap3216cInfo.cdev,&ap3216c_fops);
	result = cdev_add(&_ap3216cInfo.cdev,_ap3216cInfo.devid,AP3216C_CNT);

	if(result < 0)
	{
		printk("**Kernel** : init ap3216c driver faild!!!\r\n");
		goto init_faild;
	}

	_ap3216cInfo.class = class_create(THIS_MODULE,AP3216C_NAME);
	if(IS_ERR(_ap3216cInfo.class))
	{
		printk("**Kernel** : create class faild!!!\r\n");
		result = PTR_ERR(_ap3216cInfo.class);
		goto class_faild;
	}

	
	_ap3216cInfo.device = device_create(_ap3216cInfo.class,NULL,_ap3216cInfo.devid,NULL,AP3216C_NAME);
	if(IS_ERR(_ap3216cInfo.device))
	{
		printk("**Kernel** : create device faild!!!\r\n");
		result = PTR_ERR(_ap3216cInfo.device);
		goto device_faild;
	}

	_ap3216cInfo.private_data = client;	//把私有数据设置为client

	printk("**Kernel** : probe ap3216c driver succeed!!!\r\n");
	return 0;


device_faild:
	class_destroy(_ap3216cInfo.class);
class_faild:
	cdev_del(&_ap3216cInfo.cdev);	
init_faild:
	unregister_chrdev_region(_ap3216cInfo.devid, AP3216C_CNT);
dev_faild:
	return result;
}



static int ap3216c_remove(struct i2c_client *client)
{
	/* 1,删除字符设备 */
	cdev_del(&_ap3216cInfo.cdev);

	/* 2,注销设备号 */
	unregister_chrdev_region(_ap3216cInfo.devid, AP3216C_CNT);

	/* 3,摧毁设备 */
	device_destroy(_ap3216cInfo.class, _ap3216cInfo.devid);

	/* 4,摧毁类 */
	class_destroy(_ap3216cInfo.class);

	printk("**Kernel** : exti ap3216c driver succeed!!!\r\n");

	return 0;
}




static const struct i2c_device_id ap3216c_i2c_id[] = {
	{ "ap3216c", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, ap3216c_i2c_id);

static const struct of_device_id ap3216c_of_match[] = {
       { .compatible = "bingo,ap3216c", },
       { }
};

MODULE_DEVICE_TABLE(of, ap3216c_of_match);

static struct i2c_driver ap3216c_driver = {
	.driver = {
		.name	= "i2cDriver_ap3216c",
		.owner	= THIS_MODULE,
		.of_match_table = ap3216c_of_match,
	},
	.probe = ap3216c_probe,
	.remove = ap3216c_remove,
	.id_table = ap3216c_i2c_id,
};

static int __init ap3216c_init(void)
{
	return i2c_add_driver(&ap3216c_driver);
}

static void __exit ap3216c_exit(void)
{
	i2c_del_driver(&ap3216c_driver);
}



module_init(ap3216c_init);
module_exit(ap3216c_exit);


MODULE_LICENSE("GPL"); 
MODULE_AUTHOR("BINGO");




