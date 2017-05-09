/*
 * extcon_gpio.c - Single-state GPIO extcon driver based on extcon class
 *
 * Copyright (C) 2008 Google, Inc.
 * Author: Mike Lockwood <lockwood@android.com>
 *
 * Modified by MyungJoo Ham <myungjoo.ham@samsung.com> to support extcon
 * (originally switch class is supported)
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/extcon-provider.h>
#include <linux/gpio/consumer.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/workqueue.h>
#include <linux/of_gpio.h>

/**
 * struct gpio_extcon_data - A simple GPIO-controlled extcon device state container.
 * @edev:		Extcon device.
 * @irq:		Interrupt line for the external connector.
 * @work:		Work fired by the interrupt.
 * @debounce_jiffies:	Number of jiffies to wait for the GPIO to stabilize, from the debounce
 *			value.
 * @gpiod:		GPIO descriptor for this external connector.
 * @extcon_id:		The unique id of specific external connector.
 * @debounce:		Debounce time for GPIO IRQ in ms.
 * @irq_flags:		IRQ Flags (e.g., IRQF_TRIGGER_LOW).
 * @check_on_resume:	Boolean describing whether to check the state of gpio
 *			while resuming from sleep.
 * @pctrl:		GPIO pinctrl handle.
 * @pctrl_default:	GPIO pinctrl default state handle.
 * @supported_cable:	Supported extcon cables.
 */
struct gpio_extcon_data {
	struct extcon_dev *edev;
	int irq;
	struct delayed_work work;
	unsigned long debounce_jiffies;
	struct gpio_desc *gpiod;
	unsigned int extcon_id;
	unsigned long debounce;
	unsigned long irq_flags;
	bool check_on_resume;
	struct pinctrl *pctrl;
	struct pinctrl_state *pins_default;
	unsigned int *supported_cable;
};

static void gpio_extcon_work(struct work_struct *work)
{
	int state;
	struct gpio_extcon_data	*data =
		container_of(to_delayed_work(work), struct gpio_extcon_data,
			     work);

	state = gpiod_get_value_cansleep(data->gpiod);
	extcon_set_state_sync(data->edev, data->extcon_id, state);
}

static irqreturn_t gpio_irq_handler(int irq, void *dev_id)
{
	struct gpio_extcon_data *data = dev_id;

	queue_delayed_work(system_power_efficient_wq, &data->work,
			      data->debounce_jiffies);
	return IRQ_HANDLED;
}

static int extcon_parse_pinctrl_data(struct device *dev,
				     struct gpio_extcon_data *data)
{
	struct pinctrl *pctrl;
	int ret = 0;

	/* Try to obtain pinctrl handle */
	pctrl = devm_pinctrl_get(dev);
	if (IS_ERR(pctrl)) {
		ret = PTR_ERR(pctrl);
		goto out;
	}
	data->pctrl = pctrl;

	/* Look-up and keep the state handy to be used later */
	data->pins_default = pinctrl_lookup_state(data->pctrl, "default");
	if (IS_ERR(data->pins_default)) {
		ret = PTR_ERR(data->pins_default);
		dev_err(dev, "Can't get default pinctrl state, ret %d\n", ret);
	}
out:
	return ret;
}

/* Parse extcon data */
static int extcon_populate_data(struct device *dev,
				struct gpio_extcon_data *data)
{
	struct device_node *np = dev->of_node;
	u32 val;
	int ret = 0;

	ret = of_property_read_u32(np, "extcon-id", &data->extcon_id);
	if (ret) {
		dev_err(dev, "failed to read extcon-id property, %d\n", ret);
		goto out;
	}

	ret = of_property_read_u32(np, "irq-flags", &val);
	if (ret) {
		dev_err(dev, "failed to read irq-flags property, %d\n", ret);
		goto out;
	}
	data->irq_flags = val;

	ret = of_property_read_u32(np, "debounce-ms", &val);
	if (ret) {
		dev_err(dev, "failed to read debounce-ms property, %d\n", ret);
		goto out;
	}
	data->debounce = val;

	ret = extcon_parse_pinctrl_data(dev, data);
	if (ret)
		dev_err(dev, "failed to parse pinctrl data\n");

out:
	return ret;
}

static int gpio_extcon_probe(struct platform_device *pdev)
{
	struct gpio_extcon_data *data;
	struct device *dev = &pdev->dev;
	int ret;

	data = devm_kzalloc(dev, sizeof(struct gpio_extcon_data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	if (!data->irq_flags) {
		/* try populating gpio extcon data from device tree */
		ret = extcon_populate_data(dev, data);
		if (ret)
			return ret;
	}
	if (!data->irq_flags || data->extcon_id >= EXTCON_NUM)
		return -EINVAL;

	ret = pinctrl_select_state(data->pctrl, data->pins_default);
	if (ret < 0)
		dev_err(dev, "pinctrl state select failed, ret %d\n", ret);

	data->gpiod = devm_gpiod_get(dev, "extcon", GPIOD_IN);
	if (IS_ERR(data->gpiod))
		return PTR_ERR(data->gpiod);

	if (data->debounce) {
		ret = gpiod_set_debounce(data->gpiod, data->debounce * 1000);
		if (ret < 0)
			data->debounce_jiffies =
				msecs_to_jiffies(data->debounce);
	}

	data->irq = gpiod_to_irq(data->gpiod);
	if (data->irq <= 0)
		return data->irq;

	data->supported_cable = devm_kzalloc(dev,
					     sizeof(*data->supported_cable) * 2,
					     GFP_KERNEL);
	if (!data->supported_cable)
		return -ENOMEM;

	data->supported_cable[0] = data->extcon_id;
	data->supported_cable[1] = EXTCON_NONE;
	/* Allocate the memory of extcon devie and register extcon device */
	data->edev = devm_extcon_dev_allocate(dev, data->supported_cable);
	if (IS_ERR(data->edev)) {
		dev_err(dev, "failed to allocate extcon device\n");
		return -ENOMEM;
	}

	ret = devm_extcon_dev_register(dev, data->edev);
	if (ret < 0)
		return ret;

	INIT_DELAYED_WORK(&data->work, gpio_extcon_work);

	/*
	 * Request the interrupt of gpio to detect whether external connector
	 * is attached or detached.
	 */
	ret = devm_request_any_context_irq(dev, data->irq,
					gpio_irq_handler, data->irq_flags,
					pdev->name, data);
	if (ret < 0)
		return ret;

	platform_set_drvdata(pdev, data);
	/* Perform initial detection */
	gpio_extcon_work(&data->work.work);

	return 0;
}

static int gpio_extcon_remove(struct platform_device *pdev)
{
	struct gpio_extcon_data *data = platform_get_drvdata(pdev);

	cancel_delayed_work_sync(&data->work);

	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int gpio_extcon_resume(struct device *dev)
{
	struct gpio_extcon_data *data;

	data = dev_get_drvdata(dev);
	if (data->check_on_resume)
		queue_delayed_work(system_power_efficient_wq,
			&data->work, data->debounce_jiffies);

	return 0;
}
#endif

static SIMPLE_DEV_PM_OPS(gpio_extcon_pm_ops, NULL, gpio_extcon_resume);

static const struct of_device_id extcon_gpio_of_match[] = {
	{ .compatible = "extcon-gpio"},
	{},
};

static struct platform_driver gpio_extcon_driver = {
	.probe		= gpio_extcon_probe,
	.remove		= gpio_extcon_remove,
	.driver		= {
		.name	= "extcon-gpio",
		.pm	= &gpio_extcon_pm_ops,
		.of_match_table = of_match_ptr(extcon_gpio_of_match),
	},
};

module_platform_driver(gpio_extcon_driver);

MODULE_AUTHOR("Mike Lockwood <lockwood@android.com>");
MODULE_DESCRIPTION("GPIO extcon driver");
MODULE_LICENSE("GPL");
