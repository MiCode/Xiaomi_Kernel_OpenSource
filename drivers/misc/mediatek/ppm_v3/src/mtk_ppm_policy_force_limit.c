// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>

#include "mtk_ppm_internal.h"


static void ppm_forcelimit_update_limit_cb(void);
static void ppm_forcelimit_status_change_cb(bool enable);

/* other members will init by ppm_main */
static struct ppm_policy_data forcelimit_policy = {
	.name			= __stringify(PPM_POLICY_FORCE_LIMIT),
	.lock			= __MUTEX_INITIALIZER(forcelimit_policy.lock),
	.policy			= PPM_POLICY_FORCE_LIMIT,
	.priority		= PPM_POLICY_PRIO_HIGHEST,
	.update_limit_cb	= ppm_forcelimit_update_limit_cb,
	.status_change_cb	= ppm_forcelimit_status_change_cb,
};

struct ppm_userlimit_data forcelimit_data = {
	.is_freq_limited_by_user = false,
	.is_core_limited_by_user = false,
};


/* MUST in lock */
static bool ppm_forcelimit_is_policy_active(void)
{
	if (!forcelimit_data.is_core_limited_by_user)
		return false;
	else
		return true;
}

static void ppm_forcelimit_update_limit_cb(void)
{
	unsigned int i;
	struct ppm_policy_req *req = &forcelimit_policy.req;

	FUNC_ENTER(FUNC_LV_POLICY);

	if (forcelimit_data.is_core_limited_by_user) {
		ppm_clear_policy_limit(&forcelimit_policy);

		for (i = 0; i < req->cluster_num; i++) {
			req->limit[i].min_cpu_core =
				(forcelimit_data.limit[i].min_core_num == -1)
				? req->limit[i].min_cpu_core
				: forcelimit_data.limit[i].min_core_num;
			req->limit[i].max_cpu_core =
				(forcelimit_data.limit[i].max_core_num == -1)
				? req->limit[i].max_cpu_core
				: forcelimit_data.limit[i].max_core_num;
		}
	}

	FUNC_EXIT(FUNC_LV_POLICY);
}

static void ppm_forcelimit_status_change_cb(bool enable)
{
	FUNC_ENTER(FUNC_LV_POLICY);

	ppm_ver("forcelimit policy status changed to %d\n", enable);

	FUNC_EXIT(FUNC_LV_POLICY);
}

unsigned int mt_ppm_forcelimit_cpu_core(unsigned int cluster_num,
	struct ppm_limit_data *data)
{
	int i = 0;
	int min_core, max_core;
	bool is_limit = false;

	/* Error check */
	if (cluster_num > NR_PPM_CLUSTERS) {
		ppm_err("Invalid cluster num = %d\n", cluster_num);
		return -1;
	}

	if (!data) {
		ppm_err("@%s: limit data is NULL!\n", __func__);
		return -1;
	}

	for (i = 0; i < cluster_num; i++) {
		min_core = data[i].min;
		max_core = data[i].max;

		/* invalid input check */
		if (min_core != -1
			&& min_core < (int)get_cluster_min_cpu_core(i)) {
			ppm_err("Invalid input! min_core for cluster%d = %d\n",
				i, min_core);
			return -1;
		}
		if (max_core != -1
			&& max_core > (int)get_cluster_max_cpu_core(i)) {
			ppm_err("Invalid input! max_core for cluster%d = %d\n",
				i, max_core);
			return -1;
		}

		/* check is all limit clear or not */
		if (min_core != -1 || max_core != -1)
			is_limit = true;

		/* sync to max_core if min > max */
		if (min_core != -1 && max_core != -1 && min_core > max_core)
			data[i].min = data[i].max;
	}

	ppm_lock(&forcelimit_policy.lock);
	if (!forcelimit_policy.is_enabled) {
		ppm_warn("forcelimit policy is not enabled!\n");
		ppm_unlock(&forcelimit_policy.lock);
		return -1;
	}

	/* update policy data */
	for (i = 0; i < cluster_num; i++) {
		min_core = data[i].min;
		max_core = data[i].max;

		if (min_core != forcelimit_data.limit[i].min_core_num ||
			max_core != forcelimit_data.limit[i].max_core_num) {
			forcelimit_data.limit[i].min_core_num = min_core;
			forcelimit_data.limit[i].max_core_num = max_core;
			ppm_info("forcelimit min/max core for cl%d=%d/%d\n",
				i, min_core, max_core);
		}
	}

	forcelimit_data.is_core_limited_by_user = is_limit;
	forcelimit_policy.is_activated = ppm_forcelimit_is_policy_active();

	ppm_unlock(&forcelimit_policy.lock);
	mt_ppm_main();

	return 0;
}

static int ppm_forcelimit_cpu_core_proc_show(
	struct seq_file *m, void *v)
{
	int i;

	for (i = 0; i < forcelimit_policy.req.cluster_num; i++) {
		seq_printf(m, "cl%d: min_core_num = %d, max_core_num = %d\n",
			i, forcelimit_data.limit[i].min_core_num,
			forcelimit_data.limit[i].max_core_num);
	}

	return 0;
}

static ssize_t ppm_forcelimit_cpu_core_proc_write(struct file *file,
	const char __user *buffer, size_t count, loff_t *pos)
{
	int i = 0, data;
	struct ppm_limit_data core_limit[NR_PPM_CLUSTERS];
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
			ppm_err("@%s: Invalid input: %s\n", __func__, tok);
			goto out;
		} else {
			if (i % 2) /* max */
				core_limit[i/2].max = data;
			else /* min */
				core_limit[i/2].min = data;

			i++;
		}
	}

	if (i < arg_num)
		ppm_err("number of arguments < %d!\n", arg_num);
	else
		mt_ppm_forcelimit_cpu_core(NR_PPM_CLUSTERS, core_limit);

out:
	free_page((unsigned long)buf);
	return count;
}

PROC_FOPS_RW(forcelimit_cpu_core);

static int __init ppm_forcelimit_policy_init(void)
{
	int i, ret = 0;

	struct pentry {
		const char *name;
		const struct file_operations *fops;
	};

	const struct pentry entries[] = {
		PROC_ENTRY(forcelimit_cpu_core),
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

	forcelimit_data.limit = kcalloc(ppm_main_info.cluster_num,
		sizeof(*forcelimit_data.limit), GFP_KERNEL);
	if (!forcelimit_data.limit) {
		ret = -ENOMEM;
		goto out;
	}

	/* init forcelimit_data */
	for_each_ppm_clusters(i) {
		forcelimit_data.limit[i].min_freq_idx = -1;
		forcelimit_data.limit[i].max_freq_idx = -1;
		forcelimit_data.limit[i].min_core_num = -1;
		forcelimit_data.limit[i].max_core_num = -1;
	}

	if (ppm_main_register_policy(&forcelimit_policy)) {
		ppm_err("@%s: forcelimit policy register failed\n", __func__);
		kfree(forcelimit_data.limit);
		ret = -EINVAL;
		goto out;
	}

	ppm_info("@%s: register %s done!\n", __func__, forcelimit_policy.name);

out:
	FUNC_EXIT(FUNC_LV_POLICY);

	return ret;
}

static void __exit ppm_forcelimit_policy_exit(void)
{
	FUNC_ENTER(FUNC_LV_POLICY);

	kfree(forcelimit_data.limit);

	ppm_main_unregister_policy(&forcelimit_policy);

	FUNC_EXIT(FUNC_LV_POLICY);
}

module_init(ppm_forcelimit_policy_init);
module_exit(ppm_forcelimit_policy_exit);

