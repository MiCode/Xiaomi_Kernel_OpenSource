/*
 * Copyright (C) 2010-2012, NVIDIA Corporation
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
#include <linux/cpumask.h>	/* Required by asm/hardware/gic.h */
#include <linux/io.h>
#include <linux/irqnr.h>
#include <linux/cpu_pm.h>

#include <asm/hardware/gic.h>

#include <mach/iomap.h>
#include <mach/irqs.h>

#include "gic.h"
#include "pm.h"

#define ARM_VERSION_CORTEX_A15	0xC0F

void __iomem *tegra_gic_cpu_base;
static u32 gic_version;

#if defined(CONFIG_HOTPLUG_CPU) || defined(CONFIG_PM_SLEEP)

void tegra_gic_cpu_disable(bool disable_pass_through)
{
	u32 gic_cpu_ctrl = 0;

#ifndef CONFIG_ARCH_TEGRA_2x_SOC
	if (disable_pass_through) {
		if (gic_version == GIC_V2)
			gic_cpu_ctrl = 0x1E0;
		else
			gic_cpu_ctrl = 2;
	}
#endif
	writel(gic_cpu_ctrl, tegra_gic_cpu_base + GIC_CPU_CTRL);
}

void tegra_gic_cpu_enable(void)
{
	writel(1, tegra_gic_cpu_base + GIC_CPU_CTRL);
}

#endif

#if defined(CONFIG_PM_SLEEP)

int tegra_gic_pending_interrupt(void)
{
	u32 irq = readl(tegra_gic_cpu_base + GIC_CPU_HIGHPRI);
	irq &= 0x3FF;

	return irq;
}

#ifndef CONFIG_ARCH_TEGRA_2x_SOC

static void __iomem *gic_dist_base = IO_ADDRESS(TEGRA_ARM_INT_DIST_BASE);
static u32 gic_affinity[INT_GIC_NR/4];

void tegra_gic_dist_disable(void)
{
	writel(0, gic_dist_base + GIC_DIST_CTRL);
}

void tegra_gic_dist_enable(void)
{
	writel(1, gic_dist_base + GIC_DIST_CTRL);
}

void tegra_gic_disable_affinity(void)
{
	unsigned int i;

	BUG_ON(is_lp_cluster());

	/* The GIC distributor TARGET register is one byte per IRQ. */
	for (i = 32; i < INT_GIC_NR; i += 4) {
		/* Save the affinity. */
		gic_affinity[i/4] = __raw_readl(gic_dist_base +
						GIC_DIST_TARGET + i);

		/* Force this interrupt to CPU0. */
		__raw_writel(0x01010101, gic_dist_base + GIC_DIST_TARGET + i);
	}

	wmb();
}

void tegra_gic_restore_affinity(void)
{
	unsigned int i;

	BUG_ON(is_lp_cluster());

	/* The GIC distributor TARGET register is one byte per IRQ. */
	for (i = 32; i < INT_GIC_NR; i += 4) {
#ifdef CONFIG_BUG
		u32 reg = __raw_readl(gic_dist_base + GIC_DIST_TARGET + i);
		if (reg & 0xFEFEFEFE)
			panic("GIC affinity changed!");
#endif
		/* Restore this interrupt's affinity. */
		__raw_writel(gic_affinity[i/4], gic_dist_base +
			     GIC_DIST_TARGET + i);
	}

	wmb();
}

void tegra_gic_affinity_to_cpu0(void)
{
	unsigned int i;

	BUG_ON(is_lp_cluster());

	for (i = 32; i < INT_GIC_NR; i += 4)
		__raw_writel(0x01010101, gic_dist_base + GIC_DIST_TARGET + i);
	wmb();
}
#endif

static int tegra_gic_notifier(struct notifier_block *self, unsigned long cmd, void *v)
{
	switch (cmd) {
	case CPU_PM_ENTER:
		writel(0x1E0, tegra_gic_cpu_base + GIC_CPU_CTRL);
		break;
	}

	return NOTIFY_OK;
}

static struct notifier_block tegra_gic_notifier_block = {
	.notifier_call = tegra_gic_notifier,
};
#endif

u32 tegra_gic_version(void)
{
	return gic_version;
}

void __init tegra_gic_init(bool is_dt)
{
	u32 midr;

	__asm__("mrc p15, 0, %0, c0, c0, 0\n" : "=r" (midr));

	midr = (midr & 0x0000FFF0) >> 4;

	if (midr == ARM_VERSION_CORTEX_A15)
		tegra_gic_cpu_base = IO_ADDRESS(TEGRA_ARM_PERIF_BASE + 0x2000);
	else
		tegra_gic_cpu_base = IO_ADDRESS(TEGRA_ARM_PERIF_BASE + 0x100);

	if (!is_dt)
		gic_init(0, 29, IO_ADDRESS(TEGRA_ARM_INT_DIST_BASE),
			tegra_gic_cpu_base);

	gic_version = readl(IO_ADDRESS(TEGRA_ARM_INT_DIST_BASE)+0xFE8);
	gic_version = (gic_version & 0xF0) >> 4;

#ifdef CONFIG_PM_SLEEP
	if (gic_version == GIC_V2)
		cpu_pm_register_notifier(&tegra_gic_notifier_block);
#endif
}
