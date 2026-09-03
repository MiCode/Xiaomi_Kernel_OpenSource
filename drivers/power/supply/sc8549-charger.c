// SPDX-License-Identifier: GPL-2.0:
// Copyright (c) 2021 unisoc.

/*
 * Driver for the SC Solutions sc8549 charger.
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
#include <linux/power_supply.h>
#include <linux/power/charger-manager.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>
#include <linux/regulator/of_regulator.h>
#include <linux/sched.h>
#include <linux/slab.h>

#include <linux/power/sc8549_reg.h>

enum {
	ADC_IBUS,
	ADC_VBUS,
	ADC_VAC,
	ADC_VOUT,
	ADC_VBAT,
	ADC_IBAT,
	ADC_TDIE,
	ADC_MAX_NUM,
};

#define SC8549_ROLE_STDALONE		0
#define SC8549_ROLE_SLAVE		1
#define SC8549_ROLE_MASTER		2

#define SAMP_R_2M_OHM			2
#define SAMP_R_5M_OHM			5

enum {
	SC8549_STDALONE,
	SC8549_SLAVE,
	SC8549_MASTER,
};

static int sc8549_mode_data[] = {
	[SC8549_STDALONE] = SC8549_ROLE_STDALONE,
	[SC8549_MASTER] = SC8549_ROLE_MASTER,
	[SC8549_SLAVE] = SC8549_ROLE_SLAVE,
};

#define ADC_REG_BASE			SC8549_REG_13
#define ADC_REG_POL_H_MASK		0x0F
#define ADC_REG_POL_H_SHIFT		8
#define ADC_REG_POL_L_MASK		0xFF
#define ADC_REG_POL_L_SHIFT		0

struct sc8549_cfg {
	bool bat_ovp_disable;
	bool bat_ocp_disable;
	bool bat_ovp_alm_disable;
	bool bat_ocp_alm_disable;

	int bat_ovp_th;
	int bat_ovp_alm_th;
	int bat_ovp_default_alm_th;
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

	bool die_therm_disable;

	int die_therm_th; /*in degC*/

	int sense_r_mohm;

	int ibat_reg_th;
	int vbat_reg_th;
	int vdrop_th;
	int vdrop_deglitch;
	int ss_timeout;
	int wdt_timer;
	int ibus_ucp;
	bool regulation_disable;

	const char *psy_fuel_gauge;
};

struct sc8549 {
	struct device *dev;
	struct i2c_client *client;

	int part_no;
	int revision;

	int mode;

	struct mutex data_lock;
	struct mutex i2c_rw_lock;
	struct mutex charging_disable_lock;
	struct mutex irq_complete;

	bool irq_waiting;
	bool irq_disabled;
	bool resume_completed;

	int irq_gpio;
	int irq;

	bool batt_present;
	bool vbus_present;

	bool usb_present;
	bool charge_enabled;	/* Register bit status */

	bool is_sc8549;
	int  vbus_error;

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

	bool bus_err_lo;
	bool bus_err_hi;

	bool therm_shutdown_flag;
	bool therm_shutdown_stat;

	bool vbat_reg;
	bool ibat_reg;

	int  prev_alarm;
	int  prev_fault;

	int chg_ma;
	int chg_mv;

	int charge_state;

	struct sc8549_cfg *cfg;

	int skip_writes;
	int skip_reads;

	struct sc8549_platform_data *platform_data;

	struct delayed_work monitor_work;
	struct delayed_work wdt_work;
	struct delayed_work det_init_stat_work;

	struct dentry *debug_root;

	struct power_supply_desc psy_desc;
	struct power_supply_config psy_cfg;
	struct power_supply *fc2_psy;
};

static void sc8549_dump_reg(struct sc8549 *sc);

static int __sc8549_read_byte(struct sc8549 *sc, u8 reg, u8 *data)
{
	s32 ret;

	ret = i2c_smbus_read_byte_data(sc->client, reg);
	if (ret < 0) {
		dev_err(sc->dev, "i2c read fail: can't read from reg 0x%02X\n", reg);
		return ret;
	}

	*data = (u8) ret;

	return 0;
}

static int __sc8549_write_byte(struct sc8549 *sc, int reg, u8 val)
{
	s32 ret;

	ret = i2c_smbus_write_byte_data(sc->client, reg, val);
	if (ret < 0) {
		dev_err(sc->dev, "i2c write fail: can't write 0x%02X to reg 0x%02X: %d\n",
			val, reg, ret);
		return ret;
	}
	return 0;
}

static int sc8549_read_byte(struct sc8549 *sc, u8 reg, u8 *data)
{
	int ret;

	if (sc->skip_reads) {
		*data = 0;
		return 0;
	}

	mutex_lock(&sc->i2c_rw_lock);
	ret = __sc8549_read_byte(sc, reg, data);
	mutex_unlock(&sc->i2c_rw_lock);

	return ret;
}

static int sc8549_write_byte(struct sc8549 *sc, u8 reg, u8 data)
{
	int ret;

	if (sc->skip_writes)
		return 0;

	mutex_lock(&sc->i2c_rw_lock);
	ret = __sc8549_write_byte(sc, reg, data);
	mutex_unlock(&sc->i2c_rw_lock);

	return ret;
}

static int sc8549_update_bits(struct sc8549 *sc, u8 reg, u8 mask, u8 data)
{
	int ret;
	u8 tmp;

	if (sc->skip_reads || sc->skip_writes)
		return 0;

	mutex_lock(&sc->i2c_rw_lock);
	ret = __sc8549_read_byte(sc, reg, &tmp);
	if (ret) {
		dev_err(sc->dev, "Failed: reg=%02X, ret=%d\n", reg, ret);
		goto out;
	}

	tmp &= ~mask;
	tmp |= data & mask;

	ret = __sc8549_write_byte(sc, reg, tmp);
	if (ret)
		dev_err(sc->dev, "Failed: reg=%02X, ret=%d\n", reg, ret);

out:
	mutex_unlock(&sc->i2c_rw_lock);
	return ret;
}

static int sc8549_enable_charge(struct sc8549 *sc, bool enable)
{
	int ret;
	u8 val;

	if (enable)
		val = SC8549_CHG_ENABLE;
	else
		val = SC8549_CHG_DISABLE;

	val <<= SC8549_CHG_EN_SHIFT;

	dev_info(sc->dev, "sc8549 charger %s\n", enable == false ? "disable" : "enable");
	ret = sc8549_update_bits(sc, SC8549_REG_07, SC8549_CHG_EN_MASK, val);

	return ret;
}

static int sc8549_check_charge_enabled(struct sc8549 *sc, bool *enabled)
{
	int ret;
	u8 val;

	ret = sc8549_read_byte(sc, SC8549_REG_07, &val);
	if (ret < 0) {
		dev_err(sc->dev, "failed to check charge enable, ret = %d\n", ret);
		*enabled = false;
		return ret;
	}

	*enabled = !!(val & SC8549_CHG_EN_MASK);
	return ret;
}

static int sc8549_reset(struct sc8549 *sc, bool reset)
{
	u8 val;

	if (reset)
		val = SC8549_REG_RST_ENABLE;
	else
		val = SC8549_REG_RST_DISABLE;

	val <<= SC8549_REG_RST_SHIFT;

	return sc8549_update_bits(sc, SC8549_REG_07, SC8549_REG_RST_MASK, val);
}

static int sc8549_disable_wdt(struct sc8549 *sc)
{
	return sc8549_update_bits(sc, SC8549_REG_09,
				 SC8549_WD_TIMEOUT_MASK,
				 SC8549_WD_TIMEOUT_DISABLE << SC8549_WD_TIMEOUT_SHIFT);
}


static int sc8549_set_wdt(struct sc8549 *sc, int ms)
{
	u8 val;

	if (ms == 200)
		val = SC8549_WD_TIMEOUT_0P2S;
	else if (ms == 500)
		val = SC8549_WD_TIMEOUT_0P5S;
	else if (ms == 1000)
		val = SC8549_WD_TIMEOUT_1S;
	else if (ms == 5000)
		val = SC8549_WD_TIMEOUT_5S;
	else
		val = SC8549_WD_TIMEOUT_30S;

	val <<= SC8549_WD_TIMEOUT_SHIFT;

	return sc8549_update_bits(sc, SC8549_REG_09,
				  SC8549_WD_TIMEOUT_MASK, val);
}

static int sc8549_enable_batovp(struct sc8549 *sc, bool enable)
{
	int ret;
	u8 val;

	if (enable)
		val = SC8549_BAT_OVP_ENABLE;
	else
		val = SC8549_BAT_OVP_DISABLE;

	val <<= SC8549_BAT_OVP_DIS_SHIFT;

	ret = sc8549_update_bits(sc, SC8549_REG_00,
				 SC8549_BAT_OVP_DIS_MASK, val);
	return ret;
}

static int sc8549_set_batovp_th(struct sc8549 *sc, int threshold)
{
	int ret;
	u8 val;

	if (threshold < SC8549_BAT_OVP_BASE)
		threshold = SC8549_BAT_OVP_BASE;
	else if (threshold > SC8549_BAT_OVP_MAX)
		threshold = SC8549_BAT_OVP_MAX;

	val = (threshold - SC8549_BAT_OVP_BASE) / SC8549_BAT_OVP_LSB;

	val <<= SC8549_BAT_OVP_SHIFT;

	ret = sc8549_update_bits(sc, SC8549_REG_00,
				 SC8549_BAT_OVP_MASK, val);
	return ret;
}

static int sc8549_enable_batocp(struct sc8549 *sc, bool enable)
{
	int ret;
	u8 val;

	if (enable)
		val = SC8549_BAT_OCP_ENABLE;
	else
		val = SC8549_BAT_OCP_DISABLE;

	val <<= SC8549_BAT_OCP_DIS_SHIFT;

	ret = sc8549_update_bits(sc, SC8549_REG_01,
				 SC8549_BAT_OCP_DIS_MASK, val);
	return ret;
}

static int sc8549_set_batocp_th(struct sc8549 *sc, int threshold)
{
	int ret;
	u8 val;

	if (sc->mode != SC8549_ROLE_STDALONE)
		threshold = DIV_ROUND_CLOSEST(threshold * SAMP_R_2M_OHM, SAMP_R_5M_OHM);

	if (threshold < SC8549_BAT_OCP_BASE)
		threshold = SC8549_BAT_OCP_BASE;
	else if (threshold > SC8549_BAT_OCP_MAX)
		threshold = SC8549_BAT_OCP_MAX;

	val = (threshold - SC8549_BAT_OCP_BASE) / SC8549_BAT_OCP_LSB;

	val <<= SC8549_BAT_OCP_SHIFT;

	ret = sc8549_update_bits(sc, SC8549_REG_01,
				 SC8549_BAT_OCP_MASK, val);
	return ret;
}

static int sc8549_set_busovp_th(struct sc8549 *sc, int threshold)
{
	int ret;
	u8 val;

	if (threshold < SC8549_BUS_OVP_BASE)
		threshold = SC8549_BUS_OVP_BASE;
	else if (threshold > SC8549_BUS_OVP_MAX)
		threshold = SC8549_BUS_OVP_MAX;

	val = (threshold - SC8549_BUS_OVP_BASE) / SC8549_BUS_OVP_LSB;

	val <<= SC8549_BUS_OVP_SHIFT;

	ret = sc8549_update_bits(sc, SC8549_REG_04,
				 SC8549_BUS_OVP_MASK, val);
	return ret;
}

static int sc8549_enable_busocp(struct sc8549 *sc, bool enable)
{
	int ret;
	u8 val;

	if (enable)
		val = SC8549_IBUS_OCP_ENABLE;
	else
		val = SC8549_IBUS_OCP_DISABLE;

	val <<= SC8549_IBUS_OCP_DIS_SHIFT;

	ret = sc8549_update_bits(sc, SC8549_REG_05,
				 SC8549_IBUS_OCP_DIS_MASK, val);
	return ret;
}

static int sc8549_set_busocp_th(struct sc8549 *sc, int threshold)
{
	int ret;
	u8 val;

	if (threshold < SC8549_IBUS_OCP_BASE)
		threshold = SC8549_IBUS_OCP_BASE;
	else if (threshold > SC8549_IBUS_OCP_MAX)
		threshold = SC8549_IBUS_OCP_MAX;

	val = (threshold - SC8549_IBUS_OCP_BASE) / SC8549_IBUS_OCP_LSB;

	val <<= SC8549_IBUS_OCP_SHIFT;

	ret = sc8549_update_bits(sc, SC8549_REG_05,
				 SC8549_IBUS_OCP_MASK, val);
	return ret;
}

static int sc8549_set_acovp_th(struct sc8549 *sc, int threshold)
{
	u8 val;

	if (threshold == SC8549_AC_OVP_6P5V) {
		dev_info(sc->dev, "%s, VAC_OVP set default 6.5V\n", __func__);
		threshold = SC8549_AC_OVP_MAX + SC8549_AC_OVP_LSB;
	} else if (threshold < SC8549_AC_OVP_BASE)
		threshold = SC8549_AC_OVP_BASE;
	else if (threshold > SC8549_AC_OVP_MAX)
		threshold = SC8549_AC_OVP_MAX;

	val = (threshold - SC8549_AC_OVP_BASE) / SC8549_AC_OVP_LSB;

	val <<= SC8549_AC_OVP_SHIFT;

	return sc8549_update_bits(sc, SC8549_REG_02,
				  SC8549_AC_OVP_MASK, val);
}

static int sc8549_set_vdrop_th(struct sc8549 *sc, int threshold)
{
	int ret;
	u8 val;

	if (threshold == 300)
		val = SC8549_VDROP_THRESHOLD_300MV;
	else
		val = SC8549_VDROP_THRESHOLD_400MV;

	val <<= SC8549_VDROP_THRESHOLD_SET_SHIFT;

	ret = sc8549_update_bits(sc, SC8549_REG_03,
				 SC8549_VDROP_THRESHOLD_SET_MASK,
				 val);

	return ret;
}

static int sc8549_set_vdrop_deglitch(struct sc8549 *sc, int us)
{
	int ret;
	u8 val;

	if (us == 8)
		val = SC8549_VDROP_DEGLITCH_8US;
	else
		val = SC8549_VDROP_DEGLITCH_5MS;

	val <<= SC8549_VDROP_DEGLITCH_SET_SHIFT;

	ret = sc8549_update_bits(sc, SC8549_REG_03,
				 SC8549_VDROP_DEGLITCH_SET_MASK,
				 val);
	return ret;
}

static int sc8549_enable_adc(struct sc8549 *sc, bool enable)
{
	int ret;
	u8 val;

	if (enable)
		val = SC8549_ADC_ENABLE;
	else
		val = SC8549_ADC_DISABLE;

	val <<= SC8549_ADC_EN_SHIFT;

	ret = sc8549_update_bits(sc, SC8549_REG_11,
				 SC8549_ADC_EN_MASK, val);
	return ret;
}

static int sc8549_set_adc_scanrate(struct sc8549 *sc, bool oneshot)
{
	int ret;
	u8 val;

	if (oneshot)
		val = SC8549_ADC_RATE_ONESHOT;
	else
		val = SC8549_ADC_RATE_CONTINUOUS;

	val <<= SC8549_ADC_RATE_SHIFT;

	ret = sc8549_update_bits(sc, SC8549_REG_11,
				 SC8549_ADC_RATE_MASK, val);
	return ret;
}

static int sc8549_get_adc_data(struct sc8549 *sc, int channel,  int *result)
{
	int ret;
	u8 val_l, val_h;
	u8 val_h_mask = ADC_REG_POL_H_MASK, val_h_shift = ADC_REG_POL_H_SHIFT;
	u8 val_l_mask = ADC_REG_POL_L_MASK, val_l_shift = ADC_REG_POL_L_SHIFT;
	u16 val;

	if (channel >= ADC_MAX_NUM)
		return 0;

	ret = sc8549_read_byte(sc, ADC_REG_BASE + (channel << 1), &val_h);
	if (ret < 0) {
		dev_err(sc->dev, "%s, failed to get adc high bit data\n", __func__);
		return ret;
	}

	ret = sc8549_read_byte(sc, ADC_REG_BASE + (channel << 1) + 1, &val_l);
	if (ret < 0) {
		dev_err(sc->dev, "%s, failed to get adc low bit data\n", __func__);
		return ret;
	}

	val = ((val_h & val_h_mask) << val_h_shift) | ((val_l & val_l_mask) << val_l_shift);

	if (sc->is_sc8549) {
		if (channel == ADC_IBUS)
			val = DIV_ROUND_CLOSEST(val * 15, 8);
		else if (channel == ADC_VBUS)
			val = DIV_ROUND_CLOSEST(val * 15, 4);
		else if (channel == ADC_VAC)
			val = val * 5;
		else if (channel == ADC_VOUT)
			val = DIV_ROUND_CLOSEST(val * 5, 4);
		else if (channel == ADC_VBAT)
			val = DIV_ROUND_CLOSEST(val * 5, 4);
		else if (channel == ADC_IBAT) {
			if (sc->mode == SC8549_STDALONE)
				val = DIV_ROUND_CLOSEST(val * 25, 8);
			else
				val = DIV_ROUND_CLOSEST(val * 25 * SAMP_R_5M_OHM,
							8 * SAMP_R_2M_OHM);
		} else if (channel == ADC_TDIE)
			val = DIV_ROUND_CLOSEST(val, 2);
	}

	*result = val;

	return 0;
}

static int sc8549_get_ibat_now_mA(struct sc8549 *sc, int *batt_ma)
{
	struct power_supply *fuel_gauge;
	union power_supply_propval fgu_val;
	int ret = 0;

	if (sc->cfg->psy_fuel_gauge) {
		fuel_gauge = power_supply_get_by_name(sc->cfg->psy_fuel_gauge);
		if (!fuel_gauge)
			return -ENODEV;

		fgu_val.intval = 0;
		ret = power_supply_get_property(fuel_gauge, POWER_SUPPLY_PROP_CURRENT_NOW,
						&fgu_val);
		power_supply_put(fuel_gauge);
		if (ret) {
			dev_err(sc->dev, "%s, get batt_ma error, ret=%d\n", __func__, ret);
			return ret;
		}

		*batt_ma = DIV_ROUND_CLOSEST(fgu_val.intval, 1000);
	} else {
		ret = sc8549_get_adc_data(sc, ADC_IBAT, batt_ma);
		if (ret)
			dev_err(sc->dev, "%s, get batt_ma error, ret=%d\n", __func__, ret);
	}

	return ret;
}

static int sc8549_set_adc_scan(struct sc8549 *sc, int channel, bool enable)
{
	int ret;
	u8 mask;
	u8 shift;
	u8 val;

	if (channel > ADC_MAX_NUM)
		return -EINVAL;

	if (channel == ADC_IBUS) {
		shift = SC8549_IBUS_ADC_DIS_SHIFT;
		mask = SC8549_IBUS_ADC_DIS_MASK;
	} else if (channel == ADC_TDIE) {
		shift = SC8549_TDIE_ADC_DIS_SHIFT;
		mask = SC8549_TDIE_ADC_DIS_MASK;
	} else {
		shift = 7 - channel;
		mask = 1 << shift;
	}

	if (enable)
		val = 0 << shift;
	else
		val = 1 << shift;

	ret = sc8549_update_bits(sc, SC8549_REG_12, mask, val);

	return ret;
}

static int sc8549_enable_adc_done_alarm_int(struct sc8549 *sc, bool enable)
{
	u8 val;

	if (enable)
		val = SC8549_ADC_DONE_MASK_ENABLE;
	else
		val = SC8549_ADC_DONE_MASK_DISABLE;

	val <<= SC8549_ADC_DONE_MASK_SHIFT;

	return sc8549_update_bits(sc, SC8549_REG_11,
				  SC8549_ADC_DONE_MASK_MASK, val);
}

static int sc8549_set_alarm_int_mask(struct sc8549 *sc, u8 mask)
{
	int ret;
	u8 val;

	ret = sc8549_read_byte(sc, SC8549_REG_10, &val);
	if (ret)
		return ret;

	val |= mask;

	return sc8549_write_byte(sc, SC8549_REG_10, val);
}

static int sc8549_set_sense_resistor(struct sc8549 *sc, int r_mohm)
{
	int ret;
	u8 val;

	if (r_mohm == SAMP_R_2M_OHM)
		val = SC8549_SET_IBAT_SNS_RES_2M_OHM;
	else if (r_mohm == SAMP_R_5M_OHM)
		val = SC8549_SET_IBAT_SNS_RES_5M_OHM;
	else
		return -EINVAL;

	/*
	 * In double CP mode, the actual sampling registance must be 2milliohm,
	 * and the register sampling resistance must be set to 5milliohm, otherwise,
	 * the battery overcurrent protection will be triggered.
	 */
	if (sc->mode != SC8549_ROLE_STDALONE)
		val = SC8549_SET_IBAT_SNS_RES_5M_OHM;

	val <<= SC8549_SET_IBAT_SNS_RES_SHIFT;

	ret = sc8549_update_bits(sc, SC8549_REG_08,
				 SC8549_SET_IBAT_SNS_RES_MASK,
				 val);
	return ret;
}

static int sc8549_disable_vbat_regulation(struct sc8549 *sc, bool disable)
{
	u8 val;

	if (disable)
		val = SC8549_VBAT_REG_DISABLE;
	else
		val = SC8549_VBAT_REG_ENABLE;

	val <<= SC8549_VBAT_REG_EN_SHIFT;

	return sc8549_update_bits(sc, SC8549_REG_0A,
				  SC8549_VBAT_REG_EN_MASK,
				  val);
}

static int sc8549_disable_ibat_regulation(struct sc8549 *sc, bool disable)
{
	u8 val;

	if (disable)
		val = SC8549_IBAT_REG_DISABLE;
	else
		val = SC8549_IBAT_REG_ENABLE;

	val <<= SC8549_IBAT_REG_EN_SHIFT;

	return sc8549_update_bits(sc, SC8549_REG_0B,
				  SC8549_IBAT_REG_EN_MASK,
				  val);
}

static int sc8549_disable_regulation(struct sc8549 *sc, bool disable)
{
	int ret = 0;

	ret = sc8549_disable_vbat_regulation(sc, disable);
	if (ret) {
		dev_err(sc->dev, "%s, failed to %s vbat regulation, ret=%d\n",
			__func__, disable ? "disable" : "enable", ret);
		goto done;
	}

	ret = sc8549_disable_ibat_regulation(sc, disable);
	if (ret) {
		dev_err(sc->dev, "%s, failed to %s ibat regulation, ret=%d\n",
			__func__, disable ? "disable" : "enable", ret);
		goto done;
	}

done:
	return ret;
}

static int sc8549_set_ss_timeout(struct sc8549 *sc, int timeout)
{
	u8 val;

	if (timeout < 40)
		val = SC8549_SS_TIMEOUT_DISABLE;
	else if (timeout < 80)
		val = SC8549_SS_TIMEOUT_40MS;
	else if (timeout < 320)
		val = SC8549_SS_TIMEOUT_80MS;
	else if (timeout < 1280)
		val = SC8549_SS_TIMEOUT_320MS;
	else if (timeout < 5120)
		val = SC8549_SS_TIMEOUT_1280MS;
	else if (timeout < 20480)
		val = SC8549_SS_TIMEOUT_5120MS;
	else if (timeout < 81920)
		val = SC8549_SS_TIMEOUT_20480MS;
	else
		val = SC8549_SS_TIMEOUT_81920MS;

	val <<= SC8549_SS_TIMEOUT_SET_SHIFT;

	return sc8549_update_bits(sc, SC8549_REG_08,
				 SC8549_SS_TIMEOUT_SET_MASK,
				 val);
}

static int sc8549_set_ibat_reg_th(struct sc8549 *sc, int th_ma)
{
	int ret;
	u8 val;

	if (th_ma == 200)
		val = SC8549_IBAT_REG_200MA;
	else if (th_ma == 300)
		val = SC8549_IBAT_REG_300MA;
	else if (th_ma == 400)
		val = SC8549_IBAT_REG_400MA;
	else
		val = SC8549_IBAT_REG_500MA;

	val <<= SC8549_IBAT_REG_SHIFT;
	ret = sc8549_update_bits(sc, SC8549_REG_0B,
				 SC8549_IBAT_REG_MASK,
				 val);

	return ret;
}

static int sc8549_set_vbat_reg_th(struct sc8549 *sc, int th_mv)
{
	int ret;
	u8 val;

	if (th_mv == 50)
		val = SC8549_VBAT_REG_50MV;
	else if (th_mv == 100)
		val = SC8549_VBAT_REG_100MV;
	else if (th_mv == 150)
		val = SC8549_VBAT_REG_150MV;
	else
		val = SC8549_VBAT_REG_200MV;

	val <<= SC8549_VBAT_REG_SHIFT;

	ret = sc8549_update_bits(sc, SC8549_REG_0A,
				 SC8549_VBAT_REG_MASK,
				 val);

	return ret;
}

static int sc8549_set_ibus_ucp(struct sc8549 *sc, int val)
{
	int ret;

	if (val < SC8549_IBUS_UCP_FALL_DG_SET_10US)
		val = SC8549_IBUS_UCP_FALL_DG_SET_10US;
	else if (val > SC8549_IBUS_UCP_FALL_DG_SET_5MS)
		val = SC8549_IBUS_UCP_FALL_DG_SET_5MS;

	val <<= SC8549_IBUS_UCP_FALL_DG_SET_SHIFT;

	ret = sc8549_update_bits(sc, SC8549_REG_05, SC8549_IBUS_UCP_FALL_DG_SET_MASK, val);
	return ret;
}

static int sc8549_check_vbus_error_status(struct sc8549 *sc)
{
	int ret;
	u8 data;

	sc->bus_err_lo = false;
	sc->bus_err_hi = false;

	ret = sc8549_read_byte(sc, SC8549_REG_06, &data);
	if (ret == 0) {
		dev_err(sc->dev, "vbus error >>>>%02x\n", data);
		sc->bus_err_lo = !!(data & SC8549_VBUS_ERRORLO_STAT_MASK);
		sc->bus_err_hi = !!(data & SC8549_VBUS_ERRORHI_STAT_MASK);
		sc->vbus_error = data;
	}

	return ret;
}

static int sc8549_detect_device(struct sc8549 *sc)
{
	int ret;
	u8 data;

	ret = sc8549_read_byte(sc, SC8549_REG_36, &data);
	if (ret < 0) {
		dev_err(sc->dev, "Failed to get device id, ret = %d\n", ret);
		return ret;
	}

	sc->part_no = (data & SC8549_DEV_ID_MASK);
	sc->part_no >>= SC8549_DEV_ID_SHIFT;

	if (sc->part_no != SC8549_DEV_ID) {
		dev_err(sc->dev, "The device id is 0x%x\n", sc->part_no);
		ret = -EINVAL;
	}

	return ret;
}

static int sc8549_parse_dt(struct sc8549 *sc, struct device *dev)
{
	int ret;
	struct device_node *np = dev->of_node;

	sc->cfg = devm_kzalloc(dev, sizeof(struct sc8549_cfg), GFP_KERNEL);
	if (!sc->cfg)
		return -ENOMEM;

	sc->cfg->bat_ovp_disable = of_property_read_bool(np,
			"sc,sc8549,bat-ovp-disable");
	sc->cfg->bat_ocp_disable = of_property_read_bool(np,
			"sc,sc8549,bat-ocp-disable");
	sc->cfg->bat_ovp_alm_disable = of_property_read_bool(np,
			"sc,sc8549,bat-ovp-alarm-disable");
	sc->cfg->bat_ocp_alm_disable = of_property_read_bool(np,
			"sc,sc8549,bat-ocp-alarm-disable");
	sc->cfg->bus_ocp_disable = of_property_read_bool(np,
			"sc,sc8549,bus-ocp-disable");
	sc->cfg->bus_ovp_alm_disable = of_property_read_bool(np,
			"sc,sc8549,bus-ovp-alarm-disable");
	sc->cfg->bus_ocp_alm_disable = of_property_read_bool(np,
			"sc,sc8549,bus-ocp-alarm-disable");
	sc->cfg->bat_ucp_alm_disable = of_property_read_bool(np,
			"sc,sc8549,bat-ucp-alarm-disable");

	ret = of_property_read_u32(np, "sc,sc8549,bat-ovp-threshold",
				   &sc->cfg->bat_ovp_th);
	if (ret) {
		dev_err(sc->dev, "failed to read bat-ovp-threshold\n");
		return ret;
	}

	ret = of_property_read_u32(np, "sc,sc8549,bat-ovp-alarm-threshold",
				   &sc->cfg->bat_ovp_alm_th);
	if (ret) {
		dev_err(sc->dev, "failed to read bat-ovp-alarm-threshold\n");
		return ret;
	}

	sc->cfg->bat_ovp_default_alm_th = sc->cfg->bat_ovp_alm_th;

	ret = of_property_read_u32(np, "sc,sc8549,bat-ocp-threshold",
				   &sc->cfg->bat_ocp_th);
	if (ret) {
		dev_err(sc->dev, "failed to read bat-ocp-threshold\n");
		return ret;
	}

	ret = of_property_read_u32(np, "sc,sc8549,bat-ocp-alarm-threshold",
				   &sc->cfg->bat_ocp_alm_th);
	if (ret) {
		dev_err(sc->dev, "failed to read bat-ocp-alarm-threshold\n");
		return ret;
	}

	ret = of_property_read_u32(np, "sc,sc8549,bus-ovp-threshold",
				   &sc->cfg->bus_ovp_th);
	if (ret) {
		dev_err(sc->dev, "failed to read bus-ovp-threshold\n");
		return ret;
	}

	ret = of_property_read_u32(np, "sc,sc8549,bus-ovp-alarm-threshold",
				   &sc->cfg->bus_ovp_alm_th);
	if (ret) {
		dev_err(sc->dev, "failed to read bus-ovp-alarm-threshold\n");
		return ret;
	}

	ret = of_property_read_u32(np, "sc,sc8549,bus-ocp-threshold",
				   &sc->cfg->bus_ocp_th);
	if (ret) {
		dev_err(sc->dev, "failed to read bus-ocp-threshold\n");
		return ret;
	}

	ret = of_property_read_u32(np, "sc,sc8549,bus-ocp-alarm-threshold",
				   &sc->cfg->bus_ocp_alm_th);
	if (ret) {
		dev_err(sc->dev, "failed to read bus-ocp-alarm-threshold\n");
		return ret;
	}

	ret = of_property_read_u32(np, "sc,sc8549,bat-ucp-alarm-threshold",
				   &sc->cfg->bat_ucp_alm_th);
	if (ret) {
		dev_err(sc->dev, "failed to read bat-ucp-alarm-threshold\n");
		return ret;
	}

	ret = of_property_read_u32(np, "sc,sc8549,die-therm-threshold",
				   &sc->cfg->die_therm_th);
	if (ret) {
		dev_err(sc->dev, "failed to read die-therm-threshold\n");
		return ret;
	}

	ret = of_property_read_u32(np, "sc,sc8549,ac-ovp-threshold",
				   &sc->cfg->ac_ovp_th);
	if (ret) {
		dev_err(sc->dev, "failed to read ac-ovp-threshold\n");
		return ret;
	}

	ret = of_property_read_u32(np, "sc,sc8549,sense-resistor-mohm",
				   &sc->cfg->sense_r_mohm);
	if (ret) {
		dev_err(sc->dev, "failed to read sense-resistor-mohm\n");
		return ret;
	}

	ret = of_property_read_u32(np, "sc,sc8549,ibat-regulation-threshold",
				   &sc->cfg->ibat_reg_th);
	if (ret) {
		dev_err(sc->dev, "failed to read ibat-regulation-threshold\n");
		return ret;
	}

	ret = of_property_read_u32(np, "sc,sc8549,vbat-regulation-threshold",
				   &sc->cfg->vbat_reg_th);
	if (ret) {
		dev_err(sc->dev, "failed to read vbat-regulation-threshold\n");
		return ret;
	}

	ret = of_property_read_u32(np, "sc,sc8549,vdrop-threshold",
				   &sc->cfg->vdrop_th);
	if (ret) {
		dev_err(sc->dev, "failed to read vdrop-threshold\n");
		return ret;
	}

	ret = of_property_read_u32(np, "sc,sc8549,vdrop-deglitch",
				   &sc->cfg->vdrop_deglitch);
	if (ret) {
		dev_err(sc->dev, "failed to read vdrop-deglitch\n");
		return ret;
	}

	sc->cfg->regulation_disable = of_property_read_bool(np, "sc,sc8549,regulation_disable");
	ret = of_property_read_u32(np, "sc,sc8549,ss-timeout",
				   &sc->cfg->ss_timeout);
	if (ret) {
		dev_err(sc->dev, "failed to read ss-timeout\n");
		return ret;
	}

	ret = of_property_read_u32(np, "sc,sc8549,watchdog-timer",
				   &sc->cfg->wdt_timer);
	if (ret) {
		dev_err(sc->dev, "failed to read watchdog-timer\n");
		return ret;
	}

	ret = of_property_read_u32(np, "sc,sc8549,ibus-ucp",
				   &sc->cfg->ibus_ucp);
	if (ret) {
		dev_err(sc->dev, "failed to read ibus-ucp\n");
		return ret;
	}

	if (sc->cfg->bat_ovp_th && sc->cfg->bat_ovp_alm_th) {
		sc->cfg->bat_delta_volt = sc->cfg->bat_ovp_th - sc->cfg->bat_ovp_alm_th;
		if (sc->cfg->bat_delta_volt < 0)
			sc->cfg->bat_delta_volt = 0;
	}

	ret = of_get_named_gpio(np, "sc,sc8549,interrupt_gpios", 0);
	if (ret < 0) {
		dev_err(sc->dev, "no intr_gpio info\n");
		return ret;
	}
	sc->irq_gpio = ret;

	return 0;
}

static int sc8549_init_protection(struct sc8549 *sc)
{
	int ret;

	ret = sc8549_enable_batovp(sc, !sc->cfg->bat_ovp_disable);
	if (ret)
		dev_err(sc->dev, "%s, failed to %s bat ovp, ret = %d\n",
			 __func__, sc->cfg->bat_ovp_disable ? "disable" : "enable", ret);

	ret = sc8549_enable_batocp(sc, !sc->cfg->bat_ocp_disable);
	if (ret)
		dev_err(sc->dev, "%s, failed to %s bat ocp, ret = %d\n",
			 __func__, sc->cfg->bat_ocp_disable ? "disable" : "enable", ret);

	ret = sc8549_enable_busocp(sc, !sc->cfg->bus_ocp_disable);
	if (ret)
		dev_err(sc->dev, "%s, failed to %s bus ocp, ret = %d\n",
			 __func__, sc->cfg->bus_ocp_disable ? "disable" : "enable", ret);

	ret = sc8549_set_batovp_th(sc, sc->cfg->bat_ovp_th);
	if (ret)
		dev_err(sc->dev, "%s, failed to set bat ovp th %d, ret = %d\n",
			__func__, sc->cfg->bat_ovp_th, ret);

	sc->cfg->bat_ovp_alm_th = sc->cfg->bat_ovp_default_alm_th;

	ret = sc8549_set_batocp_th(sc, sc->cfg->bat_ocp_th);
	if (ret)
		dev_err(sc->dev, "%s, failed to set bat ocp th %d, ret = %d\n",
			__func__, sc->cfg->bat_ocp_th, ret);

	ret = sc8549_set_busovp_th(sc, sc->cfg->bus_ovp_th);
	if (ret)
		dev_err(sc->dev, "%s, failed to set bus ovp th %d, ret = %d\n",
			__func__, sc->cfg->bus_ovp_th, ret);

	ret = sc8549_set_busocp_th(sc, sc->cfg->bus_ocp_th);
	if (ret)
		dev_err(sc->dev, "%s, failed to set bus ocp th %d, ret = %d\n",
			__func__, sc->cfg->bus_ocp_th, ret);

	ret = sc8549_set_acovp_th(sc, sc->cfg->ac_ovp_th);
	if (ret)
		dev_err(sc->dev, "%s, failed to set ac ovp th %d, ret = %d\n",
			__func__, sc->cfg->ac_ovp_th, ret);

	return 0;
}

static int sc8549_init_adc(struct sc8549 *sc)
{
	sc8549_set_adc_scanrate(sc, false);
	sc8549_set_adc_scan(sc, ADC_IBUS, true);
	sc8549_set_adc_scan(sc, ADC_VBUS, true);
	sc8549_set_adc_scan(sc, ADC_VOUT, false);
	sc8549_set_adc_scan(sc, ADC_VBAT, true);
	sc8549_set_adc_scan(sc, ADC_IBAT, true);
	sc8549_set_adc_scan(sc, ADC_TDIE, true);
	sc8549_set_adc_scan(sc, ADC_VAC, true);

	sc8549_enable_adc(sc, true);

	return 0;
}

static int sc8549_init_int_src(struct sc8549 *sc)
{
	int ret;
	/*TODO:be careful ts bus and ts bat alarm bit mask is in
	 *	fault mask register, so you need call
	 *	sc8549_set_fault_int_mask for tsbus and tsbat alarm
	 */
	ret = sc8549_set_alarm_int_mask(sc,
					SC8549_VBAT_INSERT_MASK_MASK |
					SC8549_ADAPTER_INSERT_MASK_MASK);
	if (ret) {
		dev_err(sc->dev, "failed to set alarm mask:%d\n", ret);
		return ret;
	}

	ret = sc8549_enable_adc_done_alarm_int(sc, true);
	if (ret) {
		dev_err(sc->dev, "%s, failed to enable ADC_DONE_MASK, ret = %d\n", __func__, ret);
		return ret;
	}

	return ret;
}

static int sc8549_init_regulation(struct sc8549 *sc)
{
	sc8549_set_ibat_reg_th(sc, sc->cfg->ibat_reg_th);
	sc8549_set_vbat_reg_th(sc, sc->cfg->vbat_reg_th);

	sc8549_set_vdrop_deglitch(sc, sc->cfg->vdrop_deglitch);
	sc8549_set_vdrop_th(sc, sc->cfg->vdrop_th);

	sc8549_disable_regulation(sc, sc->cfg->regulation_disable);

	sc8549_set_ibus_ucp(sc, sc->cfg->ibus_ucp);

	return 0;
}

static int sc8549_init_device(struct sc8549 *sc)
{
	sc8549_reset(sc, false);
	sc8549_disable_wdt(sc);

	sc8549_set_ss_timeout(sc, sc->cfg->ss_timeout);
	sc8549_set_sense_resistor(sc, sc->cfg->sense_r_mohm);

	sc8549_init_protection(sc);
	sc8549_init_adc(sc);
	sc8549_init_int_src(sc);

	sc8549_init_regulation(sc);

	return 0;
}

static int sc8549_set_present(struct sc8549 *sc, bool present)
{
	int ret;

	sc->usb_present = present;

	if (present) {
		sc8549_init_device(sc);
		ret = sc8549_set_wdt(sc, sc->cfg->wdt_timer);
		if (ret) {
			dev_err(sc->dev, "%s, failed to set wdt time, ret=%d\n", __func__, ret);
			return ret;
		}

		schedule_delayed_work(&sc->wdt_work, 0);
	}
	return 0;
}

static ssize_t registers_show(struct device *dev,
			      struct device_attribute *attr,
			      char *buf)
{
	struct sc8549 *sc = dev_get_drvdata(dev);
	u8 addr, val, tmpbuf[300];
	u8 addr_start = SC8549_REG_00, addr_end = SC8549_REG_36;
	int len, ret;
	int idx = 0;

	idx = snprintf(buf, PAGE_SIZE, "%s:\n", "sc8549");
	for (addr = addr_start; addr <= addr_end; addr++) {
		ret = sc8549_read_byte(sc, addr, &val);
		if (ret == 0) {
			len = snprintf(tmpbuf, PAGE_SIZE - idx,
				       "Reg[%.2X] = 0x%.2x\n", addr, val);
			memcpy(&buf[idx], tmpbuf, len);
			idx += len;
		}
	}

	return idx;
}

static ssize_t registers_store(struct device *dev,
			       struct device_attribute *attr,
			       const char *buf,
			       size_t count)
{
	struct sc8549 *sc = dev_get_drvdata(dev);
	int ret;
	unsigned int reg;
	unsigned int val;
	u8 addr_end = SC8549_REG_36;

	ret = sscanf(buf, "%x %x", &reg, &val);
	if (ret == 2 && reg <= addr_end)
		sc8549_write_byte(sc, (unsigned char)reg, (unsigned char)val);

	return count;
}

static DEVICE_ATTR_RW(registers);

static int sc8549_create_device_node(struct device *dev)
{
	return device_create_file(dev, &dev_attr_registers);
}

static enum power_supply_property sc8549_charger_props[] = {
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

static void sc8549_check_alarm_status(struct sc8549 *sc);
static void sc8549_check_fault_status(struct sc8549 *sc);

static int sc8549_get_present_status(struct sc8549 *sc, int *intval)
{
	int ret = 0;
	u8 reg_val;
	bool result = false;

	if (*intval == CM_USB_PRESENT_CMD) {
		result = sc->usb_present;
	} else if (*intval == CM_BATTERY_PRESENT_CMD) {
		ret = sc8549_read_byte(sc, SC8549_REG_0E, &reg_val);
		if (!ret)
			sc->batt_present = !!(reg_val & SC8549_VBAT_INSERT_STAT_MASK);
		result = sc->batt_present;
	} else if (*intval == CM_VBUS_PRESENT_CMD) {
		ret = sc8549_read_byte(sc, SC8549_REG_0E, &reg_val);
		if (!ret)
			sc->vbus_present  = !!(reg_val & SC8549_ADAPTER_INSERT_STAT_MASK);
		result = sc->vbus_present;
	} else {
		dev_err(sc->dev, "get present cmd = %d is error\n", *intval);
	}

	*intval = result;

	return ret;
}

static int sc8549_get_temperature(struct sc8549 *sc, int *intval)
{
	int ret = 0;
	int result = 0;

	if (*intval == CM_DIE_TEMP_CMD) {
		ret = sc8549_get_adc_data(sc, ADC_TDIE, &result);
		if (!ret)
			sc->die_temp = result;
	} else {
		dev_err(sc->dev, "get temperature cmd = %d is error\n", *intval);
	}

	*intval = result;

	return ret;
}

static void sc8549_clear_alarm_status(struct sc8549 *sc)
{
	sc->bat_ucp_alarm = false;
	sc->bat_ocp_alarm = false;
	sc->bat_ovp_alarm = false;
	sc->bus_ovp_alarm = false;
	sc->bus_ocp_alarm = false;
}

static int sc8549_get_alarm_status(struct sc8549 *sc)
{
	int ret, batt_ma, batt_mv, vbus_mv, ibus_ma;

	ret = sc8549_get_ibat_now_mA(sc, &batt_ma);
	if (ret) {
		dev_err(sc->dev, "%s[%d], get batt_ma error, ret=%d\n",
			__func__, __LINE__, ret);
		return ret;
	}

	ret = sc8549_get_adc_data(sc, ADC_VBAT, &batt_mv);
	if (ret) {
		dev_err(sc->dev, "%s[%d], get batt_mv error, ret=%d\n",
			__func__, __LINE__, ret);
		return ret;
	}

	ret = sc8549_get_adc_data(sc, ADC_VBUS, &vbus_mv);
	if (ret) {
		dev_err(sc->dev, "%s[%d], get vbus_mv error, ret=%d\n",
			__func__, __LINE__, ret);
		return ret;
	}

	ret = sc8549_get_adc_data(sc, ADC_IBUS, &ibus_ma);
	if (ret) {
		dev_err(sc->dev, "%s[%d], get ibus_ma error, ret=%d\n",
			__func__, __LINE__, ret);
		return ret;
	}

	if (!sc->cfg->bat_ucp_alm_disable && sc->cfg->bat_ucp_alm_th > 0 &&
	    batt_ma < sc->cfg->bat_ucp_alm_th)
		sc->bat_ucp_alarm = true;

	if (!sc->cfg->bat_ocp_alm_disable && sc->cfg->bat_ocp_alm_th > 0 &&
	    batt_ma > sc->cfg->bat_ocp_alm_th)
		sc->bat_ocp_alarm = true;

	if (!sc->cfg->bat_ovp_alm_disable && sc->cfg->bat_ovp_alm_th > 0 &&
	    batt_mv > sc->cfg->bat_ovp_alm_th)
		sc->bat_ovp_alarm = true;

	if (!sc->cfg->bus_ovp_alm_disable && sc->cfg->bus_ovp_alm_th > 0 &&
	    vbus_mv > sc->cfg->bus_ovp_alm_th)
		sc->bus_ovp_alarm = true;

	if (!sc->cfg->bus_ocp_alm_disable && sc->cfg->bus_ocp_alm_th > 0 &&
	    ibus_ma > sc->cfg->bus_ocp_alm_th)
		sc->bus_ocp_alarm = true;

	return 0;
}

static void sc8549_charger_watchdog_work(struct work_struct *work)
{
	struct delayed_work *dwork = to_delayed_work(work);
	struct sc8549 *sc = container_of(dwork, struct sc8549, wdt_work);

	if (sc8549_set_wdt(sc, sc->cfg->wdt_timer) < 0)
		dev_err(sc->dev, "Fail to feed watchdog\n");

	schedule_delayed_work(&sc->wdt_work, HZ * 15);
}

static int sc8549_charger_get_property(struct power_supply *psy,
				       enum power_supply_property psp,
				       union power_supply_propval *val)
{
	struct sc8549 *sc = power_supply_get_drvdata(psy);
	int result = 0;
	int ret, cmd;
	u8 reg_val;

	if (!sc) {
		pr_err("%s[%d], NULL pointer!!!\n", __func__, __LINE__);
		return -EINVAL;
	}

	dev_dbg(sc->dev, ">>>>>psp = %d\n", psp);

	switch (psp) {
	case POWER_SUPPLY_PROP_CALIBRATE:
		if (!sc8549_check_charge_enabled(sc, &sc->charge_enabled))
			val->intval = sc->charge_enabled;
		break;
	case POWER_SUPPLY_PROP_PRESENT:
		cmd = val->intval;
		if (!sc8549_get_present_status(sc, &val->intval))
			dev_err(sc->dev, "fail to get present status, cmd = %d\n", cmd);
		break;
	case POWER_SUPPLY_PROP_ONLINE:
		ret = sc8549_read_byte(sc, SC8549_REG_0E, &reg_val);
		if (!ret)
			sc->vbus_present  = !!(reg_val & SC8549_ADAPTER_INSERT_STAT_MASK);
		val->intval = sc->vbus_present;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		ret = sc8549_get_adc_data(sc, ADC_VBAT, &result);
		if (!ret)
			sc->vbat_volt = result;

		val->intval = sc->vbat_volt * 1000;
		dev_dbg(sc->dev, "ADC_VBAT = %d\n", val->intval);
		break;
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		if (val->intval == CM_IBAT_CURRENT_NOW_CMD) {
			if (!sc8549_get_ibat_now_mA(sc, &result))
				sc->ibat_curr = result;

			val->intval = sc->ibat_curr * 1000;
			dev_dbg(sc->dev, "ADC_IBAT = %d\n", val->intval);
			break;
		}

		if (!sc8549_check_charge_enabled(sc, &sc->charge_enabled)) {
			if (!sc->charge_enabled) {
				val->intval = 0;
			} else {
				ret = sc8549_get_adc_data(sc, ADC_IBUS, &result);
				if (!ret)
					sc->ibus_curr = result;
				val->intval = sc->ibus_curr * 1000;
				dev_dbg(sc->dev, "ADC_IBUS = %d\n", val->intval);
			}
		}
		break;
	case POWER_SUPPLY_PROP_TEMP:
		cmd = val->intval;
		if (sc8549_get_temperature(sc, &val->intval))
			dev_err(sc->dev, "fail to get temperature, cmd = %d\n", cmd);
		break;
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE:
		ret = sc8549_get_adc_data(sc, ADC_VBUS, &result);
		if (!ret)
			sc->vbus_volt = result;

		val->intval = sc->vbus_volt * 1000;
		dev_dbg(sc->dev, "ADC_VBUS = %d\n", val->intval);
		break;
	case POWER_SUPPLY_PROP_HEALTH:
		if (val->intval == CM_SOFT_ALARM_HEALTH_CMD) {
			ret = sc8549_get_alarm_status(sc);
			if (ret) {
				dev_err(sc->dev, "fail to get alarm status, ret=%d\n", ret);
				val->intval = 0;
				break;
			}

			val->intval = ((sc->bat_ovp_alarm << CM_CHARGER_BAT_OVP_ALARM_SHIFT)
				| (sc->bat_ocp_alarm << CM_CHARGER_BAT_OCP_ALARM_SHIFT)
				| (sc->bat_ucp_alarm << CM_CHARGER_BAT_UCP_ALARM_SHIFT)
				| (sc->bus_ovp_alarm << CM_CHARGER_BUS_OVP_ALARM_SHIFT)
				| (sc->bus_ocp_alarm << CM_CHARGER_BUS_OCP_ALARM_SHIFT));

			sc8549_clear_alarm_status(sc);
			break;
		}

		if (val->intval == CM_BUS_ERR_HEALTH_CMD) {
			sc8549_check_vbus_error_status(sc);
			val->intval = (sc->bus_err_lo  << CM_CHARGER_BUS_ERR_LO_SHIFT);
			val->intval |= (sc->bus_err_hi  << CM_CHARGER_BUS_ERR_HI_SHIFT);
			break;
		}

		sc8549_check_fault_status(sc);
		val->intval = ((sc->bat_ovp_fault << CM_CHARGER_BAT_OVP_FAULT_SHIFT)
			| (sc->bat_ocp_fault << CM_CHARGER_BAT_OCP_FAULT_SHIFT)
			| (sc->bus_ovp_fault << CM_CHARGER_BUS_OVP_FAULT_SHIFT)
			| (sc->bus_ocp_fault << CM_CHARGER_BUS_OCP_FAULT_SHIFT));

		sc8549_check_alarm_status(sc);
		sc8549_dump_reg(sc);
		break;
	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
		if (!sc8549_check_charge_enabled(sc, &sc->charge_enabled)) {
			if (!sc->charge_enabled)
				val->intval = 0;
			else
				val->intval = sc->cfg->bus_ocp_alm_th * 1000;
		}
		break;
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT:
		if (!sc8549_check_charge_enabled(sc, &sc->charge_enabled)) {
			if (!sc->charge_enabled)
				val->intval = 0;
			else
				val->intval = sc->cfg->bat_ocp_alm_th * 1000;
		}
		break;
	default:
		return -EINVAL;

	}

	return 0;
}

static int sc8549_charger_set_property(struct power_supply *psy,
				       enum power_supply_property prop,
				       const union power_supply_propval *val)
{
	struct sc8549 *sc = power_supply_get_drvdata(psy);
	int ret;

	if (!sc) {
		pr_err("%s[%d], NULL pointer!!!\n", __func__, __LINE__);
		return -EINVAL;
	}

	dev_dbg(sc->dev, "<<<<<prop = %d\n", prop);

	switch (prop) {
	case POWER_SUPPLY_PROP_CALIBRATE:
		if (!val->intval) {
			sc8549_enable_adc(sc, false);
			cancel_delayed_work_sync(&sc->wdt_work);
		}

		ret = sc8549_enable_charge(sc, val->intval);
		if (ret)
			dev_err(sc->dev, "%s, failed to %s charge\n",
				__func__, val->intval ? "enable" : "disable");

		if (sc8549_check_charge_enabled(sc, &sc->charge_enabled))
			dev_err(sc->dev, "%s, failed to check charge enabled\n", __func__);

		break;
	case POWER_SUPPLY_PROP_PRESENT:
		if (val->intval == CM_USB_PRESENT_CMD)
			sc8549_set_present(sc, true);
		break;

	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE_MAX:
		ret = sc8549_set_batovp_th(sc, val->intval / 1000);
		if (ret)
			dev_err(sc->dev, "%s, failed to set bat ovp th %d mv, ret = %d\n",
				__func__, val->intval / 1000, ret);

		sc->cfg->bat_ovp_alm_th = val->intval / 1000 - sc->cfg->bat_delta_volt;
		break;

	default:
		return -EINVAL;
	}

	return 0;
}

static int sc8549_charger_is_writeable(struct power_supply *psy,
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

static int sc8549_psy_register(struct sc8549 *sc)
{
	sc->psy_cfg.drv_data = sc;
	sc->psy_cfg.of_node = sc->dev->of_node;

	if (sc->mode == SC8549_ROLE_MASTER)
		sc->psy_desc.name = "sc8549-master";
	else if (sc->mode == SC8549_ROLE_SLAVE)
		sc->psy_desc.name = "sc8549-slave";
	else
		sc->psy_desc.name = "sc8549-standalone";

	sc->psy_desc.type = POWER_SUPPLY_TYPE_UNKNOWN;
	sc->psy_desc.properties = sc8549_charger_props;
	sc->psy_desc.num_properties = ARRAY_SIZE(sc8549_charger_props);
	sc->psy_desc.get_property = sc8549_charger_get_property;
	sc->psy_desc.set_property = sc8549_charger_set_property;
	sc->psy_desc.property_is_writeable = sc8549_charger_is_writeable;

	sc->fc2_psy = devm_power_supply_register(sc->dev,
						 &sc->psy_desc, &sc->psy_cfg);
	if (IS_ERR(sc->fc2_psy)) {
		dev_err(sc->dev, "failed to register fc2_psy\n");
		return PTR_ERR(sc->fc2_psy);
	}

	dev_info(sc->dev, "%s power supply register successfully\n", sc->psy_desc.name);

	return 0;
}

static irqreturn_t sc8549_charger_interrupt(int irq, void *dev_id);

static int sc8549_init_irq(struct sc8549 *sc)
{
	int ret;
	const char *dev_name;

	gpio_free(sc->irq_gpio);

	dev_err(sc->dev, ">>>>>>>>>>>>%d\n", sc->irq_gpio);
	ret = gpio_request(sc->irq_gpio, "sc8549");
	if (ret < 0) {
		dev_err(sc->dev, "fail to request GPIO(%d), ret = %d\n", sc->irq_gpio, ret);
		return ret;
	}

	ret = gpio_direction_input(sc->irq_gpio);
	if (ret < 0) {
		dev_err(sc->dev, "fail to set GPIO(%d) as input pin, ret = %d\n",
			sc->irq_gpio, ret);
		return ret;
	}

	sc->irq = gpio_to_irq(sc->irq_gpio);
	if (sc->irq <= 0) {
		dev_err(sc->dev, "irq mapping fail\n");
		return 0;
	}

	dev_info(sc->dev, "irq : %d\n",  sc->irq);

	if (sc->mode == SC8549_ROLE_MASTER)
		dev_name = "sc8549 master irq";
	else if (sc->mode == SC8549_ROLE_SLAVE)
		dev_name = "sc8549 slave irq";
	else
		dev_name = "sc8549 standalone irq";

	ret = devm_request_threaded_irq(sc->dev, sc->irq,
					NULL, sc8549_charger_interrupt,
					IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
					dev_name, sc);
	if (ret < 0)
		dev_err(sc->dev, "request irq for irq=%d failed, ret =%d\n", sc->irq, ret);

	enable_irq_wake(sc->irq);

	device_init_wakeup(sc->dev, 1);

	return 0;
}

static void sc8549_dump_reg(struct sc8549 *sc)
{

	int ret;
	u8 val;
	u8 addr;

	for (addr = SC8549_REG_00; addr <= SC8549_REG_36; addr++) {
		ret = sc8549_read_byte(sc, addr, &val);
		if (!ret)
			dev_err(sc->dev, "Reg[%02X] = 0x%02X\n", addr, val);
	}
}

static void sc8549_check_alarm_status(struct sc8549 *sc)
{
	int ret;
	u8 flag = 0;
	u8 stat = 0;

	mutex_lock(&sc->data_lock);
	ret = sc8549_read_byte(sc, SC8549_REG_03, &flag);
	if (!ret && (flag & SC8549_VDROP_OVP_FLAG_MASK))
		dev_err(sc->dev, "Vdrop ovp event, VDROP_OVP[%02X] =0x%02X\n",
			SC8549_REG_03, flag);

	ret = sc8549_read_byte(sc, SC8549_REG_0E, &stat);
	if (!ret && stat != sc->prev_alarm) {
		dev_dbg(sc->dev, "INT_STAT[%02X] = 0X%02x\n", SC8549_REG_0E, stat);
		sc->prev_alarm = stat;
		sc->batt_present  = !!(stat & SC8549_VBAT_INSERT_STAT_MASK);
		sc->vbus_present  = !!(stat & SC8549_ADAPTER_INSERT_STAT_MASK);
	}

	ret = sc8549_read_byte(sc, SC8549_REG_09, &stat);
	if (!ret && (stat & SC8549_IBUS_UCP_RISE_FLAG_MASK))
		dev_err(sc->dev, "Ibus ucp rise event, CTRL3[%02X] = 0x%02X\n",
			SC8549_REG_09, stat);

	mutex_unlock(&sc->data_lock);
}

static void sc8549_check_fault_status(struct sc8549 *sc)
{
	int ret;
	u8 flag = 0;
	u8 stat = 0;

	mutex_lock(&sc->data_lock);
	ret = sc8549_read_byte(sc, SC8549_REG_0E, &stat);
	if (!ret && stat)
		dev_err(sc->dev, "INT_STAT[%02X] = 0x%02X\n", SC8549_REG_0E, stat);

	ret = sc8549_read_byte(sc, SC8549_REG_0F, &flag);
	if (!ret && flag)
		dev_err(sc->dev, "INT_FLAG[%02X] = 0x%02X\n", SC8549_REG_0F, flag);

	if (!ret && (flag & SC8549_IBUS_UCP_FALL_FLAG_MASK))
		dev_dbg(sc->dev, "Ibus ucp fall event, FAULT_FLAG[%02X] = 0x%02X\n",
			SC8549_REG_0F, flag);

	if (!ret && flag != sc->prev_fault) {
		sc->prev_fault = flag;
		sc->bat_ovp_fault = !!(flag & SC8549_BAT_OVP_FLT_FLAG_MASK);
		sc->bat_ocp_fault = !!(flag & SC8549_BAT_OCP_FLT_FLAG_MASK);
		sc->bus_ovp_fault = !!(flag & SC8549_BUS_OVP_FLT_FLAG_MASK);
		sc->bus_ocp_fault = !!(flag & SC8549_BUS_OCP_FLT_FLAG_MASK);
	}

	mutex_unlock(&sc->data_lock);
}

/*
 * interrupt does nothing, just info event chagne, other module could get info
 * through power supply interface
 */
static irqreturn_t sc8549_charger_interrupt(int irq, void *dev_id)
{
	struct sc8549 *sc = dev_id;

	dev_info(sc->dev, "INT OCCURRED\n");
	cm_notify_event(sc->fc2_psy, CM_EVENT_INT, NULL);

	return IRQ_HANDLED;
}

static void sc8549_determine_initial_status_work(struct work_struct *work)
{
	struct delayed_work *dwork = to_delayed_work(work);
	struct sc8549 *sc = container_of(dwork, struct sc8549, det_init_stat_work);

	sc8549_dump_reg(sc);
}

static const struct of_device_id sc8549_charger_match_table[] = {
	{
		.compatible = "sc,sc8549-standalone",
		.data = &sc8549_mode_data[SC8549_STDALONE],
	},
	{
		.compatible = "sc,sc8549-master",
		.data = &sc8549_mode_data[SC8549_MASTER],
	},

	{
		.compatible = "sc,sc8549-slave",
		.data = &sc8549_mode_data[SC8549_SLAVE],
	},
	{},
};

static int sc8549_charger_probe(struct i2c_client *client,
				const struct i2c_device_id *id)
{
	struct sc8549 *sc;
	const struct of_device_id *match;
	struct device_node *node = client->dev.of_node;
	struct power_supply *fuel_gauge;
	int ret;

	sc = devm_kzalloc(&client->dev, sizeof(struct sc8549), GFP_KERNEL);
	if (!sc)
		return -ENOMEM;

	sc->dev = &client->dev;

	sc->client = client;

	mutex_init(&sc->i2c_rw_lock);
	mutex_init(&sc->data_lock);
	mutex_init(&sc->charging_disable_lock);
	mutex_init(&sc->irq_complete);

	sc->resume_completed = true;
	sc->irq_waiting = false;
	sc->is_sc8549 = true;

	ret = sc8549_detect_device(sc);
	if (ret) {
		dev_err(sc->dev, "No sc8549 device found!\n");
		return -ENODEV;
	}

	i2c_set_clientdata(client, sc);
	ret = sc8549_create_device_node(&(client->dev));
	if (ret) {
		dev_err(sc->dev, "failed to create device node, ret = %d\n", ret);
		return ret;
	}

	match = of_match_node(sc8549_charger_match_table, node);
	if (match == NULL) {
		dev_err(sc->dev, "device tree match not found!\n");
		return -ENODEV;
	}

	sc->mode = *(int *)match->data;
	dev_info(sc->dev, "work mode:%s\n", sc->mode == SC8549_ROLE_STDALONE ? "Standalone" :
		 (sc->mode == SC8549_ROLE_SLAVE ? "Slave" : "Master"));

	ret = sc8549_parse_dt(sc, &client->dev);
	if (ret)
		return -EIO;

	of_property_read_string(node, "cm-fuel-gauge", &sc->cfg->psy_fuel_gauge);
	if (sc->cfg->psy_fuel_gauge) {
		fuel_gauge = power_supply_get_by_name(sc->cfg->psy_fuel_gauge);
		if (!fuel_gauge) {
			dev_err(sc->dev, "Cannot find power supply \"%s\"\n",
				sc->cfg->psy_fuel_gauge);
			return -EPROBE_DEFER;
		}

		power_supply_put(fuel_gauge);
		dev_info(sc->dev, "Get battery information from FGU\n");
	}

	ret = sc8549_init_device(sc);
	if (ret) {
		dev_err(sc->dev, "Failed to init device\n");
		return ret;
	}

	INIT_DELAYED_WORK(&sc->wdt_work, sc8549_charger_watchdog_work);
	INIT_DELAYED_WORK(&sc->det_init_stat_work, sc8549_determine_initial_status_work);
	ret = sc8549_psy_register(sc);
	if (ret)
		return ret;

	ret = sc8549_init_irq(sc);
	if (ret)
		return ret;

	schedule_delayed_work(&sc->det_init_stat_work, msecs_to_jiffies(100));
	dev_info(sc->dev, "sc8549 probe successfully, Part Num:%d\n!",
		 sc->part_no);

	return 0;
}

static inline bool is_device_suspended(struct sc8549 *sc)
{
	return !sc->resume_completed;
}

static int sc8549_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct sc8549 *sc = i2c_get_clientdata(client);

	mutex_lock(&sc->irq_complete);
	sc->resume_completed = false;
	mutex_unlock(&sc->irq_complete);
	dev_info(dev, "Suspend successfully!");

	return 0;
}

static int sc8549_suspend_noirq(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct sc8549 *sc = i2c_get_clientdata(client);

	if (sc->irq_waiting) {
		pr_err_ratelimited("Aborting suspend, an interrupt was detected while suspending\n");
		return -EBUSY;
	}
	return 0;
}

static int sc8549_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct sc8549 *sc = i2c_get_clientdata(client);

	mutex_lock(&sc->irq_complete);
	sc->resume_completed = true;
	if (sc->irq_waiting) {
		sc->irq_disabled = false;
		enable_irq(client->irq);
		mutex_unlock(&sc->irq_complete);
		sc8549_charger_interrupt(client->irq, sc);
	} else {
		mutex_unlock(&sc->irq_complete);
	}

	power_supply_changed(sc->fc2_psy);
	dev_err(dev, "Resume successfully!");

	return 0;
}
static int sc8549_charger_remove(struct i2c_client *client)
{
	struct sc8549 *sc = i2c_get_clientdata(client);

	sc8549_enable_adc(sc, false);
	cancel_delayed_work_sync(&sc->wdt_work);

	mutex_destroy(&sc->charging_disable_lock);
	mutex_destroy(&sc->data_lock);
	mutex_destroy(&sc->i2c_rw_lock);
	mutex_destroy(&sc->irq_complete);

	return 0;
}

static void sc8549_charger_shutdown(struct i2c_client *client)
{
	struct sc8549 *sc = i2c_get_clientdata(client);

	sc8549_enable_adc(sc, false);
	cancel_delayed_work_sync(&sc->wdt_work);
}

static const struct dev_pm_ops sc8549_pm_ops = {
	.resume		= sc8549_resume,
	.suspend_noirq	= sc8549_suspend_noirq,
	.suspend	= sc8549_suspend,
};

static const struct i2c_device_id sc8549_charger_id[] = {
	{"sc8549-standalone", SC8549_ROLE_STDALONE},
	{"sc8549-master", SC8549_ROLE_MASTER},
	{"sc8549-slave", SC8549_ROLE_SLAVE},
	{},
};

static struct i2c_driver sc8549_charger_driver = {
	.driver		= {
		.name	= "sc8549-charger",
		.owner	= THIS_MODULE,
		.of_match_table = sc8549_charger_match_table,
		.pm	= &sc8549_pm_ops,
	},
	.id_table	= sc8549_charger_id,

	.probe		= sc8549_charger_probe,
	.remove		= sc8549_charger_remove,
	.shutdown	= sc8549_charger_shutdown,
};

module_i2c_driver(sc8549_charger_driver);

MODULE_DESCRIPTION("SC SC8549 Charge Pump Driver");
MODULE_LICENSE("GPL v2");

