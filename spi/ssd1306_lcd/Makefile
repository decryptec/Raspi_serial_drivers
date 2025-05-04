j-m += spi_lcd.o

all: module dt
	echo Built .dtbo and kernel module
module:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules
dt:
	dtc -@ -I dts -O dtb -o my_LCD.dtbo my_LCD.dts
clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
	rm -rf my_LCD.dtbo
