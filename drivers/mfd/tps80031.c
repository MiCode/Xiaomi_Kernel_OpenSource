/*
 * driver/mfd/tps80031.c
 *
 * Core driver for TI TPS80031
 *
 * Copyright (C) 2011 NVIDIA Corporation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 */

#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/pm.h>
#include <linux/regmap.h>

#include <linux/mfd/core.h>
#include <linux/mfd/tps80031.h>

/* interrupt related registers */
#define TPS80031_INT_STS_A		0xD0
#define TPS80031_INT_STS_B		0xD1
#define TPS80031_INT_STS_C		0xD2
#define TPS80031_INT_MSK_LINE_A		0xD3
#define TPS80031_INT_MSK_LINE_B		0xD4
#define TPS80031_INT_MSK_LINE_C		0xD5
#define TPS80031_INT_MSK_STS_A		0xD6
#define TPS80031_INT_MSK_STS_B		0xD7
#define TPS80031_INT_MSK_STS_C		0xD8

#define TPS80031_CONTROLLER_STAT1		0xE3
#define CONTROLLER_STAT1_BAT_TEMP		0
#define CONTROLLER_STAT1_BAT_REMOVED		1
#define CONTROLLER_STAT1_VBUS_DET		2
#define CONTROLLER_STAT1_VAC_DET		3
#define CONTROLLER_STAT1_FAULT_WDG		4
#define CONTROLLER_STAT1_LINCH_GATED		6

#define TPS80031_CONTROLLER_INT_MASK		0xE0
#define CONTROLLER_INT_MASK_MVAC_DET		0
#define CONTROLLER_INT_MASK_MVBUS_DET		1
#define CONTROLLER_INT_MASK_MBAT_TEMP		2
#define CONTROLLER_INT_MASK_MFAULT_WDG		3
#define CONTROLLER_INT_MASK_MBAT_REMOVED	4
#define CONTROLLER_INT_MASK_MLINCH_GATED	5

#define CHARGE_CONTROL_SUB_INT_MASK		0x3F

/* Version number related register */
#define TPS80031_JTAGVERNUM		0x87
/* Epprom version */
#define TPS80031_EPROM_REV		0xDF

/* External control register */
#define REGEN1_BASE_ADD		0xAE
#define REGEN2_BASE_ADD		0xB1
#define SYSEN_BASE_ADD		0xB4

/* device control registers */
#define TPS80031_PHOENIX_DEV_ON	0x25
#define DEVOFF	1

#define CLK32KAO_BASE_ADD	0xBA
#define CLK32KG_BASE_ADD	0xBD
#define CLK32KAUDIO_BASE_ADD	0xC0

#define EXT_CONTROL_CFG_TRANS 0
#define EXT_CONTROL_CFG_STATE 1

#define STATE_OFF	0x00
#define STATE_ON	0x01
#define STATE_MASK	0x03

#define TRANS_SLEEP_OFF		0x00
#define TRANS_SLEEP_ON		0x04
#define TRANS_SLEEP_MASK	0x0C

#define TPS_NUM_SLAVES	4
#define EXT_PWR_REQ (PWR_REQ_INPUT_PREQ1 | PWR_REQ_INPUT_PREQ2 | \
		PWR_REQ_INPUT_PREQ3)
#define TPS80031_PREQ1_RES_ASS_A	0xD7
#define TPS80031_PREQ2_RES_ASS_A	0xDA
#define TPS80031_PREQ3_RES_ASS_A	0xDD
#define TPS80031_PHOENIX_MSK_TRANSITION 0x20

#define TPS80031_CFG_INPUT_PUPD1 0xF0
#define TPS80031_CFG_INPUT_PUPD2 0xF1
#define TPS80031_CFG_INPUT_PUPD3 0xF2
#define TPS80031_CFG_INPUT_PUPD4 0xF3

#define TPS80031_BBSPOR_CFG	0xE6
#define TPS80031_BBSPOR_CHG_EN	0x8

/* Valid Address ranges */
#define TPS80031_ID0_PMIC_SLAVE_SMPS_DVS	0x55 ... 0x5C

#define TPS80031_ID1_RTC			0x00 ... 0x16
#define TPS80031_ID1_MEMORY			0x17 ... 0x1E
#define TPS80031_ID1_PMC_MASTER			0x1F ... 0x2D
#define TPS80031_ID1_PMC_SLAVE_MISC		0x31 ... 0x34
#define TPS80031_ID1_PMC_SLAVE_SMPS		0x40 ... 0x68
#define TPS80031_ID1_PMC_SLAVE_LDO		0x80 ... 0xA7
#define TPS80031_ID1_PMC_SLAVE_REOSURCES	0XAD ... 0xD0
#define TPS80031_ID1_PMC_PREQ_ASSIGN		0XD7 ... 0xDF
#define TPS80031_ID1_PMC_MISC			0xE2 ... 0xEF
#define TPS80031_ID1_PMC_PU_PD_HZ		0xF0 ... 0xF6
#define TPS80031_ID1_PMC_BACKUP			0xFA

#define TPS80031_ID2_USB			0x00 ... 0x1A
#define TPS80031_ID2_GPADC_CONTROL		0x2E ... 0x36
#define TPS80031_ID2_GPADC_RESULTS		0x37 ... 0x3C
#define TPS80031_ID2_AUXILLIARIES		0x90 ... 0x9C
#define TPS80031_ID2_CUSTOM			0xA0 ... 0xB9
#define TPS80031_ID2_PWM			0xBA ... 0xBE
#define TPS80031_ID2_FUEL_GAUSE			0xC0 ... 0xCB
#define TPS80031_ID2_INTERFACE_INTERRUPTS	0xD0 ... 0xD8
#define TPS80031_ID2_CHARGER			0xDA ... 0xF5

#define TPS80031_ID3_TEST_LDO			0x00 ... 0x09
#define TPS80031_ID3_TEST_SMPS			0x10 ... 0x2B
#define TPS80031_ID3_TEST_POWER			0x30 ... 0x36
#define TPS80031_ID3_TEST_CHARGER		0x40 ... 0x48
#define TPS80031_ID3_TEST_AUXILIIARIES		0x50 ... 0xB1

#define TPS80031_ID3_DIEID			0xC0 ... 0xC8
#define TPS80031_ID3_TRIM_PHOENIX		0xCC ... 0xEA
#define TPS80031_ID3_TRIM_CUSTOM		0xEC ... 0xED

#define TPS80031_MAX_REGISTER	0x100

struct tps80031_pupd_data {
	u8	reg;
	u8	pullup_bit;
	u8	pulldown_bit;
};

static u8 pmc_ext_control_base[] = {
	REGEN1_BASE_ADD,
	REGEN2_BASE_ADD,
	SYSEN_BASE_ADD,
};

static u8 pmc_clk32k_control_base[] = {
	CLK32KAO_BASE_ADD,
	CLK32KG_BASE_ADD,
	CLK32KAUDIO_BASE_ADD,
};
struct tps80031_irq_data {
	u8	mask_reg;
	u8	mask_mask;
	u8	is_sec_int;
	u8	parent_int;
	u8	mask_sec_int_reg;
	u8	int_mask_bit;
	u8	int_sec_sts_reg;
	u8	int_sts_bit;
};

#define TPS80031_IRQ(_reg, _mask)	\
	{							\
		.mask_reg = (TPS80031_INT_MSK_LINE_##_reg) -	\
				TPS80031_INT_MSK_LINE_A,	\
		.mask_mask = (_mask),				\
	}

#define TPS80031_IRQ_SEC(_reg, _mask, _pint, _sint_mask_bit, _sint_sts_bit) \
	{								\
		.mask_reg = (TPS80031_INT_MSK_LINE_##_reg) -		\
				TPS80031_INT_MSK_LINE_A,		\
		.mask_mask = (_mask),					\
		.is_sec_int = true,					\
		.parent_int = TPS80031_INT_##_pint,			\
		.mask_sec_int_reg = TPS80031_CONTROLLER_INT_MASK,	\
		.int_mask_bit = CONTROLLER_INT_MASK_##_sint_mask_bit,	\
		.int_sec_sts_reg = TPS80031_CONTROLLER_STAT1,		\
		.int_sts_bit = CONTROLLER_STAT1_##_sint_sts_bit		\
	}

static const struct tps80031_irq_data tps80031_irqs[] = {

	[TPS80031_INT_PWRON]		= TPS80031_IRQ(A, 0),
	[TPS80031_INT_RPWRON]		= TPS80031_IRQ(A, 1),
	[TPS80031_INT_SYS_VLOW]		= TPS80031_IRQ(A, 2),
	[TPS80031_INT_RTC_ALARM]	= TPS80031_IRQ(A, 3),
	[TPS80031_INT_RTC_PERIOD]	= TPS80031_IRQ(A, 4),
	[TPS80031_INT_HOT_DIE]		= TPS80031_IRQ(A, 5),
	[TPS80031_INT_VXX_SHORT]	= TPS80031_IRQ(A, 6),
	[TPS80031_INT_SPDURATION]	= TPS80031_IRQ(A, 7),
	[TPS80031_INT_WATCHDOG]		= TPS80031_IRQ(B, 0),
	[TPS80031_INT_BAT]		= TPS80031_IRQ(B, 1),
	[TPS80031_INT_SIM]		= TPS80031_IRQ(B, 2),
	[TPS80031_INT_MMC]		= TPS80031_IRQ(B, 3),
	[TPS80031_INT_RES]		= TPS80031_IRQ(B, 4),
	[TPS80031_INT_GPADC_RT]		= TPS80031_IRQ(B, 5),
	[TPS80031_INT_GPADC_SW2_EOC]	= TPS80031_IRQ(B, 6),
	[TPS80031_INT_CC_AUTOCAL]	= TPS80031_IRQ(B, 7),
	[TPS80031_INT_ID_WKUP]		= TPS80031_IRQ(C, 0),
	[TPS80031_INT_VBUSS_WKUP]	= TPS80031_IRQ(C, 1),
	[TPS80031_INT_ID]		= TPS80031_IRQ(C, 2),
	[TPS80031_INT_VBUS]		= TPS80031_IRQ(C, 3),
	[TPS80031_INT_CHRG_CTRL]	= TPS80031_IRQ(C, 4),
	[TPS80031_INT_EXT_CHRG]		= TPS80031_IRQ(C, 5),
	[TPS80031_INT_INT_CHRG]		= TPS80031_IRQ(C, 6),
	[TPS80031_INT_RES2]		= TPS80031_IRQ(C, 7),
	[TPS80031_INT_BAT_TEMP_OVRANGE]	= TPS80031_IRQ_SEC(C, 4, CHRG_CTRL,
						MBAT_TEMP,	BAT_TEMP),
	[TPS80031_INT_BAT_REMOVED]	= TPS80031_IRQ_SEC(C, 4, CHRG_CTRL,
						MBAT_REMOVED,	BAT_REMOVED),
	[TPS80031_INT_VBUS_DET]		= TPS80031_IRQ_SEC(C, 4, CHRG_CTRL,
						MVBUS_DET,	VBUS_DET),
	[TPS80031_INT_VAC_DET]		= TPS80031_IRQ_SEC(C, 4, CHRG_CTRL,
						MVAC_DET,	VAC_DET),
	[TPS80031_INT_FAULT_WDG]	= TPS80031_IRQ_SEC(C, 4, CHRG_CTRL,
						MFAULT_WDG,	FAULT_WDG),
	[TPS80031_INT_LINCH_GATED]	= TPS80031_IRQ_SEC(C, 4, CHRG_CTRL,
						MLINCH_GATED,	LINCH_GATED),
};

#define PUPD_DATA(_reg, _pulldown_bit, _pullup_bit)	\
	{						\
		.reg = TPS80031_CFG_INPUT_PUPD##_reg,	\
		.pulldown_bit = _pulldown_bit,		\
		.pullup_bit = _pullup_bit,		\
	}

static const struct tps80031_pupd_data tps80031_pupds[] = {
	[TPS80031_PREQ1]		= PUPD_DATA(1, 1 << 0,	1 << 1	),
	[TPS80031_PREQ2A]		= PUPD_DATA(1, 1 << 2,	1 << 3	),
	[TPS80031_PREQ2B]		= PUPD_DATA(1, 1 << 4,	1 << 5	),
	[TPS80031_PREQ2C]		= PUPD_DATA(1, 1 << 6,	1 << 7	),
	[TPS80031_PREQ3]		= PUPD_DATA(2, 1 << 0,	1 << 1	),
	[TPS80031_NRES_WARM]		= PUPD_DATA(2, 0,	1 << 2	),
	[TPS80031_PWM_FORCE]		= PUPD_DATA(2, 1 << 5,	0	),
	[TPS80031_CHRG_EXT_CHRG_STATZ]	= PUPD_DATA(2, 0,	1 << 6	),
	[TPS80031_SIM]			= PUPD_DATA(3, 1 << 0,	1 << 1	),
	[TPS80031_MMC]			= PUPD_DATA(3, 1 << 2,	1 << 3	),
	[TPS80031_GPADC_START]		= PUPD_DATA(3, 1 << 4,	0	),
	[TPS80031_DVSI2C_SCL]		= PUPD_DATA(4, 0,	1 << 0	),
	[TPS80031_DVSI2C_SDA]		= PUPD_DATA(4, 0,	1 << 1	),
	[TPS80031_CTLI2C_SCL]		= PUPD_DATA(4, 0,	1 << 2	),
	[TPS80031_CTLI2C_SDA]		= PUPD_DATA(4, 0,	1 << 3	),
};

static const int controller_stat1_irq_nr[] = {
	TPS80031_INT_BAT_TEMP_OVRANGE,
	TPS80031_INT_BAT_REMOVED,
	TPS80031_INT_VBUS_DET,
	TPS80031_INT_VAC_DET,
	TPS80031_INT_FAULT_WDG,
	0,
	TPS80031_INT_LINCH_GATED,
	0
};

/* Structure for TPS80031 Slaves */
struct tps80031_client {
	struct i2c_client *client;
	struct mutex lock;
	u8 addr;
};

struct tps80031 {
	struct device		*dev;
	unsigned long		chip_info;
	int			es_version;

	struct gpio_chip	gpio;
	struct irq_chip		irq_chip;
	struct mutex		irq_lock;
	int			irq_base;
	u32			irq_en;
	u8			mask_cache[3];
	u8			mask_reg[3];
	u8			cont_int_mask_reg;
	u8			cont_int_mask_cache;
	u8			cont_int_en;
	u8			prev_cont_stat1;
	struct tps80031_client	tps_clients[TPS_NUM_SLAVES];
	struct regmap		*regmap[TPS_NUM_SLAVES];
};

/* TPS80031 sub mfd devices */
static struct mfd_cell tps80031_cell[] = {
	{
		.name = "tps80031-regulators",
	},
	{
		.name = "tps80031-rtc",
	},
	{
		.name = "tps80031-gpadc",
	},
	{
		.name = "tps80031-battery-gauge",
	},
	{
		.name = "tps80031-charger",
	},
};


int tps80031_write(struct device *dev, int sid, int reg, uint8_t val)
{
	struct tps80031 *tps80031 = dev_get_drvdata(dev);

	return regmap_write(tps80031->regmap[sid], reg, val);
}
EXPORT_SYMBOL_GPL(tps80031_write);

int tps80031_writes(struct device *dev, int sid, int reg, int len, uint8_t *val)
{
	struct tps80031 *tps80031 = dev_get_drvdata(dev);

	return regmap_bulk_write(tps80031->regmap[sid], reg, val, len);
}
EXPORT_SYMBOL_GPL(tps80031_writes);

int tps80031_read(struct device *dev, int sid, int reg, uint8_t *val)
{
	struct tps80031 *tps80031 = dev_get_drvdata(dev);
	unsigned int ival;
	int ret;

	ret = regmap_read(tps80031->regmap[sid], reg, &ival);
	if (ret < 0) {
		dev_err(dev, "failed reading from reg 0x%02x\n", reg);
		return ret;
	}

	*val = ival;
	return ret;
}
EXPORT_SYMBOL_GPL(tps80031_read);

int tps80031_reads(struct device *dev, int sid, int reg, int len, uint8_t *val)
{
	struct tps80031 *tps80031 = dev_get_drvdata(dev);

	return regmap_bulk_read(tps80031->regmap[sid], reg, val, len);
}
EXPORT_SYMBOL_GPL(tps80031_reads);

int tps80031_set_bits(struct device *dev, int sid, int reg, uint8_t bit_mask)
{
	struct tps80031 *tps80031 = dev_get_drvdata(dev);

	return regmap_update_bits(tps80031->regmap[sid], reg,
				bit_mask, bit_mask);
}
EXPORT_SYMBOL_GPL(tps80031_set_bits);

int tps80031_clr_bits(struct device *dev, int sid, int reg, uint8_t bit_mask)
{
	struct tps80031 *tps80031 = dev_get_drvdata(dev);

	return regmap_update_bits(tps80031->regmap[sid], reg, bit_mask, 0);
}
EXPORT_SYMBOL_GPL(tps80031_clr_bits);

int tps80031_update(struct device *dev, int sid, int reg, uint8_t val,
		uint8_t mask)
{
	struct tps80031 *tps80031 = dev_get_drvdata(dev);

	return regmap_update_bits(tps80031->regmap[sid], reg, mask, val);
}
EXPORT_SYMBOL_GPL(tps80031_update);

int tps80031_force_update(struct device *dev, int sid, int reg, uint8_t val,
			  uint8_t mask)
{
	struct tps80031 *tps80031 = dev_get_drvdata(dev);
	struct tps80031_client *tps = &tps80031->tps_clients[sid];
	uint8_t reg_val;
	int ret = 0;

	mutex_lock(&tps->lock);

	ret = tps80031_read(dev, sid, reg, &reg_val);
	if (ret)
		goto out;

	reg_val = (reg_val & ~mask) | (val & mask);
	ret = tps80031_write(dev, sid, reg, reg_val);

out:
	mutex_unlock(&tps->lock);
	return ret;
}
EXPORT_SYMBOL_GPL(tps80031_force_update);

int tps80031_ext_power_req_config(struct device *dev,
		unsigned long ext_ctrl_flag, int preq_bit,
		int state_reg_add, int trans_reg_add)
{
	u8 res_ass_reg = 0;
	int preq_mask_bit = 0;
	int ret;

	if (!(ext_ctrl_flag & EXT_PWR_REQ))
		return 0;

	if (ext_ctrl_flag & PWR_REQ_INPUT_PREQ1) {
		res_ass_reg = TPS80031_PREQ1_RES_ASS_A + (preq_bit >> 3);
		preq_mask_bit = 5;
	} else if (ext_ctrl_flag & PWR_REQ_INPUT_PREQ2) {
		res_ass_reg = TPS80031_PREQ2_RES_ASS_A + (preq_bit >> 3);
		preq_mask_bit = 6;
	} else if (ext_ctrl_flag & PWR_REQ_INPUT_PREQ3) {
		res_ass_reg = TPS80031_PREQ3_RES_ASS_A + (preq_bit >> 3);
		preq_mask_bit = 7;
	}

	/* Configure REQ_ASS registers */
	ret = tps80031_set_bits(dev, SLAVE_ID1, res_ass_reg,
					BIT(preq_bit & 0x7));
	if (ret < 0) {
		dev_err(dev, "%s() Not able to set bit %d of "
			"reg %d error %d\n",
			__func__, preq_bit, res_ass_reg, ret);
		return ret;
	}

	/* Unmask the PREQ */
	ret = tps80031_clr_bits(dev, SLAVE_ID1,
			TPS80031_PHOENIX_MSK_TRANSITION, BIT(preq_mask_bit));
	if (ret < 0) {
		dev_err(dev, "%s() Not able to clear bit %d of "
			"reg %d error %d\n",
			 __func__, preq_mask_bit,
			TPS80031_PHOENIX_MSK_TRANSITION, ret);
		return ret;
	}

	/* Switch regulator control to resource now */
	if (ext_ctrl_flag & (PWR_REQ_INPUT_PREQ2 | PWR_REQ_INPUT_PREQ3)) {
		ret = tps80031_update(dev, SLAVE_ID1, state_reg_add, 0x0,
						STATE_MASK);
		if (ret < 0)
			dev_err(dev, "%s() Error in writing the STATE "
				"register %d error %d\n", __func__,
				state_reg_add, ret);
	} else {
		ret = tps80031_update(dev, SLAVE_ID1, trans_reg_add,
				TRANS_SLEEP_OFF, TRANS_SLEEP_MASK);
		if (ret < 0)
			dev_err(dev, "%s() Error in writing the TRANS "
				"register %d error %d\n", __func__,
				trans_reg_add, ret);
	}
	return ret;
}
EXPORT_SYMBOL_GPL(tps80031_ext_power_req_config);

unsigned long tps80031_get_chip_info(struct device *dev)
{
	struct tps80031 *tps80031 = dev_get_drvdata(dev);
	return tps80031->chip_info;
}
EXPORT_SYMBOL_GPL(tps80031_get_chip_info);

int tps80031_get_pmu_version(struct device *dev)
{
	struct tps80031 *tps80031 = dev_get_drvdata(dev);
	return tps80031->es_version;
}
EXPORT_SYMBOL_GPL(tps80031_get_pmu_version);

static struct tps80031 *tps80031_dev;
static void tps80031_power_off(void)
{
	struct tps80031_client *tps = &tps80031_dev->tps_clients[SLAVE_ID1];

	if (!tps->client)
		return;
	dev_info(&tps->client->dev, "switching off PMU\n");
	tps80031_write(&tps->client->dev, SLAVE_ID1,
				TPS80031_PHOENIX_DEV_ON, DEVOFF);
}

static void tps80031_pupd_init(struct tps80031 *tps80031,
			       struct tps80031_platform_data *pdata)
{
	struct tps80031_pupd_init_data *pupd_init_data = pdata->pupd_init_data;
	int data_size = pdata->pupd_init_data_size;
	int i;

	for (i = 0; i < data_size; ++i) {
		struct tps80031_pupd_init_data *pupd_init = &pupd_init_data[i];
		const struct tps80031_pupd_data *pupd =
			&tps80031_pupds[pupd_init->input_pin];
		u8 update_value = 0;
		u8 update_mask = pupd->pulldown_bit | pupd->pullup_bit;

		if (pupd_init->setting == TPS80031_PUPD_PULLDOWN)
			update_value = pupd->pulldown_bit;
		else if (pupd_init->setting == TPS80031_PUPD_PULLUP)
			update_value = pupd->pullup_bit;

		tps80031_update(tps80031->dev, SLAVE_ID1, pupd->reg,
				update_value, update_mask);
	}
}

static void tps80031_backup_battery_charger_control(struct tps80031 *tps80031,
						    int enable)
{
	if (enable)
		tps80031_update(tps80031->dev, SLAVE_ID1, TPS80031_BBSPOR_CFG,
				TPS80031_BBSPOR_CHG_EN, TPS80031_BBSPOR_CHG_EN);
	else
		tps80031_update(tps80031->dev, SLAVE_ID1, TPS80031_BBSPOR_CFG,
				0, TPS80031_BBSPOR_CHG_EN);
}

static void tps80031_init_ext_control(struct tps80031 *tps80031,
			struct tps80031_platform_data *pdata) {
	int ret;
	int i;

	/* Clear all external control for this rail */
	for (i = 0; i < 9; ++i) {
		ret = tps80031_write(tps80031->dev, SLAVE_ID1,
				TPS80031_PREQ1_RES_ASS_A + i, 0);
		if (ret < 0)
			dev_err(tps80031->dev, "%s() Error in clearing "
				"register %02x\n", __func__,
				TPS80031_PREQ1_RES_ASS_A + i);
	}

	/* Mask the PREQ */
	ret = tps80031_set_bits(tps80031->dev, SLAVE_ID1,
			TPS80031_PHOENIX_MSK_TRANSITION, 0x7 << 5);
	if (ret < 0)
		dev_err(tps80031->dev, "%s() Not able to mask register "
			"0x%02x\n", __func__, TPS80031_PHOENIX_MSK_TRANSITION);
}

static int tps80031_gpio_get(struct gpio_chip *gc, unsigned offset)
{
	struct tps80031 *tps80031 = container_of(gc, struct tps80031, gpio);
	struct tps80031_client *tps = &tps80031->tps_clients[SLAVE_ID1];
	uint8_t state;
	uint8_t trans;
	int ret;

	ret = tps80031_read(&tps->client->dev, SLAVE_ID1,
			pmc_ext_control_base[offset] +
				EXT_CONTROL_CFG_STATE, &state);
	if (ret)
		return ret;

	if (state != 0) {
		ret = tps80031_read(&tps->client->dev, SLAVE_ID1,
				pmc_ext_control_base[offset] +
					EXT_CONTROL_CFG_TRANS, &trans);
		if (ret)
			return ret;
		return trans & 0x1;
	}
	return 0;
}

static void tps80031_gpio_set(struct gpio_chip *gc, unsigned offset,
			int value)
{
	struct tps80031 *tps80031 = container_of(gc, struct tps80031, gpio);

	tps80031_update(tps80031->dev, SLAVE_ID1,
		pmc_ext_control_base[offset] + EXT_CONTROL_CFG_TRANS,
			value, 0x1);
}

static int tps80031_gpio_input(struct gpio_chip *gc, unsigned offset)
{
	return -EIO;
}

static int tps80031_gpio_output(struct gpio_chip *gc, unsigned offset,
				int value)
{
	tps80031_gpio_set(gc, offset, value);
	return 0;
}

static int tps80031_gpio_enable(struct gpio_chip *gc, unsigned offset)
{
	struct tps80031 *tps80031 = container_of(gc, struct tps80031, gpio);
	int ret;

	ret = tps80031_update(tps80031->dev, SLAVE_ID1,
		pmc_ext_control_base[offset] + EXT_CONTROL_CFG_STATE,
						STATE_ON, STATE_MASK);
	if (ret)
		return ret;

	return tps80031_write(tps80031->dev, SLAVE_ID1,
		pmc_ext_control_base[offset] + EXT_CONTROL_CFG_TRANS, 0x0);
}

static void tps80031_gpio_disable(struct gpio_chip *gc, unsigned offset)
{
	struct tps80031 *tps80031 = container_of(gc, struct tps80031, gpio);
	tps80031_update(tps80031->dev, SLAVE_ID1,
		pmc_ext_control_base[offset] + EXT_CONTROL_CFG_STATE,
						STATE_OFF, STATE_MASK);
}

static void tps80031_gpio_init(struct tps80031 *tps80031,
			struct tps80031_platform_data *pdata)
{
	int ret;
	int gpio_base = pdata->gpio_base;
	struct tps80031_client *tps = &tps80031->tps_clients[SLAVE_ID1];
	struct tps80031_gpio_init_data *gpio_init_data = pdata->gpio_init_data;
	int data_size = pdata->gpio_init_data_size;
	static int preq_bit_pos[TPS80031_GPIO_NR] = {16, 17, 18};
	int base_add;
	int i;

	if (gpio_base <= 0)
		return;

	/* Configure the external request mode */
	for (i = 0; i < data_size; ++i) {
		struct tps80031_gpio_init_data *gpio_pd = &gpio_init_data[i];
		base_add = pmc_ext_control_base[gpio_pd->gpio_nr];

		if (gpio_pd->ext_ctrl_flag & EXT_PWR_REQ) {
			ret = tps80031_ext_power_req_config(tps80031->dev,
				gpio_pd->ext_ctrl_flag,
				preq_bit_pos[gpio_pd->gpio_nr],
				base_add + EXT_CONTROL_CFG_STATE,
				base_add + EXT_CONTROL_CFG_TRANS);
			if (ret < 0)
				dev_warn(tps80031->dev, "Ext pwrreq GPIO "
					"sleep control fails\n");
		}

		if (gpio_pd->ext_ctrl_flag & PWR_OFF_ON_SLEEP) {
			ret = tps80031_update(tps80031->dev, SLAVE_ID1,
				base_add + EXT_CONTROL_CFG_TRANS, 0x0, 0xC);
			if (ret < 0)
				dev_warn(tps80031->dev, "GPIO OFF on sleep "
					"control fails\n");
		}

		if (gpio_pd->ext_ctrl_flag & PWR_ON_ON_SLEEP) {
			ret = tps80031_update(tps80031->dev, SLAVE_ID1,
				base_add + EXT_CONTROL_CFG_TRANS, 0x4, 0xC);
			if (ret < 0)
				dev_warn(tps80031->dev, "GPIO ON on sleep "
					"control fails\n");
		}
	}

	tps80031->gpio.owner		= THIS_MODULE;
	tps80031->gpio.label		= tps->client->name;
	tps80031->gpio.dev		= tps80031->dev;
	tps80031->gpio.base		= gpio_base;
	tps80031->gpio.ngpio		= TPS80031_GPIO_NR;
	tps80031->gpio.can_sleep	= 1;

	tps80031->gpio.request		= tps80031_gpio_enable;
	tps80031->gpio.free		= tps80031_gpio_disable;
	tps80031->gpio.direction_input	= tps80031_gpio_input;
	tps80031->gpio.direction_output	= tps80031_gpio_output;
	tps80031->gpio.set		= tps80031_gpio_set;
	tps80031->gpio.get		= tps80031_gpio_get;

	ret = gpiochip_add(&tps80031->gpio);
	if (ret)
		dev_warn(tps80031->dev, "GPIO registration failed: %d\n", ret);
}

static void tps80031_irq_lock(struct irq_data *data)
{
	struct tps80031 *tps80031 = irq_data_get_irq_chip_data(data);

	mutex_lock(&tps80031->irq_lock);
}

static void tps80031_irq_enable(struct irq_data *data)
{
	struct tps80031 *tps80031 = irq_data_get_irq_chip_data(data);
	unsigned int __irq = data->irq - tps80031->irq_base;
	const struct tps80031_irq_data *irq_data = &tps80031_irqs[__irq];

	if (irq_data->is_sec_int) {
		tps80031->cont_int_mask_reg &= ~(1 << irq_data->int_mask_bit);
		tps80031->cont_int_en |= (1 << irq_data->int_mask_bit);
		tps80031->mask_reg[irq_data->mask_reg] &= ~(1 << irq_data->mask_mask);
		tps80031->irq_en |= (1 << irq_data->parent_int);
	} else
		tps80031->mask_reg[irq_data->mask_reg] &= ~(1 << irq_data->mask_mask);

	tps80031->irq_en |= (1 << __irq);
}

static void tps80031_irq_disable(struct irq_data *data)
{
	struct tps80031 *tps80031 = irq_data_get_irq_chip_data(data);

	unsigned int __irq = data->irq - tps80031->irq_base;
	const struct tps80031_irq_data *irq_data = &tps80031_irqs[__irq];

	if (irq_data->is_sec_int) {
		tps80031->cont_int_mask_reg |= (1 << irq_data->int_mask_bit);
		tps80031->cont_int_en &= ~(1 << irq_data->int_mask_bit);
		if (!tps80031->cont_int_en) {
			tps80031->mask_reg[irq_data->mask_reg] |=
						(1 << irq_data->mask_mask);
			tps80031->irq_en &= ~(1 << irq_data->parent_int);
		}
		tps80031->irq_en &= ~(1 << __irq);
	} else
		tps80031->mask_reg[irq_data->mask_reg] |= (1 << irq_data->mask_mask);

	tps80031->irq_en &= ~(1 << __irq);
}

static void tps80031_irq_sync_unlock(struct irq_data *data)
{
	struct tps80031 *tps80031 = irq_data_get_irq_chip_data(data);
	int i;

	for (i = 0; i < ARRAY_SIZE(tps80031->mask_reg); i++) {
		if (tps80031->mask_reg[i] != tps80031->mask_cache[i]) {
			if (!WARN_ON(tps80031_write(tps80031->dev, SLAVE_ID2,
						TPS80031_INT_MSK_LINE_A + i,
						tps80031->mask_reg[i])))
				if (!WARN_ON(tps80031_write(tps80031->dev,
						SLAVE_ID2,
						TPS80031_INT_MSK_STS_A + i,
						tps80031->mask_reg[i])))
					tps80031->mask_cache[i] =
							tps80031->mask_reg[i];
		}
	}

	if (tps80031->cont_int_mask_reg != tps80031->cont_int_mask_cache) {
		if (!WARN_ON(tps80031_write(tps80031->dev, SLAVE_ID2,
				TPS80031_CONTROLLER_INT_MASK,
				tps80031->cont_int_mask_reg)))
			tps80031->cont_int_mask_cache =
						tps80031->cont_int_mask_reg;
	}

	mutex_unlock(&tps80031->irq_lock);
}

static irqreturn_t tps80031_charge_control_irq(int irq, void *data)
{
	struct tps80031 *tps80031 = data;
	int ret = 0;
	int i;
	u8 cont_sts;
	u8 org_sts;
	if (irq != (tps80031->irq_base + TPS80031_INT_CHRG_CTRL)) {
		dev_err(tps80031->dev, "%s() Got the illegal interrupt %d\n",
					__func__, irq);
		return IRQ_NONE;
	}

	ret = tps80031_read(tps80031->dev, SLAVE_ID2,
			TPS80031_CONTROLLER_STAT1, &org_sts);
	if (ret < 0) {
		dev_err(tps80031->dev, "%s(): failed to read controller state1 "
				"status %d\n", __func__, ret);
		return IRQ_NONE;
	}

	/* Get change from last interrupt and mask for interested interrupt
	 * for charge control interrupt */
	cont_sts = org_sts ^ tps80031->prev_cont_stat1;
	tps80031->prev_cont_stat1 = org_sts;
	/* Clear watchdog timer state */
	tps80031->prev_cont_stat1 &= ~(1 << 4);
	cont_sts &= 0x5F;

	for (i = 0; i < 8; ++i) {
		if (!controller_stat1_irq_nr[i])
			continue;

		if ((cont_sts & BIT(i)) &&
			(tps80031->irq_en & BIT(controller_stat1_irq_nr[i])))
			handle_nested_irq(tps80031->irq_base +
						controller_stat1_irq_nr[i]);
		cont_sts &= ~BIT(i);
	}
	return IRQ_HANDLED;
}

static irqreturn_t tps80031_irq(int irq, void *data)
{
	struct tps80031 *tps80031 = data;
	int ret = 0;
	u32 acks;
	int i;
	uint8_t tmp[3];

	ret = tps80031_reads(tps80031->dev, SLAVE_ID2,
			     TPS80031_INT_STS_A, 3, tmp);
	if (ret < 0) {
		dev_err(tps80031->dev, "failed to read interrupt status\n");
		return IRQ_NONE;
	}
	acks = (tmp[2] << 16) | (tmp[1] << 8) | tmp[0];

	if (acks) {
		/*
		 * Hardware behavior: hardware have the shadow register for
		 * interrupt status register which is updated if interrupt
		 * comes just after the interrupt status read. This shadow
		 * register gets written to main status register and cleared
		 * if any byte write happens in any of status register like
		 * STS_A, STS_B or STS_C.
		 * Hence here to clear the original interrupt status and
		 * updating the STS register with the shadow register, it is
		 * require to write only one byte in any of STS register.
		 * Having multiple register write can cause the STS register
		 * to clear without handling those interrupt and can cause
		 * interrupt miss.
		 */
		ret = tps80031_write(tps80031->dev, SLAVE_ID2,
				      TPS80031_INT_STS_A, 0);
		if (ret < 0) {
			dev_err(tps80031->dev, "failed to write "
						"interrupt status\n");
			return IRQ_NONE;
		}

		while (acks) {
			i = __ffs(acks);
			if (tps80031->irq_en & (1 << i))
				handle_nested_irq(tps80031->irq_base + i);
			acks &= ~(1 << i);
		}
	}

	return IRQ_HANDLED;
}

static int __devinit tps80031_irq_init(struct tps80031 *tps80031, int irq,
				int irq_base)
{
	int i, ret;

	if (!irq_base) {
		dev_warn(tps80031->dev, "No interrupt support on IRQ base\n");
		return -EINVAL;
	}

	mutex_init(&tps80031->irq_lock);

	for (i = 0; i < 3; i++) {
		tps80031->mask_reg[i] = 0xFF;
		tps80031->mask_cache[i] = tps80031->mask_reg[i];
		tps80031_write(tps80031->dev, SLAVE_ID2,
					TPS80031_INT_MSK_LINE_A + i,
					tps80031->mask_cache[i]);
		tps80031_write(tps80031->dev, SLAVE_ID2,
					TPS80031_INT_MSK_STS_A + i, 0xFF);
		tps80031_write(tps80031->dev, SLAVE_ID2,
					TPS80031_INT_STS_A + i, 0xFF);
	}

	ret = tps80031_read(tps80031->dev, SLAVE_ID2,
				TPS80031_CONTROLLER_INT_MASK,
				&tps80031->cont_int_mask_reg);
	if (ret < 0) {
		dev_err(tps80031->dev, "Error in reading the controller_mask "
					"register %d\n", ret);
		return ret;
	}

	tps80031->cont_int_mask_reg |= CHARGE_CONTROL_SUB_INT_MASK;
	tps80031->cont_int_mask_cache = tps80031->cont_int_mask_reg;
	tps80031->cont_int_en = 0;
	ret = tps80031_write(tps80031->dev, SLAVE_ID2,
				TPS80031_CONTROLLER_INT_MASK,
				tps80031->cont_int_mask_reg);
	if (ret < 0) {
		dev_err(tps80031->dev, "Error in writing the controller_mask "
					"register %d\n", ret);
		return ret;
	}

	ret = tps80031_read(tps80031->dev, SLAVE_ID2,
			TPS80031_CONTROLLER_STAT1, &tps80031->prev_cont_stat1);
	if (ret < 0) {
		dev_err(tps80031->dev, "%s(): failed to read controller state1 "
				"status %d\n", __func__, ret);
		return ret;
	}

	/* Clear watch dog interrupt status in status */
	tps80031->prev_cont_stat1 &= ~(1 << 4);

	tps80031->irq_base = irq_base;

	tps80031->irq_chip.name = "tps80031";
	tps80031->irq_chip.irq_enable = tps80031_irq_enable;
	tps80031->irq_chip.irq_disable = tps80031_irq_disable;
	tps80031->irq_chip.irq_bus_lock = tps80031_irq_lock;
	tps80031->irq_chip.irq_bus_sync_unlock = tps80031_irq_sync_unlock;

	for (i = 0; i < TPS80031_INT_NR; i++) {
		int __irq = i + tps80031->irq_base;
		irq_set_chip_data(__irq, tps80031);
		irq_set_chip_and_handler(__irq, &tps80031->irq_chip,
					 handle_simple_irq);
		irq_set_nested_thread(__irq, 1);
#ifdef CONFIG_ARM
		set_irq_flags(__irq, IRQF_VALID);
#endif
	}

	ret = request_threaded_irq(irq, NULL, tps80031_irq, IRQF_ONESHOT,
				"tps80031", tps80031);
	/* register the isr for the secondary interrupt */
	if (!ret)
		ret = request_threaded_irq(irq_base + TPS80031_INT_CHRG_CTRL,
				NULL, tps80031_charge_control_irq,
				IRQF_ONESHOT, "80031_chg_ctl", tps80031);
	if (!ret) {

		device_init_wakeup(tps80031->dev, 1);
		enable_irq_wake(irq);
	}

	return ret;
}

static void tps80031_clk32k_enable(struct tps80031 *tps80031, int base_add)
{
	int ret;
	ret = tps80031_update(tps80031->dev, SLAVE_ID1,
			base_add + EXT_CONTROL_CFG_STATE, STATE_ON, STATE_MASK);
	if (!ret)
		ret = tps80031_update(tps80031->dev, SLAVE_ID1,
				base_add + EXT_CONTROL_CFG_TRANS,
				STATE_ON, STATE_MASK);
	if (ret < 0)
		dev_err(tps80031->dev, "Error in updating clock register\n");
}

static void tps80031_clk32k_init(struct tps80031 *tps80031,
			struct tps80031_platform_data *pdata)
{
	int ret;
	struct tps80031_clk32k_init_data *clk32_idata = pdata->clk32k_init_data;
	int data_size = pdata->clk32k_init_data_size;
	static int clk32k_preq_bit_pos[TPS80031_CLOCK32K_NR] = {-1, 20, 19};
	int base_add;
	int i;

	if (!clk32_idata || !data_size)
		return;

	/* Configure the external request mode */
	for (i = 0; i < data_size; ++i) {
		struct tps80031_clk32k_init_data *clk32_pd =  &clk32_idata[i];
		base_add = pmc_clk32k_control_base[clk32_pd->clk32k_nr];
		if (clk32_pd->enable)
			tps80031_clk32k_enable(tps80031, base_add);

		if ((clk32_pd->ext_ctrl_flag & EXT_PWR_REQ) &&
			 (clk32k_preq_bit_pos[clk32_pd->clk32k_nr] != -1)) {
			ret = tps80031_ext_power_req_config(tps80031->dev,
				clk32_pd->ext_ctrl_flag,
				clk32k_preq_bit_pos[clk32_pd->clk32k_nr],
				base_add + EXT_CONTROL_CFG_STATE,
				base_add + EXT_CONTROL_CFG_TRANS);
			if (ret < 0)
				dev_warn(tps80031->dev, "Clk32 ext control "
					"fails\n");
		}

		if (clk32_pd->ext_ctrl_flag & PWR_OFF_ON_SLEEP) {
			ret = tps80031_update(tps80031->dev, SLAVE_ID1,
				base_add + EXT_CONTROL_CFG_TRANS, 0x0, 0xC);
			if (ret < 0)
				dev_warn(tps80031->dev, "clk OFF on sleep "
					"control fails\n");
		}

		if (clk32_pd->ext_ctrl_flag & PWR_ON_ON_SLEEP) {
			ret = tps80031_update(tps80031->dev, SLAVE_ID1,
				base_add + EXT_CONTROL_CFG_TRANS, 0x4, 0xC);
			if (ret < 0)
				dev_warn(tps80031->dev, "clk ON sleep "
					"control fails\n");
		}
	}
}

#ifdef CONFIG_DEBUG_FS
#include <linux/debugfs.h>
#include <linux/seq_file.h>
static void print_regs(const char *header, struct seq_file *s,
		int sid, int start_offset, int end_offset)
{
	struct tps80031 *tps80031 = s->private;
	struct tps80031_client *tps = &tps80031->tps_clients[sid];
	uint8_t reg_val;
	int i;
	int ret;

	seq_printf(s, "%s\n", header);
	for (i = start_offset; i <= end_offset; ++i) {
		ret = tps80031_read(&tps->client->dev, sid, i, &reg_val);
		if (ret >= 0)
			seq_printf(s, "Addr = 0x%02x Reg 0x%02x Value 0x%02x\n",
						tps->client->addr, i, reg_val);
	}
	seq_printf(s, "------------------\n");
}

static int dbg_tps_show(struct seq_file *s, void *unused)
{
	seq_printf(s, "TPS80031 Registers\n");
	seq_printf(s, "------------------\n");
	print_regs("VIO Regs",       s, SLAVE_ID1, 0x47, 0x49);
	print_regs("VIO Regs",       s, SLAVE_ID0, 0x49, 0x4A);
	print_regs("SMPS1 Regs",     s, SLAVE_ID1, 0x53, 0x54);
	print_regs("SMPS1 Regs",     s, SLAVE_ID0, 0x55, 0x56);
	print_regs("SMPS1 Regs",     s, SLAVE_ID1, 0x57, 0x57);
	print_regs("SMPS2 Regs",     s, SLAVE_ID1, 0x59, 0x5B);
	print_regs("SMPS2 Regs",     s, SLAVE_ID0, 0x5B, 0x5C);
	print_regs("SMPS2 Regs",     s, SLAVE_ID1, 0x5C, 0x5D);
	print_regs("SMPS3 Regs",     s, SLAVE_ID1, 0x65, 0x68);
	print_regs("SMPS4 Regs",     s, SLAVE_ID1, 0x41, 0x44);
	print_regs("VANA Regs",      s, SLAVE_ID1, 0x81, 0x83);
	print_regs("VRTC Regs",      s, SLAVE_ID1, 0xC3, 0xC4);
	print_regs("LDO1 Regs",      s, SLAVE_ID1, 0x9D, 0x9F);
	print_regs("LDO2 Regs",      s, SLAVE_ID1, 0x85, 0x87);
	print_regs("LDO3 Regs",      s, SLAVE_ID1, 0x8D, 0x8F);
	print_regs("LDO4 Regs",      s, SLAVE_ID1, 0x89, 0x8B);
	print_regs("LDO5 Regs",      s, SLAVE_ID1, 0x99, 0x9B);
	print_regs("LDO6 Regs",      s, SLAVE_ID1, 0x91, 0x93);
	print_regs("LDO7 Regs",      s, SLAVE_ID1, 0xA5, 0xA7);
	print_regs("LDOUSB Regs",    s, SLAVE_ID1, 0xA1, 0xA3);
	print_regs("LDOLN Regs",     s, SLAVE_ID1, 0x95, 0x97);
	print_regs("REGEN1 Regs",    s, SLAVE_ID1, 0xAE, 0xAF);
	print_regs("REGEN2 Regs",    s, SLAVE_ID1, 0xB1, 0xB2);
	print_regs("SYSEN Regs",     s, SLAVE_ID1, 0xB4, 0xB5);
	print_regs("CLK32KAO Regs",  s, SLAVE_ID1, 0xBA, 0xBB);
	print_regs("CLK32KG Regs",   s, SLAVE_ID1, 0xBD, 0xBE);
	print_regs("CLK32KAUD Regs", s, SLAVE_ID1, 0xC0, 0xC1);
	print_regs("INT Regs",       s, SLAVE_ID2, 0xD0, 0xD8);
	print_regs("PREQ Regs",      s, SLAVE_ID1, 0xD7, 0xDF);
	print_regs("MASK_PH Regs",   s, SLAVE_ID1, 0x20, 0x21);
	print_regs("PMC MISC Regs",  s, SLAVE_ID1, 0xE0, 0xEF);
	print_regs("CONT_STATE",     s, SLAVE_ID2, 0xE0, 0xE4);
	print_regs("VERNUM Regs",    s, SLAVE_ID3, 0x87, 0x87);
	print_regs("EEPROM Regs",    s, SLAVE_ID3, 0xDF, 0xDF);
	print_regs("CHARGE Regs",    s, SLAVE_ID2, 0xDA, 0xF5);
	return 0;
}

static int dbg_tps_open(struct inode *inode, struct file *file)
{
	return single_open(file, dbg_tps_show, inode->i_private);
}

static const struct file_operations debug_fops = {
	.open		= dbg_tps_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static void __init tps80031_debuginit(struct tps80031 *tps)
{
	(void)debugfs_create_file("tps80031", S_IRUGO, NULL,
			tps, &debug_fops);
}
#else
static void __init tps80031_debuginit(struct tps80031 *tpsi)
{
	return;
}
#endif

static bool rd_wr_reg_id0(struct device *dev, unsigned int reg)
{
	switch(reg) {
	case TPS80031_ID0_PMIC_SLAVE_SMPS_DVS:
		return true;
	default:
		pr_err("non-existing reg %s() %d reg %x\n", __func__, __LINE__, reg);
		BUG();
		return false;
	}
}

static bool rd_wr_reg_id1(struct device *dev, unsigned int reg)
{
	switch(reg) {
	case TPS80031_ID1_RTC:
	case TPS80031_ID1_MEMORY:
	case TPS80031_ID1_PMC_MASTER:
	case TPS80031_ID1_PMC_SLAVE_SMPS:
	case TPS80031_ID1_PMC_SLAVE_MISC:
	case TPS80031_ID1_PMC_SLAVE_LDO:
	case TPS80031_ID1_PMC_SLAVE_REOSURCES:
	case TPS80031_ID1_PMC_PREQ_ASSIGN:
	case TPS80031_ID1_PMC_MISC:
	case TPS80031_ID1_PMC_PU_PD_HZ:
	case TPS80031_ID1_PMC_BACKUP:
		return true;
	default:
		pr_err("non-existing reg %s() %d reg %x\n", __func__, __LINE__, reg);
		BUG();
		return false;
	}
}

static bool rd_wr_reg_id2(struct device *dev, unsigned int reg)
{
	switch(reg) {
	case TPS80031_ID2_USB:
	case TPS80031_ID2_GPADC_CONTROL:
	case TPS80031_ID2_GPADC_RESULTS:
	case TPS80031_ID2_AUXILLIARIES:
	case TPS80031_ID2_CUSTOM:
	case TPS80031_ID2_PWM:
	case TPS80031_ID2_FUEL_GAUSE:
	case TPS80031_ID2_INTERFACE_INTERRUPTS:
	case TPS80031_ID2_CHARGER:
		return true;
	default:
		pr_err("non-existing reg %s() %d reg %x\n", __func__, __LINE__, reg);
		BUG();
		return false;
	}
}
static bool rd_wr_reg_id3(struct device *dev, unsigned int reg)
{
	switch(reg) {
	case TPS80031_ID3_TEST_LDO:
	case TPS80031_ID3_TEST_SMPS:
	case TPS80031_ID3_TEST_POWER:
	case TPS80031_ID3_TEST_CHARGER:
	case TPS80031_ID3_TEST_AUXILIIARIES:
	case TPS80031_ID3_DIEID:
	case TPS80031_ID3_TRIM_PHOENIX:
	case TPS80031_ID3_TRIM_CUSTOM:
		return true;
	default:
		pr_err("non-existing reg %s() %d reg %x\n", __func__, __LINE__, reg);
		BUG();
		return false;
	}
}

static const struct regmap_config tps80031_regmap_configs[] = {
	{
		.reg_bits = 8,
		.val_bits = 8,
		.writeable_reg = rd_wr_reg_id0,
		.readable_reg = rd_wr_reg_id0,
		.max_register = TPS80031_MAX_REGISTER - 1,
	},
	{
		.reg_bits = 8,
		.val_bits = 8,
		.writeable_reg = rd_wr_reg_id1,
		.readable_reg = rd_wr_reg_id1,
		.max_register = TPS80031_MAX_REGISTER - 1,
	},
	{
		.reg_bits = 8,
		.val_bits = 8,
		.writeable_reg = rd_wr_reg_id2,
		.readable_reg = rd_wr_reg_id2,
		.max_register = TPS80031_MAX_REGISTER - 1,
	},
	{
		.reg_bits = 8,
		.val_bits = 8,
		.writeable_reg = rd_wr_reg_id3,
		.readable_reg = rd_wr_reg_id3,
		.max_register = TPS80031_MAX_REGISTER - 1,
	},
};

static int __devexit tps80031_i2c_remove(struct i2c_client *client)
{
	struct tps80031 *tps80031 = i2c_get_clientdata(client);
	int i;

	mfd_remove_devices(tps80031->dev);

	if (client->irq)
		free_irq(client->irq, tps80031);

	if (tps80031->gpio.owner != NULL)
		if (gpiochip_remove(&tps80031->gpio) < 0)
			dev_err(&client->dev, "Error in removing the gpio driver\n");

	for (i = 0; i < TPS_NUM_SLAVES; i++) {
		struct tps80031_client *tps = &tps80031->tps_clients[i];
		if (tps->client && tps->client != client)
			i2c_unregister_device(tps->client);
		tps80031->tps_clients[i].client = NULL;
		mutex_destroy(&tps->lock);
	}

	return 0;
}

static int __devinit tps80031_i2c_probe(struct i2c_client *client,
					const struct i2c_device_id *id)
{
	struct tps80031_platform_data *pdata = client->dev.platform_data;
	struct tps80031 *tps80031;
	struct tps80031_client *tps;
	int ret;
	int jtag_ver;
	int ep_ver;
	int i;

	if (!pdata) {
		dev_err(&client->dev, "tps80031 requires platform data\n");
		return -ENOTSUPP;
	}

	jtag_ver = i2c_smbus_read_byte_data(client, TPS80031_JTAGVERNUM);
	if (jtag_ver < 0) {
		dev_err(&client->dev, "Silicon version number read"
				" failed: %d\n", jtag_ver);
		return -EIO;
	}

	ep_ver = i2c_smbus_read_byte_data(client, TPS80031_EPROM_REV);
	if (ep_ver < 0) {
		dev_err(&client->dev, "Silicon eeprom version read"
				" failed: %d\n", ep_ver);
		return -EIO;
	}

	dev_info(&client->dev, "Jtag version 0x%02x and Eeprom version 0x%02x\n",
						jtag_ver, ep_ver);

	tps80031 = devm_kzalloc(&client->dev, sizeof(*tps80031), GFP_KERNEL);
	if (!tps80031) {
		dev_err(&client->dev, "Memory alloc for tps80031 failed\n");
		return -ENOMEM;
	}

	tps80031->es_version = jtag_ver;
	tps80031->dev = &client->dev;
	i2c_set_clientdata(client, tps80031);
	tps80031->chip_info = id->driver_data;

	/* Set up slaves */
	tps80031->tps_clients[SLAVE_ID0].addr = I2C_ID0_ADDR;
	tps80031->tps_clients[SLAVE_ID1].addr = I2C_ID1_ADDR;
	tps80031->tps_clients[SLAVE_ID2].addr = I2C_ID2_ADDR;
	tps80031->tps_clients[SLAVE_ID3].addr = I2C_ID3_ADDR;
	for (i = 0; i < TPS_NUM_SLAVES; i++) {
		tps = &tps80031->tps_clients[i];
		if (tps->addr == client->addr)
			tps->client = client;
		else
			tps->client = i2c_new_dummy(client->adapter,
						tps->addr);
		if (!tps->client) {
			dev_err(&client->dev, "can't attach client %d\n", i);
			ret = -ENOMEM;
			goto fail_client_reg;
		}
		i2c_set_clientdata(tps->client, tps80031);
		mutex_init(&tps->lock);

		tps80031->regmap[i] = devm_regmap_init_i2c(tps->client,
					&tps80031_regmap_configs[i]);
		if (IS_ERR(tps80031->regmap[i])) {
			ret = PTR_ERR(tps80031->regmap[i]);
			dev_err(&client->dev,
				"regmap %d init failed, err %d\n", i, ret);
			goto fail_client_reg;
		}
	}

	if (client->irq) {
		ret = tps80031_irq_init(tps80031, client->irq,
					pdata->irq_base);
		if (ret) {
			dev_err(&client->dev, "IRQ init failed: %d\n", ret);
			goto fail_client_reg;
		}
	}

	tps80031_pupd_init(tps80031, pdata);

	tps80031_init_ext_control(tps80031, pdata);

	ret = mfd_add_devices(tps80031->dev, -1,
			tps80031_cell, ARRAY_SIZE(tps80031_cell), NULL, 0);
	if (ret < 0) {
		dev_err(&client->dev, "mfd_add_devices failed: %d\n", ret);
		goto fail_mfd_add;
	}

	tps80031_gpio_init(tps80031, pdata);

	tps80031_clk32k_init(tps80031, pdata);

	tps80031_debuginit(tps80031);

	tps80031_backup_battery_charger_control(tps80031, 1);

	if (pdata->use_power_off && !pm_power_off)
		pm_power_off = tps80031_power_off;

	tps80031_dev = tps80031;

	return 0;

fail_mfd_add:
	if (client->irq)
		free_irq(client->irq, tps80031);
fail_client_reg:
	for (i = 0; i < TPS_NUM_SLAVES; i++) {
		struct tps80031_client *tps = &tps80031->tps_clients[i];
		if (tps->client && tps->client != client)
			i2c_unregister_device(tps->client);
		tps80031->tps_clients[i].client = NULL;
		mutex_destroy(&tps->lock);
	}
	return ret;
}

#ifdef CONFIG_PM
static int tps80031_i2c_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct tps80031 *tps80031 = i2c_get_clientdata(client);
	if (client->irq)
		disable_irq(client->irq);
	tps80031_backup_battery_charger_control(tps80031, 0);
	return 0;
}

static int tps80031_i2c_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct tps80031 *tps80031 = i2c_get_clientdata(client);
	tps80031_backup_battery_charger_control(tps80031, 1);
	if (client->irq)
		enable_irq(client->irq);
	return 0;
}
static const struct dev_pm_ops tps80031_dev_pm_ops = {
	.suspend	= tps80031_i2c_suspend,
	.resume		= tps80031_i2c_resume,
};
#define TPS80031_DEV_PM (&tps80031_dev_pm_ops)
#else
#define TPS80031_DEV_PM NULL
#endif


static const struct i2c_device_id tps80031_id_table[] = {
	{ "tps80031", TPS80031 },
	{ "tps80032", TPS80032 },
};
MODULE_DEVICE_TABLE(i2c, tps80031_id_table);

static struct i2c_driver tps80031_driver = {
	.driver	= {
		.name	= "tps80031",
		.owner	= THIS_MODULE,
		.pm	= TPS80031_DEV_PM,
	},
	.probe		= tps80031_i2c_probe,
	.remove		= __devexit_p(tps80031_i2c_remove),
	.id_table	= tps80031_id_table,
};

static int __init tps80031_init(void)
{
	return i2c_add_driver(&tps80031_driver);
}
subsys_initcall(tps80031_init);

static void __exit tps80031_exit(void)
{
	i2c_del_driver(&tps80031_driver);
}
module_exit(tps80031_exit);

MODULE_DESCRIPTION("TPS80031 core driver");
MODULE_LICENSE("GPL");
