/*
 * Copyright (C) 2018 MediaTek Inc.
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
#include <linux/module.h>
#include <linux/rbtree.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/cpumask.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/debugfs.h>
#include <linux/timer.h>
#include <linux/workqueue.h>
#include <linux/preempt.h>
#include <linux/trace_events.h>
#include <trace/events/fpsgo.h>
#include <linux/sched/sched.h>
#include <linux/sched/clock.h>
#include <linux/sched/topology.h>
#include <linux/sched/energy.h>
#include <ap_thermal_limit.h>
#include "mtk_ppm_platform.h"
#include "mtk_ppm_internal.h"
#include "mtk_upower.h"
#include "mtk_gpufreq.h"

#ifdef CONFIG_MTK_PERF_OBSERVER
#include <mt-plat/mtk_perfobserver.h>
#endif
#include "thermal_budget_platform.h"
#include "thermal_budget.h"

#ifdef MAX
#undef MAX
#endif
#ifdef MIN
#undef MIN
#endif

#include <tscpu_settings.h>

#if defined(CONFIG_MTK_VPU_SUPPORT) && defined(THERMAL_VPU_SUPPORT)
#define EARA_THERMAL_VPU_SUPPORT
#endif

#if defined(CONFIG_MTK_MDLA_SUPPORT) && defined(THERMAL_MDLA_SUPPORT)
#define EARA_THERMAL_MDLA_SUPPORT
#endif

#if defined(EARA_THERMAL_VPU_SUPPORT) || defined(EARA_THERMAL_MDLA_SUPPORT)
#define EARA_THERMAL_AI_SUPPORT
#endif


#ifdef EARA_THERMAL_VPU_SUPPORT
#include "vpu_dvfs.h"
#endif

#ifdef EARA_THERMAL_MDLA_SUPPORT
#include "mdla_dvfs.h"
#endif

#define INIT_UNSET -1
#define TIME_1S_MS  1000
#define TIME_1S  1000000000ULL
#define TOO_LONG_TIME 200000000
#define TOO_SHORT_TIME 0
#define REPLACE_FRAME_COUNT 3
#define STABLE_TH 5
#define BYPASS_TH 3

#define EARA_THRM_TAG		"EARA_THRM:"
#define EARA_THRM_LOGI(fmt, args...)				\
	do {							\
		if (enable_debug_log) {				\
			pr_debug(EARA_THRM_TAG fmt, ##args);	\
		}						\
	} while (0)

#define EARA_THRM_LOGE(fmt, args...)				\
	do {							\
		static int count;				\
								\
		if (enable_debug_log)				\
			pr_debug(EARA_THRM_TAG "ERROR:" fmt, ##args);	\
		else {						\
			if (count > 100) {			\
				pr_debug(EARA_THRM_TAG "ERROR:" fmt, ##args); \
				count = 0;			\
			} else					\
				count++;			\
		}						\
	} while (0)

#define thrm_systrace(mask, pid, val, fmt...) \
	do { \
		if (mask) \
			__systrace(pid, val, fmt); \
	} while (0)

#define thrm_systrace_default(pid, val, fmt...) \
	thrm_systrace(1, pid, val, EARA_THRM_TAG fmt)

#define TIME_MAX(a, b) ((a) >= (b) ? (a) : (b))
#define DIFF_ABS(a, b) (((a) >= (b)) ? ((a) - (b)) : ((b) - (a)))
#define ACT_CORE(cluster)	(active_core[CLUSTER_##cluster])
#define CORE_LIMIT(cluster)	(core_limit[CLUSTER_##cluster])
#define for_each_clusters(i)	for (i = 0; i < g_cluster_num; i++)
#define CORE_CAP(cls, opp)	(cpu_dvfs_tbl[cls].capacity_ratio[opp])

static void wq_func(unsigned long data);
static void thrm_pb_turn_record_locked(int ready);
static void set_major_pair_locked(int pid, int max_time);

enum COBRA_RESULT {
	NO_CHANGE = 0,
	COBRA_SUCCESS,
};

enum RENDER_AI_TYPE {
	NO_AI = 0,
	AI_PER_FRAME = 1,
	AI_CROSS_VPU = 2,
	AI_CROSS_MDLA = 4,
};

enum THRM_MODULE {
	THRM_GPU,
	THRM_VPU,
	THRM_MDLA,
	THRM_CPU_OFFSET,
	THRM_CPU,
};

#ifdef CPU_OPP_NUM
struct cpu_dvfs_info {
	unsigned int power[CPU_OPP_NUM];
	unsigned int capacity_ratio[CPU_OPP_NUM];
};
#endif

#ifdef EARA_THERMAL_VPU_SUPPORT
struct vpu_dvfs_info {
	unsigned int freq[VPU_OPP_NUM];
	unsigned int cap[VPU_OPP_NUM];
	unsigned int power[VPU_OPP_NUM];
};
#endif

#ifdef EARA_THERMAL_MDLA_SUPPORT
struct mdla_dvfs_info {
	unsigned int freq[MDLA_OPP_NUM];
	unsigned int cap[MDLA_OPP_NUM];
	unsigned int power[MDLA_OPP_NUM];
};
#endif

struct thrm_pb_frame {
	int cpu_time;
	int gpu_time;
#ifdef EARA_THERMAL_VPU_SUPPORT
	int vpu_time;
	int vpu_opp;
#endif
#ifdef EARA_THERMAL_MDLA_SUPPORT
	int mdla_time;
	int mdla_opp;
#endif
	int AI_type;
	int queue_fps;
	int q2q_time;

	int bypass;
	int bypass_cnt;
};

struct thrm_pb_render {
	int pid;
	unsigned long long ts;
	int count;
	struct thrm_pb_frame frame_info;
	struct rb_node entry;
};

struct thrm_pb_realloc {
	int frame_time;
};

struct dentry *eara_thrm_debugfs_dir;

static struct apthermolmt_user ap_eara;
static char *ap_eara_log = "ap_eara";

static struct ppm_cobra_data *thr_cobra_tbl;
static struct cpu_dvfs_info *cpu_dvfs_tbl;
static struct mt_gpufreq_power_table_info *thr_gpu_tbl;
static int g_cluster_num;
static int g_modules_num;
static int g_gpu_opp_num;
static int g_max_gpu_opp_idx;

static int is_enable;
static int is_throttling;
static int has_record;
static int is_controllable;
static int is_timer_active;
static int is_perf_first;
static int enable_debug_log;
static int notified_clear;
static int fake_throttle;
static int fake_pb;
static int stable_count;

static int g_total_pb;
static int cur_cpu_pb;
static int cur_gpu_pb;
static int g_max_cpu_power;

static int cur_max_pid;
static int cur_max_time;

static int has_bg_AI;
static int is_VPU_on;
static int is_MDLA_on;
static int is_AI_on;

#ifdef EARA_THERMAL_VPU_SUPPORT
struct vpu_dvfs_info vpu_dvfs_tbl;
static int cur_vpu_pb;
static int *g_vpu_opp;
static int vpu_num;
static int min_vpu_power;
#endif

#ifdef EARA_THERMAL_MDLA_SUPPORT
struct mdla_dvfs_info mdla_dvfs_tbl;
static int cur_mdla_pb;
static int *g_mdla_opp;
static int mdla_num;
static int min_mdla_power;
#endif

static struct workqueue_struct *wq;
static DECLARE_WORK(eara_thrm_work, (void *) wq_func);
struct rb_root render_list;
static struct timer_list g_timer;
static DEFINE_MUTEX(thrm_lock);
static unsigned long __read_mostly mark_addr;

static struct ppm_cluster_status *g_cl_status;
static int *g_cl_cap;
static struct thrm_pb_ratio *g_opp_ratio;
static int *g_mod_opp;
static int *g_active_core;
static int *g_core_limit;

static unsigned long long get_time(void)
{
	unsigned long long temp;

	preempt_disable();
	temp = cpu_clock(smp_processor_id());
	preempt_enable();

	return temp;
}

static void __systrace(pid_t pid, int val, const char *fmt, ...)
{
	char log[256];
	va_list args;

	memset(log, ' ', sizeof(log));
	va_start(args, fmt);
	vsnprintf(log, sizeof(log), fmt, args);
	va_end(args);

	preempt_disable();
	event_trace_printk(mark_addr, "C|%d|%s|%d\n", pid, log, val);
	preempt_enable();
}

static void thrm_pb_list_clear(void)
{
	struct rb_node *n;
	struct thrm_pb_render *pos;

	EARA_THRM_LOGI("list clear\n");

	while ((n = rb_first(&render_list))) {
		pos = rb_entry(n, struct thrm_pb_render, entry);
		rb_erase(n, &render_list);
		kfree(pos);
	}
}

static void thrm_pb_list_delete(struct thrm_pb_render *obj)
{
	EARA_THRM_LOGI("list del %d\n", obj->pid);

	if (obj) {
		rb_erase(&obj->entry, &render_list);
		kfree(obj);
	}
}

static struct thrm_pb_render *thrm_pb_list_get(int pid, int add)
{
	struct rb_node **p = &render_list.rb_node;
	struct rb_node *parent = NULL;
	struct thrm_pb_render *thrm;

	while (*p) {
		parent = *p;
		thrm = rb_entry(parent, struct thrm_pb_render, entry);
		if (pid < thrm->pid)
			p = &(*p)->rb_left;
		else if (pid > thrm->pid)
			p = &(*p)->rb_right;
		else
			return thrm;
	}

	if (!add)
		return NULL;

	thrm = kzalloc(sizeof(*thrm), GFP_KERNEL);
	if (!thrm)
		return NULL;

	EARA_THRM_LOGI("list add %d\n", pid);

	thrm->pid = pid;
	rb_link_node(&thrm->entry, parent, p);
	rb_insert_color(&thrm->entry, &render_list);
	return thrm;
}

static int thrm_pb_list_find_AI(void)
{
	struct rb_node *n;
	struct thrm_pb_render *p = NULL;

	for (n = rb_first(&render_list); n ; n = rb_next(n)) {
		p = rb_entry(n, struct thrm_pb_render, entry);
		if (p->frame_info.AI_type != NO_AI)
			return 1;
	}

	return 0;
}

static inline int rb_is_singular(struct rb_root *root)
{
	struct rb_node *n;

	n = root->rb_node;
	if (!n)
		return 0;

	return (n->rb_left == NULL && n->rb_right == NULL);
}

static void get_cobra_tbl(void)
{
	struct ppm_cobra_data *cobra_tbl;

	if (thr_cobra_tbl)
		return;

	cobra_tbl = ppm_cobra_pass_tbl();
	if (!cobra_tbl)
		return;

	thr_cobra_tbl = kzalloc(sizeof(*thr_cobra_tbl), GFP_KERNEL);
	if (!thr_cobra_tbl)
		return;

	memcpy(thr_cobra_tbl, cobra_tbl, sizeof(*cobra_tbl));
}

static void activate_timer_locked(void)
{
	unsigned long expire;

	expire = jiffies + msecs_to_jiffies(TIME_1S_MS);
	g_timer.expires = round_jiffies_relative(expire);
	add_timer(&g_timer);
	is_timer_active = 1;
}

static void wq_func(unsigned long data)
{
	unsigned long long cur_ts;
	struct rb_node *n;
	struct rb_node *next;
	struct thrm_pb_render *p;

	mutex_lock(&thrm_lock);

	is_timer_active = 0;

	if (!has_record)
		goto exit;

	cur_ts = get_time();

	if (cur_ts < TIME_1S)
		goto next;

	eara_thrm_update_gpu_info(&g_gpu_opp_num, &g_max_gpu_opp_idx,
			&thr_gpu_tbl, &g_opp_ratio);

	for (n = rb_first(&render_list); n; n = next) {
		next = rb_next(n);

		p = rb_entry(n, struct thrm_pb_render, entry);
		if (p->ts >= cur_ts - TIME_1S)
			continue;

		if (p->pid == cur_max_pid)
			set_major_pair_locked(0, 0);
		thrm_pb_list_delete(p);
	}

	if (RB_EMPTY_ROOT(&render_list)) {
		EARA_THRM_LOGI("list is empty\n");
		thrm_pb_turn_record_locked(0);
		goto exit;
	}

next:
	activate_timer_locked();

exit:
	mutex_unlock(&thrm_lock);
}

static void timer_func(unsigned long data)
{
	if (wq)
		queue_work(wq, &eara_thrm_work);
}

static int gpu_freq_to_opp(unsigned int freq)
{
	int tgt_opp;

	for (tgt_opp = (g_gpu_opp_num - 1); tgt_opp > 0; tgt_opp--) {
		if (thr_gpu_tbl[tgt_opp].gpufreq_khz >= freq)
			break;
	}

	return tgt_opp;
}

#ifdef EARA_THERMAL_VPU_SUPPORT
static int vpu_cap_to_opp(unsigned int capacity_ratio)
{
	int opp;

	if (!capacity_ratio)
		return -1;

	for (opp = VPU_OPP_NUM - 1; opp > 0; opp--) {
		if (capacity_ratio <= vpu_dvfs_tbl.cap[opp])
			break;
	}

	EARA_THRM_LOGI("vpu opp %d, cap %d\n", opp, capacity_ratio);

	return opp;
}
#endif

#ifdef EARA_THERMAL_MDLA_SUPPORT
static int mdla_cap_to_opp(unsigned int capacity_ratio)
{
	int opp;

	if (!capacity_ratio)
		return -1;

	for (opp = MDLA_OPP_NUM - 1; opp > 0; opp--) {
		if (capacity_ratio <= mdla_dvfs_tbl.cap[opp])
			break;
	}

	EARA_THRM_LOGI("mdla opp %d, cap %d\n", opp, capacity_ratio);

	return opp;
}
#endif

static void set_major_pair_locked(int pid, int max_time)
{
	if (cur_max_pid != pid) {
		EARA_THRM_LOGI("Replace tracking thread %d -> %d\n",
			cur_max_pid, pid);
		cur_max_pid = pid;
	}

	cur_max_time = max_time;
}

static inline int __eara_arr_max(int *opp, int size)
{
	int ret = INT_MIN;
	int i;

	for (i = 0; i < size; i++) {
		if (opp[i] <= ret)
			continue;
		ret = opp[i];
	}
	return ret;
}

static int cal_max_cap_of_core(struct ppm_cluster_status *cl_status)
{
	int *arr;
	int i;
	int ret;

	arr = kcalloc(g_cluster_num, sizeof(int), GFP_KERNEL);
	if (!arr)
		return 0;

	for_each_clusters(i) {
		int opp = cl_status[i].freq_idx;

		arr[i] = (opp == -1) ? INT_MIN : CORE_CAP(i, opp);
	}

	ret = __eara_arr_max(arr, i);
	kfree(arr);

	return ret;
}

static int cal_system_capacity(int *cpu_cl_cap, int *core_limit)
{
	int i;
	int ret = 0;

	if (!cpu_cl_cap || !core_limit) {
		EARA_THRM_LOGE("%s\n", __func__);
		return 0;
	}

	for_each_clusters(i)
		ret += core_limit[i] * cpu_cl_cap[i];

	return ret;
}

static int cal_cpu_time(int cpu_time, int cur_cpu_cap, int cur_sys_cap,
	int *cpu_cl_cap, int *core_limit)
{
	uint64_t ret = 0;
	int cpu_cap = 0;
	int sys_cap;
	int i;

	if (!cpu_cl_cap || !core_limit) {
		EARA_THRM_LOGE("%s\n", __func__);
		return cpu_time;
	}

	if (!cur_sys_cap || !cur_cpu_cap || !cpu_time) {
		EARA_THRM_LOGI("%s:sys_cap %d, cpu_time %d, cpu_cap %d\n",
			__func__, cur_sys_cap, cpu_time, cur_cpu_cap);
		return cpu_time;
	}

	for_each_clusters(i) {
		if (cpu_cl_cap[i]) {
			ret = 1;
			break;
		}
	}

	if (!ret) {
		EARA_THRM_LOGI("%s:cpu_cl_cap all 0\n", __func__);
		return cpu_time;
	}

	sys_cap = cal_system_capacity(cpu_cl_cap, core_limit);
	cpu_cap = __eara_arr_max(cpu_cl_cap, g_cluster_num);
	if (!sys_cap || !cpu_cap) {
		EARA_THRM_LOGI("%s:sys_cap %d, cpu_cap %d\n",
			__func__, sys_cap, cpu_cap);
		return cpu_time;
	}

	ret = ((uint64_t)cpu_time) * cur_sys_cap * cur_cpu_cap;
	do_div(ret, ((uint64_t)sys_cap) * cpu_cap);

	EARA_THRM_LOGI("%s time %d curcap %d cursys %d cap %d sys %d ret %d\n",
		__func__, cpu_time, cur_cpu_cap, cur_sys_cap,
		cpu_cap, sys_cap, (int)ret);

	return (int)ret;
}

static int cal_xpu_time(int xpu_time, int xpu_freq, int new_freq)
{
	uint64_t ret;

	if (!new_freq || !xpu_freq || !xpu_time) {
		EARA_THRM_LOGI("%s:new_freq %d, xpu_freq %d, xpu_time %d\n",
			__func__, new_freq, xpu_freq, xpu_time);
		return xpu_time;
	}

	if (xpu_freq == new_freq)
		return xpu_time;

	ret = ((uint64_t)xpu_time) * xpu_freq;
	do_div(ret, new_freq);

	EARA_THRM_LOGI("%s xpu_time %d xpu_freq %d new_freq %d ret %d\n",
		__func__, xpu_time, xpu_freq, new_freq, (int)ret);

	return (int)ret;
}

static void eara_thrm_cpu_power_limit(int limit)
{
	int final_limit = (limit != 0) ? limit : 0x7FFFFFFF;

	if (!is_enable && limit)
		return;

	if (final_limit != cur_cpu_pb && final_limit > 0) {
		EARA_THRM_LOGI("%s: limit %d <- %d\n",
			__func__, final_limit, cur_cpu_pb);
		if (cur_max_pid > 1)
			thrm_systrace_default(cur_max_pid, final_limit,
					"cpu_limit");
		cur_cpu_pb = final_limit;
		apthermolmt_set_cpu_power_limit(&ap_eara, final_limit);
	}
}

static void eara_thrm_gpu_power_limit(int limit)
{
	int final_limit = (limit != 0) ? limit : 0x7FFFFFFF;

	if (!is_enable && limit)
		return;

	if (final_limit != cur_gpu_pb && final_limit > 0) {
		EARA_THRM_LOGI("%s: limit %d <- %d\n",
			__func__, final_limit, cur_gpu_pb);
		if (cur_max_pid > 1)
			thrm_systrace_default(cur_max_pid, final_limit,
					"gpu_limit");
		cur_gpu_pb = final_limit;
		apthermolmt_set_gpu_power_limit(&ap_eara, final_limit);
	}
}

static void eara_thrm_vpu_power_limit(int limit)
{
#ifdef EARA_THERMAL_VPU_SUPPORT
	int final_limit = (limit != 0) ? limit : 0x7FFFFFFF;

	if (!is_enable && limit)
		return;

	if (final_limit != cur_vpu_pb && final_limit > 0) {
		EARA_THRM_LOGI("%s: limit %d <- %d\n",
			__func__, final_limit, cur_vpu_pb);
		if (cur_max_pid > 1)
			thrm_systrace_default(cur_max_pid, final_limit,
					"vpu_limit");
		cur_vpu_pb = final_limit;
		apthermolmt_set_vpu_power_limit(&ap_eara, final_limit);
	}
#endif
}

static void eara_thrm_mdla_power_limit(int limit)
{
#ifdef EARA_THERMAL_MDLA_SUPPORT
	int final_limit = (limit != 0) ? limit : 0x7FFFFFFF;

	if (!is_enable && limit)
		return;

	if (final_limit != cur_mdla_pb && final_limit > 0) {
		EARA_THRM_LOGI("%s: limit %d <- %d\n",
			__func__, final_limit, cur_mdla_pb);
		if (cur_max_pid > 1)
			thrm_systrace_default(cur_max_pid, final_limit,
					"mdla_limit");
		cur_mdla_pb = final_limit;
		apthermolmt_set_mdla_power_limit(&ap_eara, final_limit);
	}
#endif
}

static void eara_thrm_set_power_limit(
	int cpu_limit, int gpu_limit, int vpu_limit, int mdla_limit)
{
	eara_thrm_cpu_power_limit(cpu_limit);
	eara_thrm_gpu_power_limit(gpu_limit);
	eara_thrm_vpu_power_limit(vpu_limit);
	eara_thrm_mdla_power_limit(mdla_limit);
}

static void eara_thrm_clear_limit(void)
{
	eara_thrm_set_power_limit(0, 0, 0, 0);
}

static void pass_perf_first_hint(int enable)
{
	WARN_ON(!mutex_is_locked(&thrm_lock));

	if (!is_enable && enable)
		return;

	if (is_perf_first == enable)
		return;

	is_perf_first = enable;
	if (cur_max_pid > 1)
		thrm_systrace_default(cur_max_pid, is_perf_first,
				"is_perf_first");

	EARA_THRM_LOGI("%s, %d\n", __func__, enable);
	eara_pass_perf_first_hint(enable);
}

static void thrm_pb_turn_cont_locked(int input)
{
	if (input == is_controllable)
		return;

	EARA_THRM_LOGI("%s, input %d, is_controllable %d\n",
		__func__, input, is_controllable);

	is_controllable = input;
	pass_perf_first_hint(input);

	if (!input)
		stable_count = 0;
}

static void thrm_pb_turn_record_locked(int input)
{
	if (input == has_record)
		return;

	EARA_THRM_LOGI("%s, input %d, has_record %d\n",
		__func__, input, has_record);

	has_record = input;

	if (input) {
		eara_thrm_update_gpu_info(&g_gpu_opp_num, &g_max_gpu_opp_idx,
			&thr_gpu_tbl, &g_opp_ratio);
		if (!is_timer_active)
			activate_timer_locked();
	} else {
		thrm_pb_turn_cont_locked(0);
		thrm_pb_list_clear();
		set_major_pair_locked(0, 0);
	}
}

static void thrm_pb_turn_throttling_locked(int throttling)
{
	if (throttling == is_throttling)
		return;

	EARA_THRM_LOGI("%s throttle %d is_throttling %d is_controllable %d\n",
		__func__, throttling, is_throttling, is_controllable);

	is_throttling = throttling;

	if (!throttling) {
		thrm_pb_turn_record_locked(0);
		g_total_pb = 0;
	}
}

#if defined(EARA_THERMAL_VPU_SUPPORT) || defined(EARA_THERMAL_MDLA_SUPPORT)
void check_AI_onoff_locked(int module)
{
	int AI_onoff;

#ifdef EARA_THERMAL_VPU_SUPPORT
	if (module == THRM_VPU && g_vpu_opp) {
		int i, onoff = 0;

		for (i = 0; i < vpu_num; i++) {
			if (g_vpu_opp[i] != -1)
				onoff = 1;
		}
		is_VPU_on = onoff;
	}
#endif
#ifdef EARA_THERMAL_MDLA_SUPPORT
	if (module == THRM_MDLA && g_mdla_opp) {
		int i, onoff = 0;

		for (i = 0; i < mdla_num; i++) {
			if (g_mdla_opp[i] != -1)
				onoff = 1;
		}
		is_MDLA_on = onoff;
	}
#endif

	EARA_THRM_LOGI("VPU [%d] MDLA [%d] AI [%d]\n",
		is_VPU_on, is_MDLA_on, is_AI_on);

	AI_onoff = is_VPU_on | is_MDLA_on;

	if (is_AI_on != AI_onoff)
		is_AI_on = AI_onoff;
}
#endif

#ifdef CONFIG_MTK_PERF_OBSERVER
static int eara_thrm_xpufreq_notifier_cb(struct notifier_block *nb,
			unsigned long val, void *data)
{
	mutex_lock(&thrm_lock);

	if (!is_enable)
		goto exit;

	switch (val) {
#ifdef EARA_THERMAL_VPU_SUPPORT
	case POB_XPUFREQ_VPU:
		{
			struct pob_xpufreq_info *pxi =
				(struct pob_xpufreq_info *) data;
			if (pxi->id >= eara_thrm_get_vpu_core_num())
				break;
			if (g_vpu_opp)
				g_vpu_opp[pxi->id] = pxi->opp;
			check_AI_onoff_locked(THRM_VPU);
			EARA_THRM_LOGI("VPU [%d] %d\n", pxi->id, pxi->opp);
		}
		break;
#endif
#ifdef EARA_THERMAL_MDLA_SUPPORT
	case POB_XPUFREQ_MDLA:
		{
			struct pob_xpufreq_info *pxi =
				(struct pob_xpufreq_info *) data;
			if (pxi->id >= eara_thrm_get_mdla_core_num())
				break;
			if (g_mdla_opp)
				g_mdla_opp[pxi->id] = pxi->opp;
			check_AI_onoff_locked(THRM_MDLA);
			EARA_THRM_LOGI("MDLA [%d] %d\n", pxi->id, pxi->opp);
		}
		break;
#endif
	default:
		break;
	}

exit:
	mutex_unlock(&thrm_lock);
	return NOTIFY_OK;
}

struct notifier_block eara_thrm_xpufreq_notifier = {
	.notifier_call = eara_thrm_xpufreq_notifier_cb,
};
#endif

static int is_AI_controllable(void)
{
	if (is_AI_on && !has_bg_AI && !thrm_pb_list_find_AI())
		return 0;

	return 1;
}

static int update_AI_type(int vpu_time, int vpu_boost, int vpu_cross,
		int mdla_time, int mdla_boost, int mdla_cross)
{
	int ret = 0;

	if (vpu_time || mdla_time)
		ret = AI_PER_FRAME;

	if (vpu_cross == 1)
		ret += AI_CROSS_VPU;

	if (mdla_cross == 1)
		ret += AI_CROSS_MDLA;

	return ret;
}

static void update_AI_bg(int vpu_bg, int mdla_bg)
{
	if (vpu_bg || mdla_bg)
		has_bg_AI = 1;
	else
		has_bg_AI = 0;
}

#define HAS_AI_PERFRAME(type) (type & AI_PER_FRAME)
#define HAS_AI_CROSS_V(type) ((type >> 1) & 1)
#define HAS_AI_CROSS_M(type) ((type >> 2) & 1)

static int AI_info_error_check(int type, int vpu_time,
		int mdla_time, int vpu_opp, int mdla_opp)
{
	if (vpu_time < 0 || mdla_time < 0)
		return 11;

#ifdef EARA_THERMAL_VPU_SUPPORT
	if (vpu_opp > VPU_OPP_NUM - 1)
		return 12;
#endif

#ifdef EARA_THERMAL_MDLA_SUPPORT
	if (mdla_opp > MDLA_OPP_NUM - 1)
		return 13;
#endif

	if (type == NO_AI) {
		if (vpu_time || mdla_time || vpu_opp != -1 || mdla_opp != -1)
			return 1;
		return 0;
	}

	if (HAS_AI_PERFRAME(type)) {
		if (!vpu_time && !mdla_time)
			return 2;
		if (vpu_opp == -1 && mdla_opp == -1)
			return 3;
		if (vpu_opp != -1 && !is_VPU_on)
			return 4;
		if (mdla_opp != -1 && !is_MDLA_on)
			return 5;
		if (vpu_opp != -1 && !vpu_time)
			return 6;
		if (mdla_opp != -1 && !mdla_time)
			return 7;
	} else {
		if (vpu_time || mdla_time)
			return 8;
	}
	if (HAS_AI_CROSS_V(type) && !is_VPU_on)
		return 9;
	if (HAS_AI_CROSS_M(type) && !is_MDLA_on)
		return 10;

	return 0;
}

static int need_reallocate(struct thrm_pb_render *thr, int pid)
{
	int max_time;
	int cpu_time = thr->frame_info.cpu_time;
	int gpu_time = thr->frame_info.gpu_time;
	int vpu_time = 0;
	int mdla_time = 0;

#ifdef EARA_THERMAL_VPU_SUPPORT
	vpu_time = thr->frame_info.vpu_time;
#endif
#ifdef EARA_THERMAL_MDLA_SUPPORT
	mdla_time = thr->frame_info.mdla_time;
#endif
	max_time = TIME_MAX((cpu_time + vpu_time + mdla_time), gpu_time);

	if (rb_is_singular(&render_list)) {
		if (cur_max_pid != pid)
			set_major_pair_locked(pid, max_time);
		return 1;
	}

	if (max_time > cur_max_time || pid == cur_max_pid) {
		if (pid != cur_max_pid && cur_max_pid) {
			thr->count++;

			if (thr->count > REPLACE_FRAME_COUNT)
				thr->count = 0;
			else
				return 0;
		}

		set_major_pair_locked(pid, max_time);
		return 1;
	}

	thr->count = 0;

	return 0;
}

static unsigned int get_idx_in_pwr_tbl(enum ppm_cluster cluster)
{
	unsigned int idx = 0;

	if (cluster >= g_cluster_num) {
		EARA_THRM_LOGE("%s: Invalid input: cluster=%d\n",
			__func__, cluster);
		cluster = 0;
	}

	while (cluster)
		idx += get_cluster_max_cpu_core(--cluster);

	return idx;
}

static short get_delta_pwr(enum THRM_MODULE module, unsigned int core,
	unsigned int opp)
{
	unsigned int idx;
	unsigned int cur_opp, prev_opp;
	int delta_pwr;
	enum ppm_cluster cluster;

#ifdef EARA_THERMAL_VPU_SUPPORT
	if (module == THRM_VPU) {
		if (opp >= VPU_OPP_NUM - 1) {
			EARA_THRM_LOGE("%s:Invalid module=%d core=%d opp=%d\n",
			__func__, module, core, opp);
			return 0;
		}
		delta_pwr = vpu_dvfs_tbl.power[opp]
			- vpu_dvfs_tbl.power[opp + 1];
		return delta_pwr;
	}
#endif

#ifdef EARA_THERMAL_MDLA_SUPPORT
	if (module == THRM_MDLA) {
		if (opp >= MDLA_OPP_NUM - 1) {
			EARA_THRM_LOGE("%s:Invalid module=%d core=%d opp=%d\n",
			__func__, module, core, opp);
			return 0;
		}
		delta_pwr = mdla_dvfs_tbl.power[opp]
			- mdla_dvfs_tbl.power[opp + 1];
		return delta_pwr;
	}
#endif

	if (!thr_cobra_tbl) {
		EARA_THRM_LOGE("%s: NULL cobra table\n", __func__);
		return 0;
	}

	cluster = module - THRM_CPU_OFFSET;

	if (core > get_cluster_max_cpu_core(cluster)
		|| opp > get_cluster_min_cpufreq_idx(cluster)) {
		EARA_THRM_LOGE("%s:Invalid cl=%d core=%d opp=%d\n",
			__func__, cluster, core, opp);
		return 0;
	}

	if (core == 0)
		return 0;

	idx = get_idx_in_pwr_tbl(cluster);

	cur_opp = opp;
	prev_opp = opp + 1;

	if (opp == CPU_OPP_NUM - 1) {
		delta_pwr = (core == 1)
			? thr_cobra_tbl->basic_pwr_tbl
				[idx+core-1][cur_opp].power_idx
			: (thr_cobra_tbl->basic_pwr_tbl
				[idx+core-1][cur_opp].power_idx
				- thr_cobra_tbl->basic_pwr_tbl
				[idx+core-2][cur_opp].power_idx);
	} else {
		delta_pwr = thr_cobra_tbl->basic_pwr_tbl
				[idx+core-1][cur_opp].power_idx
				- thr_cobra_tbl->basic_pwr_tbl
				[idx+core-1][prev_opp].power_idx;
	}

	return delta_pwr;
}

static int get_perf(enum ppm_cluster cluster, unsigned int core,
			unsigned int opp)
{
	unsigned int idx, min_idx;
	int perf;
	int ratio = 0;

	if (!thr_cobra_tbl) {
		EARA_THRM_LOGE("%s: NULL cobra table\n", __func__);
		return 0;
	}

	if (core > get_cluster_max_cpu_core(cluster)) {
		EARA_THRM_LOGE("%s:Invalid cluster=%d, core=%d\n",
			__func__, cluster, core);
		return 0;
	}

	if (core == 0)
		return 0;

	min_idx = get_cluster_min_cpufreq_idx(cluster);

	if (opp >= min_idx) {
		opp = min_idx;
		core--;
		ratio = 100;
	}

	if (core == 0)
		core = 1;

	idx = get_idx_in_pwr_tbl(cluster);

	perf = thr_cobra_tbl->basic_pwr_tbl[idx+core-1][opp].perf_idx *
		thr_cobra_tbl->basic_pwr_tbl[idx][opp].perf_idx;

	if (ratio)
		perf = perf * ratio;

	return perf;
}

#define IS_ABLE_DOWN_GEAR(core, opp) \
	(core > 0 && opp < (PPM_COBRA_MAX_FREQ_IDX - 1))

#define IS_ABLE_OFF_CORE(core, opp) \
	(core > 0 && opp == (PPM_COBRA_MAX_FREQ_IDX - 1))

static int reallocate_perf_first(int remain_budget,
		struct ppm_cluster_status *cl_status,
		int *active_core, int curr_power, int *opp, int *core_limit,
		int *out_cl_cap,
		int cpu_time, int cur_cap, int cur_sys_cap,
		int vpu_time, int vpu_opp, int mdla_time, int mdla_opp,
		struct thrm_pb_realloc *out_st)
{
	int i;
	int delta_power;
	int new_cpu_time, frame_time;

	if (!out_cl_cap || !cl_status || !active_core
		|| !core_limit || !opp || !out_st) {
		EARA_THRM_LOGE("%s:%d\n",
			__func__, __LINE__);
		return NO_CHANGE;
	}

	delta_power = remain_budget - curr_power;
	EARA_THRM_LOGI("E %s: remain %d, curr_power %d, delta_power %d, ",
			__func__, remain_budget, curr_power, delta_power);
	EARA_THRM_LOGI("opp (%d, %d, %d, %d), lmt (%d, %d)\n",
			opp[THRM_CPU_OFFSET + CLUSTER_L],
			opp[THRM_CPU_OFFSET + CLUSTER_B],
			opp[THRM_VPU], opp[THRM_MDLA],
			CORE_LIMIT(L), CORE_LIMIT(B));

	if (unlikely(!delta_power))
		return NO_CHANGE;

	/* increase ferquency limit */
	if (delta_power >= 0) {
		while (1) {
			int ChoosenCl = -1, MaxPerf = 0, ChoosenPwr = 0;
			int target_delta_pwr, target_perf;
			int ChoosenModule = -1;
#ifdef EARA_THERMAL_VPU_SUPPORT
			int new_vpu_time = 0;
#endif
#ifdef EARA_THERMAL_MDLA_SUPPORT
			int new_mdla_time = 0;
#endif

			if (opp[THRM_CPU_OFFSET + CLUSTER_L] == 0
				&& ACT_CORE(B) == 0) {
				target_delta_pwr = get_delta_pwr(
					THRM_CPU_OFFSET + CLUSTER_B, 1,
					CPU_OPP_NUM-1);
				if (delta_power >= target_delta_pwr) {
					ACT_CORE(B) = 1;
					delta_power -= target_delta_pwr;
					opp[THRM_CPU_OFFSET + CLUSTER_B] =
						CPU_OPP_NUM - 1;
				}
			}

			/* B-cluster */
			if (ACT_CORE(B) > 0
				&& opp[THRM_CPU_OFFSET + CLUSTER_B] > 0) {
				target_delta_pwr = get_delta_pwr(
					THRM_CPU_OFFSET + CLUSTER_B,
					ACT_CORE(B),
					opp[THRM_CPU_OFFSET + CLUSTER_B]-1);
				if (delta_power >= target_delta_pwr) {
					MaxPerf = get_perf(CLUSTER_B,
						ACT_CORE(B),
						opp[THRM_CPU_OFFSET +
						CLUSTER_B] - 1);
					ChoosenCl = CLUSTER_B;
					ChoosenPwr = target_delta_pwr;
				}
			}

			/* L-cluster */
			if (ACT_CORE(L) > 0
				&& opp[THRM_CPU_OFFSET + CLUSTER_L] > 0) {
				target_delta_pwr = get_delta_pwr(
					THRM_CPU_OFFSET + CLUSTER_L,
					ACT_CORE(L),
					opp[THRM_CPU_OFFSET + CLUSTER_L]-1);
				target_perf = get_perf(CLUSTER_L, ACT_CORE(L),
					opp[THRM_CPU_OFFSET + CLUSTER_L]-1);
				if (delta_power >= target_delta_pwr
					&& MaxPerf <= target_perf) {
					MaxPerf = target_perf;
					ChoosenCl = CLUSTER_L;
					ChoosenPwr = target_delta_pwr;
				}
			}

			if (!vpu_time && !mdla_time) {
				if (ChoosenCl != -1) {
					opp[THRM_CPU_OFFSET + ChoosenCl] -= 1;
					goto prepare_next_round;
				}
			}

			if (ChoosenCl != -1) {
				ChoosenModule = THRM_CPU;
				opp[THRM_CPU_OFFSET + ChoosenCl] -= 1;
				for_each_clusters(i) {
					out_cl_cap[i] = CORE_CAP(i,
						opp[THRM_CPU_OFFSET + i]);
					EARA_THRM_LOGI(
						"cl[%d] cap %d opp %d core %d\n",
						i, out_cl_cap[i],
						opp[THRM_CPU_OFFSET + i],
						core_limit[i]);
				}
				new_cpu_time = cal_cpu_time(cpu_time, cur_cap,
					cur_sys_cap, out_cl_cap, core_limit);
				opp[THRM_CPU_OFFSET + ChoosenCl] += 1;
			} else
				new_cpu_time = cpu_time;

			frame_time = new_cpu_time + vpu_time + mdla_time;

#ifdef EARA_THERMAL_VPU_SUPPORT
			if (vpu_time && opp[THRM_VPU] != -1
				&& opp[THRM_VPU] > 0) {
				target_delta_pwr = get_delta_pwr(
						THRM_VPU, 1, opp[THRM_VPU]-1);
				if (delta_power >= target_delta_pwr) {
					new_vpu_time = cal_xpu_time(vpu_time,
						vpu_dvfs_tbl.cap[vpu_opp],
						vpu_dvfs_tbl.cap[
							opp[THRM_VPU]-1]);
					if (frame_time > cpu_time + new_vpu_time
							+ mdla_time) {
						ChoosenCl = -1;
						ChoosenModule = THRM_VPU;
						ChoosenPwr = target_delta_pwr;
						frame_time = cpu_time
							+ new_vpu_time
							+ mdla_time;
					}
				}
			}
#endif

#ifdef EARA_THERMAL_MDLA_SUPPORT
			if (mdla_time && opp[THRM_MDLA] != -1
				&& opp[THRM_MDLA] > 0) {
				target_delta_pwr = get_delta_pwr(
					THRM_MDLA, 1, opp[THRM_MDLA]-1);
				if (delta_power >= target_delta_pwr) {
					new_mdla_time = cal_xpu_time(
						mdla_time,
						mdla_dvfs_tbl.cap[mdla_opp],
						mdla_dvfs_tbl.cap[
							opp[THRM_MDLA]-1]);
					if (frame_time >
						cpu_time + vpu_time
						+ new_mdla_time) {
						ChoosenCl = -1;
						ChoosenModule = THRM_MDLA;
						ChoosenPwr = target_delta_pwr;
					}
				}
			}
#endif

			if (ChoosenCl != -1) {
				opp[THRM_CPU_OFFSET + ChoosenCl] -= 1;
				cpu_time = new_cpu_time;
				cur_cap = __eara_arr_max(out_cl_cap,
						g_cluster_num);
				cur_sys_cap = cal_system_capacity(out_cl_cap,
						core_limit);
				goto prepare_next_round;
			}
#ifdef EARA_THERMAL_AI_SUPPORT
			else if (ChoosenModule != -1) {
				opp[ChoosenModule] -= 1;
#ifdef EARA_THERMAL_VPU_SUPPORT
				if (ChoosenModule == THRM_VPU) {
					vpu_time = new_vpu_time;
					vpu_opp = opp[ChoosenModule];
				}
#endif
#ifdef EARA_THERMAL_MDLA_SUPPORT
				if (ChoosenModule == THRM_MDLA) {
					mdla_time = new_mdla_time;
					mdla_opp = opp[ChoosenModule];
				}
#endif
				goto prepare_next_round;
			}
#endif
			/* give budget to B */
			while (CORE_LIMIT(B) <
				get_cluster_max_cpu_core(CLUSTER_B)) {
				target_delta_pwr = get_delta_pwr(
					THRM_CPU_OFFSET + CLUSTER_B,
					CORE_LIMIT(B)+1, CPU_OPP_NUM-1);
				if (delta_power < target_delta_pwr)
					break;

				delta_power -= target_delta_pwr;
				core_limit[CLUSTER_B] =
					core_limit[CLUSTER_B] + 1;
			}

			/* give budget to L */
			while (CORE_LIMIT(L) <
				get_cluster_max_cpu_core(CLUSTER_L)) {
				target_delta_pwr = get_delta_pwr(
					THRM_CPU_OFFSET + CLUSTER_L,
					CORE_LIMIT(L)+1, CPU_OPP_NUM-1);
				if (delta_power < target_delta_pwr)
					break;

				delta_power -= target_delta_pwr;
				core_limit[CLUSTER_L] =
					core_limit[CLUSTER_L] + 1;
			}

			EARA_THRM_LOGI("[+]ChoosenCl=-1! delta=%d\n",
					delta_power);
			EARA_THRM_LOGI("[+](opp)=(%d,%d,%d,%d)\n",
					opp[THRM_CPU_OFFSET + CLUSTER_L],
					opp[THRM_CPU_OFFSET + CLUSTER_B],
					opp[THRM_VPU], opp[THRM_MDLA]);
			EARA_THRM_LOGI("[+](act/c_lmt)=(%d,%d/%d,%d)\n",
					ACT_CORE(L), ACT_CORE(B),
					CORE_LIMIT(L), CORE_LIMIT(B));

			break;

prepare_next_round:
			delta_power -= ChoosenPwr;

			EARA_THRM_LOGI("[+](delta/Cl/Mod/Pwr)=(%d,%d,%d,%d)\n",
				delta_power, ChoosenCl, ChoosenModule,
				ChoosenPwr);
			EARA_THRM_LOGI("[+]opp=%d,%d,%d,%d\n",
				opp[THRM_CPU_OFFSET + CLUSTER_L],
				opp[THRM_CPU_OFFSET + CLUSTER_B],
				opp[THRM_VPU],
				opp[THRM_MDLA]);
		}
	} else {
		while (delta_power < 0) {
			int ChoosenCl = -1;
			int ChoosenModule = -1;
			int MinPerf = INT_MAX;
			int ChoosenPwr = 0;
			int target_perf;
#ifdef EARA_THERMAL_VPU_SUPPORT
			int new_vpu_time = 0;
#endif
#ifdef EARA_THERMAL_MDLA_SUPPORT
			int new_mdla_time = 0;
#endif

			/* B-cluster */
			if (IS_ABLE_DOWN_GEAR(CORE_LIMIT(B),
					opp[THRM_CPU_OFFSET + CLUSTER_B])) {
				MinPerf = get_perf(CLUSTER_B, CORE_LIMIT(B),
						opp[THRM_CPU_OFFSET
							+ CLUSTER_B]);
				ChoosenCl = CLUSTER_B;
				ChoosenPwr = get_delta_pwr(
						THRM_CPU_OFFSET + CLUSTER_B,
						CORE_LIMIT(B),
						opp[THRM_CPU_OFFSET
							+ CLUSTER_B]);
			}

			/* L-cluster */
			if (IS_ABLE_DOWN_GEAR(CORE_LIMIT(L),
					opp[THRM_CPU_OFFSET + CLUSTER_L])) {
				target_perf = get_perf(CLUSTER_L,
						CORE_LIMIT(L),
						opp[THRM_CPU_OFFSET
							+ CLUSTER_L]);
				if (MinPerf > target_perf) {
					MinPerf = target_perf;
					ChoosenCl = CLUSTER_L;
					ChoosenPwr = get_delta_pwr(
						THRM_CPU_OFFSET + CLUSTER_L,
						CORE_LIMIT(L),
						opp[THRM_CPU_OFFSET
							+ CLUSTER_L]);
				}
			}

			frame_time = 0;

			if (!vpu_time && !mdla_time) {
				if (ChoosenCl != -1) {
					/* change opp of cluster */
					opp[THRM_CPU_OFFSET + ChoosenCl] += 1;
					goto prepare_next_round_down;
				}
			}

			if (ChoosenCl != -1) {
				ChoosenModule = THRM_CPU;
				opp[THRM_CPU_OFFSET + ChoosenCl] += 1;
				for_each_clusters(i) {
					out_cl_cap[i] = CORE_CAP(i,
						opp[THRM_CPU_OFFSET + i]);
					EARA_THRM_LOGI(
						"cl[%d] cap %d opp %d core %d\n",
						i, out_cl_cap[i],
						opp[THRM_CPU_OFFSET + i],
						core_limit[i]);
				}
				new_cpu_time = cal_cpu_time(cpu_time, cur_cap,
					cur_sys_cap, out_cl_cap, core_limit);
				opp[THRM_CPU_OFFSET + ChoosenCl] -= 1;
				frame_time = new_cpu_time + vpu_time
						+ mdla_time;
			} else {
				new_cpu_time = cpu_time;
				frame_time = INT_MAX;
			}

#ifdef EARA_THERMAL_VPU_SUPPORT
			if (vpu_time && opp[THRM_VPU] != -1
				&& opp[THRM_VPU] < VPU_OPP_NUM - 1) {
				new_vpu_time = cal_xpu_time(vpu_time,
					vpu_dvfs_tbl.cap[vpu_opp],
					vpu_dvfs_tbl.cap[opp[THRM_VPU]+1]);
				if (frame_time >
					cpu_time + new_vpu_time + mdla_time) {
					ChoosenCl = -1;
					ChoosenModule = THRM_VPU;
					ChoosenPwr = get_delta_pwr(THRM_VPU, 1,
						opp[THRM_VPU]);
					frame_time = cpu_time + new_vpu_time
							+ mdla_time;
				}
			}
#endif

#ifdef EARA_THERMAL_MDLA_SUPPORT
			if (mdla_time && opp[THRM_MDLA] != -1
				&& opp[THRM_MDLA] < MDLA_OPP_NUM - 1) {
				new_mdla_time = cal_xpu_time(mdla_time,
						mdla_dvfs_tbl.cap[mdla_opp],
						mdla_dvfs_tbl.cap[
						opp[THRM_MDLA]+1]);
				if (frame_time > cpu_time + vpu_time +
						new_mdla_time) {
					ChoosenCl = -1;
					ChoosenModule = THRM_MDLA;
					ChoosenPwr = get_delta_pwr(THRM_MDLA,
							1, opp[THRM_MDLA]);
					frame_time = cpu_time + vpu_time
							+ new_mdla_time;
				}
			}
#endif

			if (ChoosenCl != -1) {
				opp[THRM_CPU_OFFSET + ChoosenCl] += 1;
				cpu_time = new_cpu_time;
				cur_cap = __eara_arr_max(out_cl_cap,
						g_cluster_num);
				cur_sys_cap = cal_system_capacity(out_cl_cap,
						core_limit);
				goto prepare_next_round_down;
			}
#ifdef EARA_THERMAL_AI_SUPPORT
			else if (ChoosenModule != -1) {
				opp[ChoosenModule] += 1;
#ifdef EARA_THERMAL_VPU_SUPPORT
				if (ChoosenModule == THRM_VPU) {
					vpu_time = new_vpu_time;
					vpu_opp = opp[ChoosenModule];
				}
#endif
#ifdef EARA_THERMAL_MDLA_SUPPORT
				if (ChoosenModule == THRM_MDLA) {
					mdla_time = new_mdla_time;
					mdla_opp = opp[ChoosenModule];
				}
#endif
				goto prepare_next_round_down;
			}
#endif
			EARA_THRM_LOGI("No lower OPP!\n");
			EARA_THRM_LOGI("(bgt/delta/cur)=(%d/%d/%d)\n",
				remain_budget, delta_power, curr_power);
			EARA_THRM_LOGI("(opp)=(%d,%d,%d,%d)\n",
				opp[THRM_CPU_OFFSET + CLUSTER_L],
				opp[THRM_CPU_OFFSET + CLUSTER_B],
				opp[THRM_VPU], opp[THRM_MDLA]);
			EARA_THRM_LOGI("(act/c_lmt)=(%d,%d/%d,%d)\n",
				ACT_CORE(L), ACT_CORE(B),
				CORE_LIMIT(L), CORE_LIMIT(B));

			frame_time = 0;

			if (CORE_LIMIT(L) > KEEP_L_CORE &&
				IS_ABLE_OFF_CORE(CORE_LIMIT(L),
					opp[THRM_CPU_OFFSET + CLUSTER_L])) {
				ChoosenPwr = get_delta_pwr(
						THRM_CPU_OFFSET + CLUSTER_L,
						CORE_LIMIT(L),
						PPM_COBRA_MAX_FREQ_IDX - 1);
				--CORE_LIMIT(L);
			} else if (IS_ABLE_OFF_CORE(CORE_LIMIT(B),
					opp[THRM_CPU_OFFSET + CLUSTER_B])) {
				ChoosenPwr = get_delta_pwr(
						THRM_CPU_OFFSET + CLUSTER_B,
						CORE_LIMIT(B),
						PPM_COBRA_MAX_FREQ_IDX - 1);
				--CORE_LIMIT(B);
			} else {
				EARA_THRM_LOGE("No way to lower power!\n");
				EARA_THRM_LOGI("(bgt/delta/cur)=(%d/%d/%d)\n",
					remain_budget, delta_power,
					curr_power);
				EARA_THRM_LOGI("(opp)=(%d,%d,%d,%d)\n",
					opp[THRM_CPU_OFFSET + CLUSTER_L],
					opp[THRM_CPU_OFFSET + CLUSTER_B],
					opp[THRM_VPU], opp[THRM_MDLA]);
				EARA_THRM_LOGI("(act/c_lmt)=(%d,%d/%d,%d)\n",
					ACT_CORE(L), ACT_CORE(B),
					CORE_LIMIT(L), CORE_LIMIT(B));
				break;
			}

prepare_next_round_down:
			delta_power += ChoosenPwr;
			curr_power -= ChoosenPwr;
			out_st->frame_time = frame_time;

			EARA_THRM_LOGI("[-](delta/Cl/Pwr/Prf)=(%d,%d,%d,%d)\n",
				delta_power, ChoosenCl, ChoosenPwr, MinPerf);
			EARA_THRM_LOGI("[-](opp)=(%d,%d,%d,%d)\n",
				opp[THRM_CPU_OFFSET + CLUSTER_L],
				opp[THRM_CPU_OFFSET + CLUSTER_B],
				opp[THRM_VPU], opp[THRM_MDLA]);
			EARA_THRM_LOGI("[-](act/c_lmt)=(%d,%d/%d,%d)\n",
				ACT_CORE(L), ACT_CORE(B),
				CORE_LIMIT(L), CORE_LIMIT(B));
		}
	}

	for_each_clusters(i) {
		if (opp[THRM_CPU_OFFSET + i] != cl_status[i].freq_idx)
			break;
		if (active_core[i] != core_limit[i])
			break;
	}

	if (i == g_cluster_num &&
		vpu_opp == opp[THRM_VPU] && mdla_opp == opp[THRM_MDLA]) {
		EARA_THRM_LOGI("L %s: no change\n", __func__);
		return NO_CHANGE;
	}

	for_each_clusters(i) {
		out_cl_cap[i] = CORE_CAP(i, opp[THRM_CPU_OFFSET + i]);
		EARA_THRM_LOGI("out: %d: cap %d, opp %d, core %d\n",
			i, out_cl_cap[i], opp[THRM_CPU_OFFSET + i],
			core_limit[i]);
	}
	EARA_THRM_LOGI("out: vpu %d mdla %d\n", opp[THRM_VPU], opp[THRM_MDLA]);

	return COBRA_SUCCESS;
}

static void get_cur_status(int vpu_opp, int mdla_opp, int *out_cur_sys_cap,
	int *out_cur_cap, int *out_cpu_power, int *out_vpu_power,
	int *out_mdla_power)
{
	int i;
	struct cpumask cluster_cpu, online_cpu;

	for_each_clusters(i) {
		arch_get_cluster_cpus(&cluster_cpu, i);
		cpumask_and(&online_cpu, &cluster_cpu, cpu_online_mask);

		g_cl_status[i].core_num = cpumask_weight(&online_cpu);
		if (!g_cl_status[i].core_num)
			g_cl_status[i].freq_idx = -1;
		else
			g_cl_status[i].freq_idx = ppm_main_freq_to_idx(i,
					mt_cpufreq_get_cur_phy_freq_no_lock(i),
					CPUFREQ_RELATION_L);

		g_active_core[i] = (g_cl_status[i].core_num >= 0)
			? g_cl_status[i].core_num : 0;
		g_core_limit[i] = g_active_core[i];
		g_mod_opp[THRM_CPU_OFFSET + i] = g_cl_status[i].freq_idx;
		g_cl_cap[i] = CORE_CAP(i, g_cl_status[i].freq_idx);

		EARA_THRM_LOGI("current: [%d] core %d, opp %d, cap %d\n", i,
			g_active_core[i], g_mod_opp[THRM_CPU_OFFSET + i],
			g_cl_cap[i]);
	}

	*out_cpu_power = ppm_find_pwr_idx(g_cl_status);
	if (*out_cpu_power < 0)
		*out_cpu_power = mt_ppm_thermal_get_max_power();

	*out_cur_sys_cap = cal_system_capacity(g_cl_cap, g_active_core);
	*out_cur_cap = cal_max_cap_of_core(g_cl_status);

#ifdef EARA_THERMAL_VPU_SUPPORT
	g_mod_opp[THRM_VPU] = vpu_opp;
	if (vpu_opp != -1)
		*out_vpu_power = vpu_dvfs_tbl.power[vpu_opp];
#endif

#ifdef EARA_THERMAL_MDLA_SUPPORT
	g_mod_opp[THRM_MDLA] = mdla_opp;
	if (mdla_opp != -1)
		*out_mdla_power = mdla_dvfs_tbl.power[mdla_opp];
#endif
}

void eara_thrm_pb_frame_start(int pid,
	int cpu_time, int vpu_time, int mdla_time,
	int cpu_cap, int vpu_boost, int mdla_boost,
	int queuefps, unsigned long long q2q_time,
	int vpu_cross, int mdla_cross, int vpu_bg, int mdla_bg,
	ktime_t cur_time)
{
	struct thrm_pb_render *thr = NULL;
	struct task_struct *tsk;

	mutex_lock(&thrm_lock);

	if (!is_enable || !is_throttling)
		goto exit;

	if (enable_debug_log) {
		rcu_read_lock();
		tsk = find_task_by_vpid(pid);
		if (tsk)
			get_task_struct(tsk);
		rcu_read_unlock();

		EARA_THRM_LOGI("E %s: pid %d-%s, c_time %d, cap %d\n", __func__,
			pid, (tsk)?tsk->comm:"NULL", cpu_time, cpu_cap);
		EARA_THRM_LOGI(" v_time %d, v_boost %d, v_cross %d, v_bg %d\n",
			vpu_time, vpu_boost, vpu_cross, vpu_bg);
		EARA_THRM_LOGI(" m_time %d, m_boost %d, m_cross %d, m_bg %d\n",
			mdla_time, mdla_boost, mdla_cross, mdla_bg);

		if (tsk)
			put_task_struct(tsk);
	}

	thr = thrm_pb_list_get(pid, 1);
	if (!thr) {
		EARA_THRM_LOGE("%s:NO MEM\n", __func__);
		goto exit;
	}

	thr->ts = ktime_to_ns(cur_time);
#ifdef EARA_THERMAL_VPU_SUPPORT
	thr->frame_info.vpu_time = vpu_time;
	thr->frame_info.vpu_opp = vpu_cap_to_opp(vpu_boost);
#endif
#ifdef EARA_THERMAL_MDLA_SUPPORT
	thr->frame_info.mdla_time = mdla_time;
	thr->frame_info.mdla_opp = mdla_cap_to_opp(mdla_boost);
#endif
	thr->frame_info.AI_type = update_AI_type(vpu_time, vpu_boost,
		vpu_cross, mdla_time, mdla_boost, mdla_cross);
	update_AI_bg(vpu_bg, mdla_bg);

	EARA_THRM_LOGI("pid %d, AI_type %d, bg %d, ts %llu\n",
		pid, thr->frame_info.AI_type, has_bg_AI, thr->ts);

	thr->frame_info.queue_fps = queuefps;
	thr->frame_info.q2q_time = (int)q2q_time;

	if (cpu_cap)
		thr->frame_info.cpu_time = cpu_time;
	else
		thr->frame_info.cpu_time = 0;

	thrm_pb_turn_record_locked(1);

exit:
	mutex_unlock(&thrm_lock);
}
EXPORT_SYMBOL(eara_thrm_pb_frame_start);

static inline int __scale_gpu_opp(int init_opp, int base, int target)
{
	unsigned long long ratio, freq;

	ratio = ((unsigned long long)base) * 1000;
	do_div(ratio, target);

	freq = ratio * thr_gpu_tbl[init_opp].gpufreq_khz;
	do_div(freq, 1000);

	return gpu_freq_to_opp((unsigned int)freq);
}

static int gpu_opp_cand_high(int init_opp, int gpu_time, int cpu_time)
{
	if (!cpu_time || !gpu_time)
		return 0;

	if (gpu_time < cpu_time)
		return init_opp - 1;

	return __scale_gpu_opp(init_opp, gpu_time, cpu_time);
}

static int gpu_opp_cand_low(int init_opp, int gpu_time, int cpu_time)
{
	if (!cpu_time || !gpu_time)
		return g_gpu_opp_num;

	if (gpu_time > cpu_time)
		return init_opp;

	return __scale_gpu_opp(init_opp, gpu_time, cpu_time);
}

static int is_limit_in_range(int cpu_limit, int gpu_limit,
			int vpu_limit, int mdla_limit)
{
	int min_cpu, min_gpu;
#ifdef EARA_THERMAL_VPU_SUPPORT
	int min_vpu;
#endif
#ifdef EARA_THERMAL_MDLA_SUPPORT
	int min_mdla;
#endif

	min_cpu = apthermolmt_get_cpu_min_power();
	min_gpu = apthermolmt_get_gpu_min_power();

	if (cpu_limit <=
		thr_cobra_tbl->basic_pwr_tbl[0][CPU_OPP_NUM - 1].power_idx
		|| cpu_limit <= min_cpu) {
		EARA_THRM_LOGE("CPU limit too low (%d, %d)\n",
			cpu_limit, min_cpu);
		return 0;
	}

	if (gpu_limit <= min_gpu) {
		EARA_THRM_LOGE("GPU limit too low (%d, %d)\n",
			gpu_limit, min_gpu);
		return 0;
	}

#ifdef EARA_THERMAL_VPU_SUPPORT
	/* when vpu is not on, limit is always equal to min power */
	min_vpu = apthermolmt_get_vpu_min_power();

	if (vpu_limit < min_vpu) {
		EARA_THRM_LOGE("VPU limit too low (%d, %d)\n",
			vpu_limit, min_vpu);
		return 0;
	}
#endif

#ifdef EARA_THERMAL_MDLA_SUPPORT
	min_mdla = apthermolmt_get_mdla_min_power();

	if (mdla_limit < min_mdla) {
		EARA_THRM_LOGE("MDLA limit too low (%d, %d)\n",
			mdla_limit, min_mdla);
		return 0;
	}
#endif

	return 1;

}

void eara_thrm_pb_enqueue_end(int pid, int gpu_time,
			int gpu_freq, unsigned long long enq)
{
	struct thrm_pb_render *thr = NULL;
	int i, j;
	int remain_budget;
	int new_gpu_time;
	int best_ratio = INT_MAX;
	int best_gpu_opp = INIT_UNSET;
	int diff;
	int curr_cpu_power = 0;
	int curr_vpu_power = 0;
	int curr_mdla_power = 0;
	struct task_struct *tsk;
	int cur_sys_cap;
	int cur_cap;
	int cpu_time, vpu_time = 0, mdla_time = 0;
	int vpu_opp = -1, mdla_opp = -1;
	int AI_type = 0;
	int queuefps;
	unsigned long long q2q_time;
	unsigned int cur_gpu_freq;
	int cur_gpu_opp, start_gpu_opp, end_gpu_opp;
	int AI_check;
#ifdef EARA_THERMAL_VPU_SUPPORT
	int temp_vpu_power = 0;
#endif
#ifdef EARA_THERMAL_MDLA_SUPPORT
	int temp_mdla_power = 0;
#endif

	mutex_lock(&thrm_lock);

	if (!is_enable || !is_throttling)
		goto exit;

	if (enable_debug_log) {
		rcu_read_lock();
		tsk = find_task_by_vpid(pid);
		if (tsk)
			get_task_struct(tsk);
		rcu_read_unlock();

		EARA_THRM_LOGI("E %s: pid %d-%s, g_t %d, g_f %d, enq %llu\n",
			__func__, pid, (tsk)?tsk->comm:"NULL",
			gpu_time, gpu_freq, enq);

		if (tsk)
			put_task_struct(tsk);
	}

	if (!is_AI_controllable()) {
		EARA_THRM_LOGE("%s:AI NOT under control\n", __func__);
		set_major_pair_locked(0, 0);
		thrm_pb_turn_cont_locked(0);
		goto exit;
	}

	if (!pid || !gpu_time || !gpu_freq || gpu_time == -1
		|| gpu_time > TOO_LONG_TIME || gpu_time < TOO_SHORT_TIME) {
		EARA_THRM_LOGE("%s:gpu_time %d, gpu_freq %d\n",
				__func__, gpu_time, gpu_freq);
		if (pid == cur_max_pid && pid) {
			set_major_pair_locked(0, 0);
			thrm_pb_turn_cont_locked(0);
		}
		goto exit;
	}

	thr = thrm_pb_list_get(pid, 0);
	if (!thr)
		goto exit;

	cpu_time = thr->frame_info.cpu_time;
	queuefps = thr->frame_info.queue_fps;
	q2q_time = thr->frame_info.q2q_time;
	thr->frame_info.gpu_time = gpu_time;

#ifdef EARA_THERMAL_VPU_SUPPORT
	vpu_time = thr->frame_info.vpu_time;
	vpu_opp = thr->frame_info.vpu_opp;
#endif
#ifdef EARA_THERMAL_MDLA_SUPPORT
	mdla_time = thr->frame_info.mdla_time;
	mdla_opp = thr->frame_info.mdla_opp;
#endif
	AI_type = thr->frame_info.AI_type;
	AI_check = AI_info_error_check(AI_type, vpu_time, mdla_time,
				vpu_opp, mdla_opp);

	if (!cpu_time || cpu_time > TOO_LONG_TIME
		|| cpu_time < TOO_SHORT_TIME
		|| AI_check) {
		EARA_THRM_LOGE("%s:cpu_time %d, AI_check -%d\n",
			__func__, cpu_time, AI_check);
		if (pid == cur_max_pid && pid) {
			set_major_pair_locked(0, 0);
			thrm_pb_turn_cont_locked(0);
		}
		goto exit;
	}

	if (!need_reallocate(thr, pid) || thr->frame_info.bypass)
		goto exit;

	get_cobra_tbl();
	if (!thr_cobra_tbl) {
		EARA_THRM_LOGE("%s: NULL cobra table\n", __func__);
		goto exit;
	}

	if (!thr_gpu_tbl || !g_opp_ratio) {
		EARA_THRM_LOGE("%s: NULL gpu table\n", __func__);
		goto exit;
	}

	get_cur_status(vpu_opp, mdla_opp, &cur_sys_cap, &cur_cap,
			&curr_cpu_power, &curr_vpu_power, &curr_mdla_power);
	cur_gpu_freq = mt_gpufreq_get_cur_freq();
	cur_gpu_opp = gpu_freq_to_opp(cur_gpu_freq);
	EARA_THRM_LOGI("current: power %d, syscap %d, cap %d, gpuF %d, pb %d\n",
		curr_cpu_power, cur_sys_cap, cur_cap,
		cur_gpu_freq, g_total_pb);

	start_gpu_opp = gpu_opp_cand_high(cur_gpu_opp, gpu_time, cpu_time);
	start_gpu_opp = clamp(start_gpu_opp, 0, g_gpu_opp_num - 1);
	end_gpu_opp = gpu_opp_cand_low(cur_gpu_opp, gpu_time, cpu_time);
	end_gpu_opp = clamp(end_gpu_opp, 0, g_gpu_opp_num - 1);
	if (start_gpu_opp > end_gpu_opp) {
		EARA_THRM_LOGE("cur %d gtime %d ctime %d, start %d end %d\n",
			cur_gpu_opp, gpu_time, cpu_time,
			start_gpu_opp, end_gpu_opp);
		end_gpu_opp = start_gpu_opp;
	}

	if (is_AI_on) {
		if (AI_type == NO_AI || !HAS_AI_PERFRAME(AI_type)) {
#ifdef EARA_THERMAL_VPU_SUPPORT
			if (is_VPU_on) {
				temp_vpu_power =
					vpu_dvfs_tbl.power[VPU_OPP_NUM - 1];
			}
#endif
#ifdef EARA_THERMAL_MDLA_SUPPORT
			if (is_MDLA_on) {
				temp_mdla_power =
					mdla_dvfs_tbl.power[MDLA_OPP_NUM - 1];
			}
#endif
		}
	} else {
#ifdef EARA_THERMAL_VPU_SUPPORT
		temp_vpu_power = min_vpu_power;
#endif
#ifdef EARA_THERMAL_MDLA_SUPPORT
		temp_mdla_power = min_mdla_power;
#endif
	}

	memset(g_opp_ratio, 0, g_gpu_opp_num * sizeof(struct thrm_pb_ratio));
	for (i = start_gpu_opp; i <= end_gpu_opp; i++) {
		unsigned long long temp;
		int realloc_ret;
		struct thrm_pb_realloc realloc_st = {0};

		remain_budget = g_total_pb - thr_gpu_tbl[i].gpufreq_power - 1;
		if (remain_budget <= 0)
			continue;

		new_gpu_time = cal_xpu_time(gpu_time, gpu_freq,
					thr_gpu_tbl[i].gpufreq_khz);
		if (!new_gpu_time) {
			EARA_THRM_LOGE("gtime %d gfreq %d, [%d]gpufreq %d\n",
				gpu_time, gpu_freq, i,
				thr_gpu_tbl[i].gpufreq_khz);
			continue;
		}

		realloc_ret = reallocate_perf_first(remain_budget,
				g_cl_status, g_active_core, curr_cpu_power,
				g_mod_opp, g_core_limit, g_cl_cap,
				cpu_time, cur_cap, cur_sys_cap,
				vpu_time, vpu_opp,
				mdla_time, mdla_opp, &realloc_st);

		if (realloc_ret == NO_CHANGE)
			realloc_st.frame_time = cpu_time + vpu_time + mdla_time;

		if (!realloc_st.frame_time)
			realloc_st.frame_time = cal_cpu_time(cpu_time, cur_cap,
			cur_sys_cap, g_cl_cap, g_core_limit);

		temp = (long long)(realloc_st.frame_time) * 100;
		do_div(temp, new_gpu_time);
		g_opp_ratio[i].ratio = (int)temp;
#ifdef EARA_THERMAL_VPU_SUPPORT
		if (g_mod_opp[THRM_VPU] != -1)
			g_opp_ratio[i].vpu_power =
			vpu_dvfs_tbl.power[g_mod_opp[THRM_VPU]];
		else
			g_opp_ratio[i].vpu_power = temp_vpu_power;
#else
		g_opp_ratio[i].vpu_power = 0;
#endif
#ifdef EARA_THERMAL_MDLA_SUPPORT
		if (g_mod_opp[THRM_MDLA] != -1)
			g_opp_ratio[i].mdla_power =
			mdla_dvfs_tbl.power[g_mod_opp[THRM_MDLA]];
		else
			g_opp_ratio[i].mdla_power = temp_mdla_power;
#else
		g_opp_ratio[i].mdla_power = 0;
#endif
		EARA_THRM_LOGI("[%d] Gt %d, Ft %d, R %d, vpu_p %d, m_p %d\n",
		i, new_gpu_time, realloc_st.frame_time, g_opp_ratio[i].ratio,
		g_opp_ratio[i].vpu_power, g_opp_ratio[i].mdla_power);

		if (realloc_ret != NO_CHANGE) {
			/* reset for next round */
			for_each_clusters(j) {
				g_mod_opp[THRM_CPU_OFFSET + j] =
					g_cl_status[j].freq_idx;
				g_core_limit[j] = g_active_core[j];
			}
			g_mod_opp[THRM_VPU] = vpu_opp;
			g_mod_opp[THRM_MDLA] = mdla_opp;
		}

		if (!g_opp_ratio[i].ratio)
			continue;
		diff = DIFF_ABS(g_opp_ratio[i].ratio, 100);
		if (diff < best_ratio) {
			best_ratio = diff;
			best_gpu_opp = i;
		}

		if (g_opp_ratio[i].ratio <= 100)
			break;
	}

	if (best_gpu_opp != INIT_UNSET
		&& best_gpu_opp >= 0
		&& best_gpu_opp < g_gpu_opp_num) {

		int new_gpu_power = thr_gpu_tbl[best_gpu_opp].gpufreq_power + 1;
		int new_cpu_power = g_total_pb
				- new_gpu_power
				- g_opp_ratio[best_gpu_opp].vpu_power
				- g_opp_ratio[best_gpu_opp].mdla_power;

		if (!is_limit_in_range(new_cpu_power, new_gpu_power,
					g_opp_ratio[best_gpu_opp].vpu_power,
					g_opp_ratio[best_gpu_opp].mdla_power))
			goto CAN_NOT_CONTROL;

		if (stable_count < STABLE_TH) {
			stable_count++;
			goto CAN_NOT_CONTROL;
		}

		thrm_pb_turn_cont_locked(1);

		if (new_cpu_power > g_max_cpu_power && g_max_cpu_power) {
			int diff = new_cpu_power - g_max_cpu_power;

			new_cpu_power = g_max_cpu_power;
			new_gpu_power += diff;
		}

		eara_thrm_set_power_limit(new_cpu_power,
				new_gpu_power,
				g_opp_ratio[best_gpu_opp].vpu_power,
				g_opp_ratio[best_gpu_opp].mdla_power);

		EARA_THRM_LOGI("max_gpu_opp %d, max_gpu_freq %d\n",
			best_gpu_opp, thr_gpu_tbl[best_gpu_opp].gpufreq_khz);

		goto exit;
	}

CAN_NOT_CONTROL:
	EARA_THRM_LOGI("CAN NOT CONTROL %d\n", stable_count);
	thrm_pb_turn_cont_locked(0);

exit:
	mutex_unlock(&thrm_lock);
}
EXPORT_SYMBOL(eara_thrm_pb_enqueue_end);

void eara_thrm_pb_gblock_bypass(int pid, int bypass)
{
	struct thrm_pb_render *thr = NULL;

	mutex_lock(&thrm_lock);

	if (!is_enable || !is_throttling)
		goto exit;

	EARA_THRM_LOGI("pid %d(cur %d), bypass %d\n",
			pid, cur_max_pid, bypass);

	if (!pid || cur_max_pid != pid)
		goto exit;

	thr = thrm_pb_list_get(pid, 0);
	if (!thr)
		goto exit;

	if (bypass) {
		if (thr->frame_info.bypass_cnt >= BYPASS_TH)
			thr->frame_info.bypass = 1;
		else
			thr->frame_info.bypass_cnt++;
	} else {
		if (thr->frame_info.bypass_cnt > 0)
			thr->frame_info.bypass_cnt--;

		if (thr->frame_info.bypass &&
				!thr->frame_info.bypass_cnt)
			thr->frame_info.bypass = 0;
	}

	EARA_THRM_LOGI("pid %d->bypass %d, cnt %d\n", pid,
		thr->frame_info.bypass, thr->frame_info.bypass_cnt);
exit:
	mutex_unlock(&thrm_lock);
}
EXPORT_SYMBOL(eara_thrm_pb_gblock_bypass);

int mtk_eara_thermal_pb_handle(int total_pwr_budget, int max_cpu_power,
	int max_gpu_power, int max_vpu_power, int max_mdla_power)
{
	int ret = 0;
	int throttling = 0;
	int compare_pb;

	mutex_lock(&thrm_lock);

	if (!is_enable)
		goto exit;

	g_total_pb = total_pwr_budget;
	g_max_cpu_power = max_cpu_power;
	if (total_pwr_budget != 0) {
		compare_pb = max_cpu_power + max_gpu_power;
		if (max_vpu_power != -1)
			compare_pb += max_vpu_power;
		if (max_mdla_power != -1)
			compare_pb += max_mdla_power;

		if (total_pwr_budget < compare_pb)
			throttling = 1;
	}

	if (fake_throttle) {
		throttling = 1;
		g_total_pb = fake_pb;
	}

	thrm_pb_turn_throttling_locked(throttling);

	ret = is_controllable;

	EARA_THRM_LOGI("%s: total %d, (%d, %d, %d, %d), ret %d, notified %d\n",
		__func__, total_pwr_budget, max_cpu_power, max_gpu_power,
		max_vpu_power, max_mdla_power, ret, notified_clear);

	/* contiguous turn off control */
	if (!ret) {
		if (notified_clear || !is_throttling) {
			pass_perf_first_hint(0);
			eara_thrm_clear_limit();
		}
		notified_clear = 1;
	} else
		notified_clear = 0;

exit:
	mutex_unlock(&thrm_lock);

	return ret;
}
EXPORT_SYMBOL(mtk_eara_thermal_pb_handle);


#define EARA_THRM_DEBUGFS_ENTRY(name) \
static int eara_thrm_##name##_open(struct inode *i, struct file *file) \
{ \
	return single_open(file, eara_thrm_##name##_show, i->i_private); \
} \
\
static const struct file_operations eara_thrm_##name##_fops = { \
	.owner = THIS_MODULE, \
	.open = eara_thrm_##name##_open, \
	.read = seq_read, \
	.write = eara_thrm_##name##_write, \
	.llseek = seq_lseek, \
	.release = single_release, \
}

static int eara_thrm_info_show(struct seq_file *m, void *unused)
{
	struct thrm_pb_render *thr = NULL;

	mutex_lock(&thrm_lock);

	thr = thrm_pb_list_get(cur_max_pid, 0);

	seq_printf(m, "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d\n",
		g_total_pb,
		cur_cpu_pb,
		cur_gpu_pb,
		(thr)?thr->frame_info.cpu_time:0,
		(thr)?thr->frame_info.gpu_time:0,
		(thr)?thr->frame_info.q2q_time:0,
		(thr)?thr->frame_info.queue_fps:0,
		cur_max_pid,
		is_perf_first,
		is_controllable,
		(thr)?thr->frame_info.AI_type:0,
		is_VPU_on,
		is_MDLA_on,
#ifdef EARA_THERMAL_VPU_SUPPORT
		cur_vpu_pb, (thr)?thr->frame_info.vpu_time:0,
#else
		0, 0,
#endif
#ifdef EARA_THERMAL_MDLA_SUPPORT
		cur_mdla_pb, (thr)?thr->frame_info.mdla_time:0
#else
		0, 0
#endif
	);
	mutex_unlock(&thrm_lock);

	return 0;
}

static ssize_t eara_thrm_info_write(struct file *flip,
			const char *ubuf, size_t cnt, loff_t *data)
{
	return 0;
}

EARA_THRM_DEBUGFS_ENTRY(info);

static int eara_thrm_status_show(struct seq_file *m, void *unused)
{
	struct rb_node *n;
	struct thrm_pb_render *pos;
	int i;

	mutex_lock(&thrm_lock);
	seq_printf(m, "is_enable %d\n", is_enable);
	seq_printf(m, "is_throttling %d\n", is_throttling);
	seq_printf(m, "has_record %d\n", has_record);
	seq_printf(m, "is_controllable %d\n", is_controllable);
	seq_printf(m, "is_timer_active %d\n", is_timer_active);
	seq_printf(m, "g_total_pb %d\n", g_total_pb);
	seq_printf(m, "cur_cpu_pb %d\n", cur_cpu_pb);
	seq_printf(m, "cur_gpu_pb %d\n", cur_gpu_pb);
#ifdef EARA_THERMAL_VPU_SUPPORT
	seq_printf(m, "cur_vpu_pb %d\n", cur_vpu_pb);
#endif
#ifdef EARA_THERMAL_MDLA_SUPPORT
	seq_printf(m, "cur_mdla_pb %d\n", cur_mdla_pb);
#endif
	seq_printf(m, "cur_max_pid %d\n", cur_max_pid);
	seq_printf(m, "cur_max_time %d\n", cur_max_time);
	seq_printf(m, "is_perf_first %d\n", is_perf_first);

	for (n = rb_first(&render_list); n; n = rb_next(n)) {
		pos = rb_entry(n, struct thrm_pb_render, entry);
		seq_printf(m, "pid:%d\n", pos->pid);
	}

	for_each_clusters(i)
		seq_printf(m, "cluster %d: core num %d, freq %d\n",
		i, g_cl_status[i].core_num, g_cl_status[i].freq_idx);

	mutex_unlock(&thrm_lock);

	return 0;
}

static ssize_t eara_thrm_status_write(struct file *flip,
			const char *ubuf, size_t cnt, loff_t *data)
{
	return 0;
}

EARA_THRM_DEBUGFS_ENTRY(status);

static int eara_thrm_table_show(struct seq_file *m, void *unused)
{
	int i, j;

	mutex_lock(&thrm_lock);

	seq_printf(m, "#CPU cluster: %d\n", g_cluster_num);
	seq_printf(m, "#module: %d\n", g_modules_num);
	seq_printf(m, "CPU_OFFSET: %d\n", THRM_CPU_OFFSET);
	seq_printf(m, "GPU max_opp %d\n", g_max_gpu_opp_idx);
	seq_printf(m, "GPU num_opp %d\n", mt_gpufreq_get_dvfs_table_num());

	get_cobra_tbl();
	eara_thrm_update_gpu_info(&g_gpu_opp_num, &g_max_gpu_opp_idx,
			&thr_gpu_tbl, &g_opp_ratio);
	if (!thr_cobra_tbl || !thr_gpu_tbl) {
		mutex_unlock(&thrm_lock);
		return 0;
	}

	seq_puts(m, "\nCPU(power, perf)==================\n");
	for (i = 0; i < CPU_CORE_NUM; i++) {
		for (j = 0; j < CPU_OPP_NUM; j++) {
			seq_printf(m, "(%2d, %2d) = (%4d, %4d)\n", i, j,
				thr_cobra_tbl->basic_pwr_tbl[i][j].power_idx,
				thr_cobra_tbl->basic_pwr_tbl[i][j].perf_idx);
		}
	}

	seq_puts(m, "\nCPU(freq, cap)==================\n");
	for (i = 0; i < g_cluster_num; i++) {
		for (j = 0; j < CPU_OPP_NUM; j++) {
			seq_printf(m, "[%d][%2d] (%7d, %3d)\n",
					i, j,
					cpu_dvfs_tbl[i].power[j],
					cpu_dvfs_tbl[i].capacity_ratio[j]);
		}
	}

	seq_puts(m, "\nGPU(freq, power)==================\n");
	for (i = 0; i < g_gpu_opp_num; i++) {
		seq_printf(m, "[%2d] = (%d, %d)\n", i,
			thr_gpu_tbl[i].gpufreq_khz,
			thr_gpu_tbl[i].gpufreq_power);
	}

#ifdef EARA_THERMAL_VPU_SUPPORT
	seq_puts(m, "\nVPU(freq, power, cap)==================\n");
	for (i = 0; i < VPU_OPP_NUM; i++) {
		seq_printf(m, "[%2d] = (%d, %d, %d)\n", i,
			vpu_dvfs_tbl.freq[i],
			vpu_dvfs_tbl.power[i],
			vpu_dvfs_tbl.cap[i]);
	}
#endif

#ifdef EARA_THERMAL_MDLA_SUPPORT
	seq_puts(m, "\nMDLA(freq, power, cap)==================\n");
	for (i = 0; i < MDLA_OPP_NUM; i++) {
		seq_printf(m, "[%2d] = (%d, %d, %d)\n", i,
			mdla_dvfs_tbl.freq[i],
			mdla_dvfs_tbl.power[i],
			mdla_dvfs_tbl.cap[i]);
	}
#endif

	mutex_unlock(&thrm_lock);

	return 0;
}

static ssize_t eara_thrm_table_write(struct file *flip,
			const char *ubuf, size_t cnt, loff_t *data)
{
	return 0;
}

EARA_THRM_DEBUGFS_ENTRY(table);

static int eara_thrm_enable_show(struct seq_file *m, void *unused)
{
	mutex_lock(&thrm_lock);
	seq_printf(m, "is_enable %d\n", is_enable);
	mutex_unlock(&thrm_lock);

	return 0;
}

static ssize_t eara_thrm_enable_write(struct file *flip,
			const char *ubuf, size_t cnt, loff_t *data)
{
	int val;
	int ret;

	ret = kstrtoint_from_user(ubuf, cnt, 0, &val);
	if (ret)
		return ret;

	if (val > 1 || val < 0)
		return cnt;

	mutex_lock(&thrm_lock);
	is_enable = val;

	if (!is_enable)
		thrm_pb_turn_throttling_locked(0);

	mutex_unlock(&thrm_lock);

	return cnt;
}

EARA_THRM_DEBUGFS_ENTRY(enable);

static int eara_thrm_enable_log_show(struct seq_file *m, void *unused)
{
	mutex_lock(&thrm_lock);
	seq_printf(m, "debug log %s\n", enable_debug_log?"ON":"OFF");
	mutex_unlock(&thrm_lock);
	return 0;
}

static ssize_t eara_thrm_enable_log_write(struct file *flip,
			const char *ubuf, size_t cnt, loff_t *data)
{
	int val;
	int ret;

	ret = kstrtoint_from_user(ubuf, cnt, 0, &val);
	if (ret)
		return ret;

	if (val > 1 || val < 0)
		return cnt;

	mutex_lock(&thrm_lock);
	enable_debug_log = val;
	mutex_unlock(&thrm_lock);

	return cnt;
}
EARA_THRM_DEBUGFS_ENTRY(enable_log);

static int eara_thrm_fake_th_show(struct seq_file *m, void *unused)
{
	mutex_lock(&thrm_lock);
	seq_printf(m, "fake_throttle %s\n", fake_throttle?"ON":"OFF");
	mutex_unlock(&thrm_lock);
	return 0;
}

static ssize_t eara_thrm_fake_th_write(struct file *flip,
			const char *ubuf, size_t cnt, loff_t *data)
{
	int val;
	int ret;

	ret = kstrtoint_from_user(ubuf, cnt, 0, &val);
	if (ret)
		return ret;

	mutex_lock(&thrm_lock);
	if (val) {
		fake_throttle = 1;
		fake_pb = val;
	} else {
		fake_throttle = 0;
		fake_pb = 0;
		thrm_pb_turn_throttling_locked(0);
		eara_thrm_clear_limit();
	}
	mutex_unlock(&thrm_lock);

	mtk_eara_thermal_pb_handle(fake_pb, 3000, 0, 0, 0);

	return cnt;
}
EARA_THRM_DEBUGFS_ENTRY(fake_th);

static int ut_opp[5];
static int ut_core[2];
static int ut_cap[2];
static int eara_thrm_test_show(struct seq_file *m, void *unused)
{
	int i;

	for_each_clusters(i) {
		seq_printf(m, "out: %d: opp %d, core %d, cap %d\n", i,
			ut_opp[THRM_CPU_OFFSET + i], ut_core[i], ut_cap[i]);
	}
	seq_printf(m, "out: vpu %d\n", ut_opp[THRM_VPU]);
	seq_printf(m, "out: mdla %d\n", ut_opp[THRM_MDLA]);

	return 0;
}

static ssize_t eara_thrm_test_write(struct file *flip, const char *ubuf,
			size_t cnt, loff_t *data)
{
	char ut_str[1024];
	int ret = -EINVAL;
	int i;

	if (cnt > sizeof(ut_str))
		return ret;

	memset(ut_str, 0, sizeof(ut_str));
	if (copy_from_user(ut_str, ubuf, cnt)) {
		ret = -EFAULT;
		return ret;
	}
	if (cnt > 1023)
		cnt = 1023;
	ut_str[cnt] = '\0';

	for_each_clusters(i) {
		ut_opp[THRM_CPU_OFFSET + i] = 0;
		ut_core[i] = 0;
		ut_cap[i] = 0;
	}
	ut_opp[THRM_GPU] = -1;
	ut_opp[THRM_VPU] = -1;
	ut_opp[THRM_MDLA] = -1;

	get_cobra_tbl();
	eara_thrm_update_gpu_info(&g_gpu_opp_num, &g_max_gpu_opp_idx,
			&thr_gpu_tbl, &g_opp_ratio);
	if (!thr_cobra_tbl || !thr_gpu_tbl)
		return -EAGAIN;

	{
		int budget;
		struct ppm_cluster_status cls_sts[2];
		int act_core[2];
		int cur_power;

		int cpu_time;
		int tmp_cap[2];
		int sys_cap;
		int cap;
		struct thrm_pb_realloc out_st;
		int test_vpu_time = 0, test_mdla_time = 0;
		int test_vpu_opp = 0, test_mdla_opp = 0;

		if (sscanf(ut_str, "%d, %d %d %d %d, %d, %d, %d %d, %d %d",
			&budget,
			&cls_sts[0].freq_idx, &act_core[0],
			&cls_sts[1].freq_idx, &act_core[1],
			&cur_power,
			&cpu_time, &test_vpu_time, &test_mdla_time,
			&test_vpu_opp, &test_mdla_opp) != 11) {
			return ret;
		}
		for_each_clusters(i) {
			tmp_cap[i] = CORE_CAP(i, cls_sts[i].freq_idx);
			ut_opp[THRM_CPU_OFFSET + i] = cls_sts[i].freq_idx;
			ut_core[i] = act_core[i];
		}
		ut_opp[THRM_VPU] = test_vpu_opp;
		ut_opp[THRM_MDLA] = test_mdla_opp;
		sys_cap = cal_system_capacity(tmp_cap, act_core);
		cap = cal_max_cap_of_core(cls_sts);

		mutex_lock(&thrm_lock);
		reallocate_perf_first(budget, cls_sts, act_core, cur_power,
			ut_opp, ut_core, ut_cap, cpu_time, cap, sys_cap,
			test_vpu_time, test_vpu_opp,
			test_mdla_time, test_mdla_opp,
			&out_st);
		mutex_unlock(&thrm_lock);
	}
	return cnt;
}
EARA_THRM_DEBUGFS_ENTRY(test);

static void update_cpu_info(void)
{
	int cluster, opp;
	unsigned long long temp = 0ULL;
	unsigned int temp2;

	g_cluster_num = arch_get_nr_clusters();

	cpu_dvfs_tbl =
		kcalloc(g_cluster_num, sizeof(struct cpu_dvfs_info),
			GFP_KERNEL);
	if (!cpu_dvfs_tbl)
		return;

	for (cluster = 0; cluster < g_cluster_num ; cluster++) {
		struct cpumask cluster_cpus;
		int cpu;
		const struct sched_group_energy *core_energy = NULL;

		arch_get_cluster_cpus(&cluster_cpus, cluster);
		for_each_cpu(cpu, &cluster_cpus) {
			core_energy = cpu_core_energy(cpu);
			break; /* only needs once for each cluster */
		}

		if (!core_energy)
			break;

		for (opp = 0; opp < CPU_OPP_NUM; opp++) {
			cpu_dvfs_tbl[cluster].power[opp] =
				mt_cpufreq_get_freq_by_idx(cluster, opp);
			temp = core_energy->cap_states[opp].cap * 100;
			do_div(temp, 1024);
			temp2 = (unsigned int)temp;
			temp2 = clamp(temp2, 1U, 100U);
			cpu_dvfs_tbl[cluster].capacity_ratio[
				CPU_OPP_NUM - 1 - opp] = temp2;
		}
	}

	get_cobra_tbl();
}

static void update_vpu_info(void)
{
#ifdef EARA_THERMAL_VPU_SUPPORT
	int opp, i;

	for (opp = 0; opp < VPU_OPP_NUM; opp++) {
		vpu_dvfs_tbl.power[opp] = vpu_power_table[opp].power;
		vpu_dvfs_tbl.freq[opp] = get_vpu_opp_to_freq(opp);
		vpu_dvfs_tbl.cap[opp] =
			get_vpu_opp_to_freq(opp) * 100 /
			get_vpu_opp_to_freq(0);
	}
#ifdef CONFIG_MTK_PERF_OBSERVER
	vpu_num = eara_thrm_get_vpu_core_num();
#endif
	if (vpu_num) {
		g_vpu_opp = kcalloc(vpu_num, sizeof(int), GFP_KERNEL);
		for (i = 0; i < vpu_num; i++)
			g_vpu_opp[i] = -1;
	}
	min_vpu_power = vpu_dvfs_tbl.power[VPU_OPP_NUM - 1];
#endif
}

static void update_mdla_info(void)
{
#ifdef EARA_THERMAL_MDLA_SUPPORT
	int opp, i;

	for (opp = 0; opp < MDLA_OPP_NUM; opp++) {
		mdla_dvfs_tbl.power[opp] = mdla_power_table[opp].power;
		mdla_dvfs_tbl.freq[opp] = get_mdla_opp_to_freq(opp);
		mdla_dvfs_tbl.cap[opp] =
			get_mdla_opp_to_freq(opp) * 100 /
			get_mdla_opp_to_freq(0);
	}
#ifdef CONFIG_MTK_PERF_OBSERVER
	mdla_num = eara_thrm_get_mdla_core_num();
#endif
	if (mdla_num) {
		g_mdla_opp = kcalloc(mdla_num, sizeof(int), GFP_KERNEL);
		for (i = 0; i < mdla_num; i++)
			g_mdla_opp[i] = -1;
	}
	min_mdla_power = mdla_dvfs_tbl.power[MDLA_OPP_NUM - 1];
#endif
}

static void get_power_tbl(void)
{
	update_cpu_info();
	eara_thrm_update_gpu_info(&g_gpu_opp_num, &g_max_gpu_opp_idx,
			&thr_gpu_tbl, &g_opp_ratio);
	update_vpu_info();
	update_mdla_info();

	g_modules_num = g_cluster_num + THRM_CPU_OFFSET;
}

static void prepare_mem(void)
{
	g_cl_cap = kcalloc(g_cluster_num, sizeof(int), GFP_KERNEL);
	g_cl_status = kcalloc(g_cluster_num, sizeof(struct ppm_cluster_status),
				GFP_KERNEL);
	g_active_core = kcalloc(g_cluster_num, sizeof(int), GFP_KERNEL);
	g_core_limit = kcalloc(g_cluster_num, sizeof(int), GFP_KERNEL);
	g_mod_opp = kcalloc(g_modules_num, sizeof(int), GFP_KERNEL);
}

static void __exit eara_thrm_pb_exit(void)
{
	apthermolmt_unregister_user(&ap_eara);
	is_enable = 0;
	is_throttling = 0;
	thrm_pb_turn_record_locked(0);
	g_total_pb = 0;
	eara_thrm_clear_limit();

	kfree(cpu_dvfs_tbl);
	kfree(thr_cobra_tbl);
	kfree(thr_gpu_tbl);
#ifdef EARA_THERMAL_VPU_SUPPORT
	kfree(g_vpu_opp);
#endif
#ifdef EARA_THERMAL_MDLA_SUPPORT
	kfree(g_mdla_opp);
#endif

	kfree(g_opp_ratio);
	kfree(g_cl_cap);
	kfree(g_cl_status);
	kfree(g_active_core);
	kfree(g_core_limit);
	kfree(g_mod_opp);
}

static int __init eara_thrm_pb_init(void)
{
	render_list = RB_ROOT;

	eara_thrm_frame_start_fp = eara_thrm_pb_frame_start;
	eara_thrm_enqueue_end_fp = eara_thrm_pb_enqueue_end;
	eara_thrm_gblock_bypass_fp = eara_thrm_pb_gblock_bypass;

	get_power_tbl();
	prepare_mem();

	g_timer.function = timer_func;
	g_timer.data = 0UL;
	init_timer_deferrable(&g_timer);

	wq = create_singlethread_workqueue("eara_thrm");

	mark_addr = kallsyms_lookup_name("tracing_mark_write");

	is_enable = 1;

	apthermolmt_register_user(&ap_eara, ap_eara_log);
#ifdef CONFIG_MTK_PERF_OBSERVER
	pob_xpufreq_register_client(&eara_thrm_xpufreq_notifier);
#endif
	eara_thrm_debugfs_dir = debugfs_create_dir("eara_thermal", NULL);
	if (!eara_thrm_debugfs_dir)
		return -ENODEV;

	debugfs_create_file("status",
		0664,
		eara_thrm_debugfs_dir,
		NULL,
		&eara_thrm_status_fops);
	debugfs_create_file("enable",
		0664,
		eara_thrm_debugfs_dir,
		NULL,
		&eara_thrm_enable_fops);
	debugfs_create_file("table",
		0664,
		eara_thrm_debugfs_dir,
		NULL,
		&eara_thrm_table_fops);
	debugfs_create_file("debug_log",
		0664,
		eara_thrm_debugfs_dir,
		NULL,
		&eara_thrm_enable_log_fops);
	debugfs_create_file("info",
		0664,
		eara_thrm_debugfs_dir,
		NULL,
		&eara_thrm_info_fops);
	debugfs_create_file("fake_throttle",
		0664,
		eara_thrm_debugfs_dir,
		NULL,
		&eara_thrm_fake_th_fops);
	debugfs_create_file("unit_test",
		0664,
		eara_thrm_debugfs_dir,
		NULL,
		&eara_thrm_test_fops);

	return 0;
}

module_init(eara_thrm_pb_init);
module_exit(eara_thrm_pb_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("MediaTek EARA-QoS");
MODULE_AUTHOR("MediaTek Inc.");
