/* Copyright (c) 2012, Code Aurora Forum. All rights reserved.
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
#include <linux/mutex.h>
#include <linux/kthread.h>
#include <linux/kobject.h>
#include <linux/ktime.h>
#include <linux/hrtimer.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/stringify.h>
#include <linux/debugfs.h>
#include <asm/atomic.h>
#include <asm/page.h>
#include <mach/msm_dcvs.h>

#define CORE_HANDLE_OFFSET (0xA0)
#define __err(f, ...) pr_err("MSM_DCVS: %s: " f, __func__, __VA_ARGS__)
#define __info(f, ...) pr_info("MSM_DCVS: %s: " f, __func__, __VA_ARGS__)
#define MAX_PENDING	(5)

enum {
	MSM_DCVS_DEBUG_NOTIFIER    = BIT(0),
	MSM_DCVS_DEBUG_IDLE_PULSE  = BIT(1),
	MSM_DCVS_DEBUG_FREQ_CHANGE = BIT(2),
};

struct core_attribs {
	struct kobj_attribute idle_enabled;
	struct kobj_attribute freq_change_enabled;
	struct kobj_attribute actual_freq;
	struct kobj_attribute freq_change_us;

	struct kobj_attribute max_time_us;

	struct kobj_attribute slack_time_us;
	struct kobj_attribute scale_slack_time;
	struct kobj_attribute scale_slack_time_pct;
	struct kobj_attribute disable_pc_threshold;
	struct kobj_attribute em_window_size;
	struct kobj_attribute em_max_util_pct;
	struct kobj_attribute ss_window_size;
	struct kobj_attribute ss_util_pct;
	struct kobj_attribute ss_iobusy_conv;

	struct attribute_group attrib_group;
};

struct dcvs_core {
	char core_name[CORE_NAME_MAX];
	uint32_t new_freq[MAX_PENDING];
	uint32_t actual_freq;
	uint32_t freq_change_us;

	uint32_t max_time_us; /* core param */

	struct msm_dcvs_algo_param algo_param;
	struct msm_dcvs_idle *idle_driver;
	struct msm_dcvs_freq *freq_driver;

	/* private */
	int64_t time_start;
	struct mutex lock;
	spinlock_t cpu_lock;
	struct task_struct *task;
	struct core_attribs attrib;
	uint32_t handle;
	uint32_t group_id;
	uint32_t freq_pending;
	struct hrtimer timer;
	int32_t timer_disabled;
	/* track if kthread for change_freq is active */
	int32_t change_freq_activated;
};

static int msm_dcvs_debug;
static int msm_dcvs_enabled = 1;
module_param_named(enable, msm_dcvs_enabled, int, S_IRUGO | S_IWUSR | S_IWGRP);

static struct dentry *debugfs_base;

static struct dcvs_core core_list[CORES_MAX];
static DEFINE_MUTEX(core_list_lock);

static struct kobject *cores_kobj;
static struct dcvs_core *core_handles[CORES_MAX];

/* Change core frequency, called with core mutex locked */
static int __msm_dcvs_change_freq(struct dcvs_core *core)
{
	int ret = 0;
	unsigned long flags = 0;
	unsigned int requested_freq = 0;
	unsigned int prev_freq = 0;
	int64_t time_start = 0;
	int64_t time_end = 0;
	uint32_t slack_us = 0;
	uint32_t ret1 = 0;

	if (!core->freq_driver || !core->freq_driver->set_frequency) {
		/* Core may have unregistered or hotplugged */
		return -ENODEV;
	}
repeat:
	spin_lock_irqsave(&core->cpu_lock, flags);
	if (unlikely(!core->freq_pending)) {
		spin_unlock_irqrestore(&core->cpu_lock, flags);
		return ret;
	}
	requested_freq = core->new_freq[core->freq_pending - 1];
	if (unlikely(core->freq_pending > 1) &&
		(msm_dcvs_debug & MSM_DCVS_DEBUG_FREQ_CHANGE)) {
		int i;
		for (i = 0; i < core->freq_pending - 1; i++) {
			__info("Core %s missing freq %u\n",
				core->core_name, core->new_freq[i]);
		}
	}
	time_start = core->time_start;
	core->time_start = 0;
	core->freq_pending = 0;
	/**
	 * Cancel the timers, we dont want the timer firing as we are
	 * changing the clock rate. Dont let idle_exit and others setup
	 * timers as well.
	 */
	hrtimer_cancel(&core->timer);
	core->timer_disabled = 1;
	spin_unlock_irqrestore(&core->cpu_lock, flags);

	if (requested_freq == core->actual_freq)
		return ret;

	/**
	 * Call the frequency sink driver to change the frequency
	 * We will need to get back the actual frequency in KHz and
	 * the record the time taken to change it.
	 */
	ret = core->freq_driver->set_frequency(core->freq_driver,
				requested_freq);
	if (ret <= 0) {
		__err("Core %s failed to set freq %u\n",
				core->core_name, requested_freq);
		/* continue to call TZ to get updated slack timer */
	} else {
		prev_freq = core->actual_freq;
		core->actual_freq = ret;
	}

	time_end = ktime_to_ns(ktime_get());
	if (msm_dcvs_debug & MSM_DCVS_DEBUG_FREQ_CHANGE)
		__info("Core %s Time end %llu Time start: %llu\n",
			core->core_name, time_end, time_start);
	time_end -= time_start;
	do_div(time_end, NSEC_PER_USEC);
	core->freq_change_us = (uint32_t)time_end;

	/**
	 * Disable low power modes if the actual frequency is >
	 * disable_pc_threshold.
	 */
	if (core->actual_freq >
			core->algo_param.disable_pc_threshold) {
		core->idle_driver->enable(core->idle_driver,
				MSM_DCVS_DISABLE_HIGH_LATENCY_MODES);
		if (msm_dcvs_debug & MSM_DCVS_DEBUG_IDLE_PULSE)
			__info("Disabling LPM for %s\n", core->core_name);
	} else if (core->actual_freq <=
			core->algo_param.disable_pc_threshold) {
		core->idle_driver->enable(core->idle_driver,
				MSM_DCVS_ENABLE_HIGH_LATENCY_MODES);
		if (msm_dcvs_debug & MSM_DCVS_DEBUG_IDLE_PULSE)
			__info("Enabling LPM for %s\n", core->core_name);
	}

	/**
	 * Update algorithm with new freq and time taken to change
	 * to this frequency and that will get us the new slack
	 * timer
	 */
	ret = msm_dcvs_scm_event(core->handle, MSM_DCVS_SCM_CLOCK_FREQ_UPDATE,
		core->actual_freq, (uint32_t)time_end, &slack_us, &ret1);
	if (!ret) {
		/* Reset the slack timer */
		if (slack_us) {
			core->timer_disabled = 0;
			ret = hrtimer_start(&core->timer,
				ktime_set(0, slack_us * 1000),
				HRTIMER_MODE_REL_PINNED);
			if (ret)
				__err("Failed to register timer for core %s\n",
						core->core_name);
		}
	} else {
		__err("Error sending core (%s) freq change (%u)\n",
				core->core_name, core->actual_freq);
	}

	if (msm_dcvs_debug & MSM_DCVS_DEBUG_FREQ_CHANGE)
		__info("Freq %u requested for core %s (actual %u prev %u) "
			"change time %u us slack time %u us\n",
			requested_freq, core->core_name,
			core->actual_freq, prev_freq,
			core->freq_change_us, slack_us);

	/**
	 * By the time we are done with freq changes, we could be asked to
	 * change again. Check before exiting.
	 */
	if (core->freq_pending)
		goto repeat;

	core->change_freq_activated = 0;
	return ret;
}

static int msm_dcvs_do_freq(void *data)
{
	struct dcvs_core *core = (struct dcvs_core *)data;
	static struct sched_param param = {.sched_priority = MAX_RT_PRIO - 1};

	sched_setscheduler(current, SCHED_FIFO, &param);
	set_current_state(TASK_UNINTERRUPTIBLE);

	while (!kthread_should_stop()) {
		mutex_lock(&core->lock);
		__msm_dcvs_change_freq(core);
		mutex_unlock(&core->lock);

		schedule();

		if (kthread_should_stop())
			break;

		set_current_state(TASK_UNINTERRUPTIBLE);
	}

	__set_current_state(TASK_RUNNING);

	return 0;
}

static int msm_dcvs_update_freq(struct dcvs_core *core,
		enum msm_dcvs_scm_event event, uint32_t param0,
		uint32_t *ret1, int *freq_changed)
{
	int ret = 0;
	unsigned long flags = 0;
	uint32_t new_freq = 0;

	spin_lock_irqsave(&core->cpu_lock, flags);
	ret = msm_dcvs_scm_event(core->handle, event, param0,
				core->actual_freq, &new_freq, ret1);
	if (ret) {
		__err("Error (%d) sending SCM event %d for core %s\n",
				ret, event, core->core_name);
		goto freq_done;
	}

	if ((core->actual_freq != new_freq) &&
			(core->new_freq[core->freq_pending] != new_freq)) {
		if (core->freq_pending >= MAX_PENDING - 1)
			core->freq_pending = MAX_PENDING - 1;
		core->new_freq[core->freq_pending++] = new_freq;
		core->time_start = ktime_to_ns(ktime_get());

		/* Schedule the frequency change */
		if (!core->task)
			__err("Uninitialized task for core %s\n",
					core->core_name);
		else {
			if (freq_changed)
				*freq_changed = 1;
			core->change_freq_activated = 1;
			wake_up_process(core->task);
		}
	} else {
		if (freq_changed)
			*freq_changed = 0;
	}
freq_done:
	spin_unlock_irqrestore(&core->cpu_lock, flags);

	return ret;
}

static enum hrtimer_restart msm_dcvs_core_slack_timer(struct hrtimer *timer)
{
	int ret = 0;
	struct dcvs_core *core = container_of(timer, struct dcvs_core, timer);
	uint32_t ret1;
	uint32_t ret2;

	if (msm_dcvs_debug & MSM_DCVS_DEBUG_FREQ_CHANGE)
		__info("Slack timer fired for core %s\n", core->core_name);

	/**
	 * Timer expired, notify TZ
	 * Dont care about the third arg.
	 */
	ret = msm_dcvs_update_freq(core, MSM_DCVS_SCM_QOS_TIMER_EXPIRED, 0,
				   &ret1, &ret2);
	if (ret)
		__err("Timer expired for core %s but failed to notify.\n",
				core->core_name);

	return HRTIMER_NORESTART;
}

/* Helper functions and macros for sysfs nodes for a core */
#define CORE_FROM_ATTRIBS(attr, name) \
	container_of(container_of(attr, struct core_attribs, name), \
		struct dcvs_core, attrib);

#define DCVS_PARAM_SHOW(_name, v) \
static ssize_t msm_dcvs_attr_##_name##_show(struct kobject *kobj, \
		struct kobj_attribute *attr, char *buf) \
{ \
	struct dcvs_core *core = CORE_FROM_ATTRIBS(attr, _name); \
	return snprintf(buf, PAGE_SIZE, "%d\n", v); \
}

#define DCVS_ALGO_PARAM(_name) \
static ssize_t msm_dcvs_attr_##_name##_show(struct kobject *kobj,\
		struct kobj_attribute *attr, char *buf) \
{ \
	struct dcvs_core *core = CORE_FROM_ATTRIBS(attr, _name); \
	return snprintf(buf, PAGE_SIZE, "%d\n", core->algo_param._name); \
} \
static ssize_t msm_dcvs_attr_##_name##_store(struct kobject *kobj, \
		struct kobj_attribute *attr, const char *buf, size_t count) \
{ \
	int ret = 0; \
	uint32_t val = 0; \
	struct dcvs_core *core = CORE_FROM_ATTRIBS(attr, _name); \
	mutex_lock(&core->lock); \
	ret = kstrtouint(buf, 10, &val); \
	if (ret) { \
		__err("Invalid input %s for %s\n", buf, __stringify(_name));\
	} else { \
		uint32_t old_val = core->algo_param._name; \
		core->algo_param._name = val; \
		ret = msm_dcvs_scm_set_algo_params(core->handle, \
				&core->algo_param); \
		if (ret) { \
			core->algo_param._name = old_val; \
			__err("Error(%d) in setting %d for algo param %s\n",\
					ret, val, __stringify(_name)); \
		} \
	} \
	mutex_unlock(&core->lock); \
	return count; \
}

#define DCVS_RO_ATTRIB(i, _name) \
	core->attrib._name.attr.name = __stringify(_name); \
	core->attrib._name.attr.mode = S_IRUGO; \
	core->attrib._name.show = msm_dcvs_attr_##_name##_show; \
	core->attrib._name.store = NULL; \
	core->attrib.attrib_group.attrs[i] = &core->attrib._name.attr;

#define DCVS_RW_ATTRIB(i, _name) \
	core->attrib._name.attr.name = __stringify(_name); \
	core->attrib._name.attr.mode = S_IRUGO | S_IWUSR; \
	core->attrib._name.show = msm_dcvs_attr_##_name##_show; \
	core->attrib._name.store = msm_dcvs_attr_##_name##_store; \
	core->attrib.attrib_group.attrs[i] = &core->attrib._name.attr;

/**
 * Function declarations for different attributes.
 * Gets used when setting the attribute show and store parameters.
 */
DCVS_PARAM_SHOW(idle_enabled, (core->idle_driver != NULL))
DCVS_PARAM_SHOW(freq_change_enabled, (core->freq_driver != NULL))
DCVS_PARAM_SHOW(actual_freq, (core->actual_freq))
DCVS_PARAM_SHOW(freq_change_us, (core->freq_change_us))
DCVS_PARAM_SHOW(max_time_us, (core->max_time_us))

DCVS_ALGO_PARAM(slack_time_us)
DCVS_ALGO_PARAM(scale_slack_time)
DCVS_ALGO_PARAM(scale_slack_time_pct)
DCVS_ALGO_PARAM(disable_pc_threshold)
DCVS_ALGO_PARAM(em_window_size)
DCVS_ALGO_PARAM(em_max_util_pct)
DCVS_ALGO_PARAM(ss_window_size)
DCVS_ALGO_PARAM(ss_util_pct)
DCVS_ALGO_PARAM(ss_iobusy_conv)

static int msm_dcvs_setup_core_sysfs(struct dcvs_core *core)
{
	int ret = 0;
	struct kobject *core_kobj = NULL;
	const int attr_count = 15;

	BUG_ON(!cores_kobj);

	core->attrib.attrib_group.attrs =
		kzalloc(attr_count * sizeof(struct attribute *), GFP_KERNEL);

	if (!core->attrib.attrib_group.attrs) {
		ret = -ENOMEM;
		goto done;
	}

	DCVS_RO_ATTRIB(0, idle_enabled);
	DCVS_RO_ATTRIB(1, freq_change_enabled);
	DCVS_RO_ATTRIB(2, actual_freq);
	DCVS_RO_ATTRIB(3, freq_change_us);
	DCVS_RO_ATTRIB(4, max_time_us);

	DCVS_RW_ATTRIB(5, slack_time_us);
	DCVS_RW_ATTRIB(6, scale_slack_time);
	DCVS_RW_ATTRIB(7, scale_slack_time_pct);
	DCVS_RW_ATTRIB(8, disable_pc_threshold);
	DCVS_RW_ATTRIB(9, em_window_size);
	DCVS_RW_ATTRIB(10, em_max_util_pct);
	DCVS_RW_ATTRIB(11, ss_window_size);
	DCVS_RW_ATTRIB(12, ss_util_pct);
	DCVS_RW_ATTRIB(13, ss_iobusy_conv);

	core->attrib.attrib_group.attrs[14] = NULL;

	core_kobj = kobject_create_and_add(core->core_name, cores_kobj);
	if (!core_kobj) {
		ret = -ENOMEM;
		goto done;
	}

	ret = sysfs_create_group(core_kobj, &core->attrib.attrib_group);
	if (ret)
		__err("Cannot create core %s attr group\n", core->core_name);
	else if (msm_dcvs_debug & MSM_DCVS_DEBUG_NOTIFIER)
		__info("Setting up attributes for core %s\n", core->core_name);

done:
	if (ret) {
		kfree(core->attrib.attrib_group.attrs);
		kobject_del(core_kobj);
	}

	return ret;
}

/* Return the core if found or add to list if @add_to_list is true */
static struct dcvs_core *msm_dcvs_get_core(const char *name, int add_to_list)
{
	struct dcvs_core *core = NULL;
	int i;
	int empty = -1;

	if (!name[0] ||
		(strnlen(name, CORE_NAME_MAX - 1) == CORE_NAME_MAX - 1))
		return core;

	mutex_lock(&core_list_lock);
	for (i = 0; i < CORES_MAX; i++) {
		core = &core_list[i];
		if ((empty < 0) && !core->core_name[0]) {
			empty = i;
			continue;
		}
		if (!strncmp(name, core->core_name, CORE_NAME_MAX))
			break;
	}

	/* Check for core_list full */
	if ((i == CORES_MAX) && (empty < 0)) {
		mutex_unlock(&core_list_lock);
		return NULL;
	}

	if (i == CORES_MAX && add_to_list) {
		core = &core_list[empty];
		strlcpy(core->core_name, name, CORE_NAME_MAX);
		mutex_init(&core->lock);
		spin_lock_init(&core->cpu_lock);
		core->handle = empty + CORE_HANDLE_OFFSET;
		hrtimer_init(&core->timer,
				CLOCK_MONOTONIC, HRTIMER_MODE_REL_PINNED);
		core->timer.function = msm_dcvs_core_slack_timer;
	}
	mutex_unlock(&core_list_lock);

	return core;
}

int msm_dcvs_register_core(const char *core_name, uint32_t group_id,
		struct msm_dcvs_core_info *info)
{
	int ret = -EINVAL;
	struct dcvs_core *core = NULL;

	if (!core_name || !core_name[0])
		return ret;

	core = msm_dcvs_get_core(core_name, true);
	if (!core)
		return ret;

	mutex_lock(&core->lock);
	if (group_id) {
		/**
		 * Create a group for cores, if this core is part of a group
		 * if the group_id is 0, the core is not part of a group.
		 * If the group_id already exits, it will through an error
		 * which we will ignore.
		 */
		ret = msm_dcvs_scm_create_group(group_id);
		if (ret == -ENOMEM)
			goto bail;
	}
	core->group_id = group_id;

	core->max_time_us = info->core_param.max_time_us;
	memcpy(&core->algo_param, &info->algo_param,
			sizeof(struct msm_dcvs_algo_param));

	ret = msm_dcvs_scm_register_core(core->handle, group_id,
			&info->core_param, info->freq_tbl);
	if (ret)
		goto bail;

	ret = msm_dcvs_scm_set_algo_params(core->handle, &info->algo_param);
	if (ret)
		goto bail;

	ret = msm_dcvs_setup_core_sysfs(core);
	if (ret) {
		__err("Unable to setup core %s sysfs\n", core->core_name);
		core_handles[core->handle - CORE_HANDLE_OFFSET] = NULL;
		goto bail;
	}

bail:
	mutex_unlock(&core->lock);
	return ret;
}
EXPORT_SYMBOL(msm_dcvs_register_core);

int msm_dcvs_freq_sink_register(struct msm_dcvs_freq *drv)
{
	int ret = -EINVAL;
	struct dcvs_core *core = NULL;
	uint32_t ret1;
	uint32_t ret2;

	if (!drv || !drv->core_name)
		return ret;

	core = msm_dcvs_get_core(drv->core_name, true);
	if (!core)
		return ret;

	mutex_lock(&core->lock);
	if (core->freq_driver && (msm_dcvs_debug & MSM_DCVS_DEBUG_NOTIFIER))
		__info("Frequency notifier for %s being replaced\n",
				core->core_name);
	core->freq_driver = drv;
	core->task = kthread_create(msm_dcvs_do_freq, (void *)core,
			"msm_dcvs/%d", core->handle);
	if (IS_ERR(core->task)) {
		mutex_unlock(&core->lock);
		return -EFAULT;
	}

	if (msm_dcvs_debug & MSM_DCVS_DEBUG_IDLE_PULSE)
		__info("Enabling idle pulse for %s\n", core->core_name);

	if (core->idle_driver) {
		core->actual_freq = core->freq_driver->get_frequency(drv);
		/* Notify TZ to start receiving idle info for the core */
		ret = msm_dcvs_update_freq(core, MSM_DCVS_SCM_ENABLE_CORE, 1,
					   &ret1, &ret2);
		core->idle_driver->enable(core->idle_driver,
				MSM_DCVS_ENABLE_IDLE_PULSE);
	}

	mutex_unlock(&core->lock);

	return core->handle;
}
EXPORT_SYMBOL(msm_dcvs_freq_sink_register);

int msm_dcvs_freq_sink_unregister(struct msm_dcvs_freq *drv)
{
	int ret = -EINVAL;
	struct dcvs_core *core = NULL;
	uint32_t ret1;
	uint32_t ret2;

	if (!drv || !drv->core_name)
		return ret;

	core = msm_dcvs_get_core(drv->core_name, false);
	if (!core)
		return ret;

	mutex_lock(&core->lock);
	if (msm_dcvs_debug & MSM_DCVS_DEBUG_IDLE_PULSE)
		__info("Disabling idle pulse for %s\n", core->core_name);
	if (core->idle_driver) {
		core->idle_driver->enable(core->idle_driver,
				MSM_DCVS_DISABLE_IDLE_PULSE);
		/* Notify TZ to stop receiving idle info for the core */
		ret = msm_dcvs_update_freq(core, MSM_DCVS_SCM_ENABLE_CORE, 0,
					   &ret1, &ret2);
		hrtimer_cancel(&core->timer);
		core->idle_driver->enable(core->idle_driver,
				MSM_DCVS_ENABLE_HIGH_LATENCY_MODES);
		if (msm_dcvs_debug & MSM_DCVS_DEBUG_IDLE_PULSE)
			__info("Enabling LPM for %s\n", core->core_name);
	}
	core->freq_pending = 0;
	core->freq_driver = NULL;
	mutex_unlock(&core->lock);
	kthread_stop(core->task);

	return 0;
}
EXPORT_SYMBOL(msm_dcvs_freq_sink_unregister);

int msm_dcvs_idle_source_register(struct msm_dcvs_idle *drv)
{
	int ret = -EINVAL;
	struct dcvs_core *core = NULL;

	if (!drv || !drv->core_name)
		return ret;

	core = msm_dcvs_get_core(drv->core_name, true);
	if (!core)
		return ret;

	mutex_lock(&core->lock);
	if (core->idle_driver && (msm_dcvs_debug & MSM_DCVS_DEBUG_NOTIFIER))
		__info("Idle notifier for %s being replaced\n",
				core->core_name);
	core->idle_driver = drv;
	mutex_unlock(&core->lock);

	return core->handle;
}
EXPORT_SYMBOL(msm_dcvs_idle_source_register);

int msm_dcvs_idle_source_unregister(struct msm_dcvs_idle *drv)
{
	int ret = -EINVAL;
	struct dcvs_core *core = NULL;

	if (!drv || !drv->core_name)
		return ret;

	core = msm_dcvs_get_core(drv->core_name, false);
	if (!core)
		return ret;

	mutex_lock(&core->lock);
	core->idle_driver = NULL;
	mutex_unlock(&core->lock);

	return 0;
}
EXPORT_SYMBOL(msm_dcvs_idle_source_unregister);

int msm_dcvs_idle(int handle, enum msm_core_idle_state state, uint32_t iowaited)
{
	int ret = 0;
	struct dcvs_core *core = NULL;
	uint32_t timer_interval_us = 0;
	uint32_t r0, r1;
	uint32_t freq_changed = 0;

	if (handle >= CORE_HANDLE_OFFSET &&
			(handle - CORE_HANDLE_OFFSET) < CORES_MAX)
		core = &core_list[handle - CORE_HANDLE_OFFSET];

	BUG_ON(!core);

	if (msm_dcvs_debug & MSM_DCVS_DEBUG_IDLE_PULSE)
		__info("Core %s idle state %d\n", core->core_name, state);

	switch (state) {
	case MSM_DCVS_IDLE_ENTER:
		hrtimer_cancel(&core->timer);
		ret = msm_dcvs_scm_event(core->handle,
				MSM_DCVS_SCM_IDLE_ENTER, 0, 0, &r0, &r1);
		if (ret)
			__err("Error (%d) sending idle enter for %s\n",
					ret, core->core_name);
		break;

	case MSM_DCVS_IDLE_EXIT:
		hrtimer_cancel(&core->timer);
		ret = msm_dcvs_update_freq(core, MSM_DCVS_SCM_IDLE_EXIT,
				iowaited, &timer_interval_us, &freq_changed);
		if (ret)
			__err("Error (%d) sending idle exit for %s\n",
					ret, core->core_name);
		/* only start slack timer if change_freq won't */
		if (freq_changed || core->change_freq_activated)
			break;
		if (timer_interval_us && !core->timer_disabled) {
			ret = hrtimer_start(&core->timer,
				ktime_set(0, timer_interval_us * 1000),
				HRTIMER_MODE_REL_PINNED);

			if (ret)
				__err("Failed to register timer for core %s\n",
				      core->core_name);
		}
		break;
	}

	return ret;
}
EXPORT_SYMBOL(msm_dcvs_idle);

static int __init msm_dcvs_late_init(void)
{
	struct kobject *module_kobj = NULL;
	int ret = 0;

	module_kobj = kset_find_obj(module_kset, KBUILD_MODNAME);
	if (!module_kobj) {
		pr_err("%s: cannot find kobject for module %s\n",
				__func__, KBUILD_MODNAME);
		ret = -ENOENT;
		goto err;
	}

	cores_kobj = kobject_create_and_add("cores", module_kobj);
	if (!cores_kobj) {
		__err("Cannot create %s kobject\n", "cores");
		ret = -ENOMEM;
		goto err;
	}

	debugfs_base = debugfs_create_dir("msm_dcvs", NULL);
	if (!debugfs_base) {
		__err("Cannot create debugfs base %s\n", "msm_dcvs");
		ret = -ENOENT;
		goto err;
	}

	if (!debugfs_create_u32("debug_mask", S_IRUGO | S_IWUSR,
				debugfs_base, &msm_dcvs_debug)) {
		__err("Cannot create debugfs entry %s\n", "debug_mask");
		ret = -ENOMEM;
		goto err;
	}

err:
	if (ret) {
		kobject_del(cores_kobj);
		cores_kobj = NULL;
		debugfs_remove(debugfs_base);
	}

	return ret;
}
late_initcall(msm_dcvs_late_init);

static int __init msm_dcvs_early_init(void)
{
	int ret = 0;

	if (!msm_dcvs_enabled) {
		__info("Not enabled (%d)\n", msm_dcvs_enabled);
		return 0;
	}

	ret = msm_dcvs_scm_init(10 * 1024);
	if (ret)
		__err("Unable to initialize DCVS err=%d\n", ret);

	return ret;
}
postcore_initcall(msm_dcvs_early_init);
