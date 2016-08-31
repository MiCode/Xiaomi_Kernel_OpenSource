/*
 * Copyright (C) 2011 Google, Inc.
 *
 * Author:
 *	Colin Cross <ccross@android.com>
 *
 * Copyright (c) 2010-2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include <linux/kernel.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/irqnr.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/irqchip/arm-gic.h>
#include <linux/syscore_ops.h>
#include <linux/clk/tegra.h>
#include <linux/irqchip.h>
#include <linux/irqchip/arm-gic.h>
#include <linux/irqchip/tegra.h>
#include <linux/cpumask.h>	/* Required by asm/hardware/gic.h */
#include <linux/cpu_pm.h>

/* HACK: will be removed once cpuidle is moved to drivers */
#include "../../arch/arm/mach-tegra/pm.h"

#include "irqchip.h"

#define ICTLR_CPU_IEP_VFIQ	0x08
#define ICTLR_CPU_IEP_FIR	0x14
#define ICTLR_CPU_IEP_FIR_SET	0x18
#define ICTLR_CPU_IEP_FIR_CLR	0x1c

#define ICTLR_CPU_IER		0x20
#define ICTLR_CPU_IER_SET	0x24
#define ICTLR_CPU_IER_CLR	0x28
#define ICTLR_CPU_IEP_CLASS	0x2C

#define ICTLR_COP_IER		0x30
#define ICTLR_COP_IER_SET	0x34
#define ICTLR_COP_IER_CLR	0x38
#define ICTLR_COP_IEP_CLASS	0x3c

#ifdef CONFIG_ARCH_TEGRA_2x_SOC
#define NUM_ICTLRS 4
#else
#define NUM_ICTLRS 5
#endif

#define FIRST_LEGACY_IRQ 32
#define ARM_VERSION_CORTEX_A15	0xC0F


static u32 gic_version;

static void __iomem *gic_dist_base;
static void __iomem *gic_cpu_base;
static u32 gic_affinity[INT_GIC_NR/4];

static int num_ictlrs;

static void __iomem **ictlr_reg_base;

#ifdef CONFIG_PM_SLEEP
static u32 cop_ier[NUM_ICTLRS];
static u32 cop_iep[NUM_ICTLRS];
static u32 cpu_ier[NUM_ICTLRS];
static u32 cpu_iep[NUM_ICTLRS];

static u32 ictlr_wake_mask[NUM_ICTLRS];
#endif

#ifdef CONFIG_FIQ
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
	/* enable FIQ */
	u32 val = readl(gic_cpu_base + GIC_CPU_CTRL);
	val &= ~8; /* pass FIQs through */
	val |= 2; /* enableNS */
	writel(val, gic_cpu_base + GIC_CPU_CTRL);
	tegra_legacy_select_fiq(irq, true);
	tegra_fiq_unmask(irq_get_irq_data(irq));
}

void tegra_fiq_disable(int irq)
{
	tegra_fiq_mask(irq_get_irq_data(irq));
	tegra_legacy_select_fiq(irq, false);
}
#endif /* CONFIG_FIQ */

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
	writel(gic_cpu_ctrl, gic_cpu_base + GIC_CPU_CTRL);
}
#endif

#ifdef CONFIG_PM_SLEEP
int tegra_gic_pending_interrupt(void)
{
	u32 irq = readl(gic_cpu_base + GIC_CPU_HIGHPRI);
	irq &= 0x3FF;

	return irq;
}

#ifndef CONFIG_ARCH_TEGRA_2x_SOC
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
#endif /* CONFIG_ARCH_TEGRA_2x_SOC */

static int tegra_gic_notifier(struct notifier_block *self,
					unsigned long cmd, void *v)
{
	switch (cmd) {
	case CPU_PM_ENTER:
		writel(0x1E0, gic_cpu_base + GIC_CPU_CTRL);
		break;
	}

	return NOTIFY_OK;
}

static struct notifier_block tegra_gic_notifier_block = {
	.notifier_call = tegra_gic_notifier,
};
#endif /* CONFIG_PM_SLEEP */

int tegra_update_lp1_irq_wake(unsigned int irq, bool enable)
{
#ifdef CONFIG_PM_SLEEP
	u8 index;
	u32 mask;

	BUG_ON(irq < FIRST_LEGACY_IRQ ||
		irq >= FIRST_LEGACY_IRQ + num_ictlrs * 32);

	index = ((irq - FIRST_LEGACY_IRQ) / 32);
	mask = BIT((irq - FIRST_LEGACY_IRQ) % 32);
	if (enable)
		ictlr_wake_mask[index] |= mask;
	else
		ictlr_wake_mask[index] &= ~mask;
#endif

	return 0;
}

static inline void tegra_irq_write_mask(unsigned int irq, unsigned long reg)
{
	void __iomem *base;
	u32 mask;

	BUG_ON(irq < FIRST_LEGACY_IRQ ||
		irq >= FIRST_LEGACY_IRQ + num_ictlrs * 32);

	base = ictlr_reg_base[(irq - FIRST_LEGACY_IRQ) / 32];
	mask = BIT((irq - FIRST_LEGACY_IRQ) % 32);

	__raw_writel(mask, base + reg);
}

static void tegra_mask(struct irq_data *d)
{
	if (d->irq < FIRST_LEGACY_IRQ)
		return;

	tegra_irq_write_mask(d->irq, ICTLR_CPU_IER_CLR);
}

static void tegra_unmask(struct irq_data *d)
{
	if (d->irq < FIRST_LEGACY_IRQ)
		return;

	tegra_irq_write_mask(d->irq, ICTLR_CPU_IER_SET);
}

static void tegra_ack(struct irq_data *d)
{
	if (d->irq < FIRST_LEGACY_IRQ)
		return;

	tegra_irq_write_mask(d->irq, ICTLR_CPU_IEP_FIR_CLR);
}

static void tegra_eoi(struct irq_data *d)
{
	if (d->irq < FIRST_LEGACY_IRQ)
		return;

	tegra_irq_write_mask(d->irq, ICTLR_CPU_IEP_FIR_CLR);
}

static int tegra_retrigger(struct irq_data *d)
{
	if (d->irq < FIRST_LEGACY_IRQ)
		return 0;

	tegra_irq_write_mask(d->irq, ICTLR_CPU_IEP_FIR_SET);

	return 1;
}

static int tegra_set_type(struct irq_data *d, unsigned int flow_type)
{
	int wake_size;
	int wake_list[PMC_MAX_WAKE_COUNT];
	int i;
	int err = 0;
	int ret;

	tegra_irq_to_wake(d->irq, wake_list, &wake_size);

	for (i = 0; i < wake_size; i++) {
		ret = tegra_pm_irq_set_wake_type(wake_list[i], flow_type);
		if (ret < 0) {
			pr_err("Set lp0 wake type=%d fail for irq=%d, wake%d ret=%d\n",
				flow_type, d->irq, wake_list[i], ret);
			if (!err)
				err = ret;
		}
	}
	return err;
}


#ifdef CONFIG_PM_SLEEP
/*
 * Caller ensures that tegra_set_wake (irq_set_wake callback)
 * is called for non-gpio wake sources only
 */
static int tegra_set_wake(struct irq_data *d, unsigned int enable)
{
	int ret;
	int wake_size;
	int wake_list[PMC_MAX_WAKE_COUNT];
	int i;
	int err = 0;

	tegra_irq_to_wake(d->irq, wake_list, &wake_size);

	for (i = 0; i < wake_size; i++) {
		/* pmc lp0 wake enable for non-gpio wake sources */
		ret = tegra_pm_irq_set_wake(wake_list[i], enable);
		if (ret < 0) {
			pr_err("Failed lp0 wake %s for irq=%d, wake%d ret=%d\n",
				(enable ? "enable" : "disable"), d->irq,
				wake_list[i], ret);
			if (!err)
				err = ret;
		}
	}

	/* lp1 wake enable for wake sources */
	ret = tegra_update_lp1_irq_wake(d->irq, enable);
	if (ret)
		pr_err("Failed lp1 wake %s for irq=%d\n",
			(enable ? "enable" : "disable"), d->irq);

	return ret;
}

static int tegra_legacy_irq_suspend(void)
{
	unsigned long flags;
	int i;

	local_irq_save(flags);
	for (i = 0; i < NUM_ICTLRS; i++) {
		void __iomem *ictlr = ictlr_reg_base[i];
		/* save interrupt state */
		cpu_ier[i] = readl(ictlr + ICTLR_CPU_IER);
		cpu_iep[i] = readl(ictlr + ICTLR_CPU_IEP_CLASS);
		cop_ier[i] = readl(ictlr + ICTLR_COP_IER);
		cop_iep[i] = readl(ictlr + ICTLR_COP_IEP_CLASS);

		/* disable COP interrupts */
		writel(~0, ictlr + ICTLR_COP_IER_CLR);

		/* disable CPU interrupts */
		writel(~0, ictlr + ICTLR_CPU_IER_CLR);

		/* enable lp1 wake sources */
		writel(ictlr_wake_mask[i], ictlr + ICTLR_CPU_IER_SET);
	}
	local_irq_restore(flags);

	return 0;
}

static void tegra_legacy_irq_resume(void)
{
	unsigned long flags;
	int i;

	local_irq_save(flags);
	for (i = 0; i < NUM_ICTLRS; i++) {
		void __iomem *ictlr = ictlr_reg_base[i];
		writel(cpu_iep[i], ictlr + ICTLR_CPU_IEP_CLASS);
		writel(~0ul, ictlr + ICTLR_CPU_IER_CLR);
		writel(cpu_ier[i], ictlr + ICTLR_CPU_IER_SET);
		writel(cop_iep[i], ictlr + ICTLR_COP_IEP_CLASS);
		writel(~0ul, ictlr + ICTLR_COP_IER_CLR);
		writel(cop_ier[i], ictlr + ICTLR_COP_IER_SET);
	}
	local_irq_restore(flags);
}

static struct syscore_ops tegra_legacy_irq_syscore_ops = {
	.suspend = tegra_legacy_irq_suspend,
	.resume = tegra_legacy_irq_resume,
	.save = tegra_legacy_irq_suspend,
	.restore = tegra_legacy_irq_resume,
};

static int __init tegra_legacy_irq_syscore_init(void)
{
	register_syscore_ops(&tegra_legacy_irq_syscore_ops);

	return 0;
}
#else
#define tegra_set_wake NULL
#endif

void tegra_init_legacy_irq_cop(void)
{
	int i;

	for (i = 0; i < NUM_ICTLRS; i++) {
		void __iomem *ictlr = ictlr_reg_base[i];
		writel(~0, ictlr + ICTLR_COP_IER_CLR);
		writel(0, ictlr + ICTLR_COP_IEP_CLASS);
	}
}

static int __init tegra_gic_of_init(struct device_node *node,
					struct device_node *parent)
{
	int i;
	struct device_node *arm_gic_np =
		of_find_compatible_node(NULL, NULL, "arm,cortex-a15-gic");
	gic_dist_base = of_iomap(arm_gic_np, 0);
	gic_cpu_base = of_iomap(arm_gic_np, 1);
	num_ictlrs = readl_relaxed(gic_dist_base + GIC_DIST_CTR) & 0x1f;
	gic_version = (readl(gic_dist_base + 0xFE8) & 0xF0) >> 4;

	pr_info("the number of interrupt controllers found is %d", num_ictlrs);
	ictlr_reg_base = kzalloc(sizeof(void *) * num_ictlrs, GFP_KERNEL);

	tegra_clocks_init();

	for (i = 0; i < num_ictlrs; i++) {
		ictlr_reg_base[i] = of_iomap(node, i);
		if (!ictlr_reg_base[i]) {
			pr_info("failed to get the right register\n");
			return -EINVAL;
		}
		writel(~0, ictlr_reg_base[i] + ICTLR_CPU_IER_CLR);
		writel(0, ictlr_reg_base[i] + ICTLR_CPU_IEP_CLASS);
		writel(~0, ictlr_reg_base[i] + ICTLR_CPU_IEP_FIR_CLR);
	}

	gic_arch_extn.irq_ack = tegra_ack;
	gic_arch_extn.irq_eoi = tegra_eoi;
	gic_arch_extn.irq_mask = tegra_mask;
	gic_arch_extn.irq_unmask = tegra_unmask;
	gic_arch_extn.irq_retrigger = tegra_retrigger;
	gic_arch_extn.irq_set_type = tegra_set_type;
	gic_arch_extn.irq_set_wake = tegra_set_wake;
	gic_arch_extn.flags = IRQCHIP_MASK_ON_SUSPEND;

#ifdef CONFIG_PM_SLEEP
	tegra_legacy_irq_syscore_init();
	if (gic_version == GIC_V2)
		cpu_pm_register_notifier(&tegra_gic_notifier_block);
#endif

	return 0;
}
IRQCHIP_DECLARE(tegra_gic, "nvidia,tegra-gic", tegra_gic_of_init);
