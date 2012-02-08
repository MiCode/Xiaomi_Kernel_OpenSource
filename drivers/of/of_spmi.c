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

struct of_spmi_dev_info {
	struct spmi_controller *ctrl;
	struct spmi_boardinfo b_info;
};

struct of_spmi_res_info {
	struct device_node *node;
	uint32_t num_reg;
	uint32_t num_irq;
};

static void of_spmi_sum_resources(struct of_spmi_res_info *r_info, bool has_reg)
{
	struct of_irq oirq;
	uint64_t size;
	uint32_t flags;

	while (of_irq_map_one(r_info->node, r_info->num_irq, &oirq) == 0)
		r_info->num_irq++;

	if (!has_reg)
		return;

	/*
	 * We can't use of_address_to_resource here since it includes
	 * address translation; and address translation assumes that no
	 * parent buses have a size-cell of 0. But SPMI does have a
	 * size-cell of 0.
	 */
	while (of_get_address(r_info->node, r_info->num_reg,
						&size, &flags) != NULL)
		r_info->num_reg++;
}

/**
 * Allocate resources for a child of a spmi-slave node.
 */
static int of_spmi_allocate_resources(struct of_spmi_dev_info *d_info,
				      struct of_spmi_res_info *r_info)

{
	uint32_t num_irq = r_info->num_irq, num_reg = r_info->num_reg;
	int i;
	struct resource *res;
	const  __be32 *addrp;
	uint64_t size;
	uint32_t flags;

	if (num_irq || num_reg) {
		res = kzalloc(sizeof(*res) * (num_irq + num_reg), GFP_KERNEL);
		if (!res)
			return -ENOMEM;

		d_info->b_info.num_resources = num_reg + num_irq;
		d_info->b_info.resource = res;
		for (i = 0; i < num_reg; i++, res++) {
			/* Addresses are always 16 bits */
			addrp = of_get_address(r_info->node, i, &size, &flags);
			BUG_ON(!addrp);
			res->start = be32_to_cpup(addrp);
			res->end = res->start + size - 1;
			res->flags = flags;
		}
		WARN_ON(of_irq_to_resource_table(r_info->node, res, num_irq) !=
								num_irq);
	}

	return 0;
}

static int of_spmi_create_device(struct of_spmi_dev_info *d_info,
				 struct device_node *node)
{
	struct spmi_controller *ctrl = d_info->ctrl;
	struct spmi_boardinfo *b_info = &d_info->b_info;
	void *result;
	int rc;

	rc = of_modalias_node(node, b_info->name, sizeof(b_info->name));
	if (rc < 0) {
		dev_err(&ctrl->dev, "of_spmi modalias failure on %s\n",
				node->full_name);
		return rc;
	}

	b_info->of_node = of_node_get(node);
	result = spmi_new_device(ctrl, b_info);

	if (result == NULL) {
		dev_err(&ctrl->dev, "of_spmi: Failure registering %s\n",
				node->full_name);
		of_node_put(node);
		return -ENODEV;
	}

	return 0;
}

static void of_spmi_walk_slave_container(struct of_spmi_dev_info *d_info,
					struct device_node *container)
{
	struct spmi_controller *ctrl = d_info->ctrl;
	struct device_node *node;
	int rc;

	for_each_child_of_node(container, node) {
		struct of_spmi_res_info r_info;

		r_info.node = node;
		of_spmi_sum_resources(&r_info, 1);

		rc = of_spmi_allocate_resources(d_info, &r_info);
		if (rc) {
			dev_err(&ctrl->dev, "%s: unable to allocate"
						" resources\n", __func__);
			return;
		}
		rc = of_spmi_create_device(d_info, node);
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
		struct of_spmi_dev_info d_info = {};
		const __be32 *slave_id;
		int len, rc;

		slave_id = of_get_property(node, "reg", &len);
		if (!slave_id) {
			dev_err(&ctrl->dev, "of_spmi: invalid sid "
					"on %s\n", node->full_name);
			continue;
		}

		d_info.b_info.slave_id = be32_to_cpup(slave_id);
		d_info.ctrl = ctrl;

		if (of_get_property(node, "spmi-slave-container", NULL)) {
			of_spmi_walk_slave_container(&d_info, node);
			continue;
		} else {
			struct of_spmi_res_info r_info;

			r_info.node = node;
			of_spmi_sum_resources(&r_info, 0);
			rc = of_spmi_allocate_resources(&d_info, &r_info);
			if (rc)
				continue;
			of_spmi_create_device(&d_info, node);
		}
	}

	return 0;
}
EXPORT_SYMBOL(of_spmi_register_devices);

MODULE_LICENSE("GPL");
