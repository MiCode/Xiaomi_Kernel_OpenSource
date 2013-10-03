/* Copyright (c) 2013, The Linux Foundation. All rights reserved.
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

#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/err.h>
#include <linux/errno.h>
#include <linux/slab.h>
#include <linux/of_platform.h>
#include <linux/interrupt.h>
#include <linux/power_supply.h>
#include <linux/regulator/consumer.h>

struct qpnp_usbdetect {
	struct platform_device	*pdev;
	struct regulator	*vin;
	struct power_supply	*usb_psy;
	int			vbus_det_irq;
};

static irqreturn_t qpnp_usbdetect_vbus_irq(int irq, void *data)
{
	struct qpnp_usbdetect *usb = data;
	int vbus;

	vbus = !!irq_read_line(irq);
	power_supply_set_present(usb->usb_psy, vbus);

	return IRQ_HANDLED;
}

static int qpnp_usbdetect_probe(struct platform_device *pdev)
{
	struct qpnp_usbdetect *usb;
	struct power_supply *usb_psy;
	int rc;

	usb_psy = power_supply_get_by_name("usb");
	if (!usb_psy) {
		dev_dbg(&pdev->dev, "USB power_supply not found, deferring probe\n");
		return -EPROBE_DEFER;
	}

	usb = devm_kzalloc(&pdev->dev, sizeof(*usb), GFP_KERNEL);
	if (!usb)
		return -ENOMEM;

	usb->pdev = pdev;
	usb->usb_psy = usb_psy;

	usb->vin = devm_regulator_get(&pdev->dev, "vin");
	if (IS_ERR(usb->vin)) {
		dev_err(&pdev->dev, "Failed to get VIN regulator: %ld\n",
			PTR_ERR(usb->vin));
		return PTR_ERR(usb->vin);
	}

	rc = regulator_enable(usb->vin);
	if (rc) {
		dev_err(&pdev->dev, "Failed to enable VIN regulator: %d\n", rc);
		return rc;
	}

	usb->vbus_det_irq = platform_get_irq_byname(pdev, "vbus_det_irq");
	if (usb->vbus_det_irq < 0) {
		regulator_disable(usb->vin);
		return usb->vbus_det_irq;
	}

	rc = devm_request_irq(&pdev->dev, usb->vbus_det_irq,
			      qpnp_usbdetect_vbus_irq,
			      IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING,
			      "vbus_det_irq", usb);
	if (rc) {
		dev_err(&pdev->dev, "request for vbus_det_irq failed: %d\n",
			rc);
		regulator_disable(usb->vin);
		return rc;
	}

	enable_irq_wake(usb->vbus_det_irq);
	dev_set_drvdata(&pdev->dev, usb);

	return 0;
}

static int qpnp_usbdetect_remove(struct platform_device *pdev)
{
	struct qpnp_usbdetect *usb = dev_get_drvdata(&pdev->dev);

	disable_irq_wake(usb->vbus_det_irq);
	disable_irq(usb->vbus_det_irq);
	regulator_disable(usb->vin);

	return 0;
}

static struct of_device_id of_match_table[] = {
	{	.compatible = "qcom,qpnp-usbdetect",
	}
};

static struct platform_driver qpnp_usbdetect_driver = {
	.driver		= {
		.name	= "qcom,qpnp-usbdetect",
		.of_match_table = of_match_table,
	},
	.probe		= qpnp_usbdetect_probe,
	.remove		= qpnp_usbdetect_remove,
};

module_driver(qpnp_usbdetect_driver, platform_driver_register,
		platform_driver_unregister);

MODULE_DESCRIPTION("QPNP PMIC USB VBUS Detection driver");
MODULE_LICENSE("GPL v2");
