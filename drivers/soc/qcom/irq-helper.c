/* Copyright (c) 2016, The Linux Foundation. All rights reserved.
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

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/of.h>
#include <linux/cpu.h>
#include <linux/sysfs.h>
#include <linux/kobject.h>
#include <linux/spinlock.h>
#include <linux/slab.h>
#include <soc/qcom/irq-helper.h>

struct irq_helper {
	bool enable;
	bool deploy;
	uint32_t count;
	struct kobject kobj;
	/* spinlock to protect reference count variable 'count' */
	spinlock_t lock;
};

struct irq_helper_attr {
	struct attribute        attr;
	ssize_t (*show)(struct kobject *kobj, struct attribute *attr,
			char *buf);
	size_t (*store)(struct kobject *kobj, struct attribute *attr,
			const char *buf, size_t count);
};

#define IRQ_HELPER_ATTR(_name, _mode, _show, _store)    \
	struct irq_helper_attr irq_helper_##_name =  \
		__ATTR(_name, _mode, _show, _store)

#define to_irq_helper(kobj) \
	container_of(kobj, struct irq_helper, kobj)

#define to_irq_helper_attr(_attr) \
	container_of(_attr, struct irq_helper_attr, attr)

static ssize_t attr_show(struct kobject *kobj, struct attribute *attr,
		char *buf)
{
	struct irq_helper_attr *irq_attr = to_irq_helper_attr(attr);
	ssize_t ret = -EIO;

	if (irq_attr->show)
		ret = irq_attr->show(kobj, attr, buf);

	return ret;
}

static const struct sysfs_ops irq_helper_sysfs_ops = {
	.show   = attr_show,
};

static struct kobj_type irq_helper_ktype = {
	.sysfs_ops  = &irq_helper_sysfs_ops,
};

static ssize_t show_deploy(struct kobject *kobj, struct attribute *attr,
		char *buf)
{
	struct irq_helper *irq = to_irq_helper(kobj);

	return snprintf(buf, PAGE_SIZE, "%u\n", irq->deploy);
}
IRQ_HELPER_ATTR(irq_blacklist_on, 0444, show_deploy, NULL);

static struct irq_helper *irq_h;

/* Do not call this API in an atomic context */
int irq_blacklist_on(void)
{
	bool flag = false;

	might_sleep();
	if (!irq_h) {
		pr_err("%s: init function is not called", __func__);
		return -EPERM;
	}
	if (!irq_h->enable) {
		pr_err("%s: enable bit is not set up", __func__);
		return -EPERM;
	}
	spin_lock(&irq_h->lock);
	irq_h->count++;
	if (!irq_h->deploy) {
		irq_h->deploy = true;
		flag = true;
	}
	spin_unlock(&irq_h->lock);
	if (flag)
		sysfs_notify(&irq_h->kobj, NULL, "irq_blacklist_on");
	return 0;
}
EXPORT_SYMBOL(irq_blacklist_on);

/* Do not call this API in an atomic context */
int irq_blacklist_off(void)
{
	bool flag = false;

	might_sleep();
	if (!irq_h) {
		pr_err("%s: init function is not called", __func__);
		return -EPERM;
	}
	if (!irq_h->enable) {
		pr_err("%s: enable bit is not set up", __func__);
		return -EPERM;
	}
	spin_lock(&irq_h->lock);
	if (irq_h->count == 0) {
		pr_err("%s: ref-count is 0, cannot call irq blacklist off.",
				__func__);
		spin_unlock(&irq_h->lock);
		return -EPERM;
	}
	irq_h->count--;
	if (irq_h->count == 0) {
		irq_h->deploy = false;
		flag = true;
	}
	spin_unlock(&irq_h->lock);

	if (flag)
		sysfs_notify(&irq_h->kobj, NULL, "irq_blacklist_on");
	return 0;
}
EXPORT_SYMBOL(irq_blacklist_off);

static int __init irq_helper_init(void)
{
	int ret;

	irq_h = kzalloc(sizeof(struct irq_helper), GFP_KERNEL);
	if (!irq_h)
		return -ENOMEM;

	ret = kobject_init_and_add(&irq_h->kobj, &irq_helper_ktype,
			kernel_kobj,  "%s", "irq_helper");
	if (ret) {
		pr_err("%s:Error in creation kobject_add\n", __func__);
		goto out_free_irq;
	}

	ret = sysfs_create_file(&irq_h->kobj,
			&irq_helper_irq_blacklist_on.attr);
	if (ret) {
		pr_err("%s:Error in sysfs_create_file\n", __func__);
		goto out_put_kobj;
	}

	spin_lock_init(&irq_h->lock);
	irq_h->count = 0;
	irq_h->enable = true;
	return 0;
out_put_kobj:
	kobject_put(&irq_h->kobj);
out_free_irq:
	kfree(irq_h);
	return ret;
}
module_init(irq_helper_init);

static void __exit irq_helper_exit(void)
{
	sysfs_remove_file(&irq_h->kobj, &irq_helper_irq_blacklist_on.attr);
	kobject_del(&irq_h->kobj);
	kobject_put(&irq_h->kobj);
	kfree(irq_h);
}
module_exit(irq_helper_exit);
MODULE_DESCRIPTION("IRQ Helper APIs");
