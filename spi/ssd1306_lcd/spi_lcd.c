#include <linux/module.h>
<<<<<<< HEAD
#include <linux/init.h>
#include <linux/spi/spi.h>
#include <linux/delay.h>

=======
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/spi/spi.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/delay.h>

#define DEVICE_NAME "my_lcd"
#define MAJOR_NUM 300
>>>>>>> spi_lcd
static struct spi_device *my_spi_device;

// Register information about slave device;
struct spi_board_info my_spi_device_info = 
{
	.modalias = "spi-ssd1306-driver",
	.max_speed_hz = 10000000, // Minimum clock cycle 100ns, Max = 1/(100*(10^-9))
	.bus_num = 1,
	.chip_select = 0,
	.mode = SPI_MODE_0
};

<<<<<<< HEAD
static int __init my_init(void){
	return 0;
}

static void __exit my_exit(void)
{
}

module_init(my_init);
module_exit(my_exit);
=======
// Device tree
static const struct spi_device_id my_ssd1306_id[] = {
	{"decryptec,my_SSD1306", 0},
	{}
};
MODULE_DEVICE_TABLE(spi, my_ssd1306_id);

static int my_ssd1306_probe(struct spi_device *spi){
	return 0;
}

static void my_ssd1306_remove(struct spi_device *spi){
	
}

static struct spi_driver my_ssd1306_driver = {
	.driver = {
		.name = "my_ssd1306",
		.owner = THIS_MODULE,
	},
	.probe = my_ssd1306_probe,
	.remove = my_ssd1306_remove,
	.it_table = my_ssd1306_id,
};

module_spi_driver(my_ssd1306_driver);
>>>>>>> spi_lcd

/* Meta info */
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Decryptec");
MODULE_DESCRIPTION(("Simple driver for SSD1306 lcd via SPI");
