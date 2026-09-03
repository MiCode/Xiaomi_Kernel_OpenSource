// SPDX-License-Identifier: GPL-2.0:
// Copyright (c) 2024 unisoc.
/*
 * Driver for the Sgm sgm41513 charger.
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
#include <linux/of_gpio.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/power_supply.h>
#include <linux/power/charger-manager.h>
#include <linux/power/sprd_battery_info.h>
#include <linux/regmap.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>
#include <linux/slab.h>
#include <linux/sysfs.h>
#include <linux/pm_wakeup.h>
#include "lc_charger_class.h"

#define SGM41513_BATTERY_NAME			"sc27xx-fgu"

#define SGM41513_DEV_ID				0x0
#define SGM41513A_OR_D_DEV_ID			0x1
#define SGM41542S_DEV_ID            0xe

#define SGM41513_REG_0				0x0
#define SGM41513_REG_1				0x1
#define SGM41513_REG_2				0x2
#define SGM41513_REG_3				0x3
#define SGM41513_REG_4				0x4
#define SGM41513_REG_5				0x5
#define SGM41513_REG_6				0x6
#define SGM41513_REG_7				0x7
#define SGM41513_REG_8				0x8
#define SGM41513_REG_9				0x9
#define SGM41513_REG_A				0xa
#define SGM41513_REG_B				0xb
#define SGM41513_REG_C				0xc
#define SGM41513_REG_D				0xd
#define SGM41513_REG_E				0xe
#define SGM41513_REG_F				0xf
#define SGM41513_REG_10				0x10
#define SGM41513_REG_NUM			17

#define BIT_DP_DM_BC_ENB			BIT(0)
#define SGM41513_OTG_VALID_MS			500
#define SGM41513_FEED_WATCHDOG_VALID_MS		50
#define SGM41513_OTG_ALARM_TIMER_S		15
#define SGM41513_REG_IINLIM_BASE		100
#define SGM41513_REG_ICHG_LSB			60
#define SGM41513_REG_ICHG_MASK			GENMASK(5, 0)
#define SGM41513_REG_ICHG_SHIFT			0
#define SGM41513_REG_CHG_MASK			GENMASK(4, 4)
#define SGM41513_REG_CHG_SHIFT			4
#define SGM41513_REG_CHG_DISABLE		0
#define SGM41513_REG_CHG_ENABLE			1
#define SGM41513_REG_WD_RST_MASK		GENMASK(6, 6)
#define SGM41513_REG_WD_RST_SHIFT		6
#define SGM41513_REG_WD_RST				1
#define SGM41513_REG_OTG_MASK			GENMASK(5, 5)
#define SGM41513_REG_OTG_SHIFT			5
#define SGM41513_REG_OTG_ENABLE			1
#define SGM41513_REG_OTG_DISABLE		0
#define SGM41513_REG_BOOST_FAULT_MASK		GENMASK(6, 6)
#define SGM41513_REG_BOOST_LIMIT_MASK		GENMASK(7, 7)
#define SGM41513_REG_BOOST_LIMIT_SHIFT		7
#define SGM41513_REG_LIMIT_SEL_MASK		GENMASK(6, 6)
#define SGM41513_REG_LIMIT_SEL_SHIFT		6
#define SGM41513_REG_WATCHDOG_MASK		GENMASK(6, 6)
#define SGM41513_REG_WATCHDOG_TIMER_MASK	GENMASK(5, 4)
#define SGM41513_REG_WATCHDOG_TIMER_SHIFT	4
#define SGM41513_REG_EN_TIMER_MASK		GENMASK(3, 3)
#define SGM41513_REG_EN_TIMER_SHIFT		3
#define SGM41513_WDT_DISABLE			0
#define SGM41513_WDT_40S			1
#define SGM41513_REG_TERMINAL_VOLTAGE_MASK	GENMASK(7, 3)
#define SGM41513_REG_TERMINAL_VOLTAGE_SHIFT	3
#define SGM41513_REG_TERMINAL_VOLTAGE_BASE	3856
#define SGM41513_REG_TERMINAL_VOLTAGE_MAX	4624
#define SGM41513_REG_TERMINAL_VOLTAGE_LSB	32
#define SGM41513_REG_IPRECHG_CUR_MASK		GENMASK(7, 4)
#define SGM41513_REG_IPRECHG_CUR_SHIFT		4
#define SGM41513_REG_TERMINAL_CUR_MASK		GENMASK(3, 0)
#define SGM41513_REG_VINDPM_VOLTAGE_MASK	GENMASK(3, 0)
#define SGM41513_REG_OVP_MASK			GENMASK(7, 6)
#define SGM41513_REG_OVP_SHIFT			6
#define SGM41513_REG_OVP_5P5V			0
#define SGM41513_REG_OVP_6P5V			1
#define SGM41513_REG_OVP_10P5V			2
#define SGM41513_REG_OVP_14V			3
#define SGM41513_REG_EN_HIZ_MASK		GENMASK(7, 7)
#define SGM41513_REG_EN_HIZ_SHIFT		7
#define SGM41513_PN_MASK			0x78
#define SGM41513_PN_SHIFT			3
#define SGM41513_VINDPM_STATE_MASK		0x40 //GENMASK(6, 6)
#define SGM41513_VINDPM_STATE_SHIFT		6

#define SGM41513_DISABLE_BATFET_RST_MASK        BIT(2)
#define SGM41513_DISABLE_BATFET_RST_SHIFT       2
#define SGM41513_REG_LIMIT_CURRENT_MASK		GENMASK(4, 0)

#define SGM41513_DISABLE_PIN_MASK		BIT(0)
#define SGM41513_DISABLE_PIN_MASK_2730		BIT(0)
#define SGM41513_DISABLE_PIN_MASK_2721		BIT(15)
#define SGM41513_DISABLE_PIN_MASK_2720		BIT(0)

#define SGM41513_OTG_RETRY_TIMES		10
#define SGM41513_LIMIT_CURRENT_MAX		3200000
#define SGM41513_LIMIT_CURRENT_OFFSET		100000
#define SGM41513_REG_IINDPM_LSB			100
#define SGM41513_SHUTDOWN_LIMIT			1500000
#define SGM41513_SHUTDOWN_CURRENT		500000

#define SGM41513_ROLE_MASTER_DEFAULT		1
#define SGM41513_ROLE_SLAVE			2
#define SGM41513_FCHG_OVP_6V			6000
#define SGM41513_FCHG_OVP_9V			9000
#define SGM41513_FCHG_OVP_14V			14000
#define SGM41513_FAST_CHARGER_VOLTAGE_MAX	10500000
#define SGM41513_NORMAL_CHARGER_VOLTAGE_MAX	6500000
#define SGM41513_WAKE_UP_MS			1000
#define SGM41513_CURRENT_WORK_MS		100
#define SGM41513_WAIT_WL_VBUS_STABLE_CUR_THR	200000
#define SGM41513_PROBE_TIMEOUT			msecs_to_jiffies(500)
#define SGM41513_WATCH_DOG_TIME_OUT_MS		20000

#define SGM41513_REG4_VRECHG_200mV		1
#define SGM41513_REG4_VRECHG_MASK		0x01

#define SGM41513_REG6_BOOSTV_5P3V		0x30
#define SGM41513_REG6_BOOSTV_MASK		0x30

#define SGM41513_REG3_IPRECHG_OFFSET		60
#define SGM41513_REG3_IPRECHG_MIN		5
#define SGM41513_REG3_IPRECHG_MAX		240
#define SGM41513_REG3_IPRECHG_STEP		60

#define SGM41513_REG3_ITERM_OFFSET		60
#define SGM41513_REG3_ITERM_MIN			5
#define SGM41513_REG3_ITERM_MAX			240
#define SGM41513_REG3_ITERM_STEP		60

#define SGM41513_REG2_ICHG_MIN			0
#define SGM41513_REG2_ICHG_MAX			3000
#define SGM41513_REG2_ICHG_STEP			60

#define SGM41513_REG7_BATFET_DLY_SHIFT	3
#define SGM41513_REG7_BATFET_DLY_MASK	GENMASK(3, 3)
#define SGM41513_REG7_BATFET_DIS_SHIFT	5
#define SGM41513_REG7_BATFET_DIS_MASK	GENMASK(5, 5)
#define SGM41513_REG7_IINDET_EN_MASK    GENMASK(7, 7)
#define SGM41513_REG7_IINDET_EN_SHIFT		7
#define SGM41513_REG7_IINDET_EN_ENABLE		1
#define SGM41513_REG7_IINDET_EN_DISABLE		0

#define SGM41513_REG8_VBUS_STAT_MASK		0xE0
#define SGM41513_REG8_VBUS_STAT_SHIFT		5
#define SGM41513_REG8_PG_STAT_MASK		0x04
#define SGM41513_REG8_PG_STAT_SHIFT		2
#define SGM41513_REG8_PG_STAT_GOOD		1
#define SGM41513_VBUS_TYPE_NONE			0
#define SGM41513_VBUS_TYPE_USB			1
#define SGM41513_VBUS_TYPE_CDP			2
#define SGM41513_VBUS_TYPE_ADAPTER		3
#define SGM41513_VBUS_TYPE_UNKNOWN_ADAPTER	5
#define SGM41513_VBUS_TYPE_NON_STANDARD_ADAPTER	6
#define SGM41513_VBUS_TYPE_OTG			7
#define SGM41513_REGE_INPUT_DET_DONE_MASK	0x80
#define SGM41513_REGE_INPUT_DET_DONE_SHIFT	7
#define SGM41513_REGE_INPUT_DET_DONE		1

#define SGM41513_REGA_DPM_INT_MASK		GENMASK(1, 0)
#define SGM41513_REGA_DPM_VALUE			3

#define SGM41513_VINDPM_OS_MASK		GENMASK(1, 0)
#define SGM41513_VINDPM_OS_SHIFT	0
#define SGM41513_VINDPM_OS_3P9V		0
#define SGM41513_VINDPM_OS_5P9V		1
#define SGM41513_VINDPM_OS_7P5V		2
#define SGM41513_VINDPM_OS_10P5V	3
#define SGM41513_VINDPM_OS_3P9V_MAX	5400
#define SGM41513_VINDPM_OS_3P9V_BASE	3900
#define SGM41513_VINDPM_OS_5P9V_MAX	7400
#define SGM41513_VINDPM_OS_5P9V_BASE	5900
#define SGM41513_VINDPM_OS_7P5V_MAX	9000
#define SGM41513_VINDPM_OS_7P5V_BASE	7500
#define SGM41513_VINDPM_OS_10P5V_MAX	12000
#define SGM41513_VINDPM_OS_10P5V_BASE	10500
#define SGM41513_VINDPM_LSB		100

#define SGM41513_ITERM_PRE_SEL_MASK	GENMASK(0, 0)
#define SGM41513_ITERM_PRE_SEL_SHIFT	0
#define SGM41513_ITERM_PRE_SEL_ENABLE	1
#define SGM41513_ITERM_PRE_SEL_DISABLE	1

//reg_f
#define SGM4154X_VREG_FT_MASK			GENMASK(7, 6)
#define SGM4154X_VREG_FT_SHIT			6
#define SGM4154X_VREG_FT_DISABLE		0
#define SGM4154X_VREG_FT_ADD_8MV		1
#define SGM4154X_VREG_FT_DEC_8MV		2
#define SGM4154X_VREG_FT_DEC_16MV		3

#define SGM41513_REGA_VBUS_GD_MASK		0x80
#define SGM41513_REGA_VBUS_GD_SHIFT		7

#define SGM41513_REG01_PFM_DIS_MASK		0x80
#define SGM41513_REG01_PFM_DIS_SHIFT		7
#define SGM41513_REG01_PFM_ENABLE		0
#define SGM41513_REG01_PFM_DISABLE		1

#define SGM41513_REG08_CHRG_STAT_MASK		0x18
#define SGM41513_REG08_CHRG_STAT_SHIFT		3
#define SGM41513_REG08_CHRG_STAT_IDLE		0
#define SGM41513_REG08_CHRG_STAT_PRECHG		1
#define SGM41513_REG08_CHRG_STAT_FASTCHG		2
#define SGM41513_REG08_CHRG_STAT_CHGDONE		3

static bool boot_calibration = false;

/* SGM41513 Register 0x02 ICHG[5:0] */
static const u32 sgm41513_ichg[] = {
	0, 5, 10, 15, 20, 25, 30, 35,
	40, 50, 60, 70, 80, 90, 100, 110,
	130, 150, 170, 190, 210, 230, 250, 270,
	300, 330, 360, 390, 420, 450, 480, 510,
	540, 600, 660, 720, 780, 840, 900, 960, 1020, 1080, 1140, 1200, 1260, 1320, 1380, 1440,
	1500, 1620, 1740, 1860, 1980, 2100, 2220, 2340, 2460, 2580, 2700, 2820, 2940, 3000
};

/* SGM41513 Register 0x03 IPRECHG[7:4] or ITERM[3:0] */
static const u32 sgm41513_iprechg_iterm[] = {
	5, 10, 15, 20, 30, 40, 50, 60,
	80, 100, 120, 140, 160, 180, 200, 240
};

struct sgm41513_charger_sysfs {
	char *name;
	struct attribute_group attr_g;
	struct device_attribute attr_sgm41513_dump_reg;
	struct device_attribute attr_sgm41513_lookup_reg;
	struct device_attribute attr_sgm41513_sel_reg_id;
	struct device_attribute attr_sgm41513_reg_val;
	struct attribute *attrs[5];
	struct sgm41513_charger_info *info;
};

struct sgm41513_charge_current {
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

struct sgm41513_charger_info {
	struct i2c_client *client;
	struct device *dev;
	struct power_supply *psy_usb;
	struct sgm41513_charge_current cur;
	struct mutex lock;
	struct mutex input_limit_cur_lock;
	struct delayed_work otg_work;
	struct delayed_work wdt_work;
	struct delayed_work cur_work;
	struct regmap *pmic;
	struct gpio_desc *gpiod;
	struct extcon_dev *typec_extcon;
	struct alarm otg_timer;
	struct sgm41513_charger_sysfs *sysfs;
	struct completion probe_init;
	u32 charger_detect;
	struct charger_dev *sgm_charger;
	u32 charger_pd;
	u32 charger_pd_mask;
	u32 new_charge_limit_cur;
	u32 current_charge_limit_cur;
	u32 new_input_limit_cur;
	u32 current_input_limit_cur;
	u32 last_limit_cur;
	u32 actual_limit_cur;
	u32 role;
	u64 last_wdt_time;
	bool charging;
	bool need_disable_Q1;
	int termination_cur;
	int voltage_max_microvolt;
	int iprechg_cur;
	bool disable_wdg;
	bool otg_enable;
	unsigned int irq_gpio;
	bool is_wireless_charge;
	bool is_charger_online;
	int reg_id;
	bool disable_power_path;
	bool probe_initialized;
	bool use_typec_extcon;
	bool shutdown_flag;
	bool vbus_gd;
};

struct sgm41513_charger_reg_tab {
	int id;
	u32 addr;
	char *name;
};

static struct sgm41513_charger_reg_tab reg_tab[SGM41513_REG_NUM + 1] = {
	{0, SGM41513_REG_0, "EN_HIZ/EN_ICHG_MON/IINDPM"},
	{1, SGM41513_REG_1, "PFM _DIS/WD_RST/OTG_CONFIG/CHG_CONFIG/SYS_Min/Min_VBAT_SEL"},
	{2, SGM41513_REG_2, "BOOST_LIM/Q1_FULLON/ICHG"},
	{3, SGM41513_REG_3, "IPRECHG/ITERM"},
	{4, SGM41513_REG_4, "VREG/TOPOFF_TIMER/VRECHG"},
	{5, SGM41513_REG_5, "EN_TERM/WATCHDOG/EN_TIMER/CHG_TIMER/TREG/JEITA_ISET"},
	{6, SGM41513_REG_6, "OVP/BOOSTV/VINDPM"},
	{7, SGM41513_REG_7, "IINDET_EN/TMR2X_EN/BATFET_DIS/JEITA_VSET/BATFET_DLY/BATFET_RST_EN/VDPM_BAT_TRACK"},
	{8, SGM41513_REG_8, "VBUS_STAT/CHRG_STAT/PG_STAT/THERM_STAT/VSYS_STAT"},
	{9, SGM41513_REG_9, "WATCHDOG_FAULT/BOOST_FAULT/CHRG_FAULT/BAT_FAULT/NTC_FAULT"},
	{10, SGM41513_REG_A, "VBUS_GD/VINDPM_STAT/IINDPM_STAT/TOPOFF_ACTIVE/ACOV_STAT/VINDPM_INT_ MASK/IINDPM_INT_ MASK"},
	{11, SGM41513_REG_B, "REG_RST/PN/SGMPART/DEV_REV"},
	{12, SGM41513_REG_C, "JEITA_VSET_L/JEITA_ISET_L_EN/JEITA_ISET_H/JEITA_VT2/JEITA_VT3"},
	{13, SGM41513_REG_D, "EN_PUMPX/PUMPX_UP/PUMPX_DN/DP_VSET/DM_VSET"},
	{14, SGM41513_REG_E, "INPUT_DET_DONE"},
	{15, SGM41513_REG_F, "VREG_FT/ISHORT_SET/STAT_SET/VINDPM_OS"},
	{16, SGM41513_REG_10, "BOOST_LIM_SEL/BC1P2_AUTO_DIS/BATOCP/OTG_FREQ/ITERM_PRE"},
	{17, 0, "null"},
};

static bool enable_dump_stack;
module_param(enable_dump_stack, bool, 0644);
static int sgm41513_charger_set_power_path_status(struct sgm41513_charger_info *info, bool enable);
static int sgm41513_charger_set_vindpm(struct sgm41513_charger_info *info, u32 vol);

static void sgm41513_charger_dump_stack(void)
{
	if (enable_dump_stack)
		dump_stack();
}

static void power_path_control(struct sgm41513_charger_info *info)
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

static bool sgm41513_charger_is_bat_present(struct sgm41513_charger_info *info)
{
	struct power_supply *psy;
	union power_supply_propval val;
	bool present = false;
	int ret;

	psy = power_supply_get_by_name(SGM41513_BATTERY_NAME);
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

static int sgm41513_charger_is_fgu_present(struct sgm41513_charger_info *info)
{
	struct power_supply *psy;

	psy = power_supply_get_by_name(SGM41513_BATTERY_NAME);
	if (!psy) {
		dev_err(info->dev, "Failed to find psy of sc27xx_fgu\n");
		return -ENODEV;
	}
	power_supply_put(psy);

	return 0;
}

static int sgm41513_read(struct sgm41513_charger_info *info, u8 reg, u8 *data)
{
	int ret;

	ret = i2c_smbus_read_byte_data(info->client, reg);
	if (ret < 0)
		return ret;

	*data = ret;
	return 0;
}

static int sgm41513_write(struct sgm41513_charger_info *info, u8 reg, u8 data)
{
	return i2c_smbus_write_byte_data(info->client, reg, data);
}

static int sgm41513_update_bits(struct sgm41513_charger_info *info, u8 reg, u8 mask, u8 data)
{
	u8 v;
	int ret;

	ret = sgm41513_read(info, reg, &v);
	if (ret < 0)
		return ret;

	v &= ~mask;
	v |= (data & mask);

	return sgm41513_write(info, reg, v);
}

static u32 sgm41513_charger_get_limit_current(struct sgm41513_charger_info *info, u32 *limit_cur)
{
	u8 reg_val;
	int ret;
	ret = sgm41513_read(info, SGM41513_REG_0, &reg_val);
	if (ret < 0)
		return ret;
	reg_val &= SGM41513_REG_LIMIT_CURRENT_MASK;
	*limit_cur = reg_val * SGM41513_REG_IINLIM_BASE * 1000;
	*limit_cur += SGM41513_LIMIT_CURRENT_OFFSET;
	if (*limit_cur >= SGM41513_LIMIT_CURRENT_MAX)
		*limit_cur = SGM41513_LIMIT_CURRENT_MAX;
	return 0;
}

static int sgm41513_charger_set_limit_current(struct sgm41513_charger_info *info,
					     u32 limit_cur, bool enable)
{
	u8 reg_val;
	int ret = 0;

	mutex_lock(&info->input_limit_cur_lock);
	if (enable) {
		ret = sgm41513_charger_get_limit_current(info, &limit_cur);
		if (ret) {
			dev_err(info->dev, "get limit cur failed\n");
			goto out;
		}

		dev_info(info->dev, "%s, act_limit:%d, limit:%d \n", __func__, limit_cur, info->actual_limit_cur);
		if (limit_cur == info->actual_limit_cur)
			goto out;
		limit_cur = info->actual_limit_cur;
		dev_info(info->dev, "set limit current limit_cur = %d\n", limit_cur);
	}

	if (limit_cur >= SGM41513_LIMIT_CURRENT_MAX)
		limit_cur = SGM41513_LIMIT_CURRENT_MAX;
	if (limit_cur < SGM41513_LIMIT_CURRENT_OFFSET)
		limit_cur = SGM41513_LIMIT_CURRENT_OFFSET;
	info->last_limit_cur = limit_cur;
	limit_cur = limit_cur / 1000;
	reg_val = (limit_cur - SGM41513_REG_IINLIM_BASE) / SGM41513_REG_IINLIM_BASE;
	info->actual_limit_cur = reg_val * SGM41513_REG_IINLIM_BASE * 1000;
	info->actual_limit_cur += SGM41513_LIMIT_CURRENT_OFFSET;
	ret = sgm41513_update_bits(info, SGM41513_REG_0,
				  SGM41513_REG_LIMIT_CURRENT_MASK,
				  reg_val);
	if (ret)
		dev_err(info->dev, "set sgm41513 limit cur failed\n");

	if (limit_cur <= SGM41513_REG_IINLIM_BASE) {
		ret = sgm41513_charger_set_vindpm(info, 5400);
		if (ret)
			dev_err(info->dev, "%s set sgm41513 vindpm vol 5400 failed\n", __func__);
		else
			dev_err(info->dev, "%s set sgm41513 vindpm vol 5400 success\n", __func__);
	} else {
		ret = sgm41513_charger_set_vindpm(info, info->voltage_max_microvolt);
		if (ret)
			dev_err(info->dev, "%s set sgm41513 vindpm vol about %d failed\n", __func__, info->voltage_max_microvolt);
		else
			dev_err(info->dev, "%s set sgm41513 vindpm vol about %d success\n", __func__, info->voltage_max_microvolt);
	}

	dev_info(info->dev, "set limit_cur = %d, reg_val = %#x, actual_limit_cur = %d\n",
		 limit_cur, reg_val, info->actual_limit_cur);

out:
	mutex_unlock(&info->input_limit_cur_lock);

	return ret;
}

static int sgm41513_charger_set_ovp(struct sgm41513_charger_info *info, u32 vol)
{
	u8 reg_val;

	dev_dbg(info->dev, "%s:line%d: set ovp vol = %d\n", __func__, __LINE__, vol);
	if (vol < 5500)
		reg_val = SGM41513_REG_OVP_5P5V;
	else if (vol > 5500 && vol < 6500)
		reg_val = SGM41513_REG_OVP_6P5V;
	else if (vol > 6500 && vol < 10500)
		reg_val = SGM41513_REG_OVP_10P5V;
	else
		reg_val = SGM41513_REG_OVP_14V;
	return sgm41513_update_bits(info, SGM41513_REG_6,
				   SGM41513_REG_OVP_MASK,
				   reg_val << SGM41513_REG_OVP_SHIFT);
}

static int sgm41513_charger_set_dpm(struct sgm41513_charger_info *info)
{
	return sgm41513_update_bits(info, SGM41513_REG_A,
				   SGM41513_REGA_DPM_INT_MASK,
				   SGM41513_REGA_DPM_VALUE);
}

static int sgm41513_enable_charger(struct sgm41513_charger_info *info, bool enable)
{
	u8 val = SGM41513_REG_CHG_DISABLE;

	if (enable)
		val = SGM41513_REG_CHG_ENABLE;
	return sgm41513_update_bits(info, SGM41513_REG_1, SGM41513_REG_CHG_MASK,
				   val << SGM41513_REG_CHG_SHIFT);
}

static int sgm41513_check_charge_full(struct sgm41513_charger_info *info)
{
	int ret = 0;
	u8 reg_val = 0;
	ret = sgm41513_read(info, SGM41513_REG_8, &reg_val);
	if (ret < 0)
		return ret;

	reg_val &= SGM41513_REG08_CHRG_STAT_MASK;
	reg_val = reg_val >> SGM41513_REG08_CHRG_STAT_SHIFT;
	if (reg_val == SGM41513_REG08_CHRG_STAT_CHGDONE){
		pr_err("%s:charge is done, recharge in probe!\n",__func__);
		ret = sgm41513_enable_charger(info, false);
		if (ret) {
			dev_err(info->dev, "%s, failed to disable charger, ret = %d\n", __func__, ret);
			return ret;
		}
		msleep(10);
		ret = sgm41513_enable_charger(info, true);
		if (ret) {
			dev_err(info->dev, "%s, failed to enable charger, ret = %d\n", __func__, ret);
			return ret;
		}
	}
	return ret;
}

static int sgm41513_enable_pfm(struct sgm41513_charger_info *info, bool enable)
{
	u8 val = SGM41513_REG01_PFM_ENABLE;

	if (!enable)
		val = SGM41513_REG01_PFM_DISABLE;
	return sgm41513_update_bits(info, SGM41513_REG_1, SGM41513_REG01_PFM_DIS_MASK,
				   val << SGM41513_REG01_PFM_DIS_SHIFT);
}

static int sgm41513_charger_set_vindpm_th_base(struct sgm41513_charger_info *info,
					       int vindpm_th_base)
{
	u8 reg_val = SGM41513_VINDPM_OS_3P9V;

	if (vindpm_th_base >= SGM41513_VINDPM_OS_10P5V_BASE)
		reg_val = SGM41513_VINDPM_OS_10P5V;
	else if (vindpm_th_base >= SGM41513_VINDPM_OS_7P5V_BASE)
		reg_val = SGM41513_VINDPM_OS_7P5V;
	else if (vindpm_th_base >= SGM41513_VINDPM_OS_5P9V_BASE)
		reg_val = SGM41513_VINDPM_OS_5P9V;

	return sgm41513_update_bits(info, SGM41513_REG_F,
				    SGM41513_VINDPM_OS_MASK,
				    reg_val << SGM41513_VINDPM_OS_SHIFT);
}

static int sgm41513_charger_set_vindpm(struct sgm41513_charger_info *info, u32 vol)
{
	u8 reg_val;
	int ret;
	int vindpm_th_base = SGM41513_VINDPM_OS_3P9V_BASE;
	int vindpm_th_max = SGM41513_VINDPM_OS_3P9V_MAX;

	if (vol >= SGM41513_VINDPM_OS_10P5V_BASE) {
		vindpm_th_base = SGM41513_VINDPM_OS_10P5V_BASE;
		vindpm_th_max = SGM41513_VINDPM_OS_10P5V_MAX;
	} else if (vol >= SGM41513_VINDPM_OS_7P5V_BASE) {
		vindpm_th_base = SGM41513_VINDPM_OS_7P5V_BASE;
		vindpm_th_max = SGM41513_VINDPM_OS_7P5V_MAX;
	} else if (vol >= SGM41513_VINDPM_OS_5P9V_BASE) {
		vindpm_th_base = SGM41513_VINDPM_OS_5P9V_BASE;
		vindpm_th_max = SGM41513_VINDPM_OS_5P9V_MAX;
	}

	ret = sgm41513_charger_set_vindpm_th_base(info, vindpm_th_base);
	if (ret) {
		dev_err(info->dev, "%s, failed to set vindpm th base %d, ret=%d\n",
			__func__, vindpm_th_base, ret);
		return ret;
	}

	if (vol < vindpm_th_base)
		vol = vindpm_th_base;
	else if (vol > vindpm_th_max)
		vol = vindpm_th_max;

	reg_val = (vol - vindpm_th_base) / SGM41513_VINDPM_LSB;

	return sgm41513_update_bits(info, SGM41513_REG_6,
				   SGM41513_REG_VINDPM_VOLTAGE_MASK, reg_val);
}

static int sgm41513_get_charger_vindpm_state(struct sgm41513_charger_info *info, bool *vindpm_stat)
{
	u8 reg_val = 0;
	int ret = 0;

	if (!info) {
		pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
		return -EINVAL;
	}

	ret = sgm41513_read(info, SGM41513_REG_A, &reg_val);
	if (ret < 0)
		return ret;

	*vindpm_stat = (reg_val & SGM41513_VINDPM_STATE_MASK) >> SGM41513_VINDPM_STATE_SHIFT;
	if(*vindpm_stat == false) {
		*vindpm_stat = !info->vbus_gd;
		dev_info(info->dev,  "%s: reg0A = 0x%x, vbus_gd: %d \n", __func__, reg_val, info->vbus_gd);
	}

	return ret;
}

static int sgm41513_charger_set_termina_vol(struct sgm41513_charger_info *info, u32 vol)
{
	u8 reg_val, vreg_ft;
	int ret = 0;

	dev_dbg(info->dev, "%s:line%d: set termina vol = %d\n", __func__, __LINE__, vol);
	if (vol < SGM41513_REG_TERMINAL_VOLTAGE_BASE)
		vol = SGM41513_REG_TERMINAL_VOLTAGE_BASE;
	else if (vol > SGM41513_REG_TERMINAL_VOLTAGE_MAX)
		vol = SGM41513_REG_TERMINAL_VOLTAGE_MAX;

	/*
	 * Reason for rounding up: Avoid insufficiency problem.
	 *
	 * Example: Assume that the battery charging limit voltage
	 *          is 4.43V. If it is rounded down, the actual
	 *          charging limit voltage will be 4.40V, and the
	 *          problem of insufficient charging will occur.
	 */

	vreg_ft = (vol - SGM41513_REG_TERMINAL_VOLTAGE_BASE) % SGM41513_REG_TERMINAL_VOLTAGE_LSB;
	if (vreg_ft >= 24) {
		/* +32mV, -8mV */
		reg_val = (vol - SGM41513_REG_TERMINAL_VOLTAGE_BASE) / SGM41513_REG_TERMINAL_VOLTAGE_LSB;
		reg_val = (reg_val + 1) << SGM41513_REG_TERMINAL_VOLTAGE_SHIFT;
		ret = sgm41513_update_bits(info, SGM41513_REG_4, SGM41513_REG_TERMINAL_VOLTAGE_MASK, reg_val);
		if (ret)
			dev_err(info->dev, "set sgm41513 termina_vol SGM41513_REG_4 failed\n");
		ret = sgm41513_update_bits(info, SGM41513_REG_F, SGM4154X_VREG_FT_MASK,
								SGM4154X_VREG_FT_DEC_8MV << SGM4154X_VREG_FT_SHIT);
		if (ret)
			dev_err(info->dev, "set sgm41513 termina_vol SGM41513_REG_F failed\n");
		else
			dev_err(info->dev, "set SGM41513_REG_F bit[7:6]: 0x%x\n", SGM4154X_VREG_FT_DEC_8MV);
	} else if (vreg_ft >= 16) {
		/* +32mV, -16mV */
		reg_val = (vol - SGM41513_REG_TERMINAL_VOLTAGE_BASE) / SGM41513_REG_TERMINAL_VOLTAGE_LSB;
		reg_val = (reg_val + 1) << SGM41513_REG_TERMINAL_VOLTAGE_SHIFT;
		ret = sgm41513_update_bits(info, SGM41513_REG_4, SGM41513_REG_TERMINAL_VOLTAGE_MASK, reg_val);
		if (ret)
			dev_err(info->dev, "set sgm41513 termina_vol SGM41513_REG_4 failed\n");
		ret = sgm41513_update_bits(info, SGM41513_REG_F, SGM4154X_VREG_FT_MASK,
								SGM4154X_VREG_FT_DEC_16MV << SGM4154X_VREG_FT_SHIT);
		if (ret)
			dev_err(info->dev, "set sgm41513 termina_vol SGM41513_REG_F failed\n");
		else
			dev_err(info->dev, "set SGM41513_REG_F bit[7:6]: 0x%x\n", SGM4154X_VREG_FT_DEC_16MV);
	} else if (vreg_ft >= 8) {
		/* +8mV */
		reg_val = (vol - SGM41513_REG_TERMINAL_VOLTAGE_BASE) / SGM41513_REG_TERMINAL_VOLTAGE_LSB;
		reg_val <<= SGM41513_REG_TERMINAL_VOLTAGE_SHIFT;
		ret = sgm41513_update_bits(info, SGM41513_REG_4, SGM41513_REG_TERMINAL_VOLTAGE_MASK, reg_val);
		if (ret)
			dev_err(info->dev, "set sgm41513 termina_vol SGM41513_REG_4 failed\n");
		ret = sgm41513_update_bits(info, SGM41513_REG_F, SGM4154X_VREG_FT_MASK,
								SGM4154X_VREG_FT_ADD_8MV << SGM4154X_VREG_FT_SHIT);
		if (ret)
			dev_err(info->dev, "set sgm41513 termina_vol SGM41513_REG_F failed\n");
		else
			dev_err(info->dev, "set SGM41513_REG_F bit[7:6]: 0x%x\n", SGM4154X_VREG_FT_ADD_8MV);
	} else {
		reg_val = (vol - SGM41513_REG_TERMINAL_VOLTAGE_BASE) / SGM41513_REG_TERMINAL_VOLTAGE_LSB;
		reg_val <<= SGM41513_REG_TERMINAL_VOLTAGE_SHIFT;
		ret = sgm41513_update_bits(info, SGM41513_REG_4, SGM41513_REG_TERMINAL_VOLTAGE_MASK, reg_val);
		if (ret)
			dev_err(info->dev, "set sgm41513 termina_vol SGM41513_REG_4 failed\n");
		ret = sgm41513_update_bits(info, SGM41513_REG_F, SGM4154X_VREG_FT_MASK,
								SGM4154X_VREG_FT_DISABLE << SGM4154X_VREG_FT_SHIT);
		if (ret)
			dev_err(info->dev, "set sgm41513 termina_vol SGM41513_REG_F failed\n");
		else
			dev_err(info->dev, "set SGM41513_REG_F bit[7:6]: 0x%x\n", SGM4154X_VREG_FT_DISABLE);
	}
	return ret;
}

static int sgm41513_charger_iterm_pre_sel(struct sgm41513_charger_info *info, bool enable)
{
	u8 val = SGM41513_ITERM_PRE_SEL_DISABLE;

	if (enable)
		val = SGM41513_ITERM_PRE_SEL_ENABLE;
	return sgm41513_update_bits(info, SGM41513_REG_10, SGM41513_ITERM_PRE_SEL_MASK,
				   val << SGM41513_ITERM_PRE_SEL_SHIFT);
}

static int sgm41513_charger_get_iterm_pre_sel(struct sgm41513_charger_info *info, bool *enable)
{
	u8 reg_val = 0;
	int ret = 0;

	ret = sgm41513_read(info, SGM41513_REG_10, &reg_val);
	if (ret < 0)
		return ret;

	*enable= (reg_val & SGM41513_ITERM_PRE_SEL_MASK) >> SGM41513_ITERM_PRE_SEL_SHIFT;

	return ret;
}

static int sgm41513_charger_set_termina_cur(struct sgm41513_charger_info *info, u32 cur)
{
	u8 reg_val;
	int i, ret;
	bool enable;

	ret = sgm41513_charger_get_iterm_pre_sel(info, &enable);
	if (ret < 0)
		return ret;
	else if (enable)
		cur = cur / 6;

	if (cur < SGM41513_REG3_ITERM_MIN)
		cur = SGM41513_REG3_ITERM_MIN;
	else if (cur > SGM41513_REG3_ITERM_MAX)
		cur = SGM41513_REG3_ITERM_MAX;

	for (i = 0; i < ARRAY_SIZE(sgm41513_iprechg_iterm); i++) {
		if (cur <= sgm41513_iprechg_iterm[i])
			break;
	}
	if (i == ARRAY_SIZE(sgm41513_iprechg_iterm))
		reg_val = ARRAY_SIZE(sgm41513_iprechg_iterm) - 1;
	else
		reg_val = i;

	pr_info("%s:line%d:termina_cur[%d], reg_val[0x%2x]\n", __func__, __LINE__, cur, reg_val);
	return sgm41513_update_bits(info, SGM41513_REG_3,
				SGM41513_REG_TERMINAL_CUR_MASK,
				reg_val);
}

static int sgm41513_charger_set_iterm(struct sgm41513_charger_info *info, u32 cur)
{
	int ret;

	ret = sgm41513_charger_iterm_pre_sel(info, true);
	if (ret) {
		dev_err(info->dev, "set sgm41513 iterm_pre_sel failed\n");
		return ret;
	}

	ret = sgm41513_charger_set_termina_cur(info, cur);
	if (ret)
		dev_err(info->dev, "set sgm41513 terminal cur failed\n");
	else
		info->termination_cur = cur;

	return ret;
}

static int sgm41513_charger_set_iprechg_cur(struct sgm41513_charger_info *info, u32 cur)
{
	u8 reg_val;
	int i, ret;
	bool enable;

	ret = sgm41513_charger_get_iterm_pre_sel(info, &enable);
	if (ret < 0)
		return ret;
	else if (enable)
		cur = cur / 6;

	if (cur >= SGM41513_REG3_IPRECHG_MIN && cur <= SGM41513_REG3_IPRECHG_MAX) {
		for (i = 0; i < ARRAY_SIZE(sgm41513_iprechg_iterm); i++) {
			if (cur < sgm41513_iprechg_iterm[i])
				break;
		}
		if (i == 0)
			reg_val = 0;
		else
			reg_val = i - 1;

		pr_info("%s:line%d:iprechg cur[%d], reg_val[0x%2x]\n", __func__, __LINE__, cur, reg_val);
		return sgm41513_update_bits(info, SGM41513_REG_3,
					SGM41513_REG_IPRECHG_CUR_MASK,
					reg_val << SGM41513_REG_IPRECHG_CUR_SHIFT);
	} else {
		pr_err("%s:line%d:iprechg[%d] is overflow,min~max:[%d:%d]\n", __func__, __LINE__,
					cur, SGM41513_REG3_IPRECHG_MIN, SGM41513_REG3_IPRECHG_MAX);
		return -EOVERFLOW;
	}
}

static int sgm41513_charger_set_shipmode(struct sgm41513_charger_info *info, bool en)
{
  	int ret = 0;

  	pr_info("%s:shipmode en=%d\n", __func__, en);
  	if (en) {
  		ret = sgm41513_update_bits(info, SGM41513_REG_7, SGM41513_REG7_BATFET_DLY_MASK, 0);
  		ret |= sgm41513_update_bits(info, SGM41513_REG_7, SGM41513_REG7_BATFET_DIS_MASK, 1 << SGM41513_REG7_BATFET_DIS_SHIFT);
  	} else {
  		ret |= sgm41513_update_bits(info, SGM41513_REG_7, SGM41513_REG7_BATFET_DIS_MASK, 0);
  	}
  	return ret;
}

static int sgm41513_charger_hw_init(struct sgm41513_charger_info *info)
{
	struct sprd_battery_info bat_info = {};
	int termination_cur, iprechg_cur;
	int ret;

	pr_info("%s:line%d +++\n", __func__, __LINE__);
	ret = sprd_battery_get_battery_info(info->psy_usb, &bat_info, 0);
	if (ret) {
		dev_warn(info->dev, "no battery information is supplied\n");
		info->cur.sdp_limit = 500000;
		info->cur.sdp_cur = 500000;
		info->cur.dcp_limit = 2000000;
		info->cur.dcp_cur = 2000000;
		info->cur.cdp_limit = 900000;
		info->cur.cdp_cur = 900000;
		info->cur.unknown_limit = 1000000;
		info->cur.unknown_cur = 1000000;
		/*
		 * If no battery information is supplied, we should set
		 * default charge termination current to 120 mA, and default
		 * charge termination voltage to 4.44V.
		 */
		info->voltage_max_microvolt = 4440;
		termination_cur = 120;
		info->termination_cur = termination_cur;
		iprechg_cur = 240;
		info->iprechg_cur = iprechg_cur;
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
		info->voltage_max_microvolt = bat_info.constant_charge_voltage_max_uv / 1000;
		iprechg_cur = bat_info.precharge_current_ua / 1000;
		info->iprechg_cur = iprechg_cur;
		termination_cur = bat_info.charge_term_current_ua / 1000;
		info->termination_cur = termination_cur;
		sprd_battery_put_battery_info(info->psy_usb, &bat_info);
	}

	ret = sgm41513_charger_set_dpm(info);
	if (ret) {
		dev_err(info->dev, "set sgm41513 vindpm&&iindpm failed\n");
		return ret;
	}

	if (info->role == SGM41513_ROLE_MASTER_DEFAULT) {
		ret = sgm41513_charger_set_ovp(info, SGM41513_FCHG_OVP_6V);
		if (ret) {
			dev_err(info->dev, "set sgm41513 ovp failed\n");
			return ret;
		}
	} else if (info->role == SGM41513_ROLE_SLAVE) {
		ret = sgm41513_charger_set_ovp(info, SGM41513_FCHG_OVP_9V);
		if (ret) {
			dev_err(info->dev, "set sgm41513 slave ovp failed\n");
			return ret;
		}
	}
	ret = sgm41513_charger_set_vindpm(info, info->voltage_max_microvolt);
	if (ret) {
		dev_err(info->dev, "set sgm41513 vindpm vol failed\n");
		return ret;
	}
	ret = sgm41513_charger_iterm_pre_sel(info, true);
	if (ret) {
		dev_err(info->dev, "set sgm41513 iterm_pre_sel failed\n");
		return ret;
	}
	ret = sgm41513_charger_set_termina_vol(info, info->voltage_max_microvolt);
	if (ret) {
		dev_err(info->dev, "set sgm41513 terminal vol failed\n");
		return ret;
	}
	ret = sgm41513_charger_set_termina_cur(info, termination_cur);
	if (ret) {
		dev_err(info->dev, "set sgm41513 terminal cur failed\n");
		return ret;
	}
	ret = sgm41513_charger_set_iprechg_cur(info, iprechg_cur);
	if (ret) {
		dev_err(info->dev, "set sgm41513 iprechg cur failed\n");
		return ret;
	}
	ret = sgm41513_charger_set_limit_current(info, info->cur.unknown_cur, false);
	if (ret)
		dev_err(info->dev, "set sgm41513 limit current failed\n");

	ret = sgm41513_update_bits(info, SGM41513_REG_7,
				  SGM41513_DISABLE_BATFET_RST_MASK,
				  0x0 << SGM41513_DISABLE_BATFET_RST_SHIFT);
	if (ret)
		dev_err(info->dev, "disable batfet_rst_en failed\n");
	ret = sgm41513_update_bits(info, SGM41513_REG_5, SGM41513_REG_EN_TIMER_MASK,
				0 << SGM41513_REG_EN_TIMER_SHIFT);
	if (ret)
		dev_err(info->dev, "fail to set SGM41513_REG_EN_TIMER_MASK, ret = %d\n", ret);
	ret = sgm41513_update_bits(info, SGM41513_REG_4,
				  SGM41513_REG4_VRECHG_MASK,
				  SGM41513_REG4_VRECHG_200mV);

	if (ret)
		dev_err(info->dev, "set vrechg failed\n");

	ret = sgm41513_update_bits(info, SGM41513_REG_6,
				  SGM41513_REG6_BOOSTV_MASK,
				  SGM41513_REG6_BOOSTV_5P3V);
	if (ret)
		dev_err(info->dev, "set boostv failed\n");

	info->current_charge_limit_cur = SGM41513_REG_ICHG_LSB * 1000;
	info->current_input_limit_cur = SGM41513_REG_IINDPM_LSB * 1000;

	pr_info("%s:line%d ---\n", __func__, __LINE__);
	return ret;
}

static int sgm41513_charger_get_charge_voltage(struct sgm41513_charger_info *info, u32 *charge_vol)
{
	struct power_supply *psy;
	union power_supply_propval val;
	int ret;

	psy = power_supply_get_by_name(SGM41513_BATTERY_NAME);
	if (!psy) {
		dev_err(info->dev, "failed to get SGM41513_BATTERY_NAME\n");
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

static void sgm41513_dump_register(struct sgm41513_charger_info *info)
{
	int i, ret, len, idx = 0;
	u8 reg_val;
	char buf[SGM41513_REG_NUM * 17 + 1];

	memset(buf, '\0', sizeof(buf));
	for (i = 0; i < SGM41513_REG_NUM; i++) {
		if(i == 0x0e)
                  	continue;
		ret = sgm41513_read(info,  reg_tab[i].addr, &reg_val);
		if (ret == 0) {
			if(i == SGM41513_REG_A)
				info->vbus_gd = reg_val & 0x80;
			len = snprintf(buf + idx, sizeof(buf) - idx,
				       "[REG_0x%.2x]=0x%.2x  ",
				       reg_tab[i].addr, reg_val);
			idx += len;
		}
	}
	dev_info(info->dev, "%s: %s", __func__, buf);
}

static int sgm41513_charger_enable_wdg(struct sgm41513_charger_info *info, bool en)
{
	u8 val = SGM41513_WDT_DISABLE;

	if (en)
		val = SGM41513_WDT_40S;
	return sgm41513_update_bits(info, SGM41513_REG_5,
				   SGM41513_REG_WATCHDOG_TIMER_MASK,
				   val << SGM41513_REG_WATCHDOG_TIMER_SHIFT);
}

static int sgm41513_charger_start_charge(struct sgm41513_charger_info *info)
{
	int ret = 0;

	ret = sgm41513_update_bits(info, SGM41513_REG_0,
				  SGM41513_REG_EN_HIZ_MASK, 0);
	if (ret)
		dev_err(info->dev, "disable HIZ mode failed\n");

	ret = sgm41513_charger_enable_wdg(info, true);
	if (ret) {
		dev_err(info->dev, "%s, failed to enable watchdog, ret = %d\n", __func__, ret);
		return ret;
	}

	if (info->role == SGM41513_ROLE_MASTER_DEFAULT) {
		ret = regmap_update_bits(info->pmic, info->charger_pd,
					 info->charger_pd_mask, 0);
		if (ret) {
			dev_err(info->dev, "enable sgm41513 charge failed\n");
			return ret;
		}
	} else if (info->role == SGM41513_ROLE_SLAVE) {
		gpiod_set_value_cansleep(info->gpiod, 0);
	}

	ret = sgm41513_enable_charger(info, true);
	if (ret) {
		dev_err(info->dev, "%s, failed to enable charger, ret = %d\n", __func__, ret);
		return ret;
	}

	ret = sgm41513_charger_set_limit_current(info, info->last_limit_cur, false);
	if (ret) {
		dev_err(info->dev, "failed to set limit current\n");
		return ret;
	}

	ret = sgm41513_charger_iterm_pre_sel(info, true);
	if (ret) {
		dev_err(info->dev, "set sgm41513 iterm_pre_sel failed\n");
		return ret;
	}

	ret = sgm41513_charger_set_termina_cur(info, info->termination_cur);
	if (ret)
		dev_err(info->dev, "set sgm41513 terminal cur failed\n");

	ret = sgm41513_update_bits(info, SGM41513_REG_5, SGM41513_REG_EN_TIMER_MASK,
				0 << SGM41513_REG_EN_TIMER_SHIFT);
	if (ret)
		dev_err(info->dev, "fail to set SGM41513_REG_EN_TIMER_MASK, ret = %d\n", ret);
	ret = sgm41513_update_bits(info, SGM41513_REG_4,
				  SGM41513_REG4_VRECHG_MASK,
				  SGM41513_REG4_VRECHG_200mV);
	if (ret)
		dev_err(info->dev, "set vrechg failed\n");

	return ret;
}

static void sgm41513_charger_stop_charge(struct sgm41513_charger_info *info, bool present)
{
	int ret;

	dev_info(info->dev, "%s:line%d: stop charge\n", __func__, __LINE__);

	ret = sgm41513_enable_charger(info, false);
	if (ret)
		dev_err(info->dev, "%s, disable charger failed, ret = %d\n", __func__, ret);
	if (info->role == SGM41513_ROLE_MASTER_DEFAULT) {
		if ((!present || info->need_disable_Q1) && boot_calibration) {
			ret = sgm41513_update_bits(info, SGM41513_REG_0,
						  SGM41513_REG_EN_HIZ_MASK,
						  0x01 << SGM41513_REG_EN_HIZ_SHIFT);
			if (ret)
				dev_err(info->dev, "enable HIZ mode failed\n");
			info->need_disable_Q1 = false;
		}
		ret = regmap_update_bits(info->pmic, info->charger_pd, info->charger_pd_mask,
					 info->charger_pd_mask);
		if (ret)
			dev_err(info->dev, "disable sgm41513 charge_pd failed\n");
	} else if (info->role == SGM41513_ROLE_SLAVE) {
		if (!present && boot_calibration) {
			ret = sgm41513_update_bits(info, SGM41513_REG_0, SGM41513_REG_EN_HIZ_MASK,
						0x01 << SGM41513_REG_EN_HIZ_SHIFT);
			if (ret)
				dev_err(info->dev, "enable HIZ mode failed\n");
		}
		gpiod_set_value_cansleep(info->gpiod, 1);
	}

	if (info->disable_power_path) {
		ret = sgm41513_update_bits(info, SGM41513_REG_0,
					  SGM41513_REG_EN_HIZ_MASK,
					  0x01 << SGM41513_REG_EN_HIZ_SHIFT);
		if (ret)
			dev_err(info->dev, "Failed to disable power path\n");
	}

	ret = sgm41513_charger_enable_wdg(info, false);
	if (ret)
		dev_err(info->dev, "%s, failed to disable watchdog, ret = %d\n", __func__, ret);
}

static int sgm41513_charger_set_current(struct sgm41513_charger_info *info, u32 cur)
{
	u8 reg_val;
	int i;

	cur = cur / 1000;

	if (cur < SGM41513_REG2_ICHG_MIN)
		cur = SGM41513_REG2_ICHG_MIN;
	else if (cur > SGM41513_REG2_ICHG_MAX)
		cur = SGM41513_REG2_ICHG_MAX;

	for (i = 0; i < ARRAY_SIZE(sgm41513_ichg); i++) {
		if (cur < sgm41513_ichg[i])
			break;
	}

	if (i == 0)
		reg_val = 0;
	else
		reg_val = i - 1;

	pr_err("%s, set charge_cur:%u, reg_val:%d\n", __func__, cur, reg_val);

	return sgm41513_update_bits(info, SGM41513_REG_2,
				    SGM41513_REG_ICHG_MASK,
				    reg_val);
}

static int sgm41513_charger_get_current(struct sgm41513_charger_info *info, u32 *cur)
{
	u8 reg_val;
	int ret;
	ret = sgm41513_read(info, SGM41513_REG_2, &reg_val);
	if (ret < 0)
		return ret;
	reg_val &= SGM41513_REG_ICHG_MASK;
	reg_val = reg_val >> SGM41513_REG_ICHG_SHIFT;
	if (reg_val >= ARRAY_SIZE(sgm41513_ichg))
		reg_val = sgm41513_ichg[ARRAY_SIZE(sgm41513_ichg) - 1];

	*cur = sgm41513_ichg[reg_val] * 1000;
	return 0;
}

static int sgm41513_charger_get_health(struct sgm41513_charger_info *info, u32 *health)
{
	*health = POWER_SUPPLY_HEALTH_GOOD;

	return 0;
}

static int sgm41513_charger_feed_watchdog(struct sgm41513_charger_info *info)
{
	int ret = 0;
	u8 reg_val = SGM41513_REG_WD_RST << SGM41513_REG_WD_RST_SHIFT;
	u64 duration, curr = ktime_to_ms(ktime_get());

	dev_info(info->dev, "%s, start\n", __func__);

	ret = sgm41513_update_bits(info, SGM41513_REG_1,
				  SGM41513_REG_WD_RST_MASK,
				  reg_val);
	if (ret) {
		dev_err(info->dev, "reset sgm41513 failed\n");
		return ret;
	}

	duration = curr - info->last_wdt_time;
	if (duration >= SGM41513_WATCH_DOG_TIME_OUT_MS) {
		dev_err(info->dev, "charger wdg maybe time out:%lld ms\n", duration);
		sgm41513_dump_register(info);
	}

	info->last_wdt_time = curr;

	if (info->otg_enable)
		return ret;

	ret = sgm41513_charger_set_limit_current(info, info->actual_limit_cur, true);
	if (ret)
		dev_err(info->dev, "set limit cur failed\n");

	return ret;
}

static __attribute__((unused)) irqreturn_t sgm41513_int_handler(int irq, void *dev_id)
{
	struct sgm41513_charger_info *info = dev_id;

	if (!info) {
		pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
		return IRQ_HANDLED;
	}
	dev_info(info->dev, "interrupt occurs\n");
	sgm41513_dump_register(info);
	sgm41513_charger_feed_watchdog(info);
	return IRQ_HANDLED;
}

static int sgm41513_charger_get_status(struct sgm41513_charger_info *info)
{
	if (info->charging)
		return POWER_SUPPLY_STATUS_CHARGING;
	else
		return POWER_SUPPLY_STATUS_NOT_CHARGING;
}

static bool sgm41513_charger_get_power_path_status(struct sgm41513_charger_info *info)
{
	u8 value;
	int ret;
	bool power_path_enabled = true;
	ret = sgm41513_read(info, SGM41513_REG_0, &value);
	if (ret < 0) {
		dev_err(info->dev, "Fail to get power path status, ret = %d\n", ret);
		return power_path_enabled;
	}
	if (value & SGM41513_REG_EN_HIZ_MASK)
		power_path_enabled = false;
        dev_info(info->dev, "get HIZ mode: %s\n", power_path_enabled ? "Disable" : "Enable");
	return power_path_enabled;
}

static int sgm41513_charger_set_power_path_status(struct sgm41513_charger_info *info, bool enable)
{
	int ret = 0;
	u8 value = 0x1;
	dev_info(info->dev, "set HIZ mode: %s\n", enable ? "Disable" : "Enable");

	if (enable)
		value = 0;
	ret = sgm41513_update_bits(info, SGM41513_REG_0,
				  SGM41513_REG_EN_HIZ_MASK,
				  value << SGM41513_REG_EN_HIZ_SHIFT);
	if (ret)
		dev_err(info->dev, "%s HIZ mode failed, ret = %d\n",
			enable ? "Enable" : "Disable", ret);
	return ret;
}

static int sgm41513_charger_check_power_path_status(struct sgm41513_charger_info *info)
{
	int ret = 0;
	if (info->disable_power_path)
		return 0;
	if (sgm41513_charger_get_power_path_status(info))
		return 0;
	dev_info(info->dev, "%s:line%d, disable HIZ\n", __func__, __LINE__);
	ret = sgm41513_update_bits(info, SGM41513_REG_0,
				  SGM41513_REG_EN_HIZ_MASK, 0);
	if (ret)
		dev_err(info->dev, "disable HIZ mode failed, ret = %d\n", ret);
	return ret;
}

static void sgm41513_check_wireless_charge(struct sgm41513_charger_info *info, bool enable)
{
	int ret;
	if (!enable)
		cancel_delayed_work_sync(&info->cur_work);
	if (info->is_wireless_charge && enable) {
		cancel_delayed_work_sync(&info->cur_work);
		ret = sgm41513_charger_set_current(info, info->current_charge_limit_cur);
		if (ret < 0)
			dev_err(info->dev, "%s:set charge current failed\n", __func__);

		ret = sgm41513_charger_set_current(info, info->current_input_limit_cur);
		if (ret < 0)
			dev_err(info->dev, "%s:set charge current failed\n", __func__);

		pm_wakeup_event(info->dev, SGM41513_WAKE_UP_MS);
		schedule_delayed_work(&info->cur_work, msecs_to_jiffies(SGM41513_CURRENT_WORK_MS));
	} else if (info->is_wireless_charge && !enable) {
		info->new_charge_limit_cur = info->current_charge_limit_cur;
		info->current_charge_limit_cur = SGM41513_REG_ICHG_LSB * 1000;
		info->new_input_limit_cur = info->current_input_limit_cur;
		info->current_input_limit_cur = SGM41513_REG_IINDPM_LSB * 1000;
	} else if (!info->is_wireless_charge && !enable) {
		info->new_charge_limit_cur = SGM41513_REG_ICHG_LSB * 1000;
		info->current_charge_limit_cur = SGM41513_REG_ICHG_LSB * 1000;
		info->new_input_limit_cur = SGM41513_REG_IINDPM_LSB * 1000;
		info->current_input_limit_cur = SGM41513_REG_IINDPM_LSB * 1000;
	}
}

static int sgm41513_charger_set_status(struct sgm41513_charger_info *info,
				      int val, u32 input_vol, bool bat_present)
{
	int ret = 0;

	if (val == CM_BUCK_MAX_TERMINA_VOL) {
		ret = sgm41513_charger_set_termina_vol(info, SGM41513_REG_TERMINAL_VOLTAGE_MAX);
		if (ret) {
			dev_err(info->dev, "failed to set terminate max voltage\n");
			return ret;
		}
	} else if (val == CM_FAST_CHARGE_MAX_OVP_ENABLE_CMD) {
		ret = sgm41513_charger_set_ovp(info, SGM41513_FCHG_OVP_14V);
		if (ret) {
			dev_err(info->dev, "failed to set fast charge max ovp\n");
			return ret;
		}
	} else if (val == CM_FAST_CHARGE_OVP_ENABLE_CMD) {
		ret = sgm41513_charger_set_ovp(info, SGM41513_FCHG_OVP_9V);
		if (ret) {
			dev_err(info->dev, "failed to set fast charge 9V ovp\n");
			return ret;
		}
	} else if (val == CM_FAST_CHARGE_OVP_DISABLE_CMD) {
		ret = sgm41513_charger_set_ovp(info, SGM41513_FCHG_OVP_6V);
		if (ret) {
			dev_err(info->dev, "failed to set fast charge 5V ovp\n");
			return ret;
		}
		if (info->role == SGM41513_ROLE_MASTER_DEFAULT) {
			if (input_vol > SGM41513_FAST_CHARGER_VOLTAGE_MAX)
				info->need_disable_Q1 = true;
		}
	} else if ((val == false) && (info->role == SGM41513_ROLE_MASTER_DEFAULT)) {
		if (input_vol > SGM41513_NORMAL_CHARGER_VOLTAGE_MAX)
			info->need_disable_Q1 = true;
	}

	if (val > CM_FAST_CHARGE_NORMAL_CMD)
		return 0;

	if (!val && info->charging) {
		sgm41513_check_wireless_charge(info, false);
		sgm41513_charger_stop_charge(info, bat_present);
		info->charging = false;
	} else if (val && !info->charging) {
		sgm41513_check_wireless_charge(info, true);
		ret = sgm41513_charger_start_charge(info);
		if (ret)
			dev_err(info->dev, "start charge failed\n");
		else
			info->charging = true;
	}

	return ret;
}

static void sgm41513_current_work(struct work_struct *data)
{
	struct delayed_work *dwork = to_delayed_work(data);
	struct sgm41513_charger_info *info =
		container_of(dwork, struct sgm41513_charger_info, cur_work);
	int ret = 0, delay_work_ms = 10 * SGM41513_CURRENT_WORK_MS;
	bool need_return = false;

	if (!info) {
		pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
		return;
	}

	if (info->shutdown_flag)
		return;

	if (info->current_charge_limit_cur > info->new_charge_limit_cur) {
		ret = sgm41513_charger_set_current(info, info->new_charge_limit_cur);
		if (ret < 0)
			dev_err(info->dev, "%s: set charge limit cur failed\n", __func__);
		return;
	}

	if (info->current_input_limit_cur > info->new_input_limit_cur) {
		ret = sgm41513_charger_set_limit_current(info, info->new_input_limit_cur, false);
		if (ret < 0)
			dev_err(info->dev, "%s: set input limit cur failed\n", __func__);
		return;
	}

	if (info->current_charge_limit_cur + SGM41513_REG_ICHG_LSB * 1000 <=
	    info->new_charge_limit_cur)
		info->current_charge_limit_cur += SGM41513_REG_ICHG_LSB * 1000;
	else
		need_return = true;

	if (info->current_input_limit_cur + SGM41513_REG_IINDPM_LSB * 1000 <=
	    info->new_input_limit_cur)
		info->current_input_limit_cur += SGM41513_REG_IINDPM_LSB * 1000;
	else if (need_return)
		return;

	ret = sgm41513_charger_set_current(info, info->current_charge_limit_cur);
	if (ret < 0) {
		dev_err(info->dev, "set charge limit current failed\n");
		return;
	}

	ret = sgm41513_charger_set_limit_current(info, info->current_input_limit_cur, false);
	if (ret < 0) {
		dev_err(info->dev, "set input limit current failed\n");
		return;
	}

	dev_info(info->dev, "set charge_limit_cur %duA, input_limit_curr %duA\n",
		 info->current_charge_limit_cur, info->current_input_limit_cur);

	if (info->current_charge_limit_cur < SGM41513_WAIT_WL_VBUS_STABLE_CUR_THR)
		delay_work_ms = SGM41513_CURRENT_WORK_MS * 50;

	schedule_delayed_work(&info->cur_work, msecs_to_jiffies(delay_work_ms));
}

static bool sgm41513_probe_is_ready(struct sgm41513_charger_info *info)
{
	unsigned long timeout;

	if (unlikely(!info->probe_initialized)) {
		timeout = wait_for_completion_timeout(&info->probe_init, SGM41513_PROBE_TIMEOUT);
		if (!timeout) {
			dev_err(info->dev, "%s wait probe timeout\n", __func__);
			return false;
		}
	}

	return true;
}

static int sgm41513_get_usb_online(struct sgm41513_charger_info *info)
{
	int ret;
	bool vbus_gd = false;
	u8 reg_val = 0;
	int i;

	/* timeout: 20*5 = 100ms, if vbus is not good, wait 20ms retry */
	for (i = 0; i < 5; i++) {
		ret = sgm41513_read(info, SGM41513_REG_A, &reg_val);
		if (ret < 0) {
			dev_err(info->dev, "%s, read SGM41513_REG_A failed, ret = %d\n", __func__, ret);
			return 0;
		}

		vbus_gd = (reg_val & SGM41513_REGA_VBUS_GD_MASK) >> SGM41513_REGA_VBUS_GD_SHIFT;
		if (vbus_gd) {
			dev_info(info->dev, "%s: vbus is good\n", __func__);
			return 1;
		}
		msleep(20);
	}

	dev_info(info->dev,  "%s: vbus is not good\n", __func__);
	return 0;
}

static int sgm41513_get_auto_bc1p2_done(struct sgm41513_charger_info *info)
{
	int ret;
	u8 i;
	u8 reg_val = 0;
	u8 reg08_val = 0;
	bool bc12_done = false;
	bool pg_stat = false;

	/* timeout: 100*10 = 1000ms, if bc1.2 is not done, wait 100ms retry */
	for (i = 0; i < 10; i++) {
		msleep(100);
		if (!sgm41513_get_usb_online(info) && i > 3)
			return bc12_done;
		ret = sgm41513_read(info, SGM41513_REG_E, &reg_val);
		if (ret < 0) {
			dev_err(info->dev, "%s, read SGM41513_REG_E failed, ret = %d\n", __func__, ret);
			return bc12_done;
		}

		ret = sgm41513_read(info, SGM41513_REG_8, &reg08_val);
		if (ret < 0) {
			dev_err(info->dev, "%s, read SGM41513_REG_E failed, ret = %d\n", __func__, ret);
			return bc12_done;
		}

		bc12_done = (reg_val & SGM41513_REGE_INPUT_DET_DONE_MASK) >>
			SGM41513_REGE_INPUT_DET_DONE_SHIFT;
		pg_stat = (reg08_val & SGM41513_REG8_PG_STAT_MASK) >>
			SGM41513_REG8_PG_STAT_SHIFT;
		if (bc12_done == SGM41513_REGE_INPUT_DET_DONE || pg_stat == SGM41513_REG8_PG_STAT_GOOD) {
			dev_info(info->dev, "%s: auto bc1.2 is done\n", __func__);
			return bc12_done;
		}
	}
	dev_info(info->dev, "%s: auto bc1.2 is not done\n", __func__);
	return bc12_done;
}

static int sgm41513_get_force_bc1p2_done(struct sgm41513_charger_info *info)
{
	int ret;
	bool bc12_done = false;

	bc12_done = sgm41513_get_auto_bc1p2_done(info);

	/* force dpdm */
	ret = sgm41513_update_bits(info, SGM41513_REG_7, SGM41513_REG7_IINDET_EN_MASK,
		SGM41513_REG7_IINDET_EN_ENABLE << SGM41513_REG7_IINDET_EN_SHIFT);
	if (ret < 0)
		dev_err(info->dev, "%s, Set IINDET_EN 1 failed, ret = %d\n", __func__, ret);

	msleep(100);
	bc12_done = sgm41513_get_auto_bc1p2_done(info);

	return bc12_done;
}

static int sgm41513_get_bc1p2_result(struct sgm41513_charger_info *info, bool flag)
{
	int ret;
	u8 reg_val = 0;
	int vbus_stat;
	int chg_type = POWER_SUPPLY_USB_TYPE_UNKNOWN;

	ret = sgm41513_read(info, SGM41513_REG_8, &reg_val);
	if (ret < 0) {
		dev_err(info->dev, "%s, read SGM41513_REG_8 failed, ret = %d\n", __func__, ret);
		return chg_type;
	}

	vbus_stat = (reg_val & SGM41513_REG8_VBUS_STAT_MASK) >> SGM41513_REG8_VBUS_STAT_SHIFT;
	switch (vbus_stat) {
	case SGM41513_VBUS_TYPE_USB:
		chg_type = POWER_SUPPLY_USB_TYPE_SDP;
		break;
	case SGM41513_VBUS_TYPE_CDP:
		chg_type = POWER_SUPPLY_USB_TYPE_CDP;
		break;
	case SGM41513_VBUS_TYPE_ADAPTER:
		chg_type = POWER_SUPPLY_USB_TYPE_DCP;
		break;
	case SGM41513_VBUS_TYPE_UNKNOWN_ADAPTER:
		if (flag)
			chg_type = POWER_SUPPLY_USB_TYPE_C;
		else
			chg_type = POWER_SUPPLY_USB_TYPE_UNKNOWN;
		break;
	case SGM41513_VBUS_TYPE_NON_STANDARD_ADAPTER:
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

static int sgm41513_charger_is_present(struct sgm41513_charger_info *sgm)
{
	if (!sgm) {
		pr_err("%s: sgm is null\n", __func__);
		return 0;
	}
	return sgm41513_get_usb_online(sgm);
}

static int sgm41513_charger_chg_type_det(struct sgm41513_charger_info *sgm, bool force_dpdm)
{
	int ret;
	int chg_type = POWER_SUPPLY_USB_TYPE_UNKNOWN;

	if (!sgm) {
		pr_err("%s: sgm is null\n", __func__);
		return chg_type;
	}

	if (force_dpdm) {
		sprd_hsphy_set_high_impedance_state();
		msleep(100);
		ret = sgm41513_get_force_bc1p2_done(sgm);
		sprd_hsphy_cancel_high_impedance_state();
	} else {
		ret = sgm41513_get_auto_bc1p2_done(sgm);
	}
	chg_type = sgm41513_get_bc1p2_result(sgm, false);

	return chg_type;
}

static int sgm41513_charger_get_vbus_stat(struct sgm41513_charger_info *sgm)
{
	int ret;
	int chg_type = POWER_SUPPLY_USB_TYPE_UNKNOWN;

	if (!sgm) {
		pr_err("%s: sgm is null\n", __func__);
		return chg_type;
	}

	if (!sgm41513_get_usb_online(sgm))
		return chg_type;

	ret = sgm41513_get_auto_bc1p2_done(sgm);
	chg_type = sgm41513_get_bc1p2_result(sgm, true);

	return chg_type;
}

static int sgm41513_charger_first_bc1p2(struct sgm41513_charger_info *info, int *bc1p2_result)
{
	int ret = 0;
	int chg_type = POWER_SUPPLY_USB_TYPE_UNKNOWN;

	if (!info) {
		pr_err("%s: info is null\n", __func__);
		goto first_end;
	}

	ret = sgm41513_get_auto_bc1p2_done(info);
	chg_type = sgm41513_get_bc1p2_result(info, false);

first_end:
	*bc1p2_result = chg_type;
	return 0;
}

static int sgm41513_charger_retry_bc1p2(struct sgm41513_charger_info *info, int *bc1p2_result)
{
	*bc1p2_result = sgm41513_charger_chg_type_det(info, true);
	return 0;
}

static int sgm41513_charger_usb_get_property(struct power_supply *psy,
					    enum power_supply_property psp,
					    union power_supply_propval *val)
{
	struct sgm41513_charger_info *info = power_supply_get_drvdata(psy);
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

	if (!sgm41513_probe_is_ready(info)) {
		dev_err(info->dev, "%s wait probe timeout\n", __func__);
		return -EINVAL;
	}

	mutex_lock(&info->lock);
	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
		if (val->intval == CM_POWER_PATH_ENABLE_CMD ||
		    val->intval == CM_POWER_PATH_DISABLE_CMD) {
			val->intval = sgm41513_charger_get_power_path_status(info);
			break;
		} else if (val->intval == CM_BUCK_MAX_TERMINA_VOL) {
			val->intval = SGM41513_REG_TERMINAL_VOLTAGE_MAX * 1000;
			break;
		}

		val->intval = sgm41513_charger_get_status(info);
		break;
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT:
		if (!info->charging) {
			val->intval = 0;
		} else {
			ret = sgm41513_charger_get_current(info, &cur);
			if (ret)
				goto out;
			val->intval = cur;
		}
		break;
	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
		if (!info->charging) {
			val->intval = 0;
		} else {
			ret = sgm41513_charger_get_limit_current(info, &cur);
			if (ret)
				goto out;
			val->intval = cur;
		}
		break;
	case POWER_SUPPLY_PROP_HEALTH:
		if (info->charging) {
			val->intval = 0;
		} else {
			ret = sgm41513_charger_get_health(info, &health);
			if (ret)
				goto out;

			val->intval = health;
		}
		break;
	case POWER_SUPPLY_PROP_CALIBRATE:
		if (info->role == SGM41513_ROLE_MASTER_DEFAULT) {
			ret = regmap_read(info->pmic, info->charger_pd, &enabled);
			if (ret) {
				dev_err(info->dev, "get sgm41513 charge status failed\n");
				goto out;
			}
			val->intval = !(enabled & info->charger_pd_mask);
		} else if (info->role == SGM41513_ROLE_SLAVE) {
			enabled = gpiod_get_value_cansleep(info->gpiod);
			val->intval = !enabled;
		}
		break;
	case POWER_SUPPLY_PROP_USB_TYPE:
		val->intval = sgm41513_charger_chg_type_det(info, false);
		sgm41513_dump_register(info);
		break;
	case POWER_SUPPLY_PROP_MANUFACTURER:
		val->strval = "sgm41512s";
		break;
	default:
		ret = -EINVAL;
	}

out:
	mutex_unlock(&info->lock);
	return ret;
}

static int sgm41513_charger_usb_set_property(struct power_supply *psy,
					    enum power_supply_property psp,
					    const union power_supply_propval *val)
{
	struct sgm41513_charger_info *info = power_supply_get_drvdata(psy);
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
		bat_present = sgm41513_charger_is_bat_present(info);
		ret = sgm41513_charger_get_charge_voltage(info, &input_vol);
		if (ret) {
			input_vol = 0;
			dev_err(info->dev, "failed to get charge voltage! ret = %d\n", ret);
		}
	}

	if (!sgm41513_probe_is_ready(info)) {
		dev_err(info->dev, "%s wait probe timeout\n", __func__);
		return -EINVAL;
	}

	mutex_lock(&info->lock);

	switch (psp) {
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT:
		if (info->is_wireless_charge) {
			cancel_delayed_work_sync(&info->cur_work);
			info->new_charge_limit_cur = val->intval;
			pm_wakeup_event(info->dev, SGM41513_WAKE_UP_MS);
			schedule_delayed_work(&info->cur_work,
					      msecs_to_jiffies(SGM41513_CURRENT_WORK_MS * 2));
			break;
		}
		ret = sgm41513_charger_set_current(info, val->intval);
		if (ret < 0)
			dev_err(info->dev, "set charge current failed\n");
		break;
	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
		if (info->is_wireless_charge) {
			cancel_delayed_work_sync(&info->cur_work);
			info->new_input_limit_cur = val->intval;
			pm_wakeup_event(info->dev, SGM41513_WAKE_UP_MS);
			schedule_delayed_work(&info->cur_work,
					      msecs_to_jiffies(SGM41513_CURRENT_WORK_MS * 2));
			break;
		}
		ret = sgm41513_charger_set_limit_current(info, val->intval, false);
		if (ret < 0)
			dev_err(info->dev, "set input current limit failed\n");
		break;
	case POWER_SUPPLY_PROP_STATUS:
		if (val->intval == CM_POWER_PATH_ENABLE_CMD) {
			ret = sgm41513_charger_set_power_path_status(info, true);
			break;
		} else if (val->intval == CM_POWER_PATH_DISABLE_CMD) {
			ret = sgm41513_charger_set_power_path_status(info, false);
			break;
		}
		ret = sgm41513_charger_set_status(info, val->intval, input_vol, bat_present);
		if (ret < 0)
			dev_err(info->dev, "set charge status failed\n");
		break;
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE_MAX:
		ret = sgm41513_charger_set_termina_vol(info, val->intval / 1000);
		if (ret < 0)
			dev_err(info->dev, "failed to set terminate voltage\n");
		break;
	case POWER_SUPPLY_PROP_CALIBRATE:
		dev_info(info->dev, "POWER_SUPPLY_PROP_CHARGE_ENABLED = %d\n", val->intval);
		if (val->intval == true) {
			sgm41513_check_wireless_charge(info, true);
			ret = sgm41513_charger_start_charge(info);
			if (ret)
				dev_err(info->dev, "start charge failed\n");
		} else if (val->intval == false) {
			sgm41513_check_wireless_charge(info, false);
			sgm41513_charger_stop_charge(info, bat_present);
		}
		break;
	case POWER_SUPPLY_PROP_TYPE:
		if (val->intval == POWER_SUPPLY_WIRELESS_CHARGER_TYPE_UNKNOWN) {
			info->is_wireless_charge = true;
			ret = sgm41513_charger_set_ovp(info, SGM41513_FCHG_OVP_6V);
		} else if (val->intval == POWER_SUPPLY_WIRELESS_CHARGER_TYPE_BPP) {
			info->is_wireless_charge = true;
			ret = sgm41513_charger_set_ovp(info, SGM41513_FCHG_OVP_6V);
		} else if (val->intval == POWER_SUPPLY_WIRELESS_CHARGER_TYPE_EPP) {
			info->is_wireless_charge = true;
			ret = sgm41513_charger_set_ovp(info, SGM41513_FCHG_OVP_14V);
		} else {
			info->is_wireless_charge = false;
			ret = sgm41513_charger_set_ovp(info, SGM41513_FCHG_OVP_6V);
		}
		if (ret)
			dev_err(info->dev, "failed to set fast charge ovp\n");
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

static int sgm41513_charger_property_is_writeable(struct power_supply *psy,
						 enum power_supply_property psp)
{
	int ret;

	switch (psp) {
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT:
	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
	case POWER_SUPPLY_PROP_CALIBRATE:
	case POWER_SUPPLY_PROP_TYPE:
	case POWER_SUPPLY_PROP_STATUS:
	case POWER_SUPPLY_PROP_PRESENT:
		ret = 1;
		break;
	default:
		ret = 0;
	}

	return ret;
}

static enum power_supply_property sgm41513_usb_props[] = {
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT,
	POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT,
	POWER_SUPPLY_PROP_HEALTH,
	POWER_SUPPLY_PROP_CALIBRATE,
	POWER_SUPPLY_PROP_TYPE,
	POWER_SUPPLY_PROP_MANUFACTURER,
};

static const struct power_supply_desc sgm41513_charger_desc = {
	.name			= "sgm41513_charger",
	.type			= POWER_SUPPLY_TYPE_UNKNOWN,
	.properties		= sgm41513_usb_props,
	.num_properties		= ARRAY_SIZE(sgm41513_usb_props),
	.get_property		= sgm41513_charger_usb_get_property,
	.set_property		= sgm41513_charger_usb_set_property,
	.property_is_writeable	= sgm41513_charger_property_is_writeable,
};

static const struct power_supply_desc sgm41513_slave_charger_desc = {
	.name			= "sgm41513_slave_charger",
	.type			= POWER_SUPPLY_TYPE_UNKNOWN,
	.properties		= sgm41513_usb_props,
	.num_properties		= ARRAY_SIZE(sgm41513_usb_props),
	.get_property		= sgm41513_charger_usb_get_property,
	.set_property		= sgm41513_charger_usb_set_property,
	.property_is_writeable	= sgm41513_charger_property_is_writeable,
};

static ssize_t sgm41513_register_value_show(struct device *dev,
					   struct device_attribute *attr,
					   char *buf)
{
	struct sgm41513_charger_sysfs *sgm41513_sysfs =
		container_of(attr, struct sgm41513_charger_sysfs,
			     attr_sgm41513_reg_val);
	struct  sgm41513_charger_info *info =  sgm41513_sysfs->info;
	u8 val;
	int ret;

	if (!info)
		return snprintf(buf, PAGE_SIZE, "%s  sgm41513_sysfs->info is null\n", __func__);

	ret = sgm41513_read(info, reg_tab[info->reg_id].addr, &val);
	if (ret) {
		dev_err(info->dev, "fail to get  SGM41513_REG_0x%.2x value, ret = %d\n",
			reg_tab[info->reg_id].addr, ret);
		return snprintf(buf, PAGE_SIZE, "fail to get  SGM41513_REG_0x%.2x value\n",
			       reg_tab[info->reg_id].addr);
	}

	return snprintf(buf, PAGE_SIZE, "SGM41513_REG_0x%.2x = 0x%.2x\n",
			reg_tab[info->reg_id].addr, val);
}

static ssize_t sgm41513_register_value_store(struct device *dev,
					    struct device_attribute *attr,
					    const char *buf, size_t count)
{
	struct sgm41513_charger_sysfs *sgm41513_sysfs =
		container_of(attr, struct sgm41513_charger_sysfs,
			     attr_sgm41513_reg_val);
	struct sgm41513_charger_info *info = sgm41513_sysfs->info;
	u8 val;
	int ret;

	if (!info) {
		dev_err(dev, "%s sgm41513_sysfs->info is null\n", __func__);
		return count;
	}

	ret =  kstrtou8(buf, 16, &val);
	if (ret) {
		dev_err(info->dev, "fail to get addr, ret = %d\n", ret);
		return count;
	}

	ret = sgm41513_write(info, reg_tab[info->reg_id].addr, val);
	if (ret) {
		dev_err(info->dev, "fail to wite 0x%.2x to REG_0x%.2x, ret = %d\n",
				val, reg_tab[info->reg_id].addr, ret);
		return count;
	}

	dev_info(info->dev, "wite 0x%.2x to REG_0x%.2x success\n", val, reg_tab[info->reg_id].addr);
	return count;
}

static ssize_t sgm41513_register_id_store(struct device *dev,
					 struct device_attribute *attr,
					 const char *buf, size_t count)
{
	struct sgm41513_charger_sysfs *sgm41513_sysfs =
		container_of(attr, struct sgm41513_charger_sysfs,
			     attr_sgm41513_sel_reg_id);
	struct sgm41513_charger_info *info = sgm41513_sysfs->info;
	int ret, id;

	if (!info) {
		dev_err(dev, "%s sgm41513_sysfs->info is null\n", __func__);
		return count;
	}

	ret =  kstrtoint(buf, 10, &id);
	if (ret) {
		dev_err(info->dev, "%s store register id fail\n", sgm41513_sysfs->name);
		return count;
	}

	if (id < 0 || id >= SGM41513_REG_NUM) {
		dev_err(info->dev, "%s store register id fail, id = %d is out of range\n",
			sgm41513_sysfs->name, id);
		return count;
	}

	info->reg_id = id;
	dev_info(info->dev, "%s store register id = %d success\n", sgm41513_sysfs->name, id);

	return count;
}

static ssize_t sgm41513_register_id_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	struct sgm41513_charger_sysfs *sgm41513_sysfs =
		container_of(attr, struct sgm41513_charger_sysfs,
			     attr_sgm41513_sel_reg_id);
	struct sgm41513_charger_info *info = sgm41513_sysfs->info;

	if (!info)
		return snprintf(buf, PAGE_SIZE, "%s sgm41513_sysfs->info is null\n", __func__);

	return snprintf(buf, PAGE_SIZE, "Curent register id = %d\n", info->reg_id);
}

static ssize_t sgm41513_register_table_show(struct device *dev,
					   struct device_attribute *attr,
					   char *buf)
{
	struct sgm41513_charger_sysfs *sgm41513_sysfs =
		container_of(attr, struct sgm41513_charger_sysfs,
			     attr_sgm41513_lookup_reg);
	struct sgm41513_charger_info *info = sgm41513_sysfs->info;
	int i, len, idx = 0;
	char reg_tab_buf[1024];

	if (!info)
		return snprintf(buf, PAGE_SIZE, "%s sgm41513_sysfs->info is null\n", __func__);

	memset(reg_tab_buf, '\0', sizeof(reg_tab_buf));
	len = snprintf(reg_tab_buf + idx, sizeof(reg_tab_buf) - idx,
		       "Format: [id] [addr] [desc]\n");
	idx += len;
	for (i = 0; i < SGM41513_REG_NUM; i++) {
		len = snprintf(reg_tab_buf + idx, sizeof(reg_tab_buf) - idx,
			       "[%d] [REG_0x%.2x] [%s];\n",
			       reg_tab[i].id, reg_tab[i].addr, reg_tab[i].name);
		idx += len;
	}

	return snprintf(buf, PAGE_SIZE, "%s\n", reg_tab_buf);
}

static ssize_t sgm41513_dump_register_show(struct device *dev,
					  struct device_attribute *attr,
					  char *buf)
{
	struct sgm41513_charger_sysfs *sgm41513_sysfs =
		container_of(attr, struct sgm41513_charger_sysfs,
			     attr_sgm41513_dump_reg);
	struct sgm41513_charger_info *info = sgm41513_sysfs->info;

	if (!info)
		return snprintf(buf, PAGE_SIZE, "%s sgm41513_sysfs->info is null\n", __func__);

	sgm41513_dump_register(info);
	return snprintf(buf, PAGE_SIZE, "%s\n", sgm41513_sysfs->name);
}

static int sgm41513_register_sysfs(struct sgm41513_charger_info *info)
{
	struct sgm41513_charger_sysfs *sgm41513_sysfs;
	int ret;

	sgm41513_sysfs = devm_kzalloc(info->dev, sizeof(*sgm41513_sysfs), GFP_KERNEL);
	if (!sgm41513_sysfs)
		return -ENOMEM;
	info->sysfs = sgm41513_sysfs;
	sgm41513_sysfs->name = "sgm41513_sysfs";
	sgm41513_sysfs->info = info;
	sgm41513_sysfs->attrs[0] = &sgm41513_sysfs->attr_sgm41513_dump_reg.attr;
	sgm41513_sysfs->attrs[1] = &sgm41513_sysfs->attr_sgm41513_lookup_reg.attr;
	sgm41513_sysfs->attrs[2] = &sgm41513_sysfs->attr_sgm41513_sel_reg_id.attr;
	sgm41513_sysfs->attrs[3] = &sgm41513_sysfs->attr_sgm41513_reg_val.attr;
	sgm41513_sysfs->attrs[4] = NULL;
	sgm41513_sysfs->attr_g.name = "debug";
	sgm41513_sysfs->attr_g.attrs = sgm41513_sysfs->attrs;
	sysfs_attr_init(&sgm41513_sysfs->attr_sgm41513_dump_reg.attr);
	sgm41513_sysfs->attr_sgm41513_dump_reg.attr.name = "sgm41513_dump_reg";
	sgm41513_sysfs->attr_sgm41513_dump_reg.attr.mode = 0444;
	sgm41513_sysfs->attr_sgm41513_dump_reg.show = sgm41513_dump_register_show;
	sysfs_attr_init(&sgm41513_sysfs->attr_sgm41513_lookup_reg.attr);
	sgm41513_sysfs->attr_sgm41513_lookup_reg.attr.name = "sgm41513_lookup_reg";
	sgm41513_sysfs->attr_sgm41513_lookup_reg.attr.mode = 0444;
	sgm41513_sysfs->attr_sgm41513_lookup_reg.show = sgm41513_register_table_show;
	sysfs_attr_init(&sgm41513_sysfs->attr_sgm41513_sel_reg_id.attr);
	sgm41513_sysfs->attr_sgm41513_sel_reg_id.attr.name = "sgm41513_sel_reg_id";
	sgm41513_sysfs->attr_sgm41513_sel_reg_id.attr.mode = 0644;
	sgm41513_sysfs->attr_sgm41513_sel_reg_id.show = sgm41513_register_id_show;
	sgm41513_sysfs->attr_sgm41513_sel_reg_id.store = sgm41513_register_id_store;
	sysfs_attr_init(&sgm41513_sysfs->attr_sgm41513_reg_val.attr);
	sgm41513_sysfs->attr_sgm41513_reg_val.attr.name = "sgm41513_reg_val";
	sgm41513_sysfs->attr_sgm41513_reg_val.attr.mode = 0644;
	sgm41513_sysfs->attr_sgm41513_reg_val.show = sgm41513_register_value_show;
	sgm41513_sysfs->attr_sgm41513_reg_val.store = sgm41513_register_value_store;
	ret = sysfs_create_group(&info->psy_usb->dev.kobj, &sgm41513_sysfs->attr_g);
	if (ret < 0)
		dev_err(info->dev, "Cannot create sysfs , ret = %d\n", ret);

	return ret;
}

static void sgm41513_charger_feed_watchdog_work(struct work_struct *work)
{
	struct delayed_work *dwork = to_delayed_work(work);
	struct sgm41513_charger_info *info = container_of(dwork,
							 struct sgm41513_charger_info,
							 wdt_work);
	int ret;

	if (!info) {
		pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
		return;
	}

	ret = sgm41513_charger_feed_watchdog(info);
	if (ret)
		schedule_delayed_work(&info->wdt_work, HZ * 1);
	else
		schedule_delayed_work(&info->wdt_work, HZ * 15);
}

#if IS_ENABLED(CONFIG_REGULATOR)
static bool sgm41513_charger_check_otg_valid(struct sgm41513_charger_info *info)
{
	int ret;
	u8 value = 0;
	bool status = false;

	ret = sgm41513_read(info, SGM41513_REG_1, &value);
	if (ret) {
		dev_err(info->dev, "get sgm41513 charger otg valid status failed\n");
		return status;
	}

	if (value & SGM41513_REG_OTG_MASK)
		status = true;
	else
		dev_err(info->dev, "otg is not valid, REG_1 = 0x%x\n", value);
	return status;
}

static bool sgm41513_charger_check_otg_fault(struct sgm41513_charger_info *info)
{
	int ret;
	u8 value = 0;
	bool status = true;
	ret = sgm41513_read(info, SGM41513_REG_9, &value);
	if (ret) {
		dev_err(info->dev, "get sgm41513 charger otg fault status failed\n");
		return status;
	}
	if (!(value & SGM41513_REG_BOOST_FAULT_MASK))
		status = false;
	else
		dev_err(info->dev, "boost fault occurs, REG_9 = 0x%x\n", value);

	return status;
}

static void sgm41513_charger_otg_work(struct work_struct *work)
{
	struct delayed_work *dwork = to_delayed_work(work);
	struct sgm41513_charger_info *info = container_of(dwork,
			struct sgm41513_charger_info, otg_work);
	bool otg_valid = sgm41513_charger_check_otg_valid(info);
	bool otg_fault;
	int ret, retry = 0;

	if (!info) {
		pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
		return;
	}

	if (otg_valid)
		goto out;

	do {
		otg_fault = sgm41513_charger_check_otg_fault(info);
		if (!otg_fault) {
			dev_dbg(info->dev, "%s:line%d:restart charger otg\n", __func__, __LINE__);
			ret = sgm41513_update_bits(info, SGM41513_REG_1,
						  SGM41513_REG_OTG_MASK,
						  SGM41513_REG_OTG_ENABLE << SGM41513_REG_OTG_SHIFT
						  );

			if (ret)
				dev_err(info->dev, "restart sgm41513 charger otg failed\n");
		}
		otg_valid = sgm41513_charger_check_otg_valid(info);
	} while (!otg_valid && retry++ < SGM41513_OTG_RETRY_TIMES);

	if (retry >= SGM41513_OTG_RETRY_TIMES) {
		dev_err(info->dev, "Restart OTG failed\n");
		return;
	}

out:
	dev_dbg(info->dev, "%s:line%d:schedule_work\n", __func__, __LINE__);
	schedule_delayed_work(&info->otg_work, msecs_to_jiffies(1500));
}

static int sgm41513_charger_enable_otg(struct regulator_dev *dev)
{
	int temp, capacity;
	struct power_supply *batt_psy = NULL;
	union power_supply_propval val;
	struct sgm41513_charger_info *info = rdev_get_drvdata(dev);
	struct charger_manager *cm = NULL;
	int ret = 0;

	if (!info) {
		pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
		return -EINVAL;
	}

	if (info->shutdown_flag)
		return ret;

	sgm41513_charger_dump_stack();

	if (!sgm41513_probe_is_ready(info)) {
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
	if (temp > 0 && capacity > 15 && capacity <= 40) {
		ret = sgm41513_update_bits(info, SGM41513_REG_2,
				  SGM41513_REG_BOOST_LIMIT_MASK,
				  1 << SGM41513_REG_BOOST_LIMIT_SHIFT);
		ret |= sgm41513_update_bits(info, SGM41513_REG_10,
				  SGM41513_REG_LIMIT_SEL_MASK,
				  0);
	} else if (temp > 0 && capacity > 40) {
		ret = sgm41513_update_bits(info, SGM41513_REG_10,
				  SGM41513_REG_LIMIT_SEL_MASK,
				  1 << SGM41513_REG_LIMIT_SEL_SHIFT);
	} else {
		ret = sgm41513_update_bits(info, SGM41513_REG_2,
				  SGM41513_REG_BOOST_LIMIT_MASK,
				  0);
		ret |= sgm41513_update_bits(info, SGM41513_REG_10,
				  SGM41513_REG_LIMIT_SEL_MASK,
				  0);
	}

	cm = power_supply_get_drvdata(batt_psy);

	if (IS_ERR_OR_NULL(cm)) {
		pr_err("sgm41513 couldn't get cm of battery!\n");
		goto err;
	}

	if(cm->otg_debug){
		ret = sgm41513_update_bits(info, SGM41513_REG_10,
				  SGM41513_REG_LIMIT_SEL_MASK,
				  1 << SGM41513_REG_LIMIT_SEL_SHIFT);
	}

	if (ret) {
		dev_err(info->dev, "Set boost limit failed\n");
		goto err;
	}

err:
	ret = sgm41513_update_bits(info, SGM41513_REG_1,
				  SGM41513_REG_OTG_MASK,
				  SGM41513_REG_OTG_ENABLE << SGM41513_REG_OTG_SHIFT);
	if (ret) {
		dev_err(info->dev, "enable sgm41513 otg failed\n");
		regmap_update_bits(info->pmic, info->charger_detect, BIT_DP_DM_BC_ENB, 0);
		return ret;
	}

	ret = sgm41513_charger_enable_wdg(info, true);
	if (ret) {
		dev_err(info->dev, "%s, failed to enable watchdog, ret = %d\n", __func__, ret);
		return ret;
	}

	ret = sgm41513_charger_feed_watchdog(info);
	if (ret) {
		dev_err(info->dev, "%s, failed to feed watchdog, ret = %d\n", __func__, ret);
		return ret;
	}
	ret = sgm41513_charger_set_power_path_status(info, true);
	if (ret)
		dev_err(info->dev, "Failed to enable power path\n");

	info->otg_enable = true;
	info->last_wdt_time = ktime_to_ms(ktime_get());
	schedule_delayed_work(&info->wdt_work,
			      msecs_to_jiffies(SGM41513_FEED_WATCHDOG_VALID_MS));
	schedule_delayed_work(&info->otg_work,
			      msecs_to_jiffies(SGM41513_OTG_VALID_MS));
	dev_info(info->dev, "%s:line%d:enable_otg\n", __func__, __LINE__);

	return ret;
}

static int sgm41513_charger_disable_otg(struct regulator_dev *dev)
{
	struct sgm41513_charger_info *info = rdev_get_drvdata(dev);
	int ret = 0;

	if (!info) {
		pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
		return -EINVAL;
	}

	sgm41513_charger_dump_stack();

	if (!sgm41513_probe_is_ready(info)) {
		dev_err(info->dev, "%s wait probe timeout\n", __func__);
		return -EINVAL;
	}

	info->otg_enable = false;
	cancel_delayed_work_sync(&info->wdt_work);
	cancel_delayed_work_sync(&info->otg_work);
	ret = sgm41513_update_bits(info, SGM41513_REG_1,
				  SGM41513_REG_OTG_MASK,
				  SGM41513_REG_OTG_DISABLE << SGM41513_REG_OTG_SHIFT);
	if (ret) {
		dev_err(info->dev, "disable sgm41513 otg failed\n");
		return ret;
	}

	ret = sgm41513_charger_enable_wdg(info, false);
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

static int sgm41513_charger_vbus_is_enabled(struct regulator_dev *dev)
{
	struct sgm41513_charger_info *info = rdev_get_drvdata(dev);
	int ret;
	u8 val;

	if (!info) {
		pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
		return -EINVAL;
	}
	ret = sgm41513_read(info, SGM41513_REG_1, &val);
	if (ret) {
		dev_err(info->dev, "failed to get sgm41513 otg status\n");
		return ret;
	}
	val &= SGM41513_REG_OTG_MASK;
	val = (val >> SGM41513_REG_OTG_SHIFT) & 0x01;
	dev_dbg(info->dev, "%s:line%d:vbus_is_enabled\n", __func__, __LINE__);
	return val;
}

static const struct regulator_ops sgm41513_charger_vbus_ops = {
	.enable = sgm41513_charger_enable_otg,
	.disable = sgm41513_charger_disable_otg,
	.is_enabled = sgm41513_charger_vbus_is_enabled,
};

static const struct regulator_desc sgm41513_charger_vbus_desc = {
	.name = "otg-vbus",
	.of_match = "otg-vbus",
	.type = REGULATOR_VOLTAGE,
	.owner = THIS_MODULE,
	.ops = &sgm41513_charger_vbus_ops,
	.fixed_uV = 5000000,
	.n_voltages = 1,
};

static void sgm41513_charger_check_otg_status(struct sgm41513_charger_info *info)
{
	int ret;
	u8 val;
	ret = sgm41513_read(info, SGM41513_REG_1, &val);
	if (ret < 0) {
		dev_err(info->dev, "%s:line%d, failed to get reg1(%d)\n", __func__, __LINE__, ret);
		return;
	}
	if (val & SGM41513_REG_OTG_MASK) {
		dev_info(info->dev, "%s:line%d, exit otg mode\n", __func__, __LINE__);

		ret = sgm41513_update_bits(info, SGM41513_REG_1,
					SGM41513_REG_OTG_MASK,
					SGM41513_REG_OTG_DISABLE << SGM41513_REG_OTG_SHIFT);
		if (ret)
			dev_err(info->dev, "disable sgm41513 otg failed\n");
	}
}

static int sgm41513_charger_register_vbus_regulator(struct sgm41513_charger_info *info)
{
	struct regulator_config cfg = { };
	struct regulator_dev *reg;
	int ret = 0;

	/*
	 * only master to support otg
	 */
	if (info->role != SGM41513_ROLE_MASTER_DEFAULT)
		return 0;

	sgm41513_charger_check_otg_status(info);

	cfg.dev = info->dev;
	cfg.driver_data = info;
	reg = devm_regulator_register(info->dev, &sgm41513_charger_vbus_desc, &cfg);
	if (IS_ERR(reg)) {
		ret = PTR_ERR(reg);
		dev_err(info->dev, "Can't register regulator:%d\n", ret);
	}

	return ret;
}

static int sgm41513_charger_register_external_vbus_regulator(struct sgm41513_charger_info *info)
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
	if (info->role != SGM41513_ROLE_MASTER_DEFAULT)
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
	reg = devm_regulator_register(cfg.dev, &sgm41513_charger_vbus_desc, &cfg);
	if (IS_ERR(reg)) {
		ret = PTR_ERR(reg);
		dev_warn(info->dev, "%s, failed to register vddvbus regulator:%d\n",
			 __func__, ret);
	}

	return ret;
}

#else
static int sgm41513_charger_register_vbus_regulator(struct sgm41513_charger_info *info)
{
	return 0;
}

static int sgm41513_charger_register_external_vbus_regulator(struct sgm41513_charger_info *info)
{
	return 0;
}
#endif

static int sgm41513_charger_detect_device(struct sgm41513_charger_info *info)
{
	int ret, part_id;
	u8 reg_val;
	ret = sgm41513_read(info, SGM41513_REG_B, &reg_val);
	if (ret < 0) {
		dev_err(info->dev, "%s, failed to get device id, ret = %d\n", __func__, ret);
		return ret;
	}

	part_id = (reg_val & SGM41513_PN_MASK) >> SGM41513_PN_SHIFT;
	if (part_id != SGM41513_DEV_ID && part_id != SGM41513A_OR_D_DEV_ID && part_id != SGM41542S_DEV_ID) {
		dev_err(info->dev, "%s, the device id is 0x%x\n", __func__, part_id);
		return -EINVAL;
	}

	return ret;
}

static int sgm_chg_is_present(struct charger_dev *charger, int *present)
{
	struct sgm41513_charger_info *sgm = charger_get_private(charger);
	*present = sgm41513_charger_is_present(sgm);
	return 0;
}

static int sgm_chg_get_vindpm_state(struct charger_dev *charger, bool *state)
{
	struct sgm41513_charger_info *sgm = charger_get_private(charger);

	return sgm41513_get_charger_vindpm_state(sgm, state);
}

static int sgm_chg_set_shipmode(struct charger_dev *charger, bool en)
{
	struct sgm41513_charger_info *sgm = charger_get_private(charger);

	return sgm41513_charger_set_shipmode(sgm, en);
}

static int sgm_chg_first_bc1p2(struct charger_dev *charger, int *bc1p2_result)
{
	struct sgm41513_charger_info *sgm = charger_get_private(charger);

	return sgm41513_charger_first_bc1p2(sgm, bc1p2_result);
}

static int sgm_chg_retry_bc1p2(struct charger_dev *charger, int *bc1p2_result)
{
	struct sgm41513_charger_info *sgm = charger_get_private(charger);

	return sgm41513_charger_retry_bc1p2(sgm, bc1p2_result);
}

static int sgm_chg_get_vbus_type(struct charger_dev *charger, int *type)
{
	struct sgm41513_charger_info *sgm = charger_get_private(charger);
	*type = sgm41513_charger_get_vbus_stat(sgm);
	return 0;
}

static int sgm_chg_set_vindpm(struct charger_dev *charger, int vindpm)
{
	struct sgm41513_charger_info *sgm = charger_get_private(charger);
	sgm->voltage_max_microvolt = vindpm;
	return sgm41513_charger_set_vindpm(sgm, vindpm);
}

static int sgm_chg_get_iindpm(struct charger_dev *charger, int *iindpm)
{
	struct sgm41513_charger_info *sgm = charger_get_private(charger);

	return sgm41513_charger_get_limit_current(sgm, iindpm);
}

static int sgm_chg_set_iterm(struct charger_dev *charger, int iterm)
{
	struct sgm41513_charger_info *sgm = charger_get_private(charger);

	return sgm41513_charger_set_iterm(sgm, iterm);
}

static int sgm_chg_set_pfm(struct charger_dev *charger, bool en)
{
	struct sgm41513_charger_info *sgm = charger_get_private(charger);

	return sgm41513_enable_pfm(sgm, en);
}
/*****************************charger ops**************************************/
static struct charger_ops charger_ops = {
	.is_present = sgm_chg_is_present,
	.get_vindpm_state = sgm_chg_get_vindpm_state,
	.set_shipmode = sgm_chg_set_shipmode,
	.first_bc1p2 = sgm_chg_first_bc1p2,
	.retry_bc1p2 = sgm_chg_retry_bc1p2,
	.get_vbus_type = sgm_chg_get_vbus_type,
	.set_vindpm = sgm_chg_set_vindpm,
	.get_iindpm = sgm_chg_get_iindpm,
	.set_iterm = sgm_chg_set_iterm,
	.set_pfm = sgm_chg_set_pfm,
};


static int sgm41513_charger_probe(struct i2c_client *client,
		const struct i2c_device_id *id)
{
	struct i2c_adapter *adapter = to_i2c_adapter(client->dev.parent);
	struct device *dev = &client->dev;
	struct power_supply_config charger_cfg = { };
	struct sgm41513_charger_info *info;
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

	ret = sgm41513_charger_detect_device(info);
	if (ret) {
		dev_err(dev, "%s, failed to detect device, ret = %d\n", __func__, ret);
		return -ENODEV;
	}

	power_path_control(info);

	ret = sgm41513_charger_is_fgu_present(info);
	if (ret) {
		dev_err(dev, "sc27xx_fgu not ready.\n");
		return -EPROBE_DEFER;
	}

	info->use_typec_extcon = device_property_read_bool(dev, "use-typec-extcon");
	info->disable_wdg = device_property_read_bool(dev, "disable-otg-wdg-in-sleep");

	ret = device_property_read_bool(dev, "role-slave");
	if (ret)
		info->role = SGM41513_ROLE_SLAVE;
	else
		info->role = SGM41513_ROLE_MASTER_DEFAULT;
	if (info->role == SGM41513_ROLE_SLAVE) {
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
			info->charger_pd_mask = SGM41513_DISABLE_PIN_MASK_2730;
		else if (of_device_is_compatible(regmap_np->parent, "sprd,sc2721"))
			info->charger_pd_mask = SGM41513_DISABLE_PIN_MASK_2721;
		else if (of_device_is_compatible(regmap_np->parent, "sprd,sc2720"))
			info->charger_pd_mask = SGM41513_DISABLE_PIN_MASK_2720;
		else if (of_device_is_compatible(regmap_np, "sprd,ump962x-syscon"))
			info->charger_pd_mask = SGM41513_DISABLE_PIN_MASK;
		else {
			dev_err(dev, "failed to get charger_pd mask\n");
			info->charger_pd_mask = SGM41513_DISABLE_PIN_MASK;
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
	if (info->role == SGM41513_ROLE_MASTER_DEFAULT) {
		info->psy_usb = devm_power_supply_register(dev,
							   &sgm41513_charger_desc,
							   &charger_cfg);
	} else if (info->role == SGM41513_ROLE_SLAVE) {
		info->psy_usb = devm_power_supply_register(dev,
							   &sgm41513_slave_charger_desc,
							   &charger_cfg);
	}

	if (IS_ERR(info->psy_usb)) {
		dev_err(dev, "failed to register power supply\n");
		ret = PTR_ERR(info->psy_usb);
		goto err_regmap_exit;
	}

	ret = sgm41513_charger_hw_init(info);
	if (ret) {
		dev_err(dev, "failed to sgm41513_charger_hw_init\n");
		goto err_psy_usb;
	}

	sgm41513_charger_check_power_path_status(info);

	device_init_wakeup(info->dev, true);

	alarm_init(&info->otg_timer, ALARM_BOOTTIME, NULL);
	INIT_DELAYED_WORK(&info->otg_work, sgm41513_charger_otg_work);
	INIT_DELAYED_WORK(&info->wdt_work, sgm41513_charger_feed_watchdog_work);
	INIT_DELAYED_WORK(&info->cur_work, sgm41513_current_work);

	if (device_property_read_bool(dev, "otg-vbus-node-external"))
		ret = sgm41513_charger_register_external_vbus_regulator(info);
	else
		ret = sgm41513_charger_register_vbus_regulator(info);

	if (ret) {
		dev_err(dev, "failed to register vbus regulator.\n");
		goto err_psy_usb;
	}

	ret = sgm41513_register_sysfs(info);
	if (ret) {
		dev_err(info->dev, "register sysfs fail, ret = %d\n", ret);
		goto error_sysfs;
	}

	info->sgm_charger = charger_register("master_chg", info->dev, &charger_ops, info);
	if (!info->sgm_charger) {
		ret = PTR_ERR(info->sgm_charger);
		goto error_sgm_charger;
	}

	info->irq_gpio = of_get_named_gpio(info->dev->of_node, "irq-gpio", 0);
	if (gpio_is_valid(info->irq_gpio)) {
		ret = devm_gpio_request_one(info->dev, info->irq_gpio,
					    GPIOF_DIR_IN, "sgm41513_int");
		if (!ret)
			info->client->irq = gpio_to_irq(info->irq_gpio);
		else
			dev_err(dev, "int request failed, ret = %d\n", ret);
		if (info->client->irq < 0) {
			dev_err(dev, "failed to get irq no\n");
			gpio_free(info->irq_gpio);
		} else {
			ret = devm_request_threaded_irq(&info->client->dev, info->client->irq,
							NULL, sgm41513_int_handler,
							IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
							"sgm41513 interrupt", info);
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

	sgm41513_check_charge_full(info);
	sgm41513_dump_register(info);
	dev_info(dev, "use_typec_extcon = %d\n", info->use_typec_extcon);

	return 0;
error_sgm_charger:
	charger_unregister(info->sgm_charger);
error_sysfs:
	sysfs_remove_group(&info->psy_usb->dev.kobj, &info->sysfs->attr_g);
err_psy_usb:
	if (info->irq_gpio)
		gpio_free(info->irq_gpio);
err_regmap_exit:
	mutex_destroy(&info->input_limit_cur_lock);
	mutex_destroy(&info->lock);
	return ret;
}

static void sgm41513_charger_shutdown(struct i2c_client *client)
{
	struct sgm41513_charger_info *info = i2c_get_clientdata(client);
	int ret = 0;

	if (!info) {
		pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
		return;
	}

	if (info->is_wireless_charge)
		cancel_delayed_work_sync(&info->cur_work);

	cancel_delayed_work_sync(&info->wdt_work);

	if (info->otg_enable) {
		info->otg_enable = false;
		cancel_delayed_work_sync(&info->otg_work);
		ret = sgm41513_update_bits(info, SGM41513_REG_1,
					  SGM41513_REG_OTG_MASK,
					  0);
		if (ret)
			dev_err(info->dev, "disable sgm41513 otg failed ret = %d\n", ret);
		ret = sgm41513_charger_set_power_path_status(info, false);
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
		ret = sgm41513_charger_set_limit_current(info, SGM41513_SHUTDOWN_LIMIT, false);
		if (ret < 0)
			dev_err(info->dev, "%s: set input limit cur failed\n", __func__);

		ret = sgm41513_charger_set_current(info, SGM41513_SHUTDOWN_CURRENT);
		if (ret < 0)
			dev_err(info->dev, "%s:set charge current failed\n", __func__);
//	}
}

static int sgm41513_charger_remove(struct i2c_client *client)
{
	struct sgm41513_charger_info *info = i2c_get_clientdata(client);

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
static int sgm41513_charger_suspend(struct device *dev)
{
	ktime_t now, add;
	struct sgm41513_charger_info *info = dev_get_drvdata(dev);

	if (!info) {
		pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
		return -EINVAL;
	}

	if (info->otg_enable || info->is_charger_online) {
		if (sgm41513_charger_feed_watchdog(info))
			dev_err(info->dev, "%s, failed to feed watchdog\n", __func__);

		cancel_delayed_work_sync(&info->wdt_work);
	}

	if (!info->otg_enable)
		return 0;

	if (info->is_wireless_charge)
		cancel_delayed_work_sync(&info->cur_work);

	if (info->disable_wdg) {
		if (sgm41513_charger_enable_wdg(info, false)) {
			dev_err(info->dev, "%s, failed to disable watchdog\n", __func__);
			return -EBUSY;
		}
	} else {
		now = ktime_get_boottime();
		add = ktime_set(SGM41513_OTG_ALARM_TIMER_S, 0);
		alarm_start(&info->otg_timer, ktime_add(now, add));
		pr_info("sgm41513_charger set alarm, triggered at [%lld]ms\n",
			ktime_to_ms(ktime_add(now, add)));
	}

	return 0;
}

static int sgm41513_charger_resume(struct device *dev)
{
	struct sgm41513_charger_info *info = dev_get_drvdata(dev);

	if (!info) {
		pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
		return -EINVAL;
	}

	if (info->otg_enable || info->is_charger_online) {
		if (sgm41513_charger_feed_watchdog(info))
			dev_err(info->dev, "%s, failed to feed watchdog\n", __func__);

		schedule_delayed_work(&info->wdt_work, HZ * 15);
	}

	if (!info->otg_enable)
		return 0;

	if (info->disable_wdg) {
		if (sgm41513_charger_enable_wdg(info, true)) {
			dev_err(info->dev, "%s, failed to enable watchdog\n", __func__);
			return -EBUSY;
		}
	} else {
		alarm_cancel(&info->otg_timer);
	}

	if (info->is_wireless_charge)
		schedule_delayed_work(&info->cur_work, 0);

	return 0;
}
#endif

static const struct dev_pm_ops sgm41513_charger_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(sgm41513_charger_suspend,
				sgm41513_charger_resume)
};

static const struct i2c_device_id sgm41513_i2c_id[] = {
	{"sgm41513_chg", 0},
	{"sgm41513_slave_chg", 0},
	{}
};

static const struct of_device_id sgm41513_charger_of_match[] = {
	{ .compatible = "Sgm,sgm41513_chg", },
	{ .compatible = "Sgm,sgm41513_slave_chg", },
	{ }
};

MODULE_DEVICE_TABLE(of, sgm41513_charger_of_match);

static struct i2c_driver sgm41513_charger_driver = {
	.driver = {
		.name = "sgm41513_chg",
		.of_match_table = sgm41513_charger_of_match,
		.pm = &sgm41513_charger_pm_ops,
	},
	.probe = sgm41513_charger_probe,
	.shutdown = sgm41513_charger_shutdown,
	.remove = sgm41513_charger_remove,
	.id_table = sgm41513_i2c_id,
};

module_i2c_driver(sgm41513_charger_driver);
MODULE_DESCRIPTION("SGM41513 Charger Driver");
MODULE_LICENSE("GPL v2");
