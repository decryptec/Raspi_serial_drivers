#ifndef INTERFACE_H
#include "ADXL345_spi.h"

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

static ssize_t adxl345_read(struct file *file, char __user *ubuf, size_t user_count, loff_t *ppos) {
    struct my_ADXL345 *adxl = (struct my_ADXL345 *)file->private_data;
    char local_buffer[128];
    int data_len_in_buffer = 0;
    int get_data_ret;
    ssize_t bytes_copied = 0;

    if (!adxl || !adxl->spi) return -ENODEV;
    if (user_count == 0) return 0;

    if (adxl->irq >= 0) disable_irq(adxl->irq);
    mutex_lock(&adxl->lock);

    // Fetch fresh sensor data on each read
    get_data_ret = get_data(adxl);
    if (get_data_ret) dev_err(adxl->dev, "READ: get_data() failed: %d\n", get_data_ret);

    data_len_in_buffer = scnprintf(local_buffer, sizeof(local_buffer),
                                   "X: %d\nY: %d\nZ: %d\n",
                                   adxl->x, adxl->y, adxl->z);

    mutex_unlock(&adxl->lock);
    if (adxl->irq >= 0) enable_irq(adxl->irq);

    bytes_copied = min_t(size_t, user_count, data_len_in_buffer);
    if (bytes_copied <= 0) return 0;

    if (copy_to_user(ubuf, local_buffer, bytes_copied)) return -EFAULT;

    // Reset file position so multiple reads work
    *ppos = 0; 

    return bytes_copied;
}


static const struct file_operations adxl345_fops = {
    .owner = THIS_MODULE,
    .open = adxl345_open,
    .release = adxl345_release,
    .read = adxl345_read,
};

static int interface_init(struct my_ADXL345 *adxl, struct spi_device *spi)
{
    int ret;

    // Register Character Device
    ret = alloc_chrdev_region(&adxl->dev_num, 0, 1, DEVICE_NAME);
    if (ret < 0) { dev_err(adxl->dev, "alloc_chrdev_region failed: %d\n", ret); return ret; }
    adxl->dev_class = class_create(CLASS_NAME);
    if (IS_ERR(adxl->dev_class)) { /* ... error handling & unregister_chrdev_region ... */ return PTR_ERR(adxl->dev_class); }
    if (!device_create(adxl->dev_class, &spi->dev, adxl->dev_num, adxl, DEVICE_NAME)) { /* ... error handling & class_destroy ... */ return -EFAULT; }
    cdev_init(&adxl->cdev, &adxl345_fops);
    ret = cdev_add(&adxl->cdev, adxl->dev_num, 1);
    if (ret < 0) { /* ... error handling & device_destroy ... */ return ret; }
    // dev_info(adxl->dev, "/dev/%s created\n", DEVICE_NAME); // Minimal

    // Register Sysfs attributes
    ret = sysfs_create_group(&spi->dev.kobj, &adxl345_attr_group);
    if (ret) { dev_err(adxl->dev, "sysfs_create_group failed: %d\n", ret); /* ... cdev cleanup ... */ return ret; }
    // dev_info(adxl->dev, "Sysfs attributes created\n"); // Minimal

    return 0;
}

static void interface_cleanup(struct my_ADXL345 *adxl, struct spi_device *spi)
{
    // Sysfs Unregistration
    sysfs_remove_group(&spi->dev.kobj, &adxl345_attr_group);
    dev_info(&spi->dev, "Sysfs attributes removed\n");

    // Character Device Unregistration
    cdev_del(&adxl->cdev);
    device_destroy(adxl->dev_class, adxl->dev_num);
    class_destroy(adxl->dev_class);
    unregister_chrdev_region(adxl->dev_num, 1);
    dev_info(&spi->dev, "Character device removed\n");
}
#endif