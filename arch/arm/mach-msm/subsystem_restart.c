/* Copyright (c) 2011-2012, The Linux Foundation. All rights reserved.
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

#include <asm/current.h>

#include <mach/peripheral-loader.h>
#include <mach/socinfo.h>
#include <mach/subsystem_notif.h>
#include <mach/subsystem_restart.h>

#include "smd_private.h"

struct subsys_soc_restart_order {
	const char * const *subsystem_list;
	int count;

	struct mutex shutdown_lock;
	struct mutex powerup_lock;
	struct subsys_device *subsys_ptrs[];
};

struct restart_log {
	struct timeval time;
	struct subsys_device *dev;
	struct list_head list;
};

struct subsys_device {
	struct subsys_desc *desc;
	struct list_head list;
	struct wake_lock wake_lock;
	char wlname[64];
	struct work_struct work;
	spinlock_t restart_lock;
	int restart_count;

	void *notify;

	struct mutex shutdown_lock;
	struct mutex powerup_lock;

	void *restart_order;
};

static int enable_ramdumps;
module_param(enable_ramdumps, int, S_IRUGO | S_IWUSR);

struct workqueue_struct *ssr_wq;

static LIST_HEAD(restart_log_list);
static LIST_HEAD(subsystem_list);
static DEFINE_MUTEX(subsystem_list_lock);
static DEFINE_MUTEX(soc_order_reg_lock);
static DEFINE_MUTEX(restart_log_mutex);

/* SOC specific restart orders go here */

#define DEFINE_SINGLE_RESTART_ORDER(name, order)		\
	static struct subsys_soc_restart_order __##name = {	\
		.subsystem_list = order,			\
		.count = ARRAY_SIZE(order),			\
		.subsys_ptrs = {[ARRAY_SIZE(order)] = NULL}	\
	};							\
	static struct subsys_soc_restart_order *name[] = {      \
		&__##name,					\
	}

/* MSM 8x60 restart ordering info */
static const char * const _order_8x60_all[] = {
	"external_modem",  "modem", "lpass"
};
DEFINE_SINGLE_RESTART_ORDER(orders_8x60_all, _order_8x60_all);

static const char * const _order_8x60_modems[] = {"external_modem", "modem"};
DEFINE_SINGLE_RESTART_ORDER(orders_8x60_modems, _order_8x60_modems);

/* MSM 8960 restart ordering info */
static const char * const order_8960[] = {"modem", "lpass"};
/*SGLTE restart ordering info*/
static const char * const order_8960_sglte[] = {"external_modem",
						"modem"};

static struct subsys_soc_restart_order restart_orders_8960_one = {
	.subsystem_list = order_8960,
	.count = ARRAY_SIZE(order_8960),
	.subsys_ptrs = {[ARRAY_SIZE(order_8960)] = NULL}
	};

static struct subsys_soc_restart_order restart_orders_8960_fusion_sglte = {
	.subsystem_list = order_8960_sglte,
	.count = ARRAY_SIZE(order_8960_sglte),
	.subsys_ptrs = {[ARRAY_SIZE(order_8960_sglte)] = NULL}
	};

static struct subsys_soc_restart_order *restart_orders_8960[] = {
	&restart_orders_8960_one,
	};

static struct subsys_soc_restart_order *restart_orders_8960_sglte[] = {
	&restart_orders_8960_fusion_sglte,
	};

/* These will be assigned to one of the sets above after
 * runtime SoC identification.
 */
static struct subsys_soc_restart_order **restart_orders;
static int n_restart_orders;

static int restart_level = RESET_SOC;

int get_restart_level()
{
	return restart_level;
}
EXPORT_SYMBOL(get_restart_level);

static int restart_level_set(const char *val, struct kernel_param *kp)
{
	int ret;
	int old_val = restart_level;

	if (cpu_is_msm9615()) {
		pr_err("Only Phase 1 subsystem restart is supported\n");
		return -EINVAL;
	}

	ret = param_set_int(val, kp);
	if (ret)
		return ret;

	switch (restart_level) {
	case RESET_SOC:
	case RESET_SUBSYS_COUPLED:
	case RESET_SUBSYS_INDEPENDENT:
		pr_info("Phase %d behavior activated.\n", restart_level);
		break;
	default:
		restart_level = old_val;
		return -EINVAL;
	}
	return 0;
}

module_param_call(restart_level, restart_level_set, param_get_int,
			&restart_level, 0644);

static struct subsys_soc_restart_order *
update_restart_order(struct subsys_device *dev)
{
	int i, j;
	struct subsys_soc_restart_order *order;
	const char *name = dev->desc->name;
	int len = SUBSYS_NAME_MAX_LENGTH;

	mutex_lock(&soc_order_reg_lock);
	for (j = 0; j < n_restart_orders; j++) {
		order = restart_orders[j];
		for (i = 0; i < order->count; i++) {
			if (!strncmp(order->subsystem_list[i], name, len)) {
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

static void __send_notification_to_order(struct subsys_device *dev, void *data)
{
	enum subsys_notif_type type = (enum subsys_notif_type)data;

	subsys_notif_queue_notification(dev->notify, type);
}

static void send_notification_to_order(struct subsys_device **l, unsigned n,
		enum subsys_notif_type t)
{
	for_each_subsys_device(l, n, (void *)t, __send_notification_to_order);
}

static void subsystem_shutdown(struct subsys_device *dev, void *data)
{
	const char *name = dev->desc->name;

	pr_info("[%p]: Shutting down %s\n", current, name);
	if (dev->desc->shutdown(dev->desc) < 0)
		panic("subsys-restart: [%p]: Failed to shutdown %s!",
			current, name);
}

static void subsystem_ramdump(struct subsys_device *dev, void *data)
{
	const char *name = dev->desc->name;

	if (dev->desc->ramdump)
		if (dev->desc->ramdump(enable_ramdumps, dev->desc) < 0)
			pr_warn("%s[%p]: Ramdump failed.\n", name, current);
}

static void subsystem_powerup(struct subsys_device *dev, void *data)
{
	const char *name = dev->desc->name;

	pr_info("[%p]: Powering up %s\n", current, name);
	if (dev->desc->powerup(dev->desc) < 0)
		panic("[%p]: Failed to powerup %s!", current, name);
}

static void subsystem_restart_wq_func(struct work_struct *work)
{
	struct subsys_device *dev = container_of(work,
						struct subsys_device, work);
	struct subsys_device **list;
	struct subsys_desc *desc = dev->desc;
	struct subsys_soc_restart_order *soc_restart_order = NULL;
	struct mutex *powerup_lock;
	struct mutex *shutdown_lock;
	unsigned count;
	unsigned long flags;

	if (restart_level != RESET_SUBSYS_INDEPENDENT)
		soc_restart_order = dev->restart_order;

	/*
	 * It's OK to not take the registration lock at this point.
	 * This is because the subsystem list inside the relevant
	 * restart order is not being traversed.
	 */
	if (!soc_restart_order) {
		list = &dev;
		count = 1;
		powerup_lock = &dev->powerup_lock;
		shutdown_lock = &dev->shutdown_lock;
	} else {
		list = soc_restart_order->subsys_ptrs;
		count = soc_restart_order->count;
		powerup_lock = &soc_restart_order->powerup_lock;
		shutdown_lock = &soc_restart_order->shutdown_lock;
	}

	pr_debug("[%p]: Attempting to get shutdown lock!\n", current);

	/*
	 * Try to acquire shutdown_lock. If this fails, these subsystems are
	 * already being restarted - return.
	 */
	if (!mutex_trylock(shutdown_lock))
		goto out;

	pr_debug("[%p]: Attempting to get powerup lock!\n", current);

	/*
	 * Now that we've acquired the shutdown lock, either we're the first to
	 * restart these subsystems or some other thread is doing the powerup
	 * sequence for these subsystems. In the latter case, panic and bail
	 * out, since a subsystem died in its powerup sequence.
	 */
	if (!mutex_trylock(powerup_lock))
		panic("%s[%p]: Subsystem died during powerup!",
						__func__, current);

	do_epoch_check(dev);

	/*
	 * It's necessary to take the registration lock because the subsystem
	 * list in the SoC restart order will be traversed and it shouldn't be
	 * changed until _this_ restart sequence completes.
	 */
	mutex_lock(&soc_order_reg_lock);

	pr_debug("[%p]: Starting restart sequence for %s\n", current,
			desc->name);
	send_notification_to_order(list, count, SUBSYS_BEFORE_SHUTDOWN);
	for_each_subsys_device(list, count, NULL, subsystem_shutdown);
	send_notification_to_order(list, count, SUBSYS_AFTER_SHUTDOWN);

	/*
	 * Now that we've finished shutting down these subsystems, release the
	 * shutdown lock. If a subsystem restart request comes in for a
	 * subsystem in _this_ restart order after the unlock below, and
	 * before the powerup lock is released, panic and bail out.
	 */
	mutex_unlock(shutdown_lock);

	/* Collect ram dumps for all subsystems in order here */
	for_each_subsys_device(list, count, NULL, subsystem_ramdump);

	send_notification_to_order(list, count, SUBSYS_BEFORE_POWERUP);
	for_each_subsys_device(list, count, NULL, subsystem_powerup);
	send_notification_to_order(list, count, SUBSYS_AFTER_POWERUP);

	pr_info("[%p]: Restart sequence for %s completed.\n",
			current, desc->name);

	mutex_unlock(powerup_lock);

	mutex_unlock(&soc_order_reg_lock);

	pr_debug("[%p]: Released powerup lock!\n", current);

out:
	spin_lock_irqsave(&dev->restart_lock, flags);
	dev->restart_count--;
	if (!dev->restart_count)
		wake_unlock(&dev->wake_lock);
	spin_unlock_irqrestore(&dev->restart_lock, flags);
}

static void __subsystem_restart_dev(struct subsys_device *dev)
{
	struct subsys_desc *desc = dev->desc;
	unsigned long flags;

	pr_debug("Restarting %s [level=%d]!\n", desc->name, restart_level);

	spin_lock_irqsave(&dev->restart_lock, flags);
	if (!dev->restart_count)
		wake_lock(&dev->wake_lock);
	dev->restart_count++;
	spin_unlock_irqrestore(&dev->restart_lock, flags);

	if (!queue_work(ssr_wq, &dev->work)) {
		spin_lock_irqsave(&dev->restart_lock, flags);
		dev->restart_count--;
		if (!dev->restart_count)
			wake_unlock(&dev->wake_lock);
		spin_unlock_irqrestore(&dev->restart_lock, flags);
	}
}

int subsystem_restart_dev(struct subsys_device *dev)
{
	const char *name = dev->desc->name;

	pr_info("Restart sequence requested for %s, restart_level = %d.\n",
		name, restart_level);

	switch (restart_level) {

	case RESET_SUBSYS_COUPLED:
	case RESET_SUBSYS_INDEPENDENT:
		__subsystem_restart_dev(dev);
		break;
	case RESET_SOC:
		panic("subsys-restart: Resetting the SoC - %s crashed.", name);
		break;
	default:
		panic("subsys-restart: Unknown restart level!\n");
		break;
	}

	return 0;
}
EXPORT_SYMBOL(subsystem_restart_dev);

int subsystem_restart(const char *name)
{
	struct subsys_device *dev;

	mutex_lock(&subsystem_list_lock);
	list_for_each_entry(dev, &subsystem_list, list)
		if (!strncmp(dev->desc->name, name, SUBSYS_NAME_MAX_LENGTH))
			goto found;
	dev = NULL;
found:
	mutex_unlock(&subsystem_list_lock);
	if (dev)
		return subsystem_restart_dev(dev);
	return -ENODEV;
}
EXPORT_SYMBOL(subsystem_restart);

struct subsys_device *subsys_register(struct subsys_desc *desc)
{
	struct subsys_device *dev;

	dev = kzalloc(sizeof(*dev), GFP_KERNEL);
	if (!dev)
		return ERR_PTR(-ENOMEM);

	dev->desc = desc;
	dev->notify = subsys_notif_add_subsys(desc->name);
	dev->restart_order = update_restart_order(dev);

	snprintf(dev->wlname, sizeof(dev->wlname), "ssr(%s)", desc->name);
	wake_lock_init(&dev->wake_lock, WAKE_LOCK_SUSPEND, dev->wlname);
	INIT_WORK(&dev->work, subsystem_restart_wq_func);
	spin_lock_init(&dev->restart_lock);

	mutex_init(&dev->shutdown_lock);
	mutex_init(&dev->powerup_lock);

	mutex_lock(&subsystem_list_lock);
	list_add(&dev->list, &subsystem_list);
	mutex_unlock(&subsystem_list_lock);

	return dev;
}
EXPORT_SYMBOL(subsys_register);

void subsys_unregister(struct subsys_device *dev)
{
	if (IS_ERR_OR_NULL(dev))
		return;
	mutex_lock(&subsystem_list_lock);
	list_del(&dev->list);
	mutex_unlock(&subsystem_list_lock);
	wake_lock_destroy(&dev->wake_lock);
	kfree(dev);
}
EXPORT_SYMBOL(subsys_unregister);

static int ssr_panic_handler(struct notifier_block *this,
				unsigned long event, void *ptr)
{
	struct subsys_device *dev;

	list_for_each_entry(dev, &subsystem_list, list)
		if (dev->desc->crash_shutdown)
			dev->desc->crash_shutdown(dev->desc);
	return NOTIFY_DONE;
}

static struct notifier_block panic_nb = {
	.notifier_call  = ssr_panic_handler,
};

static int __init ssr_init_soc_restart_orders(void)
{
	int i;

	atomic_notifier_chain_register(&panic_notifier_list,
			&panic_nb);

	if (cpu_is_msm8x60()) {
		for (i = 0; i < ARRAY_SIZE(orders_8x60_all); i++) {
			mutex_init(&orders_8x60_all[i]->powerup_lock);
			mutex_init(&orders_8x60_all[i]->shutdown_lock);
		}

		for (i = 0; i < ARRAY_SIZE(orders_8x60_modems); i++) {
			mutex_init(&orders_8x60_modems[i]->powerup_lock);
			mutex_init(&orders_8x60_modems[i]->shutdown_lock);
		}

		restart_orders = orders_8x60_all;
		n_restart_orders = ARRAY_SIZE(orders_8x60_all);
	}

	if (cpu_is_msm8960() || cpu_is_msm8930() || cpu_is_msm8930aa() ||
	    cpu_is_msm9615() || cpu_is_apq8064() || cpu_is_msm8627() ||
	    cpu_is_msm8960ab()) {
		if (socinfo_get_platform_subtype() == PLATFORM_SUBTYPE_SGLTE) {
			restart_orders = restart_orders_8960_sglte;
			n_restart_orders =
				ARRAY_SIZE(restart_orders_8960_sglte);
		} else {
			restart_orders = restart_orders_8960;
			n_restart_orders = ARRAY_SIZE(restart_orders_8960);
		}
		for (i = 0; i < n_restart_orders; i++) {
			mutex_init(&restart_orders[i]->powerup_lock);
			mutex_init(&restart_orders[i]->shutdown_lock);
		}
	}

	if (restart_orders == NULL || n_restart_orders < 1) {
		WARN_ON(1);
		return -EINVAL;
	}

	return 0;
}

static int __init subsys_restart_init(void)
{
	ssr_wq = alloc_workqueue("ssr_wq", 0, 0);
	if (!ssr_wq)
		panic("Couldn't allocate workqueue for subsystem restart.\n");

	return ssr_init_soc_restart_orders();
}
arch_initcall(subsys_restart_init);

MODULE_DESCRIPTION("Subsystem Restart Driver");
MODULE_LICENSE("GPL v2");
