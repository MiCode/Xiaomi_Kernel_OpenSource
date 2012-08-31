/* Copyright (c) 2012, Code Aurora Forum. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#define pr_fmt(fmt) "%s: " fmt, __func__

#include <linux/module.h>
#include <linux/err.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/stddef.h>
#include <linux/debugfs.h>
#include <linux/mfd/pm8xxx/core.h>
#include <linux/mfd/pm8xxx/spk.h>

#define PM8XXX_SPK_CTL1_REG_OFF		0
#define PM8XXX_SPK_TEST_REG_1_OFF	1
#define PM8XXX_SPK_TEST_REG_2_OFF	2

#define PM8XXX_SPK_BANK_SEL		4
#define PM8XXX_SPK_BANK_WRITE		0x80
#define PM8XXX_SPK_BANK_VAL_MASK	0xF

#define BOOST_6DB_GAIN_EN_MASK		0x8
#define VSEL_LD0_1P1			0x0
#define VSEL_LD0_1P2			0x2
#define VSEL_LD0_1P0			0x4

#define PWM_EN_MASK			0xF
#define PM8XXX_SPK_TEST_REG_1_BANKS	8
#define PM8XXX_SPK_TEST_REG_2_BANKS	2

#define PM8XXX_SPK_GAIN			0x5
#define PM8XXX_ADD_EN			0x1

struct pm8xxx_spk_chip {
	struct list_head                        link;
	struct pm8xxx_spk_platform_data		pdata;
	struct device                           *dev;
	enum pm8xxx_version                     version;
	struct mutex				spk_mutex;
	u16					base;
	u16					end;
};

static struct pm8xxx_spk_chip *the_spk_chip;

static inline bool spk_defined(void)
{
	if (the_spk_chip == NULL || IS_ERR(the_spk_chip))
		return false;
	return true;
}

static int pm8xxx_spk_bank_write(u16 reg, u16 bank, u8 val)
{
	int rc = 0;
	u8 bank_val = PM8XXX_SPK_BANK_WRITE | (bank << PM8XXX_SPK_BANK_SEL);

	bank_val |= (val & PM8XXX_SPK_BANK_VAL_MASK);
	mutex_lock(&the_spk_chip->spk_mutex);
	rc = pm8xxx_writeb(the_spk_chip->dev->parent, reg, bank_val);
	if (rc)
		pr_err("pm8xxx_writeb(): rc=%d\n", rc);
	mutex_unlock(&the_spk_chip->spk_mutex);
	return rc;
}


static int pm8xxx_spk_read(u16 addr)
{
	int rc = 0;
	u8 val = 0;

	mutex_lock(&the_spk_chip->spk_mutex);
	rc = pm8xxx_readb(the_spk_chip->dev->parent,
			the_spk_chip->base + addr, &val);
	if (rc) {
		pr_err("pm8xxx_spk_readb() failed: rc=%d\n", rc);
		val = rc;
	}
	mutex_unlock(&the_spk_chip->spk_mutex);

	return val;
}

static int pm8xxx_spk_write(u16 addr, u8 val)
{
	int rc = 0;

	mutex_lock(&the_spk_chip->spk_mutex);
	rc = pm8xxx_writeb(the_spk_chip->dev->parent,
			the_spk_chip->base + addr, val);
	if (rc)
		pr_err("pm8xxx_writeb() failed: rc=%d\n", rc);
	mutex_unlock(&the_spk_chip->spk_mutex);
	return rc;
}

int pm8xxx_spk_mute(bool mute)
{
	u8 val = 0;
	int ret = 0;
	if (spk_defined() == false) {
		pr_err("Invalid spk handle or no spk_chip\n");
		return -ENODEV;
	}

	val = pm8xxx_spk_read(PM8XXX_SPK_CTL1_REG_OFF);
	if (val < 0)
		return val;
	val |= mute << 2;
	ret = pm8xxx_spk_write(PM8XXX_SPK_CTL1_REG_OFF, val);
	return ret;
}
EXPORT_SYMBOL_GPL(pm8xxx_spk_mute);

int pm8xxx_spk_gain(u8 gain)
{
	u8 val;
	int ret = 0;

	if (spk_defined() == false) {
		pr_err("Invalid spk handle or no spk_chip\n");
		return -ENODEV;
	}

	val = pm8xxx_spk_read(PM8XXX_SPK_CTL1_REG_OFF);
	if (val < 0)
		return val;
	val = (gain << 4) | (val & 0xF);
	ret = pm8xxx_spk_write(PM8XXX_SPK_CTL1_REG_OFF, val);
	if (!ret) {
		pm8xxx_spk_bank_write(the_spk_chip->base
			+ PM8XXX_SPK_TEST_REG_1_OFF,
			0, BOOST_6DB_GAIN_EN_MASK | VSEL_LD0_1P2);
	}
	return ret;
}
EXPORT_SYMBOL_GPL(pm8xxx_spk_gain);

int pm8xxx_spk_enable(int enable)
{
	int val = 0;
	u16 addr;
	int ret = 0;

	if (spk_defined() == false) {
		pr_err("Invalid spk handle or no spk_chip\n");
		return -ENODEV;
	}

	addr = the_spk_chip->base + PM8XXX_SPK_TEST_REG_1_OFF;
	val = pm8xxx_spk_read(PM8XXX_SPK_CTL1_REG_OFF);
	if (val < 0)
		return val;
	if (enable)
		val |= (1 << 3);
	else
		val &= ~(1 << 3);
	ret = pm8xxx_spk_write(PM8XXX_SPK_CTL1_REG_OFF, val);
	if (!ret)
		ret = pm8xxx_spk_bank_write(addr, 6, PWM_EN_MASK);
	return ret;
}
EXPORT_SYMBOL_GPL(pm8xxx_spk_enable);

static int pm8xxx_spk_config(void)
{
	u16 addr;
	int ret = 0;

	if (spk_defined() == false) {
		pr_err("Invalid spk handle or no spk_chip\n");
		return -ENODEV;
	}

	addr = the_spk_chip->base + PM8XXX_SPK_TEST_REG_1_OFF;
	ret = pm8xxx_spk_bank_write(addr, 6, PWM_EN_MASK & 0);
	if (!ret)
		ret = pm8xxx_spk_gain(PM8XXX_SPK_GAIN);
	return ret;
}

static int __devinit pm8xxx_spk_probe(struct platform_device *pdev)
{
	const struct pm8xxx_spk_platform_data *pdata = pdev->dev.platform_data;
	int ret = 0;

	if (!pdata) {
		pr_err("missing platform data\n");
		return -EINVAL;
	}

	the_spk_chip = kzalloc(sizeof(struct pm8xxx_spk_chip), GFP_KERNEL);
	if (the_spk_chip == NULL) {
		pr_err("kzalloc() failed.\n");
		return -ENOMEM;
	}

	mutex_init(&the_spk_chip->spk_mutex);

	the_spk_chip->dev = &pdev->dev;
	the_spk_chip->version = pm8xxx_get_version(the_spk_chip->dev->parent);
	switch (pm8xxx_get_version(the_spk_chip->dev->parent)) {
	case PM8XXX_VERSION_8038:
		break;
	default:
		ret = -ENODEV;
		goto err_handle;
	}

	memcpy(&(the_spk_chip->pdata), pdata,
			sizeof(struct pm8xxx_spk_platform_data));

	the_spk_chip->base = pdev->resource[0].start;
	the_spk_chip->end = pdev->resource[0].end;

	if (the_spk_chip->pdata.spk_add_enable) {
		int val;
		val = pm8xxx_spk_read(PM8XXX_SPK_CTL1_REG_OFF);
		if (val < 0) {
			ret = val;
			goto err_handle;
		}
		val |= (the_spk_chip->pdata.spk_add_enable & PM8XXX_ADD_EN);
		ret = pm8xxx_spk_write(PM8XXX_SPK_CTL1_REG_OFF, val);
		if (ret < 0)
			goto err_handle;
	}
	return pm8xxx_spk_config();
err_handle:
	pr_err("pm8xxx_spk_probe failed."
			"Audio unavailable on speaker.\n");
	mutex_destroy(&the_spk_chip->spk_mutex);
	kfree(the_spk_chip);
	return ret;
}

static int __devexit pm8xxx_spk_remove(struct platform_device *pdev)
{
	if (spk_defined() == false) {
		pr_err("Invalid spk handle or no spk_chip\n");
		return -ENODEV;
	}
	mutex_destroy(&the_spk_chip->spk_mutex);
	kfree(the_spk_chip);
	return 0;
}

static struct platform_driver pm8xxx_spk_driver = {
	.probe		= pm8xxx_spk_probe,
	.remove		= __devexit_p(pm8xxx_spk_remove),
	.driver		= {
		.name = PM8XXX_SPK_DEV_NAME,
		.owner = THIS_MODULE,
	},
};

static int __init pm8xxx_spk_init(void)
{
	return platform_driver_register(&pm8xxx_spk_driver);
}
subsys_initcall(pm8xxx_spk_init);

static void __exit pm8xxx_spk_exit(void)
{
	platform_driver_unregister(&pm8xxx_spk_driver);
}
module_exit(pm8xxx_spk_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("PM8XXX SPK driver");
MODULE_ALIAS("platform:" PM8XXX_SPK_DEV_NAME);
