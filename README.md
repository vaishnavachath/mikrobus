# mikroBUS

mikroBUS kernel driver for instantiating mikroElektronika Click Boards from Manifest descriptors, See [eLinux/mikrobus](https://elinux.org/mikrobus) for more information.

## Image Tested on :
```
debian@beaglebone:~/mikrobus$ cat /etc/dogtag 
BeagleBoard.org Debian Buster IoT Image 2020-04-06
debian@beaglebone:~/mikrobus$ uname -a
Linux beaglebone 5.7.0-rc5-bone4 #1buster PREEMPT Mon May 11 16:23:42 UTC 2020 armv7l GNU/Linux
```

## Trying out :

```
git clone https://github.com/vaishnav98/mikrobus.git
cd mikrobus
make all
sudo insmod mikrobus.ko
```
## Status

* Basic Clicks and Clicks with IRQ Requirement working

## TODO

* Manifest Parsing Logic not complete (so right now for demonstration insmod command-line parameters are used)
* Fetching Manifest from EEPROM
* Multiple Devices on a Click(in single manifest)
* Devices under an i2c-gate
* Devices with gpio_cs

## MPU9DOF Click On PocketBeagle mikroBUS port 1 (Techlab mikroBUS port)
```
sudo insmod mikrobus_port.ko i2c_adap_nr=1 spi_master_nr=0 serdev_ctlr_nr=4 rst_gpio_nr=89 pwm_gpio_nr=50 int_gpio_nr=23 drvname=mpu9150 reg=104 protocol=2 irq=1 irq_type=1
```
## MPU9DOF Click On PocketBeagle mikroBUS port 2
```
sudo insmod mikrobus_port.ko i2c_adap_nr=2 spi_master_nr=1 serdev_ctlr_nr=1 rst_gpio_nr=45 pwm_gpio_nr=110 int_gpio_nr=26 drvname=mpu9150 reg=104 protocol=2 irq=1 irq_type=1
```
