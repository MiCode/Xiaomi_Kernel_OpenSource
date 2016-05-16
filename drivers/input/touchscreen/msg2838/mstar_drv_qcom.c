/**
 *
 * @file	mstar_drv_qcom.c
 *
 * @brief   This file defines the interface of touch screen
 *
 *
 */

/*=============================================================*/

/*=============================================================*/

#include <linux/module.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/types.h>
#include <linux/input.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/kobject.h>
#include <asm/irq.h>
#include <asm/io.h>


#include "mstar_drv_platform_interface.h"

#ifdef CONFIG_ENABLE_REGULATOR_POWER_ON
#include <linux/regulator/consumer.h>
#endif

/*=============================================================*/

/*=============================================================*/

#define MSG_TP_IC_NAME "msg2xxx"

/*=============================================================*/

/*=============================================================*/

struct i2c_client *g_I2cClient = NULL;

#ifdef CONFIG_ENABLE_REGULATOR_POWER_ON
struct regulator *g_ReguVdd = NULL;
#endif

/*=============================================================*/

/*=============================================================*/

/* probe function is used for matching and initializing input device */
static int  touch_driver_probe(struct i2c_client *client,
		const struct i2c_device_id *id)
{

#ifdef CONFIG_ENABLE_REGULATOR_POWER_ON
	const char *vdd_name = "vdd";
#endif

	DBG("touch_driver_probe\n");
	DBG("*** %s ***\n", __FUNCTION__);

	if (client == NULL) {
		DBG("i2c client is NULL\n");
		return -EPERM;
	}
	g_I2cClient = client;

#ifdef CONFIG_ENABLE_REGULATOR_POWER_ON
	g_ReguVdd = regulator_get(&g_I2cClient->dev, vdd_name);
#endif

	return MsDrvInterfaceTouchDeviceProbe(g_I2cClient, id);
}

/* remove function is triggered when the input device is removed from input sub-system */
static int touch_driver_remove(struct i2c_client *client)
{
	DBG("*** %s ***\n", __FUNCTION__);

	return MsDrvInterfaceTouchDeviceRemove(client);
}

/* The I2C device list is used for matching I2C device and I2C device driver. */
static const struct i2c_device_id touch_device_id[] = {
	{MSG_TP_IC_NAME, 0},
	{}, /* should not omitted */
};

MODULE_DEVICE_TABLE(i2c, touch_device_id);

static struct of_device_id touch_match_table[] = {
	{.compatible = "mstar, msg2xxx",},
	{},
};

static struct i2c_driver touch_device_driver = {
	.driver = {
		.name = MSG_TP_IC_NAME,
		.owner = THIS_MODULE,
		.of_match_table = touch_match_table,
	},
	.probe = touch_driver_probe,
	.remove = touch_driver_remove,
	.id_table = touch_device_id,
};

static int __init touch_driver_init(void)
{
	int ret;

	/* register driver */
	DBG("add touch device driver i2c driver: touch_driver_init 1.\n");
	ret = i2c_add_driver(&touch_device_driver);
	if (ret < 0) {
		DBG("add touch device driver i2c driver failed.\n");
		return -ENODEV;
	}
	DBG("add touch device driver i2c driver: touch_driver_init 2 .\n");

	return ret;
}

static void __exit touch_driver_exit(void)
{
	DBG("remove touch device driver i2c driver.\n");

	i2c_del_driver(&touch_device_driver);
}

module_init(touch_driver_init);
module_exit(touch_driver_exit);
MODULE_LICENSE("GPL");
