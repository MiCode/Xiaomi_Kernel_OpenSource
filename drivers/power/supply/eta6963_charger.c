/*
 * BQ2560x battery charging driver
 *
 * Copyright (C) 2013 Texas Instruments
 *
 * This package is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundaetaon.
 * THIS PACKAGE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

#define pr_fmt(fmt)	"[eta6963]:%s: " fmt, __func__

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
#include "eta6963_charger.h"
#include "eta6963.h"

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

static u8 eta6963_otg_flag = 0;
enum eta6963_part_no {
	ETA6963 = 0x07,
};

enum eta6963_charge_state {
	CHARGE_STATE_IDLE = ETA6963_REG08_CHRG_STAT_IDLE,
	CHARGE_STATE_PRECHG = ETA6963_REG08_CHRG_STAT_PRECHG,
	CHARGE_STATE_FASTCHG = ETA6963_REG08_CHRG_STAT_FASTCHG,
	CHARGE_STATE_CHGDONE = ETA6963_REG08_CHRG_STAT_CHGDONE,
};


struct eta6963 {
	struct device *dev;
	struct i2c_client *client;

	enum eta6963_part_no part_no;
	int revision;

	int status;
	
	struct mutex i2c_rw_lock;

	bool charge_enabled;/* Register bit status */

	struct eta6963_platform_data* platform_data;
	struct charger_device *chg_dev;
	
};

static const struct charger_properties eta6963_chg_props = {
	.alias_name = "eta6963",
};

static int __eta6963_read_reg(struct eta6963* eta, u8 reg, u8 *data)
{
	s32 ret;

	ret = i2c_smbus_read_byte_data(eta->client, reg);
	if (ret < 0) {
		pr_err("i2c read fail: can't read from reg 0x%02X\n", reg);
		return ret;
	}
	*data = (u8)ret;
	
	return 0;
}

static int __eta6963_write_reg(struct eta6963* eta, int reg, u8 val)
{
	s32 ret;
	ret = i2c_smbus_write_byte_data(eta->client, reg, val);
   /*	pr_err("i2c write>>>>>>: write 0x%02X to reg 0x%02X ret: %d\n",
				val, reg, ret); */
	if (ret < 0) {
		pr_err("i2c write fail: can't write 0x%02X to reg 0x%02X: %d\n",
				val, reg, ret);
		return ret;
	}
	return 0;
}

static int eta6963_read_byte(struct eta6963 *eta, u8 *data, u8 reg)
{
	int ret;

	mutex_lock(&eta->i2c_rw_lock);
	ret = __eta6963_read_reg(eta, reg, data);
	mutex_unlock(&eta->i2c_rw_lock);

	return ret;
}


static int eta6963_write_byte(struct eta6963 *eta, u8 reg, u8 data)
{
	int ret;

	mutex_lock(&eta->i2c_rw_lock);
	ret = __eta6963_write_reg(eta, reg, data);
	mutex_unlock(&eta->i2c_rw_lock);

	if (ret) {
		pr_err("Failed: reg=%02X, ret=%d\n", reg, ret);
	}

	return ret;
}


static int eta6963_update_bits(struct eta6963 *eta, u8 reg, 
				u8 mask, u8 data)
{
	int ret;
	u8 tmp,tmp2;

	mutex_lock(&eta->i2c_rw_lock);
	ret = __eta6963_read_reg(eta, reg, &tmp);
	if (ret) {
		pr_err("Failed: reg=%02X, ret=%d\n", reg, ret);
		goto out;
	}
	tmp2=tmp;
	tmp &= ~mask;
	tmp |= data & mask;
	/*pr_info("tmp=%x,tmp2=%x",tmp,tmp2);*/
	if(tmp != tmp2)
		ret = __eta6963_write_reg(eta, reg, tmp);
	if (ret) {
		pr_err("Failed: reg=%02X, ret=%d\n", reg, ret);
	}
out:
	mutex_unlock(&eta->i2c_rw_lock);
	return ret;
}

static int eta6963_enable_otg(struct eta6963 *eta)
{
	u8 val = ETA6963_REG01_OTG_ENABLE << ETA6963_REG01_OTG_CHG_CONFIG_SHIFT;
    pr_info("eta6963_enable_otg enter\n");
	return eta6963_update_bits(eta, ETA6963_REG_01,
				ETA6963_REG01_OTG_CHG_CONFIG_MASK, val);

}

static int eta6963_disable_otg(struct eta6963 *eta)
{
	u8 val = ETA6963_REG01_OTG_DISABLE << ETA6963_REG01_OTG_CHG_CONFIG_SHIFT;
	pr_info("eta6963_disable_otg enter\n");
	return eta6963_update_bits(eta, ETA6963_REG_01,
				   ETA6963_REG01_OTG_CHG_CONFIG_MASK, val);

}

static int eta6963_enable_charger(struct eta6963 *eta)
{
	int i = 2, ret =0;
	u8 data = 0;
	if (eta6963_otg_flag == 0)
	{
		/*ret = eta6963_disable_otg(eta);*/
		while(i)
		{
		ret = eta6963_read_byte(eta, &data, ETA6963_REG_01);
			if(data & 0x20)
			{
				ret = eta6963_disable_otg(eta);
				i--;
			} else {
				break;
			}
		}
	}
	ret = eta6963_update_bits(eta, ETA6963_REG_01, ETA6963_REG01_CHG_CONFIG_MASK,
		ETA6963_REG01_CHG_ENABLE << ETA6963_REG01_CHG_CONFIG_SHIFT);
	return ret;
}

static int eta6963_disable_charger(struct eta6963 *eta)
{
	int ret;
	u8 val = ETA6963_REG01_CHG_DISABLE << ETA6963_REG01_CHG_CONFIG_SHIFT;

	ret = eta6963_update_bits(eta, ETA6963_REG_01, ETA6963_REG01_CHG_CONFIG_MASK, val);
	return ret;
}

int eta6963_set_chargecurrent(struct eta6963 *eta, int curr)
{
	u8 ichg;

	if (curr < ETA6963_REG02_ICHG_BASE)
		curr = ETA6963_REG02_ICHG_BASE;

	ichg = (curr - ETA6963_REG02_ICHG_BASE)/ETA6963_REG02_ICHG_LSB;
	return eta6963_update_bits(eta, ETA6963_REG_02, ETA6963_REG02_ICHG_MASK, 
				ichg << ETA6963_REG02_ICHG_SHIFT);

}

int eta6963_set_term_current(struct eta6963 *eta, int curr)
{
	u8 iterm;

	if (curr < ETA6963_REG03_ITERM_BASE)
		curr = ETA6963_REG03_ITERM_BASE;

	iterm = (curr - ETA6963_REG03_ITERM_BASE) / ETA6963_REG03_ITERM_LSB;

	return eta6963_update_bits(eta, ETA6963_REG_03, ETA6963_REG03_ITERM_MASK, 
				iterm << ETA6963_REG03_ITERM_SHIFT);
}

EXPORT_SYMBOL_GPL(eta6963_set_term_current);



int eta6963_set_prechg_current(struct eta6963 *eta, int curr)
{
	u8 iprechg;

	if (curr < ETA6963_REG03_IPRECHG_BASE)
		curr = ETA6963_REG03_IPRECHG_BASE;

	iprechg = (curr - ETA6963_REG03_IPRECHG_BASE) / ETA6963_REG03_IPRECHG_LSB;

	return eta6963_update_bits(eta, ETA6963_REG_03, ETA6963_REG03_IPRECHG_MASK, 
				iprechg << ETA6963_REG03_IPRECHG_SHIFT);
}
EXPORT_SYMBOL_GPL(eta6963_set_prechg_current);

int eta6963_set_chargevolt(struct eta6963 *eta, int volt)
{
	u8 val;
	
	if (volt < ETA6963_REG04_VREG_BASE)
		volt = ETA6963_REG04_VREG_BASE;

	val = (volt - ETA6963_REG04_VREG_BASE) / ETA6963_REG04_VREG_LSB;
	return eta6963_update_bits(eta, ETA6963_REG_04, ETA6963_REG04_VREG_MASK, 
				val << ETA6963_REG04_VREG_SHIFT);
}


int eta6963_set_input_volt_limit(struct eta6963 *eta, int volt)
{
	u8 val;

	if (volt < ETA6963_REG06_VINDPM_BASE)
		volt = ETA6963_REG06_VINDPM_BASE;

	val = (volt - ETA6963_REG06_VINDPM_BASE) / ETA6963_REG06_VINDPM_LSB;
	return eta6963_update_bits(eta, ETA6963_REG_06, ETA6963_REG06_VINDPM_MASK, 
				val << ETA6963_REG06_VINDPM_SHIFT);
}

int eta6963_set_input_current_limit(struct eta6963 *eta, int curr)
{
	u8 val;

	if (curr < ETA6963_REG00_IINLIM_BASE)
		curr = ETA6963_REG00_IINLIM_BASE;

	val = (curr - ETA6963_REG00_IINLIM_BASE) / ETA6963_REG00_IINLIM_LSB;
	return eta6963_update_bits(eta, ETA6963_REG_00, ETA6963_REG00_IINLIM_MASK, 
				val << ETA6963_REG00_IINLIM_SHIFT);
}


int eta6963_set_watchdog_timer(struct eta6963 *eta, u8 timeout)
{
	u8 temp;

	temp = (u8)(((timeout - ETA6963_REG05_WDT_BASE) / ETA6963_REG05_WDT_LSB) << ETA6963_REG05_WDT_SHIFT);

	return eta6963_update_bits(eta, ETA6963_REG_05, ETA6963_REG05_WDT_MASK, temp); 
}
EXPORT_SYMBOL_GPL(eta6963_set_watchdog_timer);

int eta6963_disable_watchdog_timer(struct eta6963 *eta)
{
	u8 val = ETA6963_REG05_WDT_DISABLE << ETA6963_REG05_WDT_SHIFT;

	return eta6963_update_bits(eta, ETA6963_REG_05, ETA6963_REG05_WDT_MASK, val);
}
EXPORT_SYMBOL_GPL(eta6963_disable_watchdog_timer);

int eta6963_reset_watchdog_timer(struct eta6963 *eta)
{
	u8 val = ETA6963_REG01_WDT_RESET << ETA6963_REG01_WDT_RESET_SHIFT;

	return eta6963_update_bits(eta, ETA6963_REG_01, ETA6963_REG01_WDT_RESET_MASK, val);
}
EXPORT_SYMBOL_GPL(eta6963_reset_watchdog_timer);

int eta6963_reset_chip(struct eta6963 *eta)
{
	int ret;
	u8 val = ETA6963_REG0B_RESET << ETA6963_REG0B_RESET_SHIFT;

	ret = eta6963_update_bits(eta, ETA6963_REG_0B, ETA6963_REG0B_RESET_MASK, val);
	return ret;
}
EXPORT_SYMBOL_GPL(eta6963_reset_chip);

int eta6963_enter_hiz_mode(struct eta6963 *eta)
{
	u8 val = ETA6963_REG00_HIZ_ENABLE << ETA6963_REG00_ENHIZ_SHIFT;

	return eta6963_update_bits(eta, ETA6963_REG_00, ETA6963_REG00_ENHIZ_MASK, val);

}
EXPORT_SYMBOL_GPL(eta6963_enter_hiz_mode);

int eta6963_exit_hiz_mode(struct eta6963 *eta)
{

	u8 val = ETA6963_REG00_HIZ_DISABLE << ETA6963_REG00_ENHIZ_SHIFT;

	return eta6963_update_bits(eta, ETA6963_REG_00, ETA6963_REG00_ENHIZ_MASK, val);

}
EXPORT_SYMBOL_GPL(eta6963_exit_hiz_mode);

int eta6963_get_hiz_mode(struct eta6963 *eta, u8 *state)
{
	u8 val;
	int ret;

	ret = eta6963_read_byte(eta, &val, ETA6963_REG_00);
	if (ret)
		return ret;
	*state = (val & ETA6963_REG00_ENHIZ_MASK) >> ETA6963_REG00_ENHIZ_SHIFT;

	return 0;
}
EXPORT_SYMBOL_GPL(eta6963_get_hiz_mode);


static int eta6963_enable_term(struct eta6963* eta, bool enable)
{
	u8 val;
	int ret;

	if (enable)
		val = ETA6963_REG05_TERM_ENABLE << ETA6963_REG05_EN_TERM_SHIFT;
	else
		val = ETA6963_REG05_TERM_DISABLE << ETA6963_REG05_EN_TERM_SHIFT;

	ret = eta6963_update_bits(eta, ETA6963_REG_05, ETA6963_REG05_EN_TERM_MASK, val);

	return ret;
}
EXPORT_SYMBOL_GPL(eta6963_enable_term);

int eta6963_set_boost_current(struct eta6963 *eta, int curr)
{
	u8 val;

	val = ETA6963_REG02_BOOST_LIM_1P2A;
	if (curr == BOOSTI_500)
		val = ETA6963_REG02_BOOST_LIM_0P5A;

	return eta6963_update_bits(eta, ETA6963_REG_02, ETA6963_REG02_BOOST_LIM_MASK, 
				val << ETA6963_REG02_BOOST_LIM_SHIFT);
}

int eta6963_set_boost_voltage(struct eta6963 *eta, int volt)
{
	u8 val;

  val = ETA6963_REG06_BOOSTV_5P15V;
	if (volt == BOOSTV_4850)
		val = ETA6963_REG06_BOOSTV_4P85V;
	else if (volt == BOOSTV_5150)
		val = ETA6963_REG06_BOOSTV_5P15V;
	else if (volt == BOOSTV_5300)
		val = ETA6963_REG06_BOOSTV_5P3V;
	else
		val = ETA6963_REG06_BOOSTV_5P15V;

	return eta6963_update_bits(eta, ETA6963_REG_06, ETA6963_REG06_BOOSTV_MASK, 
				val << ETA6963_REG06_BOOSTV_SHIFT);
}
EXPORT_SYMBOL_GPL(eta6963_set_boost_voltage);

static int eta6963_set_acovp_threshold(struct eta6963 *eta, int volt)
{
	u8 val;
  
  val = ETA6963_REG06_OVP_10P5V;
	if (volt == VAC_OVP_14300)
		val = ETA6963_REG06_OVP_14V;
	else if (volt == VAC_OVP_10500)
		val = ETA6963_REG06_OVP_10P5V;
	else if (volt == VAC_OVP_6500)
		val = ETA6963_REG06_OVP_6P5V;
	else
		val = ETA6963_REG06_OVP_10P5V;

	return eta6963_update_bits(eta, ETA6963_REG_06, ETA6963_REG06_OVP_MASK, 
				val << ETA6963_REG06_OVP_SHIFT);
}
EXPORT_SYMBOL_GPL(eta6963_set_acovp_threshold);

static int eta6963_set_stat_ctrl(struct eta6963 *eta, int ctrl)
{
	u8 val;

	val = ctrl;

	return eta6963_update_bits(eta, ETA6963_REG_00, ETA6963_REG00_STAT_CTRL_MASK, 
				val << ETA6963_REG00_STAT_CTRL_SHIFT);
}


static int eta6963_set_int_mask(struct eta6963 *eta, int mask)
{
	u8 val;

	val = mask;

	return eta6963_update_bits(eta, ETA6963_REG_0A, ETA6963_REG0A_INT_MASK_MASK,
				val << ETA6963_REG0A_INT_MASK_SHIFT);
}


static int eta6963_enable_batfet(struct eta6963 *eta)
{
	const u8 val = ETA6963_REG07_BATFET_ON << ETA6963_REG07_BATFET_DIS_SHIFT;

	return eta6963_update_bits(eta, ETA6963_REG_07, ETA6963_REG07_BATFET_DIS_MASK,
				val);
}
EXPORT_SYMBOL_GPL(eta6963_enable_batfet);


static int eta6963_disable_batfet(struct eta6963 *eta)
{
	const u8 val = ETA6963_REG07_BATFET_OFF << ETA6963_REG07_BATFET_DIS_SHIFT;

	return eta6963_update_bits(eta, ETA6963_REG_07, ETA6963_REG07_BATFET_DIS_MASK,
				val);
}
EXPORT_SYMBOL_GPL(eta6963_disable_batfet);

static int eta6963_set_batfet_delay(struct eta6963 *eta, uint8_t delay)
{
	u8 val;

	if (delay == 0)
		val = ETA6963_REG07_BATFET_DLY_0S;
	else
		val = ETA6963_REG07_BATFET_DLY_10S;
	
	val <<= ETA6963_REG07_BATFET_DLY_SHIFT;

	return eta6963_update_bits(eta, ETA6963_REG_07, ETA6963_REG07_BATFET_DLY_MASK,
								val);
}
EXPORT_SYMBOL_GPL(eta6963_set_batfet_delay);

static int eta6963_set_vdpm_bat_track(struct eta6963 *eta)
{
	const u8 val = ETA6963_REG07_VDPM_BAT_TRACK_200MV << ETA6963_REG07_VDPM_BAT_TRACK_SHIFT;

	return eta6963_update_bits(eta, ETA6963_REG_07, ETA6963_REG07_VDPM_BAT_TRACK_MASK,
				val);
}
EXPORT_SYMBOL_GPL(eta6963_set_vdpm_bat_track);

static int eta6963_enable_safety_timer(struct eta6963 *eta)
{
	const u8 val = ETA6963_REG05_CHG_TIMER_ENABLE << ETA6963_REG05_EN_TIMER_SHIFT;
	
	return eta6963_update_bits(eta, ETA6963_REG_05, ETA6963_REG05_EN_TIMER_MASK,
				val);
}
EXPORT_SYMBOL_GPL(eta6963_enable_safety_timer);


static int eta6963_disable_safety_timer(struct eta6963 *eta)
{
	const u8 val = ETA6963_REG05_CHG_TIMER_DISABLE << ETA6963_REG05_EN_TIMER_SHIFT;
	
	return eta6963_update_bits(eta, ETA6963_REG_05, ETA6963_REG05_EN_TIMER_MASK,
				val);
}
EXPORT_SYMBOL_GPL(eta6963_disable_safety_timer);

static struct eta6963_platform_data* eta6963_parse_dt(struct device *dev, 
							struct eta6963 * eta)
{
    int ret;
    struct device_node *np = dev->of_node;
	struct eta6963_platform_data* pdata;
	
	pdata = devm_kzalloc(dev, sizeof(struct eta6963_platform_data), 
						GFP_KERNEL);
	if (!pdata) {
		pr_err("Out of memory\n");
		return NULL;
	}
#if 0	
	ret = of_property_read_u32(np, "eta,eta6963,chip-enable-gpio", &eta->gpio_ce);
    if(ret) {
		pr_err("Failed to read node of eta,eta6963,chip-enable-gpio\n");
	}
#endif

    ret = of_property_read_u32(np,"eta,eta6963,usb-vlim",&pdata->usb.vlim);
    if(ret) {
		pr_err("Failed to read node of eta,eta6963,usb-vlim\n");
	}

    ret = of_property_read_u32(np,"eta,eta6963,usb-ilim",&pdata->usb.ilim);
    if(ret) {
		pr_err("Failed to read node of eta,eta6963,usb-ilim\n");
	}
	
    ret = of_property_read_u32(np,"eta,eta6963,usb-vreg",&pdata->usb.vreg);
    if(ret) {
		pr_err("Failed to read node of eta,eta6963,usb-vreg\n");
	}

    ret = of_property_read_u32(np,"eta,eta6963,usb-ichg",&pdata->usb.ichg);
    if(ret) {
		pr_err("Failed to read node of eta,eta6963,usb-ichg\n");
	}

    ret = of_property_read_u32(np,"eta,eta6963,stat-pin-ctrl",&pdata->statctrl);
    if(ret) {
		pr_err("Failed to read node of eta,eta6963,stat-pin-ctrl\n");
	}
	
    ret = of_property_read_u32(np,"eta,eta6963,precharge-current",&pdata->iprechg);
    if(ret) {
		pr_err("Failed to read node of eta,eta6963,precharge-current\n");
	}

    ret = of_property_read_u32(np,"eta,eta6963,termination-current",&pdata->iterm);
    if(ret) {
		pr_err("Failed to read node of eta,eta6963,termination-current\n");
	}
	
    ret = of_property_read_u32(np,"eta,eta6963,boost-voltage",&pdata->boostv);
    if(ret) {
		pr_err("Failed to read node of eta,eta6963,boost-voltage\n");
	}

    ret = of_property_read_u32(np,"eta,eta6963,boost-current",&pdata->boosti);
    if(ret) {
		pr_err("Failed to read node of eta,eta6963,boost-current\n");
	}

    ret = of_property_read_u32(np,"eta,eta6963,vac-ovp-threshold",&pdata->vac_ovp);
    if(ret) {
		pr_err("Failed to read node of eta,eta6963,vac-ovp-threshold\n");
	}

    return pdata;   
}

static int eta6963_init_device(struct eta6963 *eta)
{
	int ret;
	
	eta6963_disable_watchdog_timer(eta);

	eta6963_set_vdpm_bat_track(eta);

	eta6963_disable_safety_timer(eta);
	
	ret = eta6963_set_stat_ctrl(eta, eta->platform_data->statctrl);
	if (ret)
		pr_err("Failed to set stat pin control mode, ret = %d\n",ret);

	ret = eta6963_set_prechg_current(eta, eta->platform_data->iprechg);
	if (ret)
		pr_err("Failed to set prechg current, ret = %d\n",ret);
	/* pr_err(">>>>>>set termination current  %d\n",eta->platform_data->iterm);*/
	ret = eta6963_set_term_current(eta, eta->platform_data->iterm);
	if (ret)
		pr_err("Failed to set termination current, ret = %d\n",ret);
	
	ret = eta6963_set_boost_voltage(eta, eta->platform_data->boostv);
	if (ret)
		pr_err("Failed to set boost voltage, ret = %d\n",ret);
	
	ret = eta6963_set_boost_current(eta, eta->platform_data->boosti);
	if (ret)
		pr_err("Failed to set boost current, ret = %d\n",ret);
	
	ret = eta6963_set_acovp_threshold(eta, eta->platform_data->vac_ovp);
	if (ret)
		pr_err("Failed to set acovp threshold, ret = %d\n",ret);	

	ret = eta6963_set_int_mask(eta, ETA6963_REG0A_IINDPM_INT_MASK | ETA6963_REG0A_VINDPM_INT_MASK);
	if (ret)
		pr_err("Failed to set vindpm and iindpm int mask\n");

	return 0;
}


static int eta6963_detect_device(struct eta6963* eta)
{
    int ret;
    u8 data;
    pr_err("[%s]",__func__);
    ret = eta6963_read_byte(eta, &data, ETA6963_REG_0B);
    if(ret == 0){
        eta->part_no = (data & ETA6963_REG0B_PN_MASK) >> ETA6963_REG0B_PN_SHIFT;
        eta->revision = (data & ETA6963_REG0B_DEV_REV_MASK) >> ETA6963_REG0B_DEV_REV_SHIFT;
    }

    pr_err("[%s] part_no=%d revision=%d\n", __func__, eta->part_no, eta->revision);
    if(eta->part_no != ETA6963)
		return 1;
    return ret;
}
//begin gerrit:202736
static int eta6963_read_byte_m155(struct eta6963 *eta, u8 reg, u8 *data)
{
	return eta6963_read_byte(eta, data, reg);
}

static int eta6963_detect_device_m155(struct eta6963* eta)
{
    int ret;
    u8 data;
    pr_err("[%s]",__func__);

	u8 data_03,data_0B;
	u8 partno;

	pr_info("[chg_detect][0/5]:%s runing.\n", __func__);

	ret = eta6963_read_byte_m155(eta, 0x03, &data_03);
	pr_info("[chg_detect][1/5]:read 0x03 reg, ret [%d], data [0x%x].\n", ret, data_03);
	if (ret < 0)
		return ret;

	ret = eta6963_write_byte(eta, 0x03, (data_03 & 0xEF));
	pr_info("[chg_detect][2/5]:write 0x03 reg bit4 = 0, ret [%d], data [0x%x].\n", ret, (data_03 & 0xEF));
	if (ret < 0)
		return ret;
	
	ret = eta6963_read_byte_m155(eta, 0x0B, &data_0B);
	pr_info("[chg_detect][3/5]:read 0x0B reg, ret [%d], data [0x%x].\n", ret, data_0B);
	if (ret < 0)
		return ret;

	ret = eta6963_write_byte(eta, 0x03, data_03);
	pr_info("[chg_detect][4/5]:restore 0x03 reg, ret [%d], data [0x%x].\n", ret, data_03);
	if (ret < 0)
		return ret;

	partno = (data_0B & 0x78) >> 3;
	if (partno == 0x07)
	{
		pr_info("[chg_detect][5/5]:reg[0x0B].bit[6:3][%d] == 7, chg_ic is eta6963.\n", partno);
	}
	else
	{
		pr_info("[chg_detect][5/5]:reg[0x0B].bit[6:3][%d] != 7, chg_ic is sc89601a.\n", partno);
		return -1;
	}
	
    ret = eta6963_read_byte(eta, &data, ETA6963_REG_0B);
    if(ret == 0){
        eta->part_no = (data & ETA6963_REG0B_PN_MASK) >> ETA6963_REG0B_PN_SHIFT;
        eta->revision = (data & ETA6963_REG0B_DEV_REV_MASK) >> ETA6963_REG0B_DEV_REV_SHIFT;
    }

    pr_err("[%s] part_no=%d revision=%d\n", __func__, eta->part_no, eta->revision);

    return ret;
}
//end gerrit:202736

static void eta6963_dump_regs(struct eta6963 *eta)
{
	int addr;
	u8 val;
	int ret;

	for (addr = 0x0; addr <= 0x0B; addr++) {
		ret = eta6963_read_byte(eta, &val, addr);
		if (ret == 0)
			pr_err("Reg[%.2x] = 0x%.2x\n", addr, val);
	}


}

static ssize_t eta6963_show_registers(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct eta6963 *eta = dev_get_drvdata(dev);
	u8 addr;
	u8 val;
	u8 tmpbuf[200];
	int len;
	int idx = 0;
	int ret ;

	idx = snprintf(buf, PAGE_SIZE, "%s:\n", "eta6963 Reg");
	for (addr = 0x0; addr <= 0x0B; addr++) {
		ret = eta6963_read_byte(eta, &val, addr);
		if (ret == 0) {
			len = snprintf(tmpbuf, PAGE_SIZE - idx,"Reg[%.2x] = 0x%.2x\n", addr, val);
			memcpy(&buf[idx], tmpbuf, len);
			idx += len;
		}
	}

	return idx;
}

static ssize_t eta6963_store_registers(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct eta6963 *eta = dev_get_drvdata(dev);
	int ret;
	unsigned int reg;
	unsigned int val;

	ret = sscanf(buf, "%x %x", &reg, &val);
	if (ret == 2 && reg < 0x0B) {
		eta6963_write_byte(eta, (unsigned char)reg, (unsigned char)val);
	}

	return count;
}

static DEVICE_ATTR(registers, S_IRUGO | S_IWUSR, eta6963_show_registers, eta6963_store_registers);

static struct attribute *eta6963_attributes[] = {
	&dev_attr_registers.attr,
	NULL,
};

static const struct attribute_group eta6963_attr_group = {
	.attrs = eta6963_attributes,
};

static int eta6963_set_hizmode(struct charger_device *chg_dev, bool enable)
{

	struct eta6963 *eta = dev_get_drvdata(&chg_dev->dev);
	int ret = 0;
	u8 tmp;

	if (enable)
  	{
		ret = eta6963_enter_hiz_mode(eta);
  	}
	else
  	{
		ret = eta6963_exit_hiz_mode(eta);
   	}

	ret = eta6963_read_byte(eta, &tmp, ETA6963_REG_00);

	pr_err("set hiz mode i2c  read reg 0x%02X is 0x%02X  ret :%d\n",ETA6963_REG_00,tmp,ret);

	pr_err("%s set hizmode %s\n", enable ? "enable" : "disable",
				  !ret ? "successfully" : "failed");

	return ret;
}

static int eta6963_charging(struct charger_device *chg_dev, bool enable)
{

	struct eta6963 *eta = dev_get_drvdata(&chg_dev->dev);
	int ret = 0,rc = 0;
	u8 val;

	if (enable)
  	{
		ret = eta6963_enable_charger(eta);
  	}
	else
  	{
		ret = eta6963_disable_charger(eta);
   	}
	pr_err("%s charger %s\n", enable ? "enable" : "disable",
				  !ret ? "successfully" : "failed");

	ret = eta6963_read_byte(eta, &val, ETA6963_REG_01);
	if (!ret && !rc )
		eta->charge_enabled = !!(val & ETA6963_REG01_CHG_CONFIG_MASK);

	return ret;
}

static int eta6963_plug_in(struct charger_device *chg_dev)
{

	int ret;
	
	pr_info("[%s] enter!", __func__);
	ret = eta6963_charging(chg_dev, true);

	if (ret)
		pr_err("Failed to enable charging:%d\n", ret);
	
	return ret;
}

static int eta6963_plug_out(struct charger_device *chg_dev)
{
	int ret;

	pr_info("[%s] enter!", __func__);
	ret = eta6963_charging(chg_dev, false);

	if (!ret)
		pr_err("Failed to disable charging:%d\n", ret);
	
	return ret;
}

static int eta6963_dump_register(struct charger_device *chg_dev)
{
	struct eta6963 *eta = dev_get_drvdata(&chg_dev->dev);

	eta6963_dump_regs(eta);

	return 0;
}

static int eta6963_is_charging_enable(struct charger_device *chg_dev, bool *en)
{
	struct eta6963 *eta = dev_get_drvdata(&chg_dev->dev);
	
	*en = eta->charge_enabled;
	
	return 0;
}

static int eta6963_is_charging_done(struct charger_device *chg_dev, bool *done)
{
	struct eta6963 *eta = dev_get_drvdata(&chg_dev->dev);
	int ret;
	u8 val;
	
	ret = eta6963_read_byte(eta, &val, ETA6963_REG_08);
	if (!ret) {
		val = val & ETA6963_REG08_CHRG_STAT_MASK;
		val = val >> ETA6963_REG08_CHRG_STAT_SHIFT;
		*done = (val == ETA6963_REG08_CHRG_STAT_CHGDONE);	
	}
	
	return ret;
}

static int eta6963_set_ichg(struct charger_device *chg_dev, u32 curr)
{
	struct eta6963 *eta = dev_get_drvdata(&chg_dev->dev);
	u8 reg_val;
	int ichg_bef;
	int ret;

	ret = eta6963_read_byte(eta, &reg_val, ETA6963_REG_02);
	if (!ret) {
		ichg_bef = (reg_val & ETA6963_REG02_ICHG_MASK) >> ETA6963_REG02_ICHG_SHIFT;
		ichg_bef = ichg_bef * ETA6963_REG02_ICHG_LSB + ETA6963_REG02_ICHG_BASE;
		if ((ichg_bef <= curr / 1000) &&
			(ichg_bef + ETA6963_REG02_ICHG_LSB > curr / 1000)) {
			pr_info("[%s] current has set! [%d]\n", __func__, curr);
			return ret;
		}
	}

	pr_info("[%s] curr=%d, ichg_bef = %d\n", __func__, curr, ichg_bef);

	return eta6963_set_chargecurrent(eta, curr/1000);
}

static int eta6963_get_ichg(struct charger_device *chg_dev, u32 *curr)
{
	struct eta6963 *eta = dev_get_drvdata(&chg_dev->dev);
	u8 reg_val;
	int ichg;
	int ret;

	ret = eta6963_read_byte(eta, &reg_val, ETA6963_REG_02);
	if (!ret) {
		ichg = (reg_val & ETA6963_REG02_ICHG_MASK) >> ETA6963_REG02_ICHG_SHIFT;
		ichg = ichg * ETA6963_REG02_ICHG_LSB + ETA6963_REG02_ICHG_BASE;
		*curr = ichg * 1000;
	}
	pr_info("[%s] curr=%d\n", __func__, *curr);

	return ret;
}

static int eta6963_get_min_ichg(struct charger_device *chg_dev, u32 *curr)
{

	*curr = 60 * 1000;

	return 0;
}

static int eta6963_set_vchg(struct charger_device *chg_dev, u32 volt)
{
	
	struct eta6963 *eta = dev_get_drvdata(&chg_dev->dev);

	return eta6963_set_chargevolt(eta, volt/1000);	
}

static int eta6963_get_vchg(struct charger_device *chg_dev, u32 *volt)
{
	struct eta6963 *eta = dev_get_drvdata(&chg_dev->dev);
	u8 reg_val;
	int vchg;
	int ret;

	ret = eta6963_read_byte(eta, &reg_val, ETA6963_REG_04);
	if (!ret) {
		vchg = (reg_val & ETA6963_REG04_VREG_MASK) >> ETA6963_REG04_VREG_SHIFT;
		vchg = vchg * ETA6963_REG04_VREG_LSB + ETA6963_REG04_VREG_BASE;
		*volt = vchg * 1000;
	}

	return ret;
}

static int eta6963_set_ivl(struct charger_device *chg_dev, u32 volt)
{

	struct eta6963 *eta = dev_get_drvdata(&chg_dev->dev);

	return eta6963_set_input_volt_limit(eta, volt/1000);

}

static int eta6963_set_icl(struct charger_device *chg_dev, u32 curr)
{

	struct eta6963 *eta = dev_get_drvdata(&chg_dev->dev);
	return eta6963_set_input_current_limit(eta, curr/1000);
}

static int eta6963_get_icl(struct charger_device *chg_dev, u32 *curr)
{
	struct eta6963 *eta = dev_get_drvdata(&chg_dev->dev);
	u8 reg_val;
	int icl;
	int ret;

	ret = eta6963_read_byte(eta, &reg_val, ETA6963_REG_00);
	if (!ret) {
		icl = (reg_val & ETA6963_REG00_IINLIM_MASK) >> ETA6963_REG00_IINLIM_SHIFT;
		icl = icl * ETA6963_REG00_IINLIM_LSB + ETA6963_REG00_IINLIM_BASE;
		*curr = icl * 1000;
	}

	return ret;

}

static int eta6963_kick_wdt(struct charger_device *chg_dev)
{
	struct eta6963 *eta = dev_get_drvdata(&chg_dev->dev);

	return eta6963_reset_watchdog_timer(eta);
}

static int eta6963_set_otg(struct charger_device *chg_dev, bool en)
{
	int ret;
	struct eta6963 *eta = dev_get_drvdata(&chg_dev->dev);
	
	if (en)
  	{ 
		eta6963_otg_flag = 1;
          	/*add gerrit 201119 begin*/
		ret =  eta6963_disable_charger(eta);
          	/*end*/
		ret = eta6963_enable_otg(eta);
   	}
	else
  	{ 
		eta6963_otg_flag = 0;
		ret = eta6963_disable_otg(eta);
         	/*add gerrit 201119 begin*/
		ret = eta6963_enable_charger(eta);
          	/*end*/
   	}
	return ret;
}

static int eta6963_set_safety_timer(struct charger_device *chg_dev, bool en)
{
	struct eta6963 *eta = dev_get_drvdata(&chg_dev->dev);
	int ret;
		
	if (en)
		ret = eta6963_enable_safety_timer(eta);
	else
		ret = eta6963_disable_safety_timer(eta);
		
	return ret;
}

static int eta6963_is_safety_timer_enabled(struct charger_device *chg_dev, bool *en)
{
	struct eta6963 *eta = dev_get_drvdata(&chg_dev->dev);
	int ret;
	u8 reg_val;

	ret = eta6963_read_byte(eta, &reg_val, ETA6963_REG_05);

	if (!ret) 
		*en = !!(reg_val & ETA6963_REG05_EN_TIMER_MASK);
	
	return ret;
}


static int eta6963_set_boost_ilmt(struct charger_device *chg_dev, u32 curr)
{
	struct eta6963 *eta = dev_get_drvdata(&chg_dev->dev);
	int ret;

	ret = eta6963_set_boost_current(eta, curr/1000);

	return ret;
}

static int eta6963_do_event(struct charger_device *chg_dev, u32 event,
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


static struct charger_ops eta6963_chg_ops = {
	/* Normal charging */
  .hiz_mode = eta6963_set_hizmode,
	.plug_in = eta6963_plug_in,
	.plug_out = eta6963_plug_out,
	.dump_registers = eta6963_dump_register,
	.enable = eta6963_charging,
	.is_enabled = eta6963_is_charging_enable,
	.get_charging_current = eta6963_get_ichg,
	.set_charging_current = eta6963_set_ichg,
	.get_input_current = eta6963_get_icl,
	.set_input_current = eta6963_set_icl,
	.get_constant_voltage = eta6963_get_vchg,
	.set_constant_voltage = eta6963_set_vchg,
	.kick_wdt = eta6963_kick_wdt,
	.set_mivr = eta6963_set_ivl,
	.is_charging_done = eta6963_is_charging_done,
	.get_min_charging_current = eta6963_get_min_ichg,

	/* Safety timer */
	.enable_safety_timer = eta6963_set_safety_timer,
	.is_safety_timer_enabled = eta6963_is_safety_timer_enabled,

	/* Power path */
	.enable_powerpath = NULL,
	.is_powerpath_enabled = NULL,

	/* OTG */
	.enable_otg = eta6963_set_otg,
	.set_boost_current_limit = eta6963_set_boost_ilmt,
	.enable_discharge = NULL,
	.event = eta6963_do_event,

	/* PE+/PE+20 */
	.send_ta_current_pattern = NULL,
	.set_pe20_efficiency_table = NULL,
	.send_ta20_current_pattern = NULL,
//	.set_ta20_reset = NULL,
	.enable_cable_drop_comp = NULL,

	/* ADC */
	.get_tchg_adc = NULL,
};



static int eta6963_charger_probe(struct i2c_client *client, 
					const struct i2c_device_id *id)
{
	struct eta6963 *eta;

	int ret;
	

	eta = devm_kzalloc(&client->dev, sizeof(struct eta6963), GFP_KERNEL);
	if (!eta) {
		pr_err("Out of memory\n");
		return -ENOMEM;
	}

	eta->dev = &client->dev;
	eta->client = client;

	i2c_set_clientdata(client, eta);
	
	mutex_init(&eta->i2c_rw_lock);

	/* 2022.5.18 longcheer zhangfeng5 edit. kenrel driver porting: remove eta6963 charger */
	ret = eta6963_detect_device(eta);//modify by gerrit:202736
	if(ret) {
		pr_err("No eta6963 device found!\n");
		return -ENODEV;
	}
	
	if (client->dev.of_node)
		eta->platform_data = eta6963_parse_dt(&client->dev, eta);
	else
		eta->platform_data = client->dev.platform_data;
	
	if (!eta->platform_data) {
		pr_err("No platform data provided.\n");
		return -EINVAL;
	}

	eta->chg_dev = charger_device_register("primary_chg",
			&client->dev, eta, 
			&eta6963_chg_ops,
			&eta6963_chg_props);
	if (IS_ERR_OR_NULL(eta->chg_dev)) {
		ret = PTR_ERR(eta->chg_dev);
		goto err_0;
	}

	ret = eta6963_init_device(eta);
	if (ret) {
		pr_err("Failed to init device\n");
		return ret;
	}

	
	ret = sysfs_create_group(&eta->dev->kobj, &eta6963_attr_group);
	if (ret) {
		dev_err(eta->dev, "failed to register sysfs. err: %d\n", ret);
	}


	pr_err("eta6963 probe successfully, Part Num:%d, Revision:%d\n!", 
				eta->part_no, eta->revision);
	
	return 0;
	
err_0:
	
	return ret;
}

static int eta6963_charger_remove(struct i2c_client *client)
{
	struct eta6963 *eta = i2c_get_clientdata(client);


	mutex_destroy(&eta->i2c_rw_lock);

	sysfs_remove_group(&eta->dev->kobj, &eta6963_attr_group);


	return 0;
}


static void eta6963_charger_shutdown(struct i2c_client *client)
{
}

static struct of_device_id eta6963_charger_match_table[] = {
	{.compatible = "eta,eta6963_charger",},
	{},
};
MODULE_DEVICE_TABLE(of,eta6963_charger_match_table);

static const struct i2c_device_id eta6963_charger_id[] = {
	{ "eta6963-charger", ETA6963 },
	{},
};
MODULE_DEVICE_TABLE(i2c, eta6963_charger_id);

static struct i2c_driver eta6963_charger_driver = {
	.driver 	= {
		.name 	= "eta6963-charger",
		.owner 	= THIS_MODULE,
		.of_match_table = eta6963_charger_match_table,
	},
	.id_table	= eta6963_charger_id,
	
	.probe		= eta6963_charger_probe,
	.remove		= eta6963_charger_remove,
	.shutdown	= eta6963_charger_shutdown,
	
};

module_i2c_driver(eta6963_charger_driver);

MODULE_DESCRIPTION("ETA ETA6963 Charger Driver");
MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("ETA COMP.");
