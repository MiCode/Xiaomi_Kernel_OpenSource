/*
 * Copyright (C) 2010 Google, Inc.
 * Copyright (c) 2013, NVIDIA CORPORATION.  All rights reserved.
 * Copyright (C) 2016 XiaoMi, Inc.
 *
 * Author:
 *	Brian Swetland <swetland@google.com>
 *	Iliyan Malchev <malchev@google.com>
 *	Lucas Dai <lucasd@nvidia.com>
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

#include "gic.h"
#include "board.h"

#define ICTLR_CPU_IER		0x20
#define ICTLR_CPU_IER_SET	0x24
#define ICTLR_CPU_IER_CLR	0x28
#define ICTLR_CPU_IEP_CLASS	0x2C

#define ICTLR_COP_IER          0x30
#define ICTLR_COP_IER_SET      0x34
#define ICTLR_COP_IER_CLR      0x38
#define ICTLR_COP_IEP_CLASS    0x3c


#define FIRST_LEGACY_IRQ	32

static void __iomem *ictlr_reg_base[] = {
	IO_ADDRESS(TEGRA_PRIMARY_ICTLR_BASE),
	IO_ADDRESS(TEGRA_SECONDARY_ICTLR_BASE),
	IO_ADDRESS(TEGRA_TERTIARY_ICTLR_BASE),
	IO_ADDRESS(TEGRA_QUATERNARY_ICTLR_BASE),
};

static bool tegra_check_cop_irq_enabled(unsigned int irq)
{
	void __iomem *base;
	pr_debug("%s: %d\n", __func__, irq);

	irq -= FIRST_LEGACY_IRQ;
	base = ictlr_reg_base[irq >> 5];
	if ((1 << (irq & 31)) & readl(base + ICTLR_COP_IER))
		return true;
	return false;
}

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
	u32 val;
	void __iomem *base;

	base = tegra_get_gic_cpu_base();
	if (!has_fiq_gic_war()) {
		/* enable FIQ */
		val = readl(base + GIC_CPU_CTRL);
		val &= ~GIC_CPU_CTRL_FIQ_EN; /* pass FIQs through */
		val |= GIC_CPU_CTRL_ENABLE_GRP1; /* enableNS */
		writel(val, base + GIC_CPU_CTRL);
		tegra_legacy_select_fiq(irq, true);
		tegra_fiq_unmask(irq_get_irq_data(irq));
	} else {
		/*
		 * For Tegra FIQ WAR, we cannot use FIQ to CPU route directly.
		 * We are relying on GIC Group 0 (which this IRQ will be
		 * routed to) to generate FIQ, we need check if AVP also using
		 * this IRQ.
		 */
		BUG_ON(tegra_check_cop_irq_enabled(irq));
		enable_irq(irq);
	}
}

void tegra_fiq_ack(unsigned int fiq)
{
	if (has_fiq_gic_war())
		/*
		 * We will continuously get into the FIQ handler if we don't
		 * do ACK; if we do ACK we will hit into FIQ handler once per
		 * one FIQ:
		 *  * For UART FIQ user interactive debugger, we might want make
		 *    FIQ handler continuously running to check the UART input,
		 *    so we don't need do ACK in this case;
		 *  * If we just want use FIQ debugger dump the stack once, we
		 *    need do the FIQ ACK by this function.
		 */
		readl_relaxed(tegra_get_gic_cpu_base() + GIC_CPU_INTACK);
}

void tegra_fiq_disable(int irq)
{
	if (!has_fiq_gic_war()) {
		tegra_fiq_mask(irq_get_irq_data(irq));
		tegra_legacy_select_fiq(irq, false);
	} else {
		disable_irq(irq);
	}
}
