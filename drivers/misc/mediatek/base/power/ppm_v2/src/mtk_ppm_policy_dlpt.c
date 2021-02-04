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
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/uaccess.h>

#include "mtk_ppm_internal.h"
#include "mtk_ppm_platform.h"
#include "mach/mtk_pbm.h"


enum PPM_DLPT_MODE {
	SW_MODE,	/* use SW DLPT only */
	HYBRID_MODE,	/* use SW DLPT + HW OCP */
	HW_MODE,	/* use HW OCP only */
};

static unsigned int ppm_dlpt_pwr_budget_preprocess(unsigned int budget);
#if PPM_DLPT_ENHANCEMENT
static unsigned int ppm_dlpt_pwr_budget_postprocess(unsigned int budget, unsigned int pwr_idx);
#else
static unsigned int ppm_dlpt_calc_trans_precentage(void);
static unsigned int ppm_dlpt_pwr_budget_postprocess(unsigned int budget);
#endif
static void ppm_dlpt_update_limit_cb(enum ppm_power_state new_state);
static void ppm_dlpt_status_change_cb(bool enable);
static void ppm_dlpt_mode_change_cb(enum ppm_mode mode);

static unsigned int dlpt_percentage_to_real_power;
static enum PPM_DLPT_MODE dlpt_mode;

/* other members will init by ppm_main */
static struct ppm_policy_data dlpt_policy = {
	.name			= __stringify(PPM_POLICY_DLPT),
	.lock			= __MUTEX_INITIALIZER(dlpt_policy.lock),
	.policy			= PPM_POLICY_DLPT,
	.priority		= PPM_POLICY_PRIO_POWER_BUDGET_BASE,
	.get_power_state_cb	= NULL,	/* decide in ppm main via min power budget */
	.update_limit_cb	= ppm_dlpt_update_limit_cb,
	.status_change_cb	= ppm_dlpt_status_change_cb,
	.mode_change_cb		= ppm_dlpt_mode_change_cb,

};

void __attribute__((weak)) kicker_pbm_by_cpu(unsigned int loading, int core, int voltage) { }

void mt_ppm_dlpt_kick_PBM(struct ppm_cluster_status *cluster_status, unsigned int cluster_num)
{
	int power_idx;
	unsigned int total_core = 0;
	unsigned int max_volt = 0;
	unsigned int budget = 0;
	int i;

	FUNC_ENTER(FUNC_LV_POLICY);

	/* find power budget in table, skip this round if idx not found in table */
	power_idx = ppm_find_pwr_idx(cluster_status);
	if (power_idx < 0)
		goto end;

	for (i = 0; i < cluster_num; i++) {
		total_core += cluster_status[i].core_num;
		max_volt = MAX(max_volt, cluster_status[i].volt);
	}
#if PPM_DLPT_ENHANCEMENT

#if PPM_HW_OCP_SUPPORT
	budget = ppm_calc_total_power_by_ocp(cluster_status, cluster_num);
	if (!budget)
		budget = ppm_calc_total_power(cluster_status, cluster_num, DYNAMIC_TABLE2REAL_PERCENTAGE);
#else
	budget = ppm_calc_total_power(cluster_status, cluster_num, DYNAMIC_TABLE2REAL_PERCENTAGE);
#endif
	if (!budget)
		goto end;

	budget = ppm_dlpt_pwr_budget_postprocess(budget, (unsigned int)power_idx);
#else
	budget = ppm_dlpt_pwr_budget_postprocess((unsigned int)power_idx);
#endif

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

	ppm_dbg(DLPT, "Get PBM notifier => budget = %d(%d)\n", budget, limited_power);

	ppm_lock(&dlpt_policy.lock);

	if (!dlpt_policy.is_enabled) {
		ppm_warn("@%s: dlpt policy is not enabled!\n", __func__);
		ppm_unlock(&dlpt_policy.lock);
		goto end;
	}

	switch (dlpt_mode) {
	case SW_MODE:
	case HYBRID_MODE:
#if PPM_HW_OCP_SUPPORT
		dlpt_policy.req.power_budget = (dlpt_mode == SW_MODE)
			? budget : ppm_set_ocp(budget, dlpt_percentage_to_real_power);
#else
		dlpt_policy.req.power_budget = budget;
#endif
		dlpt_policy.is_activated = (budget) ? true : false;
		ppm_unlock(&dlpt_policy.lock);
		mt_ppm_main();

		break;
	case HW_MODE:	/* TBD */
	default:
		break;
	}

end:
	FUNC_EXIT(FUNC_LV_POLICY);
}

#if PPM_DLPT_ENHANCEMENT
static unsigned int ppm_dlpt_pwr_budget_preprocess(unsigned int budget)
{
	unsigned int percentage = dlpt_percentage_to_real_power;

	if (!percentage)
		percentage = 100;

	return (budget * percentage + (100 - 1)) / 100;
}

static unsigned int ppm_dlpt_pwr_budget_postprocess(unsigned int budget, unsigned int pwr_idx)
{
	/* just calculate new ratio */
	dlpt_percentage_to_real_power = (pwr_idx * 100 + (budget - 1)) / budget;
	ppm_dbg(DLPT, "new dlpt ratio = %d (%d/%d)\n", dlpt_percentage_to_real_power, pwr_idx, budget);

	return budget;
}
#else
static unsigned int ppm_dlpt_calc_trans_precentage(void)
{
	int max_pwr_idx;
	unsigned int max_real_power = get_max_real_power_by_segment(ppm_main_info.dvfs_tbl_type);

	/* dvfs table is null means ppm doesn't know chip type now */
	/* return 100 to make default ratio is 1 and check real ratio next time */
	if (!ppm_main_info.cluster_info[0].dvfs_tbl)
		return 100;

	max_pwr_idx = ppm_get_max_pwr_idx();
	if (max_pwr_idx < 0)
		return 100;

	dlpt_percentage_to_real_power = (max_pwr_idx * 100 + (max_real_power - 1)) / max_real_power;

	return dlpt_percentage_to_real_power;
}

static unsigned int ppm_dlpt_pwr_budget_preprocess(unsigned int budget)
{
	unsigned int percentage = dlpt_percentage_to_real_power;

	if (!percentage)
		percentage = ppm_dlpt_calc_trans_precentage();

	return (budget * percentage + (100 - 1)) / 100;
}

static unsigned int ppm_dlpt_pwr_budget_postprocess(unsigned int budget)
{
	unsigned int percentage = dlpt_percentage_to_real_power;

	if (!percentage)
		percentage = ppm_dlpt_calc_trans_precentage();

	return (budget * 100 + (percentage - 1)) / percentage;
}
#endif

static void ppm_dlpt_update_limit_cb(enum ppm_power_state new_state)
{
	FUNC_ENTER(FUNC_LV_POLICY);

	ppm_ver("@%s: dlpt policy update limit for new state = %s\n",
		__func__, ppm_get_power_state_name(new_state));

	ppm_hica_set_default_limit_by_state(new_state, &dlpt_policy);

	/* update limit according to power budget */
	ppm_update_req_by_pwr(new_state, &dlpt_policy.req);

	FUNC_EXIT(FUNC_LV_POLICY);
}

static void ppm_dlpt_status_change_cb(bool enable)
{
	FUNC_ENTER(FUNC_LV_POLICY);

	ppm_ver("@%s: dlpt policy status changed to %d\n", __func__, enable);

	FUNC_EXIT(FUNC_LV_POLICY);
}

static void ppm_dlpt_mode_change_cb(enum ppm_mode mode)
{
	FUNC_ENTER(FUNC_LV_POLICY);

	ppm_ver("@%s: ppm mode changed to %d\n", __func__, mode);

	FUNC_EXIT(FUNC_LV_POLICY);
}

static int ppm_dlpt_limit_proc_show(struct seq_file *m, void *v)
{
	seq_printf(m, "limited power = %d\n", dlpt_policy.req.power_budget);
	seq_printf(m, "PPM DLPT activate = %d\n", dlpt_policy.is_activated);

	return 0;
}

static ssize_t ppm_dlpt_limit_proc_write(struct file *file, const char __user *buffer,
					size_t count, loff_t *pos)
{
	unsigned int limited_power;

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

static int ppm_dlpt_budget_trans_percentage_proc_show(struct seq_file *m, void *v)
{
	seq_printf(m, "trans_percentage = %d\n", dlpt_percentage_to_real_power);
	seq_printf(m, "PPM DLPT activate = %d\n", dlpt_policy.is_activated);

	return 0;
}

static ssize_t ppm_dlpt_budget_trans_percentage_proc_write(struct file *file, const char __user *buffer,
					size_t count, loff_t *pos)
{
	unsigned int percentage;

	char *buf = ppm_copy_from_user_for_proc(buffer, count);

	if (!buf)
		return -EINVAL;

	if (!kstrtouint(buf, 10, &percentage)) {
		if (!percentage)
			ppm_err("@%s: percentage should not be 0!\n", __func__);
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
		if (!proc_create(entries[i].name, 0664, policy_dir, entries[i].fops)) {
			ppm_err("%s(), create /proc/ppm/policy/%s failed\n", __func__, entries[i].name);
			ret = -EINVAL;
			goto out;
		}
	}

	if (ppm_main_register_policy(&dlpt_policy)) {
		ppm_err("@%s: dlpt policy register failed\n", __func__);
		ret = -EINVAL;
		goto out;
	}

	dlpt_mode = PPM_DLPT_DEFAULT_MODE;

	ppm_info("@%s: register %s done! dlpt mode = %d\n", __func__, dlpt_policy.name, dlpt_mode);

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
#if PPM_HW_OCP_SUPPORT
/* should not init before OCP driver */
late_initcall(ppm_dlpt_policy_init);
#else
module_init(ppm_dlpt_policy_init);
#endif
module_exit(ppm_dlpt_policy_exit);

