/* arch/arm/mach-msm/vreg.c
 *
 * Copyright (C) 2008 Google, Inc.
 * Copyright (c) 2009-2012 Code Aurora Forum. All rights reserved.
 * Author: Brian Swetland <swetland@google.com>
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

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/list.h>
#include <linux/mutex.h>
#include <linux/atomic.h>
#include <linux/debugfs.h>
#include <linux/regulator/consumer.h>
#include <linux/string.h>
#include <mach/vreg.h>
#include <mach/proc_comm.h>

#if defined(CONFIG_MSM_VREG_SWITCH_INVERTED)
#define VREG_SWITCH_ENABLE 0
#define VREG_SWITCH_DISABLE 1
#else
#define VREG_SWITCH_ENABLE 1
#define VREG_SWITCH_DISABLE 0
#endif

struct vreg {
	struct list_head	list;
	struct mutex		lock;
	const char		*name;
	u64			refcnt;
	unsigned		mv;
	struct regulator	*reg;
};

static LIST_HEAD(vreg_list);
static DEFINE_MUTEX(vreg_lock);

#ifdef CONFIG_DEBUG_FS
static void vreg_add_debugfs(struct vreg *vreg);
#else
static inline void vreg_add_debugfs(struct vreg *vreg) { }
#endif

static struct vreg *vreg_create(const char *id)
{
	int rc;
	struct vreg *vreg;

	vreg = kzalloc(sizeof(*vreg), GFP_KERNEL);
	if (!vreg) {
		rc = -ENOMEM;
		goto error;
	}

	INIT_LIST_HEAD(&vreg->list);
	mutex_init(&vreg->lock);

	vreg->reg = regulator_get(NULL, id);
	if (IS_ERR(vreg->reg)) {
		rc = PTR_ERR(vreg->reg);
		goto free_vreg;
	}

	vreg->name = kstrdup(id, GFP_KERNEL);
	if (!vreg->name) {
		rc = -ENOMEM;
		goto put_reg;
	}

	list_add_tail(&vreg->list, &vreg_list);
	vreg_add_debugfs(vreg);

	return vreg;

put_reg:
	regulator_put(vreg->reg);
free_vreg:
	kfree(vreg);
error:
	return ERR_PTR(rc);
}

static void vreg_destroy(struct vreg *vreg)
{
	if (!vreg)
		return;

	if (vreg->refcnt)
		regulator_disable(vreg->reg);

	kfree(vreg->name);
	regulator_put(vreg->reg);
	kfree(vreg);
}

struct vreg *vreg_get(struct device *dev, const char *id)
{
	struct vreg *vreg = NULL;

	if (!id)
		return ERR_PTR(-EINVAL);

	mutex_lock(&vreg_lock);
	list_for_each_entry(vreg, &vreg_list, list) {
		if (!strncmp(vreg->name, id, 10))
			goto ret;
	}

	vreg = vreg_create(id);

ret:
	mutex_unlock(&vreg_lock);
	return vreg;
}
EXPORT_SYMBOL(vreg_get);

void vreg_put(struct vreg *vreg)
{
	kfree(vreg->name);
	regulator_put(vreg->reg);
	list_del(&vreg->list);
	kfree(vreg);
}

int vreg_enable(struct vreg *vreg)
{
	int rc = 0;
	if (!vreg)
		return -ENODEV;

	mutex_lock(&vreg->lock);
	if (vreg->refcnt == 0) {
		rc = regulator_enable(vreg->reg);
		if (!rc)
			vreg->refcnt++;
	} else {
		rc = 0;
		if (vreg->refcnt < UINT_MAX)
			vreg->refcnt++;
	}
	mutex_unlock(&vreg->lock);

	return rc;
}
EXPORT_SYMBOL(vreg_enable);

int vreg_disable(struct vreg *vreg)
{
	int rc = 0;
	if (!vreg)
		return -ENODEV;

	mutex_lock(&vreg->lock);
	if (vreg->refcnt == 0) {
		pr_warn("%s: unbalanced disables for vreg %s\n",
				__func__, vreg->name);
		rc = -EINVAL;
	} else if (vreg->refcnt == 1) {
		rc = regulator_disable(vreg->reg);
		if (!rc)
			vreg->refcnt--;
	} else {
		rc = 0;
		vreg->refcnt--;
	}
	mutex_unlock(&vreg->lock);

	return rc;
}
EXPORT_SYMBOL(vreg_disable);

int vreg_set_level(struct vreg *vreg, unsigned mv)
{
	unsigned uv;
	int rc;

	if (!vreg)
		return -EINVAL;

	if (mv > (UINT_MAX / 1000))
		return -ERANGE;

	uv = mv * 1000;

	mutex_lock(&vreg->lock);
	rc = regulator_set_voltage(vreg->reg, uv, uv);
	if (!rc)
		vreg->mv = mv;
	mutex_unlock(&vreg->lock);

	return rc;
}
EXPORT_SYMBOL(vreg_set_level);

#if defined(CONFIG_DEBUG_FS)

static int vreg_debug_enabled_set(void *data, u64 val)
{
	struct vreg *vreg = data;

	if (val == 0)
		return vreg_disable(vreg);
	else if (val == 1)
		return vreg_enable(vreg);
	else
		return -EINVAL;
}

static int vreg_debug_enabled_get(void *data, u64 *val)
{
	struct vreg *vreg = data;

	*val = vreg->refcnt;

	return 0;
}

static int vreg_debug_voltage_set(void *data, u64 val)
{
	struct vreg *vreg = data;
	return vreg_set_level(vreg, val);
}

static int vreg_debug_voltage_get(void *data, u64 *val)
{
	struct vreg *vreg = data;
	*val = vreg->mv;
	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(vreg_debug_enabled, vreg_debug_enabled_get,
		vreg_debug_enabled_set, "%llu");
DEFINE_SIMPLE_ATTRIBUTE(vreg_debug_voltage, vreg_debug_voltage_get,
		vreg_debug_voltage_set, "%llu");

static struct dentry *root;

static void vreg_add_debugfs(struct vreg *vreg)
{
	struct dentry *dir;

	if (!root)
		return;

	dir = debugfs_create_dir(vreg->name, root);

	if (IS_ERR_OR_NULL(dir))
		goto err;

	if (IS_ERR_OR_NULL(debugfs_create_file("enabled", 0644,	dir, vreg,
					&vreg_debug_enabled)))
		goto destroy;

	if (IS_ERR_OR_NULL(debugfs_create_file("voltage", 0644, dir, vreg,
					&vreg_debug_voltage)))
		goto destroy;

	return;

destroy:
	debugfs_remove_recursive(dir);
err:
	pr_warn("%s: could not create debugfs for vreg %s\n",
			__func__, vreg->name);
}

static int __devinit vreg_debug_init(void)
{
	root = debugfs_create_dir("vreg", NULL);

	if (IS_ERR_OR_NULL(root)) {
		pr_debug("%s: error initializing debugfs: %ld - "
				"disabling debugfs\n",
				__func__, root ? PTR_ERR(root) : 0);
		root = NULL;
	}

	return 0;
}
static void __devexit vreg_debug_exit(void)
{
	if (root)
		debugfs_remove_recursive(root);
	root = NULL;
}
#else
static inline int __init vreg_debug_init(void) { return 0; }
static inline void __exit vreg_debug_exit(void) { return 0; }
#endif

static int __init vreg_init(void)
{
	return vreg_debug_init();
}
module_init(vreg_init);

static void __exit vreg_exit(void)
{
	struct vreg *vreg, *next;
	vreg_debug_exit();

	mutex_lock(&vreg_lock);
	list_for_each_entry_safe(vreg, next, &vreg_list, list)
		vreg_destroy(vreg);
	mutex_unlock(&vreg_lock);
}
module_exit(vreg_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("vreg.c regulator shim");
MODULE_VERSION("1.0");
