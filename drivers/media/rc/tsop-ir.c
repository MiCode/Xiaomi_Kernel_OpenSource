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
#include <media/tsop-ir.h>

#define TSOP_DRIVER_NAME	"tsop-rc"
#define TSOP_DEVICE_NAME	"tsop_ir"

struct tsop_remote {
	struct rc_dev *rcdev;
	struct mutex lock;
	unsigned int gpio_nr;
	bool active_low;
	bool can_wakeup;
	struct work_struct work;
};

static void ir_decoder_work(struct work_struct *work)
{
	struct tsop_remote *tsop_dev = container_of(work,
			struct tsop_remote, work);
	unsigned int gval;
	int rc = 0;
	enum raw_event_type type = IR_SPACE;

	mutex_lock(&tsop_dev->lock);
	gval = gpio_get_value_cansleep(tsop_dev->gpio_nr);

	if (gval < 0)
		goto err_get_value;

	if (tsop_dev->active_low)
		gval = !gval;

	if (gval == 1)
		type = IR_PULSE;

	rc = ir_raw_event_store_edge(tsop_dev->rcdev, type);
	if (rc < 0)
		goto err_get_value;

	ir_raw_event_handle(tsop_dev->rcdev);

err_get_value:
	mutex_unlock(&tsop_dev->lock);
}

static irqreturn_t tsop_irq_handler(int irq, void *data)
{
	struct tsop_remote *tsop_dev = data;

	schedule_work(&tsop_dev->work);

	return IRQ_HANDLED;
}

static int __devinit tsop_driver_probe(struct platform_device *pdev)
{
	struct tsop_remote *tsop_dev;
	struct rc_dev *rcdev;
	const struct tsop_platform_data *pdata = pdev->dev.platform_data;
	int rc = 0;

	if (!pdata)
		return -EINVAL;

	if (pdata->gpio_nr < 0)
		return -EINVAL;

	tsop_dev = kzalloc(sizeof(struct tsop_remote), GFP_KERNEL);
	if (!tsop_dev)
		return -ENOMEM;

	mutex_init(&tsop_dev->lock);

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

	tsop_dev->rcdev = rcdev;
	tsop_dev->gpio_nr = pdata->gpio_nr;
	tsop_dev->active_low = pdata->active_low;
	tsop_dev->can_wakeup = pdata->can_wakeup;

	INIT_WORK(&tsop_dev->work, ir_decoder_work);

	rc = gpio_request(pdata->gpio_nr, "tsop-ir");
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

	platform_set_drvdata(pdev, tsop_dev);

	rc = request_irq(gpio_to_irq(pdata->gpio_nr), tsop_irq_handler,
			IRQF_TRIGGER_FALLING | IRQF_TRIGGER_RISING,
			"tsop-irq", tsop_dev);
	if (rc < 0)
		goto err_request_irq;

	if (pdata->can_wakeup == true) {
		rc = enable_irq_wake(gpio_to_irq(pdata->gpio_nr));
		if (rc < 0)
			goto err_enable_irq_wake;
	}

	return 0;

err_enable_irq_wake:
	free_irq(gpio_to_irq(tsop_dev->gpio_nr), tsop_dev);
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
	mutex_destroy(&tsop_dev->lock);
	kfree(tsop_dev);
	return rc;
}

static int __devexit tsop_driver_remove(struct platform_device *pdev)
{
	struct tsop_remote *tsop_dev = platform_get_drvdata(pdev);

	flush_work_sync(&tsop_dev->work);
	disable_irq_wake(gpio_to_irq(tsop_dev->gpio_nr));
	free_irq(gpio_to_irq(tsop_dev->gpio_nr), tsop_dev);
	platform_set_drvdata(pdev, NULL);
	rc_unregister_device(tsop_dev->rcdev);
	gpio_free(tsop_dev->gpio_nr);
	rc_free_device(tsop_dev->rcdev);
	mutex_destroy(&tsop_dev->lock);
	kfree(tsop_dev);
	return 0;
}

static struct platform_driver tsop_driver = {
	.probe  = tsop_driver_probe,
	.remove = __devexit_p(tsop_driver_remove),
	.driver = {
		.name   = TSOP_DRIVER_NAME,
		.owner  = THIS_MODULE,
	},
};

static int __init tsop_init(void)
{
	return platform_driver_register(&tsop_driver);
}
module_init(tsop_init);

static void __exit tsop_exit(void)
{
	platform_driver_unregister(&tsop_driver);
}
module_exit(tsop_exit);

MODULE_DESCRIPTION("TSOP IR driver");
MODULE_LICENSE("GPL v2");
