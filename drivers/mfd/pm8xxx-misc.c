/*
 * Copyright (c) 2011, Code Aurora Forum. All rights reserved.
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

#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/platform_device.h>
#include <linux/mfd/pm8xxx/core.h>
#include <linux/mfd/pm8xxx/misc.h>

/* PON CTRL 1 register */
#define REG_PM8058_PON_CTRL_1			0x01C
#define REG_PM8921_PON_CTRL_1			0x01C

#define PON_CTRL_1_PULL_UP_MASK			0xE0
#define PON_CTRL_1_USB_PWR_EN			0x10

#define PON_CTRL_1_WD_EN_MASK			0x08
#define PON_CTRL_1_WD_EN_RESET			0x08
#define PON_CTRL_1_WD_EN_PWR_OFF		0x00

/* Regulator L22 control register */
#define REG_PM8058_L22_CTRL			0x121

/* SLEEP CTRL register */
#define REG_PM8058_SLEEP_CTRL			0x02B
#define REG_PM8921_SLEEP_CTRL			0x10A

#define SLEEP_CTRL_SMPL_EN_MASK			0x04
#define SLEEP_CTRL_SMPL_EN_RESET		0x04
#define SLEEP_CTRL_SMPL_EN_PWR_OFF		0x00

/* FTS regulator PMR registers */
#define REG_PM8901_REGULATOR_S1_PMR		0xA7
#define REG_PM8901_REGULATOR_S2_PMR		0xA8
#define REG_PM8901_REGULATOR_S3_PMR		0xA9
#define REG_PM8901_REGULATOR_S4_PMR		0xAA

#define PM8901_REGULATOR_PMR_STATE_MASK		0x60
#define PM8901_REGULATOR_PMR_STATE_OFF		0x20

struct pm8xxx_misc_chip {
	struct list_head			link;
	struct pm8xxx_misc_platform_data	pdata;
	struct device				*dev;
	enum pm8xxx_version			version;
};

static LIST_HEAD(pm8xxx_misc_chips);
static DEFINE_SPINLOCK(pm8xxx_misc_chips_lock);

static int pm8xxx_misc_masked_write(struct pm8xxx_misc_chip *chip, u16 addr,
				    u8 mask, u8 val)
{
	int rc;
	u8 reg;

	rc = pm8xxx_readb(chip->dev->parent, addr, &reg);
	if (rc) {
		pr_err("pm8xxx_readb(0x%03X) failed, rc=%d\n", addr, rc);
		return rc;
	}
	reg &= ~mask;
	reg |= val & mask;
	rc = pm8xxx_writeb(chip->dev->parent, addr, reg);
	if (rc)
		pr_err("pm8xxx_writeb(0x%03X)=0x%02X failed, rc=%d\n", addr,
			reg, rc);
	return rc;
}

static int __pm8058_reset_pwr_off(struct pm8xxx_misc_chip *chip, int reset)
{
	int rc;

	/*
	 * Fix-up: Set regulator LDO22 to 1.225 V in high power mode. Leave its
	 * pull-down state intact. This ensures a safe shutdown.
	 */
	rc = pm8xxx_misc_masked_write(chip, REG_PM8058_L22_CTRL, 0xBF, 0x93);
	if (rc) {
		pr_err("pm8xxx_misc_masked_write failed, rc=%d\n", rc);
		goto read_write_err;
	}

	/* Enable SMPL if resetting is desired. */
	rc = pm8xxx_misc_masked_write(chip, REG_PM8058_SLEEP_CTRL,
	       SLEEP_CTRL_SMPL_EN_MASK,
	       (reset ? SLEEP_CTRL_SMPL_EN_RESET : SLEEP_CTRL_SMPL_EN_PWR_OFF));
	if (rc) {
		pr_err("pm8xxx_misc_masked_write failed, rc=%d\n", rc);
		goto read_write_err;
	}

	/*
	 * Select action to perform (reset or shutdown) when PS_HOLD goes low.
	 * Also ensure that KPD, CBL0, and CBL1 pull ups are enabled and that
	 * USB charging is enabled.
	 */
	rc = pm8xxx_misc_masked_write(chip, REG_PM8058_PON_CTRL_1,
		PON_CTRL_1_PULL_UP_MASK | PON_CTRL_1_USB_PWR_EN
		| PON_CTRL_1_WD_EN_MASK,
		PON_CTRL_1_PULL_UP_MASK | PON_CTRL_1_USB_PWR_EN
		| (reset ? PON_CTRL_1_WD_EN_RESET : PON_CTRL_1_WD_EN_PWR_OFF));
	if (rc) {
		pr_err("pm8xxx_misc_masked_write failed, rc=%d\n", rc);
		goto read_write_err;
	}

read_write_err:
	return rc;
}

static int __pm8901_reset_pwr_off(struct pm8xxx_misc_chip *chip, int reset)
{
	int rc = 0, i;
	u8 pmr_addr[4] = {
		REG_PM8901_REGULATOR_S2_PMR,
		REG_PM8901_REGULATOR_S3_PMR,
		REG_PM8901_REGULATOR_S4_PMR,
		REG_PM8901_REGULATOR_S1_PMR,
	};

	/* Fix-up: Turn off regulators S1, S2, S3, S4 when shutting down. */
	if (!reset) {
		for (i = 0; i < 4; i++) {
			rc = pm8xxx_misc_masked_write(chip, pmr_addr[i],
				PM8901_REGULATOR_PMR_STATE_MASK,
				PM8901_REGULATOR_PMR_STATE_OFF);
			if (rc) {
				pr_err("pm8xxx_misc_masked_write failed, "
					"rc=%d\n", rc);
				goto read_write_err;
			}
		}
	}

read_write_err:
	return rc;
}

static int __pm8921_reset_pwr_off(struct pm8xxx_misc_chip *chip, int reset)
{
	int rc;

	/* Enable SMPL if resetting is desired. */
	rc = pm8xxx_misc_masked_write(chip, REG_PM8921_SLEEP_CTRL,
	       SLEEP_CTRL_SMPL_EN_MASK,
	       (reset ? SLEEP_CTRL_SMPL_EN_RESET : SLEEP_CTRL_SMPL_EN_PWR_OFF));
	if (rc) {
		pr_err("pm8xxx_misc_masked_write failed, rc=%d\n", rc);
		goto read_write_err;
	}

	/*
	 * Select action to perform (reset or shutdown) when PS_HOLD goes low.
	 * Also ensure that KPD, CBL0, and CBL1 pull ups are enabled and that
	 * USB charging is enabled.
	 */
	rc = pm8xxx_misc_masked_write(chip, REG_PM8921_PON_CTRL_1,
		PON_CTRL_1_PULL_UP_MASK | PON_CTRL_1_USB_PWR_EN
		| PON_CTRL_1_WD_EN_MASK,
		PON_CTRL_1_PULL_UP_MASK | PON_CTRL_1_USB_PWR_EN
		| (reset ? PON_CTRL_1_WD_EN_RESET : PON_CTRL_1_WD_EN_PWR_OFF));
	if (rc) {
		pr_err("pm8xxx_misc_masked_write failed, rc=%d\n", rc);
		goto read_write_err;
	}

read_write_err:
	return rc;
}

/**
 * pm8xxx_reset_pwr_off - switch all PM8XXX PMIC chips attached to the system to
 *			  either reset or shutdown when they are turned off
 * @reset: 0 = shudown the PMICs, 1 = shutdown and then restart the PMICs
 *
 * RETURNS: an appropriate -ERRNO error value on error, or zero for success.
 */
int pm8xxx_reset_pwr_off(int reset)
{
	struct pm8xxx_misc_chip *chip;
	unsigned long flags;
	int rc = 0;

	spin_lock_irqsave(&pm8xxx_misc_chips_lock, flags);

	/* Loop over all attached PMICs and call specific functions for them. */
	list_for_each_entry(chip, &pm8xxx_misc_chips, link) {
		switch (chip->version) {
		case PM8XXX_VERSION_8058:
			rc = __pm8058_reset_pwr_off(chip, reset);
			break;
		case PM8XXX_VERSION_8901:
			rc = __pm8901_reset_pwr_off(chip, reset);
			break;
		case PM8XXX_VERSION_8921:
			rc = __pm8921_reset_pwr_off(chip, reset);
			break;
		default:
			/* PMIC doesn't have reset_pwr_off; do nothing. */
			break;
		}
		if (rc) {
			pr_err("reset_pwr_off failed, rc=%d\n", rc);
			break;
		}
	}

	spin_unlock_irqrestore(&pm8xxx_misc_chips_lock, flags);

	return rc;
}
EXPORT_SYMBOL_GPL(pm8xxx_reset_pwr_off);

static int __devinit pm8xxx_misc_probe(struct platform_device *pdev)
{
	const struct pm8xxx_misc_platform_data *pdata = pdev->dev.platform_data;
	struct pm8xxx_misc_chip *chip;
	struct pm8xxx_misc_chip *sibling;
	struct list_head *prev;
	unsigned long flags;
	int rc = 0;

	if (!pdata) {
		pr_err("missing platform data\n");
		return -EINVAL;
	}

	chip = kzalloc(sizeof(struct pm8xxx_misc_chip), GFP_KERNEL);
	if (!chip) {
		pr_err("Cannot allocate %d bytes\n",
			sizeof(struct pm8xxx_misc_chip));
		return -ENOMEM;
	}

	chip->dev = &pdev->dev;
	chip->version = pm8xxx_get_version(chip->dev->parent);
	memcpy(&(chip->pdata), pdata, sizeof(struct pm8xxx_misc_platform_data));

	/* Insert PMICs in priority order (lowest value first). */
	spin_lock_irqsave(&pm8xxx_misc_chips_lock, flags);
	prev = &pm8xxx_misc_chips;
	list_for_each_entry(sibling, &pm8xxx_misc_chips, link) {
		if (chip->pdata.priority < sibling->pdata.priority)
			break;
		else
			prev = &sibling->link;
	}
	list_add(&chip->link, prev);
	spin_unlock_irqrestore(&pm8xxx_misc_chips_lock, flags);

	platform_set_drvdata(pdev, chip);

	return rc;
}

static int __devexit pm8xxx_misc_remove(struct platform_device *pdev)
{
	struct pm8xxx_misc_chip *chip = platform_get_drvdata(pdev);
	unsigned long flags;

	spin_lock_irqsave(&pm8xxx_misc_chips_lock, flags);
	list_del(&chip->link);
	spin_unlock_irqrestore(&pm8xxx_misc_chips_lock, flags);

	platform_set_drvdata(pdev, NULL);
	kfree(chip);

	return 0;
}

static struct platform_driver pm8xxx_misc_driver = {
	.probe	= pm8xxx_misc_probe,
	.remove	= __devexit_p(pm8xxx_misc_remove),
	.driver	= {
		.name	= PM8XXX_MISC_DEV_NAME,
		.owner	= THIS_MODULE,
	},
};

static int __init pm8xxx_misc_init(void)
{
	return platform_driver_register(&pm8xxx_misc_driver);
}
postcore_initcall(pm8xxx_misc_init);

static void __exit pm8xxx_misc_exit(void)
{
	platform_driver_unregister(&pm8xxx_misc_driver);
}
module_exit(pm8xxx_misc_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("PMIC 8XXX misc driver");
MODULE_VERSION("1.0");
MODULE_ALIAS("platform:" PM8XXX_MISC_DEV_NAME);
