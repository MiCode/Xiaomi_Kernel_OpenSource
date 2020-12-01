/*
 * Copyright (C) 2017 MediaTek Inc.
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
#include <mt-plat/aee.h>
#include <mt-plat/mtk_sched.h>
#include <linux/sort.h>
#include <linux/string.h>
#include <asm/div64.h>
#include <linux/topology.h>
#include <linux/slab.h>
#include <linux/hrtimer.h>
#include <linux/list.h>
#include <linux/kernel.h>
#include <linux/sched/rt.h>
#include <linux/sched/deadline.h>
#include <linux/bsearch.h>
#include <linux/sched/task.h>
#include <linux/sched/topology.h>
#include <sched/sched.h>
#include <linux/list_sort.h>

#include <mt-plat/mtk_perfobserver.h>
#include <mt-plat/eas_ctrl.h>

#include <fpsgo_common.h>

#include <trace/events/fpsgo.h>

#include "eas_ctrl.h"
#include "cpu_ctrl.h"

#include "fpsgo_base.h"
#include "fpsgo_sysfs.h"
#include "fbt_usedext.h"
#include "fbt_cpu.h"
#include "fbt_cpu_platform.h"
#include "../fstb/fstb.h"
#include "xgf.h"
#include "mini_top.h"
#include "fps_composer.h"
#include "eara_job.h"
#include "mtk_upower.h"

#define GED_VSYNC_MISS_QUANTUM_NS 16666666
#define TIME_3MS  3000000
#define TIME_2MS  2000000
#define TIME_1MS  1000000
#define TARGET_UNLIMITED_FPS 240
#define TARGET_DEFAULT_FPS 60
#define FBTCPU_SEC_DIVIDER 1000000000
#define NSEC_PER_HUSEC 100000
#define TIME_MS_TO_NS  1000000ULL
#define MAX_DEP_NUM 30
#define LOADING_WEIGHT 50
#define DEF_RESCUE_PERCENT 33
#define DEF_RESCUE_NS_TH 0
#define INVALID_NUM -1

#define MAX(a, b) (((a) > (b)) ? (a) : (b))
#define MIN(a, b) (((a) < (b)) ? (a) : (b))

#define SEQ_printf(m, x...)\
do {\
	if (m)\
		seq_printf(m, x);\
	else\
		pr_debug(x);\
} while (0)

#ifdef NR_FREQ_CPU
struct fbt_cpu_dvfs_info {
	unsigned int power[NR_FREQ_CPU];
	unsigned int capacity_ratio[NR_FREQ_CPU];
};
#endif

struct fbt_pid_list {
	int pid;
	struct list_head entry;
};

enum FPSGO_JERK {
	FPSGO_JERK_ONLY_CEILING = 0,
	FPSGO_JERK_NEED = 1,
	FPSGO_JERK_POSTPONE = 2,
	FPSGO_JERK_DISAPPEAR = 3,
};

enum FPSGO_TASK_POLICY {
	FPSGO_TPOLICY_NONE = 0,
	FPSGO_TPOLICY_AFFINITY,
	FPSGO_TPOLICY_PREFER,
	FPSGO_TPOLICY_MAX,
};

enum FPSGO_LIMIT_POLICY {
	FPSGO_LIMIT_NONE = 0,
	FPSGO_LIMIT_CAPACITY,
	FPSGO_LIMIT_MAX,
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
	FPSGO_JERK_UBOOST,
};

static struct kobject *fbt_kobj;

static int bhr;
static int bhr_opp;
static int rescue_opp_f;
static int rescue_enhance_f;
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
static int rescue_percent_90;
static int rescue_percent_120;
static int fps_level_range;
static int check_running;
static int uboost_enhance_f;
static int boost_affinity;
static int boost_affinity_90;
static int boost_affinity_120;
static int cm_big_cap;
static int cm_tdiff;

module_param(bhr, int, 0644);
module_param(bhr_opp, int, 0644);
module_param(rescue_opp_f, int, 0644);
module_param(rescue_enhance_f, int, 0644);
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
module_param(rescue_percent_90, int, 0644);
module_param(rescue_percent_120, int, 0644);
module_param(fps_level_range, int, 0644);
module_param(check_running, int, 0644);
module_param(uboost_enhance_f, int, 0644);
module_param(boost_affinity, int, 0644);
module_param(boost_affinity_90, int, 0644);
module_param(boost_affinity_120, int, 0644);
module_param(cm_big_cap, int, 0644);
module_param(cm_tdiff, int, 0644);

static DEFINE_SPINLOCK(freq_slock);
static DEFINE_MUTEX(fbt_mlock);
static DEFINE_SPINLOCK(loading_slock);
static DEFINE_MUTEX(blc_mlock);

static struct list_head loading_list;
static struct list_head blc_list;

static int fbt_enable;
static int fbt_idleprefer_enable;
static int bypass_flag;
static int set_idleprefer;
static int suppress_ceiling;
static int boost_ta;
static int down_throttle_ns;
static int fbt_down_throttle_enable;
static int sync_flag;
static int fbt_sync_flag_enable;
static int set_cap_margin;
static int fbt_cap_margin_enable;
static int ultra_rescue;
static int loading_policy;
static int llf_task_policy;

static int cluster_num;
static unsigned int cpu_max_freq;
static struct fbt_cpu_dvfs_info *cpu_dvfs;
static int max_cap_cluster, min_cap_cluster;
static unsigned int def_capacity_margin;

static int limit_policy;
static int limit_cluster;
static int limit_copp; /* ceiling limit */
static int limit_ropp; /* rescue ceiling limit */
static int limit_cfreq; /* tuning ceiling */
static int limit_rfreq; /* tuning rescue ceiling */

static int *clus_max_cap;

static unsigned int *base_opp;
static unsigned int max_blc;
static int max_blc_pid;
static unsigned long long  max_blc_buffer_id;
static int max_blc_stage;

static unsigned int *clus_obv;
static unsigned int last_obv;

static unsigned long long vsync_time;

static int vsync_period;
static int _gdfrc_fps_limit;

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

int fbt_cpu_set_bhr_opp(int new_opp)
{
	if (new_opp < 0)
		return -EINVAL;

	mutex_lock(&fbt_mlock);
	bhr_opp = new_opp;
	mutex_unlock(&fbt_mlock);

	return 0;
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

static struct fbt_thread_loading *fbt_list_loading_add(int pid,
	unsigned long long buffer_id)
{
	struct fbt_thread_loading *obj;
	unsigned long flags;
	atomic_t *loading_cl;

	obj = kzalloc(sizeof(struct fbt_thread_loading), GFP_KERNEL);
	if (!obj) {
		FPSGO_LOGE("ERROR OOM\n");
		return NULL;
	}

	loading_cl = kcalloc(cluster_num, sizeof(atomic_t), GFP_KERNEL);
	if (!loading_cl) {
		kfree(obj);
		FPSGO_LOGE("ERROR OOM\n");
		return NULL;
	}

	INIT_LIST_HEAD(&obj->entry);
	obj->pid = pid;
	obj->buffer_id = buffer_id;
	obj->loading_cl = loading_cl;

	spin_lock_irqsave(&loading_slock, flags);
	list_add_tail(&obj->entry, &loading_list);
	spin_unlock_irqrestore(&loading_slock, flags);

	return obj;
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
	obj->pid = pid;
	obj->buffer_id = buffer_id;

	mutex_lock(&blc_mlock);
	list_add(&obj->entry, &blc_list);
	mutex_unlock(&blc_mlock);

	return obj;
}

static void fbt_find_max_blc(unsigned int *temp_blc, int *temp_blc_pid,
	unsigned long long *temp_blc_buffer_id)
{
	struct fbt_thread_blc *pos, *next;

	*temp_blc = 0;
	*temp_blc_pid = 0;
	*temp_blc_buffer_id = 0;

	mutex_lock(&blc_mlock);
	list_for_each_entry_safe(pos, next, &blc_list, entry) {
		if (pos->blc > *temp_blc) {
			*temp_blc = pos->blc;
			*temp_blc_pid = pos->pid;
			*temp_blc_buffer_id = pos->buffer_id;
		}
	}
	mutex_unlock(&blc_mlock);
}

static void fbt_find_ex_max_blc(int pid, unsigned long long buffer_id,
	unsigned int *temp_blc, int *temp_blc_pid,
	unsigned long long *temp_blc_buffer_id)
{
	struct fbt_thread_blc *pos, *next;

	*temp_blc = 0;
	*temp_blc_pid = 0;
	*temp_blc_buffer_id = 0;

	mutex_lock(&blc_mlock);
	list_for_each_entry_safe(pos, next, &blc_list, entry) {
		if (pos->blc > *temp_blc &&
			(pid != pos->pid || buffer_id != pos->buffer_id)) {
			*temp_blc = pos->blc;
			*temp_blc_pid = pos->pid;
			*temp_blc_buffer_id = pos->buffer_id;
		}
	}
	mutex_unlock(&blc_mlock);
}

static void fbt_set_cap_margin_locked(int set)
{
	if (!fbt_cap_margin_enable)
		return;

	if (set_cap_margin == set)
		return;

	fpsgo_main_trace("fpsgo set margin %d", set?1024:def_capacity_margin);
	fpsgo_systrace_c_fbt_gm(-100, 0, set?1024:def_capacity_margin,
					"cap_margin");

	if (set)
		set_capacity_margin(1024);
	else
		set_capacity_margin(def_capacity_margin);
	set_cap_margin = set;
}

static void fbt_set_idleprefer_locked(int enable)
{
	if (!fbt_idleprefer_enable)
		return;

	if (set_idleprefer == enable)
		return;

	xgf_trace("fpsgo %s idelprefer", enable?"enable":"disbale");
#ifdef CONFIG_SCHED_TUNE
	/* use eas_ctrl to control prefer idle */
	update_prefer_idle_value(EAS_PREFER_IDLE_KIR_FPSGO, CGROUP_TA, enable);
#endif
	set_idleprefer = enable;
}

static void fbt_set_down_throttle_locked(int nsec)
{
	if (!fbt_down_throttle_enable)
		return;

	if (down_throttle_ns == nsec)
		return;

	xgf_trace("fpsgo set down_throttle %d", nsec);
	update_schedplus_down_throttle_ns(EAS_THRES_KIR_FPSGO, nsec);
	update_schedplus_up_throttle_ns(EAS_THRES_KIR_FPSGO, nsec);
	down_throttle_ns = nsec;
}

static void fbt_set_sync_flag_locked(int input)
{
	if (!fbt_sync_flag_enable)
		return;

	if (sync_flag == input)
		return;

	xgf_trace("fpsgo set sync_flag %d", input);
	update_schedplus_sync_flag(EAS_SYNC_FLAG_KIR_FPSGO, input);
	sync_flag = input;
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

static void fbt_set_ceiling(struct ppm_limit_data *pld,
			int pid, unsigned long long buffer_id)
{
	int i;

	if (!pld)
		return;

	update_userlimit_cpu_freq(CPU_KIR_FPSGO, cluster_num, pld);

	if (!pid)
		return;

	for (i = 0 ; i < cluster_num; i++)
		fpsgo_systrace_c_fbt(pid, buffer_id, pld[i].max,
			"cluster%d ceiling_freq", i);
}

static void fbt_limit_ceiling_locked(struct ppm_limit_data *pld, int is_rescue)
{
	int opp;

	if (!pld)
		return;

	if (limit_cluster >= cluster_num || limit_cluster < 0)
		return;

	opp = is_rescue ? limit_ropp : limit_copp;

	if (opp <= 0 || opp >= NR_FREQ_CPU)
		return;

	pld[limit_cluster].max = MIN(cpu_dvfs[limit_cluster].power[opp],
					pld[limit_cluster].max);
}

static void fbt_set_hard_limit_locked(int input, struct ppm_limit_data *pld)
{
	int set_margin, set_ceiling, is_rescue;

	if (limit_policy == FPSGO_LIMIT_NONE) {
		if (input == FPSGO_HARD_NONE || input == FPSGO_HARD_CEILING)
			fbt_set_cap_margin_locked(0);

		return;
	}

	fpsgo_main_trace("limit: %d", input);

	set_margin = input & 1;
	set_ceiling = (input & 2) >> 1;
	is_rescue = !set_margin;

	if (cluster_num == 1 || bhr_opp == (NR_FREQ_CPU - 1)
		|| (pld && pld[max_cap_cluster].max == -1)) {
		set_ceiling = 0;
		set_margin = 0;
	}

	fbt_set_cap_margin_locked(set_margin);

	if (set_ceiling)
		fbt_limit_ceiling_locked(pld, is_rescue);
}

static int fbt_limit_capacity(int blc_wt, int is_rescue)
{
	int cap = 0;
	int opp;

	if (limit_policy == FPSGO_LIMIT_NONE)
		return blc_wt;

	if (limit_cluster >= cluster_num || limit_cluster < 0)
		return blc_wt;

	opp = (is_rescue) ? limit_ropp : limit_copp;

	if (opp >= 0 && opp < NR_FREQ_CPU)
		cap = cpu_dvfs[limit_cluster].capacity_ratio[opp];

	if (cap <= 0)
		return blc_wt;

	return MIN(blc_wt, cap);
}

static void fbt_filter_ppm_log_locked(int filter)
{
	ppm_game_mode_change_cb(filter);
}

static void fbt_free_bhr(void)
{
	struct ppm_limit_data *pld;
	int i;

	pld = kcalloc(cluster_num, sizeof(struct ppm_limit_data), GFP_KERNEL);
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

#define FPS_LEVEL 3
static const int fbt_fps_level[FPS_LEVEL] = {60, 90, 120};
static int fbt_get_fps_level(int target_fps)
{
	int tol_fps;
	int i;

	if (_gdfrc_fps_limit && _gdfrc_fps_limit <= fbt_fps_level[0])
		return fbt_fps_level[0];

	for (i = 0; i < FPS_LEVEL - 1; i++) {
		tol_fps = fbt_fps_level[i] * (100+fps_level_range) / 100;
		if (target_fps < tol_fps)
			return fbt_fps_level[i];
	}

	return fbt_fps_level[FPS_LEVEL - 1];
}

static void fbt_set_task_policy(struct fpsgo_loading *fl,
			int policy, unsigned int prefer_type)
{
	if (!fl || !fl->pid)
		return;

	/* policy changes, reset */
	if (fl->policy != policy && fl->prefer_type != FPSGO_PREFER_NONE)
		fbt_set_task_policy(fl, fl->policy, FPSGO_PREFER_NONE);

	switch (policy) {
	case FPSGO_TPOLICY_PREFER:
		fbt_set_cpu_prefer(fl->pid, prefer_type);
		break;
	case FPSGO_TPOLICY_AFFINITY:
		fbt_set_affinity(fl->pid, prefer_type);
		break;
	case FPSGO_TPOLICY_NONE:
	default:
		break;
	}

	fl->prefer_type = prefer_type;
	fl->policy = policy;
	fpsgo_systrace_c_fbt_gm(fl->pid, 0, prefer_type, "prefer_type");
	fpsgo_systrace_c_fbt_gm(fl->pid, 0, policy, "task_policy");
}

static void fbt_reset_task_setting(struct fpsgo_loading *fl, int reset_boost)
{
	if (!fl || !fl->pid)
		return;

	if (reset_boost)
		fbt_set_per_task_min_cap(fl->pid, 0);

	fbt_set_task_policy(fl, FPSGO_TPOLICY_NONE, FPSGO_PREFER_NONE);
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

static int __cmp(const void *a, const void *b)
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

static int fbt_get_dep_list(struct render_info *thr)
{
	int pid;
	int count = 0;
	int ret_size;
	struct fpsgo_loading dep_new[MAX_DEP_NUM];
	int i, j;
	struct fpsgo_loading *fl_new, *fl_old;
	int incr_i, incr_j;

	memset(dep_new, 0,
		MAX_DEP_NUM * sizeof(struct fpsgo_loading));

	if (!thr)
		return 1;

	pid = thr->pid;
	if (!pid)
		return 2;

	count = fpsgo_fbt2xgf_get_dep_list_num(pid, thr->buffer_id);
	if (count <= 0)
		return 3;
	count = clamp(count, 1, MAX_DEP_NUM);

	ret_size = fpsgo_fbt2xgf_get_dep_list(pid, count,
		dep_new, thr->buffer_id);

	if (ret_size == 0 || ret_size != count)
		return 4;

	fbt_dep_list_filter(dep_new, count);
	sort(dep_new, count, sizeof(struct fpsgo_loading), __cmp, NULL);

	for (i = 0, j = 0, fl_new = &dep_new[0], fl_old = &(thr->dep_arr[0]);
	     count > 0 && thr->dep_valid_size &&
	     (i < count || j < thr->dep_valid_size);
	     i = incr_i ? __incr(i, count) : i,
	     j = incr_j ? __incr(j, thr->dep_valid_size) : j,
	     fl_new = &dep_new[clamp(i, i, count - 1)],
	     fl_old = &(thr->dep_arr[clamp(j, j, thr->dep_valid_size - 1)])) {

		incr_i = incr_j = 0;

		if (fl_new->pid == 0) {
			if (i >= count && j < thr->dep_valid_size)
				fbt_reset_task_setting(fl_old, 1);
			__incr_alt(i, count, &incr_i, &incr_j);
			continue;
		}

		if (fl_old->pid == 0) {
			__incr_alt(j, thr->dep_valid_size, &incr_j, &incr_i);
			continue;
		}

		if (fl_new->pid == fl_old->pid) {
			fl_new->loading = fl_old->loading;
			fl_new->prefer_type = fl_old->prefer_type;
			fl_new->policy = fl_old->policy;
			incr_i = incr_j = 1;
		} else if (fl_new->pid > fl_old->pid) {
			if (j < thr->dep_valid_size)
				fbt_reset_task_setting(fl_old, 1);
			__incr_alt(j, thr->dep_valid_size, &incr_j, &incr_i);
		} else { /* new pid < old pid */
			if (i >= count && j < thr->dep_valid_size)
				fbt_reset_task_setting(fl_old, 1);
			__incr_alt(i, count, &incr_i, &incr_j);
		}
	}

	if (!thr->dep_arr) {
		thr->dep_arr = (struct fpsgo_loading *)
			fpsgo_alloc_atomic(MAX_DEP_NUM *
				sizeof(struct fpsgo_loading));
		if (thr->dep_arr == NULL) {
			thr->dep_valid_size = 0;
			return 5;
		}
	}

	thr->dep_valid_size = count;
	memset(thr->dep_arr, 0,
		MAX_DEP_NUM * sizeof(struct fpsgo_loading));
	memcpy(thr->dep_arr, dep_new,
		thr->dep_valid_size * sizeof(struct fpsgo_loading));

	return 0;
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

	if (!thr || !thr->dep_arr)
		return;

	for (i = 0; i < thr->dep_valid_size; i++)
		fbt_reset_task_setting(&thr->dep_arr[i], 1);
}

static int fbt_is_light_loading(int loading)
{
	if (!loading_th || loading > loading_th
		|| loading == -1 || loading == 0)
		return 0;

	return 1;
}

static int fbt_get_heavy_pid(int size, struct fpsgo_loading *dep_arr)
{
	int i;
	int max = 0;
	int ret_pid = 0;

	if (!dep_arr || !size)
		return 0;

	for (i = 0; i < size; i++) {
		struct fpsgo_loading *fl = &dep_arr[i];

		if (!fl->pid)
			continue;

		if (fl->loading <= 0) {
			fl->loading = fpsgo_fbt2minitop_query_single(fl->pid);
			if (fl->loading <= 0)
				continue;
		}

		if (fl->loading > max) {
			ret_pid = fl->pid;
			max = fl->loading;
		}
	}

	return ret_pid;
}

#define MAX_PID_DIGIT 7
#define MAIN_LOG_SIZE (256)
static void fbt_set_min_cap_locked(struct render_info *thr, int min_cap,
					int jerk)
{
/*
 * boost_ta should be checked during the flow, not here.
 */
	int size = 0, i;
	char *dep_str = NULL;
	int ret;
	int heavy_pid = 0;
	int do_affinity = 0;
	unsigned long long target_time;

	if (!thr)
		return;

	if (!min_cap) {
		fbt_clear_min_cap(thr);
		return;
	}

	if (!jerk) {
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

	target_time = thr->boost_info.target_time;

	if (target_time &&
		(boost_affinity || boost_affinity_90 || boost_affinity_120)) {
		int tfps_level, target_fps;

		target_fps = div64_u64(FBTCPU_SEC_DIVIDER, target_time);
		tfps_level = fbt_get_fps_level(target_fps);

		if (tfps_level == fbt_fps_level[0])
			do_affinity = boost_affinity;
		else if (tfps_level == fbt_fps_level[1])
			do_affinity = boost_affinity_90;
		else
			do_affinity = boost_affinity_120;
	}

	if (loading_th
		|| (do_affinity && limit_policy == FPSGO_LIMIT_CAPACITY))
		fbt_query_dep_list_loading(thr);

	if (do_affinity && limit_policy == FPSGO_LIMIT_CAPACITY)
		heavy_pid = fbt_get_heavy_pid(thr->dep_valid_size, thr->dep_arr);

	dep_str = kcalloc(size + 1, MAX_PID_DIGIT * sizeof(char),
				GFP_KERNEL);
	if (!dep_str)
		return;

	for (i = 0; i < size; i++) {
		char temp[MAX_PID_DIGIT] = {"\0"};
		struct fpsgo_loading *fl = &thr->dep_arr[i];

		if (!fl->pid)
			continue;

		if (loading_th
			|| (do_affinity && limit_policy == FPSGO_LIMIT_CAPACITY)) {
			fpsgo_systrace_c_fbt_gm(fl->pid, thr->buffer_id,
				fl->loading, "dep-loading");

			if (fl->loading == 0 || fl->loading == -1) {
				fl->loading = fpsgo_fbt2minitop_query_single(
					fl->pid);
				fpsgo_systrace_c_fbt_gm(fl->pid, thr->buffer_id,
					fl->loading, "dep-loading");
			}
		}

		if (fbt_is_light_loading(fl->loading)) {
			fbt_set_per_task_min_cap(fl->pid,
				(!loading_policy) ? 0
				: min_cap * loading_policy / 100);
			fbt_set_task_policy(fl, llf_task_policy,
					FPSGO_PREFER_LITTLE);
		} else if (heavy_pid && heavy_pid == fl->pid) {
			fpsgo_systrace_c_fbt(thr->pid, thr->buffer_id,
					heavy_pid, "heavy_pid");
			fbt_set_per_task_min_cap(fl->pid, min_cap);
			if (do_affinity)
				fbt_set_task_policy(fl, FPSGO_TPOLICY_AFFINITY,
						FPSGO_PREFER_BIG);
		} else {
			fbt_set_per_task_min_cap(fl->pid, min_cap);
			if (do_affinity && heavy_pid && heavy_pid != fl->pid)
				fbt_set_task_policy(fl, FPSGO_TPOLICY_AFFINITY,
						FPSGO_PREFER_L_M);
			else
				fbt_reset_task_setting(fl, 0);
		}

		if (strlen(dep_str) == 0)
			snprintf(temp, sizeof(temp), "%d", fl->pid);
		else
			snprintf(temp, sizeof(temp), ",%d", fl->pid);

		if (strlen(dep_str) + strlen(temp) < MAIN_LOG_SIZE)
			strncat(dep_str, temp, strlen(temp));
	}

	fpsgo_main_trace("[%d] dep-list %s", thr->pid, dep_str);
	kfree(dep_str);
}

static int fbt_get_target_cluster(unsigned int blc_wt)
{
	int cluster = min_cap_cluster;
	int i = max_cap_cluster;
	int order = (max_cap_cluster > min_cap_cluster)?1:0;

	while (i != min_cap_cluster) {
		if (blc_wt >= cpu_dvfs[i].capacity_ratio[NR_FREQ_CPU - 1]) {
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

static int fbt_get_opp_by_normalized_cap(unsigned int cap, int cluster)
{
	int tgt_opp;

	if (cluster >= cluster_num)
		cluster = 0;

	for (tgt_opp = (NR_FREQ_CPU - 1); tgt_opp > 0; tgt_opp--) {
		if (cpu_dvfs[cluster].capacity_ratio[tgt_opp] >= cap)
			break;
	}

	return tgt_opp;
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

static unsigned int fbt_get_new_base_blc(struct ppm_limit_data *pld,
				int floor, int enhance, int headroom)
{
	int cluster;
	unsigned int blc_wt = 0U;
	int base;

	if (!pld) {
		FPSGO_LOGE("ERROR %d\n", __LINE__);
		return 0U;
	}

	if (boost_ta)
		base = max_blc;
	else
		base = floor;

	blc_wt = fbt_enhance_floor(base, rescue_opp_f, enhance);

	for (cluster = 0 ; cluster < cluster_num; cluster++) {
		pld[cluster].min = -1;
		if (suppress_ceiling) {
			int opp =
				fbt_get_opp_by_normalized_cap(blc_wt, cluster);

			opp = MIN(opp, base_opp[cluster]);
			opp = clamp(opp, 0, NR_FREQ_CPU - 1);

			pld[cluster].max = cpu_dvfs[cluster].power[max(
				(int)(opp - headroom), 0)];
		} else
			pld[cluster].max = -1;
	}

	return blc_wt;
}

static int cmpint(const void *a, const void *b)
{
	return *(int *)a - *(int *)b;
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
	unsigned int tsk_state = READ_ONCE(tsk->state);

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
		raw_spin_lock_irqsave(&cpu_rq(cpu)->lock, flags);
		list_for_each_entry(p, &cpu_rq(cpu)->cfs_tasks, se.group_node) {
			rq_pid[rq_idx] = p->pid;
			rq_idx++;

			if (rq_idx >= MAX_RQ_COUNT) {
				fpsgo_systrace_c_fbt(tgid, 0, rq_idx, "rq_shrink");
				raw_spin_unlock_irqrestore(&cpu_rq(cpu)->lock, flags);
				goto NEXT;
			}
		}
		raw_spin_unlock_irqrestore(&cpu_rq(cpu)->lock, flags);
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

static int fbt_check_to_jerk(
		unsigned long long enq_start, unsigned long long enq_end,
		unsigned long long deq_start, unsigned long long deq_end,
		unsigned long long deq_len, int pid,
		unsigned long long buffer_id, int tgid)
{
	/*not running*/
	if (check_running && !fbt_query_state(pid, tgid)) {
		fpsgo_systrace_c_fbt(pid, buffer_id, 1, "not_running");
		fpsgo_systrace_c_fbt(pid, buffer_id, 0, "not_running");
		return FPSGO_JERK_DISAPPEAR;
	}

	/*during enqueue*/
	if (enq_start >= enq_end) {
		fpsgo_systrace_c_fbt(pid, buffer_id, 1, "wait_enqueue");
		fpsgo_systrace_c_fbt(pid, buffer_id, 0, "wait_enqueue");
		return FPSGO_JERK_ONLY_CEILING;
	}

	/*after enqueue before dequeue*/
	if (enq_end > enq_start && enq_end >= deq_start)
		return FPSGO_JERK_NEED;

	/*after dequeue before enqueue*/
	if (deq_end >= deq_start && deq_start > enq_end) {
		if (deq_len > deqtime_bound) {
			fpsgo_systrace_c_fbt(pid, buffer_id, 1, "wait_dequeue");
			fpsgo_systrace_c_fbt(pid, buffer_id, 0, "wait_dequeue");
			return FPSGO_JERK_ONLY_CEILING;
		} else
			return FPSGO_JERK_NEED;
	}

	/*during dequeue*/
	fpsgo_systrace_c_fbt(pid, buffer_id, 1, "jerk_postpone");
	fpsgo_systrace_c_fbt(pid, buffer_id, 0, "jerk_postpone");
	return FPSGO_JERK_POSTPONE;
}

static void fbt_do_jerk_boost(struct render_info *thr, int blc_wt)
{
	if (boost_ta)
		fbt_set_boost_value(blc_wt);
	else
		fbt_set_min_cap_locked(thr, blc_wt, 1);

	if (ultra_rescue)
		fbt_boost_dram(1);
}

static void fbt_do_jerk_locked(struct render_info *thr, struct fbt_jerk *jerk)
{
	unsigned int blc_wt = 0U;
	struct ppm_limit_data *pld;
	int do_jerk;

	blc_wt  = thr->boost_info.last_blc;
	if (!blc_wt)
		return;

	pld = kcalloc(cluster_num,
			sizeof(struct ppm_limit_data), GFP_KERNEL);
	if (!pld)
		return;

	mutex_lock(&fbt_mlock);

	blc_wt = fbt_get_new_base_blc(pld, blc_wt, rescue_enhance_f, rescue_opp_c);
	if (!blc_wt)
		goto EXIT;

	blc_wt = fbt_limit_capacity(blc_wt, 1);

	do_jerk = fbt_check_to_jerk(thr->t_enqueue_start, thr->t_enqueue_end,
				thr->t_dequeue_start, thr->t_dequeue_end,
				thr->dequeue_length,
				thr->pid, thr->buffer_id, thr->tgid);

	if (do_jerk == FPSGO_JERK_DISAPPEAR)
		goto EXIT;

	if (do_jerk != FPSGO_JERK_POSTPONE) {
		fbt_set_hard_limit_locked(FPSGO_HARD_CEILING, pld);
		fbt_set_ceiling(pld, thr->pid, thr->buffer_id);

		thr->boost_info.cur_stage = FPSGO_JERK_FIRST;
		if (thr->pid == max_blc_pid && thr->buffer_id == max_blc_buffer_id) {
			max_blc_stage = FPSGO_JERK_FIRST;
			fpsgo_systrace_c_fbt_gm(-100, 0, max_blc_stage, "max_blc_stage");
		}
	} else if (jerk)
		jerk->postpone = 1;

	if (do_jerk == FPSGO_JERK_NEED) {
		fbt_do_jerk_boost(thr, blc_wt);
		fpsgo_systrace_c_fbt(thr->pid, thr->buffer_id, blc_wt, "perf idx");
		thr->boost_info.last_blc = blc_wt;
	}

EXIT:
	mutex_unlock(&fbt_mlock);
	kfree(pld);

	{
		struct pob_fpsgo_qtsk_info pfqi = {0};

		pfqi.tskid = thr->pid;
		pfqi.cur_cpu_cap = blc_wt;
		pfqi.rescue_cpu = 1;

		pob_fpsgo_qtsk_update(POB_FPSGO_QTSK_CPUCAP_UPDATE, &pfqi);
	}
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

	if (jerk->id != proc->active_jerk_id || thr->linger != 0)
		goto EXIT;

	fbt_do_jerk_locked(thr, jerk);

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

static void fbt_check_cm_limit(struct render_info *thread_info,
		long long runtime)
{
	int last_blc = 0;

	if (!thread_info || !runtime)
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

	if (last_blc > cm_big_cap &&
		runtime > thread_info->boost_info.target_time + cm_tdiff)
		fbt_notify_CM_limit(1);
	else
		fbt_notify_CM_limit(0);
}

static const int fbt_varfps_level[] = {30, 45, 60, 75, 90, 105, 120};
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
	struct ppm_limit_data *pld;
	int *clus_opp;
	unsigned int *clus_floor_freq;

	int tgt_opp = 0;
	unsigned int mbhr;
	int mbhr_opp;
	int cluster, i = 0;
	int min_ceiling = 0;

	pld =
		kcalloc(cluster_num, sizeof(struct ppm_limit_data),
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

		mbhr_opp = max((clus_opp[cluster] - bhr_opp), 0);

		mbhr = clus_floor_freq[cluster] * 100U;
		mbhr = mbhr / cpu_max_freq;
		mbhr = mbhr + (unsigned int)bhr;
		mbhr = (min(mbhr, 100U) * cpu_max_freq);
		mbhr = mbhr / 100U;

		for (i = (NR_FREQ_CPU - 1); i > 0; i--) {
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
		|| bhr_opp == (NR_FREQ_CPU - 1))
		fbt_set_hard_limit_locked(FPSGO_HARD_NONE, pld);
	else {
		if (limit_policy != FPSGO_LIMIT_NONE)
			fbt_set_hard_limit_locked(FPSGO_HARD_LIMIT, pld);
		else if (blc_wt < cpu_dvfs[min_cap_cluster].capacity_ratio[0]
			&& pld[min_cap_cluster].max
				< cpu_dvfs[min_cap_cluster].power[0])
			fbt_set_cap_margin_locked(1);
		else
			fbt_set_cap_margin_locked(0);
	}

	if (boost_ta)
		fbt_set_boost_value(blc_wt);

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

	if (!thr || thr->boost_info.cur_stage == FPSGO_JERK_INACTIVE)
		return;

	// the max one is jerking
	if (max_blc_stage != FPSGO_JERK_INACTIVE)
		return;

	if (ultra_rescue)
		fbt_boost_dram(0);

	fbt_find_max_blc(&temp_blc, &temp_blc_pid, &temp_blc_buffer_id);
	if (temp_blc)
		fbt_do_boost(temp_blc, temp_blc_pid, temp_blc_buffer_id);

	fpsgo_systrace_c_fbt_gm(thr->pid, thr->buffer_id, temp_blc_pid, "reset");
}

static void fbt_set_limit(unsigned int blc_wt,
	int pid, unsigned long long buffer_id,
	struct render_info *thread_info, long long runtime)
{
	unsigned int final_blc = blc_wt;
	int final_blc_pid = pid;
	unsigned long long final_blc_buffer_id = buffer_id;

	if (!(blc_wt > max_blc ||
		(pid == max_blc_pid && buffer_id == max_blc_buffer_id))) {
		fbt_clear_state(thread_info);
		return;
	}

	if (ultra_rescue)
		fbt_boost_dram(0);

	if (pid == max_blc_pid && buffer_id == max_blc_buffer_id
			&& blc_wt < max_blc) {
		unsigned int temp_blc = 0;
		int temp_blc_pid = 0;
		unsigned long long temp_blc_buffer_id = 0;

		fbt_find_ex_max_blc(pid, buffer_id, &temp_blc,
				&temp_blc_pid, &temp_blc_buffer_id);
		if (blc_wt && temp_blc > blc_wt && temp_blc_pid
			&& (temp_blc_pid != pid ||
				temp_blc_buffer_id != buffer_id)) {
			fpsgo_systrace_c_fbt_gm(pid, buffer_id,
						temp_blc_pid, "replace");

			final_blc = temp_blc;
			final_blc_pid = temp_blc_pid;
			final_blc_buffer_id = temp_blc_buffer_id;

			goto EXIT;
		}
	}

	fbt_check_cm_limit(thread_info, runtime);

EXIT:
	fbt_do_boost(final_blc, final_blc_pid, final_blc_buffer_id);

	max_blc = final_blc;
	max_blc_pid = final_blc_pid;
	max_blc_buffer_id = final_blc_buffer_id;
	max_blc_stage = FPSGO_JERK_INACTIVE;
	fpsgo_systrace_c_fbt_gm(-100, 0, max_blc, "max_blc");
	fpsgo_systrace_c_fbt_gm(-100, 0, max_blc_pid, "max_blc_pid");
	fpsgo_systrace_c_fbt_gm(-100, 0, max_blc_buffer_id, "max_blc_buffer_id");
	fpsgo_systrace_c_fbt_gm(-100, 0, max_blc_stage, "max_blc_stage");
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
		clus_max_idx[i] = mt_ppm_userlimit_freq_limit_by_others(i);

	mutex_lock(&fbt_mlock);
	for (i = 0; i < cluster_num; i++) {
		if (clus_max_idx[i] < 0 || clus_max_idx[i] >= NR_FREQ_CPU)
			continue;

		clus_max_cap[i] = cpu_dvfs[i].capacity_ratio[clus_max_idx[i]];
		if (clus_max_cap[i] > max_cap) {
			max_cap = clus_max_cap[i];
			max_cluster = i;
		}
	}

	for (i = 0 ; i < cluster_num; i++)
		fpsgo_systrace_c_fbt_gm(-100, 0, clus_max_cap[i],
				"cluster%d max cap", i);
	fpsgo_systrace_c_fbt_gm(-100, 0, max_cluster, "max_cluster");

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
	int fps_rescue_percent, fps_short_rescue_ns, fps_min_rescue_percent;
	unsigned long long t_cpu_target;
	int target_fps_level;

	mutex_lock(&fbt_mlock);

	target_fps = min_t(int, target_fps, TARGET_UNLIMITED_FPS);

	target_fps_level = fbt_get_fps_level(target_fps);

	t_cpu_target = (unsigned long long)FBTCPU_SEC_DIVIDER;
	t_cpu_target = div64_u64(t_cpu_target,
		(unsigned long long)target_fps);

	if (target_fps_level == fbt_fps_level[0]) {
		fps_rescue_percent = rescue_percent;
		fps_short_rescue_ns = short_rescue_ns;
		fps_min_rescue_percent = min_rescue_percent;
	} else if (target_fps_level == fbt_fps_level[1]) {
		fps_rescue_percent = rescue_percent_90;
		fps_min_rescue_percent = rescue_percent_90;
		fps_short_rescue_ns =
			(rescue_percent_90 == DEF_RESCUE_PERCENT)
			? DEF_RESCUE_NS_TH : vsync_period;
	} else {
		fps_rescue_percent = rescue_percent_120;
		fps_min_rescue_percent = rescue_percent_120;
		fps_short_rescue_ns =
			(rescue_percent_120 == DEF_RESCUE_PERCENT)
			? DEF_RESCUE_NS_TH : vsync_period;
	}

	xgf_trace("fps=%d, rescue@(%d, %d, %d)", target_fps_level,
			fps_rescue_percent, fps_min_rescue_percent,
			fps_short_rescue_ns);

	queue_end = queue_start + t_cpu_target;
	next_vsync = fbt_get_next_vsync_locked(queue_end);

	if (next_vsync == 0ULL)
		goto exit;

	if (!always_running && fbt_maybe_vsync_aligned(queue_start))
		t2wnt = fbt_cal_t2wnt(t_cpu_target,
			ts, next_vsync, short_min_rescue_p);
	else {
		t2wnt = fbt_cal_t2wnt(t_cpu_target,
				ts, next_vsync, fps_rescue_percent);
		if (t2wnt == 0ULL)
			goto ERROR;

		if (t_cpu_target > t2wnt) {
			t2wnt = t_cpu_target;

			rescue_length = next_vsync - t2wnt - queue_start;
			if (rescue_length <= fps_short_rescue_ns)
				t2wnt = fbt_cal_t2wnt(t_cpu_target,
					ts, next_vsync,
					fps_min_rescue_percent);
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

static int fbt_boost_policy(
	long long t_cpu_cur,
	long long target_time,
	unsigned int target_fps,
	struct render_info *thread_info,
	unsigned long long ts,
	long aa)
{
	unsigned int blc_wt = 0U;
	unsigned long long temp_blc;
	unsigned long long t1, t2, t_Q2Q;
	struct fbt_boost_info *boost_info;
	int pid;
	unsigned long long buffer_id;
	struct hrtimer *timer;
	u64 t2wnt = 0ULL;
	int active_jerk_id = 0;

	if (!thread_info) {
		FPSGO_LOGE("ERROR %d\n", __LINE__);
		return 0;
	}

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
	if (aa < 0) {
		mutex_lock(&blc_mlock);
		if (thread_info->p_blc)
			blc_wt = thread_info->p_blc->blc;
		mutex_unlock(&blc_mlock);
		aa = 0;
	} else if (t1 > 0 && t_Q2Q > 0) {
		long long new_aa;

		new_aa = aa * t1;
		new_aa = div64_s64(new_aa, t_Q2Q);
		aa = new_aa;
		temp_blc = new_aa;
		do_div(temp_blc, (unsigned int)t2);
		blc_wt = (unsigned int)temp_blc;
	} else {
		temp_blc = aa;
		do_div(temp_blc, (unsigned int)t2);
		blc_wt = (unsigned int)temp_blc;
	}

	xgf_trace("perf_index=%d aa=%lld run=%llu target=%llu Q2Q=%llu",
		blc_wt, aa, t1, t2, t_Q2Q);
	fpsgo_systrace_c_fbt_gm(pid, buffer_id, aa, "aa");

	fbt_check_var(aa, target_fps, nsec_to_usec(target_time),
		&(boost_info->f_iter),
		&(boost_info->frame_info[0]),
		&(boost_info->floor),
		&(boost_info->floor_count),
		&(boost_info->reset_floor_bound));
	fpsgo_systrace_c_fbt(pid, buffer_id, boost_info->floor, "variance");

	blc_wt = clamp(blc_wt, 1U, 100U);

	if (boost_info->floor > 1) {
		int orig_blc = blc_wt;

		blc_wt = (blc_wt * (boost_info->floor + 100)) / 100U;
		blc_wt = clamp(blc_wt, 1U, 100U);
		blc_wt = fbt_must_enhance_floor(blc_wt, orig_blc, floor_opp);
	}

	blc_wt = fbt_limit_capacity(blc_wt, 0);

	fbt_set_limit(blc_wt, pid, buffer_id, thread_info, t_cpu_cur);

	if (!boost_ta)
		fbt_set_min_cap_locked(thread_info, blc_wt, 0);

	boost_info->target_time = target_time;
	boost_info->last_blc = blc_wt;
	boost_info->cur_stage = FPSGO_JERK_INACTIVE;
	mutex_unlock(&fbt_mlock);

	mutex_lock(&blc_mlock);
	if (thread_info->p_blc)
		thread_info->p_blc->blc = blc_wt;
	mutex_unlock(&blc_mlock);

	fpsgo_systrace_c_fbt(pid, buffer_id, blc_wt, "perf idx");
	boost_info->last_blc = blc_wt;

	if (blc_wt) {
		t2wnt = (u64) fbt_get_t2wnt(target_fps, ts,
			fbt_is_always_running(t_cpu_cur, target_time));
		fpsgo_systrace_c_fbt(pid, buffer_id, t2wnt, "t2wnt");

		if (t2wnt) {
			if (t2wnt == 1ULL) {
				fbt_do_jerk_locked(thread_info, NULL);
				goto EXIT;
			}

			if (t2wnt > FBTCPU_SEC_DIVIDER) {
				t2wnt = FBTCPU_SEC_DIVIDER;
				fpsgo_systrace_c_fbt_gm(pid, buffer_id, t2wnt,
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
		atomic_t last_ts, unsigned int obv)
{
	if (obv == 0)
		return 0;

	if (atomic_read(&last_ts) == 0)
		return 0;

	if (cur_ts > atomic_read(&last_ts)) {
		unsigned long long dur = (unsigned long long)cur_ts -
						atomic_read(&last_ts);

		return dur * obv;
	}

	return 0;
/*
 *	Since qu/deq notification may be delayed because of wq,
 *	loading could be over estimated if qu/deq start comes later.
 *	Keep the current design, and be awared of low power.
 */
}

static void fbt_check_max_blc_locked(void)
{
	unsigned int temp_blc = 0;
	int temp_blc_pid = 0;
	unsigned long long temp_blc_buffer_id = 0;

	fbt_find_max_blc(&temp_blc, &temp_blc_pid, &temp_blc_buffer_id);

	max_blc = temp_blc;
	max_blc_pid = temp_blc_pid;
	max_blc_buffer_id = temp_blc_buffer_id;
	max_blc_stage = FPSGO_JERK_INACTIVE;
	fpsgo_systrace_c_fbt_gm(-100, 0, max_blc, "max_blc");
	fpsgo_systrace_c_fbt_gm(-100, 0, max_blc_pid, "max_blc_pid");
	fpsgo_systrace_c_fbt_gm(-100, 0, max_blc_buffer_id,
		"max_blc_buffer_id");
	fpsgo_systrace_c_fbt_gm(-100, 0, max_blc_stage, "max_blc_stage");

	if (max_blc == 0 && max_blc_pid == 0 && max_blc_buffer_id == 0) {
		if (boost_ta)
			fbt_clear_boost_value();
		fbt_free_bhr();
		if (ultra_rescue)
			fbt_boost_dram(0);
		memset(base_opp, 0, cluster_num * sizeof(unsigned int));
		fbt_notify_CM_limit(0);
	} else
		fbt_set_limit(max_blc, max_blc_pid, max_blc_buffer_id, NULL, 0);
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

static int fbt_adjust_loading(struct render_info *thr, unsigned long long ts,
					int adjust)
{
	int new_ts;
	long loading = 0L;
	long *loading_cl;
	unsigned int temp_obv, *temp_obv_cl;
	unsigned long long loading_result = 0U;
	unsigned long flags;
	int i;

	new_ts = nsec_to_100usec(ts);

	loading_cl = kcalloc(cluster_num, sizeof(long), GFP_KERNEL);
	temp_obv_cl = kcalloc(cluster_num, sizeof(unsigned int), GFP_KERNEL);
	if (!loading_cl || !temp_obv_cl)
		return 0;

	spin_lock_irqsave(&freq_slock, flags);
	temp_obv = last_obv;
	for (i = 0; i < cluster_num; i++)
		temp_obv_cl[i] = clus_obv[i];
	spin_unlock_irqrestore(&freq_slock, flags);

	spin_lock_irqsave(&loading_slock, flags);

	if (thr->pLoading) {
		loading_result = fbt_est_loading(new_ts,
			thr->pLoading->last_cb_ts, temp_obv);
		atomic_add_return(loading_result, &thr->pLoading->loading);
		fpsgo_systrace_c_fbt_gm(thr->pid, thr->buffer_id,
			atomic_read(&thr->pLoading->loading), "loading");
		loading = atomic_read(&thr->pLoading->loading);
		atomic_set(&thr->pLoading->loading, 0);
		fpsgo_systrace_c_fbt_gm(thr->pid, thr->buffer_id,
			atomic_read(&thr->pLoading->loading), "loading");

		if (!adjust_loading || cluster_num == 1
			|| !thr->pLoading->loading_cl) {
			adjust = FPSGO_ADJ_NONE;
			goto SKIP;
		}

		for (i = 0; i < cluster_num; i++) {
			loading_result = fbt_est_loading(new_ts,
				thr->pLoading->last_cb_ts, temp_obv_cl[i]);
			atomic_add_return(loading_result,
					&thr->pLoading->loading_cl[i]);
			fpsgo_systrace_c_fbt_gm(thr->pid, thr->buffer_id,
				atomic_read(&thr->pLoading->loading_cl[i]),
				"loading_cl[%d]", i);
			loading_cl[i] =
				atomic_read(&thr->pLoading->loading_cl[i]);
			atomic_set(&thr->pLoading->loading_cl[i], 0);
			fpsgo_systrace_c_fbt_gm(thr->pid, thr->buffer_id,
				atomic_read(&thr->pLoading->loading_cl[i]),
				"loading_cl[%d]", i);
		}

SKIP:
		atomic_set(&thr->pLoading->last_cb_ts, new_ts);
		fpsgo_systrace_c_fbt_gm(thr->pid, thr->buffer_id,
			atomic_read(&thr->pLoading->last_cb_ts), "last_cb_ts");
	}
	spin_unlock_irqrestore(&loading_slock, flags);

	if (adjust != FPSGO_ADJ_NONE) {
		int first_cluster, sec_cluster;

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

		loading_result = thr->boost_info.loading_weight;
		loading_result = loading_result *
					loading_cl[sec_cluster];
		loading_result += (100 - thr->boost_info.loading_weight) *
					loading_cl[first_cluster];
		do_div(loading_result, 100);
		loading = (long)loading_result;
	}

	kfree(loading_cl);
	kfree(temp_obv_cl);
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

	if (!adjust_loading || cluster_num == 1)
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

	fpsgo_systrace_c_fbt_gm(thr->pid, thr->buffer_id,
		boost->weight_cnt, "weight_cnt");
	fpsgo_systrace_c_fbt_gm(thr->pid, thr->buffer_id,
		boost->hit_cnt, "hit_cnt");
	fpsgo_systrace_c_fbt_gm(thr->pid, thr->buffer_id,
		boost->deb_cnt, "deb_cnt");
	fpsgo_systrace_c_fbt_gm(thr->pid, thr->buffer_id,
		boost->hit_cluster, "hit_cluster");

SKIP:
	loading = fbt_adjust_loading(thr, ts, adjust);

	return loading;
}

static void fbt_reset_boost(struct render_info *thr)
{
	struct fbt_boost_info *boost = NULL;

	if (!thr)
		return;

	mutex_lock(&blc_mlock);
	if (thr->p_blc)
		thr->p_blc->blc = 0;
	mutex_unlock(&blc_mlock);

	boost = &(thr->boost_info);

	boost->last_blc = 0;
	boost->target_time = 0;

	memset(boost->frame_info, 0, WINDOW * sizeof(struct fbt_frame_info));
	boost->f_iter = 0;
	boost->floor_count = 0;
	boost->floor = 0;
	boost->reset_floor_bound = 0;

	mutex_lock(&fbt_mlock);
	if (!boost_ta)
		fbt_set_min_cap_locked(thr, 0, 0);
	fbt_check_max_blc_locked();
	mutex_unlock(&fbt_mlock);

	fpsgo_systrace_c_fbt(thr->pid, thr->buffer_id, 0, "perf idx");
}

static void fbt_frame_start(struct render_info *thr, unsigned long long ts)
{
	struct fbt_boost_info *boost;
	long long runtime;
	int targettime, targetfps;
	unsigned int limited_cap = 0;
	int blc_wt = 0;
	long loading = 0L;

	if (!thr)
		return;

	boost = &(thr->boost_info);

	runtime = thr->running_time;
	boost->frame_info[boost->f_iter].running_time = runtime;

	fpsgo_fbt2fstb_query_fps(thr->pid, thr->buffer_id,
		&targetfps, &targettime, thr->tgid, thr->mid);
	if (!targetfps)
		targetfps = TARGET_UNLIMITED_FPS;

	fpsgo_systrace_c_fbt(thr->pid, thr->buffer_id, targetfps, "target_fps");
	fpsgo_systrace_c_fbt(thr->pid, thr->buffer_id,
		targettime, "target_time");

	loading = fbt_get_loading(thr, ts);
	fpsgo_systrace_c_fbt_gm(thr->pid, thr->buffer_id,
		loading, "compute_loading");

	/* unreliable targetfps */
	if (targetfps == -1) {
		fbt_reset_boost(thr);
		runtime = -1;
		goto EXIT;
	}

	blc_wt = fbt_boost_policy(runtime,
			targettime, targetfps,
			thr, ts, loading);

	limited_cap = fbt_get_max_userlimit_freq();
	fpsgo_systrace_c_fbt(thr->pid, thr->buffer_id,
		limited_cap, "limited_cap");

EXIT:
	fpsgo_fbt2fstb_update_cpu_frame_info(thr->pid, thr->buffer_id,
		thr->tgid, thr->frame_type,
		thr->Q2Q_time, runtime,
		blc_wt, limited_cap, thr->mid);
}

static void fbt_setting_reset(int reset_idleprefer)
{
	if (reset_idleprefer)
		fbt_set_idleprefer_locked(0);

	fbt_filter_ppm_log_locked(0);
	fbt_set_down_throttle_locked(-1);
	fbt_set_sync_flag_locked(-1);
	fbt_free_bhr();
	fbt_notify_CM_limit(0);

	if (boost_ta)
		fbt_clear_boost_value();

	if (ultra_rescue)
		fbt_boost_dram(0);
}

void fpsgo_ctrl2fbt_cpufreq_cb(int cid, unsigned long freq)
{
	unsigned long flags, flags2;
	unsigned int curr_obv = 0U;
	unsigned long long curr_cb_ts;
	int new_ts;
	int i;
	int opp;
	unsigned long long loading_result = 0U;
	struct fbt_thread_loading *pos, *next;

	if (!fbt_enable)
		return;

	if (cid >= cluster_num)
		return;

	curr_cb_ts = fpsgo_get_time();
	new_ts = nsec_to_100usec(curr_cb_ts);

	for (opp = (NR_FREQ_CPU - 1); opp > 0; opp--) {
		if (cpu_dvfs[cid].power[opp] >= freq)
			break;
	}
	curr_obv = cpu_dvfs[cid].capacity_ratio[opp];

	spin_lock_irqsave(&freq_slock, flags);

	if (clus_obv[cid] == curr_obv) {
		spin_unlock_irqrestore(&freq_slock, flags);
		return;
	}

	fpsgo_systrace_c_fbt(0, 0, freq, "curr_freq[%d]", cid);

	spin_lock_irqsave(&loading_slock, flags2);
	list_for_each_entry_safe(pos, next, &loading_list, entry) {
		if (atomic_read(&pos->last_cb_ts) != 0) {
			loading_result =
				fbt_est_loading(new_ts,
					pos->last_cb_ts, last_obv);
			atomic_add_return(loading_result, &(pos->loading));
			fpsgo_systrace_c_fbt_gm(pos->pid, pos->buffer_id,
				atomic_read(&pos->loading), "loading");

			if (!adjust_loading || cluster_num == 1
				|| !pos->loading_cl)
				goto SKIP;

			for (i = 0; i < cluster_num; i++) {
				loading_result =
					fbt_est_loading(new_ts,
					pos->last_cb_ts, clus_obv[i]);
				atomic_add_return(loading_result,
					&(pos->loading_cl[i]));
				fpsgo_systrace_c_fbt_gm(pos->pid,
					pos->buffer_id,
					atomic_read(&pos->loading_cl[i]),
					"loading_cl[%d]", i);
			}
		}
SKIP:
		atomic_set(&pos->last_cb_ts, new_ts);
		fpsgo_systrace_c_fbt_gm(pos->pid, pos->buffer_id,
			atomic_read(&pos->last_cb_ts), "last_cb_ts");
	}
	spin_unlock_irqrestore(&loading_slock, flags2);

	clus_obv[cid] = curr_obv;

	for (i = 0; i < cluster_num; i++) {
		if (curr_obv < clus_obv[i])
			curr_obv = clus_obv[i];
	}
	last_obv = curr_obv;
	fpsgo_systrace_c_fbt_gm(-100, 0, last_obv, "last_obv");

	spin_unlock_irqrestore(&freq_slock, flags);
}

void fpsgo_ctrl2fbt_vsync(unsigned long long ts)
{
	if (!fbt_is_enable())
		return;

	mutex_lock(&fbt_mlock);
	vsync_time = ts;
	xgf_trace("vsync_time=%llu", nsec_to_usec(vsync_time));
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

	mutex_lock(&fbt_mlock);
	fbt_set_idleprefer_locked(1);
	fbt_set_down_throttle_locked(0);
	fbt_set_sync_flag_locked(0);
	mutex_unlock(&fbt_mlock);

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

void fpsgo_comp2fbt_bypass_enq(void)
{
	mutex_lock(&fbt_mlock);

	if (!fbt_enable) {
		mutex_unlock(&fbt_mlock);
		return;
	}

	if (!bypass_flag) {
		bypass_flag = 1;
		fbt_set_idleprefer_locked(1);

		fpsgo_systrace_c_fbt_gm(-100, 0, bypass_flag, "bypass_flag");
	}
	mutex_unlock(&fbt_mlock);
}

void fpsgo_base2fbt_set_bypass(int has_bypass)
{
	mutex_lock(&fbt_mlock);

	if (!fbt_enable) {
		mutex_unlock(&fbt_mlock);
		return;
	}

	if (bypass_flag != has_bypass) {
		bypass_flag = has_bypass;
		fpsgo_systrace_c_fbt_gm(-100, 0, bypass_flag, "bypass_flag");
	}
	mutex_unlock(&fbt_mlock);
}

void fpsgo_comp2fbt_bypass_disconnect(void)
{
	int has_bypass;

	if (!fbt_is_enable())
		return;

	has_bypass = fpsgo_has_bypass();

	fpsgo_base2fbt_set_bypass(has_bypass);
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
	struct fbt_thread_loading *link;
	struct fbt_thread_blc *blc_link;
	struct fbt_boost_info *boost;
	int i;

	if (!obj)
		return;

	boost = &(obj->boost_info);
	for (i = 0; i < RESCUE_TIMER_NUM; i++)
		fbt_init_jerk(&(boost->proc.jerks[i]), i);

	boost->loading_weight = LOADING_WEIGHT;

	link = fbt_list_loading_add(obj->pid, obj->buffer_id);
	obj->pLoading = link;

	blc_link = fbt_list_blc_add(obj->pid, obj->buffer_id);
	obj->p_blc = blc_link;
}

void fpsgo_base2fbt_item_del(struct fbt_thread_loading *obj,
		struct fbt_thread_blc *pblc,
		struct fpsgo_loading *pdep,
		struct render_info *thr)
{
	unsigned long flags;

	if (!obj || !pblc)
		return;

	spin_lock_irqsave(&loading_slock, flags);
	list_del(&obj->entry);
	spin_unlock_irqrestore(&loading_slock, flags);
	kfree(obj->loading_cl);
	obj->loading_cl = NULL;
	kfree(obj);

	mutex_lock(&blc_mlock);
	fpsgo_systrace_c_fbt(pblc->pid, pblc->buffer_id, 0, "perf idx");
	list_del(&pblc->entry);
	kfree(pblc);
	mutex_unlock(&blc_mlock);

	mutex_lock(&fbt_mlock);
	if (!boost_ta)
		fbt_set_min_cap_locked(thr, 0, 0);
	if (thr)
		thr->boost_info.last_blc = 0;
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
	fbt_check_max_blc_locked();
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
	max_blc_pid = 0;
	max_blc_buffer_id = 0;
	max_blc_stage = FPSGO_JERK_INACTIVE;
	memset(base_opp, 0, cluster_num * sizeof(unsigned int));
	fpsgo_systrace_c_fbt_gm(-100, 0, max_blc, "max_blc");
	fpsgo_systrace_c_fbt_gm(-100, 0, max_blc_pid, "max_blc_pid");
	fpsgo_systrace_c_fbt_gm(-100, 0, max_blc_buffer_id,
		"max_blc_buffer_id");
	fpsgo_systrace_c_fbt_gm(-100, 0, max_blc_stage, "max_blc_stage");

	fbt_setting_reset(1);

	if (!boost_ta)
		clear_uclamp = 1;

	mutex_unlock(&fbt_mlock);

	if (clear_uclamp)
		fpsgo_clear_uclamp_boost();
}

void fpsgo_base2fbt_only_bypass(void)
{
	int clear_uclamp = 0;

	mutex_lock(&fbt_mlock);

	if (!fbt_enable) {
		mutex_unlock(&fbt_mlock);
		return;
	}

	xgf_trace("fpsgo only_bypass");

	fbt_setting_reset(0);

	if (!boost_ta)
		clear_uclamp = 1;

	mutex_unlock(&fbt_mlock);

	if (clear_uclamp)
		fpsgo_clear_uclamp_boost();
}

void fpsgo_base2fbt_set_min_cap(struct render_info *thr, int min_cap)
{
	mutex_lock(&fbt_mlock);
	fbt_set_min_cap_locked(thr, min_cap, 0);
	if (thr)
		thr->boost_info.last_blc = min_cap;
	mutex_unlock(&fbt_mlock);
}

void fpsgo_base2fbt_clear_llf_policy(struct render_info *thr, int orig_policy)
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

void fpsgo_uboost2fbt_uboost(struct render_info *thr)
{
	int floor, blc_wt = 0;
	struct ppm_limit_data *pld;

	if (!thr)
		return;

	pld = kcalloc(cluster_num, sizeof(struct ppm_limit_data), GFP_KERNEL);
	if (!pld)
		return;

	mutex_lock(&fbt_mlock);

	floor = thr->boost_info.last_blc;
	if (!floor)
		goto leave;

	blc_wt = fbt_get_new_base_blc(pld, floor, uboost_enhance_f, rescue_opp_c);
	if (!blc_wt)
		goto leave;

	blc_wt = fbt_limit_capacity(blc_wt, 1);
	fbt_set_hard_limit_locked(FPSGO_HARD_CEILING, pld);
	fbt_set_ceiling(pld, thr->pid, thr->buffer_id);

	if (boost_ta)
		fbt_set_boost_value(blc_wt);
	else
		fbt_set_min_cap_locked(thr, blc_wt, 1);

	thr->boost_info.last_blc = blc_wt;
	thr->boost_info.cur_stage = FPSGO_JERK_UBOOST;

	if (thr->pid == max_blc_pid && thr->buffer_id == max_blc_buffer_id) {
		max_blc_stage = FPSGO_JERK_UBOOST;
		fpsgo_systrace_c_fbt_gm(-100, 0, max_blc_stage, "max_blc_stage");
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

static int fbt_get_opp_by_freq(int cluster, unsigned int freq)
{
	int opp;

	if (cluster < 0 || cluster >= cluster_num)
		return INVALID_NUM;

	for (opp = (NR_FREQ_CPU - 1); opp > 0; opp--) {
		if (cpu_dvfs[cluster].power[opp] == freq)
			break;

		if (cpu_dvfs[cluster].power[opp] > freq) {
			opp++;
			break;
		}
	}

	opp = clamp(opp, 0, NR_FREQ_CPU - 1);

	return opp;
}

static void fbt_set_cap_limit(void)
{
	int freq = 0, limit_ret, r_freq = 0;
	int opp;
	int cluster;

	limit_cluster = INVALID_NUM;
	limit_copp = INVALID_NUM;
	limit_ropp = INVALID_NUM;
	limit_cfreq = 0;
	limit_rfreq = 0;
	limit_policy = FPSGO_LIMIT_NONE;

	limit_ret = fbt_get_cluster_limit(&cluster, &freq, &r_freq);
	if (!limit_ret)
		return;

	if (cluster < 0 || cluster >= cluster_num)
		return;

	limit_policy = FPSGO_LIMIT_CAPACITY;
	limit_cluster = cluster;

	if (!freq)
		goto RCEILING;

	opp = fbt_get_opp_by_freq(cluster, freq);
	if (opp == INVALID_NUM)
		goto RCEILING;

	limit_copp = opp;
	limit_cfreq = cpu_dvfs[cluster].power[opp];

RCEILING:
	if (!r_freq)
		goto CHECK;

	opp = fbt_get_opp_by_freq(cluster, r_freq);
	if (opp == INVALID_NUM)
		goto CHECK;

	limit_ropp = opp;
	limit_rfreq = cpu_dvfs[cluster].power[opp];

CHECK:
	if (limit_copp == INVALID_NUM && limit_ropp == INVALID_NUM)
		limit_policy = FPSGO_LIMIT_NONE;

	return;
}

static void fbt_update_pwd_tbl(void)
{
	int cluster, opp;
	unsigned int max_cap = 0, min_cap = UINT_MAX;

	for (cluster = 0; cluster < cluster_num ; cluster++) {
		struct cpumask cluster_cpus;
		int cpu;
		const struct sched_group_energy *core_energy = NULL;

		arch_get_cluster_cpus(&cluster_cpus, cluster);
		for_each_cpu(cpu, &cluster_cpus) {
			core_energy = cpu_core_energy(cpu);
			break;
		}

		if (!core_energy)
			break;

		for (opp = 0; opp < NR_FREQ_CPU; opp++) {
			unsigned long long cap = 0ULL;
			unsigned int temp;

			cpu_dvfs[cluster].power[opp] =
				mt_cpufreq_get_freq_by_idx(cluster, opp);

#ifdef CONFIG_NONLINEAR_FREQ_CTL
			cap = core_energy->cap_states[
					NR_FREQ_CPU - opp - 1].cap;
#else
			cap = core_energy->cap_states[NR_FREQ_CPU - 1].cap;
			cap = cap * cpu_dvfs[cluster].power[opp];
			if (cpu_dvfs[cluster].power[0])
				do_div(cap, cpu_dvfs[cluster].power[0]);
#endif

			cap = (cap * 100) >> 10;
			temp = (unsigned int)cap;
			temp = clamp(temp, 1U, 100U);
			cpu_dvfs[cluster].capacity_ratio[opp] = temp;
		}

		if (cpu_dvfs[cluster].capacity_ratio[0] > max_cap) {
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
	fbt_set_cap_limit();

	if (!cpu_max_freq) {
		FPSGO_LOGE("NULL power table\n");
		cpu_max_freq = 1;
	}
}

static void fbt_setting_exit(void)
{
	bypass_flag = 0;
	vsync_time = 0;
	memset(base_opp, 0, cluster_num * sizeof(unsigned int));
	max_blc = 0;
	max_blc_pid = 0;
	max_blc_buffer_id = 0;
	max_blc_stage = FPSGO_JERK_INACTIVE;

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

static ssize_t light_loading_policy_show(struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{
	char temp[FPSGO_SYSFS_MAX_BUFF_SIZE] = "";
	int posi = 0;
	int length;

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

	return scnprintf(buf, PAGE_SIZE, "%s", temp);
}

static ssize_t light_loading_policy_store(struct kobject *kobj,
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

	if (val < 0 || val > 100)
		return count;

	loading_policy = val;

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

	fbt_switch_idleprefer(val);

	return count;
}

static KOBJ_ATTR_RW(switch_idleprefer);

static ssize_t fbt_info_show(struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{
	struct fbt_thread_blc *pos, *next;
	char temp[FPSGO_SYSFS_MAX_BUFF_SIZE] = "";
	int posi = 0;
	int length;

	mutex_lock(&fbt_mlock);

	length = scnprintf(temp + posi,
		FPSGO_SYSFS_MAX_BUFF_SIZE - posi,
		"enable\tbypass\tidleprefer\tmax_blc\tmax_pid\tmax_bufID\tdfps\tvsync\n");
	posi += length;

	length = scnprintf(temp + posi,
		FPSGO_SYSFS_MAX_BUFF_SIZE - posi,
		"%d\t%d\t%d\t\t%d\t%d\t0x%llx\t%d\t%llu\n\n",
		fbt_enable, bypass_flag,
		set_idleprefer, max_blc, max_blc_pid,
		max_blc_buffer_id, _gdfrc_fps_limit, vsync_time);
	posi += length;

	mutex_unlock(&fbt_mlock);

	length = scnprintf(temp + posi,
		FPSGO_SYSFS_MAX_BUFF_SIZE - posi,
		"pid\tperfidx\t\n");
	posi += length;

	mutex_lock(&blc_mlock);
	list_for_each_entry_safe(pos, next, &blc_list, entry) {
		length = scnprintf(temp + posi,
			FPSGO_SYSFS_MAX_BUFF_SIZE - posi,
			"%d\t0x%llx\t%d\n", pos->pid, pos->buffer_id, pos->blc);
		posi += length;
	}
	mutex_unlock(&blc_mlock);

	return scnprintf(buf, PAGE_SIZE, "%s", temp);
}

static KOBJ_ATTR_RO(fbt_info);

static ssize_t table_show(struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{
	int cluster, opp;
	char temp[FPSGO_SYSFS_MAX_BUFF_SIZE] = "";
	int posi = 0;
	int length;

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
		"limit\tclus\tcopp\tropp\n");
	posi += length;

	length = scnprintf(temp + posi, FPSGO_SYSFS_MAX_BUFF_SIZE - posi,
		"%d\t%d\t%d\t%d\n\n",
		limit_policy, limit_cluster, limit_copp, limit_ropp);
	posi += length;

	for (cluster = 0; cluster < cluster_num ; cluster++) {
		for (opp = 0; opp < NR_FREQ_CPU; opp++) {
			length = scnprintf(temp + posi,
				FPSGO_SYSFS_MAX_BUFF_SIZE - posi,
				"[%d][%2d] %d, %d\n",
				cluster, opp,
				cpu_dvfs[cluster].power[opp],
				cpu_dvfs[cluster].capacity_ratio[opp]);
			posi += length;
		}
	}

	mutex_unlock(&fbt_mlock);

	return scnprintf(buf, PAGE_SIZE, "%s", temp);
}

static KOBJ_ATTR_RO(table);

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

	fbt_switch_to_ta(val);

	return count;
}

static KOBJ_ATTR_RW(boost_ta);

static ssize_t enable_switch_down_throttle_show(struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{
	char temp[FPSGO_SYSFS_MAX_BUFF_SIZE] = "";
	int posi = 0;
	int length;

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

	return scnprintf(buf, PAGE_SIZE, "%s", temp);
}

static ssize_t enable_switch_down_throttle_store(struct kobject *kobj,
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

	mutex_lock(&fbt_mlock);

	if (!fbt_enable) {
		mutex_unlock(&fbt_mlock);
		return count;
	}

	if (!val && down_throttle_ns != -1)
		fbt_set_down_throttle_locked(-1);

	fbt_down_throttle_enable = val;

	mutex_unlock(&fbt_mlock);

	return count;
}

static KOBJ_ATTR_RW(enable_switch_down_throttle);

static ssize_t enable_switch_sync_flag_show(struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{
	char temp[FPSGO_SYSFS_MAX_BUFF_SIZE] = "";
	int posi = 0;
	int length;

	mutex_lock(&fbt_mlock);
	length = scnprintf(temp + posi,
		FPSGO_SYSFS_MAX_BUFF_SIZE - posi,
		"fbt_sync_flag_enable %d\n", fbt_sync_flag_enable);
	posi += length;

	length = scnprintf(temp + posi,
		FPSGO_SYSFS_MAX_BUFF_SIZE - posi,
		"sync_flag %d\n", sync_flag);
	posi += length;
	mutex_unlock(&fbt_mlock);

	return scnprintf(buf, PAGE_SIZE, "%s", temp);
}

static ssize_t enable_switch_sync_flag_store(struct kobject *kobj,
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


	mutex_lock(&fbt_mlock);

	if (!fbt_enable) {
		mutex_unlock(&fbt_mlock);
		return count;
	}

	if (!val && sync_flag != -1)
		fbt_set_sync_flag_locked(-1);
	fbt_sync_flag_enable = val;

	mutex_unlock(&fbt_mlock);

	return count;
}

static KOBJ_ATTR_RW(enable_switch_sync_flag);

static ssize_t enable_switch_cap_margin_show(struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{
	char temp[FPSGO_SYSFS_MAX_BUFF_SIZE] = "";
	int posi = 0;
	int length;

	mutex_lock(&fbt_mlock);

	length = scnprintf(temp + posi,
		FPSGO_SYSFS_MAX_BUFF_SIZE - posi,
		"fbt_cap_margin_enable %d\n", fbt_cap_margin_enable);
	posi += length;

	length = scnprintf(temp + posi,
		FPSGO_SYSFS_MAX_BUFF_SIZE - posi,
		"set_cap_margin %d\n", set_cap_margin);
	posi += length;

	length = scnprintf(temp + posi,
		FPSGO_SYSFS_MAX_BUFF_SIZE - posi,
		"get_cap_margin %d\n", get_capacity_margin());
	posi += length;
	mutex_unlock(&fbt_mlock);

	return scnprintf(buf, PAGE_SIZE, "%s", temp);
}

static ssize_t enable_switch_cap_margin_store(struct kobject *kobj,
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

	mutex_lock(&fbt_mlock);

	if (!fbt_enable) {
		mutex_unlock(&fbt_mlock);
		return count;
	}

	if (!val && set_cap_margin != 0)
		fbt_set_cap_margin_locked(0);
	fbt_cap_margin_enable = val;

	mutex_unlock(&fbt_mlock);

	return count;
}

static KOBJ_ATTR_RW(enable_switch_cap_margin);

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

	mutex_lock(&fbt_mlock);

	if (!fbt_enable) {
		mutex_unlock(&fbt_mlock);
		return count;
	}

	fbt_set_ultra_rescue_locked(val);

	mutex_unlock(&fbt_mlock);

	return count;
}

static KOBJ_ATTR_RW(ultra_rescue);

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

	mutex_lock(&fbt_mlock);
	if (!fbt_enable) {
		mutex_unlock(&fbt_mlock);
		return count;
	}

	if (val < FPSGO_TPOLICY_NONE || val >= FPSGO_TPOLICY_MAX) {
		mutex_unlock(&fbt_mlock);
		return count;
	}

	if (llf_task_policy == val) {
		mutex_unlock(&fbt_mlock);
		return count;
	}

	orig_policy = llf_task_policy;
	llf_task_policy = val;
	xgf_trace("fpsgo set llf_task_policy %d", llf_task_policy);
	mutex_unlock(&fbt_mlock);

	fpsgo_clear_llf_cpu_policy(orig_policy);

	return count;
}

static KOBJ_ATTR_RW(llf_task_policy);

static ssize_t limit_policy_show(struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{
	int val = -1;

	mutex_lock(&fbt_mlock);
	val = limit_policy;
	mutex_unlock(&fbt_mlock);

	return scnprintf(buf, PAGE_SIZE, "%d\n", val);
}

static ssize_t limit_policy_store(struct kobject *kobj,
		struct kobj_attribute *attr,
		const char *buf, size_t count)
{
	int val = 0;
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

	mutex_lock(&fbt_mlock);
	if (!fbt_enable)
		goto EXIT;

	if (val < FPSGO_LIMIT_NONE || val >= FPSGO_LIMIT_MAX)
		goto EXIT;

	if (limit_policy == val)
		goto EXIT;

	if (limit_cluster < 0 || limit_cluster >= cluster_num)
		goto EXIT;

	fbt_set_hard_limit_locked(FPSGO_HARD_NONE, NULL);

	if (val == FPSGO_LIMIT_CAPACITY
		&& (limit_cluster < 0 || limit_cluster >= cluster_num))
		goto EXIT;

	limit_policy = val;

EXIT:
	mutex_unlock(&fbt_mlock);

	return count;
}

static KOBJ_ATTR_RW(limit_policy);

static ssize_t limit_cfreq_show(struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{
	int val = -1;

	mutex_lock(&fbt_mlock);
	val = limit_cfreq;
	mutex_unlock(&fbt_mlock);

	return scnprintf(buf, PAGE_SIZE, "%d\n", val);
}

static ssize_t limit_cfreq_store(struct kobject *kobj,
		struct kobj_attribute *attr,
		const char *buf, size_t count)
{
	int val = 0;
	char acBuffer[FPSGO_SYSFS_MAX_BUFF_SIZE];
	int arg;
	int opp;

	if ((count > 0) && (count < FPSGO_SYSFS_MAX_BUFF_SIZE)) {
		if (scnprintf(acBuffer, FPSGO_SYSFS_MAX_BUFF_SIZE, "%s", buf)) {
			if (kstrtoint(acBuffer, 0, &arg) == 0)
				val = arg;
			else
				return count;
		}
	}

	mutex_lock(&fbt_mlock);
	if (!fbt_enable)
		goto EXIT;

	if (limit_policy != FPSGO_LIMIT_CAPACITY)
		goto EXIT;

	if (val == 0) {
		limit_cfreq = 0;
		limit_copp = INVALID_NUM;
		goto EXIT;
	}

	if (limit_cluster >= cluster_num || limit_cluster < 0)
		goto EXIT;

	opp = fbt_get_opp_by_freq(limit_cluster, val);
	if (opp == INVALID_NUM)
		goto EXIT;

	limit_cfreq = cpu_dvfs[limit_cluster].power[opp];
	limit_copp = opp;

EXIT:
	mutex_unlock(&fbt_mlock);

	return count;
}

static KOBJ_ATTR_RW(limit_cfreq);

static ssize_t limit_rfreq_show(struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{
	int val = -1;

	mutex_lock(&fbt_mlock);
	val = limit_rfreq;
	mutex_unlock(&fbt_mlock);

	return scnprintf(buf, PAGE_SIZE, "%d\n", val);
}

static ssize_t limit_rfreq_store(struct kobject *kobj,
		struct kobj_attribute *attr,
		const char *buf, size_t count)
{
	int val = 0;
	char acBuffer[FPSGO_SYSFS_MAX_BUFF_SIZE];
	int arg;
	int opp;

	if ((count > 0) && (count < FPSGO_SYSFS_MAX_BUFF_SIZE)) {
		if (scnprintf(acBuffer, FPSGO_SYSFS_MAX_BUFF_SIZE, "%s", buf)) {
			if (kstrtoint(acBuffer, 0, &arg) == 0)
				val = arg;
			else
				return count;
		}
	}

	mutex_lock(&fbt_mlock);
	if (!fbt_enable)
		goto EXIT;

	if (limit_policy != FPSGO_LIMIT_CAPACITY)
		goto EXIT;

	if (val == 0) {
		limit_rfreq = 0;
		limit_ropp = INVALID_NUM;
		goto EXIT;
	}

	if (limit_cluster >= cluster_num || limit_cluster < 0)
		goto EXIT;

	opp = fbt_get_opp_by_freq(limit_cluster, val);
	if (opp == INVALID_NUM)
		goto EXIT;

	limit_rfreq = cpu_dvfs[limit_cluster].power[opp];
	limit_ropp = opp;

EXIT:
	mutex_unlock(&fbt_mlock);

	return count;
}

static KOBJ_ATTR_RW(limit_rfreq);

void __exit fbt_cpu_exit(void)
{
	minitop_exit();
	fbt_reg_dram_request(0);

	fpsgo_sysfs_remove_file(fbt_kobj,
			&kobj_attr_light_loading_policy);
	fpsgo_sysfs_remove_file(fbt_kobj,
			&kobj_attr_fbt_info);
	fpsgo_sysfs_remove_file(fbt_kobj,
			&kobj_attr_switch_idleprefer);
	fpsgo_sysfs_remove_file(fbt_kobj,
			&kobj_attr_table);
	fpsgo_sysfs_remove_file(fbt_kobj,
			&kobj_attr_enable_switch_down_throttle);
	fpsgo_sysfs_remove_file(fbt_kobj,
			&kobj_attr_enable_switch_sync_flag);
	fpsgo_sysfs_remove_file(fbt_kobj,
			&kobj_attr_enable_switch_cap_margin);
	fpsgo_sysfs_remove_file(fbt_kobj,
			&kobj_attr_ultra_rescue);
	fpsgo_sysfs_remove_file(fbt_kobj,
			&kobj_attr_llf_task_policy);
	fpsgo_sysfs_remove_file(fbt_kobj,
			&kobj_attr_boost_ta);
	fpsgo_sysfs_remove_file(fbt_kobj,
			&kobj_attr_limit_policy);
	fpsgo_sysfs_remove_file(fbt_kobj,
			&kobj_attr_limit_cfreq);
	fpsgo_sysfs_remove_file(fbt_kobj,
			&kobj_attr_limit_rfreq);

	fpsgo_sysfs_remove_dir(&fbt_kobj);

	kfree(base_opp);
	kfree(clus_obv);
	kfree(cpu_dvfs);
	kfree(clus_max_cap);
}

int __init fbt_cpu_init(void)
{
	bhr = 5;
	bhr_opp = 1;
	rescue_opp_c = (NR_FREQ_CPU - 1);
	rescue_opp_f = 5;
	rescue_percent = DEF_RESCUE_PERCENT;
	min_rescue_percent = 10;
	short_rescue_ns = DEF_RESCUE_NS_TH;
	rescue_percent_90 = DEF_RESCUE_PERCENT;
	rescue_percent_120 = DEF_RESCUE_PERCENT;
	short_min_rescue_p = 0;
	run_time_percent = 50;
	deqtime_bound = TIME_3MS;
	variance = 40;
	floor_bound = 3;
	kmin = 10;
	floor_opp = 2;
	loading_th = 0;
	sampling_period_MS = 256;
	rescue_enhance_f = 25;
	loading_adj_cnt = 30;
	loading_debnc_cnt = 30;
	loading_time_diff = TIME_2MS;
	llf_task_policy = FPSGO_TPOLICY_NONE;
	fps_level_range = 10;
	check_running = 1;
	uboost_enhance_f = 25;
	cm_big_cap = 95;
	cm_tdiff = TIME_1MS;

	_gdfrc_fps_limit = TARGET_DEFAULT_FPS;
	vsync_period = GED_VSYNC_MISS_QUANTUM_NS;

	fbt_idleprefer_enable = 1;
	suppress_ceiling = 1;
	down_throttle_ns = -1;
	fbt_down_throttle_enable = 1;
	sync_flag = -1;
	fbt_sync_flag_enable = 1;
	def_capacity_margin = get_capacity_margin();
	fbt_cap_margin_enable = 1;
	boost_ta = fbt_get_default_boost_ta();
	adjust_loading = fbt_get_default_adj_loading();

	cluster_num = arch_get_nr_clusters();

	base_opp =
		kcalloc(cluster_num, sizeof(unsigned int), GFP_KERNEL);

	clus_obv =
		kcalloc(cluster_num, sizeof(unsigned int), GFP_KERNEL);

	cpu_dvfs =
		kcalloc(cluster_num, sizeof(struct fbt_cpu_dvfs_info),
				GFP_KERNEL);

	clus_max_cap =
		kcalloc(cluster_num, sizeof(int), GFP_KERNEL);

	fbt_update_pwd_tbl();

	if (!fpsgo_sysfs_create_dir(NULL, "fbt", &fbt_kobj)) {
		fpsgo_sysfs_create_file(fbt_kobj,
				&kobj_attr_light_loading_policy);
		fpsgo_sysfs_create_file(fbt_kobj,
				&kobj_attr_fbt_info);
		fpsgo_sysfs_create_file(fbt_kobj,
				&kobj_attr_switch_idleprefer);
		fpsgo_sysfs_create_file(fbt_kobj,
				&kobj_attr_table);
		fpsgo_sysfs_create_file(fbt_kobj,
				&kobj_attr_enable_switch_down_throttle);
		fpsgo_sysfs_create_file(fbt_kobj,
				&kobj_attr_enable_switch_sync_flag);
		fpsgo_sysfs_create_file(fbt_kobj,
				&kobj_attr_enable_switch_cap_margin);
		fpsgo_sysfs_create_file(fbt_kobj,
				&kobj_attr_ultra_rescue);
		fpsgo_sysfs_create_file(fbt_kobj,
				&kobj_attr_llf_task_policy);
		fpsgo_sysfs_create_file(fbt_kobj,
				&kobj_attr_boost_ta);
		fpsgo_sysfs_create_file(fbt_kobj,
				&kobj_attr_limit_policy);
		fpsgo_sysfs_create_file(fbt_kobj,
				&kobj_attr_limit_cfreq);
		fpsgo_sysfs_create_file(fbt_kobj,
				&kobj_attr_limit_rfreq);
	}


	INIT_LIST_HEAD(&loading_list);
	INIT_LIST_HEAD(&blc_list);

	/* sub-module initialization */
	init_xgf();
	minitop_init();
	fbt_reg_dram_request(1);

	return 0;
}
