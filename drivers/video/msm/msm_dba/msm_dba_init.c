/*
 * Copyright (c) 2015, The Linux Foundation. All rights reserved.
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

#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/err.h>

#include <video/msm_dba.h>
#include "msm_dba_internal.h"

struct msm_dba_device_list {
	struct msm_dba_device_info *dev;
	struct list_head list;
};

static LIST_HEAD(device_list);
static DEFINE_MUTEX(init_mutex);

int msm_dba_add_probed_device(struct msm_dba_device_info *dev)
{
	struct msm_dba_device_list *node;
	if (!dev) {
		pr_err("%s: Invalid params\n", __func__);
		return -EINVAL;
	}

	mutex_lock(&init_mutex);

	node = kzalloc(sizeof(*node), GFP_KERNEL);
	if (!node) {
		pr_err("%s: Not enough memory\n", __func__);
		mutex_unlock(&init_mutex);
		return -ENOMEM;
	}

	memset(node, 0x0, sizeof(*node));
	node->dev = dev;
	list_add(&node->list, &device_list);

	pr_debug("%s: Added new device (%s, %d)", __func__, dev->chip_name,
						dev->instance_id);

	mutex_unlock(&init_mutex);

	return 0;
}

int msm_dba_get_probed_device(struct msm_dba_reg_info *reg,
			      struct msm_dba_device_info **dev)
{
	int rc = 0;
	struct msm_dba_device_list *node;
	struct list_head *position = NULL;

	if (!reg || !dev) {
		pr_err("%s: Invalid params\n", __func__);
		return -EINVAL;
	}

	mutex_lock(&init_mutex);

	*dev = NULL;
	list_for_each(position, &device_list) {
		node = list_entry(position, struct msm_dba_device_list, list);
		if (!strcmp(reg->chip_name, node->dev->chip_name) &&
		    reg->instance_id == node->dev->instance_id) {
			pr_debug("%s: Found device (%s, %d)\n", __func__,
							reg->chip_name,
							reg->instance_id);
			*dev = node->dev;
			break;
		}
	}

	if (!*dev) {
		pr_err("%s: Device not found (%s, %d)\n", __func__,
							reg->chip_name,
							reg->instance_id);
		rc = -ENODEV;
	}

	mutex_unlock(&init_mutex);

	return rc;
}

int msm_dba_remove_probed_device(struct msm_dba_device_info *dev)
{
	struct msm_dba_device_list *node;
	struct list_head *position = NULL;
	struct list_head *temp = NULL;

	if (!dev) {
		pr_err("%s: Invalid params\n", __func__);
		return -EINVAL;
	}

	mutex_lock(&init_mutex);

	list_for_each_safe(position, temp, &device_list) {
		node = list_entry(position, struct msm_dba_device_list, list);
		if (node->dev == dev) {
			list_del(&node->list);
			pr_debug("%s: Removed device (%s, %d)\n", __func__,
							    dev->chip_name,
							    dev->instance_id);
			kfree(node);
			break;
		}
	}

	mutex_unlock(&init_mutex);

	return 0;
}
