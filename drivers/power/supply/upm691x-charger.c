// SPDX-License-Identifier: GPL-2.0-only
/*
 * Driver for the Unisemipower upm691x charger.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

//This file has been modified by Unisoc (Shanghai) Technologies Co., Ltd in 2023.

#include <linux/alarmtimer.h>
#include <linux/gpio/consumer.h>
#include <linux/interrupt.h>
#include <linux/i2c.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
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
#include <linux/sysfs.h>
#include <linux/power/upm691x_reg.h>
#include "lc_charger_class.h"

#define UPM691X_BATTERY_NAME			"sc27xx-fgu"
#define BIT_DP_DM_BC_ENB			BIT(0)
#define UPM691X_OTG_VALID_MS			500
#define UPM691X_FEED_WATCHDOG_VALID_MS		50
#define UPM691X_OTG_ALARM_TIMER_S		15
//#define UPM6910_I2C_ADDR                0x6B

#define UPM691X_REG_HZ_MODE_MASK		GENMASK(1, 1)
#define UPM691X_REG_OPA_MODE_MASK		GENMASK(0, 0)
#define UPM691X_REG_OTG_MASK			GENMASK(5, 5)
#define UPM691X_REG_RESET_MASK			GENMASK(6, 6)
#define UPM691X_REG_BOOST_FAULT_MASK		GENMASK(6, 6)

#define UPM691X_REG_BOOST_LIMIT_MASK		GENMASK(7, 7)
#define UPM691X_REG_BOOST_LIMIT_SHIFT		7
#define UPM691X_REG_LIMIT_SEL_MASK		GENMASK(6, 6)
#define UPM691X_REG_LIMIT_SEL_SHIFT		6
#define UPM691X_REG_TERMINAL_CUR_MASK		GENMASK(3, 0)

#define UPM691X_REG_WATCHDOG_TIMER_MASK	GENMASK(5, 4)
#define UPM691X_REG_WATCHDOG_TIMER_SHIFT	4

#define UPM691X_REG_EN_HIZ_MASK		GENMASK(7, 7)
#define UPM691X_REG_EN_HIZ_SHIFT		7

#define UPM691X_REG7_IINDET_EN_SHIFT		7
#define UPM691X_REG7_IINDET_EN_MASK		GENMASK(7, 7)

#define UPM691X_REGA_DPM_INT_MASK		GENMASK(1, 0)
#define UPM691X_REGA_DPM_VALUE			3

#define UPM691X_DISABLE_PIN_MASK		BIT(0)
#define UPM691X_DISABLE_PIN_MASK_2721		BIT(15)

#define VENDOR_UPM691X				(0x1)

#define UPM691X_OTG_RETRY_TIMES		10

#define UPM691X_ROLE_MASTER			1
#define UPM691X_ROLE_SLAVE			2

#define UPM691X_FCHG_OVP_6V			6000
#define UPM691X_FCHG_OVP_9V			9000
#define UPM691X_FCHG_OVP_14V			14000
#define UPM691X_FAST_CHARGER_VOLTAGE_MAX	10500000
#define UPM691X_NORMAL_CHARGER_VOLTAGE_MAX	6500000
#define UPM691X_VINPDM_VOLTAGE_MAX		5400
#define UPM691X_TERMINA_VOLTAGE_MAX		4808
#define UPM691X_TERMINA_CURRENT_MAX		960
#define UPM691X_LIMIT_CURRENT_MAX		3200000
#define UPM691X_ICHG_CURRENT_MAX		3000

#define UPM691X_WAKE_UP_MS			1000
#define UPM691X_PROBE_TIMEOUT			msecs_to_jiffies(500)

#define UPM691X_REG4_VRECHG_220mV		1
#define UPM691X_REG4_VRECHG_MASK		0x01

#define UPM691X_REG6_BOOSTV_5P3V		0x30
#define UPM691X_REG6_BOOSTV_MASK		0x30

#define UPM691X_WATCH_DOG_TIME_OUT_MS		20000
#define UPM691X_SHUTDOWN_LIMIT			1500000
#define UPM691X_SHUTDOWN_CURRENT		500000

//reg 0d
#define UPM691X_REG_0D			0x0D
#define UPM691X_REG0D_VBAT_REG_FT_MASK		0xC0
#define UPM691X_REG0D_VBAT_REG_FT_SHIFT		6
#define UPM691X_REG0D_VREG_FT_DEFAULT		0
#define UPM691X_REG0D_VREG_FT_INC8MV		1
#define UPM691X_REG0D_VREG_FT_INC16MV		2
#define UPM691X_REG0D_VREG_FT_INC24MV		3
#define UPM691X_REG0D_VREG_FT_INC_BASE		0
#define UPM691X_REG0D_VREG_FT_INC_LSB		8

static const char * const pmic_syscon_name[] = {
	"sprd,sc27xx-syscon",
	"sprd,ump962x-syscon",
	"sprd,ump9651-syscon",
};

static bool boot_calibration;

struct upm691x_charger_sysfs {
	char *name;
	struct attribute_group attr_g;
	struct device_attribute attr_dump_reg;
	struct device_attribute attr_lookup_reg;
	struct device_attribute attr_sel_reg_id;
	struct device_attribute attr_reg_val;
	struct attribute *attrs[5];

	struct upm691x_charger_info *info;
};

struct upm691x_charge_current {
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

struct upm691x_charger_info {
	struct i2c_client *client;
	struct device *dev;
	struct power_supply *psy_usb;
	struct upm691x_charge_current cur;
	struct work_struct work;
	struct mutex lock;
	struct mutex input_limit_cur_lock;
	struct mutex i2c_rw_lock;
	bool charging;
	struct delayed_work otg_work;
	struct delayed_work wdt_work;
	struct regmap *pmic;
	struct upm691x_charger_sysfs *sysfs;
	struct completion probe_init;
	struct charger_dev *upm_charger;
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
	bool try_otg_enable;
	unsigned int irq_gpio;
	bool is_charger_online;
	bool disable_power_path;
	bool probe_initialized;
	bool use_typec_extcon;
	bool shutdown_flag;
	bool suspend;
	int voltage_max_microvolt;
	int termination_cur;
	int reg_id;
	bool vbus_gd;
};

struct upm691x_charger_reg_tab {
	int id;
	u32 addr;
	char *name;
};

static struct upm691x_charger_reg_tab upm691x_reg_tab[] = {
	{0, UPM691X_REG_00, "EN_HIZ/EN_ICHG_MON/IINDPM"},
	{1, UPM691X_REG_01, "PFM_DIS/WD_RST/OTG_CONFIG/CHG_CONFIG/SYS_Min/Min_VBAT_SEL"},
	{2, UPM691X_REG_02, "BOOST_LIM/Q1_FULLON/ICHG"},
	{3, UPM691X_REG_03, "IPRECHG/ITERM"},
	{4, UPM691X_REG_04, "VREG/TOPOFF_TIMER/VRECHG"},
	{5, UPM691X_REG_05, "EN_TERM/WATCHDOG/EN_TIMER/CHG_TIMER/TREG/JEITA_ISET"},
	{6, UPM691X_REG_06, "OVP/BOOSTV/VINDPM"},
	{7, UPM691X_REG_07,
	 "IINDET_EN/TMR2X_EN/BATFET_DIS/JEITA_VSET/BATFET_DLY/BATFET_RST_EN/VDPM_BAT_TRACK"},
	{8, UPM691X_REG_08, "VBUS_STAT/CHRG_STAT/PG_STAT/THERM_STAT/VSYS_STAT"},
	{9, UPM691X_REG_09, "WATCHDOG_FAULT/BOOST_FAULT/CHRG_FAULT/BAT_FAULT/NTC_FAULT"},
	{10, UPM691X_REG_0A,
	 "VBUS_GD/VINDPM_STAT/IINDPM_STAT/TOPOFF_ACTIVE/ACOV_STAT/VINDPM_INT_MASK/IINDPM_INT_MASK"},
	{11, UPM691X_REG_0B, "REG_RST/PN/DEV_REV"},
	{12, 0, NULL},
};

static bool enable_dump_stack;
module_param(enable_dump_stack, bool, 0644);
static int upm691x_enter_hiz_mode(struct upm691x_charger_info *info);
static int upm691x_exit_hiz_mode(struct upm691x_charger_info *info);

static void upm691x_dump_register(struct upm691x_charger_info *info);
static int upm691x_charger_set_vindpm(struct upm691x_charger_info *info, u32 vol);

static void upm691x_charger_dump_stack(void)
{
	if (enable_dump_stack)
		dump_stack();
}

static void power_path_control(struct upm691x_charger_info *info)
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

static bool upm691x_charger_is_bat_present(struct upm691x_charger_info *info)
{
	struct power_supply *psy;
	union power_supply_propval val;
	bool present = false;
	int ret;

	psy = power_supply_get_by_name(UPM691X_BATTERY_NAME);
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

static int upm691x_charger_is_fgu_present(struct upm691x_charger_info *info)
{
	struct power_supply *psy;

	psy = power_supply_get_by_name(UPM691X_BATTERY_NAME);
	if (!psy) {
		dev_err(info->dev, "Failed to find psy of sc27xx_fgu\n");
		return -ENODEV;
	}
	power_supply_put(psy);

	return 0;
}

static int __upm691x_write(struct upm691x_charger_info *info, u8 reg, u8 data)
{
	int ret;

	ret = i2c_smbus_write_byte_data(info->client, reg, data);
	if (ret < 0) {
		dev_err(info->dev, "%s, ret: %d, i2c write fail, reg[0x%02X] = 0x%02X!!!\n",
		       __func__, ret, reg, data);
		return ret;
	}

	return 0;
}

static int __upm691x_read(struct upm691x_charger_info *info, u8 reg, u8 *data)
{
	int ret;

	ret = i2c_smbus_read_byte_data(info->client, reg);
	if (ret < 0) {
		dev_err(info->dev, "%s, ret: %d, i2c read fail, reg[0x%02X]!!!\n",
			__func__, ret, reg);
		return ret;
	}

	*data = ret;
	return 0;
}

static int upm691x_read(struct upm691x_charger_info *info, u8 reg, u8 *data)
{
	int ret;

	mutex_lock(&info->i2c_rw_lock);
	ret = __upm691x_read(info, reg, data);
	mutex_unlock(&info->i2c_rw_lock);

	return ret;
}

static int upm691x_write(struct upm691x_charger_info *info, u8 reg, u8 data)
{
	int ret;

	mutex_lock(&info->i2c_rw_lock);
	ret = __upm691x_write(info, reg, data);
	mutex_unlock(&info->i2c_rw_lock);

	return ret;
}

static int upm691x_update_bits(struct upm691x_charger_info *info, u8 reg, u8 mask, u8 data)
{
	u8 v;
	int ret;

	mutex_lock(&info->i2c_rw_lock);
	ret = __upm691x_read(info, reg, &v);
	if (ret < 0) {
		dev_err(info->dev, "%s, ret: %d, failed to read reg[0x%02X]!!!\n",
			__func__, ret, reg);
		goto out;
	}

	v &= ~mask;
	v |= (data & mask);

	ret = __upm691x_write(info, reg, v);
	if (ret < 0)
		dev_err(info->dev, "%s, ret: %d, failed to write reg[0x%02X]: 0x%02X!!!\n",
			__func__, ret, reg, v);
out:
	mutex_unlock(&info->i2c_rw_lock);
	return ret;
}

static u32 upm691x_charger_get_limit_current(struct upm691x_charger_info *info, u32 *limit_cur)
{
	u8 reg_val;
	int ret;

	ret = upm691x_read(info, UPM691X_REG_00, &reg_val);
	if (ret < 0)
		return ret;

	reg_val &= REG00_IINLIM_MASK;
	reg_val = reg_val >> REG00_IINLIM_SHIFT;
	*limit_cur = (reg_val * REG00_IINLIM_LSB + REG00_IINLIM_BASE) * 1000;
	return 0;
}

static int upm691x_charger_set_limit_current(struct upm691x_charger_info *info,
					      u32 limit_cur, bool enable)
{
	u8 reg_val;
	int ret = 0;
	static bool hiz_flag = false;

	mutex_lock(&info->input_limit_cur_lock);
	if (enable) {
		ret = upm691x_charger_get_limit_current(info, &limit_cur);
		if (ret) {
			dev_err(info->dev, "get limit cur failed\n");
			goto out;
		}
		dev_info(info->dev, "%s, act_limit:%d, limit:%d \n", __func__, limit_cur, info->actual_limit_cur);
		if (limit_cur == info->actual_limit_cur)
			goto out;

		if (!info->try_otg_enable)
			upm691x_dump_register(info);

		limit_cur = info->actual_limit_cur;
		dev_info(info->dev, "set limit current limit_cur = %d\n", limit_cur);
	}

	if (limit_cur >= UPM691X_LIMIT_CURRENT_MAX)
		limit_cur = UPM691X_LIMIT_CURRENT_MAX;

	limit_cur = limit_cur / 1000;
	if (limit_cur < REG00_IINLIM_BASE)
		limit_cur = REG00_IINLIM_BASE;
	reg_val = (limit_cur - REG00_IINLIM_BASE) / REG00_IINLIM_LSB;
	info->actual_limit_cur = ((reg_val * REG00_IINLIM_LSB) + REG00_IINLIM_BASE) * 1000;

	if (false == sc27xx_get_try_sink_flag()) {
		if (limit_cur <= REG00_IINLIM_BASE) {
			ret = upm691x_charger_set_vindpm(info, 5400);
			if (ret)
				dev_err(info->dev, "set upm691x vindpm vol 5400 failed\n");
			else
				dev_err(info->dev, "set upm691x vindpm vol 5400 succ\n");
		} else {
			ret = upm691x_charger_set_vindpm(info, info->voltage_max_microvolt);
			if (ret)
				dev_err(info->dev, "set upm691x vindpm %d failed\n", info->voltage_max_microvolt);
			else
				dev_err(info->dev, "set upm691x vindpm %d succ\n", info->voltage_max_microvolt);
		}
		ret = upm691x_update_bits(info, UPM691X_REG_00, REG00_IINLIM_MASK,
				reg_val << REG00_IINLIM_SHIFT);
		if (ret)
			dev_err(info->dev, "set upm691x limit cur failed\n");
        } else {
        	if (limit_cur <= REG00_IINLIM_BASE) {
			hiz_flag = true;
                	ret = upm691x_enter_hiz_mode(info);//enable HIZ
                	if (ret)
                        	dev_err(info->dev, "%s enter HIZ failed\n", __func__);
			else
                        	dev_err(info->dev, "%s enter HIZ success\n", __func__);
        	} else {
			if (true == hiz_flag) {
				hiz_flag = false;
                		ret = upm691x_exit_hiz_mode(info);//disable HIZ
                		if (ret)
                        		dev_err(info->dev, "%s exit HIZ failed\n", __func__);
				else
                        		dev_err(info->dev, "%s exit HIZ success\n", __func__);
                	}
			ret = upm691x_update_bits(info, UPM691X_REG_00, REG00_IINLIM_MASK,
					reg_val << REG00_IINLIM_SHIFT);
			if (ret)
				dev_err(info->dev, "set upm691x limit cur failed\n");
        	}
        }

	dev_info(info->dev, "set limit_cur = %d, reg_val = %#x, actual_limit_cur = %d\n",
		 limit_cur, reg_val, info->actual_limit_cur);

out:
	mutex_unlock(&info->input_limit_cur_lock);

	return ret;
}

static int upm691x_set_acovp_threshold(struct upm691x_charger_info *info, int volt)
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

	return upm691x_update_bits(info, UPM691X_REG_06, REG06_OVP_MASK,
				    reg_val << REG06_OVP_SHIFT);
}

static int upm691x_charger_set_dpm(struct upm691x_charger_info *info)
{
	dev_err(info->dev,"%s set dpm success\n", __func__);
	return upm691x_update_bits(info, UPM691X_REG_0A,
				UPM691X_REGA_DPM_INT_MASK,
				UPM691X_REGA_DPM_VALUE);
}

static int upm691x_enable_charger(struct upm691x_charger_info *info, bool enable)
{
	u8 val = REG01_CHG_DISABLE;

	if (enable)
		val = REG01_CHG_ENABLE;

	return upm691x_update_bits(info, UPM691X_REG_01, REG01_CHG_CONFIG_MASK,
				    val << REG01_CHG_CONFIG_SHIFT);
}

static int upm691x_check_charge_full(struct upm691x_charger_info *info)
{
	int ret = 0;
	u8 reg_val = 0;
	ret = upm691x_read(info, UPM691X_REG_08, &reg_val);
	if (ret < 0)
		return ret;

	reg_val &= REG08_CHRG_STAT_MASK;
	reg_val = reg_val >> REG08_CHRG_STAT_SHIFT;
	if (reg_val == REG08_CHRG_STAT_CHGDONE){
		pr_err("%s:charge is done, recharge in probe!\n",__func__);
		ret = upm691x_enable_charger(info, false);
		if (ret) {
			dev_err(info->dev, "%s, failed to disable charger, ret = %d\n", __func__, ret);
			return ret;
		}
		msleep(10);
		ret = upm691x_enable_charger(info, true);
		if (ret) {
			dev_err(info->dev, "%s, failed to enable charger, ret = %d\n", __func__, ret);
			return ret;
		}
	}
	return ret;
}

static int upm691x_enable_pfm(struct upm691x_charger_info *info, bool enable)
{
	u8 val = REG01_PFM_ENABLE;

	if (!enable)
		val = REG01_PFM_DISABLE;
	return upm691x_update_bits(info, UPM691X_REG_01, REG01_PFM_DIS_MASK,
				   val << REG01_PFM_DIS_SHIFT);
}

static int upm691x_enter_hiz_mode(struct upm691x_charger_info *info)
{
	u8 val = REG00_HIZ_ENABLE << REG00_ENHIZ_SHIFT;
	dev_info(info->dev, "%s\n", __func__);
	return upm691x_update_bits(info, UPM691X_REG_00, REG00_ENHIZ_MASK, val);
}

static int upm691x_exit_hiz_mode(struct upm691x_charger_info *info)
{
	u8 val = REG00_HIZ_DISABLE << REG00_ENHIZ_SHIFT;
	dev_info(info->dev, "%s\n", __func__);
	return upm691x_update_bits(info, UPM691X_REG_00, REG00_ENHIZ_MASK, val);
}

static int upm691x_enable_term(struct upm691x_charger_info *info, bool enable)
{
	u8 val;
	int ret;

	if (enable)
		val = REG05_TERM_ENABLE << REG05_EN_TERM_SHIFT;
	else
		val = REG05_TERM_DISABLE << REG05_EN_TERM_SHIFT;

	ret = upm691x_update_bits(info, UPM691X_REG_05, REG05_EN_TERM_MASK, val);

	return ret;
}

static int upm691x_charger_set_vindpm(struct upm691x_charger_info *info, u32 vol)
{
	u8 reg_val;

	if (vol < REG06_VINDPM_BASE)
		reg_val = 0x0;
	else if (vol > UPM691X_VINPDM_VOLTAGE_MAX)
		reg_val = 0x0f;
	else
		reg_val = (vol - REG06_VINDPM_BASE) / REG06_VINDPM_LSB;

	return upm691x_update_bits(info, UPM691X_REG_06, REG06_VINDPM_MASK,
				    reg_val << REG06_VINDPM_SHIFT);
}

static int upm691x_get_charger_vindpm_state(struct upm691x_charger_info *info, bool *vindpm_stat)
{
	u8 reg_val = 0;
	int ret = 0;

	if (!info) {
		pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
		return -EINVAL;
	}

	ret = upm691x_read(info, UPM691X_REG_0A, &reg_val);
	if (ret < 0)
		return ret;

	*vindpm_stat = (reg_val & REG0A_VINDPM_STAT_MASK) >> REG0A_VINDPM_STAT_SHIFT;
	if(*vindpm_stat == false) {
		*vindpm_stat = !info->vbus_gd;
		dev_info(info->dev,  "%s: reg0A = 0x%x, vbus_gd: %d \n", __func__, reg_val, info->vbus_gd);
	}

	return ret;
}

static int upm691x_charger_set_shipmode(struct upm691x_charger_info *info, bool en)
{
	int ret = 0;

  	pr_info("%s:shipmode en=%d\n", __func__, en);
  	if (en) {
  		ret = upm691x_update_bits(info, UPM691X_REG_07, UPM691x_REG7_BATFET_DLY_MASK, 0);
  		ret |= upm691x_update_bits(info, UPM691X_REG_07, UPM691x_REG7_BATFET_DIS_MASK, 1 << UPM691x_REG7_BATFET_DIS_SHIFT);
  	} else {
  		ret |= upm691x_update_bits(info, UPM691X_REG_07, UPM691x_REG7_BATFET_DIS_MASK, 0);
  	}
  	return ret;
}

static int upm691x_charger_set_termina_vol(struct upm691x_charger_info *info, u32 vol)
{
	u8 reg_val, vreg_ft;
	int ret = 0;

	dev_dbg(info->dev, "%s:line%d: set termina vol = %d\n", __func__, __LINE__, vol);

	if (vol < REG04_VREG_BASE)
		vol = REG04_VREG_BASE;
	else if (vol > UPM691X_TERMINA_VOLTAGE_MAX)
		vol = UPM691X_TERMINA_VOLTAGE_MAX;
	if (((vol - REG04_VREG_BASE) % REG04_VREG_LSB) / 8 == 1) {
		vol -= 8;
		vreg_ft = UPM691X_REG0D_VREG_FT_INC8MV;
	}
	else if (((vol - REG04_VREG_BASE) % REG04_VREG_LSB) / 8 == 2) {
		vol -= 16;
		vreg_ft = UPM691X_REG0D_VREG_FT_INC16MV;
	}
	else if (((vol - REG04_VREG_BASE) % REG04_VREG_LSB) / 8 == 3) {
		vol -= 24;
		vreg_ft = UPM691X_REG0D_VREG_FT_INC24MV;
	}
	else
		vreg_ft = UPM691X_REG0D_VREG_FT_DEFAULT;
	reg_val = (vol - REG04_VREG_BASE) / REG04_VREG_LSB;
	reg_val <<= REG04_VREG_SHIFT;
	ret = upm691x_update_bits(info, UPM691X_REG_04, REG04_VREG_MASK, reg_val);
	if (ret)
		dev_err(info->dev, "set sc89601 termina_vol failed\n");
	ret = upm691x_update_bits(info, UPM691X_REG_0D, UPM691X_REG0D_VBAT_REG_FT_MASK,
								vreg_ft << UPM691X_REG0D_VBAT_REG_FT_SHIFT);
	if (ret)
		dev_err(info->dev, "set sc89601 termina_vol UPM691X_REG_0D failed\n");
	else
		dev_err(info->dev, "set UPM691X_REG_0D bit[7:6]: 0x%x\n", vreg_ft);

	return ret;
}

static int upm691x_charger_set_termina_cur(struct upm691x_charger_info *info, u32 cur)
{
	u8 reg_val;

	dev_dbg(info->dev, "%s:line%d: set termina cur = %d\n", __func__, __LINE__, cur);

	if (cur < REG03_ITERM_BASE)
		reg_val = 0x0;
	else if (cur >= UPM691X_TERMINA_CURRENT_MAX)
		reg_val = 0x0f;
	else
		reg_val = (cur - REG03_ITERM_BASE) / REG03_ITERM_LSB;

	return upm691x_update_bits(info, UPM691X_REG_03, REG03_ITERM_MASK,
				    reg_val << REG03_ITERM_SHIFT);
}

static int upm691x_charger_set_safety_cur(struct upm691x_charger_info *info, u32 cur)
{
	u8 reg_val;

	dev_dbg(info->dev, "%s:line%d: set safety cur = %d\n", __func__, __LINE__, cur);

	if (cur >= UPM691X_LIMIT_CURRENT_MAX)
		cur = UPM691X_LIMIT_CURRENT_MAX;

	cur = cur / 1000;
	if (cur < REG00_IINLIM_BASE)
		cur = REG00_IINLIM_BASE;

	reg_val = (cur - REG00_IINLIM_BASE) / REG00_IINLIM_LSB;
	return upm691x_update_bits(info, UPM691X_REG_00, REG00_IINLIM_MASK,
				    reg_val << REG00_IINLIM_SHIFT);

}

static int upm691x_charger_hw_init(struct upm691x_charger_info *info)
{
	struct sprd_battery_info bat_info = {};
	int ret;
	int termination_cur;

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
	termination_cur = bat_info.charge_term_current_ua / 1000;
	info->termination_cur = termination_cur;

	info->voltage_max_microvolt = bat_info.constant_charge_voltage_max_uv / 1000;

	ret = upm691x_charger_set_dpm(info);
	if (ret) {
		dev_err(info->dev, "set upm691x vindpm&&iindpm failed\n");
		return ret;
	}

	ret = upm691x_charger_set_safety_cur(info, info->cur.dcp_cur);
	if (ret) {
		dev_err(info->dev, "set upm691x safety cur failed\n");
		return ret;
	}

	if (info->role ==  UPM691X_ROLE_MASTER) {
		ret = upm691x_set_acovp_threshold(info, UPM691X_FCHG_OVP_6V);
		if (ret)
			dev_err(info->dev, "set upm691x ovp failed\n");
	} else if (info->role == UPM691X_ROLE_SLAVE) {
		ret = upm691x_set_acovp_threshold(info, UPM691X_FCHG_OVP_9V);
		if (ret)
			dev_err(info->dev, "set upm691x slave ovp failed\n");
	}

	ret = upm691x_enable_term(info, 1);
	if (ret) {
		dev_err(info->dev, "set upm691x terminal cur failed\n");
		return ret;
	}
	ret = upm691x_charger_set_vindpm(info, info->voltage_max_microvolt);
	if (ret) {
		dev_err(info->dev, "set upm691x vindpm vol failed\n");
		return ret;
	}

	upm691x_update_bits(info, UPM691X_REG_01, REG01_WDT_RESET_MASK,
			     REG01_WDT_RESET << REG01_WDT_RESET_SHIFT);
	ret = upm691x_update_bits(info, UPM691X_REG_05, REG05_WDT_MASK,
				   REG05_WDT_DISABLE << REG05_WDT_SHIFT);
	if (ret) {
		dev_err(info->dev, "feed upm691x watchdog failed\n");
		return ret;
	}

	ret = upm691x_update_bits(info, UPM691X_REG_05, REG05_EN_TIMER_MASK,
				   REG05_CHG_TIMER_DISABLE);
	if (ret) {
		dev_err(info->dev, "disable chg timer failed\n");
		return ret;
	}

	ret = upm691x_charger_set_termina_vol(info, info->voltage_max_microvolt);
	if (ret) {
		dev_err(info->dev, "set upm691x terminal vol failed\n");
		return ret;
	}

	ret = upm691x_charger_set_termina_cur(info, info->termination_cur);
	if (ret) {
		dev_err(info->dev, "set upm691x terminal cur failed\n");
		return ret;
	}

	ret = upm691x_charger_set_limit_current(info,
						 info->cur.unknown_cur, false);
	if (ret)
		dev_err(info->dev, "set upm691x limit current failed\n");

	ret = upm691x_update_bits(info, UPM691X_REG_04,
				UPM691X_REG4_VRECHG_220mV,
				UPM691X_REG4_VRECHG_MASK);

	if (ret)
		dev_err(info->dev, "set vrechg failed\n");

	ret = upm691x_update_bits(info, UPM691X_REG_06,
				UPM691X_REG6_BOOSTV_5P3V,
				UPM691X_REG6_BOOSTV_MASK);

	if (ret)
		dev_err(info->dev, "set boostv failed\n");

	return ret;
}

static int upm691x_charger_get_charge_voltage(struct upm691x_charger_info *info, u32 *charge_vol)
{
	struct power_supply *psy;
	union power_supply_propval val;
	int ret;

	psy = power_supply_get_by_name(UPM691X_BATTERY_NAME);
	if (!psy) {
		dev_err(info->dev, "failed to get UPM691X_BATTERY_NAME\n");
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

static void upm691x_dump_register(struct upm691x_charger_info *info)
{
	int ret;
	u8 addr;
	u8 val;

	for (addr = 0x0; addr <= 0x0B; addr++) {
		ret = upm691x_read(info, addr, &val);
		if (ret == 0)
                {
			if(addr == UPM691X_REG_0A)
				info->vbus_gd = val & 0x80;
			dev_info(info->dev, "dump reg %s,%d 0x%x = 0x%x\n",
				 __func__, __LINE__, addr, val);
                }
	}
}

static int upm691x_charger_enable_wdg(struct upm691x_charger_info *info,
				       bool en)
{
	int ret;

	if (en)
		ret = upm691x_update_bits(info, UPM691X_REG_05,
					   UPM691X_REG_WATCHDOG_TIMER_MASK,
					   0x01 << UPM691X_REG_WATCHDOG_TIMER_SHIFT);
	else
		ret = upm691x_update_bits(info, UPM691X_REG_05,
					   UPM691X_REG_WATCHDOG_TIMER_MASK, 0);
	if (ret)
		dev_err(info->dev, "%s:Failed to update %d\n", __func__, en);

	return ret;
}

static ssize_t upm691x_register_value_store(struct device *dev,
					     struct device_attribute *attr,
					     const char *buf, size_t count)
{
	struct upm691x_charger_sysfs *upm691x_sysfs = container_of(attr,
								     struct upm691x_charger_sysfs,
								     attr_reg_val);
	struct upm691x_charger_info *info = upm691x_sysfs->info;
	u8 val;
	int ret;

	if (!info) {
		dev_err(dev, "%s, info is null\n", __func__);
		return count;
	}

	ret = kstrtou8(buf, 16, &val);
	if (ret) {
		dev_err(info->dev, "%s, fail to get addr, ret = %d\n", __func__, ret);
		return count;
	}

	ret = upm691x_write(info, upm691x_reg_tab[info->reg_id].addr, val);
	if (ret) {
		dev_err(info->dev, "%s, fail to wite 0x%.2x to REG[0x%.2x], ret = %d\n",
			__func__, val, upm691x_reg_tab[info->reg_id].addr, ret);
		return count;
	}

	dev_info(info->dev, "%s, wite 0x%.2x to REG[0x%.2x] success\n",
		 __func__, val, upm691x_reg_tab[info->reg_id].addr);
	return count;
}

static ssize_t upm691x_register_value_show(struct device *dev,
					    struct device_attribute *attr,
					    char *buf)
{
	struct upm691x_charger_sysfs *upm691x_sysfs = container_of(attr,
								     struct upm691x_charger_sysfs,
								     attr_reg_val);
	struct upm691x_charger_info *info = upm691x_sysfs->info;
	u8 val;
	int ret;

	if (!info)
		return snprintf(buf, PAGE_SIZE, "%s, info is null\n", __func__);

	ret = upm691x_read(info, upm691x_reg_tab[info->reg_id].addr, &val);
	if (ret) {
		dev_err(info->dev, "%s, fail to get REG[0x%.2x] value, ret = %d\n",
			__func__, upm691x_reg_tab[info->reg_id].addr, ret);
		return snprintf(buf, PAGE_SIZE, "fail to get REG[0x%.2x] value\n",
				upm691x_reg_tab[info->reg_id].addr);
	}

	return snprintf(buf, PAGE_SIZE, "REG[0x%.2x] = 0x%.2x\n",
			upm691x_reg_tab[info->reg_id].addr, val);
}

static ssize_t upm691x_register_id_store(struct device *dev,
					  struct device_attribute *attr,
					  const char *buf, size_t count)
{
	struct upm691x_charger_sysfs *upm691x_sysfs = container_of(attr,
								     struct upm691x_charger_sysfs,
								     attr_sel_reg_id);
	struct upm691x_charger_info *info = upm691x_sysfs->info;
	int ret, id;

	if (!info) {
		dev_err(dev, "%s, info is null\n", __func__);
		return count;
	}

	ret =  kstrtoint(buf, 10, &id);
	if (ret) {
		dev_err(info->dev, "%s, store register id fail\n", __func__);
		return count;
	}

	if (id < 0 ||
	    id >= sizeof(upm691x_reg_tab) / sizeof(struct upm691x_charger_reg_tab) - 1) {
		dev_err(info->dev, "%s, store register id fail, id = %d is out of range\n",
			__func__, id);
		return count;
	}

	info->reg_id = id;

	dev_info(info->dev, "%s, store register id = %d success\n", __func__, id);
	return count;
}

static ssize_t upm691x_register_id_show(struct device *dev,
					 struct device_attribute *attr,
					 char *buf)
{
	struct upm691x_charger_sysfs *upm691x_sysfs = container_of(attr,
								     struct upm691x_charger_sysfs,
								     attr_sel_reg_id);
	struct upm691x_charger_info *info = upm691x_sysfs->info;

	if (!info)
		return snprintf(buf, PAGE_SIZE, "%s, info is null\n", __func__);

	return snprintf(buf, PAGE_SIZE, "Curent register id = %d\n", info->reg_id);
}

static ssize_t upm691x_register_table_show(struct device *dev,
					    struct device_attribute *attr,
					    char *buf)
{
	struct upm691x_charger_sysfs *upm691x_sysfs = container_of(attr,
								     struct upm691x_charger_sysfs,
								     attr_lookup_reg);
	struct upm691x_charger_info *info = upm691x_sysfs->info;
	int i, len, idx = 0;
	char reg_tab_buf[1024];

	if (!info)
		return snprintf(buf, PAGE_SIZE, "%s, info is null\n", __func__);

	memset(reg_tab_buf, '\0', sizeof(reg_tab_buf));
	len = snprintf(reg_tab_buf + idx, sizeof(reg_tab_buf) - idx,
		       "Format: [id] [addr] [desc]\n");
	idx += len;

	for (i = 0; upm691x_reg_tab[i].name; i++) {
		len = snprintf(reg_tab_buf + idx, sizeof(reg_tab_buf) - idx,
			       "[%d] [REG_0x%.2x] [%s];\n",
			       upm691x_reg_tab[i].id, upm691x_reg_tab[i].addr,
			       upm691x_reg_tab[i].name);
		idx += len;
	}

	return snprintf(buf, PAGE_SIZE, "%s\n", reg_tab_buf);
}

static ssize_t upm691x_dump_register_show(struct device *dev,
					   struct device_attribute *attr,
					   char *buf)
{
	struct upm691x_charger_sysfs *upm691x_sysfs = container_of(attr,
								     struct upm691x_charger_sysfs,
								     attr_dump_reg);
	struct upm691x_charger_info *info = upm691x_sysfs->info;

	if (!info)
		return snprintf(buf, PAGE_SIZE, "%s, info is null\n", __func__);

	upm691x_dump_register(info);

	return snprintf(buf, PAGE_SIZE, "%s\n", upm691x_sysfs->name);
}
/*
 * SYSFS interfaces:
 * /sys/class/power_supply/upm691x_charger/debug		[rw]	 [debug]
 */
static int upm691x_register_sysfs(struct upm691x_charger_info *info)
{
	struct upm691x_charger_sysfs *upm691x_sysfs;
	int ret;

	upm691x_sysfs = devm_kzalloc(info->dev, sizeof(*upm691x_sysfs), GFP_KERNEL);
	if (!upm691x_sysfs)
		return -ENOMEM;

	info->sysfs = upm691x_sysfs;
	upm691x_sysfs->name = "upm691x_sysfs";
	upm691x_sysfs->info = info;
	upm691x_sysfs->attrs[0] = &upm691x_sysfs->attr_dump_reg.attr;
	upm691x_sysfs->attrs[1] = &upm691x_sysfs->attr_lookup_reg.attr;
	upm691x_sysfs->attrs[2] = &upm691x_sysfs->attr_sel_reg_id.attr;
	upm691x_sysfs->attrs[3] = &upm691x_sysfs->attr_reg_val.attr;
	upm691x_sysfs->attrs[4] = NULL;
	upm691x_sysfs->attr_g.name = "debug";
	upm691x_sysfs->attr_g.attrs = upm691x_sysfs->attrs;

	sysfs_attr_init(&upm691x_sysfs->attr_dump_reg.attr);
	upm691x_sysfs->attr_dump_reg.attr.name = "dump_reg";
	upm691x_sysfs->attr_dump_reg.attr.mode = 0444;
	upm691x_sysfs->attr_dump_reg.show = upm691x_dump_register_show;

	sysfs_attr_init(&upm691x_sysfs->attr_lookup_reg.attr);
	upm691x_sysfs->attr_lookup_reg.attr.name = "lookup_reg";
	upm691x_sysfs->attr_lookup_reg.attr.mode = 0444;
	upm691x_sysfs->attr_lookup_reg.show = upm691x_register_table_show;

	sysfs_attr_init(&upm691x_sysfs->attr_sel_reg_id.attr);
	upm691x_sysfs->attr_sel_reg_id.attr.name = "sel_reg_id";
	upm691x_sysfs->attr_sel_reg_id.attr.mode = 0644;
	upm691x_sysfs->attr_sel_reg_id.show = upm691x_register_id_show;
	upm691x_sysfs->attr_sel_reg_id.store = upm691x_register_id_store;

	sysfs_attr_init(&upm691x_sysfs->attr_reg_val.attr);
	upm691x_sysfs->attr_reg_val.attr.name = "reg_val";
	upm691x_sysfs->attr_reg_val.attr.mode = 0644;
	upm691x_sysfs->attr_reg_val.show = upm691x_register_value_show;
	upm691x_sysfs->attr_reg_val.store = upm691x_register_value_store;

	ret = sysfs_create_group(&info->psy_usb->dev.kobj, &upm691x_sysfs->attr_g);
	if (ret < 0)
		dev_err(info->dev, "Cannot create sysfs , ret = %d\n", ret);

	return ret;
}

static int upm691x_charger_start_charge(struct upm691x_charger_info *info)
{
	int ret = 0;

	dev_info(info->dev, "%s:line%d: start charge\n", __func__, __LINE__);

	ret = upm691x_exit_hiz_mode(info);
	if (ret)
		dev_err(info->dev, "disable HIZ mode failed\n");

	ret = upm691x_charger_enable_wdg(info, true);
	if (ret)
		return ret;

	if (info->role == UPM691X_ROLE_MASTER) {
		ret = regmap_update_bits(info->pmic, info->charger_pd,
					 info->charger_pd_mask, 0);
		if (ret) {
			dev_err(info->dev, "enable upm691x charge failed\n");
			return ret;
		}
	} else if (info->role == UPM691X_ROLE_SLAVE) {
		gpiod_set_value_cansleep(info->gpiod, 0);
	}

	ret = upm691x_enable_charger(info, true);
	if (ret) {
		dev_err(info->dev, "%s, failed to enable charger, ret = %d\n", __func__, ret);
		return ret;
	}

	ret = upm691x_charger_set_termina_cur(info, info->termination_cur);
	if (ret)
		dev_err(info->dev, "set upm691x terminal cur failed\n");

	ret = upm691x_update_bits(info, UPM691X_REG_05, REG05_EN_TIMER_MASK,
				   REG05_CHG_TIMER_DISABLE);
	if (ret) {
		dev_err(info->dev, "disable chg timer failed\n");
	}
	ret = upm691x_update_bits(info, UPM691X_REG_04,
				UPM691X_REG4_VRECHG_220mV,
				UPM691X_REG4_VRECHG_MASK);
	if (ret)
		dev_err(info->dev, "set vrechg failed\n");

	upm691x_dump_register(info);

	return ret;
}

static void upm691x_charger_stop_charge(struct upm691x_charger_info *info)
{
	int ret;

	dev_info(info->dev, "%s:line%d: stop charge\n", __func__, __LINE__);

	ret = upm691x_enable_charger(info, false);
	if (ret)
		dev_err(info->dev, "disable charger failed\n");

	if (info->role == UPM691X_ROLE_MASTER) {
		if (boot_calibration) {
			ret = upm691x_enter_hiz_mode(info);
			if (ret)
				dev_err(info->dev, "enable HIZ mode failed\n");
		}

		ret = regmap_update_bits(info->pmic, info->charger_pd,
					 info->charger_pd_mask,
					 info->charger_pd_mask);
		if (ret)
			dev_err(info->dev, "disable upm691x charge failed\n");
	} else if (info->role == UPM691X_ROLE_SLAVE) {
		if (boot_calibration) {
			ret = upm691x_enter_hiz_mode(info);
			if (ret)
				dev_err(info->dev, "enable HIZ mode failed\n");
		}

		gpiod_set_value_cansleep(info->gpiod, 1);
	}

	if (info->disable_power_path) {
		ret = upm691x_update_bits(info, UPM691X_REG_00,
					   UPM691X_REG_EN_HIZ_MASK,
					   0x01 << UPM691X_REG_EN_HIZ_SHIFT);
		if (ret)
			dev_err(info->dev, "Failed to disable power path\n");
	}

	ret = upm691x_charger_enable_wdg(info, false);
	if (ret)
		dev_err(info->dev, "Failed to update wdg\n");
}

static int upm691x_charger_set_current(struct upm691x_charger_info *info, u32 cur)
{
	u8 ichg;

	dev_dbg(info->dev, "%s:line%d: set ibat cur = %d\n", __func__, __LINE__, cur);

	cur = cur / 1000;
	if (cur > UPM691X_ICHG_CURRENT_MAX) {
		ichg = 0x32;
	} else {
		if (cur < REG02_ICHG_BASE)
			cur = REG02_ICHG_BASE;

		ichg = (cur - REG02_ICHG_BASE)/REG02_ICHG_LSB;
	}
	return upm691x_update_bits(info, UPM691X_REG_02, REG02_ICHG_MASK,
				    ichg << REG02_ICHG_SHIFT);
}

static int upm691x_charger_get_current(struct upm691x_charger_info *info, u32 *cur)
{
	u8 reg_val;
	int ret;

	ret = upm691x_read(info, UPM691X_REG_02, &reg_val);
	if (ret < 0)
		return ret;

	reg_val &= REG02_ICHG_MASK;
	reg_val = reg_val >> REG02_ICHG_SHIFT;
	*cur = ((reg_val * REG02_ICHG_LSB) + REG02_ICHG_BASE) * 1000;
	return 0;
}

static int upm691x_charger_get_health(struct upm691x_charger_info *info, u32 *health)
{
	*health = POWER_SUPPLY_HEALTH_GOOD;

	return 0;
}

static int upm691x_charger_feed_watchdog(struct upm691x_charger_info *info)
{
	int ret = 0;
	u8 reg_val = REG01_WDT_RESET << REG01_WDT_RESET_SHIFT;
	u64 duration, curr = ktime_to_ms(ktime_get());

	dev_info(info->dev, "%s, start\n", __func__);

	ret = upm691x_update_bits(info, UPM691X_REG_01, REG01_WDT_RESET_MASK, reg_val);
	if (ret) {
		dev_err(info->dev, "reset upm691x failed\n");
		return ret;
	}

	duration = curr - info->last_wdt_time;
	if (duration >= UPM691X_WATCH_DOG_TIME_OUT_MS) {
		dev_err(info->dev, "charger wdg maybe time out:%lld ms\n", duration);
		upm691x_dump_register(info);
	}

	info->last_wdt_time = curr;

	if (info->otg_enable)
		return ret;

	if (false == sc27xx_get_try_sink_flag()) {
		ret = upm691x_charger_set_limit_current(info, info->actual_limit_cur, true);
		if (ret)
			dev_err(info->dev, "set limit cur failed\n");
        }
	return ret;
}

static __attribute__((unused)) irqreturn_t upm691x_int_handler(int irq, void *dev_id)
{
	struct upm691x_charger_info *info = dev_id;

	if (!info) {
		pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
		return IRQ_HANDLED;
	}
	dev_info(info->dev, "interrupt occurs\n");
	upm691x_dump_register(info);
	upm691x_charger_feed_watchdog(info);
	return IRQ_HANDLED;
}

static int upm691x_charger_get_status(struct upm691x_charger_info *info)
{
	if (info->charging)
		return POWER_SUPPLY_STATUS_CHARGING;
	else
		return POWER_SUPPLY_STATUS_NOT_CHARGING;
}

static bool upm691x_charger_get_power_path_status(struct upm691x_charger_info *info)
{
	u8 value;
	int ret;
	bool power_path_enabled = true;

	ret = upm691x_read(info, UPM691X_REG_00, &value);
	if (ret < 0) {
		dev_err(info->dev, "Fail to get power path status, ret = %d\n", ret);
		return power_path_enabled;
	}

	if (value & UPM691X_REG_EN_HIZ_MASK)
		power_path_enabled = false;

	return power_path_enabled;
}

static int upm691x_charger_set_status(struct upm691x_charger_info *info, int val, u32 input_vol)
{
	int ret = 0;

	if (val == CM_BUCK_MAX_TERMINA_VOL) {
		ret = upm691x_charger_set_termina_vol(info, UPM691X_TERMINA_VOLTAGE_MAX);
		if (ret) {
			dev_err(info->dev, "failed to set terminate max voltage\n");
			return ret;
		}
	} else if (val == CM_FAST_CHARGE_MAX_OVP_ENABLE_CMD) {
		ret = upm691x_set_acovp_threshold(info, UPM691X_FCHG_OVP_14V);
		if (ret) {
			dev_err(info->dev, "failed to set fast charge max ovp\n");
			return ret;
		}
	} else if (val == CM_FAST_CHARGE_OVP_ENABLE_CMD) {
		ret = upm691x_set_acovp_threshold(info, UPM691X_FCHG_OVP_9V);
		if (ret) {
			dev_err(info->dev, "failed to set 9V fast charge ovp\n");
			return ret;
		}
	} else if (val == CM_FAST_CHARGE_OVP_DISABLE_CMD) {
		ret = upm691x_set_acovp_threshold(info, UPM691X_FCHG_OVP_6V);
		if (ret) {
			dev_err(info->dev, "failed to set 9V fast charge ovp\n");
			return ret;
		}
		if (info->role == UPM691X_ROLE_MASTER) {
			if (input_vol > UPM691X_FAST_CHARGER_VOLTAGE_MAX)
				info->need_disable_Q1 = true;
		}
	} else if ((val == false) && (info->role == UPM691X_ROLE_MASTER)) {
		if (input_vol > UPM691X_NORMAL_CHARGER_VOLTAGE_MAX)
			info->need_disable_Q1 = true;
	}

	if (val > CM_FAST_CHARGE_NORMAL_CMD)
		return 0;
	if (!val && info->charging) {
		upm691x_charger_stop_charge(info);
		info->charging = false;
	} else if (val && !info->charging) {
		ret = upm691x_charger_start_charge(info);
		if (ret)
			dev_err(info->dev, "start charge failed\n");
		else
			info->charging = true;
	}

	return ret;
}

static bool upm691x_probe_is_ready(struct upm691x_charger_info *info)
{
	unsigned long timeout;

	if (unlikely(!info->probe_initialized)) {
		timeout = wait_for_completion_timeout(&info->probe_init, UPM691X_PROBE_TIMEOUT);
		if (!timeout) {
			dev_err(info->dev, "%s wait probe timeout\n", __func__);
			return false;
		}
	}

	return true;
}

static int upm691x_charger_check_power_path_status(struct upm691x_charger_info *info)
{
	int ret = 0;
	u8 val;

	if (info->disable_power_path)
		return 0;

	if (upm691x_charger_get_power_path_status(info))
		return 0;

	ret = upm691x_read(info, UPM691X_REG_00, &val);
	if (ret < 0) {
		dev_err(info->dev, "%s:line%d, failed to get reg0(%d)\n", __func__, __LINE__, ret);
		return ret;
	}

	if (val & REG00_ENHIZ_MASK) {
		dev_info(info->dev, "%s:line%d, exit hiz mode\n", __func__, __LINE__);
		ret = upm691x_exit_hiz_mode(info);
		if (ret < 0) {
			dev_err(info->dev, "%s:line%d, failed to exit hiz(%d)\n",
				__func__, __LINE__, ret);
			return ret;
		}
	}

	return ret;
}


static int upm691x_get_usb_online(struct upm691x_charger_info *info)
{
	int ret;
	bool vbus_gd = false;
	u8 reg_val = 0;
	int i;

	/* timeout: 20*5 = 100ms, if vbus is not good, wait 20ms retry */
	for (i = 0; i < 5; i++) {
		ret = upm691x_read(info, UPM691X_REG_0A, &reg_val);
		if (ret < 0) {
			dev_err(info->dev, "%s, read UPM691X_REG_0A failed, ret = %d\n", __func__, ret);
			return 0;
		}

		vbus_gd = (reg_val & REG0A_VBUS_GD_MASK) >> REG0A_VBUS_GD_SHIFT;
		if (vbus_gd) {
			dev_info(info->dev, "%s: vbus is good\n", __func__);
			return 1;
		}
		msleep(20);
	}

	dev_info(info->dev,  "%s: vbus is not good\n", __func__);
	return 0;
}

static int upm691x_get_auto_bc1p2_done(struct upm691x_charger_info *info)
{
	int ret;
	u8 i;
	u8 reg_val = 0;
	bool bc12_done = false;

	/* timeout: 100*10 = 1000ms, if bc1.2 is not done, wait 100ms retry */
	for (i = 0; i < 10; i++) {
        	msleep(100);
		if (!upm691x_get_usb_online(info) && i > 3)
                {
                	dev_err(info->dev, "%s, usb no online, cnt = %d\n", __func__, i);
			return bc12_done;
                }
		ret = upm691x_read(info, UPM691X_REG_15, &reg_val);
		if (ret < 0) {
			dev_err(info->dev, "%s, read UPM691X_REG_15 failed, ret = %d\n", __func__, ret);
			return bc12_done;
		}

		bc12_done = (reg_val & REG15_INPUT_DET_DONE_MASK) >>
			REG15_INPUT_DET_DONE_SHIFT;
		if (bc12_done == REG15_INPUT_DET_DONE) {
			dev_info(info->dev, "%s: bc1.2 is done by bc_stat\n", __func__);
			return bc12_done;
		}
          
        	ret = upm691x_read(info, UPM691X_REG_08, &reg_val);
        	if (ret < 0) {
			dev_err(info->dev, "%s, read UPM691X_REG_08 failed, ret = %d\n", __func__, ret);
			return bc12_done;
		}
        	bc12_done = (reg_val & 0x04) >> 2;
		if (bc12_done == REG15_INPUT_DET_DONE) {
			dev_info(info->dev, "%s: bc1.2 is done by pg_stat\n", __func__);
			return bc12_done;
		}
	}
	dev_info(info->dev, "%s: bc1.2 is not done\n", __func__);
	return bc12_done;
}

static __attribute__((unused)) int upm691x_get_force_bc1p2_done(struct upm691x_charger_info *info)
{
	int ret;
	bool bc12_done = false;

	bc12_done = upm691x_get_auto_bc1p2_done(info);

	/* force dpdm */
	ret = upm691x_update_bits(info, UPM691X_REG_07, REG07_IINDET_EN_MASK,
		REG07_IINDET_EN_ENABLE << REG07_IINDET_EN_SHIFT);
	if (ret < 0)
		dev_err(info->dev, "%s, Set IINDET_EN 1 failed, ret = %d\n", __func__, ret);

	msleep(100);
	bc12_done = upm691x_get_auto_bc1p2_done(info);

	return bc12_done;
}

static int upm691x_get_bc1p2_result(struct upm691x_charger_info *info, bool flag)
{
	int ret;
	u8 reg_val = 0;
	int vbus_stat;
	int chg_type = POWER_SUPPLY_USB_TYPE_UNKNOWN;

	ret = upm691x_read(info, UPM691X_REG_08, &reg_val);
	if (ret < 0) {
		dev_err(info->dev, "%s, read UPM691X_REG_08 failed, ret = %d\n", __func__, ret);
		return chg_type;
	}

	vbus_stat = (reg_val & REG08_VBUS_STAT_MASK) >> REG08_VBUS_STAT_SHIFT;
	switch (vbus_stat) {
	case REG08_VBUS_TYPE_USB:
		chg_type = POWER_SUPPLY_USB_TYPE_SDP;
		break;
	case REG08_VBUS_TYPE_CDP:
		chg_type = POWER_SUPPLY_USB_TYPE_CDP;
		break;
	case REG08_VBUS_TYPE_ADAPTER:
		chg_type = POWER_SUPPLY_USB_TYPE_DCP;
		break;
	case REG08_VBUS_TYPE_UNKNOWN_ADAPTER:
		if (flag)
			chg_type = POWER_SUPPLY_USB_TYPE_C;
		else
			chg_type = POWER_SUPPLY_USB_TYPE_UNKNOWN;
		break;
	case REG08_VBUS_TYPE_NON_STANDARD_ADAPTER:
		if (flag)
			chg_type = POWER_SUPPLY_USB_TYPE_DCP;
		else
			chg_type = POWER_SUPPLY_USB_TYPE_UNKNOWN;
		break;
	default:
		chg_type = POWER_SUPPLY_USB_TYPE_UNKNOWN;
		break;
	}

	dev_info(info->dev, "%s: bool=%d, vbus_stat=%d, chg_type=%d\n", __func__, flag, vbus_stat, chg_type);
	return chg_type;
}

static int upm691x_charger_is_present(struct upm691x_charger_info *info)
{
	if (!info) {
		pr_err("%s: info is null\n", __func__);
		return 0;
	}

	return upm691x_get_usb_online(info);
}

static int upm691x_charger_chg_type_det(struct upm691x_charger_info *info, bool force_dpdm)
{
	int ret;
	int chg_type = POWER_SUPPLY_USB_TYPE_UNKNOWN;

	if (!info) {
		pr_err("%s: info is null\n", __func__);
		return chg_type;
	}

	if (force_dpdm) {
		sprd_hsphy_set_high_impedance_state();
		msleep(100);
		ret = upm691x_get_force_bc1p2_done(info);
		sprd_hsphy_cancel_high_impedance_state();
	} else {
		ret = upm691x_get_auto_bc1p2_done(info);
	}
	chg_type = upm691x_get_bc1p2_result(info, false);

	return chg_type;
}

static int upm691x_charger_get_vbus_stat(struct upm691x_charger_info *info)
{
	int ret;
	int chg_type = POWER_SUPPLY_USB_TYPE_UNKNOWN;

	if (!info) {
		pr_err("%s: info is null\n", __func__);
		return chg_type;
	}

	if (!upm691x_get_usb_online(info))
		return chg_type;

	ret = upm691x_get_auto_bc1p2_done(info);
	chg_type = upm691x_get_bc1p2_result(info, true);

	return chg_type;
}

static int upm691x_charger_first_bc1p2(struct upm691x_charger_info *info, int *bc1p2_result)
{
	int ret;
	int chg_type = POWER_SUPPLY_USB_TYPE_UNKNOWN;

	if (!info) {
		pr_err("%s: info is null\n", __func__);
		goto first_end;
	}
	if (true == sc27xx_get_try_sink_flag()) {
		msleep(300);
		ret = upm691x_exit_hiz_mode(info);//disable HIZ
		if (ret)
			dev_err(info->dev, "%s exit HIZ failed\n", __func__);
		else
			dev_err(info->dev, "%s exit HIZ success\n", __func__);
        }
	ret = upm691x_get_auto_bc1p2_done(info);
	chg_type = upm691x_get_bc1p2_result(info, false);
first_end:
	*bc1p2_result = chg_type;
	return 0;
}

static int upm691x_charger_retry_bc1p2(struct upm691x_charger_info *info, int *bc1p2_result)
{
	*bc1p2_result = upm691x_charger_chg_type_det(info, true);
	return 0;
}

static int upm691x_charger_usb_get_property(struct power_supply *psy,
					     enum power_supply_property psp,
					     union power_supply_propval *val)
{
	struct upm691x_charger_info *info = power_supply_get_drvdata(psy);
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

	if (!upm691x_probe_is_ready(info)) {
		dev_err(info->dev, "%s wait probe timeout\n", __func__);
		return -EINVAL;
	}

	mutex_lock(&info->lock);
	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
		if (val->intval == CM_POWER_PATH_ENABLE_CMD ||
		    val->intval == CM_POWER_PATH_DISABLE_CMD) {
			val->intval = upm691x_charger_get_power_path_status(info);
			break;
		} else if (val->intval == CM_BUCK_MAX_TERMINA_VOL) {
			val->intval = UPM691X_TERMINA_VOLTAGE_MAX * 1000;
			break;
		}

		val->intval = upm691x_charger_get_status(info);
		break;
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT:
		if (!info->charging) {
			val->intval = 0;
		} else {
			ret = upm691x_charger_get_current(info, &cur);
			if (ret)
				goto out;

			val->intval = cur;
		}
		break;
	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
		if (!info->charging) {
			val->intval = 0;
		} else {
			ret = upm691x_charger_get_limit_current(info, &cur);
			if (ret)
				goto out;

			val->intval = cur;
		}
		break;
	case POWER_SUPPLY_PROP_HEALTH:
		if (info->charging) {
			val->intval = 0;
		} else {
			ret = upm691x_charger_get_health(info, &health);
			if (ret)
				goto out;

			val->intval = health;
		}
		break;
	case POWER_SUPPLY_PROP_CALIBRATE:
		if (info->role == UPM691X_ROLE_MASTER) {
			ret = regmap_read(info->pmic, info->charger_pd, &enabled);
			if (ret) {
				dev_err(info->dev, "get upm691x charge status failed\n");
				goto out;
			}
		} else if (info->role == UPM691X_ROLE_SLAVE) {
			enabled = gpiod_get_value_cansleep(info->gpiod);
		}

		val->intval = !enabled;
		break;
	case POWER_SUPPLY_PROP_USB_TYPE:
		val->intval = upm691x_charger_chg_type_det(info, false);
		break;
	case POWER_SUPPLY_PROP_MANUFACTURER:
		val->strval = "upm6910";
		break;
	default:
		ret = -EINVAL;
	}

out:
	mutex_unlock(&info->lock);
	return ret;
}

static int upm691x_charger_usb_set_property(struct power_supply *psy,
					     enum power_supply_property psp,
					     const union power_supply_propval *val)
{
	struct upm691x_charger_info *info = power_supply_get_drvdata(psy);
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
		bat_present = upm691x_charger_is_bat_present(info);
		ret = upm691x_charger_get_charge_voltage(info, &input_vol);
		if (ret) {
			input_vol = 0;
			dev_err(info->dev, "failed to get charge voltage! ret = %d\n", ret);
		}
	}

	if (!upm691x_probe_is_ready(info)) {
		dev_err(info->dev, "%s wait probe timeout\n", __func__);
		return -EINVAL;
	}

	mutex_lock(&info->lock);

	switch (psp) {
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT:
		ret = upm691x_charger_set_current(info, val->intval);
		if (ret < 0)
			dev_err(info->dev, "set charge current failed\n");
		break;
	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
		ret = upm691x_charger_set_limit_current(info, val->intval, false);
		if (ret < 0)
			dev_err(info->dev, "set input current limit failed\n");
		break;

	case POWER_SUPPLY_PROP_STATUS:
		if (val->intval == CM_POWER_PATH_ENABLE_CMD) {
			ret = upm691x_exit_hiz_mode(info);
			break;
		} else if (val->intval == CM_POWER_PATH_DISABLE_CMD) {
			ret = upm691x_enter_hiz_mode(info);
			break;
		}

		ret = upm691x_charger_set_status(info, val->intval, input_vol);
		if (ret < 0)
			dev_err(info->dev, "set charge status failed\n");
		break;
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE_MAX:
		ret = upm691x_charger_set_termina_vol(info, val->intval / 1000);
		if (ret < 0)
			dev_err(info->dev, "failed to set terminate voltage\n");
		break;
	case POWER_SUPPLY_PROP_CALIBRATE:
		dev_info(info->dev, "POWER_SUPPLY_PROP_CHARGE_ENABLED = %d\n", val->intval);
		if (val->intval == true) {
			ret = upm691x_charger_start_charge(info);
			if (ret)
				dev_err(info->dev, "start charge failed\n");
		} else if (val->intval == false) {
			upm691x_charger_stop_charge(info);
		}
		break;
	case POWER_SUPPLY_PROP_PRESENT:
		info->is_charger_online = val->intval;
		if (info->otg_enable)
			dev_info(info->dev, "otg enable, online = %d\n", info->is_charger_online);
		if (val->intval == true || info->otg_enable) {
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

static int upm691x_charger_property_is_writeable(struct power_supply *psy,
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

static enum power_supply_property upm691x_usb_props[] = {
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT,
	POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT,
	POWER_SUPPLY_PROP_HEALTH,
	POWER_SUPPLY_PROP_CALIBRATE,
	POWER_SUPPLY_PROP_MANUFACTURER,
};

static const struct power_supply_desc upm691x_charger_desc = {
	.name			= "upm691x_charger",
	.type			= POWER_SUPPLY_TYPE_UNKNOWN,
	.properties		= upm691x_usb_props,
	.num_properties		= ARRAY_SIZE(upm691x_usb_props),
	.get_property		= upm691x_charger_usb_get_property,
	.set_property		= upm691x_charger_usb_set_property,
	.property_is_writeable	= upm691x_charger_property_is_writeable,
};

static const struct power_supply_desc upm691x_slave_charger_desc = {
	.name			= "upm691x_slave_charger",
	.type			= POWER_SUPPLY_TYPE_UNKNOWN,
	.properties		= upm691x_usb_props,
	.num_properties		= ARRAY_SIZE(upm691x_usb_props),
	.get_property		= upm691x_charger_usb_get_property,
	.set_property		= upm691x_charger_usb_set_property,
	.property_is_writeable	= upm691x_charger_property_is_writeable,
};

static void upm691x_charger_feed_watchdog_work(struct work_struct *work)
{
	struct delayed_work *dwork = to_delayed_work(work);
	struct upm691x_charger_info *info = container_of(dwork,
							  struct upm691x_charger_info,
							  wdt_work);
	int ret;

	if (!info) {
		pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
		return;
	}

	ret = upm691x_charger_feed_watchdog(info);
	if (ret)
		schedule_delayed_work(&info->wdt_work, HZ * 1);
	else
		schedule_delayed_work(&info->wdt_work, HZ * 15);
}

#if IS_ENABLED(CONFIG_REGULATOR)
static bool upm691x_charger_otg_vbus_is_enabled(struct upm691x_charger_info *info)
{
	bool otg_vbus_is_enabled = false;
	u8 reg_val = 0;
	int ret = 0;

	ret = upm691x_read(info, UPM691X_REG_01, &reg_val);
	if (ret) {
		dev_err(info->dev, "%s, failed to read reg[%02X], ret = %d\n",
			__func__, UPM691X_REG_01, ret);
		return otg_vbus_is_enabled;
	}

	if (reg_val & UPM691X_REG_OTG_MASK)
		otg_vbus_is_enabled = true;

	return otg_vbus_is_enabled;
}

static bool upm691x_charger_check_otg_fault(struct upm691x_charger_info *info)
{
	int ret;
	u8 value = 0;
	bool status = true;

	ret = upm691x_read(info, UPM691X_REG_09, &value);
	if (ret) {
		dev_err(info->dev, "get upm691x charger otg fault status failed\n");
		return status;
	}

	if (!(value & UPM691X_REG_BOOST_FAULT_MASK))
		status = false;
	else
		dev_err(info->dev, "boost fault occurs, REG_9 = 0x%x\n", value);

	return status;
}

static void upm691x_charger_otg_work(struct work_struct *work)
{
	struct delayed_work *dwork = to_delayed_work(work);
	struct upm691x_charger_info *info = container_of(dwork,
							  struct upm691x_charger_info, otg_work);
	bool otg_valid;
	bool otg_fault;
	int ret, retry = 0;

	if (!info) {
		pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
		return;
	}

	otg_valid = upm691x_charger_otg_vbus_is_enabled(info);
	if (otg_valid)
		goto out;

	do {
		otg_fault = upm691x_charger_check_otg_fault(info);
		if (!otg_fault) {
			ret = upm691x_update_bits(info, UPM691X_REG_01,
						   REG01_OTG_CONFIG_MASK,
						   REG01_OTG_ENABLE << REG01_OTG_CONFIG_SHIFT);
			if (ret)
				dev_err(info->dev, "restart upm691x charger otg failed\n");
		}
		otg_valid = upm691x_charger_otg_vbus_is_enabled(info);
	} while (!otg_valid && retry++ < UPM691X_OTG_RETRY_TIMES);

	if (retry >= UPM691X_OTG_RETRY_TIMES) {
		dev_err(info->dev, "Restart OTG failed\n");
		return;
	}

out:
	schedule_delayed_work(&info->otg_work, msecs_to_jiffies(1500));
}

static int upm691x_charger_enable_otg(struct regulator_dev *dev)
{
	int temp,capacity;
	struct power_supply *batt_psy = NULL;
	union power_supply_propval val;
	struct upm691x_charger_info *info = rdev_get_drvdata(dev);
	struct charger_manager *cm = NULL;
	int ret = 0;

	if (!info) {
		pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
		return -EINVAL;
	}
	if (info->shutdown_flag)
		return ret;

	upm691x_charger_dump_stack();

	if (!upm691x_probe_is_ready(info)) {
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
	batt_psy = power_supply_get_by_name("battery");
	if (!batt_psy)
		goto err;
	ret = power_supply_get_property(batt_psy, POWER_SUPPLY_PROP_TEMP, &val);
	temp = val.intval;
	ret |= power_supply_get_property(batt_psy, POWER_SUPPLY_PROP_CAPACITY, &val);
	capacity = val.intval;
	if (ret)
		goto err;
        usleep_range(49000, 50000);
	if (temp > 0 && capacity > 15 && capacity <= 40) {
		ret = upm691x_update_bits(info, UPM691X_REG_02,
				  UPM691X_REG_BOOST_LIMIT_MASK,
				  1 << UPM691X_REG_BOOST_LIMIT_SHIFT);
		ret |= upm691x_update_bits(info,UPM691X_REG_15,
				  UPM691X_REG_LIMIT_SEL_MASK,
				  0);
	} else if (temp > 0 && capacity > 40) {
		ret = upm691x_update_bits(info, UPM691X_REG_15,
				  UPM691X_REG_LIMIT_SEL_MASK,
				  1 << UPM691X_REG_LIMIT_SEL_SHIFT);
	} else {
		ret = upm691x_update_bits(info, UPM691X_REG_02,
				  UPM691X_REG_BOOST_LIMIT_MASK,
				  0);
		ret |= upm691x_update_bits(info, UPM691X_REG_15,
				  UPM691X_REG_LIMIT_SEL_MASK,
				  0);
	}

	cm = power_supply_get_drvdata(batt_psy);

	if (IS_ERR_OR_NULL(cm)) {
		pr_err("upm691x couldn't get cm of battery!\n");
		goto err;
	}

	if(cm->otg_debug){
		ret = upm691x_update_bits(info, UPM691X_REG_15,
					   UPM691X_REG_LIMIT_SEL_MASK,
					   1 << UPM691X_REG_LIMIT_SEL_SHIFT);
	}

	if (ret) {
		dev_err(info->dev, "Set boost limit failed\n");
		goto err;
	}
err:
	ret = upm691x_update_bits(info, UPM691X_REG_01,
				   REG01_OTG_CONFIG_MASK,
				   REG01_OTG_ENABLE << REG01_OTG_CONFIG_SHIFT);
	if (ret) {
		dev_err(info->dev, "enable upm691x otg failed\n");
		regmap_update_bits(info->pmic, info->charger_detect, BIT_DP_DM_BC_ENB, 0);
		return ret;
	}

	ret = upm691x_charger_enable_wdg(info, true);
	if (ret)
		return ret;

	info->last_wdt_time = ktime_to_ms(ktime_get());

	info->try_otg_enable = true;
	ret = upm691x_charger_feed_watchdog(info);
	if (ret) {
		info->try_otg_enable = false;
		dev_err(info->dev, "%s, failed to feed watchdog, ret = %d!!!\n",
			__func__, ret);
		return ret;
	}

	ret = upm691x_exit_hiz_mode(info);
	if (ret) {
		info->try_otg_enable = false;
		dev_err(info->dev, "Failed to enable power path\n");
	}

	info->otg_enable = true;
	schedule_delayed_work(&info->wdt_work,
			      msecs_to_jiffies(UPM691X_FEED_WATCHDOG_VALID_MS));
	schedule_delayed_work(&info->otg_work,
			      msecs_to_jiffies(UPM691X_OTG_VALID_MS));
	dev_info(info->dev, "%s:line%d:enable_otg\n", __func__, __LINE__);
	pr_err("lcy: this is end!");
	return ret;
}

static int upm691x_charger_disable_otg(struct regulator_dev *dev)
{
	struct upm691x_charger_info *info = rdev_get_drvdata(dev);
	int ret = 0;

	if (!info) {
		pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
		return -EINVAL;
	}

	upm691x_charger_dump_stack();

	if (!upm691x_probe_is_ready(info)) {
		dev_err(info->dev, "%s wait probe timeout\n", __func__);
		return -EINVAL;
	}

	info->try_otg_enable = false;
	info->otg_enable = false;
	cancel_delayed_work_sync(&info->wdt_work);
	cancel_delayed_work_sync(&info->otg_work);

	ret = upm691x_update_bits(info, UPM691X_REG_01,
				   REG01_OTG_CONFIG_MASK,
				   REG01_OTG_DISABLE << REG01_OTG_CONFIG_SHIFT);
	if (ret) {
		dev_err(info->dev, "disable upm691x otg failed\n");
		return ret;
	}

	ret = upm691x_charger_enable_wdg(info, false);
	if (ret)
		return ret;

	/* Enable charger detection function to identify the charger type */
	if (!info->use_typec_extcon) {
		ret = regmap_update_bits(info->pmic, info->charger_detect, BIT_DP_DM_BC_ENB, 0);
		if (ret)
			dev_err(info->dev, "enable BC1.2 failed\n");
	}
	dev_info(info->dev, "%s:line%d:disable_otg\n", __func__, __LINE__);

	return ret;
}

static int upm691x_charger_vbus_is_enabled(struct regulator_dev *dev)
{
	struct upm691x_charger_info *info = rdev_get_drvdata(dev);
	bool hw_otg_enable;
	int ret = 0;
	u8 reg_val = 0;

	if (!info) {
		pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
		return -EINVAL;
	}

	if (info->suspend) {
		ret = upm691x_read(info, UPM691X_REG_01, &reg_val);
		if (ret == -ESHUTDOWN) {
			dev_err(info->dev, "%s, ret: %d, the I2C module is in the suspend state!\n",
				__func__, ret);
			return ret;
		}
	}

	hw_otg_enable = upm691x_charger_otg_vbus_is_enabled(info);
	if (hw_otg_enable != info->otg_enable)
		dev_err(info->dev, "%s, otg enable, [HW SOFT] = [%d %d], mismatch!!!\n",
			__func__, hw_otg_enable, info->otg_enable);

	return info->otg_enable;
}

static const struct regulator_ops upm691x_charger_vbus_ops = {
	.enable = upm691x_charger_enable_otg,
	.disable = upm691x_charger_disable_otg,
	.is_enabled = upm691x_charger_vbus_is_enabled,
};

static const struct regulator_desc upm691x_charger_vbus_desc = {
	.name = "otg-vbus",
	.of_match = "otg-vbus",
	.type = REGULATOR_VOLTAGE,
	.owner = THIS_MODULE,
	.ops = &upm691x_charger_vbus_ops,
	.fixed_uV = 5000000,
	.n_voltages = 1,
};

static void upm691x_charger_check_otg_status(struct upm691x_charger_info *info)
{
	int ret;
	u8 val;

	ret = upm691x_read(info, UPM691X_REG_01, &val);
	if (ret < 0) {
		dev_err(info->dev, "%s:line%d, failed to get reg1(%d)\n", __func__, __LINE__, ret);
		return;
	}

	if (val & REG01_OTG_CONFIG_MASK) {
		dev_info(info->dev, "%s:line%d, exit otg mode\n", __func__, __LINE__);
		ret = upm691x_update_bits(info, UPM691X_REG_01, REG01_OTG_CONFIG_MASK,
					   REG01_OTG_DISABLE << REG01_OTG_CONFIG_SHIFT);
		if (ret)
			dev_err(info->dev, "exit upm691x otg failed, ret = %d\n", ret);
	}
}

static int upm691x_charger_register_vbus_regulator(struct upm691x_charger_info *info)
{
	struct regulator_config cfg = { };
	struct regulator_dev *reg;
	int ret = 0;

	/*
	 * only master to support otg
	 */
	if (info->role != UPM691X_ROLE_MASTER)
		return 0;

	upm691x_charger_check_otg_status(info);
	cfg.dev = info->dev;
	cfg.driver_data = info;
	reg = devm_regulator_register(info->dev, &upm691x_charger_vbus_desc, &cfg);
	if (IS_ERR(reg)) {
		ret = PTR_ERR(reg);
		dev_err(info->dev, "%s, failed to register vddvbus regulator:%d\n", __func__, ret);
	}

	return ret;
}

static int upm691x_charger_register_external_vbus_regulator(struct upm691x_charger_info *info)
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
	if (info->role != UPM691X_ROLE_MASTER)
		return 0;

	upm691x_charger_check_otg_status(info);
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
	reg = devm_regulator_register(cfg.dev, &upm691x_charger_vbus_desc, &cfg);
	if (IS_ERR(reg)) {
		ret = PTR_ERR(reg);
		dev_err(info->dev, "%s, failed to register vddvbus regulator:%d\n", __func__, ret);
	}

	return ret;
}

#else
static int upm691x_charger_register_vbus_regulator(struct upm691x_charger_info *info)
{
	return 0;
}

static int upm691x_charger_register_external_vbus_regulator(struct upm691x_charger_info *info)
{
	return 0;
}
#endif

static int upm691x_charger_detect_device(struct upm691x_charger_info *info)
{
	int ret, part_id;
	u8 reg_val;

	ret = upm691x_read(info, UPM691X_REG_0B, &reg_val);
	if (ret < 0) {
		dev_err(info->dev, "%s, failed to get device id, ret = %d\n", __func__, ret);
		return ret;
	}

	part_id = (reg_val & REG0B_PN_MASK) >> REG0B_PN_SHIFT;
	if (part_id != UPM691X_DEV_ID) {
		dev_err(info->dev, "%s, the device id is 0x%x\n", __func__, part_id);
		return -EINVAL;
	}

	return ret;
}

static int sgm_chg_is_present(struct charger_dev *charger, int *present)
{
	struct upm691x_charger_info *upm = charger_get_private(charger);
	*present = upm691x_charger_is_present(upm);
	return 0;
}

static int upm_chg_get_vindpm_state(struct charger_dev *charger, bool *state)
{
	struct upm691x_charger_info *upm = charger_get_private(charger);
	return upm691x_get_charger_vindpm_state(upm, state);
}

static int upm_chg_set_shipmode(struct charger_dev *charger, bool en)
{
	struct upm691x_charger_info *upm = charger_get_private(charger);
	return upm691x_charger_set_shipmode(upm, en);
}

static int upm_chg_bc1p2_first(struct charger_dev *charger, int *bc1p2_result)
{
	struct upm691x_charger_info *upm = charger_get_private(charger);
	return upm691x_charger_first_bc1p2(upm, bc1p2_result);
}

static int upm_chg_bc1p2_retry(struct charger_dev *charger, int *bc1p2_result)
{
	struct upm691x_charger_info *upm = charger_get_private(charger);
	return upm691x_charger_retry_bc1p2(upm, bc1p2_result);
}

static int upm_chg_get_vbus_type(struct charger_dev *charger, int *type )
{
	struct upm691x_charger_info *upm = charger_get_private(charger);
	*type = upm691x_charger_get_vbus_stat(upm);
	return 0;
}

static int upm_chg_set_vindpm(struct charger_dev *charger, int vindpm)
{
	struct upm691x_charger_info *upm = charger_get_private(charger);
	upm->voltage_max_microvolt = vindpm;
	return upm691x_charger_set_vindpm(upm, vindpm);
}

static int upm_chg_get_iindpm(struct charger_dev *charger, int *iindpm)
{
	struct upm691x_charger_info *upm = charger_get_private(charger);

	return upm691x_charger_get_limit_current(upm, iindpm);
}

static int upm_chg_set_iterm(struct charger_dev *charger, int iterm)
{
	struct upm691x_charger_info *upm = charger_get_private(charger);
	int ret;

	ret = upm691x_charger_set_termina_cur(upm, iterm);
	if (!ret)
		upm->termination_cur = iterm;

	return ret;
}

static int upm_chg_set_pfm(struct charger_dev *charger, bool en)
{
	struct upm691x_charger_info *sgm = charger_get_private(charger);

	return upm691x_enable_pfm(sgm, en);
}

/*****************************charger ops**************************************/
static struct charger_ops charger_ops = {
	.is_present = sgm_chg_is_present,
	.get_vindpm_state = upm_chg_get_vindpm_state,
	.set_shipmode = upm_chg_set_shipmode,
	.first_bc1p2 = upm_chg_bc1p2_first,
	.retry_bc1p2 = upm_chg_bc1p2_retry,
	.get_vbus_type = upm_chg_get_vbus_type,
	.set_vindpm = upm_chg_set_vindpm,
	.get_iindpm = upm_chg_get_iindpm,
	.set_iterm = upm_chg_set_iterm,
	.set_pfm = upm_chg_set_pfm,
};

static int upm691x_charger_probe(struct i2c_client *client,
		const struct i2c_device_id *id)
{
//	const struct i2c_device_id *id = i2c_client_get_device_id(client);
	struct i2c_adapter *adapter = to_i2c_adapter(client->dev.parent);
	struct device *dev = &client->dev;
	struct power_supply_config charger_cfg = { };
	struct upm691x_charger_info *info;
	struct device_node *regmap_np;
	struct platform_device *regmap_pdev;
	int ret, i;

//	client->addr = UPM6910_I2C_ADDR;
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

//	if (!id)
//		dev_err(dev, "Not get i2c client device id\n");

	mutex_init(&info->i2c_rw_lock);

	i2c_set_clientdata(client, info);

	ret = upm691x_charger_detect_device(info);
	if (ret) {
		dev_err(dev, "%s, failed to detect device, ret = %d\n", __func__, ret);
		ret = -ENODEV;
		goto destroy_i2c_rw_lock;
	}

	power_path_control(info);

	ret = upm691x_charger_is_fgu_present(info);
	if (ret) {
		dev_err(dev, "sc27xx_fgu not ready.\n");
		ret = -EPROBE_DEFER;
		goto destroy_i2c_rw_lock;
	}

	info->use_typec_extcon = device_property_read_bool(dev, "use-typec-extcon");
	info->disable_wdg = device_property_read_bool(dev, "disable-otg-wdg-in-sleep");

	ret = device_property_read_bool(dev, "role-slave");
	if (ret)
		info->role = UPM691X_ROLE_SLAVE;
	else
		info->role = UPM691X_ROLE_MASTER;

	if (info->role == UPM691X_ROLE_SLAVE) {
		info->gpiod = devm_gpiod_get(dev, "enable", GPIOD_OUT_HIGH);
		if (IS_ERR(info->gpiod)) {
			dev_err(dev, "failed to get enable gpio\n");
			ret = PTR_ERR(info->gpiod);
			goto destroy_i2c_rw_lock;
		}
	}

	for (i = 0; i < ARRAY_SIZE(pmic_syscon_name); i++) {
		regmap_np = of_find_compatible_node(NULL, NULL, pmic_syscon_name[i]);
		if (regmap_np)
			break;
	}

	if (regmap_np) {
		if (of_device_is_compatible(regmap_np->parent, "sprd,sc2721"))
			info->charger_pd_mask = UPM691X_DISABLE_PIN_MASK_2721;
		else
			info->charger_pd_mask = UPM691X_DISABLE_PIN_MASK;
	} else {
		dev_err(dev, "unable to get syscon node\n");
		ret = -ENODEV;
		goto destroy_i2c_rw_lock;
	}

	ret = of_property_read_u32_index(regmap_np, "reg", 1,
					 &info->charger_detect);
	if (ret) {
		dev_err(dev, "failed to get charger_detect\n");
		ret = -EINVAL;
		goto destroy_i2c_rw_lock;
	}

	ret = of_property_read_u32_index(regmap_np, "reg", 2,
					 &info->charger_pd);
	if (ret) {
		dev_err(dev, "failed to get charger_pd reg\n");
		goto destroy_i2c_rw_lock;
	}

	regmap_pdev = of_find_device_by_node(regmap_np);
	if (!regmap_pdev) {
		of_node_put(regmap_np);
		dev_err(dev, "unable to get syscon device\n");
		ret = -ENODEV;
		goto destroy_i2c_rw_lock;
	}

	of_node_put(regmap_np);
	info->pmic = dev_get_regmap(regmap_pdev->dev.parent, NULL);
	if (!info->pmic) {
		dev_err(dev, "unable to get pmic regmap device\n");
		ret = -ENODEV;
		goto destroy_i2c_rw_lock;
	}
	mutex_init(&info->lock);
	mutex_init(&info->input_limit_cur_lock);
	init_completion(&info->probe_init);

	charger_cfg.drv_data = info;
	charger_cfg.of_node = dev->of_node;
	if (info->role == UPM691X_ROLE_MASTER) {
		info->psy_usb = devm_power_supply_register(dev,
							   &upm691x_charger_desc,
							   &charger_cfg);
	} else if (info->role == UPM691X_ROLE_SLAVE) {
		info->psy_usb = devm_power_supply_register(dev,
							   &upm691x_slave_charger_desc,
							   &charger_cfg);
	}

	if (IS_ERR(info->psy_usb)) {
		dev_err(dev, "failed to register power supply\n");
		ret = PTR_ERR(info->psy_usb);
		goto out;
	}

	ret = upm691x_charger_hw_init(info);
	if (ret)
		goto out;

	upm691x_charger_check_power_path_status(info);

	device_init_wakeup(info->dev, true);

	alarm_init(&info->otg_timer, ALARM_BOOTTIME, NULL);
	INIT_DELAYED_WORK(&info->otg_work, upm691x_charger_otg_work);
	INIT_DELAYED_WORK(&info->wdt_work, upm691x_charger_feed_watchdog_work);

	if (device_property_read_bool(dev, "otg-vbus-node-external"))
		ret = upm691x_charger_register_external_vbus_regulator(info);
	else
		ret = upm691x_charger_register_vbus_regulator(info);

	if (ret) {
		dev_err(dev, "failed to register vbus regulator.\n");
		goto out;
	}

	ret = upm691x_register_sysfs(info);
	if (ret) {
		dev_err(info->dev, "register sysfs fail, ret = %d\n", ret);
		goto out;
	}
	info->upm_charger = charger_register("master_chg", info->dev, &charger_ops, info);
	if (!info->upm_charger) {
		ret = PTR_ERR(info->upm_charger);
		goto error_upm_charger;
	}

	info->irq_gpio = of_get_named_gpio(info->dev->of_node, "irq-gpio", 0);
	if (gpio_is_valid(info->irq_gpio)) {
		ret = devm_gpio_request_one(info->dev, info->irq_gpio,
					    GPIOF_DIR_IN, "upm691x_int");
		if (!ret)
			info->client->irq = gpio_to_irq(info->irq_gpio);
		else
			dev_err(dev, "int request failed, ret = %d\n", ret);
		if (info->client->irq < 0) {
			dev_err(dev, "failed to get irq no\n");
			gpio_free(info->irq_gpio);
		} else {
			ret = devm_request_threaded_irq(&info->client->dev, info->client->irq,
							NULL, upm691x_int_handler,
							IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
							"upm61x interrupt", info);
			if (ret)
				dev_err(info->dev, "Failed irq = %d ret = %d\n",
					info->client->irq, ret);
			else
				enable_irq_wake(client->irq);
		}
	} else {
		dev_err(dev, "failed to get irq gpio\n");
	}

	info->probe_initialized = true;
	complete_all(&info->probe_init);

	upm691x_check_charge_full(info);
	upm691x_dump_register(info);
	dev_info(dev, "use_typec_extcon = %d\n", info->use_typec_extcon);

	return 0;

error_upm_charger:
	charger_unregister(info->upm_charger);
out:
	mutex_destroy(&info->input_limit_cur_lock);
	mutex_destroy(&info->lock);

destroy_i2c_rw_lock:
	mutex_destroy(&info->i2c_rw_lock);

	return ret;
}

static void upm691x_charger_shutdown(struct i2c_client *client)
{
	struct upm691x_charger_info *info = i2c_get_clientdata(client);
	int ret = 0;

	if (!info) {
		pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
		return;
	}

	cancel_delayed_work_sync(&info->wdt_work);
	if (info->otg_enable) {
		info->otg_enable = false;
		cancel_delayed_work_sync(&info->otg_work);
		ret = upm691x_update_bits(info, UPM691X_REG_01,
					   UPM691X_REG_OTG_MASK,
					   0);
		if (ret)
			dev_err(info->dev, "disable upm691x otg failed ret = %d\n", ret);

		ret = upm691x_enter_hiz_mode(info);
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
//	if (info->charging) {
		ret = upm691x_charger_set_limit_current(info, UPM691X_SHUTDOWN_LIMIT, false);
		if (ret < 0)
			dev_err(info->dev, "%s: set input limit cur failed\n", __func__);

		ret = upm691x_charger_set_current(info, UPM691X_SHUTDOWN_CURRENT);
		if (ret < 0)
			dev_err(info->dev, "%s:set charge current failed\n", __func__);
//	}
}

static int upm691x_charger_remove(struct i2c_client *client)
{
	struct upm691x_charger_info *info = i2c_get_clientdata(client);

	if (!info)
		pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);

	cancel_delayed_work_sync(&info->wdt_work);
	cancel_delayed_work_sync(&info->otg_work);

	mutex_destroy(&info->input_limit_cur_lock);
	mutex_destroy(&info->lock);

	return 0;
}

#if IS_ENABLED(CONFIG_PM_SLEEP)
static int upm691x_charger_suspend(struct device *dev)
{
	int ret;
	ktime_t now, add;
	struct upm691x_charger_info *info = dev_get_drvdata(dev);

	if (!info) {
		pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
		return -EINVAL;
	}

	info->suspend = true;
	if (info->otg_enable || info->is_charger_online) {
		upm691x_charger_feed_watchdog(info);
		cancel_delayed_work_sync(&info->wdt_work);
	}

	if (!info->otg_enable)
		return 0;

	cancel_delayed_work_sync(&info->otg_work);

	if (info->disable_wdg) {
		ret = upm691x_charger_enable_wdg(info, false);
		if (ret)
			return -EBUSY;
	} else {
		dev_dbg(info->dev, "%s:line%d: set alarm\n", __func__, __LINE__);
		now = ktime_get_boottime();
		add = ktime_set(UPM691X_OTG_ALARM_TIMER_S, 0);
		alarm_start(&info->otg_timer, ktime_add(now, add));
	}

	return 0;
}

static int upm691x_charger_resume(struct device *dev)
{
	int ret = 0;
	struct upm691x_charger_info *info = dev_get_drvdata(dev);

	if (!info) {
		pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
		return -EINVAL;
	}

	if (info->otg_enable || info->is_charger_online) {
		upm691x_charger_feed_watchdog(info);
		schedule_delayed_work(&info->wdt_work, HZ * 15);
	}

	if (!info->otg_enable)
		goto done;

	schedule_delayed_work(&info->otg_work, msecs_to_jiffies(UPM691X_OTG_VALID_MS));

	if (info->disable_wdg) {
		ret = upm691x_charger_enable_wdg(info, true);
		if (ret)
			ret = -EBUSY;
	} else {
		alarm_cancel(&info->otg_timer);
	}

done:
	info->suspend = false;
	return ret;
}
#endif

static const struct dev_pm_ops upm691x_charger_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(upm691x_charger_suspend,
				upm691x_charger_resume)
};

static const struct i2c_device_id upm691x_i2c_id[] = {
	{"upm691x_chg", 0},
	{"upm691x_slave_chg", 0},
	{}
};

static const struct of_device_id upm691x_charger_of_match[] = {
	{ .compatible = "Upm,upm691x_chg", },
	{ .compatible = "Upm,upm691x_slave_chg", },
	{ }
};

MODULE_DEVICE_TABLE(of, upm691x_charger_of_match);

static struct i2c_driver upm691x_charger_driver = {
	.driver = {
		.name = "upm691x_chg",
		.of_match_table = upm691x_charger_of_match,
		.pm = &upm691x_charger_pm_ops,
	},
	.probe = upm691x_charger_probe,
	.shutdown = upm691x_charger_shutdown,
	.remove = upm691x_charger_remove,
	.id_table = upm691x_i2c_id,
};

module_i2c_driver(upm691x_charger_driver);
MODULE_DESCRIPTION("UPM691X Charger Driver");
MODULE_LICENSE("GPL");
