#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

#define SYSFS_RAW_PATH "/sys/class/i2c-dev/i2c-1/device/1-0048/raw_value"
#define SYSFS_VOLTAGE_PATH "/sys/class/i2c-dev/i2c-1/device/1-0048/voltage"
#define SYSFS_CHANNEL_PATH "/sys/class/i2c-dev/i2c-1/device/1-0048/channel"
#define BUFFER_SIZE 64
#define SLEEP_SECONDS 1 // Interval between readings

int main() {
    int raw_fd, voltage_fd, channel_fd;
    char raw_buffer[BUFFER_SIZE];
    char voltage_buffer[BUFFER_SIZE];
    char channel_buffer[BUFFER_SIZE];
    ssize_t raw_bytes_read, voltage_bytes_read, channel_bytes_written;
    long raw_value;
    long volts;
    long mV;
    int channel = 0; // Start with channel 0

    // Open the sysfs files for reading
    raw_fd = open(SYSFS_RAW_PATH, O_RDONLY);
    if (raw_fd == -1) {
        fprintf(stderr, "Error opening %s: %s\n", SYSFS_RAW_PATH, strerror(errno));
        return EXIT_FAILURE;
    }

    voltage_fd = open(SYSFS_VOLTAGE_PATH, O_RDONLY);
    if (voltage_fd == -1) {
        fprintf(stderr, "Error opening %s: %s\n", SYSFS_VOLTAGE_PATH, strerror(errno));
        close(raw_fd);
        return EXIT_FAILURE;
    }

    // Open the channel sysfs file for writing
    channel_fd = open(SYSFS_CHANNEL_PATH, O_WRONLY);
    if (channel_fd == -1) {
        fprintf(stderr, "Error opening %s: %s\n", SYSFS_CHANNEL_PATH, strerror(errno));
        close(raw_fd);
        close(voltage_fd);
        return EXIT_FAILURE;
    }

    while (1) {
        // Set the channel
        snprintf(channel_buffer, sizeof(channel_buffer), "%d", channel);
        channel_bytes_written = write(channel_fd, channel_buffer, strlen(channel_buffer));
        if (channel_bytes_written == -1) {
            fprintf(stderr, "Error writing to %s: %s\n", SYSFS_CHANNEL_PATH, strerror(errno));
            break;
        }
        printf("Setting channel to %d\n", channel);

        // Read the raw value from the sysfs file
        lseek(raw_fd, 0, SEEK_SET);
        raw_bytes_read = read(raw_fd, raw_buffer, sizeof(raw_buffer) - 1);
        if (raw_bytes_read == -1) {
            fprintf(stderr, "Error reading from %s: %s\n", SYSFS_RAW_PATH, strerror(errno));
            break;
        }
        raw_buffer[raw_bytes_read] = '\0';

        // Convert the raw value to a long integer
        raw_value = strtol(raw_buffer, NULL, 10);
        if (errno != 0) {
            fprintf(stderr, "Error converting raw value to integer: %s\n", strerror(errno));
            break;
        }

        // Read the voltage from the sysfs file
        lseek(voltage_fd, 0, SEEK_SET);
        voltage_bytes_read = read(voltage_fd, voltage_buffer, sizeof(voltage_buffer) - 1);
        if (voltage_bytes_read == -1) {
            fprintf(stderr, "Error reading from %s: %s\n", SYSFS_VOLTAGE_PATH, strerror(errno));
            break;
        }
        voltage_buffer[voltage_bytes_read] = '\0';

        // Extract volts and mV
        if (sscanf(voltage_buffer, "%ld.%03ld", &volts, &mV) != 2) {
            fprintf(stderr, "Error parsing voltage string: %s\n", voltage_buffer);
            break;
        }

        // Print the values
        printf("Raw ADC Value: %ld, Voltage: %ld.%03ld V, Channel: %d\n", raw_value, volts, mV, channel);

        sleep(SLEEP_SECONDS);

        // Cycle to the next channel
        channel = (channel + 1) % 4; // Cycle through channels 0, 1, 2, 3
    }

    // Close the sysfs files
    close(raw_fd);
    close(voltage_fd);
    close(channel_fd);

    return EXIT_SUCCESS;
}
