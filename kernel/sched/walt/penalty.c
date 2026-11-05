// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2023, Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include <linux/module.h>
#include <linux/sched.h>
#include <linux/kmemleak.h>
#include <trace/hooks/sched.h>
#include <linux/kprobes.h>
#include <linux/delay.h>

#include "walt.h"

#define DEBUG 0
#define USE_WALT_HEAVY 0

#define  PENALTY_DEF    (1)
#define  PENALTY_SCHED  (2)
#define  PENALTY_CTL    (4)
#define  PENALTY_LOG    (128)

#define ENABLE_SCHED (PENALTY_SCHED|PENALTY_DEF)
#define ENABLE_CTL (PENALTY_CTL|PENALTY_DEF)

#define PENALTY_DEBUG_VER  1
#define PENALTY_DEBUG_PROF 2

#define MAX_CLUSTER 5
#define MAX_KEY_TASK 2

static atomic64_t frame_time_ns = ATOMIC_INIT(0);

/* per task info for yield */
struct penalty_status {
	pid_t          pid;                /* task pid */
	u64            count;
	u64            last_yield_time;    /* last trigger time of sched_yield request */
	u64            est_yield_end;       /*end of last yield */
	atomic64_t       yield_total;          /* yield time in last window,sleep+running */
	u32            last_sleep_ns;        /* decay of sleep */
};
#define PENALTY_UNACTIVE_NS  (5000000000)
static DEFINE_RAW_SPINLOCK(penalty_lock); /* protect pstatus */
static struct penalty_status pstatus[MAX_KEY_TASK];
static inline  struct penalty_status * get_penalty_status_lru(pid_t pid_, u64 clock)
{
	int i;
	int min_idx=0;
	u64 min_count=UINT_MAX;
	for(i=0; i<MAX_KEY_TASK; i++)
	{
		if(pid_ == pstatus[i].pid && pstatus[i].count > 0)
		{
			return &pstatus[i];
		}
		/*find long time not used */
		if(pstatus[i].last_yield_time >0 && clock > pstatus[i].last_yield_time &&
		(clock-pstatus[i].last_yield_time)>PENALTY_UNACTIVE_NS ){
			pstatus[i].count = 1;
		}
		if(pstatus[i].count <= min_count)
		{
			min_idx=i;
			min_count=pstatus[i].count;
		}
	}

	pstatus[min_idx].pid = pid_;
	pstatus[min_idx].count = 1;
	pstatus[min_idx].last_yield_time = 0;
	pstatus[min_idx].est_yield_end = 0;
	atomic64_set(&pstatus[min_idx].yield_total, 0);
	pstatus[min_idx].last_sleep_ns = 0;
	return  &pstatus[min_idx];
}
static inline  struct penalty_status * get_penalty_status(pid_t pid_)
{
	int i;
	for(i=0; i<MAX_KEY_TASK; i++)
	{
		if(pid_ == pstatus[i].pid && pstatus[i].count > 0)
		{
			return &pstatus[i];
		}
	}
	return  0;
}


static int __maybe_unused large_penalty = PENALTY_LOG;
static unsigned int __read_mostly sysctl_sched_auto_penaty = 0;
static unsigned int __read_mostly sysctl_penalty_debug = 0;
/* allow at most 750*1000/1024=73% scale down */

#define MAX_SYMBOL_LEN	64
#define MAX_YIELD_SLEEP  (2000000ULL)
#define MIN_YIELD_SLEEP   (200000ULL)
#define MIN_YIELD_SLEEP_HEADROOM  (100000ULL)


/* minimap sleep time in ns */
static int penalty_headroom =   100000;
static u64 target_fps = 0;
static atomic_t yield_ns_total = ATOMIC_INIT(0);
static atomic_t yield_cnt_total = ATOMIC_INIT(0);


static atomic_t game_pid = ATOMIC_INIT(0);
static void penalty_init_sysctl(void);

void tracing_mark_write(int pid, char* counter, long scale){
	if(sysctl_penalty_debug&PENALTY_DEBUG_VER)
		trace_printk("C|%d|%s|%lu\n", pid, counter, scale);
}


static int yield_penaty(struct ctl_table *table, int write,
		void __user *buffer, size_t *lenp,
		loff_t *ppos)
{
	int ret;
	static DEFINE_MUTEX(mutex);
	mutex_lock(&mutex);
	ret = proc_dointvec_minmax(table, write, buffer, lenp, ppos);
	mutex_unlock(&mutex);
	return ret;
}


static int reset_wts_stat(pid_t pid){
	atomic_set(&game_pid, pid);
	return 0;
}

static void flush_task(struct task_struct *p){
	pid_t tgid;
	tgid = atomic_read(&game_pid);
	if( tgid >0 && (tgid == p->pid))
	{
		reset_wts_stat(0);
	}
}
/* monitor process is die */
static void android_rvh_flush_task(void *unused, struct task_struct *p){
	flush_task(p);
}

static void android_rvh_before_do_sched_yield(void *unused, long *skip)
{
	u64 delta_total;
	u64 clock;
	u64 frame_time_ns_;
	unsigned long flags;
	struct penalty_status * ps;

	pid_t tgid;
	u64 sleep_ns = 0;
	struct	task_struct *p = current;
	frame_time_ns_ = atomic64_read(&frame_time_ns);
	if(!(sysctl_sched_auto_penaty & ENABLE_SCHED) || 0 == frame_time_ns_ )
	{
		return ;
	}

	tgid = atomic_read(&game_pid);
	if (tgid >0 && tgid != p->tgid){
		return ;
	}
	*skip = 1;
	raw_spin_lock_irqsave(&penalty_lock, flags);

	clock = sched_clock();
	ps = get_penalty_status_lru(p->pid, clock);
	ps->count++;
	tracing_mark_write(p->pid, "last_yield_time", ps->last_yield_time);
	tracing_mark_write(p->pid, "clock", clock);
	if(ps->last_yield_time>0 && (clock - ps->last_yield_time) > MAX_YIELD_SLEEP)   /*refresh start of yield group*/
	{
		ps->last_sleep_ns =(clock - ps->last_yield_time);
		sleep_ns = ps->last_sleep_ns >> 1; /* try to sleep 1/2 */
		ps->last_sleep_ns = min(ps->last_sleep_ns, (u32)MAX_YIELD_SLEEP);
		ps->est_yield_end = ps->last_yield_time + frame_time_ns_ - MIN_YIELD_SLEEP_HEADROOM;
	}else
	{
		sleep_ns = ps->last_sleep_ns >> 1; /* try to sleep 1/4 */
	}
	sleep_ns = max(sleep_ns, MIN_YIELD_SLEEP);
	sleep_ns = min(sleep_ns, MAX_YIELD_SLEEP);
	ps->last_sleep_ns = sleep_ns;

	sleep_ns = max(sleep_ns, (u64)penalty_headroom);
	if( clock >= ps->est_yield_end)
	{
		sleep_ns = 0;
	}else if ((clock + sleep_ns) > ps->est_yield_end)
	{
		sleep_ns = ps->est_yield_end -clock;
	}

	delta_total = div64_u64(sleep_ns, 1000);
	raw_spin_unlock_irqrestore(&penalty_lock, flags);

	if(sleep_ns >0)
		usleep_range_state(delta_total, delta_total, TASK_IDLE);

	raw_spin_lock_irqsave(&penalty_lock, flags);
	ps = get_penalty_status_lru(p->pid, clock);
	ps->last_yield_time = sched_clock();
	delta_total = ps->last_yield_time  - clock;
	atomic64_add(delta_total, &ps->yield_total);
	raw_spin_unlock_irqrestore(&penalty_lock, flags);

	tracing_mark_write(p->pid, "yield_total", atomic64_read(&ps->yield_total));

	/*account stat of yield */
	atomic_add(delta_total, &yield_ns_total);
	atomic_inc(&yield_cnt_total);
}

static struct ctl_table penalty_table[] =
{
    {
		.procname	= "sched_auto_penalty",
		.data		= &sysctl_sched_auto_penaty,
		.maxlen		= sizeof(unsigned int),
		.mode		= 0644,
		.proc_handler	= yield_penaty,
		.extra1		= SYSCTL_ZERO,
		.extra2		= &large_penalty,
	},
	{ }
};

static void penalty_init_sysctl(void)
{
    struct ctl_table_header *hdr;
    hdr = register_sysctl("walt",penalty_table);
	kmemleak_not_leak(hdr);
}

static struct kset *penalty_kset;
static struct kobject *param_kobj;


static ssize_t set_penalty_headroom(struct kobject *kobj,
	struct kobj_attribute *attr, const char *buf, size_t count)
{
	long usr_val = 0;
	int ret;
	ret = kstrtol(buf, 0, &usr_val);
	if (ret) {
		pr_err("sched_penalty: kstrtol failed, ret=%d\n", ret);
		return count;
	}
	penalty_headroom = usr_val;
	return count;
}
static ssize_t get_penalty_headroom(struct kobject *kobj,
	struct kobj_attribute *attr, char *buf)
{
	long usr_val  = penalty_headroom;
	return scnprintf(buf, PAGE_SIZE, "%ld\n", usr_val);
}


static ssize_t set_target_fps(struct kobject *kobj,
	struct kobj_attribute *attr, const char *buf, size_t count)
{
	int ret;
	long usr_val = 0;
	u64 frame_time_ns_ = 0;
	ret = kstrtol(buf, 0, &usr_val);
	if (ret)
	{
		pr_err("sched_penalty: kstrtol failed, ret=%d\n", ret);
		return count;
	}
	if(usr_val< 0 || usr_val >240)
	{
		return count;
	}
	target_fps = usr_val;
	if(target_fps>0)
	{
		frame_time_ns_ = div64_u64(1000000000UL, target_fps);
		pr_info("sched_penalty:fps=%llu, frame_time_ns=%llu  \n",
		target_fps, frame_time_ns_);
	}
	atomic64_set(&frame_time_ns,frame_time_ns_) ;
	return count;
}
static ssize_t get_target_fps(struct kobject *kobj,
	struct kobj_attribute *attr, char *buf){
	long usr_val  = target_fps;
	return scnprintf(buf, PAGE_SIZE, "%ld\n", usr_val);
}

static ssize_t set_target_pid(struct kobject *kobj,
	struct kobj_attribute *attr, const char *buf, size_t count)
{
	long usr_val = 0;
	int ret;
	ret = kstrtol(buf, 0, &usr_val);
	if (ret)
	{
		pr_err("sched_penalty: kstrtol failed, ret=%d\n", ret);
		return count;
	}
	if(usr_val< 0 )
	{
		return count;
	}
	reset_wts_stat(usr_val);

	return count;
}
static ssize_t get_target_pid(struct kobject *kobj,
	struct kobj_attribute *attr, char *buf)
{
	pid_t usr_val  = atomic_read(&game_pid);
	return scnprintf(buf, PAGE_SIZE, "%d\n", usr_val);
}


static struct kobj_attribute attr1 =
	__ATTR(penalty_headroom, 0644, get_penalty_headroom,
	set_penalty_headroom);
static struct kobj_attribute attr2 =
	__ATTR(fps, 0644, get_target_fps, set_target_fps);
static struct kobj_attribute attr3 =
	__ATTR(target_pid, 0644, get_target_pid, set_target_pid);


static struct attribute *param_attrs[] =
{
	&attr1.attr,
	&attr2.attr,
	&attr3.attr,
	NULL };
static struct attribute_group param_attr_group =
{
	.attrs = param_attrs,
};

static int init_module_params(void)
{
	int ret;
	struct kobject *module_kobj;
	penalty_kset = kset_create_and_add("sched_penalty", NULL, kernel_kobj);

	if (!penalty_kset)
	{
		pr_err("Failed to create sched_penalty root object\n");
		return -1;
	}
	module_kobj = &penalty_kset->kobj;
	param_kobj = kobject_create_and_add("parameters", module_kobj);
	if (!param_kobj)
	{
		pr_err("sched_penalty: Failed to add param_kobj\n");
		return -ENOMEM;
	}

	ret = sysfs_create_group(param_kobj, &param_attr_group);
	if (ret)
	{
		pr_err("sched_penalty: Failed to create sysfs\n");
		return ret;
	}
	return 0;
}

static int deinit_module_params(void)
{
	if(param_kobj)
	{
		kobject_del(param_kobj);
		kobject_put(param_kobj);
	}
	if(penalty_kset)
		kset_unregister(penalty_kset);
	return 0;
}


static int __init penalty_init(void){
	pr_info("in penalty_init\n");
	penalty_init_sysctl();
	init_module_params();
	register_trace_android_rvh_flush_task(android_rvh_flush_task, NULL);
	register_trace_android_rvh_before_do_sched_yield(android_rvh_before_do_sched_yield, NULL);
    return 0;
}

static void __exit penalty_exit(void)
{
	pr_info("in penalty_exit\n");
	deinit_module_params();
}

module_init(penalty_init);
module_exit(penalty_exit);

MODULE_DESCRIPTION("QTI WALT optimization module");
MODULE_LICENSE("GPL v2");
MODULE_SOFTDEP("post: sched-walt");
MODULE_SOFTDEP("post: msm_performance");

