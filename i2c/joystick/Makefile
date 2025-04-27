obj-m += i2c_ads.o

all: module dt
	echo Built .dtbo and kernel module
module:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules
dt:
	dtc -@ -I dts -O dtb -o ads_Overlay.dtbo ads_Overlay.dts
clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
	rm -rf ads_Overlay.dtbo
