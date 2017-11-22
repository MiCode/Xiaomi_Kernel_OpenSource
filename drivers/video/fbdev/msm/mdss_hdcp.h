/* Copyright (c) 2012, 2014-2017, The Linux Foundation. All rights reserved.
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

#ifndef __MDSS_HDMI_HDCP_H__
#define __MDSS_HDMI_HDCP_H__

#include "mdss_hdmi_util.h"
#include "mdss_dp.h"
#include <video/msm_hdmi_modes.h>
#include <soc/qcom/scm.h>

#define HDCP_SRM_CHECK_FAIL 29

enum hdcp_client_id {
	HDCP_CLIENT_HDMI,
	HDCP_CLIENT_DP,
};

enum hdcp_states {
	HDCP_STATE_INACTIVE,
	HDCP_STATE_AUTHENTICATING,
	HDCP_STATE_AUTHENTICATED,
	HDCP_STATE_AUTH_FAIL,
	HDCP_STATE_AUTH_FAIL_NOREAUTH,
	HDCP_STATE_AUTH_ENC_NONE,
	HDCP_STATE_AUTH_ENC_1X,
	HDCP_STATE_AUTH_ENC_2P2
};

struct hdcp_init_data {
	struct dss_io_data *core_io;
	struct dss_io_data *qfprom_io;
	struct dss_io_data *hdcp_io;
	struct mutex *mutex;
	struct kobject *sysfs_kobj;
	struct workqueue_struct *workq;
	void *cb_data;
	void (*notify_status)(void *cb_data, enum hdcp_states status);
	struct hdmi_tx_ddc_ctrl *ddc_ctrl;
	u8 sink_rx_status;
	u16 *version;
	void *dp_data;
	u32 phy_addr;
	u32 hdmi_tx_ver;
	struct msm_hdmi_mode_timing_info *timing;
	bool tethered;
	bool sec_access;
	enum hdcp_client_id client_id;
};

struct hdcp_ops {
	int (*isr)(void *ptr);
	int (*cp_irq)(void *ptr);
	int (*reauthenticate)(void *input);
	int (*authenticate)(void *hdcp_ctrl);
	bool (*feature_supported)(void *input);
	void (*off)(void *hdcp_ctrl);
};

void *hdcp_1x_init(struct hdcp_init_data *init_data);
void hdcp_1x_deinit(void *input);

void *hdmi_hdcp2p2_init(struct hdcp_init_data *init_data);
void hdmi_hdcp2p2_deinit(void *input);

void *dp_hdcp2p2_init(struct hdcp_init_data *init_data);
void dp_hdcp2p2_deinit(void *input);

struct hdcp_ops *hdcp_1x_start(void *input);
struct hdcp_ops *hdmi_hdcp2p2_start(void *input);
struct hdcp_ops *dp_hdcp2p2_start(void *input);

const char *hdcp_state_name(enum hdcp_states hdcp_state);

#endif /* __MDSS_HDMI_HDCP_H__ */
