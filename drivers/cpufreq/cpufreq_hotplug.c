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

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/cpufreq.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/kernel_stat.h>
#include <linux/kobject.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/percpu-defs.h>
#include <linux/slab.h>
#include <linux/sysfs.h>
#include <linux/tick.h>
#include <linux/types.h>
#include <linux/cpu.h>
#include <linux/sched.h>
#include <linux/sched/rt.h>
#include <linux/kthread.h>
#include <linux/input.h>	/* <-XXX */
#include <linux/slab.h>		/* <-XXX */
#include "mach/mt_cpufreq.h"	/* <-XXX */

#include "cpufreq_governor.h"

/* Hot-plug governor macros */
#define DEF_FREQUENCY_DOWN_DIFFERENTIAL     (10)
#define DEF_FREQUENCY_UP_THRESHOLD          (80)
#define DEF_SAMPLING_DOWN_FACTOR            (1)
#define MAX_SAMPLING_DOWN_FACTOR            (100000)
#define MICRO_FREQUENCY_DOWN_DIFFERENTIAL   (15)
#define MIN_FREQUENCY_DOWN_DIFFERENTIAL     (5)	/* <-XXX */
#define MAX_FREQUENCY_DOWN_DIFFERENTIAL     (20)	/* <-XXX */
#define MICRO_FREQUENCY_UP_THRESHOLD        (85)
#ifdef CONFIG_MTK_SDIOAUTOK_SUPPORT
#define MICRO_FREQUENCY_MIN_SAMPLE_RATE     (27000)
#else
#define MICRO_FREQUENCY_MIN_SAMPLE_RATE     (30000)
#endif
#define MIN_FREQUENCY_UP_THRESHOLD          (21)
#define MAX_FREQUENCY_UP_THRESHOLD          (100)

/* <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<< */
/*
 * cpu hotplug - macro
 */
#define DEF_CPU_DOWN_DIFFERENTIAL           (10)
#define MICRO_CPU_DOWN_DIFFERENTIAL         (10)
#define MIN_CPU_DOWN_DIFFERENTIAL           (0)
#define MAX_CPU_DOWN_DIFFERENTIAL           (30)

#define DEF_CPU_UP_THRESHOLD                (90)
#define MICRO_CPU_UP_THRESHOLD              (90)
#define MIN_CPU_UP_THRESHOLD                (50)
#define MAX_CPU_UP_THRESHOLD                (100)

#define DEF_CPU_UP_AVG_TIMES                (10)
#define MIN_CPU_UP_AVG_TIMES                (1)
#define MAX_CPU_UP_AVG_TIMES                (20)

#define DEF_CPU_DOWN_AVG_TIMES              (100)
#define MIN_CPU_DOWN_AVG_TIMES              (20)
#define MAX_CPU_DOWN_AVG_TIMES              (200)

#define DEF_CPU_INPUT_BOOST_ENABLE          (1)
#define DEF_CPU_INPUT_BOOST_NUM             (2)

#define DEF_CPU_RUSH_BOOST_ENABLE           (1)

#define DEF_CPU_RUSH_THRESHOLD              (98)
#define MICRO_CPU_RUSH_THRESHOLD            (98)
#define MIN_CPU_RUSH_THRESHOLD              (80)
#define MAX_CPU_RUSH_THRESHOLD              (100)

#define DEF_CPU_RUSH_AVG_TIMES              (5)
#define MIN_CPU_RUSH_AVG_TIMES              (1)
#define MAX_CPU_RUSH_AVG_TIMES              (10)

#define DEF_CPU_RUSH_TLP_TIMES              (5)
#define MIN_CPU_RUSH_TLP_TIMES              (1)
#define MAX_CPU_RUSH_TLP_TIMES              (10)

/* #define DEBUG_LOG */

/*
 * cpu hotplug - enum
 */
typedef enum {
	CPU_HOTPLUG_WORK_TYPE_NONE = 0,
	CPU_HOTPLUG_WORK_TYPE_BASE,
	CPU_HOTPLUG_WORK_TYPE_LIMIT,
	CPU_HOTPLUG_WORK_TYPE_UP,
	CPU_HOTPLUG_WORK_TYPE_DOWN,
	CPU_HOTPLUG_WORK_TYPE_RUSH,
} cpu_hotplug_work_type_t;

/*
 * cpu hotplug - global variable, function declaration
 */
static DEFINE_MUTEX(hp_mutex);
DEFINE_MUTEX(hp_onoff_mutex);

int g_cpus_sum_load_current = 0;	/* set global for information purpose */
#ifdef CONFIG_HOTPLUG_CPU

static long g_cpu_up_sum_load;
static int g_cpu_up_count;
static int g_cpu_up_load_index;
static long g_cpu_up_load_history[MAX_CPU_UP_AVG_TIMES] = { 0 };

static long g_cpu_down_sum_load;
static int g_cpu_down_count;
static int g_cpu_down_load_index;
static long g_cpu_down_load_history[MAX_CPU_DOWN_AVG_TIMES] = { 0 };

static cpu_hotplug_work_type_t g_trigger_hp_work;
static unsigned int g_next_hp_action;
static struct delayed_work hp_work;
struct workqueue_struct *hp_wq = NULL;

static int g_tlp_avg_current;	/* set global for information purpose */
static int g_tlp_avg_sum;
static int g_tlp_avg_count;
static int g_tlp_avg_index;
static int g_tlp_avg_average;	/* set global for information purpose */
static int g_tlp_avg_history[MAX_CPU_RUSH_TLP_TIMES] = { 0 };

static int g_tlp_iowait_av;

static int g_cpu_rush_count;

static void hp_reset_strategy_nolock(void);
static void hp_reset_strategy(void);

#else				/* #ifdef CONFIG_HOTPLUG_CPU */

static void hp_reset_strategy_nolock(void)
{
};

#endif				/* #ifdef CONFIG_HOTPLUG_CPU */

/* dvfs - function declaration */
static void dbs_freq_increase(struct cpufreq_policy *p, unsigned int freq);

#if defined(CONFIG_THERMAL_LIMIT_TEST)
extern unsigned int mt_cpufreq_thermal_test_limited_load(void);
#endif

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


void (*cpufreq_freq_check) (enum mt_cpu_dvfs_id id) = NULL;
/* >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>> */

static DEFINE_PER_CPU(struct hp_cpu_dbs_info_s, hp_cpu_dbs_info);

static struct hp_ops hp_ops;

#ifndef CONFIG_CPU_FREQ_DEFAULT_GOV_HOTPLUG
static struct cpufreq_governor cpufreq_gov_hotplug;
#endif

static unsigned int default_powersave_bias;

static void hotplug_powersave_bias_init_cpu(int cpu)
{
	struct hp_cpu_dbs_info_s *dbs_info = &per_cpu(hp_cpu_dbs_info, cpu);

	dbs_info->freq_table = cpufreq_frequency_get_table(cpu);
	dbs_info->freq_lo = 0;
}

/*
 * Not all CPUs want IO time to be accounted as busy; this depends on how
 * efficient idling at a higher frequency/voltage is.
 * Pavel Machek says this is not so for various generations of AMD and old
 * Intel systems.
 * Mike Chan (android.com) claims this is also not true for ARM.
 * Because of this, whitelist specific known (series) of CPUs by default, and
 * leave all others up to the user.
 */
static int should_io_be_busy(void)
{
#if defined(CONFIG_X86)
	/*
	 * For Intel, Core 2 (model 15) and later have an efficient idle.
	 */
	if (boot_cpu_data.x86_vendor == X86_VENDOR_INTEL &&
	    boot_cpu_data.x86 == 6 && boot_cpu_data.x86_model >= 15)
		return 1;
#endif
	return 1;		/* io wait time should be subtracted from idle time // <-XXX */
}

/*
 * Find right freq to be set now with powersave_bias on.
 * Returns the freq_hi to be used right now and will set freq_hi_jiffies,
 * freq_lo, and freq_lo_jiffies in percpu area for averaging freqs.
 */
static unsigned int generic_powersave_bias_target(struct cpufreq_policy *policy,
						  unsigned int freq_next, unsigned int relation)
{
	unsigned int freq_req, freq_reduc, freq_avg;
	unsigned int freq_hi, freq_lo;
	unsigned int index = 0;
	unsigned int jiffies_total, jiffies_hi, jiffies_lo;
	struct hp_cpu_dbs_info_s *dbs_info = &per_cpu(hp_cpu_dbs_info,
						      policy->cpu);
	struct dbs_data *dbs_data = policy->governor_data;
	struct hp_dbs_tuners *hp_tuners = dbs_data->tuners;

	if (!dbs_info->freq_table) {
		dbs_info->freq_lo = 0;
		dbs_info->freq_lo_jiffies = 0;
		return freq_next;
	}

	cpufreq_frequency_table_target(policy, dbs_info->freq_table, freq_next, relation, &index);
	freq_req = dbs_info->freq_table[index].frequency;
	freq_reduc = freq_req * hp_tuners->powersave_bias / 1000;
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
	jiffies_total = usecs_to_jiffies(hp_tuners->sampling_rate);
	jiffies_hi = (freq_avg - freq_lo) * jiffies_total;
	jiffies_hi += ((freq_hi - freq_lo) / 2);
	jiffies_hi /= (freq_hi - freq_lo);
	jiffies_lo = jiffies_total - jiffies_hi;
	dbs_info->freq_lo = freq_lo;
	dbs_info->freq_lo_jiffies = jiffies_lo;
	dbs_info->freq_hi_jiffies = jiffies_hi;
	return freq_hi;
}

static void hotplug_powersave_bias_init(void)
{
	int i;
	for_each_online_cpu(i) {
		hotplug_powersave_bias_init_cpu(i);
	}
}

static void dbs_freq_increase(struct cpufreq_policy *p, unsigned int freq)
{
	struct dbs_data *dbs_data = p->governor_data;
	struct hp_dbs_tuners *hp_tuners = dbs_data->tuners;

	if (hp_tuners->powersave_bias)
		freq = hp_ops.powersave_bias_target(p, freq, CPUFREQ_RELATION_H);
	else if (p->cur == p->max) {
/* <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<< */
		if (dbs_ignore == 0) {
			if ((dbs_thermal_limited == 1) && (freq > dbs_thermal_limited_freq)) {
				freq = dbs_thermal_limited_freq;
				pr_debug("[dbs_freq_increase] thermal limit freq = %d\n", freq);
			}

			dbs_ignore = 1;
		} else
/* >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>> */
			return;
	}

	__cpufreq_driver_target(p, freq, hp_tuners->powersave_bias ?
				CPUFREQ_RELATION_L : CPUFREQ_RELATION_H);
}

/* <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<< */
/*
 * cpu hotplug - function definition
 */
int hp_get_dynamic_cpu_hotplug_enable(void)
{
	struct dbs_data *dbs_data = per_cpu(hp_cpu_dbs_info, 0).cdbs.cur_policy->governor_data;	/* TODO: FIXME, cpu = 0 */
	struct hp_dbs_tuners *hp_tuners;

	if (!dbs_data)
		return 0;
	hp_tuners = dbs_data->tuners;
	if (!hp_tuners)
		return 0;

	return !(hp_tuners->is_cpu_hotplug_disable);
}
EXPORT_SYMBOL(hp_get_dynamic_cpu_hotplug_enable);

void hp_set_dynamic_cpu_hotplug_enable(int enable)
{
	struct dbs_data *dbs_data = per_cpu(hp_cpu_dbs_info, 0).cdbs.cur_policy->governor_data;	/* TODO: FIXME, cpu = 0 */
	struct hp_dbs_tuners *hp_tuners;

	if (!dbs_data)
		return;
	hp_tuners = dbs_data->tuners;
	if (!hp_tuners)
		return;

	if (enable > 1 || enable < 0)
		return;

	mutex_lock(&hp_mutex);

	if (hp_tuners->is_cpu_hotplug_disable && enable)
		hp_reset_strategy_nolock();

	hp_tuners->is_cpu_hotplug_disable = !enable;
	mutex_unlock(&hp_mutex);
}
EXPORT_SYMBOL(hp_set_dynamic_cpu_hotplug_enable);

void hp_limited_cpu_num(int num)
{
	struct dbs_data *dbs_data = per_cpu(hp_cpu_dbs_info, 0).cdbs.cur_policy->governor_data;	/* TODO: FIXME, cpu = 0 */
	struct hp_dbs_tuners *hp_tuners;

	if (!dbs_data)
		return;
	hp_tuners = dbs_data->tuners;
	if (!hp_tuners)
		return;

	if (num > num_possible_cpus() || num < 1)
		return;

	mutex_lock(&hp_mutex);
	hp_tuners->cpu_num_limit = num;
	mutex_unlock(&hp_mutex);
}
EXPORT_SYMBOL(hp_limited_cpu_num);

void hp_based_cpu_num(int num)
{
	unsigned int online_cpus_count;
	struct dbs_data *dbs_data = per_cpu(hp_cpu_dbs_info, 0).cdbs.cur_policy->governor_data;	/* TODO: FIXME, cpu = 0 */
	struct hp_dbs_tuners *hp_tuners;

	if (!dbs_data)
		return;
	hp_tuners = dbs_data->tuners;
	if (!hp_tuners)
		return;

	if (num > num_possible_cpus() || num < 1)
		return;

	mutex_lock(&hp_mutex);

	hp_tuners->cpu_num_base = num;
	online_cpus_count = num_online_cpus();
#ifdef CONFIG_HOTPLUG_CPU

	if (online_cpus_count < num && online_cpus_count < hp_tuners->cpu_num_limit) {
		struct hp_cpu_dbs_info_s *dbs_info;
		struct cpufreq_policy *policy;

		dbs_info = &per_cpu(hp_cpu_dbs_info, 0);	/* TODO: FIXME, cpu = 0 */
		policy = dbs_info->cdbs.cur_policy;

		dbs_freq_increase(policy, policy->max);
		g_trigger_hp_work = CPU_HOTPLUG_WORK_TYPE_BASE;
		/* schedule_delayed_work_on(0, &hp_work, 0); */
		if (hp_wq == NULL)
			pr_emerg("[power/hotplug] %s():%d, impossible\n", __func__, __LINE__);
		else
			queue_delayed_work_on(0, hp_wq, &hp_work, 0);
	}
#endif

	mutex_unlock(&hp_mutex);
}
EXPORT_SYMBOL(hp_based_cpu_num);

int hp_get_cpu_rush_boost_enable(void)
{
	struct dbs_data *dbs_data = per_cpu(hp_cpu_dbs_info, 0).cdbs.cur_policy->governor_data;	/* TODO: FIXME, cpu = 0 */
	struct hp_dbs_tuners *hp_tuners;

	if (!dbs_data)
		return 0;
	hp_tuners = dbs_data->tuners;
	if (!hp_tuners)
		return 0;

	return hp_tuners->cpu_rush_boost_enable;
}
EXPORT_SYMBOL(hp_get_cpu_rush_boost_enable);

void hp_set_cpu_rush_boost_enable(int enable)
{
	struct dbs_data *dbs_data = per_cpu(hp_cpu_dbs_info, 0).cdbs.cur_policy->governor_data;	/* TODO: FIXME, cpu = 0 */
	struct hp_dbs_tuners *hp_tuners;

	if (!dbs_data)
		return;
	hp_tuners = dbs_data->tuners;
	if (!hp_tuners)
		return;

	if (enable > 1 || enable < 0)
		return;

	mutex_lock(&hp_mutex);
	hp_tuners->cpu_rush_boost_enable = enable;
	mutex_unlock(&hp_mutex);
}
EXPORT_SYMBOL(hp_set_cpu_rush_boost_enable);

#ifdef CONFIG_HOTPLUG_CPU

#ifdef CONFIG_MTK_SCHED_RQAVG_KS
extern void sched_get_nr_running_avg(int *avg, int *iowait_avg);
#else				/* #ifdef CONFIG_MTK_SCHED_RQAVG_KS */
static void sched_get_nr_running_avg(int *avg, int *iowait_avg)
{
	*avg = num_possible_cpus() * 100;
}
#endif				/* #ifdef CONFIG_MTK_SCHED_RQAVG_KS */

static void hp_reset_strategy_nolock(void)
{
	struct dbs_data *dbs_data = per_cpu(hp_cpu_dbs_info, 0).cdbs.cur_policy->governor_data;	/* TODO: FIXME, cpu = 0 */
	struct hp_dbs_tuners *hp_tuners;

	if (!dbs_data)
		return;
	hp_tuners = dbs_data->tuners;
	if (!hp_tuners)
		return;

	g_cpu_up_count = 0;
	g_cpu_up_sum_load = 0;
	g_cpu_up_load_index = 0;
	g_cpu_up_load_history[hp_tuners->cpu_up_avg_times - 1] = 0;
	/* memset(g_cpu_up_load_history, 0, sizeof(long) * MAX_CPU_UP_AVG_TIMES); */

	g_cpu_down_count = 0;
	g_cpu_down_sum_load = 0;
	g_cpu_down_load_index = 0;
	g_cpu_down_load_history[hp_tuners->cpu_down_avg_times - 1] = 0;
	/* memset(g_cpu_down_load_history, 0, sizeof(long) * MAX_CPU_DOWN_AVG_TIMES); */

	g_tlp_avg_sum = 0;
	g_tlp_avg_count = 0;
	g_tlp_avg_index = 0;
	g_tlp_avg_history[hp_tuners->cpu_rush_tlp_times - 1] = 0;
	g_cpu_rush_count = 0;

	g_trigger_hp_work = CPU_HOTPLUG_WORK_TYPE_NONE;
}

static void hp_reset_strategy(void)
{
	mutex_lock(&hp_mutex);

	hp_reset_strategy_nolock();

	mutex_unlock(&hp_mutex);
}

static void hp_work_handler(struct work_struct *work)
{
	struct dbs_data *dbs_data = per_cpu(hp_cpu_dbs_info, 0).cdbs.cur_policy->governor_data;	/* TODO: FIXME, cpu = 0 */
	struct hp_dbs_tuners *hp_tuners;

	if (!dbs_data)
		return;
	hp_tuners = dbs_data->tuners;
	if (!hp_tuners)
		return;

	if (mutex_trylock(&hp_onoff_mutex)) {
		if (!hp_tuners->is_cpu_hotplug_disable) {
			unsigned int online_cpus_count = num_online_cpus();
			unsigned int i;

			pr_debug
			    ("[power/hotplug] hp_work_handler(%d)(%d)(%d)(%d)(%ld)(%ld)(%d)(%d) begin\n",
			     g_trigger_hp_work, g_tlp_avg_average, g_tlp_avg_current,
			     g_cpus_sum_load_current, g_cpu_up_sum_load, g_cpu_down_sum_load,
			     hp_tuners->cpu_num_base, hp_tuners->cpu_num_limit);

			switch (g_trigger_hp_work) {
			case CPU_HOTPLUG_WORK_TYPE_RUSH:
				for (i = online_cpus_count;
				     i < min(g_next_hp_action, hp_tuners->cpu_num_limit); ++i)
					cpu_up(i);

				break;

			case CPU_HOTPLUG_WORK_TYPE_BASE:
				for (i = online_cpus_count;
				     i < min(hp_tuners->cpu_num_base, hp_tuners->cpu_num_limit);
				     ++i)
					cpu_up(i);

				break;

			case CPU_HOTPLUG_WORK_TYPE_LIMIT:
				for (i = online_cpus_count - 1; i >= hp_tuners->cpu_num_limit; --i)
					cpu_down(i);

				break;

			case CPU_HOTPLUG_WORK_TYPE_UP:
				for (i = online_cpus_count; i < g_next_hp_action; ++i)
					cpu_up(i);

				break;

			case CPU_HOTPLUG_WORK_TYPE_DOWN:
				for (i = online_cpus_count - 1; i >= g_next_hp_action; --i)
					cpu_down(i);

				break;

			default:
				for (i = online_cpus_count;
				     i < min(hp_tuners->cpu_input_boost_num,
					     hp_tuners->cpu_num_limit); ++i)
					cpu_up(i);

				/* pr_debug("[power/hotplug] cpu input boost\n"); */
				break;
			}

			hp_reset_strategy();
			dbs_ignore = 0;	/* force trigger frequency scaling */

			pr_debug("[power/hotplug] hp_work_handler end\n");

			/*
			   if (g_next_hp_action) // turn on CPU
			   {
			   if (online_cpus_count < num_possible_cpus())
			   {
			   pr_debug("hp_work_handler: cpu_up(%d) kick off\n", online_cpus_count);
			   cpu_up(online_cpus_count);
			   hp_reset_strategy();
			   pr_debug("hp_work_handler: cpu_up(%d) completion\n", online_cpus_count);

			   dbs_ignore = 0; // force trigger frequency scaling
			   }
			   }
			   else // turn off CPU
			   {
			   if (online_cpus_count > 1)
			   {
			   pr_debug("hp_work_handler: cpu_down(%d) kick off\n", (online_cpus_count - 1));
			   cpu_down((online_cpus_count - 1));
			   hp_reset_strategy();
			   pr_debug("hp_work_handler: cpu_down(%d) completion\n", (online_cpus_count - 1));

			   dbs_ignore = 0; // force trigger frequency scaling
			   }
			   }
			 */
		}

		mutex_unlock(&hp_onoff_mutex);
	}
}
#endif				/* #ifdef CONFIG_HOTPLUG_CPU */
/* >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>> */

/*
 * Every sampling_rate, we check, if current idle time is less than 20%
 * (default), then we try to increase frequency. Every sampling_rate, we look
 * for the lowest frequency which can sustain the load while keeping idle time
 * over 30%. If such a frequency exist, we try to decrease to this frequency.
 *
 * Any frequency increase takes it to the maximum frequency. Frequency reduction
 * happens at minimum steps of 5% (default) of current frequency
 */
static void hp_check_cpu(int cpu, unsigned int load_freq)
{
	struct hp_cpu_dbs_info_s *dbs_info = &per_cpu(hp_cpu_dbs_info, cpu);
	struct cpufreq_policy *policy = dbs_info->cdbs.cur_policy;
	struct dbs_data *dbs_data = policy->governor_data;
	struct hp_dbs_tuners *hp_tuners;

	if (!dbs_data)
		return;
	hp_tuners = dbs_data->tuners;
	if (!hp_tuners)
		return;

	dbs_info->freq_lo = 0;

	/* pr_emerg("***** cpu: %d, load_freq: %u, smp_processor_id: %d *****\n", cpu, load_freq, smp_processor_id()); */

	/* Check for frequency increase */
	if (load_freq > hp_tuners->up_threshold * policy->cur) {
		/* If switching to max speed, apply sampling_down_factor */
		if (policy->cur < policy->max)
			dbs_info->rate_mult = hp_tuners->sampling_down_factor;
		dbs_freq_increase(policy, policy->max);
		goto hp_check;	/* <-XXX */
	}

	/* Check for frequency decrease */
	/* if we cannot reduce the frequency anymore, break out early */
	if (policy->cur == policy->min)
		goto hp_check;	/* <-XXX */

	/*
	 * The optimal frequency is the frequency that is the lowest that can
	 * support the current CPU usage without triggering the up policy. To be
	 * safe, we focus 10 points under the threshold.
	 */
	if (load_freq < hp_tuners->adj_up_threshold * policy->cur) {
		unsigned int freq_next;
		freq_next = load_freq / hp_tuners->adj_up_threshold;

		/* No longer fully busy, reset rate_mult */
		dbs_info->rate_mult = 1;

		if (freq_next < policy->min)
			freq_next = policy->min;

		if (!hp_tuners->powersave_bias) {
			__cpufreq_driver_target(policy, freq_next, CPUFREQ_RELATION_L);
		} else {
			freq_next = hp_ops.powersave_bias_target(policy, freq_next,
								 CPUFREQ_RELATION_L);
			__cpufreq_driver_target(policy, freq_next, CPUFREQ_RELATION_L);
		}
	}
/* <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<< */
 hp_check:{
#ifdef CONFIG_HOTPLUG_CPU
		long cpus_sum_load_last_up = 0;
		long cpus_sum_load_last_down = 0;
		unsigned int online_cpus_count;

		int v_tlp_avg_last = 0;
#endif

		/* If Hot Plug policy disable, return directly */
		if (hp_tuners->is_cpu_hotplug_disable)
			return;

#ifdef CONFIG_HOTPLUG_CPU

		if (g_trigger_hp_work != CPU_HOTPLUG_WORK_TYPE_NONE)
			return;

		mutex_lock(&hp_mutex);

		online_cpus_count = num_online_cpus();

		sched_get_nr_running_avg(&g_tlp_avg_current, &g_tlp_iowait_av);

		v_tlp_avg_last = g_tlp_avg_history[g_tlp_avg_index];
		g_tlp_avg_history[g_tlp_avg_index] = g_tlp_avg_current;
		g_tlp_avg_sum += g_tlp_avg_current;

		g_tlp_avg_index =
		    (g_tlp_avg_index + 1 ==
		     hp_tuners->cpu_rush_tlp_times) ? 0 : g_tlp_avg_index + 1;
		g_tlp_avg_count++;

		if (g_tlp_avg_count >= hp_tuners->cpu_rush_tlp_times) {
			if (g_tlp_avg_sum > v_tlp_avg_last)
				g_tlp_avg_sum -= v_tlp_avg_last;
			else
				g_tlp_avg_sum = 0;
		}

		g_tlp_avg_average = g_tlp_avg_sum / hp_tuners->cpu_rush_tlp_times;

		if (hp_tuners->cpu_rush_boost_enable) {
			/* pr_debug("@@@@@@@@@@@@@@@@@@@@@@@@@@@ tlp: %d @@@@@@@@@@@@@@@@@@@@@@@@@@@\n", g_tlp_avg_average); */

			if (g_cpus_sum_load_current >
			    hp_tuners->cpu_rush_threshold * online_cpus_count)
				++g_cpu_rush_count;
			else
				g_cpu_rush_count = 0;

			if ((g_cpu_rush_count >= hp_tuners->cpu_rush_avg_times) &&
			    (online_cpus_count * 100 < g_tlp_avg_average) &&
			    (online_cpus_count < hp_tuners->cpu_num_limit) &&
			    (online_cpus_count < num_possible_cpus())) {
				dbs_freq_increase(policy, policy->max);
				pr_debug("dbs_check_cpu: turn on CPU\n");
				g_next_hp_action =
				    g_tlp_avg_average / 100 + (g_tlp_avg_average % 100 ? 1 : 0);

				if (g_next_hp_action > num_possible_cpus())
					g_next_hp_action = num_possible_cpus();

				g_trigger_hp_work = CPU_HOTPLUG_WORK_TYPE_RUSH;
				/* schedule_delayed_work_on(0, &hp_work, 0); */
				if (hp_wq == NULL)
					pr_emerg("[power/hotplug] %s():%d, impossible\n", __func__, __LINE__);
				else
					queue_delayed_work_on(0, hp_wq, &hp_work, 0);

				goto hp_check_end;
			}
		}

		if (online_cpus_count < hp_tuners->cpu_num_base
		    && online_cpus_count < hp_tuners->cpu_num_limit) {
			dbs_freq_increase(policy, policy->max);
			pr_debug("dbs_check_cpu: turn on CPU\n");
			g_trigger_hp_work = CPU_HOTPLUG_WORK_TYPE_BASE;
			/* schedule_delayed_work_on(0, &hp_work, 0); */
			if (hp_wq == NULL)
				pr_emerg("[power/hotplug] %s():%d, impossible\n", __func__, __LINE__);
			else
				queue_delayed_work_on(0, hp_wq, &hp_work, 0);

			goto hp_check_end;
		}

		if (online_cpus_count > hp_tuners->cpu_num_limit) {
			dbs_freq_increase(policy, policy->max);
			pr_debug("dbs_check_cpu: turn off CPU\n");
			g_trigger_hp_work = CPU_HOTPLUG_WORK_TYPE_LIMIT;
			/* schedule_delayed_work_on(0, &hp_work, 0); */
			if (hp_wq == NULL)
				pr_emerg("[power/hotplug] %s():%d, impossible\n", __func__, __LINE__);
			else
				queue_delayed_work_on(0, hp_wq, &hp_work, 0);

			goto hp_check_end;
		}

		/* Check CPU loading to power up slave CPU */
		if (online_cpus_count < num_possible_cpus()) {
			cpus_sum_load_last_up = g_cpu_up_load_history[g_cpu_up_load_index];
			g_cpu_up_load_history[g_cpu_up_load_index] = g_cpus_sum_load_current;
			g_cpu_up_sum_load += g_cpus_sum_load_current;

			g_cpu_up_count++;
			g_cpu_up_load_index =
			    (g_cpu_up_load_index + 1 ==
			     hp_tuners->cpu_up_avg_times) ? 0 : g_cpu_up_load_index + 1;

			if (g_cpu_up_count >= hp_tuners->cpu_up_avg_times) {
				if (g_cpu_up_sum_load > cpus_sum_load_last_up)
					g_cpu_up_sum_load -= cpus_sum_load_last_up;
				else
					g_cpu_up_sum_load = 0;

				/* g_cpu_up_sum_load /= hp_tuners->cpu_up_avg_times; */
				if (g_cpu_up_sum_load >
				    (hp_tuners->cpu_up_threshold * online_cpus_count *
				     hp_tuners->cpu_up_avg_times)) {
					if (online_cpus_count < hp_tuners->cpu_num_limit) {
#ifdef DEBUG_LOG
						pr_debug("dbs_check_cpu: g_cpu_up_sum_load = %d\n",
							 g_cpu_up_sum_load);
#endif
						dbs_freq_increase(policy, policy->max);
						pr_debug("dbs_check_cpu: turn on CPU\n");
						g_next_hp_action = online_cpus_count + 1;
						g_trigger_hp_work = CPU_HOTPLUG_WORK_TYPE_UP;
						/* schedule_delayed_work_on(0, &hp_work, 0); */
						if (hp_wq == NULL)
							pr_emerg("[power/hotplug] %s():%d, impossible\n", __func__, __LINE__);
						else
							queue_delayed_work_on(0, hp_wq, &hp_work, 0);

						goto hp_check_end;
					}
				}
			}
#ifdef DEBUG_LOG
			pr_debug("dbs_check_cpu: g_cpu_up_count = %d, g_cpu_up_sum_load = %d\n",
				 g_cpu_up_count, g_cpu_up_sum_load);
			pr_debug("dbs_check_cpu: cpu_up_threshold = %d\n",
				 (hp_tuners->cpu_up_threshold * online_cpus_count));
#endif

		}

		/* Check CPU loading to power down slave CPU */
		if (online_cpus_count > 1) {
			cpus_sum_load_last_down = g_cpu_down_load_history[g_cpu_down_load_index];
			g_cpu_down_load_history[g_cpu_down_load_index] = g_cpus_sum_load_current;
			g_cpu_down_sum_load += g_cpus_sum_load_current;

			g_cpu_down_count++;
			g_cpu_down_load_index =
			    (g_cpu_down_load_index + 1 ==
			     hp_tuners->cpu_down_avg_times) ? 0 : g_cpu_down_load_index + 1;

			if (g_cpu_down_count >= hp_tuners->cpu_down_avg_times) {
				long cpu_down_threshold;

				if (g_cpu_down_sum_load > cpus_sum_load_last_down)
					g_cpu_down_sum_load -= cpus_sum_load_last_down;
				else
					g_cpu_down_sum_load = 0;

				g_next_hp_action = online_cpus_count;
				cpu_down_threshold =
				    ((hp_tuners->cpu_up_threshold -
				      hp_tuners->cpu_down_differential) *
				     hp_tuners->cpu_down_avg_times);

				while ((g_cpu_down_sum_load <
					cpu_down_threshold * (g_next_hp_action - 1)) &&
				       /* (g_next_hp_action > tlp_cpu_num) && */
				       (g_next_hp_action > hp_tuners->cpu_num_base))
					--g_next_hp_action;

				/* pr_debug("### g_next_hp_action: %d, tlp_cpu_num: %d, g_cpu_down_sum_load / hp_tuners->cpu_down_avg_times: %d ###\n", g_next_hp_action, tlp_cpu_num, g_cpu_down_sum_load / hp_tuners->cpu_down_avg_times); */
				if (g_next_hp_action < online_cpus_count) {
#ifdef DEBUG_LOG
					pr_debug("dbs_check_cpu: g_cpu_down_sum_load = %d\n",
						 g_cpu_down_sum_load);
#endif
					dbs_freq_increase(policy, policy->max);
					pr_debug("dbs_check_cpu: turn off CPU\n");
					g_trigger_hp_work = CPU_HOTPLUG_WORK_TYPE_DOWN;
					/* schedule_delayed_work_on(0, &hp_work, 0); */
					if (hp_wq == NULL)
						pr_emerg("[power/hotplug] %s():%d, impossible\n", __func__, __LINE__);
					else
						queue_delayed_work_on(0, hp_wq, &hp_work, 0);
				}
			}
#ifdef DEBUG_LOG
			pr_debug("dbs_check_cpu: g_cpu_down_count = %d, g_cpu_down_sum_load = %d\n",
				 g_cpu_down_count, g_cpu_down_sum_load);
			pr_debug("dbs_check_cpu: cpu_down_threshold = %d\n",
				 ((hp_tuners->cpu_up_threshold -
				   hp_tuners->cpu_down_differential) * (online_cpus_count - 1)));
#endif
		}

 hp_check_end:
		mutex_unlock(&hp_mutex);

#endif				/* #ifdef CONFIG_HOTPLUG_CPU */
	}
/* >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>> */
}

static void hp_dbs_timer(struct work_struct *work)
{
	struct hp_cpu_dbs_info_s *dbs_info =
	    container_of(work, struct hp_cpu_dbs_info_s, cdbs.work.work);
	unsigned int cpu = dbs_info->cdbs.cur_policy->cpu;
	struct hp_cpu_dbs_info_s *core_dbs_info = &per_cpu(hp_cpu_dbs_info,
							   cpu);
	struct dbs_data *dbs_data = dbs_info->cdbs.cur_policy->governor_data;
	struct hp_dbs_tuners *hp_tuners;

	int delay = 0, sample_type = core_dbs_info->sample_type;
	bool modify_all = true;

	if (!dbs_data)
		return;
	hp_tuners = dbs_data->tuners;
	if (!hp_tuners)
		return;

	mutex_lock(&core_dbs_info->cdbs.timer_mutex);
	if (!need_load_eval(&core_dbs_info->cdbs, hp_tuners->sampling_rate)) {
		modify_all = false;
		goto max_delay;
	}

	/* Common NORMAL_SAMPLE setup */
	core_dbs_info->sample_type = HP_NORMAL_SAMPLE;
	if (sample_type == HP_SUB_SAMPLE) {
		delay = core_dbs_info->freq_lo_jiffies;
		__cpufreq_driver_target(core_dbs_info->cdbs.cur_policy,
					core_dbs_info->freq_lo, CPUFREQ_RELATION_H);
	} else {
		dbs_check_cpu(dbs_data, cpu);
		if (core_dbs_info->freq_lo) {
			/* Setup timer for SUB_SAMPLE */
			core_dbs_info->sample_type = HP_SUB_SAMPLE;
			delay = core_dbs_info->freq_hi_jiffies;
		}
	}

 max_delay:
	if (!delay)
		delay = delay_for_sampling_rate(hp_tuners->sampling_rate
						* core_dbs_info->rate_mult);

	gov_queue_work(dbs_data, dbs_info->cdbs.cur_policy, delay, modify_all);
	mutex_unlock(&core_dbs_info->cdbs.timer_mutex);

/* <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<< */
	/* for downgrade */ /* TODO: FIXME */
	if (cpufreq_freq_check)
		cpufreq_freq_check(0);	/* TODO: FIXME, fix cpuid = 0 */
/* >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>> */
}

/************************** sysfs interface ************************/
static struct common_dbs_data hp_dbs_cdata;

/**
 * update_sampling_rate - update sampling rate effective immediately if needed.
 * @new_rate: new sampling rate
 *
 * If new rate is smaller than the old, simply updating
 * dbs_tuners_int.sampling_rate might not be appropriate. For example, if the
 * original sampling_rate was 1 second and the requested new sampling rate is 10
 * ms because the user needs immediate reaction from hotplug governor, but not
 * sure if higher frequency will be required or not, then, the governor may
 * change the sampling rate too late; up to 1 second later. Thus, if we are
 * reducing the sampling rate, we need to make the new value effective
 * immediately.
 */
static void update_sampling_rate(struct dbs_data *dbs_data, unsigned int new_rate)
{
	struct hp_dbs_tuners *hp_tuners = dbs_data->tuners;

	hp_tuners->sampling_rate = new_rate = max(new_rate, dbs_data->min_sampling_rate);

	{
		struct cpufreq_policy *policy;
		struct hp_cpu_dbs_info_s *dbs_info;
		unsigned long next_sampling, appointed_at;

		policy = cpufreq_cpu_get(0);
		if (!policy)
			return;
		if (policy->governor != &cpufreq_gov_hotplug) {
			cpufreq_cpu_put(policy);
			return;
		}
		dbs_info = &per_cpu(hp_cpu_dbs_info, 0);
		cpufreq_cpu_put(policy);

		mutex_lock(&dbs_info->cdbs.timer_mutex);

		if (!delayed_work_pending(&dbs_info->cdbs.work)) {
			mutex_unlock(&dbs_info->cdbs.timer_mutex);
			return;
		}

		next_sampling = jiffies + usecs_to_jiffies(new_rate);
		appointed_at = dbs_info->cdbs.work.timer.expires;

		if (time_before(next_sampling, appointed_at)) {

			mutex_unlock(&dbs_info->cdbs.timer_mutex);
			cancel_delayed_work_sync(&dbs_info->cdbs.work);
			mutex_lock(&dbs_info->cdbs.timer_mutex);

			gov_queue_work(dbs_data, dbs_info->cdbs.cur_policy,
				       usecs_to_jiffies(new_rate), true);

		}
		mutex_unlock(&dbs_info->cdbs.timer_mutex);
	}
}

void hp_enable_timer(int enable)
{
#if 1
	struct dbs_data *dbs_data = per_cpu(hp_cpu_dbs_info, 0).cdbs.cur_policy->governor_data; /* TODO: FIXME, cpu = 0 */
	static unsigned int sampling_rate_backup = 0;

	if (!dbs_data || dbs_data->cdata->governor != GOV_HOTPLUG || (enable && !sampling_rate_backup))
		return;

	if (enable)
		update_sampling_rate(dbs_data, sampling_rate_backup);
	else {
		struct hp_dbs_tuners *hp_tuners = dbs_data->tuners;

		sampling_rate_backup = hp_tuners->sampling_rate;
		update_sampling_rate(dbs_data, 30000 * 100);
	}
#else
	struct dbs_data *dbs_data = per_cpu(hp_cpu_dbs_info, 0).cdbs.cur_policy->governor_data; /* TODO: FIXME, cpu = 0 */
	int cpu = 0;
	struct cpufreq_policy *policy;
	struct hp_dbs_tuners *hp_tuners;
	struct hp_cpu_dbs_info_s *dbs_info;

	policy = cpufreq_cpu_get(cpu);
	if (!policy)
		continue;
	if (policy->governor != &cpufreq_gov_hotplug) {
		cpufreq_cpu_put(policy);
		continue;
	}
	dbs_info = &per_cpu(hp_cpu_dbs_info, cpu);
	cpufreq_cpu_put(policy);

	if (enable) {
		hp_tuners = dbs_data->tuners;
		mutex_lock(&dbs_info->cdbs.timer_mutex);
		gov_queue_work(dbs_data, dbs_info->cdbs.cur_policy, usecs_to_jiffies(hp_tuners->sampling_rate), true);
		mutex_unlock(&dbs_info->cdbs.timer_mutex);
	} else
		cancel_delayed_work_sync(&dbs_info->cdbs.work);
	}
#endif
}
EXPORT_SYMBOL(hp_enable_timer);

static ssize_t store_sampling_rate(struct dbs_data *dbs_data, const char *buf, size_t count)
{
	unsigned int input;
	int ret;
	ret = sscanf(buf, "%u", &input);
	if (ret != 1)
		return -EINVAL;

	update_sampling_rate(dbs_data, input);
	return count;
}

static ssize_t store_io_is_busy(struct dbs_data *dbs_data, const char *buf, size_t count)
{
	struct hp_dbs_tuners *hp_tuners = dbs_data->tuners;
	unsigned int input;
	int ret;
	unsigned int j;

	ret = sscanf(buf, "%u", &input);
	if (ret != 1)
		return -EINVAL;
	hp_tuners->io_is_busy = !!input;

	/* we need to re-evaluate prev_cpu_idle */
	for_each_online_cpu(j) {
		struct hp_cpu_dbs_info_s *dbs_info = &per_cpu(hp_cpu_dbs_info,
							      j);
		dbs_info->cdbs.prev_cpu_idle = get_cpu_idle_time(j,
								 &dbs_info->cdbs.prev_cpu_wall,
								 hp_tuners->io_is_busy);
	}
	return count;
}

static ssize_t store_up_threshold(struct dbs_data *dbs_data, const char *buf, size_t count)
{
	struct hp_dbs_tuners *hp_tuners = dbs_data->tuners;
	unsigned int input;
	int ret;
	ret = sscanf(buf, "%u", &input);

	if (ret != 1 || input > MAX_FREQUENCY_UP_THRESHOLD || input < MIN_FREQUENCY_UP_THRESHOLD)
		return -EINVAL;

	/* Calculate the new adj_up_threshold */
	hp_tuners->adj_up_threshold += input;
	hp_tuners->adj_up_threshold -= hp_tuners->up_threshold;

	hp_tuners->up_threshold = input;
	return count;
}

static ssize_t store_sampling_down_factor(struct dbs_data *dbs_data, const char *buf, size_t count)
{
	struct hp_dbs_tuners *hp_tuners = dbs_data->tuners;
	unsigned int input, j;
	int ret;
	ret = sscanf(buf, "%u", &input);

	if (ret != 1 || input > MAX_SAMPLING_DOWN_FACTOR || input < 1)
		return -EINVAL;
	hp_tuners->sampling_down_factor = input;

	/* Reset down sampling multiplier in case it was active */
	for_each_online_cpu(j) {
		struct hp_cpu_dbs_info_s *dbs_info = &per_cpu(hp_cpu_dbs_info,
							      j);
		dbs_info->rate_mult = 1;
	}
	return count;
}

static ssize_t store_ignore_nice_load(struct dbs_data *dbs_data, const char *buf, size_t count)
{
	struct hp_dbs_tuners *hp_tuners = dbs_data->tuners;
	unsigned int input;
	int ret;

	unsigned int j;

	ret = sscanf(buf, "%u", &input);
	if (ret != 1)
		return -EINVAL;

	if (input > 1)
		input = 1;

	if (input == hp_tuners->ignore_nice_load)	/* nothing to do */
		return count;

	hp_tuners->ignore_nice_load = input;

	/* we need to re-evaluate prev_cpu_idle */
	for_each_online_cpu(j) {
		struct hp_cpu_dbs_info_s *dbs_info;
		dbs_info = &per_cpu(hp_cpu_dbs_info, j);
		dbs_info->cdbs.prev_cpu_idle = get_cpu_idle_time(j,
								 &dbs_info->cdbs.prev_cpu_wall,
								 hp_tuners->io_is_busy);
		if (hp_tuners->ignore_nice_load)
			dbs_info->cdbs.prev_cpu_nice = kcpustat_cpu(j).cpustat[CPUTIME_NICE];

	}
	return count;
}

static ssize_t store_powersave_bias(struct dbs_data *dbs_data, const char *buf, size_t count)
{
	struct hp_dbs_tuners *hp_tuners = dbs_data->tuners;
	unsigned int input;
	int ret;
	ret = sscanf(buf, "%u", &input);

	if (ret != 1)
		return -EINVAL;

	if (input > 1000)
		input = 1000;

	hp_tuners->powersave_bias = input;
	hotplug_powersave_bias_init();
	return count;
}

show_store_one(hp, sampling_rate);
show_store_one(hp, io_is_busy);
show_store_one(hp, up_threshold);
show_store_one(hp, sampling_down_factor);
show_store_one(hp, ignore_nice_load);
show_store_one(hp, powersave_bias);
declare_show_sampling_rate_min(hp);

gov_sys_pol_attr_rw(sampling_rate);
gov_sys_pol_attr_rw(io_is_busy);
gov_sys_pol_attr_rw(up_threshold);
gov_sys_pol_attr_rw(sampling_down_factor);
gov_sys_pol_attr_rw(ignore_nice_load);
gov_sys_pol_attr_rw(powersave_bias);
gov_sys_pol_attr_ro(sampling_rate_min);

/* <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<< */
static ssize_t store_down_differential(struct dbs_data *dbs_data, const char *buf, size_t count)
{
	struct hp_dbs_tuners *hp_tuners = dbs_data->tuners;
	unsigned int input;
	int ret;
	ret = sscanf(buf, "%u", &input);

	if (ret != 1
	    || input > MAX_FREQUENCY_DOWN_DIFFERENTIAL || input < MIN_FREQUENCY_DOWN_DIFFERENTIAL)
		return -EINVAL;

	hp_tuners->down_differential = input;

	return count;
}

/*
 * cpu hotplug - function definition of sysfs
 */
static ssize_t store_cpu_up_threshold(struct dbs_data *dbs_data, const char *buf, size_t count)
{
	struct hp_dbs_tuners *hp_tuners = dbs_data->tuners;
	unsigned int input;
	int ret;
	ret = sscanf(buf, "%u", &input);

	if (ret != 1 || input > MAX_CPU_UP_THRESHOLD || input < MIN_CPU_UP_THRESHOLD)
		return -EINVAL;

	mutex_lock(&hp_mutex);
	hp_tuners->cpu_up_threshold = input;
	hp_reset_strategy_nolock();
	mutex_unlock(&hp_mutex);

	return count;
}

static ssize_t store_cpu_down_differential(struct dbs_data *dbs_data, const char *buf, size_t count)
{
	struct hp_dbs_tuners *hp_tuners = dbs_data->tuners;
	unsigned int input;
	int ret;
	ret = sscanf(buf, "%u", &input);

	if (ret != 1 || input > MAX_CPU_DOWN_DIFFERENTIAL || input < MIN_CPU_DOWN_DIFFERENTIAL)
		return -EINVAL;

	mutex_lock(&hp_mutex);
	hp_tuners->cpu_down_differential = input;
	hp_reset_strategy_nolock();
	mutex_unlock(&hp_mutex);

	return count;
}

static ssize_t store_cpu_up_avg_times(struct dbs_data *dbs_data, const char *buf, size_t count)
{
	struct hp_dbs_tuners *hp_tuners = dbs_data->tuners;
	unsigned int input;
	int ret;
	ret = sscanf(buf, "%u", &input);

	if (ret != 1 || input > MAX_CPU_UP_AVG_TIMES || input < MIN_CPU_UP_AVG_TIMES)
		return -EINVAL;

	mutex_lock(&hp_mutex);
	hp_tuners->cpu_up_avg_times = input;
	hp_reset_strategy_nolock();
	mutex_unlock(&hp_mutex);

	return count;
}

static ssize_t store_cpu_down_avg_times(struct dbs_data *dbs_data, const char *buf, size_t count)
{
	struct hp_dbs_tuners *hp_tuners = dbs_data->tuners;
	unsigned int input;
	int ret;
	ret = sscanf(buf, "%u", &input);

	if (ret != 1 || input > MAX_CPU_DOWN_AVG_TIMES || input < MIN_CPU_DOWN_AVG_TIMES)
		return -EINVAL;

	mutex_lock(&hp_mutex);
	hp_tuners->cpu_down_avg_times = input;
	hp_reset_strategy_nolock();
	mutex_unlock(&hp_mutex);

	return count;
}

static ssize_t store_cpu_num_limit(struct dbs_data *dbs_data, const char *buf, size_t count)
{
	struct hp_dbs_tuners *hp_tuners = dbs_data->tuners;
	unsigned int input;
	int ret;
	ret = sscanf(buf, "%u", &input);

	if (ret != 1 || input > num_possible_cpus()
	    || input < 1)
		return -EINVAL;

	mutex_lock(&hp_mutex);
	hp_tuners->cpu_num_limit = input;
	mutex_unlock(&hp_mutex);

	return count;
}

static ssize_t store_cpu_num_base(struct dbs_data *dbs_data, const char *buf, size_t count)
{
	struct hp_dbs_tuners *hp_tuners = dbs_data->tuners;
	unsigned int input;
	unsigned int online_cpus_count;
	int ret;
	ret = sscanf(buf, "%u", &input);

	if (ret != 1 || input > num_possible_cpus()
	    || input < 1)
		return -EINVAL;

	mutex_lock(&hp_mutex);

	hp_tuners->cpu_num_base = input;
	online_cpus_count = num_online_cpus();
#ifdef CONFIG_HOTPLUG_CPU

	if (online_cpus_count < input && online_cpus_count < hp_tuners->cpu_num_limit) {
		struct cpufreq_policy *policy = per_cpu(hp_cpu_dbs_info, 0).cdbs.cur_policy;	/* TODO: FIXME, cpu = 0 */

		dbs_freq_increase(policy, policy->max);
		g_trigger_hp_work = CPU_HOTPLUG_WORK_TYPE_BASE;
		/* schedule_delayed_work_on(0, &hp_work, 0); */
		if (hp_wq == NULL)
			pr_emerg("[power/hotplug] %s():%d, impossible\n", __func__, __LINE__);
		else
			queue_delayed_work_on(0, hp_wq, &hp_work, 0);
	}
#endif

	mutex_unlock(&hp_mutex);

	return count;
}

static ssize_t store_is_cpu_hotplug_disable(struct dbs_data *dbs_data, const char *buf,
					    size_t count)
{
	struct hp_dbs_tuners *hp_tuners = dbs_data->tuners;
	unsigned int input;
	int ret;
	ret = sscanf(buf, "%u", &input);

	if (ret != 1 || input > 1 || input < 0)
		return -EINVAL;

	mutex_lock(&hp_mutex);

	if (hp_tuners->is_cpu_hotplug_disable && !input)
		hp_reset_strategy_nolock();

	hp_tuners->is_cpu_hotplug_disable = input;
	mutex_unlock(&hp_mutex);

	return count;
}

static ssize_t store_cpu_input_boost_enable(struct dbs_data *dbs_data, const char *buf,
					    size_t count)
{
	struct hp_dbs_tuners *hp_tuners = dbs_data->tuners;
	unsigned int input;
	int ret;
	ret = sscanf(buf, "%u", &input);

	if (ret != 1 || input > 1 || input < 0)
		return -EINVAL;

	mutex_lock(&hp_mutex);
	hp_tuners->cpu_input_boost_enable = input;
	mutex_unlock(&hp_mutex);

	return count;
}

static ssize_t store_cpu_input_boost_num(struct dbs_data *dbs_data, const char *buf, size_t count)
{
	struct hp_dbs_tuners *hp_tuners = dbs_data->tuners;
	unsigned int input;
	int ret;
	ret = sscanf(buf, "%u", &input);

	if (ret != 1 || input > num_possible_cpus()
	    || input < 2)
		return -EINVAL;

	mutex_lock(&hp_mutex);
	hp_tuners->cpu_input_boost_num = input;
	mutex_unlock(&hp_mutex);

	return count;
}

static ssize_t store_cpu_rush_boost_enable(struct dbs_data *dbs_data, const char *buf, size_t count)
{
	struct hp_dbs_tuners *hp_tuners = dbs_data->tuners;
	unsigned int input;
	int ret;
	ret = sscanf(buf, "%u", &input);

	if (ret != 1 || input > 1 || input < 0)
		return -EINVAL;

	mutex_lock(&hp_mutex);
	hp_tuners->cpu_rush_boost_enable = input;
	mutex_unlock(&hp_mutex);

	return count;
}

static ssize_t store_cpu_rush_boost_num(struct dbs_data *dbs_data, const char *buf, size_t count)
{
	struct hp_dbs_tuners *hp_tuners = dbs_data->tuners;
	unsigned int input;
	int ret;
	ret = sscanf(buf, "%u", &input);

	if (ret != 1 || input > num_possible_cpus()
	    || input < 2)
		return -EINVAL;

	mutex_lock(&hp_mutex);
	hp_tuners->cpu_rush_boost_num = input;
	mutex_unlock(&hp_mutex);

	return count;
}

static ssize_t store_cpu_rush_threshold(struct dbs_data *dbs_data, const char *buf, size_t count)
{
	struct hp_dbs_tuners *hp_tuners = dbs_data->tuners;
	unsigned int input;
	int ret;
	ret = sscanf(buf, "%u", &input);

	if (ret != 1 || input > MAX_CPU_RUSH_THRESHOLD || input < MIN_CPU_RUSH_THRESHOLD)
		return -EINVAL;

	mutex_lock(&hp_mutex);
	hp_tuners->cpu_rush_threshold = input;
	/* hp_reset_strategy_nolock(); //no need */
	mutex_unlock(&hp_mutex);

	return count;
}

static ssize_t store_cpu_rush_tlp_times(struct dbs_data *dbs_data, const char *buf, size_t count)
{
	struct hp_dbs_tuners *hp_tuners = dbs_data->tuners;
	unsigned int input;
	int ret;
	ret = sscanf(buf, "%u", &input);

	if (ret != 1 || input > MAX_CPU_RUSH_TLP_TIMES || input < MIN_CPU_RUSH_TLP_TIMES)
		return -EINVAL;

	mutex_lock(&hp_mutex);
	hp_tuners->cpu_rush_tlp_times = input;
	hp_reset_strategy_nolock();
	mutex_unlock(&hp_mutex);

	return count;
}

static ssize_t store_cpu_rush_avg_times(struct dbs_data *dbs_data, const char *buf, size_t count)
{
	struct hp_dbs_tuners *hp_tuners = dbs_data->tuners;
	unsigned int input;
	int ret;
	ret = sscanf(buf, "%u", &input);

	if (ret != 1 || input > MAX_CPU_RUSH_AVG_TIMES || input < MIN_CPU_RUSH_AVG_TIMES)
		return -EINVAL;

	mutex_lock(&hp_mutex);
	hp_tuners->cpu_rush_avg_times = input;
	hp_reset_strategy_nolock();
	mutex_unlock(&hp_mutex);

	return count;
}

show_store_one(hp, down_differential);
show_store_one(hp, cpu_up_threshold);
show_store_one(hp, cpu_down_differential);
show_store_one(hp, cpu_up_avg_times);
show_store_one(hp, cpu_down_avg_times);
show_store_one(hp, cpu_num_limit);
show_store_one(hp, cpu_num_base);
show_store_one(hp, is_cpu_hotplug_disable);
show_store_one(hp, cpu_input_boost_enable);
show_store_one(hp, cpu_input_boost_num);
show_store_one(hp, cpu_rush_boost_enable);
show_store_one(hp, cpu_rush_boost_num);
show_store_one(hp, cpu_rush_threshold);
show_store_one(hp, cpu_rush_tlp_times);
show_store_one(hp, cpu_rush_avg_times);

gov_sys_pol_attr_rw(down_differential);
gov_sys_pol_attr_rw(cpu_up_threshold);
gov_sys_pol_attr_rw(cpu_down_differential);
gov_sys_pol_attr_rw(cpu_up_avg_times);
gov_sys_pol_attr_rw(cpu_down_avg_times);
gov_sys_pol_attr_rw(cpu_num_limit);
gov_sys_pol_attr_rw(cpu_num_base);
gov_sys_pol_attr_rw(is_cpu_hotplug_disable);
gov_sys_pol_attr_rw(cpu_input_boost_enable);
gov_sys_pol_attr_rw(cpu_input_boost_num);
gov_sys_pol_attr_rw(cpu_rush_boost_enable);
gov_sys_pol_attr_rw(cpu_rush_boost_num);
gov_sys_pol_attr_rw(cpu_rush_threshold);
gov_sys_pol_attr_rw(cpu_rush_tlp_times);
gov_sys_pol_attr_rw(cpu_rush_avg_times);
/* >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>> */

static struct attribute *dbs_attributes_gov_sys[] = {
	&sampling_rate_min_gov_sys.attr,
	&sampling_rate_gov_sys.attr,
	&up_threshold_gov_sys.attr,
	&sampling_down_factor_gov_sys.attr,
	&ignore_nice_load_gov_sys.attr,
	&powersave_bias_gov_sys.attr,
	&io_is_busy_gov_sys.attr,
/* <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<< */
	&down_differential_gov_sys.attr,
	&cpu_up_threshold_gov_sys.attr,
	&cpu_down_differential_gov_sys.attr,
	&cpu_up_avg_times_gov_sys.attr,
	&cpu_down_avg_times_gov_sys.attr,
	&cpu_num_limit_gov_sys.attr,
	&cpu_num_base_gov_sys.attr,
	&is_cpu_hotplug_disable_gov_sys.attr,
	&cpu_input_boost_enable_gov_sys.attr,
	&cpu_input_boost_num_gov_sys.attr,
	&cpu_rush_boost_enable_gov_sys.attr,
	&cpu_rush_boost_num_gov_sys.attr,
	&cpu_rush_threshold_gov_sys.attr,
	&cpu_rush_tlp_times_gov_sys.attr,
	&cpu_rush_avg_times_gov_sys.attr,
/* >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>> */
	NULL
};

static struct attribute_group hp_attr_group_gov_sys = {
	.attrs = dbs_attributes_gov_sys,
	.name = "hotplug",
};

static struct attribute *dbs_attributes_gov_pol[] = {
	&sampling_rate_min_gov_pol.attr,
	&sampling_rate_gov_pol.attr,
	&up_threshold_gov_pol.attr,
	&sampling_down_factor_gov_pol.attr,
	&ignore_nice_load_gov_pol.attr,
	&powersave_bias_gov_pol.attr,
	&io_is_busy_gov_pol.attr,
/* <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<< */
	&down_differential_gov_pol.attr,
	&cpu_up_threshold_gov_pol.attr,
	&cpu_down_differential_gov_pol.attr,
	&cpu_up_avg_times_gov_pol.attr,
	&cpu_down_avg_times_gov_pol.attr,
	&cpu_num_limit_gov_pol.attr,
	&cpu_num_base_gov_pol.attr,
	&is_cpu_hotplug_disable_gov_pol.attr,
	&cpu_input_boost_enable_gov_pol.attr,
	&cpu_input_boost_num_gov_pol.attr,
	&cpu_rush_boost_enable_gov_pol.attr,
	&cpu_rush_boost_num_gov_pol.attr,
	&cpu_rush_threshold_gov_pol.attr,
	&cpu_rush_tlp_times_gov_pol.attr,
	&cpu_rush_avg_times_gov_pol.attr,
/* >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>> */
	NULL
};

static struct attribute_group hp_attr_group_gov_pol = {
	.attrs = dbs_attributes_gov_pol,
	.name = "hotplug",
};

/************************** sysfs end ************************/

/* <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<< */

#ifdef CONFIG_HOTPLUG_CPU

static struct task_struct *freq_up_task;

static int touch_freq_up_task(void *data)
{
	struct cpufreq_policy *policy;

	while (1) {
		policy = cpufreq_cpu_get(0);
		dbs_freq_increase(policy, policy->max);
		cpufreq_cpu_put(policy);
		/* mt_cpufreq_set_ramp_down_count_const(0, 100); */
		pr_debug("@%s():%d\n", __func__, __LINE__);

		set_current_state(TASK_INTERRUPTIBLE);
		schedule();

		if (kthread_should_stop())
			break;
	}

	return 0;
}

static void dbs_input_event(struct input_handle *handle, unsigned int type,
			    unsigned int code, int value)
{
	/* int i; */

	/* if ((dbs_tuners_ins.powersave_bias == POWERSAVE_BIAS_MAXLEVEL) || */
	/* (dbs_tuners_ins.powersave_bias == POWERSAVE_BIAS_MINLEVEL)) { */
	/* nothing to do */
	/* return; */
	/* } */

	/* for_each_online_cpu(i) { */
	/* queue_work_on(i, input_wq, &per_cpu(dbs_refresh_work, i)); */
	/* } */
	/* pr_debug("$$$ in_interrupt(): %d, in_irq(): %d, type: %d, code: %d, value: %d $$$\n", in_interrupt(), in_irq(), type, code, value); */

	struct dbs_data *dbs_data = per_cpu(hp_cpu_dbs_info, 0).cdbs.cur_policy->governor_data;	/* TODO: FIXME, cpu = 0 */
	struct hp_dbs_tuners *hp_tuners;

	if (!dbs_data)
		return;
	hp_tuners = dbs_data->tuners;
	if (!hp_tuners)
		return;

	if ((type == EV_KEY) && (code == BTN_TOUCH) && (value == 1)
	    && (dbs_data->cdata->governor == GOV_HOTPLUG && hp_tuners->cpu_input_boost_enable)) {
		/* if (!in_interrupt()) */
		/* { */
		unsigned int online_cpus_count = num_online_cpus();

		pr_debug("@%s():%d, online_cpus_count = %d, cpu_input_boost_num = %d\n", __func__, __LINE__, online_cpus_count, hp_tuners->cpu_input_boost_num);

		if (online_cpus_count < hp_tuners->cpu_input_boost_num && online_cpus_count < hp_tuners->cpu_num_limit) {
			/* schedule_delayed_work_on(0, &hp_work, 0); */
			if (hp_wq == NULL)
				pr_emerg("[power/hotplug] %s():%d, impossible\n", __func__, __LINE__);
			else
				queue_delayed_work_on(0, hp_wq, &hp_work, 0);
		}

		if (online_cpus_count <= hp_tuners->cpu_input_boost_num && online_cpus_count <= hp_tuners->cpu_num_limit)
			wake_up_process(freq_up_task);

		/* } */
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
	{.driver_info = 1},
	{},
};

static struct input_handler dbs_input_handler = {
	.event = dbs_input_event,
	.connect = dbs_input_connect,
	.disconnect = dbs_input_disconnect,
	.name = "cpufreq_ond",
	.id_table = dbs_ids,
};
#endif				/* #ifdef CONFIG_HOTPLUG_CPU */

/* >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>> */

static int hp_init(struct dbs_data *dbs_data)
{
	struct hp_dbs_tuners *tuners;
	u64 idle_time;
	int cpu;

	tuners = kzalloc(sizeof(struct hp_dbs_tuners), GFP_KERNEL);
	if (!tuners) {
		pr_err("%s: kzalloc failed\n", __func__);
		return -ENOMEM;
	}

	cpu = get_cpu();
	idle_time = get_cpu_idle_time_us(cpu, NULL);
	put_cpu();
	if (idle_time != -1ULL) {
		/* Idle micro accounting is supported. Use finer thresholds */
		tuners->up_threshold = MICRO_FREQUENCY_UP_THRESHOLD;
		tuners->adj_up_threshold = MICRO_FREQUENCY_UP_THRESHOLD -
		    MICRO_FREQUENCY_DOWN_DIFFERENTIAL;
		tuners->down_differential = MICRO_FREQUENCY_DOWN_DIFFERENTIAL;	/* <-XXX */
		tuners->cpu_up_threshold = MICRO_CPU_UP_THRESHOLD;	/* <-XXX */
		tuners->cpu_down_differential = MICRO_CPU_DOWN_DIFFERENTIAL;	/* <-XXX */
		/*
		 * In nohz/micro accounting case we set the minimum frequency
		 * not depending on HZ, but fixed (very low). The deferred
		 * timer might skip some samples if idle/sleeping as needed.
		 */
		dbs_data->min_sampling_rate = MICRO_FREQUENCY_MIN_SAMPLE_RATE;

		/* cpu rush boost */
		tuners->cpu_rush_threshold = MICRO_CPU_RUSH_THRESHOLD;	/* <-XXX */
	} else {
		tuners->up_threshold = DEF_FREQUENCY_UP_THRESHOLD;
		tuners->adj_up_threshold = DEF_FREQUENCY_UP_THRESHOLD -
		    DEF_FREQUENCY_DOWN_DIFFERENTIAL;
		tuners->down_differential = DEF_FREQUENCY_DOWN_DIFFERENTIAL;	/* <-XXX */
		tuners->cpu_up_threshold = DEF_CPU_UP_THRESHOLD;	/* <-XXX */
		tuners->cpu_down_differential = DEF_CPU_DOWN_DIFFERENTIAL;	/* <-XXX */

		/* For correct statistics, we need 10 ticks for each measure */
		dbs_data->min_sampling_rate = MIN_SAMPLING_RATE_RATIO * jiffies_to_usecs(10);

		/* cpu rush boost */
		tuners->cpu_rush_threshold = DEF_CPU_RUSH_THRESHOLD;	/* <-XXX */
	}

	tuners->sampling_down_factor = DEF_SAMPLING_DOWN_FACTOR;
	tuners->ignore_nice_load = 0;
	tuners->powersave_bias = default_powersave_bias;
	tuners->io_is_busy = should_io_be_busy();
/* <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<< */
	tuners->cpu_up_avg_times = DEF_CPU_UP_AVG_TIMES;
	tuners->cpu_down_avg_times = DEF_CPU_DOWN_AVG_TIMES;
	tuners->cpu_num_limit = num_possible_cpus();
	tuners->cpu_num_base = 1;
	tuners->is_cpu_hotplug_disable = (tuners->cpu_num_limit > 1) ? 0 : 1;
	tuners->cpu_input_boost_enable = DEF_CPU_INPUT_BOOST_ENABLE;
	tuners->cpu_input_boost_num = DEF_CPU_INPUT_BOOST_NUM;
	tuners->cpu_rush_boost_enable = DEF_CPU_RUSH_BOOST_ENABLE;
	tuners->cpu_rush_boost_num = num_possible_cpus();
	tuners->cpu_rush_tlp_times = DEF_CPU_RUSH_TLP_TIMES;
	tuners->cpu_rush_avg_times = DEF_CPU_RUSH_AVG_TIMES;

#ifdef CONFIG_HOTPLUG_CPU
	INIT_DEFERRABLE_WORK(&hp_work, hp_work_handler);
	hp_wq = alloc_workqueue("hp_work_handler", WQ_HIGHPRI, 0);
	g_next_hp_action = num_online_cpus();
#endif

#ifdef DEBUG_LOG
	pr_debug("cpufreq_gov_dbs_init: min_sampling_rate = %d\n", dbs_data->min_sampling_rate);
	pr_debug("cpufreq_gov_dbs_init: dbs_tuners_ins.up_threshold = %d\n", tuners->up_threshold);
	pr_debug("cpufreq_gov_dbs_init: dbs_tuners_ins.down_differential = %d\n",
		 tuners->down_differential);
	pr_debug("cpufreq_gov_dbs_init: dbs_tuners_ins.cpu_up_threshold = %d\n",
		 tuners->cpu_up_threshold);
	pr_debug("cpufreq_gov_dbs_init: dbs_tuners_ins.cpu_down_differential = %d\n",
		 tuners->cpu_down_differential);
	pr_debug("cpufreq_gov_dbs_init: dbs_tuners_ins.cpu_up_avg_times = %d\n",
		 tuners->cpu_up_avg_times);
	pr_debug("cpufreq_gov_dbs_init: dbs_tuners_ins.cpu_down_avg_times = %d\n",
		 tuners->cpu_down_avg_times);
	pr_debug("cpufreq_gov_dbs_init: dbs_tuners_ins.cpu_num_limit = %d\n",
		 tuners->cpu_num_limit);
	pr_debug("cpufreq_gov_dbs_init: dbs_tuners_ins.cpu_num_base = %d\n", tuners->cpu_num_base);
	pr_debug("cpufreq_gov_dbs_init: dbs_tuners_ins.is_cpu_hotplug_disable = %d\n",
		 tuners->is_cpu_hotplug_disable);
	pr_debug("cpufreq_gov_dbs_init: dbs_tuners_ins.cpu_input_boost_enable = %d\n",
		 tuners->cpu_input_boost_enable);
	pr_debug("cpufreq_gov_dbs_init: dbs_tuners_ins.cpu_input_boost_num = %d\n",
		 tuners->cpu_input_boost_num);
	pr_debug("cpufreq_gov_dbs_init: dbs_tuners_ins.cpu_rush_boost_enable = %d\n",
		 tuners->cpu_rush_boost_enable);
	pr_debug("cpufreq_gov_dbs_init: dbs_tuners_ins.cpu_rush_boost_num = %d\n",
		 tuners->cpu_rush_boost_num);
	pr_debug("cpufreq_gov_dbs_init: dbs_tuners_ins.cpu_rush_threshold = %d\n",
		 tuners->cpu_rush_threshold);
	pr_debug("cpufreq_gov_dbs_init: dbs_tuners_ins.cpu_rush_tlp_times = %d\n",
		 tuners->cpu_rush_tlp_times);
	pr_debug("cpufreq_gov_dbs_init: dbs_tuners_ins.cpu_rush_avg_times = %d\n",
		 tuners->cpu_rush_avg_times);
#endif
/* >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>> */

	dbs_data->tuners = tuners;
	mutex_init(&dbs_data->mutex);
	return 0;
}

static void hp_exit(struct dbs_data *dbs_data)
{
/* <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<< */
#ifdef CONFIG_HOTPLUG_CPU
	cancel_delayed_work_sync(&hp_work);
	if (hp_wq)
		destroy_workqueue(hp_wq);
#endif
/* >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>> */
	kfree(dbs_data->tuners);
}

define_get_cpu_dbs_routines(hp_cpu_dbs_info);

static struct hp_ops hp_ops = {
	.powersave_bias_init_cpu = hotplug_powersave_bias_init_cpu,
	.powersave_bias_target = generic_powersave_bias_target,
	.freq_increase = dbs_freq_increase,
	.input_handler = &dbs_input_handler,
};

static struct common_dbs_data hp_dbs_cdata = {
	.governor = GOV_HOTPLUG,
	.attr_group_gov_sys = &hp_attr_group_gov_sys,
	.attr_group_gov_pol = &hp_attr_group_gov_pol,
	.get_cpu_cdbs = get_cpu_cdbs,
	.get_cpu_dbs_info_s = get_cpu_dbs_info_s,
	.gov_dbs_timer = hp_dbs_timer,
	.gov_check_cpu = hp_check_cpu,
	.gov_ops = &hp_ops,
	.init = hp_init,
	.exit = hp_exit,
};

static void hp_set_powersave_bias(unsigned int powersave_bias)
{
	struct cpufreq_policy *policy;
	struct dbs_data *dbs_data;
	struct hp_dbs_tuners *hp_tuners;
	unsigned int cpu;
	cpumask_t done;

	default_powersave_bias = powersave_bias;
	cpumask_clear(&done);

	get_online_cpus();
	for_each_online_cpu(cpu) {
		if (cpumask_test_cpu(cpu, &done))
			continue;

		policy = per_cpu(hp_cpu_dbs_info, cpu).cdbs.cur_policy;
		if (!policy)
			continue;

		cpumask_or(&done, &done, policy->cpus);

		if (policy->governor != &cpufreq_gov_hotplug)
			continue;

		dbs_data = policy->governor_data;
		hp_tuners = dbs_data->tuners;
		hp_tuners->powersave_bias = default_powersave_bias;
	}
	put_online_cpus();
}

void hp_register_powersave_bias_handler(unsigned int (*f)
					 (struct cpufreq_policy *, unsigned int, unsigned int),
					unsigned int powersave_bias)
{
	hp_ops.powersave_bias_target = f;
	hp_set_powersave_bias(powersave_bias);
}
EXPORT_SYMBOL_GPL(hp_register_powersave_bias_handler);

void hp_unregister_powersave_bias_handler(void)
{
	hp_ops.powersave_bias_target = generic_powersave_bias_target;
	hp_set_powersave_bias(0);
}
EXPORT_SYMBOL_GPL(hp_unregister_powersave_bias_handler);

static int hp_cpufreq_governor_dbs(struct cpufreq_policy *policy, unsigned int event)
{
/* <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<< */
	struct dbs_data *dbs_data;
	int rc = 0;

	if (have_governor_per_policy())
		dbs_data = policy->governor_data;
	else
		dbs_data = hp_dbs_cdata.gdbs_data;

	/* pr_emerg("***** policy->cpu: %d, event: %u, smp_processor_id: %d, have_governor_per_policy: %d *****\n", policy->cpu, event, smp_processor_id(), have_governor_per_policy()); */
	switch (event) {
	case CPUFREQ_GOV_START:
#ifdef DEBUG_LOG
		{
			struct hp_dbs_tuners *hp_tuners = dbs_data->tuners;

			BUG_ON(NULL == dbs_data);
			BUG_ON(NULL == dbs_data->tuners);

			pr_debug("cpufreq_governor_dbs: min_sampling_rate = %d\n",
				 dbs_data->min_sampling_rate);
			pr_debug("cpufreq_governor_dbs: dbs_tuners_ins.sampling_rate = %d\n",
				 hp_tuners->sampling_rate);
			pr_debug("cpufreq_governor_dbs: dbs_tuners_ins.io_is_busy = %d\n",
				 hp_tuners->io_is_busy);
		}
#endif
#ifdef CONFIG_HOTPLUG_CPU
		if (0)		/* (!policy->cpu) // <-XXX */
			rc = input_register_handler(&dbs_input_handler);
#endif
		break;

	case CPUFREQ_GOV_STOP:
#ifdef CONFIG_HOTPLUG_CPU
		if (0)		/* (!policy->cpu) // <-XXX */
			input_unregister_handler(&dbs_input_handler);

#endif
		break;
	}

/* >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>> */
	return cpufreq_governor_dbs(policy, &hp_dbs_cdata, event);
}

/* <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<< */
#if 0
int cpufreq_gov_dbs_get_sum_load(void)
{
	return g_cpus_sum_load_current;
}
#endif
/* >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>> */

#ifndef CONFIG_CPU_FREQ_DEFAULT_GOV_HOTPLUG
static
#endif
struct cpufreq_governor cpufreq_gov_hotplug = {
	.name = "hotplug",
	.governor = hp_cpufreq_governor_dbs,
	.max_transition_latency = TRANSITION_LATENCY_LIMIT,
	.owner = THIS_MODULE,
};

#ifdef CONFIG_MTK_SDIOAUTOK_SUPPORT
void cpufreq_min_sampling_rate_change(unsigned int sample_rate)
{
	struct dbs_data *dbs_data = per_cpu(hp_cpu_dbs_info, 0).cdbs.cur_policy->governor_data;	/* TODO: FIXME, cpu = 0 */

	if (!dbs_data)
		return;

	dbs_data->min_sampling_rate = sample_rate;
	update_sampling_rate(dbs_data, sample_rate);
}
EXPORT_SYMBOL(cpufreq_min_sampling_rate_change);
#endif

static int __init cpufreq_gov_dbs_init(void)
{
	struct sched_param param = { .sched_priority = MAX_RT_PRIO-1 };

	freq_up_task = kthread_create(touch_freq_up_task, NULL, "touch_freq_up_task");

	if (IS_ERR(freq_up_task))
		return PTR_ERR(freq_up_task);

	sched_setscheduler_nocheck(freq_up_task, SCHED_FIFO, &param);
	get_task_struct(freq_up_task);

	return cpufreq_register_governor(&cpufreq_gov_hotplug);
}

static void __exit cpufreq_gov_dbs_exit(void)
{
	cpufreq_unregister_governor(&cpufreq_gov_hotplug);

	kthread_stop(freq_up_task);
	put_task_struct(freq_up_task);
}

MODULE_AUTHOR("Venkatesh Pallipadi <venkatesh.pallipadi@intel.com>");
MODULE_AUTHOR("Alexey Starikovskiy <alexey.y.starikovskiy@intel.com>");
MODULE_DESCRIPTION("'cpufreq_hotplug' - A dynamic cpufreq governor for "
		   "Low Latency Frequency Transition capable processors");
MODULE_LICENSE("GPL");

#ifdef CONFIG_CPU_FREQ_DEFAULT_GOV_HOTPLUG
fs_initcall(cpufreq_gov_dbs_init);
#else
module_init(cpufreq_gov_dbs_init);
#endif
module_exit(cpufreq_gov_dbs_exit);
