/*
 *  drivers/cpufreq/cpufreq_hotplug.c
 *
 *  Copyright (C)  2001 Russell King
 *            (C)  2003 Venkatesh Pallipadi <venkatesh.pallipadi@intel.com>.
 *                      Jun Nakajima <jun.nakajima@intel.com>
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
#include <linux/sched.h>
#include <linux/input.h>
#include <linux/slab.h>
#include <linux/sched/rt.h>
#include <linux/kthread.h>

extern unsigned int get_normal_max_freq(void);
extern unsigned int mt_dvfs_power_dispatch_safe(void);
extern int mt_gpufreq_target(int idx);
/*
 * dbs is used in this file as a shortform for demandbased switching
 * It helps to keep variable names smaller, simpler
 */

#define DEF_FREQUENCY_DOWN_DIFFERENTIAL     (10)
#define DEF_FREQUENCY_OD_THRESHOLD          (98)
#define DEF_FREQUENCY_UP_THRESHOLD          (80)
#define DEF_SAMPLING_DOWN_FACTOR            (1)
#define MAX_SAMPLING_DOWN_FACTOR            (100000)
#define MICRO_FREQUENCY_DOWN_DIFFERENTIAL   (15)
#define MIN_FREQUENCY_DOWN_DIFFERENTIAL     (5)
#define MAX_FREQUENCY_DOWN_DIFFERENTIAL     (20)
#define MICRO_FREQUENCY_UP_THRESHOLD        (85)
#define MICRO_FREQUENCY_MIN_SAMPLE_RATE     (30000)
#define MIN_FREQUENCY_UP_THRESHOLD          (21)
#define MAX_FREQUENCY_UP_THRESHOLD          (100)

#define DEF_CPU_DOWN_DIFFERENTIAL   (10)
#define MICRO_CPU_DOWN_DIFFERENTIAL (10)
#define MIN_CPU_DOWN_DIFFERENTIAL   (0)
#define MAX_CPU_DOWN_DIFFERENTIAL   (30)

#define DEF_CPU_UP_THRESHOLD        (90)
#define MICRO_CPU_UP_THRESHOLD      (90)
#define MIN_CPU_UP_THRESHOLD        (80)
#define MAX_CPU_UP_THRESHOLD        (100)

#define CPU_UP_AVG_TIMES            (10)
#define CPU_DOWN_AVG_TIMES          (50)
#define THERMAL_DISPATCH_AVG_TIMES  (30)

#define DEF_CPU_PERSIST_COUNT   (10)

//#define DEBUG_LOG
#define INPUT_BOOST             (1)

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

static void do_dbs_timer(struct work_struct *work);
static int cpufreq_governor_dbs(struct cpufreq_policy *policy,
				unsigned int event);

#ifndef CONFIG_CPU_FREQ_DEFAULT_GOV_BALANCE
static
#endif
struct cpufreq_governor cpufreq_gov_balance = {
       .name                   = "hotplug",
       .governor               = cpufreq_governor_dbs,
       .max_transition_latency = TRANSITION_LATENCY_LIMIT,
       .owner                  = THIS_MODULE,
};

#ifdef CONFIG_SMP

static int g_next_hp_action = 0;

static long g_cpu_up_sum_load = 0;
static int g_cpu_up_count = 0;

static long g_cpu_down_sum_load = 0;
static int g_cpu_down_count = 0;
static int g_max_cpu_persist_count = 0;
static int g_thermal_count = 0;

static void hp_work_handler(struct work_struct *work);
static struct delayed_work hp_work;

#if INPUT_BOOST
static struct task_struct *freq_up_task;
#endif

#endif

static int cpu_loading = 0;
static int cpus_sum_load = 0;
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
	int cpu;
	unsigned int sample_type:1;
	/*
	 * percpu mutex that serializes governor limit change with
	 * do_dbs_timer invocation. We do not want do_dbs_timer to run
	 * when user is changing the governor or limits.
	 */
	struct mutex timer_mutex;
};
static DEFINE_PER_CPU(struct cpu_dbs_info_s, hp_cpu_dbs_info);

static unsigned int dbs_enable;	/* number of CPUs using this policy */
static unsigned int dbs_ignore = 1;
static unsigned int dbs_thermal_limited;
static unsigned int dbs_thermal_limited_freq;

/* dvfs thermal limit */
void dbs_freq_thermal_limited(unsigned int limited, unsigned int freq)
{
	dbs_thermal_limited = limited;
	dbs_thermal_limited_freq = freq;
}
EXPORT_SYMBOL(dbs_freq_thermal_limited);

/*
 * dbs_mutex protects dbs_enable in governor start/stop.
 */
static DEFINE_MUTEX(dbs_mutex);

/*
 * dbs_hotplug protects all hotplug related global variables
 */
static DEFINE_MUTEX(hp_mutex);

DEFINE_MUTEX(bl_onoff_mutex);

static struct dbs_tuners {
    unsigned int sampling_rate;
    unsigned int od_threshold;
    unsigned int up_threshold;
    unsigned int down_differential;
    unsigned int ignore_nice;
    unsigned int sampling_down_factor;
    unsigned int powersave_bias;
    unsigned int io_is_busy;
    unsigned int cpu_up_threshold;
    unsigned int cpu_down_differential;
    unsigned int cpu_up_avg_times;
    unsigned int cpu_down_avg_times;
    unsigned int thermal_dispatch_avg_times;
    unsigned int cpu_num_limit;
    unsigned int cpu_num_base;
    unsigned int is_cpu_hotplug_disable;
#if INPUT_BOOST
    unsigned int cpu_input_boost_enable;
#endif
} dbs_tuners_ins = {
    .od_threshold = DEF_FREQUENCY_OD_THRESHOLD,
    .up_threshold = DEF_FREQUENCY_UP_THRESHOLD,
    .sampling_down_factor = DEF_SAMPLING_DOWN_FACTOR,
    .down_differential = DEF_FREQUENCY_DOWN_DIFFERENTIAL,
    .ignore_nice = 0,
    .powersave_bias = 0,
    .cpu_up_threshold = DEF_CPU_UP_THRESHOLD,
    .cpu_down_differential = DEF_CPU_DOWN_DIFFERENTIAL,
    .cpu_up_avg_times = CPU_UP_AVG_TIMES,
    .cpu_down_avg_times = CPU_DOWN_AVG_TIMES,
    .thermal_dispatch_avg_times = THERMAL_DISPATCH_AVG_TIMES,
    .cpu_num_limit = 1,
    .cpu_num_base = 1,
    .is_cpu_hotplug_disable = 1,
#if INPUT_BOOST
    .cpu_input_boost_enable = 1,
#endif
};

static void dbs_freq_increase(struct cpufreq_policy *p, unsigned int freq);

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

/* static inline cputime64_t get_cpu_idle_time(unsigned int cpu, cputime64_t *wall) */
/* { */
/* 	u64 idle_time = get_cpu_idle_time_us(cpu, NULL); */

/* 	if (idle_time == -1ULL) */
/* 		return get_cpu_idle_time_jiffy(cpu, wall); */
/* 	else */
/* 		idle_time += get_cpu_iowait_time_us(cpu, wall); */

/* 	return idle_time; */
/* } */

static inline cputime64_t get_cpu_iowait_time(unsigned int cpu, cputime64_t *wall)
{
	u64 iowait_time = get_cpu_iowait_time_us(cpu, wall);

	if (iowait_time == -1ULL)
		return 0;

	return iowait_time;
}

void force_two_core(void)
{
    bool raise_freq = false;

    mutex_lock(&hp_mutex);
    g_cpu_down_count = 0;
    g_cpu_down_sum_load = 0;
    if (num_online_cpus() < dbs_tuners_ins.cpu_num_limit) {
        raise_freq = true;
        g_next_hp_action = 1;
        schedule_delayed_work_on(0, &hp_work, 0);
    }
    mutex_unlock(&hp_mutex);

    if (raise_freq == true) {
	wake_up_process(freq_up_task);
    }

    mt_gpufreq_target(0);
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
	unsigned int freq_req, freq_reduc, freq_avg;
	unsigned int freq_hi, freq_lo;
	unsigned int index = 0;
	unsigned int jiffies_total, jiffies_hi, jiffies_lo;
	struct cpu_dbs_info_s *dbs_info = &per_cpu(hp_cpu_dbs_info,
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

static void hotplug_powersave_bias_init_cpu(int cpu)
{
	struct cpu_dbs_info_s *dbs_info = &per_cpu(hp_cpu_dbs_info, cpu);
	dbs_info->freq_table = cpufreq_frequency_get_table(cpu);
	dbs_info->freq_lo = 0;
}

static void hotplug_powersave_bias_init(void)
{
	int i;
	for_each_online_cpu(i) {
		hotplug_powersave_bias_init_cpu(i);
	}
}

/************************** sysfs interface ************************/

static ssize_t show_sampling_rate_min(struct kobject *kobj,
				      struct attribute *attr, char *buf)
{
	return sprintf(buf, "%u\n", min_sampling_rate);
}

define_one_global_ro(sampling_rate_min);

/* cpufreq_hotplug Governor Tunables */
#define show_one(file_name, object)					\
static ssize_t show_##file_name						\
(struct kobject *kobj, struct attribute *attr, char *buf)              \
{									\
	return sprintf(buf, "%u\n", dbs_tuners_ins.object);		\
}

show_one(sampling_rate, sampling_rate);
show_one(io_is_busy, io_is_busy);
show_one(up_threshold, up_threshold);
show_one(od_threshold, od_threshold);
show_one(down_differential, down_differential);
show_one(sampling_down_factor, sampling_down_factor);
show_one(ignore_nice_load, ignore_nice);
show_one(powersave_bias, powersave_bias);
show_one(cpu_up_threshold, cpu_up_threshold);
show_one(cpu_down_differential, cpu_down_differential);
show_one(cpu_up_avg_times, cpu_up_avg_times);
show_one(cpu_down_avg_times, cpu_down_avg_times);
show_one(thermal_dispatch_avg_times, thermal_dispatch_avg_times);
show_one(cpu_num_limit, cpu_num_limit);
show_one(cpu_num_base, cpu_num_base);
show_one(is_cpu_hotplug_disable, is_cpu_hotplug_disable);
#if INPUT_BOOST
show_one(cpu_input_boost_enable, cpu_input_boost_enable);
#endif

/**
 * update_sampling_rate - update sampling rate effective immediately if needed.
 * @new_rate: new sampling rate
 *
 * If new rate is smaller than the old, simply updaing
 * dbs_tuners_int.sampling_rate might not be appropriate. For example,
 * if the original sampling_rate was 1 second and the requested new sampling
 * rate is 10 ms because the user needs immediate reaction from hotplug
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

	for_each_online_cpu(cpu) {
		struct cpufreq_policy *policy;
		struct cpu_dbs_info_s *dbs_info;
		unsigned long next_sampling, appointed_at;

		policy = cpufreq_cpu_get(cpu);
		if (!policy)
			continue;
		dbs_info = &per_cpu(hp_cpu_dbs_info, policy->cpu);
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

			schedule_delayed_work_on(dbs_info->cpu, &dbs_info->work,
						 usecs_to_jiffies(new_rate));

		}
		mutex_unlock(&dbs_info->timer_mutex);
	}
}

void bl_enable_timer(int enable)
{
	static unsigned int sampling_rate_backup = 0;

	if (enable && !sampling_rate_backup)
		return;

	if (enable)
		update_sampling_rate(sampling_rate_backup);
	else {
		struct cpufreq_policy *policy;
		struct cpu_dbs_info_s *dbs_info;
		unsigned int new_rate = 30000 * 100;  // change to 3s

		/* restore original sampling rate */
		sampling_rate_backup = dbs_tuners_ins.sampling_rate;
		update_sampling_rate(new_rate);

		policy = cpufreq_cpu_get(0);
		if (!policy)
			return;
        
		dbs_info = &per_cpu(hp_cpu_dbs_info, 0);
		cpufreq_cpu_put(policy);

		mutex_lock(&dbs_info->timer_mutex);

		if (!delayed_work_pending(&dbs_info->work)) {
			mutex_unlock(&dbs_info->timer_mutex);
			return;
		}

		mutex_unlock(&dbs_info->timer_mutex);
        
		cancel_delayed_work_sync(&dbs_info->work);

		mutex_lock(&dbs_info->timer_mutex);

		schedule_delayed_work_on(dbs_info->cpu, &dbs_info->work,
						 usecs_to_jiffies(new_rate));

		mutex_unlock(&dbs_info->timer_mutex);
	}
}
EXPORT_SYMBOL(bl_enable_timer);

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

static ssize_t store_od_threshold(struct kobject *a, struct attribute *b,
				  const char *buf, size_t count)
{
	unsigned int input;
	int ret;
	ret = sscanf(buf, "%u", &input);

	if (ret != 1 || input > MAX_FREQUENCY_UP_THRESHOLD ||
			input < MIN_FREQUENCY_UP_THRESHOLD) {
		return -EINVAL;
	}
	dbs_tuners_ins.od_threshold = input;
	return count;
}

static ssize_t store_down_differential(struct kobject *a, struct attribute *b,
                const char *buf, size_t count)
{
	unsigned int input;
	int ret;
	ret = sscanf(buf, "%u", &input);

	if (ret != 1 || input > MAX_FREQUENCY_DOWN_DIFFERENTIAL ||
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
		dbs_info = &per_cpu(hp_cpu_dbs_info, j);
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
		dbs_info = &per_cpu(hp_cpu_dbs_info, j);
		dbs_info->prev_cpu_idle = get_cpu_idle_time(j,
                                                            &dbs_info->prev_cpu_wall,
                                                            dbs_tuners_ins.io_is_busy);
		if (dbs_tuners_ins.ignore_nice)
			dbs_info->prev_cpu_nice = kcpustat_cpu(j).cpustat[CPUTIME_NICE];

	}
	return count;
}

static ssize_t store_powersave_bias(struct kobject *a, struct attribute *b,
				    const char *buf, size_t count)
{
	unsigned int input;
	int ret;
	ret = sscanf(buf, "%u", &input);

	if (ret != 1)
		return -EINVAL;

	if (input > 1000)
		input = 1000;

	dbs_tuners_ins.powersave_bias = input;
	hotplug_powersave_bias_init();
	return count;
}

static ssize_t store_cpu_up_threshold(struct kobject *a, struct attribute *b,
				    const char *buf, size_t count)
{
	unsigned int input;
	int ret;
	ret = sscanf(buf, "%u", &input);

	if (ret != 1 || input > MAX_CPU_UP_THRESHOLD ||
		input < MIN_CPU_UP_THRESHOLD) {
		return -EINVAL;
	}
	dbs_tuners_ins.cpu_up_threshold = input;
	return count;
}

static ssize_t store_cpu_down_differential(struct kobject *a, struct attribute *b,
				    const char *buf, size_t count)
{
	unsigned int input;
	int ret;
	ret = sscanf(buf, "%u", &input);

	if (ret != 1 || input > MAX_CPU_DOWN_DIFFERENTIAL ||
		input < MIN_CPU_DOWN_DIFFERENTIAL) {
		return -EINVAL;
	}
	dbs_tuners_ins.cpu_down_differential = input;
	return count;
}

static ssize_t store_cpu_up_avg_times(struct kobject *a, struct attribute *b,
				    const char *buf, size_t count)
{
	unsigned int input;
	int ret;
	ret = sscanf(buf, "%u", &input);

	dbs_tuners_ins.cpu_up_avg_times = input;
	return count;
}

static ssize_t store_cpu_down_avg_times(struct kobject *a, struct attribute *b,
				    const char *buf, size_t count)
{
	unsigned int input;
	int ret;
	ret = sscanf(buf, "%u", &input);

	dbs_tuners_ins.cpu_down_avg_times = input;
	return count;
}

static ssize_t store_thermal_dispatch_avg_times(struct kobject *a, struct attribute *b,
				    const char *buf, size_t count)
{
	unsigned int input;
	int ret;
	ret = sscanf(buf, "%u", &input);

	dbs_tuners_ins.thermal_dispatch_avg_times = input;
	return count;
}

static ssize_t store_cpu_num_limit(struct kobject *a, struct attribute *b,
				    const char *buf, size_t count)
{
	unsigned int input;
	int ret;
	ret = sscanf(buf, "%u", &input);

	dbs_tuners_ins.cpu_num_limit = input;
	return count;
}

static ssize_t store_cpu_num_base(struct kobject *a, struct attribute *b,
				    const char *buf, size_t count)
{
	unsigned int input;
	bool raise_freq = false;
	int ret;
	struct cpufreq_policy *policy;

	policy = cpufreq_cpu_get(0);
	ret = sscanf(buf, "%u", &input);

	dbs_tuners_ins.cpu_num_base = input;
	mutex_lock(&hp_mutex);
	if (num_online_cpus() < dbs_tuners_ins.cpu_num_base && num_online_cpus() < dbs_tuners_ins.cpu_num_limit) {
		raise_freq = true;
		g_next_hp_action = 1;
		schedule_delayed_work_on(0, &hp_work, 0);
	}
	mutex_unlock(&hp_mutex);

	if(raise_freq == true)
		dbs_freq_increase(policy, policy->max);

	return count;
}

static ssize_t store_is_cpu_hotplug_disable(struct kobject *a, struct attribute *b,
				    const char *buf, size_t count)
{
	unsigned int input;
	int ret;
	ret = sscanf(buf, "%u", &input);

	dbs_tuners_ins.is_cpu_hotplug_disable = input;
	return count;
}

#if INPUT_BOOST
static ssize_t store_cpu_input_boost_enable(struct kobject *a, struct attribute *b,
				    const char *buf, size_t count)
{
	unsigned int input;
	int ret;
	ret = sscanf(buf, "%u", &input);

	if (ret != 1 || input > 1 ||
		input < 0) {
		return -EINVAL;
	}

	mutex_lock(&hp_mutex);
	dbs_tuners_ins.cpu_input_boost_enable = input;
	mutex_unlock(&hp_mutex);

	return count;
}
#endif

define_one_global_rw(sampling_rate);
define_one_global_rw(io_is_busy);
define_one_global_rw(up_threshold);
define_one_global_rw(od_threshold);
define_one_global_rw(down_differential);
define_one_global_rw(sampling_down_factor);
define_one_global_rw(ignore_nice_load);
define_one_global_rw(powersave_bias);
define_one_global_rw(cpu_up_threshold);
define_one_global_rw(cpu_down_differential);
define_one_global_rw(cpu_up_avg_times);
define_one_global_rw(cpu_down_avg_times);
define_one_global_rw(thermal_dispatch_avg_times);
define_one_global_rw(cpu_num_limit);
define_one_global_rw(cpu_num_base);
define_one_global_rw(is_cpu_hotplug_disable);
#if INPUT_BOOST
define_one_global_rw(cpu_input_boost_enable);
#endif

static struct attribute *dbs_attributes[] = {
    &sampling_rate_min.attr,
    &sampling_rate.attr,
    &up_threshold.attr,
    &od_threshold.attr,
    &down_differential.attr,
    &sampling_down_factor.attr,
    &ignore_nice_load.attr,
    &powersave_bias.attr,
    &io_is_busy.attr,
    &cpu_up_threshold.attr,
    &cpu_down_differential.attr,
    &cpu_up_avg_times.attr,
    &cpu_down_avg_times.attr,
    &thermal_dispatch_avg_times.attr,
    &cpu_num_limit.attr,
    &cpu_num_base.attr,
    &is_cpu_hotplug_disable.attr,
#if INPUT_BOOST
    &cpu_input_boost_enable.attr,
#endif
    NULL
};

static struct attribute_group dbs_attr_group = {
	.attrs = dbs_attributes,
	.name = "hotplug",
};

/************************** sysfs end ************************/

static void dbs_freq_increase(struct cpufreq_policy *p, unsigned int freq)
{
	if (dbs_tuners_ins.powersave_bias)
		freq = powersave_bias_target(p, freq, CPUFREQ_RELATION_H);
	else if (p->cur == p->max)
	{
		if (dbs_ignore == 0)
			dbs_ignore = 1;
		else
			return;
	}

	__cpufreq_driver_target(p, freq, dbs_tuners_ins.powersave_bias ?
			CPUFREQ_RELATION_L : CPUFREQ_RELATION_H);
}

int mt_cpufreq_cur_load(void)
{
    return cpu_loading;
}
EXPORT_SYMBOL(mt_cpufreq_cur_load);

void hp_set_dynamic_cpu_hotplug_enable(int enable)
{
	mutex_lock(&hp_mutex);
	dbs_tuners_ins.is_cpu_hotplug_disable = !enable;
	mutex_unlock(&hp_mutex);
}
EXPORT_SYMBOL(hp_set_dynamic_cpu_hotplug_enable);

void hp_limited_cpu_num(int num)
{
	mutex_lock(&hp_mutex);
	dbs_tuners_ins.cpu_num_limit = num;

	if (num < num_online_cpus()) {
		printk("%s: CPU off due to thermal protection! limit_num = %d < online = %d\n", 
                    __func__, num, num_online_cpus());
		g_next_hp_action = 0;
		schedule_delayed_work_on(0, &hp_work, 0);
		g_cpu_down_count = 0;
		g_cpu_down_sum_load = 0;
	}
    
	mutex_unlock(&hp_mutex);
}
EXPORT_SYMBOL(hp_limited_cpu_num);
void hp_based_cpu_num(int num)
{
	mutex_lock(&hp_mutex);
	dbs_tuners_ins.cpu_num_base = num;
	mutex_unlock(&hp_mutex);
}
EXPORT_SYMBOL(hp_based_cpu_num);

#ifdef CONFIG_SMP

static void hp_work_handler(struct work_struct *work)
{
	if (mutex_trylock(&bl_onoff_mutex))
	{
		if (!dbs_tuners_ins.is_cpu_hotplug_disable)
		{
			int onlines_cpu_n = num_online_cpus();

			if (g_next_hp_action) // turn on CPU
			{
				if (onlines_cpu_n < num_possible_cpus())
				{
					printk("hp_work_handler: cpu_up(%d) kick off\n", onlines_cpu_n);
					cpu_up(onlines_cpu_n);
					printk("hp_work_handler: cpu_up(%d) completion\n", onlines_cpu_n);

					dbs_ignore = 0; // force trigger frequency scaling
				}
			}
			else // turn off CPU
			{
				if (onlines_cpu_n > 1)
				{
					printk("hp_work_handler: cpu_down(%d) kick off\n", (onlines_cpu_n - 1));
					cpu_down((onlines_cpu_n - 1));
					printk("hp_work_handler: cpu_down(%d) completion\n", (onlines_cpu_n - 1));

					dbs_ignore = 0; // force trigger frequency scaling
				}
			}
		}
		mutex_unlock(&bl_onoff_mutex);
	}
}

#endif

static void dbs_check_cpu(struct cpu_dbs_info_s *this_dbs_info)
{
	unsigned int max_load_freq;
    bool raise_freq = false;

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
	cpus_sum_load = 0;

	for_each_cpu(j, policy->cpus) {
		struct cpu_dbs_info_s *j_dbs_info;
		cputime64_t cur_wall_time, cur_idle_time, cur_iowait_time;
		unsigned int idle_time, wall_time, iowait_time;
		unsigned int load, load_freq;
		int freq_avg;

		j_dbs_info = &per_cpu(hp_cpu_dbs_info, j);

		cur_idle_time = get_cpu_idle_time(j, &cur_wall_time,
                                                  dbs_tuners_ins.io_is_busy);
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
		 * For the purpose of hotplug, waiting for disk IO is an
		 * indication that you're performance critical, and not that
		 * the system is actually idle. So subtract the iowait time
		 * from the cpu idle time.
		 */

		if (dbs_tuners_ins.io_is_busy && idle_time >= iowait_time)
			idle_time -= iowait_time;

		if (unlikely(!wall_time || wall_time < idle_time))
			continue;

		load = 100 * (wall_time - idle_time) / wall_time;

		cpus_sum_load += load;

		freq_avg = __cpufreq_driver_getavg(policy, j);
		if (freq_avg <= 0)
			freq_avg = policy->cur;

		load_freq = load * freq_avg;
		if (load_freq > max_load_freq)
			max_load_freq = load_freq;

		#ifdef DEBUG_LOG
		printk("dbs_check_cpu: cpu = %d\n", j);
		printk("dbs_check_cpu: wall_time = %d, idle_time = %d, load = %d\n", wall_time, idle_time, load);
		printk("dbs_check_cpu: freq_avg = %d, max_load_freq = %d, cpus_sum_load = %d\n", freq_avg, max_load_freq, cpus_sum_load);
		#endif
	}
    // record loading information
    cpu_loading = max_load_freq / policy->cur;
    // dispatch power budget
    if(g_thermal_count >= dbs_tuners_ins.thermal_dispatch_avg_times) {
        g_thermal_count = 0;
        mt_dvfs_power_dispatch_safe();
        if ((dbs_thermal_limited == 1) && (policy->cur > dbs_thermal_limited_freq))
    		__cpufreq_driver_target(policy, dbs_thermal_limited_freq, CPUFREQ_RELATION_L);
    }
    else
        g_thermal_count++;

    if (policy->cur >= get_normal_max_freq()){
        if ((max_load_freq > dbs_tuners_ins.od_threshold * policy->cur) && (num_online_cpus() == num_possible_cpus())){
            g_max_cpu_persist_count++;
		    #ifdef DEBUG_LOG
            printk("dvfs_od: g_max_cpu_persist_count: %d\n", g_max_cpu_persist_count);
    		#endif
            if(g_max_cpu_persist_count == DEF_CPU_PERSIST_COUNT){
                //only ramp up to OD OPP here
		        #ifdef DEBUG_LOG
		        printk("dvfs_od: cpu loading = %d\n", max_load_freq/policy->cur);
	    	    #endif
                if (policy->cur < policy->max)
                    this_dbs_info->rate_mult =
                        dbs_tuners_ins.sampling_down_factor;
                dbs_freq_increase(policy, policy->max);
		        #ifdef DEBUG_LOG
                printk("reset g_max_cpu_persist_count, count = 10\n");
    		    #endif
                g_max_cpu_persist_count = 0;
                goto hp_check;
            }
        }
        else {
            g_max_cpu_persist_count = 0;
        }
    }
    else{
        if (max_load_freq > dbs_tuners_ins.up_threshold * policy->cur) {
    		/* If switching to max speed, apply sampling_down_factor */
	    	if (policy->cur < get_normal_max_freq())
		    	this_dbs_info->rate_mult =
			    	dbs_tuners_ins.sampling_down_factor;
    		dbs_freq_increase(policy, get_normal_max_freq());
            if(g_max_cpu_persist_count != 0){
                g_max_cpu_persist_count = 0;
    		    #ifdef DEBUG_LOG
                printk("reset g_max_cpu_persist_count, and fallback to normal max\n");
                #endif
            }
	    	goto hp_check;
       }
    }

	/* Check for frequency decrease */
	/* if we cannot reduce the frequency anymore, break out early */
	if (policy->cur == policy->min)
		goto hp_check;

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

        if(g_max_cpu_persist_count != 0){
            g_max_cpu_persist_count = 0;
		    #ifdef DEBUG_LOG
            printk("reset g_max_cpu_persist_count, decrease freq accrording to loading\n");
            #endif
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

hp_check:

	/* If Hot Plug policy disable, return directly */
	if (dbs_tuners_ins.is_cpu_hotplug_disable)
		return;

	#ifdef CONFIG_SMP
	mutex_lock(&hp_mutex);

	/* Check CPU loading to power up slave CPU */
	if (num_online_cpus() < dbs_tuners_ins.cpu_num_base && num_online_cpus() < dbs_tuners_ins.cpu_num_limit) {
		raise_freq = true;
		printk("dbs_check_cpu: turn on CPU by perf service\n");
		g_next_hp_action = 1;
		schedule_delayed_work_on(0, &hp_work, 0);
	} else if (num_online_cpus() < num_possible_cpus() && num_online_cpus() < dbs_tuners_ins.cpu_num_limit) {
		g_cpu_up_count++;
		g_cpu_up_sum_load += cpus_sum_load;
		if (g_cpu_up_count == dbs_tuners_ins.cpu_up_avg_times) {
			g_cpu_up_sum_load /= dbs_tuners_ins.cpu_up_avg_times;
			if (g_cpu_up_sum_load >
				(dbs_tuners_ins.cpu_up_threshold * num_online_cpus())) {
				#ifdef DEBUG_LOG
				printk("dbs_check_cpu: g_cpu_up_sum_load = %d\n", g_cpu_up_sum_load);
				#endif
				raise_freq = true;
				printk("dbs_check_cpu: turn on CPU\n");
				g_next_hp_action = 1;
				schedule_delayed_work_on(0, &hp_work, 0);
			}
			g_cpu_up_count = 0;
			g_cpu_up_sum_load = 0;
		}
		#ifdef DEBUG_LOG
		printk("dbs_check_cpu: g_cpu_up_count = %d, g_cpu_up_sum_load = %d\n", g_cpu_up_count, g_cpu_up_sum_load);
		printk("dbs_check_cpu: cpu_up_threshold = %d\n", (dbs_tuners_ins.cpu_up_threshold * num_online_cpus()));
		#endif
	}

	/* Check CPU loading to power down slave CPU */
	if (num_online_cpus() > 1) {
		g_cpu_down_count++;
		g_cpu_down_sum_load += cpus_sum_load;
		if (g_cpu_down_count == dbs_tuners_ins.cpu_down_avg_times) {
			g_cpu_down_sum_load /= dbs_tuners_ins.cpu_down_avg_times;
			if (g_cpu_down_sum_load <
				((dbs_tuners_ins.cpu_up_threshold - dbs_tuners_ins.cpu_down_differential) * (num_online_cpus() - 1))) {
				if (num_online_cpus() > dbs_tuners_ins.cpu_num_base) {
				#ifdef DEBUG_LOG
				printk("dbs_check_cpu: g_cpu_down_sum_load = %d\n", g_cpu_down_sum_load);
				#endif
				raise_freq = true;
				printk("dbs_check_cpu: turn off CPU\n");
				g_next_hp_action = 0;
				schedule_delayed_work_on(0, &hp_work, 0);
				}
			}
			g_cpu_down_count = 0;
			g_cpu_down_sum_load = 0;
		}
		#ifdef DEBUG_LOG
		printk("dbs_check_cpu: g_cpu_down_count = %d, g_cpu_down_sum_load = %d\n", g_cpu_down_count, g_cpu_down_sum_load);
		printk("dbs_check_cpu: cpu_down_threshold = %d\n", ((dbs_tuners_ins.cpu_up_threshold - dbs_tuners_ins.cpu_down_differential) * (num_online_cpus() - 1)));
		#endif
	}

	mutex_unlock(&hp_mutex);
	#endif
	// need to retrieve dbs_freq_increase out of hp_mutex
	// in case of self-deadlock
	if(raise_freq == true)
		dbs_freq_increase(policy, policy->max);

	return;
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
			/* We want all CPUs to do sampling nearly on
			 * same jiffy
			 */
			delay = usecs_to_jiffies(dbs_tuners_ins.sampling_rate
				* dbs_info->rate_mult);

			if (num_online_cpus() > 1)
				delay -= jiffies % delay;
		}
	} else {
		__cpufreq_driver_target(dbs_info->cur_policy,
			dbs_info->freq_lo, CPUFREQ_RELATION_H);
		delay = dbs_info->freq_lo_jiffies;
	}
	schedule_delayed_work_on(cpu, &dbs_info->work, delay);
	mutex_unlock(&dbs_info->timer_mutex);
}

static inline void dbs_timer_init(struct cpu_dbs_info_s *dbs_info)
{
	/* We want all CPUs to do sampling nearly on same jiffy */
	int delay = usecs_to_jiffies(dbs_tuners_ins.sampling_rate);

	if (num_online_cpus() > 1)
		delay -= jiffies % delay;

	dbs_info->sample_type = DBS_NORMAL_SAMPLE;
	INIT_DELAYED_WORK(&dbs_info->work, do_dbs_timer);
	schedule_delayed_work_on(dbs_info->cpu, &dbs_info->work, delay);
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
	return 1; // io wait time should be subtracted from idle time
}

#if INPUT_BOOST
static void dbs_input_event(struct input_handle *handle, unsigned int type,
		unsigned int code, int value)
{
	if ((type == EV_KEY) && (code == BTN_TOUCH) && (value == 1) && (dbs_tuners_ins.cpu_input_boost_enable))
	{
		force_two_core();
	}
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
	handle->name = "cpufreq_balance";

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
        {
                .flags = INPUT_DEVICE_ID_MATCH_EVBIT |
                         INPUT_DEVICE_ID_MATCH_ABSBIT,
                .evbit = { BIT_MASK(EV_ABS) },
                .absbit = { [BIT_WORD(ABS_MT_POSITION_X)] =
                            BIT_MASK(ABS_MT_POSITION_X) |
                            BIT_MASK(ABS_MT_POSITION_Y) },
        }, /* multi-touch touchscreen */
        {
                .flags = INPUT_DEVICE_ID_MATCH_KEYBIT |
                         INPUT_DEVICE_ID_MATCH_ABSBIT,
                .keybit = { [BIT_WORD(BTN_TOUCH)] = BIT_MASK(BTN_TOUCH) },
                .absbit = { [BIT_WORD(ABS_X)] =
                            BIT_MASK(ABS_X) | BIT_MASK(ABS_Y) },
        }, /* touchpad */
        { },
};

static struct input_handler dbs_input_handler = {
	.event		= dbs_input_event,
	.connect	= dbs_input_connect,
	.disconnect	= dbs_input_disconnect,
	.name		= "cpufreq_balance",
	.id_table	= dbs_ids,
};
#endif //#ifdef CONFIG_HOTPLUG_CPU



static int cpufreq_governor_dbs(struct cpufreq_policy *policy,
				   unsigned int event)
{
	unsigned int cpu = policy->cpu;
	struct cpu_dbs_info_s *this_dbs_info;
	unsigned int j;
	int rc;

	this_dbs_info = &per_cpu(hp_cpu_dbs_info, cpu);

	switch (event) {
	case CPUFREQ_GOV_START:
		if ((!cpu_online(cpu)) || (!policy->cur))
			return -EINVAL;

		mutex_lock(&dbs_mutex);

		dbs_enable++;
		for_each_cpu(j, policy->cpus) {
			struct cpu_dbs_info_s *j_dbs_info;
			j_dbs_info = &per_cpu(hp_cpu_dbs_info, j);
			j_dbs_info->cur_policy = policy;

			j_dbs_info->prev_cpu_idle = get_cpu_idle_time(j,
                                                                      &j_dbs_info->prev_cpu_wall,
                                                                      dbs_tuners_ins.io_is_busy);

			if (dbs_tuners_ins.ignore_nice)
				j_dbs_info->prev_cpu_nice =
						kcpustat_cpu(j).cpustat[CPUTIME_NICE];
		}
		this_dbs_info->cpu = cpu;
		this_dbs_info->rate_mult = 1;
		hotplug_powersave_bias_init_cpu(cpu);
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

			#ifdef DEBUG_LOG
			printk("cpufreq_governor_dbs: min_sampling_rate = %d\n", min_sampling_rate);
			printk("cpufreq_governor_dbs: dbs_tuners_ins.sampling_rate = %d\n", dbs_tuners_ins.sampling_rate);
			printk("cpufreq_governor_dbs: dbs_tuners_ins.io_is_busy = %d\n", dbs_tuners_ins.io_is_busy);
			#endif
		}
#if INPUT_BOOST
		if (!cpu)
			rc = input_register_handler(&dbs_input_handler);
#endif
		mutex_unlock(&dbs_mutex);

		mutex_init(&this_dbs_info->timer_mutex);
		dbs_timer_init(this_dbs_info);
		break;

	case CPUFREQ_GOV_STOP:
		dbs_timer_exit(this_dbs_info);

		mutex_lock(&dbs_mutex);
		mutex_destroy(&this_dbs_info->timer_mutex);
		dbs_enable--;
#if INPUT_BOOST
		if (!cpu)
			input_unregister_handler(&dbs_input_handler);

#endif
		mutex_unlock(&dbs_mutex);
		if (!dbs_enable)
			sysfs_remove_group(cpufreq_global_kobject,
					   &dbs_attr_group);

		break;

	case CPUFREQ_GOV_LIMITS:
		mutex_lock(&this_dbs_info->timer_mutex);
		if (get_normal_max_freq() < this_dbs_info->cur_policy->cur)
			__cpufreq_driver_target(this_dbs_info->cur_policy,
				get_normal_max_freq(), CPUFREQ_RELATION_H);
		else if (policy->min > this_dbs_info->cur_policy->cur)
			__cpufreq_driver_target(this_dbs_info->cur_policy,
				policy->min, CPUFREQ_RELATION_L);
		mutex_unlock(&this_dbs_info->timer_mutex);
		break;
	}
	return 0;
}

/*int cpufreq_gov_dbs_get_sum_load(void)
{
	return cpus_sum_load;
}*/

#if INPUT_BOOST
static int touch_freq_up_task(void *data)
{
	struct cpufreq_policy *policy;

	while (1) {
		policy = cpufreq_cpu_get(0);
                if(policy != NULL)
		{
                    dbs_freq_increase(policy, policy->max);
                    cpufreq_cpu_put(policy);
                }
		set_current_state(TASK_INTERRUPTIBLE);
		schedule();

		if (kthread_should_stop())
			break;
	}

	return 0;
}
#endif

static int __init cpufreq_gov_dbs_init(void)
{
	u64 idle_time;
	int cpu = get_cpu();

	#if INPUT_BOOST
	struct sched_param param = { .sched_priority = MAX_RT_PRIO-1 };
	#endif

	idle_time = get_cpu_idle_time_us(cpu, NULL);
	put_cpu();
	if (idle_time != -1ULL) {
		/* Idle micro accounting is supported. Use finer thresholds */
		dbs_tuners_ins.up_threshold = MICRO_FREQUENCY_UP_THRESHOLD;
		dbs_tuners_ins.down_differential =
					MICRO_FREQUENCY_DOWN_DIFFERENTIAL;
		dbs_tuners_ins.cpu_up_threshold =
					MICRO_CPU_UP_THRESHOLD;
		dbs_tuners_ins.cpu_down_differential =
					MICRO_CPU_DOWN_DIFFERENTIAL;
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

	dbs_tuners_ins.cpu_num_limit = num_possible_cpus();
	dbs_tuners_ins.cpu_num_base = 1;

	if (dbs_tuners_ins.cpu_num_limit > 1)
		dbs_tuners_ins.is_cpu_hotplug_disable = 0;

	#ifdef CONFIG_SMP
	INIT_DELAYED_WORK(&hp_work, hp_work_handler);
	#endif


	#if INPUT_BOOST
	freq_up_task = kthread_create(touch_freq_up_task, NULL,
		"touch_freq_up_task");
	if (IS_ERR(freq_up_task))
		return PTR_ERR(freq_up_task);

	sched_setscheduler_nocheck(freq_up_task, SCHED_FIFO, &param);
	get_task_struct(freq_up_task);
	#endif

	#ifdef DEBUG_LOG
	printk("cpufreq_gov_dbs_init: min_sampling_rate = %d\n", min_sampling_rate);
	printk("cpufreq_gov_dbs_init: dbs_tuners_ins.up_threshold = %d\n", dbs_tuners_ins.up_threshold);
	printk("cpufreq_gov_dbs_init: dbs_tuners_ins.od_threshold = %d\n", dbs_tuners_ins.od_threshold);
	printk("cpufreq_gov_dbs_init: dbs_tuners_ins.down_differential = %d\n", dbs_tuners_ins.down_differential);
	printk("cpufreq_gov_dbs_init: dbs_tuners_ins.cpu_up_threshold = %d\n", dbs_tuners_ins.cpu_up_threshold);
	printk("cpufreq_gov_dbs_init: dbs_tuners_ins.cpu_down_differential = %d\n", dbs_tuners_ins.cpu_down_differential);
	printk("cpufreq_gov_dbs_init: dbs_tuners_ins.cpu_up_avg_times = %d\n", dbs_tuners_ins.cpu_up_avg_times);
	printk("cpufreq_gov_dbs_init: dbs_tuners_ins.cpu_down_avg_times = %d\n", dbs_tuners_ins.cpu_down_avg_times);
	printk("cpufreq_gov_dbs_init: dbs_tuners_ins.thermal_di_avg_times = %d\n", dbs_tuners_ins.thermal_dispatch_avg_times);
	printk("cpufreq_gov_dbs_init: dbs_tuners_ins.cpu_num_limit = %d\n", dbs_tuners_ins.cpu_num_limit);
	printk("cpufreq_gov_dbs_init: dbs_tuners_ins.cpu_num_base = %d\n", dbs_tuners_ins.cpu_num_base);
	printk("cpufreq_gov_dbs_init: dbs_tuners_ins.is_cpu_hotplug_disable = %d\n", dbs_tuners_ins.is_cpu_hotplug_disable);
	#if INPUT_BOOST
	printk("cpufreq_gov_dbs_init: dbs_tuners_ins.cpu_input_boost_enable = %d\n", dbs_tuners_ins.cpu_input_boost_enable);
	#endif /* INPUT_BOOST */
	#endif /* DEBUG_LOG */

	return cpufreq_register_governor(&cpufreq_gov_balance);
}

static void __exit cpufreq_gov_dbs_exit(void)
{
	#ifdef CONFIG_SMP
	cancel_delayed_work_sync(&hp_work);
	#endif

	cpufreq_unregister_governor(&cpufreq_gov_balance);

	#if INPUT_BOOST
	kthread_stop(freq_up_task);
	put_task_struct(freq_up_task);
	#endif
}


MODULE_AUTHOR("Venkatesh Pallipadi <venkatesh.pallipadi@intel.com>");
MODULE_AUTHOR("Alexey Starikovskiy <alexey.y.starikovskiy@intel.com>");
MODULE_DESCRIPTION("'cpufreq_balance' - A dynamic cpufreq governor for "
	"Low Latency Frequency Transition capable processors");
MODULE_LICENSE("GPL");

#ifdef CONFIG_CPU_FREQ_DEFAULT_GOV_BALANCE
fs_initcall(cpufreq_gov_dbs_init);
#else
module_init(cpufreq_gov_dbs_init);
#endif
module_exit(cpufreq_gov_dbs_exit);
