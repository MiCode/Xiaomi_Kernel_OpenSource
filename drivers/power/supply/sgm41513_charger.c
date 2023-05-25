/*
 * BQ2560x battery charging driver
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

#define pr_fmt(fmt)	"[sgm41513]:%s: " fmt, __func__

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

#include "mtk_charger.h"
#include "sgm41513_charger.h"
#include "sgm41513.h"
#if 1
#undef pr_debug
#define pr_debug pr_err
#undef pr_info
#define pr_info pr_err
#undef dev_dbg
#define dev_dbg dev_err
#else
#undef pr_info
#define pr_info pr_debug
#endif

static u8 sgm41513_otg_flag = 0;
enum sgm41513_part_no {
	SGM41513 = 0x00,
};

enum sgm41513_charge_state {
	CHARGE_STATE_IDLE = SGM41513_REG08_CHRG_STAT_IDLE,
	CHARGE_STATE_PRECHG = SGM41513_REG08_CHRG_STAT_PRECHG,
	CHARGE_STATE_FASTCHG = SGM41513_REG08_CHRG_STAT_FASTCHG,
	CHARGE_STATE_CHGDONE = SGM41513_REG08_CHRG_STAT_CHGDONE,
};


struct sgm41513 {
	struct device *dev;
	struct i2c_client *client;

	enum sgm41513_part_no part_no;
	int revision;

	int status;
	
	struct mutex i2c_rw_lock;

	bool charge_enabled;/* Register bit status */

	struct sgm41513_platform_data* platform_data;
	struct charger_device *chg_dev;
	
};

static const struct charger_properties sgm41513_chg_props = {
	.alias_name = "sgm41513",
};

static int __sgm41513_read_reg(struct sgm41513* sgm, u8 reg, u8 *data)
{
	s32 ret;

	ret = i2c_smbus_read_byte_data(sgm->client, reg);
	if (ret < 0) {
		pr_err("i2c read fail: can't read from reg 0x%02X\n", reg);
		return ret;
	}
	*data = (u8)ret;
	
	return 0;
}

static int __sgm41513_write_reg(struct sgm41513* sgm, int reg, u8 val)
{
	s32 ret;
	ret = i2c_smbus_write_byte_data(sgm->client, reg, val);
   /*	pr_err("i2c write>>>>>>: write 0x%02X to reg 0x%02X ret: %d\n",
				val, reg, ret); */
	if (ret < 0) {
		pr_err("i2c write fail: can't write 0x%02X to reg 0x%02X: %d\n",
				val, reg, ret);
		return ret;
	}
	return 0;
}

static int sgm41513_read_byte(struct sgm41513 *sgm, u8 *data, u8 reg)
{
	int ret;

	mutex_lock(&sgm->i2c_rw_lock);
	ret = __sgm41513_read_reg(sgm, reg, data);
	mutex_unlock(&sgm->i2c_rw_lock);

	return ret;
}


static int sgm41513_write_byte(struct sgm41513 *sgm, u8 reg, u8 data)
{
	int ret;

	mutex_lock(&sgm->i2c_rw_lock);
	ret = __sgm41513_write_reg(sgm, reg, data);
	mutex_unlock(&sgm->i2c_rw_lock);

	if (ret) {
		pr_err("Failed: reg=%02X, ret=%d\n", reg, ret);
	}

	return ret;
}


static int sgm41513_update_bits(struct sgm41513 *sgm, u8 reg, 
				u8 mask, u8 data)
{
	int ret;
	u8 tmp,tmp2;

	mutex_lock(&sgm->i2c_rw_lock);
	ret = __sgm41513_read_reg(sgm, reg, &tmp);
	if (ret) {
		pr_err("Failed: reg=%02X, ret=%d\n", reg, ret);
		goto out;
	}
	tmp2=tmp;
	tmp &= ~mask;
	tmp |= data & mask;
	pr_info("tmp=%x,tmp2=%x",tmp,tmp2);
	if(tmp != tmp2)
	ret = __sgm41513_write_reg(sgm, reg, tmp);
	if (ret) {
		pr_err("Failed: reg=%02X, ret=%d\n", reg, ret);
	}
out:
	mutex_unlock(&sgm->i2c_rw_lock);
	return ret;
}

static int sgm41513_enable_otg(struct sgm41513 *sgm)
{
	u8 val = SGM41513_REG01_OTG_ENABLE << SGM41513_REG01_OTG_CHG_CONFIG_SHIFT;
    pr_info("sgm41513_enable_otg enter\n");
	return sgm41513_update_bits(sgm, SGM41513_REG_01,
				SGM41513_REG01_OTG_CHG_CONFIG_MASK, val);
}

static int sgm41513_disable_otg(struct sgm41513 *sgm)
{
	u8 val = SGM41513_REG01_OTG_DISABLE << SGM41513_REG01_OTG_CHG_CONFIG_SHIFT;
	pr_info("sgm41513_disable_otg enter\n");
	return sgm41513_update_bits(sgm, SGM41513_REG_01,
				   SGM41513_REG01_OTG_CHG_CONFIG_MASK, val);

}

static int sgm41513_enable_charger(struct sgm41513 *sgm)
{
	int i = 2,ret =0;
	u8 data = 0;
	if (sgm41513_otg_flag == 0)
	{
		/* ret = sgm41513_disable_otg(sgm); */
		while(i)
		{
			ret = sgm41513_read_byte(sgm, &data, SGM41513_REG_01);

			if(data & 0x20)
			{
				ret = sgm41513_disable_otg(sgm);
				i--;
			} else {
				break;
			}
		}
	}
	ret = sgm41513_update_bits(sgm, SGM41513_REG_01, SGM41513_REG01_CHG_CONFIG_MASK,
		SGM41513_REG01_CHG_ENABLE << SGM41513_REG01_CHG_CONFIG_SHIFT);
	return ret;
}

static int sgm41513_disable_charger(struct sgm41513 *sgm)
{
	int ret;
	u8 val = SGM41513_REG01_CHG_DISABLE << SGM41513_REG01_CHG_CONFIG_SHIFT;

	ret = sgm41513_update_bits(sgm, SGM41513_REG_01, SGM41513_REG01_CHG_CONFIG_MASK, val);
	return ret;
}

int sgm41513_set_chargecurrent(struct sgm41513 *sgm, int curr)
{
	u8 reg_ichg,read_ichg,ret=0;
/*
	if (curr < SGM41513_REG02_ICHG_BASE)
		curr = SGM41513_REG02_ICHG_BASE;

	ichg = (curr - SGM41513_REG02_ICHG_BASE)/SGM41513_REG02_ICHG_LSB;
	return sgm41513_update_bits(sgm, SGM41513_REG_02, SGM41513_REG02_ICHG_MASK, 
				ichg << SGM41513_REG02_ICHG_SHIFT);
*/
	if (curr < 5)
		reg_ichg = 0;
	else if(curr < 40)
		reg_ichg = (curr - 0) /5;
	else if(curr < 110)
		reg_ichg = (curr - 40)/10 + 8;
	else if(curr < 270)
		reg_ichg = (curr - 110)/20 + 15;
	else if(curr < 540)
		reg_ichg = (curr - 270)/30 + 23;
	else if(curr < 1500)
		reg_ichg = (curr - 540)/60 + 32;
	else if(curr < 3000)
		reg_ichg = (curr - 1500)/120 + 48;
	else
		reg_ichg = 61;
	
	
	ret = sgm41513_read_byte(sgm, &read_ichg, SGM41513_REG_02);
	if (!ret) {
		read_ichg = (read_ichg & SGM41513_REG02_ICHG_MASK) >> SGM41513_REG02_ICHG_SHIFT;
	}
	pr_info("[%s]:curr =%d,reg_ichg=%x,read_ichg=%x\n",__func__,curr,reg_ichg,read_ichg);	
	if (read_ichg == reg_ichg)
		return ret;
	else
		return sgm41513_update_bits(sgm, SGM41513_REG_02, SGM41513_REG02_ICHG_MASK, 
				reg_ichg << SGM41513_REG02_ICHG_SHIFT);
}

int sgm41513_set_term_current(struct sgm41513 *sgm, int curr)
{
	u8 reg_iterm;
/*
	if (curr < SGM41513_REG03_ITERM_BASE)
		curr = SGM41513_REG03_ITERM_BASE;

	iterm = (curr - SGM41513_REG03_ITERM_BASE) / SGM41513_REG03_ITERM_LSB;
*/
	if (curr < 5)
		reg_iterm = 0;
	else if(curr < 20)
		reg_iterm = (curr - 5) /5;
	else if(curr < 60)
		reg_iterm = (curr - 20)/10 + 3;
	else if(curr < 200)
		reg_iterm = (curr - 60)/20 + 7;
	else if(curr < 240)
		reg_iterm = (curr - 200)/40 + 14;
	else
		reg_iterm = 15;
	pr_info("[%s]:curr =%d,reg_iterm=%x\n",__func__,curr,reg_iterm);	
	return sgm41513_update_bits(sgm, SGM41513_REG_03, SGM41513_REG03_ITERM_MASK, 
				reg_iterm << SGM41513_REG03_ITERM_SHIFT);
}
EXPORT_SYMBOL_GPL(sgm41513_set_term_current);


int sgm41513_set_prechg_current(struct sgm41513 *sgm, int curr)
{
	u8 reg_iprechg;

	pr_info("%s curr = %d\n", __func__, curr);

	if (curr < 20)
		reg_iprechg = (curr -5)/5;
	else if(curr < 60)
		reg_iprechg = (curr - 20)/10 + 3;
	else if(curr < 200)
		reg_iprechg = (curr - 60)/20 + 7;
	else if(curr < 240)
		reg_iprechg = (curr - 200)/40 + 14;
	else 
		reg_iprechg = 15;
	pr_info("%s reg_iprechg=%d\n", __func__, reg_iprechg);
/*
	if (curr < SGM41513_REG03_IPRECHG_BASE)
		curr = SGM41513_REG03_IPRECHG_BASE;
	iprechg = (curr - SGM41513_REG03_IPRECHG_BASE) / SGM41513_REG03_IPRECHG_LSB;
*/
	return sgm41513_update_bits(sgm, SGM41513_REG_03, SGM41513_REG03_IPRECHG_MASK, 
				reg_iprechg << SGM41513_REG03_IPRECHG_SHIFT);
}
EXPORT_SYMBOL_GPL(sgm41513_set_prechg_current);

int sgm41513_set_chargevolt(struct sgm41513 *sgm, int volt)
{
	u8 val;
	
	if (volt < SGM41513_REG04_VREG_BASE)
		volt = SGM41513_REG04_VREG_BASE;

	val = (volt - SGM41513_REG04_VREG_BASE) / SGM41513_REG04_VREG_LSB;
	return sgm41513_update_bits(sgm, SGM41513_REG_04, SGM41513_REG04_VREG_MASK, 
				val << SGM41513_REG04_VREG_SHIFT);
}


int sgm41513_set_input_volt_limit(struct sgm41513 *sgm, int volt)
{
	u8 val;

	if (volt < SGM41513_REG06_VINDPM_BASE)
		volt = SGM41513_REG06_VINDPM_BASE;

	val = (volt - SGM41513_REG06_VINDPM_BASE) / SGM41513_REG06_VINDPM_LSB;
	return sgm41513_update_bits(sgm, SGM41513_REG_06, SGM41513_REG06_VINDPM_MASK, 
				val << SGM41513_REG06_VINDPM_SHIFT);
}

int sgm41513_set_input_current_limit(struct sgm41513 *sgm, int curr)
{
	u8 val;

	if (curr < SGM41513_REG00_IINLIM_BASE)
		curr = SGM41513_REG00_IINLIM_BASE;

	val = (curr - SGM41513_REG00_IINLIM_BASE) / SGM41513_REG00_IINLIM_LSB;
	return sgm41513_update_bits(sgm, SGM41513_REG_00, SGM41513_REG00_IINLIM_MASK, 
				val << SGM41513_REG00_IINLIM_SHIFT);
}


int sgm41513_set_watchdog_timer(struct sgm41513 *sgm, u8 timeout)
{
	u8 temp;

	temp = (u8)(((timeout - SGM41513_REG05_WDT_BASE) / SGM41513_REG05_WDT_LSB) << SGM41513_REG05_WDT_SHIFT);

	return sgm41513_update_bits(sgm, SGM41513_REG_05, SGM41513_REG05_WDT_MASK, temp); 
}
EXPORT_SYMBOL_GPL(sgm41513_set_watchdog_timer);

int sgm41513_disable_watchdog_timer(struct sgm41513 *sgm)
{
	u8 val = SGM41513_REG05_WDT_DISABLE << SGM41513_REG05_WDT_SHIFT;

	return sgm41513_update_bits(sgm, SGM41513_REG_05, SGM41513_REG05_WDT_MASK, val);
}
EXPORT_SYMBOL_GPL(sgm41513_disable_watchdog_timer);

int sgm41513_reset_watchdog_timer(struct sgm41513 *sgm)
{
	u8 val = SGM41513_REG01_WDT_RESET << SGM41513_REG01_WDT_RESET_SHIFT;

	return sgm41513_update_bits(sgm, SGM41513_REG_01, SGM41513_REG01_WDT_RESET_MASK, val);
}
EXPORT_SYMBOL_GPL(sgm41513_reset_watchdog_timer);

int sgm41513_reset_chip(struct sgm41513 *sgm)
{
	int ret;
	u8 val = SGM41513_REG0B_RESET << SGM41513_REG0B_RESET_SHIFT;

	ret = sgm41513_update_bits(sgm, SGM41513_REG_0B, SGM41513_REG0B_RESET_MASK, val);
	return ret;
}
EXPORT_SYMBOL_GPL(sgm41513_reset_chip);

int sgm41513_enter_hiz_mode(struct sgm41513 *sgm)
{
	u8 val = SGM41513_REG00_HIZ_ENABLE << SGM41513_REG00_ENHIZ_SHIFT;

	return sgm41513_update_bits(sgm, SGM41513_REG_00, SGM41513_REG00_ENHIZ_MASK, val);

}
EXPORT_SYMBOL_GPL(sgm41513_enter_hiz_mode);

int sgm41513_exit_hiz_mode(struct sgm41513 *sgm)
{

	u8 val = SGM41513_REG00_HIZ_DISABLE << SGM41513_REG00_ENHIZ_SHIFT;

	return sgm41513_update_bits(sgm, SGM41513_REG_00, SGM41513_REG00_ENHIZ_MASK, val);

}
EXPORT_SYMBOL_GPL(sgm41513_exit_hiz_mode);

int sgm41513_get_hiz_mode(struct sgm41513 *sgm, u8 *state)
{
	u8 val;
	int ret;

	ret = sgm41513_read_byte(sgm, &val, SGM41513_REG_00);
	if (ret)
		return ret;
	*state = (val & SGM41513_REG00_ENHIZ_MASK) >> SGM41513_REG00_ENHIZ_SHIFT;

	return 0;
}
EXPORT_SYMBOL_GPL(sgm41513_get_hiz_mode);


static int sgm41513_enable_term(struct sgm41513* sgm, bool enable)
{
	u8 val;
	int ret;

	if (enable)
		val = SGM41513_REG05_TERM_ENABLE << SGM41513_REG05_EN_TERM_SHIFT;
	else
		val = SGM41513_REG05_TERM_DISABLE << SGM41513_REG05_EN_TERM_SHIFT;

	ret = sgm41513_update_bits(sgm, SGM41513_REG_05, SGM41513_REG05_EN_TERM_MASK, val);

	return ret;
}
EXPORT_SYMBOL_GPL(sgm41513_enable_term);

int sgm41513_set_boost_current(struct sgm41513 *sgm, int curr)
{
	u8 val;

	val = SGM41513_REG02_BOOST_LIM_1P2A;
	if (curr == BOOSTI_500)
		val = SGM41513_REG02_BOOST_LIM_0P5A;

	return sgm41513_update_bits(sgm, SGM41513_REG_02, SGM41513_REG02_BOOST_LIM_MASK, 
				val << SGM41513_REG02_BOOST_LIM_SHIFT);
}

int sgm41513_set_boost_voltage(struct sgm41513 *sgm, int volt)
{
	u8 val;

  val = SGM41513_REG06_BOOSTV_5P15V;
	if (volt == BOOSTV_4850)
		val = SGM41513_REG06_BOOSTV_4P85V;
	else if (volt == BOOSTV_5150)
		val = SGM41513_REG06_BOOSTV_5P15V;
	else if (volt == BOOSTV_5300)
		val = SGM41513_REG06_BOOSTV_5P3V;
	else
		val = SGM41513_REG06_BOOSTV_5P15V;

	return sgm41513_update_bits(sgm, SGM41513_REG_06, SGM41513_REG06_BOOSTV_MASK, 
				val << SGM41513_REG06_BOOSTV_SHIFT);
}
EXPORT_SYMBOL_GPL(sgm41513_set_boost_voltage);

static int sgm41513_set_acovp_threshold(struct sgm41513 *sgm, int volt)
{
	u8 val;
  
  val = SGM41513_REG06_OVP_10P5V;
	if (volt == VAC_OVP_14300)
		val = SGM41513_REG06_OVP_14V;
	else if (volt == VAC_OVP_10500)
		val = SGM41513_REG06_OVP_10P5V;
	else if (volt == VAC_OVP_6500)
		val = SGM41513_REG06_OVP_6P5V;
	else
		val = SGM41513_REG06_OVP_10P5V;

	return sgm41513_update_bits(sgm, SGM41513_REG_06, SGM41513_REG06_OVP_MASK, 
				val << SGM41513_REG06_OVP_SHIFT);
}
EXPORT_SYMBOL_GPL(sgm41513_set_acovp_threshold);

static int sgm41513_set_stat_ctrl(struct sgm41513 *sgm, int ctrl)
{
	u8 val;

	val = ctrl;

	return sgm41513_update_bits(sgm, SGM41513_REG_00, SGM41513_REG00_STAT_CTRL_MASK, 
				val << SGM41513_REG00_STAT_CTRL_SHIFT);
}


static int sgm41513_set_int_mask(struct sgm41513 *sgm, int mask)
{
	u8 val;

	val = mask;

	return sgm41513_update_bits(sgm, SGM41513_REG_0A, SGM41513_REG0A_INT_MASK_MASK,
				val << SGM41513_REG0A_INT_MASK_SHIFT);
}


static int sgm41513_enable_batfet(struct sgm41513 *sgm)
{
	const u8 val = SGM41513_REG07_BATFET_ON << SGM41513_REG07_BATFET_DIS_SHIFT;

	return sgm41513_update_bits(sgm, SGM41513_REG_07, SGM41513_REG07_BATFET_DIS_MASK,
				val);
}
EXPORT_SYMBOL_GPL(sgm41513_enable_batfet);


static int sgm41513_disable_batfet(struct sgm41513 *sgm)
{
	const u8 val = SGM41513_REG07_BATFET_OFF << SGM41513_REG07_BATFET_DIS_SHIFT;

	return sgm41513_update_bits(sgm, SGM41513_REG_07, SGM41513_REG07_BATFET_DIS_MASK,
				val);
}
EXPORT_SYMBOL_GPL(sgm41513_disable_batfet);

static int sgm41513_set_batfet_delay(struct sgm41513 *sgm, uint8_t delay)
{
	u8 val;

	if (delay == 0)
		val = SGM41513_REG07_BATFET_DLY_0S;
	else
		val = SGM41513_REG07_BATFET_DLY_10S;
	
	val <<= SGM41513_REG07_BATFET_DLY_SHIFT;

	return sgm41513_update_bits(sgm, SGM41513_REG_07, SGM41513_REG07_BATFET_DLY_MASK,
								val);
}
EXPORT_SYMBOL_GPL(sgm41513_set_batfet_delay);

static int sgm41513_set_vdpm_bat_track(struct sgm41513 *sgm)
{
	const u8 val = SGM41513_REG07_VDPM_BAT_TRACK_200MV << SGM41513_REG07_VDPM_BAT_TRACK_SHIFT;

	return sgm41513_update_bits(sgm, SGM41513_REG_07, SGM41513_REG07_VDPM_BAT_TRACK_MASK,
				val);
}
EXPORT_SYMBOL_GPL(sgm41513_set_vdpm_bat_track);

static int sgm41513_enable_safety_timer(struct sgm41513 *sgm)
{
	const u8 val = SGM41513_REG05_CHG_TIMER_ENABLE << SGM41513_REG05_EN_TIMER_SHIFT;
	
	return sgm41513_update_bits(sgm, SGM41513_REG_05, SGM41513_REG05_EN_TIMER_MASK,
				val);
}
EXPORT_SYMBOL_GPL(sgm41513_enable_safety_timer);


static int sgm41513_disable_safety_timer(struct sgm41513 *sgm)
{
	const u8 val = SGM41513_REG05_CHG_TIMER_DISABLE << SGM41513_REG05_EN_TIMER_SHIFT;
	
	return sgm41513_update_bits(sgm, SGM41513_REG_05, SGM41513_REG05_EN_TIMER_MASK,
				val);
}
EXPORT_SYMBOL_GPL(sgm41513_disable_safety_timer);

static struct sgm41513_platform_data* sgm41513_parse_dt(struct device *dev, 
							struct sgm41513 * sgm)
{
    int ret;
    struct device_node *np = dev->of_node;
	struct sgm41513_platform_data* pdata;
	
	pdata = devm_kzalloc(dev, sizeof(struct sgm41513_platform_data), 
						GFP_KERNEL);
	if (!pdata) {
		pr_err("Out of memory\n");
		return NULL;
	}
#if 0	
	ret = of_property_read_u32(np, "sgm,sgm41513,chip-enable-gpio", &sgm->gpio_ce);
    if(ret) {
		pr_err("Failed to read node of sgm,sgm41513,chip-enable-gpio\n");
	}
#endif

    ret = of_property_read_u32(np,"sgm,sgm41513,usb-vlim",&pdata->usb.vlim);
    if(ret) {
		pr_err("Failed to read node of sgm,sgm41513,usb-vlim\n");
	}

    ret = of_property_read_u32(np,"sgm,sgm41513,usb-ilim",&pdata->usb.ilim);
    if(ret) {
		pr_err("Failed to read node of sgm,sgm41513,usb-ilim\n");
	}
	
    ret = of_property_read_u32(np,"sgm,sgm41513,usb-vreg",&pdata->usb.vreg);
    if(ret) {
		pr_err("Failed to read node of sgm,sgm41513,usb-vreg\n");
	}

    ret = of_property_read_u32(np,"sgm,sgm41513,usb-ichg",&pdata->usb.ichg);
    if(ret) {
		pr_err("Failed to read node of sgm,sgm41513,usb-ichg\n");
	}

    ret = of_property_read_u32(np,"sgm,sgm41513,stat-pin-ctrl",&pdata->statctrl);
    if(ret) {
		pr_err("Failed to read node of sgm,sgm41513,stat-pin-ctrl\n");
	}
	
    ret = of_property_read_u32(np,"sgm,sgm41513,precharge-current",&pdata->iprechg);
    if(ret) {
		pr_err("Failed to read node of sgm,sgm41513,precharge-current\n");
	}

    ret = of_property_read_u32(np,"sgm,sgm41513,termination-current",&pdata->iterm);
    if(ret) {
		pr_err("Failed to read node of sgm,sgm41513,termination-current\n");
	}
	
    ret = of_property_read_u32(np,"sgm,sgm41513,boost-voltage",&pdata->boostv);
    if(ret) {
		pr_err("Failed to read node of sgm,sgm41513,boost-voltage\n");
	}

    ret = of_property_read_u32(np,"sgm,sgm41513,boost-current",&pdata->boosti);
    if(ret) {
		pr_err("Failed to read node of sgm,sgm41513,boost-current\n");
	}

    ret = of_property_read_u32(np,"sgm,sgm41513,vac-ovp-threshold",&pdata->vac_ovp);
    if(ret) {
		pr_err("Failed to read node of sgm,sgm41513,vac-ovp-threshold\n");
	}

    return pdata;   
}

static int sgm41513_init_device(struct sgm41513 *sgm)
{
	int ret;
	
	sgm41513_disable_watchdog_timer(sgm);

	sgm41513_set_vdpm_bat_track(sgm);

	sgm41513_disable_safety_timer(sgm);
	
	ret = sgm41513_set_stat_ctrl(sgm, sgm->platform_data->statctrl);
	if (ret)
		pr_err("Failed to set stat pin control mode, ret = %d\n",ret);

	ret = sgm41513_set_prechg_current(sgm, sgm->platform_data->iprechg);
	if (ret)
		pr_err("Failed to set prechg current, ret = %d\n",ret);
	
	ret = sgm41513_set_term_current(sgm, sgm->platform_data->iterm);
	if (ret)
		pr_err("Failed to set termination current, ret = %d\n",ret);
	
	ret = sgm41513_set_boost_voltage(sgm, sgm->platform_data->boostv);
	if (ret)
		pr_err("Failed to set boost voltage, ret = %d\n",ret);
	
	ret = sgm41513_set_boost_current(sgm, sgm->platform_data->boosti);
	if (ret)
		pr_err("Failed to set boost current, ret = %d\n",ret);
	
	ret = sgm41513_set_acovp_threshold(sgm, sgm->platform_data->vac_ovp);
	if (ret)
		pr_err("Failed to set acovp threshold, ret = %d\n",ret);	

	ret = sgm41513_set_int_mask(sgm, SGM41513_REG0A_IINDPM_INT_MASK | SGM41513_REG0A_VINDPM_INT_MASK);
	if (ret)
		pr_err("Failed to set vindpm and iindpm int mask\n");
	sgm41513_update_bits(sgm, SGM41513_REG_0D, 0xFF, 0);
	return 0;
}


static int sgm41513_detect_device(struct sgm41513* sgm)
{
    int ret;
    u8 data;

    ret = sgm41513_read_byte(sgm, &data, SGM41513_REG_0B);
    if(ret == 0){
        sgm->part_no = (data & SGM41513_REG0B_PN_MASK) >> SGM41513_REG0B_PN_SHIFT;
        sgm->revision = (data & SGM41513_REG0B_DEV_REV_MASK) >> SGM41513_REG0B_DEV_REV_SHIFT;
    }

    pr_err("[%s] part_no=%d revision=%d\n", __func__, sgm->part_no, sgm->revision);

    if(sgm->part_no != SGM41513) 
		return 1;
    return ret;
}

static void sgm41513_dump_regs(struct sgm41513 *sgm)
{
	int addr;
	u8 val;
	int ret;

	for (addr = 0x0; addr <= 0x0B; addr++) {
		ret = sgm41513_read_byte(sgm, &val, addr);
		if (ret == 0)
			pr_err("Reg[%.2x] = 0x%.2x\n", addr, val);
	}


}

static ssize_t sgm41513_show_registers(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct sgm41513 *sgm = dev_get_drvdata(dev);
	u8 addr;
	u8 val;
	u8 tmpbuf[200];
	int len;
	int idx = 0;
	int ret ;

	idx = snprintf(buf, PAGE_SIZE, "%s:\n", "sgm41513 Reg");
	for (addr = 0x0; addr <= 0x0B; addr++) {
		ret = sgm41513_read_byte(sgm, &val, addr);
		if (ret == 0) {
			len = snprintf(tmpbuf, PAGE_SIZE - idx,"Reg[%.2x] = 0x%.2x\n", addr, val);
			memcpy(&buf[idx], tmpbuf, len);
			idx += len;
		}
	}

	return idx;
}

static ssize_t sgm41513_store_registers(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct sgm41513 *sgm = dev_get_drvdata(dev);
	int ret;
	unsigned int reg;
	unsigned int val;

	ret = sscanf(buf, "%x %x", &reg, &val);
	if (ret == 2 && reg < 0x0B) {
		sgm41513_write_byte(sgm, (unsigned char)reg, (unsigned char)val);
	}

	return count;
}

static DEVICE_ATTR(registers, S_IRUGO | S_IWUSR, sgm41513_show_registers, sgm41513_store_registers);

static struct attribute *sgm41513_attributes[] = {
	&dev_attr_registers.attr,
	NULL,
};

static const struct attribute_group sgm41513_attr_group = {
	.attrs = sgm41513_attributes,
};

static int sgm41513_set_hizmode(struct charger_device *chg_dev, bool enable)
{

	struct sgm41513 *sgm = dev_get_drvdata(&chg_dev->dev);
	int ret = 0;
	u8 tmp;

	if (enable)
  	{
		ret = sgm41513_enter_hiz_mode(sgm);
  	}
	else
  	{
		ret = sgm41513_exit_hiz_mode(sgm);
   	}

	ret = sgm41513_read_byte(sgm, &tmp, SGM41513_REG_00);

	pr_err("set hiz mode i2c  read reg 0x%02X is 0x%02X  ret :%d\n",SGM41513_REG_00,tmp,ret);

	pr_err("%s set hizmode %s\n", enable ? "enable" : "disable",
				  !ret ? "successfully" : "failed");

	return ret;
}

static int sgm41513_charging(struct charger_device *chg_dev, bool enable)
{

	struct sgm41513 *sgm = dev_get_drvdata(&chg_dev->dev);
	int ret = 0,rc = 0;
	u8 val;

	if (enable)
  	{
		ret = sgm41513_enable_charger(sgm);
  	}
	else
  	{
		ret = sgm41513_disable_charger(sgm);
   	}
	pr_err("%s charger %s\n", enable ? "enable" : "disable",
				  !ret ? "successfully" : "failed");

	ret = sgm41513_read_byte(sgm, &val, SGM41513_REG_01);
	if (!ret && !rc )
		sgm->charge_enabled = !!(val & SGM41513_REG01_CHG_CONFIG_MASK);

	return ret;
}

static int sgm41513_plug_in(struct charger_device *chg_dev)
{

	int ret;
	
	pr_info("[%s] enter!", __func__);
	ret = sgm41513_charging(chg_dev, true);

	if (!ret)
		pr_err("Failed to enable charging:%d\n", ret);
	
	return ret;
}

static int sgm41513_plug_out(struct charger_device *chg_dev)
{
	int ret;

	pr_info("[%s] enter!", __func__);
	ret = sgm41513_charging(chg_dev, false);

	if (!ret)
		pr_err("Failed to disable charging:%d\n", ret);
	
	return ret;
}

static int sgm41513_dump_register(struct charger_device *chg_dev)
{
	struct sgm41513 *sgm = dev_get_drvdata(&chg_dev->dev);

	sgm41513_dump_regs(sgm);

	return 0;
}

static int sgm41513_is_charging_enable(struct charger_device *chg_dev, bool *en)
{
	struct sgm41513 *sgm = dev_get_drvdata(&chg_dev->dev);
	
	*en = sgm->charge_enabled;
	
	return 0;
}

static int sgm41513_is_charging_done(struct charger_device *chg_dev, bool *done)
{
	struct sgm41513 *sgm = dev_get_drvdata(&chg_dev->dev);
	int ret;
	u8 val;
	
	ret = sgm41513_read_byte(sgm, &val, SGM41513_REG_08);
	if (!ret) {
		val = val & SGM41513_REG08_CHRG_STAT_MASK;
		val = val >> SGM41513_REG08_CHRG_STAT_SHIFT;
		*done = (val == SGM41513_REG08_CHRG_STAT_CHGDONE);	
	}
	
	return ret;
}

static int sgm41513_set_ichg(struct charger_device *chg_dev, u32 curr)
{
	struct sgm41513 *sgm = dev_get_drvdata(&chg_dev->dev);
	/*
	u8 reg_val;
	int ichg_bef;
	int ret;

	ret = sgm41513_read_byte(sgm, &reg_val, SGM41513_REG_02);
	if (!ret) {
		ichg_bef = (reg_val & SGM41513_REG02_ICHG_MASK) >> SGM41513_REG02_ICHG_SHIFT;
		ichg_bef = ichg_bef * SGM41513_REG02_ICHG_LSB + SGM41513_REG02_ICHG_BASE;
		if ((ichg_bef <= curr / 1000) &&
			(ichg_bef + SGM41513_REG02_ICHG_LSB > curr / 1000)) {
			pr_info("[%s] current has set! [%d]\n", __func__, curr);
			return ret;
		}
	}

	pr_info("[%s] curr=%d, ichg_bef = %d\n", __func__, curr, ichg_bef);
	*/
   if(sgm != NULL)
		return sgm41513_set_chargecurrent(sgm, curr/1000);
   else 
   		return -1;
}

static int sgm41513_get_ichg(struct charger_device *chg_dev, u32 *curr)
{
	struct sgm41513 *sgm = dev_get_drvdata(&chg_dev->dev);
	u8 reg_val;
	int reg_ichg;
	int ret;

	ret = sgm41513_read_byte(sgm, &reg_val, SGM41513_REG_02);
	if (!ret) {
		reg_ichg = (reg_val & SGM41513_REG02_ICHG_MASK) >> SGM41513_REG02_ICHG_SHIFT;
		/* ichg = ichg * SGM41513_REG02_ICHG_LSB + SGM41513_REG02_ICHG_BASE; */
	pr_info("%s reg_ichg = %x \n", __func__, reg_ichg);
	if(reg_ichg <= 8)
		reg_ichg = 5*reg_ichg;
	else if(reg_ichg <= 15)
		reg_ichg = 10*(reg_ichg - 8) + 40;
	else if(reg_ichg <= 23)
		reg_ichg = 20*(reg_ichg - 15) + 110;
	else if(reg_ichg <= 32)
		reg_ichg = 30*(reg_ichg - 23) + 270;
	else if(reg_ichg <= 48)
		reg_ichg = 60*(reg_ichg - 32) + 540;
	else if(reg_ichg <= 60)
		reg_ichg = 120*(reg_ichg - 48) + 1500;
	else 
		reg_ichg = 3000;
	}
	*curr = reg_ichg * 1000;
	pr_info("[%s] curr=%d\n", __func__, *curr);

	return ret;
}

static int sgm41513_get_min_ichg(struct charger_device *chg_dev, u32 *curr)
{

	*curr = 0; /* 60 * 1000; */

	return 0;
}

static int sgm41513_set_vchg(struct charger_device *chg_dev, u32 volt)
{
	
	struct sgm41513 *sgm = dev_get_drvdata(&chg_dev->dev);

	return sgm41513_set_chargevolt(sgm, volt/1000);	
}

static int sgm41513_get_vchg(struct charger_device *chg_dev, u32 *volt)
{
	struct sgm41513 *sgm = dev_get_drvdata(&chg_dev->dev);
	u8 reg_val;
	int vchg;
	int ret;

	ret = sgm41513_read_byte(sgm, &reg_val, SGM41513_REG_04);
	if (!ret) {
		vchg = (reg_val & SGM41513_REG04_VREG_MASK) >> SGM41513_REG04_VREG_SHIFT;
		vchg = vchg * SGM41513_REG04_VREG_LSB + SGM41513_REG04_VREG_BASE;
		*volt = vchg * 1000;
	}

	return ret;
}

static int sgm41513_set_ivl(struct charger_device *chg_dev, u32 volt)
{

	struct sgm41513 *sgm = dev_get_drvdata(&chg_dev->dev);

	return sgm41513_set_input_volt_limit(sgm, volt/1000);

}

static int sgm41513_set_icl(struct charger_device *chg_dev, u32 curr)
{

	struct sgm41513 *sgm = dev_get_drvdata(&chg_dev->dev);
	return sgm41513_set_input_current_limit(sgm, curr/1000);
}

static int sgm41513_get_icl(struct charger_device *chg_dev, u32 *curr)
{
	struct sgm41513 *sgm = dev_get_drvdata(&chg_dev->dev);
	u8 reg_val;
	int icl;
	int ret;

	ret = sgm41513_read_byte(sgm, &reg_val, SGM41513_REG_00);
	if (!ret) {
		icl = (reg_val & SGM41513_REG00_IINLIM_MASK) >> SGM41513_REG00_IINLIM_SHIFT;
		icl = icl * SGM41513_REG00_IINLIM_LSB + SGM41513_REG00_IINLIM_BASE;
		*curr = icl * 1000;
	}

	return ret;

}
 

static int sgm41513_kick_wdt(struct charger_device *chg_dev)
{
	struct sgm41513 *sgm = dev_get_drvdata(&chg_dev->dev);

	return sgm41513_reset_watchdog_timer(sgm);
}

static int sgm41513_set_otg(struct charger_device *chg_dev, bool en)
{
	int ret;
	struct sgm41513 *sgm = dev_get_drvdata(&chg_dev->dev);
	
	if (en)
  	{ 
		sgm41513_otg_flag = 1;
		ret = sgm41513_disable_charger(sgm);
		ret = sgm41513_enable_otg(sgm);
   	}
	else
  	{ 
		sgm41513_otg_flag = 0;
		ret = sgm41513_disable_otg(sgm);
		ret = sgm41513_enable_charger(sgm);
   	}
	return ret;
}

static int sgm41513_set_safety_timer(struct charger_device *chg_dev, bool en)
{
	struct sgm41513 *sgm = dev_get_drvdata(&chg_dev->dev);
	int ret;
		
	if (en)
		ret = sgm41513_enable_safety_timer(sgm);
	else
		ret = sgm41513_disable_safety_timer(sgm);
		
	return ret;
}

static int sgm41513_is_safety_timer_enabled(struct charger_device *chg_dev, bool *en)
{
	struct sgm41513 *sgm = dev_get_drvdata(&chg_dev->dev);
	int ret;
	u8 reg_val;

	ret = sgm41513_read_byte(sgm, &reg_val, SGM41513_REG_05);

	if (!ret) 
		*en = !!(reg_val & SGM41513_REG05_EN_TIMER_MASK);
	
	return ret;
}


static int sgm41513_set_boost_ilmt(struct charger_device *chg_dev, u32 curr)
{
	struct sgm41513 *sgm = dev_get_drvdata(&chg_dev->dev);
	int ret;

	ret = sgm41513_set_boost_current(sgm, curr/1000);

	return ret;
}

static int sgm41513_do_event(struct charger_device *chg_dev, u32 event,
			    u32 args)
{
	if (chg_dev == NULL)
		return -EINVAL;

	pr_info("%s: event = %d\n", __func__, event);
	switch (event) {
	case EVENT_EOC:
	case EVENT_FULL:
		charger_dev_notify(chg_dev, CHARGER_DEV_NOTIFY_EOC);
		break;
	case EVENT_RECHARGE:
		charger_dev_notify(chg_dev, CHARGER_DEV_NOTIFY_RECHG);
		break;
	case EVENT_DISCHARGE:
		charger_dev_notify(chg_dev, CHARGER_DEV_NOTIFY_DISCHARGE);
		break;
	default:
		break;
	}

	return 0;
}

/* 2022.6.3 longcheer zhangfeng5 add. support set/get_cv step-8mV begin */
#define SGM41513_REG_0F 0x0f
#define SGM41513_VREG_FT_MASK GENMASK(7, 6)
static int sgm41513_set_vchg_step8(struct charger_device *chg_dev, u32 chrg_volt)
{
	u8 reg_val,vreg_ft;
	struct sgm41513 *sgm = charger_get_data(chg_dev);
	u32 chrg_volt_mV;

	chrg_volt_mV = chrg_volt / 1000;
	if (chrg_volt_mV < SGM41513_REG04_VREG_BASE)
		chrg_volt_mV = SGM41513_REG04_VREG_BASE;
	else if (chrg_volt_mV > SGM41513_REG04_VREG_MAX)
		chrg_volt_mV = SGM41513_REG04_VREG_MAX;

	vreg_ft = (chrg_volt_mV-SGM41513_REG04_VREG_BASE) % SGM41513_REG04_VREG_LSB;
	if (vreg_ft >= 24){
		/* +32mV, -8mV */
		reg_val = (chrg_volt_mV-SGM41513_REG04_VREG_BASE) / SGM41513_REG04_VREG_LSB;
		reg_val = (reg_val+1)<<3;
		sgm41513_update_bits(sgm, SGM41513_REG_04,SGM41513_REG04_VREG_MASK, reg_val);
		sgm41513_update_bits(sgm, SGM41513_REG_0F,SGM41513_VREG_FT_MASK, 2<<6);
		pr_err("[step_8]: sgm set reg0f.bit[7:6]: 0x%x.\n", 2);
	}else if(vreg_ft >= 16){
		/* +32mV, -16mV */
		reg_val = (chrg_volt_mV-SGM41513_REG04_VREG_BASE) / SGM41513_REG04_VREG_LSB;
		reg_val = (reg_val+1)<<3;
		sgm41513_update_bits(sgm, SGM41513_REG_04,SGM41513_REG04_VREG_MASK, reg_val);
		sgm41513_update_bits(sgm, SGM41513_REG_0F,SGM41513_VREG_FT_MASK, 3<<6);
		pr_err("[step_8]: sgm set reg0f.bit[7:6]: 0x%x.\n", 3);
	}else if(vreg_ft >= 8){
		/* +8mV */
		reg_val = (chrg_volt_mV-SGM41513_REG04_VREG_BASE) / SGM41513_REG04_VREG_LSB;
		reg_val = reg_val<<3;
		sgm41513_update_bits(sgm, SGM41513_REG_04,SGM41513_REG04_VREG_MASK, reg_val);
		sgm41513_update_bits(sgm, SGM41513_REG_0F,SGM41513_VREG_FT_MASK, 1<<6);
		pr_err("[step_8]: sgm set reg0f.bit[7:6]: 0x%x.\n", 1);
	}else{
		reg_val = (chrg_volt_mV-SGM41513_REG04_VREG_BASE) / SGM41513_REG04_VREG_LSB;
		reg_val = reg_val<<3;
		sgm41513_update_bits(sgm, SGM41513_REG_04,SGM41513_REG04_VREG_MASK, reg_val);
		sgm41513_update_bits(sgm, SGM41513_REG_0F,SGM41513_VREG_FT_MASK, 0<<6);
		pr_err("[step_8]: sgm set reg0f.bit[7:6]: 0x%x.\n", 0);
	}
	pr_err("[step_8]: sgm set reg04.bit[7:3]: 0x%x.\n", reg_val>>3);
	pr_err("[step_8]: sgm set valt: %d.\n", chrg_volt);
	return 0;
}

static int sgm41513_get_vchg_step8(struct charger_device *chg_dev,u32 *volt)
{
	int ret;
	u8 vreg_val;
	u8 vreg_ft;
	u32 vol_temp;
	struct sgm41513 *sgm = charger_get_data(chg_dev);

	ret = sgm41513_read_byte(sgm, &vreg_val, SGM41513_REG_04);
	if (ret)
		return ret;

	ret = sgm41513_read_byte(sgm, &vreg_ft, SGM41513_REG_0F);
	if (ret)
		return ret;

	vreg_val = (vreg_val & SGM41513_REG04_VREG_MASK)>>3;
	vreg_ft = (vreg_ft & SGM41513_VREG_FT_MASK)>>6;

	if (15 == vreg_val)
		vol_temp = 4352; //default
	else if (vreg_val < 25)
		vol_temp = vreg_val*SGM41513_REG04_VREG_LSB + SGM41513_REG04_VREG_BASE;

	if(vreg_ft == 1)
		vol_temp = vol_temp + 8;
	else if(vreg_ft == 2)
		vol_temp = vol_temp - 8;
	else if(vreg_ft == 3)
		vol_temp = vol_temp - 16;
	else
		vol_temp = vol_temp;
	*volt = vol_temp * 1000;
	pr_err("[step_8]: sgm get reg04.bit[7:3]: 0x%x.\n", vreg_val);
	pr_err("[step_8]: sgm get reg0f.bit[7:6]: 0x%x.\n", vreg_ft);
	pr_err("[step_8]: sgm get valt: %d.\n", *volt);

	return 0;
}
/* 2022.6.3 longcheer zhangfeng5 add. support set/get_cv step-8mV end */


static struct charger_ops sgm41513_chg_ops = {
	/* Normal charging */
  .hiz_mode = sgm41513_set_hizmode,
	.plug_in = sgm41513_plug_in,
	.plug_out = sgm41513_plug_out,
	.dump_registers = sgm41513_dump_register,
	.enable = sgm41513_charging,
	.is_enabled = sgm41513_is_charging_enable,
	.get_charging_current = sgm41513_get_ichg,
	.set_charging_current = sgm41513_set_ichg,
	.get_input_current = sgm41513_get_icl,
	.set_input_current = sgm41513_set_icl,
	.get_constant_voltage = sgm41513_get_vchg_step8,
	.set_constant_voltage = sgm41513_set_vchg_step8,
	.kick_wdt = sgm41513_kick_wdt,
	.set_mivr = sgm41513_set_ivl,
	.is_charging_done = sgm41513_is_charging_done,
	.get_min_charging_current = sgm41513_get_min_ichg,

	/* Safety timer */
	.enable_safety_timer = sgm41513_set_safety_timer,
	.is_safety_timer_enabled = sgm41513_is_safety_timer_enabled,

	/* Power path */
	.enable_powerpath = NULL,
	.is_powerpath_enabled = NULL,

	/* OTG */
	.enable_otg = sgm41513_set_otg,
	.set_boost_current_limit = sgm41513_set_boost_ilmt,
	.enable_discharge = NULL,
	.event = sgm41513_do_event,

	/* PE+/PE+20 */
	.send_ta_current_pattern = NULL,
	.set_pe20_efficiency_table = NULL,
	.send_ta20_current_pattern = NULL,
//	.set_ta20_reset = NULL,
	.enable_cable_drop_comp = NULL,

	/* ADC */
	.get_tchg_adc = NULL,
};



static int sgm41513_charger_probe(struct i2c_client *client, 
					const struct i2c_device_id *id)
{
	struct sgm41513 *sgm;

	int ret;
	

	sgm = devm_kzalloc(&client->dev, sizeof(struct sgm41513), GFP_KERNEL);
	if (!sgm) {
		pr_err("Out of memory\n");
		return -ENOMEM;
	}

	sgm->dev = &client->dev;
	sgm->client = client;

	i2c_set_clientdata(client, sgm);
	
	mutex_init(&sgm->i2c_rw_lock);
	
	ret = sgm41513_detect_device(sgm);
	if(ret) {
		pr_err("No sgm41513 device found!\n");
		return -ENODEV;
	}
	
	if (client->dev.of_node)
		sgm->platform_data = sgm41513_parse_dt(&client->dev, sgm);
	else
		sgm->platform_data = client->dev.platform_data;
	
	if (!sgm->platform_data) {
		pr_err("No platform data provided.\n");
		return -EINVAL;
	}

	sgm->chg_dev = charger_device_register("primary_chg",
			&client->dev, sgm, 
			&sgm41513_chg_ops,
			&sgm41513_chg_props);
	if (IS_ERR_OR_NULL(sgm->chg_dev)) {
		ret = PTR_ERR(sgm->chg_dev);
		goto err_0;
	}

	ret = sgm41513_init_device(sgm);
	if (ret) {
		pr_err("Failed to init device\n");
		return ret;
	}

	
	ret = sysfs_create_group(&sgm->dev->kobj, &sgm41513_attr_group);
	if (ret) {
		dev_err(sgm->dev, "failed to register sysfs. err: %d\n", ret);
	}


	pr_err("sgm41513 probe successfully, Part Num:%d, Revision:%d\n!", 
				sgm->part_no, sgm->revision);
	
	return 0;
	
err_0:
	
	return ret;
}

static int sgm41513_charger_remove(struct i2c_client *client)
{
	struct sgm41513 *sgm = i2c_get_clientdata(client);


	mutex_destroy(&sgm->i2c_rw_lock);

	sysfs_remove_group(&sgm->dev->kobj, &sgm41513_attr_group);


	return 0;
}


static void sgm41513_charger_shutdown(struct i2c_client *client)
{
}

static struct of_device_id sgm41513_charger_match_table[] = {
	{.compatible = "sgm,sgm41513_charger",},
	{},
};
MODULE_DEVICE_TABLE(of,sgm41513_charger_match_table);

static const struct i2c_device_id sgm41513_charger_id[] = {
	{ "sgm41513-charger", SGM41513 },
	{},
};
MODULE_DEVICE_TABLE(i2c, sgm41513_charger_id);

static struct i2c_driver sgm41513_charger_driver = {
	.driver 	= {
		.name 	= "sgm41513-charger",
		.owner 	= THIS_MODULE,
		.of_match_table = sgm41513_charger_match_table,
	},
	.id_table	= sgm41513_charger_id,
	
	.probe		= sgm41513_charger_probe,
	.remove		= sgm41513_charger_remove,
	.shutdown	= sgm41513_charger_shutdown,
	
};

module_i2c_driver(sgm41513_charger_driver);

MODULE_DESCRIPTION("SGM SGM41513 Charger Driver");
MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("ETA COMP.");
