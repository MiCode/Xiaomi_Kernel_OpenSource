/*
 * Support MediaTek intpol through gic_arch_extn
 *
 * Copyright (C) 2014 Mediatek Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#include <linux/err.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/io.h>
#include <linux/irq.h>
#include <linux/irqchip/arm-gic.h>

#include "irqchip.h"

#define GIC_HW_IRQ_BASE  32
#define INT_POL_INDEX(a)   ((a) - GIC_HW_IRQ_BASE)

static void __iomem *int_pol_base;

static int mtk_int_pol_set_type(struct irq_data *d, unsigned int type)
{
	unsigned int irq = d->hwirq;
	u32 offset, reg_index, value;

	offset = INT_POL_INDEX(irq) & 0x1F;
	reg_index = INT_POL_INDEX(irq) >> 5;

	/* This arch extension was called with irq_controller_lock held,
	   so the read-modify-write will be atomic */
	value = readl(int_pol_base + reg_index * 4);
	if (type == IRQ_TYPE_LEVEL_LOW || type == IRQ_TYPE_EDGE_FALLING)
		value |= (1 << offset);
	else
		value &= ~(1 << offset);
	writel(value, int_pol_base + reg_index * 4);

	return 0;
}

static int __init mtk_intpol_of_init(struct device_node *node,
				     struct device_node *parent)
{
	int_pol_base = of_io_request_and_map(node, 0, "intpol");
	if (IS_ERR(int_pol_base)) {
		pr_warn("Can't get resource\n");
		return PTR_ERR(int_pol_base);
	}

	gic_arch_extn.irq_set_type = mtk_int_pol_set_type;

	return 0;
}
IRQCHIP_DECLARE(mtk_intpol, "mediatek,mt6577-intpol", mtk_intpol_of_init);
