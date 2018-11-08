/* Copyright (c) 2016, 2018, The Linux Foundation. All rights reserved.
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

#ifndef __MDSS_HDMI_PANEL_H__
#define __MDSS_HDMI_PANEL_H__

#include "mdss_panel.h"
#include "mdss_hdmi_util.h"

/**
 * struct hdmi_panel_data - panel related data information
 *
 * @pinfo: pointer to mdss panel information
 * @s3d_mode: 3d mode supported
 * @vic: video indentification code
 * @scan_info: scan information of the TV
 * @s3d_support: set to true if 3d supported, false otherwize
 * @content_type: type of content like game, cinema etc
 * @infoframe: set to true if infoframes should be sent to sink
 * @is_it_content: set to true if content is IT
 * @scrambler: set to true if scrambler needs to be enabled
 * @dc_enable: set to true if deep color is enabled
 */
struct hdmi_panel_data {
	struct mdss_panel_info *pinfo;
	u32 s3d_mode;
	u32 vic;
	u32 scan_info;
	u8 content_type;
	bool s3d_support;
	bool infoframe;
	bool is_it_content;
	bool scrambler;
	bool dc_enable;
};

/**
 * struct hdmi_panel_ops - panel operation for clients
 *
 * @on: pointer to a function which powers on the panel
 * @off: pointer to a function which powers off the panel
 * @vendor: pointer to a function which programs vendor specific infoframe
 * @update_fps: pointer to a function which updates fps
 */
struct hdmi_panel_ops {
	int (*on)(void *input);
	int (*off)(void *input);
	void (*vendor)(void *input);
	int (*update_fps)(void *input, u32 fps);
};

/**
 * struct hdmi_panel_init_data - initialization data for hdmi panel
 *
 * @io: pointer to logical memory of the hdmi tx core
 * @ds_data: pointer to down stream data
 * @panel_data: pointer to panel data
 * @ddc: pointer to display data channel's data
 * @ops: pointer to pnael ops to be filled by hdmi panel
 * @timing: pointer to the timing details of current resolution
 * @spd_vendor_name: pointer to spd vendor infoframe data
 * @spd_product_description:  pointer to spd product description infoframe data
 * @version:  hardware version of the hdmi tx
 */
struct hdmi_panel_init_data {
	struct dss_io_data *io;
	struct hdmi_util_ds_data *ds_data;
	struct hdmi_panel_data *panel_data;
	struct hdmi_tx_ddc_ctrl *ddc;
	struct hdmi_panel_ops *ops;
	struct msm_hdmi_mode_timing_info *timing;
	u8 *spd_vendor_name;
	u8 *spd_product_description;
	u32 version;
};

/**
 * hdmi_panel_init() - initializes hdmi panel
 *
 * initializes the hdmi panel, allocates the memory, assign the input
 * data to local variables and provide the operation function pointers.
 *
 * @data: initialization data.
 * return: hdmi panel data that need to be send with hdmi ops.
 */
void *hdmi_panel_init(struct hdmi_panel_init_data *data);

/**
 * hdmi_panel_deinit() - deinitializes hdmi panel
 *
 * releases memory and all resources.
 *
 * @input: hdmi panel data.
 */
void hdmi_panel_deinit(void *input);

int hdmi_panel_get_vic(struct mdss_panel_info *pinfo,
				struct hdmi_util_ds_data *ds_data);

#endif /* __MDSS_HDMI_PANEL_H__ */
