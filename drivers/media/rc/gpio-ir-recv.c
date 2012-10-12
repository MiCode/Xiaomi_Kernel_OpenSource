/* Copyright (c) 2012, The Linux Foundation. All rights reserved.
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

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/gpio.h>
#include <linux/slab.h>
#include <linux/of_gpio.h>
#include <linux/platform_device.h>
#include <linux/irq.h>
#include <linux/pm_qos.h>
#include <linux/timer.h>
#include <media/rc-core.h>
#include <media/gpio-ir-recv.h>

#define GPIO_IR_DRIVER_NAME	"gpio-rc-recv"
#define GPIO_IR_DEVICE_NAME	"gpio_ir_recv"

static int gpio_ir_timeout = 200;
module_param_named(gpio_ir_timeout, gpio_ir_timeout, int, 0664);

static int __init gpio_ir_timeout_setup(char *p)
{
	gpio_ir_timeout = memparse(p, NULL);
	return 0;
}

early_param("gpio_ir_timeout", gpio_ir_timeout_setup);

struct gpio_rc_dev {
	struct rc_dev *rcdev;
	struct pm_qos_request pm_qos_req;
	struct timer_list gpio_ir_timer;
	int gpio_nr;
	bool active_low;
	int can_sleep;
	bool can_wakeup;
	bool pm_qos_vote;
	int gpio_irq_latency;
};

#ifdef CONFIG_OF
/*
 * Translate OpenFirmware node properties into platform_data
 */
static int gpio_ir_recv_get_devtree_pdata(struct device *dev,
				  struct gpio_ir_recv_platform_data *pdata)
{
	struct device_node *np = dev->of_node;
	enum of_gpio_flags flags;
	int gpio;

	gpio = of_get_gpio_flags(np, 0, &flags);
	if (gpio < 0) {
		if (gpio != -EPROBE_DEFER)
			dev_err(dev, "Failed to get gpio flags (%d)\n", gpio);
		return gpio;
	}

	pdata->gpio_nr = gpio;
	pdata->active_low = (flags & OF_GPIO_ACTIVE_LOW);
	/* probe() takes care of map_name == NULL or allowed_protos == 0 */
	pdata->map_name = of_get_property(np, "linux,rc-map-name", NULL);
	pdata->allowed_protos = 0;

	return 0;
}

static struct of_device_id gpio_ir_recv_of_match[] = {
	{ .compatible = "gpio-ir-receiver", },
	{ },
};
MODULE_DEVICE_TABLE(of, gpio_ir_recv_of_match);

#else /* !CONFIG_OF */

#define gpio_ir_recv_get_devtree_pdata(dev, pdata)	(-ENOSYS)

#endif

static irqreturn_t gpio_ir_recv_irq(int irq, void *dev_id)
{
	struct gpio_rc_dev *gpio_dev = dev_id;
	int gval;
	int rc = 0;
	enum raw_event_type type = IR_SPACE;

	if (!gpio_dev->pm_qos_vote && gpio_dev->can_wakeup) {
		gpio_dev->pm_qos_vote = 1;
		pm_qos_update_request(&gpio_dev->pm_qos_req,
					 gpio_dev->gpio_irq_latency);
	}

	if (gpio_dev->can_sleep)
		gval = gpio_get_value_cansleep(gpio_dev->gpio_nr);
	else
		gval = gpio_get_value(gpio_dev->gpio_nr);

	if (gval < 0)
		goto err_get_value;

	if (gpio_dev->active_low)
		gval = !gval;

	if (gval == 1)
		type = IR_PULSE;

	rc = ir_raw_event_store_edge(gpio_dev->rcdev, type);
	if (rc < 0)
		goto err_get_value;

	ir_raw_event_handle(gpio_dev->rcdev);

	if (gpio_dev->can_wakeup)
		mod_timer(&gpio_dev->gpio_ir_timer,
				jiffies + msecs_to_jiffies(gpio_ir_timeout));
err_get_value:
	return IRQ_HANDLED;
}

static void gpio_ir_timer(unsigned long data)
{
	struct gpio_rc_dev *gpio_dev = (struct gpio_rc_dev *)data;

	pm_qos_update_request(&gpio_dev->pm_qos_req, PM_QOS_DEFAULT_VALUE);
	pm_qos_request_active(&gpio_dev->pm_qos_req);
	gpio_dev->pm_qos_vote = 0;
}

static int gpio_ir_recv_probe(struct platform_device *pdev)
{
	struct gpio_rc_dev *gpio_dev;
	struct rc_dev *rcdev;
	const struct gpio_ir_recv_platform_data *pdata =
					pdev->dev.platform_data;
	int rc;

	if (pdev->dev.of_node) {
		struct gpio_ir_recv_platform_data *dtpdata =
			devm_kzalloc(&pdev->dev, sizeof(*dtpdata), GFP_KERNEL);
		if (!dtpdata)
			return -ENOMEM;
		rc = gpio_ir_recv_get_devtree_pdata(&pdev->dev, dtpdata);
		if (rc)
			return rc;
		pdata = dtpdata;
	}

	if (!pdata)
		return -EINVAL;

	if (pdata->gpio_nr < 0)
		return -EINVAL;

	gpio_dev = kzalloc(sizeof(struct gpio_rc_dev), GFP_KERNEL);
	if (!gpio_dev)
		return -ENOMEM;

	rcdev = rc_allocate_device();
	if (!rcdev) {
		rc = -ENOMEM;
		goto err_allocate_device;
	}

	rcdev->priv = gpio_dev;
	rcdev->driver_type = RC_DRIVER_IR_RAW;
	rcdev->input_name = GPIO_IR_DEVICE_NAME;
	rcdev->input_phys = GPIO_IR_DEVICE_NAME "/input0";
	rcdev->input_id.bustype = BUS_HOST;
	rcdev->input_id.vendor = 0x0001;
	rcdev->input_id.product = 0x0001;
	rcdev->input_id.version = 0x0100;
	rcdev->dev.parent = &pdev->dev;
	rcdev->driver_name = GPIO_IR_DRIVER_NAME;
	if (pdata->allowed_protos)
		rcdev->allowed_protos = pdata->allowed_protos;
	else
		rcdev->allowed_protos = RC_BIT_ALL;
	rcdev->map_name = pdata->map_name ?: RC_MAP_EMPTY;

	gpio_dev->rcdev = rcdev;
	gpio_dev->gpio_nr = pdata->gpio_nr;
	gpio_dev->active_low = pdata->active_low;
	gpio_dev->can_wakeup = pdata->can_wakeup;
	gpio_dev->gpio_irq_latency = pdata->swfi_latency + 1;
	gpio_dev->pm_qos_vote = 0;

	rc = gpio_request(pdata->gpio_nr, "gpio-ir-recv");
	if (rc < 0)
		goto err_gpio_request;

	gpio_dev->can_sleep = gpio_cansleep(pdata->gpio_nr);

	rc  = gpio_direction_input(pdata->gpio_nr);
	if (rc < 0)
		goto err_gpio_direction_input;

	rc = rc_register_device(rcdev);
	if (rc < 0) {
		dev_err(&pdev->dev, "failed to register rc device\n");
		goto err_register_rc_device;
	}

	platform_set_drvdata(pdev, gpio_dev);

	rc = request_any_context_irq(gpio_to_irq(pdata->gpio_nr),
				gpio_ir_recv_irq,
			IRQF_TRIGGER_FALLING | IRQF_TRIGGER_RISING,
					"gpio-ir-recv-irq", gpio_dev);
	if (rc < 0)
		goto err_request_irq;

	if (gpio_dev->can_wakeup) {
		pm_qos_add_request(&gpio_dev->pm_qos_req,
					PM_QOS_CPU_DMA_LATENCY,
					PM_QOS_DEFAULT_VALUE);
		device_init_wakeup(&pdev->dev, pdata->can_wakeup);
		setup_timer(&gpio_dev->gpio_ir_timer, gpio_ir_timer,
						(unsigned long)gpio_dev);
	}

	return 0;

err_request_irq:
	platform_set_drvdata(pdev, NULL);
	rc_unregister_device(rcdev);
	rcdev = NULL;
err_register_rc_device:
err_gpio_direction_input:
	gpio_free(pdata->gpio_nr);
err_gpio_request:
	rc_free_device(rcdev);
err_allocate_device:
	kfree(gpio_dev);
	return rc;
}

static int gpio_ir_recv_remove(struct platform_device *pdev)
{
	struct gpio_rc_dev *gpio_dev = platform_get_drvdata(pdev);

	if (gpio_dev->can_wakeup) {
		del_timer_sync(&gpio_dev->gpio_ir_timer);
		pm_qos_remove_request(&gpio_dev->pm_qos_req);
	}
	free_irq(gpio_to_irq(gpio_dev->gpio_nr), gpio_dev);
	platform_set_drvdata(pdev, NULL);
	rc_unregister_device(gpio_dev->rcdev);
	gpio_free(gpio_dev->gpio_nr);
	kfree(gpio_dev);
	return 0;
}

#ifdef CONFIG_PM
static int gpio_ir_recv_suspend(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct gpio_rc_dev *gpio_dev = platform_get_drvdata(pdev);

	if (device_may_wakeup(dev))
		enable_irq_wake(gpio_to_irq(gpio_dev->gpio_nr));
	else
		disable_irq(gpio_to_irq(gpio_dev->gpio_nr));

	return 0;
}

static int gpio_ir_recv_resume(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct gpio_rc_dev *gpio_dev = platform_get_drvdata(pdev);

	if (device_may_wakeup(dev))
		disable_irq_wake(gpio_to_irq(gpio_dev->gpio_nr));
	else
		enable_irq(gpio_to_irq(gpio_dev->gpio_nr));

	return 0;
}

static const struct dev_pm_ops gpio_ir_recv_pm_ops = {
	.suspend        = gpio_ir_recv_suspend,
	.resume         = gpio_ir_recv_resume,
};
#endif

static struct platform_driver gpio_ir_recv_driver = {
	.probe  = gpio_ir_recv_probe,
	.remove = gpio_ir_recv_remove,
	.driver = {
		.name   = GPIO_IR_DRIVER_NAME,
		.owner  = THIS_MODULE,
		.of_match_table = of_match_ptr(gpio_ir_recv_of_match),
#ifdef CONFIG_PM
		.pm	= &gpio_ir_recv_pm_ops,
#endif
	},
};
module_platform_driver(gpio_ir_recv_driver);

MODULE_DESCRIPTION("GPIO IR Receiver driver");
MODULE_LICENSE("GPL v2");
