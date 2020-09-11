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
#include <linux/spi/spi.h>



struct ft5426_info 
{
	int irq_gpio;
	int rst_gpio;
	int irqIndex;
	struct input_dev*	input_dev;
};











static void ld3320_of_get_deviceTree_info(const struct device_node *node)
{
	


}




static int ld3320_probe(struct spi_device *spi)
{


	



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


