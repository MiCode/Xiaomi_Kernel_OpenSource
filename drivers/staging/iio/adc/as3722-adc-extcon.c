/*
 * as3722-adc-extcon.c -- AMS AS3722 ADC EXTCON.
 *
 * Copyright (c) 2013, NVIDIA Corporation. All rights reserved.
 *
 * Author: Mallikarjun Kasoju<mkasoju@nvidia.com>
 * Author: Laxman Dewangan <ldewangan@nvidia.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation version 2.
 *
 * This program is distributed "as is" WITHOUT ANY WARRANTY of any kind,
 * whether express or implied; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
 * 02111-1307, USA
 */
#include <linux/module.h>
#include <linux/err.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/pm.h>
#include <linux/extcon.h>
#include <linux/mfd/as3722.h>

struct as3722_adc {
	struct device		*dev;
	struct as3722		*as3722;
	struct extcon_dev	edev;
	int			irq;
	int			hi_threshold;
	int			low_threshold;
};

const char *as3722_adc_excon_cable[] = {
	[0] = "USB-Host",
	NULL,
};

static int as3722_read_adc1_cable_update(struct as3722_adc *adc)
{
	struct as3722 *as3722 = adc->as3722;
	int result;
	int ret;
	u32 val;

	ret = as3722_read(as3722, AS3722_ADC1_MSB_RESULT_REG , &val);
	if (ret < 0) {
		dev_err(adc->dev, "ADC1_MSB_RESULT read failed %d\n", ret);
		return ret;
	}
	result = ((val & AS3722_ADC_MASK_MSB_VAL) << 3);

	ret = as3722_read(as3722, AS3722_ADC1_LSB_RESULT_REG, &val);
	if (ret < 0) {
		dev_err(adc->dev, "ADC1_LSB_RESULT read failed %d\n", ret);
		return ret;
	}
	result |= val & AS3722_ADC_MASK_LSB_VAL;

	if (result >= adc->hi_threshold) {
		extcon_set_cable_state(&adc->edev, "USB-Host", false);
		dev_info(adc->dev, "USB-Host is disconnected\n");
	} else if (result <= adc->low_threshold) {
		extcon_set_cable_state(&adc->edev, "USB-Host", true);
		dev_info(adc->dev, "USB-Host is connected\n");
	}
	return ret;
}

static irqreturn_t as3722_adc_extcon_irq(int irq, void *data)
{
	struct as3722_adc *adc = data;

	as3722_read_adc1_cable_update(adc);
	return IRQ_HANDLED;
}

static int as3722_adc_extcon_probe(struct platform_device *pdev)
{
	struct as3722 *as3722 = dev_get_drvdata(pdev->dev.parent);
	struct as3722_platform_data *pdata = dev_get_platdata(pdev->dev.parent);
	struct as3722_adc_extcon_platform_data *extcon_pdata;
	struct as3722_adc *adc;
	int ret = 0;
	unsigned int try_counter = 0;
	u32 val;

	if (!pdata || !pdata->extcon_pdata) {
		dev_err(&pdev->dev, "no platform data available\n");
		return -ENODEV;
	}

	extcon_pdata = pdata->extcon_pdata;
	adc = devm_kzalloc(&pdev->dev, sizeof(*adc), GFP_KERNEL);
	if (!adc) {
		dev_err(&pdev->dev, "Malloc adc failed\n");
		return -ENOMEM;
	}

	adc->dev = &pdev->dev;
	adc->as3722 = as3722;
	dev_set_drvdata(&pdev->dev, adc);
	adc->irq = platform_get_irq(pdev, 0);
	adc->hi_threshold = extcon_pdata->hi_threshold;
	adc->low_threshold = extcon_pdata->low_threshold;

	if (!extcon_pdata->enable_adc1_continuous_mode)
		goto skip_adc_config;

	/* Set ADC threshold values */
	ret = as3722_write(as3722, AS3722_ADC1_THRESHOLD_HI_MSB_REG,
				(extcon_pdata->hi_threshold >> 3) & 0x7F);
	if (ret < 0) {
		dev_err(adc->dev, "ADC1_THRESHOLD_HI_MSB write failed %d\n",
				ret);
		return ret;
	}

	ret = as3722_write(as3722, AS3722_ADC1_THRESHOLD_HI_LSB_REG,
				(extcon_pdata->hi_threshold) & 0x7);
	if (ret < 0) {
		dev_err(adc->dev, "ADC1_THRESHOLD_HI_LSB write failed %d\n",
				ret);
		return ret;
	}

	ret = as3722_write(as3722, AS3722_ADC1_THRESHOLD_LO_MSB_REG,
				(extcon_pdata->low_threshold >> 3) & 0x7F);
	if (ret < 0) {
		dev_err(adc->dev, "ADC1_THRESHOLD_LO_MSB write failed %d\n",
				ret);
		return ret;
	}

	ret = as3722_write(as3722, AS3722_ADC1_THRESHOLD_LO_LSB_REG,
				(extcon_pdata->low_threshold) & 0x7);
	if (ret < 0) {
		dev_err(adc->dev, "ADC1_THRESHOLD_LO_LSB write failed %d\n",
				ret);
		return ret;
	}

	/* Configure adc1 */
	val = (extcon_pdata->adc_channel & 0x1F) |
			AS3722_ADC1_INTEVAL_SCAN_MASK;
	if (extcon_pdata->enable_low_voltage_range)
		val |= AS3722_ADC1_LOW_VOLTAGE_RANGE_MASK;
	ret = as3722_write(as3722, AS3722_ADC1_CONTROL_REG, val);
	if (ret < 0) {
		dev_err(adc->dev, "ADC1_CONTROL write failed %d\n", ret);
		return ret;
	}

	/* Start ADC */
	ret = as3722_update_bits(as3722, AS3722_ADC1_CONTROL_REG,
				AS3722_ADC1_CONVERSION_START_MASK,
				AS3722_ADC1_CONVERSION_START_MASK);
	if (ret < 0) {
		dev_err(adc->dev, "ADC1_CONTROL write failed %d\n", ret);
		return ret;
	}

	/* Wait for 1 conversion */
	do {
		ret = as3722_read(as3722, AS3722_ADC1_MSB_RESULT_REG ,
				&val);
		if (ret < 0) {
			dev_err(adc->dev, "ADC1_MSB_RESULT read failed %d\n",
				ret);
			return ret;
		}
		if (!(val & AS3722_ADC1_MASK_CONV_NOTREADY))
			break;
		udelay(500);
	} while (try_counter++ < 10);

	adc->edev.name = (extcon_pdata->connection_name) ?
				extcon_pdata->connection_name : pdev->name;
	adc->edev.supported_cable = as3722_adc_excon_cable;
	ret = extcon_dev_register(&adc->edev, NULL);
	if (ret < 0) {
		dev_err(&pdev->dev, "extcon dev register failed %d\n", ret);
		return ret;
	}

	/* Read ADC result */
	ret = as3722_read_adc1_cable_update(adc);
	if (ret < 0) {
		dev_err(&pdev->dev, "ADC read failed %d\n", ret);
		goto scrub_edev;
	}

	ret = as3722_update_bits(as3722, AS3722_ADC1_CONTROL_REG,
		AS3722_ADC1_INTEVAL_SCAN_MASK, AS3722_ADC1_INTEVAL_SCAN_MASK);
	if (ret < 0) {
		dev_err(adc->dev, "ADC1 INTEVAL_SCAN set failed: %d\n", ret);
		goto scrub_edev;
	}

	ret = request_threaded_irq(adc->irq, NULL, as3722_adc_extcon_irq,
		IRQF_ONESHOT | IRQF_EARLY_RESUME, dev_name(adc->dev),
		adc);
	if (ret < 0) {
		dev_err(adc->dev, "request irq %d failed: %dn", adc->irq, ret);
		goto stop_adc1;
	}

skip_adc_config:
	device_init_wakeup(&pdev->dev, 1);
	return 0;

stop_adc1:
	as3722_update_bits(as3722, AS3722_ADC1_CONTROL_REG,
			AS3722_ADC1_CONVERSION_START_MASK, 0);
scrub_edev:
	extcon_dev_unregister(&adc->edev);

	return ret;
}

static int as3722_adc_extcon_remove(struct platform_device *pdev)
{
	struct as3722_adc *adc = dev_get_drvdata(&pdev->dev);
	struct as3722 *as3722 = adc->as3722;

	as3722_update_bits(as3722, AS3722_ADC1_CONTROL_REG,
			AS3722_ADC1_CONVERSION_START_MASK, 0);
	extcon_dev_unregister(&adc->edev);
	free_irq(adc->irq, adc);
	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int as3722_adc_extcon_suspend(struct device *dev)
{
	struct as3722_adc *adc = dev_get_drvdata(dev);

	if (device_may_wakeup(dev))
		enable_irq_wake(adc->irq);

	return 0;
}

static int as3722_adc_extcon_resume(struct device *dev)
{
	struct as3722_adc *adc = dev_get_drvdata(dev);

	if (device_may_wakeup(dev))
		disable_irq_wake(adc->irq);

	return 0;
};
#endif

static const struct dev_pm_ops as3722_adc_extcon_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(as3722_adc_extcon_suspend,
				as3722_adc_extcon_resume)
};

static struct platform_driver as3722_adc_extcon_driver = {
	.probe = as3722_adc_extcon_probe,
	.remove = as3722_adc_extcon_remove,
	.driver = {
		.name = "as3722-adc-extcon",
		.owner = THIS_MODULE,
		.pm = &as3722_adc_extcon_pm_ops,
	},
};

static int __init as3722_adc_extcon_init(void)
{
	return platform_driver_register(&as3722_adc_extcon_driver);
}

subsys_initcall_sync(as3722_adc_extcon_init);

static void __exit as3722_adc_extcon_exit(void)
{
	platform_driver_unregister(&as3722_adc_extcon_driver);
}
module_exit(as3722_adc_extcon_exit);

MODULE_DESCRIPTION("as3722 ADC extcon driver");
MODULE_AUTHOR("Mallikarjun Kasoju <mkasoju@nvidia.com>");
MODULE_AUTHOR("Laxman Dewangan <ldewangan@nvidia.com>");
MODULE_ALIAS("platform:as3722-adc-extcon");
MODULE_LICENSE("GPL v2");
