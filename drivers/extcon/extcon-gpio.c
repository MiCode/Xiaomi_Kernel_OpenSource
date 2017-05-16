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

#include <linux/extcon.h>
#include <linux/extcon/extcon-gpio.h>
#include <linux/gpio.h>
#include <linux/gpio/consumer.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/workqueue.h>
#include <linux/of_gpio.h>

struct gpio_extcon_data {
	struct extcon_dev *edev;
	int irq;
	struct delayed_work work;
	unsigned long debounce_jiffies;

	struct gpio_desc *id_gpiod;
	struct gpio_extcon_pdata *pdata;
	unsigned int *supported_cable;
};

static void gpio_extcon_work(struct work_struct *work)
{
	int state;
	struct gpio_extcon_data	*data =
		container_of(to_delayed_work(work), struct gpio_extcon_data,
			     work);

	state = gpiod_get_value_cansleep(data->id_gpiod);
	if (data->pdata->gpio_active_low)
		state = !state;

	extcon_set_state_sync(data->edev, data->pdata->extcon_id, state);
}

static irqreturn_t gpio_irq_handler(int irq, void *dev_id)
{
	struct gpio_extcon_data *data = dev_id;

	queue_delayed_work(system_power_efficient_wq, &data->work,
			      data->debounce_jiffies);
	return IRQ_HANDLED;
}

static int gpio_extcon_init(struct device *dev, struct gpio_extcon_data *data)
{
	struct gpio_extcon_pdata *pdata = data->pdata;
	int ret;

	ret = devm_gpio_request_one(dev, pdata->gpio, GPIOF_DIR_IN,
				dev_name(dev));
	if (ret < 0)
		return ret;

	data->id_gpiod = gpio_to_desc(pdata->gpio);
	if (!data->id_gpiod)
		return -EINVAL;

	if (pdata->debounce) {
		ret = gpiod_set_debounce(data->id_gpiod,
					pdata->debounce * 1000);
		if (ret < 0)
			data->debounce_jiffies =
				msecs_to_jiffies(pdata->debounce);
	}

	data->irq = gpiod_to_irq(data->id_gpiod);
	if (data->irq < 0)
		return data->irq;

	return 0;
}

static int extcon_parse_pinctrl_data(struct device *dev,
				     struct gpio_extcon_pdata *pdata)
{
	struct pinctrl *pctrl;
	int ret = 0;

	/* Try to obtain pinctrl handle */
	pctrl = devm_pinctrl_get(dev);
	if (IS_ERR(pctrl)) {
		ret = PTR_ERR(pctrl);
		goto out;
	}
	pdata->pctrl = pctrl;

	/* Look-up and keep the state handy to be used later */
	pdata->pins_default = pinctrl_lookup_state(pdata->pctrl,
						   "default");
	if (IS_ERR(pdata->pins_default)) {
		ret = PTR_ERR(pdata->pins_default);
		dev_err(dev, "Can't get default pinctrl state, ret %d\n", ret);
	}
out:
	return ret;
}

/* Parse platform data */
static
struct gpio_extcon_pdata *extcon_populate_pdata(struct device *dev)
{
	struct gpio_extcon_pdata *pdata = NULL;
	struct device_node *np = dev->of_node;
	enum of_gpio_flags flags;
	u32 val;

	pdata = devm_kzalloc(dev, sizeof(*pdata), GFP_KERNEL);
	if (!pdata)
		goto out;

	if (of_property_read_u32(np, "extcon-id", &pdata->extcon_id)) {
		dev_err(dev, "extcon-id property not found\n");
		goto out;
	}

	pdata->gpio = of_get_named_gpio_flags(np, "gpio", 0, &flags);
	if (gpio_is_valid(pdata->gpio)) {
		if (flags & OF_GPIO_ACTIVE_LOW)
			pdata->gpio_active_low = true;
	} else {
		dev_err(dev, "gpio property not found or invalid\n");
		goto out;
	}

	if (of_property_read_u32(np, "irq-flags", &val)) {
		dev_err(dev, "irq-flags property not found\n");
		goto out;
	}
	pdata->irq_flags = val;

	if (of_property_read_u32(np, "debounce-ms", &val)) {
		dev_err(dev, "debounce-ms property not found\n");
		goto out;
	}
	pdata->debounce = val;

	if (extcon_parse_pinctrl_data(dev, pdata)) {
		dev_err(dev, "failed to parse pinctrl data\n");
		goto out;
	}

	return pdata;
out:
	return NULL;
}

static int gpio_extcon_probe(struct platform_device *pdev)
{
	struct gpio_extcon_pdata *pdata = dev_get_platdata(&pdev->dev);
	struct gpio_extcon_data *data;
	int ret;

	if (!pdata) {
		/* try populating pdata from device tree */
		pdata = extcon_populate_pdata(&pdev->dev);
		if (!pdata)
			return -EBUSY;
	}
	if (!pdata->irq_flags || pdata->extcon_id >= EXTCON_NUM)
		return -EINVAL;

	data = devm_kzalloc(&pdev->dev, sizeof(struct gpio_extcon_data),
				   GFP_KERNEL);
	if (!data)
		return -ENOMEM;
	data->pdata = pdata;

	ret = pinctrl_select_state(pdata->pctrl, pdata->pins_default);
	if (ret < 0)
		dev_err(&pdev->dev, "pinctrl state select failed, ret %d\n",
			ret);

	/* Initialize the gpio */
	ret = gpio_extcon_init(&pdev->dev, data);
	if (ret < 0)
		return ret;

	data->supported_cable = devm_kzalloc(&pdev->dev,
					     sizeof(*data->supported_cable) * 2,
					     GFP_KERNEL);
	if (!data->supported_cable)
		return -ENOMEM;

	data->supported_cable[0] = pdata->extcon_id;
	data->supported_cable[1] = EXTCON_NONE;
	/* Allocate the memory of extcon devie and register extcon device */
	data->edev = devm_extcon_dev_allocate(&pdev->dev,
					      data->supported_cable);
	if (IS_ERR(data->edev)) {
		dev_err(&pdev->dev, "failed to allocate extcon device\n");
		return -ENOMEM;
	}

	ret = devm_extcon_dev_register(&pdev->dev, data->edev);
	if (ret < 0)
		return ret;

	INIT_DELAYED_WORK(&data->work, gpio_extcon_work);

	/*
	 * Request the interrupt of gpio to detect whether external connector
	 * is attached or detached.
	 */
	ret = devm_request_any_context_irq(&pdev->dev, data->irq,
					gpio_irq_handler, pdata->irq_flags,
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
	if (data->pdata->check_on_resume)
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
