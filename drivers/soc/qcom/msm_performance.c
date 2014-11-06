/*
 * Copyright (c) 2014, The Linux Foundation. All rights reserved.
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

#include <trace/events/power.h>

static struct mutex managed_cpus_lock;

/* Maximum number to clusters that this module will manage*/
static unsigned int num_clusters;
struct cpu_hp {
	cpumask_var_t cpus;
	/* Number of CPUs to maintain online */
	int max_cpu_request;
	/* To track CPUs that the module decides to offline */
	cpumask_var_t offlined_cpus;
};
static struct cpu_hp **managed_clusters;

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

	if (!num_clusters)
		return -EINVAL;

	while ((cp = strpbrk(cp + 1, ":")))
		ntokens++;

	if (!ntokens)
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

	if (!num_clusters)
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

	if (!num_clusters)
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

	if (!num_clusters)
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
	struct cpu_hp *i_cpu_hp;

	if (!num_clusters)
		return cnt;

	for (i = 0; i < num_clusters; i++) {
		i_cpu_hp = managed_clusters[i];

		cpumask_clear(&tmp_mask);
		cpumask_complement(&tmp_mask, i_cpu_hp->offlined_cpus);
		cpumask_and(&tmp_mask, i_cpu_hp->cpus, &tmp_mask);

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
		if (cpu > num_present_cpus())
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
		if (cpu > num_present_cpus())
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

/*
 * try_hotplug tries to online/offline cores based on the current requirement.
 * It loops through the currently managed CPUs and tries to online/offline
 * them until the max_cpu_request criteria is met.
 */
static void __ref try_hotplug(struct cpu_hp *data)
{
	unsigned int i;

	pr_debug("msm_perf: Trying hotplug...%d:%d\n",
			num_online_managed(data->cpus),	num_online_cpus());

	mutex_lock(&managed_cpus_lock);
	if (num_online_managed(data->cpus) > data->max_cpu_request) {
		for (i = num_present_cpus() - 1; i >= 0; i--) {
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
		pr_err("msm_perf: Release CPU %d\n", cpu);
		if (!cpu_up(cpu))
			cpumask_clear_cpu(cpu, off_cpus);
	}
}

/* Work to evaluate current online CPU status and hotplug CPUs as per need*/
static void check_cluster_status(struct work_struct *work)
{
	int i;
	struct cpu_hp *i_chp;

	for (i = 0; i < num_clusters; i++) {
		i_chp = managed_clusters[i];

		if (cpumask_empty(i_chp->cpus))
			continue;

		if (i_chp->max_cpu_request < 0) {
			if (!cpumask_empty(i_chp->offlined_cpus))
				release_cluster_control(i_chp->offlined_cpus);
			continue;
		}

		if (num_online_managed(i_chp->cpus) !=
					i_chp->max_cpu_request)
			try_hotplug(i_chp);
	}
}

static int __ref msm_performance_cpu_callback(struct notifier_block *nfb,
		unsigned long action, void *hcpu)
{
	uint32_t cpu = (uintptr_t)hcpu;
	unsigned int i;
	struct cpu_hp *i_hp = NULL;

	for (i = 0; i < num_clusters; i++) {
		if (cpumask_test_cpu(cpu, managed_clusters[i]->cpus)) {
			i_hp = managed_clusters[i];
			break;
		}
	}

	if (i_hp == NULL)
		return NOTIFY_OK;

	if (action == CPU_UP_PREPARE || action == CPU_UP_PREPARE_FROZEN) {
		/*
		 * Prevent onlining of a managed CPU if max_cpu criteria is
		 * already satisfied
		 */
		if (i_hp->max_cpu_request <=
					num_online_managed(i_hp->cpus)) {
			pr_debug("msm_perf: Prevent CPU%d onlining\n", cpu);
			return NOTIFY_BAD;
		}
		cpumask_clear_cpu(cpu, i_hp->offlined_cpus);

	} else if (action == CPU_DEAD) {
		if (cpumask_test_cpu(cpu, i_hp->offlined_cpus))
			return NOTIFY_OK;
		/*
		 * Schedule a re-evaluation to check if any more CPUs can be
		 * brought online to meet the max_cpu_request requirement. This
		 * work is delayed to account for CPU hotplug latencies
		 */
		if (schedule_delayed_work(&evaluate_hotplug_work, 0)) {
			trace_reevaluate_hotplug(cpumask_bits(i_hp->cpus)[0],
							i_hp->max_cpu_request);
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

static int init_cluster_control(void)
{
	unsigned int i;

	managed_clusters = kzalloc(num_clusters * sizeof(struct cpu_hp *),
								GFP_KERNEL);
	if (!managed_clusters) {
		pr_err("msm_perf: Memory allocation failed\n");
		return -ENOMEM;
	}

	for (i = 0; i < num_clusters; i++) {
		managed_clusters[i] = kzalloc(sizeof(struct cpu_hp),
								GFP_KERNEL);
		if (!managed_clusters[i]) {
			pr_err("msm_perf:Cluster %u mem alloc failed\n", i);
			return -ENOMEM;
		}

		managed_clusters[i]->max_cpu_request = -1;
	}

	INIT_DELAYED_WORK(&evaluate_hotplug_work, check_cluster_status);
	mutex_init(&managed_cpus_lock);

	return 0;
}

static int __init msm_performance_init(void)
{
	unsigned int cpu;

	cpufreq_register_notifier(&perf_cpufreq_nb, CPUFREQ_POLICY_NOTIFIER);
	for_each_present_cpu(cpu)
		per_cpu(cpu_stats, cpu).max = UINT_MAX;
	register_cpu_notifier(&msm_performance_cpu_notifier);
	return 0;
}
late_initcall(msm_performance_init);

