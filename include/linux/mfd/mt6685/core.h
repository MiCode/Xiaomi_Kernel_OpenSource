/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#ifndef __MFD_MT6685_CORE_H__
#define __MFD_MT6685_CORE_H__

struct mt6685_chip {
	struct device *dev;
	struct regmap *regmap;
	int irq;
	struct irq_domain *irq_domain;
	struct mutex irqlock;
	u16 wake_mask[2];
	u16 irq_masks_cur[2];
	u16 irq_masks_cache[2];
	u16 int_con[2];
	u16 int_status[2];
};

#endif /* __MFD_MT6685_CORE_H__ */
