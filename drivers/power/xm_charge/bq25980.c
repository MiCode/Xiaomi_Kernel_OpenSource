// SPDX-License-Identifier: GPL-2.0
// BQ25980 Battery Charger Driver
// Copyright (C) 2020 Texas Instruments Incorporated - http://www.ti.com/

#include <linux/err.h>
#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/gpio/consumer.h>
#include <linux/regmap.h>
#include <linux/types.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/moduleparam.h>
#include <linux/slab.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/string.h>

#include "xmc_core.h"
#include "bq25980.h"

enum product_name {
	PISSARRO,
	PISSARROPRO,
};

static int log_level = 2;
static int product_name = PISSARRO;

#define bq_err(fmt, ...)					\
do {								\
	if (log_level >= 0)					\
			printk(KERN_ERR "" fmt, ##__VA_ARGS__);	\
} while (0)

#define bq_info(fmt, ...)					\
do {								\
	if (log_level >= 1)					\
			printk(KERN_ERR "" fmt, ##__VA_ARGS__);	\
} while (0)

#define bq_dbg(fmt, ...)					\
do {								\
	if (log_level >= 2)					\
			printk(KERN_ERR "" fmt, ##__VA_ARGS__);	\
} while (0)

struct bq25980_state {
	bool dischg;
	bool ovp;
	bool ocp;
	bool wdt;
	bool tflt;
	bool online;
	bool ce;
	bool hiz;
	bool bypass;
};

enum bq_work_mode {
	BQ_STANDALONE,
	BQ_SLAVE,
	BQ_MASTER,
};

enum bq_device_id {
	BQ25980 = 0,
	SC8571 = 8,
};

enum {
	BQ25968,
	TI25980,
	SOUTH8571,
};


enum bq_compatible_id {
	BQ25960_STANDALONE,
	BQ25960_SLAVE,
	BQ25960_MASTER,

	BQ25980_STANDALONE,
	BQ25980_SLAVE,
	BQ25980_MASTER,

	BQ25975_STANDALONE,
};

enum {
	VBUS_ERROR_NONE,
	VBUS_ERROR_LOW,
	VBUS_ERROR_HIGHT,
};

struct bq25980_chip_info {

	int model_id;

	const struct regmap_config *regmap_config;

	const struct reg_default *reg_init_values;

	int busocp_sc_def;
	int busocp_byp_def;
	int busocp_sc_max;
	int busocp_byp_max;
	int busocp_sc_min;
	int busocp_byp_min;
	int busocp_step;
	int busocp_offset;

	int busovp_sc_def;
	int busovp_byp_def;
	int busovp_sc_step;

	int busovp_sc_offset;
	int busovp_byp_step;
	int busovp_byp_offset;
	int busovp_sc_min;
	int busovp_sc_max;
	int busovp_byp_min;
	int busovp_byp_max;

	int batovp_def;
	int batovp_max;
	int batovp_min;
	int batovp_step;
	int batovp_offset;

	int batocp_def;
	int batocp_max;

	int vac_sc_ovp;
	int vac_byp_ovp;

	int adc_curr_step;
	int adc_vbat_volt_step;
	int adc_vbus_volt_step;
	int adc_vbus_volt_offset;
	int adc_tchg_step;
};

struct bq25980_device {
	struct i2c_client *client;
	struct device *dev;
	struct pinctrl *bq_pinctrl;
	struct pinctrl_state *sda_normal;
	struct pinctrl_state *sda_gpio;
	struct charger_device *chg_dev;
	struct power_supply *charger;
	struct mutex state_lock;
	struct regmap *regmap;
	struct mutex i2c_rw_lock;
	bool chip_ok;

	char model_name[I2C_NAME_SIZE];
	char log_tag[25];

	const struct bq25980_chip_info *chip_info;
	struct bq25980_state state;
	int watchdog_timer;
	int mode;
	int device_id;
	int chip_vendor;
	int part_no;
	struct power_supply_desc psy_desc;
	struct mutex data_lock;

	struct delayed_work irq_handle_work;
	int irq_gpio;
	int irq;
};

static struct reg_default bq25980_reg_init_val[] = {
	{BQ25980_BATOVP,	0x69}, /* CP sense battery package, so set BATOVP to 9100mV */
	{BQ25980_BATOVP_ALM,	0x64}, /* CP sense battery package, so set BATOVP to 9000mV */
	{BQ25980_BATOCP,	0xFF}, /* disable */
	{BQ25980_BATOCP_ALM,	0xFF}, /* disable */
	{BQ25980_CHRGR_CFG_1,	0xA8},
	{BQ25980_CHRGR_CTRL_1,	0x4A},
	{BQ25980_BUSOVP,	0x46}, /* 21000mV */
	{BQ25980_BUSOVP_ALM,	0x3C}, /* 20000mV */
	{BQ25980_BUSOCP,	0x0C},//0X0c:4200mA
	{BQ25980_REG_09,	0x8C},
	{BQ25980_TEMP_CONTROL,	0x5C},
	{BQ25980_TDIE_ALM,	0x78},//0x78:85C
	{BQ25980_TSBUS_FLT,	0x15},
	{BQ25980_TSBAT_FLG,	0x15},
	{BQ25980_VAC_CONTROL,	0x00},//0xD8:6.5V
	{BQ25980_CHRGR_CTRL_2,	0x00},
	{BQ25980_CHRGR_CTRL_3,	0x74},//0x74:watchdog disable 5s,275kHz
	{BQ25980_CHRGR_CTRL_4,	0x61},
	{BQ25980_CHRGR_CTRL_5,	0x00},
	{BQ25980_MASK1,		0x00},
	{BQ25980_MASK2,		0x00},
	{BQ25980_MASK3,		0x20},
	{BQ25980_MASK4,		0x00},
	{BQ25980_MASK5,		0x80},
	{BQ25980_ADC_CONTROL1,	0x85},//sample 14 bit
	{BQ25980_ADC_CONTROL2,	0xFE},
};

static struct reg_default sc8571_reg_init_val[] = {
	{BQ25980_BATOVP,	0x69}, /* CP sense battery package, so set BATOVP to 9100mV */
	{BQ25980_BATOVP_ALM,	0x64}, /* CP sense battery package, so set BATOVP to 9000mV */
	{BQ25980_BATOCP,	0xFF}, /* disable */
	{BQ25980_BATOCP_ALM,	0xFF}, /* disable */
	{BQ25980_CHRGR_CFG_1,	0xA8},
	{BQ25980_CHRGR_CTRL_1,	0x4A},
	{BQ25980_BUSOVP,	0x46}, /* 21000mV */
	{BQ25980_BUSOVP_ALM,	0x3C}, /* 20000mV */
	{BQ25980_BUSOCP,	0x0C},//0X0c:4200mA
	{BQ25980_REG_09,	0x8C},
	{BQ25980_TEMP_CONTROL,	0x5C},
	{BQ25980_TDIE_ALM,	0x78},//0x78:85C
	{BQ25980_TSBUS_FLT,	0x15},
	{BQ25980_TSBAT_FLG,	0x15},
	{BQ25980_VAC_CONTROL,	0x00},//0xD8:6.5V
	{BQ25980_CHRGR_CTRL_2,	0x00},
	{BQ25980_CHRGR_CTRL_3,	0x34},//0x74:watchdog disable 5s,275kHz
	{BQ25980_CHRGR_CTRL_4,	0x61},
	{BQ25980_CHRGR_CTRL_5,	0x00},
	{BQ25980_MASK1,		0x00},
	{BQ25980_MASK2,		0x00},
	{BQ25980_MASK3,		0x20},
	{BQ25980_MASK4,		0x00},
	{BQ25980_MASK5,		0x80},
	{BQ25980_ADC_CONTROL1,	0x81},//sample 14 bit
	{BQ25980_ADC_CONTROL2,	0xFE},


};

static struct reg_default bq25980_reg_defs[] = {
	{BQ25980_BATOVP, 0x5A},
	{BQ25980_BATOVP_ALM, 0x46},
	{BQ25980_BATOCP, 0x51},
	{BQ25980_BATOCP_ALM, 0x50},
	{BQ25980_CHRGR_CFG_1, 0x28},
	{BQ25980_CHRGR_CTRL_1, 0x0},
	{BQ25980_BUSOVP, 0x26},
	{BQ25980_BUSOVP_ALM, 0x22},
	{BQ25980_BUSOCP, 0xD},
	{BQ25980_REG_09, 0xC},
	{BQ25980_TEMP_CONTROL, 0x30},
	{BQ25980_TDIE_ALM, 0xC8},
	{BQ25980_TSBUS_FLT, 0x15},
	{BQ25980_TSBAT_FLG, 0x15},
	{BQ25980_VAC_CONTROL, 0x0},
	{BQ25980_CHRGR_CTRL_2, 0x0},
	{BQ25980_CHRGR_CTRL_3, 0x20},
	{BQ25980_CHRGR_CTRL_4, 0x1D},
	{BQ25980_CHRGR_CTRL_5, 0x18},
	{BQ25980_STAT1, 0x0},
	{BQ25980_STAT2, 0x0},
	{BQ25980_STAT3, 0x0},
	{BQ25980_STAT4, 0x0},
	{BQ25980_STAT5, 0x0},
	{BQ25980_FLAG1, 0x0},
	{BQ25980_FLAG2, 0x0},
	{BQ25980_FLAG3, 0x0},
	{BQ25980_FLAG4, 0x0},
	{BQ25980_FLAG5, 0x0},
	{BQ25980_MASK1, 0x0},
	{BQ25980_MASK2, 0x0},
	{BQ25980_MASK3, 0x0},
	{BQ25980_MASK4, 0x0},
	{BQ25980_MASK5, 0x0},
	{BQ25980_DEVICE_INFO, 0x8},
	{BQ25980_ADC_CONTROL1, 0x0},
	{BQ25980_ADC_CONTROL2, 0x0},
	{BQ25980_IBUS_ADC_LSB, 0x0},
	{BQ25980_IBUS_ADC_MSB, 0x0},
	{BQ25980_VBUS_ADC_LSB, 0x0},
	{BQ25980_VBUS_ADC_MSB, 0x0},
	{BQ25980_VAC1_ADC_LSB, 0x0},
	{BQ25980_VAC2_ADC_LSB, 0x0},
	{BQ25980_VOUT_ADC_LSB, 0x0},
	{BQ25980_VBAT_ADC_LSB, 0x0},
	{BQ25980_IBAT_ADC_MSB, 0x0},
	{BQ25980_IBAT_ADC_LSB, 0x0},
	{BQ25980_TSBUS_ADC_LSB, 0x0},
	{BQ25980_TSBAT_ADC_LSB, 0x0},
	{BQ25980_TDIE_ADC_LSB, 0x0},
	{BQ25980_DEGLITCH_TIME, 0x0},
	{BQ25980_CHRGR_CTRL_6, 0x0},
};

static struct reg_default bq25975_reg_defs[] = {
	{BQ25980_BATOVP, 0x5A},
	{BQ25980_BATOVP_ALM, 0x46},
	{BQ25980_BATOCP, 0x51},
	{BQ25980_BATOCP_ALM, 0x50},
	{BQ25980_CHRGR_CFG_1, 0x28},
	{BQ25980_CHRGR_CTRL_1, 0x0},
	{BQ25980_BUSOVP, 0x26},
	{BQ25980_BUSOVP_ALM, 0x22},
	{BQ25980_BUSOCP, 0xD},
	{BQ25980_REG_09, 0xC},
	{BQ25980_TEMP_CONTROL, 0x30},
	{BQ25980_TDIE_ALM, 0xC8},
	{BQ25980_TSBUS_FLT, 0x15},
	{BQ25980_TSBAT_FLG, 0x15},
	{BQ25980_VAC_CONTROL, 0x0},
	{BQ25980_CHRGR_CTRL_2, 0x0},
	{BQ25980_CHRGR_CTRL_3, 0x20},
	{BQ25980_CHRGR_CTRL_4, 0x1D},
	{BQ25980_CHRGR_CTRL_5, 0x18},
	{BQ25980_STAT1, 0x0},
	{BQ25980_STAT2, 0x0},
	{BQ25980_STAT3, 0x0},
	{BQ25980_STAT4, 0x0},
	{BQ25980_STAT5, 0x0},
	{BQ25980_FLAG1, 0x0},
	{BQ25980_FLAG2, 0x0},
	{BQ25980_FLAG3, 0x0},
	{BQ25980_FLAG4, 0x0},
	{BQ25980_FLAG5, 0x0},
	{BQ25980_MASK1, 0x0},
	{BQ25980_MASK2, 0x0},
	{BQ25980_MASK3, 0x0},
	{BQ25980_MASK4, 0x0},
	{BQ25980_MASK5, 0x0},
	{BQ25980_DEVICE_INFO, 0x8},
	{BQ25980_ADC_CONTROL1, 0x0},
	{BQ25980_ADC_CONTROL2, 0x0},
	{BQ25980_IBUS_ADC_LSB, 0x0},
	{BQ25980_IBUS_ADC_MSB, 0x0},
	{BQ25980_VBUS_ADC_LSB, 0x0},
	{BQ25980_VBUS_ADC_MSB, 0x0},
	{BQ25980_VAC1_ADC_LSB, 0x0},
	{BQ25980_VAC2_ADC_LSB, 0x0},
	{BQ25980_VOUT_ADC_LSB, 0x0},
	{BQ25980_VBAT_ADC_LSB, 0x0},
	{BQ25980_IBAT_ADC_MSB, 0x0},
	{BQ25980_IBAT_ADC_LSB, 0x0},
	{BQ25980_TSBUS_ADC_LSB, 0x0},
	{BQ25980_TSBAT_ADC_LSB, 0x0},
	{BQ25980_TDIE_ADC_LSB, 0x0},
	{BQ25980_DEGLITCH_TIME, 0x0},
	{BQ25980_CHRGR_CTRL_6, 0x0},
};

static struct reg_default bq25960_reg_defs[] = {
	{BQ25980_BATOVP, 0x5A},
	{BQ25980_BATOVP_ALM, 0x46},
	{BQ25980_BATOCP, 0x51},
	{BQ25980_BATOCP_ALM, 0x50},
	{BQ25980_CHRGR_CFG_1, 0x28},
	{BQ25980_CHRGR_CTRL_1, 0x0},
	{BQ25980_BUSOVP, 0x26},
	{BQ25980_BUSOVP_ALM, 0x22},
	{BQ25980_BUSOCP, 0xD},
	{BQ25980_REG_09, 0xC},
	{BQ25980_TEMP_CONTROL, 0x30},
	{BQ25980_TDIE_ALM, 0xC8},
	{BQ25980_TSBUS_FLT, 0x15},
	{BQ25980_TSBAT_FLG, 0x15},
	{BQ25980_VAC_CONTROL, 0x0},
	{BQ25980_CHRGR_CTRL_2, 0x0},
	{BQ25980_CHRGR_CTRL_3, 0x20},
	{BQ25980_CHRGR_CTRL_4, 0x1D},
	{BQ25980_CHRGR_CTRL_5, 0x18},
	{BQ25980_STAT1, 0x0},
	{BQ25980_STAT2, 0x0},
	{BQ25980_STAT3, 0x0},
	{BQ25980_STAT4, 0x0},
	{BQ25980_STAT5, 0x0},
	{BQ25980_FLAG1, 0x0},
	{BQ25980_FLAG2, 0x0},
	{BQ25980_FLAG3, 0x0},
	{BQ25980_FLAG4, 0x0},
	{BQ25980_FLAG5, 0x0},
	{BQ25980_MASK1, 0x0},
	{BQ25980_MASK2, 0x0},
	{BQ25980_MASK3, 0x0},
	{BQ25980_MASK4, 0x0},
	{BQ25980_MASK5, 0x0},
	{BQ25980_DEVICE_INFO, 0x8},
	{BQ25980_ADC_CONTROL1, 0x0},
	{BQ25980_ADC_CONTROL2, 0x0},
	{BQ25980_IBUS_ADC_LSB, 0x0},
	{BQ25980_IBUS_ADC_MSB, 0x0},
	{BQ25980_VBUS_ADC_LSB, 0x0},
	{BQ25980_VBUS_ADC_MSB, 0x0},
	{BQ25980_VAC1_ADC_LSB, 0x0},
	{BQ25980_VAC2_ADC_LSB, 0x0},
	{BQ25980_VOUT_ADC_LSB, 0x0},
	{BQ25980_VBAT_ADC_LSB, 0x0},
	{BQ25980_IBAT_ADC_MSB, 0x0},
	{BQ25980_IBAT_ADC_LSB, 0x0},
	{BQ25980_TSBUS_ADC_LSB, 0x0},
	{BQ25980_TSBAT_ADC_LSB, 0x0},
	{BQ25980_TDIE_ADC_LSB, 0x0},
	{BQ25980_DEGLITCH_TIME, 0x0},
	{BQ25980_CHRGR_CTRL_6, 0x0},
};

static void dump_reg(struct bq25980_device *bq, int start, int end)
{
	int ret;
	unsigned int val;
	int addr;

	for (addr = start; addr <= end; addr++) {
		ret = regmap_read(bq->regmap, addr, &val);
		if (!ret)
			bq_err("%s Reg[%02X] = 0x%02X\n", bq->log_tag, addr, val);
	}
}

static int bq25980_enable_adc(struct bq25980_device *bq, bool enable)
{
	int ret = 0;

	ret = regmap_update_bits(bq->regmap, BQ25980_ADC_CONTROL1, BQ25980_ADC_EN, enable ? BQ25980_ADC_EN : 0);

	return ret;
}

static int bq25980_enable_hiz(struct bq25980_device *bq, bool enable)
{
	int ret = 0;

	ret = regmap_update_bits(bq->regmap, BQ25980_CHRGR_CTRL_2, BQ25980_EN_HIZ, enable ? BQ25980_EN_HIZ : 0);

	return ret;
}

static int bq25980_set_bus_ocp(struct bq25980_device *bq, int busocp)
{
	unsigned int busocp_reg_code;

	if (!busocp)
		return bq25980_enable_hiz(bq, true);

	bq25980_enable_hiz(bq, false);

	if (bq->state.bypass) {
		busocp = max(busocp, bq->chip_info->busocp_byp_min);
		busocp = min(busocp, bq->chip_info->busocp_byp_max);
	} else {
		busocp = max(busocp, bq->chip_info->busocp_sc_min);
		busocp = min(busocp, bq->chip_info->busocp_sc_max);
	}

	busocp_reg_code = (busocp - bq->chip_info->busocp_offset) / bq->chip_info->busocp_step;

	return regmap_write(bq->regmap, BQ25980_BUSOCP, busocp_reg_code);
}

static int bq25980_set_bus_ovp(struct bq25980_device *bq, int busovp)
{
	unsigned int busovp_reg_code;
	unsigned int busovp_step;
	unsigned int busovp_offset;

	if (bq->state.bypass) {
		busovp_step = bq->chip_info->busovp_byp_step;
		busovp_offset = bq->chip_info->busovp_byp_offset;
		if (busovp > bq->chip_info->busovp_byp_max)
			busovp = bq->chip_info->busovp_byp_max;
		else if (busovp < bq->chip_info->busovp_byp_min)
			busovp = bq->chip_info->busovp_byp_min;
	} else {
		busovp_step = bq->chip_info->busovp_sc_step;
		busovp_offset = bq->chip_info->busovp_sc_offset;
		if (busovp > bq->chip_info->busovp_sc_max)
			busovp = bq->chip_info->busovp_sc_max;
		else if (busovp < bq->chip_info->busovp_sc_min)
			busovp = bq->chip_info->busovp_sc_min;
	}

	busovp_reg_code = (busovp - busovp_offset) / busovp_step;

	return regmap_write(bq->regmap, BQ25980_BUSOVP, busovp_reg_code);
}

static int bq25980_set_bus_ovp_alarm(struct bq25980_device *bq, int busovp)
{
	unsigned int busovp_reg_code;
	unsigned int busovp_step;
	unsigned int busovp_offset;

	if (bq->state.bypass) {
		busovp_step = bq->chip_info->busovp_byp_step;
		busovp_offset = bq->chip_info->busovp_byp_offset;
		if (busovp > bq->chip_info->busovp_byp_max)
			busovp = bq->chip_info->busovp_byp_max;
		else if (busovp < bq->chip_info->busovp_byp_min)
			busovp = bq->chip_info->busovp_byp_min;
	} else {
		busovp_step = bq->chip_info->busovp_sc_step;
		busovp_offset = bq->chip_info->busovp_sc_offset;
		if (busovp > bq->chip_info->busovp_sc_max)
			busovp = bq->chip_info->busovp_sc_max;
		else if (busovp < bq->chip_info->busovp_sc_min)
			busovp = bq->chip_info->busovp_sc_min;
	}

	busovp_reg_code = (busovp - busovp_offset) / busovp_step;

	return regmap_write(bq->regmap, BQ25980_BUSOVP_ALM, busovp_reg_code);
}

static int bq25980_set_ac_ovp(struct bq25980_device *bq, int ac_ovp)
{
	int ret = 0;
	u8 data = 0;

	bq_info("%s set ac_ovp = %d\n", bq->log_tag, ac_ovp);

	if (ac_ovp == 6500)
		data = BQ25980_AC_OVP_6500MV;
	else if (ac_ovp == 10500)
		data = BQ25980_AC_OVP_10500MV;
	else if (ac_ovp == 12000)
		data = BQ25980_AC_OVP_12000MV;
	else if (ac_ovp == 14000)
		data = BQ25980_AC_OVP_14000MV;
	else if (ac_ovp == 16000)
		data = BQ25980_AC_OVP_16000MV;
	else if (ac_ovp == 18000)
		data = BQ25980_AC_OVP_18000MV;
	else if (ac_ovp == 22000)
		data = BQ25980_AC_OVP_22000MV;
	else if (ac_ovp == 24000)
		data = BQ25980_AC_OVP_24000MV;
	else
		bq_err("%s not support ac_ovp %d\n", bq->log_tag, ac_ovp);

	data = data << BQ25980_AC_OVP_SHIFT;

	ret =  regmap_update_bits(bq->regmap, BQ25980_VAC_CONTROL, BQ25980_AC_OVP_MASK, data);
	if (ret)
		bq_err("%s I2C failed to set AC_OVP\n", bq->log_tag);
}

static int bq25980_enable_bypass(struct bq25980_device *bq, bool enable)
{
	int ret = 0;

	bq_info("%s [ENABLE_BYPASS] %d\n", bq->log_tag, enable);

	ret = regmap_update_bits(bq->regmap, BQ25980_CHRGR_CTRL_2, BQ25980_EN_BYPASS, enable ? BQ25980_EN_BYPASS : 0);

	bq->state.bypass = enable;

	if (enable) {
		regmap_write(bq->regmap, BQ25980_BUSOCP, 0x1A);
		bq25980_set_ac_ovp(bq, 12000);
	} else {
		regmap_write(bq->regmap, BQ25980_BUSOCP, 0x0C);
		bq25980_set_ac_ovp(bq, 22000);
	}

	return ret;
}

static int bq25980_get_bypass_enable(struct bq25980_device *bq, bool *enable)
{
	int ret = 0;
	unsigned int data = 0;

	ret = regmap_read(bq->regmap, BQ25980_CHRGR_CTRL_2, &data);

	*enable = !!(data & BQ25980_EN_BYPASS);
	bq->state.bypass = *enable;

	return ret;
}

static int bq25980_enable_charge(struct bq25980_device *bq, bool enable)
{
	int ret = 0;

	bq_info("%s [ENABLE_CHARGE_PUMP] %d\n", bq->log_tag, enable);

	ret = regmap_update_bits(bq->regmap, BQ25980_CHRGR_CTRL_2, BQ25980_CHG_EN, enable ? BQ25980_CHG_EN : 0);

	bq->state.ce = enable;

	return ret;
}

static int bq25980_get_charge_enable(struct bq25980_device *bq, bool *enable)
{
	int ret = 0;
	unsigned int data = 0;

	ret = regmap_read(bq->regmap, BQ25980_CHRGR_CTRL_2, &data);

	*enable = !!(data & BQ25980_CHG_EN);
	bq->state.ce = *enable;

	return ret;
}

static int bq25980_get_adc_ibus(struct bq25980_device *bq)
{
	unsigned int ibus_adc_lsb, ibus_adc_msb;
	u16 ibus_adc;
	int ret;

	ret = regmap_read(bq->regmap, BQ25980_IBUS_ADC_MSB, &ibus_adc_msb);
	if (ret)
		return ret;

	ret = regmap_read(bq->regmap, BQ25980_IBUS_ADC_LSB, &ibus_adc_lsb);
	if (ret)
		return ret;

	ibus_adc = (ibus_adc_msb << 8) | ibus_adc_lsb;

	if (ibus_adc_msb & BQ25980_ADC_POLARITY_BIT)
		return ((ibus_adc ^ 0xffff) + 1) * bq->chip_info->adc_curr_step;

	return ibus_adc * bq->chip_info->adc_curr_step / 1000;
}

static int bq25980_get_adc_tchg(struct bq25980_device *bq)
{
	unsigned int tchg_adc_lsb, tchg_adc_msb;
	u16 abs_value;
	int result, ret;

	ret = regmap_read(bq->regmap, BQ25980_TDIE_ADC_MSB, &tchg_adc_msb);
	if (ret)
		return ret;

	ret = regmap_read(bq->regmap, BQ25980_TDIE_ADC_LSB, &tchg_adc_lsb);
	if (ret)
		return ret;

	abs_value = ((tchg_adc_msb & 0x7F) << 8) | tchg_adc_lsb;
	result = abs_value * bq->chip_info->adc_tchg_step;

	if (tchg_adc_msb & BQ25980_ADC_POLARITY_BIT)
		return -result;
	else
		return result;
}

static int bq25980_enable_otg(struct bq25980_device *bq, bool enable)
{
	unsigned int data = 0;
	int retry_count = 5, ret = 0;

	if (bq->chip_vendor == SOUTH8571)
		return ret;

	if (enable) {
		regmap_update_bits(bq->regmap, BQ25980_CHRGR_CTRL_3, BQ25980_WATCHDOG_DIS, BQ25980_WATCHDOG_DIS);
		regmap_update_bits(bq->regmap, BQ25980_CHRGR_CTRL_2, BQ25980_EN_OTG, BQ25980_EN_OTG);
		while (retry_count) {
			msleep(50);
			retry_count--;
			regmap_read(bq->regmap, BQ25980_CHRGR_CTRL_2, &data);
			if (data & BQ25980_DIS_ACDRV_BOTH)
				break;
		}
		regmap_update_bits(bq->regmap, BQ25980_CHRGR_CTRL_2, BQ25980_DIS_ACDRV_BOTH, 0);
		regmap_update_bits(bq->regmap, BQ25980_CHRGR_CTRL_2, BQ25980_ACDRV1_EN, BQ25980_ACDRV1_EN);
	} else {
		regmap_update_bits(bq->regmap, BQ25980_CHRGR_CTRL_2, BQ25980_ACDRV1_EN, 0);
		regmap_update_bits(bq->regmap, BQ25980_CHRGR_CTRL_2, BQ25980_DIS_ACDRV_BOTH, 0);
		regmap_update_bits(bq->regmap, BQ25980_CHRGR_CTRL_2, BQ25980_EN_OTG, 0);
	}

	ret = regmap_read(bq->regmap, BQ25980_CHRGR_CTRL_2, &data);
	bq_info("%s enable OTG, enable = %d, data = 0x%02x\n", bq->log_tag, enable, data);

	return ret;
}

static int ops_bq25980_enable_charge(struct charger_device *chg_dev, bool enable)
{
	struct bq25980_device *bq = charger_get_data(chg_dev);
	int ret = 0;

	ret = bq25980_enable_charge(bq, enable);

	return ret;
}

static int ops_bq25980_get_charge_enable(struct charger_device *chg_dev, bool *enabled)
{
	struct bq25980_device *bq = charger_get_data(chg_dev);
	int ret = 0;

	ret = bq25980_get_charge_enable(bq, enabled);

	return ret;
}

static int ops_bq25980_enable_bypass(struct charger_device *chg_dev, bool enable)
{
	struct bq25980_device *bq = charger_get_data(chg_dev);
	int ret = 0;

	ret = bq25980_enable_bypass(bq, enable);

	return ret;
}

static int ops_bq25980_is_bypass_enabled(struct charger_device *chg_dev, bool *enabled)
{
	struct bq25980_device *bq = charger_get_data(chg_dev);
	int ret = 0;

	ret = bq25980_get_bypass_enable(bq, enabled);

	return ret;
}

static int ops_bq25980_get_ibus(struct charger_device *chg_dev, u32 *ibus_curr)
{
	struct bq25980_device *bq = charger_get_data(chg_dev);

	*ibus_curr = bq25980_get_adc_ibus(bq);

	return 0;
}

static int ops_bq25980_get_tchg(struct charger_device *chg_dev, int *value_min, int *value_max)
{
	struct bq25980_device *bq = charger_get_data(chg_dev);

	*value_min = bq25980_get_adc_tchg(bq) / 10;
	*value_max = *value_min;

	return 0;
}

static int ops_bq25980_set_ac_ovp(struct charger_device *chg_dev, int value)
{
	struct bq25980_device *bq = charger_get_data(chg_dev);
	int ret = 0;

	ret = bq25980_set_ac_ovp(bq, value);
	if (ret)
		bq_err("%s failed to set AC_OVP\n", bq->log_tag);

	return ret;
}

static int ops_bq25980_enable_otg(struct charger_device *chg_dev, bool enable)
{
	struct bq25980_device *bq = charger_get_data(chg_dev);

	return bq25980_enable_otg(bq, enable);
}

static const struct charger_ops bq25980_chg_ops = {
	.enable = ops_bq25980_enable_charge,
	.is_enabled = ops_bq25980_get_charge_enable,
	.enable_otg = ops_bq25980_enable_otg,
	.enable_bypass = ops_bq25980_enable_bypass,
	.is_bypass_enabled = ops_bq25980_is_bypass_enabled,
	.get_ibus_adc = ops_bq25980_get_ibus,
	.get_tchg_adc = ops_bq25980_get_tchg,
	.set_ac_ovp = ops_bq25980_set_ac_ovp,
};

static const struct charger_properties bq25980_standalone_chg_props = {
	.alias_name = "cp_standalone",
};

static const struct charger_properties bq25980_master_chg_props = {
	.alias_name = "cp_master",
};

static const struct charger_properties bq25980_slave_chg_props = {
	.alias_name = "cp_slave",
};

static enum power_supply_property bq25980_power_supply_props[] = {
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_CHIP_OK,
	POWER_SUPPLY_PROP_MANUFACTURER,
	POWER_SUPPLY_PROP_MODEL_NAME,
	POWER_SUPPLY_PROP_CHARGE_ENABLED,
	POWER_SUPPLY_PROP_BYPASS,
	POWER_SUPPLY_PROP_BYPASS_SUPPORT,
	POWER_SUPPLY_PROP_CURRENT_NOW,
};

static int bq25980_property_is_writeable(struct power_supply *psy, enum power_supply_property prop)
{
	switch (prop) {
	case POWER_SUPPLY_PROP_CHARGE_ENABLED:
	case POWER_SUPPLY_PROP_BYPASS:
		return true;
	default:
		return false;
	}
}

static int bq25980_set_property(struct power_supply *psy, enum power_supply_property prop, const union power_supply_propval *val)
{
	struct bq25980_device *bq = power_supply_get_drvdata(psy);
	int ret = -EINVAL;

	switch (prop) {
	case POWER_SUPPLY_PROP_CHARGE_ENABLED:
		ret = bq25980_enable_charge(bq, val->intval);
		if (ret)
			return ret;
		break;
	case POWER_SUPPLY_PROP_BYPASS:
		ret = bq25980_enable_bypass(bq, val->intval);
		if (ret)
			return ret;
		if (val->intval)
			ret = bq25980_set_bus_ocp(bq, bq->chip_info->busocp_byp_def);
		else
			ret = bq25980_set_bus_ocp(bq, bq->chip_info->busocp_sc_def);
		if (ret)
			return ret;

		break;

	default:
		return -EINVAL;
	}

	return 0;
}

static int bq25980_get_property(struct power_supply *psy, enum power_supply_property psp, union power_supply_propval *val)
{
	struct bq25980_device *bq = power_supply_get_drvdata(psy);
	bool enable = false;
	int ret = 0;

	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
	case POWER_SUPPLY_PROP_CHIP_OK:
		val->intval = bq->chip_ok;
		break;
	case POWER_SUPPLY_PROP_MANUFACTURER:
		val->strval = BQ25980_MANUFACTURER;
		break;
	case POWER_SUPPLY_PROP_MODEL_NAME:
		val->strval = bq->model_name;
		break;
	case POWER_SUPPLY_PROP_CHARGE_ENABLED:
		val->intval = bq->state.ce;
		break;
	case POWER_SUPPLY_PROP_BYPASS:
		val->intval = bq->state.bypass;
		break;
	case POWER_SUPPLY_PROP_BYPASS_SUPPORT:
		val->intval = 1;
		break;
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		val->intval = bq25980_get_adc_ibus(bq);
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int bq25980_init_device(struct bq25980_device *bq)
{
	int i = 0, ret = 0;

	for (i = 0; i < ARRAY_SIZE(bq25980_reg_init_val); i++) {
		ret = regmap_update_bits(bq->regmap, bq->chip_info->reg_init_values[i].reg, 0xFF, bq->chip_info->reg_init_values[i].def);
		bq_err("%s init Reg[%02X] = 0x%02X\n", bq->log_tag, bq->chip_info->reg_init_values[i].reg, bq->chip_info->reg_init_values[i].def);
		if (ret)
			return ret;
	}

	ret = bq25980_enable_adc(bq, true);
	if (ret)
		bq_err("%s failed to enable ADC\n", bq->log_tag);

	return ret;
}

static void bq25980_irq_handler(struct work_struct *work)
{
	struct bq25980_device *bq = container_of(work, struct bq25980_device, irq_handle_work.work);
	unsigned int data = 0;
	u8 flag = 0;
	int ret = 0;

	dump_reg(bq, 0x00, 0x43);

	if(bq->chip_vendor == SOUTH8571){
		ret = regmap_read(bq->regmap, SC8571_INTERNAL_1, &data);
		if(ret < 0){
			bq_err("%s bq25980_interrupt regmap read err and return:%d\n", bq->log_tag,ret);
			return;
		}
		flag = !!(data & SC8571_POR_FLAG);
		bq_err("%s bq25980_interrupt with por flag:%d\n", bq->log_tag,flag);
		if(flag)
			bq25980_init_device(bq);
	}

	return;
}

static irqreturn_t bq25980_interrupt(int irq, void *private)
{
	struct bq25980_device *bq = private;

	bq_info("%s bq25980_interrupt\n", bq->log_tag);

	schedule_delayed_work(&bq->irq_handle_work, 0);

	return IRQ_HANDLED;
}

static bool bq25980_is_volatile_reg(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case BQ25980_CHRGR_CTRL_2:
	case BQ25980_STAT1...BQ25980_FLAG5:
	case BQ25980_ADC_CONTROL1...BQ25980_TDIE_ADC_LSB:
		return true;
	default:
		return false;
	}
}

static const struct regmap_config bq25980_regmap_config = {
	.reg_bits = 8,
	.val_bits = 8,

	.max_register = BQ25980_CHRGR_CTRL_6,
	.reg_defaults	= bq25980_reg_defs,
	.num_reg_defaults = ARRAY_SIZE(bq25980_reg_defs),
	.cache_type = REGCACHE_NONE,
	.volatile_reg = bq25980_is_volatile_reg,
};

static const struct regmap_config bq25975_regmap_config = {
	.reg_bits = 8,
	.val_bits = 8,

	.max_register = BQ25980_CHRGR_CTRL_6,
	.reg_defaults	= bq25975_reg_defs,
	.num_reg_defaults = ARRAY_SIZE(bq25975_reg_defs),
	.cache_type = REGCACHE_NONE,
	.volatile_reg = bq25980_is_volatile_reg,
};

static const struct regmap_config sc8571_regmap_config = {
	.reg_bits = 8,
	.val_bits = 8,

	.max_register = 0x43,
	.reg_defaults	= bq25980_reg_defs,
	.num_reg_defaults = ARRAY_SIZE(bq25980_reg_defs),
	.cache_type = REGCACHE_NONE,
	.volatile_reg = bq25980_is_volatile_reg,
};

static const struct bq25980_chip_info bq25980_chip_info_tbl[] = {
	[BQ25980] = {
		.model_id = BQ25980,
		.regmap_config = &bq25980_regmap_config,
		.reg_init_values = bq25980_reg_init_val,

		.busocp_sc_def = BQ25980_BUSOCP_SC_DFLT_uA,
		.busocp_byp_def = BQ25980_BUSOCP_BYP_DFLT_uA,
		.busocp_sc_min = BQ25980_BUSOCP_MIN_uA,
		.busocp_sc_max = BQ25980_BUSOCP_SC_MAX_uA,
		.busocp_byp_max = BQ25980_BUSOCP_BYP_MAX_uA,
		.busocp_byp_min = BQ25980_BUSOCP_MIN_uA,
		.busocp_step = BQ25980_BUSOCP_STEP_uA,
		.busocp_offset = BQ25980_BUSOCP_OFFSET_uA,

		.busovp_sc_def = BQ25980_BUSOVP_DFLT_uV,
		.busovp_byp_def = BQ25980_BUSOVP_BYPASS_DFLT_uV,
		.busovp_sc_step = BQ25980_BUSOVP_SC_STEP_uV,
		.busovp_sc_offset = BQ25980_BUSOVP_SC_OFFSET_uV,
		.busovp_byp_step = BQ25980_BUSOVP_BYP_STEP_uV,
		.busovp_byp_offset = BQ25980_BUSOVP_BYP_OFFSET_uV,
		.busovp_sc_min = BQ25980_BUSOVP_SC_MIN_uV,
		.busovp_sc_max = BQ25980_BUSOVP_SC_MAX_uV,
		.busovp_byp_min = BQ25980_BUSOVP_BYP_MIN_uV,
		.busovp_byp_max = BQ25980_BUSOVP_BYP_MAX_uV,

		.batovp_def = BQ25980_BATOVP_DFLT_uV,
		.batovp_max = BQ25980_BATOVP_MAX_uV,
		.batovp_min = BQ25980_BATOVP_MIN_uV,
		.batovp_step = BQ25980_BATOVP_STEP_uV,
		.batovp_offset = BQ25980_BATOVP_OFFSET_uV,

		.batocp_def = BQ25980_BATOCP_DFLT_uA,
		.batocp_max = BQ25980_BATOCP_MAX_uA,

		.adc_curr_step = BQ25980_ADC_CURR_STEP_IBUS_uA,
		.adc_vbat_volt_step = BQ25980_ADC_VOLT_STEP_VBAT_deciuV,
		.adc_vbus_volt_step = BQ25980_ADC_VOLT_STEP_VBUS_deciuV,
		.adc_vbus_volt_offset = BQ25980_ADC_VOLT_OFFSET_VBUS,
		.adc_tchg_step = BQ25980_ADC_TCHG_STEP,
	},

	[SC8571] = {
		.model_id = SC8571,
		.regmap_config = &sc8571_regmap_config,
		.reg_init_values = sc8571_reg_init_val,

		.busocp_sc_def = BQ25980_BUSOCP_SC_DFLT_uA,
		.busocp_byp_def = BQ25980_BUSOCP_BYP_DFLT_uA,
		.busocp_sc_min = BQ25980_BUSOCP_MIN_uA,
		.busocp_sc_max = BQ25980_BUSOCP_SC_MAX_uA,
		.busocp_byp_max = BQ25980_BUSOCP_BYP_MAX_uA,
		.busocp_byp_min = BQ25980_BUSOCP_MIN_uA,
		.busocp_step = BQ25980_BUSOCP_STEP_uA,
		.busocp_offset = BQ25980_BUSOCP_OFFSET_uA,

		.busovp_sc_def = BQ25980_BUSOVP_DFLT_uV,
		.busovp_byp_def = BQ25980_BUSOVP_BYPASS_DFLT_uV,
		.busovp_sc_step = BQ25980_BUSOVP_SC_STEP_uV,
		.busovp_sc_offset = BQ25980_BUSOVP_SC_OFFSET_uV,
		.busovp_byp_step = BQ25980_BUSOVP_BYP_STEP_uV,
		.busovp_byp_offset = BQ25980_BUSOVP_BYP_OFFSET_uV,
		.busovp_sc_min = BQ25980_BUSOVP_SC_MIN_uV,
		.busovp_sc_max = BQ25980_BUSOVP_SC_MAX_uV,
		.busovp_byp_min = BQ25980_BUSOVP_BYP_MIN_uV,
		.busovp_byp_max = BQ25980_BUSOVP_BYP_MAX_uV,

		.batovp_def = BQ25980_BATOVP_DFLT_uV,
		.batovp_max = BQ25980_BATOVP_MAX_uV,
		.batovp_min = BQ25980_BATOVP_MIN_uV,
		.batovp_step = BQ25980_BATOVP_STEP_uV,
		.batovp_offset = BQ25980_BATOVP_OFFSET_uV,

		.batocp_def = BQ25980_BATOCP_DFLT_uA,
		.batocp_max = BQ25980_BATOCP_MAX_uA,

		.adc_curr_step = SC8571_ADC_CURR_STEP_IBUS_uA,
		.adc_vbat_volt_step = SC8571_ADC_VOLT_STEP_VBAT_deciuV,
		.adc_vbus_volt_step = SC8571_ADC_VOLT_STEP_VBUS_deciuV,
		.adc_vbus_volt_offset = BQ25980_ADC_VOLT_OFFSET_VBUS,
		.adc_tchg_step = BQ25980_ADC_TCHG_STEP,

	},
};

static int bq25980_power_supply_init(struct bq25980_device *bq,
							struct device *dev,
							int driver_data)
{
	struct power_supply_config psy_cfg = { .drv_data = bq,
						.of_node = dev->of_node, };

	switch (driver_data) {
	case BQ25980_MASTER:
		bq->psy_desc.name = "cp_master";
		break;
	case BQ25980_SLAVE:
		bq->psy_desc.name = "cp_slave";
		break;
	case BQ25980_STANDALONE:
		bq->psy_desc.name = "cp_standalone";
		break;
	case BQ25960_MASTER:
		bq->psy_desc.name = "cp_master";
		break;
	case BQ25960_SLAVE:
		bq->psy_desc.name = "cp_slave";
		break;
	case BQ25960_STANDALONE:
		bq->psy_desc.name = "cp_standalone";
		break;
	default:
		return -EINVAL;
	}

	bq->psy_desc.type = POWER_SUPPLY_TYPE_UNKNOWN,
	bq->psy_desc.properties = bq25980_power_supply_props,
	bq->psy_desc.num_properties = ARRAY_SIZE(bq25980_power_supply_props),
	bq->psy_desc.get_property = bq25980_get_property,
	bq->psy_desc.set_property = bq25980_set_property,
	bq->psy_desc.property_is_writeable = bq25980_property_is_writeable,

	bq->charger = devm_power_supply_register(bq->dev, &bq->psy_desc, &psy_cfg);
	if (IS_ERR(bq->charger))
		return -EINVAL;

	return 0;
}

static int bq25980_parse_dt(struct bq25980_device *bq)
{
	struct device_node *np = bq->dev->of_node;
	int ret = 0;

	if (!np) {
		bq_err("%s device tree info missing\n", bq->log_tag);
		return -1;
	}

	bq->irq_gpio = of_get_named_gpio(np, "bq25980_irq_gpio", 0);
	if (!gpio_is_valid(bq->irq_gpio)) {
		bq_err("%s failed to parse bq25980_irq_gpio\n", bq->log_tag);
		return -1;
	}

	return ret;
}

static int bq25980_check_work_mode(struct bq25980_device *bq)
{
	int ret;
	int val;

	ret = regmap_read(bq->regmap, BQ25980_CHRGR_CTRL_5, &val);
	if (ret) {
		bq_err("%s failed to read operation mode register\n", bq->log_tag);
		return ret;
	}

	val = (val & BQ25980_MS_MASK);
	if (bq->mode != val) {
		bq_err("%d dts mode %d mismatch with hardware mode %d\n", bq->log_tag, bq->mode, val);
		return -EINVAL;
	}

	bq_info("%s mode = %s\n", bq->log_tag, bq->mode == BQ_STANDALONE ? "Standalone" : (bq->mode == BQ_SLAVE ? "Slave" : "Master"));
	return 0;
}

static int bq25980_parse_dt_id(struct bq25980_device *bq, int driver_data)
{
	switch (driver_data) {
	case BQ25980_STANDALONE:
		if(bq->chip_vendor == TI25980){
			bq->device_id = BQ25980;
			bq->mode = BQ_STANDALONE;
			strcpy(bq->log_tag, "[XMCHG_BQ25980_ALONE]");
		}else if(bq->chip_vendor == SOUTH8571){
			bq->device_id = SC8571;
			bq->mode = BQ_STANDALONE;
			strcpy(bq->log_tag, "[XMCHG_SC8571_ALONE]");
		}
		break;	
	case BQ25980_SLAVE:
		if(bq->chip_vendor == TI25980){
			bq->device_id = BQ25980;
			bq->mode = BQ_SLAVE;
			strcpy(bq->log_tag, "[XMCHG_BQ25980_SLAVE]");
		}else if(bq->chip_vendor == SOUTH8571){
			bq->device_id = SC8571;
			bq->mode = BQ_SLAVE;
			strcpy(bq->log_tag, "[XMCHG_SC8571_SLAVE]");
		}
		break;
	case BQ25980_MASTER:
		if(bq->chip_vendor == TI25980){
			bq->device_id = BQ25980;
			bq->mode = BQ_MASTER;
			strcpy(bq->log_tag, "[XMCHG_BQ25980_MASTER]");
		}else if(bq->chip_vendor == SOUTH8571){
			bq->device_id = SC8571;
			bq->mode = BQ_MASTER;
			strcpy(bq->log_tag, "[XMCHG_SC8571_MASTER]");
		}
		break;
	default:
		bq_err("%s unknown dts id\n", bq->log_tag);
		return -EINVAL;
	}

	bq_info("%s device_id = %d,mode = %d\n", bq->log_tag,bq->device_id,bq->mode);

	return 0;
}

static int bq25980_init_irq(struct bq25980_device *bq)
{
	int ret = 0;

	ret = devm_gpio_request(bq->dev, bq->irq_gpio, dev_name(bq->dev));
	if (ret < 0) {
		bq_err("%s failed to request gpio\n", bq->log_tag);
		return -1;
	}

	bq->irq = gpio_to_irq(bq->irq_gpio);
	if (bq->irq < 0) {
		bq_err("%s failed to get gpio_irq\n", bq->log_tag);
		return -1;
	}

	ret = request_irq(bq->irq, bq25980_interrupt, IRQF_TRIGGER_FALLING | IRQF_ONESHOT, dev_name(bq->dev), bq);
	if (ret < 0) {
		bq_err("%s failed to request irq\n", bq->log_tag);
		return -1;
	}

	enable_irq_wake(bq->irq);

	return 0;
}

static int bq25980_register_charger(struct bq25980_device *bq, int driver_data)
{
	switch (driver_data) {
	case BQ25980_STANDALONE:
		bq->chg_dev = charger_device_register("cp_standalone", bq->dev, bq, &bq25980_chg_ops, &bq25980_standalone_chg_props);
		break;
	case BQ25980_SLAVE:
		bq->chg_dev = charger_device_register("cp_slave", bq->dev, bq, &bq25980_chg_ops, &bq25980_slave_chg_props);
		break;
	case BQ25980_MASTER:
		bq->chg_dev = charger_device_register("cp_master", bq->dev, bq, &bq25980_chg_ops, &bq25980_master_chg_props);
		break;
	case BQ25960_STANDALONE:
	case BQ25960_SLAVE:
	case BQ25960_MASTER:
	default:
		return -EINVAL;
		break;
	}

	return 0;
}

static int __bq25980_read_byte(struct bq25980_device *bq, u8 reg, u8 *data)
{
	s32 ret;

	ret = i2c_smbus_read_byte_data(bq->client, reg);
	if (ret < 0) {
		bq_err("i2c read fail: can't read from reg 0x%02X\n", reg);
		return ret;
	}

	*data = (u8) ret;

	return 0;
}

static int bq25980_read_byte(struct bq25980_device *bq, u8 reg, u8 *data)
{
	int ret;

	mutex_lock(&bq->i2c_rw_lock);
	ret = __bq25980_read_byte(bq, reg, data);
	mutex_unlock(&bq->i2c_rw_lock);

	return ret;
}

static int bq25980_detect_device(struct bq25980_device *bq)
{
	int ret;
	u8 data;

	ret = bq25980_read_byte(bq, BQ25980_DEVICE_INFO, &data);
	if (ret == 0) {
		bq->part_no = (data & BQ25980_DEV_ID_MASK);
		bq->part_no >>= BQ25980_DEV_ID_SHIFT;

		if (data == SC8571_DEVICE_ID)
			bq->chip_vendor = SOUTH8571;
		else if (data == BQ25980_DEVICE_ID)
			bq->chip_vendor = TI25980;
		bq_info("data = %x,ret = %d,bq->vendor = %d\n",data,ret,bq->chip_vendor);
	}

	return ret;
}

#define I2C_STATE		"i2c11_sda_default"
#define OUTPUT_LOW_STATE	"i2c11_sda_gpio"

static void bq25980_reset_i2c(struct bq25980_device *bq)
{
	int ret = 0;

	bq->bq_pinctrl = devm_pinctrl_get(bq->dev);
	if (IS_ERR_OR_NULL(bq->bq_pinctrl)) {
		bq_info("no pinctrl setting!\n");
		return;
	}

	bq->sda_normal = pinctrl_lookup_state(bq->bq_pinctrl, I2C_STATE);
	if (IS_ERR_OR_NULL(bq->sda_normal)) {
		bq_info("get pinctrl state: %s failed!\n", I2C_STATE);
 		return;
	}

	bq->sda_gpio = pinctrl_lookup_state(bq->bq_pinctrl, OUTPUT_LOW_STATE);
	if (IS_ERR_OR_NULL(bq->sda_gpio)) {
		bq_info("get pinctrl state: %s failed!\n", OUTPUT_LOW_STATE);
		return;
	}

	ret = pinctrl_select_state(bq->bq_pinctrl, bq->sda_gpio);
	if (ret < 0) {
		bq_info("set pinctrl state: %s failed!\n", OUTPUT_LOW_STATE);
		return;
	}

	msleep(2000);

	ret = pinctrl_select_state(bq->bq_pinctrl, bq->sda_normal);
	if (ret < 0) {
		bq_info("set pinctrl state: %s failed!\n", I2C_STATE);
		return;
	}

	msleep(500);
	bq_info("gpio reset successful\n");
}

static int bq25980_probe(struct i2c_client *client,
			 const struct i2c_device_id *id)
{
	struct device *dev = &client->dev;
	struct bq25980_device *bq;
	int ret = 0;

	if (product_name == PISSARROPRO) {
		bq_info("BQ25980 probe start\n");
	} else {
		bq_info("PISSARRO no need to probe BQ25980\n");
		return -ENODEV;
	}

	bq = devm_kzalloc(dev, sizeof(*bq), GFP_KERNEL);
	if (!bq)
		return -ENOMEM;

	bq->client = client;
	bq->dev = dev;
	mutex_init(&bq->state_lock);
	mutex_init(&bq->i2c_rw_lock);
	strncpy(bq->model_name, id->name, I2C_NAME_SIZE);

	if (product_name == PISSARROPRO) {
		if (client->addr == 0x65 || client->addr == 0x67) {
			ret = i2c_smbus_read_byte_data(client, BQ25980_DEVICE_INFO);
			if (ret < 0) {
				msleep(100);
				bq_err("failed to communicate with 0x%02x, retry\n", client->addr);
				ret = i2c_smbus_read_byte_data(client, BQ25980_DEVICE_INFO);
				if (ret < 0) {
					bq_err("failed to communicate with 0x%02x again, reset IIC\n", client->addr);
					bq25980_reset_i2c(bq);
					ret = i2c_smbus_read_byte_data(client, BQ25980_DEVICE_INFO);
					if (ret < 0) {
						bq_err("failed to communicate with 0x%02x again, even reset IIC\n", client->addr);
					}
				}
			}
		}
	}

	ret = bq25980_detect_device(bq);
	if (ret) {
		bq_err("No bq25980 device found! retry\n");
		msleep(250);
		ret = bq25980_detect_device(bq);
		if (ret) {
			bq_err("No bq25980 device found!\n");
			return -ENODEV;
		}
	}

	ret = bq25980_parse_dt_id(bq, id->driver_data);
	if (ret)
		return ret;

	bq->chip_info = &bq25980_chip_info_tbl[bq->device_id];
	bq->regmap = devm_regmap_init_i2c(client, bq->chip_info->regmap_config);
	if (IS_ERR(bq->regmap)) {
		bq_err("%s failed to allocate register map\n", bq->log_tag);
		return PTR_ERR(bq->regmap);
	}

	i2c_set_clientdata(client, bq);

	INIT_DELAYED_WORK(&bq->irq_handle_work, bq25980_irq_handler);

	ret = bq25980_check_work_mode(bq);
	if (ret)
		return ret;

	ret = bq25980_parse_dt(bq);
	if (ret) {
		bq_err("%s failed to parse DTS\n", bq->log_tag);
		return ret;
	}

	ret = bq25980_init_irq(bq);
	if (ret) {
		bq_err("%s failed to int irq\n", bq->log_tag);
		return ret;
	}

	ret = bq25980_register_charger(bq, id->driver_data);
	if (ret) {
		bq_err("%s failed to register charger\n", bq->log_tag);
		return ret;
	}

	ret = bq25980_power_supply_init(bq, dev, id->driver_data);
	if (ret) {
		bq_err("%s failed to init psy\n", bq->log_tag);
		return ret;
	}

	ret = bq25980_init_device(bq);
	if (ret) {
		bq_err("%s failed to init registers\n", bq->log_tag);
		return ret;
	}

	bq->chip_ok = true;
	bq_info("%s probe success\n", bq->log_tag);

	return 0;
}

static int bq25980_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct bq25980_device *bq = i2c_get_clientdata(client);
	int ret = 0;

	ret = bq25980_enable_adc(bq, false);
	if (ret)
		bq_err("%s failed to disable ADC\n", bq->log_tag);

	bq_info("%s BQ25980 suspend!\n", bq->log_tag);

	return enable_irq_wake(bq->irq);
}

static int bq25980_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct bq25980_device *bq = i2c_get_clientdata(client);
	int ret = 0;

	ret = bq25980_enable_adc(bq, true);
	if (ret)
		bq_err("%s failed to disable ADC\n", bq->log_tag);

	bq_info("%s BQ25980 resume!\n", bq->log_tag);

	return disable_irq_wake(bq->irq);
}

static const struct dev_pm_ops bq25980_pm_ops = {
	.suspend	= bq25980_suspend,
	.resume		= bq25980_resume,
};

static int bq25980_remove(struct i2c_client *client)
{
	struct bq25980_device *bq = i2c_get_clientdata(client);
	int ret = 0;

	ret = bq25980_enable_adc(bq, false);
	if (ret)
		bq_err("%s failed to disable ADC\n", bq->log_tag);

	power_supply_unregister(bq->charger);
	mutex_destroy(&bq->i2c_rw_lock);
	bq_info("%s BQ25980 shutdown!\n", bq->log_tag);

	return ret;
}

static void bq25980_shutdown(struct i2c_client *client)
{
	struct bq25980_device *bq = i2c_get_clientdata(client);
	int ret = 0;

	ret = bq25980_enable_bypass(bq, false);
	if (ret)
		bq_err("%s failed to disable bypass\n", bq->log_tag);

	ret = bq25980_enable_charge(bq, false);
	if (ret)
		bq_err("%s failed to disable charge\n", bq->log_tag);

	ret = bq25980_enable_adc(bq, false);
	if (ret)
		bq_err("%s failed to disable ADC\n", bq->log_tag);

	bq_info("%s BQ25980 shutdown!\n", bq->log_tag);
}

static const struct i2c_device_id bq25980_i2c_ids[] = {
	{ "bq25980_standalone", BQ25980_STANDALONE },
	{ "bq25980_master", BQ25980_MASTER },
	{ "bq25980_slave", BQ25980_SLAVE },
	{ "bq25960_standalone", BQ25960_STANDALONE },
	{ "bq25960_master", BQ25960_MASTER },
	{ "bq25960_slave", BQ25960_SLAVE },
	{},
};
MODULE_DEVICE_TABLE(i2c, bq25980_i2c_ids);

static const struct of_device_id bq25980_of_match[] = {
	{ .compatible = "bq25980_standalone", .data = (void *)BQ25980_STANDALONE},
	{ .compatible = "bq25980_master", .data = (void *)BQ25980_MASTER},
	{ .compatible = "bq25980_slave", .data = (void *)BQ25980_SLAVE},
	{ .compatible = "bq25960_standalone", .data = (void *)BQ25960_STANDALONE},
	{ .compatible = "bq25960_master", .data = (void *)BQ25960_MASTER},
	{ .compatible = "bq25960_slave", .data = (void *)BQ25960_SLAVE},
	{ },
};
MODULE_DEVICE_TABLE(of, bq25980_of_match);

static struct i2c_driver bq25980_driver = {
	.driver = {
		.name = "bq25980_charger",
		.of_match_table = bq25980_of_match,
		.pm = &bq25980_pm_ops,
	},
	.id_table = bq25980_i2c_ids,
	.probe = bq25980_probe,
	.remove = bq25980_remove,
	.shutdown = bq25980_shutdown,
};

bool bq25980_init(void)
{
        if (i2c_add_driver(&bq25980_driver))
                return false;
        else
                return true;
}
