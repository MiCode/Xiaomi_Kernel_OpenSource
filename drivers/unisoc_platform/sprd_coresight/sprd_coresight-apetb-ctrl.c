/*
 * Copyright (C) 2022 Unisoc Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/device.h>
#include <linux/io.h>
#include <linux/err.h>
#include <linux/export.h>
#include <linux/slab.h>
#include <linux/stringhash.h>
#include <linux/mutex.h>
#include <linux/clk.h>
#include <linux/of_platform.h>
#include <linux/delay.h>
#include <linux/pm_runtime.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/sysfs.h>
#include "sprd_coresight-priv.h"
#include "sprd_coresight-apetb-ctrl.h"

static ssize_t enable_apetb_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct apetb_device *csdev = dev_get_drvdata(dev);
	int val;

	val = sprd_tmc_enable_sink_show(csdev->apetb_sink);

	return scnprintf(buf, PAGE_SIZE, "%u\n", csdev->activated);
}

static ssize_t enable_apetb_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
	int ret;
	int i;
	unsigned long val;
	struct apetb_device *csdev = dev_get_drvdata(dev);

	ret = kstrtoul(buf, 10, &val);
	if (ret)
		return ret;

	if (val) {
		csdev->activated = true;
		sprd_tmc_enable_sink_store(csdev->apetb_sink, true);
		for (i = 0; i < csdev->source_num; i++) {
			pr_info("enable source %d", i);
			sprd_etm4_enable_source_store(csdev->apetb_source[i], true);
		}
	} else {
		csdev->activated = false;
		sprd_tmc_enable_sink_store(csdev->apetb_sink, false);
		for (i = 0; i < csdev->source_num; i++)
			sprd_etm4_enable_source_store(csdev->apetb_source[i], false);
	}

	return size;
}

static DEVICE_ATTR_RW(enable_apetb);

static struct attribute *coresight_apetb_attrs[] = {
	&dev_attr_enable_apetb.attr,
	NULL,
};
ATTRIBUTE_GROUPS(coresight_apetb);

static struct device_type coresight_dev_type[] = {
	{
		.name = "none",
	},
	{
		.name = "apetb",
		.groups = coresight_apetb_groups,
	},
	{
		.name = "helper",
	},
};

int apetb_sysfs_init(struct device *dev)
{
	int rc;

	rc = sysfs_create_groups(&dev->kobj, coresight_apetb_groups);
	if (rc)
		dev_err(dev, "create apetb attr node failed, rc=%d\n", rc);

	return rc;
}

static struct class apetb_class = {
	.name = "sprd_apetb",
};

static void apetb_device_release(struct device *dev)
{
	struct apetb_device *dbg = to_apetb_device(dev);

	kfree(dbg);
}

struct apetb_device *apetb_device_register(struct device *parent, struct apetb_ops *ops, const char *apetb_name)
{
	struct apetb_device *dbg;

	dbg = devm_kzalloc(parent, sizeof(struct apetb_device), GFP_KERNEL);
	if (!dbg)
		return NULL;

	dbg->ops = ops;

	dbg->dev.class = &apetb_class;

	dbg->dev.type = &coresight_dev_type[1];

	dbg->dev.parent = parent;
	dbg->dev.release = apetb_device_release;
	dbg->dev.of_node = parent->of_node;
	dev_set_name(&dbg->dev, "%s", apetb_name);

	dev_set_drvdata(&dbg->dev, dbg);

	if (device_register(&dbg->dev)) {
		dev_err(&dbg->dev, "%s failed\n", __func__);
		goto err;
	}

	return dbg;

err:
	kfree(dbg);
	return NULL;
}
EXPORT_SYMBOL_GPL(apetb_device_register);

static int __init apetb_class_init(void)
{
	return class_register(&apetb_class);
}

static void __exit apetb_class_exit(void)
{
	class_unregister(&apetb_class);
}

fs_initcall(apetb_class_init);
module_exit(apetb_class_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Spreadtrum SoC Coresight APETB_CTRL Driver");
