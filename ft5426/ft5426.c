#include <linux/module.h>
#include <linux/ratelimit.h>
#include <linux/interrupt.h>
#include <linux/input.h>
#include <linux/i2c.h>
#include <linux/uaccess.h>
#include <linux/delay.h>
#include <linux/debugfs.h>
#include <linux/slab.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/input/mt.h>
#include <linux/input/touchscreen.h>
#include <linux/input/edt-ft5x06.h>
#include <linux/i2c.h>
#include <linux/of.h>
#include <linux/of_irq.h>



#define LCD_TOUCH_MAX_X			1024
#define LCD_TOUCH_MAX_Y			600
#define MAX_SUPPORT_POINTS		5
#define FT5X06_READLEN			29			/* 要读取的寄存器个数 */

#define TOUCH_EVENT_DOWN		0x00		/* 按下 */
#define TOUCH_EVENT_UP			0x01		/* 抬起 */
#define TOUCH_EVENT_ON			0x02		/* 接触 */
#define TOUCH_EVENT_RESERVED	0x03		/* 保留 */



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

static void ft5426_reset(struct i2c_client *client)
{
	struct ft5426_info *dev_info = (struct ft5426_info *)client->dev.driver_data;
	if(gpio_is_valid(dev_info->rst_gpio))
	{
		gpio_set_value(dev_info->rst_gpio,0);		
		msleep(5);
		gpio_set_value(dev_info->rst_gpio,1);	
		msleep(300);
	}
}


static irqreturn_t ft5426_irq_handler(int irq, void *dev_id)
{
	__u8 buff[29],i,offset;
	int x,y,type,id,down;
	struct i2c_client *client = (struct i2c_client *)dev_id;
	struct ft5426_info *dev_info = (struct ft5426_info *)client->dev.driver_data;

	memset(buff, 0, sizeof(buff));		/* 清除 */
	ft5426_read_regs(client,FT5X06_TD_STATUS_REG,buff,FT5X06_READLEN);

	offset = 1;

	for(i=0;i<MAX_SUPPORT_POINTS;i++)
	{
		__u8 * dat = &buff[offset+i*6];
		type = dat[0]>>6;
		id = dat[2]>>4;

		/* 开发板上，X和Y的坐标相反 */
		y = ((int)(dat[0]&0x0f)<<8) | (dat[1]);
		x = ((int)(dat[2]&0x0f)<<8) | (dat[3]);

		if(type == TOUCH_EVENT_RESERVED)	continue;

		down = (type != TOUCH_EVENT_UP);		//如果状态不是抬起，就让input子系统内核会自动分配一个ABS_MT_TRACKING_ID 给 slot
												//如果状态是抬起，那么就让子系统删除slot

		input_mt_slot(dev_info->input_dev, id);
		input_mt_report_slot_state(dev_info->input_dev, MT_TOOL_FINGER, down);

		if(!down)	continue;
		
		input_report_abs(dev_info->input_dev, ABS_MT_POSITION_X, x);
		input_report_abs(dev_info->input_dev, ABS_MT_POSITION_Y, y);
	}

	input_mt_report_pointer_emulation(dev_info->input_dev, true);
	input_sync(dev_info->input_dev);
	return IRQ_HANDLED;
}


static int ft5426_of_get_deviceTree_info(struct i2c_client *client)
{
	int result=0;
	struct device_node *of_node = client->dev.of_node;
	struct ft5426_info *dev_info = (struct ft5426_info *)client->dev.driver_data;
	
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
	return 0;
}


static int ft5426_alloc_input_dev(struct i2c_client *client)
{
	int result=0;
	struct ft5426_info *dev_info = (struct ft5426_info *)client->dev.driver_data;
	
	dev_info->input_dev = devm_input_allocate_device(&client->dev);
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
	result = input_mt_init_slots(dev_info->input_dev, MAX_SUPPORT_POINTS, 0);
	if(result)
		printk("**Kernel** : init slots faild!!!\r\n");

	result = input_register_device(dev_info->input_dev);
	if(result)	
	{
		printk("**Kernel** : register input device faild!!!\r\n");
		return -EINVAL;
	}
	
	return 0;
}


static int ft5426_init_irq(struct i2c_client *client)
{
	int result=0;
	struct ft5426_info *dev_info = (struct ft5426_info *)client->dev.driver_data;
	
	//IRQF_ONESHOT表示中断是一次性触发，触发后直到整个中断过程处理结束这段时间内不能再嵌套触发
	result = devm_request_threaded_irq(	&client->dev,
										dev_info->irqIndex,
										NULL,
										ft5426_irq_handler,
										IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
										client->name,
										client);
	if(result)
	{
		printk("**Kernel** : request irq handler faild!!!\r\n");
		return -EINVAL;
	}
	return 0;
}



static void ft5426_reg_init(struct i2c_client *client)
{
	__u8 data[10];
	__u16 libVersion,chipVendor,fimwareId,ctpmVendor;

	ft5426_reset(client);

	ft5426_read_regs(client,FT5426_VERSION_REG,data,8);
	
	libVersion = ((__u16)data[0] << 8)|(data[1]);
	chipVendor = data[2];
	fimwareId = data[5];
	ctpmVendor = data[7];

	printk("Version:0x%04x,Chip vendor ID:0x%02x,Firmware ID:0x%02x,CTPM Vendor ID:0x%02x\r\n",
			libVersion,chipVendor,fimwareId,ctpmVendor);
	
	/* 初始化FT5X06 */
	ft5426_write_one_reg(client, FT5x06_DEVICE_MODE_REG, 0);	/* 进入正常模式	*/
	ft5426_write_one_reg(client, FT5426_IDG_MODE_REG, 1);		/* FT5426中断模式	*/	
}


static int ft5426_probe(struct i2c_client *client,const struct i2c_device_id *id)
{
	int result=0;
	struct ft5426_info *dev_info = NULL;

	//当设备（装置）被拆卸或者驱动（驱动程序）卸载（空载）时，内存会被自动释放。
	dev_info = devm_kzalloc(&client->dev, sizeof(struct ft5426_info), GFP_KERNEL);//
	if (!dev_info)	return -ENOMEM;	

	dev_set_drvdata(&client->dev, dev_info);

	result = ft5426_of_get_deviceTree_info(client);
	if(result)	return -EINVAL;

	ft5426_reg_init(client);

	result = ft5426_alloc_input_dev(client);
	if(result)	return -EINVAL;

	result = ft5426_init_irq(client);
	if(result)	return -EINVAL;
	

	

	return result;
}

static int ft5426_remove(struct i2c_client *client)
{
	struct ft5426_info *dev_info = (struct ft5426_info *)client->dev.driver_data;
	input_unregister_device(dev_info->input_dev);

	return 0;
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















