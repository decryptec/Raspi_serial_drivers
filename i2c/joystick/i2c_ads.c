#include <linux/module.h>
#include <linux/init.h>
#include <linux/i2c.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/uaccess.h>
#include <linux/delay.h>
#include <linux/of.h>
#include <linux/mutex.h>
#include <linux/kernel.h>

/* Meta Info */
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Decryptec");
MODULE_DESCRIPTION("ADS1115 driver kernel module");

// Constants
#define CONVERSION_REG 0x00
#define CONFIG_REG 0x01
#define VREF 4096 // VREF * 1000 (mV)
#define FULL_SCALE 32768 // 16 Bit ADC = 2^(16-1)

// Configuration bits
#define CONFIG_OS_SINGLE    0x8000
#define CONFIG_PGA_4_096V   0x0200
#define CONFIG_MODE_SINGLE  0x0100
#define CONFIG_DR_128SPS    0x0080
#define CONFIG_COMP_DISABLE 0x0003

// Channel
typedef enum {
        MUX_AIN0 = 0x4000,
        MUX_AIN1 = 0x5000,
        MUX_AIN2 = 0x6000,
        MUX_AIN3 = 0x7000
} mux_bits_t;

#define CONFIG_DEFAULT (CONFIG_OS_SINGLE | MUX_AIN0 | CONFIG_PGA_4_096V | \
  CONFIG_MODE_SINGLE | CONFIG_DR_128SPS | CONFIG_COMP_DISABLE)


/* ADC struct */
struct my_adc {
        struct i2c_client *client;
        struct device *dev;
        int channel;
        int raw_value;
        long voltage_mV;
        struct mutex lock;
};

/* Read ADC raw and calculate voltage */
static int read_ads1115(struct my_adc *adc)
{
        int ret;
        u16 config_value;
        u8 config_bytes[2];
        s16 raw_val;
        long voltage_temp;

        // Write config to start conversion
        config_value = CONFIG_OS_SINGLE | adc->channel | CONFIG_PGA_4_096V | CONFIG_MODE_SINGLE | CONFIG_DR_128SPS | CONFIG_COMP_DISABLE;
        config_bytes[0] = (config_value >> 8) & 0xFF; // Most significant byte
        config_bytes[1] = config_value & 0xFF; // Least significant byte

        mutex_lock(&adc->lock); // Lock before accessing i2c

        ret = i2c_smbus_write_i2c_block_data(adc->client, CONFIG_REG, 2, config_bytes);
        if (ret < 0) {
                dev_err(adc->dev, "Failed to write config: %d\n", ret);
                mutex_unlock(&adc->lock); // Unlock on error
                return ret;
        }

        // Wait for conversion
        msleep(100);

        // Read conversion register
        ret = i2c_smbus_read_i2c_block_data(adc->client, CONVERSION_REG, 2, (u8 *)&config_bytes);
        if (ret < 0) {
                dev_err(adc->dev, "Failed to read conversion: %d\n", ret);
                mutex_unlock(&adc->lock); // Unlock on error
                return ret;
        }

        mutex_unlock(&adc->lock);  // Unlock after i2c access

        raw_val = (config_bytes[0] << 8) | config_bytes[1];

        // Handle 2's complement negative values
        if (raw_val > 0x7FFF) {
                raw_val -= 0x10000;
        }

        adc->raw_value = raw_val;

        // Convert to voltage
        voltage_temp = (long)raw_val * VREF;
        adc->voltage_mV = voltage_temp / FULL_SCALE;

        return 0;
}

/* Sysfs attribute show - raw_value */
static ssize_t raw_value_show(struct device *dev, struct device_attribute *attr, char *buf)
{
        struct my_adc *adc = dev_get_drvdata(dev);
        int ret;

        ret = read_ads1115(adc);

        if (ret < 0) {
                return ret;
        }

        return sprintf(buf, "%d\n", adc->raw_value);
}

/* Sysfs attribute show - voltage */
static ssize_t voltage_show(struct device *dev, struct device_attribute *attr, char *buf)
{
        struct my_adc *adc = dev_get_drvdata(dev);
        int ret;

        ret = read_ads1115(adc);

        if (ret < 0) {
                return ret;
        }


        return sprintf(buf, "%ld.%03ld\n", adc->voltage_mV / 1000, adc->voltage_mV % 1000); // Format mV to V
}

/* Sysfs attribute show - channel */
static ssize_t channel_show(struct device *dev, struct device_attribute *attr, char *buf)
{
        struct my_adc *adc = dev_get_drvdata(dev);

        u16 temp = adc->channel;
        int ret_val;


        switch(temp)
        {
                case MUX_AIN0:
                        ret_val = 0;
                        break;
                case MUX_AIN1:
                        ret_val = 1;
                        break;
                case MUX_AIN2:
                        ret_val = 2;
                        break;
                case MUX_AIN3:
                        ret_val = 3;
                        break;
                default:
                        dev_err(dev, "Error reading channel\n");
                        return -EINVAL; // Correct error code
        }

        return sprintf(buf, "%d\n", ret_val);
}

/* Sysfs attribute store - channel */
static ssize_t channel_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
        struct my_adc *adc = dev_get_drvdata(dev);
        int temp;

        if (kstrtoint(buf, 0, &temp)) {
            dev_err(dev, "Invalid input for channel\n");
            return -EINVAL;
        }

        mutex_lock(&adc->lock);
        switch(temp)
        {
                case 0:
                        adc->channel = MUX_AIN0;
                        break;
                case 1:
                        adc->channel = MUX_AIN1;
                        break;
                case 2:
                        adc->channel = MUX_AIN2;
                        break;
                case 3:
                        adc->channel = MUX_AIN3;
                        break;
                default:
                        dev_err(dev, "Invalid channel inputted\n");
                        mutex_unlock(&adc->lock);
                        return -EINVAL;  // Correct error code
        }
        mutex_unlock(&adc->lock);

        return count;
}

/* Sysfs attribute store - N/A */
static ssize_t raw_value_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
        return -EPERM;
}

static ssize_t voltage_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
        return -EPERM;
}

/* Define sysfs attributes */
static DEVICE_ATTR(raw_value, 0444, raw_value_show, raw_value_store); // All Read-only
static DEVICE_ATTR(voltage, 0444, voltage_show, voltage_store); // All Read-only
static DEVICE_ATTR(channel, 0664, channel_show, channel_store); // Owner and Group Write and Read, Others Read-only

/* Device Tree Compatibility */
static const struct of_device_id my_driver_ids[] =
{
        { .compatible = "decryptec,my_ads"},
        {},
};
MODULE_DEVICE_TABLE(of, my_driver_ids);

/* I2C device IDs */
static const struct i2c_device_id my_ads[] =
{
        {"my_ads", 0},
        {},
};
MODULE_DEVICE_TABLE(i2c, my_ads); //Missing module device table for i2c

/* Probe function */
static int my_adc_probe(struct i2c_client *client)
{
        struct my_adc *adc;
        int ret;

        dev_info(&client->dev, "i2c_ads - Probe called\n");

        if(!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
                dev_err(&client->dev, "I2C functionality not supported\n");
                return -ENODEV;
        }

        adc = devm_kzalloc(&client->dev, sizeof(struct my_adc), GFP_KERNEL);
        if (!adc) {
                dev_err(&client->dev, "Failed to allocate memory\n");
                return -ENOMEM;
        }

        adc->client = client;
        adc->dev = &client->dev;
        mutex_init(&adc->lock);
        adc->channel = MUX_AIN0;  // Initialize to a valid channel

        dev_set_drvdata(&client->dev, adc);

        // Create sysfs attributes
        ret = device_create_file(&client->dev, &dev_attr_raw_value);
        if(ret) {
                dev_err(&client->dev, "Failed to create raw_value attribute\n");
                return ret;
        }

        ret = device_create_file(&client->dev, &dev_attr_voltage);
        if(ret) {
                dev_err(&client->dev, "Failed to create voltage attribute\n");
                device_remove_file(&client->dev, &dev_attr_raw_value);
                return ret;
        }

        ret = device_create_file(&client->dev, &dev_attr_channel);
        if(ret) {
                dev_err(&client->dev, "Failed to create channel attribute\n");
                device_remove_file(&client->dev, &dev_attr_voltage); 
                device_remove_file(&client->dev, &dev_attr_raw_value);
                return ret;
        }

        return 0;
}

static void my_adc_remove(struct i2c_client *client)
{
        struct my_adc *adc = dev_get_drvdata(&client->dev);

        dev_info(&client->dev, "i2c_ads - Remove called\n");

        device_remove_file(&client->dev, &dev_attr_raw_value);
        device_remove_file(&client->dev, &dev_attr_voltage);
        device_remove_file(&client->dev, &dev_attr_channel);

        mutex_destroy(&adc->lock);
}

static struct i2c_driver my_driver = {
        .probe = my_adc_probe,
        .remove = my_adc_remove,
        .id_table = my_ads,
        .driver = {
                .name = "my_ads",
                .of_match_table = my_driver_ids,
        },
};

/* Auto-load driver */
module_i2c_driver(my_driver);
