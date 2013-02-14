/* Copyright (c) 2010-2011, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/io.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <asm/irq.h>

#include "sirc.h"

static unsigned int sirc_int_enable[2];

static struct sirc_regs_t sirc_regs = {
	.int_enable       = SPSS_SIRC_INT_ENABLE,
	.int_type         = SPSS_SIRC_INT_TYPE,
	.int_polarity     = SPSS_SIRC_INT_POLARITY,
	.int_clear        = SPSS_SIRC_INT_CLEAR,
};

static inline void sirc_get_group_offset_mask(unsigned int irq,
	unsigned int *group, unsigned int *offset, unsigned int *mask)
{
	*group = 0;
	*offset = irq - FIRST_SIRC_IRQ;
	if (*offset >= NR_SIRC_IRQS_GROUPA) {
		*group = 1;
		*offset -= NR_SIRC_IRQS_GROUPA;
	}
	*mask = 1 << *offset;
}

static void sirc_irq_mask(struct irq_data *d)
{
	void *reg_enable;
	unsigned int group, offset, mask;
	unsigned int val;

	sirc_get_group_offset_mask(d->irq, &group, &offset, &mask);

	reg_enable = sirc_regs.int_enable + group * 4;
	val = __raw_readl(reg_enable);
	__raw_writel(val & ~mask, reg_enable);
	sirc_int_enable[group] &= ~mask;
	mb();
}

static void sirc_irq_unmask(struct irq_data *d)
{
	void *reg_enable;
	void *reg_clear;
	unsigned int group, offset, mask;
	unsigned int val;

	sirc_get_group_offset_mask(d->irq, &group, &offset, &mask);

	if (irq_desc[d->irq].handle_irq == handle_level_irq) {
		reg_clear = sirc_regs.int_clear + group * 4;
		__raw_writel(mask, reg_clear);
	}

	reg_enable = sirc_regs.int_enable + group * 4;
	val = __raw_readl(reg_enable);
	__raw_writel(val | mask, reg_enable);
	sirc_int_enable[group] |= mask;
	mb();
}

static void sirc_irq_ack(struct irq_data *d)
{
	void *reg_clear;
	unsigned int group, offset, mask;

	sirc_get_group_offset_mask(d->irq, &group, &offset, &mask);

	reg_clear = sirc_regs.int_clear + group * 4;
	__raw_writel(mask, reg_clear);
}

static int sirc_irq_set_wake(struct irq_data *d, unsigned int on)
{
	return 0;
}

static int sirc_irq_set_type(struct irq_data *d, unsigned int flow_type)
{
	void *reg_polarity, *reg_type;
	unsigned int group, offset, mask;
	unsigned int val;

	sirc_get_group_offset_mask(d->irq, &group, &offset, &mask);

	reg_polarity = sirc_regs.int_polarity + group * 4;
	val = __raw_readl(reg_polarity);

	if (flow_type & (IRQF_TRIGGER_LOW | IRQF_TRIGGER_FALLING))
		val &= ~mask;
	else
		val |= mask;

	__raw_writel(val, reg_polarity);

	reg_type = sirc_regs.int_type + group * 4;
	val = __raw_readl(reg_type);

	if (flow_type & (IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING)) {
		val |= mask;
		irq_desc[d->irq].handle_irq = handle_edge_irq;
	} else {
		val &= ~mask;
		irq_desc[d->irq].handle_irq = handle_level_irq;
	}

	__raw_writel(val, reg_type);

	return 0;
}

/* Finds the pending interrupt on the passed cascade irq and redrives it */
static void sirc_irq_handler(unsigned int irq, struct irq_desc *desc)
{
	unsigned int sirq;

	for (;;) {
		sirq = __raw_readl(SPSS_SIRC_VEC_INDEX_RD);
		if (sirq >= NR_SIRC_IRQS)
			break;

		generic_handle_irq(sirq + FIRST_SIRC_IRQ);
	}

	irq_desc_get_chip(desc)->irq_ack(irq_get_irq_data(irq));
}

static struct irq_chip sirc_irq_chip = {
	.name		= "sirc",
	.irq_ack	= sirc_irq_ack,
	.irq_mask	= sirc_irq_mask,
	.irq_unmask	= sirc_irq_unmask,
	.irq_set_wake	= sirc_irq_set_wake,
	.irq_set_type	= sirc_irq_set_type,
};

void __init msm_init_sirc(void)
{
	int i;

	sirc_int_enable[0] = 0;
	sirc_int_enable[1] = 0;

	for (i = FIRST_SIRC_IRQ; i <= LAST_SIRC_IRQ; i++) {
		irq_set_chip_and_handler(i, &sirc_irq_chip, handle_edge_irq);
		set_irq_flags(i, IRQF_VALID);
	}

	irq_set_chained_handler(INT_SIRC_0, sirc_irq_handler);
	irq_set_irq_wake(INT_SIRC_0, 1);
}
