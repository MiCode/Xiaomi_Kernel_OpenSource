/* Copyright (c) 2010-2017, The Linux Foundation. All rights reserved.
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

#ifndef __HDMI_EDID_H__
#define __HDMI_EDID_H__

#include <linux/msm_ext_display.h>
#include "mdss_hdmi_util.h"

#define EDID_BLOCK_SIZE 0x80
#define EDID_BLOCK_ADDR 0xA0
#define MAX_EDID_BLOCKS 5

struct hdmi_edid_init_data {
	struct kobject *kobj;
	struct hdmi_util_ds_data ds_data;
	u32 max_pclk_khz;
	u8 *buf;
	u32 buf_size;
};

/*
 * struct hdmi_edid_hdr_data - HDR Static Metadata
 * @eotf: Electro-Optical Transfer Function
 * @metadata_type_one: Static Metadata Type 1 support
 * @max_luminance: Desired Content Maximum Luminance
 * @avg_luminance: Desired Content Frame-average Luminance
 * @min_luminance: Desired Content Minimum Luminance
 */
struct hdmi_edid_hdr_data {
	u32 eotf;
	bool metadata_type_one;
	u32 max_luminance;
	u32 avg_luminance;
	u32 min_luminance;
};

/*
 * struct hdmi_override_data - Resolution Override Data
 * @scramble - scrambler enable
 * @sink_mode - 0 for DVI and 1 for HDMI
 * @format - pixel format (refer to msm_hdmi_modes.h)
 * @vic - resolution code
 */
struct hdmi_edid_override_data {
	int scramble;
	int sink_mode;
	int format;
	int vic;
};

enum edid_sink_mode {
	SINK_MODE_DVI,
	SINK_MODE_HDMI
};

int hdmi_edid_parser(void *edid_ctrl);
u32 hdmi_edid_get_raw_data(void *edid_ctrl, u8 *buf, u32 size);
u8 hdmi_edid_get_sink_scaninfo(void *edid_ctrl, u32 resolution);
bool hdmi_edid_is_dvi_mode(void *input);
u32 hdmi_edid_get_sink_mode(void *edid_ctrl, u32 mode);
bool hdmi_edid_sink_scramble_override(void *input);
bool hdmi_edid_get_sink_scrambler_support(void *input);
bool hdmi_edid_get_scdc_support(void *input);
int hdmi_edid_get_audio_blk(void *edid_ctrl,
	struct msm_ext_disp_audio_edid_blk *blk);
void hdmi_edid_set_video_resolution(void *edid_ctrl, u32 resolution,
	bool reset);
void hdmi_edid_deinit(void *edid_ctrl);
void *hdmi_edid_init(struct hdmi_edid_init_data *init_data);
bool hdmi_edid_is_s3d_mode_supported(void *input,
	u32 video_mode, u32 s3d_mode);
u8 hdmi_edid_get_deep_color(void *edid_ctrl);
u32 hdmi_edid_get_max_pclk(void *edid_ctrl);
void hdmi_edid_get_hdr_data(void *edid_ctrl,
		struct hdmi_edid_hdr_data **hdr_data);
void hdmi_edid_config_override(void *input, bool enable,
		struct hdmi_edid_override_data *data);
void hdmi_edid_set_max_pclk_rate(void *input, u32 max_pclk_khz);
bool hdmi_edid_is_audio_supported(void *input);
u32 hdmi_edid_get_sink_caps_max_tmds_clk(void *input);

#endif /* __HDMI_EDID_H__ */
