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
#include <linux/jiffies.h> // For jiffies, msecs_to_jiffies, time_before

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
#define DOUBLE_TAP_COOLDOWN_MS 1000 // Cooldown period in milliseconds

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
    unsigned long last_double_tap_jiffies;

    //Char device members
    dev_t dev_num;
    struct class *dev_class;
    struct cdev cdev;

    //Configuration (Sysfs interface)
    int range;
    int rate;
    int int1_gpio;
};

// Helper functions
static int read_reg(struct spi_device *spi, uint8_t reg, uint8_t *val) {
    int ret;
    
    // Simple helper function approach
    ret = spi_w8r8(spi, reg | 0x80);
    if (ret < 0)
    {
        dev_err(&spi->dev, "Failed to read REG 0x%02x: Error: %d\n", reg, ret);
        return ret;
    }
    *val = (u8)ret;
    /* Optional: Explicit approach
    //u8 tx_buf[2], rx_buf[2];
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
    */
    return 0;
}

static int write_reg(struct spi_device *spi, uint8_t reg, uint8_t val) {
    int ret;

    // Simple helper function approach
    u8 tx_buf[2] = {reg & ~0x80, val};
    ret = spi_write(spi, tx_buf, 2);
    if (ret) {
        dev_err(&spi->dev, "Failed to write to REG 0x%02x: Error: %d\n", reg, ret);
        return ret;
    }
    /* Optional: Explicit Approach
    //u8 tx_buf[2];
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
    */
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

    dev_dbg(&adxl->spi->dev, "GET_DATA: Calling spi_sync...\n");
    ret = spi_sync(adxl->spi, &msg);
    dev_dbg(&adxl->spi->dev, "GET_DATA: spi_sync returned %d\n", ret);

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
    //dev_info(&adxl->spi->dev, "get_data raw rx_buf: %s\n", temp_buf);

    adxl->x = (s16)((rx_buf[2] << 8) | rx_buf[1]); // Use s16 for signed 16-bit
    adxl->y = (s16)((rx_buf[4] << 8) | rx_buf[3]);
    adxl->z = (s16)((rx_buf[6] << 8) | rx_buf[5]);

    return 0;
}
static int adxl345_probe(struct spi_device *spi);
static void adxl345_remove(struct spi_device *spi);

#endif
