// SPDX-License-Identifier: GPL-2.0-only
/* Copyright (c) 2019, Linux Foundation. All rights reserved.
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
#include <linux/of_gpio.h>

struct cd_gpio_extcon_data {
	struct extcon_dev *edev;
	int irq;
	struct gpio_desc *gpiod;
	unsigned int extcon_id;
	unsigned long irq_flags;
	struct pinctrl *pctrl;
	struct pinctrl_state *pins_default;
	unsigned int *supported_cable;
};

static irqreturn_t cd_gpio_threaded_irq_handler(int irq, void *dev_id)
{
	int state;
	struct cd_gpio_extcon_data *data = dev_id;

	state = gpiod_get_value_cansleep(data->gpiod);
	extcon_set_state_sync(data->edev, data->extcon_id, state);

	return IRQ_HANDLED;
}

static int extcon_parse_pinctrl_data(struct device *dev,
				     struct cd_gpio_extcon_data *data)
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

static int extcon_populate_data(struct device *dev,
				struct cd_gpio_extcon_data *data)
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

	ret = extcon_parse_pinctrl_data(dev, data);
	if (ret)
		dev_err(dev, "failed to parse pinctrl data\n");

out:
	return ret;
}

static int cd_gpio_extcon_probe(struct platform_device *pdev)
{
	struct cd_gpio_extcon_data *data;
	struct device *dev = &pdev->dev;
	int state, ret;

	data = devm_kzalloc(dev, sizeof(struct cd_gpio_extcon_data),
			    GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	if (!data->irq_flags) {
		/* try populating cd gpio extcon data from device tree */
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

	ret = devm_request_threaded_irq(dev, data->irq, NULL,
				  cd_gpio_threaded_irq_handler,
				  data->irq_flags | IRQF_ONESHOT,
				  pdev->name, data);
	if (ret < 0)
		return ret;

	ret = enable_irq_wake(data->irq);
	if (ret)
		return ret;

	platform_set_drvdata(pdev, data);

	/* Update initial state */
	state = gpiod_get_value_cansleep(data->gpiod);
	extcon_set_state(data->edev, data->extcon_id, state);

	return 0;
}

static int cd_gpio_extcon_remove(struct platform_device *pdev)
{
	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int cd_gpio_extcon_resume(struct device *dev)
{
	struct cd_gpio_extcon_data *data;
	int state, ret = 0;

	data = dev_get_drvdata(dev);
	state = gpiod_get_value_cansleep(data->gpiod);
	ret = extcon_set_state_sync(data->edev, data->extcon_id, state);
	if (ret)
		dev_err(dev, "%s: Failed to set extcon gpio state\n",
				__func__);

	return ret;
}

static const struct dev_pm_ops cd_gpio_extcon_pm_ops = {
	SET_LATE_SYSTEM_SLEEP_PM_OPS(NULL, cd_gpio_extcon_resume)
};

#define EXTCON_GPIO_PMOPS (&cd_gpio_extcon_pm_ops)

#else
#define EXTCON_GPIO_PMOPS NULL
#endif

static const struct of_device_id extcon_cd_gpio_of_match[] = {
	{ .compatible = "extcon-storage-cd-gpio"},
	{},
};

static struct platform_driver cd_gpio_extcon_driver = {
	.probe		= cd_gpio_extcon_probe,
	.remove		= cd_gpio_extcon_remove,
	.driver		= {
		.name	= "extcon-storage-cd-gpio",
		.pm	= EXTCON_GPIO_PMOPS,
		.of_match_table = of_match_ptr(extcon_cd_gpio_of_match),
	},
};

module_platform_driver(cd_gpio_extcon_driver);

MODULE_DESCRIPTION("Storage card detect GPIO based extcon driver");
MODULE_LICENSE("GPL v2");
