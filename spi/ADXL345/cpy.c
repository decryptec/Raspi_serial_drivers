#include "ADXL345_spi.h"

/* META INFO */
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Decryptec");
MODULE_DESCRIPTION("ADXL345 driver using cdev, sysfs, and Interrupt");

// sysfs - Measuring range
static ssize_t range_show(struct device *dev, struct device_attribute *attr, char *buf) {
	struct my_ADXL345 *adxl = dev_get_drvdata(dev);
	return sprintf(buf, "%d\n", adxl->range);
}

static ssize_t range_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count) {
	struct my_ADXL345 *adxl = dev_get_drvdata(dev);
	int new_range;
	int ret;
	uint8_t data_format;

	ret = kstrtoint(buf, 0, &new_range);
	if (ret)
		return ret;

	if (new_range != 2 && new_range != 4 && new_range != 8 && new_range != 16) {
		return -EINVAL;
	}

	adxl->range = new_range;

	switch (new_range) {
		case 2:
			data_format = 0x08; // +/- 2g
			break;
		case 4:
			data_format = 0x09; // +/- 4g
			break;
		case 8:
			data_format = 0x0A; // +/- 8g
			break;
		case 16:
			data_format = 0x0B; // +/- 16g
			break;
		default:
			return -EINVAL;
	}

	ret = write_reg(adxl->spi, REG_DATA_FORMAT, data_format);
	if (ret)
		return ret;
	return count;
}

// sysfs - Sampling rate
static ssize_t rate_show(struct device *dev, struct device_attribute *attr, char *buf) {
	struct my_ADXL345 *adxl = dev_get_drvdata(dev);
	return sprintf(buf, "%d\n", adxl->rate);
}

static ssize_t rate_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count) {
	struct my_ADXL345 *adxl = dev_get_drvdata(dev);
	int new_rate;
	int ret;
	uint8_t bw_rate;

	ret = kstrtoint(buf, 0, &new_rate);
	if (ret) {
		return ret;
	}

	if (new_rate <= 0) {
		return -EINVAL;
	}

	adxl->rate = new_rate;

	/* Write to BW_RATE register */
	if (new_rate <= 6)
		bw_rate = 0x06;
	else if (new_rate <= 16)
		bw_rate = 0x07;
	else if (new_rate <= 25)
		bw_rate = 0x08;
	else if (new_rate <= 50)
		bw_rate = 0x09;
	else if (new_rate <= 100)
		bw_rate = 0x0A;
	else if (new_rate <= 200)
		bw_rate = 0x0B;
	else if (new_rate <= 400)
		bw_rate = 0x0C;
	else
		bw_rate = 0x0D;

	ret = write_reg(adxl->spi, REG_BW_RATE, bw_rate);
	if (ret) {
		return ret;
	}

	return count;
}

static DEVICE_ATTR(range, S_IRUGO | S_IWUSR, range_show, range_store);
static DEVICE_ATTR(rate, S_IRUGO | S_IWUSR, rate_show, rate_store);

// Char Device file operations
static int adxl345_open(struct inode *inode, struct file *file) {
	struct my_ADXL345 *adxl = container_of(inode->i_cdev, struct my_ADXL345, cdev);
	file->private_data = adxl;

	return 0;
}

static int adxl345_release(struct inode *inode, struct file *file) {
	return 0;
}

static ssize_t adxl345_read(struct file *file, char __user *ubuf, size_t count, loff_t *ppos) {
	struct my_ADXL345 *adxl = (struct my_ADXL345 *)file->private_data;
	char *buffer;
	int len = 0;
	loff_t offset = *ppos;

	buffer = kmalloc(256, GFP_KERNEL);
	if (!buffer)
	{
		return -ENOMEM;
	}
	
	dev_info(adxl->dev, "READ: Attempting lock\n");
	mutex_lock(&adxl->lock); // Protect read access to x,y,z
	dev_info(adxl->dev, "READ: Lock acquired\n");
	int ret = get_data(adxl);
	if (ret) {
		dev_err(&adxl->spi->dev, "Error reading data\n");
	} 
	len = sprintf(buffer, "X: %d\nY: %d\nZ: %d\n", adxl->x, adxl->y, adxl->z);

	dev_info(adxl->dev, "READ: Releasing lock\n");
	mutex_unlock(&adxl->lock);
	dev_info(adxl->dev, "READ: Lock released\n");

	if (offset >= len || count == 0){
		kfree(buffer);
		return 0;
	}

	if (count > len - offset)
	{
		count = len - offset;
	}

	if (copy_to_user(ubuf, buffer + offset, count)) {
		kfree(buffer);
		return -EFAULT;
	}

	*ppos += count;
	kfree(buffer);
	return count;
}

static const struct file_operations adxl345_fops = {
	.owner = THIS_MODULE,
	.open = adxl345_open,
	.release = adxl345_release,
	.read = adxl345_read,
};

static int read_reg(struct spi_device *spi, uint8_t reg, uint8_t *val) {
	int ret;
	uint8_t tx_buf[2], rx_buf[2];

	tx_buf[0] = reg | 0x80; // Read (MSB = 1)
	tx_buf[1] = 0x00;

	struct spi_transfer xfer[] = {
		{
			.tx_buf = tx_buf,
			.rx_buf = rx_buf,
			.len = 2,
			.delay.value = 5,
			.delay.unit = SPI_DELAY_UNIT_USECS,
		},
	};

	struct spi_message msg;
	spi_message_init(&msg);
	spi_message_add_tail(xfer, &msg);

	ret = spi_sync(spi, &msg);
	if (ret) {
		dev_err(&spi->dev, "SPI read failed: %d\n", ret);
		return ret;
	}

	*val = rx_buf[1];
	return 0;
}

static int write_reg(struct spi_device *spi, uint8_t reg, uint8_t val) {
	int ret;
	uint8_t tx_buf[2];

	tx_buf[0] = reg & ~0x80; // Write (MSB = 0)
	tx_buf[1] = val;

	struct spi_transfer xfer[] = {
		{
			.tx_buf = tx_buf,
			.len = 2,
			.delay.value = 5,
			.delay.unit = SPI_DELAY_UNIT_USECS,
		},
	};

	struct spi_message msg;
	spi_message_init(&msg);
	spi_message_add_tail(xfer, &msg);

	ret = spi_sync(spi, &msg);
	if (ret) {
		dev_err(&spi->dev, "SPI Write failed: %d\n", ret);
		return ret;
	}

	return 0;
}

// Get acceleration data
static int get_data(struct my_ADXL345 *adxl) {
	int ret;
	uint8_t tx_buf[7], rx_buf[7];

	tx_buf[0] = REG_DATAX0 | 0x80 | 0x40;
	memset(&tx_buf[1], 0 , 6);

	struct spi_transfer xfer[] = {
		{
			.tx_buf = tx_buf,
			.rx_buf = rx_buf,
			.len = 7,
			.delay.value = 5,
			.delay.unit = SPI_DELAY_UNIT_USECS,
		},
	};

	struct spi_message msg;
	spi_message_init(&msg);
	spi_message_add_tail(xfer, &msg);

	ret = spi_sync(adxl->spi, &msg);
	if (ret) {
		dev_err(&adxl->spi->dev, "SPI Acceleration read failed: %d\n", ret);
		return ret;
	}

	if (ret == 0) { // Only print if spi_sync was okay
		int i;
		char temp_buf[6 * 5 + 1]; // Enough for "0xXX 0xXX ..."
		char *ptr = temp_buf;
		for (i = 0; i < 7; i++) { // Print all 7 received bytes
			ptr += sprintf(ptr, "0x%02x ", rx_buf[i]);
		}
		dev_info(&adxl->spi->dev, "get_data raw rx_buf: %s\n", temp_buf);
	}

	adxl->x = (int16_t)((rx_buf[2] << 8) | rx_buf[1]);
	adxl->y = (int16_t)((rx_buf[4] << 8) | rx_buf[3]);
	adxl->z = (int16_t)((rx_buf[6] << 8) | rx_buf[5]);

	return 0;
}

// Interrupt handler function
static irqreturn_t irq_handler(int irq, void *dev_id) {
	struct my_ADXL345 *adxl = (struct my_ADXL345 *)dev_id;
	uint8_t int_source;
	int ret;

	//Read INT source register to clear
	ret = read_reg(adxl->spi, REG_INT_SOURCE, &int_source);
	if (ret){
		dev_err(&adxl->spi->dev, "Error reading INT controller: %d\n", ret);
		return IRQ_NONE;
	}

	dev_info(&adxl->spi->dev, "Interrupt triggered. Source: 0x%02x\n", int_source);

	mutex_lock(&adxl->lock); // Protect access to x,y,z

	// Check interrupt bits
	if (int_source & INT_DATA_READY){
		ret = get_data(adxl);
		if (ret) {
			dev_err(&adxl->spi->dev, "Error reading data\n");
		}
		dev_info(&adxl->spi->dev, "Data Ready Interrupt. X: %d, Y: %d, Z: %d\n", adxl->x, adxl->y, adxl->z);
	}
	else if (int_source & INT_SINGLE_TAP || int_source & INT_DOUBLE_TAP){
		dev_info(&adxl->spi->dev, "Tap Interrupt!\n");
	}

	mutex_unlock(&adxl->lock);

	return IRQ_HANDLED;
}

// SPI probe
static int adxl345_probe(struct spi_device *spi) {
	struct my_ADXL345 *adxl;
	uint8_t devid;
	int ret;
	uint8_t data_format;
	int irq_num;

	dev_info(&spi->dev, "Probing ADXL345\n");

	// Allocate memory for dev struct
	adxl = devm_kzalloc(&spi->dev, sizeof(*adxl), GFP_KERNEL);
	if (!adxl)
		return -ENOMEM;

	adxl->spi = spi;
	spi_set_drvdata(spi, adxl);

	spi->mode = SPI_MODE_3;
	spi->bits_per_word = 8;
	//spi->mode &= ~SPI_CS_HIGH;
	ret = spi_setup(spi); // Apply the above settings to the SPI device
	if (ret) {
		dev_err(&spi->dev, "spi_setup failed: %d\n", ret);
		// devm_kzalloc memory will be freed automatically on error return from probe
		return ret;
	}
	dev_info(&spi->dev, "SPI configured: mode=0x%x, speed=%dHz, bpw=%d\n",
			spi->mode, spi->max_speed_hz, spi->bits_per_word);

	mutex_init(&adxl->lock);

	//Initialize default values
	adxl->range = 2;
	adxl->rate = 100;
	adxl->int1_gpio = -1;

	//Read device ID
	ret = read_reg(spi, REG_DEVID, &devid);
	if (ret) {
		dev_err(&spi->dev, "Failed to read device ID\n");
		return ret;
	}

	if (devid != 0xE5) {
		dev_err(&spi->dev, "Invalid device ID: 0x%02x\n", devid);
		return -ENODEV;
	}

	dev_info(&spi->dev, "ADXL345 Device ID: 0x%02x\n", devid);

	// Initialize ADXL345
	uint8_t check_val;
	write_reg(spi, REG_POWER_CTL, POWER_CTL_MEASURE); // 0x08
	read_reg(spi, REG_POWER_CTL, &check_val);
	dev_info(&spi->dev, "POWER_CTL after write: 0x%02x (expected 0x08 or similar)\n", check_val);

	write_reg(spi, REG_DATA_FORMAT, 0x0B); // +/-16g, full-res
	read_reg(spi, REG_DATA_FORMAT, &check_val);
	dev_info(&spi->dev, "DATA_FORMAT after write: 0x%02x (expected 0x0B)\n", check_val);

	write_reg(spi, REG_BW_RATE, 0x0A); // 100Hz
	read_reg(spi, REG_BW_RATE, &check_val);
	dev_info(&spi->dev, "BW_RATE after write: 0x%02x (expected 0x0A)\n", check_val);

	// ** Get Interrupt GPIO **
	struct device_node *node = spi->dev.of_node;
	if (!node) {
		dev_err(&spi->dev, "Device Tree node not found\n");
		return -ENODEV;
	}

	// Get INT1 GPIO
	adxl->int1_gpio = of_get_named_gpio(node, "int1-gpio", 0);

	if (adxl->int1_gpio < 0) {
		dev_err(&spi->dev, "Failed to get int1-gpio: %d\n", adxl->int1_gpio);
		return adxl->int1_gpio; // Propagate the error
	}

	ret = devm_gpio_request_one(&spi->dev, adxl->int1_gpio, GPIOF_IN, "ADXL345_INT1");
	if (ret) {
		dev_err(&spi->dev, "Failed to request INT1 GPIO: %d\n", adxl->int1_gpio);
		return ret;
	}
	dev_info(&spi->dev, "INT1 GPIO: %d\n", adxl->int1_gpio);

	// ** Request and Enable Interrupt **
	irq_num = gpio_to_irq(adxl->int1_gpio);
	if (irq_num < 0) {
		dev_err(&spi->dev, "Failed to get IRQ number\n");
		return irq_num;
	}

	adxl->irq = irq_num;  //Store it for future use.

	ret = request_irq(adxl->irq, irq_handler, IRQF_TRIGGER_RISING, DEVICE_NAME, adxl);
	if (ret < 0) {
		dev_err(&spi->dev, "Failed to request interrupt\n");
		return ret;
	}

	// Enable DATA_READY interrupt
	ret = write_reg(spi, REG_INT_ENABLE, INT_DATA_READY);
	if (ret) {
		dev_err(&spi->dev, "Failed to enable DATA_READY interrupt\n");
		free_irq(adxl->irq, adxl);
		return ret;
	}

	// Set the INT_MAP register so that DATA_READY goes to INT1
	ret = write_reg(spi, REG_INT_MAP, 0x00); // Route all interrupts to INT1
	if (ret) {
		dev_err(&spi->dev, "Failed to set interrupt map\n");
		free_irq(adxl->irq, adxl);
		return ret;
	}

	//** Character Device Registration **
	//1. Allocate a device number
	ret = alloc_chrdev_region(&adxl->dev_num, 0, 1, DEVICE_NAME);
	if (ret < 0) {
		dev_err(&spi->dev, "Failed to allocate device number\n");
		free_irq(adxl->irq, adxl);
		return ret;
	}

	//2. Create a device class
	adxl->dev_class = class_create(CLASS_NAME);
	if (IS_ERR(adxl->dev_class)) {
		dev_err(&spi->dev, "Failed to create device class\n");
		unregister_chrdev_region(adxl->dev_num, 1);
		free_irq(adxl->irq, adxl);
		return PTR_ERR(adxl->dev_class);
	}
	//3. Create a device node
	device_create(adxl->dev_class, &spi->dev, adxl->dev_num, NULL, DEVICE_NAME);
	dev_info(&spi->dev, "Device class created\n");

	//4. Initialize the cdev structure
	cdev_init(&adxl->cdev, &adxl345_fops);
	adxl->cdev.owner = THIS_MODULE;

	//5. Add the character device to the system
	ret = cdev_add(&adxl->cdev, adxl->dev_num, 1);
	if (ret < 0) {
		dev_err(&spi->dev, "Failed to add cdev\n");
		device_destroy(adxl->dev_class, adxl->dev_num);
		class_destroy(adxl->dev_class);
		unregister_chrdev_region(adxl->dev_num, 1);
		free_irq(adxl->irq, adxl);
		return ret;
	}
	// ** Sysfs Registration **
	ret = device_create_file(&spi->dev, &dev_attr_range);
	/* Error check later */
	ret = device_create_file(&spi->dev, &dev_attr_rate);
	/* Error check later */
	dev_info(&spi->dev, "ADXL345 driver initialized\n");
	return 0;
}

// SPI remove function
static void adxl345_remove(struct spi_device *spi) {
	struct my_ADXL345 *adxl = spi_get_drvdata(spi);

	dev_info(&spi->dev, "Removing ADXL345 driver\n");

	//Disable Interrupts before removing.
	write_reg(spi, REG_INT_ENABLE, 0x00);

	//Free resources before removing.
	free_irq(adxl->irq, adxl);  // Important to free the IRQ

	//** Sysfs Unregistration **
	device_remove_file(&spi->dev, &dev_attr_range);
	device_remove_file(&spi->dev, &dev_attr_rate);

	//** Character Device Unregistration **
	cdev_del(&adxl->cdev);
	device_destroy(adxl->dev_class, adxl->dev_num);
	class_destroy(adxl->dev_class);
	unregister_chrdev_region(adxl->dev_num, 1);

	//Power down the ADXL345 (optional)
	write_reg(spi, REG_POWER_CTL, 0x00); //Put in standby mode

	dev_info(&spi->dev, "ADXL345 driver removed\n");
}

/* Device Tree Compatibility */
static const struct of_device_id adxl345_of_match[] = {
	{ .compatible = "decryptec,ADXL345_spi"},
	{}
};
MODULE_DEVICE_TABLE(of, adxl345_of_match);

static const struct spi_device_id adxl345_id[] = {
	{"ADXL345_spi", 0},
	{}
};
MODULE_DEVICE_TABLE(spi, adxl345_id);

// SPI Driver Structure
static struct spi_driver adxl345_driver = {
	.probe = adxl345_probe,
	.remove = adxl345_remove,
	.id_table = adxl345_id,
	.driver = {
		.name = "adxl345_id",
		.of_match_table = adxl345_of_match,
	},
};

module_spi_driver(adxl345_driver);
