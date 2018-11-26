/*
 * Copyright (c) 2016-2018, The Linux Foundation. All rights reserved.
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

#include <linux/gpio.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/pinctrl/pinconf-generic.h>
#include <linux/pinctrl/pinconf.h>
#include <linux/pinctrl/pinmux.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <asoc/wcd934x_registers.h>

#include "core.h"
#include "pinctrl-utils.h"

#define WCD_REG_DIR_CTL WCD934X_CHIP_TIER_CTRL_GPIO_CTL_OE
#define WCD_REG_VAL_CTL WCD934X_CHIP_TIER_CTRL_GPIO_CTL_DATA
#define WCD_GPIO_PULL_UP       1
#define WCD_GPIO_PULL_DOWN     2
#define WCD_GPIO_BIAS_DISABLE  3
#define WCD_GPIO_STRING_LEN    20

/**
 * struct wcd_gpio_pad - keep current GPIO settings
 * @offset: offset of gpio.
 * @is_valid: Set to false, when GPIO in high Z state.
 * @value: value of a pin
 * @output_enabled: Set to true if GPIO is output and false if it is input
 * @pullup: Constant current which flow through GPIO output buffer.
 * @strength: Drive strength of a pin
 */
struct wcd_gpio_pad {
	u16  offset;
	bool is_valid;
	bool value;
	bool output_enabled;
	unsigned int pullup;
	unsigned int strength;
};

struct wcd_gpio_priv {
	struct device *dev;
	struct regmap *map;
	struct pinctrl_dev *ctrl;
	struct gpio_chip chip;
};

static int wcd_gpio_read(struct wcd_gpio_priv *priv_data,
			  struct wcd_gpio_pad *pad, unsigned int addr)
{
	unsigned int val;
	int ret;

	ret = regmap_read(priv_data->map, addr, &val);
	if (ret < 0)
		dev_err(priv_data->dev, "%s: read 0x%x failed\n",
			__func__, addr);
	else
		ret = (val >> pad->offset);

	return ret;
}

static int wcd_gpio_write(struct wcd_gpio_priv *priv_data,
			   struct wcd_gpio_pad *pad, unsigned int addr,
			   unsigned int val)
{
	int ret;

	ret = regmap_update_bits(priv_data->map, addr, (1 << pad->offset),
					val << pad->offset);
	if (ret < 0)
		dev_err(priv_data->dev, "write 0x%x failed\n", addr);

	return ret;
}

static int wcd_get_groups_count(struct pinctrl_dev *pctldev)
{
	return pctldev->desc->npins;
}

static const char *wcd_get_group_name(struct pinctrl_dev *pctldev,
		unsigned int pin)
{
	return pctldev->desc->pins[pin].name;
}

static int wcd_get_group_pins(struct pinctrl_dev *pctldev, unsigned int pin,
		const unsigned int **pins, unsigned int *num_pins)
{
	*pins = &pctldev->desc->pins[pin].number;
	*num_pins = 1;
	return 0;
}

static const struct pinctrl_ops wcd_pinctrl_ops = {
	.get_groups_count       = wcd_get_groups_count,
	.get_group_name         = wcd_get_group_name,
	.get_group_pins         = wcd_get_group_pins,
	.dt_node_to_map         = pinconf_generic_dt_node_to_map_group,
	.dt_free_map            = pinctrl_utils_free_map,
};

static int wcd_config_get(struct pinctrl_dev *pctldev,
				unsigned int pin, unsigned long *config)
{
	unsigned int param = pinconf_to_config_param(*config);
	struct wcd_gpio_pad *pad;
	unsigned int arg;

	pad = pctldev->desc->pins[pin].drv_data;

	switch (param) {
	case PIN_CONFIG_BIAS_PULL_DOWN:
		arg = pad->pullup == WCD_GPIO_PULL_DOWN;
		break;
	case PIN_CONFIG_BIAS_DISABLE:
		arg = pad->pullup = WCD_GPIO_BIAS_DISABLE;
		break;
	case PIN_CONFIG_BIAS_PULL_UP:
		arg = pad->pullup == WCD_GPIO_PULL_UP;
		break;
	case PIN_CONFIG_BIAS_HIGH_IMPEDANCE:
		arg = !pad->is_valid;
		break;
	case PIN_CONFIG_INPUT_ENABLE:
		arg = pad->output_enabled;
		break;
	case PIN_CONFIG_OUTPUT:
		arg = pad->value;
		break;
	default:
		return -EINVAL;
	}

	*config = pinconf_to_config_packed(param, arg);
	return 0;
}

static int wcd_config_set(struct pinctrl_dev *pctldev, unsigned int pin,
				unsigned long *configs, unsigned int nconfs)
{
	struct wcd_gpio_priv *priv_data = pinctrl_dev_get_drvdata(pctldev);
	struct wcd_gpio_pad *pad;
	unsigned int param, arg;
	int i, ret;

	pad = pctldev->desc->pins[pin].drv_data;

	for (i = 0; i < nconfs; i++) {
		param = pinconf_to_config_param(configs[i]);
		arg = pinconf_to_config_argument(configs[i]);

		dev_dbg(priv_data->dev, "%s: param: %d arg: %d",
			__func__, param, arg);

		switch (param) {
		case PIN_CONFIG_BIAS_DISABLE:
			pad->pullup = WCD_GPIO_BIAS_DISABLE;
			break;
		case PIN_CONFIG_BIAS_PULL_UP:
			pad->pullup = WCD_GPIO_PULL_UP;
			break;
		case PIN_CONFIG_BIAS_PULL_DOWN:
			pad->pullup = WCD_GPIO_PULL_DOWN;
			break;
		case PIN_CONFIG_BIAS_HIGH_IMPEDANCE:
			pad->is_valid = false;
			break;
		case PIN_CONFIG_INPUT_ENABLE:
			pad->output_enabled = false;
			break;
		case PIN_CONFIG_OUTPUT:
			pad->output_enabled = true;
			pad->value = arg;
			break;
		case PIN_CONFIG_DRIVE_STRENGTH:
			pad->strength = arg;
			break;
		default:
			ret = -EINVAL;
			goto done;
		}
	}

	if (pad->output_enabled) {
		ret = wcd_gpio_write(priv_data, pad, WCD_REG_DIR_CTL,
				     pad->output_enabled);
		if (ret < 0)
			goto done;
		ret = wcd_gpio_write(priv_data, pad, WCD_REG_VAL_CTL,
				     pad->value);
	} else
		ret = wcd_gpio_write(priv_data, pad, WCD_REG_DIR_CTL,
				     pad->output_enabled);
done:
	return ret;
}

static const struct pinconf_ops wcd_pinconf_ops = {
	.is_generic  = true,
	.pin_config_group_get = wcd_config_get,
	.pin_config_group_set = wcd_config_set,
};

static int wcd_gpio_direction_input(struct gpio_chip *chip, unsigned int pin)
{
	struct wcd_gpio_priv *priv_data = gpiochip_get_data(chip);
	unsigned long config;

	config = pinconf_to_config_packed(PIN_CONFIG_INPUT_ENABLE, 1);

	return wcd_config_set(priv_data->ctrl, pin, &config, 1);
}

static int wcd_gpio_direction_output(struct gpio_chip *chip,
				      unsigned int pin, int val)
{
	struct wcd_gpio_priv *priv_data = gpiochip_get_data(chip);
	unsigned long config;

	config = pinconf_to_config_packed(PIN_CONFIG_OUTPUT, val);

	return wcd_config_set(priv_data->ctrl, pin, &config, 1);
}

static int wcd_gpio_get(struct gpio_chip *chip, unsigned int pin)
{
	struct wcd_gpio_priv *priv_data = gpiochip_get_data(chip);
	struct wcd_gpio_pad *pad;
	int value;

	pad = priv_data->ctrl->desc->pins[pin].drv_data;

	if (!pad->is_valid)
		return -EINVAL;

	value = wcd_gpio_read(priv_data, pad, WCD_REG_VAL_CTL);
	return value;
}

static void wcd_gpio_set(struct gpio_chip *chip, unsigned int pin, int value)
{
	struct wcd_gpio_priv *priv_data = gpiochip_get_data(chip);
	unsigned long config;

	config = pinconf_to_config_packed(PIN_CONFIG_OUTPUT, value);

	wcd_config_set(priv_data->ctrl, pin, &config, 1);
}

static const struct gpio_chip wcd_gpio_chip = {
	.direction_input  = wcd_gpio_direction_input,
	.direction_output = wcd_gpio_direction_output,
	.get = wcd_gpio_get,
	.set = wcd_gpio_set,
};

static int wcd_pinctrl_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct pinctrl_pin_desc *pindesc;
	struct pinctrl_desc *pctrldesc;
	struct wcd_gpio_pad *pad, *pads;
	struct wcd_gpio_priv *priv_data;
	int ret, i, j;
	u32 npins;
	char **name;

	ret = of_property_read_u32(dev->of_node, "qcom,num-gpios", &npins);
	if (ret) {
		dev_err(dev, "%s: Looking up %s property in node %s failed\n",
			__func__, "qcom,num-gpios", dev->of_node->full_name);
		ret = -EINVAL;
		goto err_priv_alloc;
	}
	if (!npins) {
		dev_err(dev, "%s: no.of pins are 0\n", __func__);
		ret = -EINVAL;
		goto err_priv_alloc;
	}

	priv_data = devm_kzalloc(dev, sizeof(*priv_data), GFP_KERNEL);
	if (!priv_data) {
		ret = -ENOMEM;
		goto err_priv_alloc;
	}

	priv_data->dev = dev;
	priv_data->map = dev_get_regmap(dev->parent, NULL);
	if (!priv_data->map) {
		dev_err(dev, "%s: failed to get regmap\n", __func__);
		ret = -EINVAL;
		goto err_regmap;
	}

	pindesc = devm_kcalloc(dev, npins, sizeof(*pindesc), GFP_KERNEL);
	if (!pindesc) {
		ret = -ENOMEM;
		goto err_pinsec_alloc;
	}

	pads = devm_kcalloc(dev, npins, sizeof(*pads), GFP_KERNEL);
	if (!pads) {
		ret = -ENOMEM;
		goto err_pads_alloc;
	}

	pctrldesc = devm_kzalloc(dev, sizeof(*pctrldesc), GFP_KERNEL);
	if (!pctrldesc) {
		ret = -ENOMEM;
		goto err_pinctrl_alloc;
	}

	pctrldesc->pctlops = &wcd_pinctrl_ops;
	pctrldesc->confops = &wcd_pinconf_ops;
	pctrldesc->owner = THIS_MODULE;
	pctrldesc->name = dev_name(dev);
	pctrldesc->pins = pindesc;
	pctrldesc->npins = npins;

	name = devm_kcalloc(dev, npins, sizeof(char *), GFP_KERNEL);
	if (!name) {
		ret = -ENOMEM;
		goto err_name_alloc;
	}
	for (i = 0; i < npins; i++, pindesc++) {
		name[i] = devm_kzalloc(dev, sizeof(char) * WCD_GPIO_STRING_LEN,
				       GFP_KERNEL);
		if (!name[i]) {
			ret = -ENOMEM;
			goto err_pin;
		}
		pad = &pads[i];
		pindesc->drv_data = pad;
		pindesc->number = i;
		snprintf(name[i], (WCD_GPIO_STRING_LEN - 1), "gpio%d", (i+1));
		pindesc->name = name[i];
		pad->offset = i;
		pad->is_valid  = true;
	}

	priv_data->chip = wcd_gpio_chip;
	priv_data->chip.parent = dev;
	priv_data->chip.base = -1;
	priv_data->chip.ngpio = npins;
	priv_data->chip.label = dev_name(dev);
	priv_data->chip.of_gpio_n_cells = 2;
	priv_data->chip.can_sleep = false;

	priv_data->ctrl = devm_pinctrl_register(dev, pctrldesc, priv_data);
	if (IS_ERR(priv_data->ctrl)) {
		dev_err(dev, "%s: failed to register to pinctrl\n", __func__);
		ret = PTR_ERR(priv_data->ctrl);
		goto err_pin;
	}

	ret = gpiochip_add_data(&priv_data->chip, priv_data);
	if (ret) {
		dev_err(dev, "%s: can't add gpio chip\n", __func__);
		goto err_pin;
	}

	ret = gpiochip_add_pin_range(&priv_data->chip, dev_name(dev), 0, 0,
				     npins);
	if (ret) {
		dev_err(dev, "%s: failed to add pin range\n", __func__);
		goto err_range;
	}
	platform_set_drvdata(pdev, priv_data);

	return 0;

err_range:
	gpiochip_remove(&priv_data->chip);
err_pin:
	for (j = 0; j < i; j++)
		devm_kfree(dev, name[j]);
	devm_kfree(dev, name);
err_name_alloc:
	devm_kfree(dev, pctrldesc);
err_pinctrl_alloc:
	devm_kfree(dev, pads);
err_pads_alloc:
	devm_kfree(dev, pindesc);
err_pinsec_alloc:
err_regmap:
	devm_kfree(dev, priv_data);
err_priv_alloc:
	return ret;
}

static int wcd_pinctrl_remove(struct platform_device *pdev)
{
	struct wcd_gpio_priv *priv_data = platform_get_drvdata(pdev);

	gpiochip_remove(&priv_data->chip);

	return 0;
}

static const struct of_device_id wcd_pinctrl_of_match[] = {
	{ .compatible = "qcom,wcd-pinctrl" },
	{ },
};

MODULE_DEVICE_TABLE(of, wcd_pinctrl_of_match);

static struct platform_driver wcd_pinctrl_driver = {
	.driver = {
		   .name = "qcom-wcd-pinctrl",
		   .of_match_table = wcd_pinctrl_of_match,
		   .suppress_bind_attrs = true,
	},
	.probe = wcd_pinctrl_probe,
	.remove = wcd_pinctrl_remove,
};

module_platform_driver(wcd_pinctrl_driver);

MODULE_DESCRIPTION("Qualcomm Technologies, Inc WCD GPIO pin control driver");
MODULE_LICENSE("GPL v2");
