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
extern int hq_config(void);
enum {
	PN_BQ25890,
	PN_BQ25892,
	PN_BQ25895,
};

static int pn_data[] = {
	[PN_BQ25890] = 0x03,
	[PN_BQ25892] = 0x00,
	[PN_BQ25895] = 0x07,
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
/* Huaqin add for HQ-132657 by miaozhichao at 2021/5/6 start */
	struct delayed_work	read_byte_work;
/* Huaqin add for HQ-132657 by miaozhichao at 2021/5/6 start */
	struct power_supply *psy;
	/*K19A HQHW-963 K19A for sy cdp by langjunjun at 2021/7/15 start*/
	struct delayed_work	cdp_work;
	/*K19A HQHW-963 K19A for sy cdp  by langjunjun at 2021/7/15 end*/
};
/* Huaqin modify for WXYFB-592 by miaozhichao at 2021/3/29 start */
extern enum hvdcp_status hvdcp_type_tmp;
/* Huaqin modify for WXYFB-592 by miaozhichao at 2021/3/29 end */
static int g_charger_type = 0;
/* Huaqin add for HQ-134476 by miaozhichao at 2021/5/29 start */

/*K19A HQHW-963 K19A for sy cdp by langjunjun at 2021/7/15 start*/
bool cdp_detect = false;
/*K19A HQHW-963 K19A for sy cdp by langjunjun at 2021/7/15 end*/

static int charger_detect_count = 3;
static int charger_float_count = 0;
/* Huaqin add for HQ-134476 by miaozhichao at 2021/5/29 end */

bool cdp_unattach = false;

int get_charger_type()
{
	return g_charger_type;
}
EXPORT_SYMBOL_GPL(get_charger_type);
static const struct charger_properties bq2589x_chg_props = {
	.alias_name = "bq2589x",
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
/*K19A HQHW-963 K19A  kernel charger by zhixueyin at 2021/6/29 start*/
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
/*K19A HQHW-963 K19A  kernel charger by zhixueyin at 2021/6/29 end*/
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

	if (curr < BQ2589X_ITERM_BASE)
		curr = BQ2589X_ITERM_BASE;

	iterm = (curr - BQ2589X_ITERM_BASE) / BQ2589X_ITERM_LSB;

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

	pr_info("Force DPDM %s\n", !ret ?
"successfully" : "failed");

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
/*K19A HQHW-881 K19A charger of 2.5v by wangqi at 2021/5/20 start*/
		pdata->iprechg = 256;
/*K19A HQHW-881 K19A charger of 2.5v by wangqi at 2021/5/20 end*/
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


	return pdata;
}
static int bq2589x_get_charger_type_ext(struct charger_device *chg_dev, u32 *type)
{
	//*type = g_charger_type;
	return 0;
}
EXPORT_SYMBOL_GPL(bq2589x_get_charger_type_ext);
/*K19A HQ-138863 K19A  cdp by zhixueyin at 2021/7/10 start*/
bool bq2589x_get_cdp_status(void)
{
	return cdp_detect;
}
static int chip_num(struct bq2589x *bq)
{
	int ret = 0;
	int id_dis = 0;
	u8 reg_val = 0;
	ret = bq2589x_read_byte(bq,BQ2589X_REG_14,&reg_val);
	id_dis = (reg_val & BQ2589X_PN_MASK);
	id_dis >>= BQ2589X_PN_SHIFT;
	pr_err(" bq2589x:id_dis:%d", id_dis);
	return id_dis;
}
/*K19A HQ-138863 K19A  cdp by zhixueyin at 2021/7/10 end*/
static int bq2589x_get_charger_type(struct bq2589x *bq, enum charger_type *type)
{
	int ret;
	u8 reg_val = 0;
	int vbus_stat = 0;
	enum charger_type chg_type = CHARGER_UNKNOWN;
	/*K19A HQ-129052 K19A charger of thermal by wangqi at 2021/4/22 start*/
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
	/*K19A HQ-129052 K19A charger of thermal by wangqi at 2021/4/22 end*/
	/*K19A k19A-143 K19A charger_type by wangqi at 2021/4/15 start*/
	hvdcp_type_tmp = HVDCP_NULL;
	/*K19A k19A-143 K19A charger_type by wangqi at 2021/4/15 end*/	
	ret = bq2589x_read_byte(bq, BQ2589X_REG_0B, &reg_val);
	if (ret)
		return ret;
	vbus_stat = (reg_val & BQ2589X_VBUS_STAT_MASK);
	vbus_stat >>= BQ2589X_VBUS_STAT_SHIFT;
	switch (vbus_stat) {

	case BQ2589X_VBUS_TYPE_NONE:
		chg_type = CHARGER_UNKNOWN;
		break;
	case BQ2589X_VBUS_TYPE_SDP:
		chg_type = STANDARD_HOST;
		break;
	case BQ2589X_VBUS_TYPE_CDP:
		chg_type = CHARGING_HOST;
		break;
	case BQ2589X_VBUS_TYPE_DCP:
		chg_type = STANDARD_CHARGER;
		break;
/*K19A WXYFB-996 K19A quick charger bq25890 bring up by miaozhichao at 2021/3/29 start*/
	case BQ2589X_VBUS_TYPE_HVDCP:
		chg_type = HVDCP_CHARGER;
/* Huaqin modify for WXYFB-592 by miaozhichao at 2021/3/29 start */
		hvdcp_type_tmp = HVDCP;
/* Huaqin modify for WXYFB-592 by miaozhichao at 2021/3/29 end */
		break;
/*K19A WXYFB-996 K19A quick charger bq25890 bring up by miaozhichao at 2021/3/29 end*/
	case BQ2589X_VBUS_TYPE_UNKNOWN:
		chg_type = NONSTANDARD_CHARGER;
		break;
	case BQ2589X_VBUS_TYPE_NON_STD:
		chg_type = NONSTANDARD_CHARGER;
		break;
/* Huaqin add for HQ-136291 by miaozhichao at 2021/5/20 start */
	case BQ2589X_VBUS_TYPE_OTG:
		chg_type = CHARGER_UNKNOWN;
		break;
/* Huaqin add for HQ-136291 by miaozhichao at 2021/5/20 end */
	default:
		chg_type = NONSTANDARD_CHARGER;
		break;
	}

	*type = chg_type;
	g_charger_type = chg_type;
	/*K19A HQ-129052 K19A charger of thermal by wangqi at 2021/4/22 start*/
	if(usb_psy == NULL)
		usb_psy = power_supply_get_by_name("usb");

	if(usb_psy != NULL)
		mt_chg = power_supply_get_drvdata(usb_psy);

	if(mt_chg != NULL)
		mt_chg->usb_desc.type = smblib_apsd_results[chg_type];
	/*K19A HQ-129052 K19A charger of thermal by wangqi at 2021/4/22 end*/
	/*K19A-104 charge by wangchao at 2021/4/8 start*/
	pr_err("vbus_stat:%d ,chg_type:%d\n", vbus_stat,chg_type);
	/*K19A-104 charge by wangchao at 2021/4/8 end*/
	return 0;
}

static int bq2589x_inform_charger_type_report(struct bq2589x *bq)
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

/*K19A HQHW-963 K19A for sy cdp by langjunjun at 2021/7/15 start*/
static void bq2589x_cdp_work(struct work_struct *work)
{
	int timer_count = 0;
	
	struct bq2589x *bq = container_of(work, struct bq2589x, cdp_work.work);
	while (1) {
		if (timer_count >= 10 && cdp_unattach){
			bq2589x_inform_charger_type_report(bq);
			cdp_detect = false;
			wusb3801_intr_handler_resume();
			pr_err("wlc timer_count more than 10,timer_count:%d \n", timer_count);
			timer_count = 0;
			cdp_unattach = false;
			break;
		} else if(timer_count < 10  && cdp_unattach) {
			pr_err("wlc 5S count down! \n");
			msleep(5000);
			bq2589x_inform_charger_type_report(bq);
			cdp_detect = false;
			wusb3801_intr_handler_resume();
			pr_err("wlc 5S finish! timer_count less than 10,timer_count:%d \n", timer_count);
			timer_count = 0;
			cdp_unattach = false;
			break;
		}

		msleep(1000);
		timer_count++;
	}
}

static void bq2589x_inform_charger_type(struct bq2589x *bq)
{
	pr_err("ljj bq2589x_inform_charger_type chg_type = %d, cdp_detect=%d, bq->power_good=%d, \n", bq->chg_type, cdp_detect, bq->power_good);
	if (chip_num(bq) != 3) {
		if (((cdp_detect == false) && (bq->chg_type == BQ2589X_VBUS_TYPE_CDP)) || (cdp_detect == true)) {
			if ((bq->power_good) && (cdp_detect == false)) {
				cdp_detect = true;
				bq2589x_inform_charger_type_report(bq);
				schedule_delayed_work(&bq->cdp_work, msecs_to_jiffies(1));
				pr_err("wlc cdp detected \n");
			} else if ((!bq->power_good) && (cdp_detect == true)) {
				cdp_unattach = true;
				pr_err("wlc cdp unattach happend \n");
			} else {
				cancel_delayed_work_sync(&bq->cdp_work);
			}
		} else {
			bq2589x_inform_charger_type_report(bq);
		}
	} else {
		bq2589x_inform_charger_type_report(bq);
	}
}
/*K19A HQHW-963 K19A for sy cdp  by langjunjun at 2021/7/15 end*/

/*K19A WXYFB-996 K19A charger by wangchao at 2021/4/22 start*/
static int bq2589x_enable_chg_type_det(struct charger_device *chg_dev, bool en)
{
/* Huaqin add for HQ-138817 by miaozhichao at 2021/6/3 start */
	int ret;
/* Huaqin add for HQ-138817 by miaozhichao at 2021/6/3 end*/
	struct bq2589x *bq = dev_get_drvdata(&chg_dev->dev);
/* Huaqin add for HQ-138817 by miaozhichao at 2021/6/3 start */
	ret = bq2589x_get_charger_type(bq, &bq->chg_type);
	if (!ret)
		bq2589x_inform_charger_type(bq);
/* Huaqin add for HQ-138817 by miaozhichao at 2021/6/3 end*/
/* Huaqin add for HQ-134476 by miaozhichao at 2021/5/29 start */
	schedule_delayed_work(&bq->read_byte_work, msecs_to_jiffies(600));
/* Huaqin add for HQ-134476 by miaozhichao at 2021/5/29 end */
/* Huaqin add for HQ-138817 by miaozhichao at 2021/6/3 start */
	pr_err("end,bq->chg_type = %d\n",bq->chg_type);
/* Huaqin add for HQ-138817 by miaozhichao at 2021/6/3 end*/
	return 0;
}
/*K19A WXYFB-996 K19A charger by wangchao at 2021/4/22 end*/
/* Huaqin add for HQ-132657 by miaozhichao at 2021/5/6 start */
extern void Charger_Detect_Init(void);
extern void Charger_Detect_Release(void);

static void bq2589x_read_byte_work(struct work_struct *work)
{
        int ret;
/* Huaqin add for HQ-132657 by miaozhichao at 2021/5/27 start */
	u8 reg_val = 0;
	int vbus_stat = 0;
	int vbus_gd = 0;
	int id_dis = 0;
/* Huaqin add for HQ-132657 by miaozhichao at 2021/5/27 end */
	struct bq2589x *bq = container_of(work,
			struct bq2589x, read_byte_work.work);
	enum charger_type prev_chg_type;
/* Huaqin add for HQHW-963 by wanglicheng at 2021/7/1 start */
	static bool std_mode_dec = true;
/* Huaqin add for HQHW-963 by wanglicheng at 2021/7/1 end */

	prev_chg_type = bq->chg_type;
	ret = bq2589x_get_charger_type(bq, &bq->chg_type);
/* Huaqin add for HQ-132657 by miaozhichao at 2021/5/27 start */
/* Huaqin add for HQ-134476 by miaozhichao at 2021/5/29 start */
	ret = bq2589x_read_byte(bq, BQ2589X_REG_0B, &reg_val);
	vbus_stat = (reg_val & BQ2589X_VBUS_STAT_MASK);
	vbus_stat >>= BQ2589X_VBUS_STAT_SHIFT;
	ret = bq2589x_read_byte(bq, BQ2589X_REG_11, &reg_val);
	vbus_gd = (reg_val & BQ2589X_VBUS_GD_MASK);
	vbus_gd >>= BQ2589X_VBUS_GD_SHIFT;
/* Huaqin add for HQ-134476 by miaozhichao at 2021/5/29 end */
	ret = bq2589x_read_byte(bq,BQ2589X_REG_14,&reg_val);
	id_dis = (reg_val & BQ2589X_PN_MASK);
	id_dis >>= BQ2589X_PN_SHIFT;
	pr_err(" bq2589x:id_dis:%d",id_dis);
/* Huaqin add for HQ-134476 by miaozhichao at 2021/5/29 start */
	if(id_dis == 3){
		if(charger_detect_count > 0){
			if(vbus_gd && vbus_stat == BQ2589X_VBUS_TYPE_DCP) {
				pr_err("bq2589x:enable hvdcp\n");
				ret = bq2589x_enable_hvdcp(bq);
				bq2589x_force_dpdm(bq);
			}else if(vbus_gd && vbus_stat == BQ2589X_VBUS_TYPE_HVDCP ) {
				ret = bq2589x_disable_hvdcp(bq);
				pr_err("bq2589x:reset\n");
			}else if(vbus_gd && vbus_stat == BQ2589X_VBUS_TYPE_SDP ) {
				pr_err("foce dpdm ti\n");
				bq2589x_force_dpdm(bq);
			}else if(vbus_gd && vbus_stat == BQ2589X_VBUS_TYPE_UNKNOWN ) {
				charger_detect_count++;
				charger_float_count++;
				if(charger_float_count <= 30){
					bq2589x_force_dpdm(bq);
				} else {
					charger_float_count = 31;//avoid overflow
				}
				pr_err(" foce UNKNOWN ti,charger_float_count:%d\n",charger_float_count);
			}else if (vbus_gd && vbus_stat == BQ2589X_VBUS_TYPE_NON_STD) {
				Charger_Detect_Init();
				bq2589x_dp_set_3P3V(bq);
				bq2589x_dm_set_0P6V(bq);
				bq2589x_enable_auto_dpdm(bq, true);
				bq2589x_force_dpdm(bq);
				mdelay(2000);
				Charger_Detect_Release();
				pr_err("foce NONSTD ti\n");
			} 
			charger_detect_count --;
			pr_err("charger_detect_count:%d\n",charger_detect_count);
		}
	}else{
		if(vbus_gd && vbus_stat == BQ2589X_VBUS_TYPE_SDP ){
			bq2589x_force_dpdm(bq);
			std_mode_dec = false;
			pr_err("foce dpdm silergy\n");
		} else if (vbus_gd && vbus_stat == BQ2589X_VBUS_TYPE_UNKNOWN) {
			charger_float_count++;
			if (charger_float_count <= 30) {
				bq2589x_force_dpdm(bq);
			} else {
				charger_float_count = 31;//avoid overflow
			}
			std_mode_dec = false;
			pr_err(" foce UNKNOWN silergy,charger_float_count:%d\n",charger_float_count);
		} else if (vbus_gd && vbus_stat == BQ2589X_VBUS_TYPE_NON_STD && !std_mode_dec) {
			std_mode_dec = true;
			ret = bq2589x_enter_hiz_mode(bq);
			ret = bq2589x_exit_hiz_mode(bq);
			bq2589x_force_dpdm(bq);
			pr_err("non std foce dpdm\n");
		} else {
			std_mode_dec = true;
			pr_err("vbus_stat = %d\n", vbus_stat);
		}
	}
/* Huaqin add for HQ-134476 by miaozhichao at 2021/5/29 end */
/* Huaqin add for HQ-132657 by miaozhichao at 2021/5/27 end */
	if (prev_chg_type != bq->chg_type && bq->chg_det_enable)
		bq2589x_inform_charger_type(bq);
}
/* Huaqin add for HQ-132657 by miaozhichao at 2021/5/6 end */
static irqreturn_t bq2589x_irq_handler(int irq, void *data)
{
	int ret;
	u8 reg_val;
	bool prev_pg;
/* Huaqin add for K19A-309 by wangchao at 2021/5/29 start */
	enum charger_type prev_chg_type;
/* Huaqin add for K19A-309 by wangchao at 2021/5/29 end */
	struct bq2589x *bq = data;
	ret = bq2589x_read_byte(bq, BQ2589X_REG_0B, &reg_val);
	if (ret)
		return IRQ_HANDLED;

	prev_pg = bq->power_good;

	bq->power_good = !!(reg_val & BQ2589X_PG_STAT_MASK);

/* Huaqin modify for WXYFB-592 by miaozhichao at 2021/3/29 start */
/* Huaqin add for HQ-134476 by miaozhichao at 2021/5/29 start */
	if (!prev_pg && bq->power_good) {
		pr_err("adapter/usb inserted\n");
		charger_detect_count = 3;
		charger_float_count = 0;
	}else if (prev_pg && !bq->power_good){
		hvdcp_type_tmp = HVDCP_NULL;
		charger_detect_count = 0;
		charger_float_count = 30;
	}else{
		pr_err("prev_pg = %d  bq->power_good = %d\n",prev_pg,bq->power_good);
	}
/* Huaqin add for HQ-134476 by miaozhichao at 2021/5/29 end */
/* Huaqin modify for WXYFB-592 by miaozhichao at 2021/3/29 end */
/* Huaqin add for K19A-309 by wangchao at 2021/5/29 start */
	prev_chg_type = bq->chg_type;
	ret = bq2589x_get_charger_type(bq, &bq->chg_type);
	if (!ret &&prev_chg_type != bq->chg_type)
		bq2589x_inform_charger_type(bq);
/* Huaqin add for K19A-309 by wangchao at 2021/5/29 end */
/* Huaqin add for HQ-132657 by miaozhichao at 2021/5/6 start */
	schedule_delayed_work(&bq->read_byte_work, msecs_to_jiffies(600));
/* Huaqin add for HQ-132657 by miaozhichao at 2021/5/6 end */
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
/* Huaqin add for K19A-312 by wangchao at 2021/6/3 start */
static void bq2589x_dump_regs(struct bq2589x *bq);
/* Huaqin add for K19A-312 by wangchao at 2021/6/3 end */

static int bq2589x_init_device(struct bq2589x *bq)
{
	int ret;
/* Huaqin add for HQ-132657 by miaozhichao at 2021/5/27 start */
	int id_dis = 0;
	u8 reg_val = 0;
/* Huaqin add for HQ-132657 by miaozhichao at 2021/5/27 end */
/* Huaqin add for K19A-312 by wangchao at 2021/6/3 start */
	pr_err("bq2589x_dump_regs before init: \n");
	bq2589x_dump_regs(bq);
/* Huaqin add for K19A-312 by wangchao at 2021/6/3 end */
	/* Huaqin add for HQHW-963 by zhixueyin at 2021/6/29 start */
	ret = bq2589x_disable_12V(bq);
	if (ret)
		pr_err("Failed to disable 12V, ret = %d\n", ret);
	bq2589x_enable_auto_dpdm(bq, true);
	/* Huaqin add for HQHW-963 by zhixueyin at 2021/6/29 end */
	bq2589x_disable_watchdog_timer(bq);
	/*K19A HQ-133582 K19A charger time by wangqi at 2021/5/6 start*/
	bq2589x_disable_safety_timer(bq);
	/*K19A HQ-133582 K19A charger time by wangqi at 2021/5/6 end*/
	/*K19A HQ-133295 K19A charger full time by wangqi at 2021/5/6 start*/
	bq2589x_set_ir_compensation(bq, 20, 64);
	/*K19A HQ-133295 K19A charger full time by wangqi at 2021/5/6 end*/

	ret = bq2589x_set_prechg_current(bq, bq->platform_data->iprechg);
	if (ret)
		pr_err("Failed to set prechg current, ret = %d\n", ret);

	/*K19A HQ-124114 K19A charger of jeita by wangqi at 2021/4/23 start*/
	ret = bq2589x_set_term_current(bq, 200);
	/*K19A HQ-124114 K19A charger of jeita by wangqi at 2021/4/23 end*/
	if (ret)
		pr_err("Failed to set termination current, ret = %d\n", ret);

	ret = bq2589x_set_boost_voltage(bq, bq->platform_data->boostv);
	if (ret)
		pr_err("Failed to set boost voltage, ret = %d\n", ret);

	ret = bq2589x_set_boost_current(bq, bq->platform_data->boosti);
	if (ret)
		pr_err("Failed to set boost current, ret = %d\n", ret);
/* Huaqin add/modify/del for WXYFB-996 by miaozhichao at 2021/3/29 start */
	ret = bq2589x_disable_maxcen(bq);
	if (ret)
		pr_err("Failed to set disable maxcen, ret = %d\n", ret);
/* Huaqin add/modify/del for WXYFB-996 by miaozhichao at 2021/3/29 end */
/* Huaqin add for HQ-132657 by miaozhichao at 2021/5/27 start */
	ret = bq2589x_read_byte(bq,BQ2589X_REG_14,&reg_val);
	id_dis = (reg_val & BQ2589X_PN_MASK);
	id_dis >>= BQ2589X_PN_SHIFT;
	if(id_dis == 3){
		/* Huaqin add for HQ-134273 by wangqi at 2021/6/1 start */
		ret = bq2589x_set_term_current(bq, 128);
		/* Huaqin add for HQ-134273 by wangqi at 2021/6/1 end */
		if (hq_config() == 4 ||hq_config() == 5 ||
			 hq_config() == 6 || hq_config() == 7) {
			ret = bq2589x_set_term_current(bq, 200);
			pr_err("only K19S set 200ma ieoc wlc\n");
		}
		ret = bq2589x_disable_hvdcp(bq);
		pr_err("disable hvdcp,ret = %d\n",ret);
	}else{
		/* Huaqin add for HQ-134273 by wangqi at 2021/6/1 start */
		ret = bq2589x_set_term_current(bq, 200);
		/* Huaqin add for HQ-134273 by wangqi at 2021/6/1 end */
		ret = bq2589x_enable_hvdcp(bq);
		pr_err("enable hvdcp,ret = %d\n",ret);
	}
/* Huaqin add for HQ-132657 by miaozhichao at 2021/5/27 end */
/* Huaqin add for HQ-135953 by wangchao at 2021/6/11 start */
	ret = bq2589x_enable_ico(bq, 0);//disable ico
	if (ret)
		pr_err("Failed to disable ico, ret = %d\n", ret);
/* Huaqin add for HQ-135953 by wangchao at 2021/6/11 end */

/* Huaqin add for K19A-312 by wangchao at 2021/6/3 start */
	ret = bq2589x_exit_hiz_mode(bq);
	if (ret)
		pr_err("Failed to set exit_hiz_mode, ret = %d\n", ret);
/* Huaqin add for HQHW-963 by zhixueyin at 2021/6/29 start */
	ret = bq2589x_enable_hvdcp(bq);
	bq2589x_enable_auto_dpdm(bq, true);
	ret = bq2589x_force_dpdm(bq);
/* Huaqin add for HQHW-963 by zhixueyin at 2021/6/29 end */
	pr_err("bq2589x_dump_regs after init: \n");
	bq2589x_dump_regs(bq);
/* Huaqin add for K19A-312 by wangchao at 2021/6/3 end */

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

/*K19A-75 charge by wangchao at 2021/4/22 start*/
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
/*K19A-75 charge by wangchao at 2021/4/22 end*/

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

/*K19A WXYFB-588 K19A charger usb_otg by wangqi at 2021/3/27 start*/
extern bool usb_otg;
/*K19A WXYFB-588 K19A charger usb_otg by wangqi at 2021/3/27 end*/
static int bq2589x_set_otg(struct charger_device *chg_dev, bool en)
{
	int ret;
	struct bq2589x *bq = dev_get_drvdata(&chg_dev->dev);

/*K19A WXYFB-588 K19A charger usb_otg by wangqi at 2021/3/27 start*/
	usb_otg = en;
/*K19A WXYFB-588 K19A charger usb_otg by wangqi at 2021/3/27 end*/
	if (en)
		ret = bq2589x_enable_otg(bq);
	else
		ret = bq2589x_disable_otg(bq);

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

/*K19A HQ-135863 K19A charger of charge full by wangqi at 2021/5/20 start*/
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
/*K19A HQ-135863 K19A charger of charge full by wangqi at 2021/5/20 end*/

static struct charger_ops bq2589x_chg_ops = {
	/* Normal charging */
	.plug_in = bq2589x_plug_in,
	.plug_out = bq2589x_plug_out,
	.dump_registers = bq2589x_dump_register,
	.enable = bq2589x_charging,
	/*K19A-75 charge by wangchao at 2021/4/15 start*/
	.enable_hz = bq2589x_enable_hiz,
	/*K19A-75 charge by wangchao at 2021/4/15 start*/
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
	.get_charger_type = bq2589x_get_charger_type_ext,
	/*K19A WXYFB-996 K19A charger by wangchao at 2021/4/2 start*/
	.enable_chg_type_det = bq2589x_enable_chg_type_det,
	/*K19A WXYFB-996 K19A charger by wangchao at 2021/4/2 end*/

	/* Safety timer */
	.enable_safety_timer = bq2589x_set_safety_timer,
	.is_safety_timer_enabled = bq2589x_is_safety_timer_enabled,

	/* Power path */
	.enable_powerpath = NULL,
	.is_powerpath_enabled = NULL,

	/* OTG */
	.enable_otg = bq2589x_set_otg,
	/*K19A K19A-187 K19A charger of set boost current by wangqi at 2021/4/27 start*/
	.set_otg_current = bq2589x_set_boost_ilmt,
	/*K19A K19A-187 K19A charger of set boost current by wangqi at 2021/4/27 start*/
	.enable_discharge = NULL,
	/*K19A HQ-135863 K19A charger of charge full by wangqi at 2021/5/20 start*/
	.event = bq2589x_do_event,
	/*K19A HQ-135863 K19A charger of charge full by wangqi at 2021/5/20 end*/

	/* PE+/PE+20 */
	.send_ta_current_pattern = NULL,
	.set_pe20_efficiency_table = NULL,
	.send_ta20_current_pattern = NULL,
	.enable_cable_drop_comp = NULL,

	/* ADC */
	.get_tchg_adc = NULL,
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
/* Huaqin add for HQ-132657 by miaozhichao at 2021/5/6 start */
	INIT_DELAYED_WORK(&bq->read_byte_work,bq2589x_read_byte_work);
/* Huaqin add for HQ-132657 by miaozhichao at 2021/5/6 end */
	/*K19A HQHW-963 K19A for sy cdp by langjunjun at 2021/7/15 start*/
	INIT_DELAYED_WORK(&bq->cdp_work, bq2589x_cdp_work);
	/*K19A HQHW-963 K19A for sy cdp  by langjunjun at 2021/7/15 end*/
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
	/*K19A-185 charge by wangchao at 2021/4/15 start*/
	struct bq2589x *bq = i2c_get_clientdata(client);
	/*HQ-138863 charge by zhixueyin at 2021/7/13 start*/
	u8 reg_val = 0;
	int ichg = 0;
	/*HQ-138863 charge by zhixueyin at 2021/7/13 end*/
	bq2589x_disable_otg(bq);
	pr_err("bq2589x_disable_otg for shutdown\n");
	/*K19A-185 charge by wangchao at 2021/4/26 end*/
	/*HQ-138863 charge by zhixueyin at 2021/7/13 start*/
	if (chip_num(bq) != 3) {
		bq2589x_read_byte(bq, BQ2589X_REG_04, &reg_val);
		ichg = (reg_val & BQ2589X_ICHG_MASK) >> BQ2589X_ICHG_SHIFT;
		ichg = ichg * BQ2589X_ICHG_LSB + BQ2589X_ICHG_BASE;
		if (ichg < 128) {
			bq2589x_set_chargecurrent(bq, 128);
		}
	}
	/*HQ-138863 charge by zhixueyin at 2021/7/13 end*/
	/*K19A HQ-138863 K19A  cdp by zhixueyin at 2021/7/10 start*/
	bq2589x_disable_maxcen(bq);
	pr_err("bq2589x_disable_maxcen for shutdown\n");
	/*K19A HQ-138863 K19A  cdp by zhixueyin at 2021/7/10 start*/
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
