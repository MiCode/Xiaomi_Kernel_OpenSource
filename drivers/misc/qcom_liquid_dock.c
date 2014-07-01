/* Copyright (c) 2013-2014, The Linux Foundation. All rights reserved.
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
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/interrupt.h>
#include <linux/notifier.h>
#include <linux/platform_device.h>
#include <linux/pm_wakeup.h>
#include <linux/workqueue.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/of_gpio.h>
#include <soc/qcom/liquid_dock.h>

static int docked;
static BLOCKING_NOTIFIER_HEAD(dock_notifier_list);

/**
 * register_liquid_dock_notify - register dock notifier callback
 * @nb: pointer to the notifier block for the callback events.
 *
 * Calls the notifier callback to when dock is inserted or removed, indicated
 * by a boolean passed to the callback's action parameter.
 */
void register_liquid_dock_notify(struct notifier_block *nb)
{
	if (!nb)
		return;

	/* inform new client of current state */
	if (nb->notifier_call)
		nb->notifier_call(nb, docked, NULL);

	blocking_notifier_chain_register(&dock_notifier_list, nb);
}
EXPORT_SYMBOL(register_liquid_dock_notify);

/**
 * unregister_liquid_dock_notify - unregister a notifier callback
 * @nb: pointer to the notifier block for the callback events.
 *
 * register_liquid_dock_notify() must have been previously called for this
 * function to work properly.
 */
void unregister_liquid_dock_notify(struct notifier_block *nb)
{
	blocking_notifier_chain_unregister(&dock_notifier_list, nb);
}
EXPORT_SYMBOL(unregister_liquid_dock_notify);

struct liquid_dock {
	struct device		*dev;
	struct work_struct	dock_work;
	int			dock_detect;
	int			dock_hub_reset;
	int			dock_eth_reset;
	int			dock_enable;
	struct platform_device	*usb3_pdev;
};

static void dock_detected_work(struct work_struct *w)
{
	struct liquid_dock *dock = container_of(w, struct liquid_dock,
						 dock_work);
	docked = gpio_get_value(dock->dock_detect);

	if (dock->dock_enable)
		gpio_direction_output(dock->dock_enable, 0);

	if (docked) {
		/* assert RESETs before turning on power */
		gpio_direction_output(dock->dock_hub_reset, 1);
		gpio_direction_output(dock->dock_eth_reset, 1);

		if (device_attach(&dock->usb3_pdev->dev) != 1) {
			dev_err(dock->dev, "Could not add USB driver 0x%p\n",
				dock->usb3_pdev);
			return;
		}

		if (dock->dock_enable)
			gpio_direction_output(dock->dock_enable, 1);

		msleep(20); /* short delay before de-asserting RESETs */
		gpio_direction_output(dock->dock_hub_reset, 0);
		gpio_direction_output(dock->dock_eth_reset, 0);
	} else {
		device_release_driver(&dock->usb3_pdev->dev);
	}

	blocking_notifier_call_chain(&dock_notifier_list, docked, NULL);

	/* Allow system suspend */
	pm_relax(dock->dev);
}

static irqreturn_t dock_detected(int irq, void *data)
{
	struct liquid_dock *dock = data;

	/* Ensure suspend can't happen until after work function commpletes */
	pm_stay_awake(dock->dev);
	schedule_work(&dock->dock_work);
	return IRQ_HANDLED;
}

static int liquid_dock_probe(struct platform_device *pdev)
{
	struct device_node *node = pdev->dev.of_node;
	struct device_node *usb3_node;
	struct liquid_dock *dock;
	int ret;

	dock = devm_kzalloc(&pdev->dev, sizeof(*dock), GFP_KERNEL);
	if (!dock)
		return -ENOMEM;

	dock->dev = &pdev->dev;
	platform_set_drvdata(pdev, dock);
	INIT_WORK(&dock->dock_work, dock_detected_work);

	dock->dock_detect = of_get_named_gpio(node, "qcom,dock-detect-gpio", 0);
	if (dock->dock_detect < 0) {
		dev_err(dock->dev, "unable to get dock-detect-gpio\n");
		return dock->dock_detect;
	}

	ret = devm_gpio_request(dock->dev, dock->dock_detect, "dock_detect");
	if (ret)
		return ret;

	ret = devm_request_irq(&pdev->dev, gpio_to_irq(dock->dock_detect),
				dock_detected, IRQF_TRIGGER_RISING |
				IRQF_TRIGGER_FALLING | IRQF_SHARED,
				"dock_detect_irq", dock);
	if (ret)
		return ret;

	dock->dock_hub_reset = of_get_named_gpio(node,
						 "qcom,dock-hub-reset-gpio", 0);
	if (dock->dock_hub_reset < 0) {
		dev_err(dock->dev, "unable to get dock-hub-reset-gpio\n");
		return dock->dock_hub_reset;
	}

	ret = devm_gpio_request(dock->dev, dock->dock_hub_reset,
				"dock_hub_reset");
	if (ret)
		return ret;

	dock->dock_eth_reset = of_get_named_gpio(node,
						 "qcom,dock-eth-reset-gpio", 0);
	if (dock->dock_eth_reset < 0) {
		dev_err(dock->dev, "unable to get dock-eth-reset-gpio\n");
		return dock->dock_eth_reset;
	}

	ret = devm_gpio_request(dock->dev, dock->dock_eth_reset,
				"dock_eth_reset");
	if (ret)
		return ret;

	dock->dock_enable = of_get_named_gpio(node, "qcom,dock-enable-gpio", 0);
	if (dock->dock_enable < 0) { /* optional */
		dock->dock_enable = 0;
	} else {
		ret = devm_gpio_request(dock->dev, dock->dock_enable,
					"dock_enable");
		if (ret)
			return ret;
	}

	usb3_node = of_parse_phandle(node, "qcom,usb-host", 0);
	if (!usb3_node) {
		dev_err(dock->dev, "unable to get usb-host\n");
		return -ENODEV;
	}

	dock->usb3_pdev = of_find_device_by_node(usb3_node);
	if (!dock->usb3_pdev || !dock->usb3_pdev->dev.driver) {
		dev_dbg(dock->dev, "usb pdev not ready\n");
		of_node_put(usb3_node);
		return -EPROBE_DEFER;
	}

	of_node_put(usb3_node);
	schedule_work(&dock->dock_work);
	device_init_wakeup(dock->dev, true);
	enable_irq_wake(gpio_to_irq(dock->dock_detect));

	return 0;
}

static int liquid_dock_remove(struct platform_device *pdev)
{
	struct liquid_dock *dock = platform_get_drvdata(pdev);

	disable_irq_wake(gpio_to_irq(dock->dock_detect));
	cancel_work_sync(&dock->dock_work);
	platform_device_put(dock->usb3_pdev);

	return 0;
}

static struct of_device_id of_match_table[] = {
	{ .compatible = "qcom,liquid-dock", },
	{ .compatible = "qcom,apq8084-dock", },
	{ },
};

static struct platform_driver liquid_dock_driver = {
	.driver         = {
		.name   = "liquid-dock-driver",
		.of_match_table = of_match_table,
	},
	.probe          = liquid_dock_probe,
	.remove		= liquid_dock_remove,
};

module_platform_driver(liquid_dock_driver);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("QTI LiQUID Docking Station driver");
