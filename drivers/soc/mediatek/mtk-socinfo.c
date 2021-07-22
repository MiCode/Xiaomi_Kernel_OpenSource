// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2021 MediaTek Inc.
 * Author: Po-Kai Chi <pk.chi@mediatek.com>
 */

#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/printk.h>
#include <linux/slab.h>
#include <linux/sys_soc.h>

static struct soc_device *soc_dev;
static struct soc_device_attribute *attrs;

static int __init mediatek_socinfo_init(void)
{
	struct device_node *np;

	attrs = kzalloc(sizeof(*attrs), GFP_KERNEL);
	if (!attrs)
		return -ENODEV;

	attrs->family = "MediaTek";

	np = of_find_node_by_path("/");
	of_property_read_string(np, "model", &attrs->machine);
	of_property_read_string(np, "soc_id", &attrs->soc_id);
	of_node_put(np);

	soc_dev = soc_device_register(attrs);

	if (IS_ERR(soc_dev)) {
		kfree(attrs);
		return PTR_ERR(soc_dev);
	}

	pr_info("%s SoC detected.\n", attrs->soc_id);

	return 0;
}
module_init(mediatek_socinfo_init);

static void __exit mediatek_socinfo_exit(void)
{
	if (soc_dev)
		soc_device_unregister(soc_dev);
	kfree(attrs);
}
module_exit(mediatek_socinfo_exit);

MODULE_DESCRIPTION("MediaTek SoC Info Sysfs Interface");
MODULE_LICENSE("GPL v2");
