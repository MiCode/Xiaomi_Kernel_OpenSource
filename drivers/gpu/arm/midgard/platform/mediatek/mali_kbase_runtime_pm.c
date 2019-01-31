/*
 * Copyright (c) 2017 MediaTek Inc.
 * Author: Chiawen Lee <chiawen.lee@mediatek.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */


#include <mali_kbase.h>
#include <mali_kbase_defs.h>
#include <linux/pm_runtime.h>
#include <linux/suspend.h>
#include <linux/of_platform.h>
#include <linux/of_address.h>
#include "mali_kbase_config_platform.h"
#include <linux/devfreq_cooling.h>
#include <backend/gpu/mali_kbase_power_model_simple.h>


static int pm_callback_power_on(struct kbase_device *kbdev)
{
	struct mfg_base *mfg = (struct mfg_base *)kbdev->platform_context;

	pm_runtime_get_sync(kbdev->dev);
	clk_prepare_enable(mfg->mfg_pll);
	clk_prepare_enable(mfg->mfg_sel);
	clk_prepare_enable(mfg->mfg_bg3d);

	return 1;
}

static void pm_callback_power_off(struct kbase_device *kbdev)
{
	struct mfg_base *mfg = (struct mfg_base *)kbdev->platform_context;

	clk_disable_unprepare(mfg->mfg_bg3d);
	clk_disable_unprepare(mfg->mfg_sel);
	clk_disable_unprepare(mfg->mfg_pll);
	pm_runtime_put_sync(kbdev->dev);
}

int kbase_device_runtime_init(struct kbase_device *kbdev)
{
	return 0;
}

void kbase_device_runtime_disable(struct kbase_device *kbdev)
{
}

static int pm_callback_runtime_on(struct kbase_device *kbdev)
{
	return 0;
}

static void pm_callback_runtime_off(struct kbase_device *kbdev)
{
}

static void pm_callback_resume(struct kbase_device *kbdev)
{
}

static void pm_callback_suspend(struct kbase_device *kbdev)
{
	pm_callback_runtime_off(kbdev);
}

struct kbase_pm_callback_conf pm_callbacks = {
	.power_on_callback = pm_callback_power_on,
	.power_off_callback = pm_callback_power_off,
	.power_suspend_callback = pm_callback_suspend,
	.power_resume_callback = pm_callback_resume,
#ifdef KBASE_PM_RUNTIME
	.power_runtime_init_callback = kbase_device_runtime_init,
	.power_runtime_term_callback = kbase_device_runtime_disable,
	.power_runtime_on_callback = pm_callback_runtime_on,
	.power_runtime_off_callback = pm_callback_runtime_off,
#else				/* KBASE_PM_RUNTIME */
	.power_runtime_init_callback = NULL,
	.power_runtime_term_callback = NULL,
	.power_runtime_on_callback = NULL,
	.power_runtime_off_callback = NULL,
#endif				/* KBASE_PM_RUNTIME */
};

int mali_mfgsys_init(struct kbase_device *kbdev, struct mfg_base *mfg)
{
	int err = 0;
	u32 mp = 0;

	mfg->reg_base = of_iomap(kbdev->dev->of_node, 1);
	if (!mfg->reg_base)
		return -ENOMEM;

	mfg->mfg_pll = devm_clk_get(kbdev->dev, "mfg_pll");
	if (IS_ERR(mfg->mfg_pll)) {
		err = PTR_ERR(mfg->mfg_pll);
		dev_err(kbdev->dev, "devm_clk_get mfg_pll failed\n");
		goto err_iounmap_reg_base;
	}

	mfg->mfg_sel = devm_clk_get(kbdev->dev, "mfg_sel");
	if (IS_ERR(mfg->mfg_sel)) {
		err = PTR_ERR(mfg->mfg_sel);
		dev_err(kbdev->dev, "devm_clk_get mfg_sel failed\n");
		goto err_iounmap_reg_base;
	}

	mfg->mfg_bg3d = devm_clk_get(kbdev->dev, "mfg_bg3d");
	if (IS_ERR(mfg->mfg_bg3d)) {
		err = PTR_ERR(mfg->mfg_bg3d);
		dev_notice(kbdev->dev, "devm_clk_get mfg_bg3d failed\n");
		goto err_iounmap_reg_base;
	}

	of_property_read_u32(kbdev->dev->of_node, "mp", &mp);
	if (mp == 2) /* 2 core */
		mfg->gpu_core_mask = (u64)0x3;
	else /* 4 core */
		mfg->gpu_core_mask = (u64)0xf;

	return 0;

err_iounmap_reg_base:
	iounmap(mfg->reg_base);
	return err;
}

void mali_mfgsys_deinit(struct kbase_device *kbdev)
{
	struct mfg_base *mfg = (struct mfg_base *)kbdev->platform_context;

	iounmap(mfg->reg_base);
}


static int platform_init(struct kbase_device *kbdev)
{
	int err;
	struct mfg_base *mfg;

	if (!kbdev->dev->pm_domain)
		return -EPROBE_DEFER;

	mfg = devm_kzalloc(kbdev->dev, sizeof(*mfg), GFP_KERNEL);
	if (!mfg)
		return -ENOMEM;

	err = mali_mfgsys_init(kbdev, mfg);
	if (err)
		return err;

	kbdev->platform_context = mfg;
	pm_runtime_enable(kbdev->dev);
	err = clk_set_rate(mfg->mfg_pll, GPU_FREQ_KHZ_MAX * 1000);
	if (err) {
		dev_err(kbdev->dev, "Failed to set clock %d kHz\n",
				GPU_FREQ_KHZ_MAX);
		return err;
	}

	return err;
}

static void platform_term_func(struct kbase_device *kbdev)
{
	mali_mfgsys_deinit(kbdev);
}

struct kbase_platform_funcs_conf platform_funcs = {
	.platform_init_func = platform_init,
	.platform_term_func = platform_term_func
};

#ifdef CONFIG_MALI_DEVFREQ
#ifdef CONFIG_DEVFREQ_THERMAL
#define FALLBACK_STATIC_TEMPERATURE 65000

static struct thermal_zone_device *gpu_tz;

/* Temperatures on power over-temp-and-voltage curve (C) */
static const int vt_temperatures[] = { 25, 65, 85, 105 };

/* Voltages on power over-temp-and-voltage curve (mV) */
static const int vt_voltages[] = { 1000 };

#define POWER_TABLE_NUM_TEMP ARRAY_SIZE(vt_temperatures)
#define POWER_TABLE_NUM_VOLT ARRAY_SIZE(vt_voltages)

static const unsigned int
power_table[POWER_TABLE_NUM_VOLT][POWER_TABLE_NUM_TEMP] = {
	/*   25     65      85     105 */
	{ 25000, 108800, 212660, 402360 },  /* 1000 mV */
};

static const int f_range[] = {253500000, 299000000, 396500000, 455000000, 494000000, 520000000};
static const u32 max_dynamic_power[] = {950, 1121, 1486, 1706, 1852, 1950};

static u32 interpolate(int value, const int *x, const unsigned int *y, int len)
{
	u64 tmp64;
	u32 dx;
	u32 dy;
	int i, ret;

	if (value <= x[0])
		return y[0];
	if (value >= x[len - 1])
		return y[len - 1];

	for (i = 1; i < len - 1; i++) {
		/* If value is identical, no need to interpolate */
		if (value == x[i])
			return y[i];
		if (value < x[i])
			break;
	}

	/* Linear interpolation between the two (x,y) points */
	dy = y[i] - y[i - 1];
	dx = x[i] - x[i - 1];

	tmp64 = value - x[i - 1];
	tmp64 *= dy;
	do_div(tmp64, dx);
	ret = y[i - 1] + tmp64;

	return ret;
}

static unsigned long model_static_power(unsigned long voltage)
{
	unsigned long power;
	int temperature = FALLBACK_STATIC_TEMPERATURE;
	int low_idx = 0, high_idx = POWER_TABLE_NUM_VOLT - 1;
	int i;

	if (gpu_tz->ops->get_temp(gpu_tz, &temperature))
		pr_warn_ratelimited("Failed to read temperature\n");

	do_div(temperature, 1000);

	for (i = 0; i < POWER_TABLE_NUM_VOLT; i++) {
		if (voltage <= vt_voltages[POWER_TABLE_NUM_VOLT - 1 - i])
			high_idx = POWER_TABLE_NUM_VOLT - 1 - i;

		if (voltage >= vt_voltages[i])
			low_idx = i;
	}

	if (low_idx == high_idx) {
		power = interpolate(temperature,
				    vt_temperatures,
				    &power_table[low_idx][0],
				    POWER_TABLE_NUM_TEMP);
	} else {
		unsigned long dvt =
				vt_voltages[high_idx] - vt_voltages[low_idx];
		unsigned long power1, power2;

		power1 = interpolate(temperature,
				     vt_temperatures,
				     &power_table[high_idx][0],
				     POWER_TABLE_NUM_TEMP);

		power2 = interpolate(temperature,
				     vt_temperatures,
				     &power_table[low_idx][0],
				     POWER_TABLE_NUM_TEMP);

		power = (power1 - power2) * (voltage - vt_voltages[low_idx]);
		do_div(power, dvt);
		power += power2;
	}

	/* convert to mw */
	do_div(power, 1000);

	return power;

}

static unsigned long model_dynamic_power(unsigned long freq,
		unsigned long voltage)
{
	#define NUM_RANGE  ARRAY_SIZE(f_range)
	/** Frequency and Power in Khz and mW respectively */
	s32 i, low_idx = 0, high_idx = NUM_RANGE - 1;
	u32 power;

	for (i = 0; i < NUM_RANGE; i++) {
		if (freq <= f_range[NUM_RANGE - 1 - i])
			high_idx = NUM_RANGE - 1 - i;

		if (freq >= f_range[i])
			low_idx = i;
	}

	if (low_idx == high_idx) {
		power = max_dynamic_power[low_idx];
	} else {
		u32 f_interval = f_range[high_idx] - f_range[low_idx];
		u32 p_interval = max_dynamic_power[high_idx] -
				max_dynamic_power[low_idx];

		power = p_interval * (freq - f_range[low_idx]);
		do_div(power, f_interval);
		power += max_dynamic_power[low_idx];
	}

	power = (u32)(((u64)power * voltage * voltage) / 1000000);

	return power;
	#undef NUM_RANGE
}

struct devfreq_cooling_power power_model_simple_ops = {
	.get_static_power = model_static_power,
	.get_dynamic_power = model_dynamic_power,
};

int kbase_power_model_simple_init(struct kbase_device *kbdev)
{
	struct device_node *power_model_node;
	const char *tz_name;

	power_model_node = of_get_child_by_name(kbdev->dev->of_node,
			"power_model");
	if (!power_model_node) {
		dev_err(kbdev->dev, "could not find power_model node\n");
		return -ENODEV;
	}
	if (!of_device_is_compatible(power_model_node,
			"arm,mali-simple-power-model")) {
		dev_err(kbdev->dev, "power_model incompatible with simple power model\n");
		return -ENODEV;
	}

	if (of_property_read_string(power_model_node, "thermal-zone",
			&tz_name)) {
		dev_err(kbdev->dev, "ts in power_model not available\n");
		return -EINVAL;
	}

	gpu_tz = thermal_zone_get_zone_by_name(tz_name);
	if (IS_ERR(gpu_tz)) {
		pr_warn_ratelimited("Error getting gpu thermal zone (%ld), not yet ready?\n",
				PTR_ERR(gpu_tz));
		gpu_tz = NULL;

		return -EPROBE_DEFER;
	}

	return 0;

}
#endif
#endif

