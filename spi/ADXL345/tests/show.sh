#!/bin/bash

# Define color codes
RED='\033[0;31m'
GREEN='\033[0;32m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
YELLOW='\033[0;33m'
RESET='\033[0m'  # Reset color

echo -e "${GREEN}Checking kernel modules...${RESET}"
echo -e "${YELLOW}lsmod | grep ADX output:${RESET}"
lsmod | grep ADX

echo -e "${GREEN}Showing sysfs entries...${RESET}"
sleep 2
ls /sys/bus/spi/devices/spi0.0/adxl345_class/adxl345/device/
sleep 2

echo -e "${CYAN}Custom attributes added:${RESET}"
echo -e "${BLUE}cat rate output:${RESET}"
cat /sys/bus/spi/devices/spi0.0/adxl345_class/adxl345/device/rate
echo -e "${BLUE}cat range output:${RESET}"
cat /sys/bus/spi/devices/spi0.0/adxl345_class/adxl345/device/range
sleep 2

echo -e "${CYAN}Configure:${RESET}"
echo -e "${BLUE}echo "200" > rate:${RESET}"
echo "200" > /sys/bus/spi/devices/spi0.0/adxl345_class/adxl345/device/rate
echo -e "${BLUE}echo "4" > range:${RESET}"
echo "4" > /sys/bus/spi/devices/spi0.0/adxl345_class/adxl345/device/range
sleep 2

echo -e "${BLUE}cat rate output:${RESET}"
cat /sys/bus/spi/devices/spi0.0/adxl345_class/adxl345/device/rate
echo -e "${BLUE}cat range output:${RESET}"
cat /sys/bus/spi/devices/spi0.0/adxl345_class/adxl345/device/range
sleep 2

echo -e "${GREEN}Checking cdev entries...${RESET}"
echo -e "${YELLOW}/dev/ | grep adx output:${RESET}"
ls /dev/ | grep adx

# Reset terminal color at the end
echo -e "${RESET}"

