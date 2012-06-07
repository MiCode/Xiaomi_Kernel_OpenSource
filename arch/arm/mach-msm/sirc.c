/* linux/arch/arm/mach-msm/irq.c
 *
 * Copyright (c) 2009-2011 Code Aurora Forum. All rights reserved.
 * Copyright (C) 2009 Google, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/io.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/irqdesc.h>
#include <asm/irq.h>
#include <asm/io.h>
#include <mach/fiq.h>
#include <mach/msm_iomap.h>

#include "sirc.h"

static unsigned int int_enable;
static unsigned int wake_enable;

static struct sirc_regs_t sirc_regs = {
	.int_enable       = SPSS_SIRC_INT_ENABLE,
	.int_enable_clear = SPSS_SIRC_INT_ENABLE_CLEAR,
	.int_enable_set   = SPSS_SIRC_INT_ENABLE_SET,
	.int_type         = SPSS_SIRC_INT_TYPE,
	.int_polarity     = SPSS_SIRC_INT_POLARITY,
	.int_clear        = SPSS_SIRC_INT_CLEAR,
};

static struct sirc_cascade_regs sirc_reg_table[] = {
	{
		.int_status  = SPSS_SIRC_IRQ_STATUS,
		.cascade_irq = INT_SIRC_0,
		.cascade_fiq = INT_SIRC_1,
	}
};

static unsigned int save_type;
static unsigned int save_polarity;

/* Mask off the given interrupt. Keep the int_enable mask in sync with
   the enable reg, so it can be restored after power collapse. */
static void sirc_irq_mask(struct irq_data *d)
{
	unsigned int mask;

	mask = 1 << (d->irq - FIRST_SIRC_IRQ);
	writel(mask, sirc_regs.int_enable_clear);
	int_enable &= ~mask;
	mb();
	return;
}

/* Unmask the given interrupt. Keep the int_enable mask in sync with
   the enable reg, so it can be restored after power collapse. */
static void sirc_irq_unmask(struct irq_data *d)
{
	unsigned int mask;

	mask = 1 << (d->irq - FIRST_SIRC_IRQ);
	writel(mask, sirc_regs.int_enable_set);
	mb();
	int_enable |= mask;
	return;
}

static void sirc_irq_ack(struct irq_data *d)
{
	unsigned int mask;

	mask = 1 << (d->irq - FIRST_SIRC_IRQ);
	writel(mask, sirc_regs.int_clear);
	mb();
	return;
}

static int sirc_irq_set_wake(struct irq_data *d, unsigned int on)
{
	unsigned int mask;

	/* Used to set the interrupt enable mask during power collapse. */
	mask = 1 << (d->irq - FIRST_SIRC_IRQ);
	if (on)
		wake_enable |= mask;
	else
		wake_enable &= ~mask;

	return 0;
}

static int sirc_irq_set_type(struct irq_data *d, unsigned int flow_type)
{
	unsigned int mask;
	unsigned int val;

	mask = 1 << (d->irq - FIRST_SIRC_IRQ);
	val = readl(sirc_regs.int_polarity);

	if (flow_type & (IRQF_TRIGGER_LOW | IRQF_TRIGGER_FALLING))
		val |= mask;
	else
		val &= ~mask;

	writel(val, sirc_regs.int_polarity);

	val = readl(sirc_regs.int_type);
	if (flow_type & (IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING)) {
		val |= mask;
	} else {
		val &= ~mask;
	}

	writel(val, sirc_regs.int_type);
	mb();

	return 0;
}

#if defined(CONFIG_MSM_FIQ_SUPPORT)
void sirc_fiq_select(int irq, bool enable)
{
	uint32_t mask = 1 << (irq - FIRST_SIRC_IRQ);
	uint32_t val;
	unsigned long flags;

	local_irq_save(flags);
	val = readl(SPSS_SIRC_INT_SELECT);
	if (enable)
		val |= mask;
	else
		val &= ~mask;
	writel(val, SPSS_SIRC_INT_SELECT);
	mb();
	local_irq_restore(flags);
}
#endif

/* Finds the pending interrupt on the passed cascade irq and redrives it */
static void sirc_irq_handler(unsigned int irq, struct irq_desc *desc)
{
	unsigned int reg = 0;
	unsigned int sirq;
	unsigned int status;

	while ((reg < ARRAY_SIZE(sirc_reg_table)) &&
		(sirc_reg_table[reg].cascade_irq != irq))
		reg++;

	if (reg == ARRAY_SIZE(sirc_reg_table)) {
		printk(KERN_ERR "%s: incorrect irq %d called\n",
			__func__, irq);
		return;
	}

	status = readl(sirc_reg_table[reg].int_status);
	status &= SIRC_MASK;
	if (status == 0)
		return;

	for (sirq = 0;
	     (sirq < NR_SIRC_IRQS) && ((status & (1U << sirq)) == 0);
	     sirq++)
		;
	generic_handle_irq(sirq+FIRST_SIRC_IRQ);

	irq_desc_get_chip(desc)->irq_ack(irq_get_irq_data(irq));
}

void msm_sirc_enter_sleep(void)
{
	save_type     = readl(sirc_regs.int_type);
	save_polarity = readl(sirc_regs.int_polarity);
	writel(wake_enable, sirc_regs.int_enable);
	mb();
	return;
}

void msm_sirc_exit_sleep(void)
{
	writel(save_type, sirc_regs.int_type);
	writel(save_polarity, sirc_regs.int_polarity);
	writel(int_enable, sirc_regs.int_enable);
	mb();
	return;
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

	int_enable = 0;
	wake_enable = 0;

	for (i = FIRST_SIRC_IRQ; i < LAST_SIRC_IRQ; i++) {
		irq_set_chip_and_handler(i, &sirc_irq_chip, handle_edge_irq);
		set_irq_flags(i, IRQF_VALID);
	}

	for (i = 0; i < ARRAY_SIZE(sirc_reg_table); i++) {
		irq_set_chained_handler(sirc_reg_table[i].cascade_irq,
					sirc_irq_handler);
		irq_set_irq_wake(sirc_reg_table[i].cascade_irq, 1);
#if defined(CONFIG_MSM_FIQ_SUPPORT)
		msm_fiq_select(sirc_reg_table[i].cascade_fiq);
		msm_fiq_enable(sirc_reg_table[i].cascade_fiq);
#endif
	}
	return;
}

