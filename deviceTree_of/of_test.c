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



/*
要读取的设备树信息
backlight {
		compatible = "pwm-backlight";
		pwms = <&pwm1 0 5000000>;
		brightness-levels = <0 4 8 16 32 64 128 255>;
		default-brightness-level = <6>;
		status = "okay";
};
*/



static int __init of_test_init(void)
{
	int result = 0;
	struct device_node * bl_nd = NULL;
	struct property *	 comppro = NULL;
	const char* compproStr = NULL;
	u32 data,dataSize=0;
	u32* ptr = NULL;

	/* 找到backlight的节点 */
	bl_nd = of_find_node_by_path("/backlight");
	if(bl_nd == NULL)
	{
		result = -EINVAL;
		goto find_node_fail;
	}

	/* 获取字符属性 */
	/* 方法1 */
	comppro = of_find_property(bl_nd,"compatible",NULL);
	if(comppro == NULL)
	{
		result = -EINVAL;
		goto find_property_fail;
	}
	else
	{
		printk("1.compatible = %s\r\n",(char*)comppro->value);
	}

	/* 方法2 */
	result = of_property_read_string(bl_nd,"status",&compproStr);
	if(result == 0)	//读取成功
	{
		printk("2.status = %s\r\n",compproStr);	
	}
	else
	{
		result = -EINVAL;
		goto read_string_fail;
	}


	/* 获取数字属性 */
	result = of_property_read_u32(bl_nd,"default-brightness-level",&data);
	if(result == 0)
	{
		printk("3.default-brightness-level = %d\r\n",data);
	}
	else
	{
		result = -EINVAL;
		goto read_u32_fail;
	}

	/* 获取数组 */
	dataSize = of_property_count_elems_of_size(bl_nd,"brightness-levels",sizeof(u32));
	if(dataSize < 0)
	{
		result = -EINVAL;
		goto count_elems_of_size_fail;
	}
	else
	{
		printk("4.brightness-levels size = %d\r\n",dataSize);
	}

	ptr = kmalloc(dataSize*(sizeof(u32)), GFP_KERNEL);
	if(ptr == NULL)
	{
		printk("5.kmalloc faild\r\n");
		result = -EINVAL;
		goto kmalloc_fail;
	}

	result = of_property_read_u32_array(bl_nd,"brightness-levels",ptr, dataSize);
	if(result == 0)
	{
		u32 i;
		printk("6.brightness-levels data is: ");
		for(i=0;i<dataSize;i++)
		{
			printk("%d ",*(ptr+i));
		}
	}
	else
	{
		result = -EINVAL;
		goto read_u32_array_fail;
	}

	result = 0;
	
read_u32_array_fail:
	kfree(ptr);

kmalloc_fail:
count_elems_of_size_fail:
read_u32_fail:	
read_string_fail:
find_property_fail:
find_node_fail:

	return result;
}




static void __exit of_test_exit(void)
{
	


}








module_init(of_test_init);
module_exit(of_test_exit);





MODULE_LICENSE("GPL"); 
MODULE_AUTHOR("BINGO");






