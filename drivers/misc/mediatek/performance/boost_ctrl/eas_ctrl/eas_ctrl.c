// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#define pr_fmt(fmt) "[eas_ctrl]"fmt

#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/string.h>
#include <linux/uaccess.h>

#include "boost_ctrl.h"
#include "eas_ctrl_plat.h"
#include <mt-plat/eas_ctrl.h>
#include "mtk_perfmgr_internal.h"
//#include <mt-plat/mtk_sched.h>
#include <linux/sched.h>

#ifdef CONFIG_TRACING
#include <linux/kallsyms.h>
#include <linux/trace_events.h>
#endif

#define API_READY 0
/* boost value */
static struct mutex boost_eas;

static int cur_schedplus_sync_flag;
static int default_schedplus_sync_flag;
static unsigned long schedplus_sync_flag_policy_mask;
static int schedplus_sync_flag[EAS_SYNC_FLAG_MAX_KIR];
static int debug_schedplus_sync_flag;

static bool perf_sched_big_task_rotation;
static int  perf_sched_stune_task_thresh;

/************************/

static void walt_mode(int enable)
{
#ifdef CONFIG_SCHED_WALT
	sched_walt_enable(LT_WALT_POWERHAL, enable);
#else
	pr_debug("walt not be configured\n");
#endif
}

void walt_mode_set(int on_off)
{
	/*--feature start from here--*/
	if (on_off) {
#ifdef CONFIG_TRACING
		perfmgr_trace_begin("walt_mode_start", 0, 1, 0);
#endif
		walt_mode(1);
	} else {
#ifdef CONFIG_TRACING
		perfmgr_trace_begin("walt_mode_end", 0, 1, 0);
#endif
		walt_mode(0);
	}

#ifdef CONFIG_TRACING
	perfmgr_trace_end();
#endif
}


/************************/

/************************/
int update_schedplus_sync_flag(int kicker, int enable)
{
	int i;
	int final_sync_flag = -1;

	mutex_lock(&boost_eas);


	schedplus_sync_flag[kicker] = clamp(enable, -1, 1);

	for (i = 0; i < EAS_SYNC_FLAG_MAX_KIR; i++) {
		if (schedplus_sync_flag[i] == -1) {
			clear_bit(i, &schedplus_sync_flag_policy_mask);
			continue;
		}

		if (final_sync_flag >= 0)
			final_sync_flag = MAX(final_sync_flag,
				schedplus_sync_flag[i]);
		else
			final_sync_flag = schedplus_sync_flag[i];

		set_bit(i, &schedplus_sync_flag_policy_mask);
	}

	cur_schedplus_sync_flag = final_sync_flag < 0 ?
		-1 : final_sync_flag;

	if (debug_schedplus_sync_flag == -1) {
		if (cur_schedplus_sync_flag >= 0)
			sysctl_sched_sync_hint_enable =
				cur_schedplus_sync_flag;
		else
			sysctl_sched_sync_hint_enable =
				default_schedplus_sync_flag;
	}
	pr_debug("%s %d %d %d %d", __func__, kicker, enable,
		cur_schedplus_sync_flag, debug_schedplus_sync_flag);

	mutex_unlock(&boost_eas);

	return cur_schedplus_sync_flag;
}

/********************************************************************/
static int sched_walt_state;
static ssize_t perfmgr_sched_walt_mode_proc_write(struct file *filp,
		const char *ubuf, size_t cnt, loff_t *pos)
{
	int data = 0;
	int rv = check_proc_write(&data, ubuf, cnt);

	if (rv != 0)
		return rv;

	if (data) {
		walt_mode_set(1);
		sched_walt_state = 1;
	} else {
		walt_mode_set(0);
		sched_walt_state = 0;
	}

	return cnt;
}

static int perfmgr_sched_walt_mode_proc_show(
		struct seq_file *m, void *v)
{
	seq_printf(m, "%d\n", sched_walt_state);
	return 0;
}


static ssize_t perfmgr_perfserv_schedplus_sync_flag_proc_write(
		struct file *filp, const char *ubuf,
		size_t cnt, loff_t *pos)
{
	int data = 0;

	int rv = check_proc_write(&data, ubuf, cnt);

	if (rv != 0)
		return rv;

	update_schedplus_sync_flag(EAS_SYNC_FLAG_KIR_PERF, data);

	return cnt;
}

static int perfmgr_perfserv_schedplus_sync_flag_proc_show(
	struct seq_file *m, void *v)
{
	seq_printf(m, "%d\n", schedplus_sync_flag[EAS_SYNC_FLAG_KIR_PERF]);

	return 0;
}

static ssize_t perfmgr_debug_schedplus_sync_flag_proc_write(
		struct file *filp, const char *ubuf,
		size_t cnt, loff_t *pos)
{
	int data = 0;

	int rv = check_proc_write(&data, ubuf, cnt);

	if (rv != 0)
		return rv;

	mutex_lock(&boost_eas);
	debug_schedplus_sync_flag = data;
	if (data == -1)
		sysctl_sched_sync_hint_enable = default_schedplus_sync_flag;
	else
		sysctl_sched_sync_hint_enable = data ? 1 : 0;
	mutex_unlock(&boost_eas);

	return cnt;
}

static int perfmgr_debug_schedplus_sync_flag_proc_show(
	struct seq_file *m, void *v)
{
	seq_printf(m, "%d\n", debug_schedplus_sync_flag);

	return 0;
}

/* Add procfs to control sysctl_sched_migration_cost */
/* sysctl_sched_migration_cost: eas_ctrl_plat.h */
static ssize_t perfmgr_m_sched_migrate_cost_n_proc_write(struct file *filp,
		const char *ubuf, size_t cnt, loff_t *pos)
{
	int data = 0;
	int rv = check_proc_write(&data, ubuf, cnt);

	if (rv != 0)
		return rv;

	sysctl_sched_migration_cost = data;

	return cnt;
}

static int perfmgr_m_sched_migrate_cost_n_proc_show(struct seq_file *m, void *v)
{
	seq_printf(m, "%d\n", sysctl_sched_migration_cost);

	return 0;
}

/* Add procfs to control sysctl_sched_rotation_enable */
/* sysctl_sched_rotation_enable: eas_ctrl_plat.h */
static ssize_t perfmgr_sched_big_task_rotation_proc_write(struct file *filp,
		const char *ubuf, size_t cnt, loff_t *pos)
{
	int data = 0;
	int rv = check_proc_write(&data, ubuf, cnt);

	if (rv != 0)
		return rv;

	perf_sched_big_task_rotation = data;

#if API_READY
	if (data)
		set_sched_rotation_enable(true);
	else
		set_sched_rotation_enable(false);
#endif

	return cnt;
}

static int perfmgr_sched_big_task_rotation_proc_show(struct seq_file *m,
	void *v)
{
	seq_printf(m, "%d\n", perf_sched_big_task_rotation);

	return 0;
}

static ssize_t perfmgr_sched_stune_task_thresh_proc_write(struct file *filp,
		const char *ubuf, size_t cnt, loff_t *pos)
{
	int data = 0;
	int rv = check_proc_write(&data, ubuf, cnt);

	if (rv != 0)
		return rv;

	perf_sched_stune_task_thresh = data;
#ifdef CONFIG_SCHED_TUNE
#if API_READY
	if (perf_sched_stune_task_thresh >= 0)
		set_stune_task_threshold(perf_sched_stune_task_thresh);
	else
		set_stune_task_threshold(-1);
#endif
#endif

	return cnt;
}

static int perfmgr_sched_stune_task_thresh_proc_show(struct seq_file *m,
	void *v)
{
	seq_printf(m, "%d\n", perf_sched_stune_task_thresh);

	return 0;
}

PROC_FOPS_RW(perfserv_schedplus_sync_flag);
PROC_FOPS_RW(debug_schedplus_sync_flag);

/* others */
PROC_FOPS_RW(sched_walt_mode);
PROC_FOPS_RW(m_sched_migrate_cost_n);
PROC_FOPS_RW(sched_big_task_rotation);
PROC_FOPS_RW(sched_stune_task_thresh);

/*******************************************/
int eas_ctrl_init(struct proc_dir_entry *parent)
{
	int i, ret = 0;

	struct pentry {
		const char *name;
		const struct file_operations *fops;
	};

	const struct pentry entries[] = {
		PROC_ENTRY(perfserv_schedplus_sync_flag),
		PROC_ENTRY(debug_schedplus_sync_flag),

		/*--ext_launch--*/
		PROC_ENTRY(sched_walt_mode),
		/*--sched migrate cost n--*/
		PROC_ENTRY(m_sched_migrate_cost_n),
		PROC_ENTRY(sched_big_task_rotation),
		PROC_ENTRY(sched_stune_task_thresh),
	};
	mutex_init(&boost_eas);

	/* create procfs */
	for (i = 0; i < ARRAY_SIZE(entries); i++) {
		if (!proc_create(entries[i].name, 0644,
					parent, entries[i].fops)) {
			pr_debug("%s(), create /eas_ctrl%s failed\n",
					__func__, entries[i].name);
			ret = -EINVAL;
			goto out;
		}
	}

	perf_sched_big_task_rotation = 0;
	perf_sched_stune_task_thresh = -1;

	default_schedplus_sync_flag = 1;
	cur_schedplus_sync_flag = -1;
	debug_schedplus_sync_flag = -1;
	for (i = 0; i < EAS_SYNC_FLAG_MAX_KIR; i++)
		schedplus_sync_flag[i] = -1;

out:
	return ret;
}
