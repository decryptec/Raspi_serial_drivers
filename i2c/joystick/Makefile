obj-m += joystick_ads.o

all: module dt
	echo Built .dtbo and kernel module
module:
        make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules
dt:
	dtc -@ -I dts -O dtb -o ads1115.dtbo ads1115.dts
clean:
        make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
	rm -rf ads1115.dtbo
