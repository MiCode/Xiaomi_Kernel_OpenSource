/* Copyright (c) 2010-2015, The Linux Foundation. All rights reserved.
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
#include "mdss_cec_abstract.h"

#define MAX_SWITCH_NAME_SIZE        5

enum hdmi_tx_io_type {
	HDMI_TX_CORE_IO,
	HDMI_TX_QFPROM_IO,
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
	/* bitfield representing each module's pin state */
	u64 pin_states;
};

struct hdmi_audio {
	int sample_rate;
	int channel_num;
	int spkr_alloc;
	int level_shift;
	int down_mix;
};

struct hdmi_tx_pinctrl {
	struct pinctrl *pinctrl;
	struct pinctrl_state *state_active;
	struct pinctrl_state *state_hpd_active;
	struct pinctrl_state *state_cec_active;
	struct pinctrl_state *state_ddc_active;
	struct pinctrl_state *state_suspend;
};

struct hdmi_tx_ctrl {
	struct platform_device *pdev;
	struct hdmi_tx_platform_data pdata;
	struct mdss_panel_data panel_data;
	struct mdss_util_intf *mdss_util;


	struct hdmi_tx_pinctrl pin_res;
	struct hdmi_audio audio_data;

	struct mutex mutex;
	struct mutex lut_lock;
	struct mutex power_mutex;
	struct mutex cable_notify_mutex;
	struct list_head cable_notify_handlers;
	struct kobject *kobj;
	struct switch_dev sdev;
	struct switch_dev audio_sdev;
	struct workqueue_struct *workq;
	spinlock_t hpd_state_lock;

	uint32_t video_resolution;

	u32 panel_power_on;
	u32 panel_suspend;

	u32 hpd_state;
	u32 hpd_off_pending;
	u32 hpd_feature_on;
	u32 hpd_initialized;
	u32 vote_hdmi_core_on;
	u8  timing_gen_on;
	u8  mhl_hpd_on;

	struct hdmi_util_ds_data ds_data;
	struct completion hpd_int_done;
	struct completion hpd_off_done;
	struct work_struct hpd_int_work;

	struct work_struct cable_notify_work;

	bool hdcp_feature_on;
	bool hpd_disabled;
	bool ds_registered;
	bool audio_ack_enabled;
	u32 present_hdcp;

	u8 spd_vendor_name[9];
	u8 spd_product_description[17];

	struct hdmi_tx_ddc_ctrl ddc_ctrl;

	void (*hdmi_tx_hpd_done) (void *data);
	void *downstream_data;

	void *feature_data[HDMI_TX_FEAT_MAX];
	u32 s3d_mode;
	atomic_t audio_ack_pending;

	struct cec_ops hdmi_cec_ops;
	struct cec_cbs hdmi_cec_cbs;

	u8 *edid_buf;
	u32 edid_buf_size;

	char disp_switch_name[MAX_SWITCH_NAME_SIZE];
};

#endif /* __MDSS_HDMI_TX_H__ */
