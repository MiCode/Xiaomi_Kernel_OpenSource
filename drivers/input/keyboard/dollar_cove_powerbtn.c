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
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA.
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/input.h>
#include <linux/io.h>

#define DRIVER_NAME "dollar_cove_power_button"

static struct input_dev *pb_input;
static int irq_press, irq_release;

static irqreturn_t pb_isr(int irq, void *dev_id)
{
	input_event(pb_input, EV_KEY, KEY_POWER, irq == irq_press);
	input_sync(pb_input);
	pr_info("[%s] power button %s\n", pb_input->name,
			irq == irq_press ? "pressed" : "released");
	return IRQ_HANDLED;
}

static int pb_probe(struct platform_device *pdev)
{
	int ret;

	irq_press = platform_get_irq(pdev, 0);
	irq_release = platform_get_irq(pdev, 1);
	if (irq_press < 0 || irq_release < 0) {
		dev_err(&pdev->dev,
			"get irq fail: irq1:%d irq2:%d\n",
					irq_press, irq_release);
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
		return ret;
	}

	ret = request_threaded_irq(irq_press, NULL, pb_isr,
		IRQF_NO_SUSPEND, DRIVER_NAME, pdev);
	if (ret) {
		dev_err(&pdev->dev,
			"[request irq fail0]irq:%d err:%d\n", irq_press, ret);
		input_unregister_device(pb_input);
		return ret;
	}

	ret = request_threaded_irq(irq_release, NULL, pb_isr,
		IRQF_NO_SUSPEND, DRIVER_NAME, pdev);
	if (ret) {
		dev_err(&pdev->dev,
			"[request irq fail1]irq:%d err:%d\n", irq_release, ret);
		free_irq(irq_press, NULL);
		input_unregister_device(pb_input);
		return ret;
	}

	return 0;
}

static int pb_remove(struct platform_device *pdev)
{
	free_irq(irq_press, NULL);
	free_irq(irq_release, NULL);
	input_unregister_device(pb_input);
	return 0;
}

static struct platform_driver pb_driver = {
	.driver = {
		.name = DRIVER_NAME,
		.owner = THIS_MODULE,
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

MODULE_AUTHOR("Yang, Bin <bin.yang@intel.com>");
MODULE_DESCRIPTION("Dollar Cove Power Button Driver");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:" DRIVER_NAME);
