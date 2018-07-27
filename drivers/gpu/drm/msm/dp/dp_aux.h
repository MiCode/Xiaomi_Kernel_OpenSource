/*
 * Copyright (c) 2012-2018, The Linux Foundation. All rights reserved.
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

#ifndef _DP_AUX_H_
#define _DP_AUX_H_

#include "dp_catalog.h"
#include "drm_dp_helper.h"

#define DP_STATE_NOTIFICATION_SENT          BIT(0)
#define DP_STATE_TRAIN_1_STARTED            BIT(1)
#define DP_STATE_TRAIN_1_SUCCEEDED          BIT(2)
#define DP_STATE_TRAIN_1_FAILED             BIT(3)
#define DP_STATE_TRAIN_2_STARTED            BIT(4)
#define DP_STATE_TRAIN_2_SUCCEEDED          BIT(5)
#define DP_STATE_TRAIN_2_FAILED             BIT(6)
#define DP_STATE_CTRL_POWERED_ON            BIT(7)
#define DP_STATE_CTRL_POWERED_OFF           BIT(8)
#define DP_STATE_LINK_MAINTENANCE_STARTED   BIT(9)
#define DP_STATE_LINK_MAINTENANCE_COMPLETED BIT(10)
#define DP_STATE_LINK_MAINTENANCE_FAILED    BIT(11)

enum dp_aux_error {
	DP_AUX_ERR_NONE	= 0,
	DP_AUX_ERR_ADDR	= -1,
	DP_AUX_ERR_TOUT	= -2,
	DP_AUX_ERR_NACK	= -3,
	DP_AUX_ERR_DEFER	= -4,
	DP_AUX_ERR_NACK_DEFER	= -5,
	DP_AUX_ERR_PHY	= -6,
};

struct dp_aux {
	u32 reg;
	u32 state;

	struct drm_dp_aux *drm_aux;
	int (*drm_aux_register)(struct dp_aux *aux);
	void (*drm_aux_deregister)(struct dp_aux *aux);
	void (*isr)(struct dp_aux *aux);
	void (*init)(struct dp_aux *aux, struct dp_aux_cfg *aux_cfg);
	void (*deinit)(struct dp_aux *aux);
	void (*reconfig)(struct dp_aux *aux);
	void (*abort)(struct dp_aux *aux);
	void (*dpcd_updated)(struct dp_aux *aux);
	void (*set_sim_mode)(struct dp_aux *aux, bool en, u8 *edid, u8 *dpcd);
	int (*aux_switch)(struct dp_aux *aux, bool enable, int orientation);
};

struct dp_aux *dp_aux_get(struct device *dev, struct dp_catalog_aux *catalog,
		struct dp_parser *parser, struct device_node *aux_switch);
void dp_aux_put(struct dp_aux *aux);

#endif /*__DP_AUX_H_*/
