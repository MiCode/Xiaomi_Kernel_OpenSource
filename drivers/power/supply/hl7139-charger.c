// SPDX-License-Identifier: GPL-2.0:
// Copyright (c) 2021 unisoc.

/*
 * Driver for the HL Solutions hl7139 charger.
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

#include <linux/power/hl7139_reg.h>

enum {
	ADC_VBUS = 0,
	ADC_IBUS,
	ADC_VBAT,
	ADC_IBAT,
	ADC_VTS,
	ADC_VOUT,
	ADC_TDIE,
	ADC_MAX_NUM,
};

#define HL7139_ROLE_STDALONE		0
#define HL7139_ROLE_SLAVE		1
#define HL7139_ROLE_MASTER		2

enum {
	HL7139_STDALONE,
	HL7139_SLAVE,
	HL7139_MASTER,
};

static int hl7139_mode_data[] = {
	[HL7139_STDALONE] = HL7139_STDALONE,
	[HL7139_MASTER] = HL7139_ROLE_MASTER,
	[HL7139_SLAVE] = HL7139_ROLE_SLAVE,
};

enum dev_work_mode {
	CP_MODE = 0,
	BP_MODE,
};

enum adc_data_mode {
	FREE_RUNNING_MODE = 0,
	MANUAL_MODE,
};

enum adc_operation_mode {
	ADC_AUTO_MODE = 0,
	ADC_FORCE_ENABLE_MDOE,
	ADC_FORCE_DISABLE_MDOE,
};

#define SAMP_R_2M_OHM			2
#define SAMP_R_5M_OHM			5

#define ADC_REG_BASE			HL7139_REG_42
#define ADC_REG_POL_H_MASK		0xFF
#define ADC_REG_POL_H_SHIFT		4
#define ADC_REG_POL_L_MASK		0x0F
#define ADC_REG_POL_L_SHIFT		0

struct hl7139_cfg {
	bool bat_ovp_disable;
	bool vbat_reg_disable;
	bool bat_ovp_alm_disable;

	bool bat_ocp_disable;
	bool ibat_reg_disable;
	bool bat_ocp_alm_disable;
	bool bat_ucp_alm_disable;

	bool bus_ovp_disable;
	bool bus_ovp_alm_disable;

	bool bus_ocp_disable;
	bool ibus_reg_disable;
	bool bus_ocp_alm_disable;

	bool track_ov_disable;
	bool track_uv_disable;

	bool out_ovp_disable;

	bool die_temp_reg_disable;

	int bat_ovp_th;
	int bat_ovp_alm_th;
	int bat_delta_volt;
	int bat_ovp_default_alm_th;
	int vbat_reg_th;

	int bat_ocp_th;
	int bat_ocp_alm_th;
	int ibat_reg_th;
	int bat_ucp_alm_th;

	int bus_ovp_th;
	int bus_ovp_alm_th;

	int bus_ocp_th;
	int bus_ocp_alm_th;

	int ac_ovp_th;

	int sense_r_mohm;

	int track_ov_delta_th;
	int track_uv_delta_th;

	int die_temp_reg_th;

	int wdt_timer;

	const char *psy_fuel_gauge;
};

struct hl7139 {
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

	bool usb_present;
	bool charge_enabled;	/* Register bit status */

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
	int  prev_fault_a;
	int  prev_fault_b;

	int chg_ma;
	int chg_mv;

	int charge_state;

	struct hl7139_cfg *cfg;

	int skip_writes;
	int skip_reads;

	struct hl7139_platform_data *platform_data;

	struct delayed_work monitor_work;
	struct delayed_work wdt_work;
	struct delayed_work det_init_stat_work;

	struct dentry *debug_root;

	struct power_supply_desc psy_desc;
	struct power_supply_config psy_cfg;
	struct power_supply *fc2_psy;

	enum dev_work_mode work_mode;
	enum adc_data_mode adc_mode;
};

static void hl7139_dump_reg(struct hl7139 *hl);

static int __hl7139_read_byte(struct hl7139 *hl, u8 reg, u8 *data)
{
	s32 ret;

	ret = i2c_smbus_read_byte_data(hl->client, reg);
	if (ret < 0) {
		dev_err(hl->dev, "i2c read fail: can't read from reg 0x%02X\n", reg);
		return ret;
	}

	*data = (u8) ret;

	return 0;
}

static int __hl7139_write_byte(struct hl7139 *hl, int reg, u8 val)
{
	s32 ret;

	ret = i2c_smbus_write_byte_data(hl->client, reg, val);
	if (ret < 0) {
		dev_err(hl->dev, "i2c write fail: can't write 0x%02X to reg 0x%02X: %d\n",
			val, reg, ret);
		return ret;
	}
	return 0;
}

static int hl7139_read_byte(struct hl7139 *hl, u8 reg, u8 *data)
{
	int ret;

	if (hl->skip_reads) {
		*data = 0;
		return 0;
	}

	mutex_lock(&hl->i2c_rw_lock);
	ret = __hl7139_read_byte(hl, reg, data);
	mutex_unlock(&hl->i2c_rw_lock);

	return ret;
}

static int hl7139_write_byte(struct hl7139 *hl, u8 reg, u8 data)
{
	int ret;

	if (hl->skip_writes)
		return 0;

	mutex_lock(&hl->i2c_rw_lock);
	ret = __hl7139_write_byte(hl, reg, data);
	mutex_unlock(&hl->i2c_rw_lock);

	return ret;
}

static int hl7139_update_bits(struct hl7139 *hl, u8 reg, u8 mask, u8 data)
{
	int ret;
	u8 tmp;

	if (hl->skip_reads || hl->skip_writes)
		return 0;

	mutex_lock(&hl->i2c_rw_lock);
	ret = __hl7139_read_byte(hl, reg, &tmp);
	if (ret) {
		dev_err(hl->dev, "Failed: reg=%02X, ret=%d\n", reg, ret);
		goto out;
	}

	tmp &= ~mask;
	tmp |= data & mask;

	ret = __hl7139_write_byte(hl, reg, tmp);
	if (ret)
		dev_err(hl->dev, "Failed: reg=%02X, ret=%d\n", reg, ret);

out:
	mutex_unlock(&hl->i2c_rw_lock);
	return ret;
}

static int hl7139_enable_charge(struct hl7139 *hl, bool enable)
{
	u8 val;

	if (enable)
		val = HL7139_CHG_ENABLE;
	else
		val = HL7139_CHG_DISABLE;

	val <<= HL7139_CHG_EN_SHIFT;

	dev_info(hl->dev, "hl7139 charger %s\n", enable == false ? "disable" : "enable");

	return hl7139_update_bits(hl, HL7139_REG_12,
				  HL7139_CHG_EN_MASK, val);
}

static int hl7139_check_charge_enabled(struct hl7139 *hl, bool *enabled)
{
	int ret;
	u8 val;

	ret = hl7139_read_byte(hl, HL7139_REG_12, &val);
	if (ret < 0) {
		dev_err(hl->dev, "failed to check charge enable, ret = %d\n", ret);
		*enabled = false;
		return ret;
	}

	*enabled = !!(val & HL7139_CHG_EN_MASK);

	return ret;
}

static int hl7139_reset(struct hl7139 *hl, bool reset)
{
	u8 val;

	if (reset)
		val = HL7139_RST_ENABLE;
	else
		val = HL7139_RST_DISABLE;

	val <<= HL7139_RST_SHIFT;

	return hl7139_update_bits(hl, HL7139_REG_14, HL7139_RST_MASK, val);
}

static int hl7139_enable_wdt(struct hl7139 *hl, bool enable)
{
	u8 val;

	if (enable)
		val = HL7139_WD_ENABLE;
	else
		val = HL7139_WD_DISABLE;

	val <<= HL7139_WD_DIS_SHIFT;

	return hl7139_update_bits(hl, HL7139_REG_14,
				  HL7139_WD_DIS_MASK, val);
}


static int hl7139_set_wdt(struct hl7139 *hl, int ms)
{
	u8 val;

	if (ms < 500)
		val = HL7139_WD_TIMEOUT_0P2S;
	else if (ms < 1000)
		val = HL7139_WD_TIMEOUT_0P5S;
	else if (ms < 2000)
		val = HL7139_WD_TIMEOUT_1S;
	else if (ms < 5000)
		val = HL7139_WD_TIMEOUT_2S;
	else if (ms < 10000)
		val = HL7139_WD_TIMEOUT_5S;
	else if (ms < 20000)
		val = HL7139_WD_TIMEOUT_10S;
	else if (ms < 40000)
		val = HL7139_WD_TIMEOUT_20S;
	else
		val = HL7139_WD_TIMEOUT_40S;

	val <<= HL7139_WD_TIMEOUT_SHIFT;

	return hl7139_update_bits(hl, HL7139_REG_14,
				  HL7139_WD_TIMEOUT_MASK, val);
}

static int hl7139_set_work_mode(struct hl7139 *hl, enum dev_work_mode work_mode)
{
	u8 val = HL7139_DEV_MODE_CP_MODE;

	hl->work_mode = work_mode;

	if (work_mode == BP_MODE)
		val = HL7139_DEV_MODE_BP_MODE;

	val <<= HL7139_DEV_MODE_SHIFT;

	return hl7139_update_bits(hl, HL7139_REG_15,
				  HL7139_DEV_MODE_MASK, val);
}

static int hl7139_enable_vbat_reg(struct hl7139 *hl, bool enable)
{
	u8 val;

	if (enable)
		val = HL7139_VBAT_REG_ENABLE;
	else
		val = HL7139_VBAT_REG_DISABLE;

	val <<= HL7139_VBAT_REG_DIS_SHIFT;

	return hl7139_update_bits(hl, HL7139_REG_11,
				  HL7139_VBAT_REG_DIS_MASK, val);
}

static int hl7139_enable_batovp(struct hl7139 *hl, bool enable)
{
	u8 val;

	if (enable)
		val = HL7139_VBAT_OVP_ENABLE;
	else
		val = HL7139_VBAT_OVP_DISABLE;

	val <<= HL7139_VBAT_OVP_DIS_SHIFT;

	return hl7139_update_bits(hl, HL7139_REG_08,
				  HL7139_VBAT_OVP_DIS_MASK, val);
}

static int hl7139_set_vbat_reg_th(struct hl7139 *hl, int threshold)
{
	u8 val;

	if (threshold < HL7139_VBAT_REG_TH_BASE)
		threshold = HL7139_VBAT_REG_TH_BASE;
	else if (threshold > HL7139_VBAT_REG_TH_MAX)
		threshold = HL7139_VBAT_REG_TH_MAX;

	val = (threshold - HL7139_VBAT_REG_TH_BASE) / HL7139_VBAT_REG_TH_LSB;

	val <<= HL7139_VBAT_REG_TH_SHIFT;

	return hl7139_update_bits(hl, HL7139_REG_08,
				  HL7139_VBAT_REG_TH_MASK, val);
}

static int hl7139_set_vbat_reg_th_ovp_shift(struct hl7139 *hl, int bat_ovp_shift)
{
	u8 val;

	if (bat_ovp_shift < HL7139_BAT_OVP_TH_BASE)
		bat_ovp_shift = HL7139_BAT_OVP_TH_BASE;
	else if (bat_ovp_shift > HL7139_BAT_OVP_TH_MAX)
		bat_ovp_shift = HL7139_BAT_OVP_TH_MAX;

	val = (bat_ovp_shift - HL7139_BAT_OVP_TH_BASE) / HL7139_BAT_OVP_TH_LSB;

	val <<= HL7139_BAT_OVP_TH_SHIFT;

	return hl7139_update_bits(hl, HL7139_REG_11,
				  HL7139_BAT_OVP_TH_MASK, val);
}

static void hl7139_get_vbat_reg_th_ovp_shift(struct hl7139 *hl, int *bat_ovp_shift)
{
	u8 val;
	int ret;

	ret = hl7139_read_byte(hl, HL7139_REG_11, &val);
	if (ret < 0) {
		dev_err(hl->dev, "%s, failed to get bat ovp threshold, ret=%d\n", __func__, ret);
		*bat_ovp_shift = HL7139_BAT_OVP_TH_DEFAULT;
		return;
	}

	val = (val & HL7139_BAT_OVP_TH_MASK) >> HL7139_BAT_OVP_TH_SHIFT;

	*bat_ovp_shift = val * HL7139_BAT_OVP_TH_LSB + HL7139_BAT_OVP_TH_BASE;
}

static int hl7139_set_batovp_th(struct hl7139 *hl, int threshold)
{
	int vbat_ovp_shift;

	hl7139_get_vbat_reg_th_ovp_shift(hl, &vbat_ovp_shift);

	return hl7139_set_vbat_reg_th(hl, threshold - vbat_ovp_shift);
}

static int hl7139_enable_ibat_reg(struct hl7139 *hl, bool enable)
{
	u8 val;

	if (enable)
		val = HL7139_IBAT_REG_ENABLE;
	else
		val = HL7139_IBAT_REG_DISABLE;

	val <<= HL7139_IBAT_REG_DIS_SHIFT;

	return hl7139_update_bits(hl, HL7139_REG_11,
				  HL7139_IBAT_REG_DIS_MASK, val);
}

static int hl7139_enable_batocp(struct hl7139 *hl, bool enable)
{
	int ret;
	u8 val;

	if (enable)
		val = HL7139_IBAT_OCP_ENABLE;
	else
		val = HL7139_IBAT_OCP_DISABLE;

	val <<= HL7139_IBAT_OCP_DIS_SHIFT;

	ret = hl7139_update_bits(hl, HL7139_REG_0A,
				 HL7139_IBAT_OCP_DIS_MASK, val);
	return ret;
}

static int hl7139_set_ibat_reg_th(struct hl7139 *hl, int threshold)
{
	u8 val;

	threshold = DIV_ROUND_CLOSEST(threshold * hl->cfg->sense_r_mohm, SAMP_R_5M_OHM);

	if (threshold < HL7139_IBAT_REG_TH_BASE)
		threshold = HL7139_IBAT_REG_TH_BASE;
	else if (threshold > HL7139_IBAT_REG_TH_MAX)
		threshold = HL7139_IBAT_REG_TH_MAX;

	val = (threshold - HL7139_IBAT_REG_TH_BASE) / HL7139_IBAT_REG_TH_LSB;

	val <<= HL7139_IBAT_REG_TH_SHIFT;

	return hl7139_update_bits(hl, HL7139_REG_0A,
				  HL7139_IBAT_REG_TH_MASK, val);
}

static int hl7139_set_ibat_reg_th_ocp_shift(struct hl7139 *hl, int bat_ocp_shift)
{
	u8 val;

	if (bat_ocp_shift < HL7139_BAT_OCP_TH_BASE)
		bat_ocp_shift = HL7139_BAT_OCP_TH_BASE;
	else if (bat_ocp_shift > HL7139_BAT_OCP_TH_MAX)
		bat_ocp_shift = HL7139_BAT_OCP_TH_MAX;

	val = (bat_ocp_shift - HL7139_BAT_OCP_TH_BASE) / HL7139_BAT_OCP_TH_LSB;

	val <<= HL7139_BAT_OCP_TH_SHIFT;

	return hl7139_update_bits(hl, HL7139_REG_11,
				  HL7139_BAT_OCP_TH_MASK, val);
}

static void hl7139_get_ibat_reg_th_ocp_shift(struct hl7139 *hl, int *bat_ovp_shift)
{
	u8 val;
	int ret;

	ret = hl7139_read_byte(hl, HL7139_REG_11, &val);
	if (ret < 0) {
		dev_err(hl->dev, "%s, failed to get bat ocp shift, ret=%d\n", __func__, ret);
		*bat_ovp_shift = HL7139_BAT_OCP_TH_DEFAULT;
		return;
	}

	val = (val & HL7139_BAT_OCP_TH_MASK) >> HL7139_BAT_OCP_TH_SHIFT;

	*bat_ovp_shift = val * HL7139_BAT_OCP_TH_LSB + HL7139_BAT_OCP_TH_BASE;
}

static int hl7139_set_batocp_th(struct hl7139 *hl, int threshold)
{
	int ibat_ocp_shift;

	hl7139_get_ibat_reg_th_ocp_shift(hl, &ibat_ocp_shift);

	return hl7139_set_ibat_reg_th(hl, threshold - ibat_ocp_shift);
}

static int hl7139_enable_busovp(struct hl7139 *hl, bool enable)
{
	u8 val;

	if (enable)
		val = HL7139_VIN_OVP_ENABLE;
	else
		val = HL7139_VIN_OVP_DISABLE;

	val <<= HL7139_VIN_OVP_DIS_SHIFT;

	return hl7139_update_bits(hl, HL7139_REG_0C,
				  HL7139_VIN_OVP_DIS_MASK, val);
}

static void hl7139_get_ibus_reg_th_ocp_shift(struct hl7139 *hl,
					     int *bus_ocp_shift)
{
	u8 val;
	int ret;
	int th_base = HL7139_IIN_OCP_TH_CP_MODE_BASE;
	int th_lsb = HL7139_IIN_OCP_TH_CP_MODE_LSB;

	if (hl->work_mode == BP_MODE) {
		th_base = HL7139_IIN_OCP_TH_BP_MODE_BASE;
		th_lsb = HL7139_IIN_OCP_TH_BP_MODE_LSB;
	}

	ret = hl7139_read_byte(hl, HL7139_REG_0F, &val);
	if (ret < 0) {
		dev_err(hl->dev, "%s, failed to get bus ocp shift, ret=%d\n", __func__, ret);
		val = 0;
	}

	val = (val & HL7139_IIN_OCP_TH_MASK) >> HL7139_IIN_OCP_TH_SHIFT;

	*bus_ocp_shift = val * th_lsb + th_base;
}

static int hl7139_set_busovp_th(struct hl7139 *hl, int threshold)
{
	u8 val;
	int th_base = HL7139_VIN_OVP_CP_MODE_BASE;
	int th_max = HL7139_VIN_OVP_CP_MODE_MAX;
	int th_lsb = HL7139_VIN_OVP_CP_MODE_LSB;

	if (hl->work_mode == BP_MODE) {
		th_base = HL7139_VIN_OVP_BP_MODE_BASE;
		th_max = HL7139_VIN_OVP_BP_MODE_MAX;
		th_lsb = HL7139_VIN_OVP_BP_MODE_LSB;
	}

	if (threshold < th_base)
		threshold = th_base;
	else if (threshold > th_max)
		threshold = th_max;

	val = (threshold - th_base) / th_lsb;

	val <<= HL7139_VIN_OVP_SHIFT;

	return hl7139_update_bits(hl, HL7139_REG_0C,
				  HL7139_VIN_OVP_MASK, val);
}

static int hl7139_enable_ibus_reg(struct hl7139 *hl, bool enable)
{
	u8 val;

	if (enable)
		val = HL7139_IIN_REG_ENABLE;
	else
		val = HL7139_IIN_REG_DISABLE;

	val <<= HL7139_IIN_REG_DIS_SHIFT;

	return hl7139_update_bits(hl, HL7139_REG_10,
				  HL7139_IIN_REG_DIS_MASK, val);
}

static int hl7139_enable_busocp(struct hl7139 *hl, bool enable)
{
	u8 val;

	if (enable)
		val = HL7139_IIN_OCP_ENABLE;
	else
		val = HL7139_IIN_OCP_DISABLE;

	val <<= HL7139_IIN_OCP_DIS_SHIFT;

	return hl7139_update_bits(hl, HL7139_REG_0E,
				  HL7139_IIN_OCP_DIS_MASK, val);
}

static int hl7139_set_ibus_reg_th(struct hl7139 *hl, int threshold)
{
	u8 val;

	int th_base = HL7139_IIN_REG_TH_CP_WORK_BASE;
	int th_max = HL7139_IIN_REG_TH_CP_WORK_MAX;
	int th_lsb = HL7139_IIN_REG_TH_CP_WORK_LSB;

	if (hl->work_mode == BP_MODE) {
		th_base = HL7139_IIN_REG_TH_BP_WORK_BASE;
		th_max = HL7139_IIN_REG_TH_BP_WORK_MAX;
		th_lsb = HL7139_IIN_REG_TH_BP_WORK_LSB;
	}

	if (threshold < th_base)
		threshold = th_base;
	else if (threshold > th_max)
		threshold = th_max;

	val = (threshold - th_base) / th_lsb;

	val <<= HL7139_IIN_REG_TH_SHIFT;

	return hl7139_update_bits(hl, HL7139_REG_0E,
				  HL7139_IIN_REG_TH_MASK, val);
}

static int hl7139_set_busocp_th(struct hl7139 *hl, int threshold)
{
	int bus_ocp_shift;

	hl7139_get_ibus_reg_th_ocp_shift(hl, &bus_ocp_shift);

	return hl7139_set_ibus_reg_th(hl, threshold - bus_ocp_shift);
}

static int hl7139_set_acovp_th(struct hl7139 *hl, int threshold)
{
	u8 val;

	if (threshold < HL7139_VBUS_OVP_TH_BASE)
		threshold = HL7139_VBUS_OVP_TH_BASE;
	else if (threshold > HL7139_VBUS_OVP_TH_MAX)
		threshold = HL7139_VBUS_OVP_TH_MAX;

	val = (threshold - HL7139_VBUS_OVP_TH_BASE) / HL7139_VBUS_OVP_TH_LSB;

	val <<= HL7139_VBUS_OVP_TH_SHIFT;

	return hl7139_update_bits(hl, HL7139_REG_0B,
				  HL7139_VBUS_OVP_TH_MASK, val);
}

static int hl7139_enable_track_ov(struct hl7139 *hl, bool enable)
{
	u8 val;

	if (enable)
		val = HL7139_TRACK_OV_ENABLE;
	else
		val = HL7139_TRACK_OV_DISABLE;

	val <<= HL7139_TRACK_OV_DIS_SHIFT;

	return hl7139_update_bits(hl, HL7139_REG_16,
				  HL7139_TRACK_OV_DIS_MASK, val);
}

static int hl7139_set_track_ov_delta_th(struct hl7139 *hl, int threshold)
{
	u8 val;

	if (threshold < HL7139_TRACK_OV_DELTA_TH_BASE)
		threshold = HL7139_TRACK_OV_DELTA_TH_BASE;
	else if (threshold > HL7139_TRACK_OV_DELTA_TH_MAX)
		threshold = HL7139_TRACK_OV_DELTA_TH_MAX;

	val = (threshold - HL7139_TRACK_OV_DELTA_TH_BASE) / HL7139_TRACK_OV_DELTA_TH_LSB;

	val <<= HL7139_TRACK_OV_SHIFT;

	return hl7139_update_bits(hl, HL7139_REG_16,
				  HL7139_TRACK_OV_MASK, val);
}

static int hl7139_enable_track_uv(struct hl7139 *hl, bool enable)
{
	u8 val;

	if (enable)
		val = HL7139_TRACK_UV_ENABLE;
	else
		val = HL7139_TRACK_UV_DISABLE;

	val <<= HL7139_TRACK_UV_DIS_SHIFT;

	return hl7139_update_bits(hl, HL7139_REG_16,
				  HL7139_TRACK_UV_DIS_MASK, val);
}

static int hl7139_set_track_uv_delta_th(struct hl7139 *hl, int threshold)
{
	u8 val;

	if (threshold < HL7139_TRACK_UV_DELTA_TH_BASE)
		threshold = HL7139_TRACK_UV_DELTA_TH_BASE;
	else if (threshold > HL7139_TRACK_UV_DELTA_TH_MAX)
		threshold = HL7139_TRACK_UV_DELTA_TH_MAX;

	val = (threshold - HL7139_TRACK_UV_DELTA_TH_BASE) / HL7139_TRACK_UV_DELTA_TH_LSB;

	val <<= HL7139_TRACK_UV_SHIFT;

	return hl7139_update_bits(hl, HL7139_REG_16,
				  HL7139_TRACK_UV_MASK, val);
}

static int hl7139_enable_out_ovp(struct hl7139 *hl, bool enable)
{
	u8 val;

	if (enable)
		val = HL7139_VOUT_OVP_ENABLE;
	else
		val = HL7139_VOUT_OVP_DISABLE;

	val <<= HL7139_VOUT_OVP_DIS_SHIFT;

	return hl7139_update_bits(hl, HL7139_REG_13,
				  HL7139_VOUT_OVP_DIS_MASK, val);
}

static int hl7139_enable_die_temp_reg(struct hl7139 *hl, bool enable)
{
	u8 val;

	if (enable)
		val = HL7139_TDIE_REG_ENABLE;
	else
		val = HL7139_TDIE_REG_DISABLE;

	val <<= HL7139_TDIE_REG_DIS_SHIFT;

	return hl7139_update_bits(hl, HL7139_REG_10,
				  HL7139_TDIE_REG_DIS_MASK, val);
}

static int hl7139_set_die_temp_reg_th(struct hl7139 *hl, int threshold)
{
	u8 val;

	if (threshold < HL7139_TDIE_REG_TH_BASE)
		threshold = HL7139_TDIE_REG_TH_BASE;
	else if (threshold > HL7139_TDIE_REG_TH_MAX)
		threshold = HL7139_TDIE_REG_TH_MAX;

	val = (threshold - HL7139_TDIE_REG_TH_BASE) / HL7139_TDIE_REG_TH_LSB;

	val <<= HL7139_TDIE_REG_TH_SHIFT;

	return hl7139_update_bits(hl, HL7139_REG_10,
				  HL7139_TDIE_REG_TH_MASK, val);
}

static int hl7139_set_adc_operation_mode(struct hl7139 *hl, enum adc_operation_mode mode)
{
	u8 val = HL7139_ADC_MODE_CFG_AUTO_MODE;

	if (mode == ADC_FORCE_ENABLE_MDOE)
		val = HL7139_ADC_MODE_CFG_FORCE_ENABLE_MODE;
	else if (mode == ADC_FORCE_DISABLE_MDOE)
		val = HL7139_ADC_MODE_CFG_FORCE_DISABLE_MODE;

	val <<= HL7139_ADC_MODE_CFG_SHIFT;

	return hl7139_update_bits(hl, HL7139_REG_40,
				  HL7139_ADC_MODE_CFG_MASK, val);
}

static int hl7139_enable_adc(struct hl7139 *hl, bool enable)
{
	u8 val;

	if (enable)
		val = HL7139_ADC_READ_ENABLE;
	else
		val = HL7139_ADC_READ_DISABLE;

	val <<= HL7139_ADC_READ_EN_SHIFT;

	return hl7139_update_bits(hl, HL7139_REG_40,
				  HL7139_ADC_READ_EN_MASK, val);
}

static int hl7139_set_adc_avg_time(struct hl7139 *hl, int adc_avg_time)
{
	u8 val;

	if (adc_avg_time == 1)
		val = HL7139_ADC_AVG_TIME_1;
	else if (adc_avg_time == 2)
		val = HL7139_ADC_AVG_TIME_2;
	else
		val = HL7139_ADC_AVG_TIME_4;

	val <<= HL7139_ADC_AVG_TIME_SHIFT;

	return hl7139_update_bits(hl, HL7139_REG_40,
				  HL7139_ADC_AVG_TIME_MASK, val);
}

static int hl7139_set_adc_data_mode(struct hl7139 *hl, enum adc_data_mode mode)
{
	u8 val = HL7139_ADC_REG_COPY_FREE_RUNNING_MODE;

	hl->adc_mode = mode;
	/*
	 * 1. Free running (auto) mode is equivalent to continuous mode.
	 * 2. Manual mode is equivalent to one-shot mode.
	 *
	 * According to the measured results, the manual mode has higher
	 * accuracy, while the free mode has larger deviation.
	 *
	 * Therefore, manual mode is recommended.
	 */
	if (mode == MANUAL_MODE)
		val = HL7139_ADC_REG_COPY_MANUAL_MODE;

	val <<= HL7139_ADC_REG_COPY_SHIFT;

	return hl7139_update_bits(hl, HL7139_REG_40,
				  HL7139_ADC_REG_COPY_MASK, val);
}

static int hl7139_set_adc_scan(struct hl7139 *hl, int channel, bool enable)
{
	u8 mask, shift, val;

	if (channel > ADC_MAX_NUM)
		return -EINVAL;

	if (channel == ADC_TDIE) {
		shift = HL7139_TDIE_ADC_DIS_SHIFT;
		mask = HL7139_TDIE_ADC_DIS_MASK;
	} else if (channel == ADC_VOUT) {
		shift = HL7139_VOUT_ADC_DIS_SHIFT;
		mask = HL7139_VOUT_ADC_DIS_MASK;
	} else {
		shift = 7 - channel;
		mask = 1 << shift;
	}

	if (enable)
		val = 0 << shift;
	else
		val = 1 << shift;

	return hl7139_update_bits(hl, HL7139_REG_41, mask, val);
}

static int hl7139_get_adc_data(struct hl7139 *hl, int channel,  int *result)
{
	int ret = 0;
	u8 val_l, val_h;
	u8 val_h_mask = ADC_REG_POL_H_MASK, val_h_shift = ADC_REG_POL_H_SHIFT;
	u8 val_l_mask = ADC_REG_POL_L_MASK, val_l_shift = ADC_REG_POL_L_SHIFT;
	u16 val;

	if (channel >= ADC_MAX_NUM)
		return 0;

	ret = hl7139_enable_adc(hl, true);
	if (ret) {
		dev_err(hl->dev, "%s, failed to enable adc, ret = %d\n", __func__, ret);
		return ret;
	}

	ret = hl7139_set_adc_scan(hl, channel, true);
	if (ret) {
		dev_err(hl->dev, "%s, failed to enable adc_channel: %d, ret = %d\n",
			__func__, channel, ret);
		return ret;
	}

	if (hl->adc_mode == MANUAL_MODE) {
		/*
		 * The adc operation process of hl7139 manufacturer requires a
		 * delay of 10ms to obtain adc data to ensure the accuracy of
		 * adc data.
		 */
		usleep_range(9000, 11000);
		val = HL7139_ADC_MAN_COPY_ENABLE << HL7139_ADC_MAN_COPY_SHIFT;
		ret = hl7139_update_bits(hl, HL7139_REG_40,
					 HL7139_ADC_MAN_COPY_MASK, val);
		if (ret < 0) {
			dev_err(hl->dev, "%s, failed to migrate adc buffer data manually, ret=%d\n",
				__func__, ret);
			return ret;
		}
	}

	ret = hl7139_read_byte(hl, ADC_REG_BASE + (channel << 1), &val_h);
	if (ret < 0) {
		dev_err(hl->dev, "%s, failed to get adc high bit data\n", __func__);
		return ret;
	}

	ret = hl7139_read_byte(hl, ADC_REG_BASE + (channel << 1) + 1, &val_l);
	if (ret < 0) {
		dev_err(hl->dev, "%s, failed to get adc low bit data\n", __func__);
		return ret;
	}

	val = ((val_h & val_h_mask) << val_h_shift) | ((val_l & val_l_mask) << val_l_shift);

	if (channel == ADC_VBUS) {
		val = val * 4;
	} else if (channel == ADC_IBUS) {
		if (hl->work_mode == CP_MODE)
			val = DIV_ROUND_CLOSEST(val * 11, 10);
		else
			val = DIV_ROUND_CLOSEST(val * 43, 20);
	} else if (channel == ADC_VBAT) {
		val = DIV_ROUND_CLOSEST(val * 5, 4);
	} else if (channel == ADC_IBAT) {
		val = DIV_ROUND_CLOSEST(val * SAMP_R_5M_OHM * 11, hl->cfg->sense_r_mohm * 5);
	} else if (channel == ADC_VTS) {
		val = DIV_ROUND_CLOSEST(val * 11, 25);
	} else if (channel == ADC_VOUT) {
		val = DIV_ROUND_CLOSEST(val * 5, 4);
	} else if (channel == ADC_TDIE) {
		val = DIV_ROUND_CLOSEST(val * 1, 16);
	}

	*result = val;

	ret = hl7139_set_adc_scan(hl, channel, false);
	if (ret)
		dev_err(hl->dev, "%s, failed to disable adc_channel: %d, ret = %d\n",
			__func__, channel, ret);

	return 0;
}

static int hl7139_get_ibat_now_mA(struct hl7139 *hl, int *batt_ma)
{
	struct power_supply *fuel_gauge;
	union power_supply_propval fgu_val;
	int ret = 0;

	if (hl->cfg->psy_fuel_gauge) {
		fuel_gauge = power_supply_get_by_name(hl->cfg->psy_fuel_gauge);
		if (!fuel_gauge)
			return -ENODEV;

		fgu_val.intval = 0;
		ret = power_supply_get_property(fuel_gauge, POWER_SUPPLY_PROP_CURRENT_NOW,
						&fgu_val);
		power_supply_put(fuel_gauge);
		if (ret) {
			dev_err(hl->dev, "%s, get batt_ma error, ret=%d\n", __func__, ret);
			return ret;
		}

		*batt_ma = DIV_ROUND_CLOSEST(fgu_val.intval, 1000);
	} else {
		ret = hl7139_get_adc_data(hl, ADC_IBAT, batt_ma);
		if (ret)
			dev_err(hl->dev, "%s, get batt_ma error, ret=%d\n", __func__, ret);
	}

	return ret;
}

static int hl7139_set_int_unmask(struct hl7139 *hl, u8 unmask)
{
	int ret;
	u8 val;

	ret = hl7139_read_byte(hl, HL7139_REG_02, &val);
	if (ret) {
		dev_err(hl->dev, "%s, failed to get int mask, ret = %d\n", __func__, ret);
		return ret;
	}

	val &= unmask;

	return hl7139_write_byte(hl, HL7139_REG_02, val);
}

static int hl7139_check_vbus_error_status(struct hl7139 *hl)
{
	int ret;
	u8 data;

	hl->bus_err_lo = false;
	hl->bus_err_hi = false;

	ret = hl7139_read_byte(hl, HL7139_REG_07, &data);
	if (ret == 0) {
		dev_err(hl->dev, "vbus error, Reg[%02X] = 0x%02X\n", HL7139_REG_07, data);
		hl->bus_err_lo = !!(data & HL7139_TRACK_UV_S_MASK);
		hl->bus_err_hi = !!(data & HL7139_TRACK_OV_S_MASK);
	}

	return ret;
}

static int hl7139_detect_device(struct hl7139 *hl)
{
	int ret;
	u8 data;

	ret = hl7139_read_byte(hl, HL7139_REG_00, &data);
	if (ret < 0) {
		dev_err(hl->dev, "Failed to get device id, ret = %d\n", ret);
		return ret;
	}

	hl->part_no = (data & HL7139_DEV_ID_MASK);
	hl->part_no >>= HL7139_DEV_ID_SHIFT;

	if (hl->part_no != HL7139_DEV_ID) {
		dev_err(hl->dev, "The device id is 0x%x\n", hl->part_no);
		ret = -EINVAL;
	}

	return ret;
}

static int hl7139_parse_dt(struct hl7139 *hl, struct device *dev)
{
	int ret;
	struct device_node *np = dev->of_node;

	hl->cfg = devm_kzalloc(dev, sizeof(struct hl7139_cfg), GFP_KERNEL);
	if (!hl->cfg)
		return -ENOMEM;

	hl->cfg->bat_ovp_disable = of_property_read_bool(np,
			"hl,hl7139,bat-ovp-disable");
	hl->cfg->vbat_reg_disable = of_property_read_bool(np,
			"hl,hl7139,vbat-reg-disable");
	hl->cfg->bat_ovp_alm_disable = of_property_read_bool(np,
			"hl,hl7139,bat-ovp-alarm-disable");
	hl->cfg->bat_ocp_disable = of_property_read_bool(np,
			"hl,hl7139,bat-ocp-disable");
	hl->cfg->ibat_reg_disable = of_property_read_bool(np,
			"hl,hl7139,ibat-reg-disable");
	hl->cfg->bat_ucp_alm_disable = of_property_read_bool(np,
			"hl,hl7139,bat-ucp-alarm-disable");
	hl->cfg->bat_ocp_alm_disable = of_property_read_bool(np,
			"hl,hl7139,bat-ocp-alarm-disable");
	hl->cfg->bus_ovp_disable = of_property_read_bool(np,
			"hl,hl7139,bus-ovp-disable");
	hl->cfg->bus_ovp_alm_disable = of_property_read_bool(np,
			"hl,hl7139,bus-ovp-alarm-disable");
	hl->cfg->bus_ocp_disable = of_property_read_bool(np,
			"hl,hl7139,bus-ocp-disable");
	hl->cfg->ibus_reg_disable = of_property_read_bool(np,
			"hl,hl7139,ibus-reg-disable");
	hl->cfg->bus_ocp_alm_disable = of_property_read_bool(np,
			"hl,hl7139,bus-ocp-alarm-disable");
	hl->cfg->track_ov_disable = of_property_read_bool(np,
			"hl,hl7139,track-ov-disable");
	hl->cfg->track_uv_disable = of_property_read_bool(np,
			"hl,hl7139,track-uv-disable");
	hl->cfg->out_ovp_disable = of_property_read_bool(np,
			"hl,hl7139,out-ovp-disable");
	hl->cfg->die_temp_reg_disable = of_property_read_bool(np,
			"hl,hl7139, die-temp-reg-disable");

	ret = of_property_read_u32(np, "hl,hl7139,bat-ovp-threshold",
				   &hl->cfg->bat_ovp_th);
	if (ret) {
		dev_err(hl->dev, "failed to read bat-ovp-threshold\n");
		return ret;
	}

	ret = of_property_read_u32(np, "hl,hl7139,bat-ovp-alarm-threshold",
				   &hl->cfg->bat_ovp_alm_th);
	if (ret) {
		dev_err(hl->dev, "failed to read bat-ovp-alarm-threshold\n");
		return ret;
	}

	hl->cfg->bat_ovp_default_alm_th = hl->cfg->bat_ovp_alm_th;

	ret = of_property_read_u32(np, "hl,hl7139,bat-ocp-threshold",
				   &hl->cfg->bat_ocp_th);
	if (ret) {
		dev_err(hl->dev, "failed to read bat-ocp-threshold\n");
		return ret;
	}

	ret = of_property_read_u32(np, "hl,hl7139,bat-ocp-alarm-threshold",
				   &hl->cfg->bat_ocp_alm_th);
	if (ret) {
		dev_err(hl->dev, "failed to read bat-ocp-alarm-threshold\n");
		return ret;
	}

	ret = of_property_read_u32(np, "hl,hl7139,bus-ovp-threshold",
				   &hl->cfg->bus_ovp_th);
	if (ret) {
		dev_err(hl->dev, "failed to read bus-ovp-threshold\n");
		return ret;
	}

	ret = of_property_read_u32(np, "hl,hl7139,bus-ovp-alarm-threshold",
				   &hl->cfg->bus_ovp_alm_th);
	if (ret) {
		dev_err(hl->dev, "failed to read bus-ovp-alarm-threshold\n");
		return ret;
	}

	ret = of_property_read_u32(np, "hl,hl7139,bus-ocp-threshold",
				   &hl->cfg->bus_ocp_th);
	if (ret) {
		dev_err(hl->dev, "failed to read bus-ocp-threshold\n");
		return ret;
	}

	ret = of_property_read_u32(np, "hl,hl7139,bus-ocp-alarm-threshold",
				   &hl->cfg->bus_ocp_alm_th);
	if (ret) {
		dev_err(hl->dev, "failed to read bus-ocp-alarm-threshold\n");
		return ret;
	}

	ret = of_property_read_u32(np, "hl,hl7139,bat-ucp-alarm-threshold",
				   &hl->cfg->bat_ucp_alm_th);
	if (ret) {
		dev_err(hl->dev, "failed to read bat-ucp-alarm-threshold\n");
		return ret;
	}

	ret = of_property_read_u32(np, "hl,hl7139,die-tmp-reg-threshold",
				   &hl->cfg->die_temp_reg_th);
	if (ret) {
		dev_err(hl->dev, "failed to read die-therm-threshold\n");
		return ret;
	}

	ret = of_property_read_u32(np, "hl,hl7139,ac-ovp-threshold",
				   &hl->cfg->ac_ovp_th);
	if (ret) {
		dev_err(hl->dev, "failed to read ac-ovp-threshold\n");
		return ret;
	}

	ret = of_property_read_u32(np, "hl,hl7139,sense-resistor-mohm",
				   &hl->cfg->sense_r_mohm);
	if (ret) {
		dev_err(hl->dev, "failed to read sense-resistor-mohm\n");
		/* If this parameter is not defined in DTS, the default resistor is 5mΩ */
		hl->cfg->sense_r_mohm = SAMP_R_5M_OHM;
	}

	ret = of_property_read_u32(np, "hl,hl7139,track-ov-delta-threshold",
				   &hl->cfg->track_ov_delta_th);
	if (ret) {
		dev_err(hl->dev, "failed to read track-ov-delta-threshold\n");
		return ret;
	}

	ret = of_property_read_u32(np, "hl,hl7139,track-uv-delta-threshold",
				   &hl->cfg->track_uv_delta_th);
	if (ret) {
		dev_err(hl->dev, "failed to read track-uv-delta-threshold\n");
		return ret;
	}

	ret = of_property_read_u32(np, "hl,hl7139,ibat-regulation-threshold",
				   &hl->cfg->ibat_reg_th);
	if (ret) {
		dev_err(hl->dev, "failed to read ibat-regulation-threshold\n");
		return ret;
	}

	ret = of_property_read_u32(np, "hl,hl7139,vbat-regulation-threshold",
				   &hl->cfg->vbat_reg_th);
	if (ret) {
		dev_err(hl->dev, "failed to read vbat-regulation-threshold\n");
		return ret;
	}

	ret = of_property_read_u32(np, "hl,hl7139,watchdog-timer",
				   &hl->cfg->wdt_timer);
	if (ret) {
		dev_err(hl->dev, "failed to read watchdog-timer\n");
		return ret;
	}

	if (hl->cfg->bat_ovp_th && hl->cfg->bat_ovp_alm_th) {
		hl->cfg->bat_delta_volt = hl->cfg->bat_ovp_th - hl->cfg->bat_ovp_alm_th;
		if (hl->cfg->bat_delta_volt < 0)
			hl->cfg->bat_delta_volt = 0;
	}

	ret = of_get_named_gpio(np, "hl,hl7139,interrupt_gpios", 0);
	if (ret < 0) {
		dev_err(hl->dev, "no intr_gpio info\n");
		return ret;
	}
	hl->irq_gpio = ret;

	return 0;
}

static int hl7139_init_protection(struct hl7139 *hl)
{
	int ret = 0;

	ret = hl7139_enable_vbat_reg(hl, !hl->cfg->vbat_reg_disable);
	if (ret)
		dev_err(hl->dev, "%s, failed to %s vbat reg, ret = %d\n",
			__func__, hl->cfg->vbat_reg_disable ? "disable" : "enable", ret);

	ret = hl7139_enable_batovp(hl, !hl->cfg->bat_ovp_disable);
	if (ret)
		dev_err(hl->dev, "%s, failed to %s bat ovp, ret = %d\n",
			__func__, hl->cfg->bat_ovp_disable ? "disable" : "enable", ret);

	ret = hl7139_enable_ibat_reg(hl, !hl->cfg->ibat_reg_disable);
	if (ret)
		dev_err(hl->dev, "%s, failed to %s ibat reg, ret = %d\n",
			__func__, hl->cfg->ibat_reg_disable ? "disable" : "enable", ret);

	ret = hl7139_enable_batocp(hl, !hl->cfg->bat_ocp_disable);
	if (ret)
		dev_err(hl->dev, "%s, failed to %s bat ocp, ret = %d\n",
			__func__, hl->cfg->bat_ocp_disable ? "disable" : "enable", ret);

	ret = hl7139_enable_busovp(hl, !hl->cfg->bus_ovp_disable);
	if (ret)
		dev_err(hl->dev, "%s, failed to %s bus ovp, ret = %d\n",
			__func__, hl->cfg->bus_ovp_disable ? "disable" : "enable", ret);

	ret = hl7139_enable_ibus_reg(hl, !hl->cfg->ibus_reg_disable);
	if (ret)
		dev_err(hl->dev, "%s, failed to %s ibus reg, ret = %d\n",
			__func__, hl->cfg->ibus_reg_disable ? "disable" : "enable", ret);

	ret = hl7139_enable_busocp(hl, !hl->cfg->bus_ocp_disable);
	if (ret)
		dev_err(hl->dev, "%s, failed to %s bus ocp, ret = %d\n",
			__func__, hl->cfg->bus_ocp_disable ? "disable" : "enable", ret);

	ret = hl7139_enable_track_ov(hl, !hl->cfg->track_ov_disable);
	if (ret)
		dev_err(hl->dev, "%s, failed to %s track ov, ret = %d\n",
			__func__, hl->cfg->track_ov_disable ? "disable" : "enable", ret);

	ret = hl7139_enable_track_uv(hl, !hl->cfg->track_uv_disable);
	if (ret)
		dev_err(hl->dev, "%s, failed to %s track uv, ret = %d\n",
			__func__, hl->cfg->track_uv_disable ? "disable" : "enable", ret);

	ret = hl7139_enable_out_ovp(hl, !hl->cfg->out_ovp_disable);
	if (ret)
		dev_err(hl->dev, "%s, failed to %s out ovp, ret = %d\n",
			__func__, hl->cfg->out_ovp_disable ? "disable" : "enable", ret);

	ret = hl7139_enable_die_temp_reg(hl, !hl->cfg->die_temp_reg_disable);
	if (ret)
		dev_err(hl->dev, "%s, failed to %s die temperature regulation, ret = %d\n",
			__func__, hl->cfg->die_temp_reg_disable ? "disable" : "enable", ret);

	ret = hl7139_set_batovp_th(hl, hl->cfg->bat_ovp_th);
	if (ret)
		dev_err(hl->dev, "%s, failed to set bat ovp th %d, ret = %d\n",
			__func__, hl->cfg->bat_ovp_th, ret);

	hl->cfg->bat_ovp_alm_th = hl->cfg->bat_ovp_default_alm_th;

	ret = hl7139_set_batocp_th(hl, hl->cfg->bat_ocp_th);
	if (ret)
		dev_err(hl->dev, "%s, failed to set bat ocp th %d, ret = %d\n",
			__func__, hl->cfg->bat_ocp_th, ret);

	ret = hl7139_set_acovp_th(hl, hl->cfg->ac_ovp_th);
	if (ret)
		dev_err(hl->dev, "%s, failed to set ac ovp th %d, ret = %d\n",
			__func__, hl->cfg->ac_ovp_th, ret);

	ret = hl7139_set_busovp_th(hl, hl->cfg->bus_ovp_th);
	if (ret)
		dev_err(hl->dev, "%s, failed to set %s bus ovp th %d, ret = %d\n",
			__func__, hl->work_mode == CP_MODE ? "CP mode" : "BP mode",
			hl->cfg->bus_ovp_th, ret);

	ret = hl7139_set_busocp_th(hl, hl->cfg->bus_ocp_th);
	if (ret)
		dev_err(hl->dev, "%s, failed to set %s bus ocp th %d, ret = %d\n",
			__func__, hl->work_mode == CP_MODE ? "CP mode" : "BP mode",
			hl->cfg->bus_ocp_th, ret);

	ret = hl7139_set_track_ov_delta_th(hl, hl->cfg->track_ov_delta_th);
	if (ret)
		dev_err(hl->dev, "%s, failed to set track ov delta th %d, ret = %d\n",
			__func__, hl->cfg->track_ov_delta_th, ret);

	ret = hl7139_set_track_uv_delta_th(hl, hl->cfg->track_uv_delta_th);
	if (ret)
		dev_err(hl->dev, "%s, failed to set track uv delta th %d, ret = %d\n",
			__func__, hl->cfg->track_uv_delta_th, ret);

	ret = hl7139_set_die_temp_reg_th(hl, hl->cfg->die_temp_reg_th);
	if (ret)
		dev_err(hl->dev, "%s, failed to set die temperature th %d, ret = %d\n",
			__func__, hl->cfg->die_temp_reg_th, ret);

	return ret;
}

static int hl7139_init_adc(struct hl7139 *hl)
{
	int i, ret = 0;

	ret = hl7139_set_adc_avg_time(hl, 4);
	if (ret) {
		dev_err(hl->dev, "%s, failed to set adc averge time, ret = %d\n", __func__, ret);
		return ret;
	}

	ret = hl7139_set_adc_data_mode(hl, MANUAL_MODE);
	if (ret) {
		dev_err(hl->dev, "%s, failed to set adc data mode, ret = %d\n", __func__, ret);
		return ret;
	}

	for (i = ADC_VBUS; i < ADC_MAX_NUM; i++) {
		ret = hl7139_set_adc_scan(hl, i, false);
		if (ret) {
			dev_err(hl->dev, "%s, failed to disable adc_channel: %d, ret = %d\n",
				__func__, i, ret);
			return ret;
		}
	}

	ret = hl7139_set_adc_operation_mode(hl, ADC_AUTO_MODE);
	if (ret)
		dev_err(hl->dev, "%s, failed to set adc operation mode, ret = %d\n", __func__, ret);

	return ret;
}

static int hl7139_init_int_src(struct hl7139 *hl)
{
	int ret;

	ret = hl7139_set_int_unmask(hl,
				    (u8)~(HL7139_V_OK_M_MASK |
					  HL7139_CUR_M_MASK |
					  HL7139_SHORT_M_MASK));
	if (ret) {
		dev_err(hl->dev, "failed to set int unmask:%d\n", ret);
		return ret;
	}

	return ret;
}

static int hl7139_init_regulation(struct hl7139 *hl)
{
	hl7139_set_ibat_reg_th(hl, hl->cfg->ibat_reg_th);
	hl7139_set_ibat_reg_th_ocp_shift(hl, hl->cfg->bat_ocp_th - hl->cfg->ibat_reg_th);

	hl7139_set_vbat_reg_th(hl, hl->cfg->vbat_reg_th);
	hl7139_set_vbat_reg_th_ovp_shift(hl, hl->cfg->bat_ovp_th - hl->cfg->vbat_reg_th);

	return 0;
}

static int hl7139_init_dp_dm_cfg(struct hl7139 *hl)
{
	u8 val = HL7139_DPDM_CFG_SYNC_AND_TS_MODE;

	if (hl->mode == HL7139_ROLE_STDALONE)
		val = HL7139_DPDM_CFG_DP_DM_MODE;

	val <<= HL7139_DPDM_CFG_SHIFT;

	return hl7139_update_bits(hl, HL7139_REG_15,
				  HL7139_DPDM_CFG_MASK, val);
}

static bool hl7139_check_device_mode_match(struct hl7139 *hl)
{
	u8 val;
	int ret, dev_mode_status;
	bool check_dev_mode_match = false;

	ret = hl7139_read_byte(hl, HL7139_REG_06, &val);
	if (ret < 0) {
		dev_err(hl->dev, "%s, failed to get device mode status, ret=%d\n", __func__, ret);
		goto done;
	}

	val = (val & HL7139_DEV_MODE_STS_MASK) >> HL7139_DEV_MODE_STS_SHIFT;

	if (val == HL7139_DEV_MODE_STS_SLAVE_MODE)
		dev_mode_status = HL7139_ROLE_SLAVE;
	else if (val == HL7139_DEV_MODE_STS_MASTER_MODE)
		dev_mode_status = HL7139_ROLE_MASTER;
	else
		dev_mode_status = HL7139_ROLE_STDALONE;

	if (dev_mode_status == hl->mode)
		check_dev_mode_match = true;

done:
	return check_dev_mode_match;
}

static int hl7139_init_gpp_cfg(struct hl7139 *hl)
{
	u8 val = HL7139_GPP_CFG_GPP_SNSN_FUNC;

	if (hl->mode == HL7139_ROLE_SLAVE)
		val = HL7139_GPP_CFG_SYNC_FUNC;

	val <<= HL7139_GPP_CFG_SHIFT;

	return hl7139_update_bits(hl, HL7139_REG_15,
				  HL7139_GPP_CFG_MASK, val);
}

static int hl7139_init_device_mode(struct hl7139 *hl)
{
	int ret;

	ret = hl7139_init_dp_dm_cfg(hl);
	if (ret < 0) {
		dev_err(hl->dev, "%s, failed to configure DP/DM in %s mode, ret = %d\n",
			__func__,  hl->mode == HL7139_ROLE_STDALONE ? "Standalone" :
			(hl->mode == HL7139_ROLE_SLAVE ? "Slave" : "Master"), ret);
		return ret;
	}

	ret = hl7139_init_gpp_cfg(hl);
	if (ret < 0)
		dev_err(hl->dev, "%s, failed to configure gpp in %s mode, ret = %d\n",
			__func__,  hl->mode == HL7139_ROLE_STDALONE ? "Standalone" :
			(hl->mode == HL7139_ROLE_SLAVE ? "Slave" : "Master"), ret);

	return ret;
}

static int hl7139_init_device(struct hl7139 *hl)
{
	int ret = 0;

	ret = hl7139_reset(hl, false);
	if (ret < 0) {
		dev_err(hl->dev, "%s, failed to reset device, ret = %d\n", __func__, ret);
		return ret;
	}

	ret = hl7139_init_device_mode(hl);
	if (ret < 0) {
		dev_err(hl->dev, "%s, failed to init device mode, ret = %d\n", __func__, ret);
		return ret;
	}

	ret = hl7139_enable_wdt(hl, false);
	if (ret < 0) {
		dev_err(hl->dev, "%s, failed to disable watchdog, ret = %d\n", __func__, ret);
		return ret;
	}

	ret = hl7139_set_work_mode(hl, CP_MODE);
	if (ret < 0) {
		dev_err(hl->dev, "%s, failed to set work mode, ret = %d\n", __func__, ret);
		return ret;
	}

	hl7139_init_regulation(hl);

	ret = hl7139_init_protection(hl);
	if (ret < 0) {
		dev_err(hl->dev, "%s, failed to init protection, ret = %d\n", __func__, ret);
		return ret;
	}

	ret = hl7139_init_adc(hl);
	if (ret < 0) {
		dev_err(hl->dev, "%s, failed to init adc, ret = %d\n", __func__, ret);
		return ret;
	}

	ret = hl7139_init_int_src(hl);
	if (ret < 0)
		dev_err(hl->dev, "%s, failed to init int source, ret = %d\n", __func__, ret);

	return ret;
}

static int hl7139_set_present(struct hl7139 *hl, bool present)
{
	int ret;

	hl->usb_present = present;

	if (present) {
		hl7139_init_device(hl);
		ret = hl7139_set_wdt(hl, hl->cfg->wdt_timer);
		if (ret) {
			dev_err(hl->dev, "%s, failed to set wdt time, ret=%d\n", __func__, ret);
			return ret;
		}

		ret = hl7139_enable_wdt(hl, true);
		if (ret) {
			dev_err(hl->dev, "%s, failed to enable wdt, ret=%d\n", __func__, ret);
			return ret;
		}

		schedule_delayed_work(&hl->wdt_work, 0);
	}

	return 0;
}

static ssize_t registers_show(struct device *dev,
			      struct device_attribute *attr,
			      char *buf)
{
	struct hl7139 *hl = dev_get_drvdata(dev);
	u8 addr, val, tmpbuf[300];
	u8 addr_start = HL7139_REG_00, addr_end = HL7139_REG_4F;
	int len, ret;
	int idx = 0;

	idx = snprintf(buf, PAGE_SIZE, "%s:\n", "hl7139");
	for (addr = addr_start; addr <= addr_end; addr++) {
		ret = hl7139_read_byte(hl, addr, &val);
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
	struct hl7139 *hl = dev_get_drvdata(dev);
	int ret;
	unsigned int reg;
	unsigned int val;
	u8 addr_end = HL7139_REG_4F;

	ret = sscanf(buf, "%x %x", &reg, &val);
	if (ret == 2 && reg <= addr_end)
		hl7139_write_byte(hl, (unsigned char)reg, (unsigned char)val);

	return count;
}

static DEVICE_ATTR_RW(registers);

static int hl7139_create_device_node(struct device *dev)
{
	return device_create_file(dev, &dev_attr_registers);
}

static enum power_supply_property hl7139_charger_props[] = {
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

static void hl7139_check_fault_status(struct hl7139 *hl);

static int hl7139_get_present_status(struct hl7139 *hl, int *intval)
{
	int ret = 0;
	bool result = false;

	if (*intval == CM_USB_PRESENT_CMD)
		result = hl->usb_present;
	else
		dev_err(hl->dev, "get present cmd = %d is error\n", *intval);

	*intval = result;

	return ret;
}

static int hl7139_get_temperature(struct hl7139 *hl, int *intval)
{
	int ret = 0;
	int result = 0;

	if (*intval == CM_DIE_TEMP_CMD) {
		ret = hl7139_get_adc_data(hl, ADC_TDIE, &result);
		if (!ret)
			hl->die_temp = result;
	} else {
		dev_err(hl->dev, "get temperature cmd = %d is error\n", *intval);
	}

	*intval = result;

	return ret;
}

static void hl7139_clear_alarm_status(struct hl7139 *hl)
{
	hl->bat_ucp_alarm = false;
	hl->bat_ocp_alarm = false;
	hl->bat_ovp_alarm = false;
	hl->bus_ovp_alarm = false;
	hl->bus_ocp_alarm = false;
}

static int hl7139_get_alarm_status(struct hl7139 *hl)
{
	int ret, batt_ma, batt_mv, vbus_mv, ibus_ma;

	ret = hl7139_get_ibat_now_mA(hl, &batt_ma);
	if (ret) {
		dev_err(hl->dev, "%s[%d], get batt_ma error, ret=%d\n",
			__func__, __LINE__, ret);
		return ret;
	}

	ret = hl7139_get_adc_data(hl, ADC_VBAT, &batt_mv);
	if (ret) {
		dev_err(hl->dev, "%s[%d], get batt_mv error, ret=%d\n",
			__func__, __LINE__, ret);
		return ret;
	}

	ret = hl7139_get_adc_data(hl, ADC_VBUS, &vbus_mv);
	if (ret) {
		dev_err(hl->dev, "%s[%d], get vbus_mv error, ret=%d\n",
			__func__, __LINE__, ret);
		return ret;
	}

	ret = hl7139_get_adc_data(hl, ADC_IBUS, &ibus_ma);
	if (ret) {
		dev_err(hl->dev, "%s[%d], get ibus_ma error, ret=%d\n",
			__func__, __LINE__, ret);
		return ret;
	}

	if (!hl->cfg->bat_ucp_alm_disable && hl->cfg->bat_ucp_alm_th > 0 &&
	    batt_ma < hl->cfg->bat_ucp_alm_th)
		hl->bat_ucp_alarm = true;

	if (!hl->cfg->bat_ocp_alm_disable && hl->cfg->bat_ocp_alm_th > 0 &&
	    batt_ma > hl->cfg->bat_ocp_alm_th)
		hl->bat_ocp_alarm = true;

	if (!hl->cfg->bat_ovp_alm_disable && hl->cfg->bat_ovp_alm_th > 0 &&
	    batt_mv > hl->cfg->bat_ovp_alm_th)
		hl->bat_ovp_alarm = true;

	if (!hl->cfg->bus_ovp_alm_disable && hl->cfg->bus_ovp_alm_th > 0 &&
	    vbus_mv > hl->cfg->bus_ovp_alm_th)
		hl->bus_ovp_alarm = true;

	if (!hl->cfg->bus_ocp_alm_disable && hl->cfg->bus_ocp_alm_th > 0 &&
	    ibus_ma > hl->cfg->bus_ocp_alm_th)
		hl->bus_ocp_alarm = true;

	dev_dbg(hl->dev, "%s, batt_ma = %d, batt_mv = %d, vbus_mv = %d, ibus_ma = %d\n",
		__func__, batt_ma, batt_mv, vbus_mv, ibus_ma);

	dev_dbg(hl->dev, "%s, bat_alarm: ucp=%d, ocp=%d, ovp=%d, bus_alarm: ovp=%d, ocp=%d\n",
		__func__, hl->bat_ucp_alarm, hl->bat_ocp_alarm, hl->bat_ovp_alarm,
		hl->bus_ovp_alarm, hl->bus_ocp_alarm);

	return 0;
}

static void hl7139_charger_watchdog_work(struct work_struct *work)
{
	struct delayed_work *dwork = to_delayed_work(work);
	struct hl7139 *hl = container_of(dwork, struct hl7139, wdt_work);

	if (hl7139_set_wdt(hl, hl->cfg->wdt_timer) < 0)
		dev_err(hl->dev, "Fail to feed watchdog\n");

	schedule_delayed_work(&hl->wdt_work, HZ * 15);
}

static int hl7139_charger_get_property(struct power_supply *psy,
				       enum power_supply_property psp,
				       union power_supply_propval *val)
{
	struct hl7139 *hl = power_supply_get_drvdata(psy);
	int result = 0;
	int ret, cmd;

	if (!hl) {
		pr_err("%s[%d], NULL pointer!!!\n", __func__, __LINE__);
		return -EINVAL;
	}

	dev_dbg(hl->dev, ">>>>>psp = %d\n", psp);

	switch (psp) {
	case POWER_SUPPLY_PROP_CALIBRATE:
		if (!hl7139_check_charge_enabled(hl, &hl->charge_enabled))
			val->intval = hl->charge_enabled;
		break;
	case POWER_SUPPLY_PROP_PRESENT:
		cmd = val->intval;
		if (!hl7139_get_present_status(hl, &val->intval))
			dev_err(hl->dev, "fail to get present status, cmd = %d\n", cmd);
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		ret = hl7139_get_adc_data(hl, ADC_VBAT, &result);
		if (!ret)
			hl->vbat_volt = result;

		val->intval = hl->vbat_volt * 1000;
		dev_dbg(hl->dev, "ADC_VBAT = %d\n", val->intval);
		break;
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		if (val->intval == CM_IBAT_CURRENT_NOW_CMD) {
			if (!hl7139_get_ibat_now_mA(hl, &result))
				hl->ibat_curr = result;

			val->intval = hl->ibat_curr * 1000;
			dev_dbg(hl->dev, "ADC_IBAT = %d\n", val->intval);
			break;
		}

		if (!hl7139_check_charge_enabled(hl, &hl->charge_enabled)) {
			if (!hl->charge_enabled) {
				val->intval = 0;
			} else {
				ret = hl7139_get_adc_data(hl, ADC_IBUS, &result);
				if (!ret)
					hl->ibus_curr = result;
				val->intval = hl->ibus_curr * 1000;
				dev_dbg(hl->dev, "ADC_IBUS = %d\n", val->intval);
			}
		}
		break;
	case POWER_SUPPLY_PROP_TEMP:
		cmd = val->intval;
		if (hl7139_get_temperature(hl, &val->intval))
			dev_err(hl->dev, "fail to get temperature, cmd = %d\n", cmd);
		break;
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE:
		ret = hl7139_get_adc_data(hl, ADC_VBUS, &result);
		if (!ret)
			hl->vbus_volt = result;

		val->intval = hl->vbus_volt * 1000;
		dev_dbg(hl->dev, "ADC_VBUS = %d\n", val->intval);
		break;
	case POWER_SUPPLY_PROP_HEALTH:
		if (val->intval == CM_SOFT_ALARM_HEALTH_CMD) {
			ret = hl7139_get_alarm_status(hl);
			if (ret) {
				dev_err(hl->dev, "fail to get alarm status, ret=%d\n", ret);
				val->intval = 0;
				break;
			}

			val->intval = ((hl->bat_ovp_alarm << CM_CHARGER_BAT_OVP_ALARM_SHIFT)
				| (hl->bat_ocp_alarm << CM_CHARGER_BAT_OCP_ALARM_SHIFT)
				| (hl->bat_ucp_alarm << CM_CHARGER_BAT_UCP_ALARM_SHIFT)
				| (hl->bus_ovp_alarm << CM_CHARGER_BUS_OVP_ALARM_SHIFT)
				| (hl->bus_ocp_alarm << CM_CHARGER_BUS_OCP_ALARM_SHIFT));

			hl7139_clear_alarm_status(hl);
			break;
		}

		if (val->intval == CM_BUS_ERR_HEALTH_CMD) {
			hl7139_check_vbus_error_status(hl);
			val->intval = (hl->bus_err_lo  << CM_CHARGER_BUS_ERR_LO_SHIFT);
			val->intval |= (hl->bus_err_hi  << CM_CHARGER_BUS_ERR_HI_SHIFT);
			break;
		}

		hl7139_check_fault_status(hl);
		val->intval = ((hl->bat_ovp_fault << CM_CHARGER_BAT_OVP_FAULT_SHIFT)
			| (hl->bat_ocp_fault << CM_CHARGER_BAT_OCP_FAULT_SHIFT)
			| (hl->bus_ovp_fault << CM_CHARGER_BUS_OVP_FAULT_SHIFT)
			| (hl->bus_ocp_fault << CM_CHARGER_BUS_OCP_FAULT_SHIFT));

		hl7139_dump_reg(hl);
		break;
	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
		if (!hl7139_check_charge_enabled(hl, &hl->charge_enabled)) {
			if (!hl->charge_enabled)
				val->intval = 0;
			else
				val->intval = hl->cfg->bus_ocp_alm_th * 1000;
		}
		break;
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT:
		if (!hl7139_check_charge_enabled(hl, &hl->charge_enabled)) {
			if (!hl->charge_enabled)
				val->intval = 0;
			else
				val->intval = hl->cfg->bat_ocp_alm_th * 1000;
		}
		break;
	default:
		return -EINVAL;

	}

	return 0;
}

static int hl7139_charger_set_property(struct power_supply *psy,
				       enum power_supply_property prop,
				       const union power_supply_propval *val)
{
	struct hl7139 *hl = power_supply_get_drvdata(psy);
	int ret;

	if (!hl) {
		pr_err("%s[%d], NULL pointer!!!\n", __func__, __LINE__);
		return -EINVAL;
	}

	dev_dbg(hl->dev, "<<<<<prop = %d\n", prop);

	switch (prop) {
	case POWER_SUPPLY_PROP_CALIBRATE:
		if (!val->intval) {
			hl7139_enable_adc(hl, false);
			cancel_delayed_work_sync(&hl->wdt_work);
		}

		ret = hl7139_enable_charge(hl, val->intval);
		if (ret)
			dev_err(hl->dev, "%s, failed to %s charge\n",
				__func__, val->intval ? "enable" : "disable");

		if (hl7139_check_charge_enabled(hl, &hl->charge_enabled))
			dev_err(hl->dev, "%s, failed to check charge enabled\n", __func__);

		break;
	case POWER_SUPPLY_PROP_PRESENT:
		if (val->intval == CM_USB_PRESENT_CMD)
			hl7139_set_present(hl, true);
		break;
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE_MAX:
		ret = hl7139_set_batovp_th(hl, val->intval / 1000);
		if (ret)
			dev_err(hl->dev, "%s, failed to set bat ovp th %d mv, ret = %d\n",
				__func__, val->intval / 1000, ret);

		hl->cfg->bat_ovp_alm_th = val->intval / 1000 - hl->cfg->bat_delta_volt;
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int hl7139_charger_is_writeable(struct power_supply *psy,
				       enum power_supply_property prop)
{
	int ret;

	switch (prop) {
	case POWER_SUPPLY_PROP_CALIBRATE:
	case POWER_SUPPLY_PROP_PRESENT:
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE_MAX:
		ret = 1;
		break;
	default:
		ret = 0;
		break;
	}
	return ret;
}

static int hl7139_psy_register(struct hl7139 *hl)
{
	hl->psy_cfg.drv_data = hl;
	hl->psy_cfg.of_node = hl->dev->of_node;

	if (hl->mode == HL7139_ROLE_MASTER)
		hl->psy_desc.name = "hl7139-master";
	else if (hl->mode == HL7139_ROLE_SLAVE)
		hl->psy_desc.name = "hl7139-slave";
	else
		hl->psy_desc.name = "hl7139-standalone";

	hl->psy_desc.type = POWER_SUPPLY_TYPE_UNKNOWN;
	hl->psy_desc.properties = hl7139_charger_props;
	hl->psy_desc.num_properties = ARRAY_SIZE(hl7139_charger_props);
	hl->psy_desc.get_property = hl7139_charger_get_property;
	hl->psy_desc.set_property = hl7139_charger_set_property;
	hl->psy_desc.property_is_writeable = hl7139_charger_is_writeable;

	hl->fc2_psy = devm_power_supply_register(hl->dev,
						 &hl->psy_desc, &hl->psy_cfg);
	if (IS_ERR(hl->fc2_psy)) {
		dev_err(hl->dev, "failed to register fc2_psy\n");
		return PTR_ERR(hl->fc2_psy);
	}

	dev_info(hl->dev, "%s power supply register successfully\n", hl->psy_desc.name);

	return 0;
}

static irqreturn_t hl7139_charger_interrupt(int irq, void *dev_id);

static int hl7139_init_irq(struct hl7139 *hl)
{
	int ret;
	const char *dev_name;

	gpio_free(hl->irq_gpio);

	dev_err(hl->dev, ">>>>>>>>>>>>%d\n", hl->irq_gpio);
	ret = gpio_request(hl->irq_gpio, "hl7139");
	if (ret < 0) {
		dev_err(hl->dev, "fail to request GPIO(%d), ret = %d\n", hl->irq_gpio, ret);
		return ret;
	}

	ret = gpio_direction_input(hl->irq_gpio);
	if (ret < 0) {
		dev_err(hl->dev, "fail to set GPIO(%d) as input pin, ret = %d\n",
			hl->irq_gpio, ret);
		return ret;
	}

	hl->irq = gpio_to_irq(hl->irq_gpio);
	if (hl->irq <= 0) {
		dev_err(hl->dev, "irq mapping fail\n");
		return 0;
	}

	dev_info(hl->dev, "irq: %d\n",  hl->irq);

	if (hl->mode == HL7139_ROLE_MASTER)
		dev_name = "hl7139 master irq";
	else if (hl->mode == HL7139_ROLE_SLAVE)
		dev_name = "hl7139 slave irq";
	else
		dev_name = "hl7139 standalone irq";

	ret = devm_request_threaded_irq(hl->dev, hl->irq,
					NULL, hl7139_charger_interrupt,
					IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
					dev_name, hl);
	if (ret < 0)
		dev_err(hl->dev, "request irq for irq=%d failed, ret =%d\n", hl->irq, ret);

	enable_irq_wake(hl->irq);

	device_init_wakeup(hl->dev, 1);

	return 0;
}

static void hl7139_dump_reg(struct hl7139 *hl)
{

	int ret;
	u8 val;
	u8 addr;
	u8 addr_start = HL7139_REG_00, addr_end = HL7139_REG_4F;

	for (addr = addr_start; addr <= addr_end; addr++) {
		ret = hl7139_read_byte(hl, addr, &val);
		if (!ret)
			dev_err(hl->dev, "Reg[%02X] = 0x%02X\n", addr, val);
	}
}

static void hl7139_check_fault_status(struct hl7139 *hl)
{
	int ret;
	u8 reg_val = 0;
	u8 int_val = 0, sts_a_val = 0, sts_b_val = 0, sts_c_val = 0;

	mutex_lock(&hl->data_lock);

	ret = hl7139_read_byte(hl, HL7139_REG_01, &int_val);
	if (!ret && int_val)
		dev_err(hl->dev, "%s, INT = 0x%02x\n", __func__, int_val);

	ret = hl7139_read_byte(hl, HL7139_REG_03, &reg_val);
	if (!ret && reg_val)
		dev_err(hl->dev, "%s, INT_STS_A = 0x%02x\n", __func__, reg_val);

	ret = hl7139_read_byte(hl, HL7139_REG_04, &reg_val);
	if (!ret && reg_val)
		dev_err(hl->dev, "%s, INT_STS_B = 0x%02x\n", __func__, reg_val);

	ret = hl7139_read_byte(hl, HL7139_REG_05, &sts_a_val);
	if (!ret && sts_a_val)
		dev_err(hl->dev, "%s, STATUS_A = 0x%02x\n", __func__, sts_a_val);

	ret = hl7139_read_byte(hl, HL7139_REG_06, &sts_b_val);
	if (!ret && sts_b_val)
		dev_err(hl->dev, "%s, STATUS_B = 0x%02x\n", __func__, sts_b_val);

	ret = hl7139_read_byte(hl, HL7139_REG_07, &sts_c_val);
	if (!ret && sts_c_val)
		dev_err(hl->dev, "%s, STATUS_C = 0x%02x\n", __func__, sts_c_val);

	if (!ret && sts_a_val != hl->prev_fault_a && (int_val & HL7139_V_OK_I_MASK)) {
		hl->prev_fault_a = sts_a_val;
		hl->bus_ovp_fault = !!(sts_a_val & HL7139_VIN_OVP_STS_MASK);
		hl->bat_ovp_fault = !!(sts_a_val & HL7139_VBAT_OVP_STS_MASK);
	}

	if (!ret && sts_b_val != hl->prev_fault_b && (int_val & HL7139_CUR_I_MASK)) {
		hl->prev_fault_b = sts_b_val;
		hl->bus_ocp_fault = !!(sts_b_val & HL7139_IIN_OCP_STS_MASK);
		hl->bat_ocp_fault = !!(sts_b_val & HL7139_IBAT_OCP_STS_MASK);
	}

	if (!ret && sts_b_val && (int_val & HL7139_SHORT_I_MASK)) {
		if (sts_b_val & HL7139_FET_SHORT_STS_MASK)
			dev_err(hl->dev, "%s, The chip is damaged.\n", __func__);
		else if (sts_b_val & HL7139_CFLY_SHORT_STS_MASK)
			dev_err(hl->dev, "%s, Abnormal connection of external capacitor.\n",
				__func__);
	}

	mutex_unlock(&hl->data_lock);
}

/*
 * interrupt does nothing, just info event chagne, other module could get info
 * through power supply interface
 */
static irqreturn_t hl7139_charger_interrupt(int irq, void *dev_id)
{
	struct hl7139 *hl = dev_id;

	dev_info(hl->dev, "INT OCCURRED\n");
	cm_notify_event(hl->fc2_psy, CM_EVENT_INT, NULL);

	return IRQ_HANDLED;
}

static void hl7139_determine_initial_status_work(struct work_struct *work)
{
	struct delayed_work *dwork = to_delayed_work(work);
	struct hl7139 *hl = container_of(dwork, struct hl7139, det_init_stat_work);

	hl7139_dump_reg(hl);
}


static const struct of_device_id hl7139_charger_match_table[] = {
	{
		.compatible = "hl,hl7139-standalone",
		.data = &hl7139_mode_data[HL7139_STDALONE],
	},
	{
		.compatible = "hl,hl7139-master",
		.data = &hl7139_mode_data[HL7139_MASTER],
	},

	{
		.compatible = "hl,hl7139-slave",
		.data = &hl7139_mode_data[HL7139_SLAVE],
	},
	{},
};

static int hl7139_charger_probe(struct i2c_client *client,
				const struct i2c_device_id *id)
{
	struct hl7139 *hl;
	const struct of_device_id *match;
	struct device_node *node = client->dev.of_node;
	struct power_supply *fuel_gauge;
	int ret;

	hl = devm_kzalloc(&client->dev, sizeof(struct hl7139), GFP_KERNEL);
	if (!hl)
		return -ENOMEM;

	hl->dev = &client->dev;

	hl->client = client;

	mutex_init(&hl->i2c_rw_lock);
	mutex_init(&hl->data_lock);
	mutex_init(&hl->charging_disable_lock);
	mutex_init(&hl->irq_complete);

	hl->resume_completed = true;
	hl->irq_waiting = false;

	ret = hl7139_detect_device(hl);
	if (ret) {
		dev_err(hl->dev, "No hl7139 device found!\n");
		return -ENODEV;
	}

	i2c_set_clientdata(client, hl);
	ret = hl7139_create_device_node(&(client->dev));
	if (ret) {
		dev_err(hl->dev, "failed to create device node, ret = %d\n", ret);
		return ret;
	}

	match = of_match_node(hl7139_charger_match_table, node);
	if (match == NULL) {
		dev_err(hl->dev, "device tree match not found!\n");
		return -ENODEV;
	}

	hl->mode = *(int *)match->data;
	dev_info(hl->dev, "work mode:%s\n", hl->mode == HL7139_ROLE_STDALONE ? "Standalone" :
		 (hl->mode == HL7139_ROLE_SLAVE ? "Slave" : "Master"));

	ret = hl7139_parse_dt(hl, &client->dev);
	if (ret)
		return -EIO;

	of_property_read_string(node, "cm-fuel-gauge", &hl->cfg->psy_fuel_gauge);
	if (hl->cfg->psy_fuel_gauge) {
		fuel_gauge = power_supply_get_by_name(hl->cfg->psy_fuel_gauge);
		if (!fuel_gauge) {
			dev_err(hl->dev, "Cannot find power supply \"%s\"\n",
				hl->cfg->psy_fuel_gauge);
			return -EPROBE_DEFER;
		}

		power_supply_put(fuel_gauge);
		dev_info(hl->dev, "Get battery information from FGU\n");
	}

	ret = hl7139_init_device(hl);
	if (ret) {
		dev_err(hl->dev, "Failed to init device\n");
		return ret;
	}

	if (!hl7139_check_device_mode_match(hl)) {
		dev_err(hl->dev, "Device mode mismatch\n");
		return ret;
	}

	INIT_DELAYED_WORK(&hl->wdt_work, hl7139_charger_watchdog_work);
	INIT_DELAYED_WORK(&hl->det_init_stat_work, hl7139_determine_initial_status_work);
	ret = hl7139_psy_register(hl);
	if (ret)
		return ret;

	ret = hl7139_init_irq(hl);
	if (ret)
		return ret;

	schedule_delayed_work(&hl->det_init_stat_work, msecs_to_jiffies(100));
	dev_info(hl->dev, "hl7139 probe successfully, Part Num:%d\n!",
		 hl->part_no);

	return 0;
}

static inline bool is_device_suspended(struct hl7139 *hl)
{
	return !hl->resume_completed;
}

static int hl7139_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct hl7139 *hl = i2c_get_clientdata(client);

	mutex_lock(&hl->irq_complete);
	hl->resume_completed = false;
	mutex_unlock(&hl->irq_complete);
	dev_info(dev, "Suspend successfully!");

	return 0;
}

static int hl7139_suspend_noirq(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct hl7139 *hl = i2c_get_clientdata(client);

	if (hl->irq_waiting) {
		pr_err_ratelimited("Aborting suspend, an interrupt was detected while suspending\n");
		return -EBUSY;
	}
	return 0;
}

static int hl7139_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct hl7139 *hl = i2c_get_clientdata(client);

	mutex_lock(&hl->irq_complete);
	hl->resume_completed = true;
	if (hl->irq_waiting) {
		hl->irq_disabled = false;
		enable_irq(client->irq);
		mutex_unlock(&hl->irq_complete);
		hl7139_charger_interrupt(client->irq, hl);
	} else {
		mutex_unlock(&hl->irq_complete);
	}

	dev_info(dev, "Resume successfully!");

	return 0;
}
static int hl7139_charger_remove(struct i2c_client *client)
{
	struct hl7139 *hl = i2c_get_clientdata(client);

	hl7139_enable_adc(hl, false);
	cancel_delayed_work_sync(&hl->wdt_work);

	mutex_destroy(&hl->charging_disable_lock);
	mutex_destroy(&hl->data_lock);
	mutex_destroy(&hl->i2c_rw_lock);
	mutex_destroy(&hl->irq_complete);

	return 0;
}

static void hl7139_charger_shutdown(struct i2c_client *client)
{
	struct hl7139 *hl = i2c_get_clientdata(client);

	hl7139_enable_adc(hl, false);
	cancel_delayed_work_sync(&hl->wdt_work);
}

static const struct dev_pm_ops hl7139_pm_ops = {
	.resume		= hl7139_resume,
	.suspend_noirq	= hl7139_suspend_noirq,
	.suspend	= hl7139_suspend,
};

static const struct i2c_device_id hl7139_charger_id[] = {
	{"hl7139-standalone", HL7139_ROLE_STDALONE},
	{"hl7139-master", HL7139_ROLE_MASTER},
	{"hl7139-slave", HL7139_ROLE_SLAVE},
	{},
};

static struct i2c_driver hl7139_charger_driver = {
	.driver		= {
		.name	= "hl7139-charger",
		.owner	= THIS_MODULE,
		.of_match_table = hl7139_charger_match_table,
		.pm	= &hl7139_pm_ops,
	},
	.id_table	= hl7139_charger_id,

	.probe		= hl7139_charger_probe,
	.remove		= hl7139_charger_remove,
	.shutdown	= hl7139_charger_shutdown,
};

module_i2c_driver(hl7139_charger_driver);

MODULE_DESCRIPTION("HL hl7139 Charge Pump Driver");
MODULE_LICENSE("GPL v2");

