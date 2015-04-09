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
#include <linux/types.h>

struct of_spmi_dev_info {
	struct spmi_controller *ctrl;
	struct spmi_boardinfo b_info;
};

struct of_spmi_res_info {
	struct device_node *node;
	uint32_t num_reg;
	uint32_t num_irq;
};

/*
 * Initialize r_info structure for safe usage
 */
static inline void of_spmi_init_resource(struct of_spmi_res_info *r_info,
					 struct device_node *node)
{
	r_info->node = node;
	r_info->num_reg = 0;
	r_info->num_irq = 0;
}

/*
 * Calculate the number of resources to allocate
 *
 * The caller is responsible for initializing the of_spmi_res_info structure.
 */
static void of_spmi_sum_resources(struct of_spmi_res_info *r_info,
				  bool has_reg)
{
	struct of_phandle_args oirq;
	uint64_t size;
	uint32_t flags;
	int i = 0;

	while (of_irq_parse_one(r_info->node, i, &oirq) == 0)
		i++;

	r_info->num_irq += i;

	if (!has_reg)
		return;

	/*
	 * We can't use of_address_to_resource here since it includes
	 * address translation; and address translation assumes that no
	 * parent buses have a size-cell of 0. But SPMI does have a
	 * size-cell of 0.
	 */
	i = 0;
	while (of_get_address(r_info->node, i, &size, &flags) != NULL)
		i++;

	r_info->num_reg += i;
}

/*
 * Allocate dev_node array for spmi_device - used with spmi-dev-container
 */
static inline int of_spmi_alloc_devnode_store(struct of_spmi_dev_info *d_info,
					      uint32_t num_dev_node)
{
	d_info->b_info.num_dev_node = num_dev_node;
	d_info->b_info.dev_node = kzalloc(sizeof(struct spmi_resource) *
						num_dev_node, GFP_KERNEL);
	if (!d_info->b_info.dev_node)
		return -ENOMEM;

	return 0;
}

/*
 * Allocate enough memory to handle the resources associated with the
 * primary node.
 */
static int of_spmi_allocate_node_resources(struct of_spmi_dev_info *d_info,
					   struct of_spmi_res_info *r_info)
{
	uint32_t num_irq = r_info->num_irq, num_reg = r_info->num_reg;
	struct resource *res = NULL;

	if (num_irq || num_reg) {
		res = kzalloc(sizeof(*res) * (num_irq + num_reg), GFP_KERNEL);
		if (!res)
			return -ENOMEM;
	}
	d_info->b_info.res.num_resources = num_reg + num_irq;
	d_info->b_info.res.resource = res;

	return 0;
}

/*
 * Allocate enough memory to handle the resources associated with the
 * spmi-dev-container nodes.
 */
static int of_spmi_allocate_devnode_resources(struct of_spmi_dev_info *d_info,
					      struct of_spmi_res_info *r_info,
					      uint32_t idx)
{
	uint32_t num_irq = r_info->num_irq, num_reg = r_info->num_reg;
	struct resource *res = NULL;

	if (num_irq || num_reg) {
		res = kzalloc(sizeof(*res) * (num_irq + num_reg), GFP_KERNEL);
		if (!res)
			return -ENOMEM;
	}
	d_info->b_info.dev_node[idx].num_resources = num_reg + num_irq;
	d_info->b_info.dev_node[idx].resource = res;

	return 0;
}

/*
 * free node resources - used with primary node
 */
static void of_spmi_free_node_resources(struct of_spmi_dev_info *d_info)
{
	kfree(d_info->b_info.res.resource);
}

/*
 * free devnode resources - used with spmi-dev-container
 */
static void of_spmi_free_devnode_resources(struct of_spmi_dev_info *d_info)
{
	int i;

	for (i = 0; i < d_info->b_info.num_dev_node; i++)
		kfree(d_info->b_info.dev_node[i].resource);

	kfree(d_info->b_info.dev_node);
}

static void of_spmi_populate_resources(struct of_spmi_dev_info *d_info,
				       struct of_spmi_res_info *r_info,
				       struct resource *res)

{
	uint32_t num_irq = r_info->num_irq, num_reg = r_info->num_reg;
	int i;
	const  __be32 *addrp;
	uint64_t size;
	uint32_t flags;

	if ((num_irq || num_reg) && (res != NULL)) {
		for (i = 0; i < num_reg; i++, res++) {
			/* Addresses are always 16 bits */
			addrp = of_get_address(r_info->node, i, &size, &flags);
			BUG_ON(!addrp);
			res->start = be32_to_cpup(addrp);
			res->end = res->start + size - 1;
			res->flags = flags;
			of_property_read_string_index(r_info->node, "reg-names",
								i, &res->name);
		}
		WARN_ON(of_irq_to_resource_table(r_info->node, res, num_irq) !=
								num_irq);
	}
}

/*
 * Gather primary node resources and populate.
 */
static void of_spmi_populate_node_resources(struct of_spmi_dev_info *d_info,
					    struct of_spmi_res_info *r_info)

{
	struct resource *res;

	res = d_info->b_info.res.resource;
	d_info->b_info.res.of_node = r_info->node;
	of_property_read_string(r_info->node, "label",
				&d_info->b_info.res.label);
	of_spmi_populate_resources(d_info, r_info, res);
}

/*
 * Gather node devnode resources and populate - used with spmi-dev-container.
 */
static void of_spmi_populate_devnode_resources(struct of_spmi_dev_info *d_info,
					       struct of_spmi_res_info *r_info,
					       int idx)

{
	struct resource *res;

	res = d_info->b_info.dev_node[idx].resource;
	d_info->b_info.dev_node[idx].of_node = r_info->node;
	of_property_read_string(r_info->node, "label",
				&d_info->b_info.dev_node[idx].label);
	of_spmi_populate_resources(d_info, r_info, res);
}

/*
 * create a single spmi_device
 */
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

/*
 * Walks all children of a node containing the spmi-dev-container
 * binding. This special type of spmi_device can include resources
 * from more than one device node.
 */
static void of_spmi_walk_dev_container(struct of_spmi_dev_info *d_info,
					struct device_node *container)
{
	struct of_spmi_res_info r_info = {};
	struct spmi_controller *ctrl = d_info->ctrl;
	struct device_node *node;
	int rc, i, num_dev_node = 0;

	if (!of_device_is_available(container))
		return;

	/*
	 * Count the total number of device_nodes so we know how much
	 * device_store to allocate.
	 */
	for_each_child_of_node(container, node) {
		if (!of_device_is_available(node))
			continue;
		num_dev_node++;
	}

	rc = of_spmi_alloc_devnode_store(d_info, num_dev_node);
	if (rc) {
		dev_err(&ctrl->dev, "%s: unable to allocate devnode resources\n",
								__func__);
		return;
	}

	i = 0;
	for_each_child_of_node(container, node) {
		if (!of_device_is_available(node))
			continue;
		of_spmi_init_resource(&r_info, node);
		of_spmi_sum_resources(&r_info, true);
		rc = of_spmi_allocate_devnode_resources(d_info, &r_info, i);
		if (rc) {
			dev_err(&ctrl->dev, "%s: unable to allocate"
					" resources\n", __func__);
			of_spmi_free_devnode_resources(d_info);
			return;
		}
		of_spmi_populate_devnode_resources(d_info, &r_info, i);
		i++;
	}

	of_spmi_init_resource(&r_info, container);
	of_spmi_sum_resources(&r_info, true);

	rc = of_spmi_allocate_node_resources(d_info, &r_info);
	if (rc) {
		dev_err(&ctrl->dev, "%s: unable to allocate resources\n",
								  __func__);
		of_spmi_free_node_resources(d_info);
	}

	of_spmi_populate_node_resources(d_info, &r_info);


	rc = of_spmi_create_device(d_info, container);
	if (rc) {
		dev_err(&ctrl->dev, "%s: unable to create device for"
				" node %s\n", __func__, container->full_name);
		of_spmi_free_devnode_resources(d_info);
		return;
	}
}

/*
 * Walks all children of a node containing the spmi-slave-container
 * binding. This indicates that all spmi_devices created from this
 * point all share the same slave_id.
 */
static void of_spmi_walk_slave_container(struct of_spmi_dev_info *d_info,
					 struct device_node *container)
{
	struct spmi_controller *ctrl = d_info->ctrl;
	struct device_node *node;
	int rc;

	for_each_child_of_node(container, node) {
		struct of_spmi_res_info r_info;

		if (!of_device_is_available(node))
			continue;

		/**
		 * Check to see if this node contains children which
		 * should be all created as the same spmi_device.
		 */
		if (of_get_property(node, "spmi-dev-container", NULL)) {
			of_spmi_walk_dev_container(d_info, node);
			continue;
		}

		of_spmi_init_resource(&r_info, node);
		of_spmi_sum_resources(&r_info, true);

		rc = of_spmi_allocate_node_resources(d_info, &r_info);
		if (rc) {
			dev_err(&ctrl->dev, "%s: unable to allocate"
						" resources\n", __func__);
			goto slave_err;
		}

		of_spmi_populate_node_resources(d_info, &r_info);

		rc = of_spmi_create_device(d_info, node);
		if (rc) {
			dev_err(&ctrl->dev, "%s: unable to create device for"
				     " node %s\n", __func__, node->full_name);
			goto slave_err;
		}
	}
	return;

slave_err:
	of_spmi_free_node_resources(d_info);
}

int of_spmi_register_devices(struct spmi_controller *ctrl)
{
	struct device_node *node = ctrl->dev.of_node;

	/* Only register child devices if the ctrl has a node pointer set */
	if (!node)
		return -ENODEV;

	if (of_get_property(node, "spmi-slave-container", NULL)) {
		dev_err(&ctrl->dev, "%s: structural error: spmi-slave-container"
			" is prohibited at the root level\n", __func__);
		return -EINVAL;
	} else if (of_get_property(node, "spmi-dev-container", NULL)) {
		dev_err(&ctrl->dev, "%s: structural error: spmi-dev-container"
			" is prohibited at the root level\n", __func__);
		return -EINVAL;
	}

	/**
	 * Make best effort to launch as many nodes as possible. If there are
	 * syntax errors, we will simply ignore that subtree and keep going.
	 */
	for_each_child_of_node(ctrl->dev.of_node, node) {
		struct of_spmi_dev_info d_info = {};
		const __be32 *slave_id;
		int len, rc, have_dev_container = 0;

		slave_id = of_get_property(node, "reg", &len);
		if (!slave_id) {
			dev_err(&ctrl->dev, "%s: invalid sid "
					"on %s\n", __func__, node->full_name);
			continue;
		}

		d_info.b_info.slave_id = be32_to_cpup(slave_id);
		d_info.ctrl = ctrl;

		if (of_get_property(node, "spmi-dev-container", NULL))
			have_dev_container = 1;
		if (of_get_property(node, "spmi-slave-container", NULL)) {
			if (have_dev_container)
				of_spmi_walk_dev_container(&d_info, node);
			else
				of_spmi_walk_slave_container(&d_info, node);
		} else {
			struct of_spmi_res_info r_info;

			/**
			 * A dev container at the second level without a slave
			 * container is considered an error.
			 */
			if (have_dev_container) {
				dev_err(&ctrl->dev, "%s: structural error,"
				     " node %s has spmi-dev-container without"
				     " specifying spmi-slave-container\n",
				     __func__, node->full_name);
				continue;
			}

			if (!of_device_is_available(node))
				continue;

			of_spmi_init_resource(&r_info, node);
			of_spmi_sum_resources(&r_info, false);
			rc = of_spmi_allocate_node_resources(&d_info, &r_info);
			if (rc) {
				dev_err(&ctrl->dev, "%s: unable to allocate"
						" resources\n", __func__);
				of_spmi_free_node_resources(&d_info);
				continue;
			}

			of_spmi_populate_node_resources(&d_info, &r_info);

			rc = of_spmi_create_device(&d_info, node);
			if (rc) {
				dev_err(&ctrl->dev, "%s: unable to create"
						" device\n", __func__);
				of_spmi_free_node_resources(&d_info);
				continue;
			}
		}
	}

	return 0;
}
EXPORT_SYMBOL(of_spmi_register_devices);

MODULE_LICENSE("GPL v2");
