/*
 * linux/drivers/cpufreq/cpufreq_lionfish.c
 *
 * Copyright (C) 2015 Sultan Qasim Khan <sultanqasim@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#include <linux/types.h>
#include <linux/slab.h>
#include <linux/kernel_stat.h>
#include "cpufreq_governor.h"

/* Lionfish governor tunable defaults */
#define DEF_FREQUENCY_JUMP_THRESHOLD	(95)
#define DEF_FREQUENCY_UP_THRESHOLD	(80)
#define DEF_FREQUENCY_DOWN_THRESHOLD	(40)
#define DEF_FREQUENCY_JUMP_LEVEL	(800000)
#define DEF_SAMPLING_UP_FACTOR		(2)
#define DEF_SAMPLING_DOWN_FACTOR	(3)

/* Lionfish governor fixed settings */
#define RAMP_UP_PERCENTAGE		(130)
#define RAMP_DOWN_PERCENTAGE		(65)
#define FREQUENCY_STEP_PERCENTAGE	(5)
#define JUMP_HISPEED_FREQ_PERCENTAGE	(83)
#define MAX_SAMPLING_FACTOR		(10)

#define LIONFISH_VERSION_MAJOR		(1)
#define LIONFISH_VERSION_MINOR		(1)

/************************** type definitions ****************************/
struct lf_cpu_dbs_info_s {
	struct cpu_dbs_common_info cdbs;
	unsigned int up_ticks;
	unsigned int down_ticks;
	unsigned int requested_freq;
	unsigned int enable:1;
	unsigned int prev_load;
};

struct lf_dbs_tuners {
	unsigned int ignore_nice_load;
	unsigned int sampling_rate;
	unsigned int sampling_up_factor;
	unsigned int sampling_down_factor;
	unsigned int jump_threshold;
	unsigned int up_threshold;
	unsigned int down_threshold;
	unsigned int jump_level;
};

struct lf_gdbs_data;

/* Common Governor data across policies */
struct lf_dbs_data {
	/*
	 * governor sysfs attributes
	 * for system governor and per policy governor
	 */
	struct attribute_group *attr_group_gov_sys;
	struct attribute_group *attr_group_gov_pol;

	/*
	 * Common data for platforms that don't set
	 * CPUFREQ_HAVE_GOVERNOR_PER_POLICY
	 */
	struct lf_gdbs_data *gdbs_data;

	/* handles frequency change notifications */
	struct notifier_block *notifier_block;
};

/* Governor per policy data */
struct lf_gdbs_data {
	struct lf_dbs_data *cdata;
	unsigned int min_sampling_rate;
	int usage_count;
	struct lf_dbs_tuners *tuners;

	/* dbs_mutex protects dbs_enable in governor start/stop */
	struct mutex mutex;
};

/************************ function declarations *************************/
static void lf_gov_queue_work(struct lf_gdbs_data *dbs_data,
		struct cpufreq_policy *policy, unsigned int delay, bool all_cpus);

/************************** sysfs macro magic ***************************/
#define lf_show_one(_gov, file_name)					\
static ssize_t show_##file_name##_gov_sys				\
(struct kobject *kobj, struct attribute *attr, char *buf)		\
{									\
	struct _gov##_dbs_tuners *tuners = _gov##_dbs_cdata.gdbs_data->tuners; \
	return sprintf(buf, "%u\n", tuners->file_name);			\
}									\
									\
static ssize_t show_##file_name##_gov_pol				\
(struct cpufreq_policy *policy, char *buf)				\
{									\
	struct lf_gdbs_data *dbs_data = policy->governor_data;		\
	struct _gov##_dbs_tuners *tuners = dbs_data->tuners;		\
	return sprintf(buf, "%u\n", tuners->file_name);			\
}

#define lf_store_one(_gov, file_name)					\
static ssize_t store_##file_name##_gov_sys				\
(struct kobject *kobj, struct attribute *attr, const char *buf, size_t count) \
{									\
	struct lf_gdbs_data *dbs_data = _gov##_dbs_cdata.gdbs_data;	\
	return store_##file_name(dbs_data, buf, count);			\
}									\
									\
static ssize_t store_##file_name##_gov_pol				\
(struct cpufreq_policy *policy, const char *buf, size_t count)		\
{									\
	struct lf_gdbs_data *dbs_data = policy->governor_data;		\
	return store_##file_name(dbs_data, buf, count);			\
}

#define lf_show_store_one(_gov, file_name)				\
lf_show_one(_gov, file_name);						\
lf_store_one(_gov, file_name)

#define lf_declare_show_sampling_rate_min(_gov)				\
static ssize_t show_sampling_rate_min_gov_sys				\
(struct kobject *kobj, struct attribute *attr, char *buf)		\
{									\
	struct lf_gdbs_data *dbs_data = _gov##_dbs_cdata.gdbs_data;	\
	return sprintf(buf, "%u\n", dbs_data->min_sampling_rate);	\
}									\
									\
static ssize_t show_sampling_rate_min_gov_pol				\
(struct cpufreq_policy *policy, char *buf)				\
{									\
	struct lf_gdbs_data *dbs_data = policy->governor_data;		\
	return sprintf(buf, "%u\n", dbs_data->min_sampling_rate);	\
}

/************************** global variables ****************************/
static DEFINE_PER_CPU(struct lf_cpu_dbs_info_s, lf_cpu_dbs_info);
static struct lf_dbs_data lf_dbs_cdata;
/* there are more globals declared after the sysfs code */

/*********************** lionfish governor logic ************************/
static inline unsigned int get_freq_target(struct lf_dbs_tuners *lf_tuners,
					   struct cpufreq_policy *policy)
{
	unsigned int freq_target =
		(FREQUENCY_STEP_PERCENTAGE * policy->max) / 100;

	/* max freq cannot be less than 100. But who knows... */
	if (unlikely(freq_target == 0))
		freq_target = FREQUENCY_STEP_PERCENTAGE;

	return freq_target;
}

/* Computes the maximum absolute load for the policy  */
static unsigned int lf_get_load(cpumask_var_t cpus, unsigned int sampling_rate,
	unsigned int ignore_nice_load, struct cpu_dbs_common_info *cdbs)
{
	unsigned int j;
	unsigned int load = 0;

	for_each_cpu(j, cpus) {
		struct cpu_dbs_common_info *j_cdbs;
		u64 cur_wall_time, cur_idle_time;
		unsigned int idle_time, wall_time;
		unsigned int cpu_load;

		j_cdbs = &per_cpu(lf_cpu_dbs_info, j).cdbs;

		/* last parameter 0 means that IO wait is considered idle */
		cur_idle_time = get_cpu_idle_time(j, &cur_wall_time, 0);

		if (cur_wall_time < j_cdbs->prev_cpu_wall + sampling_rate) {
			cpu_load = per_cpu(lf_cpu_dbs_info, j).prev_load;
		} else {
			wall_time = (unsigned int)
				(cur_wall_time - j_cdbs->prev_cpu_wall);
			j_cdbs->prev_cpu_wall = cur_wall_time;

			idle_time = (unsigned int)
				(cur_idle_time - j_cdbs->prev_cpu_idle);
			j_cdbs->prev_cpu_idle = cur_idle_time;

			if (ignore_nice_load) {
				u64 cur_nice;
				unsigned long cur_nice_jiffies;

				cur_nice = kcpustat_cpu(j).cpustat[CPUTIME_NICE] -
						cdbs->prev_cpu_nice;
				/*
				* Assumption: nice time between sampling periods
				* will be less than 2^32 jiffies for 32 bit sys
				*/
				cur_nice_jiffies = (unsigned long)
						cputime64_to_jiffies64(cur_nice);

				cdbs->prev_cpu_nice =
					kcpustat_cpu(j).cpustat[CPUTIME_NICE];
				idle_time += jiffies_to_usecs(cur_nice_jiffies);
			}

			if (unlikely(!wall_time || wall_time < idle_time))
				continue;

			cpu_load = 100 * (wall_time - idle_time) / wall_time;
			per_cpu(lf_cpu_dbs_info, j).prev_load = cpu_load;
		}

		if (cpu_load > load)
			load = cpu_load;
	}

	return load;
}

/*
 * This function is the heart of the governor. It is called periodically
 * for each CPU core.
 *
 * This governor uses two different approaches to control the CPU frequency.
 * It jumps quickly to a target frequency under very heavy load, and it votes
 * to gradually ramp up or down the frequency under moderate load.
 *
 * When the CPU is near saturation (load above jump_threshold, 95% by default),
 * the goveror will quickly jump to a higher frequency and skip intermediate
 * levels for fast response. When saturated while the frequency is below
 * jump_level (800 MHz by default), the CPU jumps to jump_level. When saturated
 * at or above this frequency, the governor jumps to a large fraction of the
 * full CPU speed (83% as presently defined). It does not jump straight to the
 * full frequency because performance per watt at full speed is usually
 * significantly worse than performance per watt slightly below it.
 *
 * Under more moderate loads, during each sample period, the governor votes to
 * increase or decrease CPU frequency, aiming to maintain load between
 * up_threshold and down_threshold. If the load is within the desired range,
 * no votes are cast and the vote counters are decremented by one to indicate
 * that the current frequency is good. When enough votes (sampling_up_factor
 * or sampling_down_factor) are cast to raise/lower the frequency, the frequency
 * will be ramped up or down by an amount proportional to the current frequency.
 */
static void lf_check_cpu(struct lf_gdbs_data *dbs_data, int cpu)
{
	struct lf_cpu_dbs_info_s *dbs_info = &per_cpu(lf_cpu_dbs_info, cpu);
	struct cpu_dbs_common_info *cdbs = &per_cpu(lf_cpu_dbs_info, cpu).cdbs;
	struct cpufreq_policy *policy = dbs_info->cdbs.cur_policy;
	struct lf_dbs_tuners *lf_tuners = dbs_data->tuners;
	unsigned int freq_shift;
	unsigned int hispeed;
	unsigned int new_frequency;
	unsigned int load;
	bool voted = false;

	/* compute the maximum absolute load */
	load = lf_get_load(policy->cpus, lf_tuners->sampling_rate,
		lf_tuners->ignore_nice_load, cdbs);

	freq_shift = get_freq_target(lf_tuners, policy);
	hispeed = policy->max * JUMP_HISPEED_FREQ_PERCENTAGE / 100;
	if (unlikely(hispeed < policy->min))
		hispeed = policy->min;

	/* Check for frequency jump */
	if ((load > lf_tuners->jump_threshold) &&
			(dbs_info->requested_freq < hispeed )) {
		/* if we are already at full speed then break out early */
		if (policy->cur == policy->max)
			return;

		if (dbs_info->requested_freq + freq_shift < lf_tuners->jump_level) {
			dbs_info->requested_freq = lf_tuners->jump_level;
			if (unlikely(dbs_info->requested_freq > policy->max))
				dbs_info->requested_freq = policy->max;
		} else {
			dbs_info->requested_freq = hispeed;
		}

		__cpufreq_driver_target(policy, dbs_info->requested_freq,
			CPUFREQ_RELATION_H);
		dbs_info->up_ticks = dbs_info->down_ticks = 0;
		return;
	}

	/* Check for frequency increase */
	if (load > lf_tuners->up_threshold) {
		/* if we are already at full speed then break out early */
		if (dbs_info->requested_freq >= policy->max)
			return;

		/* vote to raise the frequency */
		dbs_info->up_ticks += 1;
		dbs_info->down_ticks = 0;
		voted = true;
	}

	/* Check for frequency decrease */
	if (load < lf_tuners->down_threshold) {
		/* if we cannot reduce the frequency anymore, break out early */
		if (policy->cur <= policy->min)
			return;

		/* vote to lower the frequency */
		dbs_info->down_ticks += 1;
		dbs_info->up_ticks = 0;
		voted = true;
	}

	if (!voted)
	{
	    if (dbs_info->down_ticks) dbs_info->down_ticks -= 1;
	    if (dbs_info->up_ticks) dbs_info->up_ticks -= 1;
	}

	/* update the frequency if enough votes to change */
	if (dbs_info->up_ticks >= lf_tuners->sampling_up_factor) {
		new_frequency = policy->cur * RAMP_UP_PERCENTAGE / 100;
		if (new_frequency < dbs_info->requested_freq + freq_shift)
			new_frequency = dbs_info->requested_freq + freq_shift;
		if (new_frequency > policy->max)
			new_frequency = policy->max;

		dbs_info->requested_freq = new_frequency;

		__cpufreq_driver_target(policy, dbs_info->requested_freq,
			CPUFREQ_RELATION_H);
		dbs_info->up_ticks = dbs_info->down_ticks = 0;
	} else if (dbs_info->down_ticks >= lf_tuners->sampling_down_factor) {
		new_frequency = policy->cur * RAMP_DOWN_PERCENTAGE / 100;
		if (new_frequency > dbs_info->requested_freq - freq_shift)
			new_frequency = dbs_info->requested_freq - freq_shift;
		if (new_frequency < policy->min)
			new_frequency = policy->min;

		dbs_info->requested_freq = new_frequency;

		__cpufreq_driver_target(policy, dbs_info->requested_freq,
			CPUFREQ_RELATION_L);
		dbs_info->up_ticks = dbs_info->down_ticks = 0;
	}
}

static void lf_dbs_timer(struct work_struct *work)
{
	struct lf_cpu_dbs_info_s *dbs_info = container_of(work,
			struct lf_cpu_dbs_info_s, cdbs.work.work);
	unsigned int cpu = dbs_info->cdbs.cur_policy->cpu;
	struct lf_cpu_dbs_info_s *core_dbs_info = &per_cpu(lf_cpu_dbs_info,
			cpu);
	struct lf_gdbs_data *dbs_data = dbs_info->cdbs.cur_policy->governor_data;
	struct lf_dbs_tuners *lf_tuners = dbs_data->tuners;
	int delay = delay_for_sampling_rate(lf_tuners->sampling_rate);
	bool modify_all = true;

	mutex_lock(&core_dbs_info->cdbs.timer_mutex);
	if (!need_load_eval(&core_dbs_info->cdbs, lf_tuners->sampling_rate))
		modify_all = false;
	else
		lf_check_cpu(dbs_data, cpu);

	lf_gov_queue_work(dbs_data, dbs_info->cdbs.cur_policy, delay, modify_all);
	mutex_unlock(&core_dbs_info->cdbs.timer_mutex);
}

static int dbs_cpufreq_notifier(struct notifier_block *nb, unsigned long val,
		void *data)
{
	struct cpufreq_freqs *freq = data;
	struct lf_cpu_dbs_info_s *dbs_info =
					&per_cpu(lf_cpu_dbs_info, freq->cpu);
	struct cpufreq_policy *policy;

	if (!dbs_info->enable)
		return 0;

	policy = dbs_info->cdbs.cur_policy;

	/*
	 * we only care if our internally tracked freq moves outside the 'valid'
	 * ranges of frequency available to us otherwise we do not change it
	*/
	if (dbs_info->requested_freq > policy->max
			|| dbs_info->requested_freq < policy->min)
		dbs_info->requested_freq = freq->new;

	return 0;
}

/*************************** sysfs interface ****************************/
static ssize_t store_sampling_up_factor(struct lf_gdbs_data *dbs_data,
		const char *buf, size_t count)
{
	struct lf_dbs_tuners *lf_tuners = dbs_data->tuners;
	unsigned int input;
	int ret;
	ret = sscanf(buf, "%u", &input);

	if (ret != 1 || input > MAX_SAMPLING_FACTOR || input < 1)
		return -EINVAL;

	lf_tuners->sampling_up_factor = input;
	return count;
}

static ssize_t store_sampling_down_factor(struct lf_gdbs_data *dbs_data,
		const char *buf, size_t count)
{
	struct lf_dbs_tuners *lf_tuners = dbs_data->tuners;
	unsigned int input;
	int ret;
	ret = sscanf(buf, "%u", &input);

	if (ret != 1 || input > MAX_SAMPLING_FACTOR || input < 1)
		return -EINVAL;

	lf_tuners->sampling_down_factor = input;
	return count;
}

static ssize_t store_sampling_rate(struct lf_gdbs_data *dbs_data, const char *buf,
		size_t count)
{
	struct lf_dbs_tuners *lf_tuners = dbs_data->tuners;
	unsigned int input;
	int ret;
	ret = sscanf(buf, "%u", &input);

	if (ret != 1)
		return -EINVAL;

	lf_tuners->sampling_rate = max(input, dbs_data->min_sampling_rate);
	return count;
}

static ssize_t store_jump_threshold(struct lf_gdbs_data *dbs_data, const char *buf,
		size_t count)
{
	struct lf_dbs_tuners *lf_tuners = dbs_data->tuners;
	unsigned int input;
	int ret;
	ret = sscanf(buf, "%u", &input);

	if (ret != 1 || input > 100 || input <= lf_tuners->down_threshold)
		return -EINVAL;

	lf_tuners->jump_threshold = input;
	return count;
}

static ssize_t store_up_threshold(struct lf_gdbs_data *dbs_data, const char *buf,
		size_t count)
{
	struct lf_dbs_tuners *lf_tuners = dbs_data->tuners;
	unsigned int input;
	int ret;
	ret = sscanf(buf, "%u", &input);

	if (ret != 1 || input > 100 || input <= lf_tuners->down_threshold)
		return -EINVAL;

	lf_tuners->up_threshold = input;
	return count;
}

static ssize_t store_down_threshold(struct lf_gdbs_data *dbs_data, const char *buf,
		size_t count)
{
	struct lf_dbs_tuners *lf_tuners = dbs_data->tuners;
	unsigned int input;
	int ret;
	ret = sscanf(buf, "%u", &input);

	/* cannot be lower than 11 otherwise freq will not fall */
	if (ret != 1 || input < 11 || input > 100 ||
			input >= lf_tuners->up_threshold)
		return -EINVAL;

	lf_tuners->down_threshold = input;
	return count;
}

static ssize_t store_jump_level(struct lf_gdbs_data *dbs_data, const char *buf,
		size_t count)
{
	struct lf_dbs_tuners *lf_tuners = dbs_data->tuners;
	unsigned int input;
	int ret;
	ret = sscanf(buf, "%u", &input);

	/* jump level is a frequency in kHz */
	if (ret != 1 || input < 80000 || input > 5000000)
		return -EINVAL;

	lf_tuners->jump_level = input;
	return count;
}

static ssize_t store_ignore_nice_load(struct lf_gdbs_data *dbs_data,
		const char *buf, size_t count)
{
	struct lf_dbs_tuners *lf_tuners = dbs_data->tuners;
	unsigned int input, j;
	int ret;

	ret = sscanf(buf, "%u", &input);
	if (ret != 1)
		return -EINVAL;

	if (input > 1)
		input = 1;

	if (input == lf_tuners->ignore_nice_load) /* nothing to do */
		return count;

	lf_tuners->ignore_nice_load = input;

	/* we need to re-evaluate prev_cpu_idle */
	for_each_online_cpu(j) {
		struct lf_cpu_dbs_info_s *dbs_info;
		dbs_info = &per_cpu(lf_cpu_dbs_info, j);
		dbs_info->cdbs.prev_cpu_idle = get_cpu_idle_time(j,
					&dbs_info->cdbs.prev_cpu_wall, 0);
		if (lf_tuners->ignore_nice_load)
			dbs_info->cdbs.prev_cpu_nice =
				kcpustat_cpu(j).cpustat[CPUTIME_NICE];
	}
	return count;
}

/* macros to autogenerate sysfs interface */
lf_show_store_one(lf, sampling_rate);
lf_show_store_one(lf, sampling_up_factor);
lf_show_store_one(lf, sampling_down_factor);
lf_show_store_one(lf, jump_threshold);
lf_show_store_one(lf, up_threshold);
lf_show_store_one(lf, down_threshold);
lf_show_store_one(lf, jump_level);
lf_show_store_one(lf, ignore_nice_load);
lf_declare_show_sampling_rate_min(lf);

gov_sys_pol_attr_rw(sampling_rate);
gov_sys_pol_attr_rw(sampling_up_factor);
gov_sys_pol_attr_rw(sampling_down_factor);
gov_sys_pol_attr_rw(jump_threshold);
gov_sys_pol_attr_rw(up_threshold);
gov_sys_pol_attr_rw(down_threshold);
gov_sys_pol_attr_rw(jump_level);
gov_sys_pol_attr_rw(ignore_nice_load);
gov_sys_pol_attr_ro(sampling_rate_min);

/********** globals requiring sysfs functions to be defined *************/
static struct attribute *dbs_attributes_gov_sys[] = {
	&sampling_rate_min_gov_sys.attr,
	&sampling_rate_gov_sys.attr,
	&sampling_up_factor_gov_sys.attr,
	&sampling_down_factor_gov_sys.attr,
	&jump_threshold_gov_sys.attr,
	&up_threshold_gov_sys.attr,
	&down_threshold_gov_sys.attr,
	&jump_level_gov_sys.attr,
	&ignore_nice_load_gov_sys.attr,
	NULL
};

static struct attribute_group lf_attr_group_gov_sys = {
	.attrs = dbs_attributes_gov_sys,
	.name = "lionfish",
};

static struct attribute *dbs_attributes_gov_pol[] = {
	&sampling_rate_min_gov_pol.attr,
	&sampling_rate_gov_pol.attr,
	&sampling_up_factor_gov_pol.attr,
	&sampling_down_factor_gov_pol.attr,
	&jump_threshold_gov_pol.attr,
	&up_threshold_gov_pol.attr,
	&down_threshold_gov_pol.attr,
	&jump_level_gov_pol.attr,
	&ignore_nice_load_gov_pol.attr,
	NULL
};

static struct attribute_group lf_attr_group_gov_pol = {
	.attrs = dbs_attributes_gov_pol,
	.name = "lionfish",
};

static struct notifier_block lf_cpufreq_notifier_block = {
	.notifier_call = dbs_cpufreq_notifier,
};

static struct lf_dbs_data lf_dbs_cdata = {
	.attr_group_gov_sys = &lf_attr_group_gov_sys,
	.attr_group_gov_pol = &lf_attr_group_gov_pol,
	.notifier_block = &lf_cpufreq_notifier_block,
};

/*************************** boilerplate code ***************************/
static struct attribute_group *get_sysfs_attr(struct lf_gdbs_data *dbs_data)
{
	if (have_governor_per_policy())
		return dbs_data->cdata->attr_group_gov_pol;
	else
		return dbs_data->cdata->attr_group_gov_sys;
}

static inline void __lf_gov_queue_work(int cpu, struct lf_gdbs_data *dbs_data,
		unsigned int delay)
{
	struct cpu_dbs_common_info *cdbs = &per_cpu(lf_cpu_dbs_info, cpu).cdbs;

	mod_delayed_work_on(cpu, system_wq, &cdbs->work, delay);
}

static void lf_gov_queue_work(struct lf_gdbs_data *dbs_data,
		struct cpufreq_policy *policy, unsigned int delay, bool all_cpus)
{
	int i;

	if (!policy->governor_enabled)
		return;

	if (!all_cpus) {
		/*
		 * Use raw_smp_processor_id() to avoid preemptible warnings.
		 * We know that this is only called with all_cpus == false from
		 * works that have been queued with *_work_on() functions and
		 * those works are canceled during CPU_DOWN_PREPARE so they
		 * can't possibly run on any other CPU.
		 */
		__lf_gov_queue_work(raw_smp_processor_id(), dbs_data, delay);
	} else {
		for_each_cpu(i, policy->cpus)
			__lf_gov_queue_work(i, dbs_data, delay);
	}
}

static inline void lf_gov_cancel_work(struct lf_gdbs_data *dbs_data,
		struct cpufreq_policy *policy)
{
	struct cpu_dbs_common_info *cdbs;
	int i;

	for_each_cpu(i, policy->cpus) {
		cdbs = &per_cpu(lf_cpu_dbs_info, i).cdbs;
		cancel_delayed_work_sync(&cdbs->work);
	}
}

static int lf_init(struct lf_gdbs_data *dbs_data)
{
	struct lf_dbs_tuners *tuners;

	tuners = kzalloc(sizeof(*tuners), GFP_KERNEL);
	if (!tuners) {
		pr_err("%s: kzalloc failed\n", __func__);
		return -ENOMEM;
	}

	tuners->jump_threshold = DEF_FREQUENCY_JUMP_THRESHOLD;
	tuners->up_threshold = DEF_FREQUENCY_UP_THRESHOLD;
	tuners->down_threshold = DEF_FREQUENCY_DOWN_THRESHOLD;
	tuners->jump_level = DEF_FREQUENCY_JUMP_LEVEL;
	tuners->sampling_up_factor = DEF_SAMPLING_UP_FACTOR;
	tuners->sampling_down_factor = DEF_SAMPLING_DOWN_FACTOR;
	tuners->ignore_nice_load = 0;

	dbs_data->tuners = tuners;

	/*
	 * you want at least 8 jiffies between sample intervals for the
	 * CPU usage stats to be reasonable
	 */
	dbs_data->min_sampling_rate = jiffies_to_usecs(8);

	mutex_init(&dbs_data->mutex);
	return 0;
}

static void lf_exit(struct lf_gdbs_data *dbs_data)
{
	kfree(dbs_data->tuners);
}

static int lf_cpufreq_governor_dbs(struct cpufreq_policy *policy,
				   unsigned int event)
{
	struct lf_dbs_data *cdata = &lf_dbs_cdata;
	struct lf_gdbs_data *dbs_data;
	struct lf_cpu_dbs_info_s *lf_dbs_info = NULL;
	struct lf_dbs_tuners *lf_tuners = NULL;
	struct cpu_dbs_common_info *cpu_cdbs;
	unsigned int sampling_rate, latency, ignore_nice, j, cpu = policy->cpu;
	int rc;

	if (have_governor_per_policy())
		dbs_data = policy->governor_data;
	else
		dbs_data = cdata->gdbs_data;

	WARN_ON(!dbs_data && (event != CPUFREQ_GOV_POLICY_INIT));

	switch (event) {
	case CPUFREQ_GOV_POLICY_INIT:
		if (have_governor_per_policy()) {
			WARN_ON(dbs_data);
		} else if (dbs_data) {
			dbs_data->usage_count++;
			policy->governor_data = dbs_data;
			return 0;
		}

		dbs_data = kzalloc(sizeof(*dbs_data), GFP_KERNEL);
		if (!dbs_data) {
			pr_err("%s: POLICY_INIT: kzalloc failed\n", __func__);
			return -ENOMEM;
		}

		dbs_data->cdata = cdata;
		dbs_data->usage_count = 1;
		rc = lf_init(dbs_data);
		if (rc) {
			pr_err("%s: POLICY_INIT: init() failed\n", __func__);
			kfree(dbs_data);
			return rc;
		}

		rc = sysfs_create_group(get_governor_parent_kobj(policy),
				get_sysfs_attr(dbs_data));
		if (rc) {
			lf_exit(dbs_data);
			kfree(dbs_data);
			return rc;
		}

		policy->governor_data = dbs_data;

		/* policy latency is in ns. Convert it to us first */
		latency = policy->cpuinfo.transition_latency / 1000;
		if (latency == 0)
			latency = 1;

		/*
		 * The minimum sampling rate should be at least 1000x the
		 * CPU frequency transition latency. We compare this
		 * requirement with the governor's minimum sampling rate
		 * and use whichever is greater. By default, we will sample
		 * at half the fastest acceptable rate.
		 */
		dbs_data->min_sampling_rate = max(dbs_data->min_sampling_rate,
				LATENCY_MULTIPLIER * latency);
		dbs_data->tuners->sampling_rate = dbs_data->min_sampling_rate * 2;

		if (!policy->governor->initialized) {
			cpufreq_register_notifier(dbs_data->cdata->notifier_block,
					CPUFREQ_TRANSITION_NOTIFIER);
		}

		if (!have_governor_per_policy())
			cdata->gdbs_data = dbs_data;

		return 0;

	case CPUFREQ_GOV_POLICY_EXIT:
		if (!--dbs_data->usage_count) {
			sysfs_remove_group(get_governor_parent_kobj(policy),
					get_sysfs_attr(dbs_data));

			if (policy->governor->initialized == 1) {
				cpufreq_unregister_notifier(dbs_data->cdata->notifier_block,
						CPUFREQ_TRANSITION_NOTIFIER);
			}

			lf_exit(dbs_data);
			kfree(dbs_data);
			cdata->gdbs_data = NULL;
		}

		policy->governor_data = NULL;
		return 0;

	case CPUFREQ_GOV_START:
		if (!policy->cur)
			return -EINVAL;

		cpu_cdbs = &per_cpu(lf_cpu_dbs_info, cpu).cdbs;
		lf_tuners = dbs_data->tuners;
		lf_dbs_info = &per_cpu(lf_cpu_dbs_info, cpu);
		sampling_rate = lf_tuners->sampling_rate;
		ignore_nice = lf_tuners->ignore_nice_load;

		mutex_lock(&dbs_data->mutex);

		for_each_cpu(j, policy->cpus) {
			struct cpu_dbs_common_info *j_cdbs =
				&per_cpu(lf_cpu_dbs_info, j).cdbs;

			j_cdbs->cpu = j;
			j_cdbs->cur_policy = policy;
			j_cdbs->prev_cpu_idle = get_cpu_idle_time(j,
					       &j_cdbs->prev_cpu_wall, 0);
			if (ignore_nice)
				j_cdbs->prev_cpu_nice =
					kcpustat_cpu(j).cpustat[CPUTIME_NICE];

			mutex_init(&j_cdbs->timer_mutex);
			INIT_DEFERRABLE_WORK(&j_cdbs->work, lf_dbs_timer);
		}

		lf_dbs_info->up_ticks = 0;
		lf_dbs_info->down_ticks = 0;
		lf_dbs_info->enable = 1;
		lf_dbs_info->requested_freq = policy->cur;

		mutex_unlock(&dbs_data->mutex);

		/* Initiate timer time stamp */
		cpu_cdbs->time_stamp = ktime_get();

		lf_gov_queue_work(dbs_data, policy,
				delay_for_sampling_rate(sampling_rate), true);
		break;

	case CPUFREQ_GOV_STOP:
		cpu_cdbs = &per_cpu(lf_cpu_dbs_info, cpu).cdbs;
		lf_dbs_info = &per_cpu(lf_cpu_dbs_info, cpu);
		lf_dbs_info->enable = 0;

		lf_gov_cancel_work(dbs_data, policy);

		mutex_lock(&dbs_data->mutex);
		mutex_destroy(&cpu_cdbs->timer_mutex);
		cpu_cdbs->cur_policy = NULL;

		mutex_unlock(&dbs_data->mutex);

		break;

	case CPUFREQ_GOV_LIMITS:
		cpu_cdbs = &per_cpu(lf_cpu_dbs_info, cpu).cdbs;
		mutex_lock(&cpu_cdbs->timer_mutex);
		if (policy->max < cpu_cdbs->cur_policy->cur)
			__cpufreq_driver_target(cpu_cdbs->cur_policy,
					policy->max, CPUFREQ_RELATION_H);
		else if (policy->min > cpu_cdbs->cur_policy->cur)
			__cpufreq_driver_target(cpu_cdbs->cur_policy,
					policy->min, CPUFREQ_RELATION_L);
		lf_check_cpu(dbs_data, cpu);
		mutex_unlock(&cpu_cdbs->timer_mutex);
		break;
	default:
		break;
	}

	return 0;
}

#ifndef CONFIG_CPU_FREQ_DEFAULT_GOV_LIONFISH
static
#endif
struct cpufreq_governor cpufreq_gov_lionfish = {
	.name			= "lionfish",
	.governor		= lf_cpufreq_governor_dbs,
	.max_transition_latency	= TRANSITION_LATENCY_LIMIT,
	.owner			= THIS_MODULE,
};

static int __init cpufreq_gov_dbs_init(void)
{
	return cpufreq_register_governor(&cpufreq_gov_lionfish);
}

static void __exit cpufreq_gov_dbs_exit(void)
{
	cpufreq_unregister_governor(&cpufreq_gov_lionfish);
}

MODULE_AUTHOR("Sultan Qasim Khan <sultanqasim@gmail.com>");
MODULE_DESCRIPTION("'cpufreq_lionfish' - A dynamic cpufreq governor for mobile "
		"devices designed to keep CPU frequencies to a minimum while "
		"still briefly boosting frequencies as required to avoid lag."
);
MODULE_LICENSE("GPL");

#ifdef CONFIG_CPU_FREQ_DEFAULT_GOV_LIONFISH
fs_initcall(cpufreq_gov_dbs_init);
#else
module_init(cpufreq_gov_dbs_init);
#endif
module_exit(cpufreq_gov_dbs_exit);
