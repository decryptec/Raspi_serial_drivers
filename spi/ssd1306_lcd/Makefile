j-m += spi_lcd.o

all: module dt
	echo Built .dtbo and kernel module
module:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules
dt:
	dtc -@ -I dts -O dtb -o my_SSD1306.dtbo my_SSD1306.dts
clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
	rm -rf my_SSD1306.dtbo