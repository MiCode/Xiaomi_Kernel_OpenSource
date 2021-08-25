/*
 * BQ25700 battery charging driver
 *
 * Copyright (C) 2017 Texas Instruments *
 * Copyright (C) 2021 XiaoMi, Inc.
 * This package is free software; you can redistribute it and/or modify
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

#define	bq_info	pr_err
#define bq_dbg	pr_debug
#define bq_err	pr_err
#define bq_log	pr_err

enum {
	USER		= BIT(0),
	JEITA		= BIT(1),
	BATT_FC		= BIT(2),		/* Batt FULL */
	BATT_PRES	= BIT(3),
};


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

	bool irq_waiting;
	bool irq_disabled;
	bool resume_completed;

	bool batt_present;
	bool usb_present;

	bool power_good;
	bool batt_full;
	int charge_state;
	int charging_disabled_status;
	bool charge_enabled;	/* Register bit status */
	bool chip_ok;
	int charger_type;
	int arti_vbus_gpio;
	bool arti_vbus_enable;

	int vbus_volt;
	int vbat_volt;
	int vsys_volt;
	int ibus_curr;
	int ichg_curr;
	int die_temp;
	int ts_temp;
	int batt_temp;

	int dev_id;

	struct delayed_work charge_monitor_work;

	int skip_writes;
	int skip_reads;

	struct bq25790_platform_data *platform_data;

	struct delayed_work charge_irq_work; /*charge mode jeita work*/

	struct power_supply *usb_psy;
	struct power_supply *bms_psy;
	struct power_supply *main_psy;
	struct power_supply *bbc_psy;
	struct power_supply *bat_psy;
	struct power_supply_desc bbc_psy_d;

	struct regmap    *regmap;
	struct votable	*fcc_votable;
	struct votable	*fv_votable;
	struct votable	*ilim_votable;
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
		bq_err("i2c read byte fail: can't read from reg 0x%02X\n", reg);
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
		bq_err("i2c write byte fail: can't write 0x%02X to reg 0x%02X\n",
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
		bq_err("Failed: reg=%02X, ret=%d\n", reg, ret);
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
		bq_err("Failed: reg=%02X, ret=%d\n", reg, ret);
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
		bq_err("Failed: reg=%02X, ret=%d\n", reg, ret);

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
		bq_err("Failed: reg=%02X, ret=%d\n", reg, ret);
		goto out;
	}

	tmp &= ~mask;
	tmp |= data & mask;

	ret = __bq25790_write_reg(bq, reg, tmp);
	if (ret)
		bq_err("Failed: reg=%02X, ret=%d\n", reg, ret);

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
		bq_err("Failed: reg=%02X, ret=%d\n", reg, ret);
	}

	mutex_unlock(&bq->i2c_rw_lock);

	return ret;
}

static int bq25790_set_arti_vbus_enable(struct bq25790 *bq, bool enable)
{
	int ret, val;

	if (enable) {
		val = ARTI_VBUS_ENABLE;
	} else {
		val = ARTI_VBUS_DISABLE;
	}
	ret = gpio_direction_output(bq->arti_vbus_gpio, val);
	if (ret) {
		bq_err("%s: unable to set direction artirq i gpio [%d]\n",
				__func__, bq->arti_vbus_gpio);
	}
	return ret;
}

/*
static int power_supply_set_online(struct power_supply *usb_psy, bool enable)
{
	union power_supply_propval prop = {0,};
	int ret;

	prop.intval = enable;
	ret = power_supply_set_property(usb_psy, POWER_SUPPLY_PROP_ONLINE, &prop);
	if (ret < 0) {
		bq_err("couldn't set USB Online property, ret=%d\n", ret);
		return ret;
	}

	return ret;
}
*/
static int power_supply_set_present(struct power_supply *usb_psy, bool enable)
{
	union power_supply_propval prop = {0,};
	int ret;

	ret = power_supply_set_property(usb_psy, POWER_SUPPLY_PROP_PRESENT, &prop);
	if (ret < 0) {
		bq_err("couldn't set USB present property, ret=%d\n", ret);
		return ret;
	}

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

	if (enable == false)
		reg_val = BQ25790_HIZ_DISABLE;
	else
		reg_val = BQ25790_HIZ_ENABLE;

	reg_val <<= BQ25790_EN_HIZ_SHIFT;

	ret = bq25790_update_byte_bits(bq, BQ25790_REG_CHG_CTRL0,
				BQ25790_EN_HIZ_MASK, reg_val);

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

static int bq25790_set_term_current(struct bq25790 *bq, int curr)
{
	int ret;
	u8 reg_val;

	if (curr < BQ25790_ITERM_BASE)
		curr = BQ25790_ITERM_BASE;

	curr -= BQ25790_ITERM_BASE;
	reg_val = curr / BQ25790_ITERM_LSB;
	reg_val <<= BQ25790_ITERM_SHIFT;

	ret = bq25790_update_byte_bits(bq, BQ25790_REG_ITERM_CTRL,
				BQ25790_ITERM_MASK, reg_val);

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

static int bq25790_set_adc_dis_mask(struct bq25790 *bq)
{
	int ret;
	u8 reg_val;

	ret = bq25790_update_byte_bits(bq, BQ25790_REG_ADC_CTRL_REG,
			BQ25790_ADC_EN_MASK, BQ25790_ADC_EN << BQ25790_ADC_EN_SHIFT);
	if (ret)
		bq_err("failed to set adc enable\n");

	reg_val = TS_ADC_DIS | TDIE_ADC_DIS;

	ret = bq25790_update_byte_bits(bq, BQ25790_REG_ADC_DISABLE_REG,
			0xFF, reg_val);
	if (ret)
		bq_err("failed to set adc enable\n");

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
	printk("==test *curr:%x\n", *curr);
	reg_val = *curr;
	printk("==test reg_val:%d\n", reg_val);

	if (ret >= 0) {
		*curr = reg_val * BQ25790_ICHG_ADC_LB_LSB * (-1);
	}
	*curr *= 1000;
	printk("==test reg_val:%d, *curr:%d\n\n", reg_val, *curr);

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

static int bq25790_charging_disable(struct bq25790 *bq, int reason,
					int disable)
{

	int ret = 0;
	int disabled;

	mutex_lock(&bq->charging_disable_lock);

	disabled = bq->charging_disabled_status;

	bq_log("reason=%d requested_disable=%d disabled_status=%d\n",
					reason, disable, disabled);

	if (disable == true)
		disabled |= reason;
	else
		disabled &= ~reason;

	if (disabled && bq->charge_enabled)
		ret = bq25790_charge_enable(bq, false);
	else if (!disabled && !bq->charge_enabled)
		ret = bq25790_charge_enable(bq, true);

	if (ret) {
		bq_err("Couldn't disable/enable charging for reason=%d ret=%d\n",
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

static struct power_supply *get_bms_psy(struct bq25790 *bq)
{
	if (bq->bms_psy)
		return bq->bms_psy;
	bq->bms_psy = power_supply_get_by_name("bms");
	if (!bq->bms_psy)
		pr_debug("bms power supply not found\n");

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
	u8 val = 0;

	if (is_device_suspended(bq))
		return POWER_SUPPLY_CHARGE_TYPE_UNKNOWN;

	bq25790_read_reg(bq, BQ25790_REG_CHG_STATUS1, &val);
	val &= BQ25790_CHRG_STAT_MASK;
	val >>= BQ25790_CHRG_STAT_SHIFT;

	switch (val) {
	case BQ25790_CHRG_STAT_FAST:
		return POWER_SUPPLY_CHARGE_TYPE_FAST;
	case BQ25790_CHRG_STAT_PRECHG:
		return POWER_SUPPLY_CHARGE_TYPE_TRICKLE;
	case BQ25790_CHRG_STAT_DONE:
	case BQ25790_CHRG_STAT_NOT_CHARGING:
		return POWER_SUPPLY_CHARGE_TYPE_NONE;
	case BQ25790_CHRG_STAT_TAPER:
		return POWER_SUPPLY_CHARGE_TYPE_TAPER;
	default:
		return POWER_SUPPLY_CHARGE_TYPE_UNKNOWN;
	}
}

static int bq25790_get_prop_batt_present(struct bq25790 *bq)
{
	union power_supply_propval batt_prop = {0,};
	int ret;

	ret = bq25790_get_property_from_bms(bq,
			POWER_SUPPLY_PROP_PRESENT, &batt_prop);
	if (!ret)
		bq->batt_present = batt_prop.intval;

	return ret;

}

static int bq25790_get_prop_batt_full(struct bq25790 *bq)
{
	union power_supply_propval batt_prop = {0,};
	int ret;

	ret = bq25790_get_property_from_bms(bq,
			POWER_SUPPLY_PROP_STATUS, &batt_prop);
	if (!ret)
		bq->batt_full = (batt_prop.intval == POWER_SUPPLY_STATUS_FULL);

	return ret;
}

static int bq25790_get_prop_charge_status(struct bq25790 *bq)
{
	union power_supply_propval batt_prop = {0,};
	int ret;
	u8 status;

	ret = bq25790_get_property_from_bms(bq,
			POWER_SUPPLY_PROP_STATUS, &batt_prop);
	if (!ret && batt_prop.intval == POWER_SUPPLY_STATUS_FULL)
		return POWER_SUPPLY_STATUS_FULL;

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
		return POWER_SUPPLY_STATUS_NOT_CHARGING;
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
	POWER_SUPPLY_PROP_CHIP_OK,
	POWER_SUPPLY_PROP_CHARGING_ENABLED,
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_HEALTH,
	POWER_SUPPLY_PROP_USB_CURRENT_NOW,
	POWER_SUPPLY_PROP_USB_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT,
	POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE,
	POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT,
	POWER_SUPPLY_PROP_HIZ_MODE,
	POWER_SUPPLY_PROP_CHARGE_DONE,
	POWER_SUPPLY_PROP_VOLTAGE_MAX,
	POWER_SUPPLY_PROP_CURRENT_MAX,
	POWER_SUPPLY_PROP_MAIN_FCC_MAX,
	POWER_SUPPLY_PROP_ARTI_VBUS_ENABLE,
};

static int bq25790_charger_get_property(struct power_supply *psy,
				enum power_supply_property psp,
				union power_supply_propval *val)
{

	struct bq25790 *bq = power_supply_get_drvdata(psy);

	switch (psp) {
	case POWER_SUPPLY_PROP_CHARGE_TYPE:
		val->intval = bq25790_get_prop_charge_type(bq);
		bq_log("POWER_SUPPLY_PROP_CHARGE_TYPE:%d\n", val->intval);
		break;
	case POWER_SUPPLY_PROP_PRESENT:
		val->intval = bq->usb_present;
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
	case POWER_SUPPLY_PROP_USB_CURRENT_NOW:
		bq25790_read_bus_curr(bq, &val->intval);
		bq_log("POWER_SUPPLY_PROP_USB_CURRENT_NOW: %d\n", val->intval);
		break;
	case POWER_SUPPLY_PROP_USB_VOLTAGE_NOW:
		bq25790_read_bus_volt(bq, &val->intval);
		bq_log("POWER_SUPPLY_PROP_USB_VOLTAGE_NOW: %d\n", val->intval);
		break;
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT:
		bq25790_get_charge_current(bq, &val->intval);
		bq_log("POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT: %d\n", val->intval);
		break;
	case POWER_SUPPLY_PROP_HIZ_MODE:
		bq25790_get_hiz_mode(bq, &val->intval);
		break;
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE:
		bq25790_get_charge_voltage(bq, &val->intval);
		bq_log("POWER_SUPPLY_PROP_CHARGE_VOLTAGE: %d\n", val->intval);
		break;
	case POWER_SUPPLY_PROP_CHARGE_DONE:
	case POWER_SUPPLY_PROP_VOLTAGE_MAX:
	case POWER_SUPPLY_PROP_CURRENT_MAX:
		bq25790_get_property_from_bms(bq, psp, val);
		break;
	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
		bq25790_get_input_current_limit(bq, &val->intval);
		bq_log("POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT: %d\n",
					val->intval);
		break;
	case POWER_SUPPLY_PROP_MAIN_FCC_MAX:
		val->intval = 3000000;
		break;
	case POWER_SUPPLY_PROP_ARTI_VBUS_ENABLE:
		val->intval = bq->arti_vbus_enable;
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
		bq25790_charging_disable(bq, USER, !val->intval);

		power_supply_changed(bq->bbc_psy);
		power_supply_changed(bq->usb_psy);
		bq_log("POWER_SUPPLY_PROP_CHARGING_ENABLED: %s\n",
					val->intval ? "enable" : "disable");
		break;
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT:
		bq_log("POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT: %d\n",
					val->intval);
		vote(bq->fcc_votable, BQ25790_PROP_VOTER,
				val->intval > 0 ? true : false, val->intval);
	     break;
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE:
		bq_log("POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE: %d\n",
					val->intval);
		vote(bq->fv_votable, BQ25790_PROP_VOTER,
				val->intval > 0 ? true : false, val->intval);
	     break;
	case POWER_SUPPLY_PROP_HIZ_MODE:
		bq_log("POWER_SUPPLY_PROP_HIZ_MODE: %d\n",
					val->intval);
		bq25790_set_hiz_mode(bq, val->intval);
		break;
	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
		bq_log("POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT: %d\n",
					val->intval);
		vote(bq->ilim_votable, BQ25790_PROP_VOTER,
				val->intval > 0 ? true : false, val->intval);
		break;
	case POWER_SUPPLY_PROP_ARTI_VBUS_ENABLE:
		bq25790_set_arti_vbus_enable(bq, !!val->intval);
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
	int vindpm, iindpm, fcc;
	union power_supply_propval prop = {0,};


	if (!bq->usb_present)
		return 0;

	ret = power_supply_get_property(bq->usb_psy,
				POWER_SUPPLY_PROP_REAL_TYPE, &prop);

	if (ret < 0) {
		bq_err("couldn't read USB TYPE property, ret=%d\n", ret);
		return ret;
	}
	bq->charger_type = prop.intval;

	mutex_lock(&bq->profile_change_lock);
	switch (bq->charger_type) {
	case POWER_SUPPLY_TYPE_USB_HVDCP:
	case POWER_SUPPLY_TYPE_USB_HVDCP_3:
	case POWER_SUPPLY_TYPE_USB_PD:
		vindpm = BQ25790_HVDCP_VINDPM_MV;
		iindpm = BQ25790_HVDCP_IINDPM_MA;
		fcc = BQ25790_HVDCP_FCC;
		break;
	case POWER_SUPPLY_TYPE_USB_DCP:
	case POWER_SUPPLY_TYPE_USB_CDP:
		vindpm = BQ25790_DCP_VINDPM_MV;
		iindpm = BQ25790_DCP_IINDPM_MA;
		fcc = BQ25790_DCP_FCC;
		break;
	case POWER_SUPPLY_TYPE_USB:
		vindpm = BQ25790_SDP_VINDPM_MV;
		iindpm = BQ25790_SDP_IINDPM_MA;
		fcc = BQ25790_SDP_FCC;
		break;
	default:
		vindpm = BQ25790_SDP_VINDPM_MV;
		iindpm = BQ25790_SDP_IINDPM_MA;
		fcc = BQ25790_DEFUALT_FCC;
		break;

	}

	bq_log("charge_type:%d, vindpm:%d, iindpm:%d\n", bq->charger_type, vindpm, iindpm);

	ret = bq25790_set_input_volt_limit(bq, vindpm);
	if (ret < 0)
		bq_err("couldn't set input voltage limit, ret=%d\n", ret);

	vote(bq->ilim_votable, BQ25790_INIT_VOTER, true, iindpm);
	vote(bq->fcc_votable, BQ25790_INIT_VOTER, true, fcc);
	vote(bq->fv_votable, BQ25790_INIT_VOTER, true, BQ25790_DEFUALT_FV);

	mutex_unlock(&bq->profile_change_lock);
	power_supply_changed(bq->bbc_psy);

	return 0;
}

static void bq25790_external_power_changed(struct power_supply *psy)
{
	struct bq25790 *bq = power_supply_get_drvdata(psy);

	union power_supply_propval prop = {0,};
	int ret;

	ret = power_supply_get_property(bq->usb_psy,
				POWER_SUPPLY_PROP_REAL_TYPE, &prop);

	if (ret < 0) {
		bq_err("couldn't read USB TYPE property, ret=%d\n", ret);
		return;
	}
	bq_info("charger type :%d", prop.intval);
	if(bq->charger_type != prop.intval) {
		bq25790_update_charging_profile(bq);
	}

	ret = power_supply_get_property(bq->usb_psy,
				POWER_SUPPLY_PROP_ONLINE, &prop);
	if (ret < 0)
		bq_err("could not read USB ONLINE property, ret=%d\n", ret);
	else
		bq_dbg("usb online status =%d\n", prop.intval);

	ret = 0;
	bq25790_get_prop_charge_status(bq);
	if (bq->usb_present && bq->charge_state != BQ25790_CHRG_STAT_NOT_CHARGING) {
		if (prop.intval == 0) {
			bq_dbg("set boost charger online\n");
			//ret = power_supply_set_online(bq->usb_psy, 2);
		}
	} else {
		if (prop.intval == 1) {
			bq_dbg("set boost charger offline\n");
			//ret = power_supply_set_online(bq->usb_psy, 0);
		}
	}

	if (ret < 0)
		bq_err("could not set usb online state, ret=%d\n", ret);

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
	bq->bbc_psy_d.external_power_changed = bq25790_external_power_changed;
	bq->bbc_psy_d.property_is_writeable = bq25790_charger_is_writeable;

	bbc_psy_cfg.drv_data = bq;
	bbc_psy_cfg.num_supplicants = 0;

	bq->bbc_psy = power_supply_register(bq->dev,
				&bq->bbc_psy_d,
				&bbc_psy_cfg);
	if (IS_ERR(bq->bbc_psy)) {
		bq_err("couldn't register battery psy, ret = %ld\n",
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
		bq_err("Failed to read node of ti,bq25790,precharge-current\n");

	ret = of_property_read_u32(np, "ti,bq25790,termi-curr", &pdata->iterm);
	if (ret)
		bq_err("Failed to read node of ti,bq25790,termi-curr\n");

	ret = of_property_read_u32(np, "ti,bq25790,safe_timer_en", &pdata->safe_timer_en);
	if (ret)
		bq_err("Failed to read node of ti,bq25790,safe_timer_en\n");

	ret = of_property_read_u32(np, "ti,bq25790,safe_timer", &pdata->safe_timer);
	if (ret)
		bq_err("Failed to read node of ti,bq25790,safe_timer\n");

	ret = of_property_read_u32(np, "ti,bq25790,presafe_timer", &pdata->presafe_timer);
	if (ret)
		bq_err("Failed to read node of ti,bq25790,presafe_timer\n");

	ret = of_property_read_u32(np, "ti,bq25790,vac_ovp", &pdata->vac_ovp);
	if (ret)
		bq_err("Failed to read node of ti,bq25790,vac_ovp\n");

	ret = of_property_read_u32(np, "ti,bq25790,cell_num", &pdata->cell_num);
	if (ret)
		bq_err("Failed to read node of ti,bq25790,cell_num\n");

	ret = of_property_read_u32(np, "ti,bq25790,vsys_min", &pdata->vsys_min);
	if (ret)
		bq_err("Failed to read node of ti,vsys_min\n");

	irq_gpio = of_get_named_gpio(np, "ti,bq25790,irq", 0);
	if ((!gpio_is_valid(irq_gpio))) {
		bq_err("Failed to read node of ti,bq25790,boost-current\n");

	} else {
		ret = gpio_request(irq_gpio, "bq25790_irq_gpio");
		if (ret) {
			bq_err("%s: unable to request bq25790 irq gpio [%d]\n",
					__func__, irq_gpio);
		}
		ret = gpio_direction_input(irq_gpio);
		if (ret) {
			bq_err("%s: unable to set direction for bq25790 irq gpio [%d]\n",
					__func__, irq_gpio);
		}
		client->irq = gpio_to_irq(irq_gpio);
	}

	bq->arti_vbus_gpio = of_get_named_gpio(np, "ti,bq25790,arti_vbus", 0);
	if ((!gpio_is_valid(bq->arti_vbus_gpio))) {
		bq_err("Failed to read node of ti,bq25790,boost-current\n");

	} else {
		ret = gpio_request(bq->arti_vbus_gpio, "bq25790_arti_gpio");
		if (ret) {
			bq_err("%s: unable to request bq25790 vbus gpio [%d]\n",
					__func__, bq->arti_vbus_gpio);
		}
		ret = gpio_direction_output(bq->arti_vbus_gpio, 0);
		if (ret) {
			bq_err("%s: unable to set direction for bq25790 arti vbus gpio [%d]\n",
					__func__, bq->arti_vbus_gpio);
		}
	}
	bq_info("irq_gpio:%d, arti_vbus_gpio:%d\n", irq_gpio, bq->arti_vbus_gpio);

	return pdata;
}

static int bq25790_init_device(struct bq25790 *bq)
{
	int ret;

	ret = bq25790_set_prechg_current(bq, bq->platform_data->iprechg);
	if (ret)
		bq_err("Failed to set prechg current, ret = %d\n", ret);

	ret = bq25790_set_term_current(bq, bq->platform_data->iterm);
	if (ret)
		bq_err("Failed to set termination current, ret = %d\n", ret);

	ret = bq25790_enable_safety_timer(bq, bq->platform_data->safe_timer_en);
	if (ret)
		bq_err("Failed to enable safety timer, ret = %d\n", ret);

	ret = bq25790_set_safety_timer(bq, bq->platform_data->safe_timer);
	if (ret)
		bq_err("Failed to set safety timer, ret = %d\n", ret);

	ret = bq25790_set_pre_safety_timer(bq, bq->platform_data->presafe_timer);
	if (ret)
		bq_err("Failed to set safety timer, ret = %d\n", ret);

	ret = bq25790_set_vac_ovp(bq, bq->platform_data->vac_ovp);
	if (ret)
		bq_err("Failed to set vac ovp, ret = %d\n", ret);

	ret = bq25790_set_cell_num(bq, bq->platform_data->cell_num);
	if (ret)
		bq_err("Failed to set cell number, ret = %d\n", ret);

	ret = bq25790_set_vsys_min_volt(bq, bq->platform_data->vsys_min);
	if (ret)
		bq_err("Failed to set vsysmin, ret = %d\n", ret);

	bq25790_set_adc_dis_mask(bq);
	bq25790_set_int_mask(bq);
	bq25790_set_fault_mask(bq);
	bq25790_set_ico_mode(bq, true);

	ret = bq25790_charge_enable(bq, true);
	if (ret) {
		bq_err("Failed to enable charger, ret = %d\n", ret);
	} else {
		bq->charge_enabled = true;
		bq_log("Charger Enabled Successfully!\n");
	}

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
	bq_info("%s:BQ25790_REG_PART_NUM:0x%x bq->part_no:0x%x bq->revision:%x",
			__func__, data, bq->part_no, bq->revision);

	return ret;
}

static void bq25790_update_status(struct bq25790 *bq)
{

	bq25790_read_bus_volt(bq, &bq->vbus_volt);
	bq25790_read_bat_volt(bq, &bq->vbat_volt);
	bq25790_read_bus_curr(bq, &bq->ibus_curr);
	bq25790_read_bat_curr(bq, &bq->ichg_curr);

	bq_log("vbus:%d, vbat:%d, ibus:%d, ichg:%d\n",
		bq->vbus_volt, bq->vbat_volt,
		bq->ibus_curr, bq->ichg_curr);

}

static void bq25790_check_batt_pres(struct bq25790 *bq)
{
	int ret = 0;
	bool chg_disabled_pres;

	ret = bq25790_get_prop_batt_present(bq);
	if (!ret) {
		chg_disabled_pres = !!(bq->charging_disabled_status & BATT_PRES);
		if (chg_disabled_pres ^ !bq->batt_present) {
			ret = bq25790_charging_disable(bq, BATT_PRES, !bq->batt_present);
			if (ret) {
				bq_err("failed to %s charging, ret = %d\n",
					bq->batt_present ? "disable" : "enable",
					ret);
			}
			bq_log("battery present:%d\n", bq->batt_present);
			power_supply_changed(bq->bbc_psy);
			power_supply_changed(bq->usb_psy);
		}
	}


}

static void bq25790_check_batt_full(struct bq25790 *bq)
{
	int ret = 0;
	bool chg_disabled_fc;

	ret = bq25790_get_prop_batt_full(bq);
	if (!ret) {
		chg_disabled_fc = !!(bq->charging_disabled_status & BATT_FC);
		if (chg_disabled_fc ^ bq->batt_full) {
			ret = bq25790_charging_disable(bq, BATT_FC, bq->batt_full);
			if (ret) {
				bq_err("failed to %s charging, ret = %d\n",
					bq->batt_full ? "disable" : "enable",
					ret);
			}
			bq_log("battery full:%d\n", bq->batt_present);
			power_supply_changed(bq->bbc_psy);
			power_supply_changed(bq->usb_psy);
		}
	}
}


static void bq25790_charge_monitor_workfunc(struct work_struct *work)
{
	struct bq25790 *bq = container_of(work,
			struct bq25790, charge_monitor_work.work);
#if 0
	u8 addr;
	u8 val;


	for (addr = 0x0; addr <= 0x25; addr++) {
		bq25790_read_reg(bq, addr, &val);
			bq_err("bq Reg0x%.2x=0x%.2x\n", addr, val);
	}

#endif

	bq25790_check_batt_pres(bq);
	bq25790_check_batt_full(bq);

	bq25790_update_status(bq);
	bq25790_reset_wdt(bq);

	schedule_delayed_work(&bq->charge_monitor_work, 6 * HZ);
}


static void bq25790_charge_irq_workfunc(struct work_struct *work)
{
	struct bq25790 *bq = container_of(work,
			struct bq25790, charge_irq_work.work);
	u8 status;
	int ret;

	mutex_lock(&bq->irq_complete);
	bq->irq_waiting = true;
	if (!bq->resume_completed) {
		dev_dbg(bq->dev, "IRQ triggered before device-resume\n");
		if (!bq->irq_disabled) {
			disable_irq_nosync(bq->client->irq);
			bq->irq_disabled = true;
		}
		mutex_unlock(&bq->irq_complete);
		return;
	}
	bq->irq_waiting = false;
	ret = bq25790_read_reg(bq, BQ25790_REG_CHG_STATUS0, &status);
	if (ret) {
		mutex_unlock(&bq->irq_complete);
		return;
	}

	mutex_lock(&bq->data_lock);
	bq->power_good = !!(status & BQ25790_PG_STAT_MASK);
	mutex_unlock(&bq->data_lock);

	if (!bq->power_good) {
		if (bq->usb_present) {
			bq->usb_present = false;
			power_supply_set_present(bq->usb_psy, bq->usb_present);
		}

		bq25790_set_wdt_timer(bq, 0);

		bq_err("usb removed, set usb present = %d\n", bq->usb_present);
	} else if (bq->power_good && !bq->usb_present) {
		bq->usb_present = true;
		msleep(10);/*for cdp detect*/
		power_supply_set_present(bq->usb_psy, bq->usb_present);
		bq25790_set_wdt_timer(bq, 80);
		bq25790_reset_wdt(bq);
		ret = bq25790_init_device(bq);
		if (ret) {
			bq_err("Failed to init device\n");
			return;
		}
		bq25790_update_charging_profile(bq);
		schedule_delayed_work(&bq->charge_monitor_work, 60 * HZ);

		bq_err("usb plugged in, set usb present = %d\n", bq->usb_present);
	}

	bq25790_update_status(bq);
	mutex_unlock(&bq->irq_complete);

	power_supply_changed(bq->bbc_psy);
}

static irqreturn_t bq25790_charger_interrupt(int irq, void *dev_id)
{
	struct bq25790 *bq = dev_id;

	schedule_delayed_work(&bq->charge_irq_work, 0);

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

	rc = bq25790_set_charge_current(bq, fcc_ua);
	if (rc < 0) {
		bq_err("failed to allocate register map\n");
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
		bq_err("failed to allocate register map\n");
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

	rc = bq25790_set_input_curr_limit(bq, icl_ua);
	if (rc < 0) {
		bq_err("failed to allocate register map\n");
		return rc;
	}

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

	return rc;
}


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

static int bq25790_charger_probe(struct i2c_client *client,
				 const struct i2c_device_id *id)
{
	struct bq25790 *bq;
	struct power_supply *usb_psy;
	struct power_supply *bms_psy;
	struct power_supply *main_psy;

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

	main_psy = power_supply_get_by_name("main");
	if (!main_psy) {
		dev_dbg(&client->dev, "USB supply not found, defer probe\n");
		return -EPROBE_DEFER;
	}

	bq = devm_kzalloc(&client->dev, sizeof(struct bq25790), GFP_KERNEL);
	if (!bq)
		return -ENOMEM;

	bq->dev = &client->dev;
	bq->usb_psy = usb_psy;
	bq->bms_psy = bms_psy;
	bq->main_psy = main_psy;

	bq->client = client;

	bq->regmap = devm_regmap_init_i2c(client, &bq25790_regmap_config);
	if (IS_ERR(bq->regmap)) {
		bq_err("failed to allocate register map\n");
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
		bq_err("No bq25790 device found!\n");
		return -ENODEV;
	}

	if (client->dev.of_node)
		bq->platform_data = bq25790_parse_dt(client, bq);
	else
		bq->platform_data = client->dev.platform_data;

	if (!bq->platform_data) {
		bq_err("No platform data provided.\n");
		return -EINVAL;
	}

	ret = bq25790_init_device(bq);
	if (ret) {
		bq_err("Failed to init device\n");
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

	if (client->irq) {
		ret = devm_request_threaded_irq(&client->dev, client->irq,
				NULL,
				bq25790_charger_interrupt,
				IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
				"bq25790 charger irq", bq);
		if (ret < 0) {
			bq_err("request irq for irq=%d failed, ret =%d\n",
				client->irq, ret);
			goto err_1;
		}
		enable_irq_wake(client->irq);
	}

	ret = sysfs_create_group(&bq->dev->kobj, &bq25790_attr_group);
	if (ret)
		dev_err(bq->dev, "failed to register sysfs. err: %d\n", ret);

	determine_initial_status(bq);

	bq_err("bq25790 probe successfully, Part Num:%d, Revision:0x%x \n",
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
