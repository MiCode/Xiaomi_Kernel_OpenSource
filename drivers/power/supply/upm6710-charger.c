// SPDX-License-Identifier: GPL-2.0:
// Copyright (c) 2021 unisoc.

/*
 * Driver for the UPM Solutions upm6710 charger.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/bitops.h>
#include <linux/debugfs.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/math64.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/power/charger-manager.h>
#include <linux/power_supply.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>
#include <linux/slab.h>
#include <linux/sched.h>

#include <linux/power/upm6710_reg.h>

enum {
	ADC_IBUS,
	ADC_VBUS,
	ADC_VAC,
	ADC_VOUT,
	ADC_VBAT,
	ADC_IBAT,
	ADC_TBUS,
	ADC_TBAT,
	ADC_TDIE,
	ADC_MAX_NUM,
};

#define UPM6710_ROLE_STDALONE   0
#define UPM6710_ROLE_SLAVE	1
#define UPM6710_ROLE_MASTER	2

enum {
	UPM6710_STDALONE,
	UPM6710_SLAVE,
	UPM6710_MASTER,
};

static int upm6710_mode_data[] = {
	[UPM6710_STDALONE] = UPM6710_STDALONE,
	[UPM6710_MASTER] = UPM6710_ROLE_MASTER,
	[UPM6710_SLAVE] = UPM6710_ROLE_SLAVE,
};

#define	BAT_OVP_ALARM		BIT(7)
#define BAT_OCP_ALARM		BIT(6)
#define	BUS_OVP_ALARM		BIT(5)
#define	BUS_OCP_ALARM		BIT(4)
#define	BAT_UCP_ALARM		BIT(3)
#define	VBUS_INSERT		BIT(2)
#define VBAT_INSERT		BIT(1)
#define	ADC_DONE		BIT(0)

#define BAT_OVP_FAULT		BIT(7)
#define BAT_OCP_FAULT		BIT(6)
#define BUS_OVP_FAULT		BIT(5)
#define BUS_OCP_FAULT		BIT(4)
#define TBUS_TBAT_ALARM		BIT(3)
#define TS_BAT_FAULT		BIT(2)
#define	TS_BUS_FAULT		BIT(1)
#define	TS_DIE_FAULT		BIT(0)

#define VBAT_REG_STATUS_SHIFT			0
#define IBAT_REG_STATUS_SHIFT			1

#define VBAT_REG_STATUS_MASK		(1 << VBAT_REG_STATUS_SHIFT)
#define IBAT_REG_STATUS_MASK		(1 << VBAT_REG_STATUS_SHIFT)

#define ADC_REG_BASE			0x16
#define ADC_SAMPLE_15BITS		15
#define ADC_SAMPLE_12BITS		12

struct upm6710_charger_cfg {
	bool bat_ovp_disable;
	bool bat_ocp_disable;
	bool bat_ovp_alm_disable;
	bool bat_ocp_alm_disable;

	int bat_ovp_th;
	int bat_ovp_alm_th;
	int bat_ocp_th;
	int bat_ocp_alm_th;
	int bat_delta_volt;

	bool bus_ovp_alm_disable;
	bool bus_ocp_disable;
	bool bus_ocp_alm_disable;

	int bus_ovp_th;
	int bus_ovp_alm_th;
	int bus_ocp_th;
	int bus_ocp_alm_th;

	bool bat_ucp_alm_disable;

	int bat_ucp_alm_th;
	int ac_ovp_th;

	bool bat_therm_disable;
	bool bus_therm_disable;
	bool die_therm_disable;

	/* in % */
	int bat_therm_th;
	/* in % */
	int bus_therm_th;
	/* in degC */
	int die_therm_th;

	int sense_r_mohm;

	int adc_sample_bits;

	bool regulation_disable;
	int ibat_reg_th;
	int vbat_reg_th;
	int vdrop_th;
	int vdrop_deglitch;

	int ss_timeout;
	int wdt_timer;
};

struct upm6710_charger_info {
	struct device *dev;
	struct i2c_client *client;

	int part_no;
	int revision;

	int mode;

	struct mutex data_lock;
	struct mutex i2c_rw_lock;

	bool irq_waiting;
	bool irq_disabled;
	bool resume_completed;

	bool batt_present;
	bool vbus_present;

	bool usb_present;

	/* Register bit status */
	bool charge_enabled;

	/* ADC reading */
	int vbat_volt;
	int vbus_volt;
	int vout_volt;
	int vac_volt;

	int ibat_curr;
	int ibus_curr;

	int bat_temp;
	int bus_temp;
	int die_temp;

	/* alarm/fault status */
	bool bat_ovp_fault;
	bool bat_ocp_fault;
	bool bus_ovp_fault;
	bool bus_ocp_fault;

	bool bat_ovp_alarm;
	bool bat_ocp_alarm;
	bool bus_ovp_alarm;
	bool bus_ocp_alarm;

	bool bat_ucp_alarm;

	bool bat_therm_alarm;
	bool bus_therm_alarm;
	bool die_therm_alarm;

	bool bat_therm_fault;
	bool bus_therm_fault;
	bool die_therm_fault;

	bool bus_err_lo;
	bool bus_err_hi;

	bool therm_shutdown_flag;
	bool therm_shutdown_stat;

	bool vbat_reg;
	bool ibat_reg;

	int prev_alarm;
	int prev_fault;

	int chg_ma;
	int chg_mv;

	struct upm6710_charger_cfg *cfg;

	int skip_writes;
	int skip_reads;

	struct upm6710_platform_data *platform_data;

	struct delayed_work monitor_work;
	struct delayed_work wdt_work;
	struct delayed_work det_init_stat_work;

	struct dentry *debug_root;

	struct power_supply_desc psy_desc;
	struct power_supply_config psy_cfg;
	struct power_supply *upm6710_psy;

	unsigned int int_pin;
};

static int __upm6710_read_byte(struct upm6710_charger_info *upm, u8 reg, u8 *data)
{
	s32 ret;

	ret = i2c_smbus_read_byte_data(upm->client, reg);
	if (ret < 0) {
		dev_err(upm->dev, "i2c read fail: can't read from reg 0x%02X\n", reg);
		return ret;
	}

	*data = (u8) ret;

	return 0;
}

static int __upm6710_write_byte(struct upm6710_charger_info *upm, int reg, u8 val)
{
	s32 ret;

	ret = i2c_smbus_write_byte_data(upm->client, reg, val);
	if (ret < 0) {
		dev_err(upm->dev, "i2c write fail: can't write 0x%02X to reg 0x%02X: %d\n",
		       val, reg, ret);
		return ret;
	}
	return 0;
}

static int __upm6710_read_word(struct upm6710_charger_info *upm, u8 reg, u16 *data)
{
	s32 ret;

	ret = i2c_smbus_read_word_data(upm->client, reg);
	if (ret < 0) {
		dev_err(upm->dev, "i2c read fail: can't read from reg 0x%02X\n", reg);
		return ret;
	}

	*data = (u16) ret;

	return 0;
}

static int upm6710_read_byte(struct upm6710_charger_info *upm, u8 reg, u8 *data)
{
	int ret;

	if (upm->skip_reads) {
		*data = 0;
		return 0;
	}

	mutex_lock(&upm->i2c_rw_lock);
	ret = __upm6710_read_byte(upm, reg, data);
	mutex_unlock(&upm->i2c_rw_lock);

	return ret;
}

static int upm6710_write_byte(struct upm6710_charger_info *upm, u8 reg, u8 data)
{
	int ret;

	if (upm->skip_writes)
		return 0;

	mutex_lock(&upm->i2c_rw_lock);
	ret = __upm6710_write_byte(upm, reg, data);
	mutex_unlock(&upm->i2c_rw_lock);

	return ret;
}

static int upm6710_read_word(struct upm6710_charger_info *upm, u8 reg, u16 *data)
{
	int ret;

	if (upm->skip_reads) {
		*data = 0;
		return 0;
	}

	mutex_lock(&upm->i2c_rw_lock);
	ret = __upm6710_read_word(upm, reg, data);
	mutex_unlock(&upm->i2c_rw_lock);

	return ret;
}

static int upm6710_update_bits(struct upm6710_charger_info *upm, u8 reg, u8 mask, u8 data)
{
	int ret;
	u8 tmp;

	if (upm->skip_reads || upm->skip_writes)
		return 0;

	mutex_lock(&upm->i2c_rw_lock);
	ret = __upm6710_read_byte(upm, reg, &tmp);
	if (ret) {
		dev_err(upm->dev, "Failed: reg=%02X, ret=%d\n", reg, ret);
		goto out;
	}

	tmp &= ~mask;
	tmp |= data & mask;

	ret = __upm6710_write_byte(upm, reg, tmp);
	if (ret)
		dev_err(upm->dev, "Failed: reg=%02X, ret=%d\n", reg, ret);

out:
	mutex_unlock(&upm->i2c_rw_lock);
	return ret;
}

static int upm6710_enable_charge(struct upm6710_charger_info *upm, bool enable)
{
	u8 val;

	if (enable)
		val = UPM6710_CHG_ENABLE;
	else
		val = UPM6710_CHG_DISABLE;

	val <<= UPM6710_CHG_EN_SHIFT;

	return upm6710_update_bits(upm, UPM6710_REG_0C, UPM6710_CHG_EN_MASK, val);

}

static int upm6710_check_charge_enabled(struct upm6710_charger_info *upm, bool *enabled)
{
	int ret;
	u8 val;

	ret = upm6710_read_byte(upm, UPM6710_REG_0C, &val);
	if (ret < 0) {
		dev_err(upm->dev, "failed to check charge enable, ret = %d\n", ret);
		*enabled = false;
		return ret;
	}

	*enabled = !!(val & UPM6710_CHG_EN_MASK);

	return 0;
}

static int upm6710_reset(struct upm6710_charger_info *upm, bool reset)
{
	u8 val;

	if (reset)
		val = UPM6710_REG_RST_ENABLE;
	else
		val = UPM6710_REG_RST_DISABLE;

	val <<= UPM6710_REG_RST_SHIFT;

	return upm6710_update_bits(upm, UPM6710_REG_0B, UPM6710_REG_RST_MASK, val);
}

static int upm6710_enable_wdt(struct upm6710_charger_info *upm, bool enable)
{
	u8 val;

	if (enable)
		val = UPM6710_WATCHDOG_ENABLE;
	else
		val = UPM6710_WATCHDOG_DISABLE;

	val <<= UPM6710_WATCHDOG_DIS_SHIFT;

	return upm6710_update_bits(upm, UPM6710_REG_0B, UPM6710_WATCHDOG_DIS_MASK, val);
}

static int upm6710_set_wdt(struct upm6710_charger_info *upm, int ms)
{
	u8 val;

	if (ms == 500)
		val = UPM6710_WATCHDOG_0P5S;
	else if (ms == 1000)
		val = UPM6710_WATCHDOG_1S;
	else if (ms == 5000)
		val = UPM6710_WATCHDOG_5S;
	else if (ms == 30000)
		val = UPM6710_WATCHDOG_30S;
	else
		val = UPM6710_WATCHDOG_30S;

	val <<= UPM6710_WATCHDOG_SHIFT;

	return upm6710_update_bits(upm, UPM6710_REG_0B, UPM6710_WATCHDOG_MASK, val);
}

static int upm6710_enable_batovp(struct upm6710_charger_info *upm, bool enable)
{
	u8 val;

	if (enable)
		val = UPM6710_BAT_OVP_ENABLE;
	else
		val = UPM6710_BAT_OVP_DISABLE;

	val <<= UPM6710_BAT_OVP_DIS_SHIFT;

	return upm6710_update_bits(upm, UPM6710_REG_00, UPM6710_BAT_OVP_DIS_MASK, val);
}

static int upm6710_set_batovp_th(struct upm6710_charger_info *upm, int threshold)
{
	u8 val;

	if (threshold < UPM6710_BAT_OVP_BASE)
		threshold = UPM6710_BAT_OVP_BASE;
	else if (threshold > UPM6710_BAT_OVP_MAX)
		threshold = UPM6710_BAT_OVP_MAX;

	val = (threshold - UPM6710_BAT_OVP_BASE) / UPM6710_BAT_OVP_LSB;

	val <<= UPM6710_BAT_OVP_SHIFT;

	return upm6710_update_bits(upm, UPM6710_REG_00, UPM6710_BAT_OVP_MASK, val);
}

static int upm6710_enable_batovp_alarm(struct upm6710_charger_info *upm, bool enable)
{
	u8 val;

	if (enable)
		val = UPM6710_BAT_OVP_ALM_ENABLE;
	else
		val = UPM6710_BAT_OVP_ALM_DISABLE;

	val <<= UPM6710_BAT_OVP_ALM_DIS_SHIFT;

	return upm6710_update_bits(upm, UPM6710_REG_01, UPM6710_BAT_OVP_ALM_DIS_MASK, val);
}

static int upm6710_set_batovp_alarm_th(struct upm6710_charger_info *upm, int threshold)
{
	u8 val;

	if (threshold < UPM6710_BAT_OVP_ALM_BASE)
		threshold = UPM6710_BAT_OVP_ALM_BASE;
	else if (threshold > UPM6710_BAT_OVP_ALM_MAX)
		threshold = UPM6710_BAT_OVP_ALM_MAX;

	val = (threshold - UPM6710_BAT_OVP_ALM_BASE) / UPM6710_BAT_OVP_ALM_LSB;

	val <<= UPM6710_BAT_OVP_ALM_SHIFT;

	return upm6710_update_bits(upm, UPM6710_REG_01, UPM6710_BAT_OVP_ALM_MASK, val);
}

static int upm6710_enable_batocp(struct upm6710_charger_info *upm, bool enable)
{
	u8 val;

	if (enable)
		val = UPM6710_BAT_OCP_ENABLE;
	else
		val = UPM6710_BAT_OCP_DISABLE;

	val <<= UPM6710_BAT_OCP_DIS_SHIFT;

	return upm6710_update_bits(upm, UPM6710_REG_02, UPM6710_BAT_OCP_DIS_MASK, val);
}

static int upm6710_set_batocp_th(struct upm6710_charger_info *upm, int threshold)
{
	u8 val;

	if (threshold < UPM6710_BAT_OCP_BASE)
		threshold = UPM6710_BAT_OCP_BASE;
	else if (threshold > UPM6710_BAT_OCP_MAX)
		threshold = UPM6710_BAT_OCP_MAX;

	val = (threshold - UPM6710_BAT_OCP_BASE) / UPM6710_BAT_OCP_LSB;

	val <<= UPM6710_BAT_OCP_SHIFT;

	return upm6710_update_bits(upm, UPM6710_REG_02, UPM6710_BAT_OCP_MASK, val);
}

static int upm6710_enable_batocp_alarm(struct upm6710_charger_info *upm, bool enable)
{
	u8 val;

	if (enable)
		val = UPM6710_BAT_OCP_ALM_ENABLE;
	else
		val = UPM6710_BAT_OCP_ALM_DISABLE;

	val <<= UPM6710_BAT_OCP_ALM_DIS_SHIFT;

	return upm6710_update_bits(upm, UPM6710_REG_03,
				   UPM6710_BAT_OCP_ALM_DIS_MASK, val);
}

static int upm6710_set_batocp_alarm_th(struct upm6710_charger_info *upm, int threshold)
{
	u8 val;

	if (threshold < UPM6710_BAT_OCP_ALM_BASE)
		threshold = UPM6710_BAT_OCP_ALM_BASE;
	else if (threshold > UPM6710_BAT_OCP_ALM_MAX)
		threshold = UPM6710_BAT_OCP_ALM_MAX;

	val = (threshold - UPM6710_BAT_OCP_ALM_BASE) / UPM6710_BAT_OCP_ALM_LSB;

	val <<= UPM6710_BAT_OCP_ALM_SHIFT;

	return upm6710_update_bits(upm, UPM6710_REG_03,  UPM6710_BAT_OCP_ALM_MASK, val);
}

static int upm6710_set_busovp_th(struct upm6710_charger_info *upm, int threshold)
{
	u8 val;

	if (threshold < UPM6710_BUS_OVP_BASE)
		threshold = UPM6710_BUS_OVP_BASE;
	else if (threshold > UPM6710_BUS_OVP_MAX)
		threshold = UPM6710_BUS_OVP_MAX;

	val = (threshold - UPM6710_BUS_OVP_BASE) / UPM6710_BUS_OVP_LSB;

	val <<= UPM6710_BUS_OVP_SHIFT;

	return upm6710_update_bits(upm, UPM6710_REG_06, UPM6710_BUS_OVP_MASK, val);
}

static int upm6710_enable_busovp_alarm(struct upm6710_charger_info *upm, bool enable)
{
	u8 val;

	if (enable)
		val = UPM6710_BUS_OVP_ALM_ENABLE;
	else
		val = UPM6710_BUS_OVP_ALM_DISABLE;

	val <<= UPM6710_BUS_OVP_ALM_DIS_SHIFT;

	return upm6710_update_bits(upm, UPM6710_REG_07,
				   UPM6710_BUS_OVP_ALM_DIS_MASK, val);
}

static int upm6710_set_busovp_alarm_th(struct upm6710_charger_info *upm, int threshold)
{
	u8 val;

	if (threshold < UPM6710_BUS_OVP_ALM_BASE)
		threshold = UPM6710_BUS_OVP_ALM_BASE;
	else if (threshold > UPM6710_BUS_OVP_ALM_MAX)
		threshold = UPM6710_BUS_OVP_ALM_MAX;

	val = (threshold - UPM6710_BUS_OVP_ALM_BASE) / UPM6710_BUS_OVP_ALM_LSB;

	val <<= UPM6710_BUS_OVP_ALM_SHIFT;

	return upm6710_update_bits(upm, UPM6710_REG_07, UPM6710_BUS_OVP_ALM_MASK, val);
}

static int upm6710_enable_busocp(struct upm6710_charger_info *upm, bool enable)
{
	u8 val;

	if (enable)
		val = UPM6710_BUS_OCP_ENABLE;
	else
		val = UPM6710_BUS_OCP_DISABLE;

	val <<= UPM6710_BUS_OCP_DIS_SHIFT;

	return upm6710_update_bits(upm, UPM6710_REG_08, UPM6710_BUS_OCP_DIS_MASK, val);
}

static int upm6710_set_busocp_th(struct upm6710_charger_info *upm, int threshold)
{
	u8 val;

	if (threshold < UPM6710_BUS_OCP_BASE)
		threshold = UPM6710_BUS_OCP_BASE;
	else if (threshold > UPM6710_BUS_OCP_MAX)
		threshold = UPM6710_BUS_OCP_MAX;

	val = (threshold - UPM6710_BUS_OCP_BASE) / UPM6710_BUS_OCP_LSB;

	val <<= UPM6710_BUS_OCP_SHIFT;

	return upm6710_update_bits(upm, UPM6710_REG_08, UPM6710_BUS_OCP_MASK, val);
}

static int upm6710_enable_busocp_alarm(struct upm6710_charger_info *upm, bool enable)
{
	u8 val;

	if (enable)
		val = UPM6710_BUS_OCP_ALM_ENABLE;
	else
		val = UPM6710_BUS_OCP_ALM_DISABLE;

	val <<= UPM6710_BUS_OCP_ALM_DIS_SHIFT;

	return upm6710_update_bits(upm, UPM6710_REG_09,
				   UPM6710_BUS_OCP_ALM_DIS_MASK, val);
}

static int upm6710_set_busocp_alarm_th(struct upm6710_charger_info *upm, int threshold)
{
	u8 val;

	if (threshold < UPM6710_BUS_OCP_ALM_BASE)
		threshold = UPM6710_BUS_OCP_ALM_BASE;
	else if (threshold > UPM6710_BUS_OCP_ALM_MAX)
		threshold = UPM6710_BUS_OCP_ALM_MAX;

	val = (threshold - UPM6710_BUS_OCP_ALM_BASE) / UPM6710_BUS_OCP_ALM_LSB;

	val <<= UPM6710_BUS_OCP_ALM_SHIFT;

	return upm6710_update_bits(upm, UPM6710_REG_09, UPM6710_BUS_OCP_ALM_MASK, val);
}

static int upm6710_enable_batucp_alarm(struct upm6710_charger_info *upm, bool enable)
{
	u8 val;

	if (enable)
		val = UPM6710_BAT_UCP_ALM_ENABLE;
	else
		val = UPM6710_BAT_UCP_ALM_DISABLE;

	val <<= UPM6710_BAT_UCP_ALM_DIS_SHIFT;

	return upm6710_update_bits(upm, UPM6710_REG_04, UPM6710_BAT_UCP_ALM_DIS_MASK, val);
}

static int upm6710_set_batucp_alarm_th(struct upm6710_charger_info *upm, int threshold)
{
	u8 val;

	if (threshold < UPM6710_BAT_UCP_ALM_BASE)
		threshold = UPM6710_BAT_UCP_ALM_BASE;
	else if (threshold > UPM6710_BAT_UCP_ALM_MAX)
		threshold = UPM6710_BAT_UCP_ALM_MAX;

	val = (threshold - UPM6710_BAT_UCP_ALM_BASE) / UPM6710_BAT_UCP_ALM_LSB;

	val <<= UPM6710_BAT_UCP_ALM_SHIFT;

	return upm6710_update_bits(upm, UPM6710_REG_04, UPM6710_BAT_UCP_ALM_MASK, val);
}

static int upm6710_set_acovp_th(struct upm6710_charger_info *upm, int threshold)
{
	u8 val;

	if (threshold == UPM6710_AC_OVP_6P5V) {
		dev_info(upm->dev, "%s, VAC_OVP set default 6.5V\n", __func__);
		threshold = UPM6710_AC_OVP_MAX + UPM6710_AC_OVP_LSB;
	} else if (threshold < UPM6710_AC_OVP_BASE) {
		threshold = UPM6710_AC_OVP_BASE;
	} else if (threshold > UPM6710_AC_OVP_MAX) {
		threshold = UPM6710_AC_OVP_MAX;
	}

	val = (threshold - UPM6710_AC_OVP_BASE) /  UPM6710_AC_OVP_LSB;
	val <<= UPM6710_AC_OVP_SHIFT;

	return upm6710_update_bits(upm, UPM6710_REG_05, UPM6710_AC_OVP_MASK, val);
}

static int upm6710_set_vdrop_th(struct upm6710_charger_info *upm, int threshold)
{
	u8 val;

	if (threshold <= 300)
		val = UPM6710_VDROP_THRESHOLD_300MV;
	else
		val = UPM6710_VDROP_THRESHOLD_400MV;

	val <<= UPM6710_VDROP_THRESHOLD_SET_SHIFT;

	return upm6710_update_bits(upm, UPM6710_REG_05,
				   UPM6710_VDROP_THRESHOLD_SET_MASK,
				   val);
}

static int upm6710_set_vdrop_deglitch(struct upm6710_charger_info *upm, int us)
{
	u8 val;

	if (us <= 8)
		val = UPM6710_VDROP_DEGLITCH_8US;
	else
		val = UPM6710_VDROP_DEGLITCH_5MS;

	val <<= UPM6710_VDROP_DEGLITCH_SET_SHIFT;

	return upm6710_update_bits(upm, UPM6710_REG_05,
				   UPM6710_VDROP_DEGLITCH_SET_MASK, val);
}

static int upm6710_enable_bat_therm(struct upm6710_charger_info *upm, bool enable)
{
	u8 val;

	if (enable)
		val = UPM6710_TSBAT_ENABLE;
	else
		val = UPM6710_TSBAT_DISABLE;

	val <<= UPM6710_TSBAT_DIS_SHIFT;

	return upm6710_update_bits(upm, UPM6710_REG_0C, UPM6710_TSBAT_DIS_MASK, val);
}

/*
 * the input threshold is the raw value that would write to register directly.
 */
static int upm6710_set_bat_therm_th(struct upm6710_charger_info *upm, u8 threshold)
{
	return upm6710_write_byte(upm, UPM6710_REG_29, threshold);
}

static int upm6710_enable_bus_therm(struct upm6710_charger_info *upm, bool enable)
{
	u8 val;

	if (enable)
		val = UPM6710_TSBUS_ENABLE;
	else
		val = UPM6710_TSBUS_DISABLE;

	val <<= UPM6710_TSBUS_DIS_SHIFT;

	return upm6710_update_bits(upm, UPM6710_REG_0C, UPM6710_TSBUS_DIS_MASK, val);
}

/*
 * the input threshold is the raw value that would write to register directly.
 */
static int upm6710_set_bus_therm_th(struct upm6710_charger_info *upm, u8 threshold)
{
	return upm6710_write_byte(upm, UPM6710_REG_28, threshold);
}

static int upm6710_enable_die_therm(struct upm6710_charger_info *upm, bool enable)
{
	u8 val;

	if (enable)
		val = UPM6710_TDIE_ENABLE;
	else
		val = UPM6710_TDIE_DISABLE;

	val <<= UPM6710_TDIE_DIS_SHIFT;

	return upm6710_update_bits(upm, UPM6710_REG_0C, UPM6710_TDIE_DIS_MASK, val);
}

/*
 * please be noted that the unit here is degC
 */
static int upm6710_set_die_therm_th(struct upm6710_charger_info *upm, u8 threshold)
{
	u8 val;

	/* BE careful, LSB is here is 1/LSB, so we use multiply here */
	val = (threshold - UPM6710_TDIE_ALM_BASE) * UPM6710_TDIE_ALM_LSB;
	val <<= UPM6710_TDIE_ALM_SHIFT;

	return upm6710_update_bits(upm, UPM6710_REG_2A, UPM6710_TDIE_ALM_MASK, val);
}

static int upm6710_enable_adc(struct upm6710_charger_info *upm, bool enable)
{
	u8 val;

	if (enable)
		val = UPM6710_ADC_ENABLE;
	else
		val = UPM6710_ADC_DISABLE;

	val <<= UPM6710_ADC_EN_SHIFT;

	return upm6710_update_bits(upm, UPM6710_REG_14, UPM6710_ADC_EN_MASK, val);
}

static int upm6710_set_adc_average(struct upm6710_charger_info *upm, bool avg)
{
	u8 val;

	if (avg)
		val = UPM6710_ADC_AVG_ENABLE;
	else
		val = UPM6710_ADC_AVG_DISABLE;

	val <<= UPM6710_ADC_AVG_SHIFT;

	return upm6710_update_bits(upm, UPM6710_REG_14, UPM6710_ADC_AVG_MASK, val);
}

static int upm6710_set_adc_scanrate(struct upm6710_charger_info *upm, bool oneshot)
{
	u8 val;

	if (oneshot)
		val = UPM6710_ADC_RATE_ONESHOT;
	else
		val = UPM6710_ADC_RATE_CONTINUOUS;

	val <<= UPM6710_ADC_RATE_SHIFT;

	return upm6710_update_bits(upm, UPM6710_REG_14, UPM6710_ADC_RATE_MASK, val);
}

static int upm6710_set_adc_bits(struct upm6710_charger_info *upm, int bits)
{
	u8 val;

	if (bits > ADC_SAMPLE_15BITS)
		bits = ADC_SAMPLE_15BITS;
	if (bits < ADC_SAMPLE_12BITS)
		bits = ADC_SAMPLE_12BITS;
	val = ADC_SAMPLE_15BITS - bits;

	val <<= UPM6710_ADC_SAMPLE_SHIFT;

	return upm6710_update_bits(upm, UPM6710_REG_14, UPM6710_ADC_SAMPLE_MASK, val);
}

static int upm6710_get_adc_data(struct upm6710_charger_info *upm, int channel,  int *result)
{
	int ret;
	u16 val;
	s16 t;

	if (channel > ADC_MAX_NUM)
		return -EINVAL;

	ret = upm6710_read_word(upm, ADC_REG_BASE + (channel << 1), &val);
	if (ret < 0)
		return ret;

	t = val & 0xFF;
	t <<= 8;
	t |= (val >> 8) & 0xFF;
	*result = t;

	return 0;
}

static int upm6710_set_adc_scan(struct upm6710_charger_info *upm, int channel, bool enable)
{
	u8 reg;
	u8 mask;
	u8 shift;
	u8 val;

	if (channel > ADC_MAX_NUM)
		return -EINVAL;

	if (channel == ADC_IBUS) {
		reg = UPM6710_REG_14;
		shift = UPM6710_IBUS_ADC_DIS_SHIFT;
		mask = UPM6710_IBUS_ADC_DIS_MASK;
	} else {
		reg = UPM6710_REG_15;
		shift = 8 - channel;
		mask = 1 << shift;
	}

	if (enable)
		val = 0 << shift;
	else
		val = 1 << shift;

	return upm6710_update_bits(upm, reg, mask, val);
}

static int upm6710_set_alarm_int_mask(struct upm6710_charger_info *upm, u8 mask)
{
	int ret;
	u8 val;

	ret = upm6710_read_byte(upm, UPM6710_REG_0F, &val);
	if (ret)
		return ret;

	val |= mask;

	return upm6710_write_byte(upm, UPM6710_REG_0F, val);
}

static int upm6710_set_sense_resistor(struct upm6710_charger_info *upm, int r_mohm)
{
	u8 val;

	if (r_mohm == 2)
		val = UPM6710_SET_IBAT_SNS_RES_2MHM;
	else if (r_mohm == 5)
		val = UPM6710_SET_IBAT_SNS_RES_5MHM;
	else
		return -EINVAL;

	val <<= UPM6710_SET_IBAT_SNS_RES_SHIFT;

	return upm6710_update_bits(upm, UPM6710_REG_2B,
				   UPM6710_SET_IBAT_SNS_RES_MASK,
				   val);
}

static int upm6710_disable_regulation(struct upm6710_charger_info *upm, bool disable)
{
	u8 val;

	if (disable)
		val = UPM6710_EN_REGULATION_DISABLE;
	else
		val = UPM6710_EN_REGULATION_ENABLE;

	val <<= UPM6710_EN_REGULATION_SHIFT;

	return upm6710_update_bits(upm, UPM6710_REG_2B,
				UPM6710_EN_REGULATION_MASK,
				val);
}

static int upm6710_set_ss_timeout(struct upm6710_charger_info *upm, int timeout)
{
	u8 val;

	switch (timeout) {
	case 0:
		val = UPM6710_SS_TIMEOUT_DISABLE;
		break;
	case 12:
		val = UPM6710_SS_TIMEOUT_12P5MS;
		break;
	case 25:
		val = UPM6710_SS_TIMEOUT_25MS;
		break;
	case 50:
		val = UPM6710_SS_TIMEOUT_50MS;
		break;
	case 100:
		val = UPM6710_SS_TIMEOUT_100MS;
		break;
	case 400:
		val = UPM6710_SS_TIMEOUT_400MS;
		break;
	case 1500:
		val = UPM6710_SS_TIMEOUT_1500MS;
		break;
	case 100000:
		val = UPM6710_SS_TIMEOUT_100000MS;
		break;
	default:
		val = UPM6710_SS_TIMEOUT_DISABLE;
		break;
	}

	val <<= UPM6710_SS_TIMEOUT_SET_SHIFT;

	return upm6710_update_bits(upm, UPM6710_REG_2B,
				   UPM6710_SS_TIMEOUT_SET_MASK,
				   val);
}

static int upm6710_set_ibat_reg_th(struct upm6710_charger_info *upm, int th_ma)
{
	u8 val;

	if (th_ma == 200)
		val = UPM6710_IBAT_REG_200MA;
	else if (th_ma == 300)
		val = UPM6710_IBAT_REG_300MA;
	else if (th_ma == 400)
		val = UPM6710_IBAT_REG_400MA;
	else if (th_ma == 500)
		val = UPM6710_IBAT_REG_500MA;
	else
		val = UPM6710_IBAT_REG_500MA;

	val <<= UPM6710_IBAT_REG_SHIFT;

	return upm6710_update_bits(upm, UPM6710_REG_2C, UPM6710_IBAT_REG_MASK, val);
}

static int upm6710_set_vbat_reg_th(struct upm6710_charger_info *upm, int th_mv)
{
	u8 val;

	if (th_mv == 50)
		val = UPM6710_VBAT_REG_50MV;
	else if (th_mv == 100)
		val = UPM6710_VBAT_REG_100MV;
	else if (th_mv == 150)
		val = UPM6710_VBAT_REG_150MV;
	else
		val = UPM6710_VBAT_REG_200MV;

	val <<= UPM6710_VBAT_REG_SHIFT;

	return upm6710_update_bits(upm, UPM6710_REG_2C, UPM6710_VBAT_REG_MASK, val);
}

static int upm6710_check_vbus_error_status(struct upm6710_charger_info *upm)
{
	int ret;
	u8 data;

	upm->bus_err_lo = false;
	upm->bus_err_hi = false;

	ret = upm6710_read_byte(upm, UPM6710_REG_0A, &data);
	if (ret == 0) {
		dev_err(upm->dev, "vbus error >>>>%02x\n", data);
		upm->bus_err_lo = !!(data & UPM6710_VBUS_ERRORLO_STAT_MASK);
		upm->bus_err_hi = !!(data & UPM6710_VBUS_ERRORHI_STAT_MASK);
	}

	return ret;
}

static int upm6710_get_work_mode(struct upm6710_charger_info *upm, int *mode)
{
	int ret;
	u8 val;

	ret = upm6710_read_byte(upm, UPM6710_REG_0C, &val);

	if (ret) {
		dev_err(upm->dev, "Failed to read operation mode register\n");
		return ret;
	}

	val = (val & UPM6710_MS_MASK) >> UPM6710_MS_SHIFT;
	if (val == UPM6710_MS_MASTER)
		*mode = UPM6710_ROLE_MASTER;
	else if (val == UPM6710_MS_SLAVE)
		*mode = UPM6710_ROLE_SLAVE;
	else
		*mode = UPM6710_ROLE_STDALONE;

	dev_info(upm->dev, "work mode:%s\n", *mode == UPM6710_ROLE_STDALONE ? "Standalone" :
		 (*mode == UPM6710_ROLE_SLAVE ? "Slave" : "Master"));
	return ret;
}

static int upm6710_detect_device(struct upm6710_charger_info *upm)
{
	int ret;
	u8 data;

	ret = upm6710_read_byte(upm, UPM6710_REG_13, &data);
	if (ret < 0) {
		dev_err(upm->dev, "Failed to get device id, ret = %d\n", ret);
		return ret;
	}
	upm->part_no = (data & UPM6710_DEV_ID_MASK);
	upm->part_no >>= UPM6710_DEV_ID_SHIFT;

	if (upm->part_no != UPM6710_DEV_ID) {
		dev_err(upm->dev, "The device id is 0x%x\n", upm->part_no);
		ret = -EINVAL;
	}

	return ret;
}

static int upm6710_parse_dt(struct upm6710_charger_info *upm, struct device *dev)
{
	int ret;
	struct device_node *np = dev->of_node;

	upm->cfg = devm_kzalloc(dev, sizeof(struct upm6710_charger_info),
					GFP_KERNEL);

	if (!upm->cfg)
		return -ENOMEM;

	upm->cfg->bat_ovp_disable =
		of_property_read_bool(np, "uni,upm6710,bat-ovp-disable");
	upm->cfg->bat_ocp_disable =
		of_property_read_bool(np, "uni,upm6710,bat-ocp-disable");
	upm->cfg->bat_ovp_alm_disable =
		of_property_read_bool(np, "uni,upm6710,bat-ovp-alarm-disable");
	upm->cfg->bat_ocp_alm_disable =
		of_property_read_bool(np, "uni,upm6710,bat-ocp-alarm-disable");
	upm->cfg->bus_ocp_disable =
		of_property_read_bool(np, "uni,upm6710,bus-ocp-disable");
	upm->cfg->bus_ovp_alm_disable =
		of_property_read_bool(np, "uni,upm6710,bus-ovp-alarm-disable");
	upm->cfg->bus_ocp_alm_disable
		= of_property_read_bool(np, "uni,upm6710,bus-ocp-alarm-disable");
	upm->cfg->bat_ucp_alm_disable
		= of_property_read_bool(np, "uni,upm6710,bat-ucp-alarm-disable");
	upm->cfg->bat_therm_disable
		= of_property_read_bool(np, "uni,upm6710,bat-therm-disable");
	upm->cfg->bus_therm_disable
		= of_property_read_bool(np, "uni,upm6710,bus-therm-disable");
	upm->cfg->die_therm_disable
		= of_property_read_bool(np, "uni,upm6710,die-therm-disable");
	upm->cfg->regulation_disable
		= of_property_read_bool(np, "uni,upm6710,regulation-disable");
	upm->int_pin = of_get_named_gpio(np, "irq-gpio", 0);

	if (!gpio_is_valid(upm->int_pin))
		dev_info(upm->dev, "no irq pin provided\n");

	ret = of_property_read_u32(np, "uni,upm6710,bat-ovp-threshold",
				   &upm->cfg->bat_ovp_th);
	if (ret) {
		dev_err(upm->dev, "failed to read bat-ovp-threshold\n");
		return ret;
	}

	ret = of_property_read_u32(np, "uni,upm6710,bat-ovp-alarm-threshold",
				   &upm->cfg->bat_ovp_alm_th);
	if (ret) {
		dev_err(upm->dev, "failed to read bat-ovp-alarm-threshold\n");
		return ret;
	}

	ret = of_property_read_u32(np, "uni,upm6710,bat-ocp-threshold",
				   &upm->cfg->bat_ocp_th);
	if (ret) {
		dev_err(upm->dev, "failed to read bat-ocp-threshold\n");
		return ret;
	}

	ret = of_property_read_u32(np, "uni,upm6710,bat-ocp-alarm-threshold",
				   &upm->cfg->bat_ocp_alm_th);
	if (ret) {
		dev_err(upm->dev, "failed to read bat-ocp-alarm-threshold\n");
		return ret;
	}

	ret = of_property_read_u32(np, "uni,upm6710,bus-ovp-threshold",
				   &upm->cfg->bus_ovp_th);
	if (ret) {
		dev_err(upm->dev, "failed to read bus-ovp-threshold\n");
		return ret;
	}

	ret = of_property_read_u32(np, "uni,upm6710,bus-ovp-alarm-threshold",
				   &upm->cfg->bus_ovp_alm_th);
	if (ret) {
		dev_err(upm->dev, "failed to read bus-ovp-alarm-threshold\n");
		return ret;
	}

	ret = of_property_read_u32(np, "uni,upm6710,bus-ocp-threshold",
				   &upm->cfg->bus_ocp_th);
	if (ret) {
		dev_err(upm->dev, "failed to read bus-ocp-threshold\n");
		return ret;
	}

	ret = of_property_read_u32(np, "uni,upm6710,bus-ocp-alarm-threshold",
				   &upm->cfg->bus_ocp_alm_th);
	if (ret) {
		dev_err(upm->dev, "failed to read bus-ocp-alarm-threshold\n");
		return ret;
	}

	ret = of_property_read_u32(np, "uni,upm6710,bat-ucp-alarm-threshold",
				   &upm->cfg->bat_ucp_alm_th);
	if (ret) {
		dev_err(upm->dev, "failed to read bat-ucp-alarm-threshold\n");
		return ret;
	}

	ret = of_property_read_u32(np, "uni,upm6710,bat-therm-threshold",
				   &upm->cfg->bat_therm_th);
	if (ret) {
		dev_err(upm->dev, "failed to read bat-therm-threshold\n");
		return ret;
	}

	ret = of_property_read_u32(np, "uni,upm6710,bus-therm-threshold",
				   &upm->cfg->bus_therm_th);
	if (ret) {
		dev_err(upm->dev, "failed to read bus-therm-threshold\n");
		return ret;
	}

	ret = of_property_read_u32(np, "uni,upm6710,die-therm-threshold",
				   &upm->cfg->die_therm_th);
	if (ret) {
		dev_err(upm->dev, "failed to read die-therm-threshold\n");
		return ret;
	}

	ret = of_property_read_u32(np, "uni,upm6710,ac-ovp-threshold",
				   &upm->cfg->ac_ovp_th);
	if (ret) {
		dev_err(upm->dev, "failed to read ac-ovp-threshold\n");
		return ret;
	}

	ret = of_property_read_u32(np, "uni,upm6710,sense-resistor-mohm",
				   &upm->cfg->sense_r_mohm);
	if (ret) {
		dev_err(upm->dev, "failed to read sense-resistor-mohm\n");
		return ret;
	}

	ret = of_property_read_u32(np, "uni,upm6710,adc-sample-bits",
				   &upm->cfg->adc_sample_bits);
	if (ret) {
		dev_err(upm->dev, "failed to read adc-sample-bits\n");
		return ret;
	}

	ret = of_property_read_u32(np, "uni,upm6710,ibat-regulation-threshold",
				   &upm->cfg->ibat_reg_th);
	if (ret) {
		dev_err(upm->dev, "failed to read ibat-regulation-threshold\n");
		return ret;
	}

	ret = of_property_read_u32(np, "uni,upm6710,vbat-regulation-threshold",
				   &upm->cfg->vbat_reg_th);
	if (ret) {
		dev_err(upm->dev, "failed to read vbat-regulation-threshold\n");
		return ret;
	}

	ret = of_property_read_u32(np, "uni,upm6710,vdrop-threshold",
				   &upm->cfg->vdrop_th);
	if (ret) {
		dev_err(upm->dev, "failed to read vdrop-threshold\n");
		return ret;
	}

	ret = of_property_read_u32(np, "uni,upm6710,vdrop-deglitch",
				   &upm->cfg->vdrop_deglitch);
	if (ret) {
		dev_err(upm->dev, "failed to read vdrop-deglitch\n");
		return ret;
	}

	ret = of_property_read_u32(np, "uni,upm6710,ss-timeout",
				   &upm->cfg->ss_timeout);
	if (ret) {
		dev_err(upm->dev, "failed to read ss-timeout\n");
		return ret;
	}

	ret = of_property_read_u32(np, "uni,upm6710,watchdog-timer",
				   &upm->cfg->wdt_timer);
	if (ret) {
		dev_err(upm->dev, "failed to read watchdog-timer\n");
		return ret;
	}

	if (upm->cfg->bat_ovp_th && upm->cfg->bat_ovp_alm_th) {
		upm->cfg->bat_delta_volt = upm->cfg->bat_ovp_th - upm->cfg->bat_ovp_alm_th;
		if (upm->cfg->bat_delta_volt < 0)
			upm->cfg->bat_delta_volt = 0;
	}

	return 0;
}

static int upm6710_init_protection(struct upm6710_charger_info *upm)
{
	int ret;

	ret = upm6710_enable_batovp(upm, !upm->cfg->bat_ovp_disable);
	if (ret)
		dev_err(upm->dev, "%s, failed to %s bat ovp, ret = %d\n",
			__func__, upm->cfg->bat_ovp_disable ? "disable" : "enable", ret);

	ret = upm6710_enable_batocp(upm, !upm->cfg->bat_ocp_disable);
	if (ret)
		dev_err(upm->dev, "%s, failed to %s bat ocp, ret = %d\n",
			__func__, upm->cfg->bat_ocp_disable ? "disable" : "enable", ret);

	ret = upm6710_enable_batovp_alarm(upm, !upm->cfg->bat_ovp_alm_disable);
	if (ret)
		dev_err(upm->dev, "%s, failed to %s bat ovp alarm, ret = %d\n",
			__func__, upm->cfg->bat_ovp_alm_disable ? "disable" : "enable", ret);

	ret = upm6710_enable_batocp_alarm(upm, !upm->cfg->bat_ocp_alm_disable);
	if (ret)
		dev_err(upm->dev, "%s, failed to %s bat ocp alarm, ret = %d\n",
			__func__, upm->cfg->bat_ocp_alm_disable ? "disable" : "enable", ret);

	ret = upm6710_enable_batucp_alarm(upm, !upm->cfg->bat_ucp_alm_disable);
	if (ret)
		dev_err(upm->dev, "%s, failed to %s bat ocp alarm, ret = %d\n",
			__func__, upm->cfg->bat_ucp_alm_disable ? "disable" : "enable", ret);

	ret = upm6710_enable_busovp_alarm(upm, !upm->cfg->bus_ovp_alm_disable);
	if (ret)
		dev_err(upm->dev, "%s, failed to %s bus ovp alarm, ret = %d\n",
			__func__, upm->cfg->bus_ovp_alm_disable ? "disable" : "enable", ret);

	ret = upm6710_enable_busocp(upm, !upm->cfg->bus_ocp_disable);
	if (ret)
		dev_err(upm->dev, "%s, failed to %s bus ocp, ret = %d\n",
			__func__, upm->cfg->bus_ocp_disable ? "disable" : "enable", ret);

	ret = upm6710_enable_busocp_alarm(upm, !upm->cfg->bus_ocp_alm_disable);
	if (ret)
		dev_err(upm->dev, "%s, failed to %s bus ocp alarm, ret = %d\n",
			__func__, upm->cfg->bus_ocp_alm_disable ? "disable" : "enable", ret);

	ret = upm6710_set_batovp_th(upm, upm->cfg->bat_ovp_th);
	if (ret)
		dev_err(upm->dev, "%s, failed to set bat ovp th %d, ret = %d\n",
			__func__, upm->cfg->bat_ovp_th, ret);

	ret = upm6710_set_batovp_alarm_th(upm, upm->cfg->bat_ovp_alm_th);
	if (ret)
		dev_err(upm->dev, "%s, failed to set bat ovp alarm th %d, ret = %d\n",
			__func__, upm->cfg->bat_ovp_alm_th, ret);

	ret = upm6710_set_batocp_th(upm, upm->cfg->bat_ocp_th);
	if (ret)
		dev_err(upm->dev, "%s, failed to set bat ocp th %d, ret = %d\n",
			__func__, upm->cfg->bat_ocp_th, ret);

	ret = upm6710_set_batocp_alarm_th(upm, upm->cfg->bat_ocp_alm_th);
	if (ret)
		dev_err(upm->dev, "%s, failed to set bat ocp alarm th %d, ret = %d\n",
			__func__, upm->cfg->bat_ocp_alm_th, ret);

	ret = upm6710_set_busovp_th(upm, upm->cfg->bus_ovp_th);
	if (ret)
		dev_err(upm->dev, "%s, failed to set bus ovp th %d, ret = %d\n",
			__func__, upm->cfg->bus_ovp_th, ret);

	ret = upm6710_set_busovp_alarm_th(upm, upm->cfg->bus_ovp_alm_th);
	if (ret)
		dev_err(upm->dev, "%s, failed to set bus ovp alarm th %d, ret = %d\n",
			__func__, upm->cfg->bus_ovp_alm_th, ret);

	ret = upm6710_set_busocp_th(upm, upm->cfg->bus_ocp_th);
	if (ret)
		dev_err(upm->dev, "%s, failed to set bus ocp th %d, ret = %d\n",
			__func__, upm->cfg->bus_ocp_th, ret);

	ret = upm6710_set_busocp_alarm_th(upm, upm->cfg->bus_ocp_alm_th);
	if (ret)
		dev_err(upm->dev, "%s, failed to set bus ocp alarm th %d, ret = %d\n",
			__func__, upm->cfg->bus_ocp_alm_th, ret);

	ret = upm6710_set_batucp_alarm_th(upm, upm->cfg->bat_ucp_alm_th);
	if (ret)
		dev_err(upm->dev, "%s, failed to set bat ucp th %d, ret = %d\n",
			__func__, upm->cfg->bat_ucp_alm_th, ret);

	ret = upm6710_set_bat_therm_th(upm, upm->cfg->bat_therm_th);
	if (ret)
		dev_err(upm->dev, "%s, failed to set die therm th %d, ret = %d\n",
			__func__, upm->cfg->bat_therm_th, ret);

	ret = upm6710_set_bus_therm_th(upm, upm->cfg->bus_therm_th);
	if (ret)
		dev_err(upm->dev, "%s, failed to set bus therm th %d, ret = %d\n",
			__func__, upm->cfg->bus_therm_th, ret);

	ret = upm6710_set_die_therm_th(upm, upm->cfg->die_therm_th);
	if (ret)
		dev_err(upm->dev, "%s, failed to set die therm th %d, ret = %d\n",
			__func__, upm->cfg->die_therm_th, ret);

	ret = upm6710_set_acovp_th(upm, upm->cfg->ac_ovp_th);
	if (ret)
		dev_err(upm->dev, "%s, failed to set ac ovp th %d, ret = %d\n",
			__func__, upm->cfg->ac_ovp_th, ret);

	/* The delay of 50ms is to ensure that the register can respond normally. */
	msleep(50);
	ret = upm6710_enable_bat_therm(upm, !upm->cfg->bat_therm_disable);
	if (ret)
		dev_err(upm->dev, "%s, failed to %s bat therm, ret = %d\n",
			__func__, upm->cfg->bat_therm_disable ? "disable" : "enable", ret);

	ret = upm6710_enable_bus_therm(upm, !upm->cfg->bus_therm_disable);
	if (ret)
		dev_err(upm->dev, "%s, failed to %s bus therm, ret = %d\n",
			__func__, upm->cfg->bus_therm_disable ? "disable" : "enable", ret);

	ret = upm6710_enable_die_therm(upm, !upm->cfg->die_therm_disable);
	if (ret)
		dev_err(upm->dev, "%s, failed to %s die therm, ret = %d\n",
			__func__, upm->cfg->die_therm_disable ? "disable" : "enable", ret);

	return 0;
}

static int upm6710_init_adc(struct upm6710_charger_info *upm)
{

	upm6710_set_adc_scanrate(upm, false);
	upm6710_set_adc_bits(upm, upm->cfg->adc_sample_bits);
	upm6710_set_adc_average(upm, true);
	upm6710_set_adc_scan(upm, ADC_IBUS, true);
	upm6710_set_adc_scan(upm, ADC_VBUS, true);
	upm6710_set_adc_scan(upm, ADC_VOUT, false);
	upm6710_set_adc_scan(upm, ADC_VBAT, true);
	upm6710_set_adc_scan(upm, ADC_IBAT, true);
	upm6710_set_adc_scan(upm, ADC_TBUS, true);
	upm6710_set_adc_scan(upm, ADC_TBAT, true);
	upm6710_set_adc_scan(upm, ADC_TDIE, true);
	upm6710_set_adc_scan(upm, ADC_VAC, true);

	upm6710_enable_adc(upm, true);

	return 0;
}

static int upm6710_init_int_src(struct upm6710_charger_info *upm)
{
	int ret;
	/*
	 * TODO:be careful ts bus and ts bat alarm bit mask is in
	 *	fault mask register, so you need call
	 *	upm6710_set_fault_int_mask for tsbus and tsbat alarm
	 */
	ret = upm6710_set_alarm_int_mask(upm, ADC_DONE |
					VBUS_INSERT |
					VBAT_INSERT);
	if (ret) {
		dev_err(upm->dev, "failed to set alarm mask:%d\n", ret);
		return ret;
	}

	return ret;
}

static int upm6710_init_regulation(struct upm6710_charger_info *upm)
{
	upm6710_set_ibat_reg_th(upm, upm->cfg->ibat_reg_th);
	upm6710_set_vbat_reg_th(upm, upm->cfg->vbat_reg_th);

	upm6710_set_vdrop_deglitch(upm, upm->cfg->vdrop_deglitch);
	upm6710_set_vdrop_th(upm, upm->cfg->vdrop_th);

	upm6710_disable_regulation(upm, upm->cfg->regulation_disable);

	return 0;
}

static int upm6710_init_device(struct upm6710_charger_info *upm)
{
	upm6710_reset(upm, false);
	upm6710_enable_wdt(upm, false);

	upm6710_set_ss_timeout(upm, upm->cfg->ss_timeout);
	upm6710_set_sense_resistor(upm, upm->cfg->sense_r_mohm);

	upm6710_init_protection(upm);
	upm6710_init_adc(upm);
	upm6710_init_int_src(upm);

	upm6710_init_regulation(upm);

	return 0;
}

static int upm6710_set_present(struct upm6710_charger_info *upm, bool present)
{
	upm->usb_present = present;

	if (present) {
		upm6710_init_device(upm);
		upm6710_enable_wdt(upm, true);
		upm6710_set_wdt(upm, upm->cfg->wdt_timer);
		schedule_delayed_work(&upm->wdt_work, 0);
	}
	return 0;
}


static ssize_t upm6710_show_registers(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct upm6710_charger_info *upm = dev_get_drvdata(dev);
	u8 addr;
	u8 val;
	u8 tmpbuf[300];
	int len;
	int idx = 0;
	int ret;

	idx = snprintf(buf, PAGE_SIZE, "%s:\n", "upm6710");
	for (addr = 0x0; addr <= 0x2A; addr++) {
		ret = upm6710_read_byte(upm, addr, &val);
		if (ret == 0) {
			len = snprintf(tmpbuf, PAGE_SIZE - idx,
				       "Reg[%.2X] = 0x%.2x\n", addr, val);
			memcpy(&buf[idx], tmpbuf, len);
			idx += len;
		}
	}

	return idx;
}

static ssize_t upm6710_store_register(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct upm6710_charger_info *upm = dev_get_drvdata(dev);
	int ret;
	unsigned int reg;
	unsigned int val;

	ret = sscanf(buf, "%x %x", &reg, &val);
	if (ret == 2 && reg <= 0x2A)
		upm6710_write_byte(upm, (unsigned char)reg, (unsigned char)val);

	return count;
}

static DEVICE_ATTR(registers, 0644, upm6710_show_registers, upm6710_store_register);

static struct attribute *upm6710_attributes[] = {
	&dev_attr_registers.attr,
	NULL,
};

static const struct attribute_group upm6710_attr_group = {
	.attrs = upm6710_attributes,
};

static enum power_supply_property upm6710_charger_props[] = {
	POWER_SUPPLY_PROP_CALIBRATE,
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_CURRENT_NOW,
	POWER_SUPPLY_PROP_TEMP,
	POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE,
	POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE_MAX,
	POWER_SUPPLY_PROP_HEALTH,
	POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT,
	POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT,
};

static void upm6710_check_alarm_status(struct upm6710_charger_info *upm);
static void upm6710_check_fault_status(struct upm6710_charger_info *upm);

static int upm6710_get_present_status(struct upm6710_charger_info *upm, int *intval)
{
	int ret = 0;
	u8 reg_val;
	bool result = false;

	if (*intval == CM_USB_PRESENT_CMD) {
		result = upm->usb_present;
	} else if (*intval == CM_BATTERY_PRESENT_CMD) {
		ret = upm6710_read_byte(upm, UPM6710_REG_0D, &reg_val);
		if (!ret)
			upm->batt_present = !!(reg_val & VBAT_INSERT);
		result = upm->batt_present;
	} else if (*intval == CM_VBUS_PRESENT_CMD) {
		ret = upm6710_read_byte(upm, UPM6710_REG_0D, &reg_val);
		if (!ret)
			upm->vbus_present  = !!(reg_val & VBUS_INSERT);
		result = upm->vbus_present;
	} else {
		dev_err(upm->dev, "get present cmd = %d is error\n", *intval);
	}

	*intval = result;

	return ret;
}

static int upm6710_get_temperature(struct upm6710_charger_info *upm, int *intval)
{
	int ret = 0;
	int result = 0;

	if (*intval == CMD_BATT_TEMP_CMD) {
		ret = upm6710_get_adc_data(upm, ADC_TBAT, &result);
		if (!ret)
			upm->bat_temp = result;
	} else if (*intval == CM_BUS_TEMP_CMD) {
		ret = upm6710_get_adc_data(upm, ADC_TBUS, &result);
		if (!ret)
			upm->bus_temp = result;
	} else if (*intval == CM_DIE_TEMP_CMD) {
		ret = upm6710_get_adc_data(upm, ADC_TDIE, &result);
		if (!ret)
			upm->die_temp = result;
	} else {
		dev_err(upm->dev, "get temperature cmd = %d is error\n", *intval);
	}

	*intval = result;

	return ret;
}

static void upm6710_charger_watchdog_work(struct work_struct *work)
{
	struct delayed_work *dwork = to_delayed_work(work);
	struct upm6710_charger_info *upm = container_of(dwork,
							struct upm6710_charger_info,
							wdt_work);

	if (upm6710_set_wdt(upm, upm->cfg->wdt_timer) < 0)
		dev_err(upm->dev, "Fail to feed watchdog\n");

	schedule_delayed_work(&upm->wdt_work, HZ * 15);
}

static int upm6710_charger_get_property(struct power_supply *psy,
				enum power_supply_property psp,
				union power_supply_propval *val)
{
	struct upm6710_charger_info *upm = power_supply_get_drvdata(psy);
	int result = 0;
	int ret, cmd;
	u8 reg_val;

	if (!upm) {
		pr_err("%s[%d], NULL pointer!!!\n", __func__, __LINE__);
		return -EINVAL;
	}

	switch (psp) {
	case POWER_SUPPLY_PROP_CALIBRATE:
		upm6710_check_charge_enabled(upm, &upm->charge_enabled);
		val->intval = upm->charge_enabled;
		break;
	case POWER_SUPPLY_PROP_PRESENT:
		cmd = val->intval;
		if (!upm6710_get_present_status(upm, &val->intval))
			dev_err(upm->dev, "fail to get present status, cmd = %d\n", cmd);

		break;
	case POWER_SUPPLY_PROP_ONLINE:
		ret = upm6710_read_byte(upm, UPM6710_REG_0D, &reg_val);
		if (!ret)
			upm->vbus_present  = !!(reg_val & VBUS_INSERT);
		val->intval = upm->vbus_present;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		ret = upm6710_get_adc_data(upm, ADC_VBAT, &result);
		if (!ret)
			upm->vbat_volt = result;

		val->intval = upm->vbat_volt * 1000;
		break;
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		if (val->intval == CM_IBAT_CURRENT_NOW_CMD) {
			ret = upm6710_get_adc_data(upm, ADC_IBAT, &result);
			if (!ret)
				upm->ibat_curr = result;

			val->intval = upm->ibat_curr * 1000;
			break;
		}

		upm6710_check_charge_enabled(upm, &upm->charge_enabled);
		if (!upm->charge_enabled) {
			val->intval = 0;
		} else {
			ret = upm6710_get_adc_data(upm, ADC_IBUS, &result);
			if (!ret)
				upm->ibus_curr = result;
			val->intval = upm->ibus_curr * 1000;
		}
		break;
	case POWER_SUPPLY_PROP_TEMP:
		cmd = val->intval;
		if (upm6710_get_temperature(upm, &val->intval))
			dev_err(upm->dev, "fail to get temperature, cmd = %d\n", cmd);
		break;
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE:
		ret = upm6710_get_adc_data(upm, ADC_VBUS, &result);
		if (!ret)
			upm->vbus_volt = result;

		val->intval = upm->vbus_volt * 1000;
		break;
	case POWER_SUPPLY_PROP_HEALTH:
		if (val->intval == CM_SOFT_ALARM_HEALTH_CMD) {
			val->intval = 0;
			break;
		}

		if (val->intval == CM_BUS_ERR_HEALTH_CMD) {
			upm6710_check_vbus_error_status(upm);
			val->intval = (upm->bus_err_lo  << CM_CHARGER_BUS_ERR_LO_SHIFT);
			val->intval |= (upm->bus_err_hi  << CM_CHARGER_BUS_ERR_HI_SHIFT);
			break;
		}

		upm6710_check_fault_status(upm);
		val->intval = ((upm->bat_ovp_fault << CM_CHARGER_BAT_OVP_FAULT_SHIFT)
			| (upm->bat_ocp_fault << CM_CHARGER_BAT_OCP_FAULT_SHIFT)
			| (upm->bus_ovp_fault << CM_CHARGER_BUS_OVP_FAULT_SHIFT)
			| (upm->bus_ocp_fault << CM_CHARGER_BUS_OCP_FAULT_SHIFT)
			| (upm->bat_therm_fault << CM_CHARGER_BAT_THERM_FAULT_SHIFT)
			| (upm->bus_therm_fault << CM_CHARGER_BUS_THERM_FAULT_SHIFT)
			| (upm->die_therm_fault << CM_CHARGER_DIE_THERM_FAULT_SHIFT));

		upm6710_check_alarm_status(upm);
		val->intval |= ((upm->bat_ovp_alarm << CM_CHARGER_BAT_OVP_ALARM_SHIFT)
			| (upm->bat_ocp_alarm << CM_CHARGER_BAT_OCP_ALARM_SHIFT)
			| (upm->bat_ucp_alarm << CM_CHARGER_BAT_UCP_ALARM_SHIFT)
			| (upm->bus_ovp_alarm << CM_CHARGER_BUS_OVP_ALARM_SHIFT)
			| (upm->bus_ocp_alarm << CM_CHARGER_BUS_OCP_ALARM_SHIFT)
			| (upm->bat_therm_alarm << CM_CHARGER_BAT_THERM_ALARM_SHIFT)
			| (upm->bus_therm_alarm << CM_CHARGER_BUS_THERM_ALARM_SHIFT)
			| (upm->die_therm_alarm << CM_CHARGER_DIE_THERM_ALARM_SHIFT));
		break;
	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
		upm6710_check_charge_enabled(upm, &upm->charge_enabled);
		if (!upm->charge_enabled)
			val->intval = 0;
		else
			val->intval = upm->cfg->bus_ocp_alm_th  * 1000;
		break;
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT:
		upm6710_check_charge_enabled(upm, &upm->charge_enabled);
		if (!upm->charge_enabled)
			val->intval = 0;
		else
			val->intval = upm->cfg->bat_ocp_alm_th * 1000;
		break;
	default:
		return -EINVAL;

	}

	return 0;
}

static int upm6710_charger_set_property(struct power_supply *psy,
				       enum power_supply_property prop,
				       const union power_supply_propval *val)
{
	struct upm6710_charger_info *upm = power_supply_get_drvdata(psy);
	int ret, value;

	if (!upm) {
		pr_err("%s[%d], NULL pointer!!!\n", __func__, __LINE__);
		return -EINVAL;
	}

	switch (prop) {
	case POWER_SUPPLY_PROP_CALIBRATE:
		if (!val->intval) {
			upm6710_enable_adc(upm, false);
			cancel_delayed_work_sync(&upm->wdt_work);
		}

		ret = upm6710_enable_charge(upm, val->intval);
		if (ret)
			dev_err(upm->dev, "%s, failed to %s charge\n",
				__func__, val->intval ? "enable" : "disable");

		if (upm6710_check_charge_enabled(upm, &upm->charge_enabled))
			dev_err(upm->dev, "%s, failed to check charge enabled\n", __func__);

		break;
	case POWER_SUPPLY_PROP_PRESENT:
		if (val->intval == CM_USB_PRESENT_CMD)
			upm6710_set_present(upm, true);
		break;
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE_MAX:
		ret = upm6710_set_batovp_th(upm, val->intval / 1000);
		if (ret)
			dev_err(upm->dev, "%s, failed to set bat ovp th %d mv, ret = %d\n",
				__func__, val->intval / 1000, ret);

		value = val->intval / 1000 - upm->cfg->bat_delta_volt;
		ret = upm6710_set_batovp_alarm_th(upm, value);
		if (ret)
			dev_err(upm->dev, "%s, failed to set bat ovp alm th %d mv, ret = %d\n",
				__func__, value, ret);
		break;

	default:
		return -EINVAL;
	}

	return 0;
}

static int upm6710_charger_is_writeable(struct power_supply *psy,
				       enum power_supply_property prop)
{
	int ret;

	switch (prop) {
	case POWER_SUPPLY_PROP_CALIBRATE:
	case POWER_SUPPLY_PROP_PRESENT:
		ret = 1;
		break;
	default:
		ret = 0;
		break;
	}
	return ret;
}

static int upm6710_psy_register(struct upm6710_charger_info *upm)
{
	upm->psy_cfg.drv_data = upm;
	upm->psy_cfg.of_node = upm->dev->of_node;

	if (upm->mode == UPM6710_ROLE_MASTER)
		upm->psy_desc.name = "upm6710-master";
	else if (upm->mode == UPM6710_ROLE_SLAVE)
		upm->psy_desc.name = "upm6710-slave";
	else
		upm->psy_desc.name = "upm6710-standalone";

	upm->psy_desc.type = POWER_SUPPLY_TYPE_UNKNOWN;
	upm->psy_desc.properties = upm6710_charger_props;
	upm->psy_desc.num_properties = ARRAY_SIZE(upm6710_charger_props);
	upm->psy_desc.get_property = upm6710_charger_get_property;
	upm->psy_desc.set_property = upm6710_charger_set_property;
	upm->psy_desc.property_is_writeable = upm6710_charger_is_writeable;


	upm->upm6710_psy = devm_power_supply_register(upm->dev,
						     &upm->psy_desc, &upm->psy_cfg);
	if (IS_ERR(upm->upm6710_psy)) {
		dev_err(upm->dev, "failed to register upm6710_psy\n");
		return PTR_ERR(upm->upm6710_psy);
	}

	dev_info(upm->dev, "%s power supply register successfully\n", upm->psy_desc.name);

	return 0;
}

static void upm6710_dump_reg(struct upm6710_charger_info *upm)
{

	int ret;
	u8 val;
	u8 addr;

	for (addr = 0x00; addr < 0x2F; addr++) {
		ret = upm6710_read_byte(upm, addr, &val);
		if (!ret)
			dev_err(upm->dev, "Reg[%02X] = 0x%02X\n", addr, val);
	}
}

static void upm6710_check_alarm_status(struct upm6710_charger_info *upm)
{
	int ret;
	u8 flag = 0;
	u8 stat = 0;

	mutex_lock(&upm->data_lock);
	ret = upm6710_read_byte(upm, UPM6710_REG_2D, &flag);
	if (!ret && (flag & UPM6710_VDROP_OVP_FLAG_MASK))
		dev_err(upm->dev, "VDROP OVP event, REG_FLAG_MASK[%02x] =0x%02X\n",
			UPM6710_REG_2D, flag);

	/* read to clear alarm flag */
	ret = upm6710_read_byte(upm, UPM6710_REG_0E, &flag);
	if (!ret && flag)
		dev_dbg(upm->dev, "INT_FLAG[%02x] =0x%02X\n", UPM6710_REG_0E, flag);

	ret = upm6710_read_byte(upm, UPM6710_REG_0D, &stat);
	if (!ret && stat != upm->prev_alarm) {
		dev_dbg(upm->dev, "INT_STAT[%02x]= 0X%02x\n", UPM6710_REG_0D, stat);
		upm->prev_alarm = stat;
		upm->bat_ovp_alarm = !!(stat & BAT_OVP_ALARM);
		upm->bat_ocp_alarm = !!(stat & BAT_OCP_ALARM);
		upm->bus_ovp_alarm = !!(stat & BUS_OVP_ALARM);
		upm->bus_ocp_alarm = !!(stat & BUS_OCP_ALARM);
		upm->batt_present  = !!(stat & VBAT_INSERT);
		upm->vbus_present  = !!(stat & VBUS_INSERT);
		upm->bat_ucp_alarm = !!(stat & BAT_UCP_ALARM);
	}

	ret = upm6710_read_byte(upm, UPM6710_REG_08, &stat);
	if (!ret && (stat & (UPM6710_IBUS_UCP_FALL_FLAG_MASK | UPM6710_IBUS_UCP_RISE_FLAG_MASK)))
		dev_err(upm->dev, "Ibus ucp rise or fall event, IBUS_OCP_UCP[%02x] = 0x%02X\n",
			UPM6710_REG_08, stat);

	ret = upm6710_read_byte(upm, UPM6710_REG_0A, &stat);
	if (!ret && (stat & UPM6710_CONV_OCP_FLAG_MASK))
		dev_err(upm->dev, "Internal MOSFET OCP event, CONVERTER_STATE[%02x] = 0x%02X\n",
			UPM6710_REG_0A, stat);

	upm6710_dump_reg(upm);
	mutex_unlock(&upm->data_lock);
}

static void upm6710_check_fault_status(struct upm6710_charger_info *upm)
{
	int ret;
	u8 flag = 0;
	u8 stat = 0;

	mutex_lock(&upm->data_lock);
	ret = upm6710_read_byte(upm, UPM6710_REG_10, &stat);
	if (!ret && stat)
		dev_err(upm->dev, "FAULT_STAT[%02X] = 0x%02X\n", UPM6710_REG_10, stat);

	ret = upm6710_read_byte(upm, UPM6710_REG_11, &flag);
	if (!ret && flag)
		dev_err(upm->dev, "FAULT_FLAG[%02X] = 0x%02X\n", UPM6710_REG_11, flag);

	if (!ret && flag != upm->prev_fault) {
		upm->prev_fault = flag;
		upm->bat_ovp_fault = !!(flag & BAT_OVP_FAULT);
		upm->bat_ocp_fault = !!(flag & BAT_OCP_FAULT);
		upm->bus_ovp_fault = !!(flag & BUS_OVP_FAULT);
		upm->bus_ocp_fault = !!(flag & BUS_OCP_FAULT);
		upm->bat_therm_fault = !!(flag & TS_BAT_FAULT);
		upm->bus_therm_fault = !!(flag & TS_BUS_FAULT);

		upm->bat_therm_alarm = !!(flag & TBUS_TBAT_ALARM);
		upm->bus_therm_alarm = !!(flag & TBUS_TBAT_ALARM);
	}

	mutex_unlock(&upm->data_lock);
}

/*
 * interrupt does nothing, just info event chagne, other module could get info
 * through power supply interface
 */
static irqreturn_t upm6710_charger_interrupt(int irq, void *dev_id)
{
	struct upm6710_charger_info *upm = dev_id;

	dev_info(upm->dev, "INT OCCURRED\n");
	cm_notify_event(upm->upm6710_psy, CM_EVENT_INT, NULL);

	return IRQ_HANDLED;
}

static void upm6710_determine_initial_status_work(struct work_struct *work)
{
	struct delayed_work *dwork = to_delayed_work(work);
	struct upm6710_charger_info *upm = container_of(dwork,
							struct upm6710_charger_info,
							det_init_stat_work);

	upm6710_dump_reg(upm);
}

static int show_registers(struct seq_file *m, void *data)
{
	struct upm6710_charger_info *upm = m->private;
	u8 addr;
	int ret;
	u8 val;

	for (addr = 0x0; addr <= 0x2B; addr++) {
		ret = upm6710_read_byte(upm, addr, &val);
		if (!ret)
			seq_printf(m, "Reg[%02X] = 0x%02X\n", addr, val);
	}
	return 0;
}

static int reg_debugfs_open(struct inode *inode, struct file *file)
{
	struct upm6710_charger_info *upm = inode->i_private;

	return single_open(file, show_registers, upm);
}

static const struct file_operations reg_debugfs_ops = {
	.owner		= THIS_MODULE,
	.open		= reg_debugfs_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static void create_debugfs_entry(struct upm6710_charger_info *upm)
{
	if (upm->mode == UPM6710_ROLE_MASTER)
		upm->debug_root = debugfs_create_dir("upm6710-master", NULL);
	else if (upm->mode == UPM6710_ROLE_SLAVE)
		upm->debug_root = debugfs_create_dir("upm6710-slave", NULL);
	else
		upm->debug_root = debugfs_create_dir("upm6710-standalone", NULL);

	if (!upm->debug_root)
		dev_err(upm->dev, "Failed to create debug dir\n");

	if (upm->debug_root) {
		debugfs_create_file("registers", 0444, upm->debug_root, upm, &reg_debugfs_ops);

		debugfs_create_x32("skip_reads", 0644, upm->debug_root, &(upm->skip_reads));
		debugfs_create_x32("skip_writes", 0644, upm->debug_root, &(upm->skip_writes));
	}
}

static const struct of_device_id upm6710_charger_match_table[] = {
	{
		.compatible = "uni,upm6710-standalone",
		.data = &upm6710_mode_data[UPM6710_STDALONE],
	},
	{
		.compatible = "uni,upm6710-master",
		.data = &upm6710_mode_data[UPM6710_MASTER],
	},

	{
		.compatible = "uni,upm6710-slave",
		.data = &upm6710_mode_data[UPM6710_SLAVE],
	},
	{},
};

static int upm6710_charger_probe(struct i2c_client *client,
					const struct i2c_device_id *id)
{
	struct upm6710_charger_info *upm;
	const struct of_device_id *match;
	struct device *dev = &client->dev;
	struct device_node *node = client->dev.of_node;
	struct i2c_adapter *adapter = to_i2c_adapter(client->dev.parent);
	int ret;

	if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_BYTE_DATA)) {
		dev_err(dev, "No support for SMBUS_BYTE_DATA\n");
		return -ENODEV;
	}

	upm = devm_kzalloc(dev, sizeof(struct upm6710_charger_info), GFP_KERNEL);
	if (!upm)
		return -ENOMEM;

	upm->dev = &client->dev;

	upm->client = client;
	i2c_set_clientdata(client, upm);

	mutex_init(&upm->i2c_rw_lock);
	mutex_init(&upm->data_lock);

	upm->resume_completed = true;
	upm->irq_waiting = false;

	ret = upm6710_detect_device(upm);
	if (ret) {
		dev_err(upm->dev, "No upm6710 device found!\n");
		return -ENODEV;
	}

	match = of_match_node(upm6710_charger_match_table, node);
	if (match == NULL) {
		dev_err(upm->dev, "device tree match not found!\n");
		return -ENODEV;
	}

	upm6710_get_work_mode(upm, &upm->mode);

	if (upm->mode !=  *(int *)match->data) {
		dev_err(upm->dev, "device operation mode mismatch with dts configuration\n");
		return -EINVAL;
	}

	ret = upm6710_parse_dt(upm, &client->dev);
	if (ret)
		return -EIO;

	ret = upm6710_init_device(upm);
	if (ret) {
		dev_err(upm->dev, "Failed to init device\n");
		return ret;
	}

	INIT_DELAYED_WORK(&upm->wdt_work, upm6710_charger_watchdog_work);
	INIT_DELAYED_WORK(&upm->det_init_stat_work, upm6710_determine_initial_status_work);
	ret = upm6710_psy_register(upm);
	if (ret)
		return ret;

	if (gpio_is_valid(upm->int_pin)) {
		ret = devm_gpio_request_one(upm->dev, upm->int_pin,
					    GPIOF_DIR_IN, "upm6710_int");
		if (ret) {
			dev_err(upm->dev, "int request failed\n");
			return ret;
		}
	}

	client->irq = gpio_to_irq(upm->int_pin);
	if (client->irq) {
		ret = devm_request_threaded_irq(&client->dev, client->irq,
						NULL, upm6710_charger_interrupt,
						IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
						"upm6710 charger irq", upm);
		if (ret < 0) {
			dev_err(upm->dev, "request irq for irq=%d failed, ret =%d\n",
				client->irq, ret);
			return ret;
		}
		enable_irq_wake(client->irq);
	}

	device_init_wakeup(upm->dev, 1);
	create_debugfs_entry(upm);

	ret = sysfs_create_group(&upm->dev->kobj, &upm6710_attr_group);
	if (ret) {
		dev_err(upm->dev, "failed to register sysfs. err: %d\n", ret);
		return ret;
	}

	schedule_delayed_work(&upm->det_init_stat_work, msecs_to_jiffies(100));
	dev_info(upm->dev, "upm6710 probe successfully, Part Num:%d\n!", upm->part_no);

	return 0;
}

static int upm6710_charger_remove(struct i2c_client *client)
{
	struct upm6710_charger_info *upm = i2c_get_clientdata(client);


	upm6710_enable_adc(upm, false);
	cancel_delayed_work_sync(&upm->wdt_work);

	mutex_destroy(&upm->data_lock);
	mutex_destroy(&upm->i2c_rw_lock);

	debugfs_remove_recursive(upm->debug_root);

	sysfs_remove_group(&upm->dev->kobj, &upm6710_attr_group);

	return 0;
}

static void upm6710_charger_shutdown(struct i2c_client *client)
{
	struct upm6710_charger_info *upm = i2c_get_clientdata(client);

	upm6710_enable_adc(upm, false);
	upm6710_enable_charge(upm, false);
	cancel_delayed_work_sync(&upm->wdt_work);
}

static const struct i2c_device_id upm6710_charger_id[] = {
	{"upm6710-standalone", UPM6710_ROLE_STDALONE},
	{"upm6710-master", UPM6710_ROLE_MASTER},
	{"upm6710-slave", UPM6710_ROLE_SLAVE},
	{},
};

static struct i2c_driver upm6710_charger_driver = {
	.driver		= {
		.name	= "upm6710-charger",
		.owner	= THIS_MODULE,
		.of_match_table = upm6710_charger_match_table,
	},
	.id_table	= upm6710_charger_id,
	.probe		= upm6710_charger_probe,
	.remove		= upm6710_charger_remove,
	.shutdown	= upm6710_charger_shutdown,
};

module_i2c_driver(upm6710_charger_driver);

MODULE_DESCRIPTION("UNISEMI POWER UPM6710 Charger Driver");
MODULE_LICENSE("GPL v2");
