/*
 * Copyright (c) 2012-2013, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/err.h>
#include <linux/of.h>

#include "dbm.h"

struct dbm_manager {
	struct list_head	dbm_list;
};

static struct dbm_manager dbm_manager = {
	.dbm_list = LIST_HEAD_INIT(dbm_manager.dbm_list)
};


static struct dbm *of_usb_find_dbm(struct device_node *node)
{
	struct dbm  *dbm;

	list_for_each_entry(dbm, &dbm_manager.dbm_list, head) {
		if (node != dbm->dev->of_node)
			continue;
		return dbm;
	}
	return ERR_PTR(-ENODEV);
}

struct dbm *usb_get_dbm_by_phandle(struct device *dev,
	const char *phandle, u8 index)
{
	struct device_node *node;

	if (!dev->of_node) {
		dev_dbg(dev, "device does not have a device node entry\n");
		return ERR_PTR(-EINVAL);
	}

	node = of_parse_phandle(dev->of_node, phandle, index);
	if (!node) {
		dev_dbg(dev, "failed to get %s phandle in %s node\n", phandle,
			dev->of_node->full_name);
		return ERR_PTR(-ENODEV);
	}

	return of_usb_find_dbm(node);
}
EXPORT_SYMBOL(usb_get_dbm_by_phandle);


int usb_add_dbm(struct dbm *x)
{
	list_add_tail(&x->head, &dbm_manager.dbm_list);
	return 0;
}
EXPORT_SYMBOL(usb_add_dbm);


