/*
 * TI BQ25910 charger driver
 *
 * Copyright (C) 2015 Intel Corporation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/acpi.h>
#include <linux/alarmtimer.h>
#include <linux/delay.h>
#include <linux/gpio/consumer.h>
#include <linux/i2c.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/pm_wakeup.h>
#include <linux/power/charger-manager.h>
#include <linux/power/sprd_battery_info.h>
#include <linux/power_supply.h>
#include <linux/regmap.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>
#include <linux/types.h>
#include <linux/usb/phy.h>
#include <uapi/linux/usb/charger.h>

#define BQ25910_REG_0			0x0
#define BQ25910_REG_1			0x1
#define BQ25910_REG_2			0x2
#define BQ25910_REG_3			0x3
#define BQ25910_REG_4			0x4
#define BQ25910_REG_5			0x5
#define BQ25910_REG_6			0x6
#define BQ25910_REG_7			0x7
#define BQ25910_REG_8			0x8
#define BQ25910_REG_9			0x9
#define BQ25910_REG_A			0xa
#define BQ25910_REG_B			0xb
#define BQ25910_REG_C			0xc
#define BQ25910_REG_D			0xd
#define BQ25910_REG_NUM			14

#define BQ25910_BATTERY_NAME		"sc27xx-fgu"

/* Reg0 set TERM voltage register*/
#define BQ25910_TERM_VOLTAGE_MIN	3500
#define BQ25910_TERM_VOLTAGE_MAX	4775
#define BQ25910_TERM_VOLTAGE_STEP	5
#define BQ25910_TERM_VOLTAGE_OFFSET	3500
#define BQ25910_TERM_VOLTAGE_MASK	GENMASK(7, 0)

/* Reg1 set limit current register */
#define BQ25910_ICHG_MIN		0
#define BQ25910_ICHG_MAX		6000000
#define BQ25910_ICHG_STEP		50000
#define BQ25910_ICHG_MASK		GENMASK(6, 0)

/* Reg2 set vindpm register */
#define BQ25910_VINDPM_MIN		3900
#define BQ25910_VINDPM_MAX		14000
#define BQ25910_VINDPM_STEP		100
#define BQ25910_VINDPM_OFFSET		3900
#define BQ25910_VINDPM_VOLTAGE_MASK	GENMASK(6, 0)

/* Reg3 set input limit current register */
#define BQ25910_LIMIT_CURRENT_MIN	500000
#define BQ25910_LIMIT_CURRENT_MAX	3600000
#define BQ25910_LIMIT_CURRENT_STEP	100000
#define BQ25910_LIMIT_CURRENT_OFFSET	500000
#define BQ25910_LIMIT_CURRENT_MASK	GENMASK(5, 0)

/* Reg5 set Termination control register */
#define BQ25910_EN_TERM_MASK		GENMASK(7, 7)
/* Reg5 set watchdog register */
#define BQ25910_WDG_RESET_MASK		GENMASK(6, 6)
#define BQ25910_WDG_TMR_DISABLE		0x0
#define BQ25910_WDG_TMR_40S		0x1
#define BQ25910_WDG_TMR_80S		0x2
#define BQ25910_WDG_TMR_160S		0x3
#define BQ25910_WDG_TMR_OFFSET		4
#define BQ25910_WDG_TMR_MASK		GENMASK(5, 4)
/* Reg5 set Charging safety-timer register */
#define BQ25910_SEC_TMR_EN_MASK		GENMASK(3, 3)
#define BQ25910_SEC_TMR_5h		0x0
#define BQ25910_SEC_TMR_8h		0x1
#define BQ25910_SEC_TMR_12h		0x2
#define BQ25910_SEC_TMR_20h		0x3
#define BQ25910_SEC_TMR_OFFSET		1
#define BQ25910_SEC_TMR_MASK		GENMASK(2, 1)
#define BQ25910_SEC_TMR2X_EN_MASK	GENMASK(0, 0)

/* Reg6 set thermal regulation threshold register */
#define BQ25910_THERMAL_60		0x0
#define BQ25910_THERMAL_80		0x1
#define BQ25910_THERMAL_100		0x2
#define BQ25910_THERMAL_120		0x3
#define BQ25910_THERMAL_OFFSET		4
#define BQ25910_THERMAL_MASK		GENMASK(5, 4)
/* Reg6 set charging enable register */
#define BQ25910_CHG_EN_MASK		GENMASK(3, 3)
/* Reg6 set vbat_lowv threshold register */
#define BQ25910_BATLOWV_26		0x0
#define BQ25910_BATLOWV_29		0x1
#define BQ25910_BATLOWV_32		0x2
#define BQ25910_BATLOWV_35		0x3
#define BQ25910_BATLOWV_MASK		GENMASK(1, 0)

/* Reg7 INT Status Register register */
#define BQ25910_PG_STAT_MASK		GENMASK(7, 7)
#define BQ25910_INDPM_STAT_MASK		GENMASK(6, 6)
#define BQ25910_VINDPM_STAT_MASK	GENMASK(5, 5)
#define BQ25910_TREG_STAT_MASK		GENMASK(4, 4)
#define BQ25910_WD_STAT_MASK		GENMASK(3, 3)
#define BQ25910_CHG_STAT_MASK		GENMASK(2, 0)

/* Reg8 FAULT Status register */
#define BQ25910_VBUS_OVP_STAT_MASK	GENMASK(7, 7)
#define BQ25910_TSHUT_STAT_MASK		GENMASK(6, 6)
#define BQ25910_BATOVP_STAT_MASK	GENMASK(5, 5)
#define BQ25910_CFLY_STAT_MASK		GENMASK(4, 4)
#define BQ25910_CAP_COND_STAT_MASK	GENMASK(2, 2)
#define BQ25910_POORSRC_STAT_MASK	GENMASK(1, 1)

/* Reg9 INT Flag Status register */
#define BQ25910_PG_FLAG_MASK		GENMASK(7, 7)
#define BQ25910_INDPM_FLAG_MASK		GENMASK(6, 6)
#define BQ25910_VINDPM_FLAG_MASK	GENMASK(5, 5)
#define BQ25910_TREG_FLAG_MASK		GENMASK(4, 4)
#define BQ25910_WD_FLAG_MASK		GENMASK(3, 3)
#define BQ25910_CHRG_TERM_FLAG_MASK	GENMASK(2, 2)
#define BQ25910_CHRG_FLAG_MASK		GENMASK(0, 0)

/* Reg0xA FAULT Flag register */
#define BQ25910_VBUS_OVP_FLAG_MASK	GENMASK(7, 7)
#define BQ25910_TSHUT_FLAG_MASK		GENMASK(6, 6)
#define BQ25910_BATOVP_FLAG_MASK	GENMASK(5, 5)
#define BQ25910_CFLY_FLAG_MASK		GENMASK(4, 4)
#define BQ25910_TMR_FLAG_MASK		GENMASK(3, 3)
#define BQ25910_CAP_COND_FLAG_MASK	GENMASK(2, 2)
#define BQ25910_POORSRC_FLAG_MASK	GENMASK(1, 1)

/* Reg0xB INT Mask register */
#define BQ25910_PG_MASK		        GENMASK(7, 7)
#define BQ25910_INDPM_MASK		GENMASK(6, 6)
#define BQ25910_VINDPM_MASK		GENMASK(5, 5)
#define BQ25910_TREG_MASK		GENMASK(4, 4)
#define BQ25910_WD_MASK			GENMASK(3, 3)
#define BQ25910_CHRG_TERM_MASK		GENMASK(2, 2)
#define BQ25910_CHRG_MASK		GENMASK(0, 0)

/* Reg0xC FAULT Mask register */
#define BQ25910_VBUS_OVP_MASK		GENMASK(7, 7)
#define BQ25910_TSHUT_MASK		GENMASK(6, 6)
#define BQ25910_BATOVP_MASK		GENMASK(5, 5)
#define BQ25910_CFLY_MASK		GENMASK(4, 4)
#define BQ25910_TMR_MASK		GENMASK(3, 3)
#define BQ25910_CAP_COND_MASK		GENMASK(2, 2)
#define BQ25910_POORSRC_MASK		GENMASK(1, 1)

/* Reg0xD reset register definition */
#define BQ25910_RESET_MASK		GENMASK(7, 7)

struct bq25910_charger_sysfs {
	char *name;
	struct attribute_group attr_g;
	struct device_attribute attr_bq25910_dump_reg;
	struct device_attribute attr_bq25910_lookup_reg;
	struct device_attribute attr_bq25910_sel_reg_id;
	struct device_attribute attr_bq25910_reg_val;
	struct attribute *attrs[5];
	struct bq25910_charger_info *info;
};

struct bq25910_charge_current {
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

struct bq25910_charger_info {
	bool charging;
	int reg_id;
	int vol_max_mv;
	u32 limit;
	u32 last_limit_current;
	u32 actual_limit_current;
	struct i2c_client *client;
	struct regmap *regmap;
	struct device *dev;
	struct usb_phy *usb_phy;
	struct notifier_block usb_notify;
	struct power_supply *psy_usb;
	struct bq25910_charge_current cur;
	struct mutex lock;
	struct delayed_work wdt_work;
	struct bq25910_charger_sysfs *sysfs;
};

struct bq25910_charger_reg_tab {
	int id;
	u32 addr;
	char *name;
};

static struct bq25910_charger_reg_tab reg_tab[BQ25910_REG_NUM + 1] = {
	{0, BQ25910_REG_0, "Setting Charge Limit Voltage reg"},
	{1, BQ25910_REG_1, "Setting Charge Limit Current reg"},
	{2, BQ25910_REG_2, "Setting Vindpm reg"},
	{3, BQ25910_REG_3, "IDPM Limit Setting reg"},
	{4, BQ25910_REG_4, "Reserved"},
	{5, BQ25910_REG_5, "Related Charger Control 1 reg"},
	{6, BQ25910_REG_6, "Related Charger Control 2 reg"},
	{7, BQ25910_REG_7, "INT status reg"},
	{8, BQ25910_REG_8, "Fault reg"},
	{9, BQ25910_REG_9, "INT Flag Status reg"},
	{10, BQ25910_REG_A, "FAULT Flag reg"},
	{11, BQ25910_REG_B, "INT Mask reg"},
	{12, BQ25910_REG_C, "FAULT Mask reg"},
	{13, BQ25910_REG_D, "Part Information reg"},
	{14, 0, "null"},
};

static const struct regmap_config bq25910_charger_regmap_config = {
	.reg_bits = 8,
	.val_bits = 8,
	.max_register = 0xd,
};

static void bq25910_charger_dump_register(struct bq25910_charger_info *info)
{
	int i, ret, len, idx = 0;
	u32 reg_val = 0;
	char buf[128];

	memset(buf, '\0', sizeof(buf));
	for (i = 0; i < BQ25910_REG_NUM; i++) {
		ret = regmap_read(info->regmap, reg_tab[i].addr, &reg_val);
		if (ret == 0) {
			len = snprintf(buf + idx, sizeof(buf) - idx,
				       "[REG_0x%.2x]=0x%.2x; ", reg_tab[i].addr,
				       reg_val);
			idx += len;
		}
	}

	dev_info(info->dev, "%s: %s", __func__, buf);
}

static int bq25910_charger_set_vindpm(struct bq25910_charger_info *info, u32 vol)
{
	u8 reg_val;

	if (vol < BQ25910_VINDPM_MIN)
		vol = BQ25910_VINDPM_MIN;
	else if (vol > BQ25910_VINDPM_MAX)
		vol = BQ25910_VINDPM_MAX;
	reg_val = (vol - BQ25910_VINDPM_OFFSET) / BQ25910_VINDPM_STEP;

	return regmap_update_bits(info->regmap, BQ25910_REG_2,
				  BQ25910_VINDPM_VOLTAGE_MASK, reg_val);
}

static int bq25910_charger_set_termina_vol(struct bq25910_charger_info *info, u32 vol)
{
	u8 reg_val;

	if (vol < BQ25910_TERM_VOLTAGE_MIN)
		vol = BQ25910_TERM_VOLTAGE_MIN;
	else if (vol >= BQ25910_TERM_VOLTAGE_MAX)
		vol = BQ25910_TERM_VOLTAGE_MAX;
	reg_val = (vol - BQ25910_TERM_VOLTAGE_OFFSET) /
		BQ25910_TERM_VOLTAGE_STEP;

	return regmap_update_bits(info->regmap, BQ25910_REG_0,
				  BQ25910_TERM_VOLTAGE_MASK, reg_val);
}

static int bq25910_charger_set_limit_current(struct bq25910_charger_info *info,
					     u32 limit_cur)
{
	u8 reg_val;
	int ret;

	if (limit_cur >= BQ25910_LIMIT_CURRENT_MAX)
		limit_cur = BQ25910_LIMIT_CURRENT_MAX;
	if (limit_cur <= BQ25910_LIMIT_CURRENT_MIN)
		limit_cur = BQ25910_LIMIT_CURRENT_MIN;

	info->last_limit_current = limit_cur;
	reg_val = (limit_cur - BQ25910_LIMIT_CURRENT_OFFSET) /
		BQ25910_LIMIT_CURRENT_STEP;

	ret = regmap_update_bits(info->regmap, BQ25910_REG_3,
				 BQ25910_LIMIT_CURRENT_MASK,
				 reg_val);
	if (ret)
		dev_err(info->dev, "set bq25910 limit cur failed\n");

	info->actual_limit_current = reg_val * BQ25910_LIMIT_CURRENT_STEP +
		 BQ25910_LIMIT_CURRENT_OFFSET;

	return ret;
}

static int bq25910_charger_hw_init(struct bq25910_charger_info *info)
{
	struct sprd_battery_info bat_info = {};
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
		info->vol_max_mv = 4440;
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

		info->vol_max_mv = bat_info.constant_charge_voltage_max_uv / 1000;
		sprd_battery_put_battery_info(info->psy_usb, &bat_info);
	}

	ret = regmap_update_bits(info->regmap, BQ25910_REG_D,
				 BQ25910_RESET_MASK,
				 BQ25910_RESET_MASK);
	if (ret) {
		dev_err(info->dev, "reset bq25910 failed\n");
		return ret;
	}

	ret = regmap_update_bits(info->regmap, BQ25910_REG_6,
				 BQ25910_BATLOWV_MASK,
				 BQ25910_BATLOWV_32);
	if (ret) {
		dev_err(info->dev, "set bq25910 batlowv failed\n");
		return ret;
	}

	ret = regmap_update_bits(info->regmap, BQ25910_REG_5,
				 BQ25910_EN_TERM_MASK,
				 BQ25910_EN_TERM_MASK);
	if (ret) {
		dev_err(info->dev, "set bq25910 batlowv failed\n");
		return ret;
	}

	ret = bq25910_charger_set_vindpm(info, info->vol_max_mv);
	if (ret) {
		dev_err(info->dev, "set bq25910 vindpm vol failed\n");
		return ret;
	}

	ret = bq25910_charger_set_termina_vol(info, info->vol_max_mv);
	if (ret) {
		dev_err(info->dev, "set bq25910 terminal vol failed\n");
		return ret;
	}

	ret = bq25910_charger_set_limit_current(info, info->cur.unknown_cur);
	if (ret)
		dev_err(info->dev, "set bq25910 limit current failed\n");

	return ret;
}

static int bq25910_charger_start_charge(struct bq25910_charger_info *info)
{
	int ret;

	ret = regmap_update_bits(info->regmap, BQ25910_REG_5,
				 BQ25910_WDG_TMR_MASK,
				 BQ25910_WDG_TMR_40S << BQ25910_WDG_TMR_OFFSET);
	if (ret) {
		dev_err(info->dev, "Failed to enable bq25910 watchdog\n");
		return ret;
	}

	ret = regmap_update_bits(info->regmap, BQ25910_REG_6,
				 BQ25910_CHG_EN_MASK,
				 BQ25910_CHG_EN_MASK);
	if (ret) {
		dev_err(info->dev, "enable bq25910 charge en failed\n");
		return ret;
	}

	ret = bq25910_charger_set_limit_current(info,
						info->last_limit_current);
	if (ret) {
		dev_err(info->dev, "failed to set limit current\n");
		return ret;
	}

	return ret;
}

static void bq25910_charger_stop_charge(struct bq25910_charger_info *info)
{
	int ret;

	ret = regmap_update_bits(info->regmap, BQ25910_REG_6, BQ25910_CHG_EN_MASK, 0);
	if (ret)
		dev_err(info->dev, "disable bq25910 charge en failed\n");

	ret = regmap_update_bits(info->regmap, BQ25910_REG_5,
				 BQ25910_WDG_TMR_MASK,
				 BQ25910_WDG_TMR_DISABLE);
	if (ret)
		dev_err(info->dev, "Failed to disable bq25910 watchdog\n");
}

static int bq25910_charger_set_current(struct bq25910_charger_info *info, u32 cur)
{
	u8 reg_val;
	int ret;

	if (cur <= BQ25910_ICHG_MIN)
		cur = BQ25910_ICHG_MIN;
	else if (cur >= BQ25910_ICHG_MAX)
		cur = BQ25910_ICHG_MAX;
	reg_val = cur / BQ25910_ICHG_STEP;

	ret = regmap_update_bits(info->regmap, BQ25910_REG_1, BQ25910_ICHG_MASK,
				 reg_val);

	return ret;

}

static int bq25910_charger_get_current(struct bq25910_charger_info *info,
				       u32 *cur)
{
	u32 reg_val = 0;
	int ret;

	ret = regmap_read(info->regmap, BQ25910_REG_1, &reg_val);
	if (ret < 0)
		return ret;

	reg_val &= BQ25910_ICHG_MASK;
	*cur = reg_val * BQ25910_ICHG_STEP;

	return 0;
}

static u32 bq25910_charger_get_limit_current(struct bq25910_charger_info *info,
					     u32 *limit_cur)
{
	u32 reg_val = 0;
	int ret;

	ret = regmap_read(info->regmap, BQ25910_REG_3, &reg_val);
	if (ret < 0)
		return ret;

	reg_val &= BQ25910_LIMIT_CURRENT_MASK;
	*limit_cur = reg_val * BQ25910_LIMIT_CURRENT_STEP +
		BQ25910_LIMIT_CURRENT_OFFSET;
	if (*limit_cur >= BQ25910_LIMIT_CURRENT_MAX)
		*limit_cur = BQ25910_LIMIT_CURRENT_MAX;

	return 0;
}

static inline int bq25910_charger_get_health(struct bq25910_charger_info *info, u32 *health)
{
	*health = POWER_SUPPLY_HEALTH_GOOD;

	return 0;
}

static inline int bq25910_charger_get_online(struct bq25910_charger_info *info, u32 *online)
{
	if (info->limit)
		*online = true;
	else
		*online = false;

	return 0;
}

static int bq25910_charger_feed_watchdog(struct bq25910_charger_info *info)
{
	int ret;
	u32 limit_cur = 0;

	ret = regmap_update_bits(info->regmap, BQ25910_REG_5,
				 BQ25910_WDG_RESET_MASK,
				 BQ25910_WDG_RESET_MASK);
	if (ret) {
		dev_err(info->dev, "reset bq25910 failed\n");
		return ret;
	}

	ret = bq25910_charger_get_limit_current(info, &limit_cur);
	if (ret) {
		dev_err(info->dev, "get limit cur failed\n");
		return ret;
	}

	if (info->actual_limit_current == limit_cur)
		return 0;

	ret = bq25910_charger_set_limit_current(info, info->actual_limit_current);
	if (ret) {
		dev_err(info->dev, "set limit cur failed\n");
		return ret;
	}

	return 0;
}

static void bq25910_charger_feed_watchdog_work(struct work_struct *work)
{
	struct delayed_work *dwork = to_delayed_work(work);
	struct bq25910_charger_info *info = container_of(dwork,
							 struct bq25910_charger_info,
							 wdt_work);
	int ret;

	ret = regmap_update_bits(info->regmap, BQ25910_REG_5,
				 BQ25910_WDG_RESET_MASK,
				 BQ25910_WDG_RESET_MASK);
	if (ret) {
		dev_err(info->dev, "reset bq25910 failed\n");
		return;
	}

	schedule_delayed_work(&info->wdt_work, HZ * 15);
}

static inline int bq25910_charger_get_status(struct bq25910_charger_info *info)
{
	if (info->charging)
		return POWER_SUPPLY_STATUS_CHARGING;
	else
		return POWER_SUPPLY_STATUS_NOT_CHARGING;
}

static int bq25910_charger_set_status(struct bq25910_charger_info *info,
				      int val)
{
	int ret = 0;

	if (val > CM_FAST_CHARGE_NORMAL_CMD)
		return 0;

	if (!val && info->charging) {
		bq25910_charger_stop_charge(info);
		info->charging = false;
	} else if (val && !info->charging) {
		ret = bq25910_charger_start_charge(info);
		if (ret)
			dev_err(info->dev, "start charge failed\n");
		else
			info->charging = true;
	}

	return ret;
}

static ssize_t bq25910_reg_val_show(struct device *dev,
				    struct device_attribute *attr,
				    char *buf)
{
	struct bq25910_charger_sysfs *bq25910_sysfs =
		container_of(attr, struct bq25910_charger_sysfs,
			     attr_bq25910_reg_val);
	struct bq25910_charger_info *info = bq25910_sysfs->info;
	u32 val = 0;
	int ret;

	if (!info)
		return snprintf(buf, PAGE_SIZE, "%s bq25910_sysfs->info is null\n", __func__);

	ret = regmap_read(info->regmap, reg_tab[info->reg_id].addr, &val);
	if (ret) {
		dev_err(info->dev, "fail to get bq25910_REG_0x%.2x value, ret = %d\n",
			reg_tab[info->reg_id].addr, ret);
		return snprintf(buf, PAGE_SIZE, "fail to get bq25910_REG_0x%.2x value\n",
			       reg_tab[info->reg_id].addr);
	}

	return snprintf(buf, PAGE_SIZE, "bq25910_REG_0x%.2x = 0x%.2x\n",
		       reg_tab[info->reg_id].addr, val);
}

static ssize_t bq25910_reg_val_store(struct device *dev,
				     struct device_attribute *attr,
				     const char *buf, size_t count)
{
	struct bq25910_charger_sysfs *bq25910_sysfs =
		container_of(attr, struct bq25910_charger_sysfs,
			     attr_bq25910_reg_val);
	struct bq25910_charger_info *info = bq25910_sysfs->info;
	u8 val;
	int ret;

	if (!info) {
		dev_err(dev, "%s bq25910_sysfs->info is null\n", __func__);
		return count;
	}

	ret =  kstrtou8(buf, 16, &val);
	if (ret) {
		dev_err(info->dev, "fail to get addr, ret = %d\n", ret);
		return count;
	}

	ret = regmap_write(info->regmap, reg_tab[info->reg_id].addr, val);
	if (ret) {
		dev_err(info->dev, "fail to wite 0x%.2x to REG_0x%.2x, ret = %d\n",
				val, reg_tab[info->reg_id].addr, ret);
		return count;
	}

	dev_info(info->dev, "wite 0x%.2x to REG_0x%.2x success\n", val,
		 reg_tab[info->reg_id].addr);
	return count;
}

static ssize_t bq25910_reg_id_store(struct device *dev,
				    struct device_attribute *attr,
				    const char *buf, size_t count)
{
	struct bq25910_charger_sysfs *bq25910_sysfs =
		container_of(attr, struct bq25910_charger_sysfs,
			     attr_bq25910_sel_reg_id);
	struct bq25910_charger_info *info = bq25910_sysfs->info;
	int ret, id;

	if (!info) {
		dev_err(dev, "%s bq25910_sysfs->info is null\n", __func__);
		return count;
	}

	ret =  kstrtoint(buf, 10, &id);
	if (ret) {
		dev_err(info->dev, "%s store register id fail\n", bq25910_sysfs->name);
		return count;
	}

	if (id < 0 || id >= BQ25910_REG_NUM) {
		dev_err(info->dev, "%s store register id fail, id = %d is out of range\n",
			bq25910_sysfs->name, id);
		return count;
	}

	info->reg_id = id;

	dev_info(info->dev, "%s store register id = %d success\n", bq25910_sysfs->name, id);
	return count;
}

static ssize_t bq25910_reg_id_show(struct device *dev,
				   struct device_attribute *attr,
				   char *buf)
{
	struct bq25910_charger_sysfs *bq25910_sysfs =
		container_of(attr, struct bq25910_charger_sysfs,
			     attr_bq25910_sel_reg_id);
	struct bq25910_charger_info *info = bq25910_sysfs->info;

	if (!info)
		return snprintf(buf, PAGE_SIZE, "%s bq25910_sysfs->info is null\n", __func__);

	return snprintf(buf, PAGE_SIZE, "Cuurent register id = %d\n", info->reg_id);
}

static ssize_t bq25910_reg_table_show(struct device *dev,
				      struct device_attribute *attr,
				      char *buf)
{
	struct bq25910_charger_sysfs *bq25910_sysfs =
		container_of(attr, struct bq25910_charger_sysfs,
			     attr_bq25910_lookup_reg);
	struct bq25910_charger_info *info = bq25910_sysfs->info;
	int i, len, idx = 0;
	char reg_tab_buf[2048];

	if (!info)
		return snprintf(buf, PAGE_SIZE, "%s bq25910_sysfs->info is null\n", __func__);

	memset(reg_tab_buf, '\0', sizeof(reg_tab_buf));
	len = snprintf(reg_tab_buf + idx, sizeof(reg_tab_buf) - idx,
		       "Format: [id] [addr] [desc]\n");
	idx += len;

	for (i = 0; i < BQ25910_REG_NUM; i++) {
		len = snprintf(reg_tab_buf + idx, sizeof(reg_tab_buf) - idx,
			       "[%d] [REG_0x%.2x] [%s];\n", reg_tab[i].id,
			       reg_tab[i].addr, reg_tab[i].name);
		idx += len;
	}

	return snprintf(buf, PAGE_SIZE, "%s\n", reg_tab_buf);
}

static ssize_t bq25910_dump_reg_show(struct device *dev,
				     struct device_attribute *attr,
				     char *buf)
{
	struct bq25910_charger_sysfs *bq25910_sysfs =
		container_of(attr, struct bq25910_charger_sysfs,
			     attr_bq25910_dump_reg);
	struct bq25910_charger_info *info = bq25910_sysfs->info;

	if (!info)
		return snprintf(buf, PAGE_SIZE, "%s bq25910_sysfs->info is null\n", __func__);

	bq25910_charger_dump_register(info);

	return snprintf(buf, PAGE_SIZE, "%s\n", bq25910_sysfs->name);
}

static int bq25910_register_sysfs(struct bq25910_charger_info *info)
{
	struct bq25910_charger_sysfs *bq25910_sysfs;
	int ret;

	bq25910_sysfs = devm_kzalloc(info->dev, sizeof(*bq25910_sysfs), GFP_KERNEL);
	if (!bq25910_sysfs)
		return -ENOMEM;

	info->sysfs = bq25910_sysfs;
	bq25910_sysfs->name = "bq25910_sysfs";
	bq25910_sysfs->info = info;
	bq25910_sysfs->attrs[0] = &bq25910_sysfs->attr_bq25910_dump_reg.attr;
	bq25910_sysfs->attrs[1] = &bq25910_sysfs->attr_bq25910_lookup_reg.attr;
	bq25910_sysfs->attrs[2] = &bq25910_sysfs->attr_bq25910_sel_reg_id.attr;
	bq25910_sysfs->attrs[3] = &bq25910_sysfs->attr_bq25910_reg_val.attr;
	bq25910_sysfs->attrs[4] = NULL;
	bq25910_sysfs->attr_g.name = "debug";
	bq25910_sysfs->attr_g.attrs = bq25910_sysfs->attrs;

	sysfs_attr_init(&bq25910_sysfs->attr_bq25910_dump_reg.attr);
	bq25910_sysfs->attr_bq25910_dump_reg.attr.name = "bq25910_dump_reg";
	bq25910_sysfs->attr_bq25910_dump_reg.attr.mode = 0444;
	bq25910_sysfs->attr_bq25910_dump_reg.show = bq25910_dump_reg_show;

	sysfs_attr_init(&bq25910_sysfs->attr_bq25910_lookup_reg.attr);
	bq25910_sysfs->attr_bq25910_lookup_reg.attr.name = "bq25910_lookup_reg";
	bq25910_sysfs->attr_bq25910_lookup_reg.attr.mode = 0444;
	bq25910_sysfs->attr_bq25910_lookup_reg.show = bq25910_reg_table_show;

	sysfs_attr_init(&bq25910_sysfs->attr_bq25910_sel_reg_id.attr);
	bq25910_sysfs->attr_bq25910_sel_reg_id.attr.name = "bq25910_sel_reg_id";
	bq25910_sysfs->attr_bq25910_sel_reg_id.attr.mode = 0644;
	bq25910_sysfs->attr_bq25910_sel_reg_id.show = bq25910_reg_id_show;
	bq25910_sysfs->attr_bq25910_sel_reg_id.store = bq25910_reg_id_store;

	sysfs_attr_init(&bq25910_sysfs->attr_bq25910_reg_val.attr);
	bq25910_sysfs->attr_bq25910_reg_val.attr.name = "bq25910_reg_val";
	bq25910_sysfs->attr_bq25910_reg_val.attr.mode = 0644;
	bq25910_sysfs->attr_bq25910_reg_val.show = bq25910_reg_val_show;
	bq25910_sysfs->attr_bq25910_reg_val.store = bq25910_reg_val_store;

	ret = sysfs_create_group(&info->psy_usb->dev.kobj, &bq25910_sysfs->attr_g);
	if (ret < 0)
		dev_err(info->dev, "Cannot create sysfs , ret = %d\n", ret);

	return ret;
}

static int bq25910_charger_usb_change(struct notifier_block *nb,
				      unsigned long limit, void *data)
{
	struct bq25910_charger_info *info =
		container_of(nb, struct bq25910_charger_info, usb_notify);

	info->limit = limit;

	return NOTIFY_OK;
}

static int bq25910_charger_usb_get_property(struct power_supply *psy,
					    enum power_supply_property psp,
					    union power_supply_propval *val)
{
	struct bq25910_charger_info *info = power_supply_get_drvdata(psy);
	u32 cur, online, health, enabled = 0;
	enum usb_charger_type type;
	int ret = 0;

	if (!info)
		return -EINVAL;

	mutex_lock(&info->lock);

	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
		if (info->limit)
			val->intval = bq25910_charger_get_status(info);
		else
			val->intval = POWER_SUPPLY_STATUS_DISCHARGING;
		break;

	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT:
		if (!info->charging) {
			val->intval = 0;
		} else {
			ret = bq25910_charger_get_current(info, &cur);
			if (ret)
				goto out;

			val->intval = cur;
		}
		break;

	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
		if (!info->charging) {
			val->intval = 0;
		} else {
			ret = bq25910_charger_get_limit_current(info, &cur);
			if (ret)
				goto out;

			val->intval = cur;
		}
		break;

	case POWER_SUPPLY_PROP_ONLINE:
		ret = bq25910_charger_get_online(info, &online);
		if (ret)
			goto out;

		val->intval = online;

		break;

	case POWER_SUPPLY_PROP_HEALTH:
		if (info->charging) {
			val->intval = 0;
		} else {
			ret = bq25910_charger_get_health(info, &health);
			if (ret)
				goto out;

			val->intval = health;
		}
		break;

	case POWER_SUPPLY_PROP_USB_TYPE:
		type = info->usb_phy->chg_type;

		switch (type) {
		case SDP_TYPE:
			val->intval = POWER_SUPPLY_USB_TYPE_SDP;
			break;

		case DCP_TYPE:
			val->intval = POWER_SUPPLY_USB_TYPE_DCP;
			break;

		case CDP_TYPE:
			val->intval = POWER_SUPPLY_USB_TYPE_CDP;
			break;

		default:
			val->intval = POWER_SUPPLY_USB_TYPE_UNKNOWN;
		}

		break;

	case POWER_SUPPLY_PROP_CALIBRATE:
		ret = regmap_read(info->regmap, BQ25910_REG_7, &enabled);
		if (ret) {
			dev_err(info->dev, "get bq25910 charge status failed\n");
			goto out;
		}

		enabled &= BQ25910_CHG_STAT_MASK;
		val->intval = enabled;

		break;

	default:
		ret = -EINVAL;
	}

out:
	mutex_unlock(&info->lock);
	return ret;
}

static int bq25910_charger_usb_set_property(struct power_supply *psy,
					    enum power_supply_property psp,
					    const union power_supply_propval *val)
{
	struct bq25910_charger_info *info = power_supply_get_drvdata(psy);
	int ret = 0;

	if (!info)
		return -EINVAL;

	mutex_lock(&info->lock);

	switch (psp) {
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT:
		ret = bq25910_charger_set_current(info, val->intval);
		if (ret < 0)
			dev_err(info->dev, "set charge current failed\n");
		break;
	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
		ret = bq25910_charger_set_limit_current(info, val->intval);
		if (ret < 0)
			dev_err(info->dev, "set input current limit failed\n");
		break;

	case POWER_SUPPLY_PROP_STATUS:
		ret = bq25910_charger_set_status(info, val->intval);
		if (ret < 0)
			dev_err(info->dev, "set charge status failed\n");

		if (info->limit)
			schedule_delayed_work(&info->wdt_work, 0);
		else
			cancel_delayed_work(&info->wdt_work);
		break;

	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE_MAX:
		ret = bq25910_charger_set_termina_vol(info, val->intval / 1000);
		if (ret < 0)
			dev_err(info->dev, "failed to set terminate voltage\n");
		break;

	case POWER_SUPPLY_PROP_CALIBRATE:
		if (val->intval == true) {
			ret = bq25910_charger_start_charge(info);
			if (ret)
				dev_err(info->dev, "start charge failed\n");
		} else if (val->intval == false) {
			bq25910_charger_stop_charge(info);
		}
		break;
	default:
		ret = -EINVAL;
	}

	mutex_unlock(&info->lock);
	return ret;
}

static int bq25910_charger_property_is_writeable(struct power_supply *psy,
						 enum power_supply_property psp)
{
	int ret;

	switch (psp) {
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT:
	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
	case POWER_SUPPLY_PROP_STATUS:
	case POWER_SUPPLY_PROP_CALIBRATE:
		ret = 1;
		break;

	default:
		ret = 0;
	}

	return ret;
}

static enum power_supply_usb_type bq25910_charger_usb_types[] = {
	POWER_SUPPLY_USB_TYPE_UNKNOWN,
	POWER_SUPPLY_USB_TYPE_SDP,
	POWER_SUPPLY_USB_TYPE_DCP,
	POWER_SUPPLY_USB_TYPE_CDP,
	POWER_SUPPLY_USB_TYPE_C,
	POWER_SUPPLY_USB_TYPE_PD,
	POWER_SUPPLY_USB_TYPE_PD_DRP,
	POWER_SUPPLY_USB_TYPE_APPLE_BRICK_ID
};

static enum power_supply_property bq25910_usb_props[] = {
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT,
	POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT,
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_HEALTH,
	POWER_SUPPLY_PROP_USB_TYPE,
	POWER_SUPPLY_PROP_CALIBRATE,
};

static const struct power_supply_desc bq25910_charger_desc = {
	.name			= "bq25910_charger",
	.type			= POWER_SUPPLY_TYPE_UNKNOWN,
	.properties		= bq25910_usb_props,
	.num_properties		= ARRAY_SIZE(bq25910_usb_props),
	.get_property		= bq25910_charger_usb_get_property,
	.set_property		= bq25910_charger_usb_set_property,
	.property_is_writeable	= bq25910_charger_property_is_writeable,
	.usb_types		= bq25910_charger_usb_types,
	.num_usb_types		= ARRAY_SIZE(bq25910_charger_usb_types),
};

static void bq25910_charger_detect_status(struct bq25910_charger_info *info)
{
	unsigned int min, max;

	/*
	 * If the USB charger status has been USB_CHARGER_PRESENT before
	 * registering the notifier, we should start to charge with getting
	 * the charge current.
	 */
	if (info->usb_phy->chg_state != USB_CHARGER_PRESENT)
		return;

	usb_phy_get_charger_current(info->usb_phy, &min, &max);
	info->limit = min;
}

static int bq25910_charger_probe(struct i2c_client *client,
				 const struct i2c_device_id *id)
{
	struct device *dev = &client->dev;
	struct power_supply_config charger_cfg = { };
	struct bq25910_charger_info *info;
	int ret;

	info = devm_kzalloc(dev, sizeof(*info), GFP_KERNEL);
	if (!info)
		return -ENOMEM;

	info->regmap = regmap_init_i2c(client, &bq25910_charger_regmap_config);
	if (IS_ERR(info->regmap)) {
		dev_err(&client->dev, "i2c to regmap fail\n");
		return PTR_ERR(info->regmap);
	}

	info->client = client;
	info->dev = dev;

	mutex_init(&info->lock);
	i2c_set_clientdata(client, info);

	info->usb_phy = devm_usb_get_phy_by_phandle(dev, "phys", 0);
	if (IS_ERR(info->usb_phy)) {
		dev_err(dev, "failed to find USB phy\n");
		return PTR_ERR(info->usb_phy);
	}

	charger_cfg.drv_data = info;
	charger_cfg.of_node = dev->of_node;
	info->psy_usb = devm_power_supply_register(dev,
						   &bq25910_charger_desc,
						   &charger_cfg);

	if (IS_ERR(info->psy_usb)) {
		dev_err(dev, "failed to register power supply\n");
		ret = PTR_ERR(info->psy_usb);
		goto err_mutex_lock;
	}

	ret = bq25910_charger_hw_init(info);
	if (ret) {
		dev_err(dev, "failed to bq25910_charger_hw_init\n");
		goto err_mutex_lock;
	}

	device_init_wakeup(info->dev, true);
	info->usb_notify.notifier_call = bq25910_charger_usb_change;
	ret = usb_register_notifier(info->usb_phy, &info->usb_notify);
	if (ret) {
		dev_err(dev, "failed to register notifier:%d\n", ret);
		goto err_psy_usb;
	}

	ret = bq25910_register_sysfs(info);
	if (ret) {
		dev_err(info->dev, "register sysfs fail, ret = %d\n", ret);
		goto err_sysfs;
	}

	bq25910_charger_detect_status(info);

	INIT_DELAYED_WORK(&info->wdt_work,
			  bq25910_charger_feed_watchdog_work);
	return 0;

err_sysfs:
	sysfs_remove_group(&info->psy_usb->dev.kobj, &info->sysfs->attr_g);
	usb_unregister_notifier(info->usb_phy, &info->usb_notify);
err_psy_usb:
	power_supply_unregister(info->psy_usb);
err_mutex_lock:
	mutex_destroy(&info->lock);

	return ret;
}

static int bq25910_charger_remove(struct i2c_client *client)
{
	struct bq25910_charger_info *info = i2c_get_clientdata(client);

	cancel_delayed_work_sync(&info->wdt_work);
	usb_unregister_notifier(info->usb_phy, &info->usb_notify);

	mutex_destroy(&info->lock);

	return 0;
}

#if IS_ENABLED(CONFIG_PM_SLEEP)
static int bq25910_charger_suspend(struct device *dev)
{
	struct bq25910_charger_info *info = dev_get_drvdata(dev);

	if (info->limit)
		bq25910_charger_feed_watchdog(info);

	return 0;
}

static int bq25910_charger_resume(struct device *dev)
{
	struct bq25910_charger_info *info = dev_get_drvdata(dev);

	if (info->limit)
		bq25910_charger_feed_watchdog(info);

	return 0;
}
#endif

static const struct dev_pm_ops bq25910_charger_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(bq25910_charger_suspend,
				bq25910_charger_resume)
};

static const struct i2c_device_id bq25910_i2c_id[] = {
	{"bq25910_chg", 0},
	{}
};

static const struct of_device_id bq25910_charger_of_match[] = {
	{ .compatible = "ti,bq25910_chg", },
	{ }
};

MODULE_DEVICE_TABLE(of, bq25910_charger_of_match);

static struct i2c_driver bq25910_charger_driver = {
	.driver = {
		.name = "bq25910_chg",
		.of_match_table = bq25910_charger_of_match,
		.pm = &bq25910_charger_pm_ops,
	},
	.probe = bq25910_charger_probe,
	.remove = bq25910_charger_remove,
	.id_table = bq25910_i2c_id,
};

module_i2c_driver(bq25910_charger_driver);

MODULE_AUTHOR("Yongzhi Chen<Yongzhi.Chen@unisoc.com>");
MODULE_DESCRIPTION("BQ25910 Charger Driver");
MODULE_LICENSE("GPL");


