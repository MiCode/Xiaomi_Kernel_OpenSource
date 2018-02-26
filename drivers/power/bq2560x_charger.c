/*
 * BQ2560x battery charging driver
 *
 * Copyright (C) 2013 Texas Instruments
 * Copyright (C) 2018 XiaoMi, Inc.
 *
 * This package is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.

 * THIS PACKAGE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

#define pr_fmt(fmt)	"bq2560x: %s: " fmt, __func__

#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/power_supply.h>
#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/err.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/of_regulator.h>
#include <linux/regulator/machine.h>
#include <linux/debugfs.h>
#include <linux/bitops.h>
#include <linux/math64.h>
#include <linux/alarmtimer.h>

#include "bq2560x_reg.h"
#include "bq2560x.h"

#if 1
#undef pr_debug
#define pr_debug pr_err
#undef pr_info
#define pr_info pr_err
#undef dev_dbg
#define dev_dbg dev_err
#else
#undef pr_info
#define pr_info pr_debug
#endif

enum bq2560x_vbus_type {
	BQ2560X_VBUS_NONE = REG08_VBUS_TYPE_NONE,
	BQ2560X_VBUS_USB = REG08_VBUS_TYPE_USB,
	BQ2560X_VBUS_ADAPTER = REG08_VBUS_TYPE_ADAPTER,
	BQ2560X_VBUS_OTG = REG08_VBUS_TYPE_OTG,
};

enum bq2560x_part_no {
	BQ25600 = 0x00,
	BQ25601 = 0x02,
};

enum {
	USER		= BIT(0),
	JEITA		= BIT(1),
	BATT_FC		= BIT(2),
	BATT_PRES		= BIT(3),
	SOC		= BIT(4),
};

enum wakeup_src {
	WAKEUP_SRC_MONITOR = 0,
	WAKEUP_SRC_JEITA,
	WAKEUP_SRC_MAX,
};

#define WAKEUP_SRC_MASK (~(~0 << WAKEUP_SRC_MAX))
struct bq2560x_wakeup_source {
	struct wakeup_source source;
	unsigned long enabled_bitmap;
	spinlock_t ws_lock;
};

enum bq2560x_charge_state {
	CHARGE_STATE_IDLE = REG08_CHRG_STAT_IDLE,
	CHARGE_STATE_PRECHG = REG08_CHRG_STAT_PRECHG,
	CHARGE_STATE_FASTCHG = REG08_CHRG_STAT_FASTCHG,
	CHARGE_STATE_CHGDONE = REG08_CHRG_STAT_CHGDONE,
};

struct bq2560x_otg_regulator {
	struct regulator_desc	rdesc;
	struct regulator_dev	*rdev;
};


struct bq2560x {
	struct device *dev;
	struct i2c_client *client;

	enum bq2560x_part_no part_no;
	int	revision;

	int gpio_ce;

	int	vbus_type;

	int status;

	struct mutex data_lock;
	struct mutex i2c_rw_lock;
	struct mutex profile_change_lock;
	struct mutex charging_disable_lock;
	struct mutex irq_complete;

	struct bq2560x_wakeup_source bq2560x_ws;

	bool irq_waiting;
	bool irq_disabled;
	bool resume_completed;

	bool batt_present;
	bool usb_present;
	int  batt_capacity;

	bool batt_full;

	bool charge_enabled;/* Register bit status */
	bool otg_enabled;
	bool batfet_enabled;
	bool in_hiz;

	bool vindpm_triggered;
	bool iindpm_triggered;

	bool in_therm_regulation;
	bool in_vsys_regulation;

	bool power_good;
	bool vbus_good;

	bool topoff_active;
	bool acov_triggered;


	bool software_jeita_supported;
	bool jeita_active;

	bool batt_hot;
	bool batt_cold;
	bool batt_warm;
	bool batt_cool;

	int batt_hot_degc;
	int batt_warm_degc;
	int batt_cool_degc;
	int batt_cold_degc;
	int hot_temp_hysteresis;
	int cold_temp_hysteresis;

	int batt_cool_ma;
	int batt_warm_ma;
	int batt_cool_mv;
	int batt_warm_mv;


	int batt_temp;

	int jeita_ma;
	int jeita_mv;

	unsigned int thermal_levels;
	unsigned int therm_lvl_sel;
	unsigned int *thermal_mitigation;

	int	usb_psy_ma;
	int charge_state;
	int charging_disabled_status;

	int fault_status;

	int skip_writes;
	int	skip_reads;

	struct bq2560x_platform_data *platform_data;

	struct delayed_work discharge_jeita_work;
	struct delayed_work charge_jeita_work;
	struct delayed_work factory_control_work;

	struct alarm jeita_alarm;

	struct dentry *debug_root;

	struct bq2560x_otg_regulator otg_vreg;

	struct power_supply *usb_psy;
	struct power_supply *bms_psy;
	struct power_supply batt_psy;
};

static int BatteryTestStatus_enable
static int bq2560x_battery_capacity

static void bq2560x_dump_fg_reg(struct bq2560x *bq)
{
	union power_supply_propval val = {0,};

	val.intval = 0;
	bq->bms_psy->set_property(bq->bms_psy,
			POWER_SUPPLY_PROP_UPDATE_NOW, &val);
}


static int __bq2560x_read_reg(struct bq2560x *bq, u8 reg, u8 *data)
{
	s32 ret;

	ret = i2c_smbus_read_byte_data(bq->client, reg);
	if (ret < 0) {
		pr_err("i2c read fail: can't read from reg 0x%02X\n", reg);
		return ret;
	}

	*data = (u8)ret;

	return 0;
}

static int __bq2560x_write_reg(struct bq2560x *bq, int reg, u8 val)
{
	s32 ret;

	ret = i2c_smbus_write_byte_data(bq->client, reg, val);
	if (ret < 0) {
		pr_err("i2c write fail: can't write 0x%02X to reg 0x%02X: %d\n",
				val, reg, ret);
		return ret;
	}
	return 0;
}

static int bq2560x_read_byte(struct bq2560x *bq, u8 *data, u8 reg)
{
	int ret;

	if (bq->skip_reads) {
		*data = 0;
		return 0;
	}

	mutex_lock(&bq->i2c_rw_lock);
	ret = __bq2560x_read_reg(bq, reg, data);
	mutex_unlock(&bq->i2c_rw_lock);

	return ret;
}


static int bq2560x_write_byte(struct bq2560x *bq, u8 reg, u8 data)
{
	int ret;

	if (bq->skip_writes) {
		return 0;
	}

	mutex_lock(&bq->i2c_rw_lock);
	ret = __bq2560x_write_reg(bq, reg, data);
	mutex_unlock(&bq->i2c_rw_lock);

	if (ret) {
		pr_err("Failed: reg=%02X, ret=%d\n", reg, ret);
	}

	return ret;
}


static int bq2560x_update_bits(struct bq2560x *bq, u8 reg,
		u8 mask, u8 data)
{
	int ret;
	u8 tmp;


	if (bq->skip_reads || bq->skip_writes)
		return 0;

	mutex_lock(&bq->i2c_rw_lock);
	ret = __bq2560x_read_reg(bq, reg, &tmp);
	if (ret) {
		pr_err("Failed: reg=%02X, ret=%d\n", reg, ret);
		goto out;
	}

	tmp &= ~mask;
	tmp |= data & mask;

	ret = __bq2560x_write_reg(bq, reg, tmp);
	if (ret) {
		pr_err("Failed: reg=%02X, ret=%d\n", reg, ret);
	}

out:
	mutex_unlock(&bq->i2c_rw_lock);
	return ret;
}
static void bq2560x_stay_awake(struct bq2560x_wakeup_source *source,
	enum wakeup_src wk_src)
{
	unsigned long flags;

	spin_lock_irqsave(&source->ws_lock, flags);

	if (!__test_and_set_bit(wk_src, &source->enabled_bitmap)) {
		__pm_stay_awake(&source->source);
		pr_debug("enabled source %s, wakeup_src %d\n",
				source->source.name, wk_src);
	}
	spin_unlock_irqrestore(&source->ws_lock, flags);
}

static void bq2560x_relax(struct bq2560x_wakeup_source *source,
	enum wakeup_src wk_src)
{
	unsigned long flags;

	spin_lock_irqsave(&source->ws_lock, flags);
	if (__test_and_clear_bit(wk_src, &source->enabled_bitmap) &&
			!(source->enabled_bitmap & WAKEUP_SRC_MASK)) {
		__pm_relax(&source->source);
		pr_debug("disabled source %s\n", source->source.name);
	}
	spin_unlock_irqrestore(&source->ws_lock, flags);

	pr_debug("relax source %s, wakeup_src %d\n",
			source->source.name, wk_src);
}

static void bq2560x_wakeup_src_init(struct bq2560x *bq)
{
	spin_lock_init(&bq->bq2560x_ws.ws_lock);
	wakeup_source_init(&bq->bq2560x_ws.source, "bq2560x");
}



static int bq2560x_enable_otg(struct bq2560x *bq)
{
	u8 val = REG01_OTG_ENABLE << REG01_OTG_CONFIG_SHIFT;

	return bq2560x_update_bits(bq, BQ2560X_REG_01,
			REG01_OTG_CONFIG_MASK, val);

}

static int bq2560x_disable_otg(struct bq2560x *bq)
{
	u8 val = REG01_OTG_DISABLE << REG01_OTG_CONFIG_SHIFT;

	return bq2560x_update_bits(bq, BQ2560X_REG_01,
			REG01_OTG_CONFIG_MASK, val);

}

static int bq2560x_enable_charger(struct bq2560x *bq)
{
	int ret;
	u8 val = REG01_CHG_ENABLE << REG01_CHG_CONFIG_SHIFT;

	ret = bq2560x_update_bits(bq, BQ2560X_REG_01, REG01_CHG_CONFIG_MASK, val);

	return ret;
}

static int bq2560x_disable_charger(struct bq2560x *bq)
{
	int ret;
	u8 val = REG01_CHG_DISABLE << REG01_CHG_CONFIG_SHIFT;

	ret = bq2560x_update_bits(bq, BQ2560X_REG_01, REG01_CHG_CONFIG_MASK, val);
	return ret;
}

int bq2560x_set_chargecurrent(struct bq2560x *bq, int curr)
{
	u8 ichg;

	ichg = (curr - REG02_ICHG_BASE)/REG02_ICHG_LSB;
	return bq2560x_update_bits(bq, BQ2560X_REG_02, REG02_ICHG_MASK,
			ichg << REG02_ICHG_SHIFT);

}

int bq2560x_set_term_current(struct bq2560x *bq, int curr)
{
	u8 iterm;

	iterm = (curr - REG03_ITERM_BASE) / REG03_ITERM_LSB;

	return bq2560x_update_bits(bq, BQ2560X_REG_03, REG03_ITERM_MASK,
			iterm << REG03_ITERM_SHIFT);
}


int bq2560x_set_prechg_current(struct bq2560x *bq, int curr)
{
	u8 iprechg;

	iprechg = (curr - REG03_IPRECHG_BASE) / REG03_IPRECHG_LSB;

	return bq2560x_update_bits(bq, BQ2560X_REG_03, REG03_IPRECHG_MASK,
			iprechg << REG03_IPRECHG_SHIFT);
}

int bq2560x_set_chargevolt(struct bq2560x *bq, int volt)
{
	u8 val;

	val = (volt - REG04_VREG_BASE)/REG04_VREG_LSB;
	return bq2560x_update_bits(bq, BQ2560X_REG_04, REG04_VREG_MASK,
			val << REG04_VREG_SHIFT);
}


int bq2560x_set_input_volt_limit(struct bq2560x *bq, int volt)
{
	u8 val;
	val = (volt - REG06_VINDPM_BASE) / REG06_VINDPM_LSB;
	return bq2560x_update_bits(bq, BQ2560X_REG_06, REG06_VINDPM_MASK,
			val << REG06_VINDPM_SHIFT);
}

int bq2560x_set_input_current_limit(struct bq2560x *bq, int curr)
{
	u8 val;

	val = (curr - REG00_IINLIM_BASE) / REG00_IINLIM_LSB;
	return bq2560x_update_bits(bq, BQ2560X_REG_00, REG00_IINLIM_MASK,
			val << REG00_IINLIM_SHIFT);
}


int bq2560x_set_watchdog_timer(struct bq2560x *bq, u8 timeout)
{
	u8 temp;

	temp = (u8)(((timeout - REG05_WDT_BASE) / REG05_WDT_LSB) << REG05_WDT_SHIFT);

	return bq2560x_update_bits(bq, BQ2560X_REG_05, REG05_WDT_MASK, temp);
}
EXPORT_SYMBOL_GPL(bq2560x_set_watchdog_timer);

int bq2560x_disable_watchdog_timer(struct bq2560x *bq)
{
	u8 val = REG05_WDT_DISABLE << REG05_WDT_SHIFT;

	return bq2560x_update_bits(bq, BQ2560X_REG_05, REG05_WDT_MASK, val);
}
EXPORT_SYMBOL_GPL(bq2560x_disable_watchdog_timer);

int bq2560x_reset_watchdog_timer(struct bq2560x *bq)
{
	u8 val = REG01_WDT_RESET << REG01_WDT_RESET_SHIFT;

	return bq2560x_update_bits(bq, BQ2560X_REG_01, REG01_WDT_RESET_MASK, val);
}
EXPORT_SYMBOL_GPL(bq2560x_reset_watchdog_timer);

int bq2560x_reset_chip(struct bq2560x *bq)
{
	int ret;
	u8 val = REG0B_REG_RESET << REG0B_REG_RESET_SHIFT;

	ret = bq2560x_update_bits(bq, BQ2560X_REG_0B, REG0B_REG_RESET_MASK, val);
	return ret;
}
EXPORT_SYMBOL_GPL(bq2560x_reset_chip);

int bq2560x_enter_hiz_mode(struct bq2560x *bq)
{
	u8 val = REG00_HIZ_ENABLE << REG00_ENHIZ_SHIFT;

	return bq2560x_update_bits(bq, BQ2560X_REG_00, REG00_ENHIZ_MASK, val);

}
EXPORT_SYMBOL_GPL(bq2560x_enter_hiz_mode);

int bq2560x_exit_hiz_mode(struct bq2560x *bq)
{

	u8 val = REG00_HIZ_DISABLE << REG00_ENHIZ_SHIFT;

	return bq2560x_update_bits(bq, BQ2560X_REG_00, REG00_ENHIZ_MASK, val);

}
EXPORT_SYMBOL_GPL(bq2560x_exit_hiz_mode);

int bq2560x_get_hiz_mode(struct bq2560x *bq, u8 *state)
{
	u8 val;
	int ret;

	ret = bq2560x_read_byte(bq, &val, BQ2560X_REG_00);
	if (ret)
		return ret;
	*state = (val & REG00_ENHIZ_MASK) >> REG00_ENHIZ_SHIFT;

	return 0;
}
EXPORT_SYMBOL_GPL(bq2560x_get_hiz_mode);


static int bq2560x_enable_term(struct bq2560x *bq, bool enable)
{
	u8 val;
	int ret;

	if (enable)
		val = REG05_TERM_ENABLE << REG05_EN_TERM_SHIFT;
	else
		val = REG05_TERM_DISABLE << REG05_EN_TERM_SHIFT;

	ret = bq2560x_update_bits(bq, BQ2560X_REG_05, REG05_EN_TERM_MASK, val);

	return ret;
}
EXPORT_SYMBOL_GPL(bq2560x_enable_term);

int bq2560x_set_boost_current(struct bq2560x *bq, int curr)
{
	u8 val;

	val = REG02_BOOST_LIM_0P5A;
	if (curr == BOOSTI_1200)
		val = REG02_BOOST_LIM_1P2A;

	return bq2560x_update_bits(bq, BQ2560X_REG_02, REG02_BOOST_LIM_MASK,
			val << REG02_BOOST_LIM_SHIFT);
}

int bq2560x_set_boost_voltage(struct bq2560x *bq, int volt)
{
	u8 val;

	if (volt == BOOSTV_4850)
		val = REG06_BOOSTV_4P85V;
	else if (volt == BOOSTV_5150)
		val = REG06_BOOSTV_5P15V;
	else if (volt == BOOSTV_5300)
		val = REG06_BOOSTV_5P3V;
	else
		val = REG06_BOOSTV_5V;

	return bq2560x_update_bits(bq, BQ2560X_REG_06, REG06_BOOSTV_MASK,
			val << REG06_BOOSTV_SHIFT);
}

static int bq2560x_set_acovp_threshold(struct bq2560x *bq, int volt)
{
	u8 val;

	if (volt == VAC_OVP_14300)
		val = REG06_OVP_14P3V;
	else if (volt == VAC_OVP_10500)
		val = REG06_OVP_10P5V;
	else if (volt == VAC_OVP_6200)
		val = REG06_OVP_6P2V;
	else
		val = REG06_OVP_5P5V;

	return bq2560x_update_bits(bq, BQ2560X_REG_06, REG06_OVP_MASK,
			val << REG06_OVP_SHIFT);
}


static int bq2560x_set_stat_ctrl(struct bq2560x *bq, int ctrl)
{
	u8 val;

	val = ctrl;

	return bq2560x_update_bits(bq, BQ2560X_REG_00, REG00_STAT_CTRL_MASK,
			val << REG00_STAT_CTRL_SHIFT);
}


static int bq2560x_set_int_mask(struct bq2560x *bq, int mask)
{
	u8 val;

	val = mask;

	return bq2560x_update_bits(bq, BQ2560X_REG_0A, REG0A_INT_MASK_MASK,
			val << REG0A_INT_MASK_SHIFT);
}


static int bq2560x_enable_batfet(struct bq2560x *bq)
{
	const u8 val = REG07_BATFET_ON << REG07_BATFET_DIS_SHIFT;

	return bq2560x_update_bits(bq, BQ2560X_REG_07, REG07_BATFET_DIS_MASK,
			val);
}
EXPORT_SYMBOL_GPL(bq2560x_enable_batfet);


static int bq2560x_disable_batfet(struct bq2560x *bq)
{
	const u8 val = REG07_BATFET_OFF << REG07_BATFET_DIS_SHIFT;

	return bq2560x_update_bits(bq, BQ2560X_REG_07, REG07_BATFET_DIS_MASK,
			val);
}
EXPORT_SYMBOL_GPL(bq2560x_disable_batfet);

static int bq2560x_set_batfet_delay(struct bq2560x *bq, uint8_t delay)
{
	u8 val;

	if (delay == 0)
		val = REG07_BATFET_DLY_0S;
	else
		val = REG07_BATFET_DLY_10S;

	val <<= REG07_BATFET_DLY_SHIFT;

	return bq2560x_update_bits(bq, BQ2560X_REG_07, REG07_BATFET_DLY_MASK,
			val);
}
EXPORT_SYMBOL_GPL(bq2560x_set_batfet_delay);

static int bq2560x_charging_disable(struct bq2560x *bq, int reason,
			int disable)
{

	int ret = 0;
	int disabled;

	mutex_lock(&bq->charging_disable_lock);

	disabled = bq->charging_disabled_status;

	pr_info("reason=%d requested_disable=%d disabled_status=%d\n",
			reason, disable, disabled);

	if (disable == true)
		disabled |= reason;
	else
		disabled &= ~reason;

	if (disabled && bq->charge_enabled)
		ret = bq2560x_disable_charger(bq);
	else if (!disabled && !bq->charge_enabled)
		ret = bq2560x_enable_charger(bq);

	if (ret) {
		pr_err("Couldn't disable/enable charging for reason=%d ret=%d\n",
				ret, reason);
	} else {
		bq->charging_disabled_status = disabled;
		mutex_lock(&bq->data_lock);
		bq->charge_enabled = !disabled;
		mutex_unlock(&bq->data_lock);
	}
	mutex_unlock(&bq->charging_disable_lock);

	return ret;
}


static struct power_supply *get_bms_psy(struct bq2560x *bq)
{
	if (bq->bms_psy)
		return bq->bms_psy;
	bq->bms_psy = power_supply_get_by_name("bms");
	if (!bq->bms_psy)
		pr_debug("bms power supply not found\n");

	return bq->bms_psy;
}

static int bq2560x_get_batt_property(struct bq2560x *bq,
		enum power_supply_property psp,
		union power_supply_propval *val)
{
	struct power_supply *bms_psy = get_bms_psy(bq);

	int ret;

	if (!bms_psy)
		return -EINVAL;

	ret = bms_psy->get_property(bms_psy, psp, val);

	return ret;
}

static inline bool is_device_suspended(struct bq2560x *bq);
static int bq2560x_get_prop_charge_type(struct bq2560x *bq)
{
	u8 val = 0;
	if (is_device_suspended(bq))
		return POWER_SUPPLY_CHARGE_TYPE_UNKNOWN;

	bq2560x_read_byte(bq, &val, BQ2560X_REG_08);
	val &= REG08_CHRG_STAT_MASK;
	val >>= REG08_CHRG_STAT_SHIFT;
	switch (val) {
	case CHARGE_STATE_FASTCHG:
		return POWER_SUPPLY_CHARGE_TYPE_FAST;
	case CHARGE_STATE_PRECHG:
		return POWER_SUPPLY_CHARGE_TYPE_TRICKLE;
	case CHARGE_STATE_CHGDONE:
	case CHARGE_STATE_IDLE:
		return POWER_SUPPLY_CHARGE_TYPE_NONE;
	default:
		return POWER_SUPPLY_CHARGE_TYPE_UNKNOWN;
	}
}

static int bq2560x_get_prop_batt_present(struct bq2560x *bq)
{
	union power_supply_propval batt_prop = {0,};
	int ret;

	ret = bq2560x_get_batt_property(bq,
			POWER_SUPPLY_PROP_PRESENT, &batt_prop);
	if (!ret)
		bq->batt_present = batt_prop.intval;

	return ret;

}

static int bq2560x_get_prop_batt_capacity(struct bq2560x *bq)
{
	union power_supply_propval batt_prop = {0,};
	int ret;

	ret = bq2560x_get_batt_property(bq,
			POWER_SUPPLY_PROP_CAPACITY, &batt_prop);

	if (!ret)
		bq->batt_capacity = batt_prop.intval;

	return ret;

}


static int bq2560x_get_prop_batt_full(struct bq2560x *bq)
{
	union power_supply_propval batt_prop = {0,};
	int ret;

	ret = bq2560x_get_batt_property(bq,
			POWER_SUPPLY_PROP_STATUS, &batt_prop);
	if (!ret)
		bq->batt_full = (batt_prop.intval == POWER_SUPPLY_STATUS_FULL);

	return ret;
}

static int bq2560x_get_prop_charge_status(struct bq2560x *bq)
{
	union power_supply_propval batt_prop = {0,};
	int ret;
	u8 status;

	ret = bq2560x_get_batt_property(bq,
			POWER_SUPPLY_PROP_STATUS, &batt_prop);
	if (!ret && batt_prop.intval == POWER_SUPPLY_STATUS_FULL)
		return POWER_SUPPLY_STATUS_FULL;

	ret = bq2560x_read_byte(bq, &status, BQ2560X_REG_08);
	if (ret) {
		return 	POWER_SUPPLY_STATUS_UNKNOWN;
	}

	mutex_lock(&bq->data_lock);
	bq->charge_state = (status & REG08_CHRG_STAT_MASK) >> REG08_CHRG_STAT_SHIFT;
	mutex_unlock(&bq->data_lock);

	switch (bq->charge_state) {
	case CHARGE_STATE_FASTCHG:
	case CHARGE_STATE_PRECHG:
		return POWER_SUPPLY_STATUS_CHARGING;
	case CHARGE_STATE_CHGDONE:
		return POWER_SUPPLY_STATUS_NOT_CHARGING;
	case CHARGE_STATE_IDLE:
		return POWER_SUPPLY_STATUS_DISCHARGING;
	default:
		return 	POWER_SUPPLY_STATUS_UNKNOWN;
	}

}

static int bq2560x_get_prop_health(struct bq2560x *bq)
{
	int ret;
	union power_supply_propval batt_prop = {0,};

	if (bq->software_jeita_supported) {
		if (bq->jeita_active) {
			if (bq->batt_hot)
				ret = POWER_SUPPLY_HEALTH_OVERHEAT;
			else if (bq->batt_warm)
				ret = POWER_SUPPLY_HEALTH_WARM;
			else if (bq->batt_cool)
				ret = POWER_SUPPLY_HEALTH_COOL;
			else if (bq->batt_cold)
				ret = POWER_SUPPLY_HEALTH_COLD;
		} else {
			ret = POWER_SUPPLY_HEALTH_GOOD;
		}
	} else {
		ret = bq2560x_get_batt_property(bq,
				POWER_SUPPLY_PROP_HEALTH, &batt_prop);
		if (!ret)
			ret = batt_prop.intval;
		else
			ret = POWER_SUPPLY_HEALTH_UNKNOWN;
	}
	return ret;
}


static enum power_supply_property bq2560x_charger_props[] = {

	POWER_SUPPLY_PROP_CHARGE_TYPE,
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_CHARGING_ENABLED,
	POWER_SUPPLY_PROP_HEALTH,
	POWER_SUPPLY_PROP_CAPACITY,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_CURRENT_NOW,
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN,

	POWER_SUPPLY_PROP_CAPACITY_LEVEL,
	POWER_SUPPLY_PROP_TEMP,

	POWER_SUPPLY_PROP_CHARGE_FULL,



	POWER_SUPPLY_PROP_SYSTEM_TEMP_LEVEL,
	POWER_SUPPLY_PROP_TECHNOLOGY,
	POWER_SUPPLY_PROP_RESISTANCE_ID,
};

void static runin_work(struct bq2560x *bq, int runin_flag)
{
	int rc;

	if (1 == runin_flag) {
		pr_err("bq2560x_enter_hiz_mode, bq->usb_present = %d\n", bq->usb_present);
		rc = bq2560x_enter_hiz_mode(bq);
		if (rc) {
			dev_err(bq->dev, "Couldn't disenable charge rc=%d\n", rc);
		}
	} else if (2 == runin_flag) {
		pr_err("bq2560x_exit_hiz_mode, bq->usb_present = %d\n", bq->usb_present);
		rc = bq2560x_exit_hiz_mode(bq);
		if (rc) {
			dev_err(bq->dev, "Couldn't enable charge rc=%d\n", rc);
		}
	}
}


static int bq2560x_charger_get_property(struct power_supply *psy,
		enum power_supply_property psp,
		union power_supply_propval *val)
{

	struct bq2560x *bq = container_of(psy, struct bq2560x, batt_psy);

	switch (psp) {
	case POWER_SUPPLY_PROP_CHARGE_TYPE:
		val->intval = bq2560x_get_prop_charge_type(bq);
		pr_debug("POWER_SUPPLY_PROP_CHARGE_TYPE:%d\n", val->intval);
		break;
	case POWER_SUPPLY_PROP_CHARGING_ENABLED:
		val->intval = bq->charge_enabled;
		break;
	case POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN:
		val->intval = 3000;
		break;
	case POWER_SUPPLY_PROP_STATUS:
		val->intval = bq2560x_get_prop_charge_status(bq);
		break;
	case POWER_SUPPLY_PROP_HEALTH:
		val->intval = bq2560x_get_prop_health(bq);
		break;
	case POWER_SUPPLY_PROP_CAPACITY:
		bq2560x_get_batt_property(bq, psp, val);
		bq2560x_battery_capacity = val->intval;

		break;
	case POWER_SUPPLY_PROP_SYSTEM_TEMP_LEVEL:
		val->intval = bq->therm_lvl_sel;
		break;
	case POWER_SUPPLY_PROP_PRESENT:
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
	case POWER_SUPPLY_PROP_CURRENT_NOW:
	case POWER_SUPPLY_PROP_CAPACITY_LEVEL:
	case POWER_SUPPLY_PROP_TEMP:
	case POWER_SUPPLY_PROP_CHARGE_FULL:
	case POWER_SUPPLY_PROP_TECHNOLOGY:
	case POWER_SUPPLY_PROP_RESISTANCE_ID:
		return bq2560x_get_batt_property(bq, psp, val);
	default:
		return -EINVAL;

	}

	return 0;
}

static int bq2560x_system_temp_level_set(struct bq2560x *bq, int);
extern int bq_runin_test;
static int bq2560x_charger_set_property(struct power_supply *psy,
		enum power_supply_property prop,
		const union power_supply_propval *val)
{
	struct bq2560x *bq = container_of(psy,
		struct bq2560x, batt_psy);

	switch (prop) {
	case POWER_SUPPLY_PROP_CHARGING_ENABLED:
		bq2560x_charging_disable(bq, USER, !val->intval);

		if (bq_runin_test)  {
			pr_err("bq_runin_test\n");
			runin_work(bq, bq_runin_test);
			bq_runin_test = 0;
		}

		power_supply_changed(&bq->batt_psy);
		power_supply_changed(bq->usb_psy);
		pr_info("POWER_SUPPLY_PROP_CHARGING_ENABLED: %s\n",
				val->intval ? "enable" : "disable");
		break;
	case POWER_SUPPLY_PROP_SYSTEM_TEMP_LEVEL:
		bq2560x_system_temp_level_set(bq, val->intval);
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int bq2560x_charger_is_writeable(struct power_supply *psy,
		enum power_supply_property prop)
{
	int ret;

	switch (prop) {
	case POWER_SUPPLY_PROP_CHARGING_ENABLED:
	case POWER_SUPPLY_PROP_SYSTEM_TEMP_LEVEL:
		ret = 1;
		break;
	default:
		ret = 0;
		break;
	}
	return ret;
}

static int bq2560x_update_charging_profile(struct bq2560x *bq)
{
	int ret;
	int chg_ma;
	int chg_mv;
	int icl;
	int therm_ma;

	union power_supply_propval prop = {0,};


	if (!bq->usb_present)
		return 0;

	ret = bq->usb_psy->get_property(bq->usb_psy,
			POWER_SUPPLY_PROP_TYPE, &prop);

	if (ret < 0) {
		pr_err("couldn't read USB TYPE property, ret=%d\n", ret);
		return ret;
	}

	mutex_lock(&bq->profile_change_lock);
	if (bq->jeita_active) {
		chg_ma = bq->jeita_ma;
		chg_mv = bq->jeita_mv;
	} else {
		if (prop.intval == POWER_SUPPLY_TYPE_USB_DCP || prop.intval == POWER_SUPPLY_TYPE_USB_CDP) {
			chg_ma = bq->platform_data->ta.ichg;
			chg_mv = bq->platform_data->ta.vreg;
		} else {
			chg_ma = bq->platform_data->usb.ichg;
			chg_mv = bq->platform_data->usb.vreg;
		}
	}

	icl = bq->usb_psy_ma;
	if (bq->therm_lvl_sel > 0
			&& bq->therm_lvl_sel < (bq->thermal_levels - 1))
		therm_ma = bq->thermal_mitigation[bq->therm_lvl_sel];
	else
		therm_ma = chg_ma;

	icl = min(therm_ma, icl);

	pr_info("charge volt = %d, charge curr = %d, input curr limit = %d\n",
			chg_mv, chg_ma, icl);

	ret = bq2560x_set_input_current_limit(bq, icl);
	if (ret < 0)
		pr_err("couldn't set input current limit, ret=%d\n", ret);

	ret = bq2560x_set_input_volt_limit(bq, bq->platform_data->ta.vlim);
	if (ret < 0)
		pr_err("couldn't set input voltage limit, ret=%d\n", ret);

#ifdef CONFIG_DISABLE_TEMP_PROTECT
	chg_mv = 4100;
#endif

	ret = bq2560x_set_chargevolt(bq, chg_mv);
	if (ret < 0)
		pr_err("couldn't set charge voltage ret=%d\n", ret);

	ret = bq2560x_set_chargecurrent(bq, chg_ma);
	if (ret < 0)
		pr_err("couldn't set charge current, ret=%d\n", ret);

	if (bq->jeita_active && (bq->batt_hot || bq->batt_cold))
		bq2560x_charging_disable(bq, JEITA, true);
	else
		bq2560x_charging_disable(bq, JEITA, false);

	mutex_unlock(&bq->profile_change_lock);

	return 0;
}


static int bq2560x_system_temp_level_set(struct bq2560x *bq,
		int lvl_sel)
{
	int ret = 0;
	int prev_therm_lvl;

	pr_err("%s lvl_sel=%d, bq->therm_lvl_sel = %d\n", __func__, lvl_sel, bq->therm_lvl_sel);
	if (BatteryTestStatus_enable)
		return 0;

	if (!bq->thermal_mitigation) {
		pr_err("Thermal mitigation not supported\n");
		return -EINVAL;
	}

	if (lvl_sel < 0) {
		pr_err("Unsupported level selected %d\n", lvl_sel);
		return -EINVAL;
	}

	if (lvl_sel >= bq->thermal_levels) {
		pr_err("Unsupported level selected %d forcing %d\n", lvl_sel,
				bq->thermal_levels - 1);
		lvl_sel = bq->thermal_levels - 1;
	}

	if (lvl_sel == bq->therm_lvl_sel)
		return 0;


	prev_therm_lvl = bq->therm_lvl_sel;
	bq->therm_lvl_sel = lvl_sel;

	ret = bq2560x_update_charging_profile(bq);
	if (ret)
		pr_err("Couldn't set USB current ret = %d\n", ret);


	return ret;
}

static void bq2560x_factory_mode_control_capacity_work(struct work_struct *work)
{
#ifdef WT_COMPILE_FACTORY_VERSION
	int ret;
	struct bq2560x *bq = container_of(work,
			struct bq2560x, factory_control_work.work);

	if (bq2560x_battery_capacity >= 80 && bq2560x_battery_capacity <= 100)  {
		ret = bq2560x_charging_disable(bq, SOC, true);
		if (ret) {
			dev_err(bq->dev, "factory_mode_control_capacity disable fail: %d\n", ret);
		}
	} else if (bq2560x_battery_capacity >= 0 && bq2560x_battery_capacity < 80)  {
		ret = bq2560x_charging_disable(bq, SOC, false);
		if (ret) {
			dev_err(bq->dev, "actory_mode_control_capacity enable fail: %d\n", ret);
		}
	}

	schedule_delayed_work(&bq->factory_control_work, msecs_to_jiffies(20000));
#endif

}


static void bq2560x_external_power_changed(struct power_supply *psy)
{
	struct bq2560x *bq = container_of(psy, struct bq2560x, batt_psy);

	union power_supply_propval prop = {0,};
	int ret, current_limit = 0;


	ret = bq->usb_psy->get_property(bq->usb_psy,
			POWER_SUPPLY_PROP_CURRENT_MAX, &prop);
	if (ret < 0)
		pr_err("could not read USB current_max property, ret=%d\n", ret);
	else
		current_limit = prop.intval / 1000;

	pr_info("current_limit = %d\n", current_limit);

	if (bq->usb_psy_ma != current_limit) {
		bq->usb_psy_ma = current_limit;
		bq2560x_update_charging_profile(bq);
	}

	ret = bq->usb_psy->get_property(bq->usb_psy,
			POWER_SUPPLY_PROP_ONLINE, &prop);
	if (ret < 0)
		pr_err("could not read USB ONLINE property, ret=%d\n", ret);
	else
		pr_info("usb online status =%d\n", prop.intval);

	ret = 0;
	bq2560x_get_prop_charge_status(bq);
	if (bq->usb_present) {
		if (prop.intval == 0)  {
			pr_err("set usb online\n");
			ret = power_supply_set_online(bq->usb_psy, true);
		}
	} else {
		if (prop.intval == 1) {
			pr_err("set usb offline\n");
			ret = power_supply_set_online(bq->usb_psy, false);
		}
	}

	if (ret < 0)
		pr_info("could not set usb online state, ret=%d\n", ret);

}


static int bq2560x_psy_register(struct bq2560x *bq)
{
	int ret;

	bq->batt_psy.name = "battery";
	bq->batt_psy.type = POWER_SUPPLY_TYPE_BATTERY;
	bq->batt_psy.properties = bq2560x_charger_props;
	bq->batt_psy.num_properties = ARRAY_SIZE(bq2560x_charger_props);
	bq->batt_psy.get_property = bq2560x_charger_get_property;
	bq->batt_psy.set_property = bq2560x_charger_set_property;
	bq->batt_psy.external_power_changed = bq2560x_external_power_changed;
	bq->batt_psy.property_is_writeable = bq2560x_charger_is_writeable;

	ret = power_supply_register(bq->dev, &bq->batt_psy);
	if (ret < 0) {
		pr_err("failed to register batt_psy:%d\n", ret);
		return ret;
	}

	return 0;
}

static void bq2560x_psy_unregister(struct bq2560x *bq)
{
	power_supply_unregister(&bq->batt_psy);
}


static int bq2560x_otg_regulator_enable(struct regulator_dev *rdev)
{
	int ret;
	struct bq2560x *bq = rdev_get_drvdata(rdev);

	ret = bq2560x_enable_otg(bq);
	if (ret) {
		pr_err("Couldn't enable OTG mode ret=%d\n", ret);
	} else {
		bq->otg_enabled = true;
		pr_info("bq2560x OTG mode Enabled!\n");
	}

	return ret;
}


static int bq2560x_otg_regulator_disable(struct regulator_dev *rdev)
{
	int ret;
	struct bq2560x *bq = rdev_get_drvdata(rdev);

	ret = bq2560x_disable_otg(bq);
	if (ret) {
		pr_err("Couldn't disable OTG mode, ret=%d\n", ret);
	} else {
		bq->otg_enabled = false;
		pr_info("bq2560x OTG mode Disabled\n");
	}

	return ret;
}


static int bq2560x_otg_regulator_is_enable(struct regulator_dev *rdev)
{
	int ret;
	u8 status;
	u8 enabled;
	struct bq2560x *bq = rdev_get_drvdata(rdev);

	ret = bq2560x_read_byte(bq, &status, BQ2560X_REG_01);
	if (ret)
		return ret;
	enabled = ((status & REG01_OTG_CONFIG_MASK) >> REG01_OTG_CONFIG_SHIFT);

	return (enabled == REG01_OTG_ENABLE) ? 1 : 0;

}


struct regulator_ops bq2560x_otg_reg_ops = {
	.enable		= bq2560x_otg_regulator_enable,
	.disable	= bq2560x_otg_regulator_disable,
	.is_enabled = bq2560x_otg_regulator_is_enable,
};

static int bq2560x_regulator_init(struct bq2560x *bq)
{
	int ret = 0;
	struct regulator_init_data *init_data;
	struct regulator_config cfg = {};

	init_data = of_get_regulator_init_data(bq->dev, bq->dev->of_node);
	if (!init_data) {
		dev_err(bq->dev, "Unable to allocate memory\n");
		return -ENOMEM;
	}

	if (init_data->constraints.name) {
		bq->otg_vreg.rdesc.owner = THIS_MODULE;
		bq->otg_vreg.rdesc.type = REGULATOR_VOLTAGE;
		bq->otg_vreg.rdesc.ops = &bq2560x_otg_reg_ops;
		bq->otg_vreg.rdesc.name = init_data->constraints.name;
		pr_info("regualtor name = %s\n", bq->otg_vreg.rdesc.name);

		cfg.dev = bq->dev;
		cfg.init_data = init_data;
		cfg.driver_data = bq;
		cfg.of_node = bq->dev->of_node;

		init_data->constraints.valid_ops_mask
				|= REGULATOR_CHANGE_STATUS;

		bq->otg_vreg.rdev = regulator_register(
				&bq->otg_vreg.rdesc, &cfg);
		if (IS_ERR(bq->otg_vreg.rdev)) {
			ret = PTR_ERR(bq->otg_vreg.rdev);
			bq->otg_vreg.rdev = NULL;
			if (ret != -EPROBE_DEFER)
				dev_err(bq->dev,
					"OTG reg failed, rc=%d\n", ret);
		}
	}

	return ret;
}


static int bq2560x_parse_jeita_dt(struct device *dev, struct bq2560x *bq)
{
	struct device_node *np = dev->of_node;
	int ret;

	ret = of_property_read_u32(np, "ti, bq2560x, jeita-hot-degc",
			&bq->batt_hot_degc);
	if (ret) {
		pr_err("Failed to read ti,bq2560x,jeita-hot-degc\n");
		return ret;
	}

	ret = of_property_read_u32(np, "ti, bq2560x, jeita-warm-degc",
			&bq->batt_warm_degc);
	if (ret) {
		pr_err("Failed to read ti,bq2560x,jeita-warm-degc\n");
		return ret;
	}

	ret = of_property_read_u32(np, "ti, bq2560x, jeita-cool-degc",
			&bq->batt_cool_degc);
	if (ret) {
		pr_err("Failed to read ti,bq2560x,jeita-cool-degc\n");
		return ret;
	}
	ret = of_property_read_u32(np, "ti, bq2560x, jeita-cold-degc",
			&bq->batt_cold_degc);
	if (ret) {
		pr_err("Failed to read ti,bq2560x,jeita-cold-degc\n");
		return ret;
	}

	ret = of_property_read_u32(np, "ti, bq2560x, jeita-hot-hysteresis",
			&bq->hot_temp_hysteresis);
	if (ret) {
		pr_err("Failed to read ti,bq2560x,jeita-hot-hysteresis\n");
		return ret;
	}

	ret = of_property_read_u32(np, "ti, bq2560x, jeita-cold-hysteresis",
			&bq->cold_temp_hysteresis);
	if (ret) {
		pr_err("Failed to read ti,bq2560x,jeita-cold-hysteresis\n");
		return ret;
	}

	ret = of_property_read_u32(np, "ti, bq2560x, jeita-cool-ma",
			&bq->batt_cool_ma);
	if (ret) {
		pr_err("Failed to read ti,bq2560x,jeita-cool-ma\n");
		return ret;
	}

	ret = of_property_read_u32(np, "ti, bq2560x, jeita-cool-mv",
			&bq->batt_cool_mv);
	if (ret) {
		pr_err("Failed to read ti,bq2560x,jeita-cool-mv\n");
		return ret;
	}

	ret = of_property_read_u32(np, "ti, bq2560x, jeita-warm-ma",
			&bq->batt_warm_ma);
	if (ret) {
		pr_err("Failed to read ti,bq2560x,jeita-warm-ma\n");
		return ret;
	}

	ret = of_property_read_u32(np, "ti, bq2560x, jeita-warm-mv",
			&bq->batt_warm_mv);
	if (ret) {
		pr_err("Failed to read ti,bq2560x,jeita-warm-mv\n");
		return ret;
	}

	bq->software_jeita_supported =
			of_property_read_bool(np, "ti, bq2560x, software-jeita-supported");

	return 0;
}


static struct bq2560x_platform_data *bq2560x_parse_dt(struct device *dev,
		struct bq2560x *bq)
{
	int ret;
	struct device_node *np = dev->of_node;
	struct bq2560x_platform_data *pdata;

	pdata = devm_kzalloc(dev, sizeof(struct bq2560x_platform_data),
				GFP_KERNEL);
	if (!pdata) {
		pr_err("Out of memory\n");
		return NULL;
	}

	ret = of_property_read_u32(np, "ti,bq2560x,chip-enable-gpio", &bq->gpio_ce);
	if (ret) {
		pr_err("Failed to read node of ti,bq2560x,chip-enable-gpio\n");
	}

	ret = of_property_read_u32(np, "ti, bq2560x, usb-vlim",  &pdata->usb.vlim);
	if (ret) {
		pr_err("Failed to read node of ti,bq2560x,usb-vlim\n");
	}

	ret = of_property_read_u32(np, "ti, bq2560x, usb-ilim",  &pdata->usb.ilim);
	if (ret) {
		pr_err("Failed to read node of ti,bq2560x,usb-ilim\n");
	}

	ret = of_property_read_u32(np, "ti, bq2560x, usb-vreg",  &pdata->usb.vreg);
	if (ret) {
		pr_err("Failed to read node of ti,bq2560x,usb-vreg\n");
	}

	ret = of_property_read_u32(np, "ti, bq2560x, usb-ichg",  &pdata->usb.ichg);
	if (ret) {
		pr_err("Failed to read node of ti,bq2560x,usb-ichg\n");
	}

	ret = of_property_read_u32(np, "ti, bq2560x, ta-vlim",  &pdata->ta.vlim);
	if (ret) {
		pr_err("Failed to read node of ti,bq2560x,ta-vlim\n");
	}

	ret = of_property_read_u32(np, "ti, bq2560x, ta-ilim",  &pdata->ta.ilim);
	if (ret) {
		pr_err("Failed to read node of ti,bq2560x,ta-ilim\n");
	}

	ret = of_property_read_u32(np, "ti, bq2560x, ta-vreg",  &pdata->ta.vreg);
	if (ret) {
		pr_err("Failed to read node of ti,bq2560x,ta-vreg\n");
	}

	ret = of_property_read_u32(np, "ti, bq2560x, ta-ichg",  &pdata->ta.ichg);
	if (ret) {
		pr_err("Failed to read node of ti,bq2560x,ta-ichg\n");
	}

	ret = of_property_read_u32(np, "ti, bq2560x, stat-pin-ctrl",  &pdata->statctrl);
	if (ret) {
		pr_err("Failed to read node of ti,bq2560x,stat-pin-ctrl\n");
	}

	ret = of_property_read_u32(np, "ti, bq2560x, precharge-current",  &pdata->iprechg);
	if (ret) {
		pr_err("Failed to read node of ti,bq2560x,precharge-current\n");
	}

	ret = of_property_read_u32(np, "ti, bq2560x, termination-current",  &pdata->iterm);
	if (ret) {
		pr_err("Failed to read node of ti,bq2560x,termination-current\n");
	}

	ret = of_property_read_u32(np, "ti, bq2560x, boost-voltage",  &pdata->boostv);
	if (ret) {
		pr_err("Failed to read node of ti,bq2560x,boost-voltage\n");
	}

	ret = of_property_read_u32(np, "ti, bq2560x, boost-current",  &pdata->boosti);
	if (ret) {
		pr_err("Failed to read node of ti,bq2560x,boost-current\n");
	}

	ret = of_property_read_u32(np, "ti, bq2560x, vac-ovp-threshold",  &pdata->vac_ovp);
	if (ret) {
		pr_err("Failed to read node of ti,bq2560x,vac-ovp-threshold\n");
	}

	if (of_find_property(np, "qcom,thermal-mitigation",
			&bq->thermal_levels)) {
		bq->thermal_mitigation = devm_kzalloc(bq->dev,
				bq->thermal_levels,
				GFP_KERNEL);

		if (bq->thermal_mitigation == NULL) {
			pr_err("thermal mitigation kzalloc() failed.\n");

		}

		bq->thermal_levels /= sizeof(int);
		ret = of_property_read_u32_array(np,
				"qcom,thermal-mitigation",
				bq->thermal_mitigation, bq->thermal_levels);
		if (ret) {
			pr_err("Couldn't read thermal limits ret = %d\n", ret);

		}
	}


	return pdata;
}


static void bq2560x_init_jeita(struct bq2560x *bq)
{

	bq->batt_temp = -EINVAL;


	bq->batt_hot_degc = 600;
	bq->batt_warm_degc = 450;
	bq->batt_cool_degc = 100;
	bq->batt_cold_degc = 0;

	bq->hot_temp_hysteresis = 50;
	bq->cold_temp_hysteresis = 50;

	bq->batt_cool_ma = 400;
	bq->batt_cool_mv = 4100;
	bq->batt_warm_ma = 400;
	bq->batt_warm_mv = 4100;

	bq->software_jeita_supported = true;



	bq2560x_parse_jeita_dt(&bq->client->dev, bq);
}

static int bq2560x_init_device(struct bq2560x *bq)
{
	int ret;

	bq2560x_disable_watchdog_timer(bq);

	bq2560x_enable_batfet(bq);

	ret = bq2560x_set_stat_ctrl(bq, bq->platform_data->statctrl);
	if (ret)
		pr_err("Failed to set stat pin control mode, ret = %d\n", ret);

	ret = bq2560x_set_prechg_current(bq, bq->platform_data->iprechg);
	if (ret)
		pr_err("Failed to set prechg current, ret = %d\n", ret);

	ret = bq2560x_set_term_current(bq, bq->platform_data->iterm);
	if (ret)
		pr_err("Failed to set termination current, ret = %d\n", ret);

	ret = bq2560x_set_boost_voltage(bq, bq->platform_data->boostv);
	if (ret)
		pr_err("Failed to set boost voltage, ret = %d\n", ret);

	ret = bq2560x_set_boost_current(bq, bq->platform_data->boosti);
	if (ret)
		pr_err("Failed to set boost current, ret = %d\n", ret);

	ret = bq2560x_set_acovp_threshold(bq, bq->platform_data->vac_ovp);
	if (ret)
		pr_err("Failed to set acovp threshold, ret = %d\n", ret);

	ret = bq2560x_set_int_mask(bq, REG0A_IINDPM_INT_MASK | REG0A_VINDPM_INT_MASK);
	if (ret)
		pr_err("Failed to set vindpm and iindpm int mask\n");

	ret = bq2560x_enable_charger(bq);
	if (ret) {
		pr_err("Failed to enable charger, ret = %d\n", ret);
	} else {
		bq->charge_enabled = true;
		pr_info("Charger Enabled Successfully!\n");
	}

	return 0;
}


static int bq2560x_detect_device(struct bq2560x *bq)
{
	int ret;
	u8 data;

	ret = bq2560x_read_byte(bq, &data, BQ2560X_REG_0B);
	if (ret == 0) {
		bq->part_no = (data & REG0B_PN_MASK) >> REG0B_PN_SHIFT;
		bq->revision = (data & REG0B_DEV_REV_MASK) >> REG0B_DEV_REV_SHIFT;
	}

	return ret;
}

static void bq2560x_check_jeita(struct bq2560x *bq)
{

	int ret;
	bool last_hot, last_warm, last_cool, last_cold;
	union power_supply_propval batt_prop = {0,};

	ret = bq2560x_get_batt_property(bq,
			POWER_SUPPLY_PROP_TEMP, &batt_prop);
	if (!ret)
		bq->batt_temp = batt_prop.intval;

	if (bq->batt_temp == -EINVAL)
		return;

	last_hot = bq->batt_hot;
	last_warm = bq->batt_warm;
	last_cool = bq->batt_cool;
	last_cold = bq->batt_cold;

	if (bq->batt_temp >= bq->batt_hot_degc) {/* HOT */
		if (!bq->batt_hot) {
			bq->batt_hot  = true;
			bq->batt_warm = false;
			bq->batt_cool = false;
			bq->batt_cold = false;
			bq->jeita_ma = 0;
			bq->jeita_mv = 0;
		}
	} else if (bq->batt_temp >= bq->batt_warm_degc) {/* WARM */
		if (!bq->batt_hot ||
				(bq->batt_temp < bq->batt_hot_degc - bq->hot_temp_hysteresis))  {
			bq->batt_hot  = false;
			bq->batt_warm = true;
			bq->batt_cool = false;
			bq->batt_cold = false;
			bq->jeita_mv = bq->batt_warm_mv;
			bq->jeita_ma = bq->batt_warm_ma;
		}
	} else if (bq->batt_temp < bq->batt_cold_degc) {/* COLD */
		if (!bq->batt_cold) {
			bq->batt_hot  = false;
			bq->batt_warm = false;
			bq->batt_cool = false;
			bq->batt_cold = true;
			bq->jeita_ma = 0;
			bq->jeita_mv = 0;
		}
	} else if (bq->batt_temp < bq->batt_cool_degc) {/* COOL */
		if (!bq->batt_cold ||
				(bq->batt_temp > bq->batt_cold_degc + bq->cold_temp_hysteresis)) {
			bq->batt_hot  = false;
			bq->batt_warm = false;
			bq->batt_cool = true;
			bq->batt_cold = false;
			bq->jeita_mv = bq->batt_cool_mv;
			bq->jeita_ma = bq->batt_cool_ma;
		}
	} else {/* NORMAL */
		bq->batt_hot  = false;
		bq->batt_warm = false;
		bq->batt_cool = false;
		bq->batt_cold = false;
	}

	bq->jeita_active = bq->batt_cool || bq->batt_hot ||
			bq->batt_cold || bq->batt_warm;

	if ((last_cold != bq->batt_cold) || (last_warm != bq->batt_warm) ||
			(last_cool != bq->batt_cool) || (last_hot != bq->batt_hot)) {
		bq2560x_update_charging_profile(bq);
		power_supply_changed(&bq->batt_psy);
		power_supply_changed(bq->usb_psy);
	} else if (bq->batt_hot || bq->batt_cold) {
		power_supply_changed(&bq->batt_psy);
		power_supply_changed(bq->usb_psy);
	}

}

static void bq2560x_check_batt_pres(struct bq2560x *bq)
{
	int ret = 0;
	bool last_batt_pres = bq->batt_present;

	ret = bq2560x_get_prop_batt_present(bq);
	if (!ret) {
		if (last_batt_pres != bq->batt_present) {
			ret = bq2560x_charging_disable(bq, BATT_PRES, !bq->batt_present);
			if (ret) {
				pr_err("failed to %s charging, ret = %d\n",
						bq->batt_full ? "disable" : "enable",
						ret);
			}
			power_supply_changed(&bq->batt_psy);
			power_supply_changed(bq->usb_psy);
		}
	}

}

static void bq2560x_check_batt_capacity(struct bq2560x *bq)
{
	int ret = 0;
	int last_batt_capacity = bq->batt_capacity;

	ret = bq2560x_get_prop_batt_capacity(bq);
	if (!ret) {
		if (last_batt_capacity != bq->batt_capacity) {
			pr_err("Battery capacity changed, new capacity = %d\n", bq->batt_capacity);
			power_supply_changed(&bq->batt_psy);
		}
	}

}

static void bq2560x_check_batt_full(struct bq2560x *bq)
{
	int ret = 0;
	bool last_batt_full = bq->batt_full;

	ret = bq2560x_get_prop_batt_full(bq);
	if (!ret) {
		if (last_batt_full != bq->batt_full) {
			ret = bq2560x_charging_disable(bq, BATT_FC, bq->batt_full);
			if (ret) {
				pr_err("failed to %s charging, ret = %d\n",
						bq->batt_full ? "disable" : "enable",
						ret);
			}
			power_supply_changed(&bq->batt_psy);
			power_supply_changed(bq->usb_psy);
		}
	}
}


static int calculate_jeita_poll_interval(struct bq2560x *bq)
{
	int interval;

	if (bq->batt_hot || bq->batt_cold)
		interval = 5;
	else if (bq->batt_warm || bq->batt_cool)
		interval = 10;
	else
		interval = 15;
	return interval;
}

static enum alarmtimer_restart bq2560x_jeita_alarm_cb(struct alarm *alarm,
		ktime_t now)
{
	struct bq2560x *bq = container_of(alarm,
			struct bq2560x, jeita_alarm);
	unsigned long ns;

	bq2560x_stay_awake(&bq->bq2560x_ws, WAKEUP_SRC_JEITA);
	schedule_delayed_work(&bq->charge_jeita_work, HZ/2);

	ns = calculate_jeita_poll_interval(bq) * 1000000000LL;
	alarm_forward_now(alarm, ns_to_ktime(ns));
	return ALARMTIMER_RESTART;
}

static void bq2560x_dump_status(struct bq2560x *bq);
static void bq2560x_charge_jeita_workfunc(struct work_struct *work)
{
	struct bq2560x *bq = container_of(work,
			struct bq2560x, charge_jeita_work.work);

	bq2560x_reset_watchdog_timer(bq);

	bq2560x_check_batt_pres(bq);
	bq2560x_check_batt_full(bq);
	bq2560x_check_batt_capacity(bq);

	bq2560x_dump_fg_reg(bq);

	bq2560x_check_jeita(bq);
	bq2560x_dump_status(bq);
	bq2560x_relax(&bq->bq2560x_ws, WAKEUP_SRC_JEITA);
}


static void bq2560x_discharge_jeita_workfunc(struct work_struct *work)
{
	struct bq2560x *bq = container_of(work,
			struct bq2560x, discharge_jeita_work.work);

	bq2560x_check_batt_pres(bq);
	bq2560x_check_batt_full(bq);
	bq2560x_check_batt_capacity(bq);

	bq2560x_dump_fg_reg(bq);

	bq2560x_check_jeita(bq);

	schedule_delayed_work(&bq->discharge_jeita_work,
			calculate_jeita_poll_interval(bq) * HZ);
}

static const unsigned char *charge_stat_str[] = {
	"Not Charging",
	"Precharging",
	"Fast Charging",
	"Charge Done",
};

static void bq2560x_dump_status(struct bq2560x *bq)
{
	u8 status;
	u8 addr;
	int ret;
	u8 val;
	union power_supply_propval batt_prop = {0,};

	ret = bq2560x_get_batt_property(bq,
				POWER_SUPPLY_PROP_CURRENT_NOW, &batt_prop);

	if (!ret)
			pr_err("FG current:%d\n", batt_prop.intval);

	for (addr = 0x0; addr <= 0x0B; addr++) {
		ret = bq2560x_read_byte(bq, &val, addr);
		if (!ret)
			pr_err("bq Reg[0x%02X] = 0x%02X\n", addr, val);
		else
			pr_err("bq Reg red err\n");
	}
	if (!bq->power_good)
		pr_info("Power Poor\n");
	if (!bq->vbus_good)
		pr_info("Vbus voltage not good!\n");
	if (bq->vindpm_triggered)
		pr_info("VINDPM triggered\n");
	if (bq->iindpm_triggered)
		pr_info("IINDPM triggered\n");
	if (bq->acov_triggered)
		pr_info("ACOV triggered\n");

	if (bq->fault_status & REG09_FAULT_WDT_MASK)
		pr_info("Watchdog timer expired!\n");
	if (bq->fault_status & REG09_FAULT_BOOST_MASK)
		pr_info("Boost fault occurred!\n");

	status = (bq->fault_status & REG09_FAULT_CHRG_MASK) >> REG09_FAULT_CHRG_SHIFT;
	if (status == REG09_FAULT_CHRG_INPUT)
		pr_info("input fault!\n");
	else if (status == REG09_FAULT_CHRG_THERMAL)
		pr_info("charge thermal shutdown fault!\n");
	else if (status == REG09_FAULT_CHRG_TIMER)
		pr_info("charge timer expired fault!\n");

	if (bq->fault_status & REG09_FAULT_BAT_MASK)
		pr_info("battery ovp fault!\n");

	if (!bq->software_jeita_supported) {
		status = (bq->fault_status & REG09_FAULT_NTC_MASK) >> REG09_FAULT_NTC_SHIFT;

		if (status == REG09_FAULT_NTC_WARM)
			pr_debug("JEITA ACTIVE: WARM\n");
		else if (status == REG09_FAULT_NTC_COOL)
			pr_debug("JEITA ACTIVE: COOL\n");
		else if (status == REG09_FAULT_NTC_COLD)
			pr_debug("JEITA ACTIVE: COLD\n");
		else if (status == REG09_FAULT_NTC_HOT)
			pr_debug("JEITA ACTIVE: HOT!\n");
	} else if (bq->jeita_active) {
		if (bq->batt_hot)
			pr_debug("JEITA ACTIVE: HOT\n");
		else if (bq->batt_warm)
			pr_debug("JEITA ACTIVE: WARM\n");
		else if (bq->batt_cool)
			pr_debug("JEITA ACTIVE: COOL\n");
		else if (bq->batt_cold)
			pr_debug("JEITA ACTIVE: COLD\n");
	}

	pr_info("%s\n", charge_stat_str[bq->charge_state]);
}


static void bq2560x_update_status(struct bq2560x *bq)
{
	u8 status;
	int ret;

	ret = bq2560x_read_byte(bq, &status, BQ2560X_REG_0A);
	if (ret) {
		pr_err("failed to read reg0a\n");
		return;
	}

	mutex_lock(&bq->data_lock);
	bq->vbus_good = !!(status & REG0A_VBUS_GD_MASK);
	bq->vindpm_triggered = !!(status & REG0A_VINDPM_STAT_MASK);
	bq->iindpm_triggered = !!(status & REG0A_IINDPM_STAT_MASK);
	bq->topoff_active = !!(status & REG0A_TOPOFF_ACTIVE_MASK);
	bq->acov_triggered = !!(status & REG0A_ACOV_STAT_MASK);
	mutex_unlock(&bq->data_lock);


	ret = bq2560x_read_byte(bq, &status, BQ2560X_REG_09);
	ret = bq2560x_read_byte(bq, &status, BQ2560X_REG_09);
	if (ret)
		return;

	mutex_lock(&bq->data_lock);
	bq->fault_status = status;
	mutex_unlock(&bq->data_lock);

}


static irqreturn_t bq2560x_charger_interrupt(int irq, void *dev_id)
{
	struct bq2560x *bq = dev_id;

	u8 status;
	int ret;

	mutex_lock(&bq->irq_complete);
	bq->irq_waiting = true;
	if (!bq->resume_completed) {
		dev_dbg(bq->dev, "IRQ triggered before device-resume\n");
		if (!bq->irq_disabled) {
			disable_irq_nosync(irq);
			bq->irq_disabled = true;
		}
		mutex_unlock(&bq->irq_complete);
		return IRQ_HANDLED;
	}
	bq->irq_waiting = false;
	ret = bq2560x_read_byte(bq, &status, BQ2560X_REG_08);
	if (ret) {
		mutex_unlock(&bq->irq_complete);
		return IRQ_HANDLED;
	}

	mutex_lock(&bq->data_lock);
	bq->power_good = !!(status & REG08_PG_STAT_MASK);
	mutex_unlock(&bq->data_lock);

	if (!bq->power_good) {
		if (bq->usb_present)  {
			bq->usb_present = false;
			power_supply_set_present(bq->usb_psy, bq->usb_present);
		}
		if (bq->software_jeita_supported) {
			alarm_try_to_cancel(&bq->jeita_alarm);
		}

		bq2560x_disable_watchdog_timer(bq);

		schedule_delayed_work(&bq->discharge_jeita_work,
				calculate_jeita_poll_interval(bq) * HZ);

		pr_err("usb removed, set usb present = %d\n", bq->usb_present);
	} else if (bq->power_good && !bq->usb_present) {
		bq->usb_present = true;
		msleep(10);
		power_supply_set_present(bq->usb_psy, bq->usb_present);

		cancel_delayed_work(&bq->discharge_jeita_work);

		if (bq->software_jeita_supported) {
			ret = alarm_start_relative(&bq->jeita_alarm,
					ns_to_ktime(calculate_jeita_poll_interval(bq) * 1000000000LL));
			if (ret)
				pr_err("start alarm for JEITA detection failed, ret=%d\n",
					ret);
		}

		bq2560x_set_watchdog_timer(bq, 80);

		pr_err("usb plugged in, set usb present = %d\n", bq->usb_present);
	}

	bq2560x_update_status(bq);

	mutex_unlock(&bq->irq_complete);

	power_supply_changed(&bq->batt_psy);

	return IRQ_HANDLED;
}


static void determine_initial_status(struct bq2560x *bq)
{
	int ret;
	u8 status = 0;
	ret = bq2560x_get_hiz_mode(bq, &status);
	if (!ret)
		bq->in_hiz = !!status;

	bq2560x_charger_interrupt(bq->client->irq, bq);
}


static ssize_t bq2560x_show_registers(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct bq2560x *bq = dev_get_drvdata(dev);
	u8 addr;
	u8 val;
	u8 tmpbuf[200];
	int len;
	int idx = 0;
	int ret ;

	idx = snprintf(buf, PAGE_SIZE, "%s:\n", "bq2560x Reg");
	for (addr = 0x0; addr <= 0x0B; addr++) {
		ret = bq2560x_read_byte(bq, &val, addr);
		if (ret == 0) {
			len = snprintf(tmpbuf, PAGE_SIZE - idx, "Reg[0x%.2x] = 0x%.2x\n", addr, val);
			memcpy(&buf[idx], tmpbuf, len);
			idx += len;
		}
	}

	return idx;
}

static ssize_t bq2560x_store_registers(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct bq2560x *bq = dev_get_drvdata(dev);
	int ret;
	unsigned int reg;
	unsigned int val;

	ret = sscanf(buf, "%x %x", &reg, &val);
	if (ret == 2 && reg < 0x0B) {
		bq2560x_write_byte(bq, (unsigned char)reg, (unsigned char)val);
	}

	return count;
}

static ssize_t bq2560x_battery_test_status_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", BatteryTestStatus_enable);
}
static ssize_t bq2560x_battery_test_status_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int retval;
	unsigned int input;

	if (sscanf(buf, "%u", &input) != 1)
		retval = -EINVAL;
	else
	       BatteryTestStatus_enable = input;

	pr_err("BatteryTestStatus_enable = %d\n", BatteryTestStatus_enable);

	return retval;
}
static DEVICE_ATTR(registers, S_IRUGO | S_IWUSR, bq2560x_show_registers, bq2560x_store_registers);
static DEVICE_ATTR(BatteryTestStatus, S_IRUGO | S_IWUSR, bq2560x_battery_test_status_show, bq2560x_battery_test_status_store);

static struct attribute *bq2560x_attributes[] = {
	&dev_attr_registers.attr,
	&dev_attr_BatteryTestStatus.attr,
	NULL,
};

static const struct attribute_group bq2560x_attr_group = {
	.attrs = bq2560x_attributes,
};


static int show_registers(struct seq_file *m, void *data)
{
	struct bq2560x *bq = m->private;
	u8 addr;
	int ret;
	u8 val;

	for (addr = 0x0; addr <= 0x0B; addr++) {
		ret = bq2560x_read_byte(bq, &val, addr);
		if (!ret)
			seq_printf(m, "Reg[0x%02X] = 0x%02X\n", addr, val);
	}
	return 0;
}


static int reg_debugfs_open(struct inode *inode, struct file *file)
{
	struct bq2560x *bq = inode->i_private;

	return single_open(file, show_registers, bq);
}


static const struct file_operations reg_debugfs_ops = {
	.owner		= THIS_MODULE,
	.open		= reg_debugfs_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static void create_debugfs_entry(struct bq2560x *bq)
{
	bq->debug_root = debugfs_create_dir("bq2560x", NULL);
	if (!bq->debug_root)
		pr_err("Failed to create debug dir\n");

	if (bq->debug_root) {

		debugfs_create_file("registers", S_IFREG | S_IRUGO,
				bq->debug_root, bq, &reg_debugfs_ops);

		debugfs_create_x32("charging_disable_status", S_IFREG | S_IRUGO,
				bq->debug_root, &(bq->charging_disabled_status));

		debugfs_create_x32("fault_status", S_IFREG | S_IRUGO,
				bq->debug_root, &(bq->fault_status));

		debugfs_create_x32("vbus_type", S_IFREG | S_IRUGO,
				bq->debug_root, &(bq->vbus_type));

		debugfs_create_x32("charge_state", S_IFREG | S_IRUGO,
				bq->debug_root, &(bq->charge_state));

		debugfs_create_x32("skip_reads",
				S_IFREG | S_IWUSR | S_IRUGO,
				bq->debug_root,
				&(bq->skip_reads));
		debugfs_create_x32("skip_writes",
				S_IFREG | S_IWUSR | S_IRUGO,
				bq->debug_root,
				&(bq->skip_writes));
	}
}


static int bq2560x_charger_probe(struct i2c_client *client,
		const struct i2c_device_id *id)
{
	struct bq2560x *bq;
	struct power_supply *usb_psy;
	struct power_supply *bms_psy;

	int ret;

	usb_psy = power_supply_get_by_name("usb");
	if (!usb_psy) {
		dev_dbg(&client->dev, "USB supply not found, defer probe\n");
		return -EPROBE_DEFER;
	}

	bms_psy = power_supply_get_by_name("bms");
	if (!bms_psy) {
		dev_dbg(&client->dev, "bms supply not found, defer probe\n");
		return -EPROBE_DEFER;
	}

	bq = devm_kzalloc(&client->dev, sizeof(struct bq2560x), GFP_KERNEL);
	if (!bq) {
		pr_err("Out of memory\n");
		return -ENOMEM;
	}

	bq->dev = &client->dev;
	bq->usb_psy = usb_psy;
	bq->bms_psy = bms_psy;

	bq->client = client;
	i2c_set_clientdata(client, bq);

	mutex_init(&bq->i2c_rw_lock);
	mutex_init(&bq->data_lock);
	mutex_init(&bq->profile_change_lock);
	mutex_init(&bq->charging_disable_lock);
	mutex_init(&bq->irq_complete);

	bq->resume_completed = true;
	bq->irq_waiting = false;

	ret = bq2560x_detect_device(bq);
	if (ret) {
		pr_err("No bq2560x device found!\n");
		return -ENODEV;
	}

	bq2560x_init_jeita(bq);

	if (client->dev.of_node)
		bq->platform_data = bq2560x_parse_dt(&client->dev, bq);
	else
		bq->platform_data = client->dev.platform_data;

	if (!bq->platform_data) {
		pr_err("No platform data provided.\n");
		return -EINVAL;
	}

	if (gpio_is_valid(bq->gpio_ce)) {
		ret = devm_gpio_request(&client->dev, bq->gpio_ce, "bq2560x_ce");
		if (ret) {
			pr_err("Failed to request chip enable gpio %d:, err: %d\n", bq->gpio_ce, ret);
			return ret;
		}
		gpio_direction_output(bq->gpio_ce, 0);
	}

	ret = bq2560x_init_device(bq);
	if (ret) {
		pr_err("Failed to init device\n");
		return ret;
	}

	ret = bq2560x_psy_register(bq);
	if (ret)
		return ret;
	ret = bq2560x_regulator_init(bq);
	if (ret) {
		pr_err("Couldn't initialize bq2560x regulator ret=%d\n", ret);
		return ret;
	}

	INIT_DELAYED_WORK(&bq->charge_jeita_work, bq2560x_charge_jeita_workfunc);
	INIT_DELAYED_WORK(&bq->discharge_jeita_work, bq2560x_discharge_jeita_workfunc);
	INIT_DELAYED_WORK(&bq->factory_control_work, bq2560x_factory_mode_control_capacity_work);

	alarm_init(&bq->jeita_alarm, ALARM_BOOTTIME, bq2560x_jeita_alarm_cb);

	if (client->irq) {
		ret = devm_request_threaded_irq(&client->dev, client->irq, NULL,
				bq2560x_charger_interrupt,
				IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
				"bq2560x charger irq", bq);
		if (ret < 0) {
			pr_err("request irq for irq=%d failed, ret =%d\n", client->irq, ret);
			goto err_1;
		}
		enable_irq_wake(client->irq);
	}

	bq2560x_wakeup_src_init(bq);

	device_init_wakeup(bq->dev, 1);
	create_debugfs_entry(bq);

	ret = sysfs_create_group(&bq->dev->kobj, &bq2560x_attr_group);
	if (ret) {
		dev_err(bq->dev, "failed to register sysfs. err: %d\n", ret);
	}

	bq2560x_exit_hiz_mode(bq);
	determine_initial_status(bq);


	pr_info("bq2560x probe successfully, Part Num:%d, Revision:%d\n!",
			bq->part_no, bq->revision);

	schedule_delayed_work(&bq->factory_control_work, 0);

	return 0;

err_1:
	bq2560x_psy_unregister(bq);

	return ret;
}

static inline bool is_device_suspended(struct bq2560x *bq)
{
	return !bq->resume_completed;
}

static int bq2560x_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct bq2560x *bq = i2c_get_clientdata(client);

	mutex_lock(&bq->irq_complete);
	bq->resume_completed = false;
	mutex_unlock(&bq->irq_complete);
	pr_err("Suspend successfully!");

	return 0;
}

static int bq2560x_suspend_noirq(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct bq2560x *bq = i2c_get_clientdata(client);

	if (bq->irq_waiting) {
		pr_err_ratelimited("Aborting suspend, an interrupt was detected while suspending\n");
		return -EBUSY;
	}
	return 0;
}

static int bq2560x_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct bq2560x *bq = i2c_get_clientdata(client);


	mutex_lock(&bq->irq_complete);
	bq->resume_completed = true;
	if (bq->irq_waiting) {
		bq->irq_disabled = false;
		enable_irq(client->irq);
		mutex_unlock(&bq->irq_complete);
		bq2560x_charger_interrupt(client->irq, bq);
	} else {
		mutex_unlock(&bq->irq_complete);
	}

	power_supply_changed(&bq->batt_psy);
	pr_err("Resume successfully!");

	return 0;
}
static int bq2560x_charger_remove(struct i2c_client *client)
{
	struct bq2560x *bq = i2c_get_clientdata(client);

	alarm_try_to_cancel(&bq->jeita_alarm);

	cancel_delayed_work_sync(&bq->charge_jeita_work);
	cancel_delayed_work_sync(&bq->discharge_jeita_work);
	cancel_delayed_work_sync(&bq->factory_control_work);

	regulator_unregister(bq->otg_vreg.rdev);

	bq2560x_psy_unregister(bq);

	mutex_destroy(&bq->charging_disable_lock);
	mutex_destroy(&bq->profile_change_lock);
	mutex_destroy(&bq->data_lock);
	mutex_destroy(&bq->i2c_rw_lock);
	mutex_destroy(&bq->irq_complete);

	debugfs_remove_recursive(bq->debug_root);
	sysfs_remove_group(&bq->dev->kobj, &bq2560x_attr_group);


	return 0;
}


static void bq2560x_charger_shutdown(struct i2c_client *client)
{
	pr_info("Shutdown Successfully\n");
}

static struct of_device_id bq2560x_charger_match_table[] = {
	{.compatible = "ti,bq25600-charger",},
	{.compatible = "ti,bq25601-charger",},
	{},
};
MODULE_DEVICE_TABLE(of, bq2560x_charger_match_table);

static const struct i2c_device_id bq2560x_charger_id[] = {
	{ "bq25600-charger", BQ25600 },
	{ "bq25601-charger", BQ25601 },
	{},
};
MODULE_DEVICE_TABLE(i2c, bq2560x_charger_id);

static const struct dev_pm_ops bq2560x_pm_ops = {
	.resume		= bq2560x_resume,
	.suspend_noirq = bq2560x_suspend_noirq,
	.suspend	= bq2560x_suspend,
};
static struct i2c_driver bq2560x_charger_driver = {
	.driver 	= {
		.name 	= "bq2560x-charger",
		.owner 	= THIS_MODULE,
		.of_match_table = bq2560x_charger_match_table,
		.pm		= &bq2560x_pm_ops,
	},
	.id_table	= bq2560x_charger_id,

	.probe		= bq2560x_charger_probe,
	.remove		= bq2560x_charger_remove,
	.shutdown	= bq2560x_charger_shutdown,

};

module_i2c_driver(bq2560x_charger_driver);

MODULE_DESCRIPTION("TI BQ2560x Charger Driver");
MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Texas Instruments");
