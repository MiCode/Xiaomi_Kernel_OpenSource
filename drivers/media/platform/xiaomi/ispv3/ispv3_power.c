/*
 * Copyright (c) 2020, Xiaomi, Inc. All rights reserved.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/gpio/driver.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/of_device.h>
#include <linux/of_platform.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/genalloc.h>
#include <linux/mfd/ispv3_dev.h>

static int ispv3_regulator_enable(struct ispv3_data *data,
				  enum ispv3_rgltr_type type)
{
	int32_t ret = 0;

	if (!data->rgltr[type]) {
		dev_err(data->dev, "Invalid NULL parameter\n");
		return -EINVAL;
	}

	if (regulator_count_voltages(data->rgltr[type]) > 0) {
		dev_dbg(data->dev, "rgltr_name %s voltage min=%d, max=%d",
			data->rgltr_name[type], data->rgltr_min_volt[type],
			data->rgltr_max_volt[type]);

		ret = regulator_set_voltage(data->rgltr[type],
			data->rgltr_min_volt[type], data->rgltr_max_volt[type]);
		if (ret) {
			dev_err(data->dev, "%s set voltage failed\n",
				data->rgltr_name[type]);
			return ret;
		}
	}

	ret = regulator_enable(data->rgltr[type]);
	if (ret) {
		dev_err(data->dev, "%s regulator_enable failed\n",
			data->rgltr_name[type]);
		return ret;
	}

	return ret;
}

static int ispv3_regulator_disable(struct ispv3_data *data,
				   enum ispv3_rgltr_type type)
{
	int32_t ret = 0;

	if (!data->rgltr[type]) {
		dev_err(data->dev, "Invalid NULL parameter\n");
		return -EINVAL;
	}

	ret = regulator_disable(data->rgltr[type]);
	if (ret) {
		dev_err(data->dev, "%s regulator disable failed\n",
			data->rgltr_name[type]);
		return ret;
	}

	if (regulator_count_voltages(data->rgltr[type]) > 0) {
		//regulator_set_load(data->rgltr[type], 0);
		regulator_set_voltage(data->rgltr[type], 0,
				      data->rgltr_max_volt[type]);
	}

	return ret;
}

static int ispv3_clk_enable(struct ispv3_data *data)
{
	long clk_rate_round;
	int ret = 0;

	if (!data->clk || !data->clk_name)
		return -EINVAL;

	clk_rate_round = clk_round_rate(data->clk, data->clk_rate);
	if (clk_rate_round < 0) {
		dev_err(data->dev, "round failed for clock %s ret = %ld",
			data->clk_name, clk_rate_round);
		return clk_rate_round;
	}

	ret = clk_set_rate(data->clk, clk_rate_round);
	if (ret) {
		dev_err(data->dev, "set_rate failed on %s", data->clk_name);
		return ret;
	}
	dev_dbg(data->dev, "set %s, rate %d, new_rate %ld", data->clk_name,
		data->clk_rate, clk_rate_round);

	ret = clk_prepare_enable(data->clk);
	if (ret) {
		dev_err(data->dev, "enable failed for %s: ret(%d)", data->clk_name, ret);
		return ret;
	}

	return ret;
}

static int ispv3_clk_disable(struct ispv3_data *data)
{
	if (!data->clk || !data->clk_name)
		return -EINVAL;

	clk_disable_unprepare(data->clk);

	return 0;
}

void ispv3_sys_reset(struct ispv3_data *data, u32 value)
{
	if (data->gpio_sys_reset > 0)
		gpio_set_value(data->gpio_sys_reset, value);
}

void ispv3_swcr_isolation(struct ispv3_data *data, u32 value)
{
	if (data->gpio_isolation > 0)
		gpio_set_value(data->gpio_isolation, value);
}

void ispv3_swcr_reset(struct ispv3_data *data, u32 value)
{
	if (data->gpio_swcr_reset > 0)
		gpio_set_value(data->gpio_swcr_reset, value);
}

void ispv3_fan_enable(struct ispv3_data *data, u32 value)
{
	if (data->gpio_fan_en > 0)
		gpio_set_value(data->gpio_fan_en, value);
}

static int ispv3_set_pci_config_space(struct ispv3_data *data, bool save)
{
	int ret = 0;

	if (save) {
		ret = pci_save_state(data->pci);
		data->saved_state = pci_store_saved_state(data->pci);
	} else {
		if (data->saved_state)
			pci_load_and_free_saved_state(data->pci, &data->saved_state);
		else
			pci_load_saved_state(data->pci, data->default_state);

		pci_restore_state(data->pci);
	}

	return ret;
}

static int ispv3_set_pci_link(struct ispv3_data *data, bool link_up)
{
	enum msm_pcie_pm_opt pm_ops;
	int retry_time = 0;
	int ret = 0;

	if (link_up)
		pm_ops = MSM_PCIE_RESUME;
	else
		pm_ops = MSM_PCIE_SUSPEND;

retry:
	ret = msm_pcie_pm_control(pm_ops, data->pci->bus->number, data->pci,
				  NULL, PM_OPTIONS_DEFAULT);
	if (ret) {
		dev_err(data->dev, "Failed to %s PCI link with default option, err = %d\n",
			link_up ? "resume" : "suspend", ret);
		if (link_up && retry_time++ < LINK_TRAINING_RETRY_MAX_TIMES) {
			dev_dbg(data->dev, "Retry PCI link training #%d\n",
				retry_time);
			goto retry;
		}
	}

	return ret;
}

int ispv3_resume_pci_link(struct ispv3_data *data)
{
	int ret = 0;

	if (!data->pci)
		return -ENODEV;

	ret = ispv3_set_pci_link(data, ISPV3_PCI_LINK_UP);
	if (ret) {
		dev_err(data->dev, "Failed to set link up status, err =  %d\n",
			ret);
		return ret;
	}

	ret = pci_enable_device(data->pci);
	if (ret) {
		dev_err(data->dev, "Failed to enable PCI device, err = %d\n",
			ret);
		return ret;
	}

	ret = ispv3_set_pci_config_space(data, RESTORE_PCI_CONFIG_SPACE);
	if (ret) {
		dev_err(data->dev, "Failed to restore config space, err = %d\n",
			ret);
		return ret;
	}

	pci_set_master(data->pci);

	atomic_set(&data->pci_link_state, ISPV3_PCI_LINK_UP);

	return 0;
}
EXPORT_SYMBOL_GPL(ispv3_resume_pci_link);

int ispv3_suspend_pci_link(struct ispv3_data *data)
{
	int ret = 0;

	if (!data->pci)
		return -ENODEV;

	pci_clear_master(data->pci);

	ret = ispv3_set_pci_config_space(data, SAVE_PCI_CONFIG_SPACE);
	if (ret) {
		dev_err(data->dev, "Failed to save config space, err =  %d\n",
			ret);
		return ret;
	}

	pci_disable_device(data->pci);

	mutex_lock(&data->ispv3_interf_mutex);
	ret = pci_set_power_state(data->pci, PCI_D3hot);
	mutex_unlock(&data->ispv3_interf_mutex);
	if (ret) {
		dev_err(data->dev, "Failed to set D3Hot, err =  %d\n", ret);
		return ret;
	}

	ret = ispv3_set_pci_link(data, ISPV3_PCI_LINK_DOWN);
	if (ret) {
		dev_err(data->dev, "Failed to set link down status, err =  %d\n",
			ret);
		return ret;
	}

	atomic_set(&data->pci_link_state, ISPV3_PCI_LINK_DOWN);

	return 0;
}
EXPORT_SYMBOL_GPL(ispv3_suspend_pci_link);

void ispv3_gpio_reset_clear(struct ispv3_data *data)
{
	ispv3_swcr_isolation(data, 0);
	udelay(100);
	ispv3_swcr_reset(data, 0);
	udelay(100);
	ispv3_sys_reset(data, 0);
	udelay(100);
}
EXPORT_SYMBOL_GPL(ispv3_gpio_reset_clear);

int ispv3_power_on(struct ispv3_data *data)
{
	int ret = 0;

	/* regulator enable and configure reset pin to work state */
	if (data->soc_id == ISPV3_SOC_ID_SM8475) {
		ret = ispv3_regulator_enable(data, ISPV3_RGLTR_S11B);
		if (ret) {
			dev_err(data->dev, "ISPV3_RGLTR_S11B regulator enable failed\n");
			return ret;
		}
		ret = ispv3_regulator_enable(data, ISPV3_RGLTR_S12B);
		if (ret) {
			dev_err(data->dev, "ISPV3_RGLTR_S12B regulator enable failed\n");
			return ret;
		}
	}

	ret = ispv3_regulator_enable(data, ISPV3_RGLTR_VDD1);
	if (ret) {
		dev_err(data->dev, "ISPV3_RGLTR_VDD1 regulator enable failed\n");
		return ret;
	}
	udelay(100);

	ret = ispv3_regulator_enable(data, ISPV3_RGLTR_L12C);
	if (ret) {
		dev_err(data->dev, "ISPV3_RGLTR_L12C regulator enable failed\n");
		return ret;
	}
	udelay(1);

	ispv3_fan_enable(data, 1);
	udelay(50);
	ispv3_swcr_isolation(data, 1);
	udelay(1);
	ispv3_swcr_reset(data, 1);
	udelay(50);

	ret = ispv3_regulator_enable(data, ISPV3_RGLTR_VDD2);
	if (ret) {
		dev_err(data->dev, "ISPV3_RGLTR_VDD2 regulator enable failed\n");
		return ret;
	}
	udelay(100);

	ret = ispv3_regulator_enable(data, ISPV3_RGLTR_VDD);
	if (ret) {
		dev_err(data->dev, "ISPV3_RGLTR_VDD regulator enable failed\n");
		return ret;
	}
	udelay(1);

	ispv3_swcr_isolation(data, 0);
	udelay(100);

	ret = ispv3_regulator_enable(data, ISPV3_RGLTR_VDDR);
	if (ret) {
		dev_err(data->dev, "ISPV3_RGLTR_VDDR regulator enable failed\n");
		return ret;
	}
	udelay(100);

	ret = ispv3_regulator_enable(data, ISPV3_RGLTR_L10C);
	if (ret) {
		dev_err(data->dev, "ISPV3_RGLTR_L10C regulator enable failed\n");
		return ret;
	}

	udelay(2500);
	if (data->pinctrl_info.pinctrl_status) {
		ret = pinctrl_select_state(
			data->pinctrl_info.pinctrl,
			data->pinctrl_info.gpio_state_active);
		if (ret) {
			dev_err(data->dev, "cannot set pin to active state\n");
			return ret;
		}
	}

	ret = ispv3_regulator_enable(data, ISPV3_RGLTR_MCLK);
	if (ret) {
		dev_err(data->dev, "ISPV3_RGLTR_MCLK regulator enable failed\n");
		return ret;
	}

	ret = ispv3_clk_enable(data);
	if (ret) {
		dev_err(data->dev, "mclk enable failed\n");
		return ret;
	}
	udelay(1);

	ispv3_sys_reset(data, 1);

#ifdef BUG_SOF
	atomic_set(&data->power_state, 0);
#else
	data->power_state = ISPV3_POWER_ON;
#endif
	dev_info(data->dev, "ispv3 power on success");

	return ret;
}
EXPORT_SYMBOL_GPL(ispv3_power_on);

int ispv3_power_off(struct ispv3_data *data)
{
	int ret = 0;

	/* regulator disable and configure reset pin to defaule value */
	ispv3_sys_reset(data, 0);
	udelay(1);
	ispv3_swcr_isolation(data, 1);

	ispv3_clk_disable(data);
	if (data->pinctrl_info.pinctrl_status) {
		ret = pinctrl_select_state(
			data->pinctrl_info.pinctrl,
			data->pinctrl_info.gpio_state_suspend);
		if (ret)
			dev_err(data->dev, "cannot set pin to suspend state\n");
	}

	ispv3_regulator_disable(data, ISPV3_RGLTR_MCLK);
	udelay(1);

	ispv3_regulator_disable(data, ISPV3_RGLTR_L10C);
	udelay(1);

	ispv3_regulator_disable(data, ISPV3_RGLTR_VDDR);
	udelay(1);

#ifndef CONFIG_ZISP_OCRAM_AON
	ispv3_swcr_reset(data, 0);
#endif

	ispv3_regulator_disable(data, ISPV3_RGLTR_VDD);
	udelay(1);

	ispv3_regulator_disable(data, ISPV3_RGLTR_VDD2);
	udelay(1);

	ispv3_fan_enable(data, 0);
	udelay(1);

#ifndef CONFIG_ZISP_OCRAM_AON
	ispv3_regulator_disable(data, ISPV3_RGLTR_L12C);
	udelay(1);

	ispv3_regulator_disable(data, ISPV3_RGLTR_VDD1);
	udelay(1);

	if (data->soc_id == ISPV3_SOC_ID_SM8475) {
		ispv3_regulator_disable(data, ISPV3_RGLTR_S11B);
		ispv3_regulator_disable(data, ISPV3_RGLTR_S12B);
	}
#endif

	ispv3_swcr_isolation(data, 0);
#ifdef BUG_SOF
	atomic_set(&data->power_state, 1);
#else
	data->power_state = ISPV3_POWER_OFF;
#endif
	dev_info(data->dev, "ispv3 power down success");

	return ret;
}
EXPORT_SYMBOL_GPL(ispv3_power_off);

MODULE_LICENSE("GPL v2");
