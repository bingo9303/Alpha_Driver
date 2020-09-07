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
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_gpio.h>
#include <linux/input.h>
#include <linux/semaphore.h>
#include <linux/timer.h>
#include <linux/of_irq.h>
#include <linux/irq.h>
#include <asm/mach/map.h>
#include <asm/uaccess.h>
#include <asm/io.h>


#define LCD_TOUCH_MAX_X			1024
#define LCD_TOUCH_MAX_Y			600
#define MAX_SUPPORT_POINTS		5
#define FT5X06_READLEN			29			/* 要读取的寄存器个数 */



/* FT5X06寄存器相关宏定义 */
#define FT5X06_TD_STATUS_REG	0X02		/*	状态寄存器地址 */
#define FT5x06_DEVICE_MODE_REG	0X00 		/* 模式寄存器 		*/
#define FT5426_IDG_MODE_REG		0XA4		/* 中断模式				*/
#define FT5426_VERSION_REG		0XA1		/* 芯片程序版本号 */



struct ft5426_info 
{
	int irq_gpio;
	int rst_gpio;
	int irqIndex;
	struct input_dev*	input_dev;
	void* private_data;
};



static void ft5426_write_regs(struct i2c_client *client,__u8 startReg,__u8* buff,int len)
{
	struct i2c_client * dev = client;
	struct i2c_msg msg;
	__u8 msgBuff[256];

	msgBuff[0] = startReg;			//第一个字节为要写入的寄存器首地址
	memcpy(&msgBuff[1],buff,len);	//后面的字节才是要依次写入的数据

	msg.addr = dev->addr;
	msg.flags = 0;
	msg.buf = msgBuff;
	msg.len = len+1;			//要加上寄存器首地址那一个字节

	i2c_transfer(dev->adapter,&msg,1);
}

static void ft5426_read_regs(struct i2c_client *client,__u8 startReg,__u8* buff,int len)
{
	struct i2c_client * dev = client;
	struct i2c_msg msg[2];
	
	//写入读取的寄存器地址
	msg[0].addr = dev->addr;
	msg[0].flags = 0;
	msg[0].buf = &startReg;
	msg[0].len = 1;
	
	//读取寄存器的数值
	msg[1].addr = dev->addr;
	msg[1].flags = I2C_M_RD;
	msg[1].buf = buff;
	msg[1].len = len;
		
	i2c_transfer(dev->adapter,msg,2);
}



static void ft5426_write_one_reg(struct i2c_client *client,__u8 reg,__u8 value)
{
	__u8 dat = value;
	ft5426_write_regs(client,reg,&dat,1);
}


static __u8 ft5426_read_one_reg(struct i2c_client *client,__u8 reg)
{
	__u8 dat;
	ft5426_read_regs(client,reg,&dat,1);
	return dat;
}




static irqreturn_t ft5426_irq_handler(int irq, void *dev_id)
{

}


static int ft5426_of_get_deviceTree_info(struct ft5426_info *dev_info)
{
	struct i2c_client *client = (struct i2c_client *)dev_info->private_data;
	struct device_node *of_node = client->dev.of_node;
	dev_info->irq_gpio = of_get_named_gpio(of_node,"int-gpios",0);
	dev_info->rst_gpio = of_get_named_gpio(of_node,"rst-gpios",0);

	if((dev_info->irq_gpio<0)||(dev_info->rst_gpio<0))
	{
		printk("**Kernel** : get deviceTree io faild!!!\r\n");
		return -1;
	}

	dev_info->irqIndex = irq_of_parse_and_map(of_node, 0);
	if(dev_info->irqIndex == 0)
	{
		printk("**Kernel** : get deviceTree irq faild!!!\r\n");
		return -1;
	}
	return 0;
}


static int ft5426_alloc_input_dev(struct ft5426_info* dev_info)
{
	int result=0;
	struct i2c_client *client = (struct i2c_client *)dev_info->private_data;

	dev_info->input_dev = devm_input_allocate_device();
	if(dev_info->input_dev == NULL)	return -ENOMEM;

	dev_info->input_dev->name = client->name;
	dev_info->input_dev->id.bustype = BUS_I2C;
	dev_info->input_dev->dev.parent = &client->dev;

	__set_bit(EV_KEY, 	dev_info->input_dev->evbit);	
	__set_bit(EV_ABS, 	dev_info->input_dev->evbit);	
	__set_bit(BTN_TOUCH,dev_info->input_dev->keybit);	//有效的key值为触摸
	input_set_abs_params(dev_info->input_dev, ABS_X, 0, LCD_TOUCH_MAX_X, 0, 0);
	input_set_abs_params(dev_info->input_dev, ABS_Y, 0, LCD_TOUCH_MAX_Y, 0, 0);
	input_set_abs_params(dev_info->input_dev, ABS_MT_POSITION_X,0, LCD_TOUCH_MAX_X, 0, 0);
	input_set_abs_params(dev_info->input_dev, ABS_MT_POSITION_Y,0, LCD_TOUCH_MAX_Y, 0, 0);
	input_mt_init_slots(dev_info->input_dev, MAX_SUPPORT_POINTS, 0);

	result = input_register_device(dev_info->input_dev);
	if(result)	
	{
		printk("**Kernel** : register input device faild!!!\r\n");
		return -EINVAL;
	}
	
	return 0;
}


static int ft5426_init_io_irq(struct ft5426_info* dev_info)
{
	int result=0;
	struct i2c_client *client = (struct i2c_client *)dev_info->private_data;

	if(gpio_is_valid(dev_info->rst_gpio))
	{
		result = devm_gpio_request_one(&client->dev,dev_info->rst_gpio,GPIOF_OUT_INIT_HIGH,"bingo_tf5426_rst");
		if(result)
		{
			printk("**Kernel** : request reset io faild!!!\r\n");
			return -EINVAL;
		}
	}	
	
	if(gpio_is_valid(dev_info->irq_gpio))
	{
		result = devm_gpio_request_one(&client->dev,dev_info->irq_gpio,GPIOF_IN,"bingo_tf5426_irq");
		if(result)
		{
			printk("**Kernel** : request irq io faild!!!\r\n");
			return -EINVAL;
		}
	}	
	
	//IRQF_ONESHOT表示中断是一次性触发，触发后直到整个中断过程处理结束这段时间内不能再嵌套触发
	result = devm_request_threaded_irq(	&client->dev,
										dev_info->irqIndex,
										NULL,
										ft5426_irq_handler,
										IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
										client->name,
										dev_info);
	if(result)
	{
		printk("**Kernel** : request irq handler faild!!!\r\n");
		return -EINVAL;
	}
	return 0;
}



static void ft5426_init(struct ft5426_info* dev_info)
{
	__u8 data[10];
	__u16 libVersion,chipVendor,fimwareId,ctpmVendor;
	struct i2c_client *client = (struct i2c_client *)dev_info->private_data;

	ft5426_read_regs(client,FT5426_VERSION_REG,data,8);
	
	libVersion = ((__u16)data[0] << 8)|(data[1]);
	chipVendor = data[2];
	fimwareId = data[5];
	ctpmVendor = data[7];

	printk("Version:0x%04x,Chip vendor ID:0x%02x,Firmware ID:0x%02x,CTPM Vendor ID:0x%02x\r\n",
			libVersion,chipVendor,fimwareId,ctpmVendor);
	

	
}


static int ft5426_probe(struct i2c_client *client,const struct i2c_device_id *id)
{
	int result=0;
	struct ft5426_info *dev_info = NULL;

	//当设备（装置）被拆卸或者驱动（驱动程序）卸载（空载）时，内存会被自动释放。
	dev_info = devm_kzalloc(&client->dev, sizeof(ft5426_info), GFP_KERNEL);//
	if (!dev_info)	return -ENOMEM;

	dev_info->private_data = client;

	result = ft5426_of_get_deviceTree_info(dev_info);
	if(result)	return -EINVAL;

	result = ft5426_alloc_input_dev(dev_info);
	if(result)	return -EINVAL;

	result = ft5426_init_io_irq(dev_info);
	if(result)	return -EINVAL;



	return result;
}




static const struct i2c_device_id ft5426_i2c_id[] = {
	{ "bingo,ft5426", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, ft5426_i2c_id);

static const struct of_device_id ft5426_of_match[] = {
       { .compatible = "bingo,ft5426", },
       { }
};

MODULE_DEVICE_TABLE(of, ft5426_of_match);

static struct i2c_driver ft5426_driver = {
	.driver = {
		.name	= "i2cDriver_ft5426",
		.owner	= THIS_MODULE,
		.of_match_table = ft5426_of_match,
	},
	.probe = ft5426_probe,
	.remove = ft5426_remove,
	.id_table = ft5426_i2c_id,
};

static int __init ft5426_init(void)
{
	return i2c_add_driver(&ft5426_driver);
}

static void __exit ft5426_exit(void)
{
	i2c_del_driver(&ft5426_driver);
}



module_init(ft5426_init);
module_exit(ft5426_exit);


MODULE_LICENSE("GPL"); 
MODULE_AUTHOR("BINGO");















