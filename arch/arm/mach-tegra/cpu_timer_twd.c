/*
 * arch/arch/mach-tegra/cpu_timer_twd.c
 *
 * Copyright (C) 2010 Google, Inc.
 *
 * Author:
 *	Colin Cross <ccross@google.com>
 *
 * Copyright (C) 2013 NVIDIA Corporation
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

#include <linux/init.h>
#include <linux/err.h>
#include <linux/time.h>
#include <linux/io.h>
#include <linux/of.h>

#include <asm/mach/time.h>
#include <asm/cputype.h>
#include <asm/system.h>
#include <asm/smp_twd.h>

#include <mach/irqs.h>

#include "clock.h"
#include "iomap.h"
#include "timer.h"

static DEFINE_TWD_LOCAL_TIMER(twd_local_timer,
			      TEGRA_ARM_PERIF_BASE + 0x600,
			      IRQ_LOCALTIMER);
static void __iomem *tegra_twd_base = IO_ADDRESS(TEGRA_ARM_PERIF_BASE + 0x600);

void __init tegra_cpu_timer_init(void)
{
	struct clk *cpu, *twd_clk;
	int err;

	/* The twd clock is a detached child of the CPU complex clock.
	   Force an update of the twd clock after DVFS has updated the
	   CPU clock rate. */

	twd_clk = tegra_get_clock_by_name("twd");
	BUG_ON(!twd_clk);
	cpu = tegra_get_clock_by_name("cpu");
	err = clk_set_rate(twd_clk, clk_get_rate(cpu));

	if (err)
		pr_err("Failed to set twd clock rate: %d\n", err);
	else
		pr_debug("TWD clock rate: %ld\n", clk_get_rate(twd_clk));
}

int tegra_twd_get_state(struct tegra_twd_context *context)
{
	context->twd_ctrl = readl(tegra_twd_base + TWD_TIMER_CONTROL);
	context->twd_load = readl(tegra_twd_base + TWD_TIMER_LOAD);
	context->twd_cnt = readl(tegra_twd_base + TWD_TIMER_COUNTER);

	return 0;
}

void tegra_twd_suspend(struct tegra_twd_context *context)
{
	context->twd_ctrl = readl(tegra_twd_base + TWD_TIMER_CONTROL);
	context->twd_load = readl(tegra_twd_base + TWD_TIMER_LOAD);
	if ((context->twd_load == 0) &&
	    (context->twd_ctrl & TWD_TIMER_CONTROL_PERIODIC) &&
	    (context->twd_ctrl & (TWD_TIMER_CONTROL_ENABLE |
				  TWD_TIMER_CONTROL_IT_ENABLE))) {
		WARN("%s: TWD enabled but counter was 0\n", __func__);
		context->twd_load = 1;
	}
	__raw_writel(0, tegra_twd_base + TWD_TIMER_CONTROL);
}

void tegra_twd_resume(struct tegra_twd_context *context)
{
	BUG_ON((context->twd_load == 0) &&
	       (context->twd_ctrl & TWD_TIMER_CONTROL_PERIODIC) &&
	       (context->twd_ctrl & (TWD_TIMER_CONTROL_ENABLE |
				     TWD_TIMER_CONTROL_IT_ENABLE)));
	writel(context->twd_load, tegra_twd_base + TWD_TIMER_LOAD);
	writel(context->twd_ctrl, tegra_twd_base + TWD_TIMER_CONTROL);
}

void __init tegra_init_late_timer(void)
{
	int err;

	if (of_have_populated_dt()) {
		twd_local_timer_of_register();
		return;
	}

	err = twd_local_timer_register(&twd_local_timer);
	if (err)
		pr_err("twd_timer_register failed %d\n", err);
}
