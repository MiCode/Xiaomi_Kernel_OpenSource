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
#include "mdss_cec_core.h"
#include "mdss_hdmi_audio.h"

#define MAX_SWITCH_NAME_SIZE        5

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

enum hdmi_scan_info {
	HDMI_SCAN_DEFAULT,
	HDMI_SCAN_OVERSCAN,
	HDMI_SCAN_UNDERSCAN,
};

enum hdmi_colorimetry_info {
	HDMI_COLORIMETRY_SMPTE170M,
	HDMI_COLORIMETRY_ITUR_BT_709,
	HDMI_COLORIMETRY_ADOBE_RGB,
	HDMI_COLORIMETRY_ADOBE_YCC601,
	HDMI_COLORIMETRY_ITUR_BT_2020
};

enum hdmi_quantization_range {
	HDMI_QUANTIZATION_DEFAULT,
	HDMI_QUANTIZATION_LIMITED_RANGE,
	HDMI_QUANTIZATION_FULL_RANGE
};

enum hdmi_scaling_info {
	HDMI_SCALING_NONE,
	HDMI_SCALING_HORZ,
	HDMI_SCALING_VERT,
	HDMI_SCALING_HORZ_VERT,
};

enum hdmi_avi_content_type {
	HDMI_CONTENT_GRAPHICS,
	HDMI_CONTENT_PHOTO,
	HDMI_CONTENT_CINEMA,
	HDMI_CONTENT_GAME,
};

struct hdmi_avi_iframe_bar_info {
	bool vert_binfo_present;
	bool horz_binfo_present;
	u32 end_of_top_bar;
	u32 start_of_bottom_bar;
	u32 end_of_left_bar;
	u32 start_of_right_bar;
};

struct hdmi_avi_infoframe_config {
	u32 pixel_format;
	u32 scan_info;
	bool act_fmt_info_present;
	u32 colorimetry_info;
	u32 ext_colorimetry_info;
	u32 rgb_quantization_range;
	u32 yuv_quantization_range;
	u32 scaling_info;
	bool is_it_content;
	u8 content_type;
	u8 pixel_rpt_factor;
	struct hdmi_avi_iframe_bar_info bar_info;
};

struct hdmi_video_config {
	u32 vic;
	struct msm_hdmi_mode_timing_info timing;
	struct hdmi_avi_infoframe_config avi_iframe;
};

struct hdmi_tx_ctrl {
	struct platform_device *pdev;
	u32 hdmi_tx_ver;
	u32 max_pclk_khz;
	struct hdmi_tx_platform_data pdata;
	struct mdss_panel_data panel_data;
	struct mdss_util_intf *mdss_util;


	struct hdmi_tx_pinctrl pin_res;

	struct mutex mutex;
	struct mutex tx_lock;
	struct list_head cable_notify_handlers;
	struct kobject *kobj;
	struct switch_dev sdev;
	struct workqueue_struct *workq;
	spinlock_t hpd_state_lock;

	struct hdmi_video_config vid_cfg;

	u32 panel_power_on;
	u32 panel_suspend;

	u32 hpd_state;
	u32 hpd_off_pending;
	u32 hpd_feature_on;
	u32 hpd_initialized;
	u32 vote_hdmi_core_on;
	u8  timing_gen_on;
	u8  mhl_hpd_on;
	u8  hdcp_status;

	struct hdmi_util_ds_data ds_data;
	struct completion hpd_int_done;
	struct work_struct hpd_int_work;
	struct delayed_work hdcp_cb_work;

	struct work_struct cable_notify_work;

	bool hdcp_feature_on;
	bool hpd_disabled;
	bool ds_registered;
	bool scrambler_enabled;
	u32 hdcp14_present;
	bool hdcp1_use_sw_keys;
	bool hdcp14_sw_keys;
	bool auth_state;
	bool custom_edid;
	bool sim_mode;
	u32 enc_lvl;

	u8 spd_vendor_name[9];
	u8 spd_product_description[17];

	struct hdmi_tx_ddc_ctrl ddc_ctrl;

	void (*hdmi_tx_hpd_done) (void *data);
	void *downstream_data;
	void *audio_data;

	void *feature_data[HDMI_TX_FEAT_MAX];
	struct hdmi_hdcp_ops *hdcp_ops;
	void *hdcp_data;
	bool hdcp22_present;

	u8 *edid_buf;
	u32 edid_buf_size;
	u32 s3d_mode;

	struct cec_ops hdmi_cec_ops;
	struct cec_cbs hdmi_cec_cbs;
	struct hdmi_audio_ops audio_ops;
	struct msm_hdmi_audio_setup_params audio_params;

	char disp_switch_name[MAX_SWITCH_NAME_SIZE];
	bool power_data_enable[HDMI_TX_MAX_PM];
};

#endif /* __MDSS_HDMI_TX_H__ */
