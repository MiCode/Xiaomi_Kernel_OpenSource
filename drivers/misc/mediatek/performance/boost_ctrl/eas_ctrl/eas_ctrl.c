/*
 * Copyright (C) 2016-2017 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#define pr_fmt(fmt) "[eas_ctrl]"fmt

#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/string.h>
#include <linux/uaccess.h>

#include "boost_ctrl.h"
#include "eas_ctrl_plat.h"
#include "eas_ctrl.h"
#include "mtk_perfmgr_internal.h"
#include <mt-plat/mtk_sched.h>

#ifdef CONFIG_TRACING
#include <linux/kallsyms.h>
#include <linux/trace_events.h>
#endif

static struct mutex boost_eas;
#ifdef CONFIG_CGROUP_SCHEDTUNE
static int current_boost_value[NR_CGROUP];
#endif
static int boost_value[NR_CGROUP][EAS_MAX_KIR];
static int debug_boost_value[NR_CGROUP];
static int debug;
static int log_enable;
static unsigned long policy_mask[NR_CGROUP];

/************************/

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
	pr_debug("ext_launch_start\n");
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
	pr_debug("ext_launch_end\n");
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
	return clamp(boost_value, -100, 5000);
}

/************************/
#ifdef CONFIG_CGROUP_SCHEDTUNE
int update_eas_boost_value(int kicker, int cgroup_idx, int value)
{
	int final_boost_value = 0, final_boost_value_1 = 0;
	int final_boost_value_2 = -101;
	int first_prio_boost_value = 0;
	int boost_1[EAS_MAX_KIR], boost_2[EAS_MAX_KIR];
	int has_set = 0, first_prio_set = 0;
	int i, len = 0, len1 = 0;

	char msg[LOG_BUF_SIZE];
	char msg1[LOG_BUF_SIZE];

	mutex_lock(&boost_eas);

	if (cgroup_idx >= NR_CGROUP) {
		mutex_unlock(&boost_eas);
		pr_debug(" cgroup_idx >= NR_CGROUP, error\n");
		return -1;
	}

	boost_value[cgroup_idx][kicker] = value;
	len += snprintf(msg + len, sizeof(msg) - len, "[%d] [%d] [%d]",
		 kicker, cgroup_idx, value);

	/*ptr return error EIO:I/O error */
	if (len < 0)
		return -EIO;

	for (i = 0; i < EAS_MAX_KIR; i++) {
		if (boost_value[cgroup_idx][i] == 0) {
			clear_bit(i, &policy_mask[cgroup_idx]);
			continue;
		}

		if (boost_value[cgroup_idx][i] == 1100 ||
			boost_value[cgroup_idx][i] == 100) {
			first_prio_boost_value =
				MAX(boost_value[cgroup_idx][i],
						first_prio_boost_value);
			first_prio_set = 1;
		}

		boost_1[i] = boost_value[cgroup_idx][i] / 1000;
		boost_2[i] = boost_value[cgroup_idx][i] % 1000;
		final_boost_value_1 = MAX(boost_1[i], final_boost_value_1);
		final_boost_value_2 = MAX(boost_2[i], final_boost_value_2);
		has_set = 1;

		set_bit(i, &policy_mask[cgroup_idx]);

	}

	if (first_prio_set)
		final_boost_value = first_prio_boost_value;
	else if (has_set)
		final_boost_value =
			final_boost_value_1 * 1000 + final_boost_value_2;
	else
		final_boost_value = 0;

	current_boost_value[cgroup_idx] = check_boost_value(final_boost_value);

	len += snprintf(msg + len, sizeof(msg) - len, "{%d} ",
			 final_boost_value);
	/*ptr return error EIO:I/O error */
	if (len < 0)
		return -EIO;

	len1 += snprintf(msg1 + len1, sizeof(msg1) - len1, "[0x %lx] ",
			 policy_mask[cgroup_idx]);

	if (len1 < 0)
		return -EIO;

	if (!debug)
		boost_write_for_perf_idx(cgroup_idx,
				current_boost_value[cgroup_idx]);

	strncat(msg, msg1, LOG_BUF_SIZE);
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

/****************/
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

/************************************************/
static int perfmgr_current_fg_boost_proc_show(struct seq_file *m, void *v)
{
#ifdef CONFIG_CGROUP_SCHEDTUNE
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

	debug = debug_boost_value[CGROUP_FG] > 0 ? 1:0;

#ifdef CONFIG_CGROUP_SCHEDTUNE
		boost_write_for_perf_idx(CGROUP_FG,
			 debug_boost_value[CGROUP_FG]);
#endif
	return cnt;
}

static int perfmgr_debug_fg_boost_proc_show(struct seq_file *m, void *v)
{
	seq_printf(m, "%d\n", debug_boost_value[CGROUP_FG]);

	return 0;
}
/******************************************************/
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
/*******************************************************/
static int perfmgr_current_bg_boost_proc_show(struct seq_file *m, void *v)
{
#ifdef CONFIG_CGROUP_SCHEDTUNE
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

	debug = debug_boost_value[CGROUP_BG] > 0 ? 1:0;

#ifdef CONFIG_CGROUP_SCHEDTUNE
		boost_write_for_perf_idx(CGROUP_BG,
		 debug_boost_value[CGROUP_BG]);
#endif

	return cnt;
}

static int perfmgr_debug_bg_boost_proc_show(struct seq_file *m, void *v)
{
	seq_printf(m, "%d\n", debug_boost_value[CGROUP_BG]);

	return 0;
}
/************************************************/
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
/************************************************/
static ssize_t perfmgr_boot_boost_proc_write(
		struct file *filp, const char *ubuf,
		size_t cnt, loff_t *pos)
{
	int cgroup = 0, data = 0;

	int rv = check_boot_boost_proc_write(&cgroup, &data, ubuf, cnt);

	if (rv != 0)
		return rv;

	data = check_boost_value(data);

	if (cgroup >= 0 && cgroup < NR_CGROUP)
		update_eas_boost_value(EAS_KIR_BOOT, cgroup, data);

	return cnt;
}

static int perfmgr_boot_boost_proc_show(struct seq_file *m, void *v)
{
	int i;

	for (i = 0; i < NR_CGROUP; i++)
		seq_printf(m, "%d\n", boost_value[i][EAS_KIR_BOOT]);

	return 0;
}
/************************************************/
static int perfmgr_current_ta_boost_proc_show(struct seq_file *m, void *v)
{
#ifdef CONFIG_CGROUP_SCHEDTUNE
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

	debug = debug_boost_value[CGROUP_TA] > 0 ? 1:0;

#ifdef CONFIG_CGROUP_SCHEDTUNE
		boost_write_for_perf_idx(CGROUP_TA,
		 debug_boost_value[CGROUP_TA]);
#endif

	return cnt;
}

static int perfmgr_debug_ta_boost_proc_show(struct seq_file *m, void *v)
{
	seq_printf(m, "%d\n", debug_boost_value[CGROUP_TA]);

	return 0;
}

/********************************************************************/
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

/* Add procfs to control sysctl_sched_migration_cost */
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
static ssize_t perfmgr_perfmgr_log_proc_write(
		struct file *filp, const char __user *ubuf,
		size_t cnt, loff_t *pos)
{
	int data = 0;

	int rv = check_proc_write(&data, ubuf, cnt);

	if (rv != 0)
		return rv;

	log_enable = data > 0 ? 1 : 0;

	return cnt;
}

static int perfmgr_perfmgr_log_proc_show(struct seq_file *m, void *v)
{
	if (m)
		seq_printf(m, "%d\n", log_enable);
	return 0;
}

PROC_FOPS_RW(perfserv_fg_boost);
PROC_FOPS_RO(current_fg_boost);
PROC_FOPS_RW(debug_fg_boost);
PROC_FOPS_RW(perfserv_bg_boost);
PROC_FOPS_RO(current_bg_boost);
PROC_FOPS_RW(debug_bg_boost);
PROC_FOPS_RW(perfserv_ta_boost);
PROC_FOPS_RO(current_ta_boost);
PROC_FOPS_RW(debug_ta_boost);
PROC_FOPS_RW(boot_boost);
PROC_FOPS_RW(perfserv_ext_launch_mon);
PROC_FOPS_RW(m_sched_migrate_cost_n);
PROC_FOPS_RW(perfmgr_log);

/*******************************************/
int eas_ctrl_init(struct proc_dir_entry *parent)
{
	int i, j, ret = 0;
	struct proc_dir_entry *boost_dir = NULL;

	struct pentry {
		const char *name;
		const struct file_operations *fops;
	};

	const struct pentry entries[] = {
		PROC_ENTRY(perfserv_fg_boost),
		PROC_ENTRY(current_fg_boost),
		PROC_ENTRY(debug_fg_boost),
		PROC_ENTRY(perfserv_bg_boost),
		PROC_ENTRY(current_bg_boost),
		PROC_ENTRY(debug_bg_boost),
		PROC_ENTRY(perfserv_ta_boost),
		PROC_ENTRY(current_ta_boost),
		PROC_ENTRY(debug_ta_boost),
		PROC_ENTRY(boot_boost),
		PROC_ENTRY(perfmgr_log),
		/*--ext_launch--*/
		PROC_ENTRY(perfserv_ext_launch_mon),
		/*--sched migrate cost n--*/
		PROC_ENTRY(m_sched_migrate_cost_n),
	};
	mutex_init(&boost_eas);
	boost_dir = proc_mkdir("eas_ctrl", parent);

	if (!boost_dir)
		pr_debug("boost_dir null\n ");

	/* create procfs */
	for (i = 0; i < ARRAY_SIZE(entries); i++) {
		if (!proc_create(entries[i].name, 0644,
			boost_dir, entries[i].fops)) {
			pr_debug("%s(), create /eas_ctrl%s failed\n",
				__func__, entries[i].name);
			ret = -EINVAL;
			goto out;
		}
	}
		for (i = 0; i < NR_CGROUP; i++)
			for (j = 0; j < EAS_MAX_KIR; j++)
				boost_value[i][j] = 0;
out:
	return ret;
}
