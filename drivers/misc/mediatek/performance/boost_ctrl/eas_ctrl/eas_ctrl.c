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
#include "topo_ctrl.h"
#include "mtk_perfmgr_internal.h"
#include <mt-plat/mtk_sched.h>
#include <linux/sched.h>

#ifdef CONFIG_TRACING
#include <linux/kallsyms.h>
#include <linux/trace_events.h>
#endif

#if defined(CONFIG_MTK_PLAT_MT6885_EMULATION) || defined(CONFIG_MACH_MT6893) \
	|| defined(CONFIG_MACH_MT6833)
#define CONFIG_CPUFREQ_HAVE_GOVERNOR_PER_POLICY
#endif

/* boost value */
static struct mutex boost_eas;
#ifdef CONFIG_SCHED_TUNE
static int current_boost_value[NR_CGROUP];
static unsigned long policy_mask[NR_CGROUP];
#endif
static int boost_value[NR_CGROUP][EAS_MAX_KIR];
static int debug_boost_value[NR_CGROUP];
static int debug_fix_boost;

/* uclamp */
#if defined(CONFIG_UCLAMP_TASK_GROUP) && defined(CONFIG_SCHED_TUNE)
static int cur_uclamp_min[NR_CGROUP];
static unsigned long uclamp_policy_mask[NR_CGROUP];
#endif
static int uclamp_min[NR_CGROUP][EAS_UCLAMP_MAX_KIR];
static int debug_uclamp_min[NR_CGROUP];

static int cur_schedplus_down_throttle_ns;
static int default_schedplus_down_throttle_ns;
static unsigned long schedplus_down_throttle_ns_policy_mask;
static int schedplus_down_throttle_ns[EAS_THRES_MAX_KIR];
static int debug_schedplus_down_throttle_nsec;
static int cur_schedplus_up_throttle_ns;
static int default_schedplus_up_throttle_ns;
static unsigned long schedplus_up_throttle_ns_policy_mask;
static int schedplus_up_throttle_ns[EAS_THRES_MAX_KIR];
static int debug_schedplus_up_throttle_nsec;

static int cur_schedplus_sync_flag;
static int default_schedplus_sync_flag;
static unsigned long schedplus_sync_flag_policy_mask;
static int schedplus_sync_flag[EAS_SYNC_FLAG_MAX_KIR];
static int debug_schedplus_sync_flag;

static unsigned long prefer_idle[NR_CGROUP];
static int debug_prefer_idle[NR_CGROUP];

/* log */
static int log_enable;

static bool perf_sched_big_task_rotation;
static int  perf_sched_stune_task_thresh;

#if defined(CONFIG_CPUFREQ_HAVE_GOVERNOR_PER_POLICY)
static int cluster_num;
static int *cpu_id;
#endif

#define MAX_BOOST_VALUE	(100)
#define MIN_BOOST_VALUE	(-100)
#define MAX_UCLAMP_VALUE		(100)
#define MIN_UCLAMP_VALUE		(0)
#define MIN_DEBUG_UCLAMP_VALUE	(-1)

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
	return clamp(boost_value, MIN_BOOST_VALUE, MAX_BOOST_VALUE);
}

static int check_uclamp_value(int value)
{
	return clamp(value, MIN_UCLAMP_VALUE, MAX_UCLAMP_VALUE);
}

static int check_debug_uclamp_value(int value)
{
	return clamp(value, MIN_DEBUG_UCLAMP_VALUE, MAX_UCLAMP_VALUE);
}

/************************/
int update_schedplus_down_throttle_ns(int kicker, int nsec)
{
	int i;
	int final_down_thres = -1;

	if (kicker < 0 || kicker >= EAS_THRES_MAX_KIR) {
		pr_debug(" kicker:%d error\n", kicker);
		return -1;
	}

#if defined(CONFIG_CPUFREQ_HAVE_GOVERNOR_PER_POLICY)
	if (cpu_id == NULL) {
		pr_debug(" cpu_id is NULL\n");
		return -1;
	}
#endif

	mutex_lock(&boost_eas);

	schedplus_down_throttle_ns[kicker] = nsec;

	for (i = 0; i < EAS_THRES_MAX_KIR; i++) {
		if (schedplus_down_throttle_ns[i] == -1) {
			clear_bit(i, &schedplus_down_throttle_ns_policy_mask);
			continue;
		}

		if (final_down_thres >= 0)
			final_down_thres = MAX(final_down_thres,
				schedplus_down_throttle_ns[i]);
		else
			final_down_thres = schedplus_down_throttle_ns[i];

		set_bit(i, &schedplus_down_throttle_ns_policy_mask);
	}

	cur_schedplus_down_throttle_ns = final_down_thres < 0 ?
		-1 : final_down_thres;

#ifdef CONFIG_CPU_FREQ_GOV_SCHEDUTIL
	if (debug_schedplus_down_throttle_nsec == -1) {
#if defined(CONFIG_CPUFREQ_HAVE_GOVERNOR_PER_POLICY)
		if (cur_schedplus_down_throttle_ns >= 0)
			for (i = 0; i < cluster_num; i++)
				schedutil_set_down_rate_limit_us(cpu_id[i],
					cur_schedplus_down_throttle_ns / 1000);
		else
			for (i = 0; i < cluster_num; i++)
				schedutil_set_down_rate_limit_us(cpu_id[i],
					default_schedplus_down_throttle_ns / 1000);
#else
		if (cur_schedplus_down_throttle_ns >= 0)
			schedutil_set_down_rate_limit_us(0,
				cur_schedplus_down_throttle_ns / 1000);
		else
			schedutil_set_down_rate_limit_us(0,
				default_schedplus_down_throttle_ns / 1000);
#endif
	}
#endif

	pr_debug("%s %d %d %d %d", __func__, kicker, nsec,
		cur_schedplus_down_throttle_ns,
		debug_schedplus_down_throttle_nsec);

	mutex_unlock(&boost_eas);

	return 0;
}

int update_schedplus_up_throttle_ns(int kicker, int nsec)
{
	int i;
	int final_up_thres = -1;

	if (kicker < 0 || kicker >= EAS_THRES_MAX_KIR) {
		pr_debug(" kicker:%d error\n", kicker);
		return -1;
	}

#if defined(CONFIG_CPUFREQ_HAVE_GOVERNOR_PER_POLICY)
	if (cpu_id == NULL) {
		pr_debug(" cpu_id is NULL\n");
		return -1;
	}
#endif

	mutex_lock(&boost_eas);

	schedplus_up_throttle_ns[kicker] = nsec;

	for (i = 0; i < EAS_THRES_MAX_KIR; i++) {
		if (schedplus_up_throttle_ns[i] == -1) {
			clear_bit(i, &schedplus_up_throttle_ns_policy_mask);
			continue;
		}

		if (final_up_thres >= 0)
			final_up_thres = MAX(final_up_thres,
				schedplus_up_throttle_ns[i]);
		else
			final_up_thres = schedplus_up_throttle_ns[i];

		set_bit(i, &schedplus_up_throttle_ns_policy_mask);
	}

	cur_schedplus_up_throttle_ns = final_up_thres < 0 ?
		-1 : final_up_thres;

#ifdef CONFIG_CPU_FREQ_GOV_SCHEDUTIL
	if (debug_schedplus_up_throttle_nsec == -1) {
#if defined(CONFIG_CPUFREQ_HAVE_GOVERNOR_PER_POLICY)
		if (cur_schedplus_up_throttle_ns >= 0)
			for (i = 0; i < cluster_num; i++)
				schedutil_set_up_rate_limit_us(cpu_id[i],
					cur_schedplus_up_throttle_ns / 1000);
		else
			for (i = 0; i < cluster_num; i++)
				schedutil_set_up_rate_limit_us(cpu_id[i],
					default_schedplus_up_throttle_ns / 1000);
#else
		if (cur_schedplus_up_throttle_ns >= 0)
			schedutil_set_up_rate_limit_us(0,
				cur_schedplus_up_throttle_ns / 1000);
		else
			schedutil_set_up_rate_limit_us(0,
				default_schedplus_up_throttle_ns / 1000);
#endif
	}
#endif

	pr_debug("%s %d %d %d %d", __func__, kicker, nsec,
		cur_schedplus_up_throttle_ns,
		debug_schedplus_up_throttle_nsec);

	mutex_unlock(&boost_eas);

	return 0;
}

int update_schedplus_sync_flag(int kicker, int enable)
{
	int i;
	int final_sync_flag = -1;

	if (kicker < 0 || kicker >= EAS_SYNC_FLAG_MAX_KIR) {
		pr_debug(" kicker:%d error\n", kicker);
		return -1;
	}

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
	if (!debug_fix_boost)
		boost_write_for_perf_idx(cgroup_idx,
				current_boost_value[cgroup_idx]);

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

#if defined(CONFIG_UCLAMP_TASK_GROUP) && defined(CONFIG_SCHED_TUNE)
int update_eas_uclamp_min(int kicker, int cgroup_idx, int value)
{
	int final_uclamp = 0;
	int i, len = 0, len1 = 0;

	char msg[LOG_BUF_SIZE];
	char msg1[LOG_BUF_SIZE];

	if (cgroup_idx >= NR_CGROUP || cgroup_idx < 0) {
		pr_debug(" cgroup_idx:%d, error\n", cgroup_idx);
		perfmgr_trace_printk("uclamp_min", "cgroup_idx >= NR_CGROUP\n");
		return -1;
	}

	if (kicker < 0 || kicker >= EAS_UCLAMP_MAX_KIR) {
		pr_debug(" kicker:%d error\n", kicker);
		return -1;
	}

	mutex_lock(&boost_eas);

	uclamp_min[cgroup_idx][kicker] = value;
	len += snprintf(msg + len, sizeof(msg) - len, "[%d] [%d] [%d]",
			kicker, cgroup_idx, value);

	/* ptr return error EIO:I/O error */
	if (len < 0) {
		perfmgr_trace_printk("uclamp_min", "return -EIO 1\n");
		mutex_unlock(&boost_eas);
		return -EIO;
	}

	for (i = 0; i < EAS_UCLAMP_MAX_KIR; i++) {
		if (uclamp_min[cgroup_idx][i] == 0) {
			clear_bit(i, &uclamp_policy_mask[cgroup_idx]);
			continue;
		}

		final_uclamp = MAX(final_uclamp,
			uclamp_min[cgroup_idx][i]);

		set_bit(i, &uclamp_policy_mask[cgroup_idx]);
	}

	cur_uclamp_min[cgroup_idx] = check_uclamp_value(final_uclamp);

	len += snprintf(msg + len, sizeof(msg) - len, "{%d} ", final_uclamp);

	/*ptr return error EIO:I/O error */
	if (len < 0) {
		perfmgr_trace_printk("uclamp_min", "return -EIO 2\n");
		mutex_unlock(&boost_eas);
		return -EIO;
	}

	len1 += snprintf(msg1 + len1, sizeof(msg1) - len1, "[0x %lx] ",
			uclamp_policy_mask[cgroup_idx]);

	if (len1 < 0) {
		perfmgr_trace_printk("uclamp_min", "return -EIO 3\n");
		mutex_unlock(&boost_eas);
		return -EIO;
	}
	if (debug_uclamp_min[cgroup_idx] == -1)
		uclamp_min_pct_for_perf_idx(cgroup_idx,
				cur_uclamp_min[cgroup_idx]);

	if (strlen(msg) + strlen(msg1) < LOG_BUF_SIZE)
		strncat(msg, msg1, strlen(msg1));
	if (log_enable)
		pr_debug("%s\n", msg);

#ifdef CONFIG_TRACING
	perfmgr_trace_printk("eas_ctrl (uclamp)", msg);
#endif
	mutex_unlock(&boost_eas);

	return cur_uclamp_min[cgroup_idx];
}
#else
int update_eas_uclamp_min(int kicker, int cgroup_idx, int value)
{
	return -1;
}
#endif
EXPORT_SYMBOL(update_eas_uclamp_min);

#ifdef CONFIG_SCHED_TUNE
int update_prefer_idle_value(int kicker, int cgroup_idx, int value)
{
	if (cgroup_idx < 0 || cgroup_idx >= NR_CGROUP) {
		pr_debug("cgroup_idx:%d, error\n", cgroup_idx);
		return -EINVAL;
	}

	if (kicker < 0 || kicker >= EAS_PREFER_IDLE_MAX_KIR) {
		pr_debug("kicker:%d, error\n", kicker);
		return -EINVAL;
	}

	mutex_lock(&boost_eas);

	if (value != 0)
		set_bit(kicker, &prefer_idle[cgroup_idx]);
	else
		clear_bit(kicker, &prefer_idle[cgroup_idx]);

	if (debug_prefer_idle[cgroup_idx] == -1) {
		if (prefer_idle[cgroup_idx] > 0)
			prefer_idle_for_perf_idx(cgroup_idx, 1);
		else
			prefer_idle_for_perf_idx(cgroup_idx, 0);
	}

	mutex_unlock(&boost_eas);

	return prefer_idle[cgroup_idx];
}
#else
int update_prefer_idle_value(int kicker, int cgroup_idx, int value)
{
	return -1;
}
#endif
EXPORT_SYMBOL(update_prefer_idle_value);

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

#ifdef CONFIG_SCHED_TUNE
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

#ifdef CONFIG_SCHED_TUNE
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

#ifdef CONFIG_SCHED_TUNE
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

/****************/
/* uclamp min   */
/****************/
static ssize_t perfmgr_perfserv_uclamp_min_proc_write(struct file *filp
		, const char *ubuf, size_t cnt, loff_t *pos)
{
	int data = 0;

	int rv = check_proc_write(&data, ubuf, cnt);

	if (rv != 0)
		return rv;

	data = check_uclamp_value(data);

	update_eas_uclamp_min(EAS_UCLAMP_KIR_PERF, CGROUP_ROOT, data);

	return cnt;
}

static int perfmgr_perfserv_uclamp_min_proc_show(struct seq_file *m, void *v)
{
	seq_printf(m, "%d\n", uclamp_min[CGROUP_ROOT][EAS_UCLAMP_KIR_PERF]);

	return 0;
}

/************************************************/
static int perfmgr_current_uclamp_min_proc_show(struct seq_file *m, void *v)
{
#if defined(CONFIG_UCLAMP_TASK_GROUP) && defined(CONFIG_SCHED_TUNE)
	seq_printf(m, "%d\n", cur_uclamp_min[CGROUP_ROOT]);
#else
	seq_printf(m, "%d\n", -1);
#endif

	return 0;
}

/************************************************************/
static ssize_t perfmgr_debug_uclamp_min_proc_write(
		struct file *filp, const char *ubuf,
		size_t cnt, loff_t *pos)
{
	int data = 0;

	int rv = check_proc_write(&data, ubuf, cnt);

	if (rv != 0)
		return rv;

	debug_uclamp_min[CGROUP_ROOT] = check_debug_uclamp_value(data);

#if defined(CONFIG_UCLAMP_TASK_GROUP) && defined(CONFIG_SCHED_TUNE)
	if (debug_uclamp_min[CGROUP_ROOT] >= 0)
		uclamp_min_pct_for_perf_idx(CGROUP_ROOT,
			debug_uclamp_min[CGROUP_ROOT]);
	else
		uclamp_min_pct_for_perf_idx(CGROUP_ROOT,
			cur_uclamp_min[CGROUP_ROOT]);
#endif
	return cnt;
}

static int perfmgr_debug_uclamp_min_proc_show(struct seq_file *m, void *v)
{
	seq_printf(m, "%d\n", debug_uclamp_min[CGROUP_ROOT]);

	return 0;
}

static ssize_t perfmgr_perfserv_fg_uclamp_min_proc_write(struct file *filp
		, const char *ubuf, size_t cnt, loff_t *pos)
{
	int data = 0;

	int rv = check_proc_write(&data, ubuf, cnt);

	if (rv != 0)
		return rv;

	data = check_uclamp_value(data);

	update_eas_uclamp_min(EAS_UCLAMP_KIR_PERF, CGROUP_FG, data);

	return cnt;
}

static int perfmgr_perfserv_fg_uclamp_min_proc_show(struct seq_file *m, void *v)
{
	seq_printf(m, "%d\n", uclamp_min[CGROUP_FG][EAS_UCLAMP_KIR_PERF]);

	return 0;
}

/************************************************/
static int perfmgr_current_fg_uclamp_min_proc_show(struct seq_file *m, void *v)
{
#if defined(CONFIG_UCLAMP_TASK_GROUP) && defined(CONFIG_SCHED_TUNE)
	seq_printf(m, "%d\n", cur_uclamp_min[CGROUP_FG]);
#else
	seq_printf(m, "%d\n", -1);
#endif

	return 0;
}

/************************************************************/
static ssize_t perfmgr_debug_fg_uclamp_min_proc_write(
		struct file *filp, const char *ubuf,
		size_t cnt, loff_t *pos)
{
	int data = 0;

	int rv = check_proc_write(&data, ubuf, cnt);

	if (rv != 0)
		return rv;

	debug_uclamp_min[CGROUP_FG] = check_debug_uclamp_value(data);

#if defined(CONFIG_UCLAMP_TASK_GROUP) && defined(CONFIG_SCHED_TUNE)
	if (debug_uclamp_min[CGROUP_FG] >= 0)
		uclamp_min_pct_for_perf_idx(CGROUP_FG,
			debug_uclamp_min[CGROUP_FG]);
	else
		uclamp_min_pct_for_perf_idx(CGROUP_FG,
			cur_uclamp_min[CGROUP_FG]);
#endif
	return cnt;
}

static int perfmgr_debug_fg_uclamp_min_proc_show(struct seq_file *m, void *v)
{
	seq_printf(m, "%d\n", debug_uclamp_min[CGROUP_FG]);

	return 0;
}

static ssize_t perfmgr_perfserv_bg_uclamp_min_proc_write(struct file *filp
		, const char *ubuf, size_t cnt, loff_t *pos)
{
	int data = 0;

	int rv = check_proc_write(&data, ubuf, cnt);

	if (rv != 0)
		return rv;

	data = check_uclamp_value(data);

	update_eas_uclamp_min(EAS_UCLAMP_KIR_PERF, CGROUP_BG, data);

	return cnt;
}

static int perfmgr_perfserv_bg_uclamp_min_proc_show(struct seq_file *m, void *v)
{
	seq_printf(m, "%d\n", uclamp_min[CGROUP_BG][EAS_UCLAMP_KIR_PERF]);

	return 0;
}

/************************************************/
static int perfmgr_current_bg_uclamp_min_proc_show(struct seq_file *m, void *v)
{
#if defined(CONFIG_UCLAMP_TASK_GROUP) && defined(CONFIG_SCHED_TUNE)
	seq_printf(m, "%d\n", cur_uclamp_min[CGROUP_BG]);
#else
	seq_printf(m, "%d\n", -1);
#endif

	return 0;
}

/************************************************************/
static ssize_t perfmgr_debug_bg_uclamp_min_proc_write(
		struct file *filp, const char *ubuf,
		size_t cnt, loff_t *pos)
{
	int data = 0;

	int rv = check_proc_write(&data, ubuf, cnt);

	if (rv != 0)
		return rv;

	debug_uclamp_min[CGROUP_BG] = check_debug_uclamp_value(data);

#if defined(CONFIG_UCLAMP_TASK_GROUP) && defined(CONFIG_SCHED_TUNE)
	if (debug_uclamp_min[CGROUP_BG] >= 0)
		uclamp_min_pct_for_perf_idx(CGROUP_BG,
			debug_uclamp_min[CGROUP_BG]);
	else
		uclamp_min_pct_for_perf_idx(CGROUP_BG,
			cur_uclamp_min[CGROUP_BG]);
#endif
	return cnt;
}

static int perfmgr_debug_bg_uclamp_min_proc_show(struct seq_file *m, void *v)
{
	seq_printf(m, "%d\n", debug_uclamp_min[CGROUP_BG]);

	return 0;
}

static ssize_t perfmgr_perfserv_ta_uclamp_min_proc_write(struct file *filp
		, const char *ubuf, size_t cnt, loff_t *pos)
{
	int data = 0;

	int rv = check_proc_write(&data, ubuf, cnt);

	if (rv != 0)
		return rv;

	data = check_uclamp_value(data);

	update_eas_uclamp_min(EAS_UCLAMP_KIR_PERF, CGROUP_TA, data);

	return cnt;
}

static int perfmgr_perfserv_ta_uclamp_min_proc_show(struct seq_file *m, void *v)
{
	seq_printf(m, "%d\n", uclamp_min[CGROUP_TA][EAS_UCLAMP_KIR_PERF]);

	return 0;
}

/************************************************/
static int perfmgr_current_ta_uclamp_min_proc_show(struct seq_file *m, void *v)
{
#if defined(CONFIG_UCLAMP_TASK_GROUP) && defined(CONFIG_SCHED_TUNE)
	seq_printf(m, "%d\n", cur_uclamp_min[CGROUP_TA]);
#else
	seq_printf(m, "%d\n", -1);
#endif

	return 0;
}

/************************************************************/
static ssize_t perfmgr_debug_ta_uclamp_min_proc_write(
		struct file *filp, const char *ubuf,
		size_t cnt, loff_t *pos)
{
	int data = 0;

	int rv = check_proc_write(&data, ubuf, cnt);

	if (rv != 0)
		return rv;

	debug_uclamp_min[CGROUP_TA] = check_debug_uclamp_value(data);

#if defined(CONFIG_UCLAMP_TASK_GROUP) && defined(CONFIG_SCHED_TUNE)
	if (debug_uclamp_min[CGROUP_TA] >= 0)
		uclamp_min_pct_for_perf_idx(CGROUP_TA,
			debug_uclamp_min[CGROUP_TA]);
	else
		uclamp_min_pct_for_perf_idx(CGROUP_TA,
			cur_uclamp_min[CGROUP_TA]);
#endif
	return cnt;
}

static int perfmgr_debug_ta_uclamp_min_proc_show(struct seq_file *m, void *v)
{
	seq_printf(m, "%d\n", debug_uclamp_min[CGROUP_TA]);

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

static ssize_t perfmgr_perfserv_schedplus_down_throttle_proc_write(
	struct file *filp, const char *ubuf,
	size_t cnt, loff_t *pos)
{
	int data = 0;
	int rv = check_proc_write(&data, ubuf, cnt);

	if (rv != 0)
		return rv;

	update_schedplus_down_throttle_ns(EAS_THRES_KIR_PERF, data);

	return cnt;
}

static int perfmgr_perfserv_schedplus_down_throttle_proc_show(
	struct seq_file *m, void *v)
{
	seq_printf(m, "%d\n",
		schedplus_down_throttle_ns[EAS_THRES_KIR_PERF]);

	return 0;
}

static ssize_t perfmgr_debug_schedplus_down_throttle_proc_write(
		struct file *filp, const char *ubuf,
		size_t cnt, loff_t *pos)
{
	int data = 0;

	int rv = check_proc_write(&data, ubuf, cnt);

	if (rv != 0)
		return rv;

	mutex_lock(&boost_eas);
	debug_schedplus_down_throttle_nsec = data;

#ifdef CONFIG_CPU_FREQ_GOV_SCHEDUTIL
	if (data == -1)
		schedutil_set_down_rate_limit_us(0,
			cur_schedplus_down_throttle_ns);
	else if (data >= 0)
		schedutil_set_down_rate_limit_us(0, data / 1000);
#endif

	mutex_unlock(&boost_eas);

	return cnt;
}

static int perfmgr_debug_schedplus_down_throttle_proc_show(
	struct seq_file *m, void *v)
{
	seq_printf(m, "%d\n", debug_schedplus_down_throttle_nsec);

	return 0;
}

static ssize_t perfmgr_perfserv_schedplus_up_throttle_proc_write(
	struct file *filp, const char *ubuf,
	size_t cnt, loff_t *pos)
{
	int data = 0;
	int rv = check_proc_write(&data, ubuf, cnt);

	if (rv != 0)
		return rv;

	update_schedplus_up_throttle_ns(EAS_THRES_KIR_PERF, data);

	return cnt;
}

static int perfmgr_perfserv_schedplus_up_throttle_proc_show(
	struct seq_file *m, void *v)
{
	seq_printf(m, "%d\n",
		schedplus_up_throttle_ns[EAS_THRES_KIR_PERF]);

	return 0;
}

static ssize_t perfmgr_debug_schedplus_up_throttle_proc_write(
		struct file *filp, const char *ubuf,
		size_t cnt, loff_t *pos)
{
	int data = 0;

	int rv = check_proc_write(&data, ubuf, cnt);

	if (rv != 0)
		return rv;

	mutex_lock(&boost_eas);
	debug_schedplus_up_throttle_nsec = data;

#ifdef CONFIG_CPU_FREQ_GOV_SCHEDUTIL
	if (data == -1)
		schedutil_set_up_rate_limit_us(0,
			cur_schedplus_up_throttle_ns);
	else if (data >= 0)
		schedutil_set_up_rate_limit_us(0, data / 1000);
#endif

	mutex_unlock(&boost_eas);

	return cnt;
}

static int perfmgr_debug_schedplus_up_throttle_proc_show(
	struct seq_file *m, void *v)
{
	seq_printf(m, "%d\n", debug_schedplus_up_throttle_nsec);

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

	if (data)
		set_sched_rotation_enable(true);
	else
		set_sched_rotation_enable(false);

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
	if (perf_sched_stune_task_thresh >= 0)
		set_stune_task_threshold(perf_sched_stune_task_thresh);
	else
		set_stune_task_threshold(-1);
#endif

	return cnt;
}

static int perfmgr_sched_stune_task_thresh_proc_show(struct seq_file *m,
	void *v)
{
	seq_printf(m, "%d\n", perf_sched_stune_task_thresh);

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

static int perfmgr_current_prefer_idle_proc_show(struct seq_file *m, void *v)
{
	int i;

	for (i = 0; i < NR_CGROUP; i++)
		seq_printf(m, "%lx\n", prefer_idle[i]);

	return 0;
}

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

static ssize_t perfmgr_debug_prefer_idle_proc_write(
		struct file *filp, const char *ubuf,
		size_t cnt, loff_t *pos)
{
	int cgroup = 0, data = 0;

	int rv = check_group_proc_write(&cgroup, &data, ubuf, cnt);

	if (rv != 0)
		return rv;

	if (data < -1 || data > 1)
		return -EINVAL;

	if (cgroup >= 0 && cgroup < NR_CGROUP) {
		debug_prefer_idle[cgroup] = data;
#if defined(CONFIG_SCHED_TUNE)
		if (data == 1)
			prefer_idle_for_perf_idx(cgroup, 1);
		else if (data == 0)
			prefer_idle_for_perf_idx(cgroup, 0);
		else {
			if (prefer_idle[cgroup] > 0)
				prefer_idle_for_perf_idx(cgroup, 1);
			else
				prefer_idle_for_perf_idx(cgroup, 0);
		}
#endif
	}

	return cnt;
}

static int perfmgr_debug_prefer_idle_proc_show(struct seq_file *m, void *v)
{
	int i;

	for (i = 0; i < NR_CGROUP; i++)
		seq_printf(m, "%d\n", debug_prefer_idle[i]);

	return 0;
}

/* boost value */
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

/* uclamp */
PROC_FOPS_RW(perfserv_uclamp_min);
PROC_FOPS_RW(debug_uclamp_min);
PROC_FOPS_RO(current_uclamp_min);
PROC_FOPS_RW(perfserv_fg_uclamp_min);
PROC_FOPS_RW(debug_fg_uclamp_min);
PROC_FOPS_RO(current_fg_uclamp_min);
PROC_FOPS_RW(perfserv_bg_uclamp_min);
PROC_FOPS_RW(debug_bg_uclamp_min);
PROC_FOPS_RO(current_bg_uclamp_min);
PROC_FOPS_RW(perfserv_ta_uclamp_min);
PROC_FOPS_RW(debug_ta_uclamp_min);
PROC_FOPS_RO(current_ta_uclamp_min);

PROC_FOPS_RW(perfserv_schedplus_down_throttle);
PROC_FOPS_RW(debug_schedplus_down_throttle);
PROC_FOPS_RW(perfserv_schedplus_up_throttle);
PROC_FOPS_RW(debug_schedplus_up_throttle);
PROC_FOPS_RW(perfserv_schedplus_sync_flag);
PROC_FOPS_RW(debug_schedplus_sync_flag);
PROC_FOPS_RO(current_prefer_idle);
PROC_FOPS_RW(perfserv_prefer_idle);
PROC_FOPS_RW(debug_prefer_idle);

/* others */
PROC_FOPS_RW(perfserv_ext_launch_mon);
PROC_FOPS_RW(m_sched_migrate_cost_n);
PROC_FOPS_RW(sched_big_task_rotation);
PROC_FOPS_RW(sched_stune_task_thresh);
PROC_FOPS_RW(perfmgr_log);

/*******************************************/
int eas_ctrl_init(struct proc_dir_entry *parent)
{
	int i, ret = 0;
#if defined(CONFIG_SCHED_TUNE)
	int j;
#endif
	struct proc_dir_entry *boost_dir = NULL;

	struct pentry {
		const char *name;
		const struct file_operations *fops;
	};

	const struct pentry entries[] = {
		/* boost value */
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
		/* uclamp */
		PROC_ENTRY(perfserv_uclamp_min),
		PROC_ENTRY(debug_uclamp_min),
		PROC_ENTRY(current_uclamp_min),
		PROC_ENTRY(perfserv_fg_uclamp_min),
		PROC_ENTRY(debug_fg_uclamp_min),
		PROC_ENTRY(current_fg_uclamp_min),
		PROC_ENTRY(perfserv_bg_uclamp_min),
		PROC_ENTRY(debug_bg_uclamp_min),
		PROC_ENTRY(current_bg_uclamp_min),
		PROC_ENTRY(perfserv_ta_uclamp_min),
		PROC_ENTRY(debug_ta_uclamp_min),
		PROC_ENTRY(current_ta_uclamp_min),

		PROC_ENTRY(perfserv_schedplus_down_throttle),
		PROC_ENTRY(debug_schedplus_down_throttle),
		PROC_ENTRY(perfserv_schedplus_up_throttle),
		PROC_ENTRY(debug_schedplus_up_throttle),
		PROC_ENTRY(perfserv_schedplus_sync_flag),
		PROC_ENTRY(debug_schedplus_sync_flag),
		PROC_ENTRY(current_prefer_idle),
		PROC_ENTRY(perfserv_prefer_idle),
		PROC_ENTRY(debug_prefer_idle),

		/* log */
		PROC_ENTRY(perfmgr_log),
		/*--ext_launch--*/
		PROC_ENTRY(perfserv_ext_launch_mon),
		/*--sched migrate cost n--*/
		PROC_ENTRY(m_sched_migrate_cost_n),
		PROC_ENTRY(sched_big_task_rotation),
		PROC_ENTRY(sched_stune_task_thresh),
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

#if defined(CONFIG_SCHED_TUNE)
	/* boost value */
	for (i = 0; i < NR_CGROUP; i++) {
		current_boost_value[i] = 0;
		for (j = 0; j < EAS_MAX_KIR; j++)
			boost_value[i][j] = 0;
		prefer_idle[i] = 0;
		debug_prefer_idle[i] = -1;
	}
#endif

#if defined(CONFIG_UCLAMP_TASK_GROUP) && defined(CONFIG_SCHED_TUNE)
	/* uclamp */
	for (i = 0; i < NR_CGROUP; i++) {
		cur_uclamp_min[i] = 0;
		debug_uclamp_min[i] = -1;
		for (j = 0; j < EAS_UCLAMP_MAX_KIR; j++)
			uclamp_min[i][j] = 0;
	}
#endif

	perf_sched_big_task_rotation = 0;
	perf_sched_stune_task_thresh = -1;

	default_schedplus_down_throttle_ns = 1000000;
	default_schedplus_up_throttle_ns = 1000000;
	default_schedplus_sync_flag = 1;
	cur_schedplus_down_throttle_ns = -1;
	cur_schedplus_up_throttle_ns = -1;
	cur_schedplus_sync_flag = -1;
	debug_schedplus_down_throttle_nsec = -1;
	debug_schedplus_up_throttle_nsec = -1;
	debug_schedplus_sync_flag = -1;
	for (i = 0; i < EAS_THRES_MAX_KIR; i++) {
		schedplus_down_throttle_ns[i] = -1;
		schedplus_up_throttle_ns[i] = -1;
	}
	for (i = 0; i < EAS_SYNC_FLAG_MAX_KIR; i++)
		schedplus_sync_flag[i] = -1;

	debug_fix_boost = 0;

#if defined(CONFIG_CPUFREQ_HAVE_GOVERNOR_PER_POLICY)
	cluster_num = topo_ctrl_get_nr_clusters();
	if (cluster_num > 0)
		cpu_id = kcalloc(cluster_num, sizeof(int), GFP_KERNEL);

	if (cpu_id)
		for (i = 0; i < cluster_num; i++)
			cpu_id[i] = topo_ctrl_get_cluster_cpu_id(i);
#endif

out:
	return ret;
}
