// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2022 Southchip Semiconductor Technology(Shanghai) Co., Ltd.
 */
#define pr_fmt(fmt)	"[sc8960x]:%s: " fmt, __func__

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
#include <mt-plat/v1/charger_type.h>
/* C3T code for HQHW-2797 by tongjiacheng at 2022/09/13 start*/
#include <mt-plat/upmu_common.h>
/* C3T code for HQHW-2797 by tongjiacheng at 2022/09/13 end*/

#include "mtk_charger_intf.h"
#include "sc8960x_reg.h"
#include "sc8960x.h"

enum {
	PN_SC89601D,
};

/* C3T code for HQ-249726 by tongjiacheng at 2022/09/30 start */
enum sc8960x_part_no {
	SC89601D = 0x03,
};

static int pn_data[] = {
	[PN_SC89601D] = 0x03,
};
/* C3T code for HQ-249726 by tongjiacheng at 2022/09/30 end*/

static char *pn_str[] = {
	[PN_SC89601D] = "sc89601d",
};

struct sc8960x {
	struct device *dev;
	struct i2c_client *client;

	enum sc8960x_part_no part_no;
	int revision;

	const char *chg_dev_name;
	const char *eint_name;

	bool chg_det_enable;

	//enum charger_type chg_type;
	int psy_usb_type;
	struct delayed_work psy_dwork;

	int status;
	int irq;

	struct mutex i2c_rw_lock;

	bool charge_enabled;	/* Register bit status */
	bool power_good;

	struct sc8960x_platform_data *platform_data;
	struct charger_device *chg_dev;

	struct power_supply *psy;
/* C3T code for HQ-237347 by tongjiacheng at 2022/08/31 start*/
	bool hiz_mode;
	bool vbus_good;
/* C3T code for HQ-237347 by tongjiacheng at 2022/08/31 end*/
/* C3T code for HQHW-2797 by tongjiacheng at 2022/09/13 start*/
	struct delayed_work force_detect_dwork;
	u8 force_detect_count;
/* C3T code for HQ-253377 by tongjiacheng at 2022/10/24 start*/
	u8 first_sdp_detect;
/* C3T code for HQ-253377 by tongjiacheng at 2022/10/24 end*/
/* C3T code for HQHW-2797 by tongjiacheng at 2022/09/13 end*/
};

static const struct charger_properties sc8960x_chg_props = {
	.alias_name = "sc8960x",
};

/* C3T code for HQHW-3513 by tongjiacheng at 2022/10/18 start*/
extern struct musb *mtk_musb;
extern void hq_musb_pullup(struct musb *musb, int is_on);
/* C3T code for HQHW-3513 by tongjiacheng at 2022/10/18 end*/

static int __sc8960x_read_reg(struct sc8960x *sc, u8 reg, u8 *data)
{
	s32 ret;

	ret = i2c_smbus_read_byte_data(sc->client, reg);
	if (ret < 0) {
		pr_err("i2c read fail: can't read from reg 0x%02X\n", reg);
		return ret;
	}

	*data = (u8) ret;

	return 0;
}

static int __sc8960x_write_reg(struct sc8960x *sc, int reg, u8 val)
{
	s32 ret;

	ret = i2c_smbus_write_byte_data(sc->client, reg, val);
	if (ret < 0) {
		pr_err("i2c write fail: can't write 0x%02X to reg 0x%02X: %d\n",
		       val, reg, ret);
		return ret;
	}
	return 0;
}

static int sc8960x_read_byte(struct sc8960x *sc, u8 reg, u8 *data)
{
	int ret;

	mutex_lock(&sc->i2c_rw_lock);
	ret = __sc8960x_read_reg(sc, reg, data);
	mutex_unlock(&sc->i2c_rw_lock);

	return ret;
}

static int sc8960x_write_byte(struct sc8960x *sc, u8 reg, u8 data)
{
	int ret;

	mutex_lock(&sc->i2c_rw_lock);
	ret = __sc8960x_write_reg(sc, reg, data);
	mutex_unlock(&sc->i2c_rw_lock);

	if (ret)
		pr_err("Failed: reg=%02X, ret=%d\n", reg, ret);

	return ret;
}

static int sc8960x_update_bits(struct sc8960x *sc, u8 reg, u8 mask, u8 data)
{
	int ret;
	u8 tmp;

	mutex_lock(&sc->i2c_rw_lock);
	ret = __sc8960x_read_reg(sc, reg, &tmp);
	if (ret) {
		pr_err("Failed: reg=%02X, ret=%d\n", reg, ret);
		goto out;
	}

	tmp &= ~mask;
	tmp |= data & mask;

	ret = __sc8960x_write_reg(sc, reg, tmp);
	if (ret)
		pr_err("Failed: reg=%02X, ret=%d\n", reg, ret);

out:
	mutex_unlock(&sc->i2c_rw_lock);
	return ret;
}

/* C3T code for HQ-249726 by tongjiacheng at 2022/09/30 start */
static int sc98960x_set_key(struct sc8960x *sc)
{
	sc8960x_write_byte(sc, 0x7F, 0x5A);
	sc8960x_write_byte(sc, 0x7F, 0x68);
	sc8960x_write_byte(sc, 0x7F, 0x65);
	sc8960x_write_byte(sc, 0x7F, 0x6E);
	sc8960x_write_byte(sc, 0x7F, 0x67);
	sc8960x_write_byte(sc, 0x7F, 0x4C);
	sc8960x_write_byte(sc, 0x7F, 0x69);
	return sc8960x_write_byte(sc, 0x7F, 0x6E);
}

static int sc8960x_set_wa(struct sc8960x *sc)
{
	int ret;
	u8 reg_val;

	ret = sc8960x_read_byte(sc, 0x99, &reg_val);
	if (ret) {
		sc98960x_set_key(sc);
	}
	sc8960x_write_byte(sc, 0x92, 0x71);
	ret = sc8960x_read_byte(sc, 0xB8, &reg_val);
	sc8960x_write_byte(sc, 0x94, 0x10);
	sc8960x_write_byte(sc, 0x96, 0x09 - ((reg_val & 0x80) >> 7));
	sc8960x_write_byte(sc, 0x93, 0x19 + ((reg_val & 0x80) >> 3));
	sc8960x_write_byte(sc, 0x0E, 0xB2 - (reg_val & 0x80));
	sc8960x_write_byte(sc, 0x99, 0x4C - ((reg_val & 0x80) >> 4));

	return sc98960x_set_key(sc);
}

/* C3T code for HQ-253377 by tongjiacheng at 2022/10/24 start*/
static int sc8960x_enable_bc12(struct sc8960x *sc, bool en)
{
	int ret;
	u8 reg_val;

	ret = sc8960x_read_byte(sc, 0x99, &reg_val);
	if (ret) {
		sc98960x_set_key(sc);
	}

	sc8960x_read_byte(sc, 0xB8, &reg_val);
	if (!(reg_val & 0x80)) {
		if (en) {
			sc8960x_write_byte(sc, 0x99, 0x44);
		} else {
			sc8960x_write_byte(sc, 0x99, 0x4C);
		}
	}

	pr_err("southchip sc8960x_enable_bc12	%s\n", en ? "enable":"disable");
	return sc98960x_set_key(sc);
}
/* C3T code for HQ-253377 by tongjiacheng at 2022/10/24 end*/

/* C3T code for HQHW-3513 by tongjiacheng at 2022/10/18 end*/
static int sc8960x_enable_otg(struct sc8960x *sc)
{
	u8 val = REG01_OTG_ENABLE << REG01_OTG_CONFIG_SHIFT;

	return sc8960x_update_bits(sc, SC8960X_REG_01, REG01_OTG_CONFIG_MASK,
				   val);

}

static int sc8960x_disable_otg(struct sc8960x *sc)
{
	u8 val = REG01_OTG_DISABLE << REG01_OTG_CONFIG_SHIFT;

	return sc8960x_update_bits(sc, SC8960X_REG_01, REG01_OTG_CONFIG_MASK,
				   val);

}

static int sc8960x_enable_charger(struct sc8960x *sc)
{
	int ret;
	u8 val = REG01_CHG_ENABLE << REG01_CHG_CONFIG_SHIFT;

	ret =
	    sc8960x_update_bits(sc, SC8960X_REG_01, REG01_CHG_CONFIG_MASK, val);

	return ret;
}

static int sc8960x_disable_charger(struct sc8960x *sc)
{
	int ret;
	u8 val = REG01_CHG_DISABLE << REG01_CHG_CONFIG_SHIFT;

	ret =
	    sc8960x_update_bits(sc, SC8960X_REG_01, REG01_CHG_CONFIG_MASK, val);
	return ret;
}

int sc8960x_set_chargecurrent(struct sc8960x *sc, int curr)
{
	u8 ichg;

	if (curr < REG02_ICHG_BASE)
		curr = REG02_ICHG_BASE;

	ichg = (curr - REG02_ICHG_BASE) / REG02_ICHG_LSB;
	return sc8960x_update_bits(sc, SC8960X_REG_02, REG02_ICHG_MASK,
				   ichg << REG02_ICHG_SHIFT);

}

int sc8960x_set_term_current(struct sc8960x *sc, int curr)
{
	u8 iterm;

	if (curr < REG03_ITERM_BASE)
		curr = REG03_ITERM_BASE;

	iterm = (curr - REG03_ITERM_BASE) / REG03_ITERM_LSB;

	return sc8960x_update_bits(sc, SC8960X_REG_03, REG03_ITERM_MASK,
				   iterm << REG03_ITERM_SHIFT);
}
EXPORT_SYMBOL_GPL(sc8960x_set_term_current);

int sc8960x_set_prechg_current(struct sc8960x *sc, int curr)
{
	u8 iprechg;

	if (curr < REG03_IPRECHG_BASE)
		curr = REG03_IPRECHG_BASE;

	iprechg = (curr - REG03_IPRECHG_BASE) / REG03_IPRECHG_LSB;

	return sc8960x_update_bits(sc, SC8960X_REG_03, REG03_IPRECHG_MASK,
				   iprechg << REG03_IPRECHG_SHIFT);
}
EXPORT_SYMBOL_GPL(sc8960x_set_prechg_current);

/* C3T code for HQ-223445 by tongjiacheng at 2022/08/30 start */
int sc8960x_set_chargevolt(struct sc8960x *sc, int volt)
{
	u8 val;
	u8 vreg_ft;
	int ret;

	if (volt < REG04_VREG_BASE)
		volt = REG04_VREG_BASE;

	if (((volt - REG04_VREG_BASE - 8) % REG04_VREG_LSB) == 0) {
		volt -= 8;
		vreg_ft = REG0D_VREG_INC_8MV;
	}
	else if (((volt - REG04_VREG_BASE - 16) % REG04_VREG_LSB) == 0) {
		volt -= 16;
		vreg_ft = REG0D_VREG_INC_16MV;
	}
	else if (((volt - REG04_VREG_BASE - 24) % REG04_VREG_LSB) == 0) {
		volt -= 24;
		vreg_ft = REG0D_VREG_INC_24MV;
	}
	else
		vreg_ft = REG0D_VREG_DEFAULT;

	val = (volt - REG04_VREG_BASE ) / REG04_VREG_LSB;

	ret = sc8960x_update_bits(sc, SC8960X_REG_04, REG04_VREG_MASK,
				   val << REG04_VREG_SHIFT);
	if (ret) {
		dev_err(sc->dev, "%s: failed to set charger volt\n", __func__);
		return ret;
	}

	ret = sc8960x_update_bits(sc, SC8960X_REG_0D, REG0D_VREG_FT_MASK,
					vreg_ft << REG0D_VREG_FT_SHIFT);
	if (ret) {
		dev_err(sc->dev, "%s: failed to set charger volt ft\n", __func__);
		return ret;
	}

	return 0;
}
/* C3T code for HQ-223445 by tongjiacheng at 2022/08/30 end*/

int sc8960x_set_input_volt_limit(struct sc8960x *sc, int volt)
{
	u8 val;

	if (volt < REG06_VINDPM_BASE)
		volt = REG06_VINDPM_BASE;

	val = (volt - REG06_VINDPM_BASE) / REG06_VINDPM_LSB;
	return sc8960x_update_bits(sc, SC8960X_REG_06, REG06_VINDPM_MASK,
				   val << REG06_VINDPM_SHIFT);
}

int sc8960x_set_input_current_limit(struct sc8960x *sc, int curr)
{
	u8 val;

	if (curr < REG00_IINLIM_BASE)
		curr = REG00_IINLIM_BASE;

	val = (curr - REG00_IINLIM_BASE) / REG00_IINLIM_LSB;
	return sc8960x_update_bits(sc, SC8960X_REG_00, REG00_IINLIM_MASK,
				   val << REG00_IINLIM_SHIFT);
}

int sc8960x_set_watchdog_timer(struct sc8960x *sc, u8 timeout)
{
	u8 temp;

	temp = (u8) (((timeout -
		       REG05_WDT_BASE) / REG05_WDT_LSB) << REG05_WDT_SHIFT);

	return sc8960x_update_bits(sc, SC8960X_REG_05, REG05_WDT_MASK, temp);
}
EXPORT_SYMBOL_GPL(sc8960x_set_watchdog_timer);

int sc8960x_disable_watchdog_timer(struct sc8960x *sc)
{
	u8 val = REG05_WDT_DISABLE << REG05_WDT_SHIFT;

	return sc8960x_update_bits(sc, SC8960X_REG_05, REG05_WDT_MASK, val);
}
EXPORT_SYMBOL_GPL(sc8960x_disable_watchdog_timer);

int sc8960x_reset_watchdog_timer(struct sc8960x *sc)
{
	u8 val = REG01_WDT_RESET << REG01_WDT_RESET_SHIFT;

	return sc8960x_update_bits(sc, SC8960X_REG_01, REG01_WDT_RESET_MASK,
				   val);
}
EXPORT_SYMBOL_GPL(sc8960x_reset_watchdog_timer);

int sc8960x_reset_chip(struct sc8960x *sc)
{
	int ret;
	u8 val = REG0B_REG_RESET << REG0B_REG_RESET_SHIFT;

	ret =
	    sc8960x_update_bits(sc, SC8960X_REG_0B, REG0B_REG_RESET_MASK, val);
	return ret;
}
EXPORT_SYMBOL_GPL(sc8960x_reset_chip);

int sc8960x_enter_hiz_mode(struct sc8960x *sc)
{
	u8 val = REG00_HIZ_ENABLE << REG00_ENHIZ_SHIFT;

	return sc8960x_update_bits(sc, SC8960X_REG_00, REG00_ENHIZ_MASK, val);

}
EXPORT_SYMBOL_GPL(sc8960x_enter_hiz_mode);

int sc8960x_exit_hiz_mode(struct sc8960x *sc)
{

	u8 val = REG00_HIZ_DISABLE << REG00_ENHIZ_SHIFT;

	return sc8960x_update_bits(sc, SC8960X_REG_00, REG00_ENHIZ_MASK, val);

}
EXPORT_SYMBOL_GPL(sc8960x_exit_hiz_mode);

static int sc8960x_enable_term(struct sc8960x *sc, bool enable)
{
	u8 val;
	int ret;

	if (enable)
		val = REG05_TERM_ENABLE << REG05_EN_TERM_SHIFT;
	else
		val = REG05_TERM_DISABLE << REG05_EN_TERM_SHIFT;

	ret = sc8960x_update_bits(sc, SC8960X_REG_05, REG05_EN_TERM_MASK, val);

	return ret;
}
EXPORT_SYMBOL_GPL(sc8960x_enable_term);

int sc8960x_set_boost_current(struct sc8960x *sc, int curr)
{
	u8 val;

	val = REG02_BOOST_LIM_0P5A;
	if (curr == BOOSTI_1200)
		val = REG02_BOOST_LIM_1P2A;

	return sc8960x_update_bits(sc, SC8960X_REG_02, REG02_BOOST_LIM_MASK,
				   val << REG02_BOOST_LIM_SHIFT);
}

int sc8960x_set_boost_voltage(struct sc8960x *sc, int volt)
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

	return sc8960x_update_bits(sc, SC8960X_REG_06, REG06_BOOSTV_MASK,
				   val << REG06_BOOSTV_SHIFT);
}
EXPORT_SYMBOL_GPL(sc8960x_set_boost_voltage);

static int sc8960x_set_acovp_threshold(struct sc8960x *sc, int volt)
{
	u8 val;

	if (volt == VAC_OVP_14000)
		val = REG06_OVP_14V;
	else if (volt == VAC_OVP_10500)
		val = REG06_OVP_10P5V;
	else if (volt == VAC_OVP_6500)
		val = REG06_OVP_6P5V;
	else
		val = REG06_OVP_5P8V;

	return sc8960x_update_bits(sc, SC8960X_REG_06, REG06_OVP_MASK,
				   val << REG06_OVP_SHIFT);
}
EXPORT_SYMBOL_GPL(sc8960x_set_acovp_threshold);

static int sc8960x_set_stat_ctrl(struct sc8960x *sc, int ctrl)
{
	u8 val;

	val = ctrl;

	return sc8960x_update_bits(sc, SC8960X_REG_00, REG00_STAT_CTRL_MASK,
				   val << REG00_STAT_CTRL_SHIFT);
}

static int sc8960x_set_int_mask(struct sc8960x *sc, int mask)
{
	u8 val;

	val = mask;

	return sc8960x_update_bits(sc, SC8960X_REG_0A, REG0A_INT_MASK_MASK,
				   val << REG0A_INT_MASK_SHIFT);
}

static int sc8960x_enable_batfet(struct sc8960x *sc)
{
	const u8 val = REG07_BATFET_ON << REG07_BATFET_DIS_SHIFT;

	return sc8960x_update_bits(sc, SC8960X_REG_07, REG07_BATFET_DIS_MASK,
				   val);
}
EXPORT_SYMBOL_GPL(sc8960x_enable_batfet);

static int sc8960x_disable_batfet(struct sc8960x *sc)
{
	const u8 val = REG07_BATFET_OFF << REG07_BATFET_DIS_SHIFT;

	return sc8960x_update_bits(sc, SC8960X_REG_07, REG07_BATFET_DIS_MASK,
				   val);
}
EXPORT_SYMBOL_GPL(sc8960x_disable_batfet);

static int sc8960x_set_batfet_delay(struct sc8960x *sc, uint8_t delay)
{
	u8 val;

	if (delay == 0)
		val = REG07_BATFET_DLY_0S;
	else
		val = REG07_BATFET_DLY_10S;

	val <<= REG07_BATFET_DLY_SHIFT;

	return sc8960x_update_bits(sc, SC8960X_REG_07, REG07_BATFET_DLY_MASK,
				   val);
}
EXPORT_SYMBOL_GPL(sc8960x_set_batfet_delay);

static int sc8960x_enable_safety_timer(struct sc8960x *sc)
{
	const u8 val = REG05_CHG_TIMER_ENABLE << REG05_EN_TIMER_SHIFT;

	return sc8960x_update_bits(sc, SC8960X_REG_05, REG05_EN_TIMER_MASK,
				   val);
}
EXPORT_SYMBOL_GPL(sc8960x_enable_safety_timer);

static int sc8960x_disable_safety_timer(struct sc8960x *sc)
{
	const u8 val = REG05_CHG_TIMER_DISABLE << REG05_EN_TIMER_SHIFT;

	return sc8960x_update_bits(sc, SC8960X_REG_05, REG05_EN_TIMER_MASK,
				   val);
}
EXPORT_SYMBOL_GPL(sc8960x_disable_safety_timer);

static struct sc8960x_platform_data *sc8960x_parse_dt(struct device_node *np,
						      struct sc8960x *sc)
{
	int ret;
	struct sc8960x_platform_data *pdata;

	pdata = devm_kzalloc(sc->dev, sizeof(struct sc8960x_platform_data),
			     GFP_KERNEL);
	if (!pdata)
		return NULL;

	if (of_property_read_string(np, "charger_name", &sc->chg_dev_name) < 0) {
		sc->chg_dev_name = "primary_chg";
		pr_warn("no charger name\n");
	}

	if (of_property_read_string(np, "eint_name", &sc->eint_name) < 0) {
		sc->eint_name = "chr_stat";
		pr_warn("no eint name\n");
	}

	sc->chg_det_enable =
	    of_property_read_bool(np, "sc,sc8960x,charge-detect-enable");

	ret = of_property_read_u32(np, "sc,sc8960x,usb-vlim", &pdata->usb.vlim);
	if (ret) {
		pdata->usb.vlim = 4500;
		pr_err("Failed to read node of sc,sc8960x,usb-vlim\n");
	}

	ret = of_property_read_u32(np, "sc,sc8960x,usb-ilim", &pdata->usb.ilim);
	if (ret) {
		pdata->usb.ilim = 2000;
		pr_err("Failed to read node of sc,sc8960x,usb-ilim\n");
	}

	ret = of_property_read_u32(np, "sc,sc8960x,usb-vreg", &pdata->usb.vreg);
	if (ret) {
		pdata->usb.vreg = 4200;
		pr_err("Failed to read node of sc,sc8960x,usb-vreg\n");
	}

	ret = of_property_read_u32(np, "sc,sc8960x,usb-ichg", &pdata->usb.ichg);
	if (ret) {
		pdata->usb.ichg = 2000;
		pr_err("Failed to read node of sc,sc8960x,usb-ichg\n");
	}

	ret = of_property_read_u32(np, "sc,sc8960x,stat-pin-ctrl",
				   &pdata->statctrl);
	if (ret) {
		pdata->statctrl = 0;
		pr_err("Failed to read node of sc,sc8960x,stat-pin-ctrl\n");
	}

	ret = of_property_read_u32(np, "sc,sc8960x,precharge-current",
				   &pdata->iprechg);
	if (ret) {
		pdata->iprechg = 180;
		pr_err("Failed to read node of sc,sc8960x,precharge-current\n");
	}

	ret = of_property_read_u32(np, "sc,sc8960x,termination-current",
				   &pdata->iterm);
	if (ret) {
		pdata->iterm = 180;
		pr_err
		    ("Failed to read node of sc,sc8960x,termination-current\n");
	}

	ret =
	    of_property_read_u32(np, "sc,sc8960x,boost-voltage",
				 &pdata->boostv);
	if (ret) {
		pdata->boostv = 5000;
		pr_err("Failed to read node of sc,sc8960x,boost-voltage\n");
	}

	ret =
	    of_property_read_u32(np, "sc,sc8960x,boost-current",
				 &pdata->boosti);
	if (ret) {
		pdata->boosti = 1200;
		pr_err("Failed to read node of sc,sc8960x,boost-current\n");
	}

	ret = of_property_read_u32(np, "sc,sc8960x,vac-ovp-threshold",
				   &pdata->vac_ovp);
	if (ret) {
		pdata->vac_ovp = 6500;
		pr_err("Failed to read node of sc,sc8960x,vac-ovp-threshold\n");
	}

	return pdata;
}

/* C3T code for HQHW-2797 by tongjiacheng at 2022/09/13 start*/
/* C3T code for HQ-253377 by tongjiacheng at 2022/10/24 start*/
static int sc8960x_force_dpdm(struct sc8960x *sc)
{
	int ret;
	u8 data;

	sc8960x_write_byte(sc, 0x00, 0x04);
	ret = sc8960x_read_byte(sc, 0x81, &data);
	if (ret) {
		sc8960x_write_byte(sc, 0x7D, 0x36);
		sc8960x_write_byte(sc, 0x7D, 0x48);
		sc8960x_write_byte(sc, 0x7D, 0x54);
		sc8960x_write_byte(sc, 0x7D, 0x4C);
		sc8960x_write_byte(sc, 0x81, 0x25);
	}
	msleep(250);

	ret = sc8960x_update_bits(sc, SC8960X_REG_07, REG07_FORCE_DPDM_MASK,
						REG07_FORCE_DPDM << REG07_FORCE_DPDM_SHIFT);

	sc8960x_write_byte(sc, 0x7D, 0x36);
	sc8960x_write_byte(sc, 0x7D, 0x48);
	sc8960x_write_byte(sc, 0x7D, 0x54);
	sc8960x_write_byte(sc, 0x7D, 0x4C);
	ret = sc8960x_update_bits(sc, SC8960X_REG_07, REG07_FORCE_DPDM_MASK,
						REG07_FORCE_DPDM << REG07_FORCE_DPDM_SHIFT);

	return ret;
}
/* C3T code for HQ-253377 by tongjiacheng at 2022/10/24 end*/

static void sc8960x_force_detection_dwork_handler(struct work_struct *work)
{
	int ret;
	struct sc8960x *sc = container_of(work, struct sc8960x, force_detect_dwork.work);

	Charger_Detect_Init();
/* C3T code for HQHW-3513 by tongjiacheng at 2022/10/18 start*/
	hq_musb_pullup(mtk_musb, 0);
/* C3T code for HQHW-3513 by tongjiacheng at 2022/10/18 end*/

	ret = sc8960x_force_dpdm(sc);
	if (ret) {
		dev_err(sc->dev, "%s: failed to force detection\n", __func__);
		return;
	}

	sc->power_good = false;
	sc->force_detect_count++;
}
/* C3T code for HQHW-2797 by tongjiacheng at 2022/09/13 end*/

static int sc8960x_get_charger_type(struct sc8960x *sc, int *type)
{
	int ret;

	u8 reg_val = 0;
	int vbus_stat = 0;
	int  chg_type = CHARGER_UNKNOWN;

	ret = sc8960x_read_byte(sc, SC8960X_REG_08, &reg_val);

	if (ret)
		return ret;

	vbus_stat = (reg_val & REG08_VBUS_STAT_MASK);
	vbus_stat >>= REG08_VBUS_STAT_SHIFT;

	switch (vbus_stat) {

	case REG08_VBUS_TYPE_NONE:
		chg_type = CHARGER_UNKNOWN;
		break;
	case REG08_VBUS_TYPE_SDP:
	/* C3T code for HQ-253377 by tongjiacheng at 2022/10/24 start*/
		if (sc->first_sdp_detect == 0) {
			sc8960x_enable_bc12(sc, false);
			chg_type = NONSTANDARD_CHARGER;
			msleep(100);
			sc8960x_force_dpdm(sc);
			sc->power_good = false;
			sc->first_sdp_detect = 1;
			return 0;
		}
	/* C3T code for HQ-253377 by tongjiacheng at 2022/10/24 end*/
		chg_type = STANDARD_HOST;
		break;
	case REG08_VBUS_TYPE_CDP:
		chg_type = CHARGING_HOST;
	/* C3T code for HQ-257872by tongjiacheng at 2022/11/03 start*/
		hq_musb_pullup(mtk_musb, 1);
	/* C3T code for HQ-257872by tongjiacheng at 2022/11/03 end*/
        /* C3T code for HQ-258572 by tongjiacheng at 2022/11/03 start*/
		sc8960x_set_input_volt_limit(sc, 4800);
        /* C3T code for HQ-258572 by tongjiacheng at 2022/11/03 end*/
		break;
	case REG08_VBUS_TYPE_DCP:
		chg_type = STANDARD_CHARGER;
		break;
	case REG08_VBUS_TYPE_UNKNOWN:
/* C3T code for HQ-249726 by tongjiacheng at 2022/09/30 start */
	case REG08_VBUS_TYPE_NON_STD:
/* C3T code for HQ-249726 by tongjiacheng at 2022/09/30 end*/
	/* C3T code for HQHW-2797 by tongjiacheng at 2022/09/13 start*/
		chg_type = NONSTANDARD_CHARGER;
	/* C3T code for HQ-253377 by tongjiacheng at 2022/10/24 start*/
        /* C3T code for HQ-258572 by tongjiacheng at 2022/11/06 start*/
		sc8960x_set_input_volt_limit(sc, 4800);
        /* C3T code for HQ-258572 by tongjiacheng at 2022/11/06 end*/
		if (sc->force_detect_count < 10) {
			if (sc->force_detect_count % 2 == 0) {
				sc8960x_enable_bc12(sc, true);
			} else {
				sc8960x_enable_bc12(sc, false);
			}
			schedule_delayed_work(&sc->force_detect_dwork, msecs_to_jiffies(2000));
		}
	/* C3T code for HQ-253377 by tongjiacheng at 2022/10/24 end*/
	/* C3T code for HQHW-2797 by tongjiacheng at 2022/09/13 end*/
		break;
	default:
		chg_type = NONSTANDARD_CHARGER;
		break;
	}
/* C3T code for HQHW-2797 by tongjiacheng at 2022/09/13 start*/
/* C3T code for HQ-253377 by tongjiacheng at 2022/10/24 start*/
	if (chg_type != STANDARD_CHARGER && chg_type != NONSTANDARD_CHARGER)
/* C3T code for HQ-253377 by tongjiacheng at 2022/10/24 end*/
		Charger_Detect_Release();
/* C3T code for HQHW-2797 by tongjiacheng at 2022/09/13 end*/
	*type = chg_type;

	return 0;
}

/* C3T code for HQ-237347 by tongjiacheng at 2022/08/31 start */
static int sc8960x_set_hiz_mode(struct charger_device *chg_dev, bool en);
/* C3T code for HQ-237347 by tongjiacheng at 2022/08/31 end */
static void sc8960x_inform_psy_dwork_handler(struct work_struct *work)
{
	int ret = 0;
	union power_supply_propval propval;
/* C3T code for HQ-259048 by tongjiacheng at 2022/10/28 start*/
	struct mt_charger *mt_chg = NULL;
/* C3T code for HQ-259048 by tongjiacheng at 2022/10/28 end*/

	struct sc8960x *sc = container_of(work, struct sc8960x,
								psy_dwork.work);

	if (!sc->psy) {
		sc->psy = power_supply_get_by_name("charger");
		if (!sc->psy) {
			pr_err("%s get power supply fail\n", __func__);
			mod_delayed_work(system_wq, &sc->psy_dwork, 
					msecs_to_jiffies(2000));
			return ;
		}
/* C3T code for HQ-259048 by tongjiacheng at 2022/10/28 start*/
		else {
			mt_chg = power_supply_get_drvdata(sc->psy);
			if (!mt_chg->init_done) {
				pr_err("%s mtk charger init failed\n", __func__);
				mod_delayed_work(system_wq, &sc->psy_dwork,
					msecs_to_jiffies(2000));
				return ;
			}
		}
/* C3T code for HQ-259048 by tongjiacheng at 2022/10/28 end*/
	}
/* C3T code for HQ-259695 by tongjiacheng at 2022/10/31 start */
	if (sc->psy_usb_type != CHARGER_UNKNOWN || sc->vbus_good)
		propval.intval = 1;
	else
		propval.intval = 0;
/* C3T code for HQ-259695 by tongjiacheng at 2022/10/31 end */
	ret = power_supply_set_property(sc->psy, POWER_SUPPLY_PROP_ONLINE,
					&propval);

	if (ret < 0)
		pr_notice("inform power supply online failed:%d\n", ret);

	propval.intval = sc->psy_usb_type;

	ret = power_supply_set_property(sc->psy,
					POWER_SUPPLY_PROP_CHARGE_TYPE,
					&propval);

	if (ret < 0)
		pr_notice("inform power supply charge type failed:%d\n", ret);
/* C3T code for HQ-237347 by tongjiacheng at 2022/08/31 start */
	sc8960x_set_hiz_mode(sc->chg_dev, sc->hiz_mode);
/* C3T code for HQ-237347 by tongjiacheng at 2022/08/31 end*/
}

/* C3T code for HQ-237347 by tongjiacheng at 2022/08/31 start */
static irqreturn_t sc8960x_irq_handler(int irq, void *data)
{
	int ret;
	u8 reg_val;
	bool prev_pg;
	bool prev_vbus_gd;
	struct sc8960x *sc = (struct sc8960x *)data;

	ret = sc8960x_read_byte(sc, SC8960X_REG_08, &reg_val);
	if (ret)
		return IRQ_HANDLED;

	prev_pg = sc->power_good;

	sc->power_good = !!(reg_val & REG08_PG_STAT_MASK);

	ret = sc8960x_read_byte(sc, SC8960X_REG_0A, &reg_val);
	if (ret)
		return IRQ_HANDLED;

	prev_vbus_gd = sc->vbus_good;

	sc->vbus_good = !!(reg_val & REG0A_VBUS_GD_MASK);

/* C3T code for HQHW-2797 by tongjiacheng at 2022/09/13 start*/
	if (!prev_vbus_gd && sc->vbus_good) {
		pr_notice("adapter/usb inserted\n");
		sc->force_detect_count = 0;
	/* C3T code for HQ-253377 by tongjiacheng at 2022/10/24 start*/
		sc->first_sdp_detect = 0;
	/* C3T code for HQ-253377 by tongjiacheng at 2022/10/24 end*/
/* C3T code for HQHW-2797 by tongjiacheng at 2022/09/13 end*/
	}
	else if (prev_vbus_gd && !sc->vbus_good) {
		pr_notice("adapter/usb removed\n");
		/* C3T code for HQ-253377 by tongjiacheng at 2022/10/24 start*/
		sc8960x_enable_bc12(sc, false);
		/* C3T code for HQ-253377 by tongjiacheng at 2022/10/24 end*/
		sc8960x_get_charger_type(sc, &sc->psy_usb_type);
		schedule_delayed_work(&sc->psy_dwork, 0);
/* C3T code for HQHW-2797 by tongjiacheng at 2022/09/13 start*/
		Charger_Detect_Init();
/* C3T code for HQHW-3513 by tongjiacheng at 2022/10/18 start*/
		hq_musb_pullup(mtk_musb, 0);
/* C3T code for HQHW-3513 by tongjiacheng at 2022/10/18 end*/
/* C3T code for HQHW-2797 by tongjiacheng at 2022/09/13 end*/
		return IRQ_HANDLED;
	}

	if (!prev_pg && sc->power_good) {
		sc8960x_get_charger_type(sc, &sc->psy_usb_type);
		schedule_delayed_work(&sc->psy_dwork, 0);
	}

	return IRQ_HANDLED;
}
/* C3T code for HQ-237347 by tongjiacheng at 2022/08/31 end*/

static int sc8960x_register_interrupt(struct sc8960x *sc)
{
	int ret = 0;
	//struct device_node *np;

	/*np = of_find_node_by_name(NULL, sc->eint_name);
	if (np) {
		sc->irq = irq_of_parse_and_map(np, 0);
	} else {
		pr_err("couldn't get irq node\n");
		return -ENODEV;
	}

	pr_info("irq = %d\n", sc->irq);*/

	if (! sc->client->irq) {
		pr_info("sc->client->irq is NULL\n");//remember to config dws
		return -ENODEV;
	}

	ret = devm_request_threaded_irq(sc->dev, sc->client->irq, NULL,
					sc8960x_irq_handler,
					IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
					"ti_irq", sc);
	if (ret < 0) {
		pr_err("request thread irq failed:%d\n", ret);
		return ret;
	}

	device_init_wakeup(sc->dev, true);

	return 0;
}

static int sc8960x_init_device(struct sc8960x *sc)
{
	int ret;

	sc8960x_disable_watchdog_timer(sc);

	/* C3T code for HQHW-3610 by tongjiacheng at 2022/10/19 start*/
	ret = sc8960x_update_bits(sc, SC8960X_REG_05, REG05_EN_TIMER_MASK,
					REG05_CHG_TIMER_DISABLE << REG05_EN_TIMER_SHIFT);
	if (ret)
		pr_err("Failed to disable safty time\n");
	/* C3T code for HQHW-3610 by tongjiacheng at 2022/10/19 end*/

	ret = sc8960x_set_stat_ctrl(sc, sc->platform_data->statctrl);
	if (ret)
		pr_err("Failed to set stat pin control mode, ret = %d\n", ret);

	ret = sc8960x_set_prechg_current(sc, sc->platform_data->iprechg);
	if (ret)
		pr_err("Failed to set prechg current, ret = %d\n", ret);

	ret = sc8960x_set_term_current(sc, sc->platform_data->iterm);
	if (ret)
		pr_err("Failed to set termination current, ret = %d\n", ret);

	ret = sc8960x_set_boost_voltage(sc, sc->platform_data->boostv);
	if (ret)
		pr_err("Failed to set boost voltage, ret = %d\n", ret);

	ret = sc8960x_set_boost_current(sc, sc->platform_data->boosti);
	if (ret)
		pr_err("Failed to set boost current, ret = %d\n", ret);

	ret = sc8960x_set_acovp_threshold(sc, sc->platform_data->vac_ovp);
	if (ret)
		pr_err("Failed to set acovp threshold, ret = %d\n", ret);

	ret = sc8960x_set_int_mask(sc,
				   REG0A_IINDPM_INT_MASK |
				   REG0A_VINDPM_INT_MASK);
	if (ret)
		pr_err("Failed to set vindpm and iindpm int mask\n");

/* C3T code for HQ-249726 by tongjiacheng at 2022/09/30 start */
	ret = sc8960x_set_wa(sc);
	if (ret)
		pr_err("Failed to set sc private\n");
/* C3T code for HQ-249726 by tongjiacheng at 2022/09/30 end*/
/* C3T code for HQ-253377 by tongjiacheng at 2022/10/24 start*/
	ret = sc8960x_enable_bc12(sc, false);
	if (ret)
		pr_err("Failed to disable bc12\n");
/* C3T code for HQ-253377 by tongjiacheng at 2022/10/24 end*/
/* C3T code for HQHW-35136 by tongjiacheng at 2022/10/18 end*/
	return 0;
}

static void determine_initial_status(struct sc8960x *sc)
{
	sc8960x_irq_handler(sc->irq, (void *) sc);
}

static int sc8960x_detect_device(struct sc8960x *sc)
{
	int ret;
	u8 data;

	ret = sc8960x_read_byte(sc, SC8960X_REG_0B, &data);
	if (!ret) {
		sc->part_no = (data & REG0B_PN_MASK) >> REG0B_PN_SHIFT;
		sc->revision =
		    (data & REG0B_DEV_REV_MASK) >> REG0B_DEV_REV_SHIFT;
	}

	return ret;
}

static void sc8960x_dump_regs(struct sc8960x *sc)
{
	int addr;
	u8 val;
	int ret;

	for (addr = 0x0; addr <= 0x0D; addr++) {
		ret = sc8960x_read_byte(sc, addr, &val);
		if (ret == 0)
			pr_err("Reg[%.2x] = 0x%.2x\n", addr, val);
	}
}

static ssize_t
sc8960x_show_registers(struct device *dev, struct device_attribute *attr,
		       char *buf)
{
	struct sc8960x *sc = dev_get_drvdata(dev);
	u8 addr;
	u8 val;
	u8 tmpbuf[200];
	int len;
	int idx = 0;
	int ret;

	idx = snprintf(buf, PAGE_SIZE, "%s:\n", "sc8960x Reg");
	for (addr = 0x0; addr <= 0x0B; addr++) {
		ret = sc8960x_read_byte(sc, addr, &val);
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
sc8960x_store_registers(struct device *dev,
			struct device_attribute *attr, const char *buf,
			size_t count)
{
	struct sc8960x *sc = dev_get_drvdata(dev);
	int ret;
	unsigned int reg;
	unsigned int val;

	ret = sscanf(buf, "%x %x", &reg, &val);
	if (ret == 2 && reg < 0x0B) {
		sc8960x_write_byte(sc, (unsigned char) reg,
				   (unsigned char) val);
	}

	return count;
}

static DEVICE_ATTR(registers, S_IRUGO | S_IWUSR, sc8960x_show_registers,
		   sc8960x_store_registers);

static struct attribute *sc8960x_attributes[] = {
	&dev_attr_registers.attr,
	NULL,
};

static const struct attribute_group sc8960x_attr_group = {
	.attrs = sc8960x_attributes,
};

static int sc8960x_charging(struct charger_device *chg_dev, bool enable)
{

	struct sc8960x *sc = dev_get_drvdata(&chg_dev->dev);
	int ret = 0;
	u8 val;

	if (enable)
		ret = sc8960x_enable_charger(sc);
	else
		ret = sc8960x_disable_charger(sc);

	pr_err("%s charger %s\n", enable ? "enable" : "disable",
	       !ret ? "successfully" : "failed");

	ret = sc8960x_read_byte(sc, SC8960X_REG_01, &val);

	if (!ret)
		sc->charge_enabled = !!(val & REG01_CHG_CONFIG_MASK);

	return ret;
}

static int sc8960x_plug_in(struct charger_device *chg_dev)
{

	int ret;

	ret = sc8960x_charging(chg_dev, true);

	if (ret)
		pr_err("Failed to enable charging:%d\n", ret);

	return ret;
}

static int sc8960x_plug_out(struct charger_device *chg_dev)
{
	int ret;

	ret = sc8960x_charging(chg_dev, false);

	if (ret)
		pr_err("Failed to disable charging:%d\n", ret);

	return ret;
}

static int sc8960x_dump_register(struct charger_device *chg_dev)
{
	struct sc8960x *sc = dev_get_drvdata(&chg_dev->dev);

	sc8960x_dump_regs(sc);

	return 0;
}

static int sc8960x_is_charging_enable(struct charger_device *chg_dev, bool *en)
{
	struct sc8960x *sc = dev_get_drvdata(&chg_dev->dev);

	*en = sc->charge_enabled;

	return 0;
}

static int sc8960x_is_charging_done(struct charger_device *chg_dev, bool *done)
{
	struct sc8960x *sc = dev_get_drvdata(&chg_dev->dev);
	int ret;
	u8 val;

	ret = sc8960x_read_byte(sc, SC8960X_REG_08, &val);
	if (!ret) {
		val = val & REG08_CHRG_STAT_MASK;
		val = val >> REG08_CHRG_STAT_SHIFT;
		*done = (val == REG08_CHRG_STAT_CHGDONE);
	}

	return ret;
}

static int sc8960x_set_ichg(struct charger_device *chg_dev, u32 curr)
{
	struct sc8960x *sc = dev_get_drvdata(&chg_dev->dev);

	pr_err("charge curr = %d\n", curr);

	return sc8960x_set_chargecurrent(sc, curr / 1000);
}

static int sc8960x_get_ichg(struct charger_device *chg_dev, u32 *curr)
{
	struct sc8960x *sc = dev_get_drvdata(&chg_dev->dev);
	u8 reg_val;
	int ichg;
	int ret;

	ret = sc8960x_read_byte(sc, SC8960X_REG_02, &reg_val);
	if (!ret) {
		ichg = (reg_val & REG02_ICHG_MASK) >> REG02_ICHG_SHIFT;
		ichg = ichg * REG02_ICHG_LSB + REG02_ICHG_BASE;
		*curr = ichg * 1000;
	}

	return ret;
}

static int sc8960x_get_min_ichg(struct charger_device *chg_dev, u32 *curr)
{
	*curr = 60 * 1000;

	return 0;
}

static int sc8960x_set_vchg(struct charger_device *chg_dev, u32 volt)
{
	struct sc8960x *sc = dev_get_drvdata(&chg_dev->dev);

	pr_err("charge volt = %d\n", volt);

	return sc8960x_set_chargevolt(sc, volt / 1000);
}

static int sc8960x_get_vchg(struct charger_device *chg_dev, u32 *volt)
{
	struct sc8960x *sc = dev_get_drvdata(&chg_dev->dev);
	u8 reg_val;
	int vchg;
	int ret;

	ret = sc8960x_read_byte(sc, SC8960X_REG_04, &reg_val);
	if (!ret) {
		vchg = (reg_val & REG04_VREG_MASK) >> REG04_VREG_SHIFT;
		vchg = vchg * REG04_VREG_LSB + REG04_VREG_BASE;
		*volt = vchg * 1000;
	}

	return ret;
}

static int sc8960x_set_ivl(struct charger_device *chg_dev, u32 volt)
{
	struct sc8960x *sc = dev_get_drvdata(&chg_dev->dev);

	pr_err("vindpm volt = %d\n", volt);

	return sc8960x_set_input_volt_limit(sc, volt / 1000);

}

static int sc8960x_set_icl(struct charger_device *chg_dev, u32 curr)
{
	struct sc8960x *sc = dev_get_drvdata(&chg_dev->dev);

	pr_err("indpm curr = %d\n", curr);

	return sc8960x_set_input_current_limit(sc, curr / 1000);
}

static int sc8960x_get_icl(struct charger_device *chg_dev, u32 *curr)
{
	struct sc8960x *sc = dev_get_drvdata(&chg_dev->dev);
	u8 reg_val;
	int icl;
	int ret;

	ret = sc8960x_read_byte(sc, SC8960X_REG_00, &reg_val);
	if (!ret) {
		icl = (reg_val & REG00_IINLIM_MASK) >> REG00_IINLIM_SHIFT;
		icl = icl * REG00_IINLIM_LSB + REG00_IINLIM_BASE;
		*curr = icl * 1000;
	}

	return ret;

}

static int sc8960x_kick_wdt(struct charger_device *chg_dev)
{
	struct sc8960x *sc = dev_get_drvdata(&chg_dev->dev);

	return sc8960x_reset_watchdog_timer(sc);
}

static int sc8960x_set_otg(struct charger_device *chg_dev, bool en)
{
	int ret;
	struct sc8960x *sc = dev_get_drvdata(&chg_dev->dev);

	if (en) {
        ret = sc8960x_disable_charger(sc);
/* C3T code for HQHW-2797 by tongjiacheng at 2022/09/13 start*/
		Charger_Detect_Release();
/* C3T code for HQHW-2797 by tongjiacheng at 2022/09/13 end*/
		ret = sc8960x_enable_otg(sc);
    }
	else {
        ret = sc8960x_disable_otg(sc);
/* C3T code for HQ-237347 by tongjiacheng at 2022/08/31 start*/
        ret = sc8960x_enable_charger(sc);
/* C3T code for HQ-237347 by tongjiacheng at 2022/08/31 end*/
    }

	pr_err("%s OTG %s\n", en ? "enable" : "disable",
	       !ret ? "successfully" : "failed");

	return ret;
}

static int sc8960x_set_safety_timer(struct charger_device *chg_dev, bool en)
{
	struct sc8960x *sc = dev_get_drvdata(&chg_dev->dev);
	int ret;

	if (en)
		ret = sc8960x_enable_safety_timer(sc);
	else
		ret = sc8960x_disable_safety_timer(sc);

	return ret;
}

static int sc8960x_is_safety_timer_enabled(struct charger_device *chg_dev,
					   bool *en)
{
	struct sc8960x *sc = dev_get_drvdata(&chg_dev->dev);
	int ret;
	u8 reg_val;

	ret = sc8960x_read_byte(sc, SC8960X_REG_05, &reg_val);

	if (!ret)
		*en = !!(reg_val & REG05_EN_TIMER_MASK);

	return ret;
}

static int sc8960x_set_boost_ilmt(struct charger_device *chg_dev, u32 curr)
{
	struct sc8960x *sc = dev_get_drvdata(&chg_dev->dev);
	int ret;

	pr_err("otg curr = %d\n", curr);

	ret = sc8960x_set_boost_current(sc, curr / 1000);

	return ret;
}

/* C3T code for HQ-HQ-218837 by tongjiacheng at 2022/08/26 start */
static int sc8960x_do_event(struct charger_device *chg_dev, u32 event, u32 args)
{
	struct sc8960x *sc = dev_get_drvdata(&chg_dev->dev);

	if (!sc->psy) {
		dev_notice(sc->dev, "%s: cannot get psy \n", __func__);
		return -ENODEV;
	}

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
/* C3T code for HQ-HQ-218837 by tongjiacheng at 2022/08/26 end*/

/* C3T code for HQ-237347 by tongjiacheng at 2022/08/31 start*/
static int sc8960x_get_hiz_mode(struct charger_device *chg_dev)
{
	u8 val;
	int ret;

	struct sc8960x *sc = dev_get_drvdata(&chg_dev->dev);

	ret = sc8960x_read_byte(sc, SC8960X_REG_00, &val);
	if (ret)
		return ret;

	val = (val & REG00_ENHIZ_MASK) >> REG00_ENHIZ_SHIFT;

	pr_err("%s:hiz mode %s\n",__func__, ret ? "enabled" : "disabled");

	return val;
}

extern struct charger_manager *_pinfo;
static int sc8960x_set_hiz_mode(struct charger_device *chg_dev, bool en)
{
	int ret;

	struct sc8960x *sc = dev_get_drvdata(&chg_dev->dev);

	if (en) {
		ret = sc8960x_enter_hiz_mode(sc);
		sc->hiz_mode = true;
		charger_manager_notifier(_pinfo, CHARGER_NOTIFY_STOP_CHARGING);
	}
	else {
		ret = sc8960x_exit_hiz_mode(sc);
		sc->hiz_mode = false;
	}

	pr_err("%s hiz mode %s\n", en ? "enable" : "disable",
			!ret ? "successfully" : "failed");

	return ret;
}
/* C3T code for HQ-237347 by tongjiacheng at 2022/08/31 end*/


static struct charger_ops sc8960x_chg_ops = {
	/* Normal charging */
	.plug_in = sc8960x_plug_in,
	.plug_out = sc8960x_plug_out,
	.dump_registers = sc8960x_dump_register,
	.enable = sc8960x_charging,
	.is_enabled = sc8960x_is_charging_enable,
	.get_charging_current = sc8960x_get_ichg,
	.set_charging_current = sc8960x_set_ichg,
	.get_input_current = sc8960x_get_icl,
	.set_input_current = sc8960x_set_icl,
	.get_constant_voltage = sc8960x_get_vchg,
	.set_constant_voltage = sc8960x_set_vchg,
	.kick_wdt = sc8960x_kick_wdt,
	.set_mivr = sc8960x_set_ivl,
	.is_charging_done = sc8960x_is_charging_done,
	.get_min_charging_current = sc8960x_get_min_ichg,

	/* Safety timer */
	.enable_safety_timer = sc8960x_set_safety_timer,
	.is_safety_timer_enabled = sc8960x_is_safety_timer_enabled,

	/* Power path */
	.enable_powerpath = NULL,
	.is_powerpath_enabled = NULL,

	/* OTG */
	.enable_otg = sc8960x_set_otg,
	.set_boost_current_limit = sc8960x_set_boost_ilmt,
	.enable_discharge = NULL,

	/* PE+/PE+20 */
	.send_ta_current_pattern = NULL,
	.set_pe20_efficiency_table = NULL,
	.send_ta20_current_pattern = NULL,
	.enable_cable_drop_comp = NULL,

	/* ADC */
	.get_tchg_adc = NULL,
/* C3T code for HQ-HQ-218837 by tongjiacheng at 2022/08/26 start */
	.event = sc8960x_do_event,
/* C3T code for HQ-HQ-218837 by tongjiacheng at 2022/08/26 end*/

/* C3T code for HQ-237347 by tongjiacheng at 2022/08/31 start*/
	.set_hiz_mode = sc8960x_set_hiz_mode,
	.get_hiz_mode = sc8960x_get_hiz_mode,
/* C3T code for HQ-237347 by tongjiacheng at 2022/08/31 end*/
};

static struct of_device_id sc8960x_charger_match_table[] = {
	{
	 .compatible = "sc,sc89601d",
	 .data = &pn_data[PN_SC89601D],
	 },
	{},
};
MODULE_DEVICE_TABLE(of, sc8960x_charger_match_table);


static int sc8960x_charger_probe(struct i2c_client *client,
				 const struct i2c_device_id *id)
{
	struct sc8960x *sc;
	const struct of_device_id *match;
	struct device_node *node = client->dev.of_node;

	int ret = 0;

	sc = devm_kzalloc(&client->dev, sizeof(struct sc8960x), GFP_KERNEL);
	if (!sc)
		return -ENOMEM;

	client->addr = 0x6B;	
	sc->dev = &client->dev;
	sc->client = client;

	i2c_set_clientdata(client, sc);

	mutex_init(&sc->i2c_rw_lock);

	ret = sc8960x_detect_device(sc);
	if (ret) {
		pr_err("No sc8960x device found!\n");
		return -ENODEV;
	}

	match = of_match_node(sc8960x_charger_match_table, node);
	if (match == NULL) {
		pr_err("device tree match not found\n");
		return -EINVAL;
	}

/*C3T code for HQ-223293 by gengyifei at 2022/9/1 start*/
/* C3T code for HQ-249726 by tongjiacheng at 2022/09/30 start */
	if (sc->part_no != SC89601D){
/* C3T code for HQ-249726 by tongjiacheng at 2022/09/30 end*/
		pr_info("part no mismatch, hw:%s, devicetree:%s\n",
			pn_str[sc->part_no], pn_str[*(int *) match->data]);
		return -1;
	}
/*C3T code for HQ-223293 by gengyifei at 2022/9/1 end*/
	sc->platform_data = sc8960x_parse_dt(node, sc);

	if (!sc->platform_data) {
		pr_err("No platform data provided.\n");
		return -EINVAL;
	}

	ret = sc8960x_init_device(sc);
	if (ret) {
		pr_err("Failed to init device\n");
		return ret;
	}

	INIT_DELAYED_WORK(&sc->psy_dwork, sc8960x_inform_psy_dwork_handler);
	/* C3T code for HQHW-2797 by tongjiacheng at 2022/09/13 start*/
	INIT_DELAYED_WORK(&sc->force_detect_dwork, sc8960x_force_detection_dwork_handler);
	/* C3T code for HQHW-2797 by tongjiacheng at 2022/09/13 end*/

	sc8960x_register_interrupt(sc);

	sc->chg_dev = charger_device_register(sc->chg_dev_name,
					      &client->dev, sc,
					      &sc8960x_chg_ops,
					      &sc8960x_chg_props);
	if (IS_ERR_OR_NULL(sc->chg_dev)) {
		ret = PTR_ERR(sc->chg_dev);
		return ret;
	}

	ret = sysfs_create_group(&sc->dev->kobj, &sc8960x_attr_group);
	if (ret)
		dev_err(sc->dev, "failed to register sysfs. err: %d\n", ret);

/* C3T code for HQ-249726 by tongjiacheng at 2022/09/30 start */
	Charger_Detect_Init();
	hq_musb_pullup(mtk_musb, 0);
	sc8960x_force_dpdm(sc);
/* C3T code for HQ-249726 by tongjiacheng at 2022/09/30 end*/

	determine_initial_status(sc);

	pr_err("sc8960x probe successfully, Part Num:%d, Revision:%d\n!",
	       sc->part_no, sc->revision);

	return 0;
}

static int sc8960x_charger_remove(struct i2c_client *client)
{
	struct sc8960x *sc = i2c_get_clientdata(client);

	mutex_destroy(&sc->i2c_rw_lock);

	sysfs_remove_group(&sc->dev->kobj, &sc8960x_attr_group);

	return 0;
}

static void sc8960x_charger_shutdown(struct i2c_client *client)
{

}

static int sc8960x_suspend(struct device *dev)
{
	struct sc8960x *sc = dev_get_drvdata(dev);

	pr_err("%s\n", __func__);

	if (device_may_wakeup(dev))
			enable_irq_wake(sc->client->irq);

	disable_irq(sc->client->irq);

	return 0;
}

static int sc8960x_resume(struct device *dev)
{
	struct sc8960x *sc = dev_get_drvdata(dev);

	pr_err("%s\n", __func__);
	enable_irq(sc->client->irq);
	if (device_may_wakeup(dev))
		disable_irq_wake(sc->client->irq);

	return 0;
}

static SIMPLE_DEV_PM_OPS(sc8960x_pm_ops, sc8960x_suspend, sc8960x_resume);

static struct i2c_driver sc8960x_charger_driver = {
	.driver = {
		   .name = "sc8960x-charger",
		   .owner = THIS_MODULE,
		   .of_match_table = sc8960x_charger_match_table,
		   .pm = &sc8960x_pm_ops,
		   },

	.probe = sc8960x_charger_probe,
	.remove = sc8960x_charger_remove,
	.shutdown = sc8960x_charger_shutdown,

};

module_i2c_driver(sc8960x_charger_driver);

MODULE_DESCRIPTION("SC SC8960x Charger Driver");
MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("South Chip");
