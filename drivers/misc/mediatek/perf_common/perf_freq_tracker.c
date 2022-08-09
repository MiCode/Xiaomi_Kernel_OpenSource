// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2021 MediaTek Inc.
 */
#include <linux/pm_qos.h>
#include <linux/kallsyms.h>
#include <linux/hashtable.h>
#include <linux/slab.h>
#include <perf_tracker_internal.h>
#include <perf_tracker_trace.h>


struct h_node {
	unsigned long addr;
	char symbol[KSYM_SYMBOL_LEN];
	struct hlist_node node;
};

static DECLARE_HASHTABLE(tbl, 5);
static int is_inited;
static int is_hooked;
static struct notifier_block *freq_qos_max_notifier, *freq_qos_min_notifier;
static int cluster_num;

static const char *find_and_get_symobls(unsigned long caller_addr)
{
	struct h_node *cur_node = NULL;
	struct h_node *new_node = NULL;
	const char *cur_symbol = NULL;
	unsigned int cur_key = 0;

	cur_key = (unsigned int) caller_addr & 0x1f;
	// Try to find symbols from history records
	hash_for_each_possible(tbl, cur_node, node, cur_key) {
		if (cur_node->addr == caller_addr) {
			cur_symbol = cur_node->symbol;
			break;
		}
	}
	// Symbols not found. Add new records
	if (!cur_symbol) {
		new_node = kzalloc(sizeof(struct h_node), GFP_KERNEL);
		if (!new_node)
			return NULL;
		new_node->addr = caller_addr;
		sprint_symbol(new_node->symbol, caller_addr);
		cur_symbol = new_node->symbol;
		hash_add(tbl, &new_node->node, cur_key);
	}
	return cur_symbol;
}

static int freq_qos_max_notifier_call(struct notifier_block *nb,
					unsigned long freq_limit_max, void *ptr)
{
	int cid = nb - freq_qos_max_notifier;
	const char *caller_info = find_and_get_symobls(
		(unsigned long)__builtin_return_address(2));
	const char *caller_info2 = find_and_get_symobls(
		(unsigned long)__builtin_return_address(3));
	if (caller_info && caller_info2)
		trace_freq_qos_user_setting(cid, FREQ_QOS_MAX, freq_limit_max,
			caller_info, caller_info2);
	return 0;
}

static int freq_qos_min_notifier_call(struct notifier_block *nb,
					unsigned long freq_limit_min, void *ptr)
{
	int cid = nb - freq_qos_min_notifier;
	const char *caller_info = find_and_get_symobls(
		(unsigned long)__builtin_return_address(2));
	const char *caller_info2 = find_and_get_symobls(
		(unsigned long)__builtin_return_address(3));
	if (caller_info && caller_info2)
		trace_freq_qos_user_setting(cid, FREQ_QOS_MIN, freq_limit_min,
			caller_info, caller_info2);
	return 0;
}

int insert_freq_qos_hook(void)
{
	struct cpufreq_policy *policy;
	int cpu;
	int cid = 0;
	int ret = -1;

	if (is_hooked || !is_inited)
		return ret;

	for_each_possible_cpu(cpu) {
		if (cid >= cluster_num) {
			pr_info("[%s] cid over-index\n", __func__);
			break;
		}
		policy = cpufreq_cpu_get(cpu);
		if (policy) {
			ret = freq_qos_add_notifier(&policy->constraints, FREQ_QOS_MAX,
				freq_qos_max_notifier + cid);
			if (ret) {
				pr_info("[%s] max_notifier failed\n", __func__);
				goto register_failed;
			}

			ret = freq_qos_add_notifier(&policy->constraints, FREQ_QOS_MIN,
				freq_qos_min_notifier + cid);
			if (ret) {
				pr_info("[%s] min_notifier failed\n", __func__);
				goto register_failed;
			}
			cid++;
			cpu = cpumask_last(policy->related_cpus);
			cpufreq_cpu_put(policy);
		}
	}
	is_hooked = 1;
	return ret;

register_failed:
	remove_freq_qos_hook();
	return ret;
}

void remove_freq_qos_hook(void)
{
	struct cpufreq_policy *policy;
	int cpu;
	int cid = 0;
	int ret;

	is_hooked = 0;

	for_each_possible_cpu(cpu) {
		if (cid >= cluster_num) {
			pr_info("[%s] cid over-index\n", __func__);
			break;
		}
		policy = cpufreq_cpu_get(cpu);

		if (policy) {
			ret = freq_qos_remove_notifier(&policy->constraints, FREQ_QOS_MAX,
				freq_qos_max_notifier + cid);
			if (ret)
				pr_info("[%s] max_notifier failed\n", __func__);
			ret = freq_qos_remove_notifier(&policy->constraints, FREQ_QOS_MIN,
				freq_qos_min_notifier + cid);
			if (ret)
				pr_info("[%s] min_notifier failed\n", __func__);
			cid++;
			cpu = cpumask_last(policy->related_cpus);
			cpufreq_cpu_put(policy);
		}
	}
}

static void init_freq_qos_notifier(void)
{
	struct cpufreq_policy *policy;
	int cpu, cid;

	cluster_num = 0;
	for_each_possible_cpu(cpu) {
		policy = cpufreq_cpu_get(cpu);
		if (policy) {
			cluster_num++;
			cpu = cpumask_last(policy->related_cpus);
			cpufreq_cpu_put(policy);
		}
	}
	freq_qos_max_notifier = kcalloc(cluster_num, sizeof(struct notifier_block), GFP_KERNEL);
	freq_qos_min_notifier = kcalloc(cluster_num, sizeof(struct notifier_block), GFP_KERNEL);

	for (cid = 0; cid < cluster_num; cid++) {
		freq_qos_max_notifier[cid].notifier_call = freq_qos_max_notifier_call;
		freq_qos_min_notifier[cid].notifier_call = freq_qos_min_notifier_call;
	}
}

static void clear_freq_qos_notifier(void)
{
	kfree(freq_qos_max_notifier);
	kfree(freq_qos_min_notifier);
}

void init_perf_freq_tracker(void)
{
	is_hooked = 0;
	init_freq_qos_notifier();
	// Initialize hash table
	hash_init(tbl);

	is_inited = 1;
}

void exit_perf_freq_tracker(void)
{
	int bkt = 0;
	struct h_node *cur = NULL;
	struct hlist_node *tmp = NULL;

	remove_freq_qos_hook();
	clear_freq_qos_notifier();
	// Remove hash table
	hash_for_each_safe(tbl, bkt, tmp, cur, node) {
		hash_del(&cur->node);
		kfree(cur);
	}

	is_inited = 0;
}
