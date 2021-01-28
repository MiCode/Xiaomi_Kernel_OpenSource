/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef __MT6360_LDO_H
#define __MT6360_LDO_H

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
	MT6360_LDO_LDO1,
	MT6360_LDO_LDO2,
	MT6360_LDO_LDO3,
	MT6360_LDO_LDO5,
	MT6360_LDO_MAX,
};

#define MT6360_LDO_CTRLS_NUM	(5)

struct mt6360_ldo_platform_data {
	struct regulator_init_data *init_data[MT6360_LDO_MAX];
	struct resource *irq_res;
	int irq_res_cnt;
	u32 sdcard_det_en;
};

struct mt6360_ldo_info {
	struct i2c_client *i2c;
	struct device *dev;
	struct rt_regmap_device *regmap;
	struct regulator_dev *rdev[MT6360_LDO_MAX];
	struct mutex io_lock;
	u8 crc8_table[CRC8_TABLE_SIZE];
	u8 chip_rev;
};

struct mt6360_ldo_irq_desc {
	const char *name;
	irq_handler_t irq_handler;
	int irq;
};

#define MT6360_LDO_IRQDESC(name) { #name, mt6360_ldo_##name##_handler, -1}

/* register defininition */
#define MT6360_LDO_RST_LDO_PAS_CODE1	(0x00)
#define MT6360_LDO_RST_LDO_PAS_CODE2	(0x01)
#define MT6360_LDO_RST_LDO		(0x02)
#define MT6360_LDO_RESV1		(0x03)
#define MT6360_LDO_LDO3_EN_CTRL1	(0x04)
#define MT6360_LDO_LDO3_EN_CTRL2	(0x05)
#define MT6360_LDO_LDO3_CTRL0		(0x06)
#define MT6360_LDO_LDO3_CTRL1		(0x07)
#define MT6360_LDO_LDO3_CTRL2		(0x08)
#define MT6360_LDO_LDO3_CTRL3		(0x09)
#define MT6360_LDO_LDO5_EN_CTRL1	(0x0A)
#define MT6360_LDO_LDO5_EN_CTRL2	(0x0B)
#define MT6360_LDO_LDO5_CTRL0		(0x0C)
#define MT6360_LDO_LDO5_CTRL1		(0x0D)
#define MT6360_LDO_LDO5_CTRL2		(0x0E)
#define MT6360_LDO_LDO5_CTRL3		(0x0F)
#define MT6360_LDO_LDO2_EN_CTRL1	(0x10)
#define MT6360_LDO_LDO2_EN_CTRL2	(0x11)
#define MT6360_LDO_LDO2_CTRL0		(0x12)
#define MT6360_LDO_LDO2_CTRL1		(0x13)
#define MT6360_LDO_LDO2_CTRL2		(0x14)
#define MT6360_LDO_LDO2_CTRL3		(0x15)
#define MT6360_LDO_LDO1_EN_CTRL1	(0x16)
#define MT6360_LDO_LDO1_EN_CTRL2	(0x17)
#define MT6360_LDO_LDO1_CTRL0		(0x18)
#define MT6360_LDO_LDO1_CTRL1		(0x19)
#define MT6360_LDO_LDO1_CTRL2		(0x1A)
#define MT6360_LDO_LDO1_CTRL3		(0x1B)
#define MT6360_LDO_RESV2		(0x1C)
#define MT6360_LDO_SPARE		(0x20)

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

#define MT6360_PDATA_VALPROP(name, type, reg, shift, mask, func, base) \
			{offsetof(type, name), reg, shift, mask, func, base}

struct mt6360_pdata_prop {
	size_t offset;
	u8 reg;
	u8 shift;
	u8 mask;
	u32 (*transform)(u32 val);
	u8 base;
};

extern int mt6360_ldo_regmap_register(struct mt6360_ldo_info *mli,
				      struct rt_regmap_fops *fops);
extern void mt6360_ldo_regmap_unregister(struct mt6360_ldo_info *mli);

#endif /* __MT6360_LDO_H */
