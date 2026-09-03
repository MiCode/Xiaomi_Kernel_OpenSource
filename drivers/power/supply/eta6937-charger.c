// SPDX-License-Identifier: GPL-2.0:
// Copyright (c) 2021 unisoc.

/*
 * Driver for the ETA Solutions eta6937 charger.
 * Author: Jinfeng.Lin1 <jinfeng.lin1@unisoc.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/alarmtimer.h>
#include <linux/interrupt.h>
#include <linux/i2c.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/power_supply.h>
#include <linux/power/charger-manager.h>
#include <linux/power/sprd_battery_info.h>
#include <linux/regmap.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>
#include <linux/slab.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/pm_wakeup.h>

#define ETA6937_REG_0					0x0
#define ETA6937_REG_1					0x1
#define ETA6937_REG_2					0x2
#define ETA6937_REG_3					0x3
#define ETA6937_REG_4					0x4
#define ETA6937_REG_5					0x5
#define ETA6937_REG_6					0x6
#define ETA6937_REG_7					0x7
/* Register bits */
/* ETA6937_REG_0 (0x00) status and control reg*/
#define ETA6937_REG_TMR_RST_OTG_STAT_MASK		GENMASK(7, 7)
#define ETA6937_REG_TMR_RST_OTG_STAT_SHIFT		(7)
#define ETA6937_REG_EN_STAT_MASK			GENMASK(6, 6)
#define ETA6937_REG_EN_STAT_SHIFT			(6)
#define ETA6937_REG_STAT1_STAT2_MASK			GENMASK(5, 4)
#define ETA6937_REG_STAT1_STAT2_SHIFT			(4)
#define ETA6937_REG_BOOST_MASK				GENMASK(3, 3)
#define ETA6937_REG_BOOST_SHIFT				(3)
#define ETA6937_REG_FAULT_MASK				GENMASK(2, 0)
#define ETA6937_REG_FAULT_SHIFT				(0)

/* FAN5405_REG_1 (0x01) control reg */
#define ETA6937_REG_IIN_LIMIT_1_MASK			GENMASK(7, 6)
#define ETA6937_REG_IIN_LIMIT_1_SHIFT			(6)
#define ETA6937_REG_VLOWV_MASK				GENMASK(5, 4)
#define ETA6937_REG_VLOWV_SHIFT				(4)
#define ETA6937_REG_TE_MASK				GENMASK(3, 3)
#define ETA6937_REG_TE_SHIFT				(3)
#define ETA6937_REG_NCE_MASK				GENMASK(2, 2)
#define ETA6937_REG_NCE_SHIFT				(2)
#define ETA6937_REG_HZ_MODE_MASK			GENMASK(1, 1)
#define ETA6937_REG_HZ_MODE_SHIFT			(1)
#define ETA6937_REG_OPA_MODE_MASK			GENMASK(0, 0)
#define ETA6937_REG_OPA_MODE_SHIFT			(0)

/* ETA6937_REG_2 (0x02) control and battery voltage reg*/
#define ETA6937_REG_VOREG_MASK				GENMASK(7, 2)
#define ETA6937_REG_VOREG_SHIFT				(2)
#define ETA6937_REG_OTG_PL_MASK				GENMASK(1, 1)
#define ETA6937_REG_OTG_PL_SHIFT			(1)
#define ETA6937_REG_OTG_EN_MASK				GENMASK(0, 0)
#define ETA6937_REG_OTG_EN_SHIFT			(0)

/* ETA6937_REG_3 (0x03) Vendor/Part/Revision reg */
#define ETA6937_REG_VENDOR_CODE_MASK			GENMASK(7, 5)
#define ETA6937_REG_VENDOR_CODE_SHIFT			(5)
#define ETA6937_REG_PN_CODE_MASK			GENMASK(4, 3)
#define ETA6937_REG_PN_CODE_SHIFT			(3)
#define ETA6937_DEV_ID					2
#define ETA6937_REG_REV_CODE_MASK			GENMASK(2, 0)
#define ETA6937_REG_REV_CODE_SHIFT			(0)

/* ETA6937_REG_4 (0x04) battery termination and fast charge current reg*/
#define ETA6937_REG_RESET_MASK				GENMASK(7, 7)
#define ETA6937_REG_RESET_SHIFT				(7)
#define ETA6937_REG_ICHG_MASK				GENMASK(6, 4)
#define ETA6937_REG_ICHG_SHIFT				(4)
#define ETA6937_REG_ICHG_OFFSET_MASK			GENMASK(3, 3)
#define ETA6937_REG_ICHG_OFFSET_SHIFT			(3)
#define ETA6937_REG_ITERM_MASK				GENMASK(2, 0)
#define ETA6937_REG_ITERM_SHIFT				(0)

/* ETA6937_REG_5 (0x05) special charger voltage and enable pin status reg*/
#define ETA6937_REG_ICHG_4_MASK				GENMASK(7, 7)
#define ETA6937_REG_ICHG_4_SHIFT			(7)
#define ETA6937_REG_ICHG_3_MASK				GENMASK(6, 6)
#define ETA6937_REG_ICHG_3_SHIFT			(6)
#define ETA6937_REG_LOW_CHG_MASK			GENMASK(5, 5)
#define ETA6937_REG_LOW_CHG_SHIFT			(5)
#define ETA6937_REG_DPM_STATUS_MASK		        GENMASK(4, 4)
#define ETA6937_REG_DPM_STATUS_SHIFT			(4)
#define ETA6937_REG_CD_STATUS_MASK			GENMASK(3, 3)
#define ETA6937_REG_CD_STATUS_SHIFT			(3)
#define ETA6937_REG_VINDPM_MASK				GENMASK(2, 0)
#define ETA6937_REG_VINDPM_SHIFT			(0)

/* ETA6937_REG_6 (0x06) savety limit reg*/
#define ETA6937_REG_IMCHRG_MASK				GENMASK(7, 4)
#define ETA6937_REG_IMCHRG_SHIFT			(4)
#define ETA6937_REG_VMREG_MASK				GENMASK(3, 0)
#define ETA6937_REG_VMREG_SHIFT				(0)

/* ETA6937_REG_7 (0x07) etra current limit and DPM Level Setting reg */
#define ETA6937_REG_VINDPM_ADD_MASK                     GENMASK(7, 4)
#define ETA6937_REG_VINDPM_ADD_SHIFT			(4)
#define ETA6937_REG_EN_ILIM_2_MASK			GENMASK(3, 3)
#define ETA6937_REG_EN_ILIM_2_SHIFT			(3)
#define ETA6937_REG_IIN_LIMIT_2_MASK			GENMASK(2, 0)
#define ETA6937_REG_IIN_LIMIT_2_SHIFT			(0)

#define ETA6937_BATTERY_NAME				"sc27xx-fgu"
#define BIT_DP_DM_BC_ENB				BIT(0)
#define ETA6937_OTG_VALID_MS				(500)
#define ETA6937_FEED_WATCHDOG_VALID_MS			(50)
#define ETA6937_WDG_TIMER_S				(15)

#define ETA6937_OTG_TIMER_FAULT				(0x6)

#define ETA6937_DISABLE_PIN_MASK_2730			BIT(0)
#define ETA6937_DISABLE_PIN_MASK_2721			BIT(15)
#define ETA6937_DISABLE_PIN_MASK_2720			BIT(0)

#define ETA6937_CHG_IMMIN		(550000)
#define ETA6937_CHG_IMSTEP		(200000)
#define ETA6937_CHG_IMMAX		(3050000)
#define ETA6937_CHG_IMIN		(550000)
#define ETA6937_CHG_ISTEP		(100000)
#define ETA6937_CHG_IMAX		(3050000)
#define ETA6937_CHG_IMAX30500		(0x19)

#define ETA6937_CHG_VMREG_MIN		(4200)
#define ETA6937_CHG_VMREG_STEP		(20)
#define ETA6937_CHG_VMREG_MAX		(4440)
#define ETA6937_CHG_VOREG_MIN		(3500)
#define ETA6937_CHG_VOREG_STEP		(20)
#define ETA6937_CHG_VOREG_MAX		(4440)

#define ETA6937_REG_VOREG		(BIT(5) | BIT(3))
#define ETA6937_IIN_LIM_SEL		(1)
#define ETA6937_REG_ICHG_OFFSET		(0)
#define ETA6937_REG_IIN_LIMIT1_MAX	(3000000)

#define ENABLE_CHARGE 0
#define DISABLE_CHARGE 1

static int eta6937_max_chg_cur[] = {
	550000,
	750000,
	950000,
	1150000,
	1350000,
	1550000,
	1750000,
	1950000,
	2150000,
	2350000,
	2550000,
	2750000,
	2950000,
	3050000,
};

static int eta6937_iin_lim1_tbl[] = {
	100000,
	500000,
	800000,
};

static int eta6937_iin_lim2_tbl[] = {
	300000,
	500000,
	800000,
	1200000,
	1500000,
	2000000,
	3000000,
	5000000,
};

struct eta6937_charge_current {
	int sdp_limit;
	int sdp_cur;
	int dcp_limit;
	int dcp_cur;
	int cdp_limit;
	int cdp_cur;
	int unknown_limit;
	int unknown_cur;
	int fchg_limit;
	int fchg_cur;
};

struct eta6937_charger_info {
	struct i2c_client *client;
	struct device *dev;
	struct power_supply *psy_usb;
	struct eta6937_charge_current cur;
	struct mutex lock;
	bool charging;
	bool is_charger_online;
	struct delayed_work otg_work;
	struct delayed_work wdt_work;
	struct regmap *pmic;
	u32 charger_detect;
	u32 charger_pd;
	u32 charger_pd_mask;
	struct gpio_desc *gpiod;
	struct extcon_dev *edev;
	bool otg_enable;
	struct alarm wdg_timer;
	bool need_reinit;
	int termination_cur;
};

static int eta6937_charger_set_limit_current(struct eta6937_charger_info *info, u32 limit_cur);

static int eta6937_read(struct eta6937_charger_info *info, u8 reg, u8 *data)
{
	int ret;

	ret = i2c_smbus_read_byte_data(info->client, reg);
	if (ret < 0)
		return ret;

	*data = ret;
	return 0;
}

static int eta6937_write(struct eta6937_charger_info *info, u8 reg, u8 data)
{
	return i2c_smbus_write_byte_data(info->client, reg, data);
}

static int eta6937_update_bits(struct eta6937_charger_info *info, u8 reg, u8 mask, u8 data)
{
	u8 v;
	int ret;

	ret = eta6937_read(info, reg, &v);
	if (ret < 0)
		return ret;

	v &= ~mask;
	v |= (data & mask);

	return eta6937_write(info, reg, v);
}

static int eta6937_charger_set_max_batt_reg_vol(struct eta6937_charger_info *info, u32 vol)
{
	u8 reg_val;

	if (vol < ETA6937_CHG_VMREG_MIN)
		vol = ETA6937_CHG_VMREG_MIN;
	else if (vol > ETA6937_CHG_VMREG_MAX)
		vol = ETA6937_CHG_VMREG_MAX;

	reg_val = (vol - ETA6937_CHG_VMREG_MIN) / ETA6937_CHG_VMREG_STEP;

	return eta6937_update_bits(info, ETA6937_REG_6, ETA6937_REG_VMREG_MASK,
				   (reg_val << ETA6937_REG_VMREG_SHIFT));
}

static int eta6937_charger_set_termina_vol(struct eta6937_charger_info *info, u32 vol)
{
	u8 reg_val;

	if (vol < ETA6937_CHG_VOREG_MIN)
		vol = ETA6937_CHG_VOREG_MIN;
	else if (vol > ETA6937_CHG_VOREG_MAX)
		vol =  ETA6937_CHG_VOREG_MAX;

	reg_val = (vol - ETA6937_CHG_VOREG_MIN) / ETA6937_CHG_VOREG_STEP;

	return eta6937_update_bits(info, ETA6937_REG_2, ETA6937_REG_VOREG_MASK,
				   reg_val << ETA6937_REG_VOREG_SHIFT);
}

static int eta6937_charger_set_max_chg_cur(struct eta6937_charger_info *info, u32 cur)
{
	u8 reg_val;
	int i;

	/*if ICHG_OFFSET = 1, chg cur + 100mA*/
	cur -= 100 * ETA6937_REG_ICHG_OFFSET;
	for (i = 1; i < ARRAY_SIZE(eta6937_max_chg_cur); ++i)
		if (cur < eta6937_max_chg_cur[i])
			break;

	reg_val = i - 1;

	return eta6937_update_bits(info, ETA6937_REG_6, ETA6937_REG_IMCHRG_MASK,
				    reg_val << ETA6937_REG_IMCHRG_SHIFT);
}

static int eta6937_charger_hw_init(struct eta6937_charger_info *info)
{
	struct sprd_battery_info bat_info = {};
	int voltage_max_microvolt, termination_cur;
	int ret;

	ret = sprd_battery_get_battery_info(info->psy_usb, &bat_info, 0);
	if (ret) {
		dev_warn(info->dev, "no battery information is supplied\n");

		info->cur.sdp_limit = 500000;
		info->cur.sdp_cur = 500000;
		info->cur.dcp_limit = 1500000;
		info->cur.dcp_cur = 1500000;
		info->cur.cdp_limit = 1000000;
		info->cur.cdp_cur = 1000000;
		info->cur.unknown_limit = 500000;
		info->cur.unknown_cur = 500000;

		/*
		 * If no battery information is supplied, we should set
		 * default charge termination current to 120 mA, and default
		 * charge termination voltage to 4.44V.
		 */
		voltage_max_microvolt = 4440;
		termination_cur = 120;
		info->termination_cur = termination_cur;
	} else {
		info->cur.sdp_limit = bat_info.cur.sdp_limit;
		info->cur.sdp_cur = bat_info.cur.sdp_cur;
		info->cur.dcp_limit = bat_info.cur.dcp_limit;
		info->cur.dcp_cur = bat_info.cur.dcp_cur;
		info->cur.cdp_limit = bat_info.cur.cdp_limit;
		info->cur.cdp_cur = bat_info.cur.cdp_cur;
		info->cur.unknown_limit = bat_info.cur.unknown_limit;
		info->cur.unknown_cur = bat_info.cur.unknown_cur;
		info->cur.fchg_limit = bat_info.cur.fchg_limit;
		info->cur.fchg_cur = bat_info.cur.fchg_cur;

		voltage_max_microvolt = bat_info.constant_charge_voltage_max_uv / 1000;
		termination_cur = bat_info.charge_term_current_ua / 1000;
		info->termination_cur = termination_cur;
		sprd_battery_put_battery_info(info->psy_usb, &bat_info);
	}

	if (of_device_is_compatible(info->dev->of_node,
				    "eta,eta6937_chg")) {
		ret = eta6937_update_bits(info, ETA6937_REG_4,
					  ETA6937_REG_RESET_MASK,
					  ETA6937_REG_RESET_MASK);
		if (ret) {
			dev_err(info->dev, "reset eta6937 failed ret = %d\n", ret);
			return ret;
		}

		ret = eta6937_charger_set_max_batt_reg_vol(info,
					voltage_max_microvolt);
		if (ret) {
			dev_err(info->dev, "set eta6937 safety vol failed ret = %d\n", ret);
			return ret;
		}

		ret = eta6937_charger_set_max_chg_cur(info,
					info->cur.dcp_cur);
		if (ret) {
			dev_err(info->dev, "set eta6937 safety cur failed, ret = %d\n", ret);
			return ret;
		}
	}

	/* do not force charge to 550mA */
	ret = eta6937_update_bits(info, ETA6937_REG_5, ETA6937_REG_LOW_CHG_MASK, 0);
	if (ret) {
		dev_err(info->dev, "release eta6937 force charge failed ret = %d\n", ret);
		return ret;
	}
	/* set wake battery voltage threshold 3.4V */
	ret = eta6937_update_bits(info, ETA6937_REG_1, ETA6937_REG_VLOWV_MASK, 0);
	if (ret) {
		dev_err(info->dev, "set eta6937 weak voltage threshold failed ret = %d\n", ret);
		return ret;
	}
	/* set ichg offset , ichg begain from 550mA or 650mA*/
	ret = eta6937_update_bits(info, ETA6937_REG_4, ETA6937_REG_ICHG_OFFSET_MASK,
				  ETA6937_REG_ICHG_OFFSET << ETA6937_REG_ICHG_OFFSET_SHIFT);
	if (ret) {
		dev_err(info->dev, "set eta6937 io level failed ret = %d\n", ret);
		return ret;
	}
	/*set VOREG 4.3V?*/
	ret = eta6937_update_bits(info, ETA6937_REG_2, ETA6937_REG_VOREG_MASK,
				  ETA6937_REG_VOREG << ETA6937_REG_VOREG_SHIFT);
	if (ret) {
		dev_err(info->dev, "set eta6937 VOREG failed ret = %d\n", ret);
		return ret;
	}
	/*Disable termination charge current function*/
	ret = eta6937_update_bits(info, ETA6937_REG_1, ETA6937_REG_TE_MASK, 0);
	if (ret) {
		dev_err(info->dev, "set eta6937 terminal cur failed ret = %d\n", ret);
		return ret;
	}

	/*Feed Watchdog*/
	ret = eta6937_update_bits(info,	ETA6937_REG_0, ETA6937_REG_TMR_RST_OTG_STAT_MASK,
				  ETA6937_REG_TMR_RST_OTG_STAT_MASK);
	if (ret) {
		dev_err(info->dev, "feed eta6937 watchdog failed ret = %d\n", ret);
		return ret;
	}

	/*input current limit sel*/
	ret = eta6937_update_bits(info,	ETA6937_REG_7, ETA6937_REG_EN_ILIM_2_MASK,
				  ETA6937_IIN_LIM_SEL << ETA6937_REG_EN_ILIM_2_SHIFT);
	if (ret) {
		dev_err(info->dev, "feed eta6937 watchdog failed ret = %d\n", ret);
		return ret;
	}

	ret = eta6937_charger_set_termina_vol(info, voltage_max_microvolt);
	if (ret) {
		dev_err(info->dev, "set eta6937 terminal vol failed ret = %d\n", ret);
		return ret;
	}

	ret = eta6937_charger_set_limit_current(info, info->cur.unknown_cur);
	if (ret)
		dev_err(info->dev, "set eta6937 limit current failed ret = %d\n", ret);

	return ret;
}

static int eta6937_charger_start_charge(struct eta6937_charger_info *info)
{
	int ret = 0;

	if (info->gpiod) {
		gpiod_set_value_cansleep(info->gpiod, ENABLE_CHARGE);
	} else {
		ret = regmap_update_bits(info->pmic, info->charger_pd,
					 info->charger_pd_mask, 0);
		if (ret)
			dev_err(info->dev, "enable eta6937 charge failed ret = %d\n", ret);
	}

	return ret;
}

static void eta6937_charger_stop_charge(struct eta6937_charger_info *info)
{
	int ret;

	if (info->gpiod) {
		gpiod_set_value_cansleep(info->gpiod, DISABLE_CHARGE);
	} else {
		ret = regmap_update_bits(info->pmic, info->charger_pd,
					 info->charger_pd_mask,
					 info->charger_pd_mask);
		if (ret)
			dev_err(info->dev, "disable eta6937 charge failed ret = %d\n", ret);
	}
}

static int eta6937_charger_set_current(struct eta6937_charger_info *info, u32 cur)
{
	u8 reg_val;
	int ret;

	/*if ICHG_OFFSET = 1, chg cur + 100mA*/
	cur -= 100 * ETA6937_REG_ICHG_OFFSET;

	if (cur < ETA6937_CHG_IMIN)
		cur = ETA6937_CHG_IMIN;
	else if (cur > ETA6937_CHG_IMAX)
		cur = ETA6937_CHG_IMAX;

	reg_val = (cur - ETA6937_CHG_IMIN) / ETA6937_CHG_ISTEP;
	/*set reg 4 ichg[2:0]*/
	ret = eta6937_update_bits(info, ETA6937_REG_4, ETA6937_REG_ICHG_MASK,
				    (reg_val & 0x7) << ETA6937_REG_ICHG_SHIFT);
	if (ret) {
		dev_err(info->dev, "set eta6937 ichg[2:0] cur failed ret = %d\n", ret);
		return ret;
	}
	/*set reg5 ichg[4:3]*/
	ret = eta6937_update_bits(info, ETA6937_REG_5, ETA6937_REG_ICHG_3_MASK | ETA6937_REG_ICHG_4_MASK,
				    ((reg_val & 0x18) >> 0x3) << ETA6937_REG_ICHG_3_SHIFT);
	if (ret)
		dev_err(info->dev, "set eta6937 ichg[4:3] cur failed ret = %d\n", ret);

	return ret;
}

static int eta6937_charger_get_current(struct eta6937_charger_info *info, u32 *cur)
{
	u8 reg_val = 0, val;
	int ret;

	ret = eta6937_read(info, ETA6937_REG_4, &val);
	if (ret < 0) {
		dev_err(info->dev, "set eta6937 ichg[2:0] cur failed ret = %d\n", ret);
		return ret;
	}

	val &= ETA6937_REG_ICHG_MASK;
	val = val >> ETA6937_REG_ICHG_SHIFT;
	reg_val |= val;

	ret = eta6937_read(info, ETA6937_REG_5, &val);
	if (ret < 0) {
		dev_err(info->dev, "set eta6937 ichg[3:4] cur failed ret = %d\n", ret);
		return ret;
	}

	val &= (ETA6937_REG_ICHG_3_MASK | ETA6937_REG_ICHG_4_MASK);
	val = (val >> ETA6937_REG_ICHG_3_SHIFT << 0x3);
	reg_val |= val;

	if (reg_val > ETA6937_CHG_IMAX30500)
		reg_val = ETA6937_CHG_IMAX30500;

	*cur = reg_val * ETA6937_CHG_ISTEP + ETA6937_CHG_IMIN + 100 * ETA6937_REG_ICHG_OFFSET;

	return 0;
}

static int eta6937_charger_set_limit1_current(struct eta6937_charger_info *info, u32 limit_cur)
{
	u8 reg_val;
	int i;

	if (limit_cur > eta6937_iin_lim1_tbl[ARRAY_SIZE(eta6937_iin_lim1_tbl) - 1]) {
		reg_val = ARRAY_SIZE(eta6937_iin_lim1_tbl);
	} else {
		for (i = 1; i < ARRAY_SIZE(eta6937_iin_lim1_tbl); ++i)
			if (limit_cur < eta6937_iin_lim1_tbl[i])
				break;

		reg_val = i - 1;
	}

	return eta6937_update_bits(info, ETA6937_REG_1, ETA6937_REG_IIN_LIMIT_1_MASK,
				   reg_val << ETA6937_REG_IIN_LIMIT_1_SHIFT);
}


static int eta6937_charger_set_limit2_current(struct eta6937_charger_info *info, u32 limit_cur)
{
	u8 reg_val;
	int i;

	for (i = 1; i < ARRAY_SIZE(eta6937_iin_lim2_tbl); ++i)
		if (limit_cur < eta6937_iin_lim2_tbl[i])
			break;

	reg_val = i - 1;

	return eta6937_update_bits(info, ETA6937_REG_7, ETA6937_REG_IIN_LIMIT_2_MASK,
				    reg_val << ETA6937_REG_IIN_LIMIT_2_SHIFT);
}

static int eta6937_charger_set_limit_current(struct eta6937_charger_info *info, u32 limit_cur)
{
	int ret;

	if (ETA6937_IIN_LIM_SEL)
		ret = eta6937_charger_set_limit2_current(info, limit_cur);
	else
		ret = eta6937_charger_set_limit1_current(info, limit_cur);

	if (ret)
		dev_err(info->dev, "set eta6937 ichg[2:0] cur failed ret = %d\n", ret);

	return ret;
}

static int eat6937_charger_get_limit1_current(struct eta6937_charger_info *info, u32 *limit_cur)
{
	u8 reg_val;
	int ret;

	ret = eta6937_read(info, ETA6937_REG_1, &reg_val);
	if (ret < 0) {
		dev_err(info->dev, "set eta6937 limit1 cur failed ret = %d\n", ret);
		return ret;
	}

	reg_val &= ETA6937_REG_IIN_LIMIT_1_MASK;
	reg_val = reg_val >> ETA6937_REG_IIN_LIMIT_1_SHIFT;

	if (reg_val >= ARRAY_SIZE(eta6937_iin_lim1_tbl))
		*limit_cur = ETA6937_REG_IIN_LIMIT1_MAX;
	else
		*limit_cur = eta6937_iin_lim1_tbl[reg_val];

	return ret;
}

static int eat6937_charger_get_limit2_current(struct eta6937_charger_info *info, u32 *limit_cur)
{
	u8 reg_val;
	int ret;

	ret = eta6937_read(info, ETA6937_REG_7, &reg_val);
	if (ret < 0) {
		dev_err(info->dev, "set eta6937 limit2 cur failed ret = %d\n", ret);
		return ret;
	}

	reg_val &= ETA6937_REG_IIN_LIMIT_2_MASK;
	reg_val = reg_val >> ETA6937_REG_IIN_LIMIT_2_SHIFT;

	if (reg_val >= ARRAY_SIZE(eta6937_iin_lim2_tbl))
		reg_val = ARRAY_SIZE(eta6937_iin_lim2_tbl) - 1;

	*limit_cur = eta6937_iin_lim2_tbl[reg_val];

	return ret;
}

static int eta6937_charger_get_limit_current(struct eta6937_charger_info *info, u32 *limit_cur)
{
	int ret;

	if (ETA6937_IIN_LIM_SEL)
		ret = eat6937_charger_get_limit2_current(info, limit_cur);
	else
		ret = eat6937_charger_get_limit1_current(info, limit_cur);

	if (ret)
		dev_err(info->dev, "set eta6937  get limit cur failed ret = %d\n", ret);

	return ret;
}

static int eta6937_charger_get_health(struct eta6937_charger_info *info, u32 *health)
{
	*health = POWER_SUPPLY_HEALTH_GOOD;

	return 0;
}

static int eta6937_charger_feed_watchdog(struct eta6937_charger_info *info)
{
	int ret;

	ret = eta6937_update_bits(info, ETA6937_REG_0, ETA6937_REG_TMR_RST_OTG_STAT_MASK,
				  ETA6937_REG_TMR_RST_OTG_STAT_MASK);
	if (ret)
		dev_err(info->dev, "eta6937 feed watchdog failed ret = %d\n", ret);

	return ret;
}

static int eta6937_charger_get_status(struct eta6937_charger_info *info)
{
	if (info->charging)
		return POWER_SUPPLY_STATUS_CHARGING;
	else
		return POWER_SUPPLY_STATUS_NOT_CHARGING;
}

static int eta6937_charger_set_status(struct eta6937_charger_info *info, int val)
{
	int ret = 0;

	if (val == CM_BUCK_MAX_TERMINA_VOL) {
		ret = eta6937_charger_set_termina_vol(info, ETA6937_CHG_VOREG_MAX);
		if (ret) {
			dev_err(info->dev, "failed to set terminate max voltage\n");
			return ret;
		}
	}

	if (val > CM_FAST_CHARGE_NORMAL_CMD)
		return 0;

	if (!val && info->charging) {
		eta6937_charger_stop_charge(info);
		info->charging = false;
	} else if (val && !info->charging) {
		ret = eta6937_charger_start_charge(info);
		if (ret)
			dev_err(info->dev, "start charge failed\n");
		else
			info->charging = true;
	}

	return ret;
}

static int eta6937_charger_usb_get_property(struct power_supply *psy,
					     enum power_supply_property psp,
					     union power_supply_propval *val)
{
	struct eta6937_charger_info *info = power_supply_get_drvdata(psy);
	u32 cur, health;
	int ret = 0;

	if (!info) {
		pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
		return -EINVAL;
	}

	mutex_lock(&info->lock);

	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
		if (val->intval == CM_BUCK_MAX_TERMINA_VOL) {
			val->intval = ETA6937_CHG_VOREG_MAX * 1000;
			break;
		}

		val->intval = eta6937_charger_get_status(info);
		break;

	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT:
		if (!info->charging) {
			val->intval = 0;
		} else {
			ret = eta6937_charger_get_current(info, &cur);
			if (ret)
				goto out;

			val->intval = cur;
		}
		break;

	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
		if (!info->charging) {
			val->intval = 0;
		} else {
			ret = eta6937_charger_get_limit_current(info, &cur);
			if (ret)
				goto out;

			val->intval = cur;
		}
		break;

	case POWER_SUPPLY_PROP_HEALTH:
		if (info->charging) {
			val->intval = 0;
		} else {
			ret = eta6937_charger_get_health(info, &health);
			if (ret)
				goto out;

			val->intval = health;
		}
		break;

	default:
		ret = -EINVAL;
	}

out:
	mutex_unlock(&info->lock);
	return ret;
}

static int eta6937_charger_usb_set_property(struct power_supply *psy,
					    enum power_supply_property psp,
					    const union power_supply_propval *val)
{
	struct eta6937_charger_info *info = power_supply_get_drvdata(psy);
	int ret = 0;

	if (!info) {
		pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
		return -EINVAL;
	}

	mutex_lock(&info->lock);

	switch (psp) {
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT:
		ret = eta6937_charger_set_current(info, val->intval);
		if (ret < 0)
			dev_err(info->dev, "set charge current failed\n");
		break;
	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
		ret = eta6937_charger_set_limit_current(info, val->intval);
		if (ret < 0)
			dev_err(info->dev, "set input current limit failed\n");
		break;

	case POWER_SUPPLY_PROP_STATUS:
		ret = eta6937_charger_set_status(info, val->intval);
		if (ret < 0)
			dev_err(info->dev, "set charge status failed\n");
		break;

	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE_MAX:
		ret = eta6937_charger_set_termina_vol(info, val->intval / 1000);
		if (ret < 0)
			dev_err(info->dev, "failed to set terminate voltage\n");
		break;

	case POWER_SUPPLY_PROP_PRESENT:
		info->is_charger_online = val->intval;
		if (val->intval == true)
			schedule_delayed_work(&info->wdt_work, 0);
		else
			cancel_delayed_work_sync(&info->wdt_work);
		break;

	default:
		ret = -EINVAL;
	}

	mutex_unlock(&info->lock);
	return ret;
}

static int eta6937_charger_property_is_writeable(struct power_supply *psy,
						enum power_supply_property psp)
{
	int ret;

	switch (psp) {
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT:
	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
	case POWER_SUPPLY_PROP_STATUS:
	case POWER_SUPPLY_PROP_PRESENT:
		ret = 1;
		break;

	default:
		ret = 0;
	}

	return ret;
}

static enum power_supply_property eta6937_usb_props[] = {
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT,
	POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT,
	POWER_SUPPLY_PROP_HEALTH,
};

static const struct power_supply_desc eta6937_charger_desc = {
	.name			= "eta6937_charger",
	.type			= POWER_SUPPLY_TYPE_UNKNOWN,
	.properties		= eta6937_usb_props,
	.num_properties		= ARRAY_SIZE(eta6937_usb_props),
	.get_property		= eta6937_charger_usb_get_property,
	.set_property		= eta6937_charger_usb_set_property,
	.property_is_writeable	= eta6937_charger_property_is_writeable,
};

static void eta6937_charger_feed_watchdog_work(struct work_struct *work)
{
	struct delayed_work *dwork = to_delayed_work(work);
	struct eta6937_charger_info *info = container_of(dwork,
							 struct eta6937_charger_info,
							 wdt_work);
	int ret;

	if (!info) {
		pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
		return;
	}

	ret = eta6937_charger_feed_watchdog(info);
	if (ret)
		schedule_delayed_work(&info->wdt_work, HZ * 5);
	else
		schedule_delayed_work(&info->wdt_work, HZ * 15);
}

#if IS_ENABLED(CONFIG_REGULATOR)
static void eta6937_charger_otg_work(struct work_struct *work)
{
	struct delayed_work *dwork = to_delayed_work(work);
	struct eta6937_charger_info *info = container_of(dwork,
			struct eta6937_charger_info, otg_work);
	int ret;

	if (!extcon_get_state(info->edev, EXTCON_USB)) {
		ret = eta6937_update_bits(info, ETA6937_REG_1,
					   ETA6937_REG_HZ_MODE_MASK |
					   ETA6937_REG_OPA_MODE_MASK,
					   ETA6937_REG_OPA_MODE_MASK);
		if (ret)
			dev_err(info->dev, "restart eta6937 charger otg failed, ret = %d\n", ret);
	}

	schedule_delayed_work(&info->otg_work, msecs_to_jiffies(500));
}

static int eta6937_charger_enable_otg(struct regulator_dev *dev)
{
	struct eta6937_charger_info *info = rdev_get_drvdata(dev);
	int ret;

	if (!info) {
		pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
		return -EINVAL;
	}

	/*
	 * Disable charger detection function in case
	 * affecting the OTG timing sequence.
	 */
	ret = regmap_update_bits(info->pmic, info->charger_detect,
				 BIT_DP_DM_BC_ENB, BIT_DP_DM_BC_ENB);
	if (ret) {
		dev_err(info->dev, "failed to disable bc1.2 detect function. ret = %d\n", ret);
		return ret;
	}

	ret = eta6937_update_bits(info, ETA6937_REG_1,
				   ETA6937_REG_HZ_MODE_MASK |
				   ETA6937_REG_OPA_MODE_MASK,
				   ETA6937_REG_OPA_MODE_MASK);
	if (ret) {
		dev_err(info->dev, "enable eta6937 otg failed ret = %d\n", ret);
		regmap_update_bits(info->pmic, info->charger_detect,
				   BIT_DP_DM_BC_ENB, 0);
		return ret;
	}

	info->otg_enable = true;
	schedule_delayed_work(&info->wdt_work,
			      msecs_to_jiffies(ETA6937_FEED_WATCHDOG_VALID_MS));
	schedule_delayed_work(&info->otg_work,
			      msecs_to_jiffies(ETA6937_OTG_VALID_MS));

	return 0;
}

static int eta6937_charger_disable_otg(struct regulator_dev *dev)
{
	struct eta6937_charger_info *info = rdev_get_drvdata(dev);
	int ret;

	if (!info) {
		pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
		return -EINVAL;
	}

	info->otg_enable = false;
	info->need_reinit = true;
	cancel_delayed_work_sync(&info->wdt_work);
	cancel_delayed_work_sync(&info->otg_work);
	ret = eta6937_update_bits(info, ETA6937_REG_1,
				   ETA6937_REG_HZ_MODE_MASK |
				   ETA6937_REG_OPA_MODE_MASK,
				   0);
	if (ret) {
		dev_err(info->dev, "disable eta6937 otg failed ret = %d\n", ret);
		return ret;
	}

	/* Enable charger detection function to identify the charger type */
	return regmap_update_bits(info->pmic, info->charger_detect,
				  BIT_DP_DM_BC_ENB, 0);
}

static int eta6937_charger_vbus_is_enabled(struct regulator_dev *dev)
{
	struct eta6937_charger_info *info = rdev_get_drvdata(dev);
	int ret;
	u8 val;

	if (!info) {
		pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
		return -EINVAL;
	}

	ret = eta6937_read(info, ETA6937_REG_1, &val);
	if (ret) {
		dev_err(info->dev, "failed to get eta6937 otg status\n");
		return ret;
	}

	val &= ETA6937_REG_OPA_MODE_MASK;

	return val;
}

static const struct regulator_ops eta6937_charger_vbus_ops = {
	.enable = eta6937_charger_enable_otg,
	.disable = eta6937_charger_disable_otg,
	.is_enabled = eta6937_charger_vbus_is_enabled,
};

static const struct regulator_desc eta6937_charger_vbus_desc = {
	.name = "otg-vbus",
	.of_match = "otg-vbus",
	.type = REGULATOR_VOLTAGE,
	.owner = THIS_MODULE,
	.ops = &eta6937_charger_vbus_ops,
	.fixed_uV = 5000000,
	.n_voltages = 1,
};

static void eta6937_charger_check_otg_status(struct eta6937_charger_info *info)
{
	int ret;
	u8 val;

	ret = eta6937_read(info, ETA6937_REG_1, &val);
	if (ret) {
		dev_err(info->dev, "%s:line%d, failed to get reg1(%d)\n", __func__, __LINE__, ret);
		return;
	}

	if (val & ETA6937_REG_OPA_MODE_MASK) {
		dev_info(info->dev, "%s:line%d, exit otg mode\n", __func__, __LINE__);
		ret = eta6937_update_bits(info, ETA6937_REG_1, ETA6937_REG_HZ_MODE_MASK |
					  ETA6937_REG_OPA_MODE_MASK, 0);
		if (ret)
			dev_err(info->dev, "disable eta6937 otg failed\n");
	}
}

static int eta6937_charger_register_vbus_regulator(struct eta6937_charger_info *info)
{
	struct regulator_config cfg = { };
	struct regulator_dev *reg;
	int ret = 0;

	eta6937_charger_check_otg_status(info);

	cfg.dev = info->dev;
	cfg.driver_data = info;
	reg = devm_regulator_register(info->dev,
				      &eta6937_charger_vbus_desc, &cfg);
	if (IS_ERR(reg)) {
		ret = PTR_ERR(reg);
		dev_err(info->dev, "Can't register regulator:%d\n", ret);
	}

	return ret;
}

static int eta6937_charger_register_external_vbus_regulator(struct eta6937_charger_info *info)
{
	struct regulator_config cfg = { };
	struct regulator_dev *reg;
	int ret = 0;
	struct device_node *otg_nd;
	struct device_node *otg_parent_nd;
	struct platform_device *otg_parent_nd_pdev;

	otg_nd = of_find_node_by_name(NULL, "otg-vbus");
	if (!otg_nd) {
		dev_warn(info->dev, "%s, unable to get otg node\n", __func__);
		return -EPROBE_DEFER;
	}

	otg_parent_nd = of_get_parent(otg_nd);
	of_node_put(otg_nd);
	if (!otg_parent_nd) {
		dev_warn(info->dev, "%s, unable to get otg parent node\n", __func__);
		return -EPROBE_DEFER;
	}

	otg_parent_nd_pdev = of_find_device_by_node(otg_parent_nd);
	of_node_put(otg_parent_nd);
	if (!otg_parent_nd_pdev) {
		dev_warn(info->dev, "%s, unable to get otg parent node device\n", __func__);
		return -EPROBE_DEFER;
	}

	cfg.dev = &otg_parent_nd_pdev->dev;
	platform_device_put(otg_parent_nd_pdev);
	cfg.driver_data = info;
	reg = devm_regulator_register(cfg.dev, &eta6937_charger_vbus_desc, &cfg);
	if (IS_ERR(reg)) {
		ret = PTR_ERR(reg);
		dev_warn(info->dev, "%s, failed to register vddvbus regulator:%d\n",
			 __func__, ret);
	}

	return ret;
}

#else
static int eta6937_charger_register_vbus_regulator(struct eta6937_charger_info *info)
{
	return 0;
}

static int eta6937_charger_register_external_vbus_regulator(struct bq2560x_charger_info *info)
{
	return 0;
}
#endif

static int eta6937_charger_detect_device(struct eta6937_charger_info *info)
{
	int ret, part_id;
	u8 reg_val;

	ret = eta6937_read(info, ETA6937_REG_3, &reg_val);
	if (ret < 0) {
		dev_err(info->dev, "%s, failed to get device id, ret = %d\n", __func__, ret);
		return ret;
	}

	part_id = (reg_val & ETA6937_REG_PN_CODE_MASK) >> ETA6937_REG_PN_CODE_SHIFT;
	if (part_id != ETA6937_DEV_ID) {
		dev_err(info->dev, "%s, the device id is 0x%x\n", __func__, part_id);
		return -EINVAL;
	}

	return ret;
}

static int eta6937_charger_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct i2c_adapter *adapter = to_i2c_adapter(client->dev.parent);
	struct device *dev = &client->dev;
	struct power_supply_config charger_cfg = { };
	struct eta6937_charger_info *info;
	struct device_node *regmap_np;
	struct platform_device *regmap_pdev;
	int ret;

	if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_BYTE_DATA)) {
		dev_err(dev, "No support for SMBUS_BYTE_DATA\n");
		return -ENODEV;
	}

	info = devm_kzalloc(dev, sizeof(*info), GFP_KERNEL);
	if (!info)
		return -ENOMEM;
	info->client = client;
	info->dev = dev;

	i2c_set_clientdata(client, info);
	ret = eta6937_charger_detect_device(info);
	if (ret) {
		dev_err(dev, "%s, failed to detect device, ret = %d\n", __func__, ret);
		return -ENODEV;
	}

	alarm_init(&info->wdg_timer, ALARM_BOOTTIME, NULL);

	mutex_init(&info->lock);

	info->edev = extcon_get_edev_by_phandle(info->dev, 0);
	if (IS_ERR(info->edev)) {
		dev_err(dev, "failed to find vbus extcon device.\n");
		return PTR_ERR(info->edev);
	}

	if (device_property_read_bool(dev, "otg-vbus-node-external"))
		ret = eta6937_charger_register_external_vbus_regulator(info);
	else
		ret = eta6937_charger_register_vbus_regulator(info);

	if (ret) {
		dev_err(dev, "failed to register vbus regulator.\n");
		return ret;
	}

	regmap_np = of_find_compatible_node(NULL, NULL, "sprd,sc27xx-syscon");
	if (!regmap_np) {
		dev_err(dev, "unable to get syscon node\n");
		return -ENODEV;
	}

	ret = of_property_read_u32_index(regmap_np, "reg", 1,
					 &info->charger_detect);
	if (ret) {
		dev_err(dev, "failed to get charger_detect\n");
		return -EINVAL;
	}

	info->gpiod = devm_gpiod_get(dev, "chg-enable", ENABLE_CHARGE);
	if (IS_ERR(info->gpiod)) {
		dev_err(dev, "failed to get enable gpio\n");
		return PTR_ERR(info->gpiod);
	}

	ret = of_property_read_u32_index(regmap_np, "reg", 2,
					 &info->charger_pd);
	if (ret) {
		dev_err(dev, "failed to get charger_pd reg\n");
		return ret;
	}

	if (of_device_is_compatible(regmap_np->parent, "sprd,sc2730"))
		info->charger_pd_mask = ETA6937_DISABLE_PIN_MASK_2730;
	else if (of_device_is_compatible(regmap_np->parent, "sprd,sc2721"))
		info->charger_pd_mask = ETA6937_DISABLE_PIN_MASK_2721;
	else if (of_device_is_compatible(regmap_np->parent, "sprd,sc2720"))
		info->charger_pd_mask = ETA6937_DISABLE_PIN_MASK_2720;
	else {
		dev_err(dev, "failed to get charger_pd mask\n");
		return -EINVAL;
	}

	regmap_pdev = of_find_device_by_node(regmap_np);
	if (!regmap_pdev) {
		of_node_put(regmap_np);
		dev_err(dev, "unable to get syscon device\n");
		return -ENODEV;
	}

	of_node_put(regmap_np);
	info->pmic = dev_get_regmap(regmap_pdev->dev.parent, NULL);
	if (!info->pmic) {
		dev_err(dev, "unable to get pmic regmap device\n");
		return -ENODEV;
	}

	charger_cfg.drv_data = info;
	charger_cfg.of_node = dev->of_node;
	info->psy_usb = devm_power_supply_register(dev,
						   &eta6937_charger_desc,
						   &charger_cfg);
	if (IS_ERR(info->psy_usb)) {
		dev_err(dev, "failed to register power supply\n");
		return PTR_ERR(info->psy_usb);
	}

	ret = eta6937_charger_hw_init(info);
	if (ret)
		return ret;

	eta6937_charger_stop_charge(info);

	device_init_wakeup(info->dev, true);

	INIT_DELAYED_WORK(&info->otg_work, eta6937_charger_otg_work);
	INIT_DELAYED_WORK(&info->wdt_work,
			  eta6937_charger_feed_watchdog_work);

	return 0;
}

static void eta6937_charger_shutdown(struct i2c_client *client)
{
	struct eta6937_charger_info *info = i2c_get_clientdata(client);
	int ret = 0;

	cancel_delayed_work_sync(&info->wdt_work);
	if (info->otg_enable) {
		info->otg_enable = false;
		info->need_reinit = true;
		cancel_delayed_work_sync(&info->otg_work);
		ret = eta6937_update_bits(info, ETA6937_REG_1,
					  ETA6937_REG_HZ_MODE_MASK |
					  ETA6937_REG_OPA_MODE_MASK,
					  0);
		if (ret)
			dev_err(info->dev, "disable eta6937 otg failed ret = %d\n", ret);

		/* Enable charger detection function to identify the charger type */
		ret = regmap_update_bits(info->pmic, info->charger_detect,
					 BIT_DP_DM_BC_ENB, 0);
		if (ret)
			dev_err(info->dev,
				"enable charger detection function failed ret = %d\n", ret);
	}
}

static int eta6937_charger_remove(struct i2c_client *client)
{
	struct eta6937_charger_info *info = i2c_get_clientdata(client);

	cancel_delayed_work_sync(&info->wdt_work);
	cancel_delayed_work_sync(&info->otg_work);

	mutex_destroy(&info->lock);

	return 0;
}

#if IS_ENABLED(CONFIG_PM_SLEEP)
static int eta6937_charger_alarm_prepare(struct device *dev)
{
	struct eta6937_charger_info *info = dev_get_drvdata(dev);
	ktime_t now, add;

	if (!info) {
		pr_err("%s: info is null!\n", __func__);
		return 0;
	}

	if (!info->otg_enable)
		return 0;

	now = ktime_get_boottime();
	add = ktime_set(ETA6937_WDG_TIMER_S, 0);
	alarm_start(&info->wdg_timer, ktime_add(now, add));
	pr_info("eta6937_charger set alarm, triggered at [%lld]ms\n",
		ktime_to_ms(ktime_add(now, add)));
	return 0;
}

static void eta6937_charger_alarm_complete(struct device *dev)
{
	struct eta6937_charger_info *info = dev_get_drvdata(dev);

	if (!info) {
		pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
		return;
	}

	if (!info->otg_enable)
		return;

	alarm_cancel(&info->wdg_timer);
}

static int eta6937_charger_suspend(struct device *dev)
{
	struct eta6937_charger_info *info = dev_get_drvdata(dev);

	if (info->otg_enable || info->is_charger_online)
		/* feed watchdog first before suspend */
		eta6937_charger_feed_watchdog(info);

	if (!info->otg_enable)
		return 0;

	cancel_delayed_work_sync(&info->wdt_work);

	return 0;
}

static int eta6937_charger_resume(struct device *dev)
{
	struct eta6937_charger_info *info = dev_get_drvdata(dev);

	if (!info) {
		pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
		return -EINVAL;
	}

	if (info->otg_enable || info->is_charger_online)
		/* feed watchdog first before suspend */
		eta6937_charger_feed_watchdog(info);

	if (!info->otg_enable)
		return 0;

	schedule_delayed_work(&info->wdt_work, HZ * 15);

	return 0;
}
#endif

static const struct dev_pm_ops eta6937_charger_pm_ops = {
	.prepare = eta6937_charger_alarm_prepare,
	.complete = eta6937_charger_alarm_complete,
	SET_SYSTEM_SLEEP_PM_OPS(eta6937_charger_suspend,
				eta6937_charger_resume)
};

static const struct i2c_device_id eta6937_i2c_id[] = {
	{"eta6937_chg", 0},
	{}
};

static const struct of_device_id eta6937_charger_of_match[] = {
	{ .compatible = "eta,eta6937_chg", },
	{ }
};

MODULE_DEVICE_TABLE(of, eta6937_charger_of_match);

static struct i2c_driver eta6937_charger_driver = {
	.driver = {
		.name = "eta6937_chg",
		.of_match_table = eta6937_charger_of_match,
		.pm = &eta6937_charger_pm_ops,
	},
	.probe = eta6937_charger_probe,
	.shutdown = eta6937_charger_shutdown,
	.remove = eta6937_charger_remove,
	.id_table = eta6937_i2c_id,
};

module_i2c_driver(eta6937_charger_driver);
MODULE_DESCRIPTION("ETA6937 Charger Driver");
MODULE_LICENSE("GPL v2");
