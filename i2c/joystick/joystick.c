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
#define SLEEP_SECONDS 1

// Joystick neutral
#define horizontal_origin 12974
#define vertical_origin 13139

#define tolerance 500 // For noise

// Open sysfs file
int open_sysfs_file(const char *path, int flags) {
    int fd = open(path, flags);
    if (fd == -1) {
        fprintf(stderr, "Error opening %s: %s\n", path, strerror(errno));
    }
    return fd;
}

// Write to channel
int write_channel(int fd, int channel) {
    char buffer[BUFFER_SIZE];
    snprintf(buffer, sizeof(buffer), "%d", channel);

    if (write(fd, buffer, strlen(buffer)) == -1) {
        fprintf(stderr, "Error writing to channel: %s\n", strerror(errno));
        return -1;
    }
    return 0;
}

// Read from sysfs file
int read_sysfs_value(int fd, char *buffer, size_t size) {
    lseek(fd, 0, SEEK_SET);
    ssize_t bytes_read = read(fd, buffer, size - 1);

    if (bytes_read == -1) {
        fprintf(stderr, "Error reading from sysfs: %s\n", strerror(errno));
        return -1;
    }

    buffer[bytes_read] = '\0';
    return 0;
}

// Convert string to long
long parse_long(const char *str) {
    errno = 0;
    long val = strtol(str, NULL, 10);

    if (errno != 0) {
        fprintf(stderr, "Error converting string to long: %s\n", strerror(errno));
    }
    return val;
}

// Parse voltage string into volts + millivolts
int parse_voltage(const char *str, long *volts, long *mV) {
    return sscanf(str, "%ld.%03ld", volts, mV);
}

// Determine joystick direction, including corners (diagonal directions)
const char* get_direction(long raw_x, long raw_y) {
    int left  = (raw_x < (horizontal_origin - tolerance));
    int right = (raw_x > (horizontal_origin + tolerance));
    int up    = (raw_y < (vertical_origin - tolerance));
    int down  = (raw_y > (vertical_origin + tolerance));

    if (up && left) {
        return "UP-LEFT";
    }

    if (up && right) {
        return "UP-RIGHT";
    }

    if (down && left) {
        return "DOWN-LEFT";
    }

    if (down && right) {
        return "DOWN-RIGHT";
    }

    if (left) {
        return "LEFT";
    }

    if (right) {
        return "RIGHT";
    }

    if (up) {
        return "UP";
    }

    if (down) {
        return "DOWN";
    }

    return "CENTER";
}

int main() {
    int raw_fd = open_sysfs_file(SYSFS_RAW_PATH, O_RDONLY);
    int voltage_fd = open_sysfs_file(SYSFS_VOLTAGE_PATH, O_RDONLY);
    int channel_fd = open_sysfs_file(SYSFS_CHANNEL_PATH, O_WRONLY);

    if ((raw_fd == -1) || (voltage_fd == -1) || (channel_fd == -1)) {
        if (raw_fd != -1) {
          close(raw_fd);
        }

        if (voltage_fd != -1) {
          close(voltage_fd);
        }

        if (channel_fd != -1) {
          close(channel_fd);
        }
        return EXIT_FAILURE;
    }

    char raw_buffer[BUFFER_SIZE];
    char voltage_buffer[BUFFER_SIZE];
    long raw_value, volts, mV;
    long raw_x = 0, raw_y = 0;
    int channel = 0;

    while (1) {
        if (write_channel(channel_fd, channel) == -1) {
            break;
        }

        if (read_sysfs_value(raw_fd, raw_buffer, sizeof(raw_buffer)) == -1) {
            break;
        }

        raw_value = parse_long(raw_buffer);

        if (read_sysfs_value(voltage_fd, voltage_buffer, sizeof(voltage_buffer)) == -1) {
            break;
        }

        if (parse_voltage(voltage_buffer, &volts, &mV) != 2) {
            fprintf(stderr, "Error parsing voltage string: %s\n", voltage_buffer);
            break;
        }

        const char *axis = (channel == 0) ? "VRx (X-axis)" : "VRy (Y-axis)";
        printf("Channel %d [%s] → Raw: %ld, Voltage: %ld.%03ld V\n",
               channel, axis, raw_value, volts, mV);

        if (channel == 0) {
            raw_x = raw_value;
        } else {
            raw_y = raw_value;
            const char* direction = get_direction(raw_x, raw_y);
            printf("Joystick Direction: %s\n", direction);
        }

        sleep(SLEEP_SECONDS);
        channel = (channel == 0) ? 1 : 0;
    }

    close(raw_fd);
    close(voltage_fd);
    close(channel_fd);

    return EXIT_SUCCESS;
}
