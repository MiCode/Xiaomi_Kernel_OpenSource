/*
 * drivers/cpufreq/cpufreq_interactive.c
 *
 * Copyright (C) 2010 Google, Inc.
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
 * Author: Mike Chan (mike@android.com)
 *
 */

#include <linux/cpu.h>
#include <linux/cpumask.h>
#include <linux/cpufreq.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/rwsem.h>
#include <linux/sched.h>
#include <linux/sched/rt.h>
#include <linux/tick.h>
#include <linux/time.h>
#include <linux/timer.h>
#include <linux/hrtimer.h>
#include <linux/workqueue.h>
#include <linux/kthread.h>
#include <linux/slab.h>

#define CREATE_TRACE_POINTS
#include <trace/events/cpufreq_interactive.h>

static DEFINE_PER_CPU(struct update_util_data, update_util);

struct cpufreq_interactive_policyinfo {
	bool work_in_progress;
	struct irq_work irq_work;
	spinlock_t irq_work_lock; /* protects work_in_progress */
	struct timer_list policy_slack_timer;
	struct hrtimer notif_timer;
	spinlock_t load_lock; /* protects load tracking stat */
	u64 last_evaluated_jiffy;
	struct cpufreq_policy *policy;
	struct cpufreq_policy p_nolim; /* policy copy with no limits */
	struct cpufreq_frequency_table *freq_table;
	spinlock_t target_freq_lock; /*protects target freq */
	unsigned int target_freq;
	unsigned int floor_freq;
	unsigned int min_freq;
	u64 floor_validate_time;
	u64 hispeed_validate_time;
	u64 max_freq_hyst_start_time;
	struct rw_semaphore enable_sem;
	bool reject_notification;
	bool notif_pending;
	unsigned long notif_cpu;
	int governor_enabled;
	struct cpufreq_interactive_tunables *cached_tunables;
	struct sched_load *sl;
};

/* Protected by per-policy load_lock */
struct cpufreq_interactive_cpuinfo {
	u64 time_in_idle;
	u64 time_in_idle_timestamp;
	u64 cputime_speedadj;
	u64 cputime_speedadj_timestamp;
	unsigned int loadadjfreq;
};

static DEFINE_PER_CPU(struct cpufreq_interactive_policyinfo *, polinfo);
static DEFINE_PER_CPU(struct cpufreq_interactive_cpuinfo, cpuinfo);

/* realtime thread handles frequency scaling */
static struct task_struct *speedchange_task;
static cpumask_t speedchange_cpumask;
static spinlock_t speedchange_cpumask_lock;
static struct mutex gov_lock;

static int set_window_count;
static int migration_register_count;
static struct mutex sched_lock;
static cpumask_t controlled_cpus;

/* Target load.  Lower values result in higher CPU speeds. */
#define DEFAULT_TARGET_LOAD 90
static unsigned int default_target_loads[] = {DEFAULT_TARGET_LOAD};

#define DEFAULT_TIMER_RATE (20 * USEC_PER_MSEC)
#define DEFAULT_ABOVE_HISPEED_DELAY DEFAULT_TIMER_RATE
static unsigned int default_above_hispeed_delay[] = {
	DEFAULT_ABOVE_HISPEED_DELAY };

struct cpufreq_interactive_tunables {
	int usage_count;
	/* Hi speed to bump to from lo speed when load burst (default max) */
	unsigned int hispeed_freq;
	/* Go to hi speed when CPU load at or above this value. */
#define DEFAULT_GO_HISPEED_LOAD 99
	unsigned long go_hispeed_load;
	/* Target load. Lower values result in higher CPU speeds. */
	spinlock_t target_loads_lock;
	unsigned int *target_loads;
	int ntarget_loads;
	/*
	 * The minimum amount of time to spend at a frequency before we can ramp
	 * down.
	 */
#define DEFAULT_MIN_SAMPLE_TIME (80 * USEC_PER_MSEC)
	unsigned long min_sample_time;
	/*
	 * The sample rate of the timer used to increase frequency
	 */
	unsigned long timer_rate;
	/*
	 * Wait this long before raising speed above hispeed, by default a
	 * single timer interval.
	 */
	spinlock_t above_hispeed_delay_lock;
	unsigned int *above_hispeed_delay;
	int nabove_hispeed_delay;
	/* Non-zero means indefinite speed boost active */
	int boost_val;
	/* Duration of a boot pulse in usecs */
	int boostpulse_duration_val;
	/* End time of boost pulse in ktime converted to usecs */
	u64 boostpulse_endtime;
	bool boosted;
	/*
	 * Max additional time to wait in idle, beyond timer_rate, at speeds
	 * above minimum before wakeup to reduce speed, or -1 if unnecessary.
	 */
#define DEFAULT_TIMER_SLACK (4 * DEFAULT_TIMER_RATE)
	int timer_slack_val;
	bool io_is_busy;

	/* scheduler input related flags */
	bool use_sched_load;
	bool use_migration_notif;

	/*
	 * Whether to align timer windows across all CPUs. When
	 * use_sched_load is true, this flag is ignored and windows
	 * will always be aligned.
	 */
	bool align_windows;

	/*
	 * Stay at max freq for at least max_freq_hysteresis before dropping
	 * frequency.
	 */
	unsigned int max_freq_hysteresis;

	/* Ignore hispeed_freq and above_hispeed_delay for notification */
	bool ignore_hispeed_on_notif;

	/* Ignore min_sample_time for notification */
	bool fast_ramp_down;

	/* Whether to enable prediction or not */
	bool enable_prediction;
};

/* For cases where we have single governor instance for system */
static struct cpufreq_interactive_tunables *common_tunables;
static struct cpufreq_interactive_tunables *cached_common_tunables;

static struct attribute_group *get_sysfs_attr(void);

/* Round to starting jiffy of next evaluation window */
static u64 round_to_nw_start(u64 jif,
			     struct cpufreq_interactive_tunables *tunables)
{
	unsigned long step = usecs_to_jiffies(tunables->timer_rate);
	u64 ret;

	if (tunables->use_sched_load || tunables->align_windows) {
		do_div(jif, step);
		ret = (jif + 1) * step;
	} else {
		ret = jiffies + usecs_to_jiffies(tunables->timer_rate);
	}

	return ret;
}

static inline int set_window_helper(
			struct cpufreq_interactive_tunables *tunables)
{
	return sched_set_window(round_to_nw_start(get_jiffies_64(), tunables),
			 usecs_to_jiffies(tunables->timer_rate));
}

static void cpufreq_interactive_timer_resched(unsigned long cpu,
					      bool slack_only)
{
	struct cpufreq_interactive_policyinfo *ppol = per_cpu(polinfo, cpu);
	struct cpufreq_interactive_cpuinfo *pcpu;
	struct cpufreq_interactive_tunables *tunables =
		ppol->policy->governor_data;
	u64 expires;
	unsigned long flags;
	int i;

	spin_lock_irqsave(&ppol->load_lock, flags);
	expires = round_to_nw_start(ppol->last_evaluated_jiffy, tunables);
	if (!slack_only) {
		for_each_cpu(i, ppol->policy->cpus) {
			pcpu = &per_cpu(cpuinfo, i);
			pcpu->time_in_idle = get_cpu_idle_time(i,
						&pcpu->time_in_idle_timestamp,
						tunables->io_is_busy);
			pcpu->cputime_speedadj = 0;
			pcpu->cputime_speedadj_timestamp =
						pcpu->time_in_idle_timestamp;
		}
	}

	if (tunables->timer_slack_val >= 0 &&
	    ppol->target_freq > ppol->policy->min) {
		expires += usecs_to_jiffies(tunables->timer_slack_val);
		del_timer(&ppol->policy_slack_timer);
		ppol->policy_slack_timer.expires = expires;
		add_timer(&ppol->policy_slack_timer);
	}

	spin_unlock_irqrestore(&ppol->load_lock, flags);
}

static void update_util_handler(struct update_util_data *data, u64 time,
				unsigned int sched_flags)
{
	struct cpufreq_interactive_policyinfo *ppol;
	unsigned long flags;

	ppol = *this_cpu_ptr(&polinfo);
	spin_lock_irqsave(&ppol->irq_work_lock, flags);
	/*
	 * The irq-work may not be allowed to be queued up right now
	 * because work has already been queued up or is in progress.
	 */
	if (ppol->work_in_progress ||
	    sched_flags & SCHED_CPUFREQ_INTERCLUSTER_MIG)
		goto out;

	ppol->work_in_progress = true;
	irq_work_queue(&ppol->irq_work);
out:
	spin_unlock_irqrestore(&ppol->irq_work_lock, flags);
}

static inline void gov_clear_update_util(struct cpufreq_policy *policy)
{
	int i;

	for_each_cpu(i, policy->cpus)
		cpufreq_remove_update_util_hook(i);

	synchronize_sched();
}

static void gov_set_update_util(struct cpufreq_policy *policy)
{
	struct update_util_data *util;
	int cpu;

	for_each_cpu(cpu, policy->cpus) {
		util = &per_cpu(update_util, cpu);
		cpufreq_add_update_util_hook(cpu, util, update_util_handler);
	}
}

/* The caller shall take enable_sem write semaphore to avoid any timer race.
 * The policy_slack_timer must be deactivated when calling this function.
 */
static void cpufreq_interactive_timer_start(
	struct cpufreq_interactive_tunables *tunables, int cpu)
{
	struct cpufreq_interactive_policyinfo *ppol = per_cpu(polinfo, cpu);
	struct cpufreq_interactive_cpuinfo *pcpu;
	u64 expires = round_to_nw_start(ppol->last_evaluated_jiffy, tunables);
	unsigned long flags;
	int i;

	spin_lock_irqsave(&ppol->load_lock, flags);
	gov_set_update_util(ppol->policy);
	if (tunables->timer_slack_val >= 0 &&
	    ppol->target_freq > ppol->policy->min) {
		expires += usecs_to_jiffies(tunables->timer_slack_val);
		ppol->policy_slack_timer.expires = expires;
		add_timer(&ppol->policy_slack_timer);
	}

	for_each_cpu(i, ppol->policy->cpus) {
		pcpu = &per_cpu(cpuinfo, i);
		pcpu->time_in_idle =
			get_cpu_idle_time(i, &pcpu->time_in_idle_timestamp,
					  tunables->io_is_busy);
		pcpu->cputime_speedadj = 0;
		pcpu->cputime_speedadj_timestamp = pcpu->time_in_idle_timestamp;
	}
	spin_unlock_irqrestore(&ppol->load_lock, flags);
}


static unsigned int freq_to_above_hispeed_delay(
	struct cpufreq_interactive_tunables *tunables,
	unsigned int freq)
{
	int i;
	unsigned int ret;
	unsigned long flags;

	spin_lock_irqsave(&tunables->above_hispeed_delay_lock, flags);

	for (i = 0; i < tunables->nabove_hispeed_delay - 1 &&
			freq >= tunables->above_hispeed_delay[i+1]; i += 2)
		;

	ret = tunables->above_hispeed_delay[i];
	spin_unlock_irqrestore(&tunables->above_hispeed_delay_lock, flags);
	return ret;
}

static unsigned int freq_to_targetload(
	struct cpufreq_interactive_tunables *tunables, unsigned int freq)
{
	int i;
	unsigned int ret;
	unsigned long flags;

	spin_lock_irqsave(&tunables->target_loads_lock, flags);

	for (i = 0; i < tunables->ntarget_loads - 1 &&
		    freq >= tunables->target_loads[i+1]; i += 2)
		;

	ret = tunables->target_loads[i];
	spin_unlock_irqrestore(&tunables->target_loads_lock, flags);
	return ret;
}

#define DEFAULT_MAX_LOAD 100
u32 get_freq_max_load(int cpu, unsigned int freq)
{
	struct cpufreq_interactive_policyinfo *ppol = per_cpu(polinfo, cpu);

	if (!cpumask_test_cpu(cpu, &controlled_cpus))
		return DEFAULT_MAX_LOAD;

	if (have_governor_per_policy()) {
		if (!ppol || !ppol->cached_tunables)
			return DEFAULT_MAX_LOAD;
		return freq_to_targetload(ppol->cached_tunables, freq);
	}

	if (!cached_common_tunables)
		return DEFAULT_MAX_LOAD;
	return freq_to_targetload(cached_common_tunables, freq);
}

/*
 * If increasing frequencies never map to a lower target load then
 * choose_freq() will find the minimum frequency that does not exceed its
 * target load given the current load.
 */
static unsigned int choose_freq(struct cpufreq_interactive_policyinfo *pcpu,
		unsigned int loadadjfreq)
{
	unsigned int freq = pcpu->policy->cur;
	unsigned int prevfreq, freqmin, freqmax;
	unsigned int tl;
	int index;

	freqmin = 0;
	freqmax = UINT_MAX;

	do {
		prevfreq = freq;
		tl = freq_to_targetload(pcpu->policy->governor_data, freq);

		/*
		 * Find the lowest frequency where the computed load is less
		 * than or equal to the target load.
		 */

		index = cpufreq_frequency_table_target(&pcpu->p_nolim,
						       loadadjfreq / tl,
						       CPUFREQ_RELATION_L);
		freq = pcpu->freq_table[index].frequency;

		if (freq > prevfreq) {
			/* The previous frequency is too low. */
			freqmin = prevfreq;

			if (freq >= freqmax) {
				/*
				 * Find the highest frequency that is less
				 * than freqmax.
				 */
				index = cpufreq_frequency_table_target(
					    &pcpu->p_nolim,
					    freqmax - 1, CPUFREQ_RELATION_H);
				freq = pcpu->freq_table[index].frequency;

				if (freq == freqmin) {
					/*
					 * The first frequency below freqmax
					 * has already been found to be too
					 * low.  freqmax is the lowest speed
					 * we found that is fast enough.
					 */
					freq = freqmax;
					break;
				}
			}
		} else if (freq < prevfreq) {
			/* The previous frequency is high enough. */
			freqmax = prevfreq;

			if (freq <= freqmin) {
				/*
				 * Find the lowest frequency that is higher
				 * than freqmin.
				 */
				index = cpufreq_frequency_table_target(
					    &pcpu->p_nolim,
					    freqmin + 1, CPUFREQ_RELATION_L);
				freq = pcpu->freq_table[index].frequency;

				/*
				 * If freqmax is the first frequency above
				 * freqmin then we have already found that
				 * this speed is fast enough.
				 */
				if (freq == freqmax)
					break;
			}
		}

		/* If same frequency chosen as previous then done. */
	} while (freq != prevfreq);

	return freq;
}

static u64 update_load(int cpu)
{
	struct cpufreq_interactive_policyinfo *ppol = per_cpu(polinfo, cpu);
	struct cpufreq_interactive_cpuinfo *pcpu = &per_cpu(cpuinfo, cpu);
	struct cpufreq_interactive_tunables *tunables =
		ppol->policy->governor_data;
	u64 now_idle, now, active_time, delta_idle, delta_time;

	now_idle = get_cpu_idle_time(cpu, &now, tunables->io_is_busy);
	delta_idle = (now_idle - pcpu->time_in_idle);
	delta_time = (now - pcpu->time_in_idle_timestamp);

	if (delta_time <= delta_idle)
		active_time = 0;
	else
		active_time = delta_time - delta_idle;

	pcpu->cputime_speedadj += active_time * ppol->policy->cur;

	pcpu->time_in_idle = now_idle;
	pcpu->time_in_idle_timestamp = now;
	return now;
}

static unsigned int sl_busy_to_laf(struct cpufreq_interactive_policyinfo *ppol,
				   unsigned long busy)
{
	int prev_load;
	struct cpufreq_interactive_tunables *tunables =
		ppol->policy->governor_data;

	prev_load = mult_frac(ppol->policy->cpuinfo.max_freq * 100,
				busy, tunables->timer_rate);
	return prev_load;
}

#define NEW_TASK_RATIO 75
#define PRED_TOLERANCE_PCT 10
static void cpufreq_interactive_timer(int data)
{
	s64 now;
	unsigned int delta_time;
	u64 cputime_speedadj;
	int cpu_load;
	int pol_load = 0;
	struct cpufreq_interactive_policyinfo *ppol = per_cpu(polinfo, data);
	struct cpufreq_interactive_tunables *tunables =
		ppol->policy->governor_data;
	struct sched_load *sl = ppol->sl;
	struct cpufreq_interactive_cpuinfo *pcpu;
	unsigned int new_freq;
	unsigned int prev_laf = 0, t_prevlaf;
	unsigned int pred_laf = 0, t_predlaf = 0;
	unsigned int prev_chfreq, pred_chfreq, chosen_freq;
	unsigned int index;
	unsigned long flags;
	unsigned long max_cpu;
	int i, cpu;
	int new_load_pct = 0;
	int prev_l, pred_l = 0;
	struct cpufreq_govinfo govinfo;
	bool skip_hispeed_logic, skip_min_sample_time;
	bool jump_to_max_no_ts = false;
	bool jump_to_max = false;

	if (!down_read_trylock(&ppol->enable_sem))
		return;
	if (!ppol->governor_enabled)
		goto exit;

	now = ktime_to_us(ktime_get());

	spin_lock_irqsave(&ppol->target_freq_lock, flags);
	spin_lock(&ppol->load_lock);

	skip_hispeed_logic =
		tunables->ignore_hispeed_on_notif && ppol->notif_pending;
	skip_min_sample_time = tunables->fast_ramp_down && ppol->notif_pending;
	ppol->notif_pending = false;
	now = ktime_to_us(ktime_get());
	ppol->last_evaluated_jiffy = get_jiffies_64();

	if (tunables->use_sched_load)
		sched_get_cpus_busy(sl, ppol->policy->cpus);
	max_cpu = cpumask_first(ppol->policy->cpus);
	i = 0;
	for_each_cpu(cpu, ppol->policy->cpus) {
		pcpu = &per_cpu(cpuinfo, cpu);
		if (tunables->use_sched_load) {
			t_prevlaf = sl_busy_to_laf(ppol, sl[i].prev_load);
			prev_l = t_prevlaf / ppol->target_freq;
			if (tunables->enable_prediction) {
				t_predlaf = sl_busy_to_laf(ppol,
						sl[i].predicted_load);
				pred_l = t_predlaf / ppol->target_freq;
			}
			if (sl[i].prev_load)
				new_load_pct = sl[i].new_task_load * 100 /
							sl[i].prev_load;
			else
				new_load_pct = 0;
		} else {
			now = update_load(cpu);
			delta_time = (unsigned int)
				(now - pcpu->cputime_speedadj_timestamp);
			if (WARN_ON_ONCE(!delta_time))
				continue;
			cputime_speedadj = pcpu->cputime_speedadj;
			do_div(cputime_speedadj, delta_time);
			t_prevlaf = (unsigned int)cputime_speedadj * 100;
			prev_l = t_prevlaf / ppol->target_freq;
		}

		/* find max of loadadjfreq inside policy */
		if (t_prevlaf > prev_laf) {
			prev_laf = t_prevlaf;
			max_cpu = cpu;
		}
		pred_laf = max(t_predlaf, pred_laf);

		cpu_load = max(prev_l, pred_l);
		pol_load = max(pol_load, cpu_load);
		trace_cpufreq_interactive_cpuload(cpu, cpu_load, new_load_pct,
						  prev_l, pred_l);

		/* save loadadjfreq for notification */
		pcpu->loadadjfreq = max(t_prevlaf, t_predlaf);

		/* detect heavy new task and jump to policy->max */
		if (prev_l >= tunables->go_hispeed_load &&
		    new_load_pct >= NEW_TASK_RATIO) {
			skip_hispeed_logic = true;
			jump_to_max = true;
		}
		i++;
	}
	spin_unlock(&ppol->load_lock);

	tunables->boosted = tunables->boost_val || now < tunables->boostpulse_endtime;

	prev_chfreq = choose_freq(ppol, prev_laf);
	pred_chfreq = choose_freq(ppol, pred_laf);
	chosen_freq = max(prev_chfreq, pred_chfreq);

	if (prev_chfreq < ppol->policy->max && pred_chfreq >= ppol->policy->max)
		if (!jump_to_max)
			jump_to_max_no_ts = true;

	if (now - ppol->max_freq_hyst_start_time <
	    tunables->max_freq_hysteresis &&
	    pol_load >= tunables->go_hispeed_load &&
	    ppol->target_freq < ppol->policy->max) {
		skip_hispeed_logic = true;
		skip_min_sample_time = true;
		if (!jump_to_max)
			jump_to_max_no_ts = true;
	}

	new_freq = chosen_freq;
	if (jump_to_max_no_ts || jump_to_max) {
		new_freq = ppol->policy->cpuinfo.max_freq;
	} else if (!skip_hispeed_logic) {
		if (pol_load >= tunables->go_hispeed_load ||
		    tunables->boosted) {
			if (ppol->target_freq < tunables->hispeed_freq)
				new_freq = tunables->hispeed_freq;
			else
				new_freq = max(new_freq,
					       tunables->hispeed_freq);
		}
	}

	if (now - ppol->max_freq_hyst_start_time <
	    tunables->max_freq_hysteresis)
		new_freq = max(tunables->hispeed_freq, new_freq);

	if (!skip_hispeed_logic &&
	    ppol->target_freq >= tunables->hispeed_freq &&
	    new_freq > ppol->target_freq &&
	    now - ppol->hispeed_validate_time <
	    freq_to_above_hispeed_delay(tunables, ppol->target_freq)) {
		trace_cpufreq_interactive_notyet(
			max_cpu, pol_load, ppol->target_freq,
			ppol->policy->cur, new_freq);
		spin_unlock_irqrestore(&ppol->target_freq_lock, flags);
		goto rearm;
	}

	ppol->hispeed_validate_time = now;

	index = cpufreq_frequency_table_target(&ppol->p_nolim, new_freq,
					   CPUFREQ_RELATION_L);
	new_freq = ppol->freq_table[index].frequency;

	/*
	 * Do not scale below floor_freq unless we have been at or above the
	 * floor frequency for the minimum sample time since last validated.
	 */
	if (!skip_min_sample_time && new_freq < ppol->floor_freq) {
		if (now - ppol->floor_validate_time <
				tunables->min_sample_time) {
			trace_cpufreq_interactive_notyet(
				max_cpu, pol_load, ppol->target_freq,
				ppol->policy->cur, new_freq);
			spin_unlock_irqrestore(&ppol->target_freq_lock, flags);
			goto rearm;
		}
	}

	/*
	 * Update the timestamp for checking whether speed has been held at
	 * or above the selected frequency for a minimum of min_sample_time,
	 * if not boosted to hispeed_freq.  If boosted to hispeed_freq then we
	 * allow the speed to drop as soon as the boostpulse duration expires
	 * (or the indefinite boost is turned off). If policy->max is restored
	 * for max_freq_hysteresis, don't extend the timestamp. Otherwise, it
	 * could incorrectly extended the duration of max_freq_hysteresis by
	 * min_sample_time.
	 */

	if ((!tunables->boosted || new_freq > tunables->hispeed_freq)
	    && !jump_to_max_no_ts) {
		ppol->floor_freq = new_freq;
		ppol->floor_validate_time = now;
	}

	if (new_freq >= ppol->policy->max && !jump_to_max_no_ts)
		ppol->max_freq_hyst_start_time = now;

	if (ppol->target_freq == new_freq &&
			ppol->target_freq <= ppol->policy->cur) {
		trace_cpufreq_interactive_already(
			max_cpu, pol_load, ppol->target_freq,
			ppol->policy->cur, new_freq);
		spin_unlock_irqrestore(&ppol->target_freq_lock, flags);
		goto rearm;
	}

	trace_cpufreq_interactive_target(max_cpu, pol_load, ppol->target_freq,
					 ppol->policy->cur, new_freq);

	ppol->target_freq = new_freq;
	spin_unlock_irqrestore(&ppol->target_freq_lock, flags);
	spin_lock_irqsave(&speedchange_cpumask_lock, flags);
	cpumask_set_cpu(max_cpu, &speedchange_cpumask);
	spin_unlock_irqrestore(&speedchange_cpumask_lock, flags);

	wake_up_process(speedchange_task);

rearm:
	cpufreq_interactive_timer_resched(data, false);

	/*
	 * Send govinfo notification.
	 * Govinfo notification could potentially wake up another thread
	 * managed by its clients. Thread wakeups might trigger a load
	 * change callback that executes this function again. Therefore
	 * no spinlock could be held when sending the notification.
	 */
	for_each_cpu(i, ppol->policy->cpus) {
		pcpu = &per_cpu(cpuinfo, i);
		govinfo.cpu = i;
		govinfo.load = pcpu->loadadjfreq / ppol->policy->max;
		govinfo.sampling_rate_us = tunables->timer_rate;
		atomic_notifier_call_chain(&cpufreq_govinfo_notifier_list,
					   CPUFREQ_LOAD_CHANGE, &govinfo);
	}

exit:
	up_read(&ppol->enable_sem);
	return;
}

static int cpufreq_interactive_speedchange_task(void *data)
{
	unsigned int cpu;
	cpumask_t tmp_mask;
	unsigned long flags;
	struct cpufreq_interactive_policyinfo *ppol;

	while (1) {
		set_current_state(TASK_INTERRUPTIBLE);
		spin_lock_irqsave(&speedchange_cpumask_lock, flags);

		if (cpumask_empty(&speedchange_cpumask)) {
			spin_unlock_irqrestore(&speedchange_cpumask_lock,
					       flags);
			schedule();

			if (kthread_should_stop())
				break;

			spin_lock_irqsave(&speedchange_cpumask_lock, flags);
		}

		set_current_state(TASK_RUNNING);
		tmp_mask = speedchange_cpumask;
		cpumask_clear(&speedchange_cpumask);
		spin_unlock_irqrestore(&speedchange_cpumask_lock, flags);

		for_each_cpu(cpu, &tmp_mask) {
			ppol = per_cpu(polinfo, cpu);
			if (!down_read_trylock(&ppol->enable_sem))
				continue;
			if (!ppol->governor_enabled) {
				up_read(&ppol->enable_sem);
				continue;
			}

			if (ppol->target_freq != ppol->policy->cur)
				__cpufreq_driver_target(ppol->policy,
							ppol->target_freq,
							CPUFREQ_RELATION_H);
			trace_cpufreq_interactive_setspeed(cpu,
						     ppol->target_freq,
						     ppol->policy->cur);
			up_read(&ppol->enable_sem);
		}
	}

	return 0;
}

static void cpufreq_interactive_boost(struct cpufreq_interactive_tunables *tunables)
{
	int i;
	int anyboost = 0;
	unsigned long flags[2];
	struct cpufreq_interactive_policyinfo *ppol;

	tunables->boosted = true;

	spin_lock_irqsave(&speedchange_cpumask_lock, flags[0]);

	for_each_online_cpu(i) {
		ppol = per_cpu(polinfo, i);
		if (!ppol || tunables != ppol->policy->governor_data)
			continue;

		spin_lock_irqsave(&ppol->target_freq_lock, flags[1]);
		if (ppol->target_freq < tunables->hispeed_freq) {
			ppol->target_freq = tunables->hispeed_freq;
			cpumask_set_cpu(i, &speedchange_cpumask);
			ppol->hispeed_validate_time =
				ktime_to_us(ktime_get());
			anyboost = 1;
		}

		/*
		 * Set floor freq and (re)start timer for when last
		 * validated.
		 */

		ppol->floor_freq = tunables->hispeed_freq;
		ppol->floor_validate_time = ktime_to_us(ktime_get());
		spin_unlock_irqrestore(&ppol->target_freq_lock, flags[1]);
		break;
	}

	spin_unlock_irqrestore(&speedchange_cpumask_lock, flags[0]);

	if (anyboost)
		wake_up_process(speedchange_task);
}

static int load_change_callback(struct notifier_block *nb, unsigned long val,
				void *data)
{
	unsigned long cpu = (unsigned long) data;
	struct cpufreq_interactive_policyinfo *ppol = per_cpu(polinfo, cpu);
	struct cpufreq_interactive_tunables *tunables;
	unsigned long flags;

	if (!ppol || ppol->reject_notification)
		return 0;

	if (!down_read_trylock(&ppol->enable_sem))
		return 0;
	if (!ppol->governor_enabled)
		goto exit;

	tunables = ppol->policy->governor_data;
	if (!tunables->use_sched_load || !tunables->use_migration_notif)
		goto exit;

	spin_lock_irqsave(&ppol->target_freq_lock, flags);
	ppol->notif_pending = true;
	ppol->notif_cpu = cpu;
	spin_unlock_irqrestore(&ppol->target_freq_lock, flags);

	if (!hrtimer_is_queued(&ppol->notif_timer))
		hrtimer_start(&ppol->notif_timer, ms_to_ktime(1),
			      HRTIMER_MODE_REL);
exit:
	up_read(&ppol->enable_sem);
	return 0;
}

static enum hrtimer_restart cpufreq_interactive_hrtimer(struct hrtimer *timer)
{
	struct cpufreq_interactive_policyinfo *ppol = container_of(timer,
			struct cpufreq_interactive_policyinfo, notif_timer);
	int cpu;

	if (!down_read_trylock(&ppol->enable_sem))
		return 0;
	if (!ppol->governor_enabled) {
		up_read(&ppol->enable_sem);
		return 0;
	}
	cpu = ppol->notif_cpu;
	trace_cpufreq_interactive_load_change(cpu);
	del_timer(&ppol->policy_slack_timer);
	cpufreq_interactive_timer(cpu);

	up_read(&ppol->enable_sem);
	return HRTIMER_NORESTART;
}

static struct notifier_block load_notifier_block = {
	.notifier_call = load_change_callback,
};

static int cpufreq_interactive_notifier(
	struct notifier_block *nb, unsigned long val, void *data)
{
	struct cpufreq_freqs *freq = data;
	struct cpufreq_interactive_policyinfo *ppol;
	int cpu;
	unsigned long flags;

	if (val == CPUFREQ_POSTCHANGE) {
		ppol = per_cpu(polinfo, freq->cpu);
		if (!ppol)
			return 0;
		if (!down_read_trylock(&ppol->enable_sem))
			return 0;
		if (!ppol->governor_enabled) {
			up_read(&ppol->enable_sem);
			return 0;
		}

		if (cpumask_first(ppol->policy->cpus) != freq->cpu) {
			up_read(&ppol->enable_sem);
			return 0;
		}
		spin_lock_irqsave(&ppol->load_lock, flags);
		for_each_cpu(cpu, ppol->policy->cpus)
			update_load(cpu);
		spin_unlock_irqrestore(&ppol->load_lock, flags);

		up_read(&ppol->enable_sem);
	}
	return 0;
}

static struct notifier_block cpufreq_notifier_block = {
	.notifier_call = cpufreq_interactive_notifier,
};

static unsigned int *get_tokenized_data(const char *buf, int *num_tokens)
{
	const char *cp;
	int i;
	int ntokens = 1;
	unsigned int *tokenized_data;
	int err = -EINVAL;

	cp = buf;
	while ((cp = strpbrk(cp + 1, " :")))
		ntokens++;

	if (!(ntokens & 0x1))
		goto err;

	tokenized_data = kmalloc(ntokens * sizeof(unsigned int), GFP_KERNEL);
	if (!tokenized_data) {
		err = -ENOMEM;
		goto err;
	}

	cp = buf;
	i = 0;
	while (i < ntokens) {
		if (sscanf(cp, "%u", &tokenized_data[i++]) != 1)
			goto err_kfree;

		cp = strpbrk(cp, " :");
		if (!cp)
			break;
		cp++;
	}

	if (i != ntokens)
		goto err_kfree;

	*num_tokens = ntokens;
	return tokenized_data;

err_kfree:
	kfree(tokenized_data);
err:
	return ERR_PTR(err);
}

static ssize_t show_target_loads(
	struct cpufreq_interactive_tunables *tunables,
	char *buf)
{
	int i;
	ssize_t ret = 0;
	unsigned long flags;

	spin_lock_irqsave(&tunables->target_loads_lock, flags);

	for (i = 0; i < tunables->ntarget_loads; i++)
		ret += sprintf(buf + ret, "%u%s", tunables->target_loads[i],
			       i & 0x1 ? ":" : " ");

	sprintf(buf + ret - 1, "\n");
	spin_unlock_irqrestore(&tunables->target_loads_lock, flags);
	return ret;
}

static ssize_t store_target_loads(
	struct cpufreq_interactive_tunables *tunables,
	const char *buf, size_t count)
{
	int ntokens;
	unsigned int *new_target_loads = NULL;
	unsigned long flags;

	new_target_loads = get_tokenized_data(buf, &ntokens);
	if (IS_ERR(new_target_loads))
		return PTR_RET(new_target_loads);

	spin_lock_irqsave(&tunables->target_loads_lock, flags);
	if (tunables->target_loads != default_target_loads)
		kfree(tunables->target_loads);
	tunables->target_loads = new_target_loads;
	tunables->ntarget_loads = ntokens;
	spin_unlock_irqrestore(&tunables->target_loads_lock, flags);

	sched_update_freq_max_load(&controlled_cpus);

	return count;
}

static ssize_t show_above_hispeed_delay(
	struct cpufreq_interactive_tunables *tunables, char *buf)
{
	int i;
	ssize_t ret = 0;
	unsigned long flags;

	spin_lock_irqsave(&tunables->above_hispeed_delay_lock, flags);

	for (i = 0; i < tunables->nabove_hispeed_delay; i++)
		ret += sprintf(buf + ret, "%u%s",
			       tunables->above_hispeed_delay[i],
			       i & 0x1 ? ":" : " ");

	sprintf(buf + ret - 1, "\n");
	spin_unlock_irqrestore(&tunables->above_hispeed_delay_lock, flags);
	return ret;
}

static ssize_t store_above_hispeed_delay(
	struct cpufreq_interactive_tunables *tunables,
	const char *buf, size_t count)
{
	int ntokens;
	unsigned int *new_above_hispeed_delay = NULL;
	unsigned long flags;

	new_above_hispeed_delay = get_tokenized_data(buf, &ntokens);
	if (IS_ERR(new_above_hispeed_delay))
		return PTR_RET(new_above_hispeed_delay);

	spin_lock_irqsave(&tunables->above_hispeed_delay_lock, flags);
	if (tunables->above_hispeed_delay != default_above_hispeed_delay)
		kfree(tunables->above_hispeed_delay);
	tunables->above_hispeed_delay = new_above_hispeed_delay;
	tunables->nabove_hispeed_delay = ntokens;
	spin_unlock_irqrestore(&tunables->above_hispeed_delay_lock, flags);
	return count;

}

static ssize_t show_hispeed_freq(struct cpufreq_interactive_tunables *tunables,
		char *buf)
{
	return sprintf(buf, "%u\n", tunables->hispeed_freq);
}

static ssize_t store_hispeed_freq(struct cpufreq_interactive_tunables *tunables,
		const char *buf, size_t count)
{
	int ret;
	long unsigned int val;

	ret = kstrtoul(buf, 0, &val);
	if (ret < 0)
		return ret;
	tunables->hispeed_freq = val;
	return count;
}

#define show_store_one(file_name)					\
static ssize_t show_##file_name(					\
	struct cpufreq_interactive_tunables *tunables, char *buf)	\
{									\
	return snprintf(buf, PAGE_SIZE, "%u\n", tunables->file_name);	\
}									\
static ssize_t store_##file_name(					\
		struct cpufreq_interactive_tunables *tunables,		\
		const char *buf, size_t count)				\
{									\
	int ret;							\
	unsigned long int val;						\
									\
	ret = kstrtoul(buf, 0, &val);				\
	if (ret < 0)							\
		return ret;						\
	tunables->file_name = val;					\
	return count;							\
}
show_store_one(max_freq_hysteresis);
show_store_one(align_windows);
show_store_one(ignore_hispeed_on_notif);
show_store_one(fast_ramp_down);
show_store_one(enable_prediction);

static ssize_t show_go_hispeed_load(struct cpufreq_interactive_tunables
		*tunables, char *buf)
{
	return sprintf(buf, "%lu\n", tunables->go_hispeed_load);
}

static ssize_t store_go_hispeed_load(struct cpufreq_interactive_tunables
		*tunables, const char *buf, size_t count)
{
	int ret;
	unsigned long val;

	ret = kstrtoul(buf, 0, &val);
	if (ret < 0)
		return ret;
	tunables->go_hispeed_load = val;
	return count;
}

static ssize_t show_min_sample_time(struct cpufreq_interactive_tunables
		*tunables, char *buf)
{
	return sprintf(buf, "%lu\n", tunables->min_sample_time);
}

static ssize_t store_min_sample_time(struct cpufreq_interactive_tunables
		*tunables, const char *buf, size_t count)
{
	int ret;
	unsigned long val;

	ret = kstrtoul(buf, 0, &val);
	if (ret < 0)
		return ret;
	tunables->min_sample_time = val;
	return count;
}

static ssize_t show_timer_rate(struct cpufreq_interactive_tunables *tunables,
		char *buf)
{
	return sprintf(buf, "%lu\n", tunables->timer_rate);
}

static ssize_t store_timer_rate(struct cpufreq_interactive_tunables *tunables,
		const char *buf, size_t count)
{
	int ret;
	unsigned long val, val_round;
	struct cpufreq_interactive_tunables *t;
	int cpu;

	ret = kstrtoul(buf, 0, &val);
	if (ret < 0)
		return ret;

	val_round = jiffies_to_usecs(usecs_to_jiffies(val));
	if (val != val_round)
		pr_warn("timer_rate not aligned to jiffy. Rounded up to %lu\n",
			val_round);
	tunables->timer_rate = val_round;

	if (!tunables->use_sched_load)
		return count;

	for_each_possible_cpu(cpu) {
		if (!per_cpu(polinfo, cpu))
			continue;
		t = per_cpu(polinfo, cpu)->cached_tunables;
		if (t && t->use_sched_load)
			t->timer_rate = val_round;
	}
	set_window_helper(tunables);

	return count;
}

static ssize_t show_timer_slack(struct cpufreq_interactive_tunables *tunables,
		char *buf)
{
	return sprintf(buf, "%d\n", tunables->timer_slack_val);
}

static ssize_t store_timer_slack(struct cpufreq_interactive_tunables *tunables,
		const char *buf, size_t count)
{
	int ret;
	unsigned long val;

	ret = kstrtol(buf, 10, &val);
	if (ret < 0)
		return ret;

	tunables->timer_slack_val = val;
	return count;
}

static ssize_t show_boost(struct cpufreq_interactive_tunables *tunables,
			  char *buf)
{
	return sprintf(buf, "%d\n", tunables->boost_val);
}

static ssize_t store_boost(struct cpufreq_interactive_tunables *tunables,
			   const char *buf, size_t count)
{
	int ret;
	unsigned long val;

	ret = kstrtoul(buf, 0, &val);
	if (ret < 0)
		return ret;

	tunables->boost_val = val;

	if (tunables->boost_val) {
		trace_cpufreq_interactive_boost("on");
		if (!tunables->boosted)
			cpufreq_interactive_boost(tunables);
	} else {
		tunables->boostpulse_endtime = ktime_to_us(ktime_get());
		trace_cpufreq_interactive_unboost("off");
	}

	return count;
}

static ssize_t store_boostpulse(struct cpufreq_interactive_tunables *tunables,
				const char *buf, size_t count)
{
	int ret;
	unsigned long val;

	ret = kstrtoul(buf, 0, &val);
	if (ret < 0)
		return ret;

	tunables->boostpulse_endtime = ktime_to_us(ktime_get()) +
		tunables->boostpulse_duration_val;
	trace_cpufreq_interactive_boost("pulse");
	if (!tunables->boosted)
		cpufreq_interactive_boost(tunables);
	return count;
}

static ssize_t show_boostpulse_duration(struct cpufreq_interactive_tunables
		*tunables, char *buf)
{
	return sprintf(buf, "%d\n", tunables->boostpulse_duration_val);
}

static ssize_t store_boostpulse_duration(struct cpufreq_interactive_tunables
		*tunables, const char *buf, size_t count)
{
	int ret;
	unsigned long val;

	ret = kstrtoul(buf, 0, &val);
	if (ret < 0)
		return ret;

	tunables->boostpulse_duration_val = val;
	return count;
}

static ssize_t show_io_is_busy(struct cpufreq_interactive_tunables *tunables,
		char *buf)
{
	return sprintf(buf, "%u\n", tunables->io_is_busy);
}

static ssize_t store_io_is_busy(struct cpufreq_interactive_tunables *tunables,
		const char *buf, size_t count)
{
	int ret;
	unsigned long val;
	struct cpufreq_interactive_tunables *t;
	int cpu;

	ret = kstrtoul(buf, 0, &val);
	if (ret < 0)
		return ret;
	tunables->io_is_busy = val;

	if (!tunables->use_sched_load)
		return count;

	for_each_possible_cpu(cpu) {
		if (!per_cpu(polinfo, cpu))
			continue;
		t = per_cpu(polinfo, cpu)->cached_tunables;
		if (t && t->use_sched_load)
			t->io_is_busy = val;
	}
	sched_set_io_is_busy(val);

	return count;
}

static int cpufreq_interactive_enable_sched_input(
			struct cpufreq_interactive_tunables *tunables)
{
	int rc = 0, j;
	struct cpufreq_interactive_tunables *t;

	mutex_lock(&sched_lock);

	set_window_count++;
	if (set_window_count > 1) {
		for_each_possible_cpu(j) {
			if (!per_cpu(polinfo, j))
				continue;
			t = per_cpu(polinfo, j)->cached_tunables;
			if (t && t->use_sched_load) {
				tunables->timer_rate = t->timer_rate;
				tunables->io_is_busy = t->io_is_busy;
				break;
			}
		}
	} else {
		rc = set_window_helper(tunables);
		if (rc) {
			pr_err("%s: Failed to set sched window\n", __func__);
			set_window_count--;
			goto out;
		}
		sched_set_io_is_busy(tunables->io_is_busy);
	}

	if (!tunables->use_migration_notif)
		goto out;

	migration_register_count++;
	if (migration_register_count > 1)
		goto out;
	else
		atomic_notifier_chain_register(&load_alert_notifier_head,
						&load_notifier_block);
out:
	mutex_unlock(&sched_lock);
	return rc;
}

static int cpufreq_interactive_disable_sched_input(
			struct cpufreq_interactive_tunables *tunables)
{
	mutex_lock(&sched_lock);

	if (tunables->use_migration_notif) {
		migration_register_count--;
		if (migration_register_count < 1)
			atomic_notifier_chain_unregister(
					&load_alert_notifier_head,
					&load_notifier_block);
	}
	set_window_count--;

	mutex_unlock(&sched_lock);
	return 0;
}

static ssize_t show_use_sched_load(
		struct cpufreq_interactive_tunables *tunables, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n", tunables->use_sched_load);
}

static ssize_t store_use_sched_load(
			struct cpufreq_interactive_tunables *tunables,
			const char *buf, size_t count)
{
	int ret;
	unsigned long val;

	ret = kstrtoul(buf, 0, &val);
	if (ret < 0)
		return ret;

	if (tunables->use_sched_load == (bool) val)
		return count;

	tunables->use_sched_load = val;

	if (val)
		ret = cpufreq_interactive_enable_sched_input(tunables);
	else
		ret = cpufreq_interactive_disable_sched_input(tunables);

	if (ret) {
		tunables->use_sched_load = !val;
		return ret;
	}

	return count;
}

static ssize_t show_use_migration_notif(
		struct cpufreq_interactive_tunables *tunables, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n",
			tunables->use_migration_notif);
}

static ssize_t store_use_migration_notif(
			struct cpufreq_interactive_tunables *tunables,
			const char *buf, size_t count)
{
	int ret;
	unsigned long val;

	ret = kstrtoul(buf, 0, &val);
	if (ret < 0)
		return ret;

	if (tunables->use_migration_notif == (bool) val)
		return count;
	tunables->use_migration_notif = val;

	if (!tunables->use_sched_load)
		return count;

	mutex_lock(&sched_lock);
	if (val) {
		migration_register_count++;
		if (migration_register_count == 1)
			atomic_notifier_chain_register(
					&load_alert_notifier_head,
					&load_notifier_block);
	} else {
		migration_register_count--;
		if (!migration_register_count)
			atomic_notifier_chain_unregister(
					&load_alert_notifier_head,
					&load_notifier_block);
	}
	mutex_unlock(&sched_lock);

	return count;
}

/*
 * Create show/store routines
 * - sys: One governor instance for complete SYSTEM
 * - pol: One governor instance per struct cpufreq_policy
 */
#define show_gov_pol_sys(file_name)					\
static ssize_t show_##file_name##_gov_sys				\
(struct kobject *kobj, struct kobj_attribute *attr, char *buf)		\
{									\
	return show_##file_name(common_tunables, buf);			\
}									\
									\
static ssize_t show_##file_name##_gov_pol				\
(struct cpufreq_policy *policy, char *buf)				\
{									\
	return show_##file_name(policy->governor_data, buf);		\
}

#define store_gov_pol_sys(file_name)					\
static ssize_t store_##file_name##_gov_sys				\
(struct kobject *kobj, struct kobj_attribute *attr, const char *buf,		\
	size_t count)							\
{									\
	return store_##file_name(common_tunables, buf, count);		\
}									\
									\
static ssize_t store_##file_name##_gov_pol				\
(struct cpufreq_policy *policy, const char *buf, size_t count)		\
{									\
	return store_##file_name(policy->governor_data, buf, count);	\
}

#define show_store_gov_pol_sys(file_name)				\
show_gov_pol_sys(file_name);						\
store_gov_pol_sys(file_name)

show_store_gov_pol_sys(target_loads);
show_store_gov_pol_sys(above_hispeed_delay);
show_store_gov_pol_sys(hispeed_freq);
show_store_gov_pol_sys(go_hispeed_load);
show_store_gov_pol_sys(min_sample_time);
show_store_gov_pol_sys(timer_rate);
show_store_gov_pol_sys(timer_slack);
show_store_gov_pol_sys(boost);
store_gov_pol_sys(boostpulse);
show_store_gov_pol_sys(boostpulse_duration);
show_store_gov_pol_sys(io_is_busy);
show_store_gov_pol_sys(use_sched_load);
show_store_gov_pol_sys(use_migration_notif);
show_store_gov_pol_sys(max_freq_hysteresis);
show_store_gov_pol_sys(align_windows);
show_store_gov_pol_sys(ignore_hispeed_on_notif);
show_store_gov_pol_sys(fast_ramp_down);
show_store_gov_pol_sys(enable_prediction);

#define gov_sys_attr_rw(_name)						\
static struct kobj_attribute _name##_gov_sys =				\
__ATTR(_name, 0644, show_##_name##_gov_sys, store_##_name##_gov_sys)

#define gov_pol_attr_rw(_name)						\
static struct freq_attr _name##_gov_pol =				\
__ATTR(_name, 0644, show_##_name##_gov_pol, store_##_name##_gov_pol)

#define gov_sys_pol_attr_rw(_name)					\
	gov_sys_attr_rw(_name);						\
	gov_pol_attr_rw(_name)

gov_sys_pol_attr_rw(target_loads);
gov_sys_pol_attr_rw(above_hispeed_delay);
gov_sys_pol_attr_rw(hispeed_freq);
gov_sys_pol_attr_rw(go_hispeed_load);
gov_sys_pol_attr_rw(min_sample_time);
gov_sys_pol_attr_rw(timer_rate);
gov_sys_pol_attr_rw(timer_slack);
gov_sys_pol_attr_rw(boost);
gov_sys_pol_attr_rw(boostpulse_duration);
gov_sys_pol_attr_rw(io_is_busy);
gov_sys_pol_attr_rw(use_sched_load);
gov_sys_pol_attr_rw(use_migration_notif);
gov_sys_pol_attr_rw(max_freq_hysteresis);
gov_sys_pol_attr_rw(align_windows);
gov_sys_pol_attr_rw(ignore_hispeed_on_notif);
gov_sys_pol_attr_rw(fast_ramp_down);
gov_sys_pol_attr_rw(enable_prediction);

static struct kobj_attribute boostpulse_gov_sys =
	__ATTR(boostpulse, 0200, NULL, store_boostpulse_gov_sys);

static struct freq_attr boostpulse_gov_pol =
	__ATTR(boostpulse, 0200, NULL, store_boostpulse_gov_pol);

/* One Governor instance for entire system */
static struct attribute *interactive_attributes_gov_sys[] = {
	&target_loads_gov_sys.attr,
	&above_hispeed_delay_gov_sys.attr,
	&hispeed_freq_gov_sys.attr,
	&go_hispeed_load_gov_sys.attr,
	&min_sample_time_gov_sys.attr,
	&timer_rate_gov_sys.attr,
	&timer_slack_gov_sys.attr,
	&boost_gov_sys.attr,
	&boostpulse_gov_sys.attr,
	&boostpulse_duration_gov_sys.attr,
	&io_is_busy_gov_sys.attr,
	&use_sched_load_gov_sys.attr,
	&use_migration_notif_gov_sys.attr,
	&max_freq_hysteresis_gov_sys.attr,
	&align_windows_gov_sys.attr,
	&ignore_hispeed_on_notif_gov_sys.attr,
	&fast_ramp_down_gov_sys.attr,
	&enable_prediction_gov_sys.attr,
	NULL,
};

static struct attribute_group interactive_attr_group_gov_sys = {
	.attrs = interactive_attributes_gov_sys,
	.name = "interactive",
};

/* Per policy governor instance */
static struct attribute *interactive_attributes_gov_pol[] = {
	&target_loads_gov_pol.attr,
	&above_hispeed_delay_gov_pol.attr,
	&hispeed_freq_gov_pol.attr,
	&go_hispeed_load_gov_pol.attr,
	&min_sample_time_gov_pol.attr,
	&timer_rate_gov_pol.attr,
	&timer_slack_gov_pol.attr,
	&boost_gov_pol.attr,
	&boostpulse_gov_pol.attr,
	&boostpulse_duration_gov_pol.attr,
	&io_is_busy_gov_pol.attr,
	&use_sched_load_gov_pol.attr,
	&use_migration_notif_gov_pol.attr,
	&max_freq_hysteresis_gov_pol.attr,
	&align_windows_gov_pol.attr,
	&ignore_hispeed_on_notif_gov_pol.attr,
	&fast_ramp_down_gov_pol.attr,
	&enable_prediction_gov_pol.attr,
	NULL,
};

static struct attribute_group interactive_attr_group_gov_pol = {
	.attrs = interactive_attributes_gov_pol,
	.name = "interactive",
};

static struct attribute_group *get_sysfs_attr(void)
{
	if (have_governor_per_policy())
		return &interactive_attr_group_gov_pol;
	else
		return &interactive_attr_group_gov_sys;
}

static void cpufreq_interactive_nop_timer(unsigned long data)
{
}

static struct cpufreq_interactive_tunables *alloc_tunable(
					struct cpufreq_policy *policy)
{
	struct cpufreq_interactive_tunables *tunables;

	tunables = kzalloc(sizeof(*tunables), GFP_KERNEL);
	if (!tunables)
		return ERR_PTR(-ENOMEM);

	tunables->above_hispeed_delay = default_above_hispeed_delay;
	tunables->nabove_hispeed_delay =
		ARRAY_SIZE(default_above_hispeed_delay);
	tunables->go_hispeed_load = DEFAULT_GO_HISPEED_LOAD;
	tunables->target_loads = default_target_loads;
	tunables->ntarget_loads = ARRAY_SIZE(default_target_loads);
	tunables->min_sample_time = DEFAULT_MIN_SAMPLE_TIME;
	tunables->timer_rate = DEFAULT_TIMER_RATE;
	tunables->boostpulse_duration_val = DEFAULT_MIN_SAMPLE_TIME;
	tunables->timer_slack_val = DEFAULT_TIMER_SLACK;

	spin_lock_init(&tunables->target_loads_lock);
	spin_lock_init(&tunables->above_hispeed_delay_lock);

	return tunables;
}

static void irq_work(struct irq_work *irq_work)
{
	struct cpufreq_interactive_policyinfo *ppol;
	unsigned long flags;

	ppol = container_of(irq_work, struct cpufreq_interactive_policyinfo,
			    irq_work);

	cpufreq_interactive_timer(smp_processor_id());
	spin_lock_irqsave(&ppol->irq_work_lock, flags);
	ppol->work_in_progress = false;
	spin_unlock_irqrestore(&ppol->irq_work_lock, flags);
}

static struct cpufreq_interactive_policyinfo *get_policyinfo(
					struct cpufreq_policy *policy)
{
	struct cpufreq_interactive_policyinfo *ppol =
				per_cpu(polinfo, policy->cpu);
	int i;
	struct sched_load *sl;

	/* polinfo already allocated for policy, return */
	if (ppol)
		return ppol;

	ppol = kzalloc(sizeof(*ppol), GFP_KERNEL);
	if (!ppol)
		return ERR_PTR(-ENOMEM);

	sl = kcalloc(cpumask_weight(policy->related_cpus), sizeof(*sl),
		     GFP_KERNEL);
	if (!sl) {
		kfree(ppol);
		return ERR_PTR(-ENOMEM);
	}
	ppol->sl = sl;

	init_timer(&ppol->policy_slack_timer);
	ppol->policy_slack_timer.function = cpufreq_interactive_nop_timer;
	hrtimer_init(&ppol->notif_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	ppol->notif_timer.function = cpufreq_interactive_hrtimer;
	init_irq_work(&ppol->irq_work, irq_work);
	spin_lock_init(&ppol->irq_work_lock);
	spin_lock_init(&ppol->load_lock);
	spin_lock_init(&ppol->target_freq_lock);
	init_rwsem(&ppol->enable_sem);

	for_each_cpu(i, policy->related_cpus)
		per_cpu(polinfo, i) = ppol;
	return ppol;
}

/* This function is not multithread-safe. */
static void free_policyinfo(int cpu)
{
	struct cpufreq_interactive_policyinfo *ppol = per_cpu(polinfo, cpu);
	int j;

	if (!ppol)
		return;

	for_each_possible_cpu(j)
		if (per_cpu(polinfo, j) == ppol)
			per_cpu(polinfo, cpu) = NULL;
	kfree(ppol->cached_tunables);
	kfree(ppol->sl);
	kfree(ppol);
}

static struct cpufreq_interactive_tunables *get_tunables(
				struct cpufreq_interactive_policyinfo *ppol)
{
	if (have_governor_per_policy())
		return ppol->cached_tunables;
	else
		return cached_common_tunables;
}

/* Interactive Governor callbacks */
struct interactive_governor {
	struct cpufreq_governor gov;
	unsigned int usage_count;
};

static struct interactive_governor interactive_gov;

#define CPU_FREQ_GOV_INTERACTIVE	(&interactive_gov.gov)

int cpufreq_interactive_init(struct cpufreq_policy *policy)
{
	int rc;
	struct cpufreq_interactive_policyinfo *ppol;
	struct cpufreq_interactive_tunables *tunables;

	if (have_governor_per_policy())
		tunables = policy->governor_data;
	else
		tunables = common_tunables;

	ppol = get_policyinfo(policy);
	if (IS_ERR(ppol))
		return PTR_ERR(ppol);

	if (have_governor_per_policy()) {
		WARN_ON(tunables);
	} else if (tunables) {
		tunables->usage_count++;
		cpumask_or(&controlled_cpus, &controlled_cpus,
			   policy->related_cpus);
		sched_update_freq_max_load(policy->related_cpus);
		policy->governor_data = tunables;
		return 0;
	}

	tunables = get_tunables(ppol);
	if (!tunables) {
		tunables = alloc_tunable(policy);
		if (IS_ERR(tunables))
			return PTR_ERR(tunables);
	}

	tunables->usage_count = 1;
	policy->governor_data = tunables;
	if (!have_governor_per_policy())
		common_tunables = tunables;

	rc = sysfs_create_group(get_governor_parent_kobj(policy),
			get_sysfs_attr());
	if (rc) {
		kfree(tunables);
		policy->governor_data = NULL;
		if (!have_governor_per_policy())
			common_tunables = NULL;
		return rc;
	}

	if (!interactive_gov.usage_count++)
		cpufreq_register_notifier(&cpufreq_notifier_block,
				CPUFREQ_TRANSITION_NOTIFIER);

	if (tunables->use_sched_load)
		cpufreq_interactive_enable_sched_input(tunables);

	cpumask_or(&controlled_cpus, &controlled_cpus,
		   policy->related_cpus);
	sched_update_freq_max_load(policy->related_cpus);

	if (have_governor_per_policy())
		ppol->cached_tunables = tunables;
	else
		cached_common_tunables = tunables;

	return 0;
}

void cpufreq_interactive_exit(struct cpufreq_policy *policy)
{
	struct cpufreq_interactive_tunables *tunables;

	if (have_governor_per_policy())
		tunables = policy->governor_data;
	else
		tunables = common_tunables;

	BUG_ON(!tunables);

	cpumask_andnot(&controlled_cpus, &controlled_cpus,
		       policy->related_cpus);
	sched_update_freq_max_load(cpu_possible_mask);
	if (!--tunables->usage_count) {
		/* Last policy using the governor ? */
		if (!--interactive_gov.usage_count)
			cpufreq_unregister_notifier(&cpufreq_notifier_block,
					CPUFREQ_TRANSITION_NOTIFIER);

		sysfs_remove_group(get_governor_parent_kobj(policy),
				get_sysfs_attr());

		common_tunables = NULL;
	}

	policy->governor_data = NULL;

	if (tunables->use_sched_load)
		cpufreq_interactive_disable_sched_input(tunables);
}

int cpufreq_interactive_start(struct cpufreq_policy *policy)
{
	struct cpufreq_interactive_policyinfo *ppol;
	struct cpufreq_frequency_table *freq_table;
	struct cpufreq_interactive_tunables *tunables;

	if (have_governor_per_policy())
		tunables = policy->governor_data;
	else
		tunables = common_tunables;

	BUG_ON(!tunables);
	mutex_lock(&gov_lock);

	freq_table = policy->freq_table;
	if (!tunables->hispeed_freq)
		tunables->hispeed_freq = policy->max;

	ppol = per_cpu(polinfo, policy->cpu);
	ppol->policy = policy;
	ppol->target_freq = policy->cur;
	ppol->freq_table = freq_table;
	ppol->p_nolim = *policy;
	ppol->p_nolim.min = policy->cpuinfo.min_freq;
	ppol->p_nolim.max = policy->cpuinfo.max_freq;
	ppol->floor_freq = ppol->target_freq;
	ppol->floor_validate_time = ktime_to_us(ktime_get());
	ppol->hispeed_validate_time = ppol->floor_validate_time;
	ppol->min_freq = policy->min;
	ppol->reject_notification = true;
	ppol->notif_pending = false;
	down_write(&ppol->enable_sem);
	del_timer_sync(&ppol->policy_slack_timer);
	ppol->last_evaluated_jiffy = get_jiffies_64();
	cpufreq_interactive_timer_start(tunables, policy->cpu);
	ppol->governor_enabled = 1;
	up_write(&ppol->enable_sem);
	ppol->reject_notification = false;

	mutex_unlock(&gov_lock);
	return 0;
}

void cpufreq_interactive_stop(struct cpufreq_policy *policy)
{
	struct cpufreq_interactive_policyinfo *ppol;
	struct cpufreq_interactive_tunables *tunables;

	if (have_governor_per_policy())
		tunables = policy->governor_data;
	else
		tunables = common_tunables;

	BUG_ON(!tunables);

	mutex_lock(&gov_lock);

	ppol = per_cpu(polinfo, policy->cpu);
	ppol->reject_notification = true;
	down_write(&ppol->enable_sem);
	ppol->governor_enabled = 0;
	ppol->target_freq = 0;
	gov_clear_update_util(ppol->policy);
	irq_work_sync(&ppol->irq_work);
	ppol->work_in_progress = false;
	del_timer_sync(&ppol->policy_slack_timer);
	up_write(&ppol->enable_sem);
	ppol->reject_notification = false;

	mutex_unlock(&gov_lock);
}

void cpufreq_interactive_limits(struct cpufreq_policy *policy)
{
	struct cpufreq_interactive_policyinfo *ppol;
	struct cpufreq_interactive_tunables *tunables;

	if (have_governor_per_policy())
		tunables = policy->governor_data;
	else
		tunables = common_tunables;

	BUG_ON(!tunables);
	ppol = per_cpu(polinfo, policy->cpu);

	__cpufreq_driver_target(policy,
			ppol->target_freq, CPUFREQ_RELATION_L);

	down_read(&ppol->enable_sem);
	if (ppol->governor_enabled) {
		if (policy->min < ppol->min_freq)
			cpufreq_interactive_timer_resched(policy->cpu,
							  true);
		ppol->min_freq = policy->min;
	}
	up_read(&ppol->enable_sem);
}

static struct interactive_governor interactive_gov = {
	.gov = {
		.name			= "interactive",
		.max_transition_latency	= 10000000,
		.owner			= THIS_MODULE,
		.init			= cpufreq_interactive_init,
		.exit			= cpufreq_interactive_exit,
		.start			= cpufreq_interactive_start,
		.stop			= cpufreq_interactive_stop,
		.limits			= cpufreq_interactive_limits,
	}
};

static int __init cpufreq_interactive_gov_init(void)
{
	struct sched_param param = { .sched_priority = MAX_RT_PRIO-1 };

	spin_lock_init(&speedchange_cpumask_lock);
	mutex_init(&gov_lock);
	mutex_init(&sched_lock);
	speedchange_task =
		kthread_create(cpufreq_interactive_speedchange_task, NULL,
			       "cfinteractive");
	if (IS_ERR(speedchange_task))
		return PTR_ERR(speedchange_task);

	sched_setscheduler_nocheck(speedchange_task, SCHED_FIFO, &param);
	get_task_struct(speedchange_task);

	/* NB: wake up so the thread does not look hung to the freezer */
	wake_up_process(speedchange_task);

	return cpufreq_register_governor(CPU_FREQ_GOV_INTERACTIVE);
}

#ifdef CONFIG_CPU_FREQ_DEFAULT_GOV_INTERACTIVE
struct cpufreq_governor *cpufreq_default_governor(void)
{
	return CPU_FREQ_GOV_INTERACTIVE;
}

fs_initcall(cpufreq_interactive_gov_init);
#else
module_init(cpufreq_interactive_gov_init);
#endif

static void __exit cpufreq_interactive_gov_exit(void)
{
	int cpu;

	cpufreq_unregister_governor(CPU_FREQ_GOV_INTERACTIVE);
	kthread_stop(speedchange_task);
	put_task_struct(speedchange_task);

	for_each_possible_cpu(cpu)
		free_policyinfo(cpu);
}

module_exit(cpufreq_interactive_gov_exit);

MODULE_AUTHOR("Mike Chan <mike@android.com>");
MODULE_DESCRIPTION("'cpufreq_interactive' - A cpufreq governor for "
	"Latency sensitive workloads");
MODULE_LICENSE("GPL");
