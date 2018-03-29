/*
 * Copyright (C) 2015 MediaTek Inc.
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
#include <linux/string.h>

#include "mt_ppm_internal.h"


static enum ppm_power_state ppm_sysboost_get_power_state_cb(enum ppm_power_state cur_state);
static void ppm_sysboost_update_limit_cb(enum ppm_power_state new_state);
static void ppm_sysboost_status_change_cb(bool enable);
static void ppm_sysboost_mode_change_cb(enum ppm_mode mode);

/* other members will init by ppm_main */
static struct ppm_policy_data sysboost_policy = {
	.name			= __stringify(PPM_POLICY_SYS_BOOST),
	.lock			= __MUTEX_INITIALIZER(sysboost_policy.lock),
	.policy			= PPM_POLICY_SYS_BOOST,
	.priority		= PPM_POLICY_PRIO_PERFORMANCE_BASE,
	.get_power_state_cb	= ppm_sysboost_get_power_state_cb,
	.update_limit_cb	= ppm_sysboost_update_limit_cb,
	.status_change_cb	= ppm_sysboost_status_change_cb,
	.mode_change_cb		= ppm_sysboost_mode_change_cb,
};

struct ppm_sysboost_data {
	enum ppm_sysboost_user user;
	char *user_name;
	unsigned int min_freq;
	unsigned int min_core_num;
	struct list_head link;
};

static LIST_HEAD(sysboost_user_list);
static struct ppm_sysboost_data sysboost_data[NR_PPM_SYSBOOST_USER];
static unsigned int target_boost_core;
static unsigned int target_boost_freq;


void mt_ppm_sysboost_core(enum ppm_sysboost_user user, unsigned int core_num)
{
	struct ppm_sysboost_data *data;
	unsigned int max_core_num = 0;

	if (core_num > num_possible_cpus() || user >= NR_PPM_SYSBOOST_USER) {
		ppm_err("@%s: Invalid input: user = %d, core_num = %d\n",
			__func__, user, core_num);
		return;
	}

	ppm_lock(&sysboost_policy.lock);

	if (!sysboost_policy.is_enabled) {
		ppm_err("@%s: sysboost policy is not enabled!\n", __func__);
		ppm_unlock(&sysboost_policy.lock);
		return;
	}

	/* update user core setting */
	list_for_each_entry_reverse(data, &sysboost_user_list, link) {
		if (data->user == user)
			data->min_core_num = core_num;

		/* find max boost core num */
		max_core_num = MAX(max_core_num, data->min_core_num);
	}

	target_boost_core = max_core_num;
	sysboost_policy.is_activated = (target_boost_core || target_boost_freq) ? true : false;

	ppm_info("sys boost by %s: req_core = %d, target_boost_core = %d\n",
		sysboost_data[user].user_name, core_num, target_boost_core);

	ppm_unlock(&sysboost_policy.lock);
	ppm_task_wakeup();
}

void mt_ppm_sysboost_freq(enum ppm_sysboost_user user, unsigned int freq)
{
	struct ppm_sysboost_data *data;
	unsigned int max_freq = 0;

	if (user >= NR_PPM_SYSBOOST_USER) {
		ppm_err("@%s: Invalid input: user = %d, freq = %d\n",
			__func__, user, freq);
		return;
	}

	ppm_lock(&sysboost_policy.lock);

	if (!sysboost_policy.is_enabled) {
		ppm_err("@%s: sysboost policy is not enabled!\n", __func__);
		ppm_unlock(&sysboost_policy.lock);
		return;
	}

	/* update user freq setting */
	list_for_each_entry_reverse(data, &sysboost_user_list, link) {
		if (data->user == user)
			data->min_freq = freq;

		/* find max boost freq */
		max_freq = MAX(max_freq, data->min_freq);
	}

	target_boost_freq = max_freq;
	sysboost_policy.is_activated = (target_boost_core || target_boost_freq) ? true : false;

	ppm_info("sys boost by %s: req_freq = %d, target_boost_freq = %d\n",
		sysboost_data[user].user_name, freq, target_boost_freq);

	ppm_unlock(&sysboost_policy.lock);
	ppm_task_wakeup();
}

static enum ppm_power_state ppm_sysboost_get_power_state_cb(enum ppm_power_state cur_state)
{
	if (target_boost_core > 4)
		return (ppm_main_info.fixed_root_cluster == 1)
			? PPM_POWER_STATE_4L_LL : PPM_POWER_STATE_4LL_L;
	else
		return cur_state;
}

static void ppm_sysboost_update_limit_cb(enum ppm_power_state new_state)
{
	unsigned int LL_max = get_cluster_max_cpu_core(0);
	unsigned int L_max = get_cluster_max_cpu_core(1);

	FUNC_ENTER(FUNC_LV_POLICY);

	ppm_ver("@%s: sysboost policy update limit for new state = %s\n",
		__func__, ppm_get_power_state_name(new_state));

	if (!target_boost_core && !target_boost_freq)
		goto end;

	ppm_hica_set_default_limit_by_state(new_state, &sysboost_policy);

	/* get limit according to perf idx */
	switch (new_state) {
	case PPM_POWER_STATE_LL_ONLY:
		sysboost_policy.req.limit[0].min_cpu_core =
			(target_boost_core >= LL_max) ? LL_max : target_boost_core;
		sysboost_policy.req.limit[0].min_cpufreq_idx = MIN(
			sysboost_policy.req.limit[0].min_cpufreq_idx,
			ppm_main_freq_to_idx(0, target_boost_freq, CPUFREQ_RELATION_L)
			);
		break;
	case PPM_POWER_STATE_L_ONLY:
		sysboost_policy.req.limit[1].min_cpu_core =
			(target_boost_core >= L_max) ? L_max : target_boost_core;
		sysboost_policy.req.limit[1].min_cpufreq_idx = MIN(
			sysboost_policy.req.limit[1].min_cpufreq_idx,
			ppm_main_freq_to_idx(1, target_boost_freq, CPUFREQ_RELATION_L)
			);
		break;
	case PPM_POWER_STATE_4L_LL:
		if (target_boost_core > L_max) {
			sysboost_policy.req.limit[1].min_cpu_core = L_max;
			sysboost_policy.req.limit[0].min_cpu_core =
				(target_boost_core - L_max >= LL_max) ? LL_max : (target_boost_core - L_max);
		} else
			sysboost_policy.req.limit[1].min_cpu_core = target_boost_core;

		/* boost both due to shared buck */
		sysboost_policy.req.limit[0].min_cpufreq_idx = MIN(
			sysboost_policy.req.limit[0].min_cpufreq_idx,
			ppm_main_freq_to_idx(0, target_boost_freq, CPUFREQ_RELATION_L)
			);
		sysboost_policy.req.limit[1].min_cpufreq_idx = MIN(
			sysboost_policy.req.limit[1].min_cpufreq_idx,
			ppm_main_freq_to_idx(1, target_boost_freq, CPUFREQ_RELATION_L)
			);
		break;
	case PPM_POWER_STATE_4LL_L:
	case PPM_POWER_STATE_NONE:
	default:
		if (target_boost_core > LL_max) {
			sysboost_policy.req.limit[0].min_cpu_core = LL_max;
			sysboost_policy.req.limit[1].min_cpu_core =
				(target_boost_core - LL_max >= L_max) ? L_max : (target_boost_core - LL_max);
		} else
			sysboost_policy.req.limit[0].min_cpu_core = target_boost_core;

		/* boost both due to shared buck */
		sysboost_policy.req.limit[0].min_cpufreq_idx = MIN(
			sysboost_policy.req.limit[0].min_cpufreq_idx,
			ppm_main_freq_to_idx(0, target_boost_freq, CPUFREQ_RELATION_L)
			);
		sysboost_policy.req.limit[1].min_cpufreq_idx = MIN(
			sysboost_policy.req.limit[1].min_cpufreq_idx,
			ppm_main_freq_to_idx(1, target_boost_freq, CPUFREQ_RELATION_L)
			);
		break;
	}

end:
	FUNC_EXIT(FUNC_LV_POLICY);
}

static void ppm_sysboost_status_change_cb(bool enable)
{
	FUNC_ENTER(FUNC_LV_POLICY);

	ppm_ver("@%s: sysboost policy status changed to %d\n", __func__, enable);

	FUNC_EXIT(FUNC_LV_POLICY);
}

static void ppm_sysboost_mode_change_cb(enum ppm_mode mode)
{
	FUNC_ENTER(FUNC_LV_POLICY);

	ppm_ver("@%s: ppm mode changed to %d\n", __func__, mode);

	FUNC_EXIT(FUNC_LV_POLICY);
}

static int ppm_sysboost_core_proc_show(struct seq_file *m, void *v)
{
	struct ppm_sysboost_data *data;

	/* update user core setting */
	list_for_each_entry_reverse(data, &sysboost_user_list, link)
		seq_printf(m, "[%d] %s: %d\n", data->user, data->user_name, data->min_core_num);

	seq_printf(m, "target_boost_core = %d\n", target_boost_core);

	return 0;
}

static ssize_t ppm_sysboost_core_proc_write(struct file *file, const char __user *buffer,
					size_t count, loff_t *pos)
{
	int user, core;

	char *buf = ppm_copy_from_user_for_proc(buffer, count);

	if (!buf)
		return -EINVAL;

	if (sscanf(buf, "%d %d", &user, &core) == 2)
		mt_ppm_sysboost_core((enum ppm_sysboost_user)user, core);
	else
		ppm_err("@%s: Invalid input!\n", __func__);

	free_page((unsigned long)buf);
	return count;
}

static int ppm_sysboost_freq_proc_show(struct seq_file *m, void *v)
{
	struct ppm_sysboost_data *data;

	/* update user core setting */
	list_for_each_entry_reverse(data, &sysboost_user_list, link)
		seq_printf(m, "[%d] %s: %d\n", data->user, data->user_name, data->min_freq);

	seq_printf(m, "target_boost_freq = %d\n", target_boost_freq);

	return 0;
}

static ssize_t ppm_sysboost_freq_proc_write(struct file *file, const char __user *buffer,
					size_t count, loff_t *pos)
{
	int user, freq;

	char *buf = ppm_copy_from_user_for_proc(buffer, count);

	if (!buf)
		return -EINVAL;

	if (sscanf(buf, "%d %d", &user, &freq) == 2)
		mt_ppm_sysboost_freq((enum ppm_sysboost_user)user, freq);
	else
		ppm_err("@%s: Invalid input!\n", __func__);

	free_page((unsigned long)buf);
	return count;
}

PROC_FOPS_RW(sysboost_core);
PROC_FOPS_RW(sysboost_freq);

static int __init ppm_sysboost_policy_init(void)
{
	int i, ret = 0;

	struct pentry {
		const char *name;
		const struct file_operations *fops;
	};

	const struct pentry entries[] = {
		PROC_ENTRY(sysboost_core),
		PROC_ENTRY(sysboost_freq),
	};

	FUNC_ENTER(FUNC_LV_POLICY);

	/* create procfs */
	for (i = 0; i < ARRAY_SIZE(entries); i++) {
		if (!proc_create(entries[i].name, S_IRUGO | S_IWUSR | S_IWGRP, policy_dir, entries[i].fops)) {
			ppm_err("%s(), create /proc/ppm/policy/%s failed\n", __func__, entries[i].name);
			ret = -EINVAL;
			goto out;
		}
	}

	/* list init */
	for (i = 0; i < NR_PPM_SYSBOOST_USER; i++) {
		sysboost_data[i].user = (enum ppm_sysboost_user)i;
		sysboost_data[i].min_core_num = 0;
		sysboost_data[i].min_freq = 0;

		switch (sysboost_data[i].user) {
		case BOOST_BY_WIFI:
			sysboost_data[i].user_name = "WIFI";
			break;
		case BOOST_BY_PERFSERV:
			sysboost_data[i].user_name = "PERFSERV";
			break;
		case BOOST_BY_USB:
			sysboost_data[i].user_name = "USB";
			break;
		case BOOST_BY_UT:
		default:
			sysboost_data[i].user_name = "UT";
			break;
		}

		list_add(&sysboost_data[i].link, &sysboost_user_list);
	}

	if (ppm_main_register_policy(&sysboost_policy)) {
		ppm_err("@%s: sysboost policy register failed\n", __func__);
		ret = -EINVAL;
		goto out;
	}

	ppm_info("@%s: register %s done!\n", __func__, sysboost_policy.name);

out:
	FUNC_EXIT(FUNC_LV_POLICY);

	return ret;
}

static void __exit ppm_sysboost_policy_exit(void)
{
	FUNC_ENTER(FUNC_LV_POLICY);

	ppm_main_unregister_policy(&sysboost_policy);

	FUNC_EXIT(FUNC_LV_POLICY);
}

module_init(ppm_sysboost_policy_init);
module_exit(ppm_sysboost_policy_exit);

