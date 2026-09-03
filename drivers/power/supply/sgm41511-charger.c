// SPDX-License-Identifier: GPL-2.0:
// Copyright (c) 2021 unisoc.

/*
 * Driver for the Sgm sgm41511 charger.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/alarmtimer.h>
#include <linux/gpio/consumer.h>
#include <linux/interrupt.h>
#include <linux/i2c.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/pm_wakeup.h>
#include <linux/power_supply.h>
#include <linux/power/charger-manager.h>
#include <linux/power/sprd_battery_info.h>
#include <linux/regmap.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>
#include <linux/slab.h>

#include <linux/power/sgm41511_reg.h>

#define SGM41511_BATTERY_NAME			"sc27xx-fgu"
#define BIT_DP_DM_BC_ENB			BIT(0)
#define SGM41511_OTG_VALID_MS			500
#define SGM41511_FEED_WATCHDOG_VALID_MS		50
#define SGM41511_OTG_ALARM_TIMER_S		15

#define SGM41511_REG_HZ_MODE_MASK		GENMASK(1, 1)
#define SGM41511_REG_OPA_MODE_MASK		GENMASK(0, 0)
#define SGM41511_REG_OTG_MASK			GENMASK(5, 5)
#define SGM41511_REG_RESET_MASK			GENMASK(6, 6)
#define SGM41511_REG_BOOST_FAULT_MASK		GENMASK(6, 6)

#define SGM41511_REG_TERMINAL_CUR_MASK		GENMASK(3, 0)

#define SGM41511_REG_WATCHDOG_TIMER_MASK	GENMASK(5, 4)
#define SGM41511_REG_WATCHDOG_TIMER_SHIFT	4

#define SGM41511_REG_EN_HIZ_MASK		GENMASK(7, 7)
#define SGM41511_REG_EN_HIZ_SHIFT		7

#define SGM41511_DISABLE_PIN_MASK		BIT(0)
#define SGM41511_DISABLE_PIN_MASK_2730		BIT(0)
#define SGM41511_DISABLE_PIN_MASK_2721		BIT(15)
#define SGM41511_DISABLE_PIN_MASK_2720		BIT(0)

#define VENDOR_SGM41511				(0x1)

#define SGM41511_OTG_RETRY_TIMES		10

#define SGM41511_ROLE_MASTER			1
#define SGM41511_ROLE_SLAVE			2

#define SGM41511_FCHG_OVP_6V			6000
#define SGM41511_FCHG_OVP_9V			9000
#define SGM41511_FCHG_OVP_14V			14000
#define SGM41511_FAST_CHARGER_VOLTAGE_MAX	10500000
#define SGM41511_NORMAL_CHARGER_VOLTAGE_MAX	6500000
#define SGM41511_VINPDM_VOLTAGE_MAX		5400
#define SGM41511_TERMINA_VOLTAGE_MAX		4624
#define SGM41511_LIMIT_CURRENT_MAX		3200000
#define SGM41511_ICHG_CURRENT_MAX		3000

#define SGM41511_WAKE_UP_MS			1000
#define SGM41511_PROBE_TIMEOUT			msecs_to_jiffies(500)

#define SGM41511_WATCH_DOG_TIME_OUT_MS		20000
#define SGM41511_SHUTDOWN_LIMIT			600000
#define SGM41511_SHUTDOWN_CURRENT		500000

static bool boot_calibration;

struct sgm41511_charge_current {
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

struct sgm41511_charger_info {
	struct i2c_client *client;
	struct device *dev;
	struct power_supply *psy_usb;
	struct sgm41511_charge_current cur;
	struct work_struct work;
	struct mutex lock;
	struct mutex input_limit_cur_lock;
	bool charging;
	struct delayed_work otg_work;
	struct delayed_work wdt_work;
	struct regmap *pmic;
	struct completion probe_init;
	u32 charger_detect;
	u32 charger_pd;
	u32 charger_pd_mask;
	struct gpio_desc *gpiod;
	struct extcon_dev *typec_extcon;
	struct alarm otg_timer;
	u32 last_limit_current;
	u32 actual_limit_cur;
	u32 role;
	u64 last_wdt_time;
	bool need_disable_Q1;
	bool disable_wdg;
	bool otg_enable;
	bool is_charger_online;
	bool disable_power_path;
	bool probe_initialized;
	bool use_typec_extcon;
	bool shutdown_flag;
	int voltage_max_microvolt;
};

static bool enable_dump_stack;
module_param(enable_dump_stack, bool, 0644);

static int sgm41511_charger_set_vindpm(struct sgm41511_charger_info *info, u32 vol);

static void sgm41511_charger_dump_stack(void)
{
	if (enable_dump_stack)
		dump_stack();
}

static void power_path_control(struct sgm41511_charger_info *info)
{
	struct device_node *cmdline_node;
	const char *cmd_line;
	char *match;
	char result[5] = {0};
	int ret;

	cmdline_node = of_find_node_by_path("/chosen");
	ret = of_property_read_string(cmdline_node, "bootargs", &cmd_line);
	if (ret) {
		info->disable_power_path = false;
		return;
	}

	if (strncmp(cmd_line, "charger", strlen("charger")) == 0)
		info->disable_power_path = true;

	match = strstr(cmd_line, "sprdboot.mode=");
	if (match) {
		memcpy(result, (match + strlen("sprdboot.mode=")), sizeof(result) - 1);
		if ((!strcmp(result, "cali")) || (!strcmp(result, "auto")))
			info->disable_power_path = true;

		if (!strcmp(result, "cali"))
			boot_calibration = true;
	}

	dev_info(info->dev, "disable_power_path=%d\n", info->disable_power_path);
}

static bool sgm41511_charger_is_bat_present(struct sgm41511_charger_info *info)
{
	struct power_supply *psy;
	union power_supply_propval val;
	bool present = false;
	int ret;

	psy = power_supply_get_by_name(SGM41511_BATTERY_NAME);
	if (!psy) {
		dev_err(info->dev, "Failed to get psy of sc27xx_fgu\n");
		return present;
	}

	val.intval = 0;
	ret = power_supply_get_property(psy, POWER_SUPPLY_PROP_PRESENT,
					&val);
	if (ret == 0 && val.intval)
		present = true;
	power_supply_put(psy);

	if (ret)
		dev_err(info->dev,
			"Failed to get property of present:%d\n", ret);

	return present;
}

static int sgm41511_charger_is_fgu_present(struct sgm41511_charger_info *info)
{
	struct power_supply *psy;

	psy = power_supply_get_by_name(SGM41511_BATTERY_NAME);
	if (!psy) {
		dev_err(info->dev, "Failed to find psy of sc27xx_fgu\n");
		return -ENODEV;
	}
	power_supply_put(psy);

	return 0;
}

static int sgm41511_read(struct sgm41511_charger_info *info, u8 reg, u8 *data)
{
	int ret;

	ret = i2c_smbus_read_byte_data(info->client, reg);
	if (ret < 0)
		return ret;

	*data = ret;
	return 0;
}

static int sgm41511_write(struct sgm41511_charger_info *info, u8 reg, u8 data)
{
	return i2c_smbus_write_byte_data(info->client, reg, data);
}

static int sgm41511_update_bits(struct sgm41511_charger_info *info, u8 reg, u8 mask, u8 data)
{
	u8 v;
	int ret;

	ret = sgm41511_read(info, reg, &v);
	if (ret < 0)
		return ret;

	v &= ~mask;
	v |= (data & mask);

	return sgm41511_write(info, reg, v);
}

static u32 sgm41511_charger_get_limit_current(struct sgm41511_charger_info *info, u32 *limit_cur)
{
	u8 reg_val;
	int ret;

	ret = sgm41511_read(info, SGM4151X_REG_00, &reg_val);
	if (ret < 0)
		return ret;

	reg_val &= REG00_IINLIM_MASK;
	reg_val = reg_val >> REG00_IINLIM_SHIFT;
	*limit_cur = (reg_val * REG00_IINLIM_LSB + REG00_IINLIM_BASE) * 1000;
	return 0;
}

static int sgm41511_charger_set_limit_current(struct sgm41511_charger_info *info,
					      u32 limit_cur, bool enable)
{
	u8 reg_val;
	int ret = 0;

	mutex_lock(&info->input_limit_cur_lock);
	if (enable) {
		ret = sgm41511_charger_get_limit_current(info, &limit_cur);
		if (ret) {
			dev_err(info->dev, "get limit cur failed\n");
			goto out;
		}

		if (limit_cur == info->actual_limit_cur)
			goto out;
		limit_cur = info->actual_limit_cur;
		dev_info(info->dev, "set limit current limit_cur = %d\n", limit_cur);
	}

	if (limit_cur >= SGM41511_LIMIT_CURRENT_MAX)
		limit_cur = SGM41511_LIMIT_CURRENT_MAX;

	limit_cur = limit_cur / 1000;
	if (limit_cur < REG00_IINLIM_BASE)
		limit_cur = REG00_IINLIM_BASE;
	reg_val = (limit_cur - REG00_IINLIM_BASE) / REG00_IINLIM_LSB;
	info->actual_limit_cur = ((reg_val * REG00_IINLIM_LSB) + REG00_IINLIM_BASE) * 1000;
	ret = sgm41511_update_bits(info, SGM4151X_REG_00, REG00_IINLIM_MASK,
				   reg_val << REG00_IINLIM_SHIFT);
	if (ret)
		dev_err(info->dev, "set sgm41511 limit cur failed\n");

	if (limit_cur <= REG00_IINLIM_BASE) {
		ret = sgm41511_charger_set_vindpm(info, 5400);
		if (ret)
			dev_err(info->dev, "set sgm41511 vindpm vol 5400 failed\n");
	} else {
		ret = sgm41511_charger_set_vindpm(info, info->voltage_max_microvolt);
		if (ret)
			dev_err(info->dev, "set sgm41511 vindpm vol defalut failed\n");
	}

	dev_info(info->dev, "set limit_cur = %d, reg_val = %#x, actual_limit_cur = %d\n",
		 limit_cur, reg_val, info->actual_limit_cur);

out:
	mutex_unlock(&info->input_limit_cur_lock);

	return ret;
}

static int sgm41511_set_acovp_threshold(struct sgm41511_charger_info *info, int volt)
{
	u8 reg_val;

	if (volt <= 5500)
		reg_val = 0x0;
	else if (volt <= 6500)
		reg_val = 0x01;
	else if (volt <= 10500)
		reg_val = 0x02;
	else
		reg_val = 0x03;

	return sgm41511_update_bits(info, SGM4151X_REG_06, REG06_OVP_MASK,
				    reg_val << REG06_OVP_SHIFT);
}

static int sgm41511_enable_charger(struct sgm41511_charger_info *info, bool enable)
{
	u8 val = REG01_CHG_DISABLE;

	if (enable)
		val = REG01_CHG_ENABLE;

	return sgm41511_update_bits(info, SGM4151X_REG_01, REG01_CHG_CONFIG_MASK,
				    val << REG01_CHG_CONFIG_SHIFT);
}

static int sgm41511_enter_hiz_mode(struct sgm41511_charger_info *info)
{
	u8 val = REG00_HIZ_ENABLE << REG00_ENHIZ_SHIFT;

	return sgm41511_update_bits(info, SGM4151X_REG_00, REG00_ENHIZ_MASK, val);
}

static int sgm41511_exit_hiz_mode(struct sgm41511_charger_info *info)
{
	u8 val = REG00_HIZ_DISABLE << REG00_ENHIZ_SHIFT;

	return sgm41511_update_bits(info, SGM4151X_REG_00, REG00_ENHIZ_MASK, val);
}

static int sgm41511_enable_term(struct sgm41511_charger_info *info, bool enable)
{
	u8 val;
	int ret;

	if (enable)
		val = REG05_TERM_ENABLE << REG05_EN_TERM_SHIFT;
	else
		val = REG05_TERM_DISABLE << REG05_EN_TERM_SHIFT;

	ret = sgm41511_update_bits(info, SGM4151X_REG_05, REG05_EN_TERM_MASK, val);

	return ret;
}

static int sgm41511_charger_set_vindpm(struct sgm41511_charger_info *info, u32 vol)
{
	u8 reg_val;

	if (vol < REG06_VINDPM_BASE)
		reg_val = 0x0;
	else if (vol > SGM41511_VINPDM_VOLTAGE_MAX)
		reg_val = 0x0f;
	else
		reg_val = (vol - REG06_VINDPM_BASE) / REG06_VINDPM_LSB;

	return sgm41511_update_bits(info, SGM4151X_REG_06, REG06_VINDPM_MASK,
				    reg_val << REG06_VINDPM_SHIFT);
}

static int sgm41511_charger_set_termina_vol(struct sgm41511_charger_info *info, u32 vol)
{
	u8 reg_val;

	dev_dbg(info->dev, "%s:line%d: set termina vol = %d\n", __func__, __LINE__, vol);

	if (vol < REG04_VREG_BASE)
		reg_val = 0x0;
	else if (vol >= SGM41511_TERMINA_VOLTAGE_MAX)
		reg_val = 0x18;
	else
		reg_val = (vol - REG04_VREG_BASE) / REG04_VREG_LSB;

	return sgm41511_update_bits(info, SGM4151X_REG_04, REG04_VREG_MASK,
				    reg_val << REG04_VREG_SHIFT);
}

static int sgm41511_charger_set_safety_cur(struct sgm41511_charger_info *info, u32 cur)
{
	u8 reg_val;

	dev_dbg(info->dev, "%s:line%d: set safety cur = %d\n", __func__, __LINE__, cur);

	if (cur >= SGM41511_LIMIT_CURRENT_MAX)
		cur = SGM41511_LIMIT_CURRENT_MAX;

	cur = cur / 1000;
	if (cur < REG00_IINLIM_BASE)
		cur = REG00_IINLIM_BASE;

	reg_val = (cur - REG00_IINLIM_BASE) / REG00_IINLIM_LSB;
	return sgm41511_update_bits(info, SGM4151X_REG_00, REG00_IINLIM_MASK,
				    reg_val << REG00_IINLIM_SHIFT);

}

static int sgm41511_charger_hw_init(struct sgm41511_charger_info *info)
{
	struct sprd_battery_info bat_info = {};
	int ret;

	ret = sprd_battery_get_battery_info(info->psy_usb, &bat_info, 0);
	if (ret) {
		dev_warn(info->dev, "no battery information is supplied\n");
		return -EPROBE_DEFER;
	}

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

	info->voltage_max_microvolt = bat_info.constant_charge_voltage_max_uv / 1000;
	sprd_battery_put_battery_info(info->psy_usb, &bat_info);

	ret = sgm41511_charger_set_safety_cur(info, info->cur.dcp_cur);
	if (ret) {
		dev_err(info->dev, "set sgm41511 safety cur failed\n");
		return ret;
	}

	if (info->role ==  SGM41511_ROLE_MASTER) {
		ret = sgm41511_set_acovp_threshold(info, SGM41511_FCHG_OVP_6V);
		if (ret)
			dev_err(info->dev, "set sgm41511 ovp failed\n");
	} else if (info->role == SGM41511_ROLE_SLAVE) {
		ret = sgm41511_set_acovp_threshold(info, SGM41511_FCHG_OVP_9V);
		if (ret)
			dev_err(info->dev, "set sgm41511 slave ovp failed\n");
	}

	ret = sgm41511_enable_term(info, 1);
	if (ret) {
		dev_err(info->dev, "set sgm41511 terminal cur failed\n");
		return ret;
	}

	ret = sgm41511_charger_set_vindpm(info, info->voltage_max_microvolt);
	if (ret) {
		dev_err(info->dev, "set sgm41511 vindpm vol failed\n");
		return ret;
	}

	sgm41511_update_bits(info, SGM4151X_REG_01, REG01_WDT_RESET_MASK,
			     REG01_WDT_RESET << REG01_WDT_RESET_SHIFT);
	ret = sgm41511_update_bits(info, SGM4151X_REG_05, REG05_WDT_MASK,
				   REG05_WDT_DISABLE << REG05_WDT_SHIFT);
	if (ret) {
		dev_err(info->dev, "feed sgm41511 watchdog failed\n");
		return ret;
	}

	ret = sgm41511_charger_set_termina_vol(info, info->voltage_max_microvolt);
	if (ret) {
		dev_err(info->dev, "set sgm41511 terminal vol failed\n");
		return ret;
	}

	ret = sgm41511_charger_set_limit_current(info,
						 info->cur.unknown_cur, false);
	if (ret)
		dev_err(info->dev, "set sgm41511 limit current failed\n");

	return ret;
}

static int sgm41511_charger_get_charge_voltage(struct sgm41511_charger_info *info, u32 *charge_vol)
{
	struct power_supply *psy;
	union power_supply_propval val;
	int ret;

	psy = power_supply_get_by_name(SGM41511_BATTERY_NAME);
	if (!psy) {
		dev_err(info->dev, "failed to get SGM41511_BATTERY_NAME\n");
		return -ENODEV;
	}

	val.intval = 0;
	ret = power_supply_get_property(psy,
					POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE,
					&val);
	power_supply_put(psy);
	if (ret) {
		dev_err(info->dev, "failed to get CONSTANT_CHARGE_VOLTAGE\n");
		return ret;
	}

	*charge_vol = val.intval;

	return 0;
}

static void sgm41511_dump_register(struct sgm41511_charger_info *info)
{
	int ret;
	u8 addr;
	u8 val;

	for (addr = 0x0; addr <= 0x0B; addr++) {
		ret = sgm41511_read(info, addr, &val);
		if (ret == 0)
			dev_info(info->dev, "dump reg %s,%d 0x%x = 0x%x\n",
				 __func__, __LINE__, addr, val);
	}
}

static int sgm41511_charger_enable_wdg(struct sgm41511_charger_info *info, bool en)
{
	u8 val = REG05_WDT_DISABLE;

	if (en)
		val = REG05_WDT_40S;

	return sgm41511_update_bits(info, SGM4151X_REG_05,
				    SGM41511_REG_WATCHDOG_TIMER_MASK,
				    val << REG05_WDT_SHIFT);
}

static int sgm41511_charger_start_charge(struct sgm41511_charger_info *info)
{
	int ret = 0;

	dev_info(info->dev, "%s:line%d: start charge\n", __func__, __LINE__);

	ret = sgm41511_exit_hiz_mode(info);
	if (ret)
		dev_err(info->dev, "disable HIZ mode failed\n");

	ret = sgm41511_charger_enable_wdg(info, true);
	if (ret) {
		dev_err(info->dev, "%s, failed to enable watchdog, ret = %d\n", __func__, ret);
		return ret;
	}

	if (info->role == SGM41511_ROLE_MASTER) {
		ret = regmap_update_bits(info->pmic, info->charger_pd,
					 info->charger_pd_mask, 0);
		if (ret) {
			dev_err(info->dev, "enable sgm41511 charge failed\n");
			return ret;
		}
	} else if (info->role == SGM41511_ROLE_SLAVE) {
		gpiod_set_value_cansleep(info->gpiod, 0);
	}

	ret = sgm41511_enable_charger(info, true);
	if (ret) {
		dev_err(info->dev, "%s, failed to enable charger, ret = %d\n", __func__, ret);
		return ret;
	}

	sgm41511_dump_register(info);

	return ret;
}

static void sgm41511_charger_stop_charge(struct sgm41511_charger_info *info)
{
	int ret;

	dev_info(info->dev, "%s:line%d: stop charge\n", __func__, __LINE__);

	ret = sgm41511_enable_charger(info, false);
	if (ret)
		dev_err(info->dev, "disable charger failed\n");

	if (info->role == SGM41511_ROLE_MASTER) {
		if (boot_calibration) {
			ret = sgm41511_enter_hiz_mode(info);
			if (ret)
				dev_err(info->dev, "enable HIZ mode failed\n");
		}

		ret = regmap_update_bits(info->pmic, info->charger_pd,
					 info->charger_pd_mask,
					 info->charger_pd_mask);
		if (ret)
			dev_err(info->dev, "disable sgm41511 charge failed\n");
	} else if (info->role == SGM41511_ROLE_SLAVE) {
		if (boot_calibration) {
			ret = sgm41511_enter_hiz_mode(info);
			if (ret)
				dev_err(info->dev, "enable HIZ mode failed\n");
		}

		gpiod_set_value_cansleep(info->gpiod, 1);
	}

	if (info->disable_power_path) {
		ret = sgm41511_update_bits(info, SGM4151X_REG_00,
					   SGM41511_REG_EN_HIZ_MASK,
					   0x01 << SGM41511_REG_EN_HIZ_SHIFT);
		if (ret)
			dev_err(info->dev, "Failed to disable power path\n");
	}

	ret = sgm41511_charger_enable_wdg(info, false);
	if (ret)
		dev_err(info->dev, "%s, failed to disable watchdog, ret = %d\n", __func__, ret);
}

static int sgm41511_charger_set_current(struct sgm41511_charger_info *info, u32 cur)
{
	u8 ichg;

	dev_dbg(info->dev, "%s:line%d: set ibat cur = %d\n", __func__, __LINE__, cur);

	cur = cur / 1000;
	if (cur > SGM41511_ICHG_CURRENT_MAX) {
		ichg = 0x32;
	} else {
		if (cur < REG02_ICHG_BASE)
			cur = REG02_ICHG_BASE;

		ichg = (cur - REG02_ICHG_BASE)/REG02_ICHG_LSB;
	}
	return sgm41511_update_bits(info, SGM4151X_REG_02, REG02_ICHG_MASK,
				    ichg << REG02_ICHG_SHIFT);
}

static int sgm41511_charger_get_current(struct sgm41511_charger_info *info, u32 *cur)
{
	u8 reg_val;
	int ret;

	ret = sgm41511_read(info, SGM4151X_REG_02, &reg_val);
	if (ret < 0)
		return ret;

	reg_val &= REG02_ICHG_MASK;
	reg_val = reg_val >> REG02_ICHG_SHIFT;
	*cur = ((reg_val * REG02_ICHG_LSB) + REG02_ICHG_BASE) * 1000;
	return 0;
}

static int sgm41511_charger_get_health(struct sgm41511_charger_info *info, u32 *health)
{
	*health = POWER_SUPPLY_HEALTH_GOOD;

	return 0;
}

static int sgm41511_charger_feed_watchdog(struct sgm41511_charger_info *info)
{
	int ret = 0;
	u8 reg_val = REG01_WDT_RESET << REG01_WDT_RESET_SHIFT;
	u64 duration, curr = ktime_to_ms(ktime_get());

	ret = sgm41511_update_bits(info, SGM4151X_REG_01, REG01_WDT_RESET_MASK, reg_val);
	if (ret) {
		dev_err(info->dev, "reset sgm41511 failed\n");
		return ret;
	}

	duration = curr - info->last_wdt_time;
	if (duration >= SGM41511_WATCH_DOG_TIME_OUT_MS) {
		dev_err(info->dev, "charger wdg maybe time out:%lld ms\n", duration);
		sgm41511_dump_register(info);
	}

	info->last_wdt_time = curr;

	if (info->otg_enable)
		return ret;

	ret = sgm41511_charger_set_limit_current(info, info->actual_limit_cur, true);
	if (ret)
		dev_err(info->dev, "set limit cur failed\n");

	return ret;
}

static int sgm41511_charger_get_status(struct sgm41511_charger_info *info)
{
	if (info->charging)
		return POWER_SUPPLY_STATUS_CHARGING;
	else
		return POWER_SUPPLY_STATUS_NOT_CHARGING;
}

static bool sgm41511_charger_get_power_path_status(struct sgm41511_charger_info *info)
{
	u8 value;
	int ret;
	bool power_path_enabled = true;

	ret = sgm41511_read(info, SGM4151X_REG_00, &value);
	if (ret < 0) {
		dev_err(info->dev, "Fail to get power path status, ret = %d\n", ret);
		return power_path_enabled;
	}

	if (value & SGM41511_REG_EN_HIZ_MASK)
		power_path_enabled = false;

	return power_path_enabled;
}

static int sgm41511_charger_set_status(struct sgm41511_charger_info *info, int val, u32 input_vol)
{
	int ret = 0;

	if (val == CM_BUCK_MAX_TERMINA_VOL) {
		ret = sgm41511_charger_set_termina_vol(info, SGM41511_TERMINA_VOLTAGE_MAX);
		if (ret) {
			dev_err(info->dev, "failed to set terminate max voltage\n");
			return ret;
		}
	} else if (val == CM_FAST_CHARGE_MAX_OVP_ENABLE_CMD) {
		ret = sgm41511_set_acovp_threshold(info, SGM41511_FCHG_OVP_14V);
		if (ret) {
			dev_err(info->dev, "failed to set fast charge max ovp\n");
			return ret;
		}
	} else if (val == CM_FAST_CHARGE_OVP_ENABLE_CMD) {
		ret = sgm41511_set_acovp_threshold(info, SGM41511_FCHG_OVP_9V);
		if (ret) {
			dev_err(info->dev, "failed to set 9V fast charge ovp\n");
			return ret;
		}
	} else if (val == CM_FAST_CHARGE_OVP_DISABLE_CMD) {
		ret = sgm41511_set_acovp_threshold(info, SGM41511_FCHG_OVP_6V);
		if (ret) {
			dev_err(info->dev, "failed to set 9V fast charge ovp\n");
			return ret;
		}
		if (info->role == SGM41511_ROLE_MASTER) {
			if (input_vol > SGM41511_FAST_CHARGER_VOLTAGE_MAX)
				info->need_disable_Q1 = true;
		}
	} else if ((val == false) && (info->role == SGM41511_ROLE_MASTER)) {
		if (input_vol > SGM41511_NORMAL_CHARGER_VOLTAGE_MAX)
			info->need_disable_Q1 = true;
	}

	if (val > CM_FAST_CHARGE_NORMAL_CMD)
		return 0;
	if (!val && info->charging) {
		sgm41511_charger_stop_charge(info);
		info->charging = false;
	} else if (val && !info->charging) {
		ret = sgm41511_charger_start_charge(info);
		if (ret)
			dev_err(info->dev, "start charge failed\n");
		else
			info->charging = true;
	}

	return ret;
}

static bool sgm41511_probe_is_ready(struct sgm41511_charger_info *info)
{
	unsigned long timeout;

	if (unlikely(!info->probe_initialized)) {
		timeout = wait_for_completion_timeout(&info->probe_init, SGM41511_PROBE_TIMEOUT);
		if (!timeout) {
			dev_err(info->dev, "%s wait probe timeout\n", __func__);
			return false;
		}
	}

	return true;
}

static int sgm41511_charger_check_power_path_status(struct sgm41511_charger_info *info)
{
	int ret = 0;
	u8 val;

	if (info->disable_power_path)
		return 0;

	if (sgm41511_charger_get_power_path_status(info))
		return 0;

	ret = sgm41511_read(info, SGM4151X_REG_00, &val);
	if (ret < 0) {
		dev_err(info->dev, "%s:line%d, failed to get reg0(%d)\n", __func__, __LINE__, ret);
		return ret;
	}

	if (val & REG00_ENHIZ_MASK) {
		dev_info(info->dev, "%s:line%d, exit hiz mode\n", __func__, __LINE__);
		ret = sgm41511_exit_hiz_mode(info);
		if (ret < 0) {
			dev_err(info->dev, "%s:line%d, failed to exit hiz(%d)\n",
				__func__, __LINE__, ret);
			return ret;
		}
	}

	return ret;
}

static int sgm41511_charger_usb_get_property(struct power_supply *psy,
					     enum power_supply_property psp,
					     union power_supply_propval *val)
{
	struct sgm41511_charger_info *info = power_supply_get_drvdata(psy);
	u32 cur = 0, health, enabled = 0;
	int ret = 0;

	if (!info) {
		pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
		return -EINVAL;
	}

	if (unlikely(!psy->initialized && atomic_read(&psy->use_cnt) > 0)) {
		dev_err(info->dev, "%s psy is not ready\n", __func__);
		return -ENODEV;
	}

	if (!sgm41511_probe_is_ready(info)) {
		dev_err(info->dev, "%s wait probe timeout\n", __func__);
		return -EINVAL;
	}

	mutex_lock(&info->lock);
	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
		if (val->intval == CM_POWER_PATH_ENABLE_CMD ||
		    val->intval == CM_POWER_PATH_DISABLE_CMD) {
			val->intval = sgm41511_charger_get_power_path_status(info);
			break;
		} else if (val->intval == CM_BUCK_MAX_TERMINA_VOL) {
			val->intval = SGM41511_TERMINA_VOLTAGE_MAX * 1000;
			break;
		}

		val->intval = sgm41511_charger_get_status(info);
		break;
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT:
		if (!info->charging) {
			val->intval = 0;
		} else {
			ret = sgm41511_charger_get_current(info, &cur);
			if (ret)
				goto out;

			val->intval = cur;
		}
		break;
	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
		if (!info->charging) {
			val->intval = 0;
		} else {
			ret = sgm41511_charger_get_limit_current(info, &cur);
			if (ret)
				goto out;

			val->intval = cur;
		}
		break;
	case POWER_SUPPLY_PROP_HEALTH:
		if (info->charging) {
			val->intval = 0;
		} else {
			ret = sgm41511_charger_get_health(info, &health);
			if (ret)
				goto out;

			val->intval = health;
		}
		break;
	case POWER_SUPPLY_PROP_CALIBRATE:
		if (info->role == SGM41511_ROLE_MASTER) {
			ret = regmap_read(info->pmic, info->charger_pd, &enabled);
			if (ret) {
				dev_err(info->dev, "get sgm41511 charge status failed\n");
				goto out;
			}
		} else if (info->role == SGM41511_ROLE_SLAVE) {
			enabled = gpiod_get_value_cansleep(info->gpiod);
		}

		val->intval = !enabled;
		break;
	default:
		ret = -EINVAL;
	}

out:
	mutex_unlock(&info->lock);
	return ret;
}

static int sgm41511_charger_usb_set_property(struct power_supply *psy,
					     enum power_supply_property psp,
					     const union power_supply_propval *val)
{
	struct sgm41511_charger_info *info = power_supply_get_drvdata(psy);
	int ret = 0;
	u32 input_vol;
	bool bat_present;

	if (!info) {
		pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
		return -EINVAL;
	}

	if (unlikely(!psy->initialized && atomic_read(&psy->use_cnt) > 0)) {
		dev_err(info->dev, "%s psy is not ready\n", __func__);
		return -ENODEV;
	}

	/*
	 * input_vol and bat_present should be assigned a value, only if psp is
	 * POWER_SUPPLY_PROP_STATUS and POWER_SUPPLY_PROP_CALIBRATE.
	 */
	if (psp == POWER_SUPPLY_PROP_STATUS || psp == POWER_SUPPLY_PROP_CALIBRATE) {
		bat_present = sgm41511_charger_is_bat_present(info);
		ret = sgm41511_charger_get_charge_voltage(info, &input_vol);
		if (ret) {
			input_vol = 0;
			dev_err(info->dev, "failed to get charge voltage! ret = %d\n", ret);
		}
	}

	if (!sgm41511_probe_is_ready(info)) {
		dev_err(info->dev, "%s wait probe timeout\n", __func__);
		return -EINVAL;
	}

	mutex_lock(&info->lock);

	switch (psp) {
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT:
		ret = sgm41511_charger_set_current(info, val->intval);
		if (ret < 0)
			dev_err(info->dev, "set charge current failed\n");
		break;
	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
		ret = sgm41511_charger_set_limit_current(info, val->intval, false);
		if (ret < 0)
			dev_err(info->dev, "set input current limit failed\n");
		break;

	case POWER_SUPPLY_PROP_STATUS:
		if (val->intval == CM_POWER_PATH_ENABLE_CMD) {
			ret = sgm41511_exit_hiz_mode(info);
			break;
		} else if (val->intval == CM_POWER_PATH_DISABLE_CMD) {
			ret = sgm41511_enter_hiz_mode(info);
			break;
		}

		ret = sgm41511_charger_set_status(info, val->intval, input_vol);
		if (ret < 0)
			dev_err(info->dev, "set charge status failed\n");
		break;
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE_MAX:
		ret = sgm41511_charger_set_termina_vol(info, val->intval / 1000);
		if (ret < 0)
			dev_err(info->dev, "failed to set terminate voltage\n");
		break;
	case POWER_SUPPLY_PROP_CALIBRATE:
		dev_info(info->dev, "POWER_SUPPLY_PROP_CHARGE_ENABLED = %d\n", val->intval);
		if (val->intval == true) {
			ret = sgm41511_charger_start_charge(info);
			if (ret)
				dev_err(info->dev, "start charge failed\n");
		} else if (val->intval == false) {
			sgm41511_charger_stop_charge(info);
		}
		break;
	case POWER_SUPPLY_PROP_PRESENT:
		info->is_charger_online = val->intval;
		if (val->intval == true) {
			info->last_wdt_time = ktime_to_ms(ktime_get());
			schedule_delayed_work(&info->wdt_work, 0);
		} else {
			info->actual_limit_cur = 0;
			cancel_delayed_work_sync(&info->wdt_work);
		}
		break;
	default:
		ret = -EINVAL;
	}

	mutex_unlock(&info->lock);
	return ret;
}

static int sgm41511_charger_property_is_writeable(struct power_supply *psy,
						  enum power_supply_property psp)
{
	int ret;

	switch (psp) {
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT:
	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
	case POWER_SUPPLY_PROP_CALIBRATE:
	case POWER_SUPPLY_PROP_STATUS:
	case POWER_SUPPLY_PROP_PRESENT:
		ret = 1;
		break;

	default:
		ret = 0;
	}

	return ret;
}

static enum power_supply_property sgm41511_usb_props[] = {
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT,
	POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT,
	POWER_SUPPLY_PROP_HEALTH,
	POWER_SUPPLY_PROP_CALIBRATE,
};

static const struct power_supply_desc sgm41511_charger_desc = {
	.name			= "sgm41511_charger",
	.type			= POWER_SUPPLY_TYPE_UNKNOWN,
	.properties		= sgm41511_usb_props,
	.num_properties		= ARRAY_SIZE(sgm41511_usb_props),
	.get_property		= sgm41511_charger_usb_get_property,
	.set_property		= sgm41511_charger_usb_set_property,
	.property_is_writeable	= sgm41511_charger_property_is_writeable,
};

static const struct power_supply_desc sgm41511_slave_charger_desc = {
	.name			= "sgm41511_slave_charger",
	.type			= POWER_SUPPLY_TYPE_UNKNOWN,
	.properties		= sgm41511_usb_props,
	.num_properties		= ARRAY_SIZE(sgm41511_usb_props),
	.get_property		= sgm41511_charger_usb_get_property,
	.set_property		= sgm41511_charger_usb_set_property,
	.property_is_writeable	= sgm41511_charger_property_is_writeable,
};

static void sgm41511_charger_feed_watchdog_work(struct work_struct *work)
{
	struct delayed_work *dwork = to_delayed_work(work);
	struct sgm41511_charger_info *info = container_of(dwork,
							  struct sgm41511_charger_info,
							  wdt_work);
	int ret;

	if (!info) {
		pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
		return;
	}

	ret = sgm41511_charger_feed_watchdog(info);
	if (ret)
		schedule_delayed_work(&info->wdt_work, HZ * 1);
	else
		schedule_delayed_work(&info->wdt_work, HZ * 15);
}

#if IS_ENABLED(CONFIG_REGULATOR)
static bool sgm41511_charger_check_otg_valid(struct sgm41511_charger_info *info)
{
	int ret;
	u8 value = 0;
	bool status = false;

	ret = sgm41511_read(info, SGM4151X_REG_01, &value);
	if (ret) {
		dev_err(info->dev, "get sgm41511 charger otg valid status failed\n");
		return status;
	}

	if (value & SGM41511_REG_OTG_MASK)
		status = true;
	else
		dev_err(info->dev, "otg is not valid, REG_1 = 0x%x\n", value);

	return status;
}

static bool sgm41511_charger_check_otg_fault(struct sgm41511_charger_info *info)
{
	int ret;
	u8 value = 0;
	bool status = true;

	ret = sgm41511_read(info, SGM4151X_REG_09, &value);
	if (ret) {
		dev_err(info->dev, "get sgm41511 charger otg fault status failed\n");
		return status;
	}

	if (!(value & SGM41511_REG_BOOST_FAULT_MASK))
		status = false;
	else
		dev_err(info->dev, "boost fault occurs, REG_9 = 0x%x\n", value);

	return status;
}

static void sgm41511_charger_otg_work(struct work_struct *work)
{
	struct delayed_work *dwork = to_delayed_work(work);
	struct sgm41511_charger_info *info = container_of(dwork,
							  struct sgm41511_charger_info, otg_work);
	bool otg_valid;
	bool otg_fault;
	int ret, retry = 0;

	if (!info) {
		pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
		return;
	}

	otg_valid = sgm41511_charger_check_otg_valid(info);
	if (otg_valid)
		goto out;

	do {
		otg_fault = sgm41511_charger_check_otg_fault(info);
		if (!otg_fault) {
			ret = sgm41511_update_bits(info, SGM4151X_REG_01,
						   REG01_OTG_CONFIG_MASK,
						   REG01_OTG_ENABLE << REG01_OTG_CONFIG_SHIFT);
			if (ret)
				dev_err(info->dev, "restart sgm41511 charger otg failed\n");
		}
		otg_valid = sgm41511_charger_check_otg_valid(info);
	} while (!otg_valid && retry++ < SGM41511_OTG_RETRY_TIMES);

	if (retry >= SGM41511_OTG_RETRY_TIMES) {
		dev_err(info->dev, "Restart OTG failed\n");
		return;
	}

out:
	schedule_delayed_work(&info->otg_work, msecs_to_jiffies(1500));
}

static int sgm41511_charger_enable_otg(struct regulator_dev *dev)
{
	struct sgm41511_charger_info *info = rdev_get_drvdata(dev);
	int ret = 0;

	if (!info) {
		pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
		return -EINVAL;
	}

	if (info->shutdown_flag)
		return ret;

	sgm41511_charger_dump_stack();

	if (!sgm41511_probe_is_ready(info)) {
		dev_err(info->dev, "%s wait probe timeout\n", __func__);
		return -EINVAL;
	}
	/*
	 * Disable charger detection function in case
	 * affecting the OTG timing sequence.
	 */
	if (!info->use_typec_extcon) {
		ret = regmap_update_bits(info->pmic, info->charger_detect,
					 BIT_DP_DM_BC_ENB, BIT_DP_DM_BC_ENB);
		if (ret) {
			dev_err(info->dev, "failed to disable bc1.2 detect function.\n");
			return ret;
		}
	}
	ret = sgm41511_update_bits(info, SGM4151X_REG_01,
				   REG01_OTG_CONFIG_MASK, REG01_OTG_ENABLE << REG01_OTG_CONFIG_SHIFT);
	if (ret) {
		dev_err(info->dev, "enable sgm41511 otg failed\n");
		regmap_update_bits(info->pmic, info->charger_detect, BIT_DP_DM_BC_ENB, 0);
		return ret;
	}

	ret = sgm41511_charger_enable_wdg(info, true);
	if (ret) {
		dev_err(info->dev, "%s, failed to enable watchdog, ret = %d\n", __func__, ret);
		return ret;
	}

	ret = sgm41511_charger_feed_watchdog(info);
	if (ret) {
		dev_err(info->dev, "%s, failed to feed watchdog, ret = %d\n", __func__, ret);
		return ret;
	}

	ret = sgm41511_exit_hiz_mode(info);
	if (ret)
		dev_err(info->dev, "Failed to enable power path\n");

	info->otg_enable = true;
	info->last_wdt_time = ktime_to_ms(ktime_get());
	schedule_delayed_work(&info->wdt_work,
			      msecs_to_jiffies(SGM41511_FEED_WATCHDOG_VALID_MS));
	schedule_delayed_work(&info->otg_work,
			      msecs_to_jiffies(SGM41511_OTG_VALID_MS));
	dev_info(info->dev, "%s:line%d:enable_otg\n", __func__, __LINE__);

	return ret;
}

static int sgm41511_charger_disable_otg(struct regulator_dev *dev)
{
	struct sgm41511_charger_info *info = rdev_get_drvdata(dev);
	int ret = 0;

	if (!info) {
		pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
		return -EINVAL;
	}

	sgm41511_charger_dump_stack();

	if (!sgm41511_probe_is_ready(info)) {
		dev_err(info->dev, "%s wait probe timeout\n", __func__);
		return -EINVAL;
	}

	info->otg_enable = false;
	cancel_delayed_work_sync(&info->wdt_work);
	cancel_delayed_work_sync(&info->otg_work);

	ret = sgm41511_update_bits(info, SGM4151X_REG_01,
				   REG01_OTG_CONFIG_MASK,
				   REG01_OTG_DISABLE << REG01_OTG_CONFIG_SHIFT);
	if (ret) {
		dev_err(info->dev, "disable sgm41511 otg failed\n");
		return ret;
	}

	ret = sgm41511_charger_enable_wdg(info, false);
	if (ret) {
		dev_err(info->dev, "%s, failed to disable watchdog, ret = %d\n", __func__, ret);
		return ret;
	}

	/* Enable charger detection function to identify the charger type */
	if (!info->use_typec_extcon) {
		ret = regmap_update_bits(info->pmic, info->charger_detect, BIT_DP_DM_BC_ENB, 0);
		if (ret)
			dev_err(info->dev, "enable BC1.2 failed\n");
	}
	dev_info(info->dev, "%s:line%d:disable_otg\n", __func__, __LINE__);

	return ret;
}

static int sgm41511_charger_vbus_is_enabled(struct regulator_dev *dev)
{
	struct sgm41511_charger_info *info = rdev_get_drvdata(dev);
	int ret;
	u8 val;

	if (!info) {
		pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
		return -EINVAL;
	}

	ret = sgm41511_read(info, SGM4151X_REG_01, &val);
	val &= REG01_OTG_CONFIG_MASK;
	val = (val >> REG01_OTG_CONFIG_SHIFT) & 0x01;
	if (ret) {
		dev_err(info->dev, "failed to get sgm41511 otg status\n");
		return ret;
	}

	return val;
}

static const struct regulator_ops sgm41511_charger_vbus_ops = {
	.enable = sgm41511_charger_enable_otg,
	.disable = sgm41511_charger_disable_otg,
	.is_enabled = sgm41511_charger_vbus_is_enabled,
};

static const struct regulator_desc sgm41511_charger_vbus_desc = {
	.name = "otg-vbus",
	.of_match = "otg-vbus",
	.type = REGULATOR_VOLTAGE,
	.owner = THIS_MODULE,
	.ops = &sgm41511_charger_vbus_ops,
	.fixed_uV = 5000000,
	.n_voltages = 1,
};

static void sgm41511_charger_check_otg_status(struct sgm41511_charger_info *info)
{
	int ret;
	u8 val;

	ret = sgm41511_read(info, SGM4151X_REG_01, &val);
	if (ret < 0) {
		dev_err(info->dev, "%s:line%d, failed to get reg1(%d)\n", __func__, __LINE__, ret);
		return;
	}

	if (val & REG01_OTG_CONFIG_MASK) {
		dev_info(info->dev, "%s:line%d, exit otg mode\n", __func__, __LINE__);
		ret = sgm41511_update_bits(info, SGM4151X_REG_01, REG01_OTG_CONFIG_MASK,
					   REG01_OTG_DISABLE << REG01_OTG_CONFIG_SHIFT);
		if (ret)
			dev_err(info->dev, "exit sgm41511 otg failed, ret = %d\n", ret);
	}
}

static int sgm41511_charger_register_vbus_regulator(struct sgm41511_charger_info *info)
{
	struct regulator_config cfg = { };
	struct regulator_dev *reg;
	int ret = 0;

	/*
	 * only master to support otg
	 */
	if (info->role != SGM41511_ROLE_MASTER)
		return 0;

	sgm41511_charger_check_otg_status(info);

	cfg.dev = info->dev;
	cfg.driver_data = info;
	reg = devm_regulator_register(info->dev, &sgm41511_charger_vbus_desc, &cfg);
	if (IS_ERR(reg)) {
		ret = PTR_ERR(reg);
		dev_err(info->dev, "%s, failed to register vddvbus regulator:%d\n", __func__, ret);
	}

	return ret;
}

static int sgm41511_charger_register_external_vbus_regulator(struct sgm41511_charger_info *info)
{
	struct regulator_config cfg = { };
	struct regulator_dev *reg;
	int ret = 0;
	struct device_node *otg_nd;
	struct device_node *otg_parent_nd;
	struct platform_device *otg_parent_nd_pdev;

	/*
	 * only master to support otg
	 */
	if (info->role != SGM41511_ROLE_MASTER)
		return 0;

	otg_nd = of_find_node_by_name(NULL, "otg-vbus");
	if (!otg_nd) {
		dev_err(info->dev, "%s, unable to get otg node\n", __func__);
		return -EPROBE_DEFER;
	}

	otg_parent_nd = of_get_parent(otg_nd);
	of_node_put(otg_nd);
	if (!otg_parent_nd) {
		dev_err(info->dev, "%s, unable to get otg parent node\n", __func__);
		return -EPROBE_DEFER;
	}

	otg_parent_nd_pdev = of_find_device_by_node(otg_parent_nd);
	of_node_put(otg_parent_nd);
	if (!otg_parent_nd_pdev) {
		dev_err(info->dev, "%s, unable to get otg parent node device\n", __func__);
		return -EPROBE_DEFER;
	}

	cfg.dev = &otg_parent_nd_pdev->dev;
	platform_device_put(otg_parent_nd_pdev);
	cfg.driver_data = info;
	reg = devm_regulator_register(cfg.dev, &sgm41511_charger_vbus_desc, &cfg);
	if (IS_ERR(reg)) {
		ret = PTR_ERR(reg);
		dev_err(info->dev, "%s, failed to register vddvbus regulator:%d\n", __func__, ret);
	}

	return ret;
}

#else
static int sgm41511_charger_register_vbus_regulator(struct sgm41511_charger_info *info)
{
	return 0;
}

static int sgm41511_charger_register_external_vbus_regulator(struct sgm41511_charger_info *info)
{
	return 0;
}
#endif

static int sgm41511_charger_detect_device(struct sgm41511_charger_info *info)
{
	int ret, part_id;
	u8 reg_val;

	ret = sgm41511_read(info, SGM4151X_REG_0B, &reg_val);
	if (ret < 0) {
		dev_err(info->dev, "%s, failed to get device id, ret = %d\n", __func__, ret);
		return ret;
	}

	part_id = (reg_val & REG0B_PN_MASK) >> REG0B_PN_SHIFT;
	if (part_id != SGM41511_DEV_ID) {
		dev_err(info->dev, "%s, the device id is 0x%x\n", __func__, part_id);
		return -EINVAL;
	}

	return ret;
}

static int sgm41511_charger_probe(struct i2c_client *client,
				  const struct i2c_device_id *id)
{
	struct i2c_adapter *adapter = to_i2c_adapter(client->dev.parent);
	struct device *dev = &client->dev;
	struct power_supply_config charger_cfg = { };
	struct sgm41511_charger_info *info;
	struct device_node *regmap_np;
	struct platform_device *regmap_pdev;
	int ret;

	if (!adapter) {
		pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
		return -EINVAL;
	}

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

	ret = sgm41511_charger_detect_device(info);
	if (ret) {
		dev_err(dev, "%s, failed to detect device, ret = %d\n", __func__, ret);
		return -ENODEV;
	}

	power_path_control(info);

	ret = sgm41511_charger_is_fgu_present(info);
	if (ret) {
		dev_err(dev, "sc27xx_fgu not ready.\n");
		return -EPROBE_DEFER;
	}

	info->use_typec_extcon = device_property_read_bool(dev, "use-typec-extcon");
	info->disable_wdg = device_property_read_bool(dev, "disable-otg-wdg-in-sleep");

	ret = device_property_read_bool(dev, "role-slave");
	if (ret)
		info->role = SGM41511_ROLE_SLAVE;
	else
		info->role = SGM41511_ROLE_MASTER;

	if (info->role == SGM41511_ROLE_SLAVE) {
		info->gpiod = devm_gpiod_get(dev, "enable", GPIOD_OUT_HIGH);
		if (IS_ERR(info->gpiod)) {
			dev_err(dev, "failed to get enable gpio\n");
			return PTR_ERR(info->gpiod);
		}
	}

	regmap_np = of_find_compatible_node(NULL, NULL, "sprd,sc27xx-syscon");
	if (!regmap_np)
		regmap_np = of_find_compatible_node(NULL, NULL, "sprd,ump962x-syscon");

	if (regmap_np) {
		if (of_device_is_compatible(regmap_np->parent, "sprd,sc2730"))
			info->charger_pd_mask = SGM41511_DISABLE_PIN_MASK_2730;
		else if (of_device_is_compatible(regmap_np->parent, "sprd,sc2721"))
			info->charger_pd_mask = SGM41511_DISABLE_PIN_MASK_2721;
		else if (of_device_is_compatible(regmap_np->parent, "sprd,sc2720"))
			info->charger_pd_mask = SGM41511_DISABLE_PIN_MASK_2720;
		else if (of_device_is_compatible(regmap_np, "sprd,ump962x-syscon"))
			info->charger_pd_mask = SGM41511_DISABLE_PIN_MASK;
		else {
			dev_err(dev, "failed to get charger_pd mask\n");
			return -EINVAL;
		}
	} else {
		dev_err(dev, "unable to get syscon node\n");
		return -ENODEV;
	}

	ret = of_property_read_u32_index(regmap_np, "reg", 1,
					 &info->charger_detect);
	if (ret) {
		dev_err(dev, "failed to get charger_detect\n");
		return -EINVAL;
	}

	ret = of_property_read_u32_index(regmap_np, "reg", 2,
					 &info->charger_pd);
	if (ret) {
		dev_err(dev, "failed to get charger_pd reg\n");
		return ret;
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
	mutex_init(&info->lock);
	mutex_init(&info->input_limit_cur_lock);
	init_completion(&info->probe_init);

	charger_cfg.drv_data = info;
	charger_cfg.of_node = dev->of_node;
	if (info->role == SGM41511_ROLE_MASTER) {
		info->psy_usb = devm_power_supply_register(dev,
							   &sgm41511_charger_desc,
							   &charger_cfg);
	} else if (info->role == SGM41511_ROLE_SLAVE) {
		info->psy_usb = devm_power_supply_register(dev,
							   &sgm41511_slave_charger_desc,
							   &charger_cfg);
	}

	if (IS_ERR(info->psy_usb)) {
		dev_err(dev, "failed to register power supply\n");
		ret = PTR_ERR(info->psy_usb);
		goto out;
	}

	ret = sgm41511_charger_hw_init(info);
	if (ret)
		goto out;

	sgm41511_charger_stop_charge(info);
	sgm41511_charger_check_power_path_status(info);

	device_init_wakeup(info->dev, true);

	alarm_init(&info->otg_timer, ALARM_BOOTTIME, NULL);
	INIT_DELAYED_WORK(&info->otg_work, sgm41511_charger_otg_work);
	INIT_DELAYED_WORK(&info->wdt_work, sgm41511_charger_feed_watchdog_work);

	if (device_property_read_bool(dev, "otg-vbus-node-external"))
		ret = sgm41511_charger_register_external_vbus_regulator(info);
	else
		ret = sgm41511_charger_register_vbus_regulator(info);

	if (ret) {
		dev_err(dev, "failed to register vbus regulator.\n");
		goto out;
	}

	info->probe_initialized = true;
	complete_all(&info->probe_init);

	sgm41511_dump_register(info);
	dev_info(dev, "use_typec_extcon = %d\n", info->use_typec_extcon);

	return 0;

out:
	mutex_destroy(&info->input_limit_cur_lock);
	mutex_destroy(&info->lock);
	return ret;
}

static void sgm41511_charger_shutdown(struct i2c_client *client)
{
	struct sgm41511_charger_info *info = i2c_get_clientdata(client);
	int ret = 0;

	if (!info) {
		pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
		return;
	}

	cancel_delayed_work_sync(&info->wdt_work);
	if (info->otg_enable) {
		info->otg_enable = false;
		cancel_delayed_work_sync(&info->otg_work);
		ret = sgm41511_update_bits(info, SGM4151X_REG_01,
					   SGM41511_REG_OTG_MASK,
					   0);
		if (ret)
			dev_err(info->dev, "disable sgm41511 otg failed ret = %d\n", ret);

		ret = sgm41511_enter_hiz_mode(info);
		if (ret)
			dev_err(info->dev, "Failed to disable power path\n");

		/* Enable charger detection function to identify the charger type */
		ret = regmap_update_bits(info->pmic, info->charger_detect,
					 BIT_DP_DM_BC_ENB, 0);
		if (ret)
			dev_err(info->dev,
				"enable charger detection function failed ret = %d\n", ret);
	}

	info->shutdown_flag = true;
	if (info->charging) {
		ret = sgm41511_charger_set_limit_current(info, SGM41511_SHUTDOWN_LIMIT, false);
		if (ret < 0)
			dev_err(info->dev, "%s: set input limit cur failed\n", __func__);

		ret = sgm41511_charger_set_current(info, SGM41511_SHUTDOWN_CURRENT);
		if (ret < 0)
			dev_err(info->dev, "%s:set charge current failed\n", __func__);
	}
}

static int sgm41511_charger_remove(struct i2c_client *client)
{
	struct sgm41511_charger_info *info = i2c_get_clientdata(client);

	if (!info) {
		pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
		return -EINVAL;
	}

	cancel_delayed_work_sync(&info->wdt_work);
	cancel_delayed_work_sync(&info->otg_work);

	mutex_destroy(&info->input_limit_cur_lock);
	mutex_destroy(&info->lock);

	return 0;
}

#if IS_ENABLED(CONFIG_PM_SLEEP)
static int sgm41511_charger_suspend(struct device *dev)
{
	ktime_t now, add;
	struct sgm41511_charger_info *info = dev_get_drvdata(dev);

	if (!info) {
		pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
		return -EINVAL;
	}

	if (info->otg_enable || info->is_charger_online) {
		if (sgm41511_charger_feed_watchdog(info))
			dev_err(info->dev, "%s, failed to feed watchdog\n", __func__);

		cancel_delayed_work_sync(&info->wdt_work);
	}

	if (!info->otg_enable)
		return 0;

	if (info->disable_wdg) {
		if (sgm41511_charger_enable_wdg(info, false)) {
			dev_err(info->dev, "%s, failed to disable watchdog\n", __func__);
			return -EBUSY;
		}
	} else {
		dev_dbg(info->dev, "%s:line%d: set alarm\n", __func__, __LINE__);
		now = ktime_get_boottime();
		add = ktime_set(SGM41511_OTG_ALARM_TIMER_S, 0);
		alarm_start(&info->otg_timer, ktime_add(now, add));
		pr_info("sgm41511_charger set alarm, triggered at [%lld]ms\n",
			ktime_to_ms(ktime_add(now, add)));
	}

	return 0;
}

static int sgm41511_charger_resume(struct device *dev)
{
	struct sgm41511_charger_info *info = dev_get_drvdata(dev);

	if (!info) {
		pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
		return -EINVAL;
	}

	if (info->otg_enable || info->is_charger_online) {
		if (sgm41511_charger_feed_watchdog(info))
			dev_err(info->dev, "%s, failed to feed watchdog\n", __func__);

		schedule_delayed_work(&info->wdt_work, HZ * 15);
	}

	if (!info->otg_enable)
		return 0;

	if (info->disable_wdg) {
		if (sgm41511_charger_enable_wdg(info, true)) {
			dev_err(info->dev, "%s, failed to enable watchdog\n", __func__);
			return -EBUSY;
		}
	} else {
		alarm_cancel(&info->otg_timer);
	}

	return 0;
}
#endif

static const struct dev_pm_ops sgm41511_charger_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(sgm41511_charger_suspend,
				sgm41511_charger_resume)
};

static const struct i2c_device_id sgm41511_i2c_id[] = {
	{"sgm41511_chg", 0},
	{"sgm41511_slave_chg", 0},
	{}
};

static const struct of_device_id sgm41511_charger_of_match[] = {
	{ .compatible = "Sgm,sgm41511_chg", },
	{ .compatible = "Sgm,sgm41511_slave_chg", },
	{ }
};

MODULE_DEVICE_TABLE(of, sgm41511_charger_of_match);

static struct i2c_driver sgm41511_charger_driver = {
	.driver = {
		.name = "sgm41511_chg",
		.of_match_table = sgm41511_charger_of_match,
		.pm = &sgm41511_charger_pm_ops,
	},
	.probe = sgm41511_charger_probe,
	.shutdown = sgm41511_charger_shutdown,
	.remove = sgm41511_charger_remove,
	.id_table = sgm41511_i2c_id,
};

module_i2c_driver(sgm41511_charger_driver);
MODULE_DESCRIPTION("SGM41511 Charger Driver");
MODULE_LICENSE("GPL v2");
