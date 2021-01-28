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

#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/uaccess.h>

#include "mtk_ppm_internal.h"


#define PPM_MODE_NAME_LEN	16

/* procfs dir for policies */
struct proc_dir_entry *policy_dir;
struct proc_dir_entry *profile_dir;
struct proc_dir_entry *cpi_dir;
unsigned int ppm_debug;
unsigned int ppm_func_lv_mask;


char *ppm_copy_from_user_for_proc(const char __user *buffer, size_t count)
{
	char *buf = (char *)__get_free_page(GFP_USER);

	if (!buf)
		return NULL;

	if (count >= PAGE_SIZE)
		goto out;

	if (copy_from_user(buf, buffer, count))
		goto out;

	buf[count] = '\0';

	return buf;

out:
	free_page((unsigned long)buf);

	return NULL;
}

static int ppm_func_debug_proc_show(struct seq_file *m, void *v)
{
	seq_printf(m, "ppm func lv debug mask = 0x%x\n", ppm_func_lv_mask);

	return 0;
}

static ssize_t ppm_func_debug_proc_write(struct file *file,
	const char __user *buffer, size_t count, loff_t *pos)
{
	unsigned int func_dbg_lv = 0;

	char *buf = ppm_copy_from_user_for_proc(buffer, count);

	if (!buf)
		return -EINVAL;

	if (!kstrtouint(buf, 10, &func_dbg_lv))
		ppm_func_lv_mask = func_dbg_lv;
	else
		ppm_err("echo func_dbg_lv (dec) > /proc/ppm/func_debug\n");

	free_page((unsigned long)buf);
	return count;
}

static int ppm_debug_proc_show(struct seq_file *m, void *v)
{
	seq_printf(m, "ppm debug (log level) = %d\n", ppm_debug);

	return 0;
}

static ssize_t ppm_debug_proc_write(struct file *file,
	const char __user *buffer, size_t count, loff_t *pos)
{
	unsigned int dbg_lv = 0;

	char *buf = ppm_copy_from_user_for_proc(buffer, count);

	if (!buf)
		return -EINVAL;

	if (!kstrtouint(buf, 10, &dbg_lv))
		ppm_debug = dbg_lv;
	else
		ppm_err("echo dbg_lv (dec) > /proc/ppm/debug\n");

	free_page((unsigned long)buf);
	return count;
}

static int ppm_enabled_proc_show(struct seq_file *m, void *v)
{
	if (ppm_main_info.is_enabled == true)
		seq_puts(m, "ppm is enabled\n");
	else
		seq_puts(m, "ppm is disabled\n");

	return 0;
}

static ssize_t ppm_enabled_proc_write(struct file *file,
	const char __user *buffer, size_t count, loff_t *pos)
{
	unsigned int enabled = 0;

	char *buf = ppm_copy_from_user_for_proc(buffer, count);

	if (!buf)
		return -EINVAL;

	ppm_lock(&ppm_main_info.lock);

	if (!kstrtouint(buf, 10, &enabled)) {
		ppm_main_info.is_enabled = (enabled) ? true : false;
		if (!ppm_main_info.is_enabled) {
			int i;
			struct ppm_client_req *c_req =
				&(ppm_main_info.client_req);
			struct ppm_client_req *last_req =
				&(ppm_main_info.last_req);

			/* send default limit to client */
			ppm_main_clear_client_req(c_req);
#ifdef PPM_SSPM_SUPPORT
			/* update limit to SSPM first */
			ppm_ipi_update_limit(*c_req);
#endif
			for_each_ppm_clients(i) {
				if (!ppm_main_info.client_info[i].limit_cb)
					continue;

				ppm_main_info.client_info[i].limit_cb(*c_req);
			}
			memcpy(last_req->cpu_limit, c_req->cpu_limit,
				ppm_main_info.cluster_num *
				sizeof(*c_req->cpu_limit));

			ppm_info("PPM disabled, send no limit to clinet!\n");
		}
	} else
		ppm_err("echo [0/1] > /proc/ppm/enabled\n");

	ppm_unlock(&ppm_main_info.lock);

	free_page((unsigned long)buf);
	return count;
}

static int ppm_exclusive_core_proc_show(struct seq_file *m, void *v)
{
	seq_printf(m, "ppm exclusive core = %*pbl\n",
		cpumask_pr_args(ppm_main_info.exclusive_core));

	return 0;
}

static ssize_t ppm_exclusive_core_proc_write(struct file *file,
	const char __user *buffer, size_t count, loff_t *pos)
{
	unsigned int mask = 0;
	int cpu;

	char *buf = ppm_copy_from_user_for_proc(buffer, count);

	if (!buf)
		return -EINVAL;

	if (!kstrtouint(buf, 10, &mask)) {
		ppm_lock(&ppm_main_info.lock);
		cpumask_clear(ppm_main_info.exclusive_core);
		for_each_present_cpu(cpu) {
			if (mask & 0x1)
				cpumask_set_cpu(cpu,
					ppm_main_info.exclusive_core);
			mask >>= 1;
		}
		ppm_unlock(&ppm_main_info.lock);
		ppm_info("update exclusive core = %*pbl\n",
			cpumask_pr_args(ppm_main_info.exclusive_core));
		mt_ppm_main();
	} else
		ppm_err("echo <bitmask> > /proc/ppm/exclusive_core\n");

	free_page((unsigned long)buf);
	return count;
}

static int ppm_dump_power_table_proc_show(struct seq_file *m, void *v)
{
	ppm_cobra_dump_tbl(m);

	return 0;
}

static int ppm_dump_policy_list_proc_show(struct seq_file *m, void *v)
{
	struct ppm_policy_data *pos;
	unsigned int i = 0, j = 0;

	ppm_lock(&ppm_main_info.lock);
	seq_puts(m, "\nFinal limit:\n");
	for_each_ppm_clusters(j) {
		seq_printf(m, "cluster %d: (%d)(%d)(%d)(%d)\n", j,
			ppm_main_info.last_req.cpu_limit[j].min_cpufreq_idx,
			ppm_main_info.last_req.cpu_limit[j].max_cpufreq_idx,
			ppm_main_info.last_req.cpu_limit[j].min_cpu_core,
			ppm_main_info.last_req.cpu_limit[j].max_cpu_core);
	}

	seq_puts(m, "\nPolicy limit:\n");
	list_for_each_entry(pos, &ppm_main_info.policy_list, link) {
		ppm_lock(&pos->lock);

		seq_printf(m, "[%d] %s (priority: %d)\n",
			i, pos->name, pos->priority);
		seq_printf(m, "is_enabled = %d, is_activated = %d\n",
			pos->is_enabled, pos->is_activated);
		seq_printf(m, "req_perf_idx = %d, req_power_budget = %d\n",
			pos->req.perf_idx, pos->req.power_budget);
		for_each_ppm_clusters(j) {
			seq_printf(m, "cluster %d: (%d)(%d)(%d)(%d)\n", j,
				pos->req.limit[j].min_cpufreq_idx,
				pos->req.limit[j].max_cpufreq_idx,
				pos->req.limit[j].min_cpu_core,
				pos->req.limit[j].max_cpu_core);
		}
		seq_puts(m, "\n");
		ppm_unlock(&pos->lock);

		i++;
	}
	ppm_unlock(&ppm_main_info.lock);

	return 0;
}

static int ppm_policy_status_proc_show(struct seq_file *m, void *v)
{
	struct ppm_policy_data *pos;

	list_for_each_entry_reverse(pos, &ppm_main_info.policy_list, link)
		seq_printf(m, "[%d] %s: %s\n", pos->policy, pos->name,
				(pos->is_enabled) ? "enabled" : "disabled");

	seq_puts(m, "\nUsage: echo <idx> <1/0> > /proc/ppm/policy_status\n\n");

	return 0;
}

static ssize_t ppm_policy_status_proc_write(struct file *file,
	const char __user *buffer, size_t count, loff_t *pos)
{
	struct ppm_policy_data *l_pos;
	unsigned int policy_idx, enabled;

	char *buf = ppm_copy_from_user_for_proc(buffer, count);

	if (!buf)
		return -EINVAL;

	if (sscanf(buf, "%d %d", &policy_idx, &enabled) == 2) {
		if (enabled > 1)
			enabled = 1;

		/* set mode status and notify policy via status change cb */
		list_for_each_entry(l_pos,
			&ppm_main_info.policy_list, link) {
			if (l_pos->policy == policy_idx
				&& l_pos->is_enabled != enabled) {
				ppm_lock(&l_pos->lock);
				l_pos->is_enabled =
					(enabled) ? true : false;
				if (!l_pos->is_enabled)
					l_pos->is_activated = false;
				if (l_pos->status_change_cb)
					l_pos->status_change_cb(
						l_pos->is_enabled);
				ppm_unlock(&l_pos->lock);

				mt_ppm_main();
				break;
			}
		}
	} else
		ppm_err("Usage: echo <idx> <1/0> > /proc/ppm/policy_status\n");

	free_page((unsigned long)buf);
	return count;
}

static int ppm_dump_dvfs_table_proc_show(struct seq_file *m, void *v)
{
	struct ppm_cluster_info *info = (struct ppm_cluster_info *)m->private;
	unsigned int i;

	if (!info->dvfs_tbl) {
		ppm_err("DVFS table for cluster %d is NULL!\n",
			info->cluster_id);
		goto end;
	}

	for (i = 0; i < info->dvfs_opp_num; i++)
		seq_printf(m, "%d ", info->dvfs_tbl[i].frequency);

	seq_puts(m, "\n");

end:
	return 0;
}

static int ppm_cobra_budget_to_limit_proc_show(struct seq_file *m, void *v)
{
	ppm_cobra_lookup_get_result(m, LOOKUP_BY_BUDGET);

	return 0;
}

static ssize_t ppm_cobra_budget_to_limit_proc_write(struct file *file,
	const char __user *buffer, size_t count, loff_t *pos)
{
	unsigned int budget;

	char *buf = ppm_copy_from_user_for_proc(buffer, count);

	if (!buf)
		return -EINVAL;

	if (!kstrtoint(buf, 10, &budget))
		cobra_lookup_data.budget = budget;
	else
		ppm_err("echo <budget> > /proc/ppm/cobra_budget_to_limit\n");

	free_page((unsigned long)buf);
	return count;
}

static int ppm_cobra_limit_to_budget_proc_show(struct seq_file *m, void *v)
{
	ppm_cobra_lookup_get_result(m, LOOKUP_BY_LIMIT);

	return 0;
}

static ssize_t ppm_cobra_limit_to_budget_proc_write(struct file *file,
	const char __user *buffer, size_t count, loff_t *pos)
{
	char *tok, *tmp;
	unsigned int i = 0, data;

	char *buf = ppm_copy_from_user_for_proc(buffer, count);

	if (!buf)
		return -EINVAL;

	tmp = buf;
	while ((tok = strsep(&tmp, " ")) != NULL) {
		if (i == NR_PPM_CLUSTERS * 2) {
			ppm_err("@%s: number of arguments > %d!\n",
				__func__, NR_PPM_CLUSTERS * 2);
			goto out;
		}

		if (kstrtoint(tok, 10, &data)) {
			ppm_err("@%s: Invalid input: %s\n", __func__, tok);
			goto out;
		} else {
			if (i % 2) /* OPP */
				cobra_lookup_data.limit[i/2].opp = data;
			else /* core */
				cobra_lookup_data.limit[i/2].core = data;

			i++;
		}
	}

out:
	free_page((unsigned long)buf);
	return count;

}
PROC_FOPS_RW(func_debug);
PROC_FOPS_RW(debug);
PROC_FOPS_RW(enabled);
PROC_FOPS_RW(exclusive_core);
PROC_FOPS_RO(dump_power_table);
PROC_FOPS_RO(dump_policy_list);
PROC_FOPS_RW(policy_status);
PROC_FOPS_RO(dump_dvfs_table);
PROC_FOPS_RW(cobra_budget_to_limit);
PROC_FOPS_RW(cobra_limit_to_budget);

int ppm_procfs_init(void)
{
	struct proc_dir_entry *dir = NULL;
	int i;
	char str[32];

	struct pentry {
		const char *name;
		const struct file_operations *fops;
	};

	const struct pentry entries[] = {
		PROC_ENTRY(func_debug),
		PROC_ENTRY(debug),
		PROC_ENTRY(enabled),
		PROC_ENTRY(exclusive_core),
		PROC_ENTRY(dump_power_table),
		PROC_ENTRY(dump_policy_list),
		PROC_ENTRY(policy_status),
		PROC_ENTRY(cobra_budget_to_limit),
		PROC_ENTRY(cobra_limit_to_budget),
	};

	dir = proc_mkdir("ppm", NULL);
	if (!dir) {
		ppm_err("@%s: fail to create /proc/ppm dir\n", __func__);
		return -ENOMEM;
	}

	/* mkdir for policies */
	policy_dir = proc_mkdir("policy", dir);
	if (!policy_dir) {
		ppm_err("fail to create /proc/ppm/policy dir\n");
		return -ENOMEM;
	}

	profile_dir = proc_mkdir("profile", dir);
	if (!profile_dir) {
		ppm_err("fail to create /proc/ppm/profile dir\n");
		return -ENOMEM;
	}

	cpi_dir = proc_mkdir("cpi", dir);
	if (!cpi_dir) {
		ppm_err("fail to create /proc/ppm/cpi dir\n");
		return -ENOMEM;
	}

	for (i = 0; i < ARRAY_SIZE(entries); i++) {
		if (!proc_create(entries[i].name, 0664,
			dir, entries[i].fops))
			ppm_err("%s(), create /proc/ppm/%s failed\n",
				__func__, entries[i].name);
	}

	for_each_ppm_clusters(i) {
		sprintf(str, "dump_cluster_%d_dvfs_table", i);

		if (!proc_create_data(str, 0644,
			dir, &ppm_dump_dvfs_table_proc_fops,
			&ppm_main_info.cluster_info[i]))
			ppm_err("%s(), create /proc/ppm/%s failed\n",
				__func__, str);
	}

	return 0;
}

