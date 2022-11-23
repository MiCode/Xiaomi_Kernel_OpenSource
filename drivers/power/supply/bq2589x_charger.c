// SPDX-License-Identifier: GPL-2.0-or-later
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
#include <linux/platform_device.h>
#include <linux/regulator/driver.h>
#include "bq2589x_reg.h"
#include "charger_class.h"
#include "mtk_charger.h"

struct delayed_work plug_work;
struct bq2589x *bq_ex;
int boot_mode;
bool prev_pg;
bool update;
bool force_discharging_flag;
int thub_plug_flag;
bool pd_type;

#define NULL ((void *)0)

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

	struct chg_para usb;
};

enum {
	PN_BQ25890,
	PN_BQ25892,
	PN_BQ25895,
	PN_SYV6970QCC,
	PN_SC89890H,
};

static const u32 bq2589x_otg_oc_threshold[] = {
	500000, 700000, 1100000, 1300000, 1800000, 2100000, 2400000, 3000000,
}; /* uA */

static int pn_data[] = {
	[PN_BQ25890] = 0x03,
	[PN_BQ25892] = 0x00,
	[PN_BQ25895] = 0x07,
	[PN_SYV6970QCC] = 0X01,
	[PN_SC89890H] = 0x04,
};

static const struct charger_properties bq2589x_chg_props = {
	.alias_name = "bq2589x",
};

EXPORT_SYMBOL(thub_plug_flag);
extern void Charger_Detect_Init(void);
extern void Charger_Detect_Release(void);
static void bq2589x_dump_regs(struct bq2589x *bq);
static int bq2589x_get_vbus(struct charger_device *chgdev, u32 *vbus);

static int __bq2589x_read_reg(struct bq2589x *bq, u8 reg, u8 *data)
{
	s32 ret;

	ret = i2c_smbus_read_byte_data(bq->client, reg);
	if (ret < 0) {
		msleep(500);
		ret = i2c_smbus_read_byte_data(bq->client, reg);
		if (ret < 0) {
			pr_info("i2c read fail: can't read from reg 0x%02X\n", reg);
			return ret;
		}
		*data = (u8)ret;

		return 0;
	}
	*data = (u8)ret;

	return 0;
}

static int __bq2589x_write_reg(struct bq2589x *bq, int reg, u8 val)
{
	s32 ret;

	ret = i2c_smbus_write_byte_data(bq->client, reg, val);
	if (ret < 0) {
		pr_info("i2c write fail: can't write 0x%02X to reg 0x%02X: %d\n",
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
		pr_info("Failed: reg=%02X, ret=%d\n", reg, ret);

	return ret;
}

static int bq2589x_update_bits(struct bq2589x *bq, u8 reg, u8 mask, u8 data)
{
	int ret;
	u8 tmp;

	mutex_lock(&bq->i2c_rw_lock);
	ret = __bq2589x_read_reg(bq, reg, &tmp);
	if (ret) {
		pr_info("Failed: reg=%02X, ret=%d\n", reg, ret);
		goto out;
	}

	tmp &= ~mask;
	tmp |= data & mask;

	ret = __bq2589x_write_reg(bq, reg, tmp);
	if (ret)
		pr_info("Failed: reg=%02X, ret=%d\n", reg, ret);

out:
	mutex_unlock(&bq->i2c_rw_lock);
	return ret;
}

static int bq2589x_disable_12V(struct bq2589x *bq)
{
	u8 val;
	int ret;
	val = BQ2589X_ENABLE_9V;
	val <<= BQ2589X_HV_TYPE_SHIFT;

	ret = bq2589x_update_bits(bq, BQ2589X_REG_02,
				BQ2589X_HV_TYPE_MASK, val);

	return ret;
}

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

static int bq2589x_disable_hvdcp(struct bq2589x *bq)
{
	int ret;
	u8 val = BQ2589X_HVDCP_DISABLE << BQ2589X_HVDCPEN_SHIFT;

	ret = bq2589x_update_bits(bq, BQ2589X_REG_02,
				BQ2589X_HVDCPEN_MASK, val);
	return ret;
}

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

int bq2589x_adc_start(struct bq2589x *bq, bool oneshot)
{
	u8 val;
	int ret;

	ret = bq2589x_read_byte(bq, BQ2589X_REG_02, &val);
	if (ret < 0) {
		dev_err(bq->dev, "%s failed to read register 0x02:%d\n", __func__, ret);
		return ret;
	}

	if (((val & BQ2589X_CONV_RATE_MASK) >> BQ2589X_CONV_RATE_SHIFT)
		== BQ2589X_ADC_CONTINUE_ENABLE)

		return 0; /*is doing continuous scan*/
	if (oneshot)
		ret = bq2589x_update_bits(bq, BQ2589X_REG_02, BQ2589X_CONV_START_MASK,
					BQ2589X_CONV_START << BQ2589X_CONV_START_SHIFT);
	else
		ret = bq2589x_update_bits(bq, BQ2589X_REG_02, BQ2589X_CONV_RATE_MASK,
					BQ2589X_ADC_CONTINUE_ENABLE << BQ2589X_CONV_RATE_SHIFT);
	return ret;
}

int bq2589x_adc_stop(struct bq2589x *bq)
{
	return bq2589x_update_bits(bq, BQ2589X_REG_02, BQ2589X_CONV_RATE_MASK,
				BQ2589X_ADC_CONTINUE_DISABLE << BQ2589X_CONV_RATE_SHIFT);
}

int bq2589x_adc_read_battery_volt(struct bq2589x *bq)
{
	uint8_t val;
	int volt;
	int ret;

	ret = bq2589x_read_byte(bq, BQ2589X_REG_0E, &val);
	if (ret < 0) {
		dev_err(bq->dev, "read battery voltage failed :%d\n", ret);

		return ret;
	} else {
		volt = BQ2589X_BATV_BASE + (((val & BQ2589X_BATV_MASK) >> BQ2589X_BATV_SHIFT)
			* BQ2589X_BATV_LSB);

		return volt;
	}
}

int bq2589x_adc_read_sys_volt(struct bq2589x *bq)
{
	uint8_t val;
	int volt;
	int ret;

	ret = bq2589x_read_byte(bq,  BQ2589X_REG_0F, &val);
	if (ret < 0) {
		dev_err(bq->dev, "read system voltage failed :%d\n", ret);

		return ret;
	} else {
		volt = BQ2589X_SYSV_BASE + (((val & BQ2589X_SYSV_MASK) >> BQ2589X_SYSV_SHIFT)
			* BQ2589X_SYSV_LSB);

		return volt;
	}
}

int bq2589x_is_otg_enable(struct bq2589x *bq)
{
	uint8_t val;
	int otg_enable;
	int ret;

	ret = bq2589x_read_byte(bq,  BQ2589X_REG_03, &val);
	if (ret < 0) {
		dev_err(bq->dev, "read otg_enable failed :%d\n", ret);

		return ret;
	} else {
		otg_enable = !!(val & BQ2589X_OTG_CONFIG_MASK);

		return otg_enable;
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
	} else {
		volt = BQ2589X_VBUSV_BASE + (((val & BQ2589X_VBUSV_MASK) >> BQ2589X_VBUSV_SHIFT)
			* BQ2589X_VBUSV_LSB);
		if (volt == BQ2589X_VBUSV_BASE)
			volt = 0;
		*vol = volt * 1000;
	}

	return ret;
}

int bq2589x_adc_read_temperature(struct bq2589x *bq)
{
	uint8_t val;
	int temp;
	int ret;

	ret = bq2589x_read_byte(bq, BQ2589X_REG_10, &val);
	if (ret < 0) {
		dev_err(bq->dev, "read temperature failed :%d\n", ret);

		return ret;
	} else {
		temp = BQ2589X_TSPCT_BASE + (((val & BQ2589X_TSPCT_MASK) >> BQ2589X_TSPCT_SHIFT)
			* BQ2589X_TSPCT_LSB);

		return temp;
	}
}

int bq2589x_adc_read_charge_current(struct bq2589x *bq, u32 *cur)
{
	uint8_t val;
	int curr;
	int ret;

	ret = bq2589x_read_byte(bq, BQ2589X_REG_12, &val);
	if (ret < 0) {
		dev_err(bq->dev, "read charge current failed :%d\n", ret);
	} else {
		curr = (int)(BQ2589X_ICHGR_BASE +
			((val & BQ2589X_ICHGR_MASK) >> BQ2589X_ICHGR_SHIFT)
			* BQ2589X_ICHGR_LSB) ;
		*cur = curr * 1000;
	}

	return ret;
}

int bq2589x_set_chargecurrent(struct bq2589x *bq, int curr)
{
	u8 ichg;

	if (curr < BQ2589X_ICHG_BASE)
		curr = BQ2589X_ICHG_BASE;

	if (bq->part_no == pn_data[PN_SC89890H]) {
		ichg = (curr - SC89890H_ICHG_BASE) / SC89890H_ICHG_LSB;
	} else {
		ichg = (curr - BQ2589X_ICHG_BASE) / BQ2589X_ICHG_LSB;
	}

	return bq2589x_update_bits(bq, BQ2589X_REG_04,
						BQ2589X_ICHG_MASK, ichg << BQ2589X_ICHG_SHIFT);
}

int bq2589x_set_term_current(struct bq2589x *bq, int curr)
{
	u8 iterm;

	if (curr < BQ2589X_ITERM_BASE)
		curr = BQ2589X_ITERM_BASE;

	if (bq->part_no == pn_data[PN_SC89890H]) {
		iterm = (curr - SC89890H_ITERM_BASE) / SC89890H_ITERM_LSB;
	} else {
		iterm = (curr - BQ2589X_ITERM_BASE) / BQ2589X_ITERM_LSB;
	}

	return bq2589x_update_bits(bq, BQ2589X_REG_05,
						BQ2589X_ITERM_MASK, iterm << BQ2589X_ITERM_SHIFT);
}

int bq2589x_get_term_current(struct bq2589x *bq, int *curr)
{
	u8 reg_val;
	int iterm;
	int ret;

	ret = bq2589x_read_byte(bq, BQ2589X_REG_05, &reg_val);
	if (!ret) {
		iterm = (reg_val & BQ2589X_ITERM_MASK) >> BQ2589X_ITERM_SHIFT;
		if (bq->part_no == pn_data[PN_SC89890H]) {
			iterm = iterm * SC89890H_ITERM_LSB + SC89890H_ITERM_BASE;
		} else {
			iterm = iterm * BQ2589X_ITERM_LSB + BQ2589X_ITERM_BASE;
		}
		*curr = iterm * 1000;
	}
	return ret;
}

int bq2589x_set_prechg_current(struct bq2589x *bq, int curr)
{
	u8 iprechg;

	if (curr < BQ2589X_IPRECHG_BASE)
		curr = BQ2589X_IPRECHG_BASE;

	if (bq->part_no == pn_data[PN_SC89890H]) {
		iprechg = (curr - SC89890H_IPRECHG_BASE) / SC89890H_IPRECHG_LSB;
	} else {
		iprechg = (curr - BQ2589X_IPRECHG_BASE) / BQ2589X_IPRECHG_LSB;
	}

	return bq2589x_update_bits(bq, BQ2589X_REG_05,
						BQ2589X_IPRECHG_MASK, iprechg << BQ2589X_IPRECHG_SHIFT);

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

int bq2589x_get_chargevol(struct bq2589x *bq, int *volt)
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

int bq2589x_set_input_icoen(struct bq2589x *bq)
{
	u8 val;

	val = BQ2589X_ICO_DISABLE << BQ2589X_ICOEN_SHIFT;

	return bq2589x_update_bits(bq, BQ2589X_REG_02, BQ2589X_ICOEN_MASK, val);
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

int bq2589x_get_input_current_limit(struct	bq2589x *sc, u32 *curr)
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

int bq2589x_set_watchdog_timer(struct bq2589x *bq, u8 timeout)
{
	u8 val;

	val = (timeout - BQ2589X_WDT_BASE) / BQ2589X_WDT_LSB;
	val <<= BQ2589X_WDT_SHIFT;

	return bq2589x_update_bits(bq, BQ2589X_REG_07,
						BQ2589X_WDT_MASK, val);
}

int bq2589x_disable_watchdog_timer(struct bq2589x *bq)
{
	u8 val = BQ2589X_WDT_DISABLE << BQ2589X_WDT_SHIFT;

	return bq2589x_update_bits(bq, BQ2589X_REG_07,
						BQ2589X_WDT_MASK, val);
}

int bq2589x_reset_watchdog_timer(struct bq2589x *bq)
{
	u8 val = BQ2589X_WDT_RESET << BQ2589X_WDT_RESET_SHIFT;

	return bq2589x_update_bits(bq, BQ2589X_REG_03,
						BQ2589X_WDT_RESET_MASK, val);
}

int bq2589x_force_dpdm(struct bq2589x *bq)
{
	int ret;
	int vbus_stat = 0;
	u8 reg_val;
	u8 val = BQ2589X_FORCE_DPDM << BQ2589X_FORCE_DPDM_SHIFT;

	ret = bq2589x_read_byte(bq, BQ2589X_REG_0B, &reg_val);
	if (ret)
		return ret;

	vbus_stat = (reg_val & BQ2589X_VBUS_STAT_MASK);
	vbus_stat >>= BQ2589X_VBUS_STAT_SHIFT;
		if (bq->part_no == pn_data[PN_SC89890H]) {
		if (vbus_stat == BQ2589X_VBUS_TYPE_HVDCP) {
			bq2589x_write_byte(bq, BQ2589X_REG_01, SC89890H_FORCEDPDM1);
			msleep(30);
			bq2589x_write_byte(bq, BQ2589X_REG_01, SC89890H_FORCEDPDM2);
			msleep(30);
			pr_info("it is hvdcp ,force dp dm\n");
		}
	 }

	ret = bq2589x_update_bits(bq, BQ2589X_REG_02,
						BQ2589X_FORCE_DPDM_MASK, val);

	pr_info("Force DPDM %s\n", !ret ?
"successfully" : "failed");

	return ret;

}
int bq2589x_rerun_bc12(struct charger_device *chg_dev)
{
	int ret;
	int vbus_stat = 0;
	u8 reg_val;
	struct bq2589x *bq = dev_get_drvdata(&chg_dev->dev);

	u8 val = BQ2589X_FORCE_DPDM << BQ2589X_FORCE_DPDM_SHIFT;

	ret = bq2589x_read_byte(bq, BQ2589X_REG_0B, &reg_val);
	if (ret)
		return ret;

	vbus_stat = (reg_val & BQ2589X_VBUS_STAT_MASK);
	vbus_stat >>= BQ2589X_VBUS_STAT_SHIFT;
		if (bq->part_no == pn_data[PN_SC89890H]) {
		if (vbus_stat == BQ2589X_VBUS_TYPE_HVDCP) {
			bq2589x_write_byte(bq, BQ2589X_REG_01, SC89890H_FORCEDPDM1);
			msleep(30);
			bq2589x_write_byte(bq, BQ2589X_REG_01, SC89890H_FORCEDPDM2);
			msleep(30);
			pr_info("it is hvdcp ,force dp dm\n");
		}
	 }

	ret = bq2589x_update_bits(bq, BQ2589X_REG_02,
						BQ2589X_FORCE_DPDM_MASK, val);

	pr_info("Force DPDM %s\n", !ret ? "successfully" : "failed");
	msleep(1000);
	ret = bq2589x_get_usb_type(bq, &bq->psy_usb_type);
	if (bq->psy_desc.type == POWER_SUPPLY_TYPE_USB_DCP)
		power_supply_changed(bq->psy);

	return ret;
}

#if 0
int bq2589x_enable_dpdm(struct charger_device *chg_dev, bool en)
{
	int ret;
	u8 val;
	struct bq2589x *bq = dev_get_drvdata(&chg_dev->dev);
	if (en)
		val = BQ2589X_AUTO_DPDM_ENABLE << BQ2589X_AUTO_DPDM_EN_SHIFT;
	else
		val = BQ2589X_AUTO_DPDM_ENABLE << BQ2589X_AUTO_DPDM_EN_SHIFT;

	ret = bq2589x_update_bits(bq, BQ2589X_REG_02,
						BQ2589X_AUTO_DPDM_EN_MASK, val);

	pr_info("bq2589x_enable_or_disable_dpdm %s,enable=%d\n", !ret ?
	"successfully" : "failed", en);

	return ret;

}
#endif

int bq2589x_reset_chip(struct bq2589x *bq)
{
	int ret;
	u8 val = BQ2589X_RESET << BQ2589X_RESET_SHIFT;

	ret = bq2589x_update_bits(bq, BQ2589X_REG_14,
						BQ2589X_RESET_MASK, val);
	return ret;
}

int bq2589x_enable_hiz_mode(struct bq2589x *bq, bool en)
{
	u8 val;

	if (en) {
		val = BQ2589X_HIZ_ENABLE << BQ2589X_ENHIZ_SHIFT;
		pr_info("bq2589x hiz enable\n");
	} else {
		val = BQ2589X_HIZ_DISABLE << BQ2589X_ENHIZ_SHIFT;
		pr_info("bq2589x hiz disable\n");
	}

	return bq2589x_update_bits(bq, BQ2589X_REG_00,
						BQ2589X_ENHIZ_MASK, val);

}

int bq2589x_exit_hiz_mode(struct bq2589x *bq)
{

	u8 val = BQ2589X_HIZ_DISABLE << BQ2589X_ENHIZ_SHIFT;

	return bq2589x_update_bits(bq, BQ2589X_REG_00,
						BQ2589X_ENHIZ_MASK, val);

}

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

#if 0
int bq2589x_is_enable_hiz(struct charger_device *chg_dev, bool *en)
{
	u8 state = 0;
	struct bq2589x *bq = dev_get_drvdata(&chg_dev->dev);
	bq2589x_get_hiz_mode(bq, &state);
	*en = (bool)state;
	pr_info("bq2589x_is_enable_hiz,en = %d\n", en);

	return 0;
}
#endif
#if 0
int bq2589x_get_hub(struct charger_device *chgdev)
{
	pr_info("[%s]: thub_plug_flag = %d\n", __func__, thub_plug_flag);
	return thub_plug_flag;

}
EXPORT_SYMBOL(bq2589x_get_hub);
#endif

int bq2589x_close_adc(struct charger_device *chg_dev)
{
	struct bq2589x *bq = dev_get_drvdata(&chg_dev->dev);
	bq2589x_adc_stop(bq);
	return 0;
}

#if 0
int bq2589x_set_bootmode(struct charger_device *chg_dev, int bootmode)
{
	pr_info("bq2589x_set_bootmode bootmode = %d", bootmode);
	boot_mode = bootmode;
	return 0;
}
#endif

#if 0
int bq2589x_get_float_type_flag(struct charger_device *chg_dev)
{
	struct bq2589x *bq = dev_get_drvdata(&chg_dev->dev);

	return bq->float_type_flag;
}
#endif

#if 0
int bq2589x_get_pogo(struct charger_device *chg_dev)
{
	int pogo_type = 0;
	pogo_type = get_pogo_in_status();
	return pogo_type;
}
#endif

static int bq2589x_enable_term(struct bq2589x *bq, bool enable)
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

int bq2589x_set_boost_current(struct bq2589x *bq, int curr)
{
	u8 temp;

	if (curr == 500)
		temp = BQ2589X_BOOST_LIM_500MA;
	else if (curr == 750)
		temp = BQ2589X_BOOST_LIM_700MA; //real:750ma
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
		temp = BQ2589X_BOOST_LIM_1300MA; //default
	pr_info("jojo:set boot limit temp = %d\n", temp);

	return bq2589x_update_bits(bq, BQ2589X_REG_0A,
				BQ2589X_BOOST_LIM_MASK,
				temp << BQ2589X_BOOST_LIM_SHIFT);
}

int bq2589x_set_boost_voltage(struct bq2589x *bq, int volt)
{
	u8 val = 0;

	if (bq->part_no == pn_data[PN_SC89890H]) {
		if (volt < SC89890H_BOOSTV_BASE)
			volt = SC89890H_BOOSTV_BASE;
		if (volt > SC89890H_BOOSTV_BASE
				+ (BQ2589X_BOOSTV_MASK >> BQ2589X_BOOSTV_SHIFT)
				* SC89890H_BOOSTV_LSB)
			volt = SC89890H_BOOSTV_BASE
				+ (BQ2589X_BOOSTV_MASK >> BQ2589X_BOOSTV_SHIFT)
				* SC89890H_BOOSTV_LSB;

		val = ((volt - SC89890H_BOOSTV_BASE) / SC89890H_BOOSTV_LSB)
				<< BQ2589X_BOOSTV_SHIFT;
	} else {
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
	}

	return bq2589x_update_bits(bq, BQ2589X_REG_0A,
				BQ2589X_BOOSTV_MASK, val);
}

static int bq2589x_set_safety_timer_timeout(struct bq2589x *bq, int time)
{
	u8 val = BQ2589X_CHG_TIMER_12HOURS;

	switch (time) {
	case 5:
		val = BQ2589X_CHG_TIMER_5HOURS << BQ2589X_CHG_TIMER_SHIFT;
		break;
	case 8:
		val = BQ2589X_CHG_TIMER_8HOURS << BQ2589X_CHG_TIMER_SHIFT;
		break;
	case 12:
		val = BQ2589X_CHG_TIMER_12HOURS << BQ2589X_CHG_TIMER_SHIFT;
		break;
	case 20:
		val = BQ2589X_CHG_TIMER_20HOURS << BQ2589X_CHG_TIMER_SHIFT;
		break;
	default:
		val = BQ2589X_CHG_TIMER_12HOURS << BQ2589X_CHG_TIMER_SHIFT;
		break;
	}

	return bq2589x_update_bits(bq, BQ2589X_REG_07,
		BQ2589X_CHG_TIMER_MASK, val);
}

static int bq2589x_enable_safety_timer(struct bq2589x *bq)
{
	const u8 val = BQ2589X_CHG_TIMER_ENABLE << BQ2589X_EN_TIMER_SHIFT;

	return bq2589x_update_bits(bq, BQ2589X_REG_07,
		BQ2589X_EN_TIMER_MASK, val);
}

static int bq2589x_disable_safety_timer(struct bq2589x *bq)
{
	const u8 val = BQ2589X_CHG_TIMER_DISABLE << BQ2589X_EN_TIMER_SHIFT;

	return bq2589x_update_bits(bq, BQ2589X_REG_07,
		BQ2589X_EN_TIMER_MASK, val);
}

static struct bq2589x_platform_data *bq2589x_parse_dt(struct device_node *np,
							  struct bq2589x *bq)
{
	int ret;
	struct bq2589x_platform_data *pdata;

	pdata = devm_kzalloc(bq->dev, sizeof(struct bq2589x_platform_data), GFP_KERNEL);
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
		pr_info("Failed to read node of ti,bq2589x,usb-vlim\n");
	}

	ret = of_property_read_u32(np, "ti,bq2589x,usb-ilim", &pdata->usb.ilim);
	if (ret) {
		pdata->usb.ilim = 2000;
		pr_info("Failed to read node of ti,bq2589x,usb-ilim\n");
	}

	ret = of_property_read_u32(np, "ti,bq2589x,usb-vreg", &pdata->usb.vreg);
	if (ret) {
		pdata->usb.vreg = 4200;
		pr_info("Failed to read node of ti,bq2589x,usb-vreg\n");
	}

	ret = of_property_read_u32(np, "ti,bq2589x,usb-ichg", &pdata->usb.ichg);
	if (ret) {
		pdata->usb.ichg = 2000;
		pr_info("Failed to read node of ti,bq2589x,usb-ichg\n");
	}

	ret = of_property_read_u32(np, "ti,bq2589x,precharge-current",
				   &pdata->iprechg);
	if (ret) {
		pdata->iprechg = 180;
		pr_info("Failed to read node of ti,bq2589x,precharge-current\n");
	}

	ret = of_property_read_u32(np, "ti,bq2589x,termination-current",
				   &pdata->iterm);
	if (ret) {
		pdata->iterm = 270;
		pr_info
			("Failed to read node of ti,bq2589x,termination-current\n");
	}

	ret =
		of_property_read_u32(np, "ti,bq2589x,boost-voltage",
				 &pdata->boostv);
	if (ret) {
		pdata->boostv = 5000;
		pr_info("Failed to read node of ti,bq2589x,boost-voltage\n");
	}

	ret =
		of_property_read_u32(np, "ti,bq2589x,boost-current",
				 &pdata->boosti);
	if (ret) {
		pdata->boosti = 750;
		pr_info("Failed to read node of ti,bq2589x,boost-current\n");
	}
	bq->otg_enable_pin = of_get_named_gpio(np, "ti,otg-en-gpio", 0);
	if (bq->otg_enable_pin < 0)
		pr_info("of get ti,otg-en-gpio failed, ti,otg-en-gpio:%d\n", bq->otg_enable_pin);

	if (gpio_request(bq->otg_enable_pin, "otg_enable_pin")) {
		pr_info("reset otg_enable_pin failed\n");
	}
	if (gpio_direction_output(bq->otg_enable_pin, 0))
		pr_info("otg_enable_pin gpio_direction_output reset_pin failed\n");

	bq->otg_en2_pin = of_get_named_gpio(np, "ti,otg-en2-gpio", 0);
	if (bq->otg_en2_pin < 0)
		pr_info("of get otg_en2_pin failed, otg_en2_pin:%d\n", bq->otg_en2_pin);

	if (gpio_request(bq->otg_en2_pin, "otg_en2_pin")) {
		pr_info("reset otg_enable_pin failed\n");
	}
	if (gpio_direction_output(bq->otg_en2_pin, 0))
		pr_info("otg_en2_pin gpio_direction_output reset_pin failed\n");

	bq->otg_sgm6111_pin = of_get_named_gpio(np, "ti,otg-sgm6111-gpio", 0);
	if (bq->otg_sgm6111_pin < 0)
		pr_info("of get otg_en2_pin failed, otg_sgm6111_pin:%d\n", bq->otg_sgm6111_pin);

	if (gpio_request(bq->otg_sgm6111_pin, "otg_sgm6111_pin")) {
		pr_info("reset otg_enable_pin failed\n");
	}
	if (gpio_direction_output(bq->otg_sgm6111_pin, 0))
		pr_info("otg_sgm6111_pin gpio_direction_output reset_pin failed\n");

	bq->otg_ocflag_pin = of_get_named_gpio(np, "ti,otg-ocflag-gpio", 0);
	if (bq->otg_ocflag_pin < 0)
		pr_info("of get otg_ocflag_pin failed, otg_ocflag_pin:%d\n", bq->otg_ocflag_pin);

	if (gpio_request(bq->otg_ocflag_pin, "otg_ocflag_pin")) {
		pr_info("reset otg_ocflag_pin failed\n");
	}
	if (gpio_direction_output(bq->otg_ocflag_pin, 0))
		pr_info("otg_ocflag_pin gpio_direction_output reset_pin failed\n");

	return pdata;
}

static int bq2589x_get_charge_stat(struct bq2589x *bq, int *state)
{
		int ret;
	u8 val;
	u32 vbus;

	bq2589x_get_vbus(bq->chg_dev, &vbus);
	ret = bq2589x_read_byte(bq, BQ2589X_REG_0B, &val);
	if (!ret) {
		if ((val & BQ2589X_VBUS_STAT_MASK) >> BQ2589X_VBUS_STAT_SHIFT
				== BQ2589X_VBUS_TYPE_OTG) {
			*state = POWER_SUPPLY_STATUS_DISCHARGING;
			return ret;
		}
		val = val & BQ2589X_CHRG_STAT_MASK;
		val = val >> BQ2589X_CHRG_STAT_SHIFT;
		switch (val) {
		case BQ2589X_CHRG_STAT_IDLE:
			if (((bq->power_good) || (vbus > BQ2589X_VBUS_VALID))
				&& (bq->part_no != pn_data[PN_SC89890H]))
				*state = POWER_SUPPLY_STATUS_CHARGING;
			else
				*state = POWER_SUPPLY_STATUS_DISCHARGING;
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
	pr_info("%s, charge stat = %d\n", __func__, *state);

	return ret;
}

#if 0
static int bq2589x_get_state(struct charger_device *chgdev, u32 *state)
{
	struct bq2589x *bq = dev_get_drvdata(&chgdev->dev);

	return bq2589x_get_charge_stat(bq, state);
}
#endif

int bq2589x_get_usb_type(struct bq2589x *bq, int *type)
{
	int ret;

	u8 reg_val = 0;
	int vbus_stat = 0;
	int usb_type = POWER_SUPPLY_TYPE_UNKNOWN;

	ret = bq2589x_read_byte(bq, BQ2589X_REG_0B, &reg_val);
	if (ret)
		return ret;

	vbus_stat = (reg_val & BQ2589X_VBUS_STAT_MASK);
	vbus_stat >>= BQ2589X_VBUS_STAT_SHIFT;

	switch (vbus_stat) {
	case BQ2589X_VBUS_TYPE_NONE:
		bq->psy_desc.type = POWER_SUPPLY_TYPE_UNKNOWN;
		usb_type = POWER_SUPPLY_USB_TYPE_UNKNOWN;
		pr_info("BQ2589X charger type: UNKNOW\n");
		break;
	case BQ2589X_VBUS_TYPE_SDP:
		bq->psy_desc.type = POWER_SUPPLY_TYPE_USB;
		usb_type = POWER_SUPPLY_USB_TYPE_SDP;
		pr_info("BQ2589X charger type: SDP\n");
		if (prev_pg)
			Charger_Detect_Release();
		break;
	case BQ2589X_VBUS_TYPE_CDP:
		bq->psy_desc.type = POWER_SUPPLY_TYPE_USB_CDP;
		usb_type = POWER_SUPPLY_USB_TYPE_CDP;
		pr_info("BQ2589X charger type: CDP\n");
		if (prev_pg)
			Charger_Detect_Release();
		break;
	case BQ2589X_VBUS_TYPE_DCP:
		bq->psy_desc.type = POWER_SUPPLY_TYPE_USB_DCP;
		usb_type = POWER_SUPPLY_USB_TYPE_DCP;
		pr_info("BQ2589X charger type: DCP\n");
		break;
	case BQ2589X_VBUS_TYPE_HVDCP:
		bq->psy_desc.type = POWER_SUPPLY_TYPE_USB_PD; //hvdcp
		usb_type = POWER_SUPPLY_USB_TYPE_PD;  //hvdcp
		pr_info("BQ2589X charger type: HVDCP\n");
	break;
	case BQ2589X_VBUS_TYPE_UNKNOWN:
		bq->psy_desc.type = POWER_SUPPLY_TYPE_USB;//float
		usb_type = POWER_SUPPLY_USB_TYPE_SDP;
		pr_info("BQ2589X charger type: UNKNOW\n");
		if (prev_pg)
			Charger_Detect_Release();
		break;
	case BQ2589X_VBUS_TYPE_NON_STD:
		bq->psy_desc.type = POWER_SUPPLY_TYPE_USB_DCP;// non_stand set by usb
		usb_type = POWER_SUPPLY_USB_TYPE_DCP;
		pr_info("BQ2589X charger type: NON_STANDARD\n");
		break;
	default:
		bq->psy_desc.type = POWER_SUPPLY_TYPE_UNKNOWN;
		usb_type = POWER_SUPPLY_USB_TYPE_UNKNOWN;
		pr_info("BQ2589X charger type: UNKNOW\n");
		break;
	}
#if 0

	if (pogo_type && bq->power_good) {
		bq->psy_desc.type = POWER_SUPPLY_TYPE_USB_PD; //hvdcp
		usb_type = POWER_SUPPLY_USB_TYPE_PD;  //hvdcp
	}


	if (thub_plug_flag) {
		bq->psy_desc.type = POWER_SUPPLY_TYPE_USB_PD; //hvdcp
		usb_type = POWER_SUPPLY_USB_TYPE_PD;  //hvdcp
	}

	if (vbus_stat == BQ2589X_VBUS_TYPE_UNKNOWN && !thub_plug_flag && !pogo_type) {
		bq->float_type_flag = true;
	} else {
		bq->float_type_flag = false;
	}
#endif
	pr_info("bq2589x is bq->psy_desc.type:%d, usb_type:%d,vbus stat:%d\n",
		bq->psy_desc.type, usb_type, vbus_stat);
	*type = usb_type;

	return 0;
}
EXPORT_SYMBOL(bq2589x_get_usb_type);


static void bq2589x_read_byte_work(struct work_struct *work)
{
	int ret;
	u8 reg_val;
	int vbus_stat = 0;
	u8 reg_type;
	u32 vbus = 0;

	struct bq2589x *bq = container_of(work, struct bq2589x, read_byte_work.work);

	prev_pg = bq->power_good;
	if (bq->part_no == pn_data[PN_SC89890H]) {
		ret = bq2589x_read_byte(bq, BQ2589X_REG_11, &reg_val);
		if (ret)
			return;
		bq->power_good = !!(reg_val & BQ2589X_VBUS_GD_MASK);
		pr_info("%s 0x11 = %X, bq->power_good = %d, prev_pg = %d\n",
			__func__, reg_val, bq->power_good, prev_pg);
	} else {
		ret = bq2589x_read_byte(bq, BQ2589X_REG_0B, &reg_val);
		if (ret)
			return;
		bq->power_good = !!(reg_val & BQ2589X_PG_STAT_MASK);
		pr_info("%s 0x0B = %X, bq->power_good = %d, prev_pg = %d\n",
			__func__, reg_val, bq->power_good, prev_pg);
	}


	if (bq2589x_adc_read_vbus_volt(bq, &vbus)) {
		vbus = 0;
		pr_info("%s get vbus fail\n", __func__);
	}

	if (!(bq->power_good) && (vbus > BQ2589X_VBUS_VALID)
		&& (bq->part_no != pn_data[PN_SC89890H]))
		bq->power_good = 1;

	if (!prev_pg && bq->power_good)
		bq2589x_set_input_current_limit(bq, 400);
#if 0
	/*Tulip code for TULIP-137 by wangtao53 at 20220531 start*/
	if (bq->part_no == pn_data[PN_SC89890H]) {
		ret = bq2589x_get_usb_type(bq, &bq->psy_usb_type);
		if (bq->psy_desc.type == POWER_SUPPLY_TYPE_USB || bq->psy_desc.type == POWER_SUPPLY_TYPE_USB_CDP)
			Charger_Detect_Release();
	}
	/*Tulip code for TULIP-137 by wangtao53 at 20220531 end*/
#endif
	ret = bq2589x_read_byte(bq, BQ2589X_REG_0B, &reg_type);
	if (ret)
		return;
	vbus_stat = (reg_type & BQ2589X_VBUS_STAT_MASK);
	vbus_stat >>= BQ2589X_VBUS_STAT_SHIFT;

	pr_info("vbus_stat = %d,BQ2589X_VBUS_TYPE_DCP =%d\n",
		vbus_stat, BQ2589X_VBUS_TYPE_DCP);
	if (bq->part_no == pn_data[PN_SC89890H]) {
		if (!prev_pg && bq->power_good) {
			pr_info("adapter/usb inserted\n");
			Charger_Detect_Init();
			if (vbus_stat != BQ2589X_VBUS_TYPE_DCP) {
				ret = bq2589x_force_dpdm(bq);
				msleep(400);
			}
		} else if (prev_pg && !bq->power_good) {
			pr_info("adapter/usb removed\n");
			bq->psy_desc.type = POWER_SUPPLY_TYPE_UNKNOWN;
		}
	} else {//for bq25890h
		if (!prev_pg && bq->power_good) {
			pr_info("adapter/usb inserted\n");
			Charger_Detect_Init();
			if (vbus_stat != BQ2589X_VBUS_TYPE_DCP) {
				ret = bq2589x_force_dpdm(bq);
				msleep(400);
			}
	} else if (prev_pg && !bq->power_good) {
			pr_info("adapter/usb removed\n");
			bq->psy_desc.type = POWER_SUPPLY_TYPE_UNKNOWN;
		}
	}

	bq2589x_dump_regs(bq);
	ret = bq2589x_get_usb_type(bq, &bq->psy_usb_type);
	power_supply_changed(bq->psy);
	pr_info("%s, power_good(%d), prev_pg(%d), vbus(%d), type(%d)\n",
		__func__, bq->power_good, prev_pg, vbus, bq->psy_usb_type);

	return;
}

static irqreturn_t bq2589x_irq_handler_thread(int irq, void *data)
{
	struct bq2589x *bq = data;

	schedule_delayed_work(&bq->read_byte_work, msecs_to_jiffies(40));

	return IRQ_HANDLED;
}

static int bq2589x_register_interrupt(struct bq2589x *bq)
{
	int ret = 0;

	ret = devm_request_threaded_irq(bq->dev, bq->client->irq, NULL,
					bq2589x_irq_handler_thread,
					IRQF_TRIGGER_FALLING | IRQF_NO_SUSPEND |
					IRQF_ONESHOT, "chr_stat", bq);
	if (ret < 0) {
		pr_info("request thread irq failed:%d\n", ret);
		return ret;
	} else {
		pr_info("request thread irq pass:%d  bq->client->irq =%d\n",
			ret, bq->client->irq);
	}

	enable_irq_wake(bq->irq);

	return 0;
}

static int bq2589x_init_device(struct bq2589x *bq)
{
	int ret;

	update = false;
	force_discharging_flag = false;
	pd_type = false;

	bq2589x_disable_watchdog_timer(bq);

	if (bq->part_no == pn_data[PN_SC89890H]) {
		bq2589x_update_bits(bq, BQ2589X_REG_00, BQ2589X_ENILIM_MASK,
				BQ2589X_ENILIM_DISABLE << BQ2589X_ENILIM_SHIFT);
	} else {
		ret = bq2589x_disable_12V(bq);
		if (ret)
			pr_info("Failed to disable 12V,open 9V, ret = %d\n", ret);
	}
	if (bq->part_no == pn_data[PN_SC89890H]) {
		ret = bq2589x_disable_hvdcp(bq);
	} else {
		ret = bq2589x_disable_hvdcp(bq);
	}
	bq2589x_adc_stop(bq);
	bq2589x_dump_regs(bq);
	ret = bq2589x_set_prechg_current(bq, bq->platform_data->iprechg);
	if (ret)
		pr_info("Failed to set prechg current, ret = %d\n", ret);

	ret = bq2589x_set_term_current(bq, bq->platform_data->iterm);
	if (ret)
		pr_info("Failed to set termination current, ret = %d\n", ret);

	ret = bq2589x_set_boost_voltage(bq, bq->platform_data->boostv);
	if (ret)
		pr_info("Failed to set boost voltage, ret = %d\n", ret);

	ret = bq2589x_set_boost_current(bq, bq->platform_data->boosti);
	if (ret)
		pr_info("Failed to set boost current, ret = %d\n", ret);
	ret = bq2589x_set_chargecurrent(bq, 300);
	if (ret)
		pr_info("Failed to bq2589x_set_chargecurrent, ret = %d\n", ret);
	ret = bq2589x_set_input_current_limit(bq, 300);
	if (ret)
		pr_info("Failed to bq2589x_set_chargecurrent, ret = %d\n", ret);
	ret = bq2589x_set_input_icoen(bq);
	if (ret)
		pr_info("Failed to bq2589x_set_icoen, ret = %d\n", ret);
	ret = bq2589x_set_safety_timer_timeout(bq, 20);
	if (ret)
		pr_info("Failed to bq2589x_set_safety time timeout, ret = %d\n", ret);

	bq2589x_dump_regs(bq);
	return 0;
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
			pr_debug("Reg[%.2x] = 0x%.2x\n", addr, val);
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

	pr_info("%s charger %s\n", enable ? "enable" : "disable",
		   !ret ? "successfully" : "failed");

	ret = bq2589x_read_byte(bq, BQ2589X_REG_03, &val);

	if (!ret)
		bq->charge_enabled = !!(val & BQ2589X_CHG_CONFIG_MASK);

	return ret;
}

static int bq2589x_do_event(struct charger_device *chgdev, u32 event, u32 args)
{
	struct bq2589x *bq = charger_get_data(chgdev);

	switch (event) {
	case EVENT_FULL:
	case EVENT_RECHARGE:
	case EVENT_DISCHARGE:
		power_supply_changed(bq->psy);
		break;
	default:
		break;
	}
	return 0;
}

static int bq2589x_plug_in(struct charger_device *chg_dev)
{
	struct bq2589x *bq = dev_get_drvdata(&chg_dev->dev);
	int ret;
	cancel_delayed_work_sync(&plug_work);
	ret = bq2589x_adc_start(bq, false);
	if (ret)
		pr_info("Failed to set adc start, ret = %d\n", ret);

	ret = bq2589x_charging(chg_dev, true);
	pr_info("bq2589x:plug in\n");
	if (ret)
		pr_info("Failed to enable charging:%d\n", ret);

	if (!bq->bat_psy) {
		bq->bat_psy = power_supply_get_by_name("battery");
	}

	if (bq->bat_psy) {
		pr_info("%s power supply change\n", __func__);
		msleep(50);
		power_supply_changed(bq->bat_psy);
	}

	return ret;
}

static void charge_plug_work(struct work_struct *work)
{
	pr_info("bq2589x:plug out charge_plug_work,bq_ex->power_good = %d\n",
		bq_ex->power_good);
	if (!bq_ex->power_good)
		bq2589x_adc_stop(bq_ex);
	bq2589x_dump_regs(bq_ex);

	return;
}

static int bq2589x_plug_out(struct charger_device *chg_dev)
{
	struct bq2589x *bq = dev_get_drvdata(&chg_dev->dev);
	int ret;

	ret = bq2589x_charging(chg_dev, false);
	pr_info("bq2589x:plug out\n");
	if (ret)
		pr_info("Failed to disable charging:%d\n", ret);

	if (!bq->bat_psy) {
		bq->bat_psy = power_supply_get_by_name("battery");
	}

	if (bq->bat_psy) {
		pr_info("%s power supply change\n", __func__);
		msleep(50);
		power_supply_changed(bq->bat_psy);
	}
	if (boot_mode == 8 || boot_mode == 9)
		pr_info("bq2589x:plug out,do nothing\n");
	else
		schedule_delayed_work(&plug_work, msecs_to_jiffies(5000));

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

	pr_info("charge curr = %d\n", curr);

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
		 if (bq->part_no == pn_data[PN_SC89890H]) {
			ichg = (reg_val & BQ2589X_ICHG_MASK) >> BQ2589X_ICHG_SHIFT;
			ichg = ichg * SC89890H_ICHG_LSB + SC89890H_ICHG_BASE;
			*curr = ichg * 1000;
		 } else {
			ichg = (reg_val & BQ2589X_ICHG_MASK) >> BQ2589X_ICHG_SHIFT;
			ichg = ichg * BQ2589X_ICHG_LSB + BQ2589X_ICHG_BASE;
			*curr = ichg * 1000;
		 }
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

	pr_info("charge volt = %d\n", volt);

	return bq2589x_set_chargevolt(bq, volt / 1000);
}

static int bq2589x_get_vchg(struct charger_device *chg_dev, u32 *volt)
{
	struct bq2589x *bq = dev_get_drvdata(&chg_dev->dev);

	return bq2589x_get_chargevol(bq, volt);
}

static int bq2589x_set_ivl(struct charger_device *chg_dev, u32 volt)
{
	struct bq2589x *bq = dev_get_drvdata(&chg_dev->dev);

	pr_info("vindpm volt = %d\n", volt);

	return bq2589x_set_input_volt_limit(bq, volt / 1000);
}

static int bq2589x_get_ivl(struct charger_device *chgdev, u32 *volt)
{
	struct bq2589x *bq = dev_get_drvdata(&chgdev->dev);

	return bq2589x_get_input_volt_limit(bq, volt);
}

static int bq2589x_set_icl(struct charger_device *chg_dev, u32 curr)
{
	struct bq2589x *bq = dev_get_drvdata(&chg_dev->dev);

	pr_info("indpm curr = %d\n", curr);

	return bq2589x_set_input_current_limit(bq, curr / 1000);
}

static int bq2589x_get_icl(struct charger_device *chg_dev, u32 *curr)
{
	struct bq2589x *bq = dev_get_drvdata(&chg_dev->dev);

	return bq2589x_get_input_current_limit(bq, curr);
}

static int bq2589x_get_vbus(struct charger_device *chgdev, u32 *vbus)
{
	struct bq2589x *bq = dev_get_drvdata(&chgdev->dev);

	return bq2589x_adc_read_vbus_volt(bq, vbus);
}

static int bq2589x_get_ibus(struct charger_device *chgdev, u32 *ibus)
{
	struct bq2589x *bq = dev_get_drvdata(&chgdev->dev);

	return bq2589x_adc_read_charge_current(bq, ibus);
}

static int bq2589x_kick_wdt(struct charger_device *chg_dev)
{
	struct bq2589x *bq = dev_get_drvdata(&chg_dev->dev);

	return bq2589x_reset_watchdog_timer(bq);
}

static int bq2589x_set_ieoc(struct charger_device *chgdev, u32 ieoc)
{
	struct bq2589x *bq = dev_get_drvdata(&chgdev->dev);

	return bq2589x_set_term_current(bq, ieoc / 1000);
}

static int bq2589x_enable_te(struct charger_device *chgdev, bool en)
{
	struct bq2589x *bq = dev_get_drvdata(&chgdev->dev);

	return bq2589x_enable_term(bq, en);
}

int bq2589x_enable_hz(struct charger_device *chgdev, bool en)
{
	struct bq2589x *bq = dev_get_drvdata(&chgdev->dev);

	if (en)
		bq->is_input_suspend = true;
	else
		bq->is_input_suspend = false;
	pr_info("[%s]: en = %d\n", __func__, en);

	return bq2589x_enable_hiz_mode(bq, en);
}
EXPORT_SYMBOL(bq2589x_enable_hz);

int disable_show_lightning(struct charger_device *chgdev, bool en)
{

	int ret = 0;
	struct bq2589x *bq = dev_get_drvdata(&chgdev->dev);
	force_discharging_flag = en;
	pr_info("jojo[%s]:because jeita or battery protect, en = %d,update=%d\n",
		__func__, force_discharging_flag, update);
	if (force_discharging_flag)
		update = true;
	if (!force_discharging_flag && update) {
		power_supply_changed(bq->psy);
		update = false;
	}

	return ret;
}

#if 0
int set_pd_type(struct charger_device *chgdev, bool en)
{
	pd_type = en;
	return 0;
}
#endif
#if 0
/*Tulip code for TULIP-456 by wanglicheng at 20220523 start*/
int bq2589x_enable_ship(struct charger_device *chgdev, bool en)
{
	int ret = 0;
	int ret2 = 0;
	u8 state = 0;
	u8 delay_state = 0;
	u8 batfet_state = 0;
	u8 enable = 0;
	struct bq2589x *bq = dev_get_drvdata(&chgdev->dev);

	if (en) {

		bq2589x_adc_stop(bq);
		bq2589x_update_bits(bq, BQ2589X_REG_09,
			BQ2589X_BATFET_DLY_MASK, (BQ2589X_BATFET_DLY_EN << BQ2589X_BATFET_DLY_SHIFT));

		enable = BQ2589X_BATFET_OFF << BQ2589X_BATFET_DIS_SHIFT;
		ret = bq2589x_update_bits(bq, BQ2589X_REG_09,
			BQ2589X_BATFET_DIS_MASK, enable);
		pr_info("wlc set bq2589x shipmode enable/ batfet off\n");
	}

	ret2 = bq2589x_read_byte(bq, BQ2589X_REG_09, &state);
	if (!ret2) {
		delay_state = (state & BQ2589X_BATFET_DLY_MASK) >> BQ2589X_BATFET_DLY_SHIFT;
		pr_info("wlc %s ship mode 10S delay%s\n", en ? "enable" : "disable", delay_state ? "successfully" : "failed");
		batfet_state = (state & BQ2589X_BATFET_DIS_MASK) >> BQ2589X_BATFET_DIS_SHIFT;
		pr_info("wlc %s ship mode %s\n", en ? "enable" : "disable", batfet_state ? "successfully" : "failed");
	}
	return ret;

}
EXPORT_SYMBOL(bq2589x_enable_ship);
#endif
/*Tulip code for TULIP-456 by wanglicheng at 20220523 end*/

static int bq2589x_set_otg(struct charger_device *chg_dev, bool en)
{
	int ret;
	struct bq2589x *bq = dev_get_drvdata(&chg_dev->dev);

	if (en) {
		if (bq->part_no == pn_data[PN_SC89890H]) {
			ret = bq2589x_disable_charger(bq);
		}
	gpio_set_value(bq_ex->otg_enable_pin, true);
	gpio_set_value(bq_ex->otg_en2_pin, true);
	gpio_set_value(bq_ex->otg_sgm6111_pin, true);
	gpio_set_value(bq_ex->otg_ocflag_pin, true);
	msleep(30);
		ret = bq2589x_enable_otg(bq);
	} else {
	gpio_set_value(bq_ex->otg_enable_pin, false);
	gpio_set_value(bq_ex->otg_en2_pin, false);
	gpio_set_value(bq_ex->otg_sgm6111_pin, false);
	gpio_set_value(bq_ex->otg_ocflag_pin, false);
	msleep(30);
		ret = bq2589x_disable_otg(bq);
		if (bq->part_no == pn_data[PN_SC89890H]) {
			ret = bq2589x_enable_charger(bq);
		}
	}

	pr_info("%s OTG %s\n", en ? "enable" : "disable",
		   !ret ? "successfully" : "failed");

	return ret;
}

int bq2589x_set_enable_otg(bool en)
{
	int ret;

	if (en) {
			if (bq_ex->part_no == pn_data[PN_SC89890H]) {
					ret = bq2589x_disable_charger(bq_ex);
		}
		gpio_set_value(bq_ex->otg_enable_pin, true);
		gpio_set_value(bq_ex->otg_en2_pin, true);
		gpio_set_value(bq_ex->otg_sgm6111_pin, true);
		gpio_set_value(bq_ex->otg_ocflag_pin, true);
		msleep(30);
			ret = bq2589x_enable_otg(bq_ex);
	 }
	else {
		gpio_set_value(bq_ex->otg_enable_pin, false);
		gpio_set_value(bq_ex->otg_en2_pin, false);
		gpio_set_value(bq_ex->otg_sgm6111_pin, false);
		gpio_set_value(bq_ex->otg_ocflag_pin, false);
		msleep(30);
		ret = bq2589x_disable_otg(bq_ex);
		if (bq_ex->part_no == pn_data[PN_SC89890H]) {
					ret = bq2589x_enable_charger(bq_ex);
			}
	 }
	bq2589x_dump_regs(bq_ex);
	pr_info("%s OTG %s\n", en ? "enable" : "disable",
		   !ret ? "successfully" : "failed");

	return ret;
}
EXPORT_SYMBOL_GPL(bq2589x_set_enable_otg);

bool bq2589x_is_usb_type(void)
{
	int ret;
	int vbus_stat = 0;
	u8 reg_type;
	if (prev_pg) {
		ret = bq2589x_read_byte(bq_ex, BQ2589X_REG_0B, &reg_type);
		if (ret)
			return 0;
		vbus_stat = (reg_type & BQ2589X_VBUS_STAT_MASK);
		vbus_stat >>= BQ2589X_VBUS_STAT_SHIFT;
	}
	pr_info("vbus_stat = %d,BQ2589X_VBUS_TYPE_SDP =%d\n",
		vbus_stat, BQ2589X_VBUS_TYPE_SDP);
	if (vbus_stat == BQ2589X_VBUS_TYPE_SDP || vbus_stat == BQ2589X_VBUS_TYPE_CDP)
		return true;
	else
		return false;
}
EXPORT_SYMBOL_GPL(bq2589x_is_usb_type);

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

	pr_info("otg curr = %d\n", curr);

	ret = bq2589x_set_boost_current(bq, curr / 1000);

	return ret;
}

static enum power_supply_usb_type bq2589x_chg_psy_usb_types[] = {
	POWER_SUPPLY_USB_TYPE_UNKNOWN,
	POWER_SUPPLY_USB_TYPE_SDP,
	POWER_SUPPLY_USB_TYPE_CDP,
	POWER_SUPPLY_USB_TYPE_DCP,
};


static enum power_supply_property bq2589x_chg_psy_properties[] = {
	POWER_SUPPLY_PROP_MANUFACTURER,
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT,
	POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE,
	POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT,
	POWER_SUPPLY_PROP_INPUT_VOLTAGE_LIMIT,
	POWER_SUPPLY_PROP_CHARGE_TERM_CURRENT,
	POWER_SUPPLY_PROP_TYPE,
	POWER_SUPPLY_PROP_USB_TYPE,
	POWER_SUPPLY_PROP_CURRENT_MAX,
	POWER_SUPPLY_PROP_VOLTAGE_MAX,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
};

static int bq2589x_chg_property_is_writeable(struct power_supply *psy,
						enum power_supply_property psp)
{
	switch (psp) {
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT:
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE:
	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
	case POWER_SUPPLY_PROP_INPUT_VOLTAGE_LIMIT:
	case POWER_SUPPLY_PROP_CHARGE_TERM_CURRENT:
	case POWER_SUPPLY_PROP_STATUS:
	case POWER_SUPPLY_PROP_ONLINE:
	case POWER_SUPPLY_PROP_ENERGY_EMPTY:
		return 1;
	default:
		return 0;
	}
	return 0;
}

static int bq2589x_chg_get_property(struct power_supply *psy,
				   enum power_supply_property psp,
				   union power_supply_propval *val)
{
	int ret = 0, vbus_volt = 0;
	u32 _val;
	u32 data;
	u32 vbus = 0;
	struct bq2589x *bq = power_supply_get_drvdata(psy);

	pr_info("psp=%d\n", psp);
	switch (psp) {
	case POWER_SUPPLY_PROP_MANUFACTURER:
		if (bq->part_no == pn_data[PN_SC89890H])
			val->strval = "SouthChip";
		else
			val->strval = "Silergy";
		break;
	case POWER_SUPPLY_PROP_ONLINE:
		if ((bq->part_no != pn_data[PN_SC89890H])) {
			bq2589x_get_vbus(bq->chg_dev, &vbus);
			if (!bq->power_good && vbus < BQ2589X_VBUS_UVLO)
				val->intval = 0;
			else if (vbus < BQ2589X_VBUS_UVLO)
				val->intval = 0;
			else
				val->intval = 1;
			pr_info("%s usb online(%d),pd(%d), vbus(%d)\n",
				__func__, val->intval, bq->power_good, vbus);
		} else
			val->intval = bq->power_good;
		break;
	case POWER_SUPPLY_PROP_STATUS:
		bq2589x_adc_read_vbus_volt(bq, &vbus_volt);
		ret = bq2589x_get_charge_stat(bq, &_val);
		if (ret < 0)
			break;
		if (vbus_volt > 2700 && bq->power_good) {
			val->intval = POWER_SUPPLY_STATUS_CHARGING;
			if (force_discharging_flag == true) {
				val->intval = POWER_SUPPLY_STATUS_NOT_CHARGING;
			}
		} else {
			val->intval = _val;
		}
		break;
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT:
			ret = bq2589x_adc_read_charge_current(bq, &val->intval);
		if (ret < 0)
					break;
		break;
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE:
		ret = bq2589x_get_chargevol(bq, &data);
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
		pr_info("bq2589x:bq->psy_usb_type:%d\n", bq->psy_usb_type);
		val->intval = bq->psy_usb_type;
		break;
	case POWER_SUPPLY_PROP_CURRENT_MAX:
		if (pd_type == true)
			val->intval = 2000000;
		else if (bq->float_type_flag && !thub_plug_flag)
			val->intval = 500000;
		else if ((bq->psy_desc.type == POWER_SUPPLY_TYPE_USB)
			|| (bq->psy_desc.type == POWER_SUPPLY_TYPE_USB_CDP)
			|| thub_plug_flag)
			val->intval = 500000;
		else
			val->intval = 2000000;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_MAX:
		if (bq->psy_desc.type == POWER_SUPPLY_TYPE_USB)
			val->intval = 5000000;
		else
			val->intval = 5000000;
		break;
	case POWER_SUPPLY_PROP_TYPE:
		val->intval = bq->psy_desc.type;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		bq2589x_get_vbus(bq->chg_dev, &(val->intval));
		break;
	default:
		ret = -EINVAL;
		break;
	}
	return ret;
}

static int bq2589x_chg_set_property(struct power_supply *psy,
				   enum power_supply_property psp,
				   const union power_supply_propval *val)
{
	int ret = 0;

	struct bq2589x *bq = power_supply_get_drvdata(psy);
	pr_info("psp=%d\n", psp);
	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		 bq2589x_force_dpdm(bq);
		break;
	case POWER_SUPPLY_PROP_STATUS:
			  ret = val->intval ? bq2589x_enable_charger(bq) : bq2589x_disable_charger(bq);
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

#if 1
static int bq2589x_boost_enable(struct regulator_dev *rdev)
{
	pr_info("%s\n", __func__);

	return bq2589x_set_enable_otg(1);
}

static int bq2589x_boost_disable(struct regulator_dev *rdev)
{
	pr_info("%s\n", __func__);

	return bq2589x_set_enable_otg(0);
}

static int bq2589x_boost_is_enabled(struct regulator_dev *rdev)
{
	struct bq2589x *bq = rdev_get_drvdata(rdev);

	pr_info("%s\n", __func__);

	return bq2589x_is_otg_enable(bq);
}
/*
static int bq2589x_boost_set_current_limit(struct regulator_dev *rdev,
					  int min_uA, int max_uA)
{
	struct bq2589x *bq = rdev_get_drvdata(rdev);
	int i;

	for (i = 0; i < ARRAY_SIZE(bq2589x_otg_oc_threshold); i++) {
		if (min_uA <= bq2589x_otg_oc_threshold[i])
			break;
	}
	if (i == ARRAY_SIZE(bq2589x_otg_oc_threshold) ||
		bq2589x_otg_oc_threshold[i] > max_uA) {
		pr_info("bq2589x:out of current range\n");
		return -EINVAL;
	}

	pr_info("bq2589x:select otg_oc = %d\n", bq2589x_otg_oc_threshold[i]);
	return bq2589x_set_boost_current(bq,bq2589x_otg_oc_threshold[i]);
}
*/

static const struct regulator_ops bq2589x_chg_otg_ops = {
	//.list_voltage = regulator_list_voltage_linear,
	.enable = bq2589x_boost_enable,
	.disable = bq2589x_boost_disable,
	.is_enabled = bq2589x_boost_is_enabled,
	//.set_voltage_sel = bq2589x_boost_set_voltage_sel,
	//.get_voltage_sel = mt6360_boost_get_voltage_sel,
	//.set_current_limit = bq2589x_boost_set_current_limit,
	//.get_current_limit = mt6360_boost_get_current_limit,
};

static const struct regulator_desc bq2589x_otg_rdesc = {
	.name = "usb_otg_vbus",
	.of_match = "usb-otg-vbus",
	.type = REGULATOR_VOLTAGE,
	.owner = THIS_MODULE,
	.ops = &bq2589x_chg_otg_ops,
	.fixed_uV = 5000000,
	.n_voltages = 1,
/*	.vsel_reg = NULL,
	.vsel_mask = NULL,
	.enable_reg = NULL,
	.enable_mask = NULL,
	.csel_reg = NULL,
	.csel_mask = NULL, */
};
#endif

static char *bq2589x_psy_supplied_to[] = {
	"battery",
	"mtk-master-charger",
};

static const struct power_supply_desc bq2589x_psy_desc = {
	.type = POWER_SUPPLY_TYPE_USB,
	.usb_types = bq2589x_chg_psy_usb_types,
	.num_usb_types = ARRAY_SIZE(bq2589x_chg_psy_usb_types),
	.properties = bq2589x_chg_psy_properties,
	.num_properties = ARRAY_SIZE(bq2589x_chg_psy_properties),
	.property_is_writeable = bq2589x_chg_property_is_writeable,
	.get_property = bq2589x_chg_get_property,
	.set_property = bq2589x_chg_set_property,
};

static int bq2589x_chg_init_psy(struct bq2589x *bq)
{
	struct power_supply_config cfg = {
		.drv_data = bq,
		.of_node = bq->dev->of_node,
		.supplied_to = bq2589x_psy_supplied_to,
		.num_supplicants = ARRAY_SIZE(bq2589x_psy_supplied_to),
	};
	pr_info("bq2589x_chg_init_psy!\n");
	//pr_info(bq->dev, "%s\n", __func__);
	memcpy(&bq->psy_desc, &bq2589x_psy_desc, sizeof(bq->psy_desc));
	bq->psy_desc.name = "charger";
	bq->psy = devm_power_supply_register(bq->dev, &bq->psy_desc, &cfg);

	return IS_ERR(bq->psy) ? PTR_ERR(bq->psy) : 0;
}

static struct charger_ops bq2589x_chg_ops = {
	/* cable plug in/out */
	.plug_in = bq2589x_plug_in,
	.plug_out = bq2589x_plug_out,
	/* enable */
	.enable = bq2589x_charging,
	.is_enabled = bq2589x_is_charging_enable,
	/* charging current */
	.set_charging_current = bq2589x_set_ichg,
	.get_charging_current = bq2589x_get_ichg,
	.get_min_charging_current = bq2589x_get_min_ichg,
	/* charging voltage */
	.set_constant_voltage = bq2589x_set_vchg,
	.get_constant_voltage = bq2589x_get_vchg,
	/* input current limit */
	.set_input_current = bq2589x_set_icl,
	.get_input_current = bq2589x_get_icl,
	.get_min_input_current = NULL,
	/* MIVR */
	.set_mivr = bq2589x_set_ivl,
	.get_mivr = bq2589x_get_ivl,
	.get_mivr_state = NULL,
	/* ADC */
	.get_adc = NULL,
	.get_vbus_adc = bq2589x_get_vbus,
//	.get_vbus_state = bq2589x_get_state,
	.get_ibus_adc = bq2589x_get_ibus,
	.get_ibat_adc = NULL,
	.get_tchg_adc = NULL,
	.get_zcv = NULL,
	/* charing termination */
	.set_eoc_current = bq2589x_set_ieoc,
	.enable_termination = bq2589x_enable_te,
	.reset_eoc_state = NULL,
	.safety_check = NULL,
	.is_charging_done = bq2589x_is_charging_done,
	/* power path */
	.enable_powerpath = NULL,
	.is_powerpath_enabled = NULL,
	/* timer */
	.enable_safety_timer = bq2589x_set_safety_timer,
	.is_safety_timer_enabled = bq2589x_is_safety_timer_enabled,
	.kick_wdt = bq2589x_kick_wdt,
	/* AICL */
	.run_aicl = NULL,
	/* PE+/PE+20 */
	.send_ta_current_pattern = NULL,
	.set_pe20_efficiency_table = NULL,
	.send_ta20_current_pattern = NULL,
	.reset_ta = NULL,
	.enable_cable_drop_comp = NULL,
	/* OTG */
	.set_boost_current_limit = bq2589x_set_boost_ilmt,
	.enable_otg = bq2589x_set_otg,
	.enable_discharge = NULL,
	/* charger type detection */
	.enable_chg_type_det = NULL,
	/* misc */
	.dump_registers = bq2589x_dump_register,
	/* event */
	.event = bq2589x_do_event,
	/* 6pin battery */
	.enable_6pin_battery_charging = NULL,
	.enable_hz = bq2589x_enable_hz,
//	.is_enable_hz = bq2589x_is_enable_hiz,
//	.enable_ship_mode = bq2589x_enable_ship,
//	.enable_dpdm = bq2589x_enable_dpdm,
//	.get_float_type_flag = bq2589x_get_float_type_flag,
//	.disable_lightning = disable_show_lightning,
//	.get_thub = bq2589x_get_hub,
//	.rerun_bc12 = bq2589x_rerun_bc12,
//	.get_pogo = bq2589x_get_pogo,
//	.close_adc = bq2589x_close_adc,
//	.set_bootmode = bq2589x_set_bootmode,
//	.set_pd_type = set_pd_type,
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
	 {
	 .compatible = "syv,syv6970QCC",
	 .data = &pn_data[PN_SYV6970QCC],
	 },
	 {
	 .compatible = "sc,sc89890h",
	 .data = &pn_data[PN_SC89890H],
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
	struct regulator_config config = { };
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
		pr_info("No bq2589x device found!\n");
		return -ENODEV;
	}

	match = of_match_node(bq2589x_charger_match_table, node);
	if (match == NULL) {
		pr_info("device tree match not found\n");
		return -EINVAL;
	}

	bq->platform_data = bq2589x_parse_dt(node, bq);

	if (!bq->platform_data) {
		pr_info("No platform data provided.\n");
		return -EINVAL;
	}

		ret = bq2589x_chg_init_psy(bq);
	if (ret < 0) {
		pr_info("failed to init power supply\n");
		return -EINVAL;
	}

	ret = bq2589x_init_device(bq);
	if (ret) {
		pr_info("Failed to init device\n");
		return ret;
	}

	bq2589x_register_interrupt(bq);

	INIT_DELAYED_WORK(&bq->read_byte_work, bq2589x_read_byte_work);
	schedule_delayed_work(&bq->read_byte_work, msecs_to_jiffies(0));
	//bq2589x_force_dpdm(bq);

	bq->chg_dev = charger_device_register(bq->chg_dev_name,
						  &client->dev, bq,
						  &bq2589x_chg_ops,
						  &bq2589x_chg_props);
	if (IS_ERR_OR_NULL(bq->chg_dev)) {
		dev_err(bq->dev, "failed to register chg_dev. err: %d\n", ret);
		ret = PTR_ERR(bq->chg_dev);
		return ret;
	}

	ret = sysfs_create_group(&bq->dev->kobj, &bq2589x_attr_group);
	if (ret)
		dev_err(bq->dev, "failed to register sysfs. err: %d\n", ret);

	bq->is_input_suspend = false;

#if 1
	/* otg regulator */
	config.dev = &client->dev;
	config.driver_data = bq;
	bq->otg_rdev = devm_regulator_register(&client->dev,
						&bq2589x_otg_rdesc, &config);
	if (IS_ERR(bq->otg_rdev)) {
		ret = PTR_ERR(bq->otg_rdev);
		printk("otg_rdev error\n");
		return ret;
	}
#endif
	bq_ex = bq;
	INIT_DELAYED_WORK(&plug_work, charge_plug_work);
	pr_info("bq2589x probe successfully, Part Num:%d, Revision:%d\n!",
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
	bq2589x_adc_stop(bq);
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
