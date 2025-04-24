#include <linux/module.h>
#include <linux/init.h>
#include <linux/mod_devicetable.h>
#include <linux/property.h>
#include <linux/of_device.h>
#include <linux/serdev.h>
#include <linux/delay.h>  // for msleep()

#define MAX_BUFFER_SIZE 256

static int ser_echo_probe(struct serdev_device *serdev);
static void ser_echo_remove(struct serdev_device *serdev);

static const struct of_device_id ser_echo_ids[] = {
    { .compatible = "decryptec,echo_dev" },
    {} // Sentinel
};
MODULE_DEVICE_TABLE(of, ser_echo_ids);

static int serdev_echo_recv(struct serdev_device *serdev, const unsigned char *buffer, size_t size)
{
    static char rx_buffer[MAX_BUFFER_SIZE];
    static size_t rx_index = 0;

    size_t i;
    for (i = 0; i < size; i++) {
        if (rx_index < MAX_BUFFER_SIZE - 1) {
            char c = buffer[i];
            rx_buffer[rx_index++] = c;

            if (c == '\n') {
                rx_buffer[rx_index - 1] = '\0'; // Terminate string
                pr_info("echo - Received message: %s\n", rx_buffer);
                rx_index = 0;
            }
        } else {
            pr_warn("echo - Buffer overflow, resetting\n");
            rx_index = 0;
        }
    }

    return serdev_device_write_buf(serdev, buffer, size); // echo back (optional)
}

static const struct serdev_device_ops ser_echo_ops = {
    .receive_buf = serdev_echo_recv,
};

static int ser_echo_probe(struct serdev_device *serdev)
{
    int status;

    pr_info("echo - Probe called\n");

    serdev_device_set_client_ops(serdev, &ser_echo_ops);

    status = serdev_device_open(serdev);
    if (status) {
        pr_err("echo - Error opening serial port (%d)\n", status);
        return -status;
    }

    pr_info("echo - Configuring UART\n");

    serdev_device_set_baudrate(serdev, 115200);
    serdev_device_set_flow_control(serdev, false);
    serdev_device_set_parity(serdev, SERDEV_PARITY_NONE);

    msleep(100); // Allow Arduino time to boot/reset

    // Send initial command
    const char* ON_cmd = "ON\n";
    int bytes_sent = serdev_device_write_buf(serdev, ON_cmd, strlen(ON_cmd));
    pr_info("echo - Sent LED_ON (%d bytes)\n", bytes_sent);

    return 0;
}

static void ser_echo_remove(struct serdev_device *serdev)
{
    pr_info("echo - Remove called\n");

    const char* OFF_cmd = "OFF\n";
    int bytes_sent = serdev_device_write_buf(serdev, OFF_cmd, strlen(OFF_cmd));
    pr_info("echo - Sent LED_OFF (%d bytes)\n", bytes_sent);

    serdev_device_close(serdev);
}

static struct serdev_device_driver serdev_device_driver = {
    .probe = ser_echo_probe,
    .remove = ser_echo_remove,
    .driver = {
        .name = "uart-echo",
        .of_match_table = ser_echo_ids,
    },
};

static int __init my_init(void)
{
    pr_info("echo - Loading driver\n");

    int ret = serdev_device_driver_register(&serdev_device_driver);
    if (ret) {
        pr_err("echo - Could not load driver (%d)\n", ret);
        return ret;
    }

    pr_info("echo - Driver loaded successfully\n");
    return 0;
}

static void __exit my_exit(void)
{
    pr_info("echo - Unloading driver\n");
    serdev_device_driver_unregister(&serdev_device_driver);
}

module_init(my_init);
module_exit(my_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Decryptec");
MODULE_DESCRIPTION("UART Echo + LED Toggle Driver for Arduino");
