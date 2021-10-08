/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __MT6360_PRIVATE_H__
#define __MT6360_PRIVATE_H__

#include <linux/interrupt.h>
#include <linux/of.h>
#include <linux/regmap.h>

/* IRQ definitions */
struct mt6360_pmu_irq_desc {
	const char *name;
	irq_handler_t irq_handler;
	int irq;
};

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
	u16 reg;
	u8 shift;
	u8 mask;
	u32 (*transform)(u32 val);
	u8 base;
};

static inline int mt6360_pdata_apply_helper(void *context, void *pdata,
					   const struct mt6360_pdata_prop *prop,
					   int prop_cnt)
{
	int i, ret;
	u32 val;

	for (i = 0; i < prop_cnt; i++) {
		val = *(u32 *)(pdata + prop[i].offset);
		if (prop[i].transform)
			val = prop[i].transform(val);
		val += prop[i].base;
		ret = regmap_update_bits(context,
			     prop[i].reg, prop[i].mask, val << prop[i].shift);
		if (ret < 0)
			return ret;
	}
	return 0;
}

#endif /* __MT6360_PRIVATE_H__ */
