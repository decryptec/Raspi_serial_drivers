#include <linux/module.h>
#include <linux/cdev.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/device.h>
#include <linux/kdev_t.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/err.h>
#include <linux/delay.h>

#define DEVICE_NAME "ssd1306_spi"
static struct spi_device *my_spi_device;

struct my_lcd {
	struct spi_device *spi;
	struct gpio_desc *reset_gpio;
	struct gpio_desc *dc_gpio;
	u8 *framebuffer;
	int width;
	int height;
	struct cdev cdev;
	struct device *device;
};

<<<<<<< HEAD
<<<<<<< HEAD
static int __init my_init(void){
	return 0;
}

static void __exit my_exit(void)
{
}

// Device tree
static const struct spi_device_id my_ssd1306_id[] = {
	{"decryptec,my_SSD1306", 0},
	{}
};
MODULE_DEVICE_TABLE(spi, my_ssd1306_id);

static int my_ssd1306_probe(struct spi_device *spi){
	struct my_lcd *lcd;
	int ret;

	dev_info(&spi->dev, "SSD1306 lcd probe start\n");

	/* Allocate driver data */
	lcd = devm_kzalloc(&spi->dev, sizeof(struct my_lcd), GFP_KERNEL);
	if (!lcd){
		dev_err(&spi->dev, "Failed to allocate driver data\n");
		return -ENOMEM;
	}

	/* Store spi_device */
	lcd->spi = spi;

	/* Setup GPIO referencing device tree */
	lcd->reset_gpio = devm_gpiod_get(&spi->dev, "reset", GPIOD_OUT_HIGH);
	if (IS_ERR(lcd->reset_gpio)){
		dev_err(&spi->dev, "Failed to request reset GPIO\n");
		return -1;
	}

	lcd->dc_gpio = devm_gpiod_get(&spi->dev, "dc", GPIOD_OUT_HIGH);
	if (IS_ERR(lcd->dc_gpio)){
		dev_err(&spi->dev, "Failed to request dc GPIO\n");
		return -1;
	}

	/* Width and Height from device tree */
	ret = of_property_read_u32(spi->dev.of_node, "width", &lcd->width);
	if (ret) {
		dev_err(&spi->dev, "Failed to read 'width' from device tree\n");
		return ret;
	}

	ret = of_property_read_u32(spi->dev.of_node, "height", &lcd->height);
	if (ret) {
		dev_err(&spi->dev, "Failed to read 'height' from device tree\n");
		return ret;
	}

	dev_info(&spi->dev, "Display Width: %d, Height: %d\n", lcd->width, lcd->height);

	/* Allocate Framebuffer */
	lcd->framebuffer = dezm_kazalloc(&spi->dev, (lcd->width * lcd->height / 8), GFP_KERNEL); // 1 bit per pixel
	if (!lcd->framebuffer){
		dev_err(&spi->dev, "Failed to allocate framebuffer\n");
		return -ENOMEM;
	}

	/* SPI Settings */
	spi->mode = SPI_MODE_0;
	spi->bits_per_word = 8;

	ret = of_property_read_u32(spi->dev.of_node, "spi-max-frequency", &spi->max_speed_hz);
	if (ret){
		dev_warn(&spi->dev, "failed to read spi-max-frequncy from device tree, using default\n");
		spi->max_speed_hz = 10000000; // Default 10MHz
	}
	dev_info(&spi->dev, "SPI MAX freq: %d Hz\n", spi->max_speed_hz);

	ret = spi_setup(spi);
	if (ret < 0) {
		dev_err(&spi->dev, "SPI setup failed: %d\n", ret);
		return ret;
	}

	/* Store data in spi_device */
	spi_set_drvdata(spi, lcd);

	/* LCD Init */

	dev_info(&spi->dev, "SSD1306 LCD probe complete\n");

	return 0;
}

static void my_ssd1306_remove(struct spi_device *spi){
	
}

static struct spi_driver my_ssd1306_driver = {
	.driver = {
		.name = "my_ssd1306",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(my_ssd1306_id), // DTO
	},
	.probe = my_ssd1306_probe,
	.remove = my_ssd1306_remove,
	.id_table = my_ssd1306_id,
};

module_spi_driver(my_ssd1306_driver);

/* Meta info */
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Decryptec");
MODULE_DESCRIPTION(("Simple driver for SSD1306 lcd via SPI");
