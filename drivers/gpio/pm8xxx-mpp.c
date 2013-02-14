/*
 * Qualcomm PM8XXX Multi-Purpose Pin (MPP) driver
 *
 * Copyright (c) 2011-2012, The Linux Foundation. All rights reserved.
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
#include <linux/platform_device.h>
#include <linux/gpio.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/mfd/pm8xxx/core.h>
#include <linux/mfd/pm8xxx/mpp.h>

/* MPP Type */
#define	PM8XXX_MPP_TYPE_MASK		0xE0
#define	PM8XXX_MPP_TYPE_SHIFT		5

/* MPP Config Level */
#define	PM8XXX_MPP_CONFIG_LVL_MASK	0x1C
#define	PM8XXX_MPP_CONFIG_LVL_SHIFT	2

/* MPP Config Control */
#define	PM8XXX_MPP_CONFIG_CTRL_MASK	0x03
#define	PM8XXX_MPP_CONFIG_CTRL_SHIFT	0

struct pm8xxx_mpp_chip {
	struct list_head	link;
	struct gpio_chip	gpio_chip;
	spinlock_t		pm_lock;
	u8			*ctrl_reg;
	int			mpp_base;
	int			irq_base;
	int			nmpps;
	u16			base_addr;
};

static LIST_HEAD(pm8xxx_mpp_chips);
static DEFINE_MUTEX(pm8xxx_mpp_chips_lock);

static int pm8xxx_mpp_write(struct pm8xxx_mpp_chip *mpp_chip, u16 offset,
				u8 val, u8 mask)
{
	u8 reg;
	int rc;
	unsigned long flags;

	spin_lock_irqsave(&mpp_chip->pm_lock, flags);

	reg = (mpp_chip->ctrl_reg[offset] & ~mask) | (val & mask);
	rc = pm8xxx_writeb(mpp_chip->gpio_chip.dev->parent,
				mpp_chip->base_addr + offset, reg);
	if (!rc)
		mpp_chip->ctrl_reg[offset] = reg;

	spin_unlock_irqrestore(&mpp_chip->pm_lock, flags);

	return rc;
}

static int pm8xxx_mpp_to_irq(struct gpio_chip *chip, unsigned offset)
{
	struct pm8xxx_mpp_chip *mpp_chip = dev_get_drvdata(chip->dev);

	return mpp_chip->irq_base + offset;
}

static int pm8xxx_mpp_get(struct gpio_chip *chip, unsigned offset)
{
	struct pm8xxx_mpp_chip *mpp_chip = dev_get_drvdata(chip->dev);
	int rc;

	if ((mpp_chip->ctrl_reg[offset] & PM8XXX_MPP_TYPE_MASK) >>
			PM8XXX_MPP_TYPE_SHIFT == PM8XXX_MPP_TYPE_D_OUTPUT)
		rc = mpp_chip->ctrl_reg[offset] & PM8XXX_MPP_CONFIG_CTRL_MASK;
	else
		rc = pm8xxx_read_irq_stat(mpp_chip->gpio_chip.dev->parent,
				mpp_chip->irq_base + offset);

	return rc;
}

static void pm8xxx_mpp_set(struct gpio_chip *chip, unsigned offset, int val)
{
	struct pm8xxx_mpp_chip *mpp_chip = dev_get_drvdata(chip->dev);
	u8 reg = val ? PM8XXX_MPP_DOUT_CTRL_HIGH : PM8XXX_MPP_DOUT_CTRL_LOW;
	int rc;

	rc = pm8xxx_mpp_write(mpp_chip, offset, reg,
			PM8XXX_MPP_CONFIG_CTRL_MASK);
	if (rc)
		pr_err("pm8xxx_mpp_write(): rc=%d\n", rc);
}

static int pm8xxx_mpp_dir_input(struct gpio_chip *chip, unsigned offset)
{
	struct pm8xxx_mpp_chip *mpp_chip = dev_get_drvdata(chip->dev);
	int rc = pm8xxx_mpp_write(mpp_chip, offset,
			PM8XXX_MPP_TYPE_D_INPUT << PM8XXX_MPP_TYPE_SHIFT,
			PM8XXX_MPP_TYPE_MASK);

	if (rc)
		pr_err("pm8xxx_mpp_write(): rc=%d\n", rc);
	return rc;
}

static int pm8xxx_mpp_dir_output(struct gpio_chip *chip,
		unsigned offset, int val)
{
	struct pm8xxx_mpp_chip *mpp_chip = dev_get_drvdata(chip->dev);
	u8 reg = (PM8XXX_MPP_TYPE_D_OUTPUT << PM8XXX_MPP_TYPE_SHIFT) |
		(val & PM8XXX_MPP_CONFIG_CTRL_MASK);
	u8 mask = PM8XXX_MPP_TYPE_MASK | PM8XXX_MPP_CONFIG_CTRL_MASK;
	int rc = pm8xxx_mpp_write(mpp_chip, offset, reg, mask);

	if (rc)
		pr_err("pm8xxx_mpp_write(): rc=%d\n", rc);
	return rc;
}

static void pm8xxx_mpp_dbg_show(struct seq_file *s, struct gpio_chip *chip)
{
	static const char * const ctype[] = {	"d_in", "d_out", "bi_dir",
						"a_in", "a_out", "sink",
						"dtest_sink", "dtest_out"
	};
	struct pm8xxx_mpp_chip *mpp_chip = dev_get_drvdata(chip->dev);
	u8 type, state;
	const char *label;
	int i;

	for (i = 0; i < mpp_chip->nmpps; i++) {
		label = gpiochip_is_requested(chip, i);
		type = (mpp_chip->ctrl_reg[i] & PM8XXX_MPP_TYPE_MASK) >>
			PM8XXX_MPP_TYPE_SHIFT;
		state = pm8xxx_mpp_get(chip, i);
		seq_printf(s, "gpio-%-3d (%-12.12s) %-10.10s"
				" %s 0x%02x\n",
				chip->base + i,
				label ? label : "--",
				ctype[type],
				state ? "hi" : "lo",
				mpp_chip->ctrl_reg[i]);
	}
}

int pm8xxx_mpp_config(unsigned mpp, struct pm8xxx_mpp_config_data *config)
{
	struct pm8xxx_mpp_chip *mpp_chip;
	int rc, found = 0;
	u8 config_reg, mask;

	if (!config) {
		pr_err("config not specified for MPP %d\n", mpp);
		return -EINVAL;
	}

	mutex_lock(&pm8xxx_mpp_chips_lock);
	list_for_each_entry(mpp_chip, &pm8xxx_mpp_chips, link) {
		if (mpp >= mpp_chip->mpp_base
		    && mpp < mpp_chip->mpp_base + mpp_chip->nmpps) {
			found = 1;
			break;
		}
	}
	mutex_unlock(&pm8xxx_mpp_chips_lock);
	if (!found) {
		pr_err("called on mpp %d not handled by any pmic\n", mpp);
		return -EINVAL;
	}

	mask = PM8XXX_MPP_TYPE_MASK | PM8XXX_MPP_CONFIG_LVL_MASK |
		PM8XXX_MPP_CONFIG_CTRL_MASK;
	config_reg = (config->type << PM8XXX_MPP_TYPE_SHIFT)
			& PM8XXX_MPP_TYPE_MASK;
	config_reg |= (config->level << PM8XXX_MPP_CONFIG_LVL_SHIFT)
			& PM8XXX_MPP_CONFIG_LVL_MASK;
	config_reg |= config->control & PM8XXX_MPP_CONFIG_CTRL_MASK;

	rc = pm8xxx_mpp_write(mpp_chip, mpp - mpp_chip->mpp_base, config_reg,
			      mask);

	if (rc)
		pr_err("pm8xxx_mpp_write(): rc=%d\n", rc);

	return rc;
}
EXPORT_SYMBOL_GPL(pm8xxx_mpp_config);

static int __devinit pm8xxx_mpp_reg_init(struct pm8xxx_mpp_chip *mpp_chip)
{
	int rc, i;

	for (i = 0; i < mpp_chip->nmpps; i++) {
		rc = pm8xxx_readb(mpp_chip->gpio_chip.dev->parent,
					mpp_chip->base_addr + i,
					&mpp_chip->ctrl_reg[i]);
		if (rc) {
			pr_err("failed to read register 0x%x rc=%d\n",
						mpp_chip->base_addr + i, rc);
			return rc;
		}
	}
	return 0;
}

static int __devinit pm8xxx_mpp_probe(struct platform_device *pdev)
{
	int rc;
	const struct pm8xxx_mpp_platform_data *pdata = pdev->dev.platform_data;
	struct pm8xxx_mpp_chip *mpp_chip;

	if (!pdata) {
		pr_err("missing platform data\n");
		return -EINVAL;
	}

	mpp_chip = kzalloc(sizeof(struct pm8xxx_mpp_chip), GFP_KERNEL);
	if (!mpp_chip) {
		pr_err("Cannot allocate %d bytes\n",
			sizeof(struct pm8xxx_mpp_chip));
		return -ENOMEM;
	}

	mpp_chip->ctrl_reg = kzalloc(pdata->core_data.nmpps, GFP_KERNEL);
	if (!mpp_chip->ctrl_reg) {
		pr_err("Cannot allocate %d bytes\n", pdata->core_data.nmpps);
		rc = -ENOMEM;
		goto free_mpp_chip;
	}

	spin_lock_init(&mpp_chip->pm_lock);

	mpp_chip->gpio_chip.label = PM8XXX_MPP_DEV_NAME;
	mpp_chip->gpio_chip.direction_input = pm8xxx_mpp_dir_input;
	mpp_chip->gpio_chip.direction_output = pm8xxx_mpp_dir_output;
	mpp_chip->gpio_chip.to_irq = pm8xxx_mpp_to_irq;
	mpp_chip->gpio_chip.get = pm8xxx_mpp_get;
	mpp_chip->gpio_chip.set = pm8xxx_mpp_set;
	mpp_chip->gpio_chip.dbg_show = pm8xxx_mpp_dbg_show;
	mpp_chip->gpio_chip.ngpio = pdata->core_data.nmpps;
	mpp_chip->gpio_chip.can_sleep = 0;
	mpp_chip->gpio_chip.dev = &pdev->dev;
	mpp_chip->gpio_chip.base = pdata->mpp_base;
	mpp_chip->irq_base = platform_get_irq(pdev, 0);
	mpp_chip->mpp_base = pdata->mpp_base;
	mpp_chip->base_addr = pdata->core_data.base_addr;
	mpp_chip->nmpps = pdata->core_data.nmpps;

	mutex_lock(&pm8xxx_mpp_chips_lock);
	list_add(&mpp_chip->link, &pm8xxx_mpp_chips);
	mutex_unlock(&pm8xxx_mpp_chips_lock);

	platform_set_drvdata(pdev, mpp_chip);

	rc = gpiochip_add(&mpp_chip->gpio_chip);
	if (rc) {
		pr_err("gpiochip_add failed, rc=%d\n", rc);
		goto reset_drvdata;
	}

	rc = pm8xxx_mpp_reg_init(mpp_chip);
	if (rc) {
		pr_err("failed to read MPP ctrl registers, rc=%d\n", rc);
		goto remove_chip;
	}

	pr_info("OK: base=%d, ngpio=%d\n", mpp_chip->gpio_chip.base,
		mpp_chip->gpio_chip.ngpio);

	return 0;

remove_chip:
	if (gpiochip_remove(&mpp_chip->gpio_chip))
		pr_err("failed to remove gpio chip\n");
reset_drvdata:
	platform_set_drvdata(pdev, NULL);
free_mpp_chip:
	kfree(mpp_chip);
	return rc;
}

static int __devexit pm8xxx_mpp_remove(struct platform_device *pdev)
{
	struct pm8xxx_mpp_chip *mpp_chip = platform_get_drvdata(pdev);

	mutex_lock(&pm8xxx_mpp_chips_lock);
	list_del(&mpp_chip->link);
	mutex_unlock(&pm8xxx_mpp_chips_lock);
	platform_set_drvdata(pdev, NULL);
	if (gpiochip_remove(&mpp_chip->gpio_chip))
		pr_err("failed to remove gpio chip\n");
	kfree(mpp_chip->ctrl_reg);
	kfree(mpp_chip);

	return 0;
}

static struct platform_driver pm8xxx_mpp_driver = {
	.probe		= pm8xxx_mpp_probe,
	.remove		= __devexit_p(pm8xxx_mpp_remove),
	.driver		= {
		.name	= PM8XXX_MPP_DEV_NAME,
		.owner	= THIS_MODULE,
	},
};

static int __init pm8xxx_mpp_init(void)
{
	return platform_driver_register(&pm8xxx_mpp_driver);
}
postcore_initcall(pm8xxx_mpp_init);

static void __exit pm8xxx_mpp_exit(void)
{
	platform_driver_unregister(&pm8xxx_mpp_driver);
}
module_exit(pm8xxx_mpp_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("PM8XXX MPP driver");
MODULE_VERSION("1.0");
MODULE_ALIAS("platform:" PM8XXX_MPP_DEV_NAME);
