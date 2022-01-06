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

#ifdef MTK_K14_EAS_BOOST
#include "eas_ctrl.h"

#define SCHED_API_READY 0

#define MAX_BOOST_VALUE	(100)
#define MIN_BOOST_VALUE	(-100)
#define MAX_UCLAMP_VALUE		(100)
#define MIN_UCLAMP_VALUE		(0)
#define MIN_DEBUG_UCLAMP_VALUE	(-1)

static int uclamp_min[NR_CGROUP][EAS_UCLAMP_MAX_KIR];
static int boost_value[NR_CGROUP][EAS_MAX_KIR];
static unsigned long prefer_idle[NR_CGROUP];
static int  perf_sched_stune_task_thresh;
static int debug_fix_boost;
static int debug_boost_value[NR_CGROUP];
/* log */
static int log_enable;

#ifdef CONFIG_SCHED_TUNE
static int current_boost_value[NR_CGROUP];
static unsigned long policy_mask[NR_CGROUP];
#endif
#endif
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

#ifdef MTK_K14_EAS_BOOST
static void walt_mode(int enable)
{
#ifdef CONFIG_SCHED_WALT
	sched_walt_enable(LT_WALT_POWERHAL, enable);
#else
	pr_debug("walt not be configured\n");
#endif
}

void ext_launch_start(void)
{
	pr_debug("_ext_launch_start\n");
	/*--feature start from here--*/
#ifdef CONFIG_TRACING
	perfmgr_trace_begin("ext_launch_start", 0, 1, 0);
#endif
	walt_mode(1);

#ifdef CONFIG_TRACING
	perfmgr_trace_end();
#endif
}

void ext_launch_end(void)
{
	pr_debug("_ext_launch_end\n");
	/*--feature end from here--*/
#ifdef CONFIG_TRACING
	perfmgr_trace_begin("ext_launch_end", 0, 0, 1);
#endif
	walt_mode(0);

#ifdef CONFIG_TRACING
	perfmgr_trace_end();
#endif
}
/************************/

static int check_boost_value(int boost_value)
{
	return clamp(boost_value, MIN_BOOST_VALUE, MAX_BOOST_VALUE);
}

static int check_uclamp_value(int value)
{
	return clamp(value, MIN_UCLAMP_VALUE, MAX_UCLAMP_VALUE);
}

#ifdef CONFIG_SCHED_TUNE
int update_eas_boost_value(int kicker, int cgroup_idx, int value)
{
	int final_boost = 0;
	int i, len = 0, len1 = 0;

	char msg[LOG_BUF_SIZE];
	char msg1[LOG_BUF_SIZE];

	if (cgroup_idx >= NR_CGROUP || cgroup_idx < 0) {
		pr_debug("cgroup_idx:%d, error\n", cgroup_idx);
		perfmgr_trace_printk("cpu_ctrl", "cgroup_idx >= NR_CGROUP\n");
		return -1;
	}

	if (kicker < 0 || kicker >= EAS_MAX_KIR) {
		pr_debug("kicker:%d error\n", kicker);
		return -1;
	}

	mutex_lock(&boost_eas);

	boost_value[cgroup_idx][kicker] = value;
	len += snprintf(msg + len, sizeof(msg) - len, "[%d] [%d] [%d]",
			kicker, cgroup_idx, value);

	/*ptr return error EIO:I/O error */
	if (len < 0) {
		perfmgr_trace_printk("cpu_ctrl", "return -EIO 1\n");
		mutex_unlock(&boost_eas);
		return -EIO;
	}

	for (i = 0; i < EAS_MAX_KIR; i++) {
		if (boost_value[cgroup_idx][i] == 0) {
			clear_bit(i, &policy_mask[cgroup_idx]);
			continue;
		}

		/* Always set first to handle negative input */
		if (final_boost == 0)
			final_boost = boost_value[cgroup_idx][i];
		else
			final_boost = MAX(final_boost,
				boost_value[cgroup_idx][i]);

		set_bit(i, &policy_mask[cgroup_idx]);
	}

	current_boost_value[cgroup_idx] = check_boost_value(final_boost);

	len += snprintf(msg + len, sizeof(msg) - len, "{%d} ", final_boost);
	/*ptr return error EIO:I/O error */
	if (len < 0) {
		perfmgr_trace_printk("cpu_ctrl", "return -EIO 2\n");
		mutex_unlock(&boost_eas);
		return -EIO;
	}
	len1 += snprintf(msg1 + len1, sizeof(msg1) - len1, "[0x %lx] ",
			policy_mask[cgroup_idx]);

	if (len1 < 0) {
		perfmgr_trace_printk("cpu_ctrl", "return -EIO 3\n");
		mutex_unlock(&boost_eas);
		return -EIO;
	}
#if SCHED_API_READY
	if (!debug_fix_boost)
		boost_write_for_perf_idx(cgroup_idx,
				current_boost_value[cgroup_idx]);
#endif
	if (strlen(msg) + strlen(msg1) < LOG_BUF_SIZE)
		strncat(msg, msg1, strlen(msg1));

	if (log_enable)
		pr_debug("%s\n", msg);

#ifdef CONFIG_TRACING
	perfmgr_trace_printk("eas_ctrl", msg);
#endif
	mutex_unlock(&boost_eas);

	return current_boost_value[cgroup_idx];
}
#else
int update_eas_boost_value(int kicker, int cgroup_idx, int value)
{
	return -1;
}
#endif

static ssize_t perfmgr_perfserv_prefer_idle_proc_write(
		struct file *filp, const char *ubuf,
		size_t cnt, loff_t *pos)
{
	int cgroup = 0, data = 0;

	int rv = check_group_proc_write(&cgroup, &data, ubuf, cnt);

	if (rv != 0)
		return rv;

	if (data < 0 || data > 1)
		return -EINVAL;

	if (cgroup >= 0 && cgroup < NR_CGROUP) {
		if (data != 0)
			update_prefer_idle_value(EAS_PREFER_IDLE_KIR_PERF,
			cgroup, 1);
		else
			update_prefer_idle_value(EAS_PREFER_IDLE_KIR_PERF,
			cgroup, 0);
	}

	return cnt;
}

static int perfmgr_perfserv_prefer_idle_proc_show(struct seq_file *m, void *v)
{
	int i;

	for (i = 0; i < NR_CGROUP; i++)
		seq_printf(m, "%d\n",
		test_bit(EAS_PREFER_IDLE_KIR_PERF, &prefer_idle[i]));

	return 0;
}

static ssize_t perfmgr_boot_boost_proc_write(
		struct file *filp, const char *ubuf,
		size_t cnt, loff_t *pos)
{
	int cgroup = 0, data = 0;

	int rv = check_group_proc_write(&cgroup, &data, ubuf, cnt);

	if (rv != 0)
		return rv;

	data = check_uclamp_value(data);

	if (cgroup >= 0 && cgroup < NR_CGROUP)
		update_eas_uclamp_min(EAS_UCLAMP_KIR_BOOT, cgroup, data);

	return cnt;
}

static int perfmgr_boot_boost_proc_show(struct seq_file *m, void *v)
{
	int i;

	for (i = 0; i < NR_CGROUP; i++)
		seq_printf(m, "%d\n", uclamp_min[i][EAS_UCLAMP_KIR_BOOT]);

	return 0;
}

static ssize_t perfmgr_perfserv_bg_boost_proc_write(struct file *filp,
		const char *ubuf, size_t cnt, loff_t *pos)
{
	int data = 0;

	int rv = check_proc_write(&data, ubuf, cnt);

	if (rv != 0)
		return rv;

	data = check_boost_value(data);

	update_eas_boost_value(EAS_KIR_PERF, CGROUP_BG, data);

	return cnt;
}

static int perfmgr_perfserv_bg_boost_proc_show(struct seq_file *m, void *v)
{
	seq_printf(m, "%d\n", boost_value[CGROUP_BG][EAS_KIR_PERF]);

	return 0;
}

static ssize_t perfmgr_perfserv_fg_boost_proc_write(struct file *filp
		, const char *ubuf, size_t cnt, loff_t *pos)
{
	int data = 0;

	int rv = check_proc_write(&data, ubuf, cnt);

	if (rv != 0)
		return rv;

	data = check_boost_value(data);

	update_eas_boost_value(EAS_KIR_PERF, CGROUP_FG, data);

	return cnt;
}

static int perfmgr_perfserv_fg_boost_proc_show(struct seq_file *m, void *v)
{
	seq_printf(m, "%d\n", boost_value[CGROUP_FG][EAS_KIR_PERF]);

	return 0;
}

static ssize_t perfmgr_perfserv_ta_boost_proc_write(
		struct file *filp, const char *ubuf,
		size_t cnt, loff_t *pos)
{
	int data = 0;
	int rv = check_proc_write(&data, ubuf, cnt);

	if (rv != 0)
		return rv;

	data = check_boost_value(data);

	update_eas_boost_value(EAS_KIR_PERF, CGROUP_TA, data);

	return cnt;
}

static int perfmgr_perfserv_ta_boost_proc_show(
		struct seq_file *m, void *v)
{
	seq_printf(m, "%d\n", boost_value[CGROUP_TA][EAS_KIR_PERF]);

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

static ssize_t perfmgr_sched_stune_task_thresh_proc_write(struct file *filp,
		const char *ubuf, size_t cnt, loff_t *pos)
{
	int data = 0;
	int rv = check_proc_write(&data, ubuf, cnt);

	if (rv != 0)
		return rv;

	perf_sched_stune_task_thresh = data;

#if SCHED_API_READY
#ifdef CONFIG_SCHED_TUNE
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

static int ext_launch_state;
static ssize_t perfmgr_perfserv_ext_launch_mon_proc_write(struct file *filp,
		const char *ubuf, size_t cnt, loff_t *pos)
{
	int data = 0;
	int rv = check_proc_write(&data, ubuf, cnt);

	if (rv != 0)
		return rv;

	if (data) {
		ext_launch_start();
		ext_launch_state = 1;
	} else {
		ext_launch_end();
		ext_launch_state = 0;
	}

	pr_debug("perfmgr_perfserv_ext_launch_mon");
	return cnt;
}

static int
perfmgr_perfserv_ext_launch_mon_proc_show(
		struct seq_file *m, void *v)
{
	seq_printf(m, "%d\n", ext_launch_state);

	return 0;
}

/************************************************/
static int perfmgr_current_fg_boost_proc_show(struct seq_file *m, void *v)
{
#ifdef CONFIG_SCHED_TUNE
	seq_printf(m, "%d\n", current_boost_value[CGROUP_FG]);
#else
	seq_printf(m, "%d\n", -1);
#endif

	return 0;
}

/************************************************************/
static ssize_t perfmgr_debug_fg_boost_proc_write(
		struct file *filp, const char *ubuf,
		size_t cnt, loff_t *pos)
{
	int data = 0;

	int rv = check_proc_write(&data, ubuf, cnt);

	if (rv != 0)
		return rv;

	debug_boost_value[CGROUP_FG] = check_boost_value(data);

	debug_fix_boost = debug_boost_value[CGROUP_FG] > 0 ? 1:0;

#if SCHED_API_READY
#ifdef CONFIG_SCHED_TUNE
	boost_write_for_perf_idx(CGROUP_FG,
			debug_boost_value[CGROUP_FG]);
#endif
#endif
	return cnt;
}

static int perfmgr_debug_fg_boost_proc_show(struct seq_file *m, void *v)
{
	seq_printf(m, "%d\n", debug_boost_value[CGROUP_FG]);

	return 0;
}

/*******************************************************/
static int perfmgr_current_bg_boost_proc_show(struct seq_file *m, void *v)
{
#ifdef CONFIG_SCHED_TUNE
	seq_printf(m, "%d\n", current_boost_value[CGROUP_BG]);
#else
	seq_printf(m, "%d\n", -1);
#endif

	return 0;
}

/**************************************************/
static ssize_t perfmgr_debug_bg_boost_proc_write(
		struct file *filp, const char *ubuf,
		size_t cnt, loff_t *pos)
{
	int data = 0;
	int rv = check_proc_write(&data, ubuf, cnt);

	if (rv != 0)
		return rv;

	debug_boost_value[CGROUP_BG] = check_boost_value(data);

	debug_fix_boost = debug_boost_value[CGROUP_BG] > 0 ? 1:0;

#if SCHED_API_READY
#ifdef CONFIG_SCHED_TUNE
	boost_write_for_perf_idx(CGROUP_BG,
			debug_boost_value[CGROUP_BG]);
#endif
#endif
	return cnt;
}

static int perfmgr_debug_bg_boost_proc_show(struct seq_file *m, void *v)
{
	seq_printf(m, "%d\n", debug_boost_value[CGROUP_BG]);

	return 0;
}

/************************************************/
static int perfmgr_current_ta_boost_proc_show(struct seq_file *m, void *v)
{
#ifdef CONFIG_SCHED_TUNE
	seq_printf(m, "%d\n", current_boost_value[CGROUP_TA]);
#else
	seq_printf(m, "%d\n", -1);
#endif

	return 0;
}

/**********************************/
static ssize_t perfmgr_debug_ta_boost_proc_write(
		struct file *filp, const char *ubuf,
		size_t cnt, loff_t *pos)
{
	int data = 0;
	int rv = check_proc_write(&data, ubuf, cnt);

	if (rv != 0)
		return rv;

	debug_boost_value[CGROUP_TA] = check_boost_value(data);

	debug_fix_boost = debug_boost_value[CGROUP_TA] > 0 ? 1:0;
#if SCHED_API_READY
#ifdef CONFIG_SCHED_TUNE
	boost_write_for_perf_idx(CGROUP_TA,
			debug_boost_value[CGROUP_TA]);
#endif
#endif
	return cnt;
}

static int perfmgr_debug_ta_boost_proc_show(struct seq_file *m, void *v)
{
	seq_printf(m, "%d\n", debug_boost_value[CGROUP_TA]);

	return 0;
}

#endif

/* others */
#ifdef MTK_K14_EAS_BOOST
PROC_FOPS_RW(m_sched_migrate_cost_n);
PROC_FOPS_RW(sched_stune_task_thresh);
#endif
PROC_FOPS_RW(sched_big_task_rotation);
PROC_FOPS_RW(sched_boost);
PROC_FOPS_RW(cpu_prefer);

#ifdef MTK_K14_EAS_BOOST
/* boost value */
PROC_FOPS_RW(perfserv_fg_boost);
PROC_FOPS_RW(perfserv_bg_boost);
PROC_FOPS_RW(perfserv_ta_boost);
PROC_FOPS_RW(boot_boost);
PROC_FOPS_RO(current_fg_boost);
PROC_FOPS_RW(debug_fg_boost);
PROC_FOPS_RO(current_bg_boost);
PROC_FOPS_RW(debug_bg_boost);
PROC_FOPS_RO(current_ta_boost);
PROC_FOPS_RW(debug_ta_boost);
#endif

#ifdef MTK_K14_EAS_BOOST
/*--ext_launch--*/
PROC_FOPS_RW(perfserv_ext_launch_mon);
PROC_FOPS_RW(perfserv_prefer_idle);
#endif

PROC_FOPS_RW(set_sched_isolation);
PROC_FOPS_RW(set_sched_deisolation);
PROC_FOPS_RO(sched_isolated);
/*******************************************/
int eas_ctrl_init(struct proc_dir_entry *parent)
{
	int ret = 0;
	size_t i;
#ifdef MTK_K14_EAS_BOOST
#if defined(CONFIG_SCHED_TUNE)
	int j;
#endif
#endif
	struct pentry {
		const char *name;
		const struct file_operations *fops;
	};

	const struct pentry entries[] = {
		/*--sched migrate cost n--*/
#ifdef MTK_K14_EAS_BOOST
		PROC_ENTRY(m_sched_migrate_cost_n),
		PROC_ENTRY(sched_stune_task_thresh),
#endif
		PROC_ENTRY(sched_big_task_rotation),
		PROC_ENTRY(sched_boost),
		PROC_ENTRY(cpu_prefer),
		PROC_ENTRY(set_sched_isolation),
		PROC_ENTRY(set_sched_deisolation),
		PROC_ENTRY(sched_isolated),
#ifdef MTK_K14_EAS_BOOST
		PROC_ENTRY(perfserv_prefer_idle),
		/*--ext_launch--*/
		PROC_ENTRY(perfserv_ext_launch_mon),
		/* boost value */
		PROC_ENTRY(perfserv_fg_boost),
		PROC_ENTRY(perfserv_bg_boost),
		PROC_ENTRY(perfserv_ta_boost),
		PROC_ENTRY(current_fg_boost),
		PROC_ENTRY(debug_fg_boost),
		PROC_ENTRY(current_bg_boost),
		PROC_ENTRY(debug_bg_boost),
		PROC_ENTRY(current_ta_boost),
		PROC_ENTRY(debug_ta_boost),
		PROC_ENTRY(boot_boost),
#endif
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
#ifdef MTK_K14_EAS_BOOST
#if defined(CONFIG_SCHED_TUNE)
	/* boost value */
	for (i = 0; i < NR_CGROUP; i++) {
		current_boost_value[i] = 0;
		for (j = 0; j < EAS_MAX_KIR; j++)
			boost_value[i][j] = 0;
		prefer_idle[i] = 0;
	}
#endif

#if defined(CONFIG_UCLAMP_TASK_GROUP) && defined(CONFIG_SCHED_TUNE)
	/* uclamp */
	for (i = 0; i < NR_CGROUP; i++) {
		//cur_uclamp_min[i] = 0;
		//debug_uclamp_min[i] = -1;
		for (j = 0; j < EAS_UCLAMP_MAX_KIR; j++)
			uclamp_min[i][j] = 0;
	}
#endif
	perf_sched_stune_task_thresh = -1;
	debug_fix_boost = 0;
#endif
	last_cpu_prefer_pid = (pid_t)0;
	last_cpu_perfer_type = 0;

out:
	return ret;
}
