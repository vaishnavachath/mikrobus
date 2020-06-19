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
* Debug Interfaces for adding and Removing mikroBUS ports
* Multiple Devices on a Click(in single manifest)
* Manifest Parsing Logic complete
* Fetching Manifest from EEPROM

## TODO
* Devices under an i2c-gate
* Devices with gpio_cs

## Attaching PocketBeagle mikroBUS port 1 (Techlab mikroBUS port)(run as root)
```
printf "%b" '\x01\x00\x00\x59\x32\x17' > /sys/bus/mikrobus/add_port
```
The bytes in the byte array sequence are (in order):
* i2c_adap_nr
* spi_master_nr
* serdev_ctlr_nr
* rst_gpio_nr
* pwm_gpio_nr
* int_gpio_nr

Note:- Attaching the mikrobus driver automatically probes an EEPROM on the I2C bus and if the probe is succesful, the driver tries to load a manifest from the eeprom and instantiate the click devices on the mikrobus port.
## Instantiating a Click Device through a manifest blob

See [manifesto tool](https://github.com/vaishnav98/manifesto/tree/mikrobus) for creating manifest blobs and instantiating clicks on the mikrobus port.
