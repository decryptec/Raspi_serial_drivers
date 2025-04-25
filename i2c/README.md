# I2C Tools Setup on Raspberry Pi 4B

## ✅ Step-by-Step Guide

### 1. Enable I2C Interface
Use the Raspberry Pi configuration tool to enable the I2C interface:

```bash
sudo raspi-config
```
- Navigate to: `Interface Options` → `I2C` → **Enable**
- Exit and reboot:

```bash
sudo reboot
```

---

### 2. Install I2C Tools
Update your package list and install the `i2c-tools` package:

```bash
sudo apt update
sudo apt install -y i2c-tools
```

---

### 3. Check I2C Kernel Modules
Ensure the I2C drivers are loaded:

```bash
lsmod | grep i2c
```
You should see `i2c_bcm2835` and `i2c_dev`. If not, load them manually:

```bash
sudo modprobe i2c_bcm2835
sudo modprobe i2c_dev
```

---

### 4. Add User to I2C Group *(Optional)*
Add your user to the `i2c` group so you can run tools without `sudo`:

```bash
sudo usermod -aG i2c $USER
```
Reboot or log out and back in:

```bash
sudo reboot
```

---

### 5. Verify I2C Device Detection
Scan the I2C bus (typically `1` on Raspberry Pi 4B):

```bash
sudo /usr/sbin/i2cdetect -y 1
```

If `/usr/sbin` is not in your `$PATH`, add it:

```bash
echo 'export PATH="$PATH:/usr/sbin"' >> ~/.bashrc
source ~/.bashrc
```
