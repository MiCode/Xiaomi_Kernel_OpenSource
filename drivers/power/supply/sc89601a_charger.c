/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2022 Southchip Semiconductor Technology(Shanghai) Co., Ltd.
 */
#define pr_fmt(fmt)	"[sc89601a]:%s: " fmt, __func__

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
#include "sc89601a_charger.h"

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

enum sc89601a_part_no {
	SC89601A = 0x05,
};

enum {
    BOOSTI_500 = 500,
    BOOSTI_1200 = 1200,
};

struct chg_para{
	int vlim;
	int ilim;

	int vreg;
	int ichg;
};

struct sc89601a_platform_data {
	int iprechg;
	int iterm;
    int statctrl;
    int vac_ovp;

	int boostv;
	int boosti;

	struct chg_para usb;
};

enum sc89601a_charge_state {
	CHARGE_STATE_IDLE = SC89601A_CHRG_STAT_IDLE,
	CHARGE_STATE_PRECHG = SC89601A_CHRG_STAT_PRECHG,
	CHARGE_STATE_FASTCHG = SC89601A_CHRG_STAT_FASTCHG,
	CHARGE_STATE_CHGDONE = SC89601A_CHRG_STAT_CHGDONE,
};


struct sc89601a {
	struct device *dev;
	struct i2c_client *client;

	enum sc89601a_part_no part_no;
	int revision;

	int status;
	
	struct mutex i2c_rw_lock;

	bool charge_enabled;/* Register bit status */

	struct sc89601a_platform_data* platform_data;
	struct charger_device *chg_dev;
	
};

static const struct charger_properties sc89601a_chg_props = {
	.alias_name = "sc89601a",
};

static int __sc89601a_read_reg(struct sc89601a* sc, u8 reg, u8 *data)
{
	s32 ret;

	ret = i2c_smbus_read_byte_data(sc->client, reg);
	if (ret < 0) {
		pr_err("i2c read fail: can't read from reg 0x%02X\n", reg);
		return ret;
	}
	*data = (u8)ret;
	
	return 0;
}

static int __sc89601a_write_reg(struct sc89601a* sc, int reg, u8 val)
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

static int sc89601a_read_byte(struct sc89601a *sc, u8 *data, u8 reg)
{
	int ret;

	mutex_lock(&sc->i2c_rw_lock);
	ret = __sc89601a_read_reg(sc, reg, data);
	mutex_unlock(&sc->i2c_rw_lock);

	return ret;
}


static int sc89601a_write_byte(struct sc89601a *sc, u8 reg, u8 data)
{
	int ret;

	mutex_lock(&sc->i2c_rw_lock);
	ret = __sc89601a_write_reg(sc, reg, data);
	mutex_unlock(&sc->i2c_rw_lock);

	if (ret) {
		pr_err("Failed: reg=%02X, ret=%d\n", reg, ret);
	}

	return ret;
}


static int sc89601a_update_bits(struct sc89601a *sc, u8 reg, 
				u8 mask, u8 data)
{
	int ret;
	u8 tmp;

	mutex_lock(&sc->i2c_rw_lock);
	ret = __sc89601a_read_reg(sc, reg, &tmp);
	if (ret) {
		pr_err("Failed: reg=%02X, ret=%d\n", reg, ret);
		goto out;
	}
	tmp &= ~mask;
	tmp |= data & mask;

	ret = __sc89601a_write_reg(sc, reg, tmp);
	if (ret) {
		pr_err("Failed: reg=%02X, ret=%d\n", reg, ret);
	}
out:
	mutex_unlock(&sc->i2c_rw_lock);
	return ret;
}

static int sc89601a_enable_otg(struct sc89601a *sc)
{
	u8 val = SC89601A_OTG_ENABLE << SC89601A_OTG_CONFIG_SHIFT;
    pr_info("sc89601a_enable_otg enter\n");
	return sc89601a_update_bits(sc, SC89601A_REG_03,
				SC89601A_OTG_CONFIG_MASK, val);

}

static int sc89601a_disable_otg(struct sc89601a *sc)
{
	u8 val = SC89601A_OTG_DISABLE << SC89601A_OTG_CONFIG_SHIFT;
	pr_info("sc89601a_disable_otg enter\n");
	return sc89601a_update_bits(sc, SC89601A_REG_03,
				   SC89601A_OTG_CONFIG_MASK, val);

}

static int sc89601a_enable_charger(struct sc89601a *sc)
{
	int ret;
	u8 val = SC89601A_CHG_ENABLE << SC89601A_CHG_CONFIG_SHIFT;

	ret = sc89601a_update_bits(sc, SC89601A_REG_03, SC89601A_CHG_CONFIG_MASK, val);

	return ret;
}

static int sc89601a_disable_charger(struct sc89601a *sc)
{
	int ret;
	u8 val = SC89601A_CHG_DISABLE << SC89601A_CHG_CONFIG_SHIFT;

	ret = sc89601a_update_bits(sc, SC89601A_REG_03, SC89601A_CHG_CONFIG_MASK, val);
	return ret;
}

int sc89601a_set_chargecurrent(struct sc89601a *sc, int curr)
{
	u8 ichg;

	if (curr < SC89601A_ICHG_BASE)
		curr = SC89601A_ICHG_BASE;

	ichg = (curr - SC89601A_ICHG_BASE)/SC89601A_ICHG_LSB;
	return sc89601a_update_bits(sc, SC89601A_REG_04, SC89601A_ICHG_MASK, 
				ichg << SC89601A_ICHG_SHIFT);

}

int sc89601a_set_term_current(struct sc89601a *sc, int curr)
{
	u8 iterm;

	if (curr < SC89601A_ITERM_BASE)
		curr = SC89601A_ITERM_BASE;

	iterm = (curr - SC89601A_ITERM_BASE) / SC89601A_ITERM_LSB;

	return sc89601a_update_bits(sc, SC89601A_REG_05, SC89601A_ITERM_MASK, 
				iterm << SC89601A_ITERM_SHIFT);
}
EXPORT_SYMBOL_GPL(sc89601a_set_term_current);


int sc89601a_set_prechg_current(struct sc89601a *sc, int curr)
{
	u8 iprechg;

	if (curr < SC89601A_IPRECHG_BASE)
		curr = SC89601A_IPRECHG_BASE;

	iprechg = (curr - SC89601A_IPRECHG_BASE) / SC89601A_IPRECHG_LSB;

	return sc89601a_update_bits(sc, SC89601A_REG_05, SC89601A_IPRECHG_MASK, 
				iprechg << SC89601A_IPRECHG_SHIFT);
}
EXPORT_SYMBOL_GPL(sc89601a_set_prechg_current);

int sc89601a_set_chargevolt(struct sc89601a *sc, int volt)
{
	u8 val;
	
	if (volt < SC89601A_VREG_BASE)
		volt = SC89601A_VREG_BASE;

	val = (volt - SC89601A_VREG_BASE)/SC89601A_VREG_LSB;
	return sc89601a_update_bits(sc, SC89601A_REG_06, SC89601A_VREG_MASK, 
				val << SC89601A_VREG_SHIFT);
}


int sc89601a_set_input_volt_limit(struct sc89601a *sc, int volt)
{
	u8 val;

	if (volt < SC89601A_VINDPM_BASE)
		volt = SC89601A_VINDPM_BASE;

	sc89601a_update_bits(sc, SC89601A_REG_0D, SC89601A_FORCE_VINDPM_MASK,
				SC89601A_FORCE_VINDPM_ENABLE << SC89601A_FORCE_VINDPM_SHIFT);

	val = (volt - SC89601A_VINDPM_BASE) / SC89601A_VINDPM_LSB;
	return sc89601a_update_bits(sc, SC89601A_REG_0D, SC89601A_VINDPM_MASK, 
				val << SC89601A_VINDPM_SHIFT);
}

int sc89601a_set_input_current_limit(struct sc89601a *sc, int curr)
{
	u8 val;

	if (curr < SC89601A_IINLIM_BASE)
		curr = SC89601A_IINLIM_BASE;

	val = (curr - SC89601A_IINLIM_BASE) / SC89601A_IINLIM_LSB;
	return sc89601a_update_bits(sc, SC89601A_REG_00, SC89601A_IINLIM_MASK, 
				val << SC89601A_IINLIM_SHIFT);
}
//begin gerrit 203957
int sc89601a_set_iilimit_enable(struct sc89601a *sc, bool en)

{
	u8 val;

	if (en)
	    val = SC89601A_ILIM_ENABLE;
	else
	    val = SC89601A_ILIM_DISABLE;

	    return sc89601a_update_bits(sc, SC89601A_REG_00, SC89601A_ENILIM_MASK, 
			val << SC89601A_ENILIM_SHIFT);

}//end gerrit 203957

int sc89601a_ico_enable(struct sc89601a *sc, bool en)
{
	u8 val;

	if (en)
		val = SC89601A_ICO_ENABLE << SC89601A_ICO_EN_SHIFT;
	else
		val = SC89601A_ICO_DISABLE << SC89601A_ICO_EN_SHIFT;

	return sc89601a_update_bits(sc, SC89601A_REG_02, SC89601A_ICO_EN_MASK,
				val);
}

int sc89601a_set_watchdog_timer(struct sc89601a *sc, u8 timeout)
{
	u8 temp;

	temp = (u8)(((timeout - SC89601A_WDT_BASE) / SC89601A_WDT_LSB) << SC89601A_WDT_SHIFT);

	return sc89601a_update_bits(sc, SC89601A_REG_07, SC89601A_WDT_MASK, temp); 
}
EXPORT_SYMBOL_GPL(sc89601a_set_watchdog_timer);

int sc89601a_disable_watchdog_timer(struct sc89601a *sc)
{
	u8 val = SC89601A_WDT_DISABLE << SC89601A_WDT_SHIFT;

	return sc89601a_update_bits(sc, SC89601A_REG_07, SC89601A_WDT_MASK, val);
}
EXPORT_SYMBOL_GPL(sc89601a_disable_watchdog_timer);

int sc89601a_reset_watchdog_timer(struct sc89601a *sc)
{
	u8 val = SC89601A_WDT_RESET << SC89601A_WDT_RESET_SHIFT;

	return sc89601a_update_bits(sc, SC89601A_REG_03, SC89601A_WDT_RESET_MASK, val);
}
EXPORT_SYMBOL_GPL(sc89601a_reset_watchdog_timer);

int sc89601a_reset_chip(struct sc89601a *sc)
{
	int ret;
	u8 val = SC89601A_RESET << SC89601A_RESET_SHIFT;

	ret = sc89601a_update_bits(sc, SC89601A_REG_14, SC89601A_RESET_MASK, val);
	return ret;
}
EXPORT_SYMBOL_GPL(sc89601a_reset_chip);

int sc89601a_enter_hiz_mode(struct sc89601a *sc)
{
	u8 val = SC89601A_HIZ_ENABLE << SC89601A_ENHIZ_SHIFT;

	return sc89601a_update_bits(sc, SC89601A_REG_00, SC89601A_ENHIZ_MASK, val);
}
EXPORT_SYMBOL_GPL(sc89601a_enter_hiz_mode);

int sc89601a_exit_hiz_mode(struct sc89601a *sc)
{
	u8 val = SC89601A_HIZ_DISABLE << SC89601A_ENHIZ_SHIFT;

	return sc89601a_update_bits(sc, SC89601A_REG_00, SC89601A_ENHIZ_MASK, val);
}
EXPORT_SYMBOL_GPL(sc89601a_exit_hiz_mode);

int sc89601a_get_hiz_mode(struct sc89601a *sc, u8 *state)
{
	u8 val;
	int ret;

	ret = sc89601a_read_byte(sc, &val, SC89601A_REG_00);
	if (ret)
		return ret;
	*state = (val & SC89601A_ENHIZ_MASK) >> SC89601A_ENHIZ_SHIFT;

	return 0;
}
EXPORT_SYMBOL_GPL(sc89601a_get_hiz_mode);


static int sc89601a_enable_term(struct sc89601a* sc, bool enable)
{
	u8 val;
	int ret;

	if (enable)
		val = SC89601A_TERM_ENABLE << SC89601A_EN_TERM_SHIFT;
	else
		val = SC89601A_TERM_DISABLE << SC89601A_EN_TERM_SHIFT;

	ret = sc89601a_update_bits(sc, SC89601A_REG_07, SC89601A_EN_TERM_MASK, val);

	return ret;
}
EXPORT_SYMBOL_GPL(sc89601a_enable_term);

int sc89601a_set_boost_current(struct sc89601a *sc, int curr)
{
	u8 val;

	val = SC89601A_BOOST_LIM_1200MA;
	if (curr == BOOSTI_500)
		val = SC89601A_BOOST_LIM_500MA;

	return sc89601a_update_bits(sc, SC89601A_REG_0A, SC89601A_BOOST_LIM_MASK, 
				val << SC89601A_BOOST_LIM_SHIFT);
}

int sc89601a_set_boost_voltage(struct sc89601a *sc, int volt)
{
	u8 val;

  	if (volt < SC89601A_BOOSTV_BASE) {
		volt = SC89601A_BOOSTV_BASE;
	}

	val = (volt - SC89601A_BOOSTV_BASE) / SC89601A_BOOSTV_LSB;
	
	return sc89601a_update_bits(sc, SC89601A_REG_0A, SC89601A_BOOSTV_MASK, 
				val << SC89601A_BOOSTV_SHIFT);
}
EXPORT_SYMBOL_GPL(sc89601a_set_boost_voltage);

static int sc89601a_set_acovp_threshold(struct sc89601a *sc, int volt)
{
	//defualt value is 14v
	return 0;
}
EXPORT_SYMBOL_GPL(sc89601a_set_acovp_threshold);

static int sc89601a_set_stat_ctrl(struct sc89601a *sc, int ctrl)
{
	return 0;
}


static int sc89601a_enable_batfet(struct sc89601a *sc)
{
	const u8 val = SC89601A_BATFET_ON << SC89601A_BATFET_DIS_SHIFT;

	return sc89601a_update_bits(sc, SC89601A_REG_09, SC89601A_BATFET_DIS_MASK,
				val);
}
EXPORT_SYMBOL_GPL(sc89601a_enable_batfet);


static int sc89601a_disable_batfet(struct sc89601a *sc)
{
	const u8 val = SC89601A_BATFET_OFF << SC89601A_BATFET_DIS_SHIFT;

	return sc89601a_update_bits(sc, SC89601A_REG_09, SC89601A_BATFET_DIS_MASK,
				val);
}
EXPORT_SYMBOL_GPL(sc89601a_disable_batfet);

static int sc89601a_set_batfet_delay(struct sc89601a *sc, uint8_t delay)
{
	u8 val;

	if (delay == 0)
		val = SC89601A_BATFET_DLY_0S;
	else
		val = SC89601A_BATFET_DLY_10S;
	
	val <<= SC89601A_BATFET_DLY_SHIFT;

	return sc89601a_update_bits(sc, SC89601A_REG_09, SC89601A_BATFET_DLY_MASK,
								val);
}
EXPORT_SYMBOL_GPL(sc89601a_set_batfet_delay);

static int sc89601a_enable_safety_timer(struct sc89601a *sc)
{
	const u8 val = SC89601A_CHG_TIMER_ENABLE << SC89601A_EN_TIMER_SHIFT;
	
	return sc89601a_update_bits(sc, SC89601A_REG_07, SC89601A_EN_TIMER_MASK,
				val);
}
EXPORT_SYMBOL_GPL(sc89601a_enable_safety_timer);


static int sc89601a_disable_safety_timer(struct sc89601a *sc)
{
	const u8 val = SC89601A_CHG_TIMER_DISABLE << SC89601A_EN_TIMER_SHIFT;
	
	return sc89601a_update_bits(sc, SC89601A_REG_07, SC89601A_EN_TIMER_MASK,
				val);
}
EXPORT_SYMBOL_GPL(sc89601a_disable_safety_timer);

static struct sc89601a_platform_data* sc89601a_parse_dt(struct device *dev, 
							struct sc89601a * sc)
{
    int ret;
    struct device_node *np = dev->of_node;
	struct sc89601a_platform_data* pdata;
	
	pdata = devm_kzalloc(dev, sizeof(struct sc89601a_platform_data), 
						GFP_KERNEL);
	if (!pdata) {
		pr_err("Out of memory\n");
		return NULL;
	}
#if 0	
	ret = of_property_read_u32(np, "sc,sc89601a,chip-enable-gpio", &sc->gpio_ce);
    if(ret) {
		pr_err("Failed to read node of sc,sc89601a,chip-enable-gpio\n");
	}
#endif

    ret = of_property_read_u32(np,"sc,sc89601a,usb-vlim",&pdata->usb.vlim);
    if(ret) {
		pr_err("Failed to read node of sc,sc89601a,usb-vlim\n");
	}

    ret = of_property_read_u32(np,"sc,sc89601a,usb-ilim",&pdata->usb.ilim);
    if(ret) {
		pr_err("Failed to read node of sc,sc89601a,usb-ilim\n");
	}
	
    ret = of_property_read_u32(np,"sc,sc89601a,usb-vreg",&pdata->usb.vreg);
    if(ret) {
		pr_err("Failed to read node of sc,sc89601a,usb-vreg\n");
	}

    ret = of_property_read_u32(np,"sc,sc89601a,usb-ichg",&pdata->usb.ichg);
    if(ret) {
		pr_err("Failed to read node of sc,sc89601a,usb-ichg\n");
	}

    ret = of_property_read_u32(np,"sc,sc89601a,stat-pin-ctrl",&pdata->statctrl);
    if(ret) {
		pr_err("Failed to read node of sc,sc89601a,stat-pin-ctrl\n");
	}
	
    ret = of_property_read_u32(np,"sc,sc89601a,precharge-current",&pdata->iprechg);
    if(ret) {
		pr_err("Failed to read node of sc,sc89601a,precharge-current\n");
	}

    ret = of_property_read_u32(np,"sc,sc89601a,termination-current",&pdata->iterm);
    if(ret) {
		pr_err("Failed to read node of sc,sc89601a,termination-current\n");
	}
	
    ret = of_property_read_u32(np,"sc,sc89601a,boost-voltage",&pdata->boostv);
    if(ret) {
		pr_err("Failed to read node of sc,sc89601a,boost-voltage\n");
	}

    ret = of_property_read_u32(np,"sc,sc89601a,boost-current",&pdata->boosti);
    if(ret) {
		pr_err("Failed to read node of sc,sc89601a,boost-current\n");
	}

    ret = of_property_read_u32(np,"sc,sc89601a,vac-ovp-threshold",&pdata->vac_ovp);
    if(ret) {
		pr_err("Failed to read node of sc,sc89601a,vac-ovp-threshold\n");
	}

    return pdata;   
}

static int sc89601a_init_device(struct sc89601a *sc)
{
	int ret;
	
	sc89601a_disable_watchdog_timer(sc);

	sc89601a_disable_safety_timer(sc);

	sc89601a_ico_enable(sc, false);
	
	sc89601a_set_iilimit_enable(sc, false);//add gerrit 203957
	ret = sc89601a_set_stat_ctrl(sc, sc->platform_data->statctrl);
	if (ret)
		pr_err("Failed to set stat pin control mode, ret = %d\n",ret);

	ret = sc89601a_set_prechg_current(sc, sc->platform_data->iprechg);
	if (ret)
		pr_err("Failed to set prechg current, ret = %d\n",ret);
	
	ret = sc89601a_set_term_current(sc, sc->platform_data->iterm);
	if (ret)
		pr_err("Failed to set termination current, ret = %d\n",ret);
	
	ret = sc89601a_set_boost_voltage(sc, sc->platform_data->boostv);
	if (ret)
		pr_err("Failed to set boost voltage, ret = %d\n",ret);
	
	ret = sc89601a_set_boost_current(sc, sc->platform_data->boosti);
	if (ret)
		pr_err("Failed to set boost current, ret = %d\n",ret);
	
	ret = sc89601a_set_acovp_threshold(sc, sc->platform_data->vac_ovp);
	if (ret)
		pr_err("Failed to set acovp threshold, ret = %d\n",ret);	

	return 0;
}

/* 2022.5.18 longcheer zhangfeng5 edit. begin */
/* kenrel driver porting: remove eta6963 charger */
#define  PART_NO_SC89601A   5
static int sc89601a_detect_device(struct sc89601a* sc)
{
    int ret;
    u8 data;

    ret = sc89601a_read_byte(sc, &data, SC89601A_REG_14);
    if(ret == 0){
        sc->part_no = (data & SC89601A_PN_MASK) >> SC89601A_PN_SHIFT;
        sc->revision = (data & SC89601A_DEV_REV_MASK) >> SC89601A_DEV_REV_SHIFT;
    }
    pr_info("[%s] part_no=%d revision=%d\n", __func__, sc->part_no, sc->revision);
    if(sc->part_no != PART_NO_SC89601A)
		return 1;
    return ret;
}
/* 2022.5.18 longcheer zhangfeng5 edit. begin */

//begin gerrit:202736
static int sc89601a_read_byte_m155(struct sc89601a *sc, u8 reg, u8 *data)
{
	return sc89601a_read_byte(sc, data, reg);
}

static int sc89601a_detect_device_m155(struct sc89601a* sc)
{
    int ret;
    u8 data;
	
	u8 data_03,data_0B;
	u8 partno;

	pr_info("[chg_detect][0/5]:%s runing.\n", __func__);

	ret = sc89601a_read_byte_m155(sc, 0x03, &data_03);
	pr_info("[chg_detect][1/5]:read 0x03 reg, ret [%d], data [0x%x].\n", ret, data_03);
	if (ret < 0)
		return ret;

	ret = sc89601a_write_byte(sc, 0x03, (data_03 & 0xEF));
	pr_info("[chg_detect][2/5]:write 0x03 reg bit4 = 0, ret [%d], data [0x%x].\n", ret, (data_03 & 0xEF));
	if (ret < 0)
		return ret;
	
	ret = sc89601a_read_byte_m155(sc, 0x0B, &data_0B);
	pr_info("[chg_detect][3/5]:read 0x0B reg, ret [%d], data [0x%x].\n", ret, data_0B);
	if (ret < 0)
		return ret;

	ret = sc89601a_write_byte(sc, 0x03, data_03);
	pr_info("[chg_detect][4/5]:restore 0x03 reg, ret [%d], data [0x%x].\n", ret, data_03);
	if (ret < 0)
		return ret;

	partno = (data_0B & 0x78) >> 3;
	if (partno == 0x07)
	{
		pr_info("[chg_detect][5/5]:reg[0x0B].bit[6:3][%d] == 7, chg_ic is eta6963.\n", partno);
		return -1;
	}
	else
	{
		pr_info("[chg_detect][5/5]:reg[0x0B].bit[6:3][%d] != 7, chg_ic is sc89601a.\n", partno);
	}
	
    ret = sc89601a_read_byte(sc, &data, SC89601A_REG_14);
    if(ret == 0){
        sc->part_no = (data & SC89601A_PN_MASK) >> SC89601A_PN_SHIFT;
        sc->revision = (data & SC89601A_DEV_REV_MASK) >> SC89601A_DEV_REV_SHIFT;
    }

    pr_info("[%s] part_no=%d revision=%d\n", __func__, sc->part_no, sc->revision);

    return ret;
}
//end gerrit:202736
static void sc89601a_dump_regs(struct sc89601a *sc)
{
	int addr;
	u8 val;
	int ret;

	for (addr = 0x0; addr <= 0x14; addr++) {
		ret = sc89601a_read_byte(sc, &val, addr);
		if (ret == 0)
			pr_err("Reg[%.2x] = 0x%.2x\n", addr, val);
	}


}

static ssize_t sc89601a_show_registers(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct sc89601a *sc = dev_get_drvdata(dev);
	u8 addr;
	u8 val;
	u8 tmpbuf[200];
	int len;
	int idx = 0;
	int ret ;

	idx = snprintf(buf, PAGE_SIZE, "%s:\n", "sc89601a Reg");
	for (addr = 0x0; addr <= 0x14; addr++) {
		ret = sc89601a_read_byte(sc, &val, addr);
		if (ret == 0) {
			len = snprintf(tmpbuf, PAGE_SIZE - idx,"Reg[%.2x] = 0x%.2x\n", addr, val);
			memcpy(&buf[idx], tmpbuf, len);
			idx += len;
		}
	}

	return idx;
}

static ssize_t sc89601a_store_registers(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct sc89601a *sc = dev_get_drvdata(dev);
	int ret;
	unsigned int reg;
	unsigned int val;

	ret = sscanf(buf, "%x %x", &reg, &val);
	if (ret == 2 && reg <= 0x14) {
		sc89601a_write_byte(sc, (unsigned char)reg, (unsigned char)val);
	}

	return count;
}

static DEVICE_ATTR(registers, S_IRUGO | S_IWUSR, sc89601a_show_registers, sc89601a_store_registers);

static struct attribute *sc89601a_attributes[] = {
	&dev_attr_registers.attr,
	NULL,
};

static const struct attribute_group sc89601a_attr_group = {
	.attrs = sc89601a_attributes,
};

static int sc89601a_set_hizmode(struct charger_device *chg_dev, bool enable)
{

	struct sc89601a *sc = dev_get_drvdata(&chg_dev->dev);
	int ret = 0;
	u8 tmp;

	if (enable)
  	{
		ret = sc89601a_enter_hiz_mode(sc);
  	}
	else
  	{
		ret = sc89601a_exit_hiz_mode(sc);
   	}

	ret = sc89601a_read_byte(sc, &tmp, SC89601A_REG_00);

	pr_err("set hiz mode i2c  read reg 0x%02X is 0x%02X  ret :%d\n",SC89601A_REG_00,tmp,ret);

	pr_err("%s set hizmode %s\n", enable ? "enable" : "disable",
				  !ret ? "successfully" : "failed");

	return ret;
}

static int sc89601a_charging(struct charger_device *chg_dev, bool enable)
{

	struct sc89601a *sc = dev_get_drvdata(&chg_dev->dev);
	int ret = 0,rc = 0;
	u8 val;

	if (enable)
  	{
		ret = sc89601a_enable_charger(sc);
  	}
	else
  	{
		ret = sc89601a_disable_charger(sc);
   	}
	pr_err("%s charger %s\n", enable ? "enable" : "disable",
				  !ret ? "successfully" : "failed");

	ret = sc89601a_read_byte(sc, &val, SC89601A_REG_03);
	if (!ret && !rc )
		sc->charge_enabled = !!(val & SC89601A_CHG_CONFIG_MASK);

	return ret;
}

static int sc89601a_plug_in(struct charger_device *chg_dev)
{

	int ret;
	
	pr_info("[%s] enter!", __func__);
	ret = sc89601a_charging(chg_dev, true);

	if (!ret)
		pr_err("Failed to enable charging:%d\n", ret);
	
	return ret;
}

static int sc89601a_plug_out(struct charger_device *chg_dev)
{
	int ret;

	pr_info("[%s] enter!", __func__);
	ret = sc89601a_charging(chg_dev, false);

	if (!ret)
		pr_err("Failed to disable charging:%d\n", ret);
	
	return ret;
}

static int sc89601a_dump_register(struct charger_device *chg_dev)
{
	struct sc89601a *sc = dev_get_drvdata(&chg_dev->dev);

	sc89601a_dump_regs(sc);

	return 0;
}

static int sc89601a_is_charging_enable(struct charger_device *chg_dev, bool *en)
{
	struct sc89601a *sc = dev_get_drvdata(&chg_dev->dev);
	
	*en = sc->charge_enabled;
	
	return 0;
}

static int sc89601a_is_charging_done(struct charger_device *chg_dev, bool *done)
{
	struct sc89601a *sc = dev_get_drvdata(&chg_dev->dev);
	int ret;
	u8 val;
	
	ret = sc89601a_read_byte(sc, &val, SC89601A_REG_0B);
	if (!ret) {
		val = val & SC89601A_CHRG_STAT_MASK;
		val = val >> SC89601A_CHRG_STAT_SHIFT;
		*done = (val == SC89601A_CHRG_STAT_CHGDONE);	
	}
	
	return ret;
}

static int sc89601a_set_ichg(struct charger_device *chg_dev, u32 curr)
{
	struct sc89601a *sc = dev_get_drvdata(&chg_dev->dev);
	u8 reg_val;
	int ichg_bef;
	int ret;

	ret = sc89601a_read_byte(sc, &reg_val, SC89601A_REG_04);
	if (!ret) {
		ichg_bef = (reg_val & SC89601A_ICHG_MASK) >> SC89601A_ICHG_SHIFT;
		ichg_bef = ichg_bef * SC89601A_ICHG_LSB + SC89601A_ICHG_BASE;
		if ((ichg_bef <= curr / 1000) &&
			(ichg_bef + SC89601A_ICHG_LSB > curr / 1000)) {
			pr_info("[%s] current has set!\n", __func__, curr);
			return ret;
		}
	}

	pr_info("[%s] curr=%d, ichg_bef = %d\n", __func__, curr, ichg_bef);

	return sc89601a_set_chargecurrent(sc, curr/1000);
}

static int sc89601a_get_ichg(struct charger_device *chg_dev, u32 *curr)
{
	struct sc89601a *sc = dev_get_drvdata(&chg_dev->dev);
	u8 reg_val;
	int ichg;
	int ret;

	ret = sc89601a_read_byte(sc, &reg_val, SC89601A_REG_04);
	if (!ret) {
		ichg = (reg_val & SC89601A_ICHG_MASK) >> SC89601A_ICHG_SHIFT;
		ichg = ichg * SC89601A_ICHG_LSB + SC89601A_ICHG_BASE;
		*curr = ichg * 1000;
	}
	pr_info("[%s] curr=%d\n", __func__, *curr);

	return ret;
}

static int sc89601a_get_min_ichg(struct charger_device *chg_dev, u32 *curr)
{

	*curr = 60 * 1000;

	return 0;
}

static int sc89601a_set_vchg(struct charger_device *chg_dev, u32 volt)
{
	
	struct sc89601a *sc = dev_get_drvdata(&chg_dev->dev);

	return sc89601a_set_chargevolt(sc, volt/1000);	
}

static int sc89601a_get_vchg(struct charger_device *chg_dev, u32 *volt)
{
	struct sc89601a *sc = dev_get_drvdata(&chg_dev->dev);
	u8 reg_val;
	int vchg;
	int ret;

	ret = sc89601a_read_byte(sc, &reg_val, SC89601A_REG_06);
	if (!ret) {
		vchg = (reg_val & SC89601A_VREG_MASK) >> SC89601A_VREG_SHIFT;
		vchg = vchg * SC89601A_VREG_LSB + SC89601A_VREG_BASE;
		*volt = vchg * 1000;
	}

	return ret;
}

static int sc89601a_set_ivl(struct charger_device *chg_dev, u32 volt)
{

	struct sc89601a *sc = dev_get_drvdata(&chg_dev->dev);

	return sc89601a_set_input_volt_limit(sc, volt/1000);

}

static int sc89601a_set_icl(struct charger_device *chg_dev, u32 curr)
{

	struct sc89601a *sc = dev_get_drvdata(&chg_dev->dev);

	return sc89601a_set_input_current_limit(sc, curr/1000);
}

static int sc89601a_get_icl(struct charger_device *chg_dev, u32 *curr)
{
	struct sc89601a *sc = dev_get_drvdata(&chg_dev->dev);
	u8 reg_val;
	int icl;
	int ret;

	ret = sc89601a_read_byte(sc, &reg_val, SC89601A_REG_00);
	if (!ret) {
		icl = (reg_val & SC89601A_IINLIM_MASK) >> SC89601A_IINLIM_SHIFT;
		icl = icl * SC89601A_IINLIM_LSB + SC89601A_IINLIM_BASE;
		*curr = icl * 1000;
	}

	return ret;

}

static int sc89601a_kick_wdt(struct charger_device *chg_dev)
{
	struct sc89601a *sc = dev_get_drvdata(&chg_dev->dev);

	return sc89601a_reset_watchdog_timer(sc);
}

static int sc89601a_set_otg(struct charger_device *chg_dev, bool en)
{
	int ret;
	struct sc89601a *sc = dev_get_drvdata(&chg_dev->dev);
	
	if (en)
  	{ 
		ret = sc89601a_disable_charger(sc);//add gerrit 203957
		ret = sc89601a_enable_otg(sc);
   	}
	else
  	{ 
		ret = sc89601a_disable_otg(sc);
		ret = sc89601a_enable_charger(sc);//add gerrit 203957
   	}
  	return ret;
}

static int sc89601a_set_safety_timer(struct charger_device *chg_dev, bool en)
{
	struct sc89601a *sc = dev_get_drvdata(&chg_dev->dev);
	int ret;
		
	if (en)
		ret = sc89601a_enable_safety_timer(sc);
	else
		ret = sc89601a_disable_safety_timer(sc);
		
	return ret;
}

static int sc89601a_is_safety_timer_enabled(struct charger_device *chg_dev, bool *en)
{
	struct sc89601a *sc = dev_get_drvdata(&chg_dev->dev);
	int ret;
	u8 reg_val;

	ret = sc89601a_read_byte(sc, &reg_val, SC89601A_REG_07);

	if (!ret) 
		*en = !!(reg_val & SC89601A_EN_TIMER_MASK);
	
	return ret;
}


static int sc89601a_set_boost_ilmt(struct charger_device *chg_dev, u32 curr)
{
	struct sc89601a *sc = dev_get_drvdata(&chg_dev->dev);
	int ret;

	ret = sc89601a_set_boost_current(sc, curr/1000);

	return ret;
}

static int sc89601a_do_event(struct charger_device *chg_dev, u32 event,
			    u32 args)
{
	if (chg_dev == NULL)
		return -EINVAL;

	pr_info("%s: event = %d\n", __func__, event);
	switch (event) {
	case EVENT_EOC:
	//begin 233208
	case EVENT_FULL: //end 233208
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

/* 2022.6.3 longcheer zhangfeng5 edit. support set/get_cv_step-8mv begin */
static int sc89601a_set_key(struct sc89601a *sc) {
	sc89601a_write_byte(sc, 0x7D, 0x48);
	sc89601a_write_byte(sc, 0x7D, 0x54);
	sc89601a_write_byte(sc, 0x7D, 0x53);
	sc89601a_write_byte(sc, 0x7D, 0x38);
	return 0;
}

int sc89601a_set_chargevolt_step8(struct sc89601a *sc, int volt)
{
	u8 val;
	u8 val_ft;

	if (volt < SC89601A_VREG_BASE)
		volt = SC89601A_VREG_BASE;

	if (volt % SC89601A_VREG_LSB >= 8) {
		val_ft = 0x40;
	}else{
		val_ft = 0x00;
	}
	sc89601a_set_key(sc);
	sc89601a_write_byte(sc, 0x80, val_ft);
	sc89601a_set_key(sc);
	pr_err("[step_8]: sc set reg80: 0x%x.\n", val_ft);

	val = (volt - SC89601A_VREG_BASE)/SC89601A_VREG_LSB;
	pr_err("[step_8]: sc set reg06.bit[7:2]: 0x%x.\n", val);
	return sc89601a_update_bits(sc, SC89601A_REG_06, SC89601A_VREG_MASK,
				val << SC89601A_VREG_SHIFT);
}

static int sc89601a_set_vchg_step8(struct charger_device *chg_dev, u32 volt)
{

	struct sc89601a *sc = dev_get_drvdata(&chg_dev->dev);
	pr_err("[step_8]: sc set volt: %d.\n", volt);
	return sc89601a_set_chargevolt_step8(sc, volt/1000);
}

#define SC89601A_REG_80 0x80
static int sc89601a_get_vchg_step8(struct charger_device *chg_dev, u32 *volt)
{
	struct sc89601a *sc = dev_get_drvdata(&chg_dev->dev);
	u8 reg_val;
	int vchg;
	int ret;

	ret = sc89601a_read_byte(sc, &reg_val, SC89601A_REG_06);
	if (!ret) {
		vchg = (reg_val & SC89601A_VREG_MASK) >> SC89601A_VREG_SHIFT;
		pr_err("[step_8]: sc get reg06.bit[7:2]: 0x%x.\n", vchg);
		vchg = vchg * SC89601A_VREG_LSB + SC89601A_VREG_BASE;
		sc89601a_set_key(sc);
		sc89601a_read_byte(sc, &reg_val, SC89601A_REG_80);
		if (reg_val & 0x40) {
			vchg = vchg + 8;
		}
		sc89601a_set_key(sc);
		*volt = vchg * 1000;
		pr_err("[step_8]: sc get reg80: 0x%x.\n", reg_val);
		pr_err("[step_8]: sc get volt: %d.\n", *volt);
	}
	return ret;
}
/* 2022.6.3 longcheer zhangfeng5 edit. support set/get_cv_step-8mv end */

static struct charger_ops sc89601a_chg_ops = {
	/* Normal charging */
  	.hiz_mode = sc89601a_set_hizmode,
	.plug_in = sc89601a_plug_in,
	.plug_out = sc89601a_plug_out,
	.dump_registers = sc89601a_dump_register,
	.enable = sc89601a_charging,
	.is_enabled = sc89601a_is_charging_enable,
	.get_charging_current = sc89601a_get_ichg,
	.set_charging_current = sc89601a_set_ichg,
	.get_input_current = sc89601a_get_icl,
	.set_input_current = sc89601a_set_icl,
	.get_constant_voltage = sc89601a_get_vchg_step8,
	.set_constant_voltage = sc89601a_set_vchg_step8,
	.kick_wdt = sc89601a_kick_wdt,
	.set_mivr = sc89601a_set_ivl,
	.is_charging_done = sc89601a_is_charging_done,
	.get_min_charging_current = sc89601a_get_min_ichg,

	/* Safety timer */
	.enable_safety_timer = sc89601a_set_safety_timer,
	.is_safety_timer_enabled = sc89601a_is_safety_timer_enabled,

	/* Power path */
	.enable_powerpath = NULL,
	.is_powerpath_enabled = NULL,

	/* OTG */
	.enable_otg = sc89601a_set_otg,
	.set_boost_current_limit = sc89601a_set_boost_ilmt,
	.enable_discharge = NULL,
	.event = sc89601a_do_event,

	/* PE+/PE+20 */
	.send_ta_current_pattern = NULL,
	.set_pe20_efficiency_table = NULL,
	.send_ta20_current_pattern = NULL,
//	.set_ta20_reset = NULL,
	.enable_cable_drop_comp = NULL,

	/* ADC */
	.get_tchg_adc = NULL,
};



static int sc89601a_charger_probe(struct i2c_client *client, 
					const struct i2c_device_id *id)
{
	struct sc89601a *sc;

	int ret;
	

	sc = devm_kzalloc(&client->dev, sizeof(struct sc89601a), GFP_KERNEL);
	if (!sc) {
		pr_err("Out of memory\n");
		return -ENOMEM;
	}

	sc->dev = &client->dev;
	sc->client = client;

	i2c_set_clientdata(client, sc);
	
	mutex_init(&sc->i2c_rw_lock);

	/* 2022.5.18 longcheer zhangfeng5 edit. kenrel driver porting: remove eta6963 charger */
	ret = sc89601a_detect_device(sc);//modify by gerrit:202736
	if(ret) {
		pr_err("No sc89601a device found!\n");
		return -ENODEV;
	}
	
	if (client->dev.of_node)
		sc->platform_data = sc89601a_parse_dt(&client->dev, sc);
	else
		sc->platform_data = client->dev.platform_data;
	
	if (!sc->platform_data) {
		pr_err("No platform data provided.\n");
		return -EINVAL;
	}

	sc->chg_dev = charger_device_register("primary_chg",
			&client->dev, sc, 
			&sc89601a_chg_ops,
			&sc89601a_chg_props);
	if (IS_ERR_OR_NULL(sc->chg_dev)) {
		ret = PTR_ERR(sc->chg_dev);
		goto err_0;
	}

	ret = sc89601a_init_device(sc);
	if (ret) {
		pr_err("Failed to init device\n");
		return ret;
	}

	
	ret = sysfs_create_group(&sc->dev->kobj, &sc89601a_attr_group);
	if (ret) {
		dev_err(sc->dev, "failed to register sysfs. err: %d\n", ret);
	}


	pr_err("sc89601a probe successfully, Part Num:%d, Revision:%d\n!", 
				sc->part_no, sc->revision);
	
	return 0;
	
err_0:
	
	return ret;
}

static int sc89601a_charger_remove(struct i2c_client *client)
{
	struct sc89601a *sc = i2c_get_clientdata(client);


	mutex_destroy(&sc->i2c_rw_lock);

	sysfs_remove_group(&sc->dev->kobj, &sc89601a_attr_group);


	return 0;
}


static void sc89601a_charger_shutdown(struct i2c_client *client)
{
}

static struct of_device_id sc89601a_charger_match_table[] = {
	{.compatible = "sc,sc89601a_charger",},
	{},
};
MODULE_DEVICE_TABLE(of,sc89601a_charger_match_table);

static const struct i2c_device_id sc89601a_charger_id[] = {
	{ "sc89601a-charger", SC89601A },
	{},
};
MODULE_DEVICE_TABLE(i2c, sc89601a_charger_id);

static struct i2c_driver sc89601a_charger_driver = {
	.driver 	= {
		.name 	= "sc89601a-charger",
		.owner 	= THIS_MODULE,
		.of_match_table = sc89601a_charger_match_table,
	},
	.id_table	= sc89601a_charger_id,
	
	.probe		= sc89601a_charger_probe,
	.remove		= sc89601a_charger_remove,
	.shutdown	= sc89601a_charger_shutdown,
	
};

module_i2c_driver(sc89601a_charger_driver);

MODULE_DESCRIPTION("SC sc89601a Charger Driver");
MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("South Chip");
