/* Copyright (c) 2011-2012, Code Aurora Forum. All rights reserved.
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
#include <linux/proc_fs.h>
#include <linux/delay.h>
#include <linux/list.h>
#include <linux/io.h>
#include <linux/kthread.h>
#include <linux/time.h>
#include <linux/wakelock.h>
#include <linux/suspend.h>

#include <asm/current.h>

#include <mach/peripheral-loader.h>
#include <mach/scm.h>
#include <mach/socinfo.h>
#include <mach/subsystem_notif.h>
#include <mach/subsystem_restart.h>

#include "smd_private.h"

struct subsys_soc_restart_order {
	const char * const *subsystem_list;
	int count;

	struct mutex shutdown_lock;
	struct mutex powerup_lock;
	struct subsys_data *subsys_ptrs[];
};

struct restart_wq_data {
	struct subsys_data *subsys;
	struct wake_lock ssr_wake_lock;
	char wlname[64];
	int use_restart_order;
	struct work_struct work;
};

struct restart_log {
	struct timeval time;
	struct subsys_data *subsys;
	struct list_head list;
};

static int restart_level;
static int enable_ramdumps;
struct workqueue_struct *ssr_wq;

static LIST_HEAD(restart_log_list);
static LIST_HEAD(subsystem_list);
static DEFINE_SPINLOCK(subsystem_list_lock);
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

module_param(enable_ramdumps, int, S_IRUGO | S_IWUSR);

static struct subsys_soc_restart_order *_update_restart_order(
		struct subsys_data *subsys);

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
	break;

	}
	return 0;
}

module_param_call(restart_level, restart_level_set, param_get_int,
			&restart_level, 0644);

static struct subsys_data *_find_subsystem(const char *subsys_name)
{
	struct subsys_data *subsys;
	unsigned long flags;

	spin_lock_irqsave(&subsystem_list_lock, flags);
	list_for_each_entry(subsys, &subsystem_list, list)
		if (!strncmp(subsys->name, subsys_name,
				SUBSYS_NAME_MAX_LENGTH)) {
			spin_unlock_irqrestore(&subsystem_list_lock, flags);
			return subsys;
		}
	spin_unlock_irqrestore(&subsystem_list_lock, flags);

	return NULL;
}

static struct subsys_soc_restart_order *_update_restart_order(
		struct subsys_data *subsys)
{
	int i, j;

	if (!subsys)
		return NULL;

	if (!subsys->name)
		return NULL;

	mutex_lock(&soc_order_reg_lock);
	for (j = 0; j < n_restart_orders; j++) {
		for (i = 0; i < restart_orders[j]->count; i++)
			if (!strncmp(restart_orders[j]->subsystem_list[i],
				subsys->name, SUBSYS_NAME_MAX_LENGTH)) {

					restart_orders[j]->subsys_ptrs[i] =
						subsys;
					mutex_unlock(&soc_order_reg_lock);
					return restart_orders[j];
			}
	}

	mutex_unlock(&soc_order_reg_lock);

	return NULL;
}

static void _send_notification_to_order(struct subsys_data
			**restart_list, int count,
			enum subsys_notif_type notif_type)
{
	int i;

	for (i = 0; i < count; i++)
		if (restart_list[i])
			subsys_notif_queue_notification(
				restart_list[i]->notif_handle, notif_type);
}

static int max_restarts;
module_param(max_restarts, int, 0644);

static long max_history_time = 3600;
module_param(max_history_time, long, 0644);

static void do_epoch_check(struct subsys_data *subsys)
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
	r_log->subsys = subsys;
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

static void subsystem_restart_wq_func(struct work_struct *work)
{
	struct restart_wq_data *r_work = container_of(work,
						struct restart_wq_data, work);
	struct subsys_data **restart_list;
	struct subsys_data *subsys = r_work->subsys;
	struct subsys_soc_restart_order *soc_restart_order = NULL;

	struct mutex *powerup_lock;
	struct mutex *shutdown_lock;

	int i;
	int restart_list_count = 0;

	if (r_work->use_restart_order)
		soc_restart_order = subsys->restart_order;

	/* It's OK to not take the registration lock at this point.
	 * This is because the subsystem list inside the relevant
	 * restart order is not being traversed.
	 */
	if (!soc_restart_order) {
		restart_list = subsys->single_restart_list;
		restart_list_count = 1;
		powerup_lock = &subsys->powerup_lock;
		shutdown_lock = &subsys->shutdown_lock;
	} else {
		restart_list = soc_restart_order->subsys_ptrs;
		restart_list_count = soc_restart_order->count;
		powerup_lock = &soc_restart_order->powerup_lock;
		shutdown_lock = &soc_restart_order->shutdown_lock;
	}

	pr_debug("[%p]: Attempting to get shutdown lock!\n", current);

	/* Try to acquire shutdown_lock. If this fails, these subsystems are
	 * already being restarted - return.
	 */
	if (!mutex_trylock(shutdown_lock))
		goto out;

	pr_debug("[%p]: Attempting to get powerup lock!\n", current);

	/* Now that we've acquired the shutdown lock, either we're the first to
	 * restart these subsystems or some other thread is doing the powerup
	 * sequence for these subsystems. In the latter case, panic and bail
	 * out, since a subsystem died in its powerup sequence.
	 */
	if (!mutex_trylock(powerup_lock))
		panic("%s[%p]: Subsystem died during powerup!",
						__func__, current);

	do_epoch_check(subsys);

	/* Now it is necessary to take the registration lock. This is because
	 * the subsystem list in the SoC restart order will be traversed
	 * and it shouldn't be changed until _this_ restart sequence completes.
	 */
	mutex_lock(&soc_order_reg_lock);

	pr_debug("[%p]: Starting restart sequence for %s\n", current,
			r_work->subsys->name);

	_send_notification_to_order(restart_list,
				restart_list_count,
				SUBSYS_BEFORE_SHUTDOWN);

	for (i = 0; i < restart_list_count; i++) {

		if (!restart_list[i])
			continue;

		pr_info("[%p]: Shutting down %s\n", current,
			restart_list[i]->name);

		if (restart_list[i]->shutdown(subsys) < 0)
			panic("subsys-restart: %s[%p]: Failed to shutdown %s!",
				__func__, current, restart_list[i]->name);
	}

	_send_notification_to_order(restart_list, restart_list_count,
				SUBSYS_AFTER_SHUTDOWN);

	/* Now that we've finished shutting down these subsystems, release the
	 * shutdown lock. If a subsystem restart request comes in for a
	 * subsystem in _this_ restart order after the unlock below, and
	 * before the powerup lock is released, panic and bail out.
	 */
	mutex_unlock(shutdown_lock);

	/* Collect ram dumps for all subsystems in order here */
	for (i = 0; i < restart_list_count; i++) {
		if (!restart_list[i])
			continue;

		if (restart_list[i]->ramdump)
			if (restart_list[i]->ramdump(enable_ramdumps,
							subsys) < 0)
				pr_warn("%s[%p]: Ramdump failed.\n",
						restart_list[i]->name, current);
	}

	_send_notification_to_order(restart_list,
			restart_list_count,
			SUBSYS_BEFORE_POWERUP);

	for (i = restart_list_count - 1; i >= 0; i--) {

		if (!restart_list[i])
			continue;

		pr_info("[%p]: Powering up %s\n", current,
					restart_list[i]->name);

		if (restart_list[i]->powerup(subsys) < 0)
			panic("%s[%p]: Failed to powerup %s!", __func__,
				current, restart_list[i]->name);
	}

	_send_notification_to_order(restart_list,
				restart_list_count,
				SUBSYS_AFTER_POWERUP);

	pr_info("[%p]: Restart sequence for %s completed.\n",
			current, r_work->subsys->name);

	mutex_unlock(powerup_lock);

	mutex_unlock(&soc_order_reg_lock);

	pr_debug("[%p]: Released powerup lock!\n", current);

out:
	wake_unlock(&r_work->ssr_wake_lock);
	wake_lock_destroy(&r_work->ssr_wake_lock);
	kfree(r_work);
}

static void __subsystem_restart(struct subsys_data *subsys)
{
	struct restart_wq_data *data = NULL;
	int rc;

	pr_debug("Restarting %s [level=%d]!\n", subsys->name,
				restart_level);

	data = kzalloc(sizeof(struct restart_wq_data), GFP_ATOMIC);
	if (!data)
		panic("%s: Unable to allocate memory to restart %s.",
		      __func__, subsys->name);

	data->subsys = subsys;

	if (restart_level != RESET_SUBSYS_INDEPENDENT)
		data->use_restart_order = 1;

	snprintf(data->wlname, sizeof(data->wlname), "ssr(%s)", subsys->name);
	wake_lock_init(&data->ssr_wake_lock, WAKE_LOCK_SUSPEND, data->wlname);
	wake_lock(&data->ssr_wake_lock);

	INIT_WORK(&data->work, subsystem_restart_wq_func);
	rc = queue_work(ssr_wq, &data->work);
	if (rc < 0)
		panic("%s: Unable to schedule work to restart %s (%d).",
		     __func__, subsys->name, rc);
}

int subsystem_restart(const char *subsys_name)
{
	struct subsys_data *subsys;

	if (!subsys_name) {
		pr_err("Invalid subsystem name.\n");
		return -EINVAL;
	}

	pr_info("Restart sequence requested for %s, restart_level = %d.\n",
		subsys_name, restart_level);

	/* List of subsystems is protected by a lock. New subsystems can
	 * still come in.
	 */
	subsys = _find_subsystem(subsys_name);

	if (!subsys) {
		pr_warn("Unregistered subsystem %s!\n", subsys_name);
		return -EINVAL;
	}

	switch (restart_level) {

	case RESET_SUBSYS_COUPLED:
	case RESET_SUBSYS_INDEPENDENT:
		__subsystem_restart(subsys);
		break;

	case RESET_SOC:
		panic("subsys-restart: Resetting the SoC - %s crashed.",
			subsys->name);
		break;

	default:
		panic("subsys-restart: Unknown restart level!\n");
	break;

	}

	return 0;
}
EXPORT_SYMBOL(subsystem_restart);

int ssr_register_subsystem(struct subsys_data *subsys)
{
	unsigned long flags;

	if (!subsys)
		goto err;

	if (!subsys->name)
		goto err;

	if (!subsys->powerup || !subsys->shutdown)
		goto err;

	subsys->notif_handle = subsys_notif_add_subsys(subsys->name);
	subsys->restart_order = _update_restart_order(subsys);
	subsys->single_restart_list[0] = subsys;

	mutex_init(&subsys->shutdown_lock);
	mutex_init(&subsys->powerup_lock);

	spin_lock_irqsave(&subsystem_list_lock, flags);
	list_add(&subsys->list, &subsystem_list);
	spin_unlock_irqrestore(&subsystem_list_lock, flags);

	return 0;

err:
	return -EINVAL;
}
EXPORT_SYMBOL(ssr_register_subsystem);

static int ssr_panic_handler(struct notifier_block *this,
				unsigned long event, void *ptr)
{
	struct subsys_data *subsys;

	list_for_each_entry(subsys, &subsystem_list, list)
		if (subsys->crash_shutdown)
			subsys->crash_shutdown(subsys);
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
	    cpu_is_msm9615() || cpu_is_apq8064() || cpu_is_msm8627()) {
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
	int ret = 0;

	restart_level = RESET_SOC;

	ssr_wq = alloc_workqueue("ssr_wq", 0, 0);

	if (!ssr_wq)
		panic("Couldn't allocate workqueue for subsystem restart.\n");

	ret = ssr_init_soc_restart_orders();

	return ret;
}

arch_initcall(subsys_restart_init);

MODULE_DESCRIPTION("Subsystem Restart Driver");
MODULE_LICENSE("GPL v2");
