/*
 * extcon-max77665.c - MAX77665 extcon driver to support MAX77665 MUIC
 *
 * Copyright (c) 2009-2013, NVIDIA CORPORATION.  All rights reserved.
 * Syed Rafiuddin <srafiuddin@nvidia.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/err.h>
#include <linux/platform_device.h>
#include <linux/kobject.h>
#include <linux/extcon.h>
#include <linux/mfd/max77665.h>
#include <linux/max77665-charger.h>

#define	DEV_NAME	"max77665-muic"

#define MAX77665_MUIC_REG_ID		0x0
#define MAX77665_MUIC_REG_INT1		0x1
#define MAX77665_MUIC_REG_INT2		0x2
#define MAX77665_MUIC_REG_INT3		0x3
#define MAX77665_MUIC_REG_STATUS1	0x4
#define MAX77665_MUIC_REG_STATUS2	0x5
#define MAX77665_MUIC_REG_STATUS3	0x6
#define MAX77665_MUIC_REG_INTMASK1	0x7
#define MAX77665_MUIC_REG_INTMASK2	0x8
#define MAX77665_MUIC_REG_INTMASK3	0x9
#define MAX77665_MUIC_REG_CDETCTRL	0xa

#define MAX77665_MUIC_REG_CONTROL1	0xc
#define MAX77665_MUIC_REG_CONTROL2	0xd
#define MAX77665_MUIC_REG_CONTROL3	0xe

/* MAX77665-MUIC STATUS1 register */
#define STATUS1_ADC_SHIFT		0
#define STATUS1_ADCLOW_SHIFT		5
#define STATUS1_ADCERR_SHIFT		6
#define STATUS1_ADC_MASK		(0x1f << STATUS1_ADC_SHIFT)

/* MAX77665-MUIC STATUS2 register */
#define STATUS2_CHGTYP_SHIFT		0
#define STATUS2_CHGDETRUN_SHIFT		3
#define STATUS2_DCDTMR_SHIFT		4
#define STATUS2_DBCHG_SHIFT		5
#define STATUS2_VBVOLT_SHIFT		6
#define STATUS2_CHGTYP_MASK		(0x7 << STATUS2_CHGTYP_SHIFT)

/* MAX77665-MUIC CONTROL1 register */
#define COMN1SW_SHIFT			0
#define COMP2SW_SHIFT			3
#define COMN1SW_MASK			(0x7 << COMN1SW_SHIFT)
#define COMP2SW_MASK			(0x7 << COMP2SW_SHIFT)
#define SW_MASK				(COMP2SW_MASK | COMN1SW_MASK)

#define MAX77665_SW_USB		((1 << COMP2SW_SHIFT) | (1 << COMN1SW_SHIFT))
#define MAX77665_SW_OPEN	((0 << COMP2SW_SHIFT) | (0 << COMN1SW_SHIFT))

#define MAX77665_ADC_GROUND	0x00
#define MAX77665_ADC_OPEN	0x1f

struct max77665_muic {
	struct device *dev;
	struct device *parent;
	int irq;
	struct extcon_dev	edev;
};

const char *max77665_extcon_cable[] = {
	[0] = "USB-Host",
	NULL,
};

static int max77660_id_cable_update(struct max77665_muic *muic)
{
	int ret;
	uint8_t val = 0;

	ret = max77665_read(muic->parent, MAX77665_I2C_SLAVE_MUIC,
			MAX77665_MUIC_REG_STATUS1, &val);
	if (ret < 0) {
		dev_err(muic->dev, "MUIC_STATUS1 read failed %d\n", ret);
		return ret;
	}

	dev_dbg(muic->dev, "STATUS1 = 0x%02x\n", val);

	if (val & 0x1F) {
		extcon_set_cable_state(&muic->edev, "USB-Host", false);
		dev_info(muic->dev, "USB-Host is disconnected\n");
	} else {
		extcon_set_cable_state(&muic->edev, "USB-Host", true);
		dev_info(muic->dev, "USB-Host is connected\n");
	}
	return ret;
}

static irqreturn_t max77665_muic_irq_handler(int irq, void *data)
{
	struct max77665_muic *muic = data;
	uint8_t status = 0;
	int ret;

	ret = max77665_read(muic->parent, MAX77665_I2C_SLAVE_MUIC,
			MAX77665_MUIC_REG_INT1, &status);
	if (ret < 0) {
		dev_dbg(muic->dev, "MUIC_INT1 read failed %d\n", ret);
		goto done;
	}
	dev_dbg(muic->dev, "INT1 is 0x%02x\n", status);

	if (status & 0x1)
		max77660_id_cable_update(muic);
	else
		dev_err(muic->dev, "Unknown interrupt %d\n", irq);
done:
	return IRQ_HANDLED;
}

static int max77665_enable_id_detect_interrupt(struct max77665_muic *muic)
{

	int ret;

	ret = max77665_set_bits(muic->parent, MAX77665_I2C_SLAVE_MUIC,
			MAX77665_MUIC_REG_INTMASK1, 0);
	if (ret < 0)
		dev_err(muic->dev, "MUIC_INTMASK1  set bits failed: %d\n", ret);
	return ret;
}

static int __devinit max77665_muic_probe(struct platform_device *pdev)
{
	struct max77665_muic_platform_data *pdata;
	struct max77665_muic *muic;
	int ret = 0;

	pdata = pdev->dev.platform_data;
	if (!pdata) {
		dev_err(&pdev->dev, "no platform data available\n");
		return -ENODEV;
	}

	muic = devm_kzalloc(&pdev->dev, sizeof(struct max77665_muic),
			GFP_KERNEL);
	if (!muic) {
		dev_err(&pdev->dev, "failed to allocate memory\n");
		return -ENOMEM;
	}

	muic->dev = &pdev->dev;
	muic->parent = pdev->dev.parent;

	muic->irq = platform_get_irq(pdev, 0);
	dev_set_drvdata(&pdev->dev, muic);

	ret = max77665_write(muic->parent, MAX77665_I2C_SLAVE_MUIC,
			MAX77665_MUIC_REG_INTMASK1, 0);
	if (ret < 0) {
		dev_err(&pdev->dev, "MUIC_INTMASK1 write failed: %d\n", ret);
		return ret;
	}

	ret = max77665_write(muic->parent, MAX77665_I2C_SLAVE_MUIC,
			MAX77665_MUIC_REG_INTMASK2, 0);
	if (ret < 0) {
		dev_err(&pdev->dev, "MUIC_INTMASK2 write failed: %d\n", ret);
		return ret;
	}

	ret = max77665_write(muic->parent, MAX77665_I2C_SLAVE_MUIC,
			MAX77665_MUIC_REG_INTMASK3, 0);
	if (ret < 0) {
		dev_err(&pdev->dev, "MUIC_INTMASK3 write failed: %d\n", ret);
		return ret;
	}

	muic->edev.name = (pdata->ext_conn_name) ?
			pdata->ext_conn_name : DEV_NAME;
	muic->edev.supported_cable = max77665_extcon_cable;
	ret = extcon_dev_register(&muic->edev, NULL);
	if (ret < 0) {
		dev_err(&pdev->dev, "extcon device register failed %d\n", ret);
		return ret;
	}

	ret = max77660_id_cable_update(muic);
	if (ret < 0)
		goto scrub;

	ret = request_threaded_irq(muic->irq, NULL, max77665_muic_irq_handler,
			IRQF_ONESHOT | IRQF_EARLY_RESUME, dev_name(&pdev->dev), muic);
	if (ret) {
		dev_err(&pdev->dev, "failed: irq request error :%d)\n", ret);
		goto scrub;
	}

	ret = max77665_enable_id_detect_interrupt(muic);
	if (ret < 0)
		goto scrub;

	device_init_wakeup(&pdev->dev, 1);
	return ret;

scrub:
	extcon_dev_unregister(&muic->edev);
	return ret;
}

static int __devexit max77665_muic_remove(struct platform_device *pdev)
{
	struct max77665_muic *muic = platform_get_drvdata(pdev);

	free_irq(muic->irq, muic);
	extcon_dev_unregister(&muic->edev);
	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int max77665_extcon_suspend(struct device *dev)
{
	struct max77665_muic *muic = dev_get_drvdata(dev);

	if (device_may_wakeup(dev))
		enable_irq_wake(muic->irq);

	return 0;
}

static int max77665_extcon_resume(struct device *dev)
{
	struct max77665_muic *muic = dev_get_drvdata(dev);

	if (device_may_wakeup(dev))
		disable_irq_wake(muic->irq);

	max77665_muic_irq_handler(muic->irq, muic);
	return 0;
};
#endif

static const struct dev_pm_ops max77665_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(max77665_extcon_suspend,
				max77665_extcon_resume)
};

static struct platform_driver max77665_muic_driver = {
	.driver		= {
		.name	= DEV_NAME,
		.owner	= THIS_MODULE,
		.pm = &max77665_pm_ops,
	},
	.probe		= max77665_muic_probe,
	.remove		= __devexit_p(max77665_muic_remove),
};

static int __init max77665_extcon_driver_init(void)
{
	return platform_driver_register(&max77665_muic_driver);
}
subsys_initcall_sync(max77665_extcon_driver_init);

static void __exit max77665_extcon_driver_exit(void)
{
	platform_driver_unregister(&max77665_muic_driver);
}
module_exit(max77665_extcon_driver_exit);

MODULE_DESCRIPTION("Maxim MAX77665 Extcon driver");
MODULE_AUTHOR("Syed Rafiuddin <srafiuddin@nvidia.com>");
MODULE_LICENSE("GPL v2");
