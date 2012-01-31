/* Copyright (c) 2012, Code Aurora Forum. All rights reserved.
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
#include <linux/platform_device.h>
#include <linux/irq.h>
#include <media/rc-core.h>
#include <media/gpio-ir-recv.h>

#define TSOP_DRIVER_NAME	"gpio-rc-recv"
#define TSOP_DEVICE_NAME	"gpio_ir_recv"

struct gpio_rc_dev {
	struct rc_dev *rcdev;
	struct mutex lock;
	unsigned int gpio_nr;
	bool active_low;
	bool can_wakeup;
	struct work_struct work;
};

static void ir_decoder_work(struct work_struct *work)
{
	struct gpio_rc_dev *gpio_dev = container_of(work,
			struct gpio_rc_dev, work);
	unsigned int gval;
	int rc = 0;
	enum raw_event_type type = IR_SPACE;

	mutex_lock(&gpio_dev->lock);
	gval = gpio_get_value_cansleep(gpio_dev->gpio_nr);

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

err_get_value:
	mutex_unlock(&gpio_dev->lock);
}

static irqreturn_t gpio_ir_recv_irq_handler(int irq, void *data)
{
	struct gpio_rc_dev *gpio_dev = data;

	schedule_work(&gpio_dev->work);

	return IRQ_HANDLED;
}

static int __devinit gpio_ir_recv_probe(struct platform_device *pdev)
{
	struct gpio_rc_dev *gpio_dev;
	struct rc_dev *rcdev;
	const struct gpio_ir_recv_platform_data *pdata =
					pdev->dev.platform_data;
	int rc = 0;

	if (!pdata)
		return -EINVAL;

	if (pdata->gpio_nr < 0)
		return -EINVAL;

	gpio_dev = kzalloc(sizeof(struct gpio_rc_dev), GFP_KERNEL);
	if (!gpio_dev)
		return -ENOMEM;

	mutex_init(&gpio_dev->lock);

	rcdev = rc_allocate_device();
	if (!rcdev) {
		rc = -ENOMEM;
		goto err_allocate_device;
	}

	rcdev->driver_type = RC_DRIVER_IR_RAW;
	rcdev->allowed_protos = RC_TYPE_NEC;
	rcdev->input_name = TSOP_DEVICE_NAME;
	rcdev->input_id.bustype = BUS_HOST;
	rcdev->driver_name = TSOP_DRIVER_NAME;
	rcdev->map_name = RC_MAP_SAMSUNG_NECX;

	gpio_dev->rcdev = rcdev;
	gpio_dev->gpio_nr = pdata->gpio_nr;
	gpio_dev->active_low = pdata->active_low;
	gpio_dev->can_wakeup = pdata->can_wakeup;

	INIT_WORK(&gpio_dev->work, ir_decoder_work);

	rc = gpio_request(pdata->gpio_nr, "gpio-ir-recv");
	if (rc < 0)
		goto err_gpio_request;
	rc  = gpio_direction_input(pdata->gpio_nr);
	if (rc < 0)
		goto err_gpio_direction_input;

	rc = rc_register_device(rcdev);
	if (rc < 0) {
		dev_err(&pdev->dev, "failed to register rc device\n");
		goto err_register_rc_device;
	}

	platform_set_drvdata(pdev, gpio_dev);

	rc = request_irq(gpio_to_irq(pdata->gpio_nr), gpio_ir_recv_irq_handler,
			IRQF_TRIGGER_FALLING | IRQF_TRIGGER_RISING,
			"gpio-ir-recv-irq", gpio_dev);
	if (rc < 0)
		goto err_request_irq;

	if (pdata->can_wakeup == true) {
		rc = enable_irq_wake(gpio_to_irq(pdata->gpio_nr));
		if (rc < 0)
			goto err_enable_irq_wake;
	}

	return 0;

err_enable_irq_wake:
	free_irq(gpio_to_irq(gpio_dev->gpio_nr), gpio_dev);
err_request_irq:
	platform_set_drvdata(pdev, NULL);
	rc_unregister_device(rcdev);
err_register_rc_device:
err_gpio_direction_input:
	gpio_free(pdata->gpio_nr);
err_gpio_request:
	rc_free_device(rcdev);
	rcdev = NULL;
err_allocate_device:
	mutex_destroy(&gpio_dev->lock);
	kfree(gpio_dev);
	return rc;
}

static int __devexit gpio_ir_recv_remove(struct platform_device *pdev)
{
	struct gpio_rc_dev *gpio_dev = platform_get_drvdata(pdev);

	flush_work_sync(&gpio_dev->work);
	disable_irq_wake(gpio_to_irq(gpio_dev->gpio_nr));
	free_irq(gpio_to_irq(gpio_dev->gpio_nr), gpio_dev);
	platform_set_drvdata(pdev, NULL);
	rc_unregister_device(gpio_dev->rcdev);
	gpio_free(gpio_dev->gpio_nr);
	rc_free_device(gpio_dev->rcdev);
	mutex_destroy(&gpio_dev->lock);
	kfree(gpio_dev);
	return 0;
}

static struct platform_driver gpio_ir_recv_driver = {
	.probe  = gpio_ir_recv_probe,
	.remove = __devexit_p(gpio_ir_recv_remove),
	.driver = {
		.name   = TSOP_DRIVER_NAME,
		.owner  = THIS_MODULE,
	},
};

static int __init gpio_ir_recv_init(void)
{
	return platform_driver_register(&gpio_ir_recv_driver);
}
module_init(gpio_ir_recv_init);

static void __exit gpio_ir_recv_exit(void)
{
	platform_driver_unregister(&gpio_ir_recv_driver);
}
module_exit(gpio_ir_recv_exit);

MODULE_DESCRIPTION("GPIO IR Receiver driver");
MODULE_LICENSE("GPL v2");
