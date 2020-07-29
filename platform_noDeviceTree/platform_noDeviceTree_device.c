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



#define CCM_CCGR0           (0X020C4068)
#define CCM_CCGR1 			(0X020C406C)
#define CCM_CCGR2 			(0X020C4070)
#define CCM_CCGR3 			(0X020C4074)
#define CCM_CCGR4 			(0X020C4078)
#define CCM_CCGR5 			(0X020C407C)
#define CCM_CCGR6 			(0X020C4080)

#define SW_MUX_GPIO1_IO03 	(0X020E0068)
#define SW_PAD_GPIO1_IO03 	(0X020E02F4)
#define GPIO1_DR 			(0X0209C000)
#define GPIO1_GDIR 			(0X0209C004)

#define REGISTER_LEN		4


static struct resource deviceResource[] = 
{
	[0] = {
			.start = CCM_CCGR1,
			.end = CCM_CCGR1 + REGISTER_LEN - 1,
			.flags = IORESOURCE_MEM,
			},
	[1] = {
			.start = SW_MUX_GPIO1_IO03,
			.end = SW_MUX_GPIO1_IO03 + REGISTER_LEN - 1,
			.flags = IORESOURCE_MEM,
			},
	[2] = {
			.start = SW_PAD_GPIO1_IO03,
			.end = SW_PAD_GPIO1_IO03 + REGISTER_LEN - 1,
			.flags = IORESOURCE_MEM,
			},
	[3] = {
			.start = GPIO1_DR,
			,end = GPIO1_DR + REGISTER_LEN - 1,
			.flags = IORESOURCE_MEM,
			},
	[4] = {
			.start = GPIO1_GDIR,
			.end = GPIO1_GDIR + REGISTER_LEN - 1,
			.flags = IORESOURCE_MEM,
			},
};





static struct platform_device leddevice = 
{
	.name = 
	.resource = deviceResource,
	




};




























