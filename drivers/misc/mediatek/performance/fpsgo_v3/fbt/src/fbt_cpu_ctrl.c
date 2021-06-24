// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#define pr_fmt(fmt) "[fbt_cpu_ctrl]"fmt

#define DEBUG_LOG	0

#include <linux/slab.h>
#include <linux/string.h>
#include <linux/notifier.h>
#include <linux/cpufreq.h>
#include <linux/pm_qos.h>
#include <linux/module.h>
#include <linux/cpufreq.h>

#include "fpsgo_base.h"
#include "fpsgo_cpu_policy.h"
#include "fbt_cpu_ctrl.h"

/*--------------------------------------------*/

#define MAX(a, b) (((a) > (b)) ? (a) : (b))
#define MIN(a, b) (((a) < (b)) ? (a) : (b))


struct FBT_CPU_CTRL_NOTIFIER_PUSH_TAG {
	int policy_id;
	unsigned int freq;
	struct work_struct sWork;
};

struct cpu_info {
	u64 time;
};

/* Configurable */
struct workqueue_struct *g_psFbtCpuCtrlWorkQueue;

struct cpufreq_policy **fbt_cpu_policy;
struct freq_qos_request *fbt_cpu_rq;

static int policy_num;
static int *fbt_opp_cnt;
static unsigned int **fbt_opp_tbl;
static unsigned int *fbt_min_freq;
static unsigned int *fbt_max_freq;
static int *fbt_cur_ceiling;
static int *fbt_final_ceiling;
static int *fbt_cur_floor;

static struct mutex cpu_ctrl_lock;

static int cfp_onoff;
static int cfp_polling_ms;
static int cfp_up_time;
static int cfp_down_time;
static int cfp_up_loading;
static int cfp_down_loading;

module_param(cfp_onoff, int, 0644);
module_param(cfp_polling_ms, int, 0644);
module_param(cfp_up_time, int, 0644);
module_param(cfp_down_time, int, 0644);
module_param(cfp_up_loading, int, 0644);
module_param(cfp_down_loading, int, 0644);

static int cfp_cur_up_time;
static int cfp_cur_down_time;
static int cfp_cur_loading; /*record previous cpu loading*/
static int cfp_ceil_rel; /* release ceiling */
static int cfp_enable;

static struct workqueue_struct *g_psCpuLoadingWorkQueue;
static struct hrtimer cpu_ctrl_hrt;
static void notify_cpu_loading_timeout(struct work_struct *work);
static DECLARE_WORK(cpu_loading_timeout_work,
		(void *) notify_cpu_loading_timeout);/*statically create work */

/* cpu loading tracking */
static struct cpu_info *cur_wall_time, *cur_idle_time,
		       *prev_wall_time, *prev_idle_time;


/*--------------------INIT------------------------*/
/* local function */
static noinline int tracing_mark_write(const char *buf)
{
	trace_printk(buf);
	return 0;
}

static void __cpu_ctrl_systrace(int val, const char *fmt, ...)
{
	char log[256];
	va_list args;
	int len;
	char buf2[256];

	memset(log, ' ', sizeof(log));
	va_start(args, fmt);
	len = vsnprintf(log, sizeof(log), fmt, args);
	va_end(args);

	if (unlikely(len < 0))
		return;
	else if (unlikely(len == 256))
		log[255] = '\0';

	len = snprintf(buf2, sizeof(buf2), "C|%d|%s|%d\n", powerhal_tid, log, val);
	if (unlikely(len < 0))
		return;
	else if (unlikely(len == 256))
		buf2[255] = '\0';
	tracing_mark_write(buf2);
}

static void __cpu_ctrl_freq_systrace(int policy, int freq)
{
	if (policy == 0)
		__cpu_ctrl_systrace(freq, "cpu_ceil_cluster_0");
	else if (policy == 1)
		__cpu_ctrl_systrace(freq, "cpu_ceil_cluster_1");
	else if (policy == 2)
		__cpu_ctrl_systrace(freq, "cpu_ceil_cluster_2");
}

static void __update_cpu_freq_locked(void)
{
	int i, ceiling_to_set;

	for (i = 0; i < policy_num; i++) {
#if DEBUG_LOG
		pr_info("%s, i:%d, min:%d, max:%d, cur_min:%d, cur_max:%d,
		final:%d\n", __func__, i, fbt_min_freq[i], fbt_max_freq[i],
		fbt_cur_floor[i], fbt_cur_ceiling[i], fbt_final_ceiling[i]);
#endif

		if (cfp_ceil_rel == 0 || cfp_onoff == 0) {

			if (fbt_cur_ceiling[i] < fbt_cur_floor[i])
				ceiling_to_set = fbt_cur_floor[i];
			else
				ceiling_to_set = fbt_cur_ceiling[i];

			if (fbt_final_ceiling[i] != ceiling_to_set) {
				fbt_final_ceiling[i] = ceiling_to_set;
				freq_qos_update_request(&(fbt_cpu_rq[i]), fbt_final_ceiling[i]);
				__cpu_ctrl_systrace(fbt_final_ceiling[i],
					"cpu_ceil_cluster_%d", i);
			}
		} else {
			freq_qos_update_request(&(fbt_cpu_rq[i]), fbt_max_freq[i]);
			__cpu_ctrl_freq_systrace(i, fbt_max_freq[i]);
		}
	}
}

/*hrtimer trigger*/
static void enable_cpu_loading_timer(void)
{
	ktime_t ktime;

	if (!cfp_onoff || policy_num == 0)
		return;

	ktime = ktime_set(0, cfp_polling_ms * 1000000);
	hrtimer_start(&cpu_ctrl_hrt, ktime, HRTIMER_MODE_REL);
}

/*close hrtimer*/
static void disable_cpu_loading_timer(void)
{
	hrtimer_cancel(&cpu_ctrl_hrt);
}

static enum hrtimer_restart handle_cpu_loading_timeout(struct hrtimer *timer)
{
	if (g_psCpuLoadingWorkQueue)
		queue_work(g_psCpuLoadingWorkQueue, &cpu_loading_timeout_work);
	return HRTIMER_NORESTART;
}

/*update info*/
static int update_cpu_loading_locked(void)
{
	int i, tmp_cpu_loading = 0;

	u64 wall_time = 0, idle_time = 0;

	for_each_possible_cpu(i) {
		prev_wall_time[i].time = cur_wall_time[i].time;
		prev_idle_time[i].time = cur_idle_time[i].time;

		/*idle time include iowait time*/
		cur_idle_time[i].time = get_cpu_idle_time(i,
				&cur_wall_time[i].time, 1);

		wall_time += cur_wall_time[i].time - prev_wall_time[i].time;
		idle_time += cur_idle_time[i].time - prev_idle_time[i].time;
	}

	if (wall_time > 0 && wall_time > idle_time)
		tmp_cpu_loading = div_u64((100 * (wall_time - idle_time)),
			wall_time);

	cfp_cur_loading = tmp_cpu_loading;

#if DEBUG_LOG
	pr_info("%s loading:%d\n", __func__, cfp_cur_loading);
#endif
	__cpu_ctrl_systrace(cfp_cur_loading, "cfp_loading");
	return 0;
}

static void update_cfp_policy_locked(void)
{
	if (cfp_cur_loading >= cfp_up_loading) {
		cfp_cur_down_time = 0;
		cfp_cur_up_time =
			MIN(cfp_cur_up_time + 1, cfp_up_time);

		if (cfp_cur_up_time >= cfp_up_time) {
			if (!cfp_ceil_rel) {
				cfp_ceil_rel = 1;
				__cpu_ctrl_systrace(1, "cfp_ceil_rel");
				__update_cpu_freq_locked();
			}
		}

	} else if (cfp_cur_loading < cfp_down_loading) {
		cfp_cur_up_time = 0;
		cfp_cur_down_time =
			MIN(cfp_cur_down_time + 1, cfp_down_time);

		if (cfp_cur_down_time >= cfp_down_time) {
			if (cfp_ceil_rel) {
				cfp_ceil_rel = 0;
				__cpu_ctrl_systrace(0, "cfp_ceil_rel");
				__update_cpu_freq_locked();
			}
		}
	} else {
		cfp_cur_up_time = 0;
		cfp_cur_down_time = 0;
	}
}

static void notify_cpu_loading_timeout(struct work_struct *work)
{
#if DEBUG_LOG
	pr_info("%s\n", __func__);
#endif

	mutex_lock(&cpu_ctrl_lock);
	update_cpu_loading_locked();
	update_cfp_policy_locked();
	mutex_unlock(&cpu_ctrl_lock);
	enable_cpu_loading_timer();
}

static void fbt_cpu_ctrl_notifier_wq_cb(struct work_struct *psWork)
{
	int id;

	struct FBT_CPU_CTRL_NOTIFIER_PUSH_TAG *vpPush =
		FPSGO_CONTAINER_OF(psWork,
				struct FBT_CPU_CTRL_NOTIFIER_PUSH_TAG, sWork);

	if (!vpPush)
		return;

#if DEBUG_LOG
	pr_info("%s, policy:%d, freq:%d", __func__, vpPush->policy_id, vpPush->freq);
#endif

	id = vpPush->policy_id;

	if (id < 0)
		return;

	mutex_lock(&cpu_ctrl_lock);

	if (vpPush->policy_id < policy_num) {

		if (vpPush->freq > fbt_max_freq[id])
			fbt_cur_floor[id] = fbt_max_freq[id];
		else if (vpPush->freq < fbt_min_freq[id])
			fbt_cur_floor[id] = fbt_min_freq[id];
		else
			fbt_cur_floor[id] = vpPush->freq;
	}

	__update_cpu_freq_locked();

	mutex_unlock(&cpu_ctrl_lock);

	fpsgo_free(vpPush, sizeof(struct FBT_CPU_CTRL_NOTIFIER_PUSH_TAG));
}

static struct notifier_block *fbt_freq_min_notifier;

static int freq_min_notifier_call(struct notifier_block *this,
	unsigned long event, void *ptr)
{
	int i;
	struct FBT_CPU_CTRL_NOTIFIER_PUSH_TAG *vpPush;
#if DEBUG_LOG
	pr_info("%s, event:%d", __func__, event);
#endif

	if (!g_psFbtCpuCtrlWorkQueue)
		return NOTIFY_DONE;

	vpPush = (struct FBT_CPU_CTRL_NOTIFIER_PUSH_TAG *)
		fpsgo_alloc_atomic(sizeof(struct FBT_CPU_CTRL_NOTIFIER_PUSH_TAG));

	if (!vpPush)
		return NOTIFY_DONE;

	vpPush->policy_id = -1;

	for (i = 0; i < policy_num; i++) {
		if (this == fbt_freq_min_notifier+i) {
#if DEBUG_LOG
			pr_info("%s, policy:%d", __func__, i);
#endif
			vpPush->policy_id = i;
			vpPush->freq = event;
		}
	}

	INIT_WORK(&vpPush->sWork, fbt_cpu_ctrl_notifier_wq_cb);
	queue_work(g_psFbtCpuCtrlWorkQueue, &vpPush->sWork);

	return NOTIFY_DONE;
}

static int fbt_cpu_topo_info(void)
{
	int cpu, i;
	int num = 0;
	struct cpufreq_policy *policy;

	policy_num = fpsgo_get_cpu_policy_num();
#if DEBUG_LOG
	pr_info("%s, policy_num:%d", __func__, policy_num);
#endif

	if (policy_num <= 0)
		return -EFAULT;

	if (fpsgo_get_cpu_opp_info(&fbt_opp_cnt, &fbt_opp_tbl) < 0)
		return -EFAULT;

	fbt_min_freq = kcalloc(policy_num, sizeof(unsigned int), GFP_KERNEL);
	fbt_max_freq = kcalloc(policy_num, sizeof(unsigned int), GFP_KERNEL);

	for (i = 0; i < policy_num; i++) {
		fbt_max_freq[i] = fbt_opp_tbl[i][0];
		fbt_min_freq[i] = fbt_opp_tbl[i][fbt_opp_cnt[i]-1];
#if DEBUG_LOG
		pr_info("i:%d, opp_cnt:%d, (%d,%d)",
		i, fbt_opp_cnt[i], fbt_min_freq[i], fbt_max_freq[i]);
#endif
	}

	fbt_cpu_policy = kcalloc(policy_num,
		sizeof(struct cpufreq_policy *), GFP_KERNEL);
	fbt_cpu_rq = kcalloc(policy_num,
		sizeof(struct freq_qos_request), GFP_KERNEL);

	fbt_freq_min_notifier = kcalloc(policy_num, sizeof(struct notifier_block), GFP_KERNEL);

	num = 0;

	for_each_possible_cpu(cpu) {
		if (num >= policy_num)
			break;

		policy = cpufreq_cpu_get(cpu);
		if (!policy)
			continue;

		fbt_cpu_policy[num] = policy;
#if DEBUG_LOG
		pr_info("%s, policy[%d]: first:%d", __func__, num, cpu);
#endif

		fbt_freq_min_notifier[num].notifier_call = freq_min_notifier_call;

		freq_qos_add_request(&policy->constraints,
			&(fbt_cpu_rq[num]), FREQ_QOS_MAX, fbt_opp_tbl[num][0]);
		freq_qos_add_notifier(&policy->constraints,
			FREQ_QOS_MIN, fbt_freq_min_notifier+num);

		num++;
		cpu = cpumask_last(policy->related_cpus);
	}

	return 0;
}


int fbt_set_cpu_freq_ceiling(int num, int *freq)
{
	int i, need_cfp = 0;

	mutex_lock(&cpu_ctrl_lock);

	for (i = 0; i < policy_num && i < num; i++) {
#if DEBUG_LOG
		pr_info("%s i:%d, freq:%d\n", __func__, i, freq[i]);
#endif

		if (freq[i] == -1)
			fbt_cur_ceiling[i] = fbt_max_freq[i];
		else {
			need_cfp = 1;

			if (freq[i] > fbt_max_freq[i])
				fbt_cur_ceiling[i] = fbt_max_freq[i];
			else if (freq[i] < fbt_min_freq[i])
				fbt_cur_ceiling[i] = fbt_min_freq[i];
			else
				fbt_cur_ceiling[i] = freq[i];
		}
	}
	__update_cpu_freq_locked();

	/* enable / disable CFP */
	if (need_cfp) {
		if (!cfp_enable) {
			cfp_enable = 1;
			enable_cpu_loading_timer();
		}
	} else {
		if (cfp_enable) {
			cfp_enable = 0;
			disable_cpu_loading_timer();
		}
	}
	mutex_unlock(&cpu_ctrl_lock);
	return 0;
}

void update_userlimit_cpu_freq(int kicker, int cluster_num, struct cpu_ctrl_data *pld)
{
	int *freq;
	int i;

	freq = kcalloc(policy_num,
		sizeof(struct cpufreq_policy *), GFP_KERNEL);
	for (i = 0; i < cluster_num; i++)
		freq[i] = pld[i].max;

	fbt_set_cpu_freq_ceiling(cluster_num, freq);

	kfree(freq);

	return;
}


int fbt_cpu_ctrl_init(void)
{
	int i, cpu_num;

	pr_info("%s init\n", __func__);

	if (fbt_cpu_topo_info() < 0)
		return -EFAULT;

	mutex_init(&cpu_ctrl_lock);

	g_psCpuLoadingWorkQueue =
		create_singlethread_workqueue("fpt_cpu_load_wq");

	if (g_psCpuLoadingWorkQueue == NULL)
		return -EFAULT;

	/*timer init*/
	hrtimer_init(&cpu_ctrl_hrt, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	cpu_ctrl_hrt.function = &handle_cpu_loading_timeout;

	g_psFbtCpuCtrlWorkQueue =
		create_singlethread_workqueue("fpt_cpu_ctrl_wq");

	if (g_psFbtCpuCtrlWorkQueue == NULL)
		return -EFAULT;

	fbt_cur_ceiling = kcalloc(policy_num,
		sizeof(struct cpufreq_policy *), GFP_KERNEL);
	fbt_final_ceiling = kcalloc(policy_num,
		sizeof(struct freq_qos_request), GFP_KERNEL);
	fbt_cur_floor = kcalloc(policy_num,
		sizeof(struct freq_qos_request), GFP_KERNEL);

	for (i = 0; i < policy_num; i++) {
		fbt_cur_ceiling[i] = fbt_max_freq[i];
		fbt_final_ceiling[i] = fbt_max_freq[i];
		fbt_cur_floor[i] = fbt_min_freq[i];
	}

	cpu_num = num_possible_cpus();
	if (cpu_num <= 0)
		return -EFAULT;

	cur_wall_time  =  kcalloc(cpu_num, sizeof(struct cpu_info), GFP_KERNEL);
	cur_idle_time  =  kcalloc(cpu_num, sizeof(struct cpu_info), GFP_KERNEL);
	prev_wall_time  =  kcalloc(cpu_num, sizeof(struct cpu_info), GFP_KERNEL);
	prev_idle_time  =  kcalloc(cpu_num, sizeof(struct cpu_info), GFP_KERNEL);

	for_each_possible_cpu(i) {
		prev_wall_time[i].time = cur_wall_time[i].time = 0;
		prev_idle_time[i].time = cur_idle_time[i].time = 0;
	}

	cfp_onoff = 1;
	cfp_enable = 0;

	cfp_polling_ms   = 64;
	cfp_up_time      = 1;
	cfp_down_time    = 16;
	cfp_up_loading   = 90;
	cfp_down_loading = 80;


	pr_info("%s done\n", __func__);
	return 0;
}

int fbt_cpu_ctrl_exit(void)
{
	kfree(fbt_min_freq);
	kfree(fbt_max_freq);
	kfree(fbt_cpu_policy);
	kfree(fbt_cpu_rq);
	kfree(fbt_freq_min_notifier);

	kfree(fbt_cur_ceiling);
	kfree(fbt_final_ceiling);
	kfree(fbt_cur_floor);
	kfree(cur_wall_time);
	kfree(cur_idle_time);
	kfree(prev_wall_time);
	kfree(prev_idle_time);

	return 0;
}

