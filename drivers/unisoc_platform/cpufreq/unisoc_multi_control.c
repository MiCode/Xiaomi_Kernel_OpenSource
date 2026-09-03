// SPDX-License-Identifier: GPL-2.0
/*
 * unisoc multi control for cpufreq
 *
 * Copyright (C) 2022 Unisoc corporation. http://www.unisoc.com
 */

#include "unisoc_multi_control.h"

static LIST_HEAD(sprd_mc_list);
static DEFINE_PER_CPU(struct sprd_multi_control *, sprd_mc_data);

static ssize_t show_user_max_freq(struct cpufreq_policy *policy, char *buf)
{
	struct sprd_multi_control *sprd_mc;

	list_for_each_entry(sprd_mc, &sprd_mc_list, node) {
		if (sprd_mc->policy == policy)
			return sprintf(buf, "%u\n", sprd_mc->user_max_freq);
	}

	return -EINVAL;
}

static ssize_t store_user_max_freq(struct cpufreq_policy *policy,
							const char *buf, size_t count)
{
	unsigned int val = 0;
	struct sprd_multi_control *sprd_mc;
	int ret;

	ret = kstrtouint(buf, 10, &val);
	if (ret)
		return -EINVAL;

	list_for_each_entry(sprd_mc, &sprd_mc_list, node) {
		if (sprd_mc->policy == policy) {
			ret = freq_qos_update_request(&sprd_mc->qos_req, val);
			if (ret < 0) {
				pr_err("Failed to update freq constraint\n");
				return -EINVAL;
			}

			sprd_mc->user_max_freq = val;
			pr_info("Update cpu%d policy max freq constraint to %u\n",
				policy->cpu, val);
			return count;
		}
	}

	return -EINVAL;
}

static ssize_t show_high_level_freq_max(struct cpufreq_policy *policy, char *buf)
{
	struct sprd_multi_control *sprd_mc;

	list_for_each_entry(sprd_mc, &sprd_mc_list, node) {
		if (sprd_mc->policy == policy)
			return sprintf(buf, "%u\n", sprd_mc->high_level_limit_max);
	}

	return -EINVAL;
}

static ssize_t show_high_level_freq_min(struct cpufreq_policy *policy, char *buf)
{
	struct sprd_multi_control *sprd_mc;

	list_for_each_entry(sprd_mc, &sprd_mc_list, node) {
		if (sprd_mc->policy == policy)
			return sprintf(buf, "%u\n", sprd_mc->high_level_limit_min);
	}

	return -EINVAL;
}

static ssize_t
show_high_level_freq_control_enable(struct cpufreq_policy *policy, char *buf)
{
	struct sprd_multi_control *sprd_mc;

	list_for_each_entry(sprd_mc, &sprd_mc_list, node) {
		if (sprd_mc->policy == policy)
			return sprintf(buf, "%u\n", sprd_mc->hl_control_enabled);
	}

	return -EINVAL;
}

#ifdef CONFIG_UNISOC_FIX_FREQ

static ssize_t
show_scaling_fixed_freq(struct cpufreq_policy *policy, char *buf)
{
	struct sprd_multi_control *sprd_mc;

	list_for_each_entry(sprd_mc, &sprd_mc_list, node) {
		if (sprd_mc->policy == policy)
			return sprintf(buf, "%u\n", sprd_mc->scaling_fixed_freq);
	}

	return -EINVAL;
}

static ssize_t store_scaling_fixed_freq(struct cpufreq_policy *policy,
							const char *buf, size_t count)
{
	unsigned int fix_freq = 0;
	unsigned int ret;
	struct sprd_multi_control *sprd_mc;

	ret = kstrtouint(buf, 10, &fix_freq);
	if (ret != 0)
		return -EINVAL;

	if (fix_freq != 0) {
		struct cpufreq_frequency_table *table = policy->freq_table;
		struct cpufreq_frequency_table *pos;
		unsigned int freq = 0;

		if (fix_freq > policy->cpuinfo.max_freq || fix_freq < policy->cpuinfo.min_freq)
			return -EINVAL;

		cpufreq_for_each_valid_entry(pos, table) {
			freq = pos->frequency;
			if (freq == fix_freq)
				break;
		}

		if (freq != fix_freq)
			return -EINVAL;
	}

	list_for_each_entry(sprd_mc, &sprd_mc_list, node) {
		if (sprd_mc->policy == policy) {
			sprd_mc->scaling_fixed_freq = fix_freq;
			if (fix_freq) {
				smp_wmb(); // Ensure that the fix_freq update is visible
				__cpufreq_driver_target(policy, fix_freq, CPUFREQ_RELATION_L);
			}
			refresh_frequency_limits(policy);

			return count;
		}
	}

	return -EINVAL;
}
#endif

static ssize_t
store_high_level_freq_control_enable(struct cpufreq_policy *policy,
		const char *buf, size_t count)
{
	unsigned int val;
	struct sprd_multi_control *sprd_mc;
	int ret;

	ret = kstrtouint(buf, 10, &val);

	if (ret)
		return -EINVAL;

	list_for_each_entry(sprd_mc, &sprd_mc_list, node) {
		if (sprd_mc->policy == policy) {

			sprd_mc->hl_control_enabled = !!val;

			if ((policy->cur > policy->min && policy->cur < policy->max) &&
					(!sprd_mc->hl_control_enabled))
				return count;

			__cpufreq_driver_target(policy, policy->cur, CPUFREQ_RELATION_L);
			refresh_frequency_limits(policy);

			return count;
		}
	}

	return -EINVAL;
}

static ssize_t store_high_level_freq_max(struct cpufreq_policy *policy,
							const char *buf, size_t count)
{
	unsigned int val = 0;
	unsigned int clamp_freq;
	struct sprd_multi_control *sprd_mc;
	int ret;

	ret = kstrtouint(buf, 10, &val);
	if (ret)
		return -EINVAL;

	if (val > policy->cpuinfo.max_freq || val < policy->cpuinfo.min_freq)
		return -EINVAL;

	if (val) {
		struct cpufreq_frequency_table *table = policy->freq_table;
		struct cpufreq_frequency_table *pos;
		unsigned int freq = 0;

		cpufreq_for_each_valid_entry(pos, table) {
			freq = pos->frequency;
			if (freq == val)
				break;
		}
		if (freq != val)
			return -EINVAL;
	}

	list_for_each_entry(sprd_mc, &sprd_mc_list, node) {
		if (sprd_mc->policy == policy) {
			if (val < sprd_mc->high_level_limit_min)
				return -EINVAL;

			sprd_mc->high_level_limit_max = val;
			clamp_freq = clamp_val(policy->cur, sprd_mc->high_level_limit_min,
					sprd_mc->high_level_limit_max);

			if (clamp_freq != policy->cur) {
				__cpufreq_driver_target(policy, clamp_freq, CPUFREQ_RELATION_L);
				refresh_frequency_limits(policy);
			}

			return count;
		}
	}
	return -EINVAL;
}

static ssize_t store_high_level_freq_min(struct cpufreq_policy *policy,
							const char *buf, size_t count)
{
	unsigned int val = 0;
	unsigned int clamp_freq;
	struct sprd_multi_control *sprd_mc;
	int ret;

	ret = kstrtouint(buf, 10, &val);
	if (ret != 0)
		return -EINVAL;

	if (val > policy->cpuinfo.max_freq || val < policy->cpuinfo.min_freq)
		return -EINVAL;

	if (val) {
		struct cpufreq_frequency_table *table = policy->freq_table;
		struct cpufreq_frequency_table *pos;
		unsigned int freq = 0;

		cpufreq_for_each_valid_entry(pos, table) {
			freq = pos->frequency;
			if (freq == val)
				break;
		}
		if (freq != val)
			return -EINVAL;
	}

	list_for_each_entry(sprd_mc, &sprd_mc_list, node) {
		if (sprd_mc->policy == policy) {
			if (val > sprd_mc->high_level_limit_max)
				return -EINVAL;

			sprd_mc->high_level_limit_min = val;
			clamp_freq = clamp_val(policy->cur, sprd_mc->high_level_limit_min,
					sprd_mc->high_level_limit_max);

			if (clamp_freq != policy->cur) {
				__cpufreq_driver_target(policy, clamp_freq, CPUFREQ_RELATION_H);
				refresh_frequency_limits(policy);
			}

			return count;
		}
	}

	return -EINVAL;
}

cpufreq_attr_rw(user_max_freq);
cpufreq_attr_rw(high_level_freq_max);
cpufreq_attr_rw(high_level_freq_min);
cpufreq_attr_rw(high_level_freq_control_enable);
#ifdef CONFIG_UNISOC_FIX_FREQ
cpufreq_attr_rw(scaling_fixed_freq);
#endif

static struct sprd_multi_control *sprd_mc_alloc(void)
{
	struct sprd_multi_control *sprd_mc;

	sprd_mc = kzalloc(sizeof(*sprd_mc), GFP_KERNEL);

	return sprd_mc;
}

static void trace_cpufreq_target(void *data, struct cpufreq_policy *policy,
					unsigned int *target_freq, unsigned int old_target)
{
	int idx;
	struct sprd_multi_control *sprd_mc;

	list_for_each_entry(sprd_mc, &sprd_mc_list, node) {

#ifdef CONFIG_UNISOC_FIX_FREQ
		if (unlikely(sprd_mc->policy == policy &&
				sprd_mc->scaling_fixed_freq)){

			if (sprd_mc->scaling_fixed_freq == *target_freq)
				break;

			if (unlikely(policy->freq_table_sorted == CPUFREQ_TABLE_UNSORTED)) {
				pr_info("%s: freq table unsorted\n", __func__);
				idx = cpufreq_table_index_unsorted(policy, sprd_mc->scaling_fixed_freq,
									CPUFREQ_RELATION_L);
			} else {

				if (policy->freq_table_sorted == CPUFREQ_TABLE_SORTED_ASCENDING)
					idx = cpufreq_table_find_index_al(policy, sprd_mc->scaling_fixed_freq);
				else
					idx = cpufreq_table_find_index_dl(policy, sprd_mc->scaling_fixed_freq);
			}

			policy->cached_resolved_idx = idx;
			policy->cached_target_freq = sprd_mc->scaling_fixed_freq;
			*target_freq = policy->freq_table[idx].frequency;
			break;
		}
#endif
		if (sprd_mc->policy == policy && sprd_mc->hl_control_enabled) {
			*target_freq = clamp_val(old_target,
					sprd_mc->high_level_limit_min,
					sprd_mc->high_level_limit_max);

			if (unlikely(policy->freq_table_sorted == CPUFREQ_TABLE_UNSORTED)) {
				pr_info("%s: freq table unsorted\n", __func__);
				idx = cpufreq_table_index_unsorted(policy, *target_freq,
									CPUFREQ_RELATION_L);
			} else {

				if (policy->freq_table_sorted == CPUFREQ_TABLE_SORTED_ASCENDING)
					idx = cpufreq_table_find_index_al(policy, *target_freq);
				else
					idx = cpufreq_table_find_index_dl(policy, *target_freq);
			}

			policy->cached_resolved_idx = idx;
			policy->cached_target_freq = *target_freq;
			*target_freq = policy->freq_table[idx].frequency;
			break;
		}
	}
}

static const struct attribute *multi_control_attrs[] = {
	&user_max_freq.attr,
	&high_level_freq_max.attr,
	&high_level_freq_min.attr,
	&high_level_freq_control_enable.attr,
#ifdef CONFIG_UNISOC_FIX_FREQ
	&scaling_fixed_freq.attr,
#endif
	NULL,
};

static void multi_control_exit(void)
{
	struct sprd_multi_control *sprd_mc, *temp_sprd_mc;
	int cpu;

	unregister_trace_android_vh_cpufreq_target(trace_cpufreq_target, NULL);
	list_for_each_entry_safe(sprd_mc, temp_sprd_mc, &sprd_mc_list, node) {
		freq_qos_remove_request(&sprd_mc->qos_req);
		sysfs_remove_files(&sprd_mc->policy->kobj, multi_control_attrs);
		for_each_cpu(cpu, sprd_mc->policy->related_cpus)
			per_cpu(sprd_mc_data, cpu) = NULL;
		list_del(&sprd_mc->node);
		kfree(sprd_mc);
	}

}

static inline void multi_control_failed(void)
{
	struct sprd_multi_control *sprd_mc, *temp_sprd_mc;
	int cpu;

	list_for_each_entry_safe(sprd_mc, temp_sprd_mc, &sprd_mc_list, node) {
		freq_qos_remove_request(&sprd_mc->qos_req);
		sysfs_remove_files(&sprd_mc->policy->kobj, multi_control_attrs);
		for_each_cpu(cpu, sprd_mc->policy->related_cpus)
			per_cpu(sprd_mc_data, cpu) = NULL;
		list_del(&sprd_mc->node);
		kfree(sprd_mc);
	}
}

static int __init sprd_multi_control_init(void)
{
	struct sprd_multi_control *sprd_mc;
	struct cpufreq_policy *policy;
	int cpu;
	int i, j;
	int ret;

	for_each_possible_cpu(cpu) {
		bool new_sprd_policy = false;
		sprd_mc = per_cpu(sprd_mc_data, cpu);

		if (!sprd_mc) {
			new_sprd_policy = true;
			sprd_mc = sprd_mc_alloc();

			if (!sprd_mc) {
				ret = -ENOMEM;
				goto mem_failed;
			}
		}

		if (new_sprd_policy) {
			policy = cpufreq_cpu_get(cpu);
			if (!policy) {
				ret = -EINVAL;
				kfree(sprd_mc);
				goto mem_failed;
			}
			sprd_mc->policy = policy;
			INIT_LIST_HEAD(&sprd_mc->node);
			for_each_cpu(j, sprd_mc->policy->related_cpus)
				per_cpu(sprd_mc_data, j) = sprd_mc;
			sprd_mc->high_level_limit_max = policy->cpuinfo.max_freq;
			sprd_mc->high_level_limit_min = policy->cpuinfo.min_freq;
			sprd_mc->hl_control_enabled = 0;
#ifdef CONFIG_UNISOC_FIX_FREQ
			sprd_mc->scaling_fixed_freq = 0;
#endif
			ret = freq_qos_add_request(&policy->constraints,
				   &sprd_mc->qos_req, FREQ_QOS_MAX,
				   FREQ_QOS_MAX_DEFAULT_VALUE);
			if (ret < 0) {
				pr_err("Failed to add freq constraint\n");
				cpufreq_cpu_put(policy);
				goto qos_fail;
			}

			ret = sysfs_create_files(&sprd_mc->policy->kobj, multi_control_attrs);
			if (ret) {
				cpufreq_cpu_put(policy);
				goto create_files_failed;
			}
			list_add(&sprd_mc->node, &sprd_mc_list);
			cpufreq_cpu_put(policy);
		}
	}
	register_trace_android_vh_cpufreq_target(trace_cpufreq_target, NULL);
	return 0;

create_files_failed:
	freq_qos_remove_request(&sprd_mc->qos_req);
qos_fail:
	for_each_cpu(i, sprd_mc->policy->related_cpus)
		per_cpu(sprd_mc_data, i) = NULL;
	kfree(sprd_mc);

mem_failed:
	multi_control_failed();

	return ret;
}

static void __exit sprd_multi_control_exit(void)
{
	multi_control_exit();
}

late_initcall(sprd_multi_control_init);
module_exit(sprd_multi_control_exit);

MODULE_DESCRIPTION("for cpufreq high level freq control");
MODULE_LICENSE("GPL");
