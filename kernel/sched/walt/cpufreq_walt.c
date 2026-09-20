// SPDX-License-Identifier: GPL-2.0-only
/*
 * This is based on schedutil governor but modified to work with
 * WALT.
 *
 * Copyright (C) 2016, Intel Corporation
 * Copyright (c) 2020-2021, The Linux Foundation. All rights reserved.
 * Copyright (c) 2022-2025, Qualcomm Innovation Center, Inc. All rights reserved.
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/kthread.h>
#include <trace/events/power.h>

#include "walt.h"
#include "trace.h"

#define MAX_ZONES 10
#define ZONE_TUPLE_SIZE 2
#define MAX_UTIL 1024

struct waltgov_zones {
	int util_thresh;
	int inflate_factor;
};

struct waltgov_tunables {
	struct gov_attr_set	attr_set;
	unsigned int		up_rate_limit_us;
	unsigned int		down_rate_limit_us;
	unsigned int		hispeed_load;
	unsigned int		hispeed_freq;
	unsigned int		hispeed_cond_freq;
	unsigned int		rtg_boost_freq;
	unsigned int		adaptive_level_1;
	unsigned int		adaptive_low_freq;
	unsigned int		adaptive_high_freq;
	unsigned int		adaptive_level_1_kernel;
	unsigned int		adaptive_low_freq_kernel;
	unsigned int		adaptive_high_freq_kernel;
	bool			pl;
	int			boost;
	int			zone_util_pct[MAX_ZONES][ZONE_TUPLE_SIZE];
};

struct waltgov_policy {
	struct cpufreq_policy	*policy;
	u64			last_ws;
	u64			curr_cycles;
	u64			last_cyc_update_time;
	unsigned long		avg_cap;
	struct waltgov_tunables	*tunables;
	struct list_head	tunables_hook;
	unsigned long		hispeed_cond_util;
	struct waltgov_zones	zone_util[MAX_ZONES];

	raw_spinlock_t		update_lock;
	u64			last_freq_update_time;
	s64			min_rate_limit_ns;
	s64			up_rate_delay_ns;
	s64			down_rate_delay_ns;
	unsigned int		next_freq;
	unsigned int		cached_raw_freq;
	unsigned int		driving_cpu;
	unsigned int		ipc_smart_freq;

	/* The next fields are only needed if fast switch cannot be used: */
	struct	irq_work	irq_work;
	struct	kthread_work	work;
	struct	mutex		work_lock;
	struct	kthread_worker	worker;
	struct task_struct	*thread;

	bool			limits_changed;
	bool			need_freq_update;
	bool			thermal_isolated;
	bool			rtg_boost_flag;
	bool			hispeed_flag;
	bool			conservative_pl_flag;
};

struct waltgov_cpu {
	struct waltgov_callback	cb;
	struct waltgov_policy	*wg_policy;
	unsigned int		cpu;
	struct walt_cpu_load	walt_load;
	unsigned long		util;
	unsigned int		flags;
	unsigned int		reasons;
	bool			rtg_boost_flag;
	bool			hispeed_flag;
	bool			conservative_pl_flag;
};

DEFINE_PER_CPU(struct waltgov_callback *, waltgov_cb_data);
// MIUI ADD: Game_TurboSched
#ifdef CONFIG_MIGT_WALT
EXPORT_PER_CPU_SYMBOL_GPL(waltgov_cb_data);
#endif
// END Game_TurboSched
static DEFINE_PER_CPU(struct waltgov_cpu, waltgov_cpu);
static DEFINE_PER_CPU(struct waltgov_tunables *, cached_tunables);

// MIUI ADD: Performance_TurboSched
#ifdef CONFIG_METIS_WALT
extern bool (oem_should_update_freq)(struct cpufreq_policy *policy, u64 time);
#endif
// END Performance_TurboSched

// MIUI ADD: Game_TurboSched
#ifdef CONFIG_MIGT_WALT
typedef void (*oem_freq_f)(void *, unsigned long, unsigned long,
		unsigned long, unsigned long *,
		struct cpufreq_policy *, bool *,
		unsigned int, unsigned int, unsigned int);

static oem_freq_f oem_set_freq_hook = NULL;
#endif
// END Game_TurboSched

/************************ Governor internals ***********************/

static bool waltgov_should_update_freq(struct waltgov_policy *wg_policy, u64 time)
{
	s64 delta_ns;

	if (unlikely(wg_policy->limits_changed)) {
		wg_policy->limits_changed = false;
		wg_policy->need_freq_update = true;
		return true;
	}

// MIUI ADD: Performance_TurboSched
#ifdef CONFIG_METIS_WALT
    if (unlikely(oem_should_update_freq(wg_policy->policy, time)))
        return true;
#endif
// END Performance_TurboSched
	/*
	 * No need to recalculate next freq for min_rate_limit_us
	 * at least. However we might still decide to further rate
	 * limit once frequency change direction is decided, according
	 * to the separate rate limits.
	 */

	delta_ns = time - wg_policy->last_freq_update_time;
	return delta_ns >= wg_policy->min_rate_limit_ns;
}

static bool waltgov_up_down_rate_limit(struct waltgov_policy *wg_policy, u64 time,
				     unsigned int next_freq)
{
	s64 delta_ns;

	delta_ns = time - wg_policy->last_freq_update_time;

	if (next_freq > wg_policy->next_freq &&
	    delta_ns < wg_policy->up_rate_delay_ns)
		return true;

	if (next_freq < wg_policy->next_freq &&
	    delta_ns < wg_policy->down_rate_delay_ns)
		return true;

	return false;
}

static void __waltgov_update_next_freq(struct waltgov_policy *wg_policy,
		u64 time, unsigned int next_freq, unsigned int raw_freq)
{
	wg_policy->cached_raw_freq = raw_freq;
	wg_policy->next_freq = next_freq;
	wg_policy->last_freq_update_time = time;
}

static bool waltgov_update_next_freq(struct waltgov_policy *wg_policy, u64 time,
					unsigned int next_freq,
					unsigned int raw_freq)
{
	if (wg_policy->next_freq == next_freq)
		return false;

	if (waltgov_up_down_rate_limit(wg_policy, time, next_freq)) {
		wg_policy->cached_raw_freq = 0;
		return false;
	}

	__waltgov_update_next_freq(wg_policy, time, next_freq, raw_freq);

	return true;
}

static unsigned long freq_to_util(struct waltgov_policy *wg_policy,
				  unsigned int freq)
{
	return mult_frac(arch_scale_cpu_capacity(wg_policy->policy->cpu),
			freq, wg_policy->policy->cpuinfo.max_freq);
}

#define KHZ 1000
static void waltgov_track_cycles(struct waltgov_policy *wg_policy,
				unsigned int prev_freq,
				u64 upto)
{
	u64 delta_ns, cycles;
	u64 next_ws = wg_policy->last_ws + sched_ravg_window;

	upto = min(upto, next_ws);
	/* Track cycles in current window */
	delta_ns = upto - wg_policy->last_cyc_update_time;
	delta_ns *= prev_freq;
	do_div(delta_ns, (NSEC_PER_SEC / KHZ));
	cycles = delta_ns;
	wg_policy->curr_cycles += cycles;
	wg_policy->last_cyc_update_time = upto;
}

static void waltgov_calc_avg_cap(struct waltgov_policy *wg_policy, u64 curr_ws,
				unsigned int prev_freq)
{
	u64 last_ws = wg_policy->last_ws;
	unsigned int avg_freq;
	int cpu;

	if (curr_ws < last_ws) {
		printk_deferred("============ WALT CPUFREQ DUMP START ==============\n");
		for_each_online_cpu(cpu) {
			struct waltgov_cpu *wg_cpu = &per_cpu(waltgov_cpu, cpu);
			struct waltgov_policy *wg_policy_internal = wg_cpu->wg_policy;

			printk_deferred("cpu=%d walt_load->ws=%llu and policy->last_ws=%llu\n",
					wg_cpu->cpu, wg_cpu->walt_load.ws,
					wg_policy_internal->last_ws);
		}
		printk_deferred("============ WALT CPUFREQ DUMP END  ==============\n");
		WALT_BUG(WALT_BUG_WALT, NULL,
				"policy->related_cpus=0x%lx curr_ws=%llu < last_ws=%llu",
				cpumask_bits(wg_policy->policy->related_cpus)[0], curr_ws,
				last_ws);
	}

	if (curr_ws <= last_ws)
		return;

	/* If we skipped some windows */
	if (curr_ws > (last_ws + sched_ravg_window)) {
		avg_freq = prev_freq;
		/* Reset tracking history */
		wg_policy->last_cyc_update_time = curr_ws;
	} else {
		waltgov_track_cycles(wg_policy, prev_freq, curr_ws);
		avg_freq = wg_policy->curr_cycles;
		avg_freq /= sched_ravg_window / (NSEC_PER_SEC / KHZ);
	}
	wg_policy->avg_cap = freq_to_util(wg_policy, avg_freq);
	wg_policy->curr_cycles = 0;
	wg_policy->last_ws = curr_ws;
}

static void waltgov_fast_switch(struct waltgov_policy *wg_policy, u64 time,
			      unsigned int next_freq)
{
	struct cpufreq_policy *policy = wg_policy->policy;

	waltgov_track_cycles(wg_policy, wg_policy->policy->cur, time);
	cpufreq_driver_fast_switch(policy, next_freq);
}

static void waltgov_deferred_update(struct waltgov_policy *wg_policy, u64 time,
				  unsigned int next_freq)
{
	walt_irq_work_queue(&wg_policy->irq_work);
}

#define TARGET_LOAD 80
static inline unsigned long walt_map_util_freq(unsigned long util,
					struct waltgov_policy *wg_policy,
					unsigned long cap, int cpu)
{
	unsigned long fmax = wg_policy->policy->cpuinfo.max_freq;
	unsigned long util_boost_factor = (fmax + (fmax >> 2));
	int i;

	util = min(MAX_UTIL, util);

	/*
	 * We are updating util_boost_factor to a set value for a specific utilization if it falls
	 * under a zone which is defined by sysfs tunable.
	 */
	for (i = 0 ; i < MAX_ZONES; i++) {
		if (wg_policy->zone_util[i].util_thresh == -1)
			break;

		if (util <= wg_policy->zone_util[i].util_thresh) {
			util_boost_factor = wg_policy->zone_util[i].inflate_factor;
			break;
		}
	}

	return (util_boost_factor * util/cap);
}

// MIUI ADD: Game_TurboSched
#ifdef CONFIG_MIGT_WALT
void register_oem_freq_hook(oem_freq_f f)
{
	if (likely(f))
		oem_set_freq_hook = f;
}
EXPORT_SYMBOL_GPL(register_oem_freq_hook);

void unregister_oem_freq_hook(void)
{
	oem_set_freq_hook = NULL;
}
EXPORT_SYMBOL_GPL(unregister_oem_freq_hook);
#endif
// END Game_TurboSched

static inline unsigned int get_adaptive_level_1(struct waltgov_policy *wg_policy)
{
	return(max(wg_policy->tunables->adaptive_level_1,
		   wg_policy->tunables->adaptive_level_1_kernel));
}


static inline unsigned int get_adaptive_low_freq(struct waltgov_policy *wg_policy)
{
	return(max(wg_policy->tunables->adaptive_low_freq,
		   wg_policy->tunables->adaptive_low_freq_kernel));
}

static inline unsigned int get_adaptive_high_freq(struct waltgov_policy *wg_policy)
{
	return(max(wg_policy->tunables->adaptive_high_freq,
		   wg_policy->tunables->adaptive_high_freq_kernel));
}

static unsigned int get_smart_freq_limit(unsigned int freq, struct waltgov_policy *wg_policy,
		struct waltgov_cpu *wg_driv_cpu)
{
	unsigned int smart_freq = FREQ_QOS_MAX_DEFAULT_VALUE;
	unsigned int smart_reason = 0;
	struct walt_sched_cluster *cluster = cpu_cluster(wg_policy->policy->cpu);
	/*
	 * if ipc is enabled, then we update freq with respect to ipc and legacy both;
	 * if ipc is disabled and legacy is enabled then we update freq with respect to legacy only;
	 * if both ipc and legacy are disabled we don't need to update freq with smart_freq.
	 */
	if (cluster->smart_freq_info->smart_freq_ipc_participation_mask) {
		if (freq_cap[SMART_FREQ][cluster->id] > wg_policy->ipc_smart_freq) {
			smart_freq = freq_cap[SMART_FREQ][cluster->id];
			smart_reason = CPUFREQ_REASON_SMART_FREQ_BIT;
		} else if (freq_cap[SMART_FREQ][cluster->id] < wg_policy->ipc_smart_freq) {
			smart_freq = wg_policy->ipc_smart_freq;
			smart_reason = CPUFREQ_REASON_IPC_SMART_FREQ_BIT;
		} else {
			smart_freq = wg_policy->ipc_smart_freq;
			smart_reason = CPUFREQ_REASON_SMART_FREQ_BIT |
				CPUFREQ_REASON_IPC_SMART_FREQ_BIT;
		}
	} else {
		smart_freq = freq_cap[SMART_FREQ][cluster->id];
		smart_reason = CPUFREQ_REASON_SMART_FREQ_BIT;
	}

	if (freq > smart_freq) {
		freq = smart_freq;
		wg_driv_cpu->reasons |= smart_reason;
	}

	return freq;
}

void post_update_cleanups(struct waltgov_policy *wg_policy)
{
	struct cpufreq_policy *policy = wg_policy->policy;
	int cpu;

	for_each_cpu(cpu, policy->cpus) {
		struct waltgov_cpu *wg_cpu = &per_cpu(waltgov_cpu, cpu);

		wg_cpu->rtg_boost_flag = false;
		wg_cpu->hispeed_flag = false;
		wg_cpu->conservative_pl_flag = false;
		wg_cpu->reasons = 0;
	}

	wg_policy->rtg_boost_flag = false;
	wg_policy->hispeed_flag = false;
	wg_policy->conservative_pl_flag = false;

}

static unsigned int get_next_freq(struct waltgov_policy *wg_policy,
				  unsigned long util, unsigned long max,
				  struct waltgov_cpu *wg_cpu, u64 time)
{
	struct cpufreq_policy *policy = wg_policy->policy;
	unsigned int freq, raw_freq, final_freq, mod_freq, mod_adap_freq;
	struct waltgov_cpu *wg_driv_cpu = &per_cpu(waltgov_cpu, wg_policy->driving_cpu);
	struct walt_rq *wrq = &per_cpu(walt_rq, wg_policy->driving_cpu);
	struct walt_sched_cluster *cluster = NULL;
	bool skip = false;
	bool thermal_isolated_now = cpus_halted_by_client(
			wg_policy->policy->related_cpus, PAUSE_THERMAL);
	bool reset_need_freq_update = false;
	unsigned int j;

// MIUI ADD: Game_TurboSched
#ifdef CONFIG_MIGT_WALT
	unsigned long glk_freq;
	unsigned int adaptive_low_freq = 0, adaptive_high_freq = 0;
#endif
// END Game_TurboSched

	if (soc_feat(SOC_ENABLE_THERMAL_HALT_LOW_FREQ_BIT)) {
		if (thermal_isolated_now) {
			if (!wg_policy->thermal_isolated) {
				/* Entering thermal isolation */
				wg_policy->thermal_isolated = true;
				wg_policy->policy->cached_resolved_idx = 0;
				final_freq = wg_policy->policy->freq_table[0].frequency;
				__waltgov_update_next_freq(wg_policy, time, final_freq, final_freq);
			} else {
				/* no need to change freq, i.e. continue with min freq */
				final_freq = 0;
			}
			raw_freq = final_freq;
			freq = raw_freq;
			goto out;
		} else {
			if (wg_policy->thermal_isolated) {
				/* Exiting thermal isolation*/
				wg_policy->thermal_isolated = false;
				wg_policy->need_freq_update = true;
			}
		}
	}

	raw_freq = walt_map_util_freq(util, wg_policy, max, wg_driv_cpu->cpu);
	mod_adap_freq = raw_freq;

	if (wg_policy->rtg_boost_flag == true) {
		for_each_cpu(j, policy->cpus) {
			struct waltgov_cpu *j_wg_cpu = &per_cpu(waltgov_cpu, j);

			mod_freq = wg_policy->tunables->rtg_boost_freq;
			if (mod_freq > mod_adap_freq && j_wg_cpu->rtg_boost_flag == true) {
				mod_adap_freq = mod_freq;
				wg_driv_cpu = j_wg_cpu;
				wg_driv_cpu->reasons |= CPUFREQ_REASON_RTG_BOOST_BIT;
				break;
			}
		}
	}

	if (wg_policy->hispeed_flag == true) {
		for_each_cpu(j, policy->cpus) {
			struct waltgov_cpu *j_wg_cpu = &per_cpu(waltgov_cpu, j);

			mod_freq = wg_policy->tunables->hispeed_freq;
			if (mod_freq > mod_adap_freq && j_wg_cpu->hispeed_flag == true) {
				mod_adap_freq = mod_freq;
				wg_driv_cpu = j_wg_cpu;
				wg_driv_cpu->reasons |= CPUFREQ_REASON_HISPEED_BIT;
				break;
			}
		}
	}

	if (wg_policy->conservative_pl_flag == true) {
		for_each_cpu(j, policy->cpus) {
			struct waltgov_cpu *j_wg_cpu = &per_cpu(waltgov_cpu, j);
			unsigned long cap = arch_scale_cpu_capacity(j_wg_cpu->cpu);
			unsigned long fmax = j_wg_cpu->wg_policy->policy->cpuinfo.max_freq;

			mod_freq = (fmax * j_wg_cpu->walt_load.pl)/cap;
			if (mod_freq > mod_adap_freq &&
					j_wg_cpu->conservative_pl_flag == true) {
				mod_adap_freq = mod_freq;
				wg_driv_cpu = j_wg_cpu;
				wg_driv_cpu->reasons |= CPUFREQ_REASON_PL_BIT;
				break;
			}
		}
	}

	freq = mod_adap_freq;

	cluster = cpu_cluster(policy->cpu);
	if (cpumask_intersects(&cluster->cpus, cpu_partial_halt_mask) &&
			is_state1())
		skip = true;

	if (wg_cpu->walt_load.trailblazer_state && freq < trailblazer_floor_freq[cluster->id] &&
		walt_feat(WALT_FEAT_TRAILBLAZER_BIT)) {
		freq = trailblazer_floor_freq[cluster->id];
		wg_driv_cpu->reasons |= CPUFREQ_REASON_TRAILBLAZER_STATE_BIT;
	}

	if (wg_policy->tunables->adaptive_high_freq && !skip) {
		if (mod_adap_freq < get_adaptive_level_1(wg_policy)) {
			freq = get_adaptive_level_1(wg_policy);
			wg_driv_cpu->reasons |= CPUFREQ_REASON_ADAPTIVE_LVL_1_BIT;
		} else if (mod_adap_freq < get_adaptive_low_freq(wg_policy)) {
			freq = get_adaptive_low_freq(wg_policy);
			wg_driv_cpu->reasons |= CPUFREQ_REASON_ADAPTIVE_LOW_BIT;
		} else if (mod_adap_freq <= get_adaptive_high_freq(wg_policy)) {
			freq = get_adaptive_high_freq(wg_policy);
			wg_driv_cpu->reasons |= CPUFREQ_REASON_ADAPTIVE_HIGH_BIT;
		}
// MIUI ADD: Game_TurboSched
#ifdef CONFIG_MIGT_WALT
		adaptive_low_freq = get_adaptive_low_freq(wg_policy);
		adaptive_high_freq = get_adaptive_high_freq(wg_policy);
#endif
// END Game_TurboSched
	}

	freq = get_smart_freq_limit(freq, wg_policy, wg_driv_cpu);

	if (freq > freq_cap[HIGH_PERF_CAP][cluster->id]) {
		freq = freq_cap[HIGH_PERF_CAP][cluster->id];
		wg_driv_cpu->reasons |= CPUFREQ_REASON_HIGH_PERF_CAP_BIT;
	}

	if (freq > freq_cap[PARTIAL_HALT_CAP][cluster->id]) {
		freq = freq_cap[PARTIAL_HALT_CAP][cluster->id];
		wg_driv_cpu->reasons |= CPUFREQ_REASON_PARTIAL_HALT_CAP_BIT;
	}

// MIUI ADD: Game_TurboSched
#ifdef CONFIG_MIGT_WALT
	glk_freq = freq;
	if (oem_set_freq_hook)
		oem_set_freq_hook(NULL, util, policy->cpuinfo.max_freq, max,
			&glk_freq, policy, &wg_policy->need_freq_update,
			adaptive_low_freq, adaptive_high_freq, wg_driv_cpu->reasons);
	freq = glk_freq;
#endif
// END Game_TurboSched

	if ((wg_driv_cpu->flags & WALT_CPUFREQ_UCLAMP_BIT) &&
		((wrq->uclamp_limit[UCLAMP_MIN] != 0) ||
			(wrq->uclamp_limit[UCLAMP_MAX] != SCHED_CAPACITY_SCALE)))
		wg_driv_cpu->reasons |= CPUFREQ_REASON_UCLAMP_BIT;

	if (wg_policy->cached_raw_freq && freq == wg_policy->cached_raw_freq &&
		!wg_policy->need_freq_update) {
		final_freq = 0;
		goto out;
	}

	reset_need_freq_update = true;

	final_freq = cpufreq_driver_resolve_freq(policy, freq);

	if (!waltgov_update_next_freq(wg_policy, time, final_freq, freq))
		final_freq = 0;
out:
	trace_waltgov_next_freq(policy, util, max, raw_freq, freq,
				wg_policy->cached_raw_freq, wg_policy->need_freq_update,
				wg_policy->thermal_isolated,
				wg_driv_cpu->cpu, wg_driv_cpu->reasons,
				wg_policy->ipc_smart_freq,
				final_freq);

	if (reset_need_freq_update)
		wg_policy->need_freq_update = false;

	post_update_cleanups(wg_policy);

	return final_freq;
}

#define NL_RATIO 75
#define DEFAULT_HISPEED_LOAD 90
#define DEFAULT_SILVER_RTG_BOOST_FREQ 1000000
#define DEFAULT_GOLD_RTG_BOOST_FREQ 768000
#define DEFAULT_PRIME_RTG_BOOST_FREQ 0
static inline void max_and_reason(unsigned long *cur_util, unsigned long boost_util,
		struct waltgov_cpu *wg_cpu, unsigned int reason)
{
	if (boost_util && boost_util >= *cur_util) {
		*cur_util = boost_util;
		wg_cpu->reasons = reason;
		wg_cpu->wg_policy->driving_cpu = wg_cpu->cpu;
	}
}

static void waltgov_walt_adjust(struct waltgov_cpu *wg_cpu, unsigned long cpu_util,
				unsigned long nl, unsigned long *util,
				unsigned long *max)
{
	struct waltgov_policy *wg_policy = wg_cpu->wg_policy;
	bool is_migration = wg_cpu->flags & WALT_CPUFREQ_IC_MIGRATION_BIT;
	bool is_rtg_boost = wg_cpu->walt_load.rtgb_active;
	bool is_hiload;
	bool employ_ed_boost = wg_cpu->walt_load.ed_active && sysctl_ed_boost_pct;
	unsigned long pl = wg_cpu->walt_load.pl;
	unsigned long min_util = *util;

	if (is_rtg_boost && (!cpumask_test_cpu(wg_cpu->cpu, cpu_partial_halt_mask) ||
				!is_state1())) {
		wg_policy->rtg_boost_flag = true;
		wg_cpu->rtg_boost_flag = true;
	}

	is_hiload = (cpu_util >= mult_frac(wg_policy->avg_cap,
					   wg_policy->tunables->hispeed_load,
					   100));

	if (cpumask_test_cpu(wg_cpu->cpu, cpu_partial_halt_mask) &&
			is_state1())
		is_hiload = false;

	if (wg_policy->avg_cap < wg_policy->hispeed_cond_util)
		is_hiload = false;

	if (is_hiload && !is_migration) {
		wg_policy->hispeed_flag = true;
		wg_cpu->hispeed_flag = true;
	}

	if (is_hiload && nl >= mult_frac(cpu_util, NL_RATIO, 100))
		max_and_reason(util, *max, wg_cpu, CPUFREQ_REASON_NWD_BIT);

	/*
	 * For conservative PL, 2 cases may arise, if we have set
	 * sysctl_sched_conservative_pl that means we need to run on that pl
	 * equivalent frequency, if it is not the case, i.e.
	 * sysctl_sched_conservative_pl is not set we would like to go ahead
	 * with pl frequency to be inflated based on target load for that zone.
	 */
	if (wg_policy->tunables->pl) {
		if (sysctl_sched_conservative_pl) {
			wg_policy->conservative_pl_flag = true;
			wg_cpu->conservative_pl_flag = true;
		} else {
			max_and_reason(util, pl, wg_cpu, CPUFREQ_REASON_PL_BIT);
		}
	}

	if (employ_ed_boost)
		wg_cpu->reasons |= CPUFREQ_REASON_EARLY_DET_BIT;

	*util = uclamp_rq_util_with(cpu_rq(wg_cpu->cpu), *util, NULL);
	*util = max(min_util, *util);
}

static unsigned int waltgov_next_freq_shared(struct waltgov_cpu *wg_cpu, u64 time)
{
	struct waltgov_policy *wg_policy = wg_cpu->wg_policy;
	struct cpufreq_policy *policy = wg_policy->policy;
	unsigned long util = 0;
	unsigned int j;
	int boost = wg_policy->tunables->boost;
	unsigned long max = arch_scale_cpu_capacity(wg_cpu->cpu);

	for_each_cpu(j, policy->cpus) {
		struct waltgov_cpu *j_wg_cpu = &per_cpu(waltgov_cpu, j);
		unsigned long j_util, j_nl;

		j_util = j_wg_cpu->util;
		j_nl = j_wg_cpu->walt_load.nl;
		if (boost) {
			j_util = mult_frac(j_util, boost + 100, 100);
			j_nl = mult_frac(j_nl, boost + 100, 100);
		}

		if (j_util > util) {
			util = j_util;
			wg_policy->driving_cpu = j;
		}

		waltgov_walt_adjust(j_wg_cpu, j_util, j_nl, &util, &max);
	}

	return get_next_freq(wg_policy, util, max, wg_cpu, time);
}

static void waltgov_update_smart_freq(struct waltgov_callback *cb, u64 time,
				unsigned int flags)
{
	struct waltgov_cpu *wg_cpu = container_of(cb, struct waltgov_cpu, cb);
	struct waltgov_policy *wg_policy = wg_cpu->wg_policy;
	unsigned int next_f;

	raw_spin_lock(&wg_policy->update_lock);

	wg_policy->ipc_smart_freq = get_cluster_ipc_level_freq(wg_cpu->cpu, time);
	update_smart_freq_capacities_one_cluster(cpu_cluster(wg_cpu->cpu));

	next_f = waltgov_next_freq_shared(wg_cpu, time);

	if (!next_f)
		goto out;

	if (wg_policy->policy->fast_switch_enabled)
		waltgov_fast_switch(wg_policy, time, next_f);
	else
		waltgov_deferred_update(wg_policy, time, next_f);

out:
	raw_spin_unlock(&wg_policy->update_lock);
}

static void waltgov_update_freq(struct waltgov_callback *cb, u64 time,
				unsigned int flags)
{
	struct waltgov_cpu *wg_cpu = container_of(cb, struct waltgov_cpu, cb);
	struct waltgov_policy *wg_policy = wg_cpu->wg_policy;
	unsigned int next_f;

	if (flags & WALT_CPUFREQ_SMART_FREQ_BIT) {
		waltgov_update_smart_freq(cb, time, flags);
		return;
	}

	if (!wg_policy->tunables->pl && flags & WALT_CPUFREQ_PL_BIT)
		return;

	wg_cpu->util = cpu_util_freq_walt(wg_cpu->cpu, &wg_cpu->walt_load, &wg_cpu->reasons);
	wg_cpu->flags = flags;
	raw_spin_lock(&wg_policy->update_lock);

	waltgov_calc_avg_cap(wg_policy, wg_cpu->walt_load.ws,
			   wg_policy->policy->cur);

	trace_waltgov_util_update(wg_cpu->cpu, wg_cpu->util, wg_policy->avg_cap,
				arch_scale_cpu_capacity(wg_cpu->cpu),
				wg_cpu->walt_load.nl,
				wg_cpu->walt_load.pl,
				wg_cpu->walt_load.rtgb_active, flags,
				wg_policy->tunables->boost);

	if (waltgov_should_update_freq(wg_policy, time) &&
	    !(flags & WALT_CPUFREQ_CONTINUE_BIT)) {
		next_f = waltgov_next_freq_shared(wg_cpu, time);

		if (!next_f)
			goto out;

		if (wg_policy->policy->fast_switch_enabled)
			waltgov_fast_switch(wg_policy, time, next_f);
		else
			waltgov_deferred_update(wg_policy, time, next_f);
	}

out:
	raw_spin_unlock(&wg_policy->update_lock);
}

static void waltgov_work(struct kthread_work *work)
{
	struct waltgov_policy *wg_policy = container_of(work, struct waltgov_policy, work);
	unsigned int freq;
	unsigned long flags;

	raw_spin_lock_irqsave(&wg_policy->update_lock, flags);
	freq = wg_policy->next_freq;
	waltgov_track_cycles(wg_policy, wg_policy->policy->cur,
			   walt_sched_clock());
	raw_spin_unlock_irqrestore(&wg_policy->update_lock, flags);

	mutex_lock(&wg_policy->work_lock);
	__cpufreq_driver_target(wg_policy->policy, freq, CPUFREQ_RELATION_L);
	mutex_unlock(&wg_policy->work_lock);
}

static void waltgov_irq_work(struct irq_work *irq_work)
{
	struct waltgov_policy *wg_policy;

	wg_policy = container_of(irq_work, struct waltgov_policy, irq_work);

	kthread_queue_work(&wg_policy->worker, &wg_policy->work);
}

/************************** sysfs interface ************************/

static inline struct waltgov_tunables *to_waltgov_tunables(struct gov_attr_set *attr_set)
{
	return container_of(attr_set, struct waltgov_tunables, attr_set);
}

static DEFINE_MUTEX(min_rate_lock);

static void update_min_rate_limit_ns(struct waltgov_policy *wg_policy)
{
	mutex_lock(&min_rate_lock);
	wg_policy->min_rate_limit_ns = min(wg_policy->up_rate_delay_ns,
					   wg_policy->down_rate_delay_ns);
	mutex_unlock(&min_rate_lock);
}

static ssize_t up_rate_limit_us_show(struct gov_attr_set *attr_set, char *buf)
{
	struct waltgov_tunables *tunables = to_waltgov_tunables(attr_set);

	return scnprintf(buf, PAGE_SIZE, "%u\n", tunables->up_rate_limit_us);
}

static ssize_t down_rate_limit_us_show(struct gov_attr_set *attr_set, char *buf)
{
	struct waltgov_tunables *tunables = to_waltgov_tunables(attr_set);

	return scnprintf(buf, PAGE_SIZE, "%u\n", tunables->down_rate_limit_us);
}

static ssize_t up_rate_limit_us_store(struct gov_attr_set *attr_set,
				      const char *buf, size_t count)
{
	struct waltgov_tunables *tunables = to_waltgov_tunables(attr_set);
	struct waltgov_policy *wg_policy;
	unsigned int rate_limit_us;

	if (kstrtouint(buf, 10, &rate_limit_us))
		return -EINVAL;

	tunables->up_rate_limit_us = rate_limit_us;

	list_for_each_entry(wg_policy, &attr_set->policy_list, tunables_hook) {
		wg_policy->up_rate_delay_ns = rate_limit_us * NSEC_PER_USEC;
		update_min_rate_limit_ns(wg_policy);
	}

	return count;
}

static ssize_t down_rate_limit_us_store(struct gov_attr_set *attr_set,
					const char *buf, size_t count)
{
	struct waltgov_tunables *tunables = to_waltgov_tunables(attr_set);
	struct waltgov_policy *wg_policy;
	unsigned int rate_limit_us;

	if (kstrtouint(buf, 10, &rate_limit_us))
		return -EINVAL;

	tunables->down_rate_limit_us = rate_limit_us;

	list_for_each_entry(wg_policy, &attr_set->policy_list, tunables_hook) {
		wg_policy->down_rate_delay_ns = rate_limit_us * NSEC_PER_USEC;
		update_min_rate_limit_ns(wg_policy);
	}

	return count;
}

static struct governor_attr up_rate_limit_us = __ATTR_RW(up_rate_limit_us);
static struct governor_attr down_rate_limit_us = __ATTR_RW(down_rate_limit_us);

static ssize_t hispeed_load_show(struct gov_attr_set *attr_set, char *buf)
{
	struct waltgov_tunables *tunables = to_waltgov_tunables(attr_set);

	return scnprintf(buf, PAGE_SIZE, "%u\n", tunables->hispeed_load);
}

static ssize_t hispeed_load_store(struct gov_attr_set *attr_set,
				  const char *buf, size_t count)
{
	struct waltgov_tunables *tunables = to_waltgov_tunables(attr_set);

	if (kstrtouint(buf, 10, &tunables->hispeed_load))
		return -EINVAL;

	tunables->hispeed_load = min(100U, tunables->hispeed_load);

	return count;
}

static ssize_t hispeed_freq_show(struct gov_attr_set *attr_set, char *buf)
{
	struct waltgov_tunables *tunables = to_waltgov_tunables(attr_set);

	return scnprintf(buf, PAGE_SIZE, "%u\n", tunables->hispeed_freq);
}

static ssize_t hispeed_freq_store(struct gov_attr_set *attr_set,
					const char *buf, size_t count)
{
	struct waltgov_tunables *tunables = to_waltgov_tunables(attr_set);
	unsigned int val;

	if (kstrtouint(buf, 10, &val))
		return -EINVAL;

	tunables->hispeed_freq = val;

	return count;
}

static ssize_t hispeed_cond_freq_show(struct gov_attr_set *attr_set, char *buf)
{
	struct waltgov_tunables *tunables = to_waltgov_tunables(attr_set);

	return scnprintf(buf, PAGE_SIZE, "%u\n", tunables->hispeed_cond_freq);
}

static ssize_t hispeed_cond_freq_store(struct gov_attr_set *attr_set,
				  const char *buf, size_t count)
{
	struct waltgov_tunables *tunables = to_waltgov_tunables(attr_set);
	struct waltgov_policy *wg_policy;
	unsigned long flags;

	if (kstrtouint(buf, 10, &tunables->hispeed_cond_freq))
		return -EINVAL;

	list_for_each_entry(wg_policy, &attr_set->policy_list, tunables_hook) {
		raw_spin_lock_irqsave(&wg_policy->update_lock, flags);
		wg_policy->hispeed_cond_util =  freq_to_util(wg_policy,
							tunables->hispeed_cond_freq);
		raw_spin_unlock_irqrestore(&wg_policy->update_lock, flags);
	}
	return count;
}

static ssize_t rtg_boost_freq_show(struct gov_attr_set *attr_set, char *buf)
{
	struct waltgov_tunables *tunables = to_waltgov_tunables(attr_set);

	return scnprintf(buf, PAGE_SIZE, "%u\n", tunables->rtg_boost_freq);
}

static ssize_t rtg_boost_freq_store(struct gov_attr_set *attr_set,
				    const char *buf, size_t count)
{
	struct waltgov_tunables *tunables = to_waltgov_tunables(attr_set);
	unsigned int val;

	if (kstrtouint(buf, 10, &val))
		return -EINVAL;

	tunables->rtg_boost_freq = val;

	return count;
}

static ssize_t pl_show(struct gov_attr_set *attr_set, char *buf)
{
	struct waltgov_tunables *tunables = to_waltgov_tunables(attr_set);

	return scnprintf(buf, PAGE_SIZE, "%u\n", tunables->pl);
}

static ssize_t pl_store(struct gov_attr_set *attr_set, const char *buf,
				   size_t count)
{
	struct waltgov_tunables *tunables = to_waltgov_tunables(attr_set);

	if (kstrtobool(buf, &tunables->pl))
		return -EINVAL;

	return count;
}

static ssize_t boost_show(struct gov_attr_set *attr_set, char *buf)
{
	struct waltgov_tunables *tunables = to_waltgov_tunables(attr_set);

	return scnprintf(buf, PAGE_SIZE, "%d\n", tunables->boost);
}

static ssize_t boost_store(struct gov_attr_set *attr_set, const char *buf,
				   size_t count)
{
	struct waltgov_tunables *tunables = to_waltgov_tunables(attr_set);
	struct waltgov_policy *wg_policy;
	int val;

	if (kstrtoint(buf, 10, &val))
		return -EINVAL;

	if (val < -100 || val > 1000)
		return -EINVAL;

	tunables->boost = val;
	list_for_each_entry(wg_policy, &attr_set->policy_list, tunables_hook) {
		struct rq *rq = cpu_rq(wg_policy->policy->cpu);
		unsigned long flags;

		raw_spin_lock_irqsave(&rq->__lock, flags);
		waltgov_run_callback(rq, WALT_CPUFREQ_BOOST_UPDATE_BIT);
		raw_spin_unlock_irqrestore(&rq->__lock, flags);
	}
	return count;
}

/*
 * update_util_inflate_factor() - Updates the zone ranges and the equivalent
 * utilization inflation factor for each zone. This function stores the updated
 * values gathered from sysfs node in the internally maintained wg_policy
 * structure data member, zone_util.
 */
void update_util_inflate_factor(struct waltgov_tunables *tunables,
		struct waltgov_policy *wg_policy)
{
	unsigned long fmax, cap;
	int i = 0;

	fmax = wg_policy->policy->cpuinfo.max_freq;
	cap = arch_scale_cpu_capacity(wg_policy->policy->cpu);

	for (i = 0; i < MAX_ZONES; i++) {
		if (tunables->zone_util_pct[i][0] == -1)
			break;

		wg_policy->zone_util[i].util_thresh =
			tunables->zone_util_pct[i][0] * cap/fmax;
		wg_policy->zone_util[i].inflate_factor =
			((fmax * 100) / tunables->zone_util_pct[i][1]);
	}
}

static ssize_t zone_max_util_pct_show(struct gov_attr_set *attr_set, char *buf)
{
	struct waltgov_tunables *tunables = to_waltgov_tunables(attr_set);
	ssize_t len = 0;
	int i, j;

	for (i = 0; i < MAX_ZONES; i++) {
		if (tunables->zone_util_pct[i][0] == -1)
			break;

		for (j = 0; j < ZONE_TUPLE_SIZE; j++) {
			len += scnprintf(buf + len, PAGE_SIZE, "%d ",
					tunables->zone_util_pct[i][j]);
		}
	}
	len += scnprintf(buf + len, PAGE_SIZE, "\n");

	return len;
}


int write_once_zone_max_util_pct_cluster[MAX_CLUSTERS];

static ssize_t zone_max_util_pct_store(struct gov_attr_set *attr_set,
		const char *buf, size_t count)
{
	struct waltgov_tunables *tunables = to_waltgov_tunables(attr_set);
	struct waltgov_policy *wg_policy;
	unsigned int value;
	static DEFINE_MUTEX(target_load_lock);
	size_t size = 0;
	unsigned long flags;
	char *token;
	char str[1024];
	char *ex;
	ssize_t ret;
	int temp[MAX_ZONES*ZONE_TUPLE_SIZE];
	int temp2[MAX_ZONES*ZONE_TUPLE_SIZE];
	int i, j, k;
	struct walt_sched_cluster *cluster;

	size = strlen(buf) + 1;
	ex = str;
	strscpy(str, buf, size);

	size = 0;
	mutex_lock(&target_load_lock);

	ret = -EINVAL;

	/*
	 * read and write values one by one and stores it in temp array which we got from user.
	 */
	while ((token = strsep(&ex, " ")) != NULL) {
		if (size >= MAX_ZONES*ZONE_TUPLE_SIZE)
			goto exit;

		if (kstrtouint(token, 10, &value) == 0)
			temp[size++] = value;
		else
			goto exit;
	}

	/* Check if every zone value has a corresponding target load percentage value */
	if (size%2 != 0)
		goto exit;

	list_for_each_entry(wg_policy, &attr_set->policy_list, tunables_hook) {
		cluster = cpu_cluster(wg_policy->policy->cpu);

		/*
		 * Check if target load percentange entered for zones are in range
		 * and greater than 80%.
		 */
		for (i = 1; i < size; i += 2) {
			if (temp[i] < 80 || temp[i] > 100)
				goto exit;

			if (i > 1 && temp[i] > temp[i-2])
				goto exit;
		}

		k = 0;
		/*
		 * If a user specifies target load to be lower than the previous one
		 * but we need to maintain the restriction of it being in descending
		 * order.
		 */
		if (write_once_zone_max_util_pct_cluster[cluster->id]) {

			for (i = 0; i < MAX_ZONES; i++) {
				for (j = 0; j < ZONE_TUPLE_SIZE; j++)
					temp2[k++] = tunables->zone_util_pct[i][j];
			}

			for (i = 0; i < MAX_ZONES; i++) {

				if (tunables->zone_util_pct[i][0] == -1)
					break;

				for (j = 0; j < MAX_ZONES*ZONE_TUPLE_SIZE; j += 2) {
					if (temp[j] == tunables->zone_util_pct[i][0]) {
						temp2[(2*i+1)] = temp[j+1];
						break;
					}
				}
			}

			for (i = 1; i < MAX_ZONES*ZONE_TUPLE_SIZE; i += 2) {
				if (temp2[i] == -1)
					break;

				if (temp2[i] < 80 || temp2[i] > 100)
					goto exit;

				if (i > 1 && temp2[i] > temp2[i-2])
					goto exit;

			}
		}
		/*
		 * If a user enters a specified target load percentage for a defined zone already.
		 * We update it here. If user enters some undefined zone, then that will be ignored.
		 */
		if (write_once_zone_max_util_pct_cluster[cluster->id]) {
			for (i = 0; i < MAX_ZONES; i++) {
				if (tunables->zone_util_pct[i][0] == -1)
					break;

				for (j = 0; j < MAX_ZONES*ZONE_TUPLE_SIZE; j += 2) {
					if (temp[j] == tunables->zone_util_pct[i][0])
						tunables->zone_util_pct[i][1] = temp[j+1];
				}
			}
		}

		k = 0;
		/*
		 * Writing once initially for all the zones defined and equivalent target load.
		 */
		if (!write_once_zone_max_util_pct_cluster[cluster->id]) {
			for (i = 0; i < size/2; i++) {
				for (j = 0; j < 2; j++)
					tunables->zone_util_pct[i][j] = temp[k++];
			}
			write_once_zone_max_util_pct_cluster[cluster->id] = 1;
		}

		ret = count;
		raw_spin_lock_irqsave(&wg_policy->update_lock, flags);
		update_util_inflate_factor(tunables, wg_policy);
		raw_spin_unlock_irqrestore(&wg_policy->update_lock, flags);
	}

exit:
	mutex_unlock(&target_load_lock);
	return ret;
}

/**
 * cpufreq_walt_set_adaptive_freq() - set the waltgov adaptive freq for cpu
 * @cpu:               the cpu for which the values should be set
 * @adaptive_level_1: level 1 freq
 * @adaptive_low_freq: low freq (i.e. level 2 freq)
 * @adaptive_high_freq: high_freq (i.e. level 3 freq)
 *
 * Configure the adaptive_low/high_freq for the cpu specified. This will impact all
 * cpus governed by the policy (e.g. all cpus in a cluster). The actual value used
 * for adaptive frequencies will be governed by the user space setting for the
 * policy, and this value.
 *
 * Return: 0 if successful, error otherwise
 */
int cpufreq_walt_set_adaptive_freq(unsigned int cpu,
				unsigned int adaptive_level_1,
				unsigned int adaptive_low_freq,
				unsigned int adaptive_high_freq)
{
	struct waltgov_cpu *wg_cpu = &per_cpu(waltgov_cpu, cpu);
	struct waltgov_policy *wg_policy;
	struct cpufreq_policy *policy;

	if (unlikely(walt_disabled))
		return -EAGAIN;

	if (!cpu_possible(cpu))
		return -EFAULT;

	wg_policy = wg_cpu->wg_policy;
	policy = wg_policy->policy;

	/*
	 * To maintain backwards compatibility, ensure that adaptive_low_freq,
	 * which is effectively the same thing as adaptive_level_2, is able to be set
	 * even if adaptive_level_1 is unset. In this case, simply set adaptive_level_1
	 * to be the same as adaptive_low_freq.
	 */
	if (adaptive_low_freq && !adaptive_level_1)
		adaptive_level_1 = adaptive_low_freq;

	if (policy->min <= adaptive_level_1 && policy->max >= adaptive_high_freq &&
		adaptive_low_freq >= adaptive_level_1 && adaptive_low_freq <= adaptive_high_freq) {
		wg_policy->tunables->adaptive_level_1_kernel = adaptive_level_1;
		wg_policy->tunables->adaptive_low_freq_kernel = adaptive_low_freq;
		wg_policy->tunables->adaptive_high_freq_kernel = adaptive_high_freq;
		return 0;
	}

	return -EINVAL;
}
EXPORT_SYMBOL_GPL(cpufreq_walt_set_adaptive_freq);

/**
 * cpufreq_walt_get_adaptive_freq() - get the waltgov adaptive freq for cpu
 * @cpu:               the cpu for which the values should be returned
 * @adaptive_level_1: pointer to write the current kernel adaptive_level_1 freq value
 * @adaptive_low_freq: pointer to write the current kernel adaptive_low_freq value
 * @adaptive_high_freq:pointer to write the current kernel adaptive_high_freq value
 *
 * Get the currently active adaptive_low/high_freq for the cpu specified.
 *
 * Return: 0 if successful, error otherwise
 */
int cpufreq_walt_get_adaptive_freq(unsigned int cpu,
				unsigned int *adaptive_level_1,
				unsigned int *adaptive_low_freq,
				unsigned int *adaptive_high_freq)
{
	struct waltgov_cpu *wg_cpu = &per_cpu(waltgov_cpu, cpu);
	struct waltgov_policy *wg_policy;

	if (unlikely(walt_disabled))
		return -EAGAIN;

	if (!cpu_possible(cpu))
		return -EFAULT;

	wg_policy = wg_cpu->wg_policy;
	if (adaptive_level_1 && adaptive_low_freq && adaptive_high_freq) {
		*adaptive_level_1 = get_adaptive_level_1(wg_policy);
		*adaptive_low_freq = get_adaptive_low_freq(wg_policy);
		*adaptive_high_freq = get_adaptive_high_freq(wg_policy);
		return 0;
	}

	return -EINVAL;
}
EXPORT_SYMBOL_GPL(cpufreq_walt_get_adaptive_freq);

/**
 * cpufreq_walt_reset_adaptive_freq() - reset the waltgov adaptive freq for cpu
 * @cpu:               the cpu for which the values should be set
 *
 * Reset the kernel adaptive_low/high_freq to zero.
 *
 * Return: 0 if successful, error otherwise
 */
int cpufreq_walt_reset_adaptive_freq(unsigned int cpu)
{
	struct waltgov_cpu *wg_cpu = &per_cpu(waltgov_cpu, cpu);
	struct waltgov_policy *wg_policy;

	if (unlikely(walt_disabled))
		return -EAGAIN;

	if (!cpu_possible(cpu))
		return -EFAULT;

	wg_policy = wg_cpu->wg_policy;
	wg_policy->tunables->adaptive_level_1_kernel = 0;
	wg_policy->tunables->adaptive_low_freq_kernel = 0;
	wg_policy->tunables->adaptive_high_freq_kernel = 0;

	return 0;
}
EXPORT_SYMBOL_GPL(cpufreq_walt_reset_adaptive_freq);

#define WALTGOV_ATTR_RW(_name)						\
static struct governor_attr _name =					\
__ATTR(_name, 0644, show_##_name, store_##_name)			\

#define show_attr(name)							\
static ssize_t show_##name(struct gov_attr_set *attr_set, char *buf)	\
{									\
	struct waltgov_tunables *tunables = to_waltgov_tunables(attr_set);	\
	return scnprintf(buf, PAGE_SIZE, "%lu\n", (unsigned long)tunables->name);	\
}									\

#define store_attr(name)						\
static ssize_t store_##name(struct gov_attr_set *attr_set,		\
				const char *buf, size_t count)		\
{									\
	struct waltgov_tunables *tunables = to_waltgov_tunables(attr_set);	\
										\
	if (kstrtouint(buf, 10, &tunables->name))			\
		return -EINVAL;						\
									\
	return count;							\
}									\

show_attr(adaptive_level_1);
store_attr(adaptive_level_1);
show_attr(adaptive_low_freq);
store_attr(adaptive_low_freq);
show_attr(adaptive_high_freq);
store_attr(adaptive_high_freq);

static struct governor_attr hispeed_load = __ATTR_RW(hispeed_load);
static struct governor_attr hispeed_freq = __ATTR_RW(hispeed_freq);
static struct governor_attr hispeed_cond_freq = __ATTR_RW(hispeed_cond_freq);
static struct governor_attr rtg_boost_freq = __ATTR_RW(rtg_boost_freq);
static struct governor_attr pl = __ATTR_RW(pl);
static struct governor_attr boost = __ATTR_RW(boost);
static struct governor_attr zone_max_util_pct = __ATTR_RW(zone_max_util_pct);
WALTGOV_ATTR_RW(adaptive_level_1);
WALTGOV_ATTR_RW(adaptive_low_freq);
WALTGOV_ATTR_RW(adaptive_high_freq);

static struct attribute *waltgov_attrs[] = {
	&up_rate_limit_us.attr,
	&down_rate_limit_us.attr,
	&hispeed_load.attr,
	&hispeed_freq.attr,
	&hispeed_cond_freq.attr,
	&rtg_boost_freq.attr,
	&pl.attr,
	&boost.attr,
	&adaptive_level_1.attr,
	&adaptive_low_freq.attr,
	&adaptive_high_freq.attr,
	&zone_max_util_pct.attr,
	NULL
};
ATTRIBUTE_GROUPS(waltgov);

static const struct kobj_type waltgov_tunables_ktype = {
	.default_groups = waltgov_groups,
	.sysfs_ops	= &governor_sysfs_ops,
};

/********************** cpufreq governor interface *********************/

static struct cpufreq_governor walt_gov;

static struct waltgov_policy *waltgov_policy_alloc(struct cpufreq_policy *policy)
{
	struct waltgov_policy *wg_policy;

	wg_policy = kzalloc(sizeof(*wg_policy), GFP_KERNEL);
	if (!wg_policy)
		return NULL;

	wg_policy->policy = policy;
	raw_spin_lock_init(&wg_policy->update_lock);
	return wg_policy;
}

static void waltgov_policy_free(struct waltgov_policy *wg_policy)
{
	kfree(wg_policy);
}

static int waltgov_kthread_create(struct waltgov_policy *wg_policy)
{
	struct task_struct *thread;
	struct sched_param param = { .sched_priority = MAX_RT_PRIO / 2 };
	struct cpufreq_policy *policy = wg_policy->policy;
	int ret;

	/* kthread only required for slow path */
	if (policy->fast_switch_enabled)
		return 0;

	kthread_init_work(&wg_policy->work, waltgov_work);
	kthread_init_worker(&wg_policy->worker);
	thread = kthread_create(kthread_worker_fn, &wg_policy->worker,
				"waltgov:%d",
				cpumask_first(policy->related_cpus));
	if (IS_ERR(thread)) {
		pr_err("failed to create waltgov thread: %ld\n", PTR_ERR(thread));
		return PTR_ERR(thread);
	}

	ret = sched_setscheduler_nocheck(thread, SCHED_FIFO, &param);
	if (ret) {
		kthread_stop(thread);
		pr_warn("%s: failed to set SCHED_FIFO\n", __func__);
		return ret;
	}

	wg_policy->thread = thread;
	kthread_bind_mask(thread, policy->related_cpus);
	init_irq_work(&wg_policy->irq_work, waltgov_irq_work);
	mutex_init(&wg_policy->work_lock);

	wake_up_process(thread);

	return 0;
}

static void waltgov_kthread_stop(struct waltgov_policy *wg_policy)
{
	/* kthread only required for slow path */
	if (wg_policy->policy->fast_switch_enabled)
		return;

	kthread_flush_worker(&wg_policy->worker);
	kthread_stop(wg_policy->thread);
	mutex_destroy(&wg_policy->work_lock);
}

static void waltgov_tunables_save(struct cpufreq_policy *policy,
		struct waltgov_tunables *tunables)
{
	int cpu, i, j;
	struct waltgov_tunables *cached = per_cpu(cached_tunables, policy->cpu);

	if (!cached) {
		cached = kzalloc(sizeof(*tunables), GFP_KERNEL);
		if (!cached)
			return;

		for_each_cpu(cpu, policy->related_cpus)
			per_cpu(cached_tunables, cpu) = cached;
	}

	cached->pl = tunables->pl;
	cached->hispeed_load = tunables->hispeed_load;
	cached->rtg_boost_freq = tunables->rtg_boost_freq;
	cached->hispeed_freq = tunables->hispeed_freq;
	cached->hispeed_cond_freq = tunables->hispeed_cond_freq;
	cached->up_rate_limit_us = tunables->up_rate_limit_us;
	cached->down_rate_limit_us = tunables->down_rate_limit_us;
	cached->boost = tunables->boost;
	cached->adaptive_level_1 = tunables->adaptive_level_1;
	cached->adaptive_low_freq = tunables->adaptive_low_freq;
	cached->adaptive_high_freq = tunables->adaptive_high_freq;
	cached->adaptive_level_1_kernel = tunables->adaptive_level_1_kernel;
	cached->adaptive_low_freq_kernel = tunables->adaptive_low_freq_kernel;
	cached->adaptive_high_freq_kernel = tunables->adaptive_high_freq_kernel;
	for (i = 0; i < MAX_ZONES; i++) {
		for (j = 0; j < ZONE_TUPLE_SIZE; j++)
			cached->zone_util_pct[i][j] = tunables->zone_util_pct[i][j];
	}
}

static void waltgov_tunables_restore(struct cpufreq_policy *policy)
{
	struct waltgov_policy *wg_policy = policy->governor_data;
	struct waltgov_tunables *tunables = wg_policy->tunables;
	struct waltgov_tunables *cached = per_cpu(cached_tunables, policy->cpu);
	int i, j;

	if (!cached)
		return;

	tunables->pl = cached->pl;
	tunables->hispeed_load = cached->hispeed_load;
	tunables->rtg_boost_freq = cached->rtg_boost_freq;
	tunables->hispeed_freq = cached->hispeed_freq;
	tunables->hispeed_cond_freq = cached->hispeed_cond_freq;
	tunables->up_rate_limit_us = cached->up_rate_limit_us;
	tunables->down_rate_limit_us = cached->down_rate_limit_us;
	tunables->boost	= cached->boost;
	tunables->adaptive_level_1 = cached->adaptive_level_1;
	tunables->adaptive_low_freq = cached->adaptive_low_freq;
	tunables->adaptive_high_freq = cached->adaptive_high_freq;
	tunables->adaptive_level_1_kernel = cached->adaptive_level_1_kernel;
	tunables->adaptive_low_freq_kernel = cached->adaptive_low_freq_kernel;
	tunables->adaptive_high_freq_kernel = cached->adaptive_high_freq_kernel;
	for (i = 0; i < MAX_ZONES; i++) {
		for (j = 0; j < ZONE_TUPLE_SIZE; j++)
			tunables->zone_util_pct[i][j] = cached->zone_util_pct[i][j];
	}

	update_util_inflate_factor(tunables, wg_policy);
}

bool waltgov_disabled = true;
static int waltgov_init(struct cpufreq_policy *policy)
{
	struct waltgov_policy *wg_policy;
	struct waltgov_tunables *tunables;
	int ret = 0;

	/* State should be equivalent to EXIT */
	if (policy->governor_data)
		return -EBUSY;

	cpufreq_enable_fast_switch(policy);

	if (policy->fast_switch_possible && !policy->fast_switch_enabled)
		BUG_ON(1);

	wg_policy = waltgov_policy_alloc(policy);
	if (!wg_policy) {
		ret = -ENOMEM;
		goto disable_fast_switch;
	}

	ret = waltgov_kthread_create(wg_policy);
	if (ret)
		goto free_wg_policy;

	tunables = kzalloc(sizeof(*tunables), GFP_KERNEL);
	if (!tunables) {
		ret = -ENOMEM;
		goto stop_kthread;
	}

	gov_attr_set_init(&tunables->attr_set, &wg_policy->tunables_hook);
	tunables->hispeed_load = DEFAULT_HISPEED_LOAD;

	/*
	 * Initialize each zone and its util inflate factor to -1 during
	 * cpufreq_walt initialization.
	 */
	memset(tunables->zone_util_pct,
			-1, sizeof(tunables->zone_util_pct));
	memset(wg_policy->zone_util, -1, sizeof(wg_policy->zone_util));

	if (is_min_possible_cluster_cpu(policy->cpu))
		tunables->rtg_boost_freq = DEFAULT_SILVER_RTG_BOOST_FREQ;
	else if (is_max_possible_cluster_cpu(policy->cpu))
		tunables->rtg_boost_freq = DEFAULT_PRIME_RTG_BOOST_FREQ;
	else
		tunables->rtg_boost_freq = DEFAULT_GOLD_RTG_BOOST_FREQ;

	policy->governor_data = wg_policy;
	wg_policy->tunables = tunables;
	waltgov_tunables_restore(policy);

	ret = kobject_init_and_add(&tunables->attr_set.kobj, &waltgov_tunables_ktype,
				   get_governor_parent_kobj(policy), "%s",
				   walt_gov.name);
	if (ret)
		goto fail;

	return 0;

fail:
	kobject_put(&tunables->attr_set.kobj);
	policy->governor_data = NULL;
	kfree(tunables);
stop_kthread:
	waltgov_kthread_stop(wg_policy);
free_wg_policy:
	waltgov_policy_free(wg_policy);
disable_fast_switch:
	cpufreq_disable_fast_switch(policy);

	pr_err("initialization failed (error %d)\n", ret);
	return ret;
}

static void waltgov_exit(struct cpufreq_policy *policy)
{
	struct waltgov_policy *wg_policy = policy->governor_data;
	struct waltgov_tunables *tunables = wg_policy->tunables;
	unsigned int count;

	count = gov_attr_set_put(&tunables->attr_set, &wg_policy->tunables_hook);
	policy->governor_data = NULL;
	if (!count) {
		waltgov_tunables_save(policy, tunables);
		kfree(tunables);
	}

	waltgov_kthread_stop(wg_policy);
	waltgov_policy_free(wg_policy);
	cpufreq_disable_fast_switch(policy);
}

static int waltgov_start(struct cpufreq_policy *policy)
{
	struct waltgov_policy *wg_policy = policy->governor_data;
	unsigned int cpu;

	wg_policy->up_rate_delay_ns =
		wg_policy->tunables->up_rate_limit_us * NSEC_PER_USEC;
	wg_policy->down_rate_delay_ns =
		wg_policy->tunables->down_rate_limit_us * NSEC_PER_USEC;
	update_min_rate_limit_ns(wg_policy);
	wg_policy->last_freq_update_time	= 0;
	wg_policy->next_freq			= 0;
	wg_policy->limits_changed		= false;
	wg_policy->need_freq_update		= false;
	wg_policy->cached_raw_freq		= 0;

	for_each_cpu(cpu, policy->cpus) {
		struct waltgov_cpu *wg_cpu = &per_cpu(waltgov_cpu, cpu);

		memset(wg_cpu, 0, sizeof(*wg_cpu));
		wg_cpu->cpu			= cpu;
		wg_cpu->wg_policy		= wg_policy;
	}

	for_each_cpu(cpu, policy->cpus) {
		struct waltgov_cpu *wg_cpu = &per_cpu(waltgov_cpu, cpu);

		waltgov_add_callback(cpu, &wg_cpu->cb, waltgov_update_freq);
	}

	waltgov_disabled = false;
	return 0;
}

static void waltgov_stop(struct cpufreq_policy *policy)
{
	struct waltgov_policy *wg_policy = policy->governor_data;
	unsigned int cpu;

	for_each_cpu(cpu, policy->cpus)
		waltgov_remove_callback(cpu);

	synchronize_rcu();

	if (!policy->fast_switch_enabled) {
		irq_work_sync(&wg_policy->irq_work);
		kthread_cancel_work_sync(&wg_policy->work);
	}

	waltgov_disabled = true;
}

static void waltgov_limits(struct cpufreq_policy *policy)
{
	struct waltgov_policy *wg_policy = policy->governor_data;
	unsigned long flags, now;
	unsigned int freq, final_freq;

	if (!policy->fast_switch_enabled) {
		mutex_lock(&wg_policy->work_lock);
		raw_spin_lock_irqsave(&wg_policy->update_lock, flags);
		waltgov_track_cycles(wg_policy, wg_policy->policy->cur,
				   walt_sched_clock());
		raw_spin_unlock_irqrestore(&wg_policy->update_lock, flags);
		cpufreq_policy_apply_limits(policy);
		mutex_unlock(&wg_policy->work_lock);
	} else {
		raw_spin_lock_irqsave(&wg_policy->update_lock, flags);
		if (!wg_policy->thermal_isolated) {
			freq = policy->cur;
			now = walt_sched_clock();
			/*
			 * cpufreq_driver_resolve_freq() has a clamp, so we do not need
			 * to do any sort of additional validation here.
			 */
			final_freq = cpufreq_driver_resolve_freq(policy, freq);
			if (wg_policy->next_freq != final_freq) {
				__waltgov_update_next_freq(wg_policy, now, final_freq, final_freq);
				waltgov_fast_switch(wg_policy, now, final_freq);
			}
		}
		raw_spin_unlock_irqrestore(&wg_policy->update_lock, flags);
	}

	wg_policy->limits_changed = true;
}

static struct cpufreq_governor walt_gov = {
	.name			= "walt",
	.init			= waltgov_init,
	.exit			= waltgov_exit,
	.start			= waltgov_start,
	.stop			= waltgov_stop,
	.limits			= waltgov_limits,
	.owner			= THIS_MODULE,
};

int waltgov_register(void)
{
	return cpufreq_register_governor(&walt_gov);
}
