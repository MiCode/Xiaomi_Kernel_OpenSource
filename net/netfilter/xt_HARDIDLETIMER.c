/*
 * linux/net/netfilter/xt_HARDIDLETIMER.c
 *
 * Netfilter module to trigger a timer when packet matches.
 * After timer expires a kevent will be sent.
 *
 * Copyright (c) 2014-2015, 2017 The Linux Foundation. All rights reserved.
 *
 * Copyright (C) 2004, 2010 Nokia Corporation
 *
 * Written by Timo Teras <ext-timo.teras@nokia.com>
 *
 * Converted to x_tables and reworked for upstream inclusion
 * by Luciano Coelho <luciano.coelho@nokia.com>
 *
 * Contact: Luciano Coelho <luciano.coelho@nokia.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/module.h>
#include <linux/timer.h>
#include <linux/alarmtimer.h>
#include <linux/list.h>
#include <linux/mutex.h>
#include <linux/netfilter.h>
#include <linux/netfilter/x_tables.h>
#include <linux/netfilter/xt_HARDIDLETIMER.h>
#include <linux/kdev_t.h>
#include <linux/kobject.h>
#include <linux/skbuff.h>
#include <linux/workqueue.h>
#include <linux/sysfs.h>
#include <net/net_namespace.h>

struct hardidletimer_tg_attr {
	struct attribute attr;
	ssize_t	(*show)(struct kobject *kobj,
			struct attribute *attr, char *buf);
};

struct hardidletimer_tg {
	struct list_head entry;
	struct alarm alarm;
	struct work_struct work;

	struct kobject *kobj;
	struct hardidletimer_tg_attr attr;

	unsigned int refcnt;
	bool send_nl_msg;
	bool active;
};

static LIST_HEAD(hardidletimer_tg_list);
static DEFINE_MUTEX(list_mutex);

static struct kobject *hardidletimer_tg_kobj;

static void notify_netlink_uevent(const char *iface,
				  struct hardidletimer_tg *timer)
{
	char iface_msg[NLMSG_MAX_SIZE];
	char state_msg[NLMSG_MAX_SIZE];
	char *envp[] = { iface_msg, state_msg, NULL };
	int res;

	res = snprintf(iface_msg, NLMSG_MAX_SIZE, "INTERFACE=%s",
		       iface);
	if (res >= NLMSG_MAX_SIZE) {
		pr_err("message too long (%d)", res);
		return;
	}
	res = snprintf(state_msg, NLMSG_MAX_SIZE, "STATE=%s",
		       timer->active ? "active" : "inactive");
	if (res >= NLMSG_MAX_SIZE) {
		pr_err("message too long (%d)", res);
		return;
	}
	pr_debug("putting nlmsg: <%s> <%s>\n", iface_msg, state_msg);
	kobject_uevent_env(hardidletimer_tg_kobj, KOBJ_CHANGE, envp);
}

static
struct hardidletimer_tg *__hardidletimer_tg_find_by_label(const char *label)
{
	struct hardidletimer_tg *entry;

	WARN_ON(!label);

	list_for_each_entry(entry, &hardidletimer_tg_list, entry) {
		if (!strcmp(label, entry->attr.attr.name))
			return entry;
	}

	return NULL;
}

static ssize_t hardidletimer_tg_show(struct kobject *kobj,
				     struct attribute *attr, char *buf)
{
	struct hardidletimer_tg *timer;
	ktime_t expires;
	struct timespec ktimespec;

	memset(&ktimespec, 0, sizeof(struct timespec));
	mutex_lock(&list_mutex);

	timer =	__hardidletimer_tg_find_by_label(attr->name);
	if (timer) {
		expires = alarm_expires_remaining(&timer->alarm);
		ktimespec = ktime_to_timespec(expires);
	}

	mutex_unlock(&list_mutex);

	if (ktimespec.tv_sec >= 0)
		return snprintf(buf, PAGE_SIZE, "%ld\n", ktimespec.tv_sec);

	if ((timer) && (timer->send_nl_msg))
		return snprintf(buf, PAGE_SIZE, "0 %ld\n", ktimespec.tv_sec);
	else
		return snprintf(buf, PAGE_SIZE, "0\n");
}

static void hardidletimer_tg_work(struct work_struct *work)
{
	struct hardidletimer_tg *timer = container_of(work,
				struct hardidletimer_tg, work);

	sysfs_notify(hardidletimer_tg_kobj, NULL, timer->attr.attr.name);

	if (timer->send_nl_msg)
		notify_netlink_uevent(timer->attr.attr.name, timer);
}

static enum alarmtimer_restart hardidletimer_tg_alarmproc(struct alarm *alarm,
							  ktime_t now)
{
	struct hardidletimer_tg *timer = alarm->data;

	pr_debug("alarm %s expired\n", timer->attr.attr.name);

	timer->active = false;
	schedule_work(&timer->work);
	return ALARMTIMER_NORESTART;
}

static int hardidletimer_tg_create(struct hardidletimer_tg_info *info)
{
	int ret;
	ktime_t tout;

	info->timer = kmalloc(sizeof(*info->timer), GFP_KERNEL);
	if (!info->timer) {
		ret = -ENOMEM;
		goto out;
	}

	info->timer->attr.attr.name = kstrdup(info->label, GFP_KERNEL);
	if (!info->timer->attr.attr.name) {
		ret = -ENOMEM;
		goto out_free_timer;
	}
	info->timer->attr.attr.mode = 0444;
	info->timer->attr.show = hardidletimer_tg_show;

	ret = sysfs_create_file(hardidletimer_tg_kobj, &info->timer->attr.attr);
	if (ret < 0) {
		pr_debug("couldn't add file to sysfs");
		goto out_free_attr;
	}
	/*  notify userspace  */
	kobject_uevent(hardidletimer_tg_kobj, KOBJ_ADD);

	list_add(&info->timer->entry, &hardidletimer_tg_list);

	alarm_init(&info->timer->alarm, ALARM_BOOTTIME,
		   hardidletimer_tg_alarmproc);
	info->timer->alarm.data = info->timer;
	info->timer->refcnt = 1;
	info->timer->send_nl_msg = (info->send_nl_msg == 0) ? false : true;
	info->timer->active = true;
	tout = ktime_set(info->timeout, 0);
	alarm_start_relative(&info->timer->alarm, tout);

	INIT_WORK(&info->timer->work, hardidletimer_tg_work);

	return 0;

out_free_attr:
	kfree(info->timer->attr.attr.name);
out_free_timer:
	kfree(info->timer);
out:
	return ret;
}

/* The actual xt_tables plugin. */
static unsigned int hardidletimer_tg_target(struct sk_buff *skb,
					    const struct xt_action_param *par)
{
	const struct hardidletimer_tg_info *info = par->targinfo;
	ktime_t tout;

	pr_debug("resetting timer %s, timeout period %u\n",
		 info->label, info->timeout);

	WARN_ON(!info->timer);

	if (!info->timer->active) {
		schedule_work(&info->timer->work);
		pr_debug("Starting timer %s\n", info->label);
	}

	info->timer->active = true;
	/* TODO: Avoid modifying timers on each packet */
	tout = ktime_set(info->timeout, 0);
	alarm_start_relative(&info->timer->alarm, tout);

	return XT_CONTINUE;
}

static int hardidletimer_tg_checkentry(const struct xt_tgchk_param *par)
{
	struct hardidletimer_tg_info *info = par->targinfo;
	int ret;
	ktime_t tout;
	struct timespec ktimespec;

	memset(&ktimespec, 0, sizeof(struct timespec));

	pr_debug("checkentry targinfo %s\n", info->label);

	if (info->timeout == 0) {
		pr_debug("timeout value is zero\n");
		return -EINVAL;
	}

	if (info->label[0] == '\0' ||
	    strnlen(info->label, MAX_HARDIDLETIMER_LABEL_SIZE)
				== MAX_HARDIDLETIMER_LABEL_SIZE) {
		pr_debug("label is empty or not nul-terminated\n");
		return -EINVAL;
	}

	mutex_lock(&list_mutex);

	info->timer = __hardidletimer_tg_find_by_label(info->label);
	if (info->timer) {
		info->timer->refcnt++;
		/* calculate remaining expiry time */
		tout = alarm_expires_remaining(&info->timer->alarm);
		ktimespec = ktime_to_timespec(tout);

		if (ktimespec.tv_sec > 0) {
			pr_debug("time_expiry_remaining %ld\n",
				 ktimespec.tv_sec);
			alarm_start_relative(&info->timer->alarm, tout);
		}

		pr_debug("increased refcnt of timer %s to %u\n",
			 info->label, info->timer->refcnt);
	} else {
		ret = hardidletimer_tg_create(info);
		if (ret < 0) {
			pr_debug("failed to create timer\n");
			mutex_unlock(&list_mutex);
			return ret;
		}
	}

	mutex_unlock(&list_mutex);

	return 0;
}

static void hardidletimer_tg_destroy(const struct xt_tgdtor_param *par)
{
	const struct hardidletimer_tg_info *info = par->targinfo;

	pr_debug("destroy targinfo %s\n", info->label);

	mutex_lock(&list_mutex);

	if (--info->timer->refcnt == 0) {
		pr_debug("deleting timer %s\n", info->label);

		list_del(&info->timer->entry);
		alarm_cancel(&info->timer->alarm);
		cancel_work_sync(&info->timer->work);
		sysfs_remove_file(hardidletimer_tg_kobj,
				  &info->timer->attr.attr);
		kfree(info->timer->attr.attr.name);
		kfree(info->timer);
	} else {
		pr_debug("decreased refcnt of timer %s to %u\n",
			 info->label, info->timer->refcnt);
	}

	mutex_unlock(&list_mutex);
}

static struct xt_target hardidletimer_tg __read_mostly = {
	.name		= "HARDIDLETIMER",
	.revision	= 1,
	.family		= NFPROTO_UNSPEC,
	.target		= hardidletimer_tg_target,
	.targetsize     = sizeof(struct hardidletimer_tg_info),
	.checkentry	= hardidletimer_tg_checkentry,
	.destroy        = hardidletimer_tg_destroy,
	.me		= THIS_MODULE,
};

static struct class *hardidletimer_tg_class;

static struct device *hardidletimer_tg_device;

static int __init hardidletimer_tg_init(void)
{
	int err;

	hardidletimer_tg_class = class_create(THIS_MODULE, "xt_hardidletimer");
	err = PTR_ERR(hardidletimer_tg_class);
	if (IS_ERR(hardidletimer_tg_class)) {
		pr_debug("couldn't register device class\n");
		goto out;
	}

	hardidletimer_tg_device = device_create(hardidletimer_tg_class, NULL,
						MKDEV(0, 0), NULL, "timers");
	err = PTR_ERR(hardidletimer_tg_device);
	if (IS_ERR(hardidletimer_tg_device)) {
		pr_debug("couldn't register system device\n");
		goto out_class;
	}

	hardidletimer_tg_kobj = &hardidletimer_tg_device->kobj;

	err = xt_register_target(&hardidletimer_tg);
	if (err < 0) {
		pr_debug("couldn't register xt target\n");
		goto out_dev;
	}

	return 0;
out_dev:
	device_destroy(hardidletimer_tg_class, MKDEV(0, 0));
out_class:
	class_destroy(hardidletimer_tg_class);
out:
	return err;
}

static void __exit hardidletimer_tg_exit(void)
{
	xt_unregister_target(&hardidletimer_tg);

	device_destroy(hardidletimer_tg_class, MKDEV(0, 0));
	class_destroy(hardidletimer_tg_class);
}

module_init(hardidletimer_tg_init);
module_exit(hardidletimer_tg_exit);

MODULE_AUTHOR("Timo Teras <ext-timo.teras@nokia.com>");
MODULE_AUTHOR("Luciano Coelho <luciano.coelho@nokia.com>");
MODULE_DESCRIPTION("Xtables: idle time monitor");
MODULE_LICENSE("GPL v2");

