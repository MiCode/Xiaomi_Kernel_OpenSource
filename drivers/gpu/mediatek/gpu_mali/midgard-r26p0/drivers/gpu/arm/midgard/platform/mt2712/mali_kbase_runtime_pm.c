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
#include "mali_kbase_runtime_pm.h"
#include <linux/devfreq_cooling.h>

#define TOPCKGEN_MAP_START			(0x10000000)
#define MFG_MAP_START				(0x13000000)

static s32 pm_callback_power_on(struct kbase_device *kbdev)
{
	s32 ret;
	const struct mfg_base *mfg = (struct mfg_base *)kbdev->platform_context;

	ret = pm_runtime_get_sync(kbdev->dev);
	ret = clk_prepare_enable(mfg->mfg_pll);
	ret = clk_prepare_enable(mfg->mfg_sel);
	ret = clk_prepare_enable(mfg->mfg_bg3d);

	return 1;
}

static void pm_callback_power_off(struct kbase_device *kbdev)
{
	s32 ret;
	const struct mfg_base *mfg = (struct mfg_base *)kbdev->platform_context;

	clk_disable_unprepare(mfg->mfg_bg3d);
	clk_disable_unprepare(mfg->mfg_sel);
	clk_disable_unprepare(mfg->mfg_pll);
	ret = pm_runtime_put_sync(kbdev->dev);
}

struct kbase_pm_callback_conf pm_callbacks = {
	.power_on_callback = pm_callback_power_on,
	.power_off_callback = pm_callback_power_off,
	.power_suspend_callback = NULL,
	.power_resume_callback = NULL,
#ifdef KBASE_PM_RUNTIME
	.power_runtime_init_callback = NULL,
	.power_runtime_term_callback = NULL,
	.power_runtime_on_callback = NULL,
	.power_runtime_off_callback = NULL,
#else				/* KBASE_PM_RUNTIME */
	.power_runtime_init_callback = NULL,
	.power_runtime_term_callback = NULL,
	.power_runtime_on_callback = NULL,
	.power_runtime_off_callback = NULL,
#endif				/* KBASE_PM_RUNTIME */
};

/*MTK: enable mfg ecc */
void mfg_ecc_enable(const struct kbase_device *kbdev)
{
	void __iomem *ecc_enable_reg = NULL;
	void __iomem *ecc_reset_reg = NULL;
	u32 ecc_enable_reg_value = 0;
	u32 ecc_reset_reg_value = 0;

	ecc_enable_reg = ioremap(MFG_MAP_START, 0x80);
	if (ecc_enable_reg == NULL) {
		dev_notice(kbdev->dev, "unable to map MFG_MAP_START address space\n");
		return;
	}

	ecc_reset_reg = ioremap(TOPCKGEN_MAP_START, 0x1000);
	if (ecc_reset_reg == NULL) {
		iounmap(ecc_enable_reg);
		dev_notice(kbdev->dev, "unable to map TOPCKGEN_MAP_START address space\n");
		return;
	}

	ecc_enable_reg_value = readl(ecc_enable_reg + 0x80);
	ecc_reset_reg_value = readl(ecc_reset_reg + 0x0428);
	writel(ecc_enable_reg_value | 0x0000000F, ecc_enable_reg + 0x80);
	writel(ecc_reset_reg_value | 0x0000000F, ecc_reset_reg + 0x0428);

	/* check mfg ecc value */
	ecc_enable_reg_value = readl(ecc_enable_reg + 0x80);
	ecc_reset_reg_value = readl(ecc_reset_reg + 0x0428);
	if (((ecc_enable_reg_value & 0x0000000F) == 0x0000000F)
		&& ((ecc_reset_reg_value & 0x0000000F) == 0x0000000F)) {
		dev_notice(kbdev->dev, "mfg ecc support is enable\n");
	} else {
		dev_notice(kbdev->dev, "unable to enable mfg ecc support\n");
	}

	iounmap(ecc_enable_reg);
	iounmap(ecc_reset_reg);
}

static s32 mali_mfgsys_init(const struct kbase_device *kbdev,
				struct mfg_base *mfg)
{
	s32 err = 0;
	u32 mp = 0;

	mfg->reg_base = of_iomap(kbdev->dev->of_node, 1);
	if (mfg->reg_base == NULL)
		return -ENOMEM;

	mfg->mfg_pll = devm_clk_get(kbdev->dev, "mfg_pll");
	if (IS_ERR(mfg->mfg_pll)) {
		err = (s32)PTR_ERR(mfg->mfg_pll);
		dev_notice(kbdev->dev, "devm_clk_get mfg_pll failed\n");
		goto err_iounmap_reg_base;
	}

	mfg->mfg_sel = devm_clk_get(kbdev->dev, "mfg_sel");
	if (IS_ERR(mfg->mfg_sel)) {
		err = (s32)PTR_ERR(mfg->mfg_sel);
		dev_notice(kbdev->dev, "devm_clk_get mfg_sel failed\n");
		goto err_iounmap_reg_base;
	}

	mfg->mfg_bg3d = devm_clk_get(kbdev->dev, "mfg_bg3d");
	if (IS_ERR(mfg->mfg_bg3d)) {
		err = (s32)PTR_ERR(mfg->mfg_bg3d);
		dev_notice(kbdev->dev, "devm_clk_get mfg_bg3d failed\n");
		goto err_iounmap_reg_base;
	}

	err = of_property_read_u32(kbdev->dev->of_node, "mp", &mp);
	if (mp == 2UL) /* 2 core */
		mfg->gpu_core_mask = 0x3;
	else /* 4 core */
		mfg->gpu_core_mask = 0xf;

	mfg_ecc_enable(kbdev);

	return 0;

err_iounmap_reg_base:
	iounmap(mfg->reg_base);
	return err;
}

static void mali_mfgsys_deinit(const struct kbase_device *kbdev)
{
	const struct mfg_base *mfg = (struct mfg_base *)kbdev->platform_context;

	iounmap(mfg->reg_base);
}


static s32 platform_init(struct kbase_device *kbdev)
{
	s32 err;
	struct mfg_base *mfg;

	if (kbdev->dev->pm_domain == NULL)
		return -EPROBE_DEFER;

	mfg = devm_kzalloc(kbdev->dev, sizeof(*mfg), GFP_KERNEL);
	if (mfg == NULL)
		return -ENOMEM;

	err = mali_mfgsys_init(kbdev, mfg);
	if (err != 0)
		return err;

	kbdev->platform_context = mfg;
	pm_runtime_enable(kbdev->dev);
	err = clk_set_rate(mfg->mfg_pll, GPU_FREQ_KHZ_MAX * 1000);
	if (err != 0) {
		dev_notice(kbdev->dev, "Failed to set clock %d kHz\n",
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
static const s32 vt_temperatures[] = { 25, 65, 85, 105 };

/* Voltages on power over-temp-and-voltage curve (mV) */
static const u_long vt_voltages[] = { 1000 };

#define POWER_TABLE_NUM_TEMP ((u32)ARRAY_SIZE(vt_temperatures))
#define POWER_TABLE_NUM_VOLT ((u32)ARRAY_SIZE(vt_voltages))

static const s32
power_table[POWER_TABLE_NUM_VOLT][POWER_TABLE_NUM_TEMP] = {
	/*   25     65      85     105 */
	{ 25000, 108800, 212660, 402360 },  /* 1000 mV */
};

static const u32 f_range[] = {253500000, 299000000, 396500000,
	455000000, 494000000, 520000000};
static const u32 max_dynamic_power[] = {950, 1121, 1486, 1706, 1852, 1950};

static s32 interpolate(s32 value, const s32 *x, const s32 *y, u32 len)
{
	u64 tmp64;
	s32 dx;
	s32 dy;
	s32 ret;
	u32 i;

	if (value <= x[0])
		return y[0];
	if (value >= x[len - 1UL])
		return y[len - 1UL];

	for (i = 1; i < (len - 1UL); i++) {
		/* If value is identical, no need to interpolate */
		if (value == x[i])
			return y[i];
		if (value < x[i])
			break;
	}

	/* Linear interpolation between the two (x,y) points */
	dy = y[i] - y[i - 1UL];
	dx = x[i] - x[i - 1UL];

	tmp64 = (u64)value - (u64)x[i - 1UL];
	tmp64 *= (u64)dy;
	do_div(tmp64, (u32)dx);
	ret = y[i - 1UL] + (s32)tmp64;

	return ret;
}

static u_long model_static_power(struct devfreq *devfreq, u_long voltage)
{
	u_long power;
	u64 power_div;
	s32 temperature = FALLBACK_STATIC_TEMPERATURE;
	s64 temperature_div;
	u32 low_idx = 0, high_idx = POWER_TABLE_NUM_VOLT - 1U;
	u32 i;

	if (gpu_tz->ops->get_temp(gpu_tz, &temperature) != 0)
		pr_notice_ratelimited("Failed to read temperature\n");

	temperature_div = temperature;
	div_s64(temperature_div, 1000);
	temperature = (s32)temperature_div;

	for (i = 0; i < POWER_TABLE_NUM_VOLT; i++) {
		if (voltage <= vt_voltages[POWER_TABLE_NUM_VOLT - 1U - i])
			high_idx = POWER_TABLE_NUM_VOLT - 1U - i;

		if (voltage >= vt_voltages[i])
			low_idx = i;
	}

	if (low_idx == high_idx) {
		power = (u_long)interpolate(temperature,
				    vt_temperatures,
				    &power_table[low_idx][0],
				    POWER_TABLE_NUM_TEMP);
	} else {
		u_long dvt = vt_voltages[high_idx] - vt_voltages[low_idx];
		u_long power1, power2;

		power1 = (u_long)interpolate(temperature,
				     vt_temperatures,
				     &power_table[high_idx][0],
				     POWER_TABLE_NUM_TEMP);

		power2 = (u_long)interpolate(temperature,
				     vt_temperatures,
				     &power_table[low_idx][0],
				     POWER_TABLE_NUM_TEMP);

		power = (power1 - power2) * (voltage - vt_voltages[low_idx]);
		power_div = power;
		do_div(power_div, dvt);
		power = (u_long)power_div;
		power += power2;
	}

	/* convert to mw */
	power_div = power;
	do_div(power_div, 1000);
	power = (u_long)power_div;

	return power;

}

static u_long model_dynamic_power(struct devfreq *devfreq, u_long freq,
		u_long voltage)
{
	#define NUM_RANGE  ((u32)ARRAY_SIZE(f_range))
	/** Frequency and Power in Khz and mW respectively */
	u32 low_idx = 0, high_idx = NUM_RANGE - 1U;
	u32 power, i;
	u64 power_div;

	for (i = 0; i < NUM_RANGE; i++) {
		if ((u32)freq <= f_range[NUM_RANGE - 1U - i])
			high_idx = NUM_RANGE - 1U - i;

		if ((u32)freq >= f_range[i])
			low_idx = i;
	}

	if (low_idx == high_idx) {
		power = max_dynamic_power[low_idx];
	} else {
		u32 f_interval = f_range[high_idx] - f_range[low_idx];
		u32 p_interval = max_dynamic_power[high_idx] -
				max_dynamic_power[low_idx];

		power = p_interval * (u32)(freq - f_range[low_idx]);
		power_div = power;
		do_div(power_div, f_interval);
		power = (u32)power_div;
		power += max_dynamic_power[low_idx];
	}

	power_div = (u64)power * voltage * voltage;
	do_div(power_div, 1000000);
	power = (u32)power_div;

	return power;
	#undef NUM_RANGE
}

struct devfreq_cooling_power power_model_simple_ops = {
	.get_static_power = model_static_power,
	.get_dynamic_power = model_dynamic_power,
};

s32 kbase_power_model_simple_init(struct kbase_device *kbdev)
{
	struct device_node *power_model_node;
	const char *tz_name;

	power_model_node = of_get_child_by_name(kbdev->dev->of_node,
			"power_model");
	if (power_model_node == NULL) {
		dev_notice(kbdev->dev, "could not find power_model node\n");
		return -ENODEV;
	}
	if (of_device_is_compatible(power_model_node,
			"arm,mali-simple-power-model") == 0) {
		dev_notice(kbdev->dev, "power_model incompatible with simple power model\n");
		return -ENODEV;
	}

	if (of_property_read_string(power_model_node, "thermal-zone",
			&tz_name) != 0) {
		dev_notice(kbdev->dev, "ts in power_model not available\n");
		return -EINVAL;
	}

	gpu_tz = thermal_zone_get_zone_by_name(tz_name);
	if (IS_ERR(gpu_tz)) {
		pr_notice_ratelimited("Error getting gpu thermal zone (%ld), not yet ready?\n",
				PTR_ERR(gpu_tz));
		gpu_tz = NULL;

		return -EPROBE_DEFER;
	}

	return 0;

}
#endif
#endif

