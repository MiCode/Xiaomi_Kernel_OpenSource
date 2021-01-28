/*
 * Copyright (C) 2018 MediaTek Inc.

 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#ifndef __MFD_MT6358_CORE_H__
#define __MFD_MT6358_CORE_H__

struct mt6358_chip {
	struct device *dev;
	struct regmap *regmap;
	int irq;
	struct irq_domain *irq_domain;
	struct mutex irqlock;
	unsigned int num_sps;
	unsigned int num_pmic_irqs;
	unsigned short top_int_status_reg;
};

extern unsigned int mt6358_irq_get_virq(struct device *dev, unsigned int hwirq);
extern const char *mt6358_irq_get_name(struct device *dev, unsigned int hwirq);

#endif /* __MFD_MT6358_CORE_H__ */
