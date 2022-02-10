// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#define pr_fmt(fmt) "[cpu_ctrl_cfp]"fmt
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/module.h>
#include <linux/string.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/cpumask.h>

#include <mt-plat/cpu_ctrl.h>
#include <mt-plat/mtk_ppm_api.h>
#include "cpu_ctrl.h"
#include "boost_ctrl.h"
#include "mtk_perfmgr_internal.h"
#include "load_track.h"

#ifdef CONFIG_TRACING
#include <linux/kallsyms.h>
#include <linux/trace_events.h>
#endif

#define MAX_NR_FREQ 16

/* Configurable */
static int __cfp_enable;
static int __cfp_polling_ms;
static int __cfp_up_opp;
static int __cfp_down_opp;
static int __cfp_up_time;
static int __cfp_down_time;
static int __cfp_up_loading;
static int __cfp_down_loading;

/* Non-Configurable */
static int cc_is_ceiled;
static struct ppm_limit_data *cc_freq, *cfp_freq;
static int cfp_curr_headroom_opp;
static int cfp_curr_up_time;
static int cfp_curr_down_time;
static int cfp_curr_loading;

static int **freq_tbl;
DEFINE_MUTEX(cfp_mlock);

static inline void cfp_lock(const char *tag)
{
	mutex_lock(&cfp_mlock);
}

static inline void cfp_unlock(const char *tag)
{
	mutex_unlock(&cfp_mlock);
}

static inline void cfp_lockprove(const char *tag)
{
	WARN_ON(!mutex_is_locked(&cfp_mlock));
}

static int cfp_get_idx_by_freq(int clu_idx, int freq)
{
	int opp_idx;

	cfp_lockprove(__func__);

	for (opp_idx = 0; opp_idx < MAX_NR_FREQ &&
		freq <= freq_tbl[clu_idx][opp_idx]; opp_idx++)
		;

	return opp_idx >= 1 ? opp_idx - 1 : 0;
}

static void set_cfp_ppm(struct ppm_limit_data *desired_freq, int headroom_opp)
{
	int clu_idx, cfp_ceiling_opp;

	cfp_lockprove(__func__);
	for_each_perfmgr_clusters(clu_idx) {
		cfp_freq[clu_idx].min = desired_freq[clu_idx].min;

		if (headroom_opp > 0) {
			cfp_ceiling_opp =
				MAX(cfp_get_idx_by_freq(clu_idx,
					desired_freq[clu_idx].max)
					- headroom_opp, 0);
			cfp_freq[clu_idx].max =
				freq_tbl[clu_idx][cfp_ceiling_opp];

		} else {
			cfp_freq[clu_idx].max = desired_freq[clu_idx].max;
		}
#ifdef CONFIG_TRACING
		perfmgr_trace_count(cfp_freq[clu_idx].min,
			"cfp_min%d", clu_idx);
		perfmgr_trace_count(cfp_freq[clu_idx].max,
			"cfp_max%d", clu_idx);
#endif
	}
#ifdef CONFIG_TRACING
	perfmgr_trace_count(cc_is_ceiled, "cfp_ceiled");
#endif
	mt_ppm_userlimit_cpu_freq(perfmgr_clusters, cfp_freq);
}

static void cfp_lt_callback(int mask_loading, int loading)
{
	cfp_lock(__func__);

	cfp_curr_loading = loading;

	if (loading > __cfp_up_loading) {
		cfp_curr_down_time = 0;
		cfp_curr_up_time =
			MIN(cfp_curr_up_time + 1, __cfp_up_time);

		if (cfp_curr_up_time >= __cfp_up_time) {
			cfp_curr_headroom_opp =
				MIN(cfp_curr_headroom_opp + __cfp_up_opp,
					MAX_NR_FREQ - 1);

			set_cfp_ppm(cc_freq, cfp_curr_headroom_opp);
		}

	} else if (loading > __cfp_down_loading) {
		cfp_curr_down_time = cfp_curr_up_time = 0;

	} else {
		cfp_curr_up_time = 0;
		cfp_curr_down_time =
			MIN(cfp_curr_down_time + 1, __cfp_down_time);

		if (cfp_curr_down_time >= __cfp_down_time) {
			cfp_curr_headroom_opp =
				MAX(cfp_curr_headroom_opp - __cfp_down_opp, 0);

			set_cfp_ppm(cc_freq, cfp_curr_headroom_opp);
		}
	}
#ifdef CONFIG_TRACING
	perfmgr_trace_count(cfp_curr_loading, "cfp_loading");
	perfmgr_trace_count(cfp_curr_headroom_opp, "cfp_headroom_opp");
	perfmgr_trace_count(cfp_curr_up_time, "cfp_up_time");
	perfmgr_trace_count(cfp_curr_down_time, "cfp_down_time");
#endif

	cfp_unlock(__func__);
}

static void start_cfp(void)
{
	int reg_ret;
	int poll_ms;

	cfp_lockprove(__func__);
	cc_is_ceiled = 1;
	poll_ms = __cfp_polling_ms;

	pr_debug("%s\n", __func__);

	cfp_unlock(__func__);
	reg_ret = reg_loading_tracking(cfp_lt_callback, poll_ms, cpu_possible_mask);
	if (reg_ret)
		pr_debug("%s reg_ret=%d\n", __func__, reg_ret);
	cfp_lock(__func__);
}

static void stop_cfp(void)
{
	int unreg_ret;

	cfp_lockprove(__func__);
	cfp_curr_headroom_opp = 0;
	cfp_curr_up_time = 0;
	cfp_curr_down_time = 0;
	cfp_curr_loading = 0;
	cc_is_ceiled = 0;

	pr_debug("%s\n", __func__);

	cfp_unlock(__func__);
	unreg_ret = unreg_loading_tracking(cfp_lt_callback);
	if (unreg_ret)
		pr_debug("%s unreg_ret=%d\n", __func__, unreg_ret);
	cfp_lock(__func__);
}

static void restart_cfp(void)
{
	pr_debug("%s\n", __func__);
	stop_cfp();
	start_cfp();
}

static int cfp_check_ceiled(struct ppm_limit_data *freq)
{
	int clu_idx;
	int ret = 0;

	cfp_lockprove(__func__);
	for_each_perfmgr_clusters(clu_idx) {
		if (freq[clu_idx].max != -1 &&
			/* FPSGO jerk max is max freq not -1.
			 * cfp treated as ceiled to avoid ping-pong
			 */
			freq[clu_idx].max <= freq_tbl[clu_idx][0]) {
			ret = 1;
			break;
		}
	}

	return ret;
}

void cpu_ctrl_cfp(struct ppm_limit_data *desired_freq)
{
	int v_cc_is_ceiled;

	cfp_lock(__func__);

	memcpy(cc_freq, desired_freq,
		perfmgr_clusters * sizeof(struct ppm_limit_data));

	if (!__cfp_enable) {
		set_cfp_ppm(desired_freq, cfp_curr_headroom_opp);
		goto out_cpu_ctrl_cfp;
	}

	v_cc_is_ceiled = cfp_check_ceiled(desired_freq);

	if (cc_is_ceiled && !v_cc_is_ceiled)
		stop_cfp();
	else if (!cc_is_ceiled && v_cc_is_ceiled)
		start_cfp();

	set_cfp_ppm(desired_freq, cfp_curr_headroom_opp);

out_cpu_ctrl_cfp:
	cfp_unlock(__func__);
}

static int set_cfp_enable(int val)
{
	int ret = 0;

	cfp_lock(__func__);
	if (__cfp_enable == !!val) {
		ret = -EINVAL;
		goto out;
	}

	__cfp_enable = !!val;

	if (cfp_check_ceiled(cc_freq)) {
		if (__cfp_enable)
			start_cfp();
		else
			stop_cfp();
	}

out:
	cfp_unlock(__func__);
	return ret;
}

static int perfmgr_cfp_curr_stat_proc_show(struct seq_file *m, void *v)
{
	cfp_lock(__func__);
	if (m) {
		seq_printf(m,
			"enable\tceiled\tloading\theadroom_opp\tup_time\tdown_time\n");
		seq_printf(m, "%d\t%d\t%d\t%d\t\t%d\t%d\n",
			__cfp_enable, cc_is_ceiled,
			cfp_curr_loading, cfp_curr_headroom_opp,
			cfp_curr_up_time, cfp_curr_down_time);
	}
	cfp_unlock(__func__);
	return 0;
}

#define SET_CFP(name, lb, ub) \
static int set_cfp_##name(int val) \
{ \
	int ret = 0; \
\
	cfp_lock(__func__); \
	if (__cfp_##name == val || val < (lb) || val > (ub)) { \
		ret = -EINVAL; \
		goto out; \
	} \
\
	__cfp_##name = val; \
\
	if (cc_is_ceiled) \
		restart_cfp(); \
\
out: \
	cfp_unlock(__func__); \
	return ret; \
\
}

#define GET_CFP(name) \
static int get_cfp_##name(void) \
{ \
	int ret; \
\
	cfp_lock(__func__); \
	ret = __cfp_##name; \
	cfp_unlock(__func__); \
\
	return ret; \
}

#define CFP_WRITE(name) \
static ssize_t perfmgr_cfp_##name##_proc_write(struct file *filp, \
		const char __user *ubuf, size_t cnt, loff_t *pos) \
{ \
	int data = 0; \
\
	int rv = check_proc_write(&data, ubuf, cnt); \
\
	if (rv != 0) \
		return rv; \
\
	if (set_cfp_##name(data)) \
		return -EINVAL; \
\
	return cnt; \
}

#define CFP_SHOW(name) \
static int perfmgr_cfp_##name##_proc_show(struct seq_file *m, void *v) \
{ \
	if (m) \
		seq_printf(m, "%d\n", get_cfp_##name()); \
	return 0; \
}

SET_CFP(polling_ms, 1, INT_MAX);
SET_CFP(up_opp, 0, MAX_NR_FREQ - 1);
SET_CFP(down_opp, 0, MAX_NR_FREQ - 1);
SET_CFP(up_time, 1, INT_MAX);
SET_CFP(down_time, 1, INT_MAX);
SET_CFP(up_loading, __cfp_down_loading, 100);
SET_CFP(down_loading, 0, __cfp_up_loading);

GET_CFP(enable);
GET_CFP(polling_ms);
GET_CFP(up_opp);
GET_CFP(down_opp);
GET_CFP(up_time);
GET_CFP(down_time);
GET_CFP(up_loading);
GET_CFP(down_loading);

CFP_WRITE(enable);
CFP_WRITE(polling_ms);
CFP_WRITE(up_opp);
CFP_WRITE(down_opp);
CFP_WRITE(up_time);
CFP_WRITE(down_time);
CFP_WRITE(up_loading);
CFP_WRITE(down_loading);

CFP_SHOW(enable);
CFP_SHOW(polling_ms);
CFP_SHOW(up_opp);
CFP_SHOW(down_opp);
CFP_SHOW(up_time);
CFP_SHOW(down_time);
CFP_SHOW(up_loading);
CFP_SHOW(down_loading);

PROC_FOPS_RW(cfp_enable);
PROC_FOPS_RW(cfp_polling_ms);
PROC_FOPS_RW(cfp_up_opp);
PROC_FOPS_RW(cfp_down_opp);
PROC_FOPS_RW(cfp_up_time);
PROC_FOPS_RW(cfp_down_time);
PROC_FOPS_RW(cfp_up_loading);
PROC_FOPS_RW(cfp_down_loading);
PROC_FOPS_RO(cfp_curr_stat);

int cpu_ctrl_cfp_init(struct proc_dir_entry *parent)
{
	int i;
	int clu_idx, opp_idx;
	int ret = 0;
	size_t idx;

	struct pentry {
		const char *name;
		const struct file_operations *fops;
	};

	const struct pentry entries[] = {
		PROC_ENTRY(cfp_enable),
		PROC_ENTRY(cfp_polling_ms),
		PROC_ENTRY(cfp_up_opp),
		PROC_ENTRY(cfp_down_opp),
		PROC_ENTRY(cfp_up_time),
		PROC_ENTRY(cfp_down_time),
		PROC_ENTRY(cfp_up_loading),
		PROC_ENTRY(cfp_down_loading),
		PROC_ENTRY(cfp_curr_stat),
	};

	for (idx = 0; idx < ARRAY_SIZE(entries); idx++) {
		if (!proc_create(entries[idx].name, 0644,
					parent, entries[idx].fops)) {
			pr_debug("%s(), create /cpu_ctrl%s failed\n",
					__func__, entries[idx].name);
			ret = -EINVAL;
			goto out_err;
		}
	}

	cc_freq = kcalloc(perfmgr_clusters,
		sizeof(struct ppm_limit_data), GFP_KERNEL);
	if (!cc_freq) {
		ret = -ENOMEM;
		goto out_err;
	}

	cfp_freq = kcalloc(perfmgr_clusters,
		sizeof(struct ppm_limit_data), GFP_KERNEL);
	if (!cfp_freq) {
		ret = -ENOMEM;
		goto out_cfp_freq_alloc_err;
	}

	freq_tbl = kcalloc(perfmgr_clusters,
		sizeof(int *), GFP_KERNEL);
	if (!freq_tbl) {
		ret = -ENOMEM;
		goto out_freq_tbl_alloc_err;
	}

	for_each_perfmgr_clusters(clu_idx) {
		freq_tbl[clu_idx] = kcalloc(MAX_NR_FREQ,
		sizeof(int), GFP_KERNEL);
		if (!freq_tbl[clu_idx]) {
			ret = -ENOMEM;
			goto out_freq_tbl_opp_alloc_err;
		}

		for (opp_idx = 0; opp_idx < MAX_NR_FREQ; opp_idx++)
			freq_tbl[clu_idx][opp_idx] =
#ifdef CONFIG_MTK_CPU_FREQ
			mt_cpufreq_get_freq_by_idx(clu_idx, opp_idx);
#else
			0;
#endif
	}

	__cfp_enable       = 1;
	__cfp_polling_ms   = 64;
	__cfp_up_opp       = 15;
	__cfp_down_opp     = 15;
	__cfp_up_time      = 1;
	__cfp_down_time    = 16;
	__cfp_up_loading   = 90;
	__cfp_down_loading = 80;

	return ret;

out_freq_tbl_opp_alloc_err:
	for (i = 0; i < clu_idx; i++)
		kfree(freq_tbl[i]);
out_freq_tbl_alloc_err:
	kfree(freq_tbl);
out_cfp_freq_alloc_err:
	kfree(cc_freq);
out_err:
	return ret;
}

void cpu_ctrl_cfp_exit(void)
{
	int clu_idx;

	stop_cfp();
	kfree(cc_freq);
	kfree(cfp_freq);

	for_each_perfmgr_clusters(clu_idx)
		kfree(freq_tbl[clu_idx]);

	kfree(freq_tbl);
}
