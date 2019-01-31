/*
 * Copyright (C) 2016 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

/*
 * Support multi-type scheduler driven dvfs
 *
 * typeI: scheduler assistant
 *   The sched-assist is not a governor. Scheduler can trigger a request
 *
 * typeII: sched+ govder with tiny system
 *
 * typeIII: sched+ governor
 *
 */

#include <linux/cpufreq.h>
#include <linux/module.h>
#include <linux/kthread.h>
#include <linux/percpu.h>
#include <linux/irq_work.h>
#include <linux/delay.h>
#include <linux/string.h>
#include <trace/events/sched.h>

#include "sched.h"
#include "cpufreq_schedplus.h"

/* next throttling period expiry if increasing OPP */
#define THROTTLE_DOWN_NSEC     4000000 /* 4ms default */
/* next throttling period expiry if decreasing OPP */
#define THROTTLE_UP_NSEC       0  /* 0us */

#define THROTTLE_NSEC          2000000 /* 2ms default */

#define MAX_CLUSTER_NR 3

#ifdef CONFIG_CPU_FREQ_SCHED_ASSIST
int sched_dvfs_type = 1;
#else
#ifdef CONFIG_MTK_TINYSYS_SSPM_SUPPORT
int sched_dvfs_type = 2;
#else
int sched_dvfs_type = 3;
#endif /* end of tiny sys */
#endif /* end of sched-assist */

#ifdef CONFIG_MTK_TINYSYS_SSPM_SUPPORT
static DEFINE_PER_CPU(unsigned long, freq_scale) = SCHED_CAPACITY_SCALE;
#endif

#ifdef CONFIG_CPU_FREQ_SCHED_ASSIST
struct static_key __read_mostly __sched_freq = STATIC_KEY_INIT_TRUE;
#else /* GOV_SCHED */
struct static_key __read_mostly __sched_freq = STATIC_KEY_INIT_FALSE;
#endif
/* To confirm kthread if created */
static bool g_inited[MAX_CLUSTER_NR] = {false};

static bool __read_mostly cpufreq_driver_slow;

#ifndef CONFIG_CPU_FREQ_DEFAULT_GOV_SCHED
static struct cpufreq_governor cpufreq_gov_sched;
#endif

static DEFINE_PER_CPU(unsigned long, enabled);
DEFINE_PER_CPU(struct sched_capacity_reqs, cpu_sched_capacity_reqs);

/* keep goverdata as gloabl variable */
static struct gov_data *g_gd[MAX_CLUSTER_NR] = { NULL };


#define DEBUG 0
#define DEBUG_KLOG 0

#if DEBUG_KLOG
#define printk_dbg(f, a...) printk_deferred("[scheddvfs] "f, ##a)
#else
#define printk_dbg(f, a...) do {} while (0)
#endif

#include <mt-plat/met_drv.h>

struct sugov_cpu {
	struct sugov_policy *sg_policy;

	unsigned int cached_raw_freq;
	unsigned long iowait_boost;
	unsigned long iowait_boost_max;
	u64 last_update;

	/* The fields below are only needed when sharing a policy. */
	unsigned long util;
	unsigned long max;
	unsigned int flags;
	int idle;
};

static DEFINE_PER_CPU(struct sugov_cpu, sugov_cpu);

static void sugov_set_iowait_boost(struct sugov_cpu *sg_cpu, u64 time,
		unsigned int flags)
{
	if (flags == SCHE_IOWAIT)
		sg_cpu->iowait_boost = sg_cpu->iowait_boost_max;
	else if (sg_cpu->iowait_boost) {
		s64 delta_ns = time - sg_cpu->last_update;

		/* Clear iowait_boost if the CPU apprears to have been idle. */
		if (delta_ns > TICK_NSEC)
			sg_cpu->iowait_boost = 0;
	}
}

static char met_iowait_info[10][32] = {
	"sched_ioboost_cpu0",
	"sched_ioboost_cpu1",
	"sched_ioboost_cpu2",
	"sched_ioboost_cpu3",
	"sched_ioboost_cpu4",
	"sched_ioboost_cpu5",
	"sched_ioboost_cpu6",
	"sched_ioboost_cpu7",
	"NULL",
	"NULL"
};

static char met_dvfs_info[5][16] = {
	"sched_dvfs_cid0",
	"sched_dvfs_cid1",
	"sched_dvfs_cid2",
	"NULL",
	"NULL"
};

unsigned long int min_boost_freq[3] = {0}; /* boost3xxx */
unsigned long int cap_min_freq[3] = {0};   /* boost4xxx */

void (*cpufreq_notifier_fp)(int cluster_id, unsigned long freq);
EXPORT_SYMBOL(cpufreq_notifier_fp);

unsigned int capacity_margin_dvfs = DEFAULT_CAP_MARGIN_DVFS;
int dbg_id = DEBUG_FREQ_DISABLED;

/**
 * gov_data - per-policy data internal to the governor
 * @up_throttle: next throttling period expiry if increasing OPP
 * @down_throttle: next throttling period expiry if decreasing OPP
 * @up_throttle_nsec: throttle period length in nanoseconds if increasing OPP
 * @down_throttle_nsec: throttle period length in nanoseconds if decreasing OPP
 * @task: worker thread for dvfs transition that may block/sleep
 * @irq_work: callback used to wake up worker thread
 * @requested_freq: last frequency requested by the sched governor
 *
 * struct gov_data is the per-policy cpufreq_sched-specific data structure. A
 * per-policy instance of it is created when the cpufreq_sched governor receives
 * the CPUFREQ_GOV_START condition and a pointer to it exists in the gov_data
 * member of struct cpufreq_policy.
 *
 * Readers of this data must call down_read(policy->rwsem). Writers must
 * call down_write(policy->rwsem).
 */
struct gov_data {
	ktime_t throttle;
	ktime_t up_throttle;
	ktime_t down_throttle;
	unsigned int up_throttle_nsec;
	unsigned int down_throttle_nsec;
	unsigned int up_throttle_nsec_bk;
	unsigned int down_throttle_nsec_bk;
	unsigned int throttle_nsec;
	struct task_struct *task;
	struct irq_work irq_work;
	unsigned int requested_freq;
	struct cpufreq_policy *policy;
	int target_cpu;
	int cid;
	enum throttle_type thro_type; /* throttle up or down */
	u64 last_freq_update_time;
};

static inline bool is_sched_assist(void)
{
#ifdef CONFIG_CPU_FREQ_SCHED_ASSIST
	return true;
#else
	return false;
#endif
}

void temporary_dvfs_down_throttle_change(int change, unsigned long new_throttle)
{
	int i;

	for (i = 0; i < MAX_CLUSTER_NR; i++) {
		if (change)
			g_gd[i]->down_throttle_nsec = new_throttle;
		else
			g_gd[i]->down_throttle_nsec =
					g_gd[i]->down_throttle_nsec_bk;
	}
}

/*
 * return requested frequency if sched-gov used.
 */
unsigned int get_sched_cur_freq(int cid)
{
	if (!sched_freq())
		return 0;

	if (is_sched_assist())
		return 0;

	if ((cid > -1 && cid < MAX_CLUSTER_NR) && g_gd[cid])
		return g_gd[cid]->requested_freq;
	else
		return 0;
}
EXPORT_SYMBOL(get_sched_cur_freq);

void show_freq_kernel_log(int dbg_id, int cid, unsigned int freq)
{
	if (dbg_id == cid || dbg_id == DEBUG_FREQ_ALL)
		printk_deferred("[name:sched_power&] cid=%d freq=%u\n",
				cid, freq);
}

#ifdef CONFIG_MTK_TINYSYS_SSPM_SUPPORT
unsigned long cpufreq_scale_freq_capacity(struct sched_domain *sd, int cpu)
{
	return per_cpu(freq_scale, cpu);
}
#endif

static void cpufreq_sched_try_driver_target(
	int target_cpu, struct cpufreq_policy *policy,
	unsigned int freq, int type)
{
	struct gov_data *gd;
	int cid;
#ifdef CONFIG_MTK_TINYSYS_SSPM_SUPPORT
	int cpu;
	struct cpumask cls_cpus;
	unsigned int max;
	unsigned long scale;
#endif
	ktime_t cur_time;

	cid = arch_get_cluster_id(target_cpu);

	if (cid >= MAX_CLUSTER_NR || cid < 0) {
		WARN_ON(1);
		return;
	}

	/* policy may NOT trusted! */
	gd = g_gd[cid];

	if (dbg_id  < DEBUG_FREQ_DISABLED)
		show_freq_kernel_log(dbg_id, cid, freq);

	/* no freq = 0 case */
	if (!freq)
		return;

	/* if freq min of stune changed, notify fps tracker */
	if (min_boost_freq[cid] || cap_min_freq[cid])
		if (cpufreq_notifier_fp)
			cpufreq_notifier_fp(cid, freq);

	cur_time = ktime_get();

	/* update current freq asap if tiny system. */
#ifdef CONFIG_MTK_TINYSYS_SSPM_SUPPORT
	max = arch_scale_get_max_freq(target_cpu);

	/* freq is real world frequency already. */
	scale = (freq << SCHED_CAPACITY_SHIFT) / max;

	arch_get_cluster_cpus(&cls_cpus, cid);

	for_each_cpu(cpu, &cls_cpus) {
		per_cpu(freq_scale, cpu) = scale;
		arch_scale_set_curr_freq(cpu, freq);

		#ifdef CONFIG_SCHED_WALT
		cpu_rq(cpu)->cur_freq = freq;
		#endif
	}
#endif

	printk_dbg("%s: cid=%d cpu=%d freq=%u max_freq=%lu\n",
			__func__,
			cid, gd->target_cpu, freq,
			arch_scale_get_max_freq(target_cpu));

	/*
	 * try to apply requested frequency to platform.
	 */
#ifdef CONFIG_MTK_TINYSYS_SSPM_SUPPORT
#ifdef CONFIG_CPU_FREQ_SCHED_ASSIST
	mt_cpufreq_set_by_schedule_load_cluster(cid, freq);
#else
	mt_cpufreq_set_by_wfi_load_cluster(cid, freq);
#endif
#else
	policy = cpufreq_cpu_get(gd->target_cpu);

	if (IS_ERR_OR_NULL(policy))
		return;

	if (policy->governor != &cpufreq_gov_sched ||
		!policy->governor_data)
		return;

	/* avoid race with cpufreq_sched_stop. */
	if (!down_write_trylock(&policy->rwsem))
		return;

	__cpufreq_driver_target(policy, freq, CPUFREQ_RELATION_L);

	up_write(&policy->rwsem);

	if (policy)
		cpufreq_cpu_put(policy);
#endif
	/* debug */
	met_tag_oneshot(0, met_dvfs_info[cid], freq);

	/*
	 * update throttle time:
	 * avoid inteference betwewn increasing/decreasing OPP.
	 */
	gd->up_throttle   = ktime_add_ns(cur_time, gd->up_throttle_nsec);
	gd->down_throttle = ktime_add_ns(cur_time, gd->down_throttle_nsec);
	gd->throttle      = ktime_add_ns(cur_time, gd->throttle_nsec);
}

void update_cpu_freq_quick(int cpu, int freq)
{
	int cid = arch_get_cluster_id(cpu);
	int freq_new;
	struct gov_data *gd;
	int max_clus_nr = arch_get_nr_clusters();
	unsigned int cur_freq;

	if (cid >= max_clus_nr || cid < 0)
		return;

	gd = g_gd[cid];
	cur_freq = gd->requested_freq;

	freq_new = mt_cpufreq_find_close_freq(cid, freq);

#if 0
	if (freq_new == cur_freq)
		return;
#endif

	gd->thro_type = freq_new < cur_freq ?
			DVFS_THROTTLE_DOWN : DVFS_THROTTLE_UP;

	cpufreq_sched_try_driver_target(cpu, NULL, freq_new, -1);
}
EXPORT_SYMBOL(update_cpu_freq_quick);

#if 0
static bool finish_last_request(struct gov_data *gd)
{
	ktime_t now = ktime_get();

	if (ktime_after(now, gd->throttle))
		return false;

	while (1) {
		int usec_left = ktime_to_ns(ktime_sub(gd->throttle, now));

		usec_left /= NSEC_PER_USEC;
		usleep_range(usec_left, usec_left + 100);
		now = ktime_get();
		if (ktime_after(now, gd->throttle))
			return true;
	}
}
#endif

/*
 * we pass in struct cpufreq_policy. This is safe because changing out the
 * policy requires a call to __cpufreq_governor(policy, CPUFREQ_GOV_STOP),
 * which tears down all of the data structures and __cpufreq_governor(policy,
 * CPUFREQ_GOV_START) will do a full rebuild, including this kthread with the
 * new policy pointer
 */
static int cpufreq_sched_thread(void *data)
{
	struct cpufreq_policy *policy;
	struct gov_data *gd;
	/* unsigned int new_request = 0; */
	int cpu;
	/* unsigned int last_request = 0; */

	policy = (struct cpufreq_policy *) data;
	gd = policy->governor_data;
	cpu = g_gd[gd->cid]->target_cpu;

	do {
		set_current_state(TASK_INTERRUPTIBLE);
		schedule();

		if (kthread_should_stop())
			break;

		cpufreq_sched_try_driver_target(cpu, policy,
			g_gd[gd->cid]->requested_freq, SCHE_INVALID);
#if 0
		new_request = gd->requested_freq;
		if (new_request == last_request) {
			set_current_state(TASK_INTERRUPTIBLE);
			schedule();
		} else {
			/*
			 * if the frequency thread sleeps while waiting to be
			 * unthrottled, start over to check for a newer request
			 */
			if (finish_last_request(gd))
				continue;
			last_request = new_request;
			cpufreq_sched_try_driver_target(-1,
					policy, new_request);
		}
#endif
	} while (!kthread_should_stop());

	return 0;
}

static void cpufreq_sched_irq_work(struct irq_work *irq_work)
{
	struct gov_data *gd;

	if (!irq_work)
		return;

	gd = container_of(irq_work, struct gov_data, irq_work);

	wake_up_process(gd->task);
}

static inline bool is_cur(int new_freq, int cur_freq, int cid)
{
	if (is_sched_assist())
		return false;

	if (new_freq == cur_freq) {
		if (!cpufreq_driver_slow) {
			if (new_freq == mt_cpufreq_get_cur_freq(cid))
				return true;
		} else {
			return true;
		}
	}

	return false;
}

static void update_fdomain_capacity_request(int cpu, int type)
{
	unsigned int freq_new, cpu_tmp;
	struct gov_data *gd;
	unsigned long capacity = 0;
	int cid = arch_get_cluster_id(cpu);
	struct cpumask cls_cpus;
	s64 delta_ns;
	unsigned long arch_max_freq = arch_scale_get_max_freq(cpu);
	u64 time = cpu_rq(cpu)->clock;
	struct cpufreq_policy *policy = NULL;
	ktime_t throttle, now;
	unsigned int cur_freq;
	unsigned int max, min;
	int cap_min = 0;

	/*
	 * Avoid grabbing the policy if possible. A test is still
	 * required after locking the CPU's policy to avoid racing
	 * with the governor changing.
	 */
	if (!per_cpu(enabled, cpu))
		return;

	gd = g_gd[cid];

#ifdef CONFIG_CPU_FREQ_SCHED_ASSIST
	/* type.I */
	if (!mt_cpufreq_get_sched_enable())
		goto out;
#else
	policy = cpufreq_cpu_get(cpu);

	if (IS_ERR_OR_NULL(policy))
		return;

	if (policy->governor != &cpufreq_gov_sched ||
		 !policy->governor_data)
		goto out;
#endif
	arch_get_cluster_cpus(&cls_cpus, cid);

	/* find max capacity requested by cpus in this policy */
	for_each_cpu(cpu_tmp, &cls_cpus) {
		struct sched_capacity_reqs *scr;
		unsigned long boosted_util = 0;
		struct sugov_cpu *sg_cpu = &per_cpu(sugov_cpu, cpu_tmp);

		if (!cpu_online(cpu_tmp))
			continue;

		/* convert IO boosted freq to capacity */
		boosted_util = (sg_cpu->iowait_boost << SCHED_CAPACITY_SHIFT) /
					arch_max_freq;

		/* iowait boost */
		if (cpu_tmp == cpu) {
			/* IO boosting only for CFS */
			if (type != SCHE_RT && type != SCHE_DL) {

				/* update iowait_boost */
				sugov_set_iowait_boost(sg_cpu, time, type);

				 /* convert IO boosted freq to capacity */
				boosted_util = (sg_cpu->iowait_boost <<
						SCHED_CAPACITY_SHIFT) /
						arch_max_freq;

				met_tag_oneshot(0, met_iowait_info[cpu_tmp],
						sg_cpu->iowait_boost);

				/*
				 * the boost is reduced by half during
				 * each following update
				 */
				sg_cpu->iowait_boost >>= 1;
			}
			sg_cpu->last_update = time;
		}
		scr = &per_cpu(cpu_sched_capacity_reqs, cpu_tmp);

		/*
		 * If the CPU utilization was last updated before the previous
		 * frequency update and the time elapsed between the last update
		 * of the CPU utilization and the last frequency update is long
		 * enough, don't take the CPU into account as it probably is
		 * idle now (and clear iowait_boost for it).
		 */
		delta_ns = gd->last_freq_update_time - cpu_rq(cpu_tmp)->clock;

		if (delta_ns > TICK_NSEC * 2) {/* 2tick */
			sg_cpu->iowait_boost = 0;
			sg_cpu->idle = 1;
			continue;
		}

		sg_cpu->idle = 0;

		/* check if IO boosting */
		if (boosted_util > scr->total)
			capacity = max(capacity, boosted_util);
		else
			capacity = max(capacity, scr->total);

#ifdef CONFIG_CGROUP_SCHEDTUNE
		/* see if capacity_min exist */
		if (!cap_min)
			cap_min = schedtune_cpu_capacity_min(cpu_tmp);
#endif
	}

	/* get real world frequency */
	freq_new = capacity * arch_max_freq >> SCHED_CAPACITY_SHIFT;

	/* clamp frequency for governor limit */
	max = arch_scale_get_max_freq(cpu);
	min = arch_scale_get_min_freq(cpu);

	/* boost3xxx: clamp frequency by boost limit */
	if (min_boost_freq[cid])
		freq_new = (freq_new > min_boost_freq[cid]) ?
				freq_new : min_boost_freq[cid];

	/* boost4xxx: clamp frequency if cap_min exist */
	if (cap_min && cap_min_freq[cid])
		freq_new = (freq_new > cap_min_freq[cid]) ?
				freq_new : cap_min_freq[cid];

	/* governor limit: clamp frequency with min/max */
	freq_new = clamp(freq_new, min, max);

	/* to get frequency in real world */
	freq_new = mt_cpufreq_find_close_freq(cid, freq_new);

	/* no freq = 0 case */
	if (!freq_new)
		goto out;

	now = ktime_get();

	cur_freq = gd->requested_freq;

	gd->target_cpu = cpu;

#ifdef CONFIG_MTK_TINYSYS_SSPM_SUPPORT
	/* type.II:
	 *
	 * Freq from SSPM is not in time.
	 * mt_cpufreq_get_cur_freq(cid);
	 */
	cur_freq =  gd->requested_freq;
#else
	/* type.III */
	cur_freq = policy->cur;
#endif

	/* get throttling type */
	throttle = freq_new < cur_freq ?
			gd->down_throttle : gd->up_throttle;

	gd->thro_type = freq_new < cur_freq ?
			DVFS_THROTTLE_DOWN : DVFS_THROTTLE_UP;

	/* No throttling in time? Bail and return. */
	if (ktime_before(now, throttle))
		goto out;

	/*
	 * W/O co-working governor:
	 * if no change in frequency, bail and return current capacity.
	 * to decrease overhead of freq swtich.
	 */
	if (is_cur(freq_new, cur_freq, cid)) {
		/* Update throttle windows only if same frequency */
		gd->up_throttle   = ktime_add_ns(now, gd->up_throttle_nsec);
		gd->down_throttle = ktime_add_ns(now, gd->down_throttle_nsec);
		gd->throttle      = ktime_add_ns(now, gd->throttle_nsec);
		goto out;
	}

	/* update request freq */
	gd->requested_freq = freq_new;

	gd->last_freq_update_time = time;

	mt_sched_printf(sched_dvfs,
	"cpu=%d type=%d cur=%d new=%d thro_type=%s now=%lld thro_time=%lld",
			gd->target_cpu, sched_dvfs_type,
			cur_freq, freq_new,
			(gd->thro_type == DVFS_THROTTLE_UP) ? "up":"dw",
			now.tv64, throttle.tv64
		       );

	/*
	 * Throttling is not yet supported on platforms with fast cpufreq
	 * drivers.
	 */
	if (cpufreq_driver_slow)
		irq_work_queue_on(&gd->irq_work, cpu);
	else
		cpufreq_sched_try_driver_target(cpu, policy, freq_new, type);

out:
	if (policy)
		cpufreq_cpu_put(policy);
}

void update_cpu_capacity_request(int cpu, bool request, int type)
{
	unsigned long new_capacity;
	struct sched_capacity_reqs *scr;

	/* The rq lock serializes access to the CPU's sched_capacity_reqs. */
	lockdep_assert_held(&cpu_rq(cpu)->lock);

	scr = &per_cpu(cpu_sched_capacity_reqs, cpu);

	new_capacity = scr->cfs + scr->rt;
	new_capacity = new_capacity * capacity_margin_dvfs
		/ SCHED_CAPACITY_SCALE;
	new_capacity += scr->dl;

#ifndef CONFIG_CPU_FREQ_SCHED_ASSIST
	if (new_capacity == scr->total)
		return;
#endif

	scr->total = new_capacity;
	if (request || type == SCHE_IOWAIT)
		update_fdomain_capacity_request(cpu, type);
}

static inline void set_sched_freq(void)
{
	static_key_slow_inc(&__sched_freq);
}

static inline void clear_sched_freq(void)
{
	static_key_slow_dec(&__sched_freq);
}


static struct attribute_group sched_attr_group_gov_pol;
static struct attribute_group *get_sysfs_attr(void)
{
	return &sched_attr_group_gov_pol;
}

static int cpufreq_sched_policy_init(struct cpufreq_policy *policy)
{
	struct gov_data *gd_ptr;
	int cpu;
	int rc;
	int cid = arch_get_cluster_id(policy->cpu);

	/* sched-assist is not a governor, return. */
	if (is_sched_assist())
		return 0;

	/* if kthread is created, return */
	if (g_inited[cid]) {
		policy->governor_data = g_gd[cid];

		/* [MUST] backup policy, because it changed */
		g_gd[cid]->policy = policy;

		for_each_cpu(cpu, policy->cpus)
			memset(&per_cpu(cpu_sched_capacity_reqs, cpu), 0,
				sizeof(struct sched_capacity_reqs));

		set_sched_freq();

		/* for "/sys/devices/system/cpu/cpufreq/policy(cpu)/sched" */
		rc = sysfs_create_group(&policy->kobj, get_sysfs_attr());
		if (rc) {
			pr_debug("%s: couldn't create sysfs attributes: %d\n",
					__func__, rc);
			goto err;
		}

		return 0;
	}

	/* keep goverdata as gloabl variable */
	gd_ptr = g_gd[cid];

	/* [MUST] backup policy in first time */
	gd_ptr->policy = policy;

	/* [MUST] backup target_cpu */
	gd_ptr->target_cpu = policy->cpu;

	policy->governor_data = gd_ptr;

	for_each_cpu(cpu, policy->cpus)
		memset(&per_cpu(cpu_sched_capacity_reqs, cpu), 0,
		       sizeof(struct sched_capacity_reqs));

	pr_debug("%s: throttle threshold = %u [ns]\n",
		  __func__, gd_ptr->throttle_nsec);

	/* for "/sys/devices/system/cpu/cpufreq/policy(cpu)/sched" */
	rc = sysfs_create_group(&policy->kobj, get_sysfs_attr());
	if (rc) {
		pr_debug("%s: couldn't create sysfs attributes: %d\n",
				__func__, rc);
		goto err;
	}

	if (cpufreq_driver_is_slow()) {
		int ret;
		struct sched_param param;

		cpufreq_driver_slow = true;
		gd_ptr->task = kthread_create(cpufreq_sched_thread, policy,
					  "kschedfreq:%d",
					  cpumask_first(policy->related_cpus));
		if (IS_ERR_OR_NULL(gd_ptr->task)) {
			pr_debug("%s: failed to create kschedfreq thread\n",
			       __func__);
			goto err;
		}

		param.sched_priority = 50;
		ret = sched_setscheduler_nocheck(gd_ptr->task,
				SCHED_FIFO, &param);
		if (ret) {
			pr_debug("%s: failed to set SCHED_FIFO\n", __func__);
			goto err;
		} else {
			pr_debug("%s: kthread (%d) set to SCHED_FIFO\n",
					__func__, gd_ptr->task->pid);
		}

		/* Task never die???? */
		get_task_struct(gd_ptr->task);
		kthread_bind_mask(gd_ptr->task, policy->related_cpus);
		wake_up_process(gd_ptr->task);
		init_irq_work(&gd_ptr->irq_work, cpufreq_sched_irq_work);
	}

	/* To confirm kthread is created. */
	g_inited[cid] = true;

	set_sched_freq();

	return 0;

err:
	sysfs_remove_group(get_governor_parent_kobj(policy), get_sysfs_attr());
	policy->governor_data = NULL;
	WARN_ON(1);

	return -ENOMEM;
}

static void cpufreq_sched_policy_exit(struct cpufreq_policy *policy)
{
	/* struct gov_data *gd = policy->governor_data; */

#ifdef CONFIG_CPU_FREQ_SCHED_ASSIST
	return 0;
#else
	clear_sched_freq();

	sysfs_remove_group(&policy->kobj, get_sysfs_attr());

	policy->governor_data = NULL;

	/* kfree(gd); */
	return;
#endif
}

static int cpufreq_sched_start(struct cpufreq_policy *policy)
{
#ifdef CONFIG_CPU_FREQ_SCHED_ASSIST
	return 0;
#else
	int cpu;

	for_each_cpu(cpu, policy->cpus)
		per_cpu(enabled, cpu) = 1;

	return 0;
#endif
}

static void cpufreq_sched_stop(struct cpufreq_policy *policy)
{
#ifdef CONFIG_CPU_FREQ_SCHED_ASSIST
	return;
#else
	int cpu;

	for_each_cpu(cpu, policy->cpus)
		per_cpu(enabled, cpu) = 0;
#endif
}

static struct notifier_block cpu_hotplug;

static int cpu_hotplug_handler(struct notifier_block *nb,
		unsigned long val, void *data)
{
	int cpu = (unsigned long)data;

	switch (val) {
	case CPU_ONLINE:
		printk_dbg("%s cpu=%d online\n", __func__, cpu);
		break;
	case CPU_ONLINE_FROZEN:
		break;
	case CPU_UP_PREPARE:
#ifdef CONFIG_CPU_FREQ_SCHED_ASSIST
		per_cpu(enabled, cpu) = 1;
#endif
		printk_dbg("%s cpu=%d up_prepare\n", __func__, cpu);
		break;
	case CPU_DOWN_PREPARE:
		per_cpu(enabled, cpu) = 0;

		printk_dbg("%s cpu=%d down_prepare\n", __func__, cpu);
		break;
	}
	return NOTIFY_OK;
}

/* Tunables */
static ssize_t show_up_throttle_nsec(struct cpufreq_policy *policy, char *buf)
{
	int cid = arch_get_cluster_id(policy->cpu);
	struct gov_data *gd = g_gd[cid];

	return sprintf(buf, "%u\n", gd->up_throttle_nsec);
}

static ssize_t store_up_throttle_nsec(struct cpufreq_policy *policy,
		const char *buf, size_t count)
{
	int ret;
	unsigned long int val;
	int cid = arch_get_cluster_id(policy->cpu);
	struct gov_data *gd = g_gd[cid];

	ret = kstrtoul(buf, 0, &val);
	if (ret < 0)
		return ret;
	gd->up_throttle_nsec = val;
	gd->up_throttle_nsec_bk = val;
	return count;
}

static ssize_t show_down_throttle_nsec(struct cpufreq_policy *policy, char *buf)
{
	int cid = arch_get_cluster_id(policy->cpu);
	struct gov_data *gd = g_gd[cid];

	return sprintf(buf, "%u\n", gd->down_throttle_nsec);
}

static ssize_t store_down_throttle_nsec(struct cpufreq_policy *policy,
		const char *buf, size_t count)
{
	int ret;
	unsigned long int val;
	int cid = arch_get_cluster_id(policy->cpu);
	struct gov_data *gd = g_gd[cid];

	ret = kstrtoul(buf, 0, &val);
	if (ret < 0)
		return ret;
	gd->down_throttle_nsec = val;
	gd->down_throttle_nsec_bk = val;
	return count;
}

/*
 * Create show/store routines
 * - sys: One governor instance for complete SYSTEM
 * - pol: One governor instance per struct cpufreq_policy
 */
#define show_gov_pol_sys(file_name)                                     \
	static ssize_t show_##file_name##_gov_pol                       \
(struct cpufreq_policy *policy, char *buf)                              \
{                                                                       \
	return show_##file_name(policy, buf);            \
}

#define store_gov_pol_sys(file_name)                                    \
	static ssize_t store_##file_name##_gov_pol                      \
(struct cpufreq_policy *policy, const char *buf, size_t count)          \
{                                                                       \
	return store_##file_name(policy, buf, count);    \
}

#define gov_pol_attr_rw(_name)                                          \
	static struct freq_attr _name##_gov_pol =                       \
__ATTR(_name, 0644, show_##_name##_gov_pol, store_##_name##_gov_pol)

#define show_store_gov_pol_sys(file_name)                               \
	show_gov_pol_sys(file_name);                                    \
	store_gov_pol_sys(file_name)

#define tunable_handlers(file_name) \
	show_gov_pol_sys(file_name); \
	store_gov_pol_sys(file_name); \
	gov_pol_attr_rw(file_name)

tunable_handlers(down_throttle_nsec);
tunable_handlers(up_throttle_nsec);

/* Per policy governor instance */
static struct attribute *sched_attributes_gov_pol[] = {
	&up_throttle_nsec_gov_pol.attr,
	&down_throttle_nsec_gov_pol.attr,
	NULL,
};

static struct attribute_group sched_attr_group_gov_pol = {
	.attrs = sched_attributes_gov_pol,
	.name = "sched",
};

#ifndef CONFIG_CPU_FREQ_DEFAULT_GOV_SCHED
static
#endif
struct cpufreq_governor cpufreq_gov_sched = {
	.name			= "schedplus",
	.init                   = cpufreq_sched_policy_init,
	.exit                   = cpufreq_sched_policy_exit,
	.start                  = cpufreq_sched_start,
	.stop                   = cpufreq_sched_stop,
	/*.limits                 = cpufreq_sched_limits,*/
	.owner			= THIS_MODULE,
};

static int __init cpufreq_sched_init(void)
{
	int cpu;
	int i;

	for_each_cpu(cpu, cpu_possible_mask) {
		struct sugov_cpu *sg_cpu = &per_cpu(sugov_cpu, cpu);
		int cid = arch_get_cluster_id(cpu);

		memset(&per_cpu(cpu_sched_capacity_reqs, cpu), 0,
				sizeof(struct sched_capacity_reqs));

		sg_cpu->util = 0;
		sg_cpu->max = 0;
		sg_cpu->flags = 0;
		sg_cpu->last_update = 0;
		sg_cpu->cached_raw_freq = 0;
		sg_cpu->iowait_boost = 0;
		sg_cpu->iowait_boost_max = mt_cpufreq_get_freq_by_idx(cid, 0);
		sg_cpu->iowait_boost_max >>= 1; /* limit max to half */
	}

	for (i = 0; i < MAX_CLUSTER_NR; i++) {
		g_gd[i] = kzalloc(sizeof(struct gov_data), GFP_KERNEL);
		if (!g_gd[i]) {
			WARN_ON(1);
			return -ENOMEM;
		}
		g_gd[i]->up_throttle_nsec      = THROTTLE_UP_NSEC;
		g_gd[i]->down_throttle_nsec    = THROTTLE_DOWN_NSEC;
		g_gd[i]->up_throttle_nsec_bk   = THROTTLE_UP_NSEC;
		g_gd[i]->down_throttle_nsec_bk = THROTTLE_DOWN_NSEC;
		g_gd[i]->throttle_nsec = THROTTLE_NSEC;
		g_gd[i]->last_freq_update_time = 0;
		/* keep cid needed */
		g_gd[i]->cid = i;
	}

#ifdef CONFIG_CPU_FREQ_SCHED_ASSIST
	for_each_cpu(cpu, cpu_possible_mask)
		per_cpu(enabled, cpu) = 1;

#else
	for_each_cpu(cpu, cpu_possible_mask)
		per_cpu(enabled, cpu) = 0;
#endif

	cpu_hotplug.notifier_call = cpu_hotplug_handler;
	register_hotcpu_notifier(&cpu_hotplug);

	return cpufreq_register_governor(&cpufreq_gov_sched);
}

#ifdef CONFIG_CPU_FREQ_SCHED_ASSIST
static int cpufreq_callback(struct notifier_block *nb,
		unsigned long val, void *data)
{
	struct cpufreq_freqs *freq = data;
	int cpu = freq->cpu;
	struct cpumask cls_cpus;
	int id;
	int cid = arch_get_cluster_id(cpu);
	ktime_t throttle = g_gd[cid]->throttle;
	bool sched_dvfs;

	if (freq->flags & CPUFREQ_CONST_LOOPS)
		return NOTIFY_OK;

	sched_dvfs = mt_cpufreq_get_sched_enable();

	if (val == CPUFREQ_PRECHANGE) {
		/* consider DVFS has been changed by PPM or other governors */
		if (!sched_dvfs ||
		    !ktime_before(ktime_get(), ktime_add_ns(throttle,
					(20000000 - THROTTLE_NSEC)/*20ms*/))) {
			arch_get_cluster_cpus(&cls_cpus, cid);
			for_each_cpu(id, &cls_cpus)
				arch_scale_set_curr_freq(id, freq->new);
		}
	}

	return NOTIFY_OK;
}


static struct notifier_block cpufreq_notifier = {
	.notifier_call = cpufreq_callback,
};

static int __init register_cpufreq_notifier(void)
{
	return cpufreq_register_notifier(&cpufreq_notifier,
			CPUFREQ_TRANSITION_NOTIFIER);
}

core_initcall(register_cpufreq_notifier);
/* sched-assist dvfs is NOT a governor. */
late_initcall(cpufreq_sched_init);
#else
/* Try to make this the default governor */
late_initcall(cpufreq_sched_init);
#endif
