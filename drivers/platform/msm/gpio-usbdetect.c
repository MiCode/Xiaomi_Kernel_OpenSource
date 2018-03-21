/* Copyright (c) 2013-2018, The Linux Foundation. All rights reserved.
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
#include <linux/of.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/interrupt.h>
#include <linux/power_supply.h>
#include <linux/regulator/consumer.h>

#define VBUS_ON_DEBOUNCE_MS		400
#define WAKEUP_SRC_TIMEOUT_MS		1000

struct gpio_usbdetect {
	struct platform_device	*pdev;
	struct regulator	*vin;
	struct power_supply	*usb_psy;
	int			vbus_det_irq;
	struct delayed_work     chg_work;
	int                     vbus;
	struct regulator	*vdd33;
	struct regulator	*vdd12;
	int			gpio_usbdetect;

	int			id;
	int			id_det_gpio;
	int			id_det_irq;

	bool			notify_host_mode;
	bool			disable_device_mode;

	int			dpdm_switch_gpio;
};

static int gpio_enable_ldos(struct gpio_usbdetect *usb, int on)
{
	struct platform_device *pdev = usb->pdev;
	int ret = 0;

	if (!on)
		goto disable_regulator;

	if (of_get_property(pdev->dev.of_node, "vin-supply", NULL)) {
		usb->vin = devm_regulator_get(&pdev->dev, "vin");
		if (IS_ERR(usb->vin)) {
			dev_err(&pdev->dev, "Failed to get VIN regulator: %ld\n",
				PTR_ERR(usb->vin));
			return PTR_ERR(usb->vin);
		}
	}

	if (of_get_property(pdev->dev.of_node, "vdd33-supply", NULL)) {
		usb->vdd33 = devm_regulator_get(&pdev->dev, "vdd33");
		if (IS_ERR(usb->vdd33)) {
			dev_err(&pdev->dev, "Failed to get vdd33 regulator: %ld\n",
				PTR_ERR(usb->vdd33));
			return PTR_ERR(usb->vdd33);
		}
	}

	if (of_get_property(pdev->dev.of_node, "vdd12-supply", NULL)) {
		usb->vdd12 = devm_regulator_get(&pdev->dev, "vdd12");
		if (IS_ERR(usb->vdd12)) {
			dev_err(&pdev->dev, "Failed to get vdd12 regulator: %ld\n",
				PTR_ERR(usb->vdd12));
			return PTR_ERR(usb->vdd12);
		}
	}

	if (usb->vin) {
		ret = regulator_enable(usb->vin);
		if (ret) {
			dev_err(&pdev->dev, "Failed to enable VIN regulator: %d\n",
									ret);
			return ret;
		}
	}

	if (usb->vdd33) {
		ret = regulator_set_optimum_mode(usb->vdd33, 500000);
		if (ret < 0) {
			dev_err(&pdev->dev, "unable to set load for vdd33\n");
			goto disable_vin;
		}

		ret = regulator_set_voltage(usb->vdd33, 3300000, 3300000);
		if (ret) {
			dev_err(&pdev->dev, "unable to set volt for vdd33\n");
			regulator_set_optimum_mode(usb->vdd33, 0);
			goto disable_vin;
		}
		ret = regulator_enable(usb->vdd33);
		if (ret) {
			dev_err(&pdev->dev, "unable to enable vdd33 regulator\n");
			regulator_set_voltage(usb->vdd33, 0, 3300000);
			regulator_set_optimum_mode(usb->vdd33, 0);
			goto disable_vin;
		}
		dev_dbg(&pdev->dev, "vdd33 successful\n");
	}

	if (usb->vdd12) {
		ret = regulator_set_optimum_mode(usb->vdd12, 500000);
		if (ret < 0) {
			dev_err(&pdev->dev, "unable to set load for vdd12\n");
			goto disable_3p3;
		}

		ret = regulator_set_voltage(usb->vdd12, 1220000, 1220000);
		if (ret) {
			dev_err(&pdev->dev, "unable to set volt for vddi12\n");
			regulator_set_optimum_mode(usb->vdd12, 0);
			goto disable_3p3;
		}
		ret = regulator_enable(usb->vdd12);
		if (ret) {
			dev_err(&pdev->dev, "unable to enable vdd12 regulator\n");
			regulator_set_voltage(usb->vdd12, 0, 1225000);
			regulator_set_optimum_mode(usb->vdd12, 0);
			goto disable_3p3;
		}
		dev_dbg(&pdev->dev, "vdd12 successful\n");
	}

	return ret;

disable_regulator:
	if (usb->vdd12) {
		regulator_disable(usb->vdd12);
		regulator_set_voltage(usb->vdd12, 0, 1225000);
		regulator_set_optimum_mode(usb->vdd12, 0);
	}

disable_3p3:
	if (usb->vdd33) {
		regulator_disable(usb->vdd33);
		regulator_set_voltage(usb->vdd33, 0, 3300000);
		regulator_set_optimum_mode(usb->vdd33, 0);
	}

disable_vin:
	if (usb->vin)
		regulator_disable(usb->vin);

	return ret;
}

static irqreturn_t gpio_usbdetect_irq(int irq, void *data)
{
	struct gpio_usbdetect *usb = data;

	if (gpio_is_valid(usb->id_det_gpio)) {
		usb->id = gpio_get_value(usb->id_det_gpio);
		if (usb->id) {
			dev_dbg(&usb->pdev->dev, "ID\n");
			usb->vbus = gpio_get_value(usb->gpio_usbdetect);
			goto queue_chg_work;
		}
		dev_dbg(&usb->pdev->dev, "!ID\n");
		schedule_delayed_work(&usb->chg_work, 0);
		return IRQ_HANDLED;
	}

	if (gpio_is_valid(usb->gpio_usbdetect))
		usb->vbus = gpio_get_value(usb->gpio_usbdetect);
	else
		usb->vbus = !!irq_read_line(irq);

queue_chg_work:
	pm_wakeup_event(&usb->pdev->dev, WAKEUP_SRC_TIMEOUT_MS);
	if (!usb->vbus)
		schedule_delayed_work(&usb->chg_work, 0);
	else
		schedule_delayed_work(&usb->chg_work,
					msecs_to_jiffies(VBUS_ON_DEBOUNCE_MS));

	return IRQ_HANDLED;
}

static int gpio_usbdetect_notify_usb_type(struct gpio_usbdetect *usb,
					enum power_supply_type type)
{
	int rc;
	union power_supply_propval pval = {0, };

	pval.intval = type;
	rc = usb->usb_psy->set_property(usb->usb_psy,
			POWER_SUPPLY_PROP_REAL_TYPE, &pval);
	if (rc < 0) {
		if (rc == -EINVAL) {
			rc = usb->usb_psy->set_property(usb->usb_psy,
					POWER_SUPPLY_PROP_TYPE, &pval);
			if (!rc)
				return 0;
		}
		pr_err("notify charger type to usb_psy failed, rc=%d\n", rc);
	}

	return rc;
}

static void gpio_usbdetect_chg_work(struct work_struct *w)
{
	struct gpio_usbdetect *usb = container_of(w, struct gpio_usbdetect,
							chg_work.work);

	if (gpio_is_valid(usb->id_det_gpio)) {
		dev_dbg(&usb->pdev->dev, "ID:%d VBUS:%d\n",
						usb->id, usb->vbus);
		if (!usb->id) {
			gpio_usbdetect_notify_usb_type(usb,
					POWER_SUPPLY_TYPE_UNKNOWN);
			power_supply_set_present(usb->usb_psy, 0);
			power_supply_set_usb_otg(usb->usb_psy, 1);
			if (gpio_is_valid(usb->dpdm_switch_gpio))
				gpio_set_value(usb->dpdm_switch_gpio, 1);

			return;
		}

		power_supply_set_usb_otg(usb->usb_psy, 0);
		if (gpio_is_valid(usb->dpdm_switch_gpio))
			gpio_set_value(usb->dpdm_switch_gpio, 0);

		if (usb->vbus) {
			gpio_usbdetect_notify_usb_type(usb,
						POWER_SUPPLY_TYPE_USB);
			power_supply_set_present(usb->usb_psy, usb->vbus);
		} else {
			gpio_usbdetect_notify_usb_type(usb,
					POWER_SUPPLY_TYPE_UNKNOWN);
			power_supply_set_present(usb->usb_psy, usb->vbus);
		}

		return;
	}

	if (usb->vbus) {
		if (usb->notify_host_mode)
			power_supply_set_usb_otg(usb->usb_psy, 0);

		if (!usb->disable_device_mode) {
			gpio_usbdetect_notify_usb_type(usb,
						POWER_SUPPLY_TYPE_USB);
			power_supply_set_present(usb->usb_psy, usb->vbus);
		}
	} else {
		/* notify gpio_state = LOW as disconnect */
		gpio_usbdetect_notify_usb_type(usb,
				POWER_SUPPLY_TYPE_UNKNOWN);
		power_supply_set_present(usb->usb_psy, usb->vbus);

		/* Cheeck if low gpio_state be treated as HOST mode */
		if (usb->notify_host_mode)
			power_supply_set_usb_otg(usb->usb_psy, 1);
	}
}

static int gpio_usbdetect_probe(struct platform_device *pdev)
{
	struct gpio_usbdetect *usb;
	struct power_supply *usb_psy;
	int rc;
	unsigned long flags;

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
	INIT_DELAYED_WORK(&usb->chg_work, gpio_usbdetect_chg_work);
	usb->notify_host_mode = of_property_read_bool(pdev->dev.of_node,
					"qcom,notify-host-mode");
	usb->disable_device_mode = of_property_read_bool(pdev->dev.of_node,
					"qcom,disable-device-mode");
	rc = gpio_enable_ldos(usb, 1);
	if (rc)
		return rc;

	usb->gpio_usbdetect = of_get_named_gpio(pdev->dev.of_node,
					"qcom,gpio-mode-sel", 0);

	if (gpio_is_valid(usb->gpio_usbdetect)) {
		rc = devm_gpio_request(&pdev->dev, usb->gpio_usbdetect,
					"GPIO_MODE_SEL");
		if (rc) {
			dev_err(&pdev->dev, "gpio req failed for gpio_%d\n",
						 usb->gpio_usbdetect);
			goto disable_ldo;
		}
		rc = gpio_direction_input(usb->gpio_usbdetect);
		if (rc) {
			dev_err(&pdev->dev, "Invalid input from GPIO_%d\n",
					usb->gpio_usbdetect);
			goto disable_ldo;
		}
	}

	usb->vbus_det_irq = platform_get_irq_byname(pdev, "vbus_det_irq");
	if (usb->vbus_det_irq < 0) {
		dev_err(&pdev->dev, "vbus_det_irq failed\n");
		rc = usb->vbus_det_irq;
		goto disable_ldo;
	}

	rc = devm_request_irq(&pdev->dev, usb->vbus_det_irq,
			      gpio_usbdetect_irq,
			      IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING,
			      "vbus_det_irq", usb);
	if (rc) {
		dev_err(&pdev->dev, "request for vbus_det_irq failed: %d\n",
			rc);
		goto disable_ldo;
	}

	usb->id_det_gpio = of_get_named_gpio(pdev->dev.of_node,
						"qcom,id-det-gpio", 0);
	if (gpio_is_valid(usb->id_det_gpio)) {
		rc = devm_gpio_request(&pdev->dev, usb->id_det_gpio,
					"GPIO_ID_DET");
		if (rc) {
			dev_err(&pdev->dev, "gpio req failed for gpio_%d\n",
							usb->id_det_gpio);
			goto disable_ldo;
		}

		usb->id_det_irq = gpio_to_irq(usb->id_det_gpio);
		if (usb->id_det_irq < 0) {
			dev_err(&pdev->dev, "get id_det_irq failed\n");
			goto disable_ldo;
		}
		rc = devm_request_irq(&pdev->dev, usb->id_det_irq,
					gpio_usbdetect_irq,
					IRQF_TRIGGER_RISING |
					IRQF_TRIGGER_FALLING, "id_det_irq",
					usb);
		if (rc) {
			dev_err(&pdev->dev, "request for id_det_irq failed:%d\n",
					rc);
			goto disable_ldo;
		}
	}

	usb->dpdm_switch_gpio = of_get_named_gpio(pdev->dev.of_node,
						"qcom,dpdm_switch_gpio", 0);
	dev_dbg(&pdev->dev, "is dpdm_switch_gpio valid:%d\n",
					gpio_is_valid(usb->dpdm_switch_gpio));

	device_init_wakeup(&pdev->dev, 1);

	enable_irq_wake(usb->vbus_det_irq);
	enable_irq_wake(usb->id_det_irq);
	dev_set_drvdata(&pdev->dev, usb);

	/* Read and report initial VBUS state */
	local_irq_save(flags);
	gpio_usbdetect_irq(usb->vbus_det_irq, usb);
	local_irq_restore(flags);

	return 0;

disable_ldo:
	gpio_enable_ldos(usb, 0);

	return rc;
}

static int gpio_usbdetect_remove(struct platform_device *pdev)
{
	struct gpio_usbdetect *usb = dev_get_drvdata(&pdev->dev);

	device_wakeup_disable(&usb->pdev->dev);
	disable_irq_wake(usb->vbus_det_irq);
	disable_irq(usb->vbus_det_irq);

	gpio_enable_ldos(usb, 0);
	cancel_delayed_work_sync(&usb->chg_work);

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
