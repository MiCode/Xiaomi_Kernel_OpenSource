/*
 * File:   fusb30x_driver.c
 * Company: Fairchild Semiconductor
 *
 * Created on January 8, 2018, 11:07 AM
 */

#ifndef	VIRTUALSAR_DRIVER_H
#define	VIRTUALSAR_DRIVER_H

#ifdef	__cplusplus
extern "C" {
#endif

#include <linux/spinlock.h>
#include <asm/switch_to.h>
#include <linux/gpio_keys.h>
#include <linux/proc_fs.h>
#include <linux/of_gpio.h>
#include <linux/gpio.h>
#include <linux/input.h>
/*******************************************************************************
 * Platform-specific configuration data
 ******************************************************************************/
#define VIRTUALSAR_I2C_DRIVER_NAME			"virtualsar"				// Length must be less than I2C_NAME_SIZE (currently 20, see include/linux/mod_devicetable.h)
#define VIRTUAL_I2C_DEVICETREE_NAME		"virtualsar,sar"	// Must match device tree .compatible string exactly
				// If no block reads/writes, try single reads/block writes

/*******************************************************************************
* Driver structs
******************************************************************************/
#ifdef CONFIG_OF		// Defined by the build system when configuring "open firmware" (OF) aka device-tree
static const struct of_device_id virtualsar_dt_match[] = {				// Used by kernel to match device-tree entry to driver
	{ .compatible = VIRTUAL_I2C_DEVICETREE_NAME },				// String must match device-tree node exactly
	{},
};
MODULE_DEVICE_TABLE(of, virtualsar_dt_match);
#endif	/* CONFIG_OF */

/* This identifies our I2C driver in the kernel's driver module table */
static const struct i2c_device_id virtualsar_i2c_device_id[] = {
	{ VIRTUAL_I2C_DEVICETREE_NAME, 0 },
	{}
};
MODULE_DEVICE_TABLE(i2c, virtualsar_i2c_device_id);		// Used to generate map files used by depmod for module dependencies

#define SAR_INT_TRIGGER    2
#define SAR_IRQ_TAB                     {IRQ_TYPE_EDGE_RISING, IRQ_TYPE_EDGE_FALLING, IRQ_TYPE_EDGE_BOTH, IRQ_TYPE_LEVEL_LOW, IRQ_TYPE_LEVEL_HIGH}

static const char *sar_name = "virtual-sar";
static const char *sar_input_phys = "input/sar";


struct virtualsar_data {
	spinlock_t irq_lock;
	struct i2c_client *client;
	struct input_dev  *input_dev;
	u8  int_trigger_type;
};

struct gpio_keys_button gpio_key = {
	.code              = KEY_F24,
	.type              = EV_KEY,
	.wakeup            = 1,   //change to 1 for wake up
	.debounce_interval = 0,
	.can_disable       = true,
};

static unsigned int irq;
/*******************************************************************************
 * Driver module functions
 ******************************************************************************/
static int __init virtualsar_init(void);		// Called when driver is inserted into the kernel
static void __exit virtualsar_exit(void);		// Called when driver is removed from the kernel
static int virtualsar_probe(struct i2c_client *client,			// Called when the associated device is added
		const struct i2c_device_id *id);
static int virtualsar_suspend(struct device *dev);
static int virtualsar_resume(struct device *dev);

static SIMPLE_DEV_PM_OPS(virtualsar_pm_ops, virtualsar_suspend, virtualsar_resume);
/* Defines our driver's name, device-tree match, and required driver callbacks */
static struct i2c_driver virtualsar_driver = {
	.driver = {
		.name = VIRTUAL_I2C_DEVICETREE_NAME,		// Must match our id_table name
		.pm = &virtualsar_pm_ops,
		.of_match_table = of_match_ptr(virtualsar_dt_match),			// Device-tree match structure to pair the DT device with our driver
	},
	.probe = virtualsar_probe,			// Called on device add, inits/starts driver
	.id_table = virtualsar_i2c_device_id,		// I2C id structure to associate with our driver
};

#endif	/* FUSB30X_DRIVER_H */
