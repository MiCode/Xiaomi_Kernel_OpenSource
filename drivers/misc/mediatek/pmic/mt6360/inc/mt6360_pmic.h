/*
 *  drivers/misc/mediatek/pmic/mt6360/inc/mt6360_pmic.h
 *
 *  Copyright (C) 2018 Mediatek Technology Corp.
 *  cy_huang <cy_huang@richtek.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *  See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#ifndef __MT6360_PMIC_H
#define __MT6360_PMIC_H

#include <linux/mutex.h>
#include <linux/crc8.h>
#include <linux/interrupt.h>
#include <linux/of.h>
#include "config.h"
#include "rt-regmap.h"

#define mt_dbg(dev, fmt, ...) \
	do { \
		if (dbg_log_en) \
			dev_dbg(dev, fmt, ##__VA_ARGS__); \
	} while (0)

enum {
	MT6360_PMIC_BUCK1,
	MT6360_PMIC_BUCK2,
	MT6360_PMIC_LDO6,
	MT6360_PMIC_LDO7,
	MT6360_PMIC_MAX,
};

#define MT6360_SYS_CTRLS_NUM	(3)
#define MT6360_BUCK_CTRLS_NUM	(8)
#define MT6360_LDO_CTRLS_NUM	(5)

struct mt6360_pmic_platform_data {
	struct regulator_init_data *init_data[MT6360_PMIC_MAX];
	struct resource *irq_res;
	int irq_res_cnt;
	u32 buck1_lp_vout;
	u32 buck2_lp_vout;
	u8 pwr_off_seq[MT6360_PMIC_MAX];
};

struct mt6360_pmic_info {
	struct i2c_client *i2c;
	struct device *dev;
	struct rt_regmap_device *regmap;
	struct regulator_dev *rdev[MT6360_PMIC_MAX];
	struct mutex io_lock;
	u8 crc8_table[CRC8_TABLE_SIZE];
	struct charger_device *chg_dev;
	u8 chip_rev;
};

struct mt6360_pmic_irq_desc {
	const char *name;
	irq_handler_t irq_handler;
	int irq;
};

#define MT6360_PMIC_IRQDESC(name) { #name, mt6360_pmic_##name##_handler, -1}

/* register defininition */
#define MT6360_PMIC_RST_PMIC_PAS_CODE1	(0x00)
#define MT6360_PMIC_RST_PMIC_PAS_CODE2	(0x01)
#define MT6360_PMIC_RST_PMIC		(0x02)
#define MT6360_PMIC_RESV1		(0x03)
#define MT6360_PMIC_SYSUV_CTRL1		(0x04)
#define MT6360_PMIC_SYSUV_CTRL2		(0x05)
#define MT6360_PMIC_HW_TRAPPING		(0x06)
#define MT6360_PMIC_BUCK1_SEQOFFDLY	(0x07)
#define MT6360_PMIC_BUCK2_SEQOFFDLY	(0x08)
#define MT6360_PMIC_LDO7_SEQOFFDLY	(0x09)
#define MT6360_PMIC_LDO6_SEQOFFDLY	(0x0A)
#define MT6360_PMIC_RESV2		(0x0B)
#define MT6360_PMIC_BUCK1_VOSEL		(0x10)
#define MT6360_PMIC_BUCK1_LP_VOSEL	(0x11)
#define MT6360_PMIC_BUCK1_OC		(0x12)
#define MT6360_PMIC_BUCK1_SFCHG_R	(0x13)
#define MT6360_PMIC_BUCK1_SFCHG_F	(0x14)
#define MT6360_PMIC_BUCK1_DVS		(0x15)
#define MT6360_PMIC_BUCK1_EN_CTRL1	(0x16)
#define MT6360_PMIC_BUCK1_EN_CTRL2	(0x17)
#define MT6360_PMIC_BUCK1_CTRL1		(0x18)
#define MT6360_PMIC_BUCK1_CTRL2		(0x19)
#define MT6360_PMIC_BUCK1_Hidden1	(0x1A)
#define MT6360_PMIC_BUCK1_Hidden2	(0x1B)
#define MT6360_PMIC_BUCK1_Hidden3	(0x1C)
#define MT6360_PMIC_BUCK1_Hidden4	(0x1D)
#define MT6360_PMIC_BUCK1_Hidden5	(0x1E)
#define MT6360_PMIC_BUCK1_Hidden6	(0x1F)
#define MT6360_PMIC_BUCK2_VOSEL		(0x20)
#define MT6360_PMIC_BUCK2_LP_VOSEL	(0x21)
#define MT6360_PMIC_BUCK2_OC		(0x22)
#define MT6360_PMIC_BUCK2_SFCHG_R	(0x23)
#define MT6360_PMIC_BUCK2_SFCHG_F	(0x24)
#define MT6360_PMIC_BUCK2_DVS		(0x25)
#define MT6360_PMIC_BUCK2_EN_CTRL1	(0x26)
#define MT6360_PMIC_BUCK2_EN_CTRL2	(0x27)
#define MT6360_PMIC_BUCK2_CTRL1		(0x28)
#define MT6360_PMIC_BUCK2_CTRL2		(0x29)
#define MT6360_PMIC_BUCK2_Hidden1	(0x2A)
#define MT6360_PMIC_BUCK2_Hidden2	(0x2B)
#define MT6360_PMIC_BUCK2_Hidden3	(0x2C)
#define MT6360_PMIC_BUCK2_Hidden4	(0x2D)
#define MT6360_PMIC_BUCK2_Hidden5	(0x2E)
#define MT6360_PMIC_BUCK2_Hidden6	(0x2F)
#define MT6360_PMIC_LDO7_EN_CTRL1	(0x30)
#define MT6360_PMIC_LDO7_EN_CTRL2	(0x31)
#define MT6360_PMIC_LDO7_CTRL0		(0x32)
#define MT6360_PMIC_LDO7_CTRL1		(0x33)
#define MT6360_PMIC_LDO7_CTRL2		(0x34)
#define MT6360_PMIC_LDO7_CTRL3		(0x35)
#define MT6360_PMIC_LDO6_EN_CTRL1	(0x36)
#define MT6360_PMIC_LDO6_EN_CTRL2	(0x37)
#define MT6360_PMIC_LDO6_CTRL0		(0x38)
#define MT6360_PMIC_LDO6_CTRL1		(0x39)
#define MT6360_PMIC_LDO6_CTRL2		(0x3A)
#define MT6360_PMIC_LDO6_CTRL3		(0x3B)
#define MT6360_PMIC_RESV3		(0x3C)

#define  MT6360_DT_VALPROP(name, type) \
			{#name, offsetof(type, name)}

struct mt6360_val_prop {
	const char *name;
	size_t offset;
};

static inline void mt6360_dt_parser_helper(struct device_node *np, void *data,
					   const struct mt6360_val_prop *props,
					   int prop_cnt)
{
	int i;

	for (i = 0; i < prop_cnt; i++) {
		if (unlikely(!props[i].name))
			continue;
		of_property_read_u32(np, props[i].name, data + props[i].offset);
	}
}

#define MT6360_PDATA_VALPROP(name, type, reg, shift, mask, func, base, rewrit) \
		{offsetof(type, name), reg, shift, mask, func, base, rewrit}

struct mt6360_pdata_prop {
	size_t offset;
	u8 reg;
	u8 shift;
	u8 mask;
	u32 (*transform)(u32 val);
	u8 base;
	bool (*rewrite_needed)(u32 val);
};

extern int mt6360_pmic_regmap_register(struct mt6360_pmic_info *mli,
				       struct rt_regmap_fops *fops);
extern void mt6360_pmic_regmap_unregister(struct mt6360_pmic_info *mli);

#endif /* __MT6360_PMIC_H */
