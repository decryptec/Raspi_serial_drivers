#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>

#define RAW_VALUE_PATH "/sys/class/i2c-dev/i2c-1/device/1-0048/raw_value"  // Adjust if necessary
#define VREF 3.3
#define ADS1115_FULL_SCALE 32768.0 // Use floating-point for calculations

int main() {
    int fd, raw_value;
    float voltage;
    char buf[16];  // Buffer to read the raw value

    // Open the raw_value sysfs entry
    fd = open(RAW_VALUE_PATH, O_RDONLY);
    if (fd < 0) {
        perror("Failed to open raw_value");
        return 1;
    }

    // Read the raw value from the sysfs entry
    if (read(fd, buf, sizeof(buf)) < 0) {
        perror("Failed to read raw_value");
        close(fd);
        return 1;
    }

    // Convert the raw value to an integer
    raw_value = atoi(buf);

    // Close the file descriptor
    close(fd);

    // Calculate the voltage
    voltage = (raw_value / ADS1115_FULL_SCALE) * VREF;

    // Print the results
    printf("Raw ADC Value: %d\n", raw_value);
    printf("Voltage: %.3f V\n", voltage);

    return 0;
}
