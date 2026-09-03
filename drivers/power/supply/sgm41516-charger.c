// SPDX-License-Identifier: GPL-2.0:
// Copyright (c) 2021 unisoc.

/*
 * Driver for the Sgm sgm41516 charger.
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

#include <linux/power/sgm41516_reg.h>

#define BIT_DP_DM_BC_ENB			BIT(0)
#define SGM41516_OTG_VALID_MS			500
#define SGM41516_FEED_WATCHDOG_VALID_MS		50
#define SGM41516_OTG_ALARM_TIMER_S		15

#define SGM41516_DISABLE_PIN_MASK		BIT(0)
#define SGM41516_DISABLE_PIN_MASK_2730		BIT(0)
#define SGM41516_DISABLE_PIN_MASK_2721		BIT(15)
#define SGM41516_DISABLE_PIN_MASK_2720		BIT(0)

#define SGM41516_OTG_RETRY_TIMES		10

#define SGM41516_ROLE_MASTER			1
#define SGM41516_ROLE_SLAVE			2

#define SGM41516_FCHG_OVP_6V			6000
#define SGM41516_FCHG_OVP_9V			9000
#define SGM41516_FCHG_OVP_14V			14000

#define SGM41516_WAKE_UP_MS			1000
#define SGM41516_PROBE_TIMEOUT			msecs_to_jiffies(500)

#define SGM41516_WATCH_DOG_TIME_OUT_MS		20000
#define SGM41516_SHUTDOWN_LIMIT			600000
#define SGM41516_SHUTDOWN_CURRENT		500000

static bool boot_calibration;

enum {
	SGM41516_MASTER,
	SGM41516_SLAVE,
};

static int sgm41516_mode_data[] = {
	[SGM41516_MASTER] = SGM41516_ROLE_MASTER,
	[SGM41516_SLAVE] = SGM41516_ROLE_SLAVE,
};

/* SGM41516 Register 0x00 IINDPM[4:0] */
static const u32 sgm41516_iindpm[] = {
	100, 200, 300, 400, 500, 600, 700, 800,
	900, 1000, 1100, 1200, 1300, 1400, 1500, 1600,
	1700, 1800, 1900, 2000, 2100, 2200, 2300, 2400,
	2500, 2600, 2700, 2800, 2900, 3000, 3100, 3800
};

struct sgm41516_charge_current {
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

struct sgm41516_charger_info {
	struct i2c_client *client;
	struct device *dev;
	struct power_supply *psy_usb;
	struct sgm41516_charge_current cur;
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
	u32 last_limit_current;
	u32 actual_limit_cur;
	u32 role;
	u64 last_wdt_time;
	struct alarm otg_timer;
	bool disable_wdg;
	bool otg_enable;
	bool is_charger_online;
	bool disable_power_path;
	bool probe_initialized;
	bool use_typec_extcon;
	bool shutdown_flag;

	struct power_supply_desc psy_desc;
	struct power_supply_config psy_cfg;
};

static bool enable_dump_stack;
module_param(enable_dump_stack, bool, 0644);

static void sgm41516_charger_dump_stack(void)
{
	if (enable_dump_stack)
		dump_stack();
}

static void sgm41516_charger_power_path_control(struct sgm41516_charger_info *info)
{
	struct device_node *cmdline_node;
	const char *cmd_line;
	int ret;
	char *match;
	char result[5];

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
		memcpy(result, (match + strlen("sprdboot.mode=")),
			sizeof(result) - 1);
		if ((!strcmp(result, "cali")) || (!strcmp(result, "auto")))
			info->disable_power_path = true;

		if (!strcmp(result, "cali"))
			boot_calibration = true;
	}
}

static int sgm41516_read(struct sgm41516_charger_info *info, u8 reg, u8 *data)
{
	int ret;

	ret = i2c_smbus_read_byte_data(info->client, reg);
	if (ret < 0)
		return ret;

	*data = ret;
	return 0;
}

static int sgm41516_write(struct sgm41516_charger_info *info, u8 reg, u8 data)
{
	return i2c_smbus_write_byte_data(info->client, reg, data);
}

static int sgm41516_update_bits(struct sgm41516_charger_info *info, u8 reg, u8 mask, u8 data)
{
	u8 v;
	int ret;

	ret = sgm41516_read(info, reg, &v);
	if (ret < 0) {
		dev_err(info->dev, "%s, failed to read reg[%02X], ret = %d\n",
			__func__, reg, ret);
		return ret;
	}

	v &= ~mask;
	v |= (data & mask);

	return sgm41516_write(info, reg, v);
}

static int sgm41516_charger_get_limit_current(struct sgm41516_charger_info *info,
					      u32 *limit_cur)
{
	u8 reg_val;
	int ret;

	ret = sgm41516_read(info, SGM41516_REG_00, &reg_val);
	if (ret < 0) {
		dev_err(info->dev, "%s, failed to read reg[%02X], ret = %d\n",
			__func__, SGM41516_REG_00, ret);
		return ret;
	}

	reg_val = (reg_val & SGM41516_IINLIM_MASK) >> SGM41516_IINLIM_SHIFT;
	if (reg_val >= ARRAY_SIZE(sgm41516_iindpm))
		reg_val = sgm41516_iindpm[ARRAY_SIZE(sgm41516_iindpm) - 1];

	*limit_cur = sgm41516_iindpm[reg_val] * 1000;
	return 0;
}

static int sgm41516_charger_set_limit_current(struct sgm41516_charger_info *info,
					      u32 limit_cur, bool enable)
{
	u8 reg_val;
	int ret = 0, i;

	mutex_lock(&info->input_limit_cur_lock);
	if (enable) {
		ret = sgm41516_charger_get_limit_current(info, &limit_cur);
		if (ret) {
			dev_err(info->dev, "get limit cur failed\n");
			goto out;
		}

		if (limit_cur == info->actual_limit_cur)
			goto out;
		limit_cur = info->actual_limit_cur;
		dev_info(info->dev, "set limit current limit_cur = %d\n", limit_cur);
	}

	limit_cur = limit_cur / 1000;
	for (i = 0; i < ARRAY_SIZE(sgm41516_iindpm); i++) {
		if (limit_cur < sgm41516_iindpm[i])
			break;
	}

	if (i == 0)
		reg_val = 0;
	else
		reg_val = i - 1;

	info->actual_limit_cur = sgm41516_iindpm[reg_val] * 1000;
	ret = sgm41516_update_bits(info, SGM41516_REG_00,
				   SGM41516_IINLIM_MASK,
				   reg_val << SGM41516_IINLIM_SHIFT);
	if (ret)
		dev_err(info->dev, "set sgm41516 limit cur failed\n");

	dev_info(info->dev, "set limit_cur = %d, reg_val = %#x, actual_limit_cur = %d\n",
		 limit_cur, reg_val, info->actual_limit_cur);

out:
	mutex_unlock(&info->input_limit_cur_lock);

	return ret;
}

static int sgm41516_charger_set_acovp_threshold(struct sgm41516_charger_info *info, int volt)
{
	u8 reg_val;

	if (volt <= 5500)
		reg_val = SGM41516_OVP_5P5V;
	else if (volt <= 6500)
		reg_val = SGM41516_OVP_6P5V;
	else if (volt <= 10500)
		reg_val = SGM41516_OVP_10P5V;
	else
		reg_val = SGM41516_OVP_14V;

	return sgm41516_update_bits(info, SGM41516_REG_06,
				    SGM41516_OVP_MASK,
				    reg_val << SGM41516_OVP_SHIFT);
}

static int sgm41516_charger_enable(struct sgm41516_charger_info *info, bool enable)
{
	u8 val = SGM41516_CHG_DISABLE;

	if (enable)
		val = SGM41516_CHG_ENABLE;

	return sgm41516_update_bits(info, SGM41516_REG_01,
				    SGM41516_CHG_CONFIG_MASK,
				    val  << SGM41516_CHG_CONFIG_SHIFT);
}

static int sgm41516_charger_enable_hiz_mode(struct sgm41516_charger_info *info, bool enable)
{
	u8 val = SGM41516_HIZ_DISABLE;

	if (enable)
		val = SGM41516_HIZ_ENABLE;

	return sgm41516_update_bits(info, SGM41516_REG_00,
				    SGM41516_ENHIZ_MASK,
				    val << SGM41516_ENHIZ_SHIFT);
}

static bool sgm41516_charger_hiz_mode_is_enabled(struct sgm41516_charger_info *info)
{
	bool hiz_mode_is_enabled = false;
	u8 reg_val = 0;
	int ret = 0;

	ret = sgm41516_read(info, SGM41516_REG_00, &reg_val);
	if (ret) {
		dev_err(info->dev, "%s, failed to read reg[%02X], ret = %d\n",
			__func__, SGM41516_REG_00, ret);
		return hiz_mode_is_enabled;
	}

	if (reg_val & SGM41516_ENHIZ_MASK)
		hiz_mode_is_enabled = true;

	return hiz_mode_is_enabled;
}

static int sgm41516_charger_enable_power_path(struct sgm41516_charger_info *info, bool enable)
{
	return sgm41516_charger_enable_hiz_mode(info, !enable);
}

static bool sgm41516_charger_power_path_is_enabled(struct sgm41516_charger_info *info)
{
	return !sgm41516_charger_hiz_mode_is_enabled(info);
}

static int sgm41516_charger_enable_term(struct sgm41516_charger_info *info, bool enable)
{
	u8 val = SGM41516_TERM_DISABLE;

	if (enable)
		val = SGM41516_TERM_ENABLE;

	return sgm41516_update_bits(info, SGM41516_REG_05,
				    SGM41516_EN_TERM_MASK,
				    val << SGM41516_EN_TERM_SHIFT);
}

static int sgm41516_charger_set_vindpm_th_base(struct sgm41516_charger_info *info,
					       int vindpm_th_base)
{
	u8 reg_val = SGM41516_VINDPM_OS_3P9V;

	if (vindpm_th_base >= 10500)
		reg_val = SGM41516_VINDPM_OS_10P5V;
	else if (vindpm_th_base >= 7500)
		reg_val = SGM41516_VINDPM_OS_7P5V;
	else if (vindpm_th_base >= 5900)
		reg_val = SGM41516_VINDPM_OS_5P9V;

	return sgm41516_update_bits(info, SGM41516_REG_0F,
				    SGM41516_VINDPM_OS_MASK,
				    reg_val << SGM41516_VINDPM_OS_SHIFT);
}

static int sgm41516_charger_set_vindpm(struct sgm41516_charger_info *info, u32 vol_th)
{
	u8 reg_val;
	int ret;
	int vindpm_th_base = SGM41516_VINDPM_OS_3P9V_BASE;
	int vindpm_th_max = SGM41516_VINDPM_OS_3P9V_MAX;

	if (vol_th >= SGM41516_VINDPM_OS_10P5V_BASE) {
		vindpm_th_base = SGM41516_VINDPM_OS_10P5V_BASE;
		vindpm_th_max = SGM41516_VINDPM_OS_10P5V_MAX;
	} else if (vol_th >= SGM41516_VINDPM_OS_7P5V_BASE) {
		vindpm_th_base = SGM41516_VINDPM_OS_7P5V_BASE;
		vindpm_th_max = SGM41516_VINDPM_OS_7P5V_MAX;
	} else if (vol_th >= SGM41516_VINDPM_OS_5P9V_BASE) {
		vindpm_th_base = SGM41516_VINDPM_OS_5P9V_BASE;
		vindpm_th_max = SGM41516_VINDPM_OS_5P9V_MAX;
	}

	ret = sgm41516_charger_set_vindpm_th_base(info, vindpm_th_base);
	if (ret) {
		dev_err(info->dev, "%s, failed to set vindpm th base %d, ret=%d\n",
			__func__, vindpm_th_base, ret);
		return ret;
	}

	if (vol_th < vindpm_th_base)
		vol_th = vindpm_th_base;
	else if (vol_th > vindpm_th_max)
		vol_th = vindpm_th_max;

	reg_val = (vol_th - vindpm_th_base) / SGM41516_VINDPM_LSB;

	return sgm41516_update_bits(info, SGM41516_REG_06,
				    SGM41516_VINDPM_MASK,
				    reg_val << SGM41516_VINDPM_SHIFT);
}

static int sgm41516_charger_set_termina_vol(struct sgm41516_charger_info *info, u32 vol)
{
	u8 reg_val;

	dev_dbg(info->dev, "%s:line%d: set termina vol = %d\n", __func__, __LINE__, vol);

	if (vol < SGM41516_VREG_BASE)
		vol = SGM41516_VREG_BASE;
	else if (vol >= SGM41516_VREG_MAX)
		vol = SGM41516_VREG_MAX;

	/*
	 * Reason for rounding up: Avoid insufficiency problem.
	 *
	 * Example: Assume that the battery charging limit voltage
	 *          is 4.43V. If it is rounded down, the actual
	 *          charging limit voltage will be 4.40V, and the
	 *          problem of insufficient charging will occur.
	 */
	reg_val = DIV_ROUND_UP(vol - SGM41516_VREG_BASE, SGM41516_VREG_LSB);

	return sgm41516_update_bits(info, SGM41516_REG_04,
				    SGM41516_VREG_MASK,
				    reg_val << SGM41516_VREG_SHIFT);
}

static int sgm41516_charger_set_safety_cur(struct sgm41516_charger_info *info, u32 cur)
{
	u8 reg_val;
	int i;

	dev_dbg(info->dev, "%s:line%d: set safety cur = %d\n", __func__, __LINE__, cur);

	cur = cur / 1000;
	for (i = 0; i < ARRAY_SIZE(sgm41516_iindpm); i++) {
		if (cur < sgm41516_iindpm[i])
			break;
	}

	if (i == 0)
		reg_val = 0;
	else
		reg_val = i - 1;

	return sgm41516_update_bits(info, SGM41516_REG_00,
				    SGM41516_IINLIM_MASK,
				    reg_val << SGM41516_IINLIM_SHIFT);
}

static int sgm41516_charger_enable_wdg(struct sgm41516_charger_info *info, bool enable)
{
	u8 val = SGM41516_WDT_DISABLE;

	if (enable)
		val = SGM41516_WDT_40S;

	return sgm41516_update_bits(info, SGM41516_REG_05,
				    SGM41516_WDT_MASK,
				    val << SGM41516_WDT_SHIFT);
}

static int sgm41516_charger_wdg_timer_rst(struct sgm41516_charger_info *info)
{
	return sgm41516_update_bits(info, SGM41516_REG_01,
				    SGM41516_WDT_RESET_MASK,
				    SGM41516_WDT_RESET << SGM41516_WDT_RESET_SHIFT);
}

static int sgm41516_charger_hw_init(struct sgm41516_charger_info *info)
{
	struct sprd_battery_info bat_info = {};
	int voltage_max_microvolt;
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

	voltage_max_microvolt = bat_info.constant_charge_voltage_max_uv / 1000;
	sprd_battery_put_battery_info(info->psy_usb, &bat_info);

	ret = sgm41516_charger_set_safety_cur(info, info->cur.dcp_cur);
	if (ret) {
		dev_err(info->dev, "set sgm41516 safety cur failed\n");
		return ret;
	}

	if (info->role ==  SGM41516_ROLE_MASTER) {
		ret = sgm41516_charger_set_acovp_threshold(info, SGM41516_FCHG_OVP_6V);
		if (ret)
			dev_err(info->dev, "%s, failed to set 6V bus ovp, ret = %d\n",
				__func__, ret);
	} else if (info->role == SGM41516_ROLE_SLAVE) {
		ret = sgm41516_charger_set_acovp_threshold(info, SGM41516_FCHG_OVP_9V);
		if (ret)
			dev_err(info->dev, "%s, failed to set 9V bus ovp, ret = %d\n",
				__func__, ret);
	}

	ret = sgm41516_charger_enable_term(info, true);
	if (ret) {
		dev_err(info->dev, "set sgm41516 terminal cur failed\n");
		return ret;
	}

	ret = sgm41516_charger_set_vindpm(info, voltage_max_microvolt);
	if (ret) {
		dev_err(info->dev, "set sgm41516 vindpm vol failed\n");
		return ret;
	}

	ret = sgm41516_charger_wdg_timer_rst(info);
	if (ret)
		dev_err(info->dev, "%s, failed to reset watchdog timer, ret = %d\n",
			__func__, ret);

	ret = sgm41516_charger_enable_wdg(info, false);
	if (ret) {
		dev_err(info->dev, "%s, failed to disable watchdog, ret = %d\n",
			__func__, ret);
		return ret;
	}

	ret = sgm41516_charger_set_termina_vol(info, voltage_max_microvolt);
	if (ret) {
		dev_err(info->dev, "set sgm41516 terminal vol failed\n");
		return ret;
	}

	ret = sgm41516_charger_set_limit_current(info,
						 info->cur.unknown_cur, false);
	if (ret)
		dev_err(info->dev, "set sgm41516 limit current failed\n");

	return ret;
}

static void sgm41516_charger_dump_register(struct sgm41516_charger_info *info)
{
	int ret;
	u8 addr;
	u8 val;
	u8 addr_start = SGM41516_REG_00, addr_end = SGM41516_REG_0F;

	for (addr = addr_start; addr <= addr_end; addr++) {
		ret = sgm41516_read(info, addr, &val);
		if (ret == 0)
			dev_info(info->dev, "dump reg %s,%d 0x%x = 0x%x\n",
				 __func__, __LINE__, addr, val);
	}
}

static int sgm41516_charger_start_charge(struct sgm41516_charger_info *info)
{
	int ret = 0;

	dev_info(info->dev, "%s:line%d: start charge\n", __func__, __LINE__);

	ret = sgm41516_charger_enable_power_path(info, true);
	if (ret)
		dev_err(info->dev, "%s, failed to enable power path, ret = %d\n", __func__, ret);

	ret = sgm41516_charger_enable_wdg(info, true);
	if (ret) {
		dev_err(info->dev, "%s, failed to enable watchdog, ret = %d\n", __func__, ret);
		return ret;
	}

	if (info->role == SGM41516_ROLE_MASTER) {
		ret = regmap_update_bits(info->pmic, info->charger_pd,
					 info->charger_pd_mask, 0);
		if (ret) {
			dev_err(info->dev, "enable sgm41516 charge failed\n");
			return ret;
		}
	} else if (info->role == SGM41516_ROLE_SLAVE) {
		gpiod_set_value_cansleep(info->gpiod, 0);
	}

	ret = sgm41516_charger_enable(info, true);
	if (ret) {
		dev_err(info->dev, "%s, failed to enable charge, ret = %d\n",
			__func__, ret);
		return ret;
	}

	sgm41516_charger_dump_register(info);

	return ret;
}

static void sgm41516_charger_stop_charge(struct sgm41516_charger_info *info)
{
	int ret;

	dev_info(info->dev, "%s:line%d: stop charge\n", __func__, __LINE__);

	ret = sgm41516_charger_enable(info, false);
	if (ret)
		dev_err(info->dev, "%s, failed to disable charge, ret = %d\n", __func__, ret);

	if (info->role == SGM41516_ROLE_MASTER) {
		ret = regmap_update_bits(info->pmic, info->charger_pd,
					 info->charger_pd_mask,
					 info->charger_pd_mask);
		if (ret)
			dev_err(info->dev, "disable sgm41516 charge failed\n");
	} else if (info->role == SGM41516_ROLE_SLAVE) {
		gpiod_set_value_cansleep(info->gpiod, 1);
	}

	ret = sgm41516_charger_enable_wdg(info, false);
	if (ret)
		dev_err(info->dev, "%s, failed to disable watchdog, ret = %d\n", __func__, ret);

	if (info->disable_power_path) {
		ret = sgm41516_charger_enable_power_path(info, false);
		if (ret)
			dev_err(info->dev, "%s, failed to disable power path, ret = %d\n",
				__func__, ret);
	}
}

static int sgm41516_charger_set_current(struct sgm41516_charger_info *info, u32 cur)
{
	u8 reg_val;

	cur = cur / 1000;
	if (cur < SGM41516_ICHG_BASE)
		cur = SGM41516_ICHG_BASE;
	else if (cur > SGM41516_ICHG_MAX)
		cur = SGM41516_ICHG_MAX;

	reg_val = (cur - SGM41516_ICHG_BASE) / SGM41516_ICHG_LSB;
	reg_val <<= SGM41516_ICHG_SHIFT;

	return sgm41516_update_bits(info, SGM41516_REG_02,
				    SGM41516_ICHG_MASK,
				    reg_val);
}

static int sgm41516_charger_get_current(struct sgm41516_charger_info *info, u32 *cur)
{
	u8 reg_val;
	int ret;

	ret = sgm41516_read(info, SGM41516_REG_02, &reg_val);
	if (ret < 0) {
		dev_err(info->dev, "%s, failed to read reg[%02X], ret = %d\n",
			__func__, SGM41516_REG_02, ret);
		return ret;
	}

	reg_val &= SGM41516_ICHG_MASK;
	reg_val = reg_val >> SGM41516_ICHG_SHIFT;
	*cur = ((reg_val * SGM41516_ICHG_LSB) + SGM41516_ICHG_BASE) * 1000;
	return 0;
}

static int sgm41516_charger_enable_otg_vbus(struct sgm41516_charger_info *info, bool enable)
{
	u8 val = SGM41516_OTG_DISABLE;

	if (enable)
		val = SGM41516_OTG_ENABLE;

	return sgm41516_update_bits(info, SGM41516_REG_01,
				    SGM41516_OTG_CONFIG_MASK,
				    val << SGM41516_OTG_CONFIG_SHIFT);
}

static bool sgm41516_charger_otg_vbus_is_enabled(struct sgm41516_charger_info *info)
{
	bool otg_vbus_is_enabled = false;
	u8 reg_val = 0;
	int ret = 0;

	ret = sgm41516_read(info, SGM41516_REG_01, &reg_val);
	if (ret) {
		dev_err(info->dev, "%s, failed to read reg[%02X], ret = %d\n",
			__func__, SGM41516_REG_01, ret);
		return otg_vbus_is_enabled;
	}

	if (reg_val & SGM41516_OTG_CONFIG_MASK)
		otg_vbus_is_enabled = true;

	return otg_vbus_is_enabled;
}

static int sgm41516_charger_get_health(struct sgm41516_charger_info *info, u32 *health)
{
	*health = POWER_SUPPLY_HEALTH_GOOD;

	return 0;
}

static int sgm41516_charger_feed_watchdog(struct sgm41516_charger_info *info)
{
	int ret = 0;
	u64 duration, curr = ktime_to_ms(ktime_get());

	ret = sgm41516_charger_wdg_timer_rst(info);
	if (ret) {
		dev_err(info->dev, "%s, failed to reset watchdog timer, ret = %d\n",
			__func__, ret);
		return ret;
	}

	duration = curr - info->last_wdt_time;
	if (duration >= SGM41516_WATCH_DOG_TIME_OUT_MS) {
		dev_err(info->dev, "charger wdg maybe time out:%lld ms\n", duration);
		sgm41516_charger_dump_register(info);
	}

	info->last_wdt_time = curr;
	if (info->otg_enable)
		return ret;

	ret = sgm41516_charger_set_limit_current(info, info->actual_limit_cur, true);
	if (ret)
		dev_err(info->dev, "set limit cur failed\n");

	return ret;
}

static int sgm41516_charger_get_status(struct sgm41516_charger_info *info)
{
	if (info->charging)
		return POWER_SUPPLY_STATUS_CHARGING;
	else
		return POWER_SUPPLY_STATUS_NOT_CHARGING;
}

static int sgm41516_charger_set_extend_status(struct sgm41516_charger_info *info, int intval)
{
	int ret = 0;

	switch (intval) {
	case CM_FAST_CHARGE_OVP_ENABLE_CMD:
		ret = sgm41516_charger_set_acovp_threshold(info, SGM41516_FCHG_OVP_9V);
		if (ret)
			dev_err(info->dev, "%s, failed to set 9V bus ovp, ret = %d\n",
				__func__, ret);
		break;
	case CM_FAST_CHARGE_MAX_OVP_ENABLE_CMD:
		ret = sgm41516_charger_set_acovp_threshold(info, SGM41516_FCHG_OVP_14V);
		if (ret)
			dev_err(info->dev, "%s, failed to set fast charge max ovp, ret = %d\n",
				__func__, ret);
		break;
	case CM_FAST_CHARGE_OVP_DISABLE_CMD:
		ret = sgm41516_charger_set_acovp_threshold(info, SGM41516_FCHG_OVP_6V);
		if (ret)
			dev_err(info->dev, "%s, failed to set 6V bus ovp, ret = %d\n",
				__func__, ret);
		break;
	case CM_POWER_PATH_ENABLE_CMD:
		ret = sgm41516_charger_enable_power_path(info, true);
		if (ret)
			dev_err(info->dev, "%s, failed to enable power path, ret = %d\n",
				__func__, ret);
		break;
	case CM_POWER_PATH_DISABLE_CMD:
		ret = sgm41516_charger_enable_power_path(info, false);
		if (ret)
			dev_err(info->dev, "%s, failed to disable power path, ret = %d\n",
				__func__, ret);
		break;
	case CM_BUCK_MAX_TERMINA_VOL:
		ret = sgm41516_charger_set_termina_vol(info, SGM41516_VREG_MAX);
		if (ret)
			dev_err(info->dev, "%s, failed to set terminate max voltage, ret = %d\n",
				__func__, ret);
		break;
	default:
		ret = 0;
	}

	return ret;
}

static int sgm41516_charger_set_status(struct sgm41516_charger_info *info, int val)
{
	int ret = 0;

	if (!val && info->charging) {
		sgm41516_charger_stop_charge(info);
		info->charging = false;
	} else if (val && !info->charging) {
		ret = sgm41516_charger_start_charge(info);
		if (ret)
			dev_err(info->dev, "%s, failed to start charge, ret = %d\n", __func__, ret);
		else
			info->charging = true;
	}

	return ret;
}

static bool sgm41516_charger_probe_is_ready(struct sgm41516_charger_info *info)
{
	unsigned long timeout;

	if (unlikely(!info->probe_initialized)) {
		timeout = wait_for_completion_timeout(&info->probe_init, SGM41516_PROBE_TIMEOUT);
		if (!timeout) {
			dev_err(info->dev, "%s wait probe timeout\n", __func__);
			return false;
		}
	}

	return true;
}

static int sgm41516_charger_check_power_path_status(struct sgm41516_charger_info *info)
{
	int ret = 0;

	if (info->disable_power_path)
		return 0;

	if (sgm41516_charger_power_path_is_enabled(info))
		return 0;

	dev_info(info->dev, "%s, enable power path\n", __func__);
	ret = sgm41516_charger_enable_power_path(info, true);
	if (ret)
		dev_err(info->dev, "%s, failed to enable power path, ret = %d\n",
			__func__, ret);

	return ret;
}

static int sgm41516_charger_get_input_current_limit_psp(struct sgm41516_charger_info *info,
							int *intval)
{
	int ret = 0;

	if (!info->charging) {
		*intval = 0;
		return 0;
	}

	ret = sgm41516_charger_get_limit_current(info, intval);
	if (ret)
		dev_err(info->dev, "%s, failed to get limit current, ret = %d\n",
			__func__, ret);

	return ret;
}

static int sgm41516_charger_set_input_current_limit_psp(struct sgm41516_charger_info *info,
							int intval)
{
	return sgm41516_charger_set_limit_current(info, intval, false);
}

static int sgm41516_charger_get_constant_charge_current_psp(struct sgm41516_charger_info *info,
							    int *intval)
{
	int ret = 0;

	if (!info->charging) {
		*intval = 0;
		return 0;
	}

	ret = sgm41516_charger_get_current(info, intval);
	if (ret)
		dev_err(info->dev, "%s, failed to get current, ret = %d\n",
			__func__, ret);

	return ret;
}

static int sgm41516_charger_set_constant_charge_current_psp(struct sgm41516_charger_info *info,
							    int intval)
{
	return sgm41516_charger_set_current(info, intval);
}

static int sgm41516_charger_get_status_psp(struct sgm41516_charger_info *info, int *intval)
{
	int status = -EINVAL;

	switch (*intval) {
	case CM_POWER_PATH_ENABLE_CMD:
	case CM_POWER_PATH_DISABLE_CMD:
		status = sgm41516_charger_power_path_is_enabled(info);
		break;
	case CM_BUCK_MAX_TERMINA_VOL:
		status = SGM41516_VREG_MAX * 1000;
		break;
	default:
		status = sgm41516_charger_get_status(info);
	}

	*intval = status;
	return 0;
}

static int sgm41516_charger_set_status_psp(struct sgm41516_charger_info *info, int intval)
{
	int ret = 0;

	if (intval > CM_FAST_CHARGE_NORMAL_CMD)
		ret = sgm41516_charger_set_extend_status(info, intval);
	else
		ret = sgm41516_charger_set_status(info, intval);

	if (ret)
		dev_err(info->dev, "%s, failed to set%s status, ret = %d\n",
			__func__, intval > CM_FAST_CHARGE_NORMAL_CMD ? " extend" : "", ret);

	return ret;
}

static int sgm41516_charger_get_health_psp(struct sgm41516_charger_info *info, int *intval)
{
	int ret = 0;

	if (info->charging) {
		*intval = POWER_SUPPLY_HEALTH_UNKNOWN;
		return 0;
	}

	ret = sgm41516_charger_get_health(info, intval);
	if (ret)
		dev_err(info->dev, "%s, failed to get health, ret = %d\n",
			__func__, ret);

	return ret;
}

static int sgm41516_charger_set_constant_charge_voltage_max_psp(struct sgm41516_charger_info *info,
								int intval)
{
	return sgm41516_charger_set_termina_vol(info, intval / 1000);
}

static int sgm41516_charger_get_calibrate_psp(struct sgm41516_charger_info *info, int *intval)
{
	int ret = 0, status = 0;
	bool enabled = false;

	if (info->role == SGM41516_ROLE_MASTER) {
		ret = regmap_read(info->pmic, info->charger_pd, &status);
		if (ret) {
			dev_err(info->dev, "%s, failed to read chg_pd pin status, ret = %d\n",
				__func__, ret);
			return ret;
		}
	} else if (info->role == SGM41516_ROLE_SLAVE) {
		status = gpiod_get_value_cansleep(info->gpiod);
	}

	enabled = !status;
	*intval = enabled;

	return ret;
}

static int sgm41516_charger_set_calibrate_psp(struct sgm41516_charger_info *info, int intval)
{
	int ret = 0;

	dev_info(info->dev, "%s, intval = %d\n", __func__, intval);
	if (intval == true) {
		ret = sgm41516_charger_start_charge(info);
		if (ret)
			dev_err(info->dev, "start charge failed\n");
	} else if (intval == false) {
		sgm41516_charger_stop_charge(info);
	}

	return ret;
}

static int sgm41516_charger_set_present_psp(struct sgm41516_charger_info *info, int intval)
{
	info->is_charger_online = !!intval;
	if (info->is_charger_online) {
		info->last_wdt_time = ktime_to_ms(ktime_get());
		schedule_delayed_work(&info->wdt_work, 0);
	} else {
		info->actual_limit_cur = 0;
		cancel_delayed_work_sync(&info->wdt_work);
	}

	return 0;
}

static int sgm41516_charger_usb_get_property(struct power_supply *psy,
					     enum power_supply_property psp,
					     union power_supply_propval *val)
{
	struct sgm41516_charger_info *info = power_supply_get_drvdata(psy);
	int ret = 0;

	if (!info) {
		pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
		return -EINVAL;
	}

	if (unlikely(!psy->initialized && atomic_read(&psy->use_cnt) > 0)) {
		dev_err(info->dev, "%s psy is not ready\n", __func__);
		return -ENODEV;
	}

	if (!sgm41516_charger_probe_is_ready(info)) {
		dev_err(info->dev, "%s wait probe timeout\n", __func__);
		return -EINVAL;
	}

	mutex_lock(&info->lock);
	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
		ret = sgm41516_charger_get_status_psp(info, &val->intval);
		break;
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT:
		ret = sgm41516_charger_get_constant_charge_current_psp(info, &val->intval);
		break;
	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
		ret = sgm41516_charger_get_input_current_limit_psp(info, &val->intval);
		break;
	case POWER_SUPPLY_PROP_HEALTH:
		ret = sgm41516_charger_get_health_psp(info, &val->intval);
		break;
	case POWER_SUPPLY_PROP_CALIBRATE:
		ret = sgm41516_charger_get_calibrate_psp(info, &val->intval);
		break;
	default:
		ret = -EINVAL;
	}

	mutex_unlock(&info->lock);
	if (ret)
		dev_err(info->dev, "failed to get psp: %d, ret = %d\n", psp, ret);

	return ret;
}

static int sgm41516_charger_usb_set_property(struct power_supply *psy,
					     enum power_supply_property psp,
					     const union power_supply_propval *val)
{
	struct sgm41516_charger_info *info = power_supply_get_drvdata(psy);
	int ret = 0;

	if (!info) {
		pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
		return -EINVAL;
	}

	if (unlikely(!psy->initialized && atomic_read(&psy->use_cnt) > 0)) {
		dev_err(info->dev, "%s psy is not ready\n", __func__);
		return -ENODEV;
	}

	if (!sgm41516_charger_probe_is_ready(info)) {
		dev_err(info->dev, "%s wait probe timeout\n", __func__);
		return -EINVAL;
	}

	mutex_lock(&info->lock);

	switch (psp) {
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT:
		ret = sgm41516_charger_set_constant_charge_current_psp(info, val->intval);
		break;
	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
		ret = sgm41516_charger_set_input_current_limit_psp(info, val->intval);
		break;
	case POWER_SUPPLY_PROP_STATUS:
		ret = sgm41516_charger_set_status_psp(info, val->intval);
		break;
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE_MAX:
		ret = sgm41516_charger_set_constant_charge_voltage_max_psp(info, val->intval);
		break;
	case POWER_SUPPLY_PROP_CALIBRATE:
		ret = sgm41516_charger_set_calibrate_psp(info, val->intval);
		break;
	case POWER_SUPPLY_PROP_PRESENT:
		ret = sgm41516_charger_set_present_psp(info, val->intval);
		break;
	default:
		ret = -EINVAL;
	}

	mutex_unlock(&info->lock);
	if (ret)
		dev_err(info->dev, "failed to set psp: %d, intval: %d, ret = %d\n",
			psp, val->intval, ret);

	return ret;
}

static int sgm41516_charger_property_is_writeable(struct power_supply *psy,
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

static enum power_supply_property sgm41516_usb_props[] = {
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT,
	POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT,
	POWER_SUPPLY_PROP_HEALTH,
	POWER_SUPPLY_PROP_CALIBRATE,
};

static void sgm41516_charger_feed_watchdog_work(struct work_struct *work)
{
	struct delayed_work *dwork = to_delayed_work(work);
	struct sgm41516_charger_info *info = container_of(dwork,
							  struct sgm41516_charger_info,
							  wdt_work);
	int ret;

	if (!info) {
		pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
		return;
	}

	ret = sgm41516_charger_feed_watchdog(info);
	if (ret)
		schedule_delayed_work(&info->wdt_work, HZ * 1);
	else
		schedule_delayed_work(&info->wdt_work, HZ * 15);
}

#if IS_ENABLED(CONFIG_REGULATOR)
static bool sgm41516_charger_check_otg_fault(struct sgm41516_charger_info *info)
{
	int ret;
	u8 value = 0;
	bool status = true;

	ret = sgm41516_read(info, SGM41516_REG_09, &value);
	if (ret) {
		dev_err(info->dev, "%s, failed to read reg[%02X], ret = %d\n",
			__func__, SGM41516_REG_09, ret);
		return status;
	}

	if (!(value & SGM41516_FAULT_BOOST_MASK))
		status = false;
	else
		dev_err(info->dev, "boost fault occurs, REG_9 = 0x%x\n", value);

	return status;
}

static void sgm41516_charger_otg_work(struct work_struct *work)
{
	struct delayed_work *dwork = to_delayed_work(work);
	struct sgm41516_charger_info *info = container_of(dwork,
							  struct sgm41516_charger_info, otg_work);
	bool otg_valid;
	bool otg_fault;
	int ret, retry = 0;

	if (!info) {
		pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
		return;
	}

	otg_valid = sgm41516_charger_otg_vbus_is_enabled(info);
	if (otg_valid)
		goto out;

	do {
		otg_fault = sgm41516_charger_check_otg_fault(info);
		if (!otg_fault) {
			ret = sgm41516_charger_enable_otg_vbus(info, true);
			if (ret)
				dev_err(info->dev, "%s, failed to enable otg vbus, ret = %d\n",
					__func__, ret);
		}

		otg_valid = sgm41516_charger_otg_vbus_is_enabled(info);
	} while (!otg_valid && retry++ < SGM41516_OTG_RETRY_TIMES);

	if (retry >= SGM41516_OTG_RETRY_TIMES) {
		dev_err(info->dev, "Restart OTG failed\n");
		return;
	}

out:
	schedule_delayed_work(&info->otg_work, msecs_to_jiffies(1500));
}

static int sgm41516_charger_enable_otg(struct regulator_dev *dev)
{
	struct sgm41516_charger_info *info = rdev_get_drvdata(dev);
	int ret = 0;

	if (!info) {
		pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
		return -EINVAL;
	}

	if (info->shutdown_flag)
		return ret;

	sgm41516_charger_dump_stack();

	if (!sgm41516_charger_probe_is_ready(info)) {
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

	ret = sgm41516_charger_enable_otg_vbus(info, true);
	if (ret) {
		dev_err(info->dev, "%s, failed to enable otg vbus, ret = %d\n",
			__func__, ret);
		regmap_update_bits(info->pmic, info->charger_detect, BIT_DP_DM_BC_ENB, 0);
		return ret;
	}

	ret = sgm41516_charger_enable_wdg(info, true);
	if (ret) {
		dev_err(info->dev, "%s, failed to enable watchdog, ret = %d\n", __func__, ret);
		return ret;
	}

	ret = sgm41516_charger_feed_watchdog(info);
	if (ret) {
		dev_err(info->dev, "%s, failed to feed watchdog, ret = %d\n", __func__, ret);
		return ret;
	}

	ret = sgm41516_charger_enable_power_path(info, true);
	if (ret)
		dev_err(info->dev, "%s, failed to enable power path, ret = %d\n", __func__, ret);

	info->otg_enable = true;
	info->last_wdt_time = ktime_to_ms(ktime_get());
	schedule_delayed_work(&info->wdt_work,
			      msecs_to_jiffies(SGM41516_FEED_WATCHDOG_VALID_MS));
	schedule_delayed_work(&info->otg_work,
			      msecs_to_jiffies(SGM41516_OTG_VALID_MS));
	dev_info(info->dev, "%s:line%d:enable_otg\n", __func__, __LINE__);

	return ret;
}

static int sgm41516_charger_disable_otg(struct regulator_dev *dev)
{
	struct sgm41516_charger_info *info = rdev_get_drvdata(dev);
	int ret = 0;

	if (!info) {
		pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
		return -EINVAL;
	}

	sgm41516_charger_dump_stack();
	if (!sgm41516_charger_probe_is_ready(info)) {
		dev_err(info->dev, "%s wait probe timeout\n", __func__);
		return -EINVAL;
	}

	info->otg_enable = false;
	cancel_delayed_work_sync(&info->wdt_work);
	cancel_delayed_work_sync(&info->otg_work);

	ret = sgm41516_charger_enable_otg_vbus(info, false);
	if (ret) {
		dev_err(info->dev, "%s, failed to disable otg vbus, ret = %d\n",
			__func__, ret);
		return ret;
	}

	ret = sgm41516_charger_enable_wdg(info, false);
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

static int sgm41516_charger_vbus_is_enabled(struct regulator_dev *dev)
{
	struct sgm41516_charger_info *info = rdev_get_drvdata(dev);

	if (!info) {
		pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
		return -EINVAL;
	}

	return sgm41516_charger_otg_vbus_is_enabled(info);
}

static const struct regulator_ops sgm41516_charger_vbus_ops = {
	.enable = sgm41516_charger_enable_otg,
	.disable = sgm41516_charger_disable_otg,
	.is_enabled = sgm41516_charger_vbus_is_enabled,
};

static const struct regulator_desc sgm41516_charger_vbus_desc = {
	.name = "otg-vbus",
	.of_match = "otg-vbus",
	.type = REGULATOR_VOLTAGE,
	.owner = THIS_MODULE,
	.ops = &sgm41516_charger_vbus_ops,
	.fixed_uV = 5000000,
	.n_voltages = 1,
};

static void sgm41516_charger_check_otg_status(struct sgm41516_charger_info *info)
{
	int ret;
	u8 val;

	ret = sgm41516_read(info, SGM41516_REG_01, &val);
	if (ret < 0) {
		dev_err(info->dev, "%s:line%d, failed to get reg1(%d)\n", __func__, __LINE__, ret);
		return;
	}

	if (val & SGM41516_OTG_CONFIG_MASK) {
		dev_info(info->dev, "%s:line%d, exit otg mode\n", __func__, __LINE__);
		ret = sgm41516_update_bits(info, SGM41516_REG_01, SGM41516_OTG_CONFIG_MASK,
					   SGM41516_OTG_DISABLE << SGM41516_OTG_CONFIG_SHIFT);
		if (ret)
			dev_err(info->dev, "disable sgm41516 otg failed\n");
	}
}

static int sgm41516_charger_register_vbus_regulator(struct sgm41516_charger_info *info)
{
	struct regulator_config cfg = { };
	struct regulator_dev *reg;
	int ret = 0;

	/*
	 * only master to support otg
	 */
	if (info->role != SGM41516_ROLE_MASTER)
		return 0;

	sgm41516_charger_check_otg_status(info);

	cfg.dev = info->dev;
	cfg.driver_data = info;
	reg = devm_regulator_register(info->dev, &sgm41516_charger_vbus_desc, &cfg);
	if (IS_ERR(reg)) {
		ret = PTR_ERR(reg);
		dev_err(info->dev, "%s, failed to register regulator, ret = %d\n",
			__func__, ret);
	}

	return ret;
}

static int sgm41516_charger_register_external_vbus_regulator(struct sgm41516_charger_info *info)
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
	if (info->role != SGM41516_ROLE_MASTER)
		return 0;

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
	reg = devm_regulator_register(cfg.dev, &sgm41516_charger_vbus_desc, &cfg);
	if (IS_ERR(reg)) {
		ret = PTR_ERR(reg);
		dev_warn(info->dev, "%s, failed to register vddvbus regulator:%d\n",
			 __func__, ret);
	}

	return ret;
}

#else
static int sgm41516_charger_register_vbus_regulator(struct sgm41516_charger_info *info)
{
	return 0;
}

static int sgm41516_charger_register_external_vbus_regulator(struct sgm41516_charger_info *info)
{
	return 0;
}
#endif

static const struct of_device_id sgm41516_charger_of_match_table[] = {
	{
		.compatible = "Sgm,sgm41516_chg",
		.data = &sgm41516_mode_data[SGM41516_MASTER],
	},

	{
		.compatible = "Sgm,sgm41516_slave_chg",
		.data = &sgm41516_mode_data[SGM41516_SLAVE],
	},
	{},
};

static int sgm41516_charger_match_role(struct sgm41516_charger_info *info)
{
	struct device *dev = info->dev;
	const struct of_device_id *match;

	match = of_match_node(sgm41516_charger_of_match_table, dev->of_node);
	if (match == NULL) {
		dev_err(dev, "%s, device tree match not found!\n", __func__);
		return -ENODEV;
	}

	info->role = *(int *)match->data;
	dev_info(dev, "%s, match role: %s\n",
		 __func__, info->role == SGM41516_ROLE_MASTER ? "Master" : "Slave");

	return 0;
}

static int sgm41516_charger_slave_adapt_enable_pin_cfg(struct sgm41516_charger_info *info)
{
	if (info->role != SGM41516_ROLE_SLAVE)
		return 0;

	info->gpiod = devm_gpiod_get(info->dev, "enable", GPIOD_OUT_HIGH);
	if (IS_ERR(info->gpiod)) {
		dev_err(info->dev, "%s, failed to get enable gpio\n", __func__);
		return PTR_ERR(info->gpiod);
	}

	return 0;
}

static int sgm41516_charger_master_adapt_enable_pin_cfg(struct sgm41516_charger_info *info)
{
	struct device_node *regmap_np;
	struct platform_device *regmap_pdev;
	int ret = 0;

	if (info->role != SGM41516_ROLE_MASTER)
		return 0;

	regmap_np = of_find_compatible_node(NULL, NULL, "sprd,sc27xx-syscon");
	if (!regmap_np)
		regmap_np = of_find_compatible_node(NULL, NULL, "sprd,ump962x-syscon");

	if (regmap_np) {
		if (of_device_is_compatible(regmap_np->parent, "sprd,sc2730")) {
			info->charger_pd_mask = SGM41516_DISABLE_PIN_MASK_2730;
		} else if (of_device_is_compatible(regmap_np->parent, "sprd,sc2721")) {
			info->charger_pd_mask = SGM41516_DISABLE_PIN_MASK_2721;
		} else if (of_device_is_compatible(regmap_np->parent, "sprd,sc2720")) {
			info->charger_pd_mask = SGM41516_DISABLE_PIN_MASK_2720;
		} else if (of_device_is_compatible(regmap_np, "sprd,ump962x-syscon")) {
			info->charger_pd_mask = SGM41516_DISABLE_PIN_MASK;
		} else {
			dev_err(info->dev, "%s, failed to get charger_pd mask\n", __func__);
			return -EINVAL;
		}
	} else {
		dev_err(info->dev, "%s, unable to get syscon node\n", __func__);
		return -ENODEV;
	}

	ret = of_property_read_u32_index(regmap_np, "reg", 1,
					 &info->charger_detect);
	if (ret) {
		dev_err(info->dev, "%s, failed to get charger_detect, ret = %d\n", __func__, ret);
		return -EINVAL;
	}

	ret = of_property_read_u32_index(regmap_np, "reg", 2,
					 &info->charger_pd);
	if (ret) {
		dev_err(info->dev, "%s, failed to get charger_pd reg, ret = %d\n", __func__, ret);
		return -EINVAL;
	}

	regmap_pdev = of_find_device_by_node(regmap_np);
	if (!regmap_pdev) {
		of_node_put(regmap_np);
		dev_err(info->dev, "%s, unable to get syscon device\n", __func__);
		return -ENODEV;
	}

	of_node_put(regmap_np);
	info->pmic = dev_get_regmap(regmap_pdev->dev.parent, NULL);
	if (!info->pmic) {
		dev_err(info->dev, "%s, unable to get pmic regmap device\n", __func__);
		return -ENODEV;
	}

	return 0;
}

static int sgm41516_charger_psy_register(struct sgm41516_charger_info *info)
{
	info->psy_cfg.drv_data = info;
	info->psy_cfg.of_node = info->dev->of_node;

	if (info->role == SGM41516_ROLE_SLAVE)
		info->psy_desc.name = "sgm41516_slave_charger";
	else
		info->psy_desc.name = "sgm41516_charger";

	info->psy_desc.type = POWER_SUPPLY_TYPE_UNKNOWN;
	info->psy_desc.properties = sgm41516_usb_props;
	info->psy_desc.num_properties = ARRAY_SIZE(sgm41516_usb_props);
	info->psy_desc.get_property = sgm41516_charger_usb_get_property;
	info->psy_desc.set_property = sgm41516_charger_usb_set_property;
	info->psy_desc.property_is_writeable = sgm41516_charger_property_is_writeable;

	info->psy_usb = devm_power_supply_register(info->dev, &info->psy_desc, &info->psy_cfg);
	if (IS_ERR(info->psy_usb)) {
		dev_err(info->dev, "%s, failed to register power supply\n", __func__);
		return PTR_ERR(info->psy_usb);
	}

	dev_info(info->dev, "%s, %s power supply register successfully\n",
		 __func__, info->psy_desc.name);

	return 0;
}

static int sgm41516_charger_detect_device(struct sgm41516_charger_info *info)
{
	int ret, part_id;
	u8 reg_val;

	ret = sgm41516_read(info, SGM41516_REG_0B, &reg_val);
	if (ret < 0) {
		dev_err(info->dev, "%s, failed to get device id, ret = %d\n", __func__, ret);
		return ret;
	}

	part_id = (reg_val & SGM41516_PN_MASK) >> SGM41516_PN_SHIFT;
	if (part_id != SGM41516_DEV_ID && part_id != SGM41516D_DEV_ID) {
		dev_err(info->dev, "%s, the device id is 0x%x\n", __func__, part_id);
		return -EINVAL;
	}

	return ret;
}

static int sgm41516_charger_probe(struct i2c_client *client,
				  const struct i2c_device_id *id)
{
	struct i2c_adapter *adapter = to_i2c_adapter(client->dev.parent);
	struct device *dev = &client->dev;
	struct sgm41516_charger_info *info;
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

	ret = sgm41516_charger_detect_device(info);
	if (ret) {
		dev_err(dev, "%s, failed to detect device, ret = %d\n", __func__, ret);
		return -ENODEV;
	}

	sgm41516_charger_power_path_control(info);

	info->use_typec_extcon = device_property_read_bool(dev, "use-typec-extcon");
	info->disable_wdg = device_property_read_bool(dev, "disable-otg-wdg-in-sleep");

	ret = sgm41516_charger_match_role(info);
	if (ret) {
		dev_err(dev, "%s, failed to get work mode, ret = %d\n", __func__, ret);
		return ret;
	}

	ret = sgm41516_charger_slave_adapt_enable_pin_cfg(info);
	if (ret) {
		dev_err(dev, "%s, failed to configure the adptive enable pin in slave, ret = %d\n",
			__func__, ret);
		return ret;
	}

	ret = sgm41516_charger_master_adapt_enable_pin_cfg(info);
	if (ret) {
		dev_err(dev, "%s, failed to configure the adptive enable pin in master, ret = %d\n",
			__func__, ret);
		return ret;
	}

	mutex_init(&info->lock);
	mutex_init(&info->input_limit_cur_lock);
	init_completion(&info->probe_init);

	ret = sgm41516_charger_psy_register(info);
	if (ret) {
		dev_err(dev, "%s, failed to register power supply, ret = %d\n", __func__, ret);
		goto out;
	}

	ret = sgm41516_charger_hw_init(info);
	if (ret)
		goto out;

	sgm41516_charger_stop_charge(info);
	sgm41516_charger_check_power_path_status(info);

	device_init_wakeup(info->dev, true);

	alarm_init(&info->otg_timer, ALARM_BOOTTIME, NULL);
	INIT_DELAYED_WORK(&info->otg_work, sgm41516_charger_otg_work);
	INIT_DELAYED_WORK(&info->wdt_work, sgm41516_charger_feed_watchdog_work);

	if (device_property_read_bool(dev, "otg-vbus-node-external"))
		ret = sgm41516_charger_register_external_vbus_regulator(info);
	else
		ret = sgm41516_charger_register_vbus_regulator(info);

	if (ret) {
		dev_err(dev, "failed to register vbus regulator.\n");
		goto out;
	}

	info->probe_initialized = true;
	complete_all(&info->probe_init);

	sgm41516_charger_dump_register(info);
	dev_info(dev, "use_typec_extcon = %d\n", info->use_typec_extcon);

	return 0;

out:
	mutex_destroy(&info->input_limit_cur_lock);
	mutex_destroy(&info->lock);
	return ret;
}

static void sgm41516_charger_shutdown(struct i2c_client *client)
{
	struct sgm41516_charger_info *info = i2c_get_clientdata(client);
	int ret = 0;

	if (!info) {
		pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
		return;
	}

	cancel_delayed_work_sync(&info->wdt_work);
	if (info->otg_enable) {
		info->otg_enable = false;
		cancel_delayed_work_sync(&info->otg_work);
		ret = sgm41516_charger_enable_otg_vbus(info, false);
		if (ret)
			dev_err(info->dev, "%s, failed to disable otg vbus, ret = %d\n",
				__func__, ret);

		ret = sgm41516_charger_enable_power_path(info, false);
		if (ret)
			dev_err(info->dev, "%s, failed to disable power path, ret = %d\n",
				__func__, ret);

		/* Enable charger detection function to identify the charger type */
		ret = regmap_update_bits(info->pmic, info->charger_detect,
					 BIT_DP_DM_BC_ENB, 0);
		if (ret)
			dev_err(info->dev,
				"enable charger detection function failed ret = %d\n", ret);
	}

	info->shutdown_flag = true;
	if (info->charging) {
		ret = sgm41516_charger_set_limit_current(info, SGM41516_SHUTDOWN_LIMIT, false);
		if (ret < 0)
			dev_err(info->dev, "%s: set input limit cur failed\n", __func__);

		ret = sgm41516_charger_set_current(info, SGM41516_SHUTDOWN_CURRENT);
		if (ret < 0)
			dev_err(info->dev, "%s:set charge current failed\n", __func__);
	}
}

static int sgm41516_charger_remove(struct i2c_client *client)
{
	struct sgm41516_charger_info *info = i2c_get_clientdata(client);

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
static int sgm41516_charger_suspend(struct device *dev)
{
	struct sgm41516_charger_info *info = dev_get_drvdata(dev);
	ktime_t now, add;

	if (!info) {
		pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
		return -EINVAL;
	}

	if (info->otg_enable || info->is_charger_online) {
		if (sgm41516_charger_feed_watchdog(info))
			dev_err(info->dev, "%s, failed to feed watchdog\n", __func__);

		cancel_delayed_work_sync(&info->wdt_work);
	}

	if (!info->otg_enable)
		return 0;

	if (info->disable_wdg) {
		if (sgm41516_charger_enable_wdg(info, false)) {
			dev_err(info->dev, "%s, failed to disable watchdog\n", __func__);
			return -EBUSY;
		}
	} else {
		dev_dbg(info->dev, "%s:line%d: set alarm\n", __func__, __LINE__);
		now = ktime_get_boottime();
		add = ktime_set(SGM41516_OTG_ALARM_TIMER_S, 0);
		alarm_start(&info->otg_timer, ktime_add(now, add));
	}

	return 0;
}

static int sgm41516_charger_resume(struct device *dev)
{
	struct sgm41516_charger_info *info = dev_get_drvdata(dev);

	if (!info) {
		pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
		return -EINVAL;
	}

	if (info->otg_enable || info->is_charger_online) {
		if (sgm41516_charger_feed_watchdog(info))
			dev_err(info->dev, "%s, failed to feed watchdog\n", __func__);

		schedule_delayed_work(&info->wdt_work, HZ * 15);
	}

	if (!info->otg_enable)
		return 0;

	if (info->disable_wdg) {
		if (sgm41516_charger_enable_wdg(info, true)) {
			dev_err(info->dev, "%s, failed to enable watchdog\n", __func__);
			return -EBUSY;
		}
	} else {
		alarm_cancel(&info->otg_timer);
	}

	return 0;
}
#endif

static const struct dev_pm_ops sgm41516_charger_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(sgm41516_charger_suspend,
				sgm41516_charger_resume)
};

static const struct i2c_device_id sgm41516_i2c_id[] = {
	{"sgm41516_chg", 0},
	{"sgm41516_slave_chg", 0},
	{}
};

static struct i2c_driver sgm41516_charger_driver = {
	.driver = {
		.name = "sgm41516_chg",
		.of_match_table = sgm41516_charger_of_match_table,
		.pm = &sgm41516_charger_pm_ops,
	},
	.probe = sgm41516_charger_probe,
	.shutdown = sgm41516_charger_shutdown,
	.remove = sgm41516_charger_remove,
	.id_table = sgm41516_i2c_id,
};

module_i2c_driver(sgm41516_charger_driver);
MODULE_DESCRIPTION("SGM41516 Charger Driver");
MODULE_LICENSE("GPL v2");
