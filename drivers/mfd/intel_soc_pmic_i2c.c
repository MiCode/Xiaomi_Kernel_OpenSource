/*
 * intel_soc_pmic_i2c.c - Intel SoC PMIC MFD Driver
 *
 * Copyright (C) 2013, 2014 Intel Corporation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License version
 * 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * Author: Yang, Bin <bin.yang@intel.com>
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/mfd/core.h>
#include <linux/err.h>
#include <linux/i2c.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/workqueue.h>
#include <linux/acpi.h>
#include <linux/version.h>
#include <linux/gpio/consumer.h>
#include <linux/mfd/intel_soc_pmic.h>
#include "intel_soc_pmic_core.h"

static struct i2c_client *pmic_i2c_client;
static struct intel_soc_pmic *pmic_i2c;

#define I2C_ADDR_MASK		0xFF00
#define I2C_ADDR_SHIFT		8
#define I2C_REG_MASK		0xFF

static int pmic_i2c_readb(int reg)
{
	if (reg & I2C_ADDR_MASK)
		pmic_i2c_client->addr = (reg & I2C_ADDR_MASK)
						>> I2C_ADDR_SHIFT;
	else
		pmic_i2c_client->addr = pmic_i2c->default_client;

	reg &= I2C_REG_MASK;
	return i2c_smbus_read_byte_data(pmic_i2c_client, reg);
}

static int pmic_i2c_writeb(int reg, u8 val)
{
	if (reg & I2C_ADDR_MASK)
		pmic_i2c_client->addr = (reg & I2C_ADDR_MASK)
						>> I2C_ADDR_SHIFT;
	else
		pmic_i2c_client->addr = pmic_i2c->default_client;

	reg &= I2C_REG_MASK;
	return i2c_smbus_write_byte_data(pmic_i2c_client, reg, val);
}

static void pmic_shutdown(struct i2c_client *client)
{
	disable_irq(pmic_i2c_client->irq);
	return;
}

static int pmic_suspend(struct device *dev)
{
	disable_irq(pmic_i2c_client->irq);
	return 0;
}

static int pmic_resume(struct device *dev)
{
	enable_irq(pmic_i2c_client->irq);
	return 0;
}

static const struct dev_pm_ops pmic_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(pmic_suspend, pmic_resume)
};

static int pmic_i2c_lookup_gpio(struct device *dev, int acpi_index)
{
	struct gpio_desc *desc;
	int gpio;

	desc = gpiod_get_index(dev, KBUILD_MODNAME, acpi_index);
	if (IS_ERR(desc))
		return PTR_ERR(desc);

	gpio = desc_to_gpio(desc);

	gpiod_put(desc);

	return gpio;
}

static int pmic_i2c_probe(struct i2c_client *i2c,
			  const struct i2c_device_id *id)
{
	if (pmic_i2c_client != NULL || pmic_i2c != NULL)
		return -EBUSY;

	if (!id)
		return -ENODEV;

	pmic_i2c	= (struct intel_soc_pmic *)id->driver_data;
	pmic_i2c_client	= i2c;
	pmic_i2c->dev	= &i2c->dev;
	pmic_i2c->irq	= i2c->irq;
	pmic_i2c->default_client = i2c->addr;
	pmic_i2c->pmic_int_gpio = pmic_i2c_lookup_gpio(pmic_i2c->dev, 0);
	pmic_i2c->readb	= pmic_i2c_readb;
	pmic_i2c->writeb = pmic_i2c_writeb;

	return intel_pmic_add(pmic_i2c);
}

static int pmic_i2c_remove(struct i2c_client *i2c)
{
	int ret = intel_pmic_remove(pmic_i2c);

	pmic_i2c_client = NULL;
	pmic_i2c = NULL;

	return ret;
}

static const struct i2c_device_id pmic_i2c_id[] = {
	{ "crystal_cove", (kernel_ulong_t)&crystal_cove_pmic},
	{ "dollar_cove", (kernel_ulong_t)&dollar_cove_pmic},
	{ "INT33F4", (kernel_ulong_t)&dollar_cove_pmic},
	{ "INT33F4:00", (kernel_ulong_t)&dollar_cove_pmic},
	{ "INT33F5", (kernel_ulong_t)&dollar_cove_ti_pmic},
	{ "INT33F5:00", (kernel_ulong_t)&dollar_cove_ti_pmic},
	{ "INT33FD", (kernel_ulong_t)&crystal_cove_pmic},
	{ "INT33FD:00", (kernel_ulong_t)&crystal_cove_pmic},
	{ "whiskey_cove", (kernel_ulong_t)&whiskey_cove_pmic},
	{ "INT33FE", (kernel_ulong_t)&whiskey_cove_pmic},
	{ "INT33FE:00", (kernel_ulong_t)&whiskey_cove_pmic},
	{ "INT33FE:00:6e", (kernel_ulong_t)&whiskey_cove_pmic},
	{ }
};
MODULE_DEVICE_TABLE(i2c, pmic_i2c_id);

static struct acpi_device_id pmic_acpi_match[] = {
	{ "INT33FD", (kernel_ulong_t)&crystal_cove_pmic},
	{ "INT33FE", (kernel_ulong_t)&whiskey_cove_pmic},
	{ },
};
MODULE_DEVICE_TABLE(acpi, pmic_acpi_match);

static struct i2c_driver pmic_i2c_driver = {
	.driver = {
		.name = "intel_soc_pmic_i2c",
		.owner = THIS_MODULE,
		.pm = &pmic_pm_ops,
		.acpi_match_table = ACPI_PTR(pmic_acpi_match),
	},
	.probe = pmic_i2c_probe,
	.remove = pmic_i2c_remove,
	.id_table = pmic_i2c_id,
	.shutdown = pmic_shutdown,
};

static int __init pmic_i2c_init(void)
{
	int ret;

	ret = i2c_add_driver(&pmic_i2c_driver);
	if (ret != 0)
		pr_err("Failed to register pmic I2C driver: %d\n", ret);

	return ret;
}
subsys_initcall(pmic_i2c_init);

static void __exit pmic_i2c_exit(void)
{
	i2c_del_driver(&pmic_i2c_driver);
}
module_exit(pmic_i2c_exit);

MODULE_DESCRIPTION("I2C driver for Intel SoC PMIC");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Yang, Bin <bin.yang@intel.com>");
