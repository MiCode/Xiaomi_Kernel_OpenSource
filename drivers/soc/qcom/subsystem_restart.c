/* Copyright (c) 2011-2014, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#define pr_fmt(fmt) "subsys-restart: %s(): " fmt, __func__

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/uaccess.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/delay.h>
#include <linux/list.h>
#include <linux/io.h>
#include <linux/kthread.h>
#include <linux/time.h>
#include <linux/wakelock.h>
#include <linux/suspend.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/device.h>
#include <linux/idr.h>
#include <linux/debugfs.h>
#include <linux/interrupt.h>
#include <linux/of_gpio.h>
#include <linux/cdev.h>
#include <linux/platform_device.h>
#include <soc/qcom/subsystem_restart.h>
#include <soc/qcom/subsystem_notif.h>
#include <soc/qcom/socinfo.h>
#include <soc/qcom/sysmon.h>

#include <asm/current.h>

static int enable_debug;
module_param(enable_debug, int, S_IRUGO | S_IWUSR);

/**
 * enum p_subsys_state - state of a subsystem (private)
 * @SUBSYS_NORMAL: subsystem is operating normally
 * @SUBSYS_CRASHED: subsystem has crashed and hasn't been shutdown
 * @SUBSYS_RESTARTING: subsystem has been shutdown and is now restarting
 *
 * The 'private' side of the subsytem state used to determine where in the
 * restart process the subsystem is.
 */
enum p_subsys_state {
	SUBSYS_NORMAL,
	SUBSYS_CRASHED,
	SUBSYS_RESTARTING,
};

/**
 * enum subsys_state - state of a subsystem (public)
 * @SUBSYS_OFFLINE: subsystem is offline
 * @SUBSYS_ONLINE: subsystem is online
 *
 * The 'public' side of the subsytem state, exposed to userspace.
 */
enum subsys_state {
	SUBSYS_OFFLINE,
	SUBSYS_ONLINE,
};

static const char * const subsys_states[] = {
	[SUBSYS_OFFLINE] = "OFFLINE",
	[SUBSYS_ONLINE] = "ONLINE",
};

static const char * const restart_levels[] = {
	[RESET_SOC] = "SYSTEM",
	[RESET_SUBSYS_COUPLED] = "RELATED",
};

/**
 * struct subsys_tracking - track state of a subsystem or restart order
 * @p_state: private state of subsystem/order
 * @state: public state of subsystem/order
 * @s_lock: protects p_state
 * @lock: protects subsystem/order callbacks and state
 *
 * Tracks the state of a subsystem or a set of subsystems (restart order).
 * Doing this avoids the need to grab each subsystem's lock and update
 * each subsystems state when restarting an order.
 */
struct subsys_tracking {
	enum p_subsys_state p_state;
	spinlock_t s_lock;
	enum subsys_state state;
	struct mutex lock;
};

/**
 * struct subsys_soc_restart_order - subsystem restart order
 * @subsystem_list: names of subsystems in this restart order
 * @count: number of subsystems in order
 * @track: state tracking and locking
 * @subsys_ptrs: pointers to subsystems in this restart order
 */
struct subsys_soc_restart_order {
	struct device_node **device_ptrs;
	int count;

	struct subsys_tracking track;
	struct subsys_device **subsys_ptrs;
	struct list_head list;
};

struct restart_log {
	struct timeval time;
	struct subsys_device *dev;
	struct list_head list;
};

/**
 * struct subsys_device - subsystem device
 * @desc: subsystem descriptor
 * @work: context for subsystem_restart_wq_func() for this device
 * @ssr_wlock: prevents suspend during subsystem_restart()
 * @wlname: name of wakeup source
 * @device_restart_work: work struct for device restart
 * @track: state tracking and locking
 * @notify: subsys notify handle
 * @dev: device
 * @owner: module that provides @desc
 * @count: reference count of subsystem_get()/subsystem_put()
 * @id: ida
 * @restart_level: restart level (0 - panic, 1 - related, 2 - independent, etc.)
 * @restart_order: order of other devices this devices restarts with
 * @crash_count: number of times the device has crashed
 * @dentry: debugfs directory for this device
 * @do_ramdump_on_put: ramdump on subsystem_put() if true
 * @err_ready: completion variable to record error ready from subsystem
 * @crashed: indicates if subsystem has crashed
 */
struct subsys_device {
	struct subsys_desc *desc;
	struct work_struct work;
	struct wakeup_source ssr_wlock;
	char wlname[64];
	struct work_struct device_restart_work;
	struct subsys_tracking track;

	void *notify;
	struct device dev;
	struct module *owner;
	int count;
	int id;
	int restart_level;
	int crash_count;
	struct subsys_soc_restart_order *restart_order;
#ifdef CONFIG_DEBUG_FS
	struct dentry *dentry;
#endif
	bool do_ramdump_on_put;
	struct cdev char_dev;
	dev_t dev_no;
	struct completion err_ready;
	bool crashed;
	struct list_head list;
};

static struct subsys_device *to_subsys(struct device *d)
{
	return container_of(d, struct subsys_device, dev);
}

static ssize_t name_show(struct device *dev, struct device_attribute *attr,
		char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%s\n", to_subsys(dev)->desc->name);
}

static ssize_t state_show(struct device *dev, struct device_attribute *attr,
		char *buf)
{
	enum subsys_state state = to_subsys(dev)->track.state;
	return snprintf(buf, PAGE_SIZE, "%s\n", subsys_states[state]);
}

static ssize_t crash_count_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n", to_subsys(dev)->crash_count);
}

static ssize_t
restart_level_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	int level = to_subsys(dev)->restart_level;
	return snprintf(buf, PAGE_SIZE, "%s\n", restart_levels[level]);
}

static ssize_t restart_level_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct subsys_device *subsys = to_subsys(dev);
	int i;
	const char *p;

	p = memchr(buf, '\n', count);
	if (p)
		count = p - buf;

	for (i = 0; i < ARRAY_SIZE(restart_levels); i++)
		if (!strncasecmp(buf, restart_levels[i], count)) {
			subsys->restart_level = i;
			return count;
		}
	return -EPERM;
}

int subsys_get_restart_level(struct subsys_device *dev)
{
	return dev->restart_level;
}
EXPORT_SYMBOL(subsys_get_restart_level);

static void subsys_set_state(struct subsys_device *subsys,
			     enum subsys_state state)
{
	unsigned long flags;

	spin_lock_irqsave(&subsys->track.s_lock, flags);
	if (subsys->track.state != state) {
		subsys->track.state = state;
		spin_unlock_irqrestore(&subsys->track.s_lock, flags);
		sysfs_notify(&subsys->dev.kobj, NULL, "state");
		return;
	}
	spin_unlock_irqrestore(&subsys->track.s_lock, flags);
}

/**
 * subsytem_default_online() - Mark a subsystem as online by default
 * @dev: subsystem to mark as online
 *
 * Marks a subsystem as "online" without increasing the reference count
 * on the subsystem. This is typically used by subsystems that are already
 * online when the kernel boots up.
 */
void subsys_default_online(struct subsys_device *dev)
{
	subsys_set_state(dev, SUBSYS_ONLINE);
}
EXPORT_SYMBOL(subsys_default_online);

static struct device_attribute subsys_attrs[] = {
	__ATTR_RO(name),
	__ATTR_RO(state),
	__ATTR_RO(crash_count),
	__ATTR(restart_level, 0644, restart_level_show, restart_level_store),
	__ATTR_NULL,
};

static struct bus_type subsys_bus_type = {
	.name		= "msm_subsys",
	.dev_attrs	= subsys_attrs,
};

static DEFINE_IDA(subsys_ida);

static int enable_ramdumps;
module_param(enable_ramdumps, int, S_IRUGO | S_IWUSR);

struct workqueue_struct *ssr_wq;
static struct class *char_class;

static LIST_HEAD(restart_log_list);
static LIST_HEAD(subsys_list);
static LIST_HEAD(ssr_order_list);
static DEFINE_MUTEX(soc_order_reg_lock);
static DEFINE_MUTEX(restart_log_mutex);
static DEFINE_MUTEX(subsys_list_lock);
static DEFINE_MUTEX(char_device_lock);
static DEFINE_MUTEX(ssr_order_mutex);

static struct subsys_soc_restart_order *
update_restart_order(struct subsys_device *dev)
{
	int i;
	struct subsys_soc_restart_order *order;
	struct device_node *device = dev->desc->dev->of_node;

	mutex_lock(&soc_order_reg_lock);
	list_for_each_entry(order, &ssr_order_list, list) {
		for (i = 0; i < order->count; i++) {
			if (order->device_ptrs[i] == device) {
				order->subsys_ptrs[i] = dev;
				goto found;
			}
		}
	}
	order = NULL;
found:
	mutex_unlock(&soc_order_reg_lock);

	return order;
}

static int max_restarts;
module_param(max_restarts, int, 0644);

static long max_history_time = 3600;
module_param(max_history_time, long, 0644);

static void do_epoch_check(struct subsys_device *dev)
{
	int n = 0;
	struct timeval *time_first = NULL, *curr_time;
	struct restart_log *r_log, *temp;
	static int max_restarts_check;
	static long max_history_time_check;

	mutex_lock(&restart_log_mutex);

	max_restarts_check = max_restarts;
	max_history_time_check = max_history_time;

	/* Check if epoch checking is enabled */
	if (!max_restarts_check)
		goto out;

	r_log = kmalloc(sizeof(struct restart_log), GFP_KERNEL);
	if (!r_log)
		goto out;
	r_log->dev = dev;
	do_gettimeofday(&r_log->time);
	curr_time = &r_log->time;
	INIT_LIST_HEAD(&r_log->list);

	list_add_tail(&r_log->list, &restart_log_list);

	list_for_each_entry_safe(r_log, temp, &restart_log_list, list) {

		if ((curr_time->tv_sec - r_log->time.tv_sec) >
				max_history_time_check) {

			pr_debug("Deleted node with restart_time = %ld\n",
					r_log->time.tv_sec);
			list_del(&r_log->list);
			kfree(r_log);
			continue;
		}
		if (!n) {
			time_first = &r_log->time;
			pr_debug("Time_first: %ld\n", time_first->tv_sec);
		}
		n++;
		pr_debug("Restart_time: %ld\n", r_log->time.tv_sec);
	}

	if (time_first && n >= max_restarts_check) {
		if ((curr_time->tv_sec - time_first->tv_sec) <
				max_history_time_check)
			panic("Subsystems have crashed %d times in less than "
				"%ld seconds!", max_restarts_check,
				max_history_time_check);
	}

out:
	mutex_unlock(&restart_log_mutex);
}

static void for_each_subsys_device(struct subsys_device **list, unsigned count,
		void *data, void (*fn)(struct subsys_device *, void *))
{
	while (count--) {
		struct subsys_device *dev = *list++;
		if (!dev)
			continue;
		fn(dev, data);
	}
}

static void notify_each_subsys_device(struct subsys_device **list,
		unsigned count,
		enum subsys_notif_type notif, void *data)
{
	struct subsys_device *subsys;

	while (count--) {
		struct subsys_device *dev = *list++;
		struct notif_data notif_data;

		if (!dev)
			continue;

		mutex_lock(&subsys_list_lock);
		list_for_each_entry(subsys, &subsys_list, list)
			if (dev != subsys)
				sysmon_send_event(subsys->desc->name,
						dev->desc->name,
						notif);
		mutex_unlock(&subsys_list_lock);

		notif_data.crashed = subsys_get_crash_status(dev);
		notif_data.enable_ramdump = enable_ramdumps;

		subsys_notif_queue_notification(dev->notify, notif,
								&notif_data);
	}
}

static void enable_all_irqs(struct subsys_device *dev)
{
	if (dev->desc->err_ready_irq)
		enable_irq(dev->desc->err_ready_irq);
	if (dev->desc->wdog_bite_irq && dev->desc->wdog_bite_handler) {
		enable_irq(dev->desc->wdog_bite_irq);
		irq_set_irq_wake(dev->desc->wdog_bite_irq, 1);
	}
	if (dev->desc->err_fatal_irq && dev->desc->err_fatal_handler)
		enable_irq(dev->desc->err_fatal_irq);
	if (dev->desc->stop_ack_irq && dev->desc->stop_ack_handler)
		enable_irq(dev->desc->stop_ack_irq);
}

static void disable_all_irqs(struct subsys_device *dev)
{
	if (dev->desc->err_ready_irq)
		disable_irq(dev->desc->err_ready_irq);
	if (dev->desc->wdog_bite_irq && dev->desc->wdog_bite_handler) {
		disable_irq(dev->desc->wdog_bite_irq);
		irq_set_irq_wake(dev->desc->wdog_bite_irq, 0);
	}
	if (dev->desc->err_fatal_irq && dev->desc->err_fatal_handler)
		disable_irq(dev->desc->err_fatal_irq);
	if (dev->desc->stop_ack_irq && dev->desc->stop_ack_handler)
		disable_irq(dev->desc->stop_ack_irq);
}

static int wait_for_err_ready(struct subsys_device *subsys)
{
	int ret;

	if (!subsys->desc->err_ready_irq || enable_debug == 1)
		return 0;

	ret = wait_for_completion_timeout(&subsys->err_ready,
					  msecs_to_jiffies(10000));
	if (!ret) {
		pr_err("[%s]: Error ready timed out\n", subsys->desc->name);
		return -ETIMEDOUT;
	}

	return 0;
}

static void subsystem_shutdown(struct subsys_device *dev, void *data)
{
	const char *name = dev->desc->name;

	pr_info("[%p]: Shutting down %s\n", current, name);
	if (dev->desc->shutdown(dev->desc, true) < 0)
		panic("subsys-restart: [%p]: Failed to shutdown %s!",
			current, name);
	dev->crash_count++;
	subsys_set_state(dev, SUBSYS_OFFLINE);
	disable_all_irqs(dev);
}

static void subsystem_ramdump(struct subsys_device *dev, void *data)
{
	const char *name = dev->desc->name;

	if (dev->desc->ramdump)
		if (dev->desc->ramdump(enable_ramdumps, dev->desc) < 0)
			pr_warn("%s[%p]: Ramdump failed.\n", name, current);
	dev->do_ramdump_on_put = false;
}

static void subsystem_powerup(struct subsys_device *dev, void *data)
{
	const char *name = dev->desc->name;
	int ret;

	pr_info("[%p]: Powering up %s\n", current, name);
	init_completion(&dev->err_ready);

	if (dev->desc->powerup(dev->desc) < 0) {
		notify_each_subsys_device(&dev, 1, SUBSYS_POWERUP_FAILURE,
								NULL);
		panic("[%p]: Powerup error: %s!", current, name);
	}
	enable_all_irqs(dev);

	ret = wait_for_err_ready(dev);
	if (ret) {
		notify_each_subsys_device(&dev, 1, SUBSYS_POWERUP_FAILURE,
								NULL);
		panic("[%p]: Timed out waiting for error ready: %s!",
			current, name);
	}
	subsys_set_state(dev, SUBSYS_ONLINE);
	subsys_set_crash_status(dev, false);
}

static int __find_subsys(struct device *dev, void *data)
{
	struct subsys_device *subsys = to_subsys(dev);
	return !strcmp(subsys->desc->name, data);
}

static struct subsys_device *find_subsys(const char *str)
{
	struct device *dev;

	if (!str)
		return NULL;

	dev = bus_find_device(&subsys_bus_type, NULL, (void *)str,
			__find_subsys);
	return dev ? to_subsys(dev) : NULL;
}

static int subsys_start(struct subsys_device *subsys)
{
	int ret;

	notify_each_subsys_device(&subsys, 1, SUBSYS_BEFORE_POWERUP,
								NULL);

	init_completion(&subsys->err_ready);
	ret = subsys->desc->powerup(subsys->desc);
	if (ret) {
		notify_each_subsys_device(&subsys, 1, SUBSYS_POWERUP_FAILURE,
									NULL);
		return ret;
	}
	enable_all_irqs(subsys);

	if (subsys->desc->is_not_loadable) {
		subsys_set_state(subsys, SUBSYS_ONLINE);
		return 0;
	}

	ret = wait_for_err_ready(subsys);
	if (ret) {
		/* pil-boot succeeded but we need to shutdown
		 * the device because error ready timed out.
		 */
		notify_each_subsys_device(&subsys, 1, SUBSYS_POWERUP_FAILURE,
									NULL);
		subsys->desc->shutdown(subsys->desc, false);
		disable_all_irqs(subsys);
		return ret;
	} else {
		subsys_set_state(subsys, SUBSYS_ONLINE);
	}

	notify_each_subsys_device(&subsys, 1, SUBSYS_AFTER_POWERUP,
								NULL);
	return ret;
}

static void subsys_stop(struct subsys_device *subsys)
{
	notify_each_subsys_device(&subsys, 1, SUBSYS_BEFORE_SHUTDOWN, NULL);
	subsys->desc->shutdown(subsys->desc, false);
	subsys_set_state(subsys, SUBSYS_OFFLINE);
	disable_all_irqs(subsys);
	notify_each_subsys_device(&subsys, 1, SUBSYS_AFTER_SHUTDOWN, NULL);
}

static struct subsys_tracking *subsys_get_track(struct subsys_device *subsys)
{
	struct subsys_soc_restart_order *order = subsys->restart_order;

	if (order)
		return &order->track;
	else
		return &subsys->track;
}

/**
 * subsytem_get() - Boot a subsystem
 * @name: pointer to a string containing the name of the subsystem to boot
 *
 * This function returns a pointer if it succeeds. If an error occurs an
 * ERR_PTR is returned.
 *
 * If this feature is disable, the value %NULL will be returned.
 */
void *subsystem_get(const char *name)
{
	struct subsys_device *subsys;
	struct subsys_device *subsys_d;
	int ret;
	void *retval;
	struct subsys_tracking *track;

	if (!name)
		return NULL;

	subsys = retval = find_subsys(name);
	if (!subsys)
		return ERR_PTR(-ENODEV);
	if (!try_module_get(subsys->owner)) {
		retval = ERR_PTR(-ENODEV);
		goto err_module;
	}

	subsys_d = subsystem_get(subsys->desc->depends_on);
	if (IS_ERR(subsys_d)) {
		retval = subsys_d;
		goto err_depends;
	}

	track = subsys_get_track(subsys);
	mutex_lock(&track->lock);
	if (!subsys->count) {
		ret = subsys_start(subsys);
		if (ret) {
			retval = ERR_PTR(ret);
			goto err_start;
		}
	}
	subsys->count++;
	mutex_unlock(&track->lock);
	return retval;
err_start:
	mutex_unlock(&track->lock);
	subsystem_put(subsys_d);
err_depends:
	module_put(subsys->owner);
err_module:
	put_device(&subsys->dev);
	return retval;
}
EXPORT_SYMBOL(subsystem_get);

/**
 * subsystem_put() - Shutdown a subsystem
 * @peripheral_handle: pointer from a previous call to subsystem_get()
 *
 * This doesn't imply that a subsystem is shutdown until all callers of
 * subsystem_get() have called subsystem_put().
 */
void subsystem_put(void *subsystem)
{
	struct subsys_device *subsys_d, *subsys = subsystem;
	struct subsys_tracking *track;

	if (IS_ERR_OR_NULL(subsys))
		return;

	track = subsys_get_track(subsys);
	mutex_lock(&track->lock);
	if (WARN(!subsys->count, "%s: %s: Reference count mismatch\n",
			subsys->desc->name, __func__))
		goto err_out;
	if (!--subsys->count) {
		subsys_stop(subsys);
		if (subsys->do_ramdump_on_put)
			subsystem_ramdump(subsys, NULL);
	}
	mutex_unlock(&track->lock);

	subsys_d = find_subsys(subsys->desc->depends_on);
	if (subsys_d) {
		subsystem_put(subsys_d);
		put_device(&subsys_d->dev);
	}
	module_put(subsys->owner);
	put_device(&subsys->dev);
	return;
err_out:
	mutex_unlock(&track->lock);
}
EXPORT_SYMBOL(subsystem_put);

static void subsystem_restart_wq_func(struct work_struct *work)
{
	struct subsys_device *dev = container_of(work,
						struct subsys_device, work);
	struct subsys_device **list;
	struct subsys_desc *desc = dev->desc;
	struct subsys_soc_restart_order *order = dev->restart_order;
	struct subsys_tracking *track;
	unsigned count;
	unsigned long flags;

	/*
	 * It's OK to not take the registration lock at this point.
	 * This is because the subsystem list inside the relevant
	 * restart order is not being traversed.
	 */
	if (order) {
		list = order->subsys_ptrs;
		count = order->count;
		track = &order->track;
	} else {
		list = &dev;
		count = 1;
		track = &dev->track;
	}

	mutex_lock(&track->lock);
	do_epoch_check(dev);

	/*
	 * It's necessary to take the registration lock because the subsystem
	 * list in the SoC restart order will be traversed and it shouldn't be
	 * changed until _this_ restart sequence completes.
	 */
	mutex_lock(&soc_order_reg_lock);

	pr_debug("[%p]: Starting restart sequence for %s\n", current,
			desc->name);
	notify_each_subsys_device(list, count, SUBSYS_BEFORE_SHUTDOWN, NULL);
	for_each_subsys_device(list, count, NULL, subsystem_shutdown);
	notify_each_subsys_device(list, count, SUBSYS_AFTER_SHUTDOWN, NULL);

	notify_each_subsys_device(list, count, SUBSYS_RAMDUMP_NOTIFICATION,
									NULL);

	spin_lock_irqsave(&track->s_lock, flags);
	track->p_state = SUBSYS_RESTARTING;
	spin_unlock_irqrestore(&track->s_lock, flags);

	/* Collect ram dumps for all subsystems in order here */
	for_each_subsys_device(list, count, NULL, subsystem_ramdump);

	notify_each_subsys_device(list, count, SUBSYS_BEFORE_POWERUP, NULL);
	for_each_subsys_device(list, count, NULL, subsystem_powerup);
	notify_each_subsys_device(list, count, SUBSYS_AFTER_POWERUP, NULL);

	pr_info("[%p]: Restart sequence for %s completed.\n",
			current, desc->name);

	mutex_unlock(&soc_order_reg_lock);
	mutex_unlock(&track->lock);

	spin_lock_irqsave(&track->s_lock, flags);
	track->p_state = SUBSYS_NORMAL;
	__pm_relax(&dev->ssr_wlock);
	spin_unlock_irqrestore(&track->s_lock, flags);
}

static void __subsystem_restart_dev(struct subsys_device *dev)
{
	struct subsys_desc *desc = dev->desc;
	const char *name = dev->desc->name;
	struct subsys_tracking *track;
	unsigned long flags;

	pr_debug("Restarting %s [level=%s]!\n", desc->name,
			restart_levels[dev->restart_level]);

	track = subsys_get_track(dev);
	/*
	 * Allow drivers to call subsystem_restart{_dev}() as many times as
	 * they want up until the point where the subsystem is shutdown.
	 */
	spin_lock_irqsave(&track->s_lock, flags);
	if (track->p_state != SUBSYS_CRASHED) {
		if (dev->track.state == SUBSYS_ONLINE &&
		    track->p_state != SUBSYS_RESTARTING) {
			track->p_state = SUBSYS_CRASHED;
			__pm_stay_awake(&dev->ssr_wlock);
			queue_work(ssr_wq, &dev->work);
		} else {
			panic("Subsystem %s crashed during SSR!", name);
		}
	}
	spin_unlock_irqrestore(&track->s_lock, flags);
}

static void device_restart_work_hdlr(struct work_struct *work)
{
	struct subsys_device *dev = container_of(work, struct subsys_device,
							device_restart_work);

	notify_each_subsys_device(&dev, 1, SUBSYS_SOC_RESET, NULL);
	panic("subsys-restart: Resetting the SoC - %s crashed.",
							dev->desc->name);
}

int subsystem_restart_dev(struct subsys_device *dev)
{
	const char *name;

	if (!get_device(&dev->dev))
		return -ENODEV;

	if (!try_module_get(dev->owner)) {
		put_device(&dev->dev);
		return -ENODEV;
	}

	name = dev->desc->name;

	/*
	 * If a system reboot/shutdown is underway, ignore subsystem errors.
	 * However, print a message so that we know that a subsystem behaved
	 * unexpectedly here.
	 */
	if (system_state == SYSTEM_RESTART
		|| system_state == SYSTEM_POWER_OFF) {
		pr_err("%s crashed during a system poweroff/shutdown.\n", name);
		return -EBUSY;
	}

	pr_info("Restart sequence requested for %s, restart_level = %s.\n",
		name, restart_levels[dev->restart_level]);

	switch (dev->restart_level) {

	case RESET_SUBSYS_COUPLED:
		__subsystem_restart_dev(dev);
		break;
	case RESET_SOC:
		__pm_stay_awake(&dev->ssr_wlock);
		schedule_work(&dev->device_restart_work);
		return 0;
	default:
		panic("subsys-restart: Unknown restart level!\n");
		break;
	}
	module_put(dev->owner);
	put_device(&dev->dev);

	return 0;
}
EXPORT_SYMBOL(subsystem_restart_dev);

int subsystem_restart(const char *name)
{
	int ret;
	struct subsys_device *dev = find_subsys(name);

	if (!dev)
		return -ENODEV;

	ret = subsystem_restart_dev(dev);
	put_device(&dev->dev);
	return ret;
}
EXPORT_SYMBOL(subsystem_restart);

int subsystem_crashed(const char *name)
{
	struct subsys_device *dev = find_subsys(name);
	struct subsys_tracking *track;

	if (!dev)
		return -ENODEV;

	if (!get_device(&dev->dev))
		return -ENODEV;

	track = subsys_get_track(dev);

	mutex_lock(&track->lock);
	dev->do_ramdump_on_put = true;
	/*
	 * TODO: Make this work with multiple consumers where one is calling
	 * subsystem_restart() and another is calling this function. To do
	 * so would require updating private state, etc.
	 */
	mutex_unlock(&track->lock);

	put_device(&dev->dev);
	return 0;
}
EXPORT_SYMBOL(subsystem_crashed);

void subsys_set_crash_status(struct subsys_device *dev, bool crashed)
{
	dev->crashed = crashed;
}

bool subsys_get_crash_status(struct subsys_device *dev)
{
	return dev->crashed;
}

static struct subsys_device *desc_to_subsys(struct device *d)
{
	struct subsys_device *device, *subsys_dev = 0;

	mutex_lock(&subsys_list_lock);
	list_for_each_entry(device, &subsys_list, list)
		if (device->desc->dev == d)
			subsys_dev = device;
	mutex_unlock(&subsys_list_lock);
	return subsys_dev;
}

void notify_proxy_vote(struct device *device)
{
	struct subsys_device *dev = desc_to_subsys(device);

	if (dev)
		notify_each_subsys_device(&dev, 1, SUBSYS_PROXY_VOTE, NULL);
}

void notify_proxy_unvote(struct device *device)
{
	struct subsys_device *dev = desc_to_subsys(device);

	if (dev)
		notify_each_subsys_device(&dev, 1, SUBSYS_PROXY_UNVOTE, NULL);
}

#ifdef CONFIG_DEBUG_FS
static ssize_t subsys_debugfs_read(struct file *filp, char __user *ubuf,
		size_t cnt, loff_t *ppos)
{
	int r;
	char buf[40];
	struct subsys_device *subsys = filp->private_data;

	r = snprintf(buf, sizeof(buf), "%d\n", subsys->count);
	return simple_read_from_buffer(ubuf, cnt, ppos, buf, r);
}

static ssize_t subsys_debugfs_write(struct file *filp,
		const char __user *ubuf, size_t cnt, loff_t *ppos)
{
	struct subsys_device *subsys = filp->private_data;
	char buf[10];
	char *cmp;

	cnt = min(cnt, sizeof(buf) - 1);
	if (copy_from_user(&buf, ubuf, cnt))
		return -EFAULT;
	buf[cnt] = '\0';
	cmp = strstrip(buf);

	if (!strcmp(cmp, "restart")) {
		if (subsystem_restart_dev(subsys))
			return -EIO;
	} else if (!strcmp(cmp, "get")) {
		if (subsystem_get(subsys->desc->name))
			return -EIO;
	} else if (!strcmp(cmp, "put")) {
		subsystem_put(subsys);
	} else {
		return -EINVAL;
	}

	return cnt;
}

static const struct file_operations subsys_debugfs_fops = {
	.open	= simple_open,
	.read	= subsys_debugfs_read,
	.write	= subsys_debugfs_write,
};

static struct dentry *subsys_base_dir;

static int __init subsys_debugfs_init(void)
{
	subsys_base_dir = debugfs_create_dir("msm_subsys", NULL);
	return !subsys_base_dir ? -ENOMEM : 0;
}

static void subsys_debugfs_exit(void)
{
	debugfs_remove_recursive(subsys_base_dir);
}

static int subsys_debugfs_add(struct subsys_device *subsys)
{
	if (!subsys_base_dir)
		return -ENOMEM;

	subsys->dentry = debugfs_create_file(subsys->desc->name,
				S_IRUGO | S_IWUSR, subsys_base_dir,
				subsys, &subsys_debugfs_fops);
	return !subsys->dentry ? -ENOMEM : 0;
}

static void subsys_debugfs_remove(struct subsys_device *subsys)
{
	debugfs_remove(subsys->dentry);
}
#else
static int __init subsys_debugfs_init(void) { return 0; };
static void subsys_debugfs_exit(void) { }
static int subsys_debugfs_add(struct subsys_device *subsys) { return 0; }
static void subsys_debugfs_remove(struct subsys_device *subsys) { }
#endif

static int subsys_device_open(struct inode *inode, struct file *file)
{
	struct subsys_device *device, *subsys_dev = 0;
	void *retval;

	mutex_lock(&subsys_list_lock);
	list_for_each_entry(device, &subsys_list, list)
		if (MINOR(device->dev_no) == iminor(inode))
			subsys_dev = device;
	mutex_unlock(&subsys_list_lock);

	if (!subsys_dev)
		return -EINVAL;

	retval = subsystem_get(subsys_dev->desc->name);
	if (IS_ERR(retval))
		return PTR_ERR(retval);

	return 0;
}

static int subsys_device_close(struct inode *inode, struct file *file)
{
	struct subsys_device *device, *subsys_dev = 0;

	mutex_lock(&subsys_list_lock);
	list_for_each_entry(device, &subsys_list, list)
		if (MINOR(device->dev_no) == iminor(inode))
			subsys_dev = device;
	mutex_unlock(&subsys_list_lock);

	if (!subsys_dev)
		return -EINVAL;

	subsystem_put(subsys_dev);
	return 0;
}

static const struct file_operations subsys_device_fops = {
		.owner = THIS_MODULE,
		.open = subsys_device_open,
		.release = subsys_device_close,
};

static void subsys_device_release(struct device *dev)
{
	struct subsys_device *subsys = to_subsys(dev);

	wakeup_source_trash(&subsys->ssr_wlock);
	mutex_destroy(&subsys->track.lock);
	ida_simple_remove(&subsys_ida, subsys->id);
	kfree(subsys);
}
static irqreturn_t subsys_err_ready_intr_handler(int irq, void *subsys)
{
	struct subsys_device *subsys_dev = subsys;
	dev_info(subsys_dev->desc->dev,
		"Subsystem error monitoring/handling services are up\n");

	if (subsys_dev->desc->is_not_loadable)
		return IRQ_HANDLED;

	complete(&subsys_dev->err_ready);
	return IRQ_HANDLED;
}

static int subsys_char_device_add(struct subsys_device *subsys_dev)
{
	int ret = 0;
	static int major, minor;
	dev_t dev_no;

	mutex_lock(&char_device_lock);
	if (!major) {
		ret = alloc_chrdev_region(&dev_no, 0, 4, "subsys");
		if (ret < 0) {
			pr_err("Failed to alloc subsys_dev region, err %d\n",
									ret);
			goto fail;
		}
		major = MAJOR(dev_no);
		minor = MINOR(dev_no);
	} else
		dev_no = MKDEV(major, minor);

	if (!device_create(char_class, subsys_dev->desc->dev, dev_no,
			NULL, "subsys_%s", subsys_dev->desc->name)) {
		pr_err("Failed to create subsys_%s device\n",
						subsys_dev->desc->name);
		goto fail_unregister_cdev_region;
	}

	cdev_init(&subsys_dev->char_dev, &subsys_device_fops);
	subsys_dev->char_dev.owner = THIS_MODULE;
	ret = cdev_add(&subsys_dev->char_dev, dev_no, 1);
	if (ret < 0)
		goto fail_destroy_device;

	subsys_dev->dev_no = dev_no;
	minor++;
	mutex_unlock(&char_device_lock);

	return 0;

fail_destroy_device:
	device_destroy(char_class, dev_no);
fail_unregister_cdev_region:
	unregister_chrdev_region(dev_no, 1);
fail:
	mutex_unlock(&char_device_lock);
	return ret;
}

static void subsys_char_device_remove(struct subsys_device *subsys_dev)
{
	cdev_del(&subsys_dev->char_dev);
	device_destroy(char_class, subsys_dev->dev_no);
	unregister_chrdev_region(subsys_dev->dev_no, 1);
}

static struct subsys_soc_restart_order *ssr_parse_restart_orders(struct
							subsys_desc * desc)
{
	int i, j, count, num = 0;
	struct subsys_soc_restart_order *order, *tmp;
	struct device *dev = desc->dev;
	struct device_node *ssr_node;
	uint32_t len;

	if (!of_get_property(dev->of_node, "qcom,restart-group", &len))
		return NULL;

	count = len/sizeof(uint32_t);

	order = devm_kzalloc(dev, sizeof(*order), GFP_KERNEL);
	if (!order)
		return ERR_PTR(-ENOMEM);

	order->subsys_ptrs = devm_kzalloc(dev,
				count * sizeof(struct subsys_device *),
				GFP_KERNEL);
	if (!order->subsys_ptrs)
		return ERR_PTR(-ENOMEM);

	order->device_ptrs = devm_kzalloc(dev,
				count * sizeof(struct device_node *),
				GFP_KERNEL);
	if (!order->device_ptrs)
		return ERR_PTR(-ENOMEM);

	for (i = 0; i < count; i++) {
		ssr_node = of_parse_phandle(dev->of_node,
						"qcom,restart-group", i);
		if (!ssr_node)
			return ERR_PTR(-ENXIO);
		of_node_put(ssr_node);
		pr_info("%s device has been added to %s's restart group\n",
						ssr_node->name, desc->name);
		order->device_ptrs[i] = ssr_node;
	}

	/*
	 * Check for similar restart groups. If found, return
	 * without adding the new group to the ssr_order_list.
	 */
	mutex_lock(&ssr_order_mutex);
	list_for_each_entry(tmp, &ssr_order_list, list) {
		for (i = 0; i < count; i++) {
			for (j = 0; j < count; j++) {
				if (order->device_ptrs[j] !=
					tmp->device_ptrs[i])
					continue;
				else
					num++;
			}
		}

		if (num == count && tmp->count == count)
			return tmp;
		else if (num)
			return ERR_PTR(-EINVAL);
	}

	order->count = count;
	mutex_init(&order->track.lock);
	spin_lock_init(&order->track.s_lock);

	INIT_LIST_HEAD(&order->list);
	list_add_tail(&order->list, &ssr_order_list);
	mutex_unlock(&ssr_order_mutex);

	return order;
}

static int __get_gpio(struct subsys_desc *desc, const char *prop,
		int *gpio)
{
	struct device_node *dnode = desc->dev->of_node;
	int ret = -ENOENT;

	if (of_find_property(dnode, prop, NULL)) {
		*gpio = of_get_named_gpio(dnode, prop, 0);
		ret = *gpio < 0 ? *gpio : 0;
	}

	return ret;
}

static int __get_irq(struct subsys_desc *desc, const char *prop,
		unsigned int *irq)
{
	int ret, gpio, irql;

	ret = __get_gpio(desc, prop, &gpio);
	if (ret)
		return ret;

	irql = gpio_to_irq(gpio);

	if (irql == -ENOENT)
		irql = -ENXIO;

	if (irql < 0) {
		pr_err("[%s]: Error getting IRQ \"%s\"\n", desc->name,
				prop);
		return irql;
	} else {
		*irq = irql;
	}

	return 0;
}

static int subsys_parse_devicetree(struct subsys_desc *desc)
{
	struct subsys_soc_restart_order *order;
	int ret;

	struct platform_device *pdev = container_of(desc->dev,
					struct platform_device, dev);

	ret = __get_irq(desc, "qcom,gpio-err-fatal", &desc->err_fatal_irq);
	if (ret && ret != -ENOENT)
		return ret;

	ret = __get_irq(desc, "qcom,gpio-err-ready", &desc->err_ready_irq);
	if (ret && ret != -ENOENT)
		return ret;

	ret = __get_irq(desc, "qcom,gpio-stop-ack", &desc->stop_ack_irq);
	if (ret && ret != -ENOENT)
		return ret;

	ret = __get_gpio(desc, "qcom,gpio-force-stop", &desc->force_stop_gpio);
	if (ret && ret != -ENOENT)
		return ret;

	ret = platform_get_irq(pdev, 0);
	if (ret > 0)
		desc->wdog_bite_irq = ret;

	order = ssr_parse_restart_orders(desc);
	if (IS_ERR(order)) {
		pr_err("Could not initialize SSR restart order, err = %ld\n",
							PTR_ERR(order));
		return PTR_ERR(order);
	}

	return 0;
}

static int subsys_setup_irqs(struct subsys_device *subsys)
{
	struct subsys_desc *desc = subsys->desc;
	int ret;

	if (desc->err_fatal_irq && desc->err_fatal_handler) {
		ret = devm_request_irq(desc->dev, desc->err_fatal_irq,
				desc->err_fatal_handler,
				IRQF_TRIGGER_RISING, desc->name, desc);
		if (ret < 0) {
			dev_err(desc->dev, "[%s]: Unable to register error fatal IRQ handler!: %d\n",
				desc->name, ret);
			return ret;
		}
		disable_irq(desc->err_fatal_irq);
	}

	if (desc->stop_ack_irq && desc->stop_ack_handler) {
		ret = devm_request_irq(desc->dev, desc->stop_ack_irq,
			desc->stop_ack_handler,
			IRQF_TRIGGER_RISING, desc->name, desc);
		if (ret < 0) {
			dev_err(desc->dev, "[%s]: Unable to register stop ack handler!: %d\n",
				desc->name, ret);
			return ret;
		}
		disable_irq(desc->stop_ack_irq);
	}

	if (desc->wdog_bite_irq && desc->wdog_bite_handler) {
		ret = devm_request_irq(desc->dev, desc->wdog_bite_irq,
			desc->wdog_bite_handler,
			IRQF_TRIGGER_RISING, desc->name, desc);
		if (ret < 0) {
			dev_err(desc->dev, "[%s]: Unable to register wdog bite handler!: %d\n",
				desc->name, ret);
			return ret;
		}
		disable_irq(desc->wdog_bite_irq);
	}

	if (desc->err_ready_irq) {
		ret = devm_request_irq(desc->dev,
					desc->err_ready_irq,
					subsys_err_ready_intr_handler,
					IRQF_TRIGGER_RISING,
					"error_ready_interrupt", subsys);
		if (ret < 0) {
			dev_err(desc->dev,
				"[%s]: Unable to register err ready handler\n",
				desc->name);
			return ret;
		}
		disable_irq(desc->err_ready_irq);
	}

	return 0;
}

struct subsys_device *subsys_register(struct subsys_desc *desc)
{
	struct subsys_device *subsys;
	int ret;

	subsys = kzalloc(sizeof(*subsys), GFP_KERNEL);
	if (!subsys)
		return ERR_PTR(-ENOMEM);

	subsys->desc = desc;
	subsys->owner = desc->owner;
	subsys->dev.parent = desc->dev;
	subsys->dev.bus = &subsys_bus_type;
	subsys->dev.release = subsys_device_release;

	subsys->notify = subsys_notif_add_subsys(desc->name);

	snprintf(subsys->wlname, sizeof(subsys->wlname), "ssr(%s)", desc->name);
	wakeup_source_init(&subsys->ssr_wlock, subsys->wlname);
	INIT_WORK(&subsys->work, subsystem_restart_wq_func);
	INIT_WORK(&subsys->device_restart_work, device_restart_work_hdlr);
	spin_lock_init(&subsys->track.s_lock);

	subsys->id = ida_simple_get(&subsys_ida, 0, 0, GFP_KERNEL);
	if (subsys->id < 0) {
		ret = subsys->id;
		goto err_ida;
	}
	dev_set_name(&subsys->dev, "subsys%d", subsys->id);

	mutex_init(&subsys->track.lock);

	ret = subsys_debugfs_add(subsys);
	if (ret)
		goto err_debugfs;

	ret = device_register(&subsys->dev);
	if (ret) {
		device_unregister(&subsys->dev);
		goto err_register;
	}

	ret = subsys_char_device_add(subsys);
	if (ret) {
		put_device(&subsys->dev);
		goto err_register;
	}

	if (desc->dev->of_node) {
		ret = subsys_parse_devicetree(desc);
		if (ret)
			goto err_register;

		subsys->restart_order = update_restart_order(subsys);

		ret = subsys_setup_irqs(subsys);
		if (ret < 0)
			goto err_register;
	}

	mutex_lock(&subsys_list_lock);
	INIT_LIST_HEAD(&subsys->list);
	list_add_tail(&subsys->list, &subsys_list);
	mutex_unlock(&subsys_list_lock);

	return subsys;

err_register:
	subsys_debugfs_remove(subsys);
err_debugfs:
	mutex_destroy(&subsys->track.lock);
	ida_simple_remove(&subsys_ida, subsys->id);
err_ida:
	wakeup_source_trash(&subsys->ssr_wlock);
	kfree(subsys);
	return ERR_PTR(ret);
}
EXPORT_SYMBOL(subsys_register);

void subsys_unregister(struct subsys_device *subsys)
{
	struct subsys_device *subsys_dev, *tmp;

	if (IS_ERR_OR_NULL(subsys))
		return;

	if (get_device(&subsys->dev)) {
		mutex_lock(&subsys_list_lock);
		list_for_each_entry_safe(subsys_dev, tmp, &subsys_list, list)
			if (subsys_dev == subsys)
				list_del(&subsys->list);
		mutex_unlock(&subsys_list_lock);
		mutex_lock(&subsys->track.lock);
		WARN_ON(subsys->count);
		device_unregister(&subsys->dev);
		mutex_unlock(&subsys->track.lock);
		subsys_debugfs_remove(subsys);
		subsys_char_device_remove(subsys);
		put_device(&subsys->dev);
	}
}
EXPORT_SYMBOL(subsys_unregister);

static int subsys_panic(struct device *dev, void *data)
{
	struct subsys_device *subsys = to_subsys(dev);

	if (subsys->desc->crash_shutdown)
		subsys->desc->crash_shutdown(subsys->desc);
	return 0;
}

static int ssr_panic_handler(struct notifier_block *this,
				unsigned long event, void *ptr)
{
	bus_for_each_dev(&subsys_bus_type, NULL, NULL, subsys_panic);
	return NOTIFY_DONE;
}

static struct notifier_block panic_nb = {
	.notifier_call  = ssr_panic_handler,
};

static int __init subsys_restart_init(void)
{
	int ret;

	ssr_wq = alloc_workqueue("ssr_wq", WQ_CPU_INTENSIVE, 0);
	BUG_ON(!ssr_wq);

	ret = bus_register(&subsys_bus_type);
	if (ret)
		goto err_bus;
	ret = subsys_debugfs_init();
	if (ret)
		goto err_debugfs;

	char_class = class_create(THIS_MODULE, "subsys");
	if (IS_ERR(char_class)) {
		ret = -ENOMEM;
		pr_err("Failed to create subsys_dev class\n");
		goto err_class;
	}

	ret = atomic_notifier_chain_register(&panic_notifier_list,
			&panic_nb);
	if (ret)
		goto err_soc;

	return 0;

err_soc:
	class_destroy(char_class);
err_class:
	subsys_debugfs_exit();
err_debugfs:
	bus_unregister(&subsys_bus_type);
err_bus:
	destroy_workqueue(ssr_wq);
	return ret;
}
arch_initcall(subsys_restart_init);

MODULE_DESCRIPTION("Subsystem Restart Driver");
MODULE_LICENSE("GPL v2");
