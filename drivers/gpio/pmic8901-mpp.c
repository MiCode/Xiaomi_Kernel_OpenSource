/* Copyright (c) 2010, Code Aurora Forum. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
/*
 * Qualcomm PMIC8901 MPP driver
 *
 */

#include <linux/platform_device.h>
#include <linux/gpio.h>
#include <linux/mfd/pmic8901.h>
#include <mach/mpp.h>
#include <linux/seq_file.h>

/* MPP Control Registers */
#define	SSBI_MPP_CNTRL_BASE		0x27
#define	SSBI_MPP_CNTRL(n)		(SSBI_MPP_CNTRL_BASE + (n))

/* MPP Type */
#define	PM8901_MPP_TYPE_MASK		0xE0
#define	PM8901_MPP_TYPE_SHIFT		5

/* MPP Config Level */
#define	PM8901_MPP_CONFIG_LVL_MASK	0x1C
#define	PM8901_MPP_CONFIG_LVL_SHIFT	2

/* MPP Config Control */
#define	PM8901_MPP_CONFIG_CTL_MASK	0x03

struct pm8901_mpp_chip {
	struct gpio_chip	chip;
	struct pm8901_chip	*pm_chip;
	u8			ctrl[PM8901_MPPS];
};

static int pm8901_mpp_write(struct pm8901_chip *chip, u16 addr, u8 val,
		u8 mask, u8 *bak)
{
	u8 reg = (*bak & ~mask) | (val & mask);
	int rc = pm8901_write(chip, addr, &reg, 1);
	if (!rc)
		*bak = reg;
	return rc;
}

static int pm8901_mpp_to_irq(struct gpio_chip *chip, unsigned offset)
{
	struct pm8901_gpio_platform_data *pdata;
	pdata = chip->dev->platform_data;
	return pdata->irq_base + offset;
}

static int pm8901_mpp_get(struct gpio_chip *chip, unsigned offset)
{
	struct pm8901_mpp_chip *mpp_chip = dev_get_drvdata(chip->dev);
	int ret;

	if ((mpp_chip->ctrl[offset] & PM8901_MPP_TYPE_MASK) >>
			PM8901_MPP_TYPE_SHIFT == PM_MPP_TYPE_D_OUTPUT)
		ret = mpp_chip->ctrl[offset] & PM8901_MPP_CONFIG_CTL_MASK;
	else
		ret = pm8901_irq_get_rt_status(mpp_chip->pm_chip,
				pm8901_mpp_to_irq(chip, offset));
	return ret;
}

static void pm8901_mpp_set(struct gpio_chip *chip, unsigned offset, int val)
{
	struct pm8901_mpp_chip *mpp_chip = dev_get_drvdata(chip->dev);
	u8 reg = val ? PM_MPP_DOUT_CTL_HIGH : PM_MPP_DOUT_CTL_LOW;
	int rc;

	rc = pm8901_mpp_write(mpp_chip->pm_chip, SSBI_MPP_CNTRL(offset),
			reg, PM8901_MPP_CONFIG_CTL_MASK,
			&mpp_chip->ctrl[offset]);
	if (rc)
		pr_err("%s: pm8901_mpp_write(): rc=%d\n", __func__, rc);
}

static int pm8901_mpp_dir_input(struct gpio_chip *chip, unsigned offset)
{
	struct pm8901_mpp_chip *mpp_chip = dev_get_drvdata(chip->dev);
	int rc = pm8901_mpp_write(mpp_chip->pm_chip,
			SSBI_MPP_CNTRL(offset),
			PM_MPP_TYPE_D_INPUT << PM8901_MPP_TYPE_SHIFT,
			PM8901_MPP_TYPE_MASK, &mpp_chip->ctrl[offset]);
	if (rc)
		pr_err("%s: pm8901_mpp_write(): rc=%d\n", __func__, rc);
	return rc;
}

static int pm8901_mpp_dir_output(struct gpio_chip *chip,
		unsigned offset, int val)
{
	struct pm8901_mpp_chip *mpp_chip = dev_get_drvdata(chip->dev);
	u8 reg = (PM_MPP_TYPE_D_OUTPUT << PM8901_MPP_TYPE_SHIFT) |
		(val & PM8901_MPP_CONFIG_CTL_MASK);
	u8 mask = PM8901_MPP_TYPE_MASK | PM8901_MPP_CONFIG_CTL_MASK;
	int rc = pm8901_mpp_write(mpp_chip->pm_chip,
			SSBI_MPP_CNTRL(offset), reg, mask,
			&mpp_chip->ctrl[offset]);
	if (rc)
		pr_err("%s: pm8901_mpp_write(): rc=%d\n", __func__, rc);
	return rc;
}

static void pm8901_mpp_dbg_show(struct seq_file *s, struct gpio_chip *chip)
{
	static const char *ctype[] = { "d_in", "d_out", "bi_dir", "a_in",
		"a_out", "sink", "dtest_sink", "dtest_out" };
	struct pm8901_mpp_chip *mpp_chip = dev_get_drvdata(chip->dev);
	u8 type, state;
	const char *label;
	int i;

	for (i = 0; i < PM8901_MPPS; i++) {
		label = gpiochip_is_requested(chip, i);
		type = (mpp_chip->ctrl[i] & PM8901_MPP_TYPE_MASK) >>
			PM8901_MPP_TYPE_SHIFT;
		state = pm8901_mpp_get(chip, i);
		seq_printf(s, "gpio-%-3d (%-12.12s) %-10.10s"
				" %s 0x%02x\n",
				chip->base + i,
				label ? label : "--",
				ctype[type],
				state ? "hi" : "lo",
				mpp_chip->ctrl[i]);
	}
}

static struct pm8901_mpp_chip pm8901_mpp_chip = {
	.chip = {
		.label			= "pm8901-mpp",
		.to_irq			= pm8901_mpp_to_irq,
		.get			= pm8901_mpp_get,
		.set			= pm8901_mpp_set,
		.direction_input	= pm8901_mpp_dir_input,
		.direction_output	= pm8901_mpp_dir_output,
		.dbg_show		= pm8901_mpp_dbg_show,
		.ngpio			= PM8901_MPPS,
	},
};

int pm8901_mpp_config(unsigned mpp, unsigned type, unsigned level,
		      unsigned control)
{
	u8	config, mask;
	int	rc;

	if (mpp >= PM8901_MPPS)
		return -EINVAL;

	mask = PM8901_MPP_TYPE_MASK | PM8901_MPP_CONFIG_LVL_MASK |
		PM8901_MPP_CONFIG_CTL_MASK;
	config = (type << PM8901_MPP_TYPE_SHIFT) & PM8901_MPP_TYPE_MASK;
	config |= (level << PM8901_MPP_CONFIG_LVL_SHIFT) &
			PM8901_MPP_CONFIG_LVL_MASK;
	config |= control & PM8901_MPP_CONFIG_CTL_MASK;

	rc = pm8901_mpp_write(pm8901_mpp_chip.pm_chip, SSBI_MPP_CNTRL(mpp),
			config, mask, &pm8901_mpp_chip.ctrl[mpp]);
	if (rc)
		pr_err("%s: pm8901_mpp_write(): rc=%d\n", __func__, rc);

	return rc;
}
EXPORT_SYMBOL(pm8901_mpp_config);

static int __devinit pm8901_mpp_probe(struct platform_device *pdev)
{
	int ret, i;
	struct pm8901_gpio_platform_data *pdata = pdev->dev.platform_data;

	pm8901_mpp_chip.pm_chip = dev_get_drvdata(pdev->dev.parent);
	for (i = 0; i < PM8901_MPPS; i++) {
		ret = pm8901_read(pm8901_mpp_chip.pm_chip,
				SSBI_MPP_CNTRL(i), &pm8901_mpp_chip.ctrl[i], 1);
		if (ret)
			goto bail;

	}
	platform_set_drvdata(pdev, &pm8901_mpp_chip);
	pm8901_mpp_chip.chip.dev = &pdev->dev;
	pm8901_mpp_chip.chip.base = pdata->gpio_base;
	ret = gpiochip_add(&pm8901_mpp_chip.chip);

bail:
	pr_info("%s: gpiochip_add(): rc=%d\n", __func__, ret);
	return ret;
}

static int __devexit pm8901_mpp_remove(struct platform_device *pdev)
{
	return gpiochip_remove(&pm8901_mpp_chip.chip);
}

static struct platform_driver pm8901_mpp_driver = {
	.probe		= pm8901_mpp_probe,
	.remove		= __devexit_p(pm8901_mpp_remove),
	.driver		= {
		.name = "pm8901-mpp",
		.owner = THIS_MODULE,
	},
};

static int __init pm8901_mpp_init(void)
{
	return platform_driver_register(&pm8901_mpp_driver);
}

static void __exit pm8901_mpp_exit(void)
{
	platform_driver_unregister(&pm8901_mpp_driver);
}

subsys_initcall(pm8901_mpp_init);
module_exit(pm8901_mpp_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("PMIC8901 MPP driver");
MODULE_VERSION("1.0");
MODULE_ALIAS("platform:pm8901-mpp");
