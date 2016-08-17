/*
 * arch/arm/mach-tegra/cpuidle-t11x.c
 *
 * CPU idle driver for Tegra11x CPUs
 *
 * Copyright (c) 2012-2013, NVIDIA CORPORATION.  All rights reserved.
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
#include <linux/hrtimer.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/io.h>
#include <linux/ratelimit.h>
#include <linux/sched.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#include <linux/smp.h>
#include <linux/suspend.h>
#include <linux/tick.h>
#include <linux/clk.h>
#include <linux/cpu_pm.h>
#include <linux/module.h>

#include <asm/cacheflush.h>
#include <asm/hardware/gic.h>
#include <asm/localtimer.h>
#include <asm/suspend.h>
#include <asm/cputype.h>

#include <mach/iomap.h>
#include <mach/irqs.h>
#include <mach/hardware.h>

#include <trace/events/power.h>

#include "clock.h"
#include "cpuidle.h"
#include "dvfs.h"
#include "fuse.h"
#include "gic.h"
#include "pm.h"
#include "reset.h"
#include "sleep.h"
#include "timer.h"
#include "fuse.h"

#define CLK_RST_CONTROLLER_CPU_CMPLX_STATUS \
	(IO_ADDRESS(TEGRA_CLK_RESET_BASE) + 0x470)
#define PMC_POWERGATE_STATUS \
	(IO_ADDRESS(TEGRA_PMC_BASE) + 0x038)

#define ARCH_TIMER_CTRL_ENABLE          (1 << 0)
#define ARCH_TIMER_CTRL_IT_MASK         (1 << 1)

#ifdef CONFIG_SMP
static s64 tegra_cpu_wake_by_time[4] = {
	LLONG_MAX, LLONG_MAX, LLONG_MAX, LLONG_MAX };
#endif

static ulong cpu_power_gating_in_idle __read_mostly = 0x1f;
module_param(cpu_power_gating_in_idle, ulong, 0644);

static bool slow_cluster_power_gating_noncpu __read_mostly;
module_param(slow_cluster_power_gating_noncpu, bool, 0644);

static uint fast_cluster_power_down_mode __read_mostly;
module_param(fast_cluster_power_down_mode, uint, 0644);

static struct clk *cpu_clk_for_dvfs;

static int pd_exit_latencies[5];

static struct {
	unsigned int cpu_ready_count[5];
	unsigned int tear_down_count[5];
	unsigned long long cpu_wants_pd_time[5];
	unsigned long long cpu_pg_time[5];
	unsigned long long rail_pd_time;
	unsigned long long c0nc_pg_time;
	unsigned long long c1nc_pg_time;
	unsigned int rail_gating_count;
	unsigned int rail_gating_bin[32];
	unsigned int rail_gating_done_count;
	unsigned int rail_gating_done_count_bin[32];
	unsigned int c0nc_gating_count;
	unsigned int c0nc_gating_bin[32];
	unsigned int c0nc_gating_done_count;
	unsigned int c0nc_gating_done_count_bin[32];
	unsigned int c1nc_gating_count;
	unsigned int c1nc_gating_bin[32];
	unsigned int c1nc_gating_done_count;
	unsigned int c1nc_gating_done_count_bin[32];
	unsigned int pd_int_count[NR_IRQS];
	unsigned int last_pd_int_count[NR_IRQS];
	unsigned int clk_gating_vmin;
} idle_stats;

static inline unsigned int time_to_bin(unsigned int time)
{
	return fls(time);
}

static inline void tegra_irq_unmask(int irq)
{
	struct irq_data *data = irq_get_irq_data(irq);
	data->chip->irq_unmask(data);
}

static inline unsigned int cpu_number(unsigned int n)
{
	return is_lp_cluster() ? 4 : n;
}

void tegra11x_cpu_idle_stats_pd_ready(unsigned int cpu)
{
	idle_stats.cpu_ready_count[cpu_number(cpu)]++;
}

void tegra11x_cpu_idle_stats_pd_time(unsigned int cpu, s64 us)
{
	idle_stats.cpu_wants_pd_time[cpu_number(cpu)] += us;
}

/* Allow rail off only if all secondary CPUs are power gated, and no
   rail update is in progress */
static bool tegra_rail_off_is_allowed(void)
{
	u32 rst = readl(CLK_RST_CONTROLLER_CPU_CMPLX_STATUS);
	u32 pg = readl(PMC_POWERGATE_STATUS) >> 8;

	if (((rst & 0xE) != 0xE) || ((pg & 0xE) != 0))
		return false;

	if (tegra_dvfs_rail_updating(cpu_clk_for_dvfs))
		return false;

	return true;
}

bool tegra11x_pd_is_allowed(struct cpuidle_device *dev,
	struct cpuidle_state *state)
{
	s64 request;

	if (!cpumask_test_cpu(cpu_number(dev->cpu),
				to_cpumask(&cpu_power_gating_in_idle)))
		return false;

	if (tegra_cpu_timer_get_remain(&request))
		return false;

	if (state->exit_latency != pd_exit_latencies[cpu_number(dev->cpu)]) {
		/* possible on the 1st entry after cluster switch*/
		state->exit_latency = pd_exit_latencies[cpu_number(dev->cpu)];
		tegra_pd_update_target_residency(state);
	}
	if (request < state->target_residency) {
		/* Not enough time left to enter LP2 */
		return false;
	}

	return true;
}

static inline void tegra11_irq_restore_affinity(void)
{
#ifdef CONFIG_SMP
	/* Disable the distributor. */
	tegra_gic_dist_disable();

	/* Restore the other CPU's interrupt affinity. */
	tegra_gic_restore_affinity();

	/* Re-enable the distributor. */
	tegra_gic_dist_enable();
#endif
}

static bool tegra_cpu_cluster_power_down(struct cpuidle_device *dev,
			   struct cpuidle_state *state, s64 request)
{
	ktime_t entry_time;
	ktime_t exit_time;
	bool sleep_completed = false;
	bool multi_cpu_entry = false;
	int bin;
	unsigned int flag = 0;
	s64 sleep_time;

	/* LP2 entry time */
	entry_time = ktime_get();

	if (request < state->target_residency) {
		/* Not enough time left to enter LP2 */
		cpu_do_idle();
		return false;
	}

#ifdef CONFIG_SMP
	multi_cpu_entry = !is_lp_cluster() && (num_online_cpus() > 1);
	if (multi_cpu_entry) {
		s64 wake_time;
		unsigned int i;

		/* Disable the distributor -- this is the only way to
		   prevent the other CPUs from responding to interrupts
		   and potentially fiddling with the distributor
		   registers while we're fiddling with them. */
		tegra_gic_dist_disable();

		/* Did an interrupt come in for another CPU before we
		   could disable the distributor? */
		if (!tegra_rail_off_is_allowed()) {
			/* Yes, re-enable the distributor and clock gating. */
			tegra_gic_dist_enable();
			cpu_do_idle();
			return false;
		}

		/* LP2 initial targeted wake time */
		wake_time = ktime_to_us(entry_time) + request;

		/* CPU0 must wake up before any of the other CPUs. */
		smp_rmb();
		for (i = 1; i < CONFIG_NR_CPUS; i++)
			wake_time = min_t(s64, wake_time,
				tegra_cpu_wake_by_time[i]);

		/* LP2 actual targeted wake time */
		request = wake_time - ktime_to_us(entry_time);
		BUG_ON(wake_time < 0LL);

		if (request < state->target_residency) {
			/* Not enough time left to enter LP2 */
			tegra_gic_dist_enable();
			cpu_do_idle();
			return false;
		}

		/* Cancel power gating wake timers for all secondary CPUs */
		tegra_pd_timer_cancel_secondary();

		/* Save and disable the affinity setting for the other
		   CPUs and route all interrupts to CPU0. */
		tegra_gic_disable_affinity();

		/* Re-enable the distributor. */
		tegra_gic_dist_enable();
	}
#endif
	cpu_pm_enter();

	sleep_time = request -
		pd_exit_latencies[cpu_number(dev->cpu)];

	bin = time_to_bin((u32)request / 1000);
	idle_stats.tear_down_count[cpu_number(dev->cpu)]++;

	clockevents_notify(CLOCK_EVT_NOTIFY_BROADCAST_ENTER, &dev->cpu);
	if (is_lp_cluster()) {
		/* here we are not supporting emulation mode, for now */
		flag = TEGRA_POWER_CLUSTER_PART_NONCPU;
		idle_stats.c1nc_gating_count++;
		idle_stats.c1nc_gating_bin[bin]++;
	} else {
		tegra_dvfs_rail_off(tegra_cpu_rail, entry_time);
		flag = (fast_cluster_power_down_mode
			<< TEGRA_POWER_CLUSTER_PART_SHIFT)
			& TEGRA_POWER_CLUSTER_PART_MASK;

		if (((request < tegra_min_residency_crail()) &&
			(flag != TEGRA_POWER_CLUSTER_PART_MASK)) &&
			((fast_cluster_power_down_mode &
			TEGRA_POWER_CLUSTER_FORCE_MASK) == 0))
			flag = TEGRA_POWER_CLUSTER_PART_NONCPU;

		if (flag == TEGRA_POWER_CLUSTER_PART_CRAIL) {
			idle_stats.rail_gating_count++;
			idle_stats.rail_gating_bin[bin]++;
		} else if (flag == TEGRA_POWER_CLUSTER_PART_NONCPU) {
			idle_stats.c0nc_gating_count++;
			idle_stats.c0nc_gating_bin[bin]++;
		}
	}

	if (tegra_idle_power_down_last(sleep_time, flag) == 0)
		sleep_completed = true;
	else {
		int irq = tegra_gic_pending_interrupt();
		idle_stats.pd_int_count[irq]++;
	}

	clockevents_notify(CLOCK_EVT_NOTIFY_BROADCAST_EXIT, &dev->cpu);
	exit_time = ktime_get();
	if (!is_lp_cluster())
		tegra_dvfs_rail_on(tegra_cpu_rail, exit_time);

	if (flag == TEGRA_POWER_CLUSTER_PART_CRAIL)
		idle_stats.rail_pd_time +=
			ktime_to_us(ktime_sub(exit_time, entry_time));
	else if (flag == TEGRA_POWER_CLUSTER_PART_NONCPU) {
		if (is_lp_cluster())
			idle_stats.c1nc_pg_time +=
				ktime_to_us(ktime_sub(exit_time, entry_time));
		else
			idle_stats.c0nc_pg_time +=
				ktime_to_us(ktime_sub(exit_time, entry_time));
	}

	if (multi_cpu_entry)
		tegra11_irq_restore_affinity();

	if (sleep_completed) {
		/*
		 * Stayed in LP2 for the full time until the next tick,
		 * adjust the exit latency based on measurement
		 */
		int offset = ktime_to_us(ktime_sub(exit_time, entry_time))
			- request;
		int latency = pd_exit_latencies[cpu_number(dev->cpu)] +
			offset / 16;
		latency = clamp(latency, 0, 10000);
		pd_exit_latencies[cpu_number(dev->cpu)] = latency;
		state->exit_latency = latency;		/* for idle governor */
		smp_wmb();

		if (flag == TEGRA_POWER_CLUSTER_PART_CRAIL) {
			idle_stats.rail_gating_done_count++;
			idle_stats.rail_gating_done_count_bin[bin]++;
		} else if (flag == TEGRA_POWER_CLUSTER_PART_NONCPU) {
			if (is_lp_cluster()) {
				idle_stats.c1nc_gating_done_count++;
				idle_stats.c1nc_gating_done_count_bin[bin]++;
			} else {
				idle_stats.c0nc_gating_done_count++;
				idle_stats.c0nc_gating_done_count_bin[bin]++;
			}
		}

		pr_debug("%lld %lld %d %d\n", request,
			ktime_to_us(ktime_sub(exit_time, entry_time)),
			offset, bin);
	}

	cpu_pm_exit();

	return true;
}

static bool tegra_cpu_core_power_down(struct cpuidle_device *dev,
			   struct cpuidle_state *state, s64 request)
{
#ifdef CONFIG_SMP
	s64 sleep_time;
	u32 cntp_tval;
	u32 cntfrq;
	ktime_t entry_time;
	bool sleep_completed = false;
	struct tick_sched *ts = tick_get_tick_sched(dev->cpu);
	unsigned int cpu = cpu_number(dev->cpu);

	if ((tegra_cpu_timer_get_remain(&request) == -ETIME) ||
		(request <= state->target_residency) || (!ts) ||
		(ts->nohz_mode == NOHZ_MODE_INACTIVE) ||
		!tegra_is_cpu_wake_timer_ready(dev->cpu)) {
		/*
		 * Not enough time left to enter LP2, or wake timer not ready
		 */
		cpu_do_idle();
		return false;
	}

#ifdef CONFIG_TEGRA_LP2_CPU_TIMER
	asm volatile("mrc p15, 0, %0, c14, c0, 0" : "=r" (cntfrq));
	cntp_tval = (request - state->exit_latency) * (cntfrq / 1000000);
	asm volatile("mcr p15, 0, %0, c14, c2, 0" : : "r"(cntp_tval));
#endif
	cpu_pm_enter();

#if !defined(CONFIG_TEGRA_LP2_CPU_TIMER)
	sleep_time = request - state->exit_latency;
	clockevents_notify(CLOCK_EVT_NOTIFY_BROADCAST_ENTER, &dev->cpu);
	tegra_pd_set_trigger(sleep_time);
#endif
	idle_stats.tear_down_count[cpu]++;

	entry_time = ktime_get();

	/* Save time this CPU must be awakened by. */
	tegra_cpu_wake_by_time[dev->cpu] = ktime_to_us(entry_time) + request;
	smp_wmb();

#ifdef CONFIG_TRUSTED_FOUNDATIONS
	if ((cpu == 0) || (cpu == 4)) {
		tegra_generic_smc(0xFFFFFFFC, 0xFFFFFFE7,
				(TEGRA_RESET_HANDLER_BASE +
				tegra_cpu_reset_handler_offset));
	}
#endif
	cpu_suspend(0, tegra3_sleep_cpu_secondary_finish);

	tegra_cpu_wake_by_time[dev->cpu] = LLONG_MAX;

#ifdef CONFIG_TEGRA_LP2_CPU_TIMER
	asm volatile("mrc p15, 0, %0, c14, c2, 0" : "=r" (cntp_tval));
	if ((s32)cntp_tval <= 0)
		sleep_completed = true;
#else
	sleep_completed = !tegra_pd_timer_remain();
	tegra_pd_set_trigger(0);
	clockevents_notify(CLOCK_EVT_NOTIFY_BROADCAST_EXIT, &dev->cpu);
#endif
	sleep_time = ktime_to_us(ktime_sub(ktime_get(), entry_time));
	idle_stats.cpu_pg_time[cpu] += sleep_time;
	if (sleep_completed) {
		/*
		 * Stayed in LP2 for the full time until timer expires,
		 * adjust the exit latency based on measurement
		 */
		int offset = sleep_time - request;
		int latency = pd_exit_latencies[cpu] +
			offset / 16;
		latency = clamp(latency, 0, 10000);
		pd_exit_latencies[cpu] = latency;
		state->exit_latency = latency;		/* for idle governor */
		smp_wmb();
	}
#endif
	cpu_pm_exit();

	return true;
}

bool tegra11x_idle_power_down(struct cpuidle_device *dev,
			   struct cpuidle_state *state)
{
	bool power_down;
	bool cpu_gating_only = false;
	bool clkgt_at_vmin = false;
	bool power_gating_cpu_only = true;
	int status = -1;
	unsigned long rate;
	s64 request;

	if (tegra_cpu_timer_get_remain(&request)) {
		cpu_do_idle();
		return false;
	}

	tegra_set_cpu_in_pd(dev->cpu);
	cpu_gating_only = (((fast_cluster_power_down_mode
			<< TEGRA_POWER_CLUSTER_PART_SHIFT)
			& TEGRA_POWER_CLUSTER_PART_MASK) == 0);

	if (is_lp_cluster()) {
		if (slow_cluster_power_gating_noncpu &&
			(request > tegra_min_residency_ncpu()))
				power_gating_cpu_only = false;
		else
			power_gating_cpu_only = true;
	} else {
		if (num_online_cpus() > 1)
			power_gating_cpu_only = true;
		else {
			if (tegra_dvfs_rail_updating(cpu_clk_for_dvfs))
				clkgt_at_vmin = false;
			else if (tegra_force_clkgt_at_vmin ==
					TEGRA_CPUIDLE_FORCE_DO_CLKGT_VMIN)
				clkgt_at_vmin = true;
			else if (tegra_force_clkgt_at_vmin ==
					TEGRA_CPUIDLE_FORCE_NO_CLKGT_VMIN)
				clkgt_at_vmin = false;
			else if ((request >= tegra_min_residency_vmin_fmin()) &&
				 ((request < tegra_min_residency_ncpu()) ||
				   cpu_gating_only))
				clkgt_at_vmin = true;

			if (!cpu_gating_only && tegra_rail_off_is_allowed()) {
				if (fast_cluster_power_down_mode &
						TEGRA_POWER_CLUSTER_FORCE_MASK)
					power_gating_cpu_only = false;
				else if (request >
						tegra_min_residency_ncpu())
					power_gating_cpu_only = false;
				else
					power_gating_cpu_only = true;
			} else
				power_gating_cpu_only = true;
		}
	}

	if (clkgt_at_vmin) {
		rate = 0;
		status = tegra_cpu_g_idle_rate_exchange(&rate);
		if (!status) {
			idle_stats.clk_gating_vmin++;
			cpu_do_idle();
			tegra_cpu_g_idle_rate_exchange(&rate);
			power_down = true;
		} else
			power_down = tegra_cpu_core_power_down(dev, state,
								request);
	} else if (!power_gating_cpu_only) {
		if (is_lp_cluster()) {
			rate = ULONG_MAX;
			status = tegra_cpu_lp_idle_rate_exchange(&rate);
		}

		power_down = tegra_cpu_cluster_power_down(dev, state, request);

		/* restore cpu clock after cluster power ungating */
		if (status == 0)
			tegra_cpu_lp_idle_rate_exchange(&rate);
	} else
		power_down = tegra_cpu_core_power_down(dev, state, request);

	tegra_clear_cpu_in_pd(dev->cpu);

	return power_down;
}

#ifdef CONFIG_DEBUG_FS
int tegra11x_pd_debug_show(struct seq_file *s, void *data)
{
	int bin;
	int i;
	seq_printf(s, "                                    cpu0     cpu1     cpu2     cpu3     cpulp\n");
	seq_printf(s, "-----------------------------------------------------------------------------\n");
	seq_printf(s, "cpu ready:                      %8u %8u %8u %8u %8u\n",
		idle_stats.cpu_ready_count[0],
		idle_stats.cpu_ready_count[1],
		idle_stats.cpu_ready_count[2],
		idle_stats.cpu_ready_count[3],
		idle_stats.cpu_ready_count[4]);
	seq_printf(s, "tear down:                      %8u %8u %8u %8u %8u\n",
		idle_stats.tear_down_count[0],
		idle_stats.tear_down_count[1],
		idle_stats.tear_down_count[2],
		idle_stats.tear_down_count[3],
		idle_stats.tear_down_count[4]);
	seq_printf(s, "clk gating @ Vmin count:      %8u\n",
		idle_stats.clk_gating_vmin);
	seq_printf(s, "rail gating count:      %8u\n",
		idle_stats.rail_gating_count);
	seq_printf(s, "rail gating completed:  %8u %7u%%\n",
		idle_stats.rail_gating_done_count,
		idle_stats.rail_gating_done_count * 100 /
			(idle_stats.rail_gating_count ?: 1));

	seq_printf(s, "c0nc gating count:      %8u\n",
		idle_stats.c0nc_gating_count);
	seq_printf(s, "c0nc gating completed:  %8u %7u%%\n",
		idle_stats.c0nc_gating_done_count,
		idle_stats.c0nc_gating_done_count * 100 /
			(idle_stats.c0nc_gating_count ?: 1));

	seq_printf(s, "c1nc gating count:      %8u\n",
		idle_stats.c1nc_gating_count);
	seq_printf(s, "c1nc gating completed:  %8u %7u%%\n",
		idle_stats.c1nc_gating_done_count,
		idle_stats.c1nc_gating_done_count * 100 /
			(idle_stats.c1nc_gating_count ?: 1));

	seq_printf(s, "\n");
	seq_printf(s, "cpu ready time:                 " \
			"%8llu %8llu %8llu %8llu %8llu ms\n",
		div64_u64(idle_stats.cpu_wants_pd_time[0], 1000),
		div64_u64(idle_stats.cpu_wants_pd_time[1], 1000),
		div64_u64(idle_stats.cpu_wants_pd_time[2], 1000),
		div64_u64(idle_stats.cpu_wants_pd_time[3], 1000),
		div64_u64(idle_stats.cpu_wants_pd_time[4], 1000));

	seq_printf(s, "cpu power gating time:          " \
			"%8llu %8llu %8llu %8llu %8llu ms\n",
		div64_u64(idle_stats.cpu_pg_time[0], 1000),
		div64_u64(idle_stats.cpu_pg_time[1], 1000),
		div64_u64(idle_stats.cpu_pg_time[2], 1000),
		div64_u64(idle_stats.cpu_pg_time[3], 1000),
		div64_u64(idle_stats.cpu_pg_time[4], 1000));

	seq_printf(s, "power gated %%:                 " \
			"%7d%% %7d%% %7d%% %7d%% %7d%%\n",
		(int)(idle_stats.cpu_wants_pd_time[0] ?
			div64_u64(idle_stats.cpu_pg_time[0] * 100,
			idle_stats.cpu_wants_pd_time[0]) : 0),
		(int)(idle_stats.cpu_wants_pd_time[1] ?
			div64_u64(idle_stats.cpu_pg_time[1] * 100,
			idle_stats.cpu_wants_pd_time[1]) : 0),
		(int)(idle_stats.cpu_wants_pd_time[2] ?
			div64_u64(idle_stats.cpu_pg_time[2] * 100,
			idle_stats.cpu_wants_pd_time[2]) : 0),
		(int)(idle_stats.cpu_wants_pd_time[3] ?
			div64_u64(idle_stats.cpu_pg_time[3] * 100,
			idle_stats.cpu_wants_pd_time[3]) : 0),
		(int)(idle_stats.cpu_wants_pd_time[4] ?
			div64_u64(idle_stats.cpu_pg_time[4] * 100,
			idle_stats.cpu_wants_pd_time[4]) : 0));

	seq_printf(s, "\n");
	seq_printf(s, "rail gating time  c0nc gating time  c1nc gating time\n");
	seq_printf(s, "%8llu ms          %8llu ms          %8llu ms\n",
		div64_u64(idle_stats.rail_pd_time, 1000),
		div64_u64(idle_stats.c0nc_pg_time, 1000),
		div64_u64(idle_stats.c1nc_pg_time, 1000));
	seq_printf(s, "%8d%%             %8d%%             %8d%%\n",
		(int)(idle_stats.cpu_wants_pd_time[0] ?
			div64_u64(idle_stats.rail_pd_time * 100,
			idle_stats.cpu_wants_pd_time[0]) : 0),
		(int)(idle_stats.cpu_wants_pd_time[0] ?
			div64_u64(idle_stats.c0nc_pg_time * 100,
			idle_stats.cpu_wants_pd_time[0]) : 0),
		(int)(idle_stats.cpu_wants_pd_time[4] ?
			div64_u64(idle_stats.c1nc_pg_time * 100,
			idle_stats.cpu_wants_pd_time[4]) : 0));

	seq_printf(s, "\n");

	seq_printf(s, "%19s %8s %8s %8s\n", "", "rail gating", "comp", "%");
	seq_printf(s, "-------------------------------------------------\n");
	for (bin = 0; bin < 32; bin++) {
		if (idle_stats.rail_gating_bin[bin] == 0)
			continue;
		seq_printf(s, "%6u - %6u ms: %8u %8u %7u%%\n",
			1 << (bin - 1), 1 << bin,
			idle_stats.rail_gating_bin[bin],
			idle_stats.rail_gating_done_count_bin[bin],
			idle_stats.rail_gating_done_count_bin[bin] * 100 /
				idle_stats.rail_gating_bin[bin]);
	}
	seq_printf(s, "\n");

	seq_printf(s, "%19s %8s %8s %8s\n", "", "c0nc gating", "comp", "%");
	seq_printf(s, "-------------------------------------------------\n");
	for (bin = 0; bin < 32; bin++) {
		if (idle_stats.c0nc_gating_bin[bin] == 0)
			continue;
		seq_printf(s, "%6u - %6u ms: %8u %8u %7u%%\n",
			1 << (bin - 1), 1 << bin,
			idle_stats.c0nc_gating_bin[bin],
			idle_stats.c0nc_gating_done_count_bin[bin],
			idle_stats.c0nc_gating_done_count_bin[bin] * 100 /
				idle_stats.c0nc_gating_bin[bin]);
	}
	seq_printf(s, "\n");

	seq_printf(s, "%19s %8s %8s %8s\n", "", "c1nc gating", "comp", "%");
	seq_printf(s, "-------------------------------------------------\n");
	for (bin = 0; bin < 32; bin++) {
		if (idle_stats.c1nc_gating_bin[bin] == 0)
			continue;
		seq_printf(s, "%6u - %6u ms: %8u %8u %7u%%\n",
			1 << (bin - 1), 1 << bin,
			idle_stats.c1nc_gating_bin[bin],
			idle_stats.c1nc_gating_done_count_bin[bin],
			idle_stats.c1nc_gating_done_count_bin[bin] * 100 /
				idle_stats.c1nc_gating_bin[bin]);
	}

	seq_printf(s, "\n");
	seq_printf(s, "%3s %20s %6s %10s\n",
		"int", "name", "count", "last count");
	seq_printf(s, "--------------------------------------------\n");
	for (i = 0; i < NR_IRQS; i++) {
		if (idle_stats.pd_int_count[i] == 0)
			continue;
		seq_printf(s, "%3d %20s %6d %10d\n",
			i, irq_to_desc(i)->action ?
				irq_to_desc(i)->action->name ?: "???" : "???",
			idle_stats.pd_int_count[i],
			idle_stats.pd_int_count[i] -
				idle_stats.last_pd_int_count[i]);
		idle_stats.last_pd_int_count[i] = idle_stats.pd_int_count[i];
	};
	return 0;
}
#endif

int __init tegra11x_cpuidle_init_soc(struct tegra_cpuidle_ops *idle_ops)
{
	int i;
	struct tegra_cpuidle_ops ops = {
		tegra11x_idle_power_down,
		tegra11x_cpu_idle_stats_pd_ready,
		tegra11x_cpu_idle_stats_pd_time,
		tegra11x_pd_is_allowed,
#ifdef CONFIG_DEBUG_FS
		tegra11x_pd_debug_show
#endif
	};

	cpu_clk_for_dvfs = tegra_get_clock_by_name("cpu_g");

	for (i = 0; i < ARRAY_SIZE(pd_exit_latencies); i++)
		pd_exit_latencies[i] = tegra_pg_exit_latency;

	*idle_ops = ops;
	return 0;
}
