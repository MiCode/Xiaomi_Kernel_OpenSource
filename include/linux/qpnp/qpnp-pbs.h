/* Copyright (c) 2017, The Linux Foundation. All rights reserved.
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

#ifndef _QPNP_PBS_H
#define _QPNP_PBS_H

#ifdef CONFIG_QPNP_PBS
int qpnp_pbs_trigger_event(struct device_node *dev_node, u8 bitmap);
#else
static inline int qpnp_pbs_trigger_event(struct device_node *dev_node,
						 u8 bitmap) {
	return -ENODEV;
}
#endif

#endif
