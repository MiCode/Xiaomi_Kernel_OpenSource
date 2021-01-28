// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/uaccess.h>

#include "mach/mtk_pbm.h"
#include "mtk_ppm_internal.h"
#include "mtk_ppm_platform.h"


static unsigned int ppm_dlpt_pwr_budget_preprocess(
	unsigned int budget);
static unsigned int ppm_dlpt_pwr_budget_postprocess(
	unsigned int budget, unsigned int pwr_idx);
static void ppm_dlpt_update_limit_cb(void);
static void ppm_dlpt_status_change_cb(bool enable);

static unsigned int dlpt_percentage_to_real_power;

/* other members will init by ppm_main */
static struct ppm_policy_data dlpt_policy = {
	.name			= __stringify(PPM_POLICY_DLPT),
	.lock			= __MUTEX_INITIALIZER(dlpt_policy.lock),
	.policy			= PPM_POLICY_DLPT,
	.priority		= PPM_POLICY_PRIO_POWER_BUDGET_BASE,
	.update_limit_cb	= ppm_dlpt_update_limit_cb,
	.status_change_cb	= ppm_dlpt_status_change_cb,
};

void __attribute__((weak)) kicker_pbm_by_cpu(
	unsigned int loading, int core, int voltage)
{
}

void mt_ppm_dlpt_kick_PBM(struct ppm_cluster_status *cluster_status,
	unsigned int cluster_num)
{
	int power_idx;
	unsigned int total_core = 0;
	unsigned int max_volt = 0;
	unsigned int budget = 0;
	int i;

	FUNC_ENTER(FUNC_LV_POLICY);

	/* find power bgt in table, skip this round if idx not found */
	power_idx = ppm_find_pwr_idx(cluster_status);
	if (power_idx < 0)
		goto end;

	for (i = 0; i < cluster_num; i++) {
		total_core += cluster_status[i].core_num;
		max_volt = MAX(max_volt, cluster_status[i].volt);
	}

	budget = ppm_calc_total_power(cluster_status, cluster_num,
		DYNAMIC_TABLE2REAL_PERCENTAGE);
	if (!budget)
		goto end;
	budget = ppm_dlpt_pwr_budget_postprocess(
		budget, (unsigned int)power_idx);

	ppm_dbg(DLPT, "budget = %d(%d), total_core = %d, max_volt = %d\n",
		budget, power_idx, total_core, max_volt);

#ifndef DISABLE_PBM_FEATURE
#ifdef CONFIG_MTK_RAM_CONSOLE
	aee_rr_rec_ppm_waiting_for_pbm(1);
	kicker_pbm_by_cpu(budget, total_core, max_volt);
	aee_rr_rec_ppm_waiting_for_pbm(0);
#else
	kicker_pbm_by_cpu(budget, total_core, max_volt);
#endif
#endif

end:
	FUNC_EXIT(FUNC_LV_POLICY);
}

void mt_ppm_dlpt_set_limit_by_pbm(unsigned int limited_power)
{
	unsigned int budget;

	FUNC_ENTER(FUNC_LV_POLICY);

	budget = ppm_dlpt_pwr_budget_preprocess(limited_power);

	ppm_dbg(DLPT, "Get PBM notifier => budget = %d(%d)\n",
		budget, limited_power);

	ppm_lock(&dlpt_policy.lock);

	if (!dlpt_policy.is_enabled) {
		ppm_warn("@%s: dlpt policy is not enabled!\n", __func__);
		ppm_unlock(&dlpt_policy.lock);
		goto end;
	}

	dlpt_policy.req.power_budget = budget;
	dlpt_policy.is_activated = (budget) ? true : false;

	ppm_unlock(&dlpt_policy.lock);

	mt_ppm_main();

end:
	FUNC_EXIT(FUNC_LV_POLICY);
}

static unsigned int ppm_dlpt_pwr_budget_preprocess(unsigned int budget)
{
	unsigned int percentage = dlpt_percentage_to_real_power;

	if (!percentage)
		percentage = 100;

	return (budget * percentage + (100 - 1)) / 100;
}

static unsigned int ppm_dlpt_pwr_budget_postprocess(unsigned int budget,
	unsigned int pwr_idx)
{
	/* just calculate new ratio */
	dlpt_percentage_to_real_power =
		(pwr_idx * 100 + (budget - 1)) / budget;
	ppm_dbg(DLPT, "new dlpt ratio = %d (%d/%d)\n",
		dlpt_percentage_to_real_power, pwr_idx, budget);

	return budget;
}

static void ppm_dlpt_update_limit_cb(void)
{
	FUNC_ENTER(FUNC_LV_POLICY);

	ppm_clear_policy_limit(&dlpt_policy);

	/* update limit according to power budget */
	ppm_update_req_by_pwr(&dlpt_policy.req);

	FUNC_EXIT(FUNC_LV_POLICY);
}

static void ppm_dlpt_status_change_cb(bool enable)
{
	FUNC_ENTER(FUNC_LV_POLICY);

	ppm_ver("@%s: dlpt policy status changed to %d\n", __func__, enable);

	FUNC_EXIT(FUNC_LV_POLICY);
}

static int ppm_dlpt_limit_proc_show(struct seq_file *m, void *v)
{
	seq_printf(m, "limited power = %d\n", dlpt_policy.req.power_budget);
	seq_printf(m, "PPM DLPT activate = %d\n", dlpt_policy.is_activated);

	return 0;
}

static ssize_t ppm_dlpt_limit_proc_write(struct file *file,
	const char __user *buffer, size_t count, loff_t *pos)
{
	unsigned int limited_power = 0;

	char *buf = ppm_copy_from_user_for_proc(buffer, count);

	if (!buf)
		return -EINVAL;

	if (!kstrtouint(buf, 10, &limited_power))
		mt_ppm_dlpt_set_limit_by_pbm(limited_power);
	else
		ppm_err("@%s: Invalid input!\n", __func__);

	free_page((unsigned long)buf);
	return count;
}

static int ppm_dlpt_budget_trans_percentage_proc_show(
	struct seq_file *m, void *v)
{
	seq_printf(m, "trans_percentage = %d\n",
		dlpt_percentage_to_real_power);
	seq_printf(m, "PPM DLPT activate = %d\n",
		dlpt_policy.is_activated);

	return 0;
}

static ssize_t ppm_dlpt_budget_trans_percentage_proc_write(
	struct file *file, const char __user *buffer,
	size_t count, loff_t *pos)
{
	unsigned int percentage = 0;

	char *buf = ppm_copy_from_user_for_proc(buffer, count);

	if (!buf)
		return -EINVAL;

	if (!kstrtouint(buf, 10, &percentage)) {
		if (!percentage)
			ppm_err("percentage should not be 0!\n");
		else
			dlpt_percentage_to_real_power = percentage;
	} else
		ppm_err("@%s: Invalid input!\n", __func__);

	free_page((unsigned long)buf);
	return count;
}

PROC_FOPS_RW(dlpt_limit);
PROC_FOPS_RW(dlpt_budget_trans_percentage);

static int __init ppm_dlpt_policy_init(void)
{
	int i, ret = 0;

	struct pentry {
		const char *name;
		const struct file_operations *fops;
	};

	const struct pentry entries[] = {
		PROC_ENTRY(dlpt_limit),
		PROC_ENTRY(dlpt_budget_trans_percentage),
	};

	FUNC_ENTER(FUNC_LV_POLICY);

#ifdef DISABLE_DLPT_FEATURE
	goto out;
#endif

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

	if (ppm_main_register_policy(&dlpt_policy)) {
		ppm_err("@%s: dlpt policy register failed\n", __func__);
		ret = -EINVAL;
		goto out;
	}

	ppm_info("@%s: register %s done!\n", __func__, dlpt_policy.name);

out:
	FUNC_EXIT(FUNC_LV_POLICY);

	return ret;
}

static void __exit ppm_dlpt_policy_exit(void)
{
	FUNC_ENTER(FUNC_LV_POLICY);

	ppm_main_unregister_policy(&dlpt_policy);

	FUNC_EXIT(FUNC_LV_POLICY);
}

module_init(ppm_dlpt_policy_init);
module_exit(ppm_dlpt_policy_exit);

