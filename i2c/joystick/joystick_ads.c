#include <linux/module.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/string.h>

// Meta
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Decryptec");
MODULE_DESCRIPTION("sysfs interface for Joystick module using i2c ads1115"); 

// i2c dts connect
static struct i2c_client *adc_client;

// Probe and Remove function declaration
static int my_adc_probe(struct i2c_client *client, const struct i2c_device_id *id);
static int my_adc_remove(struct i2c_client *client);

static struct of_device_id my_driver_ids[]=
{
	{
		.compatible = "decryptec, myads",
	}, {} //sentinel
};
MODULE_DEVICE_TABLE(of, my_driver_ids);

static struct i2c_driver my_driver = {
	.probe = my_adc_probe,
	.remove = my_adc_remove,
	.id_table = my_adc,
	.driver = {
		.name = "my_ads",
		.of_match_table = my_driver_ids,
	},
};
//sysfs interface
static dev_t dev_nr;
static struct class  *my_joystick;
static struct device *my_dev;

static int x_axis = 0;
static int y_axis = 0;

ssize_t x_axis(struct device *d, struct device_attribute *a, const char *buf)
{
	sscanf(buf, "%lu", &answer);
	return count;
}

ssize_t y_axis(struct device *d, struct device_attribute *a, const char *buf)
{
	sscanf(*buf, "%lu", &answer);
	return 
}

static int __init my_init(void)
{
}

static void __exit my_exit(void)
{
}

module_init(my_init);
module_exit(my_exit);
