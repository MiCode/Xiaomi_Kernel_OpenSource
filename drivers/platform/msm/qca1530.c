/* Copyright (c) 2013-2014, The Linux Foundation. All rights reserved.
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

#define pr_fmt(fmt) "%s:%d] " fmt "\n", __func__, __LINE__

#include <linux/init.h>
#include <linux/clk.h>
#include <linux/err.h>
#include <linux/fs.h>
#include <linux/gpio.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/regulator/consumer.h>
#include <linux/platform_device.h>
#include <linux/uaccess.h>
#include <linux/types.h>
#include <linux/kobject.h>
#include <linux/string.h>
#include <linux/sysfs.h>

/* RTC clock name for lookup */
#define QCA1530_RTC_CLK_ID		"qca,rtc_clk"
/* TCXO clock name for lookup */
#define QCA1530_TCXO_CLK_ID		"qca,tcxo_clk"
/* SoC power regulator prefix for DTS */
#define QCA1530_OF_PWR_REG_NAME		"qca,pwr"
/* SoC optional power regulator prefix for DTS */
#define QCA1530_OF_PWR_OPT_REG_NAME	"qca,pwr2"
/* SoC power regulator pin for DTS */
#define QCA1530_OF_PWR_GPIO_NAME	"qca,pwr-gpio"
/* Reset power regulator prefix for DTS */
#define QCA1530_OF_RESET_REG_NAME	"qca,reset"
/* Reset pin name for DTS */
#define QCA1530_OF_RESET_GPIO_NAME	"qca,reset-gpio"
/* Clock pin name for DTS */
#define QCA1530_OF_CLK_GPIO_NAME	"qca,clk-gpio"
/* xLNA power regulator prefix for DTS */
#define QCA1530_OF_XLNA_REG_NAME	"qca,xlna"
/* xLNA voltage property name in DTS */
#define QCA1530_OF_XLNA_POWER_VOLTAGE	"qca,xlna-voltage-level"
/* xLNA current property name in DTS */
#define QCA1530_OF_XLNA_POWER_CURRENT	"qca,xlna-current-level"
/* xLNA pin name for DTS */
#define QCA1530_OF_XLNA_GPIO_NAME	"qca,xlna-gpio"

#define QCA1530_ALL_FLG		0x0f
#define QCA1530_POWER_FLG	0x01
#define QCA1530_XLNA_FLG	0x02
#define QCA1530_CLK_FLG		0x04
#define QCA1530_RESET_FLG	0x08

#define GPIO_OPTIONAL_FLG	0x01
#define GPIO_EXPORT_FLG		0x02
#define GPIO_OUTDIR_FLG		0x04

#define SYSFS_NODE_NAME			"qca1530"

/**
 * struct qca1530_static - keeps all driver instance variables
 * @pdev:          Platform device data
 * @reset_gpio:    Number of GPIO pin for reset control, or -1
 * @reset_reg:     Handle to power regulator for reset control, or NULL
 * @rtc_clk:       RTC clock handle (32KHz)
 * @rtc_clk_gpio:  RTC clock gppio, or -1
 * @tcxo_clk:      TCXO clock handle
 * @pwr_reg:       Main SoC power regulator handle, or NULL
 * @pwr_gpio:      Main SoC power regulator GPIO, or -1
 * @xlna_reg:      xLNA power regulator handle, or NULL
 * @xlna_gpio:     xLNA power GPIO, or -1
 *
 * The structure contains all driver instance variables.
 */
struct qca1530_static {
	struct platform_device	*pdev;
	int			reset_gpio;
	struct regulator	*reset_reg;
	struct clk		*rtc_clk;
	int			rtc_clk_gpio;
	struct clk		*tcxo_clk;
	struct regulator	*pwr_reg;
	struct regulator	*pwr_opt_reg;
	int			pwr_gpio;
	struct regulator	*xlna_reg;
	int			xlna_gpio;
	int			chip_state;
};

/*
 * qca1530_data - driver instance data
 */
static struct qca1530_static qca1530_data = {
	.reset_gpio = -1,
	.rtc_clk_gpio = -1,
	.pwr_gpio = -1,
	.xlna_gpio = -1,
	.chip_state = 0,
};

/**
 * qca1530_deinit_gpio() - release GPIO resource
 * @pdev: platform device data
 * @pgpio: pointer to GPIO handle
 * @flags: flags showing how gpio was initialized
 *
 * Function releases GPIO handle allocated by the driver. By default the
 * GPIO pin is removed from user space, switched to input direction and
 * then released.
 *
 * GPIO handle is set to -1.
 */
static void qca1530_deinit_gpio(struct platform_device *pdev, int *pgpio,
	int flags)
{
	if (*pgpio >= 0) {
		pr_debug("Releasing GPIO: %d, %d", *pgpio, flags);
		if (flags & GPIO_EXPORT_FLG)
			gpio_unexport(*pgpio);
		gpio_direction_input(*pgpio);
		devm_gpio_free(&pdev->dev, *pgpio);
		*pgpio = -1;
	}
}

/**
 * qca1530_init_gpio() - initialize GPIO resource
 * @pdev: platform device data
 * @pgpio: pointer to GPIO handle
 * @pgio_name: name of gpio in device tree
 * @flags: flags to control export, direction, etc
 *
 * Function initializes gpio and returns 0 on sucess.
 *
 */
static int qca1530_init_gpio(struct platform_device *pdev, int *pgpio,
	const char *gpio_name, int flags)
{
	int ret;

	pr_debug("Initializing gpio %s, flags %d", gpio_name, flags);

	ret = of_get_named_gpio(pdev->dev.of_node, gpio_name, 0);

	if (ret == -ENOENT && (flags & GPIO_OPTIONAL_FLG)) {
		*pgpio = -1;
		pr_debug("Optional GPIO is not defined");
		return 0;
	} else if (ret < 0) {
		pr_err("Error getting GPIO from config: %d", ret);
		goto err_gpio_init;
	}
	*pgpio = ret;

	ret = devm_gpio_request(&pdev->dev, *pgpio, gpio_name);
	if (ret < 0) {
		pr_err("failed to request gpio %d, %d", *pgpio, ret);
		goto err_gpio_init;
	}

	if (flags & GPIO_OUTDIR_FLG) {
		ret = gpio_direction_output(*pgpio, 0);
		if (ret < 0) {
			pr_err("failed to change direction for gpio %d, %d",
				*pgpio, ret);
			goto err_gpio_set_dir;
		}
	}

	if (flags & GPIO_EXPORT_FLG) {
		ret = gpio_export(*pgpio, false);
		if (ret < 0) {
			pr_err("failed to export gpio %d for user, %d",
				*pgpio, ret);
			goto err_gpio_export;
		}
	}

	pr_debug("Initialized gpio %s: %d", gpio_name, *pgpio);
	return 0;

err_gpio_export:
	gpio_direction_input(*pgpio);

err_gpio_set_dir:
	devm_gpio_free(&pdev->dev, *pgpio);

err_gpio_init:
	*pgpio = -1;
	return ret;
}

/**
 * qca1530_deinit_regulator() - release power regulator resource
 * @ppwr: pointer to power regulator handle
 *
 * Function releases power regulator and sets handle to NULL.
 */
static void qca1530_deinit_regulator(struct regulator **ppwr)
{
	if (*ppwr) {
		devm_regulator_put(*ppwr);
		*ppwr = NULL;
	}
}

/**
 * qca1530_clk_prepare() - prepare or unprepare clock
 * @clk:  clock handle
 * @mode: 0 to unprepare, 1 to prepare
 *
 * Function prepares or unprepares clock and logs the result.
 *
 * Return: 0 on success, error code on failure
 */
static int qca1530_clk_prepare(struct clk *clk, int mode)
{
	int ret = 0;

	if (mode)
		ret = clk_prepare_enable(clk);
	else
		clk_disable_unprepare(clk);

	pr_debug("Configured clock (%p): mode=%d ret=%d", clk, mode, ret);

	return ret;
}

/**
 * qca1530_clk_set_gpio() - enable or disable RTC GPIO pin
 * @mode: 1 to enable, 0 to disable
 *
 * Function enables or disable clock GPIO pin and logs the result.
 */
static void qca1530_clk_set_gpio(int mode)
{
	gpio_set_value(qca1530_data.rtc_clk_gpio, mode ? 1 : 0);
	pr_debug("Set clk GPIO (%d): mode=%d", qca1530_data.rtc_clk_gpio, mode);
}

/**
 * qca1530_clk_set() - start/stop initialized clocks
 * @mode: 1 to start clocks, 0 to stop
 *
 * Function turns clocks on or off. Clock configuration is optional, however
 * when configured, the following order is used: RTC GPIO pin, RTC clock,
 * TCXO clock. When switching off, the order is reveresed.
 *
 * Return: 0 on success, error code on failure.
 */
static int qca1530_clk_set(int mode)
{
	int ret = 0;

	if (qca1530_data.rtc_clk_gpio < 0 &&
		!qca1530_data.rtc_clk &&
		!qca1530_data.tcxo_clk) {
		ret = -ENOSYS;
	} else {
		if (qca1530_data.rtc_clk_gpio >= 0)
			qca1530_clk_set_gpio(mode);
		if (qca1530_data.rtc_clk)
			ret = qca1530_clk_prepare(qca1530_data.rtc_clk, mode);
		if (!ret && qca1530_data.tcxo_clk)
			ret = qca1530_clk_prepare(qca1530_data.tcxo_clk, mode);
	}

	pr_debug("Configured clk: mode=%d ret=%d", mode, ret);
	return ret;
}

/**
 * qca1530_clk_release_clock() - release clocks
 * @pdev: platform device data
 * @clk_name: clock name
 * @clk: pointer to clock handle pointer
 *
 * Function releases initialized clocks and sets clock handle
 * pointer to NULL.
 */
static void qca1530_clk_release_clock(
	struct platform_device *pdev,
	const char *clk_name,
	struct clk **clk)
{
	if (*clk) {
		pr_debug("Unregistering CLK: name=%s", clk_name);
		devm_clk_put(&pdev->dev, *clk);
		*clk = NULL;
	}
}

/**
 * qca1530_clk_release_clocks() - release clocks
 * @pdev: platform device data
 *
 * Function releases initialized clocks and sets clock handle
 * pointers to NULL.
 */
static void qca1530_clk_release_clocks(struct platform_device *pdev)
{
	qca1530_deinit_gpio(pdev, &qca1530_data.rtc_clk_gpio,
		GPIO_OPTIONAL_FLG | GPIO_OUTDIR_FLG);
	qca1530_clk_release_clock(pdev, QCA1530_TCXO_CLK_ID,
		&qca1530_data.tcxo_clk);
	qca1530_clk_release_clock(pdev, QCA1530_RTC_CLK_ID,
		&qca1530_data.rtc_clk);
}

/**
 * qca1530_clk_init() - allocate and initialize clock input resources
 * @pdev: platform device data
 *
 * Function tries to connect to RTC and TCXO clocks and obtain clock GPIO
 * pin.
 *
 * Return: 0 on success, error code on failure.
 */
static int qca1530_clk_init(struct platform_device *pdev)
{
	int ret;

	pr_debug("Initializing clock");

	qca1530_data.rtc_clk = devm_clk_get(&pdev->dev, QCA1530_RTC_CLK_ID);

	if (IS_ERR(qca1530_data.rtc_clk)) {
		ret = PTR_ERR(qca1530_data.rtc_clk);
		qca1530_data.rtc_clk = NULL;
		if (ret == -ENOENT) {
			pr_debug("No RTC clock controller");
		} else {
			pr_err("Clock init error: device=%s clock=%s ret=%d",
				dev_name(&pdev->dev), QCA1530_RTC_CLK_ID,
				ret);
			goto err_0;
		}
	} else
		pr_debug("Ref to clock %s obtained: %p",
				QCA1530_RTC_CLK_ID, qca1530_data.rtc_clk);

	qca1530_data.tcxo_clk = devm_clk_get(&pdev->dev, QCA1530_TCXO_CLK_ID);
	if (IS_ERR(qca1530_data.tcxo_clk)) {
		ret = PTR_ERR(qca1530_data.tcxo_clk);
		qca1530_data.tcxo_clk = NULL;
		if (ret == -ENOENT) {
			pr_debug("No TCXO clock controller");
		} else {
			pr_err("Clock init error: device=%s clock=%s ret=%d",
				dev_name(&pdev->dev), QCA1530_TCXO_CLK_ID,
				ret);
			goto err_1;
		}
	} else
		pr_debug("Ref to clock %s obtained: %p",
				QCA1530_TCXO_CLK_ID, qca1530_data.tcxo_clk);

	ret = qca1530_init_gpio(pdev, &qca1530_data.rtc_clk_gpio,
		QCA1530_OF_CLK_GPIO_NAME, GPIO_OPTIONAL_FLG | GPIO_OUTDIR_FLG);
	if (ret)
		goto err_1;

	ret = qca1530_clk_set(1);
	if (ret < 0) {
		pr_err("Clock set error: ret=%d", ret);
		goto err_1;
	}

	pr_debug("Clock init done: GPIO=%s RTC=%s TCXO=%s",
		qca1530_data.rtc_clk_gpio >= 0 ? "ok" : "unused",
		qca1530_data.rtc_clk ? "ok" : "unused",
		qca1530_data.tcxo_clk ? "ok" : "unused");

	return 0;

err_1:
	qca1530_clk_release_clocks(pdev);
err_0:
	pr_err("init error: ret=%d", ret);

	return ret;
}

/**
 * qca1530_clk_deinit() - release clock control resources
 * @pdev: platform device data
 *
 * Function disables clock control and releases GPIO and clock resources.
 */
static void qca1530_clk_deinit(struct platform_device *pdev)
{
	qca1530_clk_set(0);
	qca1530_deinit_gpio(pdev, &qca1530_data.rtc_clk_gpio,
		GPIO_OPTIONAL_FLG | GPIO_OUTDIR_FLG);
	qca1530_clk_release_clocks(pdev);
}

/**
 * qca1530_pwr_set_gpio() - set power GPIO line
 * @mode: 1 to enable, 0 to disable
 *
 * Function sets GPIO pin value and logs the result.
 */
static void qca1530_pwr_set_gpio(int mode)
{
	gpio_set_value(qca1530_data.pwr_gpio, mode ? 1 : 0);

	pr_debug("Configuring power(GPIO): mode=%d", mode);
}

/**
 * qca1530_pwr_set_regulator() - control regulator and log results
 * @reg:  regulator to control
 * @mode: 1 to enable, 0 to disable
 *
 * Function controls power regulator and logs results.
 *
 * Return: 0 on success, error code otherwise
 */
static int qca1530_pwr_set_regulator(struct regulator *reg, int mode)
{
	int ret;

	pr_debug("Setting regulator: mode=%d regulator=%p", mode, reg);

	if (mode) {
		ret = regulator_enable(reg);
		if (ret)
			pr_err("Failed to enable regulator, rc=%d", ret);
		else
			pr_debug("Regulator %p was enabled (%d)", reg, ret);

	} else {
		if (!regulator_is_enabled(reg)) {
			ret = 0;
		} else {
			ret = regulator_disable(reg);
			if (ret)
				pr_err("Failed to disable regulator, rc=%d",
					ret);
			else
				pr_debug("Regulator %p was disabled (%d)", reg,
					ret);
		}
	}

	pr_debug("Regulator set result: regulator=%p mode=%d ret=%d", reg, mode,
		ret);

	return ret;
}

/**
 * qca1530_pwr_init_regulator() - initialize power regulator if configured
 * @pdev: platform device data
 * @name: regulator name prefix in DTS file
 * @ppwr: pointer to resulting variable
 *
 * Function initializes power regulator if DTS configuration is present.
 *
 * Return: 0 on success, error code otherwise
 */
static int qca1530_pwr_init_regulator(
	struct platform_device *pdev,
	const char *name, struct regulator **ppwr)
{
	int ret;
	struct regulator *pwr;

	pwr = devm_regulator_get(&pdev->dev, name);
	if (IS_ERR_OR_NULL(pwr)) {
		ret = PTR_ERR(pwr);
		*ppwr = NULL;
		if (ret == -ENODEV) {
			pr_debug("Power regulator %s is not defined",
				name);
			ret = 0;
		} else
			pr_err("Failed to get regulator, ret=%d", ret);
	} else {
		pr_debug("Ref to regulator %s obtained: %p", name, pwr);
		*ppwr = pwr;
		ret = 0;
	}

	return ret;
}
/**
 * qca1530_power_set() - enable or disable SoC power
 * @mode: 1 to enable, 0 to disable
 *
 * Function enables or disables SoC power line.
 *
 * Return: 0 on success, error code otherwise
 */
static int qca1530_pwr_set(int mode)
{
	int ret = 0;
	if (!qca1530_data.pwr_reg &&
		!qca1530_data.pwr_opt_reg &&
		qca1530_data.pwr_gpio < 0) {
		ret = -ENOSYS;
	} else {
		if (qca1530_data.pwr_reg)
			ret = qca1530_pwr_set_regulator(
				qca1530_data.pwr_reg, mode);
		if (!ret && qca1530_data.pwr_opt_reg)
			ret = qca1530_pwr_set_regulator(
				qca1530_data.pwr_opt_reg, mode);
		if (!ret && qca1530_data.pwr_gpio >= 0)
			qca1530_pwr_set_gpio(mode);
	}
	return ret;
}

/**
 * qca1530_pwr_deinit() - release main SoC power control
 * @pdev: platform device data
 *
 * Function releases power regulator and power GPIO pin.
 */
static void qca1530_pwr_deinit(struct platform_device *pdev)
{
	qca1530_pwr_set(0);
	qca1530_deinit_gpio(pdev, &qca1530_data.pwr_gpio,
		GPIO_OPTIONAL_FLG | GPIO_OUTDIR_FLG);
	qca1530_deinit_regulator(&qca1530_data.pwr_opt_reg);
	qca1530_deinit_regulator(&qca1530_data.pwr_reg);
}

/**
 * qca1530_pwr_init() - allocate SoC power control
 * @pdev: platform device data
 *
 * Function allocates and enables SoC power control.
 *
 * Return: 0 on success, error code otherwise
 */
static int qca1530_pwr_init(struct platform_device *pdev)
{
	int ret = 0;

	pr_debug("Initializing power control");

	ret = qca1530_pwr_init_regulator(
		pdev, QCA1530_OF_PWR_REG_NAME, &qca1530_data.pwr_reg);
	if (ret)
		goto err_0;

	ret = qca1530_pwr_init_regulator(
		pdev, QCA1530_OF_PWR_OPT_REG_NAME, &qca1530_data.pwr_opt_reg);
	if (ret)
		goto err_0;

	ret = qca1530_init_gpio(pdev, &qca1530_data.pwr_gpio,
		QCA1530_OF_PWR_GPIO_NAME, GPIO_OPTIONAL_FLG | GPIO_OUTDIR_FLG);
	if (ret)
		goto err_0;

	if (qca1530_data.pwr_reg ||
		qca1530_data.pwr_gpio >= 0 ||
		qca1530_data.pwr_opt_reg) {
		ret = qca1530_pwr_set(1);
		if (ret) {
			pr_err("Failed to enable power, rc=%d", ret);
			goto err_0;
		}
		pr_debug("Configured: reg=%p gpio=%d",
			qca1530_data.pwr_reg,
			qca1530_data.pwr_gpio);
	} else {
		pr_debug("Power control is not available");
	}

	return ret;

err_0:
	qca1530_pwr_deinit(pdev);
	return ret;
}

/**
 * qca1530_reset_deinit() - release reset control resources
 * @pdev: platform device data
 *
 * Function releases reset line GPIO and switches off and releases power
 * regulator.
 */
static void qca1530_reset_deinit(struct platform_device *pdev)
{
	pr_debug("Releasing reset control");
	qca1530_deinit_gpio(pdev, &qca1530_data.reset_gpio,
		GPIO_OUTDIR_FLG);
	if (qca1530_data.reset_reg) {
		qca1530_pwr_set_regulator(qca1530_data.reset_reg, 0);
		qca1530_deinit_regulator(&qca1530_data.reset_reg);
	}
}

/**
 * qca1530_reset_init() - initialize SoC reset line resources
 * @pdev: platform device data
 *
 * Function initializes reset line resources, including configuration of GPIO
 * pin and power regulator.
 *
 * Return: 0 on success, error code otherwise
 */
static int qca1530_reset_init(struct platform_device *pdev)
{
	int ret;

	pr_debug("Initializing reset control");

	ret = qca1530_init_gpio(pdev, &qca1530_data.reset_gpio,
		QCA1530_OF_RESET_GPIO_NAME, GPIO_OUTDIR_FLG);

	if (ret < 0) {
		goto err_0;
	}

	ret = qca1530_pwr_init_regulator(
		pdev, QCA1530_OF_RESET_REG_NAME, &qca1530_data.reset_reg);
	if (ret)
		goto err_0;

	if (qca1530_data.reset_reg) {
		ret = qca1530_pwr_set_regulator(qca1530_data.reset_reg, 1);
		if (ret) {
			pr_err("failed to turn on reset regulator");
			goto err_0;
		}
	}

	return 0;

err_0:
	qca1530_reset_deinit(pdev);
	return ret;
}

/**
 * qca1530_xlna_set() - enables and disables xLNA
 * @mode: 0 to disable, 1 to enable
 *
 * Function controls xLNA. It configures power regulator and GPIO pin to
 * enable or disable the amplifier.
 *
 * Return: 0 on success, error code on error
 */
static int qca1530_xlna_set(int mode)
{
	int ret = 0;

	if (!qca1530_data.xlna_reg && qca1530_data.xlna_gpio < 0)
		ret = -ENOSYS;
	else if (mode) {
		if (qca1530_data.xlna_reg) {
			ret = qca1530_pwr_set_regulator(
				qca1530_data.xlna_reg, mode);
		}

		if (!ret && qca1530_data.xlna_gpio >= 0)
			gpio_set_value(qca1530_data.xlna_gpio, 1);
	} else {
		if (qca1530_data.xlna_gpio >= 0)
			gpio_set_value(qca1530_data.xlna_gpio, 0);
		if (qca1530_data.xlna_reg)
			ret = qca1530_pwr_set_regulator(
				qca1530_data.xlna_reg, mode);
	}
	return ret;
}

/**
 * qca1530_read_u32_arr_property() - read u32 values property
 * @pdev: platform device data
 * @pname: property name
 * @num_values: array size
 * @values: array to put results to
 *
 * Function reads property with given name. Filles passed array of
 * u32 values.
 *
 * Return: 0 on successful read, error code on error.
 */
static int qca1530_read_u32_arr_property(struct platform_device *pdev,
	const char *pname, const int num_values, u32 *values)
{
	int len = 0;
	int ret = 0;

	if (pdev->dev.of_node) {
		const void *rp = of_get_property(pdev->dev.of_node,
			pname, &len);
		if (NULL != rp && len == sizeof(u32)*num_values)
			ret = of_property_read_u32_array(pdev->dev.of_node,
				pname, values, num_values);
		else
			ret = -ENODATA;
	} else{
		ret = -EINVAL;
	}
	if (ret)
		pr_err("Error reading property %s, ret:%d", pname, ret);
	return ret;
}

/**
 * qca1530_xlna_deinit() - release all xLNA resources
 * @pdev: platform device data
 *
 * Function switches off xLNA and releases all allocated resources.
 */
static void qca1530_xlna_deinit(struct platform_device *pdev)
{
	qca1530_xlna_set(0);
	qca1530_deinit_gpio(pdev, &qca1530_data.xlna_gpio,
		GPIO_OPTIONAL_FLG | GPIO_OUTDIR_FLG);
	qca1530_deinit_regulator(&qca1530_data.xlna_reg);
}

/**
 * qca1530_xlna_init() - allocates xLNA resources
 * @pdev: platform device data
 *
 * Function allocates xLNA resources according to DTS configuration.
 * When configured, the following facilities are used:
 *   - power regulator (optionally)
 *   - GPIO pin (optionally)
 *
 * After initialization, xLNA is enabled.
 *
 * Return: 0 on successful initialization, error code on error.
 */
static int qca1530_xlna_init(struct platform_device *pdev)
{
	int ret = 0;
	u32 tmp[2];

	pr_debug("Initializing xLNA control");

	ret = qca1530_pwr_init_regulator(
		pdev, QCA1530_OF_XLNA_REG_NAME, &qca1530_data.xlna_reg);
	if (ret)
		goto err_0;

	if (qca1530_data.xlna_reg) {
		ret = qca1530_read_u32_arr_property(pdev,
			QCA1530_OF_XLNA_POWER_VOLTAGE, 2, tmp);
		if (!ret) {
			ret = regulator_set_voltage(qca1530_data.xlna_reg,
				tmp[0], tmp[1]);
			if (ret)
				pr_warn("Failed to set voltage, ret=%d", ret);
			else
				pr_debug("Regulator %p voltage was set (%d)",
					qca1530_data.xlna_reg, ret);
		}

		ret = qca1530_read_u32_arr_property(pdev,
			QCA1530_OF_XLNA_POWER_CURRENT, 2, tmp);
		if (!ret) {
			ret = regulator_set_optimum_mode(qca1530_data.xlna_reg,
				tmp[0]);
			if (ret < 0)
				pr_warn("Failed to set optimum mode, ret=%d",
					ret);
			else
				pr_debug("Optimum mode for %p was set (%d)",
					qca1530_data.xlna_reg, ret);
		}
	}

	ret = qca1530_init_gpio(pdev, &qca1530_data.xlna_gpio,
		QCA1530_OF_XLNA_GPIO_NAME, GPIO_OPTIONAL_FLG | GPIO_OUTDIR_FLG);
	if (ret)
		goto err_0;

	if (qca1530_data.xlna_reg || qca1530_data.xlna_gpio >= 0) {
		ret = qca1530_xlna_set(1);
		if (ret) {
			pr_err("Failed to enable xLNA, rc=%d", ret);
			goto err_0;
		}
		pr_debug("Configured: reg=%p gpio=%d",
			qca1530_data.xlna_reg,
			qca1530_data.xlna_gpio);
	} else {
		pr_debug("xLNA control is not available");
	}

	return ret;

err_0:
	qca1530_xlna_deinit(pdev);
	return ret;
}

/**
 * qca1530_set_chip_state() - performs driver initialization
 * @pdev: platform device data
 * @on: target state, 1 - on, 0 - off
 *
 * The driver probing includes initialization of the subsystems in the
 * following order:
 *   - reset control
 *   - RTC and TXCO Clock control
 *   - xLNA control
 *   - SoC power control
 */
static int qca1530_set_chip_state(struct platform_device *pdev,
	const int on_mask)
{
	int ret;
	int on_req;

	ret = 0;
	on_req = on_mask;
	pr_debug("on_mask=%d", on_mask);

	pr_debug("Switching on");

	if (!(qca1530_data.chip_state & QCA1530_RESET_FLG) &&
		(on_req & QCA1530_RESET_FLG)) {
		ret = qca1530_reset_init(pdev);
		if (ret < 0) {
			pr_err("failed to init reset: %d", ret);
			on_req = 0;
			goto switching_off;
		} else {
			qca1530_data.chip_state |= QCA1530_RESET_FLG;
		}
	}

	if (!(qca1530_data.chip_state & QCA1530_CLK_FLG) &&
		(on_req & QCA1530_CLK_FLG)) {
		ret = qca1530_clk_init(pdev);
		if (ret) {
			pr_err("failed to initialize clock: %d", ret);
			on_req = 0;
			goto switching_off;
		} else {
			qca1530_data.chip_state |= QCA1530_CLK_FLG;
		}
	}

	if (!(qca1530_data.chip_state & QCA1530_XLNA_FLG) &&
		(on_req & QCA1530_XLNA_FLG)) {
		ret = qca1530_xlna_init(pdev);
		if (ret) {
			pr_err("failed to initialize xLNA: %d", ret);
			on_req = 0;
			goto switching_off;
		} else {
			qca1530_data.chip_state |= QCA1530_XLNA_FLG;
		}
	}

	if (!(qca1530_data.chip_state & QCA1530_POWER_FLG) &&
		(on_req & QCA1530_POWER_FLG)) {
		ret = qca1530_pwr_init(pdev);
		if (ret < 0) {
			pr_err("failed to init power: %d", ret);
			on_req = 0;
			goto switching_off;
		} else {
			qca1530_data.chip_state |= QCA1530_POWER_FLG;
		}
	}

	pr_debug("Chip on section over, qca1530_data.chip_state=%d",
		qca1530_data.chip_state);

switching_off:
	pr_debug("Switching off");

	if ((qca1530_data.chip_state & QCA1530_POWER_FLG) &&
		!(on_req & QCA1530_POWER_FLG)) {
			qca1530_pwr_deinit(pdev);
			qca1530_data.chip_state &= ~QCA1530_POWER_FLG;
	}

	if ((qca1530_data.chip_state & QCA1530_XLNA_FLG) &&
		!(on_req & QCA1530_XLNA_FLG)) {
			qca1530_xlna_deinit(pdev);
			qca1530_data.chip_state &= ~QCA1530_XLNA_FLG;
	}

	if ((qca1530_data.chip_state & QCA1530_CLK_FLG) &&
		!(on_req & QCA1530_CLK_FLG)) {
			qca1530_clk_deinit(pdev);
			qca1530_data.chip_state &= ~QCA1530_CLK_FLG;
	}

	if ((qca1530_data.chip_state & QCA1530_RESET_FLG) &&
		!(on_req & QCA1530_RESET_FLG)) {
			qca1530_reset_deinit(pdev);
			qca1530_data.chip_state &= ~QCA1530_RESET_FLG;
	}

	pr_debug("Chip off section over, qca1530_data.chip_state=%d",
		qca1530_data.chip_state);

	return ret;
}

/**
 * qca1530_attr_chip_state_show() - provides current chip state through sysfs
 */
static ssize_t qca1530_attr_chip_state_show(struct kobject *kobj,
	struct kobj_attribute *attr,
	char *buf)
{
	return snprintf(buf, 16, "%d\n", qca1530_data.chip_state);
}

/**
 * qca1530_attr_chip_state_store() - sets new chip state
 */
static ssize_t qca1530_attr_chip_state_store(struct kobject *kobj,
	struct kobj_attribute *attr,
	const char *buf, size_t count)
{
	int retval;
	int new_state;

	sscanf(buf, "%d", &new_state);
	pr_debug("new_state=%d", new_state);
	if (count &&
		((new_state <= QCA1530_ALL_FLG) && (new_state >= 0))) {
		pr_debug("qca1530_data.chip_state=%d", qca1530_data.chip_state);
		retval = qca1530_set_chip_state(qca1530_data.pdev, new_state);
		pr_debug("qca1530_set_chip_state() returned %d", retval);
		pr_debug("qca1530_data.chip_state=%d", qca1530_data.chip_state);
	}

	return count;
}

/**
 * qca1530_attr_reset_show() - provides current reset state through sysfs
 */
static ssize_t qca1530_attr_reset_show(struct kobject *kobj,
	struct kobj_attribute *attr,
	char *buf)
{
	int v = -1;

	if (qca1530_data.chip_state & QCA1530_RESET_FLG)
		v = gpio_get_value(qca1530_data.reset_gpio);

	return snprintf(buf, 16, "%d\n", v);
}

/**
 * qca1530_attr_reset_store() - sets new reset state
 */
static ssize_t qca1530_attr_reset_store(struct kobject *kobj,
	struct kobj_attribute *attr,
	const char *buf, size_t count)
{
	int v;

	if (sscanf(buf, "%d", &v) == 1
		&& (qca1530_data.chip_state & QCA1530_RESET_FLG))
		gpio_set_value(qca1530_data.reset_gpio, v ? 1 : 0);

	return count;
}

/*
 * qca1530_attributes - sysfs attribute definition array
 */
static struct kobj_attribute qca1530_attributes[] = {
	__ATTR(chip_state,
		S_IRUSR | S_IWUSR,
		qca1530_attr_chip_state_show, qca1530_attr_chip_state_store),
	__ATTR(reset,
		S_IRUSR | S_IWUSR,
		qca1530_attr_reset_show, qca1530_attr_reset_store),
};

/*
 * qca1530_attrs - sysfs attributes for attribute group
 */
static struct attribute *qca1530_attrs[] = {
	&qca1530_attributes[0].attr,
	&qca1530_attributes[1].attr,
	NULL,
};

/*
 * qca1530_attr_group - driver sysfs attribute group
 */
static struct attribute_group qca1530_attr_group = {
	.attrs = qca1530_attrs,
};

static struct kobject *qca1530_kobject;

/**
 * qca1530_create_sysfs_node() - creates sysfs nodes for control
 *
 * Function exports two control nodes: for controlling chip control signals
 * and for reset control.
 */
static int qca1530_create_sysfs_node(void)
{
	int retval;

	pr_debug("Creating sysfs node");

	qca1530_kobject = kobject_create_and_add(SYSFS_NODE_NAME, kernel_kobj);
	pr_debug("qca1530_kobject=%p", qca1530_kobject);
	if (!qca1530_kobject)
		return -ENOMEM;

	retval = sysfs_create_group(qca1530_kobject, &qca1530_attr_group);
	pr_debug("sysfs_create_group() returned %d", retval);
	if (retval)
		kobject_put(qca1530_kobject);

	return retval;
}

/**
 * qca1530_remove_sysfs_node() - removes sysfs nodes
 */
static void qca1530_remove_sysfs_node(void)
{
	pr_debug("Removing sysfs node");
	if (qca1530_kobject) {
		sysfs_remove_group(qca1530_kobject, &qca1530_attr_group);
		kobject_put(qca1530_kobject);
		qca1530_kobject = NULL;
	}
}

/**
 * qca1530_probe() - performs driver initialization
 * @pdev: platform device data
 *
 * Function tried to turn on all required signals.
 * Refer to qca1530_set_chip_state() documentation for details.
 */
static int qca1530_probe(struct platform_device *pdev)
{
	int retval;

	pr_debug("Probing to install");

	retval = qca1530_set_chip_state(pdev, QCA1530_ALL_FLG);
	if (!retval) {
		qca1530_data.pdev = pdev;
		retval = qca1530_create_sysfs_node();
		if (retval) {
			qca1530_set_chip_state(pdev, 0);
			qca1530_data.pdev = NULL;
			pr_debug("qca1530_create_sysfs_node() returned %d",
			retval);
		}
	} else
		pr_debug("qca1530_set_chip_state() returned %d", retval);

	return retval;
}

/**
 * qca1530_remove() - releases all resources
 * @pdev: platform device data
 *
 * Driver's removal stops subsystems in the following order:
 *   - Power control
 *   - xLNA control
 *   - RTC and TCXO clock control
 *   - Reset control
 */
static int qca1530_remove(struct platform_device *pdev)
{
	int retval;
	qca1530_remove_sysfs_node();
	retval = qca1530_set_chip_state(pdev, 0);
	qca1530_data.pdev = NULL;
	return retval;
}

/*
 * qca1530_of_match - DTS driver name
 */
static struct of_device_id qca1530_of_match[] = {
	{.compatible = "qca,qca1530", },
	{ },
};

/*
 * qca1530_driver - driver registration data
 */
static struct platform_driver qca1530_driver = {
	.probe		= qca1530_probe,
	.remove         = qca1530_remove,
	.driver         = {
		.name = "qca1530",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(qca1530_of_match),
	},
};

/**
 * qca1530_init() - registers the driver
 */
static int __init qca1530_init(void)
{
	return platform_driver_register(&qca1530_driver);
}

/**
 * qca1530_exit() - unregisters the driver
 */
static void __exit qca1530_exit(void)
{
	platform_driver_unregister(&qca1530_driver);
}

module_init(qca1530_init);
module_exit(qca1530_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("qca1530 SoC chip driver");
MODULE_ALIAS("qca1530");

