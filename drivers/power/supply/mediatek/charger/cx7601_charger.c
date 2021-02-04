/*
 * cx7601 battery charging driver
 *
 * Copyright (C) 2013 Texas Instruments
 * Copyright (C) 2021 XiaoMi, Inc.
 *
 * This package is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 * THIS PACKAGE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

#define pr_fmt(fmt)	"[cx7601]:%s: " fmt, __func__

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
#include "cx7601_reg.h"
#include "cx7601.h"

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

enum cx7601_part_no {
	CX7601 = 0x02,
};

enum cx7601_charge_state {
	CHARGE_STATE_IDLE = REG08_CHRG_STAT_IDLE,
	CHARGE_STATE_PRECHG = REG08_CHRG_STAT_PRECHG,
	CHARGE_STATE_FASTCHG = REG08_CHRG_STAT_FASTCHG,
	CHARGE_STATE_CHGDONE = REG08_CHRG_STAT_CHGDONE,
};


struct cx7601 {
	struct device *dev;
	struct i2c_client *client;
	enum cx7601_part_no part_no;
	int revision;
	int status;
	struct mutex i2c_rw_lock;
	bool charge_enabled;/* Register bit status */
	struct cx7601_platform_data* platform_data;
	struct charger_device *chg_dev;
};

static const struct charger_properties cx7601_chg_props = {
	.alias_name = "cx7601",
};

static int __cx7601_read_reg(struct cx7601* bq, u8 reg, u8 *data)
{
	s32 ret;

	ret = i2c_smbus_read_byte_data(bq->client, reg);
	if (ret < 0) {
		pr_err("i2c read fail: can't read from reg 0x%02X\n", reg);
		return ret;
	}
	*data = (u8)ret;

	return 0;
}

static int __cx7601_write_reg(struct cx7601* bq, int reg, u8 val)
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

static int cx7601_read_byte(struct cx7601 *bq, u8 *data, u8 reg)
{
	int ret;

	mutex_lock(&bq->i2c_rw_lock);
	ret = __cx7601_read_reg(bq, reg, data);
	mutex_unlock(&bq->i2c_rw_lock);

	return ret;
}


static int cx7601_write_byte(struct cx7601 *bq, u8 reg, u8 data)
{
	int ret;

	mutex_lock(&bq->i2c_rw_lock);
	ret = __cx7601_write_reg(bq, reg, data);
	mutex_unlock(&bq->i2c_rw_lock);

	if (ret) {
		pr_err("Failed: reg=%02X, ret=%d\n", reg, ret);
	}

	return ret;
}


static int cx7601_update_bits(struct cx7601 *bq, u8 reg,
				u8 mask, u8 data)
{
	int ret;
	u8 tmp;

	mutex_lock(&bq->i2c_rw_lock);
	ret = __cx7601_read_reg(bq, reg, &tmp);
	if (ret) {
		pr_err("Failed: reg=%02X, ret=%d\n", reg, ret);
		goto out;
	}
	tmp &= ~mask;
	tmp |= data & mask;

	ret = __cx7601_write_reg(bq, reg, tmp);
	if (ret) {
		pr_err("Failed: reg=%02X, ret=%d\n", reg, ret);
	}
out:
	mutex_unlock(&bq->i2c_rw_lock);
	return ret;
}

static int cx7601_enable_otg(struct cx7601 *bq)
{
	u8 val = REG01_OTG_ENABLE << REG01_OTG_CONFIG_SHIFT;
        pr_info("cx7601_enable_otg enter\n");
	return cx7601_update_bits(bq, CX7601_REG_01,
				REG01_OTG_CONFIG_MASK, val);

}

static int cx7601_disable_otg(struct cx7601 *bq)
{
	u8 val = REG01_OTG_DISABLE << REG01_OTG_CONFIG_SHIFT;
	pr_info("cx7601_disable_otg enter\n");
	return cx7601_update_bits(bq, CX7601_REG_01,
				   REG01_OTG_CONFIG_MASK, val);

}

static int cx7601_enable_charger(struct cx7601 *bq)
{
	int ret;
	u8 val = REG01_CHG_ENABLE << REG01_CHG_CONFIG_SHIFT;
	//cx7601_dump_regs(bq);
	ret = cx7601_update_bits(bq, CX7601_REG_01, REG01_CHG_CONFIG_MASK, val);

	return ret;
}

static int cx7601_disable_charger(struct cx7601 *bq)
{
	int ret;
	u8 val = REG01_CHG_DISABLE << REG01_CHG_CONFIG_SHIFT;

	ret = cx7601_update_bits(bq, CX7601_REG_01, REG01_CHG_CONFIG_MASK, val);
	return ret;
}

int cx7601_set_chargecurrent(struct cx7601 *bq, int curr)
{
	u8 ichg;

	if (curr < REG02_ICHG_BASE){
		curr = REG02_ICHG_BASE;
	}

	ichg = (curr - REG02_ICHG_BASE)/REG02_ICHG_LSB;
	return cx7601_update_bits(bq, CX7601_REG_02, REG02_ICHG_MASK,
				ichg << REG02_ICHG_SHIFT);

}

int cx7601_set_term_current(struct cx7601 *bq, int curr)
{
	u8 iterm;

	if (curr < REG03_ITERM_BASE)
		curr = REG03_ITERM_BASE;

	iterm = (curr - REG03_ITERM_BASE) / REG03_ITERM_LSB;

	return cx7601_update_bits(bq, CX7601_REG_03, REG03_ITERM_MASK,
				iterm << REG03_ITERM_SHIFT);
}
EXPORT_SYMBOL_GPL(cx7601_set_term_current);


int cx7601_set_prechg_current(struct cx7601 *bq, int curr)
{
	u8 iprechg;

	if(curr <= 180)
		iprechg   = 2;
	else if (curr <= 256)
		iprechg = 1;
	else if(curr <= 384)
		iprechg = 3;
	else if(curr <=512)
		iprechg = 4;
	else
		iprechg = (curr - REG03_IPRECHG_BASE) / REG03_IPRECHG_LSB;

	return cx7601_update_bits(bq, CX7601_REG_03, REG03_IPRECHG_MASK,
				iprechg << REG03_IPRECHG_SHIFT);
}
EXPORT_SYMBOL_GPL(cx7601_set_prechg_current);

int cx7601_set_chargevolt(struct cx7601 *bq, int volt)
{
	u8 val;

	if (volt < REG04_VREG_BASE)
		volt = REG04_VREG_BASE;

	val = (volt - REG04_VREG_BASE)/REG04_VREG_LSB;
	return cx7601_update_bits(bq, CX7601_REG_04, REG04_VREG_MASK,
				val << REG04_VREG_SHIFT);
}


int cx7601_set_input_volt_limit(struct cx7601 *bq, int volt)
{
	u8 val;

	if (volt < REG00_VINDPM_BASE)
		volt = REG00_VINDPM_BASE;

	val = (volt - REG00_VINDPM_BASE) / REG00_VINDPM_LSB;
	return cx7601_update_bits(bq, CX7601_REG_00, REG00_VINDPM_MASK,
				val << REG00_VINDPM_SHIFT);//done
}

int cx7601_set_input_current_limit(struct cx7601 *bq, int curr)
{
	u8 val;

	if (curr < 150)
		val = 0;
	else if(curr < 500)
		val = 1;
	else if(curr < 900)
		val = 2;
	else if(curr < 1000)
		val = 3;
	else if(curr < 1500)
		val = 4;
	else if(curr < 2000)
		val = 5;
	else if(curr == 2000)
		val = 6;
	else
		val = 7;

	return cx7601_update_bits(bq, CX7601_REG_00, REG00_IINLIM_MASK,
				val << REG00_IINLIM_SHIFT);//done
}


int cx7601_set_watchdog_timer(struct cx7601 *bq, u8 timeout)
{
	u8 temp;

	temp = (u8)(((timeout - REG05_WDT_BASE) / REG05_WDT_LSB) << REG05_WDT_SHIFT);

	return cx7601_update_bits(bq, CX7601_REG_05, REG05_WDT_MASK, temp);
}

EXPORT_SYMBOL_GPL(cx7601_set_watchdog_timer);

int cx7601_disable_watchdog_timer(struct cx7601 *bq)
{
	u8 val = REG05_WDT_DISABLE << REG05_WDT_SHIFT;

	return cx7601_update_bits(bq, CX7601_REG_05, REG05_WDT_MASK, val);
}
EXPORT_SYMBOL_GPL(cx7601_disable_watchdog_timer);

int cx7601_reset_watchdog_timer(struct cx7601 *bq)
{
	u8 val = REG01_WDT_RESET << REG01_WDT_RESET_SHIFT;

	return cx7601_update_bits(bq, CX7601_REG_01, REG01_WDT_RESET_MASK, val);
}
EXPORT_SYMBOL_GPL(cx7601_reset_watchdog_timer);

int cx7601_reset_chip(struct cx7601 *bq)
{
	int ret;

    u8 val = REG01_REG_RESET << REG01_REG_RESET_SHIFT;
	ret = cx7601_update_bits(bq, CX7601_REG_01, REG01_REG_RESET_MASK, val);
	return ret;
}
EXPORT_SYMBOL_GPL(cx7601_reset_chip);

int cx7601_enter_hiz_mode(struct cx7601 *bq)
{
	u8 val = REG00_HIZ_ENABLE << REG00_ENHIZ_SHIFT;

	return cx7601_update_bits(bq, CX7601_REG_00, REG00_ENHIZ_MASK, val);

}
EXPORT_SYMBOL_GPL(cx7601_enter_hiz_mode);

int cx7601_exit_hiz_mode(struct cx7601 *bq)
{

	u8 val = REG00_HIZ_DISABLE << REG00_ENHIZ_SHIFT;

	return cx7601_update_bits(bq, CX7601_REG_00, REG00_ENHIZ_MASK, val);

}
EXPORT_SYMBOL_GPL(cx7601_exit_hiz_mode);

int cx7601_get_hiz_mode(struct cx7601 *bq, u8 *state)
{
	u8 val;
	int ret;

	ret = cx7601_read_byte(bq, &val, CX7601_REG_00);
	if (ret)
		return ret;
	*state = (val & REG00_ENHIZ_MASK) >> REG00_ENHIZ_SHIFT;

	return 0;
}
EXPORT_SYMBOL_GPL(cx7601_get_hiz_mode);


static int cx7601_enable_term(struct cx7601* bq, bool enable)
{
	u8 val;
	int ret;

	if (enable)
		val = REG05_TERM_ENABLE << REG05_EN_TERM_SHIFT;
	else
		val = REG05_TERM_DISABLE << REG05_EN_TERM_SHIFT;

	ret = cx7601_update_bits(bq, CX7601_REG_05, REG05_EN_TERM_MASK, val);

	return ret;
}
EXPORT_SYMBOL_GPL(cx7601_enable_term);

int cx7601_set_boost_current(struct cx7601 *bq, int curr)
{
	u8 val;

	val = REG01_BOOST_LIM_0P5A;
	if (curr == BOOSTI_1200)
		val = REG01_BOOST_LIM_1P2A;

	return cx7601_update_bits(bq, CX7601_REG_01, REG01_BOOST_LIM_MASK,
				val << REG01_BOOST_LIM_SHIFT);//done
}

int cx7601_set_boost_voltage(struct cx7601 *bq, int volt)
{
	u8 val;

	if (volt == BOOSTV_4850)
		val = REG06_BOOSTV_4P85V;
	else if (volt == BOOSTV_5150)
		val = REG06_BOOSTV_5P15V;
	else if (volt == BOOSTV_5300)
		val = REG06_BOOSTV_5P3V;
	else
		val = REG06_BOOSTV_5P15V;

	return cx7601_update_bits(bq, CX7601_REG_06, REG06_BOOSTV_MASK,
				val << REG06_BOOSTV_SHIFT);
}
EXPORT_SYMBOL_GPL(cx7601_set_boost_voltage);

static int cx7601_set_acovp_threshold(struct cx7601 *bq, int volt)
{
	u8 val;

	if (volt == VAC_OVP_14300)
		val = REG07_AVOV_TH_14P3V;
	else if (volt == VAC_OVP_10500)
		val = REG07_AVOV_TH_10P5V;
	else if (volt == VAC_OVP_6200)
		val = REG07_AVOV_TH_6P2V;
	else
		val = REG07_AVOV_TH_5P5V;

	return cx7601_update_bits(bq, CX7601_REG_07, REG07_AVOV_TH_MASK,
				val << REG07_AVOV_TH_SHIFT);//done
}
EXPORT_SYMBOL_GPL(cx7601_set_acovp_threshold);

static int cx7601_set_stat_ctrl(struct cx7601 *bq, int ctrl)
{
	u8 val;

	val = ctrl;

	return cx7601_update_bits(bq, CX7601_REG_00, REG00_STAT_CTRL_MASK,
				val << REG00_STAT_CTRL_SHIFT);
}

static int cx7601_enable_batfet(struct cx7601 *bq)
{
	const u8 val = REG07_BATFET_ON << REG07_BATFET_DIS_SHIFT;

	return cx7601_update_bits(bq, CX7601_REG_07, REG07_BATFET_DIS_MASK,
				val);
}
EXPORT_SYMBOL_GPL(cx7601_enable_batfet);


static int cx7601_disable_batfet(struct cx7601 *bq)
{
	const u8 val = REG07_BATFET_OFF << REG07_BATFET_DIS_SHIFT;

	return cx7601_update_bits(bq, CX7601_REG_07, REG07_BATFET_DIS_MASK,
				val);
}
EXPORT_SYMBOL_GPL(cx7601_disable_batfet);

static int cx7601_set_batfet_delay(struct cx7601 *bq, uint8_t delay)
{
	u8 val;

	if (delay == 0)
		val = REG0C_BATFET_DLY_0S;
	else
		val = REG0C_BATFET_DLY_10S;

	val <<= REG0C_BATFET_DLY_SHIFT;

	return cx7601_update_bits(bq, CX7601_REG_0C, REG0C_BATFET_DLY_MASK,
								val);//done
}
EXPORT_SYMBOL_GPL(cx7601_set_batfet_delay);

static int cx7601_set_vdpm_bat_track(struct cx7601 *bq)
{
	u8 val;
	u32 voltage;
	if (bq->platform_data->usb.vlim) {
		voltage = (bq->platform_data->usb.vlim - REG00_VINDPM_BASE) /
			REG00_VINDPM_LSB;
		val = (u8)(voltage << REG00_VINDPM_SHIFT);
	}
	return cx7601_update_bits(bq, CX7601_REG_00, REG00_VINDPM_MASK,
				val);
}
EXPORT_SYMBOL_GPL(cx7601_set_vdpm_bat_track);

static int cx7601_enable_safety_timer(struct cx7601 *bq)
{
	const u8 val = REG05_CHG_TIMER_ENABLE << REG05_EN_TIMER_SHIFT;

	return cx7601_update_bits(bq, CX7601_REG_05, REG05_EN_TIMER_MASK,
				val);
}
EXPORT_SYMBOL_GPL(cx7601_enable_safety_timer);


static int cx7601_disable_safety_timer(struct cx7601 *bq)
{
	const u8 val = REG05_CHG_TIMER_DISABLE << REG05_EN_TIMER_SHIFT;

	return cx7601_update_bits(bq, CX7601_REG_05, REG05_EN_TIMER_MASK,
				val);
}
EXPORT_SYMBOL_GPL(cx7601_disable_safety_timer);

static struct cx7601_platform_data* cx7601_parse_dt(struct device *dev,
							struct cx7601 * bq)
{
	int ret;
	struct device_node *np = dev->of_node;
	struct cx7601_platform_data* pdata;
	pdata = devm_kzalloc(dev, sizeof(struct cx7601_platform_data),
						GFP_KERNEL);
	if (!pdata) {
		pr_err("Out of memory\n");
		return NULL;
	}

	ret = of_property_read_u32(np,"cx,cx7601,usb-vlim",&pdata->usb.vlim);
	if(ret) {
		pr_err("Failed to read node of cx,cx7601,usb-vlim\n");
	}

	ret = of_property_read_u32(np,"cx,cx7601,usb-ilim",&pdata->usb.ilim);
	if(ret) {
		pr_err("Failed to read node of cx,cx7601,usb-ilim\n");
	}

	ret = of_property_read_u32(np,"cx,cx7601,usb-vreg",&pdata->usb.vreg);
	if(ret) {
		pr_err("Failed to read node of cx,cx7601,usb-vreg\n");
	}

	ret = of_property_read_u32(np,"cx,cx7601,usb-ichg",&pdata->usb.ichg);
	if(ret) {
		pr_err("Failed to read node of cx,cx7601,usb-ichg\n");
	}

	ret = of_property_read_u32(np,"cx,cx7601,stat-pin-ctrl",&pdata->statctrl);
	if(ret) {
		pr_err("Failed to read node of cx,cx7601,stat-pin-ctrl\n");
	}

	ret = of_property_read_u32(np,"cx,cx7601,precharge-current",&pdata->iprechg);
	if(ret) {
		pr_err("Failed to read node of cx,cx7601,precharge-current\n");
	}

	ret = of_property_read_u32(np,"cx,cx7601,termination-current",&pdata->iterm);
	if(ret) {
		pr_err("Failed to read node of cx,cx7601,termination-current\n");
	}

	ret = of_property_read_u32(np,"cx,cx7601,boost-voltage",&pdata->boostv);
	if(ret) {
		pr_err("Failed to read node of cx,cx7601,boost-voltage\n");
	}

	ret = of_property_read_u32(np,"cx,cx7601,boost-current",&pdata->boosti);
	if(ret) {
		pr_err("Failed to read node of cx,cx7601,boost-current\n");
	}

	ret = of_property_read_u32(np,"cx,cx7601,vac-ovp-threshold",&pdata->vac_ovp);
	if(ret) {
		pr_err("Failed to read node of cx,cx7601,vac-ovp-threshold\n");
	}

	return pdata;
}

static int cx7601_init_device(struct cx7601 *bq)
{
	int ret;

	cx7601_disable_watchdog_timer(bq);
	cx7601_set_vdpm_bat_track(bq);

	ret = cx7601_set_stat_ctrl(bq, bq->platform_data->statctrl);
	if (ret)
		pr_err("Failed to set stat pin control mode, ret = %d\n",ret);

	ret = cx7601_set_prechg_current(bq, bq->platform_data->iprechg);
	if (ret)
		pr_err("Failed to set prechg current, ret = %d\n",ret);

	ret = cx7601_set_term_current(bq, bq->platform_data->iterm);
	if (ret)
		pr_err("Failed to set termination current, ret = %d\n",ret);

	ret = cx7601_set_boost_voltage(bq, bq->platform_data->boostv);
	if (ret)
		pr_err("Failed to set boost voltage, ret = %d\n",ret);

	ret = cx7601_set_boost_current(bq, bq->platform_data->boosti);
	if (ret)
		pr_err("Failed to set boost current, ret = %d\n",ret);

	ret = cx7601_set_acovp_threshold(bq, bq->platform_data->vac_ovp);
	if (ret)
		pr_err("Failed to set acovp threshold, ret = %d\n",ret);

	return 0;
}


static int cx7601_detect_device(struct cx7601* bq)
{
    int ret;
    u8 data;

	ret = cx7601_read_byte(bq, &data, CX7601_REG_0A);
    if(ret == 0){
        bq->part_no = (data & REG0A_PN_MASK) >> REG0A_PN_SHIFT;
        bq->revision = (data & REG0A_DEV_REV_MASK) >> REG0A_DEV_REV_SHIFT;
    }
	ret = cx7601_read_byte(bq, &data, CX7601_REG_0D);
	if(data==0xff){
		ret=1;
	}
    return ret;
}

static void cx7601_dump_regs(struct cx7601 *bq)
{
	int addr;
	u8 val;
	int ret;

	for (addr = 0x0; addr <= 0x0D; addr++) {
		ret = cx7601_read_byte(bq, &val, addr);
		if (ret == 0)
			pr_err("Reg[%.2x] = 0x%.2x\n", addr, val);
	}


}

static ssize_t cx7601_show_registers(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct cx7601 *bq = dev_get_drvdata(dev);
	u8 addr;
	u8 val;
	u8 tmpbuf[200];
	int len;
	int idx = 0;
	int ret ;

	idx = snprintf(buf, PAGE_SIZE, "%s:\n", "cx7601 Reg");
	for (addr = 0x0; addr <= 0x0D; addr++) {
		ret = cx7601_read_byte(bq, &val, addr);
		if (ret == 0) {
			len = snprintf(tmpbuf, PAGE_SIZE - idx,"Reg[%.2x] = 0x%.2x\n", addr, val);
			memcpy(&buf[idx], tmpbuf, len);
			idx += len;
		}
	}

	return idx;
}

static ssize_t cx7601_store_registers(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct cx7601 *bq = dev_get_drvdata(dev);
	int ret;
	unsigned int reg;
	unsigned int val;

	ret = sscanf(buf, "%x %x", &reg, &val);
	if (ret == 2 && reg < 0x0D) {
		cx7601_write_byte(bq, (unsigned char)reg, (unsigned char)val);
	}

	return count;
}

static DEVICE_ATTR(registers, S_IRUGO | S_IWUSR, cx7601_show_registers, cx7601_store_registers);

static struct attribute *cx7601_attributes[] = {
	&dev_attr_registers.attr,
	NULL,
};

static const struct attribute_group cx7601_attr_group = {
	.attrs = cx7601_attributes,
};

static int cx7601_set_hizmode(struct charger_device *chg_dev, bool enable)
{

	struct cx7601 *bq = dev_get_drvdata(&chg_dev->dev);
	int ret = 0;

	if (enable)
  	{
		ret = cx7601_enter_hiz_mode(bq);
  	 }
	else
  	{
		ret = cx7601_exit_hiz_mode(bq);
   	}
	pr_err("%s set hizmode %s\n", enable ? "enable" : "disable",
				  !ret ? "successfully" : "failed");

	return ret;
}

static int cx7601_charging(struct charger_device *chg_dev, bool enable)
{

	struct cx7601 *bq = dev_get_drvdata(&chg_dev->dev);
	int ret = 0;
	u8 val;

	if (enable)
  	{
		ret = cx7601_enable_charger(bq);
  	 }
	else
  	{
		ret = cx7601_disable_charger(bq);
   	}
	pr_err("%s charger %s\n", enable ? "enable" : "disable",
				  !ret ? "successfully" : "failed");

	ret = cx7601_read_byte(bq, &val, CX7601_REG_01);
	if (!ret  )
		bq->charge_enabled = !!(val & REG01_CHG_CONFIG_MASK);

	return ret;
}

static int cx7601_plug_in(struct charger_device *chg_dev)
{

	int ret;

	ret = cx7601_charging(chg_dev, true);

	if (!ret)
		pr_err("Failed to enable charging:%d\n", ret);

	return ret;
}

static int cx7601_plug_out(struct charger_device *chg_dev)
{
	int ret;

	pr_info("[%s] enter!", __func__);
	ret = cx7601_charging(chg_dev, false);

	if (!ret)
		pr_err("Failed to disable charging:%d\n", ret);

	return ret;
}

static int cx7601_dump_register(struct charger_device *chg_dev)
{
	struct cx7601 *bq = dev_get_drvdata(&chg_dev->dev);

	cx7601_dump_regs(bq);

	return 0;
}

static int cx7601_is_charging_enable(struct charger_device *chg_dev, bool *en)
{
	struct cx7601 *bq = dev_get_drvdata(&chg_dev->dev);

	*en = bq->charge_enabled;

	return 0;
}

static int cx7601_is_charging_done(struct charger_device *chg_dev, bool *done)
{
	struct cx7601 *bq = dev_get_drvdata(&chg_dev->dev);
	int ret;
	u8 val;

	ret = cx7601_read_byte(bq, &val, CX7601_REG_08);
	if (!ret) {
		val = val & REG08_CHRG_STAT_MASK;
		val = val >> REG08_CHRG_STAT_SHIFT;
		*done = (val == REG08_CHRG_STAT_CHGDONE);
	}

	return ret;
}

static int cx7601_set_ichg(struct charger_device *chg_dev, u32 curr)
{
	struct cx7601 *bq = dev_get_drvdata(&chg_dev->dev);

	pr_info("[%s] curr=%d\n", __func__, curr);

	return cx7601_set_chargecurrent(bq, curr/1000);
}


static int cx7601_get_ichg(struct charger_device *chg_dev, u32 *curr)
{
	struct cx7601 *bq = dev_get_drvdata(&chg_dev->dev);
	u8 reg_val;
	int ichg;
	int ret;

	ret = cx7601_read_byte(bq, &reg_val, CX7601_REG_02);
	if (!ret) {
		ichg = (reg_val & REG02_ICHG_MASK) >> REG02_ICHG_SHIFT;
		ichg = ichg * REG02_ICHG_LSB + REG02_ICHG_BASE;
		*curr = ichg * 1000;
	}
	pr_info("[%s] curr=%d\n", __func__, *curr);

	return ret;
}

static int cx7601_get_min_ichg(struct charger_device *chg_dev, u32 *curr)
{

	*curr = 60 * 1000;

	return 0;
}

static int cx7601_set_vchg(struct charger_device *chg_dev, u32 volt)
{

	struct cx7601 *bq = dev_get_drvdata(&chg_dev->dev);

	return cx7601_set_chargevolt(bq, volt/1000);
}

static int cx7601_get_vchg(struct charger_device *chg_dev, u32 *volt)
{
	struct cx7601 *bq = dev_get_drvdata(&chg_dev->dev);
	u8 reg_val;
	int vchg;
	int ret;

	ret = cx7601_read_byte(bq, &reg_val, CX7601_REG_04);
	if (!ret) {
		vchg = (reg_val & REG04_VREG_MASK) >> REG04_VREG_SHIFT;
		vchg = vchg * REG04_VREG_LSB + REG04_VREG_BASE;
		*volt = vchg * 1000;
	}

	return ret;
}

static int cx7601_set_ivl(struct charger_device *chg_dev, u32 volt)
{

	struct cx7601 *bq = dev_get_drvdata(&chg_dev->dev);

	return cx7601_set_input_volt_limit(bq, volt/1000);

}

static int cx7601_set_icl(struct charger_device *chg_dev, u32 curr)
{

	struct cx7601 *bq = dev_get_drvdata(&chg_dev->dev);

	return cx7601_set_input_current_limit(bq, curr/1000);
}

static int cx7601_get_icl(struct charger_device *chg_dev, u32 *curr)
{
	struct cx7601 *bq = dev_get_drvdata(&chg_dev->dev);
	u8 reg_val;
	int icl;
	int ret;

	ret = cx7601_read_byte(bq, &reg_val, CX7601_REG_00);
	if (!ret) {
		icl = (reg_val & REG00_IINLIM_MASK) >> REG00_IINLIM_SHIFT;
		switch(icl){
		case 0:
			icl = 100;
			break;
		case 1:
			icl = 150;
			break;
		case 2:
			icl = 500;
			break;
		case 3:
			icl = 900;
			break;
		case 4:
			icl = 1000;
			break;
		case 5:
			icl = 1500;
			break;
		case 6:
			icl = 2000;
			break;
		case 7:
			icl = 3000;
			break;
			}
		*curr = icl * 1000;//done
	}

	return ret;

}

static int cx7601_kick_wdt(struct charger_device *chg_dev)
{
	struct cx7601 *bq = dev_get_drvdata(&chg_dev->dev);

	return cx7601_reset_watchdog_timer(bq);
}

static int cx7601_set_otg(struct charger_device *chg_dev, bool en)
{
	int ret;
	struct cx7601 *bq = dev_get_drvdata(&chg_dev->dev);

	if (en)
  	{
		ret = cx7601_enable_otg(bq);
   	}
	else
  	{
		ret = cx7601_disable_otg(bq);
   	}
	return ret;
}

static int cx7601_set_safety_timer(struct charger_device *chg_dev, bool en)
{
	struct cx7601 *bq = dev_get_drvdata(&chg_dev->dev);
	int ret;

	if (en)
		ret = cx7601_enable_safety_timer(bq);
	else
		ret = cx7601_disable_safety_timer(bq);

	return ret;
}

static int cx7601_is_safety_timer_enabled(struct charger_device *chg_dev, bool *en)
{
	struct cx7601 *bq = dev_get_drvdata(&chg_dev->dev);
	int ret;
	u8 reg_val;

	ret = cx7601_read_byte(bq, &reg_val, CX7601_REG_05);

	if (!ret)
		*en = !!(reg_val & REG05_EN_TIMER_MASK);

	return ret;
}


static int cx7601_set_boost_ilmt(struct charger_device *chg_dev, u32 curr)
{
	struct cx7601 *bq = dev_get_drvdata(&chg_dev->dev);
	int ret;

	ret = cx7601_set_boost_current(bq, curr/1000);

	return ret;
}

static int cx7601_do_event(struct charger_device *chg_dev, u32 event,
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

static struct charger_ops cx7601_chg_ops = {
	/* Normal charging */
        .hiz_mode = cx7601_set_hizmode,
	.plug_in = cx7601_plug_in,
	.plug_out = cx7601_plug_out,
	.dump_registers = cx7601_dump_register,
	.enable = cx7601_charging,
	.is_enabled = cx7601_is_charging_enable,
	.get_charging_current = cx7601_get_ichg,
	.set_charging_current = cx7601_set_ichg,
	.get_input_current = cx7601_get_icl,
	.set_input_current = cx7601_set_icl,
	.get_constant_voltage = cx7601_get_vchg,
	.set_constant_voltage = cx7601_set_vchg,
	.kick_wdt = cx7601_kick_wdt,
	.set_mivr = cx7601_set_ivl,
	.is_charging_done = cx7601_is_charging_done,
	.get_min_charging_current = cx7601_get_min_ichg,

	/* Safety timer */
	.enable_safety_timer = cx7601_set_safety_timer,
	.is_safety_timer_enabled = cx7601_is_safety_timer_enabled,

	/* Power path */
	.enable_powerpath = NULL,
	.is_powerpath_enabled = NULL,

	/* OTG */
	.enable_otg = cx7601_set_otg,
	.set_boost_current_limit = cx7601_set_boost_ilmt,
	.enable_discharge = NULL,
	.event = cx7601_do_event,

	/* PE+/PE+20 */
	.send_ta_current_pattern = NULL,
	.set_pe20_efficiency_table = NULL,
	.send_ta20_current_pattern = NULL,
//	.set_ta20_reset = NULL,
	.enable_cable_drop_comp = NULL,

	/* ADC */
	.get_tchg_adc = NULL,
};



static int cx7601_charger_probe(struct i2c_client *client,
					const struct i2c_device_id *id)
{
	struct cx7601 *bq;

	int ret;

	bq = devm_kzalloc(&client->dev, sizeof(struct cx7601), GFP_KERNEL);
	if (!bq) {
		pr_err("Out of memory\n");
		return -ENOMEM;
	}

	bq->dev = &client->dev;
	bq->client = client;

	i2c_set_clientdata(client, bq);

	mutex_init(&bq->i2c_rw_lock);

	ret = cx7601_detect_device(bq);
	if (ret) {
		pr_err("No cx7601 device found!\n");
		return -ENODEV;
	}

	if (client->dev.of_node)
		bq->platform_data = cx7601_parse_dt(&client->dev, bq);
	else
		bq->platform_data = client->dev.platform_data;

	if (!bq->platform_data) {
		pr_err("No platform data provided.\n");
		return -EINVAL;
	}

	bq->chg_dev = charger_device_register("primary_chg",
			&client->dev, bq,
			&cx7601_chg_ops,
			&cx7601_chg_props);
	if (IS_ERR_OR_NULL(bq->chg_dev)) {
		ret = PTR_ERR(bq->chg_dev);
		goto err_0;
	}

	ret = cx7601_init_device(bq);
	if (ret) {
		pr_err("Failed to init device\n");
		return ret;
	}

	ret = sysfs_create_group(&bq->dev->kobj, &cx7601_attr_group);
	if (ret) {
		dev_err(bq->dev, "failed to register sysfs. err: %d\n", ret);
	}

	pr_err("cx7601 probe successfully, Part Num:%d, Revision:%d\n!",
				bq->part_no, bq->revision);

	return 0;

err_0:

	return ret;
}

static int cx7601_charger_remove(struct i2c_client *client)
{
	struct cx7601 *bq = i2c_get_clientdata(client);


	mutex_destroy(&bq->i2c_rw_lock);

	sysfs_remove_group(&bq->dev->kobj, &cx7601_attr_group);


	return 0;
}


static void cx7601_charger_shutdown(struct i2c_client *client)
{
}

static struct of_device_id cx7601_charger_match_table[] = {
	{.compatible = "cx,cx7601_charger",},
	{},
};
MODULE_DEVICE_TABLE(of,cx7601_charger_match_table);

static const struct i2c_device_id cx7601_charger_id[] = {
	{ "cx7601-charger", CX7601 },
	{},
};
MODULE_DEVICE_TABLE(i2c, cx7601_charger_id);

static struct i2c_driver cx7601_charger_driver = {
	.driver 	= {
		.name 	= "cx7601-charger",
		.owner 	= THIS_MODULE,
		.of_match_table = cx7601_charger_match_table,
	},
	.id_table	= cx7601_charger_id,
	.probe		= cx7601_charger_probe,
	.remove		= cx7601_charger_remove,
	.shutdown	= cx7601_charger_shutdown,

};

module_i2c_driver(cx7601_charger_driver);

MODULE_DESCRIPTION("SUNCORE CX7601 Charger Driver");
MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("SUNCORE");
