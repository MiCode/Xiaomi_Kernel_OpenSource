/*
 * Copyright (C) 2016 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */


#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/uaccess.h>

#include "mt_ppm_internal.h"


#define PROC_FOPS_RO_HICA_SETTINGS(name, var)						\
	static int ppm_##name##_proc_show(struct seq_file *m, void *v)			\
	{										\
		struct ppm_state_transfer *p =						\
			(struct ppm_state_transfer *)m->private;			\
											\
		seq_printf(m, "%u\n", var);						\
		return 0;								\
	}										\
	PROC_FOPS_RO(name)

#define PROC_FOPS_RW_HICA_SETTINGS(name, var)						\
	static int ppm_##name##_proc_show(struct seq_file *m, void *v)			\
	{										\
		struct ppm_state_transfer *p =						\
			(struct ppm_state_transfer *)m->private;			\
											\
		seq_printf(m, "%u\n", var);						\
		return 0;								\
	}										\
	static ssize_t ppm_##name##_proc_write(struct file *file,			\
			const char __user *buffer, size_t count, loff_t *pos)		\
	{										\
		struct ppm_state_transfer *p =						\
			(struct ppm_state_transfer *)PDE_DATA(file_inode(file));	\
		unsigned int setting;							\
		char *buf = ppm_copy_from_user_for_proc(buffer, count);			\
											\
		if (!buf)								\
			return -EINVAL;							\
											\
		if (!kstrtouint(buf, 10, &setting))					\
			var = setting;							\
		else									\
			ppm_err("@%s: Bad argument(%d)!\n", __func__, setting);		\
											\
		free_page((unsigned long)buf);						\
		return count;								\
	}										\
	PROC_FOPS_RW(name)


static enum ppm_power_state fix_power_state = PPM_POWER_STATE_NONE;

static void ppm_hica_reset_data_for_state(enum ppm_power_state new_state);
static void ppm_hica_update_limit_cb(enum ppm_power_state new_state);
static void ppm_hica_status_change_cb(bool enable);
static void ppm_hica_mode_change_cb(enum ppm_mode mode);


/* other members will init by ppm_main */
static struct ppm_policy_data hica_policy = {
	.name			= __stringify(PPM_POLICY_HICA),
	.lock			= __MUTEX_INITIALIZER(hica_policy.lock),
	.policy			= PPM_POLICY_HICA,
	.priority		= PPM_POLICY_PRIO_SYSTEM_BASE,
	.get_power_state_cb	= NULL,	/* No need */
	.update_limit_cb	= ppm_hica_update_limit_cb,
	.status_change_cb	= ppm_hica_status_change_cb,
	.mode_change_cb		= ppm_hica_mode_change_cb,
};

struct ppm_hica_algo_data ppm_hica_algo_data = {
	.cur_state = PPM_POWER_STATE_4LL_L,
	.new_state = PPM_POWER_STATE_4LL_L,

	.ppm_cur_loads = 0,
	.ppm_cur_tlp = 0,
	.ppm_cur_nr_heavy_task = 0,
};

void mt_ppm_hica_update_algo_data(unsigned int cur_loads,
					unsigned int cur_nr_heavy_task, unsigned int cur_tlp)
{
	struct ppm_power_state_data *state_info = ppm_get_power_state_info();
	struct ppm_state_transfer_data *data;
	enum ppm_power_state cur_state;
	enum ppm_mode cur_mode;
	int i, j;

	FUNC_ENTER(FUNC_LV_HICA);

	ppm_lock(&hica_policy.lock);

	ppm_hica_algo_data.ppm_cur_loads = cur_loads;
	ppm_hica_algo_data.ppm_cur_tlp = cur_tlp;
	ppm_hica_algo_data.ppm_cur_nr_heavy_task = cur_nr_heavy_task;

	cur_state = ppm_hica_algo_data.cur_state;
	cur_mode = ppm_main_info.cur_mode;

	ppm_dbg(HICA, "cur_loads = %d, cur_tlp = %d, cur_nr_heavy_task = %d, cur_state = %s, cur_mode = %d\n",
		cur_loads, cur_tlp, cur_nr_heavy_task, ppm_get_power_state_name(cur_state), cur_mode);

	if (!ppm_main_info.is_enabled || !hica_policy.is_enabled || ppm_main_info.is_in_suspend ||
		cur_state == PPM_POWER_STATE_NONE)
		goto end;

#ifdef PPM_IC_SEGMENT_CHECK
	if (ppm_main_info.fix_state_by_segment != PPM_POWER_STATE_NONE)
		goto end;
#endif

	/* skip HICA if DVFS is not ready (we cannot get current freq...) */
	if (!ppm_main_info.client_info[PPM_CLIENT_DVFS].limit_cb)
		goto end;

	/* Power state is fixed by user, skip HICA state calculation */
	if (fix_power_state != PPM_POWER_STATE_NONE)
		goto end;

	for (i = 0; i < 2; i++) {
		data = (i == 0) ? state_info[cur_state].transfer_by_perf
				: state_info[cur_state].transfer_by_pwr;

		for (j = 0; j < data->size; j++) {
			if (!data->transition_data[j].transition_rule
				|| !((1 << cur_mode) & data->transition_data[j].mode_mask))
				continue;

			if (data->transition_data[j].transition_rule(
				ppm_hica_algo_data, &data->transition_data[j])) {
				ppm_hica_algo_data.new_state = data->transition_data[j].next_state;
				ppm_dbg(HICA, "[%s(%d)] Need state transfer: %s --> %s\n",
					(i == 0) ? "PERF" : "PWR",
					j,
					ppm_get_power_state_name(cur_state),
					ppm_get_power_state_name(ppm_hica_algo_data.new_state)
					);
				goto end;
			} else {
				ppm_hica_algo_data.new_state = cur_state;
				ppm_dbg(HICA, "[%s(%d)]hold in %s state, loading_cnt = %d, freq_cnt = %d\n",
					(i == 0) ? "PERF" : "PWR",
					j,
					ppm_get_power_state_name(cur_state),
					data->transition_data[j].loading_hold_cnt,
					data->transition_data[j].freq_hold_cnt
					);
			}
		}
	}

end:
	ppm_unlock(&hica_policy.lock);
	FUNC_EXIT(FUNC_LV_HICA);
}

void ppm_hica_set_default_limit_by_state(enum ppm_power_state state,
					struct ppm_policy_data *policy)
{
	unsigned int i;
	struct ppm_power_state_data *state_info = ppm_get_power_state_info();

	FUNC_ENTER(FUNC_LV_HICA);

	for (i = 0; i < policy->req.cluster_num; i++) {
		if (state >= PPM_POWER_STATE_NONE) {
			if (state > NR_PPM_POWER_STATE)
				ppm_err("@%s: Invalid PPM state(%d)\n", __func__, state);

			policy->req.limit[i].min_cpu_core = get_cluster_min_cpu_core(i);
			policy->req.limit[i].max_cpu_core = get_cluster_max_cpu_core(i);
			policy->req.limit[i].min_cpufreq_idx = get_cluster_min_cpufreq_idx(i);
			policy->req.limit[i].max_cpufreq_idx = get_cluster_max_cpufreq_idx(i);

#ifdef PPM_DISABLE_CLUSTER_MIGRATION
			/* keep at least 1 LL */
			if (i == 0)
				policy->req.limit[i].min_cpu_core = 1;
#endif
		} else {
			policy->req.limit[i].min_cpu_core =
				state_info[state].cluster_limit->state_limit[i].min_cpu_core;
			policy->req.limit[i].max_cpu_core =
				state_info[state].cluster_limit->state_limit[i].max_cpu_core;
			policy->req.limit[i].min_cpufreq_idx =
				state_info[state].cluster_limit->state_limit[i].min_cpufreq_idx;
			policy->req.limit[i].max_cpufreq_idx =
				state_info[state].cluster_limit->state_limit[i].max_cpufreq_idx;
		}
	}

#ifdef PPM_IC_SEGMENT_CHECK
		/* ignore HICA min freq setting for L cluster in L_ONLY state */
		if (state == PPM_POWER_STATE_L_ONLY && ppm_main_info.fix_state_by_segment == PPM_POWER_STATE_L_ONLY)
					policy->req.limit[1].min_cpufreq_idx = get_cluster_min_cpufreq_idx(1);
#endif

	FUNC_EXIT(FUNC_LV_HICA);
}

enum ppm_power_state ppm_hica_get_state_by_perf_idx(enum ppm_power_state state, unsigned int perf_idx)
{
	enum ppm_power_state new_state = state;
	struct ppm_power_state_data *state_info;
	unsigned int level = 0, found = 0;

	FUNC_ENTER(FUNC_LV_HICA);

	/* Power state is fixed by user */
	if (fix_power_state != PPM_POWER_STATE_NONE)
		return fix_power_state;

	if (state >= PPM_POWER_STATE_NONE)
		return PPM_POWER_STATE_NONE;

	state_info = ppm_get_power_state_info();

	if (perf_idx >= state_info[NR_PPM_POWER_STATE-1].max_perf_idx) {
		/* use the most powerful state */
		found = 1;
		new_state = NR_PPM_POWER_STATE - 1;
		goto done;
	}

	while (1) {
		if (perf_idx <= state_info[new_state].max_perf_idx) {
			found = 1;
			break;
		}

		new_state = ppm_find_next_state(state, &level, PERFORMANCE);
		if (new_state == PPM_POWER_STATE_NONE) {
			ppm_warn("HICA state not found for idx = %d, cur_state = %d\n",
					perf_idx, state);
			break;
		}

		level++; /* to find next state */
	}

done:
	FUNC_EXIT(FUNC_LV_HICA);

	return (found) ? new_state : PPM_POWER_STATE_NONE;
}

enum ppm_power_state ppm_hica_get_state_by_pwr_budget(enum ppm_power_state state, unsigned int budget)
{
	enum ppm_power_state new_state = state;
	struct ppm_power_state_data *state_info;
	unsigned int level = 0, found = 0;

	FUNC_ENTER(FUNC_LV_HICA);

	/* Power state is fixed by user */
	if (fix_power_state != PPM_POWER_STATE_NONE)
		return fix_power_state;

	if (state >= PPM_POWER_STATE_NONE)
		return PPM_POWER_STATE_NONE;

	state_info = ppm_get_power_state_info();

	while (1) {
		if (budget >= state_info[new_state].min_pwr_idx) {
			found = 1;
			break;
		}

		new_state = ppm_find_next_state(state, &level, LOW_POWER);
		if (new_state == PPM_POWER_STATE_NONE) {
			ppm_warn("HICA state not found for budget = %d, cur_state = %d\n",
					budget, state);
			break;
		}

		level++; /* to find next state */
	}

	FUNC_EXIT(FUNC_LV_HICA);

	return (found) ? new_state : PPM_POWER_STATE_NONE;
}

enum ppm_power_state ppm_hica_get_cur_state(void)
{
	enum ppm_power_state state;

	FUNC_ENTER(FUNC_LV_HICA);

	ppm_lock(&hica_policy.lock);

	if (!hica_policy.is_enabled) {
		state = PPM_POWER_STATE_NONE;
		goto end;
	}

	if (!hica_policy.is_activated) {
		unsigned int i;

		for (i = 0; i < hica_policy.req.cluster_num; i++) {
			hica_policy.req.limit[i].min_cpufreq_idx = get_cluster_min_cpufreq_idx(i);
			hica_policy.req.limit[i].max_cpufreq_idx = get_cluster_max_cpufreq_idx(i);
			hica_policy.req.limit[i].min_cpu_core = get_cluster_min_cpu_core(i);
			hica_policy.req.limit[i].max_cpu_core = get_cluster_max_cpu_core(i);
		}
		state = PPM_POWER_STATE_NONE;
	} else
		state = (fix_power_state != PPM_POWER_STATE_NONE)
			? fix_power_state : ppm_hica_algo_data.new_state;

end:
	ppm_unlock(&hica_policy.lock);

	FUNC_EXIT(FUNC_LV_HICA);

	return state;
}

void ppm_hica_fix_root_cluster_changed(int cluster_id)
{
	enum ppm_power_state new_state;

	FUNC_ENTER(FUNC_LV_HICA);

	new_state = ppm_change_state_with_fix_root_cluster(ppm_hica_algo_data.cur_state, cluster_id);
	if (new_state != ppm_hica_algo_data.cur_state) {
		ppm_info("@%s: ppm state change to %s due to root cluster is fixed at %d\n",
			__func__, ppm_get_power_state_name(new_state), ppm_main_info.fixed_root_cluster);
		ppm_lock(&hica_policy.lock);
		ppm_hica_algo_data.new_state = new_state;
		ppm_unlock(&hica_policy.lock);
		mt_ppm_main();
	}

	FUNC_EXIT(FUNC_LV_HICA);
}

static void ppm_hica_reset_data_for_state(enum ppm_power_state state)
{
	struct ppm_power_state_data *state_info = ppm_get_power_state_info();
	struct ppm_state_transfer_data *data;
	int i, j;

	FUNC_ENTER(FUNC_LV_HICA);

	for (i = 0; i < 2; i++) {
		if (i == 0)
			data = state_info[state].transfer_by_pwr;
		else
			data = state_info[state].transfer_by_perf;

		for (j = 0; j < data->size; j++) {
			data->transition_data[j].freq_hold_cnt = 0;
			data->transition_data[j].loading_hold_cnt = 0;
		}
	}

	FUNC_EXIT(FUNC_LV_HICA);
}

static void ppm_hica_update_limit_cb(enum ppm_power_state new_state)
{
	FUNC_ENTER(FUNC_LV_HICA);

	ppm_ver("@%s: hica policy update limit for new state = %s\n",
		__func__, ppm_get_power_state_name(new_state));

	ppm_hica_set_default_limit_by_state(new_state, &hica_policy);

	if (new_state >= NR_PPM_POWER_STATE)
		ppm_dbg(HICA, "PPM current state is NONE, skip HICA result...\n");
	else if (new_state != ppm_hica_algo_data.cur_state) {
		/* update HICA algo data for state transition */
		ppm_dbg(HICA, "state transfer (final): %s --> %s\n",
			ppm_get_power_state_name(ppm_hica_algo_data.cur_state),
			ppm_get_power_state_name(new_state)
			);
		ppm_hica_algo_data.cur_state = new_state;
		ppm_hica_reset_data_for_state(new_state);
	}

	FUNC_EXIT(FUNC_LV_HICA);
}

static void ppm_hica_status_change_cb(bool enable)
{
	FUNC_ENTER(FUNC_LV_HICA);

	/* HICA policy is default active if it is enabled */
	if (enable)
		hica_policy.is_activated = true;

	ppm_dbg(HICA, "@%s: hica policy status changed to %d\n", __func__, enable);

	FUNC_EXIT(FUNC_LV_HICA);
}

static void ppm_hica_mode_change_cb(enum ppm_mode mode)
{
	FUNC_ENTER(FUNC_LV_HICA);

	ppm_dbg(HICA, "@%s: ppm mode changed to %d\n", __func__, mode);

	FUNC_EXIT(FUNC_LV_HICA);
}

static int ppm_hica_power_state_proc_show(struct seq_file *m, void *v)
{
	struct ppm_power_state_data *state_info = ppm_get_power_state_info();
	enum ppm_power_state cur_state = (fix_power_state != PPM_POWER_STATE_NONE)
		? fix_power_state : ppm_hica_algo_data.cur_state;
	int i;

	seq_printf(m, "\nhica_state = %d (%s)\n\n", cur_state, ppm_get_power_state_name(cur_state));

	for_each_ppm_power_state(i)
		seq_printf(m, "[%d] %s\n", state_info[i].state, state_info[i].name);

	seq_puts(m, "\nNote: echo -1 to re-enable HICA algo!\n");
	return 0;
}

static ssize_t ppm_hica_power_state_proc_write(struct file *file, const char __user *buffer,
					size_t count, loff_t *pos)
{
	int state;

	char *buf = ppm_copy_from_user_for_proc(buffer, count);

	if (!buf)
		return -EINVAL;

	if (!kstrtoint(buf, 10, &state)) {
#ifdef PPM_DISABLE_CLUSTER_MIGRATION
		if (state == PPM_POWER_STATE_L_ONLY || state == PPM_POWER_STATE_4L_LL)
			ppm_warn("Invalid state(%d) since cluster migration is disabled!\n", state);
		else
			fix_power_state = (state == -1) ? PPM_POWER_STATE_NONE : state;
#else
		fix_power_state = (state == -1) ? PPM_POWER_STATE_NONE : state;
#endif
		ppm_info("@%s: fix_power_state = %s\n", __func__, ppm_get_power_state_name(fix_power_state));
	} else
		ppm_err("echo (state idx) > /proc/ppm/policy/hica_power_state\n");

	free_page((unsigned long)buf);
	return count;
}

PROC_FOPS_RW(hica_power_state);
PROC_FOPS_RW_HICA_SETTINGS(mode_mask, p->mode_mask);
PROC_FOPS_RW_HICA_SETTINGS(loading_delta, p->loading_delta);
PROC_FOPS_RW_HICA_SETTINGS(loading_hold_time, p->loading_hold_time);
PROC_FOPS_RO_HICA_SETTINGS(loading_hold_cnt, p->loading_hold_cnt);
PROC_FOPS_RW_HICA_SETTINGS(loading_bond, p->loading_bond);
PROC_FOPS_RW_HICA_SETTINGS(freq_hold_time, p->freq_hold_time);
PROC_FOPS_RO_HICA_SETTINGS(freq_hold_cnt, p->freq_hold_cnt);
PROC_FOPS_RW_HICA_SETTINGS(tlp_bond, p->tlp_bond);

#define OUTPUT_BUF_SIZE	32

static int __init ppm_hica_policy_init(void)
{
	int ret = 0, i, j, k;
	struct ppm_power_state_data *state_info = ppm_get_power_state_info();
	struct proc_dir_entry *hica_setting_dir = NULL;
	struct proc_dir_entry *trans_rule_dir = NULL;
	struct ppm_state_transfer_data *data;
	char str[OUTPUT_BUF_SIZE];

	struct pentry {
		const char *name;
		const struct file_operations *fops;
	};

	const struct pentry entries[] = {
		PROC_ENTRY(hica_power_state),
	};

	const struct pentry trans_rule_entries[] = {
		PROC_ENTRY(mode_mask),
		PROC_ENTRY(loading_delta),
		PROC_ENTRY(loading_hold_time),
		PROC_ENTRY(loading_hold_cnt),
		PROC_ENTRY(loading_bond),
		PROC_ENTRY(freq_hold_time),
		PROC_ENTRY(freq_hold_cnt),
		PROC_ENTRY(tlp_bond),
	};

	FUNC_ENTER(FUNC_LV_HICA);

	/* create procfs */
	for (i = 0; i < ARRAY_SIZE(entries); i++) {
		if (!proc_create(entries[i].name, S_IRUGO | S_IWUSR | S_IWGRP, policy_dir, entries[i].fops)) {
			ppm_err("%s(), create /proc/ppm/policy/%s failed\n", __func__, entries[i].name);
			ret = -EINVAL;
			goto out;
		}
	}

	hica_setting_dir = proc_mkdir("hica_settings", policy_dir);
	if (!hica_setting_dir) {
		ppm_err("fail to create /proc/ppm/policy/hica_settings dir\n");
		return -ENOMEM;
	}

	for (i = 0; i < NR_PPM_POWER_STATE * 2; i++) {
		data = (i % 2 == 0) ? state_info[i / 2].transfer_by_perf
				: state_info[i / 2].transfer_by_pwr;

		/* create dir for transfer rule */
		for (j = 0; j < data->size; j++) {
			if (!data->transition_data[j].transition_rule)
				continue;

			snprintf(str, OUTPUT_BUF_SIZE, "%s_to_%s", ppm_get_power_state_name(i / 2),
				ppm_get_power_state_name(data->transition_data[j].next_state));
			trans_rule_dir = proc_mkdir(str, hica_setting_dir);
			if (!trans_rule_dir) {
				ppm_err("fail to create /proc/ppm/policy/hica_settings/%s dir\n", str);
				return -ENOMEM;
			}

			/* create node for each parameter in trans_rule_entries */
			for (k = 0; k < ARRAY_SIZE(trans_rule_entries); k++) {
				if (!proc_create_data(trans_rule_entries[k].name,
					S_IRUGO | S_IWUSR | S_IWGRP, trans_rule_dir,
					trans_rule_entries[k].fops, &data->transition_data[j]))
					ppm_err("fail to create /proc/ppm/policy/hica_settings/%s/%s dir\n",
						str, trans_rule_entries[k].name);
			}
		}
	}

	if (ppm_main_register_policy(&hica_policy)) {
		ppm_err("@%s: hica policy register failed\n", __func__);
		ret = -EINVAL;
		goto out;
	}

	/* default enabled */
	hica_policy.is_activated = true;

	ppm_info("@%s: register %s done!\n", __func__, hica_policy.name);

out:
	FUNC_EXIT(FUNC_LV_HICA);

	return ret;
}

static void __exit ppm_hica_policy_exit(void)
{
	FUNC_ENTER(FUNC_LV_HICA);

	ppm_main_unregister_policy(&hica_policy);

	FUNC_EXIT(FUNC_LV_HICA);
}

module_init(ppm_hica_policy_init);
module_exit(ppm_hica_policy_exit);

