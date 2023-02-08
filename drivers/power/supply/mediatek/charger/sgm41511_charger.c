/*
 * SGM41511 battery charging driver
 *
 * Copyright (C) 2021 SGM
 *
 * This package is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 * THIS PACKAGE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

#define pr_fmt(fmt)	"[sgm41511]:%s: " fmt, __func__

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
#include "mtk_charger_intf.h"
/* C3T code for HQHW-2797 by tongjiacheng at 2022/09/13 start*/
#include <mt-plat/upmu_common.h>
/* C3T code for HQHW-2797 by tongjiacheng at 2022/09/13 end*/

#include "sgm41511_reg.h"


struct sgm41511_charge_param {
	int vlim;
	int ilim;
	int ichg;
	int vreg;
};

enum stat_ctrl {
	STAT_CTRL_STAT,
	STAT_CTRL_ICHG,
	STAT_CTRL_INDPM,
	STAT_CTRL_DISABLE,
};

enum vboost {
	BOOSTV_4850 = 4850,
	BOOSTV_5000 = 5000,
	BOOSTV_5150 = 5150,
	BOOSTV_5300 = 5300,
};

enum iboost {
	BOOSTI_500 = 500,
	BOOSTI_1200 = 1200,
};

enum vac_ovp {
	VAC_OVP_5500 = 5500,
	VAC_OVP_6500 = 6500,
	VAC_OVP_10500 = 10500,
	VAC_OVP_14000 = 14000,
};


struct sgm41511_platform_data {
	struct sgm41511_charge_param usb;
	int iprechg;
	int iterm;

	enum stat_ctrl statctrl;
	enum vboost boostv;	// options are 4850,
	enum iboost boosti; // options are 500mA, 1200mA
	enum vac_ovp vac_ovp;

};



enum {
	PN_SGM41511,
};

enum sgm41511_part_no {
	SGM41511 = 0x02,
};

/* add to distinguish sgm or bq */
enum extra_part_no {
	EXTRA_BQ25601 = 0x01,
	EXTRA_SGM41511 = 0x00,
};

/* add to distinguish eta or sgm */
enum reg0c_no {
	REG0C_ETA6953 = 0x00,
};

static int pn_data[] = {
	[PN_SGM41511] = 0x02,
};

static char *pn_str[] = {
	[PN_SGM41511] = "sgm41511",
};

struct sgm41511 {
	struct device *dev;
	struct i2c_client *client;

	enum sgm41511_part_no part_no;
	enum extra_part_no e_part_no; /* add to distinguish sgm or bq */
	enum reg0c_no reg0c_no; /* add to distinguish eta or sgm */
	int revision;

	const char *chg_dev_name;
	const char *eint_name;

	bool chg_det_enable;

	// enum charger_type chg_type;
	struct power_supply_desc psy_desc;
	int psy_usb_type;

	int status;
	int irq;

	struct mutex i2c_rw_lock;

	bool charge_enabled;	/* Register bit status */
	bool power_good;
	/* C3T code for HQHW-2797 by tongjiacheng at 2022/09/13 start*/
	bool vbus_good;
	/* C3T code for HQHW-2797 by tongjiacheng at 2022/09/13 end*/

	struct sgm41511_platform_data *platform_data;
	struct charger_device *chg_dev;

	struct power_supply *psy;
	struct power_supply *psy1;

/*C3T code for HQ-223303 by gengyifei at 2022/7/28 start*/
	bool hiz_mode;
/*C3T code for HQ-223303 by gengyifei at 2022/7/28 end*/
/*C3T code for HQ-228593 by tongjiacheng at 2022/08/04 start*/
	struct delayed_work psy_dwork;
/*C3T code for HQ-228593 by tongjiacheng at 2022/08/04 end*/
/* C3T code for HQHW-2797 by tongjiacheng at 2022/09/13 start*/
	struct delayed_work force_detect_dwork;
	u8 force_detect_count;
/* C3T code for HQHW-2797 by tongjiacheng at 2022/09/13 end*/
	u8 sdp_count;
};

static const struct charger_properties sgm41511_chg_props = {
	.alias_name = "sgm41511",
};

/* C3T code for HQHW-3513 by tongjiacheng at 2022/10/18 start*/
extern struct musb *mtk_musb;
extern void hq_musb_pullup(struct musb *musb, int is_on);
/* C3T code for HQHW-3513 by tongjiacheng at 2022/10/18 end*/

static int __sgm41511_read_reg(struct sgm41511 *sgm, u8 reg, u8 *data)
{
	s32 ret;

	ret = i2c_smbus_read_byte_data(sgm->client, reg);
	if (ret < 0) {
		pr_err("i2c read fail: can't read from reg 0x%02X\n", reg);
		return ret;
	}

	*data = (u8) ret;

	return 0;
}

static int __sgm41511_write_reg(struct sgm41511 *sgm, int reg, u8 val)
{
	s32 ret;

	ret = i2c_smbus_write_byte_data(sgm->client, reg, val);
	if (ret < 0) {
		pr_err("i2c write fail: can't write 0x%02X to reg 0x%02X: %d\n",
				val, reg, ret);
		return ret;
	}
	return 0;
}

static int sgm41511_read_byte(struct sgm41511 *sgm, u8 reg, u8 *data)
{
	int ret;

	mutex_lock(&sgm->i2c_rw_lock);
	ret = __sgm41511_read_reg(sgm, reg, data);
	mutex_unlock(&sgm->i2c_rw_lock);

	return ret;
}

static int sgm41511_write_byte(struct sgm41511 *sgm, u8 reg, u8 data)
{
	int ret;

	mutex_lock(&sgm->i2c_rw_lock);
	ret = __sgm41511_write_reg(sgm, reg, data);
	mutex_unlock(&sgm->i2c_rw_lock);

	if (ret)
		pr_err("Failed: reg=%02X, ret=%d\n", reg, ret);

	return ret;
}

static int sgm41511_update_bits(struct sgm41511 *sgm, u8 reg, u8 mask, u8 data)
{
	int ret;
	u8 tmp;

	mutex_lock(&sgm->i2c_rw_lock);
	ret = __sgm41511_read_reg(sgm, reg, &tmp);
	if (ret) {
		pr_err("Failed: reg=%02X, ret=%d\n", reg, ret);
		goto out;
	}

	tmp &= ~mask;
	tmp |= data & mask;

	ret = __sgm41511_write_reg(sgm, reg, tmp);
	if (ret)
		pr_err("Failed: reg=%02X, ret=%d\n", reg, ret);

out:
	mutex_unlock(&sgm->i2c_rw_lock);
	return ret;
}

static int sgm41511_enable_otg(struct sgm41511 *sgm)
{
	u8 val = REG01_OTG_ENABLE << REG01_OTG_CONFIG_SHIFT;

	return sgm41511_update_bits(sgm, SGM41511_REG_01, REG01_OTG_CONFIG_MASK,
					val);

}

static int sgm41511_disable_otg(struct sgm41511 *sgm)
{
	u8 val = REG01_OTG_DISABLE << REG01_OTG_CONFIG_SHIFT;

	return sgm41511_update_bits(sgm, SGM41511_REG_01, REG01_OTG_CONFIG_MASK,
					val);

}

static int sgm41511_enable_charger(struct sgm41511 *sgm)
{
	int ret;
	u8 val = REG01_CHG_ENABLE << REG01_CHG_CONFIG_SHIFT;

	ret =
		sgm41511_update_bits(sgm, SGM41511_REG_01, REG01_CHG_CONFIG_MASK, val);

	return ret;
}

static int sgm41511_disable_charger(struct sgm41511 *sgm)
{
	int ret;
	u8 val = REG01_CHG_DISABLE << REG01_CHG_CONFIG_SHIFT;

	ret =
		sgm41511_update_bits(sgm, SGM41511_REG_01, REG01_CHG_CONFIG_MASK, val);
	return ret;
}

int sgm41511_set_chargecurrent(struct sgm41511 *sgm, int curr)
{
	u8 ichg;

/*C3T code for HQ-223293 by  tongjiacheng at 2022/7/28 start*/
	if (curr <= 40) 
		ichg = curr / 5;
	else if (curr <= 110)
		ichg = 0x08 + (curr - 40) / 10;
	else if (curr <= 270)
		ichg = 0x0f + (curr - 110) / 20;
	else if (curr <= 540)
		ichg = 0x17 + (curr - 270) / 30;
	else if (curr <= 1500)
		ichg = 0x20 + (curr - 540) / 60;
	else if (curr <= 2940)
		ichg = 0x30 + (curr - 1500) / 120;
/*C3T code for HQ-223293 by  tongjiacheng at 2022/7/28 end*/

	return sgm41511_update_bits(sgm, SGM41511_REG_02, REG02_ICHG_MASK,
					ichg << REG02_ICHG_SHIFT);

}

int sgm41511_set_term_current(struct sgm41511 *sgm, int curr)
{
	u8 iterm;

/*C3T code for HQ-223293 by  tongjiacheng at 2022/7/28 start*/
	if (curr <= 20)
		iterm = curr / 5;
	else if (curr <= 60)
		iterm = 0x03 + (curr - 20) / 10;
	else if (curr <= 200)
		iterm = 0x07 + (curr - 60) / 20;
	else if (curr <= 240)
		iterm = 0x0e + (curr - 200) / 40;
/*C3T code for HQ-223293 by  tongjiacheng at 2022/7/28 end*/

	return sgm41511_update_bits(sgm, SGM41511_REG_03, REG03_ITERM_MASK,
					iterm << REG03_ITERM_SHIFT);
}
EXPORT_SYMBOL_GPL(sgm41511_set_term_current);

int sgm41511_set_prechg_current(struct sgm41511 *sgm, int curr)
{
	u8 iprechg;

/*C3T code for HQ-223293 by  tongjiacheng at 2022/7/28 start*/
	if (curr <= 20)
		iprechg = curr / 5;
	else if (curr <= 60)
		iprechg = 0x03 + (curr - 20) / 10;
	else if (curr <= 200)
		iprechg = 0x07 + (curr - 60) / 20;
	else if (curr <= 240)
		iprechg = 0x0e + (curr - 200) / 40;
/*C3T code for HQ-223293 by  tongjiacheng at 2022/7/28 end*/

	return sgm41511_update_bits(sgm, SGM41511_REG_03, REG03_IPRECHG_MASK,
					iprechg << REG03_IPRECHG_SHIFT);
}
EXPORT_SYMBOL_GPL(sgm41511_set_prechg_current);

/* C3T code for HQ-223445 by tongjiacheng at 2022/08/30 start */
int sgm41511_set_chargevolt(struct sgm41511 *sgm, int volt)
{
	u8 val;
	u8 vreg_ft;
	int ret;

	if (volt < REG04_VREG_BASE)
		volt = REG04_VREG_BASE;

	if (((volt + 8 - REG04_VREG_BASE) % REG04_VREG_LSB) == 0) {
		volt += 8;
		vreg_ft = REG0F_VREG_FT_DEC8MV;
	}
	else if (((volt + 16 - REG04_VREG_BASE) % REG04_VREG_LSB) == 0) {
		volt += 16;
		vreg_ft = REG0F_VREG_FT_DEC16MV;
	}
	else if (((volt - 8 - REG04_VREG_BASE) % REG04_VREG_LSB) == 0) {
		volt -= 8;
		vreg_ft = REG0F_VREG_FT_INC8MV;
	}
	else
		vreg_ft = REG0F_VREG_FT_DEFAULT;

	val = (volt - REG04_VREG_BASE) / REG04_VREG_LSB;
	ret = sgm41511_update_bits(sgm, SGM41511_REG_04, REG04_VREG_MASK,
					val << REG04_VREG_SHIFT);
	if (ret) {
		dev_err(sgm->dev, "%s: failed to set charger volt\n", __func__);
		return ret;
	}

	ret = sgm41511_update_bits(sgm, SGM41511_REG_0F, REG0F_VREG_FT_MASK,
					vreg_ft << REG0F_VREG_FT_SHIFT);
	if (ret) {
		dev_err(sgm->dev, "%s: failed to set charger volt ft\n", __func__);
		return ret;
	}

	return 0;
}
/* C3T code for HQ-223445 by tongjiacheng at 2022/08/30 end*/

int sgm41511_set_input_volt_limit(struct sgm41511 *sgm, int volt)
{
	u8 val;

	if (volt < REG06_VINDPM_BASE)
		volt = REG06_VINDPM_BASE;

	val = (volt - REG06_VINDPM_BASE) / REG06_VINDPM_LSB;
	return sgm41511_update_bits(sgm, SGM41511_REG_06, REG06_VINDPM_MASK,
					val << REG06_VINDPM_SHIFT);
}

int sgm41511_set_input_current_limit(struct sgm41511 *sgm, int curr)
{
	u8 val;

	if (curr < REG00_IINLIM_BASE)
		curr = REG00_IINLIM_BASE;

	val = (curr - REG00_IINLIM_BASE) / REG00_IINLIM_LSB;
	return sgm41511_update_bits(sgm, SGM41511_REG_00, REG00_IINLIM_MASK,
					val << REG00_IINLIM_SHIFT);
}

int sgm41511_set_watchdog_timer(struct sgm41511 *sgm, u8 timeout)
{
	u8 temp;

	temp = (u8) (((timeout -
				REG05_WDT_BASE) / REG05_WDT_LSB) << REG05_WDT_SHIFT);

	return sgm41511_update_bits(sgm, SGM41511_REG_05, REG05_WDT_MASK, temp);
}
EXPORT_SYMBOL_GPL(sgm41511_set_watchdog_timer);

int sgm41511_disable_watchdog_timer(struct sgm41511 *sgm)
{
	u8 val = REG05_WDT_DISABLE << REG05_WDT_SHIFT;

	return sgm41511_update_bits(sgm, SGM41511_REG_05, REG05_WDT_MASK, val);
}
EXPORT_SYMBOL_GPL(sgm41511_disable_watchdog_timer);

int sgm41511_reset_watchdog_timer(struct sgm41511 *sgm)
{
	u8 val = REG01_WDT_RESET << REG01_WDT_RESET_SHIFT;

	return sgm41511_update_bits(sgm, SGM41511_REG_01, REG01_WDT_RESET_MASK,
					val);
}
EXPORT_SYMBOL_GPL(sgm41511_reset_watchdog_timer);

int sgm41511_reset_chip(struct sgm41511 *sgm)
{
	int ret;
	u8 val = REG0B_REG_RESET << REG0B_REG_RESET_SHIFT;

	ret =
		sgm41511_update_bits(sgm, SGM41511_REG_0B, REG0B_REG_RESET_MASK, val);
	return ret;
}
EXPORT_SYMBOL_GPL(sgm41511_reset_chip);

int sgm41511_enter_hiz_mode(struct sgm41511 *sgm)
{
	u8 val = REG00_HIZ_ENABLE << REG00_ENHIZ_SHIFT;

	return sgm41511_update_bits(sgm, SGM41511_REG_00, REG00_ENHIZ_MASK, val);

}
EXPORT_SYMBOL_GPL(sgm41511_enter_hiz_mode);

int sgm41511_exit_hiz_mode(struct sgm41511 *sgm)
{

	u8 val = REG00_HIZ_DISABLE << REG00_ENHIZ_SHIFT;

	return sgm41511_update_bits(sgm, SGM41511_REG_00, REG00_ENHIZ_MASK, val);

}
EXPORT_SYMBOL_GPL(sgm41511_exit_hiz_mode);

/*HS03s added for DEVAL5626-463 by wangzikang at 20210729 start */
// int sgm41511_get_hiz_mode(struct sgm41511 *sgm, u8 *state)
// {
// 	u8 val;
// 	int ret;

// 	ret = sgm41511_read_byte(sgm, SGM41511_REG_00, &val);
// 	if (ret)
// 		return ret;
// 	*state = (val & REG00_ENHIZ_MASK) >> REG00_ENHIZ_SHIFT;

// 	return 0;
// }
// EXPORT_SYMBOL_GPL(sgm41511_get_hiz_mode);
/*HS03s added for DEVAL5626-463 by wangzikang at 20210729 end */

static int sgm41511_enable_term(struct sgm41511 *sgm, bool enable)
{
	u8 val;
	int ret;

	if (enable)
		val = REG05_TERM_ENABLE << REG05_EN_TERM_SHIFT;
	else
		val = REG05_TERM_DISABLE << REG05_EN_TERM_SHIFT;

	ret = sgm41511_update_bits(sgm, SGM41511_REG_05, REG05_EN_TERM_MASK, val);

	return ret;
}
EXPORT_SYMBOL_GPL(sgm41511_enable_term);

int sgm41511_set_boost_current(struct sgm41511 *sgm, int curr)
{
	u8 val;

	val = REG02_BOOST_LIM_0P5A;
	if (curr == BOOSTI_1200)
		val = REG02_BOOST_LIM_1P2A;

	return sgm41511_update_bits(sgm, SGM41511_REG_02, REG02_BOOST_LIM_MASK,
					val << REG02_BOOST_LIM_SHIFT);
}

int sgm41511_set_boost_voltage(struct sgm41511 *sgm, int volt)
{
	u8 val;

	if (volt == BOOSTV_4850)
		val = REG06_BOOSTV_4P85V;
	else if (volt == BOOSTV_5150)
		val = REG06_BOOSTV_5P15V;
	else if (volt == BOOSTV_5300)
		val = REG06_BOOSTV_5P3V;
	else
		val = REG06_BOOSTV_5V;

	return sgm41511_update_bits(sgm, SGM41511_REG_06, REG06_BOOSTV_MASK,
					val << REG06_BOOSTV_SHIFT);
}
EXPORT_SYMBOL_GPL(sgm41511_set_boost_voltage);

static int sgm41511_set_acovp_threshold(struct sgm41511 *sgm, int volt)
{
	u8 val;

	if (volt == VAC_OVP_14000)
		val = REG06_OVP_14P0V;
	else if (volt == VAC_OVP_10500)
		val = REG06_OVP_10P5V;
	else if (volt == VAC_OVP_6500)
		val = REG06_OVP_6P5V;
	else
		val = REG06_OVP_5P5V;

	return sgm41511_update_bits(sgm, SGM41511_REG_06, REG06_OVP_MASK,
					val << REG06_OVP_SHIFT);
}
EXPORT_SYMBOL_GPL(sgm41511_set_acovp_threshold);

static int sgm41511_set_stat_ctrl(struct sgm41511 *sgm, int ctrl)
{
	u8 val;

	val = ctrl;

	return sgm41511_update_bits(sgm, SGM41511_REG_00, REG00_STAT_CTRL_MASK,
					val << REG00_STAT_CTRL_SHIFT);
}

static int sgm41511_set_int_mask(struct sgm41511 *sgm, int mask)
{
	u8 val;

	val = mask;

	return sgm41511_update_bits(sgm, SGM41511_REG_0A, REG0A_INT_MASK_MASK,
					val << REG0A_INT_MASK_SHIFT);
}

static int sgm41511_enable_batfet(struct sgm41511 *sgm)
{
	const u8 val = REG07_BATFET_ON << REG07_BATFET_DIS_SHIFT;

	return sgm41511_update_bits(sgm, SGM41511_REG_07, REG07_BATFET_DIS_MASK,
					val);
}
EXPORT_SYMBOL_GPL(sgm41511_enable_batfet);

static int sgm41511_disable_batfet(struct sgm41511 *sgm)
{
	const u8 val = REG07_BATFET_OFF << REG07_BATFET_DIS_SHIFT;

	return sgm41511_update_bits(sgm, SGM41511_REG_07, REG07_BATFET_DIS_MASK,
					val);
}
EXPORT_SYMBOL_GPL(sgm41511_disable_batfet);

static int sgm41511_set_batfet_delay(struct sgm41511 *sgm, uint8_t delay)
{
	u8 val;

	if (delay == 0)
		val = REG07_BATFET_DLY_0S;
	else
		val = REG07_BATFET_DLY_10S;

	val <<= REG07_BATFET_DLY_SHIFT;

	return sgm41511_update_bits(sgm, SGM41511_REG_07, REG07_BATFET_DLY_MASK,
					val);
}
EXPORT_SYMBOL_GPL(sgm41511_set_batfet_delay);

static int sgm41511_enable_safety_timer(struct sgm41511 *sgm)
{
	const u8 val = REG05_CHG_TIMER_ENABLE << REG05_EN_TIMER_SHIFT;

	return sgm41511_update_bits(sgm, SGM41511_REG_05, REG05_EN_TIMER_MASK,
					val);
}
EXPORT_SYMBOL_GPL(sgm41511_enable_safety_timer);

static int sgm41511_disable_safety_timer(struct sgm41511 *sgm)
{
	const u8 val = REG05_CHG_TIMER_DISABLE << REG05_EN_TIMER_SHIFT;

	return sgm41511_update_bits(sgm, SGM41511_REG_05, REG05_EN_TIMER_MASK,
					val);
}
EXPORT_SYMBOL_GPL(sgm41511_disable_safety_timer);

static struct sgm41511_platform_data *sgm41511_parse_dt(struct device_node *np,
								struct sgm41511 *sgm)
{
	int ret;
	struct sgm41511_platform_data *pdata;

	pdata = devm_kzalloc(sgm->dev, sizeof(struct sgm41511_platform_data),
				 GFP_KERNEL);
	if (!pdata)
		return NULL;

	if (of_property_read_string(np, "charger_name", &sgm->chg_dev_name) < 0) {
		sgm->chg_dev_name = "primary_chg";
		pr_warn("no charger name\n");
	}

	if (of_property_read_string(np, "eint_name", &sgm->eint_name) < 0) {
		sgm->eint_name = "chr_stat";
		pr_warn("no eint name\n");
	}

	sgm->chg_det_enable =
		of_property_read_bool(np, "sgm41511,charge-detect-enable");

	ret = of_property_read_u32(np, "sgm41511,usb-vlim", &pdata->usb.vlim);
	if (ret) {
		pdata->usb.vlim = 4500;
		pr_err("Failed to read node of sgm41511,usb-vlim\n");
	}

	ret = of_property_read_u32(np, "sgm41511,usb-ilim", &pdata->usb.ilim);
	if (ret) {
		pdata->usb.ilim = 2000;
		pr_err("Failed to read node of sgm41511,usb-ilim\n");
	}

	ret = of_property_read_u32(np, "sgm41511,usb-vreg", &pdata->usb.vreg);
	if (ret) {
		pdata->usb.vreg = 4200;
		pr_err("Failed to read node of sgm41511,usb-vreg\n");
	}

	ret = of_property_read_u32(np, "sgm41511,usb-ichg", &pdata->usb.ichg);
	if (ret) {
		pdata->usb.ichg = 2000;
		pr_err("Failed to read node of sgm41511,usb-ichg\n");
	}

	ret = of_property_read_u32(np, "sgm41511,stat-pin-ctrl",
					&pdata->statctrl);
	if (ret) {
		pdata->statctrl = 0;
		pr_err("Failed to read node of sgm41511,stat-pin-ctrl\n");
	}

	ret = of_property_read_u32(np, "sgm41511,precharge-current",
					&pdata->iprechg);
	if (ret) {
		pdata->iprechg = 180;
		pr_err("Failed to read node of sgm41511,precharge-current\n");
	}

	ret = of_property_read_u32(np, "sgm41511,termination-current",
					&pdata->iterm);
	if (ret) {
		pdata->iterm = 180;
		pr_err
			("Failed to read node of sgm41511,termination-current\n");
	}

	ret =
		of_property_read_u32(np, "sgm41511,boost-voltage",
				 &pdata->boostv);
	if (ret) {
		pdata->boostv = 5000;
		pr_err("Failed to read node of sgm41511,boost-voltage\n");
	}

	ret =
		of_property_read_u32(np, "sgm41511,boost-current",
				 &pdata->boosti);
	if (ret) {
		pdata->boosti = 1200;
		pr_err("Failed to read node of sgm41511,boost-current\n");
	}

	ret = of_property_read_u32(np, "sgm41511,vac-ovp-threshold",
					&pdata->vac_ovp);
	if (ret) {
		pdata->vac_ovp = 6500;
		pr_err("Failed to read node of sgm41511,vac-ovp-threshold\n");
	}

	return pdata;
}

/* C3T code for HQHW-2797 by tongjiacheng at 2022/09/13 start*/
static int sgm41511_force_dpdm(struct sgm41511 *sgm)
{
	int ret;
	u8 data;

/*C3T code for HQ-218878 by gengyifei at 2022/9/20 start*/
	ret = sgm41511_update_bits(sgm, SGM41511_REG_0D, REG0D_DPDM_MASK,
									REG0D_DPDM_0V << REG0D_DPDM_SHIFT);
	if (ret) {
		dev_err(sgm->dev, "%s: failed to force detection\n", __func__);
		return ret;
	}
/*C3T code for HQ-218878 by gengyifei at 2022/9/20 end*/
	ret = sgm41511_update_bits(sgm, SGM41511_REG_07, REG07_FORCE_DPDM_MASK,
									REG07_FORCE_DPDM << REG07_FORCE_DPDM_SHIFT);
	if (ret) {
		dev_err(sgm->dev, "%s: failed to force detection\n", __func__);
		return ret;
	}

	sgm41511_read_byte(sgm, SGM41511_REG_0E, &data);
	data &= 0x80;
	data = data >> 7;

	if (data)
		sgm41511_update_bits(sgm, SGM41511_REG_0D, REG0D_DPDM_MASK,
										REG0D_DPDM_0V << REG0D_DPDM_SHIFT);

	return 0;
}

static irqreturn_t sgm41511_irq_handler(int irq, void *data);
static void sgm41511_force_detection_dwork_handler(struct work_struct *work)
{
	int ret;
	struct sgm41511 *sgm = container_of(work, struct sgm41511, force_detect_dwork.work);

	Charger_Detect_Init();
/* C3T code for HQHW-3513 by tongjiacheng at 2022/10/18 start*/
	hq_musb_pullup(mtk_musb, 0);
/* C3T code for HQHW-3513 by tongjiacheng at 2022/10/18 end*/

	ret = sgm41511_force_dpdm(sgm);
	if (ret) {
		dev_err(sgm->dev, "%s: failed to force detection\n", __func__);
		return;
	}

	msleep(500);
	ret = sgm41511_force_dpdm(sgm);
	if (ret) {
		dev_err(sgm->dev, "%s: failed to force detection\n", __func__);
		return;
	}

	sgm->force_detect_count++;

	sgm->power_good = false;
	sgm41511_irq_handler(sgm->client->irq, sgm);
}
/* C3T code for HQHW-2797 by tongjiacheng at 2022/09/13 end*/

static int sgm41511_get_charger_type(struct sgm41511 *sgm, int *type)
{
	int ret;

	u8 reg_val = 0;
	int vbus_stat = 0;
	int chg_type = CHARGER_UNKNOWN;

	ret = sgm41511_read_byte(sgm, SGM41511_REG_08, &reg_val);

	if (ret)
		return ret;

	vbus_stat = (reg_val & REG08_VBUS_STAT_MASK);
	vbus_stat >>= REG08_VBUS_STAT_SHIFT;

/* C3T code for HQHW-3513 by tongjiacheng at 2022/10/18 start*/
	if (sgm->sdp_count < 2 &&  vbus_stat == REG08_VBUS_TYPE_SDP) {
		schedule_delayed_work(&sgm->force_detect_dwork, 0);
		sgm->sdp_count++;
		return 0;
	}
/* C3T code for HQHW-3513 by tongjiacheng at 2022/10/18 end*/

	switch (vbus_stat) {

	case REG08_VBUS_TYPE_NONE:
		chg_type = CHARGER_UNKNOWN;
		break;
	case REG08_VBUS_TYPE_SDP:
		chg_type = STANDARD_HOST;
		break;
	case REG08_VBUS_TYPE_CDP:
		chg_type = CHARGING_HOST;
	/* C3T code for HQ-257872by tongjiacheng at 2022/11/03 start*/
		hq_musb_pullup(mtk_musb, 1);
	/* C3T code for HQ-257872by tongjiacheng at 2022/11/03 end*/
		break;
	case REG08_VBUS_TYPE_DCP:
		chg_type = STANDARD_CHARGER;
		break;
	case REG08_VBUS_TYPE_UNKNOWN:
	/* C3T code for HQHW-2797 by tongjiacheng at 2022/09/13 start*/
		chg_type = NONSTANDARD_CHARGER;
	/* C3T code for HQHW-3513 by tongjiacheng at 2022/10/18 start*/
		sgm->sdp_count = 0;
	/* C3T code for HQHW-3513 by tongjiacheng at 2022/10/18 end*/
		if (sgm->force_detect_count < 10)
			schedule_delayed_work(&sgm->force_detect_dwork, msecs_to_jiffies(2000));
	/* C3T code for HQHW-2797 by tongjiacheng at 2022/09/13 end*/
		break;
	case REG08_VBUS_TYPE_NON_STD:
		chg_type = NONSTANDARD_CHARGER;
		break;
	default:
		chg_type = NONSTANDARD_CHARGER;
		break;
	}

	*type = chg_type;
	/* C3T code for HQHW-2797 by tongjiacheng at 2022/09/13 start*/
	pr_err("%s: charger_type = %d\n", __func__, chg_type);

	if (chg_type != STANDARD_CHARGER)
		Charger_Detect_Release();
	/* C3T code for HQHW-2797 by tongjiacheng at 2022/09/13 end*/

	return 0;
}

static int sgm41511_get_chrg_stat(struct sgm41511 *sgm,
	int *chg_stat)
{
	int ret;
	u8 val;

	ret = sgm41511_read_byte(sgm, SGM41511_REG_08, &val);
	if (!ret) {
		val = val & REG08_CHRG_STAT_MASK;
		val = val >> REG08_CHRG_STAT_SHIFT;
		*chg_stat = val;
	}

	return ret;
}

/*C3T code for HQ-228593 by tongjiacheng at 2022/08/04 start*/
static void sgm41511_inform_psy_dwork_handler(struct work_struct *work)
{
	int ret = 0;
	union power_supply_propval propval;

	struct sgm41511 *sgm = container_of(work, struct sgm41511,
								psy_dwork.work);

	if (!sgm->psy) {
		sgm->psy = power_supply_get_by_name("charger");
		if (!sgm->psy) {
			pr_err("%s get power supply fail\n", __func__);
			mod_delayed_work(system_wq, &sgm->psy_dwork, 
					msecs_to_jiffies(2000));
			return ;
/*C3T code for HQ-228593 by tongjiacheng at 2022/08/04 end*/
		}
	}

/*C3T code for HQHW-2906 by gengyifei at 2022/10/08 start*/
	if (!sgm->power_good)
		sgm->psy_usb_type = CHARGER_UNKNOWN;
/*C3T code for HQHW-2906 by gengyifei at 2022/10/08 end*/

	if (sgm->psy_usb_type != POWER_SUPPLY_TYPE_UNKNOWN)
		propval.intval = 1;
	else
		propval.intval = 0;

	ret = power_supply_set_property(sgm->psy, POWER_SUPPLY_PROP_ONLINE,
					&propval);

	if (ret < 0)
		pr_notice("inform power supply online failed:%d\n", ret);

	propval.intval = sgm->psy_usb_type;

	ret = power_supply_set_property(sgm->psy,
					POWER_SUPPLY_PROP_CHARGE_TYPE,
					&propval);

	if (ret < 0)
		pr_notice("inform power supply charge type failed:%d\n", ret);
}

/* C3T code for HQHW-2797 by tongjiacheng at 2022/09/13 start*/
static irqreturn_t sgm41511_irq_handler(int irq, void *data)
{
	int ret;
	u8 reg_val;
	bool prev_pg;
	bool prev_vbus_gd;
	struct sgm41511 *sgm = (struct sgm41511 *)data;

	ret = sgm41511_read_byte(sgm, SGM41511_REG_08, &reg_val);
	if (ret)
		return IRQ_HANDLED;

	prev_pg = sgm->power_good;
	sgm->power_good = !!(reg_val & REG08_PG_STAT_MASK);

	ret = sgm41511_read_byte(sgm, SGM41511_REG_0A, &reg_val);
	if (ret)
		return IRQ_HANDLED;

	prev_vbus_gd = sgm->vbus_good;
	sgm->vbus_good = !!(reg_val & REG0A_VBUS_GD_MASK);

	if (!prev_vbus_gd && sgm->vbus_good) {
		pr_err("adapter/usb inserted\n");
		sgm->force_detect_count = 0;
	/* C3T code for HQHW-3513 by tongjiacheng at 2022/10/18 start*/
		sgm->sdp_count = 0;
	/* C3T code for HQHW-3513 by tongjiacheng at 2022/10/18 end*/
	}
	else if (prev_vbus_gd && !sgm->vbus_good) {
		pr_err("adapter/usb removed\n");
		sgm41511_get_charger_type(sgm, &sgm->psy_usb_type);
		schedule_delayed_work(&sgm->psy_dwork, 0);
		Charger_Detect_Init();
/* C3T code for HQHW-3513 by tongjiacheng at 2022/10/18 start*/
		hq_musb_pullup(mtk_musb, 0);
/* C3T code for HQHW-3513 by tongjiacheng at 2022/10/18 end*/
		return IRQ_HANDLED;
	}

	if (!prev_pg && sgm->power_good) {
		sgm41511_get_charger_type(sgm, &sgm->psy_usb_type);
		schedule_delayed_work(&sgm->psy_dwork, 0);
	}

	return IRQ_HANDLED;
}
/* C3T code for HQHW-2797 by tongjiacheng at 2022/09/13 end*/

static int sgm41511_register_interrupt(struct sgm41511 *sgm)
{
	int ret = 0;
	/*struct device_node *np;

	np = of_find_node_by_name(NULL, sgm->eint_name);
	if (np) {
		sgm->irq = irq_of_parse_and_map(np, 0);
	} else {
		pr_err("couldn't get irq node\n");
		return -ENODEV;
	}

	pr_info("irq = %d\n", sgm->irq);*/

	if (! sgm->client->irq) {
		pr_info("sgm->client->irq is NULL\n");//remember to config dws
		return -ENODEV;
	}

	ret = devm_request_threaded_irq(sgm->dev, sgm->client->irq, NULL,
					sgm41511_irq_handler,
					IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
					"ti_irq", sgm);
	if (ret < 0) {
		pr_err("request thread irq failed:%d\n", ret);
		return ret;
	}

/*C3T code for HQ-226181 by tongjiacheng at 2022/07/28 start*/
	device_init_wakeup(sgm->dev, true);
/*C3T code for HQ-226181 by tongjiacheng at 2022/07/28 end*/

	return 0;
}

static int sgm41511_init_device(struct sgm41511 *sgm)
{
	int ret;

	sgm41511_disable_watchdog_timer(sgm);

	ret = sgm41511_set_stat_ctrl(sgm, sgm->platform_data->statctrl);
	if (ret)
		pr_err("Failed to set stat pin control mode, ret = %d\n", ret);

	ret = sgm41511_set_prechg_current(sgm, sgm->platform_data->iprechg);
	if (ret)
		pr_err("Failed to set prechg current, ret = %d\n", ret);

	ret = sgm41511_set_term_current(sgm, sgm->platform_data->iterm);
	if (ret)
		pr_err("Failed to set termination current, ret = %d\n", ret);

	ret = sgm41511_set_boost_voltage(sgm, sgm->platform_data->boostv);
	if (ret)
		pr_err("Failed to set boost voltage, ret = %d\n", ret);

	ret = sgm41511_set_boost_current(sgm, sgm->platform_data->boosti);
	if (ret)
		pr_err("Failed to set boost current, ret = %d\n", ret);

	ret = sgm41511_set_acovp_threshold(sgm, sgm->platform_data->vac_ovp);
	if (ret)
		pr_err("Failed to set acovp threshold, ret = %d\n", ret);

	ret= sgm41511_disable_safety_timer(sgm);
	if (ret)
		pr_err("Failed to set safety_timer stop, ret = %d\n", ret);

	ret = sgm41511_set_int_mask(sgm,
					REG0A_IINDPM_INT_MASK |
					REG0A_VINDPM_INT_MASK);
	if (ret)
		pr_err("Failed to set vindpm and iindpm int mask\n");

	return 0;
}

static void determine_initial_status(struct sgm41511 *sgm)
{
	sgm41511_irq_handler(sgm->irq, (void *) sgm);
}

static int sgm41511_detect_device(struct sgm41511 *sgm)
{
	int ret;
	u8 data;

	ret = sgm41511_read_byte(sgm, SGM41511_REG_0B, &data);
	if (!ret) {
		sgm->part_no = (data & REG0B_PN_MASK) >> REG0B_PN_SHIFT;
		sgm->e_part_no = (data & REG0B_SGMPART_MASK) >> REG0B_SGMPART_SHIFT;
		sgm->revision =
			(data & REG0B_DEV_REV_MASK) >> REG0B_DEV_REV_SHIFT;
	}
	ret = sgm41511_read_byte(sgm, SGM41511_REG_0C, &data);
	if (!ret) {
		sgm->reg0c_no = (data & REG0C_RESERVED_MASK) >> REG0C_RESERVED_SHIFT;
	}

	return ret;
}

static void sgm41511_dump_regs(struct sgm41511 *sgm)
{
	int addr;
	u8 val;
	int ret;
/* C3T code for HQ-223445 by tongjiacheng at 2022/08/30 start */
	for (addr = 0x0; addr <= 0x0F; addr++) {
/* C3T code for HQ-223445 by tongjiacheng at 2022/08/30 end */
		ret = sgm41511_read_byte(sgm, addr, &val);
		if (ret == 0)
			pr_err("Reg[%.2x] = 0x%.2x\n", addr, val);
	}
}

static ssize_t
sgm41511_show_registers(struct device *dev, struct device_attribute *attr,
				char *buf)
{
	struct sgm41511 *sgm = dev_get_drvdata(dev);
	u8 addr;
	u8 val;
	u8 tmpbuf[200];
	int len;
	int idx = 0;
	int ret;

	idx = snprintf(buf, PAGE_SIZE, "%s:\n", "sgm41511 Reg");
	for (addr = 0x0; addr <= 0x0B; addr++) {
		ret = sgm41511_read_byte(sgm, addr, &val);
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
sgm41511_store_registers(struct device *dev,
			struct device_attribute *attr, const char *buf,
			size_t count)
{
	struct sgm41511 *sgm = dev_get_drvdata(dev);
	int ret;
	unsigned int reg;
	unsigned int val;

	ret = sscanf(buf, "%x %x", &reg, &val);
	if (ret == 2 && reg < 0x0B) {
		sgm41511_write_byte(sgm, (unsigned char) reg,
					(unsigned char) val);
	}

	return count;
}

static DEVICE_ATTR(registers, S_IRUGO | S_IWUSR, sgm41511_show_registers,
			sgm41511_store_registers);

static struct attribute *sgm41511_attributes[] = {
	&dev_attr_registers.attr,
	NULL,
};

static const struct attribute_group sgm41511_attr_group = {
	.attrs = sgm41511_attributes,
};

static int sgm41511_charging(struct charger_device *chg_dev, bool enable)
{

	struct sgm41511 *sgm = dev_get_drvdata(&chg_dev->dev);
	int ret = 0;
	u8 val;

	if (enable)
		ret = sgm41511_enable_charger(sgm);
	else
		ret = sgm41511_disable_charger(sgm);

	pr_err("%s charger %s\n", enable ? "enable" : "disable",
			!ret ? "successfully" : "failed");

	ret = sgm41511_read_byte(sgm, SGM41511_REG_01, &val);

	if (!ret)
		sgm->charge_enabled = !!(val & REG01_CHG_CONFIG_MASK);

	return ret;
}

static int sgm41511_plug_in(struct charger_device *chg_dev)
{

	int ret;

	ret = sgm41511_charging(chg_dev, true);

	if (ret)
		pr_err("Failed to enable charging:%d\n", ret);

	return ret;
}

static int sgm41511_plug_out(struct charger_device *chg_dev)
{
	int ret;

	ret = sgm41511_charging(chg_dev, false);

	if (ret)
		pr_err("Failed to disable charging:%d\n", ret);

	return ret;
}

static int sgm41511_dump_register(struct charger_device *chg_dev)
{
	struct sgm41511 *sgm = dev_get_drvdata(&chg_dev->dev);

	sgm41511_dump_regs(sgm);

	return 0;
}

static int sgm41511_is_charging_enable(struct charger_device *chg_dev, bool *en)
{
	struct sgm41511 *sgm = dev_get_drvdata(&chg_dev->dev);

	*en = sgm->charge_enabled;

	return 0;
}

#if 0
static int sgm41511_get_charging_status(struct charger_device *chg_dev,
	int *chg_stat)
{
	struct sgm41511 *sgm = dev_get_drvdata(&chg_dev->dev);
	int ret;
	u8 val;

	ret = sgm41511_read_byte(sgm, SGM41511_REG_08, &val);
	if (!ret) {
		val = val & REG08_CHRG_STAT_MASK;
		val = val >> REG08_CHRG_STAT_SHIFT;
		*chg_stat = val;
	}

	return ret;
}
#endif

static int sgm41511_is_charging_done(struct charger_device *chg_dev, bool *done)
{
	struct sgm41511 *sgm = dev_get_drvdata(&chg_dev->dev);
	int ret;
	u8 val;

	ret = sgm41511_read_byte(sgm, SGM41511_REG_08, &val);
	if (!ret) {
		val = val & REG08_CHRG_STAT_MASK;
		val = val >> REG08_CHRG_STAT_SHIFT;
		*done = (val == REG08_CHRG_STAT_CHGDONE);
	}

	return ret;
}

static int sgm41511_set_ichg(struct charger_device *chg_dev, u32 curr)
{
	struct sgm41511 *sgm = dev_get_drvdata(&chg_dev->dev);

	pr_err("charge curr = %d\n", curr);

	return sgm41511_set_chargecurrent(sgm, curr / 1000);
}

static int sgm41511_get_ichg(struct charger_device *chg_dev, u32 *curr)
{
	struct sgm41511 *sgm = dev_get_drvdata(&chg_dev->dev);
	u8 reg_val;
	int ichg;
	int ret;

	ret = sgm41511_read_byte(sgm, SGM41511_REG_02, &reg_val);
	if (!ret) {
		ichg = (reg_val & REG02_ICHG_MASK) >> REG02_ICHG_SHIFT;
		ichg = ichg * REG02_ICHG_LSB + REG02_ICHG_BASE;
		*curr = ichg * 1000;
	}

	return ret;
}

static int sgm41511_set_iterm(struct charger_device *chg_dev, u32 uA)
{
	struct sgm41511 *sgm = dev_get_drvdata(&chg_dev->dev);

	pr_err("termination curr = %d\n", uA);

	return sgm41511_set_term_current(sgm, uA / 1000);
}

static int sgm41511_get_min_ichg(struct charger_device *chg_dev, u32 *curr)
{
	*curr = 60 * 1000;

	return 0;
}

static int sgm41511_get_min_aicr(struct charger_device *chg_dev, u32 *uA)
{
	*uA = 100 * 1000;
	return 0;
}

static int sgm41511_set_vchg(struct charger_device *chg_dev, u32 volt)
{
	struct sgm41511 *sgm = dev_get_drvdata(&chg_dev->dev);

	pr_err("charge volt = %d\n", volt);

	return sgm41511_set_chargevolt(sgm, volt / 1000);
}

static int sgm41511_get_vchg(struct charger_device *chg_dev, u32 *volt)
{
	struct sgm41511 *sgm = dev_get_drvdata(&chg_dev->dev);
	u8 reg_val;
	int vchg;
	int ret;

	ret = sgm41511_read_byte(sgm, SGM41511_REG_04, &reg_val);
	if (!ret) {
		vchg = (reg_val & REG04_VREG_MASK) >> REG04_VREG_SHIFT;
		vchg = vchg * REG04_VREG_LSB + REG04_VREG_BASE;
		*volt = vchg * 1000;
	}

	return ret;
}

static int sgm41511_get_ivl_state(struct charger_device *chg_dev, bool *in_loop)
{
	int ret = 0;
	struct sgm41511 *sgm = dev_get_drvdata(&chg_dev->dev);
	u8 reg_val;

	ret = sgm41511_read_byte(sgm, SGM41511_REG_0A, &reg_val);
	if (!ret)
		*in_loop = (ret & REG0A_VINDPM_STAT_MASK) >> REG0A_VINDPM_STAT_SHIFT;

	return ret;
}

static int sgm41511_get_ivl(struct charger_device *chg_dev, u32 *volt)
{
	struct sgm41511 *sgm = dev_get_drvdata(&chg_dev->dev);
	u8 reg_val;
	int ivl;
	int ret;

	ret = sgm41511_read_byte(sgm, SGM41511_REG_06, &reg_val);
	if (!ret) {
		ivl = (reg_val & REG06_VINDPM_MASK) >> REG06_VINDPM_SHIFT;
		ivl = ivl * REG06_VINDPM_LSB + REG06_VINDPM_BASE;
		*volt = ivl * 1000;
	}

	return ret;
}

static int sgm41511_set_ivl(struct charger_device *chg_dev, u32 volt)
{
	struct sgm41511 *sgm = dev_get_drvdata(&chg_dev->dev);

	pr_err("vindpm volt = %d\n", volt);

	return sgm41511_set_input_volt_limit(sgm, volt / 1000);

}

static int sgm41511_set_icl(struct charger_device *chg_dev, u32 curr)
{
	struct sgm41511 *sgm = dev_get_drvdata(&chg_dev->dev);

	pr_err("indpm curr = %d\n", curr);

	return sgm41511_set_input_current_limit(sgm, curr / 1000);
}

static int sgm41511_get_icl(struct charger_device *chg_dev, u32 *curr)
{
	struct sgm41511 *sgm = dev_get_drvdata(&chg_dev->dev);
	u8 reg_val;
	int icl;
	int ret;

	ret = sgm41511_read_byte(sgm, SGM41511_REG_00, &reg_val);
	if (!ret) {
		icl = (reg_val & REG00_IINLIM_MASK) >> REG00_IINLIM_SHIFT;
		icl = icl * REG00_IINLIM_LSB + REG00_IINLIM_BASE;
		*curr = icl * 1000;
	}

	return ret;

}

static int sgm41511_enable_te(struct charger_device *chg_dev, bool en)
{
	struct sgm41511 *sgm = dev_get_drvdata(&chg_dev->dev);

	pr_err("enable_term = %d\n", en);

	return sgm41511_enable_term(sgm, en);
}

static int sgm41511_kick_wdt(struct charger_device *chg_dev)
{
	struct sgm41511 *sgm = dev_get_drvdata(&chg_dev->dev);

	return sgm41511_reset_watchdog_timer(sgm);
}

static int sgm41511_set_otg(struct charger_device *chg_dev, bool en)
{
	int ret;
	struct sgm41511 *sgm = dev_get_drvdata(&chg_dev->dev);

	if (en) {
		ret = sgm41511_enable_otg(sgm);
	/* C3T code for HQHW-2797 by tongjiacheng at 2022/09/13 start*/
		Charger_Detect_Release();
	/* C3T code for HQHW-2797 by tongjiacheng at 2022/09/13 end */
	}
	else
		ret = sgm41511_disable_otg(sgm);

	pr_err("%s OTG %s\n", en ? "enable" : "disable",
			!ret ? "successfully" : "failed");

	return ret;
}

static int sgm41511_set_safety_timer(struct charger_device *chg_dev, bool en)
{
	struct sgm41511 *sgm = dev_get_drvdata(&chg_dev->dev);
	int ret;

	if (en)
		ret = sgm41511_enable_safety_timer(sgm);
	else
		ret = sgm41511_disable_safety_timer(sgm);

	return ret;
}

static int sgm41511_is_safety_timer_enabled(struct charger_device *chg_dev,
						bool *en)
{
	struct sgm41511 *sgm = dev_get_drvdata(&chg_dev->dev);
	int ret;
	u8 reg_val;

	ret = sgm41511_read_byte(sgm, SGM41511_REG_05, &reg_val);

	if (!ret)
		*en = !!(reg_val & REG05_EN_TIMER_MASK);

	return ret;
}

static int sgm41511_do_event(struct charger_device *chg_dev, u32 event, u32 args)
{
	struct sgm41511 *sgm = dev_get_drvdata(&chg_dev->dev);

	if (!sgm->psy) {
		dev_notice(sgm->dev, "%s: cannot get psy\n", __func__);
		return -ENODEV;
	}
/* C3T code for HQ-HQ-218837 by tongjiacheng at 2022/08/26 start */
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
/* C3T code for HQ-HQ-218837 by tongjiacheng at 2022/08/26 end*/

	return 0;
}

static int sgm41511_set_boost_ilmt(struct charger_device *chg_dev, u32 curr)
{
	struct sgm41511 *sgm = dev_get_drvdata(&chg_dev->dev);
	int ret;

	pr_err("otg curr = %d\n", curr);

	ret = sgm41511_set_boost_current(sgm, curr / 1000);

	return ret;
}

/* C3T code for HQHW-2797 by tongjiacheng at 2022/09/13 start*/
extern struct charger_manager *_pinfo;
static int sgm41511_set_hiz_mode(struct charger_device *chg_dev, bool en)
{
	int ret;
	struct sgm41511 *sgm = dev_get_drvdata(&chg_dev->dev);

/*C3T code for HQ-223303 by gengyifei at 2022/7/28 start*/
	if (en) {
		sgm41511_set_icl(sgm->chg_dev, 0);
		_pinfo->disable_charger = true;
		charger_manager_notifier(_pinfo, CHARGER_NOTIFY_STOP_CHARGING);
		sgm->hiz_mode = true;
	}
	else {
		_pinfo->disable_charger = false;
		_pinfo->is_suspend = false;
		sgm41511_set_icl(sgm->chg_dev, -1);
		charger_manager_notifier(_pinfo, CHARGER_NOTIFY_START_CHARGING);
		sgm->hiz_mode = false;
		schedule_delayed_work(&sgm->psy_dwork, 0);
	}
/*C3T code for HQ-223303 by gengyifei at 2022/7/28 end*/
/* C3T code for HQHW-2797 by tongjiacheng at 2022/09/13 end*/
	pr_err("%s hiz mode %s\n", en ? "enable" : "disable",
			!ret ? "successfully" : "failed");

	return ret;
}

/* C3T code for HQHW-2797 by tongjiacheng at 2022/09/13 start*/
static int sgm41511_get_hiz_mode(struct charger_device *chg_dev)
{
/*C3T code for HQ-223303 by gengyifei at 2022/7/28 start*/
	struct sgm41511 *sgm = dev_get_drvdata(&chg_dev->dev);

	pr_err("%s:hiz mode %s\n",__func__, sgm->hiz_mode ? "enabled" : "disabled");

	return sgm->hiz_mode;
}
/* C3T code for HQHW-2797 by tongjiacheng at 2022/09/13 end*/


static struct charger_ops sgm41511_chg_ops = {
	/* Normal charging */
	.plug_in = sgm41511_plug_in,
	.plug_out = sgm41511_plug_out,
	.dump_registers = sgm41511_dump_register,
	.enable = sgm41511_charging,
	.is_enabled = sgm41511_is_charging_enable,
	.get_charging_current = sgm41511_get_ichg,
	.set_charging_current = sgm41511_set_ichg,
	.get_input_current = sgm41511_get_icl,
	.set_input_current = sgm41511_set_icl,
	.get_constant_voltage = sgm41511_get_vchg,
	.set_constant_voltage = sgm41511_set_vchg,
	.kick_wdt = sgm41511_kick_wdt,
	.set_mivr = sgm41511_set_ivl,
	.get_mivr = sgm41511_get_ivl,
	.get_mivr_state = sgm41511_get_ivl_state,
	.is_charging_done = sgm41511_is_charging_done,
	.set_eoc_current = sgm41511_set_iterm,
	.enable_termination = sgm41511_enable_te,
	.reset_eoc_state = NULL,
	.get_min_charging_current = sgm41511_get_min_ichg,
	.get_min_input_current = sgm41511_get_min_aicr,

	/* Safety timer */
	.enable_safety_timer = sgm41511_set_safety_timer,
	.is_safety_timer_enabled = sgm41511_is_safety_timer_enabled,

	/* Power path */
	.enable_powerpath = NULL,
	.is_powerpath_enabled = NULL,

	/* OTG */
	.enable_otg = sgm41511_set_otg,
	.set_boost_current_limit = sgm41511_set_boost_ilmt,
	.enable_discharge = NULL,

	/* PE+/PE+20 */
	.send_ta_current_pattern = NULL,
	.set_pe20_efficiency_table = NULL,
	.send_ta20_current_pattern = NULL,
	.enable_cable_drop_comp = NULL,

	/* ADC */
	.get_tchg_adc = NULL,
	.get_ibus_adc = NULL,

	/* Event */
	.event = sgm41511_do_event,

	/* hiz mode */
/*C3T code for HQ-223303 by gengyifei at 2022/7/28 start*/
	.set_hiz_mode = sgm41511_set_hiz_mode,
	.get_hiz_mode = sgm41511_get_hiz_mode,
/*C3T code for HQ-223303 by gengyifei at 2022/7/28 end*/
};

static struct of_device_id sgm41511_charger_match_table[] = {
	{
	 .compatible = "sgm,sgm41511",
	 .data = &pn_data[PN_SGM41511],
	 },
	{},
};
MODULE_DEVICE_TABLE(of, sgm41511_charger_match_table);

/* ======================= */
/* charger ic Power Supply Ops */
/* ======================= */

enum CHG_STATUS {
	CHG_STATUS_NOT_CHARGING = 0,
	CHG_STATUS_PRE_CHARGE,
	CHG_STATUS_FAST_CHARGING,
	CHG_STATUS_DONE,
};

static int charger_ic_get_online(struct sgm41511 *sgm,
				     bool *val)
{
	bool pwr_rdy = false;

	sgm41511_get_charger_type(sgm, &sgm->psy_usb_type);
	if(sgm->psy_usb_type != CHARGER_UNKNOWN)
		pwr_rdy = true;
	else
		pwr_rdy = false;

	dev_info(sgm->dev, "%s: online = %d\n", __func__, pwr_rdy);
	*val = pwr_rdy;
	return 0;
}

static int charger_ic_get_property(struct power_supply *psy,
				       enum power_supply_property psp,
				       union power_supply_propval *val)
{
	struct sgm41511 *sgm = power_supply_get_drvdata(psy);
	bool pwr_rdy = false;
	int ret = 0;
	int chr_status = 0;

	dev_dbg(sgm->dev, "%s: prop = %d\n", __func__, psp);
	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		ret = charger_ic_get_online(sgm, &pwr_rdy);
		val->intval = pwr_rdy;
		break;
	case POWER_SUPPLY_PROP_STATUS:
		if (sgm->psy_usb_type == POWER_SUPPLY_USB_TYPE_UNKNOWN)
		{
			val->intval = POWER_SUPPLY_STATUS_DISCHARGING;
			break;
		}
		ret = sgm41511_get_chrg_stat(sgm, &chr_status);
		if (ret < 0) {
			val->intval = POWER_SUPPLY_STATUS_DISCHARGING;
			break;
		}
		switch(chr_status) {
		case CHG_STATUS_NOT_CHARGING:
			val->intval = POWER_SUPPLY_STATUS_DISCHARGING;
			break;
		case CHG_STATUS_PRE_CHARGE:
		case CHG_STATUS_FAST_CHARGING:
			if(sgm->charge_enabled)
				val->intval = POWER_SUPPLY_STATUS_CHARGING;
			else
				val->intval = POWER_SUPPLY_STATUS_NOT_CHARGING;
			break;
		case CHG_STATUS_DONE:
			val->intval = POWER_SUPPLY_STATUS_FULL;
			break;
		default:
			ret = -ENODATA;
			break;
		}
		break;
	default:
		ret = -ENODATA;
	}
	return ret;
}

static enum power_supply_property charger_ic_properties[] = {
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_STATUS,
};

static const struct power_supply_desc charger_ic_desc = {
	.properties		= charger_ic_properties,
	.num_properties		= ARRAY_SIZE(charger_ic_properties),
	.get_property		= charger_ic_get_property,
};

static char *charger_ic_supplied_to[] = {
	"battery",
	"mtk-master-charger"
};

static int sgm41511_charger_remove(struct i2c_client *client);
static int sgm41511_charger_probe(struct i2c_client *client,
				 const struct i2c_device_id *id)
{
	struct sgm41511 *sgm;
	const struct of_device_id *match;
	struct device_node *node = client->dev.of_node;
	struct power_supply_config charger_cfg = {};

	int ret = 0;

	sgm = devm_kzalloc(&client->dev, sizeof(struct sgm41511), GFP_KERNEL);
	if (!sgm)
		return -ENOMEM;

	client->addr = 0x1A;
	sgm->dev = &client->dev;
	sgm->client = client;

	i2c_set_clientdata(client, sgm);

	mutex_init(&sgm->i2c_rw_lock);

	ret = sgm41511_detect_device(sgm);
	if (ret) {
		pr_err("No sgm41511 device found!\n");
		return -ENODEV;
	}

	match = of_match_node(sgm41511_charger_match_table, node);
	if (match == NULL) {
		pr_err("device tree match not found\n");
		return -EINVAL;
	}

	if ( sgm->e_part_no == EXTRA_SGM41511 &&
		sgm->reg0c_no != REG0C_ETA6953)
	{
		pr_info("part match, hw:%s, devicetree:%s, extra part no:%d, reg0c_no=%d\n",
			pn_str[sgm->part_no], pn_str[*(int *) match->data], sgm->e_part_no, sgm->reg0c_no);
	} else {

		pr_info("part no match, hw:%s, devicetree:%s, extra part no:%d, reg0c_no=%d\n",
			pn_str[sgm->part_no], pn_str[*(int *) match->data], sgm->e_part_no, sgm->reg0c_no);
		sgm41511_charger_remove(client);
		return -EINVAL;
	}

	sgm->platform_data = sgm41511_parse_dt(node, sgm);

	if (!sgm->platform_data) {
		pr_err("No platform data provided.\n");
		return -EINVAL;
	}

	ret = sgm41511_init_device(sgm);
	if (ret) {
		pr_err("Failed to init device\n");
		return ret;
	}
/*C3T code for HQ-228593 by tongjiacheng at 2022/08/04 start*/
	INIT_DELAYED_WORK(&sgm->psy_dwork, sgm41511_inform_psy_dwork_handler);
/*C3T code for HQ-228593 by tongjiacheng at 2022/08/04 end*/
/* C3T code for HQHW-2797 by tongjiacheng at 2022/09/13 start */
	INIT_DELAYED_WORK(&sgm->force_detect_dwork, sgm41511_force_detection_dwork_handler);
/* C3T code for HQHW-2797 by tongjiacheng at 2022/09/13 end */

	sgm41511_register_interrupt(sgm);

	sgm->chg_dev = charger_device_register(sgm->chg_dev_name,
							&client->dev, sgm,
							&sgm41511_chg_ops,
							&sgm41511_chg_props);
	if (IS_ERR_OR_NULL(sgm->chg_dev)) {
		ret = PTR_ERR(sgm->chg_dev);
		return ret;
	}

	ret = sysfs_create_group(&sgm->dev->kobj, &sgm41511_attr_group);
	if (ret)
		dev_err(sgm->dev, "failed to register sysfs. err: %d\n", ret);

	determine_initial_status(sgm);

	/* power supply register */
	memcpy(&sgm->psy_desc,
		&charger_ic_desc, sizeof(sgm->psy_desc));
	sgm->psy_desc.name = dev_name(&client->dev);

	charger_cfg.drv_data = sgm;
	charger_cfg.of_node = sgm->dev->of_node;
	charger_cfg.supplied_to = charger_ic_supplied_to;
	charger_cfg.num_supplicants = ARRAY_SIZE(charger_ic_supplied_to);
	sgm->psy1 = devm_power_supply_register(&client->dev,
					&sgm->psy_desc, &charger_cfg);
	if (IS_ERR(sgm->psy)) {
		dev_notice(&client->dev, "Fail to register power supply dev\n");
		ret = PTR_ERR(sgm->psy);
	}

	pr_err("sgm41511 probe successfully, Part Num:%d-%d, Revision:%d\n!",
			sgm->part_no, sgm->e_part_no, sgm->revision);

	return 0;
}

static int sgm41511_charger_remove(struct i2c_client *client)
{
	struct sgm41511 *sgm = i2c_get_clientdata(client);

	mutex_destroy(&sgm->i2c_rw_lock);

	sysfs_remove_group(&sgm->dev->kobj, &sgm41511_attr_group);

	return 0;
}

static void sgm41511_charger_shutdown(struct i2c_client *client)
{

}

/*C3T code for HQ-226181 by tongjiacheng at 2022/07/28 start*/
static int sgm41511_suspend(struct device *dev)
{
	struct sgm41511 *sgm = dev_get_drvdata(dev);

	pr_err("%s\n", __func__);

	if (device_may_wakeup(dev))
			enable_irq_wake(sgm->client->irq);

	disable_irq(sgm->client->irq);

	return 0;
}

static int sgm41511_resume(struct device *dev)
{
	struct sgm41511 *sgm = dev_get_drvdata(dev);

	pr_err("%s\n", __func__);
	enable_irq(sgm->client->irq);
	if (device_may_wakeup(dev))
		disable_irq_wake(sgm->client->irq);

	return 0;
}

static SIMPLE_DEV_PM_OPS(sgm41511_pm_ops, sgm41511_suspend, sgm41511_resume);

static struct i2c_driver sgm41511_charger_driver = {
	.driver = {
			.name = "sgm41511-charger",
			.owner = THIS_MODULE,
			.of_match_table = sgm41511_charger_match_table,
			.pm = &sgm41511_pm_ops,
	},
/*C3T code for HQ-226181 by tongjiacheng at 2022/07/28 end*/
	.probe = sgm41511_charger_probe,
	.remove = sgm41511_charger_remove,
	.shutdown = sgm41511_charger_shutdown,

};

module_i2c_driver(sgm41511_charger_driver);

MODULE_DESCRIPTION("SGM41511 Charger Driver");
MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("SGM");
