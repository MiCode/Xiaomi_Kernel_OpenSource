/* Copyright (c) 2002-3 Patrick Mochel
 * Copyright (c) 2002-3 Open Source Development Labs
 * Copyright (c) 2012, Code Aurora Forum. All rights reserved.
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
 * Resource handling based on platform.c.
 */

#include <linux/export.h>
#include <linux/spmi.h>
#include <linux/string.h>

/**
 * spmi_get_resource - get a resource for a device
 * @dev: spmi device
 * @node: device node resource
 * @type: resource type
 * @res_num: resource index
 *
 * If 'node' is specified as NULL, then the API treats this as a special
 * case to assume the first devnode. For configurations that do not use
 * spmi-dev-container, there is only one node to begin with, so NULL should
 * be passed in this case.
 *
 * Returns
 *  NULL on failure.
 */
struct resource *spmi_get_resource(struct spmi_device *dev,
				   struct spmi_resource *node,
				   unsigned int type, unsigned int res_num)
{
	int i;

	/* if a node is not specified, default to the first node */
	if (!node)
		node = &dev->res;

	for (i = 0; i < node->num_resources; i++) {
		struct resource *r = &node->resource[i];

		if (type == resource_type(r) && res_num-- == 0)
			return r;
	}
	return NULL;
}
EXPORT_SYMBOL_GPL(spmi_get_resource);

#define SPMI_MAX_RES_NAME 256

/**
 * spmi_get_resource_byname - get a resource for a device given a name
 * @dev: spmi device handle
 * @node: device node resource
 * @type: resource type
 * @name: resource name to lookup
 */
struct resource *spmi_get_resource_byname(struct spmi_device *dev,
					  struct spmi_resource *node,
					  unsigned int type,
					  const char *name)
{
	int i;

	/* if a node is not specified, default to the first node */
	if (!node)
		node = &dev->res;

	for (i = 0; i < node->num_resources; i++) {
		struct resource *r = &node->resource[i];

		if (type == resource_type(r) && r->name &&
				!strncmp(r->name, name, SPMI_MAX_RES_NAME))
			return r;
	}
	return NULL;
}
EXPORT_SYMBOL_GPL(spmi_get_resource_byname);

/**
 * spmi_get_irq - get an IRQ for a device
 * @dev: spmi device
 * @node: device node resource
 * @res_num: IRQ number index
 *
 * Returns
 *  -ENXIO on failure.
 */
int spmi_get_irq(struct spmi_device *dev, struct spmi_resource *node,
					  unsigned int res_num)
{
	struct resource *r = spmi_get_resource(dev, node,
						IORESOURCE_IRQ, res_num);

	return r ? r->start : -ENXIO;
}
EXPORT_SYMBOL_GPL(spmi_get_irq);

/**
 * spmi_get_irq_byname - get an IRQ for a device given a name
 * @dev: spmi device handle
 * @node: device node resource
 * @name: resource name to lookup
 *
 * Returns -ENXIO on failure
 */
int spmi_get_irq_byname(struct spmi_device *dev,
			struct spmi_resource *node, const char *name)
{
	struct resource *r = spmi_get_resource_byname(dev, node,
							IORESOURCE_IRQ, name);
	return r ? r->start : -ENXIO;
}
EXPORT_SYMBOL_GPL(spmi_get_irq_byname);

/*
 * spmi_get_container_dev_byname - get a device node resource
 * @dev: spmi device handle
 * @label: device name to lookup
 *
 * Only useable in spmi-dev-container configurations. Given a name,
 * find the associated spmi_resource that matches the name.
 *
 * Return NULL if the spmi_device is not a dev-container,
 * or if the lookup fails.
 */
struct spmi_resource *spmi_get_dev_container_byname(struct spmi_device *dev,
						    const char *label)
{
	int i;

	if (!label)
		return NULL;

	for (i = 0; i < dev->num_dev_node; i++) {
		struct spmi_resource *r = &dev->dev_node[i];

		if (r && r->label && !strncmp(r->label,
					label, SPMI_MAX_RES_NAME))
			return r;
	}
	return NULL;
}
EXPORT_SYMBOL(spmi_get_dev_container_byname);
