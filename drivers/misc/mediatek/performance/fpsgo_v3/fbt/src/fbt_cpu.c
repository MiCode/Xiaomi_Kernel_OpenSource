// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */
#include <linux/cdev.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/hardirq.h>
#include <linux/init.h>
#include <linux/kallsyms.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/poll.h>
#include <linux/proc_fs.h>
#include <linux/wait.h>
#include <linux/sched.h>
#include <linux/vmalloc.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/semaphore.h>
#include <linux/workqueue.h>
#include <linux/kthread.h>
#include <linux/sort.h>
#include <linux/string.h>
#include <asm/div64.h>
#include <linux/topology.h>
#include <linux/slab.h>
#include <linux/hrtimer.h>
#include <linux/list.h>
#include <linux/kernel.h>
#include <linux/bsearch.h>
#include <linux/sched/task.h>
#include <sched/sched.h>
#include <linux/cpufreq.h>
#include "sugov/cpufreq.h"

#include <mt-plat/fpsgo_common.h>

#include "fpsgo_usedext.h"
#include "fpsgo_base.h"
#include "fpsgo_sysfs.h"
#include "fbt_usedext.h"
#include "fbt_cpu.h"
#include "fbt_cpu_platform.h"
#include "../fstb/fstb.h"
#include "xgf.h"
#include "mini_top.h"
#include "fps_composer.h"
#include "fpsgo_cpu_policy.h"
#include "fbt_cpu_ctrl.h"
#include "fbt_cpu_cam.h"

#define GED_VSYNC_MISS_QUANTUM_NS 16666666
#define TIME_3MS  3000000
#define TIME_2MS  2000000
#define TIME_1MS  1000000
#define TARGET_UNLIMITED_FPS 240
#define TARGET_DEFAULT_FPS 60
#define FBTCPU_SEC_DIVIDER 1000000000
#define NSEC_PER_HUSEC 100000
#define TIME_MS_TO_NS  1000000ULL
#define LOADING_WEIGHT 50
#define DEFAULT_ADJUST_LOADING_HWUI_HINT 1
#define DEF_RESCUE_PERCENT 33
#define DEF_RESCUE_NS_TH 0
#define INVALID_NUM -1
#define SBE_RESCUE_MODE_UNTIL_QUEUE_END 2
#define DEFAULT_QR_T2WNT_X 0
#define DEFAULT_QR_T2WNT_Y_P 100
#define DEFAULT_QR_T2WNT_Y_N 0
#define DEFAULT_QR_HWUI_HINT 1
#define DEFAULT_ST2WNT_ADJ 0
#define DEFAULT_GCC_HWUI_HINT 1
#define DEFAULT_GCC_RESERVED_UP_QUOTA_PCT 100
#define DEFAULT_GCC_RESERVED_DOWN_QUOTA_PCT 5
#define DEFAULT_GCC_STD_FILTER 2
#define DEFAULT_GCC_WINDOW_SIZE 100
#define DEFAULT_GCC_UP_STEP 5
#define DEFAULT_GCC_DOWN_STEP 10
#define DEFAULT_GCC_FPS_MARGIN 0
#define DEFAULT_GCC_UP_SEC_PCT 25
#define DEFAULT_GCC_DOWN_SEC_PCT 100
#define DEFAULT_GCC_GPU_BOUND_LOADING 80
#define DEFAULT_GCC_GPU_BOUND_TIME 90
#define DEFAULT_GCC_CPU_UNKNOWN_SLEEP 80
#define DEFAULT_GCC_CHECK_QUOTA_TREND 1
#define DEFAULT_GCC_ENQ_BOUND_THRS 20
#define DEFAULT_GCC_ENQ_BOUND_QUOTA 6
#define DEFAULT_GCC_DEQ_BOUND_THRS 20
#define DEFAULT_GCC_DEQ_BOUND_QUOTA 6
#define DEFAULT_BLC_BOOST 0

#define MAX(a, b) (((a) > (b)) ? (a) : (b))
#define MIN(a, b) (((a) < (b)) ? (a) : (b))

#define SEQ_printf(m, x...)\
do {\
	if (m)\
		seq_printf(m, x);\
	else\
		pr_debug(x);\
} while (0)

struct fbt_cpu_dvfs_info {
	unsigned int *power;
	unsigned int *capacity_ratio;
	int num_cpu;
};

struct fbt_pid_list {
	int pid;
	struct list_head entry;
};

struct fbt_sjerk {
	int pid;
	unsigned long long identifier;
	int active_id;
	int jerking;
	struct hrtimer timer;
	struct work_struct work;
};

struct fbt_syslimit {
	int copp;	/* ceiling limit */
	int ropp;	/* rescue ceiling limit */
	int cfreq;	/* tuning ceiling */
	int rfreq;	/* tuning rescue ceiling */
};

enum FPSGO_JERK {
	FPSGO_JERK_DISAPPEAR = 0,
	FPSGO_JERK_ONLY_CEILING_WAIT_ENQ = 1,
	FPSGO_JERK_ONLY_CEILING_WAIT_DEQ = 2,
	FPSGO_JERK_NEED = 3,
	FPSGO_JERK_POSTPONE = 4,
};

enum FPSGO_TASK_POLICY {
	FPSGO_TPOLICY_NONE = 0,
	FPSGO_TPOLICY_AFFINITY,
	FPSGO_TPOLICY_AFFINITY_ADJ,
	FPSGO_TPOLICY_MAX,
};

enum FPSGO_LIMIT_POLICY {
	FPSGO_LIMIT_NONE = 0,
	FPSGO_LIMIT_CAPACITY,
	FPSGO_LIMIT_MAX,
};

enum FPSGO_CEILING_KICKER {
	FPSGO_CEILING_PROCFS = 0,
	FPSGO_CEILING_LIMIT_FREQ,
	FPSGO_CEILING_KICKER_MAX,
};

enum FPSGO_ADJ_STATE {
	FPSGO_ADJ_NONE = 0,
	FPSGO_ADJ_LITTLE,
	FPSGO_ADJ_MIDDLE,
};

enum FPSGO_HARD_LIMIT_POLICY {
	FPSGO_HARD_NONE = 0,
	FPSGO_HARD_MARGIN = 1,
	FPSGO_HARD_CEILING = 2,
	FPSGO_HARD_LIMIT = 3,
};

enum FPSGO_JERK_STAGE {
	FPSGO_JERK_INACTIVE = 0,
	FPSGO_JERK_FIRST,
	FPSGO_JERK_SBE,
	FPSGO_JERK_SECOND,
};

enum FPSGO_SCN_TYPE {
	FPSGO_SCN_CAM = 0,
	FPSGO_SCN_UX,
	FPSGO_SCN_GAME,
	FPSGO_SCN_VIDEO,
};

enum FPSGO_FILTER_STATE {
	FPSGO_FILTER_ACTIVE = 0,
	FPSGO_FILTER_WINDOWS_NOT_ENOUGH,
	FPSGO_FILTER_INACTIVE,
	FPSGO_FILTER_FRAME_ERR,
};


static struct kobject *fbt_kobj;

static int uclamp_boost_enable;
static int bhr;
static int bhr_opp;
static int bhr_opp_l;
static int isolation_limit_cap;
static int rescue_opp_f;
static int rescue_enhance_f;
static int sbe_enhance_f;
static int rescue_opp_c;
static int rescue_percent;
static int min_rescue_percent;
static int short_rescue_ns;
static int short_min_rescue_p;
static int run_time_percent;
static int deqtime_bound;
static int variance;
static int floor_bound;
static int kmin;
static int floor_opp;
static int loading_th;
static int sampling_period_MS;
static int loading_adj_cnt;
static int loading_debnc_cnt;
static int loading_time_diff;
static int adjust_loading;
static int adjust_loading_hwui_hint;
static int fps_level_range;
static int check_running;
static int boost_affinity;
static int cm_big_cap;
static int cm_tdiff;
static int rescue_second_time;
static int rescue_second_group;
static int rescue_second_copp;
static int rescue_second_enable;
static int rescue_second_enhance_f;
static int qr_enable;
static int qr_t2wnt_x;
static int qr_t2wnt_y_p;
static int qr_t2wnt_y_n;
static int qr_hwui_hint;
static int qr_filter_outlier;
static int qr_mod_frame;
static int qr_debug;
static int gcc_enable;
static int gcc_hwui_hint;
static int gcc_reserved_up_quota_pct;
static int gcc_reserved_down_quota_pct;
static int gcc_window_size;
static int gcc_std_filter;
static int gcc_up_step;
static int gcc_down_step;
static int gcc_fps_margin;
static int gcc_up_sec_pct;
static int gcc_down_sec_pct;
static int gcc_gpu_bound_time;
static int gcc_cpu_unknown_sleep;
static int gcc_check_quota_trend;
static int gcc_upper_clamp;
static int gcc_enq_bound_thrs;
static int gcc_enq_bound_quota;
static int gcc_deq_bound_thrs;
static int gcc_deq_bound_quota;
static int gcc_positive_clamp;
static int boost_LR;
static int aa_retarget;
static int sbe_rescue_enable;
static int loading_ignore_enable;
static int loading_enable;
static int filter_frame_enable;
static int filter_frame_window_size;
static int filter_frame_kmin;
static int variance_control_enable;

module_param(bhr, int, 0644);
module_param(bhr_opp, int, 0644);
module_param(bhr_opp_l, int, 0644);
module_param(isolation_limit_cap, int, 0644);
module_param(rescue_opp_f, int, 0644);
module_param(rescue_enhance_f, int, 0644);
module_param(sbe_enhance_f, int, 0644);
module_param(rescue_opp_c, int, 0644);
module_param(rescue_percent, int, 0644);
module_param(min_rescue_percent, int, 0644);
module_param(short_rescue_ns, int, 0644);
module_param(short_min_rescue_p, int, 0644);
module_param(run_time_percent, int, 0644);
module_param(deqtime_bound, int, 0644);
module_param(variance, int, 0644);
module_param(floor_bound, int, 0644);
module_param(kmin, int, 0644);
module_param(floor_opp, int, 0644);
module_param(loading_th, int, 0644);
module_param(sampling_period_MS, int, 0644);
module_param(loading_adj_cnt, int, 0644);
module_param(loading_debnc_cnt, int, 0644);
module_param(loading_time_diff, int, 0644);
module_param(adjust_loading, int, 0644);
module_param(adjust_loading_hwui_hint, int, 0644);
module_param(fps_level_range, int, 0644);
module_param(check_running, int, 0644);
module_param(boost_affinity, int, 0644);
module_param(cm_big_cap, int, 0644);
module_param(cm_tdiff, int, 0644);
module_param(rescue_second_time, int, 0644);
module_param(rescue_second_group, int, 0644);
module_param(rescue_second_enable, int, 0644);
module_param(rescue_second_enhance_f, int, 0644);
module_param(qr_enable, int, 0644);
module_param(qr_t2wnt_x, int, 0644);
module_param(qr_t2wnt_y_p, int, 0644);
module_param(qr_t2wnt_y_n, int, 0644);
module_param(qr_hwui_hint, int, 0644);
module_param(qr_filter_outlier, int, 0644);
module_param(qr_mod_frame, int, 0644);
module_param(qr_debug, int, 0644);
module_param(gcc_enable, int, 0644);
module_param(gcc_hwui_hint, int, 0644);
module_param(gcc_reserved_up_quota_pct, int, 0644);
module_param(gcc_reserved_down_quota_pct, int, 0644);
module_param(gcc_window_size, int, 0644);
module_param(gcc_std_filter, int, 0644);
module_param(gcc_up_step, int, 0644);
module_param(gcc_down_step, int, 0644);
module_param(gcc_fps_margin, int, 0644);
module_param(gcc_up_sec_pct, int, 0644);
module_param(gcc_down_sec_pct, int, 0644);
module_param(gcc_gpu_bound_time, int, 0644);
module_param(gcc_cpu_unknown_sleep, int, 0644);
module_param(gcc_check_quota_trend, int, 0644);
module_param(gcc_upper_clamp, int, 0644);
module_param(gcc_enq_bound_thrs, int, 0644);
module_param(gcc_enq_bound_quota, int, 0644);
module_param(gcc_deq_bound_thrs, int, 0644);
module_param(gcc_deq_bound_quota, int, 0644);
module_param(gcc_positive_clamp, int, 0644);
module_param(boost_LR, int, 0644);
module_param(aa_retarget, int, 0644);
module_param(loading_ignore_enable, int, 0644);
module_param(loading_enable, int, 0644);
module_param(variance_control_enable, int, 0644);

static DEFINE_SPINLOCK(freq_slock);
static DEFINE_MUTEX(fbt_mlock);
static DEFINE_SPINLOCK(loading_slock);
static DEFINE_MUTEX(blc_mlock);
static DEFINE_MUTEX(cam_dep_lock);

static struct list_head blc_list;

static int fbt_enable;
static int fbt_idleprefer_enable;
static int set_idleprefer;
static int suppress_ceiling;
static int boost_ta;
static int down_throttle_ns;
static int fbt_down_throttle_enable;
static int ultra_rescue;
static int loading_policy;
static int llf_task_policy;
static int enable_ceiling;
static int separate_aa;
static int separate_pct_b;
static int separate_pct_m;
static int separate_release_sec;
static int blc_boost;

static int cluster_num;
static int nr_freq_cpu;
static unsigned int cpu_max_freq;
static struct fbt_cpu_dvfs_info *cpu_dvfs;
static int max_cap_cluster, min_cap_cluster, sec_cap_cluster;
static int max_cl_core_num;

static int limit_policy;
static struct fbt_syslimit *limit_clus_ceil;
static int limit_cap;
static int limit_rcap;
static int limit_uclamp;
static int limit_ruclamp;
static int limit_uclamp_m;
static int limit_ruclamp_m;
static int limit_cpu;

static int *clus_max_cap;

static unsigned int *base_opp;
static unsigned int max_blc;
static int max_blc_pid;
static unsigned long long  max_blc_buffer_id;
static int max_blc_stage;
static unsigned int max_blc_cur;
static struct fpsgo_loading max_blc_dep[MAX_DEP_NUM];
static int max_blc_dep_num;
static int boosted_group;

static unsigned int *clus_obv;
static unsigned int *clus_status;
static unsigned int last_obv;

static unsigned long long vsync_time;
static unsigned long long vsync_duration_us_90;
static unsigned long long vsync_duration_us_60;
static unsigned long long vsync_duration_us_120;
static unsigned long long vsync_duration_us_144;

static int vsync_period;
static int _gdfrc_fps_limit;

static struct fbt_sjerk sjerk;

static struct workqueue_struct *wq_jerk;

/* for recording CPU capacity when CPU freq. changed cb is called. */
static int lastest_ts[LOADING_CNT];
static int prev_cb_ts[LOADING_CNT];
static unsigned int lastest_obv[LOADING_CNT];
static unsigned int *lastest_obv_cl[LOADING_CNT];
static unsigned int *lastest_is_cl_isolated[LOADING_CNT];
static unsigned int lastest_idx;
static int last_cb_ts;

static unsigned long long sbe_rescuing_frame_id;

static struct rb_root cam_dep_thread_tree;

static int nsec_to_100usec(unsigned long long nsec)
{
	unsigned long long husec;

	husec = div64_u64(nsec, (unsigned long long)NSEC_PER_HUSEC);

	return (int)husec;
}

static unsigned long long nsec_to_usec(unsigned long long nsec)
{
	unsigned long long usec;

	usec = div64_u64(nsec, (unsigned long long)NSEC_PER_USEC);

	return usec;
}

int fbt_cpu_set_bhr(int new_bhr)
{
	if (new_bhr < 0 || new_bhr > 100)
		return -EINVAL;

	mutex_lock(&fbt_mlock);
	bhr = new_bhr;
	mutex_unlock(&fbt_mlock);

	return 0;
}

int fbt_cpu_get_bhr(void)
{
	return bhr;
}

int fbt_cpu_set_bhr_opp(int new_opp)
{
	if (new_opp < 0)
		return -EINVAL;

	mutex_lock(&fbt_mlock);
	bhr_opp = new_opp;
	mutex_unlock(&fbt_mlock);

	return 0;
}

int fbt_cpu_get_bhr_opp(void)
{
	return bhr_opp;
}

int fbt_cpu_set_rescue_opp_c(int new_opp)
{
	if (new_opp < 0)
		return -EINVAL;

	mutex_lock(&fbt_mlock);
	rescue_opp_c = new_opp;
	mutex_unlock(&fbt_mlock);

	return 0;
}

int fbt_cpu_get_rescue_opp_c(void)
{
	return rescue_opp_c;
}

int fbt_cpu_set_rescue_opp_f(int new_opp)
{
	if (new_opp < 0)
		return -EINVAL;

	mutex_lock(&fbt_mlock);
	rescue_opp_f = new_opp;
	mutex_unlock(&fbt_mlock);

	return 0;
}

int fbt_cpu_set_rescue_percent(int percent)
{
	if (percent < 0 || percent > 100)
		return -EINVAL;

	mutex_lock(&fbt_mlock);
	rescue_percent = percent;
	mutex_unlock(&fbt_mlock);

	return 0;
}

int fbt_cpu_set_min_rescue_percent(int percent)
{
	if (percent < 0 || percent > 100)
		return -EINVAL;

	mutex_lock(&fbt_mlock);
	min_rescue_percent = percent;
	mutex_unlock(&fbt_mlock);

	return 0;
}

int fbt_cpu_set_run_time_percent(int percent)
{
	if (percent < 0 || percent > 100)
		return -EINVAL;

	mutex_lock(&fbt_mlock);
	run_time_percent = percent;
	mutex_unlock(&fbt_mlock);

	return 0;
}

int fbt_cpu_set_short_rescue_ns(int value)
{
	mutex_lock(&fbt_mlock);
	short_rescue_ns = value;
	mutex_unlock(&fbt_mlock);

	return 0;
}

int fbt_cpu_set_short_min_rescue_p(int percent)
{
	if (percent < 0 || percent > 100)
		return -EINVAL;

	mutex_lock(&fbt_mlock);
	short_min_rescue_p = percent;
	mutex_unlock(&fbt_mlock);

	return 0;
}

int fbt_cpu_set_variance(int var)
{
	if (var < 0 || var > 100)
		return -EINVAL;

	mutex_lock(&fbt_mlock);
	variance = var;
	mutex_unlock(&fbt_mlock);

	return 0;
}

int fbt_cpu_set_floor_bound(int bound)
{
	if (bound < 0 || bound > WINDOW)
		return -EINVAL;

	mutex_lock(&fbt_mlock);
	floor_bound = bound;
	mutex_unlock(&fbt_mlock);

	return 0;
}

int fbt_cpu_set_floor_kmin(int k)
{
	if (k < 0 || k > WINDOW)
		return -EINVAL;

	mutex_lock(&fbt_mlock);
	kmin = k;
	mutex_unlock(&fbt_mlock);

	return 0;
}

int fbt_cpu_set_floor_opp(int new_opp)
{
	if (new_opp < 0)
		return -EINVAL;

	mutex_lock(&fbt_mlock);
	floor_opp = new_opp;
	mutex_unlock(&fbt_mlock);

	return 0;
}

static int fbt_is_enable(void)
{
	int enable;

	mutex_lock(&fbt_mlock);
	enable = fbt_enable;
	mutex_unlock(&fbt_mlock);

	return enable;
}

static struct fbt_thread_blc *fbt_list_blc_add(int pid,
	unsigned long long buffer_id)
{
	struct fbt_thread_blc *obj;

	obj = kzalloc(sizeof(struct fbt_thread_blc), GFP_KERNEL);
	if (!obj) {
		FPSGO_LOGE("ERROR OOM\n");
		return NULL;
	}

	INIT_LIST_HEAD(&obj->entry);
	obj->blc = 0;
	obj->blc_b = 0;
	obj->blc_m = 0;
	obj->pid = pid;
	obj->buffer_id = buffer_id;
	obj->dep_num = 0;

	mutex_lock(&blc_mlock);
	list_add(&obj->entry, &blc_list);
	mutex_unlock(&blc_mlock);

	return obj;
}

static void fbt_find_max_blc(unsigned int *temp_blc, int *temp_blc_pid,
	unsigned long long *temp_blc_buffer_id,
	int *temp_blc_dep_num, struct fpsgo_loading temp_blc_dep[])
{
	struct fbt_thread_blc *pos, *next;

	*temp_blc = 0;
	*temp_blc_pid = 0;
	*temp_blc_buffer_id = 0;
	*temp_blc_dep_num = 0;

	mutex_lock(&blc_mlock);
	list_for_each_entry_safe(pos, next, &blc_list, entry) {
		if (pos->blc > *temp_blc) {
			*temp_blc = pos->blc;
			*temp_blc_pid = pos->pid;
			*temp_blc_buffer_id = pos->buffer_id;
			*temp_blc_dep_num = pos->dep_num;
			memcpy(temp_blc_dep, pos->dep,
				pos->dep_num * sizeof(struct fpsgo_loading));
		}
	}
	mutex_unlock(&blc_mlock);
}

static void fbt_find_ex_max_blc(int pid, unsigned long long buffer_id,
	unsigned int *temp_blc, int *temp_blc_pid,
	unsigned long long *temp_blc_buffer_id,
	int *temp_blc_dep_num, struct fpsgo_loading temp_blc_dep[])
{
	struct fbt_thread_blc *pos, *next;

	*temp_blc = 0;
	*temp_blc_pid = 0;
	*temp_blc_buffer_id = 0;
	*temp_blc_dep_num = 0;

	mutex_lock(&blc_mlock);
	list_for_each_entry_safe(pos, next, &blc_list, entry) {
		if (pos->blc > *temp_blc &&
			(pid != pos->pid || buffer_id != pos->buffer_id)) {
			*temp_blc = pos->blc;
			*temp_blc_pid = pos->pid;
			*temp_blc_buffer_id = pos->buffer_id;
			*temp_blc_dep_num = pos->dep_num;
			memcpy(temp_blc_dep, pos->dep,
				pos->dep_num * sizeof(struct fpsgo_loading));
		}
	}
	mutex_unlock(&blc_mlock);
}

static int fbt_is_cl_isolated(int cluster)
{
	int i;
	int first_cpu = 0;
	int nr_cpus = num_possible_cpus();

	if (cluster < 0 || cluster >= cluster_num)
		return 0;

	for (i = 0; i < cluster; i++)
		first_cpu += cpu_dvfs[i].num_cpu;

	for (i = 0; i < cpu_dvfs[cluster].num_cpu; i++) {
		int cpu = first_cpu + i;

		if (cpu >= nr_cpus || cpu < 0)
			continue;

		if (cpu_online(cpu) && cpu_active(cpu))
			return 0;
	}

	return 1;
}

static void fbt_set_idleprefer_locked(int enable)
{
	if (!fbt_idleprefer_enable)
		return;

	if (set_idleprefer == enable)
		return;

	xgf_trace("fpsgo %s idleprefer", enable?"enable":"disable");
	fpsgo_sentcmd(FPSGO_SET_IDLE_PREFER, enable, -1);
	set_idleprefer = enable;
}

static void fbt_set_down_throttle_locked(int nsec)
{
	if (!fbt_down_throttle_enable)
		return;

	if (down_throttle_ns == nsec)
		return;

	xgf_trace("fpsgo set sched_rate_ns %d", nsec);
	fpsgo_sentcmd(FPSGO_SET_SCHED_RATE, nsec, -1);
	down_throttle_ns = nsec;
}

static void fbt_set_ultra_rescue_locked(int input)
{
	if (ultra_rescue == input)
		return;

	if (ultra_rescue && !input)
		fbt_boost_dram(0);

	ultra_rescue = input;

	xgf_trace("fpsgo set ultra_rescue %d", input);
}

void fbt_set_ceiling(struct cpu_ctrl_data *pld,
			int pid, unsigned long long buffer_id)
{
	int i;
	struct cpu_ctrl_data *pld_release;

	if (!pld)
		return;

	if (enable_ceiling) {
		update_userlimit_cpu_freq(CPU_KIR_FPSGO, cluster_num, pld);

		for (i = 0 ; i < cluster_num; i++)
			fpsgo_systrace_c_fbt(pid, buffer_id, pld[i].max,
					"cluster%d ceiling_freq", i);
	} else {
		pld_release =
			kcalloc(cluster_num, sizeof(struct cpu_ctrl_data), GFP_KERNEL);

		if (!pld_release)
			return;

		for (i = 0; i < cluster_num; i++) {
			pld_release[i].max = -1;
			pld_release[i].min = -1;
		}

		update_userlimit_cpu_freq(CPU_KIR_FPSGO, cluster_num, pld_release);

		for (i = 0 ; i < cluster_num; i++)
			fpsgo_systrace_c_fbt(pid, buffer_id, -2,
					"cluster%d ceiling_freq", i);

		kfree(pld_release);
	}

}

static void fbt_limit_ceiling_locked(struct cpu_ctrl_data *pld, int is_rescue)
{
	int opp, cluster;

	if (!pld || !limit_clus_ceil)
		return;

	for (cluster = 0; cluster < cluster_num; cluster++) {
		struct fbt_syslimit *limit = &limit_clus_ceil[cluster];

		opp = (is_rescue) ? limit->ropp : limit->copp;

		if (opp <= 0 || opp >= nr_freq_cpu)
			continue;

		pld[cluster].max = MIN(cpu_dvfs[cluster].power[opp],
					pld[cluster].max);
	}
}

static void fbt_set_hard_limit_locked(int input, struct cpu_ctrl_data *pld)
{
	int set_margin, set_ceiling, is_rescue;

	if (limit_policy == FPSGO_LIMIT_NONE)
		return;

	fpsgo_main_trace("limit: %d", input);

	set_margin = input & 1;
	set_ceiling = (input & 2) >> 1;
	is_rescue = !set_margin;

	if (cluster_num == 1 || bhr_opp == (nr_freq_cpu - 1)
		|| (pld && pld[max_cap_cluster].max == -1)) {
		set_ceiling = 0;
		set_margin = 0;
	}

	if (set_ceiling)
		fbt_limit_ceiling_locked(pld, is_rescue);
}

static int fbt_limit_capacity_isolation(int blc_wt, int *max_cap_isolation)
{
	int max_cap = 0;

	if (isolation_limit_cap && fbt_is_cl_isolated(max_cap_cluster))
		*max_cap_isolation = max_cap =
			cpu_dvfs[sec_cap_cluster].capacity_ratio[0];
	else
		*max_cap_isolation = max_cap = 100;

	if (max_cap <= 0)
		return blc_wt;

	return MIN(blc_wt, max_cap);
}

int fbt_limit_capacity(int blc_wt, int is_rescue)
{
	int max_cap = 0;

	if (limit_policy == FPSGO_LIMIT_NONE)
		return blc_wt;

	max_cap = (is_rescue) ? limit_rcap : limit_cap;

	if (max_cap <= 0)
		return blc_wt;

	return MIN(blc_wt, max_cap);
}

static int fbt_get_limit_capacity(int is_rescue)
{
	int max_cap = 100, max_isolation_cap = 100;

	if (limit_policy != FPSGO_LIMIT_NONE) {
		max_cap = (is_rescue) ? limit_rcap : limit_cap;
		if (max_cap <= 0)
			max_cap = 100;
	}

	if (isolation_limit_cap && fbt_is_cl_isolated(max_cap_cluster)) {
		max_isolation_cap =
			cpu_dvfs[sec_cap_cluster].capacity_ratio[0];
		if (max_isolation_cap <= 0)
			max_isolation_cap = 100;

		max_cap = max_isolation_cap < max_cap ?
					max_isolation_cap : max_cap;
	}

	return max_cap;
}

static void fbt_free_bhr(void)
{
	struct cpu_ctrl_data *pld;
	int i;

	pld = kcalloc(cluster_num, sizeof(struct cpu_ctrl_data), GFP_KERNEL);
	if (!pld) {
		FPSGO_LOGE("ERROR OOM %d\n", __LINE__);
		return;
	}

	for (i = 0; i < cluster_num; i++) {
		pld[i].max = -1;
		pld[i].min = -1;
	}

	xgf_trace("fpsgo free bhr");

	fbt_set_hard_limit_locked(FPSGO_HARD_NONE, pld);
	fbt_set_ceiling(pld, 0, 0);

	kfree(pld);
}

static int fbt_set_priority(int pid, long nice)
{
	struct task_struct *tsk;
	int ori_nice = -1;

	rcu_read_lock();

	tsk = find_task_by_vpid(pid);
	if (!tsk)
		goto EXIT;

	get_task_struct(tsk);

	ori_nice = task_nice(tsk);

	set_user_nice(tsk, nice);

	put_task_struct(tsk);

EXIT:
	rcu_read_unlock();

	fpsgo_systrace_c_fbt(pid, 0, nice, "nice");

	return ori_nice;
}

static void fbt_set_task_policy(struct fpsgo_loading *fl,
			int policy, unsigned int prefer_type, int set_nice)
{
	int ori_nice = -1;
	int cur_nice = MIN_NICE;

	if (!fl || !fl->pid)
		return;

	/* policy changes, reset */
	if (fl->policy != policy && fl->prefer_type != FPSGO_PREFER_NONE)
		fbt_set_task_policy(fl, fl->policy, FPSGO_PREFER_NONE, 0);

	if (set_nice) {
		ori_nice = fbt_set_priority(fl->pid, cur_nice);
		if (ori_nice != -1) {
			int orig_bk = (fl->nice_bk) >> 1;

			if (!fl->nice_bk ||
				(ori_nice != orig_bk && ori_nice != cur_nice))
				fl->nice_bk = (ori_nice << 1) | 1;
		}
	} else if (fl->nice_bk) {
		fbt_set_priority(fl->pid, (fl->nice_bk) >> 1);
		fl->nice_bk = 0;
	}

	switch (policy) {
	case FPSGO_TPOLICY_AFFINITY:
		fbt_set_affinity(fl->pid, prefer_type);
		break;
	case FPSGO_TPOLICY_AFFINITY_ADJ:
		break;
	case FPSGO_TPOLICY_NONE:
	default:
		break;
	}

	fl->prefer_type = prefer_type;
	fl->policy = policy;
	fpsgo_systrace_c_fbt_debug(fl->pid, 0, prefer_type, "prefer_type");
	fpsgo_systrace_c_fbt_debug(fl->pid, 0, policy, "task_policy");
}

#define MAX_PID_DIGIT 7
#define MAIN_LOG_SIZE (256)
static void print_dep(const char *func,
	char *tag, int pid,
	unsigned long long buffer_id,
	struct fpsgo_loading dep[], int size)
{
	char *dep_str = NULL;
	char temp[MAX_PID_DIGIT] = {"\0"};
	struct fpsgo_loading *fl;
	int i;

	if (!xgf_trace_enable)
		return;

	dep_str = kcalloc(size + 1, MAX_PID_DIGIT * sizeof(char),
				GFP_KERNEL);
	if (!dep_str)
		return;

	for (i = 0; i < size; i++) {
		fl = &dep[i];

		if (strlen(dep_str) == 0)
			snprintf(temp, sizeof(temp), "%d", fl->pid);
		else
			snprintf(temp, sizeof(temp), ",%d", fl->pid);

		if (strlen(dep_str) + strlen(temp) < MAIN_LOG_SIZE)
			strncat(dep_str, temp, strlen(temp));
	}
	xgf_trace("%s %s %s %d %d size:%d dep-list %s",
			__func__, func, tag, pid, buffer_id, size, dep_str);
	kfree(dep_str);
}

/*
 * __incr- Given an array ARR with size @max, while index @i reaches @max,
 *         return @max and keeping @fl points to last valid element of
 *         ARR[max - 1]. Otherwise, do nomral increment of @i and @fl.
 */
static inline int __incr(int i, int max)
{
	if (i >= max)
		return max;

	return i + 1;
}

/*
 * __incr_alt - if @t reaches maximum already, raise @incr_c as candidate
 */
static inline void __incr_alt(int t, int max, int *incr_t, int *incr_c)
{
	if (t < max)
		*incr_t = 1;
	else
		*incr_c = 1;
}

static int cmpint(const void *a, const void *b)
{
	return *(int *)a - *(int *)b;
}

/* compare function for fbt_loading_info */
static int cmp_loading(const void *a, const void *b)
{
	long l = ((struct fbt_loading_info *)a)->loading;
	long r = ((struct fbt_loading_info *)b)->loading;

	return (l - r);
}

/* compare function for fpsgo_loading */
static int cmp_loading_desc(const void *a, const void *b)
{
	long l = ((struct fpsgo_loading *)a)->loading;
	long r = ((struct fpsgo_loading *)b)->loading;

	return (r - l);
}

static void dep_a_except_b(
	struct fpsgo_loading dep_a[], int size_a,
	struct fpsgo_loading dep_b[], int size_b,
	struct fpsgo_loading dep_result[], int *size_result,
	int copy_intersection_to_b)
{
	struct fpsgo_loading *fl_b, *fl_a;
	int i, j;
	int incr_i, incr_j;
	int temp_size_result = 0;
	int index_a = 0, index_b = 0;

	for (i = 0; i < size_a; i++)
		dep_a[i].rmidx = 0;

	if (!size_b) {
		memcpy(dep_result, dep_a,
			size_a * sizeof(struct fpsgo_loading));
		*size_result = size_a;
		return;
	}
	if (!size_a) {
		*size_result = 0;
		return;
	}

	for (i = 0, j = 0, fl_b = &dep_b[0], fl_a = &(dep_a[0]);
	     size_b > 0 && size_a &&
	     (i < size_b || j < size_a);
	     i = incr_i ? __incr(i, size_b) : i,
	     j = incr_j ? __incr(j, size_a) : j,
	     index_a = clamp(j, j, size_a - 1),
	     index_b = clamp(i, i, size_b - 1),
	     fl_b = &dep_b[index_b],
	     fl_a = &(dep_a[index_a])) {

		incr_i = incr_j = 0;

		if (fl_b->pid == 0) {
			if (i >= size_b && j < size_a) {
				dep_result[temp_size_result] = *fl_a;
				temp_size_result++;
			}
			__incr_alt(i, size_b, &incr_i, &incr_j);
			continue;
		}

		if (fl_a->pid == 0) {
			__incr_alt(j, size_a, &incr_j, &incr_i);
			continue;
		}

		if (fl_b->pid == fl_a->pid) {
			if (copy_intersection_to_b)
				*fl_b = *fl_a;
			incr_i = incr_j = 1;
			fl_a->rmidx = 1;
		} else if (fl_b->pid > fl_a->pid) {
			if (j < size_a) {
				dep_result[temp_size_result] = *fl_a;
				temp_size_result++;
			}
			__incr_alt(j, size_a, &incr_j, &incr_i);
		} else { /* b pid < a pid */
			if (i >= size_b && j < size_a) {
				dep_result[temp_size_result] = *fl_a;
				temp_size_result++;
			}
			__incr_alt(i, size_b, &incr_i, &incr_j);
		}
	}

	*size_result = temp_size_result;
}


static void fbt_reset_task_setting(struct fpsgo_loading *fl, int reset_boost)
{
	if (!fl || !fl->pid)
		return;

	if (reset_boost)
		fbt_set_per_task_cap(fl->pid, 0, 100);

	fbt_set_task_policy(fl, FPSGO_TPOLICY_NONE, FPSGO_PREFER_NONE, 0);
}

static void fbt_dep_list_filter(struct fpsgo_loading *arr, int size)
{
	struct task_struct *tsk;
	int i;

	rcu_read_lock();

	for (i = 0; i < size; i++) {
		tsk = find_task_by_vpid(arr[i].pid);
		if (!tsk) {
			arr[i].pid = 0;
			continue;
		}

		get_task_struct(tsk);
		if ((tsk->flags & PF_KTHREAD) || rt_task(tsk) || dl_task(tsk))
			arr[i].pid = 0;
		put_task_struct(tsk);
	}

	rcu_read_unlock();
}

static int __cmp1(const void *a, const void *b)
{
	return (((struct fpsgo_loading *)a)->pid)
		- (((struct fpsgo_loading *)b)->pid);
}

static void fbt_query_dep_list_loading(struct render_info *thr)
{
	if (!thr)
		return;

	if (!thr->dep_valid_size || !thr->dep_arr)
		return;

	if (thr->t_enqueue_start <= thr->dep_loading_ts
		|| (thr->dep_loading_ts
		&& thr->t_enqueue_start - thr->dep_loading_ts <
		(unsigned long long)sampling_period_MS * TIME_MS_TO_NS))
		return;

	thr->dep_loading_ts = thr->t_enqueue_start;
	fpsgo_fbt2minitop_query(thr->dep_valid_size, thr->dep_arr);
	fpsgo_fbt2minitop_start(thr->dep_valid_size, thr->dep_arr);
}

static int fbt_get_dep_list(struct render_info *thr)
{
	int pid;
	int count = 0;
	int ret_size;
	struct fpsgo_loading *dep_new, *dep_only_old, *dep_old_need_reset;
	int ret = 0;

	int i;
	int temp_size_only_old = 0, temp_size_old_need_reset = 0;

	dep_new =
		kcalloc(MAX_DEP_NUM, sizeof(struct fpsgo_loading), GFP_KERNEL);
	dep_only_old =
		kcalloc(MAX_DEP_NUM, sizeof(struct fpsgo_loading), GFP_KERNEL);
	dep_old_need_reset =
		kcalloc(MAX_DEP_NUM, sizeof(struct fpsgo_loading), GFP_KERNEL);
	if (!dep_new || !dep_only_old || !dep_old_need_reset) {
		ret = 6;
		FPSGO_LOGE("ERROR OOM %d\n", __LINE__);
		goto EXIT;
	}

	memset(dep_new, 0,
		MAX_DEP_NUM * sizeof(struct fpsgo_loading));

	if (!thr) {
		ret = 1;
		goto EXIT;
	}

	pid = thr->pid;
	if (!pid) {
		ret = 2;
		goto EXIT;
	}

	count = fpsgo_fbt2xgf_get_dep_list_num(pid, thr->buffer_id);
	if (count <= 0) {
		ret = 3;
		goto EXIT;
	}

	count = clamp(count, 1, MAX_DEP_NUM);

	ret_size = fpsgo_fbt2xgf_get_dep_list(pid, count,
		dep_new, thr->buffer_id);

	if (ret_size == 0 || ret_size != count) {
		ret = 4;
		goto EXIT;
	}

	fbt_dep_list_filter(dep_new, count);
	sort(dep_new, count, sizeof(struct fpsgo_loading), __cmp1, NULL);

	dep_a_except_b(
		&(thr->dep_arr[0]), thr->dep_valid_size,
		&dep_new[0], count,
		&dep_only_old[0], &temp_size_only_old,
		1);
	print_dep(__func__, "old_dep",
		thr->pid, thr->buffer_id,
		&(thr->dep_arr[0]), thr->dep_valid_size);
	print_dep(__func__, "new_dep",
		thr->pid, thr->buffer_id,
		&dep_new[0], count);
	print_dep(__func__, "only_in_old_dep",
		thr->pid, thr->buffer_id,
		&dep_only_old[0], temp_size_only_old);

	if (thr->pid == max_blc_pid && thr->buffer_id == max_blc_buffer_id)
		for (i = 0; i < temp_size_only_old; i++)
			fbt_reset_task_setting(&dep_only_old[i], 1);
	else {
		dep_a_except_b(
			&(dep_only_old[0]), temp_size_only_old,
			&max_blc_dep[0], max_blc_dep_num,
			&dep_old_need_reset[0], &temp_size_old_need_reset,
			0);
		print_dep(__func__, "dep_need_reset",
			thr->pid, thr->buffer_id,
			&dep_old_need_reset[0], temp_size_old_need_reset);
		for (i = 0; i < temp_size_old_need_reset; i++)
			fbt_reset_task_setting(&dep_old_need_reset[i], 1);
	}

	if (!thr->dep_arr) {
		thr->dep_arr = (struct fpsgo_loading *)
			fpsgo_alloc_atomic(MAX_DEP_NUM *
				sizeof(struct fpsgo_loading));
		if (thr->dep_arr == NULL) {
			thr->dep_valid_size = 0;
			ret = 5;
			goto EXIT;
		}
	}

	thr->dep_valid_size = count;
	memset(thr->dep_arr, 0,
		MAX_DEP_NUM * sizeof(struct fpsgo_loading));
	memcpy(thr->dep_arr, dep_new,
		thr->dep_valid_size * sizeof(struct fpsgo_loading));

EXIT:
	kfree(dep_old_need_reset);
	kfree(dep_only_old);
	kfree(dep_new);
	return ret;
}

static int fbt_get_cam_dep_list(struct fpsgo_loading *dep_arr)
{
	int index = 0;
	struct cam_dep_thread *iter;
	struct fpsgo_loading *cam_dep_list_local;
	struct rb_root *rbr;
	struct rb_node *rbn;

	cam_dep_list_local =
		kcalloc(MAX_DEP_NUM, sizeof(struct fpsgo_loading), GFP_KERNEL);
	if (!cam_dep_list_local)
		return -ENOMEM;

	mutex_lock(&cam_dep_lock);
	rbr = &cam_dep_thread_tree;
	for (rbn = rb_first(rbr); rbn; rbn = rb_next(rbn)) {
		iter = rb_entry(rbn, struct cam_dep_thread, rb_node);
		if (index < MAX_DEP_NUM) {
			cam_dep_list_local[index].pid = iter->pid;
			index++;
		}
	}
	mutex_unlock(&cam_dep_lock);

	memset(dep_arr, 0, MAX_DEP_NUM * sizeof(struct fpsgo_loading));
	memcpy(dep_arr, cam_dep_list_local, index * sizeof(struct fpsgo_loading));

	kfree(cam_dep_list_local);

	return index;
}

static void fbt_clear_dep_list(struct fpsgo_loading *pdep)
{
	if (pdep)
		fpsgo_free(pdep,
			MAX_DEP_NUM * sizeof(struct fpsgo_loading));
}

static void fbt_clear_min_cap(struct render_info *thr)
{
	int i;
	struct fpsgo_loading *dep_need_set;
	int temp_size_need_set = 0;

	dep_need_set =
		kcalloc(MAX_DEP_NUM, sizeof(struct fpsgo_loading), GFP_KERNEL);
	if (!dep_need_set) {
		FPSGO_LOGE("ERROR OOM %d\n", __LINE__);
		goto EXIT;
	}

	if (!thr || !thr->dep_arr)
		goto EXIT;

	if ((thr->pid == max_blc_pid && thr->buffer_id == max_blc_buffer_id))
		for (i = 0; i < thr->dep_valid_size; i++)
			fbt_reset_task_setting(&thr->dep_arr[i], 1);
	else {
		dep_a_except_b(
			&(thr->dep_arr[0]), thr->dep_valid_size,
			&max_blc_dep[0], max_blc_dep_num,
			&dep_need_set[0], &temp_size_need_set,
			0);
		print_dep(__func__, "dep",
			thr->pid, thr->buffer_id,
			&(thr->dep_arr[0]), thr->dep_valid_size);
		print_dep(__func__, "dep_need_clear",
			thr->pid, thr->buffer_id,
			&dep_need_set[0], temp_size_need_set);

		for (i = 0; i < temp_size_need_set; i++)
			fbt_reset_task_setting(&dep_need_set[i], 1);
	}
EXIT:
	kfree(dep_need_set);
}

int fbt_is_light_loading(int loading, int loading_threshold)
{
	if (!loading_threshold || loading > loading_threshold
		|| loading == -1 || (loading == 0 && !loading_ignore_enable))
		return 0;

	return 1;
}

static int fbt_is_heavy_task(int rank)
{
	return rank < max_cl_core_num;
}

static int  fbt_is_R_L_task(int pid, int heaviest_pid, int thr_pid)
{
	return (heaviest_pid && heaviest_pid == pid) || (thr_pid == pid);
}

static int fbt_get_opp_by_normalized_cap(unsigned int cap, int cluster)
{
	int tgt_opp;

	if (cluster >= cluster_num)
		cluster = 0;

	for (tgt_opp = (nr_freq_cpu - 1); tgt_opp > 0; tgt_opp--) {
		if (cpu_dvfs[cluster].capacity_ratio[tgt_opp] >= cap)
			break;
	}

	return tgt_opp;
}

int fbt_get_max_cap(int floor, int bhr_opp_local,
	int bhr_local, int pid, unsigned long long buffer_id)
{
	int *clus_opp;
	unsigned int *clus_floor_freq;
	int max_cap = 100;
	int tgt_opp = 0;
	int mbhr;
	int mbhr_opp;
	int cluster = 0;
	int i;

	clus_opp =
		kcalloc(cluster_num, sizeof(int), GFP_KERNEL);
	if (!clus_opp) {
		FPSGO_LOGE("ERROR OOM %d\n", __LINE__);
		return -ENOMEM;
	}

	clus_floor_freq =
		kcalloc(cluster_num, sizeof(unsigned int), GFP_KERNEL);
	if (!clus_floor_freq) {
		kfree(clus_opp);
		FPSGO_LOGE("ERROR OOM %d\n", __LINE__);
		return -ENOMEM;
	}

	if (cpu_max_freq <= 1)
		fpsgo_systrace_c_fbt(pid, buffer_id, cpu_max_freq,
			"cpu_max_freq");

	for (cluster = 0 ; cluster < cluster_num; cluster++) {
		tgt_opp = fbt_get_opp_by_normalized_cap(floor, cluster);

		clus_floor_freq[cluster] = cpu_dvfs[cluster].power[tgt_opp];
		clus_opp[cluster] = tgt_opp;

		mbhr_opp = (clus_opp[cluster] - bhr_opp_local);

		mbhr = clus_floor_freq[cluster] * 100;
		mbhr = mbhr / cpu_max_freq;
		mbhr = mbhr + bhr_local;
		mbhr = (min(mbhr, 100) * cpu_max_freq);
		mbhr = mbhr / 100;

		for (i = (nr_freq_cpu - 1); i >= 0; i--) {
			if (cpu_dvfs[cluster].power[i] > mbhr)
				break;
		}

		if (mbhr_opp > 0 && i > 0)
			max_cap = min(max_cap,
				(int)cpu_dvfs[cluster].capacity_ratio[min(mbhr_opp, i)]);
	}

	kfree(clus_floor_freq);
	kfree(clus_opp);

	return max_cap;
}

static void fbt_check_cm_limit(struct render_info *thread_info)
{
	int last_blc = 0;

	if (!thread_info || !thread_info->running_time)
		return;

	if (thread_info->pid == max_blc_pid &&
		thread_info->buffer_id == max_blc_buffer_id)
		last_blc = max_blc;
	else {
		if (thread_info->frame_type == NON_VSYNC_ALIGNED_TYPE) {
			mutex_lock(&blc_mlock);
			if (thread_info->p_blc)
				last_blc = thread_info->p_blc->blc;
			mutex_unlock(&blc_mlock);
		}
	}

	if (!last_blc)
		return;

	if (last_blc > cm_big_cap && thread_info->running_time
			> (long long)thread_info->boost_info.target_time
			+ (long long)cm_tdiff)
		fbt_notify_CM_limit(1);
	else
		fbt_notify_CM_limit(0);
}

static void fbt_cal_min_max_cap(struct render_info *thr,
	int min_cap, int min_cap_b,
	int min_cap_m, int jerk, int pid, unsigned long long buffer_id,
	int *final_min_cap, int *final_min_cap_b, int *final_min_cap_m,
	int *final_max_cap, int *final_max_cap_b, int *final_max_cap_m)
{
	int bhr_opp_local;
	int bhr_local;
	int raw_min_cap_b = 0, raw_min_cap_m = 0;
	int max_cap = 100, max_cap_b = 100, max_cap_m = 100;
	int separate_aa_final, boost_affinity_final, separate_pct_b_final,
		separate_pct_m_final, limit_uclamp_final, limit_ruclamp_final,
		limit_uclamp_m_final, limit_ruclamp_m_final;

	separate_aa_final = thr->attr.separate_aa_by_pid;
	boost_affinity_final = thr->attr.boost_affinity_by_pid;
	separate_pct_b_final = thr->attr.separate_pct_b_by_pid;
	separate_pct_m_final = thr->attr.separate_pct_m_by_pid;
	limit_uclamp_final = thr->attr.limit_uclamp_by_pid;
	limit_ruclamp_final = thr->attr.limit_ruclamp_by_pid;
	limit_uclamp_m_final = thr->attr.limit_uclamp_m_by_pid;
	limit_ruclamp_m_final = thr->attr.limit_ruclamp_m_by_pid;

	// Calculate bhr/bhr_opp
	if (jerk == FPSGO_JERK_INACTIVE) {
		bhr_opp_local = bhr_opp;
		bhr_local = bhr;
	} else {
		if (jerk == FPSGO_JERK_SECOND)
			bhr_opp_local = rescue_second_copp;
		else
			bhr_opp_local = rescue_opp_c;
		bhr_local = 0;
	}

	// Get max_cap
	max_cap = fbt_get_max_cap(min_cap, bhr_opp_local,
				bhr_local, pid, buffer_id);

	// separate_pct_b, separate_pct_m
	if (separate_aa_final && boost_affinity_final &&
		(separate_pct_b_final || separate_pct_m_final) &&
		jerk != FPSGO_JERK_SECOND) {
		raw_min_cap_b = min_cap_b;
		raw_min_cap_m = min_cap_m;
		min_cap_b = separate_pct_b_final ?
			(min_cap_b * separate_pct_b_final / 100) : min_cap_b;
		min_cap_m = separate_pct_m_final ?
			(min_cap_m * separate_pct_m_final / 100) : min_cap_m;
		min_cap_b = clamp(min_cap_b, 1, 100);
		min_cap_m = clamp(min_cap_m, 1, 100);
	}

	// Get max_cap_b, max_cap_m
	if (separate_aa_final) {
		max_cap_b = fbt_get_max_cap(min_cap_b, bhr_opp_local,
						bhr_local, pid, buffer_id);
		max_cap_m = fbt_get_max_cap(min_cap_m, bhr_opp_local,
						bhr_local, pid, buffer_id);
	}

	// limit_uclamp
	if ((limit_uclamp_final || limit_uclamp_m_final ||
		limit_ruclamp_final || limit_ruclamp_m_final) &&
		jerk != FPSGO_JERK_SECOND) {
		if (separate_aa_final && boost_affinity_final) {
			if (jerk == FPSGO_JERK_INACTIVE) {
				max_cap = limit_uclamp_final ?
					MIN(max_cap, limit_uclamp_final) : max_cap;
				max_cap_b = limit_uclamp_final ?
					MIN(max_cap_b, limit_uclamp_final) : max_cap_b;
				max_cap_m = limit_uclamp_m_final ?
					MIN(max_cap_m, limit_uclamp_m_final) : max_cap_m;
			} else {
				max_cap = limit_ruclamp_final ?
					MIN(max_cap, limit_ruclamp_final) : max_cap;
				max_cap_b = limit_ruclamp_final ?
					MIN(max_cap_b, limit_ruclamp_final) : max_cap_b;
				max_cap_m = limit_ruclamp_m_final ?
					MIN(max_cap_m, limit_ruclamp_m_final) : max_cap_m;
			}
			min_cap = (min_cap > max_cap) ? max_cap : min_cap;
			min_cap_b = (min_cap_b > max_cap_b) ? max_cap_b : min_cap_b;
			min_cap_m = (min_cap_m > max_cap_m) ? max_cap_m : min_cap_m;
		} else if (!separate_aa_final) {
			if (jerk == FPSGO_JERK_INACTIVE) {
				max_cap = limit_uclamp_final ?
					MIN(max_cap, limit_uclamp_final) : max_cap;
			} else {
				max_cap = limit_ruclamp_final ?
					MIN(max_cap, limit_ruclamp_final) : max_cap;
			}
			min_cap = (min_cap > max_cap) ? max_cap : min_cap;
		}
	}

	if (separate_aa_final) {
		fpsgo_systrace_c_fbt(pid, buffer_id,
		min_cap_b,	"perf_idx_big_core");
		fpsgo_systrace_c_fbt(pid, buffer_id,
		min_cap_m,	"perf_idx_middle_core");
		fpsgo_systrace_c_fbt_debug(pid, buffer_id,
		max_cap_b,	"perf_idx_max_big_core");
		fpsgo_systrace_c_fbt_debug(pid, buffer_id,
		max_cap_m,	"perf_idx_max_middle_core");
		if (separate_pct_b_final) {
			fpsgo_systrace_c_fbt_debug(pid, buffer_id,
			raw_min_cap_b,	"perf_idx_b_before_pct");
		}
		if (separate_pct_m_final) {
			fpsgo_systrace_c_fbt_debug(pid, buffer_id,
			raw_min_cap_m,	"perf_idx_m_before_pct");
		}
	}

	*final_min_cap = min_cap;
	*final_min_cap_b = min_cap_b;
	*final_min_cap_m = min_cap_m;
	*final_max_cap = max_cap;
	*final_max_cap_b = max_cap_b;
	*final_max_cap_m = max_cap_m;
}

static void fbt_set_min_cap_locked(struct render_info *thr, int min_cap,
			int min_cap_b, int min_cap_m, int max_cap, int max_cap_b,
			int max_cap_m, int jerk)
{
/*
 * boost_ta should be checked during the flow, not here.
 */
	int size = 0, size_final = 0, i, count_valid_dep = 0;
	char *dep_str = NULL;
	int ret;
	int heaviest_pid = 0;
	struct fpsgo_loading *dep_need_set;
	int temp_size_need_set = 0;
	char temp[MAX_PID_DIGIT] = {"\0"};
	int light_thread = 0;
	int loading_th_final = thr->attr.loading_th_by_pid;
	int loading_policy_final = thr->attr.light_loading_policy_by_pid;
	int llf_task_policy_final = thr->attr.llf_task_policy_by_pid;
	int separate_aa_final = thr->attr.separate_aa_by_pid;
	int separate_release_sec_final = thr->attr.separate_release_sec_by_pid;
	int boost_affinity_final = thr->attr.boost_affinity_by_pid;
	int boost_LR_final = thr->attr.boost_lr_by_pid;
	struct fpsgo_loading *cam_dep_arr = NULL;
	int cam_dep_arr_size = 0;
	struct fpsgo_loading *dep_final_need_set = NULL;
	int final_size_need_set = 0;

	if (!uclamp_boost_enable)
		return;

	if (!thr)
		return;

	if (!min_cap) {
		fbt_clear_min_cap(thr);
		fpsgo_systrace_c_fbt(thr->pid, thr->buffer_id,
			0,	"perf idx");
		fpsgo_systrace_c_fbt(thr->pid, thr->buffer_id,
			100,	"perf_idx_max");
		return;
	}

	if (jerk == FPSGO_JERK_INACTIVE) {
		ret = fbt_get_dep_list(thr);
		if (ret) {
			fpsgo_systrace_c_fbt(thr->pid, thr->buffer_id,
				ret, "fail dep-list");
			fpsgo_systrace_c_fbt(thr->pid, thr->buffer_id,
				0, "fail dep-list");
			return;
		}
	}

	size = thr->dep_valid_size;
	if (!size || !thr->dep_arr)
		return;

	dep_need_set =
		kcalloc(MAX_DEP_NUM, sizeof(struct fpsgo_loading), GFP_KERNEL);
	if (!dep_need_set) {
		FPSGO_LOGE("ERROR OOM %d\n", __LINE__);
		goto EXIT;
	}

	fpsgo_main_trace("FPSGO_MW PID: %d , loading_th_final: %d ", thr->pid, loading_th_final);

	if (loading_th_final || boost_affinity_final || boost_LR_final
				|| loading_enable || separate_aa_final)
		fbt_query_dep_list_loading(thr);

	dep_str = kcalloc(size + 1, MAX_PID_DIGIT * sizeof(char),
				GFP_KERNEL);
	if (!dep_str)
		goto EXIT;

	cam_dep_arr = kcalloc(MAX_DEP_NUM, sizeof(struct fpsgo_loading), GFP_KERNEL);
	if (!cam_dep_arr) {
		FPSGO_LOGE("ERROR OOM %d\n", __LINE__);
		goto EXIT;
	}
	dep_final_need_set = kcalloc(MAX_DEP_NUM, sizeof(struct fpsgo_loading), GFP_KERNEL);
	if (!dep_final_need_set) {
		FPSGO_LOGE("ERROR OOM %d\n", __LINE__);
		goto EXIT;
	}

	cam_dep_arr_size = fbt_get_cam_dep_list(cam_dep_arr);
	if (cam_dep_arr_size <= 0) {
		cam_dep_arr_size = 0;
		memset(cam_dep_arr, 0, MAX_DEP_NUM * sizeof(struct fpsgo_loading));
	}

	if (thr->pid == max_blc_pid && thr->buffer_id == max_blc_buffer_id) {
		dep_a_except_b(
			&(thr->dep_arr[0]), thr->dep_valid_size,
			&cam_dep_arr[0], cam_dep_arr_size,
			&dep_final_need_set[0], &final_size_need_set,
			0);
		print_dep(__func__, "max_dep",
			thr->pid, thr->buffer_id,
			&(thr->dep_arr[0]), thr->dep_valid_size);
		print_dep(__func__, "max_dep_final_need_set",
			max_blc_pid, max_blc_buffer_id,
			&dep_final_need_set[0], final_size_need_set);
		size_final = final_size_need_set;
	} else {
		dep_a_except_b(
			&(thr->dep_arr[0]), thr->dep_valid_size,
			&max_blc_dep[0], max_blc_dep_num,
			&dep_need_set[0], &temp_size_need_set,
			0);
		print_dep(__func__, "dep",
			thr->pid, thr->buffer_id,
			&(thr->dep_arr[0]), thr->dep_valid_size);
		print_dep(__func__, "dep_need_set",
			thr->pid, thr->buffer_id,
			&dep_need_set[0], temp_size_need_set);

		dep_a_except_b(
			&(thr->dep_arr[0]), thr->dep_valid_size,
			&cam_dep_arr[0], cam_dep_arr_size,
			&dep_final_need_set[0], &final_size_need_set,
			0);
		print_dep(__func__, "dep_final_need_set",
			thr->pid, thr->buffer_id,
			&dep_final_need_set[0], final_size_need_set);
		size_final = final_size_need_set;
	}

	if (jerk == FPSGO_JERK_INACTIVE)
		fbt_check_cm_limit(thr);

	if (loading_th_final || boost_affinity_final
			|| boost_LR_final || separate_aa_final) {
		for (i = 0; i < thr->dep_valid_size; i++) {
			struct fpsgo_loading *fl;

			fl = &(thr->dep_arr[i]);
			if (fl->rmidx)
				continue;

			if (!fl->pid)
				continue;

			if (fl->loading == 0 || fl->loading == -1)
				fl->loading = fpsgo_fbt2minitop_query_single(fl->pid);
		}

		sort(thr->dep_arr, thr->dep_valid_size,
			sizeof(struct fpsgo_loading), cmp_loading_desc, NULL);
		heaviest_pid = thr->dep_arr[0].pid;
	}



	for (i = 0, count_valid_dep = 0; i < thr->dep_valid_size; i++) {
		struct fpsgo_loading *fl;

		fl = &(thr->dep_arr[i]);
		if (fl->rmidx)
			continue;

		if (!fl->pid)
			continue;

		if (loading_th_final || boost_affinity_final || boost_LR_final ||
			separate_aa_final) {
			fpsgo_systrace_c_fbt_debug(fl->pid, thr->buffer_id,
				fl->loading, "dep-loading");
		}

		light_thread = fbt_is_light_loading(fl->loading, loading_th_final);

		if (light_thread &&
			bhr_opp != (nr_freq_cpu - 1) && fl->action == 0) {
			fbt_set_per_task_cap(fl->pid,
				(!loading_policy_final) ? 0
				: min_cap * loading_policy_final / 100, max_cap);
			if (llf_task_policy_final == FPSGO_TPOLICY_NONE && boost_affinity_final)
				fbt_set_task_policy(fl, FPSGO_TPOLICY_AFFINITY,
					FPSGO_PREFER_L_M, 0);
			else
				fbt_set_task_policy(fl, llf_task_policy_final,
					FPSGO_PREFER_LITTLE, 0);
		} else if (fbt_is_heavy_task(count_valid_dep)) {
			if (heaviest_pid && heaviest_pid == fl->pid)
				fpsgo_systrace_c_fbt(thr->pid, thr->buffer_id,
						heaviest_pid, "heavy_pid");
			if (separate_aa_final)
				fbt_set_per_task_cap(fl->pid, min_cap_b, max_cap_b);
			else
				fbt_set_per_task_cap(fl->pid, min_cap, max_cap);
			if (boost_affinity_final)
				fbt_set_task_policy(fl, boost_affinity_final,
						FPSGO_PREFER_BIG, 1);
			else if (boost_LR_final && thr->hwui == RENDER_INFO_HWUI_NONE
					&& fbt_is_R_L_task(fl->pid, heaviest_pid, thr->pid)) {
				fbt_set_task_policy(fl, FPSGO_TPOLICY_NONE,
						FPSGO_PREFER_NONE, 1);
			}
		} else {
			if (separate_aa_final) {
				if (separate_release_sec_final)
					fbt_set_per_task_cap(fl->pid, min_cap_m,
						max(max_cap_m, max_cap_b));
				else
					fbt_set_per_task_cap(fl->pid, min_cap_m, max_cap_m);
			} else
				fbt_set_per_task_cap(fl->pid, min_cap, max_cap);
			if (boost_affinity_final) {
				fbt_set_task_policy(fl, FPSGO_TPOLICY_AFFINITY,
						FPSGO_PREFER_L_M, 0);
			} else if (boost_LR_final && thr->hwui == RENDER_INFO_HWUI_NONE
					&& fbt_is_R_L_task(fl->pid, heaviest_pid, thr->pid)) {
				fbt_set_task_policy(fl, FPSGO_TPOLICY_NONE,
						FPSGO_PREFER_NONE, 1);
			} else {
				fbt_reset_task_setting(fl, 0);
			}
		}

		if (strlen(dep_str) == 0)
			snprintf(temp, sizeof(temp), "%d", fl->pid);
		else
			snprintf(temp, sizeof(temp), ",%d", fl->pid);

		if (strlen(dep_str) + strlen(temp) < MAIN_LOG_SIZE)
			strncat(dep_str, temp, strlen(temp));

		count_valid_dep++;
	}

	fpsgo_main_trace("[%d] dep-list %s", thr->pid, dep_str);
	fpsgo_systrace_c_fbt(thr->pid, thr->buffer_id,
		min_cap,	"perf idx");
	fpsgo_systrace_c_fbt(thr->pid, thr->buffer_id,
		max_cap,	"perf_idx_max");

EXIT:
	kfree(dep_final_need_set);
	kfree(cam_dep_arr);
	kfree(dep_str);
	kfree(dep_need_set);
}

void fbt_set_render_boost_attr(struct render_info *thr)
{
	struct fpsgo_boost_attr *render_attr;
#if FPSGO_MW
	struct fpsgo_attr_by_pid *fpsgo_attr;
	struct fpsgo_boost_attr pid_attr;
#endif  // FPSGO_MW

	if (!thr)
		return;

	render_attr = &(thr->attr);
	render_attr->rescue_second_enable_by_pid = rescue_second_enable;
	render_attr->rescue_second_group_by_pid = rescue_second_group;
	render_attr->rescue_second_time_by_pid = rescue_second_time;
	render_attr->llf_task_policy_by_pid = llf_task_policy;
	render_attr->light_loading_policy_by_pid = loading_policy;
	render_attr->loading_th_by_pid = loading_th;
	render_attr->filter_frame_enable_by_pid = filter_frame_enable;
	render_attr->filter_frame_window_size_by_pid = filter_frame_window_size;
	render_attr->filter_frame_kmin_by_pid = filter_frame_kmin;
	render_attr->boost_affinity_by_pid = boost_affinity;
	render_attr->boost_lr_by_pid = boost_LR;
	render_attr->separate_aa_by_pid = separate_aa;
	render_attr->separate_release_sec_by_pid = separate_release_sec;
	render_attr->limit_uclamp_by_pid = limit_uclamp;
	render_attr->limit_ruclamp_by_pid = limit_ruclamp;
	render_attr->limit_uclamp_m_by_pid = limit_uclamp_m;
	render_attr->limit_ruclamp_m_by_pid = limit_ruclamp_m;
	render_attr->separate_pct_b_by_pid = separate_pct_b;
	render_attr->separate_pct_m_by_pid = separate_pct_m;
	render_attr->blc_boost_by_pid = blc_boost;

	render_attr->qr_enable_by_pid = qr_enable;
	render_attr->qr_t2wnt_x_by_pid = qr_t2wnt_x;
	render_attr->qr_t2wnt_y_p_by_pid = qr_t2wnt_y_p;
	render_attr->qr_t2wnt_y_n_by_pid = qr_t2wnt_y_n;

	render_attr->gcc_enable_by_pid = gcc_enable;
	render_attr->gcc_reserved_up_quota_pct_by_pid = gcc_reserved_up_quota_pct;
	render_attr->gcc_reserved_down_quota_pct_by_pid = gcc_reserved_down_quota_pct;
	render_attr->gcc_up_step_by_pid = gcc_up_step;
	render_attr->gcc_down_step_by_pid = gcc_down_step;
	render_attr->gcc_fps_margin_by_pid = gcc_fps_margin;
	render_attr->gcc_up_sec_pct_by_pid = gcc_up_sec_pct;
	render_attr->gcc_down_sec_pct_by_pid = gcc_down_sec_pct;
	render_attr->gcc_enq_bound_thrs_by_pid = gcc_enq_bound_thrs;
	render_attr->gcc_enq_bound_quota_by_pid = gcc_enq_bound_quota;
	render_attr->gcc_deq_bound_thrs_by_pid = gcc_deq_bound_thrs;
	render_attr->gcc_deq_bound_quota_by_pid = gcc_deq_bound_quota;

#if FPSGO_MW
	fpsgo_attr = fpsgo_find_attr_by_pid(thr->tgid, 0);
	if (!fpsgo_attr)
		return;

	pid_attr = fpsgo_attr->attr;
	if (pid_attr.rescue_second_enable_by_pid != BY_PID_DEFAULT_VAL)
		render_attr->rescue_second_enable_by_pid =
			pid_attr.rescue_second_enable_by_pid;
	if (pid_attr.rescue_second_group_by_pid != BY_PID_DEFAULT_VAL)
		render_attr->rescue_second_group_by_pid =
			pid_attr.rescue_second_group_by_pid;
	if (pid_attr.rescue_second_time_by_pid != BY_PID_DEFAULT_VAL)
		render_attr->rescue_second_time_by_pid =
			pid_attr.rescue_second_time_by_pid;
	if (pid_attr.llf_task_policy_by_pid != BY_PID_DEFAULT_VAL)
		render_attr->llf_task_policy_by_pid =
			pid_attr.llf_task_policy_by_pid;
	if (pid_attr.light_loading_policy_by_pid != BY_PID_DEFAULT_VAL)
		render_attr->light_loading_policy_by_pid =
			pid_attr.light_loading_policy_by_pid;
	if (pid_attr.loading_th_by_pid != BY_PID_DEFAULT_VAL)
		render_attr->loading_th_by_pid = pid_attr.loading_th_by_pid;
	if (pid_attr.filter_frame_enable_by_pid != BY_PID_DEFAULT_VAL)
		render_attr->filter_frame_enable_by_pid =
			pid_attr.filter_frame_enable_by_pid;
	if (pid_attr.filter_frame_window_size_by_pid != BY_PID_DEFAULT_VAL)
		render_attr->filter_frame_window_size_by_pid =
			pid_attr.filter_frame_window_size_by_pid;
	if (pid_attr.filter_frame_kmin_by_pid != BY_PID_DEFAULT_VAL)
		render_attr->filter_frame_kmin_by_pid =
			pid_attr.filter_frame_kmin_by_pid;
	if (pid_attr.boost_affinity_by_pid != BY_PID_DEFAULT_VAL)
		render_attr->boost_affinity_by_pid =
			pid_attr.boost_affinity_by_pid;
	if (pid_attr.boost_lr_by_pid != BY_PID_DEFAULT_VAL)
		render_attr->boost_lr_by_pid = pid_attr.boost_lr_by_pid;
	if (pid_attr.separate_aa_by_pid != BY_PID_DEFAULT_VAL)
		render_attr->separate_aa_by_pid =
			pid_attr.separate_aa_by_pid;
	if (pid_attr.separate_release_sec_by_pid != BY_PID_DEFAULT_VAL)
		render_attr->separate_release_sec_by_pid =
			pid_attr.separate_release_sec_by_pid;
	if (pid_attr.limit_uclamp_by_pid != BY_PID_DEFAULT_VAL)
		render_attr->limit_uclamp_by_pid =
			pid_attr.limit_uclamp_by_pid;
	if (pid_attr.limit_ruclamp_by_pid != BY_PID_DEFAULT_VAL)
		render_attr->limit_ruclamp_by_pid =
			pid_attr.limit_ruclamp_by_pid;
	if (pid_attr.limit_uclamp_m_by_pid != BY_PID_DEFAULT_VAL)
		render_attr->limit_uclamp_m_by_pid =
			pid_attr.limit_uclamp_m_by_pid;
	if (pid_attr.limit_ruclamp_m_by_pid != BY_PID_DEFAULT_VAL)
		render_attr->limit_ruclamp_m_by_pid =
			pid_attr.limit_ruclamp_m_by_pid;
	if (pid_attr.separate_pct_b_by_pid != BY_PID_DEFAULT_VAL)
		render_attr->separate_pct_b_by_pid =
			pid_attr.separate_pct_b_by_pid;
	if (pid_attr.separate_pct_m_by_pid != BY_PID_DEFAULT_VAL)
		render_attr->separate_pct_m_by_pid =
			pid_attr.separate_pct_m_by_pid;
	if (pid_attr.blc_boost_by_pid != BY_PID_DEFAULT_VAL)
		render_attr->blc_boost_by_pid =
			pid_attr.blc_boost_by_pid;
	if (pid_attr.qr_enable_by_pid != BY_PID_DEFAULT_VAL)
		render_attr->qr_enable_by_pid =
			pid_attr.qr_enable_by_pid;
	if (pid_attr.qr_t2wnt_x_by_pid != BY_PID_DEFAULT_VAL)
		render_attr->qr_t2wnt_x_by_pid =
			pid_attr.qr_t2wnt_x_by_pid;
	if (pid_attr.qr_t2wnt_y_n_by_pid != BY_PID_DEFAULT_VAL)
		render_attr->qr_t2wnt_y_n_by_pid =
			pid_attr.qr_t2wnt_y_n_by_pid;
	if (pid_attr.qr_t2wnt_y_p_by_pid != BY_PID_DEFAULT_VAL)
		render_attr->qr_t2wnt_y_p_by_pid =
			pid_attr.qr_t2wnt_y_p_by_pid;

	if (pid_attr.gcc_enable_by_pid != BY_PID_DEFAULT_VAL)
		render_attr->gcc_enable_by_pid =
			pid_attr.gcc_enable_by_pid;
	if (pid_attr.gcc_fps_margin_by_pid != BY_PID_DEFAULT_VAL)
		render_attr->gcc_fps_margin_by_pid =
			pid_attr.gcc_fps_margin_by_pid;
	if (pid_attr.gcc_up_sec_pct_by_pid != BY_PID_DEFAULT_VAL)
		render_attr->gcc_up_sec_pct_by_pid =
			pid_attr.gcc_up_sec_pct_by_pid;
	if (pid_attr.gcc_down_sec_pct_by_pid != BY_PID_DEFAULT_VAL)
		render_attr->gcc_down_sec_pct_by_pid =
			pid_attr.gcc_down_sec_pct_by_pid;
	if (pid_attr.gcc_up_step_by_pid != BY_PID_DEFAULT_VAL)
		render_attr->gcc_up_step_by_pid =
			pid_attr.gcc_up_step_by_pid;
	if (pid_attr.gcc_down_step_by_pid != BY_PID_DEFAULT_VAL)
		render_attr->gcc_down_step_by_pid =
			pid_attr.gcc_down_step_by_pid;
	if (pid_attr.gcc_reserved_up_quota_pct_by_pid != BY_PID_DEFAULT_VAL)
		render_attr->gcc_reserved_up_quota_pct_by_pid =
			pid_attr.gcc_reserved_up_quota_pct_by_pid;
	if (pid_attr.gcc_reserved_down_quota_pct_by_pid != BY_PID_DEFAULT_VAL)
		render_attr->gcc_reserved_down_quota_pct_by_pid =
			pid_attr.gcc_reserved_down_quota_pct_by_pid;
	if (pid_attr.gcc_enq_bound_thrs_by_pid != BY_PID_DEFAULT_VAL)
		render_attr->gcc_enq_bound_thrs_by_pid =
			pid_attr.gcc_enq_bound_thrs_by_pid;
	if (pid_attr.gcc_deq_bound_thrs_by_pid != BY_PID_DEFAULT_VAL)
		render_attr->gcc_deq_bound_thrs_by_pid =
			pid_attr.gcc_deq_bound_thrs_by_pid;
	if (pid_attr.gcc_enq_bound_quota_by_pid != BY_PID_DEFAULT_VAL)
		render_attr->gcc_enq_bound_quota_by_pid =
			pid_attr.gcc_enq_bound_quota_by_pid;
	if (pid_attr.gcc_deq_bound_quota_by_pid != BY_PID_DEFAULT_VAL)
		render_attr->gcc_deq_bound_quota_by_pid =
			pid_attr.gcc_deq_bound_quota_by_pid;
#endif  // FPSGO_MW
}

void fbt_set_render_last_cb(struct render_info *thr, unsigned long long ts_ns)
{
	int new_ts_100us;

	new_ts_100us = nsec_to_100usec(ts_ns);
	thr->render_last_cb_ts = new_ts_100us;
	fpsgo_systrace_c_fbt_debug(thr->pid, thr->buffer_id,
		thr->render_last_cb_ts, "render_last_cb_ts");
}


static int fbt_get_target_cluster(unsigned int blc_wt)
{
	int cluster = min_cap_cluster;
	int i = max_cap_cluster;
	int order = (max_cap_cluster > min_cap_cluster)?1:0;

	while (i != min_cap_cluster) {
		if (blc_wt >= cpu_dvfs[i].capacity_ratio[nr_freq_cpu - 1]) {
			cluster = i;
			break;
		}

		if (order)
			i--;
		else
			i++;
	}

	if (cluster >= cluster_num || cluster < 0)
		cluster = min_cap_cluster;

	return cluster;
}

static unsigned int fbt_enhance_floor(unsigned int blc_wt, int level, int enh)
{
	int tgt_opp = 0;
	int cluster;

	int blc_wt1;
	int blc_wt2;

	cluster = fbt_get_target_cluster(blc_wt);
	tgt_opp = fbt_get_opp_by_normalized_cap(blc_wt, cluster);
	tgt_opp = max((int)(tgt_opp - level), 0);
	blc_wt1 = cpu_dvfs[cluster].capacity_ratio[tgt_opp];

	blc_wt2 = blc_wt + enh;
	blc_wt2 = clamp(blc_wt2, 1, 100);

	blc_wt = max(blc_wt1, blc_wt2);

	return blc_wt;
}

static unsigned int fbt_must_enhance_floor(unsigned int blc_wt,
		unsigned int orig_blc, int level)
{
	int tgt_opp = 0;
	int orig_opp = 0;
	int cluster;

	cluster = fbt_get_target_cluster(blc_wt);

	tgt_opp = fbt_get_opp_by_normalized_cap(blc_wt, cluster);
	orig_opp = fbt_get_opp_by_normalized_cap(orig_blc, cluster);

	if (orig_opp - tgt_opp < level)
		tgt_opp = max((int)(orig_opp - level), 0);

	blc_wt = cpu_dvfs[cluster].capacity_ratio[tgt_opp];

	return blc_wt;
}

unsigned int fbt_get_new_base_blc(struct cpu_ctrl_data *pld,
				int floor, int enhance, int enhance_opp, int headroom)
{
	int cluster;
	unsigned int blc_wt = 0U;
	int base;

	if (!pld) {
		FPSGO_LOGE("ERROR %d\n", __LINE__);
		return 0U;
	}

	if (boost_ta)
		base = max_blc_cur;
	else
		base = floor;

	blc_wt = fbt_enhance_floor(base, enhance_opp, enhance);

	for (cluster = 0 ; cluster < cluster_num; cluster++) {
		pld[cluster].min = -1;
		if (suppress_ceiling) {
			int opp =
				fbt_get_opp_by_normalized_cap(blc_wt, cluster);

			opp = MIN(opp, base_opp[cluster]);
			opp = clamp(opp, 0, nr_freq_cpu - 1);

			pld[cluster].max = cpu_dvfs[cluster].power[max(
				(int)(opp - headroom), 0)];
		} else
			pld[cluster].max = -1;
	}

	return blc_wt;
}

static void fbt_pidlist_add(int pid, struct list_head *dest_list)
{
	struct fbt_pid_list *obj = NULL;

	obj = kzalloc(sizeof(struct fbt_pid_list), GFP_ATOMIC);
	if (!obj)
		return;

	INIT_LIST_HEAD(&obj->entry);
	obj->pid = pid;
	list_add_tail(&obj->entry, dest_list);
}

static int fbt_task_running_locked(struct task_struct *tsk)
{
	unsigned int tsk_state = READ_ONCE(tsk->__state);

	if (tsk_state == TASK_RUNNING)
		return 1;

	return 0;
}

static int fbt_query_running(int tgid, struct list_head *proc_list)
{
	int ret = 0;
	struct task_struct *gtsk, *tsk;
	int hit = 0;
	int pid;

	rcu_read_lock();

	gtsk = find_task_by_vpid(tgid);
	if (!gtsk) {
		rcu_read_unlock();
		return 0;
	}

	get_task_struct(gtsk);
	hit = fbt_task_running_locked(gtsk);
	if (hit) {
		ret = 1;
		goto EXIT;
	}

	fbt_pidlist_add(gtsk->pid, proc_list);

	list_for_each_entry(tsk, &gtsk->thread_group, thread_group) {
		if (!tsk)
			continue;

		get_task_struct(tsk);
		hit = fbt_task_running_locked(tsk);
		pid = tsk->pid;
		put_task_struct(tsk);

		if (hit) {
			ret = 1;
			break;
		}

		fbt_pidlist_add(pid, proc_list);
	}

EXIT:
	put_task_struct(gtsk);

	rcu_read_unlock();

	return ret;
}

#define MAX_RQ_COUNT 200
static int fbt_query_rq(int tgid, struct list_head *proc_list)
{
	int ret = 0;
	unsigned long flags;
	struct task_struct *p;
	int cpu;
	struct fbt_pid_list *pos, *next;
	int *rq_pid;
	int rq_idx = 0, i;

	/* get rq */
	rq_pid = kcalloc(MAX_RQ_COUNT, sizeof(int), GFP_KERNEL);
	if (!rq_pid)
		return 0;

	for_each_possible_cpu(cpu) {
		raw_spin_lock_irqsave(&cpu_rq(cpu)->__lock, flags);
		list_for_each_entry(p, &cpu_rq(cpu)->cfs_tasks, se.group_node) {
			rq_pid[rq_idx] = p->pid;
			rq_idx++;

			if (rq_idx >= MAX_RQ_COUNT) {
				fpsgo_systrace_c_fbt(tgid, 0, rq_idx, "rq_shrink");
				raw_spin_unlock_irqrestore(&cpu_rq(cpu)->__lock, flags);
				goto NEXT;
			}
		}
		raw_spin_unlock_irqrestore(&cpu_rq(cpu)->__lock, flags);
	}

NEXT:
	if (!rq_idx)
		goto EXIT;

	sort(rq_pid, rq_idx, sizeof(int), cmpint, NULL);

	/* compare */
	pos = list_first_entry_or_null(proc_list, typeof(*pos), entry);
	if (!pos)
		goto EXIT;

	for (i = 0; i < rq_idx; i++) {
		list_for_each_entry_safe_from(pos, next, proc_list, entry) {
			if (rq_pid[i] == pos->pid) {
				ret = 1;
				goto EXIT;
			}

			if (rq_pid[i] < pos->pid)
				break;
		}
	}

EXIT:
	kfree(rq_pid);

	return ret;
}

static int fbt_query_state(int pid, int tgid)
{
	int hit = 0;
	struct list_head proc_list;
	struct fbt_pid_list *pos, *next;

	if (!tgid) {
		tgid = fpsgo_get_tgid(pid);
		if (!tgid)
			return 0;
	}

	INIT_LIST_HEAD(&proc_list);

	hit = fbt_query_running(tgid, &proc_list);
	if (hit)
		goto EXIT;

	if (list_empty(&proc_list))
		goto EXIT;

	hit = fbt_query_rq(tgid, &proc_list);

EXIT:
	list_for_each_entry_safe(pos, next, &proc_list, entry) {
		list_del(&pos->entry);
		kfree(pos);
	}

	return hit;
}

void eara2fbt_set_2nd_t2wnt(int pid, unsigned long long buffer_id,
				unsigned long long t_duration)
{
	struct render_info *thr = NULL;

	fpsgo_render_tree_lock(__func__);
	thr = eara2fpsgo_search_render_info(pid, buffer_id);
	if (!thr) {
		fpsgo_render_tree_unlock(__func__);
		return;
	}
	thr->boost_info.t_duration = t_duration;
	fpsgo_render_tree_unlock(__func__);
}
EXPORT_SYMBOL(eara2fbt_set_2nd_t2wnt);

static int fbt_print_rescue_info(int pid, int buffer_id,
	unsigned long long q_end_ts,
	int quota, int scn, int num, int policy, int blc_wt, int last_blw)
{
	int blc_changed = 0;

	switch (policy) {
	case FPSGO_JERK_ONLY_CEILING_WAIT_ENQ:
		fpsgo_systrace_c_fbt(pid, buffer_id, num, "wait_enqueue");
		fpsgo_systrace_c_fbt(pid, buffer_id, 0, "wait_enqueue");
		break;

	case FPSGO_JERK_ONLY_CEILING_WAIT_DEQ:
		fpsgo_systrace_c_fbt(pid, buffer_id, num, "wait_enqueue");
		fpsgo_systrace_c_fbt(pid, buffer_id, 0, "wait_enqueue");
		break;

	case FPSGO_JERK_NEED:
		fpsgo_systrace_c_fbt(pid, buffer_id, num, "jerk_need");
		fpsgo_systrace_c_fbt(pid, buffer_id, 0, "jerk_need");
		blc_changed = 1;
		break;

	case FPSGO_JERK_POSTPONE:
		fpsgo_systrace_c_fbt(pid, buffer_id, num, "jerk_postpone");
		fpsgo_systrace_c_fbt(pid, buffer_id, 0, "jerk_postpone");
		return 0;

	case FPSGO_JERK_DISAPPEAR:
		fpsgo_systrace_c_fbt(pid, buffer_id, num, "not_running");
		fpsgo_systrace_c_fbt(pid, buffer_id, 0, "not_running");
		policy = 0;
		break;

	default:
		break;
	}

	if (num > 1 || policy == FPSGO_JERK_NEED) /* perf idx is valid */
		fpsgo_main_trace("rescue_info:%llu,%d,%d,%d,%d,%d,%d",
			q_end_ts, pid, quota, scn, num, policy, blc_wt);
	else
		fpsgo_main_trace("rescue_info:%llu,%d,%d,%d,%d,%d,%d",
			q_end_ts, pid, quota, scn, num, policy, last_blw);


	return 0;
}

static int fbt_check_to_jerk(
		unsigned long long enq_start, unsigned long long enq_end,
		unsigned long long deq_start, unsigned long long deq_end,
		unsigned long long deq_len, int pid,
		unsigned long long buffer_id, int tgid)
{
	/*not running*/
	if (check_running && !fbt_query_state(pid, tgid))
		return FPSGO_JERK_DISAPPEAR;

	/*during enqueue*/
	if (enq_start >= enq_end)
		return FPSGO_JERK_ONLY_CEILING_WAIT_ENQ;

	/*after enqueue before dequeue*/
	if (enq_end > enq_start && enq_end >= deq_start)
		return FPSGO_JERK_NEED;

	/*after dequeue before enqueue*/
	if (deq_end >= deq_start && deq_start > enq_end) {
		if (deq_len > deqtime_bound)
			return FPSGO_JERK_ONLY_CEILING_WAIT_DEQ;
		else
			return FPSGO_JERK_NEED;
	}

	/*during dequeue*/
	return FPSGO_JERK_POSTPONE;
}

static void fbt_do_jerk_boost(struct render_info *thr, int blc_wt, int blc_wt_b,
			int blc_wt_m, int boost_group, int jerk)
{
	int max_cap = 100, max_cap_b = 100, max_cap_m = 100;
	if (boost_ta || boost_group) {
		fbt_set_boost_value(blc_wt);
		fpsgo_systrace_c_fbt(thr->pid, thr->buffer_id, blc_wt, "perf idx");
		max_blc_cur = blc_wt;
		boosted_group = 1;
	} else {
		fbt_cal_min_max_cap(thr, blc_wt, blc_wt_b, blc_wt_m, jerk, thr->pid,
			thr->buffer_id, &blc_wt, &blc_wt_b, &blc_wt_m, &max_cap, &max_cap_b,
			&max_cap_m);
		fbt_set_min_cap_locked(thr, blc_wt, blc_wt_b, blc_wt_m, max_cap,
			max_cap_b, max_cap_m, jerk);
	}

	if (ultra_rescue)
		fbt_boost_dram(1);
}

static void fbt_cancel_sjerk(void)
{
	if (sjerk.jerking) {
		hrtimer_cancel(&(sjerk.timer));
		sjerk.jerking = 0;
	}

	sjerk.active_id = -1;
}

static void fbt_set_sjerk(int pid, unsigned long long buffer_id,
		unsigned long long identifier, int active_id, unsigned long long st2wnt)
{
	unsigned long long t2wnt;
	int rescue_second_time_final = rescue_second_time;
	struct render_info *thr;

	thr = fpsgo_search_and_add_render_info(pid, identifier, 0);
	if (thr)
		rescue_second_time_final = thr->attr.rescue_second_time_by_pid;

	fbt_cancel_sjerk();

	t2wnt = st2wnt;
	t2wnt *= rescue_second_time_final;
	t2wnt = clamp(t2wnt, 1ULL, (unsigned long long)FBTCPU_SEC_DIVIDER);
	fpsgo_systrace_c_fbt_debug(pid, buffer_id, t2wnt, "st2wnt");

	sjerk.pid = pid;
	sjerk.identifier = identifier;
	sjerk.active_id = active_id;
	sjerk.jerking = 1;
	hrtimer_start(&(sjerk.timer), ns_to_ktime(t2wnt), HRTIMER_MODE_REL);
}


void (*jatm_notify_fp)(int enable);
EXPORT_SYMBOL(jatm_notify_fp);

static void fbt_do_sjerk(struct work_struct *work)
{
	struct fbt_sjerk *jerk;
	struct render_info *thr;
	unsigned int blc_wt = 0U, blc_wt_b = 0U, blc_wt_m = 0U;
	struct cpu_ctrl_data *pld;
	int do_jerk = FPSGO_JERK_DISAPPEAR;
	int rescue_second_enable_final;
	int rescue_second_group_final;
	int separate_aa_final;

	jerk = container_of(work, struct fbt_sjerk, work);

	if (!jerk || !jerk->pid || !jerk->identifier || !jerk->jerking)
		return;

	fpsgo_render_tree_lock(__func__);

	thr = fpsgo_search_and_add_render_info(jerk->pid, jerk->identifier, 0);
	if (!thr) {
		fpsgo_render_tree_unlock(__func__);
		return;
	}

	fpsgo_thread_lock(&(thr->thr_mlock));

	mutex_lock(&fbt_mlock);

	rescue_second_enable_final = thr->attr.rescue_second_enable_by_pid;
	rescue_second_group_final = thr->attr.rescue_second_group_by_pid;
	separate_aa_final = thr->attr.separate_aa_by_pid;

	if (thr->boost_info.sbe_rescue != 0)
		goto EXIT;

	if (thr->pid != max_blc_pid || thr->buffer_id != max_blc_buffer_id)
		goto EXIT;

	if (!rescue_second_enable_final)
		goto EXIT;

	blc_wt = thr->boost_info.last_blc;

	if (!blc_wt || thr->linger != 0 || jerk->active_id != thr->boost_info.proc.active_jerk_id)
		goto EXIT;

	do_jerk = fbt_check_to_jerk(thr->t_enqueue_start,
		thr->t_enqueue_end, thr->t_dequeue_start,
		thr->t_dequeue_end, thr->dequeue_length,
		thr->pid, thr->buffer_id, thr->tgid);

	if (do_jerk == FPSGO_JERK_DISAPPEAR)
		goto EXIT;

	pld = kcalloc(cluster_num,
			sizeof(struct cpu_ctrl_data), GFP_KERNEL);
	if (!pld)
		goto EXIT;

	blc_wt = fbt_get_new_base_blc(pld, thr->boost_info.last_blc,
		rescue_second_enhance_f, rescue_opp_f, rescue_second_copp);
	if (separate_aa_final) {
		blc_wt_b = fbt_get_new_base_blc(pld, thr->boost_info.last_blc_b,
			rescue_second_enhance_f, rescue_opp_f, rescue_second_copp);
		blc_wt_m = fbt_get_new_base_blc(pld, thr->boost_info.last_blc_m,
			rescue_second_enhance_f, rescue_opp_f, rescue_second_copp);
	}

	fbt_set_hard_limit_locked(FPSGO_HARD_NONE, pld);

	fbt_set_ceiling(pld, thr->pid, thr->buffer_id);
	fbt_do_jerk_boost(thr, blc_wt, blc_wt_b, blc_wt_m, rescue_second_group_final,
			FPSGO_JERK_SECOND);

	thr->boost_info.last_blc = blc_wt;
	if (separate_aa_final) {
		thr->boost_info.last_blc_b = blc_wt_b;
		thr->boost_info.last_blc_m = blc_wt_m;
	}

	thr->boost_info.cur_stage = FPSGO_JERK_SECOND;

	max_blc_stage = FPSGO_JERK_SECOND;
	fpsgo_systrace_c_fbt_debug(-100, 0, max_blc_stage, "max_blc_stage");

	kfree(pld);

EXIT:
	jerk->jerking = 0;

	fbt_print_rescue_info(thr->pid, thr->buffer_id, thr->t_enqueue_end,
		0, 0, 2, do_jerk, blc_wt, thr->boost_info.last_blc);

	mutex_unlock(&fbt_mlock);
	fpsgo_thread_unlock(&(thr->thr_mlock));
	fpsgo_render_tree_unlock(__func__);
}

static void fbt_do_jerk_locked(struct render_info *thr, struct fbt_jerk *jerk, int jerk_id)
{
	unsigned int blc_wt = 0U, blc_wt_b = 0U, blc_wt_m = 0U;
	struct cpu_ctrl_data *pld;
	int temp_blc = 0, temp_blc_b = 0, temp_blc_m = 0;
	int do_jerk;
	int rescue_second_enable_final = thr->attr.rescue_second_enable_by_pid;
	int separate_aa_final = thr->attr.separate_aa_by_pid;

	if (!thr)
		return;

	if (thr->boost_info.sbe_rescue != 0)
		return;

	blc_wt  = thr->boost_info.last_blc;
	blc_wt_b = thr->boost_info.last_blc_b;
	blc_wt_m = thr->boost_info.last_blc_m;
	temp_blc = blc_wt;
	temp_blc_b = blc_wt_b;
	temp_blc_m = blc_wt_m;
	if (!blc_wt || (separate_aa_final && !(blc_wt_b && blc_wt_m)))
		return;

	pld = kcalloc(cluster_num,
			sizeof(struct cpu_ctrl_data), GFP_KERNEL);
	if (!pld)
		return;

	mutex_lock(&fbt_mlock);

	if (rescue_second_enable_final && thr->pid == max_blc_pid
				&& thr->buffer_id == max_blc_buffer_id) {

		if (thr->boost_info.t_duration && thr->boost_info.t_duration
				 <= thr->boost_info.t2wnt) {
			fbt_cancel_sjerk();
			sjerk.pid = thr->pid;
			sjerk.identifier = thr->identifier;
			sjerk.active_id = jerk_id;
			sjerk.jerking = 1;
			if (wq_jerk)
				queue_work(wq_jerk, &sjerk.work);
			else
				schedule_work(&sjerk.work);
			goto EXIT;
		} else {
			unsigned long long st2wnt;

			st2wnt = (thr->boost_info.t_duration) ? thr->boost_info.t_duration
				- thr->boost_info.t2wnt : thr->boost_info.target_time;
			fbt_set_sjerk(thr->pid, thr->buffer_id, thr->identifier,
				jerk_id, st2wnt);
		}
	}

	blc_wt = fbt_get_new_base_blc(pld, blc_wt, rescue_enhance_f, rescue_opp_f, rescue_opp_c);
	if (separate_aa_final) {
		blc_wt_b = fbt_get_new_base_blc(pld, blc_wt_b,
			rescue_enhance_f, rescue_opp_f, rescue_opp_c);
		blc_wt_m = fbt_get_new_base_blc(pld, blc_wt_m,
			rescue_enhance_f, rescue_opp_f, rescue_opp_c);
	}

	if (!blc_wt || (separate_aa_final && !(blc_wt_b && blc_wt_m)))
		goto EXIT;

	blc_wt = fbt_limit_capacity(blc_wt, 1);
	if (separate_aa_final) {
		blc_wt_b = fbt_limit_capacity(blc_wt_b, 1);
		blc_wt_m = fbt_limit_capacity(blc_wt_m, 1);
	}

	do_jerk = fbt_check_to_jerk(thr->t_enqueue_start, thr->t_enqueue_end,
		thr->t_dequeue_start, thr->t_dequeue_end,
		thr->dequeue_length,
		thr->pid, thr->buffer_id, thr->tgid);

	fbt_print_rescue_info(thr->pid, thr->buffer_id, thr->t_enqueue_end,
		thr->boost_info.quota_adj, 0, 1, do_jerk,
		blc_wt, thr->boost_info.last_blc);

	switch (do_jerk) {
	case FPSGO_JERK_ONLY_CEILING_WAIT_ENQ:
	case FPSGO_JERK_ONLY_CEILING_WAIT_DEQ:
		fbt_do_jerk_boost(thr, temp_blc, temp_blc_b, temp_blc_m, 0, FPSGO_JERK_FIRST);
		fbt_set_hard_limit_locked(FPSGO_HARD_CEILING, pld);
		fbt_set_ceiling(pld, thr->pid, thr->buffer_id);
		thr->boost_info.cur_stage = FPSGO_JERK_FIRST;
		if (thr->pid == max_blc_pid && thr->buffer_id == max_blc_buffer_id) {
			max_blc_stage = FPSGO_JERK_FIRST;
			fpsgo_systrace_c_fbt_debug(-100, 0, max_blc_stage, "max_blc_stage");
		}
		break;
	case FPSGO_JERK_NEED:
		fbt_do_jerk_boost(thr, blc_wt, blc_wt_b, blc_wt_m, 0, FPSGO_JERK_FIRST);
		thr->boost_info.last_blc = blc_wt;
		if (separate_aa_final) {
			thr->boost_info.last_blc_b = blc_wt_b;
			thr->boost_info.last_blc_m = blc_wt_m;
		}

		fbt_set_hard_limit_locked(FPSGO_HARD_CEILING, pld);
		fbt_set_ceiling(pld, thr->pid, thr->buffer_id);
		thr->boost_info.cur_stage = FPSGO_JERK_FIRST;
		if (thr->pid == max_blc_pid && thr->buffer_id == max_blc_buffer_id) {
			max_blc_stage = FPSGO_JERK_FIRST;
			fpsgo_systrace_c_fbt_debug(-100, 0, max_blc_stage, "max_blc_stage");
		}
		break;
	case FPSGO_JERK_POSTPONE:
		if (jerk)
			jerk->postpone = 1;
		break;
	case FPSGO_JERK_DISAPPEAR:
		goto EXIT;
	default:
		break;
	}
EXIT:
	mutex_unlock(&fbt_mlock);
	kfree(pld);

}

static void fbt_do_jerk(struct work_struct *work)
{
	struct fbt_jerk *jerk;
	struct fbt_proc *proc;
	struct fbt_boost_info *boost;
	struct render_info *thr;
	int tofree = 0;

	jerk = container_of(work, struct fbt_jerk, work);
	if (!jerk || jerk->id < 0 || jerk->id > RESCUE_TIMER_NUM - 1) {
		FPSGO_LOGE("ERROR %d\n", __LINE__);
		return;
	}

	proc = container_of(jerk, struct fbt_proc, jerks[jerk->id]);
	if (!proc || proc->active_jerk_id < 0 ||
		proc->active_jerk_id > RESCUE_TIMER_NUM - 1) {
		FPSGO_LOGE("ERROR %d\n", __LINE__);
		return;
	}

	boost = container_of(proc, struct fbt_boost_info, proc);
	if (!boost) {
		FPSGO_LOGE("ERROR %d\n", __LINE__);
		return;
	}

	thr = container_of(boost, struct render_info, boost_info);
	if (!thr) {
		FPSGO_LOGE("ERROR %d\n", __LINE__);
		return;
	}

	fpsgo_render_tree_lock(__func__);
	fpsgo_thread_lock(&(thr->thr_mlock));

	if (jerk->id != proc->active_jerk_id ||
		jerk->frame_qu_ts != thr->t_enqueue_end ||
		thr->linger != 0)
		goto EXIT;

	fbt_do_jerk_locked(thr, jerk, jerk->id);

EXIT:
	if (!(jerk->postpone))
		jerk->jerking = 0;

	if (thr->linger > 0 && fpsgo_base_is_finished(thr)) {
		tofree = 1;
		fpsgo_del_linger(thr);
	}

	fpsgo_thread_unlock(&(thr->thr_mlock));

	if (tofree)
		kfree(thr);

	fpsgo_render_tree_unlock(__func__);
}

static enum hrtimer_restart fbt_jerk_tfn(struct hrtimer *timer)
{
	struct fbt_jerk *jerk;

	jerk = container_of(timer, struct fbt_jerk, timer);
	if (wq_jerk)
		queue_work(wq_jerk, &jerk->work);
	else
		schedule_work(&jerk->work);
	return HRTIMER_NORESTART;
}

static inline void fbt_init_jerk(struct fbt_jerk *jerk, int id)
{
	jerk->id = id;

	hrtimer_init(&jerk->timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	jerk->timer.function = &fbt_jerk_tfn;
	INIT_WORK(&jerk->work, fbt_do_jerk);
}

static void fbt_init_sjerk(void)
{
	hrtimer_init(&sjerk.timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	sjerk.timer.function = &fbt_jerk_tfn;
	INIT_WORK(&sjerk.work, fbt_do_sjerk);
}

static inline long long llabs(long long val)
{
	return (val < 0) ? -val : val;
}

static unsigned long long fbt_get_next_vsync_locked(
	unsigned long long queue_end)
{
	unsigned long long next_vsync;
	unsigned long mod;
	unsigned long long diff;

	if (vsync_time == 0 || vsync_period == 0) {
		xgf_trace("ERROR no vsync");
		return 0ULL;
	}

	if (queue_end >= vsync_time) {
		diff = queue_end - vsync_time;
		mod = do_div(diff, vsync_period);
		next_vsync = queue_end + vsync_period - mod;

	} else {
		diff = vsync_time - queue_end;
		mod = do_div(diff, vsync_period);
		next_vsync = queue_end + vsync_period + mod;
	}

	if (unlikely(next_vsync < queue_end)) {
		xgf_trace("ERROR when get next_vsync");
		return 0ULL;
	}

	return next_vsync;
}

static const int fbt_varfps_level[] = {30, 45, 60, 75, 90, 105, 120, 144};
static int fbt_get_var_fps(int target_fps)
{
	int ret = fbt_varfps_level[0];
	int i;
	int len = ARRAY_SIZE(fbt_varfps_level);

	for (i = (len - 1) ; i > 0; i--) {
		if (target_fps <= (fbt_varfps_level[i] * 90 / 100))
			continue;

		ret = fbt_varfps_level[i];
		break;
	}

	return ret;
}


static int fbt_filter_frame(long loading, unsigned int target_fps, int blc_wt,
	unsigned int *filter_frames_count, int *filter_index,
	struct fbt_loading_info *filter_loading, int *filtered_blc,
	int render_pid, unsigned long long buffer_id, int window_size, int filter_kmin)
{
	int pre_iter = 0, next_iter = 0, cur_iter = 0;
	int i = 0, tmp_index = 0;
	int ret = FPSGO_FILTER_ACTIVE;
	struct fbt_loading_info *sorted_loading;

	if (!filter_index || !filter_loading || !filtered_blc || !filter_frames_count) {
		FPSGO_LOGE("ERROR %d\n", __LINE__);
		return FPSGO_FILTER_FRAME_ERR;
	}
	if (*filter_index >= FBT_FILTER_MAX_WINDOW) {
		FPSGO_LOGE("ERROR %d\n", __LINE__);
		*filter_index = 0;
		return FPSGO_FILTER_FRAME_ERR;
	}

	sorted_loading = kcalloc(window_size, sizeof(struct fbt_loading_info),
		GFP_KERNEL);

	if (!sorted_loading) {
		FPSGO_LOGE("ERROR OOM %d\n", __LINE__);
		return FPSGO_FILTER_FRAME_ERR;
	}

	cur_iter = *filter_index;
	pre_iter = (*filter_index - 1 + FBT_FILTER_MAX_WINDOW) % FBT_FILTER_MAX_WINDOW;
	next_iter = (*filter_index + 1 + FBT_FILTER_MAX_WINDOW) % FBT_FILTER_MAX_WINDOW;

	filter_loading[*filter_index].target_fps = fbt_get_var_fps(target_fps);

	if (!(filter_loading[pre_iter].target_fps)) {
		filter_loading[*filter_index].loading = loading;
		filter_loading[*filter_index].blc_wt = blc_wt;
		(*filter_frames_count) = 0;
		(*filter_index)++;
		(*filter_index) = (*filter_index) % FBT_FILTER_MAX_WINDOW;
		(*filtered_blc) = blc_wt;

		ret = FPSGO_FILTER_WINDOWS_NOT_ENOUGH;
		goto out;
	}

	if (filter_loading[*filter_index].target_fps == filter_loading[pre_iter].target_fps) {
		filter_loading[*filter_index].loading = loading;
		filter_loading[*filter_index].blc_wt = blc_wt;

		if ((*filter_frames_count) < window_size) {
			(*filter_frames_count)++;
			(*filter_index)++;
			(*filter_index) = (*filter_index) % FBT_FILTER_MAX_WINDOW;
			(*filtered_blc) = blc_wt;

			ret = FPSGO_FILTER_WINDOWS_NOT_ENOUGH;
			goto out;
		}

		for (i = 0; i < window_size; i++) {
			tmp_index = ((*filter_index) - i + FBT_FILTER_MAX_WINDOW) %
				FBT_FILTER_MAX_WINDOW;
			sorted_loading[window_size - i - 1].loading =
				filter_loading[tmp_index].loading;
			sorted_loading[window_size - i - 1].blc_wt =
				filter_loading[tmp_index].blc_wt;
			sorted_loading[window_size - i - 1].index = window_size - i - 1;
			xgf_trace("[%s] tmp_index=%d, sorted_loading[%d]=%llu",
				__func__, tmp_index, window_size - i - 1,
				sorted_loading[window_size - i - 1].loading);
		}

		sort(sorted_loading, window_size, sizeof(struct fbt_loading_info),
			cmp_loading, NULL);
		for (i = 0; i < window_size; i++) {
			xgf_trace("[%s] after_sorted[%d].index=%d, loading=%llu",
				__func__, i, sorted_loading[i].index,
				sorted_loading[i].loading);
		}
		filter_kmin = clamp(filter_kmin, 1, window_size);
		(*filtered_blc) = sorted_loading[filter_kmin - 1].blc_wt;
		fpsgo_systrace_c_fbt_debug(render_pid, buffer_id,
			sorted_loading[filter_kmin - 1].loading, "filter_loading");

		(*filter_index)++;
		(*filter_index) = (*filter_index) % FBT_FILTER_MAX_WINDOW;
	}	else {
		/*reset frame time info*/
		(*filter_frames_count) = 0;
		(*filter_index)++;
		(*filter_index) = (*filter_index) % FBT_FILTER_MAX_WINDOW;
		(*filtered_blc) = blc_wt;

		ret = FPSGO_FILTER_WINDOWS_NOT_ENOUGH;
		goto out;
	}

out:
	kfree(sorted_loading);
	xgf_trace("[%s] enable=%d, window_size=%d, kmin=%d", __func__, filter_frame_enable,
		window_size, filter_kmin);
	xgf_trace("[%s] ret=%d, target_fps=%d, total_count=%d, filter_index=%d",
		__func__, ret, filter_loading[cur_iter].target_fps, *filter_frames_count, cur_iter);
	xgf_trace("[%s] loading=%lld, blc_wt=%d, filtered_blc=%d", __func__,
		filter_loading[cur_iter].loading, filter_loading[cur_iter].blc_wt, (*filtered_blc));

	return ret;
}

static void fbt_check_var(long loading,
		unsigned int target_fps, long long t_cpu_target,
		int *f_iter, struct fbt_frame_info *frame_info,
		unsigned int *floor, int *floor_count, int *reset_floor_bound)
{
	int pre_iter = 0;
	int next_iter = 0;

	if (!f_iter || !frame_info || !floor ||
			!floor_count || !reset_floor_bound) {
		FPSGO_LOGE("ERROR %d\n", __LINE__);
		return;
	}
	if (*f_iter >= WINDOW) {
		FPSGO_LOGE("ERROR %d\n", __LINE__);
		*f_iter = 0;
		return;
	}

	pre_iter = (*f_iter - 1 + WINDOW) % WINDOW;
	next_iter = (*f_iter + 1 + WINDOW) % WINDOW;

	frame_info[*f_iter].target_fps = fbt_get_var_fps(target_fps);

	if (!(frame_info[pre_iter].target_fps)) {
		frame_info[*f_iter].mips = loading;
		xgf_trace("first frame frame_info[%d].mips=%d run_time=%llu",
				*f_iter, frame_info[*f_iter].mips,
				frame_info[*f_iter].running_time);
		(*f_iter)++;
		*f_iter = *f_iter % WINDOW;
		return;
	}

	if (frame_info[*f_iter].target_fps == frame_info[pre_iter].target_fps) {
		long long mips_diff;
		unsigned long long frame_time;
		unsigned long long frame_bound;

		frame_info[*f_iter].mips = loading;
		mips_diff =
			(abs(frame_info[pre_iter].mips -
					frame_info[*f_iter].mips) * 100);
		if (frame_info[*f_iter].mips != 0)
			mips_diff = div64_s64(mips_diff,
					frame_info[*f_iter].mips);
		else
			mips_diff = frame_info[pre_iter].mips;
		mips_diff = MAX(1LL, mips_diff);
		frame_time =
			frame_info[pre_iter].running_time +
			frame_info[*f_iter].running_time;
		frame_time = div64_u64(frame_time,
			(unsigned long long)NSEC_PER_USEC);

		frame_info[*f_iter].mips_diff = (int)mips_diff;

		frame_bound = 21ULL * (unsigned long long)t_cpu_target;
		frame_bound = div64_u64(frame_bound, 10ULL);

		if (mips_diff > variance && frame_time > frame_bound)
			frame_info[*f_iter].count = 1;
		else
			frame_info[*f_iter].count = 0;

		*floor_count = *floor_count +
			frame_info[*f_iter].count - frame_info[next_iter].count;

		xgf_trace(
		"frame_info[%d].mips=%ld diff=%d run=%llu count=%d floor_count=%d"
		, *f_iter, frame_info[*f_iter].mips, mips_diff,
		frame_info[*f_iter].running_time,
		frame_info[*f_iter].count,
		*floor_count);

		if (*floor_count >= floor_bound) {
			int i;
			int array[WINDOW];

			for (i = 0; i < WINDOW; i++)
				array[i] = frame_info[i].mips_diff;
			sort(array, WINDOW, sizeof(int), cmpint, NULL);
			kmin = clamp(kmin, 1, WINDOW);
			*floor = array[kmin - 1];
		}

		/*reset floor check*/
		if (*floor > 0) {
			if (*floor_count == 0) {
				int reset_bound;

				reset_bound =
					5 * frame_info[*f_iter].target_fps;
				(*reset_floor_bound)++;
				*reset_floor_bound =
					min(*reset_floor_bound, reset_bound);

				if (*reset_floor_bound == reset_bound) {
					*floor = 0;
					*reset_floor_bound = 0;
				}
			} else if (*floor_count > 2) {
				*reset_floor_bound = 0;
			}
		}

		(*f_iter)++;
		*f_iter = *f_iter % WINDOW;
	}	else {
		/*reset frame time info*/
		memset(frame_info, 0, WINDOW * sizeof(struct fbt_frame_info));
		*floor_count = 0;
		*f_iter = 0;
	}
}

static void fbt_do_boost(unsigned int blc_wt, int pid,
	unsigned long long buffer_id)
{
	struct cpu_ctrl_data *pld;
	int *clus_opp;
	unsigned int *clus_floor_freq;
	int tgt_opp = 0;
	unsigned int mbhr;
	int mbhr_opp;
	int cluster, i = 0;
	int min_ceiling = 0;

	pld =
		kcalloc(cluster_num, sizeof(struct cpu_ctrl_data),
				GFP_KERNEL);
	if (!pld) {
		FPSGO_LOGE("ERROR OOM %d\n", __LINE__);
		return;
	}

	clus_opp =
		kzalloc(cluster_num * sizeof(int), GFP_KERNEL);
	if (!clus_opp) {
		kfree(pld);
		FPSGO_LOGE("ERROR OOM %d\n", __LINE__);
		return;
	}

	clus_floor_freq =
		kzalloc(cluster_num * sizeof(unsigned int), GFP_KERNEL);
	if (!clus_floor_freq) {
		kfree(pld);
		kfree(clus_opp);
		FPSGO_LOGE("ERROR OOM %d\n", __LINE__);
		return;
	}

	if (cpu_max_freq <= 1)
		fpsgo_systrace_c_fbt(pid, buffer_id, cpu_max_freq,
			"cpu_max_freq");

	for (cluster = 0 ; cluster < cluster_num; cluster++) {
		tgt_opp = fbt_get_opp_by_normalized_cap(blc_wt, cluster);

		clus_floor_freq[cluster] = cpu_dvfs[cluster].power[tgt_opp];
		clus_opp[cluster] = tgt_opp;

		if (cluster == min_cap_cluster
			&& blc_wt <= cpu_dvfs[min_cap_cluster].power[0]) {
			mbhr_opp = max((clus_opp[cluster] - MAX(bhr_opp, bhr_opp_l)), 0);
		} else
			mbhr_opp = max((clus_opp[cluster] - bhr_opp), 0);

		mbhr = clus_floor_freq[cluster] * 100U;
		mbhr = mbhr / cpu_max_freq;
		mbhr = mbhr + (unsigned int)bhr;
		mbhr = (min(mbhr, 100U) * cpu_max_freq);
		mbhr = mbhr / 100U;

		for (i = (nr_freq_cpu - 1); i > 0; i--) {
			if (cpu_dvfs[cluster].power[i] > mbhr)
				break;
		}
		mbhr = cpu_dvfs[cluster].power[i];

		pld[cluster].min = -1;

		if (suppress_ceiling) {
			pld[cluster].max =
				max(mbhr, cpu_dvfs[cluster].power[mbhr_opp]);

			if (cluster == min_cap_cluster) {
				min_ceiling = fbt_get_L_min_ceiling();
				if (min_ceiling && pld[cluster].max <
						min_ceiling)
					pld[cluster].max = min_ceiling;
			}
		} else
			pld[cluster].max = -1;

		base_opp[cluster] = clus_opp[cluster];
	}

	if (cluster_num == 1 || pld[max_cap_cluster].max == -1
		|| bhr_opp == (nr_freq_cpu - 1))
		fbt_set_hard_limit_locked(FPSGO_HARD_NONE, pld);
	else {
		if (limit_policy != FPSGO_LIMIT_NONE)
			fbt_set_hard_limit_locked(FPSGO_HARD_LIMIT, pld);
	}

	if (boost_ta) {
		fbt_set_boost_value(blc_wt);
		fpsgo_systrace_c_fbt(pid, buffer_id, blc_wt, "perf idx");
	}

	fbt_set_ceiling(pld, pid, buffer_id);

	kfree(pld);
	kfree(clus_opp);
	kfree(clus_floor_freq);
}

static void fbt_clear_state(struct render_info *thr)
{
	unsigned int temp_blc = 0;
	int temp_blc_pid = 0;
	unsigned long long temp_blc_buffer_id = 0;
	int temp_blc_dep_num = 0;
	struct fpsgo_loading temp_blc_dep[MAX_DEP_NUM];

	if (!thr || thr->boost_info.cur_stage == FPSGO_JERK_INACTIVE)
		return;

	// the max one is jerking
	if (max_blc_stage != FPSGO_JERK_INACTIVE)
		return;

	if (ultra_rescue)
		fbt_boost_dram(0);

	fbt_find_max_blc(&temp_blc, &temp_blc_pid,
		&temp_blc_buffer_id, &temp_blc_dep_num, temp_blc_dep);
	if (temp_blc)
		fbt_do_boost(temp_blc, temp_blc_pid, temp_blc_buffer_id);

	fpsgo_systrace_c_fbt_debug(thr->pid, thr->buffer_id, temp_blc_pid, "reset");
}

void fbt_set_limit(int cur_pid, unsigned int blc_wt,
	int pid, unsigned long long buffer_id,
	int dep_num, struct fpsgo_loading dep[],
	struct render_info *thread_info, long long runtime)
{
	unsigned int final_blc = blc_wt;
	int final_blc_pid = pid;
	unsigned long long final_blc_buffer_id = buffer_id;
	int final_blc_dep_num = dep_num;

	struct fpsgo_loading *final_blc_dep, *temp_blc_dep;

	final_blc_dep
		= kcalloc(MAX_DEP_NUM, sizeof(struct fpsgo_loading), GFP_KERNEL);
	temp_blc_dep
		= kcalloc(MAX_DEP_NUM, sizeof(struct fpsgo_loading), GFP_KERNEL);

	if (!final_blc_dep || !temp_blc_dep) {
		FPSGO_LOGE("ERROR OOM %d\n", __LINE__);
		goto EXIT;
	}


	if (!(blc_wt > max_blc ||
		(pid == max_blc_pid && buffer_id == max_blc_buffer_id))) {
		fbt_clear_state(thread_info);
		goto EXIT;
	}

	if (dep)
		memcpy(final_blc_dep, dep,
			final_blc_dep_num * sizeof(struct fpsgo_loading));
	else
		final_blc_dep_num = 0;


	if (ultra_rescue)
		fbt_boost_dram(0);

	if (pid == max_blc_pid && buffer_id == max_blc_buffer_id
			&& blc_wt < max_blc) {
		unsigned int temp_blc = 0;
		int temp_blc_pid = 0;
		unsigned long long temp_blc_buffer_id = 0;
		int temp_blc_dep_num = 0;

		fbt_find_ex_max_blc(pid, buffer_id, &temp_blc,
				&temp_blc_pid, &temp_blc_buffer_id,
				&temp_blc_dep_num, temp_blc_dep);
		if (blc_wt && temp_blc > blc_wt && temp_blc_pid
			&& (temp_blc_pid != pid ||
				temp_blc_buffer_id != buffer_id)) {
			fpsgo_systrace_c_fbt_debug(pid, buffer_id,
						temp_blc_pid, "replace");

			final_blc = temp_blc;
			final_blc_pid = temp_blc_pid;
			final_blc_buffer_id = temp_blc_buffer_id;
			final_blc_dep_num = temp_blc_dep_num;
			memcpy(final_blc_dep, temp_blc_dep,
				temp_blc_dep_num * sizeof(struct fpsgo_loading));

			goto BOOST;
		}
	}

BOOST:
	if (cur_pid != pid)
		goto EXIT2;

	if (boosted_group && !boost_ta) {
		fbt_clear_boost_value();
		if (thread_info)
			fpsgo_systrace_c_fbt(thread_info->pid, thread_info->buffer_id,
						0, "perf idx");
		boosted_group = 0;
	}

	fbt_do_boost(final_blc, final_blc_pid, final_blc_buffer_id);

	fbt_cancel_sjerk();

EXIT2:
	max_blc = final_blc;
	max_blc_cur = final_blc;
	max_blc_pid = final_blc_pid;
	max_blc_buffer_id = final_blc_buffer_id;
	max_blc_stage = FPSGO_JERK_INACTIVE;
	max_blc_dep_num = final_blc_dep_num;
	memcpy(max_blc_dep, final_blc_dep,
		max_blc_dep_num * sizeof(struct fpsgo_loading));

	fpsgo_systrace_c_fbt_debug(-100, 0, max_blc, "max_blc");
	fpsgo_systrace_c_fbt_debug(-100, 0, max_blc_pid, "max_blc_pid");
	fpsgo_systrace_c_fbt_debug(-100, 0, max_blc_buffer_id, "max_blc_buffer_id");
	fpsgo_systrace_c_fbt_debug(-100, 0, max_blc_stage, "max_blc_stage");
	fpsgo_systrace_c_fbt_debug(-100, 0, max_blc_dep_num, "max_blc_dep_num");
	print_dep(__func__, "max_blc_dep",
		0, 0,
		max_blc_dep, max_blc_dep_num);

EXIT:
	kfree(temp_blc_dep);
	kfree(final_blc_dep);
}

static unsigned int fbt_get_max_userlimit_freq(void)
{
	unsigned int max_cap = 0U;
	unsigned int limited_cap;
	int i;
	int *clus_max_idx;
	int max_cluster = 0;

	clus_max_idx =
		kcalloc(cluster_num, sizeof(int), GFP_KERNEL);

	if (!clus_max_idx)
		return 100U;

	for (i = 0; i < cluster_num; i++)
		clus_max_idx[i] = 0;

	mutex_lock(&fbt_mlock);
	for (i = 0; i < cluster_num; i++) {
		if (clus_max_idx[i] < 0 || clus_max_idx[i] >= nr_freq_cpu)
			continue;

		clus_max_cap[i] = cpu_dvfs[i].capacity_ratio[clus_max_idx[i]];
		if (clus_max_cap[i] > max_cap) {
			max_cap = clus_max_cap[i];
			max_cluster = i;
		}
	}

	for (i = 0 ; i < cluster_num; i++)
		fpsgo_systrace_c_fbt_debug(-100, 0, clus_max_cap[i],
				"cluster%d max cap", i);
	fpsgo_systrace_c_fbt_debug(-100, 0, max_cluster, "max_cluster");

	max_cap = clus_max_cap[max_cluster];
	mutex_unlock(&fbt_mlock);

	limited_cap = max_cap;

	kfree(clus_max_idx);

	return limited_cap;
}

static unsigned long long fbt_cal_t2wnt(long long t_cpu_target,
	unsigned long long ts,
	unsigned long long next_vsync, int percent)
{
	unsigned long long rescue_length;

	rescue_length = t_cpu_target * (unsigned long long)percent;
	rescue_length = div64_u64(rescue_length, 100ULL);

	if (next_vsync - ts < rescue_length)
		return 0ULL;

	return (next_vsync - rescue_length - ts);
}

static int fbt_maybe_vsync_aligned(unsigned long long queue_start)
{
	unsigned long long diff;

	diff = (vsync_time > queue_start)
		? (vsync_time - queue_start)
		: (queue_start - vsync_time);

	if (diff > TIME_1MS)
		return 0;
	return 1;
}

static unsigned long long fbt_get_t2wnt(int target_fps,
		unsigned long long queue_start, int always_running)
{
	unsigned long long next_vsync, queue_end, rescue_length;
	unsigned long long t2wnt = 0ULL;
	unsigned long long ts = fpsgo_get_time();
	unsigned long long t_cpu_target;

	mutex_lock(&fbt_mlock);

	target_fps = min_t(int, target_fps, TARGET_UNLIMITED_FPS);

	t_cpu_target = (unsigned long long)FBTCPU_SEC_DIVIDER;
	t_cpu_target = div64_u64(t_cpu_target,
		(unsigned long long)target_fps);
	queue_end = queue_start + t_cpu_target;
	next_vsync = fbt_get_next_vsync_locked(queue_end);

	if (next_vsync == 0ULL)
		goto exit;

	if (!always_running && fbt_maybe_vsync_aligned(queue_start))
		t2wnt = fbt_cal_t2wnt(t_cpu_target,
			ts, next_vsync, short_min_rescue_p);
	else {
		t2wnt = fbt_cal_t2wnt(t_cpu_target,
				ts, next_vsync, rescue_percent);
		if (t2wnt == 0ULL)
			goto ERROR;

		if (t_cpu_target > t2wnt) {
			t2wnt = t_cpu_target;

			rescue_length = next_vsync - t2wnt - queue_start;
			if (rescue_length <= short_rescue_ns)
				t2wnt = fbt_cal_t2wnt(t_cpu_target,
					ts, next_vsync,
					min_rescue_percent);
		}
	}
ERROR:
	t2wnt = MAX(1ULL, t2wnt);

exit:
	mutex_unlock(&fbt_mlock);

	return t2wnt;
}

static int fbt_is_always_running(long long running_time,
	long long target_time)
{
	unsigned long long target = run_time_percent * target_time;

	target = div64_u64(target, 100ULL);

	if (running_time > target)
		return 1;

	return 0;
}

static int fbt_get_next_jerk(int cur_id)
{
	int ret_id;

	ret_id = cur_id + 1;
	if (ret_id >= RESCUE_TIMER_NUM)
		ret_id = 0;

	return ret_id;
}

static int update_quota(struct fbt_boost_info *boost_info, int target_fps,
	unsigned long long t_Q2Q_ns, unsigned long long t_enq_len_ns,
	unsigned long long t_deq_len_ns, int target_fpks, int cooler_on,
	struct fpsgo_boost_attr *attr)
{
	int rm_idx, new_idx, first_idx;
	int gcc_fps_margin_final = attr->gcc_fps_margin_by_pid;
	long long target_time = div64_s64(1000000000, target_fpks + gcc_fps_margin_final * 10);
	int window_cnt;
	int s32_t_Q2Q = nsec_to_usec(t_Q2Q_ns);
	int s32_t_enq_len = nsec_to_usec(t_enq_len_ns);
	int s32_t_deq_len = nsec_to_usec(t_deq_len_ns);
	int avg = 0, i, quota_adj = 0, qr_quota = 0;
	long long std_square = 0;
	int s32_target_time;
	int gcc_enq_bound_thrs_final = attr->gcc_enq_bound_thrs_by_pid;
	int gcc_enq_bound_quota_final = attr->gcc_enq_bound_quota_by_pid;
	int gcc_deq_bound_quota_final = attr->gcc_deq_bound_quota_by_pid;
	int gcc_deq_bound_thrs_final = attr->gcc_deq_bound_thrs_by_pid;

	if (!gcc_fps_margin_final && target_fps == 60)
		target_time = max(target_time, (long long)vsync_duration_us_60);
	if (!gcc_fps_margin_final && target_fps == 90)
		target_time = max(target_time, (long long)vsync_duration_us_90);
	if (!gcc_fps_margin_final && target_fps == 120)
		target_time = max(target_time, (long long)vsync_duration_us_120);
	if (!gcc_fps_margin_final && target_fps == 144)
		target_time = max(target_time, (long long)vsync_duration_us_144);

	s32_target_time = target_time;
	window_cnt = target_fps * gcc_window_size;
	do_div(window_cnt, 100);

	if (target_fps != boost_info->quota_fps && !cooler_on) {
		boost_info->quota_cur_idx = -1;
		boost_info->quota_cnt = 0;
		boost_info->quota = 0;
		boost_info->quota_fps = target_fps;
		boost_info->enq_sum = 0;
		boost_info->deq_sum = 0;
	}

	if (boost_info->enq_avg * 100 > s32_target_time * gcc_enq_bound_thrs_final ||
		boost_info->deq_avg * 100 > s32_target_time * gcc_deq_bound_thrs_final) {
		boost_info->quota_cur_idx = -1;
		boost_info->quota_cnt = 0;
		boost_info->quota = 0;
		boost_info->enq_sum = 0;
		boost_info->deq_sum = 0;
	}

	new_idx = boost_info->quota_cur_idx + 1;

	if (new_idx >= QUOTA_MAX_SIZE)
		new_idx -= QUOTA_MAX_SIZE;

	if (boost_info->enq_avg * 100 > s32_target_time * gcc_enq_bound_thrs_final)
		boost_info->quota_raw[new_idx] =
		target_time * gcc_enq_bound_quota_final / 100;
	else if (boost_info->deq_avg * 100 > s32_target_time * gcc_deq_bound_thrs_final)
		boost_info->quota_raw[new_idx] = target_time * gcc_deq_bound_quota_final / 100;
	else
		boost_info->quota_raw[new_idx] = target_time - s32_t_Q2Q;

	boost_info->quota += boost_info->quota_raw[new_idx];

	boost_info->enq_raw[new_idx] = s32_t_enq_len;
	boost_info->enq_sum += boost_info->enq_raw[new_idx];
	boost_info->deq_raw[new_idx] = s32_t_deq_len;
	boost_info->deq_sum += boost_info->deq_raw[new_idx];

	if (boost_info->quota_cnt >= window_cnt) {
		rm_idx = new_idx - window_cnt;
		if (rm_idx < 0)
			rm_idx += QUOTA_MAX_SIZE;

		first_idx = rm_idx + 1;
		if (first_idx >= QUOTA_MAX_SIZE)
			first_idx -= QUOTA_MAX_SIZE;

		boost_info->quota -= boost_info->quota_raw[rm_idx];
		boost_info->enq_sum -= boost_info->enq_raw[rm_idx];
		boost_info->deq_sum -= boost_info->deq_raw[rm_idx];
	} else {
		first_idx = new_idx - boost_info->quota_cnt;
		if (first_idx < 0)
			first_idx += QUOTA_MAX_SIZE;

		boost_info->quota_cnt += 1;
	}
	boost_info->quota_cur_idx = new_idx;

	/* remove outlier */
	avg = boost_info->quota / boost_info->quota_cnt;
	boost_info->enq_avg = boost_info->enq_sum / boost_info->quota_cnt;
	boost_info->deq_avg = boost_info->deq_sum / boost_info->quota_cnt;

	if (first_idx <= new_idx)
		for (i = first_idx; i <= new_idx; i++)
			std_square += (long long)(boost_info->quota_raw[i] - avg) *
			(long long)(boost_info->quota_raw[i] - avg);
	else {
		for (i = first_idx; i < QUOTA_MAX_SIZE; i++)
			std_square += (long long)(boost_info->quota_raw[i] - avg) *
			(long long)(boost_info->quota_raw[i] - avg);
		for (i = 0; i <= new_idx; i++)
			std_square += (long long)(boost_info->quota_raw[i] - avg) *
			(long long)(boost_info->quota_raw[i] - avg);
	}
	do_div(std_square, boost_info->quota_cnt);

	if (first_idx <= new_idx) {
		for (i = first_idx; i <= new_idx; i++) {
			if ((boost_info->quota_raw[i] - (long long)avg) *
				(boost_info->quota_raw[i] - (long long)avg) <
				(long long)gcc_std_filter * (long long)gcc_std_filter * std_square
				|| boost_info->quota_raw[i] > -s32_target_time)
				quota_adj += boost_info->quota_raw[i];

			qr_quota += (boost_info->quota_raw[i] < -s32_target_time) ?
				boost_info->quota_raw[i] % s32_target_time :
				boost_info->quota_raw[i];
		}
	} else {
		for (i = first_idx; i < QUOTA_MAX_SIZE ; i++) {
			if ((boost_info->quota_raw[i] - (long long)avg) *
				(boost_info->quota_raw[i] - (long long)avg) <
				(long long)gcc_std_filter * (long long)gcc_std_filter * std_square
				|| boost_info->quota_raw[i] > -s32_target_time)
				quota_adj += boost_info->quota_raw[i];

			qr_quota += (boost_info->quota_raw[i] < -s32_target_time) ?
				boost_info->quota_raw[i] % s32_target_time :
				boost_info->quota_raw[i];
		}
		for (i = 0; i <= new_idx ; i++) {
			if ((boost_info->quota_raw[i] - (long long)avg) *
				(boost_info->quota_raw[i] - (long long)avg) <
				(long long)gcc_std_filter * (long long)gcc_std_filter * std_square
				|| boost_info->quota_raw[i] > -s32_target_time)
				quota_adj += boost_info->quota_raw[i];

			qr_quota += (boost_info->quota_raw[i] < -s32_target_time) ?
				boost_info->quota_raw[i] % s32_target_time :
				boost_info->quota_raw[i];
		}
	}

	boost_info->quota_adj = quota_adj;

	/* default: mod each frame */
	if (qr_mod_frame)
		boost_info->quota_mod = qr_quota % s32_target_time;
	else if (qr_filter_outlier)
		boost_info->quota_mod = boost_info->quota_adj;
	else
		boost_info->quota_mod = boost_info->quota;

	/* clamp if quota < -target_time */
	if (boost_info->quota_mod < -s32_target_time)
		boost_info->quota_mod = -s32_target_time;

	fpsgo_main_trace(
		"%s raw[%d]:%d raw[%d]:%d window_cnt:%d target_fpks:%d cnt:%d sum:%d avg:%d std_sqr:%lld quota:%d mod:%d enq:%d enq_avg:%d deq:%d deq_avg:%d",
		__func__, first_idx, boost_info->quota_raw[first_idx],
		new_idx, boost_info->quota_raw[new_idx], window_cnt, target_fpks,
		boost_info->quota_cnt, boost_info->quota, avg, std_square, quota_adj,
		boost_info->quota_mod, s32_t_enq_len, boost_info->enq_avg,
		s32_t_deq_len, boost_info->deq_avg);

	return s32_target_time;
}


int fbt_eva_gcc(struct fbt_boost_info *boost_info,
		int target_fps, int fps_margin, unsigned long long t_Q2Q,
		int blc_wt, long long t_cpu, int target_fpks,
		int max_iso_cap, int cooler_on,
		int pid, struct fpsgo_boost_attr *attr)
{
	int gcc_fps_margin_final = attr->gcc_fps_margin_by_pid;
	long long target_time = div64_s64(1000000000, target_fpks + gcc_fps_margin_final * 10);
	int gcc_down_window, gcc_up_window;
	int quota = INT_MAX;
	int weight_t_gpu = boost_info->quantile_gpu_time > 0 ?
		nsec_to_usec(boost_info->quantile_gpu_time) : -1;
	int weight_t_cpu = boost_info->quantile_cpu_time > 0 ?
		nsec_to_usec(boost_info->quantile_cpu_time) : -1;
	int ret = 0;
	int gcc_down_sec_pct_final = attr->gcc_down_sec_pct_by_pid;
	int gcc_up_sec_pct_final = attr->gcc_up_sec_pct_by_pid;
	int gcc_reserved_down_quota_pct_final = attr->gcc_reserved_down_quota_pct_by_pid;
	int gcc_reserved_up_quota_pct_final = attr->gcc_reserved_up_quota_pct_by_pid;
	int gcc_down_step_final = attr->gcc_down_step_by_pid;
	int gcc_up_step_final = attr->gcc_up_step_by_pid;

	if (!gcc_fps_margin_final && target_fps == 60)
		target_time = max(target_time, (long long)vsync_duration_us_60);
	if (!gcc_fps_margin_final && target_fps == 90)
		target_time = max(target_time, (long long)vsync_duration_us_90);
	if (!gcc_fps_margin_final && target_fps == 120)
		target_time = max(target_time, (long long)vsync_duration_us_120);
	if (!gcc_fps_margin_final && target_fps == 144)
		target_time = max(target_time, (long long)vsync_duration_us_144);

	gcc_down_window = target_fps * gcc_down_sec_pct_final;
	do_div(gcc_down_window, 100);
	if (gcc_down_window <= 0) {
		gcc_down_window = 1;
		FPSGO_LOGE(
		"%s error: pid:%d, target_fps:%d, gcc_down_sec_pct:%d",
		__func__, pid, target_fps, gcc_down_sec_pct_final);
	}
	gcc_up_window = target_fps * gcc_up_sec_pct_final;
	do_div(gcc_up_window, 100);
	if (gcc_up_window <= 0) {
		gcc_up_window = 1;
		FPSGO_LOGE(
		"%s error: pid:%d, target_fps:%d, gcc_up_sec_pct:%d",
		__func__, pid, target_fps, gcc_up_sec_pct_final);
	}

	if (boost_info->gcc_target_fps != target_fps && !cooler_on) {
		boost_info->gcc_target_fps = target_fps;
		boost_info->correction = 0;
		boost_info->gcc_count = 1;
	} else {
		boost_info->gcc_count++;
	}

	if ((boost_info->gcc_count) % gcc_up_window == 0 &&
		(boost_info->gcc_count) % gcc_down_window == 0) {

		ret = 100;

		quota = boost_info->quota_adj;

		if (quota * 100 >= target_time * gcc_reserved_down_quota_pct_final)
			goto check_deboost;
		if (quota * 100 + target_time * gcc_reserved_up_quota_pct_final <= 0)
			goto check_boost;

		goto done;
	}

check_deboost:
	if ((boost_info->gcc_count) % gcc_down_window == 0) {
		ret += 10;

		quota = boost_info->quota_adj;

		if (fps_margin > 0) {
			ret += 1;
			goto done;
		}

		if (quota * 100 < target_time * gcc_reserved_down_quota_pct_final) {
			ret += 2;
			goto done;
		}

		if (gcc_check_quota_trend &&
			boost_info->gcc_quota > quota) {
			ret += 3;
			goto done;
		}

		boost_info->correction -= gcc_down_step_final;

		goto done;
	}

check_boost:
	if ((boost_info->gcc_count) % gcc_up_window == 0) {
		ret += 20;

		quota = boost_info->quota_adj;

		if (fps_margin > 0) {
			ret += 1;
			goto done;
		}

		if (quota * 100 + target_time * gcc_reserved_up_quota_pct_final > 0) {
			ret += 2;
			goto done;
		}

		if (gcc_check_quota_trend &&
			boost_info->gcc_quota < quota) {
			ret += 3;
			goto done;
		}

		if (weight_t_gpu > 0 && weight_t_cpu > 0 &&
				weight_t_gpu * 100 > target_time * gcc_gpu_bound_time &&
				target_time * gcc_gpu_bound_time > weight_t_cpu * 100 &&
				nsec_to_usec(t_cpu) * 100 < target_time * gcc_gpu_bound_time) {
			ret += 5;
			goto done;
		}

		if (nsec_to_usec(t_cpu) * 100 < target_time * gcc_cpu_unknown_sleep) {
			ret += 6;
			goto done;
		}

		boost_info->correction += gcc_up_step_final;
	}

done:

	if (quota != INT_MAX)
		boost_info->gcc_quota = quota;

	if ((boost_info->correction) + blc_wt > max_iso_cap + gcc_upper_clamp)
		(boost_info->correction) = max_iso_cap + gcc_upper_clamp - blc_wt;
	else if ((boost_info->correction) + blc_wt < 0)
		(boost_info->correction) = -blc_wt;

	return ret;
}

extern bool mtk_get_gpu_loading(unsigned int *pLoading);

static unsigned int fbt_cal_blc(long *aa, unsigned long long t_cpu,
				unsigned long long target_time, unsigned long long t_q2q)
{
	unsigned int blc_wt = 0U;
	unsigned long long temp_blc;

	if (t_cpu > 0 && t_q2q > 0) {
		long long new_aa;

		new_aa = div64_s64((*aa) * t_cpu, t_q2q);
		*aa = new_aa;
		temp_blc = new_aa;
		if (target_time > t_q2q && aa_retarget)
			do_div(temp_blc, (unsigned int)t_q2q);
		else
			do_div(temp_blc, (unsigned int)target_time);
		blc_wt = (unsigned int)temp_blc;
	} else {
		temp_blc = *aa;
		do_div(temp_blc, (unsigned int)target_time);
		blc_wt = (unsigned int)temp_blc;
	}

	return blc_wt;
}

static int fbt_boost_policy(
	long long t_cpu_cur,
	long long target_time,
	unsigned int target_fps,
	unsigned int fps_margin,
	struct render_info *thread_info,
	unsigned long long ts,
	long aa, unsigned int target_fpks, int cooler_on)
{
	unsigned int blc_wt = 0U;
	unsigned int blc_wt_b = 0U;
	unsigned int blc_wt_m = 0U;
	unsigned long long t1, t2, t_Q2Q;
	unsigned long long cur_ts;
	struct fbt_boost_info *boost_info;
	int pid;
	unsigned long long buffer_id;
	struct hrtimer *timer;
	u64 t2wnt = 0ULL;
	int active_jerk_id = 0;
	long long rescue_target_t, qr_quota_adj;
	int isolation_cap = 100, limit_max_cap = 100;
	int filter_ret = 0;
	long aa_n, aa_b, aa_m;
	int is_filter_frame_active = thread_info->attr.filter_frame_enable_by_pid;
	int ff_window_Size = thread_info->attr.filter_frame_window_size_by_pid;
	int ff_kmin = thread_info->attr.filter_frame_kmin_by_pid;
	int separate_aa_final = thread_info->attr.separate_aa_by_pid;
	int max_cap = 100, max_cap_b = 100, max_cap_m = 100;
	int qr_enable_active = thread_info->attr.qr_enable_by_pid;
	int gcc_enable_active = thread_info->attr.gcc_enable_by_pid;
	int qr_t2wnt_x_final = thread_info->attr.qr_t2wnt_x_by_pid;
	int qr_t2wnt_y_n_final = thread_info->attr.qr_t2wnt_y_n_by_pid;
	int qr_t2wnt_y_p_final = thread_info->attr.qr_t2wnt_y_p_by_pid;
	int blc_boost_final = thread_info->attr.blc_boost_by_pid;

	if (!thread_info) {
		FPSGO_LOGE("ERROR %d\n", __LINE__);
		return 0;
	}

	cur_ts = fpsgo_get_time();

	pid = thread_info->pid;
	buffer_id = thread_info->buffer_id;
	boost_info = &(thread_info->boost_info);

	mutex_lock(&fbt_mlock);

	t1 = (unsigned long long)t_cpu_cur;
	t1 = nsec_to_100usec(t1);
	t2 = target_time;
	t2 = nsec_to_100usec(t2);
	t_Q2Q = thread_info->Q2Q_time;
	t_Q2Q = nsec_to_100usec(t_Q2Q);
	aa_n = aa;
	/* aa is original loading, while aa_n is loading multiply (tcpu/q2qtime) */

	if (separate_aa_final && cluster_num > 1) {
		if (boost_info->cl_loading) {
			aa_b = boost_info->cl_loading[max_cap_cluster];
			aa_m = boost_info->cl_loading[sec_cap_cluster];
		} else {
			aa_b = aa_n;
			aa_m = aa_n;
		}
	}

	if (aa_n < 0) {
		mutex_lock(&blc_mlock);
		if (thread_info->p_blc)
			blc_wt = thread_info->p_blc->blc;
		mutex_unlock(&blc_mlock);
		aa_n = 0;
	} else
		blc_wt = fbt_cal_blc(&aa_n, t1, t2, t_Q2Q);

	if (separate_aa_final) {
		if (aa_b < 0 || aa_m < 0) {
			mutex_lock(&blc_mlock);
			if (thread_info->p_blc) {
				blc_wt_b = thread_info->p_blc->blc_b;
				blc_wt_m = thread_info->p_blc->blc_m;
			}
			mutex_unlock(&blc_mlock);
			aa_b = 0;
			aa_m = 0;
		} else {
			blc_wt_b = fbt_cal_blc(&aa_b, t1, t2, t_Q2Q);
			blc_wt_m = fbt_cal_blc(&aa_m, t1, t2, t_Q2Q);
		}
	}

	xgf_trace("perf_index=%d aa=%lld run=%llu target=%llu Q2Q=%llu",
		blc_wt, aa_n, t1, t2, t_Q2Q);
	fpsgo_systrace_c_fbt_debug(pid, buffer_id, aa_n, "aa");
	if (separate_aa_final) {
		fpsgo_systrace_c_fbt_debug(pid, buffer_id, aa_b, "aa_b");
		fpsgo_systrace_c_fbt_debug(pid, buffer_id, aa_m, "aa_m");
	}

	if (variance_control_enable) {
		fbt_check_var(aa, target_fps, nsec_to_usec(target_time),
			&(boost_info->f_iter),
			&(boost_info->frame_info[0]),
			&(boost_info->floor),
			&(boost_info->floor_count),
			&(boost_info->reset_floor_bound));
		fpsgo_systrace_c_fbt(pid, buffer_id, boost_info->floor, "variance");
	}

	blc_wt = clamp(blc_wt, 1U, 100U);
	if (separate_aa_final) {
		blc_wt_b = clamp(blc_wt_b, 1U, 100U);
		blc_wt_m = clamp(blc_wt_m, 1U, 100U);
	}

	if (variance_control_enable) {
		if (boost_info->floor > 1) {
			int orig_blc = blc_wt;

			blc_wt = (blc_wt * (boost_info->floor + 100)) / 100U;
			blc_wt = clamp(blc_wt, 1U, 100U);
			blc_wt = fbt_must_enhance_floor(blc_wt, orig_blc, floor_opp);
		}
	}

	if (is_filter_frame_active) {
		fpsgo_systrace_c_fbt(pid, buffer_id, blc_wt, "before_filter");
		filter_ret = fbt_filter_frame(aa_n, target_fps, blc_wt,
			&(boost_info->filter_frames_count), &(boost_info->filter_index),
			(boost_info->filter_loading), &(boost_info->filter_blc), pid, buffer_id,
			ff_window_Size, ff_kmin);
		xgf_trace("[FILTER] ret=%d, blc_wt=%d", filter_ret, boost_info->filter_blc);

		blc_wt = boost_info->filter_blc;
		fpsgo_systrace_c_fbt(pid, buffer_id, blc_wt, "after_filter");
		if (separate_aa_final) {
			fpsgo_systrace_c_fbt(pid, buffer_id, blc_wt_b, "before_filter_b");
			filter_ret = fbt_filter_frame(aa_b, target_fps, blc_wt_b,
			&(boost_info->filter_frames_count_b), &(boost_info->filter_index_b),
			(boost_info->filter_loading_b), &(boost_info->filter_blc), pid, buffer_id,
			ff_window_Size, ff_kmin);
			blc_wt_b = boost_info->filter_blc;
			fpsgo_systrace_c_fbt(pid, buffer_id, blc_wt_b, "after_filter_b");

			fpsgo_systrace_c_fbt(pid, buffer_id, blc_wt_m, "before_filter_m");
			filter_ret = fbt_filter_frame(aa_m, target_fps, blc_wt_m,
			&(boost_info->filter_frames_count_m), &(boost_info->filter_index_m),
			(boost_info->filter_loading_m), &(boost_info->filter_blc), pid, buffer_id,
			ff_window_Size, ff_kmin);
			blc_wt_m = boost_info->filter_blc;
			fpsgo_systrace_c_fbt(pid, buffer_id, blc_wt_m, "after_filter_m");
		}
	}

	/* update quota */
	if (qr_enable_active || gcc_enable_active) {
		int s32_target_time = update_quota(boost_info,
				target_fps,
				thread_info->Q2Q_time,
				thread_info->enqueue_length_real,
				thread_info->dequeue_length,
				target_fpks, cooler_on, &(thread_info->attr));

		if (qr_debug)
			fpsgo_systrace_c_fbt(pid, buffer_id, boost_info->quota, "quota");
		fpsgo_systrace_c_fbt(pid, buffer_id, s32_target_time, "gcc_target_time");
		fpsgo_systrace_c_fbt(pid, buffer_id, boost_info->quota_adj, "quota_adj");
		fpsgo_systrace_c_fbt(pid, buffer_id, boost_info->quota_mod, "quota_mod");
	}

	limit_max_cap = fbt_get_limit_capacity(0);

	if (gcc_enable_active && (!gcc_hwui_hint ||
			thread_info->hwui != RENDER_INFO_HWUI_TYPE)) {
		int gcc_boost;

		gcc_boost = fbt_eva_gcc(
				boost_info,
				target_fps, fps_margin, thread_info->Q2Q_time,
				blc_wt, t_cpu_cur, target_fpks,
				limit_max_cap, cooler_on, pid, &(thread_info->attr));
		fpsgo_systrace_c_fbt(pid, buffer_id, boost_info->gcc_count, "gcc_count");
		fpsgo_systrace_c_fbt(pid, buffer_id, gcc_boost, "gcc_boost");
		fpsgo_systrace_c_fbt(pid, buffer_id, boost_info->correction, "correction");
		fpsgo_systrace_c_fbt(pid, buffer_id, blc_wt, "before correction");
		if (gcc_positive_clamp && boost_info->correction >= 0)
			fpsgo_systrace_c_fbt(pid, buffer_id, 0, "correction");
		else if (fps_margin && boost_info->correction < 0)
			fpsgo_systrace_c_fbt(pid, buffer_id, 0, "correction");
		else {
			blc_wt = clamp((int)blc_wt + boost_info->correction, 1, 100);
			if (separate_aa_final) {
				blc_wt_b = clamp((int)blc_wt_b + boost_info->correction, 1, 100);
				blc_wt_m = clamp((int)blc_wt_m + boost_info->correction, 1, 100);
			}
		}

	}

	if (blc_boost_final) {
		fpsgo_systrace_c_fbt(pid, buffer_id, blc_wt, "before boost");
		fpsgo_systrace_c_fbt(pid, buffer_id, blc_boost_final, "blc_boost");
		blc_wt = blc_wt * blc_boost_final / 100;
	}

	blc_wt = fbt_limit_capacity(blc_wt, 0);
	blc_wt = fbt_limit_capacity_isolation(blc_wt, &isolation_cap);
	if (separate_aa_final) {
		if (blc_boost_final) {
			fpsgo_systrace_c_fbt(pid, buffer_id, blc_wt_b, "before boost b");
			blc_wt_b = blc_wt_b * blc_boost_final / 100;
			fpsgo_systrace_c_fbt(pid, buffer_id, blc_wt_m, "before boost m");
			blc_wt_m = blc_wt_m * blc_boost_final / 100;
		}
		blc_wt_b = fbt_limit_capacity(blc_wt_b, 0);
		blc_wt_b = fbt_limit_capacity_isolation(blc_wt_b, &isolation_cap);
		blc_wt_m = fbt_limit_capacity(blc_wt_m, 0);
		blc_wt_m = fbt_limit_capacity_isolation(blc_wt_m, &isolation_cap);
	}

	fbt_cal_min_max_cap(thread_info, blc_wt, blc_wt_b, blc_wt_m, 0, pid, buffer_id,
		&blc_wt, &blc_wt_b, &blc_wt_m, &max_cap, &max_cap_b, &max_cap_m);

	if (boost_info->sbe_rescue == 0) {
		fbt_set_limit(pid, blc_wt, pid, buffer_id,
			thread_info->dep_valid_size, thread_info->dep_arr, thread_info, t_cpu_cur);

		if (!boost_ta) {
			fbt_set_min_cap_locked(thread_info, blc_wt, blc_wt_b, blc_wt_m,
				max_cap, max_cap_b, max_cap_m, FPSGO_JERK_INACTIVE);
		}
	}

	boost_info->target_fps = target_fps;
	boost_info->target_time = target_time;
	boost_info->last_blc = blc_wt;
	boost_info->last_normal_blc = blc_wt;
	if (separate_aa_final) {
		boost_info->last_blc_b = blc_wt_b;
		boost_info->last_normal_blc_b = blc_wt_b;
		boost_info->last_blc_m = blc_wt_m;
		boost_info->last_normal_blc_m = blc_wt_m;
	}
	boost_info->cur_stage = FPSGO_JERK_INACTIVE;
	mutex_unlock(&fbt_mlock);

	mutex_lock(&blc_mlock);
	if (thread_info->p_blc) {
		thread_info->p_blc->blc = blc_wt;
		if (separate_aa_final) {
			thread_info->p_blc->blc_b = blc_wt_b;
			thread_info->p_blc->blc_m = blc_wt_m;
		}
		thread_info->p_blc->dep_num = thread_info->dep_valid_size;
		if (thread_info->dep_arr)
			memcpy(thread_info->p_blc->dep, thread_info->dep_arr,
					thread_info->dep_valid_size * sizeof(struct fpsgo_loading));
		else
			thread_info->p_blc->dep_num = 0;
	}
	mutex_unlock(&blc_mlock);

	if (blc_wt) {
		 /* ignore hwui hint || not hwui */
		if (qr_enable_active && (!qr_hwui_hint ||
			thread_info->hwui != RENDER_INFO_HWUI_TYPE)) {
			rescue_target_t = div64_s64(1000000, target_fps); /* unit:1us */

			/* t2wnt = target_time * (1+x) + quota * y */
			if (qr_debug)
				fpsgo_systrace_c_fbt(pid, buffer_id,
				rescue_target_t * 1000, "t2wnt");

			/* rescue_target_t, unit: 1us */
			rescue_target_t = (qr_t2wnt_x_final) ?
				rescue_target_t * 10 * (100 + qr_t2wnt_x_final) :
				rescue_target_t * 1000;

			if (boost_info->quota_mod > 0) { /* qr_quota, unit: 1us */
				/* qr_t2wnt_y_p: percentage */
				qr_quota_adj = (qr_t2wnt_y_p_final != 100) ?
					(long long)boost_info->quota_mod * 10 *
					(long long)qr_t2wnt_y_p_final :
					(long long)boost_info->quota_mod * 1000;
			} else {
				 /* qr_t2wnt_y_n: percentage */
				qr_quota_adj = (qr_t2wnt_y_n_final != 0) ?
					(long long)boost_info->quota_mod * 10 *
					(long long)qr_t2wnt_y_n_final : 0;
			}
			t2wnt = (rescue_target_t + qr_quota_adj > 0)
				? rescue_target_t + qr_quota_adj : 0;

			if (cur_ts > ts) {
				if (t2wnt > cur_ts - ts) {
					if (qr_debug)
						fpsgo_systrace_c_fbt(pid,
						buffer_id, t2wnt, "t2wnt_before");
					t2wnt -= (cur_ts - ts);
				} else
					t2wnt = 1;
			}

			t2wnt = MAX(1ULL, t2wnt);
			fpsgo_systrace_c_fbt(pid, buffer_id, t2wnt, "t2wnt_adjust");
		} else {
			t2wnt = (u64) fbt_get_t2wnt(target_fps, ts,
				fbt_is_always_running(t_cpu_cur, target_time));
			fpsgo_systrace_c_fbt(pid, buffer_id, t2wnt, "t2wnt");
		}

		boost_info->t2wnt = t2wnt;

		if (t2wnt) {
			if (t2wnt == 1ULL) {
				active_jerk_id = fbt_get_next_jerk(
					boost_info->proc.active_jerk_id);
				boost_info->proc.active_jerk_id = active_jerk_id;
				fbt_do_jerk_locked(thread_info, NULL, active_jerk_id);
				goto EXIT;
			}

			if (t2wnt > FBTCPU_SEC_DIVIDER) {
				fpsgo_main_trace("%s t2wnt:%lld > FBTCPU_SEC_DIVIDER",
					__func__, t2wnt);
				t2wnt = FBTCPU_SEC_DIVIDER;
				boost_info->t2wnt = t2wnt;
				fpsgo_systrace_c_fbt_debug(pid, buffer_id, t2wnt,
					"t2wnt");
			}

			active_jerk_id = fbt_get_next_jerk(
					boost_info->proc.active_jerk_id);
			boost_info->proc.active_jerk_id = active_jerk_id;

			timer = &(boost_info->proc.jerks[active_jerk_id].timer);
			if (timer) {
				if (boost_info->proc.jerks[
					active_jerk_id].jerking == 0) {
					boost_info->proc.jerks[
						active_jerk_id].jerking = 1;
					boost_info->proc.jerks[
						active_jerk_id].frame_qu_ts = ts;
					hrtimer_start(timer,
							ns_to_ktime(t2wnt),
							HRTIMER_MODE_REL);
				}
			} else
				FPSGO_LOGE("ERROR timer\n");
		}
	}

EXIT:
	return blc_wt;
}

static unsigned long long fbt_est_loading(int cur_ts,
		int last_ts, unsigned int obv)
{
	int duration;

	if (obv <= 0)
		return 0;

	if (last_ts <= 0 || cur_ts <= 0)
		return 0;

	if (cur_ts > last_ts) {
		duration = cur_ts - last_ts;

		return (unsigned long long)duration * (unsigned long long)obv;
	}

	return 0;
/*
 *	Since qu/deq notification may be delayed because of wq,
 *	loading could be over estimated if qu/deq start comes later.
 *	Keep the current design, and be awared of low power.
 */
}

static void fbt_check_max_blc_locked(int pid)
{
	unsigned int temp_blc = 0;
	int temp_blc_pid = 0;
	unsigned long long temp_blc_buffer_id = 0;
	int temp_blc_dep_num = 0;
	struct fpsgo_loading temp_blc_dep[MAX_DEP_NUM];

	fbt_find_max_blc(&temp_blc, &temp_blc_pid,
		&temp_blc_buffer_id, &temp_blc_dep_num, temp_blc_dep);

	max_blc = temp_blc;
	max_blc_cur = temp_blc;
	max_blc_pid = temp_blc_pid;
	max_blc_buffer_id = temp_blc_buffer_id;
	max_blc_stage = FPSGO_JERK_INACTIVE;
	max_blc_dep_num = temp_blc_dep_num;
	memcpy(max_blc_dep, temp_blc_dep,
		max_blc_dep_num * sizeof(struct fpsgo_loading));

	fpsgo_systrace_c_fbt_debug(-100, 0, max_blc, "max_blc");
	fpsgo_systrace_c_fbt_debug(-100, 0, max_blc_pid, "max_blc_pid");
	fpsgo_systrace_c_fbt_debug(-100, 0, max_blc_buffer_id,
		"max_blc_buffer_id");
	fpsgo_systrace_c_fbt_debug(-100, 0, max_blc_stage, "max_blc_stage");
	fpsgo_systrace_c_fbt_debug(-100, 0, max_blc_dep_num, "max_blc_dep_num");
	print_dep(__func__, "max_blc_dep",
		0, 0,
		max_blc_dep, max_blc_dep_num);

	if (max_blc == 0 && max_blc_pid == 0 && max_blc_buffer_id == 0) {
		if (boost_ta || boosted_group) {
			fbt_clear_boost_value();
			boosted_group = 0;
		}
		fbt_free_bhr();
		if (ultra_rescue)
			fbt_boost_dram(0);
		memset(base_opp, 0, cluster_num * sizeof(unsigned int));
		fbt_notify_CM_limit(0);
		fbt_set_idleprefer_locked(0);
		fbt_set_down_throttle_locked(-1);
	} else
		fbt_set_limit(pid, max_blc, max_blc_pid, max_blc_buffer_id,
			max_blc_dep_num, max_blc_dep, NULL, 0);
}

static int fbt_overest_loading(int blc_wt, unsigned long long running_time,
				unsigned long long target_time)
{
	int next_cluster;

	if (!blc_wt)
		return FPSGO_ADJ_NONE;

	if (running_time >= (target_time - loading_time_diff))
		return FPSGO_ADJ_NONE;

	if (blc_wt < cpu_dvfs[min_cap_cluster].capacity_ratio[0])
		return FPSGO_ADJ_LITTLE;

	next_cluster = (max_cap_cluster > min_cap_cluster)
			? min_cap_cluster + 1 : max_cap_cluster + 1;

	if (next_cluster == max_cap_cluster
		|| next_cluster < 0 || next_cluster >= cluster_num)
		return FPSGO_ADJ_NONE;

	if (blc_wt < cpu_dvfs[next_cluster].capacity_ratio[0])
		return FPSGO_ADJ_MIDDLE;

	return FPSGO_ADJ_NONE;
}

static void fpsgo_update_cpufreq_to_now(int new_ts_100us)
{
	unsigned long spinlock_flag_freq, spinlock_flag_loading;
	unsigned int curr_obv = 0U;
	int i, idx;
	unsigned int *cpu_cl_obv, *cpu_isolated;

	if (!fbt_enable)
		return;

	cpu_cl_obv = kcalloc(cluster_num, sizeof(unsigned int), GFP_KERNEL);
	cpu_isolated = kcalloc(cluster_num, sizeof(unsigned int), GFP_KERNEL);
	if (!cpu_cl_obv || !cpu_isolated) {
		fpsgo_main_trace("[%s] new temp array OOM\n", __func__);
		goto out;
	}

	spin_lock_irqsave(&freq_slock, spinlock_flag_freq);
	curr_obv = last_obv;
	for (i = 0; i < cluster_num; i++) {
		cpu_cl_obv[i] = clus_obv[i];
		cpu_isolated[i] = clus_status[i];
	}
	spin_unlock_irqrestore(&freq_slock, spinlock_flag_freq);

	spin_lock_irqsave(&loading_slock, spinlock_flag_loading);
	if (last_cb_ts != 0 && last_cb_ts < new_ts_100us) {
		idx = lastest_idx;
		idx = (idx + 1) >= LOADING_CNT ? 0 : (idx + 1);
		// unit: 100us
		lastest_ts[idx] = new_ts_100us;
		prev_cb_ts[idx] = last_cb_ts;
		lastest_obv[idx] = curr_obv;
		lastest_idx = idx;

		xgf_trace("[%s] idx=%d, prev_cb_ts=%d, lastest_ts=%d, last_obv=%u",
			__func__, idx, prev_cb_ts[idx], lastest_ts[idx], lastest_obv[idx]);

		if (lastest_obv_cl[idx] == NULL || lastest_is_cl_isolated[idx] == NULL)
			goto SKIP;

		for (i = 0; i < cluster_num; i++) {
			lastest_obv_cl[idx][i] = cpu_cl_obv[i];
			lastest_is_cl_isolated[idx][i] = cpu_isolated[i];
		}

		last_cb_ts = new_ts_100us;
	}

SKIP:
	spin_unlock_irqrestore(&loading_slock, spinlock_flag_loading);
out:
	kfree(cpu_isolated);
	kfree(cpu_cl_obv);
}

static int fbt_get_loading_exp(int pid, unsigned long long buffer_id, int thr_last_cb_ts,
	unsigned long long *loading, int ts_100us)
{
	int i, ret = 0;
	unsigned long spinlock_flag_freq, spinlock_flag_loading;
	unsigned long long loading_result = 0U;
	int prev_ts, next_ts, new_ts_100us;
	int render_last_cb_ts, freq_last_cb_ts;
	int *cpu_freq_ts_prev, *cpu_freq_ts_next;
	unsigned int cpy_current_obv;
	unsigned int *cpu_obv;

	new_ts_100us = ts_100us;

	fpsgo_update_cpufreq_to_now(new_ts_100us);

	cpu_freq_ts_prev = kcalloc(LOADING_CNT, sizeof(int), GFP_KERNEL);
	cpu_freq_ts_next = kcalloc(LOADING_CNT, sizeof(int), GFP_KERNEL);
	cpu_obv = kcalloc(LOADING_CNT, sizeof(unsigned int), GFP_KERNEL);
	if (!cpu_freq_ts_prev || !cpu_freq_ts_next || !cpu_obv) {
		fpsgo_main_trace("[%s] ERROR OOM\n", __func__);
		ret = 1;
		goto out;
	}

	spin_lock_irqsave(&freq_slock, spinlock_flag_freq);
	cpy_current_obv = last_obv;
	spin_unlock_irqrestore(&freq_slock, spinlock_flag_freq);

	spin_lock_irqsave(&loading_slock, spinlock_flag_loading);
	freq_last_cb_ts = last_cb_ts;
	memcpy(cpu_freq_ts_prev, prev_cb_ts, LOADING_CNT * sizeof(int));
	memcpy(cpu_freq_ts_next, lastest_ts, LOADING_CNT * sizeof(int));
	memcpy(cpu_obv, lastest_obv, LOADING_CNT * sizeof(unsigned int));
	spin_unlock_irqrestore(&loading_slock, spinlock_flag_loading);

	render_last_cb_ts = thr_last_cb_ts;
	(*loading) = 0;

	xgf_trace("[FBT][%s]pid=%d, bufferid=%llu, render_cb_ts=%d, ts=%d",
		__func__, pid, buffer_id, render_last_cb_ts, new_ts_100us);

	if (render_last_cb_ts <= 0) {
		xgf_trace("[FBT][%s]pid=%d, bufferid=%llu new frame!",
		__func__, pid, buffer_id);
		goto out;
	}

	/* Calculate the loading between render_last_cb and the current time. */
	if (freq_last_cb_ts < new_ts_100us && freq_last_cb_ts >= render_last_cb_ts) {
		loading_result = fbt_est_loading(new_ts_100us, freq_last_cb_ts, cpy_current_obv);
		(*loading) += loading_result;
		xgf_trace("[FBT][%s]pid=%d, freq_last_cb_ts=%d, current_obv=%u, loading=%llu",
		__func__, pid, freq_last_cb_ts, cpy_current_obv, loading_result);
	}


	for (i = 0; i < LOADING_CNT; i++) {
		prev_ts = next_ts = 0;
		// unit: 100us
		if (cpu_freq_ts_next[i] <= new_ts_100us &&
			cpu_freq_ts_next[i] > render_last_cb_ts) {
			next_ts = cpu_freq_ts_next[i];
			if (cpu_freq_ts_prev[i] >= render_last_cb_ts) {
				prev_ts = cpu_freq_ts_prev[i];
			} else { /* cpu_freq_ts_prev < render_last_cb_ts */
				prev_ts = render_last_cb_ts;
			}
		} else if (cpu_freq_ts_next[i] > new_ts_100us) {
			next_ts = new_ts_100us;
			if (cpu_freq_ts_prev[i] >= render_last_cb_ts) {
				prev_ts = cpu_freq_ts_prev[i];
			} else { /* cpu_freq_ts_prev < render_last_cb_ts */
				prev_ts = render_last_cb_ts;
			}
		}

		if (prev_ts && next_ts) {
			loading_result = fbt_est_loading(next_ts, prev_ts, cpu_obv[i]);
			(*loading) += loading_result;
			xgf_trace("[%s]i=%d, prevts=%d, nextts=%d, obv=%u, loading=%llu, acc=%llu",
			__func__, i, prev_ts, next_ts, cpu_obv[i], loading_result, (*loading));
		}
	}
out:
	kfree(cpu_freq_ts_prev);
	kfree(cpu_freq_ts_next);
	kfree(cpu_obv);
	return ret;
}


static int fbt_get_cl_loading_exp(int pid, unsigned long long buffer_id, int thr_last_cb_ts,
	unsigned long long *loading_cl, int ts_100us, int cid)
{
	int i, ret = 0;
	unsigned long spinlock_flag_freq, spinlock_flag_loading;
	unsigned long long loading_result = 0U;
	int prev_ts, next_ts, new_ts_100us;
	int render_last_cb_ts, freq_last_cb_ts;
	int *cpu_freq_ts_prev, *cpu_freq_ts_next;
	unsigned int *cpu_obv, *cpu_isolated, cpy_current_cl_obv;

	new_ts_100us = ts_100us;

	fpsgo_update_cpufreq_to_now(new_ts_100us);

	if (cid >= cluster_num || cid < 0) {
		ret = 1;
		fpsgo_main_trace("[%s] OOM!", __func__);
		goto out;
	}

	cpu_freq_ts_prev = kcalloc(LOADING_CNT, sizeof(int), GFP_KERNEL);
	cpu_freq_ts_next = kcalloc(LOADING_CNT, sizeof(int), GFP_KERNEL);
	cpu_obv = kcalloc(LOADING_CNT, sizeof(unsigned int), GFP_KERNEL);
	cpu_isolated = kcalloc(LOADING_CNT, sizeof(unsigned int), GFP_KERNEL);
	if (!cpu_freq_ts_prev || !cpu_freq_ts_next || !cpu_obv || !cpu_isolated) {
		fpsgo_main_trace("[%s] new temp array OOM\n", __func__);
		ret = 1;
		goto out;
	}

	spin_lock_irqsave(&freq_slock, spinlock_flag_freq);
	cpy_current_cl_obv = clus_obv[cid];
	spin_unlock_irqrestore(&freq_slock, spinlock_flag_freq);

	spin_lock_irqsave(&loading_slock, spinlock_flag_loading);
	freq_last_cb_ts = last_cb_ts;
	memcpy(cpu_freq_ts_prev, prev_cb_ts, LOADING_CNT * sizeof(int));
	memcpy(cpu_freq_ts_next, lastest_ts, LOADING_CNT * sizeof(int));
	for (i = 0; i < LOADING_CNT; i++) {
		if (lastest_obv_cl[i] == NULL || lastest_is_cl_isolated[i] == NULL) {
			ret = 1;
			spin_unlock_irqrestore(&loading_slock, spinlock_flag_loading);
			goto out;
		}
		cpu_obv[i] = lastest_obv_cl[i][cid];
		cpu_isolated[i] = lastest_is_cl_isolated[i][cid];
	}
	spin_unlock_irqrestore(&loading_slock, spinlock_flag_loading);

	render_last_cb_ts = thr_last_cb_ts;
	(*loading_cl) = 0;

	xgf_trace("[FBT][%s]pid=%d, bufferid=%llu, render_cb_ts=%d, ts=%d",
		__func__, pid, buffer_id, render_last_cb_ts, new_ts_100us);

	if (!render_last_cb_ts) {
		xgf_trace("[FBT][%s]pid=%d, bufferid=%d new frame!", __func__, pid, buffer_id);
		goto out;
	}

	/* Calculate the loading between render_last_cb and the current time. */
	if (freq_last_cb_ts < new_ts_100us && freq_last_cb_ts >= render_last_cb_ts) {
		loading_result = fbt_est_loading(new_ts_100us, freq_last_cb_ts, cpy_current_cl_obv);
		(*loading_cl) += loading_result;
		xgf_trace("[FBT][%s]pid=%d, freq_last_cb_ts=%d, current_cl_obv=%u, loading=%llu",
			__func__, pid, freq_last_cb_ts, cpy_current_cl_obv, loading_result);
	}

	for (i = 0; i < LOADING_CNT; i++) {
		if (cpu_isolated[i])
			continue;
		prev_ts = next_ts = 0;
		if (cpu_freq_ts_next[i] <= new_ts_100us &&
			cpu_freq_ts_next[i] > render_last_cb_ts) {
			next_ts = cpu_freq_ts_next[i];
			if (cpu_freq_ts_prev[i] >= render_last_cb_ts) {
				prev_ts = cpu_freq_ts_prev[i];
			} else { /* cpu_freq_ts_prev < render_last_cb_ts */
				prev_ts = render_last_cb_ts;
			}
		} else if (cpu_freq_ts_next[i] > new_ts_100us) {
			next_ts = new_ts_100us;
			if (cpu_freq_ts_prev[i] >= render_last_cb_ts) {
				prev_ts = cpu_freq_ts_prev[i];
			} else { /* cpu_freq_ts_prev < render_last_cb_ts */
				prev_ts = render_last_cb_ts;
			}
		}
		if (prev_ts && next_ts) {
			loading_result = fbt_est_loading(next_ts, prev_ts, cpu_obv[i]);
			(*loading_cl) += loading_result;
			xgf_trace("[%s]i=%d, prevts=%d, nextts=%d, obv=%u, loading=%llu, acc=%llu",
				__func__, i, prev_ts, next_ts, cpu_obv[i], loading_result,
				(*loading_cl));
		}
	}
out:
	kfree(cpu_freq_ts_prev);
	kfree(cpu_freq_ts_next);
	kfree(cpu_obv);
	kfree(cpu_isolated);
	return ret;
}

static unsigned long long fbt_adjust_loading_exp(struct render_info *thr,
	unsigned long long ts, int adjust)
{
	int new_ts_100us, ret = 0, i;
	unsigned long long loading = 0L;
	unsigned long long *loading_cl;
	unsigned long long loading_result = 0U;
	int first_cluster, sec_cluster;
	int separate_aa_final = thr->attr.separate_aa_by_pid;

	loading_cl = kcalloc(cluster_num, sizeof(unsigned long long), GFP_KERNEL);
	if (!loading_cl) {
		ret = 1;
		fpsgo_main_trace("[%s] OOM!", __func__);
		goto out;
	}

	new_ts_100us = nsec_to_100usec(ts);
	ret = fbt_get_loading_exp(thr->pid, thr->buffer_id, thr->render_last_cb_ts,
		&loading, new_ts_100us);
	fpsgo_systrace_c_fbt_debug(thr->pid, thr->buffer_id, loading, "q2q_loading");

	if (!(adjust_loading ||
		(adjust_loading_hwui_hint && thr->hwui == RENDER_INFO_HWUI_TYPE))
		|| cluster_num == 1) {
		adjust = FPSGO_ADJ_NONE;
		if (!(separate_aa_final && thr->boost_info.cl_loading))
			goto SKIP;
	}

	for (i = 0; i < cluster_num; i++) {
		ret = fbt_get_cl_loading_exp(thr->pid, thr->buffer_id, thr->render_last_cb_ts,
			&loading_result, new_ts_100us, i);
		fpsgo_systrace_c_fbt_debug(thr->pid, thr->buffer_id, loading_result,
			"q2q_loading_cl[%d]", i);
		if (ret)
			break;
		loading_cl[i] = loading_result;
		thr->boost_info.cl_loading[i] = loading_cl[i];
	}

SKIP:
	thr->render_last_cb_ts = new_ts_100us;
	xgf_trace("[FBT][%s] render_last_cb_ts=%d", __func__, thr->render_last_cb_ts);

	if (adjust != FPSGO_ADJ_NONE) {
		loading_result = 0;

		if (adjust == FPSGO_ADJ_LITTLE)
			first_cluster = min_cap_cluster;
		else {
			first_cluster = (max_cap_cluster > min_cap_cluster)
				? min_cap_cluster + 1
				: max_cap_cluster + 1;
		}

		sec_cluster = (max_cap_cluster > min_cap_cluster)
				? first_cluster + 1
				: first_cluster - 1;

		first_cluster = clamp(first_cluster, 0, cluster_num - 1);
		sec_cluster = clamp(sec_cluster, 0, cluster_num - 1);

		if (loading_cl[first_cluster] && loading_cl[sec_cluster]) {
			loading_result = thr->boost_info.loading_weight;
			loading_result = loading_result *
						loading_cl[sec_cluster];
			loading_result += (100 - thr->boost_info.loading_weight) *
						loading_cl[first_cluster];
			do_div(loading_result, 100);
			loading = (long)loading_result;
		} else if (loading_cl[first_cluster])
			loading = loading_cl[first_cluster];
		else if (loading_cl[sec_cluster])
			loading = loading_cl[sec_cluster];
	}

out:
	kfree(loading_cl);
	return loading;
}

static int fbt_adjust_loading_weight(struct fbt_frame_info *frame_info,
			unsigned long long target_time, int orig_weight)
{
	unsigned long long avg_running = 0ULL;
	int i;
	int new_weight = orig_weight;

	for (i = 0; i < WINDOW; i++)
		avg_running += frame_info[i].running_time;
	do_div(avg_running, WINDOW);

	if (avg_running > target_time)
		new_weight += 10;
	else if (avg_running < (target_time-loading_time_diff))
		new_weight -= 10;

	new_weight = clamp(new_weight, 0, 100);
	return new_weight;
}

static long fbt_get_loading(struct render_info *thr, unsigned long long ts)
{
	long loading = 0L;
	int adjust = FPSGO_ADJ_NONE;
	struct fbt_boost_info *boost = NULL;
	int last_blc = 0;
	int cur_hit = FPSGO_ADJ_NONE;

	if (!(adjust_loading ||
		(adjust_loading_hwui_hint && thr->hwui == RENDER_INFO_HWUI_TYPE))
		|| cluster_num == 1)
		goto SKIP;

	boost = &(thr->boost_info);

	mutex_lock(&blc_mlock);
	if (thr->p_blc)
		last_blc = thr->p_blc->blc;
	mutex_unlock(&blc_mlock);

	cur_hit = fbt_overest_loading(last_blc,
			thr->running_time, boost->target_time);

	if (cur_hit != FPSGO_ADJ_NONE && cur_hit == boost->hit_cluster)
		boost->hit_cnt++;
	else {
		boost->hit_cnt = 0;
		if (boost->deb_cnt)
			boost->deb_cnt--;
	}

	if (boost->hit_cnt >= loading_adj_cnt) {
		adjust = cur_hit;
		boost->hit_cluster = cur_hit;
		boost->hit_cnt = loading_adj_cnt;
		boost->deb_cnt = loading_debnc_cnt;
		boost->weight_cnt++;
		if (boost->weight_cnt >= loading_adj_cnt) {
			boost->loading_weight =
				fbt_adjust_loading_weight(
					&(boost->frame_info[0]),
					boost->target_time,
					boost->loading_weight);
			boost->weight_cnt = 0;
		}
	} else if (boost->deb_cnt > 0) {
		adjust = boost->hit_cluster;
		boost->weight_cnt = 0;
	} else {
		adjust = FPSGO_ADJ_NONE;
		boost->loading_weight = LOADING_WEIGHT;
		boost->weight_cnt = 0;
		boost->hit_cluster = cur_hit;
	}

	if (adjust != FPSGO_ADJ_NONE) {
		fpsgo_systrace_c_fbt(thr->pid, thr->buffer_id,
				adjust, "adjust");
		fpsgo_systrace_c_fbt(thr->pid, thr->buffer_id,
				boost->loading_weight, "weight");
	}

	fpsgo_systrace_c_fbt_debug(thr->pid, thr->buffer_id,
		boost->weight_cnt, "weight_cnt");
	fpsgo_systrace_c_fbt_debug(thr->pid, thr->buffer_id,
		boost->hit_cnt, "hit_cnt");
	fpsgo_systrace_c_fbt_debug(thr->pid, thr->buffer_id,
		boost->deb_cnt, "deb_cnt");
	fpsgo_systrace_c_fbt_debug(thr->pid, thr->buffer_id,
		boost->hit_cluster, "hit_cluster");

SKIP:
	loading = fbt_adjust_loading_exp(thr, ts, adjust);

	return loading;
}

static void fbt_reset_boost(struct render_info *thr)
{
	struct fbt_boost_info *boost = NULL;
	int cur_pid = 0;

	if (!thr)
		return;

	cur_pid = thr->pid;

	mutex_lock(&blc_mlock);
	if (thr->p_blc) {
		thr->p_blc->blc = 0;
		thr->p_blc->blc_b = 0;
		thr->p_blc->blc_m = 0;
	}
	mutex_unlock(&blc_mlock);

	boost = &(thr->boost_info);

	boost->last_blc = 0;
	boost->target_time = 0;
	boost->target_fps = -1;

	memset(boost->frame_info, 0, WINDOW * sizeof(struct fbt_frame_info));
	boost->f_iter = 0;
	boost->floor_count = 0;
	boost->floor = 0;
	boost->reset_floor_bound = 0;

	mutex_lock(&fbt_mlock);
	if (!boost_ta)
		fbt_set_min_cap_locked(thr, 0, 0, 0, 100, 100, 100, FPSGO_JERK_INACTIVE);
	fbt_check_max_blc_locked(cur_pid);
	mutex_unlock(&fbt_mlock);

}

static void fbt_frame_start(struct render_info *thr, unsigned long long ts)
{
	struct fbt_boost_info *boost;
	long long runtime;
	int targettime, targetfps, targetfpks, fps_margin, cooler_on;
	unsigned int limited_cap = 0;
	int blc_wt = 0;
	long loading = 0L;
	int q_c_time, q_g_time;

	if (!thr)
		return;

	boost = &(thr->boost_info);

	runtime = thr->running_time;
	boost->frame_info[boost->f_iter].running_time = runtime;

	fpsgo_fbt2fstb_query_fps(thr->pid, thr->buffer_id,
			&targetfps, &targettime, &fps_margin, thr->tgid,
			&q_c_time, &q_g_time, &targetfpks, &cooler_on);
	boost->quantile_cpu_time = q_c_time;
	boost->quantile_gpu_time = q_g_time;
	if (!targetfps)
		targetfps = TARGET_UNLIMITED_FPS;

	fpsgo_systrace_c_fbt(thr->pid, thr->buffer_id, targetfps, "target_fps");
	fpsgo_systrace_c_fbt(thr->pid, thr->buffer_id,
		targettime, "target_time");

	fbt_set_render_boost_attr(thr);

	loading = fbt_get_loading(thr, ts);
	fpsgo_systrace_c_fbt_debug(thr->pid, thr->buffer_id,
		loading, "compute_loading");

	if (thr->Q2Q_time != 0)
		thr->avg_freq = loading / nsec_to_100usec(thr->Q2Q_time);

	/* unreliable targetfps */
	if (targetfps == -1) {
		fbt_reset_boost(thr);
		runtime = -1;
		goto EXIT;
	}

	fbt_set_idleprefer_locked(1);
	fbt_set_down_throttle_locked(0);

	blc_wt = fbt_boost_policy(runtime,
			targettime, targetfps, fps_margin,
			thr, ts, loading, targetfpks, cooler_on);

	limited_cap = fbt_get_max_userlimit_freq();
	fpsgo_systrace_c_fbt(thr->pid, thr->buffer_id,
		limited_cap, "limited_cap");

EXIT:
	fpsgo_fbt2fstb_update_cpu_frame_info(thr->pid, thr->buffer_id,
		thr->tgid, thr->frame_type,
		thr->Q2Q_time, runtime,
		blc_wt, limited_cap, thr->enqueue_length, thr->dequeue_length);
}

static void fbt_setting_reset(int reset_idleprefer)
{
	if (reset_idleprefer)
		fbt_set_idleprefer_locked(0);

	fbt_set_down_throttle_locked(-1);
	fbt_free_bhr();
	fbt_notify_CM_limit(0);

	if (boost_ta || boosted_group) {
		fbt_clear_boost_value();
		boosted_group = 0;
	}

	if (ultra_rescue)
		fbt_boost_dram(0);
}

void fpsgo_ctrl2fbt_cpufreq_cb_exp(int cid, unsigned long freq)
{
	unsigned long spinlock_flag_freq, spinlock_flag_loading;
	unsigned int curr_obv = 0U;
	unsigned long long curr_cb_ts;
	int new_ts;
	int i, idx;
	int opp;

	if (!fbt_enable)
		return;

	if (cid >= cluster_num)
		return;

	curr_cb_ts = fpsgo_get_time();
	new_ts = nsec_to_100usec(curr_cb_ts);

	for (opp = (nr_freq_cpu - 1); opp > 0; opp--) {
		if (cpu_dvfs[cid].power[opp] >= freq)
			break;
	}
	curr_obv = cpu_dvfs[cid].capacity_ratio[opp];

	spin_lock_irqsave(&freq_slock, spinlock_flag_freq);

	if (clus_obv[cid] == curr_obv) {
		spin_unlock_irqrestore(&freq_slock, spinlock_flag_freq);
		return;
	}

	fpsgo_systrace_c_fbt_debug(-100, 0, freq, "curr_freq[%d]", cid);

	spin_lock_irqsave(&loading_slock, spinlock_flag_loading);
	if (last_cb_ts != 0) {
		idx = lastest_idx;
		idx = (idx + 1) >= LOADING_CNT ? 0 : (idx + 1);
		// unit: 100us
		lastest_ts[idx] = new_ts;
		prev_cb_ts[idx] = last_cb_ts;
		lastest_obv[idx] = last_obv;
		lastest_idx = idx;

		xgf_trace("[%s] idx=%d, prev_cb_ts=%d, lastest_ts=%d, last_obv=%u",
			__func__, idx, prev_cb_ts[idx], lastest_ts[idx], lastest_obv[idx]);

		if (lastest_obv_cl[idx] == NULL || lastest_is_cl_isolated[idx] == NULL)
			goto SKIP;

		for (i = 0; i < cluster_num; i++) {
			lastest_obv_cl[idx][i] = clus_obv[i];
			lastest_is_cl_isolated[idx][i] = clus_status[i];
		}
	}

SKIP:
	last_cb_ts = new_ts;
	spin_unlock_irqrestore(&loading_slock, spinlock_flag_loading);

	clus_obv[cid] = curr_obv;
	for (i = 0; i < cluster_num; i++) {
		clus_status[i] = fbt_is_cl_isolated(i);

		if (clus_status[i]) {
			fpsgo_systrace_c_fbt_debug(-100, 0, i, "clus_iso");
			continue;
		}

		// Get the maximum capacity within diff. clusters.
		if (curr_obv < clus_obv[i])
			curr_obv = clus_obv[i];
	}
	last_obv = curr_obv;
	spin_unlock_irqrestore(&freq_slock, spinlock_flag_freq);

	fpsgo_systrace_c_fbt_debug(-100, 0, curr_obv, "last_obv");
}

void fpsgo_ctrl2fbt_vsync(unsigned long long ts)
{
	unsigned long long vsync_duration;

	if (!fbt_is_enable())
		return;

	mutex_lock(&fbt_mlock);

	vsync_duration = nsec_to_usec(ts - vsync_time);

	vsync_time = ts;
	xgf_trace(
		"vsync_time=%llu, vsync_duration=%llu, vsync_duration_60=%llu, vsync_duration_90=%llu, vsync_duration_120=%llu, vsync_duration_144=%llu",
		nsec_to_usec(vsync_time), vsync_duration,
		vsync_duration_us_60,
		vsync_duration_us_90,
		vsync_duration_us_120,
		vsync_duration_us_144);

	mutex_unlock(&fbt_mlock);
}

void fpsgo_comp2fbt_frame_start(struct render_info *thr,
		unsigned long long ts)
{
	if (!thr)
		return;

	if (!fbt_is_enable())
		return;

	fpsgo_systrace_c_fbt(thr->pid, thr->buffer_id,
		thr->Q2Q_time, "q2q time");

	if (!thr->running_time) {
		fpsgo_systrace_c_fbt(thr->pid, thr->buffer_id,
			0, "zero running time");
		return;
	}


	fbt_frame_start(thr, ts);
}

void fpsgo_comp2fbt_deq_end(struct render_info *thr,
		unsigned long long ts)
{
	struct fbt_jerk *jerk;

	if (!thr)
		return;

	if (!fbt_is_enable())
		return;

	jerk = &(thr->boost_info.proc.jerks[
			thr->boost_info.proc.active_jerk_id]);

	if (jerk->postpone) {
		jerk->postpone = 0;
		schedule_work(&jerk->work);
	}
}


void fpsgo_ctrl2fbt_dfrc_fps(int fps_limit)
{
	if (!fps_limit || fps_limit > TARGET_UNLIMITED_FPS)
		return;

	mutex_lock(&fbt_mlock);
	_gdfrc_fps_limit = fps_limit;
	vsync_period = FBTCPU_SEC_DIVIDER / fps_limit;

	xgf_trace("_gdfrc_fps_limit %d", _gdfrc_fps_limit);
	xgf_trace("vsync_period %d", vsync_period);

	mutex_unlock(&fbt_mlock);
}

void fpsgo_base2fbt_node_init(struct render_info *obj)
{
	struct fbt_thread_blc *blc_link;
	struct fbt_boost_info *boost;
	long *cl_loading;

	int i;

	if (!obj)
		return;

	boost = &(obj->boost_info);

	cl_loading = kcalloc(cluster_num, sizeof(long), GFP_KERNEL);
	if (!cl_loading) {
		FPSGO_LOGE("ERROR OOM\n");
		return;
	}

	for (i = 0; i < RESCUE_TIMER_NUM; i++)
		fbt_init_jerk(&(boost->proc.jerks[i]), i);

	boost->loading_weight = LOADING_WEIGHT;
	boost->t_duration = DEFAULT_ST2WNT_ADJ;
	boost->cl_loading = cl_loading;

	blc_link = fbt_list_blc_add(obj->pid, obj->buffer_id);
	obj->p_blc = blc_link;
}

void fpsgo_base2fbt_item_del(struct fbt_thread_blc *pblc,
		struct fpsgo_loading *pdep,
		struct render_info *thr)
{
	if (!pblc)
		goto skip_del_pblc;

	mutex_lock(&blc_mlock);
	list_del(&pblc->entry);
	kfree(pblc);
	mutex_unlock(&blc_mlock);

skip_del_pblc:
	mutex_lock(&fbt_mlock);
	if (!boost_ta)
		fbt_set_min_cap_locked(thr, 0, 0, 0, 100, 100, 100, FPSGO_JERK_INACTIVE);
	if (thr) {
		thr->boost_info.last_blc = 0;
		thr->boost_info.last_blc_b = 0;
		thr->boost_info.last_blc_m = 0;
		kfree(thr->boost_info.cl_loading);
		thr->boost_info.cl_loading = NULL;
	}
	mutex_unlock(&fbt_mlock);

	fbt_clear_dep_list(pdep);
}

int fpsgo_base2fbt_get_max_blc_pid(int *pid, unsigned long long *buffer_id)
{
	mutex_lock(&fbt_mlock);

	if (!fbt_enable) {
		mutex_unlock(&fbt_mlock);
		return 0;
	}

	*pid = max_blc_pid;
	*buffer_id = max_blc_buffer_id;

	mutex_unlock(&fbt_mlock);

	return 1;
}

void fpsgo_base2fbt_check_max_blc(void)
{
	if (!fbt_is_enable())
		return;

	mutex_lock(&fbt_mlock);
	fbt_check_max_blc_locked(0);
	mutex_unlock(&fbt_mlock);
}

void fpsgo_base2fbt_no_one_render(void)
{
	int clear_uclamp = 0;

	mutex_lock(&fbt_mlock);

	if (!fbt_enable) {
		mutex_unlock(&fbt_mlock);
		return;
	}

	xgf_trace("fpsgo no render");

	max_blc = 0;
	max_blc_cur = 0;
	max_blc_pid = 0;
	max_blc_buffer_id = 0;
	max_blc_stage = FPSGO_JERK_INACTIVE;
	max_blc_dep_num = 0;
	memset(base_opp, 0, cluster_num * sizeof(unsigned int));
	fpsgo_systrace_c_fbt_debug(-100, 0, max_blc, "max_blc");
	fpsgo_systrace_c_fbt_debug(-100, 0, max_blc_pid, "max_blc_pid");
	fpsgo_systrace_c_fbt_debug(-100, 0, max_blc_buffer_id,
		"max_blc_buffer_id");
	fpsgo_systrace_c_fbt_debug(-100, 0, max_blc_stage, "max_blc_stage");
	fpsgo_systrace_c_fbt_debug(-100, 0, max_blc_dep_num, "max_blc_dep_num");
	print_dep(__func__, "max_blc_dep",
		0, 0,
		max_blc_dep, max_blc_dep_num);

	fbt_setting_reset(1);

	if (!boost_ta)
		clear_uclamp = 1;

	mutex_unlock(&fbt_mlock);

	if (clear_uclamp)
		fpsgo_clear_uclamp_boost();
}


void fpsgo_base2fbt_set_min_cap(struct render_info *thr, int min_cap,
					int min_cap_b, int min_cap_m)
{
	int max_cap = 100, max_cap_b = 100, max_cap_m = 100;

	mutex_lock(&fbt_mlock);
	fbt_cal_min_max_cap(thr, min_cap, min_cap_b, min_cap_m, FPSGO_JERK_INACTIVE,
		thr->pid, thr->buffer_id, &min_cap, &min_cap_b, &min_cap_m,
		&max_cap, &max_cap_b, &max_cap_m);
	fbt_set_min_cap_locked(thr, min_cap, min_cap_b, min_cap_m, max_cap,
		max_cap_b, max_cap_m, FPSGO_JERK_INACTIVE);
	if (thr) {
		thr->boost_info.last_blc = min_cap;
		thr->boost_info.last_blc_b = min_cap_b;
		thr->boost_info.last_blc_m = min_cap_m;
	}
	mutex_unlock(&fbt_mlock);
}

void fpsgo_base2fbt_clear_llf_policy(struct render_info *thr)
{
	int i;

	if (!thr || !thr->dep_arr)
		return;

	for (i = 0; i < thr->dep_valid_size; i++) {
		struct fpsgo_loading *fl = &(thr->dep_arr[i]);

		if (!fl)
			continue;

		if (fl->prefer_type == FPSGO_PREFER_NONE)
			continue;

		fbt_reset_task_setting(fl, 0);
	}
}

void fpsgo_base2fbt_cancel_jerk(struct render_info *thr)
{
	int i;

	if (!thr)
		return;

	for (i = 0; i < RESCUE_TIMER_NUM; i++) {
		if (thr->boost_info.proc.jerks[i].jerking)
			hrtimer_cancel(&(thr->boost_info.proc.jerks[i].timer));
	}
}

void fpsgo_sbe2fbt_rescue(struct render_info *thr, int start, int enhance,
		unsigned long long frame_id)
{
	int floor, blc_wt = 0, blc_wt_b = 0, blc_wt_m = 0;
	int max_cap = 100, max_cap_b = 100, max_cap_m = 100;
	struct cpu_ctrl_data *pld;
	int headroom;
	int new_enhance;
	unsigned int temp_blc = 0;
	int temp_blc_pid = 0;
	unsigned long long temp_blc_buffer_id = 0;
	int temp_blc_dep_num = 0;
	struct fpsgo_loading temp_blc_dep[MAX_DEP_NUM];
	int separate_aa_final = separate_aa;

	if (!thr || !sbe_rescue_enable)
		return;

	separate_aa_final = thr->attr.separate_aa_by_pid;

	pld = kcalloc(cluster_num, sizeof(struct cpu_ctrl_data), GFP_KERNEL);
	if (!pld)
		return;

	mutex_lock(&fbt_mlock);

	if (start) {
		if (frame_id)
			sbe_rescuing_frame_id = frame_id;
		if (thr->boost_info.sbe_rescue != 0)
			goto leave;
		floor = thr->boost_info.last_blc;
		if (!floor)
			goto leave;

		headroom = rescue_opp_c;
		new_enhance = enhance < 0 ?  rescue_enhance_f : sbe_enhance_f;

		if (thr->boost_info.cur_stage == FPSGO_JERK_SECOND)
			headroom = rescue_second_copp;

		blc_wt = fbt_get_new_base_blc(pld, floor, new_enhance, rescue_opp_f, headroom);
		if (separate_aa_final) {
			blc_wt_b = fbt_get_new_base_blc(pld, floor, new_enhance,
						rescue_opp_f, headroom);
			blc_wt_m = fbt_get_new_base_blc(pld, floor, new_enhance,
						rescue_opp_f, headroom);
		}

		if (!blc_wt)
			goto leave;
		thr->boost_info.sbe_rescue = 1;

		if (thr->boost_info.cur_stage != FPSGO_JERK_SECOND) {
			blc_wt = fbt_limit_capacity(blc_wt, 1);
			if (separate_aa_final) {
				blc_wt_b = fbt_limit_capacity(blc_wt_b, 1);
				blc_wt_m = fbt_limit_capacity(blc_wt_m, 1);
			}
			fbt_set_hard_limit_locked(FPSGO_HARD_CEILING, pld);

			thr->boost_info.cur_stage = FPSGO_JERK_SBE;

			if (thr->pid == max_blc_pid && thr->buffer_id == max_blc_buffer_id) {
				max_blc_stage = FPSGO_JERK_SBE;
				fpsgo_systrace_c_fbt(-100, 0, max_blc_stage, "max_blc_stage");
			}
		}
		fbt_set_ceiling(pld, thr->pid, thr->buffer_id);

		if (boost_ta) {
			fbt_set_boost_value(blc_wt);
			max_blc_cur = blc_wt;
		} else {
			fbt_cal_min_max_cap(thr, blc_wt, blc_wt_b, blc_wt_m, FPSGO_JERK_FIRST,
				thr->pid, thr->buffer_id, &blc_wt, &blc_wt_b, &blc_wt_m,
				&max_cap, &max_cap_b, &max_cap_m);
			fbt_set_min_cap_locked(thr, blc_wt, blc_wt_b, blc_wt_m, max_cap,
				max_cap_b, max_cap_m, FPSGO_JERK_FIRST);
		}

		thr->boost_info.last_blc = blc_wt;

		if (separate_aa_final) {
			thr->boost_info.last_blc_b = blc_wt_b;
			thr->boost_info.last_blc_m = blc_wt_m;
		}

		fpsgo_systrace_c_fbt(thr->pid, thr->buffer_id, new_enhance, "sbe rescue");


		/* support mode: sbe rescue until queue end */
		if (start == SBE_RESCUE_MODE_UNTIL_QUEUE_END) {
			thr->boost_info.sbe_rescue = 0;
			sbe_rescuing_frame_id = -1;
			fpsgo_systrace_c_fbt(thr->pid, thr->buffer_id, 0, "sbe rescue");
		}

	} else {
		if (thr->boost_info.sbe_rescue == 0)
			goto leave;
		if (frame_id < sbe_rescuing_frame_id)
			goto leave;
		sbe_rescuing_frame_id = -1;
		thr->boost_info.sbe_rescue = 0;
		blc_wt = thr->boost_info.last_normal_blc;
		if (separate_aa_final) {
			blc_wt_b = thr->boost_info.last_normal_blc_b;
			blc_wt_m = thr->boost_info.last_normal_blc_m;
		}
		if (!blc_wt || (separate_aa_final && !(blc_wt_b && blc_wt_m)))
			goto leave;

		/* find max perf index */
		fbt_find_max_blc(&temp_blc, &temp_blc_pid, &temp_blc_buffer_id,
				&temp_blc_dep_num, temp_blc_dep);
		fbt_get_new_base_blc(pld, temp_blc, 0, rescue_opp_f, bhr_opp);
		fbt_set_ceiling(pld, thr->pid, thr->buffer_id);
		if (boost_ta) {
			fbt_set_boost_value(blc_wt);
			max_blc_cur = blc_wt;
		} else {
			fbt_cal_min_max_cap(thr, blc_wt, blc_wt_b, blc_wt_m, FPSGO_JERK_FIRST,
				thr->pid, thr->buffer_id, &blc_wt, &blc_wt_b, &blc_wt_m,
				&max_cap, &max_cap_b, &max_cap_m);
			fbt_set_min_cap_locked(thr, blc_wt, blc_wt_b, blc_wt_m, max_cap,
				max_cap_b, max_cap_m, FPSGO_JERK_FIRST);
		}
		fpsgo_systrace_c_fbt(thr->pid, thr->buffer_id, 0, "sbe rescue");
	}
	fpsgo_systrace_c_fbt(thr->pid, thr->buffer_id, blc_wt, "perf idx");

leave:
	mutex_unlock(&fbt_mlock);
	kfree(pld);
}

int fpsgo_base2fbt_is_finished(struct render_info *thr)
{
	int i;

	if (!thr)
		return 1;

	for (i = 0; i < RESCUE_TIMER_NUM; i++) {
		if (thr->boost_info.proc.jerks[i].jerking) {
			FPSGO_LOGE("(%d, %llu)(%p)(%d)[%d] is (%d, %d)\n",
				thr->pid, thr->buffer_id, thr, thr->linger, i,
				thr->boost_info.proc.jerks[i].jerking,
				thr->boost_info.proc.jerks[i].postpone);
			return 0;
		}
	}

	return 1;
}

void fpsgo_base2fbt_stop_boost(struct render_info *thr)
{
	if (!thr)
		return;

	if (thr->boost_info.last_blc == 0)
		return;

	fpsgo_systrace_c_fbt(thr->pid, thr->buffer_id, 1, "stop_boost");
	fpsgo_systrace_c_fbt(thr->pid, thr->buffer_id, 0, "stop_boost");

	fbt_reset_boost(thr);
}

static int fbt_get_opp_by_freq(int cluster, unsigned int freq)
{
	int opp;

	if (cluster < 0 || cluster >= cluster_num)
		return INVALID_NUM;

	for (opp = (nr_freq_cpu - 1); opp > 0; opp--) {
		if (cpu_dvfs[cluster].power[opp] == freq)
			break;

		if (cpu_dvfs[cluster].power[opp] > freq) {
			opp++;
			break;
		}
	}

	opp = clamp(opp, 0, nr_freq_cpu - 1);

	return opp;
}

static int check_limit_cap(int is_rescue)
{
	int cap = 0, opp, cluster;
	int max_cap = 0;

	if (limit_policy == FPSGO_LIMIT_NONE || !limit_clus_ceil)
		return 0;

	for (cluster = cluster_num - 1; cluster >= 0 ; cluster--) {
		struct fbt_syslimit *limit = &limit_clus_ceil[cluster];

		opp = (is_rescue) ? limit->ropp : limit->copp;
		if (opp == -1)
			opp = 0;

		if (opp >= 0 && opp < nr_freq_cpu) {
			cap = cpu_dvfs[cluster].capacity_ratio[opp];
			if (cap > max_cap)
				max_cap = cap;
		}
	}

	if (max_cap <= 0)
		return 0;

	return max_cap;
}

static void fbt_set_cap_limit(void)
{
	int freq = 0, limit_ret, r_freq = 0;
	int opp;
	int cluster;
	struct fbt_syslimit *limit = NULL;

	limit_policy = FPSGO_LIMIT_NONE;
	limit_cpu = -1;

	if (!limit_clus_ceil)
		return;

	for (cluster = 0; cluster < cluster_num; cluster++) {
		limit_clus_ceil[cluster].copp = INVALID_NUM;
		limit_clus_ceil[cluster].ropp = INVALID_NUM;
		limit_clus_ceil[cluster].cfreq = 0;
		limit_clus_ceil[cluster].rfreq = 0;
	}

	if (cluster_num < 2)
		return;

	limit_ret = fbt_get_cluster_limit(&cluster, &freq, &r_freq,
					&limit_cpu);
	if (limit_ret != FPSGO_LIMIT_FREQ)
		return;

	if (cluster < 0 || cluster >= cluster_num)
		return;

	limit_policy = FPSGO_LIMIT_CAPACITY;
	limit = &limit_clus_ceil[cluster];

	if (!freq)
		goto RCEILING;

	opp = fbt_get_opp_by_freq(cluster, freq);
	if (opp == INVALID_NUM)
		goto RCEILING;

	limit->copp = opp;
	limit->cfreq = cpu_dvfs[cluster].power[opp];
	limit_cap = check_limit_cap(0);

RCEILING:
	if (!r_freq)
		goto CHECK;

	opp = fbt_get_opp_by_freq(cluster, r_freq);
	if (opp == INVALID_NUM)
		goto CHECK;

	limit->ropp = opp;
	limit->rfreq = cpu_dvfs[cluster].power[opp];
	limit_rcap = check_limit_cap(1);

CHECK:
	if (limit->copp == INVALID_NUM && limit->ropp == INVALID_NUM)
		limit_policy = FPSGO_LIMIT_NONE;
}
#ifdef CONFIG_MTK_OPP_CAP_INFO
static int cmp_uint(const void *a, const void *b)
{
	return *(unsigned int *)b - *(unsigned int *)a;
}
#endif

static void fbt_update_pwr_tbl(void)
{
	unsigned long long max_cap = 0ULL, min_cap = UINT_MAX;
	int cluster = 0;
#ifdef CONFIG_MTK_OPP_CAP_INFO
	int cpu, opp;
	struct cpufreq_policy *policy;
#endif


#ifdef CONFIG_MTK_OPP_CAP_INFO
	for_each_possible_cpu(cpu) {
		if (cluster_num <= 0 || cluster >= cluster_num)
			break;
		policy = cpufreq_cpu_get(cpu);
		if (!policy)
			break;

		cpu_dvfs[cluster].num_cpu = cpumask_weight(policy->cpus);

		for (opp = 0; opp < nr_freq_cpu; opp++) {
			cpu_dvfs[cluster].capacity_ratio[opp] =
				fpsgo_arch_nr_get_cap_cpu(cpu, opp) * 100 >> 10;
			cpu_dvfs[cluster].power[opp] =
				fpsgo_cpufreq_get_freq_by_idx(cpu, opp);
		}

		sort(cpu_dvfs[cluster].capacity_ratio,
				nr_freq_cpu,
				sizeof(unsigned int),
				cmp_uint, NULL);
		sort(cpu_dvfs[cluster].power,
				nr_freq_cpu,
				sizeof(unsigned int),
				cmp_uint, NULL);

		cluster++;
		cpu = cpumask_last(policy->related_cpus);
		cpufreq_cpu_put(policy);
	}
#endif


	for (cluster = 0; cluster < cluster_num ; cluster++) {
		if (cpu_dvfs[cluster].capacity_ratio[0] >= max_cap) {
			max_cap = cpu_dvfs[cluster].capacity_ratio[0];
			max_cap_cluster = cluster;
		}
		if (cpu_dvfs[cluster].capacity_ratio[0] < min_cap) {
			min_cap = cpu_dvfs[cluster].capacity_ratio[0];
			min_cap_cluster = cluster;
		}
		if (cpu_dvfs[cluster].power[0] > cpu_max_freq)
			cpu_max_freq = cpu_dvfs[cluster].power[0];
	}

	max_cap_cluster = clamp(max_cap_cluster, 0, cluster_num - 1);
	min_cap_cluster = clamp(min_cap_cluster, 0, cluster_num - 1);
	sec_cap_cluster = (max_cap_cluster > min_cap_cluster)
		? max_cap_cluster - 1 : min_cap_cluster - 1;
	fbt_set_cap_limit();

	if (cluster_num > 0 && max_cap_cluster < cluster_num)
		max_cl_core_num = cpu_dvfs[max_cap_cluster].num_cpu;

	if (!cpu_max_freq) {
		FPSGO_LOGE("NULL power table\n");
		cpu_max_freq = 1;
	}
}

static void fbt_setting_exit(void)
{
	vsync_time = 0;
	memset(base_opp, 0, cluster_num * sizeof(unsigned int));
	max_blc = 0;
	max_blc_cur = 0;
	max_blc_pid = 0;
	max_blc_buffer_id = 0;
	max_blc_stage = FPSGO_JERK_INACTIVE;
	max_blc_dep_num = 0;

	fbt_setting_reset(1);
}

int fpsgo_ctrl2fbt_switch_fbt(int enable)
{
	mutex_lock(&fbt_mlock);

	if (fbt_enable == enable) {
		mutex_unlock(&fbt_mlock);
		return 0;
	}

	fbt_enable = enable;
	xgf_trace("fbt_enable %d", fbt_enable);

	if (!enable)
		fbt_setting_exit();

	mutex_unlock(&fbt_mlock);

	if (!enable && !boost_ta)
		fpsgo_clear_uclamp_boost();

	return 0;
}

int fbt_switch_uclamp_onoff(int enable);
int fpsgo_ctrl2fbt_switch_uclamp(int enable)
{
	return fbt_switch_uclamp_onoff(enable);
}

int fbt_switch_idleprefer(int enable)
{
	int last_enable;

	mutex_lock(&fbt_mlock);

	if (!fbt_enable) {
		mutex_unlock(&fbt_mlock);
		return 0;
	}

	last_enable = fbt_idleprefer_enable;

	if (last_enable && !enable)
		fbt_set_idleprefer_locked(0);
	else if (!last_enable && enable)
		fbt_set_idleprefer_locked(1);

	fbt_idleprefer_enable = enable;

	mutex_unlock(&fbt_mlock);

	return 0;
}

int fbt_set_filter_frame_enable(int enable)
{
	if (enable == filter_frame_enable)
		return 0;
	mutex_lock(&fbt_mlock);
	filter_frame_enable = enable;
	mutex_unlock(&fbt_mlock);
	return 0;
}

int fbt_set_filter_frame_window_size(int window_size)
{
	if (window_size == filter_frame_window_size)
		return 0;
	if (window_size <= 0 || window_size > FBT_FILTER_MAX_WINDOW)
		return 1;
	mutex_lock(&fbt_mlock);
	filter_frame_window_size = window_size;
	mutex_unlock(&fbt_mlock);
	return 0;
}

int fbt_set_filter_frame_kmin(int filter_kmin)
{
	if (filter_frame_kmin == filter_kmin)
		return 0;
	if (filter_kmin <= 0 || filter_kmin > filter_frame_window_size)
		return 1;
	mutex_lock(&fbt_mlock);
	filter_frame_kmin = filter_kmin;
	mutex_unlock(&fbt_mlock);
	return 0;
}

int fbt_switch_ceiling(int enable)
{
	int last_enable;

	mutex_lock(&fbt_mlock);

	if (!fbt_enable) {
		mutex_unlock(&fbt_mlock);
		return 0;
	}

	last_enable = suppress_ceiling;
	suppress_ceiling = enable;

	if (last_enable && !suppress_ceiling)
		fbt_free_bhr();

	mutex_unlock(&fbt_mlock);

	return 0;
}

int fbt_switch_uclamp_onoff(int enable)
{
	mutex_lock(&fbt_mlock);

	if (enable == uclamp_boost_enable) {
		mutex_unlock(&fbt_mlock);
		return 0;
	}

	uclamp_boost_enable = enable;

	mutex_unlock(&fbt_mlock);

	if (!enable) {
		fpsgo_clear_uclamp_boost();
		fbt_notify_CM_limit(0);
	}

	fpsgo_fbt2cam_notify_uclamp_boost_enable(enable);

	return 0;
}

int fbt_switch_to_ta(int input)
{
	if (input < 0 || input > 1)
		return -EINVAL;

	mutex_lock(&fbt_mlock);
	if (boost_ta && !input)
		fbt_clear_boost_value();
	boost_ta = input;
	mutex_unlock(&fbt_mlock);

	if (input) {
		fpsgo_clear_uclamp_boost();
		fbt_notify_CM_limit(0);
	}

	return 0;
}

static int fbt_set_separate_aa(int enable)
{
	if (enable == separate_aa)
		return 0;
	mutex_lock(&fbt_mlock);
	separate_aa = enable;

	mutex_unlock(&fbt_mlock);
	return 0;
}

struct xgf_thread_loading fbt_xgff_list_loading_add(int pid,
	unsigned long long buffer_id, unsigned long long ts)
{
	struct xgf_thread_loading obj;
	int new_ts;

	new_ts = nsec_to_100usec(ts);
	obj.pid = pid;
	obj.buffer_id = buffer_id;
	obj.loading = 0;
	obj.last_cb_ts = new_ts;

	xgf_trace("[XGFF][%s] reset pid=%d,buffer_id=%llu,ts=%d", __func__,
		pid, buffer_id, new_ts);
	return obj;
}

long fbt_xgff_get_loading_by_cluster(struct xgf_thread_loading *ploading, unsigned long long ts,
	unsigned int prefer_cluster, int skip, long *area)
{
	int i;
	int new_ts_100us, ret = 0;
	unsigned long long loading = 0L;

	new_ts_100us = nsec_to_100usec(ts);

	if (skip)
		goto out;

	if (ploading) {
		if (area && !skip) {
			for (i = 0; i < cluster_num; i++) {
				loading = 0L;
				ret = fbt_get_cl_loading_exp(ploading->pid, ploading->buffer_id,
					ploading->last_cb_ts, &loading, new_ts_100us, i);
				area[i] = (long)loading;
				fpsgo_systrace_c_fbt_debug(ploading->pid, ploading->buffer_id,
					loading, "q2q_loading_cl[%d]", i);
			}
		} else
			ret = fbt_get_cl_loading_exp(ploading->pid, ploading->buffer_id,
				ploading->last_cb_ts, &loading, new_ts_100us, prefer_cluster);
	} else {
		xgf_trace("[XGFF][%s] !ploading", __func__);
		ret = 1;
	}

out:
	ploading->last_cb_ts = new_ts_100us;
	fpsgo_systrace_c_fbt_debug(ploading->pid, ploading->buffer_id,
		new_ts_100us, "xgff_last_cb_ts");

	if (!area)
		fpsgo_systrace_c_fbt_debug(ploading->pid, ploading->buffer_id,
			loading, "xgff_loading");

	return (long)loading;
}

static void fbt_xgff_set_min_cap(unsigned int min_cap)
{
	int tgt_opp, tgt_freq, fbt_min_cap;

	if (min_cap > 1024)
		min_cap = 1024;

	fbt_min_cap = (min_cap * 100 / 1024) + 1;
	tgt_opp = fbt_get_opp_by_normalized_cap(fbt_min_cap, 0);
	tgt_freq = cpu_dvfs[0].power[tgt_opp];
	fbt_cpu_L_ceiling_min(tgt_freq);
}

struct fbt_thread_blc *fbt_xgff_list_blc_add(int pid,
	unsigned long long buffer_id)
{
	struct fbt_thread_blc *obj;

	obj = kzalloc(sizeof(struct fbt_thread_blc), GFP_KERNEL);
	if (!obj) {
		FPSGO_LOGE("ERROR OOM\n");
		return NULL;
	}

	INIT_LIST_HEAD(&obj->entry);
	obj->blc = 0;
	obj->pid = pid;
	obj->buffer_id = buffer_id;
	obj->dep_num = 0;

	mutex_lock(&blc_mlock);
	list_add(&obj->entry, &blc_list);
	mutex_unlock(&blc_mlock);

	return obj;
}

void fbt_xgff_list_blc_del(struct fbt_thread_blc *p_blc)
{
	mutex_lock(&blc_mlock);
	list_del(&p_blc->entry);
	kfree(p_blc);
	mutex_unlock(&blc_mlock);
}

void fbt_xgff_blc_set(struct fbt_thread_blc *p_blc, int blc_wt,
	int dep_num, int *dep_arr)
{
	int i;

	mutex_lock(&blc_mlock);
	if (p_blc) {
		p_blc->blc = blc_wt;
		p_blc->dep_num = dep_num;
		if (dep_arr) {
			for (i = 0; i < dep_num; i++)
				p_blc->dep[i].pid = dep_arr[i];
		} else
			p_blc->dep_num = 0;
	}
	mutex_unlock(&blc_mlock);
}

static struct cam_dep_thread *fbt_xgff_dep_thread_notify_add(int pid, int force)
{
	struct rb_node **p = &cam_dep_thread_tree.rb_node;
	struct rb_node *parent = NULL;
	struct cam_dep_thread *iter = NULL;

	while (*p) {
		parent = *p;
		iter = rb_entry(parent, struct cam_dep_thread, rb_node);

		if (pid < iter->pid)
			p = &(*p)->rb_left;
		else if (pid > iter->pid)
			p = &(*p)->rb_right;
		else
			return iter;
	}

	if (!force)
		return NULL;

	iter = kzalloc(sizeof(*iter), GFP_KERNEL);
	if (!iter)
		return NULL;
	iter->pid = pid;

	rb_link_node(&iter->rb_node, parent, p);
	rb_insert_color(&iter->rb_node, &cam_dep_thread_tree);

	return iter;
}

static void fbt_xgff_dep_thread_notify_del(int pid)
{
	struct cam_dep_thread *iter;

	iter = fbt_xgff_dep_thread_notify_add(pid, 0);

	if (!iter)
		return;

	rb_erase(&iter->rb_node, &cam_dep_thread_tree);
	kfree(iter);
}

int fbt_xgff_dep_thread_notify(int pid, int op)
{
	int ret = 0;
	struct cam_dep_thread *iter = NULL;

	mutex_lock(&cam_dep_lock);

	if (op == 1)
		iter = fbt_xgff_dep_thread_notify_add(pid, 1);
	else if (op == 0)
		iter = fbt_xgff_dep_thread_notify_add(pid, 0);
	else if (op == -1)
		fbt_xgff_dep_thread_notify_del(pid);

	if (iter)
		ret = 1;

	mutex_unlock(&cam_dep_lock);

	return ret;
}

static ssize_t light_loading_policy_show(struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{
	char *temp = NULL;
	int posi = 0;
	int length = 0;

	temp = kcalloc(FPSGO_SYSFS_MAX_BUFF_SIZE, sizeof(char), GFP_KERNEL);
	if (!temp)
		goto out;

	mutex_lock(&fbt_mlock);
	length = scnprintf(temp + posi,
			FPSGO_SYSFS_MAX_BUFF_SIZE - posi,
			"light loading policy:%d\n", loading_policy);
	posi += length;
	length = scnprintf(temp + posi,
			FPSGO_SYSFS_MAX_BUFF_SIZE - posi,
			"0 means bypass, other value(1-100) means percent.\n");
	posi += length;
	mutex_unlock(&fbt_mlock);

	length = scnprintf(buf, PAGE_SIZE, "%s", temp);

out:
	kfree(temp);
	return length;
}

static ssize_t light_loading_policy_store(struct kobject *kobj,
		struct kobj_attribute *attr,
		const char *buf, size_t count)
{
	int val = -1;
	char *acBuffer = NULL;
	int arg;

	acBuffer = kcalloc(FPSGO_SYSFS_MAX_BUFF_SIZE, sizeof(char), GFP_KERNEL);
	if (!acBuffer)
		goto out;

	if ((count > 0) && (count < FPSGO_SYSFS_MAX_BUFF_SIZE)) {
		if (scnprintf(acBuffer, FPSGO_SYSFS_MAX_BUFF_SIZE, "%s", buf)) {
			if (kstrtoint(acBuffer, 0, &arg) == 0)
				val = arg;
			else
				goto out;
		}
	}

	if (val < 0 || val > 100)
		goto out;

	loading_policy = val;

out:
	kfree(acBuffer);
	return count;
}

static KOBJ_ATTR_RW(light_loading_policy);

static ssize_t switch_idleprefer_show(struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{
	int val = -1;

	mutex_lock(&fbt_mlock);
	val = fbt_idleprefer_enable;
	mutex_unlock(&fbt_mlock);

	return scnprintf(buf, PAGE_SIZE, "fbt_idleprefer_enable:%d\n", val);
}

static ssize_t switch_idleprefer_store(struct kobject *kobj,
		struct kobj_attribute *attr,
		const char *buf, size_t count)
{
	int val = -1;
	char *acBuffer = NULL;
	int arg;

	acBuffer = kcalloc(FPSGO_SYSFS_MAX_BUFF_SIZE, sizeof(char), GFP_KERNEL);
	if (!acBuffer)
		goto out;

	if ((count > 0) && (count < FPSGO_SYSFS_MAX_BUFF_SIZE)) {
		if (scnprintf(acBuffer, FPSGO_SYSFS_MAX_BUFF_SIZE, "%s", buf)) {
			if (kstrtoint(acBuffer, 0, &arg) == 0)
				val = arg;
			else
				goto out;
		}
	}

	fbt_switch_idleprefer(val);

out:
	kfree(acBuffer);
	return count;
}

static KOBJ_ATTR_RW(switch_idleprefer);

static ssize_t fbt_info_show(struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{
	struct fbt_thread_blc *pos, *next;
	char *temp = NULL;
	int posi = 0;
	int length = 0;
	int cluster;

	temp = kcalloc(FPSGO_SYSFS_MAX_BUFF_SIZE, sizeof(char), GFP_KERNEL);
	if (!temp)
		goto out;

	mutex_lock(&fbt_mlock);

	length = scnprintf(temp + posi,
		FPSGO_SYSFS_MAX_BUFF_SIZE - posi,
		"#clus\tmax\tmin\t\n");
	posi += length;

	length = scnprintf(temp + posi, FPSGO_SYSFS_MAX_BUFF_SIZE - posi,
		"%d\t%d\t%d\t\n\n",
		cluster_num, max_cap_cluster, min_cap_cluster);
	posi += length;

	length = scnprintf(temp + posi,
		FPSGO_SYSFS_MAX_BUFF_SIZE - posi,
		"clus\tnum\tc\tr\t\n");
	posi += length;

	for (cluster = 0; cluster < cluster_num ; cluster++) {
		length = scnprintf(temp + posi,
			FPSGO_SYSFS_MAX_BUFF_SIZE - posi,
			"%d\t%d\t%d\t%d\n",
			cluster, cpu_dvfs[cluster].num_cpu,
			limit_clus_ceil[cluster].copp,
			limit_clus_ceil[cluster].ropp);
		posi += length;
	}

	length = scnprintf(temp + posi,
		FPSGO_SYSFS_MAX_BUFF_SIZE - posi,
		"enable\tidleprefer\tmax_blc\tmax_pid\tmax_bufID\tdfps\tvsync\n");
	posi += length;

	length = scnprintf(temp + posi,
		FPSGO_SYSFS_MAX_BUFF_SIZE - posi,
		"%d\t%d\t\t%d\t%d\t0x%llx\t%d\t%llu\n\n",
		fbt_enable,
		set_idleprefer, max_blc, max_blc_pid,
		max_blc_buffer_id, _gdfrc_fps_limit, vsync_time);
	posi += length;

	mutex_unlock(&fbt_mlock);

	length = scnprintf(temp + posi,
		FPSGO_SYSFS_MAX_BUFF_SIZE - posi,
		"pid\tbufid\t\tperfidx\t\n");
	posi += length;

	mutex_lock(&blc_mlock);
	list_for_each_entry_safe(pos, next, &blc_list, entry) {
		length = scnprintf(temp + posi,
			FPSGO_SYSFS_MAX_BUFF_SIZE - posi,
			"%d\t0x%llx\t%d\n", pos->pid, pos->buffer_id, pos->blc);
		posi += length;
	}
	mutex_unlock(&blc_mlock);

	length = scnprintf(buf, PAGE_SIZE, "%s", temp);

out:
	kfree(temp);
	return length;
}

static KOBJ_ATTR_RO(fbt_info);

static ssize_t table_freq_show(struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{
	int cluster, opp;
	char *temp = NULL;
	int posi = 0;
	int length = 0;

	temp = kcalloc(FPSGO_SYSFS_MAX_BUFF_SIZE, sizeof(char), GFP_KERNEL);
	if (!temp)
		goto out;

	mutex_lock(&fbt_mlock);

	for (cluster = 0; cluster < cluster_num ; cluster++) {
		for (opp = 0; opp < nr_freq_cpu; opp++) {
			length = scnprintf(temp + posi,
				FPSGO_SYSFS_MAX_BUFF_SIZE - posi,
				"%d/%2d %d\n",
				cluster, opp,
				cpu_dvfs[cluster].power[opp] / 1000);
			posi += length;
		}
	}

	mutex_unlock(&fbt_mlock);

	length = scnprintf(buf, PAGE_SIZE, "%s", temp);

out:
	kfree(temp);
	return length;
}

static KOBJ_ATTR_RO(table_freq);

static ssize_t table_cap_show(struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{
	int cluster, opp;
	char *temp = NULL;
	int posi = 0;
	int length = 0;

	temp = kcalloc(FPSGO_SYSFS_MAX_BUFF_SIZE, sizeof(char), GFP_KERNEL);
	if (!temp)
		goto out;

	mutex_lock(&fbt_mlock);

	for (cluster = 0; cluster < cluster_num ; cluster++) {
		for (opp = 0; opp < nr_freq_cpu; opp++) {
			length = scnprintf(temp + posi,
				FPSGO_SYSFS_MAX_BUFF_SIZE - posi,
				"%d/%2d %d\n",
				cluster, opp,
				cpu_dvfs[cluster].capacity_ratio[opp]);
			posi += length;
		}
	}

	mutex_unlock(&fbt_mlock);

	length = scnprintf(buf, PAGE_SIZE, "%s", temp);

out:
	kfree(temp);
	return length;
}

static KOBJ_ATTR_RO(table_cap);

static ssize_t enable_uclamp_boost_show(struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{
	char *temp = NULL;
	int posi = 0;
	int length = 0;

	temp = kcalloc(FPSGO_SYSFS_MAX_BUFF_SIZE, sizeof(char), GFP_KERNEL);
	if (!temp)
		goto out;

	mutex_lock(&fbt_mlock);
	length = scnprintf(temp + posi,
		FPSGO_SYSFS_MAX_BUFF_SIZE - posi,
		"%s uclamp boost\n",
		uclamp_boost_enable?"enable":"disable");
	posi += length;
	mutex_unlock(&fbt_mlock);

	length = scnprintf(buf, PAGE_SIZE, "%s", temp);

out:
	kfree(temp);
	return length;
}

static KOBJ_ATTR_RO(enable_uclamp_boost);

static ssize_t boost_ta_show(struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{
	int val = -1;

	mutex_lock(&fbt_mlock);
	val = boost_ta;
	mutex_unlock(&fbt_mlock);

	return scnprintf(buf, PAGE_SIZE, "%d\n", val);
}

static ssize_t boost_ta_store(struct kobject *kobj,
		struct kobj_attribute *attr,
		const char *buf, size_t count)
{
	int val = -1;
	char *acBuffer = NULL;
	int arg;

	acBuffer = kcalloc(FPSGO_SYSFS_MAX_BUFF_SIZE, sizeof(char), GFP_KERNEL);
	if (!acBuffer)
		goto out;

	if ((count > 0) && (count < FPSGO_SYSFS_MAX_BUFF_SIZE)) {
		if (scnprintf(acBuffer, FPSGO_SYSFS_MAX_BUFF_SIZE, "%s", buf)) {
			if (kstrtoint(acBuffer, 0, &arg) == 0)
				val = arg;
			else
				goto out;
		}
	}

	fbt_switch_to_ta(val);

out:
	kfree(acBuffer);
	return count;
}

static KOBJ_ATTR_RW(boost_ta);

static ssize_t enable_switch_down_throttle_show(struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{
	char *temp = NULL;
	int posi = 0;
	int length = 0;

	temp = kcalloc(FPSGO_SYSFS_MAX_BUFF_SIZE, sizeof(char), GFP_KERNEL);
	if (!temp)
		goto out;

	mutex_lock(&fbt_mlock);
	length = scnprintf(temp + posi,
		FPSGO_SYSFS_MAX_BUFF_SIZE - posi,
		"fbt_down_throttle_enable %d\n",
		fbt_down_throttle_enable);
	posi += length;

	length = scnprintf(temp + posi,
		FPSGO_SYSFS_MAX_BUFF_SIZE - posi,
		"down_throttle_ns %d\n", down_throttle_ns);
	posi += length;

	mutex_unlock(&fbt_mlock);

	length = scnprintf(buf, PAGE_SIZE, "%s", temp);

out:
	kfree(temp);
	return length;
}

static ssize_t enable_switch_down_throttle_store(struct kobject *kobj,
		struct kobj_attribute *attr,
		const char *buf, size_t count)
{
	int val = -1;
	char *acBuffer;
	int arg;

	acBuffer =
		kcalloc(FPSGO_SYSFS_MAX_BUFF_SIZE, sizeof(char), GFP_KERNEL);
	if (!acBuffer)
		return count;

	if ((count > 0) && (count < FPSGO_SYSFS_MAX_BUFF_SIZE)) {
		if (scnprintf(acBuffer, FPSGO_SYSFS_MAX_BUFF_SIZE, "%s", buf)) {
			if (kstrtoint(acBuffer, 0, &arg) == 0)
				val = arg;
			else {
				kfree(acBuffer);
				return count;
			}
		}
	}

	mutex_lock(&fbt_mlock);

	if (!fbt_enable) {
		mutex_unlock(&fbt_mlock);
		kfree(acBuffer);
		return count;
	}

	if (!val && down_throttle_ns != -1)
		fbt_set_down_throttle_locked(-1);

	fbt_down_throttle_enable = val;

	mutex_unlock(&fbt_mlock);

	kfree(acBuffer);
	return count;
}

static KOBJ_ATTR_RW(enable_switch_down_throttle);

static ssize_t ultra_rescue_show(struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{
	int val = -1;

	mutex_lock(&fbt_mlock);
	val = ultra_rescue;
	mutex_unlock(&fbt_mlock);

	return scnprintf(buf, PAGE_SIZE, "%d\n", val);
}

static ssize_t ultra_rescue_store(struct kobject *kobj,
		struct kobj_attribute *attr,
		const char *buf, size_t count)
{
	int val = -1;
	char *acBuffer = NULL;
	int arg;

	acBuffer = kcalloc(FPSGO_SYSFS_MAX_BUFF_SIZE, sizeof(char), GFP_KERNEL);
	if (!acBuffer)
		goto out;

	if ((count > 0) && (count < FPSGO_SYSFS_MAX_BUFF_SIZE)) {
		if (scnprintf(acBuffer, FPSGO_SYSFS_MAX_BUFF_SIZE, "%s", buf)) {
			if (kstrtoint(acBuffer, 0, &arg) == 0)
				val = arg;
			else
				goto out;
		}
	}

	mutex_lock(&fbt_mlock);

	if (!fbt_enable) {
		mutex_unlock(&fbt_mlock);
		goto out;
	}

	fbt_set_ultra_rescue_locked(val);

	mutex_unlock(&fbt_mlock);

out:
	kfree(acBuffer);
	return count;
}

static KOBJ_ATTR_RW(ultra_rescue);

#if FPSGO_MW
static ssize_t fbt_attr_by_pid_store(struct kobject *kobj,
		struct kobj_attribute *attr,
		const char *buf, size_t count)
{
	int ret;
	char cmd[64];
	u32 pid;
	int val;
	char action;
	struct fpsgo_attr_by_pid *attr_render = NULL;
	struct fpsgo_boost_attr *boost_attr;
	int delete = 0;

	ret = sscanf(buf, "%63s %c %d %d", cmd, &action, &pid, &val);
	if (ret < 1) {
		ret = -EPERM;
		goto out;
	}

	FPSGO_LOGI("fpsgo cmd is %63s, action %c, pid %d, val %d", cmd, action, pid, val);

	fpsgo_render_tree_lock(__func__);
	attr_render = fpsgo_find_attr_by_pid(pid, 1);
	if (!attr_render) {
		FPSGO_LOGI("pid %d store error\n", pid);
		ret = -1;
		goto unlock_out;
	}

	boost_attr = &(attr_render->attr);
	if (val == BY_PID_DELETE_VAL && action == 'u') {
		delete = 1;
		goto delete_pid;
	}

	if (!strcmp(cmd, "rescue_second_enable")) {
		if ((val == 0 || val == 1) && action == 's')
			boost_attr->rescue_second_enable_by_pid = val;
		else if (val == BY_PID_DEFAULT_VAL && action == 'u')
			boost_attr->rescue_second_enable_by_pid = BY_PID_DEFAULT_VAL;
	} else if (!strcmp(cmd, "rescue_second_time")) {
		if (val <= 10 && val >= 1 && action == 's')
			boost_attr->rescue_second_time_by_pid = val;
		else if (val == BY_PID_DEFAULT_VAL && action == 'u')
			boost_attr->rescue_second_time_by_pid = BY_PID_DEFAULT_VAL;
	} else if (!strcmp(cmd, "rescue_second_group")) {
		if ((val == 0 || val == 1) && action == 's')
			boost_attr->rescue_second_group_by_pid = val;
		else if (val == BY_PID_DEFAULT_VAL && action == 'u')
			boost_attr->rescue_second_group_by_pid = BY_PID_DEFAULT_VAL;
	} else if (!strcmp(cmd, "loading_th")) {
		if (val <= 100 && val >= 0 && action == 's')
			boost_attr->loading_th_by_pid = val;
		else if (val == BY_PID_DEFAULT_VAL && action == 'u')
			boost_attr->loading_th_by_pid = BY_PID_DEFAULT_VAL;
	} else if (!strcmp(cmd, "light_loading_policy")) {
		if (val <= 100 && val >= 0 && action == 's')
			boost_attr->light_loading_policy_by_pid = val;
		else if (val == BY_PID_DEFAULT_VAL && action == 'u')
			boost_attr->light_loading_policy_by_pid = BY_PID_DEFAULT_VAL;
	} else if (!strcmp(cmd, "llf_task_policy")) {
		if (val < FPSGO_TPOLICY_MAX && val >= FPSGO_TPOLICY_NONE
			&& action == 's') {
			fpsgo_clear_llf_cpu_policy_by_pid(pid);
			boost_attr->llf_task_policy_by_pid = val;
		} else if (val == BY_PID_DEFAULT_VAL && action == 'u') {
			boost_attr->llf_task_policy_by_pid = BY_PID_DEFAULT_VAL;
		}
	} else if (!strcmp(cmd, "filter_frame_enable")) {
		if ((val == 1 || val == 0) && action == 's')
			boost_attr->filter_frame_enable_by_pid = val;
		else if (val == BY_PID_DEFAULT_VAL && action == 'u')
			boost_attr->filter_frame_enable_by_pid = val;
	} else if (!strcmp(cmd, "filter_frame_window_size")) {
		int ff_kmin = filter_frame_kmin;

		if (boost_attr->filter_frame_kmin_by_pid != BY_PID_DEFAULT_VAL)
			ff_kmin = boost_attr->filter_frame_kmin_by_pid;
		if (val >= ff_kmin && val <= FBT_FILTER_MAX_WINDOW
			&& action == 's')
			boost_attr->filter_frame_window_size_by_pid = val;
		else if (val == BY_PID_DEFAULT_VAL && action == 'u')
			boost_attr->filter_frame_window_size_by_pid = BY_PID_DEFAULT_VAL;
	} else if (!strcmp(cmd, "filter_frame_kmin")) {
		int ff_window_size = filter_frame_window_size;

		if (boost_attr->filter_frame_window_size_by_pid != BY_PID_DEFAULT_VAL)
			ff_window_size = boost_attr->filter_frame_window_size_by_pid;
		if (val > 0 && val <= ff_window_size && action == 's')
			boost_attr->filter_frame_kmin_by_pid = val;
		else if (val == BY_PID_DEFAULT_VAL && action == 'u')
			boost_attr->filter_frame_kmin_by_pid = BY_PID_DEFAULT_VAL;
	} else if (!strcmp(cmd, "boost_affinity")) {
		if (val >= FPSGO_TPOLICY_NONE && val < FPSGO_TPOLICY_MAX
			&& action == 's')
			boost_attr->boost_affinity_by_pid = val;
		else if (val == BY_PID_DEFAULT_VAL && action == 'u')
			boost_attr->boost_affinity_by_pid = BY_PID_DEFAULT_VAL;
	} else if (!strcmp(cmd, "boost_lr")) {
		if ((val == 0 || val == 1) && action == 's')
			boost_attr->boost_lr_by_pid = val;
		else if (val == BY_PID_DEFAULT_VAL)
			boost_attr->boost_lr_by_pid = BY_PID_DEFAULT_VAL;
	} else if (!strcmp(cmd, "separate_aa")) {
		if ((val == 0 || val == 1) && action == 's')
			boost_attr->separate_aa_by_pid = val;
		else if (val == BY_PID_DEFAULT_VAL && action == 'u')
			boost_attr->separate_aa_by_pid = BY_PID_DEFAULT_VAL;
	} else if  (!strcmp(cmd, "separate_release_sec")) {
		if ((val == 0 || val == 1) && action == 's')
			boost_attr->separate_release_sec_by_pid = val;
		else if (val == BY_PID_DEFAULT_VAL && action == 'u')
			boost_attr->separate_release_sec_by_pid = BY_PID_DEFAULT_VAL;
	} else if (!strcmp(cmd, "limit_uclamp")) {
		if (val >= 0 && val < 100 && action == 's')
			boost_attr->limit_uclamp_by_pid = val;
		else if (val == BY_PID_DEFAULT_VAL && action == 'u')
			boost_attr->limit_uclamp_by_pid = BY_PID_DEFAULT_VAL;
	} else if (!strcmp(cmd, "limit_ruclamp")) {
		if (val >= 0 && val < 100 && action == 's')
			boost_attr->limit_ruclamp_by_pid = val;
		else if (val == BY_PID_DEFAULT_VAL && action == 'u')
			boost_attr->limit_ruclamp_by_pid = BY_PID_DEFAULT_VAL;
	} else if (!strcmp(cmd, "limit_uclamp_m")) {
		if (val >= 0 && val < 100 && action == 's')
			boost_attr->limit_uclamp_m_by_pid = val;
		else if (val == BY_PID_DEFAULT_VAL && action == 'u')
			boost_attr->limit_uclamp_m_by_pid = BY_PID_DEFAULT_VAL;
	} else if (!strcmp(cmd, "limit_ruclamp_m")) {
		if (val >= 0 && val < 100 && action == 's')
			boost_attr->limit_ruclamp_m_by_pid = val;
		else if (val == BY_PID_DEFAULT_VAL && action == 'u')
			boost_attr->limit_ruclamp_m_by_pid = BY_PID_DEFAULT_VAL;
	} else if (!strcmp(cmd, "separate_pct_b")) {
		if (val >= 0 && val < 200 && action == 's')
			boost_attr->separate_pct_b_by_pid = val;
		else if (val == BY_PID_DEFAULT_VAL && action == 'u')
			boost_attr->separate_pct_b_by_pid = BY_PID_DEFAULT_VAL;
	} else if (!strcmp(cmd, "separate_pct_m")) {
		if (val >= 0 && val < 200 && action == 's')
			boost_attr->separate_pct_m_by_pid = val;
		else if (val == BY_PID_DEFAULT_VAL && action == 'u')
			boost_attr->separate_pct_m_by_pid = BY_PID_DEFAULT_VAL;
	} else if (!strcmp(cmd, "blc_boost")) {
		if (val >= 0 && val < 200 && action == 's')
			boost_attr->blc_boost_by_pid = val;
		else if (val == BY_PID_DEFAULT_VAL && action == 'u')
			boost_attr->blc_boost_by_pid = BY_PID_DEFAULT_VAL;
	} else if (!strcmp(cmd, "qr_enable")) {
		if ((val == 0 || val == 1) && action == 's')
			boost_attr->qr_enable_by_pid = val;
		else if (val == BY_PID_DEFAULT_VAL && action == 'u')
			boost_attr->qr_enable_by_pid = BY_PID_DEFAULT_VAL;
	} else if (!strcmp(cmd, "qr_t2wnt_x")) {
		if ((val >= -100 && val <= 100) && action == 's')
			boost_attr->qr_t2wnt_x_by_pid = val;
		else if (val == BY_PID_DEFAULT_VAL && action == 'u')
			boost_attr->qr_t2wnt_x_by_pid = BY_PID_DEFAULT_VAL;
	} else if (!strcmp(cmd, "qr_t2wnt_y_p")) {
		if (val >= 0 && val <= 100 && action == 's')
			boost_attr->qr_t2wnt_y_p_by_pid = val;
		else if (val == BY_PID_DEFAULT_VAL && action == 'u')
			boost_attr->qr_t2wnt_y_p_by_pid = BY_PID_DEFAULT_VAL;
	} else if (!strcmp(cmd, "qr_t2wnt_y_n")) {
		if (val >= 0 && val <= 100 && action == 's')
			boost_attr->qr_t2wnt_y_n_by_pid = val;
		else if (val == BY_PID_DEFAULT_VAL && action == 'u')
			boost_attr->qr_t2wnt_y_n_by_pid = BY_PID_DEFAULT_VAL;
	} else if (!strcmp(cmd, "gcc_enable")) {
		if ((val == 0 || val == 1) && action == 's')
			boost_attr->gcc_enable_by_pid = val;
		else if (val == BY_PID_DEFAULT_VAL && action == 'u')
			boost_attr->gcc_enable_by_pid = BY_PID_DEFAULT_VAL;
	} else if (!strcmp(cmd, "gcc_fps_margin")) {
		if (val <= 6000 && val >= -6000 && action == 's')
			boost_attr->gcc_fps_margin_by_pid = val;
		else if (val == BY_PID_DEFAULT_VAL && action == 'u')
			boost_attr->gcc_fps_margin_by_pid = BY_PID_DEFAULT_VAL;
	} else if (!strcmp(cmd, "gcc_up_sec_pct")) {
		if (val <= 100 && val >= 0 && action == 's')
			boost_attr->gcc_up_sec_pct_by_pid = val;
		else if (val == BY_PID_DEFAULT_VAL && action == 'u')
			boost_attr->gcc_up_sec_pct_by_pid = BY_PID_DEFAULT_VAL;
	} else if (!strcmp(cmd, "gcc_down_sec_pct")) {
		if (val <= 100 && val >= 0 && action == 's')
			boost_attr->gcc_down_sec_pct_by_pid = val;
		else if (val == BY_PID_DEFAULT_VAL && action == 'u')
			boost_attr->gcc_down_sec_pct_by_pid = BY_PID_DEFAULT_VAL;
	} else if (!strcmp(cmd, "gcc_up_step")) {
		if (val <= 100 && val >= 0 && action == 's')
			boost_attr->gcc_up_step_by_pid = val;
		else if (val == BY_PID_DEFAULT_VAL && action == 'u')
			boost_attr->gcc_up_step_by_pid = BY_PID_DEFAULT_VAL;
	} else if (!strcmp(cmd, "gcc_down_step")) {
		if (val <= 100 && val >= 0 && action == 's')
			boost_attr->gcc_down_step_by_pid = val;
		else if (val == BY_PID_DEFAULT_VAL && action == 'u')
			boost_attr->gcc_down_step_by_pid = BY_PID_DEFAULT_VAL;
	} else if (!strcmp(cmd, "gcc_reserved_up_quota_pct")) {
		if (val <= 100 && val >= 0 && action == 's')
			boost_attr->gcc_reserved_up_quota_pct_by_pid = val;
		else if (val == BY_PID_DEFAULT_VAL && action == 'u')
			boost_attr->gcc_reserved_up_quota_pct_by_pid =
				BY_PID_DEFAULT_VAL;
	} else if (!strcmp(cmd, "gcc_reserved_down_quota_pct")) {
		if (val <= 100 && val >= 0 && action == 's')
			boost_attr->gcc_reserved_down_quota_pct_by_pid = val;
		else if (val == BY_PID_DEFAULT_VAL && action == 'u')
			boost_attr->gcc_reserved_down_quota_pct_by_pid =
				BY_PID_DEFAULT_VAL;
	} else if (!strcmp(cmd, "gcc_enq_bound_thrs")) {
		if (val <= 200 && val >= 0 && action == 's')
			boost_attr->gcc_enq_bound_thrs_by_pid = val;
		else if (val == BY_PID_DEFAULT_VAL && action == 'u')
			boost_attr->gcc_enq_bound_thrs_by_pid = BY_PID_DEFAULT_VAL;
	} else if (!strcmp(cmd, "gcc_enq_bound_quota")) {
		if (val <= 200 && val >= 0 && action == 's')
			boost_attr->gcc_enq_bound_quota_by_pid = val;
		else if (val == BY_PID_DEFAULT_VAL && action == 'u')
			boost_attr->gcc_enq_bound_quota_by_pid = BY_PID_DEFAULT_VAL;
	} else if (!strcmp(cmd, "gcc_deq_bound_thrs")) {
		if (val <= 200 && val >= 0 && action == 's')
			boost_attr->gcc_deq_bound_thrs_by_pid = val;
		else if (val == BY_PID_DEFAULT_VAL && action == 'u')
			boost_attr->gcc_deq_bound_thrs_by_pid = BY_PID_DEFAULT_VAL;
	} else if (!strcmp(cmd, "gcc_deq_bound_quota")) {
		if (val <= 200 && val >= 0 && action == 's')
			boost_attr->gcc_deq_bound_quota_by_pid = val;
		else if (val == BY_PID_DEFAULT_VAL && action == 'u')
			boost_attr->gcc_deq_bound_quota_by_pid = BY_PID_DEFAULT_VAL;
	}

delete_pid:
	if (delete) {
		fpsgo_reset_render_pid_attr(pid);
		delete_attr_by_pid(pid);
		goto unlock_out;
	}
	if (is_to_delete_fpsgo_attr(attr_render)) {
		fpsgo_reset_render_pid_attr(pid);
		delete_attr_by_pid(pid);
	}
unlock_out:
	fpsgo_render_tree_unlock(__func__);
out:
	if (ret < 0)
		return ret;
	return count;
}

static KOBJ_ATTR_WO(fbt_attr_by_pid);
#endif  // FPSGO_MW

static void llf_switch_policy(struct work_struct *work)
{
	fpsgo_main_trace("fpsgo %s and clear_llf_cpu_policy",
		__func__);
	fpsgo_clear_llf_cpu_policy();
}
static DECLARE_WORK(llf_switch_policy_work,
		(void *) llf_switch_policy);

static ssize_t llf_task_policy_show(struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{
	int val = -1;

	mutex_lock(&fbt_mlock);
	val = llf_task_policy;
	mutex_unlock(&fbt_mlock);

	return scnprintf(buf, PAGE_SIZE, "%d\n", val);
}

static ssize_t llf_task_policy_store(struct kobject *kobj,
		struct kobj_attribute *attr,
		const char *buf, size_t count)
{
	int val = 0;
	int orig_policy;
	char *acBuffer = NULL;
	int arg;

	acBuffer = kcalloc(FPSGO_SYSFS_MAX_BUFF_SIZE, sizeof(char), GFP_KERNEL);
	if (!acBuffer)
		goto out;

	if ((count > 0) && (count < FPSGO_SYSFS_MAX_BUFF_SIZE)) {
		if (scnprintf(acBuffer, FPSGO_SYSFS_MAX_BUFF_SIZE, "%s", buf)) {
			if (kstrtoint(acBuffer, 0, &arg) == 0)
				val = arg;
			else
				goto out;
		}
	}

	mutex_lock(&fbt_mlock);
	if (!fbt_enable) {
		mutex_unlock(&fbt_mlock);
		goto out;
	}

	if (val < FPSGO_TPOLICY_NONE || val >= FPSGO_TPOLICY_MAX) {
		mutex_unlock(&fbt_mlock);
		goto out;
	}

	if (llf_task_policy == val) {
		mutex_unlock(&fbt_mlock);
		goto out;
	}

	orig_policy = llf_task_policy;
	llf_task_policy = val;
	fpsgo_main_trace("fpsgo set llf_task_policy %d",
		llf_task_policy);
	mutex_unlock(&fbt_mlock);

	schedule_work(&llf_switch_policy_work);

out:
	kfree(acBuffer);
	return count;
}

static KOBJ_ATTR_RW(llf_task_policy);

static ssize_t enable_ceiling_show(struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{
	int val;

	mutex_lock(&fbt_mlock);
	val = enable_ceiling;
	mutex_unlock(&fbt_mlock);

	return scnprintf(buf, PAGE_SIZE, "%x\n", val);
}

static ssize_t enable_ceiling_store(struct kobject *kobj,
		struct kobj_attribute *attr,
		const char *buf, size_t count)
{
	int val = -1;
	char *acBuffer = NULL;
	int arg;

	acBuffer = kcalloc(FPSGO_SYSFS_MAX_BUFF_SIZE, sizeof(char), GFP_KERNEL);
	if (!acBuffer)
		goto out;

	if ((count > 0) && (count < FPSGO_SYSFS_MAX_BUFF_SIZE)) {
		if (scnprintf(acBuffer, FPSGO_SYSFS_MAX_BUFF_SIZE, "%s", buf)) {
			if (kstrtoint(acBuffer, 0, &arg) == 0)
				val = arg;
			else
				goto out;
		}
	}

	mutex_lock(&fbt_mlock);
	if (val)
		enable_ceiling |= (1 << FPSGO_CEILING_PROCFS);
	else
		enable_ceiling &= ~(1 << FPSGO_CEILING_PROCFS);
	mutex_unlock(&fbt_mlock);

out:
	kfree(acBuffer);
	return count;
}

static KOBJ_ATTR_RW(enable_ceiling);

static int is_cfreq_rfreq_limited(void)
{
	int i;

	for (i = 0; i < cluster_num; i++) {
		if (limit_clus_ceil[i].cfreq > 0 || limit_clus_ceil[i].rfreq > 0)
			return 1;
	}

	return 0;
}

static void update_cfreq_rfreq_status_locked(void)
{
	int enable = 0;

	enable = is_cfreq_rfreq_limited();

	if (enable)
		enable_ceiling |= (1 << FPSGO_CEILING_LIMIT_FREQ);
	else
		enable_ceiling &= ~(1 << FPSGO_CEILING_LIMIT_FREQ);
}

static ssize_t limit_uclamp_show(struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{
	int val = -1;

	mutex_lock(&fbt_mlock);
	val = limit_uclamp;
	mutex_unlock(&fbt_mlock);

	return scnprintf(buf, PAGE_SIZE, "%d\n", val);
}

static ssize_t limit_uclamp_store(struct kobject *kobj,
		struct kobj_attribute *attr,
		const char *buf, size_t count)
{
	int val = -1;
	char *acBuffer = NULL;
	int arg;

	acBuffer = kcalloc(FPSGO_SYSFS_MAX_BUFF_SIZE, sizeof(char), GFP_KERNEL);
	if (!acBuffer)
		goto out;

	if ((count > 0) && (count < FPSGO_SYSFS_MAX_BUFF_SIZE)) {
		if (scnprintf(acBuffer, FPSGO_SYSFS_MAX_BUFF_SIZE, "%s", buf)) {
			if (kstrtoint(acBuffer, 0, &arg) == 0) {
				val = arg;
				if (val < 100 && val >= 0) {
					mutex_lock(&fbt_mlock);
					limit_uclamp = val;
					mutex_unlock(&fbt_mlock);
				}
			}
		}
	}

out:
	kfree(acBuffer);
	return count;
}

static KOBJ_ATTR_RW(limit_uclamp);

static ssize_t limit_ruclamp_show(struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{
	int val = -1;

	mutex_lock(&fbt_mlock);
	val = limit_ruclamp;
	mutex_unlock(&fbt_mlock);

	return scnprintf(buf, PAGE_SIZE, "%d\n", val);
}

static ssize_t limit_ruclamp_store(struct kobject *kobj,
		struct kobj_attribute *attr,
		const char *buf, size_t count)
{
	int val = -1;
	char *acBuffer = NULL;
	int arg;

	acBuffer = kcalloc(FPSGO_SYSFS_MAX_BUFF_SIZE, sizeof(char), GFP_KERNEL);
	if (!acBuffer)
		goto out;

	if ((count > 0) && (count < FPSGO_SYSFS_MAX_BUFF_SIZE)) {
		if (scnprintf(acBuffer, FPSGO_SYSFS_MAX_BUFF_SIZE, "%s", buf)) {
			if (kstrtoint(acBuffer, 0, &arg) == 0) {
				val = arg;
				if (val < 100 && val >= 0) {
					mutex_lock(&fbt_mlock);
					limit_ruclamp = val;
					mutex_unlock(&fbt_mlock);
				}
			}
		}
	}

out:
	kfree(acBuffer);
	return count;
}

static KOBJ_ATTR_RW(limit_ruclamp);

static ssize_t limit_uclamp_m_show(struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{
	int val = -1;

	mutex_lock(&fbt_mlock);
	val = limit_uclamp_m;
	mutex_unlock(&fbt_mlock);

	return scnprintf(buf, PAGE_SIZE, "%d\n", val);
}

static ssize_t limit_uclamp_m_store(struct kobject *kobj,
		struct kobj_attribute *attr,
		const char *buf, size_t count)
{
	int val = -1;
	char *acBuffer = NULL;
	int arg;

	acBuffer = kcalloc(FPSGO_SYSFS_MAX_BUFF_SIZE, sizeof(char), GFP_KERNEL);
	if (!acBuffer)
		goto out;

	if ((count > 0) && (count < FPSGO_SYSFS_MAX_BUFF_SIZE)) {
		if (scnprintf(acBuffer, FPSGO_SYSFS_MAX_BUFF_SIZE, "%s", buf)) {
			if (kstrtoint(acBuffer, 0, &arg) == 0) {
				val = arg;
				if (val < 100 && val >= 0) {
					mutex_lock(&fbt_mlock);
					limit_uclamp_m = val;
					mutex_unlock(&fbt_mlock);
				}
			}
		}
	}

out:
	kfree(acBuffer);
	return count;
}

static KOBJ_ATTR_RW(limit_uclamp_m);

static ssize_t limit_ruclamp_m_show(struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{
	int val = -1;

	mutex_lock(&fbt_mlock);
	val = limit_ruclamp_m;
	mutex_unlock(&fbt_mlock);

	return scnprintf(buf, PAGE_SIZE, "%d\n", val);
}

static ssize_t limit_ruclamp_m_store(struct kobject *kobj,
		struct kobj_attribute *attr,
		const char *buf, size_t count)
{
	int val = -1;
	char *acBuffer = NULL;
	int arg;

	acBuffer = kcalloc(FPSGO_SYSFS_MAX_BUFF_SIZE, sizeof(char), GFP_KERNEL);
	if (!acBuffer)
		goto out;

	if ((count > 0) && (count < FPSGO_SYSFS_MAX_BUFF_SIZE)) {
		if (scnprintf(acBuffer, FPSGO_SYSFS_MAX_BUFF_SIZE, "%s", buf)) {
			if (kstrtoint(acBuffer, 0, &arg) == 0) {
				val = arg;
				if (val < 100 && val >= 0) {
					mutex_lock(&fbt_mlock);
					limit_ruclamp_m = val;
					mutex_unlock(&fbt_mlock);
				}
			}
		}
	}

out:
	kfree(acBuffer);
	return count;
}

static KOBJ_ATTR_RW(limit_ruclamp_m);

static ssize_t limit_cfreq_show(struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{
	int cluster, val = 0;

	mutex_lock(&fbt_mlock);

	cluster = max_cap_cluster;
	if (cluster >= cluster_num || cluster < 0 || !limit_clus_ceil)
		goto EXIT;

	val = limit_clus_ceil[cluster].cfreq;

EXIT:
	mutex_unlock(&fbt_mlock);

	return scnprintf(buf, PAGE_SIZE, "%d\n", val);
}

static ssize_t limit_cfreq_store(struct kobject *kobj,
		struct kobj_attribute *attr,
		const char *buf, size_t count)
{
	int val = 0;
	char *acBuffer = NULL;
	int arg;
	int opp;
	int cluster;
	struct fbt_syslimit *limit;

	acBuffer = kcalloc(FPSGO_SYSFS_MAX_BUFF_SIZE, sizeof(char), GFP_KERNEL);
	if (!acBuffer)
		goto out;

	if ((count > 0) && (count < FPSGO_SYSFS_MAX_BUFF_SIZE)) {
		if (scnprintf(acBuffer, FPSGO_SYSFS_MAX_BUFF_SIZE, "%s", buf)) {
			if (kstrtoint(acBuffer, 0, &arg) == 0)
				val = arg;
			else
				goto out;
		}
	}

	mutex_lock(&fbt_mlock);
	if (!limit_clus_ceil)
		goto EXIT;

	cluster = max_cap_cluster;
	if (cluster >= cluster_num || cluster < 0)
		goto EXIT;

	limit = &limit_clus_ceil[cluster];

	if (val == 0) {
		limit->cfreq = 0;
		limit->copp = INVALID_NUM;
		limit_cap = 0;
		if (is_cfreq_rfreq_limited())
			limit_policy = FPSGO_LIMIT_CAPACITY;
		else
			limit_policy = FPSGO_LIMIT_NONE;
		goto EXIT;
	}

	opp = fbt_get_opp_by_freq(cluster, val);
	if (opp == INVALID_NUM)
		goto EXIT;

	limit->cfreq = cpu_dvfs[cluster].power[opp];
	limit->copp = opp;
	limit_policy = FPSGO_LIMIT_CAPACITY;
	limit_cap = check_limit_cap(0);

EXIT:
	update_cfreq_rfreq_status_locked();
	mutex_unlock(&fbt_mlock);

out:
	kfree(acBuffer);
	return count;
}

static KOBJ_ATTR_RW(limit_cfreq);

static ssize_t limit_cfreq_m_show(struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{
	int cluster, val = 0;

	mutex_lock(&fbt_mlock);

	cluster = sec_cap_cluster;
	if (cluster >= cluster_num || cluster < 0 || !limit_clus_ceil)
		goto EXIT;

	val = limit_clus_ceil[cluster].cfreq;

EXIT:
	mutex_unlock(&fbt_mlock);

	return scnprintf(buf, PAGE_SIZE, "%d\n", val);
}

static ssize_t limit_cfreq_m_store(struct kobject *kobj,
		struct kobj_attribute *attr,
		const char *buf, size_t count)
{
	int val = 0;
	char *acBuffer = NULL;
	int arg;
	int opp;
	int cluster;
	struct fbt_syslimit *limit;

	acBuffer = kcalloc(FPSGO_SYSFS_MAX_BUFF_SIZE, sizeof(char), GFP_KERNEL);
	if (!acBuffer)
		goto out;

	if ((count > 0) && (count < FPSGO_SYSFS_MAX_BUFF_SIZE)) {
		if (scnprintf(acBuffer, FPSGO_SYSFS_MAX_BUFF_SIZE, "%s", buf)) {
			if (kstrtoint(acBuffer, 0, &arg) == 0)
				val = arg;
			else
				goto out;
		}
	}

	mutex_lock(&fbt_mlock);
	if (!limit_clus_ceil)
		goto EXIT;

	cluster = (max_cap_cluster > min_cap_cluster)
			? max_cap_cluster - 1 : min_cap_cluster - 1;

	if (cluster >= cluster_num || cluster < 0)
		goto EXIT;

	limit = &limit_clus_ceil[cluster];

	if (val == 0) {
		limit->cfreq = 0;
		limit->copp = INVALID_NUM;
		if (is_cfreq_rfreq_limited())
			limit_policy = FPSGO_LIMIT_CAPACITY;
		else
			limit_policy = FPSGO_LIMIT_NONE;
		limit_cap = check_limit_cap(0);
		goto EXIT;
	}


	opp = fbt_get_opp_by_freq(cluster, val);
	if (opp == INVALID_NUM)
		goto EXIT;

	limit->cfreq = cpu_dvfs[cluster].power[opp];
	limit->copp = opp;
	limit_policy = FPSGO_LIMIT_CAPACITY;
	limit_cap = check_limit_cap(0);

EXIT:
	update_cfreq_rfreq_status_locked();
	mutex_unlock(&fbt_mlock);

out:
	kfree(acBuffer);
	return count;
}

static KOBJ_ATTR_RW(limit_cfreq_m);

static ssize_t limit_rfreq_show(struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{
	int cluster, val = 0;

	mutex_lock(&fbt_mlock);

	cluster = max_cap_cluster;
	if (cluster >= cluster_num || cluster < 0 || !limit_clus_ceil)
		goto EXIT;

	val = limit_clus_ceil[cluster].rfreq;

EXIT:
	mutex_unlock(&fbt_mlock);

	return scnprintf(buf, PAGE_SIZE, "%d\n", val);
}

static ssize_t limit_rfreq_store(struct kobject *kobj,
		struct kobj_attribute *attr,
		const char *buf, size_t count)
{
	int val = 0;
	char *acBuffer = NULL;
	int arg;
	int opp;
	int cluster;
	struct fbt_syslimit *limit;

	acBuffer = kcalloc(FPSGO_SYSFS_MAX_BUFF_SIZE, sizeof(char), GFP_KERNEL);
	if (!acBuffer)
		goto out;

	if ((count > 0) && (count < FPSGO_SYSFS_MAX_BUFF_SIZE)) {
		if (scnprintf(acBuffer, FPSGO_SYSFS_MAX_BUFF_SIZE, "%s", buf)) {
			if (kstrtoint(acBuffer, 0, &arg) == 0)
				val = arg;
			else
				goto out;
		}
	}

	mutex_lock(&fbt_mlock);
	if (limit_policy != FPSGO_LIMIT_CAPACITY || !limit_clus_ceil)
		goto EXIT;

	cluster = max_cap_cluster;
	if (cluster >= cluster_num || cluster < 0)
		goto EXIT;

	limit = &limit_clus_ceil[cluster];

	if (val == 0) {
		limit->rfreq = 0;
		limit->ropp = INVALID_NUM;
		limit_rcap = 0;
		if (is_cfreq_rfreq_limited())
			limit_policy = FPSGO_LIMIT_CAPACITY;
		else
			limit_policy = FPSGO_LIMIT_NONE;
		goto EXIT;
	}

	opp = fbt_get_opp_by_freq(cluster, val);
	if (opp == INVALID_NUM)
		goto EXIT;

	limit->rfreq = cpu_dvfs[cluster].power[opp];
	limit->ropp = opp;
	limit_rcap = check_limit_cap(1);

EXIT:
	update_cfreq_rfreq_status_locked();
	mutex_unlock(&fbt_mlock);

out:
	kfree(acBuffer);
	return count;
}

static KOBJ_ATTR_RW(limit_rfreq);

static ssize_t limit_rfreq_m_show(struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{
	int cluster, val = 0;

	mutex_lock(&fbt_mlock);

	cluster = (max_cap_cluster > min_cap_cluster)
			? max_cap_cluster - 1 : min_cap_cluster - 1;
	if (cluster >= cluster_num || cluster < 0 || !limit_clus_ceil)
		goto EXIT;

	val = limit_clus_ceil[cluster].rfreq;

EXIT:
	mutex_unlock(&fbt_mlock);

	return scnprintf(buf, PAGE_SIZE, "%d\n", val);
}

static ssize_t limit_rfreq_m_store(struct kobject *kobj,
		struct kobj_attribute *attr,
		const char *buf, size_t count)
{
	int val = 0;
	char *acBuffer = NULL;
	int arg;
	int opp;
	int cluster;
	struct fbt_syslimit *limit;

	acBuffer = kcalloc(FPSGO_SYSFS_MAX_BUFF_SIZE, sizeof(char), GFP_KERNEL);
	if (!acBuffer)
		goto out;

	if ((count > 0) && (count < FPSGO_SYSFS_MAX_BUFF_SIZE)) {
		if (scnprintf(acBuffer, FPSGO_SYSFS_MAX_BUFF_SIZE, "%s", buf)) {
			if (kstrtoint(acBuffer, 0, &arg) == 0)
				val = arg;
			else
				goto out;
		}
	}

	mutex_lock(&fbt_mlock);
	if (limit_policy != FPSGO_LIMIT_CAPACITY || !limit_clus_ceil)
		goto EXIT;

	cluster = (max_cap_cluster > min_cap_cluster)
			? max_cap_cluster - 1 : min_cap_cluster - 1;

	if (cluster >= cluster_num || cluster < 0)
		goto EXIT;

	limit = &limit_clus_ceil[cluster];

	if (val == 0) {
		limit->rfreq = 0;
		limit->ropp = INVALID_NUM;
		limit_rcap = check_limit_cap(1);
		if (is_cfreq_rfreq_limited())
			limit_policy = FPSGO_LIMIT_CAPACITY;
		else
			limit_policy = FPSGO_LIMIT_NONE;
		goto EXIT;
	}

	opp = fbt_get_opp_by_freq(cluster, val);
	if (opp == INVALID_NUM)
		goto EXIT;

	limit->rfreq = cpu_dvfs[cluster].power[opp];
	limit->ropp = opp;
	limit_rcap = check_limit_cap(1);

EXIT:
	update_cfreq_rfreq_status_locked();
	mutex_unlock(&fbt_mlock);

out:
	kfree(acBuffer);
	return count;
}

static KOBJ_ATTR_RW(limit_rfreq_m);

static ssize_t switch_filter_frame_show(struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{
	int val = -1;

	mutex_lock(&fbt_mlock);
	val = filter_frame_enable;
	mutex_unlock(&fbt_mlock);

	return scnprintf(buf, PAGE_SIZE, "filter frame enable: %d\n", val);
}

static ssize_t switch_filter_frame_store(struct kobject *kobj,
		struct kobj_attribute *attr,
		const char *buf, size_t count)
{
	int val = -1;
	char *acBuffer = NULL;
	int arg;

	acBuffer = kcalloc(FPSGO_SYSFS_MAX_BUFF_SIZE, sizeof(char), GFP_KERNEL);
	if (!acBuffer)
		goto out;

	if ((count > 0) && (count < FPSGO_SYSFS_MAX_BUFF_SIZE)) {
		if (scnprintf(acBuffer, FPSGO_SYSFS_MAX_BUFF_SIZE, "%s", buf)) {
			if (kstrtoint(acBuffer, 0, &arg) == 0)
				val = !!arg;
			else
				goto out;
		}
	}

	fbt_set_filter_frame_enable(val);

out:
	kfree(acBuffer);
	return count;
}

static KOBJ_ATTR_RW(switch_filter_frame);

static ssize_t filter_f_window_size_show(struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{
	int val = -1;

	mutex_lock(&fbt_mlock);
	val = filter_frame_window_size;
	mutex_unlock(&fbt_mlock);

	return scnprintf(buf, PAGE_SIZE, "filter frame window size: %d\n", val);
}

static ssize_t filter_f_window_size_store(struct kobject *kobj,
		struct kobj_attribute *attr,
		const char *buf, size_t count)
{
	int val = -1;
	char *acBuffer = NULL;
	int arg;

	acBuffer = kcalloc(FPSGO_SYSFS_MAX_BUFF_SIZE, sizeof(char), GFP_KERNEL);
	if (!acBuffer)
		goto out;

	if ((count > 0) && (count < FPSGO_SYSFS_MAX_BUFF_SIZE)) {
		if (scnprintf(acBuffer, FPSGO_SYSFS_MAX_BUFF_SIZE, "%s", buf)) {
			if (kstrtoint(acBuffer, 0, &arg) == 0)
				val = arg;
			else
				goto out;
		}
	}

	fbt_set_filter_frame_window_size(val);

out:
	kfree(acBuffer);
	return count;
}

static KOBJ_ATTR_RW(filter_f_window_size);

static ssize_t filter_f_kmin_show(struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{
	int val = -1;

	mutex_lock(&fbt_mlock);
	val = filter_frame_kmin;
	mutex_unlock(&fbt_mlock);

	return scnprintf(buf, PAGE_SIZE, "filter frame kmin: %d\n", val);
}

static ssize_t filter_f_kmin_store(struct kobject *kobj,
		struct kobj_attribute *attr,
		const char *buf, size_t count)
{
	int val = -1;
	char *acBuffer = NULL;
	int arg;

	acBuffer = kcalloc(FPSGO_SYSFS_MAX_BUFF_SIZE, sizeof(char), GFP_KERNEL);
	if (!acBuffer)
		goto out;

	if ((count > 0) && (count < FPSGO_SYSFS_MAX_BUFF_SIZE)) {
		if (scnprintf(acBuffer, FPSGO_SYSFS_MAX_BUFF_SIZE, "%s", buf)) {
			if (kstrtoint(acBuffer, 0, &arg) == 0)
				val = arg;
			else
				goto out;
		}
	}

	fbt_set_filter_frame_kmin(val);

out:
	kfree(acBuffer);
	return count;
}

static KOBJ_ATTR_RW(filter_f_kmin);

static ssize_t enable_separate_aa_show(struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{
	int val = -1;

	mutex_lock(&fbt_mlock);
	val = separate_aa;
	mutex_unlock(&fbt_mlock);
	return scnprintf(buf, PAGE_SIZE, "separate_aa enable: %d\n", val);
}

static ssize_t enable_separate_aa_store(struct kobject *kobj,
		struct kobj_attribute *attr,
		const char *buf, size_t count)
{
	int val = -1;
	char *acBuffer = NULL;
	int arg;

	acBuffer = kcalloc(FPSGO_SYSFS_MAX_BUFF_SIZE, sizeof(char), GFP_KERNEL);
	if (!acBuffer)
		goto out;

	if ((count > 0) && (count < FPSGO_SYSFS_MAX_BUFF_SIZE)) {
		if (scnprintf(acBuffer, FPSGO_SYSFS_MAX_BUFF_SIZE, "%s", buf)) {
			if (kstrtoint(acBuffer, 0, &arg) == 0) {
				val = !!arg;
				fbt_set_separate_aa(val);
			}
		}
	}

out:
	kfree(acBuffer);
	return count;
}

static KOBJ_ATTR_RW(enable_separate_aa);

static ssize_t separate_pct_b_show(struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{
	int val = -1;

	mutex_lock(&fbt_mlock);
	val = separate_pct_b;
	mutex_unlock(&fbt_mlock);
	return scnprintf(buf, PAGE_SIZE,
		"separate_aa lightloading percentage for big core: %d\n", val);
}

static ssize_t separate_pct_b_store(struct kobject *kobj,
		struct kobj_attribute *attr,
		const char *buf, size_t count)
{
	int val = -1;
	char *acBuffer = NULL;
	int arg;

	acBuffer = kcalloc(FPSGO_SYSFS_MAX_BUFF_SIZE, sizeof(char), GFP_KERNEL);
	if (!acBuffer)
		goto out;

	if ((count > 0) && (count < FPSGO_SYSFS_MAX_BUFF_SIZE)) {
		if (scnprintf(acBuffer, FPSGO_SYSFS_MAX_BUFF_SIZE, "%s", buf)) {
			if (kstrtoint(acBuffer, 0, &arg) == 0) {
				val = arg;
				if (val < 200 && val >= 0) {
					mutex_lock(&fbt_mlock);
					separate_pct_b = val;
					mutex_unlock(&fbt_mlock);
				}
			}
		}
	}

out:
	kfree(acBuffer);
	return count;
}

static KOBJ_ATTR_RW(separate_pct_b);

static ssize_t separate_pct_m_show(struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{
	int val = -1;

	mutex_lock(&fbt_mlock);
	val = separate_pct_m;
	mutex_unlock(&fbt_mlock);
	return scnprintf(buf, PAGE_SIZE,
		"separate_aa ligtloading percentage for middle core: %d\n", val);
}

static ssize_t separate_pct_m_store(struct kobject *kobj,
		struct kobj_attribute *attr,
		const char *buf, size_t count)
{
	int val = -1;
	char *acBuffer = NULL;
	int arg;

	acBuffer = kcalloc(FPSGO_SYSFS_MAX_BUFF_SIZE, sizeof(char), GFP_KERNEL);
	if (!acBuffer)
		goto out;

	if ((count > 0) && (count < FPSGO_SYSFS_MAX_BUFF_SIZE)) {
		if (scnprintf(acBuffer, FPSGO_SYSFS_MAX_BUFF_SIZE, "%s", buf)) {
			if (kstrtoint(acBuffer, 0, &arg) == 0) {
				val = arg;
				if (val < 200 && val >= 0) {
					mutex_lock(&fbt_mlock);
					separate_pct_m = val;
					mutex_unlock(&fbt_mlock);
				}
			}
		}
	}

out:
	kfree(acBuffer);
	return count;
}

static KOBJ_ATTR_RW(separate_pct_m);

static ssize_t separate_release_sec_show(struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{
	int val = -1;

	mutex_lock(&fbt_mlock);
	val = separate_release_sec;
	mutex_unlock(&fbt_mlock);
	return scnprintf(buf, PAGE_SIZE, "%d\n", val);
}

static ssize_t separate_release_sec_store(struct kobject *kobj,
		struct kobj_attribute *attr,
		const char *buf, size_t count)
{
	int val = -1;
	char *acBuffer = NULL;
	int arg;

	acBuffer = kcalloc(FPSGO_SYSFS_MAX_BUFF_SIZE, sizeof(char), GFP_KERNEL);
	if (!acBuffer)
		goto out;

	if ((count > 0) && (count < FPSGO_SYSFS_MAX_BUFF_SIZE)) {
		if (scnprintf(acBuffer, FPSGO_SYSFS_MAX_BUFF_SIZE, "%s", buf)) {
			if (kstrtoint(acBuffer, 0, &arg) == 0) {
				val = !!arg;
				mutex_lock(&fbt_mlock);
				separate_release_sec = val;
				mutex_unlock(&fbt_mlock);
			}
		}
	}

out:
	kfree(acBuffer);
	return count;
}

static KOBJ_ATTR_RW(separate_release_sec);

static ssize_t blc_boost_show(struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{
	int val = -1;

	mutex_lock(&fbt_mlock);
	val = blc_boost;
	mutex_unlock(&fbt_mlock);

	return scnprintf(buf, PAGE_SIZE, "%d\n", val);
}

static ssize_t blc_boost_store(struct kobject *kobj,
		struct kobj_attribute *attr,
		const char *buf, size_t count)
{
	int val = -1;
	char acBuffer[FPSGO_SYSFS_MAX_BUFF_SIZE];
	int arg;

	if ((count > 0) && (count < FPSGO_SYSFS_MAX_BUFF_SIZE)) {
		if (scnprintf(acBuffer, FPSGO_SYSFS_MAX_BUFF_SIZE, "%s", buf)) {
			if (kstrtoint(acBuffer, 0, &arg) == 0)
				val = arg;
			else
				return count;
		}
	}

	blc_boost = clamp(val, 0, 200);

	return count;
}

static KOBJ_ATTR_RW(blc_boost);

void fbt_init_cpu_loading_info(void)
{
	int i = 0, err_exit = 0;
	unsigned long lock_flag;

	spin_lock_irqsave(&loading_slock, lock_flag);
	lastest_idx = 0;
	last_cb_ts = 0;
	for (i = 0; i < LOADING_CNT; i++) {
		lastest_obv_cl[i] = kcalloc(cluster_num, sizeof(unsigned int), GFP_ATOMIC);
		lastest_is_cl_isolated[i] = kcalloc(cluster_num, sizeof(unsigned int), GFP_ATOMIC);
		if (!lastest_obv_cl[i] || !lastest_is_cl_isolated[i]) {
			err_exit = 1;
			FPSGO_LOGE("ERROR OOM\n");
			break;
		}
	}
	if (err_exit) {
		for (i = 0; i < LOADING_CNT; i++) {
			kfree(lastest_obv_cl[i]);
			kfree(lastest_is_cl_isolated[i]);
		}
	}
	spin_unlock_irqrestore(&loading_slock, lock_flag);
}

void fbt_delete_cpu_loading_info(void)
{
	int i = 0;
	unsigned long lock_flag;

	spin_lock_irqsave(&loading_slock, lock_flag);

	for (i = 0; i < LOADING_CNT; i++) {
		kfree(lastest_obv_cl[i]);
		kfree(lastest_is_cl_isolated[i]);
	}
	spin_unlock_irqrestore(&loading_slock, lock_flag);
}

void __exit fbt_cpu_exit(void)
{
	int i = 0;

	minitop_exit();

	fpsgo_sysfs_remove_file(fbt_kobj,
			&kobj_attr_light_loading_policy);
	fpsgo_sysfs_remove_file(fbt_kobj,
			&kobj_attr_fbt_info);
	fpsgo_sysfs_remove_file(fbt_kobj,
			&kobj_attr_switch_idleprefer);
	fpsgo_sysfs_remove_file(fbt_kobj,
			&kobj_attr_table_freq);
	fpsgo_sysfs_remove_file(fbt_kobj,
			&kobj_attr_table_cap);
	fpsgo_sysfs_remove_file(fbt_kobj,
			&kobj_attr_enable_switch_down_throttle);
	fpsgo_sysfs_remove_file(fbt_kobj,
			&kobj_attr_ultra_rescue);
	fpsgo_sysfs_remove_file(fbt_kobj,
			&kobj_attr_llf_task_policy);
	fpsgo_sysfs_remove_file(fbt_kobj,
			&kobj_attr_boost_ta);
	fpsgo_sysfs_remove_file(fbt_kobj,
			&kobj_attr_limit_uclamp);
	fpsgo_sysfs_remove_file(fbt_kobj,
			&kobj_attr_limit_ruclamp);
	fpsgo_sysfs_remove_file(fbt_kobj,
			&kobj_attr_limit_uclamp_m);
	fpsgo_sysfs_remove_file(fbt_kobj,
			&kobj_attr_limit_ruclamp_m);
	fpsgo_sysfs_remove_file(fbt_kobj,
			&kobj_attr_limit_cfreq);
	fpsgo_sysfs_remove_file(fbt_kobj,
			&kobj_attr_limit_rfreq);
	fpsgo_sysfs_remove_file(fbt_kobj,
			&kobj_attr_limit_cfreq_m);
	fpsgo_sysfs_remove_file(fbt_kobj,
			&kobj_attr_limit_rfreq_m);
	fpsgo_sysfs_remove_file(fbt_kobj,
			&kobj_attr_enable_ceiling);
	fpsgo_sysfs_remove_file(fbt_kobj,
			&kobj_attr_enable_uclamp_boost);
	fpsgo_sysfs_remove_file(fbt_kobj,
				&kobj_attr_switch_filter_frame);
	fpsgo_sysfs_remove_file(fbt_kobj,
			&kobj_attr_filter_f_window_size);
	fpsgo_sysfs_remove_file(fbt_kobj,
			&kobj_attr_filter_f_kmin);
	fpsgo_sysfs_remove_file(fbt_kobj,
			&kobj_attr_enable_separate_aa);
	fpsgo_sysfs_remove_file(fbt_kobj,
			&kobj_attr_separate_pct_b);
	fpsgo_sysfs_remove_file(fbt_kobj,
			&kobj_attr_separate_pct_m);
	fpsgo_sysfs_remove_file(fbt_kobj,
			&kobj_attr_separate_release_sec);
	fpsgo_sysfs_remove_file(fbt_kobj,
			&kobj_attr_blc_boost);

	fpsgo_sysfs_remove_dir(&fbt_kobj);
	fbt_delete_cpu_loading_info();

	#if FPSGO_MW
	fpsgo_sysfs_remove_file(fbt_kobj,
			&kobj_attr_fbt_attr_by_pid);
	#endif  // FPSGO_MW

	kfree(base_opp);
	kfree(clus_obv);
	kfree(clus_status);

	for (i = 0; i < cluster_num; i++) {
		kfree(cpu_dvfs[i].power);
		kfree(cpu_dvfs[i].capacity_ratio);
	}
	kfree(cpu_dvfs);

	kfree(clus_max_cap);
	kfree(limit_clus_ceil);

	fbt_cpu_ctrl_exit();
	exit_fbt_platform();
	exit_xgf();
	fbt_cpu_cam_exit();
}

int __init fbt_cpu_init(void)
{
	int i = 0;

	init_fbt_platform();

	cluster_num = fpsgo_arch_nr_clusters();
	nr_freq_cpu = fpsgo_arch_nr_max_opp_cpu();
	bhr = 0;
	bhr_opp = 0;
	bhr_opp_l = fbt_get_l_min_bhropp();
	isolation_limit_cap = 1;
	rescue_opp_c = (nr_freq_cpu - 1);
	rescue_opp_f = 5;
	rescue_percent = DEF_RESCUE_PERCENT;
	min_rescue_percent = 10;
	short_rescue_ns = DEF_RESCUE_NS_TH;
	short_min_rescue_p = 0;
	run_time_percent = 50;
	deqtime_bound = TIME_3MS;
	variance_control_enable = 0;
	variance = 40;
	floor_bound = 3;
	kmin = 10;
	floor_opp = 2;
	loading_th = 0;
	sampling_period_MS = 256;
	rescue_enhance_f = 25;
	sbe_enhance_f = 50;
	rescue_second_enhance_f = 100;
	loading_adj_cnt = fbt_get_default_adj_count();
	loading_debnc_cnt = 30;
	loading_time_diff = fbt_get_default_adj_tdiff();
	fps_level_range = 10;
	cm_big_cap = 95;
	cm_tdiff = TIME_1MS;
	rescue_second_time = 1;
	rescue_second_group = 1;
	rescue_second_copp = nr_freq_cpu - 1;

	_gdfrc_fps_limit = TARGET_DEFAULT_FPS;
	vsync_period = GED_VSYNC_MISS_QUANTUM_NS;

	fbt_idleprefer_enable = 0;
	suppress_ceiling = 1;
	uclamp_boost_enable = 1;
	down_throttle_ns = -1;
	fbt_down_throttle_enable = 1;
	boost_ta = fbt_get_default_boost_ta();
	adjust_loading = fbt_get_default_adj_loading();
	adjust_loading_hwui_hint = DEFAULT_ADJUST_LOADING_HWUI_HINT;
	enable_ceiling = 0;

	/* t2wnt = target_time * (1+x) + quota * y_p, if quota > 0 */
	/* t2wnt = target_time * (1+x) + quota * y_n, if quota < 0 */
	qr_enable = fbt_get_default_qr_enable();
	qr_t2wnt_x = DEFAULT_QR_T2WNT_X;
	qr_t2wnt_y_p = DEFAULT_QR_T2WNT_Y_P;
	qr_t2wnt_y_n = DEFAULT_QR_T2WNT_Y_N;
	qr_hwui_hint = DEFAULT_QR_HWUI_HINT;
	qr_filter_outlier = 0;
	qr_mod_frame = 0;
	qr_debug = 0;

	gcc_enable = fbt_get_default_gcc_enable();
	gcc_hwui_hint = DEFAULT_GCC_HWUI_HINT;
	gcc_reserved_up_quota_pct = DEFAULT_GCC_RESERVED_UP_QUOTA_PCT;
	gcc_reserved_down_quota_pct = DEFAULT_GCC_RESERVED_DOWN_QUOTA_PCT;
	gcc_window_size = DEFAULT_GCC_WINDOW_SIZE;
	gcc_std_filter = DEFAULT_GCC_STD_FILTER;
	gcc_up_step = DEFAULT_GCC_UP_STEP;
	gcc_down_step = DEFAULT_GCC_DOWN_STEP;
	gcc_fps_margin = DEFAULT_GCC_FPS_MARGIN;
	gcc_up_sec_pct = DEFAULT_GCC_UP_SEC_PCT;
	gcc_down_sec_pct = DEFAULT_GCC_DOWN_SEC_PCT;
	gcc_gpu_bound_time = DEFAULT_GCC_GPU_BOUND_TIME;
	gcc_cpu_unknown_sleep = DEFAULT_GCC_CPU_UNKNOWN_SLEEP;
	gcc_check_quota_trend = DEFAULT_GCC_CHECK_QUOTA_TREND;
	gcc_enq_bound_thrs = DEFAULT_GCC_ENQ_BOUND_THRS;
	gcc_enq_bound_quota = DEFAULT_GCC_ENQ_BOUND_QUOTA;
	gcc_deq_bound_thrs = DEFAULT_GCC_DEQ_BOUND_THRS;
	gcc_deq_bound_quota = DEFAULT_GCC_DEQ_BOUND_QUOTA;

	filter_frame_enable = 0;
	filter_frame_window_size = 6;
	filter_frame_kmin = 3;
	separate_aa = 0;
	separate_pct_b = 0;
	separate_pct_m = 0;
	separate_release_sec = 0;
	blc_boost = DEFAULT_BLC_BOOST;

	sbe_rescue_enable = fbt_get_default_sbe_rescue_enable();

	cam_dep_thread_tree = RB_ROOT;

	if (cluster_num <= 0)
		FPSGO_LOGE("cpufreq policy not found");

	base_opp =
		kcalloc(cluster_num, sizeof(unsigned int), GFP_KERNEL);

	clus_obv =
		kcalloc(cluster_num, sizeof(unsigned int), GFP_KERNEL);

	clus_status =
		kcalloc(cluster_num, sizeof(unsigned int), GFP_KERNEL);

	cpu_dvfs =
		kcalloc(cluster_num, sizeof(struct fbt_cpu_dvfs_info),
				GFP_KERNEL);
	for (i = 0; i < cluster_num; i++) {
		cpu_dvfs[i].power =
			kcalloc(nr_freq_cpu, sizeof(unsigned int), GFP_KERNEL);
		cpu_dvfs[i].capacity_ratio =
			kcalloc(nr_freq_cpu, sizeof(unsigned int), GFP_KERNEL);
	}

	clus_max_cap =
		kcalloc(cluster_num, sizeof(int), GFP_KERNEL);

	limit_clus_ceil =
		kcalloc(cluster_num, sizeof(struct fbt_syslimit), GFP_KERNEL);

	xgff_frame_min_cap_fp = fbt_xgff_set_min_cap;

	fbt_init_cpu_loading_info();

	fbt_init_sjerk();

	fbt_update_pwr_tbl();

	if (!fpsgo_sysfs_create_dir(NULL, "fbt", &fbt_kobj)) {
		fpsgo_sysfs_create_file(fbt_kobj,
				&kobj_attr_light_loading_policy);
		fpsgo_sysfs_create_file(fbt_kobj,
				&kobj_attr_fbt_info);
		fpsgo_sysfs_create_file(fbt_kobj,
				&kobj_attr_switch_idleprefer);
		fpsgo_sysfs_create_file(fbt_kobj,
				&kobj_attr_table_freq);
		fpsgo_sysfs_create_file(fbt_kobj,
				&kobj_attr_table_cap);
		fpsgo_sysfs_create_file(fbt_kobj,
				&kobj_attr_enable_switch_down_throttle);
		fpsgo_sysfs_create_file(fbt_kobj,
				&kobj_attr_ultra_rescue);
		fpsgo_sysfs_create_file(fbt_kobj,
				&kobj_attr_llf_task_policy);
		fpsgo_sysfs_create_file(fbt_kobj,
				&kobj_attr_boost_ta);
		fpsgo_sysfs_create_file(fbt_kobj,
				&kobj_attr_limit_uclamp);
		fpsgo_sysfs_create_file(fbt_kobj,
				&kobj_attr_limit_ruclamp);
		fpsgo_sysfs_create_file(fbt_kobj,
				&kobj_attr_limit_uclamp_m);
		fpsgo_sysfs_create_file(fbt_kobj,
				&kobj_attr_limit_ruclamp_m);
		fpsgo_sysfs_create_file(fbt_kobj,
				&kobj_attr_limit_cfreq);
		fpsgo_sysfs_create_file(fbt_kobj,
				&kobj_attr_limit_rfreq);
		fpsgo_sysfs_create_file(fbt_kobj,
				&kobj_attr_limit_cfreq_m);
		fpsgo_sysfs_create_file(fbt_kobj,
				&kobj_attr_limit_rfreq_m);
		fpsgo_sysfs_create_file(fbt_kobj,
				&kobj_attr_enable_ceiling);
		fpsgo_sysfs_create_file(fbt_kobj,
				&kobj_attr_enable_uclamp_boost);
		fpsgo_sysfs_create_file(fbt_kobj,
				&kobj_attr_switch_filter_frame);
		fpsgo_sysfs_create_file(fbt_kobj,
				&kobj_attr_filter_f_window_size);
		fpsgo_sysfs_create_file(fbt_kobj,
				&kobj_attr_filter_f_kmin);
		fpsgo_sysfs_create_file(fbt_kobj,
				&kobj_attr_enable_separate_aa);
		fpsgo_sysfs_create_file(fbt_kobj,
				&kobj_attr_separate_pct_b);
		fpsgo_sysfs_create_file(fbt_kobj,
				&kobj_attr_separate_pct_m);
		fpsgo_sysfs_create_file(fbt_kobj,
				&kobj_attr_separate_release_sec);
		fpsgo_sysfs_create_file(fbt_kobj,
				&kobj_attr_blc_boost);
#if FPSGO_MW
		fpsgo_sysfs_create_file(fbt_kobj,
				&kobj_attr_fbt_attr_by_pid);
#endif  // FPSGO_MW

	}

	INIT_LIST_HEAD(&blc_list);

	wq_jerk = alloc_workqueue("fbt_cpu", WQ_HIGHPRI | WQ_MEM_RECLAIM | WQ_UNBOUND, 0);

	/* sub-module initialization */
	init_xgf();
	fbt_cpu_cam_init();
	minitop_init();

	fbt_cpu_ctrl_init();

	return 0;
}
