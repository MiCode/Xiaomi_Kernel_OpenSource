/*
 * BQ2589x battery charging driver
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

#define pr_fmt(fmt)	"[bq2589x]:%s: " fmt, __func__

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
#include <linux/bitops.h>
#include <linux/math64.h>
#include <mt-plat/charger_type.h>
#include <linux/of.h>
#include <linux/of_irq.h>

#include "mtk_charger_intf.h"

#define __BQ25890H__	1

#include "bq2589x_reg.h"

enum {
	PN_BQ25890,
	PN_BQ25892,
	PN_BQ25895,
//	PN_SYV690,
};

static int pn_data[] = {
	[PN_BQ25890] = 0x03,
	[PN_BQ25892] = 0x00,
	[PN_BQ25895] = 0x07,
//	[PN_SYV690] = 0x01,
};
/*
static char *pn_str[] = {
	[PN_BQ25890] = "bq25890",
	[PN_BQ25892] = "bq25892",
	[PN_BQ25895] = "bq25895",
};
*/
struct chg_para{
	int vlim;
	int ilim;

	int vreg;
	int ichg;
};

struct bq2589x_platform_data {
	int iprechg;
	int iterm;

	int boostv;
	int boosti;

	int dp_cb1;
	int dm_cb2;

	struct chg_para usb;
};

struct bq2589x {
	struct device *dev;
	struct i2c_client *client;

	int part_no;
	int revision;

	const char *chg_dev_name;
	const char *eint_name;

	bool chg_det_enable;

	enum charger_type chg_type;

	int status;
	int irq;

	struct mutex i2c_rw_lock;

	bool charge_enabled;	/* Register bit status */
	bool power_good;

	struct bq2589x_platform_data *platform_data;
	struct charger_device *chg_dev;
	struct power_supply *psy;
};

enum {
	BQ2589X_DP_DM_CONNECT_AP = 0,
	BQ2589X_DP_DM_CONNECT_CHG,
};

static int hvdcp_type_tmp = 0;
int get_charger_type()
{
	return hvdcp_type_tmp;
}
EXPORT_SYMBOL_GPL(get_charger_type);
static const struct charger_properties bq2589x_chg_props = {
	.alias_name = "bq2589x",
};

static void __bq2589x_switch_dp_dm(struct bq2589x *bq, int dp_dm_direction)
{
	if (dp_dm_direction == BQ2589X_DP_DM_CONNECT_AP || dp_dm_direction == BQ2589X_DP_DM_CONNECT_CHG) {
		gpio_set_value(bq->platform_data->dp_cb1, dp_dm_direction);
		gpio_set_value(bq->platform_data->dm_cb2, dp_dm_direction);
	} else {
		gpio_set_value(bq->platform_data->dp_cb1, 0);
		gpio_set_value(bq->platform_data->dm_cb2, 0);
	}
	pr_info("%s: dp_dm_direction:%d\n", __func__, dp_dm_direction);
}

static int bq2589x_get_vendor_id(struct charger_device *chg_dev, u32 *vendor_id)
{
	struct bq2589x *bq = dev_get_drvdata(&chg_dev->dev);

	if (bq == NULL) {
		pr_err("%s: bq2589x is null\n", __func__);
		return -EINVAL;
	}

	*vendor_id = bq->part_no;

	return 0;
}

static int __bq2589x_read_reg(struct bq2589x *bq, u8 reg, u8 *data)
{
	s32 ret;

	ret = i2c_smbus_read_byte_data(bq->client, reg);
	if (ret < 0) {
		pr_err("i2c read fail: can't read from reg 0x%02X\n", reg);
		return ret;
	}

	*data = (u8) ret;

	return 0;
}

static int __bq2589x_write_reg(struct bq2589x *bq, int reg, u8 val)
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

static int bq2589x_read_byte(struct bq2589x *bq, u8 reg, u8 *data)
{
	int ret;

	mutex_lock(&bq->i2c_rw_lock);
	ret = __bq2589x_read_reg(bq, reg, data);
	mutex_unlock(&bq->i2c_rw_lock);

	return ret;
}

static int bq2589x_write_byte(struct bq2589x *bq, u8 reg, u8 data)
{
	int ret;

	mutex_lock(&bq->i2c_rw_lock);
	ret = __bq2589x_write_reg(bq, reg, data);
	mutex_unlock(&bq->i2c_rw_lock);

	if (ret)
		pr_err("Failed: reg=%02X, ret=%d\n", reg, ret);

	return ret;
}

static int bq2589x_update_bits(struct bq2589x *bq, u8 reg, u8 mask, u8 data)
{
	int ret;
	u8 tmp;

	mutex_lock(&bq->i2c_rw_lock);
	ret = __bq2589x_read_reg(bq, reg, &tmp);
	if (ret) {
		pr_err("Failed: reg=%02X, ret=%d\n", reg, ret);
		goto out;
	}

	tmp &= ~mask;
	tmp |= data & mask;

	ret = __bq2589x_write_reg(bq, reg, tmp);
	if (ret)
		pr_err("Failed: reg=%02X, ret=%d\n", reg, ret);

out:
	mutex_unlock(&bq->i2c_rw_lock);
	return ret;
}

static int bq2589x_disable_12V(struct bq2589x *bq)
{
	u8 val;
	int ret;
	val = BQ2589X_DISABLE_12V;
	val <<= BQ2589X_EN12V_SHIFT;

	ret = bq2589x_update_bits(bq, BQ2589X_REG_01,
				BQ2589X_EN12V_MASK, val);

	return ret;
}

static int bq2589x_force_vindpm_enable(struct bq2589x *bq)
{
	u8 val;
	int ret;
	val = BQ2589X_FORCE_VINDPM_ENABLE;
	val <<= BQ2589X_FORCE_VINDPM_SHIFT;

	ret = bq2589x_update_bits(bq, BQ2589X_REG_0D,
		BQ2589X_FORCE_VINDPM_MASK, val);

	return ret;
}

#if 0
static int bq2589x_dm_set_0P6V(struct bq2589x *bq)
{
	u8 val;
	int ret;
	val = BQ2589X_DM_0P6V;
	val <<= BQ2589X_DMDAC_SHIFT;

	ret = bq2589x_update_bits(bq, BQ2589X_REG_01,
				BQ2589X_DMDAC_MASK, val);

	return ret;
}

static int bq2589x_dp_set_3P3V(struct bq2589x *bq)
{
	u8 val;
	int ret;
	val = BQ2589X_DP_3P3V;
	val <<= BQ2589X_DPDAC_SHIFT;

	ret = bq2589x_update_bits(bq, BQ2589X_REG_01,
				BQ2589X_DPDAC_MASK, val);

	return ret;
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
/* Huaqin add/modify/del for WXYFB-996 by miaozhichao at 2021/3/29 start */
static int bq2589x_disable_maxcen(struct bq2589x *bq)
{
	int ret;
	u8 val = BQ2589X_MAXC_DISABLE << BQ2589X_MAXCEN_SHIFT;

	ret = bq2589x_update_bits(bq, BQ2589X_REG_02,
				BQ2589X_MAXCEN_MASK, val);
	return ret;
}
EXPORT_SYMBOL_GPL(bq2589x_disable_maxcen);
/* Huaqin add/modify/del for WXYFB-996 by miaozhichao at 2021/3/29 end */
static int bq2589x_enable_hvdcp(struct bq2589x *bq)
{
	int ret;
	u8 val = BQ2589X_HVDCP_ENABLE << BQ2589X_HVDCPEN_SHIFT;

	ret = bq2589x_update_bits(bq, BQ2589X_REG_02, 
				BQ2589X_HVDCPEN_MASK, val);
	return ret;
}
EXPORT_SYMBOL_GPL(bq2589x_enable_hvdcp);

static int bq2589x_disable_hvdcp(struct bq2589x *bq)
{
	int ret;
	u8 val = BQ2589X_HVDCP_DISABLE << BQ2589X_HVDCPEN_SHIFT;

	ret = bq2589x_update_bits(bq, BQ2589X_REG_02, 
				BQ2589X_HVDCPEN_MASK, val);
	return ret;
}
EXPORT_SYMBOL_GPL(bq2589x_disable_hvdcp);
static int bq2589x_enable_charger(struct bq2589x *bq)
{
	int ret;

	u8 val = BQ2589X_CHG_ENABLE << BQ2589X_CHG_CONFIG_SHIFT;

	ret = bq2589x_update_bits(bq, BQ2589X_REG_03, 
				BQ2589X_CHG_CONFIG_MASK, val);
	ret = bq2589x_force_vindpm_enable(bq);

	return ret;
}

static int bq2589x_disable_charger(struct bq2589x *bq)
{
	int ret;

	u8 val = BQ2589X_CHG_DISABLE << BQ2589X_CHG_CONFIG_SHIFT;

	ret = bq2589x_update_bits(bq, BQ2589X_REG_03, 
				BQ2589X_CHG_CONFIG_MASK, val);
	return ret;
}

int bq2589x_adc_start(struct bq2589x *bq, bool oneshot)
{
	u8 val;
	int ret;

	ret = bq2589x_read_byte(bq, BQ2589X_REG_02, &val);
	if (ret < 0) {
		dev_err(bq->dev, "%s failed to read register 0x02:%d\n", __func__, ret);
		return ret;
	}

	if (((val & BQ2589X_CONV_RATE_MASK) >> BQ2589X_CONV_RATE_SHIFT) == BQ2589X_ADC_CONTINUE_ENABLE)
		return 0; /*is doing continuous scan*/
	if (oneshot)
		ret = bq2589x_update_bits(bq, BQ2589X_REG_02, BQ2589X_CONV_START_MASK,
					BQ2589X_CONV_START << BQ2589X_CONV_START_SHIFT);
	else
		ret = bq2589x_update_bits(bq, BQ2589X_REG_02, BQ2589X_CONV_RATE_MASK,  
					BQ2589X_ADC_CONTINUE_ENABLE << BQ2589X_CONV_RATE_SHIFT);
	return ret;
}
EXPORT_SYMBOL_GPL(bq2589x_adc_start);

int bq2589x_adc_stop(struct bq2589x *bq)
{
	return bq2589x_update_bits(bq, BQ2589X_REG_02, BQ2589X_CONV_RATE_MASK, 
				BQ2589X_ADC_CONTINUE_DISABLE << BQ2589X_CONV_RATE_SHIFT);
}
EXPORT_SYMBOL_GPL(bq2589x_adc_stop);


int bq2589x_adc_read_battery_volt(struct bq2589x *bq)
{
	uint8_t val;
	int volt;
	int ret;
	ret = bq2589x_read_byte(bq, BQ2589X_REG_0E, &val);
	if (ret < 0) {
		dev_err(bq->dev, "read battery voltage failed :%d\n", ret);
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
	ret = bq2589x_read_byte(bq,  BQ2589X_REG_0F, &val);
	if (ret < 0) {
		dev_err(bq->dev, "read system voltage failed :%d\n", ret);
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
	ret = bq2589x_read_byte(bq, BQ2589X_REG_11, &val);
	if (ret < 0) {
		dev_err(bq->dev, "read vbus voltage failed :%d\n", ret);
		return ret;
	} else{
		pr_err("hao: val=0x%.2x\n", val);
		volt = BQ2589X_VBUSV_BASE + ((val & BQ2589X_VBUSV_MASK) >> BQ2589X_VBUSV_SHIFT) * BQ2589X_VBUSV_LSB ;
		if (BQ2589X_VBUSV_BASE == volt) {
			msleep(3500);
			bq2589x_read_byte(bq, BQ2589X_REG_11, &val);
			volt = BQ2589X_VBUSV_BASE + ((val & BQ2589X_VBUSV_MASK) >> BQ2589X_VBUSV_SHIFT) * BQ2589X_VBUSV_LSB ;
			if (BQ2589X_VBUSV_BASE == volt)
				volt = 0;
		}
		return volt;
	}
}
EXPORT_SYMBOL_GPL(bq2589x_adc_read_vbus_volt);

int bq2589x_adc_read_temperature(struct bq2589x *bq)
{
	uint8_t val;
	int temp;
	int ret;
	ret = bq2589x_read_byte(bq, BQ2589X_REG_10, &val);
	if (ret < 0) {
		dev_err(bq->dev, "read temperature failed :%d\n", ret);
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
	ret = bq2589x_read_byte(bq, BQ2589X_REG_12, &val);
	if (ret < 0) {
		dev_err(bq->dev, "read charge current failed :%d\n", ret);
		return ret;
	} else{
		volt = (int)(BQ2589X_ICHGR_BASE + ((val & BQ2589X_ICHGR_MASK) >> BQ2589X_ICHGR_SHIFT) * BQ2589X_ICHGR_LSB) ;
		return volt;
	}
}
EXPORT_SYMBOL_GPL(bq2589x_adc_read_charge_current);

int bq2589x_set_chargecurrent(struct bq2589x *bq, int curr)
{
	u8 ichg;

	if (curr < BQ2589X_ICHG_BASE)
		curr = BQ2589X_ICHG_BASE;

	ichg = (curr - BQ2589X_ICHG_BASE)/BQ2589X_ICHG_LSB;
	return bq2589x_update_bits(bq, BQ2589X_REG_04, 
						BQ2589X_ICHG_MASK, ichg << BQ2589X_ICHG_SHIFT);

}

int bq2589x_set_term_current(struct bq2589x *bq, int curr)
{
	u8 iterm;
	pr_info("bq2589x_set_term_current:%d\n",curr);
	if (curr < BQ2589X_ITERM_BASE)
		curr = BQ2589X_ITERM_BASE;

	iterm = (curr - BQ2589X_ITERM_BASE) / BQ2589X_ITERM_LSB;
       /* if ( (iterm * BQ2589X_ITERM_LSB + BQ2589X_ITERM_BASE) < curr)
		iterm += 1; */
	return bq2589x_update_bits(bq, BQ2589X_REG_05, 
						BQ2589X_ITERM_MASK, iterm << BQ2589X_ITERM_SHIFT);

}
EXPORT_SYMBOL_GPL(bq2589x_set_term_current);

int bq2589x_set_prechg_current(struct bq2589x *bq, int curr)
{
	u8 iprechg;

	if (curr < BQ2589X_IPRECHG_BASE)
		curr = BQ2589X_IPRECHG_BASE;

	iprechg = (curr - BQ2589X_IPRECHG_BASE) / BQ2589X_IPRECHG_LSB;

	return bq2589x_update_bits(bq, BQ2589X_REG_05, 
						BQ2589X_IPRECHG_MASK, iprechg << BQ2589X_IPRECHG_SHIFT);

}
EXPORT_SYMBOL_GPL(bq2589x_set_prechg_current);

int bq2589x_set_chargevolt(struct bq2589x *bq, int volt)
{
	u8 val;

	if (volt < BQ2589X_VREG_BASE)
		volt = BQ2589X_VREG_BASE;

	val = (volt - BQ2589X_VREG_BASE)/BQ2589X_VREG_LSB;
	return bq2589x_update_bits(bq, BQ2589X_REG_06, 
						BQ2589X_VREG_MASK, val << BQ2589X_VREG_SHIFT);
}

int bq2589x_set_input_volt_limit(struct bq2589x *bq, int volt)
{
	u8 val;

	if (volt < BQ2589X_VINDPM_BASE)
		volt = BQ2589X_VINDPM_BASE;

	val = (volt - BQ2589X_VINDPM_BASE) / BQ2589X_VINDPM_LSB;
	return bq2589x_update_bits(bq, BQ2589X_REG_0D, 
						BQ2589X_VINDPM_MASK, val << BQ2589X_VINDPM_SHIFT);
}

int bq2589x_set_input_current_limit(struct bq2589x *bq, int curr)
{
	u8 val;

	if (curr < BQ2589X_IINLIM_BASE)
		curr = BQ2589X_IINLIM_BASE;

	val = (curr - BQ2589X_IINLIM_BASE) / BQ2589X_IINLIM_LSB;

	return bq2589x_update_bits(bq, BQ2589X_REG_00, BQ2589X_IINLIM_MASK, 
						val << BQ2589X_IINLIM_SHIFT);
}

/*K19A HQ-133295 K19A charger full time by wangqi at 2021/5/6 start*/
int bq2589x_set_ir_compensation(struct bq2589x *bq, int bat_comp, int vclamp)
{
	u8 val_bat_comp;
	u8 val_vclamp;
	pr_err("bq2589x_set_ir_compensation!!!\n");

	val_bat_comp = bat_comp / BQ2589X_BAT_COMP_LSB;
	val_vclamp = vclamp / BQ2589X_VCLAMP_LSB;

	bq2589x_update_bits(bq, BQ2589X_REG_08, BQ2589X_BAT_COMP_MASK,
						val_bat_comp << BQ2589X_BAT_COMP_SHIFT);
	bq2589x_update_bits(bq, BQ2589X_REG_08, BQ2589X_VCLAMP_MASK,
						val_vclamp << BQ2589X_VCLAMP_SHIFT);
	return 0;
}
/*K19A HQ-133295 K19A charger full time by wangqi at 2021/5/6 start*/

int bq2589x_set_watchdog_timer(struct bq2589x *bq, u8 timeout)
{
	u8 val;

	val = (timeout - BQ2589X_WDT_BASE) / BQ2589X_WDT_LSB;
	val <<= BQ2589X_WDT_SHIFT;

	return bq2589x_update_bits(bq, BQ2589X_REG_07, 
						BQ2589X_WDT_MASK, val); 
}
EXPORT_SYMBOL_GPL(bq2589x_set_watchdog_timer);

int bq2589x_disable_watchdog_timer(struct bq2589x *bq)
{
	u8 val = BQ2589X_WDT_DISABLE << BQ2589X_WDT_SHIFT;

	return bq2589x_update_bits(bq, BQ2589X_REG_07, 
						BQ2589X_WDT_MASK, val);
}
EXPORT_SYMBOL_GPL(bq2589x_disable_watchdog_timer);

int bq2589x_reset_watchdog_timer(struct bq2589x *bq)
{
	u8 val = BQ2589X_WDT_RESET << BQ2589X_WDT_RESET_SHIFT;

	return bq2589x_update_bits(bq, BQ2589X_REG_03, 
						BQ2589X_WDT_RESET_MASK, val);
}
EXPORT_SYMBOL_GPL(bq2589x_reset_watchdog_timer);

int bq2589x_force_dpdm(struct bq2589x *bq)
{
	int ret;
	u8 val = BQ2589X_FORCE_DPDM << BQ2589X_FORCE_DPDM_SHIFT;

	ret = bq2589x_update_bits(bq, BQ2589X_REG_02, 
						BQ2589X_FORCE_DPDM_MASK, val);

	pr_info("Force DPDM %s\n", !ret ? "successfully" : "failed");
	
	return ret;

}
EXPORT_SYMBOL_GPL(bq2589x_force_dpdm);
int bq2589x_reset_chip(struct bq2589x *bq)
{
	int ret;
	u8 val = BQ2589X_RESET << BQ2589X_RESET_SHIFT;

	ret = bq2589x_update_bits(bq, BQ2589X_REG_14, 
						BQ2589X_RESET_MASK, val);
	return ret;
}
EXPORT_SYMBOL_GPL(bq2589x_reset_chip);

int bq2589x_enter_hiz_mode(struct bq2589x *bq)
{
	u8 val = BQ2589X_HIZ_ENABLE << BQ2589X_ENHIZ_SHIFT;

	return bq2589x_update_bits(bq, BQ2589X_REG_00, 
						BQ2589X_ENHIZ_MASK, val);

}
EXPORT_SYMBOL_GPL(bq2589x_enter_hiz_mode);

int bq2589x_disable_enilim_pin(struct bq2589x *bq)
{
	u8 val = BQ2589X_ENILIM_DISABLE << BQ2589X_ENILIM_SHIFT;

	return bq2589x_update_bits(bq, BQ2589X_REG_00,
						BQ2589X_ENILIM_MASK, val);

}
EXPORT_SYMBOL_GPL(bq2589x_disable_enilim_pin);

int bq2589x_exit_hiz_mode(struct bq2589x *bq)
{

	u8 val = BQ2589X_HIZ_DISABLE << BQ2589X_ENHIZ_SHIFT;

	return bq2589x_update_bits(bq, BQ2589X_REG_00, 
						BQ2589X_ENHIZ_MASK, val);

}
EXPORT_SYMBOL_GPL(bq2589x_exit_hiz_mode);

int bq2589x_get_hiz_mode(struct bq2589x *bq, u8 *state)
{
	u8 val;
	int ret;

	ret = bq2589x_read_byte(bq, BQ2589X_REG_00, &val);
	if (ret)
		return ret;
	*state = (val & BQ2589X_ENHIZ_MASK) >> BQ2589X_ENHIZ_SHIFT;

	return 0;
}
EXPORT_SYMBOL_GPL(bq2589x_get_hiz_mode);

static int bq2589x_enable_term(struct charger_device *chg_dev, bool enable)
{
	u8 val;
	int ret=0;
	struct bq2589x *bq = dev_get_drvdata(&chg_dev->dev);

	if (enable)
		val = BQ2589X_TERM_ENABLE << BQ2589X_EN_TERM_SHIFT;
	else
		val = BQ2589X_TERM_DISABLE << BQ2589X_EN_TERM_SHIFT;
	if(bq)
		ret = bq2589x_update_bits(bq, BQ2589X_REG_07, 
						BQ2589X_EN_TERM_MASK, val);

	return ret;
}
EXPORT_SYMBOL_GPL(bq2589x_enable_term);

static int bq2589x_enable_term2(struct bq2589x *bq, bool enable)
{
	u8 val;
	int ret=0;
	ret = bq2589x_read_byte(bq, BQ2589X_REG_07, &val);
	if(!ret) {
		val = val & BQ2589X_EN_TERM_MASK;
		val = val >> BQ2589X_EN_TERM_SHIFT;
		if(val != (u8)enable) {
			if (enable)
				val = BQ2589X_TERM_ENABLE << BQ2589X_EN_TERM_SHIFT;
			else
				val = BQ2589X_TERM_DISABLE << BQ2589X_EN_TERM_SHIFT;
			ret = bq2589x_update_bits(bq, BQ2589X_REG_07,
				BQ2589X_EN_TERM_MASK, val);
		}
	}
	return ret;
}

int bq2589x_set_boost_current(struct bq2589x *bq, int curr)
{
	u8 temp;

	if (curr == 500)
		temp = BQ2589X_BOOST_LIM_500MA;
	else if (curr == 700)
		temp = BQ2589X_BOOST_LIM_700MA;
	else if (curr == 1100)
		temp = BQ2589X_BOOST_LIM_1100MA;
	else if (curr == 1600)
		temp = BQ2589X_BOOST_LIM_1600MA;
	else if (curr == 1800)
		temp = BQ2589X_BOOST_LIM_1800MA;
	else if (curr == 2100)
		temp = BQ2589X_BOOST_LIM_2100MA;
	else if (curr == 2400)
		temp = BQ2589X_BOOST_LIM_2400MA;
	else
		temp = BQ2589X_BOOST_LIM_1300MA;

	return bq2589x_update_bits(bq, BQ2589X_REG_0A, 
				BQ2589X_BOOST_LIM_MASK, 
				temp << BQ2589X_BOOST_LIM_SHIFT);

}

static int bq2589x_enable_auto_dpdm(struct bq2589x* bq, bool enable)
{
	u8 val;
	int ret;

	if (enable)
		val = BQ2589X_AUTO_DPDM_ENABLE << BQ2589X_AUTO_DPDM_EN_SHIFT;
	else
		val = BQ2589X_AUTO_DPDM_DISABLE << BQ2589X_AUTO_DPDM_EN_SHIFT;

	ret = bq2589x_update_bits(bq, BQ2589X_REG_02, 
						BQ2589X_AUTO_DPDM_EN_MASK, val);

	return ret;

}
EXPORT_SYMBOL_GPL(bq2589x_enable_auto_dpdm);

int bq2589x_set_boost_voltage(struct bq2589x *bq, int volt)
{
	u8 val = 0;

	if (volt < BQ2589X_BOOSTV_BASE)
		volt = BQ2589X_BOOSTV_BASE;
	if (volt > BQ2589X_BOOSTV_BASE 
			+ (BQ2589X_BOOSTV_MASK >> BQ2589X_BOOSTV_SHIFT) 
			* BQ2589X_BOOSTV_LSB)
		volt = BQ2589X_BOOSTV_BASE 
			+ (BQ2589X_BOOSTV_MASK >> BQ2589X_BOOSTV_SHIFT) 
			* BQ2589X_BOOSTV_LSB;

	val = ((volt - BQ2589X_BOOSTV_BASE) / BQ2589X_BOOSTV_LSB) 
			<< BQ2589X_BOOSTV_SHIFT;

	return bq2589x_update_bits(bq, BQ2589X_REG_0A, 
				BQ2589X_BOOSTV_MASK, val);


}
EXPORT_SYMBOL_GPL(bq2589x_set_boost_voltage);

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

	ret = bq2589x_read_byte(bq, BQ2589X_REG_13, &val);
	if (ret < 0) {
		dev_err(bq->dev, "read vbus voltage failed :%d\n", ret);
		return ret;
	} else{
		curr = BQ2589X_IDPM_LIM_BASE + ((val & BQ2589X_IDPM_LIM_MASK) >> BQ2589X_IDPM_LIM_SHIFT) * BQ2589X_IDPM_LIM_LSB ;
		return curr;
	}
}
EXPORT_SYMBOL_GPL(bq2589x_read_idpm_limit);

static int bq2589x_enable_safety_timer(struct bq2589x *bq)
{
	const u8 val = BQ2589X_CHG_TIMER_ENABLE << BQ2589X_EN_TIMER_SHIFT;

	return bq2589x_update_bits(bq, BQ2589X_REG_07, BQ2589X_EN_TIMER_MASK,
				   val);
}
EXPORT_SYMBOL_GPL(bq2589x_enable_safety_timer);

static int bq2589x_disable_safety_timer(struct bq2589x *bq)
{
	const u8 val = BQ2589X_CHG_TIMER_DISABLE << BQ2589X_EN_TIMER_SHIFT;

	return bq2589x_update_bits(bq, BQ2589X_REG_07, BQ2589X_EN_TIMER_MASK,
				   val);

}
EXPORT_SYMBOL_GPL(bq2589x_disable_safety_timer);

static struct bq2589x_platform_data *bq2589x_parse_dt(struct device_node *np,
						      struct bq2589x *bq)
{
	int ret;
	struct bq2589x_platform_data *pdata;

	pdata = devm_kzalloc(bq->dev, sizeof(struct bq2589x_platform_data),
			     GFP_KERNEL);
	if (!pdata)
		return NULL;

	if (of_property_read_string(np, "charger_name", &bq->chg_dev_name) < 0) {
		bq->chg_dev_name = "primary_chg";
		pr_warn("no charger name\n");
	}

	if (of_property_read_string(np, "eint_name", &bq->eint_name) < 0) {
		bq->eint_name = "chr_stat";
		pr_warn("no eint name\n");
	}

	bq->chg_det_enable =
	    of_property_read_bool(np, "ti,bq2589x,charge-detect-enable");

	ret = of_property_read_u32(np, "ti,bq2589x,usb-vlim", &pdata->usb.vlim);
	if (ret) {
		pdata->usb.vlim = 4500;
		pr_err("Failed to read node of ti,bq2589x,usb-vlim\n");
	}

	ret = of_property_read_u32(np, "ti,bq2589x,usb-ilim", &pdata->usb.ilim);
	if (ret) {
		pdata->usb.ilim = 2000;
		pr_err("Failed to read node of ti,bq2589x,usb-ilim\n");
	}

	ret = of_property_read_u32(np, "ti,bq2589x,usb-vreg", &pdata->usb.vreg);
	if (ret) {
		pdata->usb.vreg = 4200;
		pr_err("Failed to read node of ti,bq2589x,usb-vreg\n");
	}

	ret = of_property_read_u32(np, "ti,bq2589x,usb-ichg", &pdata->usb.ichg);
	if (ret) {
		pdata->usb.ichg = 2000;
		pr_err("Failed to read node of ti,bq2589x,usb-ichg\n");
	}

	ret = of_property_read_u32(np, "ti,bq2589x,precharge-current",
				   &pdata->iprechg);
	if (ret) {
		pdata->iprechg = 256;
		pr_err("Failed to read node of ti,bq2589x,precharge-current\n");
	}

	ret = of_property_read_u32(np, "ti,bq2589x,termination-current",
				   &pdata->iterm);
	if (ret) {
		pdata->iterm = 180;
		pr_err
		    ("Failed to read node of ti,bq2589x,termination-current\n");
	}

	ret =
	    of_property_read_u32(np, "ti,bq2589x,boost-voltage",
				 &pdata->boostv);
	if (ret) {
		pdata->boostv = 5000;
		pr_err("Failed to read node of ti,bq2589x,boost-voltage\n");
	}

	ret =
	    of_property_read_u32(np, "ti,bq2589x,boost-current",
				 &pdata->boosti);
	if (ret) {
		pdata->boosti = 1200;
		pr_err("Failed to read node of ti,bq2589x,boost-current\n");
	}

	pdata->dp_cb1 = of_get_named_gpio(np, "ti,CB1", 0);
	if (!gpio_is_valid(pdata->dp_cb1))
		pr_err("Failed to read node of ti,CB1\n");

	pdata->dm_cb2 = of_get_named_gpio(np, "ti,CB2", 0);
	if (!gpio_is_valid(pdata->dm_cb2))
		pr_err("Failed to read node of ti,CB2\n");

	return pdata;
}

static int bq2589x_get_charger_type(struct bq2589x *bq, enum charger_type *type)
{
	int ret;

	u8 reg_val = 0;
	int vbus_stat = 0;
	enum charger_type chg_type = CHARGER_UNKNOWN;
	static struct power_supply * usb_psy = NULL;
	static struct mt_charger *mt_chg = NULL;
	static const enum power_supply_type const smblib_apsd_results[] = {
		POWER_SUPPLY_TYPE_UNKNOWN,
		POWER_SUPPLY_TYPE_USB,
		POWER_SUPPLY_TYPE_USB_CDP,
		POWER_SUPPLY_TYPE_USB_FLOAT,
		POWER_SUPPLY_TYPE_USB_DCP,
		POWER_SUPPLY_TYPE_USB_FLOAT,
		POWER_SUPPLY_TYPE_USB_FLOAT,
		POWER_SUPPLY_TYPE_USB_FLOAT,
		POWER_SUPPLY_TYPE_USB_FLOAT,
		POWER_SUPPLY_TYPE_USB_HVDCP,
	};
	ret = bq2589x_read_byte(bq, BQ2589X_REG_0B, &reg_val);

	if (ret)
		return ret;

	vbus_stat = (reg_val & BQ2589X_VBUS_STAT_MASK);
	vbus_stat >>= BQ2589X_VBUS_STAT_SHIFT;

	switch (vbus_stat) {

	case BQ2589X_VBUS_TYPE_NONE:
		chg_type = CHARGER_UNKNOWN;
		hvdcp_type_tmp = CHARGER_UNKNOWN;
		break;
	case BQ2589X_VBUS_TYPE_SDP:
		__bq2589x_switch_dp_dm(bq, BQ2589X_DP_DM_CONNECT_AP);
		chg_type = STANDARD_HOST;
		break;
	case BQ2589X_VBUS_TYPE_CDP:
		__bq2589x_switch_dp_dm(bq, BQ2589X_DP_DM_CONNECT_AP);
		chg_type = CHARGING_HOST;
		break;
	case BQ2589X_VBUS_TYPE_DCP:
		chg_type = STANDARD_CHARGER;
		break;
	case BQ2589X_VBUS_TYPE_HVDCP:
		chg_type = HVDCP_CHARGER;
		hvdcp_type_tmp = HVDCP_CHARGER;
		break;
	case BQ2589X_VBUS_TYPE_UNKNOWN:
		__bq2589x_switch_dp_dm(bq, BQ2589X_DP_DM_CONNECT_AP);
		chg_type = NONSTANDARD_CHARGER;
		break;
	case BQ2589X_VBUS_TYPE_NON_STD:
		chg_type = NONSTANDARD_CHARGER;
		break;
	case BQ2589X_VBUS_TYPE_OTG:
		__bq2589x_switch_dp_dm(bq, BQ2589X_DP_DM_CONNECT_AP);
		chg_type = CHARGER_UNKNOWN;
		hvdcp_type_tmp = CHARGER_UNKNOWN;
		break;
	default:
		chg_type = NONSTANDARD_CHARGER;
		break;
	}
	*type = chg_type;

	if(usb_psy == NULL)
		usb_psy = power_supply_get_by_name("usb");

	if(usb_psy != NULL)
		mt_chg = power_supply_get_drvdata(usb_psy);

	if(mt_chg != NULL)
		mt_chg->usb_desc.type = smblib_apsd_results[chg_type];
	pr_err("vbus_stat:%d ,chg_type:%d\n", vbus_stat,chg_type);
	return 0;
}

static int bq2589x_inform_charger_type(struct bq2589x *bq)
{
	int ret = 0;
	union power_supply_propval propval;

	if (!bq->psy) {
		bq->psy = power_supply_get_by_name("charger");
		if (!bq->psy)
			return -ENODEV;
	}

	if (bq->chg_type != CHARGER_UNKNOWN)
		propval.intval = 1;
	else
		propval.intval = 0;

	ret = power_supply_set_property(bq->psy, POWER_SUPPLY_PROP_ONLINE,
					&propval);

	if (ret < 0)
		pr_notice("inform power supply online failed:%d\n", ret);

	propval.intval = bq->chg_type;
	if (propval.intval == HVDCP_CHARGER) {
		propval.intval = STANDARD_CHARGER;
	}

	ret = power_supply_set_property(bq->psy,
					POWER_SUPPLY_PROP_CHARGE_TYPE,
					&propval);

	if (ret < 0)
		pr_notice("inform power supply charge type failed:%d\n", ret);

	return ret;
}

static int bq2589x_enable_chg_type_det(struct charger_device *chg_dev, bool en)
{
	int ret;
	struct bq2589x *bq = dev_get_drvdata(&chg_dev->dev);
	__bq2589x_switch_dp_dm(bq, BQ2589X_DP_DM_CONNECT_CHG);
	msleep(50);
	ret = bq2589x_get_charger_type(bq, &bq->chg_type);
	if (!ret)
		bq2589x_inform_charger_type(bq);
	pr_err("end,bq->chg_type = %d\n",bq->chg_type);
	return 0;
}

extern struct charger_device *chg_dev_retry;

static irqreturn_t bq2589x_irq_handler(int irq, void *data)
{
	int ret;
	u8 reg_val;
	bool prev_pg;
	enum charger_type prev_chg_type;
	struct bq2589x *bq = data;
	ret = bq2589x_read_byte(bq, BQ2589X_REG_0B, &reg_val);
	if (ret)
		return IRQ_HANDLED;

	prev_pg = bq->power_good;

	bq->power_good = !!(reg_val & BQ2589X_PG_STAT_MASK);

	if (!prev_pg && bq->power_good) {
		pr_err("adapter/usb inserted\n");
	}else if (prev_pg && !bq->power_good){
		bq2589x_force_dpdm(bq);
		msleep(50);
		if (bq->chg_type == NONSTANDARD_CHARGER) {
			ret = charger_dev_enable_chg_type_det(chg_dev_retry, true);
			if (ret < 0)
				pr_err("force DPDM retry bc12 failed\n");
			pr_err("force DPDM retry bc12 succ\n");
		}
		msleep(50);
		__bq2589x_switch_dp_dm(bq, BQ2589X_DP_DM_CONNECT_AP);
		hvdcp_type_tmp = CHARGER_UNKNOWN;
		pr_err("adapter/usb removed chg_type = %d\n", bq->chg_type);
	}else{
		pr_err("prev_pg = %d  bq->power_good = %d\n",prev_pg,bq->power_good);
	}

	prev_chg_type = bq->chg_type;
	ret = bq2589x_get_charger_type(bq, &bq->chg_type);
	if (!ret &&prev_chg_type != bq->chg_type)
		bq2589x_inform_charger_type(bq);

	return IRQ_HANDLED;
}

static int bq2589x_register_interrupt(struct bq2589x *bq)
{
	int ret = 0;

	ret = devm_request_threaded_irq(bq->dev, bq->client->irq, NULL,
					bq2589x_irq_handler,
					IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
					"chr_stat", bq);
	if (ret < 0) {
		pr_err("request thread irq failed:%d\n", ret);
		return ret;
	}else{
		pr_err("request thread irq pass:%d  bq->client->irq =%d\n", ret, bq->client->irq);
	}

	enable_irq_wake(bq->irq);

	return 0;
}
static void bq2589x_dump_regs(struct bq2589x *bq);

static int bq2589x_cb_init(struct bq2589x *bq)
{
	int ret;

	if (bq == NULL) {
		pr_err("bq is NULL, return\n");
		return -EINVAL;
	}

	ret = gpio_request(bq->platform_data->dp_cb1, "bq2589x-cb1");
	if (ret) {
		pr_err("unable to request gpio [%d]\n", bq->platform_data->dp_cb1);
		return ret;
	}

	ret = gpio_request(bq->platform_data->dm_cb2, "bq2589x-cb2");
	if (ret) {
		pr_err("unable to request gpio [%d]\n", bq->platform_data->dp_cb1);
		return ret;
	}

	ret = gpio_direction_output(bq->platform_data->dp_cb1, 0);
	if (ret) {
		gpio_free(bq->platform_data->dp_cb1);
		pr_err("unable to set direction of gpio cb1\n");
		return ret;
	}

	ret = gpio_direction_output(bq->platform_data->dm_cb2, 0);
	if (ret) {
		gpio_free(bq->platform_data->dp_cb1);
		gpio_free(bq->platform_data->dm_cb2);
		pr_err("unable to set direction of gpio cb2\n");
		return ret;
	}

	pr_info("cb init successfully\n");
	return ret;
}

static int bq2589x_init_device(struct bq2589x *bq)
{
	int ret;
	int id_dis = 0;
	u8 reg_val = 0;
	pr_err("bq2589x_dump_regs before init: \n");
	bq2589x_dump_regs(bq);
	ret = bq2589x_disable_12V(bq);
	if (ret)
		pr_err("Failed to disable 12V, ret = %d\n", ret);
	bq2589x_enable_auto_dpdm(bq, true);
	bq2589x_disable_watchdog_timer(bq);
	bq2589x_disable_safety_timer(bq);
	bq2589x_set_ir_compensation(bq, 0, 0);

	ret = bq2589x_force_vindpm_enable(bq);
	if (ret)
		pr_err("Failed to enable force vindpm, ret = %d\n", ret);

	ret = bq2589x_set_prechg_current(bq, bq->platform_data->iprechg);
	if (ret)
		pr_err("Failed to set prechg current, ret = %d\n", ret);

	ret = bq2589x_set_boost_voltage(bq, bq->platform_data->boostv);
	if (ret)
		pr_err("Failed to set boost voltage, ret = %d\n", ret);

	ret = bq2589x_set_boost_current(bq, bq->platform_data->boosti);
	if (ret)
		pr_err("Failed to set boost current, ret = %d\n", ret);
	ret = bq2589x_disable_maxcen(bq);
	if (ret)
		pr_err("Failed to set disable maxcen, ret = %d\n", ret);
	ret = bq2589x_read_byte(bq,BQ2589X_REG_14,&reg_val);
	id_dis = (reg_val & BQ2589X_PN_MASK);
	id_dis >>= BQ2589X_PN_SHIFT;
	if(id_dis == 3){
		ret = bq2589x_set_term_current(bq, 200);
		ret = bq2589x_disable_hvdcp(bq);
		pr_err("disable hvdcp,ret = %d\n",ret);
	}else{ // SYV690
		ret = bq2589x_set_term_current(bq, 200);
		ret = bq2589x_enable_hvdcp(bq);
		pr_err("enable hvdcp,ret = %d\n",ret);
	}
	ret = bq2589x_enable_ico(bq, 0);//disable ico
	if (ret)
		pr_err("Failed to disable ico, ret = %d\n", ret);

	ret = bq2589x_exit_hiz_mode(bq);
	if (ret)
		pr_err("Failed to set exit_hiz_mode, ret = %d\n", ret);

	ret = bq2589x_disable_enilim_pin(bq);
	if (ret)
		pr_err("Failed to disable enilim_pin, ret = %d\n", ret);

	ret = bq2589x_cb_init(bq);
	if (ret)
		pr_err("Failed to init cb gpio, ret = %d\n", ret);

	ret = bq2589x_enable_hvdcp(bq);
	bq2589x_enable_auto_dpdm(bq, true);
	ret = bq2589x_force_dpdm(bq);
	pr_err("bq2589x_dump_regs after init: \n");
	bq2589x_dump_regs(bq);

	return 0;
}

static void determine_initial_status(struct bq2589x *bq)
{
	bq2589x_irq_handler(bq->irq, (void *) bq);
}

static int bq2589x_detect_device(struct bq2589x *bq)
{
	int ret;
	u8 data;

	ret = bq2589x_read_byte(bq, BQ2589X_REG_14, &data);
	if (!ret) {
		bq->part_no = (data & BQ2589X_PN_MASK) >> BQ2589X_PN_SHIFT;
		bq->revision =
		    (data & BQ2589X_DEV_REV_MASK) >> BQ2589X_DEV_REV_SHIFT;
	}

	return ret;
}

static void bq2589x_dump_regs(struct bq2589x *bq)
{
	int addr;
	u8 val;
	int ret;

	for (addr = 0x0; addr <= 0x14; addr++) {
		ret = bq2589x_read_byte(bq, addr, &val);
		if (ret == 0)
			pr_err("Reg[%.2x] = 0x%.2x\n", addr, val);
	}
}

static ssize_t
bq2589x_show_registers(struct device *dev, struct device_attribute *attr,
		       char *buf)
{
	struct bq2589x *bq = dev_get_drvdata(dev);
	u8 addr;
	u8 val;
	u8 tmpbuf[200];
	int len;
	int idx = 0;
	int ret;

	idx = snprintf(buf, PAGE_SIZE, "%s:\n", "bq2589x Reg");
	for (addr = 0x0; addr <= 0x14; addr++) {
		ret = bq2589x_read_byte(bq, addr, &val);
		if (ret == 0) {
			len = snprintf(tmpbuf, PAGE_SIZE - idx,
				       "Reg[%.2x] = 0x%.2x\n", addr, val);
			memcpy(&buf[idx], tmpbuf, len);
			idx += len;
		}
	}

	return idx;
}

static ssize_t
bq2589x_store_registers(struct device *dev,
			struct device_attribute *attr, const char *buf,
			size_t count)
{
	struct bq2589x *bq = dev_get_drvdata(dev);
	int ret;
	unsigned int reg;
	unsigned int val;

	ret = sscanf(buf, "%x %x", &reg, &val);
	if (ret == 2 && reg < 0x14) {
		bq2589x_write_byte(bq, (unsigned char) reg,
				   (unsigned char) val);
	}

	return count;
}

static DEVICE_ATTR(registers, S_IRUGO | S_IWUSR, bq2589x_show_registers,
		   bq2589x_store_registers);

static struct attribute *bq2589x_attributes[] = {
	&dev_attr_registers.attr,
	NULL,
};

static const struct attribute_group bq2589x_attr_group = {
	.attrs = bq2589x_attributes,
};

static int bq2589x_charging(struct charger_device *chg_dev, bool enable)
{

	struct bq2589x *bq = dev_get_drvdata(&chg_dev->dev);
	int ret = 0;
	u8 val;

	if (enable)
		ret = bq2589x_enable_charger(bq);
	else
		ret = bq2589x_disable_charger(bq);

	pr_err("%s charger %s\n", enable ? "enable" : "disable",
	       !ret ? "successfully" : "failed");

	ret = bq2589x_read_byte(bq, BQ2589X_REG_03, &val);

	if (!ret)
		bq->charge_enabled = !!(val & BQ2589X_CHG_CONFIG_MASK);

	return ret;
}

static int bq2589x_enable_hiz(struct charger_device *chg_dev, bool enable)
{
	struct bq2589x *bq = dev_get_drvdata(&chg_dev->dev);
	int ret = 0;
	pr_err("bq2589x_enable_hiz : %d\n", enable);

	if(enable){
		ret = bq2589x_enter_hiz_mode(bq);
	}else{
		ret = bq2589x_exit_hiz_mode(bq);
		ret = bq2589x_enable_charger(bq);
		ret = bq2589x_force_dpdm(bq);
	}

	return ret;
}

static int bq2589x_plug_in(struct charger_device *chg_dev)
{
	int ret;
	struct bq2589x *bq = dev_get_drvdata(&chg_dev->dev);

	__bq2589x_switch_dp_dm(bq, BQ2589X_DP_DM_CONNECT_CHG);
	bq2589x_force_dpdm(bq);
	ret = bq2589x_charging(chg_dev, true);

	if (ret)
		pr_err("Failed to enable charging:%d\n", ret);

	return ret;
}

static int bq2589x_plug_out(struct charger_device *chg_dev)
{
	int ret;
	ret = bq2589x_charging(chg_dev, false);

	if (ret)
		pr_err("Failed to disable charging:%d\n", ret);

	return ret;
}

static int bq2589x_dump_register(struct charger_device *chg_dev)
{
	struct bq2589x *bq = dev_get_drvdata(&chg_dev->dev);

	bq2589x_dump_regs(bq);

	return 0;
}

static int bq2589x_is_charging_enable(struct charger_device *chg_dev, bool *en)
{
	struct bq2589x *bq = dev_get_drvdata(&chg_dev->dev);

	*en = bq->charge_enabled;

	return 0;
}

static int bq2589x_is_charging_done(struct charger_device *chg_dev, bool *done)
{
	struct bq2589x *bq = dev_get_drvdata(&chg_dev->dev);
	int ret;
	u8 val;

	ret = bq2589x_read_byte(bq, BQ2589X_REG_0B, &val);
	if (!ret) {
		val = val & BQ2589X_CHRG_STAT_MASK;
		val = val >> BQ2589X_CHRG_STAT_SHIFT;
		*done = (val == BQ2589X_CHRG_STAT_CHGDONE);
	}

	return ret;
}

static int bq2589x_set_ichg(struct charger_device *chg_dev, u32 curr)
{
	struct bq2589x *bq = dev_get_drvdata(&chg_dev->dev);

	pr_err("charge curr = %d\n", curr);

	return bq2589x_set_chargecurrent(bq, curr / 1000);
}

static int bq2589x_get_ichg(struct charger_device *chg_dev, u32 *curr)
{
	struct bq2589x *bq = dev_get_drvdata(&chg_dev->dev);
	u8 reg_val;
	int ichg;
	int ret;

	ret = bq2589x_read_byte(bq, BQ2589X_REG_04, &reg_val);
	if (!ret) {
		ichg = (reg_val & BQ2589X_ICHG_MASK) >> BQ2589X_ICHG_SHIFT;
		ichg = ichg * BQ2589X_ICHG_LSB + BQ2589X_ICHG_BASE;
		*curr = ichg * 1000;
	}

	return ret;
}

static int bq2589x_get_min_ichg(struct charger_device *chg_dev, u32 *curr)
{
	*curr = 60 * 1000;

	return 0;
}

static int bq2589x_set_vchg(struct charger_device *chg_dev, u32 volt)
{
	struct bq2589x *bq = dev_get_drvdata(&chg_dev->dev);
	struct power_supply *cp_psy;
	union power_supply_propval pval = {0,};
	if(!bq) {
		pr_err("%s: no bq\n", __func__);
		return 0;
	}
	cp_psy = power_supply_get_by_name("bq2597x-standalone");
	if (!cp_psy) {
		pr_err("%s: no cp psy\n", __func__);
		return 0;
	}
	power_supply_get_property(cp_psy, POWER_SUPPLY_PROP_CHARGING_ENABLED, &pval);
	if (pval.intval) {
		bq2589x_enable_term2(bq,false);
		volt = 4608000;
	} else {
		bq2589x_enable_term2(bq,true);
	}

	pr_err("charge volt = %d\n", volt);

	return bq2589x_set_chargevolt(bq, volt / 1000);
}

static int bq2589x_get_vchg(struct charger_device *chg_dev, u32 *volt)
{
	struct bq2589x *bq = dev_get_drvdata(&chg_dev->dev);
	u8 reg_val;
	int vchg;
	int ret;

	ret = bq2589x_read_byte(bq, BQ2589X_REG_06, &reg_val);
	if (!ret) {
		vchg = (reg_val & BQ2589X_VREG_MASK) >> BQ2589X_VREG_SHIFT;
		vchg = vchg * BQ2589X_VREG_LSB + BQ2589X_VREG_BASE;
		*volt = vchg * 1000;
	}

	return ret;
}

static int bq2589x_set_ivl(struct charger_device *chg_dev, u32 volt)
{
	struct bq2589x *bq = dev_get_drvdata(&chg_dev->dev);

	pr_err("vindpm volt = %d\n", volt);

	return bq2589x_set_input_volt_limit(bq, volt / 1000);

}

static int bq2589x_set_ieoc(struct charger_device *chg_dev, u32 curr)
{
	struct bq2589x *bq = dev_get_drvdata(&chg_dev->dev);

	pr_err("ieoc = %d\n", curr);
	/*
	if (curr == 200000 && bq->part_no == 3)
		curr = 256000;
        */
	if(bq)
		{
			if( bq->part_no == 3) {
				pr_info("charger vendor:1\n");
			} else {
				pr_info("charger vendor:0\n");
			}
		}
	return bq2589x_set_term_current(bq, curr / 1000);
}

static int bq2589x_set_icl(struct charger_device *chg_dev, u32 curr)
{
	struct bq2589x *bq = dev_get_drvdata(&chg_dev->dev);
	if(curr == 0) {
		curr = 2000000;
	}
	pr_err("indpm curr = %d\n", curr);

	return bq2589x_set_input_current_limit(bq, curr / 1000);
}

static int bq2589x_get_icl(struct charger_device *chg_dev, u32 *curr)
{
	struct bq2589x *bq = dev_get_drvdata(&chg_dev->dev);
	u8 reg_val;
	int icl;
	int ret;

	ret = bq2589x_read_byte(bq, BQ2589X_REG_00, &reg_val);
	if (!ret) {
		icl = (reg_val & BQ2589X_IINLIM_MASK) >> BQ2589X_IINLIM_SHIFT;
		icl = icl * BQ2589X_IINLIM_LSB + BQ2589X_IINLIM_BASE;
		*curr = icl * 1000;
	}

	return ret;

}

static int bq2589x_kick_wdt(struct charger_device *chg_dev)
{
	struct bq2589x *bq = dev_get_drvdata(&chg_dev->dev);

	return bq2589x_reset_watchdog_timer(bq);
}

static int bq2589x_set_otg(struct charger_device *chg_dev, bool en)
{
	int ret;
	struct bq2589x *bq = dev_get_drvdata(&chg_dev->dev);

	if (en) {
		__bq2589x_switch_dp_dm(bq, BQ2589X_DP_DM_CONNECT_AP);
		ret = bq2589x_enable_otg(bq);
	} else {
		ret = bq2589x_disable_otg(bq);
		__bq2589x_switch_dp_dm(bq, BQ2589X_DP_DM_CONNECT_CHG);
	}

	pr_err("%s OTG %s\n", en ? "enable" : "disable",
	       !ret ? "successfully" : "failed");

	return ret;
}

static int bq2589x_set_safety_timer(struct charger_device *chg_dev, bool en)
{
	struct bq2589x *bq = dev_get_drvdata(&chg_dev->dev);
	int ret;

	if (en)
		ret = bq2589x_enable_safety_timer(bq);
	else
		ret = bq2589x_disable_safety_timer(bq);

	return ret;
}

static int bq2589x_is_safety_timer_enabled(struct charger_device *chg_dev,
					   bool *en)
{
	struct bq2589x *bq = dev_get_drvdata(&chg_dev->dev);
	int ret;
	u8 reg_val;

	ret = bq2589x_read_byte(bq, BQ2589X_REG_07, &reg_val);

	if (!ret)
		*en = !!(reg_val & BQ2589X_EN_TIMER_MASK);

	return ret;
}

static int bq2589x_set_boost_ilmt(struct charger_device *chg_dev, u32 curr)
{
	struct bq2589x *bq = dev_get_drvdata(&chg_dev->dev);
	int ret;

	pr_err("otg curr = %d\n", curr);

	ret = bq2589x_set_boost_current(bq, curr / 1000);

	return ret;
}

/* 2021.7.7 LQ-zhenghao bring up start */
static int bq2589x_do_event(struct charger_device *chg_dev, u32 event,
			    u32 args)
{
	if (chg_dev == NULL)
		return -EINVAL;

	pr_info("%s: event = %d\n", __func__, event);
	switch (event) {
	case EVENT_EOC:
		charger_dev_notify(chg_dev, CHARGER_DEV_NOTIFY_EOC);
		break;
	case EVENT_RECHARGE:
		charger_dev_notify(chg_dev, CHARGER_DEV_NOTIFY_RECHG);
		break;
	default:
		break;
	}

	return 0;
}
/* 2021.7.7 LQ-zhenghao bring up end */

static int bq2589x_get_vbus(struct charger_device *chg_dev, u32 *vbus)
{
	struct bq2589x *bq = dev_get_drvdata(&chg_dev->dev);

	bq2589x_adc_start(bq, 1);
	*vbus = bq2589x_adc_read_vbus_volt(bq);
	pr_err("%s: vbus = %d\n", __func__, *vbus);
	if(*vbus < 0)
		return *vbus;

	return *vbus;
}

static int bq2589x_get_ibus(struct charger_device *chg_dev, u32 *ibus)
{
	struct bq2589x *bq = dev_get_drvdata(&chg_dev->dev);

	bq2589x_adc_start(bq, 1);
	*ibus = bq2589x_adc_read_charge_current(bq);
	pr_err("%s: ibus = %d\n", __func__, *ibus);
	if(*ibus < 0)
		return *ibus;

	return *ibus;
}

static struct charger_ops bq2589x_chg_ops = {
	/* Normal charging */
	.plug_in = bq2589x_plug_in,
	.plug_out = bq2589x_plug_out,
	.dump_registers = bq2589x_dump_register,
	.enable = bq2589x_charging,
	.enable_hz = bq2589x_enable_hiz,
	.is_enabled = bq2589x_is_charging_enable,
	.get_charging_current = bq2589x_get_ichg,
	.set_charging_current = bq2589x_set_ichg,
	.get_input_current = bq2589x_get_icl,
	.set_input_current = bq2589x_set_icl,
	.get_constant_voltage = bq2589x_get_vchg,
	.set_constant_voltage = bq2589x_set_vchg,
	.kick_wdt = bq2589x_kick_wdt,
	.set_mivr = bq2589x_set_ivl,
	.set_eoc_current = bq2589x_set_ieoc,
	.enable_termination = bq2589x_enable_term,
	.is_charging_done = bq2589x_is_charging_done,
	.get_min_charging_current = bq2589x_get_min_ichg,
	.enable_chg_type_det = bq2589x_enable_chg_type_det,
	.get_ibus_adc = bq2589x_get_ibus,
	.get_vbus_adc = bq2589x_get_vbus,

	/* Safety timer */
	.enable_safety_timer = bq2589x_set_safety_timer,
	.is_safety_timer_enabled = bq2589x_is_safety_timer_enabled,

	/* Power path */
	.enable_powerpath = NULL,
	.is_powerpath_enabled = NULL,

	/* OTG */
	.enable_otg = bq2589x_set_otg,
	.set_boost_current_limit = bq2589x_set_boost_ilmt,
	.enable_discharge = NULL,

	/* PE+/PE+20 */
	.send_ta_current_pattern = NULL,
	.set_pe20_efficiency_table = NULL,
	.send_ta20_current_pattern = NULL,
	.enable_cable_drop_comp = NULL,

	/* ADC */
	.get_tchg_adc = NULL,
	/* 2021.7.7 LQ-zhenghao bring up start */
	.event = bq2589x_do_event,
	/* 2021.7.7 LQ-zhenghao bring up end */

	/* vendor info */
	.get_vendor_id = bq2589x_get_vendor_id,
};

static struct of_device_id bq2589x_charger_match_table[] = {
	{
	 .compatible = "ti,bq25890",
	 .data = &pn_data[PN_BQ25890],
	 },
	{
	 .compatible = "ti,bq25892",
	 .data = &pn_data[PN_BQ25892],
	 },
	{
	 .compatible = "ti,bq25895",
	 .data = &pn_data[PN_BQ25895],
	 //},
	//{
	 //.compatible = "silergy,syv690",
	 //.data = &pn_data[PN_SYV690],
	 },
	{},
};
MODULE_DEVICE_TABLE(of, bq2589x_charger_match_table);


static int bq2589x_charger_probe(struct i2c_client *client,
				 const struct i2c_device_id *id)
{
	struct bq2589x *bq;
	const struct of_device_id *match;
	struct device_node *node = client->dev.of_node;

	int ret = 0;

	bq = devm_kzalloc(&client->dev, sizeof(struct bq2589x), GFP_KERNEL);
	if (!bq)
		return -ENOMEM;

	bq->dev = &client->dev;
	bq->client = client;

	i2c_set_clientdata(client, bq);

	mutex_init(&bq->i2c_rw_lock);

	ret = bq2589x_detect_device(bq);
	if (ret) {
		pr_err("No bq2589x device found!\n");
		return -ENODEV;
	}

	match = of_match_node(bq2589x_charger_match_table, node);
	if (match == NULL) {
		pr_err("device tree match not found\n");
		return -EINVAL;
	}

	bq->platform_data = bq2589x_parse_dt(node, bq);

	if (!bq->platform_data) {
		pr_err("No platform data provided.\n");
		return -EINVAL;
	}

	ret = bq2589x_init_device(bq);
	if (ret) {
		pr_err("Failed to init device\n");
		return ret;
	}

	bq2589x_register_interrupt(bq);

	bq->chg_dev = charger_device_register(bq->chg_dev_name,
					      &client->dev, bq,
					      &bq2589x_chg_ops,
					      &bq2589x_chg_props);
	if (IS_ERR_OR_NULL(bq->chg_dev)) {
		ret = PTR_ERR(bq->chg_dev);
		return ret;
	}

	ret = sysfs_create_group(&bq->dev->kobj, &bq2589x_attr_group);
	if (ret)
		dev_err(bq->dev, "failed to register sysfs. err: %d\n", ret);

	determine_initial_status(bq);

	pr_err("bq2589x probe successfully, Part Num:%d, Revision:%d\n!",
	       bq->part_no, bq->revision);

	return 0;
}

static int bq2589x_charger_remove(struct i2c_client *client)
{
	struct bq2589x *bq = i2c_get_clientdata(client);

	mutex_destroy(&bq->i2c_rw_lock);

	sysfs_remove_group(&bq->dev->kobj, &bq2589x_attr_group);

	return 0;
}

static void bq2589x_charger_shutdown(struct i2c_client *client)
{
	struct bq2589x *bq = i2c_get_clientdata(client);
	bq2589x_disable_otg(bq);
	pr_err("bq2589x_disable_otg for shutdown\n");
	bq2589x_disable_maxcen(bq);
	pr_err("bq2589x_disable_maxcen for shutdown\n");
}

static struct i2c_driver bq2589x_charger_driver = {
	.driver = {
		   .name = "bq2589x-charger",
		   .owner = THIS_MODULE,
		   .of_match_table = bq2589x_charger_match_table,
		   },

	.probe = bq2589x_charger_probe,
	.remove = bq2589x_charger_remove,
	.shutdown = bq2589x_charger_shutdown,

};

module_i2c_driver(bq2589x_charger_driver);

MODULE_DESCRIPTION("TI BQ2589X Charger Driver");
MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Texas Instruments");
