/*
 * BQ25790 battery charging driver
 *
 * Copyright (C) 2017 Texas Instruments * * This package is free software; you can redistribute it and/or modify
 * Copyright (C) 2021 XiaoMi, Inc.
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.

 * THIS PACKAGE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */


#define pr_fmt(fmt)	"[bq25790] %s: " fmt, __func__

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
#include <linux/regmap.h>
#include <linux/pmic-voter.h>

#include "bq25790_reg.h"
#include "bq25790.h"

enum print_reason {
	PR_INTERRUPT    = BIT(0),
	PR_REGISTER     = BIT(1),
	PR_OEM          = BIT(2),
	PR_DEBUG        = BIT(3),
};

static int debug_mask = PR_OEM;
module_param_named(debug_mask, debug_mask, int, 0600);

#define bq_dbg(reason, fmt, ...)                        \
	do {                                            \
		if (debug_mask & (reason))              \
			pr_info(fmt, ##__VA_ARGS__);    \
		else					\
			pr_debug(fmt, ##__VA_ARGS__);	\
	} while (0)

enum {
	BQ25790 = 0x01,
};

struct bq25790 {
	struct device *dev;
	struct i2c_client *client;

	int part_no;
	int revision;

	struct mutex data_lock;
	struct mutex i2c_rw_lock;
	struct mutex profile_change_lock;
	struct mutex charging_disable_lock;
	struct mutex irq_complete;

	struct notifier_block   nb;

	bool irq_waiting;
	bool irq_disabled;
	bool resume_completed;

	bool usb_present;

	bool power_good;
	bool batt_full;
	bool charge_done;
	int charge_state;
	bool charge_enabled;	/* Register bit status */
	bool chip_ok;
	int charger_type;
	int hvdcp_class;
	int arti_vbus_gpio;
	bool arti_vbus_enable;
	int reverse_gpio;
	bool hiz_mode;
	bool enable_term;
	bool mtbf;

	int vbus_volt;
	int vbat_volt;
	int vsys_volt;
	int ibus_curr;
	int ichg_curr;
	int die_temp;
	int ts_temp;
	int batt_temp;
	int batt_soc;
	int rechg_vol;

	int dev_id;

	struct delayed_work charge_monitor_work;
	int termi_curr;

	int skip_writes;
	int skip_reads;

	struct bq25790_platform_data *platform_data;

	struct delayed_work charge_irq_work; /*charge mode jeita work*/
	struct delayed_work charge_status_change_work;

	struct power_supply *usb_psy;
	struct power_supply *bms_psy;
	struct power_supply *bbc_psy;
	struct power_supply *bat_psy;
	struct power_supply *wls_psy;
	struct power_supply_desc bbc_psy_d;

	struct regmap    *regmap;
	struct votable	*fcc_votable;
	struct votable	*fv_votable;
	struct votable	*ilim_votable;
	struct votable	*cp_disable_votable;
	struct votable	*passthrough_dis_votable;
	struct votable	*awake_votable;
	struct votable	*chg_dis_votable;
	struct votable	*arti_vbus_dis_votable;
	struct votable	*ffc_mode_disable;
};

enum {
	ADC_IBUS,
	ADC_IBAT,
	ADC_VBUS,
	ADC_VAC1,
	ADC_VAC2,
	ADC_VBAT,
	ADC_VSYS,
	ADC_TS,
	ADC_TDIE,
	ADC_DP,
	ADC_DM,
	ADC_MAX_NUM
};


static int __bq25790_read_reg(struct bq25790 *bq, u8 reg, u8 *data)
{
	s32 ret;

	ret = i2c_smbus_read_byte_data(bq->client, reg);
	if (ret < 0) {
		bq_dbg(PR_OEM, "i2c read byte fail: can't read from reg 0x%02X\n", reg);
		return ret;
	}

	*data = (u8)ret;
	return 0;
}

static int __bq25790_write_reg(struct bq25790 *bq, int reg, u8 val)
{
	s32 ret;

	ret = i2c_smbus_write_byte_data(bq->client, reg, val);
	if (ret < 0) {
		bq_dbg(PR_OEM, "i2c write byte fail: can't write 0x%02X to reg 0x%02X\n",
				val, reg);
		return ret;
	}

	return 0;
}

static int bq25790_read_reg(struct bq25790 *bq, u8 reg, u8 *data)
{
	int ret;

	if (bq->skip_reads) {
		*data = 0;
		return 0;
	}

	mutex_lock(&bq->i2c_rw_lock);
	ret = __bq25790_read_reg(bq, reg, data);
	if (ret) {
		bq_dbg(PR_OEM, "Failed: reg=%02X, ret=%d\n", reg, ret);
	}
	mutex_unlock(&bq->i2c_rw_lock);

	return ret;
}

static int bq25790_read_word(struct bq25790 *bq, u8 reg, u16 *data)
{
	s32 ret;
	unsigned int val;

	mutex_lock(&bq->i2c_rw_lock);
	ret = regmap_read(bq->regmap, reg, &val);
	if (ret) {
		bq_dbg(PR_OEM, "Failed: reg=%02X, ret=%d\n", reg, ret);
	}
	*data = val;
	mutex_unlock(&bq->i2c_rw_lock);

	return ret;
}

static int bq25790_write_reg(struct bq25790 *bq, u8 reg, u8 data)
{
	int ret;

	if (bq->skip_writes)
		return 0;

	mutex_lock(&bq->i2c_rw_lock);
	ret = __bq25790_write_reg(bq, reg, data);
	mutex_unlock(&bq->i2c_rw_lock);

	if (ret)
		bq_dbg(PR_OEM, "Failed: reg=%02X, ret=%d\n", reg, ret);

	return ret;
}


static int bq25790_update_byte_bits(struct bq25790 *bq, u8 reg,
					u8 mask, u8 data)
{
	int ret;
	u8 tmp;

	if (bq->skip_reads || bq->skip_writes)
		return 0;

	mutex_lock(&bq->i2c_rw_lock);
	ret = __bq25790_read_reg(bq, reg, &tmp);
	if (ret) {
		bq_dbg(PR_OEM, "Failed: reg=%02X, ret=%d\n", reg, ret);
		goto out;
	}

	tmp &= ~mask;
	tmp |= data & mask;

	ret = __bq25790_write_reg(bq, reg, tmp);
	if (ret)
		bq_dbg(PR_OEM, "Failed: reg=%02X, ret=%d\n", reg, ret);

out:
	mutex_unlock(&bq->i2c_rw_lock);
	return ret;
}

static int bq25790_update_word_bits(struct bq25790 *bq, u8 reg,
					u16 mask, u16 data)
{
	int ret;

	if (bq->skip_reads || bq->skip_writes)
		return 0;

	mutex_lock(&bq->i2c_rw_lock);
	ret = regmap_update_bits(bq->regmap, reg, mask, data);
	if (ret) {
		bq_dbg(PR_OEM, "Failed: reg=%02X, ret=%d\n", reg, ret);
	}

	mutex_unlock(&bq->i2c_rw_lock);

	return ret;
}

static int bq25790_set_revese_gpio(struct bq25790 *bq, bool enable)
{
	int ret, val;

	if (enable) {
		val = REVERSE_VBUS_ENABLE;
	} else {
		val = REVERSE_VBUS_DISABLE;
	}

	ret = gpio_direction_output(bq->reverse_gpio, val);
	if (ret)
		bq_dbg(PR_OEM, "failed to set reverse gpio dir ret:%d", ret);

	gpio_set_value(bq->reverse_gpio, val);

	bq_dbg(PR_OEM, "set reverse vbus to:%d, bq->reverse_gpio:%d \n", enable, bq->reverse_gpio);
	return ret;
}

static int bq25790_set_arti_vbus_disable(struct bq25790 *bq, bool disable)
{
	int ret, val;

	if (disable) {
		val = ARTI_VBUS_DISABLE;
	} else {
		val = ARTI_VBUS_ENABLE;
	}
	ret = gpio_direction_output(bq->arti_vbus_gpio, val);
	if (ret)
		bq_dbg(PR_OEM, "failed to set gpio dir ret:%d", ret);

	gpio_set_value(bq->arti_vbus_gpio, val);
	bq25790_set_revese_gpio(bq, val);

	bq->arti_vbus_enable = val;

	bq_dbg(PR_OEM, "set arti vbus disable to:%d   bq->arti_vbus_enable:%d\n", disable,  bq->arti_vbus_enable);
	return ret;
}

static int bq25790_set_charge_voltage(struct bq25790 *bq, int volt)
{
	int ret;
	u16 reg_val;

	volt /= 1000;

	if (volt < BQ25790_VREG_BASE)
		volt = BQ25790_VREG_BASE;

	volt -= BQ25790_VREG_BASE;
	reg_val = volt / BQ25790_VREG_LSB;
	reg_val <<= BQ25790_VREG_SHIFT;

	ret = bq25790_update_word_bits(bq, BQ25790_REG_CHARGE_VOLT,
				BQ25790_VREG_MASK, reg_val);

	return ret;
}

static int bq25790_set_hiz_mode(struct bq25790 *bq, bool enable)
{
	int ret;
	u8 reg_val;

	if (bq->mtbf)
		enable = false;

	if (enable == false)
		reg_val = BQ25790_HIZ_DISABLE;
	else
		reg_val = BQ25790_HIZ_ENABLE;

	reg_val <<= BQ25790_EN_HIZ_SHIFT;

	ret = bq25790_update_byte_bits(bq, BQ25790_REG_CHG_CTRL0,
				BQ25790_EN_HIZ_MASK, reg_val);

	bq->hiz_mode = enable;
	bq_dbg(PR_OEM, "set hiz mode to enable:%d\n", enable);

	return ret;
}

static int bq25790_get_hiz_mode(struct bq25790 *bq, int *status)
{
	int ret;
	u8 reg_val;

	ret = bq25790_read_reg(bq, BQ25790_REG_CHG_CTRL0, &reg_val);

	if (!ret)
		*status = reg_val & BQ25790_EN_HIZ_MASK;

	return ret;
}

static int bq25790_get_charge_current(struct bq25790 *bq, int *curr)
{
	int ret;
	u16 reg_val;

	ret = bq25790_read_word(bq, BQ25790_REG_CHARGE_CURRENT, &reg_val);

	reg_val &= BQ25790_ICHG_MASK;
	*curr =  reg_val * BQ25790_ICHG_LSB;
	*curr += BQ25790_ICHG_BASE;

	*curr *= 1000;

	return ret;
}

static int bq25790_get_charge_voltage(struct bq25790 *bq, int *volt)
{
	int ret;
	u16 reg_val;

	ret = bq25790_read_word(bq, BQ25790_REG_CHARGE_VOLT, &reg_val);

	reg_val &= BQ25790_VREG_MASK;
	*volt =  reg_val * BQ25790_VREG_LSB;
	*volt += BQ25790_VREG_BASE;

	*volt *= 1000;

	return ret;
}


static int bq25790_get_input_current_limit(struct bq25790 *bq, int *curr)
{
	int ret;
	u16 reg_val;


	ret = bq25790_read_word(bq, BQ25790_REG_IINDPM, &reg_val);

	reg_val &= BQ25790_IINDPM_TH_MASK;

	*curr = reg_val * BQ25790_IINDPM_TH_LSB;
	*curr += BQ25790_IINDPM_TH_BASE;

	*curr *= 1000;

	return ret;
}


static int bq25790_set_charge_current(struct bq25790 *bq, int curr)
{
	int ret;
	u16 reg_val;

	curr /= 1000;

	if (curr < BQ25790_ICHG_BASE)
		curr = BQ25790_ICHG_BASE;

	curr -= BQ25790_ICHG_BASE;
	reg_val = curr / BQ25790_ICHG_LSB;
	reg_val <<= BQ25790_ICHG_SHIFT;

	ret = bq25790_update_word_bits(bq, BQ25790_REG_CHARGE_CURRENT,
				BQ25790_ICHG_MASK, reg_val);

	return ret;
}


static int bq25790_set_input_volt_limit(struct bq25790 *bq, int volt)
{
	int ret;
	u8 reg_val;

	volt /= 1000;

	if (volt < BQ25790_VINDPM_TH_BASE)
		volt = BQ25790_VINDPM_TH_BASE;

	volt -= BQ25790_VINDPM_TH_BASE;
	reg_val = volt / BQ25790_VINDPM_TH_LSB;
	reg_val <<= BQ25790_VINDPM_TH_SHIFT;

	ret = bq25790_update_byte_bits(bq, BQ25790_REG_VINDPM,
				BQ25790_VINDPM_TH_MASK, reg_val);

	return ret;
}

static int bq25790_get_input_volt_limit(struct bq25790 *bq, int *volt)
{
	int ret;
	u16 reg_val;

	ret = bq25790_read_word(bq, BQ25790_REG_VINDPM, &reg_val);

	reg_val &= BQ25790_VINDPM_TH_MASK;

	*volt = reg_val * BQ25790_VINDPM_TH_LSB;
	*volt += BQ25790_VINDPM_TH_BASE;

	*volt *= 1000;

	return ret;
}

static int bq25790_set_ico_mode(struct bq25790 *bq, bool enable)
{
	int ret;
	u8 reg_val;

	if (enable == false)
		reg_val = BQ25790_ICO_DISABLE;
	else
		reg_val = BQ25790_ICO_ENABLE;

	reg_val <<= BQ25790_EN_ICO_SHIFT;

	ret = bq25790_update_byte_bits(bq, BQ25790_REG_CHG_CTRL0,
				BQ25790_EN_ICO_MASK, reg_val);

	return ret;
}

static int bq25790_set_pfm_fwd(struct bq25790 *bq, bool enable)
{
	int ret;
	u8 reg_val;

	if (enable == false)
		reg_val = BQ25790_PFM_FWD_DIS;
	else
		reg_val = BQ25790_PFM_FWD_EN;

	reg_val <<= BQ25790_PFM_FWD_DIS_SHIFT;

	ret = bq25790_update_byte_bits(bq, BQ25790_REG_CHG_CTRL3,
				BQ25790_PFM_FWD_DIS_MASK, reg_val);

	return ret;
}

static int bq25790_set_fwd_ooa(struct bq25790 *bq, bool enable)
{
	int ret;
	u8 reg_val;

	if (enable == false)
		reg_val = BQ25790_DIS_FWD_OOA;
	else
		reg_val = BQ25790_EN_FWD_OOA;

	reg_val <<= BQ25790_DIS_FWD_OOA_SHIFT;

	ret = bq25790_update_byte_bits(bq, BQ25790_REG_CHG_CTRL3,
				BQ25790_DIS_FWD_OOA_MASK, reg_val);

	return ret;
}

static int bq25790_set_dpdm_indet(struct bq25790 *bq, bool enable)
{
	int ret;
	u8 reg_val;

	if (enable == false)
		reg_val = BQ25790_AUTO_INDET_DIS;
	else
		reg_val = BQ25790_AUTO_INDET_EN;

	reg_val <<= BQ25790_AUTO_INDET_SHIFT;

	ret = bq25790_update_byte_bits(bq, BQ25790_REG_CHG_CTRL2,
				BQ25790_AUTO_INDET_MASK, reg_val);

	return ret;
}

static int bq25790_set_en_extilim(struct bq25790 *bq, bool enable)
{
	int ret;
	u8 reg_val;

	if (enable == false)
		reg_val = BQ25790_DIS_EXTILIM;
	else
		reg_val = BQ25790_EN_EXTILIM;

	reg_val <<= BQ25790_EN_EXTILIM_SHIFT;

	ret = bq25790_update_byte_bits(bq, BQ25790_REG_CHG_CTRL5,
				BQ25790_EN_EXTILIM_MASK, reg_val);

	return ret;
}

static int bq25790_set_input_curr_limit(struct bq25790 *bq, int curr)
{
	int ret;
	u16 reg_val;

	curr /= 1000;

	if (curr < BQ25790_IINDPM_TH_BASE)
		curr = BQ25790_IINDPM_TH_BASE;

	curr -= BQ25790_IINDPM_TH_BASE;
	reg_val = curr / BQ25790_IINDPM_TH_LSB;
	reg_val <<= BQ25790_IINDPM_TH_SHIFT;

	ret = bq25790_update_word_bits(bq, BQ25790_REG_IINDPM,
				BQ25790_IINDPM_TH_MASK, reg_val);

	return ret;
}


static int bq25790_set_prechg_current(struct bq25790 *bq, int curr)
{
	int ret;
	u8 reg_val;

	if (curr < BQ25790_IPRECHG_BASE)
		curr = BQ25790_IPRECHG_BASE;

	curr -= BQ25790_IPRECHG_BASE;
	reg_val = curr / BQ25790_IPRECHG_LSB;
	reg_val <<= BQ25790_IPRECHG_SHIFT;

	ret = bq25790_update_byte_bits(bq, BQ25790_REG_PRECHG_CTRL,
				BQ25790_IPRECHG_MASK, reg_val);

	return ret;
}

static int bq25790_set_term_enable(struct bq25790 *bq, bool enable)
{
	int ret;
	u8 reg_val;

	if (enable == false)
		reg_val = BQ25790_DIS_TERM;
	else
		reg_val = BQ25790_EN_TERM;

	reg_val <<= BQ25790_EN_TERM_SHIFT;

	ret = bq25790_update_byte_bits(bq, BQ25790_REG_CHG_CTRL0,
				BQ25790_EN_TERM_MASK, reg_val);

	bq->enable_term = enable;
	bq_dbg(PR_OEM, "set termi to enable:%d\n", enable);

	return ret;
}

static int bq25790_set_term_current(struct bq25790 *bq, int curr)
{
	int ret;
	u8 reg_val;

	if (curr < BQ25790_ITERM_BASE)
		curr = BQ25790_ITERM_BASE;

	if (curr > BQ25790_ITERM_MAX)
		curr = BQ25790_ITERM_MAX;

	bq->termi_curr = curr;

	curr -= BQ25790_ITERM_BASE;
	reg_val = curr / BQ25790_ITERM_LSB;
	reg_val <<= BQ25790_ITERM_SHIFT;

	ret = bq25790_update_byte_bits(bq, BQ25790_REG_ITERM_CTRL,
				BQ25790_ITERM_MASK, reg_val);

	return ret;
}

static int bq25790_set_vrechg_voltage(struct bq25790 *bq, int vol)
{
	int ret, vreg;
	u8 reg_val;

	bq->rechg_vol = vol;
	vreg = get_effective_result(bq->fv_votable);
	vol = (vreg - bq->rechg_vol) / 1000;

	if (vol < BQ25790_VRECHG_BASE)
		vol = BQ25790_VRECHG_BASE;

	if (vol > BQ25790_VRECHG_MAX)
		vol = BQ25790_VRECHG_MAX;

	vol -= BQ25790_VRECHG_BASE;
	reg_val = vol / BQ25790_VRECHG_LSB;
	reg_val <<= BQ25790_VRECHG_SHIFT;

	ret = bq25790_update_byte_bits(bq, BQ25790_REG_RECHAEGE_CTRL,
				BQ25790_VRECHG_MASK, reg_val);

	bq_dbg(PR_OEM, "rechg_vol:%d, regval:%x\n", bq->rechg_vol, reg_val);
	return ret;
}

static int bq25790_set_wdt_timer(struct bq25790 *bq, int time)
{
	int ret;
	u8 reg_val;

	if (time == 0)
		reg_val = BQ25790_WDT_TIMER_DISABLE;
	else if (time == 40)
		reg_val = BQ25790_WDT_TIMER_40S;
	else if (time == 80)
		reg_val = BQ25790_WDT_TIMER_80S;
	else
		reg_val = BQ25790_WDT_TIMER_160S;

	reg_val <<= BQ25790_WDT_TIMER_SHIFT;

	ret = bq25790_update_byte_bits(bq, BQ25790_REG_CHG_CTRL1,
				BQ25790_WDT_TIMER_MASK, reg_val);

	return ret;
}

static int bq25790_enable_safety_timer(struct bq25790 *bq, bool enable)
{
	int ret;
	u8 reg_val;

	if (bq->mtbf)
		enable = false;

	if (enable == false)
		reg_val = BQ25790_SAFETY_TIMER_DISABLE;
	else
		reg_val = BQ25790_SAFETY_TIMER_ENABLE;

	reg_val <<= BQ25790_SAFETY_TIMER_EN_SHIFT;

	ret = bq25790_update_byte_bits(bq, BQ25790_REG_TIMER_CTRL,
				BQ25790_SAFETY_TIMER_EN_MASK, reg_val);
	return ret;
}

static int bq25790_set_safety_timer(struct bq25790 *bq, int time)
{
	int ret;
	u8 reg_val;

	if (bq->mtbf)
		time = 24;

	if (time == 5)
		reg_val = BQ25790_SAFETY_TIMER_5H;
	else if (time == 8)
		reg_val = BQ25790_SAFETY_TIMER_8H;
	else if (time == 12)
		reg_val = BQ25790_SAFETY_TIMER_12H;
	else
		reg_val = BQ25790_SAFETY_TIMER_24H;

	reg_val <<= BQ25790_SAFETY_TIMER_SHIFT;

	ret = bq25790_update_byte_bits(bq, BQ25790_REG_TIMER_CTRL,
				BQ25790_SAFETY_TIMER_MASK, reg_val);

	return ret;
}

static int bq25790_set_pre_safety_timer(struct bq25790 *bq, int time)
{
	int ret;
	u8 reg_val;

	if (time == 30)
		reg_val = BQ25790_PRECHG_0_5H;
	else
		reg_val = BQ25790_PRECHG_2H;

	reg_val <<= BQ25790_PRECHG_TMR_SHIFT;

	ret = bq25790_update_byte_bits(bq, BQ25790_REG_IOTG_REGULATION,
				BQ25790_PRECHG_TMR_MASK, reg_val);

	return ret;
}

static int bq25790_set_vac_ovp(struct bq25790 *bq, int volt)
{
	int ret;
	u8 reg_val;

	if (volt == 7)
		reg_val = BQ25790_VAC_OVP_7V;
	else if (volt == 12)
		reg_val = BQ25790_VAC_OVP_12V;
	else if (volt == 18)
		reg_val = BQ25790_VAC_OVP_18V;
	else
		reg_val = BQ25790_VAC_OVP_26V;

	reg_val <<= BQ25790_VAC_OVP_SHIFT;

	ret = bq25790_update_byte_bits(bq, BQ25790_REG_CHG_CTRL1,
				BQ25790_VAC_OVP_MASK, reg_val);
	return ret;
}

static int bq25790_charge_enable(struct bq25790 *bq, bool enable)
{
	int ret;
	u8 reg_val;

	if (bq->mtbf)
		enable = true;

	if (enable == false)
		reg_val = BQ25790_CHARGE_DISABLE;
	else
		reg_val = BQ25790_CHARGE_ENABLE;

	reg_val <<= BQ25790_CHARGE_EN_SHIFT;

	ret = bq25790_update_byte_bits(bq, BQ25790_REG_CHG_CTRL0,
				BQ25790_CHARGE_EN_MASK, reg_val);

	return ret;
}


static int bq25790_reset_wdt(struct bq25790 *bq)
{
	int ret;
	u8 reg_val;

	reg_val = BQ25790_WDT_RESET << BQ25790_WDT_RESET_SHIFT;

	ret = bq25790_update_byte_bits(bq, BQ25790_REG_CHG_CTRL1,
				BQ25790_WDT_RESET_MASK, reg_val);

	return ret;
}

static int bq25790_set_cell_num(struct bq25790 *bq, int num)
{
	int ret;
	u8 reg_val;

	num -= 1;
	reg_val = num << BQ25790_CELL_SHIFT;

	ret = bq25790_update_byte_bits(bq, BQ25790_REG_RECHAEGE_CTRL,
				BQ25790_CELL_MASK, reg_val);

	return ret;
}

static int bq25790_set_vsys_min_volt(struct bq25790 *bq, int volt)
{
	int ret;
	u8 reg_val;

	if (volt < BQ25790_VSYSMIN_BASE)
		volt = BQ25790_VSYSMIN_BASE;

	volt -= BQ25790_VSYSMIN_BASE;
	reg_val = volt / BQ25790_VSYSMIN_LSB;
	reg_val <<= BQ25790_VSYSMIN_SHIFT;

	ret = bq25790_update_byte_bits(bq, BQ25790_MINI_SYS_VOLT,
				BQ25790_VSYSMIN_MASK, reg_val);

	return ret;
}

static int bq25790_set_adc_simple(struct bq25790 *bq, int simple)
{
	int ret;

	ret = bq25790_update_byte_bits(bq, BQ25790_ADC_CONTROL_REG,
				BQ25790_ADC_SAMPLE_MASK, simple << BQ25790_ADC_SAMPLE_SHFIT);

	return ret;
}

#if 0
static int bq25790_set_topoff_timer(struct bq25790 *bq, int time)
{
	int ret;
	u8 reg_val;

	if (time == 0)
		reg_val = BQ25790_TOPOFF_TIMER_DISABLE;
	else if (time == 15)
		reg_val = BQ25790_TOPOFF_TIMER_15M;
	else if (time == 30)
		reg_val = BQ25790_TOPOFF_TIMER_30M;
	else
		reg_val = BQ25790_TOPOFF_TIMER_45M;

	reg_val <<= BQ25790_TOPOFF_TIMER_SHIFT;

	ret = bq25790_update_byte_bits(bq, BQ25790_REG_CHG_CTRL3,
				BQ25790_TOPOFF_TIMER_MASK, reg_val);

	return ret;
}

static int bq25790_get_ico_limit(struct bq25790 *bq, int *curr)
{
	int ret;
	u8 reg_val;

	ret = bq25790_read_reg(bq, BQ25790_REG_ICO_LIMIT, &reg_val);
	if (ret)
		return ret;

	*curr = reg_val & BQ25790_ICO_ILIM_MASK;
	*curr >>= BQ25790_ICO_ILIM_SHIFT;
	*curr *= BQ25790_ICO_ILIM_LSB;
	*curr += BQ25790_ICO_ILIM_BASE;

	return 0;
}
#endif

static int bq25790_set_adc_enable(struct bq25790 *bq, int enable)
{
	int ret;

	ret = bq25790_update_byte_bits(bq, BQ25790_REG_ADC_CTRL_REG,
			BQ25790_ADC_EN_MASK, enable << BQ25790_ADC_EN_SHIFT);
	if (ret)
		bq_dbg(PR_OEM, "failed to set adc enable\n");

	return ret;
}

static int bq25790_set_adc_dis_mask(struct bq25790 *bq)
{
	int ret;
	u8 reg_val;

	ret = bq25790_update_byte_bits(bq, BQ25790_REG_ADC_CTRL_REG,
			BQ25790_ADC_EN_MASK, BQ25790_ADC_EN << BQ25790_ADC_EN_SHIFT);
	if (ret)
		bq_dbg(PR_OEM, "failed to set adc enable\n");

	reg_val = TS_ADC_DIS | TDIE_ADC_DIS;

	ret = bq25790_update_byte_bits(bq, BQ25790_REG_ADC_DISABLE_REG,
			0xFF, reg_val);
	if (ret)
		bq_dbg(PR_OEM, "failed to set adc enable\n");

	return ret;

}

static int bq25790_set_int_mask(struct bq25790 *bq)
{
	int ret;
	u8 reg_val;

	reg_val = AC1_PRESENT_MASK | AC2_PRESENT_MASK | VINDPM_MASK |
		IINDPM_MASK;
	ret = bq25790_update_byte_bits(bq, BQ25790_REG_CHG_INT_MASK0, 0xFF, reg_val);
	if (ret)
		return ret;

	reg_val = BC12_DONE_MASK;
	ret = bq25790_update_byte_bits(bq, BQ25790_REG_CHG_INT_MASK1, 0xFF, reg_val);
	if (ret)
		return ret;

	reg_val = TOPOFF_TMR_MASK | PRECHG_TMR_MASK | TRICHG_TMR_MASK |
		VSYS_MASK | ADC_DONE_MASK | DPDM_DONE_MASK;
	ret = bq25790_update_byte_bits(bq, BQ25790_REG_CHG_INT_MASK2, 0xFF, reg_val);
	if (ret)
		return ret;

	reg_val = TS_HOT_MASK | TS_WARM_MASK | TS_COOL_MASK | TS_COLD_MASK |
		VBATOTG_LOW_MASK;
	ret = bq25790_update_byte_bits(bq, BQ25790_REG_CHG_INT_MASK2, 0xFF, reg_val);
	if (ret)
		return ret;

	return ret;
}

static int bq25790_set_fault_mask(struct bq25790 *bq)
{
	int ret;
	u8 reg_val;

	reg_val = VAC1_OVP_MASK | VAC2_OVP_MASK | CONV_OCP_MASK;
	ret = bq25790_update_byte_bits(bq, BQ25790_REG_CHG_FAULT_MASK0, 0xFF, reg_val);
	if (ret)
		return ret;

	reg_val = TSHUT_MASK | OTG_UVP_MASK | OTG_OVP_MASK;
	ret = bq25790_update_byte_bits(bq, BQ25790_REG_CHG_FAULT_MASK1, 0xFF, reg_val);
	if (ret)
		return ret;

	return ret;
}

#define ADC_RES_REG_BASE	0x31
static int bq25790_read_adc_data(struct bq25790 *bq, u8 channel, int *val)
{
	int ret;
	u16 res;
	u8 reg;

	if (channel >= ADC_MAX_NUM)
		return -EINVAL;
	reg = ADC_RES_REG_BASE + (channel << 1);

	ret = bq25790_read_word(bq, reg, &res);
	if (ret >= 0)
		*val = (int)res;

	return ret;
}

static int bq25790_read_bus_volt(struct bq25790 *bq, int *volt)
{
	int ret;
	int reg_val;

	ret = bq25790_read_adc_data(bq, ADC_VBUS, &reg_val);

	if (ret >= 0) {
		*volt = reg_val * BQ25790_VBUS_ADC_LB_LSB;
	}

	*volt *= 1000;
	return ret;
}

static int bq25790_read_bus_curr(struct bq25790 *bq, int *curr)
{
	int ret;
	int reg_val;

	ret = bq25790_read_adc_data(bq, ADC_IBUS, &reg_val);

	if (ret >= 0) {
		*curr = reg_val * BQ25790_IBUS_ADC_LB_LSB;
	}
	*curr *= 1000;

	return ret;
}

static int bq25790_read_bat_volt(struct bq25790 *bq, int *volt)
{
	int ret;
	int reg_val;

	ret = bq25790_read_adc_data(bq, ADC_VBAT, &reg_val);

	if (ret >= 0) {
		*volt = reg_val * BQ25790_VBAT_ADC_LB_LSB;
	}
	*volt *= 1000;

	return ret;
}

static int bq25790_read_bat_curr(struct bq25790 *bq, int *curr)
{
	int ret;
	s16 reg_val;

	ret = bq25790_read_adc_data(bq, ADC_IBAT, curr);
	reg_val = *curr;

	if (ret >= 0) {
		*curr = reg_val * BQ25790_ICHG_ADC_LB_LSB * (-1);
	}
	*curr *= 1000;

	return ret;
}

#if 0
static int bq25790_read_sys_volt(struct bq25790 *bq, int *volt)
{
	int ret;
	int reg_val;

	ret = bq25790_read_adc_data(bq, ADC_VSYS, &reg_val);

	if (ret >= 0) {
		*volt = reg_val * BQ25790_VSYS_ADC_LB_LSB;
		*volt += BQ25790_VSYS_ADC_LB_BASE;
	}

	return ret;
}

static int bq25790_read_die_temp(struct bq25790 *bq, int *temp)
{
	int ret;
	int reg_val;

	ret = bq25790_read_adc_data(bq, ADC_TDIE, &reg_val);

	if (ret >= 0) {
		*temp = reg_val * BQ25790_TDIE_ADC_LB_LSB;
	}

	return ret;
}
#endif

static struct power_supply *get_bms_psy(struct bq25790 *bq)
{
	if (bq->bms_psy)
		return bq->bms_psy;

	bq->bms_psy = power_supply_get_by_name("bms");
	if (!bq->bms_psy)
		bq_dbg(PR_DEBUG, "bms power supply not found\n");

	return bq->bms_psy;
}

static int bq25790_get_property_from_bms(struct bq25790 *bq,
				enum power_supply_property psp,
				union power_supply_propval *val)
{
	struct power_supply *bms_psy = get_bms_psy(bq);

	int ret;

	if (!bms_psy)
		return -EINVAL;

	ret = power_supply_get_property(bms_psy, psp, val);
	if (ret < 0)
		val->intval = 0;

	return ret;
}

static inline bool is_device_suspended(struct bq25790 *bq);
static int bq25790_get_prop_charge_type(struct bq25790 *bq)
{
	union power_supply_propval batt_prop = {0,};
	u8 val = 0;

	if (is_device_suspended(bq))
		return POWER_SUPPLY_CHARGE_TYPE_UNKNOWN;

	bq25790_read_reg(bq, BQ25790_REG_CHG_STATUS1, &val);
	val &= BQ25790_CHRG_STAT_MASK;
	val >>= BQ25790_CHRG_STAT_SHIFT;

	if (!bq->cp_disable_votable)
		bq->cp_disable_votable = find_votable("CP_DISABLE");

	if (!bq->passthrough_dis_votable)
		bq->passthrough_dis_votable = find_votable("PASSTHROUGH");

	bq25790_get_property_from_bms(bq,
			POWER_SUPPLY_PROP_VOLTAGE_NOW, &batt_prop);

	/*when cp is enabled, */
	if (batt_prop.intval > BQ25790_FFC_TAPER_FV &&
			(!get_effective_result(bq->cp_disable_votable) ||
			 !get_effective_result(bq->passthrough_dis_votable))) {
		return POWER_SUPPLY_CHARGE_TYPE_TAPER;
	}

	switch (val) {
	case BQ25790_CHRG_STAT_FAST:
		return POWER_SUPPLY_CHARGE_TYPE_FAST;
	case BQ25790_CHRG_STAT_PRECHG:
		return POWER_SUPPLY_CHARGE_TYPE_TRICKLE;
	case BQ25790_CHRG_STAT_DONE:
		return POWER_SUPPLY_CHARGE_TYPE_FAST;
	case BQ25790_CHRG_STAT_NOT_CHARGING:
		return POWER_SUPPLY_CHARGE_TYPE_NONE;
	case BQ25790_CHRG_STAT_TAPER:
		if (batt_prop.intval > BQ25790_FFC_TAPER_FV &&
			(!get_effective_result(bq->cp_disable_votable) ||
			 !get_effective_result(bq->passthrough_dis_votable))) {
			return POWER_SUPPLY_CHARGE_TYPE_TAPER;
		} else
			return POWER_SUPPLY_CHARGE_TYPE_FAST;
	default:
		return POWER_SUPPLY_CHARGE_TYPE_UNKNOWN;
	}
}

static int bq25790_get_prop_charge_status(struct bq25790 *bq)
{
	union power_supply_propval batt_prop = {0,};
	int ret;
	u8 status;

	ret = bq25790_get_property_from_bms(bq,
			POWER_SUPPLY_PROP_STATUS, &batt_prop);
	if (!ret && batt_prop.intval == POWER_SUPPLY_STATUS_FULL && bq->usb_present) {
		bq->charge_done = true;
		return POWER_SUPPLY_STATUS_FULL;
	}
	bq->charge_done = false;

	ret = bq25790_read_reg(bq, BQ25790_REG_CHG_STATUS1, &status);
	if (ret)
		return POWER_SUPPLY_STATUS_UNKNOWN;

	mutex_lock(&bq->data_lock);
	bq->charge_state = (status & BQ25790_CHRG_STAT_MASK) >> BQ25790_CHRG_STAT_SHIFT;
	mutex_unlock(&bq->data_lock);

	switch (bq->charge_state) {
	case BQ25790_CHRG_STAT_TRICKLE:
	case BQ25790_CHRG_STAT_PRECHG:
	case BQ25790_CHRG_STAT_FAST:
	case BQ25790_CHRG_STAT_TAPER:
		return POWER_SUPPLY_STATUS_CHARGING;
	case BQ25790_CHRG_STAT_DONE:
		bq->charge_done = true;
		return POWER_SUPPLY_STATUS_FULL;
	case BQ25790_CHRG_STAT_NOT_CHARGING:
		return POWER_SUPPLY_STATUS_DISCHARGING;
	default:
		return POWER_SUPPLY_STATUS_UNKNOWN;
	}
}

static int bq25790_get_prop_health(struct bq25790 *bq)
{
	int ret = 0;
	union power_supply_propval batt_prop = {0,};

	ret = bq25790_get_property_from_bms(bq,
			POWER_SUPPLY_PROP_HEALTH, &batt_prop);
	if (!ret)
		ret = batt_prop.intval;
	else
		ret = POWER_SUPPLY_HEALTH_UNKNOWN;

	return ret;
}


static enum power_supply_property bq25790_charger_props[] = {
	POWER_SUPPLY_PROP_CHARGE_TYPE,
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_CHIP_OK,
	POWER_SUPPLY_PROP_CHARGING_ENABLED,
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_HEALTH,
	POWER_SUPPLY_PROP_CURRENT_NOW,
	POWER_SUPPLY_PROP_USB_CURRENT_NOW,
	POWER_SUPPLY_PROP_USB_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT,
	POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE,
	POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT,
	POWER_SUPPLY_PROP_HIZ_MODE,
	POWER_SUPPLY_PROP_CHARGE_DONE,
	POWER_SUPPLY_PROP_VOLTAGE_MAX,
	POWER_SUPPLY_PROP_CURRENT_MAX,
	POWER_SUPPLY_PROP_MAIN_FCC_MAX,
	POWER_SUPPLY_PROP_ARTI_VBUS_ENABLE,
	POWER_SUPPLY_PROP_TERMINATION_CURRENT,
	POWER_SUPPLY_PROP_INPUT_VOLTAGE_REGULATION,
	POWER_SUPPLY_PROP_FORCE_MAIN_ICL,
	POWER_SUPPLY_PROP_RECHARGE_VBAT,
};

static int bq25790_charger_get_property(struct power_supply *psy,
				enum power_supply_property psp,
				union power_supply_propval *val)
{

	struct bq25790 *bq = power_supply_get_drvdata(psy);

	switch (psp) {
	case POWER_SUPPLY_PROP_CHARGE_TYPE:
		val->intval = bq25790_get_prop_charge_type(bq);
		bq_dbg(PR_DEBUG, "POWER_SUPPLY_PROP_CHARGE_TYPE:%d\n", val->intval);
		break;
	case POWER_SUPPLY_PROP_PRESENT:
		val->intval = bq->usb_present;
		break;
	case POWER_SUPPLY_PROP_ONLINE:
		val->intval = (bq->usb_present && !bq->hiz_mode);
		break;
	case POWER_SUPPLY_PROP_CHIP_OK:
		val->intval = bq->chip_ok;
		break;
	case POWER_SUPPLY_PROP_CHARGING_ENABLED:
		val->intval = bq->charge_enabled;
		break;
	case POWER_SUPPLY_PROP_STATUS:
		val->intval = bq25790_get_prop_charge_status(bq);
		break;
	case POWER_SUPPLY_PROP_HEALTH:
		val->intval = bq25790_get_prop_health(bq);
		break;
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		bq25790_read_bat_curr(bq, &val->intval);
		bq_dbg(PR_DEBUG, "POWER_SUPPLY_PROP_CURRENT_NOW: %d\n", val->intval);
		break;
	case POWER_SUPPLY_PROP_USB_CURRENT_NOW:
		bq25790_read_bus_curr(bq, &val->intval);
		bq_dbg(PR_DEBUG, "POWER_SUPPLY_PROP_USB_CURRENT_NOW: %d\n", val->intval);
		break;
	case POWER_SUPPLY_PROP_USB_VOLTAGE_NOW:
		bq25790_read_bus_volt(bq, &val->intval);
		bq_dbg(PR_DEBUG, "POWER_SUPPLY_PROP_USB_VOLTAGE_NOW: %d\n", val->intval);
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		bq25790_read_bat_volt(bq, &val->intval);
		bq_dbg(PR_DEBUG, "POWER_SUPPLY_PROP_VOLTAGE_NOW: %d\n", val->intval);
		break;
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT:
		bq25790_get_charge_current(bq, &val->intval);
		bq_dbg(PR_DEBUG, "POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT: %d\n", val->intval);
		break;
	case POWER_SUPPLY_PROP_HIZ_MODE:
		bq25790_get_hiz_mode(bq, &val->intval);
		break;
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE:
		bq25790_get_charge_voltage(bq, &val->intval);
		bq_dbg(PR_DEBUG, "POWER_SUPPLY_PROP_CHARGE_VOLTAGE: %d\n", val->intval);
		break;
	case POWER_SUPPLY_PROP_CHARGE_DONE:
		val->intval = bq->charge_done;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_MAX:
	case POWER_SUPPLY_PROP_CURRENT_MAX:
		bq25790_get_property_from_bms(bq, psp, val);
		break;
	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
		bq25790_get_input_current_limit(bq, &val->intval);
		bq_dbg(PR_DEBUG, "POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT: %d\n",
					val->intval);
		break;
	case POWER_SUPPLY_PROP_MAIN_FCC_MAX:
		val->intval = 3000000;
		break;
	case POWER_SUPPLY_PROP_ARTI_VBUS_ENABLE:
		val->intval = bq->arti_vbus_enable;
		break;
	case POWER_SUPPLY_PROP_TERMINATION_CURRENT:
		val->intval = bq->termi_curr;
		break;
	case POWER_SUPPLY_PROP_INPUT_VOLTAGE_REGULATION:
		bq25790_get_input_volt_limit(bq, &val->intval);
		break;
	case POWER_SUPPLY_PROP_FORCE_MAIN_ICL:
		bq25790_get_input_current_limit(bq, &val->intval);
		bq_dbg(PR_DEBUG, "POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT: %d\n",
					val->intval);
		break;
	case POWER_SUPPLY_PROP_RECHARGE_VBAT:
		val->intval = bq->rechg_vol;
		break;
	default:
		return -EINVAL;

	}

	return 0;
}

static int bq25790_charger_set_property(struct power_supply *psy,
				       enum power_supply_property prop,
				       const union power_supply_propval *val)
{
	struct bq25790 *bq = power_supply_get_drvdata(psy);

	switch (prop) {
	case POWER_SUPPLY_PROP_CHARGING_ENABLED:
		vote(bq->chg_dis_votable, BQ25790_USER_VOTER,
				!val->intval, !val->intval);
		bq_dbg(PR_DEBUG, "POWER_SUPPLY_PROP_CHARGING_ENABLED: %s\n",
					val->intval ? "enable" : "disable");
		break;
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT:
		bq_dbg(PR_DEBUG, "POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT: %d\n",
					val->intval);
		vote(bq->fcc_votable, BQ25790_PROP_VOTER,
				val->intval > 0 ? true : false, val->intval);
	     break;
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE:
		bq_dbg(PR_DEBUG, "POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE: %d\n",
					val->intval);
		vote(bq->fv_votable, BQ25790_PROP_VOTER,
				val->intval > 0 ? true : false, val->intval);
	     break;
	case POWER_SUPPLY_PROP_HIZ_MODE:
		bq_dbg(PR_DEBUG, "POWER_SUPPLY_PROP_HIZ_MODE: %d\n",
					val->intval);
		bq25790_set_hiz_mode(bq, val->intval);
		break;
	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
		bq_dbg(PR_DEBUG, "POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT: %d\n",
					val->intval);
		vote(bq->ilim_votable, BQ25790_PROP_VOTER,
				val->intval > 0 ? true : false, val->intval);
		break;
	case POWER_SUPPLY_PROP_ARTI_VBUS_ENABLE:
		vote(bq->arti_vbus_dis_votable, BQ25790_PROP_VOTER,
				val->intval == 0 ? true : false, 0);
		break;
	case POWER_SUPPLY_PROP_TERMINATION_CURRENT:
		bq25790_set_term_current(bq, val->intval);
		break;
	case POWER_SUPPLY_PROP_INPUT_VOLTAGE_REGULATION:
		bq25790_set_input_volt_limit(bq, val->intval);
		break;
	case POWER_SUPPLY_PROP_FORCE_MAIN_ICL:
		bq_dbg(PR_DEBUG, "POWER_SUPPLY_PROP_FORCE_MAIN_ICL: %d\n",
					val->intval);
		vote_override(bq->ilim_votable, BQ25790_PROP_VOTER,
				val->intval > 0 ? true : false, val->intval);
		break;
	case POWER_SUPPLY_PROP_RECHARGE_VBAT:
		bq_dbg(PR_DEBUG, "POWER_SUPPLY_PROP_RECHARGE_VBAT: %d\n",
					val->intval);
		bq25790_set_vrechg_voltage(bq, val->intval);
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int bq25790_charger_is_writeable(struct power_supply *psy,
				       enum power_supply_property prop)
{
	int ret = 0;

	switch (prop) {
	case POWER_SUPPLY_PROP_CHARGING_ENABLED:
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT:
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE:
	case POWER_SUPPLY_PROP_HIZ_MODE:
	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
	case POWER_SUPPLY_PROP_ARTI_VBUS_ENABLE:
	case POWER_SUPPLY_PROP_TERMINATION_CURRENT:
	case POWER_SUPPLY_PROP_INPUT_VOLTAGE_REGULATION:
	case POWER_SUPPLY_PROP_FORCE_MAIN_ICL:
	case POWER_SUPPLY_PROP_RECHARGE_VBAT:
		ret = 1;
		break;
	default:
		ret = 0;
		break;
	}
	return ret;
}

static int bq25790_update_charging_profile(struct bq25790 *bq)
{
	int ret;
	int iindpm, fcc;
	union power_supply_propval prop = {0,};


	if (!bq->usb_present || !bq->usb_psy)
		return 0;

	ret = power_supply_get_property(bq->usb_psy,
				POWER_SUPPLY_PROP_REAL_TYPE, &prop);
	if (ret < 0) {
		bq_dbg(PR_OEM, "couldn't read USB TYPE property, ret=%d\n", ret);
		return ret;
	}
	bq->charger_type = prop.intval;

	mutex_lock(&bq->profile_change_lock);
	switch (bq->charger_type) {
	case POWER_SUPPLY_TYPE_USB_PD:
		bq25790_set_input_volt_limit(bq, BQ25790_SDP_VINDPM_MV);
		iindpm = BQ25790_PPS_IINDPM_MA;
		fcc = BQ25790_PPS_FCC;
		break;
	case POWER_SUPPLY_TYPE_USB_HVDCP:
		iindpm = BQ25790_HVDCP_2_IINDPM_MA;
		fcc = BQ25790_HVDCP_2_FCC;
		bq25790_set_input_volt_limit(bq, BQ25790_SDP_VINDPM_MV);
		break;
	case POWER_SUPPLY_TYPE_USB_HVDCP_3:
		if (bq->hvdcp_class == HVDCP3_CLASSB_27W) {
			iindpm = BQ25790_HVDCP_B_IINDPM_MA;
			fcc = BQ25790_HVDCP_B_FCC;
			bq25790_set_input_volt_limit(bq, BQ25790_HVDCP_B_VINDPM_MV);
		} else {
			iindpm = BQ25790_HVDCP_A_IINDPM_MA;
			fcc = BQ25790_HVDCP_A_FCC;
			bq25790_set_input_volt_limit(bq, BQ25790_HVDCP_A_VINDPM_MV);
		}
		break;
	case POWER_SUPPLY_TYPE_USB_DCP:
	case POWER_SUPPLY_TYPE_USB_FLOAT:
		bq25790_set_input_volt_limit(bq, BQ25790_SDP_VINDPM_MV);
		iindpm = BQ25790_DCP_IINDPM_MA;
		fcc = BQ25790_DCP_FCC;
		break;
	case POWER_SUPPLY_TYPE_USB_CDP:
		bq25790_set_input_volt_limit(bq, BQ25790_SDP_VINDPM_MV);
		iindpm = BQ25790_CDP_IINDPM_MA;
		fcc = BQ25790_CDP_FCC;
		break;
	case POWER_SUPPLY_TYPE_USB:
		bq25790_set_input_volt_limit(bq, BQ25790_SDP_VINDPM_MV);
		iindpm = BQ25790_SDP_IINDPM_MA;
		fcc = BQ25790_SDP_FCC;
		break;
	case POWER_SUPPLY_TYPE_WIRELESS:
		iindpm = BQ25790_WLS_IINDPM_MA;
		fcc = BQ25790_WLS_FCC;
		break;
	default:
		bq25790_set_input_volt_limit(bq, BQ25790_SDP_VINDPM_MV);
		iindpm = BQ25790_DCP_IINDPM_MA;
		fcc = BQ25790_DCP_FCC;
		break;

	}

	bq_dbg(PR_OEM, "charge_type:%d, iindpm:%d, fcc:%d\n", bq->charger_type, iindpm, fcc);

	vote(bq->ilim_votable, BQ25790_INIT_VOTER, true, iindpm);
	vote(bq->fcc_votable, BQ25790_INIT_VOTER, true, fcc);

	rerun_election(bq->fcc_votable);
	rerun_election(bq->ilim_votable);
	rerun_election(bq->fv_votable);
	rerun_election(bq->chg_dis_votable);

	bq25790_set_en_extilim(bq, false);

	mutex_unlock(&bq->profile_change_lock);
	power_supply_changed(bq->bbc_psy);

	return 0;
}

static int bq25790_psy_register(struct bq25790 *bq)
{
	int ret;
	struct power_supply_config bbc_psy_cfg = {};

	bq->bbc_psy_d.name = "bbc";
	bq->bbc_psy_d.type = POWER_SUPPLY_TYPE_PARALLEL;
	bq->bbc_psy_d.properties = bq25790_charger_props;
	bq->bbc_psy_d.num_properties = ARRAY_SIZE(bq25790_charger_props);
	bq->bbc_psy_d.get_property = bq25790_charger_get_property;
	bq->bbc_psy_d.set_property = bq25790_charger_set_property;
	bq->bbc_psy_d.property_is_writeable = bq25790_charger_is_writeable;

	bbc_psy_cfg.drv_data = bq;
	bbc_psy_cfg.num_supplicants = 0;

	bq->bbc_psy = power_supply_register(bq->dev,
				&bq->bbc_psy_d,
				&bbc_psy_cfg);
	if (IS_ERR(bq->bbc_psy)) {
		bq_dbg(PR_OEM, "couldn't register battery psy, ret = %ld\n",
				PTR_ERR(bq->bbc_psy));
		return ret;
	}

	return 0;
}

static void bq25790_psy_unregister(struct bq25790 *bq)
{
	power_supply_unregister(bq->bbc_psy);
}


static struct bq25790_platform_data *bq25790_parse_dt(struct i2c_client *client,
						struct bq25790 *bq)
{
	int ret;
	struct device_node *np = client->dev.of_node;
	struct bq25790_platform_data *pdata;
	int irq_gpio;

	pdata = devm_kzalloc(&client->dev, sizeof(struct bq25790_platform_data),
						GFP_KERNEL);
	if (!pdata)
		return NULL;

	ret = of_property_read_u32(np, "ti,bq25790,precharge-current", &pdata->iprechg);
	if (ret)
		bq_dbg(PR_OEM, "Failed to read node of ti,bq25790,precharge-current\n");

	ret = of_property_read_u32(np, "ti,bq25790,termi-curr", &pdata->iterm);
	if (ret)
		bq_dbg(PR_OEM, "Failed to read node of ti,bq25790,termi-curr\n");

	ret = of_property_read_u32(np, "ti,bq25790,safe_timer_en", &pdata->safe_timer_en);
	if (ret)
		bq_dbg(PR_OEM, "Failed to read node of ti,bq25790,safe_timer_en\n");

	ret = of_property_read_u32(np, "ti,bq25790,safe_timer", &pdata->safe_timer);
	if (ret)
		bq_dbg(PR_OEM, "Failed to read node of ti,bq25790,safe_timer\n");

	ret = of_property_read_u32(np, "ti,bq25790,presafe_timer", &pdata->presafe_timer);
	if (ret)
		bq_dbg(PR_OEM, "Failed to read node of ti,bq25790,presafe_timer\n");

	ret = of_property_read_u32(np, "ti,bq25790,vac_ovp", &pdata->vac_ovp);
	if (ret)
		bq_dbg(PR_OEM, "Failed to read node of ti,bq25790,vac_ovp\n");

	ret = of_property_read_u32(np, "ti,bq25790,cell_num", &pdata->cell_num);
	if (ret)
		bq_dbg(PR_OEM, "Failed to read node of ti,bq25790,cell_num\n");

	ret = of_property_read_u32(np, "ti,bq25790,vsys_min", &pdata->vsys_min);
	if (ret)
		bq_dbg(PR_OEM, "Failed to read node of ti,vsys_min\n");

	irq_gpio = of_get_named_gpio(np, "ti,bq25790,irq", 0);
	if ((!gpio_is_valid(irq_gpio))) {
		bq_dbg(PR_OEM, "Failed to read node of ti,bq25790,boost-current\n");

	} else {
		ret = gpio_request(irq_gpio, "bq25790_irq_gpio");
		if (ret) {
			bq_dbg(PR_OEM, "%s: unable to request bq25790 irq gpio [%d]\n",
					__func__, irq_gpio);
		}
		ret = gpio_direction_input(irq_gpio);
		if (ret) {
			bq_dbg(PR_OEM, "%s: unable to set direction for bq25790 irq gpio [%d]\n",
					__func__, irq_gpio);
		}
		client->irq = gpio_to_irq(irq_gpio);
	}

	bq->arti_vbus_gpio = of_get_named_gpio(np, "ti,bq25790,arti_vbus", 0);
	if ((!gpio_is_valid(bq->arti_vbus_gpio))) {
		bq_dbg(PR_OEM, "Failed to read node of ti,bq25790,boost-current\n");

	} else {
		ret = gpio_request(bq->arti_vbus_gpio, "bq25790_arti_gpio");
		if (ret) {
			bq_dbg(PR_OEM, "%s: unable to request bq25790 vbus gpio [%d]\n",
					__func__, bq->arti_vbus_gpio);
		}
		ret = gpio_direction_output(bq->arti_vbus_gpio, 0);
		if (ret) {
			bq_dbg(PR_OEM, "%s: unable to set direction for bq25790 arti vbus gpio [%d]\n",
					__func__, bq->arti_vbus_gpio);
		}
	}

	bq->reverse_gpio = of_get_named_gpio(np, "ti,bq25790,reverse", 0);
	if ((!gpio_is_valid(bq->reverse_gpio))) {
		bq_dbg(PR_OEM, "Failed to read node of reverse\n");

	} else {
		ret = gpio_request(bq->reverse_gpio, "bq25790_reverse_gpio");
		if (ret) {
			bq_dbg(PR_OEM, "%s: unable to request bq25790 reverse gpio [%d]\n",
					__func__, bq->reverse_gpio);
		}
		ret = gpio_direction_output(bq->reverse_gpio, 0);
		if (ret) {
			bq_dbg(PR_OEM, "%s: unable to set direction for bq25790 reverse gpio [%d]\n",
					__func__, bq->reverse_gpio);
		}
	}
	bq_dbg(PR_OEM, "irq_gpio:%d, arti_vbus_gpio:%d, reverse:%d\n", irq_gpio, bq->arti_vbus_gpio, bq->reverse_gpio);

	return pdata;
}

static int bq25790_init_device(struct bq25790 *bq)
{
	int ret;

	bq25790_set_wdt_timer(bq, 80);
	bq25790_reset_wdt(bq);

	ret = bq25790_set_prechg_current(bq, bq->platform_data->iprechg);
	if (ret)
		bq_dbg(PR_OEM, "Failed to set prechg current, ret = %d\n", ret);

	ret = bq25790_set_term_current(bq, bq->platform_data->iterm);
	if (ret)
		bq_dbg(PR_OEM, "Failed to set termination current, ret = %d\n", ret);

	ret = bq25790_enable_safety_timer(bq, bq->platform_data->safe_timer_en);
	if (ret)
		bq_dbg(PR_OEM, "Failed to enable safety timer, ret = %d\n", ret);

	ret = bq25790_set_safety_timer(bq, bq->platform_data->safe_timer);
	if (ret)
		bq_dbg(PR_OEM, "Failed to set safety timer, ret = %d\n", ret);

	ret = bq25790_set_pre_safety_timer(bq, bq->platform_data->presafe_timer);
	if (ret)
		bq_dbg(PR_OEM, "Failed to set safety timer, ret = %d\n", ret);

	ret = bq25790_set_vac_ovp(bq, bq->platform_data->vac_ovp);
	if (ret)
		bq_dbg(PR_OEM, "Failed to set vac ovp, ret = %d\n", ret);

	ret = bq25790_set_cell_num(bq, bq->platform_data->cell_num);
	if (ret)
		bq_dbg(PR_OEM, "Failed to set cell number, ret = %d\n", ret);

	ret = bq25790_set_vsys_min_volt(bq, bq->platform_data->vsys_min);
	if (ret)
		bq_dbg(PR_OEM, "Failed to set vsysmin, ret = %d\n", ret);

	bq25790_set_adc_simple(bq, BQ25790_ADC_SAMPLE_13BIT);
	bq25790_set_adc_dis_mask(bq);
	bq25790_set_int_mask(bq);
	bq25790_set_fault_mask(bq);
	bq25790_set_ico_mode(bq, false);
	bq25790_set_fwd_ooa(bq, false);
	bq25790_set_dpdm_indet(bq, false);
	bq25790_set_pfm_fwd(bq, false);
	bq25790_set_term_enable(bq, false);

	return 0;
}

static int bq25790_detect_device(struct bq25790 *bq)
{
	int ret;
	u8 data;

	ret = bq25790_read_reg(bq, BQ25790_REG_PART_NUM, &data);

	if (ret == 0) {
		bq->part_no = data & BQ25790_PART_NO_MASK;
		bq->part_no >>= BQ25790_PART_NO_SHIFT;
		bq->revision = data & BQ25790_REVISION_MASK;
		bq->revision >>= BQ25790_REVISION_SHIFT;
		bq->chip_ok = true;
	}
	bq_dbg(PR_OEM, "%s:BQ25790_REG_PART_NUM:0x%x bq->part_no:0x%x bq->revision:%x",
			__func__, data, bq->part_no, bq->revision);

	return ret;
}

static void bq25790_update_status(struct bq25790 *bq)
{

	bq25790_read_bus_volt(bq, &bq->vbus_volt);
	bq25790_read_bat_volt(bq, &bq->vbat_volt);
	bq25790_read_bus_curr(bq, &bq->ibus_curr);
	bq25790_read_bat_curr(bq, &bq->ichg_curr);


	bq_dbg(PR_OEM, "vbus:%d,vbat:%d, ibus:%d,ichg:%d\n",
		bq->vbus_volt, bq->vbat_volt,
		bq->ibus_curr, bq->ichg_curr);

}

static void bq25790_fv_check(struct bq25790 *bq)
{
	int vcell, fv, fv_max, status;
	union power_supply_propval prop = {0,};

	bq25790_get_property_from_bms(bq,
			POWER_SUPPLY_PROP_VOLTAGE_NOW, &prop);
	vcell= prop.intval;

	bq->ffc_mode_disable = find_votable("FFC_MODE_DIS");
	if (get_effective_result(bq->ffc_mode_disable)) {
		fv_max = BQ25790_FV + 5000;
	} else
		fv_max = BQ25790_FFC_FV + 5000;

	status = bq25790_get_prop_charge_status(bq);
	if ((status == POWER_SUPPLY_STATUS_CHARGING) && (vcell > fv_max) &&
			get_effective_result(bq->cp_disable_votable)) {
		bq_dbg(PR_OEM, "vcell:%d is high, reduce fv:%d\n", vcell,
				get_effective_result(bq->fv_votable) - 10000);
		fv = get_effective_result(bq->fv_votable) - 10000;
		vote(bq->fv_votable, BQ25790_FULL_CHK_VOTER, true, fv);
	}

	bq_dbg(PR_OEM, "vcell:%d, fv:%d, fv_max:%d\n",
		vcell, get_effective_result(bq->fv_votable), fv_max);
}

static void bq25790_status_change_work(struct work_struct *work)
{
	struct bq25790 *bq = container_of(work,
			struct bq25790, charge_status_change_work.work);
	union power_supply_propval prop = {0,};
	int charger_type, hvdcp_class = 0;
	int ret;

	if (!bq->usb_psy)
		return;

	ret = power_supply_get_property(bq->usb_psy,
			POWER_SUPPLY_PROP_REAL_TYPE, &prop);
	if (ret < 0) {
		bq_dbg(PR_OEM, "couldn't read USB TYPE property, ret=%d\n", ret);
		return;
	}
	charger_type = prop.intval;

	if (charger_type == POWER_SUPPLY_TYPE_USB_HVDCP_3) {
		ret = power_supply_get_property(bq->usb_psy,
				POWER_SUPPLY_PROP_HVDCP3_TYPE, &prop);
		if (ret < 0) {
			bq_dbg(PR_OEM, "couldn't read USB TYPE property, ret=%d\n", ret);
			return;
		}
		hvdcp_class = prop.intval;
	}

	if ((bq->charger_type != charger_type) || (bq->hvdcp_class != hvdcp_class)) {
		if (charger_type == POWER_SUPPLY_TYPE_USB_HVDCP_3)
			bq->hvdcp_class = hvdcp_class;
		bq25790_update_charging_profile(bq);
	}

	return;
}

static void bq25790_dump_irq(struct bq25790 *bq)
{
	u8 addr;
	u8 val;

	if (!bq->resume_completed)
		return;

	for (addr = 0x1b; addr <= 0x27; addr++) {
		bq25790_read_reg(bq, addr, &val);
		bq_dbg(PR_OEM, "status 0x%.2x=0x%.2x\n", addr, val);
	}
}


u8 bq25790_word_reg[15] = { 0x01, 0x03, 0x06, 0x31, 0x33, 0x35,
			    0x37, 0x39, 0x3b, 0x3d, 0x3f, 0x40,
			    0x41, 0x43, 0x45 };
static void bq25790_dump_reg(struct bq25790 *bq)
{
	int i;
	u8 addr;
	u8 val;
	u16 word_val;

	return;

	for (addr = 0x0; addr <= 0x48; addr++) {
		for (i = 0; i < 15; i++) {
			if (addr == bq25790_word_reg[i]) {
				bq25790_read_word(bq, addr, &word_val);
				bq_dbg(PR_OEM, "Reg0x%.2x=0x%.4x\n", addr, word_val);
				goto next;
			}
		}
		bq25790_read_reg(bq, addr, &val);
next:
		bq_dbg(PR_OEM, "Reg0x%.2x=0x%.2x\n", addr, val);
		continue;
	}
}

static void bq25790_charge_monitor_workfunc(struct work_struct *work)
{
	struct bq25790 *bq = container_of(work,
			struct bq25790, charge_monitor_work.work);

	bq25790_dump_reg(bq);
	bq25790_fv_check(bq);
	bq25790_update_status(bq);
	bq25790_reset_wdt(bq);

	schedule_delayed_work(&bq->charge_monitor_work, 6 * HZ);
}

static int bq25790_get_psy(struct bq25790 *bq)
{
	bq->usb_psy = power_supply_get_by_name("usb");
	if (!bq->usb_psy) {
		bq_dbg(PR_OEM, "USB supply not found, defer probe\n");
		return -EINVAL;
	}

	bq->bms_psy = power_supply_get_by_name("bms");
	if (!bq->bms_psy) {
		bq_dbg(PR_OEM, "bms supply not found, defer probe\n");
		return -EINVAL;
	}

	return 0;
}

static void bq25790_charge_irq_workfunc(struct work_struct *work)
{
	struct bq25790 *bq = container_of(work,
			struct bq25790, charge_irq_work.work);
	u8 status;
	int ret;
	union power_supply_propval prop = {0,};

	ret = bq25790_read_reg(bq, BQ25790_REG_CHG_STATUS0, &status);
	if (ret) {
		return;
	}
	bq25790_dump_irq(bq);

	mutex_lock(&bq->data_lock);
	bq->power_good = !!(status & BQ25790_PG_STAT_MASK);
	mutex_unlock(&bq->data_lock);

	if (!bq->power_good) {
		if (bq->usb_present) {
			bq->usb_present = false;
		}

		if (!bq->wls_psy)
			bq->wls_psy = power_supply_get_by_name("wireless");

		if (bq->wls_psy) {
			prop.intval = 0;
			bq_dbg(PR_OEM, "usb plug-out, notice wireless = %d\n", bq->usb_present);
			power_supply_set_property(bq->wls_psy, POWER_SUPPLY_PROP_DIV_2_MODE, &prop);
		}

		bq_dbg(PR_OEM, "usb plug-out, set usb present = %d\n", bq->usb_present);
		bq->charger_type = POWER_SUPPLY_TYPE_UNKNOWN;
		bq->hvdcp_class = HVDCP3_NONE;
		bq25790_set_revese_gpio(bq, false);
		vote(bq->arti_vbus_dis_votable, BQ25790_USER_VOTER, true, 0);
		vote(bq->awake_votable, BQ25790_USER_VOTER, false, 0);
		vote(bq->fv_votable, BQ25790_FULL_CHK_VOTER, false, 0);
		cancel_delayed_work_sync(&bq->charge_monitor_work);
	} else if (bq->power_good && !bq->usb_present) {
		bq->usb_present = true;
		bq_dbg(PR_OEM, "usb plug-in, set usb present = %d\n", bq->usb_present);
		ret = bq25790_init_device(bq);
		if (ret) {
			bq_dbg(PR_OEM, "Failed to init device\n");
			return;
		}
		vote(bq->arti_vbus_dis_votable, BQ25790_USER_VOTER, false, 0);
		vote(bq->awake_votable, BQ25790_USER_VOTER, true, 0);
		schedule_delayed_work(&bq->charge_monitor_work, 10 * HZ);
		bq_dbg(PR_OEM, "usb plugged in, set usb present = %d\n", bq->usb_present);
	}

	bq25790_update_status(bq);

	power_supply_changed(bq->bbc_psy);
	if (bq->usb_psy)
		power_supply_changed(bq->usb_psy);
}

static irqreturn_t bq25790_charger_interrupt(int irq, void *dev_id)
{
	struct bq25790 *bq = dev_id;

	mutex_lock(&bq->irq_complete);
	bq->irq_waiting = true;
	if (!bq->resume_completed) {
		dev_dbg(bq->dev, "IRQ triggered before device-resume\n");
		if (!bq->irq_disabled) {
			disable_irq_nosync(bq->client->irq);
			bq->irq_disabled = true;
		}
		mutex_unlock(&bq->irq_complete);
		return IRQ_HANDLED;
	}

	bq->irq_waiting = false;
	schedule_delayed_work(&bq->charge_irq_work, 0);
	mutex_unlock(&bq->irq_complete);

	return IRQ_HANDLED;
}


static void determine_initial_status(struct bq25790 *bq)
{
	bq25790_charger_interrupt(bq->client->irq, bq);
}

static int bq25790_fcc_vote_callback(struct votable *votable, void *data,
		int fcc_ua, const char *client)
{
	struct bq25790 *bq = data;
	int rc;

	if (fcc_ua < 0)
		return 0;

	if (fcc_ua > BQ25790_MAX_FCC)
		fcc_ua = BQ25790_MAX_FCC;

	bq_dbg(PR_OEM, "set fast charge current to :%d\n", fcc_ua);

	rc = bq25790_set_charge_current(bq, fcc_ua);
	if (rc < 0) {
		bq_dbg(PR_OEM, "failed to allocate register map\n");
		return rc;
	}

	return 0;
}

static int bq25790_fv_vote_callback(struct votable *votable, void *data,
		int fv_uv, const char *client)
{
	struct bq25790 *bq = data;
	int rc;

	if (fv_uv < 0)
		return 0;

	rc = bq25790_set_charge_voltage(bq, fv_uv);
	if (rc < 0) {
		bq_dbg(PR_OEM, "failed to allocate register map\n");
		return rc;
	}

	return 0;
}

static int bq25790_ilim_vote_callback(struct votable *votable, void *data,
		int icl_ua, const char *client)
{
	struct bq25790 *bq = data;
	int rc;

	if (icl_ua < 0)
		return 0;

	if (icl_ua > BQ25790_MAX_ICL)
		icl_ua = BQ25790_MAX_ICL;

	bq_dbg(PR_OEM, "set input curr to :%d\n", icl_ua);

	rc = bq25790_set_input_curr_limit(bq, icl_ua);
	if (rc < 0) {
		bq_dbg(PR_OEM, "failed to allocate register map\n");
		return rc;
	}

	return 0;
}

static int bq25790_awake_vote_cb(struct votable *votable,
		void *data, int awake, const char *client)
{
	struct bq25790 *bq = (struct bq25790 *)data;

	if (awake)
		pm_stay_awake(bq->dev);
	else
		pm_relax(bq->dev);

	return 0;
}

static int bq25790_chg_disable_vote_callback(struct votable *votable,
		void *data, int disable, const char *client)
{
	struct bq25790 *bq = (struct bq25790 *)data;

	bq25790_charge_enable(bq, !disable);

	bq->charge_enabled = !disable;
	bq_dbg(PR_OEM, "client have set charging to %d\n", !disable);

	vote(bq->cp_disable_votable, BQ25790_USER_VOTER, disable, 0);

	power_supply_changed(bq->bbc_psy);
	if (bq->usb_psy)
		power_supply_changed(bq->usb_psy);

	return 0;
}

static int bq25790_arti_vbus_dis_vote_callback(struct votable *votable, void *data,
			int disable, const char *client)

{
	struct bq25790 *bq = (struct bq25790 *)data;

	bq25790_set_arti_vbus_disable(bq, disable);

	return 0;
}

static int bq25790_create_votable(struct bq25790 *bq)
{
	int rc;

	bq->fcc_votable = create_votable("BBC_FCC", VOTE_MIN,
			bq25790_fcc_vote_callback,
			bq);
	if (IS_ERR(bq->fcc_votable)) {
		rc = PTR_ERR(bq->fcc_votable);
		bq->fcc_votable = NULL;
	}

	bq->fv_votable = create_votable("BBC_FV", VOTE_MIN,
			bq25790_fv_vote_callback,
			bq);
	if (IS_ERR(bq->fv_votable)) {
		rc = PTR_ERR(bq->fv_votable);
		bq->fv_votable = NULL;
	}

	bq->ilim_votable = create_votable("BBC_ICL", VOTE_MIN,
			bq25790_ilim_vote_callback,
			bq);
	if (IS_ERR(bq->ilim_votable)) {
		rc = PTR_ERR(bq->ilim_votable);
		bq->ilim_votable = NULL;
	}

	bq->chg_dis_votable = create_votable("BBC_DISABLE",
			VOTE_SET_ANY, bq25790_chg_disable_vote_callback,
			bq);
	if (IS_ERR(bq->chg_dis_votable)) {
		rc = PTR_ERR(bq->chg_dis_votable);
		bq->chg_dis_votable = NULL;
	}

	bq->arti_vbus_dis_votable = create_votable("ARTI_VBUS", VOTE_SET_ANY,
					bq25790_arti_vbus_dis_vote_callback,
					bq);
	if (IS_ERR(bq->arti_vbus_dis_votable)) {
		rc = PTR_ERR(bq->arti_vbus_dis_votable);
		return rc;
	}

	vote(bq->arti_vbus_dis_votable, BQ25790_PROP_VOTER, true, 0);

	bq->awake_votable = create_votable("BBC_AWAKE",
			VOTE_SET_ANY, bq25790_awake_vote_cb, bq);
	if (IS_ERR_OR_NULL(bq->awake_votable))
		return PTR_ERR_OR_ZERO(bq->awake_votable);


	return rc;
}

static ssize_t bq25790_show_mtbf(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct bq25790 *bq = dev_get_drvdata(dev);
	int ret = 0;

	ret = snprintf(buf, PAGE_SIZE, "%d\n", bq->mtbf);

	return ret;
}

static ssize_t bq25790_store_mtbf(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct bq25790 *bq = dev_get_drvdata(dev);
	union power_supply_propval prop = {0,};
	int ret = 0;

	ret = sscanf(buf, "%d", &bq->mtbf);

	bq25790_update_byte_bits(bq, BQ25790_REG_TIMER_CTRL, 0xFF, 0x07);
	prop.intval = 288;
	power_supply_set_property(bq->bms_psy, POWER_SUPPLY_PROP_TEMP, &prop);

	vote_override(bq->fcc_votable, BQ25790_MTBF_VOTER, bq->mtbf, BQ25790_CDP_FCC);
	vote_override(bq->ilim_votable, BQ25790_MTBF_VOTER, bq->mtbf, BQ25790_CDP_IINDPM_MA);

	return count;
}
static DEVICE_ATTR(mtbf, S_IRUGO | S_IWUSR,
			bq25790_show_mtbf,
			bq25790_store_mtbf);

static ssize_t bq25790_show_registers(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct bq25790 *bq = dev_get_drvdata(dev);
	u8 addr;
	u8 val;
	u8 tmpbuf[200];
	int len;
	int idx = 0;
	int ret;

	idx = snprintf(buf, PAGE_SIZE, "%s:\n", "bq25790 Reg");
	for (addr = 0x0; addr <= 0x25; addr++) {
		ret = bq25790_read_reg(bq, addr, &val);
		if (ret == 0) {
			len = snprintf(tmpbuf, PAGE_SIZE - idx,
				"Reg[0x%.2x] = 0x%.2x\n", addr, val);
			memcpy(&buf[idx], tmpbuf, len);
			idx += len;
		}
	}

	return idx;
}

static ssize_t bq25790_store_registers(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct bq25790 *bq = dev_get_drvdata(dev);
	int ret;
	unsigned int reg;
	unsigned int val;

	ret = sscanf(buf, "%x %x", &reg, &val);
	if (ret == 2 && reg <= 0x16)
		bq25790_write_reg(bq, (unsigned char)reg, (unsigned char)val);

	return count;
}

static DEVICE_ATTR(registers, S_IRUGO | S_IWUSR,
			bq25790_show_registers,
			bq25790_store_registers);

static struct attribute *bq25790_attributes[] = {
	&dev_attr_registers.attr,
	&dev_attr_mtbf.attr,
	NULL,
};

static const struct attribute_group bq25790_attr_group = {
	.attrs = bq25790_attributes,
};

static const struct regmap_config bq25790_regmap_config = {
	.reg_bits = 8,
	.val_bits = 16,
	.max_register = 0x50,
};

static int bq25790_notifier_call(struct notifier_block *nb,
		unsigned long ev, void *v)
{
	struct power_supply *psy = v;
	struct bq25790 *bq = container_of(nb, struct bq25790, nb);
	int rc;

	if (ev != PSY_EVENT_PROP_CHANGED)
		return NOTIFY_OK;

	rc = bq25790_get_psy(bq);
	if (rc < 0) {
		bq_dbg(PR_OEM, "failed get the psy data\n");
		return NOTIFY_OK;
	}

	if (strcmp(psy->desc->name, "usb") == 0 ||
			strcmp(psy->desc->name, "battery") == 0)
		schedule_delayed_work(&bq->charge_status_change_work, 0);

	return NOTIFY_OK;
}

static int bq25790_charger_probe(struct i2c_client *client,
				 const struct i2c_device_id *id)
{
	struct bq25790 *bq;

	int ret;

	bq = devm_kzalloc(&client->dev, sizeof(struct bq25790), GFP_KERNEL);
	if (!bq)
		return -ENOMEM;

	bq->dev = &client->dev;
	bq->client = client;

	bq->regmap = devm_regmap_init_i2c(client, &bq25790_regmap_config);
	if (IS_ERR(bq->regmap)) {
		bq_dbg(PR_OEM, "failed to allocate register map\n");
		return PTR_ERR(bq->regmap);
	}
	i2c_set_clientdata(client, bq);

	mutex_init(&bq->i2c_rw_lock);
	mutex_init(&bq->data_lock);
	mutex_init(&bq->profile_change_lock);
	mutex_init(&bq->charging_disable_lock);
	mutex_init(&bq->irq_complete);

	bq->resume_completed = true;
	bq->irq_waiting = false;

	ret = bq25790_detect_device(bq);
	if (ret) {
		bq_dbg(PR_OEM, "No bq25790 device found!\n");
		return -ENODEV;
	}

	if (client->dev.of_node)
		bq->platform_data = bq25790_parse_dt(client, bq);
	else
		bq->platform_data = client->dev.platform_data;

	if (!bq->platform_data) {
		bq_dbg(PR_OEM, "No platform data provided.\n");
		return -EINVAL;
	}

	ret = bq25790_init_device(bq);
	if (ret) {
		bq_dbg(PR_OEM, "Failed to init device\n");
		return ret;
	}

	bq25790_create_votable(bq);

	ret = bq25790_psy_register(bq);
	if (ret)
		return ret;

	INIT_DELAYED_WORK(&bq->charge_irq_work,
				bq25790_charge_irq_workfunc);
	INIT_DELAYED_WORK(&bq->charge_monitor_work,
				bq25790_charge_monitor_workfunc);
	INIT_DELAYED_WORK(&bq->charge_status_change_work,
				bq25790_status_change_work);

	bq->nb.notifier_call = bq25790_notifier_call;
	ret = power_supply_reg_notifier(&bq->nb);
	if (ret < 0) {
		bq_dbg(PR_OEM, "Couldn't register psy notifier rc = %d\n", ret);
		return ret;
	}

	if (client->irq) {
		ret = devm_request_threaded_irq(&client->dev, client->irq,
				NULL,
				bq25790_charger_interrupt,
				IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
				"bq25790 charger irq", bq);
		if (ret < 0) {
			bq_dbg(PR_OEM, "request irq for irq=%d failed, ret =%d\n",
				client->irq, ret);
			goto err_1;
		}
		enable_irq_wake(client->irq);
	}

	ret = sysfs_create_group(&bq->dev->kobj, &bq25790_attr_group);
	if (ret)
		dev_err(bq->dev, "failed to register sysfs. err: %d\n", ret);

	determine_initial_status(bq);

	schedule_delayed_work(&bq->charge_irq_work, 0);
	bq_dbg(PR_OEM, "bq25790 probe successfully, Part Num:%d, Revision:0x%x \n",
				bq->part_no, bq->revision);

	return 0;
err_1:
	bq25790_psy_unregister(bq);

	return ret;
}


static inline bool is_device_suspended(struct bq25790 *bq)
{
	return !bq->resume_completed;
}

static int bq25790_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct bq25790 *bq = i2c_get_clientdata(client);

	mutex_lock(&bq->irq_complete);
	bq->resume_completed = false;
	mutex_unlock(&bq->irq_complete);

	return 0;
}

static int bq25790_suspend_noirq(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct bq25790 *bq = i2c_get_clientdata(client);

	if (bq->irq_waiting) {
		pr_err_ratelimited("Aborting suspend, an interrupt was detected while suspending\n");
		return -EBUSY;
	}
	return 0;
}

static int bq25790_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct bq25790 *bq = i2c_get_clientdata(client);


	mutex_lock(&bq->irq_complete);
	bq->resume_completed = true;
	if (bq->irq_waiting) {
		bq->irq_disabled = false;
		enable_irq(client->irq);
		mutex_unlock(&bq->irq_complete);
		bq25790_charger_interrupt(client->irq, bq);
	} else {
		mutex_unlock(&bq->irq_complete);
	}

	power_supply_changed(bq->bbc_psy);

	return 0;
}
static int bq25790_charger_remove(struct i2c_client *client)
{
	struct bq25790 *bq = i2c_get_clientdata(client);

	bq25790_psy_unregister(bq);

	mutex_destroy(&bq->charging_disable_lock);
	mutex_destroy(&bq->profile_change_lock);
	mutex_destroy(&bq->data_lock);
	mutex_destroy(&bq->i2c_rw_lock);
	mutex_destroy(&bq->irq_complete);

	sysfs_remove_group(&bq->dev->kobj, &bq25790_attr_group);


	return 0;
}


static void bq25790_charger_shutdown(struct i2c_client *client)
{
	struct bq25790 *bq = i2c_get_clientdata(client);

	bq25790_set_en_extilim(bq, true);
	bq25790_set_adc_enable(bq, false);
}

static const struct of_device_id bq25790_charger_match_table[] = {
	{.compatible = "ti,bq25790-charger",},
	{},
};
MODULE_DEVICE_TABLE(of, bq25790_charger_match_table);

static const struct i2c_device_id bq25790_charger_id[] = {
	{ "bq25790-charger", BQ25790 },
	{},
};
MODULE_DEVICE_TABLE(i2c, bq25790_charger_id);

static const struct dev_pm_ops bq25790_pm_ops = {
	.resume		= bq25790_resume,
	.suspend_noirq = bq25790_suspend_noirq,
	.suspend	= bq25790_suspend,
};
static struct i2c_driver bq25790_charger_driver = {
	.driver	= {
		.name	= "bq25790-charger",
		.owner	= THIS_MODULE,
		.of_match_table = bq25790_charger_match_table,
		.pm		= &bq25790_pm_ops,
	},
	.id_table	= bq25790_charger_id,

	.probe		= bq25790_charger_probe,
	.remove		= bq25790_charger_remove,
	.shutdown	= bq25790_charger_shutdown,

};

module_i2c_driver(bq25790_charger_driver);

MODULE_DESCRIPTION("TI bq25790 Charger Driver");
MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Texas Instruments");
