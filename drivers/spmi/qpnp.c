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

#include <mach/qpnp.h>

/**
 * qpnp_get_resource - get a resource for a device
 * @dev: qpnp device
 * @type: resource type
 * @num: resource index
 */
struct resource *qpnp_get_resource(struct spmi_device *dev,
				   unsigned int node_idx, unsigned int type,
				   unsigned int res_num)
{
	int i;

	for (i = 0; i < dev->dev_node[node_idx].num_resources; i++) {
		struct resource *r = &dev->dev_node[node_idx].resource[i];

		if (type == resource_type(r) && res_num-- == 0)
			return r;
	}
	return NULL;
}
EXPORT_SYMBOL_GPL(qpnp_get_resource);

/**
 * qpnp_get_irq - get an IRQ for a device
 * @dev: qpnp device
 * @num: IRQ number index
 */
int qpnp_get_irq(struct spmi_device *dev, unsigned int node_idx,
					  unsigned int res_num)
{
	struct resource *r = qpnp_get_resource(dev, node_idx,
						IORESOURCE_IRQ, res_num);

	return r ? r->start : -ENXIO;
}
EXPORT_SYMBOL_GPL(qpnp_get_irq);

