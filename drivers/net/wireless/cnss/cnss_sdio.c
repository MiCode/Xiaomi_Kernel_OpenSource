/* Copyright (c) 2015, The Linux Foundation. All rights reserved.
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
#include <linux/module.h>
#include <linux/init.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/regulator/consumer.h>
#include <linux/of_gpio.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/err.h>

#define WLAN_VREG_IO_NAME	"vdd-wlan-io"
#define WLAN_VREG_XTAL_NAME	"vdd-wlan-xtal"

#define WLAN_VREG_IO_MAX	1800000
#define WLAN_VREG_IO_MIN	1800000
#define WLAN_VREG_XTAL_MAX	1800000
#define WLAN_VREG_XTAL_MIN	1800000
#define POWER_ON_DELAY		4

struct cnss_sdio_wlan_gpio_info {
	u32 num;
	u32 flags;
};

static struct cnss_sdio_data {
	struct regulator *wlan_reg_io;
	struct regulator *wlan_reg_xtal;
	struct platform_device *pdev;
	struct cnss_sdio_wlan_gpio_info pmic_gpio;
} *cnss_pdata;

static int cnss_sdio_configure_gpio(void)
{
	int error;
	struct device *dev = &cnss_pdata->pdev->dev;

	if (gpio_is_valid(cnss_pdata->pmic_gpio.num)) {
		error = gpio_request(
			cnss_pdata->pmic_gpio.num, "wlan_pmic_gpio");
		if (error) {
			dev_err(dev, "PMIC gpio request failed\n");
			return error;
		}

		error = gpio_direction_output(cnss_pdata->pmic_gpio.num, 0);
		if (error) {
			dev_err(dev, "PMIC gpio set direction failed\n");
			goto err_pmic_gpio;
		} else {
			gpio_set_value_cansleep(cnss_pdata->pmic_gpio.num, 1);
			msleep(POWER_ON_DELAY);
		}
	}

	return 0;

err_pmic_gpio:
	gpio_free(cnss_pdata->pmic_gpio.num);
	return error;
}

static int cnss_sdio_configure_regulator(void)
{
	int error;
	struct device *dev = &cnss_pdata->pdev->dev;

	if (of_get_property(
		cnss_pdata->pdev->dev.of_node,
		WLAN_VREG_IO_NAME "-supply", NULL)) {
		cnss_pdata->wlan_reg_io = regulator_get(
			&cnss_pdata->pdev->dev, WLAN_VREG_IO_NAME);
		if (IS_ERR(cnss_pdata->wlan_reg_io)) {
			error = PTR_ERR(cnss_pdata->wlan_reg_io);
			dev_err(dev, "VDD-IO get failed error=%d\n", error);
			return error;
		}

		error = regulator_set_voltage(
			cnss_pdata->wlan_reg_io,
			WLAN_VREG_IO_MIN, WLAN_VREG_IO_MAX);
		if (error) {
			dev_err(dev, "VDD-IO set failed error=%d\n", error);
			goto err_vdd_io_regulator;
		} else {
			error = regulator_enable(cnss_pdata->wlan_reg_io);
			if (error) {
				dev_err(dev, "VDD-IO enable failed error=%d\n",
					error);
				goto err_vdd_io_regulator;
			}
		}
	}

	if (of_get_property(
		cnss_pdata->pdev->dev.of_node,
		WLAN_VREG_XTAL_NAME "-supply", NULL)) {
		cnss_pdata->wlan_reg_xtal = regulator_get(
			&cnss_pdata->pdev->dev, WLAN_VREG_XTAL_NAME);
		if (IS_ERR(cnss_pdata->wlan_reg_xtal)) {
			error = PTR_ERR(cnss_pdata->wlan_reg_xtal);
			dev_err(dev, "VDD-XTAL get failed error=%d\n", error);
			goto err_vdd_xtal_regulator;
		}

		error = regulator_set_voltage(
			cnss_pdata->wlan_reg_xtal,
			WLAN_VREG_XTAL_MIN, WLAN_VREG_XTAL_MAX);
		if (error) {
			dev_err(dev, "VDD-XTAL set failed error=%d\n", error);
			goto err_vdd_xtal_regulator;
		} else {
			error = regulator_enable(cnss_pdata->wlan_reg_xtal);
			if (error) {
				dev_err(dev, "VDD-XTAL enable failed err=%d\n",
					error);
				goto err_vdd_xtal_regulator;
			}
		}
	}

	return 0;

err_vdd_xtal_regulator:
	regulator_put(cnss_pdata->wlan_reg_xtal);
err_vdd_io_regulator:
	regulator_put(cnss_pdata->wlan_reg_io);
	return error;
}

static int cnss_sdio_get_resource(struct device *dev)
{
	struct device_node *np = dev->of_node;

	/* wlan enable pmic gpio info */
	cnss_pdata->pmic_gpio.num = of_get_named_gpio_flags(np,
		"cnss_sdio,wlan-pmic-gpio", 0, &cnss_pdata->pmic_gpio.flags);
	if (cnss_pdata->pmic_gpio.num < 0)
		return cnss_pdata->pmic_gpio.num;
	return 0;
}

static void cnss_sdio_release_regulator(void)
{
	if (cnss_pdata->wlan_reg_xtal)
		regulator_put(cnss_pdata->wlan_reg_xtal);
	if (cnss_pdata->wlan_reg_io)
		regulator_put(cnss_pdata->wlan_reg_io);
}

static void cnss_sdio_release_resourse(void)
{
	if (gpio_is_valid(cnss_pdata->pmic_gpio.num)) {
		gpio_set_value_cansleep(cnss_pdata->pmic_gpio.num, 0);
		gpio_free(cnss_pdata->pmic_gpio.num);
	}

	cnss_sdio_release_regulator();
}

static int cnss_sdio_probe(struct platform_device *pdev)
{
	int error;

	if (pdev->dev.of_node) {
		cnss_pdata = devm_kzalloc(
			&pdev->dev, sizeof(*cnss_pdata), GFP_KERNEL);
		if (!cnss_pdata)
			return -ENOMEM;

		error = cnss_sdio_get_resource(&pdev->dev);
		if (error)
			return error;
	} else {
		cnss_pdata = pdev->dev.platform_data;
	}

	if (!cnss_pdata)
		return -EINVAL;

	cnss_pdata->pdev = pdev;
	error = cnss_sdio_configure_regulator();
	if (error) {
		dev_err(&pdev->dev, "Failed to config voltage regulator\n");
		return error;
	}

	error = cnss_sdio_configure_gpio();
	if (error) {
		dev_err(&pdev->dev, "Failed to config gpio\n");
		goto err_gpio_config;
	}

	dev_dbg(&pdev->dev, "CNSS SDIO Driver registered");
	return 0;

err_gpio_config:
	cnss_sdio_release_regulator();
	return error;
}

static int cnss_sdio_remove(struct platform_device *pdev)
{
	cnss_sdio_release_resourse();
	return 0;
}

static const struct of_device_id cnss_sdio_dt_match[] = {
	{.compatible = "qcom,cnss_sdio"},
	{}
};
MODULE_DEVICE_TABLE(of, cnss_sdio_dt_match);

static struct platform_driver cnss_sdio_driver = {
	.probe  = cnss_sdio_probe,
	.remove = cnss_sdio_remove,
	.driver = {
		.name = "cnss_sdio",
		.owner = THIS_MODULE,
		.of_match_table = cnss_sdio_dt_match,
	},
};

static int __init cnss_sdio_init(void)
{
	return platform_driver_register(&cnss_sdio_driver);
}

static void __exit cnss_sdio_exit(void)
{
	platform_driver_unregister(&cnss_sdio_driver);
}

module_init(cnss_sdio_init);
module_exit(cnss_sdio_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION(DEVICE "CNSS SDIO Driver");
