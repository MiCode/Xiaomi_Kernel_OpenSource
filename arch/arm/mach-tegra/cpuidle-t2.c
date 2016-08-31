/*
 * arch/arm/mach-tegra/cpuidle-t2.c
 *
 * CPU idle driver for Tegra2 CPUs
 *
 * Copyright (c) 2010-2013, NVIDIA Corporation.   All rights reserved.
 * Copyright (c) 2011 Google, Inc.
 * Author: Colin Cross <ccross@android.com>
 *         Gary King <gking@nvidia.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include <linux/kernel.h>
#include <linux/cpu.h>
#include <linux/cpuidle.h>
#include <linux/debugfs.h>
#include <linux/delay.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/io.h>
#include <linux/sched.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#include <linux/smp.h>
#include <linux/suspend.h>
#include <linux/tick.h>
#include <linux/cpu_pm.h>
#include <linux/irqchip/tegra.h>

#include <asm/suspend.h>

#include <mach/iomap.h>
#include <mach/irqs.h>

#include "cpuidle.h"
#include "pm.h"
#include "sleep.h"
#include "timer.h"
#include "dvfs.h"

static struct {
	unsigned int cpu_ready_count[2];
	unsigned long long cpu_wants_lp2_time[2];
	unsigned long long in_lp2_time;
	unsigned int both_idle_count;
	unsigned int tear_down_count;
	unsigned int lp2_count;
	unsigned int lp2_completed_count;
	unsigned int lp2_count_bin[32];
	unsigned int lp2_completed_count_bin[32];
	unsigned int lp2_int_count[NR_IRQS];
	unsigned int last_lp2_int_count[NR_IRQS];
} idle_stats;

static inline unsigned int time_to_bin(unsigned int time)
{
	return fls(time);
}

#ifdef CONFIG_SMP

#define CLK_RST_CONTROLLER_CLK_CPU_CMPLX	0x4C
#define CLK_RST_CONTROLLER_RST_CPU_CMPLX_CLR	0x344

static void __iomem *clk_rst = IO_ADDRESS(TEGRA_CLK_RESET_BASE);
static void __iomem *pmc = IO_ADDRESS(TEGRA_PMC_BASE);
static s64 tegra_cpu1_wake_by_time = LLONG_MAX;

static int tegra2_reset_sleeping_cpu(int cpu)
{
	int ret = 0;

	BUG_ON(cpu == 0);
	BUG_ON(cpu == smp_processor_id());
	tegra_pen_lock();

	if (readl(pmc + PMC_SCRATCH41) == CPU_RESETTABLE)
		tegra2_cpu_reset(cpu);
	else
		ret = -EINVAL;

	tegra_pen_unlock();

	return ret;
}

static void tegra2_wake_reset_cpu(int cpu)
{
	u32 reg;

	BUG_ON(cpu == 0);
	BUG_ON(cpu == smp_processor_id());

	tegra_pen_lock();

	tegra2_cpu_clear_resettable();

	/* enable cpu clock on cpu */
	reg = readl(clk_rst + 0x4c);
	writel(reg & ~(1 << (8 + cpu)),
	       clk_rst + CLK_RST_CONTROLLER_CLK_CPU_CMPLX);

	/* take the CPU out of reset */
	reg = 0x1111 << cpu;
	writel(reg, clk_rst +
	       CLK_RST_CONTROLLER_RST_CPU_CMPLX_CLR);

	/* unhalt the cpu */
	flowctrl_writel(0, FLOW_CTRL_HALT_CPU(1));

	tegra_pen_unlock();
}

static int tegra2_reset_other_cpus(int cpu)
{
	int i;
	int ret = 0;

	BUG_ON(cpu != 0);

	for_each_online_cpu(i) {
		if (i != cpu) {
			if (tegra2_reset_sleeping_cpu(i)) {
				ret = -EBUSY;
				break;
			}
		}
	}

	if (ret) {
		for_each_online_cpu(i) {
			if (i != cpu)
				tegra2_wake_reset_cpu(i);
		}
		return ret;
	}

	return 0;
}
#else
static void tegra2_wake_reset_cpu(int cpu)
{
}

static int tegra2_reset_other_cpus(int cpu)
{
	return 0;
}
#endif

bool tegra2_lp2_is_allowed(struct cpuidle_device *dev,
			struct cpuidle_state *state)
{
	s64 request = ktime_to_us(tick_nohz_get_sleep_length());

	if (request < state->target_residency) {
		/* Not enough time left to enter LP2 */
		return false;
	}

	return true;
}

static int tegra2_idle_lp2_cpu_0(struct cpuidle_device *dev,
			   struct cpuidle_state *state, s64 request)
{
	ktime_t entry_time;
	ktime_t exit_time;
	s64 wake_time;
	bool sleep_completed = false;
	int bin;
	int i;

	while (tegra2_cpu_is_resettable_soon())
		cpu_relax();

	if (tegra2_reset_other_cpus(dev->cpu))
		return 0;

	idle_stats.both_idle_count++;

	if (request < state->target_residency) {
		cpu_do_idle();
		return -EBUSY;
	}

	/* LP2 entry time */
	entry_time = ktime_get();

	/* LP2 initial targeted wake time */
	wake_time = ktime_to_us(entry_time) + request;

	/* CPU0 must wake up before CPU1. */
	smp_rmb();
	wake_time = min_t(s64, wake_time, tegra_cpu1_wake_by_time);

	/* LP2 actual targeted wake time */
	request = wake_time - ktime_to_us(entry_time);
	BUG_ON(wake_time < 0LL);

	idle_stats.tear_down_count++;
	entry_time = ktime_get();
	tegra_dvfs_rail_off(tegra_cpu_rail, entry_time);

	if (request > state->target_residency) {
		s64 sleep_time = request - tegra_pg_exit_latency;

		bin = time_to_bin((u32)request / 1000);
		idle_stats.lp2_count++;
		idle_stats.lp2_count_bin[bin]++;

		clockevents_notify(CLOCK_EVT_NOTIFY_BROADCAST_ENTER, &dev->cpu);

		if (tegra_idle_power_down_last(sleep_time, 0) == 0)
			sleep_completed = true;
		else {
			int irq = tegra_gic_pending_interrupt();
			idle_stats.lp2_int_count[irq]++;
		}

		clockevents_notify(CLOCK_EVT_NOTIFY_BROADCAST_EXIT, &dev->cpu);
	}

	for_each_online_cpu(i) {
		if (i != dev->cpu)
			tegra2_wake_reset_cpu(i);
	}

	exit_time = ktime_get();
	tegra_dvfs_rail_on(tegra_cpu_rail, exit_time);

	if (sleep_completed) {
		/*
		 * Stayed in LP2 for the full time until the next tick,
		 * adjust the exit latency based on measurement
		 */
		s64 actual_time = ktime_to_us(ktime_sub(exit_time, entry_time));
		long offset = (long)(actual_time - request);
		int latency = tegra_pg_exit_latency + offset / 16;
		latency = clamp(latency, 0, 10000);
		tegra_pg_exit_latency = latency;
		smp_wmb();

		idle_stats.lp2_completed_count++;
		idle_stats.lp2_completed_count_bin[bin]++;
		idle_stats.in_lp2_time += actual_time;

		pr_debug("%lld %lld %ld %d\n", request, actual_time,
			offset, bin);
	}

	return 0;
}

static bool tegra2_idle_lp2_cpu_1(struct cpuidle_device *dev,
			   struct cpuidle_state *state, s64 request)
{
#ifdef CONFIG_SMP
	struct tegra_twd_context twd_context;

	if (request < tegra_pg_exit_latency) {
		tegra2_cpu_clear_resettable();
		cpu_do_idle();
		return false;
	}

	/* Save time this CPU must be awakened by. */
	tegra_cpu1_wake_by_time = ktime_to_us(ktime_get()) + request;
	smp_wmb();

	clockevents_notify(CLOCK_EVT_NOTIFY_BROADCAST_ENTER, &dev->cpu);

	tegra_twd_suspend(&twd_context);

	cpu_suspend(PHYS_OFFSET - PAGE_OFFSET, tegra2_finish_sleep_cpu_secondary);

	tegra2_cpu_clear_resettable();

	tegra_cpu1_wake_by_time = LLONG_MAX;

	tegra_twd_resume(&twd_context);

	clockevents_notify(CLOCK_EVT_NOTIFY_BROADCAST_EXIT, &dev->cpu);
#endif

	/* FIXME: Do we really know whether we went into LP2 here? */
	return true;
}

bool tegra2_idle_lp2(struct cpuidle_device *dev,
			struct cpuidle_state *state)
{
	s64 request = ktime_to_us(tick_nohz_get_sleep_length());
	bool last_cpu = tegra_set_cpu_in_pd(dev->cpu);
	bool entered_lp2 = false;

	cpu_pm_enter();

	if (dev->cpu == 0) {
		if (last_cpu) {
			if (tegra2_idle_lp2_cpu_0(dev, state, request) < 0) {
				int i;
				for_each_online_cpu(i) {
					if (i != dev->cpu)
						tegra2_wake_reset_cpu(i);
				}
			} else
				entered_lp2 = true;
		} else {
			cpu_do_idle();
		}
	} else {
		BUG_ON(last_cpu);
		entered_lp2 = tegra2_idle_lp2_cpu_1(dev, state, request);
	}

	cpu_pm_exit();
	tegra_clear_cpu_in_pd(dev->cpu);

	return entered_lp2;
}

void tegra2_cpu_idle_stats_lp2_ready(unsigned int cpu)
{
	idle_stats.cpu_ready_count[cpu]++;
}

void tegra2_cpu_idle_stats_lp2_time(unsigned int cpu, s64 us)
{
	idle_stats.cpu_wants_lp2_time[cpu] += us;
}

#ifdef CONFIG_DEBUG_FS
int tegra2_lp2_debug_show(struct seq_file *s, void *data)
{
	int bin;
	int i;
	seq_printf(s, "                                    cpu0     cpu1\n");
	seq_printf(s, "-------------------------------------------------\n");
	seq_printf(s, "cpu ready:                      %8u %8u\n",
		idle_stats.cpu_ready_count[0],
		idle_stats.cpu_ready_count[1]);
	seq_printf(s, "both idle:      %8u        %7u%% %7u%%\n",
		idle_stats.both_idle_count,
		idle_stats.both_idle_count * 100 /
			(idle_stats.cpu_ready_count[0] ?: 1),
		idle_stats.both_idle_count * 100 /
			(idle_stats.cpu_ready_count[1] ?: 1));
	seq_printf(s, "tear down:      %8u %7u%%\n", idle_stats.tear_down_count,
		idle_stats.tear_down_count * 100 /
			(idle_stats.both_idle_count ?: 1));
	seq_printf(s, "lp2:            %8u %7u%%\n", idle_stats.lp2_count,
		idle_stats.lp2_count * 100 /
			(idle_stats.both_idle_count ?: 1));
	seq_printf(s, "lp2 completed:  %8u %7u%%\n",
		idle_stats.lp2_completed_count,
		idle_stats.lp2_completed_count * 100 /
			(idle_stats.lp2_count ?: 1));

	seq_printf(s, "\n");
	seq_printf(s, "cpu ready time:                 %8llu %8llu ms\n",
		div64_u64(idle_stats.cpu_wants_lp2_time[0], 1000),
		div64_u64(idle_stats.cpu_wants_lp2_time[1], 1000));
	seq_printf(s, "lp2 time:       %8llu ms     %7d%% %7d%%\n",
		div64_u64(idle_stats.in_lp2_time, 1000),
		(int)div64_u64(idle_stats.in_lp2_time * 100,
			idle_stats.cpu_wants_lp2_time[0] ?: 1),
		(int)div64_u64(idle_stats.in_lp2_time * 100,
			idle_stats.cpu_wants_lp2_time[1] ?: 1));

	seq_printf(s, "\n");
	seq_printf(s, "%19s %8s %8s %8s\n", "", "lp2", "comp", "%");
	seq_printf(s, "-------------------------------------------------\n");
	for (bin = 0; bin < 32; bin++) {
		if (idle_stats.lp2_count_bin[bin] == 0)
			continue;
		seq_printf(s, "%6u - %6u ms: %8u %8u %7u%%\n",
			1 << (bin - 1), 1 << bin,
			idle_stats.lp2_count_bin[bin],
			idle_stats.lp2_completed_count_bin[bin],
			idle_stats.lp2_completed_count_bin[bin] * 100 /
				idle_stats.lp2_count_bin[bin]);
	}

	seq_printf(s, "\n");
	seq_printf(s, "%3s %20s %6s %10s\n",
		"int", "name", "count", "last count");
	seq_printf(s, "--------------------------------------------\n");
	for (i = 0; i < NR_IRQS; i++) {
		if (idle_stats.lp2_int_count[i] == 0)
			continue;
		seq_printf(s, "%3d %20s %6d %10d\n",
			i, irq_to_desc(i)->action ?
				irq_to_desc(i)->action->name ?: "???" : "???",
			idle_stats.lp2_int_count[i],
			idle_stats.lp2_int_count[i] -
				idle_stats.last_lp2_int_count[i]);
		idle_stats.last_lp2_int_count[i] = idle_stats.lp2_int_count[i];
	};
	return 0;
}
#endif

int __init tegra2_cpuidle_init_soc(struct tegra_cpuidle_ops *idle_ops)
{
	struct tegra_cpuidle_ops ops = {
		tegra2_idle_lp2,
		tegra2_cpu_idle_stats_lp2_ready,
		tegra2_cpu_idle_stats_lp2_time,
		tegra2_lp2_is_allowed,
#ifdef CONFIG_DEBUG_FS
		tegra2_lp2_debug_show
#endif
	};

	*idle_ops = ops;

	return 0;
}
