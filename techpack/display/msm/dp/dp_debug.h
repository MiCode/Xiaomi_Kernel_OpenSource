/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2017-2020, The Linux Foundation. All rights reserved.
 */

#ifndef _DP_DEBUG_H_
#define _DP_DEBUG_H_

#include "dp_panel.h"
#include "dp_ctrl.h"
#include "dp_link.h"
#include "dp_aux.h"
#include "dp_display.h"
#include "dp_pll.h"

#define DP_DEBUG(fmt, ...)                                                   \
	do {                                                                 \
		if (unlikely(drm_debug & DRM_UT_KMS))                        \
			DRM_DEBUG("[msm-dp-debug][%-4d]"fmt, current->pid,   \
					##__VA_ARGS__);                      \
		else                                                         \
			pr_debug("[drm:%s][msm-dp-debug][%-4d]"fmt, __func__,\
				       current->pid, ##__VA_ARGS__);         \
	} while (0)

#define DP_INFO(fmt, ...)                                                    \
	do {                                                                 \
		if (unlikely(drm_debug & DRM_UT_KMS))                        \
			DRM_INFO("[msm-dp-info][%-4d]"fmt, current->pid,    \
					##__VA_ARGS__);                      \
		else                                                         \
			pr_info("[drm:%s][msm-dp-info][%-4d]"fmt, __func__, \
				       current->pid, ##__VA_ARGS__);         \
	} while (0)

#define DP_WARN(fmt, ...)                                    \
	pr_warn("[drm:%s][msm-dp-warn][%-4d]"fmt, __func__,  \
			current->pid, ##__VA_ARGS__)

#define DP_ERR(fmt, ...)                                    \
	pr_err("[drm:%s][msm-dp-err][%-4d]"fmt, __func__,   \
		       current->pid, ##__VA_ARGS__)

#define DEFAULT_DISCONNECT_DELAY_MS 0
#define MAX_DISCONNECT_DELAY_MS 10000
#define DEFAULT_CONNECT_NOTIFICATION_DELAY_MS 150
#define MAX_CONNECT_NOTIFICATION_DELAY_MS 5000

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
 * @skip_uevent: skip hotplug uevent to the user space
 * @hdcp_status: string holding hdcp status information
 * @dp_mst_connector_list: list containing all dp mst connectors
 * @mst_hpd_sim: specifies whether simulated hpd enabled
 * @mst_sim_add_con: specifies whether new sim connector is to be added
 * @mst_sim_remove_con: specifies whether sim connector is to be removed
 * @mst_sim_remove_con_id: specifies id of sim connector to be removed
 * @mst_port_cnt: number of mst ports to be added during hpd
 * @connect_notification_delay_ms: time (in ms) to wait for any attention
 *              messages before sending the connect notification uevent
 * @disconnect_delay_ms: time (in ms) to wait before turning off the mainlink
 *              in response to HPD low of cable disconnect event
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
	bool skip_uevent;
	char hdcp_status[SZ_128];
	struct dp_mst_connector dp_mst_connector_list;
	bool mst_hpd_sim;
	bool mst_sim_add_con;
	bool mst_sim_remove_con;
	int mst_sim_remove_con_id;
	u32 mst_port_cnt;
	unsigned long connect_notification_delay_ms;
	u32 disconnect_delay_ms;

	struct dp_mst_connector mst_connector_cache;
	u8 *(*get_edid)(struct dp_debug *dp_debug);
	void (*abort)(struct dp_debug *dp_debug);
	void (*set_mst_con)(struct dp_debug *dp_debug, int con_id);
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
 * @ctrl: instance of controller module
 * @pll: instance of pll module
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
	struct dp_pll *pll;
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
