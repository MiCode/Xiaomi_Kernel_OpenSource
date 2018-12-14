/*
 * File:   vituralsar_driver.h
 * Company: Fairchild Semiconductor
 *
 * Created on September 2, 2015, 9:02 AM
 */

#ifndef VITURALSAR_DRIVER_H
#define	VITURALSAR_DRIVER_H

#ifdef	__cplusplus
extern "C" {
#endif

#include <linux/spinlock.h>
#include <asm/switch_to.h>
#include <linux/gpio_keys.h>
#include <linux/proc_fs.h>
#include <linux/of_gpio.h>
#include <asm/gpio.h>
#include <linux/input.h>
/*******************************************************************************
 * Platform-specific configuration data
 ******************************************************************************/
#define VITURALSAR_I2C_DRIVER_NAME		"virtualsar"
#define VIRTUAL_I2C_DEVICETREE_NAME			"virtualsar,sar"


/*******************************************************************************
* Driver structs
******************************************************************************/
#ifdef CONFIG_OF
static const struct of_device_id virtualsar_dt_match[] = {
	{ .compatible = VIRTUAL_I2C_DEVICETREE_NAME },
	{},
};
MODULE_DEVICE_TABLE(of, virtualsar_dt_match);
#endif	/* CONFIG_OF */

/* This identifies our I2C driver in the kernel's driver module table */
static const struct i2c_device_id virtualsar_i2c_device_id[] = {
	{ VIRTUAL_I2C_DEVICETREE_NAME, 0 },
	{}
};
MODULE_DEVICE_TABLE(i2c, virtualsar_i2c_device_id);

/*******************************************************************************
 * Driver module functions
 ******************************************************************************/
static int __init virtualsar_init(void);
static void __exit virtualsar_exit(void);
static int virtualsar_probe(struct i2c_client *client,
		const struct i2c_device_id *id);

#define SAR_INT_TRIGGER    2
#define SAR_IRQ_TAB                     {IRQ_TYPE_EDGE_RISING, IRQ_TYPE_EDGE_FALLING,IRQ_TYPE_EDGE_BOTH, IRQ_TYPE_LEVEL_LOW, IRQ_TYPE_LEVEL_HIGH}

static const char *sar_name = "vitural-sar";
static const char *sar_input_phys = "input/sar";


struct vituralsar_data {
 	spinlock_t irq_lock;
	struct i2c_client *client;
	struct input_dev  *input_dev;
	u8  int_trigger_type;
	s32 irq_is_disable;
	s32 use_irq;
};

struct gpio_keys_button gpio_key = {
        .code              = KEY_F24,
        .type              = EV_KEY,
        .wakeup            = 1,
        .debounce_interval = 0,
        .can_disable       = true,
};

/* Defines our driver's name, device-tree match, and required driver callbacks */
static struct i2c_driver virtualsar_driver = {
	.driver = {
		.name = VIRTUAL_I2C_DEVICETREE_NAME,
		.of_match_table = of_match_ptr(virtualsar_dt_match),
	},
	.probe = virtualsar_probe,
	.id_table = virtualsar_i2c_device_id,
};

#endif	/* FUSB30X_DRIVER_H */

