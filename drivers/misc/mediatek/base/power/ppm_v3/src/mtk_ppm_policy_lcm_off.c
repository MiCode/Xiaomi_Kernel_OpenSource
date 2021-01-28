// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/notifier.h>
#include <linux/fb.h>

#include "mtk_ppm_internal.h"


static void ppm_lcmoff_update_limit_cb(void);
static void ppm_lcmoff_status_change_cb(bool enable);
static int lcmoff_min_freq;

/* other members will init by ppm_main */
static struct ppm_policy_data lcmoff_policy = {
	.name			= __stringify(PPM_POLICY_LCM_OFF),
	.lock			= __MUTEX_INITIALIZER(lcmoff_policy.lock),
	.policy			= PPM_POLICY_LCM_OFF,
	.priority		= PPM_POLICY_PRIO_USER_SPECIFY_BASE,
	.update_limit_cb	= ppm_lcmoff_update_limit_cb,
	.status_change_cb	= ppm_lcmoff_status_change_cb,
};

bool ppm_lcmoff_is_policy_activated(void)
{
	bool is_activate;

	ppm_lock(&lcmoff_policy.lock);
	is_activate = lcmoff_policy.is_activated;
	ppm_unlock(&lcmoff_policy.lock);

	return is_activate;
}

static void ppm_lcmoff_update_limit_cb(void)
{
	unsigned int i;

	FUNC_ENTER(FUNC_LV_POLICY);

	ppm_clear_policy_limit(&lcmoff_policy);

	/* only apply min freq for LL cluster */
	for (i = 0; i < 1; i++) {
	/* for (i = 0; i < lcmoff_policy.req.cluster_num; i++) { */
		if (lcmoff_policy.req.limit[i].min_cpufreq_idx != -1) {
			int idx = ppm_main_freq_to_idx(i,
				lcmoff_min_freq,
				CPUFREQ_RELATION_L);

			lcmoff_policy.req.limit[i].min_cpufreq_idx =
				MIN(lcmoff_policy.req.limit[i].min_cpufreq_idx,
				idx);
		}
	}

	FUNC_EXIT(FUNC_LV_POLICY);
}

static void ppm_lcmoff_status_change_cb(bool enable)
{
	FUNC_ENTER(FUNC_LV_POLICY);

	ppm_ver("@%s: lcmoff policy status changed to %d\n", __func__, enable);

	FUNC_EXIT(FUNC_LV_POLICY);
}

static void ppm_lcmoff_switch(int onoff)
{
	unsigned int i;

	FUNC_ENTER(FUNC_LV_POLICY);

	ppm_info("@%s: onoff = %d\n", __func__, onoff);

	ppm_lock(&lcmoff_policy.lock);

	/* onoff = 0: LCM OFF */
	/* others: LCM ON */
	if (onoff) {
		/* deactivate lcmoff policy */
		if (lcmoff_policy.is_activated) {
			lcmoff_policy.is_activated = false;
			for (i = 0; i < lcmoff_policy.req.cluster_num; i++) {
				lcmoff_policy.req.limit[i].min_cpufreq_idx =
					get_cluster_min_cpufreq_idx(i);
				lcmoff_policy.req.limit[i].max_cpufreq_idx =
					get_cluster_max_cpufreq_idx(i);
			}
		}
	} else {
		/* activate lcmoff policy */
		if (lcmoff_policy.is_enabled)
			lcmoff_policy.is_activated = true;
	}

	ppm_unlock(&lcmoff_policy.lock);

	FUNC_EXIT(FUNC_LV_POLICY);
}

static int ppm_lcmoff_fb_notifier_callback(struct notifier_block *self,
	unsigned long event, void *data)
{
	struct fb_event *evdata = data;
	int blank;

	FUNC_ENTER(FUNC_LV_POLICY);

	/* skip if it's not a blank event */
	if (event != FB_EVENT_BLANK)
		return 0;

	blank = *(int *)evdata->data;
	ppm_ver("@%s: blank = %d, event = %lu\n", __func__, blank, event);

	switch (blank) {
	/* LCM ON */
	case FB_BLANK_UNBLANK:
		ppm_lcmoff_switch(1);
		break;
	/* LCM OFF */
	case FB_BLANK_POWERDOWN:
		ppm_lcmoff_switch(0);
		break;
	default:
		break;
	}

	FUNC_EXIT(FUNC_LV_POLICY);

	return 0;
}

static struct notifier_block ppm_lcmoff_fb_notifier = {
	.notifier_call = ppm_lcmoff_fb_notifier_callback,
};

static int ppm_lcmoff_min_freq_proc_show(struct seq_file *m, void *v)
{
	seq_printf(m, "lcmoff_min_freq = %d KHz\n", lcmoff_min_freq);

	return 0;
}

static ssize_t ppm_lcmoff_min_freq_proc_write(struct file *file,
		const char __user *buffer, size_t count, loff_t *pos)
{
	unsigned int freq = 0;

	char *buf = ppm_copy_from_user_for_proc(buffer, count);

	if (!buf)
		return -EINVAL;

	if (!kstrtouint(buf, 10, &freq))
		lcmoff_min_freq = freq;
	else
		ppm_err("@%s: Invalid input!\n", __func__);

	free_page((unsigned long)buf);
	return count;
}

PROC_FOPS_RW(lcmoff_min_freq);
static int __init ppm_lcmoff_policy_init(void)
{
	int ret = 0, i;

	struct pentry {
		const char *name;
		const struct file_operations *fops;
	};

	const struct pentry entries[] = {
		PROC_ENTRY(lcmoff_min_freq),
	};

	FUNC_ENTER(FUNC_LV_POLICY);

	/* create procfs */
	for (i = 0; i < ARRAY_SIZE(entries); i++) {
		if (!proc_create(entries[i].name, 0664,
			policy_dir, entries[i].fops)) {
			ppm_err("%s(), create /proc/ppm/policy/%s failed\n",
				__func__, entries[i].name);
			ret = -EINVAL;
			goto out;
		}
	}

	if (fb_register_client(&ppm_lcmoff_fb_notifier)) {
		ppm_err("@%s: lcmoff policy register FB client failed!\n",
			__func__);
		ret = -EINVAL;
		goto out;
	}

	if (ppm_main_register_policy(&lcmoff_policy)) {
		ppm_err("@%s: lcmoff policy register failed\n", __func__);
		ret = -EINVAL;
		goto out;
	}

#ifdef LCMOFF_MIN_FREQ
	lcmoff_min_freq = LCMOFF_MIN_FREQ;
#else
	lcmoff_policy.is_enabled = false;
#endif

	ppm_info("@%s: register %s done!\n", __func__, lcmoff_policy.name);

out:
	FUNC_EXIT(FUNC_LV_POLICY);

	return ret;
}

static void __exit ppm_lcmoff_policy_exit(void)
{
	FUNC_ENTER(FUNC_LV_POLICY);

	fb_unregister_client(&ppm_lcmoff_fb_notifier);

	ppm_main_unregister_policy(&lcmoff_policy);

	FUNC_EXIT(FUNC_LV_POLICY);
}

/* Cannot init before FB driver */
late_initcall(ppm_lcmoff_policy_init);
module_exit(ppm_lcmoff_policy_exit);

