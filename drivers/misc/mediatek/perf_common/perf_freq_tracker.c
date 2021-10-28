// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2021 MediaTek Inc.
 */
#include <linux/pm_qos.h>
#include <trace/hooks/power.h>
#include <linux/kallsyms.h>
#include <linux/hashtable.h>

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
static struct freq_constraints *qos_in_cluster[MAX_CLUSTER_NR] = {0};

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

static inline int find_qos_in_cluster(struct freq_constraints *qos)
{
	int cid = 0;

	for (cid = 0; cid < cluster_nr; cid++) {
		if (qos_in_cluster[cid] == qos)
			break;
	}
	return (cid < cluster_nr) ? cid : -1;
}

static void mtk_freq_qos_add_request(void *data, struct freq_constraints *qos,
	struct freq_qos_request *req, enum freq_qos_req_type type, int value, int ret)
{
	int cid = 0;
	const char *caller_info = find_and_get_symobls(
		(unsigned long)__builtin_return_address(1));
	if (caller_info) {
		cid = find_qos_in_cluster(qos);
		trace_freq_qos_user_setting(cid, type, value, caller_info);
	}
}

static void mtk_freq_qos_update_request(void *data, struct freq_qos_request *req, int value)
{
	int cid = 0;
	const char *caller_info = find_and_get_symobls(
		(unsigned long)__builtin_return_address(1));
	if (caller_info) {
		cid = find_qos_in_cluster(req->qos);
		trace_freq_qos_user_setting(cid, req->type, value, caller_info);
	}
}

int insert_freq_qos_hook(void)
{
	int ret = 0;

	if (is_hooked || !is_inited)
		return ret;
	ret = register_trace_android_vh_freq_qos_add_request(mtk_freq_qos_add_request, NULL);
	if (ret) {
		pr_info("mtk_freq_qos_add_requests: register hooks failed, returned %d\n", ret);
		goto register_failed;
	}
	ret = register_trace_android_vh_freq_qos_update_request(mtk_freq_qos_update_request, NULL);
	if (ret) {
		pr_info("mtk_freq_qos_update_requests: register hooks failed, returned %d\n", ret);
		goto register_failed;
	}
	is_hooked = 1;
	return ret;

register_failed:
	remove_freq_qos_hook();
	return ret;
}

void remove_freq_qos_hook(void)
{
	is_hooked = 0;
	unregister_trace_android_vh_freq_qos_add_request(mtk_freq_qos_add_request, NULL);
	unregister_trace_android_vh_freq_qos_update_request(mtk_freq_qos_update_request, NULL);
}

static void init_cluster_qos_info(void)
{
	struct cpufreq_policy *policy;
	int cpu;
	int num = 0;

	for_each_possible_cpu(cpu) {
		if (num >= cluster_nr)
			break;
		policy = cpufreq_cpu_get(cpu);
		if (policy) {
			qos_in_cluster[num++] = &(policy->constraints);
			cpu = cpumask_last(policy->related_cpus);
			cpufreq_cpu_put(policy);
		}
	}
}

void init_perf_freq_tracker(void)
{
	is_hooked = 0;
	is_inited = 1;
	// Initialize hash table
	hash_init(tbl);
	init_cluster_qos_info();
}

void exit_perf_freq_tracker(void)
{
	int bkt = 0;
	struct h_node *cur = NULL;
	struct hlist_node *tmp = NULL;

	is_inited = 0;
	remove_freq_qos_hook();
	// Remove hash table
	hash_for_each_safe(tbl, bkt, tmp, cur, node) {
		hash_del(&cur->node);
		kfree(cur);
	}
}
