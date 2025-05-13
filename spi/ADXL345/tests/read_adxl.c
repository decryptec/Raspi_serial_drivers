#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <string.h>

#define CDEV_PATH "/dev/adxl345"

char buf[64];
int read_counter = 0;

int main(){
	int dev = open(CDEV_PATH, O_RDONLY);
	if (dev == -1) {
		perror("Failed to open LCD device");
		return -1;
	}
	while(read_counter < 15){
		ssize_t bytes_read = read(dev, buf, sizeof(buf) - 1);
		if (bytes_read == -1) {
			perror("Failed to read from LCD device");
			close(dev);
			return -1;
		}
		buf[bytes_read] = '\0';
		printf("Content read from adxl device:\n %s\n", buf);
		read_counter++;
		sleep(1);
	}
	close(dev);
	return 0;
}
