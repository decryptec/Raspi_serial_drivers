obj-m += ADXL345_spi.o

all: module dt
	echo Built DTO and Kernel module
module:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules
dt: ADXL345_Overlay.dts
	dtc -@ -I dts -O dtb -o ADXL345_Overlay.dtbo ADXL345_Overlay.dts
overlay: ADXL345_Overlay.dtbo	
	sudo cp ADXL345_Overlay.dtbo /boot/overlays
load: ADXL345_spi.ko
	sudo insmod ADXL345_spi.ko
rm:
	sudo rmmod ADXL345_spi
clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
	rm -rf ADXL345_Overlay.dtbo
