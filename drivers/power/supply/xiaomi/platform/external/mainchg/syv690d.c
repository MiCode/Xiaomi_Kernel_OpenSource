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

#include "inc/syv690d.h"
#include "inc/syv690d_reg.h"
#include "inc/syv690d_iio.h"

struct bq2589x *g_bq2589x;

static int bq2589x_read_byte(struct bq2589x *bq, u8 *data, u8 reg)
{
	int ret;
	int count = 3;

	mutex_lock(&bq->bq2589x_i2c_lock);
	while(1) {
		ret = i2c_smbus_read_byte_data(bq->client, reg);
		if (ret < 0 && count > 1) {
			dev_err(bq->dev, "failed to read 0x%.2x\n", reg);
			count--;
		} else {
			*data = (u8)ret;
			break;
		}
		udelay(200);
	}
	mutex_unlock(&bq->bq2589x_i2c_lock);

	return 0;
}

static int bq2589x_write_byte(struct bq2589x *bq, u8 reg, u8 data)
{
	int ret;

	mutex_lock(&bq->bq2589x_i2c_lock);
	ret = i2c_smbus_write_byte_data(bq->client, reg, data);
	mutex_unlock(&bq->bq2589x_i2c_lock);

	return ret;
}

static enum bq2589x_vbus_type bq2589x_get_vbus_type(struct bq2589x *bq)
{
	u8 val = 0;
	int ret;

	ret = bq2589x_read_byte(bq, &val, BQ2589X_REG_0B);
	if (ret < 0)
		return 0;
	val &= BQ2589X_VBUS_STAT_MASK;
	val >>= BQ2589X_VBUS_STAT_SHIFT;

	return val;
}

int bq2589x_update_bits(struct bq2589x *bq, u8 reg, u8 mask, u8 data)
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

int sc89890h_set_hv(struct bq2589x *bq, u8 hv)
{
    return bq2589x_write_byte(bq, BQ2589X_REG_01, hv);
}

int bq2589x_get_chg_type(struct bq2589x *bq)
{
	u8 val = 0;

	bq2589x_read_byte(bq, &val, BQ2589X_REG_0B);
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

int bq2589x_charge_status(struct bq2589x *bq)
{
	u8 val = 0;

	bq2589x_read_byte(bq, &val, BQ2589X_REG_0B);
	val &= BQ2589X_CHRG_STAT_MASK;
	val >>= BQ2589X_CHRG_STAT_SHIFT;
	switch (val) {
	case BQ2589X_CHRG_STAT_FASTCHG:
	case BQ2589X_CHRG_STAT_PRECHG:
		return POWER_SUPPLY_STATUS_CHARGING;
	case BQ2589X_CHRG_STAT_CHGDONE:
		return POWER_SUPPLY_STATUS_FULL;
	case BQ2589X_CHRG_STAT_IDLE:
		return POWER_SUPPLY_STATUS_DISCHARGING;
	default:
		return POWER_SUPPLY_STATUS_UNKNOWN;
	}
}

int bq2589x_get_chg_usb_type(struct bq2589x *bq)
{
	u8 val;
	int type, real_type;

	val = bq2589x_get_vbus_type(bq);
	type = (int)val;

	/*if (bq->part_no == SYV690 && bq->old_type == BQ2589X_VBUS_UNKNOWN) {
		type = BQ2589X_VBUS_NONSTAND;
	}*/

	pr_err("%s type=%d, old_type=%d \n", __func__, type, bq->old_type);

	if (type == BQ2589X_VBUS_USB_SDP) {
		real_type = POWER_SUPPLY_USB_TYPE_SDP;
		charger_request_dpdm(bq, false);
	}
	else if (type == BQ2589X_VBUS_USB_CDP) {
		real_type = POWER_SUPPLY_USB_TYPE_CDP;
		charger_request_dpdm(bq, false);
	}
	else if (type == BQ2589X_VBUS_USB_DCP) {
		real_type = POWER_SUPPLY_USB_TYPE_DCP;
	}
	else if (type == BQ2589X_VBUS_NONSTAND || type == BQ2589X_VBUS_UNKNOWN) {
		real_type = POWER_SUPPLY_TYPE_USB_FLOAT;
	}
	else if (type == BQ2589X_VBUS_MAXC) {
		real_type = POWER_SUPPLY_TYPE_USB_HVDCP;
		if (bq->part_no == SC89890H) {
			sc89890h_set_hv(bq, SC89890H_HV_9V);
		}
	}
	else {
		real_type = POWER_SUPPLY_USB_TYPE_UNKNOWN;
	}

	bq->charge_type = real_type;
	pr_err("%s bc1.2 type=%d\n", __func__, real_type);

	return real_type;
}

int bq2589x_enable_otg(struct bq2589x *bq)
{
	u8 val = BQ2589X_OTG_ENABLE << BQ2589X_OTG_CONFIG_SHIFT;

	return bq2589x_update_bits(bq, BQ2589X_REG_03,
							   BQ2589X_OTG_CONFIG_MASK, val);
}

int bq2589x_disable_otg(struct bq2589x *bq)
{
	u8 val = BQ2589X_OTG_DISABLE << BQ2589X_OTG_CONFIG_SHIFT;

	return bq2589x_update_bits(bq, BQ2589X_REG_03,
							   BQ2589X_OTG_CONFIG_MASK, val);
}

int bq2589x_set_otg_volt(struct bq2589x *bq, int volt)
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

int bq2589x_set_otg_current(struct bq2589x *bq, int curr)
{
	u8 temp;

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

	pr_err("bq2589x_set_otg_current cur = %d", curr);
	return bq2589x_update_bits(bq, BQ2589X_REG_0A, BQ2589X_BOOST_LIM_MASK, temp << BQ2589X_BOOST_LIM_SHIFT);
}

int bq2589x_enable_charger(struct bq2589x *bq)
{
	int ret;
	u8 val = BQ2589X_CHG_ENABLE << BQ2589X_CHG_CONFIG_SHIFT;
	dev_err(bq->dev, "%s enable charger\n", __func__);

	ret = bq2589x_update_bits(bq, BQ2589X_REG_03, BQ2589X_CHG_CONFIG_MASK, val);
	if (ret == 0)
		bq->status |= BQ2589X_STATUS_CHARGE_ENABLE;
	return ret;
}

int bq2589x_disable_charger(struct bq2589x *bq)
{
	int ret;
	u8 val = BQ2589X_CHG_DISABLE << BQ2589X_CHG_CONFIG_SHIFT;
	dev_err(bq->dev, "%s disable charger\n", __func__);

	ret = bq2589x_update_bits(bq, BQ2589X_REG_03, BQ2589X_CHG_CONFIG_MASK, val);
	if (ret == 0)
		bq->status &= ~BQ2589X_STATUS_CHARGE_ENABLE;
	return ret;
}

int bq2589x_get_charger_enable(struct bq2589x *bq)
{
	u8 val = 0;
	int ret;

	ret = bq2589x_read_byte(bq, &val, BQ2589X_REG_03);
	if (ret < 0) {
		dev_err(bq->dev, "%s Failed to read register 0x03:%d\n", __func__, ret);
		return ret;
	}
	val &= BQ2589X_CHG_CONFIG_MASK;
	val >>= BQ2589X_CHG_CONFIG_SHIFT;

	return val;
}

int bq2589x_get_reserved_val(struct bq2589x *bq)
{
	u8 val = 0;
	int ret;

	ret = bq2589x_read_byte(bq, &val, BQ2589X_REG_03);
	if (ret < 0) {
		dev_err(bq->dev, "%s Failed to read register 0x03:%d\n", __func__, ret);
		return ret;
	}
	val &= BQ2589X_RESERVED_MASK;
	val >>= BQ2589X_RESERVED_SHIFT;

	return val;
}

int bq2589x_reserved_disable(struct bq2589x *bq)
{
	u8 val = BQ2589X_RESERVED_DISABLE << BQ2589X_RESERVED_SHIFT;

	return bq2589x_update_bits(bq, BQ2589X_REG_03,
				BQ2589X_RESERVED_MASK, val);
}

/* interfaces that can be called by other module */
int bq2589x_adc_start(struct bq2589x *bq, bool oneshot)
{
	u8 val;
	int ret;

	ret = bq2589x_read_byte(bq, &val, BQ2589X_REG_02);
	if (ret < 0) {
		dev_err(bq->dev, "%s failed to read register 0x02:%d\n", __func__, ret);
		return ret;
	}

	if (((val & BQ2589X_CONV_RATE_MASK) >> BQ2589X_CONV_RATE_SHIFT) == BQ2589X_ADC_CONTINUE_ENABLE)
		return 0; /*is doing continuous scan*/
	if (oneshot) {
		ret = bq2589x_update_bits(bq, BQ2589X_REG_02, BQ2589X_CONV_START_MASK, BQ2589X_CONV_START << BQ2589X_CONV_START_SHIFT);
	} else
		ret = bq2589x_update_bits(bq, BQ2589X_REG_02, BQ2589X_CONV_RATE_MASK,  BQ2589X_ADC_CONTINUE_ENABLE << BQ2589X_CONV_RATE_SHIFT);
	return ret;
}

int bq2589x_adc_stop(struct bq2589x *bq)
{
	return bq2589x_update_bits(bq, BQ2589X_REG_02, BQ2589X_CONV_RATE_MASK, BQ2589X_ADC_CONTINUE_DISABLE << BQ2589X_CONV_RATE_SHIFT);
}

int bq2589x_adc_read_battery_volt(struct bq2589x *bq)
{
	uint8_t val;
	int volt;
	int ret;
	ret = bq2589x_read_byte(bq, &val, BQ2589X_REG_0E);
	if (ret < 0) {
		dev_err(bq->dev, "read battery voltage failed :%d\n", ret);
		return ret;
	} else{
		volt = BQ2589X_BATV_BASE + ((val & BQ2589X_BATV_MASK) >> BQ2589X_BATV_SHIFT) * BQ2589X_BATV_LSB ;
		return volt;
	}
}

#if 0
int bq2589x_adc_read_sys_volt(struct bq2589x *bq)
{
	uint8_t val;
	int volt;
	int ret;
	ret = bq2589x_read_byte(bq, &val, BQ2589X_REG_0F);
	if (ret < 0) {
		dev_err(bq->dev, "read system voltage failed :%d\n", ret);
		return ret;
	} else{
		volt = BQ2589X_SYSV_BASE + ((val & BQ2589X_SYSV_MASK) >> BQ2589X_SYSV_SHIFT) * BQ2589X_SYSV_LSB ;
		return volt;
	}
}
#endif

int bq2589x_adc_read_vbus_volt(struct bq2589x *bq)
{
	uint8_t val;
	int volt;
	int ret;
	ret = bq2589x_read_byte(bq, &val, BQ2589X_REG_11);
	if (ret < 0) {
		dev_err(bq->dev, "read vbus voltage failed :%d\n", ret);
		return ret;
	} else{
		volt = BQ2589X_VBUSV_BASE + ((val & BQ2589X_VBUSV_MASK) >> BQ2589X_VBUSV_SHIFT) * BQ2589X_VBUSV_LSB ;
		return volt;
	}
}

int bq2589x_read_vindpm_volt(struct bq2589x *bq)
{
	uint8_t val;
	int volt;
	int ret;
	ret = bq2589x_read_byte(bq, &val, BQ2589X_REG_0D);
	if (ret < 0) {
		dev_err(bq->dev, "read vbus voltage failed :%d\n", ret);
		return ret;
	} else{
		volt = BQ2589X_VBUSV_BASE + ((val & BQ2589X_VBUSV_MASK) >> BQ2589X_VBUSV_SHIFT) * BQ2589X_VBUSV_LSB ;
		return volt;
	}
}

#if 0
int bq2589x_adc_read_temperature(struct bq2589x *bq)
{
	uint8_t val;
	int temp;
	int ret;
	ret = bq2589x_read_byte(bq, &val, BQ2589X_REG_10);
	if (ret < 0) {
		dev_err(bq->dev, "read temperature failed :%d\n", ret);
		return ret;
	} else{
		temp = BQ2589X_TSPCT_BASE + ((val & BQ2589X_TSPCT_MASK) >> BQ2589X_TSPCT_SHIFT) * BQ2589X_TSPCT_LSB ;
		return temp;
	}
}
#endif

int bq2589x_adc_read_charge_current(struct bq2589x *bq)
{
	uint8_t val;
	int volt;
	int ret;
	ret = bq2589x_read_byte(bq, &val, BQ2589X_REG_12);
	if (ret < 0) {
		dev_err(bq->dev, "read charge current failed :%d\n", ret);
		return ret;
	} else{
		volt = (int)(BQ2589X_ICHGR_BASE + ((val & BQ2589X_ICHGR_MASK) >> BQ2589X_ICHGR_SHIFT) * BQ2589X_ICHGR_LSB) ;
		return volt;
	}
}

int bq2589x_set_charge_current(struct bq2589x *bq, int curr)
{
	u8 ichg;

	if (bq->part_no == SC89890H) {
		ichg = (curr - BQ2589X_ICHG_BASE)/SC89890H_ICHG_LSB;
	} else {
		ichg = (curr - BQ2589X_ICHG_BASE)/BQ2589X_ICHG_LSB;
	}

	return bq2589x_update_bits(bq, BQ2589X_REG_04, BQ2589X_ICHG_MASK, ichg << BQ2589X_ICHG_SHIFT);

}

int bq2589x_set_term_current(struct bq2589x *bq, int curr)
{
	u8 iterm;

	if (bq->part_no == SC89890H) {
		if (curr > SC89890H_ITERM_MAX)
			curr = SC89890H_ITERM_MAX;

		iterm = (curr - SC89890H_ITERM_BASE) / SC89890H_ITERM_LSB;
	} else {
		iterm = (curr - BQ2589X_ITERM_BASE) / BQ2589X_ITERM_LSB;
	}

	return bq2589x_update_bits(bq, BQ2589X_REG_05, BQ2589X_ITERM_MASK, iterm << BQ2589X_ITERM_SHIFT);
}

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

int bq2589x_set_chargevoltage(struct bq2589x *bq, int volt)
{
	u8 val;

	val = (volt - BQ2589X_VREG_BASE)/BQ2589X_VREG_LSB;
	return bq2589x_update_bits(bq, BQ2589X_REG_06, BQ2589X_VREG_MASK, val << BQ2589X_VREG_SHIFT);
}

static int bq2589x_bat_compensation(struct bq2589x *bq, int bat_comp)
{
	u8 val;

	val = (bat_comp - BQ2589X_BAT_COMP_BASE)/BQ2589X_BAT_COMP_LSB;
	return bq2589x_update_bits(bq, BQ2589X_REG_08, BQ2589X_BAT_COMP_MASK, val << BQ2589X_BAT_COMP_SHIFT);
}

static int bq2589x_voltage_clamp(struct bq2589x *bq, int vclamp)
{
	u8 val;

	val = (vclamp - BQ2589X_VCLAMP_BASE)/BQ2589X_VCLAMP_LSB;
	return bq2589x_update_bits(bq, BQ2589X_REG_08, BQ2589X_VCLAMP_MASK, val << BQ2589X_VCLAMP_SHIFT);
}

int bq2589x_set_input_volt_limit(struct bq2589x *bq, int volt)
{
	u8 val;
	val = (volt - BQ2589X_VINDPM_BASE) / BQ2589X_VINDPM_LSB;
	return bq2589x_update_bits(bq, BQ2589X_REG_0D, BQ2589X_VINDPM_MASK, val << BQ2589X_VINDPM_SHIFT);
}

int bq2589x_set_input_current_limit(struct bq2589x *bq, int curr)
{
	u8 val;
	val = (curr - BQ2589X_IINLIM_BASE) / BQ2589X_IINLIM_LSB;
	dev_err(bq->dev, "%s icl=%d \n", __func__, curr);
	return bq2589x_update_bits(bq, BQ2589X_REG_00, BQ2589X_IINLIM_MASK, val << BQ2589X_IINLIM_SHIFT);
}

int bq2589x_set_vindpm_offset(struct bq2589x *bq, int offset)
{
	u8 val;

	if (bq->part_no == SC89890H) {
		if (offset < 400)
			val = SC89890H_VINDPMOS_400MV;
		else
			val = SC89890H_VINDPMOS_600MV;

		return bq2589x_update_bits(bq, BQ2589X_REG_01, SC89890H_VINDPMOS_MASK, val << SC89890H_VINDPMOS_SHIFT);
	} else {
		val = (offset - BQ2589X_VINDPMOS_BASE)/BQ2589X_VINDPMOS_LSB;
		return bq2589x_update_bits(bq, BQ2589X_REG_01, BQ2589X_VINDPMOS_MASK, val << BQ2589X_VINDPMOS_SHIFT);
	}
}

int bq2589x_get_charging_status(struct bq2589x *bq)
{
	u8 val = 0;
	int ret;

	ret = bq2589x_read_byte(bq, &val, BQ2589X_REG_0B);
	if (ret < 0) {
		dev_err(bq->dev, "%s Failed to read register 0x0b:%d\n", __func__, ret);
		return ret;
	}
	val &= BQ2589X_CHRG_STAT_MASK;
	val >>= BQ2589X_CHRG_STAT_SHIFT;
	return val;
}

int bq2589x_disable_watchdog_timer(struct bq2589x *bq)
{
	u8 val = BQ2589X_WDT_DISABLE << BQ2589X_WDT_SHIFT;

	return bq2589x_update_bits(bq, BQ2589X_REG_07, BQ2589X_WDT_MASK, val);
}

int bq2589x_set_watchdog_timer(struct bq2589x *bq, u8 timeout)
{
	u8 val = timeout << BQ2589X_WDT_SHIFT;
	return bq2589x_update_bits(bq, BQ2589X_REG_07, BQ2589X_WDT_MASK, val);
}

int bq2589x_reset_watchdog_timer(struct bq2589x *bq)
{
	u8 val = BQ2589X_WDT_RESET << BQ2589X_WDT_RESET_SHIFT;

	return bq2589x_update_bits(bq, BQ2589X_REG_03, BQ2589X_WDT_RESET_MASK, val);
}

int bq2589x_is_dpdm_done(struct bq2589x *bq,int *done)
{
	int ret = 0;
	u8 data=0;

	if (bq->part_no == SC89890H) {
		ret = bq2589x_read_byte(bq, &data, BQ2589X_REG_0B);
		if (data & BQ2589X_PG_STAT_MASK)
			*done = 0;
		else
			*done = 1;
	} else {
		ret = bq2589x_read_byte(bq, &data, BQ2589X_REG_02);
		data &= (BQ2589X_FORCE_DPDM << BQ2589X_FORCE_DPDM_SHIFT);
		*done = (data >> BQ2589X_FORCE_DPDM_SHIFT);
	}

	return ret;
}

int bq2589x_force_dpdm(struct bq2589x *bq)
{
	u8 data = 0;
	u8 val = BQ2589X_FORCE_DPDM << BQ2589X_FORCE_DPDM_SHIFT;
	int ret;
	if (bq->part_no == SC89890H) {
		ret = bq2589x_read_byte(bq, &data, BQ2589X_REG_0B);
		if ((data & BQ2589X_VBUS_STAT_MASK) >> BQ2589X_VBUS_STAT_SHIFT
				== BQ2589X_VBUS_MAXC) {
			bq2589x_write_byte(bq, BQ2589X_REG_01, SC89890H_FORCE_DPDM1);
			msleep(30);
			bq2589x_write_byte(bq, BQ2589X_REG_01, SC89890H_FORCE_DPDM2);
			msleep(30);
		}
	}
	dev_err(bq->dev, "%s \n", __func__);
	return bq2589x_update_bits(bq, BQ2589X_REG_02, BQ2589X_FORCE_DPDM_MASK, val);
}

void bq2589x_force_dpdm_done(struct bq2589x *bq)
{
	int retry = 0;
	int bc_count = 200;
	int done = 1;

	bq2589x_force_dpdm(bq);
	while(retry++ < bc_count){
		msleep(20);
		bq2589x_is_dpdm_done(bq,&done);
		if(!done) //already known charger type
			break;
	}
}

/*
int bq2589x_enable_hvdcp(struct bq2589x *bq)
{
	u8 val = BQ2589X_HVDCP_ENABLE << BQ2589X_HVDCPEN_SHIFT;

	return bq2589x_update_bits(bq, BQ2589X_REG_02, BQ2589X_HVDCPEN_MASK, val);
}*/

int bq2589x_reset_chip(struct bq2589x *bq)
{
	int ret;
	u8 val = BQ2589X_RESET << BQ2589X_RESET_SHIFT;

	ret = bq2589x_update_bits(bq, BQ2589X_REG_14, BQ2589X_RESET_MASK, val);
	return ret;
}

#if 0
int bq2589x_enter_ship_mode(struct bq2589x *bq)
{
	int ret;
	u8 val = BQ2589X_BATFET_OFF << BQ2589X_BATFET_DIS_SHIFT;

	ret = bq2589x_update_bits(bq, BQ2589X_REG_09, BQ2589X_BATFET_DIS_MASK, val);
	return ret;

}
#endif

int bq2589x_enter_hiz_mode(struct bq2589x *bq)
{
	u8 val = BQ2589X_HIZ_ENABLE << BQ2589X_ENHIZ_SHIFT;

	return bq2589x_update_bits(bq, BQ2589X_REG_00, BQ2589X_ENHIZ_MASK, val);

}

int bq2589x_exit_hiz_mode(struct bq2589x *bq)
{

	u8 val = BQ2589X_HIZ_DISABLE << BQ2589X_ENHIZ_SHIFT;

	return bq2589x_update_bits(bq, BQ2589X_REG_00, BQ2589X_ENHIZ_MASK, val);

}

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

#if 0
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

int bq2589x_pumpx_increase_volt(struct bq2589x *bq)
{
	u8 val;
	int ret;

	val = BQ2589X_PUMPX_UP << BQ2589X_PUMPX_UP_SHIFT;

	ret = bq2589x_update_bits(bq, BQ2589X_REG_09, BQ2589X_PUMPX_UP_MASK, val);

	return ret;

}

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

int bq2589x_pumpx_decrease_volt(struct bq2589x *bq)
{
	u8 val;
	int ret;

	val = BQ2589X_PUMPX_DOWN << BQ2589X_PUMPX_DOWN_SHIFT;

	ret = bq2589x_update_bits(bq, BQ2589X_REG_09, BQ2589X_PUMPX_DOWN_MASK, val);

	return ret;

}

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

static int bq2589x_read_idpm_limit(struct bq2589x *bq)
{
	uint8_t val;
	int curr;
	int ret;

	ret = bq2589x_read_byte(bq, &val, BQ2589X_REG_13);
	if (ret < 0) {
		dev_err(bq->dev, "read vbus voltage failed :%d\n", ret);
		return ret;
	} else{
		curr = BQ2589X_IDPM_LIM_BASE + ((val & BQ2589X_IDPM_LIM_MASK) >> BQ2589X_IDPM_LIM_SHIFT) * BQ2589X_IDPM_LIM_LSB ;
		return curr;
	}
}
#endif

int bq2589x_force_ico(struct bq2589x *bq)
{
	u8 val;
	int ret;

	val = BQ2589X_FORCE_ICO << BQ2589X_FORCE_ICO_SHIFT;

	ret = bq2589x_update_bits(bq, BQ2589X_REG_09, BQ2589X_FORCE_ICO_MASK, val);

	return ret;
}

int bq2589x_check_force_ico_done(struct bq2589x *bq)
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

int bq2589x_enable_term(struct bq2589x* bq, bool enable)
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

int bq2589x_enable_auto_dpdm(struct bq2589x* bq, bool enable)
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

int bq2589x_use_absolute_vindpm(struct bq2589x* bq, bool enable)
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

bool bq2589x_is_charge_present(struct bq2589x *bq)
{
	int ret;
	u8 val;

	if (bq->part_no == SC89890H) {
		ret = bq2589x_read_byte(bq, &val, BQ2589X_REG_11);
		if (ret < 0) {
			dev_err(bq->dev, "%s:read REG11 failed :%d\n", __func__, ret);
			return false;
		}
		val &= BQ2589X_VBUS_GD_MASK;
		val >>= BQ2589X_VBUS_GD_SHIFT;
	} else {
		ret = bq2589x_read_byte(bq, &val, BQ2589X_REG_0B);
		if (ret < 0) {
			dev_err(bq->dev, "%s:read REG0B failed :%d\n", __func__, ret);
			return false;
		}
		val &= BQ2589X_PG_STAT_MASK;
		val >>= BQ2589X_PG_STAT_SHIFT;
	}
	return (val == 1);
}


bool bq2589x_is_charge_online(struct bq2589x *bq)
{
	int ret;
	u8 val;

	if (bq->part_no == SC89890H) {
		ret = bq2589x_read_byte(bq, &val, BQ2589X_REG_11);
		if (ret < 0) {
			dev_err(bq->dev, "%s:read REG11 failed :%d\n", __func__, ret);
			return false;
		}
		val &= BQ2589X_VBUS_GD_MASK;
		val >>= BQ2589X_VBUS_GD_SHIFT;
	} else {
		ret = bq2589x_read_byte(bq, &val, BQ2589X_REG_0B);
		if (ret < 0) {
			dev_err(bq->dev, "%s:read REG0B failed :%d\n", __func__, ret);
			return false;
		}
		val &= BQ2589X_PG_STAT_MASK;
		val >>= BQ2589X_PG_STAT_SHIFT;
	}
	return (val == 1);
}


bool bq2589x_is_charge_done(struct bq2589x *bq)
{
	int ret;
	u8 val;

	ret = bq2589x_read_byte(bq, &val, BQ2589X_REG_0B);
	if (ret < 0) {
		dev_err(bq->dev, "%s:read REG0B failed :%d\n", __func__, ret);
		return false;
	}
	val &= BQ2589X_CHRG_STAT_MASK;
	val >>= BQ2589X_CHRG_STAT_SHIFT;

	return (val == BQ2589X_CHRG_STAT_CHGDONE);
}

void bq2589x_dump_regs(struct bq2589x *bq)
{
	int addr, ret;
	u8 regs[22] = {0x0, };
	char temp_buf[128] = {0,};

	for (addr = 0x0; addr <= 0x14; addr++) {
		ret = bq2589x_read_byte(bq, &regs[addr], addr);
		if (ret != 0) {
			pr_err("fail to read reg [%.2x]", addr);
			regs[addr] = 0xFF;
		}
		sprintf(temp_buf + strlen(temp_buf), "0x%02X[0x%02X],", addr, regs[addr]);

		if ((addr == 10) || (addr == 0x14)) {
			pr_err("%s:%s\n", __func__, temp_buf);
			memset(temp_buf, 0x0, sizeof(temp_buf));
		}
	}
}

bool bq_check_vote(struct bq2589x *bq)
{
	bool vote_flag;

	if (!bq->usb_icl_votable)
		bq->usb_icl_votable = find_votable("ICL");

	if (!bq->fcc_votable)
		bq->fcc_votable = find_votable("FCC");

	if (!bq->input_suspend_votable)
		bq->input_suspend_votable = find_votable("INPUT_SUSPEND");

	if (!bq->usb_icl_votable || !bq->fcc_votable || !bq->input_suspend_votable) {
		vote_flag = false;
		dev_err(bq->dev, "%s: vote_flag %d\n", __func__, vote_flag);
	}
	else
		vote_flag = true;
	dev_err(bq->dev, "%s: vote_flag %d\n", __func__, vote_flag);

	return vote_flag;

}

int bq2589x_init_device(struct bq2589x *bq)
{
	int ret;
	int reserved_val = 0;

	/*common initialization*/
	bq2589x_disable_watchdog_timer(bq);
	bq2589x_bat_compensation(bq, BQ2589X_BAT_COMP);
	bq2589x_voltage_clamp(bq, BQ2589X_VCLAMP);
	bq2589x_enable_auto_dpdm(bq, bq->cfg.enable_auto_dpdm);
	bq2589x_enable_term(bq, bq->cfg.enable_term);
	bq2589x_enable_ico(bq, bq->cfg.enable_ico);
	bq2589x_exit_hiz_mode(bq);
	bq2589x_use_absolute_vindpm(bq, bq->cfg.use_absolute_vindpm);

	if (bq->part_no == SYV690 || bq->part_no == SC89890H) {
		ret = bq2589x_set_vindpm_offset(bq, 600);
		if (ret < 0) {
			dev_err(bq->dev, "%s:Failed to set vindpm offset:%d\n", __func__, ret);
			return ret;
		}
	}
	ret = bq2589x_set_input_volt_limit(bq, 4600);

	ret = bq2589x_set_term_current(bq, bq->cfg.term_current);
	if (ret < 0) {
		dev_err(bq->dev, "%s:Failed to set termination current:%d\n", __func__, ret);
		return ret;
	}

	ret = bq2589x_set_chargevoltage(bq, bq->cfg.charge_voltage);
	if (ret < 0) {
		dev_err(bq->dev, "%s:Failed to set charge voltage:%d\n", __func__, ret);
		return ret;
	}

	ret = bq2589x_set_charge_current(bq, bq->cfg.charge_current);
	if (ret < 0) {
		dev_err(bq->dev, "%s:Failed to set charge current:%d\n", __func__, ret);
		return ret;
	}

	ret = bq2589x_set_otg_volt(bq, bq->cfg.otg_vol);
	if (ret < 0) {
		dev_err(bq->dev, "%s:Failed to set charge voltage:%d\n", __func__, ret);
		return ret;
	}

	ret = bq2589x_set_otg_current(bq, bq->cfg.otg_current);
	if (ret < 0) {
		dev_err(bq->dev, "%s:Failed to set charge current:%d\n", __func__, ret);
		return ret;
	}

	ret = bq2589x_enable_charger(bq);
	if (ret < 0) {
		dev_err(bq->dev, "%s:Failed to enable charger:%d\n", __func__, ret);
		return ret;
	}

	bq->curr_flag = 0;
	//bq2589x_adc_start(bq, false);
	bq2589x_update_bits(bq, BQ2589X_REG_00, BQ2589X_ENILIM_MASK,
		BQ2589X_ENILIM_DISABLE << BQ2589X_ENILIM_SHIFT);
	bq2589x_update_bits(bq, BQ2589X_REG_02, 0x8, 1 << 3);
	if (bq->part_no == BQ25890) {
		bq2589x_update_bits(bq, BQ2589X_REG_01, 0x2, 0 << 1);
		bq2589x_update_bits(bq, BQ2589X_REG_02, 0x4, 0 << 2);
	} else if (bq->part_no == SYV690) {
		bq2589x_update_bits(bq, BQ2589X_REG_02, 0x4, 0 << 2);
	}

	reserved_val = bq2589x_get_reserved_val(bq);
	if (reserved_val)
		bq2589x_reserved_disable(bq);
	dev_err(bq->dev, "%s: get_reserved_val: %d\n", __func__, reserved_val);

	//bq2589x_update_bits(bq, BQ2589X_REG_08, BQ2589X_BAT_COMP_MASK, 2 << 5);
	//bq2589x_update_bits(bq, BQ2589X_REG_08, BQ2589X_VCLAMP_MASK, 1 << 2);
	//bq2589x_set_watchdog_timer(bq, BQ2589X_WDT_160S);
	//bq2589x_reset_watchdog_timer(bq);
	//bq2589x_enable_ico(bq, false);

	return ret;
}

int bq2589x_detect_device(struct bq2589x *bq)
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
		ret = bq2589x_read_byte(g_bq2589x, &val, addr);
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

int charger_request_dpdm(struct bq2589x *charger, int enable)//enable == false, enable Hiz
{
	int rc = 0;

	pr_err("%s: enable %d\n", __func__, enable);

	/* fetch the DPDM regulator */
	if (!charger->dpdm_reg && of_get_property(charger->dev->of_node,
				"dpdm-supply", NULL)) {
		charger->dpdm_reg = devm_regulator_get(charger->dev, "dpdm");
		if (IS_ERR(charger->dpdm_reg)) {
			rc = PTR_ERR(charger->dpdm_reg);
			pr_err("%s: Couldn't get dpdm regulator rc=%d\n",
					__func__, rc);
			charger->dpdm_reg = NULL;
			return rc;
		}
	}

	mutex_lock(&charger->dpdm_lock);
	if (enable) {
		if (charger->dpdm_reg && !charger->dpdm_enabled) {
			pr_err("%s: enabling DPDM regulator\n", __func__);
			rc = regulator_enable(charger->dpdm_reg);
			if (rc < 0)
				pr_err("%s: Couldn't enable dpdm regulator rc=%d\n",
					__func__, rc);
			else
				charger->dpdm_enabled = true;
		}
	} else {
		if (charger->dpdm_reg && charger->dpdm_enabled) {
			pr_err("%s: disabling DPDM regulator\n", __func__);
			rc = regulator_disable(charger->dpdm_reg);
			if (rc < 0)
				pr_err("%s: Couldn't disable dpdm regulator rc=%d\n",
					__func__, rc);
			else
				charger->dpdm_enabled = false;
		}
	}
	mutex_unlock(&charger->dpdm_lock);

	return rc;
}

static int bq2589x_parse_dt(struct device *dev, struct bq2589x *bq)
{
	int ret;
	struct device_node *np = dev->of_node;

	bq->cfg.enable_auto_dpdm = of_property_read_bool(np, "ti,bq2589x,enable-auto-dpdm");
	bq->cfg.enable_term = of_property_read_bool(np, "ti,bq2589x,enable-termination");
	bq->cfg.enable_ico = of_property_read_bool(np, "ti,bq2589x,enable-ico");
	bq->cfg.use_absolute_vindpm = of_property_read_bool(np, "ti,bq2589x,use-absolute-vindpm");

	ret = of_property_read_u32(np, "ti,bq2589x,otg_vol", &bq->cfg.otg_vol);
	if (ret)
		return ret;

	ret = of_property_read_u32(np, "ti,bq2589x,otg_current", &bq->cfg.otg_current);
	if (ret)
		return ret;

	ret = of_property_read_u32(np, "ti,bq2589x,charge-voltage",&bq->cfg.charge_voltage);
	if (ret)
		return ret;

	ret = of_property_read_u32(np, "ti,bq2589x,charge-current",&bq->cfg.charge_current);
	if (ret)
		return ret;

	ret = of_property_read_u32(np, "ti,bq2589x,term-current",&bq->cfg.term_current);
	if (ret)
		return ret;

	bq->otg_gpio = of_get_named_gpio(np, "otg-gpio", 0);
	if (bq->otg_gpio < 0) {
		pr_err("%s no otg_gpio info\n", __func__);
	};

	bq->irq_gpio = of_get_named_gpio(np, "intr-gpio", 0);
	if (bq->irq_gpio < 0) {
		pr_err("%s no intr_gpio info\n", __func__);
	} else {
		pr_err("%s intr_gpio infoi %d\n", __func__, bq->irq_gpio);
	}

	return 0;
}

static void bq2589x_adapter_in_workfunc(struct work_struct *work)
{
	struct bq2589x *bq = container_of(work, struct bq2589x, adapter_in_work);

	if(!bq_check_vote(bq))
		return;
	bq2589x_adc_start(bq, false);
	if (g_battmngr_noti->pd_msg.pd_active) {
		bq2589x_set_input_volt_limit(bq, 4700);
		charger_request_dpdm(bq, false);
		dev_info(bq->dev, "%s:PD plugged in\n", __func__);
	} else if (bq->vbus_type == BQ2589X_VBUS_MAXC) {
		bq2589x_set_input_volt_limit(bq, 8400);
		dev_info(bq->dev, "%s:HVDCP or Maxcharge adapter plugged in\n", __func__);
		//schedule_delayed_work(&bq->ico_work, msecs_to_jiffies(500));
	} else if (bq->vbus_type == BQ2589X_VBUS_USB_DCP) {
		bq2589x_set_input_volt_limit(bq, 4700);
		dev_info(bq->dev, "%s:usb dcp adapter plugged in\n", __func__);
	} else if (bq->vbus_type == BQ2589X_VBUS_USB_SDP) {
		dev_info(bq->dev, "%s:host SDP plugged in\n", __func__);
		bq2589x_set_input_volt_limit(bq, 4600);
	} else if (bq->vbus_type == BQ2589X_VBUS_USB_CDP) {
		dev_info(bq->dev, "%s:host CDP plugged in\n", __func__);
		bq2589x_set_input_volt_limit(bq, 4600);
	} else if (bq->vbus_type == BQ2589X_VBUS_UNKNOWN || bq->vbus_type == BQ2589X_VBUS_NONSTAND) {
		dev_info(bq->dev, "%s:host FLOAT plugged in\n", __func__);
		bq2589x_set_input_volt_limit(bq, 4600);
	} else {
		dev_info(bq->dev, "%s:other adapter plugged in,vbus_type is %d\n", __func__, bq->vbus_type);
		bq2589x_set_input_volt_limit(bq, 4400);
		//schedule_delayed_work(&bq->ico_work, 0);
	}

	cancel_delayed_work_sync(&bq->monitor_work);
	schedule_delayed_work(&bq->monitor_work, 0);
}

static void bq2589x_adapter_out_workfunc(struct work_struct *work)
{
	struct bq2589x *bq = container_of(work, struct bq2589x, adapter_out_work);
	int ret;

	ret = bq2589x_set_input_current_limit(bq, 100);
	ret = bq2589x_set_input_volt_limit(bq, 4600);
	if(bq_check_vote(bq))
		vote(bq->fcc_votable, DETECT_FCC_VOTER, true, 500000);
	if (ret < 0)
		dev_err(bq->dev,"%s:reset vindpm threshold to 4400 failed:%d\n",__func__,ret);
	else
		dev_info(bq->dev,"%s:reset vindpm threshold to 4400 successfully\n",__func__);

	vote(bq->fcc_votable, BQ_FCC_VOTER, false, 0);
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
			dev_info(bq->dev, "%s:ICO command issued failed:%d\n", __func__, ret);
		} else {
			ico_issued = true;
			schedule_delayed_work(&bq->ico_work, 3 * HZ);
			dev_info(bq->dev, "%s:ICO command issued successfully\n", __func__);
		}
	} else {
		ico_issued = false;
		ret = bq2589x_check_force_ico_done(bq);
		if (ret) {/*ico done*/
			ret = bq2589x_read_byte(bq, &status, BQ2589X_REG_13);
			if (ret == 0) {
				idpm = ((status & BQ2589X_IDPM_LIM_MASK) >> BQ2589X_IDPM_LIM_SHIFT) * BQ2589X_IDPM_LIM_LSB + BQ2589X_IDPM_LIM_BASE;
				dev_info(bq->dev, "%s:ICO done, result is:%d mA\n", __func__, idpm);
			}
		}
	}
}

static void bq2589x_force_workfunc(struct work_struct *work)
{
	struct bq2589x *bq = container_of(work, struct bq2589x, force_work.work);
	u8 val = 0;

	bq->force_exit_flag = true;
	dev_err(bq->dev, "%s: val %d, force_exit_flag %d\n",
				__func__, val, bq->force_exit_flag);
	val = bq2589x_get_vbus_type(bq);
	if((val == BQ2589X_VBUS_UNKNOWN) || (val == BQ2589X_VBUS_NONSTAND)) {
		bq->old_type = BQ2589X_VBUS_UNKNOWN;
		dev_err(bq->dev, "%s: val %d\n", __func__, val);
		bq2589x_force_dpdm_done(bq);
		bq->old_type = BQ2589X_VBUS_NONE;
	}
	bq->force_exit_flag = false;
}

static void bq2589x_monitor_workfunc(struct work_struct *work)
{
	struct bq2589x *bq = container_of(work, struct bq2589x, monitor_work.work);
	u8 status = 0;
	int ret;
	int chg_current;
	int vindpm = 0;
	static int ibus_limit = 500;
	int thermal_icl = g_battmngr_noti->misc_msg.thermal_icl / 1000;
  	union power_supply_propval val = {0,};
	union power_supply_propval soc_val = {0,};
	static int countForcdpdm = 0;
	static int last_bc12_type;

	if(!bq_check_vote(bq))
		return;

	if (!thermal_icl)
		thermal_icl = ibus_limit;

//	bq2589x_reset_watchdog_timer(bq);
	bq2589x_dump_regs(bq);
	bq->vbus_volt = bq2589x_adc_read_vbus_volt(bq);
	bq->vbat_volt = bq2589x_adc_read_battery_volt(bq);
	chg_current = bq2589x_adc_read_charge_current(bq);
	bq->input_suspend = (get_client_vote(bq->input_suspend_votable, USER_VOTER) == 1);
	g_battmngr_noti->mainchg_msg.chg_done = bq2589x_is_charge_done(bq);
	if(bq->batt_psy)
		power_supply_get_property(bq->batt_psy, POWER_SUPPLY_PROP_CAPACITY, &soc_val);

	if (g_battmngr_noti->fg_msg.recharge_flag) {
		bq2589x_disable_charger(bq);
		bq2589x_enable_charger(bq);
		dev_err(bq->dev, "%s recharge flag enable\n", __func__);
		g_battmngr_noti->fg_msg.recharge_flag = false;
	}

	if (bq->input_suspend) {
		ibus_limit = 100;
		dev_info(bq->dev, "%s:input_suspend=%d\n", __func__, bq->input_suspend);
	} else if (!g_battmngr_noti->fg_msg.chip_ok) {
		ibus_limit = 300;
	} else if (g_battmngr_noti->pd_msg.pd_active) {
		if (g_battmngr_noti->pd_msg.pd_active == 1) {
			if ((g_battmngr_noti->pd_msg.pd_curr_max > 0) &&
				(g_battmngr_noti->pd_msg.pd_curr_max <= 3000000))
				ibus_limit = (g_battmngr_noti->pd_msg.pd_curr_max > g_battmngr_noti->misc_msg.thermal_icl) ? thermal_icl : g_battmngr_noti->pd_msg.pd_curr_max / 1000;
			else
				ibus_limit = thermal_icl;// : 3000;
		} else {
			ibus_limit = 3000;
		}
		if(get_client_vote(bq->usb_icl_votable, MAIN_CHG_ICL_VOTER) == 100 * 1000)
			ibus_limit = 100;
		else
			dev_err(bq->dev, "%s: icl of MAIN_CHG_ICL_VOTER is %d\n", __func__, get_client_vote(bq->usb_icl_votable, MAIN_CHG_ICL_VOTER));
		vote(bq->fcc_votable, BQ_FCC_VOTER, false, 0);
		bq2589x_set_input_volt_limit(bq, 4700);
	} else if(bq->vbus_type == BQ2589X_VBUS_MAXC) {
		ibus_limit = thermal_icl;// : 1500;
		if (bq->vbus_volt < 5600)
			bq2589x_set_input_volt_limit(bq, 4700);
		else
			bq2589x_set_input_volt_limit(bq, 8400);
	} else if(bq->vbus_type == BQ2589X_VBUS_USB_DCP) {
		ibus_limit = thermal_icl;// : 1500;
		if (!g_battmngr_noti->misc_msg.vindpm_temp) {
			if (soc_val.intval < 65)
				bq2589x_set_input_volt_limit(bq, 4400);
			else
				bq2589x_set_input_volt_limit(bq, 4700);
		}

	} else if(bq->vbus_type == BQ2589X_VBUS_USB_CDP) {
		ibus_limit = thermal_icl;// : 1500;
		if (!g_battmngr_noti->misc_msg.vindpm_temp) {
			if (soc_val.intval < 65)
				bq2589x_set_input_volt_limit(bq, 4400);
			else
				bq2589x_set_input_volt_limit(bq, 4700);
		}

	} else if(bq->vbus_type == BQ2589X_VBUS_USB_SDP) {
		ibus_limit = (thermal_icl < 500) ? thermal_icl : 500;
		if (!g_battmngr_noti->misc_msg.vindpm_temp) {
			if (soc_val.intval < 65)
				bq2589x_set_input_volt_limit(bq, 4400);
			else
				bq2589x_set_input_volt_limit(bq, 4600);
		}

	} else if(bq->vbus_type == BQ2589X_VBUS_UNKNOWN || bq->vbus_type == BQ2589X_VBUS_NONSTAND) {
		ibus_limit = (thermal_icl < 1000) ? thermal_icl : 1000;
	}
	ret = bq2589x_set_input_current_limit(bq, ibus_limit);
	dev_info(bq->dev, "%s:pd_active=%d icl=%d\n", __func__, g_battmngr_noti->pd_msg.pd_active, ibus_limit);
	if((g_battmngr_noti->pd_msg.pd_active == 0) && (bq->vbus_type == BQ2589X_VBUS_NONE) && (countForcdpdm < 2)){
		dev_info(bq->dev, "%s:force dpdm again when bc1.2 type is 0\n", __func__);
		bq2589x_force_dpdm_done(bq);
		countForcdpdm ++;
	}		
	if (!bq->curr_flag || last_bc12_type != bq->vbus_type) {
		if (g_battmngr_noti->pd_msg.pd_active) {
			charger_request_dpdm(bq, false);
			dev_info(bq->dev, "%s:PD plugged in,bc1.2=%d\n", __func__, bq->vbus_type);
		} else if (bq->vbus_type == BQ2589X_VBUS_MAXC) {
			vote(bq->fcc_votable, BQ_FCC_VOTER, true, 3200000);
		} else if (bq->vbus_type == BQ2589X_VBUS_USB_DCP) {
			vote(bq->fcc_votable, BQ_FCC_VOTER, true, 1971000);
		} else if (bq->vbus_type == BQ2589X_VBUS_USB_SDP) {
			vote(bq->fcc_votable, BQ_FCC_VOTER, true, 657000);
		} else if (bq->vbus_type == BQ2589X_VBUS_USB_CDP) {
			vote(bq->fcc_votable, BQ_FCC_VOTER, true, 1971000);
		} else if (bq->vbus_type == BQ2589X_VBUS_UNKNOWN || bq->vbus_type == BQ2589X_VBUS_NONSTAND) {
			vote(bq->fcc_votable, BQ_FCC_VOTER, true, 1314000);
		}
		bq->curr_flag = 1;
		vote(bq->fcc_votable, DETECT_FCC_VOTER, false, 0);
	}
	last_bc12_type = bq->vbus_type;

	if(bq->batt_psy)
		power_supply_get_property(bq->batt_psy, POWER_SUPPLY_PROP_TEMP, &val);
	if (val.intval < 100)
		bq2589x_update_bits(bq, BQ2589X_REG_06, 1, 1);
	else
		bq2589x_update_bits(bq, BQ2589X_REG_06, 1, 0);
	if (val.intval < 100)
		bq2589x_update_bits(bq, BQ2589X_REG_08, BQ2589X_BAT_COMP_MASK, 2 << 5);

	vindpm = bq2589x_read_vindpm_volt(bq);

	dev_info(bq->dev, "%s:vbus volt:%d,vbat volt:%d,charge current:%d, temp %d, vindpm %d\n",
			__func__, bq->vbus_volt, bq->vbat_volt, chg_current, val.intval, vindpm);

	ret = bq2589x_read_byte(bq, &status, BQ2589X_REG_13);
	if (ret == 0 && (status & BQ2589X_VDPM_STAT_MASK))
		dev_info(bq->dev, "%s:VINDPM occurred\n", __func__);
	if (ret == 0 && (status & BQ2589X_IDPM_STAT_MASK))
		dev_info(bq->dev, "%s:IINDPM occurred\n", __func__);

	/* read temperature,or any other check if need to decrease charge current*/
	schedule_delayed_work(&bq->monitor_work, msecs_to_jiffies(3000));
}

static void bq2589x_charger_irq_workfunc(struct work_struct *work)
{
	struct bq2589x *bq = container_of(work, struct bq2589x, irq_work);
	u8 status = 0;
	u8 fault = 0;
	u8 vbus_state = 0;
	u8 vbus_gd = 0;
	u8 power_gd = 0;
	u8 charge_status = 0;
	static int last_vbus_type;
	static int last_plug_in;

	int ret;

	//bq2589x_set_watchdog_timer(bq, BQ2589X_WDT_160S);
	//bq2589x_reset_watchdog_timer(bq);
	if (bq->part_no == SC89890H)
		msleep(200);
	else if (bq->part_no == SYV690)
		msleep(100);

	bq2589x_dump_regs(bq);

	/* Read STATUS and FAULT registers */
	ret = bq2589x_read_byte(bq, &status, BQ2589X_REG_0B);
	if (ret)
		return;

	ret = bq2589x_read_byte(bq, &fault, BQ2589X_REG_0C);
	if (ret)
		return;

	bq->vbus_type = (status & BQ2589X_VBUS_STAT_MASK) >> BQ2589X_VBUS_STAT_SHIFT;
	if(((bq->vbus_type == BQ2589X_VBUS_UNKNOWN) || (bq->vbus_type == BQ2589X_VBUS_NONSTAND))) {
		dev_err(bq->dev, "%s:type=%d,status=%d, hz_flag=%d, force_exit_flag=%d, retry_count=%d\n",
			__func__, bq->vbus_type, bq->status, bq->hz_flag, bq->force_exit_flag, bq->count);
		if((!bq->hz_flag) && (!bq->force_exit_flag) && (bq->count < 3)) {
			dev_info(bq->dev, "%s:float type, rerun bc1.2\n", __func__);
			schedule_delayed_work(&bq->force_work, msecs_to_jiffies(2000));
			bq->count++;
		}
	}

	if ((status & BQ2589X_PG_STAT_MASK) && !(bq->status & BQ2589X_STATUS_PG))
		bq->status |= BQ2589X_STATUS_PG;
	else if (!(status & BQ2589X_PG_STAT_MASK) && (bq->status & BQ2589X_STATUS_PG))
		bq->status &= ~BQ2589X_STATUS_PG;

	ret = bq2589x_read_byte(bq, &vbus_state, BQ2589X_REG_11);
	if (ret)
		return;
	vbus_gd = bq->vbus_state;
	power_gd = bq->power_state;
	bq->vbus_state = !!(vbus_state & BQ2589X_VBUS_GD_MASK);
	bq->power_state = !!(status & BQ2589X_PG_STAT_MASK);
	if (!(bq->vbus_state) && !(bq->power_state))
		charger_request_dpdm(bq, false);
	if ((((bq->part_no == SYV690) && !power_gd && bq->power_state)
			|| ((bq->part_no == SC89890H) && !vbus_gd && bq->vbus_state))
		&& !g_battmngr_noti->pd_msg.pd_active
		&& (bq->vbus_type == BQ2589X_VBUS_NONE)) {
		dev_info(bq->dev, "%s:vbus gd is on\n", __func__);
		ret = bq2589x_set_input_volt_limit(bq, 4600);
		ret = bq2589x_set_input_current_limit(bq, 100);
		charger_request_dpdm(bq, true);
		msleep(5);
		bq2589x_force_dpdm_done(bq);
	}

	dev_err(bq->dev, "%s: part_no=%d, vbus_state=%d, power_good=%d, vbus_type=%d, status=%d\n", __func__,
			bq->part_no, bq->vbus_state, bq->power_state, bq->vbus_type, bq->status);

	if ((((bq->part_no == SYV690) && (!(bq->vbus_state) || !(bq->status & BQ2589X_STATUS_PG)))
		|| ((bq->part_no == SC89890H) && !(bq->vbus_state)))
		&& ((bq->vbus_type == BQ2589X_VBUS_NONE) || (bq->vbus_type == BQ2589X_VBUS_OTG))
		&& (bq->status & BQ2589X_STATUS_PLUGIN)) {
		cancel_delayed_work_sync(&bq->monitor_work);
		cancel_delayed_work_sync(&bq->force_work);
		msleep(10);
		if (bq->part_no == SYV690)
			bq->old_type = BQ2589X_VBUS_NONE;
		bq->force_exit_flag = false;
		bq->curr_flag = 0;
		bq->count = 0;
		bq->vbus_state = 0;
		bq->power_state = 0;
		charger_request_dpdm(bq, false);
		if (bq->part_no == SC89890H)
			sc89890h_set_hv(bq, SC89890H_HV_REMOVE);
		dev_info(bq->dev, "%s:adapter removed\n", __func__);
		if (bq->vbus_type != BQ2589X_VBUS_OTG)
			bq2589x_adc_stop(bq);
		bq->status &= ~BQ2589X_STATUS_PLUGIN;
		cancel_delayed_work_sync(&bq->force_work);
		g_battmngr_noti->mainchg_msg.chg_plugin = 0;
		cancel_delayed_work_sync(&bq->monitor_work);
		schedule_work(&bq->adapter_out_work);
	} else if (bq->vbus_state
		&& (g_battmngr_noti->pd_msg.pd_active || bq->vbus_type != BQ2589X_VBUS_NONE)
		&& (bq->vbus_type != BQ2589X_VBUS_OTG)
		&& !(bq->status & BQ2589X_STATUS_PLUGIN)) {
		dev_info(bq->dev, "%s:adapter plugged in\n", __func__);
		bq->status |= BQ2589X_STATUS_PLUGIN;
		bq2589x_update_bits(bq, BQ2589X_REG_00, BQ2589X_ENILIM_MASK,
			BQ2589X_ENILIM_DISABLE << BQ2589X_ENILIM_SHIFT);
		g_battmngr_noti->mainchg_msg.chg_plugin = 1;
		schedule_work(&bq->adapter_in_work);
	}


	if (fault && !(bq->status & BQ2589X_STATUS_FAULT))
		bq->status |= BQ2589X_STATUS_FAULT;
	else if (!fault && (bq->status & BQ2589X_STATUS_FAULT))
		bq->status &= ~BQ2589X_STATUS_FAULT;

	charge_status = (status & BQ2589X_CHRG_STAT_MASK) >> BQ2589X_CHRG_STAT_SHIFT;
	if (charge_status == BQ2589X_CHRG_STAT_IDLE) {
		dev_info(bq->dev, "%s:not charging\n", __func__);
	} else if (charge_status == BQ2589X_CHRG_STAT_PRECHG)
		dev_info(bq->dev, "%s:precharging\n", __func__);
	else if (charge_status == BQ2589X_CHRG_STAT_FASTCHG)
		dev_info(bq->dev, "%s:fast charging\n", __func__);
	else if (charge_status == BQ2589X_CHRG_STAT_CHGDONE)
		dev_info(bq->dev, "%s:charge done!\n", __func__);
/*
	if (fault) {
		dev_info(bq->dev, "%s:charge fault:%02x\n", __func__,fault);
		if(fault & 0x40) {
			dev_info(bq->dev, "%s:otg error ,boost ovp fault\n", __func__);
			bq2589x_reset_chip(bq);
			msleep(5);
			bq2589x_init_device(bq);
		}
		if(fault & 0x80){
			bq2589x_reset_chip(bq);
			msleep(5);
			bq2589x_init_device(bq);
		}
	}
*/
	bq2589x_get_chg_usb_type(bq);
	if (bq->charge_type != POWER_SUPPLY_TYPE_UNKNOWN) {
		if (bq->part_no == SC89890H) {
			dev_info(bq->dev, "%s:absolute_vindpm\n", __func__);
			bq2589x_use_absolute_vindpm(bq, bq->cfg.use_absolute_vindpm);
		}
	}
	if (bq->charge_type == POWER_SUPPLY_TYPE_USB_FLOAT) {
		ret = bq2589x_set_input_current_limit(bq, 1000);
	}

	if (((last_vbus_type != bq->vbus_type)
		|| (last_plug_in != g_battmngr_noti->mainchg_msg.chg_plugin))
		&& g_battmngr_noti) {
		mutex_lock(&g_battmngr_noti->notify_lock);
		g_battmngr_noti->mainchg_msg.chg_type = bq->charge_type;
		g_battmngr_noti->mainchg_msg.msg_type = BATTMNGR_MSG_MAINCHG_TYPE;
		battmngr_notifier_call_chain(BATTMNGR_EVENT_MAINCHG, g_battmngr_noti);
		mutex_unlock(&g_battmngr_noti->notify_lock);
	}
	last_vbus_type = bq->vbus_type;
	last_plug_in = g_battmngr_noti->mainchg_msg.chg_plugin;
	return;
}

static irqreturn_t bq2589x_charger_interrupt(int irq, void *data)
{
	struct bq2589x *bq = data;

	dev_err(bq->dev, "%s: charger device bq25890 irq detected, revision:%d,bq->vbus_type %d\n",
			__func__, bq->revision, bq->charge_type);

	schedule_work(&bq->irq_work);

	return IRQ_HANDLED;
}

static void bq2589x_first_irq_workfunc(struct work_struct *work)
{
	struct bq2589x *bq = container_of(work, struct bq2589x, first_irq_work.work);

	dev_err(bq->dev, "%s: start irq_work\n", __func__);
	schedule_work(&bq->irq_work);
}

static void bq2589x_dump_regs_workfunc(struct work_struct *work)
{
	struct bq2589x *bq = container_of(work, struct bq2589x, dump_regs_work.work);

	dev_err(bq->dev, "%s: start dump_regs_work\n", __func__);
	bq2589x_dump_regs(bq);
	schedule_delayed_work(&bq->dump_regs_work, msecs_to_jiffies(300000));
}

static int bq2589x_charger_probe(struct i2c_client *client,
			   const struct i2c_device_id *id)
{
	struct iio_dev *indio_dev;
	struct bq2589x *bq;
	int irqn;
	int ret;
	static int probe_cnt = 0;

	pr_err("%s probe_cnt = %d\n", __func__, ++probe_cnt);

	indio_dev = devm_iio_device_alloc(&client->dev, sizeof(*bq));
	bq = iio_priv(indio_dev);
	if (!bq) {
		dev_err(&client->dev, "%s: out of memory\n", __func__);
		return -ENOMEM;
	}

	bq->indio_dev = indio_dev;
	bq->dev = &client->dev;
	bq->client = client;
	i2c_set_clientdata(client, bq);

	bq->force_exit_flag = false;
	bq->hz_flag = false;
	bq->count = 0;
	g_bq2589x = bq;

	mutex_init(&bq->bq2589x_i2c_lock);
	mutex_init(&bq->dpdm_lock);

	ret = bq2589x_detect_device(bq);
	if (!ret && bq->part_no == BQ25890) {
		bq->status |= BQ2589X_STATUS_EXIST;
		dev_err(bq->dev, "%s: charger device bq25890 detected, revision:%d\n", __func__, bq->revision);
	} else if (!ret && bq->part_no == SYV690) {
		bq->status |= BQ2589X_STATUS_EXIST;
		dev_err(bq->dev, "%s: charger device SYV690 detected, revision:%d\n", __func__, bq->revision);
	} else if (!ret && bq->part_no == SC89890H) {
		bq->status |= BQ2589X_STATUS_EXIST;
		dev_err(bq->dev, "%s: charger device SC89890H detected, revision:%d\n", __func__, bq->revision);
	} else {
		dev_err(bq->dev, "%s: no charger device found:%d\n", __func__, ret);
		ret = -EPROBE_DEFER;
		msleep(500);
		if (probe_cnt >= PROBE_CNT_MAX)
			return -ENODEV;
		else
			goto no_charger_device;
	}

	bq->batt_psy = power_supply_get_by_name("battery");
	bq->usb_psy = power_supply_get_by_name("usb");
	if (!g_battmngr || !bq->batt_psy || !bq->usb_psy) {
		pr_err("%s g_battmngr or battery or usb not ready\n", __func__);
		ret = -EPROBE_DEFER;
		msleep(100);
		if (probe_cnt >= PROBE_CNT_MAX)
			goto out;
		else
			goto err_get_power_supply;
	}

	ret = bq_check_vote(bq);
	if (ret == 0) {
		pr_err("Failed to initialize BQ VOTE, rc=%d\n", ret);
	}

	ret = bq_init_iio_psy(bq);
	if (ret < 0) {
		pr_err("Failed to initialize BQ IIO PSY, rc=%d\n", ret);
	}

	bq->xm_ws = wakeup_source_register(bq->dev, "bq25890_wakeup");
	if (!bq->xm_ws) {
		pr_err("xm chg workup fail!\n");
		wakeup_source_unregister(bq->xm_ws);
	}

	if (client->dev.of_node)
		bq2589x_parse_dt(&client->dev, bq);

	ret = bq2589x_init_device(bq);
	if (ret) {
		dev_err(bq->dev, "device init failure: %d\n", ret);
		goto err_0;
	}

	ret = gpio_request(bq->irq_gpio, "bq2589x irq pin");
	if (ret) {
		dev_err(bq->dev, "%s: %d gpio request failed\n", __func__, bq->irq_gpio);
		goto err_0;
	}

	irqn = gpio_to_irq(bq->irq_gpio);
	if (irqn < 0) {
		dev_err(bq->dev, "%s:%d gpio_to_irq failed\n", __func__, irqn);
		ret = irqn;
		goto err_1;
	}
	client->irq = irqn;

	ret = devm_gpio_request(&client->dev, bq->otg_gpio, "bq2589x otg pin");
	if (ret) {
		dev_err(bq->dev, "%s: %d gpio request failed\n", __func__, bq->otg_gpio);
		goto err_1;
	}
	gpio_direction_output(g_bq2589x->otg_gpio, false);

	INIT_WORK(&bq->irq_work, bq2589x_charger_irq_workfunc);
	INIT_WORK(&bq->adapter_in_work, bq2589x_adapter_in_workfunc);
	INIT_WORK(&bq->adapter_out_work, bq2589x_adapter_out_workfunc);
	INIT_DELAYED_WORK(&bq->monitor_work, bq2589x_monitor_workfunc);
	INIT_DELAYED_WORK(&bq->ico_work, bq2589x_ico_workfunc);
	INIT_DELAYED_WORK(&bq->force_work, bq2589x_force_workfunc);
	INIT_DELAYED_WORK(&bq->first_irq_work, bq2589x_first_irq_workfunc);
	INIT_DELAYED_WORK(&bq->dump_regs_work, bq2589x_dump_regs_workfunc);

	ret = sysfs_create_group(&bq->dev->kobj, &bq2589x_attr_group);
	if (ret) {
		dev_err(bq->dev, "failed to register sysfs. err: %d\n", ret);
		goto err_irq;
	}

	ret = request_irq(client->irq, bq2589x_charger_interrupt,
				IRQF_TRIGGER_FALLING | IRQF_ONESHOT, "bq2589x_charger1_irq", bq);
	if (ret) {
		dev_err(bq->dev, "%s:Request IRQ %d failed: %d\n", __func__, client->irq, ret);
		goto err_irq;
	} else {
		dev_info(bq->dev, "%s:irq = %d\n", __func__, client->irq);
	}
	vote(bq->fcc_votable, DETECT_FCC_VOTER, true, 500000);
	charger_request_dpdm(bq, true);
	enable_irq_wake(irqn);
	bq2589x_dump_regs(bq);
	schedule_delayed_work(&bq->first_irq_work, msecs_to_jiffies(100));
	schedule_delayed_work(&bq->dump_regs_work, msecs_to_jiffies(10000));

out:
	i2c_set_clientdata(client, bq);
	pr_err("%s %s!!\n", __func__, ret == -EPROBE_DEFER ?
				"Over probe cnt max" : "OK");
	return 0;

err_irq:
	cancel_work_sync(&bq->irq_work);
	cancel_work_sync(&bq->adapter_in_work);
	cancel_work_sync(&bq->adapter_out_work);
	cancel_delayed_work_sync(&bq->monitor_work);
	cancel_delayed_work_sync(&bq->ico_work);
	cancel_delayed_work_sync(&bq->force_work);
	cancel_delayed_work_sync(&bq->first_irq_work);
	cancel_delayed_work_sync(&bq->dump_regs_work);
err_1:
err_0:
err_get_power_supply:
no_charger_device:

	return ret;
}

static void bq2589x_charger_shutdown(struct i2c_client *client)
{
	struct bq2589x *bq = i2c_get_clientdata(client);

	bq2589x_disable_otg(bq);
	bq2589x_enable_charger(bq);
	bq2589x_exit_hiz_mode(bq);
	bq2589x_adc_start(bq, true);
	bq2589x_adc_stop(bq);
	bq2589x_set_input_volt_limit(bq, 4400);
	msleep(2);
	dev_info(bq->dev, "%s: shutdown\n", __func__);
	sysfs_remove_group(&bq->dev->kobj, &bq2589x_attr_group);
	mutex_destroy(&bq->bq2589x_i2c_lock);
	mutex_destroy(&bq->dpdm_lock);
	cancel_work_sync(&bq->irq_work);
	cancel_work_sync(&bq->adapter_in_work);
	cancel_work_sync(&bq->adapter_out_work);
	cancel_delayed_work_sync(&bq->monitor_work);
	cancel_delayed_work_sync(&bq->ico_work);
	cancel_delayed_work_sync(&bq->force_work);
	cancel_delayed_work_sync(&bq->first_irq_work);
	cancel_delayed_work_sync(&bq->dump_regs_work);

	if (bq->client->irq) {
		disable_irq(bq->client->irq);
		free_irq(bq->client->irq, bq);
		gpio_free(bq->irq_gpio);
	}
}

static struct of_device_id bq2589x_charger_match_table[] = {
	{.compatible = "ti,bq2589x-charger",},
	{},
};
MODULE_DEVICE_TABLE(of, bq2589x_charger_match_table);

static const struct i2c_device_id bq2589x_charger_id[] = {
	{ "bq2589x-charger", BQ25890 },
	{},
};

MODULE_DEVICE_TABLE(i2c, bq2589x_charger_id);

static struct i2c_driver bq2589x_charger_driver = {
	.driver		= {
		.name	= "bq2589x-charger",
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
