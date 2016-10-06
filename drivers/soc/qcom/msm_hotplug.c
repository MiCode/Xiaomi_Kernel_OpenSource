/*
 * MSM Hotplug Driver
 *
 * Copyright (c) 2013-2015, Pranav Vashi <neobuddy89@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/cpu.h>
#include <linux/init.h>
#include <linux/workqueue.h>
#include <linux/sched.h>
#include <linux/platform_device.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/cpufreq.h>
#ifdef CONFIG_STATE_NOTIFIER
#include <linux/state_notifier.h>
#endif
#include <linux/mutex.h>
#include <linux/input.h>
#include <linux/math64.h>
#include <linux/kernel_stat.h>
#include <linux/tick.h>

#define MSM_HOTPLUG			"msm_hotplug"
#define HOTPLUG_ENABLED			1
#define DEFAULT_UPDATE_RATE		HZ / 10
#define START_DELAY			HZ * 20
#define MIN_INPUT_INTERVAL		150 * 1000L
#define DEFAULT_HISTORY_SIZE		10
#define DEFAULT_DOWN_LOCK_DUR		1000
#define DEFAULT_BOOST_LOCK_DUR		2500 * 1000L
#define DEFAULT_NR_CPUS_BOOSTED		NR_CPUS / 2
#define DEFAULT_MIN_CPUS_ONLINE		1
#define DEFAULT_MAX_CPUS_ONLINE		NR_CPUS
#define DEFAULT_FAST_LANE_LOAD		99
#define DEFAULT_MAX_CPUS_ONLINE_SUSP	1

static unsigned int debug = 0;
module_param_named(debug_mask, debug, uint, 0644);

#define dprintk(msg...)		\
do { 				\
	if (debug)		\
		pr_info(msg);	\
} while (0)

static struct cpu_hotplug {
	unsigned int msm_enabled;
	unsigned int suspended;
	unsigned int min_cpus_online_res;
	unsigned int max_cpus_online_res;
	unsigned int max_cpus_online_susp;
	unsigned int target_cpus;
	unsigned int min_cpus_online;
	unsigned int max_cpus_online;
	unsigned int cpus_boosted;
	unsigned int offline_load;
	unsigned int down_lock_dur;
	u64 boost_lock_dur;
	u64 last_input;
	unsigned int fast_lane_load;
	struct work_struct up_work;
	struct work_struct down_work;
	struct mutex msm_hotplug_mutex;
	struct notifier_block notif;
} hotplug = {
	.msm_enabled = HOTPLUG_ENABLED,
	.min_cpus_online = DEFAULT_MIN_CPUS_ONLINE,
	.max_cpus_online = DEFAULT_MAX_CPUS_ONLINE,
	.suspended = 0,
	.min_cpus_online_res = DEFAULT_MIN_CPUS_ONLINE,
	.max_cpus_online_res = DEFAULT_MAX_CPUS_ONLINE,
	.max_cpus_online_susp = DEFAULT_MAX_CPUS_ONLINE_SUSP,
	.cpus_boosted = DEFAULT_NR_CPUS_BOOSTED,
	.down_lock_dur = DEFAULT_DOWN_LOCK_DUR,
	.boost_lock_dur = DEFAULT_BOOST_LOCK_DUR,
	.fast_lane_load = DEFAULT_FAST_LANE_LOAD
};

static struct workqueue_struct *hotplug_wq;
static struct delayed_work hotplug_work;

static u64 last_boost_time;
static unsigned int default_update_rates[] = { DEFAULT_UPDATE_RATE };

static struct cpu_stats {
	unsigned int *update_rates;
	int nupdate_rates;
	spinlock_t update_rates_lock;
	unsigned int *load_hist;
	unsigned int hist_size;
	unsigned int hist_cnt;
	unsigned int min_cpus;
	unsigned int total_cpus;
	unsigned int online_cpus;
	unsigned int cur_avg_load;
	unsigned int cur_max_load;
	struct mutex stats_mutex;
} stats = {
	.update_rates = default_update_rates,
	.nupdate_rates = ARRAY_SIZE(default_update_rates),
	.hist_size = DEFAULT_HISTORY_SIZE,
	.min_cpus = 1,
	.total_cpus = NR_CPUS
};

struct down_lock {
	unsigned int locked;
	struct delayed_work lock_rem;
};

static DEFINE_PER_CPU(struct down_lock, lock_info);

struct cpu_load_data {
	u64 prev_cpu_idle;
	u64 prev_cpu_wall;
	unsigned int avg_load_maxfreq;
	unsigned int cur_load_maxfreq;
	unsigned int samples;
	unsigned int window_size;
	cpumask_var_t related_cpus;
};

static DEFINE_PER_CPU(struct cpu_load_data, cpuload);

static bool io_is_busy;

static int update_average_load(unsigned int cpu)
{
	int ret;
	unsigned int idle_time, wall_time;
	unsigned int cur_load, load_max_freq;
	u64 cur_wall_time, cur_idle_time;
	struct cpu_load_data *pcpu = &per_cpu(cpuload, cpu);
	struct cpufreq_policy policy;

	ret = cpufreq_get_policy(&policy, cpu);
	if (ret)
		return -EINVAL;

	cur_idle_time = get_cpu_idle_time(cpu, &cur_wall_time, io_is_busy);

	wall_time = (unsigned int) (cur_wall_time - pcpu->prev_cpu_wall);
	pcpu->prev_cpu_wall = cur_wall_time;

	idle_time = (unsigned int) (cur_idle_time - pcpu->prev_cpu_idle);
	pcpu->prev_cpu_idle = cur_idle_time;

	if (unlikely(!wall_time || wall_time < idle_time))
		return 0;

	cur_load = 100 * (wall_time - idle_time) / wall_time;

	/* Calculate the scaled load across cpu */
	load_max_freq = (cur_load * policy.cur) / policy.max;

	if (!pcpu->avg_load_maxfreq) {
		/* This is the first sample in this window */
		pcpu->avg_load_maxfreq = load_max_freq;
		pcpu->window_size = wall_time;
	} else {
		/*
		 * The is already a sample available in this window.
		 * Compute weighted average with prev entry, so that
		 * we get the precise weighted load.
		 */
		pcpu->avg_load_maxfreq =
			((pcpu->avg_load_maxfreq * pcpu->window_size) +
			(load_max_freq * wall_time)) /
			(wall_time + pcpu->window_size);

		pcpu->window_size += wall_time;
	}

	return 0;
}

static unsigned int load_at_max_freq(void)
{
	int cpu;
	unsigned int total_load = 0, max_load = 0;
	struct cpu_load_data *pcpu;

	for_each_online_cpu(cpu) {
		pcpu = &per_cpu(cpuload, cpu);
		update_average_load(cpu);
		total_load += pcpu->avg_load_maxfreq;
		pcpu->cur_load_maxfreq = pcpu->avg_load_maxfreq;
		max_load = max(max_load, pcpu->avg_load_maxfreq);
		pcpu->avg_load_maxfreq = 0;
	}
	stats.cur_max_load = max_load;

	return total_load;
}
static void update_load_stats(void)
{
	unsigned int i, j;
	unsigned int load = 0;

	mutex_lock(&stats.stats_mutex);
	stats.online_cpus = num_online_cpus();

	if (stats.hist_size > 1) {
		stats.load_hist[stats.hist_cnt] = load_at_max_freq();
	} else {
		stats.cur_avg_load = load_at_max_freq();
		mutex_unlock(&stats.stats_mutex);
		return;
	}

	for (i = 0, j = stats.hist_cnt; i < stats.hist_size; i++, j--) {
		load += stats.load_hist[j];

		if (j == 0)
			j = stats.hist_size;
	}

	if (++stats.hist_cnt == stats.hist_size)
		stats.hist_cnt = 0;

	stats.cur_avg_load = load / stats.hist_size;
	mutex_unlock(&stats.stats_mutex);
}

struct loads_tbl {
	unsigned int up_threshold;
	unsigned int down_threshold;
};

#define LOAD_SCALE(u, d)     \
{                            \
	.up_threshold = u,   \
	.down_threshold = d, \
}

static struct loads_tbl loads[] = {
	LOAD_SCALE(400, 0),
	LOAD_SCALE(65, 0),
	LOAD_SCALE(120, 50),
	LOAD_SCALE(190, 100),
	LOAD_SCALE(410, 170),
	LOAD_SCALE(0, 0),
};

static void apply_down_lock(unsigned int cpu)
{
	struct down_lock *dl = &per_cpu(lock_info, cpu);

	dl->locked = 1;
	queue_delayed_work_on(0, hotplug_wq, &dl->lock_rem,
			      msecs_to_jiffies(hotplug.down_lock_dur));
}

static void remove_down_lock(struct work_struct *work)
{
	struct down_lock *dl = container_of(work, struct down_lock,
					    lock_rem.work);
	dl->locked = 0;
}

static int check_down_lock(unsigned int cpu)
{
	struct down_lock *dl = &per_cpu(lock_info, cpu);

	return dl->locked;
}

static int get_lowest_load_cpu(void)
{
	int cpu, lowest_cpu = 0;
	unsigned int lowest_load = UINT_MAX;
	unsigned int cpu_load[stats.total_cpus];
	unsigned int proj_load;
	struct cpu_load_data *pcpu;

	for_each_online_cpu(cpu) {
		if (cpu == 0)
			continue;
		pcpu = &per_cpu(cpuload, cpu);
		cpu_load[cpu] = pcpu->cur_load_maxfreq;
		if (cpu_load[cpu] < lowest_load) {
			lowest_load = cpu_load[cpu];
			lowest_cpu = cpu;
		}
	}

	proj_load = stats.cur_avg_load - lowest_load;
	if (proj_load > loads[stats.online_cpus - 1].up_threshold)
		return -EPERM;

	if (hotplug.offline_load && lowest_load >= hotplug.offline_load)
		return -EPERM;

	return lowest_cpu;
}

static void __ref cpu_up_work(struct work_struct *work)
{
	int cpu;
	unsigned int target;

	target = hotplug.target_cpus;

	for_each_cpu_not(cpu, cpu_online_mask) {
		if (target <= num_online_cpus())
			break;
		if (cpu == 0)
			continue;
		cpu_up(cpu);
		apply_down_lock(cpu);
	}
}

static void cpu_down_work(struct work_struct *work)
{
	int cpu, lowest_cpu;
	unsigned int target;

	target = hotplug.target_cpus;

	for_each_online_cpu(cpu) {
		if (cpu == 0)
			continue;
		lowest_cpu = get_lowest_load_cpu();
		if (lowest_cpu > 0 && lowest_cpu <= stats.total_cpus) {
			if (check_down_lock(lowest_cpu))
				break;
			cpu_down(lowest_cpu);
		}
		if (target >= num_online_cpus())
			break;
	}
}

static void online_cpu(unsigned int target)
{
	unsigned int online_cpus;

	if (!hotplug.msm_enabled)
		return;

	online_cpus = num_online_cpus();

	/* 
	 * Do not online more CPUs if max_cpus_online reached 
	 * and cancel online task if target already achieved.
	 */
	if (target <= online_cpus ||
		online_cpus >= hotplug.max_cpus_online)
		return;

	hotplug.target_cpus = target;
	queue_work_on(0, hotplug_wq, &hotplug.up_work);
}

static void offline_cpu(unsigned int target)
{
	unsigned int online_cpus;
	u64 now;

	if (!hotplug.msm_enabled)
		return;

	online_cpus = num_online_cpus();

	/* 
	 * Do not offline more CPUs if min_cpus_online reached
	 * and cancel offline task if target already achieved.
	 */
	if (target >= online_cpus || 
		online_cpus <= hotplug.min_cpus_online)
		return;

	now = ktime_to_us(ktime_get());
	if (online_cpus <= hotplug.cpus_boosted &&
	    (now - hotplug.last_input < hotplug.boost_lock_dur))
		return;

	hotplug.target_cpus = target;
	queue_work_on(0, hotplug_wq, &hotplug.down_work);
}

static unsigned int load_to_update_rate(unsigned int load)
{
	int i, ret;
	unsigned long flags;

	spin_lock_irqsave(&stats.update_rates_lock, flags);

	for (i = 0; i < stats.nupdate_rates - 1 &&
			load >= stats.update_rates[i+1]; i += 2)
		;

	ret = stats.update_rates[i];
	spin_unlock_irqrestore(&stats.update_rates_lock, flags);
	return ret;
}

static void reschedule_hotplug_work(void)
{
	int delay = load_to_update_rate(stats.cur_avg_load);
	queue_delayed_work_on(0, hotplug_wq, &hotplug_work,
			      msecs_to_jiffies(delay));
}

static void msm_hotplug_work(struct work_struct *work)
{
	unsigned int i, target = 0;

	if (hotplug.suspended && hotplug.max_cpus_online_susp <= 1) {
		dprintk("%s: suspended.\n", MSM_HOTPLUG);
		return;
	}

	update_load_stats();

	if (stats.cur_max_load >= hotplug.fast_lane_load) {
		/* Enter the fast lane */
		online_cpu(hotplug.max_cpus_online);
		goto reschedule;
	}

	/* If number of cpus locked, break out early */
	if (hotplug.min_cpus_online == stats.total_cpus) {
		if (stats.online_cpus != hotplug.min_cpus_online)
			online_cpu(hotplug.min_cpus_online);
		goto reschedule;
	} else if (hotplug.max_cpus_online == stats.min_cpus) {
		if (stats.online_cpus != hotplug.max_cpus_online)
			offline_cpu(hotplug.max_cpus_online);
		goto reschedule;
	}

	for (i = stats.min_cpus; loads[i].up_threshold; i++) {
		if (stats.cur_avg_load <= loads[i].up_threshold
		    && stats.cur_avg_load > loads[i].down_threshold) {
			target = i;
			break;
		}
	}

	if (target > hotplug.max_cpus_online)
		target = hotplug.max_cpus_online;
	else if (target < hotplug.min_cpus_online)
		target = hotplug.min_cpus_online;

	if (stats.online_cpus != target) {
		if (target > stats.online_cpus)
			online_cpu(target);
		else if (target < stats.online_cpus)
			offline_cpu(target);
	}

reschedule:
	dprintk("%s: cur_avg_load: %3u online_cpus: %u target: %u\n", MSM_HOTPLUG,
		stats.cur_avg_load, stats.online_cpus, target);
	reschedule_hotplug_work();
}

#ifdef CONFIG_STATE_NOTIFIER
static void msm_hotplug_suspend(void)
{
	int cpu;

	mutex_lock(&hotplug.msm_hotplug_mutex);
	hotplug.suspended = 1;
	hotplug.min_cpus_online_res = hotplug.min_cpus_online;
	hotplug.min_cpus_online = 1;
	hotplug.max_cpus_online_res = hotplug.max_cpus_online;
	hotplug.max_cpus_online = hotplug.max_cpus_online_susp;
	mutex_unlock(&hotplug.msm_hotplug_mutex);

	/* Do not cancel hotplug work unless max_cpus_online_susp is 1 */
	if (hotplug.max_cpus_online_susp > 1)
		return;

	/* Flush hotplug workqueue */
	flush_workqueue(hotplug_wq);
	cancel_delayed_work_sync(&hotplug_work);

	/* Put all sibling cores to sleep */
	for_each_online_cpu(cpu) {
		if (cpu == 0)
			continue;
		cpu_down(cpu);
	}
}

static void __ref msm_hotplug_resume(void)
{
	int cpu, required_reschedule = 0, required_wakeup = 0;

	if (hotplug.suspended) {
		mutex_lock(&hotplug.msm_hotplug_mutex);
		hotplug.suspended = 0;
		hotplug.min_cpus_online = hotplug.min_cpus_online_res;
		hotplug.max_cpus_online = hotplug.max_cpus_online_res;
		mutex_unlock(&hotplug.msm_hotplug_mutex);
		required_wakeup = 1;
		/* Initiate hotplug work if it was cancelled */
		if (hotplug.max_cpus_online_susp <= 1) {
			required_reschedule = 1;
			INIT_DELAYED_WORK(&hotplug_work, msm_hotplug_work);
		}
	}

	if (required_wakeup) {
		/* Fire up all CPUs */
		for_each_cpu_not(cpu, cpu_online_mask) {
			if (cpu == 0)
				continue;
			cpu_up(cpu);
			apply_down_lock(cpu);
		}
	}

	/* Resume hotplug workqueue if required */
	if (required_reschedule)
		reschedule_hotplug_work();
}

static int state_notifier_callback(struct notifier_block *this,
				unsigned long event, void *data)
{
	if (!hotplug.msm_enabled)
		return NOTIFY_OK;

	switch (event) {
		case STATE_NOTIFIER_ACTIVE:
			msm_hotplug_resume();
			break;
		case STATE_NOTIFIER_SUSPEND:
			msm_hotplug_suspend();
			break;
		default:
			break;
	}

	return NOTIFY_OK;
}
#endif

static void hotplug_input_event(struct input_handle *handle, unsigned int type,
				unsigned int code, int value)
{
	u64 now;

	if (hotplug.suspended) {
		dprintk("%s: suspended.\n", MSM_HOTPLUG);
		return;
	}

	now = ktime_to_us(ktime_get());
	hotplug.last_input = now;
	if (now - last_boost_time < MIN_INPUT_INTERVAL)
		return;

	if (num_online_cpus() >= hotplug.cpus_boosted ||
		hotplug.cpus_boosted <= hotplug.min_cpus_online)
		return;

	dprintk("%s: online_cpus: %u boosted\n", MSM_HOTPLUG,
		stats.online_cpus);

	online_cpu(hotplug.cpus_boosted);
	last_boost_time = ktime_to_us(ktime_get());
}

static int hotplug_input_connect(struct input_handler *handler,
				 struct input_dev *dev,
				 const struct input_device_id *id)
{
	struct input_handle *handle;
	int err;

	handle = kzalloc(sizeof(struct input_handle), GFP_KERNEL);
	if (!handle)
		return -ENOMEM;

	handle->dev = dev;
	handle->handler = handler;
	handle->name = handler->name;

	err = input_register_handle(handle);
	if (err)
		goto err_register;

	err = input_open_device(handle);
	if (err)
		goto err_open;

	return 0;
err_open:
	input_unregister_handle(handle);
err_register:
	kfree(handle);
	return err;
}

static void hotplug_input_disconnect(struct input_handle *handle)
{
	input_close_device(handle);
	input_unregister_handle(handle);
	kfree(handle);
}

static const struct input_device_id hotplug_ids[] = {
	{
		.flags = INPUT_DEVICE_ID_MATCH_EVBIT |
			 INPUT_DEVICE_ID_MATCH_ABSBIT,
		.evbit = { BIT_MASK(EV_ABS) },
		.absbit = { [BIT_WORD(ABS_MT_POSITION_X)] =
			    BIT_MASK(ABS_MT_POSITION_X) |
			    BIT_MASK(ABS_MT_POSITION_Y) },
	}, /* multi-touch touchscreen */
	{
		.flags = INPUT_DEVICE_ID_MATCH_KEYBIT |
			 INPUT_DEVICE_ID_MATCH_ABSBIT,
		.keybit = { [BIT_WORD(BTN_TOUCH)] = BIT_MASK(BTN_TOUCH) },
		.absbit = { [BIT_WORD(ABS_X)] =
			    BIT_MASK(ABS_X) | BIT_MASK(ABS_Y) },
	}, /* touchpad */
	{ },
};

static struct input_handler hotplug_input_handler = {
	.event		= hotplug_input_event,
	.connect	= hotplug_input_connect,
	.disconnect	= hotplug_input_disconnect,
	.name		= MSM_HOTPLUG,
	.id_table	= hotplug_ids,
};

static int __ref msm_hotplug_start(void)
{
	int cpu, ret = 0;
	struct down_lock *dl;

	hotplug_wq =
	    alloc_workqueue("msm_hotplug_wq", WQ_HIGHPRI | WQ_FREEZABLE, 0);
	if (!hotplug_wq) {
		pr_err("%s: Failed to allocate hotplug workqueue\n",
		       MSM_HOTPLUG);
		ret = -ENOMEM;
		goto err_out;
	}

#ifdef CONFIG_STATE_NOTIFIER
	hotplug.notif.notifier_call = state_notifier_callback;
	if (state_register_client(&hotplug.notif)) {
		pr_err("%s: Failed to register State notifier callback\n",
			MSM_HOTPLUG);
		goto err_dev;
	}
#endif

	ret = input_register_handler(&hotplug_input_handler);
	if (ret) {
		pr_err("%s: Failed to register input handler: %d\n",
		       MSM_HOTPLUG, ret);
		goto err_dev;
	}

	stats.load_hist = kmalloc(sizeof(stats.hist_size), GFP_KERNEL);
	if (!stats.load_hist) {
		pr_err("%s: Failed to allocate memory\n", MSM_HOTPLUG);
		ret = -ENOMEM;
		goto err_dev;
	}

	mutex_init(&stats.stats_mutex);
	mutex_init(&hotplug.msm_hotplug_mutex);

	INIT_DELAYED_WORK(&hotplug_work, msm_hotplug_work);
	INIT_WORK(&hotplug.up_work, cpu_up_work);
	INIT_WORK(&hotplug.down_work, cpu_down_work);
	for_each_possible_cpu(cpu) {
		dl = &per_cpu(lock_info, cpu);
		INIT_DELAYED_WORK(&dl->lock_rem, remove_down_lock);
	}

	/* Fire up all CPUs */
	for_each_cpu_not(cpu, cpu_online_mask) {
		if (cpu == 0)
			continue;
		cpu_up(cpu);
		apply_down_lock(cpu);
	}

	queue_delayed_work_on(0, hotplug_wq, &hotplug_work,
			      START_DELAY);

	return ret;
err_dev:
	destroy_workqueue(hotplug_wq);
err_out:
	hotplug.msm_enabled = 0;
	return ret;
}

static void msm_hotplug_stop(void)
{
	int cpu;
	struct down_lock *dl;

	flush_workqueue(hotplug_wq);
	for_each_possible_cpu(cpu) {
		dl = &per_cpu(lock_info, cpu);
		cancel_delayed_work_sync(&dl->lock_rem);
	}
	cancel_work_sync(&hotplug.down_work);
	cancel_work_sync(&hotplug.up_work);
	cancel_delayed_work_sync(&hotplug_work);

	mutex_destroy(&hotplug.msm_hotplug_mutex);
	mutex_destroy(&stats.stats_mutex);
	kfree(stats.load_hist);

#ifdef CONFIG_STATE_NOTIFIER
	state_unregister_client(&hotplug.notif);
#endif
	hotplug.notif.notifier_call = NULL;
	input_unregister_handler(&hotplug_input_handler);

	destroy_workqueue(hotplug_wq);

	/* Put all sibling cores to sleep */
	for_each_online_cpu(cpu) {
		if (cpu == 0)
			continue;
		cpu_down(cpu);
	}
}

static unsigned int *get_tokenized_data(const char *buf, int *num_tokens)
{
	const char *cp;
	int i;
	int ntokens = 1;
	int *tokenized_data;
	int err = -EINVAL;

	cp = buf;
	while ((cp = strpbrk(cp + 1, " :")))
		ntokens++;

	if (!(ntokens & 0x1))
		goto err;

	tokenized_data = kmalloc(ntokens * sizeof(int), GFP_KERNEL);
	if (!tokenized_data) {
		err = -ENOMEM;
		goto err;
	}

	cp = buf;
	i = 0;
	while (i < ntokens) {
		if (sscanf(cp, "%d", &tokenized_data[i++]) != 1)
			goto err_kfree;

		cp = strpbrk(cp, " :");
		if (!cp)
			break;
		cp++;
	}

	if (i != ntokens)
		goto err_kfree;

	*num_tokens = ntokens;
	return tokenized_data;

err_kfree:
	kfree(tokenized_data);
err:
	return ERR_PTR(err);
}

/************************** sysfs interface ************************/

static ssize_t show_enable_hotplug(struct device *dev,
				   struct device_attribute *msm_hotplug_attrs,
				   char *buf)
{
	return sprintf(buf, "%u\n", hotplug.msm_enabled);
}

static ssize_t store_enable_hotplug(struct device *dev,
				    struct device_attribute *msm_hotplug_attrs,
				    const char *buf, size_t count)
{
	int ret;
	unsigned int val;

	ret = sscanf(buf, "%u", &val);
	if (ret != 1 || val < 0 || val > 1)
		return -EINVAL;

	if (val == hotplug.msm_enabled)
		return count;

	hotplug.msm_enabled = val;

	if (hotplug.msm_enabled)
		ret = msm_hotplug_start();
	else
		msm_hotplug_stop();

	return count;
}

static ssize_t show_down_lock_duration(struct device *dev,
				       struct device_attribute
				       *msm_hotplug_attrs, char *buf)
{
	return sprintf(buf, "%u\n", hotplug.down_lock_dur);
}

static ssize_t store_down_lock_duration(struct device *dev,
					struct device_attribute
					*msm_hotplug_attrs, const char *buf,
					size_t count)
{
	int ret;
	unsigned int val;

	ret = sscanf(buf, "%u", &val);
	if (ret != 1)
		return -EINVAL;

	hotplug.down_lock_dur = val;

	return count;
}

static ssize_t show_boost_lock_duration(struct device *dev,
				        struct device_attribute
				        *msm_hotplug_attrs, char *buf)
{
	return sprintf(buf, "%llu\n", div_u64(hotplug.boost_lock_dur, 1000));
}

static ssize_t store_boost_lock_duration(struct device *dev,
					 struct device_attribute
					 *msm_hotplug_attrs, const char *buf,
					 size_t count)
{
	int ret;
	u64 val;

	ret = sscanf(buf, "%llu", &val);
	if (ret != 1)
		return -EINVAL;

	hotplug.boost_lock_dur = val * 1000;

	return count;
}

static ssize_t show_update_rates(struct device *dev,
				struct device_attribute *msm_hotplug_attrs,
				char *buf)
{
	int i;
	ssize_t ret = 0;
	unsigned long flags;

	spin_lock_irqsave(&stats.update_rates_lock, flags);

	for (i = 0; i < stats.nupdate_rates; i++)
		ret += sprintf(buf + ret, "%u%s", stats.update_rates[i],
			       i & 0x1 ? ":" : " ");

	sprintf(buf + ret - 1, "\n");
	spin_unlock_irqrestore(&stats.update_rates_lock, flags);
	return ret;
}

static ssize_t store_update_rates(struct device *dev,
				 struct device_attribute *msm_hotplug_attrs,
				 const char *buf, size_t count)
{
	int ntokens;
	unsigned int *new_update_rates = NULL;
	unsigned long flags;

	new_update_rates = get_tokenized_data(buf, &ntokens);
	if (IS_ERR(new_update_rates))
		return PTR_RET(new_update_rates);

	spin_lock_irqsave(&stats.update_rates_lock, flags);
	if (stats.update_rates != default_update_rates)
		kfree(stats.update_rates);
	stats.update_rates = new_update_rates;
	stats.nupdate_rates = ntokens;
	spin_unlock_irqrestore(&stats.update_rates_lock, flags);
	return count;
}

static ssize_t show_load_levels(struct device *dev,
				struct device_attribute *msm_hotplug_attrs,
				char *buf)
{
	int i, len = 0;

	if (!buf)
		return -EINVAL;

	for (i = 0; loads[i].up_threshold; i++) {
		len += sprintf(buf + len, "%u ", i);
		len += sprintf(buf + len, "%u ", loads[i].up_threshold);
		len += sprintf(buf + len, "%u\n", loads[i].down_threshold);
	}

	return len;
}

static ssize_t store_load_levels(struct device *dev,
				 struct device_attribute *msm_hotplug_attrs,
				 const char *buf, size_t count)
{
	int ret;
	unsigned int val[3];

	ret = sscanf(buf, "%u %u %u", &val[0], &val[1], &val[2]);
	if (ret != ARRAY_SIZE(val) || val[2] > val[1])
		return -EINVAL;

	loads[val[0]].up_threshold = val[1];
	loads[val[0]].down_threshold = val[2];

	return count;
}

static ssize_t show_min_cpus_online(struct device *dev,
				    struct device_attribute *msm_hotplug_attrs,
				    char *buf)
{
	return sprintf(buf, "%u\n", hotplug.min_cpus_online);
}

static ssize_t store_min_cpus_online(struct device *dev,
				     struct device_attribute *msm_hotplug_attrs,
				     const char *buf, size_t count)
{
	int ret;
	unsigned int val;

	ret = sscanf(buf, "%u", &val);
	if (ret != 1 || val < 1 || val > stats.total_cpus)
		return -EINVAL;

	if (hotplug.max_cpus_online < val)
		hotplug.max_cpus_online = val;

	hotplug.min_cpus_online = val;

	return count;
}

static ssize_t show_max_cpus_online(struct device *dev,
				    struct device_attribute *msm_hotplug_attrs,
				    char *buf)
{
	return sprintf(buf, "%u\n",hotplug.max_cpus_online);
}

static ssize_t store_max_cpus_online(struct device *dev,
				     struct device_attribute *msm_hotplug_attrs,
				     const char *buf, size_t count)
{
	int ret;
	unsigned int val;

	ret = sscanf(buf, "%u", &val);
	if (ret != 1 || val < 1 || val > stats.total_cpus)
		return -EINVAL;

	if (hotplug.min_cpus_online > val)
		hotplug.min_cpus_online = val;

	hotplug.max_cpus_online = val;

	return count;
}

static ssize_t show_max_cpus_online_susp(struct device *dev,
				    struct device_attribute *msm_hotplug_attrs,
				    char *buf)
{
	return sprintf(buf, "%u\n",hotplug.max_cpus_online_susp);
}

static ssize_t store_max_cpus_online_susp(struct device *dev,
				     struct device_attribute *msm_hotplug_attrs,
				     const char *buf, size_t count)
{
	int ret;
	unsigned int val;

	ret = sscanf(buf, "%u", &val);
	if (ret != 1 || val < 1 || val > stats.total_cpus)
		return -EINVAL;

	hotplug.max_cpus_online_susp = val;

	return count;
}

static ssize_t show_cpus_boosted(struct device *dev,
				 struct device_attribute *msm_hotplug_attrs,
				 char *buf)
{
	return sprintf(buf, "%u\n", hotplug.cpus_boosted);
}

static ssize_t store_cpus_boosted(struct device *dev,
				  struct device_attribute *msm_hotplug_attrs,
				  const char *buf, size_t count)
{
	int ret;
	unsigned int val;

	ret = sscanf(buf, "%u", &val);
	if (ret != 1 || val < 1 || val > stats.total_cpus)
		return -EINVAL;

	hotplug.cpus_boosted = val;

	return count;
}

static ssize_t show_offline_load(struct device *dev,
				 struct device_attribute *msm_hotplug_attrs,
				 char *buf)
{
	return sprintf(buf, "%u\n", hotplug.offline_load);
}

static ssize_t store_offline_load(struct device *dev,
				  struct device_attribute *msm_hotplug_attrs,
				  const char *buf, size_t count)
{
	int ret;
	unsigned int val;

	ret = sscanf(buf, "%u", &val);
	if (ret != 1)
		return -EINVAL;

	hotplug.offline_load = val;

	return count;
}

static ssize_t show_fast_lane_load(struct device *dev,
				   struct device_attribute *msm_hotplug_attrs,
				   char *buf)
{
	return sprintf(buf, "%u\n", hotplug.fast_lane_load);
}

static ssize_t store_fast_lane_load(struct device *dev,
				    struct device_attribute *msm_hotplug_attrs,
				    const char *buf, size_t count)
{
	int ret;
	unsigned int val;

	ret = sscanf(buf, "%u", &val);
	if (ret != 1)
		return -EINVAL;

	hotplug.fast_lane_load = val;

	return count;
}

static ssize_t show_io_is_busy(struct device *dev,
				   struct device_attribute *msm_hotplug_attrs,
				   char *buf)
{
	return sprintf(buf, "%u\n", io_is_busy);
}

static ssize_t store_io_is_busy(struct device *dev,
				    struct device_attribute *msm_hotplug_attrs,
				    const char *buf, size_t count)
{
	int ret;
	unsigned int val;

	ret = sscanf(buf, "%u", &val);
	if (ret != 1 || val < 0 || val > 1)
		return -EINVAL;

	io_is_busy = val ? true : false;

	return count;
}

static ssize_t show_current_load(struct device *dev,
				 struct device_attribute *msm_hotplug_attrs,
				 char *buf)
{
	return sprintf(buf, "%u\n", stats.cur_avg_load);
}

static DEVICE_ATTR(msm_enabled, 644, show_enable_hotplug, store_enable_hotplug);
static DEVICE_ATTR(down_lock_duration, 644, show_down_lock_duration,
		   store_down_lock_duration);
static DEVICE_ATTR(boost_lock_duration, 644, show_boost_lock_duration,
		   store_boost_lock_duration);
static DEVICE_ATTR(update_rates, 644, show_update_rates, store_update_rates);
static DEVICE_ATTR(load_levels, 644, show_load_levels, store_load_levels);
static DEVICE_ATTR(min_cpus_online, 644, show_min_cpus_online,
		   store_min_cpus_online);
static DEVICE_ATTR(max_cpus_online, 644, show_max_cpus_online,
		   store_max_cpus_online);
static DEVICE_ATTR(max_cpus_online_susp, 644, show_max_cpus_online_susp,
		   store_max_cpus_online_susp);
static DEVICE_ATTR(cpus_boosted, 644, show_cpus_boosted, store_cpus_boosted);
static DEVICE_ATTR(offline_load, 644, show_offline_load, store_offline_load);
static DEVICE_ATTR(fast_lane_load, 644, show_fast_lane_load,
		   store_fast_lane_load);
static DEVICE_ATTR(io_is_busy, 644, show_io_is_busy, store_io_is_busy);
static DEVICE_ATTR(current_load, 444, show_current_load, NULL);

static struct attribute *msm_hotplug_attrs[] = {
	&dev_attr_msm_enabled.attr,
	&dev_attr_down_lock_duration.attr,
	&dev_attr_boost_lock_duration.attr,
	&dev_attr_update_rates.attr,
	&dev_attr_load_levels.attr,
	&dev_attr_min_cpus_online.attr,
	&dev_attr_max_cpus_online.attr,
	&dev_attr_max_cpus_online_susp.attr,
	&dev_attr_cpus_boosted.attr,
	&dev_attr_offline_load.attr,
	&dev_attr_fast_lane_load.attr,
	&dev_attr_io_is_busy.attr,
	&dev_attr_current_load.attr,
	NULL,
};

static struct attribute_group attr_group = {
	.attrs = msm_hotplug_attrs,
};

/************************** sysfs end ************************/

static int msm_hotplug_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct kobject *module_kobj;

	module_kobj = kset_find_obj(module_kset, MSM_HOTPLUG);
	if (!module_kobj) {
		pr_err("%s: Cannot find kobject for module\n", MSM_HOTPLUG);
		goto err_dev;
	}

	ret = sysfs_create_group(module_kobj, &attr_group);
	if (ret) {
		pr_err("%s: Failed to create sysfs: %d\n", MSM_HOTPLUG, ret);
		goto err_dev;
	}

	if (hotplug.msm_enabled) {
		ret = msm_hotplug_start();
		if (ret != 0)
			goto err_dev;
	}

	return ret;
err_dev:
	module_kobj = NULL;
	return ret;
}

static struct platform_device msm_hotplug_device = {
	.name = MSM_HOTPLUG,
	.id = -1,
};

static int msm_hotplug_remove(struct platform_device *pdev)
{
	if (hotplug.msm_enabled)
		msm_hotplug_stop();

	return 0;
}

static struct platform_driver msm_hotplug_driver = {
	.probe = msm_hotplug_probe,
	.remove = msm_hotplug_remove,
	.driver = {
		.name = MSM_HOTPLUG,
		.owner = THIS_MODULE,
	},
};

static int __init msm_hotplug_init(void)
{
	int ret;

	ret = platform_driver_register(&msm_hotplug_driver);
	if (ret) {
		pr_err("%s: Driver register failed: %d\n", MSM_HOTPLUG, ret);
		return ret;
	}

	ret = platform_device_register(&msm_hotplug_device);
	if (ret) {
		pr_err("%s: Device register failed: %d\n", MSM_HOTPLUG, ret);
		return ret;
	}

	pr_info("%s: Device init\n", MSM_HOTPLUG);

	return ret;
}

static void __exit msm_hotplug_exit(void)
{
	platform_device_unregister(&msm_hotplug_device);
	platform_driver_unregister(&msm_hotplug_driver);
}

late_initcall(msm_hotplug_init);
module_exit(msm_hotplug_exit);

MODULE_AUTHOR("Pranav Vashi <neobuddy89@gmail.com>");
MODULE_DESCRIPTION("MSM Hotplug Driver");
MODULE_LICENSE("GPLv2");
