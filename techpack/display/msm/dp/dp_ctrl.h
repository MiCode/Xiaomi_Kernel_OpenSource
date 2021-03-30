/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2012-2020, The Linux Foundation. All rights reserved.
 */

#ifndef _DP_CTRL_H_
#define _DP_CTRL_H_

#include "dp_aux.h"
#include "dp_panel.h"
#include "dp_link.h"
#include "dp_parser.h"
#include "dp_power.h"
#include "dp_catalog.h"
#include "dp_debug.h"

struct dp_ctrl {
	int (*init)(struct dp_ctrl *dp_ctrl, bool flip, bool reset);
	void (*deinit)(struct dp_ctrl *dp_ctrl);
	int (*on)(struct dp_ctrl *dp_ctrl, bool mst_mode, bool fec_en,
			bool dsc_en, bool shallow);
	void (*off)(struct dp_ctrl *dp_ctrl);
	void (*abort)(struct dp_ctrl *dp_ctrl, bool abort);
	void (*isr)(struct dp_ctrl *dp_ctrl);
	bool (*handle_sink_request)(struct dp_ctrl *dp_ctrl);
	void (*process_phy_test_request)(struct dp_ctrl *dp_ctrl);
	int (*link_maintenance)(struct dp_ctrl *dp_ctrl);
	int (*stream_on)(struct dp_ctrl *dp_ctrl, struct dp_panel *panel);
	void (*stream_off)(struct dp_ctrl *dp_ctrl, struct dp_panel *panel);
	void (*stream_pre_off)(struct dp_ctrl *dp_ctrl, struct dp_panel *panel);
	void (*set_mst_channel_info)(struct dp_ctrl *dp_ctrl,
			enum dp_stream_id strm,
			u32 ch_start_slot, u32 ch_tot_slots);
	void (*set_sim_mode)(struct dp_ctrl *dp_ctrl, bool en);
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
