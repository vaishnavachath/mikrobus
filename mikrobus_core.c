#define pr_fmt(fmt) "mikrobus: " fmt

#include <linux/err.h>
#include <linux/errno.h>
#include <linux/idr.h>
#include <linux/init.h>
#include <linux/jump_label.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/gpio/consumer.h>
#include <linux/mutex.h>
#include <linux/device.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/i2c.h>
#include <linux/gpio.h>
#include <linux/gpio/machine.h>
#include <linux/interrupt.h>
#include <linux/spi/spi.h>
#include <linux/serdev.h>
#include <linux/property.h>
#include <linux/slab.h>

#include "mikrobus_core.h"
#include "mikrobus_manifest.h"

static DEFINE_IDR(mikrobus_port_idr);
static struct class_compat *mikrobus_port_compat_class;
static bool is_registered;

static ssize_t
add_port_store(struct bus_type *bt, const char *buf, size_t count)
{
	struct mikrobus_port_config *cfg;

	if (count < sizeof(*cfg))
	{
		pr_err("new_port:Incorrect CFG data received: %s \n", buf);
		return -EINVAL;
	}

	mikrobus_register_port_config((void *)buf);

	return count;
}
BUS_ATTR_WO(add_port);

static ssize_t
del_port_store(struct bus_type *bt, const char *buf, size_t count)
{
	int id;
	char end;
	int res;

	res = sscanf(buf, "%d%c", &id, &end);
	if (res < 1)
	{
		pr_err("delete_port: Can't parse Mikrobus Port ID\n");
		return -EINVAL;
	}

	mikrobus_del_port(idr_find(&mikrobus_port_idr, id));

	return count;
}
BUS_ATTR_WO(del_port);

static struct attribute *mikrobus_attrs[] = {
	&bus_attr_add_port.attr,
	&bus_attr_del_port.attr,
	NULL};
ATTRIBUTE_GROUPS(mikrobus);

struct bus_type mikrobus_bus_type = {
	.name = "mikrobus",
	.bus_groups = mikrobus_groups,
};
EXPORT_SYMBOL_GPL(mikrobus_bus_type);

static ssize_t
name_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%s\n", to_mikrobus_port(dev)->name);
}
static DEVICE_ATTR_RO(name);

static ssize_t
new_device_store(struct device *dev, struct device_attribute *attr,
				 const char *buf, size_t count)
{
	struct mikrobus_port *port = to_mikrobus_port(dev);
	struct click_board_info *click;

	int retval;

	click = kzalloc(sizeof(*click), GFP_KERNEL);
	if (!click) {
		return 0;
	}

	INIT_LIST_HEAD(&click->manifest_descs);
	INIT_LIST_HEAD(&click->devices);

	retval = mikrobus_manifest_parse(click, (void *) buf, count);

	if (!retval)
	{
		pr_err("failed to parse manifest, size: %d", count);
		return 0;
	}

	retval = mikrobus_register_click(port, click);
	if (retval)
	{
		pr_err("failed to register click: %s", click->name);
		return 0;
	}

	return count;
}
static DEVICE_ATTR_WO(new_device);

static ssize_t
delete_device_store(struct device *dev, struct device_attribute *attr,
					const char *buf, size_t count)
{
	int id;
	char end;
	int res;
	struct mikrobus_port *port = to_mikrobus_port(dev);

	res = sscanf(buf, "%d%c", &id, &end);
	if (res < 1)
	{
		pr_err("delete_click: Can't parse Click ID\n");
		return -EINVAL;
	}

	mikrobus_unregister_click(port, port->click);

	return count;
}
static DEVICE_ATTR_IGNORE_LOCKDEP(delete_device, S_IWUSR, NULL,
								  delete_device_store);

static struct attribute *mikrobus_port_attrs[] = {
	&dev_attr_new_device.attr,
	&dev_attr_delete_device.attr,
	&dev_attr_name.attr,
	NULL};
ATTRIBUTE_GROUPS(mikrobus_port);

static void mikrobus_dev_release(struct device *dev)
{
	struct mikrobus_port *port = to_mikrobus_port(dev);
	idr_remove(&mikrobus_port_idr, port->id);
	kfree(port);
}

struct device_type mikrobus_port_type = {
	.groups = mikrobus_port_groups,
	.release = mikrobus_dev_release,
};
EXPORT_SYMBOL_GPL(mikrobus_port_type);

static int mikrobus_get_irq(struct mikrobus_port *port, int irqno, int irq_type)
{
	int irq;

	switch (irqno)
	{

	case MIKROBUS_GPIO_INT:
		irq = gpiod_to_irq(port->int_gpio);
		break;
	case MIKROBUS_GPIO_RST:
		irq = gpiod_to_irq(port->rst_gpio);
		break;
	case MIKROBUS_GPIO_PWM:
		irq = gpiod_to_irq(port->pwm_gpio);
		break;
	default:
		return -EINVAL;
	}

	if (irq < 0)
	{
		pr_err("Could not get irq for irq type: %d", irqno);
		return -EINVAL;
	}

	irq_set_irq_type(irq, irq_type);

	return irq;
}

static int mikrobus_setup_gpio(struct gpio_desc *gpio, int gpio_state)
{
	int retval;

	if(gpio_state == MIKROBUS_GPIO_UNUSED)
		return 0;
		
	switch(gpio_state){
		case MIKROBUS_GPIO_INPUT:
		retval = gpiod_direction_input(gpio);
		break;
		case MIKROBUS_GPIO_OUTPUT_HIGH:
		retval = gpiod_direction_output(gpio, 1);
		break;
		case MIKROBUS_GPIO_OUTPUT_LOW:
		retval = gpiod_direction_output(gpio, 0);
		break;
		default:
		return -EINVAL;
	}

	return retval;
}

static void mikrobus_spi_device_delete(struct spi_master *master, unsigned int cs)
{
	struct device *dev;
	char str[32];

	snprintf(str, sizeof(str), "%s.%u", dev_name(&master->dev), cs);

	dev = bus_find_device_by_name(&spi_bus_type, NULL, str);
	if (dev != NULL)
	{	
		spi_unregister_device(to_spi_device(dev));		
		put_device(dev);
	}
}

static char *mikrobus_get_gpio_chip_name(struct mikrobus_port *port, int gpio)
{
	char *name;
	struct gpio_chip *gpiochip;

	switch (gpio)
	{
	case MIKROBUS_GPIO_INT:	
		gpiochip = gpiod_to_chip(port->int_gpio);
		name = kmemdup(gpiochip->label , 40, GFP_KERNEL);
		break;
	case MIKROBUS_GPIO_RST:
		gpiochip = gpiod_to_chip(port->rst_gpio);
		name = kmemdup(gpiochip->label , 40, GFP_KERNEL);
		break;
	case MIKROBUS_GPIO_PWM:
		gpiochip = gpiod_to_chip(port->pwm_gpio);
		name = kmemdup(gpiochip->label , 40, GFP_KERNEL);
		break;
	}

	return name;
}

static int mikrobus_get_gpio_hwnum(struct mikrobus_port *port, int gpio)
{
	int hwnum;

	struct gpio_chip *gpiochip;

	switch (gpio)
	{
	case MIKROBUS_GPIO_INT:	
		gpiochip = gpiod_to_chip(port->int_gpio);
		hwnum = desc_to_gpio(port->int_gpio) - gpiochip->base;
		break;
	case MIKROBUS_GPIO_RST:	
		gpiochip = gpiod_to_chip(port->rst_gpio);
		hwnum = desc_to_gpio(port->rst_gpio) - gpiochip->base;
		break;
	case MIKROBUS_GPIO_PWM:	
		gpiochip = gpiod_to_chip(port->pwm_gpio);
		hwnum = desc_to_gpio(port->pwm_gpio) - gpiochip->base;
		break;
	}

	return hwnum;	
}

static int mikrobus_register_device(struct mikrobus_port *port, struct click_device_info *dev, char *click_name)
{
	struct i2c_board_info *i2c;
	struct spi_board_info *spi;
	struct gpiod_lookup_table *lookup;
	char devname[40];
	char sys_link_name[40];
	int i;
	int retval;

	pr_info(" registering device : %s \n", dev->drv_name);
	snprintf(sys_link_name, sizeof(sys_link_name), "%s-%s", click_name , dev->drv_name);

	if(dev->gpio_lookup != NULL)
	{
		lookup=dev->gpio_lookup;

		if(dev->protocol == MIKROBUS_PROTOCOL_SPI){
			snprintf(devname, sizeof(devname), "%s.%u", dev_name(&port->spi_mstr->dev) , dev->reg);
			lookup->dev_id =  kmemdup(devname , 40, GFP_KERNEL);;
		}
		else if(dev->protocol == MIKROBUS_PROTOCOL_I2C)
			lookup->dev_id = dev->drv_name;
		
		pr_info(" adding lookup table : %s \n", lookup->dev_id);

		for (i = 0; i < dev->num_gpio_resources; i++) {
			lookup->table[i].chip_label = mikrobus_get_gpio_chip_name(port, lookup->table[i].chip_hwnum);
			lookup->table[i].chip_hwnum = mikrobus_get_gpio_hwnum(port, lookup->table[i].chip_hwnum);
			lookup->table[i].flags = GPIO_ACTIVE_HIGH;
		}

		gpiod_add_lookup_table(lookup);
	}

	switch (dev->protocol)
	{

	case MIKROBUS_PROTOCOL_SPI:
		spi = kzalloc(sizeof(*spi), GFP_KERNEL);
		if (!spi)
		{
			return -ENOMEM;
		}
		strncpy(spi->modalias, dev->drv_name, sizeof(spi->modalias) - 1);
		if (dev->irq)
			spi->irq = mikrobus_get_irq(port, dev->irq, dev->irq_type);
		if (dev->properties)
			spi->properties = dev->properties;
		spi->chip_select = dev->reg;
		spi->max_speed_hz = dev->max_speed_hz;
		spi->mode = dev->mode;
		mikrobus_spi_device_delete(port->spi_mstr, dev->reg);
		dev->dev_client = (void *)spi_new_device(port->spi_mstr, spi);
		retval = sysfs_create_link (&port->dev.kobj, &((struct spi_device *)dev->dev_client)->dev.kobj, sys_link_name);
		break;
	case MIKROBUS_PROTOCOL_I2C:
		i2c = kzalloc(sizeof(*i2c), GFP_KERNEL);
		if (!i2c)
		{
			return -ENOMEM;
		}
		strncpy(i2c->type, dev->drv_name, sizeof(i2c->type) - 1);
		if (dev->irq)
			i2c->irq = mikrobus_get_irq(port, dev->irq, dev->irq_type);
		if (dev->properties)
			i2c->properties = dev->properties;
		i2c->addr = dev->reg;
		dev->dev_client = (void *)i2c_new_device(port->i2c_adap, i2c);
		retval = sysfs_create_link (&port->dev.kobj, &((struct i2c_client *)dev->dev_client)->dev.kobj, sys_link_name);
		break;
	case MIKROBUS_PROTOCOL_UART:
		pr_info("SERDEV Devices Support Not Yet Added");
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static void mikrobus_unregister_device(struct mikrobus_port *port, struct click_device_info *dev, char *click_name)
{
	char sys_link_name[40];

	snprintf(sys_link_name, sizeof(sys_link_name), "%s-%s", click_name , dev->drv_name);

	pr_info(" removing device : %s \n", dev->drv_name);

	if(dev->gpio_lookup != NULL)
		 gpiod_remove_lookup_table(dev->gpio_lookup);

	sysfs_remove_link(&port->dev.kobj, sys_link_name);

	switch (dev->protocol)
	{

	case MIKROBUS_PROTOCOL_SPI:
		spi_unregister_device((struct spi_device *)dev->dev_client);
		break;
	case MIKROBUS_PROTOCOL_I2C:
		i2c_unregister_device((struct i2c_client *)dev->dev_client);
		break;
	case MIKROBUS_PROTOCOL_UART:
		pr_info("SERDEV Devices Support Not Yet Added");
		break;
	}
}


int mikrobus_register_click(struct mikrobus_port *port, struct click_board_info *board)
{	
	struct click_device_info *devinfo;
	struct click_device_info *next;
	int retval;

	if (WARN_ON(list_empty(&board->devices)))
		return false;
		
	retval = mikrobus_setup_gpio(port->pwm_gpio, board->pwm_gpio_state);
	if (retval)
	{
		pr_err("mikrobus_setup_gpio : can't setup pwm gpio state: (%d)\n", retval);
		return retval;
	}

	retval = mikrobus_setup_gpio(port->int_gpio, board->int_gpio_state);
	if (retval)
	{
		pr_err("mikrobus_setup_gpio : can't setup int gpio state: (%d)\n", retval);
		return retval;
	}

	retval = mikrobus_setup_gpio(port->rst_gpio, board->rst_gpio_state);
	if (retval)
	{
		pr_err("mikrobus_setup_gpio : can't setup rst gpio state: (%d)\n", retval);
		return retval;
	}

	list_for_each_entry_safe(devinfo, next, &board->devices, links)
		mikrobus_register_device(port, devinfo, board->name);

	port->click = board;

	return 0;
}
EXPORT_SYMBOL_GPL(mikrobus_register_click);

void mikrobus_unregister_click(struct mikrobus_port *port, struct click_board_info *board)
{	
	struct click_device_info *devinfo;
	struct click_device_info *next;
	
	if (WARN_ON(list_empty(&board->devices)))
		return ;

	port->click = NULL;

	list_for_each_entry_safe(devinfo, next, &board->devices, links)
		mikrobus_unregister_device(port, devinfo, board->name);
}
EXPORT_SYMBOL_GPL(mikrobus_unregister_click);

int mikrobus_register_port_config(struct mikrobus_port_config *cfg)
{
	struct mikrobus_port *port;
	int retval;

	if (WARN_ON(!is_registered))
		return -EAGAIN;

	port = kzalloc(sizeof(*port), GFP_KERNEL);
	if (!port)
	{
		return -ENOMEM;
	}

	port->pwm_gpio = gpio_to_desc(cfg->pwm_gpio_nr);
	port->int_gpio = gpio_to_desc(cfg->int_gpio_nr);
	port->rst_gpio = gpio_to_desc(cfg->rst_gpio_nr);
	port->spi_mstr = spi_busnum_to_master(cfg->spi_master_nr);
	port->i2c_adap = i2c_get_adapter(cfg->i2c_adap_nr);

	retval = mikrobus_register_port(port);
	if (retval)
	{
		pr_err("port : can't register port from config (%d)\n", retval);
		return retval;
	}

	return retval;
}
EXPORT_SYMBOL_GPL(mikrobus_register_port_config);

int mikrobus_register_port(struct mikrobus_port *port)
{
	int retval;
	int id;

	if (WARN_ON(!is_registered))
		return -EAGAIN;

	id = idr_alloc(&mikrobus_port_idr, port, 0, 0, GFP_KERNEL);
	if (id < 0)
		return id;

	port->id = id;
	port->dev.bus = &mikrobus_bus_type;
	port->dev.type = &mikrobus_port_type;

	strncpy(port->name, "mikrobus-port", sizeof(port->name) - 1);
	dev_set_name(&port->dev, "mikrobus-%d", port->id);

	retval = device_register(&port->dev);
	if (retval)
	{
		pr_err("port '%d': can't register device (%d)\n", port->id, retval);
		put_device(&port->dev);
		return retval;
	}

	retval = class_compat_create_link(mikrobus_port_compat_class, &port->dev, port->dev.parent);
	if (retval)
		dev_warn(&port->dev, "Failed to create compatibility class link\n");

	return retval;
}
EXPORT_SYMBOL_GPL(mikrobus_register_port);

void mikrobus_del_port(struct mikrobus_port *port)
{
	struct mikrobus_port *found;

	found = idr_find(&mikrobus_port_idr, port->id);
	if (found != port)
	{
		pr_debug("attempting to delete unregistered port [%s]\n", port->name);
		return;
	}

	class_compat_remove_link(mikrobus_port_compat_class, &port->dev, port->dev.parent);
	device_unregister(&port->dev);
	idr_remove(&mikrobus_port_idr, port->id);
	memset(&port->dev, 0, sizeof(port->dev));
}
EXPORT_SYMBOL_GPL(mikrobus_del_port);

static int __init mikrobus_init(void)
{
	int retval;

	retval = bus_register(&mikrobus_bus_type);
	if (retval)
	{
		pr_err("bus_register failed (%d)\n", retval);
		return retval;
	}

	mikrobus_port_compat_class = class_compat_register("mikrobus-port");

	if (!mikrobus_port_compat_class)
	{
		pr_err("class_compat register failed (%d)\n", retval);
		retval = -ENOMEM;
		goto class_err;
	}

	is_registered = true;
	return 0;

class_err:
	bus_unregister(&mikrobus_bus_type);
	idr_destroy(&mikrobus_port_idr);
	is_registered = false;
	return retval;
}
subsys_initcall(mikrobus_init);

static void __exit mikrobus_exit(void)
{
	bus_unregister(&mikrobus_bus_type);
	class_compat_unregister(mikrobus_port_compat_class);
	idr_destroy(&mikrobus_port_idr);
}
module_exit(mikrobus_exit);

MODULE_AUTHOR("Vaishnav M A <mavaishnav007@gmail.com>");
MODULE_DESCRIPTION("mikroBUS main module");
MODULE_LICENSE("GPL");