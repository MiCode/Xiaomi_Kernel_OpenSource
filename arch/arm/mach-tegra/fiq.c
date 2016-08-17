/*
 * Copyright (C) 2010 Google, Inc.
 *
 * Author:
 *	Brian Swetland <swetland@google.com>
 *	Iliyan Malchev <malchev@google.com>
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

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/io.h>
#include <linux/slab.h>

#include <asm/hardware/gic.h>

#include <mach/iomap.h>
#include <mach/fiq.h>

#include "board.h"

#define ICTLR_CPU_IER		0x20
#define ICTLR_CPU_IER_SET	0x24
#define ICTLR_CPU_IER_CLR	0x28
#define ICTLR_CPU_IEP_CLASS	0x2C

#define FIRST_LEGACY_IRQ	32

static void __iomem *ictlr_reg_base[] = {
	IO_ADDRESS(TEGRA_PRIMARY_ICTLR_BASE),
	IO_ADDRESS(TEGRA_SECONDARY_ICTLR_BASE),
	IO_ADDRESS(TEGRA_TERTIARY_ICTLR_BASE),
	IO_ADDRESS(TEGRA_QUATERNARY_ICTLR_BASE),
};

static void tegra_legacy_select_fiq(unsigned int irq, bool fiq)
{
	void __iomem *base;
	pr_debug("%s: %d\n", __func__, irq);

	irq -= FIRST_LEGACY_IRQ;
	base = ictlr_reg_base[irq>>5];
	writel(fiq << (irq & 31), base + ICTLR_CPU_IEP_CLASS);
}

static void tegra_fiq_mask(struct irq_data *d)
{
	void __iomem *base;
	int leg_irq;

	if (d->irq < FIRST_LEGACY_IRQ)
		return;

	leg_irq = d->irq - FIRST_LEGACY_IRQ;
	base = ictlr_reg_base[leg_irq >> 5];
	writel(1 << (leg_irq & 31), base + ICTLR_CPU_IER_CLR);
}

static void tegra_fiq_unmask(struct irq_data *d)
{
	void __iomem *base;
	int leg_irq;

	if (d->irq < FIRST_LEGACY_IRQ)
		return;

	leg_irq = d->irq - FIRST_LEGACY_IRQ;
	base = ictlr_reg_base[leg_irq >> 5];
	writel(1 << (leg_irq & 31), base + ICTLR_CPU_IER_SET);
}

void tegra_fiq_enable(int irq)
{
	void __iomem *base = IO_ADDRESS(TEGRA_ARM_PERIF_BASE + 0x100);
	/* enable FIQ */
	u32 val = readl(base + GIC_CPU_CTRL);
	val &= ~8; /* pass FIQs through */
	val |= 2; /* enableNS */
	writel(val, base + GIC_CPU_CTRL);
	tegra_legacy_select_fiq(irq, true);
	tegra_fiq_unmask(irq_get_irq_data(irq));
}

void tegra_fiq_disable(int irq)
{
	tegra_fiq_mask(irq_get_irq_data(irq));
	tegra_legacy_select_fiq(irq, false);
}
