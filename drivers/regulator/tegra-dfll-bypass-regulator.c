/*
 *
 * Copyright (c) 2013 NVIDIA CORPORATION. All rights reserved.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/slab.h>
#include <linux/device.h>
#include <linux/module.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/platform_device.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/of_regulator.h>
#include <linux/regulator/machine.h>
#include <linux/regulator/tegra-dfll-bypass-regulator.h>
#include <linux/gpio.h>

struct tegra_dfll_bypass_regulator {
	struct device *dev;
	struct regulator_desc desc;
	struct tegra_dfll_bypass_platform_data *pdata;
};

static int tegra_dfll_bypass_set_voltage_sel(struct regulator_dev *reg,
					     unsigned int selector)
{
	int ret;
	struct tegra_dfll_bypass_regulator *tdb = rdev_get_drvdata(reg);

	ret = tdb->pdata->set_bypass_sel(tdb->pdata->dfll_data, selector);
	if (ret)
		dev_err(tdb->dev, "set selector failed, err %d\n", ret);

	return ret;
}

static int tegra_dfll_bypass_get_voltage_sel(struct regulator_dev *reg)
{
	struct tegra_dfll_bypass_regulator *tdb = rdev_get_drvdata(reg);
	return tdb->pdata->get_bypass_sel(tdb->pdata->dfll_data);
}

static int tegra_dfll_bypass_set_voltage_time_sel(struct regulator_dev *reg,
	unsigned int old_selector, unsigned int new_selector)
{
	struct tegra_dfll_bypass_regulator *tdb = rdev_get_drvdata(reg);
	return tdb->pdata->voltage_time_sel;
}

static int tegra_dfll_bypass_set_mode(struct regulator_dev *reg,
					unsigned int mode)
{
	struct tegra_dfll_bypass_regulator *tdb = rdev_get_drvdata(reg);

	if (!gpio_is_valid(tdb->pdata->msel_gpio))
		return -EINVAL;

	switch (mode) {
	case REGULATOR_MODE_IDLE:
		gpio_set_value(tdb->pdata->msel_gpio, 0);
		break;
	case REGULATOR_MODE_NORMAL:
		gpio_set_value(tdb->pdata->msel_gpio, 1);
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static unsigned int tegra_dfll_bypass_get_mode(struct regulator_dev *reg)
{
	struct tegra_dfll_bypass_regulator *tdb = rdev_get_drvdata(reg);

	if (!gpio_is_valid(tdb->pdata->msel_gpio))
		return 0;

	if (gpio_get_value(tdb->pdata->msel_gpio))
		return REGULATOR_MODE_NORMAL;
	else
		return REGULATOR_MODE_IDLE;
}

static struct regulator_ops tegra_dfll_bypass_rops = {
	.set_voltage_sel = tegra_dfll_bypass_set_voltage_sel,
	.get_voltage_sel = tegra_dfll_bypass_get_voltage_sel,
	.list_voltage = regulator_list_voltage_linear,
	.map_voltage = regulator_map_voltage_linear,
	.set_voltage_time_sel = tegra_dfll_bypass_set_voltage_time_sel,
	.set_mode = tegra_dfll_bypass_set_mode,
	.get_mode = tegra_dfll_bypass_get_mode,
};

static int tegra_dfll_bypass_probe(struct platform_device *pdev)
{
	struct tegra_dfll_bypass_platform_data *pdata;
	struct tegra_dfll_bypass_regulator *tdb;
	struct regulator_config config = { };
	struct regulator_dev *rdev;

	pdata = pdev->dev.platform_data;
	if (!pdata) {
		dev_err(&pdev->dev, "No platform data\n");
		return -ENODATA;
	}

	if (!pdata->set_bypass_sel || !pdata->get_bypass_sel ||
	    !pdata->dfll_data) {
		dev_err(&pdev->dev, "Invalid platform data\n");
		return -EINVAL;
	}

	tdb = devm_kzalloc(&pdev->dev, sizeof(*tdb), GFP_KERNEL);
	if (!tdb) {
		dev_err(&pdev->dev, "Memory allocation failed\n");
		return -ENOMEM;
	}

	tdb->dev = &pdev->dev;
	tdb->pdata = pdata;

	tdb->desc.name = "DFLL_BYPASS";
	tdb->desc.id = 0;
	tdb->desc.ops = &tegra_dfll_bypass_rops;
	tdb->desc.type = REGULATOR_VOLTAGE;
	tdb->desc.owner = THIS_MODULE;

	tdb->desc.min_uV = pdata->reg_init_data->constraints.min_uV;
	tdb->desc.uV_step = pdata->uV_step;
	tdb->desc.linear_min_sel = pdata->linear_min_sel;
	tdb->desc.n_voltages = pdata->n_voltages;

	config.dev = &pdev->dev;
	config.init_data = pdata->reg_init_data;
	config.driver_data = tdb;

	rdev = regulator_register(&tdb->desc, &config);
	if (IS_ERR(rdev)) {
		dev_err(tdb->dev, "failed to register regulator %s\n",
			tdb->desc.name);
		return PTR_ERR(rdev);
	}

	if (gpio_is_valid(tdb->pdata->msel_gpio)) {
		if (gpio_request_one(pdata->msel_gpio, GPIOF_INIT_HIGH,
					"CPUREG_MODE_SEL")) {
			dev_err(&pdev->dev, "MODE_SEL gpio request failed\n");
			tdb->pdata->msel_gpio = -EINVAL;
		}
	}

	platform_set_drvdata(pdev, rdev);
	return 0;
}

static int tegra_dfll_bypass_remove(struct platform_device *pdev)
{
	struct regulator_dev *rdev = platform_get_drvdata(pdev);
	struct tegra_dfll_bypass_regulator *tdb = rdev_get_drvdata(rdev);

	if (gpio_is_valid(tdb->pdata->msel_gpio))
		gpio_free(tdb->pdata->msel_gpio);
	regulator_unregister(rdev);
	return 0;
}

static struct platform_driver tegra_dfll_bypass_driver = {
	.driver = {
		.name	= "tegra_dfll_bypass",
		.owner  = THIS_MODULE,
	},
	.probe	= tegra_dfll_bypass_probe,
	.remove	= tegra_dfll_bypass_remove,
};

static int __init tegra_dfll_bypass_init(void)
{
	return platform_driver_register(&tegra_dfll_bypass_driver);
}
postcore_initcall(tegra_dfll_bypass_init);

static void __exit tegra_dfll_bypass_exit(void)
{
	platform_driver_unregister(&tegra_dfll_bypass_driver);
}
module_exit(tegra_dfll_bypass_exit);

