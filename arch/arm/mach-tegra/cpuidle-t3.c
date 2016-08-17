/*
 * arch/arm/mach-tegra/cpuidle-t3.c
 *
 * CPU idle driver for Tegra3 CPUs
 *
 * Copyright (c) 2010-2013, NVIDIA Corporation.
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
#include <asm/smp_twd.h>
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

#ifdef CONFIG_SMP
static s64 tegra_cpu_wake_by_time[4] = {
	LLONG_MAX, LLONG_MAX, LLONG_MAX, LLONG_MAX };
#endif

static bool lp2_0_in_idle = true;
module_param(lp2_0_in_idle, bool, 0644);

static bool lp2_n_in_idle = true;
module_param(lp2_n_in_idle, bool, 0644);

static struct clk *cpu_clk_for_dvfs;
static struct clk *twd_clk;

static int lp2_exit_latencies[5];

static struct {
	unsigned int cpu_ready_count[5];
	unsigned int tear_down_count[5];
	unsigned long long cpu_wants_lp2_time[5];
	unsigned long long in_lp2_time[5];
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

static inline void tegra_irq_unmask(int irq)
{
	struct irq_data *data = irq_get_irq_data(irq);
	data->chip->irq_unmask(data);
}

static inline unsigned int cpu_number(unsigned int n)
{
	return is_lp_cluster() ? 4 : n;
}

void tegra3_cpu_idle_stats_lp2_ready(unsigned int cpu)
{
	idle_stats.cpu_ready_count[cpu_number(cpu)]++;
}

void tegra3_cpu_idle_stats_lp2_time(unsigned int cpu, s64 us)
{
	idle_stats.cpu_wants_lp2_time[cpu_number(cpu)] += us;
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

bool tegra3_lp2_is_allowed(struct cpuidle_device *dev,
	struct cpuidle_state *state)
{
	s64 request;

	if (!tegra_all_cpus_booted)
		return false;

	if ((!lp2_0_in_idle && !dev->cpu) || (!lp2_n_in_idle && dev->cpu))
		return false;

	/* On A01, LP2 on slave CPU's cause ranhdom CPU hangs.
	 * Refer to Bug 804085.
	 */
	if ((tegra_revision == TEGRA_REVISION_A01) &&
		(tegra_get_chipid() == TEGRA_CHIPID_TEGRA3) &&
		num_online_cpus() > 1)
			return false;

#ifndef CONFIG_TEGRA_RAIL_OFF_MULTIPLE_CPUS
	/* FIXME: All CPU's entering LP2 is not working.
	 * Don't let CPU0 enter LP2 when any secondary CPU is online.
	 */
	if ((dev->cpu == 0) && (num_online_cpus() > 1))
		return false;
#endif
	if ((dev->cpu == 0)  && (!tegra_rail_off_is_allowed()))
		return false;

	request = ktime_to_us(tick_nohz_get_sleep_length());
	if (state->exit_latency != lp2_exit_latencies[cpu_number(dev->cpu)]) {
		/* possible on the 1st entry after cluster switch*/
		state->exit_latency = lp2_exit_latencies[cpu_number(dev->cpu)];
		tegra_pd_update_target_residency(state);
	}
	if (request < state->target_residency) {
		/* Not enough time left to enter LP2 */
		return false;
	}

	return true;
}

static inline void tegra3_lp2_restore_affinity(void)
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

		/* Cancel LP2 wake timers for all secondary CPUs */
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
		lp2_exit_latencies[cpu_number(dev->cpu)];

	bin = time_to_bin((u32)request / 1000);
	idle_stats.tear_down_count[cpu_number(dev->cpu)]++;
	idle_stats.lp2_count++;
	idle_stats.lp2_count_bin[bin]++;

	clockevents_notify(CLOCK_EVT_NOTIFY_BROADCAST_ENTER, &dev->cpu);
	if (!is_lp_cluster())
		tegra_dvfs_rail_off(tegra_cpu_rail, entry_time);

	if (tegra_idle_power_down_last(sleep_time, 0) == 0)
		sleep_completed = true;
	else {
		int irq = tegra_gic_pending_interrupt();
		idle_stats.lp2_int_count[irq]++;
	}

	clockevents_notify(CLOCK_EVT_NOTIFY_BROADCAST_EXIT, &dev->cpu);
	exit_time = ktime_get();
	if (!is_lp_cluster())
		tegra_dvfs_rail_on(tegra_cpu_rail, exit_time);

	idle_stats.in_lp2_time[cpu_number(dev->cpu)] +=
		ktime_to_us(ktime_sub(exit_time, entry_time));

	if (multi_cpu_entry)
		tegra3_lp2_restore_affinity();

	if (sleep_completed) {
		/*
		 * Stayed in LP2 for the full time until the next tick,
		 * adjust the exit latency based on measurement
		 */
		int offset = ktime_to_us(ktime_sub(exit_time, entry_time))
			- request;
		int latency = lp2_exit_latencies[cpu_number(dev->cpu)] +
			offset / 16;
		latency = clamp(latency, 0, 10000);
		lp2_exit_latencies[cpu_number(dev->cpu)] = latency;
		state->exit_latency = latency;		/* for idle governor */
		smp_wmb();

		idle_stats.lp2_completed_count++;
		idle_stats.lp2_completed_count_bin[bin]++;

		pr_debug("%lld %lld %d %d\n", request,
			ktime_to_us(ktime_sub(exit_time, entry_time)),
			offset, bin);
	}

	cpu_pm_exit();

	return true;
}

#ifndef CONFIG_TRUSTED_FOUNDATIONS
static unsigned int g_diag_reg;

static void save_cpu_arch_register(void)
{
	/* read diagnostic register */
	if (((read_cpuid_id() >> 4) & 0xFFF) == 0xC09)
		asm("mrc p15, 0, %0, c15, c0, 1" : "=r"(g_diag_reg) : : "cc");
}

static void restore_cpu_arch_register(void)
{
	/* write diagnostic register */
	if (((read_cpuid_id() >> 4) & 0xFFF) == 0xC09)
		asm("mcr p15, 0, %0, c15, c0, 1" : : "r"(g_diag_reg) : "cc");
}
#endif

static bool tegra_cpu_core_power_down(struct cpuidle_device *dev,
			   struct cpuidle_state *state, s64 request)
{
#ifdef CONFIG_SMP
	s64 sleep_time;
	ktime_t entry_time;
	struct tegra_twd_context twd_context;
	bool sleep_completed = false;
	struct tick_sched *ts = tick_get_tick_sched(dev->cpu);
#if defined(CONFIG_TEGRA_LP2_CPU_TIMER)
	void __iomem *twd_base = IO_ADDRESS(TEGRA_ARM_PERIF_BASE + 0x600);
#endif

	if (!tegra_twd_get_state(&twd_context)) {
		unsigned long twd_rate = clk_get_rate(twd_clk);

		if ((twd_context.twd_ctrl & TWD_TIMER_CONTROL_ENABLE) &&
		    (twd_context.twd_ctrl & TWD_TIMER_CONTROL_IT_ENABLE)) {
			request = div_u64((u64)twd_context.twd_cnt * 1000000,
					  twd_rate);
#ifdef CONFIG_TEGRA_LP2_CPU_TIMER
			if (request >= state->target_residency) {
				twd_context.twd_cnt -= state->exit_latency *
					(twd_rate / 1000000);
				writel(twd_context.twd_cnt,
					twd_base + TWD_TIMER_COUNTER);
			}
#endif
		}
	}

	if (!tegra_is_cpu_wake_timer_ready(dev->cpu) ||
	    (request < state->target_residency) ||
	    (!ts) || (ts->nohz_mode == NOHZ_MODE_INACTIVE)) {
		/*
		 * Not enough time left to enter LP2, or wake timer not ready
		 */
		cpu_do_idle();
		return false;
	}

	cpu_pm_enter();

#if !defined(CONFIG_TEGRA_LP2_CPU_TIMER)
	sleep_time = request - state->exit_latency;
	clockevents_notify(CLOCK_EVT_NOTIFY_BROADCAST_ENTER, &dev->cpu);
	tegra_twd_suspend(&twd_context);
	tegra_pd_set_trigger(sleep_time);
#endif
	idle_stats.tear_down_count[cpu_number(dev->cpu)]++;

	entry_time = ktime_get();

	/* Save time this CPU must be awakened by. */
	tegra_cpu_wake_by_time[dev->cpu] = ktime_to_us(entry_time) + request;
	smp_wmb();

#ifndef CONFIG_TRUSTED_FOUNDATIONS
	save_cpu_arch_register();
#endif

	cpu_suspend(0, tegra3_sleep_cpu_secondary_finish);

#ifndef CONFIG_TRUSTED_FOUNDATIONS
	restore_cpu_arch_register();
#endif

	tegra_cpu_wake_by_time[dev->cpu] = LLONG_MAX;

#ifdef CONFIG_TEGRA_LP2_CPU_TIMER
	if (!tegra_twd_get_state(&twd_context))
		sleep_completed = (twd_context.twd_cnt == 0);
#else
	sleep_completed = !tegra_pd_timer_remain();
	tegra_pd_set_trigger(0);
	tegra_twd_resume(&twd_context);
	clockevents_notify(CLOCK_EVT_NOTIFY_BROADCAST_EXIT, &dev->cpu);
#endif
	sleep_time = ktime_to_us(ktime_sub(ktime_get(), entry_time));
	idle_stats.in_lp2_time[cpu_number(dev->cpu)] += sleep_time;
	if (sleep_completed) {
		/*
		 * Stayed in LP2 for the full time until timer expires,
		 * adjust the exit latency based on measurement
		 */
		int offset = sleep_time - request;
		int latency = lp2_exit_latencies[cpu_number(dev->cpu)] +
			offset / 16;
		latency = clamp(latency, 0, 10000);
		lp2_exit_latencies[cpu_number(dev->cpu)] = latency;
		state->exit_latency = latency;		/* for idle governor */
		smp_wmb();
	}
#endif
	cpu_pm_exit();

	return true;
}

bool tegra3_idle_lp2(struct cpuidle_device *dev,
			   struct cpuidle_state *state)
{
	s64 request = ktime_to_us(tick_nohz_get_sleep_length());
	bool last_cpu = tegra_set_cpu_in_pd(dev->cpu);
	bool entered_lp2;

	if ((dev->cpu == 0) && last_cpu)
		entered_lp2 = tegra_cpu_cluster_power_down(dev, state, request);
	else if (dev->cpu)
		entered_lp2 = tegra_cpu_core_power_down(dev, state, request);
	else {
		cpu_do_idle();
		entered_lp2 = false;
	}

	tegra_clear_cpu_in_pd(dev->cpu);

	return entered_lp2;
}

#ifdef CONFIG_DEBUG_FS
int tegra3_lp2_debug_show(struct seq_file *s, void *data)
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
	seq_printf(s, "lp2:            %8u\n", idle_stats.lp2_count);
	seq_printf(s, "lp2 completed:  %8u %7u%%\n",
		idle_stats.lp2_completed_count,
		idle_stats.lp2_completed_count * 100 /
			(idle_stats.lp2_count ?: 1));

	seq_printf(s, "\n");
	seq_printf(s, "cpu ready time:                 %8llu %8llu %8llu %8llu %8llu ms\n",
		div64_u64(idle_stats.cpu_wants_lp2_time[0], 1000),
		div64_u64(idle_stats.cpu_wants_lp2_time[1], 1000),
		div64_u64(idle_stats.cpu_wants_lp2_time[2], 1000),
		div64_u64(idle_stats.cpu_wants_lp2_time[3], 1000),
		div64_u64(idle_stats.cpu_wants_lp2_time[4], 1000));

	seq_printf(s, "lp2 time:                       %8llu %8llu %8llu %8llu %8llu ms\n",
		div64_u64(idle_stats.in_lp2_time[0], 1000),
		div64_u64(idle_stats.in_lp2_time[1], 1000),
		div64_u64(idle_stats.in_lp2_time[2], 1000),
		div64_u64(idle_stats.in_lp2_time[3], 1000),
		div64_u64(idle_stats.in_lp2_time[4], 1000));

	seq_printf(s, "lp2 %%:                         %7d%% %7d%% %7d%% %7d%% %7d%%\n",
		(int)(idle_stats.cpu_wants_lp2_time[0] ?
			div64_u64(idle_stats.in_lp2_time[0] * 100,
			idle_stats.cpu_wants_lp2_time[0]) : 0),
		(int)(idle_stats.cpu_wants_lp2_time[1] ?
			div64_u64(idle_stats.in_lp2_time[1] * 100,
			idle_stats.cpu_wants_lp2_time[1]) : 0),
		(int)(idle_stats.cpu_wants_lp2_time[2] ?
			div64_u64(idle_stats.in_lp2_time[2] * 100,
			idle_stats.cpu_wants_lp2_time[2]) : 0),
		(int)(idle_stats.cpu_wants_lp2_time[3] ?
			div64_u64(idle_stats.in_lp2_time[3] * 100,
			idle_stats.cpu_wants_lp2_time[3]) : 0),
		(int)(idle_stats.cpu_wants_lp2_time[4] ?
			div64_u64(idle_stats.in_lp2_time[4] * 100,
			idle_stats.cpu_wants_lp2_time[4]) : 0));
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

int __init tegra3_cpuidle_init_soc(struct tegra_cpuidle_ops *idle_ops)
{
	int i;
	struct tegra_cpuidle_ops ops = {
		tegra3_idle_lp2,
		tegra3_cpu_idle_stats_lp2_ready,
		tegra3_cpu_idle_stats_lp2_time,
		tegra3_lp2_is_allowed,
#ifdef CONFIG_DEBUG_FS
		tegra3_lp2_debug_show
#endif
	};

	cpu_clk_for_dvfs = tegra_get_clock_by_name("cpu_g");
	twd_clk = tegra_get_clock_by_name("twd");

	for (i = 0; i < ARRAY_SIZE(lp2_exit_latencies); i++)
		lp2_exit_latencies[i] = tegra_pg_exit_latency;

	*idle_ops = ops;

	return 0;
}
