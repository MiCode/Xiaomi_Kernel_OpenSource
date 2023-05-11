/*
 *otg-gpio BQ2589x battery charging driver
 *
 * Copyright (C) 2013 Texas Instruments
 *
 * This package is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 * THIS PACKAGE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

#define pr_fmt(fmt)	"[bq2589x] %s: " fmt, __func__

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
#include <linux/of_gpio.h>
#include "bq2589x_reg.h"
#include "bq2589x_charger.h"

#define PROFILE_CHG_VOTER		"PROFILE_CHG_VOTER"
#define MAIN_SET_VOTER			"MAIN_SET_VOTER"
//#define PD2SW_HITEMP_OCCURE_VOTER  "PD2SW_HITEMP_OCCURE_VOTER"
#define CHG_FCC_CURR_MAX		6000
#define CHG_ICL_CURR_MAX		3000
#define NOTIFY_COUNT_MAX		40
#define MAIN_ICL_MIN 100
extern bool g_ffc_disable;

enum print_reason {
	PR_INTERRUPT	= BIT(0),
	PR_REGISTER		= BIT(1),
	PR_OEM			= BIT(2),
	PR_DEBUG		= BIT(3),
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

static struct bq2589x *g_bq;
static struct pe_ctrl pe;
//int get_apdo_regain;
static bool vbus_on = false;

extern void start_fg_monitor_work(struct power_supply *psy);
extern void stop_fg_monitor_work(struct power_supply *psy);
extern char nopmi_set_charger_ic_type(NOPMI_CHARGER_IC_TYPE nopmi_type);

static int bq2589x_set_fast_charge_mode(struct bq2589x *bq, int pd_active)
{
	int rc = 0;
	union power_supply_propval propval ={0, };
	int batt_verify = 0, batt_soc = 0, batt_temp = 0;

	if(!bq->bms_psy)
		bq->bms_psy = power_supply_get_by_name("bms");
	if(bq->bms_psy){
		rc = power_supply_get_property(bq->bms_psy, POWER_SUPPLY_PROP_CHIP_OK, &propval);
		if (rc < 0) {
			pr_err("%s : get battery chip ok fail\n",__func__);
		}
		batt_verify = propval.intval;

		rc = power_supply_get_property(bq->bms_psy, POWER_SUPPLY_PROP_CAPACITY, &propval);
		if (rc < 0) {
			pr_err("%s : get battery capatity fail\n", __func__);
		}
		batt_soc = propval.intval;

		rc = power_supply_get_property(bq->bms_psy, POWER_SUPPLY_PROP_TEMP, &propval);
		if (rc < 0) {
			pr_err("%s : get battery temp fail\n", __func__);
		}
		batt_temp = propval.intval;
	}
	/*If TA plug in with PPS, battery auth success and soc less than 95%, FFC flag will enabled.
		The temp is normal set fastcharge mode as 1 and jeita loop also handle fastcharge prop*/
	//pr_err("%s: batt_verify: %d, batt_soc: %d, batt_temp: %d",__func__, batt_verify, batt_soc, batt_temp);
	if ((pd_active == 2) && batt_soc < 95 ){
		g_ffc_disable = false;
		if(batt_temp >= 150 && batt_temp <= 480){
			propval.intval = 1;
		} else {
			propval.intval = 0;
		}
	} else {
		/*If TA plug in without PPS, battery auth fail and soc exceed 95%, FFC will always be disabled*/
		propval.intval = 0;
		g_ffc_disable = true;
	}

	bq->bms_psy = power_supply_get_by_name("bms");

	if (!bq->bms_psy) {
		propval.intval = 0;
		rc = -ENOENT;
	} else {
		rc = power_supply_set_property(bq->bms_psy, POWER_SUPPLY_PROP_FASTCHARGE_MODE,
			&propval);
		power_supply_put(bq->bms_psy);
	}

	if (rc < 0) {
		pr_err("%s : set fastcharge mode fail\n", __func__);
	}

	return rc;
}
static int bq2589x_read_byte(struct bq2589x *bq, u8 *data, u8 reg)
{
	int ret;
	int count = 3;

  	mutex_lock(&bq->i2c_rw_lock);
	while(count--) {
          ret = i2c_smbus_read_byte_data(bq->client, reg);
          if (ret < 0) {
            pr_err("%s: failed to read 0x%.2x\n",__func__, reg);
          } else {
            *data = (u8)ret;
            mutex_unlock(&bq->i2c_rw_lock);
            return 0;
          }
          udelay(200);
        }
        mutex_unlock(&bq->i2c_rw_lock);
  return ret;
}

static int bq2589x_write_byte(struct bq2589x *bq, u8 reg, u8 data)
{
	int ret = 0;
	mutex_lock(&bq->i2c_rw_lock);
	ret = i2c_smbus_write_byte_data(bq->client, reg, data);
	mutex_unlock(&bq->i2c_rw_lock);
	return ret;
}

static int bq2589x_update_bits(struct bq2589x *bq, u8 reg, u8 mask, u8 data)
{
	int ret;
	u8 tmp;

	ret = bq2589x_read_byte(bq, &tmp, reg);
	if (ret)
		return ret;

	tmp &= ~mask;
	tmp |= data & mask;

	return bq2589x_write_byte(bq, reg, tmp);
}

#if 0
static enum bq2589x_vbus_type bq2589x_get_vbus_type(struct bq2589x *bq)
{
	u8 val = 0;
	int ret;

	ret = bq2589x_read_byte(bq, &val, BQ2589X_REG_0B);
	pr_err("%s: zsa get charger type start ret:%d\n", __func__, ret);
	if (ret < 0)
		return 0;
	val &= BQ2589X_VBUS_STAT_MASK;
	val >>= BQ2589X_VBUS_STAT_SHIFT;
	pr_err("%s: zsa get charger type end val:%d\n", __func__, val);

	return val;
}
#endif

static int bq2589x_enable_otg(struct bq2589x *bq)
{
	u8 val = BQ2589X_OTG_ENABLE << BQ2589X_OTG_CONFIG_SHIFT;

	return bq2589x_update_bits(bq, BQ2589X_REG_03,
							   BQ2589X_OTG_CONFIG_MASK, val);

}

static int bq2589x_disable_otg(struct bq2589x *bq)
{
	u8 val = BQ2589X_OTG_DISABLE << BQ2589X_OTG_CONFIG_SHIFT;

	return bq2589x_update_bits(bq, BQ2589X_REG_03,
							   BQ2589X_OTG_CONFIG_MASK, val);

}
EXPORT_SYMBOL_GPL(bq2589x_disable_otg);

static int bq2589x_set_otg_volt(struct bq2589x *bq, int volt)
{
	u8 val = 0;

	if (bq->part_no == SC89890H) {
		if (volt < SC89890H_BOOSTV_BASE)
			volt = SC89890H_BOOSTV_BASE;
		if (volt > SC89890H_BOOSTV_BASE + (BQ2589X_BOOSTV_MASK >> BQ2589X_BOOSTV_SHIFT) * SC89890H_BOOSTV_LSB)
			volt = SC89890H_BOOSTV_BASE + (BQ2589X_BOOSTV_MASK >> BQ2589X_BOOSTV_SHIFT) * SC89890H_BOOSTV_LSB;

		val = ((volt - SC89890H_BOOSTV_BASE) / SC89890H_BOOSTV_LSB) << BQ2589X_BOOSTV_SHIFT;
	} else {
		if (volt < BQ2589X_BOOSTV_BASE)
			volt = BQ2589X_BOOSTV_BASE;
		if (volt > BQ2589X_BOOSTV_BASE + (BQ2589X_BOOSTV_MASK >> BQ2589X_BOOSTV_SHIFT) * BQ2589X_BOOSTV_LSB)
			volt = BQ2589X_BOOSTV_BASE + (BQ2589X_BOOSTV_MASK >> BQ2589X_BOOSTV_SHIFT) * BQ2589X_BOOSTV_LSB;

		val = ((volt - BQ2589X_BOOSTV_BASE) / BQ2589X_BOOSTV_LSB) << BQ2589X_BOOSTV_SHIFT;
	}


	return bq2589x_update_bits(bq, BQ2589X_REG_0A, BQ2589X_BOOSTV_MASK, val);

}
EXPORT_SYMBOL_GPL(bq2589x_set_otg_volt);

static int bq2589x_set_otg_current(struct bq2589x *bq, int curr)
{
	u8 temp;

	if (bq->part_no == SC89890H) {
		if (curr < 600)
			temp = SC89890H_BOOST_LIM_500MA;
		else if (curr < 900)
			temp = SC89890H_BOOST_LIM_750MA;
		else if (curr < 1300)
			temp = SC89890H_BOOST_LIM_1200MA;
		else if (curr < 1500)
			temp = SC89890H_BOOST_LIM_1400MA;
		else if (curr < 1700)
			temp = SC89890H_BOOST_LIM_1650MA;
		else if (curr < 1900)
			temp = SC89890H_BOOST_LIM_1875MA;
		else if (curr < 2200)
			temp = SC89890H_BOOST_LIM_2150MA;
		else if (curr < 2500)
			temp = SC89890H_BOOST_LIM_2450MA;
		else
			temp = SC89890H_BOOST_LIM_1400MA;
	} else {
		if (curr <= 500)
			temp = BQ2589X_BOOST_LIM_500MA;
		else if (curr > 500 && curr <= 800)
			temp = BQ2589X_BOOST_LIM_700MA;
		else if (curr > 800 && curr <= 1200)
			temp = BQ2589X_BOOST_LIM_1100MA;
		else if (curr > 1200 && curr <= 1400)
			temp = BQ2589X_BOOST_LIM_1300MA;
		else if (curr > 1400 && curr <= 1700)
			temp = BQ2589X_BOOST_LIM_1600MA;
		else if (curr > 1700 && curr <= 1900)
			temp = BQ2589X_BOOST_LIM_1800MA;
		else if (curr > 1900 && curr <= 2200)
			temp = BQ2589X_BOOST_LIM_2100MA;
		else if (curr > 2200 && curr <= 2300)
			temp = BQ2589X_BOOST_LIM_2400MA;
		else
			temp = BQ2589X_BOOST_LIM_2400MA;
	}

	return bq2589x_update_bits(bq, BQ2589X_REG_0A, BQ2589X_BOOST_LIM_MASK, temp << BQ2589X_BOOST_LIM_SHIFT);
}
EXPORT_SYMBOL_GPL(bq2589x_set_otg_current);

static int bq2589x_enable_charger(struct bq2589x *bq)
{
	int ret;
	u8 val = BQ2589X_CHG_ENABLE << BQ2589X_CHG_CONFIG_SHIFT;

	ret = bq2589x_update_bits(bq, BQ2589X_REG_03, BQ2589X_CHG_CONFIG_MASK, val);
	if (ret == 0)
		bq->status |= BQ2589X_STATUS_CHARGE_ENABLE;
	
	return ret;
}

static int bq2589x_disable_charger(struct bq2589x *bq)
{
	int ret;
	u8 val = BQ2589X_CHG_DISABLE << BQ2589X_CHG_CONFIG_SHIFT;

	ret = bq2589x_update_bits(bq, BQ2589X_REG_03, BQ2589X_CHG_CONFIG_MASK, val);
	if (ret == 0)
		bq->status &= ~BQ2589X_STATUS_CHARGE_ENABLE;

	return ret;
}
//EXPORT_SYMBOL_GPL(bq2589x_disable_charger);


/* interfaces that can be called by other module */
int bq2589x_adc_start(struct bq2589x *bq, bool oneshot)
{
	u8 val;
	int ret;

	ret = bq2589x_read_byte(bq, &val, BQ2589X_REG_02);
	if (ret < 0) {
		bq_dbg(PR_OEM, "%s failed to read register 0x02:%d\n", __func__, ret);
		return ret;
	}

	if (((val & BQ2589X_CONV_RATE_MASK) >> BQ2589X_CONV_RATE_SHIFT) == BQ2589X_ADC_CONTINUE_ENABLE)
		return 0; /*is doing continuous scan*/
	if (oneshot)
		ret = bq2589x_update_bits(bq, BQ2589X_REG_02, BQ2589X_CONV_START_MASK, BQ2589X_CONV_START << BQ2589X_CONV_START_SHIFT);
	else
		ret = bq2589x_update_bits(bq, BQ2589X_REG_02, BQ2589X_CONV_RATE_MASK,  BQ2589X_ADC_CONTINUE_ENABLE << BQ2589X_CONV_RATE_SHIFT);
	return ret;
}
EXPORT_SYMBOL_GPL(bq2589x_adc_start);

int bq2589x_adc_stop(struct bq2589x *bq)
{
	return bq2589x_update_bits(bq, BQ2589X_REG_02, BQ2589X_CONV_RATE_MASK, BQ2589X_ADC_CONTINUE_DISABLE << BQ2589X_CONV_RATE_SHIFT);
}
EXPORT_SYMBOL_GPL(bq2589x_adc_stop);


int bq2589x_adc_read_battery_volt(struct bq2589x *bq)
{
	uint8_t val;
	int volt;
	int ret;
	ret = bq2589x_read_byte(bq, &val, BQ2589X_REG_0E);
	if (ret < 0) {
		bq_dbg(PR_OEM, "read battery voltage failed :%d\n", ret);
		return ret;
	} else{
		volt = BQ2589X_BATV_BASE + ((val & BQ2589X_BATV_MASK) >> BQ2589X_BATV_SHIFT) * BQ2589X_BATV_LSB ;
		return volt;
	}
}
EXPORT_SYMBOL_GPL(bq2589x_adc_read_battery_volt);


int bq2589x_adc_read_sys_volt(struct bq2589x *bq)
{
	uint8_t val;
	int volt;
	int ret;
	ret = bq2589x_read_byte(bq, &val, BQ2589X_REG_0F);
	if (ret < 0) {
		bq_dbg(PR_OEM, "read system voltage failed :%d\n", ret);
		return ret;
	} else{
		volt = BQ2589X_SYSV_BASE + ((val & BQ2589X_SYSV_MASK) >> BQ2589X_SYSV_SHIFT) * BQ2589X_SYSV_LSB ;
		return volt;
	}
}
EXPORT_SYMBOL_GPL(bq2589x_adc_read_sys_volt);

int bq2589x_adc_read_vbus_volt(struct bq2589x *bq)
{
	uint8_t val;
	int volt;
	int ret;
	ret = bq2589x_read_byte(bq, &val, BQ2589X_REG_11);
	if (ret < 0) {
		bq_dbg(PR_OEM, "read vbus voltage failed :%d\n", ret);
		return ret;
	} else{
		volt = BQ2589X_VBUSV_BASE + ((val & BQ2589X_VBUSV_MASK) >> BQ2589X_VBUSV_SHIFT) * BQ2589X_VBUSV_LSB ;
		return volt;
	}
}
EXPORT_SYMBOL_GPL(bq2589x_adc_read_vbus_volt);

int bq2589x_adc_read_temperature(struct bq2589x *bq)
{
	uint8_t val;
	int temp;
	int ret;
	ret = bq2589x_read_byte(bq, &val, BQ2589X_REG_10);
	if (ret < 0) {
		bq_dbg(PR_OEM, "read temperature failed :%d\n", ret);
		return ret;
	} else{
		temp = BQ2589X_TSPCT_BASE + ((val & BQ2589X_TSPCT_MASK) >> BQ2589X_TSPCT_SHIFT) * BQ2589X_TSPCT_LSB ;
		return temp;
	}
}
EXPORT_SYMBOL_GPL(bq2589x_adc_read_temperature);

int bq2589x_adc_read_charge_current(struct bq2589x *bq)
{
	uint8_t val;
	int volt;
	int ret;
	ret = bq2589x_read_byte(bq, &val, BQ2589X_REG_12);
	if (ret < 0) {
		bq_dbg(PR_OEM, "read charge current failed :%d\n", ret);
		return ret;
	} else{
		volt = (int)(BQ2589X_ICHGR_BASE + ((val & BQ2589X_ICHGR_MASK) >> BQ2589X_ICHGR_SHIFT) * BQ2589X_ICHGR_LSB) ;
		return volt;
	}
}
EXPORT_SYMBOL_GPL(bq2589x_adc_read_charge_current);

int bq2589x_set_charge_current(struct bq2589x *bq, int curr)
{
	u8 ichg;

	if (bq->part_no == SC89890H) {
		ichg = (curr - SC89890H_ICHG_BASE)/SC89890H_ICHG_LSB;
	} else {
		ichg = (curr - BQ2589X_ICHG_BASE)/BQ2589X_ICHG_LSB;
	}
	return bq2589x_update_bits(bq, BQ2589X_REG_04, BQ2589X_ICHG_MASK, ichg << BQ2589X_ICHG_SHIFT);

}
int bq2589x_get_charge_current(struct bq2589x *bq)
{
	u8 val;
	int ret = 0;

	ret = bq2589x_read_byte(bq, &val, BQ2589X_REG_04);
	if (ret < 0) {
		bq_dbg(PR_OEM, "%s Failed to read register 0x00:%d\n", __func__, ret);
		return ret;
	}
	return ((val & BQ2589X_ICHG_MASK) >> BQ2589X_ICHG_SHIFT) * BQ2589X_ICHG_LSB + BQ2589X_ICHG_BASE;

}

//EXPORT_SYMBOL_GPL(bq2589x_set_charge_current);

int bq2589x_set_term_current(struct bq2589x *bq, int curr)
{
	u8 iterm;	
	
	if (bq->part_no == SC89890H) {
		if (curr > SC89890H_ITERM_MAX) {
			curr = SC89890H_ITERM_MAX;
		}
		iterm = (curr - SC89890H_ITERM_BASE) / SC89890H_ITERM_LSB;
	} else {
		iterm = (curr - BQ2589X_ITERM_BASE) / BQ2589X_ITERM_LSB;
	}

	return bq2589x_update_bits(bq, BQ2589X_REG_05, BQ2589X_ITERM_MASK, iterm << BQ2589X_ITERM_SHIFT);
}
EXPORT_SYMBOL_GPL(bq2589x_set_term_current);


int bq2589x_set_prechg_current(struct bq2589x *bq, int curr)
{
	u8 iprechg;

	if (bq->part_no == SC89890H) {
		iprechg = (curr - SC89890H_IPRECHG_BASE) / SC89890H_IPRECHG_LSB;
	} else {
		iprechg = (curr - BQ2589X_IPRECHG_BASE) / BQ2589X_IPRECHG_LSB;
	}

	return bq2589x_update_bits(bq, BQ2589X_REG_05, BQ2589X_IPRECHG_MASK, iprechg << BQ2589X_IPRECHG_SHIFT);
}
EXPORT_SYMBOL_GPL(bq2589x_set_prechg_current);

int bq2589x_set_chargevoltage(struct bq2589x *bq, int volt)
{
	u8 val;

	val = (volt - BQ2589X_VREG_BASE)/BQ2589X_VREG_LSB;
	return bq2589x_update_bits(bq, BQ2589X_REG_06, BQ2589X_VREG_MASK, val << BQ2589X_VREG_SHIFT);
}
EXPORT_SYMBOL_GPL(bq2589x_set_chargevoltage);

int bq2589x_get_chargevoltage(struct bq2589x *bq)
{
	u8 val;
	int ret = 0;

	ret = bq2589x_read_byte(bq, &val, BQ2589X_REG_06);
	if (ret < 0) {
		bq_dbg(PR_OEM, "%s Failed to read register 0x00:%d\n", __func__, ret);
		return ret;
	}
	return ((val & BQ2589X_VREG_MASK) >> BQ2589X_VREG_SHIFT) * BQ2589X_VREG_LSB + BQ2589X_VREG_BASE;

}
EXPORT_SYMBOL_GPL(bq2589x_get_chargevoltage);

int main_set_charge_voltage(int volt)
{
	int ret = 0;
	
	if (!g_bq)
		return -1;
	ret = bq2589x_set_chargevoltage(g_bq, volt);
	
	bq_dbg(PR_OEM, "end main_set_charge_voltage ret=%d\n", ret);
	return ret;
}

int bq2589x_set_input_volt_limit(struct bq2589x *bq, int volt)
{
	u8 val;
	val = (volt - BQ2589X_VINDPM_BASE) / BQ2589X_VINDPM_LSB;
	return bq2589x_update_bits(bq, BQ2589X_REG_0D, BQ2589X_VINDPM_MASK, val << BQ2589X_VINDPM_SHIFT);
}
EXPORT_SYMBOL_GPL(bq2589x_set_input_volt_limit);

int bq2589x_set_input_current_limit(struct bq2589x *bq, int curr)
{
	u8 val;
	if (curr < BQ2589X_IINLIM_BASE)
		curr = BQ2589X_IINLIM_BASE;

	val = (curr - BQ2589X_IINLIM_BASE) / BQ2589X_IINLIM_LSB;
	return bq2589x_update_bits(bq, BQ2589X_REG_00, BQ2589X_IINLIM_MASK, val << BQ2589X_IINLIM_SHIFT);
}
EXPORT_SYMBOL_GPL(bq2589x_set_input_current_limit);

int bq2589x_get_input_current_limit(struct bq2589x *bq)
{
	u8 val;
	int ret = 0;

	ret = bq2589x_read_byte(bq, &val, BQ2589X_REG_00);
	if (ret < 0) {
		bq_dbg(PR_OEM, "%s Failed to read register 0x00:%d\n", __func__, ret);
		return ret;
	}
	return ((val & BQ2589X_IINLIM_MASK) >> BQ2589X_IINLIM_SHIFT) * BQ2589X_IINLIM_LSB + BQ2589X_IINLIM_BASE;
}
EXPORT_SYMBOL_GPL(bq2589x_get_input_current_limit);

int bq2589x_set_vindpm_offset(struct bq2589x *bq, int offset)
{
	u8 val;

	if (bq->part_no == SC89890H) {
		if (offset < 500) {
			val = SC89890h_VINDPMOS_400MV;
		} else {
			val = SC89890h_VINDPMOS_600MV; 
		}
		return bq2589x_update_bits(bq, BQ2589X_REG_01, SC89890H_VINDPMOS_MASK, val << SC89890H_VINDPMOS_SHIFT);
	} else {
		val = (offset - BQ2589X_VINDPMOS_BASE)/BQ2589X_VINDPMOS_LSB;
		return bq2589x_update_bits(bq, BQ2589X_REG_01, BQ2589X_VINDPMOS_MASK, val << BQ2589X_VINDPMOS_SHIFT);
	}

	return 0;
}
EXPORT_SYMBOL_GPL(bq2589x_set_vindpm_offset);

u8 bq2589x_get_charging_status(struct bq2589x *bq)
{
	u8 val = 0;
	int ret, cap = -1;
	union power_supply_propval propval ={0, };

	if(!bq)
		return POWER_SUPPLY_STATUS_UNKNOWN;
	ret = bq2589x_read_byte(bq, &val, BQ2589X_REG_0B);
	if (ret < 0) {
		bq_dbg(PR_OEM, "%s Failed to read register 0x0b:%d\n", __func__, ret);
		return POWER_SUPPLY_STATUS_UNKNOWN;
	}

	val &= BQ2589X_CHRG_STAT_MASK;
	val >>= BQ2589X_CHRG_STAT_SHIFT;
	if (val == BQ2589X_CHRG_STAT_IDLE){
		bq_dbg(PR_OEM, "not charging\n");
		return POWER_SUPPLY_STATUS_DISCHARGING;
	} else if (val == BQ2589X_CHRG_STAT_PRECHG){
		bq_dbg(PR_OEM, "precharging\n");
		return POWER_SUPPLY_STATUS_CHARGING;
	} else if (val == BQ2589X_CHRG_STAT_FASTCHG){
		bq_dbg(PR_OEM, "fast charging\n");
		return POWER_SUPPLY_STATUS_CHARGING;
	} else if (val == BQ2589X_CHRG_STAT_CHGDONE){
		bq_dbg(PR_OEM, "charge done!\n");
		if(!bq->bms_psy)
			bq->bms_psy = power_supply_get_by_name("bms");
		if(bq->bms_psy){
			ret = power_supply_get_property(bq->bms_psy, POWER_SUPPLY_PROP_CAPACITY, &propval);
			if (ret < 0) {
				pr_err("%s : get battery cap fail\n", __func__);
			}
			cap = propval.intval;
			pr_err("battery cap:%d\n", cap);
		}
		if(cap > 95)
			return POWER_SUPPLY_STATUS_FULL;
		else
			return POWER_SUPPLY_STATUS_CHARGING;
	}
	return POWER_SUPPLY_STATUS_UNKNOWN;
}
EXPORT_SYMBOL_GPL(bq2589x_get_charging_status);

void bq2589x_set_otg(struct bq2589x *bq, int enable)
{
	int ret;

	if (enable) {
		if (bq->part_no == SC89890H) {
			bq2589x_disable_charger(bq);
		}
		ret = bq2589x_enable_otg(bq);
		if (ret < 0) {
			bq_dbg(PR_OEM, "%s:Failed to enable otg-%d\n", __func__, ret);
			return;
		}
	} else{
		ret = bq2589x_disable_otg(bq);
		if (ret < 0)
			bq_dbg(PR_OEM, "%s:Failed to disable otg-%d\n", __func__, ret);
		if (bq->part_no == SC89890H) {
			bq2589x_enable_charger(bq);
		}
	}
}
EXPORT_SYMBOL_GPL(bq2589x_set_otg);

int bq2589x_disable_watchdog_timer(struct bq2589x *bq)
{
	u8 val = BQ2589X_WDT_DISABLE << BQ2589X_WDT_SHIFT;

	return bq2589x_update_bits(bq, BQ2589X_REG_07, BQ2589X_WDT_MASK, val);
}
int bq2589x_set_watchdog_timer(struct bq2589x *bq, u8 timeout)
{
	return bq2589x_update_bits(bq, BQ2589X_REG_07, BQ2589X_WDT_MASK, (u8)((timeout - BQ2589X_WDT_BASE) / BQ2589X_WDT_LSB) << BQ2589X_WDT_SHIFT);
}

int bq2589x_reset_watchdog_timer(struct bq2589x *bq)
{
	u8 val = BQ2589X_WDT_RESET << BQ2589X_WDT_RESET_SHIFT;

	return bq2589x_update_bits(bq, BQ2589X_REG_03, BQ2589X_WDT_RESET_MASK, val);
}


static  int bq2589x_is_dpdm_done(struct bq2589x *bq,int *done)
{
	int ret = 0;
	u8 data=0;
//modify by HTH-209427/HTH-209841 at 2022/05/12 begin
	if (bq->part_no == SC89890H) {
        ret = bq2589x_read_byte(bq, &data, BQ2589X_REG_0B);
        if (data & BQ2589X_PG_STAT_MASK) {
            *done = 0;
        } else {
            *done = 1;
        }
    } else {
        ret = bq2589x_read_byte(bq, &data, BQ2589X_REG_02);
        //pr_err("%s data(0x%x)\n",  __func__, data);
        data &= (BQ2589X_FORCE_DPDM << BQ2589X_FORCE_DPDM_SHIFT);
        *done = (data >> BQ2589X_FORCE_DPDM_SHIFT);
    }
	return ret;
//modify by HTH-209427/HTH-209841 at 2022/05/12 end
}

int bq2589x_force_dpdm(struct bq2589x *bq)
{
	u8 data = 0;
	u8 val = BQ2589X_FORCE_DPDM << BQ2589X_FORCE_DPDM_SHIFT;
//modify by HTH-209427/HTH-209841/HTH-234945/HTH-234948 at 2022/06/08 begin
    if (bq->part_no == SC89890H && bq->vbus_type == BQ2589X_VBUS_MAXC) {
		bq2589x_read_byte(bq, &data, BQ2589X_REG_0B);
		bq_dbg(PR_OEM, "bq2589x_force_dpdm 0x0B = 0x%02x\n",data);
		if ((data & 0xE0) == 0x80){
			bq2589x_write_byte(bq, BQ2589X_REG_01, 0x45);
			msleep(30);
			bq2589x_write_byte(bq, BQ2589X_REG_01, 0x25);
			msleep(30);
		}
	}
//modify by HTH-209427/HTH-209841/HTH-234945/HTH-234948 at 2022/06/08 end
	return bq2589x_update_bits(bq, BQ2589X_REG_02, BQ2589X_FORCE_DPDM_MASK, val);
}

void bq2589x_force_dpdm_done(struct bq2589x *bq)
{
     int retry = 0;
     int bc_count = 200;
     int done = 1;
     
     bq2589x_force_dpdm(bq);
  
     while(retry++ < bc_count){
       bq2589x_is_dpdm_done(bq,&done);
       msleep(20);
       if(!done) //already known charger type
         break;
     }
}


int bq2589x_reset_chip(struct bq2589x *bq)
{
	int ret;
	u8 val = BQ2589X_RESET << BQ2589X_RESET_SHIFT;

	ret = bq2589x_update_bits(bq, BQ2589X_REG_14, BQ2589X_RESET_MASK, val);
	return ret;
}
EXPORT_SYMBOL_GPL(bq2589x_reset_chip);

int bq2589x_enter_ship_mode(struct bq2589x *bq)
{
	int ret;
	u8 val = BQ2589X_BATFET_OFF << BQ2589X_BATFET_DIS_SHIFT;

	ret = bq2589x_update_bits(bq, BQ2589X_REG_09, BQ2589X_BATFET_DIS_MASK, val);
	return ret;

}
EXPORT_SYMBOL_GPL(bq2589x_enter_ship_mode);

int bq2589x_enter_hiz_mode(struct bq2589x *bq)
{
	u8 val = BQ2589X_HIZ_ENABLE << BQ2589X_ENHIZ_SHIFT;

	return bq2589x_update_bits(bq, BQ2589X_REG_00, BQ2589X_ENHIZ_MASK, val);

}
EXPORT_SYMBOL_GPL(bq2589x_enter_hiz_mode);

int bq2589x_exit_hiz_mode(struct bq2589x *bq)
{

	u8 val = BQ2589X_HIZ_DISABLE << BQ2589X_ENHIZ_SHIFT;

	return bq2589x_update_bits(bq, BQ2589X_REG_00, BQ2589X_ENHIZ_MASK, val);

}
EXPORT_SYMBOL_GPL(bq2589x_exit_hiz_mode);

int bq2589x_get_hiz_mode(struct bq2589x *bq, u8 *state)
{
	u8 val;
	int ret;

	ret = bq2589x_read_byte(bq, &val, BQ2589X_REG_00);
	if (ret)
		return ret;
	*state = (val & BQ2589X_ENHIZ_MASK) >> BQ2589X_ENHIZ_SHIFT;

	return 0;
}
EXPORT_SYMBOL_GPL(bq2589x_get_hiz_mode);


int bq2589x_pumpx_enable(struct bq2589x *bq, int enable)
{
	u8 val;
	int ret;

	if (enable)
		val = BQ2589X_PUMPX_ENABLE << BQ2589X_EN_PUMPX_SHIFT;
	else
		val = BQ2589X_PUMPX_DISABLE << BQ2589X_EN_PUMPX_SHIFT;

	ret = bq2589x_update_bits(bq, BQ2589X_REG_04, BQ2589X_EN_PUMPX_MASK, val);

	return ret;
}
EXPORT_SYMBOL_GPL(bq2589x_pumpx_enable);

int bq2589x_pumpx_increase_volt(struct bq2589x *bq)
{
	u8 val;
	int ret;

	val = BQ2589X_PUMPX_UP << BQ2589X_PUMPX_UP_SHIFT;

	ret = bq2589x_update_bits(bq, BQ2589X_REG_09, BQ2589X_PUMPX_UP_MASK, val);

	return ret;

}
EXPORT_SYMBOL_GPL(bq2589x_pumpx_increase_volt);

int bq2589x_pumpx_increase_volt_done(struct bq2589x *bq)
{
	u8 val;
	int ret;

	ret = bq2589x_read_byte(bq, &val, BQ2589X_REG_09);
	if (ret)
		return ret;

	if (val & BQ2589X_PUMPX_UP_MASK)
		return 1;   /* not finished*/
	else
		return 0;   /* pumpx up finished*/

}
EXPORT_SYMBOL_GPL(bq2589x_pumpx_increase_volt_done);

int bq2589x_pumpx_decrease_volt(struct bq2589x *bq)
{
	u8 val;
	int ret;

	val = BQ2589X_PUMPX_DOWN << BQ2589X_PUMPX_DOWN_SHIFT;

	ret = bq2589x_update_bits(bq, BQ2589X_REG_09, BQ2589X_PUMPX_DOWN_MASK, val);

	return ret;

}
EXPORT_SYMBOL_GPL(bq2589x_pumpx_decrease_volt);

int bq2589x_pumpx_decrease_volt_done(struct bq2589x *bq)
{
	u8 val;
	int ret;

	ret = bq2589x_read_byte(bq, &val, BQ2589X_REG_09);
	if (ret)
		return ret;

	if (val & BQ2589X_PUMPX_DOWN_MASK)
		return 1;   /* not finished*/
	else
		return 0;   /* pumpx down finished*/

}
EXPORT_SYMBOL_GPL(bq2589x_pumpx_decrease_volt_done);

static int bq2589x_force_ico(struct bq2589x *bq)
{
	u8 val;
	int ret;

	val = BQ2589X_FORCE_ICO << BQ2589X_FORCE_ICO_SHIFT;

	ret = bq2589x_update_bits(bq, BQ2589X_REG_09, BQ2589X_FORCE_ICO_MASK, val);

	return ret;
}
EXPORT_SYMBOL_GPL(bq2589x_force_ico);

static int bq2589x_check_force_ico_done(struct bq2589x *bq)
{
	u8 val;
	int ret;

	ret = bq2589x_read_byte(bq, &val, BQ2589X_REG_14);
	if (ret)
		return ret;

	if (val & BQ2589X_ICO_OPTIMIZED_MASK)
		return 1;  /*finished*/
	else
		return 0;   /* in progress*/
}
EXPORT_SYMBOL_GPL(bq2589x_check_force_ico_done);

static int bq2589x_enable_term(struct bq2589x* bq, bool enable)
{
	u8 val;
	int ret;

	if (enable)
		val = BQ2589X_TERM_ENABLE << BQ2589X_EN_TERM_SHIFT;
	else
		val = BQ2589X_TERM_DISABLE << BQ2589X_EN_TERM_SHIFT;

	ret = bq2589x_update_bits(bq, BQ2589X_REG_07, BQ2589X_EN_TERM_MASK, val);

	return ret;
}
EXPORT_SYMBOL_GPL(bq2589x_enable_term);

static int bq2589x_enable_auto_dpdm(struct bq2589x* bq, bool enable)
{
	u8 val;
	int ret;
	
	if (enable)
		val = BQ2589X_AUTO_DPDM_ENABLE << BQ2589X_AUTO_DPDM_EN_SHIFT;
	else
		val = BQ2589X_AUTO_DPDM_DISABLE << BQ2589X_AUTO_DPDM_EN_SHIFT;

	ret = bq2589x_update_bits(bq, BQ2589X_REG_02, BQ2589X_AUTO_DPDM_EN_MASK, val);

	return ret;

}
EXPORT_SYMBOL_GPL(bq2589x_enable_auto_dpdm);

static int bq2589x_use_absolute_vindpm(struct bq2589x* bq, bool enable)
{
	u8 val;
	int ret;
	
	if (enable)
		val = BQ2589X_FORCE_VINDPM_ENABLE << BQ2589X_FORCE_VINDPM_SHIFT;
	else
		val = BQ2589X_FORCE_VINDPM_DISABLE << BQ2589X_FORCE_VINDPM_SHIFT;

	ret = bq2589x_update_bits(bq, BQ2589X_REG_0D, BQ2589X_FORCE_VINDPM_MASK, val);



	return ret;

}
EXPORT_SYMBOL_GPL(bq2589x_use_absolute_vindpm);

static int bq2589x_enable_ico(struct bq2589x* bq, bool enable)
{
	u8 val;
	int ret;
	
	if (enable)
		val = BQ2589X_ICO_ENABLE << BQ2589X_ICOEN_SHIFT;
	else
		val = BQ2589X_ICO_DISABLE << BQ2589X_ICOEN_SHIFT;

	ret = bq2589x_update_bits(bq, BQ2589X_REG_02, BQ2589X_ICOEN_MASK, val);

	return ret;

}
EXPORT_SYMBOL_GPL(bq2589x_enable_ico);


static int bq2589x_read_idpm_limit(struct bq2589x *bq)
{
	uint8_t val;
	int curr;
	int ret;

	ret = bq2589x_read_byte(bq, &val, BQ2589X_REG_13);
	if (ret < 0) {
		bq_dbg(PR_OEM, "read vbus voltage failed :%d\n", ret);
		return ret;
	} else{
		curr = BQ2589X_IDPM_LIM_BASE + ((val & BQ2589X_IDPM_LIM_MASK) >> BQ2589X_IDPM_LIM_SHIFT) * BQ2589X_IDPM_LIM_LSB ;
		return curr;
	}
}
EXPORT_SYMBOL_GPL(bq2589x_read_idpm_limit);

bool bq2589x_is_charge_done(void)
{
	int ret;
	u8 val;

	ret = bq2589x_read_byte(g_bq, &val, BQ2589X_REG_0B);
	if (ret < 0) {
		bq_dbg(PR_OEM, "%s:read REG0B failed :%d\n", __func__, ret);
		return false;
	}
	val &= BQ2589X_CHRG_STAT_MASK;
	val >>= BQ2589X_CHRG_STAT_SHIFT;

	return (val == BQ2589X_CHRG_STAT_CHGDONE);
}
EXPORT_SYMBOL_GPL(bq2589x_is_charge_done);

#if 1
static void bq2589x_dump_regs(struct bq2589x *bq)
{
	int addr, ret;
	u8 val;

	bq_dbg(PR_OEM, "bq2589x_dump_regs: ");
	for (addr = 0x0; addr <= 0x14; addr++) {
		ret = bq2589x_read_byte(bq, &val, addr);
		if (ret == 0)
			bq_dbg(PR_OEM, "Reg[%.2x] = 0x%.2x ", addr, val);
	}
}
#endif

static int bq2589x_init_device(struct bq2589x *bq)
{
	int ret;

    /*common initialization*/
	if (bq->part_no == SC89890H) {
		bq2589x_update_bits(bq, BQ2589X_REG_00, BQ2589X_ENILIM_MASK,
				BQ2589X_ENILIM_DISABLE << BQ2589X_ENILIM_SHIFT);
//modify by HTH-209427/HTH-209841 at 2022/05/12 begin
		bq2589x_enable_ico(bq, false);
	}else{
//modify by HTH-234718 at 2022/05/25 begin
		bq2589x_enable_ico(bq, false);
//modify by HTH-234718 at 2022/05/25 end
	}
//modify by HTH-209427/HTH-209841 at 2022/05/12 end
	bq2589x_disable_watchdog_timer(bq);

	bq2589x_enable_auto_dpdm(bq, bq->cfg.enable_auto_dpdm);
	bq2589x_enable_term(bq, bq->cfg.enable_term);
	/*force use absolute vindpm if auto_dpdm not enabled*/
	if (!bq->cfg.enable_auto_dpdm)
		bq->cfg.use_absolute_vindpm = true;
	bq2589x_use_absolute_vindpm(bq, bq->cfg.use_absolute_vindpm);

	ret = bq2589x_set_vindpm_offset(bq, 600);
	if (ret < 0) {
		bq_dbg(PR_OEM, "%s:Failed to set vindpm offset:%d\n", __func__, ret);
		return ret;
	}

	ret = bq2589x_set_term_current(bq, bq->cfg.term_current);
	if (ret < 0) {
		bq_dbg(PR_OEM, "%s:Failed to set termination current:%d\n", __func__, ret);
		return ret;
	}

	ret = bq2589x_set_chargevoltage(bq, bq->cfg.charge_voltage);
	if (ret < 0) {
		bq_dbg(PR_OEM, "%s:Failed to set charge voltage:%d\n", __func__, ret);
		return ret;
	}

	main_set_charge_enable(true);
	//bq2589x_adc_start(bq, false);
	if (ret) {
		bq_dbg(PR_OEM, "%s:Failed to enable pumpx:%d\n", __func__, ret);
		return ret;
	}

	//bq2589x_set_watchdog_timer(bq, 160);
	bq2589x_update_bits(bq, BQ2589X_REG_00, BQ2589X_ENILIM_MASK, BQ2589X_ENILIM_DISABLE << BQ2589X_ENILIM_SHIFT);
	bq2589x_update_bits(bq, BQ2589X_REG_02, 0x8, 1 << 3);

	/* 2022.5.18 longcheer tangyanchang edit start */
	if (bq->part_no == SYV690) {
		bq_dbg(PR_OEM, "%s:init syv690 HV_TYPE 9/12V \n", __func__);
		bq2589x_update_bits(bq, BQ2589X_REG_02, 0x4, 0 << 2);//HV_TYPE 0-9V/1-12V
	}
	/* 2022.5.18 longcheer tangyanchang edit end */

	//bq2589x_update_bits(bq, BQ2589X_REG_01, 0x2, 0 << 1);
	
        bq2589x_adc_stop(bq);
	return ret;
}

#if 1
static int bq2589x_charge_status(struct bq2589x *bq)
{
	u8 val = 0;

	bq2589x_read_byte(bq, &val, BQ2589X_REG_0B);
	bq_dbg(PR_OEM, "get charge status:0x%x\n", val);

	val &= BQ2589X_CHRG_STAT_MASK;
	val >>= BQ2589X_CHRG_STAT_SHIFT;
	switch (val) {
	case BQ2589X_CHRG_STAT_FASTCHG:
		return POWER_SUPPLY_CHARGE_TYPE_FAST;
	case BQ2589X_CHRG_STAT_PRECHG:
		return POWER_SUPPLY_CHARGE_TYPE_TRICKLE;
	case BQ2589X_CHRG_STAT_CHGDONE:
	case BQ2589X_CHRG_STAT_IDLE:
		return POWER_SUPPLY_CHARGE_TYPE_NONE;
	default:
		return POWER_SUPPLY_CHARGE_TYPE_UNKNOWN;
	}
}
#endif

static enum power_supply_property bq2589x_charger_props[] = {
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_CHARGE_TYPE, /* Charger status output */
	POWER_SUPPLY_PROP_ONLINE, /* External power source */
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_CURRENT_NOW,
	POWER_SUPPLY_PROP_CHARGE_ENABLED,
	POWER_SUPPLY_PROP_TERM_CURRENT,
	POWER_SUPPLY_PROP_BATT_CHARGE_TYPE,
};
extern int get_prop_battery_charging_enabled(struct votable *usb_icl_votable,
					union power_supply_propval *val);

static int bq2589x_wall_get_property(struct power_supply *psy,
				enum power_supply_property psp,
				union power_supply_propval *val)
{
	struct bq2589x *bq = power_supply_get_drvdata(psy);

	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
		val->intval = bq2589x_get_charging_status(bq);
#if 0
		if (get_effective_result_locked(bq->fv_votable) < 4450) {
			if (val->intval == POWER_SUPPLY_STATUS_FULL) {
				val->intval = POWER_SUPPLY_STATUS_CHARGING;
			}
		} else if (get_client_vote_locked(bq->usb_icl_votable, "MAIN_CHG_SUSPEND_VOTER") == MAIN_ICL_MIN) {
			val->intval = POWER_SUPPLY_STATUS_CHARGING;
		}
#endif
		break;
	case POWER_SUPPLY_PROP_ONLINE:
		val->intval = bq->chg_online;
		if (bq->vbat_volt < 3300)
			val->intval = 0;
		break;
	case POWER_SUPPLY_PROP_CHARGE_TYPE:
		val->intval = bq->chg_type;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		val->intval = bq->vbus_volt;
		break;
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		val->intval = bq->chg_current;
		break;
	case POWER_SUPPLY_PROP_CHARGE_ENABLED:
		val->intval = bq->enabled;
		break;
	case POWER_SUPPLY_PROP_TERM_CURRENT:
		val->intval = bq->cfg.term_current;
		break;
	case POWER_SUPPLY_PROP_BATT_CHARGE_TYPE:
		val->intval =  bq2589x_charge_status(bq);
		bq_dbg(PR_OEM, " get_property CHARGE_TYPE :%d\n", val->intval);
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int bq2589x_wall_set_property(struct power_supply *psy,
				enum power_supply_property psp,
				const union power_supply_propval *val)
{
	int ret = 0;
	struct bq2589x *bq = power_supply_get_drvdata(psy);

	switch (psp) {
	case POWER_SUPPLY_PROP_CHARGE_TYPE:
		bq->chg_type = val->intval; 
		break;
	case POWER_SUPPLY_PROP_ONLINE:
		bq->chg_online = val->intval;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		bq->vbus_volt = val->intval;
		ret = main_set_charge_voltage(bq->vbus_volt);
		break;
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		bq->chg_current = val->intval;
		ret = main_set_charge_current(bq->chg_current);
		break;
	case POWER_SUPPLY_PROP_CHARGE_ENABLED:
		bq->enabled = val->intval;
		ret = main_set_charge_enable(bq->enabled);
		break;
	case POWER_SUPPLY_PROP_TERM_CURRENT:
		bq->cfg.term_current = val->intval;
		ret = bq2589x_set_term_current(bq, bq->cfg.term_current);
		break;
	default:
		return -EINVAL;
	}

	return ret;
}

static int bq2589x_wall_prop_is_writeable(struct power_supply *psy,
				enum power_supply_property psp)
{
	switch (psp) {
	case POWER_SUPPLY_PROP_CHARGE_TYPE:
	case POWER_SUPPLY_PROP_ONLINE:
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
	case POWER_SUPPLY_PROP_CURRENT_NOW:
	case POWER_SUPPLY_PROP_CHARGE_ENABLED:
		return 1;
	default:
		break;
	}
	return 0;
}

static int bq2589x_psy_register(struct bq2589x *bq)
{
	int ret;

	bq->wall.name = "bbc";//"bq2589x-wall";
	bq->wall.type = POWER_SUPPLY_TYPE_MAINS;
	bq->wall.properties = bq2589x_charger_props;
	bq->wall.num_properties = ARRAY_SIZE(bq2589x_charger_props);
	bq->wall.get_property = bq2589x_wall_get_property;
	bq->wall.set_property = bq2589x_wall_set_property;
	bq->wall.property_is_writeable = bq2589x_wall_prop_is_writeable;
	bq->wall.external_power_changed = NULL;

	bq->wall_cfg.drv_data = bq;
	bq->wall_cfg.of_node = bq->dev->of_node;

	bq->wall_psy = devm_power_supply_register(bq->dev, &bq->wall, &bq->wall_cfg);
	if (IS_ERR(bq->wall_psy)) {
		ret = PTR_ERR(bq->wall_psy);
		bq_dbg(PR_OEM, "%s:failed to register wall psy:%d\n", __func__, ret);
	}

	return 0;
}

static void bq2589x_psy_unregister(struct bq2589x *bq)
{
	pr_err("%s  \n", __func__);
}

static ssize_t bq2589x_show_registers(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	u8 addr;
	u8 val;
	u8 tmpbuf[300];
	int len;
	int idx = 0;
	int ret ;

	idx = snprintf(buf, PAGE_SIZE, "%s:\n", "Charger 1");
	for (addr = 0x0; addr <= 0x14; addr++) {
		ret = bq2589x_read_byte(g_bq, &val, addr);
		if (ret == 0) {
			len = snprintf(tmpbuf, PAGE_SIZE - idx,"Reg[0x%.2x] = 0x%.2x\n", addr, val);
			memcpy(&buf[idx], tmpbuf, len);
			idx += len;
		}
	}

	return idx;
}

static DEVICE_ATTR(registers, S_IRUGO, bq2589x_show_registers, NULL);

static struct attribute *bq2589x_attributes[] = {
	&dev_attr_registers.attr,
	NULL,
};

static const struct attribute_group bq2589x_attr_group = {
	.attrs = bq2589x_attributes,
};


static int bq2589x_parse_dt(struct device *dev, struct bq2589x *bq)
{
	int ret;
	struct device_node *np = dev->of_node;

	ret = of_property_read_u32(np, "ti,bq2589x,vbus-volt-high-level", &pe.high_volt_level);
	if (ret)
		return ret;

	ret = of_property_read_u32(np, "ti,bq2589x,vbus-volt-low-level", &pe.low_volt_level);
	if (ret)
		return ret;

	ret = of_property_read_u32(np, "ti,bq2589x,vbat-min-volt-to-tuneup", &pe.vbat_min_volt);
	if (ret)
		return ret;

	bq->cfg.enable_auto_dpdm = of_property_read_bool(np, "ti,bq2589x,enable-auto-dpdm");
	bq->cfg.enable_term = of_property_read_bool(np, "ti,bq2589x,enable-termination");
	bq->cfg.enable_ico = of_property_read_bool(np, "ti,bq2589x,enable-ico");
	bq->cfg.use_absolute_vindpm = of_property_read_bool(np, "ti,bq2589x,use-absolute-vindpm");

	ret = of_property_read_u32(np, "ti,bq2589x,charge-voltage",&bq->cfg.charge_voltage);
	if (ret)
		return ret;

	ret = of_property_read_u32(np, "ti,bq2589x,charge-current",&bq->cfg.charge_current);
	if (ret)
		return ret;

	ret = of_property_read_u32(np, "ti,bq2589x,charge-current-3500",&bq->cfg.charge_current_3500);
	if (ret)
		return ret;

	ret = of_property_read_u32(np, "ti,bq2589x,charge-current-1500",&bq->cfg.charge_current_1500);
	if (ret)
		return ret;

	ret = of_property_read_u32(np, "ti,bq2589x,charge-current-1000",&bq->cfg.charge_current_1000);
	if (ret)
		return ret;

	ret = of_property_read_u32(np, "ti,bq2589x,charge-current-500",&bq->cfg.charge_current_500);
	if (ret)
		return ret;

	ret = of_property_read_u32(np, "ti,bq2589x,input-current-2000",&bq->cfg.input_current_2000);
	if (ret)
		return ret;

	ret = of_property_read_u32(np, "ti,bq2589x,term-current",&bq->cfg.term_current);
	if (ret)
		return ret;

	bq->irq_gpio = of_get_named_gpio(np, "intr-gpio", 0);
        if (ret < 0) {
                bq_dbg(PR_OEM, "%s no intr_gpio info\n", __func__);
                return ret;
        } else {
                bq_dbg(PR_OEM, "%s intr_gpio infoi %d\n", __func__, bq->irq_gpio);
	}
	bq->usb_switch1 = of_get_named_gpio(np, "usb-switch1", 0);
        if (ret < 0) {
                bq_dbg(PR_OEM, "%s no usb-switch1 info\n", __func__);
                return ret;
	}
	return 0;
}

static void bq2589x_usb_switch(struct bq2589x *bq, bool en)
{
	int ret = 0;
	//msleep(5);
	pr_info("%s:%d\n", __func__, en);
	mutex_lock(&bq->usb_switch_lock);
	ret = gpio_direction_output(bq->usb_switch1, en);
	bq->usb_switch_flag = en;
	mutex_unlock(&bq->usb_switch_lock);
}


static int bq2589x_detect_device(struct bq2589x *bq)
{
	int ret;
	u8 data;

	ret = bq2589x_read_byte(bq, &data, BQ2589X_REG_14);
	if (ret == 0) {
		bq->part_no = (data & BQ2589X_PN_MASK) >> BQ2589X_PN_SHIFT;
		bq->revision = (data & BQ2589X_DEV_REV_MASK) >> BQ2589X_DEV_REV_SHIFT;
	}

	return ret;
}

static int bq2589x_read_batt_rsoc(struct bq2589x *bq)
{
	union power_supply_propval ret = {0,};

	if (!bq->batt_psy) 
		bq->batt_psy = power_supply_get_by_name("battery");

	if (bq->batt_psy) {
		power_supply_get_property(bq->batt_psy, POWER_SUPPLY_PROP_CAPACITY, &ret);
		return ret.intval;
	} else {
		return 50;
	}
}

static void bq2589x_adjust_absolute_vindpm(struct bq2589x *bq)
{
	u16 vbus_volt = 0;
	u16 vindpm_volt = 0;
	int ret = 0;

	vbus_volt = bq2589x_adc_read_vbus_volt(bq);
	if (vbus_volt < 6000)
		vindpm_volt = 4500; //vindpm for 5v charger
	else
		vindpm_volt = 8300; //vindpm for 9v+ charger

	ret = bq2589x_set_input_volt_limit(bq, vindpm_volt);
	if (ret < 0)
		bq_dbg(PR_OEM, "Set absolute vindpm threshold %d Failed:%d\n", vindpm_volt, ret);
	else
		bq_dbg(PR_OEM, "Set absolute vindpm threshold %d successfully\n", vindpm_volt);
}

int main_set_charge_enable(bool en)
{
	int ret = 0;
	if (!g_bq)
		return -1;
	bq_dbg(PR_OEM, "start set_charge_enable:%d\n", en);
	if(en)
		ret = bq2589x_enable_charger(g_bq);
	else
		ret = bq2589x_disable_charger(g_bq);

	bq_dbg(PR_OEM, "end set_charge_enable ret = %d\n", ret);
	return ret;
}

int main_set_hiz_mode(bool en)
{
	if (!g_bq)
		return -1;

	if(en)
		bq2589x_enter_hiz_mode(g_bq);
	else
		bq2589x_exit_hiz_mode(g_bq);
	return 0;
}

int main_set_input_current_limit(int curr)
{
	if (!g_bq)
		return -1;
	bq2589x_set_input_current_limit(g_bq, curr);
	return 0;
}

int main_set_charge_current(int curr)
{
	if (!g_bq)
		return -1;
	vote(g_bq->fcc_votable, MAIN_SET_VOTER, true, curr);
	
	bq_dbg(PR_OEM, "end main_set_charge_current\n");
	return 0;
}

int main_get_charge_type(void)
{
	u8 type;
	if (!g_bq) {
		return -1;
	}
	type = g_bq->vbus_type;//bq2589x_get_vbus_type(g_bq); 2021.09.11 wsy edit for crash
	return (int)type;
}


static void bq2589x_adapter_in_workfunc(struct work_struct *work)
{
	struct bq2589x *bq = container_of(work, struct bq2589x, adapter_in_work);
	int ret;
	union power_supply_propval propval ={0, };
//modify by HTH-209427/HTH-209841 at 2022/05/12 begin
	bq2589x_use_absolute_vindpm(bq, bq->cfg.use_absolute_vindpm);
//modify by HTH-209427/HTH-209841 at 2022/05/12 end
	bq2589x_adc_start(bq, false);
	switch(bq->vbus_type)
	{
		case BQ2589X_VBUS_MAXC:
			bq_dbg(PR_OEM, "charger_type: MAXC\n");
			bq2589x_enable_ico(bq, !bq->cfg.enable_ico);
			vote(bq->usb_icl_votable, PROFILE_CHG_VOTER, true, 1800);
			if (bq->cfg.use_absolute_vindpm)
				bq2589x_adjust_absolute_vindpm(bq);
			//schedule_delayed_work(&bq->ico_work, 0);
			break;
		case BQ2589X_VBUS_USB_DCP:
			bq_dbg(PR_OEM, "charger_type: DCP,pd_active=%d\n", bq->pd_active);
			vote(bq->usb_icl_votable, PROFILE_CHG_VOTER, true, bq->cfg.input_current_2000);
			schedule_delayed_work(&bq->check_pe_tuneup_work, 0);
			break;
		case BQ2589X_VBUS_USB_CDP:
			bq_dbg(PR_OEM, "charger_type: CDP\n");
			vote(bq->usb_icl_votable, PROFILE_CHG_VOTER, true, 1500);
			msleep(1000);
			bq2589x_usb_switch(bq, false);
			break;
		case BQ2589X_VBUS_USB_SDP:
			bq_dbg(PR_OEM, "charger_type: SDP,pd_active=%d\n", bq->pd_active);

			if(!bq->usb_psy)
				bq->bms_psy = power_supply_get_by_name("usb");
			if (bq->usb_psy) {
				ret = power_supply_get_property(bq->usb_psy, POWER_SUPPLY_PROP_MTBF_CUR, &propval);
				if (ret < 0) {
					pr_err("%s : get mtbf current fail\n", __func__);
				}
			}

			if (!bq->pd_active) {
				if (propval.intval >= 1500)
					vote(bq->usb_icl_votable, PROFILE_CHG_VOTER, true, propval.intval);
				else
					vote(bq->usb_icl_votable, PROFILE_CHG_VOTER, true, 500);
			} else {
				ret = bq2589x_set_input_current_limit(bq, bq->cfg.charge_current_1500);
			}
			bq2589x_usb_switch(bq, false);
			break;
		case BQ2589X_VBUS_NONSTAND:
		case BQ2589X_VBUS_UNKNOWN:
			bq_dbg(PR_OEM, "charger_type: FLOAT,pd_active=%d\n", bq->pd_active);

			if(!bq->usb_psy)
				bq->bms_psy = power_supply_get_by_name("usb");
			if(bq->usb_psy){
				ret = power_supply_get_property(bq->usb_psy, POWER_SUPPLY_PROP_MTBF_CUR, &propval);
				if (ret < 0) {
					pr_err("%s : get mtbf current fail\n", __func__);
				}
			}

			if (!bq->pd_active) {
				if (propval.intval >= 1500)
					vote(bq->usb_icl_votable, PROFILE_CHG_VOTER, true, propval.intval);
				else
					vote(bq->usb_icl_votable, PROFILE_CHG_VOTER, true, 1000);
			} else {
				vote(bq->usb_icl_votable, PROFILE_CHG_VOTER, true, bq->cfg.input_current_2000);
			}
			bq2589x_usb_switch(bq, false); //HTH-191813
			break;
		default:
			bq_dbg(PR_OEM, "charger_type: Other vbus_type is %d\n", bq->vbus_type);
			bq2589x_usb_switch(bq, false);
			schedule_delayed_work(&bq->ico_work, 0);
			break;
	}


//	bq2589x_dump_regs(bq);
//	power_supply_changed(bq->usb_psy);
//	cancel_delayed_work_sync(&bq->monitor_work);	
	schedule_delayed_work(&bq->monitor_work, 0);
}

static void bq2589x_adapter_out_workfunc(struct work_struct *work)
{
	struct bq2589x *bq = container_of(work, struct bq2589x, adapter_out_work);
	int ret;
//modify by HTH-209427/HTH-209841 at 2022/05/12 begin
	ret = bq2589x_set_input_volt_limit(bq, 4600);
	vote(bq->usb_icl_votable, PROFILE_CHG_VOTER, true, 0);
	if (ret < 0)
		bq_dbg(PR_OEM,"%s:reset vindpm threshold to 4600 failed:%d\n",__func__,ret);
	else
		bq_dbg(PR_OEM,"%s:reset vindpm threshold to 4600 successfully\n",__func__);
//modify by HTH-209427/HTH-209841 at 2022/05/12 end
	cancel_delayed_work_sync(&bq->monitor_work);
}
static void bq2589x_charger_workfunc(struct work_struct *work)
{
	u8 type_now=0;
	struct bq2589x *bq = container_of(work, struct bq2589x, charger_work.work);
	if (!bq->batt_psy)
		return;
 
	type_now = bq2589x_get_charging_status(bq);
	if(type_now > 0) {
		power_supply_changed(bq->batt_psy);
	}
}

static void bq2589x_ico_workfunc(struct work_struct *work)
{
	struct bq2589x *bq = container_of(work, struct bq2589x, ico_work.work);
	int ret;
	int idpm;
	u8 status;
	static bool ico_issued;

	if (!ico_issued) {
		ret = bq2589x_force_ico(bq);
		if (ret < 0) {
			schedule_delayed_work(&bq->ico_work, HZ); /* retry 1 second later*/
		} else {
			ico_issued = true;
			schedule_delayed_work(&bq->ico_work, 3 * HZ);
		}
	} else {
		ico_issued = false;
		ret = bq2589x_check_force_ico_done(bq);
		if (ret) {/*ico done*/
			ret = bq2589x_read_byte(bq, &status, BQ2589X_REG_13);
			if (ret == 0) {
				idpm = ((status & BQ2589X_IDPM_LIM_MASK) >> BQ2589X_IDPM_LIM_SHIFT) * BQ2589X_IDPM_LIM_LSB + BQ2589X_IDPM_LIM_BASE;
			}
		}
	}
}

static void bq2589x_check_pe_tuneup_workfunc(struct work_struct *work)
{
	struct bq2589x *bq = container_of(work, struct bq2589x, check_pe_tuneup_work.work);

	if (!pe.enable) {
		schedule_delayed_work(&bq->ico_work, 0);
		return;
	}

	bq->vbat_volt = bq2589x_adc_read_battery_volt(bq);
	bq->rsoc = bq2589x_read_batt_rsoc(bq); 

	if (bq->vbat_volt > pe.vbat_min_volt && bq->rsoc < 95) {
		pe.target_volt = pe.high_volt_level;
		pe.tune_up_volt = true;
		pe.tune_down_volt = false;
		pe.tune_done = false;
		pe.tune_count = 0;
		pe.tune_fail = false;
		schedule_delayed_work(&bq->pe_volt_tune_work, 0);
	} else if (bq->rsoc >= 95) {
		schedule_delayed_work(&bq->ico_work, 0);
	} else {
		/* wait battery voltage up enough to check again */
		schedule_delayed_work(&bq->check_pe_tuneup_work, 2*HZ);
	}
}

//20220211 : Only for time delay
static void time_delay_work(struct work_struct *work)
{
	struct bq2589x *bq = container_of(work,
				struct bq2589x,
				time_delay_work.work);

	bq2589x_usb_switch(bq, true);
	bq2589x_force_dpdm_done(bq);

}

static void bq2589x_usb_changed_workfunc(struct work_struct *work)
{
	struct bq2589x *bq = container_of(work, struct bq2589x,usb_changed_work.work);
	static int notify_count = 0;
	union power_supply_propval val = {0, };
	int ret = 0 , chg_type = 0;
	if(!bq->usb_psy)
		bq->usb_psy = power_supply_get_by_name("usb");
	if(notify_count < NOTIFY_COUNT_MAX){
		if(bq->usb_psy){
			ret = power_supply_get_property(bq->usb_psy,POWER_SUPPLY_PROP_REAL_TYPE, &val);
			chg_type = val.intval;
			pr_err("chg_type = %d", chg_type);
			if(chg_type == POWER_SUPPLY_TYPE_USB_PD || chg_type == POWER_SUPPLY_TYPE_USB_HVDCP)
				power_supply_changed(bq->usb_psy);
		}
		schedule_delayed_work(&bq->usb_changed_work, HZ);
		notify_count++;
	}
}

static void bq2589x_tune_volt_workfunc(struct work_struct *work)
{
	struct bq2589x *bq = container_of(work, struct bq2589x, pe_volt_tune_work.work);
	int ret = 0;
	static bool pumpx_cmd_issued;

	bq->vbus_volt = bq2589x_adc_read_vbus_volt(bq);

	if ((pe.tune_up_volt && bq->vbus_volt > pe.target_volt) ||
	    (pe.tune_down_volt && bq->vbus_volt < pe.target_volt)) {
		pe.tune_done = true;
		bq2589x_adjust_absolute_vindpm(bq);
		if (pe.tune_up_volt)
			schedule_delayed_work(&bq->ico_work, 0);
		return;
	}

	if (pe.tune_count > 10) {
		pe.tune_fail = true;
		bq2589x_adjust_absolute_vindpm(bq);

		if (pe.tune_up_volt)
			schedule_delayed_work(&bq->ico_work, 0);
		return;
	}

	if (!pumpx_cmd_issued) {
		if (pe.tune_up_volt)
			ret = bq2589x_pumpx_increase_volt(bq);
		else if (pe.tune_down_volt)
			ret =  bq2589x_pumpx_decrease_volt(bq);
		if (ret) {
			schedule_delayed_work(&bq->pe_volt_tune_work, HZ);
		} else {
			pumpx_cmd_issued = true;
			pe.tune_count++;
			schedule_delayed_work(&bq->pe_volt_tune_work, 3*HZ);
		}
	} else {
		if (pe.tune_up_volt)
			ret = bq2589x_pumpx_increase_volt_done(bq);
		else if (pe.tune_down_volt)
			ret = bq2589x_pumpx_decrease_volt_done(bq);
		if (ret == 0) {
			bq2589x_adjust_absolute_vindpm(bq);
			pumpx_cmd_issued = 0;
		}
		schedule_delayed_work(&bq->pe_volt_tune_work, HZ);
	}
}


static void bq2589x_monitor_workfunc(struct work_struct *work)
{
	struct bq2589x *bq = container_of(work, struct bq2589x, monitor_work.work);
	u8 status = 0;
	int ret = 0;
	int rawfcc = 0, rawfv = 0, rawicl = 0;
	int batt_temp;
        union power_supply_propval propval ={0, };
#if 0
        int batt_temp, batt_voltage_now = 0;
        union power_supply_propval propval ={0, };
	if(!bq->bms_psy)
		bq->bms_psy = power_supply_get_by_name("bms");
	if(bq->bms_psy){
		ret = power_supply_get_property(bq->bms_psy, POWER_SUPPLY_PROP_TEMP, &propval);
		if (ret < 0) {
			pr_err("%s : get battery temp fail\n", __func__);
		}
		batt_temp = propval.intval;
                pr_err("%s : get battery temp :%d\n", __func__, batt_temp);
                ret = power_supply_get_property(bq->bms_psy, POWER_SUPPLY_PROP_VOLTAGE_NOW, &propval);
		if (ret < 0) {
			pr_err("%s : get battery voltage_now fail\n", __func__);
		}
		batt_voltage_now = propval.intval;
                pr_err("%s : get battery voltage_now :%d\n", __func__, batt_voltage_now);
	}
        if(batt_temp > 480) {
                if(batt_voltage_now >= 4410) {
                    pr_err("%s : high temp :batt_voltage_now >= 4410:disable chargring\n", __func__, batt_voltage_now);
                    vote(bq->chgctrl_votable, PD2SW_HITEMP_OCCURE_VOTER, true, 1);
                 }
                if(batt_voltage_now <= 4300) {
                    pr_err("%s : high temp :batt_voltage_now <= 4300:enable chargring\n", __func__, batt_voltage_now);
                    vote(bq->chgctrl_votable, PD2SW_HITEMP_OCCURE_VOTER, false, 0);
                }
        }
#endif
	bq2589x_dump_regs(bq);
	bq2589x_reset_watchdog_timer(bq);
	bq->rsoc = bq2589x_read_batt_rsoc(bq);
	bq->vbus_volt = bq2589x_adc_read_vbus_volt(bq);
	bq->vbat_volt = bq2589x_adc_read_battery_volt(bq);
	bq->chg_current = bq2589x_adc_read_charge_current(bq);
	rawfcc = bq2589x_get_charge_current(bq);
	rawfv = bq2589x_get_chargevoltage(bq);
	rawicl = bq2589x_get_input_current_limit(bq);

	if(!bq->bms_psy)
		bq->bms_psy = power_supply_get_by_name("bms");
	if(bq->bms_psy){
		ret = power_supply_get_property(bq->bms_psy, POWER_SUPPLY_PROP_TEMP, &propval);
		if (ret < 0) {
			pr_err("%s : get battery temp fail\n", __func__);
		}
		batt_temp = propval.intval;
		pr_err("%s : get battery temp :%d\n", __func__, batt_temp);
	}
	if(batt_temp < 0){
		bq2589x_update_bits(bq, BQ2589X_REG_06, BQ2589X_VRECHG_MASK, BQ2589X_VRECHG_200MV << BQ2589X_VRECHG_SHIFT);
	}else{
		bq2589x_update_bits(bq, BQ2589X_REG_06, BQ2589X_VRECHG_MASK, BQ2589X_VRECHG_100MV << BQ2589X_VRECHG_SHIFT);
	}

	ret = bq2589x_read_byte(bq, &status, BQ2589X_REG_13);
	if (ret == 0 && (status & BQ2589X_VDPM_STAT_MASK))
		bq_dbg(PR_OEM, "VINDPM occurred\n");
	if (ret == 0 && (status & BQ2589X_IDPM_STAT_MASK))
		bq_dbg(PR_OEM, "IINDPM occurred\n");
		
	if (bq->vbus_type == BQ2589X_VBUS_USB_DCP && bq->vbus_volt > pe.high_volt_level &&
	    bq->rsoc > 95 && !pe.tune_down_volt) {
		pe.tune_down_volt = true;
		pe.tune_up_volt = false;
		pe.target_volt = pe.low_volt_level;
		pe.tune_done = false;
		pe.tune_count = 0;
		pe.tune_fail = false;
		schedule_delayed_work(&bq->pe_volt_tune_work, 0);
	}
	switch(bq->vbus_type)
	{
		case BQ2589X_VBUS_MAXC:
			bq2589x_enable_ico(bq, false);
			if (rawicl != 2000 && rawicl != MAIN_ICL_MIN) {
				rerun_election(bq->usb_icl_votable);
			}
		case BQ2589X_VBUS_USB_DCP:
			if (rawicl != 2000 && rawicl != MAIN_ICL_MIN) {
				rerun_election(bq->usb_icl_votable);
			}
		case BQ2589X_VBUS_USB_SDP:
		case BQ2589X_VBUS_USB_CDP:
		case BQ2589X_VBUS_NONSTAND:
		case BQ2589X_VBUS_UNKNOWN:
			if (rawfcc > get_effective_result_locked(bq->fcc_votable))
				rerun_election(bq->fcc_votable);
			if (rawfv > get_effective_result_locked(bq->fv_votable))
				rerun_election(bq->fv_votable);
			break;
	}

	schedule_delayed_work(&bq->monitor_work, 5 * HZ);
}

static void bq2589x_start_charging_workfunc(struct work_struct *work)
{
	struct bq2589x *bq = container_of(work, struct bq2589x, start_charging_work);
	int last_status = 0;
	int status = 0;
	bool stop = false;
	int times = 1;

	if (!bq->bms_psy)
		bq->bms_psy = power_supply_get_by_name("bms");
	if (bq->bms_psy){
		stop = true;
		stop_fg_monitor_work(bq->bms_psy);
	}

	if (!bq->batt_psy)
		bq->batt_psy = power_supply_get_by_name("battery");
	if (bq->batt_psy)
		power_supply_changed(bq->batt_psy);

	while (bq->bms_psy && bq->batt_psy && times <= 50) {
		status = bq2589x_get_charging_status(bq);
		pr_info("times: %d, status: %d", times, status);
		if (status != last_status) {
			last_status = status;
			power_supply_changed(bq->batt_psy);
		}
		if (status == POWER_SUPPLY_STATUS_CHARGING) {
			pr_info("power_supply_changed: bms_psy");
			power_supply_changed(bq->bms_psy);
			break;
		}
		times++;
		msleep(200);
	}
	if (stop)
		start_fg_monitor_work(bq->bms_psy);
}

static int bq2589x_set_charger_type(struct bq2589x *bq, enum power_supply_type chg_type)
{
	int ret = 0;
	union power_supply_propval propval;
	
	if (bq->wall_psy == NULL) {
		bq->wall_psy = power_supply_get_by_name("bbc");
		if (bq->wall_psy == NULL) {
			pr_err("%s : fail to get psy bbc\n", __func__);
			return -ENODEV;
		}
	}

	if(bq->usb_psy == NULL) {
		bq->usb_psy = power_supply_get_by_name("usb");
		if (bq->usb_psy == NULL) {
			pr_err("%s : fail to get psy usb\n", __func__);
			return -ENODEV;
		}
	}

	if (chg_type != POWER_SUPPLY_TYPE_UNKNOWN)
		propval.intval = true;
	else
		propval.intval = false;

	ret = power_supply_set_property(bq->wall_psy, POWER_SUPPLY_PROP_ONLINE,
					&propval);
	if (ret < 0)
		pr_err("inform power supply chg_online failed:%d\n", ret);

	if (chg_type != POWER_SUPPLY_TYPE_UNKNOWN)
		propval.intval = true;
	else
		propval.intval = false;

	if (bq->pd_active && (propval.intval == false))
		;//fix CtoC disconnection
	else
		ret = power_supply_set_property(bq->usb_psy, POWER_SUPPLY_PROP_ONLINE,
						&propval);
	if (ret < 0)
		pr_err("inform power supply usb_online failed:%d\n", ret);

	propval.intval = chg_type;

	ret = power_supply_set_property(bq->wall_psy,
					POWER_SUPPLY_PROP_CHARGE_TYPE,
					&propval);
	if (ret < 0)
		pr_err("inform power supply charge type failed:%d\n", ret);

	propval.intval = chg_type;
	ret = power_supply_set_property(bq->usb_psy, POWER_SUPPLY_PROP_REAL_TYPE,
					&propval);
	if (ret < 0)
		pr_err("%s : set prop REAL_TYPE fail ret:%d\n", __func__, ret);

	power_supply_changed(bq->usb_psy);
	return ret;
}

static enum power_supply_type bq2589x_get_charger_type(struct bq2589x *bq)
{	
	enum power_supply_type chg_type = POWER_SUPPLY_TYPE_UNKNOWN;

	switch(bq->vbus_type)
	{
		case BQ2589X_VBUS_NONE:
			bq_dbg(PR_OEM, "charger_type: NONE\n");
			chg_type = POWER_SUPPLY_TYPE_UNKNOWN;	
			break;
		case BQ2589X_VBUS_MAXC:
			bq_dbg(PR_OEM, "charger_type: HVDCP/Maxcharge\n");
			chg_type = POWER_SUPPLY_TYPE_USB_HVDCP;

			if (bq->part_no == SC89890H) {
//modify by HTH-223146 at 2022/06/23 begin
				bq2589x_set_input_current_limit(bq, 2000);
//modify by HTH-223146 at 2022/06/23 end
				bq2589x_write_byte(bq, BQ2589X_REG_01, 0xC9);
			}
//modify by HTH-209427/HTH-209841/HTH-234945/HTH-234948 at 2022/06/08 begin
			if (bq->cfg.use_absolute_vindpm) {
				bq2589x_adjust_absolute_vindpm(bq);
			}
//modify by HTH-209427/HTH-209841/HTH-234945/HTH-234948 at 2022/06/08 end
			break;
		case BQ2589X_VBUS_USB_DCP:
			bq_dbg(PR_OEM, "charger_type: DCP\n");
			chg_type = POWER_SUPPLY_TYPE_USB_DCP;
//modify by HTH-223146 at 2022/06/23 begin
			if (bq->part_no == SC89890H) {
				bq2589x_set_input_current_limit(bq, 2000);
			}
//modify by HTH-223146 at 2022/06/23 begin
			break;
		case BQ2589X_VBUS_USB_CDP:
			bq_dbg(PR_OEM, "charger_type: CDP\n");
			chg_type = POWER_SUPPLY_TYPE_USB_CDP;
			break;
		case BQ2589X_VBUS_USB_SDP:
			bq_dbg(PR_OEM, "charger_type: SDP\n");
			chg_type = POWER_SUPPLY_TYPE_USB;
			break;
		case BQ2589X_VBUS_NONSTAND:
			bq_dbg(PR_OEM, "charger_type: FLOAT\n");
		case BQ2589X_VBUS_UNKNOWN:
			bq_dbg(PR_OEM, "charger_type: UNKNOWN\n");
			chg_type = POWER_SUPPLY_TYPE_USB_FLOAT;
			break;
		case BQ2589X_VBUS_OTG:
			bq_dbg(PR_OEM, "charger_type: OTG\n");
			chg_type = POWER_SUPPLY_TYPE_UNKNOWN;
			break;
		default:
			bq_dbg(PR_OEM, "charger_type: Other vbus_type is %d\n", bq->vbus_type);
			chg_type = POWER_SUPPLY_TYPE_USB_FLOAT;
			break;
	}
	
	return chg_type;
}

static void bq2589x_charger_irq_workfunc(struct work_struct *work)
{
	struct bq2589x *bq = container_of(work, struct bq2589x, irq_work);
	u8 status = 0;
	u8 fault = 0;
	u8 vbus_status = 0;
	u8 charge_status = 0;
	int ret;
	//static int count =0;
	enum power_supply_type chg_type = POWER_SUPPLY_TYPE_UNKNOWN;
	//int prev_chg_type;
	//count++;
	//bq_dbg(PR_INTERRUPT, "start count:%d\n", count);

	//pr_err("wsy irq_works bq2589x_usb_switch gpio_value=%d\n", gpio_get_value(bq->usb_switch1));

	/* Read STATUS and FAULT registers */
	ret = bq2589x_read_byte(bq, &status, BQ2589X_REG_0B);
	if (ret)
		return;
	//bq2589x_dump_regs(bq);
	//prev_chg_type = bq->vbus_type;
	bq->vbus_type = (status & BQ2589X_VBUS_STAT_MASK) >> BQ2589X_VBUS_STAT_SHIFT;
	chg_type = bq2589x_get_charger_type(bq);
	if(bq->vbus_type == BQ2589X_VBUS_USB_CDP || bq->vbus_type == BQ2589X_VBUS_USB_SDP){
		bq2589x_usb_switch(bq, false);
	}

	if ((bq->vbus_type == BQ2589X_VBUS_NONSTAND || bq->vbus_type == BQ2589X_VBUS_UNKNOWN)) {
//		bq2589x_usb_switch(bq, true);
//		bq2589x_force_dpdm_done(bq);
	//	bq2589x_adc_start(bq, false);
	//	return;
	}

	bq2589x_set_charger_type(bq, chg_type);
/*
	if (prev_chg_type == bq->vbus_type) {
		pr_err("%s : prev_chg_type == new_chg_type\n", __func__);
		return;
	}
*/
	ret = bq2589x_read_byte(bq, &fault, BQ2589X_REG_0C);
	if (ret)
		return;

	if (!bq->batt_psy)
		bq->batt_psy = power_supply_get_by_name("battery");

//modify by HTH-234945/HTH-234948 at 2022/06/08 begin
	if(bq->part_no == SC89890H){
		ret = bq2589x_read_byte(bq, &vbus_status, BQ2589X_REG_11);
		if (ret)
			return;

		if (!(vbus_status & BQ2589X_VBUS_GD_MASK ) && (bq->status & BQ2589X_STATUS_PLUGIN)) {
			bq2589x_usb_switch(bq, false);
			bq2589x_adc_stop(bq);
			bq->status &= ~BQ2589X_STATUS_PLUGIN;
			schedule_work(&bq->adapter_out_work);
			pr_err("adapter removed\n");
			schedule_delayed_work(&bq->charger_work, 0);
		} else if (bq->vbus_type != BQ2589X_VBUS_NONE && (bq->vbus_type != BQ2589X_VBUS_OTG) && !(bq->status & BQ2589X_STATUS_PLUGIN)) {
			bq->status |= BQ2589X_STATUS_PLUGIN;
			bq2589x_update_bits(bq, BQ2589X_REG_00, BQ2589X_ENILIM_MASK,
				BQ2589X_ENILIM_DISABLE << BQ2589X_ENILIM_SHIFT);
			schedule_delayed_work(&bq->usb_changed_work, 0);
			schedule_work(&bq->adapter_in_work);
			pr_err("adapter plugged in\n");
			schedule_delayed_work(&bq->charger_work, 100);
			schedule_work(&bq->start_charging_work);
		}
	}else{
		if (((bq->vbus_type == BQ2589X_VBUS_NONE) || (bq->vbus_type == BQ2589X_VBUS_OTG)) && (bq->status & BQ2589X_STATUS_PLUGIN)) {
			bq2589x_usb_switch(bq, false);
			bq2589x_adc_stop(bq);
			bq->status &= ~BQ2589X_STATUS_PLUGIN;
			schedule_work(&bq->adapter_out_work);
			pr_err("adapter removed\n");
			schedule_delayed_work(&bq->charger_work, 0);
		} else if (bq->vbus_type != BQ2589X_VBUS_NONE && (bq->vbus_type != BQ2589X_VBUS_OTG) && !(bq->status & BQ2589X_STATUS_PLUGIN)) {
			bq->status |= BQ2589X_STATUS_PLUGIN;
			bq2589x_update_bits(bq, BQ2589X_REG_00, BQ2589X_ENILIM_MASK,
				BQ2589X_ENILIM_DISABLE << BQ2589X_ENILIM_SHIFT);
			schedule_delayed_work(&bq->usb_changed_work, 0);
			schedule_work(&bq->adapter_in_work);
			pr_err("adapter plugged in\n");
			schedule_delayed_work(&bq->charger_work, 100);
			schedule_work(&bq->start_charging_work);
		}
	}
//modify by HTH-234945/HTH-234948 at 2022/06/08 end

	if ((status & BQ2589X_PG_STAT_MASK) && !(bq->status & BQ2589X_STATUS_PG))
		bq->status |= BQ2589X_STATUS_PG;
	else if (!(status & BQ2589X_PG_STAT_MASK) && (bq->status & BQ2589X_STATUS_PG))
		bq->status &= ~BQ2589X_STATUS_PG;

	if (fault && !(bq->status & BQ2589X_STATUS_FAULT))
		bq->status |= BQ2589X_STATUS_FAULT;
	else if (!fault && (bq->status & BQ2589X_STATUS_FAULT))
		bq->status &= ~BQ2589X_STATUS_FAULT;

	charge_status = (status & BQ2589X_CHRG_STAT_MASK) >> BQ2589X_CHRG_STAT_SHIFT;
	if (charge_status == BQ2589X_CHRG_STAT_IDLE){
		bq_dbg(PR_OEM, "not charging\n");
	} else if (charge_status == BQ2589X_CHRG_STAT_PRECHG){
		bq_dbg(PR_OEM, "precharging\n");
	} else if (charge_status == BQ2589X_CHRG_STAT_FASTCHG){
		bq_dbg(PR_OEM, "fast charging\n");
	} else if (charge_status == BQ2589X_CHRG_STAT_CHGDONE){
		bq_dbg(PR_OEM, "charge done!\n");
	}

	if(fault & 0x40) {
		bq2589x_usb_switch(g_bq, true);
		bq2589x_set_otg(bq, false);
		bq2589x_enable_charger(bq);
	}

	if(fault & 0x80){
		bq2589x_reset_chip(bq);
		msleep(5);
		bq2589x_init_device(bq);
	}
}

static irqreturn_t bq2589x_charger_interrupt(int irq, void *data)
{
	struct bq2589x *bq = data;
	//static int irq_count = 0;

	//irq_count++;
	//bq_dbg(PR_OEM, "irq_count:%d\n",irq_count);
	schedule_work(&bq->irq_work);
	return IRQ_HANDLED;
}

#if defined(CONFIG_TCPC_RT1711H)
static void set_pd_active(struct bq2589x *bq, int pd_active)
{
	int rc = 0;
	union power_supply_propval val = {0,};

	if (!bq->usb_psy)
		bq->usb_psy = power_supply_get_by_name("usb");

	if (bq->usb_psy) {
		if (pd_active)
			bq2589x_set_charger_type(bq, POWER_SUPPLY_TYPE_USB_PD);
		else
			bq2589x_set_charger_type(bq, POWER_SUPPLY_TYPE_UNKNOWN);
		bq->pd_active = pd_active;
		val.intval = pd_active;
		rc = power_supply_set_property(bq->usb_psy,
			POWER_SUPPLY_PROP_PD_ACTIVE, &val);
		bq2589x_set_fast_charge_mode(bq, pd_active);
		if (rc < 0)
			bq_dbg(PR_OEM, "Couldn't read USB Present status, rc=%d\n", rc);
	}
}

static int get_source_mode(struct tcp_notify *noti)
{
	if (noti->typec_state.new_state == TYPEC_ATTACHED_CUSTOM_SRC || noti->typec_state.new_state == TYPEC_ATTACHED_NORP_SRC)
		return POWER_SUPPLY_TYPEC_SOURCE_DEFAULT;

	switch (noti->typec_state.rp_level) {
	case TYPEC_CC_VOLT_SNK_1_5:
		return POWER_SUPPLY_TYPEC_SOURCE_MEDIUM;
	case TYPEC_CC_VOLT_SNK_3_0:
		return POWER_SUPPLY_TYPEC_SOURCE_HIGH;
	case TYPEC_CC_VOLT_SNK_DFT:
		return POWER_SUPPLY_TYPEC_SOURCE_DEFAULT;
	default:
		break;
	}

	return POWER_SUPPLY_TYPEC_NONE;
}

static int bq2589x_set_cc_orientation(struct bq2589x *bq, int cc_orientation)
{
	int ret = 0;
	union power_supply_propval propval;

	if(bq->usb_psy == NULL) {
		bq->usb_psy = power_supply_get_by_name("usb");
		if (bq->usb_psy == NULL) {
			pr_err("%s : fail to get psy usb\n", __func__);
			return -ENODEV;
		}
	}

	propval.intval = cc_orientation;

	ret = power_supply_set_property(bq->usb_psy,
					POWER_SUPPLY_PROP_TYPEC_CC_ORIENTATION,
					&propval);
	if (ret < 0)
		pr_err("%s : set prop CC_ORIENTATION fail ret:%d\n", __func__, ret);

	return ret;	
}

static int bq2589x_set_typec_mode(struct bq2589x *bq, enum power_supply_typec_mode typec_mode)
{	
	int ret = 0;
	union power_supply_propval propval;

	if(bq->usb_psy == NULL) {
		bq->usb_psy = power_supply_get_by_name("usb");
		if (bq->usb_psy == NULL) {
			pr_err("%s : fail to get psy usb\n", __func__);
			return -ENODEV;
		}
	}

	propval.intval = typec_mode;

	ret = power_supply_set_property(bq->usb_psy,
					POWER_SUPPLY_PROP_TYPEC_MODE,
					&propval);
	if (ret < 0)
		pr_err("%s : set prop TYPEC_MODE fail ret:%d\n", __func__, ret);

	return ret;
}

static int pd_tcp_notifier_call(struct notifier_block *pnb,
				unsigned long event, void *data)
{
	struct tcp_notify *noti = data;
	struct bq2589x *bq = container_of(pnb, struct bq2589x, pd_nb);
	enum power_supply_typec_mode typec_mode = POWER_SUPPLY_TYPEC_NONE;
	int cc_orientation = 0;
	//bq_dbg(PR_OEM, "event:%d\n",event);
	switch (event) {
	case TCP_NOTIFY_SINK_VBUS:
		pr_err("%s : TCP_NOTIFY_SINK_VBUS\n", __func__);
		break;
	case TCP_NOTIFY_PD_STATE:
		bq_dbg(PR_OEM, "noti->pd_state connected:%d\n", noti->pd_state.connected);
		switch (noti->pd_state.connected){
		case  PD_CONNECT_NONE:
			bq_dbg(PR_OEM, "disconnected\n");
			break;
		case PD_CONNECT_HARD_RESET:
			break;
		case PD_CONNECT_PE_READY_SNK:
			bq_dbg(PR_OEM, "PD2.0 connect\n");
			set_pd_active(bq, 1);
			break;
		case PD_CONNECT_PE_READY_SNK_PD30:
			bq_dbg(PR_OEM, "PD3.0 connect\n");
			set_pd_active(bq, 1);
			break;
		case PD_CONNECT_PE_READY_SNK_APDO:
			bq_dbg(PR_OEM, "PPS connect\n");
			//get_apdo_regain = 1;
			set_pd_active(bq, 2);
			//schedule_delayed_work(&bq->period_work, msecs_to_jiffies(5000));
			break;
		}
		break;
	case TCP_NOTIFY_TYPEC_STATE:
		if (noti->typec_state.old_state == TYPEC_UNATTACHED &&
		    (noti->typec_state.new_state == TYPEC_ATTACHED_SNK ||
		    noti->typec_state.new_state == TYPEC_ATTACHED_CUSTOM_SRC ||
		    noti->typec_state.new_state == TYPEC_ATTACHED_NORP_SRC)) {
			bq_dbg(PR_OEM,"USB Plug in, pol = %d, state = %d\n",
					noti->typec_state.polarity, noti->typec_state.new_state);
			pm_stay_awake(bq->dev);
			bq2589x_init_device(bq);
			bq2589x_usb_switch(bq, true);
			bq2589x_force_dpdm_done(bq);
			typec_mode = get_source_mode(noti);
			cc_orientation = noti->typec_state.polarity;
			bq2589x_set_cc_orientation(bq, cc_orientation);
		} else if (noti->typec_state.old_state == TYPEC_UNATTACHED &&
			noti->typec_state.new_state == TYPEC_ATTACHED_SRC) {
			typec_mode = POWER_SUPPLY_TYPEC_SINK;
		} else if ((noti->typec_state.old_state == TYPEC_ATTACHED_SNK ||
		    noti->typec_state.old_state == TYPEC_ATTACHED_CUSTOM_SRC ||
			noti->typec_state.old_state == TYPEC_ATTACHED_NORP_SRC ||
			noti->typec_state.old_state == TYPEC_ATTACHED_SRC)
			&& noti->typec_state.new_state == TYPEC_UNATTACHED) {
			set_pd_active(bq, 0);
			typec_mode = POWER_SUPPLY_TYPEC_NONE;
			bq_dbg(PR_OEM," USB Plug out\n");
			bq2589x_usb_switch(bq, false);
			pm_relax(bq->dev);
		} else if (noti->typec_state.old_state == TYPEC_ATTACHED_SRC &&
			noti->typec_state.new_state == TYPEC_ATTACHED_SNK) {
			bq_dbg(PR_OEM," Source_to_Sink\n");
			typec_mode = POWER_SUPPLY_TYPEC_SINK;
		}  else if (noti->typec_state.old_state == TYPEC_ATTACHED_SNK &&
			noti->typec_state.new_state == TYPEC_ATTACHED_SRC) {
			typec_mode = get_source_mode(noti);
			bq_dbg(PR_OEM," Sink_to_Source\n");
		}
		if(typec_mode >= POWER_SUPPLY_TYPEC_NONE && typec_mode <= POWER_SUPPLY_TYPEC_NON_COMPLIANT)
			bq2589x_set_typec_mode(bq, typec_mode);
		break;
	case TCP_NOTIFY_EXT_DISCHARGE:
		pr_err("%s: usb plug out", __func__);
		break;
	case TCP_NOTIFY_SOURCE_VBUS:
		pr_err("%s: TCP_NOTIFY_SOURCE_VBUS", __func__);
		//2021.09.11 wsy crash this case
		if ((noti->vbus_state.mv == TCPC_VBUS_SOURCE_0V) && (vbus_on)) {
			/* disable OTG power output */
			pr_err("%s : otg plug out\n", __func__);
			vbus_on = false;
			bq2589x_set_otg(bq, false);
			bq2589x_usb_switch(bq, true);
		} else if ((noti->vbus_state.mv == TCPC_VBUS_SOURCE_5V) && (!vbus_on)) {
			/* enable OTG power output */
			pr_err("%s : otg plug in\n", __func__);
			vbus_on = true;
			bq2589x_usb_switch(bq, false);
			bq2589x_set_otg(bq, true);
			bq2589x_set_otg_current(bq, bq->cfg.charge_current_1500);
		}
		break;
	}

	return NOTIFY_OK;
}
#endif

static int fcc_vote_callback(struct votable *votable, void *data,
			int fcc_ua, const char *client)
{
	struct bq2589x *bq = data;
	int rc;

	if (fcc_ua < 0)
		return 0;
	if (fcc_ua > BQ2589X_MAX_FCC)
		fcc_ua = BQ2589X_MAX_FCC;
	rc = bq2589x_set_charge_current(bq, fcc_ua);
	if (rc < 0) {
		bq_dbg(PR_OEM, "failed to set charge current\n");
		return rc;
	}

	return 0;
}
static int chg_dis_vote_callback(struct votable *votable, void *data,
			int disable, const char *client)
{
	struct bq2589x *bq = data;
	int rc;

	if (disable) {
		rc = bq2589x_disable_charger(bq);
	} else {
		rc = bq2589x_enable_charger(bq);
	}
	if (rc < 0) {
		bq_dbg(PR_OEM, "failed to disabl:e%d\n",disable);
		return rc;
	}
	bq_dbg(PR_OEM,"disable:%d\n", disable);
	return 0;
}

static int fv_vote_callback(struct votable *votable, void *data,
			int fv_mv, const char *client)
{
	struct bq2589x *bq = data;
	int rc;
	if (fv_mv < 0)
		return 0;
	rc = bq2589x_set_chargevoltage(bq, fv_mv);
	if (rc < 0) {
		bq_dbg(PR_OEM, "failed to set chargevoltage\n");
		return rc;
	}
	bq_dbg(PR_OEM," fv:%d\n", fv_mv);
	return 0;
}

static int usb_icl_vote_callback(struct votable *votable, void *data,
			int icl_ma, const char *client)
{
	int rc;
	bq_dbg(PR_OEM," icl:%d\n", icl_ma);
	if (icl_ma < 0)
		return 0;
	if (icl_ma > BQ2589X_MAX_ICL)
		icl_ma = BQ2589X_MAX_ICL;
	rc = main_set_input_current_limit(icl_ma);
	if (rc < 0) {
		bq_dbg(PR_OEM, "failed to set input current limit\n");
		return rc;
	}
	return 0;
}
/*
static int chgctrl_vote_callback(struct votable *votable, void *data,
			int disable, const char *client)
{
        struct bq2589x *bq = data;
	int rc;
        bq_dbg(PR_OEM," chgctrl_vote_callback chgctrl disable:%d\n", disable);
        
        if(disable)
                rc = bq2589x_disable_charger(bq);
	else
		rc = bq2589x_enable_charger(bq);
	return 0;
}*/  //for ovp lead reboot


static int bq2589x_charger_probe(struct i2c_client *client,
			   const struct i2c_device_id *id)
{
	struct bq2589x *bq;
	int irqn;

	int ret;

	bq = devm_kzalloc(&client->dev, sizeof(struct bq2589x), GFP_KERNEL);
	if (!bq) {
		bq_dbg(PR_OEM, "%s: out of memory\n", __func__);
		return -ENOMEM;
	}

	bq->dev = &client->dev;
	bq->client = client;
	i2c_set_clientdata(client, bq);

	mutex_init(&bq->i2c_rw_lock);
	mutex_init(&bq->usb_switch_lock);

	ret = bq2589x_detect_device(bq);
	if (!ret && bq->part_no == BQ25890) {
		bq->status |= BQ2589X_STATUS_EXIST;
		//bq->is_bq25890h = true;
		bq_dbg(PR_OEM, "charger device bq25890 detected, revision:%d\n", bq->revision);
	} else if (!ret && bq->part_no == SYV690) {
		bq->status |= BQ2589X_STATUS_EXIST;
		//bq->is_bq25890h = false;
		bq_dbg(PR_OEM, "charger device SYV690 detected, revision:%d\n", bq->revision);
	} else if (!ret && bq->part_no == SC89890H) {
              bq->status |= BQ2589X_STATUS_EXIST;
               //bq->is_bq25890h = false;
               bq_dbg(PR_OEM, "charger device SC89890H detected, revision:%d\n", bq->revision);
       } else {
			bq_dbg(PR_OEM, "no bq25890 charger device found:%d\n", ret);
			ret = -ENODEV;
			goto err_free;
	}

	nopmi_set_charger_ic_type(NOPMI_CHARGER_IC_SYV);
	
	bq->batt_psy = power_supply_get_by_name("battery");
	bq->bms_psy = power_supply_get_by_name("bms");

	g_bq = bq;

	if (client->dev.of_node)
		bq2589x_parse_dt(&client->dev, bq);

	ret = bq2589x_init_device(bq);
	if (ret) {
		bq_dbg(PR_OEM, "device init failure: %d\n", ret);
		goto err_free;
	}
	ret = gpio_request(bq->irq_gpio, "bq2589x irq pin");
	if (ret) {
		bq_dbg(PR_OEM, "%s: %d gpio request failed\n", __func__, bq->irq_gpio);
		goto err_free;
	}

	irqn = gpio_to_irq(bq->irq_gpio);
	if (irqn < 0) {
		bq_dbg(PR_OEM, "%s:%d gpio_to_irq failed\n", __func__, irqn);
		ret = irqn;
		goto err_free;
	}
	client->irq = irqn;

	ret = bq2589x_psy_register(bq);
	if (ret)
		goto err_free;

	INIT_WORK(&bq->irq_work, bq2589x_charger_irq_workfunc);
	INIT_WORK(&bq->adapter_in_work, bq2589x_adapter_in_workfunc);
	INIT_WORK(&bq->adapter_out_work, bq2589x_adapter_out_workfunc);
	INIT_WORK(&bq->start_charging_work, bq2589x_start_charging_workfunc);
	INIT_DELAYED_WORK(&bq->monitor_work, bq2589x_monitor_workfunc);
	INIT_DELAYED_WORK(&bq->ico_work, bq2589x_ico_workfunc);
	INIT_DELAYED_WORK(&bq->charger_work, bq2589x_charger_workfunc);
	INIT_DELAYED_WORK(&bq->pe_volt_tune_work, bq2589x_tune_volt_workfunc);
	INIT_DELAYED_WORK(&bq->usb_changed_work, bq2589x_usb_changed_workfunc);
	INIT_DELAYED_WORK(&bq->check_pe_tuneup_work, bq2589x_check_pe_tuneup_workfunc);
	INIT_DELAYED_WORK(&bq->time_delay_work, time_delay_work);
	//INIT_DELAYED_WORK(&bq->period_work, bq2589x_period_workfunc);

	bq->fcc_votable = create_votable("FCC", VOTE_MIN,
					fcc_vote_callback,
					bq);
	if (IS_ERR(bq->fcc_votable)) {
		ret = PTR_ERR(bq->fcc_votable);
		bq->fcc_votable = NULL;
		goto destroy_votable;
	}
	bq->chg_dis_votable = create_votable("CHG_DISABLE", VOTE_SET_ANY,
					chg_dis_vote_callback,
					bq);
	if (IS_ERR(bq->chg_dis_votable)) {
		ret = PTR_ERR(bq->chg_dis_votable);
		bq->chg_dis_votable = NULL;
		goto destroy_votable;
	}

	bq->fv_votable = create_votable("FV", VOTE_MIN,
					fv_vote_callback,
					bq);
	if (IS_ERR(bq->fv_votable)) {
		ret = PTR_ERR(bq->fv_votable);
		bq->fv_votable = NULL;
		goto destroy_votable;
	}

	bq->usb_icl_votable = create_votable("USB_ICL", VOTE_MIN,
					usb_icl_vote_callback,
					bq);
	if (IS_ERR(bq->usb_icl_votable)) {
		ret = PTR_ERR(bq->usb_icl_votable);
		bq->usb_icl_votable = NULL;
		goto destroy_votable;
	}
#if 0
	bq->chgctrl_votable = create_votable("CHG_CTRL", VOTE_MIN,
					chgctrl_vote_callback,
					bq);
	if (IS_ERR(bq->chgctrl_votable)) {
		ret = PTR_ERR(bq->chgctrl_votable);
		bq->chgctrl_votable = NULL;
		goto destroy_votable;
	}
#endif //for ovp lead reboot
	vote(bq->fcc_votable, PROFILE_CHG_VOTER, true, CHG_FCC_CURR_MAX);
	vote(bq->usb_icl_votable, PROFILE_CHG_VOTER, true, CHG_ICL_CURR_MAX);
	vote(bq->chg_dis_votable, "BMS_FC_VOTER", false, 0);

	ret = sysfs_create_group(&bq->dev->kobj, &bq2589x_attr_group);
	if (ret) {
		bq_dbg(PR_OEM, "failed to register sysfs. err: %d\n", ret);
		goto err_irq;
	}
//modify by HTH-209427/HTH-209841 at 2022/05/12 begin
	pe.enable = false;//PE adjuested to the front of the interrupt
//modify by HTH-209427/HTH-209841 at 2022/05/12 end
	ret = request_irq(client->irq, bq2589x_charger_interrupt, IRQF_TRIGGER_FALLING | IRQF_ONESHOT, "bq2589x_charger1_irq", bq);
	if (ret) {
		bq_dbg(PR_OEM, "%s:Request IRQ %d failed: %d\n", __func__, client->irq, ret);
		goto err_irq;
	} else {
		bq_dbg(PR_OEM, "%s:irq = %d\n", __func__, client->irq);
	}
	
	//schedule_work(&bq->irq_work); // 2020.09.15 change for zsa/*in case of adapter has been in when power off*/

#if defined(CONFIG_TCPC_RT1711H)
	bq->tcpc_dev = tcpc_dev_get_by_name("type_c_port0");
	if (bq->tcpc_dev == NULL) {
		bq_dbg(PR_OEM, " tcpc device not ready, defer\n");
		ret = -EPROBE_DEFER;
		goto err_get_tcpc_dev;
	}
	bq->pd_nb.notifier_call = pd_tcp_notifier_call;
	ret = register_tcp_dev_notifier(bq->tcpc_dev,
		&bq->pd_nb, TCP_NOTIFY_TYPE_ALL);
	if (ret < 0) {
		bq_dbg(PR_OEM, " register tcpc notifer fail\n");
		ret = -EINVAL;
		goto err_get_tcpc_dev;
	}
#endif
	enable_irq_wake(irqn);

	schedule_delayed_work(&bq->time_delay_work, msecs_to_jiffies(4000));

	return 0;
#if defined(CONFIG_TCPC_RT1711H)
 err_get_tcpc_dev:
#endif
 err_irq:
	cancel_work_sync(&bq->irq_work);
	cancel_work_sync(&bq->adapter_in_work);
	cancel_work_sync(&bq->adapter_out_work);
	cancel_work_sync(&bq->start_charging_work);
	cancel_delayed_work_sync(&bq->monitor_work);
	cancel_delayed_work_sync(&bq->ico_work);
	cancel_delayed_work_sync(&bq->charger_work);
	cancel_delayed_work_sync(&bq->check_pe_tuneup_work);
	cancel_delayed_work_sync(&bq->usb_changed_work);
	cancel_delayed_work_sync(&bq->pe_volt_tune_work);
	cancel_delayed_work_sync(&bq->time_delay_work);
	//cancel_delayed_work_sync(&bq->period_work);
destroy_votable:
	destroy_votable(bq->fcc_votable);
	destroy_votable(bq->chg_dis_votable);
	destroy_votable(bq->fv_votable);
	destroy_votable(bq->usb_icl_votable);
        //destroy_votable(bq->chgctrl_votable);
err_free:
	mutex_destroy(&bq->i2c_rw_lock);
	mutex_destroy(&bq->usb_switch_lock);
	devm_kfree(&client->dev,bq);
	g_bq = NULL;
	return ret;
}

static void bq2589x_charger_shutdown(struct i2c_client *client)
{
	struct bq2589x *bq = i2c_get_clientdata(client);

	bq2589x_disable_otg(bq);
	
	bq2589x_exit_hiz_mode(bq);
	bq2589x_adc_stop(bq);
	bq2589x_psy_unregister(bq);
	msleep(2);

	sysfs_remove_group(&bq->dev->kobj, &bq2589x_attr_group);
	cancel_work_sync(&bq->irq_work);
	cancel_work_sync(&bq->adapter_in_work);
	cancel_work_sync(&bq->adapter_out_work);
	cancel_work_sync(&bq->start_charging_work);
	cancel_delayed_work_sync(&bq->monitor_work);
	cancel_delayed_work_sync(&bq->ico_work);
	cancel_delayed_work_sync(&bq->charger_work);
	cancel_delayed_work_sync(&bq->check_pe_tuneup_work);
	cancel_delayed_work_sync(&bq->usb_changed_work);
	cancel_delayed_work_sync(&bq->pe_volt_tune_work);
	cancel_delayed_work_sync(&bq->time_delay_work);
	//cancel_delayed_work_sync(&bq->period_work);
	if (bq->client->irq) {
		disable_irq(bq->client->irq);
		free_irq(bq->client->irq, bq);
		gpio_free(bq->irq_gpio);
	}
	g_bq = NULL;
}

static struct of_device_id bq2589x_charger_match_table[] = {
	{.compatible = "ti,bq2589x-1",},
	{},
};


static const struct i2c_device_id bq2589x_charger_id[] = {
	{ "bq2589x-1", BQ25890 },
	{},
};

MODULE_DEVICE_TABLE(i2c, bq2589x_charger_id);

static struct i2c_driver bq2589x_charger_driver = {
	.driver		= {
		.name	= "bq2589x-1",
		.of_match_table = bq2589x_charger_match_table,
	},
	.id_table	= bq2589x_charger_id,

	.probe		= bq2589x_charger_probe,
	.shutdown   = bq2589x_charger_shutdown,
};

module_i2c_driver(bq2589x_charger_driver);

MODULE_DESCRIPTION("TI BQ2589x Charger Driver");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Texas Instruments");
