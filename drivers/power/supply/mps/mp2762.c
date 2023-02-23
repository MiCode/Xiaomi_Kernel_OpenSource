/*
 * mp2762 battery charging driver
 *
 * Copyright (C) 2017 Texas Instruments * * This package is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.

 * THIS PACKAGE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

/*	date			author			comment
 *	2021-05-10		chenyichun@xiaomi.com	create driver for MP2762
 */

#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/ctype.h>
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
#include <linux/regulator/driver.h>
#include <linux/regulator/of_regulator.h>
#include <linux/regulator/machine.h>
#include <linux/debugfs.h>
#include <linux/bitops.h>
#include <linux/math64.h>
#include <linux/alarmtimer.h>
#include <linux/regmap.h>
#include <linux/hwid.h>
#include <linux/pmic_voter.h>
#include <mt-plat/v1/charger_type.h>
#include <mt-plat/v1/charger_class.h>
#include "mp2762.h"
#include <linux/pinctrl/consumer.h>

#define MP2762_MAX_FCC	2500
#define MP2762_MAX_ICL	2500

enum product_name {
	UNKNOW,
	RUBY,
	RUBYPRO,
	RUBYPLUS,
};

enum mp2762_adc_channel {
	ADC_VBAT,
	ADC_VSYS,
	ADC_CHARGE_IBAT,
	ADC_VBUS,
	ADC_IBUS,
	ADC_VOTG,
	ADC_IOTG,
	ADC_TJ,
	ADC_PSYS,
	ADC_DISCHARGE_IBAT,
};

struct mp2762_platform_data {
	int irq_gpio;
	int irq;
	int iprechg;
	int vprechg;
	int vinmin;
	int iterm;
	int iterm_ffc;
	int fv;
	int fv_ffc;
};

struct mp2762 {
	struct device *dev;
	struct i2c_client *client;
	char model_name[I2C_NAME_SIZE];
	struct regmap    *regmap;
	struct charger_device *chg_dev;
	struct notifier_block   nb;
	struct mp2762_platform_data platform_data;
	struct power_supply *bbc_psy;
	struct votable	*fcc_votable;
	struct votable	*fv_votable;
	struct votable	*icl_votable;
	struct votable	*iterm_votable;
	struct votable	*vinmin_votable;
	struct votable	*enable_votable;
	struct votable	*input_suspend_votable;

	bool chip_ok;
	bool shutdown_flag;
};

struct adc_desc {
	u8 reg;
	int shift;
	int step;
	int rate;
};

static const char * const adc_channel_name[] = {
	[ADC_VBAT]		= "VBAT",
	[ADC_VSYS]		= "VSYS",
	[ADC_CHARGE_IBAT]	= "CHARGE_IBAT",
	[ADC_VBUS]		= "VBUS",
	[ADC_IBUS]		= "IBUS",
	[ADC_VOTG]		= "VOTG",
	[ADC_IOTG]		= "IOTG",
	[ADC_TJ]		= "TJ",
	[ADC_PSYS]		= "PSYS",
	[ADC_DISCHARGE_IBAT]	= "DISCHARGE_IBAT",
};

static struct adc_desc adc_desc_table[] = {
	[ADC_VBAT] = {MP2762_ADC_VBAT_REG, MP2762_ADC_VBAT_SHIFT, MP2762_ADC_VBAT_STEP, MP2762_ADC_VBAT_RATE},
	[ADC_VSYS] = {MP2762_ADC_VSYS_REG, MP2762_ADC_VSYS_SHIFT, MP2762_ADC_VSYS_STEP, MP2762_ADC_VSYS_RATE},
	[ADC_CHARGE_IBAT] = {MP2762_ADC_CHARGE_IBAT_REG, MP2762_ADC_CHARGE_IBAT_SHIFT, MP2762_ADC_CHARGE_IBAT_STEP, MP2762_ADC_CHARGE_IBAT_RATE},
	[ADC_VBUS] = {MP2762_ADC_VBUS_REG, MP2762_ADC_VBUS_SHIFT, MP2762_ADC_VBUS_STEP, MP2762_ADC_VBUS_RATE},
	[ADC_IBUS] = {MP2762_ADC_IBUS_REG, MP2762_ADC_IBUS_SHIFT, MP2762_ADC_IBUS_STEP, MP2762_ADC_IBUS_RATE},
	[ADC_VOTG] = {MP2762_ADC_VOTG_REG, MP2762_ADC_VOTG_SHIFT, MP2762_ADC_VOTG_STEP,	MP2762_ADC_VOTG_RATE},
	[ADC_IOTG] = {MP2762_ADC_IOTG_REG, MP2762_ADC_IOTG_SHIFT, MP2762_ADC_IOTG_STEP,	MP2762_ADC_IOTG_RATE},
	[ADC_TJ] = {MP2762_ADC_TJ_REG, MP2762_ADC_TJ_SHIFT, MP2762_ADC_TJ_STEP, MP2762_ADC_TJ_RATE},
	[ADC_PSYS] = {MP2762_ADC_PSYS_REG, MP2762_ADC_PSYS_SHIFT, MP2762_ADC_PSYS_STEP, MP2762_ADC_PSYS_RATE},
	[ADC_DISCHARGE_IBAT] = {MP2762_ADC_DISCHARGE_IBAT_REG, MP2762_ADC_DISCHARGE_IBAT_SHIFT, MP2762_ADC_DISCHARGE_IBAT_STEP, MP2762_ADC_DISCHARGE_IBAT_RATE},
};

static int product_name = UNKNOW;
static int log_level = 2;

static const struct regmap_config mp2762_regmap_config = {
	.reg_bits = 8,
	.val_bits = 8,
	.max_register = 0x53,
};

#define mps_err(fmt, ...)						\
do {									\
	if (log_level >= 0)						\
		printk(KERN_ERR "[XMCHG_MP2762] " fmt, ##__VA_ARGS__);	\
} while (0)

#define mps_info(fmt, ...)						\
do {									\
	if (log_level >= 1)						\
		printk(KERN_ERR "[XMCHG_MP2762] " fmt, ##__VA_ARGS__);	\
} while (0)

#define mps_dbg(fmt, ...)						\
do {									\
	if (log_level >= 2)						\
		printk(KERN_ERR "[XMCHG_MP2762] " fmt, ##__VA_ARGS__);	\
} while (0)

static int mp2762_check_status(struct mp2762 *chip);

static void mp2762_enable_test_mode(struct mp2762 *chip, bool enable)
{
	int retry_count = 5;
	unsigned int data = 0, target_data = 0;

	target_data = enable ? 0x95 : 0x00;

	while (retry_count) {
		regmap_write(chip->regmap, MP2762_TEST_MODE_REG, target_data);
		msleep(50);
		regmap_read(chip->regmap, MP2762_TEST_MODE_REG, &data);

		if (data != target_data) {
			mps_info("enable_test_mode retry, [retry_count enable data] = [%d %d 0x%02x]\n", retry_count, enable, data);
			retry_count--;
			msleep(100);
		} else {
			mps_info("enable_test_mode enable = %d, data = 0x%02x\n", enable, data);
			break;
		}
	}
}

static int mp2762_set_iinlim0(struct mp2762 *chip, int value)
{
	int ret = 0;
	u8 data = 0;

	if (value < MP2762_IINLIM0_BASE)
		value = MP2762_IINLIM0_BASE;
	else if (value > MP2762_IINLIM0_MAX)
		value = MP2762_IINLIM0_MAX;

	data = ((value - MP2762_IINLIM0_BASE) / MP2762_IINLIM0_STEP) << MP2762_IINLIM0_SHIFT;

	ret = regmap_update_bits(chip->regmap, MP2762_IINLIM0_REG, MP2762_IINLIM0_MASK, data);
	if (ret)
		mps_err("I2C failed to set iinlim0\n");

	return ret;
}

static int mp2762_set_iinlim1(struct mp2762 *chip, int value)
{
	int ret = 0;
	u8 data = 0;

	if (value < MP2762_IINLIM1_BASE)
		value = MP2762_IINLIM1_BASE;
	else if (value > MP2762_IINLIM1_MAX)
		value = MP2762_IINLIM1_MAX;

	data = ((value - MP2762_IINLIM1_BASE) / MP2762_IINLIM1_STEP) << MP2762_IINLIM1_SHIFT;

	ret = regmap_update_bits(chip->regmap, MP2762_IINLIM1_REG, MP2762_IINLIM1_MASK, data);
	if (ret)
		mps_err("I2C failed to set iinlim1\n");

	return ret;
}

static int mp2762_set_vinmin(struct mp2762 *chip, int value)
{
	int ret = 0;
	u8 data = 0;

	if (value < MP2762_VINMIN_BASE)
		value = MP2762_VINMIN_BASE;
	else if (value > MP2762_VINMIN_MAX)
		value = MP2762_VINMIN_MAX;

	data = ((value - MP2762_VINMIN_BASE) / MP2762_VINMIN_STEP) << MP2762_VINMIN_SHIFT;

	ret = regmap_update_bits(chip->regmap, MP2762_VINMIN_REG, MP2762_VINMIN_MASK, data);
	if (ret)
		mps_err("I2C failed to set vinmin\n");

	return ret;
}

static int mp2762_set_fcc(struct mp2762 *chip, int value)
{
	int ret = 0;
	u8 data = 0;

	if (value < MP2762_FCC_BASE)
		value = MP2762_FCC_BASE;
	else if (value > MP2762_FCC_MAX)
		value = MP2762_FCC_MAX;

	data = ((value - MP2762_FCC_BASE) / MP2762_FCC_STEP) << MP2762_FCC_SHIFT;

	ret = regmap_update_bits(chip->regmap, MP2762_FCC_REG, MP2762_FCC_MASK, data);
	if (ret)
		mps_err("I2C failed to set fcc\n");

	return ret;
}

void mp2762_direct_set_fcc(struct charger_device *chg_dev, int value)
{
	struct mp2762 *chip = charger_get_data(chg_dev);
	int ret = 0;
	mps_err("mp2762 set FCC\n");

	ret = mp2762_set_fcc(chip, value);
	if (ret) {
		mps_err("failed to set FCC\n");
	}
}
EXPORT_SYMBOL_GPL(mp2762_direct_set_fcc);

static int mp2762_set_iterm(struct mp2762 *chip, int value)
{
	int ret = 0;
	u8 data = 0;

	if (value < MP2762_ITERM_BASE)
		value = MP2762_ITERM_BASE;
	else if (value > MP2762_ITERM_MAX)
		value = MP2762_ITERM_MAX;

	data = ((value - MP2762_ITERM_BASE) / MP2762_ITERM_STEP) << MP2762_ITERM_SHIFT;

	ret = regmap_update_bits(chip->regmap, MP2762_IPRE_ITERM_REG, MP2762_ITERM_MASK, data);
	if (ret)
		mps_err("I2C failed to set iterm\n");

	return ret;
}

static int mp2762_set_iprechg(struct mp2762 *chip, int value)
{
	int ret = 0;
	u8 data = 0;

	if (value < MP2762_IPRECHG_BASE)
		value = MP2762_IPRECHG_BASE;
	else if (value > MP2762_IPRECHG_MAX)
		value = MP2762_IPRECHG_MAX;

	data = ((value - MP2762_IPRECHG_BASE) / MP2762_IPRECHG_STEP) << MP2762_IPRECHG_SHIFT;

	ret = regmap_update_bits(chip->regmap, MP2762_IPRE_ITERM_REG, MP2762_IPRECHG_SHIFT, data);
	if (ret)
		mps_err("I2C failed to set iprechg\n");

	return ret;
}

static int mp2762_set_fv(struct mp2762 *chip, int value)
{
	int ret = 0;
	u8 data = 0;

	if (value < MP2762_FV_BASE)
		value = MP2762_FV_BASE;
	else if (value > MP2762_FV_MAX)
		value = MP2762_FV_MAX;

	data = ((value - MP2762_FV_BASE) / MP2762_FV_STEP) << MP2762_FV_SHIFT;

	ret = regmap_update_bits(chip->regmap, MP2762_FV_RECHG_VGAP_REG, MP2762_FV_MASK, data);
	if (ret)
		mps_err("I2C failed to set fv\n");

	return ret;
}

static int mp2762_set_rechg_vgap(struct mp2762 *chip, int value)
{
	int ret = 0;
	u8 data = 0;

	if (value < MP2762_RECHG_VGAP_BASE)
		value = MP2762_RECHG_VGAP_BASE;
	else if (value > MP2762_RECHG_VGAP_MAX)
		value = MP2762_RECHG_VGAP_MAX;

	data = ((value - MP2762_RECHG_VGAP_BASE) / MP2762_RECHG_VGAP_STEP) << MP2762_RECHG_VGAP_SHIFT;

	ret = regmap_update_bits(chip->regmap, MP2762_FV_RECHG_VGAP_REG, MP2762_RECHG_VGAP_MASK, data);
	if (ret)
		mps_err("I2C failed to set rechg_vgap\n");

	return ret;
}

static const u32 otg_voltage_table[] = {
	4750, 5100,5150,5300,5500
};
static const u8 otg_voltage_config_table[] = {
	0x00, 0x07,0x08,0x0B,0x0F
};

static int mp2762_set_votg(struct mp2762 *chip, int value)
{
	int ret = 0;
	u8 data = 0;
	int i = 0;

	for (i = 0; i < ARRAY_SIZE(otg_voltage_table); i++) {
		if (value <= otg_voltage_table[i])
			break;
	}
	data =  otg_voltage_config_table[i];

	ret = regmap_update_bits(chip->regmap, MP2762_VOTG_REG, MP2762_VOTG_MASK, data);
	if (ret)
		mps_err("I2C failed to set iotg\n");

	return ret;
}

static int mp2762_set_iotg(struct mp2762 *chip, int value)
{
	int ret = 0;
	u8 data = 0;

	if (value < MP2762_IOTG_BASE)
		value = MP2762_IOTG_BASE;
	else if (value > MP2762_IOTG_MAX)
		value = MP2762_IOTG_MAX;

	data = ((value - MP2762_IOTG_BASE) / MP2762_IOTG_STEP) << MP2762_IOTG_SHIFT;

	ret = regmap_update_bits(chip->regmap, MP2762_IOTG_VPRECHG_REG, MP2762_IOTG_MASK, data);
	if (ret)
		mps_err("I2C failed to set iotg\n");

	return ret;
}

static int mp2762_set_vprechg(struct mp2762 *chip, int value)
{
	int ret = 0;
	u8 data = 0;

	if (value < MP2762_VPRECHG_BASE)
		value = MP2762_VPRECHG_BASE;
	else if (value > MP2762_VPRECHG_MAX)
		value = MP2762_VPRECHG_MAX;

	data = ((value - MP2762_VPRECHG_BASE) / MP2762_VPRECHG_STEP) << MP2762_VPRECHG_SHIFT;

	ret = regmap_update_bits(chip->regmap, MP2762_IOTG_VPRECHG_REG, MP2762_VPRECHG_MASK, data);
	if (ret)
		mps_err("I2C failed to set vprechg\n");

	return ret;
}

static int mp2762_reset_wtd(struct mp2762 *chip)
{
	int ret = 0;

	ret = regmap_update_bits(chip->regmap, MP2762_CONFIGURE_REG0, MP2762_RESET_WTD_BIT, MP2762_RESET_WTD_BIT);
	if (ret)
		mps_err("I2C failed to reset wtd\n");

	return ret;
}

static int mp2762_enable_otg(struct mp2762 *chip, bool enable)
{
	int ret = 0;

	mps_info("enable OTG = %d\n", enable);

	ret = mp2762_check_status(chip);
	if (ret)
		mps_err("failed to check status\n");

	if (enable) {
		mp2762_enable_test_mode(chip, true);
		regmap_write(chip->regmap, 0x3E, 0x00); /* enable burst mode */
		regmap_write(chip->regmap, 0x31, 0x02); /* burst mode Threshold=104 */
		regmap_write(chip->regmap, 0x3F, 0x04); /* rise ZCD */
		regmap_write(chip->regmap, 0x3A, 0x40); /* rise ZCD */
		mp2762_enable_test_mode(chip, false);
	}

	ret = regmap_update_bits(chip->regmap, MP2762_CONFIGURE_REG0, MP2762_ENABLE_OTG_BIT, enable ? MP2762_ENABLE_OTG_BIT : 0);
	if (ret)
		mps_err("I2C failed to enable otg\n");

	if (!enable) {
		mp2762_enable_test_mode(chip, true);
		regmap_write(chip->regmap, 0x3E, 0x80); /* disable burst mode */
		regmap_write(chip->regmap, 0x31, 0x03); /* burst mode Threshold=101 */
		regmap_write(chip->regmap, 0x3A, 0x00); /* restore ZCD */
		regmap_write(chip->regmap, 0x3F, 0x00); /* restore ZCD */
		mp2762_enable_test_mode(chip, false);
	}

	return ret;
}

static int mp2762_enable_charge(struct mp2762 *chip, bool enable)
{
	int ret = 0;

	ret = mp2762_check_status(chip);
	if (ret)
		mps_err("failed to check status\n");

	ret = regmap_update_bits(chip->regmap, MP2762_CONFIGURE_REG0, MP2762_ENABLE_CHG_BIT, enable ? MP2762_ENABLE_CHG_BIT : 0);
	if (ret)
		mps_err("I2C failed to enable chg\n");

	return ret;
}

static int mp2762_is_enable(struct mp2762 *chip, bool *enable)
{
	int ret = 0;
	unsigned int data = 0;

	ret = regmap_read(chip->regmap, MP2762_CONFIGURE_REG0, &data);
	if (ret)
		mps_err("I2C failed to read charge enable\n");

	*enable = !!(data & MP2762_ENABLE_CHG_BIT);

	return ret;
}

static int mp2762_suspend_input(struct mp2762 *chip, bool enable)
{
	int ret = 0;

	ret = regmap_update_bits(chip->regmap, MP2762_CONFIGURE_REG0, MP2762_SUSP_INPUT_BIT, enable ? MP2762_SUSP_INPUT_BIT : 0);
	if (ret)
		mps_err("I2C failed to suspend input\n");

	return ret;
}

static int mp2762_is_suspend_input(struct mp2762 *chip, bool *enable)
{
	int ret = 0;
	unsigned int data = 0;

	ret = regmap_read(chip->regmap, MP2762_CONFIGURE_REG0, &data);
	if (ret)
		mps_err("I2C failed to read charge enable\n");

	*enable = !(data & MP2762_SUSP_INPUT_BIT);

	return ret;
}

static int mp2762_enable_terminate(struct mp2762 *chip, bool enable)
{
	int ret = 0;

	ret = regmap_update_bits(chip->regmap, MP2762_CONFIGURE_REG1, MP2762_ENABLE_TERM_BIT, enable ? MP2762_ENABLE_TERM_BIT : 0);
	if (ret)
		mps_err("I2C failed to enable terminate\n");

	return ret;
}

static int mp2762_get_charge_status(struct mp2762 *chip, int *value)
{
	int ret = 0;
	unsigned int data = 0;

	ret = regmap_read(chip->regmap, MP2762_STATUS_REG, &data);
	if (ret)
		mps_err("I2C failed to read charge status\n");

	*value = (data & MP2762_CHG_STATUS_MASK) >> MP2762_CHG_STATUS_SHIFT;

	return ret;
}

static int mp2762_enable_safety_timer(struct mp2762 *chip, bool enable)
{
	int ret = 0;

	ret = regmap_update_bits(chip->regmap, MP2762_CONFIGURE_REG1, MP2762_ENABLE_CHG_TIMER_BIT, enable ? MP2762_ENABLE_CHG_TIMER_BIT : 0);
	if (ret)
		mps_err("I2C failed to enable safety timer\n");

	return ret;
}

static int mp2762_read_adc(struct mp2762 *chip, int channel, int *value)
{
	int ret = 0;
	u8 data[2] = {0, 0};

	if (channel < ADC_VBAT || channel > ADC_DISCHARGE_IBAT) {
		mps_err("not support ADC channel\n");
		return -1;
	}

	ret = regmap_raw_read(chip->regmap, adc_desc_table[channel].reg, data, 2);
	if (ret) {
		mps_err("I2C failed to read ADC\n");
		return -1;
	}

	*value = ((((data[1] << 8) + data[0]) >> adc_desc_table[channel].shift) * adc_desc_table[channel].step) / adc_desc_table[channel].rate;
	if (channel == ADC_TJ)
		*value = MP2762_ADC_TJ_CONVERT(*value);

	return ret;
}

static int ops_mp2762_is_enable(struct charger_device *chg_dev, bool *enable)
{
	struct mp2762 *chip = charger_get_data(chg_dev);

	return mp2762_is_enable(chip, enable);
}

static int ops_mp2762_enable_safety_timer(struct charger_device *chg_dev, bool enable)
{
	struct mp2762 *chip = charger_get_data(chg_dev);

	return mp2762_enable_safety_timer(chip, enable);
}

static int ops_mp2762_enable_otg(struct charger_device *chg_dev, bool enable)
{
	struct mp2762 *chip = charger_get_data(chg_dev);

	return mp2762_enable_otg(chip, enable);
}

static int ops_mp2762_enable_powerpath(struct charger_device *chg_dev, bool enable)
{
	struct mp2762 *chip = charger_get_data(chg_dev);

	if(chip->shutdown_flag == true)
	      return 0;

	mps_info("SET MP2762_SUSPEND = %d\n", !enable);
	return mp2762_suspend_input(chip, !enable);
}

static int ops_mp2762_is_powerpath_enabled(struct charger_device *chg_dev, bool *enable)
{
	struct mp2762 *chip = charger_get_data(chg_dev);

	return mp2762_is_suspend_input(chip, enable);
}

static int ops_mp2762_enable_terminate(struct charger_device *chg_dev, bool enable)
{
	struct mp2762 *chip = charger_get_data(chg_dev);

	return mp2762_enable_terminate(chip, enable);
}

static int ops_mp2762_set_iotg(struct charger_device *chg_dev, u32 value)
{
	struct mp2762 *chip = charger_get_data(chg_dev);

	return mp2762_set_iotg(chip, value / 1000);
}

static int ops_mp2762_set_votg(struct charger_device *chg_dev, u32 value)
{
	struct mp2762 *chip = charger_get_data(chg_dev);

	return mp2762_set_votg(chip, value);
}

static int ops_mp2762_get_vbus(struct charger_device *chg_dev, u32 *value)
{
	struct mp2762 *chip = charger_get_data(chg_dev);
	int result = 0, ret = 0;

	ret = mp2762_read_adc(chip, ADC_VBUS, &result);
	if (ret)
		mps_err("ops failed to get VBUS\n");
	else
		*value = result;

	return ret;
}

static int ops_mp2762_get_ibus(struct charger_device *chg_dev, u32 *value)
{
	struct mp2762 *chip = charger_get_data(chg_dev);
	int result = 0, ret = 0;

	ret = mp2762_read_adc(chip, ADC_IBUS, &result);
	if (ret)
		mps_err("ops failed to get IBUS\n");
	else
		*value = result;

	return ret;
}

static int ops_mp2762_get_vsys(struct charger_device *chg_dev, u32 *value)
{
	struct mp2762 *chip = charger_get_data(chg_dev);
	int result = 0, ret = 0;

	ret = mp2762_read_adc(chip, ADC_VSYS, &result);
	if (ret)
		mps_err("ops failed to get IBUS\n");
	else
		*value = result;

	return ret;
}

static int ops_mp2762_get_psys(struct charger_device *chg_dev, u32 *value)
{
	struct mp2762 *chip = charger_get_data(chg_dev);
	int result = 0, ret = 0;

	ret = mp2762_read_adc(chip, ADC_PSYS, &result);
	if (ret)
		mps_err("ops failed to get PSYS\n");
	else
		*value = result;

	return ret;
}

static int ops_mp2762_get_charge_status(struct charger_device *chg_dev, int *value)
{
	struct mp2762 *chip = charger_get_data(chg_dev);
	int ret = 0;

	ret = mp2762_get_charge_status(chip, value);
	if (ret)
		mps_err("ops failed to get charge_status\n");

	return ret;
}

static int ops_mp2762_get_tchg(struct charger_device *chg_dev, int *value_min, int *value_max)
{
	struct mp2762 *chip = charger_get_data(chg_dev);
	int tchg = 0, ret = 0;

	ret = mp2762_read_adc(chip, ADC_TJ, &tchg);
	if (ret) {
		tchg = 250;
		mps_err("ops failed to get charge_status\n");
	}

	*value_max = *value_min = tchg / 10;

	return ret;
}

static const struct charger_ops mp2762_chg_ops = {
	.is_enabled = ops_mp2762_is_enable,
	.enable_powerpath = ops_mp2762_enable_powerpath,
	.is_powerpath_enabled = ops_mp2762_is_powerpath_enabled,
	.enable_safety_timer = ops_mp2762_enable_safety_timer,
	.enable_otg = ops_mp2762_enable_otg,
	.set_boost_current_limit = ops_mp2762_set_iotg,
	.set_otg_voltage = ops_mp2762_set_votg,
	.get_vbus_adc = ops_mp2762_get_vbus,
	.get_ibus_adc = ops_mp2762_get_ibus,
	.get_vsys_adc = ops_mp2762_get_vsys,
	.get_psys_adc = ops_mp2762_get_psys,
	.get_charge_status = ops_mp2762_get_charge_status,
	.enable_termination = ops_mp2762_enable_terminate,
	.get_tchg_adc = ops_mp2762_get_tchg,
};

static const struct charger_properties mp2762_chg_props = {
	.alias_name = "bbc",
};

static enum power_supply_property mp2762_properties[] = {
	POWER_SUPPLY_PROP_MODEL_NAME,
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_CHIP_OK,
	POWER_SUPPLY_PROP_CHARGE_ENABLED,
	POWER_SUPPLY_PROP_CHARGE_STATUS,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_CURRENT_NOW,
	POWER_SUPPLY_PROP_POWER_NOW,
};

static int mp2762_get_property(struct power_supply *psy, enum power_supply_property prop, union power_supply_propval *val)
{
	struct mp2762 *chip = power_supply_get_drvdata(psy);
	bool enable = false;
	int ret = 0;

	switch (prop) {
	case POWER_SUPPLY_PROP_MODEL_NAME:
		val->strval = chip->model_name;
		break;
	case POWER_SUPPLY_PROP_ONLINE:
	case POWER_SUPPLY_PROP_CHIP_OK:
		val->intval = chip->chip_ok;
		break;
	case POWER_SUPPLY_PROP_CHARGE_ENABLED:
		ret = mp2762_is_enable(chip, &enable);
		if (ret)
			return ret;
		val->intval = enable;
		break;
	case POWER_SUPPLY_PROP_CHARGE_STATUS:
		ret = mp2762_get_charge_status(chip, &val->intval);
		if (ret)
			val->intval = 0;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		ret = mp2762_read_adc(chip, ADC_VBUS, &val->intval);
		if (ret)
			val->intval = 0;
		break;
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		ret = mp2762_read_adc(chip, ADC_IBUS, &val->intval);
		if (ret)
			val->intval = 0;
		break;
	case POWER_SUPPLY_PROP_POWER_NOW:
		ret = mp2762_read_adc(chip, ADC_PSYS, &val->intval);
		if (ret)
			val->intval = 0;
		break;
	default:
		mps_err("not support property %d\n", prop);
		return -ENODATA;
	}

	return ret;
}

static const struct power_supply_desc mp2762_psy_desc = {
	.name = "bbc",
	.type = POWER_SUPPLY_TYPE_UNKNOWN,
	.properties = mp2762_properties,
	.num_properties = ARRAY_SIZE(mp2762_properties),
	.get_property = mp2762_get_property,
};

static int mp2762_init_psy(struct mp2762 *chip)
{
	struct power_supply_config mp2762_psy_cfg = {};

	mp2762_psy_cfg.drv_data = chip;
	mp2762_psy_cfg.of_node = chip->dev->of_node;
	chip->bbc_psy = devm_power_supply_register(chip->dev, &mp2762_psy_desc, &mp2762_psy_cfg);
	if (IS_ERR(chip->bbc_psy)) {
		mps_err("failed to register bbc_psy\n");
		return -1;
	}

	return 0;
}

static ssize_t mp2762_show_register(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct mp2762 *chip = dev_get_drvdata(dev);
	u8 tmpbuf[300];
	unsigned int reg = 0, data = 0;
	int len = 0, idx = 0, ret = 0;

	idx = snprintf(buf, PAGE_SIZE, "%s:\n", "MP2762_REG");
	mp2762_enable_test_mode(chip, true);
	for (reg = 0x00; reg <= 0x53; reg++) {
		ret = regmap_read(chip->regmap, reg, &data);
		if (ret) {
			mps_err("failed to read register\n");
			mp2762_enable_test_mode(chip, false);
			return idx;
		}

		len = snprintf(tmpbuf, PAGE_SIZE - idx, "Reg[0x%02x] = 0x%02x\n", reg, data);
		memcpy(&buf[idx], tmpbuf, len);
		idx += len;
	}
	mp2762_enable_test_mode(chip, false);

	return idx;
}

static ssize_t mp2762_store_register(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct mp2762 *chip = dev_get_drvdata(dev);
	unsigned int reg = 0, val = 0;
	int ret = 0;

	ret = sscanf(buf, "%d %d", &reg, &val);
	mps_info("reg = 0x%02x, val = 0x%02x, ret = %d\n", reg, val, ret);

	if (ret == 2 && reg >= 0x00 && reg <= 0x53) {
		if (reg >= 0x38 && reg <= 0x4A)
			mp2762_enable_test_mode(chip, true);

		regmap_write(chip->regmap, reg, val);

		if (reg >= 0x38 && reg <= 0x4A)
			mp2762_enable_test_mode(chip, false);
	}

	return count;
}

static ssize_t mp2762_show_adc(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct mp2762 *chip = dev_get_drvdata(dev);
	u8 tmpbuf[150];
	int channel = 0, value = 0, len = 0, idx = 0, ret = 0;

	idx = snprintf(buf, PAGE_SIZE, "%s:\n", "MP2762_ADC");
	for (channel = ADC_VBAT; channel <= ADC_DISCHARGE_IBAT; channel++) {
		ret = mp2762_read_adc(chip, channel, &value);
		if (ret) {
			mps_err("failed to read ADC\n");
			return idx;
		}

		len = snprintf(tmpbuf, PAGE_SIZE - idx, "%s = %d\n", adc_channel_name[channel], value);
		memcpy(&buf[idx], tmpbuf, len);
		idx += len;
	}

	return idx;
}

static ssize_t mp2762_show_log_level(struct device *dev, struct device_attribute *attr, char *buf)
{
	int ret = 0;

	ret = snprintf(buf, PAGE_SIZE, "%d\n", log_level);
	mps_info("show log_level = %d\n", log_level);

	return ret;
}

static ssize_t mp2762_store_log_level(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	int ret = 0;

	ret = sscanf(buf, "%d", &log_level);
	mps_info("store log_level = %d\n", log_level);

	return count;
}

static DEVICE_ATTR(register, S_IRUGO | S_IWUSR, mp2762_show_register, mp2762_store_register);
static DEVICE_ATTR(adc, S_IRUGO, mp2762_show_adc, NULL);
static DEVICE_ATTR(log_level, S_IRUGO | S_IWUSR, mp2762_show_log_level, mp2762_store_log_level);

static struct attribute *mp2762_attributes[] = {
	&dev_attr_register.attr,
	&dev_attr_adc.attr,
	&dev_attr_log_level.attr,
	NULL,
};

static const struct attribute_group mp2762_attr_group = {
	.attrs = mp2762_attributes,
};

static int mp2762_parse_dt(struct mp2762 *chip)
{
	int ret;
	struct device_node *np = chip->dev->of_node;

	if (!np) {
		mps_err("device tree info missing\n");
		return -1;
	}

	ret = of_property_read_u32(np, "iprechg", &chip->platform_data.iprechg);
	if (ret)
		mps_err("failed to parse iprechg\n");

	ret = of_property_read_u32(np, "vprechg", &chip->platform_data.vprechg);
	if (ret)
		mps_err("failed to parse vprechg\n");

	ret = of_property_read_u32(np, "iterm", &chip->platform_data.iterm);
	if (ret)
		mps_err("failed to parse iterm\n");

	ret = of_property_read_u32(np, "iterm_ffc", &chip->platform_data.iterm_ffc);
	if (ret)
		mps_err("failed to parse iterm_ffc\n");

	ret = of_property_read_u32(np, "fv", &chip->platform_data.fv);
	if (ret)
		mps_err("failed to parse fv\n");

	ret = of_property_read_u32(np, "fv_ffc", &chip->platform_data.fv_ffc);
	if (ret)
		mps_err("failed to parse fv_ffc\n");

	ret = of_property_read_u32(np, "vinmin", &chip->platform_data.vinmin);
	if (ret)
		mps_err("failed to parse vinmin\n");

	chip->platform_data.irq_gpio = of_get_named_gpio(np, "mp2762_irq_gpio", 0);
	if ((!gpio_is_valid(chip->platform_data.irq_gpio))) {
		mps_err("failed to parse mp2762_irq_gpio\n");
		return -1;
	}

	pinctrl_get_select(chip->dev, "interrupt");

	return ret;
}

static int mp2762_init_device(struct mp2762 *chip)
{
	int ret = 0;

	ret = regmap_write(chip->regmap, 0x10, 0x01);
	ret = regmap_write(chip->regmap, 0x11, 0xFF); /* enable PWM skip mode */
	ret = regmap_write(chip->regmap, 0x12, 0x77);
	ret = regmap_write(chip->regmap, 0x2D, 0x0F);
	ret = regmap_write(chip->regmap, 0x36, 0xF5); /* disabe async mode */

	mp2762_enable_test_mode(chip, true);
	regmap_write(chip->regmap, 0x3E, 0x80); /* disable burst mode */
	regmap_write(chip->regmap, 0x31, 0x03); /* burst mode Threshold=101 */
	regmap_write(chip->regmap, 0x3A, 0x00); /* restore ZCD */
	regmap_write(chip->regmap, 0x3F, 0x00); /* restore ZCD */
	mp2762_enable_test_mode(chip, false);

	ret = regmap_write(chip->regmap, 0x06, 0x05);
	ret = regmap_update_bits(chip->regmap, MP2762_CONFIGURE_REG0, MP2762_NTC_GCOMP_SEL_BIT, 0);

	vote(chip->fv_votable, HW_LIMIT_VOTER, true, chip->platform_data.fv_ffc);
	vote(chip->vinmin_votable, CHARGER_TYPE_VOTER, true, chip->platform_data.vinmin);

	ret = regmap_update_bits(chip->regmap, MP2762_VPRECHG_OPTION_REG, MP2762_VPRECHG_OPTION_BIT, 0);
	if (ret)
		mps_err("I2C failed to set vprechg_option\n");

	ret = regmap_update_bits(chip->regmap, MP2762_FV_RECHG_VGAP_REG, MP2762_RECHG_VGAP_MASK, MP2762_RECHG_VGAP_MASK);
	if (ret)
		mps_err("I2C failed to set rechg_vgap\n");

	ret = mp2762_enable_terminate(chip, true);
	if (ret)
		mps_dbg("failed to enable terminate\n");

	ret = mp2762_set_iprechg(chip, chip->platform_data.iprechg);
	if (ret)
		mps_dbg("failed to set iprechg\n");

	ret = mp2762_set_vprechg(chip, chip->platform_data.vprechg);
	if (ret)
		mps_dbg("failed to set vprechg\n");

	mp2762_enable_otg(chip, false);

	return ret;
}

/* In VBUS wave test, VCC will drop and restore, cause device reset, so check it and init registers if reset happened */
static int mp2762_check_status(struct mp2762 *chip)
{
	int ret = 0;
	unsigned int data = 0;

	ret = regmap_read(chip->regmap, MP2762_FV_RECHG_VGAP_REG, &data);
	if (ret)
		mps_err("I2C failed to read FV_RECHG_VGAP_REG\n");

	if (data == 0x4E) {
		mps_info("device is reseted, init now\n");
		ret = mp2762_init_device(chip);
		rerun_election(chip->fv_votable);
		rerun_election(chip->vinmin_votable);
		rerun_election(chip->fcc_votable);
		rerun_election(chip->icl_votable);
		rerun_election(chip->input_suspend_votable);
	}

	return ret;
}

static irqreturn_t mp2762_interrupt(int irq, void *dev_id)
{
	struct mp2762 *chip = dev_id;

	return IRQ_HANDLED;
}

static int mp2762_fcc_vote_callback(struct votable *votable, void *data, int value, const char *client)
{
	struct mp2762 *chip = data;
	int ret = 0;

	if (value > MP2762_MAX_FCC)
		value = MP2762_MAX_FCC;

	mps_info("vote FCC = %d\n", value);
	ret = mp2762_set_fcc(chip, value);
	if (ret) {
		mps_err("failed to set FCC\n");
		return ret;
	}

	return ret;
}

static int mp2762_fv_vote_callback(struct votable *votable, void *data, int value, const char *client)
{
	struct mp2762 *chip = data;
	int ret = 0;

	mps_info("vote FV = %d\n", value);

	ret = mp2762_set_fv(chip, value);
	if (ret) {
		mps_err("failed to set FV\n");
		return ret;
	}

	return ret;
}

static int mp2762_icl_vote_callback(struct votable *votable, void *data, int value, const char *client)
{
	struct mp2762 *chip = data;
	int ret = 0;

	if (value > MP2762_MAX_ICL)
		value = MP2762_MAX_ICL;

	mps_info("vote ICL = %d\n", value);
	ret = mp2762_set_iinlim0(chip, value);
	if (ret) {
		mps_err("failed to set IINLIM0\n");
		return ret;
	}

	ret = mp2762_set_iinlim1(chip, value);
	if (ret) {
		mps_err("failed to set IINLIM1\n");
		return ret;
	}

	return ret;
}

static int mp2762_iterm_vote_callback(struct votable *votable, void *data, int value, const char *client)
{
	struct mp2762 *chip = data;
	int ret = 0;

	mps_info("vote ITERM = %d\n", value);

	ret = mp2762_set_iterm(chip, value);
	if (ret) {
		mps_err("failed to set ITERM\n");
		return ret;
	}

	return ret;
}

static int mp2762_vinmin_vote_callback(struct votable *votable, void *data, int value, const char *client)
{
	struct mp2762 *chip = data;
	int ret = 0;

	mps_info("vote VINMIN = %d\n", value);

	ret = mp2762_set_vinmin(chip, value);
	if (ret) {
		mps_err("failed to set VINMIN\n");
		return ret;
	}

	return ret;
}

static int mp2762_enable_vote_callback(struct votable *votable, void *data, int enable, const char *client)
{
	struct mp2762 *chip = data;
	int ret = 0;

	mps_info("vote MP2762_ENABLE = %d\n", enable);

	ret = mp2762_enable_charge(chip, enable);
	if (ret) {
		mps_err("failed to set MP2762_ENABLE\n");
		return ret;
	}

	return ret;
}

static int mp2762_input_suspend_vote_callback(struct votable *votable, void *data, int suspend, const char *client)
{
	struct mp2762 *chip = data;
	int ret = 0;

	if(chip->shutdown_flag == true)
	      return 0;

	mps_info("vote MP2762_SUSPEND = %d\n", suspend);

	ret = mp2762_suspend_input(chip, suspend);
	if (ret) {
		mps_err("failed to set MP2762_SUSPEND\n");
		return ret;
	}

	return ret;
}

static int mp2762_create_votable(struct mp2762 *chip)
{
	int rc = 0;

	chip->fcc_votable = create_votable("BBC_FCC", VOTE_MIN, mp2762_fcc_vote_callback, chip);
	if (IS_ERR(chip->fcc_votable)) {
		mps_err("failed to create voter BBC_FCC\n");
		return -1;
	}

	chip->fv_votable = create_votable("BBC_FV", VOTE_MIN, mp2762_fv_vote_callback, chip);
	if (IS_ERR(chip->fv_votable)) {
		mps_err("failed to create voter BBC_FV\n");
		return -1;
	}

	chip->icl_votable = create_votable("BBC_ICL", VOTE_MIN, mp2762_icl_vote_callback, chip);
	if (IS_ERR(chip->icl_votable)) {
		mps_err("failed to create voter BBC_ICL\n");
		return -1;
	}

	chip->iterm_votable = create_votable("BBC_ITERM", VOTE_MIN, mp2762_iterm_vote_callback, chip);
	if (IS_ERR(chip->iterm_votable)) {
		mps_err("failed to create voter BBC_ITERM\n");
		return -1;
	}

	chip->vinmin_votable = create_votable("BBC_VINMIN", VOTE_MIN, mp2762_vinmin_vote_callback, chip);
	if (IS_ERR(chip->vinmin_votable)) {
		mps_err("failed to create voter BBC_VINMIN\n");
		return -1;
	}

	chip->enable_votable = create_votable("BBC_ENABLE", VOTE_MIN, mp2762_enable_vote_callback, chip);
	if (IS_ERR(chip->enable_votable)) {
		mps_err("failed to create voter BBC_ENABLE\n");
		return -1;
	}

	chip->input_suspend_votable = create_votable("BBC_SUSPEND", VOTE_SET_ANY, mp2762_input_suspend_vote_callback, chip);
	if (IS_ERR(chip->input_suspend_votable)) {
		mps_err("failed to create voter BBC_SUSPEND\n");
		return -1;
	}

	return rc;
}

static int mp2762_init_irq(struct mp2762 *chip)
{
	int ret = 0;

	ret = devm_gpio_request(chip->dev, chip->platform_data.irq_gpio, dev_name(chip->dev));
	if (ret < 0) {
		mps_err("failed to request gpio\n");
		return ret;
	}

	chip->platform_data.irq = gpio_to_irq(chip->platform_data.irq_gpio);
	if (chip->platform_data.irq < 0) {
		mps_err("failed to get gpio_irq\n");
		return ret;
	}

	ret = request_irq(chip->platform_data.irq, mp2762_interrupt, IRQF_TRIGGER_FALLING | IRQF_ONESHOT, dev_name(chip->dev), chip);
	if (ret < 0) {
		mps_err("failed to request irq\n");
		return ret;
	}

	return ret;
}

static void mp2762_parse_cmdline(void)
{
	char *ruby = NULL, *rubypro = NULL, *rubyplus = NULL;
	const char *sku = get_hw_sku();

	ruby = strnstr(sku, "ruby", strlen(sku));
	rubypro = strnstr(sku, "rubypro", strlen(sku));
	rubyplus = strnstr(sku, "rubyplus", strlen(sku));

	if (rubyplus)
		product_name = RUBYPLUS;
	else if (rubypro)
		product_name = RUBYPRO;
	else if (ruby)
		product_name = RUBY;

	mps_info("product_name = %d, ruby = %d, rubypro = %d, rubyplus = %d\n", product_name, ruby ? 1 : 0, rubypro ? 1 : 0, rubyplus ? 1 : 0);
}

static int mp2762_charger_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct mp2762 *chip;
	int ret;

	mp2762_parse_cmdline();

	mps_info("MP2762 probe start\n");

	if (product_name == RUBYPLUS) {
		mps_info("MP2762 probe start\n");
	} else {
		mps_info("ruby and rubypro no need to probe MP2762\n");
		return -ENODEV;
	}

	chip = devm_kzalloc(&client->dev, sizeof(struct mp2762), GFP_KERNEL);
	if (!chip)
		return -ENOMEM;

	chip->dev = &client->dev;
	chip->client = client;
	strncpy(chip->model_name, id->name, I2C_NAME_SIZE);
        i2c_set_clientdata(client, chip);

	chip->regmap = devm_regmap_init_i2c(client, &mp2762_regmap_config);
	if (IS_ERR(chip->regmap)) {
		mps_err("failed to allocate register map\n");
		return PTR_ERR(chip->regmap);
	}

	ret = mp2762_parse_dt(chip);
	if (ret) {
		mps_err("faied to parse DTS\n");
		return ret;
	}

	ret = mp2762_init_irq(chip);
	if (ret) {
		mps_err("failed to init irq\n");
		return ret;
	}

	ret = mp2762_create_votable(chip);
	if (ret) {
		mps_err("failed to create voter\n");
		return ret;
	}

	ret = mp2762_init_psy(chip);
	if (ret) {
		mps_err("failed to init psy\n");
		return ret;
	}

	ret = mp2762_init_device(chip);
	if (ret) {
		mps_err("failed to init device\n");
		return ret;
	}

	chip->chg_dev = charger_device_register("bbc", chip->dev, chip, &mp2762_chg_ops, &mp2762_chg_props);
	if (!chip->chg_dev) {
		mps_err("failed to register charger device\n");
		return -1;
	}

	ret = sysfs_create_group(&chip->dev->kobj, &mp2762_attr_group);
	if (ret) {
		mps_err("failed to register sysfs\n");
		return ret;
	}

	chip->chip_ok = true;
	mps_info("MP2762 probe success\n");
	return ret;
}

static int mp2762_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct mp2762 *chip = i2c_get_clientdata(client);

	return enable_irq_wake(chip->platform_data.irq);
}

static int mp2762_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct mp2762 *chip = i2c_get_clientdata(client);

	return disable_irq_wake(chip->platform_data.irq);
}

static int mp2762_charger_remove(struct i2c_client *client)
{
	struct mp2762 *chip = i2c_get_clientdata(client);

	sysfs_remove_group(&chip->dev->kobj, &mp2762_attr_group);

	return 0;
}

static void mp2762_charger_shutdown(struct i2c_client *client)
{
	struct mp2762 *chip = i2c_get_clientdata(client);

	chip->shutdown_flag = true;
	regmap_update_bits(chip->regmap, MP2762_CONFIGURE_REG0, MP2762_ENABLE_OTG_BIT, 0);
	regmap_update_bits(chip->regmap, MP2762_CONFIGURE_REG0, MP2762_RESET_REGS_BIT, 1);
	regmap_update_bits(chip->regmap, MP2762_CONFIGURE_REG0, MP2762_SUSP_INPUT_BIT, 0);
	regmap_update_bits(chip->regmap, MP2762_CONFIGURE_REG0, MP2762_ENABLE_CHG_BIT, 1);
}

static const struct of_device_id mp2762_charger_match_table[] = {
	{.compatible = "mp2762",},
	{},
};
MODULE_DEVICE_TABLE(of, mp2762_charger_match_table);

static const struct i2c_device_id mp2762_charger_id[] = {
	{ "mp2762", 0 },
	{},
};
MODULE_DEVICE_TABLE(i2c, mp2762_charger_id);

static const struct dev_pm_ops mp2762_pm_ops = {
	.resume		= mp2762_resume,
	.suspend	= mp2762_suspend,
};

static struct i2c_driver mp2762_charger_driver = {
	.driver	= {
		.name	= "mp2762",
		.owner	= THIS_MODULE,
		.of_match_table = mp2762_charger_match_table,
		.pm		= &mp2762_pm_ops,
	},
	.id_table	= mp2762_charger_id,
	.probe		= mp2762_charger_probe,
	.remove		= mp2762_charger_remove,
	.shutdown	= mp2762_charger_shutdown,
};

module_i2c_driver(mp2762_charger_driver);

MODULE_DESCRIPTION("MP2762 Charger Driver");
MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Chenyichun");
