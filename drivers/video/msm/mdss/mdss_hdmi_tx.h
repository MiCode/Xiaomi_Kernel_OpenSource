/* Copyright (c) 2010-2012, Code Aurora Forum. All rights reserved.
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
#include "mdss_io_util.h"

enum hdmi_tx_clk_type {
	HDMI_TX_AHB_CLK,
	HDMI_TX_APP_CLK,
	HDMI_TX_EXTP_CLK,
	HDMI_TX_MAX_CLK
};

enum hdmi_tx_io_type {
	HDMI_TX_CORE_IO,
	HDMI_TX_PHY_IO,
	HDMI_TX_QFPROM_IO,
	HDMI_TX_MAX_IO
};

enum hdmi_tx_power_module_type {
	HDMI_TX_HPD_PM,
	HDMI_TX_CORE_PM,
	HDMI_TX_CEC_PM,
	HDMI_TX_MAX_PM
};

struct hdmi_tx_platform_data {
	/* Data filled from device tree nodes */
	struct dss_io_data io[HDMI_TX_MAX_IO];
	struct dss_module_power power_data[HDMI_TX_MAX_PM];

	/* clk and regulator handles */
	struct clk *clk[HDMI_TX_MAX_CLK];
};

struct hdmi_tx_ctrl {
	struct platform_device *pdev;
	struct hdmi_tx_platform_data pdata;
	struct mdss_panel_data panel_data;

	struct mutex mutex;
	struct kobject *kobj;
	struct switch_dev sdev;
	struct workqueue_struct *workq;

	uint32_t video_resolution;
	u32 panel_power_on;

	u32 hpd_initialized;
	int hpd_stable;
	u32 hpd_prev_state;
	u32 hpd_cable_chg_detected;
	u32 hpd_state;
	u32 hpd_feature_on;
	struct work_struct hpd_state_work;
	struct timer_list hpd_state_timer;

	unsigned long pixel_clk;
	u32 xres;
	u32 yres;
	u32 frame_rate;

	u32 present_hdcp;

	u8 spd_vendor_name[8];
	u8 spd_product_description[16];

	struct hdmi_tx_ddc_ctrl ddc_ctrl;

	void *feature_data[HDMI_TX_FEAT_MAX];
};

#endif /* __MDSS_HDMI_TX_H__ */
