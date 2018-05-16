/* Copyright (c) 2012, 2014-2015, 2018, The Linux Foundation. All rights reserved.
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
#include <video/msm_hdmi_modes.h>
#include <soc/qcom/scm.h>

enum hdmi_hdcp_state {
	HDCP_STATE_INACTIVE,
	HDCP_STATE_AUTHENTICATING,
	HDCP_STATE_AUTHENTICATED,
	HDCP_STATE_AUTH_FAIL,
	HDCP_STATE_AUTH_ENC_NONE,
	HDCP_STATE_AUTH_ENC_1X,
	HDCP_STATE_AUTH_ENC_2P2
};

struct hdmi_hdcp_init_data {
	struct mdss_io_data *core_io;
	struct mdss_io_data *qfprom_io;
	struct mdss_io_data *hdcp_io;
	struct mutex *mutex;
	struct kobject *sysfs_kobj;
	struct workqueue_struct *workq;
	void *cb_data;
	void (*notify_status)(void *cb_data, enum hdmi_hdcp_state status);
	struct hdmi_tx_ddc_ctrl *ddc_ctrl;
	u32 phy_addr;
	u32 hdmi_tx_ver;
	struct msm_hdmi_mode_timing_info *timing;
	bool tethered;
};

struct hdmi_hdcp_ops {
	int (*hdmi_hdcp_isr)(void *ptr);
	int (*hdmi_hdcp_reauthenticate)(void *input);
	int (*hdmi_hdcp_authenticate)(void *hdcp_ctrl);
	bool (*feature_supported)(void *input);
	void (*hdmi_hdcp_off)(void *hdcp_ctrl);
};

void *hdmi_hdcp_init(struct hdmi_hdcp_init_data *init_data);
void *hdmi_hdcp2p2_init(struct hdmi_hdcp_init_data *init_data);
void hdmi_hdcp_deinit(void *input);
void hdmi_hdcp2p2_deinit(void *input);

struct hdmi_hdcp_ops *hdmi_hdcp_start(void *input);
struct hdmi_hdcp_ops *hdmi_hdcp2p2_start(void *input);

const char *hdcp_state_name(enum hdmi_hdcp_state hdcp_state);

#endif /* __MDSS_HDMI_HDCP_H__ */
