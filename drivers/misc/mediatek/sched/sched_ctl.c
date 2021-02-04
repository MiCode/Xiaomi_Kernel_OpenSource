/*
 * Copyright (C) 2017 MediaTek Inc.
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
 * Support asynchronus hint for external modules to get loading change
 * in time from scheduler's help.
 *
 * 1. call-back function for notification of status change
 *   - int register_sched_hint_notifier( void(*fp)(int status) )
 *
 * 2. control interface for user
 *   - /sys/devices/system/cpu/sched/...
 *
 */

#include <linux/irq_work.h>
#include <linux/kthread.h>
#include <linux/module.h>
#include <linux/string.h>
#include <trace/events/sched.h>
#include <linux/cpu.h>
#include <linux/kobject.h>
#include <linux/sysfs.h>
#include "rq_stats.h"
#include "sched_ctl.h"
#include <mt-plat/met_drv.h>
#include <mt-plat/mtk_sched.h>

#define SCHED_HINT_THROTTLE_NSEC 10000000 /* 10ms for throttle */

struct sched_hint_data {
	struct task_struct *task;
	struct irq_work irq_work;
	int sys_cap;
	int sys_util;
	ktime_t throttle;
	struct attribute_group *attr_group;
	struct kobject *kobj;
};

#if 0
/* debugging */
static char met_cpu_load[16][32] = {
	"sched_load_cpu0",
	"sched_load_cpu1",
	"sched_load_cpu2",
	"sched_load_cpu3",
	"sched_load_cpu4",
	"sched_load_cpu5",
	"sched_load_cpu6",
	"sched_load_cpu7",
	"sched_load_cpu8",
	"sched_load_cpu9",
	"NULL"
};
#endif

/* global */
static u64 sched_hint_check_timestamp;
static u64 sched_hint_check_interval;
static struct sched_hint_data g_shd;
static int sched_hint_inited;
#ifdef CONFIG_MTK_SCHED_SYSHINT
static int sched_hint_on = 1; /* default on */
#else
static int sched_hint_on; /* default off */
#endif
static enum sched_status_t kthread_status = SCHED_STATUS_INIT;
static enum sched_status_t sched_status = SCHED_STATUS_INIT;
static int sched_hint_loading_thresh = 5; /* 5% (max 100%) */
static BLOCKING_NOTIFIER_HEAD(sched_hint_notifier_list);
static DEFINE_SPINLOCK(status_lock);
static struct kobj_attribute sched_iso_attr;
static struct kobj_attribute set_sched_iso_attr;
static struct kobj_attribute set_sched_deiso_attr;
#ifdef CONFIG_MTK_SCHED_BOOST
static struct kobj_attribute sched_boost_attr;
static struct kobj_attribute sched_cpu_prefer_attr;
#endif
static struct kobj_attribute set_sched_migration_kick_attr;

static int sched_hint_status(int util, int cap)
{
	enum sched_status_t status;

	if (((util * 100) / cap) > sched_hint_loading_thresh)
		status = SCHED_STATUS_OVERUTIL;
	else
		status = SCHED_STATUS_UNDERUTIL;

	return status;
}

/* kernel thread */
static int sched_hint_thread(void *data)
{
	do {
		set_current_state(TASK_INTERRUPTIBLE);
		schedule();

		if (kthread_should_stop())
			break;

		/* status change from scheduler hint */
		if (kthread_status != sched_status) {
			int ret;

			kthread_status = sched_status;

			met_tag_oneshot(0, "sched_hint", kthread_status);

			ret = blocking_notifier_call_chain
					(&sched_hint_notifier_list,
					kthread_status, NULL);

			/* reset throttle time */
			g_shd.throttle = ktime_add_ns(
					ktime_get(),
					SCHED_HINT_THROTTLE_NSEC);
		}

		#if 0
		if (true) { /* debugging code */
			int iter_cpu;

			for (iter_cpu = 0; iter_cpu < nr_cpu_ids; iter_cpu++)
				#ifdef CONFIG_MTK_SCHED_CPULOAD
				met_tag_oneshot(0, met_cpu_load[iter_cpu],
						sched_get_cpu_load(iter_cpu));
				#endif
		}
		#endif
	} while (!kthread_should_stop());


	return 0;
}

static void sched_irq_work(struct irq_work *irq_work)
{
	struct sched_hint_data *shd;

	if (!irq_work)
		return;

	shd = container_of(irq_work, struct sched_hint_data, irq_work);

	wake_up_process(shd->task);
}

static bool hint_need(void)
{
	return (kthread_status == sched_status) ? false : true;
}

static bool do_check(u64 wallclock)
{
	bool do_check = false;
	unsigned long flags;

	/* check interval */
	spin_lock_irqsave(&status_lock, flags);
	if ((wallclock - sched_hint_check_timestamp)
			>= sched_hint_check_interval) {
		sched_hint_check_timestamp = wallclock;
		do_check = true;
	}
	spin_unlock_irqrestore(&status_lock, flags);

	/* is throttled ? */
	if (ktime_before(ktime_get(), g_shd.throttle))
		do_check = false;

	return do_check;
}

void sched_hint_check(u64 wallclock)
{
	if (!sched_hint_inited || !sched_hint_on)
		return;

	if (do_check(wallclock)) {
		if (hint_need())
			/* wake up ksched_hint */
			wake_up_process(g_shd.task);
	}
}

/* scheduler update hint in fair.c */
void update_sched_hint(int sys_util, int sys_cap)
{
	ktime_t throttle = g_shd.throttle;
	ktime_t now = ktime_get();

	if (!sched_hint_inited || !sched_hint_on)
		return;

	if (ktime_before(now, throttle)) {
		/* met_tag_oneshot(0, "sched_hint_throttle", 1); */
		return;
	}

	if (!sys_cap)
		return;

	g_shd.sys_util =  sys_util;
	g_shd.sys_cap = sys_cap;

	sched_status = sched_hint_status(sys_util, sys_cap);

	if (kthread_status != sched_status)
		irq_work_queue_on(&g_shd.irq_work, smp_processor_id());
}

/* A user-space interfaces: */
static ssize_t store_sched_enable(struct kobject *kobj,
		struct kobj_attribute *attr, const char *buf, size_t count)
{
	unsigned int val = 0;

	if (sscanf(buf, "%iu", &val) != 0)
		sched_hint_on = (val) ? 1 : 0;

	return count;
}

static ssize_t store_sched_load_thresh(struct kobject *kobj,
		struct kobj_attribute *attr, const char *buf, size_t count)
{
	unsigned int val = 0;

	if (sscanf(buf, "%iu", &val) != 0) {
		if (val >= 0 && val < 100)
			sched_hint_loading_thresh = val;
	}
	return count;
}

static ssize_t show_sched_info(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	unsigned int len = 0;
	unsigned int max_len = 4096;

	len +=  snprintf(buf, max_len, "capacity total=%d\n", g_shd.sys_cap);
	len +=  snprintf(buf+len, max_len - len,
		"capacity used=%d\n", g_shd.sys_util);

	if (sched_hint_on) {
		if (kthread_status != SCHED_STATUS_INIT)
			len +=  snprintf(buf+len,
					max_len - len,
					"status=(%s)\n",
					(kthread_status !=
					SCHED_STATUS_OVERUTIL) ?
					"under" : "over");
		else
			len +=  snprintf(buf+len,
					max_len - len,
					"status=(init)\n");
	} else
		len +=  snprintf(buf+len, max_len - len, "status=(off)\n");

	len +=  snprintf(buf+len, max_len - len, "load thresh=%d%c\n",
					sched_hint_loading_thresh, '%');

	return len;
}

static DEFINE_MUTEX(ip_mutex);
static int idle_prefer_mode;

static ssize_t store_idle_prefer(struct kobject *kobj,
		struct kobj_attribute *attr, const char *buf, size_t count)
{
	unsigned int val = 0;
#ifdef CONFIG_CPU_FREQ_GOV_SCHEDPLUS
	static unsigned int dvfs_margin_orig;
#endif
	static int is_dirty;
	int en;

	mutex_lock(&ip_mutex);

	if (sscanf(buf, "%iu", &val) == 0) {
		mutex_unlock(&ip_mutex);
		return count;
	}

	idle_prefer_mode = val;

	en = (idle_prefer_mode > 0) ? 1 : 0;

#ifdef CONFIG_CPU_FREQ_GOV_SCHEDPLUS
	/* backup system settings */
	if (!is_dirty)
		dvfs_margin_orig = capacity_margin_dvfs;

	/* marginless DVFS control for high TLP scene */
	capacity_margin_dvfs = en ? 1024 : dvfs_margin_orig;
#endif

#ifdef CONFIG_SCHED_TUNE
	/*
	 * set cgroup prefer idle cpu via stune
	 * 1: fg
	 * 2: bg
	 * 3: top-app
	 */
	prefer_idle_for_perf_idx(1, en);
#endif
	is_dirty = en;

	mutex_unlock(&ip_mutex);

	return count;
}

static ssize_t show_idle_prefer(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	unsigned int len = 0;
	unsigned int max_len = 4096;

	len +=  snprintf(buf, max_len, "idle prefer = %d\n", idle_prefer_mode);

	return len;
}

static ssize_t set_sched_migration_kick(struct kobject *kobj,
		struct kobj_attribute *attr, const char *buf, size_t count)
{
	unsigned int val = 0;

	if (sscanf(buf, "%iu", &val) != 0)
		migration_kick_cpus();

	return count;
}

static ssize_t show_walt_info(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf);

static ssize_t store_walt_info(struct kobject *kobj,
		struct kobj_attribute *attr, const char *buf, size_t count);

static struct kobj_attribute sched_enable_attr =
__ATTR(hint_enable, 0200 /* S_IWUSR */, NULL, store_sched_enable);

static struct kobj_attribute sched_load_thresh_attr =
__ATTR(hint_load_thresh, 0200 /* S_IWUSR */, NULL, store_sched_load_thresh);

static struct kobj_attribute sched_info_attr =
__ATTR(hint_info, 0400 /* S_IRUSR */, show_sched_info, NULL);

static struct kobj_attribute sched_walt_info_attr =
__ATTR(walt_debug, 0600 /* S_IWUSR | S_IRUSR */,
			show_walt_info, store_walt_info);

static struct kobj_attribute sched_idle_prefer_attr =
__ATTR(idle_prefer, 0600, show_idle_prefer, store_idle_prefer);

static struct kobj_attribute set_sched_migration_kick_attr =
__ATTR(set_sched_migration_kick, 0200, NULL, set_sched_migration_kick);

static struct attribute *sched_attrs[] = {
	&sched_info_attr.attr,
	&sched_load_thresh_attr.attr,
	&sched_enable_attr.attr,
	&sched_iso_attr.attr,
	&set_sched_iso_attr.attr,
	&set_sched_deiso_attr.attr,
#ifdef CONFIG_MTK_SCHED_BOOST
	&sched_cpu_prefer_attr.attr,
	&sched_boost_attr.attr,
#endif
	&sched_walt_info_attr.attr,
	&sched_idle_prefer_attr.attr,
	&set_sched_migration_kick_attr.attr,
	NULL,
};

static struct attribute_group sched_attr_group = {
	.attrs = sched_attrs,
};

int register_sched_hint_notifier(struct notifier_block *nb)
{
	return blocking_notifier_chain_register(&sched_hint_notifier_list, nb);
}
EXPORT_SYMBOL(register_sched_hint_notifier);

int unregister_sched_hint_notifier(struct notifier_block *nb)
{
	return blocking_notifier_chain_unregister
				(&sched_hint_notifier_list, nb);
}
EXPORT_SYMBOL(unregister_sched_hint_notifier);

/* init function */
static int __init sched_hint_init(void)
{
	int ret;

	/* create thread */
	g_shd.task = kthread_create(sched_hint_thread, NULL, "ksched_hint");

	if (IS_ERR_OR_NULL(g_shd.task)) {
		pr_info("%s: failed to create ksched_hint thread.\n", __func__);
		goto err;
	}

	/* keep thread alive */
	get_task_struct(g_shd.task);

	/* init throttle time */
	g_shd.throttle = ktime_get();

	wake_up_process(g_shd.task);

	/* init irq_work */
	init_irq_work(&g_shd.irq_work, sched_irq_work);

	/* check interval 20ms */
	sched_hint_check_interval = CPU_LOAD_AVG_DEFAULT_MS * NSEC_PER_MSEC;

	/* enable sched hint */
	sched_hint_inited = 1;

	/*
	 * create a sched in cpu_subsys:
	 * /sys/devices/system/cpu/sched/...
	 */
	g_shd.attr_group = &sched_attr_group;
	g_shd.kobj = kobject_create_and_add("sched",
					&cpu_subsys.dev_root->kobj);

	if (g_shd.kobj) {
		ret = sysfs_create_group(g_shd.kobj, g_shd.attr_group);
		if (ret)
			kobject_put(g_shd.kobj);
		else
			kobject_uevent(g_shd.kobj, KOBJ_ADD);
	}

	return 0;


err:
	return -ENOMEM;
}

late_initcall(sched_hint_init);

/*
 * sched_ktime_clock()
 *  - to get wall time but not to update in suspended.
 */
#include <linux/syscore_ops.h>

static ktime_t ktime_last;
static bool sched_ktime_suspended;

u64 sched_ktime_clock(void)
{
	if (unlikely(sched_ktime_suspended))
		return ktime_to_ns(ktime_last);
	return ktime_get_ns();
}

static void sched_resume(void)
{
	sched_ktime_suspended = false;
}

static int sched_suspend(void)
{
	ktime_last = ktime_get();
	sched_ktime_suspended = true;
	return 0;
}

static struct syscore_ops sched_syscore_ops = {
	.resume = sched_resume,
	.suspend = sched_suspend
};

static int __init sched_init_ops(void)
{
	register_syscore_ops(&sched_syscore_ops);
	return 0;
}
late_initcall(sched_init_ops);

/* turn on/off sched boost scheduling */
static ssize_t show_sched_iso(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	unsigned int len = 0;
	unsigned int max_len = 4096;

	len += snprintf(buf+len, max_len-len, "cpu_isolated_mask=0x%lx\n",
			cpu_isolated_mask->bits[0]);
	len += snprintf(buf+len, max_len-len, "iso_prio=%d\n", iso_prio);

	return len;
}

static ssize_t set_sched_iso(struct kobject *kobj,
		struct kobj_attribute *attr, const char *buf, size_t count)
{
	unsigned int val = 0;

	if (sscanf(buf, "%iu", &val) < nr_cpu_ids)
		sched_isolate_cpu(val);

	return count;
}

static ssize_t set_sched_deiso(struct kobject *kobj,
		struct kobj_attribute *attr, const char *buf, size_t count)
{
	unsigned int val = 0;

	if (sscanf(buf, "%iu", &val) < nr_cpu_ids)
		sched_deisolate_cpu(val);

	return count;
}

static struct kobj_attribute sched_iso_attr =
__ATTR(sched_isolation, 0400, show_sched_iso, NULL);

static struct kobj_attribute set_sched_iso_attr =
__ATTR(set_sched_isolation, 0200, NULL, set_sched_iso);

static struct kobj_attribute set_sched_deiso_attr =
__ATTR(set_sched_deisolation, 0200, NULL, set_sched_deiso);

#ifdef CONFIG_MTK_SCHED_BOOST
static int sched_boost_type = SCHED_NO_BOOST;

inline int valid_cpu_prefer(int task_prefer)
{
	if (task_prefer < SCHED_PREFER_NONE || task_prefer >= SCHED_PREFER_END)
		return 0;

	return 1;
}

int sched_set_cpuprefer(pid_t pid, unsigned int prefer_type)
{
	struct task_struct *p;
	unsigned long flags;
	int retval = 0;

	if (!valid_cpu_prefer(prefer_type) || pid < 0)
		return -EINVAL;

	rcu_read_lock();
	retval = -ESRCH;
	p = find_task_by_vpid(pid);
	if (p != NULL) {
		raw_spin_lock_irqsave(&p->pi_lock, flags);
		p->cpu_prefer = prefer_type;
		raw_spin_unlock_irqrestore(&p->pi_lock, flags);
		trace_sched_set_cpuprefer(p);
	}
	rcu_read_unlock();

	return retval;
}

inline int hinted_cpu_prefer(int task_prefer)
{
	if (task_prefer <= SCHED_PREFER_NONE || task_prefer >= SCHED_PREFER_END)
		return 0;

	return 1;
}

/*
 * check if the task or the whole system to prefer to put on big core
 *
 */
int cpu_prefer(struct task_struct *p)
{
	if (sched_boost_type == SCHED_ALL_BOOST)
		return SCHED_PREFER_BIG;

	if (p->cpu_prefer == SCHED_PREFER_LITTLE &&
		schedtune_task_boost(p))
		return SCHED_PREFER_NONE;

	return p->cpu_prefer;
}

int task_prefer_little(struct task_struct *p)
{
	if (cpu_prefer(p) == SCHED_PREFER_LITTLE)
		return 1;

	return 0;
}

int task_prefer_big(struct task_struct *p)
{
	if (cpu_prefer(p) == SCHED_PREFER_BIG)
		return 1;

	return 0;
}

int task_prefer_fit(struct task_struct *p, int cpu)
{
	if (cpu_prefer(p) == SCHED_PREFER_NONE)
		return 0;

	if (task_prefer_little(p) && hmp_cpu_is_slowest(cpu))
		return 1;

	if (task_prefer_big(p) && hmp_cpu_is_fastest(cpu))
		return 1;

	return 0;
}

int task_prefer_match(struct task_struct *p, int cpu)
{
	if (cpu_prefer(p) == SCHED_PREFER_NONE)
		return 1;

	if (task_prefer_little(p) && hmp_cpu_is_slowest(cpu))
		return 1;

	if (task_prefer_big(p) && hmp_cpu_is_fastest(cpu))
		return 1;

	return 0;
}

int
task_prefer_match_on_cpu(struct task_struct *p, int src_cpu, int target_cpu)
{
	/* No need to migrate*/
	if (is_intra_domain(src_cpu, target_cpu))
		return 1;

	if (cpu_prefer(p) == SCHED_PREFER_NONE)
		return 1;

	if (task_prefer_little(p) && hmp_cpu_is_slowest(src_cpu))
		return 1;

	if (task_prefer_big(p) && hmp_cpu_is_fastest(src_cpu))
		return 1;

	return 0;
}

int select_task_prefer_cpu(struct task_struct *p, int new_cpu)
{
	int task_prefer;
	struct hmp_domain *domain;
	struct hmp_domain *tmp_domain[5] = {0, 0, 0, 0, 0};
	int i, iter_domain, domain_cnt = 0;
	int iter_cpu;
	struct cpumask *tsk_cpus_allow = tsk_cpus_allowed(p);

	task_prefer = cpu_prefer(p);

	if (!hinted_cpu_prefer(task_prefer))
		return new_cpu;

	for_each_hmp_domain_L_first(domain) {
		tmp_domain[domain_cnt] = domain;
		domain_cnt++;
	}

	for (i = 0; i < domain_cnt; i++) {
		iter_domain = (task_prefer == SCHED_PREFER_BIG) ?
				domain_cnt-i-1 : i;
		domain = tmp_domain[iter_domain];

		if (cpumask_test_cpu(new_cpu, &domain->possible_cpus))
			return new_cpu;

		for_each_cpu(iter_cpu, &domain->possible_cpus) {

			/* tsk with prefer idle to find bigger idle cpu */
			if (!cpu_online(iter_cpu) || cpu_isolated(iter_cpu) ||
				!cpumask_test_cpu(iter_cpu, tsk_cpus_allow))
				continue;

			/* favoring tasks that prefer idle cpus
			 * to improve latency.
			 */
			if (idle_cpu(iter_cpu))
				return iter_cpu;

		}
	}

	return new_cpu;
}

void sched_set_boost_fg(void)
{
	struct cpumask cpus;
	int nr;

	/* 1: Root
	 * 2: foreground
	 * 3: Foregrond/boost
	 * 4: background
	 * 5: system-background
	 * 6: top-app
	 */

	nr = arch_get_nr_clusters();
	arch_get_cluster_cpus(&cpus, nr-1);

	set_user_space_global_cpuset(&cpus, 3);
	set_user_space_global_cpuset(&cpus, 2);
	set_user_space_global_cpuset(&cpus, 6);
}

void sched_unset_boost_fg(void)
{
	unset_user_space_global_cpuset(2);
	unset_user_space_global_cpuset(3);
	unset_user_space_global_cpuset(6);
}

/* A mutex for scheduling boost switcher */
static DEFINE_MUTEX(sched_boost_mutex);

int set_sched_boost(unsigned int val)
{
	static unsigned int sysctl_sched_isolation_hint_enable_backup = -1;

	if ((val < SCHED_NO_BOOST) || (val >= SCHED_UNKNOWN_BOOST))
		return -1;

	if (sched_boost_type == val)
		return 0;

	mutex_lock(&sched_boost_mutex);

	/* back to original setting*/
	if (sched_boost_type == SCHED_ALL_BOOST)
		sched_scheduler_switch(SCHED_HYBRID_LB);
	else if (sched_boost_type == SCHED_FG_BOOST)
		sched_unset_boost_fg();

	sched_boost_type = val;

	if (val == SCHED_NO_BOOST) {
		if (sysctl_sched_isolation_hint_enable_backup > 0)
			sysctl_sched_isolation_hint_enable =
				sysctl_sched_isolation_hint_enable_backup;

	} else if ((val > SCHED_NO_BOOST) && (val < SCHED_UNKNOWN_BOOST)) {

		sysctl_sched_isolation_hint_enable_backup =
				sysctl_sched_isolation_hint_enable;
		sysctl_sched_isolation_hint_enable = 0;

		if (val == SCHED_ALL_BOOST)
			sched_scheduler_switch(SCHED_HMP_LB);
		else if (val == SCHED_FG_BOOST)
			sched_set_boost_fg();
	}
	printk_deferred("[name:sched_boost&] sched boost: set %d\n",
		sched_boost_type);
	mutex_unlock(&sched_boost_mutex);


	return 0;
}
EXPORT_SYMBOL(set_sched_boost);

/* turn on/off sched boost scheduling */
static ssize_t show_cpu_prefer(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	unsigned int len = 0;

	return len;
}

static ssize_t store_cpu_prefer(struct kobject *kobj,
		struct kobj_attribute *attr, const char *buf, size_t count)
{
	pid_t pid;
	unsigned int prefer_type;

	/*
	 * 0: NO sched boost
	 * 1: boost ALL task
	 * 2: boost foreground task
	 */
	if (sscanf(buf, "%d %u", &pid, &prefer_type) != 0)
		sched_set_cpuprefer(pid, prefer_type);
	else
		return -1;

	return count;
}

static struct kobj_attribute sched_cpu_prefer_attr =
__ATTR(cpu_prefer, 0600, show_cpu_prefer,
		store_cpu_prefer);

/* turn on/off sched boost scheduling */
static ssize_t show_sched_boost(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	unsigned int len = 0;
	unsigned int max_len = 4096;

	switch (sched_boost_type) {
	case SCHED_ALL_BOOST:
		len += snprintf(buf, max_len, "sched boost= all boost\n\n");
		break;
	case SCHED_FG_BOOST:
		len += snprintf(buf, max_len,
				"sched boost= foreground boost\n\n");
		break;
	default:
		len += snprintf(buf, max_len, "sched boost= no boost\n\n");
		break;
	}

	return len;
}

static ssize_t store_sched_boost(struct kobject *kobj,
		struct kobj_attribute *attr, const char *buf, size_t count)
{
	unsigned int val = 0;

	/*
	 * 0: NO sched boost
	 * 1: boost ALL task
	 * 2: boost foreground task
	 */
	if (sscanf(buf, "%iu", &val) != 0)
		set_sched_boost(val);

	return count;
}

static struct kobj_attribute sched_boost_attr =
__ATTR(sched_boost, 0600, show_sched_boost,
		store_sched_boost);
#else

inline int task_prefer_little(struct task_struct *p)
{
	return 0;
}

inline int task_prefer_big(struct task_struct *p)
{
	return 0;
}

inline int task_prefer_fit(struct task_struct *p, int cpu)
{
	return 0;
}

inline int task_prefer_match(struct task_struct *p, int cpu)
{
	return 1;
}

int select_task_prefer_cpu(struct task_struct *p, int new_cpu)
{
	return new_cpu;
}

int sched_set_cpuprefer(pid_t pid, unsigned int prefer_type)
{
	return -EINVAL;
}

#endif


/* schedule loading trackign change
 * 0: default
 * 1: walt
 */
static int lt_walt_table[LT_UNKNOWN_USER];
static int walt_dbg;

static char walt_maker_name[LT_UNKNOWN_USER+1][32] = {
	"powerHAL",
	"fpsGO",
	"sched",
	"debug",
	"unknown"
};

int sched_walt_enable(int user, int en)
{
	int walted = 0;
	int i;
	unsigned int user_mask = 0;

	if ((user < 0) || (user >= LT_UNKNOWN_USER))
		return -1;

	lt_walt_table[user] = en;

	for (i = 0; i < LT_UNKNOWN_USER; i++) {
		walted |= lt_walt_table[i];
		user_mask |= (lt_walt_table[i] << i);
	}

	if (walt_dbg) {
		walted = lt_walt_table[LT_WALT_DEBUG];
		user_mask = 0xFFFF;
	}

#ifdef CONFIG_SCHED_WALT
	sysctl_sched_use_walt_cpu_util  = walted;
	sysctl_sched_use_walt_task_util = walted;
	trace_sched_ctl_walt(user_mask, walted);
#endif

	return 0;
}
EXPORT_SYMBOL(sched_walt_enable);

static ssize_t show_walt_info(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	unsigned int len = 0;
	unsigned int max_len = 4096;
	int i;
	int supported = 0;

	for (i = 0; i < LT_UNKNOWN_USER; i++)
		len +=  snprintf(buf+len, max_len - len, "walt[%d] = %d (%s)\n",
				i, lt_walt_table[i], walt_maker_name[i]);
#ifdef CONFIG_SCHED_WALT
	supported = 1;
#endif
	len +=  snprintf(buf+len, max_len - len,
			"\nWALT support:%d\n", supported);
	len +=  snprintf(buf+len, max_len - len,
			"debug mode:%d\n", walt_dbg);
	len +=  snprintf(buf+len, max_len - len,
			"format: echo (debug:walt)\n");

	return len;
}
/*
 * argu1: enable debug mode
 * argu2: walted or not.
 *
 * echo 1 0 : enable debug mode, foring walted off.
 */
static ssize_t store_walt_info(struct kobject *kobj,
		struct kobj_attribute *attr, const char *buf, size_t count)
{
	int walted = -1;

	if (sscanf(buf, "%d:%d", &walt_dbg, &walted) <= 0)
		return count;

	if (walted < 0 || walted > 1 || walt_dbg < 0 || walt_dbg > 1)
		return count;

	if (walt_dbg)
		sched_walt_enable(LT_WALT_DEBUG, walted);
	else
		sched_walt_enable(LT_WALT_DEBUG, 0);

	return count;
}
