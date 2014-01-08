/*
 *  drivers/cpufreq/cpufreq_ondemand.c
 *
 *  Copyright (C)  2001 Russell King
 *            (C)  2003 Venkatesh Pallipadi <venkatesh.pallipadi@intel.com>.
 *                      Jun Nakajima <jun.nakajima@intel.com>
 *            (c)  2013 The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/cpufreq.h>
#include <linux/cpu.h>
#include <linux/jiffies.h>
#include <linux/kernel_stat.h>
#include <linux/mutex.h>
#include <linux/hrtimer.h>
#include <linux/tick.h>
#include <linux/ktime.h>
#include <linux/kthread.h>
#include <linux/sched.h>
#include <linux/input.h>
#include <linux/workqueue.h>
#include <linux/slab.h>

/*
 * dbs is used in this file as a shortform for demandbased switching
 * It helps to keep variable names smaller, simpler
 */

#define DEF_FREQUENCY_DOWN_DIFFERENTIAL		(10)
#define DEF_FREQUENCY_UP_THRESHOLD		(80)
#define DEF_SAMPLING_DOWN_FACTOR		(1)
#define MAX_SAMPLING_DOWN_FACTOR		(100000)
#define MICRO_FREQUENCY_DOWN_DIFFERENTIAL	(3)
#define MICRO_FREQUENCY_UP_THRESHOLD		(95)
#define MICRO_FREQUENCY_MIN_SAMPLE_RATE		(10000)
#define MIN_FREQUENCY_UP_THRESHOLD		(11)
#define MAX_FREQUENCY_UP_THRESHOLD		(100)
#define MIN_FREQUENCY_DOWN_DIFFERENTIAL		(1)

/*
 * The polling frequency of this governor depends on the capability of
 * the processor. Default polling frequency is 1000 times the transition
 * latency of the processor. The governor will work on any processor with
 * transition latency <= 10mS, using appropriate sampling
 * rate.
 * For CPUs with transition latency > 10mS (mostly drivers with CPUFREQ_ETERNAL)
 * this governor will not work.
 * All times here are in uS.
 */
#define MIN_SAMPLING_RATE_RATIO			(2)

static unsigned int min_sampling_rate;

#define LATENCY_MULTIPLIER			(1000)
#define MIN_LATENCY_MULTIPLIER			(100)
#define TRANSITION_LATENCY_LIMIT		(10 * 1000 * 1000)

#define POWERSAVE_BIAS_MAXLEVEL			(1000)
#define POWERSAVE_BIAS_MINLEVEL			(-1000)

static void do_dbs_timer(struct work_struct *work);
static int cpufreq_governor_dbs(struct cpufreq_policy *policy,
				unsigned int event);

#ifndef CONFIG_CPU_FREQ_DEFAULT_GOV_ONDEMAND
static
#endif
struct cpufreq_governor cpufreq_gov_ondemand = {
       .name                   = "ondemand",
       .governor               = cpufreq_governor_dbs,
       .max_transition_latency = TRANSITION_LATENCY_LIMIT,
       .owner                  = THIS_MODULE,
};

/* Sampling types */
enum {DBS_NORMAL_SAMPLE, DBS_SUB_SAMPLE};

struct cpu_dbs_info_s {
	cputime64_t prev_cpu_idle;
	cputime64_t prev_cpu_iowait;
	cputime64_t prev_cpu_wall;
	cputime64_t prev_cpu_nice;
	struct cpufreq_policy *cur_policy;
	struct delayed_work work;
	struct cpufreq_frequency_table *freq_table;
	unsigned int freq_lo;
	unsigned int freq_lo_jiffies;
	unsigned int freq_hi_jiffies;
	unsigned int rate_mult;
	unsigned int prev_load;
	unsigned int max_load;
	int cpu;
	unsigned int sample_type:1;
	/*
	 * percpu mutex that serializes governor limit change with
	 * do_dbs_timer invocation. We do not want do_dbs_timer to run
	 * when user is changing the governor or limits.
	 */
	struct mutex timer_mutex;

	struct task_struct *sync_thread;
	wait_queue_head_t sync_wq;
	atomic_t src_sync_cpu;
	atomic_t sync_enabled;
};
static DEFINE_PER_CPU(struct cpu_dbs_info_s, od_cpu_dbs_info);

static inline void dbs_timer_init(struct cpu_dbs_info_s *dbs_info);
static inline void dbs_timer_exit(struct cpu_dbs_info_s *dbs_info);

static unsigned int dbs_enable;	/* number of CPUs using this policy */

/*
 * dbs_mutex protects dbs_enable and dbs_info during start/stop.
 */
static DEFINE_MUTEX(dbs_mutex);

static struct workqueue_struct *dbs_wq;

struct dbs_work_struct {
	struct work_struct work;
	unsigned int cpu;
};

static DEFINE_PER_CPU(struct dbs_work_struct, dbs_refresh_work);

static struct dbs_tuners {
	unsigned int sampling_rate;
	unsigned int up_threshold;
	unsigned int up_threshold_multi_core;
	unsigned int down_differential;
	unsigned int down_differential_multi_core;
	unsigned int optimal_freq;
	unsigned int up_threshold_any_cpu_load;
	unsigned int sync_freq;
	unsigned int ignore_nice;
	unsigned int sampling_down_factor;
	int          powersave_bias;
	unsigned int io_is_busy;
	unsigned int input_boost;
} dbs_tuners_ins = {
	.up_threshold_multi_core = DEF_FREQUENCY_UP_THRESHOLD,
	.up_threshold = DEF_FREQUENCY_UP_THRESHOLD,
	.sampling_down_factor = DEF_SAMPLING_DOWN_FACTOR,
	.down_differential = DEF_FREQUENCY_DOWN_DIFFERENTIAL,
	.down_differential_multi_core = MICRO_FREQUENCY_DOWN_DIFFERENTIAL,
	.up_threshold_any_cpu_load = DEF_FREQUENCY_UP_THRESHOLD,
	.ignore_nice = 0,
	.powersave_bias = 0,
	.sync_freq = 0,
	.optimal_freq = 0,
	.input_boost = 0,
};

static inline u64 get_cpu_idle_time_jiffy(unsigned int cpu, u64 *wall)
{
	u64 idle_time;
	u64 cur_wall_time;
	u64 busy_time;

	cur_wall_time = jiffies64_to_cputime64(get_jiffies_64());

	busy_time  = kcpustat_cpu(cpu).cpustat[CPUTIME_USER];
	busy_time += kcpustat_cpu(cpu).cpustat[CPUTIME_SYSTEM];
	busy_time += kcpustat_cpu(cpu).cpustat[CPUTIME_IRQ];
	busy_time += kcpustat_cpu(cpu).cpustat[CPUTIME_SOFTIRQ];
	busy_time += kcpustat_cpu(cpu).cpustat[CPUTIME_STEAL];
	busy_time += kcpustat_cpu(cpu).cpustat[CPUTIME_NICE];

	idle_time = cur_wall_time - busy_time;
	if (wall)
		*wall = jiffies_to_usecs(cur_wall_time);

	return jiffies_to_usecs(idle_time);
}

static inline cputime64_t get_cpu_idle_time(unsigned int cpu, cputime64_t *wall)
{
	u64 idle_time = get_cpu_idle_time_us(cpu, NULL);

	if (idle_time == -1ULL)
		return get_cpu_idle_time_jiffy(cpu, wall);
	else
		idle_time += get_cpu_iowait_time_us(cpu, wall);

	return idle_time;
}

static inline cputime64_t get_cpu_iowait_time(unsigned int cpu, cputime64_t *wall)
{
	u64 iowait_time = get_cpu_iowait_time_us(cpu, wall);

	if (iowait_time == -1ULL)
		return 0;

	return iowait_time;
}

/*
 * Find right freq to be set now with powersave_bias on.
 * Returns the freq_hi to be used right now and will set freq_hi_jiffies,
 * freq_lo, and freq_lo_jiffies in percpu area for averaging freqs.
 */
static unsigned int powersave_bias_target(struct cpufreq_policy *policy,
					  unsigned int freq_next,
					  unsigned int relation)
{
	unsigned int freq_req, freq_avg;
	unsigned int freq_hi, freq_lo;
	unsigned int index = 0;
	unsigned int jiffies_total, jiffies_hi, jiffies_lo;
	int freq_reduc;
	struct cpu_dbs_info_s *dbs_info = &per_cpu(od_cpu_dbs_info,
						   policy->cpu);

	if (!dbs_info->freq_table) {
		dbs_info->freq_lo = 0;
		dbs_info->freq_lo_jiffies = 0;
		return freq_next;
	}

	cpufreq_frequency_table_target(policy, dbs_info->freq_table, freq_next,
			relation, &index);
	freq_req = dbs_info->freq_table[index].frequency;
	freq_reduc = freq_req * dbs_tuners_ins.powersave_bias / 1000;
	freq_avg = freq_req - freq_reduc;

	/* Find freq bounds for freq_avg in freq_table */
	index = 0;
	cpufreq_frequency_table_target(policy, dbs_info->freq_table, freq_avg,
			CPUFREQ_RELATION_H, &index);
	freq_lo = dbs_info->freq_table[index].frequency;
	index = 0;
	cpufreq_frequency_table_target(policy, dbs_info->freq_table, freq_avg,
			CPUFREQ_RELATION_L, &index);
	freq_hi = dbs_info->freq_table[index].frequency;

	/* Find out how long we have to be in hi and lo freqs */
	if (freq_hi == freq_lo) {
		dbs_info->freq_lo = 0;
		dbs_info->freq_lo_jiffies = 0;
		return freq_lo;
	}
	jiffies_total = usecs_to_jiffies(dbs_tuners_ins.sampling_rate);
	jiffies_hi = (freq_avg - freq_lo) * jiffies_total;
	jiffies_hi += ((freq_hi - freq_lo) / 2);
	jiffies_hi /= (freq_hi - freq_lo);
	jiffies_lo = jiffies_total - jiffies_hi;
	dbs_info->freq_lo = freq_lo;
	dbs_info->freq_lo_jiffies = jiffies_lo;
	dbs_info->freq_hi_jiffies = jiffies_hi;
	return freq_hi;
}

static int ondemand_powersave_bias_setspeed(struct cpufreq_policy *policy,
					    struct cpufreq_policy *altpolicy,
					    int level)
{
	if (level == POWERSAVE_BIAS_MAXLEVEL) {
		/* maximum powersave; set to lowest frequency */
		__cpufreq_driver_target(policy,
			(altpolicy) ? altpolicy->min : policy->min,
			CPUFREQ_RELATION_L);
		return 1;
	} else if (level == POWERSAVE_BIAS_MINLEVEL) {
		/* minimum powersave; set to highest frequency */
		__cpufreq_driver_target(policy,
			(altpolicy) ? altpolicy->max : policy->max,
			CPUFREQ_RELATION_H);
		return 1;
	}
	return 0;
}

static void ondemand_powersave_bias_init_cpu(int cpu)
{
	struct cpu_dbs_info_s *dbs_info = &per_cpu(od_cpu_dbs_info, cpu);
	dbs_info->freq_table = cpufreq_frequency_get_table(cpu);
	dbs_info->freq_lo = 0;
}

static void ondemand_powersave_bias_init(void)
{
	int i;
	for_each_online_cpu(i) {
		ondemand_powersave_bias_init_cpu(i);
	}
}

/************************** sysfs interface ************************/

static ssize_t show_sampling_rate_min(struct kobject *kobj,
				      struct attribute *attr, char *buf)
{
	return sprintf(buf, "%u\n", min_sampling_rate);
}

define_one_global_ro(sampling_rate_min);

/* cpufreq_ondemand Governor Tunables */
#define show_one(file_name, object)					\
static ssize_t show_##file_name						\
(struct kobject *kobj, struct attribute *attr, char *buf)              \
{									\
	return sprintf(buf, "%u\n", dbs_tuners_ins.object);		\
}
show_one(sampling_rate, sampling_rate);
show_one(io_is_busy, io_is_busy);
show_one(up_threshold, up_threshold);
show_one(up_threshold_multi_core, up_threshold_multi_core);
show_one(down_differential, down_differential);
show_one(sampling_down_factor, sampling_down_factor);
show_one(ignore_nice_load, ignore_nice);
show_one(down_differential_multi_core, down_differential_multi_core);
show_one(optimal_freq, optimal_freq);
show_one(up_threshold_any_cpu_load, up_threshold_any_cpu_load);
show_one(sync_freq, sync_freq);
show_one(input_boost, input_boost);

static ssize_t show_powersave_bias
(struct kobject *kobj, struct attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n", dbs_tuners_ins.powersave_bias);
}

/**
 * update_sampling_rate - update sampling rate effective immediately if needed.
 * @new_rate: new sampling rate
 *
 * If new rate is smaller than the old, simply updaing
 * dbs_tuners_int.sampling_rate might not be appropriate. For example,
 * if the original sampling_rate was 1 second and the requested new sampling
 * rate is 10 ms because the user needs immediate reaction from ondemand
 * governor, but not sure if higher frequency will be required or not,
 * then, the governor may change the sampling rate too late; up to 1 second
 * later. Thus, if we are reducing the sampling rate, we need to make the
 * new value effective immediately.
 */
static void update_sampling_rate(unsigned int new_rate)
{
	int cpu;

	dbs_tuners_ins.sampling_rate = new_rate
				     = max(new_rate, min_sampling_rate);

	get_online_cpus();
	for_each_online_cpu(cpu) {
		struct cpufreq_policy *policy;
		struct cpu_dbs_info_s *dbs_info;
		unsigned long next_sampling, appointed_at;

		policy = cpufreq_cpu_get(cpu);
		if (!policy)
			continue;
		dbs_info = &per_cpu(od_cpu_dbs_info, policy->cpu);
		cpufreq_cpu_put(policy);

		mutex_lock(&dbs_info->timer_mutex);

		if (!delayed_work_pending(&dbs_info->work)) {
			mutex_unlock(&dbs_info->timer_mutex);
			continue;
		}

		next_sampling  = jiffies + usecs_to_jiffies(new_rate);
		appointed_at = dbs_info->work.timer.expires;


		if (time_before(next_sampling, appointed_at)) {

			mutex_unlock(&dbs_info->timer_mutex);
			cancel_delayed_work_sync(&dbs_info->work);
			mutex_lock(&dbs_info->timer_mutex);

			queue_delayed_work_on(dbs_info->cpu, dbs_wq,
				&dbs_info->work, usecs_to_jiffies(new_rate));

		}
		mutex_unlock(&dbs_info->timer_mutex);
	}
	put_online_cpus();
}

static ssize_t store_sampling_rate(struct kobject *a, struct attribute *b,
				   const char *buf, size_t count)
{
	unsigned int input;
	int ret;
	ret = sscanf(buf, "%u", &input);
	if (ret != 1)
		return -EINVAL;
	update_sampling_rate(input);
	return count;
}

static ssize_t store_input_boost(struct kobject *a, struct attribute *b,
				const char *buf, size_t count)
{
	unsigned int input;
	int ret;
	ret = sscanf(buf, "%u", &input);
	if (ret != 1)
		return -EINVAL;
	dbs_tuners_ins.input_boost = input;
	return count;
}

static ssize_t store_sync_freq(struct kobject *a, struct attribute *b,
				   const char *buf, size_t count)
{
	unsigned int input;
	int ret;

	ret = sscanf(buf, "%u", &input);
	if (ret != 1)
		return -EINVAL;
	dbs_tuners_ins.sync_freq = input;
	return count;
}

static ssize_t store_io_is_busy(struct kobject *a, struct attribute *b,
				   const char *buf, size_t count)
{
	unsigned int input;
	int ret;

	ret = sscanf(buf, "%u", &input);
	if (ret != 1)
		return -EINVAL;
	dbs_tuners_ins.io_is_busy = !!input;
	return count;
}

static ssize_t store_down_differential_multi_core(struct kobject *a,
			struct attribute *b, const char *buf, size_t count)
{
	unsigned int input;
	int ret;

	ret = sscanf(buf, "%u", &input);
	if (ret != 1)
		return -EINVAL;
	dbs_tuners_ins.down_differential_multi_core = input;
	return count;
}


static ssize_t store_optimal_freq(struct kobject *a, struct attribute *b,
				   const char *buf, size_t count)
{
	unsigned int input;
	int ret;

	ret = sscanf(buf, "%u", &input);
	if (ret != 1)
		return -EINVAL;
	dbs_tuners_ins.optimal_freq = input;
	return count;
}

static ssize_t store_up_threshold(struct kobject *a, struct attribute *b,
				  const char *buf, size_t count)
{
	unsigned int input;
	int ret;
	ret = sscanf(buf, "%u", &input);

	if (ret != 1 || input > MAX_FREQUENCY_UP_THRESHOLD ||
			input < MIN_FREQUENCY_UP_THRESHOLD) {
		return -EINVAL;
	}
	dbs_tuners_ins.up_threshold = input;
	return count;
}

static ssize_t store_up_threshold_multi_core(struct kobject *a,
			struct attribute *b, const char *buf, size_t count)
{
	unsigned int input;
	int ret;
	ret = sscanf(buf, "%u", &input);

	if (ret != 1 || input > MAX_FREQUENCY_UP_THRESHOLD ||
			input < MIN_FREQUENCY_UP_THRESHOLD) {
		return -EINVAL;
	}
	dbs_tuners_ins.up_threshold_multi_core = input;
	return count;
}

static ssize_t store_up_threshold_any_cpu_load(struct kobject *a,
			struct attribute *b, const char *buf, size_t count)
{
	unsigned int input;
	int ret;
	ret = sscanf(buf, "%u", &input);

	if (ret != 1 || input > MAX_FREQUENCY_UP_THRESHOLD ||
			input < MIN_FREQUENCY_UP_THRESHOLD) {
		return -EINVAL;
	}
	dbs_tuners_ins.up_threshold_any_cpu_load = input;
	return count;
}

static ssize_t store_down_differential(struct kobject *a, struct attribute *b,
		const char *buf, size_t count)
{
	unsigned int input;
	int ret;
	ret = sscanf(buf, "%u", &input);

	if (ret != 1 || input >= dbs_tuners_ins.up_threshold ||
			input < MIN_FREQUENCY_DOWN_DIFFERENTIAL) {
		return -EINVAL;
	}

	dbs_tuners_ins.down_differential = input;

	return count;
}

static ssize_t store_sampling_down_factor(struct kobject *a,
			struct attribute *b, const char *buf, size_t count)
{
	unsigned int input, j;
	int ret;
	ret = sscanf(buf, "%u", &input);

	if (ret != 1 || input > MAX_SAMPLING_DOWN_FACTOR || input < 1)
		return -EINVAL;
	dbs_tuners_ins.sampling_down_factor = input;

	/* Reset down sampling multiplier in case it was active */
	for_each_online_cpu(j) {
		struct cpu_dbs_info_s *dbs_info;
		dbs_info = &per_cpu(od_cpu_dbs_info, j);
		dbs_info->rate_mult = 1;
	}
	return count;
}

static ssize_t store_ignore_nice_load(struct kobject *a, struct attribute *b,
				      const char *buf, size_t count)
{
	unsigned int input;
	int ret;

	unsigned int j;

	ret = sscanf(buf, "%u", &input);
	if (ret != 1)
		return -EINVAL;

	if (input > 1)
		input = 1;

	if (input == dbs_tuners_ins.ignore_nice) { /* nothing to do */
		return count;
	}
	dbs_tuners_ins.ignore_nice = input;

	/* we need to re-evaluate prev_cpu_idle */
	for_each_online_cpu(j) {
		struct cpu_dbs_info_s *dbs_info;
		dbs_info = &per_cpu(od_cpu_dbs_info, j);
		dbs_info->prev_cpu_idle = get_cpu_idle_time(j,
						&dbs_info->prev_cpu_wall);
		if (dbs_tuners_ins.ignore_nice)
			dbs_info->prev_cpu_nice = kcpustat_cpu(j).cpustat[CPUTIME_NICE];

	}
	return count;
}

static ssize_t store_powersave_bias(struct kobject *a, struct attribute *b,
				    const char *buf, size_t count)
{
	int input  = 0;
	int bypass = 0;
	int ret, cpu, reenable_timer, j;
	struct cpu_dbs_info_s *dbs_info;

	struct cpumask cpus_timer_done;
	cpumask_clear(&cpus_timer_done);

	ret = sscanf(buf, "%d", &input);

	if (ret != 1)
		return -EINVAL;

	if (input >= POWERSAVE_BIAS_MAXLEVEL) {
		input  = POWERSAVE_BIAS_MAXLEVEL;
		bypass = 1;
	} else if (input <= POWERSAVE_BIAS_MINLEVEL) {
		input  = POWERSAVE_BIAS_MINLEVEL;
		bypass = 1;
	}

	if (input == dbs_tuners_ins.powersave_bias) {
		/* no change */
		return count;
	}

	reenable_timer = ((dbs_tuners_ins.powersave_bias ==
				POWERSAVE_BIAS_MAXLEVEL) ||
				(dbs_tuners_ins.powersave_bias ==
				POWERSAVE_BIAS_MINLEVEL));

	dbs_tuners_ins.powersave_bias = input;

	get_online_cpus();
	mutex_lock(&dbs_mutex);

	if (!bypass) {
		if (reenable_timer) {
			/* reinstate dbs timer */
			for_each_online_cpu(cpu) {
				if (lock_policy_rwsem_write(cpu) < 0)
					continue;

				dbs_info = &per_cpu(od_cpu_dbs_info, cpu);

				for_each_cpu(j, &cpus_timer_done) {
					if (!dbs_info->cur_policy) {
						pr_err("Dbs policy is NULL\n");
						goto skip_this_cpu;
					}
					if (cpumask_test_cpu(j, dbs_info->
							cur_policy->cpus))
						goto skip_this_cpu;
				}

				cpumask_set_cpu(cpu, &cpus_timer_done);
				if (dbs_info->cur_policy) {
					dbs_timer_exit(dbs_info);
					/* restart dbs timer */
					mutex_lock(&dbs_info->timer_mutex);
					dbs_timer_init(dbs_info);
					/* Enable frequency synchronization
					 * of CPUs */
					mutex_unlock(&dbs_info->timer_mutex);
					atomic_set(&dbs_info->sync_enabled, 1);
				}
skip_this_cpu:
				unlock_policy_rwsem_write(cpu);
			}
		}
		ondemand_powersave_bias_init();
	} else {
		/* running at maximum or minimum frequencies; cancel
		   dbs timer as periodic load sampling is not necessary */
		for_each_online_cpu(cpu) {
			if (lock_policy_rwsem_write(cpu) < 0)
				continue;

			dbs_info = &per_cpu(od_cpu_dbs_info, cpu);

			for_each_cpu(j, &cpus_timer_done) {
				if (!dbs_info->cur_policy) {
					pr_err("Dbs policy is NULL\n");
					goto skip_this_cpu_bypass;
				}
				if (cpumask_test_cpu(j, dbs_info->
							cur_policy->cpus))
					goto skip_this_cpu_bypass;
			}

			cpumask_set_cpu(cpu, &cpus_timer_done);

			if (dbs_info->cur_policy) {
				/* cpu using ondemand, cancel dbs timer */
				dbs_timer_exit(dbs_info);
				/* Disable frequency synchronization of
				 * CPUs to avoid re-queueing of work from
				 * sync_thread */
				atomic_set(&dbs_info->sync_enabled, 0);

				mutex_lock(&dbs_info->timer_mutex);
				ondemand_powersave_bias_setspeed(
					dbs_info->cur_policy,
					NULL,
					input);
				mutex_unlock(&dbs_info->timer_mutex);

			}
skip_this_cpu_bypass:
			unlock_policy_rwsem_write(cpu);
		}
	}

	mutex_unlock(&dbs_mutex);
	put_online_cpus();

	return count;
}

define_one_global_rw(sampling_rate);
define_one_global_rw(io_is_busy);
define_one_global_rw(up_threshold);
define_one_global_rw(down_differential);
define_one_global_rw(sampling_down_factor);
define_one_global_rw(ignore_nice_load);
define_one_global_rw(powersave_bias);
define_one_global_rw(up_threshold_multi_core);
define_one_global_rw(down_differential_multi_core);
define_one_global_rw(optimal_freq);
define_one_global_rw(up_threshold_any_cpu_load);
define_one_global_rw(sync_freq);
define_one_global_rw(input_boost);

static struct attribute *dbs_attributes[] = {
	&sampling_rate_min.attr,
	&sampling_rate.attr,
	&up_threshold.attr,
	&down_differential.attr,
	&sampling_down_factor.attr,
	&ignore_nice_load.attr,
	&powersave_bias.attr,
	&io_is_busy.attr,
	&up_threshold_multi_core.attr,
	&down_differential_multi_core.attr,
	&optimal_freq.attr,
	&up_threshold_any_cpu_load.attr,
	&sync_freq.attr,
	&input_boost.attr,
	NULL
};

static struct attribute_group dbs_attr_group = {
	.attrs = dbs_attributes,
	.name = "ondemand",
};

/************************** sysfs end ************************/

static void dbs_freq_increase(struct cpufreq_policy *p, unsigned int freq)
{
	if (dbs_tuners_ins.powersave_bias)
		freq = powersave_bias_target(p, freq, CPUFREQ_RELATION_H);
	else if (p->cur == p->max)
		return;

	__cpufreq_driver_target(p, freq, dbs_tuners_ins.powersave_bias ?
			CPUFREQ_RELATION_L : CPUFREQ_RELATION_H);
}

static void dbs_check_cpu(struct cpu_dbs_info_s *this_dbs_info)
{
	/* Extrapolated load of this CPU */
	unsigned int load_at_max_freq = 0;
	unsigned int max_load_freq;
	/* Current load across this CPU */
	unsigned int cur_load = 0;
	unsigned int max_load_other_cpu = 0;
	struct cpufreq_policy *policy;
	unsigned int j;

	this_dbs_info->freq_lo = 0;
	policy = this_dbs_info->cur_policy;

	/*
	 * Every sampling_rate, we check, if current idle time is less
	 * than 20% (default), then we try to increase frequency
	 * Every sampling_rate, we look for a the lowest
	 * frequency which can sustain the load while keeping idle time over
	 * 30%. If such a frequency exist, we try to decrease to this frequency.
	 *
	 * Any frequency increase takes it to the maximum frequency.
	 * Frequency reduction happens at minimum steps of
	 * 5% (default) of current frequency
	 */

	/* Get Absolute Load - in terms of freq */
	max_load_freq = 0;

	for_each_cpu(j, policy->cpus) {
		struct cpu_dbs_info_s *j_dbs_info;
		cputime64_t cur_wall_time, cur_idle_time, cur_iowait_time;
		unsigned int idle_time, wall_time, iowait_time;
		unsigned int load_freq;
		int freq_avg;

		j_dbs_info = &per_cpu(od_cpu_dbs_info, j);

		cur_idle_time = get_cpu_idle_time(j, &cur_wall_time);
		cur_iowait_time = get_cpu_iowait_time(j, &cur_wall_time);

		wall_time = (unsigned int)
			(cur_wall_time - j_dbs_info->prev_cpu_wall);
		j_dbs_info->prev_cpu_wall = cur_wall_time;

		idle_time = (unsigned int)
			(cur_idle_time - j_dbs_info->prev_cpu_idle);
		j_dbs_info->prev_cpu_idle = cur_idle_time;

		iowait_time = (unsigned int)
			(cur_iowait_time - j_dbs_info->prev_cpu_iowait);
		j_dbs_info->prev_cpu_iowait = cur_iowait_time;

		if (dbs_tuners_ins.ignore_nice) {
			u64 cur_nice;
			unsigned long cur_nice_jiffies;

			cur_nice = kcpustat_cpu(j).cpustat[CPUTIME_NICE] -
					 j_dbs_info->prev_cpu_nice;
			/*
			 * Assumption: nice time between sampling periods will
			 * be less than 2^32 jiffies for 32 bit sys
			 */
			cur_nice_jiffies = (unsigned long)
					cputime64_to_jiffies64(cur_nice);

			j_dbs_info->prev_cpu_nice = kcpustat_cpu(j).cpustat[CPUTIME_NICE];
			idle_time += jiffies_to_usecs(cur_nice_jiffies);
		}

		/*
		 * For the purpose of ondemand, waiting for disk IO is an
		 * indication that you're performance critical, and not that
		 * the system is actually idle. So subtract the iowait time
		 * from the cpu idle time.
		 */

		if (dbs_tuners_ins.io_is_busy && idle_time >= iowait_time)
			idle_time -= iowait_time;

		if (unlikely(!wall_time || wall_time < idle_time))
			continue;

		cur_load = 100 * (wall_time - idle_time) / wall_time;
		j_dbs_info->max_load  = max(cur_load, j_dbs_info->prev_load);
		j_dbs_info->prev_load = cur_load;
		freq_avg = __cpufreq_driver_getavg(policy, j);
		if (freq_avg <= 0)
			freq_avg = policy->cur;

		load_freq = cur_load * freq_avg;
		if (load_freq > max_load_freq)
			max_load_freq = load_freq;
	}

	for_each_online_cpu(j) {
		struct cpu_dbs_info_s *j_dbs_info;
		j_dbs_info = &per_cpu(od_cpu_dbs_info, j);

		if (j == policy->cpu)
			continue;

		if (max_load_other_cpu < j_dbs_info->max_load)
			max_load_other_cpu = j_dbs_info->max_load;
		/*
		 * The other cpu could be running at higher frequency
		 * but may not have completed it's sampling_down_factor.
		 * For that case consider other cpu is loaded so that
		 * frequency imbalance does not occur.
		 */

		if ((j_dbs_info->cur_policy != NULL)
			&& (j_dbs_info->cur_policy->cur ==
					j_dbs_info->cur_policy->max)) {

			if (policy->cur >= dbs_tuners_ins.optimal_freq)
				max_load_other_cpu =
				dbs_tuners_ins.up_threshold_any_cpu_load;
		}
	}

	/* calculate the scaled load across CPU */
	load_at_max_freq = (cur_load * policy->cur)/policy->cpuinfo.max_freq;

	cpufreq_notify_utilization(policy, load_at_max_freq);
	/* Check for frequency increase */
	if (max_load_freq > dbs_tuners_ins.up_threshold * policy->cur) {
		/* If switching to max speed, apply sampling_down_factor */
		if (policy->cur < policy->max)
			this_dbs_info->rate_mult =
				dbs_tuners_ins.sampling_down_factor;
		dbs_freq_increase(policy, policy->max);
		return;
	}

	if (num_online_cpus() > 1) {

		if (max_load_other_cpu >
				dbs_tuners_ins.up_threshold_any_cpu_load) {
			if (policy->cur < dbs_tuners_ins.sync_freq)
				dbs_freq_increase(policy,
						dbs_tuners_ins.sync_freq);
			return;
		}

		if (max_load_freq > dbs_tuners_ins.up_threshold_multi_core *
								policy->cur) {
			if (policy->cur < dbs_tuners_ins.optimal_freq)
				dbs_freq_increase(policy,
						dbs_tuners_ins.optimal_freq);
			return;
		}
	}

	/* Check for frequency decrease */
	/* if we cannot reduce the frequency anymore, break out early */
	if (policy->cur == policy->min)
		return;

	/*
	 * The optimal frequency is the frequency that is the lowest that
	 * can support the current CPU usage without triggering the up
	 * policy. To be safe, we focus 10 points under the threshold.
	 */
	if (max_load_freq <
	    (dbs_tuners_ins.up_threshold - dbs_tuners_ins.down_differential) *
	     policy->cur) {
		unsigned int freq_next;
		freq_next = max_load_freq /
				(dbs_tuners_ins.up_threshold -
				 dbs_tuners_ins.down_differential);

		/* No longer fully busy, reset rate_mult */
		this_dbs_info->rate_mult = 1;

		if (freq_next < policy->min)
			freq_next = policy->min;

		if (num_online_cpus() > 1) {
			if (max_load_other_cpu >
			(dbs_tuners_ins.up_threshold_multi_core -
			dbs_tuners_ins.down_differential) &&
			freq_next < dbs_tuners_ins.sync_freq)
				freq_next = dbs_tuners_ins.sync_freq;

			if (max_load_freq >
				 ((dbs_tuners_ins.up_threshold_multi_core -
				  dbs_tuners_ins.down_differential_multi_core) *
				  policy->cur) &&
				freq_next < dbs_tuners_ins.optimal_freq)
				freq_next = dbs_tuners_ins.optimal_freq;

		}
		if (!dbs_tuners_ins.powersave_bias) {
			__cpufreq_driver_target(policy, freq_next,
					CPUFREQ_RELATION_L);
		} else {
			int freq = powersave_bias_target(policy, freq_next,
					CPUFREQ_RELATION_L);
			__cpufreq_driver_target(policy, freq,
				CPUFREQ_RELATION_L);
		}
	}
}

static void do_dbs_timer(struct work_struct *work)
{
	struct cpu_dbs_info_s *dbs_info =
		container_of(work, struct cpu_dbs_info_s, work.work);
	unsigned int cpu = dbs_info->cpu;
	int sample_type = dbs_info->sample_type;

	int delay;

	mutex_lock(&dbs_info->timer_mutex);

	/* Common NORMAL_SAMPLE setup */
	dbs_info->sample_type = DBS_NORMAL_SAMPLE;
	if (!dbs_tuners_ins.powersave_bias ||
	    sample_type == DBS_NORMAL_SAMPLE) {
		dbs_check_cpu(dbs_info);
		if (dbs_info->freq_lo) {
			/* Setup timer for SUB_SAMPLE */
			dbs_info->sample_type = DBS_SUB_SAMPLE;
			delay = dbs_info->freq_hi_jiffies;
		} else {
			delay = usecs_to_jiffies(dbs_tuners_ins.sampling_rate
				* dbs_info->rate_mult);
		}
	} else {
		__cpufreq_driver_target(dbs_info->cur_policy,
			dbs_info->freq_lo, CPUFREQ_RELATION_H);
		delay = dbs_info->freq_lo_jiffies;
	}
	queue_delayed_work_on(cpu, dbs_wq, &dbs_info->work, delay);
	mutex_unlock(&dbs_info->timer_mutex);
}

static inline void dbs_timer_init(struct cpu_dbs_info_s *dbs_info)
{
	/* We want all CPUs to do sampling nearly on same jiffy */
	int delay = usecs_to_jiffies(dbs_tuners_ins.sampling_rate);

	if (num_online_cpus() > 1)
		delay -= jiffies % delay;

	dbs_info->sample_type = DBS_NORMAL_SAMPLE;
	INIT_DELAYED_WORK_DEFERRABLE(&dbs_info->work, do_dbs_timer);
	queue_delayed_work_on(dbs_info->cpu, dbs_wq, &dbs_info->work, delay);
}

static inline void dbs_timer_exit(struct cpu_dbs_info_s *dbs_info)
{
	cancel_delayed_work_sync(&dbs_info->work);
}

/*
 * Not all CPUs want IO time to be accounted as busy; this dependson how
 * efficient idling at a higher frequency/voltage is.
 * Pavel Machek says this is not so for various generations of AMD and old
 * Intel systems.
 * Mike Chan (androidlcom) calis this is also not true for ARM.
 * Because of this, whitelist specific known (series) of CPUs by default, and
 * leave all others up to the user.
 */
static int should_io_be_busy(void)
{
#if defined(CONFIG_X86)
	/*
	 * For Intel, Core 2 (model 15) andl later have an efficient idle.
	 */
	if (boot_cpu_data.x86_vendor == X86_VENDOR_INTEL &&
	    boot_cpu_data.x86 == 6 &&
	    boot_cpu_data.x86_model >= 15)
		return 1;
#endif
	return 0;
}

static void dbs_refresh_callback(struct work_struct *work)
{
	struct cpufreq_policy *policy;
	struct cpu_dbs_info_s *this_dbs_info;
	struct dbs_work_struct *dbs_work;
	unsigned int cpu;
	unsigned int target_freq;

	dbs_work = container_of(work, struct dbs_work_struct, work);
	cpu = dbs_work->cpu;

	get_online_cpus();

	if (lock_policy_rwsem_write(cpu) < 0)
		goto bail_acq_sema_failed;

	this_dbs_info = &per_cpu(od_cpu_dbs_info, cpu);
	policy = this_dbs_info->cur_policy;
	if (!policy) {
		/* CPU not using ondemand governor */
		goto bail_incorrect_governor;
	}

	if (dbs_tuners_ins.input_boost)
		target_freq = dbs_tuners_ins.input_boost;
	else
		target_freq = policy->max;

	if (policy->cur < target_freq) {
		/*
		 * Arch specific cpufreq driver may fail.
		 * Don't update governor frequency upon failure.
		 */
		if (__cpufreq_driver_target(policy, target_freq,
					CPUFREQ_RELATION_L) >= 0)
			policy->cur = target_freq;

		this_dbs_info->prev_cpu_idle = get_cpu_idle_time(cpu,
				&this_dbs_info->prev_cpu_wall);
	}

bail_incorrect_governor:
	unlock_policy_rwsem_write(cpu);

bail_acq_sema_failed:
	put_online_cpus();
	return;
}

static int dbs_migration_notify(struct notifier_block *nb,
				unsigned long target_cpu, void *arg)
{
	struct cpu_dbs_info_s *target_dbs_info =
		&per_cpu(od_cpu_dbs_info, target_cpu);

	atomic_set(&target_dbs_info->src_sync_cpu, (int)arg);
	wake_up(&target_dbs_info->sync_wq);

	return NOTIFY_OK;
}

static struct notifier_block dbs_migration_nb = {
	.notifier_call = dbs_migration_notify,
};

static int sync_pending(struct cpu_dbs_info_s *this_dbs_info)
{
	return atomic_read(&this_dbs_info->src_sync_cpu) >= 0;
}

static int dbs_sync_thread(void *data)
{
	int src_cpu, cpu = (int)data;
	unsigned int src_freq, src_max_load;
	struct cpu_dbs_info_s *this_dbs_info, *src_dbs_info;
	struct cpufreq_policy *policy;
	int delay;

	this_dbs_info = &per_cpu(od_cpu_dbs_info, cpu);

	while (1) {
		wait_event(this_dbs_info->sync_wq,
			   sync_pending(this_dbs_info) ||
			   kthread_should_stop());

		if (kthread_should_stop())
			break;

		get_online_cpus();

		src_cpu = atomic_read(&this_dbs_info->src_sync_cpu);
		src_dbs_info = &per_cpu(od_cpu_dbs_info, src_cpu);
		if (src_dbs_info != NULL &&
		    src_dbs_info->cur_policy != NULL) {
			src_freq = src_dbs_info->cur_policy->cur;
			src_max_load = src_dbs_info->max_load;
		} else {
			src_freq = dbs_tuners_ins.sync_freq;
			src_max_load = 0;
		}

		if (lock_policy_rwsem_write(cpu) < 0)
			goto bail_acq_sema_failed;

		if (!atomic_read(&this_dbs_info->sync_enabled)) {
			atomic_set(&this_dbs_info->src_sync_cpu, -1);
			put_online_cpus();
			unlock_policy_rwsem_write(cpu);
			continue;
		}

		policy = this_dbs_info->cur_policy;
		if (!policy) {
			/* CPU not using ondemand governor */
			goto bail_incorrect_governor;
		}
		delay = usecs_to_jiffies(dbs_tuners_ins.sampling_rate);


		if (policy->cur < src_freq) {
			/* cancel the next ondemand sample */
			cancel_delayed_work_sync(&this_dbs_info->work);

			/*
			 * Arch specific cpufreq driver may fail.
			 * Don't update governor frequency upon failure.
			 */
			if (__cpufreq_driver_target(policy, src_freq,
						    CPUFREQ_RELATION_L) >= 0) {
				policy->cur = src_freq;
				if (src_max_load > this_dbs_info->max_load) {
					this_dbs_info->max_load = src_max_load;
					this_dbs_info->prev_load = src_max_load;
				}
			}

			/* reschedule the next ondemand sample */
			mutex_lock(&this_dbs_info->timer_mutex);
			queue_delayed_work_on(cpu, dbs_wq,
					      &this_dbs_info->work, delay);
			mutex_unlock(&this_dbs_info->timer_mutex);
		}

bail_incorrect_governor:
		unlock_policy_rwsem_write(cpu);
bail_acq_sema_failed:
		put_online_cpus();
		atomic_set(&this_dbs_info->src_sync_cpu, -1);
	}

	return 0;
}

static void dbs_input_event(struct input_handle *handle, unsigned int type,
		unsigned int code, int value)
{
	int i;

	if ((dbs_tuners_ins.powersave_bias == POWERSAVE_BIAS_MAXLEVEL) ||
		(dbs_tuners_ins.powersave_bias == POWERSAVE_BIAS_MINLEVEL)) {
		/* nothing to do */
		return;
	}

	for_each_online_cpu(i)
		queue_work_on(i, dbs_wq, &per_cpu(dbs_refresh_work, i).work);
}

static int dbs_input_connect(struct input_handler *handler,
		struct input_dev *dev, const struct input_device_id *id)
{
	struct input_handle *handle;
	int error;

	handle = kzalloc(sizeof(struct input_handle), GFP_KERNEL);
	if (!handle)
		return -ENOMEM;

	handle->dev = dev;
	handle->handler = handler;
	handle->name = "cpufreq";

	error = input_register_handle(handle);
	if (error)
		goto err2;

	error = input_open_device(handle);
	if (error)
		goto err1;

	return 0;
err1:
	input_unregister_handle(handle);
err2:
	kfree(handle);
	return error;
}

static void dbs_input_disconnect(struct input_handle *handle)
{
	input_close_device(handle);
	input_unregister_handle(handle);
	kfree(handle);
}

static const struct input_device_id dbs_ids[] = {
	/* multi-touch touchscreen */
	{
		.flags = INPUT_DEVICE_ID_MATCH_EVBIT |
			INPUT_DEVICE_ID_MATCH_ABSBIT,
		.evbit = { BIT_MASK(EV_ABS) },
		.absbit = { [BIT_WORD(ABS_MT_POSITION_X)] =
			BIT_MASK(ABS_MT_POSITION_X) |
			BIT_MASK(ABS_MT_POSITION_Y) },
	},
	/* touchpad */
	{
		.flags = INPUT_DEVICE_ID_MATCH_KEYBIT |
			INPUT_DEVICE_ID_MATCH_ABSBIT,
		.keybit = { [BIT_WORD(BTN_TOUCH)] = BIT_MASK(BTN_TOUCH) },
		.absbit = { [BIT_WORD(ABS_X)] =
			BIT_MASK(ABS_X) | BIT_MASK(ABS_Y) },
	},
	/* Keypad */
	{
		.flags = INPUT_DEVICE_ID_MATCH_EVBIT,
		.evbit = { BIT_MASK(EV_KEY) },
	},
	{ },
};

static struct input_handler dbs_input_handler = {
	.event		= dbs_input_event,
	.connect	= dbs_input_connect,
	.disconnect	= dbs_input_disconnect,
	.name		= "cpufreq_ond",
	.id_table	= dbs_ids,
};

static int cpufreq_governor_dbs(struct cpufreq_policy *policy,
				   unsigned int event)
{
	unsigned int cpu = policy->cpu;
	struct cpu_dbs_info_s *this_dbs_info;
	unsigned int j;
	int rc;

	this_dbs_info = &per_cpu(od_cpu_dbs_info, cpu);

	switch (event) {
	case CPUFREQ_GOV_START:
		if ((!cpu_online(cpu)) || (!policy->cur))
			return -EINVAL;

		mutex_lock(&dbs_mutex);

		dbs_enable++;
		for_each_cpu(j, policy->cpus) {
			struct cpu_dbs_info_s *j_dbs_info;
			j_dbs_info = &per_cpu(od_cpu_dbs_info, j);
			j_dbs_info->cur_policy = policy;

			j_dbs_info->prev_cpu_idle = get_cpu_idle_time(j,
						&j_dbs_info->prev_cpu_wall);
			if (dbs_tuners_ins.ignore_nice)
				j_dbs_info->prev_cpu_nice =
						kcpustat_cpu(j).cpustat[CPUTIME_NICE];
			set_cpus_allowed(j_dbs_info->sync_thread,
					 *cpumask_of(j));
			if (!dbs_tuners_ins.powersave_bias)
				atomic_set(&j_dbs_info->sync_enabled, 1);
		}
		this_dbs_info->cpu = cpu;
		this_dbs_info->rate_mult = 1;
		ondemand_powersave_bias_init_cpu(cpu);
		/*
		 * Start the timerschedule work, when this governor
		 * is used for first time
		 */
		if (dbs_enable == 1) {
			unsigned int latency;

			rc = sysfs_create_group(cpufreq_global_kobject,
						&dbs_attr_group);
			if (rc) {
				mutex_unlock(&dbs_mutex);
				return rc;
			}

			/* policy latency is in nS. Convert it to uS first */
			latency = policy->cpuinfo.transition_latency / 1000;
			if (latency == 0)
				latency = 1;
			/* Bring kernel and HW constraints together */
			min_sampling_rate = max(min_sampling_rate,
					MIN_LATENCY_MULTIPLIER * latency);
			dbs_tuners_ins.sampling_rate =
				max(min_sampling_rate,
				    latency * LATENCY_MULTIPLIER);
			dbs_tuners_ins.io_is_busy = should_io_be_busy();

			if (dbs_tuners_ins.optimal_freq == 0)
				dbs_tuners_ins.optimal_freq = policy->min;

			if (dbs_tuners_ins.sync_freq == 0)
				dbs_tuners_ins.sync_freq = policy->min;

			atomic_notifier_chain_register(&migration_notifier_head,
					&dbs_migration_nb);
		}
		if (!cpu)
			rc = input_register_handler(&dbs_input_handler);
		mutex_unlock(&dbs_mutex);


		if (!ondemand_powersave_bias_setspeed(
					this_dbs_info->cur_policy,
					NULL,
					dbs_tuners_ins.powersave_bias))
			dbs_timer_init(this_dbs_info);
		break;

	case CPUFREQ_GOV_STOP:
		dbs_timer_exit(this_dbs_info);

		mutex_lock(&dbs_mutex);
		dbs_enable--;

		for_each_cpu(j, policy->cpus) {
			struct cpu_dbs_info_s *j_dbs_info;
			j_dbs_info = &per_cpu(od_cpu_dbs_info, j);
			atomic_set(&j_dbs_info->sync_enabled, 0);
		}

		/* If device is being removed, policy is no longer
		 * valid. */
		this_dbs_info->cur_policy = NULL;
		if (!cpu)
			input_unregister_handler(&dbs_input_handler);
		if (!dbs_enable) {
			sysfs_remove_group(cpufreq_global_kobject,
					   &dbs_attr_group);
			atomic_notifier_chain_unregister(
				&migration_notifier_head,
				&dbs_migration_nb);
		}

		mutex_unlock(&dbs_mutex);

		break;

	case CPUFREQ_GOV_LIMITS:
		mutex_lock(&this_dbs_info->timer_mutex);
		if (policy->max < this_dbs_info->cur_policy->cur)
			__cpufreq_driver_target(this_dbs_info->cur_policy,
				policy->max, CPUFREQ_RELATION_H);
		else if (policy->min > this_dbs_info->cur_policy->cur)
			__cpufreq_driver_target(this_dbs_info->cur_policy,
				policy->min, CPUFREQ_RELATION_L);
		else if (dbs_tuners_ins.powersave_bias != 0)
			ondemand_powersave_bias_setspeed(
				this_dbs_info->cur_policy,
				policy,
				dbs_tuners_ins.powersave_bias);
		mutex_unlock(&this_dbs_info->timer_mutex);
		break;
	}
	return 0;
}

static int __init cpufreq_gov_dbs_init(void)
{
	u64 idle_time;
	unsigned int i;
	int cpu = get_cpu();

	idle_time = get_cpu_idle_time_us(cpu, NULL);
	put_cpu();
	if (idle_time != -1ULL) {
		/* Idle micro accounting is supported. Use finer thresholds */
		dbs_tuners_ins.up_threshold = MICRO_FREQUENCY_UP_THRESHOLD;
		dbs_tuners_ins.down_differential =
					MICRO_FREQUENCY_DOWN_DIFFERENTIAL;
		/*
		 * In nohz/micro accounting case we set the minimum frequency
		 * not depending on HZ, but fixed (very low). The deferred
		 * timer might skip some samples if idle/sleeping as needed.
		*/
		min_sampling_rate = MICRO_FREQUENCY_MIN_SAMPLE_RATE;
	} else {
		/* For correct statistics, we need 10 ticks for each measure */
		min_sampling_rate =
			MIN_SAMPLING_RATE_RATIO * jiffies_to_usecs(10);
	}

	dbs_wq = alloc_workqueue("ondemand_dbs_wq", WQ_HIGHPRI, 0);
	if (!dbs_wq) {
		printk(KERN_ERR "Failed to create ondemand_dbs_wq workqueue\n");
		return -EFAULT;
	}
	for_each_possible_cpu(i) {
		struct cpu_dbs_info_s *this_dbs_info =
			&per_cpu(od_cpu_dbs_info, i);
		struct dbs_work_struct *dbs_work =
			&per_cpu(dbs_refresh_work, i);

		mutex_init(&this_dbs_info->timer_mutex);
		INIT_WORK(&dbs_work->work, dbs_refresh_callback);
		dbs_work->cpu = i;

		atomic_set(&this_dbs_info->src_sync_cpu, -1);
		init_waitqueue_head(&this_dbs_info->sync_wq);

		this_dbs_info->sync_thread = kthread_run(dbs_sync_thread,
							 (void *)i,
							 "dbs_sync/%d", i);
	}

	return cpufreq_register_governor(&cpufreq_gov_ondemand);
}

static void __exit cpufreq_gov_dbs_exit(void)
{
	unsigned int i;

	cpufreq_unregister_governor(&cpufreq_gov_ondemand);
	for_each_possible_cpu(i) {
		struct cpu_dbs_info_s *this_dbs_info =
			&per_cpu(od_cpu_dbs_info, i);
		mutex_destroy(&this_dbs_info->timer_mutex);
		kthread_stop(this_dbs_info->sync_thread);
	}
	destroy_workqueue(dbs_wq);
}


MODULE_AUTHOR("Venkatesh Pallipadi <venkatesh.pallipadi@intel.com>");
MODULE_AUTHOR("Alexey Starikovskiy <alexey.y.starikovskiy@intel.com>");
MODULE_DESCRIPTION("'cpufreq_ondemand' - A dynamic cpufreq governor for "
	"Low Latency Frequency Transition capable processors");
MODULE_LICENSE("GPL");

#ifdef CONFIG_CPU_FREQ_DEFAULT_GOV_ONDEMAND
fs_initcall(cpufreq_gov_dbs_init);
#else
module_init(cpufreq_gov_dbs_init);
#endif
module_exit(cpufreq_gov_dbs_exit);
