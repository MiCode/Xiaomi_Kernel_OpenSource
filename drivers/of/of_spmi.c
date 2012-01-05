/* Copyright (c) 2012, The Linux Foundation. All rights reserved.
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

#include <linux/spmi.h>
#include <linux/irq.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/of_spmi.h>
#include <linux/slab.h>
#include <linux/module.h>

/**
 * Allocate resources for a child of a spmi-container node.
 */
static int of_spmi_allocate_resources(struct spmi_controller *ctrl,
				      struct spmi_boardinfo *info,
				      struct device_node *node,
				      uint32_t num_reg)
{
	int i, num_irq = 0;
	uint64_t size;
	uint32_t flags;
	struct resource *res;
	const  __be32 *addrp;
	struct of_irq oirq;

	while (of_irq_map_one(node, num_irq, &oirq) == 0)
		num_irq++;

	if (num_irq || num_reg) {
		res = kzalloc(sizeof(*res) * (num_irq + num_reg), GFP_KERNEL);
		if (!res)
			return -ENOMEM;

		info->num_resources = num_reg + num_irq;
		info->resource = res;
		for (i = 0; i < num_reg; i++, res++) {
			/* Addresses are always 16 bits */
			addrp = of_get_address(node, i, &size, &flags);
			BUG_ON(!addrp);
			res->start = be32_to_cpup(addrp);
			res->end = res->start + size - 1;
			res->flags = flags;
		}
		WARN_ON(of_irq_to_resource_table(node, res, num_irq) !=
								num_irq);
	}

	return 0;
}

static int of_spmi_create_device(struct spmi_controller *ctrl,
				 struct spmi_boardinfo *info,
			  struct device_node *node)
{
	void *result;
	int rc;

	rc = of_modalias_node(node, info->name, sizeof(info->name));
	if (rc < 0) {
		dev_err(&ctrl->dev, "of_spmi modalias failure on %s\n",
				node->full_name);
		return rc;
	}

	info->of_node = of_node_get(node);
	result = spmi_new_device(ctrl, info);

	if (result == NULL) {
		dev_err(&ctrl->dev, "of_spmi: Failure registering %s\n",
				node->full_name);
		of_node_put(node);
		return -ENODEV;
	}

	return 0;
}

static void of_spmi_walk_container_children(struct spmi_controller *ctrl,
				     struct spmi_boardinfo *info,
				     struct device_node *container)
{
	struct device_node *node;
	uint64_t size;
	uint32_t flags, num_reg = 0;
	int rc;

	for_each_child_of_node(container, node) {
		/*
		 * We can't use of_address_to_resource here since it includes
		 * address translation; and address translation assumes that no
		 * parent buses have a size-cell of 0. But SPMI does have a
		 * size-cell of 0.
		 */
		while (of_get_address(node, num_reg, &size, &flags) != NULL)
			num_reg++;

		rc = of_spmi_allocate_resources(ctrl, info, node, num_reg);
		if (rc) {
			dev_err(&ctrl->dev, "%s: unable to allocate"
						" resources\n", __func__);
			return;
		}
		rc = of_spmi_create_device(ctrl, info, node);
		if (rc) {
			dev_err(&ctrl->dev, "%s: unable to create device for"
				     " node %s\n", __func__, node->full_name);
			return;
		}
	}
}

int of_spmi_register_devices(struct spmi_controller *ctrl)
{
	struct device_node *node;

	/* Only register child devices if the ctrl has a node pointer set */
	if (!ctrl->dev.of_node)
		return -ENODEV;

	for_each_child_of_node(ctrl->dev.of_node, node) {
		struct spmi_boardinfo info = {};
		const __be32 *slave_id;
		int len, rc;

		slave_id = of_get_property(node, "reg", &len);
		if (!slave_id) {
			dev_err(&ctrl->dev, "of_spmi: invalid sid "
					"on %s\n", node->full_name);
			continue;
		}

		info.slave_id = be32_to_cpup(slave_id);

		if (of_get_property(node, "spmi-dev-container", NULL)) {
			of_spmi_walk_container_children(ctrl, &info, node);
			continue;
		} else {
			rc = of_spmi_allocate_resources(ctrl, &info, node, 0);
			if (rc)
				continue;
			of_spmi_create_device(ctrl, &info, node);
		}
	}

	return 0;
}
EXPORT_SYMBOL(of_spmi_register_devices);

MODULE_LICENSE("GPL");
