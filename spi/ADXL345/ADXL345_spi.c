#include "ADXL345_spi.h"

/* META INFO */
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Decryptec");
MODULE_DESCRIPTION("ADXL345 driver using cdev, sysfs, and Interrupt");

// sysfs - Measuring range
static ssize_t range_show(struct device *dev, struct device_attribute *attr, char *buf) {
    struct my_ADXL345 *adxl = dev_get_drvdata(dev); // dev is &spi->dev here
    if (!adxl) return -ENODEV;
    return sprintf(buf, "%d\n", adxl->range);
}

static ssize_t range_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count) {
    struct my_ADXL345 *adxl = dev_get_drvdata(dev); // dev is &spi->dev here
    int new_range;
    int ret;
    uint8_t data_format_val; 

    if (!adxl) return -ENODEV;

    ret = kstrtoint(buf, 0, &new_range);
    if (ret)
        return ret;

    if (new_range != 2 && new_range != 4 && new_range != 8 && new_range != 16) {
        dev_err(dev, "Invalid range value: %d. Must be 2, 4, 8, or 16.\n", new_range);
        return -EINVAL;
    }

    // Protect access to adxl->range and SPI write
    mutex_lock(&adxl->lock);
    adxl->range = new_range;

    // Preserve other bits in DATA_FORMAT (like INT_INVERT if you ever set it)
    // For now, assuming FULL_RES (D3) is always desired if range is set.
    // And range bits are D0, D1. INT_INVERT is D5.
    // Let's assume INT_INVERT is always 0 (active high interrupts) for now.
    // 0x08 = FULL_RES, +/-2g
    switch (new_range) {
        case 2:  data_format_val = 0x08; break; // +/- 2g, FULL_RES
        case 4:  data_format_val = 0x09; break; // +/- 4g, FULL_RES
        case 8:  data_format_val = 0x0A; break; // +/- 8g, FULL_RES
        case 16: data_format_val = 0x0B; break; // +/- 16g, FULL_RES
        default: 
            mutex_unlock(&adxl->lock);
            return -EINVAL;
    }

    dev_info(dev, "Storing new range %dG, writing 0x%02x to DATA_FORMAT\n", new_range, data_format_val);
    ret = write_reg(adxl->spi, REG_DATA_FORMAT, data_format_val);
    mutex_unlock(&adxl->lock);

    if (ret) {
        dev_err(dev, "Failed to write DATA_FORMAT for new range: %d\n", ret);
        return ret;
    }
    return count;
}

// sysfs - Sampling rate
static ssize_t rate_show(struct device *dev, struct device_attribute *attr, char *buf) {
    struct my_ADXL345 *adxl = dev_get_drvdata(dev);
    if (!adxl) return -ENODEV;
    return sprintf(buf, "%d\n", adxl->rate);
}

static ssize_t rate_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count) {
    struct my_ADXL345 *adxl = dev_get_drvdata(dev);
    int new_rate;
    int ret;
    uint8_t bw_rate_val;

    if (!adxl) return -ENODEV;

    ret = kstrtoint(buf, 0, &new_rate);
    if (ret)
        return ret;

    if (new_rate <= 0) { // Basic check
        dev_err(dev, "Invalid rate value: %d. Must be > 0.\n", new_rate);
        return -EINVAL;
    }

    mutex_lock(&adxl->lock);
    adxl->rate = new_rate; // Store user's desired rate, map to ADXL345 value

    // Map user desired rate to ADXL345 BW_RATE register value
    // These are Output Data Rates (ODR). Lower bits also control bandwidth.
    // Example mapping (check datasheet for exact ODR vs register value)
    if (new_rate <= 6)       bw_rate_val = 0x06; // 6.25Hz
    else if (new_rate <= 12) bw_rate_val = 0x07; // 12.5Hz
    else if (new_rate <= 25) bw_rate_val = 0x08; // 25Hz
    else if (new_rate <= 50) bw_rate_val = 0x09; // 50Hz
    else if (new_rate <= 100)bw_rate_val = 0x0A; // 100Hz
    else if (new_rate <= 200)bw_rate_val = 0x0B; // 200Hz
    else if (new_rate <= 400)bw_rate_val = 0x0C; // 400Hz
    else if (new_rate <= 800)bw_rate_val = 0x0D; // 800Hz
    else if (new_rate <= 1600)bw_rate_val = 0x0E; // 1600Hz
    else                     bw_rate_val = 0x0F; // 3200Hz (max normal mode)

    dev_info(dev, "Storing new rate %dHz, writing 0x%02x to BW_RATE\n", new_rate, bw_rate_val);
    ret = write_reg(adxl->spi, REG_BW_RATE, bw_rate_val);
    mutex_unlock(&adxl->lock);

    if (ret) {
        dev_err(dev, "Failed to write BW_RATE for new rate: %d\n", ret);
        return ret;
    }
    return count;
}

static DEVICE_ATTR_RW(range); // Uses S_IRUGO | S_IWUSR by default
static DEVICE_ATTR_RW(rate);

static struct attribute *adxl345_attrs[] = {
    &dev_attr_range.attr,
    &dev_attr_rate.attr,
    NULL,
};

static const struct attribute_group adxl345_attr_group = {
    .attrs = adxl345_attrs,
};

// Char Device file operations
static int adxl345_open(struct inode *inode, struct file *file) {
    struct my_ADXL345 *adxl = container_of(inode->i_cdev, struct my_ADXL345, cdev);
    if (!adxl) {
        pr_err("ADXL345: open: no device data for inode\n");
        return -ENODEV;
    }
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
    int ret_val = 0; 
    int get_data_ret;

    if (!adxl || !adxl->spi) { // Basic sanity check
        pr_err("ADXL345: read: adxl or spi device is NULL\n");
        return -ENODEV;
    }
    
    buffer = kmalloc(128, GFP_KERNEL);
    if (!buffer) {
        return -ENOMEM;
    }

    // Disable IRQ before taking mutex to prevent deadlock with irq_handler
    if (adxl->irq >= 0) { // Check if IRQ was successfully initialized
        dev_dbg(&adxl->spi->dev, "READ: Disabling IRQ %d\n", adxl->irq);
        disable_irq(adxl->irq);
    }

    dev_dbg(&adxl->spi->dev, "READ: Attempting lock\n");
    mutex_lock(&adxl->lock);
    dev_dbg(&adxl->spi->dev, "READ: Lock acquired\n");

    get_data_ret = get_data(adxl); // fetch fresh data
    if (get_data_ret) {
        dev_err(&adxl->spi->dev, "READ: Error from get_data(): %d. Displaying most recent data\n", get_data_ret);
    }

    len = sprintf(buffer, "X: %d\nY: %d\nZ: %d\n", adxl->x, adxl->y, adxl->z);

    dev_dbg(&adxl->spi->dev, "READ: Releasing lock\n");
    mutex_unlock(&adxl->lock);
    dev_dbg(&adxl->spi->dev, "READ: Lock released\n");

    // Re-enable IRQ after releasing mutex
    if (adxl->irq >= 0) {
        dev_dbg(&adxl->spi->dev, "READ: Enabling IRQ %d\n", adxl->irq);
        enable_irq(adxl->irq);
    }

    if (offset >= len) { // No more data to read
        kfree(buffer);
        return 0;
    }

    if (count == 0) { // User requested zero bytes
        kfree(buffer);
        return 0;
    }
    
    if (count > len - offset) { // MAX BUFFER
        count = len - offset;
    }

    if (copy_to_user(ubuf, buffer + offset, count)) {
        dev_warn(&adxl->spi->dev, "READ: copy_to_user failed\n");
        kfree(buffer);
        return -EFAULT;
    }

    *ppos += count; // Advance file position
    ret_val = count;  // Bytes successfully read

    kfree(buffer);
    return ret_val;
}

static const struct file_operations adxl345_fops = {
    .owner = THIS_MODULE,
    .open = adxl345_open,
    .release = adxl345_release,
    .read = adxl345_read,
};

static int read_reg(struct spi_device *spi, uint8_t reg, uint8_t *val) {
    int ret;
    u8 tx_buf[2], rx_buf[2];

    tx_buf[0] = reg | 0x80; // Read (MSB = 1)
    tx_buf[1] = 0x00;       // Dummy byte for clocking

    struct spi_transfer xfer = {
        .tx_buf = tx_buf,
        .rx_buf = rx_buf,
        .len = 2,
        .delay.value = 5,
        .delay.unit = SPI_DELAY_UNIT_USECS,
    };

    struct spi_message msg;
    spi_message_init(&msg);
    spi_message_add_tail(&xfer, &msg);

    ret = spi_sync(spi, &msg);
    if (ret) {
        dev_err(&spi->dev, "SPI read_reg (0x%02x) failed: %d\n", reg, ret);
        return ret;
    }

    *val = rx_buf[1]; // Data is in the second byte received
    return 0;
}

static int write_reg(struct spi_device *spi, uint8_t reg, uint8_t val) {
    int ret;
    u8 tx_buf[2];

    tx_buf[0] = reg & ~0x80; // Write (MSB = 0)
    tx_buf[1] = val;

    struct spi_transfer xfer = {
        .tx_buf = tx_buf,
        .len = 2,
        .delay.value = 5,
        .delay.unit = SPI_DELAY_UNIT_USECS,
    };

    struct spi_message msg;
    spi_message_init(&msg);
    spi_message_add_tail(&xfer, &msg);

    ret = spi_sync(spi, &msg);
    if (ret) {
        dev_err(&spi->dev, "SPI write_reg (0x%02x with 0x%02x) failed: %d\n", reg, val, ret);
        return ret;
    }
    return 0;
}

// Get acceleration data
static int get_data(struct my_ADXL345 *adxl) {
    int ret;
    u8 tx_buf[7], rx_buf[7];

    if (!adxl || !adxl->spi) return -ENODEV; // Sanity check

    tx_buf[0] = REG_DATAX0 | 0x80 | 0x40; // Multi-byte read starting at DATAX0
    memset(&tx_buf[1], 0, 6);           // Dummy bytes

    struct spi_transfer xfer = {
        .tx_buf = tx_buf,
        .rx_buf = rx_buf,
        .len = 7,
        .delay.value = 5,
        .delay.unit = SPI_DELAY_UNIT_USECS,
    };

    struct spi_message msg;
    spi_message_init(&msg);
    spi_message_add_tail(&xfer, &msg);

    dev_info(&adxl->spi->dev, "GET_DATA: Calling spi_sync...\n");
    ret = spi_sync(adxl->spi, &msg);
    dev_info(&adxl->spi->dev, "GET_DATA: spi_sync returned %d\n", ret);

    if (ret) {
        dev_err(&adxl->spi->dev, "SPI Acceleration read failed in get_data: %d\n", ret);
        return ret;
    }

    // Print raw buffer only if spi_sync was okay
    int i;
    char temp_buf[7 * 6]; // Adjusted size for "0xXX " format, and safety
    char *ptr = temp_buf;
    temp_buf[0] = '\0'; // Ensure buffer is null-terminated for sprintf safety

    for (i = 0; i < 7; i++) {
        if ((ptr - temp_buf + 6) < sizeof(temp_buf)) { // Check buffer bounds
             ptr += sprintf(ptr, "0x%02x ", rx_buf[i]);
        } else {
            dev_warn(&adxl->spi->dev, "GET_DATA: temp_buf too small for full rx_buf print\n");
            break;
        }
    }
    dev_info(&adxl->spi->dev, "get_data raw rx_buf: %s\n", temp_buf);

    adxl->x = (s16)((rx_buf[2] << 8) | rx_buf[1]); // Use s16 for signed 16-bit
    adxl->y = (s16)((rx_buf[4] << 8) | rx_buf[3]);
    adxl->z = (s16)((rx_buf[6] << 8) | rx_buf[5]);

    return 0;
}

// Interrupt handler function
static irqreturn_t irq_handler(int irq, void *dev_id) {
    struct my_ADXL345 *adxl = (struct my_ADXL345 *)dev_id;
    u8 int_source;
    int ret_val;

    if (!adxl || !adxl->spi) return IRQ_NONE; // Sanity check

    dev_info(&adxl->spi->dev, "IRQ HANDLER CALLED! irq=%d\n", irq);

    // Read INT source register to clear interrupt on ADXL345
    ret_val = read_reg(adxl->spi, REG_INT_SOURCE, &int_source);
    if (ret_val) {
        dev_err(&adxl->spi->dev, "IRQ: Error reading INT_SOURCE: %d\n", ret_val);
        return IRQ_NONE; 
    }
    dev_info(&adxl->spi->dev, "IRQ: INT_SOURCE read: 0x%02x\n", int_source);

    // For DATA_READY, we update the internal values.
    if (int_source & INT_DATA_READY) {
        dev_info(&adxl->spi->dev, "IRQ: DATA_READY detected.\n");
        dev_info(&adxl->spi->dev, "IRQ: Attempting lock. IRQ %d\n", irq);
        mutex_lock(&adxl->lock);
        dev_info(&adxl->spi->dev, "IRQ: Lock acquired. IRQ %d\n", irq);

        ret_val = get_data(adxl); // Update adxl->x, y, z
        if (ret_val) {
            dev_err(&adxl->spi->dev, "IRQ: Error from get_data() in DATA_READY: %d\n", ret_val);
        } else {
            dev_info(&adxl->spi->dev, "IRQ: DATA_READY processed. X: %d, Y: %d, Z: %d\n", adxl->x, adxl->y, adxl->z);
        }
        dev_dbg(&adxl->spi->dev, "IRQ: Releasing lock. IRQ %d\n", irq);
        mutex_unlock(&adxl->lock);
        dev_dbg(&adxl->spi->dev, "IRQ: Lock released. IRQ %d\n", irq);
    }
    if (int_source & INT_SINGLE_TAP) { 
        dev_info(&adxl->spi->dev, "IRQ: SINGLE_TAP detected!\n");
    }
    if (int_source & INT_DOUBLE_TAP) {
        dev_info(&adxl->spi->dev, "IRQ: DOUBLE_TAP detected!\n");
    }

    if (int_source == 0) {
        dev_warn(&adxl->spi->dev, "IRQ: Noisy interrupt. INT_SOURCE was 0x00 after read.\n");
    }

    return IRQ_HANDLED;
}

// SPI probe
static int adxl345_probe(struct spi_device *spi) {
    struct my_ADXL345 *adxl;
    u8 devid;
    int ret;
    u8 data_format_val;
    int irq_num;
    u8 check_val;

    dev_info(&spi->dev, "Probing ADXL345\n");

    adxl = devm_kzalloc(&spi->dev, sizeof(*adxl), GFP_KERNEL);
    if (!adxl)
        return -ENOMEM;

    adxl->spi = spi;
    adxl->dev = &spi->dev; // Store a pointer to the device for convenience in dev_info
    spi_set_drvdata(spi, adxl);

    // Configure SPI parameters
    spi->mode = SPI_MODE_3;
    spi->bits_per_word = 8;
    // spi->max_speed_hz is usually set from DT's spi-max-frequency

    ret = spi_setup(spi);
    if (ret) {
        dev_err(adxl->dev, "spi_setup failed: %d\n", ret);
        return ret;
    }
    dev_info(adxl->dev, "SPI configured: mode=0x%x, speed=%dHz, bpw=%d\n",
             spi->mode, spi->max_speed_hz, spi->bits_per_word);

    mutex_init(&adxl->lock);

    // Initialize default values for sysfs and internal state
    adxl->irq = -1;   // Initialize irq to invalid, updated upon successful request_irq
    adxl->int1_gpio = -1; // Initialize to invalid

    // Read device ID
    ret = read_reg(spi, REG_DEVID, &devid);
    if (ret) {
        dev_err(adxl->dev, "Failed to read REG_DEVID: %d\n", ret);
        return ret;
    }
    if (devid != 0xE5) {
        dev_err(adxl->dev, "Invalid device ID: 0x%02x (Expected 0xE5)\n", devid);
        return -ENODEV;
    }
    dev_info(adxl->dev, "ADXL345 Device ID: 0x%02x\n", devid);

    // Initialize ADXL345 sensor registers
    dev_info(adxl->dev, "Initializing ADXL345 registers...\n");
    ret = write_reg(spi, REG_POWER_CTL, POWER_CTL_MEASURE); // Put in measurement mode
    if (ret) return ret;
    read_reg(spi, REG_POWER_CTL, &check_val);
    dev_info(adxl->dev, "POWER_CTL after write: 0x%02x (expected 0x%02x)\n", check_val, POWER_CTL_MEASURE);

    data_format_val = 0x0B; // Default to +/-16g, Full Resolution, Active HIGH interrupts
    adxl->range = 16; // Sync struct member with initial write
    ret = write_reg(spi, REG_DATA_FORMAT, data_format_val);
    if (ret) return ret;
    read_reg(spi, REG_DATA_FORMAT, &check_val);
    dev_info(adxl->dev, "DATA_FORMAT after write: 0x%02x (expected 0x%02x)\n", check_val, data_format_val);

    u8 bw_rate_val = 0x0A; // Default to 100Hz ODR
    adxl->rate = 100; // Sync struct member
    ret = write_reg(spi, REG_BW_RATE, bw_rate_val);
    if (ret) return ret;
    read_reg(spi, REG_BW_RATE, &check_val);
    dev_info(adxl->dev, "BW_RATE after write: 0x%02x (expected 0x%02x)\n", check_val, bw_rate_val);

    // Get Interrupt GPIO from Device Tree
    struct device_node *node = spi->dev.of_node;
    if (!node) {
        dev_err(adxl->dev, "Device Tree node not found\n");
        return -ENODEV;
    }

    adxl->int1_gpio = of_get_named_gpio(node, "int1-gpio", 0);
    if (adxl->int1_gpio < 0) {
        if (adxl->int1_gpio != -EPROBE_DEFER) {
            dev_err(adxl->dev, "Failed to get int1-gpio from DT: %d\n", adxl->int1_gpio);
        }
        return adxl->int1_gpio; 
    }
    dev_info(adxl->dev, "DT: int1-gpio number: %d\n", adxl->int1_gpio);

    ret = devm_gpio_request_one(&spi->dev, adxl->int1_gpio, GPIOF_IN, "adxl345_int1");
    if (ret) {
        dev_err(adxl->dev, "Failed to request INT1 GPIO %d: %d\n", adxl->int1_gpio, ret);
        return ret;
    }

    irq_num = gpio_to_irq(adxl->int1_gpio);
    if (irq_num < 0) {
        dev_err(adxl->dev, "Failed to get IRQ for GPIO %d: %d\n", adxl->int1_gpio, irq_num);
        // devm_gpio_request_one will free the GPIO on probe failure
        return irq_num;
    }
    adxl->irq = irq_num;
    dev_info(adxl->dev, "Mapped GPIO %d to IRQ %d\n", adxl->int1_gpio, adxl->irq);

    // Using devm_ for IRQ request means it's auto-freed on remove/probe fail after this point
    ret = devm_request_threaded_irq(&spi->dev, adxl->irq, NULL, irq_handler,
                                   IRQF_TRIGGER_RISING | IRQF_ONESHOT,
                                   DEVICE_NAME, adxl);
    if (ret) {
        dev_err(adxl->dev, "Failed to request threaded IRQ %d: %d\n", adxl->irq, ret);
        // devm_gpio_request_one will free GPIO
        adxl->irq = -1; // Mark IRQ as not acquired
        return ret;
    }
    dev_info(adxl->dev, "Successfully requested IRQ %d\n", adxl->irq);

    // Configure ADXL345 to generate DATA_READY interrupt on INT1 pin
    ret = write_reg(spi, REG_INT_ENABLE, INT_DATA_READY);
    if (ret) {
        dev_err(adxl->dev, "Failed to enable DATA_READY interrupt on ADXL345\n");
        return ret;
    }
    dev_info(adxl->dev, "ADXL345 DATA_READY interrupt enabled\n");

    ret = write_reg(spi, REG_INT_MAP, 0x00); // Route all interrupts to INT1
    if (ret) {
        dev_err(adxl->dev, "Failed to map interrupts to INT1 on ADXL345\n");
        write_reg(spi, REG_INT_ENABLE, 0x00); // Attempt to disable
        return ret;
    }
    dev_info(adxl->dev, "ADXL345 interrupts mapped to INT1\n");

    // Character Device Registration
    ret = alloc_chrdev_region(&adxl->dev_num, 0, 1, DEVICE_NAME);
    if (ret < 0) {
        dev_err(adxl->dev, "Failed to allocate chrdev region: %d\n", ret);
        return ret;
    }
    dev_info(adxl->dev, "Chrdev region allocated: major %d, minor %d\n", MAJOR(adxl->dev_num), MINOR(adxl->dev_num));

    adxl->dev_class = class_create(CLASS_NAME);
    if (IS_ERR(adxl->dev_class)) {
        dev_err(adxl->dev, "Failed to create device class '%s'\n", CLASS_NAME);
        unregister_chrdev_region(adxl->dev_num, 1);
        return PTR_ERR(adxl->dev_class);
    }
    dev_info(adxl->dev, "Device class '%s' created\n", CLASS_NAME);

    if (!device_create(adxl->dev_class, &spi->dev, adxl->dev_num, adxl, DEVICE_NAME)) {
        dev_err(adxl->dev, "Failed to create device node /dev/%s\n", DEVICE_NAME);
        class_destroy(adxl->dev_class);
        unregister_chrdev_region(adxl->dev_num, 1);
        return -EFAULT;
    }
    dev_info(adxl->dev, "Device node /dev/%s created\n", DEVICE_NAME);

    cdev_init(&adxl->cdev, &adxl345_fops);
    adxl->cdev.owner = THIS_MODULE;
    ret = cdev_add(&adxl->cdev, adxl->dev_num, 1);
    if (ret < 0) {
        dev_err(adxl->dev, "Failed to add cdev: %d\n", ret);
        device_destroy(adxl->dev_class, adxl->dev_num);
        class_destroy(adxl->dev_class);
        unregister_chrdev_region(adxl->dev_num, 1);
        return ret;
    }
    dev_info(adxl->dev, "Character device added\n");

    // Sysfs Registration (using device_create_file on &spi->dev)
    // Use sysfs_create_group for attribute_group
    ret = sysfs_create_group(&spi->dev.kobj, &adxl345_attr_group);
    if (ret) {
        dev_err(adxl->dev, "Failed to create sysfs attribute group: %d\n", ret);
        // Cleanup cdev, class, region
        cdev_del(&adxl->cdev);
        device_destroy(adxl->dev_class, adxl->dev_num);
        class_destroy(adxl->dev_class);
        unregister_chrdev_region(adxl->dev_num, 1);
        return ret;
    }
    dev_info(adxl->dev, "Sysfs attributes created\n");

    dev_info(adxl->dev, "ADXL345 driver initialized successfully\n");
    return 0; 
}

// SPI remove function
static void adxl345_remove(struct spi_device *spi) {
    struct my_ADXL345 *adxl = spi_get_drvdata(spi);

    dev_info(&spi->dev, "Removing ADXL345 driver\n");

    if (!adxl) {
        dev_warn(&spi->dev, "remove called on NULL adxl data\n");
        return;
    }

    // Sysfs Unregistration
    sysfs_remove_group(&spi->dev.kobj, &adxl345_attr_group);
    dev_info(&spi->dev, "Sysfs attributes removed\n");

    // Character Device Unregistration
    cdev_del(&adxl->cdev);
    device_destroy(adxl->dev_class, adxl->dev_num);
    class_destroy(adxl->dev_class);
    unregister_chrdev_region(adxl->dev_num, 1);
    dev_info(&spi->dev, "Character device removed\n");

    // Disable Interrupts on ADXL345 (best effort)
    // Do this before freeing IRQ in case an interrupt is pending
    if (adxl->spi) { // Check if spi pointer is valid
        write_reg(adxl->spi, REG_INT_ENABLE, 0x00); // Stop ADXL345 from generating interrupts
        dev_info(&spi->dev, "ADXL345 interrupts disabled\n");

        // Power down the ADXL345 (optional, good practice)
        write_reg(adxl->spi, REG_POWER_CTL, 0x00); // Put in standby mode
        dev_info(&spi->dev, "ADXL345 powered down to standby\n");
    }

    dev_info(&spi->dev, "ADXL345 driver removed successfully\n");
}

/* Device Tree Compatibility */
static const struct of_device_id adxl345_of_match[] = {
    { .compatible = "decryptec,ADXL345_spi" },
    { /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, adxl345_of_match);

static const struct spi_device_id adxl345_id_table[] = { 
    { "ADXL345_spi", 0 }, 
    { /* sentinel */ }
};
MODULE_DEVICE_TABLE(spi, adxl345_id_table);

// SPI Driver Structure
static struct spi_driver adxl345_driver = {
    .probe = adxl345_probe,
    .remove = adxl345_remove,
    .id_table = adxl345_id_table, // For non-DT probing
    .driver = {
        .name = "adxl345_custom", 
        .of_match_table = adxl345_of_match, // For DT probing
        .owner = THIS_MODULE,
    },
};

// Use module_spi_driver macro for cleaner registration/unregistration
module_spi_driver(adxl345_driver);
