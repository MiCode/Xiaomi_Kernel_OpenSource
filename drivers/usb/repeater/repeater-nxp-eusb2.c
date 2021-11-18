// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2021, Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include <linux/debugfs.h>
#include <linux/err.h>
#include <linux/gpio/consumer.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/pinctrl/consumer.h>
#include <linux/platform_device.h>
#include <linux/power_supply.h>
#include <linux/of.h>
#include <linux/regmap.h>
#include <linux/regulator/consumer.h>
#include <linux/types.h>
#include <linux/usb/repeater.h>

#define EUSB2_3P0_VOL_MIN			3075000 /* uV */
#define EUSB2_3P0_VOL_MAX			3300000 /* uV */
#define EUSB2_3P0_HPM_LOAD			3500	/* uA */

#define EUSB2_1P2_VOL_MIN			1200000 /* uV */
#define EUSB2_1P2_VOL_MAX			1320000 /* uV */
#define EUSB2_1P2_HPM_LOAD			18000	/* uA */

struct eusb2_repeater {
	struct usb_repeater	ur;
	struct regmap		*regmap;
	u16			reg_base;
	struct regulator	*vdd12;
	struct regulator	*vdd3;
	bool			power_enabled;

	struct gpio_desc	*reset_gpiod;
	int			reset_gpio_irq;

	struct dentry		*root;
};

static int eusb2_repeater_power(struct eusb2_repeater *er, bool on)
{
	int ret = 0;

	dev_dbg(er->ur.dev, "%s turn %s regulators. power_enabled:%d\n",
			__func__, on ? "on" : "off", er->power_enabled);

	if (er->power_enabled == on) {
		dev_dbg(er->ur.dev, "regulators' regulators are already ON.\n");
		return 0;
	}

	if (!on)
		goto disable_vdd3;

	ret = regulator_set_load(er->vdd12, EUSB2_1P2_HPM_LOAD);
	if (ret < 0) {
		dev_err(er->ur.dev, "Unable to set HPM of vdd12:%d\n", ret);
		goto err_vdd12;
	}

	ret = regulator_set_voltage(er->vdd12, EUSB2_1P2_VOL_MIN,
						EUSB2_1P2_VOL_MAX);
	if (ret) {
		dev_err(er->ur.dev,
				"Unable to set voltage for vdd12:%d\n", ret);
		goto put_vdd12_lpm;
	}

	ret = regulator_enable(er->vdd12);
	if (ret) {
		dev_err(er->ur.dev, "Unable to enable vdd12:%d\n", ret);
		goto unset_vdd12;
	}

	ret = regulator_set_load(er->vdd3, EUSB2_3P0_HPM_LOAD);
	if (ret < 0) {
		dev_err(er->ur.dev, "Unable to set HPM of vdd3:%d\n", ret);
		goto disable_vdd12;
	}

	ret = regulator_set_voltage(er->vdd3, EUSB2_3P0_VOL_MIN,
						EUSB2_3P0_VOL_MAX);
	if (ret) {
		dev_err(er->ur.dev,
				"Unable to set voltage for vdd3:%d\n", ret);
		goto put_vdd3_lpm;
	}

	ret = regulator_enable(er->vdd3);
	if (ret) {
		dev_err(er->ur.dev, "Unable to enable vdd3:%d\n", ret);
		goto unset_vdd3;
	}

	er->power_enabled = true;
	pr_debug("%s(): eUSB2 repeater egulators are turned ON.\n", __func__);
	return ret;

disable_vdd3:
	ret = regulator_disable(er->vdd3);
	if (ret)
		dev_err(er->ur.dev, "Unable to disable vdd3:%d\n", ret);

unset_vdd3:
	ret = regulator_set_voltage(er->vdd3, 0, EUSB2_3P0_VOL_MAX);
	if (ret)
		dev_err(er->ur.dev,
			"Unable to set (0) voltage for vdd3:%d\n", ret);

put_vdd3_lpm:
	ret = regulator_set_load(er->vdd3, 0);
	if (ret < 0)
		dev_err(er->ur.dev, "Unable to set (0) HPM of vdd3\n");

disable_vdd12:
	ret = regulator_disable(er->vdd12);
	if (ret)
		dev_err(er->ur.dev, "Unable to disable vdd12:%d\n", ret);

unset_vdd12:
	ret = regulator_set_voltage(er->vdd12, 0, EUSB2_1P2_VOL_MAX);
	if (ret)
		dev_err(er->ur.dev,
			"Unable to set (0) voltage for vdd12:%d\n", ret);

put_vdd12_lpm:
	ret = regulator_set_load(er->vdd12, 0);
	if (ret < 0)
		dev_err(er->ur.dev, "Unable to set LPM of vdd12\n");

	/* case handling when regulator turning on failed */
	if (!er->power_enabled)
		return -EINVAL;

err_vdd12:
	er->power_enabled = false;
	dev_dbg(er->ur.dev, "eUSB2 repeater's regulators are turned OFF.\n");
	return ret;
}

static int eusb2_repeater_init(struct usb_repeater *ur)
{
	u8 status;
	struct eusb2_repeater *er =
			container_of(ur, struct eusb2_repeater, ur);

	/* Don't do anything as of now */
	dev_info(er->ur.dev, "eUSB2 repeater init\n");

	return 0;
}

static int eusb2_repeater_reset(struct usb_repeater *ur,
				bool bring_out_of_reset)
{
	struct eusb2_repeater *er =
			container_of(ur, struct eusb2_repeater, ur);

	dev_dbg(ur->dev, "reset gpio:%s\n",
			bring_out_of_reset ? "assert" : "deassert");
	gpiod_set_value_cansleep(er->reset_gpiod, bring_out_of_reset);
	return 0;
}

static int eusb2_repeater_powerup(struct usb_repeater *ur)
{
	struct eusb2_repeater *er =
			container_of(ur, struct eusb2_repeater, ur);

	return eusb2_repeater_power(er, true);
}

static int eusb2_repeater_powerdown(struct usb_repeater *ur)
{
	struct eusb2_repeater *er =
			container_of(ur, struct eusb2_repeater, ur);

	return eusb2_repeater_power(er, false);
}

static irqreturn_t eusb2_reset_gpio_irq_handler(int irq, void *dev_id)
{
	struct eusb2_repeater *er = dev_id;

	dev_dbg(er->ur.dev, "reset gpio interrupt handled\n");
	return IRQ_HANDLED;
}

static int eusb2_repeater_probe(struct platform_device *pdev)
{
	struct eusb2_repeater *er;
	struct device *dev = &pdev->dev;
	int ret = 0;

	er = devm_kzalloc(dev, sizeof(*er), GFP_KERNEL);
	if (!er) {
		ret = -ENOMEM;
		goto err_probe;
	}

	er->regmap = dev_get_regmap(pdev->dev.parent, NULL);
	if (!er->regmap) {
		dev_err(&pdev->dev, "failed to get parent's regmap\n");
		ret = -EINVAL;
		goto err_probe;
	}

	ret = of_property_read_u16(pdev->dev.of_node, "reg", &er->reg_base);
	if (ret < 0) {
		dev_err(&pdev->dev, "failed to get reg base address:%d\n", ret);
		goto err_probe;
	}

	er->vdd3 = devm_regulator_get(dev, "vdd3");
	if (IS_ERR(er->vdd3)) {
		dev_err(dev, "unable to get vdd3 supply\n");
		ret = PTR_ERR(er->vdd3);
		goto err_probe;
	}

	er->vdd12 = devm_regulator_get(dev, "vdd12");
	if (IS_ERR(er->vdd12)) {
		dev_err(dev, "unable to get vdd12 supply\n");
		ret = PTR_ERR(er->vdd12);
		goto err_probe;
	}

	er->reset_gpiod = devm_gpiod_get_optional(dev, "reset", GPIOD_OUT_HIGH);
	if (IS_ERR(er->reset_gpiod)) {
		ret = PTR_ERR(er->reset_gpiod);
		goto err_probe;
	}

	er->reset_gpio_irq = gpiod_to_irq(er->reset_gpiod);
	if (er->reset_gpio_irq < 0) {
		dev_err(dev, "failed to get reset gpio IRQ\n");
		goto err_probe;
	}

	ret = devm_request_irq(dev, er->reset_gpio_irq,
			eusb2_reset_gpio_irq_handler, IRQF_TRIGGER_RISING,
			pdev->name, er);
	if (ret < 0) {
		dev_err(dev, "failed to request reset gpio irq\n");
		goto err_probe;
	}

	er->param_override_seq_cnt = of_property_count_elems_of_size(
				dev->of_node, "qcom,param-override-seq",
				sizeof(*er->param_override_seq));
	if (er->param_override_seq_cnt % 2) {
		dev_err(dev, "invalid param_override_seq_len\n");
		ret = -EINVAL;
		goto err_probe;
	}

	er->ur.dev = dev;
	platform_set_drvdata(pdev, er);

	er->ur.init		= eusb2_repeater_init;
	er->ur.reset		= eusb2_repeater_reset;
	er->ur.powerup		= eusb2_repeater_powerup;
	er->ur.powerdown	= eusb2_repeater_powerdown;

	ret = usb_add_repeater_dev(&er->ur);
	if (ret)
		goto err_probe;

	eusb2_repeater_create_debugfs(er);
	return 0;

err_probe:
	return ret;
}

static int eusb2_repeater_remove(struct platform_device *pdev)
{
	struct eusb2_repeater *er = platform_get_drvdata(pdev);

	if (!er)
		return 0;

	debugfs_remove_recursive(er->root);
	usb_remove_repeater_dev(&er->ur);
	eusb2_repeater_power(er, false);
	return 0;
}

static const struct of_device_id eusb2_repeater_id_table[] = {
	{
		.compatible = "nxp,eusb2-repeater",
	},
	{ },
};
MODULE_DEVICE_TABLE(of, eusb2_repeater_id_table);

static struct platform_driver eusb2_repeater_driver = {
	.probe		= eusb2_repeater_probe,
	.remove		= eusb2_repeater_remove,
	.driver = {
		.name	= "eusb2-repeater",
		.of_match_table = of_match_ptr(eusb2_repeater_id_table),
	},
};

module_platform_driver(eusb2_repeater_driver);
MODULE_DESCRIPTION("NXP eUSB2 repeater driver");
MODULE_LICENSE("GPL v2");
