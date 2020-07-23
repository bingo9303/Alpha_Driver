#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/types.h>
#include <linux/delay.h>
#include <linux/ide.h>

static char writeBuffer[100];
static char readBuffer[100];
static char kernelDate[]={"Kernel Send Data"};





static int chrdevbase_open(struct inode *inode, struct file *file)
{
	//printk("load chrdevbase open\r\n");
	return 0;
}

static int chrdevbase_release(struct inode *inode, struct file *file)
{
	//printk("load chrdevbase release\r\n");
	return 0;
}

static ssize_t chrdevbase_read(struct file *file, char __user *buf, size_t len,
                                  loff_t *ppos)
{
	//printk("load chrdevbase read\r\n");
	int result;
	memcpy(readBuffer,kernelDate,sizeof(kernelDate));
	result = copy_to_user(buf, readBuffer, len);
	if(result < 0)
	{
		printk("**Kernel** : read data error\r\n");
	}
	else
	{

	}
	return 0;
}

static ssize_t chrdevbase_write(struct file *file, const char __user *data,
			 						size_t len, loff_t *ppos)
{
	//printk("load chrdevbase write\r\n");
	int result;
	result = copy_from_user(writeBuffer, data, len);
	if(result < 0)
	{
		printk("**Kernel** : write data error\r\n");
	}
	else
	{
		printk("**Kernel** : write data is <%s>",writeBuffer);
	}
	return 0;
}




static struct file_operations chrdevbase_fops = {
	.owner		= THIS_MODULE,
	.open		= chrdevbase_open,
	.release	= chrdevbase_release,	
	.read		= chrdevbase_read,
	.write		= chrdevbase_write,
};


#define CHRDEVBASE_NAME		"chrdevbase"
#define CHRDEVBASE_MAJOR	200


/* 驱动入口函数 */
static int __init chrdevbase_init(void)
{ 
	int ret=0;	
	ret = register_chrdev(CHRDEVBASE_MAJOR, CHRDEVBASE_NAME,&chrdevbase_fops);
	if(ret < 0)
		printk("init chrdevbase faild!!!\r\n");
	else
		printk("init %d chrdevbase succeed!\r\n",CHRDEVBASE_MAJOR);
	
	return ret;		//返回加载函数结果
}


/* 驱动出口函数 */
static void __exit chrdevbase_exit(void)
{
	printk("exit chrdevbase\r\n");
	unregister_chrdev(CHRDEVBASE_MAJOR,CHRDEVBASE_NAME);
}

module_init(chrdevbase_init);	//向内核注册驱动加载函数
module_exit(chrdevbase_exit);	//向内核注册驱动卸载函数



MODULE_LICENSE("GPL"); //添加模块 LICENSE 信息
MODULE_AUTHOR("BINGO"); //添加模块作者信息






