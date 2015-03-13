/*
 * Copyright (c) 2013-2015, Francisco Franco <franciscofranco.1990@gmail.com>.
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
 * Simple no bullshit hot[un]plug driver for SMP
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/miscdevice.h>
#include <linux/cpu.h>
#include <linux/workqueue.h>
#include <linux/sched.h>
#include <linux/platform_device.h>
#include <linux/timer.h>
#include <linux/cpufreq.h>
#include <linux/delay.h>
#include <linux/input.h>
#include <linux/jiffies.h>

#define MAKO_HOTPLUG "mako_hotplug"

#define DEFAULT_LOAD_THRESHOLD 80
#define DEFAULT_HIGH_LOAD_COUNTER 10
#define DEFAULT_MAX_LOAD_COUNTER 20
#define DEFAULT_CPUFREQ_UNPLUG_LIMIT 1800000
#define DEFAULT_MIN_TIME_CPU_ONLINE 1
#define DEFAULT_TIMER 1

#define MIN_CPU_UP_US (200 * USEC_PER_MSEC)
#define NUM_POSSIBLE_CPUS num_possible_cpus()
#define HIGH_LOAD (95)

struct cpu_stats {
	unsigned int counter;
	u64 timestamp;
} stats = {
	.counter = 0,
	.timestamp = 0,
};

struct hotplug_tunables {
	/*
	 * system load threshold to decide when online or offline cores
	 * from 0 to 100
	 */
	unsigned int load_threshold;

	/*
	 * counter to filter online/offline calls. The load needs to be above
	 * load_threshold X high_load_counter times for the cores to go online
	 * otherwise they stay offline
	 */
	unsigned int high_load_counter;

	/*
	 * max number of samples counters allowed to be counted. The higher the
	 * value the longer it will take the driver to offline cores after a
	 * period of high and continuous load
	 */
	unsigned int max_load_counter;

	/*
	 * if the current CPU freq is above this limit don't offline the cores
	 * for a couple of extra samples
	 */
	unsigned int cpufreq_unplug_limit;

	/*
	 * minimum time in seconds that a core stays online to avoid too many
	 * online/offline calls
	 */
	unsigned int min_time_cpu_online;

	/*
	 * sample timer in seconds. The default value of 1 equals to 10
	 * samples every second. The higher the value the less samples
	 * per second it runs
	 */
	unsigned int timer;
} tunables;

static struct workqueue_struct *wq;
static struct delayed_work decide_hotplug;

static inline void cpus_online_work(void)
{
	unsigned int cpu;

	for (cpu = 2; cpu < 4; cpu++) {
		if (cpu_is_offline(cpu))
			cpu_up(cpu);
	}

	pr_info("%s: all cpus were onlined\n", MAKO_HOTPLUG);
}

static inline void cpus_offline_work(void)
{
	unsigned int cpu;

	for (cpu = 3; cpu > 1; cpu--) {
		if (cpu_online(cpu))
			cpu_down(cpu);
	}

	pr_info("%s: all cpus were offlined\n", MAKO_HOTPLUG);
}

static inline bool cpus_cpufreq_work(void)
{
	struct cpufreq_policy *policy = cpufreq_cpu_get(0);
	struct hotplug_tunables *t = &tunables;
	unsigned int current_freq = 0;
	unsigned int cpu;

	if (policy) {
		if (policy->min >= t->cpufreq_unplug_limit)
			return false;
	}

	for (cpu = 2; cpu < 4; cpu++)
		current_freq += cpufreq_quick_get(cpu);

	current_freq >>= 1;

	return current_freq >= t->cpufreq_unplug_limit;
}

static void cpu_revive(unsigned int load)
{
	struct hotplug_tunables *t = &tunables;
	unsigned int counter_hysteria = 3;

	if (unlikely(nr_running() >= 10))
		goto online_all;

	/*
	 * we should care about a very high load spike and online the
	 * cpus in question. If the device is under stress for at least 300ms
	 * online all cores, no questions asked. 300ms here equals three samples
	 */
	if (load >= HIGH_LOAD && stats.counter >= counter_hysteria)
		goto online_all;
	else if (stats.counter < t->high_load_counter)
		return;

online_all:
	cpus_online_work();
	stats.timestamp = ktime_to_us(ktime_get());
}

static void cpu_smash(void)
{
	struct hotplug_tunables *t = &tunables;
	u64 extra_time = MIN_CPU_UP_US;

	if (stats.counter >= t->high_load_counter)
		return;

	/*
	 * offline the cpu only if its freq is lower than
	 * CPUFREQ_UNPLUG_LIMIT. Else update the timestamp to now and
	 * postpone the cpu offline process to at least another second
	 */
	if (cpus_cpufreq_work())
		stats.timestamp = ktime_to_us(ktime_get());

	/*
	 * Let's not unplug this cpu unless its been online for longer than
	 * 500ms to avoid consecutive ups and downs if the load is varying
	 * closer to the threshold point.
	 */
	if (t->min_time_cpu_online > 1)
		extra_time = t->min_time_cpu_online * MIN_CPU_UP_US;

	if (ktime_to_us(ktime_get()) < stats.timestamp + extra_time)
		return;

	cpus_offline_work();

	/*
	 * reset the counter yo
	 */
	stats.counter = 0;
}

static void __ref decide_hotplug_func(struct work_struct *work)
{
	struct hotplug_tunables *t = &tunables;
	unsigned int cur_load = 0;
	unsigned int cpu;
	unsigned int online_cpus = num_online_cpus();

	/*
	 * reschedule early when the user doesn't want more than 2 cores online
	 */
	if (unlikely(t->load_threshold == 100 && online_cpus == 2))
		goto reschedule;

	/*
	 * reschedule early when users desire to run with all cores online
	 */
	if (unlikely(!t->load_threshold &&
			online_cpus == NUM_POSSIBLE_CPUS))
		goto reschedule;

	for (cpu = 0; cpu < 2; cpu++)
		cur_load += cpufreq_quick_get_util(cpu);

	cur_load >>= 1;

	if (cur_load >= t->load_threshold) {
		if (stats.counter < t->max_load_counter)
			++stats.counter;

		if (online_cpus <= 2)
			cpu_revive(cur_load);
	} else {
		if (stats.counter)
			--stats.counter;

		if (online_cpus > 2)
			cpu_smash();
	}

reschedule:
	queue_delayed_work(wq, &decide_hotplug,
		msecs_to_jiffies(t->timer * HZ));
}

/*
 * Sysfs get/set entries start
 */

static ssize_t load_threshold_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct hotplug_tunables *t = &tunables;

	return snprintf(buf, PAGE_SIZE, "%u\n", t->load_threshold);
}

static ssize_t load_threshold_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	struct hotplug_tunables *t = &tunables;
	int ret;
	unsigned long new_val;

	ret = kstrtoul(buf, 0, &new_val);
	if (ret < 0)
		return ret;

	t->load_threshold = new_val > 100 ? 100 : new_val;

	return size;
}

static ssize_t high_load_counter_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct hotplug_tunables *t = &tunables;

	return snprintf(buf, PAGE_SIZE, "%u\n", t->high_load_counter);
}

static ssize_t high_load_counter_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	struct hotplug_tunables *t = &tunables;
	int ret;
	unsigned long new_val;

	ret = kstrtoul(buf, 0, &new_val);
	if (ret < 0)
		return ret;

	t->high_load_counter = new_val > 50 ? 50 : new_val;

	return size;
}

static ssize_t max_load_counter_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct hotplug_tunables *t = &tunables;

	return snprintf(buf, PAGE_SIZE, "%u\n", t->max_load_counter);
}

static ssize_t max_load_counter_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	struct hotplug_tunables *t = &tunables;
	int ret;
	unsigned long new_val;

	ret = kstrtoul(buf, 0, &new_val);
	if (ret < 0)
		return ret;

	t->max_load_counter = new_val > 50 ? 50 : new_val;

	return size;
}

static ssize_t cpufreq_unplug_limit_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct hotplug_tunables *t = &tunables;

	return snprintf(buf, PAGE_SIZE, "%u\n", t->cpufreq_unplug_limit);
}

static ssize_t cpufreq_unplug_limit_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	struct hotplug_tunables *t = &tunables;
	int ret;
	unsigned long new_val;

	ret = kstrtoul(buf, 0, &new_val);
	if (ret < 0)
		return ret;

	t->cpufreq_unplug_limit = new_val > ULONG_MAX ? ULONG_MAX : new_val;

	return size;
}

static ssize_t min_time_cpu_online_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct hotplug_tunables *t = &tunables;

	return snprintf(buf, PAGE_SIZE, "%u\n", t->min_time_cpu_online);
}

static ssize_t min_time_cpu_online_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	struct hotplug_tunables *t = &tunables;
	int ret;
	unsigned long new_val;

	ret = kstrtoul(buf, 0, &new_val);
	if (ret < 0)
		return ret;

	t->min_time_cpu_online = new_val > 10 ? 10 : new_val;

	return size;
}

static ssize_t timer_show(struct device *dev, struct device_attribute *attr,
		char *buf)
{
	struct hotplug_tunables *t = &tunables;

	return snprintf(buf, PAGE_SIZE, "%u\n", t->timer);
}

static ssize_t timer_store(struct device *dev, struct device_attribute *attr,
		const char *buf, size_t size)
{
	struct hotplug_tunables *t = &tunables;
	int ret;
	unsigned long new_val;

	ret = kstrtoul(buf, 0, &new_val);
	if (ret < 0)
		return ret;

	t->timer = new_val > 100 ? 100 : new_val;

	return size;
}

static DEVICE_ATTR(load_threshold, 0664, load_threshold_show,
		load_threshold_store);
static DEVICE_ATTR(high_load_counter, 0664, high_load_counter_show,
		high_load_counter_store);
static DEVICE_ATTR(max_load_counter, 0664, max_load_counter_show,
		max_load_counter_store);
static DEVICE_ATTR(cpufreq_unplug_limit, 0664, cpufreq_unplug_limit_show,
		cpufreq_unplug_limit_store);
static DEVICE_ATTR(min_time_cpu_online, 0664, min_time_cpu_online_show,
		min_time_cpu_online_store);
static DEVICE_ATTR(timer, 0664, timer_show, timer_store);

static struct attribute *mako_hotplug_control_attributes[] = {
	&dev_attr_load_threshold.attr,
	&dev_attr_high_load_counter.attr,
	&dev_attr_max_load_counter.attr,
	&dev_attr_cpufreq_unplug_limit.attr,
	&dev_attr_min_time_cpu_online.attr,
	&dev_attr_timer.attr,
	NULL
};

static struct attribute_group mako_hotplug_control_group = {
	.attrs  = mako_hotplug_control_attributes,
};

static struct miscdevice mako_hotplug_control_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "mako_hotplug_control",
};

/*
 * Sysfs get/set entries end
 */

static int mako_hotplug_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct hotplug_tunables *t = &tunables;

	wq = alloc_workqueue("mako_hotplug_workqueue",
		WQ_FREEZABLE |
		WQ_UNBOUND, 1);

	if (!wq) {
		ret = -ENOMEM;
		goto err;
	}

	t->load_threshold = DEFAULT_LOAD_THRESHOLD;
	t->high_load_counter = DEFAULT_HIGH_LOAD_COUNTER;
	t->max_load_counter = DEFAULT_MAX_LOAD_COUNTER;
	t->cpufreq_unplug_limit = DEFAULT_CPUFREQ_UNPLUG_LIMIT;
	t->min_time_cpu_online = DEFAULT_MIN_TIME_CPU_ONLINE;
	t->timer = DEFAULT_TIMER;

	ret = misc_register(&mako_hotplug_control_device);
	if (ret) {
		ret = -EINVAL;
		goto err;
	}

	ret = sysfs_create_group(&mako_hotplug_control_device.this_device->kobj,
			&mako_hotplug_control_group);
	if (ret) {
		ret = -EINVAL;
		goto err;
	}

	INIT_DELAYED_WORK(&decide_hotplug, decide_hotplug_func);

	queue_delayed_work(wq, &decide_hotplug, HZ * 30);
err:
	return ret;
}

static struct platform_device mako_hotplug_device = {
	.name = MAKO_HOTPLUG,
	.id = -1,
};

static int mako_hotplug_remove(struct platform_device *pdev)
{
	destroy_workqueue(wq);

	return 0;
}

static struct platform_driver mako_hotplug_driver = {
	.probe = mako_hotplug_probe,
	.remove = mako_hotplug_remove,
	.driver = {
		.name = MAKO_HOTPLUG,
		.owner = THIS_MODULE,
	},
};

static int __init mako_hotplug_init(void)
{
	int ret;

	ret = platform_driver_register(&mako_hotplug_driver);
	if (ret)
		return ret;

	ret = platform_device_register(&mako_hotplug_device);
	if (ret)
		return ret;

	pr_info("%s: init\n", MAKO_HOTPLUG);

	return ret;
}

static void __exit mako_hotplug_exit(void)
{
	platform_device_unregister(&mako_hotplug_device);
	platform_driver_unregister(&mako_hotplug_driver);
}

late_initcall(mako_hotplug_init);
module_exit(mako_hotplug_exit);
