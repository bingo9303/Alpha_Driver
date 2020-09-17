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
#include <linux/of.h>




#define LD3320_CNT			1
#define LD3320_NAME			"bingoLD3320"

///以下三个状态定义用来记录程序是在运行ASR识别还是在运行MP3播放
#define LD_MODE_IDLE			0x00
#define LD_MODE_ASR_RUN			0x08
#define LD_MODE_MP3		 		0x40

///以下五个状态定义用来记录程序是在运行ASR识别过程中的哪个状态
#define LD_ASR_NONE					0x00	//表示没有在作ASR识别
#define LD_ASR_RUNING				0x01	//表示LD3320正在作ASR识别中
#define LD_ASR_FOUNDOK				0x10	//表示一次识别流程结束后，有一个识别结果
#define LD_ASR_FOUNDZERO 			0x11	//表示一次识别流程结束后，没有识别结果
#define LD_ASR_ERROR	 			0x31	//	表示一次识别流程中LD3320芯片内部出现不正确的状态

#define MIC_VOL 0x43


#define CLK_IN   					24/* user need modify this value according to clock in */
#define LD_PLL_11					(__u8)((CLK_IN/2.0)-1)
#define LD_PLL_MP3_19				0x0f
#define LD_PLL_MP3_1B				0x18
#define LD_PLL_MP3_1D   			(__u8)(((90.0*((LD_PLL_11)+1))/(CLK_IN))-1)

#define LD_PLL_ASR_19 				(__u8)(CLK_IN*32.0/(LD_PLL_11+1) - 0.51)
#define LD_PLL_ASR_1B 				0x48
#define LD_PLL_ASR_1D 				0x1f


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
	atomic_t ucRegVal;
	atomic_t nAsrStatus;
	atomic_t nLD_Mode;
	struct tasklet_struct taskletTab;
	void (*taskletFunc)(unsigned long);
	wait_queue_head_t r_wait;		//read的等待队列头
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
#if 0
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
#if 0
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


static void LD_Init_Common(struct spi_device *spi)
{
	struct ld3320_info *dev = (struct ld3320_info *)spi->dev.driver_data;
	
	ld3320_read_one_reg(spi,0x06);  
	ld3320_write_one_reg(spi,0x17, 0x35); 
	msleep(5);
	ld3320_read_one_reg(spi,0x06);  

	ld3320_write_one_reg(spi,0x89, 0x03);  
	msleep(5);
	ld3320_write_one_reg(spi,0xCF, 0x43);   
	msleep(5);
	ld3320_write_one_reg(spi,0xCB, 0x02);
	
	/*PLL setting*/
	ld3320_write_one_reg(spi,0x11, LD_PLL_11);       
	if (atomic_read(&dev->nLD_Mode) == LD_MODE_MP3)
	{
		ld3320_write_one_reg(spi,0x1E, 0x00); 
		ld3320_write_one_reg(spi,0x19, LD_PLL_MP3_19);   
		ld3320_write_one_reg(spi,0x1B, LD_PLL_MP3_1B);   
		ld3320_write_one_reg(spi,0x1D, LD_PLL_MP3_1D);
	}
	else
	{
		ld3320_write_one_reg(spi,0x1E,0x00);
		ld3320_write_one_reg(spi,0x19, LD_PLL_ASR_19); 
		ld3320_write_one_reg(spi,0x1B, LD_PLL_ASR_1B);		
	  	ld3320_write_one_reg(spi,0x1D, LD_PLL_ASR_1D);
	}
	msleep(5);
	
	ld3320_write_one_reg(spi,0xCD, 0x04);
	ld3320_write_one_reg(spi,0x17, 0x4c); 
	msleep(5);
	ld3320_write_one_reg(spi,0xB9, 0x00);
	ld3320_write_one_reg(spi,0xCF, 0x4F); 
	ld3320_write_one_reg(spi,0x6F, 0xFF); 
}

static void init_ld3320(struct spi_device *spi)
{
	reset_ld3320(spi);	
}


static __u8 LD_GetResult(struct spi_device *spi)
{
	return ld3320_read_one_reg(spi,0xc5);
}


static void LD_Init_ASR(struct spi_device *spi)
{
	struct ld3320_info *dev = (struct ld3320_info *)spi->dev.driver_data;
	atomic_set(&dev->nLD_Mode,LD_MODE_ASR_RUN);
	
	LD_Init_Common(spi);

	ld3320_write_one_reg(spi,0xBD, 0x00);
	ld3320_write_one_reg(spi,0x17, 0x48);	
	msleep(5);
	ld3320_write_one_reg(spi,0x3C, 0x80);    
	ld3320_write_one_reg(spi,0x3E, 0x07);
	ld3320_write_one_reg(spi,0x38, 0xff);    
	ld3320_write_one_reg(spi,0x3A, 0x07);
	ld3320_write_one_reg(spi,0x40, 0);          
	ld3320_write_one_reg(spi,0x42, 8);
	ld3320_write_one_reg(spi,0x44, 0);    
	ld3320_write_one_reg(spi,0x46, 8); 
	msleep(1);
}


static void LD_AsrStart(struct spi_device *spi)
{
	LD_Init_ASR(spi);
}



static __u8 LD_Check_ASRBusyFlag_b2(struct spi_device *spi)
{
	__u8 j;
	__u8 flag = 0;
	for (j=0; j<10; j++)
	{
		if (ld3320_read_one_reg(spi,0xb2) == 0x21)
		{
			flag = 1;
			break;
		}
		msleep(10);		
	}
	return flag;
}


static __u8 LD_AsrAddFixed(struct spi_device *spi,const char* buff,__u8 len)
{
	u8 index;
	int k, i,flag,len;

	flag = 1;
	for (k=0; k<len; k++)
	{			
		const char*  sRecog = buff[k];

		if((sRecog[0] == '<')&&(sRecog[4] == '>'))
		{
			const char indexStr[4] = {sRecog[1],sRecog[2],sRecog[3],'\0'};
			index = atoi(indexStr);
			if((index == 0)||(index > 0xFF))
			{
				printk("**Kernel** : error cmd format: %s\r\n",sRecog);
				continue;
			}
		}
		else
		{
			printk("**Kernel** : error cmd format: %s\r\n",sRecog);
			continue;
		}
		
		if(LD_Check_ASRBusyFlag_b2(spi) == 0)
		{
			flag = 0;
			break;
		}
		

		ld3320_write_one_reg(spi,0xc1, index);
		ld3320_write_one_reg(spi,0xc3, 0);
		ld3320_write_one_reg(spi,0x08, 0x04);
		msleep(1);
		ld3320_write_one_reg(spi,0x08, 0x00);
		msleep(1);

		len = strlen(sRecog);
		
		for(i=0;i<len;i++)
		{
			if(sRecog[i] == '\0')
			{
				break;
			}
			ld3320_write_one_reg(spi,0x5, sRecog[i]);
		}

		ld3320_write_one_reg(spi,0xb9, len);
		ld3320_write_one_reg(spi,0xb2, 0xff);
		ld3320_write_one_reg(spi,0x37, 0x04);
	}	 
	return flag;
}

static __u8 LD_AsrRun(struct spi_device *spi)
{
	ld3320_write_one_reg(spi,0x35, MIC_VOL);
	ld3320_write_one_reg(spi,0x1C, 0x09);
	ld3320_write_one_reg(spi,0xBD, 0x20);
	ld3320_write_one_reg(spi,0x08, 0x01);
	msleep( 5 );
	ld3320_write_one_reg(spi,0x08, 0x00);
	msleep( 5);

	if(LD_Check_ASRBusyFlag_b2(spi) == 0)
	{
		return 0;
	}

	ld3320_write_one_reg(spi,0xB2, 0xff);	
	ld3320_write_one_reg(spi,0x37, 0x06);
	ld3320_write_one_reg(spi,0x37, 0x06);
	msleep(5);
	ld3320_write_one_reg(spi,0x1C, 0x0b);
	ld3320_write_one_reg(spi,0x29, 0x10);
	ld3320_write_one_reg(spi,0xBD, 0x00);   
	return 1;
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


void taskletFunc(unsigned long data)
{
	__u8 nAsrResCount=0;
	struct spi_device *spi = (struct spi_device *)data;
	struct ld3320_info *dev = (struct ld3320_info *)spi->dev.driver_data;

	

	if((atomic_read(&dev->ucRegVal) & 0x10) && ld3320_read_one_reg(spi,0xb2)==0x21 && ld3320_read_one_reg(spi,0xbf)==0x35)		
	{	 
		nAsrResCount = ld3320_read_one_reg(spi,0xba);

		if(nAsrResCount>0 && nAsrResCount<=4) 
		{
			atomic_set(&dev->nAsrStatus,LD_ASR_FOUNDOK);
			wake_up(&dev->r_wait);		//唤醒	
		}
		else
		{
			atomic_set(&dev->nAsrStatus,LD_ASR_FOUNDZERO);
		}
	}
	else
	{
		atomic_set(&dev->nAsrStatus,LD_ASR_FOUNDZERO);
	}

	ld3320_write_one_reg(spi,0x2b,0);
	ld3320_write_one_reg(spi,0x1C,0);//写0:ADC不可用
	ld3320_write_one_reg(spi,0x29,0);
	ld3320_write_one_reg(spi,0x02,0);
	ld3320_write_one_reg(spi,0x2B,0);
	ld3320_write_one_reg(spi,0xBA,0);	
	ld3320_write_one_reg(spi,0xBC,0);	
	ld3320_write_one_reg(spi,0x08,1);//清除FIFO_DATA
	ld3320_write_one_reg(spi,0x08,0);//清除FIFO_DATA后 再次写0
}


static irqreturn_t ld3320_irq_handler(int irq, void *dev_id)
{
	struct spi_device *spi = (struct spi_device *)dev_id;
	struct ld3320_info *dev = (struct ld3320_info *)spi->dev.driver_data;
	
	atomic_set(&dev->ucRegVal,ld3320_read_one_reg(spi,0x2B));

// 语音识别产生的中断
//（有声音输入，不论识别成功或失败都有中断）
	ld3320_write_one_reg(spi,0x29,0);
	ld3320_write_one_reg(spi,0x02,0);

	/* 调度tasklet */
	tasklet_schedule(&dev->taskletTab);
	return IRQ_HANDLED;
}

static int ld3320_init_irq(struct spi_device *spi)
{
	int result = 0;
	struct ld3320_info *dev = (struct ld3320_info *)spi->dev.driver_data;

	
	dev->taskletFunc = taskletFunc;
	tasklet_init(&dev->taskletTab,dev->taskletFunc, (unsigned long)spi);

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
	__u8 nAsrRes;
	int result=0;
	struct spi_device *spi = (struct spi_device *)file->private_data;
	struct ld3320_info *dev = (struct ld3320_info *)spi->dev.driver_data;

	
	DECLARE_WAITQUEUE(wait,current);	/* 定义一个等待队列项 */		    
	if(atomic_read(&dev->nAsrStatus) == LD_ASR_FOUNDOK)
	{
		goto valid_asr;
	}

	add_wait_queue(&dev->r_wait,&wait);	//将等待队列项添加到等待队列头
	__set_current_state(TASK_INTERRUPTIBLE);	//当前进程设置为可被信号打断的状态
	schedule();                     /* 切换 */


	/* 唤醒后，程序从这里继续执行 */
	if(signal_pending(current))		//判断是信号唤醒还是中断唤醒
	{
		result = -ERESTARTSYS;
		goto exit_onqueue;
	}

	
valid_asr:
	nAsrRes = LD_GetResult(spi);//一次ASR识别流程结束，去取ASR识别结果										 
	printk("\r\n识别码:%d", nAsrRes);			 		

	result = copy_to_user(userbuf,&nAsrRes,1);	
	
exit_onqueue:
	__set_current_state(TASK_RUNNING);      /* 将当前任务设置为运行状态 */
	remove_wait_queue(&dev->r_wait, &wait);    /* 将对应的队列项从等待队列头删除 */

	atomic_set(&dev->nAsrStatus,LD_ASR_NONE);
	return result;
}

static ssize_t ld3320_write(struct file *file, const char __user *data,
			 						size_t len, loff_t *ppos)
{
	__u8 i=0;
	__u8 asrflag=0;

	struct spi_device *spi = (struct spi_device *)file->private_data;
	struct ld3320_info *dev = (struct ld3320_info *)spi->dev.driver_data;

	
	for (i=0; i<5; i++)		//防止由于硬件原因导致LD3320芯片工作不正常，所以一共尝试5次启动ASR识别流程
	{
		LD_AsrStart(spi);			//初始化ASR
		msleep(100);
		if (LD_AsrAddFixed(spi,data,len)==0)	//添加关键词语到LD3320芯片中
		{
			reset_ld3320(spi);				//LD3320芯片内部出现不正常，立即重启LD3320芯片
			msleep(50);	//并从初始化开始重新ASR识别流程
			continue;
		}
		msleep(10);
		if (LD_AsrRun(spi) == 0)
		{
			reset_ld3320(spi);			 //LD3320芯片内部出现不正常，立即重启LD3320芯片
			msleep(50);//并从初始化开始重新ASR识别流程
			continue;
		}
		asrflag=1;
		break;						//ASR流程启动成功，退出当前for循环。开始等待LD3320送出的中断信号
	}	

	atomic_set(&dev->nAsrStatus,LD_ASR_RUNING);
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

	atomic_set(&ld3320_dev.nAsrStatus,LD_ASR_NONE);
	init_waitqueue_head(&ld3320_dev.r_wait);	//初始化等待队列头

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


