/* Copyright (c) 2009-2011, Code Aurora Forum. All rights reserved.
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
 * Qualcomm PMIC8058 MPP driver
 *
 */

#include <linux/platform_device.h>
#include <linux/gpio.h>
#include <linux/mfd/pmic8058.h>
#include <mach/mpp.h>
#include <linux/seq_file.h>

#ifndef CONFIG_GPIOLIB
#include "gpio_chip.h"
#endif

/* MPP Control Registers */
#define	SSBI_MPP_CNTRL_BASE		0x50
#define	SSBI_MPP_CNTRL(n)		(SSBI_MPP_CNTRL_BASE + (n))

/* MPP Type */
#define	PM8058_MPP_TYPE_MASK		0xE0
#define	PM8058_MPP_TYPE_SHIFT		5

/* MPP Config Level */
#define	PM8058_MPP_CONFIG_LVL_MASK	0x1C
#define	PM8058_MPP_CONFIG_LVL_SHIFT	2

/* MPP Config Control */
#define	PM8058_MPP_CONFIG_CTL_MASK	0x03

static int pm8058_mpp_get(struct gpio_chip *chip, unsigned mpp)
{
	struct pm8058_gpio_platform_data *pdata;
	struct pm8058_chip *pm_chip;

	if (mpp >= PM8058_MPPS || chip == NULL)
		return -EINVAL;

	pdata = chip->dev->platform_data;
	pm_chip = dev_get_drvdata(chip->dev->parent);

	return pm8058_irq_get_rt_status(pm_chip,
		pdata->irq_base + mpp);
}

#ifndef CONFIG_GPIOLIB
static int pm8058_mpp_get_irq_num(struct gpio_chip *chip,
				   unsigned int gpio,
				   unsigned int *irqp,
				   unsigned long *irqnumflagsp)
{
	struct pm8058_gpio_platform_data *pdata;

	pdata = chip->dev->platform_data;
	gpio -= chip->start;
	*irqp = pdata->irq_base + gpio;
	if (irqnumflagsp)
		*irqnumflagsp = 0;
	return 0;
}

static int pm8058_mpp_read(struct gpio_chip *chip, unsigned n)
{
	n -= chip->start;
	return pm8058_mpp_get(chip, n);
}

struct msm_gpio_chip pm8058_mpp_chip = {
	.chip = {
		.get_irq_num = pm8058_mpp_get_irq_num,
		.read = pm8058_mpp_read,
	}
};

int pm8058_mpp_config(unsigned mpp, unsigned type, unsigned level,
		      unsigned control)
{
	u8	config;
	int	rc;
	struct pm8058_chip *pm_chip;

	if (mpp >= PM8058_MPPS)
		return -EINVAL;

	pm_chip = dev_get_drvdata(pm8058_mpp_chip->dev->parent);

	config = (type << PM8058_MPP_TYPE_SHIFT) & PM8058_MPP_TYPE_MASK;
	config |= (level << PM8058_MPP_CONFIG_LVL_SHIFT) &
			PM8058_MPP_CONFIG_LVL_MASK;
	config |= control & PM8058_MPP_CONFIG_CTL_MASK;

	rc = pm8058_write(pm_chip, SSBI_MPP_CNTRL(mpp), &config, 1);
	if (rc)
		pr_err("%s: pm8058_write(): rc=%d\n", __func__, rc);

	return rc;
}
EXPORT_SYMBOL(pm8058_mpp_config);

static int __devinit pm8058_mpp_probe(struct platform_device *pdev)
{
	int	rc;
	struct pm8058_gpio_platform_data *pdata = pdev->dev.platform_data;

	pm8058_mpp_chip.chip.dev = &pdev->dev;
	pm8058_mpp_chip.chip.start = pdata->gpio_base;
	pm8058_mpp_chip.chip.end = pdata->gpio_base + PM8058_MPPS - 1;
	rc = register_gpio_chip(&pm8058_mpp_chip.chip);
	if (!rc) {
		if (pdata->init)
			ret = pdata->init();
	}
	pr_info("%s: register_gpio_chip(): rc=%d\n", __func__, rc);

	return rc;
}

static int __devexit pm8058_mpp_remove(struct platform_device *pdev)
{
	return 0;
}

#else

static int pm8058_mpp_to_irq(struct gpio_chip *chip, unsigned offset)
{
	struct pm8058_gpio_platform_data *pdata;
	pdata = chip->dev->platform_data;
	return pdata->irq_base + offset;
}

static int pm8058_mpp_read(struct gpio_chip *chip, unsigned offset)
{
	return pm8058_mpp_get(chip, offset);
}

static void pm8058_mpp_dbg_show(struct seq_file *s, struct gpio_chip *chip)
{
	static const char *ctype[] = { "d_in", "d_out", "bi_dir", "a_in",
		"a_out", "sink", "dtest_sink", "dtest_out" };
	struct pm8058_chip *pm_chip = dev_get_drvdata(chip->dev->parent);
	u8 type, state, ctrl;
	const char *label;
	int i;

	for (i = 0; i < PM8058_MPPS; i++) {
		pm8058_read(pm_chip, SSBI_MPP_CNTRL(i), &ctrl, 1);
		label = gpiochip_is_requested(chip, i);
		type = (ctrl & PM8058_MPP_TYPE_MASK) >>
			PM8058_MPP_TYPE_SHIFT;
		state = pm8058_mpp_get(chip, i);
		seq_printf(s, "gpio-%-3d (%-12.12s) %-10.10s"
				" %s 0x%02x\n",
				chip->base + i,
				label ? label : "--",
				ctype[type],
				state ? "hi" : "lo",
				ctrl);
	}
}

static struct gpio_chip pm8058_mpp_chip = {
	.label		= "pm8058-mpp",
	.to_irq		= pm8058_mpp_to_irq,
	.get		= pm8058_mpp_read,
	.dbg_show	= pm8058_mpp_dbg_show,
	.ngpio		= PM8058_MPPS,
	.can_sleep	= 1,
};

int pm8058_mpp_config(unsigned mpp, unsigned type, unsigned level,
		      unsigned control)
{
	u8	config;
	int	rc;
	struct pm8058_chip *pm_chip;

	if (mpp >= PM8058_MPPS)
		return -EINVAL;

	pm_chip = dev_get_drvdata(pm8058_mpp_chip.dev->parent);

	config = (type << PM8058_MPP_TYPE_SHIFT) & PM8058_MPP_TYPE_MASK;
	config |= (level << PM8058_MPP_CONFIG_LVL_SHIFT) &
			PM8058_MPP_CONFIG_LVL_MASK;
	config |= control & PM8058_MPP_CONFIG_CTL_MASK;

	rc = pm8058_write(pm_chip, SSBI_MPP_CNTRL(mpp), &config, 1);
	if (rc)
		pr_err("%s: pm8058_write(): rc=%d\n", __func__, rc);

	return rc;
}
EXPORT_SYMBOL(pm8058_mpp_config);

static int __devinit pm8058_mpp_probe(struct platform_device *pdev)
{
	int ret;
	struct pm8058_gpio_platform_data *pdata = pdev->dev.platform_data;

	pm8058_mpp_chip.dev = &pdev->dev;
	pm8058_mpp_chip.base = pdata->gpio_base;
	ret = gpiochip_add(&pm8058_mpp_chip);
	if (!ret) {
		if (pdata->init)
			ret = pdata->init();
	}

	pr_info("%s: gpiochip_add(): ret=%d\n", __func__, ret);
	return ret;
}

static int __devexit pm8058_mpp_remove(struct platform_device *pdev)
{
	return gpiochip_remove(&pm8058_mpp_chip);
}

#endif

static struct platform_driver pm8058_mpp_driver = {
	.probe		= pm8058_mpp_probe,
	.remove		= __devexit_p(pm8058_mpp_remove),
	.driver		= {
		.name = "pm8058-mpp",
		.owner = THIS_MODULE,
	},
};

static int __init pm8058_mpp_init(void)
{
	return platform_driver_register(&pm8058_mpp_driver);
}

static void __exit pm8058_mpp_exit(void)
{
	platform_driver_unregister(&pm8058_mpp_driver);
}

subsys_initcall(pm8058_mpp_init);
module_exit(pm8058_mpp_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("PMIC8058 MPP driver");
MODULE_VERSION("1.0");
MODULE_ALIAS("platform:pm8058-mpp");

