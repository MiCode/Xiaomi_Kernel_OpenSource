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

static bool perf_sched_big_task_rotation;
static int  perf_sched_boost;

/************************/


/************************/

/********************************************************************/

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

/* Add procfs to control sysctl_sched_rotation_enable */
/* sysctl_sched_rotation_enable: eas_ctrl_plat.h */
static ssize_t perfmgr_sched_boost_proc_write(struct file *filp,
		const char *ubuf, size_t cnt, loff_t *pos)
{
	int data = 0;
	int rv = check_proc_write(&data, ubuf, cnt);

	if (rv != 0)
		return rv;

	if (data < 0 || data > 2)
		return -EINVAL;

	perf_sched_boost = data;

#if API_READY
	if (data)
		set_sched_boost_enable(true);
	else
		set_sched_boost_enable(false);
#endif

	return cnt;
}

static int perfmgr_sched_boost_proc_show(struct seq_file *m, void *v)
{
	seq_printf(m, "sched_boost = %d ", perf_sched_boost);
	if (perf_sched_boost == 0)
		seq_puts(m, "(no boost)\n");
	else if (perf_sched_boost == 1)
		seq_puts(m, "(all boost)\n");
	else if (perf_sched_boost == 2)
		seq_puts(m, "(foreground boost)\n");
	else
		seq_puts(m, "(invalid setting)\n");

	return 0;
}

/* others */
PROC_FOPS_RW(sched_big_task_rotation);
PROC_FOPS_RW(sched_boost);

/*******************************************/
int eas_ctrl_init(struct proc_dir_entry *parent)
{
	int i, ret = 0;

	struct pentry {
		const char *name;
		const struct file_operations *fops;
	};

	const struct pentry entries[] = {
		/*--sched migrate cost n--*/
		PROC_ENTRY(sched_big_task_rotation),
		PROC_ENTRY(sched_boost),
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
	perf_sched_boost = 0;

out:
	return ret;
}
