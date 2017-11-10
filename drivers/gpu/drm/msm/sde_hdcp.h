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

#ifndef __SDE_HDCP_H__
#define __SDE_HDCP_H__

#include <soc/qcom/scm.h>

#include <linux/types.h>
#include <linux/bitops.h>
#include <linux/debugfs.h>
#include <linux/of_device.h>
#include <linux/i2c.h>
#include <drm/drmP.h>
#include <drm/drm_crtc.h>
#include <drm/drm_edid.h>
#include "hdmi.h"
#include "sde_kms.h"
#include "sde_hdmi_util.h"

#ifdef SDE_HDCP_DEBUG_ENABLE
#define SDE_HDCP_DEBUG(fmt, args...)   SDE_ERROR(fmt, ##args)
#else
#define SDE_HDCP_DEBUG(fmt, args...)   SDE_DEBUG(fmt, ##args)
#endif

#define SDE_HDCP_SRM_FAIL 29

enum sde_hdcp_client_id {
	HDCP_CLIENT_HDMI,
	HDCP_CLIENT_DP,
};

enum sde_hdcp_states {
	HDCP_STATE_INACTIVE,
	HDCP_STATE_AUTHENTICATING,
	HDCP_STATE_AUTHENTICATED,
	HDCP_STATE_AUTH_FAIL,
	HDCP_STATE_AUTH_FAIL_NOREAUTH,
	HDCP_STATE_AUTH_ENC_NONE,
	HDCP_STATE_AUTH_ENC_1X,
	HDCP_STATE_AUTH_ENC_2P2
};

struct sde_hdcp_init_data {
	struct dss_io_data *core_io;
	struct dss_io_data *qfprom_io;
	struct dss_io_data *hdcp_io;
	struct mutex *mutex;
	struct workqueue_struct *workq;
	void *cb_data;
	void (*notify_status)(void *cb_data, enum sde_hdcp_states status);
	struct sde_hdmi_tx_ddc_ctrl *ddc_ctrl;
	u8 sink_rx_status;
	u16 *version;
	u32 phy_addr;
	u32 hdmi_tx_ver;
	bool sec_access;
	enum sde_hdcp_client_id client_id;
};

struct sde_hdcp_ops {
	int (*isr)(void *ptr);
	int (*cp_irq)(void *ptr);
	int (*reauthenticate)(void *input);
	int (*authenticate)(void *hdcp_ctrl);
	bool (*feature_supported)(void *input);
	void (*off)(void *hdcp_ctrl);
};

void *sde_hdcp_1x_init(struct sde_hdcp_init_data *init_data);
void sde_hdcp_1x_deinit(void *input);
struct sde_hdcp_ops *sde_hdcp_1x_start(void *input);
void *sde_hdmi_hdcp2p2_init(struct sde_hdcp_init_data *init_data);
void sde_hdmi_hdcp2p2_deinit(void *input);
const char *sde_hdcp_state_name(enum sde_hdcp_states hdcp_state);
struct sde_hdcp_ops *sde_hdmi_hdcp2p2_start(void *input);
#endif /* __SDE_HDCP_H__ */
