// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2011 Unisoc Co., Ltd.
 * Changhua.Zhang <Changhua.Zhang@unisoc.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/device.h>
#include <linux/err.h>
#include <linux/gpio/consumer.h>
#include <linux/iio/consumer.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/math64.h>
#include <linux/module.h>
#include <linux/nvmem-consumer.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/pm_wakeup.h>
#include <linux/power_supply.h>
#include <linux/power/charger-manager.h>
#include <linux/power/sprd_battery_info.h>
#include <linux/power/sprd_fuel_gauge_core.h>
#include <linux/regmap.h>
#include <linux/rtc.h>
#include <linux/slab.h>

/* FGU registers definition */
#define SC27XX_FGU_START		0x0
#define SC27XX_FGU_CONFIG		0x4
#define SC27XX_FGU_ADC_CONFIG		0x8
#define SC27XX_FGU_STATUS		0xc
#define SC27XX_FGU_INT_EN		0x10
#define SC27XX_FGU_INT_CLR		0x14
#define SC27XX_FGU_INT_STS		0x1c
#define SC27XX_FGU_VOLTAGE		0x20
#define SC27XX_FGU_OCV			0x24
#define SC27XX_FGU_POCV			0x28
#define SC27XX_FGU_CURRENT		0x2c
#define SC27XX_FGU_HIGH_OVERLOAD	0x30
#define SC27XX_FGU_LOW_OVERLOAD		0x34
#define SC27XX_FGU_CLBCNT_SETH		0x50
#define SC27XX_FGU_CLBCNT_SETL		0x54
#define SC27XX_FGU_CLBCNT_DELTH		0x58
#define SC27XX_FGU_CLBCNT_DELTL		0x5c
#define SC27XX_FGU_CLBCNT_VALH		0x68
#define SC27XX_FGU_CLBCNT_VALL		0x6c
#define SC27XX_FGU_CLBCNT_QMAXL		0x74
#define SC27XX_FGU_RELAX_CURT_THRE	0x80
#define SC27XX_FGU_RELAX_CNT_THRE	0x84
#define SC27XX_FGU_USER_AREA_SET0	0xa0
#define SC27XX_FGU_USER_AREA_CLEAR0	0xa4
#define SC27XX_FGU_USER_AREA_STATUS0	0xa8
#define SC27XX_FGU_USER_AREA_SET1	0xc0
#define SC27XX_FGU_USER_AREA_CLEAR1	0xc4
#define SC27XX_FGU_USER_AREA_STATUS1	0xc8
#define SC27XX_FGU_VOLTAGE_BUF		0xd0
#define SC27XX_FGU_CURRENT_BUF		0xf0
#define SC27XX_FGU_REG_MAX		0x260

/* PMIC global control registers definition */
#define SC2720_MODULE_EN0		0xc08
#define SC2720_CLK_EN0			0xc10
#define SC2721_MODULE_EN0		0xc08
#define SC2721_CLK_EN0			0xc10
#define SC2730_MODULE_EN0		0x1808
#define SC2730_CLK_EN0			0x1810
#define SC2731_MODULE_EN0		0xc08
#define SC2731_CLK_EN0			0xc18
#define SC27XX_FGU_EN			BIT(7)
#define SC27XX_FGU_RTC_EN		BIT(6)

/* Efuse fgu calibration bit definition */
#define SC27XX_FGU_CAL			GENMASK(8, 0)
#define SC27XX_FGU_CAL_SHIFT		0
#define SC27XX_FGU_IDEAL_RESISTANCE	20000

/* SC27XX_FGU_CONFIG */
#define SC27XX_FGU_LOW_POWER_MODE	BIT(1)
#define SC27XX_FGU_RELAX_CNT_MODE	0
#define SC27XX_FGU_DEEP_SLEEP_MODE	1

/* SC27XX_FGU_INT */
#define SC27XX_FGU_LOW_OVERLOAD_INT		BIT(0)
#define SC27XX_FGU_HIGH_OVERLOAD_INT		BIT(1)
#define SC27XX_FGU_CLBCNT_DELTA_INT		BIT(2)
#define SC27XX_FGU_RELAX_CNT_INT		BIT(3)
#define SC27XX_FGU_LOW_OVERLOAD_INT_SHIFT	0
#define SC27XX_FGU_HIGH_OVERLOAD_INT_SHIFT	1
#define SC27XX_FGU_CLBCNT_DELTA_INT_SHIFT	2
#define SC27XX_FGU_RELAX_CNT_INT_SHIFT		3

/* SC27XX_FGU_STS */
#define SC27XX_FGU_BATTERY_FLAG_STS_MASK	BIT(8)
#define SC27XX_FGU_BATTERY_FLAG_STS_SHIFT	8
#define SC27XX_FGU_INVALID_POCV_STS_MASK	BIT(7)
#define SC27XX_FGU_INVALID_POCV_STS_SHIFT	7
#define SC27XX_FGU_RELAX_POWER_STS_MASK		BIT(5)
#define SC27XX_FGU_RELAX_POWER_STS_SHIFT	5
#define SC27XX_FGU_RELAX_CURT_STS_MASK		BIT(4)
#define SC27XX_FGU_RELAX_CURT_STS_SHIFT		4

/* SC27XX_FGU_RELAX_CNT */
#define SC27XX_FGU_RELAX_CURT_THRE_MASK		GENMASK(13, 0)
#define SC27XX_FGU_RELAX_CURT_THRE_SHIFT	0
#define SC27XX_FGU_RELAX_CNT_THRE_MASK		GENMASK(12, 0)
#define SC27XX_FGU_RELAX_CNT_THRE_SHITF		0

#define SC27XX_WRITE_SELCLB_EN			BIT(0)
#define SC27XX_FGU_CLBCNT_MASK			GENMASK(15, 0)
#define SC27XX_FGU_HIGH_OVERLOAD_MASK		GENMASK(12, 0)
#define SC27XX_FGU_LOW_OVERLOAD_MASK		GENMASK(12, 0)

#define SC27XX_FGU_CURRENT_BUFF_CNT		8
#define SC27XX_FGU_VOLTAGE_BUFF_CNT		8
#define SC27XX_FGU_CUR_BASIC_ADC		8192

#define SC27XX_FGU_MODE_AREA_MASK		GENMASK(15, 12)
#define SC27XX_FGU_CAP_AREA_MASK		GENMASK(11, 0)
#define SC27XX_FGU_MODE_AREA_SHIFT		12
#define SC27XX_FGU_CAP_INTEGER_MASK		GENMASK(7, 0)
#define SC27XX_FGU_CAP_DECIMAL_MASK		GENMASK(3, 0)
#define SC27XX_FGU_CAP_DECIMAL_SHIFT		8
#define SC27XX_FGU_FIRST_POWERON		GENMASK(3, 0)
#define SC27XX_FGU_DEFAULT_CAP			GENMASK(11, 0)

#define SC27XX_FGU_INT_MASK			GENMASK(9, 0)
#define SC27XX_FGU_MAGIC_NUMBER			0x5a5aa5a5
#define SC27XX_FGU_FCC_PERCENT			1000
#define SC27XX_FGU_SAMPLE_HZ			2

#define SC27XX_FGU_LAST_OCV_MAGIC		0X1
#define SC27XX_FGU_LAST_OCV_MAGIC_SHIFT	14
#define SC27XX_FGU_LAST_OCV_MAGIC_MASK		GENMASK(15, 14)
#define SC27XX_FGU_LAST_OCV_DATA_MASK		GENMASK(13, 0)
#define SC27XX_FGU_LAST_SMOOTH_SOC_MAGIC_MASK         GENMASK(15, 14)
#define SC27XX_FGU_LAST_SMOOTH_SOC_DATA_MASK          GENMASK(13, 0)
#define SC27XX_FGU_LAST_SMOOTH_SOC_MASK               GENMASK(15, 0)
#define SC27XX_FGU_LAST_CC_MAH_MAGIC_SHIFT	14
#define SC27XX_FGU_LAST_CC_MAH_MAGIC_MASK	GENMASK(15, 14)
#define SC27XX_FGU_LAST_CC_MAH_DATA_MASK	GENMASK(13, 0)
#define SC27XX_FGU_LAST_BATT_TEMP_DATA_MASK	GENMASK(13, 0)
#define SC27XX_FGU_CC_MAH_MAX			8191
#define SC27XX_FGU_SMOOTH_SOC_MAX 		1000
#define SC27XX_FGU_BATT_TEMP_MAX		200

/* relax cnt define */
#define SC27XX_FGU_RELAX_CUR_THRESHOLD_MA	30
#define SC27XX_FGU_RELAX_CNT_THRESHOLD		320

static int init_clbcnt;
static int start_work_clbcnt;
static int latest_clbcnt;
static int last_cap_magic = SC27XX_FGU_LAST_OCV_MAGIC << SC27XX_FGU_LAST_OCV_MAGIC_SHIFT;

static const struct sprd_fgu_variant_data sc2720_fgu_info = {
	.module_en = SC2720_MODULE_EN0,
	.clk_en = SC2720_CLK_EN0,
	.fgu_cal = SC27XX_FGU_CAL,
	.fgu_cal_shift = SC27XX_FGU_CAL_SHIFT,
};

static const struct sprd_fgu_variant_data sc2721_fgu_info = {
	.module_en = SC2721_MODULE_EN0,
	.clk_en = SC2721_CLK_EN0,
	.fgu_cal = SC27XX_FGU_CAL,
	.fgu_cal_shift = SC27XX_FGU_CAL_SHIFT,
};

static const struct sprd_fgu_variant_data sc2730_fgu_info = {
	.module_en = SC2730_MODULE_EN0,
	.clk_en = SC2730_CLK_EN0,
	.fgu_cal = SC27XX_FGU_CAL,
	.fgu_cal_shift = SC27XX_FGU_CAL_SHIFT,
};

static const struct sprd_fgu_variant_data sc2731_fgu_info = {
	.module_en = SC2731_MODULE_EN0,
	.clk_en = SC2731_CLK_EN0,
	.fgu_cal = SC27XX_FGU_CAL,
	.fgu_cal_shift = SC27XX_FGU_CAL_SHIFT,
};

static const struct of_device_id sc27xx_fgu_dev_match_arr[] = {
	{ .compatible = "sprd,sc2720-fgu", .data = &sc2720_fgu_info},
	{ .compatible = "sprd,sc2721-fgu", .data = &sc2721_fgu_info},
	{ .compatible = "sprd,sc2730-fgu", .data = &sc2730_fgu_info},
	{ .compatible = "sprd,sc2731-fgu", .data = &sc2731_fgu_info},
	{ .compatible = "sprd,sc2730-fgu-v2", .data = &sc2730_fgu_info}
};

static int sc27xx_fgu_parse_cmdline_match(struct sprd_fgu_info *info, char *match_str,
					  char *result, int size)
{
	struct device_node *cmdline_node = NULL;
	const char *cmdline;
	char *match, *match_end;
	int len, match_str_len, ret;

	if (!result || !match_str)
		return -EINVAL;

	memset(result, '\0', size);
	match_str_len = strlen(match_str);

	cmdline_node = of_find_node_by_path("/chosen");
	ret = of_property_read_string(cmdline_node, "bootargs", &cmdline);
	if (ret) {
		dev_warn(info->dev, "%s failed to read bootargs\n", __func__);
		return -EINVAL;
	}

	match = strstr(cmdline, match_str);
	if (!match) {
		dev_warn(info->dev, "Match: %s fail in cmdline\n", match_str);
		return -EINVAL;
	}

	match_end = strstr((match + match_str_len), " ");
	if (!match_end) {
		dev_warn(info->dev, "Match end of : %s fail in cmdline\n", match_str);
		return -EINVAL;
	}

	len = match_end - (match + match_str_len);
	if (len < 0) {
		dev_warn(info->dev, "Match cmdline :%s fail, len = %d\n", match_str, len);
		return -EINVAL;
	}

	memcpy(result, (match + match_str_len), len);

	return 0;
}

static void sc27xx_fgu_parse_batt_ocv_uv(struct sprd_fgu_info *info)
{
	char result[32] = {};
	int ret, batt_ocv_mv;
	char *str;

	if (!info->use_miscdata)
		return;

	str = "charge.batt_ocv_uv=";
	batt_ocv_mv = -1;
	ret = sc27xx_fgu_parse_cmdline_match(info, str, result, sizeof(result));
	if (!ret) {
		ret = kstrtoint(result, 10, &batt_ocv_mv);
		if (ret) {
			batt_ocv_mv = -1;
			dev_err(info->dev, "Covert batt_ocv_uv fail, ret = %d, result = %s\n",
				ret, result);
		}
	}

	if (batt_ocv_mv < 0)
		info->batt_ocv_mv_magic = -1;
	else
		info->batt_ocv_mv_magic = batt_ocv_mv;
}

static void sc27xx_fgu_parse_batt_ocv_temp(struct sprd_fgu_info *info)
{
	char result[32] = {};
	int ret, batt_ocv_temp;
	char *str;

	if (!info->use_miscdata)
		return;

	str = "charge.batt_ocv_temp=";
	batt_ocv_temp = -1;
	ret = sc27xx_fgu_parse_cmdline_match(info, str, result, sizeof(result));
	if (!ret) {
		ret = kstrtoint(result, 10, &batt_ocv_temp);
		if (ret) {
			batt_ocv_temp = -1;
			dev_err(info->dev, "Covert batt_ocv_temp fail, ret = %d, result = %s\n",
				ret, result);
		}
	}

	if (batt_ocv_temp < 0)
		info->batt_ocv_temp_magic = -1;
	else
		info->batt_ocv_temp_magic = batt_ocv_temp;
}

static void sc27xx_fgu_parse_batt_shutdown_temp(struct sprd_fgu_info *info)
{
	char result[32] = {};
	int ret, batt_shutdown_temp;
	char *str;

	if (!info->use_miscdata)
		return;

	str = "charge.batt_shutdown_temp=";
	batt_shutdown_temp = -1;
	ret = sc27xx_fgu_parse_cmdline_match(info, str, result, sizeof(result));
	if (!ret) {
		ret = kstrtoint(result, 10, &batt_shutdown_temp);
		if (ret) {
			batt_shutdown_temp = -1;
			dev_err(info->dev, "Covert batt_shutdown_temp fail, ret = %d, result = %s\n",
				ret, result);
		}
	}

	if (batt_shutdown_temp < 0)
		info->batt_shutdown_temp_magic = -1;
	else
		info->batt_shutdown_temp_magic = batt_shutdown_temp;
}

static void sc27xx_fgu_parse_batt_cc_uah(struct sprd_fgu_info *info)
{
	char result[32] = {};
	int ret, batt_cc_mah;
	char *str;

	if (!info->use_miscdata)
		return;

	str = "charge.batt_cc_uah=";
	batt_cc_mah = -1;
	ret = sc27xx_fgu_parse_cmdline_match(info, str, result, sizeof(result));
	if (!ret) {
		ret = kstrtoint(result, 10, &batt_cc_mah);
		if (ret) {
			batt_cc_mah = -1;
			dev_err(info->dev, "Covert batt_cc_mah fail, ret = %d, result = %s\n",
				ret, result);
		}
	}

	if (batt_cc_mah < 0)
		info->batt_cc_mah_magic = -1;
	else
		info->batt_cc_mah_magic = batt_cc_mah;
}

static inline int sc27xx_fgu_adc2voltage(struct sprd_fgu_info *info, s64 adc)
{
	return DIV_S64_ROUND_CLOSEST(adc * 1000, info->vol_1000mv_adc);
}

static inline int sc27xx_fgu_voltage2adc(struct sprd_fgu_info *info, int vol_mv)
{
	return DIV_ROUND_CLOSEST(vol_mv * info->vol_1000mv_adc, 1000);
}

static inline int sc27xx_fgu_adc2current(struct sprd_fgu_info *info, s64 adc)
{
	return DIV_S64_ROUND_CLOSEST((adc - SC27XX_FGU_CUR_BASIC_ADC) * 1000,
				     info->cur_1000ma_adc);
}

static inline int sc27xx_fgu_current2adc(struct sprd_fgu_info *info, int cur_ma)
{
	return (cur_ma * info->cur_1000ma_adc) / 1000 + SC27XX_FGU_CUR_BASIC_ADC;
}

static inline int sc27xx_fgu_cap2mah(struct sprd_fgu_info *info, int total_mah, int cap)
{
	/*
	 * Get current capacity (mAh) = battery total capacity (mAh) *
	 * current capacity percent (capacity / 100).
	 */
	return DIV_ROUND_CLOSEST(total_mah * cap, SC27XX_FGU_FCC_PERCENT);
}

static int sc27xx_fgu_cap2clbcnt(struct sprd_fgu_info *info, int total_mah, int cap)
{
	int cur_mah = sc27xx_fgu_cap2mah(info, total_mah, cap);

	/*
	 * Convert current capacity (mAh) to coulomb counter according to the
	 * formula: 1 mAh = 3.6 coulomb.
	 * 1 clbcnt = cur_mah * 36 * cur_1000ma_adc * fgu_clk_sample_hz / 10
	 * = cur_mah * 18 * cur_1000ma_adc * fgu_clk_sample_hz / 5
	 */
	return DIV_S64_ROUND_CLOSEST(cur_mah * 18 * info->cur_1000ma_adc * SC27XX_FGU_SAMPLE_HZ, 5);
}

static int sc27xx_fgu_clbcnt2uah(struct sprd_fgu_info *info, s64 clbcnt)
{
	/*
	 * Convert coulomb counter to delta capacity (uAh), and set multiplier
	 * as 10 to improve the precision.
	 * formula: 1000 uAh = 3.6 coulomb
	 * 1 uah = clbcnt * 10 * 1000 / (36 * fgu_clk_sample_hz * cur_1000ma_adc)
	 * = clbcnt * 2500 / (9 * fgu_clk_sample_hz * cur_1000ma_adc)
	 */
	s64 uah = DIV_S64_ROUND_CLOSEST(clbcnt * 2500, 9 * SC27XX_FGU_SAMPLE_HZ);

	if (uah > 0)
		uah = uah + info->cur_1000ma_adc / 2;
	else
		uah = uah - info->cur_1000ma_adc / 2;

	return (int)div_s64(uah, info->cur_1000ma_adc);
}

static int sc27xx_fgu_enable_fgu_module(struct sprd_fgu_info *info, bool enable)
{
	int ret = 0;
	u32 reg_val;

	reg_val = enable ? SC27XX_FGU_EN : 0;
	ret = regmap_update_bits(info->regmap, info->pdata->module_en, SC27XX_FGU_EN, reg_val);
	if (ret) {
		dev_err(info->dev, "failed to %s fgu module!\n", enable ? "enable" : "disable");
		return ret;
	}

	reg_val = enable ? SC27XX_FGU_RTC_EN : 0;
	ret = regmap_update_bits(info->regmap, info->pdata->clk_en, SC27XX_FGU_RTC_EN, reg_val);
	if (ret)
		dev_err(info->dev, "failed to %s fgu RTC clock!\n", enable ? "enable" : "disable");

	return ret;
}

static inline int sc27xx_fgu_enable_high_overload_int(struct sprd_fgu_info *info, u32 reg_val)
{
	return regmap_update_bits(info->regmap, info->base + SC27XX_FGU_INT_EN,
				  SC27XX_FGU_HIGH_OVERLOAD_INT, reg_val);
}

static inline int sc27xx_fgu_enable_low_overload_int(struct sprd_fgu_info *info, u32 reg_val)
{
	return regmap_update_bits(info->regmap, info->base + SC27XX_FGU_INT_EN,
				  SC27XX_FGU_LOW_OVERLOAD_INT, reg_val);
}

static inline int sc27xx_fgu_enable_clbcnt_delta_int(struct sprd_fgu_info *info, u32 reg_val)
{
	return regmap_update_bits(info->regmap, info->base + SC27XX_FGU_INT_EN,
				  SC27XX_FGU_CLBCNT_DELTA_INT, reg_val);
}

static inline int sc27xx_fgu_enable_relax_counter_int(struct sprd_fgu_info *info, u32 reg_val)
{
	return regmap_update_bits(info->regmap, info->base + SC27XX_FGU_INT_EN,
				  SC27XX_FGU_RELAX_CNT_INT, reg_val);
}

static int sc27xx_fgu_enable_fgu_int(struct sprd_fgu_info *info,
				     enum sprd_fgu_int_command int_cmd, bool enable)
{
	int ret = 0;
	u32 reg_val;

	switch (int_cmd) {
	case SPRD_FGU_VOLT_LOW_INT_CMD:
		reg_val = enable ? SC27XX_FGU_LOW_OVERLOAD_INT : 0;
		ret = sc27xx_fgu_enable_low_overload_int(info, reg_val);
		if (ret)
			dev_err(info->dev, "failed to %s fgu low overload int!\n",
				enable ? "enable" : "disable");
		break;
	case SPRD_FGU_VOLT_HIGH_INT_CMD:
		reg_val = enable ? SC27XX_FGU_HIGH_OVERLOAD_INT : 0;
		ret = sc27xx_fgu_enable_high_overload_int(info, reg_val);
		if (ret)
			dev_err(info->dev, "failed to %s fgu high overload int!\n",
				enable ? "enable" : "disable");
		break;
	case SPRD_FGU_CLBCNT_DELTA_INT_CMD:
		reg_val = enable ? SC27XX_FGU_CLBCNT_DELTA_INT : 0;
		ret = sc27xx_fgu_enable_clbcnt_delta_int(info, reg_val);
		if (ret)
			dev_err(info->dev, "failed to %s fgu clbcnt delta int!\n",
				enable ? "enable" : "disable");
		break;
	case SPRD_FGU_RELAX_CNT_INT_CMD:
		reg_val = enable ? SC27XX_FGU_RELAX_CNT_INT : 0;
		ret = sc27xx_fgu_enable_relax_counter_int(info, reg_val);
		if (ret)
			dev_err(info->dev, "failed to %s fgu power low cnt int!\n",
				enable ? "enable" : "disable");
		break;
	default:
		dev_err(info->dev, "%s failed to identify int command!\n", __func__);
		break;
	}

	return ret;
}

static inline int sc27xx_fgu_enable_relax_cnt_mode(struct sprd_fgu_info *info)
{
	return regmap_update_bits(info->regmap, info->base + SC27XX_FGU_CONFIG,
				  SC27XX_FGU_LOW_POWER_MODE, SC27XX_FGU_RELAX_CNT_MODE);
}

static inline int sc27xx_fgu_clr_fgu_int(struct sprd_fgu_info *info)
{
	return regmap_update_bits(info->regmap, info->base + SC27XX_FGU_INT_CLR,
				  SC27XX_FGU_INT_MASK, SC27XX_FGU_INT_MASK);
}

static int sc27xx_fgu_clr_fgu_int_all(struct sprd_fgu_info *info, unsigned int int_sts)
{
	int int_clr = 0x3ff;

	return regmap_update_bits(info->regmap, info->base + SC27XX_FGU_INT_CLR,
				  int_clr, int_clr);
}

static int sc27xx_fgu_clr_fgu_int_bit(struct sprd_fgu_info *info,
				      enum sprd_fgu_int_command int_cmd)
{
	int ret = 0;
	u32 bit_val = 0;

	switch (int_cmd) {
	case SPRD_FGU_VOLT_LOW_INT_CMD:
		bit_val = SC27XX_FGU_LOW_OVERLOAD_INT;
		break;
	case SPRD_FGU_VOLT_HIGH_INT_CMD:
		bit_val = SC27XX_FGU_HIGH_OVERLOAD_INT;
		break;
	case SPRD_FGU_CLBCNT_DELTA_INT_CMD:
		bit_val = SC27XX_FGU_CLBCNT_DELTA_INT;
		break;
	case SPRD_FGU_RELAX_CNT_INT_CMD:
		bit_val = SC27XX_FGU_RELAX_CNT_INT;
		break;
	default:
		dev_err(info->dev, "%s failed to identify int command!\n", __func__);
		break;
	}

	if (bit_val) {
		ret = regmap_update_bits(info->regmap, info->base + SC27XX_FGU_INT_CLR,
					 bit_val, bit_val);
		if (ret)
			dev_err(info->dev, "failed to clr fgu int, int status = %d\n", int_cmd);
	}

	return ret;
}

static int sc27xx_fgu_get_fgu_int(struct sprd_fgu_info *info, int *int_sts)

{
	int ret = 0, low_overload_int, high_overload_int, clbcnt_delta_int, relax_cnt_int;

	ret = regmap_read(info->regmap, info->base + SC27XX_FGU_INT_STS, int_sts);
	if (ret) {
		dev_err(info->dev, "failed to get fgu int status!\n");
		return ret;
	}

	low_overload_int = (SC27XX_FGU_LOW_OVERLOAD_INT & *int_sts) >>
		SC27XX_FGU_LOW_OVERLOAD_INT_SHIFT;
	high_overload_int = (SC27XX_FGU_HIGH_OVERLOAD_INT & *int_sts) >>
		SC27XX_FGU_HIGH_OVERLOAD_INT_SHIFT;
	clbcnt_delta_int = (SC27XX_FGU_CLBCNT_DELTA_INT & *int_sts) >>
		SC27XX_FGU_CLBCNT_DELTA_INT_SHIFT;
	relax_cnt_int = (SC27XX_FGU_RELAX_CNT_INT & *int_sts) >>
		SC27XX_FGU_RELAX_CNT_INT_SHIFT;
	*int_sts = ((low_overload_int << SPRD_FGU_VOLT_LOW_INT_EVENT) |
		    (high_overload_int << SPRD_FGU_VOLT_HIGH_INT_EVENT) |
		    (clbcnt_delta_int << SPRD_FGU_CLBCNT_DELTA_INT_EVENT) |
		    (relax_cnt_int << SPRD_FGU_RELAX_CNT_INT_EVENT));

	return ret;
}

static int sc27xx_fgu_get_fgu_sts(struct sprd_fgu_info *info,
				  enum sprd_fgu_sts_command sts_cmd, int *fgu_sts)
{
	int ret = 0;

	ret = regmap_read(info->regmap, info->base + SC27XX_FGU_STATUS, fgu_sts);
	if (ret) {
		dev_err(info->dev, "failed to get fgu status!, cmd = %d\n", sts_cmd);
		return ret;
	}

	switch (sts_cmd) {
	case SPRD_FGU_CURT_LOW_STS_CMD:
		*fgu_sts = (SC27XX_FGU_RELAX_CURT_STS_MASK & *fgu_sts) >>
			SC27XX_FGU_RELAX_CURT_STS_SHIFT;
		break;
	case SPRD_FGU_POWER_LOW_STS_CMD:
		*fgu_sts = (SC27XX_FGU_RELAX_POWER_STS_MASK & *fgu_sts) >>
			SC27XX_FGU_RELAX_POWER_STS_SHIFT;
		break;
	case SPRD_FGU_INVALID_POCV_STS_CMD:
		*fgu_sts = (SC27XX_FGU_INVALID_POCV_STS_MASK & *fgu_sts) >>
			SC27XX_FGU_INVALID_POCV_STS_SHIFT;
		break;
	case SPRD_FGU_BATTERY_FLAG_STS_CMD:
		*fgu_sts = (SC27XX_FGU_BATTERY_FLAG_STS_MASK & *fgu_sts) >>
			SC27XX_FGU_BATTERY_FLAG_STS_SHIFT;
		break;
	default:
		dev_err(info->dev, "%s failed to identify sts command!\n", __func__);
		break;
	}

	return ret;
}

static int sc27xx_fgu_suspend_calib_check_relax_counter_sts(struct sprd_fgu_info *info)
{
	int ret = 0;
	u32 int_status = 0;

	mutex_lock(&info->lock);
	if (info->slp_cap_calib.relax_cnt_int_ocurred) {
		info->slp_cap_calib.relax_cnt_int_ocurred = false;
		dev_info(info->dev, "relax_cnt_int ocurred 1!!\n");
		goto no_relax_cnt_int;
	}

	ret = sc27xx_fgu_get_fgu_int(info, &int_status);
	if (ret) {
		dev_err(info->dev, "suspend_calib failed to get fgu interrupt status, ret = %d\n",
			ret);
		goto no_relax_cnt_int;
	}

	if (!(int_status & BIT(SPRD_FGU_RELAX_CNT_INT_EVENT))) {
		dev_info(info->dev, "no relax_cnt_int ocurred!!\n");
		ret = -EINVAL;
		goto no_relax_cnt_int;
	}

	ret = sc27xx_fgu_clr_fgu_int_bit(info, SPRD_FGU_RELAX_CNT_INT_CMD);
	if (ret)
		dev_err(info->dev, "failed to clear relax_cnt_sts interrupt status, ret = %d\n",
			ret);

	dev_info(info->dev, "relax_cnt_int ocurred!!\n");
	ret = 0;

no_relax_cnt_int:
	mutex_unlock(&info->lock);
	return ret;
}

static int sc27xx_fgu_set_low_overload(struct sprd_fgu_info *info, int vol)
{
	int adc;

	adc = sc27xx_fgu_voltage2adc(info, vol);
	return regmap_update_bits(info->regmap, info->base + SC27XX_FGU_LOW_OVERLOAD,
				  SC27XX_FGU_LOW_OVERLOAD_MASK, adc);
}

static int sc27xx_fgu_set_high_overload(struct sprd_fgu_info *info, int vol)
{
	int adc;

	adc = sc27xx_fgu_voltage2adc(info, vol);
	return regmap_update_bits(info->regmap, info->base + SC27XX_FGU_HIGH_OVERLOAD,
				  SC27XX_FGU_HIGH_OVERLOAD_MASK, adc);
}

static int sc27xx_fgu_get_calib_efuse(struct sprd_fgu_info *info,
				      char *calib_str, int *calib_data)
{
	struct nvmem_cell *cell;
	void *buf;
	size_t len = 0;

	*calib_data = 0;
	cell = nvmem_cell_get(info->dev, calib_str);
	if (IS_ERR(cell))
		return PTR_ERR(cell);

	buf = nvmem_cell_read(cell, &len);
	nvmem_cell_put(cell);

	if (IS_ERR(buf))
		return PTR_ERR(buf);

	memcpy(calib_data, buf, min(len, sizeof(u32)));

	kfree(buf);

	return 0;
}

static int sc27xx_fgu_calibration(struct sprd_fgu_info *info)
{
	int ret = 0, calib_data, cal_4200mv;
	const struct sprd_fgu_variant_data *pdata = info->pdata;

	if (!pdata) {
		pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
		return -EINVAL;
	}

	/* block3 */
	ret = sc27xx_fgu_get_calib_efuse(info, "fgu_calib", &calib_data);
	if (ret) {
		dev_err(info->dev, "failed to get calib efuse\n");
		return ret;
	}

	/*
	 * Get the ADC value corresponding to 4200 mV from eFuse controller
	 * according to below formula. Then convert to ADC values corresponding
	 * to 1000 mV and 1000 mA.
	 */
	cal_4200mv = ((calib_data & pdata->fgu_cal) >> pdata->fgu_cal_shift) +
		6963 - 4096 - 256;
	info->vol_1000mv_adc = DIV_ROUND_CLOSEST(cal_4200mv * 10, 42);
	info->cur_1000ma_adc =
		DIV_ROUND_CLOSEST(info->vol_1000mv_adc * 4 * info->calib_resist,
				  SC27XX_FGU_IDEAL_RESISTANCE);

	dev_info(info->dev, "%s cur_1000ma_adc = %d, cal_4200mv = %d, vol_1000mv_adc = %d\n",
		 __func__, info->cur_1000ma_adc, cal_4200mv, info->vol_1000mv_adc);

	return 0;
}

/* @val: value of battery voltage in mV*/
static int sc27xx_fgu_get_vbat_now(struct sprd_fgu_info *info, int *val)
{
	int ret, vol = 0;

	ret = regmap_read(info->regmap, info->base + SC27XX_FGU_VOLTAGE, &vol);
	if (ret)
		return ret;

	/*
	 * It is ADC values reading from registers which need to convert to
	 * corresponding voltage values.
	 */
	*val = sc27xx_fgu_adc2voltage(info, vol);

	return 0;
}

/* @val: average value of battery voltage in mV */
static int sc27xx_fgu_get_vbat_avg(struct sprd_fgu_info *info, int *val)
{
	int ret, i;
	u32 vol_adc = 0;

	*val = 0;
	for (i = 0; i < SC27XX_FGU_VOLTAGE_BUFF_CNT; i++) {
		ret = regmap_read(info->regmap,
				  info->base + SC27XX_FGU_VOLTAGE_BUF + i * 4,
				  &vol_adc);
		if (ret)
			return ret;

		/*
		 * It is ADC values reading from registers which need to convert to
		 * corresponding voltage values.
		 */
		*val += sc27xx_fgu_adc2voltage(info, vol_adc);
	}

	*val /= 8;

	return 0;
}

/* @val: buff value of battery voltage in mV */
static int sc27xx_fgu_get_vbat_buf(struct sprd_fgu_info *info, int index, int *val)
{
	int ret = 0, vol = 0;

	ret = regmap_read(info->regmap, info->base + SC27XX_FGU_VOLTAGE_BUF + index * 4, &vol);
	if (ret)
		return ret;
	/*
	 * It is ADC values reading from registers which need to convert to
	 * corresponding voltage values.
	 */
	*val = sc27xx_fgu_adc2voltage(info, vol);

	return ret;
}

/* @val: value of battery current in mA*/
static int sc27xx_fgu_get_current_now(struct sprd_fgu_info *info, int *val)
{
	int ret = 0;
	u32 cur_adc = 0;

	ret = regmap_read(info->regmap, info->base + SC27XX_FGU_CURRENT, &cur_adc);
	if (ret)
		return ret;

	/*
	 * It is ADC values reading from registers which need to convert to
	 * corresponding current values (unit mA).
	 */
	*val = sc27xx_fgu_adc2current(info, (s64)cur_adc);

	return ret;
}

/* @val: average value of battery current in mA */
static int sc27xx_fgu_get_current_avg(struct sprd_fgu_info *info, int *val)
{
	int ret = 0, i;
	u32 cur_adc = 0;

	*val = 0;

	for (i = 0; i < SC27XX_FGU_CURRENT_BUFF_CNT; i++) {
		ret = regmap_read(info->regmap,
				  info->base + SC27XX_FGU_CURRENT_BUF + i * 4,
				  &cur_adc);
		if (ret)
			return ret;
		/*
		 * It is ADC values reading from registers which need to convert to
		 * corresponding current values (unit mA).
		 */
		*val += sc27xx_fgu_adc2current(info, (s64)cur_adc);
	}

	*val /= 8;

	return ret;
}

/* @val: buf value of battery current in mA */
static int sc27xx_fgu_get_current_buf(struct sprd_fgu_info *info, int index, int *val)
{
	int ret = 0, cur_adc = 0;

	ret = regmap_read(info->regmap,
			  info->base + SC27XX_FGU_CURRENT_BUF + index * 4, &cur_adc);
	if (ret)
		return ret;

	/*
	 * It is ADC values reading from registers which need to convert to
	 * corresponding current values (unit mA).
	 */
	*val = sc27xx_fgu_adc2current(info, (s64)cur_adc);

	return ret;
}

/*
 * After system booting on, the FGU_ANA_POCI register saved
 * the first sampled open circuit current.
 * @val: value of battery current in mA*
 */
static int sc27xx_fgu_get_poci(struct sprd_fgu_info *info, int *val)
{
	int ret = 0;
	u32 cur_adc = 0;

	/*
	 * After system booting on, the SC27XX_FGU_CLBCNT_QMAXL register saved
	 * the first sampled open circuit current.
	 */
	ret = regmap_read(info->regmap, info->base + SC27XX_FGU_CLBCNT_QMAXL, &cur_adc);
	if (ret) {
		dev_err(info->dev, "Failed to read CLBCNT_QMAXL, ret = %d\n", ret);
		return ret;
	}

	cur_adc <<= 1;

	/*
	 * It is ADC values reading from registers which need to convert to
	 * corresponding current values (unit mA).
	 */
	*val = sc27xx_fgu_adc2current(info, (s64)cur_adc);

	return ret;
}

/*
 * Should get the OCV from FGU_POCV register at the system
 * beginning. It is ADC values reading from registers which need to
 * convert the corresponding voltage.
 * @val: value of battery voltage in mV
 */
static int sc27xx_fgu_get_pocv(struct sprd_fgu_info *info, int *val)
{
	int ret = 0;
	u32 vol_adc = 0;

	ret = regmap_read(info->regmap, info->base + SC27XX_FGU_POCV, &vol_adc);
	if (ret) {
		dev_err(info->dev, "Failed to read FGU_POCV, ret = %d\n", ret);
		return ret;
	}
	/*
	 * It is ADC values reading from registers which need to convert to
	 * corresponding voltage values.
	 */
	*val = sc27xx_fgu_adc2voltage(info, vol_adc);

	return ret;
}
static bool sc27xx_fgu_is_first_poweron(struct sprd_fgu_info *info)
{
	int ret;
	u32 status = 0, cap, mode;

	ret = regmap_read(info->regmap, info->base + SC27XX_FGU_USER_AREA_STATUS0, &status);
	if (ret)
		return false;

	/*
	 * We use low 12 bits to save the last battery capacity and high 4 bits
	 * to save the system boot mode.
	 */
	mode = (status & SC27XX_FGU_MODE_AREA_MASK) >> SC27XX_FGU_MODE_AREA_SHIFT;
	cap = status & SC27XX_FGU_CAP_AREA_MASK;

	/*
	 * When FGU has been powered down, the user area registers became
	 * default value (0xffff), which can be used to valid if the system is
	 * first power on or not.
	 */
	if (mode == SC27XX_FGU_FIRST_POWERON || cap == SC27XX_FGU_DEFAULT_CAP)
		return true;

	return false;
}

static int sc27xx_fgu_save_boot_mode(struct sprd_fgu_info *info, int boot_mode)
{
	int ret;

	ret = regmap_update_bits(info->regmap,
				 info->base + SC27XX_FGU_USER_AREA_CLEAR0,
				 SC27XX_FGU_MODE_AREA_MASK,
				 SC27XX_FGU_MODE_AREA_MASK);
	if (ret) {
		dev_err(info->dev, "%d Failed to write mode user clr, ret = %d\n", __LINE__, ret);
		return ret;
	}

	/*
	 * Since the user area registers are put on power always-on region,
	 * then these registers changing time will be a little long. Thus
	 * here we should delay 200us to wait until values are updated
	 * successfully according to the datasheet.
	 */
	usleep_range(200, 210);

	ret = regmap_update_bits(info->regmap,
				 info->base + SC27XX_FGU_USER_AREA_SET0,
				 SC27XX_FGU_MODE_AREA_MASK,
				 boot_mode << SC27XX_FGU_MODE_AREA_SHIFT);
	if (ret) {
		dev_err(info->dev, "Failed to write mode user set, ret = %d\n", ret);
		return ret;
	};

	/*
	 * Since the user area registers are put on power always-on region,
	 * then these registers changing time will be a little long. Thus
	 * here we should delay 200us to wait until values are updated
	 * successfully according to the datasheet.
	 */
	usleep_range(200, 210);

	/*
	 * According to the datasheet, we should set the USER_AREA_CLEAR to 0 to
	 * make the user area data available, otherwise we can not save the user
	 * area data.
	 */
	ret = regmap_update_bits(info->regmap,
				 info->base + SC27XX_FGU_USER_AREA_CLEAR0,
				 SC27XX_FGU_MODE_AREA_MASK, 0);
	if (ret) {
		dev_err(info->dev, "%d Failed to write mode user clr, ret = %d\n", __LINE__, ret);
		return ret;
	}

	/*
	 * Since the user area registers are put on power always-on region,
	 * then these registers changing time will be a little long. Thus
	 * here we should delay 200us to wait until values are updated
	 * successfully.
	 */
	usleep_range(200, 210);

	return ret;

}

static int sc27xx_fgu_read_last_cap(struct sprd_fgu_info *info, int *cap)
{
	int ret;
	unsigned int value = 0;

	ret = regmap_read(info->regmap,
			  info->base + SC27XX_FGU_USER_AREA_STATUS0, &value);
	if (ret)
		return ret;

	*cap = (value & SC27XX_FGU_CAP_INTEGER_MASK) * 10;
	*cap += (value >> SC27XX_FGU_CAP_DECIMAL_SHIFT) & SC27XX_FGU_CAP_DECIMAL_MASK;

	return 0;
}

static int sc27xx_fgu_read_normal_temperature_cap(struct sprd_fgu_info *info, int *cap)
{
	int ret;
	unsigned int value = 0;

	ret = regmap_read(info->regmap,
			  info->base + SC27XX_FGU_USER_AREA_STATUS1, &value);
	if (ret)
		return ret;

	*cap = (value & SC27XX_FGU_CAP_INTEGER_MASK) * 10;
	*cap += (value >> SC27XX_FGU_CAP_DECIMAL_SHIFT) & SC27XX_FGU_CAP_DECIMAL_MASK;

	return 0;
}

static int sc27xx_fgu_save_last_cap(struct sprd_fgu_info *info, int cap)
{
	int ret;
	u32 value;

	ret = regmap_update_bits(info->regmap,
				 info->base + SC27XX_FGU_USER_AREA_CLEAR0,
				 SC27XX_FGU_CAP_AREA_MASK,
				 SC27XX_FGU_CAP_AREA_MASK);
	if (ret) {
		dev_err(info->dev, "%d Failed to write user clr, ret = %d\n", __LINE__, ret);
		return ret;
	}

	/*
	 * Since the user area registers are put on power always-on region,
	 * then these registers changing time will be a little long. Thus
	 * here we should delay 200us to wait until values are updated
	 * successfully according to the datasheet.
	 */
	usleep_range(200, 210);

	value = (cap / 10) & SC27XX_FGU_CAP_INTEGER_MASK;
	value |= ((cap % 10) & SC27XX_FGU_CAP_DECIMAL_MASK) << SC27XX_FGU_CAP_DECIMAL_SHIFT;

	ret = regmap_update_bits(info->regmap,
				 info->base + SC27XX_FGU_USER_AREA_SET0,
				 SC27XX_FGU_CAP_AREA_MASK, value);
	if (ret) {
		dev_err(info->dev, "Failed to write user set, ret = %d\n", ret);
		return ret;
	}

	/*
	 * Since the user area registers are put on power always-on region,
	 * then these registers changing time will be a little long. Thus
	 * here we should delay 200us to wait until values are updated
	 * successfully according to the datasheet.
	 */
	usleep_range(200, 210);

	/*
	 * According to the datasheet, we should set the USER_AREA_CLEAR to 0 to
	 * make the user area data available, otherwise we can not save the user
	 * area data.
	 */
	ret = regmap_update_bits(info->regmap,
				 info->base + SC27XX_FGU_USER_AREA_CLEAR0,
				 SC27XX_FGU_CAP_AREA_MASK, 0);
	if (ret) {
		dev_err(info->dev, "%d Failed to write user clr, ret = %d\n", __LINE__, ret);
		return ret;
	}

	/*
	 * Since the user area registers are put on power always-on region,
	 * then these registers changing time will be a little long. Thus
	 * here we should delay 200us to wait until values are updated
	 * successfully.
	 */
	usleep_range(200, 210);

	return ret;
}

/*
 * We get the percentage at the current temperature by multiplying
 * the percentage at normal temperature by the temperature conversion
 * factor, and save the percentage before conversion in the rtc register
 */
static int sc27xx_fgu_save_normal_temperature_cap(struct sprd_fgu_info *info, int cap)
{
	int ret = 0;
	u32 value;

	if (cap == SC27XX_FGU_MAGIC_NUMBER) {
		dev_info(info->dev, "normal_cap = %#x\n", cap);
		return ret;
	}

	ret = regmap_update_bits(info->regmap,
				 info->base + SC27XX_FGU_USER_AREA_CLEAR1,
				 SC27XX_FGU_CAP_AREA_MASK,
				 SC27XX_FGU_CAP_AREA_MASK);
	if (ret) {
		dev_err(info->dev, "%d Failed to write user clr1, ret = %d\n", __LINE__, ret);
		return ret;
	}
	/*
	 * Since the user area registers are put on power always-on region,
	 * then these registers changing time will be a little long. Thus
	 * here we should delay 200us to wait until values are updated
	 * successfully.
	 */
	usleep_range(200, 210);

	value = (cap / 10) & SC27XX_FGU_CAP_INTEGER_MASK;
	value |= ((cap % 10) & SC27XX_FGU_CAP_DECIMAL_MASK) << SC27XX_FGU_CAP_DECIMAL_SHIFT;

	ret = regmap_update_bits(info->regmap,
				 info->base + SC27XX_FGU_USER_AREA_SET1,
				 SC27XX_FGU_CAP_AREA_MASK, value);
	if (ret) {
		dev_err(info->dev, "Failed to write user set1, ret = %d\n", ret);
		return ret;
	}

	/*
	 * Since the user area registers are put on power always-on region,
	 * then these registers changing time will be a little long. Thus
	 * here we should delay 200us to wait until values are updated
	 * successfully.
	 */
	usleep_range(200, 210);

	ret = regmap_update_bits(info->regmap,
				 info->base + SC27XX_FGU_USER_AREA_CLEAR1,
				 SC27XX_FGU_CAP_AREA_MASK, 0);
	if (ret) {
		dev_err(info->dev, "%d Failed to write user clr1, ret = %d\n", __LINE__, ret);
		return ret;
	}

	/*
	 * Since the user area registers are put on power always-on region,
	 * then these registers changing time will be a little long. Thus
	 * here we should delay 200us to wait until values are updated
	 * successfully.
	 */
	usleep_range(200, 210);

	return ret;
}

static int sc27xx_fgu_read_last_batt_ocv(struct sprd_fgu_info *info, int *batt_ocv_mv,
					 int *reg, int *magic)
{
	unsigned int value = 0;

	if (!info->use_miscdata)
		return 0;

	value = info->batt_ocv_mv_magic;
	*reg = value;
	*batt_ocv_mv = value & SC27XX_FGU_LAST_OCV_DATA_MASK;
	*magic = value & SC27XX_FGU_LAST_OCV_MAGIC_MASK;

	dev_info(info->dev, "%s:*magic = 0x%x, *reg = 0x%x\n", __func__, *magic, *reg);

	return 0;
}

static int sc27xx_fgu_read_last_batt_cc_mah(struct sprd_fgu_info *info, int *batt_cc_mah,
					    int *reg, int *magic)
{
	unsigned int value = 0;

	if (!info->use_miscdata)
		return 0;

	value = info->batt_cc_mah_magic;
	*reg = value;
	*batt_cc_mah = (value & SC27XX_FGU_LAST_CC_MAH_DATA_MASK) - SC27XX_FGU_CC_MAH_MAX;
	*magic = value & SC27XX_FGU_LAST_OCV_MAGIC_MASK;

	dev_info(info->dev, "%s:*magic = 0x%x, *reg = 0x%x\n", __func__, *magic, *reg);

	return 0;
}

static int sc27xx_fgu_read_last_batt_temp(struct sprd_fgu_info *info, int *batt_temp,
					  int *reg, int *magic)
{
	unsigned int value = 0;

	if (!info->use_miscdata)
		return 0;

	value = info->batt_ocv_temp_magic;
	*reg = value;
	*batt_temp = (value & SC27XX_FGU_LAST_BATT_TEMP_DATA_MASK) - SC27XX_FGU_BATT_TEMP_MAX;
	*magic = (value & SC27XX_FGU_LAST_OCV_MAGIC_MASK);

	dev_info(info->dev, "%s:*magic = 0x%x, *reg = 0x%x\n", __func__, *magic, *reg);

	return 0;
}

static int sc27xx_fgu_read_last_batt_shutdown_temp(struct sprd_fgu_info *info, int *batt_temp,
					int *reg, int *magic)
{
	unsigned int value = 0;

	if (!info->use_miscdata)
		return 0;

	value = info->batt_shutdown_temp_magic;
	*reg = value;
	*batt_temp = (value & SC27XX_FGU_LAST_BATT_TEMP_DATA_MASK) - SC27XX_FGU_BATT_TEMP_MAX;
	*magic = (value & SC27XX_FGU_LAST_OCV_MAGIC_MASK);

	dev_info(info->dev, "%s:*magic = 0x%x, *reg = 0x%x\n", __func__, *magic, *reg);

	return 0;
}

static int sc27xx_fgu_read_last_smooth_soc(struct sprd_fgu_info *info,
						int *smooth_soc, int *reg,
						int *magic)
{
	int ret;
	unsigned int value = 0;

	ret = regmap_read(info->regmap,
			  info->base + SC27XX_FGU_USER_AREA_STATUS1, &value);
	if (ret)
		return ret;

	*reg = value;

	*smooth_soc = value & SC27XX_FGU_LAST_CC_MAH_DATA_MASK;

	*magic = (value & SC27XX_FGU_LAST_OCV_MAGIC_MASK);

	dev_info(info->dev, "%s:*magic = 0x%x, *reg = 0x%x\n", __func__, *magic,
		 *reg);

	return 0;
}

static int sc27xx_fgu_save_last_batt_ocv(struct sprd_fgu_info *info, int batt_ocv_mv,
					 int is_first_poweron)
{
	int ret = 0;
	u32 value;

	if (!info->use_miscdata)
		return 0;

	mutex_lock(&info->magic_lock);

	if (is_first_poweron) {
		last_cap_magic = SC27XX_FGU_LAST_OCV_MAGIC << SC27XX_FGU_LAST_OCV_MAGIC_SHIFT;
		dev_info(info->dev, "%s:is_first_poweron, last_cap_magic = 0x%x\n",
			 __func__, last_cap_magic);
	} else {
		dev_info(info->dev, "%s:last_cap_magic = 0x%x\n", __func__, last_cap_magic);
		last_cap_magic = (~last_cap_magic) & SC27XX_FGU_LAST_OCV_MAGIC_MASK;
	}
	dev_info(info->dev, "%s:last_cap_magic = 0x%x\n", __func__, last_cap_magic);
	value = (batt_ocv_mv & SC27XX_FGU_LAST_OCV_DATA_MASK) | last_cap_magic;
	dev_info(info->dev, "%s:value = 0x%x, batt_ocv_mv = 0x%x\n", __func__, value, batt_ocv_mv);
	info->batt_ocv_mv_magic = value;
	mutex_unlock(&info->magic_lock);

	return ret;
}

static int sc27xx_fgu_save_last_batt_cc_mah(struct sprd_fgu_info *info, int batt_cc_mah)
{
	int ret = 0;
	u32 value;

	if (!info->use_miscdata)
		return 0;

	mutex_lock(&info->magic_lock);

	if (batt_cc_mah > SC27XX_FGU_CC_MAH_MAX || batt_cc_mah < -SC27XX_FGU_CC_MAH_MAX)
		dev_err(info->dev, "%s:batt_cc_mah = %d, out of range\n", __func__, batt_cc_mah);

	dev_info(info->dev, "%s:last_cap_magic = 0x%x\n", __func__, last_cap_magic);
	/* 14bit max 16384, zero point 8191, < 8191 discharge, > 8191 charge */
	value = (batt_cc_mah + SC27XX_FGU_CC_MAH_MAX) & SC27XX_FGU_LAST_CC_MAH_DATA_MASK;
	value |= last_cap_magic;
	info->batt_cc_mah_magic = value;
	mutex_unlock(&info->magic_lock);

	return ret;
}

static int sc27xx_fgu_save_last_batt_temp(struct sprd_fgu_info *info, int batt_temp)
{
	int ret = 0;
	u32 value;

	if (!info->use_miscdata)
		return 0;

	mutex_lock(&info->magic_lock);

	if (batt_temp > (SC27XX_FGU_BATT_TEMP_MAX * 3) || batt_temp < -SC27XX_FGU_BATT_TEMP_MAX)
		dev_err(info->dev, "%s:batt_temp = %d, out of range\n", __func__, batt_temp);

	dev_info(info->dev, "%s:last_cap_magic = 0x%x\n", __func__, last_cap_magic);
	/* 14bit max 16384, zero point 8191, < 8191 discharge, > 8191 charge */
	value = (batt_temp + SC27XX_FGU_BATT_TEMP_MAX) & SC27XX_FGU_LAST_BATT_TEMP_DATA_MASK;
	value |= last_cap_magic;
	info->batt_ocv_temp_magic = value;
	mutex_unlock(&info->magic_lock);

	return ret;
}

static int sc27xx_fgu_save_last_batt_shutdown_temp(struct sprd_fgu_info *info, int batt_temp)
{
	int ret = 0;
	u32 value;

	if (!info->use_miscdata)
		return 0;

	mutex_lock(&info->magic_lock);

	if (batt_temp > (SC27XX_FGU_BATT_TEMP_MAX * 3) || batt_temp < -SC27XX_FGU_BATT_TEMP_MAX)
		dev_err(info->dev, "%s:batt_temp = %d, out of range\n", __func__, batt_temp);

	dev_info(info->dev, "%s:last_cap_magic = 0x%x\n", __func__, last_cap_magic);
	/* 14bit max 16384, zero point 8191, < 8191 discharge, > 8191 charge */
	value = (batt_temp + SC27XX_FGU_BATT_TEMP_MAX) & SC27XX_FGU_LAST_BATT_TEMP_DATA_MASK;
	value |= last_cap_magic;
	info->batt_shutdown_temp_magic = value;
	mutex_unlock(&info->magic_lock);

	return ret;
}

static int sc27xx_fgu_save_last_smooth_soc(struct sprd_fgu_info *info, int smooth_soc)
{
       int ret;
       u32 value;

       mutex_lock(&info->magic_lock);

       ret = regmap_update_bits(info->regmap,
                                info->base + SC27XX_FGU_USER_AREA_CLEAR1,
                                SC27XX_FGU_LAST_SMOOTH_SOC_MASK,
                                SC27XX_FGU_LAST_SMOOTH_SOC_MASK);
       if (ret) {
               dev_err(info->dev, "%d Failed to write user3 clr, ret = %d\n", __LINE__, ret);
               mutex_unlock(&info->magic_lock);
               return ret;
       }

       /*
        * Since the user area registers are put on power always-on region,
        * then these registers changing time will be a little long. Thus
        * here we should delay 200us to wait until values are updated
        * successfully according to the datasheet.
        */
       usleep_range(300, 310);

       if (smooth_soc > SC27XX_FGU_SMOOTH_SOC_MAX || smooth_soc < 0)
               dev_err(info->dev, "%s:smooth_soc = %d, out of range\n", __func__, smooth_soc);

       dev_info(info->dev, "%s:last_cap_magic = 0x%x\n", __func__, last_cap_magic);
       /* 14bit max 16384, zero point 8191, < 8191 discharge, > 8191 charge */
       value = smooth_soc & SC27XX_FGU_LAST_SMOOTH_SOC_DATA_MASK;
       value |= last_cap_magic;

       ret = regmap_update_bits(info->regmap,
                                info->base + SC27XX_FGU_USER_AREA_SET1,
                                SC27XX_FGU_LAST_SMOOTH_SOC_MASK, value);
       if (ret) {
               dev_err(info->dev, "Failed to write user3 set, ret = %d\n", ret);
               mutex_unlock(&info->magic_lock);
               return ret;
       }

       /*
        * Since the user area registers are put on power always-on region,
        * then these registers changing time will be a little long. Thus
        * here we should delay 200us to wait until values are updated
        * successfully according to the datasheet.
        */
       usleep_range(300, 310);

       /*
        * According to the datasheet, we should set the USER_AREA_CLEAR to 0 to
        * make the user area data available, otherwise we can not save the user
        * area data.
        */
       ret = regmap_update_bits(info->regmap,
                                info->base + SC27XX_FGU_USER_AREA_CLEAR1,
                                SC27XX_FGU_LAST_SMOOTH_SOC_MASK, 0);
       if (ret) {
               dev_err(info->dev, "%d Failed to write user1 clr, ret = %d\n", __LINE__, ret);
               mutex_unlock(&info->magic_lock);
               return ret;
       }

       mutex_unlock(&info->magic_lock);

       /*
        * Since the user area registers are put on power always-on region,
        * then these registers changing time will be a little long. Thus
        * here we should delay 200us to wait until values are updated
        * successfully.
        */
       usleep_range(300, 310);

       return ret;
}

static int sc27xx_fgu_get_clbcnt(struct sprd_fgu_info *info, int *clb_cnt)
{
	int ret = 0, ccl = 0, cch = 0;

	ret = regmap_read(info->regmap, info->base + SC27XX_FGU_CLBCNT_VALL, &ccl);
	if (ret)
		return ret;

	ret = regmap_read(info->regmap, info->base + SC27XX_FGU_CLBCNT_VALH, &cch);
	if (ret)
		return ret;

	*clb_cnt = ccl | (cch << 16);

	return ret;
}

static int sc27xx_fgu_set_clbcnt(struct sprd_fgu_info *info, int clbcnt)
{
	int ret;

	ret = regmap_update_bits(info->regmap,
				 info->base + SC27XX_FGU_CLBCNT_SETL,
				 SC27XX_FGU_CLBCNT_MASK, clbcnt);
	if (ret)
		return ret;

	ret = regmap_update_bits(info->regmap,
				 info->base + SC27XX_FGU_CLBCNT_SETH,
				 SC27XX_FGU_CLBCNT_MASK,
				 clbcnt >> 16);
	if (ret)
		return ret;

	return regmap_update_bits(info->regmap, info->base + SC27XX_FGU_START,
				  SC27XX_WRITE_SELCLB_EN, SC27XX_WRITE_SELCLB_EN);
}

static int sc27xx_fgu_reset_cc_mah(struct sprd_fgu_info *info, int total_mah, int init_cap)
{
	int ret = 0;

	init_clbcnt = sc27xx_fgu_cap2clbcnt(info, total_mah, init_cap);
	start_work_clbcnt = init_clbcnt;
	latest_clbcnt = init_clbcnt;
	ret = sc27xx_fgu_set_clbcnt(info, init_clbcnt);
	if (ret)
		dev_err(info->dev, "failed to initialize coulomb counter\n");

	return ret;
}

static int sc27xx_fgu_get_cc_uah(struct sprd_fgu_info *info, int *cc_uah, bool is_adjust)
{
	int ret = 0, cur_clbcnt;
	s64 delta_clbcnt;

	ret = sc27xx_fgu_get_clbcnt(info, &cur_clbcnt);
	if (ret) {
		dev_err(info->dev, "%s failed to get cur_clbcnt!\n", __func__);
		return ret;
	}

	latest_clbcnt = cur_clbcnt;

	if (is_adjust)
		delta_clbcnt = cur_clbcnt - init_clbcnt;
	else
		delta_clbcnt = cur_clbcnt - start_work_clbcnt;

	*cc_uah = sc27xx_fgu_clbcnt2uah(info, delta_clbcnt);

	return ret;
}

static int sc27xx_fgu_adjust_cap(struct sprd_fgu_info *info, int cap)
{
	int ret;

	dev_dbg(info->dev, "%s:line%d: cap = %d\n", __func__, __LINE__, cap);

	ret = sc27xx_fgu_get_clbcnt(info, &init_clbcnt);
	if (ret)
		dev_err(info->dev, "%s failed to get cur_clbcnt!\n", __func__);

	return cap;
}

static int sc27xx_fgu_set_cap_delta_thre(struct sprd_fgu_info *info, int total_mah, int cap)
{
	int ret = 0;
	s64 delta_clbcnt;

	delta_clbcnt = sc27xx_fgu_cap2clbcnt(info, total_mah, cap);

	ret = regmap_update_bits(info->regmap, info->base + SC27XX_FGU_CLBCNT_DELTL,
				 SC27XX_FGU_CLBCNT_MASK, delta_clbcnt);
	if (ret) {
		dev_err(info->dev, "failed to set delta0 coulomb counter\n");
		return ret;
	}

	ret = regmap_update_bits(info->regmap, info->base + SC27XX_FGU_CLBCNT_DELTH,
				 SC27XX_FGU_CLBCNT_MASK, delta_clbcnt >> 16);
	if (ret)
		dev_err(info->dev, "failed to set delta1 coulomb counter\n");

	return ret;
}

static int sc27xx_fgu_set_relax_cur_thre(struct sprd_fgu_info *info, int relax_cur_threshold)
{
	int ret = 0, relax_cur_threshold_adc;

	relax_cur_threshold_adc = sc27xx_fgu_current2adc(info, relax_cur_threshold);
	ret = regmap_update_bits(info->regmap, info->base + SC27XX_FGU_RELAX_CURT_THRE,
				 SC27XX_FGU_RELAX_CURT_THRE_MASK,
				 (relax_cur_threshold_adc - SC27XX_FGU_CUR_BASIC_ADC) >>
				 SC27XX_FGU_RELAX_CURT_THRE_SHIFT);

	return ret;
}

static inline int sc27xx_fgu_set_relax_cnt_thre(struct sprd_fgu_info *info,
						int relax_cnt_threshold)
{
	return regmap_update_bits(info->regmap, info->base + SC27XX_FGU_RELAX_CNT_THRE,
				  SC27XX_FGU_RELAX_CNT_THRE_MASK,
				  relax_cnt_threshold >> SC27XX_FGU_RELAX_CNT_THRE_SHITF);
}

static int sc27xx_fgu_relax_mode_config(struct sprd_fgu_info *info)
{
	int ret = 0;

	ret = sc27xx_fgu_set_relax_cur_thre(info, info->slp_cap_calib.relax_cur_threshold);
	if (ret) {
		dev_err(info->dev, "Sleep calib Fail to set relax_cur_thre, ret= %d\n", ret);
		return ret;
	}

	ret = sc27xx_fgu_set_relax_cnt_thre(info, info->slp_cap_calib.relax_cnt_threshold);
	if (ret) {
		dev_err(info->dev, "Sleep calib Fail to set relax_cnt_thre, ret= %d\n", ret);
		return ret;
	}

	ret = sc27xx_fgu_enable_fgu_int(info, SPRD_FGU_RELAX_CNT_INT_CMD, true);
	if (ret) {
		dev_err(info->dev, "Sleep calib Fail to enable relax_cnt_int, ret= %d\n", ret);
		return ret;
	}

	dev_info(info->dev, "%s %d Sleep calib mode config done!!!\n", __func__, __LINE__);

	return ret;
}

static inline int sc27xx_fgu_get_reg_val(struct sprd_fgu_info *info, int offset, int *reg_val)
{
	return regmap_read(info->regmap, info->base + offset, reg_val);
}

static inline int sc27xx_fgu_set_reg_val(struct sprd_fgu_info *info, int offset, int reg_val)
{
	return regmap_write(info->regmap, info->base + offset, reg_val);
}

static void sc27xx_fgu_dump_fgu_info(struct sprd_fgu_info *info,
				     enum sprd_fgu_dump_fgu_info_level dump_level)
{
	switch (dump_level) {
	case DUMP_FGU_INFO_LEVEL_0:
		dev_info(info->dev, "dump_level = %d is too low and has no premission to dump fgu info!!!",
			 DUMP_FGU_INFO_LEVEL_0);
		break;
	case DUMP_FGU_INFO_LEVEL_1:
		dev_info(info->dev, "sc27xx_fgu_info : init_clbcnt = %d, start_work_clbcnt = %d, cur_clbcnt = %d, cur_1000ma_adc = %d, vol_1000mv_adc = %d, calib_resist = %d\n",
			 init_clbcnt, start_work_clbcnt, latest_clbcnt, info->cur_1000ma_adc,
			 info->vol_1000mv_adc, info->calib_resist);
		break;
	default:
		dev_err(info->dev, "failed to identify dump_level or dump_level is greater than %d!\n",
			DUMP_FGU_INFO_LEVEL_1);
		break;
	}
}

struct sprd_fgu_device_ops sc27xx_fgu_dev_ops = {
	.enable_fgu_module = sc27xx_fgu_enable_fgu_module,
	.get_fgu_sts = sc27xx_fgu_get_fgu_sts,
	.clr_fgu_int = sc27xx_fgu_clr_fgu_int,
	.clr_fgu_int_all = sc27xx_fgu_clr_fgu_int_all,
	.clr_fgu_int_bit = sc27xx_fgu_clr_fgu_int_bit,
	.enable_relax_cnt_mode = sc27xx_fgu_enable_relax_cnt_mode,
	.set_low_overload = sc27xx_fgu_set_low_overload,
	.set_high_overload = sc27xx_fgu_set_high_overload,
	.enable_fgu_int = sc27xx_fgu_enable_fgu_int,
	.get_fgu_int = sc27xx_fgu_get_fgu_int,
	.suspend_calib_check_relax_counter_sts = sc27xx_fgu_suspend_calib_check_relax_counter_sts,
	.cap2mah = sc27xx_fgu_cap2mah,
	.get_vbat_now = sc27xx_fgu_get_vbat_now,
	.get_vbat_avg = sc27xx_fgu_get_vbat_avg,
	.get_vbat_buf = sc27xx_fgu_get_vbat_buf,
	.get_current_now = sc27xx_fgu_get_current_now,
	.get_current_avg = sc27xx_fgu_get_current_avg,
	.get_current_buf = sc27xx_fgu_get_current_buf,
	.reset_cc_mah = sc27xx_fgu_reset_cc_mah,
	.get_cc_uah = sc27xx_fgu_get_cc_uah,
	.adjust_cap = sc27xx_fgu_adjust_cap,
	.set_cap_delta_thre = sc27xx_fgu_set_cap_delta_thre,
	.relax_mode_config = sc27xx_fgu_relax_mode_config,
	.get_poci = sc27xx_fgu_get_poci,
	.get_pocv = sc27xx_fgu_get_pocv,
	.fgu_calibration = sc27xx_fgu_calibration,
	.is_first_poweron = sc27xx_fgu_is_first_poweron,
	.save_boot_mode = sc27xx_fgu_save_boot_mode,
	.read_last_cap = sc27xx_fgu_read_last_cap,
	.save_last_cap = sc27xx_fgu_save_last_cap,
	.read_normal_temperature_cap = sc27xx_fgu_read_normal_temperature_cap,
	.save_normal_temperature_cap = sc27xx_fgu_save_normal_temperature_cap,
	.read_last_batt_ocv = sc27xx_fgu_read_last_batt_ocv,
	.save_last_batt_ocv = sc27xx_fgu_save_last_batt_ocv,
	.read_last_batt_cc_mah = sc27xx_fgu_read_last_batt_cc_mah,
	.save_last_batt_cc_mah = sc27xx_fgu_save_last_batt_cc_mah,
	.read_last_batt_temp = sc27xx_fgu_read_last_batt_temp,
	.save_last_batt_temp = sc27xx_fgu_save_last_batt_temp,
	.read_last_shutdown_batt_temp = sc27xx_fgu_read_last_batt_shutdown_temp,
	.save_last_shutdown_batt_temp = sc27xx_fgu_save_last_batt_shutdown_temp,
	.get_reg_val = sc27xx_fgu_get_reg_val,
	.set_reg_val = sc27xx_fgu_set_reg_val,
	.dump_fgu_info = sc27xx_fgu_dump_fgu_info,
	.read_last_smooth_cap = sc27xx_fgu_read_last_smooth_soc,
	.save_last_smooth_cap = sc27xx_fgu_save_last_smooth_soc,
};

#if IS_ENABLED(CONFIG_FUEL_GAUGE_SC27XX)
struct sprd_fgu_info *sc27xx_fgu_info_register(struct device *dev)
{
	struct sprd_fgu_info *info = NULL;
	int ret = 0, i;
	const char *value = {0};
	struct device_node *np = dev->of_node;
	int batt_ocv_mv, reg;

	info = devm_kzalloc(dev, sizeof(*info), GFP_KERNEL);
	if (!info)
		return ERR_PTR(-ENOMEM);

	info->ops = devm_kzalloc(dev, sizeof(*info->ops), GFP_KERNEL);
	if (!info->ops)
		return ERR_PTR(-ENOMEM);

	info->dev = dev;

	info->regmap = dev_get_regmap(dev->parent, NULL);
	if (!info->regmap) {
		dev_err(dev, "%s: %d failed to get regmap\n", __func__, __LINE__);
		return ERR_PTR(-ENODEV);
	}

	ret = of_property_read_string_index(np, "compatible", 1, &value);
	if (ret) {
		dev_err(dev, "read_string failed!\n");
		return ERR_PTR(ret);
	}

	for (i = 0; i < ARRAY_SIZE(sc27xx_fgu_dev_match_arr); i++) {
		if (strcmp(sc27xx_fgu_dev_match_arr[i].compatible, value) == 0) {
			info->pdata = sc27xx_fgu_dev_match_arr[i].data;
			break;
		} else if (i == ARRAY_SIZE(sc27xx_fgu_dev_match_arr) - 1) {
			dev_err(dev, "failed match sc27xx dev table!\n");
			return ERR_PTR(-ENODATA);
		}
	}

	ret = device_property_read_u32(dev, "reg", &info->base);
	if (ret) {
		dev_err(dev, "failed to get fgu address\n");
		return ERR_PTR(ret);
	}

	info->use_miscdata = device_property_read_bool(dev, "use-miscdata");

	ret = device_property_read_u32(dev, "sprd,calib-resistance-micro-ohms",
				       &info->calib_resist);
	if (ret) {
		dev_err(dev, "failed to get fgu calibration resistance\n");
		return ERR_PTR(ret);
	}

	/* parse sleep calibration parameters from dts */
	info->slp_cap_calib.support_slp_calib =
		device_property_read_bool(dev, "sprd,capacity-sleep-calibration");
	if (!info->slp_cap_calib.support_slp_calib) {
		dev_warn(dev, "Do not support sleep calibration function\n");
	} else {
		ret = device_property_read_u32(dev, "sprd,relax-current-threshold",
					       &info->slp_cap_calib.relax_cur_threshold);
		if (ret)
			dev_warn(dev, "no relax_current_threshold support\n");

		ret = device_property_read_u32(dev, "sprd,relax-counter-threshold",
					       &info->slp_cap_calib.relax_cnt_threshold);
		if (ret)
			dev_warn(dev, "no relax-counter-threshold support\n");

		if (info->slp_cap_calib.relax_cur_threshold == 0)
			info->slp_cap_calib.relax_cur_threshold =
				SC27XX_FGU_RELAX_CUR_THRESHOLD_MA;

		if (info->slp_cap_calib.relax_cnt_threshold < SC27XX_FGU_RELAX_CNT_THRESHOLD)
			info->slp_cap_calib.relax_cnt_threshold = SC27XX_FGU_RELAX_CNT_THRESHOLD;
	}

	sc27xx_fgu_parse_batt_ocv_uv(info);
	sc27xx_fgu_parse_batt_ocv_temp(info);
	sc27xx_fgu_parse_batt_shutdown_temp(info);
	sc27xx_fgu_parse_batt_cc_uah(info);

	dev_info(info->dev, "batt_ocv_mv_magic = %d, batt_ocv_temp_magic = %d, batt_cc_mah_magic = %d, batt_shutdown_temp_magic = %d\n",
		 info->batt_ocv_mv_magic, info->batt_ocv_temp_magic,
		 info->batt_cc_mah_magic, info->batt_shutdown_temp_magic);

	mutex_init(&info->magic_lock);
	info->ops = &sc27xx_fgu_dev_ops;

	mutex_lock(&info->magic_lock);
	ret = sc27xx_fgu_read_last_batt_ocv(info, &batt_ocv_mv, &reg, &last_cap_magic);
	if (ret)
		dev_warn(dev, "no last_cap_magic\n");
	mutex_unlock(&info->magic_lock);

	dev_info(info->dev, "%s:last_cap_magic = 0x%x\n", __func__, last_cap_magic);

	return info;
}
EXPORT_SYMBOL_GPL(sc27xx_fgu_info_register);
#endif

MODULE_LICENSE("GPL v2");
