#ifndef __MIKROBUS_H
#define __MIKROBUS_H

#include <linux/err.h>
#include <linux/errno.h>
#include <linux/i2c.h>
#include <linux/gpio.h>
#include <linux/spi/spi.h>
#include <linux/idr.h>
#include <linux/init.h>
#include <linux/jump_label.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/device.h>
#include <linux/of_device.h>
#include <linux/serdev.h>
#include <linux/of.h>
#include <linux/property.h>
#include <linux/slab.h>

extern struct bus_type mikrobus_bus_type;
extern struct device_type mikrobus_port_type;

enum mikrobus_property_type
{
	MIKROBUS_PROPERTY_TYPE_U8 = 0x00,
	MIKROBUS_PROPERTY_TYPE_U16 = 0x01,
	MIKROBUS_PROPERTY_TYPE_U32 = 0x02,
	MIKROBUS_PROPERTY_TYPE_U64 = 0x03,
};

enum mikrobus_irq_pin
{
	MIKROBUS_IRQ_INVALID = 0x00,
	MIKROBUS_IRQ_INT = 0x01,
	MIKROBUS_IRQ_RST = 0x02,
	MIKROBUS_IRQ_PWM = 0x03,
};

enum mikrobus_protocol
{
	MIKROBUS_PROTOCOL_SPI = 0x01,
	MIKROBUS_PROTOCOL_I2C = 0x02,
	MIKROBUS_PROTOCOL_UART = 0x03,
	MIKROBUS_PROTOCOL_SPI_GPIOCS = 0x04,
	MIKROBUS_PROTOCOL_I2C_MUX = 0x05
};

struct mikrobus_port_config
{
	__u8 i2c_adap_nr;
	__u8 spi_master_nr;
	__u8 serdev_ctlr_nr;
	__u8 rst_gpio_nr;
	__u8 pwm_gpio_nr;
	__u8 int_gpio_nr;
} __packed;

struct click_device_info
{
	struct list_head links;
	char *drv_name;
	unsigned short protocol;
	unsigned short reg;
	unsigned int max_speed_hz;
	unsigned int mode;
	int irq;
	int irq_type;
	struct gpio_desc *cs_gpiod;
	unsigned short num_properties;
	struct property_entry *properties;
	struct i2c_client *i2c_dev;
	struct spi_device *spi_dev;
};

struct click_board_info
{
	char *name;
	unsigned short num_devices;
	unsigned short rst_gpio_state;
	unsigned short pwm_gpio_state;
	unsigned short int_gpio_state;
	struct list_head manifest_descs;
	struct list_head devices;
};

struct mikrobus_port
{
	char name[48];
	struct module *owner;
	struct device dev;
	int id;
	struct gpio_desc *pwm_gpio;
	struct gpio_desc *int_gpio;
	struct gpio_desc *rst_gpio;
	struct spi_master *spi_mstr;
	struct i2c_adapter *i2c_adap;
};
#define to_mikrobus_port(d) container_of(d, struct mikrobus_port, dev)

int mikrobus_register_device(struct mikrobus_port *port, struct click_device_info *dev);
void mikrobus_unregister_device(struct mikrobus_port *port, struct click_device_info *dev);
int mikrobus_register_port_config(struct mikrobus_port_config *cfg);
int mikrobus_register_port(struct mikrobus_port *port);
void mikrobus_del_port(struct mikrobus_port *port);

#endif /* __MIKROBUS_H */