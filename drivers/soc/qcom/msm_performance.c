/*
 * Copyright (c) 2014-2015, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/init.h>
#include <linux/notifier.h>
#include <linux/cpu.h>
#include <linux/moduleparam.h>
#include <linux/cpumask.h>
#include <linux/cpufreq.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/tick.h>
#include <trace/events/power.h>
#include <linux/sysfs.h>
#include <linux/module.h>
#include <linux/kthread.h>

static struct mutex managed_cpus_lock;

/* Maximum number to clusters that this module will manage*/
static unsigned int num_clusters;
struct cluster {
	cpumask_var_t cpus;
	/* Number of CPUs to maintain online */
	int max_cpu_request;
	/* To track CPUs that the module decides to offline */
	cpumask_var_t offlined_cpus;
	/* stats for load detection */
	/* IO */
	u64 last_io_check_ts;
	unsigned int iowait_enter_cycle_cnt;
	unsigned int iowait_exit_cycle_cnt;
	spinlock_t iowait_lock;
	unsigned int cur_io_busy;
	bool io_change;
	/* CPU */
	unsigned int mode;
	bool mode_change;
	u64 last_mode_check_ts;
	unsigned int single_enter_cycle_cnt;
	unsigned int single_exit_cycle_cnt;
	unsigned int multi_enter_cycle_cnt;
	unsigned int multi_exit_cycle_cnt;
	spinlock_t mode_lock;
	/* Tunables */
	unsigned int single_enter_load;
	unsigned int pcpu_multi_enter_load;
	unsigned int single_exit_load;
	unsigned int pcpu_multi_exit_load;
	unsigned int single_enter_cycles;
	unsigned int single_exit_cycles;
	unsigned int multi_enter_cycles;
	unsigned int multi_exit_cycles;
	spinlock_t timer_lock;
	unsigned int timer_rate;
	struct timer_list mode_exit_timer;
};
static struct cluster **managed_clusters;
static bool clusters_inited;

/* Work to evaluate the onlining/offlining CPUs */
struct delayed_work evaluate_hotplug_work;

/* To handle cpufreq min/max request */
struct cpu_status {
	unsigned int min;
	unsigned int max;
};
static DEFINE_PER_CPU(struct cpu_status, cpu_stats);

static unsigned int num_online_managed(struct cpumask *mask);
static int init_cluster_control(void);
static int rm_high_pwr_cost_cpus(struct cluster *cl);
static int init_events_group(void);

static DEFINE_PER_CPU(unsigned int, cpu_power_cost);

struct load_stats {
	u64 last_wallclock;
	/* IO wait related */
	u64 last_iowait;
	unsigned int last_iopercent;
	/* CPU load related */
	unsigned int cpu_load;
};
static DEFINE_PER_CPU(struct load_stats, cpu_load_stats);

struct events {
	spinlock_t cpu_hotplug_lock;
	bool cpu_hotplug;
	bool init_success;
};
static struct events events_group;
static struct task_struct *events_notify_thread;

#define LAST_UPDATE_TOL		USEC_PER_MSEC

/* Bitmask to keep track of the workloads being detected */
static unsigned int workload_detect;
#define IO_DETECT	1
#define MODE_DETECT	2

/* IOwait related tunables */
static unsigned int io_enter_cycles = 4;
static unsigned int io_exit_cycles = 4;
static u64 iowait_ceiling_pct = 25;
static u64 iowait_floor_pct = 8;
#define LAST_IO_CHECK_TOL	(3 * USEC_PER_MSEC)

static unsigned int aggr_iobusy;
static unsigned int aggr_mode;

static struct task_struct *notify_thread;

/* CPU workload detection related */
#define NO_MODE		(0)
#define SINGLE		(1)
#define MULTI		(2)
#define MIXED		(3)
#define DEF_SINGLE_ENT		90
#define DEF_PCPU_MULTI_ENT	85
#define DEF_SINGLE_EX		60
#define DEF_PCPU_MULTI_EX	50
#define DEF_SINGLE_ENTER_CYCLE	4
#define DEF_SINGLE_EXIT_CYCLE	4
#define DEF_MULTI_ENTER_CYCLE	4
#define DEF_MULTI_EXIT_CYCLE	4
#define LAST_LD_CHECK_TOL	(2 * USEC_PER_MSEC)

/**************************sysfs start********************************/

static int set_num_clusters(const char *buf, const struct kernel_param *kp)
{
	unsigned int val;

	if (sscanf(buf, "%u\n", &val) != 1)
		return -EINVAL;
	if (num_clusters)
		return -EINVAL;

	num_clusters = val;

	if (init_cluster_control()) {
		num_clusters = 0;
		return -ENOMEM;
	}

	return 0;
}

static int get_num_clusters(char *buf, const struct kernel_param *kp)
{
	return snprintf(buf, PAGE_SIZE, "%u", num_clusters);
}

static const struct kernel_param_ops param_ops_num_clusters = {
	.set = set_num_clusters,
	.get = get_num_clusters,
};
device_param_cb(num_clusters, &param_ops_num_clusters, NULL, 0644);

static int set_max_cpus(const char *buf, const struct kernel_param *kp)
{
	unsigned int i, ntokens = 0;
	const char *cp = buf;
	int val;

	if (!clusters_inited)
		return -EINVAL;

	while ((cp = strpbrk(cp + 1, ":")))
		ntokens++;

	if (ntokens != (num_clusters - 1))
		return -EINVAL;

	cp = buf;
	for (i = 0; i < num_clusters; i++) {

		if (sscanf(cp, "%d\n", &val) != 1)
			return -EINVAL;
		if (val > (int)cpumask_weight(managed_clusters[i]->cpus))
			return -EINVAL;

		managed_clusters[i]->max_cpu_request = val;

		cp = strchr(cp, ':');
		cp++;
		trace_set_max_cpus(cpumask_bits(managed_clusters[i]->cpus)[0],
								val);
	}

	schedule_delayed_work(&evaluate_hotplug_work, 0);

	return 0;
}

static int get_max_cpus(char *buf, const struct kernel_param *kp)
{
	int i, cnt = 0;

	if (!clusters_inited)
		return cnt;

	for (i = 0; i < num_clusters; i++)
		cnt += snprintf(buf + cnt, PAGE_SIZE - cnt,
				"%d:", managed_clusters[i]->max_cpu_request);
	cnt--;
	cnt += snprintf(buf + cnt, PAGE_SIZE - cnt, " ");
	return cnt;
}

static const struct kernel_param_ops param_ops_max_cpus = {
	.set = set_max_cpus,
	.get = get_max_cpus,
};

device_param_cb(max_cpus, &param_ops_max_cpus, NULL, 0644);

static int set_managed_cpus(const char *buf, const struct kernel_param *kp)
{
	int i, ret;
	struct cpumask tmp_mask;

	if (!clusters_inited)
		return -EINVAL;

	ret = cpulist_parse(buf, &tmp_mask);

	if (ret)
		return ret;

	for (i = 0; i < num_clusters; i++) {
		if (cpumask_empty(managed_clusters[i]->cpus)) {
			mutex_lock(&managed_cpus_lock);
			cpumask_copy(managed_clusters[i]->cpus, &tmp_mask);
			cpumask_clear(managed_clusters[i]->offlined_cpus);
			mutex_unlock(&managed_cpus_lock);
			break;
		}
	}

	return ret;
}

static int get_managed_cpus(char *buf, const struct kernel_param *kp)
{
	int i, cnt = 0;

	if (!clusters_inited)
		return cnt;

	for (i = 0; i < num_clusters; i++) {
		cnt += cpulist_scnprintf(buf + cnt, PAGE_SIZE - cnt,
						managed_clusters[i]->cpus);
		if ((i + 1) >= num_clusters)
			break;
		cnt += snprintf(buf + cnt, PAGE_SIZE - cnt, ":");
	}

	return cnt;
}

static const struct kernel_param_ops param_ops_managed_cpus = {
	.set = set_managed_cpus,
	.get = get_managed_cpus,
};
device_param_cb(managed_cpus, &param_ops_managed_cpus, NULL, 0644);

/* Read-only node: To display all the online managed CPUs */
static int get_managed_online_cpus(char *buf, const struct kernel_param *kp)
{
	int i, cnt = 0;
	struct cpumask tmp_mask;
	struct cluster *i_cl;

	if (!clusters_inited)
		return cnt;

	for (i = 0; i < num_clusters; i++) {
		i_cl = managed_clusters[i];

		cpumask_clear(&tmp_mask);
		cpumask_complement(&tmp_mask, i_cl->offlined_cpus);
		cpumask_and(&tmp_mask, i_cl->cpus, &tmp_mask);

		cnt += cpulist_scnprintf(buf + cnt, PAGE_SIZE - cnt,
								&tmp_mask);

		if ((i + 1) >= num_clusters)
			break;
		cnt += snprintf(buf + cnt, PAGE_SIZE - cnt, ":");
	}

	return cnt;
}

static const struct kernel_param_ops param_ops_managed_online_cpus = {
	.get = get_managed_online_cpus,
};
device_param_cb(managed_online_cpus, &param_ops_managed_online_cpus,
								NULL, 0444);

/*
 * Userspace sends cpu#:min_freq_value to vote for min_freq_value as the new
 * scaling_min. To withdraw its vote it needs to enter cpu#:0
 */
static int set_cpu_min_freq(const char *buf, const struct kernel_param *kp)
{
	int i, j, ntokens = 0;
	unsigned int val, cpu;
	const char *cp = buf;
	struct cpu_status *i_cpu_stats;
	struct cpufreq_policy policy;
	cpumask_var_t limit_mask;
	int ret;

	while ((cp = strpbrk(cp + 1, " :")))
		ntokens++;

	/* CPU:value pair */
	if (!(ntokens % 2))
		return -EINVAL;

	cp = buf;
	cpumask_clear(limit_mask);
	for (i = 0; i < ntokens; i += 2) {
		if (sscanf(cp, "%u:%u", &cpu, &val) != 2)
			return -EINVAL;
		if (cpu > (num_present_cpus() - 1))
			return -EINVAL;

		i_cpu_stats = &per_cpu(cpu_stats, cpu);

		i_cpu_stats->min = val;
		cpumask_set_cpu(cpu, limit_mask);

		cp = strchr(cp, ' ');
		cp++;
	}

	/*
	 * Since on synchronous systems policy is shared amongst multiple
	 * CPUs only one CPU needs to be updated for the limit to be
	 * reflected for the entire cluster. We can avoid updating the policy
	 * of other CPUs in the cluster once it is done for at least one CPU
	 * in the cluster
	 */
	get_online_cpus();
	for_each_cpu(i, limit_mask) {
		i_cpu_stats = &per_cpu(cpu_stats, i);

		if (cpufreq_get_policy(&policy, i))
			continue;

		if (cpu_online(i) && (policy.min != i_cpu_stats->min)) {
			ret = cpufreq_update_policy(i);
			if (ret)
				continue;
		}
		for_each_cpu(j, policy.related_cpus)
			cpumask_clear_cpu(j, limit_mask);
	}
	put_online_cpus();

	return 0;
}

static int get_cpu_min_freq(char *buf, const struct kernel_param *kp)
{
	int cnt = 0, cpu;

	for_each_present_cpu(cpu) {
		cnt += snprintf(buf + cnt, PAGE_SIZE - cnt,
				"%d:%u ", cpu, per_cpu(cpu_stats, cpu).min);
	}
	cnt += snprintf(buf + cnt, PAGE_SIZE - cnt, "\n");
	return cnt;
}

static const struct kernel_param_ops param_ops_cpu_min_freq = {
	.set = set_cpu_min_freq,
	.get = get_cpu_min_freq,
};
module_param_cb(cpu_min_freq, &param_ops_cpu_min_freq, NULL, 0644);

/*
 * Userspace sends cpu#:max_freq_value to vote for max_freq_value as the new
 * scaling_max. To withdraw its vote it needs to enter cpu#:UINT_MAX
 */
static int set_cpu_max_freq(const char *buf, const struct kernel_param *kp)
{
	int i, j, ntokens = 0;
	unsigned int val, cpu;
	const char *cp = buf;
	struct cpu_status *i_cpu_stats;
	struct cpufreq_policy policy;
	cpumask_var_t limit_mask;
	int ret;

	while ((cp = strpbrk(cp + 1, " :")))
		ntokens++;

	/* CPU:value pair */
	if (!(ntokens % 2))
		return -EINVAL;

	cp = buf;
	cpumask_clear(limit_mask);
	for (i = 0; i < ntokens; i += 2) {
		if (sscanf(cp, "%u:%u", &cpu, &val) != 2)
			return -EINVAL;
		if (cpu > (num_present_cpus() - 1))
			return -EINVAL;

		i_cpu_stats = &per_cpu(cpu_stats, cpu);

		i_cpu_stats->max = val;
		cpumask_set_cpu(cpu, limit_mask);

		cp = strchr(cp, ' ');
		cp++;
	}

	get_online_cpus();
	for_each_cpu(i, limit_mask) {
		i_cpu_stats = &per_cpu(cpu_stats, i);
		if (cpufreq_get_policy(&policy, i))
			continue;

		if (cpu_online(i) && (policy.max != i_cpu_stats->max)) {
			ret = cpufreq_update_policy(i);
			if (ret)
				continue;
		}
		for_each_cpu(j, policy.related_cpus)
			cpumask_clear_cpu(j, limit_mask);
	}
	put_online_cpus();

	return 0;
}

static int get_cpu_max_freq(char *buf, const struct kernel_param *kp)
{
	int cnt = 0, cpu;

	for_each_present_cpu(cpu) {
		cnt += snprintf(buf + cnt, PAGE_SIZE - cnt,
				"%d:%u ", cpu, per_cpu(cpu_stats, cpu).max);
	}
	cnt += snprintf(buf + cnt, PAGE_SIZE - cnt, "\n");
	return cnt;
}

static const struct kernel_param_ops param_ops_cpu_max_freq = {
	.set = set_cpu_max_freq,
	.get = get_cpu_max_freq,
};
module_param_cb(cpu_max_freq, &param_ops_cpu_max_freq, NULL, 0644);

static int set_single_enter_load(const char *buf, const struct kernel_param *kp)
{
	unsigned int val, i, ntokens = 0;
	const char *cp = buf;
	unsigned int bytes_left;

	if (!clusters_inited)
		return -EINVAL;

	while ((cp = strpbrk(cp + 1, ":")))
		ntokens++;

	if (ntokens != (num_clusters - 1))
		return -EINVAL;

	cp = buf;
	for (i = 0; i < num_clusters; i++) {

		if (sscanf(cp, "%u\n", &val) != 1)
			return -EINVAL;

		if (val < managed_clusters[i]->single_exit_load)
			return -EINVAL;

		managed_clusters[i]->single_enter_load = val;

		bytes_left = PAGE_SIZE - (cp - buf);
		cp = strnchr(cp, bytes_left, ':');
		cp++;
	}

	return 0;
}

static int get_single_enter_load(char *buf, const struct kernel_param *kp)
{
	int i, cnt = 0;

	if (!clusters_inited)
		return cnt;

	for (i = 0; i < num_clusters; i++)
		cnt += snprintf(buf + cnt, PAGE_SIZE - cnt,
				"%u:", managed_clusters[i]->single_enter_load);
	cnt--;
	cnt += snprintf(buf + cnt, PAGE_SIZE - cnt, " ");
	return cnt;
}

static const struct kernel_param_ops param_ops_single_enter_load = {
	.set = set_single_enter_load,
	.get = get_single_enter_load,
};
device_param_cb(single_enter_load, &param_ops_single_enter_load, NULL, 0644);

static int set_single_exit_load(const char *buf, const struct kernel_param *kp)
{
	unsigned int val, i, ntokens = 0;
	const char *cp = buf;
	unsigned int bytes_left;

	if (!clusters_inited)
		return -EINVAL;

	while ((cp = strpbrk(cp + 1, ":")))
		ntokens++;

	if (ntokens != (num_clusters - 1))
		return -EINVAL;

	cp = buf;
	for (i = 0; i < num_clusters; i++) {

		if (sscanf(cp, "%u\n", &val) != 1)
			return -EINVAL;

		if (val > managed_clusters[i]->single_enter_load)
			return -EINVAL;

		managed_clusters[i]->single_exit_load = val;

		bytes_left = PAGE_SIZE - (cp - buf);
		cp = strnchr(cp, bytes_left, ':');
		cp++;
	}

	return 0;
}

static int get_single_exit_load(char *buf, const struct kernel_param *kp)
{
	int i, cnt = 0;

	if (!clusters_inited)
		return cnt;

	for (i = 0; i < num_clusters; i++)
		cnt += snprintf(buf + cnt, PAGE_SIZE - cnt,
				"%u:", managed_clusters[i]->single_exit_load);
	cnt--;
	cnt += snprintf(buf + cnt, PAGE_SIZE - cnt, " ");
	return cnt;
}

static const struct kernel_param_ops param_ops_single_exit_load = {
	.set = set_single_exit_load,
	.get = get_single_exit_load,
};
device_param_cb(single_exit_load, &param_ops_single_exit_load, NULL, 0644);

static int set_pcpu_multi_enter_load(const char *buf,
					const struct kernel_param *kp)
{
	unsigned int val, i, ntokens = 0;
	const char *cp = buf;
	unsigned int bytes_left;

	if (!clusters_inited)
		return -EINVAL;

	while ((cp = strpbrk(cp + 1, ":")))
		ntokens++;

	if (ntokens != (num_clusters - 1))
		return -EINVAL;

	cp = buf;
	for (i = 0; i < num_clusters; i++) {

		if (sscanf(cp, "%u\n", &val) != 1)
			return -EINVAL;

		if (val < managed_clusters[i]->pcpu_multi_exit_load)
			return -EINVAL;

		managed_clusters[i]->pcpu_multi_enter_load = val;

		bytes_left = PAGE_SIZE - (cp - buf);
		cp = strnchr(cp, bytes_left, ':');
		cp++;
	}

	return 0;
}

static int get_pcpu_multi_enter_load(char *buf, const struct kernel_param *kp)
{
	int i, cnt = 0;

	if (!clusters_inited)
		return cnt;

	for (i = 0; i < num_clusters; i++)
		cnt += snprintf(buf + cnt, PAGE_SIZE - cnt,
			"%u:", managed_clusters[i]->pcpu_multi_enter_load);
	cnt--;
	cnt += snprintf(buf + cnt, PAGE_SIZE - cnt, " ");
	return cnt;
}

static const struct kernel_param_ops param_ops_pcpu_multi_enter_load = {
	.set = set_pcpu_multi_enter_load,
	.get = get_pcpu_multi_enter_load,
};
device_param_cb(pcpu_multi_enter_load, &param_ops_pcpu_multi_enter_load,
								NULL, 0644);

static int set_pcpu_multi_exit_load(const char *buf,
						const struct kernel_param *kp)
{
	unsigned int val, i, ntokens = 0;
	const char *cp = buf;
	unsigned int bytes_left;

	if (!clusters_inited)
		return -EINVAL;

	while ((cp = strpbrk(cp + 1, ":")))
		ntokens++;

	if (ntokens != (num_clusters - 1))
		return -EINVAL;

	cp = buf;
	for (i = 0; i < num_clusters; i++) {

		if (sscanf(cp, "%u\n", &val) != 1)
			return -EINVAL;

		if (val > managed_clusters[i]->pcpu_multi_enter_load)
			return -EINVAL;

		managed_clusters[i]->pcpu_multi_exit_load = val;

		bytes_left = PAGE_SIZE - (cp - buf);
		cp = strnchr(cp, bytes_left, ':');
		cp++;
	}

	return 0;
}

static int get_pcpu_multi_exit_load(char *buf, const struct kernel_param *kp)
{
	int i, cnt = 0;

	if (!clusters_inited)
		return cnt;

	for (i = 0; i < num_clusters; i++)
		cnt += snprintf(buf + cnt, PAGE_SIZE - cnt,
			"%u:", managed_clusters[i]->pcpu_multi_exit_load);
	cnt--;
	cnt += snprintf(buf + cnt, PAGE_SIZE - cnt, " ");
	return cnt;
}

static const struct kernel_param_ops param_ops_pcpu_multi_exit_load = {
	.set = set_pcpu_multi_exit_load,
	.get = get_pcpu_multi_exit_load,
};
device_param_cb(pcpu_multi_exit_load, &param_ops_pcpu_multi_exit_load,
								NULL, 0644);

static int set_single_enter_cycles(const char *buf,
					const struct kernel_param *kp)
{
	unsigned int val, i, ntokens = 0;
	const char *cp = buf;
	unsigned int bytes_left;

	if (!clusters_inited)
		return -EINVAL;

	while ((cp = strpbrk(cp + 1, ":")))
		ntokens++;

	if (ntokens != (num_clusters - 1))
		return -EINVAL;

	cp = buf;
	for (i = 0; i < num_clusters; i++) {

		if (sscanf(cp, "%u\n", &val) != 1)
			return -EINVAL;

		managed_clusters[i]->single_enter_cycles = val;

		bytes_left = PAGE_SIZE - (cp - buf);
		cp = strnchr(cp, bytes_left, ':');
		cp++;
	}

	return 0;
}

static int get_single_enter_cycles(char *buf, const struct kernel_param *kp)
{
	int i, cnt = 0;

	if (!clusters_inited)
		return cnt;

	for (i = 0; i < num_clusters; i++)
		cnt += snprintf(buf + cnt, PAGE_SIZE - cnt, "%u:",
				managed_clusters[i]->single_enter_cycles);
	cnt--;
	cnt += snprintf(buf + cnt, PAGE_SIZE - cnt, " ");
	return cnt;
}

static const struct kernel_param_ops param_ops_single_enter_cycles = {
	.set = set_single_enter_cycles,
	.get = get_single_enter_cycles,
};
device_param_cb(single_enter_cycles, &param_ops_single_enter_cycles,
								NULL, 0644);


static int set_single_exit_cycles(const char *buf,
					const struct kernel_param *kp)
{
	unsigned int val, i, ntokens = 0;
	const char *cp = buf;
	unsigned int bytes_left;

	if (!clusters_inited)
		return -EINVAL;

	while ((cp = strpbrk(cp + 1, ":")))
		ntokens++;

	if (ntokens != (num_clusters - 1))
		return -EINVAL;

	cp = buf;
	for (i = 0; i < num_clusters; i++) {

		if (sscanf(cp, "%u\n", &val) != 1)
			return -EINVAL;

		managed_clusters[i]->single_exit_cycles = val;

		bytes_left = PAGE_SIZE - (cp - buf);
		cp = strnchr(cp, bytes_left, ':');
		cp++;
	}

	return 0;
}

static int get_single_exit_cycles(char *buf, const struct kernel_param *kp)
{
	int i, cnt = 0;

	if (!clusters_inited)
		return cnt;

	for (i = 0; i < num_clusters; i++)
		cnt += snprintf(buf + cnt, PAGE_SIZE - cnt,
				"%u:", managed_clusters[i]->single_exit_cycles);
	cnt--;
	cnt += snprintf(buf + cnt, PAGE_SIZE - cnt, " ");
	return cnt;
}

static const struct kernel_param_ops param_ops_single_exit_cycles = {
	.set = set_single_exit_cycles,
	.get = get_single_exit_cycles,
};
device_param_cb(single_exit_cycles, &param_ops_single_exit_cycles, NULL, 0644);

static int set_multi_enter_cycles(const char *buf,
					const struct kernel_param *kp)
{
	unsigned int val, i, ntokens = 0;
	const char *cp = buf;
	unsigned int bytes_left;

	if (!clusters_inited)
		return -EINVAL;

	while ((cp = strpbrk(cp + 1, ":")))
		ntokens++;

	if (ntokens != (num_clusters - 1))
		return -EINVAL;

	cp = buf;
	for (i = 0; i < num_clusters; i++) {

		if (sscanf(cp, "%u\n", &val) != 1)
			return -EINVAL;

		managed_clusters[i]->multi_enter_cycles = val;

		bytes_left = PAGE_SIZE - (cp - buf);
		cp = strnchr(cp, bytes_left, ':');
		cp++;
	}

	return 0;
}

static int get_multi_enter_cycles(char *buf, const struct kernel_param *kp)
{
	int i, cnt = 0;

	if (!clusters_inited)
		return cnt;

	for (i = 0; i < num_clusters; i++)
		cnt += snprintf(buf + cnt, PAGE_SIZE - cnt,
				"%u:", managed_clusters[i]->multi_enter_cycles);
	cnt--;
	cnt += snprintf(buf + cnt, PAGE_SIZE - cnt, " ");
	return cnt;
}

static const struct kernel_param_ops param_ops_multi_enter_cycles = {
	.set = set_multi_enter_cycles,
	.get = get_multi_enter_cycles,
};
device_param_cb(multi_enter_cycles, &param_ops_multi_enter_cycles, NULL, 0644);

static int set_multi_exit_cycles(const char *buf, const struct kernel_param *kp)
{
	unsigned int val, i, ntokens = 0;
	const char *cp = buf;
	unsigned int bytes_left;

	if (!clusters_inited)
		return -EINVAL;

	while ((cp = strpbrk(cp + 1, ":")))
		ntokens++;

	if (ntokens != (num_clusters - 1))
		return -EINVAL;

	cp = buf;
	for (i = 0; i < num_clusters; i++) {

		if (sscanf(cp, "%u\n", &val) != 1)
			return -EINVAL;

		managed_clusters[i]->multi_exit_cycles = val;

		bytes_left = PAGE_SIZE - (cp - buf);
		cp = strnchr(cp, bytes_left, ':');
		cp++;
	}

	return 0;
}

static int get_multi_exit_cycles(char *buf, const struct kernel_param *kp)
{
	int i, cnt = 0;

	if (!clusters_inited)
		return cnt;

	for (i = 0; i < num_clusters; i++)
		cnt += snprintf(buf + cnt, PAGE_SIZE - cnt,
				"%u:", managed_clusters[i]->multi_exit_cycles);
	cnt--;
	cnt += snprintf(buf + cnt, PAGE_SIZE - cnt, " ");
	return cnt;
}

static const struct kernel_param_ops param_ops_multi_exit_cycles = {
	.set = set_multi_exit_cycles,
	.get = get_multi_exit_cycles,
};
device_param_cb(multi_exit_cycles, &param_ops_multi_exit_cycles, NULL, 0644);

static int set_io_enter_cycles(const char *buf, const struct kernel_param *kp)
{
	unsigned int val;

	if (sscanf(buf, "%u\n", &val) != 1)
		return -EINVAL;

	io_enter_cycles = val;

	return 0;
}

static int get_io_enter_cycles(char *buf, const struct kernel_param *kp)
{
	return snprintf(buf, PAGE_SIZE, "%u", io_enter_cycles);
}

static const struct kernel_param_ops param_ops_io_enter_cycles = {
	.set = set_io_enter_cycles,
	.get = get_io_enter_cycles,
};
device_param_cb(io_enter_cycles, &param_ops_io_enter_cycles, NULL, 0644);

static int set_io_exit_cycles(const char *buf, const struct kernel_param *kp)
{
	unsigned int val;

	if (sscanf(buf, "%u\n", &val) != 1)
		return -EINVAL;

	io_exit_cycles = val;

	return 0;
}

static int get_io_exit_cycles(char *buf, const struct kernel_param *kp)
{
	return snprintf(buf, PAGE_SIZE, "%u", io_exit_cycles);
}

static const struct kernel_param_ops param_ops_io_exit_cycles = {
	.set = set_io_exit_cycles,
	.get = get_io_exit_cycles,
};
device_param_cb(io_exit_cycles, &param_ops_io_exit_cycles, NULL, 0644);

static int set_iowait_floor_pct(const char *buf, const struct kernel_param *kp)
{
	u64 val;

	if (sscanf(buf, "%llu\n", &val) != 1)
		return -EINVAL;
	if (val > iowait_ceiling_pct)
		return -EINVAL;

	iowait_floor_pct = val;

	return 0;
}

static int get_iowait_floor_pct(char *buf, const struct kernel_param *kp)
{
	return snprintf(buf, PAGE_SIZE, "%llu", iowait_floor_pct);
}

static const struct kernel_param_ops param_ops_iowait_floor_pct = {
	.set = set_iowait_floor_pct,
	.get = get_iowait_floor_pct,
};
device_param_cb(iowait_floor_pct, &param_ops_iowait_floor_pct, NULL, 0644);

static int set_iowait_ceiling_pct(const char *buf,
						const struct kernel_param *kp)
{
	u64 val;

	if (sscanf(buf, "%llu\n", &val) != 1)
		return -EINVAL;
	if (val < iowait_floor_pct)
		return -EINVAL;

	iowait_ceiling_pct = val;

	return 0;
}

static int get_iowait_ceiling_pct(char *buf, const struct kernel_param *kp)
{
	return snprintf(buf, PAGE_SIZE, "%llu", iowait_ceiling_pct);
}

static const struct kernel_param_ops param_ops_iowait_ceiling_pct = {
	.set = set_iowait_ceiling_pct,
	.get = get_iowait_ceiling_pct,
};
device_param_cb(iowait_ceiling_pct, &param_ops_iowait_ceiling_pct, NULL, 0644);

static int set_workload_detect(const char *buf, const struct kernel_param *kp)
{
	unsigned int val, i;
	struct cluster *i_cl;
	unsigned long flags;

	if (!clusters_inited)
		return -EINVAL;

	if (sscanf(buf, "%u\n", &val) != 1)
		return -EINVAL;

	if (val == workload_detect)
		return 0;

	workload_detect = val;

	if (!(workload_detect & IO_DETECT)) {
		for (i = 0; i < num_clusters; i++) {
			i_cl = managed_clusters[i];
			spin_lock_irqsave(&i_cl->iowait_lock, flags);
			i_cl->iowait_enter_cycle_cnt = 0;
			i_cl->iowait_exit_cycle_cnt = 0;
			i_cl->cur_io_busy = 0;
			i_cl->io_change = true;
			spin_unlock_irqrestore(&i_cl->iowait_lock, flags);
		}
	}
	if (!(workload_detect & MODE_DETECT)) {
		for (i = 0; i < num_clusters; i++) {
			i_cl = managed_clusters[i];
			spin_lock_irqsave(&i_cl->mode_lock, flags);
			i_cl->single_enter_cycle_cnt = 0;
			i_cl->single_exit_cycle_cnt = 0;
			i_cl->multi_enter_cycle_cnt = 0;
			i_cl->multi_exit_cycle_cnt = 0;
			i_cl->mode = 0;
			i_cl->mode_change = true;
			spin_unlock_irqrestore(&i_cl->mode_lock, flags);
		}
	}

	wake_up_process(notify_thread);
	return 0;
}

static int get_workload_detect(char *buf, const struct kernel_param *kp)
{
	return snprintf(buf, PAGE_SIZE, "%u", workload_detect);
}

static const struct kernel_param_ops param_ops_workload_detect = {
	.set = set_workload_detect,
	.get = get_workload_detect,
};
device_param_cb(workload_detect, &param_ops_workload_detect, NULL, 0644);

static struct kobject *mode_kobj;

static ssize_t show_aggr_mode(struct kobject *kobj,
					struct kobj_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%u\n", aggr_mode);
}
static struct kobj_attribute aggr_mode_attr =
__ATTR(aggr_mode, 0444, show_aggr_mode, NULL);

static ssize_t show_aggr_iobusy(struct kobject *kobj,
					struct kobj_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%u\n", aggr_iobusy);
}
static struct kobj_attribute aggr_iobusy_attr =
__ATTR(aggr_iobusy, 0444, show_aggr_iobusy, NULL);

static struct attribute *attrs[] = {
	&aggr_mode_attr.attr,
	&aggr_iobusy_attr.attr,
	NULL,
};

static struct attribute_group attr_group = {
	.attrs = attrs,
};

/* CPU Hotplug */
static struct kobject *events_kobj;

static ssize_t show_cpu_hotplug(struct kobject *kobj,
					struct kobj_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "\n");
}
static struct kobj_attribute cpu_hotplug_attr =
__ATTR(cpu_hotplug, 0444, show_cpu_hotplug, NULL);

static struct attribute *events_attrs[] = {
	&cpu_hotplug_attr.attr,
	NULL,
};

static struct attribute_group events_attr_group = {
	.attrs = events_attrs,
};
/*******************************sysfs ends************************************/

static unsigned int num_online_managed(struct cpumask *mask)
{
	struct cpumask tmp_mask;

	cpumask_clear(&tmp_mask);
	cpumask_and(&tmp_mask, mask, cpu_online_mask);

	return cpumask_weight(&tmp_mask);
}

static int perf_adjust_notify(struct notifier_block *nb, unsigned long val,
							void *data)
{
	struct cpufreq_policy *policy = data;
	unsigned int cpu = policy->cpu;
	struct cpu_status *cpu_st = &per_cpu(cpu_stats, cpu);
	unsigned int min = cpu_st->min, max = cpu_st->max;


	if (val != CPUFREQ_ADJUST)
		return NOTIFY_OK;

	pr_debug("msm_perf: CPU%u policy before: %u:%u kHz\n", cpu,
						policy->min, policy->max);
	pr_debug("msm_perf: CPU%u seting min:max %u:%u kHz\n", cpu, min, max);

	cpufreq_verify_within_limits(policy, min, max);

	pr_debug("msm_perf: CPU%u policy after: %u:%u kHz\n", cpu,
						policy->min, policy->max);

	return NOTIFY_OK;
}

static struct notifier_block perf_cpufreq_nb = {
	.notifier_call = perf_adjust_notify,
};

static bool check_notify_status(void)
{
	int i;
	struct cluster *cl;
	bool any_change = false;
	unsigned long flags;

	for (i = 0; i < num_clusters; i++) {
		cl = managed_clusters[i];
		spin_lock_irqsave(&cl->iowait_lock, flags);
		if (!any_change)
			any_change = cl->io_change;
		cl->io_change = false;
		spin_unlock_irqrestore(&cl->iowait_lock, flags);

		spin_lock_irqsave(&cl->mode_lock, flags);
		if (!any_change)
			any_change = cl->mode_change;
		cl->mode_change = false;
		spin_unlock_irqrestore(&cl->mode_lock, flags);
	}

	return any_change;
}

static int notify_userspace(void *data)
{
	unsigned int i, io, cpu_mode;

	while (1) {
		set_current_state(TASK_INTERRUPTIBLE);
		if (!check_notify_status()) {
			schedule();

			if (kthread_should_stop())
				break;
		}
		set_current_state(TASK_RUNNING);

		io = 0;
		cpu_mode = 0;
		for (i = 0; i < num_clusters; i++) {
			io |= managed_clusters[i]->cur_io_busy;
			cpu_mode |= managed_clusters[i]->mode;
		}

		if (io != aggr_iobusy) {
			aggr_iobusy = io;
			sysfs_notify(mode_kobj, NULL, "aggr_iobusy");
			pr_debug("msm_perf: Notifying IO: %u\n", aggr_iobusy);
		}

		if (cpu_mode != aggr_mode) {
			aggr_mode = cpu_mode;
			sysfs_notify(mode_kobj, NULL, "aggr_mode");
			pr_debug("msm_perf: Notifying CPU mode:%u\n",
								aggr_mode);
		}
	}

	return 0;
}

static void hotplug_notify(int action)
{
	unsigned long flags;

	if (!events_group.init_success)
		return;

	if ((action == CPU_ONLINE) || (action == CPU_DEAD)) {
		spin_lock_irqsave(&(events_group.cpu_hotplug_lock), flags);
		events_group.cpu_hotplug = true;
		spin_unlock_irqrestore(&(events_group.cpu_hotplug_lock), flags);
		wake_up_process(events_notify_thread);
	}
}

static int events_notify_userspace(void *data)
{
	unsigned long flags;
	bool notify_change;

	while (1) {

		set_current_state(TASK_INTERRUPTIBLE);
		spin_lock_irqsave(&(events_group.cpu_hotplug_lock), flags);

		if (!events_group.cpu_hotplug) {
			spin_unlock_irqrestore(&(events_group.cpu_hotplug_lock),
									flags);

			schedule();
			if (kthread_should_stop())
				break;
			spin_lock_irqsave(&(events_group.cpu_hotplug_lock),
									flags);
		}

		set_current_state(TASK_RUNNING);
		notify_change = events_group.cpu_hotplug;
		events_group.cpu_hotplug = false;
		spin_unlock_irqrestore(&(events_group.cpu_hotplug_lock), flags);

		if (notify_change)
			sysfs_notify(events_kobj, NULL, "cpu_hotplug");
	}

	return 0;
}

static void check_cluster_iowait(struct cluster *cl, u64 now)
{
	struct load_stats *pcpu_st;
	unsigned int i;
	unsigned long flags;
	unsigned int temp_iobusy;
	u64 max_iowait = 0;

	spin_lock_irqsave(&cl->iowait_lock, flags);

	if (((now - cl->last_io_check_ts)
		< (cl->timer_rate - LAST_IO_CHECK_TOL)) ||
		!(workload_detect & IO_DETECT)) {
		spin_unlock_irqrestore(&cl->iowait_lock, flags);
		return;
	}

	temp_iobusy = cl->cur_io_busy;
	for_each_cpu(i, cl->cpus) {
		pcpu_st = &per_cpu(cpu_load_stats, i);
		if ((now - pcpu_st->last_wallclock)
			> (cl->timer_rate + LAST_UPDATE_TOL))
			continue;
		if (max_iowait < pcpu_st->last_iopercent)
			max_iowait = pcpu_st->last_iopercent;
	}

	if (!cl->cur_io_busy) {
		if (max_iowait > iowait_ceiling_pct) {
			cl->iowait_enter_cycle_cnt++;
			if (cl->iowait_enter_cycle_cnt >= io_enter_cycles) {
				cl->cur_io_busy = 1;
				cl->iowait_enter_cycle_cnt = 0;
			}
		} else {
			cl->iowait_enter_cycle_cnt = 0;
		}
	} else {
		if (max_iowait < iowait_floor_pct) {
			cl->iowait_exit_cycle_cnt++;
			if (cl->iowait_exit_cycle_cnt >= io_exit_cycles) {
				cl->cur_io_busy = 0;
				cl->iowait_exit_cycle_cnt = 0;
			}
		} else {
			cl->iowait_exit_cycle_cnt = 0;
		}
	}

	cl->last_io_check_ts = now;
	trace_track_iowait(cpumask_first(cl->cpus), cl->iowait_enter_cycle_cnt,
			cl->iowait_exit_cycle_cnt, cl->cur_io_busy, max_iowait);

	if (temp_iobusy != cl->cur_io_busy) {
		cl->io_change = true;
		pr_debug("msm_perf: IO changed to %u\n", cl->cur_io_busy);
	}

	spin_unlock_irqrestore(&cl->iowait_lock, flags);
	if (cl->io_change)
		wake_up_process(notify_thread);
}

static void disable_timer(struct cluster *cl)
{
	unsigned long flags;

	spin_lock_irqsave(&cl->timer_lock, flags);

	if (del_timer(&cl->mode_exit_timer)) {
		trace_single_cycle_exit_timer_stop(cpumask_first(cl->cpus),
			cl->single_enter_cycles, cl->single_enter_cycle_cnt,
			cl->single_exit_cycles, cl->single_exit_cycle_cnt,
			cl->multi_enter_cycles, cl->multi_enter_cycle_cnt,
			cl->multi_exit_cycles, cl->multi_exit_cycle_cnt,
			cl->timer_rate, cl->mode);
	}

	spin_unlock_irqrestore(&cl->timer_lock, flags);
}

static void start_timer(struct cluster *cl)
{
	unsigned long flags;

	spin_lock_irqsave(&cl->timer_lock, flags);
	if ((cl->mode & SINGLE) && !timer_pending(&cl->mode_exit_timer)) {
		/*Set timer for the Cluster since there is none pending*/
		cl->mode_exit_timer.expires = get_jiffies_64() +
		usecs_to_jiffies(cl->single_exit_cycles * cl->timer_rate);
		cl->mode_exit_timer.data = cpumask_first(cl->cpus);
		add_timer(&cl->mode_exit_timer);
		trace_single_cycle_exit_timer_start(cpumask_first(cl->cpus),
			cl->single_enter_cycles, cl->single_enter_cycle_cnt,
			cl->single_exit_cycles, cl->single_exit_cycle_cnt,
			cl->multi_enter_cycles, cl->multi_enter_cycle_cnt,
			cl->multi_exit_cycles, cl->multi_exit_cycle_cnt,
			cl->timer_rate, cl->mode);
	}
	spin_unlock_irqrestore(&cl->timer_lock, flags);
}

static void check_cpu_load(struct cluster *cl, u64 now)
{
	struct load_stats *pcpu_st;
	unsigned int i, max_load = 0, total_load = 0, ret_mode, cpu_cnt = 0;
	unsigned int total_load_ceil, total_load_floor;
	unsigned long flags;

	spin_lock_irqsave(&cl->mode_lock, flags);

	if (((now - cl->last_mode_check_ts)
		< (cl->timer_rate - LAST_LD_CHECK_TOL)) ||
		!(workload_detect & MODE_DETECT)) {
		spin_unlock_irqrestore(&cl->mode_lock, flags);
		return;
	}

	for_each_cpu(i, cl->cpus) {
		pcpu_st = &per_cpu(cpu_load_stats, i);
		if ((now - pcpu_st->last_wallclock)
			> (cl->timer_rate + LAST_UPDATE_TOL))
			continue;
		if (pcpu_st->cpu_load > max_load)
			max_load = pcpu_st->cpu_load;
		total_load += pcpu_st->cpu_load;
		cpu_cnt++;
	}

	if (cpu_cnt > 1) {
		total_load_ceil = cl->pcpu_multi_enter_load * cpu_cnt;
		total_load_floor = cl->pcpu_multi_exit_load * cpu_cnt;
	} else {
		total_load_ceil = UINT_MAX;
		total_load_floor = UINT_MAX;
	}

	ret_mode = cl->mode;
	if (!(cl->mode & SINGLE)) {
		if (max_load >= cl->single_enter_load) {
			cl->single_enter_cycle_cnt++;
			if (cl->single_enter_cycle_cnt
				>= cl->single_enter_cycles) {
				ret_mode |= SINGLE;
				cl->single_enter_cycle_cnt = 0;
			}
		} else {
			cl->single_enter_cycle_cnt = 0;
		}
	} else {
		if (max_load < cl->single_exit_load) {
			start_timer(cl);
			cl->single_exit_cycle_cnt++;
			if (cl->single_exit_cycle_cnt
				>= cl->single_exit_cycles) {
				ret_mode &= ~SINGLE;
				cl->single_exit_cycle_cnt = 0;
				disable_timer(cl);
			}
		} else {
			cl->single_exit_cycle_cnt = 0;
			disable_timer(cl);
		}
	}

	if (!(cl->mode & MULTI)) {
		if (total_load >= total_load_ceil) {
			cl->multi_enter_cycle_cnt++;
			if (cl->multi_enter_cycle_cnt
				>= cl->multi_enter_cycles) {
				ret_mode |= MULTI;
				cl->multi_enter_cycle_cnt = 0;
			}
		} else {
			cl->multi_enter_cycle_cnt = 0;
		}
	} else {
		if (total_load < total_load_floor) {
			cl->multi_exit_cycle_cnt++;
			if (cl->multi_exit_cycle_cnt
				>= cl->multi_exit_cycles) {
				ret_mode &= ~MULTI;
				cl->multi_exit_cycle_cnt = 0;
			}
		} else {
			cl->multi_exit_cycle_cnt = 0;
		}
	}

	cl->last_mode_check_ts = now;

	if (ret_mode != cl->mode) {
		cl->mode = ret_mode;
		cl->mode_change = true;
		pr_debug("msm_perf: Mode changed to %u\n", ret_mode);
	}

	trace_cpu_mode_detect(cpumask_first(cl->cpus), max_load,
		cl->single_enter_cycle_cnt, cl->single_exit_cycle_cnt,
		total_load, cl->multi_enter_cycle_cnt,
		cl->multi_exit_cycle_cnt, cl->mode, cpu_cnt);

	spin_unlock_irqrestore(&cl->mode_lock, flags);

	if (cl->mode_change)
		wake_up_process(notify_thread);
}

static void check_workload_stats(unsigned int cpu, unsigned int rate, u64 now)
{
	struct cluster *cl = NULL;
	unsigned int i;

	for (i = 0; i < num_clusters; i++) {
		if (cpumask_test_cpu(cpu, managed_clusters[i]->cpus)) {
			cl = managed_clusters[i];
			break;
		}
	}
	if (cl == NULL)
		return;

	cl->timer_rate = rate;
	check_cluster_iowait(cl, now);
	check_cpu_load(cl, now);
}

static int perf_govinfo_notify(struct notifier_block *nb, unsigned long val,
								void *data)
{
	struct cpufreq_govinfo *gov_info = data;
	unsigned int cpu = gov_info->cpu;
	struct load_stats *cpu_st = &per_cpu(cpu_load_stats, cpu);
	u64 now, cur_iowait, time_diff, iowait_diff;

	if (!clusters_inited || !workload_detect)
		return NOTIFY_OK;

	cur_iowait = get_cpu_iowait_time_us(cpu, &now);
	if (cur_iowait >= cpu_st->last_iowait)
		iowait_diff = cur_iowait - cpu_st->last_iowait;
	else
		iowait_diff = 0;

	if (now > cpu_st->last_wallclock)
		time_diff = now - cpu_st->last_wallclock;
	else
		return NOTIFY_OK;

	if (iowait_diff <= time_diff) {
		iowait_diff *= 100;
		cpu_st->last_iopercent = div64_u64(iowait_diff, time_diff);
	} else {
		cpu_st->last_iopercent = 100;
	}

	cpu_st->last_wallclock = now;
	cpu_st->last_iowait = cur_iowait;
	cpu_st->cpu_load = gov_info->load;

	/*
	 * Avoid deadlock in case governor notifier ran in the context
	 * of notify_work thread
	 */
	if (current == notify_thread)
		return NOTIFY_OK;

	check_workload_stats(cpu, gov_info->sampling_rate_us, now);

	return NOTIFY_OK;
}
static struct notifier_block perf_govinfo_nb = {
	.notifier_call = perf_govinfo_notify,
};

/*
 * Attempt to offline CPUs based on their power cost.
 * CPUs with higher power costs are offlined first.
 */
static int __ref rm_high_pwr_cost_cpus(struct cluster *cl)
{
	unsigned int cpu, i;
	struct cpu_pwr_stats *per_cpu_info = get_cpu_pwr_stats();
	struct cpu_pstate_pwr *costs;
	unsigned int *pcpu_pwr;
	unsigned int max_cost_cpu, max_cost;
	int any_cpu = -1;

	if (!per_cpu_info)
		return -ENOSYS;

	for_each_cpu(cpu, cl->cpus) {
		costs = per_cpu_info[cpu].ptable;
		if (!costs || !costs[0].freq)
			continue;

		i = 1;
		while (costs[i].freq)
			i++;

		pcpu_pwr = &per_cpu(cpu_power_cost, cpu);
		*pcpu_pwr = costs[i - 1].power;
		any_cpu = (int)cpu;
		pr_debug("msm_perf: CPU:%d Power:%u\n", cpu, *pcpu_pwr);
	}

	if (any_cpu < 0)
		return -EAGAIN;

	for (i = 0; i < cpumask_weight(cl->cpus); i++) {
		max_cost = 0;
		max_cost_cpu = cpumask_first(cl->cpus);

		for_each_cpu(cpu, cl->cpus) {
			pcpu_pwr = &per_cpu(cpu_power_cost, cpu);
			if (max_cost < *pcpu_pwr) {
				max_cost = *pcpu_pwr;
				max_cost_cpu = cpu;
			}
		}

		if (!cpu_online(max_cost_cpu))
			goto end;

		pr_debug("msm_perf: Offlining CPU%d Power:%d\n", max_cost_cpu,
								max_cost);
		cpumask_set_cpu(max_cost_cpu, cl->offlined_cpus);
		if (cpu_down(max_cost_cpu)) {
			cpumask_clear_cpu(max_cost_cpu, cl->offlined_cpus);
			pr_debug("msm_perf: Offlining CPU%d failed\n",
								max_cost_cpu);
		}

end:
		pcpu_pwr = &per_cpu(cpu_power_cost, max_cost_cpu);
		*pcpu_pwr = 0;
		if (num_online_managed(cl->cpus) <= cl->max_cpu_request)
			break;
	}

	if (num_online_managed(cl->cpus) > cl->max_cpu_request)
		return -EAGAIN;
	else
		return 0;
}

/*
 * try_hotplug tries to online/offline cores based on the current requirement.
 * It loops through the currently managed CPUs and tries to online/offline
 * them until the max_cpu_request criteria is met.
 */
static void __ref try_hotplug(struct cluster *data)
{
	unsigned int i;

	if (!clusters_inited)
		return;

	pr_debug("msm_perf: Trying hotplug...%d:%d\n",
			num_online_managed(data->cpus),	num_online_cpus());

	mutex_lock(&managed_cpus_lock);
	if (num_online_managed(data->cpus) > data->max_cpu_request) {
		if (!rm_high_pwr_cost_cpus(data)) {
			mutex_unlock(&managed_cpus_lock);
			return;
		}

		/*
		 * If power aware offlining fails due to power cost info
		 * being unavaiable fall back to original implementation
		 */
		for (i = num_present_cpus() - 1; i >= 0 &&
						i < num_present_cpus(); i--) {
			if (!cpumask_test_cpu(i, data->cpus) ||	!cpu_online(i))
				continue;

			pr_debug("msm_perf: Offlining CPU%d\n", i);
			cpumask_set_cpu(i, data->offlined_cpus);
			if (cpu_down(i)) {
				cpumask_clear_cpu(i, data->offlined_cpus);
				pr_debug("msm_perf: Offlining CPU%d failed\n",
									i);
				continue;
			}
			if (num_online_managed(data->cpus) <=
							data->max_cpu_request)
				break;
		}
	} else {
		for_each_cpu(i, data->cpus) {
			if (cpu_online(i))
				continue;
			pr_debug("msm_perf: Onlining CPU%d\n", i);
			if (cpu_up(i)) {
				pr_debug("msm_perf: Onlining CPU%d failed\n",
									i);
				continue;
			}
			cpumask_clear_cpu(i, data->offlined_cpus);
			if (num_online_managed(data->cpus) >=
							data->max_cpu_request)
				break;
		}
	}
	mutex_unlock(&managed_cpus_lock);
}

static void __ref release_cluster_control(struct cpumask *off_cpus)
{
	int cpu;

	for_each_cpu(cpu, off_cpus) {
		pr_debug("msm_perf: Release CPU %d\n", cpu);
		if (!cpu_up(cpu))
			cpumask_clear_cpu(cpu, off_cpus);
	}
}

/* Work to evaluate current online CPU status and hotplug CPUs as per need*/
static void check_cluster_status(struct work_struct *work)
{
	int i;
	struct cluster *i_cl;

	for (i = 0; i < num_clusters; i++) {
		i_cl = managed_clusters[i];

		if (cpumask_empty(i_cl->cpus))
			continue;

		if (i_cl->max_cpu_request < 0) {
			if (!cpumask_empty(i_cl->offlined_cpus))
				release_cluster_control(i_cl->offlined_cpus);
			continue;
		}

		if (num_online_managed(i_cl->cpus) !=
					i_cl->max_cpu_request)
			try_hotplug(i_cl);
	}
}

static int __ref msm_performance_cpu_callback(struct notifier_block *nfb,
		unsigned long action, void *hcpu)
{
	uint32_t cpu = (uintptr_t)hcpu;
	unsigned int i;
	struct cluster *i_cl = NULL;

	hotplug_notify(action);

	if (!clusters_inited)
		return NOTIFY_OK;

	for (i = 0; i < num_clusters; i++) {
		if (managed_clusters[i]->cpus == NULL)
			return NOTIFY_OK;
		if (cpumask_test_cpu(cpu, managed_clusters[i]->cpus)) {
			i_cl = managed_clusters[i];
			break;
		}
	}

	if (i_cl == NULL)
		return NOTIFY_OK;

	if (action == CPU_UP_PREPARE || action == CPU_UP_PREPARE_FROZEN) {
		/*
		 * Prevent onlining of a managed CPU if max_cpu criteria is
		 * already satisfied
		 */
		if (i_cl->offlined_cpus == NULL)
			return NOTIFY_OK;
		if (i_cl->max_cpu_request <=
					num_online_managed(i_cl->cpus)) {
			pr_debug("msm_perf: Prevent CPU%d onlining\n", cpu);
			cpumask_set_cpu(cpu, i_cl->offlined_cpus);
			return NOTIFY_BAD;
		}
		cpumask_clear_cpu(cpu, i_cl->offlined_cpus);

	} else if (action == CPU_DEAD) {
		if (i_cl->offlined_cpus == NULL)
			return NOTIFY_OK;
		if (cpumask_test_cpu(cpu, i_cl->offlined_cpus))
			return NOTIFY_OK;
		/*
		 * Schedule a re-evaluation to check if any more CPUs can be
		 * brought online to meet the max_cpu_request requirement. This
		 * work is delayed to account for CPU hotplug latencies
		 */
		if (schedule_delayed_work(&evaluate_hotplug_work, 0)) {
			trace_reevaluate_hotplug(cpumask_bits(i_cl->cpus)[0],
							i_cl->max_cpu_request);
			pr_debug("msm_perf: Re-evaluation scheduled %d\n", cpu);
		} else {
			pr_debug("msm_perf: Work scheduling failed %d\n", cpu);
		}
	}

	return NOTIFY_OK;
}

static struct notifier_block __refdata msm_performance_cpu_notifier = {
	.notifier_call = msm_performance_cpu_callback,
};

static void single_mod_exit_timer(unsigned long data)
{
	int i;
	struct cluster *i_cl = NULL;
	unsigned long flags;
	if (!clusters_inited)
		return;

	for (i = 0; i < num_clusters; i++) {
		if (cpumask_test_cpu(data,
			managed_clusters[i]->cpus)) {
			i_cl = managed_clusters[i];
			break;
		}
	}

	if (i_cl == NULL)
		return;

	spin_lock_irqsave(&i_cl->mode_lock, flags);
	if (i_cl->mode & SINGLE) {
		/*Disable SINGLE mode and exit since the timer expired*/
		i_cl->mode = i_cl->mode & ~SINGLE;
		i_cl->single_enter_cycle_cnt = 0;
		i_cl->single_exit_cycle_cnt = 0;
		trace_single_mode_timeout(cpumask_first(i_cl->cpus),
			i_cl->single_enter_cycles, i_cl->single_enter_cycle_cnt,
			i_cl->single_exit_cycles, i_cl->single_exit_cycle_cnt,
			i_cl->multi_enter_cycles, i_cl->multi_enter_cycle_cnt,
			i_cl->multi_exit_cycles, i_cl->multi_exit_cycle_cnt,
			i_cl->timer_rate, i_cl->mode);
	}
	spin_unlock_irqrestore(&i_cl->mode_lock, flags);
	wake_up_process(notify_thread);
}

static int init_cluster_control(void)
{
	unsigned int i;
	int ret = 0;
	struct kobject *module_kobj;

	managed_clusters = kzalloc(num_clusters * sizeof(struct cluster *),
								GFP_KERNEL);
	if (!managed_clusters) {
		pr_err("msm_perf: Memory allocation failed\n");
		return -ENOMEM;
	}

	for (i = 0; i < num_clusters; i++) {
		managed_clusters[i] = kzalloc(sizeof(struct cluster),
								GFP_KERNEL);
		if (!managed_clusters[i]) {
			pr_err("msm_perf:Cluster %u mem alloc failed\n", i);
			ret = -ENOMEM;
			goto error;
		}
		if (!alloc_cpumask_var(&managed_clusters[i]->cpus,
		     GFP_KERNEL)) {
			pr_err("msm_perf:Cluster %u cpu alloc failed\n",
			       i);
			ret = -ENOMEM;
			goto error;
		}
		if (!alloc_cpumask_var(&managed_clusters[i]->offlined_cpus,
		     GFP_KERNEL)) {
			pr_err("msm_perf:Cluster %u off_cpus alloc failed\n",
			       i);
			ret = -ENOMEM;
			goto error;
		}
		managed_clusters[i]->max_cpu_request = -1;
		managed_clusters[i]->single_enter_load = DEF_SINGLE_ENT;
		managed_clusters[i]->single_exit_load = DEF_SINGLE_EX;
		managed_clusters[i]->single_enter_cycles
						= DEF_SINGLE_ENTER_CYCLE;
		managed_clusters[i]->single_exit_cycles
						= DEF_SINGLE_EXIT_CYCLE;
		managed_clusters[i]->pcpu_multi_enter_load
						= DEF_PCPU_MULTI_ENT;
		managed_clusters[i]->pcpu_multi_exit_load = DEF_PCPU_MULTI_EX;
		managed_clusters[i]->multi_enter_cycles = DEF_MULTI_ENTER_CYCLE;
		managed_clusters[i]->multi_exit_cycles = DEF_MULTI_EXIT_CYCLE;

		spin_lock_init(&(managed_clusters[i]->iowait_lock));
		spin_lock_init(&(managed_clusters[i]->mode_lock));
		spin_lock_init(&(managed_clusters[i]->timer_lock));
		init_timer(&managed_clusters[i]->mode_exit_timer);
		managed_clusters[i]->mode_exit_timer.function =
			single_mod_exit_timer;
	}

	INIT_DELAYED_WORK(&evaluate_hotplug_work, check_cluster_status);
	mutex_init(&managed_cpus_lock);

	module_kobj = kset_find_obj(module_kset, KBUILD_MODNAME);
	if (!module_kobj) {
		pr_err("msm_perf: Couldn't find module kobject\n");
		ret = -ENOENT;
		goto error;
	}
	mode_kobj = kobject_create_and_add("workload_modes", module_kobj);
	if (!mode_kobj) {
		pr_err("msm_perf: Failed to add mode_kobj\n");
		ret = -ENOMEM;
		kobject_put(module_kobj);
		goto error;
	}
	ret = sysfs_create_group(mode_kobj, &attr_group);
	if (ret) {
		pr_err("msm_perf: Failed to create sysfs\n");
		kobject_put(module_kobj);
		kobject_put(mode_kobj);
		goto error;
	}

	notify_thread = kthread_run(notify_userspace, NULL, "wrkld_notify");
	clusters_inited = true;

	return 0;

error:
	for (i = 0; i < num_clusters; i++) {
		if (!managed_clusters[i])
			break;
		if (managed_clusters[i]->offlined_cpus)
			free_cpumask_var(managed_clusters[i]->offlined_cpus);
		if (managed_clusters[i]->cpus)
			free_cpumask_var(managed_clusters[i]->cpus);
		kfree(managed_clusters[i]);
	}
	kfree(managed_clusters);
	return ret;
}

static int init_events_group(void)
{
	int ret;
	struct kobject *module_kobj;

	module_kobj = kset_find_obj(module_kset, KBUILD_MODNAME);
	if (!module_kobj) {
		pr_err("msm_perf: Couldn't find module kobject\n");
		return -ENOENT;
	}

	events_kobj = kobject_create_and_add("events", module_kobj);
	if (!events_kobj) {
		pr_err("msm_perf: Failed to add events_kobj\n");
		return -ENOMEM;
	}

	ret = sysfs_create_group(events_kobj, &events_attr_group);
	if (ret) {
		pr_err("msm_perf: Failed to create sysfs\n");
		return ret;
	}

	spin_lock_init(&(events_group.cpu_hotplug_lock));
	events_notify_thread = kthread_run(events_notify_userspace,
					NULL, "msm_perf:events_notify");
	if (IS_ERR(events_notify_thread))
		return PTR_ERR(events_notify_thread);

	events_group.init_success = true;

	return 0;
}

static int __init msm_performance_init(void)
{
	unsigned int cpu;

	cpufreq_register_notifier(&perf_cpufreq_nb, CPUFREQ_POLICY_NOTIFIER);
	cpufreq_register_notifier(&perf_govinfo_nb, CPUFREQ_GOVINFO_NOTIFIER);

	for_each_present_cpu(cpu)
		per_cpu(cpu_stats, cpu).max = UINT_MAX;

	register_cpu_notifier(&msm_performance_cpu_notifier);

	init_events_group();

	return 0;
}
late_initcall(msm_performance_init);
