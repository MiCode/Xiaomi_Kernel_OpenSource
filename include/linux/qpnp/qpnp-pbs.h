/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2017-2019, The Linux Foundation. All rights reserved.
 */

#ifndef _QPNP_PBS_H
#define _QPNP_PBS_H

#include <linux/types.h>

struct device_node;

#if IS_ENABLED(CONFIG_QPNP_PBS)
int qpnp_pbs_trigger_event(struct device_node *dev_node, u8 bitmap);
#else
static inline int qpnp_pbs_trigger_event(struct device_node *dev_node,
						 u8 bitmap) {
	return -ENODEV;
}
#endif

#endif
