// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2011, 2014-2016, 2018, 2020-2021, The Linux Foundation. All rights reserved.
 */

#include <linux/cpuidle.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>

#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/irqdomain.h>
#include <linux/irqchip/arm-gic-v3.h>
#include <trace/hooks/cpuidle_psci.h>
#include <trace/hooks/gic.h>

int msm_show_resume_irq_mask;
module_param_named(debug_mask, msm_show_resume_irq_mask, int, 0664);

static void msm_show_resume_irqs(void *data, struct gic_chip_data *gic_data)
{
	struct irq_domain *domain;
	void __iomem *base;
	unsigned int i;
	u32 enabled;
	u32 pending[32];
	u32 gic_line_nr;
	u32 typer;

	if (!msm_show_resume_irq_mask)
		return;

	base = gic_data->dist_base;
	domain = gic_data->domain;

	typer = readl_relaxed(base + GICD_TYPER);
	gic_line_nr = min(GICD_TYPER_SPIS(typer), 1023u);

	for (i = 0; i * 32 < gic_line_nr; i++) {
		enabled = readl_relaxed(base + GICD_ICENABLER + i * 4);
		pending[i] = readl_relaxed(base + GICD_ISPENDR + i * 4);
		pending[i] &= enabled;
	}

	for (i = find_first_bit((unsigned long *)pending, gic_line_nr);
	     i < gic_line_nr;
	     i = find_next_bit((unsigned long *)pending, gic_line_nr, i + 1)) {
		unsigned int irq = irq_find_mapping(domain, i);
		struct irq_desc *desc = irq_to_desc(irq);
		const char *name = "null";

		if (i < 32)
			continue;

		if (desc == NULL)
			name = "stray irq";
		else if (desc->action && desc->action->name)
			name = desc->action->name;

		pr_warn("%s: IRQ %d HWIRQ %u triggered %s\n", __func__, irq, i, name);
	}
}

static atomic_t cpus_in_s2idle;

static void gic_s2idle_enter(void *unused, struct cpuidle_device *dev, bool s2idle)
{
	if (!s2idle)
		return;

	atomic_inc(&cpus_in_s2idle);
}

static void gic_s2idle_exit(void *unused, struct cpuidle_device *dev, bool s2idle)
{
	if (!s2idle)
		return;

	if (atomic_read(&cpus_in_s2idle) == num_online_cpus())
		gic_resume();

	atomic_dec(&cpus_in_s2idle);
}

static int __init msm_show_resume_irq_init(void)
{
	register_trace_prio_android_vh_cpuidle_psci_enter(gic_s2idle_enter, NULL, INT_MAX);
	register_trace_prio_android_vh_cpuidle_psci_exit(gic_s2idle_exit, NULL, INT_MAX);

	return register_trace_android_vh_gic_resume(msm_show_resume_irqs, NULL);
}

#if IS_MODULE(CONFIG_QCOM_SHOW_RESUME_IRQ)
module_init(msm_show_resume_irq_init);
#else
pure_initcall(msm_show_resume_irq_init);
#endif

MODULE_DESCRIPTION("Qualcomm Technologies, Inc. IRQ Logging driver");
MODULE_LICENSE("GPL v2");

