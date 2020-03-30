/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2017-2019, The Linux Foundation. All rights reserved.
 */

#ifndef _DP_DEBUG_H_
#define _DP_DEBUG_H_

#include "dp_panel.h"
#include "dp_ctrl.h"
#include "dp_link.h"
#include "dp_usbpd.h"
#include "dp_aux.h"
#include "dp_display.h"

#define DP_WARN(fmt, ...)	DRM_WARN("[msm-dp-warn] "fmt, ##__VA_ARGS__)
#define DP_ERR(fmt, ...)	DRM_DEV_ERROR(NULL, "[msm-dp-error]" fmt, \
								##__VA_ARGS__)
#define DP_INFO(fmt, ...)	DRM_DEV_INFO(NULL, "[msm-dp-info] "fmt, \
								##__VA_ARGS__)
#define DP_DEBUG(fmt, ...)	DRM_DEV_DEBUG_DP(NULL, "[msm-dp-debug] "fmt, \
								##__VA_ARGS__)

/**
 * struct dp_debug
 * @debug_en: specifies whether debug mode enabled
 * @sim_mode: specifies whether sim mode enabled
 * @psm_enabled: specifies whether psm enabled
 * @hdcp_disabled: specifies if hdcp is disabled
 * @hdcp_wait_sink_sync: used to wait for sink synchronization before HDCP auth
 * @aspect_ratio: used to filter out aspect_ratio value
 * @vdisplay: used to filter out vdisplay value
 * @hdisplay: used to filter out hdisplay value
 * @vrefresh: used to filter out vrefresh value
 * @tpg_state: specifies whether tpg feature is enabled
 * @max_pclk_khz: max pclk supported
 * @force_encryption: enable/disable forced encryption for HDCP 2.2
 * @hdcp_status: string holding hdcp status information
 * @dp_mst_connector_list: list containing all dp mst connectors
 * @mst_hpd_sim: specifies whether simulated hpd enabled
 * @mst_sim_add_con: specifies whether new sim connector is to be added
 * @mst_sim_remove_con: specifies whether sim connector is to be removed
 * @mst_sim_remove_con_id: specifies id of sim connector to be removed
 * @mst_port_cnt: number of mst ports to be added during hpd
 */
struct dp_debug {
	bool debug_en;
	bool sim_mode;
	bool psm_enabled;
	bool hdcp_disabled;
	bool hdcp_wait_sink_sync;
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
	bool mst_sim_add_con;
	bool mst_sim_remove_con;
	int mst_sim_remove_con_id;
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
