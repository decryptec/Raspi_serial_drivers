#include <linux/module.h>
#include <linux/init.h>
#include <linux/i2c.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/uaccess.h> // Required for copy_to_user
#include <linux/delay.h>

/* Meta */
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Decryptec");
MODULE_DESCRIPTION("ADS1115 simple sysfs driver");

#define CONFIG_REGISTER      0x01
#define CONVERSION_REGISTER  0x00
#define VREF                  3.3   // Assuming Vref = 3.3V for voltage calculation
#define ADS1115_FULL_SCALE    32768 // Full-scale value for 16-bit signed ADC

/* ADC structure */
struct my_adc {
    struct i2c_client *client;
    struct device *dev; // Store the device pointer for sysfs
    int raw_value;      // Store the latest raw value
    struct mutex lock;   // Protect raw_value
};

/* Sysfs attribute show (read) function */
static ssize_t raw_value_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    struct my_adc *adc = dev_get_drvdata(dev);  // Get the my_adc struct
    int ret;

    mutex_lock(&adc->lock);
    // Start single-shot conversion by setting OS bit to 1
    ret = i2c_smbus_write_word_data(adc->client, CONFIG_REGISTER, cpu_to_be16(0x8483));  // 0x8483: OS = 1 (single-shot conversion)
    if (ret < 0) {
        dev_err(dev, "Failed to start single-shot conversion\n");
        mutex_unlock(&adc->lock);
        return ret;
    }

    // Wait for conversion to complete (you could add a timeout here)
    msleep(20);  // Typical conversion time for ADS1115 is ~17ms at 860SPS

    // Read the conversion result from the CONVERSION register
    ret = i2c_smbus_read_word_data(adc->client, CONVERSION_REGISTER);
    if (ret < 0) {
        dev_err(dev, "Error reading ADC value\n");
        mutex_unlock(&adc->lock);
        return ret;
    }

    adc->raw_value = be16_to_cpu(ret);
    mutex_unlock(&adc->lock);

    return sprintf(buf, "%d\n", adc->raw_value);
}

/* Sysfs attribute store (write) function - not implemented for this example */
static ssize_t raw_value_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
    // We don't allow writing to this attribute in this example
    return -EPERM;  // Permission denied
}

/* Define the sysfs attribute */
static DEVICE_ATTR(raw_value, S_IRUGO, raw_value_show, raw_value_store);

/* Device Tree compatibility */
static const struct of_device_id my_driver_ids[] = {
    { .compatible = "decryptec,my_ads" },
    {},
};
MODULE_DEVICE_TABLE(of, my_driver_ids);

/* I2C device IDs */
static const struct i2c_device_id my_ads[] = {
    { "my_adc", 0 },
    {},
};
MODULE_DEVICE_TABLE(i2c, my_ads);

/* Probe function */
static int my_adc_probe(struct i2c_client *client)
{
    struct my_adc *adc;
    int ret;

    pr_info("i2c_ads - Probe called\n");

    if (client->addr != 0x48) {
        dev_err(&client->dev, "Wrong I2C address (expected 0x48)\n");
        return -ENODEV;
    }

    adc = devm_kzalloc(&client->dev, sizeof(struct my_adc), GFP_KERNEL);
    if (!adc) {
        dev_err(&client->dev, "Failed to allocate memory\n");
        return -ENOMEM;
    }

    adc->client = client;
    adc->dev = &client->dev;  // Store the device pointer
    mutex_init(&adc->lock);

    dev_set_drvdata(&client->dev, adc); // Store the adc struct in the device

    /* Quickstart configuration */
    ret = i2c_smbus_write_word_data(adc->client, CONFIG_REGISTER, cpu_to_be16(0x8483));
    if (ret < 0) {
        dev_err(&client->dev, "Failed to configure ADS1115\n");
        return ret;
    }

    // Create the sysfs attribute
    ret = device_create_file(&client->dev, &dev_attr_raw_value);
    if (ret) {
        dev_err(&client->dev, "Failed to create sysfs attribute\n");
        return ret;
    }

    return 0;  // Success
}

/* Remove function */
static void my_adc_remove(struct i2c_client *client)
{
    struct my_adc *adc = dev_get_drvdata(&client->dev);

    pr_info("i2c_ads - Remove called\n");

    // Power down ADS1115 by setting OS bit to 0b (no conversion, device in power-down state)
    i2c_smbus_write_word_data(adc->client, CONFIG_REGISTER, cpu_to_be16(0x8583));  // 0x8583: OS = 0 (power down)

    // Remove the sysfs attribute
    device_remove_file(&client->dev, &dev_attr_raw_value);
}

/* I2C Driver */
static struct i2c_driver my_driver = {
    .probe = my_adc_probe,
    .remove = my_adc_remove,
    .id_table = my_ads,
    .driver = {
        .name = "my_adc",
        .of_match_table = my_driver_ids,
    },
};

/* Auto load driver */
module_i2c_driver(my_driver);
