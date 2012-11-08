/* Copyright (c) 2012, The Linux Foundation. All rights reserved.
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

#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/irq.h>
#include <asm/fiq.h>
#include <asm/hardware/gic.h>
#include <asm/cacheflush.h>
#include <mach/irqs-8625.h>
#include <mach/socinfo.h>

#include "msm_watchdog.h"

#define MODULE_NAME "MSM7K_FIQ"

struct msm_watchdog_dump msm_dump_cpu_ctx;
static int fiq_counter;
void *msm7k_fiq_stack;

/* Called from the FIQ asm handler */
void msm7k_fiq_handler(void)
{
	struct irq_data *d;
	struct irq_chip *c;

	pr_info("Fiq is received %s\n", __func__);
	fiq_counter++;
	d = irq_get_irq_data(MSM8625_INT_A9_M2A_2);
	c = irq_data_get_irq_chip(d);
	c->irq_mask(d);
	local_irq_disable();

	/* Clear the IRQ from the ENABLE_SET */
	gic_clear_irq_pending(MSM8625_INT_A9_M2A_2);
	local_irq_enable();
	flush_cache_all();
	outer_flush_all();
	return;
}

struct fiq_handler msm7k_fh = {
	.name = MODULE_NAME,
};

static int __init msm_setup_fiq_handler(void)
{
	int ret = 0;

	claim_fiq(&msm7k_fh);
	set_fiq_handler(&msm7k_fiq_start, msm7k_fiq_length);
	msm7k_fiq_stack = (void *)__get_free_pages(GFP_KERNEL,
				THREAD_SIZE_ORDER);
	if (msm7k_fiq_stack == NULL) {
		pr_err("FIQ STACK SETUP IS NOT SUCCESSFUL\n");
		return -ENOMEM;
	}

	fiq_set_type(MSM8625_INT_A9_M2A_2, IRQF_TRIGGER_RISING);
	gic_set_irq_secure(MSM8625_INT_A9_M2A_2);
	enable_irq(MSM8625_INT_A9_M2A_2);
	pr_info("%s : msm7k fiq setup--done\n", __func__);
	return ret;
}

static int __init init7k_fiq(void)
{
	if (!cpu_is_msm8625() && !cpu_is_msm8625q())
		return 0;

	if (msm_setup_fiq_handler())
		pr_err("MSM7K FIQ INIT FAILED\n");

	return 0;
}
late_initcall(init7k_fiq);
