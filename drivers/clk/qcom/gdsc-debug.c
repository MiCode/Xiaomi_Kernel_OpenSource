// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2024, Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include <linux/debugfs.h>
#include <linux/pm_runtime.h>
#include <linux/regmap.h>
#include <linux/regulator/consumer.h>

#include "gdsc.h"
#include "gdsc-debug.h"

#define HW_CONTROL_MASK		BIT(1)

static int gdsc_genpd_hwctrl_get(void *data, u64 *val)
{
	struct gdsc_debug *gdsc_debug = data;
	struct gdsc *sc = gdsc_debug->sc;
	u32 regval;
	int ret;

	if (sc->rsupply) {
		ret = regulator_enable(sc->rsupply);
		if (ret)
			return ret;
	}

	regmap_read(sc->regmap, sc->gdscr, &regval);
	*val = !!(regval & HW_CONTROL_MASK);

	if (sc->rsupply)
		return regulator_disable(sc->rsupply);

	return 0;
}

static int gdsc_genpd_hwctrl_set(void *data, u64 val)
{
	struct gdsc_debug *gdsc_debug = data;
	struct gdsc *sc = gdsc_debug->sc;
	u32 mask = val ? HW_CONTROL_MASK : 0;
	int ret;

	if (sc->rsupply) {
		ret = regulator_enable(sc->rsupply);
		if (ret)
			return ret;
	}

	regmap_update_bits(sc->regmap, sc->gdscr, HW_CONTROL_MASK, mask);

	if (sc->rsupply)
		return regulator_disable(sc->rsupply);

	return 0;
}
DEFINE_DEBUGFS_ATTRIBUTE(gdsc_genpd_debug_hwctrl_fops, gdsc_genpd_hwctrl_get,
					gdsc_genpd_hwctrl_set, "%llu\n");

static int gdsc_genpd_enable_get(void *data, u64 *val)
{
	struct gdsc_debug *gdsc_debug = data;

	if (!gdsc_debug->dev)
		*val = -ENODEV;
	else
		*val = pm_runtime_active(gdsc_debug->dev);

	return 0;
}

static void gdsc_debug_dev_release(struct device *dev)
{
	kfree(dev);
}

static int gdsc_genpd_enable_set(void *data, u64 val)
{
	struct gdsc_debug *gdsc_debug = data;
	struct gdsc *sc = gdsc_debug->sc;
	int ret;

	if (!gdsc_debug->dev) {
		/*
		 * Create debug consumer device and add it to power domain only
		 * when there is a first request to enable GDSC from debugfs
		 */
		gdsc_debug->dev = kzalloc(sizeof(*gdsc_debug->dev), GFP_KERNEL);
		if (!gdsc_debug->dev)
			return -ENOMEM;

		dev_set_name(gdsc_debug->dev, "%s:debug", sc->pd.name);
		/*
		 * A release callback must be registered for the device to prevent warnings
		 * about the missing release() function when device_unregister() is called
		 * in the debug unregister path.
		 */
		gdsc_debug->dev->release = gdsc_debug_dev_release;

		ret = device_register(gdsc_debug->dev);
		if (ret) {
			put_device(gdsc_debug->dev);
			kfree(gdsc_debug->dev);
			gdsc_debug->dev = NULL;
			return ret;
		}

		ret = pm_genpd_add_device(&sc->pd, gdsc_debug->dev);
		if (ret) {
			device_unregister(gdsc_debug->dev);
			kfree(gdsc_debug->dev);
			gdsc_debug->dev = NULL;
			return ret;
		}

		pm_runtime_enable(gdsc_debug->dev);
	}

	if (val)
		return pm_runtime_resume_and_get(gdsc_debug->dev);

	/* Return if GDSC is already suspended from debugfs */
	if (pm_runtime_suspended(gdsc_debug->dev))
		return 0;

	return pm_runtime_put_sync(gdsc_debug->dev);
}
DEFINE_DEBUGFS_ATTRIBUTE(gdsc_genpd_debug_enable_fops, gdsc_genpd_enable_get,
					gdsc_genpd_enable_set, "%lld\n");

static int gdsc_genpd_debug_create_one(struct gdsc_debug *gdsc_debug,
						struct dentry *pdentry)
{
	struct gdsc *sc = gdsc_debug->sc;
	struct dentry *root;
	struct dentry *tmp;

	if (!gdsc_debug || !pdentry)
		return -EINVAL;

	root = debugfs_lookup(sc->pd.name, pdentry);
	if (IS_ERR_OR_NULL(root)) {
		pr_err("Unable to find %s debugfs directory\n", sc->pd.name);
		return -ENOENT;
	}

	tmp = debugfs_create_file("enable", 0644, root, gdsc_debug,
				&gdsc_genpd_debug_enable_fops);
	if (IS_ERR_OR_NULL(tmp)) {
		pr_err("Failed to create enable debugfs node for %s\n",
			sc->pd.name);
		return -ENOENT;
	}

	if (sc->flags & HW_CTRL) {
		tmp = debugfs_create_file("hwctrl", 0644, root, gdsc_debug,
				&gdsc_genpd_debug_hwctrl_fops);
		if (IS_ERR_OR_NULL(tmp)) {
			pr_err("Failed to create hwctrl debugfs node for %s\n",
				sc->pd.name);
			return -ENOENT;
		}
	}

	return 0;
}

int gdsc_genpd_debug_register(struct gdsc *sc)
{
	struct gdsc_debug *gdsc_debug;
	int ret = 0;

	gdsc_debug = kzalloc(sizeof(*gdsc_debug), GFP_KERNEL);
	if (!gdsc_debug)
		return -ENOMEM;

	gdsc_debug->sc = sc;

	mutex_lock(&gdsc_genpd_debug_lock);

	list_add(&gdsc_debug->list, &gdsc_genpd_debug_list);
	if (genpd_rootdir)
		ret = gdsc_genpd_debug_create_one(gdsc_debug, genpd_rootdir);

	mutex_unlock(&gdsc_genpd_debug_lock);

	return ret;
}
EXPORT_SYMBOL_GPL(gdsc_genpd_debug_register);

void gdsc_genpd_debug_unregister(struct gdsc *sc)
{
	struct gdsc_debug *gdsc_debug, *tmp;

	mutex_lock(&gdsc_genpd_debug_lock);
	list_for_each_entry_safe(gdsc_debug, tmp, &gdsc_genpd_debug_list, list) {
		if (gdsc_debug->sc == sc) {
			if (gdsc_debug->dev) {
				pm_runtime_disable(gdsc_debug->dev);
				pm_genpd_remove_device(gdsc_debug->dev);
				device_unregister(gdsc_debug->dev);
				gdsc_debug->dev = NULL;
			}

			gdsc_debug->sc = NULL;
			list_del(&gdsc_debug->list);
			kfree(gdsc_debug);
		}
	}
	mutex_unlock(&gdsc_genpd_debug_lock);
}
EXPORT_SYMBOL_GPL(gdsc_genpd_debug_unregister);

/*
 * The debugfs GenPD directories will be created lazily by genpd_debug_init during
 * a late_initcall. Ensure that the GDSC debugfs entries are created only after
 * genpd_debug_init has initialized the debugfs entries from the GenPD framework.
 */
static int __init gdsc_genpd_debug_init(void)
{
	struct gdsc_debug *gdsc_debug;

	mutex_lock(&gdsc_genpd_debug_lock);

	genpd_rootdir = debugfs_lookup("pm_genpd", NULL);
	if (!genpd_rootdir) {
		pr_err("Unable to find pm_genpd debugfs directory\n");
		mutex_unlock(&gdsc_genpd_debug_lock);
		return 0;
	}

	list_for_each_entry(gdsc_debug, &gdsc_genpd_debug_list, list)
		gdsc_genpd_debug_create_one(gdsc_debug, genpd_rootdir);

	mutex_unlock(&gdsc_genpd_debug_lock);

	return 0;
}
late_initcall_sync(gdsc_genpd_debug_init);
