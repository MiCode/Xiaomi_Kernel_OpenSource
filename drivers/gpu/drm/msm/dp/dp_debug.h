/*
 * Copyright (c) 2017-2019, The Linux Foundation. All rights reserved.
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

#ifndef _DP_DEBUG_H_
#define _DP_DEBUG_H_

#include "dp_panel.h"
#include "dp_ctrl.h"
#include "dp_link.h"
#include "dp_usbpd.h"
#include "dp_aux.h"
#include "dp_display.h"

/**
 * struct dp_debug
 * @debug_en: specifies whether debug mode enabled
 * @vdisplay: used to filter out vdisplay value
 * @hdisplay: used to filter out hdisplay value
 * @vrefresh: used to filter out vrefresh value
 * @tpg_state: specifies whether tpg feature is enabled
 * @max_pclk_khz: max pclk supported
 * @force_encryption: enable/disable forced encryption for HDCP 2.2
 */
struct dp_debug {
	bool debug_en;
	bool sim_mode;
	bool psm_enabled;
	bool hdcp_disabled;
	int aspect_ratio;
	int vdisplay;
	int hdisplay;
	int vrefresh;
	bool tpg_state;
	u32 max_pclk_khz;
	bool force_encryption;
	char hdcp_status[SZ_128];
	struct dp_mst_connector dp_mst_connector_list;
	bool mst_hpd_sim;
	u32 mst_port_cnt;

	u8 *(*get_edid)(struct dp_debug *dp_debug);
	void (*abort)(struct dp_debug *dp_debug);
};

/**
 * struct dp_debug_in
 * @dev: device instance of the caller
 * @panel: instance of panel module
 * @hpd: instance of hpd module
 * @link: instance of link module
 * @aux: instance of aux module
 * @connector: double pointer to display connector
 * @catalog: instance of catalog module
 * @parser: instance of parser module
 */
struct dp_debug_in {
	struct device *dev;
	struct dp_panel *panel;
	struct dp_hpd *hpd;
	struct dp_link *link;
	struct dp_aux *aux;
	struct drm_connector **connector;
	struct dp_catalog *catalog;
	struct dp_parser *parser;
	struct dp_ctrl *ctrl;
};

/**
 * dp_debug_get() - configure and get the DisplayPlot debug module data
 *
 * @in: input structure containing data to initialize the debug module
 * return: pointer to allocated debug module data
 *
 * This function sets up the debug module and provides a way
 * for debugfs input to be communicated with existing modules
 */
struct dp_debug *dp_debug_get(struct dp_debug_in *in);

/**
 * dp_debug_put()
 *
 * Cleans up dp_debug instance
 *
 * @dp_debug: instance of dp_debug
 */
void dp_debug_put(struct dp_debug *dp_debug);
#endif /* _DP_DEBUG_H_ */
