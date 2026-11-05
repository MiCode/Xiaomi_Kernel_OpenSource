// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
 */

#include <trace/hooks/cpufreq.h>

#include "walt.h"

struct walt_cpu_cycle {
	rwlock_t	lock;
	u64		cycles;
	u64		last_time_ns;
	unsigned int	cur_freq_khz;
	unsigned int	mult_fact;
};

static DEFINE_PER_CPU(struct walt_cpu_cycle, walt_cc);

static u64 walt_compute_cpu_cycles(struct walt_cpu_cycle *wcc, u64 wc)
{
	unsigned long flags;
	u64 delta;
	u64 ret;

	/*
	 * freq is in KHz. so multiply by 1000.
	 * time in nsec. so divide by NSEC_PER_SEC.
	 *
	 * cycles = (freq * 1000) * (t/10^9)
	 *        = (freq * t)/10^6
	 *
	 */
	read_lock_irqsave(&wcc->lock, flags);
	delta = wc - wcc->last_time_ns;
	ret = wcc->cycles + ((delta * wcc->mult_fact) >> 20);
	read_unlock_irqrestore(&wcc->lock, flags);

	return ret;
}

static void update_walt_compute_cpu_cycles(struct walt_cpu_cycle *wcc, u64 wc)
{
	unsigned long flags;
	u64 delta;

	/*
	 * freq is in KHz. so multiply by 1000.
	 * time in nsec. so divide by NSEC_PER_SEC.
	 *
	 * cycles = (freq * 1000) * (t/10^9)
	 *        = (freq * t)/10^6
	 *
	 */
	write_lock_irqsave(&wcc->lock, flags);
	delta = wc - wcc->last_time_ns;
	wcc->cycles += (delta * wcc->mult_fact) >> 20;
	wcc->last_time_ns = wc;
	write_unlock_irqrestore(&wcc->lock, flags);
}

u64 walt_cpu_cycle_counter(int cpu, u64 wc)
{
	struct walt_cpu_cycle *wcc = &per_cpu(walt_cc, cpu);
	u64 cycles;

	cycles = walt_compute_cpu_cycles(wcc, wc);

	return cycles;
}

static void walt_cpufreq_transition(void *unused, struct cpufreq_policy *policy)
{
	int i;
	struct walt_cpu_cycle *wcc;
	u64 wc;
	unsigned int mult_fact;

	wc = sched_clock();
	for_each_cpu(i, policy->related_cpus) {
		wcc = &per_cpu(walt_cc, i);
		update_walt_compute_cpu_cycles(wcc, wc);
		wcc->cur_freq_khz = policy->cur;
	}

	mult_fact = (policy->cur << SCHED_CAPACITY_SHIFT)/1000;
	mult_fact = (mult_fact << SCHED_CAPACITY_SHIFT)/1000;
	for_each_cpu(i, policy->related_cpus) {
		wcc = &per_cpu(walt_cc, i);
		wcc->mult_fact = mult_fact;
	}

}

void walt_cycle_counter_init(void)
{
	int i;

	for_each_possible_cpu(i) {
		struct walt_cpu_cycle *wcc = &per_cpu(walt_cc, i);

		rwlock_init(&wcc->lock);
		wcc->cur_freq_khz = cpufreq_quick_get(i);
		wcc->last_time_ns = 0;
		wcc->cycles = 0;
		wcc->mult_fact = (wcc->cur_freq_khz << SCHED_CAPACITY_SHIFT)/1000;
		wcc->mult_fact = (wcc->mult_fact << SCHED_CAPACITY_SHIFT)/1000;
	}

	walt_get_cycle_counts_cb = walt_cpu_cycle_counter;
	use_cycle_counter = true;
	complete(&walt_get_cycle_counts_cb_completion);

	register_trace_android_rvh_cpufreq_transition(walt_cpufreq_transition, NULL);
}
