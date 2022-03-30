// SPDX-License-Identifier: GPL-2.0
// Copyright (c) 2021 MediaTek Inc.

#include <linux/platform_device.h>
#include <linux/of_device.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/list.h>
#include <linux/device.h>

#include "imgsensor-glue.h"

static LIST_HEAD(external_list);

int add_external_subdrv_entry(struct subdrv_entry *target)
{
	struct external_entry *entry = NULL;

	if (!target)
		return -EINVAL;

	entry = kzalloc(sizeof(struct external_entry), GFP_KERNEL);

	if (!entry)
		return -ENOMEM;

	entry->target = target;

	INIT_LIST_HEAD(&entry->list);

	list_add(&entry->list, &external_list);

	return 0;
};
EXPORT_SYMBOL(add_external_subdrv_entry);

struct external_entry *query_external_subdrv_entry(const char *name)
{
	struct external_entry *entry = NULL;

	pr_info("[%s] searching %s", __func__, name);

	if (list_empty(&external_list))
		return NULL;

	list_for_each_entry(entry, &external_list, list) {
		if (!strcmp(entry->target->name, name)) {
			pr_info("[%s] %s found", __func__, name);
			return entry;
		}
	}

	return NULL;
}
EXPORT_SYMBOL(query_external_subdrv_entry);


static int __init external_sensor_drv_init(void)
{
	pr_info("%s", __func__);

	return 0;
}

static void __exit external_sensor_drv_exit(void)
{
	struct external_entry *entry = NULL, *n = NULL;

	list_for_each_entry_safe(entry, n, &external_list, list) {
		list_del(&entry->list);
		kfree(entry);
	}

	pr_info("%s", __func__);
}

late_initcall(external_sensor_drv_init);
module_exit(external_sensor_drv_exit);

MODULE_LICENSE("GPL v2");
