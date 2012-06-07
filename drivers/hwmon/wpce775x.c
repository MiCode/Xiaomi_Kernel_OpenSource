/* Quanta EC driver for the Winbond Embedded Controller
 *
 * Copyright (C) 2009 Quanta Computer Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/module.h>
#include <linux/err.h>
#include <linux/i2c.h>
#include <linux/slab.h>

#define EC_ID_NAME          "qci-i2cec"
#define EC_BUFFER_LEN		16
#define EC_CMD_POWER_OFF	0xAC
#define EC_CMD_RESTART	0xAB

static struct i2c_client *g_i2cec_client;

/* General structure to hold the driver data */
struct i2cec_drv_data {
		struct i2c_client *i2cec_client;
		struct work_struct work;
		char ec_data[EC_BUFFER_LEN+1];
};

static int __devinit wpce_probe(struct i2c_client *client,
	const struct i2c_device_id *id);
static int __devexit wpce_remove(struct i2c_client *kbd);

#ifdef CONFIG_PM
static int wpce_suspend(struct device *dev)
{
	return 0;
}

static int wpce_resume(struct device *dev)
{
	return 0;
}
#endif

#ifdef CONFIG_PM
static struct dev_pm_ops wpce_pm_ops = {
	.suspend  = wpce_suspend,
	.resume   = wpce_resume,
};
#endif

static const struct i2c_device_id wpce_idtable[] = {
       { EC_ID_NAME, 0 },
       { }
};

static struct i2c_driver wpce_driver = {
	.driver = {
		.owner = THIS_MODULE,
		.name  = EC_ID_NAME,
#ifdef CONFIG_PM
		.pm = &wpce_pm_ops,
#endif
	},
	.probe	  = wpce_probe,
	.remove	  = __devexit_p(wpce_remove),
	.id_table   = wpce_idtable,
};

static int __devinit wpce_probe(struct i2c_client *client,
				    const struct i2c_device_id *id)
{
	int err = -ENOMEM;
	struct i2cec_drv_data *context = 0;

	/* there is no need to call i2c_check_functionality() since it is the
	client's job to use the interface (I2C vs SMBUS) appropriate for it. */
	client->driver = &wpce_driver;
	context = kzalloc(sizeof(struct i2cec_drv_data), GFP_KERNEL);
	if (!context)
		return err;

	context->i2cec_client = client;
	g_i2cec_client = client;
	i2c_set_clientdata(context->i2cec_client, context);

	return 0;
}

static int __devexit wpce_remove(struct i2c_client *dev)
{
	struct i2cec_drv_data *context = i2c_get_clientdata(dev);
	g_i2cec_client = NULL;
	kfree(context);

	return 0;
}

static int __init wpce_init(void)
{
	return i2c_add_driver(&wpce_driver);
}

static void __exit wpce_exit(void)
{
	i2c_del_driver(&wpce_driver);
}

struct i2c_client *wpce_get_i2c_client(void)
{
	return g_i2cec_client;
}
EXPORT_SYMBOL_GPL(wpce_get_i2c_client);

void wpce_poweroff(void)
{
	if (g_i2cec_client == NULL)
		return;
	i2c_smbus_write_byte(g_i2cec_client, EC_CMD_POWER_OFF);
}
EXPORT_SYMBOL_GPL(wpce_poweroff);

void wpce_restart(void)
{
	if (g_i2cec_client == NULL)
		return;
	i2c_smbus_write_byte(g_i2cec_client, EC_CMD_RESTART);
}
EXPORT_SYMBOL_GPL(wpce_restart);

int wpce_i2c_transfer(struct i2c_msg *msg)
{
	if (g_i2cec_client == NULL)
		return -1;
	msg->addr = g_i2cec_client->addr;
	return i2c_transfer(g_i2cec_client->adapter, msg, 1);
}
EXPORT_SYMBOL_GPL(wpce_i2c_transfer);

int wpce_smbus_write_word_data(u8 command, u16 value)
{
	if (g_i2cec_client == NULL)
		return -1;
	return i2c_smbus_write_word_data(g_i2cec_client, command, value);
}
EXPORT_SYMBOL_GPL(wpce_smbus_write_word_data);

int wpce_smbus_write_byte_data(u8 command, u8 value)
{
	if (g_i2cec_client == NULL)
		return -1;
	return i2c_smbus_write_byte_data(g_i2cec_client, command, value);
}
EXPORT_SYMBOL_GPL(wpce_smbus_write_byte_data);

module_init(wpce_init);
module_exit(wpce_exit);

MODULE_AUTHOR("Quanta Computer Inc.");
MODULE_DESCRIPTION("Quanta Embedded Controller I2C Bridge Driver");
MODULE_LICENSE("GPL v2");
