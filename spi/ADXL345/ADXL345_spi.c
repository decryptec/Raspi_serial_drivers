#include "ADXL345_spi.h"
#include "interrupts.h"
#include "interface.h"
/* META INFO */
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Decryptec");
MODULE_DESCRIPTION("ADXL345 driver using cdev, sysfs, and Interrupt");

// SPI probe
static int adxl345_probe(struct spi_device *spi) {
    struct my_ADXL345 *adxl;
    u8 devid;
    int ret;
    u8 data_format_val, bw_rate_val;

    // dev_info(&spi->dev, "Probing ADXL345\n"); // Minimal: can be enabled for debug

    // Allocate and initialize device structure
    adxl = devm_kzalloc(&spi->dev, sizeof(*adxl), GFP_KERNEL);
    if (!adxl) return -ENOMEM;

    adxl->spi = spi;
    adxl->dev = &spi->dev;
    spi_set_drvdata(spi, adxl);
    mutex_init(&adxl->lock);
    adxl->irq = -1;
    adxl->int1_gpio = -1;
    adxl->last_double_tap_jiffies = jiffies - msecs_to_jiffies(DOUBLE_TAP_COOLDOWN_MS * 2);

    // Configure SPI bus parameters for this device
    spi->mode = SPI_MODE_3;
    spi->bits_per_word = 8;
    ret = spi_setup(spi);
    if (ret) { dev_err(adxl->dev, "spi_setup failed: %d\n", ret); return ret; }
    // dev_info(adxl->dev, "SPI mode:0x%x, speed:%dHz\n", spi->mode, spi->max_speed_hz); // Minimal

    // Verify Device ID
    ret = read_reg(spi, REG_DEVID, &devid);
    if (ret) { dev_err(adxl->dev, "Read REG_DEVID failed: %d\n", ret); return ret; }
    if (devid != 0xE5) { dev_err(adxl->dev, "Invalid ID: 0x%02x\n", devid); return -ENODEV; }
    dev_info(adxl->dev, "ADXL345 ID: 0x%02x\n", devid); // Keep this important one

    // Initialize ADXL345 core operational registers
    // dev_info(adxl->dev, "Initializing ADXL345 core registers...\n"); // Minimal
    ret = write_reg(spi, REG_POWER_CTL, POWER_CTL_MEASURE); if (ret) return ret;
    data_format_val = 0x0B; adxl->range = 16; // +/-16g, Full Res
    ret = write_reg(spi, REG_DATA_FORMAT, data_format_val); if (ret) return ret;
    bw_rate_val = 0x0A; adxl->rate = 100;     // 100Hz ODR
    ret = write_reg(spi, REG_BW_RATE, bw_rate_val); if (ret) return ret;
    // Optional: Read-back checks for POWER_CTL, DATA_FORMAT, BW_RATE can be added for deep debug

    ret = interrupt_init(adxl, spi);
    if (ret) { dev_err(adxl->dev, "Failed to initialize Interrupts: %d\n", ret); return ret; }

    ret = interface_init(adxl, spi);
    if (ret) { dev_err(adxl->dev, "Failed to initialize interfaces: %d\n", ret); return ret; }

    dev_info(adxl->dev, "ADXL345 driver initialized successfully\n"); // Keep this
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

    interface_cleanup(adxl, spi);

    // Disable Interrupts on ADXL345 (best effort)
    // Do this before freeing IRQ in case an interrupt is pending
    interrupts_cleanup(adxl, spi);
    // IRQ and GPIO are managed by devm_* functions, no explicit free needed here
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
