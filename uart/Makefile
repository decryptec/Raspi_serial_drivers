obj-m += echo.o

all: module dt
	echo Built Device Tree Overlay and kernel module

module:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules
dt: uart_Overlay.dts
	dtc -@ -I dts -O dtb -o uart_Overlay.dtbo uart_Overlay.dts
clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
	rm -rf uart_Overlay.dtbo
