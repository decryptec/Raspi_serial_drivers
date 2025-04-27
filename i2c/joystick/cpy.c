#include <linux/module.h>
#include <linux/init.h>
#include <linux/i2c.h>
#include <linux/iio/iio.h>
#include <linux/iio/sysfs.h>
#include <linux/delay.h>

/* Meta */
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Decryptec");
MODULE_DESCRIPTION("ADS1115 IIO using I2C and simple sysfs interface");

#define CONFIG_REGISTER      0x01
#define CONVERSION_REGISTER  0x00
#define VREF                  3.3   // Assuming Vref = 3.3V for voltage calculation
#define ADS1115_FULL_SCALE    32768 // Full-scale value for 16-bit signed ADC

/* ADC structure */
struct my_adc {
    struct i2c_client *client;
};

/* Forward declarations */
static const struct iio_info my_adc_info;

/* Read ADC raw value */
static int adc_read_raw(struct iio_dev *indio_dev, struct iio_chan_spec const *chan,
                        int *val, int *val2, long mask)
{
    struct my_adc *adc = iio_priv(indio_dev);
    int ret;
    int raw_value;

    if (mask == IIO_CHAN_INFO_RAW) {
        // Start single-shot conversion by setting OS bit to 1
        ret = i2c_smbus_write_word_data(adc->client, CONFIG_REGISTER, cpu_to_be16(0x8483));  // 0x8483: OS = 1 (single-shot conversion)
        if (ret < 0) {
            pr_err("iio_ads - Failed to start single-shot conversion\n");
            return ret;
        }

        // Wait for conversion to complete (you could add a timeout here)
        msleep(20);  // Typical conversion time for ADS1115 is ~17ms at 860SPS

        // Read the conversion result from the CONVERSION register
        ret = i2c_smbus_read_word_data(adc->client, CONVERSION_REGISTER);
        if (ret < 0) {
            pr_err("iio_ads - Error reading ADC value\n");
            return ret;
        }

        raw_value = be16_to_cpu(ret);
       // *val = raw_value;  // Return raw value

        // Calculate voltage in mV
        //int voltage_mV = (raw_value * VREF * 1000) / ADS1115_FULL_SCALE;
        //*val2 = voltage_mV;  // Store voltage in millivolts

        return IIO_VAL_INT_PLUS_MICRO;
    }

    return -EINVAL;
}

/* Channel spec */
static const struct iio_chan_spec my_adc_channels[] = {
    {
        .type = IIO_VOLTAGE,
        .info_mask_separate = BIT(IIO_CHAN_INFO_RAW),
    },
};

/* IIO info struct */
static const struct iio_info my_adc_info = {
    .read_raw = adc_read_raw,
};

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
    struct iio_dev *indio_dev;
    struct my_adc *adc;
    int ret;

    pr_info("iio_ads - Probe called\n");

    if (client->addr != 0x48) {
        pr_err("iio_ads - Wrong I2C address (expected 0x48)\n");
        return -ENODEV;
    }

    indio_dev = devm_iio_device_alloc(&client->dev, sizeof(struct my_adc));
    if (!indio_dev) {
        pr_err("iio_ads - Failed to allocate IIO device\n");
        return -ENOMEM;
    }

    adc = iio_priv(indio_dev);
    adc->client = client;

    indio_dev->name = dev_name(&client->dev);
    indio_dev->info = &my_adc_info;
    indio_dev->modes = INDIO_DIRECT_MODE;
    indio_dev->channels = my_adc_channels;
    indio_dev->num_channels = ARRAY_SIZE(my_adc_channels);

    i2c_set_clientdata(client, indio_dev);

    /* Quickstart configuration */
    ret = i2c_smbus_write_word_data(adc->client, CONFIG_REGISTER, cpu_to_be16(0x8483));
    if (ret < 0) {
        pr_err("iio_ads - Failed to configure ADS1115\n");
        return ret;
    }

    return devm_iio_device_register(&client->dev, indio_dev);
}

/* Remove function */
static void my_adc_remove(struct i2c_client *client)
{
    struct iio_dev *indio_dev = i2c_get_clientdata(client);
    struct my_adc *adc = iio_priv(indio_dev);

    pr_info("iio_ads - Remove called\n");

    // Power down ADS1115 by setting OS bit to 0b (no conversion, device in power-down state)
    i2c_smbus_write_word_data(adc->client, CONFIG_REGISTER, cpu_to_be16(0x8583));  // 0x8583: OS = 0 (power down)
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
