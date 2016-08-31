/*
 * drivers/misc/cpuload.c
 *
 * Copyright (c) 2012-2014, NVIDIA CORPORATION.  All rights reserved.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/cpu.h>
#include <linux/cpumask.h>
#include <linux/cpufreq.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/sched.h>
#include <linux/tick.h>
#include <linux/timer.h>
#include <linux/workqueue.h>
#include <linux/kthread.h>
#include <linux/mutex.h>

#include <asm/cputime.h>

static atomic_t active_count = ATOMIC_INIT(0);
static unsigned int enabled;

static void cpuloadmon_enable(unsigned int state);

struct cpuloadmon_cpuinfo {
	/* cpu load */
	struct timer_list cpu_timer;
	int timer_idlecancel;
	u64 time_in_idle;
	u64 time_in_iowait;
	u64 idle_exit_time;
	u64 timer_run_time;
	int idling;
	int monitor_enabled;
	int cpu_load;

	/* runnable threads */
	u64 previous_integral;
	unsigned int avg;
	bool integral_sampled;
	u64 prev_timestamp;
};

static DEFINE_PER_CPU(struct cpuloadmon_cpuinfo, cpuinfo);

/* Consider IO as busy */
static unsigned long io_is_busy;

/*
 * The sample rate of the timer used to increase frequency
 */
#define DEFAULT_TIMER_RATE 20000;
static unsigned long timer_rate;

/* nr runnable threads */
#define NR_FSHIFT_EXP	3
#define NR_FSHIFT	(1 << NR_FSHIFT_EXP)
#define EXP    1497 /* 20 msec window */

static inline cputime64_t get_cpu_iowait_time(
	unsigned int cpu, cputime64_t *wall)
{
	u64 iowait_time = get_cpu_iowait_time_us(cpu, wall);

	if (iowait_time == -1ULL)
		return 0;

	return iowait_time;
}

static void cpuloadmon_timer(unsigned long data)
{
	unsigned int delta_idle;
	unsigned int delta_iowait;
	unsigned int delta_time;
	u64 time_in_idle;
	u64 time_in_iowait;
	u64 idle_exit_time;
	struct cpuloadmon_cpuinfo *pcpu =
		&per_cpu(cpuinfo, data);
	u64 now_idle;
	u64 now_iowait;
	u64 integral, old_integral, delta_integral, delta_time_nr, cur_time;

	smp_rmb();

	if (!pcpu->monitor_enabled)
		goto exit;

	/*
	 * Once pcpu->timer_run_time is updated to >= pcpu->idle_exit_time,
	 * this lets idle exit know the current idle time sample has
	 * been processed, and idle exit can generate a new sample and
	 * re-arm the timer.  This prevents a concurrent idle
	 * exit on that CPU from writing a new set of info at the same time
	 * the timer function runs (the timer function can't use that info
	 * until more time passes).
	 */
	time_in_idle = pcpu->time_in_idle;
	time_in_iowait = pcpu->time_in_iowait;
	idle_exit_time = pcpu->idle_exit_time;
	now_idle = get_cpu_idle_time_us(data, &pcpu->timer_run_time);
	now_iowait = get_cpu_iowait_time(data, NULL);
	smp_wmb();

	/* If we raced with cancelling a timer, skip. */
	if (!idle_exit_time)
		goto exit;

	delta_idle = (unsigned int)(now_idle - time_in_idle);
	delta_iowait = (unsigned int)(now_iowait - time_in_iowait);
	delta_time = (unsigned int)(pcpu->timer_run_time - idle_exit_time);

	/*
	 * If timer ran less than 1ms after short-term sample started, retry.
	 */
	if (delta_time < 1000)
		goto rearm;

	if (!io_is_busy)
		delta_idle += delta_iowait;

	if (delta_idle > delta_time)
		pcpu->cpu_load = 0;
	else
		pcpu->cpu_load = 100 * (delta_time - delta_idle) / delta_time;

	/* get avg nr runnables */
	integral = nr_running_integral(data);
	old_integral = pcpu->previous_integral;
	pcpu->previous_integral = integral;
	cur_time = ktime_to_ns(ktime_get());
	delta_time_nr = cur_time - pcpu->prev_timestamp;
	pcpu->prev_timestamp = cur_time;

	if (!pcpu->integral_sampled) {
		pcpu->integral_sampled = true;
		/* First sample to initialize prev_integral, skip
		 * avg calculation
		 */
	} else {
		if (integral < old_integral) {
			/* Overflow */
			delta_integral = (ULLONG_MAX - old_integral) + integral;
		} else {
			delta_integral = integral - old_integral;
		}

		/* Calculate average for the previous sample window */
		do_div(delta_integral, delta_time_nr);
		pcpu->avg = delta_integral;
	}

rearm:
	if (!timer_pending(&pcpu->cpu_timer)) {
		if (pcpu->idling)
			goto exit;

		pcpu->time_in_idle = get_cpu_idle_time_us(
			data, &pcpu->idle_exit_time);
		pcpu->time_in_iowait = get_cpu_iowait_time(
			data, NULL);

		mod_timer(&pcpu->cpu_timer,
			  jiffies + usecs_to_jiffies(timer_rate));
	}

exit:
	return;
}

static void cpuloadmon_idle_start(void)
{
	struct cpuloadmon_cpuinfo *pcpu =
		&per_cpu(cpuinfo, smp_processor_id());
	int pending;

	if (!pcpu->monitor_enabled)
		return;

	pcpu->idling = 1;
	smp_wmb();
	pending = timer_pending(&pcpu->cpu_timer);

	if (pending && pcpu->timer_idlecancel) {
		del_timer(&pcpu->cpu_timer);
		/*
		 * Ensure last timer run time is after current idle
		 * sample start time, so next idle exit will always
		 * start a new idle sampling period.
		 */
		pcpu->idle_exit_time = 0;
		pcpu->timer_idlecancel = 0;
	}
}

static void cpuloadmon_idle_end(void)
{
	struct cpuloadmon_cpuinfo *pcpu =
		&per_cpu(cpuinfo, smp_processor_id());

	if (!pcpu->monitor_enabled)
		return;

	pcpu->idling = 0;
	smp_wmb();

	/*
	 * Arm the timer for 1-2 ticks later if not already, and if the timer
	 * function has already processed the previous load sampling
	 * interval.  (If the timer is not pending but has not processed
	 * the previous interval, it is probably racing with us on another
	 * CPU.  Let it compute load based on the previous sample and then
	 * re-arm the timer for another interval when it's done, rather
	 * than updating the interval start time to be "now", which doesn't
	 * give the timer function enough time to make a decision on this
	 * run.)
	 */
	if (timer_pending(&pcpu->cpu_timer) == 0 &&
	    pcpu->timer_run_time >= pcpu->idle_exit_time &&
	    pcpu->monitor_enabled) {
		pcpu->time_in_idle =
			get_cpu_idle_time_us(smp_processor_id(),
					     &pcpu->idle_exit_time);
		pcpu->time_in_iowait =
			get_cpu_iowait_time(smp_processor_id(),
						NULL);
		pcpu->timer_idlecancel = 0;
		mod_timer(&pcpu->cpu_timer,
			  jiffies + usecs_to_jiffies(timer_rate));
	}
}

#define DECL_CPULOAD_ATTR(name) \
static ssize_t show_##name(struct kobject *kobj, \
	struct attribute *attr, char *buf) \
{ \
	return sprintf(buf, "%lu\n", name); \
} \
\
static ssize_t store_##name(struct kobject *kobj,\
		struct attribute *attr, const char *buf, size_t count) \
{ \
	int ret; \
	unsigned long val; \
\
	ret = kstrtoul(buf, 0, &val); \
	if (ret < 0) \
		return ret; \
	name = val; \
	return count; \
} \
\
static struct global_attr name##_attr = __ATTR(name, 0644, \
		show_##name, store_##name);

static ssize_t show_cpus_online(struct kobject *kobj,
		struct attribute *attr, char *buf)
{
	unsigned int i, t;
	const cpumask_t *cpus = cpu_online_mask;

	i = 0;
	for_each_cpu_mask(t, *cpus)
		i++;

	return sprintf(buf, "%u\n", i);
}

static struct global_attr cpus_online_attr = __ATTR(cpus_online, 0444,
		show_cpus_online, NULL);

static ssize_t show_cpu_load(struct kobject *kobj,
		struct attribute *attr, char *buf)
{
	unsigned int t, len, total;
	const cpumask_t *cpus = cpu_online_mask;
	struct cpuloadmon_cpuinfo *pcpu;

	total = 0;

	for_each_cpu_mask(t, *cpus) {
		pcpu = &per_cpu(cpuinfo, t);
		len = sprintf(buf, "%u %u %u\n",
			t, pcpu->cpu_load, pcpu->avg);
		total += len;
		buf = &buf[len];
	}

	return total;
}

static struct global_attr cpu_load_attr = __ATTR(cpu_load, 0444,
		show_cpu_load, NULL);

static ssize_t show_cpu_usage(struct kobject *kobj,
		struct attribute *attr, char *buf)
{
	unsigned int t, len, total;
	const cpumask_t *cpus = cpu_online_mask;
	struct cpuloadmon_cpuinfo *pcpu;

	total = 0;

	for_each_cpu_mask(t, *cpus) {
		pcpu = &per_cpu(cpuinfo, t);
		len = sprintf(buf, "%u %u %llu %llu %llu\n",
			      t, pcpu->avg,
			      ktime_to_us(ktime_get()),
			      get_cpu_idle_time_us(t, NULL),
			      get_cpu_iowait_time_us(t, NULL));
		total += len;
		buf = &buf[len];
	}

	return total;
}

static struct global_attr cpu_usage_attr = __ATTR(cpu_usage, 0444,
		show_cpu_usage, NULL);

static ssize_t show_enable(struct kobject *kobj,
		struct attribute *attr, char *buf)
{
	return sprintf(buf, "%u\n", enabled);
}

static ssize_t store_enable(struct kobject *kobj,
		struct attribute *attr, const char *buf, size_t count)
{
	int ret;
	unsigned long val;
	unsigned int before = enabled;

	ret = kstrtoul(buf, 0, &val);
	if (ret < 0)
		return ret;
	enabled = !!val;	/* normalize user input */
	if (before != enabled)
			cpuloadmon_enable(enabled);

	return count;
}
static struct global_attr enable_attr = __ATTR(enable, 0644,
		show_enable, store_enable);

DECL_CPULOAD_ATTR(io_is_busy)
DECL_CPULOAD_ATTR(timer_rate)
#undef DECL_CPULOAD_ATTR

static struct attribute *cpuload_attributes[] = {
	&io_is_busy_attr.attr,
	&timer_rate_attr.attr,
	&cpus_online_attr.attr,
	&cpu_load_attr.attr,
	&cpu_usage_attr.attr,
	&enable_attr.attr,
	NULL,
};

static struct attribute_group cpuload_attr_group = {
	.attrs = cpuload_attributes,
	.name = "cpuload",
};

static int cpuloadmon_idle_notifier(struct notifier_block *nb,
					     unsigned long val,
					     void *data)
{
	switch (val) {
	case IDLE_START:
		cpuloadmon_idle_start();
		break;
	case IDLE_END:
		cpuloadmon_idle_end();
		break;
	}

	return 0;
}

static struct notifier_block cpuloadmon_idle_nb = {
	.notifier_call = cpuloadmon_idle_notifier,
};

static void cpuloadmon_enable(unsigned int state)
{
	unsigned int j;
	struct cpuloadmon_cpuinfo *pcpu;
	const cpumask_t *cpus = cpu_possible_mask;

	if (state) {
		u64 last_update;

		for_each_cpu(j, cpus) {
			pcpu = &per_cpu(cpuinfo, j);
			pcpu->time_in_idle =
				get_cpu_idle_time_us(j, &last_update);
			pcpu->idle_exit_time = last_update;
			pcpu->time_in_iowait =
				get_cpu_iowait_time(j, NULL);
			pcpu->timer_idlecancel = 1;
			pcpu->monitor_enabled = 1;
			smp_wmb();

			if (!timer_pending(&pcpu->cpu_timer))
				mod_timer(&pcpu->cpu_timer, jiffies + 2);
		}
	} else {
		for_each_cpu(j, cpus) {
			pcpu = &per_cpu(cpuinfo, j);
			pcpu->monitor_enabled = 0;
			smp_wmb();
			del_timer_sync(&pcpu->cpu_timer);

			/*
			 * Reset idle exit time since we may cancel the timer
			 * before it can run after the last idle exit time,
			 * to avoid tripping the check in idle exit for a timer
			 * that is trying to run.
			 */
			pcpu->idle_exit_time = 0;
		}
	}

	enabled = state;
}

static int cpuloadmon_start(void)
{
	int rc;

	cpuloadmon_enable(1);

	/*
	 * Do not register the idle hook and create sysfs
	 * entries if we have already done so.
	 */
	if (atomic_inc_return(&active_count) > 1)
		return 0;

	rc = sysfs_create_group(cpufreq_global_kobject,
			&cpuload_attr_group);
	if (rc)
		return rc;

	idle_notifier_register(&cpuloadmon_idle_nb);

	return 0;
}

static int cpuloadmon_stop(void)
{
	cpuloadmon_enable(0);

	if (atomic_dec_return(&active_count) > 0)
		return 0;

	idle_notifier_unregister(&cpuloadmon_idle_nb);
	sysfs_remove_group(cpufreq_global_kobject,
			&cpuload_attr_group);

	return 0;
}

static int __init cpuload_monitor_init(void)
{
	unsigned int i;
	struct cpuloadmon_cpuinfo *pcpu;

	timer_rate = DEFAULT_TIMER_RATE;

	/* Initalize per-cpu timers */
	for_each_possible_cpu(i) {
		pcpu = &per_cpu(cpuinfo, i);
		init_timer(&pcpu->cpu_timer);
		pcpu->cpu_timer.function = cpuloadmon_timer;
		pcpu->cpu_timer.data = i;
	}

	cpuloadmon_start();

	/* disable by default */
	cpuloadmon_enable(0);

	return 0;
}

module_init(cpuload_monitor_init);

static void __exit cpuload_monitor_exit(void)
{
	cpuloadmon_stop();
}

module_exit(cpuload_monitor_exit);

MODULE_AUTHOR("Ilan Aelion <iaelion@nvidia.com>");
MODULE_DESCRIPTION("'cpuload_monitor' - A cpu load monitor");
MODULE_LICENSE("GPL");
