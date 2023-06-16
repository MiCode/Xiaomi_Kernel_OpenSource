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
#include "charger_class.h"
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>
#include <linux/extcon-provider.h>
#include <linux/time.h>

#include "bq2589x_charger.h"
#include "tcpci_typec.h"
#include "mtk_charger.h"
#include <linux/phy/phy.h>
#include "mtk_battery.h"
#include <linux/pm_wakeup.h>

static enum power_supply_property xm_charger_properties[] = {
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_USB_TYPE,
	POWER_SUPPLY_PROP_TYPE,
	POWER_SUPPLY_PROP_MANUFACTURER,
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT,
	POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE,
	POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT,
	POWER_SUPPLY_PROP_INPUT_VOLTAGE_LIMIT,
	POWER_SUPPLY_PROP_CHARGE_TERM_CURRENT,
	POWER_SUPPLY_PROP_CURRENT_MAX,
	POWER_SUPPLY_PROP_VOLTAGE_MAX,
};
static char *xm_charger_supplied_to[] = {
	"usb",
	"mtk-master-charger",
	"battery",
};

static enum power_supply_property xm_usb_properties[] = {
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_CURRENT_NOW,
};
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
static int bq2589x_read_mask(struct bq2589x *bq, u8 reg,
		u8 mask, u8 shift, u8 *data)
{
	u8 v;
	int ret;

	ret = bq2589x_read_byte(bq, reg, &v);
	if (ret < 0)
		return ret;

	v &= mask;
	v >>= shift;
	*data = v;

	return 0;
}
static int bq2589x_write_mask(struct bq2589x *bq, u8 reg,
		u8 mask, u8 shift, u8 data)
{
	u8 v;
	int ret;

	ret = bq2589x_read_byte(bq, reg, &v);
	if (ret < 0)
		return ret;

	v &= ~mask;
	v |= ((data << shift) & mask);

	return bq2589x_write_byte(bq, reg, v);
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
int bq2589x_force_dpdm(struct bq2589x *bq)
{
	int ret;
	u8 val = BQ2589X_FORCE_DPDM << BQ2589X_FORCE_DPDM_SHIFT;
	ret = bq2589x_update_bits(bq, BQ2589X_REG_02, 
						BQ2589X_FORCE_DPDM_MASK, val);
	
	return ret;
}
EXPORT_SYMBOL_GPL(bq2589x_force_dpdm);
static int bq2589x_enable_charger(struct bq2589x *bq)
{
	int ret;
	u8 val = BQ2589X_CHG_ENABLE << BQ2589X_CHG_CONFIG_SHIFT;
	ret = bq2589x_update_bits(bq, BQ2589X_REG_03,
				BQ2589X_CHG_CONFIG_MASK, val);
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
int bq2589x_set_chargecurrent(struct bq2589x *bq, int curr)
{
	u8 ichg;
	if (curr < BQ2589X_ICHG_BASE)
		curr = BQ2589X_ICHG_BASE;
	ichg = (curr - BQ2589X_ICHG_BASE)/BQ2589X_ICHG_LSB;
	return bq2589x_update_bits(bq, BQ2589X_REG_04, 
						BQ2589X_ICHG_MASK, ichg << BQ2589X_ICHG_SHIFT);
}
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
int bq2589x_set_term_current(struct bq2589x *bq, int curr)
{
	u8 iterm;
	if (curr < BQ2589X_ITERM_BASE)
		curr = BQ2589X_ITERM_BASE;
	iterm = (curr - BQ2589X_ITERM_BASE) / BQ2589X_ITERM_LSB;
	return bq2589x_update_bits(bq, BQ2589X_REG_05, 
						BQ2589X_ITERM_MASK, iterm << BQ2589X_ITERM_SHIFT);
}
EXPORT_SYMBOL_GPL(bq2589x_set_term_current);
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
int bq2589x_adc_read_vbus_volt(struct bq2589x *bq, u32 *vol)
{
	uint8_t val;
	int volt;
	int ret;
	ret = bq2589x_read_byte(bq, BQ2589X_REG_11, &val);
	if (ret < 0) {
		dev_err(bq->dev, "read vbus voltage failed :%d\n", ret);
	} else{
		volt = BQ2589X_VBUSV_BASE + ((val & BQ2589X_VBUSV_MASK) >> BQ2589X_VBUSV_SHIFT) * BQ2589X_VBUSV_LSB ;
		*vol = volt * 1000;
	}
    return ret;
}
static int bq2589x_get_charge_stat(struct bq2589x *bq, int *state)
{
	int ret;
	u8 val;
	ret = bq2589x_read_byte(bq, BQ2589X_REG_0B, &val);
	if (!ret) {
        if ((val & BQ2589X_VBUS_STAT_MASK) >> BQ2589X_VBUS_STAT_SHIFT 
                == BQ2589X_VBUS_TYPE_OTG) {
            *state = POWER_SUPPLY_STATUS_DISCHARGING;
            return ret;
        }
		val = val & BQ2589X_CHRG_STAT_MASK;
		val = val >> BQ2589X_CHRG_STAT_SHIFT;
	switch (val)
        {
        case BQ2589X_CHRG_STAT_IDLE:
            *state = POWER_SUPPLY_STATUS_NOT_CHARGING;
            break;
        case BQ2589X_CHRG_STAT_PRECHG:
        case BQ2589X_CHRG_STAT_FASTCHG:
            *state = POWER_SUPPLY_STATUS_CHARGING;
            break;
        case BQ2589X_CHRG_STAT_CHGDONE:
            *state = POWER_SUPPLY_STATUS_FULL;
            break;
        default:
            *state = POWER_SUPPLY_STATUS_UNKNOWN;
            break;
        }
	}

	if(bq->power_good == 1 && *state == POWER_SUPPLY_STATUS_NOT_CHARGING)
		*state = POWER_SUPPLY_STATUS_CHARGING;

	if(bq->is_soft_full && *state == POWER_SUPPLY_STATUS_CHARGING)
		*state = POWER_SUPPLY_STATUS_FULL;

	return ret;
}

int bq2589x_adc_enable(struct bq2589x *bq)
{
	u8 val;
	int ret;
	ret = bq2589x_read_byte(bq, BQ2589X_REG_02, &val);
	if (ret < 0) {
		dev_err(bq->dev, "%s failed to read register 0x02:%d\n", __func__, ret);
		return ret;
	}

	ret = bq2589x_update_bits(bq, BQ2589X_REG_02, BQ2589X_CONV_START_MASK,
				BQ2589X_ADC_CONTINUE_ENABLE << BQ2589X_CONV_START_SHIFT);

	return ret;
}

int bq2589x_adc_read_charge_current(struct bq2589x *bq, u32 *cur)
{
	uint8_t val;
	int curr;
	int ret;
	bq2589x_adc_enable(bq);
	ret = bq2589x_read_byte(bq, BQ2589X_REG_12, &val);

	curr = (int)(BQ2589X_ICHGR_BASE + ((val & BQ2589X_ICHGR_MASK) >> BQ2589X_ICHGR_SHIFT) * BQ2589X_ICHGR_LSB) ;
	*cur = curr * 1000; 
    return ret;
}
int bq2589x_get_charge_vol(struct bq2589x *bq, int *volt)
{
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
int bq2589x_get_input_current_limit(struct  bq2589x *sc, u32 *curr)
{
    u8 reg_val;
	int icl;
	int ret;
	ret = bq2589x_read_byte(sc, BQ2589X_REG_00, &reg_val);
	if (!ret) {
		icl = (reg_val & BQ2589X_IINLIM_MASK) >> BQ2589X_IINLIM_SHIFT;
		icl = icl * BQ2589X_IINLIM_LSB + BQ2589X_IINLIM_BASE;
		*curr = icl * 1000;
	}
    return ret;
}
int bq2589x_get_input_volt_limit(struct bq2589x *bq, u32 *volt)
{
    u8 reg_val;
	int vchg;
	int ret;
	ret = bq2589x_read_byte(bq, BQ2589X_REG_0D, &reg_val);
	if (!ret) {
		vchg = (reg_val & BQ2589X_VINDPM_MASK) >> BQ2589X_VINDPM_SHIFT;
		vchg = vchg * BQ2589X_VINDPM_LSB + BQ2589X_VINDPM_BASE;
		*volt = vchg * 1000;
	}
    return ret;
}
int bq2589x_get_term_current(struct bq2589x *bq, int *curr)
{
    u8 reg_val;
	int iterm;
	int ret;
	ret = bq2589x_read_byte(bq, BQ2589X_REG_05, &reg_val);
	if (!ret) {
		iterm = (reg_val & BQ2589X_ITERM_MASK) >> BQ2589X_ITERM_SHIFT;
         iterm = iterm * BQ2589X_ITERM_LSB + BQ2589X_ITERM_BASE;
		*curr = iterm * 1000;
	}
    return ret;
}

static int xm_charger_get_property(struct power_supply *psy,
	enum power_supply_property psp, union power_supply_propval *val)
{
	int ret = 0, data;
	struct bq2589x *bq = power_supply_get_drvdata(psy);
	if (!bq) {
		return -EINVAL;
	}
	switch (psp) {
	case POWER_SUPPLY_PROP_MANUFACTURER:
        if (bq->part_no == pn_data[PN_BQ25890])
		    val->strval = "Ti";
        else
            val->strval = "Silergy";
		break;
	case POWER_SUPPLY_PROP_ONLINE:
		if(bq->online != bq->power_good)
		{
			bq->online = bq->power_good;
		}
		val->intval = bq->power_good;
		break;
	case POWER_SUPPLY_PROP_STATUS:
		switch (bq->charge_status)
		{
			case POWER_SUPPLY_STATUS_NOT_CHARGING:
				val->intval = POWER_SUPPLY_STATUS_NOT_CHARGING;
				break;
			case POWER_SUPPLY_STATUS_DISCHARGING:
				val->intval = POWER_SUPPLY_STATUS_DISCHARGING;
				break;
			case POWER_SUPPLY_STATUS_CHARGING:
				val->intval = POWER_SUPPLY_STATUS_CHARGING;
				break;
			case POWER_SUPPLY_STATUS_FULL:
				val->intval = POWER_SUPPLY_STATUS_FULL;
				break;
			default:
				val ->intval = POWER_SUPPLY_STATUS_UNKNOWN;
				break;
		}
		break;
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT:
        	ret = bq2589x_adc_read_charge_current(bq, &val->intval);
		if (ret < 0)
            		break;
		break;
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE:
		ret = bq2589x_get_charge_vol(bq, &data);
        	if (ret < 0)
            		break;
        	val->intval = data;
		break;
	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
		ret = bq2589x_get_input_current_limit(bq, &data);
        	if (ret < 0)
            		break;
        	val->intval = data;
		break;
	case POWER_SUPPLY_PROP_INPUT_VOLTAGE_LIMIT:
		ret = bq2589x_get_input_volt_limit(bq, &data);
        if (ret < 0)
            break;
        val->intval = data;
		break;
	case POWER_SUPPLY_PROP_CHARGE_TERM_CURRENT:
		ret = bq2589x_get_term_current(bq, &data);
        if (ret < 0)
            break;
        val->intval = data;
		break;
	case POWER_SUPPLY_PROP_USB_TYPE:
		val->intval = bq->usb_type;
		break;
	case POWER_SUPPLY_PROP_CURRENT_MAX:
		if (bq->chg_desc.type == POWER_SUPPLY_TYPE_USB)
			val->intval = 500000;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_MAX:
		if (bq->chg_desc.type == POWER_SUPPLY_TYPE_USB)
			val->intval = 5000000;
		break;
	case POWER_SUPPLY_PROP_TYPE:
		val->intval = bq->chg_desc.type;
		break;
	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}
static int xm_charger_set_property(struct power_supply *psy,
	enum power_supply_property psp, const union power_supply_propval *val)
{
	int ret = 0;
	struct bq2589x *bq = power_supply_get_drvdata(psy);
	if (!bq->psy) {
		bq->psy = power_supply_get_by_name("xm_charger");
	}
	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		ret = val->intval;
		if(bq->psy)
			power_supply_changed(bq->chg_psy);
		break;
	case POWER_SUPPLY_PROP_STATUS:
		ret = val->intval;
		if(bq->psy)
			power_supply_changed(bq->chg_psy);
		break;
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT:
		ret = bq2589x_set_chargecurrent(bq, val->intval / 1000);
		break;
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE:
		ret = bq2589x_set_chargevolt(bq, val->intval / 1000);
		break;
	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
		ret = bq2589x_set_input_current_limit(bq, val->intval / 1000);
		break;
	case POWER_SUPPLY_PROP_INPUT_VOLTAGE_LIMIT:
		ret = bq2589x_set_input_volt_limit(bq, val->intval / 1000);
		break;
	case POWER_SUPPLY_PROP_CHARGE_TERM_CURRENT:
		ret = bq2589x_set_term_current(bq, val->intval / 1000);
		break;
	default:
		ret = -EINVAL;
		break;
	}
	
	return ret;
}
static int xm_charger_property_is_writeable(struct power_supply *psy,
					 enum power_supply_property prop)
{
	switch (prop) {
	case POWER_SUPPLY_PROP_ONLINE:
	case POWER_SUPPLY_PROP_CHARGE_TYPE:
	case POWER_SUPPLY_PROP_TYPE:
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT:
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE:
	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
	case POWER_SUPPLY_PROP_INPUT_VOLTAGE_LIMIT:
	case POWER_SUPPLY_PROP_CHARGE_TERM_CURRENT:
	case POWER_SUPPLY_PROP_STATUS:
	case POWER_SUPPLY_PROP_ENERGY_EMPTY:
		return true;
	default:
		return false;
	}
}
static int xm_usb_get_property(struct power_supply *psy,
	enum power_supply_property psp, union power_supply_propval *val)
{
	int ret = 0, vbus_volt, ibus_curr;
	struct bq2589x *bq = power_supply_get_drvdata(psy);
	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		val->intval = bq->power_good;
		break;
	case POWER_SUPPLY_PROP_PRESENT:
		val->intval = bq->power_good;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		ret = bq2589x_adc_read_vbus_volt(bq, &vbus_volt);
		val->intval = vbus_volt;
		break;
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		ret = bq2589x_adc_read_charge_current(bq, &ibus_curr);
		val->intval = ibus_curr;
		break;
	default:
		return -EINVAL;
	}
	return 0;
}
static int xm_usb_property_is_writeable(struct power_supply *psy,
					 enum power_supply_property prop)
{
	switch (prop) {
	case POWER_SUPPLY_PROP_ONLINE:
	case POWER_SUPPLY_PROP_PRESENT:
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		return true;
	default:
		return false;
	}
}
static const struct charger_properties bq2589x_chg_props = {
	.alias_name = "bq2589x",
};
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
int bq2589x_enable_hvdcp(struct bq2589x *bq)
{
	int ret;
	u8 val = BQ2589X_HVDCP_ENABLE << BQ2589X_HVDCPEN_SHIFT;
	ret = bq2589x_update_bits(bq, BQ2589X_REG_02, 
				BQ2589X_HVDCPEN_MASK, val);
	return ret;
}
EXPORT_SYMBOL_GPL(bq2589x_enable_hvdcp);
int bq2589x_disable_hvdcp(struct bq2589x *bq)
{
	int ret;
	u8 val = BQ2589X_HVDCP_DISABLE << BQ2589X_HVDCPEN_SHIFT;
	ret = bq2589x_update_bits(bq, BQ2589X_REG_02,
				BQ2589X_HVDCPEN_MASK, val);
	return ret;
}
EXPORT_SYMBOL_GPL(bq2589x_disable_hvdcp);
int bq2589x_adc_start(struct bq2589x *bq, bool oneshot)
{
	u8 val;
	int ret;
	ret = bq2589x_read_byte(bq, BQ2589X_REG_02, &val);
	if (ret < 0) {
		dev_err(bq->dev, "%s failed to read register 0x02:%d\n", __func__, ret);
		return ret;
	}
	//if (((val & BQ2589X_CONV_RATE_MASK) >> BQ2589X_CONV_RATE_SHIFT) == BQ2589X_ADC_CONTINUE_ENABLE)
	//	return 0; /*is doing continuous scan*/
	if (oneshot)
	{
		ret = bq2589x_update_bits(bq, BQ2589X_REG_02, BQ2589X_CONV_RATE_MASK,
					BQ2589X_ADC_CONTINUE_DISABLE << BQ2589X_CONV_RATE_SHIFT);
	}
	else
	{
		ret = bq2589x_update_bits(bq, BQ2589X_REG_02, BQ2589X_CONV_RATE_MASK,  
					BQ2589X_ADC_CONTINUE_ENABLE << BQ2589X_CONV_RATE_SHIFT);
	}
	return ret;
}
EXPORT_SYMBOL_GPL(bq2589x_adc_start);
static int bq2589x_disable_12V(struct bq2589x *bq)
{
	u8 val;
	int ret;
	val = BQ2589X_ENABLE_9V;
	val <<= BQ2589X_EN9V_SHIFT;
	ret = bq2589x_update_bits(bq, BQ2589X_REG_02,
				BQ2589X_EN9V_MASK, val);
	return ret;
}
static int bq2589x_disable_aicl(struct bq2589x *bq)
{
	u8 val;
	int ret;
	val = BQ2589X_DISABLE_AICL;
	val <<= BQ2589X_AICL_SHIFT;
	ret = bq2589x_update_bits(bq, BQ2589X_REG_02,
				BQ2589X_AICL_MASK, val);
	return ret;
}
static int bq2589x_set_recharge(struct bq2589x *bq)
{
	u8 val;
	int ret;
	if(bq->temp_now <= 100)
		val = BQ2589X_VRECHG_200MV;
	else
		val = BQ2589X_VRECHG_100MV;
	val <<= BQ2589X_VRECHG_SHIFT;
	ret = bq2589x_update_bits(bq, BQ2589X_REG_06,
				BQ2589X_VRECHG_MASK, val);
	return ret;
}
int bq2589x_set_ir_compensation(struct bq2589x *bq, int bat_comp, int vclamp)
{
	u8 val_bat_comp;
	u8 val_vclamp;
	val_bat_comp = bat_comp / BQ2589X_BAT_COMP_LSB;
	val_vclamp = vclamp / BQ2589X_VCLAMP_LSB;
	bq2589x_update_bits(bq, BQ2589X_REG_08, BQ2589X_BAT_COMP_MASK,
						val_bat_comp << BQ2589X_BAT_COMP_SHIFT);
	bq2589x_update_bits(bq, BQ2589X_REG_08, BQ2589X_VCLAMP_MASK,
						val_vclamp << BQ2589X_VCLAMP_SHIFT);
	return 0;
}
static int bq2589x_enable_abs_vindpm(struct bq2589x *bq)
{
	u8 val;
	int ret;
	val = BQ2589X_FORCE_VINDPM_ENABLE;
	val <<= BQ2589X_FORCE_VINDPM_SHIFT;
	ret = bq2589x_update_bits(bq, BQ2589X_REG_0D,
				BQ2589X_FORCE_VINDPM_MASK, val);
	return ret;
}
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
int bq2589x_enable_hiz_mode(struct bq2589x *bq, bool en)
{
	u8 val;

    if (en) {
        val = BQ2589X_HIZ_ENABLE << BQ2589X_ENHIZ_SHIFT;
    } else {
        val = BQ2589X_HIZ_DISABLE << BQ2589X_ENHIZ_SHIFT;
    }

	return bq2589x_update_bits(bq, BQ2589X_REG_00, 
						BQ2589X_ENHIZ_MASK, val);
}
int bq2589x_enable_term(struct bq2589x *bq, bool enable)
{
	u8 val;
	int ret;
	if (enable)
		val = BQ2589X_TERM_ENABLE << BQ2589X_EN_TERM_SHIFT;
	else
		val = BQ2589X_TERM_DISABLE << BQ2589X_EN_TERM_SHIFT;
	ret = bq2589x_update_bits(bq, BQ2589X_REG_07, 
						BQ2589X_EN_TERM_MASK, val);
	return ret;
}
EXPORT_SYMBOL_GPL(bq2589x_enable_term);
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
int bq2589x_enable_auto_dpdm(struct bq2589x* bq, bool enable)
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
int bq2589x_enable_ico(struct bq2589x* bq, bool enable)
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
int bq2589x_read_idpm_limit(struct bq2589x *bq)
{
	uint8_t val;
	int curr;
	int ret;
	ret = bq2589x_read_byte(bq, BQ2589X_REG_13, &val);
	if (ret < 0) {
		return ret;
	} else{
		curr = BQ2589X_IDPM_LIM_BASE + ((val & BQ2589X_IDPM_LIM_MASK) >> BQ2589X_IDPM_LIM_SHIFT) * BQ2589X_IDPM_LIM_LSB ;
		return curr;
	}
}
EXPORT_SYMBOL_GPL(bq2589x_read_idpm_limit);
int bq2589x_enable_safety_timer(struct bq2589x *bq)
{
	const u8 val = BQ2589X_CHG_TIMER_ENABLE << BQ2589X_EN_TIMER_SHIFT;
	return bq2589x_update_bits(bq, BQ2589X_REG_07, BQ2589X_EN_TIMER_MASK,
				   val);
}
EXPORT_SYMBOL_GPL(bq2589x_enable_safety_timer);
int bq2589x_disable_safety_timer(struct bq2589x *bq)
{
	const u8 val = BQ2589X_CHG_TIMER_DISABLE << BQ2589X_EN_TIMER_SHIFT;
	return bq2589x_update_bits(bq, BQ2589X_REG_07, BQ2589X_EN_TIMER_MASK,
				   val);
}
EXPORT_SYMBOL_GPL(bq2589x_disable_safety_timer);

static int bq2589x_set_charge_mode(struct regulator_dev *dev, u8 val)
{
	struct bq2589x *bq = rdev_get_drvdata(dev);
	int ret;

	if(val == BQ2589X_REG_POC_CHG_CONFIG_OTG)
		bq->otg_enable = true;
	else if(val == BQ2589X_REG_POC_CHG_CONFIG_CHARGE)
		bq->otg_enable = false;

	ret = bq2589x_write_mask(bq, BQ2589X_REG_03,
				 BQ2589X_REG_POC_CHG_CONFIG_MASK,
				 BQ2589X_REG_POC_CHG_CONFIG_SHIFT, val);

	return ret;
}

static int bq2589x_vbus_enable(struct regulator_dev *dev)
{
	return bq2589x_set_charge_mode(dev, BQ2589X_REG_POC_CHG_CONFIG_OTG);
}

static int bq2589x_vbus_disable(struct regulator_dev *dev)
{
	return bq2589x_set_charge_mode(dev, BQ2589X_REG_POC_CHG_CONFIG_CHARGE);
}

static int bq2589x_vbus_is_enabled(struct regulator_dev *dev)
{
	struct bq2589x *bq = rdev_get_drvdata(dev);
	int ret;
	u8 val;

	ret = bq2589x_read_mask(bq, BQ2589X_REG_03,
				BQ2589X_REG_POC_CHG_CONFIG_MASK,
				BQ2589X_REG_POC_CHG_CONFIG_SHIFT, &val);

	return ret ? ret : val == BQ2589X_REG_POC_CHG_CONFIG_OTG;
}

int bq2589x_list_voltage(struct regulator_dev *rdev,
				  unsigned int selector)
{
	if (selector >= rdev->desc->n_voltages)
		return -EINVAL;
	if (selector < rdev->desc->linear_min_sel)
		return 0;

	selector -= rdev->desc->linear_min_sel;

	return rdev->desc->min_uV + (rdev->desc->uV_step * selector);
}

static int bq2589x_set_voltage_sel(struct regulator_dev *dev, unsigned sel)
{
	struct bq2589x *bq = rdev_get_drvdata(dev);
	int ret;
	ret = bq2589x_write_byte(bq, BQ2589X_REG_0A, BQ2589X_BOOST_VOL_CURR);
	return ret;
}

static int bq2589x_set_current_limit(struct regulator_dev *dev,
				       int min_uA, int max_uA)
{
	struct bq2589x *bq = rdev_get_drvdata(dev);
	int ret;
	ret = bq2589x_write_byte(bq, BQ2589X_REG_0A, BQ2589X_BOOST_VOL_CURR);
	return ret;
}

static const struct regulator_ops bq2589x_vbus_ops = {
	.enable = bq2589x_vbus_enable,
	.disable = bq2589x_vbus_disable,
	.is_enabled = bq2589x_vbus_is_enabled,
	.list_voltage = bq2589x_list_voltage,
	.set_voltage_sel = bq2589x_set_voltage_sel,
	.set_current_limit = bq2589x_set_current_limit,
};

static const struct regulator_desc bq2589x_vbus_desc = {
	.of_match = "bq2589x,otg-vbus",
	.type = REGULATOR_VOLTAGE,
	.owner = THIS_MODULE,
	.ops = &bq2589x_vbus_ops,
	.min_uV = 4850000,
	.uV_step = 25000,
	.n_voltages = 47,
	.linear_min_sel = 20,

};

static int bq2589x_register_vbus_regulator(struct bq2589x *bq)
{
	struct regulator_config cfg = {
		.dev = bq->dev,
		.driver_data = bq,
	};
	memcpy(&bq->rdesc, &bq2589x_vbus_desc, sizeof(bq->rdesc));
	bq->rdesc.name = dev_name(bq->dev);
	bq->rdev = devm_regulator_register(bq->dev, &bq->rdesc, &cfg);
	return IS_ERR(bq->rdev) ? PTR_ERR(bq->rdev) : 0;

}

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
	}
	if (of_property_read_string(np, "eint_name", &bq->eint_name) < 0) {
		bq->eint_name = "chr_stat";
	}
	bq->chg_det_enable =
	    of_property_read_bool(np, "ti,bq2589x,charge-detect-enable");

	bq->switch_sel_en_gpio = of_get_named_gpio_flags(np, "switch_sel_en_gpio", 0 , NULL);
	gpio_direction_output(bq->switch_sel_en_gpio, 0);

	ret = of_property_read_u32(np, "ti,bq2589x,usb-vlim", &pdata->usb.vlim);
	if (ret) {
		pdata->usb.vlim = 4500;
	}
	ret = of_property_read_u32(np, "ti,bq2589x,usb-ilim", &pdata->usb.ilim);
	if (ret) {
		pdata->usb.ilim = 2000;
	}
	ret = of_property_read_u32(np, "ti,bq2589x,usb-vreg", &pdata->usb.vreg);
	if (ret) {
		pdata->usb.vreg = 4200;
	}
	ret = of_property_read_u32(np, "ti,bq2589x,usb-ichg", &pdata->usb.ichg);
	if (ret) {
		pdata->usb.ichg = 2000;
	}
	ret = of_property_read_u32(np, "ti,bq2589x,precharge-current",
				   &pdata->iprechg);
	if (ret) {
		pdata->iprechg = 180;
	}
	ret = of_property_read_u32(np, "ti,bq2589x,termination-current",
				   &pdata->iterm);
	if (ret) {
		pdata->iterm = 400;
	}
	ret =
	    of_property_read_u32(np, "ti,bq2589x,boost-voltage",
				 &pdata->boostv);
	if (ret) {
		pdata->boostv = 5000;
	}
	ret =
	    of_property_read_u32(np, "ti,bq2589x,boost-current",
				 &pdata->boosti);
	if (ret) {
		pdata->boosti = 1200;
	}
	return pdata;
}

static int bq2589x_set_usbsw(struct bq2589x *ddata,
				enum bq2589x_usbsw usbsw)
{
	struct phy *phy;
	int ret, mode = (usbsw == USBSW_CHG) ? PHY_MODE_BC11_SET : PHY_MODE_BC11_CLR;

	phy = phy_get(ddata->dev, "usb2-phy");
	if (IS_ERR_OR_NULL(phy)) {
		return -ENODEV;
	}
	ret = phy_set_mode_ext(phy, PHY_MODE_USB_DEVICE, mode);
	if (ret)
		dev_err(ddata->dev, "failed to set phy ext mode\n");
	phy_put(ddata->dev, phy);
	return ret;
}

static bool is_usb_rdy(struct bq2589x *bq)
{
	bool ready = true;
	struct device_node *node;
	node = of_parse_phandle(bq->dev->of_node, "usb", 0);
	if (node) {
		ready = !of_property_read_bool(node, "cdp-block");
	} else
		pr_err("usb node missing or invalid\n");
	return ready;
}

static void bq2589x_dcp_detect_work(struct work_struct *work)
{
	struct bq2589x *bq = container_of(work,
			struct bq2589x, dcp_detect_work.work);
	gpio_set_value(bq->switch_sel_en_gpio , 1);
	bq2589x_force_dpdm(bq);
	bq->dcp_detect_count ++;
}

static void bq2589x_float_detect_work(struct work_struct *work)
{
	struct bq2589x *bq = container_of(work,
			struct bq2589x, float_detect_work.work);
	gpio_set_value(bq->switch_sel_en_gpio , 1);
	bq2589x_force_dpdm(bq);
	bq->float_detect_count ++;
}

extern bool qc_11w_detect;
static int bq2589x_get_charger_type(struct bq2589x *bq)
{
	int ret;
	u8 reg_val = 0;
	int vbus_stat = 0;
	bool rpt_psy = true;
	int charge_status = 0;
	union power_supply_propval val = { .intval = 1 };
	bq->usb_type = POWER_SUPPLY_USB_TYPE_UNKNOWN;
	bq2589x_adc_enable(bq);
	ret = bq2589x_read_byte(bq, BQ2589X_REG_0B, &reg_val);
	if (ret)
		return ret;

	vbus_stat = (reg_val & BQ2589X_VBUS_STAT_MASK);
	vbus_stat >>= BQ2589X_VBUS_STAT_SHIFT;
	if(!bq->power_good && bq->part_no == pn_data[PN_BQ25890])
		vbus_stat = PORT_STAT_NOINFO;
	if(bq->cdp_detect && bq->part_no == pn_data[PN_BQ25890] && vbus_stat == PORT_STAT_DCP)
		vbus_stat = PORT_STAT_CDP;
	switch (vbus_stat) {
		case PORT_STAT_NOINFO:
			bq->chg_desc.type = POWER_SUPPLY_TYPE_UNKNOWN;
			bq->usb_desc.type = POWER_SUPPLY_TYPE_UNKNOWN;
			bq->usb_type = POWER_SUPPLY_USB_TYPE_UNKNOWN;
			bq->real_type = USB_REAL_TYPE_UNKNOWN;
			rpt_psy = false;
			break;
		case PORT_STAT_SDP:
			bq->chg_desc.type = POWER_SUPPLY_TYPE_USB;
			bq->usb_desc.type = POWER_SUPPLY_TYPE_USB;
			bq->usb_type = POWER_SUPPLY_USB_TYPE_SDP;
			bq->real_type = USB_REAL_TYPE_SDP;
			gpio_set_value(bq->switch_sel_en_gpio , 0);
			bq2589x_set_usbsw(bq, USBSW_USB);
			break;
		case PORT_STAT_CDP:
			bq->chg_desc.type = POWER_SUPPLY_TYPE_USB_CDP;
			bq->usb_desc.type = POWER_SUPPLY_TYPE_USB_CDP;
			bq->usb_type = POWER_SUPPLY_USB_TYPE_CDP;
			bq->real_type = USB_REAL_TYPE_CDP;
			bq->cdp_detect = true;
			bq2589x_enable_abs_vindpm(bq);
			bq2589x_set_input_volt_limit(bq, 4700);
			gpio_set_value(bq->switch_sel_en_gpio , 0);
			bq2589x_set_usbsw(bq, USBSW_USB);
			break;
		case PORT_STAT_DCP:
			if (bq->dcp_detect_count < 3) {
				schedule_delayed_work(&bq->dcp_detect_work, msecs_to_jiffies(2000));
			}
			bq->chg_desc.type = POWER_SUPPLY_TYPE_USB_DCP;
			bq->usb_desc.type = POWER_SUPPLY_TYPE_USB_DCP;
			bq->usb_type = POWER_SUPPLY_USB_TYPE_DCP;
			bq->real_type = USB_REAL_TYPE_DCP;
			break;
		case PORT_STAT_HVDCP:
			bq->chg_desc.type = POWER_SUPPLY_TYPE_USB_PD;
			bq->usb_desc.type = POWER_SUPPLY_TYPE_USB_PD;
			bq->usb_type = POWER_SUPPLY_USB_TYPE_PD;
			bq->real_type = USB_REAL_TYPE_HVDCP;
			if(bq->part_no == pn_data[PN_BQ25890])
			{
				ret = bq2589x_get_charge_stat(bq, &charge_status);
				power_supply_set_property(bq->chg_psy,
				POWER_SUPPLY_PROP_STATUS, &val);
				power_supply_set_property(bq->chg_psy,
				POWER_SUPPLY_PROP_ONLINE, &val);
			}
			if(bq->is_qc_11w)
				qc_11w_detect = true;
			break;
		case PORT_STAT_UNKOWNADT:
			if (bq->float_detect_count < 10) {
				schedule_delayed_work(&bq->float_detect_work, msecs_to_jiffies(2000));
			}
			bq->chg_desc.type = POWER_SUPPLY_TYPE_USB;
			bq->usb_desc.type = POWER_SUPPLY_TYPE_USB;
			bq->usb_type = POWER_SUPPLY_USB_TYPE_DCP;
			bq->real_type = USB_REAL_TYPE_FLOAT;
			break;
		case PORT_STAT_NOSTAND:
			if (bq->float_detect_count < 5) {
				schedule_delayed_work(&bq->float_detect_work, msecs_to_jiffies(2000));
			}
			bq->chg_desc.type = POWER_SUPPLY_TYPE_USB;
			bq->usb_desc.type = POWER_SUPPLY_TYPE_USB;
			bq->usb_type = POWER_SUPPLY_USB_TYPE_DCP;
			bq->real_type = USB_REAL_TYPE_FLOAT;
			break;
		default:
			bq->chg_desc.type = POWER_SUPPLY_TYPE_USB;
			bq->usb_desc.type = POWER_SUPPLY_TYPE_USB;
			bq->usb_type = POWER_SUPPLY_USB_TYPE_DCP;
			bq->real_type = USB_REAL_TYPE_FLOAT;
			break;
	}

	bq->pre_real_type = bq->real_type;

	return 0;
}
static irqreturn_t bq2589x_irq_handler(int irq, void *data)
{
	int ret, i;
	u8 reg_val, val;
	bool prev_pg;
	struct bq2589x *bq = data;
	struct timespec64 ts;
	static time64_t start_time = 0, end_time = 0;
	static int delta_time = 0;
	bool charge_done;
	int charge_status;
	ret = bq2589x_read_byte(bq, BQ2589X_REG_0B, &val);
	if(ret < 0)
	{
		msleep(500);
		ret = bq2589x_read_byte(bq, BQ2589X_REG_0B, &val);
	}
	if (!ret) {
		val = val & BQ2589X_CHRG_STAT_MASK;
		val = val >> BQ2589X_CHRG_STAT_SHIFT;
		charge_done = (val == BQ2589X_CHRG_STAT_CHGDONE);
	}
	bq2589x_adc_enable(bq);
	ret = bq2589x_read_byte(bq, BQ2589X_REG_0B, &reg_val);
	if (ret)
		return IRQ_HANDLED;
	prev_pg = bq->power_good;
	bq->power_good = !!(reg_val & BQ2589X_PG_STAT_MASK);
	if(charge_done){
		bq2589x_set_recharge(bq);
		__pm_relax(bq->irq_wake_lock);
	} else
		bq2589x_set_usbsw(bq, USBSW_CHG);
	if (!prev_pg && bq->power_good)
	{
		gpio_set_value(bq->switch_sel_en_gpio , 1);
		bq->is_online = false;
		bq->charge_status = 0;
		bq->float_detect_count = 0;
		bq->dcp_detect_count = 0;
		ktime_get_real_ts64(&ts);
		end_time = ts.tv_sec;
		delta_time = end_time - start_time;
		if(delta_time <= 1)
			bq->is_qc_11w = true;
		for (i = 0; i < 250; i++) {
				if (is_usb_rdy(bq))
					break;
				msleep(100);
		}
		ret = bq2589x_force_dpdm(bq);
		msleep(200);
	}else if (prev_pg && !bq->power_good){
		__pm_relax(bq->irq_wake_lock);
		gpio_set_value(bq->switch_sel_en_gpio , 0);
		qc_11w_detect = false;
		bq->cdp_detect = false;
		bq->is_qc_11w = false;
		ktime_get_real_ts64(&ts);
		start_time = ts.tv_sec;
	}

	ret = bq2589x_get_charger_type(bq);

	if (!bq->psy) {
		bq->psy = power_supply_get_by_name("xm_charger");
	}
	if (bq->psy) {
		ret = bq2589x_get_charge_stat(bq, &charge_status);
		if(bq->charge_status != charge_status)
		{
			power_supply_changed(bq->psy);
			bq->charge_status = charge_status;
		}
	}

	if (bq->psy) {
		power_supply_changed(bq->psy);
	}

	return IRQ_HANDLED;
}
static irqreturn_t irq_handler(int irq, void *data)
{
	struct bq2589x *bq = (struct bq2589x *)data;
	__pm_stay_awake(bq->irq_wake_lock);
	return IRQ_WAKE_THREAD;
}
static int bq2589x_register_interrupt(struct bq2589x *bq)
{
	int ret = 0;
	ret = devm_request_threaded_irq(bq->dev, bq->client->irq, irq_handler,
					bq2589x_irq_handler,
					IRQF_TRIGGER_FALLING | IRQF_NO_SUSPEND | IRQF_ONESHOT,
					"chr_stat", bq);
	enable_irq_wake(bq->irq);
	return 0;
}

static int bq2589x_init_device(struct bq2589x *bq)
{
	int ret;
	bq->input_suspend = false;
	bq->is_soft_full = false;
	bq2589x_dump_regs(bq);
	bq2589x_disable_watchdog_timer(bq);
	ret = bq2589x_enable_hvdcp(bq);
	ret = bq2589x_adc_start(bq, true);
	bq2589x_update_bits(bq, BQ2589X_REG_00, BQ2589X_ENILIM_MASK,
                BQ2589X_ENILIM_DISABLE << BQ2589X_ENILIM_SHIFT);
	ret = bq2589x_disable_12V(bq);
	ret = bq2589x_set_prechg_current(bq, bq->platform_data->iprechg);
	ret = bq2589x_set_term_current(bq, bq->platform_data->iterm);
	ret = bq2589x_set_boost_voltage(bq, bq->platform_data->boostv);
	ret = bq2589x_set_boost_current(bq, bq->platform_data->boosti);
	ret = bq2589x_disable_aicl(bq);
	ret = bq2589x_set_recharge(bq);
	ret = bq2589x_set_ir_compensation(bq, 40, 64);
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
	ret = bq2589x_read_byte(bq, BQ2589X_REG_03, &val);
	if (!ret)
		bq->charge_enabled = !!(val & BQ2589X_CHG_CONFIG_MASK);
	return ret;
}
static int bq2589x_plug_in(struct charger_device *chg_dev)
{
	int ret;
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
	return bq2589x_set_input_volt_limit(bq, volt / 1000);
}

static int bq2589x_set_icl(struct charger_device *chg_dev, u32 curr)
{
	struct bq2589x *bq = dev_get_drvdata(&chg_dev->dev);
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
	if (en)
		ret = bq2589x_enable_otg(bq);
	else
		ret = bq2589x_disable_otg(bq);
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
	ret = bq2589x_set_boost_current(bq, curr / 1000);
	return ret;
}
static int bq2589x_get_vbus_adc(struct charger_device *chg_dev, u32 *vbus)
{
	struct bq2589x *bq = dev_get_drvdata(&chg_dev->dev);
	u8 reg_val;
	int volt;
	int ret;
	bq2589x_adc_enable(bq);
	ret = bq2589x_read_byte(bq, BQ2589X_REG_11, &reg_val);
	if (!ret) {
		volt = (reg_val & BQ2589X_VBUSV_MASK) >> BQ2589X_VBUSV_SHIFT;
		volt = volt * BQ2589X_VBUSV_LSB + BQ2589X_VBUSV_BASE;
		*vbus = volt * 1000;
	}
	return *vbus;
}
static int bq2589x_get_ibus_adc(struct charger_device *chg_dev, u32 *ibus)
{
	int ret = 0;
	struct bq2589x *bq = dev_get_drvdata(&chg_dev->dev);
	ret = bq2589x_adc_read_charge_current(bq, ibus);
	return *ibus;
}
static int bq2589x_get_ibat_adc(struct charger_device *chg_dev, u32 *ibat)
{
	struct bq2589x *bq = dev_get_drvdata(&chg_dev->dev);
	u8 reg_val;
	int curr;
	int ret;
	ret = bq2589x_read_byte(bq, BQ2589X_REG_04, &reg_val);
	if (!ret) {
		curr = (reg_val & BQ2589X_ICHG_MASK) >> BQ2589X_ICHG_SHIFT;
		curr = curr * BQ2589X_ICHG_LSB + BQ2589X_ICHG_BASE;
		*ibat = curr * 1000;
	}
	return *ibat;
}

static int bq2589x_do_event(struct charger_device *chgdev, u32 event, u32 args)
{
	struct bq2589x *bq = charger_get_data(chgdev);
	if (!bq->psy) {
		bq->psy = power_supply_get_by_name("xm_charger");
	}
	switch (event) {
	case EVENT_FULL:
		bq->is_soft_full = true;
		bq->charge_status = POWER_SUPPLY_STATUS_FULL;
		if (bq->psy)
			power_supply_changed(bq->psy);
		break;
	case EVENT_RECHARGE:
		bq->is_soft_full = false;
		bq->charge_status = POWER_SUPPLY_STATUS_CHARGING;
		if (bq->psy)
			power_supply_changed(bq->psy);
		break;
	case EVENT_DISCHARGE:
		bq->is_soft_full = false;
		if(bq->psy)
			power_supply_changed(bq->psy);
		break;
	default:
		break;
	}
	return 0;
}

struct quick_charge_desc {
	enum xm_chg_type psy_type;
	enum quick_charge_type type;
};

struct quick_charge_desc quick_charge_table[6] = {
	{ XM_TYPE_SDP,		QUICK_CHARGE_NORMAL },
	{ XM_TYPE_CDP,		QUICK_CHARGE_NORMAL },
	{ XM_TYPE_DCP,		QUICK_CHARGE_NORMAL },
	{ XM_TYPE_FLOAT,	QUICK_CHARGE_NORMAL },
	{ XM_TYPE_HVDCP,	QUICK_CHARGE_FAST },
	{0, 0},
};

static int get_quick_charge_type(struct bq2589x *bq)
{
	int i = 0;
	union power_supply_propval pval = {0,};
	int ret = 0;

	if (!bq || !bq->usb_psy)
	return QUICK_CHARGE_NORMAL;

	if (IS_ERR_OR_NULL(bq->battery_psy)) {
		bq->battery_psy = power_supply_get_by_name("battery");
		chr_err("%s retry to get battery_psy\n", __func__);
	}

	if (IS_ERR_OR_NULL(bq->battery_psy)) {
		chr_err("%s Couldn't get battery_psy\n", __func__);
		ret = -1;
	} else {
		ret = power_supply_get_property(bq->battery_psy, POWER_SUPPLY_PROP_TEMP, &pval);
		if (ret)
			chr_err("failed to get temp\n");
		else
			bq->temp_now = pval.intval;
	}

	if (bq->temp_now > 480 || bq->temp_now < 0)
		return QUICK_CHARGE_NORMAL;

	while (quick_charge_table[i].psy_type != 0) {
		if (bq->real_type == quick_charge_table[i].psy_type) {
			return quick_charge_table[i].type;
		}
		i++;
	}

	return QUICK_CHARGE_NORMAL;
}

/*usb power supply tbl*/
/*cc_orientation*/
static int typec_cc_orientation_handler(struct tcpc_device *tcpc)
{
	uint8_t  typec_cc_orientation = 0;
	tcpci_get_cc(tcpc);
	if (typec_get_cc1() == 0 && typec_get_cc2() == 0)
		typec_cc_orientation = 0;
	else if (typec_get_cc2() == 0)
		typec_cc_orientation = 1;
	else if (typec_get_cc1() == 0)
		typec_cc_orientation = 2;
	return typec_cc_orientation;
}
/*input_suspend*/
int bq2589x_enable_hz(struct charger_device *chgdev, bool en)
{
    struct bq2589x *bq = dev_get_drvdata(&chgdev->dev);
	int ret;
	if (en){
		bq->input_suspend = true;
		bq->real_type = USB_REAL_TYPE_UNKNOWN;
	}else
		bq->input_suspend = false;
	ret = bq2589x_enable_hiz_mode(bq, en);

	if (!bq->input_suspend){
		gpio_set_value(bq->switch_sel_en_gpio , 1);
		bq2589x_force_dpdm(bq);
	}

    return ret;
}

static int typec_cc_orientation_get(struct bq2589x *bq,
	struct xm_usb_sysfs_field_info *attr,
	int *val)
{
	struct tcpc_device *tcpc;
	tcpc = tcpc_dev_get_by_name("type_c_port0");
	if (IS_ERR(tcpc)) {
		pr_err("%s: failed to get tcpc device\n", __func__);
		return PTR_ERR(tcpc);
	}
	if (bq)
		*val = typec_cc_orientation_handler(tcpc);
	else
		*val = 0;
	return 0;
}

static int typec_cc_orientation_set(struct bq2589x *bq,
	struct xm_usb_sysfs_field_info *attr,
	int val)
{
	if (bq)
		bq->cc_orientation = val;
	return 0;
}

static int input_suspend_get(struct bq2589x *bq,
	struct xm_usb_sysfs_field_info *attr,
	int *val)
{
	if (bq)
		*val = bq->input_suspend;
	else
		*val = 0;
	return 0;
}

extern bool is_input_suspend;
static int input_suspend_set(struct bq2589x *bq,
	struct xm_usb_sysfs_field_info *attr,
	int val)
{
	bool input_suspend = 0;

	input_suspend = !!val;
	if (bq) {
		bq->input_suspend = input_suspend;
		if(bq->input_suspend)
		{
			is_input_suspend = true;
			bq->real_type = USB_REAL_TYPE_UNKNOWN;
		} else {
			is_input_suspend = false;
			bq->real_type = bq->pre_real_type;
		}
	}
	return 0;
}

static int chip_state_get(struct bq2589x *bq,
	struct xm_usb_sysfs_field_info *attr,
	int *val)
{
	if (bq->part_no < 0)
		*val = 0;
	else
		*val = 1;
	return 0;
}

static int real_type_get(struct bq2589x *bq,
	struct xm_usb_sysfs_field_info *attr,
	int *val)
{
	if (bq)
		*val = bq->real_type;
	else
		*val = 0;
	return 0;
}

static int otg_enable_get(struct bq2589x *bq,
	struct xm_usb_sysfs_field_info *attr,
	int *val)
{
	if (bq)
		*val = bq->otg_enable;
	else
		*val = 9;
	return 0;
}

static int apdo_max_get(struct bq2589x *bq,
    struct xm_usb_sysfs_field_info *attr,
    int *val)
{
	int ret = 0;
	int vbus = 0, ibus = 0;
	ret = bq2589x_adc_read_vbus_volt(bq, &vbus);
	ret = bq2589x_get_input_current_limit(bq, &ibus);
	vbus /= 1000;
	ibus /= 1000;
	if (bq)
		*val = vbus * ibus / 1000000;
	else
		*val = -1;
	return 0;
}

static int quick_charge_type_get(struct bq2589x *bq,
    struct xm_usb_sysfs_field_info *attr,
    int *val)
{
	*val = get_quick_charge_type(bq);
	return 0;
}

#define MAX_UEVENT_LENGTH 50
static void generate_xm_charge_uevent(struct bq2589x *bq)
{
	static char uevent_string[][MAX_UEVENT_LENGTH+1] = {
		"POWER_SUPPLY_QUICK_CHARGE_TYPE=\n",//31+1
	};
	int val;
	u32 cnt=0, i=0;
	char *envp[2] = { NULL };  //the length of array need adjust when uevent number increase

	usb_get_property(USB_PROP_QUICK_CHARGE_TYPE, &val);
	sprintf(uevent_string[0]+31,"%d",val);
	envp[cnt++] = uevent_string[0];

	envp[cnt]=NULL;
	for(i = 0; i < cnt; ++i)
	      chr_err("%s\n", envp[i]);
	kobject_uevent_env(&bq->dev->kobj, KOBJ_CHANGE, envp);
	return;
}

static void xm_charger_external_power_usb_changed(struct power_supply *psy)
{
	struct bq2589x *bq;
	struct power_supply *chg_psy = NULL;
	bq = (struct bq2589x *)power_supply_get_drvdata(psy);
	chg_psy = bq->chg_psy;
	if (chg_psy)
		generate_xm_charge_uevent(bq);
}

static const char *get_usb_type_name(int usb_type)
{
	return "Unknown";
}

static int input_current_now_get(struct bq2589x *bq,
	struct xm_usb_sysfs_field_info *attr,
	int *val)
{
	int ibus = 0;
	if (bq)
	{
		bq2589x_adc_read_charge_current(bq, &ibus);
		*val = ibus;
	}
	else
		*val = 0;
	return 0;
}

extern bool mtbf_test;
static int mtbf_test_get(struct bq2589x *bq,
	struct xm_usb_sysfs_field_info *attr,
	int *val)
{
	if (bq)
		*val = mtbf_test;
	else
		*val = 0;
	return 0;
}
static int mtbf_test_set(struct bq2589x *bq,
	struct xm_usb_sysfs_field_info *attr,
	int val)
{
	if (bq)
		mtbf_test  = !!val;
	return 0;
}

static int typec_mode_get(struct bq2589x *bq,
	struct xm_usb_sysfs_field_info *attr,
	int *val)
{
	if (bq)
		*val = bq->typec_mode;
	else
		*val = 0;
	return 0;
}
static int typec_mode_set(struct bq2589x *bq,
	struct xm_usb_sysfs_field_info *attr,
	int val)
{
	if (bq)
		bq->typec_mode = val;
	return 0;
}

static const char *get_typec_mode_name(int typec_mode)
{
	return "Nothing attached";
}

static ssize_t usb_sysfs_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct power_supply *psy;
	struct bq2589x *bq;
	struct xm_usb_sysfs_field_info *usb_attr;
	int val;
	ssize_t ret;

	ret = kstrtos32(buf, 0, &val);
	if (ret < 0)
		return ret;

	psy = dev_get_drvdata(dev);
	bq = (struct bq2589x *)power_supply_get_drvdata(psy);

	usb_attr = container_of(attr,
		struct xm_usb_sysfs_field_info, attr);
	if (usb_attr->set != NULL)
		usb_attr->set(bq, usb_attr, val);

	return count;
}

static ssize_t usb_sysfs_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct power_supply *psy;
	struct bq2589x *bq;
	struct xm_usb_sysfs_field_info *usb_attr;
	int val = 0;
	ssize_t count;

	psy = dev_get_drvdata(dev);
	bq = (struct bq2589x *)power_supply_get_drvdata(psy);

	usb_attr = container_of(attr,
		struct xm_usb_sysfs_field_info, attr);
	if (usb_attr->get != NULL)
		usb_attr->get(bq, usb_attr, &val);

	if (usb_attr->prop == USB_PROP_REAL_TYPE) {
		count = scnprintf(buf, PAGE_SIZE, "%s\n", get_usb_type_name(val));
		return count;
	} else if (usb_attr->prop == USB_PROP_TYPEC_MODE) {
		count = scnprintf(buf, PAGE_SIZE, "%s\n", get_typec_mode_name(val));
		return count;
	}

	count = scnprintf(buf, PAGE_SIZE, "%d\n", val);
	return count;
}

/* Must be in the same order as USB_PROP_* */
static struct xm_usb_sysfs_field_info usb_sysfs_field_tbl[] = {
	USB_SYSFS_FIELD_RW(typec_cc_orientation, USB_PROP_TYPEC_CC_ORIENTATION),
	USB_SYSFS_FIELD_RW(input_suspend, USB_PROP_INPUT_SUSPEND),
	USB_SYSFS_FIELD_RO(chip_state, USB_PROP_CHIP_STATE),
	USB_SYSFS_FIELD_RO(real_type, USB_PROP_REAL_TYPE),
	USB_SYSFS_FIELD_RO(otg_enable, USB_PROP_OTG_ENABLE),
	USB_SYSFS_FIELD_RO(apdo_max, USB_PROP_APDO_MAX),
	USB_SYSFS_FIELD_RO(quick_charge_type, USB_PROP_QUICK_CHARGE_TYPE),
	USB_SYSFS_FIELD_RO(input_current_now, USB_PROP_INPUT_CURRENT_NOW),
	USB_SYSFS_FIELD_RW(mtbf_test, USB_PROP_MTBF_TEST),
	USB_SYSFS_FIELD_RW(typec_mode, USB_PROP_TYPEC_MODE),
};

int usb_get_property(enum usb_property bp,
			    int *val)
{
	struct bq2589x *bq;
	struct power_supply *psy;

	psy = power_supply_get_by_name("usb");
	if (psy == NULL)
		return -ENODEV;

	bq = (struct bq2589x *)power_supply_get_drvdata(psy);
	if (usb_sysfs_field_tbl[bp].prop == bp)
		usb_sysfs_field_tbl[bp].get(bq,
			&usb_sysfs_field_tbl[bp], val);
	else {
		return -ENOTSUPP;
	}

	return 0;
}
EXPORT_SYMBOL(usb_get_property);

int usb_set_property(enum usb_property bp,
			    int val)
{
	struct bq2589x *bq;
	struct power_supply *psy;

	psy = power_supply_get_by_name("usb");
	if (psy == NULL)
		return -ENODEV;

	bq = (struct bq2589x *)power_supply_get_drvdata(psy);

	if (usb_sysfs_field_tbl[bp].prop == bp)
		usb_sysfs_field_tbl[bp].set(bq,
			&usb_sysfs_field_tbl[bp], val);
	else {
		return -ENOTSUPP;
	}
	return 0;
}
EXPORT_SYMBOL(usb_set_property);

static struct attribute *
	usb_sysfs_attrs[ARRAY_SIZE(usb_sysfs_field_tbl) + 1];

static const struct attribute_group usb_sysfs_attr_group = {
	.attrs = usb_sysfs_attrs,
};

static void usb_sysfs_init_attrs(void)
{
	int i, limit = ARRAY_SIZE(usb_sysfs_field_tbl);

	for (i = 0; i < limit; i++)
		usb_sysfs_attrs[i] = &usb_sysfs_field_tbl[i].attr.attr;

	usb_sysfs_attrs[limit] = NULL; /* Has additional entry for this */
}

static int usb_sysfs_create_group(struct power_supply *psy)
{
	usb_sysfs_init_attrs();

	return sysfs_create_group(&psy->dev.kobj,
			&usb_sysfs_attr_group);
}
static struct charger_ops bq2589x_chg_ops = {
	/* Normal charging */
	.plug_in = bq2589x_plug_in,
	.plug_out = bq2589x_plug_out,
	.dump_registers = bq2589x_dump_register,
	.enable = bq2589x_charging,
	.is_enabled = bq2589x_is_charging_enable,
	.get_charging_current = bq2589x_get_ichg,
	.set_charging_current = bq2589x_set_ichg,
	.get_input_current = bq2589x_get_icl,
	.set_input_current = bq2589x_set_icl,
	.get_constant_voltage = bq2589x_get_vchg,
	.set_constant_voltage = bq2589x_set_vchg,
	.kick_wdt = bq2589x_kick_wdt,
	.set_mivr = bq2589x_set_ivl,
	.is_charging_done = bq2589x_is_charging_done,
	.get_min_charging_current = bq2589x_get_min_ichg,
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
	/*xm_charger*/
	.get_vbus_adc = bq2589x_get_vbus_adc,
	.get_ibus_adc = bq2589x_get_ibus_adc,
	.get_ibat_adc = bq2589x_get_ibat_adc,
	/* event */
	.event = bq2589x_do_event,
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
	bq->usb_type = POWER_SUPPLY_USB_TYPE_UNKNOWN;
	i2c_set_clientdata(client, bq);
	mutex_init(&bq->i2c_rw_lock);
	ret = bq2589x_detect_device(bq);
	if (ret) {
		return -ENODEV;
	}
	match = of_match_node(bq2589x_charger_match_table, node);
	if (match == NULL) {
		return -EINVAL;
	}
	bq->platform_data = bq2589x_parse_dt(node, bq);
	if (!bq->platform_data) {
		return -EINVAL;
	}
	ret = bq2589x_init_device(bq);
	if (ret) {
	return ret;
	}
	INIT_DELAYED_WORK(&bq->float_detect_work,bq2589x_float_detect_work);
	INIT_DELAYED_WORK(&bq->dcp_detect_work,bq2589x_dcp_detect_work);
	bq->irq_wake_lock = wakeup_source_register(bq->dev, "bq2589x_irq_wakelock");

	bq->chg_desc.name = "xm_charger";
	bq->chg_desc.usb_types = xm_chg_psy_usb_types;
	bq->chg_desc.num_usb_types = ARRAY_SIZE(xm_chg_psy_usb_types);
	bq->chg_desc.type = POWER_SUPPLY_TYPE_UNKNOWN;
	bq->chg_desc.properties = xm_charger_properties;
	bq->chg_desc.num_properties = ARRAY_SIZE(xm_charger_properties);
	bq->chg_desc.set_property = xm_charger_set_property;
	bq->chg_desc.get_property = xm_charger_get_property;
	bq->chg_desc.property_is_writeable	= xm_charger_property_is_writeable;
	bq->chg_cfg.drv_data = bq;
	bq->chg_cfg.of_node = bq->dev->of_node;
	bq->chg_cfg.supplied_to = xm_charger_supplied_to;
	bq->chg_cfg.num_supplicants = ARRAY_SIZE(xm_charger_supplied_to);
	bq->chg_psy = power_supply_register(bq->dev,
	&bq->chg_desc, &bq->chg_cfg);
	if (IS_ERR(bq->chg_psy)) {
		ret = PTR_ERR(bq->chg_psy);
		return ret;
	}

	bq->usb_desc.name = "usb";
	bq->usb_desc.type = POWER_SUPPLY_TYPE_UNKNOWN;
	bq->usb_desc.properties = xm_usb_properties;
	bq->usb_desc.num_properties = ARRAY_SIZE(xm_usb_properties);
	bq->usb_desc.get_property = xm_usb_get_property;
	bq->usb_desc.external_power_changed = xm_charger_external_power_usb_changed;
	bq->usb_desc.property_is_writeable = xm_usb_property_is_writeable;
	bq->usb_cfg.drv_data = bq;
	bq->usb_psy = power_supply_register(bq->dev,
		&bq->usb_desc, &bq->usb_cfg);
	if (IS_ERR(bq->usb_psy)) {
		ret = PTR_ERR(bq->usb_psy);
		return ret;
	} else{
		usb_sysfs_create_group(bq->usb_psy);
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
	determine_initial_status(bq);
	ret = bq2589x_register_vbus_regulator(bq);
	return 0;
}
static int bq2589x_charger_remove(struct i2c_client *client)
{
	struct bq2589x *bq = i2c_get_clientdata(client);
	sysfs_remove_group(&bq->dev->kobj, &bq2589x_attr_group);
	mutex_destroy(&bq->i2c_rw_lock);
	wakeup_source_unregister(bq->irq_wake_lock);
	return 0;
}
static void bq2589x_charger_shutdown(struct i2c_client *client)
{
	struct bq2589x *bq = i2c_get_clientdata(client);
	pr_err("enter %s\n", __func__);
	bq2589x_disable_otg(bq);
	bq2589x_disable_12V(bq);
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
