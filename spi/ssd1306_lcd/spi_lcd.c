#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/spi/spi.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/delay.h>

#define DEVICE_NAME "my_lcd"
#define MAJOR_NUM 300
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

static int __init my_init(void){
	int ret;

	ret = register_chrdev(MAJOR_NUM, DEVICE_NUM, &fops);
	if (ret < 0) {
		pr_err("Failed to register char device\n");
	}
	return 0;
}

static void __exit my_exit(void)
{
}

module_init(my_init);
module_exit(my_exit);

/* Meta info */
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Decryptec");
MODULE_DESCRIPTION(("Simple driver for SSD1306 lcd via SPI");
