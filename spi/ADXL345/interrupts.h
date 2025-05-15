#ifndef INTERRUPTS_H
#include "ADXL345_spi.h"

static irqreturn_t irq_handler(int irq, void *dev_id) {
    struct my_ADXL345 *adxl = (struct my_ADXL345 *)dev_id;
    u8 int_source;
    int ret_val;

    if (!adxl || !adxl->spi) return IRQ_NONE;

    ret_val = read_reg(adxl->spi, REG_INT_SOURCE, &int_source); // Clears ADXL345 IRQ flags
    if (ret_val) {
        dev_err(adxl->dev, "IRQ: Error reading INT_SOURCE: %d\n", ret_val);
        return IRQ_NONE;
    }

    if (int_source == 0) { // No relevant flags set, or spurious
        // dev_warn(adxl->dev, "IRQ: INT_SOURCE was 0x00 after read.\n"); // Optional: for debugging noise
        return IRQ_HANDLED;
    }

    if (int_source != 0x82){
        dev_dbg(adxl->dev, "IRQ: INT_SOURCE raw: 0x%02x\n", int_source); // Log what caused it
    }

    if (int_source & INT_DOUBLE_TAP) {
        dev_info(adxl->dev, "IRQ: DOUBLE_TAP detected!\n");
        adxl->last_double_tap_jiffies = jiffies;
    } else if (int_source & INT_SINGLE_TAP) {
        if (time_before(jiffies, adxl->last_double_tap_jiffies + msecs_to_jiffies(DOUBLE_TAP_COOLDOWN_MS))) {
            dev_dbg(adxl->dev, "IRQ: SINGLE_TAP (ignored: cooldown)\n");
        } else {
            dev_info(adxl->dev, "IRQ: SINGLE_TAP detected!\n");
        }
    }

    if (int_source & INT_DATA_READY) {
        mutex_lock(&adxl->lock);
        ret_val = get_data(adxl);
        if (ret_val) {
            dev_err(adxl->dev, "IRQ: get_data() failed in DATA_READY: %d\n", ret_val);
        }
        mutex_unlock(&adxl->lock);
    }

    return IRQ_HANDLED;
}

static int interrupt_init(struct my_ADXL345 *adxl, struct spi_device *spi)
{
    int irq_num, ret;

    // Configure Tap Detection Registers
    // dev_info(adxl->dev, "Configuring Tap Detection...\n"); // Minimal
    ret = write_reg(spi, REG_THRESH_TAP, 0x40); 
    if (ret) 
        return ret; // ~4g threshold (tune). Desired / 0.065 = THRESH
    ret = write_reg(spi, REG_DUR, 0x20);        
    if (ret) 
        return ret; // ~20ms duration (tune)
    ret = write_reg(spi, REG_LATENT, 0x50);     
    if (ret) 
        return ret; // 100ms latency (tune)
    ret = write_reg(spi, REG_WINDOW, 0xF0);     
    if (ret) 
        return ret; // 300ms window (tune)
    ret = write_reg(spi, REG_TAP_AXES, 0x07);   
    if (ret) 
        return ret; // Enable X,Y,Z tap

    // Setup Kernel-Side Interrupt Handling
    struct device_node *node = spi->dev.of_node;
    if (!node) { 
        dev_err(adxl->dev, "DT node not found\n"); 
        return -ENODEV; 
    }

    adxl->int1_gpio = of_get_named_gpio(node, "int1-gpio", 0);
    if (adxl->int1_gpio < 0) {
        return adxl->int1_gpio; 
    }
    // dev_info(adxl->dev, "DT int1-gpio: %d\n", adxl->int1_gpio);

    ret = devm_gpio_request_one(&spi->dev, adxl->int1_gpio, GPIOF_IN, "adxl345_int1");
    if (ret) { 
        dev_err(adxl->dev, "INT1 GPIO %d request failed: %d\n", adxl->int1_gpio, ret); 
        return ret; 
    }

    irq_num = gpio_to_irq(adxl->int1_gpio);
    if (irq_num < 0) { 
        dev_err(adxl->dev, "IRQ map for GPIO %d failed: %d\n", adxl->int1_gpio, irq_num); 
        return irq_num; 
    }
    adxl->irq = irq_num;
    dev_info(adxl->dev, "GPIO %d mapped to IRQ %d\n", adxl->int1_gpio, adxl->irq); 

    ret = devm_request_threaded_irq(&spi->dev, adxl->irq, NULL, irq_handler,
                                   IRQF_TRIGGER_RISING | IRQF_ONESHOT, DEVICE_NAME, adxl);
    if (ret) { dev_err(adxl->dev, "Request IRQ %d failed: %d\n", adxl->irq, ret); adxl->irq = -1; 
        return ret; 
    }
    // dev_info(adxl->dev, "Successfully requested IRQ %d\n", adxl->irq); 

    // Configure ADXL345 Interrupt Output (if kernel IRQ setup succeeded)
    if (adxl->irq >= 0) {
        // dev_info(adxl->dev, "Configuring ADXL345 HW interrupts...\n"); 
        ret = write_reg(spi, REG_INT_MAP, 0x00); 
        if (ret) 
            return ret; // Route all to INT1

        u8 int_enable_flags = INT_DATA_READY | INT_SINGLE_TAP | INT_DOUBLE_TAP;
        ret = write_reg(spi, REG_INT_ENABLE, int_enable_flags); 
        if (ret) 
            return ret;
    } else {
        dev_warn(adxl->dev, "Kernel IRQ not set, disabling ADXL345 HW interrupts.\n");
        write_reg(spi, REG_INT_ENABLE, 0x00);
    }

    return 0;
}

static void interrupts_cleanup(struct my_ADXL345 *adxl, struct spi_device *spi)
{
        if (adxl->spi) { // Check if spi pointer is valid
        write_reg(adxl->spi, REG_INT_ENABLE, 0x00); // Stop ADXL345 from generating interrupts
        dev_info(&spi->dev, "ADXL345 interrupts disabled\n");

        // Power down the ADXL345 (optional, good practice)
        write_reg(adxl->spi, REG_POWER_CTL, 0x00); // Put in standby mode
        dev_info(&spi->dev, "ADXL345 powered down to standby\n");
    }
}

#endif