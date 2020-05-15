#define pr_fmt(fmt) "mikrobus_port: " fmt

#include <linux/err.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/jump_label.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/device.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/i2c.h>
#include <linux/gpio.h>
#include <linux/spi/spi.h>
#include <linux/serdev.h>
#include <linux/slab.h>

#include "mikrobus_core.h"

static unsigned short i2c_adap_nr;
module_param(i2c_adap_nr, ushort, 0000);
MODULE_PARM_DESC(i2c_adap_nr, "mikroBUS I2C Adapter Number");

static unsigned short spi_master_nr;
module_param(spi_master_nr, ushort, 0000);
MODULE_PARM_DESC(spi_master_nr, "mikroBUS SPI Master Number");

static unsigned short serdev_ctlr_nr;
module_param(serdev_ctlr_nr, ushort, 0000);
MODULE_PARM_DESC(serdev_ctlr_nr, "mikroBUS Serdev Controller Number");

static unsigned short rst_gpio_nr;
module_param(rst_gpio_nr, ushort, 0000);
MODULE_PARM_DESC(rst_gpio_nr, "mikroBUS RST GPIO Number");

static unsigned short pwm_gpio_nr;
module_param(pwm_gpio_nr, ushort, 0000);
MODULE_PARM_DESC(pwm_gpio_nr, "mikroBUS PWM GPIO Number");

static unsigned short int_gpio_nr;
module_param(int_gpio_nr, ushort, 0000);
MODULE_PARM_DESC(int_gpio_nr, "mikroBUS INT GPIO Number");

static char *drvname;
module_param(drvname, charp, 0000);
MODULE_PARM_DESC(drvname, "Driver name (required).");

static unsigned short protocol;
module_param(protocol, ushort, 0000);
MODULE_PARM_DESC(protocol, "Device protocol");

static unsigned short reg;
module_param(reg, ushort, 0000);
MODULE_PARM_DESC(reg, "Device Address or Chip Select");

static unsigned short irq;
module_param(irq, ushort, 0000);
MODULE_PARM_DESC(irq, "Device IRQ");

static unsigned short irq_type;
module_param(irq_type, ushort, 0000);
MODULE_PARM_DESC(irq_type, "Device IRQ Type");

static unsigned long max_speed_hz;
module_param(max_speed_hz, ulong, 0000);
MODULE_PARM_DESC(max_speed_hz, "Device Max Speed");

struct mikrobus_port *port;

struct click_device_info *test_dev;

static const struct of_device_id mikrobus_port_of_match[] = {
    {.compatible = "linux,mikrobus"},
    {},
};
MODULE_DEVICE_TABLE(of, mikrobus_port_of_match);

static int mikrobus_port_probe(struct platform_device *pdev)
{
    struct device_node *i2c_adap_np, *spi_master_np;
    const struct of_device_id *match;
    int gpio;
    int retval;

    port = kzalloc(sizeof(*port), GFP_KERNEL);
    if (!port)
    {
        return -ENOMEM;
    }

    match = of_match_device(of_match_ptr(mikrobus_port_of_match), &pdev->dev);
    if (match)
    {

        i2c_adap_np = of_parse_phandle(pdev->dev.of_node, "i2c-adapter", 0);
        if (!i2c_adap_np)
        {
            dev_err(&pdev->dev, "Cannot parse i2c-adapter\n");
            return -ENODEV;
        }
        port->i2c_adap = of_find_i2c_adapter_by_node(i2c_adap_np);
        of_node_put(i2c_adap_np);

        // spi_master_np = of_parse_phandle(pdev->dev.of_node, "spi-master", 0);
        // if (!spi_master_np) {
        //     dev_err(&pdev->dev, "Cannot parse spi-master\n");
        //     return -ENODEV;
        // }
        // port->spi_mstr = of_find_spi_controller_by_node(spi_master_np);
        // of_node_put(spi_master_np);

        gpio = of_get_named_gpio(pdev->dev.of_node, "pwm-gpio", 0);
        if (!gpio_is_valid(gpio))
            return -ENODEV;
        port->pwm_gpio = gpio_to_desc(gpio);

        gpio = of_get_named_gpio(pdev->dev.of_node, "int-gpio", 0);
        if (!gpio_is_valid(gpio))
            return -ENODEV;
        port->int_gpio = gpio_to_desc(gpio);

        gpio = of_get_named_gpio(pdev->dev.of_node, "rst-gpio", 0);
        if (!gpio_is_valid(gpio))
            return -ENODEV;
        port->rst_gpio = gpio_to_desc(gpio);
    }
    else
    {

        port->spi_mstr = spi_busnum_to_master(spi_master_nr);
        port->i2c_adap = i2c_get_adapter(i2c_adap_nr);
        //port->serdev_ctlr_nr = (serdev_ctlr_nr);
        port->pwm_gpio = gpio_to_desc(pwm_gpio_nr);
        port->int_gpio = gpio_to_desc(int_gpio_nr);
        port->rst_gpio = gpio_to_desc(int_gpio_nr);
    }

    retval = mikrobus_register_port(port);
    if (retval)
    {
        pr_err("port : can't register port (%d)\n", retval);
        return retval;
    }

    //probe eeprom
    //fetch data from eeprom
    //mikrobus_parse_manifest
    //mikrobus_setup_port
    //mikrobus_register_devices

    //temporary dummy testing
    test_dev = kzalloc(sizeof(*test_dev), GFP_KERNEL);
    if (!test_dev)
    {
        return -ENOMEM;
    }

    test_dev->drv_name = kmemdup(drvname, 40, GFP_KERNEL);
    test_dev->protocol = protocol;
    test_dev->reg = reg;
    test_dev->irq = irq;
    test_dev->irq_type = irq_type;
    test_dev->max_speed_hz = max_speed_hz;
    mikrobus_register_device(port, test_dev);

    return retval;
}

static int mikrobus_port_remove(struct platform_device *pdev)
{
    mikrobus_unregister_device(port, test_dev);
    mikrobus_del_port(port);
    return 0;
}

static struct platform_driver mikrobus_port_driver = {
    .probe = mikrobus_port_probe,
    .remove = mikrobus_port_remove,
    .driver = {
        .name = "mikrobus",
        .of_match_table = of_match_ptr(mikrobus_port_of_match),
    },
};

static struct platform_device *pdev;

static int __init
mikrobus_port_init_driver(void)
{
    int retval;

    retval = platform_driver_register(&mikrobus_port_driver);
    if (retval)
    {
        pr_err("driver register failed (%d)\n", retval);
        return retval;
    }
#ifdef MODULE
    pdev = platform_device_alloc("mikrobus", 0);
    if (!pdev)
    {
        pr_err("Device allocation failed\n");
        return -ENOMEM;
    }
    retval = platform_device_add(pdev);
    if (retval)
    {
        pr_err("Device addition failed (%d)\n", retval);
        return retval;
    }
#endif

    return 0;
}
subsys_initcall(mikrobus_port_init_driver);

static void __exit mikrobus_port_exit_driver(void)
{
    platform_device_unregister(pdev);
    platform_driver_unregister(&mikrobus_port_driver);
}
module_exit(mikrobus_port_exit_driver);

MODULE_AUTHOR("Vaishnav M A <mavaishnav007@gmail.com>");
MODULE_DESCRIPTION("mikroBUS port module");
MODULE_LICENSE("GPL");