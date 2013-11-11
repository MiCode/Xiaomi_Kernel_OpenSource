/* Copyright (c) 2013, The Linux Foundation. All rights reserved.
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

/* RTC clock name for lookup */
#define QCA1530_RTC_CLK_ID		"qca,rtc_clk"
/* TCXO clock name for lookup */
#define QCA1530_TCXO_CLK_ID		"qca,tcxo_clk"
/* SoC power regulator prefix for DTS */
#define QCA1530_OF_PWR_REG_NAME		"qca,pwr"
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
/* xLNA pin name for DTS */
#define QCA1530_OF_XLNA_GPIO_NAME	"qca,xlna-gpio"

/**
 * struct qca1530_static - keeps all driver instance variables
 * @pdev:          Platform device data
 * @reset_gpio:    Number of GPIO pin for reset control, or -1
 * @reset_reg:     Handle to power regulator for reset control, or NULL
 * @rtc_clk:       RTC clock handle (32KHz)
 * @rtc_clk_state: RTC clock state
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
	int                     rtc_clk_state;
	int			rtc_clk_gpio;
	struct clk		*tcxo_clk;
	struct regulator	*pwr_reg;
	int			pwr_gpio;
	struct regulator	*xlna_reg;
	int			xlna_gpio;
};

/*
 * qca1530_data - driver instance data
 */
static struct qca1530_static	qca1530_data = {
	.reset_gpio = -1,
	.rtc_clk_state = -1,
	.rtc_clk_gpio = -1,
	.pwr_gpio = -1,
	.xlna_gpio = -1,
};

/**
 * qca1530_deinit_gpio() - release GPIO resource
 * @pgpio: pointer to GPIO handle
 *
 * Function releases GPIO handle allocated by the driver. By default the
 * GPIO pin is removed from user space, switched to input direction and
 * then released.
 *
 * GPIO handle is set to -1.
 */
static void qca1530_deinit_gpio(int *pgpio)
{
	int gpio = *pgpio;

	if (gpio >= 0) {
		pr_debug("Releasing GPIO: %d", gpio);

		gpio_unexport(gpio);
		gpio_direction_input(gpio);
		gpio_free(gpio);
		*pgpio = -1;
	}
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
		regulator_put(*ppwr);
		*ppwr = NULL;
	}
}

/**
 * qca1530_clk_set_rtc() - enable or disable clock
 * @clk:  clock handle
 * @mode: 0 to disable, 1 to enable
 *
 * Function turns clock on or off and logs the result.
 *
 * Return: 0 on success, error code on failure
 */
static int qca1530_clk_set_rtc(struct clk *clk, int mode)
{
	int ret = 0;

	if (mode)
		ret = clk_prepare_enable(clk);
	else
		clk_disable_unprepare(clk);

	pr_debug("Configured clk (%p): mode=%d ret=%d", clk, mode, ret);

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

	pr_debug("Configured clk (GPIO): mode=%d", mode);
}

/**
 * qca1530_clk_set() - start/stop RTC clocks
 * @mode: 1 to start clocks, 0 to stop
 *
 * Function turns clocks on or off. Clock configuration is optional, however
 * when configured, the following order is used: GPIO pin, RTC clock, TCXO
 * clock. When switching off, the order is reveresed.
 *
 * Return: 0 on success, error code on failure.
 */
static int qca1530_clk_set(int mode)
{
	int ret = 0;

	if (qca1530_data.rtc_clk_gpio < 0 && !qca1530_data.rtc_clk
		&& !qca1530_data.tcxo_clk) {
		ret = -ENOSYS;
	} else if (mode) {
		if (qca1530_data.rtc_clk_gpio >= 0)
			qca1530_clk_set_gpio(1);
		if (qca1530_data.rtc_clk)
			ret = qca1530_clk_set_rtc(qca1530_data.rtc_clk, 1);
		if (!ret && qca1530_data.tcxo_clk)
			ret = qca1530_clk_set_rtc(qca1530_data.tcxo_clk, 1);
		qca1530_data.rtc_clk_state = ret ? 0 : 1;
	} else {
		if (qca1530_data.tcxo_clk)
			ret = qca1530_clk_set_rtc(qca1530_data.tcxo_clk, 0);
		if (!ret && qca1530_data.rtc_clk)
			ret = qca1530_clk_set_rtc(qca1530_data.rtc_clk, 0);
		if (!ret && qca1530_data.rtc_clk_gpio >= 0)
			qca1530_clk_set_gpio(0);
		qca1530_data.rtc_clk_state = 0;
	}

	pr_debug("Configured clk: mode=%d ret=%d", mode, ret);

	return ret;
}

/**
 * qca1530_clk_deinit_rtc() - release clocks
 * @pdev: platform device data
 *
 * Function sets clock handle references to NULL.
 */
static void qca1530_clk_deinit_rtc(struct platform_device *pdev)
{
	if (qca1530_data.tcxo_clk) {
		pr_debug("Unregistering CLK: device=%s name=%s",
			dev_name(&pdev->dev), QCA1530_TCXO_CLK_ID);
		qca1530_data.tcxo_clk = NULL;
	}
	if (qca1530_data.rtc_clk) {
		pr_debug("Unregistering CLK: device=%s name=%s",
			dev_name(&pdev->dev), QCA1530_RTC_CLK_ID);
		qca1530_data.rtc_clk = NULL;
	}
}

/**
 * qca1530_clk_init_gpio() - initialize optional GPIO control for RTC
 * @pdev: platform device data
 *
 * Function initializes optional GPIO pin for RTC control using DTS bindings.
 *
 * Return: 0 on success, error code on failure.
 */
static int qca1530_clk_init_gpio(struct platform_device *pdev)
{
	int ret;

	ret = of_get_named_gpio(pdev->dev.of_node, QCA1530_OF_CLK_GPIO_NAME, 0);
	if (ret == -ENOENT) {
		qca1530_data.rtc_clk_gpio = ret;
		ret = 0;
		pr_debug("GPIO is not defined");
	} else if (ret < 0) {
		pr_err("GPIO error: %d", ret);
	} else {
		qca1530_data.rtc_clk_gpio = ret;
		pr_debug("GPIO registered: gpio=%d", ret);
		ret = 0;
	}
	return ret;
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

	pr_debug("Clock initializing");

	qca1530_data.rtc_clk = devm_clk_get(&pdev->dev, QCA1530_RTC_CLK_ID);

	if (IS_ERR(qca1530_data.rtc_clk)) {
		ret = PTR_ERR(qca1530_data.rtc_clk);
		qca1530_data.rtc_clk = NULL;
		if (ret == -ENOENT) {
			pr_debug("No RTC clock controller");
		} else {
			pr_err("error: device=%s clock=%s ret=%d",
				dev_name(&pdev->dev), QCA1530_RTC_CLK_ID,
				ret);
			goto err_0;
		}
	}
	qca1530_data.tcxo_clk = devm_clk_get(&pdev->dev, QCA1530_TCXO_CLK_ID);
	if (IS_ERR(qca1530_data.tcxo_clk)) {
		ret = PTR_ERR(qca1530_data.tcxo_clk);
		qca1530_data.tcxo_clk = NULL;
		if (ret == -ENOENT) {
			pr_debug("No TCXO clock controller");
		} else {
			pr_err("error: device=%s clock=%s ret=%d",
				dev_name(&pdev->dev), QCA1530_TCXO_CLK_ID,
				ret);
			goto err_1;
		}
	}
	ret = qca1530_clk_init_gpio(pdev);
	if (ret)
		goto err_1;

	ret = qca1530_clk_set(1);
	if (ret < 0) {
		pr_err("error: ret=%d", ret);
		goto err_2;
	}

	pr_debug("init done: GPIO=%s RTC=%s TCXO=%s",
		qca1530_data.rtc_clk_gpio >= 0 ? "ok" : "unused",
		qca1530_data.rtc_clk ? "ok" : "unused",
		qca1530_data.tcxo_clk ? "ok" : "unused");

	return 0;
err_2:
	qca1530_deinit_gpio(&qca1530_data.rtc_clk_gpio);
err_1:
	qca1530_clk_deinit_rtc(pdev);
err_0:
	qca1530_data.rtc_clk_state = -1;

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
	qca1530_deinit_gpio(&qca1530_data.rtc_clk_gpio);
	qca1530_clk_deinit_rtc(pdev);
	qca1530_data.rtc_clk_state = -1;
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
		if (regulator_is_enabled(reg)) {
			ret = 0;
			goto done;
		}

		ret = regulator_set_mode(reg, REGULATOR_MODE_NORMAL);
		if (ret)
			pr_warn("failed to set regulator mode, ret=%d", ret);

		ret = regulator_enable(reg);
		if (ret) {
			pr_err("failed to enable regulator, rc=%d", ret);
			goto done;
		}
	} else {
		if (!regulator_is_enabled(reg)) {
			ret = 0;
			goto done;
		}

		ret = regulator_disable(reg);
		if (ret) {
			pr_err("failed to disable regulator, rc=%d", ret);
			goto done;
		}
	}
done:
	pr_debug("Regulator result: regulator=%p mode=%d ret=%d", reg, mode,
		ret);

	return ret;
}

/**
 * qca1530_pwr_init_gpio() - initialize power-related GPIO
 * @pdev: platform device data
 *
 * Return: 0 on success, error code otherwise
 */
static int qca1530_pwr_init_gpio(struct platform_device *pdev)
{
	int ret;

	ret = of_get_named_gpio(pdev->dev.of_node, QCA1530_OF_PWR_GPIO_NAME, 0);
	if (ret == -ENOENT) {
		qca1530_data.pwr_gpio = ret;
		ret = 0;
		pr_debug("Power control GPIO is not defined");
	} else if (ret < 0) {
		pr_err("Power control GPIO error: %d", ret);
	} else {
		qca1530_data.pwr_gpio = ret;
		ret = 0;
	}
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
	struct platform_device *pdev, const char *name, struct regulator **ppwr)
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
	if (!qca1530_data.pwr_reg && qca1530_data.pwr_gpio < 0) {
		ret = -ENOSYS;
	} else if (mode) {
		if (qca1530_data.pwr_reg)
			ret = qca1530_pwr_set_regulator(
				qca1530_data.pwr_reg, mode);
		if (!ret && qca1530_data.pwr_gpio >= 0)
			qca1530_pwr_set_gpio(mode);
	} else {
		if (qca1530_data.pwr_gpio >= 0)
			qca1530_pwr_set_gpio(mode);
		if (!ret && qca1530_data.pwr_reg)
			ret = qca1530_pwr_set_regulator(
				qca1530_data.pwr_reg, mode);
	}
	return ret;
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

	ret = qca1530_pwr_init_gpio(pdev);
	if (ret)
		goto err_1;

	if (qca1530_data.pwr_reg || qca1530_data.pwr_gpio >= 0) {
		ret = qca1530_pwr_set(1);
		if (ret) {
			pr_err("Failed to enable power, rc=%d", ret);
			goto err_2;
		}
		pr_debug("Configured: reg=%p gpio=%d",
			qca1530_data.pwr_reg,
			qca1530_data.pwr_gpio);
	} else {
		pr_debug("Power control is not available");
	}

	return ret;
err_2:
	qca1530_deinit_gpio(&qca1530_data.pwr_gpio);
err_1:
	qca1530_deinit_regulator(&qca1530_data.pwr_reg);
err_0:
	return ret;
}

/**
 * qca1530_pwr_deinit() - release main SoC power control
 *
 * Function releases power regulator and power GPIO pin.
 */
static void qca1530_pwr_deinit(void)
{
	qca1530_pwr_set(0);
	qca1530_deinit_gpio(&qca1530_data.pwr_gpio);
	qca1530_deinit_regulator(&qca1530_data.pwr_reg);
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

	pr_debug("reset control: initializing");

	ret = of_get_named_gpio(pdev->dev.of_node, QCA1530_OF_RESET_GPIO_NAME,
				0);
	if (ret < 0) {
		pr_err("failed to get gpio from config: %d", ret);
		goto err_gpio_get;
	}

	qca1530_data.reset_gpio = ret;
	ret = devm_gpio_request(&pdev->dev, qca1530_data.reset_gpio,
				"qca1530-reset");
	if (ret < 0) {
		pr_err("failed to request gpio-%d", qca1530_data.reset_gpio);
		goto err_gpio_get;
	}

	ret = gpio_direction_output(qca1530_data.reset_gpio, 0);
	if (ret < 0) {
		pr_err("failed to change direction for gpio-%d",
			qca1530_data.reset_gpio);
		goto err_gpio_configure;
	}

	ret = gpio_export(qca1530_data.reset_gpio, false);
	if (ret < 0) {
		pr_err("failed to export gpio-%d for user",
			qca1530_data.reset_gpio);
		goto err_gpio_configure;
	}

	ret = qca1530_pwr_init_regulator(
		pdev, QCA1530_OF_RESET_REG_NAME, &qca1530_data.reset_reg);
	if (ret)
		goto err_gpio_configure;

	if (qca1530_data.reset_reg) {
		ret = qca1530_pwr_set_regulator(qca1530_data.reset_reg, 1);
		if (ret) {
			pr_err("failed to turn on reset regulator");
			goto err_supply_configure;
		}
	}

	return 0;
err_supply_configure:
	qca1530_deinit_regulator(&qca1530_data.reset_reg);
err_gpio_configure:
	qca1530_deinit_gpio(&qca1530_data.reset_gpio);
err_gpio_get:
	return ret;
}

/**
 * qca1530_reset_deinit() - release reset control resources
 *
 * Function releases reset line GPIO and switches off and releases power
 * regulator.
 */
static void qca1530_reset_deinit(void)
{
	pr_debug("reset control: releasing");
	qca1530_deinit_gpio(&qca1530_data.reset_gpio);
	if (qca1530_data.reset_reg) {
		qca1530_pwr_set_regulator(qca1530_data.reset_reg, 0);
		qca1530_deinit_regulator(&qca1530_data.reset_reg);
	}
}

/**
 * qca1530_xlna_init_gpio() - initialize xLNA GPIO if configured
 * @pdev: platform device data
 *
 * The function allocates xLNA control GPIO if specified in DTS.
 *
 * Return: 0 on success, error code on error
 */
static int qca1530_xlna_init_gpio(struct platform_device *pdev)
{
	int ret;

	ret = of_get_named_gpio(pdev->dev.of_node, QCA1530_OF_XLNA_GPIO_NAME,
				0);
	if (ret == -ENOENT) {
		qca1530_data.xlna_gpio = -1;
		ret = 0;
		pr_debug("xLNA control: GPIO is not defined");
	} else if (ret < 0) {
		pr_err("xLNA control: GPIO error: %d", ret);
	} else {
		qca1530_data.xlna_gpio = ret;
		ret = 0;
	}
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
		if (qca1530_data.xlna_reg)
			ret = qca1530_pwr_set_regulator(
				qca1530_data.xlna_reg, mode);
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
 * qca1530_xlna_init() - allocates xLNA resources
 * @pdev: platform device data
 *
 * Function allocates xLNA resources according to DTS configuration. When
 * configured, the following facilities are used:
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

	pr_debug("xLNA control: initializing");

	ret = qca1530_pwr_init_regulator(
		pdev, QCA1530_OF_XLNA_REG_NAME, &qca1530_data.xlna_reg);
	if (ret)
		goto err_0;

	ret = qca1530_xlna_init_gpio(pdev);
	if (ret)
		goto err_1;

	if (qca1530_data.xlna_reg || qca1530_data.xlna_gpio >= 0) {
		ret = qca1530_xlna_set(1);
		if (ret) {
			pr_err("Failed to enable xLNA, rc=%d", ret);
			goto err_2;
		}
		pr_debug("Configured: reg=%p gpio=%d",
			qca1530_data.xlna_reg,
			qca1530_data.xlna_gpio);
	} else {
		pr_debug("xLNA control is not available");
	}

	return ret;
err_2:
	qca1530_deinit_gpio(&qca1530_data.xlna_gpio);
err_1:
	qca1530_deinit_regulator(&qca1530_data.xlna_reg);
err_0:
	return ret;
}

/**
 * qca1530_xlna_deinit() - release all xLNA resources
 *
 * Function switches off xLNA and releases all allocated resources.
 */
static void qca1530_xlna_deinit(void)
{
	qca1530_xlna_set(0);
	qca1530_deinit_gpio(&qca1530_data.xlna_gpio);
	qca1530_deinit_regulator(&qca1530_data.xlna_reg);
}

/**
 * qca1530_probe() - performs driver initialization
 * @pdev: platform device data
 *
 * The driver probing includes initialization of the subsystems in the
 * following order:
 *   - reset control
 *   - RTC and TXCO Clock control
 *   - xLNA control
 *   - SoC power control
 */
static int qca1530_probe(struct platform_device *pdev)
{
	int ret;

	ret = qca1530_reset_init(pdev);
	if (ret < 0) {
		pr_err("failed to init reset: %d", ret);
		goto err_reset_init;
	}

	ret = qca1530_clk_init(pdev);
	if (ret) {
		pr_err("failed to initialize clock: %d", ret);
		goto err_clk_init;
	}

	ret = qca1530_xlna_init(pdev);
	if (ret) {
		pr_err("failed to initialize xLNA: %d", ret);
		goto err_xlna_init;
	}

	ret = qca1530_pwr_init(pdev);
	if (ret < 0) {
		pr_err("failed to init power: %d", ret);
		goto err_pwr_init;
	}

	qca1530_data.pdev = pdev;

	pr_debug("Probe OK");

	return ret;

err_pwr_init:
	qca1530_xlna_deinit();
err_xlna_init:
	qca1530_clk_deinit(pdev);
err_clk_init:
	qca1530_reset_deinit();
err_reset_init:
	return ret;
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
	qca1530_pwr_deinit();
	qca1530_xlna_deinit();
	qca1530_clk_deinit(pdev);
	qca1530_reset_deinit();
	qca1530_data.pdev = NULL;
	return 0;
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

