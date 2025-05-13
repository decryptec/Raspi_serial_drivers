#ifndef ADXL345_H
#define ADXL345_H

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/spi/spi.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/cdev.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/kobject.h>
#include <linux/sysfs.h>

// ADXL345 Register Definitions
#define REG_DEVID 0x00
#define REG_DATAX0 0x32
#define REG_POWER_CTL 0x2D
#define REG_BW_RATE 0x2C
#define REG_DATA_FORMAT 0x31
#define REG_INT_ENABLE 0x2E
#define REG_INT_SOURCE 0x30
#define REG_INT_MAP 0x2F

// ADXL345 Register Bit Definitions
#define POWER_CTL_MEASURE 0x08 // Set Measure bit to start measuring
#define INT_DATA_READY 0x80 // Data Ready Interrupt Enable
#define INT_SINGLE_TAP 0x40 // Single Tap Interrupt Enable
#define INT_DOUBLE_TAP 0x20 // Double Tap Interrupt Enable

// Tap config
#define REG_THRESH_TAP    0x1D // Tap threshold
#define REG_DUR           0x21 // Tap duration
#define REG_LATENT        0x22 // Tap latency (for double tap)
#define REG_WINDOW        0x23 // Tap window (for double tap)
#define REG_TAP_AXES      0x2A // Axis control for single tap/double tap
#define DOUBLE_TAP_COOLDOWN_MS 400 // Cooldown period in milliseconds

#define DEVICE_NAME "adxl345"
#define CLASS_NAME "adxl345_class"

// Device struct
struct my_ADXL345 {
    struct spi_device *spi;
    struct device *dev;
    struct mutex lock;
    int irq; // IRQ Num
    int16_t x;
    int16_t y;
    int16_t z;

    //Char device members
    dev_t dev_num;
    struct class *dev_class;
    struct cdev cdev;

    //Configuration (Sysfs interface)
    int range;
    int rate;
    int int1_gpio;
};

// Function Prototypes
static int read_reg(struct spi_device *spi, uint8_t reg, uint8_t *val);
static int write_reg(struct spi_device *spi, uint8_t reg, uint8_t val);
static int get_data(struct my_ADXL345 *adxl);
static irqreturn_t irq_handler(int irq, void *dev_id);

static ssize_t range_show(struct device *dev, struct device_attribute *attr, char *buf);
static ssize_t range_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count);
static ssize_t rate_show(struct device *dev, struct device_attribute *attr, char *buf);
static ssize_t rate_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count);

static int adxl345_open(struct inode *inode, struct file *file);
static int adxl345_release(struct inode *inode, struct file *file);
static ssize_t adxl345_read(struct file *file, char __user *ubuf, size_t count, loff_t *ppos);
static int adxl345_probe(struct spi_device *spi);
static void adxl345_remove(struct spi_device *spi);

#endif
