/*
 * Power button driver for dollar cove
 *
 * Copyright (C) 2014 Intel Corp
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/input.h>
#include <linux/io.h>
#include <linux/mfd/intel_soc_pmic.h>

#define DC_TI_IRQ_MASK_REG	0x02
#define IRQ_MASK_PWRBTN		(1 << 0)

#define DC_TI_SIRQ_REG		0x3
#define SIRQ_PWRBTN_REL		(1 << 0)

#define DRIVER_NAME "dollar_cove_ti_power_button"

static struct input_dev *pb_input;
static int pwrbtn_irq;
static int force_trigger;

static irqreturn_t pb_isr(int irq, void *dev_id)
{
	int ret;
	int state;

	ret = intel_soc_pmic_readb(DC_TI_SIRQ_REG);
	if (ret < 0) {
		pr_err("[%s] power button SIRQ REG read fail %d\n",
						pb_input->name, ret);
		return IRQ_NONE;
	}

	state = ret & SIRQ_PWRBTN_REL;

	if (force_trigger && state) {
		/* If we lost the press interrupt when short pressing
		 * power button to wake up board from S3, simulate one.
		 */
		input_event(pb_input, EV_KEY, KEY_POWER, 1);
		input_sync(pb_input);
		input_event(pb_input, EV_KEY, KEY_POWER, 0);
		input_sync(pb_input);
	} else {
		input_event(pb_input, EV_KEY, KEY_POWER, !state);
		input_sync(pb_input);
		pr_info("[%s] power button %s\n", pb_input->name,
			state ? "released" : "pressed");
	}

	if (force_trigger)
		force_trigger = 0;

	return IRQ_HANDLED;
}

static int pb_probe(struct platform_device *pdev)
{
	int ret;

	pwrbtn_irq = platform_get_irq(pdev, 0);
	if (pwrbtn_irq < 0) {
		dev_err(&pdev->dev,
			"get irq fail: irq1:%d\n", pwrbtn_irq);
		return -EINVAL;
	}
	pb_input = input_allocate_device();
	if (!pb_input) {
		dev_err(&pdev->dev, "failed to allocate input device\n");
		return -ENOMEM;
	}
	pb_input->name = pdev->name;
	pb_input->phys = "power-button/input0";
	pb_input->id.bustype = BUS_HOST;
	pb_input->dev.parent = &pdev->dev;
	input_set_capability(pb_input, EV_KEY, KEY_POWER);
	ret = input_register_device(pb_input);
	if (ret) {
		dev_err(&pdev->dev,
			"failed to register input device:%d\n", ret);
		input_free_device(pb_input);
		return ret;
	}

	ret = request_threaded_irq(pwrbtn_irq, NULL, pb_isr,
		IRQF_NO_SUSPEND, DRIVER_NAME, pdev);
	if (ret) {
		dev_err(&pdev->dev,
			"[request irq fail0]irq:%d err:%d\n", pwrbtn_irq, ret);
		input_unregister_device(pb_input);
		return ret;
	}

	return 0;
}

static int pb_remove(struct platform_device *pdev)
{
	free_irq(pwrbtn_irq, NULL);
	input_unregister_device(pb_input);
	return 0;
}

static int pb_resume_noirq(struct device *dev)
{
	force_trigger = 1;
	return 0;
}

static const struct dev_pm_ops pb_pm_ops = {
	.resume_noirq	= pb_resume_noirq,
};

static struct platform_driver pb_driver = {
	.driver = {
		.name = DRIVER_NAME,
		.owner = THIS_MODULE,
		.pm = &pb_pm_ops,
	},
	.probe	= pb_probe,
	.remove	= pb_remove,
};

static int __init pb_module_init(void)
{
	return platform_driver_register(&pb_driver);
}

static void  pb_module_exit(void)
{
	platform_driver_unregister(&pb_driver);
}

late_initcall(pb_module_init);

module_exit(pb_module_exit);

MODULE_AUTHOR("Ramakrishna Pallala <ramakrishna.pallala@intel.com>");
MODULE_DESCRIPTION("Dollar Cove(TI) Power Button Driver");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:" DRIVER_NAME);
