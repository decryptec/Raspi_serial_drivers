# Raspberry Pi Kernel Module Practice

Focused on creating and experimenting with kernel modules for UART, I2C, and SPI on a Raspberry Pi. 

### **1. Install Dependencies**  
```sh
sudo apt update && sudo apt upgrade -y
sudo apt install -y raspberrypi-kernel-headers build-essential gpiod
```

## Project Structure

- `uart/`: Contains kernel modules related to UART.
- `i2c/`
  - `i2c_char/`: I2C kernel modules without the device tree.
  - `i2c_dts/`: I2C kernel modules with the device tree.
- `spi/`
  - `spi_char/`: SPI kernel modules without the device tree.
  - `spi_dts/`: SPI kernel modules with the device tree.
