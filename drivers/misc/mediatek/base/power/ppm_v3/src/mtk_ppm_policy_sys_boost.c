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
#include <linux/string.h>
#include <linux/slab.h>

#include "mtk_ppm_internal.h"


static void ppm_sysboost_update_limit_cb(void);
static void ppm_sysboost_status_change_cb(bool enable);

/* other members will init by ppm_main */
static struct ppm_policy_data sysboost_policy = {
	.name			= __stringify(PPM_POLICY_SYS_BOOST),
	.lock			= __MUTEX_INITIALIZER(sysboost_policy.lock),
	.policy			= PPM_POLICY_SYS_BOOST,
	.priority		= PPM_POLICY_PRIO_PERFORMANCE_BASE,
	.update_limit_cb	= ppm_sysboost_update_limit_cb,
	.status_change_cb	= ppm_sysboost_status_change_cb,
};

struct ppm_sysboost_data {
	enum ppm_sysboost_user user;
	char *user_name;
	/* record user input for old interface */
	unsigned int min_freq;
	unsigned int min_core_num;

	struct ppm_user_limit limit[NR_PPM_CLUSTERS];
	struct list_head link;
};

static LIST_HEAD(sysboost_user_list);
/* to save each user's request */
static struct ppm_sysboost_data sysboost_data[NR_PPM_SYSBOOST_USER];
/* final limit */
static struct ppm_userlimit_data sysboost_final_limit = {
	.is_freq_limited_by_user = false,
	.is_core_limited_by_user = false,
};


static void ppm_sysboost_dump_final_limit(void *data)
{
	int i;

	if (!data) {
		ppm_dbg(SYS_BOOST,
			"is_core_limited = %d, is_freq_limited = %d\n",
			sysboost_final_limit.is_core_limited_by_user,
			sysboost_final_limit.is_freq_limited_by_user);
		for_each_ppm_clusters(i) {
			ppm_dbg(SYS_BOOST, "cluster %d = (%d)(%d)(%d)(%d)\n",
				i,
				sysboost_final_limit.limit[i].min_freq_idx,
				sysboost_final_limit.limit[i].max_freq_idx,
				sysboost_final_limit.limit[i].min_core_num,
				sysboost_final_limit.limit[i].max_core_num);
		}
	} else {
		struct seq_file *m = (struct seq_file *)data;

		seq_printf(m, "is_core_limited = %d, is_freq_limited = %d\n",
			sysboost_final_limit.is_core_limited_by_user,
			sysboost_final_limit.is_freq_limited_by_user);
		for_each_ppm_clusters(i) {
			seq_printf(m, "cluster %d = (%d)(%d)(%d)(%d)\n", i,
				sysboost_final_limit.limit[i].min_freq_idx,
				sysboost_final_limit.limit[i].max_freq_idx,
				sysboost_final_limit.limit[i].min_core_num,
				sysboost_final_limit.limit[i].max_core_num);
		}
	}
}

/* must acquire sysboost_policy lock before calling this functions! */
static void ppm_sysboost_update_final_limit(void)
{
	struct ppm_sysboost_data *data;
	int min_core, max_core, min_freq_idx, max_freq_idx;
	int i;

	/* update user core setting */
	for_each_ppm_clusters(i) {
		min_core = max_core = min_freq_idx = max_freq_idx = -1;

		list_for_each_entry_reverse(data, &sysboost_user_list, link) {
			min_core = MAX(min_core, data->limit[i].min_core_num);
			max_core = (data->limit[i].max_core_num == -1)
				? max_core
				: (max_core == -1)
				? data->limit[i].max_core_num
				: MIN(max_core, data->limit[i].max_core_num);
			min_freq_idx = (data->limit[i].min_freq_idx == -1)
				? min_freq_idx
				: (min_freq_idx == -1)
				? data->limit[i].min_freq_idx
				: MIN(min_freq_idx,
				data->limit[i].min_freq_idx);
			max_freq_idx = MAX(max_freq_idx,
				data->limit[i].max_freq_idx);
		}

		/* error check */
		if (min_freq_idx != -1 && min_freq_idx < max_freq_idx)
			min_freq_idx = max_freq_idx;
		if (max_core != -1 && min_core > max_core)
			min_core = max_core;

		sysboost_final_limit.limit[i].min_freq_idx = min_freq_idx;
		sysboost_final_limit.limit[i].max_freq_idx = max_freq_idx;
		sysboost_final_limit.limit[i].min_core_num = min_core;
		sysboost_final_limit.limit[i].max_core_num = max_core;
	}

	sysboost_final_limit.is_freq_limited_by_user = false;
	sysboost_final_limit.is_core_limited_by_user = false;
	for_each_ppm_clusters(i) {
		if (!sysboost_final_limit.is_freq_limited_by_user
			&& (sysboost_final_limit.limit[i].min_freq_idx != -1
			|| sysboost_final_limit.limit[i].max_freq_idx != -1)) {
			sysboost_final_limit.is_freq_limited_by_user = true;
		}
		if (!sysboost_final_limit.is_core_limited_by_user
			&& (sysboost_final_limit.limit[i].min_core_num != -1
			|| sysboost_final_limit.limit[i].max_core_num != -1)) {
			sysboost_final_limit.is_core_limited_by_user = true;
		}
		if (sysboost_final_limit.is_freq_limited_by_user
			&& sysboost_final_limit.is_core_limited_by_user)
			break;
	}

	if (!sysboost_final_limit.is_freq_limited_by_user
			&& !sysboost_final_limit.is_core_limited_by_user)
		sysboost_policy.is_activated = false;
	else
		sysboost_policy.is_activated = true;

	ppm_sysboost_dump_final_limit(NULL);
}


void mt_ppm_sysboost_core(enum ppm_sysboost_user user, unsigned int core_num)
{
	/* phase-out */
}
EXPORT_SYMBOL(mt_ppm_sysboost_core);

void mt_ppm_sysboost_freq(enum ppm_sysboost_user user, unsigned int freq)
{
	struct ppm_sysboost_data *data;
	int i, freq_idx;

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

	ppm_info("sys boost by %s: req_freq = %d\n",
		sysboost_data[user].user_name, freq);

	/* update user freq setting */
	list_for_each_entry_reverse(data, &sysboost_user_list, link) {
		if (data->user == user) {
			data->min_freq = freq;

			/* clear previous setting */
			if (freq == 0) {
				for_each_ppm_clusters(i)
					data->limit[i].min_freq_idx = -1;
				break;
			}

			for_each_ppm_clusters(i) {
				freq_idx = (freq == -1) ? -1
					: ppm_main_freq_to_idx(i, freq,
					CPUFREQ_RELATION_L);

				/* error check */
				if (data->limit[i].max_freq_idx != -1
					&& freq_idx != -1
					&& freq_idx <
					data->limit[i].max_freq_idx)
					freq_idx = data->limit[i].max_freq_idx;

				data->limit[i].min_freq_idx = freq_idx;
			}
		}
	}

	ppm_sysboost_update_final_limit();

	ppm_unlock(&sysboost_policy.lock);

	mt_ppm_main();
}
EXPORT_SYMBOL(mt_ppm_sysboost_freq);

void mt_ppm_sysboost_set_core_limit(enum ppm_sysboost_user user,
	unsigned int cluster, int min_core, int max_core)
{
	/* phase-out */
}
EXPORT_SYMBOL(mt_ppm_sysboost_set_core_limit);

void mt_ppm_sysboost_set_freq_limit(enum ppm_sysboost_user user,
	unsigned int cluster, int min_freq, int max_freq)
{
	struct ppm_sysboost_data *data;

	if (cluster >= NR_PPM_CLUSTERS) {
		ppm_err("Invalid input: cluster = %d\n", cluster);
		return;
	}

	if ((max_freq != -1 && max_freq > get_cluster_max_cpufreq(cluster))
		|| (min_freq != -1
		&& min_freq < get_cluster_min_cpufreq(cluster))
		|| user >= NR_PPM_SYSBOOST_USER) {
		ppm_err("Invalid input: user/cl=%d/%d, min/max freq=%d/%d\n",
			user, cluster, min_freq, max_freq);
		return;
	}

	ppm_info("sys boost by %s: cluster %d min/max freq = %d/%d\n",
		sysboost_data[user].user_name, cluster, min_freq, max_freq);

	if (min_freq > max_freq && max_freq != -1)
		min_freq = max_freq;

	ppm_lock(&sysboost_policy.lock);

	if (!sysboost_policy.is_enabled) {
		ppm_err("@%s: sysboost policy is not enabled!\n", __func__);
		ppm_unlock(&sysboost_policy.lock);
		return;
	}

	/* update user freq setting */
	list_for_each_entry_reverse(data, &sysboost_user_list, link) {
		if (data->user == user) {
			data->limit[cluster].min_freq_idx = (min_freq == -1)
				? -1
				: ppm_main_freq_to_idx(cluster, min_freq,
				CPUFREQ_RELATION_L);
			data->limit[cluster].max_freq_idx = (max_freq == -1)
				? -1
				: ppm_main_freq_to_idx(cluster, max_freq,
				CPUFREQ_RELATION_H);

			/* error check */
			if (data->limit[cluster].min_freq_idx != -1
				&& data->limit[cluster].min_freq_idx
				< data->limit[cluster].max_freq_idx)
				data->limit[cluster].min_freq_idx =
					data->limit[cluster].max_freq_idx;
		}
	}

	ppm_sysboost_update_final_limit();

	ppm_unlock(&sysboost_policy.lock);

	mt_ppm_main();
}
EXPORT_SYMBOL(mt_ppm_sysboost_set_freq_limit);

static void ppm_sysboost_update_limit_cb(void)
{
	unsigned int i;
	struct ppm_policy_req *req = &sysboost_policy.req;
	struct ppm_userlimit_data *p = &sysboost_final_limit;

	FUNC_ENTER(FUNC_LV_POLICY);

	if (p->is_freq_limited_by_user
		|| p->is_core_limited_by_user) {
		ppm_clear_policy_limit(&sysboost_policy);

		for (i = 0; i < req->cluster_num; i++) {
			req->limit[i].min_cpufreq_idx =
				(p->limit[i].min_freq_idx == -1)
				? req->limit[i].min_cpufreq_idx
				: p->limit[i].min_freq_idx;
			req->limit[i].max_cpufreq_idx =
				(p->limit[i].max_freq_idx == -1)
				? req->limit[i].max_cpufreq_idx
				: p->limit[i].max_freq_idx;
		}

		/* error check */
		for (i = 0; i < req->cluster_num; i++) {
			if (req->limit[i].max_cpu_core <
				req->limit[i].min_cpu_core)
				req->limit[i].min_cpu_core =
				req->limit[i].max_cpu_core;
			if (req->limit[i].max_cpufreq_idx >
				req->limit[i].min_cpufreq_idx)
				req->limit[i].min_cpufreq_idx =
				req->limit[i].max_cpufreq_idx;
		}
	}

	FUNC_EXIT(FUNC_LV_POLICY);
}

static void ppm_sysboost_status_change_cb(bool enable)
{
	FUNC_ENTER(FUNC_LV_POLICY);

	ppm_ver("sysboost policy status changed to %d\n", enable);

	FUNC_EXIT(FUNC_LV_POLICY);
}

static int ppm_sysboost_freq_proc_show(struct seq_file *m, void *v)
{
	struct ppm_sysboost_data *data;

	/* update user freq setting */
	list_for_each_entry_reverse(data, &sysboost_user_list, link)
		seq_printf(m, "[%d] %s: %d\n",
			data->user, data->user_name, data->min_freq);

	ppm_sysboost_dump_final_limit(m);

	return 0;
}

static ssize_t ppm_sysboost_freq_proc_write(struct file *file,
	const char __user *buffer, size_t count, loff_t *pos)
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

static int ppm_sysboost_cluster_freq_limit_proc_show(
	struct seq_file *m, void *v)
{
	struct ppm_sysboost_data *data;
	int i;

	/* update user freq setting */
	list_for_each_entry_reverse(data, &sysboost_user_list, link) {
		seq_printf(m, "[%d] %s: %d\t",
			data->user, data->user_name, data->min_freq);
		for_each_ppm_clusters(i)
			seq_printf(m, "(%d)(%d)",
				data->limit[i].min_freq_idx,
				data->limit[i].max_freq_idx);
		seq_puts(m, "\n");
	}

	ppm_sysboost_dump_final_limit(m);

	return 0;
}

static ssize_t ppm_sysboost_cluster_freq_limit_proc_write(struct file *file,
			const char __user *buffer, size_t count, loff_t *pos)
{
	int user, min_freq, max_freq, cluster;

	char *buf = ppm_copy_from_user_for_proc(buffer, count);

	if (!buf)
		return -EINVAL;

	if (sscanf(buf, "%d %d %d %d",
		&user, &cluster, &min_freq, &max_freq) == 4)
		mt_ppm_sysboost_set_freq_limit((enum ppm_sysboost_user)user,
			cluster, min_freq, max_freq);
	else
		ppm_err("@%s: Invalid input!\n", __func__);

	free_page((unsigned long)buf);
	return count;
}

static int ppm_sysboost_cluster_freqidx_limit_proc_show(
	struct seq_file *m, void *v)
{
	struct ppm_sysboost_data *data;
	int i;

	/* update user freq setting */
	list_for_each_entry_reverse(data, &sysboost_user_list, link) {
		seq_printf(m, "[%d] %s: %d\t",
			data->user, data->user_name, data->min_freq);
		for_each_ppm_clusters(i)
			seq_printf(m, "(%d)(%d)",
				data->limit[i].min_freq_idx,
				data->limit[i].max_freq_idx);
		seq_puts(m, "\n");
	}

	ppm_sysboost_dump_final_limit(m);

	return 0;
}

static ssize_t ppm_sysboost_cluster_freqidx_limit_proc_write(struct file *file,
			const char __user *buffer, size_t count, loff_t *pos)
{
	int user, min_freq_idx, max_freq_idx, cl;
	struct ppm_data *p = &ppm_main_info;
	char *buf = ppm_copy_from_user_for_proc(buffer, count);

	if (!buf)
		return -EINVAL;

	if (sscanf(buf, "%d %d %d %d",
			&user, &cl, &min_freq_idx, &max_freq_idx) == 4) {
		int min_freq, max_freq;

		if (cl < 0 || cl >= NR_PPM_CLUSTERS)
			goto end;

		if (!ppm_main_info.cluster_info[cl].dvfs_tbl)
			goto end;


		min_freq = (min_freq_idx < 0 || min_freq_idx >= DVFS_OPP_NUM)
			? -1
			: p->cluster_info[cl].dvfs_tbl[min_freq_idx].frequency;
		max_freq = (max_freq_idx < 0 || max_freq_idx >= DVFS_OPP_NUM)
			? -1
			: p->cluster_info[cl].dvfs_tbl[max_freq_idx].frequency;

		mt_ppm_sysboost_set_freq_limit((enum ppm_sysboost_user)user,
			cl, min_freq, max_freq);
	} else
		ppm_err("@%s: Invalid input!\n", __func__);

end:
	free_page((unsigned long)buf);
	return count;
}

PROC_FOPS_RW(sysboost_freq);
PROC_FOPS_RW(sysboost_cluster_freq_limit);
PROC_FOPS_RW(sysboost_cluster_freqidx_limit);

static int __init ppm_sysboost_policy_init(void)
{
	int i, j, ret = 0;

	struct pentry {
		const char *name;
		const struct file_operations *fops;
	};

	const struct pentry entries[] = {
		PROC_ENTRY(sysboost_freq),
		PROC_ENTRY(sysboost_cluster_freq_limit),
		PROC_ENTRY(sysboost_cluster_freqidx_limit),
	};

	FUNC_ENTER(FUNC_LV_POLICY);

	/* create procfs */
	for (i = 0; i < ARRAY_SIZE(entries); i++) {
		if (!proc_create(entries[i].name, 0644,
			policy_dir, entries[i].fops)) {
			ppm_err("%s(), create /proc/ppm/policy/%s failed\n",
				__func__, entries[i].name);
			ret = -EINVAL;
			goto out;
		}
	}

	/* allocate mem and init policy limit_data */
	sysboost_final_limit.limit =
		kcalloc(ppm_main_info.cluster_num,
			sizeof(*sysboost_final_limit.limit), GFP_KERNEL);
	if (!sysboost_final_limit.limit) {
		ret = -ENOMEM;
		goto out;
	}

	for_each_ppm_clusters(i) {
		sysboost_final_limit.limit[i].min_freq_idx = -1;
		sysboost_final_limit.limit[i].max_freq_idx = -1;
		sysboost_final_limit.limit[i].min_core_num = -1;
		sysboost_final_limit.limit[i].max_core_num = -1;
	}

	/* list init */
	for (i = 0; i < NR_PPM_SYSBOOST_USER; i++) {
		sysboost_data[i].user = (enum ppm_sysboost_user)i;

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
		case BOOST_BY_USB_PD:
			sysboost_data[i].user_name = "USB_PD";
			break;
		case BOOST_BY_DEBUGD:
			sysboost_data[i].user_name = "DEBUGD";
			break;
		case BOOST_BY_DEBUGD_64:
			sysboost_data[i].user_name = "DEBUGD_64";
			break;
		case BOOST_BY_BOOT_TIME_OPT:
			sysboost_data[i].user_name = "BOOT_TIME_OPT";
			break;
		case BOOST_BY_UT:
		default:
			sysboost_data[i].user_name = "UT";
			break;
		}

		for_each_ppm_clusters(j) {
			sysboost_data[i].limit[j].min_freq_idx = -1;
			sysboost_data[i].limit[j].max_freq_idx = -1;
			sysboost_data[i].limit[j].min_core_num = -1;
			sysboost_data[i].limit[j].max_core_num = -1;
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
	sysboost_policy.is_enabled = false;
	FUNC_EXIT(FUNC_LV_POLICY);

	return ret;
}

static void __exit ppm_sysboost_policy_exit(void)
{
	FUNC_ENTER(FUNC_LV_POLICY);

	kfree(sysboost_final_limit.limit);

	ppm_main_unregister_policy(&sysboost_policy);

	FUNC_EXIT(FUNC_LV_POLICY);
}

module_init(ppm_sysboost_policy_init);
module_exit(ppm_sysboost_policy_exit);

