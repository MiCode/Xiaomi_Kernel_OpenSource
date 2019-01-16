/* Copyright (c) 2010-2013, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/hrtimer.h>
#include <linux/cpu.h>
#include <linux/kobject.h>
#include <linux/sysfs.h>
#include <linux/notifier.h>
#include <linux/slab.h>
#include <linux/workqueue.h>
#include <linux/sched.h>
#include <linux/spinlock.h>
#include <linux/rq_stats.h>
#include <linux/cpufreq.h>
#include <linux/kernel_stat.h>
#include <linux/tick.h>
#include <linux/suspend.h>
#include <linux/version.h>
#include <asm/smp_plat.h>

#include <trace/events/sched.h>

#define MAX_LONG_SIZE 24
#define DEFAULT_RQ_POLL_JIFFIES 1
#define DEFAULT_DEF_TIMER_JIFFIES 5
#define CPU_FREQ_VARIANT 0
#ifdef CONFIG_SCHED_HMP_PRIO_FILTER
static unsigned int heavy_task_prio = NICE_TO_PRIO(CONFIG_SCHED_HMP_PRIO_FILTER_VAL);
#define task_low_priority(prio) ((prio >= heavy_task_prio)?1:0)
#endif

//struct notifier_block freq_policy;
struct notifier_block freq_transition;
struct notifier_block cpu_hotplug;
static unsigned int heavy_task_threshold = 650; // max=1023

struct cpu_load_data {
	cputime64_t prev_cpu_idle;
	cputime64_t prev_cpu_wall;
	cputime64_t prev_cpu_iowait;
	unsigned int avg_load_maxfreq;
	unsigned int samples;
	unsigned int window_size;
	unsigned int cur_freq;
	unsigned int policy_max;
	cpumask_var_t related_cpus;
	spinlock_t cpu_load_lock;
};

static DEFINE_PER_CPU(struct cpu_load_data, cpuload);

#if (LINUX_VERSION_CODE < KERNEL_VERSION(3, 10, 0))
static inline u64 get_cpu_idle_time_jiffy(unsigned int cpu, u64 *wall)
{
	u64 idle_time;
	u64 cur_wall_time;
	u64 busy_time;

	cur_wall_time = jiffies64_to_cputime64(get_jiffies_64());

	busy_time = kcpustat_cpu(cpu).cpustat[CPUTIME_USER];
	busy_time += kcpustat_cpu(cpu).cpustat[CPUTIME_SYSTEM];
	busy_time += kcpustat_cpu(cpu).cpustat[CPUTIME_IRQ];
	busy_time += kcpustat_cpu(cpu).cpustat[CPUTIME_SOFTIRQ];
	busy_time += kcpustat_cpu(cpu).cpustat[CPUTIME_STEAL];
	busy_time += kcpustat_cpu(cpu).cpustat[CPUTIME_NICE];

	idle_time = cur_wall_time - busy_time;
	if (wall)
		*wall = cputime_to_usecs(cur_wall_time);

	return cputime_to_usecs(idle_time);
}

static inline cputime64_t get_cpu_idle_time(unsigned int cpu, cputime64_t *wall)
{
	u64 idle_time = get_cpu_idle_time_us(cpu, NULL);

	if (idle_time == -1ULL)
		return get_cpu_idle_time_jiffy(cpu, wall);
	else
		idle_time += get_cpu_iowait_time_us(cpu, wall);

	return idle_time;
}
#else  /* !(LINUX_VERSION_CODE < KERNEL_VERSION(3, 10, 0)) */
extern u64 get_cpu_idle_time(unsigned int cpu, u64 *wall, int io_busy);
#endif /* (LINUX_VERSION_CODE < KERNEL_VERSION(3, 10, 0)) */

static inline cputime64_t get_cpu_iowait_time(unsigned int cpu,
							cputime64_t *wall)
{
	u64 iowait_time = get_cpu_iowait_time_us(cpu, wall);

	if (iowait_time == -1ULL)
		return 0;

	return iowait_time;
}

static int update_average_load(unsigned int freq, unsigned int cpu, bool use_maxfreq)
{

	struct cpu_load_data *pcpu = &per_cpu(cpuload, cpu);
	cputime64_t cur_wall_time, cur_idle_time, cur_iowait_time;
	unsigned int idle_time, wall_time, iowait_time;
	unsigned int cur_load, load_at_max_freq;

#if (LINUX_VERSION_CODE < KERNEL_VERSION(3, 10, 0))
	cur_idle_time = get_cpu_idle_time(cpu, &cur_wall_time);
#else /* !(LINUX_VERSION_CODE < KERNEL_VERSION(3, 10, 0)) */
	cur_idle_time = get_cpu_idle_time(cpu, &cur_wall_time, 0);
#endif /* (LINUX_VERSION_CODE < KERNEL_VERSION(3, 10, 0)) */
	cur_iowait_time = get_cpu_iowait_time(cpu, &cur_wall_time);

	wall_time = (unsigned int) (cur_wall_time - pcpu->prev_cpu_wall);
	pcpu->prev_cpu_wall = cur_wall_time;

	idle_time = (unsigned int) (cur_idle_time - pcpu->prev_cpu_idle);
	pcpu->prev_cpu_idle = cur_idle_time;

	iowait_time = (unsigned int) (cur_iowait_time - pcpu->prev_cpu_iowait);
	pcpu->prev_cpu_iowait = cur_iowait_time;

	if (idle_time >= iowait_time)
		idle_time -= iowait_time;

	if (unlikely(!wall_time || wall_time < idle_time))
		return 0;

	if (freq)
		cur_load = 100 * (wall_time - idle_time) / wall_time;
	else
		cur_load = 0;

	/* Calculate the scaled load across CPU */
	if (cpu_online(cpu))
	{
		if (use_maxfreq)
			load_at_max_freq = (cur_load * freq) / pcpu->policy_max;
		else
			load_at_max_freq = cur_load;
	}
	else
		load_at_max_freq = 0;

#if 1
	if (!pcpu->avg_load_maxfreq) {
		/* This is the first sample in this window*/
		pcpu->avg_load_maxfreq = load_at_max_freq;
		pcpu->window_size = wall_time;
	} else {
		/*
		 * The is already a sample available in this window.
		 * Compute weighted average with prev entry, so that we get
		 * the precise weighted load.
		 */
		pcpu->avg_load_maxfreq =
		    ((pcpu->avg_load_maxfreq * pcpu->window_size) +
			(load_at_max_freq * wall_time)) /
			(wall_time + pcpu->window_size);

		pcpu->window_size += wall_time;
	}
#else // debug
	pcpu->avg_load_maxfreq = load_at_max_freq;
	pcpu->window_size = wall_time;
#endif

	return 0;
}

#if 0
static unsigned int report_load_at_max_freq(bool reset)
{
	int cpu;
	struct cpu_load_data *pcpu;
	unsigned int total_load = 0;
	unsigned long flags;

	for_each_online_cpu(cpu) {
		pcpu = &per_cpu(cpuload, cpu);
		spin_lock_irqsave(&pcpu->cpu_load_lock, flags);
		update_average_load(pcpu->cur_freq, cpu, 0);
		total_load += pcpu->avg_load_maxfreq;
		if (reset)
		pcpu->avg_load_maxfreq = 0;
		spin_unlock_irqrestore(&pcpu->cpu_load_lock, flags);
	}
	return total_load;
}
#endif

unsigned int sched_get_percpu_load(int cpu, bool reset, bool use_maxfreq)
{
	struct cpu_load_data *pcpu;
	unsigned int load = 0;
    unsigned long flags;

#if 0
	if (!cpu_online(cpu))
		return 0;
#endif

	if (rq_info.init != 1)
		return 100;

	pcpu = &per_cpu(cpuload, cpu);
	spin_lock_irqsave(&pcpu->cpu_load_lock, flags);
	update_average_load(pcpu->cur_freq, cpu, use_maxfreq);
	load = pcpu->avg_load_maxfreq;
	if (reset)
		pcpu->avg_load_maxfreq = 0;
	spin_unlock_irqrestore(&pcpu->cpu_load_lock, flags);

	return load;
}
EXPORT_SYMBOL(sched_get_percpu_load);

#define HMP_RATIO 10/17
//#define DETECT_HTASK_HEAT

#ifdef DETECT_HTASK_HEAT
#define MAX_HTASK_TEMPERATURE 10
static unsigned int htask_temperature = 0;
static void __heat_refined(int *count)
{
	if (arch_is_big_little()) {
		if (*count) {
 			htask_temperature += (htask_temperature < MAX_HTASK_TEMPERATURE) ? 1 : 0;
		} else {
			*count = (htask_temperature > 0) ? 1 : 0;
			htask_temperature -= (htask_temperature > 0) ? 1 : 0;
		}
	}
}
#else
static inline void __heat_refined(int *count) {}
#endif

static void __trace_out(int heavy, int cpu, struct task_struct *p)
{
#define TRACEBUF_LEN 128
	char tracebuf[TRACEBUF_LEN];

#ifdef CONFIG_ARCH_SCALE_INVARIANT_CPU_CAPACITY
		snprintf(tracebuf, TRACEBUF_LEN, " %s cpu=%d load=%4lu cpucap=%4lu/%4lu pid=%4d name=%s",
				 heavy ? "Y" : "N",
				 cpu, p->se.avg.load_avg_ratio,
				 topology_cpu_capacity(cpu), topology_max_cpu_capacity(cpu),
				 p->pid, p->comm);
#else
		snprintf(tracebuf, TRACEBUF_LEN, " %s cpu=%d load=%4lu pid=%4d name=%s",
				 heavy ? "Y" : "N",
				 cpu, p->se.avg.load_avg_ratio,
				 p->pid, p->comm);
#endif
		trace_sched_heavy_task(tracebuf);

		if (unlikely(heavy))
			trace_sched_task_entity_avg(5, p, &p->se.avg);
}

static unsigned int htask_statistic = 0;
#ifdef CONFIG_ARCH_SCALE_INVARIANT_CPU_CAPACITY
#define OVER_L_TH(cpu) ((topology_cpu_capacity(cpu) >= topology_max_cpu_capacity(cpu)) ? 1:0)
#define OVER_B_TH(cpu) ((topology_cpu_capacity(cpu)*8 > topology_max_cpu_capacity(cpu)*5) ? 1:0)
#else
#define OVER_L_TH(cpu) (1)
#define OVER_B_TH(cpu) (1)
#endif
unsigned int sched_get_nr_heavy_task_by_threshold(unsigned int threshold)
{
	int cpu;
	struct task_struct *p;
	unsigned long flags;
	unsigned int count = 0;
	int is_heavy = 0;
	unsigned int hmp_threshold;

	if (rq_info.init != 1)
		return 0;

	for_each_online_cpu(cpu) {
		int bigcore = arch_cpu_is_big(cpu);
		hmp_threshold = bigcore ? threshold * HMP_RATIO : threshold;
		raw_spin_lock_irqsave(&cpu_rq(cpu)->lock, flags);
		list_for_each_entry(p, &cpu_rq(cpu)->cfs_tasks, se.group_node) {
			is_heavy = 0;
#ifdef CONFIG_SCHED_HMP_PRIO_FILTER
			if (task_low_priority(p->prio))
				continue;
#endif
			if (p->se.avg.load_avg_ratio >= hmp_threshold) {
				is_heavy = (!bigcore && OVER_L_TH(cpu)) || (bigcore && OVER_B_TH(cpu));
			}
			count += is_heavy ? 1 : 0;
			__trace_out(is_heavy, cpu, p);
		}
		raw_spin_unlock_irqrestore(&cpu_rq(cpu)->lock, flags);
	}

	__heat_refined(&count);
	if (count)
		htask_statistic++;
	return count;
}
EXPORT_SYMBOL(sched_get_nr_heavy_task_by_threshold);

unsigned int sched_get_nr_heavy_task(void)
{
	return sched_get_nr_heavy_task_by_threshold(heavy_task_threshold);
}
EXPORT_SYMBOL(sched_get_nr_heavy_task);

void sched_set_heavy_task_threshold(unsigned int val)
{
	heavy_task_threshold = val;
}
EXPORT_SYMBOL(sched_set_heavy_task_threshold);

#if 0
static int cpufreq_policy_handler(struct notifier_block *nb,
			unsigned long event, void *data)
{
	int cpu = 0;
	struct cpufreq_policy *policy = data;
	struct cpu_load_data *this_cpu;

	if (event == CPUFREQ_START)
		return 0;

	if (event != CPUFREQ_INCOMPATIBLE)
		return 0;

	/* Make sure that all CPUs impacted by this policy are
	 * updated since we will only get a notification when the
	 * user explicitly changes the policy on a CPU.
	 */
	for_each_cpu(cpu, policy->cpus) {
		struct cpu_load_data *pcpu = &per_cpu(cpuload, j);
		spin_lock_irqsave(&pcpu->cpu_load_lock, flags);
		pcpu->policy_max = policy->cpuinfo.max_freq;
		spin_unlock_irqrestore(&pcpu->cpu_load_lock, flags);
	}

	return 0;
}
#endif

static int cpufreq_transition_handler(struct notifier_block *nb,
			unsigned long val, void *data)
{
#if 1
	struct cpufreq_freqs *freqs = data;
	struct cpu_load_data *this_cpu = &per_cpu(cpuload, freqs->cpu);
	int j;
	unsigned long flags;

	if (rq_info.init != 1)
		return 0;

	switch (val) {
	case CPUFREQ_POSTCHANGE:
		for_each_cpu(j, this_cpu->related_cpus) {
			struct cpu_load_data *pcpu = &per_cpu(cpuload, j);
				// flush previous laod
			spin_lock_irqsave(&pcpu->cpu_load_lock, flags);
				if (cpu_online(j))
					update_average_load(freqs->old, freqs->cpu, 0);
			pcpu->cur_freq = freqs->new;
			spin_unlock_irqrestore(&pcpu->cpu_load_lock, flags);
		}
		break;
	}
#endif
	return 0;
}

static int cpu_hotplug_handler(struct notifier_block *nb,
			unsigned long val, void *data)
{
#if 1
	unsigned int cpu = (unsigned long)data;
	struct cpu_load_data *this_cpu = &per_cpu(cpuload, cpu);
    unsigned long flags;

	if (rq_info.init != 1)
		return NOTIFY_OK;

	switch (val) {
	case CPU_UP_PREPARE:
		// cpu_online()=0 here, count cpu offline period as idle
		spin_lock_irqsave(&this_cpu->cpu_load_lock, flags);
		update_average_load(0, cpu, 0);
		spin_unlock_irqrestore(&this_cpu->cpu_load_lock, flags);
		break;
	case CPU_DOWN_PREPARE:
		// cpu_online()=1 here, flush previous load
		spin_lock_irqsave(&this_cpu->cpu_load_lock, flags);
		update_average_load(this_cpu->cur_freq, cpu, 0);
		spin_unlock_irqrestore(&this_cpu->cpu_load_lock, flags);
		break;
	}
#endif
	return NOTIFY_OK;
}

static int system_suspend_handler(struct notifier_block *nb,
				unsigned long val, void *data)
{
	switch (val) {
	case PM_POST_HIBERNATION:
	case PM_POST_SUSPEND:
	case PM_POST_RESTORE:
		rq_info.hotplug_disabled = 0;
		break;
	case PM_HIBERNATION_PREPARE:
	case PM_SUSPEND_PREPARE:
		rq_info.hotplug_disabled = 1;
		break;
	default:
		return NOTIFY_DONE;
	}
	return NOTIFY_OK;
}


static ssize_t hotplug_disable_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	unsigned int val = 0;
	val = rq_info.hotplug_disabled;
	return snprintf(buf, MAX_LONG_SIZE, "%d\n", val);
}

static struct kobj_attribute hotplug_disabled_attr = __ATTR_RO(hotplug_disable);

#ifdef CONFIG_MTK_SCHED_RQAVG_US_ENABLE_WQ
static void def_work_fn(struct work_struct *work)
{
	int64_t diff;

	diff = ktime_to_ns(ktime_get()) - rq_info.def_start_time;
	do_div(diff, 1000 * 1000);
	rq_info.def_interval = (unsigned int) diff;

	/* Notify polling threads on change of value */
	sysfs_notify(rq_info.kobj, NULL, "def_timer_ms");
}
#endif

static ssize_t run_queue_avg_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	unsigned int val = 0;
	unsigned long flags = 0;

	spin_lock_irqsave(&rq_lock, flags);
	/* rq avg currently available only on one core */
	val = rq_info.rq_avg;
	rq_info.rq_avg = 0;
	spin_unlock_irqrestore(&rq_lock, flags);

	return snprintf(buf, PAGE_SIZE, "%d.%d\n", val/10, val%10);
}

static struct kobj_attribute run_queue_avg_attr = __ATTR_RO(run_queue_avg);

static ssize_t show_run_queue_poll_ms(struct kobject *kobj,
				      struct kobj_attribute *attr, char *buf)
{
	int ret = 0;
	unsigned long flags = 0;

	spin_lock_irqsave(&rq_lock, flags);
	ret = snprintf(buf, MAX_LONG_SIZE, "%u\n",
		       jiffies_to_msecs(rq_info.rq_poll_jiffies));
	spin_unlock_irqrestore(&rq_lock, flags);

	return ret;
}

static ssize_t store_run_queue_poll_ms(struct kobject *kobj,
				       struct kobj_attribute *attr,
				       const char *buf, size_t count)
{
	unsigned int val = 0;
	unsigned long flags = 0;
	static DEFINE_MUTEX(lock_poll_ms);

	mutex_lock(&lock_poll_ms);

	spin_lock_irqsave(&rq_lock, flags);
	sscanf(buf, "%u", &val);
	rq_info.rq_poll_jiffies = msecs_to_jiffies(val);
	spin_unlock_irqrestore(&rq_lock, flags);

	mutex_unlock(&lock_poll_ms);

	return count;
}

static struct kobj_attribute run_queue_poll_ms_attr =
	__ATTR(run_queue_poll_ms, S_IWUSR | S_IRUSR, show_run_queue_poll_ms,
       store_run_queue_poll_ms);

static ssize_t show_def_timer_ms(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	return snprintf(buf, MAX_LONG_SIZE, "%u\n", rq_info.def_interval);
}

static ssize_t store_def_timer_ms(struct kobject *kobj,
				  struct kobj_attribute *attr, const char *buf, size_t count)
{
	unsigned int val = 0;

	sscanf(buf, "%u", &val);
	rq_info.def_timer_jiffies = msecs_to_jiffies(val);

	rq_info.def_start_time = ktime_to_ns(ktime_get());
	return count;
}

static ssize_t store_heavy_task_threshold(struct kobject *kobj,
		struct kobj_attribute *attr, const char *buf, size_t count)
{
	unsigned int val = 0;

	sscanf(buf, "%u", &val);
	sched_set_heavy_task_threshold(val);

	return count;
}

static struct kobj_attribute def_timer_ms_attr =
	__ATTR(def_timer_ms, S_IWUSR | S_IRUSR, show_def_timer_ms,
       store_def_timer_ms);

static ssize_t show_cpu_normalized_load(struct kobject *kobj,
					struct kobj_attribute *attr, char *buf)
{
	int cpu;
	unsigned int len = 0;
	unsigned int load = 0;
	unsigned int max_len = 4096;

	//len = snprintf(buf, MAX_LONG_SIZE, "%u\n", report_load_at_max_freq(0));
	for_each_possible_cpu(cpu) {
		// reset cpu load
		//load = sched_get_percpu_load(cpu, 1, 0);
		// not reset
		load = sched_get_percpu_load(cpu, 0, 0);
		len += snprintf(buf+len, max_len-len, "cpu(%d)=%d\n", cpu, load);
#if 0
		unsigned int idle_time, wall_time, iowait_time;
		struct cpu_load_data *pcpu = &per_cpu(cpuload, cpu);

		idle_time = get_cpu_idle_time(cpu, &wall_time, 0);
		iowait_time = get_cpu_iowait_time(cpu, &wall_time);
		len += snprintf(buf+len, max_len-len, "curr idle=%u, io=%u, wall=%u\n",
			(unsigned int)idle_time,
			(unsigned int)iowait_time,
			(unsigned int)wall_time);
		len += snprintf(buf+len, max_len-len, "prev idle=%u, io=%u, wall=%u, l=%u, w=%u, f=%u m=%u, %s\n",
			(unsigned int)pcpu->prev_cpu_idle,
			(unsigned int)pcpu->prev_cpu_iowait,
			(unsigned int)pcpu->prev_cpu_wall,
			pcpu->avg_load_maxfreq,
			pcpu->window_size,
			pcpu->cur_freq,
			pcpu->policy_max,
			(unsigned int)(cpu_online(cpu))?"on":"off");
#endif
	}
	len += snprintf(buf+len, max_len-len, "htask_threshold=%d, current_htask#=%u, total_htask#=%d\n",
		heavy_task_threshold, sched_get_nr_heavy_task(), htask_statistic);

	return len;
}

static struct kobj_attribute cpu_normalized_load_attr =
	__ATTR(cpu_normalized_load, S_IWUSR | S_IRUSR, show_cpu_normalized_load,
			store_heavy_task_threshold);

static struct attribute *rq_attrs[] = {
	&cpu_normalized_load_attr.attr,
	&def_timer_ms_attr.attr,
	&run_queue_avg_attr.attr,
	&run_queue_poll_ms_attr.attr,
	&hotplug_disabled_attr.attr,
	NULL,
};

static struct attribute_group rq_attr_group = {
	.attrs = rq_attrs,
};

static int init_rq_attribs(void)
{
	int err;

	rq_info.rq_avg = 0;
	rq_info.attr_group = &rq_attr_group;

	/* Create /sys/devices/system/cpu/cpu0/rq-stats/... */
	rq_info.kobj = kobject_create_and_add("rq-stats",
			&get_cpu_device(0)->kobj);
	if (!rq_info.kobj)
		return -ENOMEM;

	err = sysfs_create_group(rq_info.kobj, rq_info.attr_group);
	if (err)
		kobject_put(rq_info.kobj);
	else
		kobject_uevent(rq_info.kobj, KOBJ_ADD);

	return err;
}

static int __init rq_stats_init(void)
{
	int ret = 0;
	int i;
#if CPU_FREQ_VARIANT
	struct cpufreq_policy cpu_policy;
#endif
	/* Bail out if this is not an SMP Target */
/* FIX-ME : mark to avoid arm64 build error
	if (!is_smp()) {
		rq_info.init = 0;
		return -ENOSYS;
	}
*/

#ifdef CONFIG_MTK_SCHED_RQAVG_US_ENABLE_WQ
	rq_wq = create_singlethread_workqueue("rq_stats");
	BUG_ON(!rq_wq);
	INIT_WORK(&rq_info.def_timer_work, def_work_fn);
#endif

	spin_lock_init(&rq_lock);
	rq_info.rq_poll_jiffies = DEFAULT_RQ_POLL_JIFFIES;
	rq_info.def_timer_jiffies = DEFAULT_DEF_TIMER_JIFFIES;
	rq_info.rq_poll_last_jiffy = 0;
	rq_info.def_timer_last_jiffy = 0;
	rq_info.hotplug_disabled = 0;
	ret = init_rq_attribs();

	for_each_possible_cpu(i) {
		struct cpu_load_data *pcpu = &per_cpu(cpuload, i);
		spin_lock_init(&pcpu->cpu_load_lock);

#if CPU_FREQ_VARIANT
		cpufreq_get_policy(&cpu_policy, i);
		pcpu->policy_max = cpu_policy.cpuinfo.max_freq;
		if (cpu_online(i))
			pcpu->cur_freq = cpufreq_get(i);
		cpumask_copy(pcpu->related_cpus, cpu_policy.cpus);
#else
		pcpu->policy_max = 1;
		pcpu->cur_freq = 1;
#endif

	}
	freq_transition.notifier_call = cpufreq_transition_handler;
	//freq_policy.notifier_call = cpufreq_policy_handler;
	cpu_hotplug.notifier_call = cpu_hotplug_handler;

#if CPU_FREQ_VARIANT
	cpufreq_register_notifier(&freq_transition, CPUFREQ_TRANSITION_NOTIFIER);
#endif
	//cpufreq_register_notifier(&freq_policy, CPUFREQ_POLICY_NOTIFIER);
	register_hotcpu_notifier(&cpu_hotplug);

	rq_info.init = 1;

	return ret;
}
late_initcall(rq_stats_init);

static int __init rq_stats_early_init(void)
{

	/* Bail out if this is not an SMP Target */
/* FIX-ME : mark to avoid arm64 build error
	if (!is_smp()) {
		rq_info.init = 0;
		return -ENOSYS;
	}
*/
	pm_notifier(system_suspend_handler, 0);
	return 0;
}
core_initcall(rq_stats_early_init);
