/* Copyright (c) 2012, Code Aurora Forum. All rights reserved.
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

/* OF helpers for SLIMbus */
#include <linux/slimbus/slimbus.h>
#include <linux/irq.h>
#include <linux/slab.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_slimbus.h>

int of_register_slim_devices(struct slim_controller *ctrl)
{
	struct device_node *node;
	struct slim_boardinfo *binfo = NULL;
	int n = 0;
	int ret = 0;

	if (!ctrl->dev.of_node)
		return -EINVAL;

	for_each_child_of_node(ctrl->dev.of_node, node) {
		struct property *prop;
		struct slim_device *slim;
		char *name;
		prop = of_find_property(node, "elemental-addr", NULL);
		if (!prop || prop->length != 6) {
			dev_err(&ctrl->dev, "of_slim: invalid E-addr");
			continue;
		}
		name = kzalloc(SLIMBUS_NAME_SIZE, GFP_KERNEL);
		if (!name) {
			dev_err(&ctrl->dev, "of_slim: out of memory");
			ret = -ENOMEM;
			goto of_slim_err;
		}
		if (of_modalias_node(node, name, SLIMBUS_NAME_SIZE) < 0) {
			dev_err(&ctrl->dev, "of_slim: modalias failure on %s\n",
				node->full_name);
			kfree(name);
			continue;
		}
		slim = kzalloc(sizeof(struct slim_device), GFP_KERNEL);
		if (!slim) {
			dev_err(&ctrl->dev, "of_slim: out of memory");
			ret = -ENOMEM;
			kfree(name);
			goto of_slim_err;
		}
		memcpy(slim->e_addr, prop->value, 6);

		binfo = krealloc(binfo, (n + 1) * sizeof(struct slim_boardinfo),
					GFP_KERNEL);
		if (!binfo) {
			dev_err(&ctrl->dev, "out of memory");
			kfree(name);
			kfree(slim);
			return -ENOMEM;
		}
		slim->name = (const char *)name;
		binfo[n].bus_num = ctrl->nr;
		binfo[n].slim_slave = slim;
		n++;
	}
	return slim_register_board_info(binfo, n);
of_slim_err:
	n--;
	while (n >= 0) {
		kfree(binfo[n].slim_slave->name);
		kfree(binfo[n].slim_slave);
	}
	kfree(binfo);
	return ret;
}
