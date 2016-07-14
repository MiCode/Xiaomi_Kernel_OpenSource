/* Copyright (c) 2010-2016, The Linux Foundation. All rights reserved.
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

#ifndef __MDSS_HDMI_TX_H__
#define __MDSS_HDMI_TX_H__

#include <linux/switch.h>
#include "mdss_hdmi_util.h"
#include "mdss_hdmi_panel.h"
#include "mdss_cec_core.h"
#include "mdss_hdmi_audio.h"

#define MAX_SWITCH_NAME_SIZE        5
#define HDR_PRIMARIES_COUNT	3

enum hdmi_tx_io_type {
	HDMI_TX_CORE_IO,
	HDMI_TX_QFPROM_IO,
	HDMI_TX_HDCP_IO,
	HDMI_TX_MAX_IO
};

enum hdmi_tx_power_module_type {
	HDMI_TX_HPD_PM,
	HDMI_TX_DDC_PM,
	HDMI_TX_CORE_PM,
	HDMI_TX_CEC_PM,
	HDMI_TX_MAX_PM
};

/* Data filled from device tree */
struct hdmi_tx_platform_data {
	bool primary;
	bool cont_splash_enabled;
	bool cond_power_on;
	struct dss_io_data io[HDMI_TX_MAX_IO];
	struct dss_module_power power_data[HDMI_TX_MAX_PM];
	struct reg_bus_client *reg_bus_clt[HDMI_TX_MAX_PM];
	/* bitfield representing each module's pin state */
	u64 pin_states;
	bool pluggable;
};

struct hdmi_tx_pinctrl {
	struct pinctrl *pinctrl;
	struct pinctrl_state *state_active;
	struct pinctrl_state *state_hpd_active;
	struct pinctrl_state *state_cec_active;
	struct pinctrl_state *state_ddc_active;
	struct pinctrl_state *state_suspend;
};

struct hdmi_tx_ctrl;
typedef int (*hdmi_tx_evt_handler) (struct hdmi_tx_ctrl *);

/*
 * struct hdmi_tx_hdr_stream - HDR video stream characteristics
 * @eotf: Electro-Optical Transfer Function
 * @display_primaries_x: display primaries data for x-coordinate
 * @display_primaries_y: display primaries data for y-coordinate
 * @white_point_x: white point data for x-coordinate
 * @white_point_y: white point data for y-coordinate
 * @max_luminance: content maximum luminance
 * @min_luminance: content minimum luminance
 * @max_content_light_level: content maximum light level
 * @max_average_light_level: content average light level
 */
struct hdmi_tx_hdr_stream_data {
	u32 eotf;
	u32 display_primaries_x[HDR_PRIMARIES_COUNT];
	u32 display_primaries_y[HDR_PRIMARIES_COUNT];
	u32 white_point_x;
	u32 white_point_y;
	u32 max_luminance;
	u32 min_luminance;
	u32 max_content_light_level;
	u32 max_average_light_level;
};

struct hdmi_tx_ctrl {
	struct platform_device *pdev;
	struct hdmi_tx_platform_data pdata;
	struct mdss_panel_data panel_data;
	struct mdss_util_intf *mdss_util;
	struct msm_hdmi_mode_timing_info timing;
	struct hdmi_tx_pinctrl pin_res;
	struct mutex mutex;
	struct mutex tx_lock;
	struct list_head cable_notify_handlers;
	struct kobject *kobj;
	struct switch_dev sdev;
	struct workqueue_struct *workq;
	struct hdmi_util_ds_data ds_data;
	struct completion hpd_int_done;
	struct work_struct hpd_int_work;
	struct delayed_work hdcp_cb_work;
	struct work_struct cable_notify_work;
	struct hdmi_tx_ddc_ctrl ddc_ctrl;
	struct hdmi_hdcp_ops *hdcp_ops;
	struct cec_ops hdmi_cec_ops;
	struct cec_cbs hdmi_cec_cbs;
	struct hdmi_audio_ops audio_ops;
	struct hdmi_panel_data panel;
	struct hdmi_panel_ops panel_ops;
	struct msm_ext_disp_audio_setup_params audio_params;
	struct work_struct fps_work;
	struct hdmi_tx_hdr_stream_data hdr_data;

	spinlock_t hpd_state_lock;

	u32 panel_power_on;
	u32 panel_suspend;
	u32 vic;
	u32 hdmi_tx_version;
	u32 hdmi_tx_major_version;
	u32 max_pclk_khz;
	u32 hpd_state;
	u32 hpd_off_pending;
	u32 hpd_feature_on;
	u32 hpd_initialized;
	u32 vote_hdmi_core_on;
	u32 dynamic_fps;
	u32 hdcp14_present;
	u32 enc_lvl;
	u32 edid_buf_size;
	u32 s3d_mode;

	u8 timing_gen_on;
	u8 mhl_hpd_on;
	u8 hdcp_status;
	u8 spd_vendor_name[9];
	u8 spd_product_description[17];

	bool hdcp_feature_on;
	bool hpd_disabled;
	bool ds_registered;
	bool scrambler_enabled;
	bool hdcp1_use_sw_keys;
	bool hdcp14_sw_keys;
	bool auth_state;
	bool custom_edid;
	bool sim_mode;
	bool hdcp22_present;
	bool power_data_enable[HDMI_TX_MAX_PM];
	bool dc_support;
	bool dc_feature_on;

	void (*hdmi_tx_hpd_done)(void *data);
	void *downstream_data;
	void *audio_data;
	void *feature_data[hweight8(HDMI_TX_FEAT_MAX)];
	void *hdcp_data;
	void *evt_arg;
	u8 *edid_buf;

	char disp_switch_name[MAX_SWITCH_NAME_SIZE];

	hdmi_tx_evt_handler evt_handler[MDSS_EVENT_MAX - 1];
};

#endif /* __MDSS_HDMI_TX_H__ */
