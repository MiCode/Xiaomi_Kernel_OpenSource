// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>

#include "mtk_ppm_internal.h"


static void ppm_hard_userlimit_update_limit_cb(void);
static void ppm_hard_userlimit_status_change_cb(bool enable);

/* other members will init by ppm_main */
static struct ppm_policy_data hard_userlimit_policy = {
	.name			= __stringify(PPM_POLICY_HARD_USER_LIMIT),
	.lock			= __MUTEX_INITIALIZER
				(hard_userlimit_policy.lock),
	.policy			= PPM_POLICY_HARD_USER_LIMIT,
	.priority		= PPM_POLICY_PRIO_USER_SPECIFY_BASE,
	.update_limit_cb	= ppm_hard_userlimit_update_limit_cb,
	.status_change_cb	= ppm_hard_userlimit_status_change_cb,
};

struct ppm_userlimit_data hard_userlimit_data = {
	.is_freq_limited_by_user = false,
	.is_core_limited_by_user = false,
};


/* MUST in lock */
static bool ppm_hard_userlimit_is_policy_active(void)
{
	if (!hard_userlimit_data.is_freq_limited_by_user
		&& !hard_userlimit_data.is_core_limited_by_user)
		return false;
	else
		return true;
}

static void ppm_hard_userlimit_update_limit_cb(void)
{
	unsigned int i;
	struct ppm_policy_req *req = &hard_userlimit_policy.req;

	FUNC_ENTER(FUNC_LV_POLICY);

	if (hard_userlimit_data.is_freq_limited_by_user
		|| hard_userlimit_data.is_core_limited_by_user) {
		ppm_clear_policy_limit(&hard_userlimit_policy);

		for (i = 0; i < req->cluster_num; i++) {
			req->limit[i].min_cpufreq_idx =
				(hard_userlimit_data.limit[i].min_freq_idx
				 == -1)
				? req->limit[i].min_cpufreq_idx
				: hard_userlimit_data.limit[i].min_freq_idx;
			req->limit[i].max_cpufreq_idx =
				(hard_userlimit_data.limit[i].max_freq_idx
				 == -1)
				? req->limit[i].max_cpufreq_idx
				: hard_userlimit_data.limit[i].max_freq_idx;
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

static void ppm_hard_userlimit_status_change_cb(bool enable)
{
	FUNC_ENTER(FUNC_LV_POLICY);

	ppm_ver("hard userlimit policy status changed to %d\n", enable);

	FUNC_EXIT(FUNC_LV_POLICY);
}

unsigned int mt_ppm_hard_userlimit_cpu_core(unsigned int cluster_num,
	struct ppm_limit_data *data)
{
	/* phase-out */

	return 0;
}

unsigned int mt_ppm_hard_userlimit_cpu_freq(
	unsigned int cluster_num, struct ppm_limit_data *data)
{
	int i = 0;
	int min_freq, max_freq, min_freq_idx, max_freq_idx;
	bool is_limit = false;

	/* Error check */
	if (cluster_num > NR_PPM_CLUSTERS) {
		ppm_err("Invalid cluster num = %d\n", cluster_num);
		return -1;
	}

	if (!data) {
		ppm_err("limit data is NULL!\n");
		return -1;
	}

	ppm_lock(&hard_userlimit_policy.lock);
	if (!hard_userlimit_policy.is_enabled) {
		ppm_warn("hard userlimit policy is not enabled!\n");
		ppm_unlock(&hard_userlimit_policy.lock);
		return -1;
	}

	/* update policy data */
	for (i = 0; i < cluster_num; i++) {
		min_freq = data[i].min;
		max_freq = data[i].max;

		/* check is all limit clear or not */
		if (min_freq != -1 || max_freq != -1)
			is_limit = true;

		/* freq to idx */
		min_freq_idx = (min_freq == -1) ? -1
				: ppm_main_freq_to_idx(i, min_freq,
				CPUFREQ_RELATION_L);
		max_freq_idx = (max_freq == -1) ? -1
				: ppm_main_freq_to_idx(i, max_freq,
				CPUFREQ_RELATION_H);

		/* sync to max_freq if min < max */
		if (min_freq_idx != -1 && max_freq_idx != -1
			&& min_freq_idx < max_freq_idx)
			min_freq_idx = max_freq_idx;

		/* write to policy data */
		if (min_freq_idx != hard_userlimit_data.limit[i].min_freq_idx ||
				max_freq_idx !=
				hard_userlimit_data.limit[i].max_freq_idx) {
			hard_userlimit_data.limit[i].min_freq_idx =
				min_freq_idx;
			hard_userlimit_data.limit[i].max_freq_idx =
				max_freq_idx;
			ppm_dbg(HARD_USER_LIMIT,
				"%d:hard user limit min/max freq = %d(%d)/%d(%d)\n",
				i, min_freq, min_freq_idx,
				max_freq, max_freq_idx);
		}
	}

	hard_userlimit_data.is_freq_limited_by_user = is_limit;
	hard_userlimit_policy.is_activated =
		ppm_hard_userlimit_is_policy_active();

	ppm_unlock(&hard_userlimit_policy.lock);
	mt_ppm_main();

	return 0;
}

unsigned int mt_ppm_hard_userlimit_freq_limit_by_others(unsigned int cluster)
{
	struct ppm_data *p = &ppm_main_info;

	if (cluster >= NR_PPM_CLUSTERS)
		return 0;
	else
		return p->cluster_info[cluster].max_freq_except_userlimit;
}

static int ppm_hard_userlimit_min_cpu_freq_proc_show
	(struct seq_file *m, void *v)
{
	int i;

	for (i = 0; i < hard_userlimit_policy.req.cluster_num; i++) {
		seq_printf(m, "%d: min_freq_idx = %d, max_freq_idx = %d\n",
			i, hard_userlimit_data.limit[i].min_freq_idx,
			hard_userlimit_data.limit[i].max_freq_idx);
	}

	return 0;
}

static ssize_t ppm_hard_userlimit_min_cpu_freq_proc_write(struct file *file,
	const char __user *buffer, size_t count, loff_t *pos)
{
	int id, min_freq, idx, i;
	bool is_limit = false;

	char *buf = ppm_copy_from_user_for_proc(buffer, count);

	if (!buf)
		return -EINVAL;

	if (sscanf(buf, "%d %d", &id, &min_freq) == 2) {
		if (id < 0 || id >= ppm_main_info.cluster_num) {
			ppm_err("Invalid cluster id: %d\n", id);
			goto out;
		}

		ppm_lock(&hard_userlimit_policy.lock);

		if (!hard_userlimit_policy.is_enabled) {
			ppm_warn("hard userlimit policy is not enabled!\n");
			ppm_unlock(&hard_userlimit_policy.lock);
			goto out;
		}

		idx = (min_freq == -1) ? -1
			: ppm_main_freq_to_idx(id, min_freq,
				CPUFREQ_RELATION_L);

		/* error check, sync to max idx if min freq > max freq */
		if (hard_userlimit_data.limit[id].max_freq_idx != -1
			&& idx != -1
			&& idx < hard_userlimit_data.limit[id].max_freq_idx)
			idx = hard_userlimit_data.limit[id].max_freq_idx;

		if (idx != hard_userlimit_data.limit[id].min_freq_idx) {
			hard_userlimit_data.limit[id].min_freq_idx = idx;
			ppm_dbg(HARD_USER_LIMIT,
				"%d:hard userlimit min_freq= %d KHz(%d)\n",
				id, min_freq, idx);
		}

		/* check is freq limited or not */
		for (i = 0; i < hard_userlimit_policy.req.cluster_num; i++) {
			if (hard_userlimit_data.limit[i].min_freq_idx != -1
			|| hard_userlimit_data.limit[i].max_freq_idx != -1) {
				is_limit = true;
				break;
			}
		}
		hard_userlimit_data.is_freq_limited_by_user = is_limit;

		hard_userlimit_policy.is_activated =
			ppm_hard_userlimit_is_policy_active();

		ppm_unlock(&hard_userlimit_policy.lock);
		mt_ppm_main();
	} else
		ppm_err("@%s: Invalid input!\n", __func__);

out:
	free_page((unsigned long)buf);
	return count;
}

static int ppm_hard_userlimit_max_cpu_freq_proc_show
(struct seq_file *m, void *v)
{
	int i;

	for (i = 0; i < hard_userlimit_policy.req.cluster_num; i++) {
		seq_printf(m, "%d: min_freq_idx = %d, max_freq_idx = %d\n",
			i, hard_userlimit_data.limit[i].min_freq_idx,
			hard_userlimit_data.limit[i].max_freq_idx);
	}

	return 0;
}

static ssize_t ppm_hard_userlimit_max_cpu_freq_proc_write(
	struct file *file, const char __user *buffer, size_t count, loff_t *pos)
{
	int id, max_freq, idx, i;
	bool is_limit = false;

	char *buf = ppm_copy_from_user_for_proc(buffer, count);

	if (!buf)
		return -EINVAL;

	if (sscanf(buf, "%d %d", &id, &max_freq) == 2) {
		if (id < 0 || id >= ppm_main_info.cluster_num) {
			ppm_err("Invalid cluster id: %d\n", id);
			goto out;
		}

		ppm_lock(&hard_userlimit_policy.lock);

		if (!hard_userlimit_policy.is_enabled) {
			ppm_warn("hard userlimit policy is not enabled!\n");
			ppm_unlock(&hard_userlimit_policy.lock);
			goto out;
		}

		idx = (max_freq == -1) ? -1
			: ppm_main_freq_to_idx(id, max_freq,
			CPUFREQ_RELATION_H);

		/* error check, sync to max idx if max freq < min freq */
		if (hard_userlimit_data.limit[id].min_freq_idx != -1
			&& idx != -1
			&& idx > hard_userlimit_data.limit[id].min_freq_idx)
			hard_userlimit_data.limit[id].min_freq_idx = idx;

		if (idx != hard_userlimit_data.limit[id].max_freq_idx) {
			hard_userlimit_data.limit[id].max_freq_idx = idx;
			ppm_dbg(HARD_USER_LIMIT,
				"%d:hard user limit max_freq = %d KHz(idx = %d)\n",
				id, max_freq, idx);
		}

		/* check is freq limited or not */
		for (i = 0; i < hard_userlimit_policy.req.cluster_num; i++) {
			if (hard_userlimit_data.limit[i].min_freq_idx != -1
			|| hard_userlimit_data.limit[i].max_freq_idx != -1) {
				is_limit = true;
				break;
			}
		}
		hard_userlimit_data.is_freq_limited_by_user = is_limit;

		hard_userlimit_policy.is_activated =
			ppm_hard_userlimit_is_policy_active();

		ppm_unlock(&hard_userlimit_policy.lock);
		mt_ppm_main();
	} else
		ppm_err("@%s: Invalid input!\n", __func__);

out:
	free_page((unsigned long)buf);
	return count;
}

static int ppm_hard_userlimit_cpu_freq_proc_show(struct seq_file *m, void *v)
{
	int i;

	for (i = 0; i < hard_userlimit_policy.req.cluster_num; i++) {
		seq_printf(m, "%d: min_freq_idx = %d, max_freq_idx = %d\n",
			i, hard_userlimit_data.limit[i].min_freq_idx,
			hard_userlimit_data.limit[i].max_freq_idx);
	}

	return 0;
}

static ssize_t ppm_hard_userlimit_cpu_freq_proc_write(struct file *file,
	const char __user *buffer,	size_t count, loff_t *pos)
{
	int i = 0, data;
	struct ppm_limit_data freq_limit[NR_PPM_CLUSTERS];
	unsigned int arg_num = NR_PPM_CLUSTERS * 2; /* for min and max */
	char *tok, *tmp;

	char *buf = ppm_copy_from_user_for_proc(buffer, count);

	if (!buf)
		return -EINVAL;

	tmp = buf;
	while ((tok = strsep(&tmp, " ")) != NULL) {
		if (i == arg_num) {
			ppm_err("number of arguments > %d!\n", arg_num);
			goto out;
		}

		if (kstrtoint(tok, 10, &data)) {
			ppm_err("Invalid input: %s\n", tok);
			goto out;
		} else {
			if (i % 2) /* max */
				freq_limit[i/2].max = data;
			else /* min */
				freq_limit[i/2].min = data;

			i++;
		}
	}

	if (i < arg_num)
		ppm_err("number of arguments < %d!\n", arg_num);
	else
		mt_ppm_hard_userlimit_cpu_freq(NR_PPM_CLUSTERS, freq_limit);

out:
	free_page((unsigned long)buf);
	return count;
}

static int ppm_hard_userlimit_freq_limit_by_others_proc_show(
	struct seq_file *m, void *v)
{
	int i;

	for_each_ppm_clusters(i) {
		seq_printf(m, "cluster %d max_freq_limit = %d\n",
			i, mt_ppm_hard_userlimit_freq_limit_by_others(i));
	}

	return 0;
}

PROC_FOPS_RW(hard_userlimit_min_cpu_freq);
PROC_FOPS_RW(hard_userlimit_max_cpu_freq);
PROC_FOPS_RW(hard_userlimit_cpu_freq);
PROC_FOPS_RO(hard_userlimit_freq_limit_by_others);

static int __init ppm_hard_userlimit_policy_init(void)
{
	int i, ret = 0;

	struct pentry {
		const char *name;
		const struct file_operations *fops;
	};

	const struct pentry entries[] = {
		PROC_ENTRY(hard_userlimit_min_cpu_freq),
		PROC_ENTRY(hard_userlimit_max_cpu_freq),
		PROC_ENTRY(hard_userlimit_cpu_freq),
		PROC_ENTRY(hard_userlimit_freq_limit_by_others),
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

	hard_userlimit_data.limit = kcalloc(ppm_main_info.cluster_num,
		sizeof(*hard_userlimit_data.limit), GFP_KERNEL);
	if (!hard_userlimit_data.limit) {
		ret = -ENOMEM;
		goto out;
	}

	/* init hard_userlimit_data */
	for_each_ppm_clusters(i) {
		hard_userlimit_data.limit[i].min_freq_idx = -1;
		hard_userlimit_data.limit[i].max_freq_idx = -1;
		hard_userlimit_data.limit[i].min_core_num = -1;
		hard_userlimit_data.limit[i].max_core_num = -1;
	}

	if (ppm_main_register_policy(&hard_userlimit_policy)) {
		ppm_err("@%s:hard userlimit policy register failed\n",
		 __func__);
		kfree(hard_userlimit_data.limit);
		ret = -EINVAL;
		goto out;
	}

	ppm_info("@%s: register %s done!\n",
		__func__, hard_userlimit_policy.name);

out:
	FUNC_EXIT(FUNC_LV_POLICY);

	return ret;
}

static void __exit ppm_hard_userlimit_policy_exit(void)
{
	FUNC_ENTER(FUNC_LV_POLICY);

	kfree(hard_userlimit_data.limit);

	ppm_main_unregister_policy(&hard_userlimit_policy);

	FUNC_EXIT(FUNC_LV_POLICY);
}

module_init(ppm_hard_userlimit_policy_init);
module_exit(ppm_hard_userlimit_policy_exit);
