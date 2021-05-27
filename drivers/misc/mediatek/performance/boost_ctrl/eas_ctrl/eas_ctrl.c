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

/* boost value */
static struct mutex boost_eas;

static bool perf_sched_big_task_rotation;
static pid_t last_cpu_prefer_pid;
static int last_cpu_perfer_type;

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

#ifdef CONFIG_MTK_SCHED_BIG_TASK_MIGRATE
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

/* Add procfs to control sched_boost */
/* set_sched_boost_type: eas_ctrl_plat.h */
static ssize_t perfmgr_sched_boost_proc_write(struct file *filp,
		const char *ubuf, size_t cnt, loff_t *pos)
{
	int data = 0;
	int rv = check_proc_write(&data, ubuf, cnt);

	if (rv != 0)
		return rv;

	if (data < 0 || data > 2)
		return -EINVAL;

#if defined(CONFIG_CGROUPS) && defined(CONFIG_MTK_SCHED_CPU_PREFER)
	if (set_sched_boost_type(data) < 0)
		return -EINVAL;
#endif

	return cnt;
}

static int perfmgr_sched_boost_proc_show(struct seq_file *m, void *v)
{
	int boost_type = 0;

#if defined(CONFIG_CGROUPS) && defined(CONFIG_MTK_SCHED_CPU_PREFER)
	boost_type = get_sched_boost_type();
#endif

	seq_printf(m, "sched_boost = %d ", boost_type);
	if (boost_type == 0)
		seq_puts(m, "(no boost)\n");
	else if (boost_type == 1)
		seq_puts(m, "(all boost)\n");
	else if (boost_type == 2)
		seq_puts(m, "(foreground boost)\n");
	else
		seq_puts(m, "(invalid setting)\n");

	return 0;
}

/* Add procfs to control cpu_prefer */
/* set_sched_boost_type: eas_ctrl_plat.h */
static ssize_t perfmgr_cpu_prefer_proc_write(struct file *filp,
		const char *ubuf, size_t cnt, loff_t *pos)
{
	char buf[64];
	pid_t pid, type;

	if (cnt >= sizeof(buf))
		return -EINVAL;

	if (copy_from_user(buf, ubuf, cnt))
		return -EFAULT;
	buf[cnt] = '\0';

	if (sscanf(buf, "%d %d", (int *)&pid, &type) != 2)
		return -EFAULT;

#ifdef CONFIG_MTK_SCHED_CPU_PREFER
	if (sched_set_cpuprefer(pid, type) != 0)
		return -EINVAL;
#endif
	last_cpu_prefer_pid = pid;
	last_cpu_perfer_type = type;

	return cnt;
}

static int perfmgr_cpu_prefer_proc_show(struct seq_file *m, void *v)
{
	seq_printf(m, "last pid:%d, type:%d\n",
		(int)last_cpu_prefer_pid, last_cpu_perfer_type);

	return 0;
}

static int perfmgr_set_sched_isolation_proc_show(struct seq_file *m, void *v)
{
	seq_puts(m, "");
	return 0;
}
static ssize_t perfmgr_set_sched_isolation_proc_write(struct file *filp,
		const char *ubuf, size_t cnt, loff_t *pos)
{
	unsigned int cpu_id = -1;
	char buf[64];

	if (cnt >= sizeof(buf))
		return -EINVAL;

	if (copy_from_user(buf, ubuf, cnt))
		return -EFAULT;
	buf[cnt] = '\0';

	if (sscanf(buf, "%iu", &cpu_id) != 1)
		return -EINVAL;

	if (cpu_id >= nr_cpu_ids)
		return cnt;

	sched_isolate_cpu(cpu_id);

	return cnt;
}
static int perfmgr_set_sched_deisolation_proc_show(struct seq_file *m, void *v)
{
	seq_puts(m, "");
	return 0;
}
static ssize_t perfmgr_set_sched_deisolation_proc_write(struct file *filp,
		const char *ubuf, size_t cnt, loff_t *pos)
{
	unsigned int cpu_id = -1;
	char buf[64];

	if (cnt >= sizeof(buf))
		return -EINVAL;

	if (copy_from_user(buf, ubuf, cnt))
		return -EFAULT;
	buf[cnt] = '\0';

	if (sscanf(buf, "%iu", &cpu_id) != 1)
		return -EINVAL;

	if (cpu_id >= nr_cpu_ids)
		return cnt;

	sched_unisolate_cpu(cpu_id);

	return cnt;
}

static int perfmgr_sched_isolated_proc_show(struct seq_file *m, void *v)
{

	seq_printf(m, "cpu_isolated_mask=0x%lx\n",
		__cpu_isolated_mask.bits[0]);

	return 0;
}

/* others */
PROC_FOPS_RW(sched_big_task_rotation);
PROC_FOPS_RW(sched_boost);
PROC_FOPS_RW(cpu_prefer);

PROC_FOPS_RW(set_sched_isolation);
PROC_FOPS_RW(set_sched_deisolation);
PROC_FOPS_RO(sched_isolated);
/*******************************************/
int eas_ctrl_init(struct proc_dir_entry *parent)
{
	int ret = 0;
	size_t i;

	struct pentry {
		const char *name;
		const struct file_operations *fops;
	};

	const struct pentry entries[] = {
		/*--sched migrate cost n--*/
		PROC_ENTRY(sched_big_task_rotation),
		PROC_ENTRY(sched_boost),
		PROC_ENTRY(cpu_prefer),
		PROC_ENTRY(set_sched_isolation),
		PROC_ENTRY(set_sched_deisolation),
		PROC_ENTRY(sched_isolated),
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
	last_cpu_prefer_pid = (pid_t)0;
	last_cpu_perfer_type = 0;

out:
	return ret;
}
