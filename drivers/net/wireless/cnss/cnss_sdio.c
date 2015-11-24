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
#include <linux/slab.h>

#define WLAN_VREG_NAME		"vdd-wlan"
#define WLAN_VREG_DSRC_NAME	"vdd-wlan-dsrc"
#define WLAN_VREG_IO_NAME	"vdd-wlan-io"
#define WLAN_VREG_XTAL_NAME	"vdd-wlan-xtal"

#define WLAN_VREG_IO_MAX	1800000
#define WLAN_VREG_IO_MIN	1800000
#define WLAN_VREG_XTAL_MAX	1800000
#define WLAN_VREG_XTAL_MIN	1800000
#define POWER_ON_DELAY		4

#define CNSS_MAX_CH_NUM 100

struct cnss_unsafe_channel_list {
	u16 unsafe_ch_count;
	u16 unsafe_ch_list[CNSS_MAX_CH_NUM];
};

struct cnss_dfs_nol_info {
	void *dfs_nol_info;
	u16 dfs_nol_info_len;
};

struct cnss_sdio_wlan_gpio_info {
	u32 num;
	u32 flags;
};

struct cnss_sdio_regulator {
	struct regulator *wlan_io;
	struct regulator *wlan_xtal;
	struct regulator *wlan_vreg;
	struct regulator *wlan_vreg_dsrc;
};

static struct cnss_sdio_data {
	struct cnss_sdio_regulator regulator;
	struct platform_device *pdev;
	struct cnss_sdio_wlan_gpio_info pmic_gpio;
	struct cnss_dfs_nol_info dfs_info;
	struct cnss_unsafe_channel_list unsafe_list;
} *cnss_pdata;

int cnss_set_wlan_unsafe_channel(u16 *unsafe_ch_list, u16 ch_count)
{
	struct cnss_unsafe_channel_list *unsafe_list;

	if (!cnss_pdata)
		return -ENODEV;

	if ((!unsafe_ch_list) || (!ch_count) || (ch_count > CNSS_MAX_CH_NUM))
		return -EINVAL;

	unsafe_list = &cnss_pdata->unsafe_list;
	unsafe_list->unsafe_ch_count = ch_count;

	memcpy(
		(char *)unsafe_list->unsafe_ch_list,
		(char *)unsafe_ch_list, ch_count * sizeof(u16));

	return 0;
}
EXPORT_SYMBOL(cnss_set_wlan_unsafe_channel);

int cnss_get_wlan_unsafe_channel(
	u16 *unsafe_ch_list, u16 *ch_count, u16 buf_len)
{
	struct cnss_unsafe_channel_list *unsafe_list;

	if (!cnss_pdata)
		return -ENODEV;

	if (!unsafe_ch_list || !ch_count)
		return -EINVAL;

	unsafe_list = &cnss_pdata->unsafe_list;

	if (buf_len < (unsafe_list->unsafe_ch_count * sizeof(u16)))
		return -ENOMEM;

	*ch_count = unsafe_list->unsafe_ch_count;
	memcpy(
		(char *)unsafe_ch_list, (char *)unsafe_list->unsafe_ch_list,
		unsafe_list->unsafe_ch_count * sizeof(u16));

	return 0;
}
EXPORT_SYMBOL(cnss_get_wlan_unsafe_channel);

int cnss_wlan_set_dfs_nol(void *info, u16 info_len)
{
	void *temp;
	struct cnss_dfs_nol_info *dfs_info;

	if (!cnss_pdata)
		return -ENODEV;

	if (!info || !info_len)
		return -EINVAL;

	temp = kmalloc(info_len, GFP_KERNEL);
	if (!temp)
		return -ENOMEM;

	memcpy(temp, info, info_len);
	dfs_info = &cnss_pdata->dfs_info;
	kfree(dfs_info->dfs_nol_info);

	dfs_info->dfs_nol_info = temp;
	dfs_info->dfs_nol_info_len = info_len;

	return 0;
}
EXPORT_SYMBOL(cnss_wlan_set_dfs_nol);

int cnss_wlan_get_dfs_nol(void *info, u16 info_len)
{
	int len;
	struct cnss_dfs_nol_info *dfs_info;

	if (!cnss_pdata)
		return -ENODEV;

	if (!info || !info_len)
		return -EINVAL;

	dfs_info = &cnss_pdata->dfs_info;

	if (dfs_info->dfs_nol_info == NULL || dfs_info->dfs_nol_info_len == 0)
		return -ENOENT;

	len = min(info_len, dfs_info->dfs_nol_info_len);

	memcpy(info, dfs_info->dfs_nol_info, len);

	return len;
}
EXPORT_SYMBOL(cnss_wlan_get_dfs_nol);

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

static int cnss_sdio_configure_wlan_enable_regulator(void)
{
	int error;
	struct device *dev = &cnss_pdata->pdev->dev;

	if (of_get_property(
		cnss_pdata->pdev->dev.of_node,
		WLAN_VREG_NAME "-supply", NULL)) {
		cnss_pdata->regulator.wlan_vreg = regulator_get(
			&cnss_pdata->pdev->dev, WLAN_VREG_NAME);
		if (IS_ERR(cnss_pdata->regulator.wlan_vreg)) {
			error = PTR_ERR(cnss_pdata->regulator.wlan_vreg);
			dev_err(dev, "VDD-VREG get failed error=%d\n", error);
			return error;
		}

		error = regulator_enable(cnss_pdata->regulator.wlan_vreg);
		if (error) {
			dev_err(dev, "VDD-VREG enable failed error=%d\n",
				error);
			goto err_vdd_vreg_regulator;
		}
	}

	return 0;

err_vdd_vreg_regulator:
	regulator_put(cnss_pdata->regulator.wlan_vreg);

	return error;
}

static int cnss_sdio_configure_wlan_enable_dsrc_regulator(void)
{
	int error;
	struct device *dev = &cnss_pdata->pdev->dev;

	if (of_get_property(
		cnss_pdata->pdev->dev.of_node,
		WLAN_VREG_DSRC_NAME "-supply", NULL)) {
		cnss_pdata->regulator.wlan_vreg_dsrc = regulator_get(
			&cnss_pdata->pdev->dev, WLAN_VREG_DSRC_NAME);
		if (IS_ERR(cnss_pdata->regulator.wlan_vreg_dsrc)) {
			error = PTR_ERR(cnss_pdata->regulator.wlan_vreg_dsrc);
			dev_err(dev, "VDD-VREG-DSRC get failed error=%d\n",
				error);
			return error;
		}

		error = regulator_enable(cnss_pdata->regulator.wlan_vreg_dsrc);
		if (error) {
			dev_err(dev, "VDD-VREG-DSRC enable failed error=%d\n",
				error);
			goto err_vdd_vreg_dsrc_regulator;
		}
	}

	return 0;

err_vdd_vreg_dsrc_regulator:
	regulator_put(cnss_pdata->regulator.wlan_vreg_dsrc);

	return error;
}

static int cnss_sdio_configure_regulator(void)
{
	int error;
	struct device *dev = &cnss_pdata->pdev->dev;

	if (of_get_property(
		cnss_pdata->pdev->dev.of_node,
		WLAN_VREG_IO_NAME "-supply", NULL)) {
		cnss_pdata->regulator.wlan_io = regulator_get(
			&cnss_pdata->pdev->dev, WLAN_VREG_IO_NAME);
		if (IS_ERR(cnss_pdata->regulator.wlan_io)) {
			error = PTR_ERR(cnss_pdata->regulator.wlan_io);
			dev_err(dev, "VDD-IO get failed error=%d\n", error);
			return error;
		}

		error = regulator_set_voltage(
			cnss_pdata->regulator.wlan_io,
			WLAN_VREG_IO_MIN, WLAN_VREG_IO_MAX);
		if (error) {
			dev_err(dev, "VDD-IO set failed error=%d\n", error);
			goto err_vdd_io_regulator;
		} else {
			error = regulator_enable(cnss_pdata->regulator.wlan_io);
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
		cnss_pdata->regulator.wlan_xtal = regulator_get(
			&cnss_pdata->pdev->dev, WLAN_VREG_XTAL_NAME);
		if (IS_ERR(cnss_pdata->regulator.wlan_xtal)) {
			error = PTR_ERR(cnss_pdata->regulator.wlan_xtal);
			dev_err(dev, "VDD-XTAL get failed error=%d\n", error);
			goto err_vdd_xtal_regulator;
		}

		error = regulator_set_voltage(
			cnss_pdata->regulator.wlan_xtal,
			WLAN_VREG_XTAL_MIN, WLAN_VREG_XTAL_MAX);
		if (error) {
			dev_err(dev, "VDD-XTAL set failed error=%d\n", error);
			goto err_vdd_xtal_regulator;
		} else {
			error = regulator_enable(
				cnss_pdata->regulator.wlan_xtal);
			if (error) {
				dev_err(dev, "VDD-XTAL enable failed err=%d\n",
					error);
				goto err_vdd_xtal_regulator;
			}
		}
	}

	return 0;

err_vdd_xtal_regulator:
	regulator_put(cnss_pdata->regulator.wlan_xtal);
err_vdd_io_regulator:
	regulator_put(cnss_pdata->regulator.wlan_io);
	return error;
}

static void cnss_sdio_release_resource(void)
{
	if (gpio_is_valid(cnss_pdata->pmic_gpio.num)) {
		gpio_set_value_cansleep(cnss_pdata->pmic_gpio.num, 0);
		gpio_free(cnss_pdata->pmic_gpio.num);
	}
	if (cnss_pdata->regulator.wlan_xtal)
		regulator_put(cnss_pdata->regulator.wlan_xtal);
	if (cnss_pdata->regulator.wlan_vreg)
		regulator_put(cnss_pdata->regulator.wlan_vreg);
	if (cnss_pdata->regulator.wlan_io)
		regulator_put(cnss_pdata->regulator.wlan_io);
	if (cnss_pdata->regulator.wlan_vreg_dsrc)
		regulator_put(cnss_pdata->regulator.wlan_vreg_dsrc);
}

static int cnss_sdio_probe(struct platform_device *pdev)
{
	int error;

	if (pdev->dev.of_node) {
		cnss_pdata = devm_kzalloc(
			&pdev->dev, sizeof(*cnss_pdata), GFP_KERNEL);
		if (!cnss_pdata)
			return -ENOMEM;
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

	cnss_pdata->pmic_gpio.num = of_get_named_gpio_flags(pdev->dev.of_node,
		"cnss_sdio,wlan-pmic-gpio", 0, &cnss_pdata->pmic_gpio.flags);
	if (cnss_pdata->pmic_gpio.num) {
		error = cnss_sdio_configure_gpio();
		if (error) {
			dev_err(&pdev->dev,
				"Failed to enable wlan enable gpio\n");
			goto err_wlan_enable_gpio;
		}
	}

	if (of_get_property(
		cnss_pdata->pdev->dev.of_node,
			WLAN_VREG_NAME "-supply", NULL)) {
		error = cnss_sdio_configure_wlan_enable_regulator();
		if (error) {
			dev_err(&pdev->dev,
				"Failed to enable wlan enable regulator\n");
			goto err_wlan_enable_regulator;
		}
	}

	if (of_get_property(
		cnss_pdata->pdev->dev.of_node,
			WLAN_VREG_DSRC_NAME "-supply", NULL)) {
		error = cnss_sdio_configure_wlan_enable_dsrc_regulator();
		if (error) {
			dev_err(&pdev->dev,
				"Failed to enable wlan dsrc enable regulator\n");
			goto err_wlan_dsrc_enable_regulator;
		}
	}

	dev_info(&pdev->dev, "CNSS SDIO Driver registered");
	return 0;

err_wlan_dsrc_enable_regulator:
	regulator_put(cnss_pdata->regulator.wlan_vreg_dsrc);
err_wlan_enable_regulator:
	regulator_put(cnss_pdata->regulator.wlan_vreg);
err_wlan_enable_gpio:
	regulator_put(cnss_pdata->regulator.wlan_xtal);
	regulator_put(cnss_pdata->regulator.wlan_io);
	return error;
}

static int cnss_sdio_remove(struct platform_device *pdev)
{
	struct cnss_dfs_nol_info *dfs_info;

	if (!cnss_pdata)
		return -ENODEV;

	dfs_info = &cnss_pdata->dfs_info;
	kfree(dfs_info->dfs_nol_info);

	cnss_sdio_release_resource();
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
