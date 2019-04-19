/* Copyright (c) 2012, 2014-2019, The Linux Foundation. All rights reserved.
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

#ifndef __SDE_HDCP_H__
#define __SDE_HDCP_H__

#include <soc/qcom/scm.h>

#include <linux/types.h>
#include <linux/bitops.h>
#include <linux/debugfs.h>
#include <linux/of_device.h>
#include <linux/i2c.h>
#include <linux/list.h>
#include <drm/drmP.h>
#include <drm/drm_crtc.h>
#include <drm/drm_edid.h>
#include <linux/hdcp_qseecom.h>
#include "sde_kms.h"

#define MAX_STREAM_COUNT 2

enum sde_hdcp_client_id {
	HDCP_CLIENT_HDMI,
	HDCP_CLIENT_DP,
};

enum sde_hdcp_state {
	HDCP_STATE_INACTIVE,
	HDCP_STATE_AUTHENTICATING,
	HDCP_STATE_AUTHENTICATED,
	HDCP_STATE_AUTH_FAIL,
};

enum sde_hdcp_version {
	HDCP_VERSION_NONE,
	HDCP_VERSION_1X = BIT(0),
	HDCP_VERSION_2P2 = BIT(1),
	HDCP_VERSION_MAX = BIT(2),
};

struct stream_info {
	u8 stream_id;
	u8 virtual_channel;
};

struct sde_hdcp_stream {
	struct list_head list;
	u8 stream_id;
	u8 virtual_channel;
	u32 stream_handle;
	bool active;
};

struct sde_hdcp_init_data {
	struct device *msm_hdcp_dev;
	struct dss_io_data *core_io;
	struct dss_io_data *dp_ahb;
	struct dss_io_data *dp_aux;
	struct dss_io_data *dp_link;
	struct dss_io_data *dp_p0;
	struct dss_io_data *qfprom_io;
	struct dss_io_data *hdcp_io;
	struct drm_dp_aux *drm_aux;
	struct mutex *mutex;
	struct workqueue_struct *workq;
	void *cb_data;
	void (*notify_status)(void *cb_data, enum sde_hdcp_state state);
	u8 sink_rx_status;
	unsigned char *revision;
	u32 phy_addr;
	bool sec_access;
	enum sde_hdcp_client_id client_id;
};

struct sde_hdcp_ops {
	int (*isr)(void *ptr);
	int (*cp_irq)(void *ptr);
	int (*reauthenticate)(void *input);
	int (*authenticate)(void *hdcp_ctrl);
	bool (*feature_supported)(void *input);
	void (*force_encryption)(void *input, bool enable);
	bool (*sink_support)(void *input);
	int (*set_mode)(void *input, bool mst_enabled);
	int (*on)(void *input);
	void (*off)(void *hdcp_ctrl);
	int (*register_streams)(void *input, u8 num_streams,
			struct stream_info *streams);
	int (*deregister_streams)(void *input, u8 num_streams,
			struct stream_info *streams);
};

static inline const char *sde_hdcp_state_name(enum sde_hdcp_state hdcp_state)
{
	switch (hdcp_state) {
	case HDCP_STATE_INACTIVE:	return "HDCP_STATE_INACTIVE";
	case HDCP_STATE_AUTHENTICATING:	return "HDCP_STATE_AUTHENTICATING";
	case HDCP_STATE_AUTHENTICATED:	return "HDCP_STATE_AUTHENTICATED";
	case HDCP_STATE_AUTH_FAIL:	return "HDCP_STATE_AUTH_FAIL";
	default:			return "???";
	}
}

static inline const char *sde_hdcp_version(enum sde_hdcp_version hdcp_version)
{
	switch (hdcp_version) {
	case HDCP_VERSION_NONE:		return "HDCP_VERSION_NONE";
	case HDCP_VERSION_1X:		return "HDCP_VERSION_1X";
	case HDCP_VERSION_2P2:		return "HDCP_VERSION_2P2";
	default:			return "???";
	}
}

void *sde_hdcp_1x_init(struct sde_hdcp_init_data *init_data);
void sde_hdcp_1x_deinit(void *input);
struct sde_hdcp_ops *sde_hdcp_1x_get(void *input);
void *sde_dp_hdcp2p2_init(struct sde_hdcp_init_data *init_data);
void sde_dp_hdcp2p2_deinit(void *input);
struct sde_hdcp_ops *sde_dp_hdcp2p2_get(void *input);
#endif /* __SDE_HDCP_H__ */
