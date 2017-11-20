/*
 * Copyright (c) 2012-2017, The Linux Foundation. All rights reserved.
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

#ifndef _DP_CTRL_H_
#define _DP_CTRL_H_

#include "dp_aux.h"
#include "dp_panel.h"
#include "dp_link.h"
#include "dp_parser.h"
#include "dp_power.h"
#include "dp_catalog.h"

struct dp_ctrl {
	int (*init)(struct dp_ctrl *dp_ctrl, bool flip, bool multi_func);
	void (*deinit)(struct dp_ctrl *dp_ctrl);
	int (*on)(struct dp_ctrl *dp_ctrl);
	void (*off)(struct dp_ctrl *dp_ctrl);
	void (*reset)(struct dp_ctrl *dp_ctrl);
	void (*push_idle)(struct dp_ctrl *dp_ctrl);
	void (*abort)(struct dp_ctrl *dp_ctrl);
	void (*isr)(struct dp_ctrl *dp_ctrl);
	bool (*handle_sink_request)(struct dp_ctrl *dp_ctrl);
	void (*process_phy_test_request)(struct dp_ctrl *dp_ctrl);
	int (*link_maintenance)(struct dp_ctrl *dp_ctrl);
};

struct dp_ctrl_in {
	struct device *dev;
	struct dp_panel *panel;
	struct dp_aux *aux;
	struct dp_link *link;
	struct dp_parser *parser;
	struct dp_power *power;
	struct dp_catalog_ctrl *catalog;
};

struct dp_ctrl *dp_ctrl_get(struct dp_ctrl_in *in);
void dp_ctrl_put(struct dp_ctrl *dp_ctrl);

#endif /* _DP_CTRL_H_ */
