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


#define KEY_INPUT_NAME		"keyInputBingo"
#define KEY_INPUT_CNT		1
#define KEY_NUM				1

enum
{
	KEY_VALUE_PRESS=0x10,
	KEY_VALUE_RAISE=0x20,
	KEY_VALUE_NONE=0xFF,
};

struct keyInfo
{
	
	atomic_t keyValue;
	int keyGpio;
	int irqIndex;		//中断号
	char name[20];
	irq_handler_t handlerFunc;//中断处理函数
	struct timer_list timer;
	unsigned int keyCode;		//上报输入子系统的按键索引号
};


/* 设备信息结构体 */
struct _keyInputInfo
{	
	u32 major;
	u32 minor;
	dev_t devid;
	struct class* class;
	struct device* device;
	struct cdev cdev;
	struct device_node *nd;
	struct keyInfo key[KEY_NUM];
	wait_queue_head_t r_wait;		//read的等待队列头
	struct input_dev*	input_key_dev;	
};
struct _keyInputInfo keyInputInfo;

static irqreturn_t irq_handler_func_key_0(int irq, void *dev_id)
{
	struct _keyInputInfo* dev = (struct _keyInputInfo*)dev_id;
	atomic_set(&dev->key[0].keyValue,__gpio_get_value(dev->key[0].keyGpio));


	/* 激活定时器进行消抖处理 */
	
	mod_timer(&dev->key[0].timer, (jiffies + msecs_to_jiffies(20)));
	
	return IRQ_HANDLED;
}

static void timerKeyCallFunction_key_0(unsigned long data)
{
	int value;
	struct keyInfo * dev = (struct keyInfo*)data;
	value = __gpio_get_value(dev->keyGpio);
	
	if(value == atomic_read(&dev->keyValue))/* 如果按键状态还是相同 */
	{
		if(value == 0)		//上报linux的输入子系统按键的状态值
		{
			input_report_key(keyInputInfo.input_key_dev,dev->keyCode,1);
			input_sync(keyInputInfo.input_key_dev);
		}
		else if(value == 1)
		{
			input_report_key(keyInputInfo.input_key_dev,dev->keyCode,0);
			input_sync(keyInputInfo.input_key_dev);
		}
		else
		{
			
		}
	}
	else
	{
		
	}
	atomic_set(&dev->keyValue,KEY_VALUE_NONE);
}

static irq_handler_t irqFuncArray[KEY_NUM] = {irq_handler_func_key_0};
static void (*irqTimerArray[KEY_NUM])(unsigned long) = {timerKeyCallFunction_key_0};

static int key_init_irq_io(struct _keyInputInfo * dev)
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

static int __init key_input_init(void)
{
	int result=0;


	keyInputInfo.input_key_dev = input_allocate_device();
	if(keyInputInfo.input_key_dev == NULL)
	{
		result = -EINVAL;
		goto dev_faild;
	}

	keyInputInfo.input_key_dev->name = KEY_INPUT_NAME;
	
	__set_bit(EV_KEY, keyInputInfo.input_key_dev->evbit);
	__set_bit(EV_REP, keyInputInfo.input_key_dev->evbit);
	__set_bit(KEY_0,keyInputInfo.input_key_dev->keybit);
	

	result = input_register_device(keyInputInfo.input_key_dev);
	if(result < 0)
	{
		result = -EINVAL;
		goto init_faild;
	}
	

	result = key_init_irq_io(&keyInputInfo);
	if(result < 0)
	{
		result = -EINVAL;
		goto init_irq_io_faild;
	}

	keyInputInfo.key[0].keyCode = KEY_0;

	return 0;

init_irq_io_faild:
	input_unregister_device(keyInputInfo.input_key_dev);
init_faild:
	input_free_device(keyInputInfo.input_key_dev);
dev_faild:
	return result;
}

static void __exit key_input_exit(void)
{
	int i;
	input_unregister_device(keyInputInfo.input_key_dev);
	input_free_device(keyInputInfo.input_key_dev);

	for(i=0;i<KEY_NUM;i++)
	{
		gpio_free(keyInputInfo.key[i].keyGpio);
		free_irq(keyInputInfo.key[i].irqIndex,&keyInputInfo);
		del_timer_sync(&keyInputInfo.key[i].timer);
	}
	printk("**Kernel** : exti led driver succeed!!!\r\n");
}


module_init(key_input_init);
module_exit(key_input_exit);



MODULE_LICENSE("GPL"); 
MODULE_AUTHOR("BINGO");










