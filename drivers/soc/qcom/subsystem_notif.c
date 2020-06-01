// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2011, 2013, 2016-2019, The Linux Foundation. All rights reserved.
 */
/*
 * Subsystem Notifier -- Provides notifications
 * of subsys events.
 *
 * Use subsys_notif_register_notifier to register for notifications
 * and subsys_notif_queue_notification to send notifications.
 *
 */

#include <linux/notifier.h>
#include <linux/init.h>
#include <linux/debugfs.h>
#include <linux/module.h>
#include <linux/workqueue.h>
#include <linux/stringify.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <soc/qcom/subsystem_notif.h>

/**
 * The callbacks that are registered in this data structure as early
 * notification callbacks will be called as soon as the SSR framework is
 * informed that the subsystem has crashed. This means that these functions will
 * be invoked as part of an IRQ handler, and thus, will be called in an atomic
 * context. Therefore, functions that are registered as early notification
 * callback must obey to the same constraints as interrupt handlers
 * (i.e. these functions must not sleep or block, etc).
 */
struct subsys_early_notif_info {
	spinlock_t cb_lock;
	void (*early_notif_cb[NUM_EARLY_NOTIFS])(void *);
	void *data[NUM_EARLY_NOTIFS];
};

struct subsys_notif_info {
	char name[50];
	struct srcu_notifier_head subsys_notif_rcvr_list;
	struct subsys_early_notif_info early_notif_info;
	struct list_head list;
};

static LIST_HEAD(subsystem_list);
static DEFINE_MUTEX(notif_lock);
static DEFINE_MUTEX(notif_add_lock);

#if defined(SUBSYS_RESTART_DEBUG)
static void subsys_notif_reg_test_notifier(const char *);
#endif

static struct subsys_notif_info *_notif_find_subsys(const char *subsys_name)
{
	struct subsys_notif_info *subsys;

	mutex_lock(&notif_lock);
	list_for_each_entry(subsys, &subsystem_list, list)
		if (!strcmp(subsys->name, subsys_name)) {
			mutex_unlock(&notif_lock);
			return subsys;
		}
	mutex_unlock(&notif_lock);

	return NULL;
}

void *subsys_notif_register_notifier(
			const char *subsys_name, struct notifier_block *nb)
{
	int ret;
	struct subsys_notif_info *subsys = _notif_find_subsys(subsys_name);

	if (!subsys) {

		/* Possible first time reference to this subsystem. Add it. */
		subsys = (struct subsys_notif_info *)
				subsys_notif_add_subsys(subsys_name);

		if (!subsys)
			return ERR_PTR(-EINVAL);
	}

	ret = srcu_notifier_chain_register(
		&subsys->subsys_notif_rcvr_list, nb);

	if (ret < 0)
		return ERR_PTR(ret);

	return subsys;
}
EXPORT_SYMBOL(subsys_notif_register_notifier);

int subsys_notif_unregister_notifier(void *subsys_handle,
				struct notifier_block *nb)
{
	int ret;
	struct subsys_notif_info *subsys =
			(struct subsys_notif_info *)subsys_handle;

	if (!subsys)
		return -EINVAL;

	ret = srcu_notifier_chain_unregister(
		&subsys->subsys_notif_rcvr_list, nb);

	return ret;
}
EXPORT_SYMBOL(subsys_notif_unregister_notifier);

void subsys_send_early_notifications(void *early_notif_handle)
{
	struct subsys_early_notif_info *early_info = early_notif_handle;
	unsigned long flags;
	unsigned int i;
	void (*notif_cb)(void *data);

	if (!early_notif_handle)
		return;

	spin_lock_irqsave(&early_info->cb_lock, flags);
	for (i = 0; i < NUM_EARLY_NOTIFS; i++) {
		notif_cb = early_info->early_notif_cb[i];
		if (notif_cb)
			notif_cb(early_info->data[i]);
	}
	spin_unlock_irqrestore(&early_info->cb_lock, flags);
}
EXPORT_SYMBOL(subsys_send_early_notifications);

static bool valid_early_notif(enum early_subsys_notif_type notif_type)
{
	return  notif_type >= 0 && notif_type < NUM_EARLY_NOTIFS;
}

/**
 * The early_notif_cb parameter must point to a function that conforms to the
 * same constraints placed upon interrupt handlers, as the function will be
 * called in an atomic context (i.e. these functions must not sleep or block).
 */
int subsys_register_early_notifier(const char *subsys_name,
				   enum early_subsys_notif_type notif_type,
				   void (*early_notif_cb)(void *), void *data)
{
	struct subsys_notif_info *subsys;
	struct subsys_early_notif_info *early_notif_info;
	unsigned long flags;
	int rc = 0;

	if (!subsys_name || !early_notif_cb || !valid_early_notif(notif_type))
		return -EINVAL;

	subsys = _notif_find_subsys(subsys_name);
	if (!subsys)
		return -EINVAL;

	early_notif_info = &subsys->early_notif_info;
	spin_lock_irqsave(&early_notif_info->cb_lock, flags);
	if (early_notif_info->early_notif_cb[notif_type]) {
		rc = -EEXIST;
		goto out;
	}
	early_notif_info->early_notif_cb[notif_type] = early_notif_cb;
	early_notif_info->data[notif_type] = data;
out:
	spin_unlock_irqrestore(&early_notif_info->cb_lock, flags);
	return rc;
}
EXPORT_SYMBOL(subsys_register_early_notifier);

int subsys_unregister_early_notifier(const char *subsys_name, enum
				     early_subsys_notif_type notif_type)
{
	struct subsys_notif_info *subsys;
	struct subsys_early_notif_info *early_notif_info;
	unsigned long flags;

	if (!subsys_name || !valid_early_notif(notif_type))
		return -EINVAL;

	subsys = _notif_find_subsys(subsys_name);
	if (!subsys)
		return -EINVAL;

	early_notif_info = &subsys->early_notif_info;
	spin_lock_irqsave(&early_notif_info->cb_lock, flags);
	early_notif_info->early_notif_cb[notif_type] = NULL;
	early_notif_info->data[notif_type] = NULL;
	spin_unlock_irqrestore(&early_notif_info->cb_lock, flags);
	return 0;
}
EXPORT_SYMBOL(subsys_unregister_early_notifier);

void *subsys_get_early_notif_info(const char *subsys_name)
{
	struct subsys_notif_info *subsys;

	if (!subsys_name)
		return ERR_PTR(-EINVAL);

	subsys = _notif_find_subsys(subsys_name);

	if (!subsys)
		return ERR_PTR(-EINVAL);

	return &subsys->early_notif_info;
}
EXPORT_SYMBOL(subsys_get_early_notif_info);

void *subsys_notif_add_subsys(const char *subsys_name)
{
	struct subsys_notif_info *subsys = NULL;

	if (!subsys_name)
		goto done;

	mutex_lock(&notif_add_lock);

	subsys = _notif_find_subsys(subsys_name);

	if (subsys) {
		mutex_unlock(&notif_add_lock);
		goto done;
	}

	subsys = kmalloc(sizeof(struct subsys_notif_info), GFP_KERNEL);

	if (!subsys) {
		mutex_unlock(&notif_add_lock);
		return ERR_PTR(-EINVAL);
	}

	strlcpy(subsys->name, subsys_name, ARRAY_SIZE(subsys->name));

	srcu_init_notifier_head(&subsys->subsys_notif_rcvr_list);

	memset(&subsys->early_notif_info, 0, sizeof(struct
						    subsys_early_notif_info));
	spin_lock_init(&subsys->early_notif_info.cb_lock);
	INIT_LIST_HEAD(&subsys->list);

	mutex_lock(&notif_lock);
	list_add_tail(&subsys->list, &subsystem_list);
	mutex_unlock(&notif_lock);

	#if defined(SUBSYS_RESTART_DEBUG)
	subsys_notif_reg_test_notifier(subsys->name);
	#endif

	mutex_unlock(&notif_add_lock);

done:
	return subsys;
}
EXPORT_SYMBOL(subsys_notif_add_subsys);

int subsys_notif_queue_notification(void *subsys_handle,
					enum subsys_notif_type notif_type,
					void *data)
{
	struct subsys_notif_info *subsys = subsys_handle;

	if (!subsys)
		return -EINVAL;

	if (notif_type < 0 || notif_type >= SUBSYS_NOTIF_TYPE_COUNT)
		return -EINVAL;

	return srcu_notifier_call_chain(&subsys->subsys_notif_rcvr_list,
				       notif_type, data);
}
EXPORT_SYMBOL(subsys_notif_queue_notification);

#if defined(SUBSYS_RESTART_DEBUG)
static const char *notif_to_string(enum subsys_notif_type notif_type)
{
	switch (notif_type) {

	case	SUBSYS_BEFORE_SHUTDOWN:
		return __stringify(SUBSYS_BEFORE_SHUTDOWN);

	case	SUBSYS_AFTER_SHUTDOWN:
		return __stringify(SUBSYS_AFTER_SHUTDOWN);

	case	SUBSYS_BEFORE_POWERUP:
		return __stringify(SUBSYS_BEFORE_POWERUP);

	case	SUBSYS_AFTER_POWERUP:
		return __stringify(SUBSYS_AFTER_POWERUP);

	default:
		return "unknown";
	}
}

static int subsys_notifier_test_call(struct notifier_block *this,
				  unsigned long code,
				  void *data)
{
	switch (code) {

	default:
		pr_warn("%s: Notification %s from subsystem %pK\n",
			__func__, notif_to_string(code), data);
	break;

	}

	return NOTIFY_DONE;
}

static struct notifier_block nb = {
	.notifier_call = subsys_notifier_test_call,
};

static void subsys_notif_reg_test_notifier(const char *subsys_name)
{
	void *handle = subsys_notif_register_notifier(subsys_name, &nb);

	pr_warn("%s: Registered test notifier, handle=%pK\n",
			__func__, handle);
}
#endif

MODULE_DESCRIPTION("Subsystem Restart Notifier");
MODULE_LICENSE("GPL v2");
