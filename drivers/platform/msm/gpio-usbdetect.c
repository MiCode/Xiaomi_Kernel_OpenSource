/* Copyright (c) 2013-2015,2017, The Linux Foundation. All rights reserved.
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
#include <linux/of_gpio.h>
#include <linux/gpio.h>
#include <linux/extcon.h>
#include <linux/regulator/consumer.h>

struct gpio_usbdetect {
	struct platform_device	*pdev;
	struct regulator	*vin;
	int			vbus_det_irq;
	int			id_det_irq;
	int			gpio;
	struct extcon_dev	*extcon_dev;
	int			vbus_state;
	bool			id_state;
};

static const unsigned int gpio_usb_extcon_table[] = {
	EXTCON_USB,
	EXTCON_USB_HOST,
	EXTCON_USB_CC,
	EXTCON_USB_SPEED,
	EXTCON_NONE,
};

static irqreturn_t gpio_usbdetect_vbus_irq(int irq, void *data)
{
	struct gpio_usbdetect *usb = data;

	usb->vbus_state = gpio_get_value(usb->gpio);
	if (usb->vbus_state) {
		dev_dbg(&usb->pdev->dev, "setting vbus notification\n");
		extcon_set_cable_state_(usb->extcon_dev, EXTCON_USB_SPEED, 1);
		extcon_set_cable_state_(usb->extcon_dev, EXTCON_USB, 1);
	} else {
		dev_dbg(&usb->pdev->dev, "setting vbus removed notification\n");
		extcon_set_cable_state_(usb->extcon_dev, EXTCON_USB, 0);
	}

	return IRQ_HANDLED;
}

static irqreturn_t gpio_usbdetect_id_irq(int irq, void *data)
{
	struct gpio_usbdetect *usb = data;
	int ret;

	ret = irq_get_irqchip_state(irq, IRQCHIP_STATE_LINE_LEVEL,
			&usb->id_state);
	if (ret < 0) {
		dev_err(&usb->pdev->dev, "unable to read ID IRQ LINE\n");
		return IRQ_HANDLED;
	}

	return IRQ_WAKE_THREAD;
}

static irqreturn_t gpio_usbdetect_id_irq_thread(int irq, void *data)
{
	struct gpio_usbdetect *usb = data;
	bool curr_id_state;
	static int prev_id_state = -EINVAL;

	curr_id_state = usb->id_state;
	if (curr_id_state == prev_id_state) {
		dev_dbg(&usb->pdev->dev, "no change in ID state\n");
		return IRQ_HANDLED;
	}

	if (curr_id_state) {
		dev_dbg(&usb->pdev->dev, "stopping usb host\n");
		extcon_set_cable_state_(usb->extcon_dev, EXTCON_USB_HOST, 0);
		enable_irq(usb->vbus_det_irq);
	} else {
		dev_dbg(&usb->pdev->dev, "starting usb HOST\n");
		disable_irq(usb->vbus_det_irq);
		extcon_set_cable_state_(usb->extcon_dev, EXTCON_USB_SPEED, 1);
		extcon_set_cable_state_(usb->extcon_dev, EXTCON_USB_HOST, 1);
	}

	prev_id_state = curr_id_state;
	return IRQ_HANDLED;
}

static const u32 gpio_usb_extcon_exclusive[] = {0x3, 0};

static int gpio_usbdetect_probe(struct platform_device *pdev)
{
	struct gpio_usbdetect *usb;
	int rc;

	usb = devm_kzalloc(&pdev->dev, sizeof(*usb), GFP_KERNEL);
	if (!usb)
		return -ENOMEM;

	usb->pdev = pdev;

	usb->extcon_dev = devm_extcon_dev_allocate(&pdev->dev,
			gpio_usb_extcon_table);
	if (IS_ERR(usb->extcon_dev)) {
		dev_err(&pdev->dev, "failed to allocate a extcon device\n");
		return PTR_ERR(usb->extcon_dev);
	}

	usb->extcon_dev->mutually_exclusive = gpio_usb_extcon_exclusive;
	rc = devm_extcon_dev_register(&pdev->dev, usb->extcon_dev);
	if (rc) {
		dev_err(&pdev->dev, "failed to register extcon device\n");
		return rc;
	}

	if (of_get_property(pdev->dev.of_node, "vin-supply", NULL)) {
		usb->vin = devm_regulator_get(&pdev->dev, "vin");
		if (IS_ERR(usb->vin)) {
			dev_err(&pdev->dev, "Failed to get VIN regulator: %ld\n",
				PTR_ERR(usb->vin));
			return PTR_ERR(usb->vin);
		}
	}

	if (usb->vin) {
		rc = regulator_enable(usb->vin);
		if (rc) {
			dev_err(&pdev->dev, "Failed to enable VIN regulator: %d\n",
				rc);
			return rc;
		}
	}

	usb->gpio = of_get_named_gpio(pdev->dev.of_node,
				"qcom,vbus-det-gpio", 0);
	if (usb->gpio < 0) {
		dev_err(&pdev->dev, "Failed to get gpio: %d\n", usb->gpio);
		rc = usb->gpio;
		goto error;
	}

	rc = gpio_request(usb->gpio, "vbus-det-gpio");
	if (rc < 0) {
		dev_err(&pdev->dev, "Failed to request gpio: %d\n", rc);
		goto error;
	}

	usb->vbus_det_irq = gpio_to_irq(usb->gpio);
	if (usb->vbus_det_irq < 0) {
		dev_err(&pdev->dev, "get vbus_det_irq failed\n");
		rc = usb->vbus_det_irq;
		goto error;
	}

	rc = devm_request_threaded_irq(&pdev->dev, usb->vbus_det_irq,
				NULL, gpio_usbdetect_vbus_irq,
			      IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING |
			      IRQF_ONESHOT, "vbus_det_irq", usb);
	if (rc) {
		dev_err(&pdev->dev, "request for vbus_det_irq failed: %d\n",
			rc);
		goto error;
	}

	usb->id_det_irq = platform_get_irq_byname(pdev, "pmic_id_irq");
	if (usb->id_det_irq < 0) {
		dev_err(&pdev->dev, "get id_det_irq failed\n");
		rc = usb->id_det_irq;
		goto error;
	}

	rc = devm_request_threaded_irq(&pdev->dev, usb->id_det_irq,
				gpio_usbdetect_id_irq,
				gpio_usbdetect_id_irq_thread,
				IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING |
				IRQF_ONESHOT, "id_det_irq", usb);
	if (rc) {
		dev_err(&pdev->dev, "request for id_det_irq failed: %d\n", rc);
		goto error;
	}

	enable_irq_wake(usb->vbus_det_irq);
	enable_irq_wake(usb->id_det_irq);
	dev_set_drvdata(&pdev->dev, usb);

	if (usb->id_det_irq) {
		gpio_usbdetect_id_irq(usb->id_det_irq, usb);
		if (!usb->id_state) {
			gpio_usbdetect_id_irq_thread(usb->id_det_irq, usb);
			return 0;
		}
	}

	/* Read and report initial VBUS state */
	gpio_usbdetect_vbus_irq(usb->vbus_det_irq, usb);

	return 0;

error:
	if (usb->vin)
		regulator_disable(usb->vin);
	return rc;
}

static int gpio_usbdetect_remove(struct platform_device *pdev)
{
	struct gpio_usbdetect *usb = dev_get_drvdata(&pdev->dev);

	disable_irq_wake(usb->vbus_det_irq);
	disable_irq(usb->vbus_det_irq);
	disable_irq_wake(usb->id_det_irq);
	disable_irq(usb->id_det_irq);
	if (usb->vin)
		regulator_disable(usb->vin);

	return 0;
}

static struct of_device_id of_match_table[] = {
	{ .compatible = "qcom,gpio-usbdetect", },
	{}
};

static struct platform_driver gpio_usbdetect_driver = {
	.driver		= {
		.name	= "qcom,gpio-usbdetect",
		.of_match_table = of_match_table,
	},
	.probe		= gpio_usbdetect_probe,
	.remove		= gpio_usbdetect_remove,
};

module_driver(gpio_usbdetect_driver, platform_driver_register,
		platform_driver_unregister);

MODULE_DESCRIPTION("GPIO USB VBUS Detection driver");
MODULE_LICENSE("GPL v2");
