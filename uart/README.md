# UART Button State Receiver (Arduino to Raspberry Pi 4B via serdev)

This project demonstrates a custom Linux kernel module using the `serdev` API to receive UART messages from an Arduino. When the button on the Arduino is pressed or released, it sends `BUTTON_PRESSED` or `BUTTON_RELEASED` over UART. The Raspberry Pi 4B captures and logs these messages using a kernel module.

---

## 🔧 Setup (Single Command Flow)

```bash
# 1. Copy Device Tree Overlay
sudo cp uart_Overlay.dtbo /boot/overlays/

# 2. Enable UART interface
sudo raspi-config
# → Interface Options → Serial Port
# → Login shell: No
# → Serial port hardware: Yes

# 3. Edit boot config to apply overlay
sudo vim /boot/firmware/config.txt
# Add this line:
dtoverlay=uart_Overlay

# 4. Reboot to apply changes
sudo reboot now

# 5. Insert the kernel module and watch logs
sudo insmod echo.ko
sudo dmesg -W
