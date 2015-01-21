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

#ifndef __HDMI_EDID_H__
#define __HDMI_EDID_H__

#include <linux/msm_hdmi.h>
#include "mdss_hdmi_util.h"

#define EDID_BLOCK_SIZE 0x80
#define EDID_BLOCK_ADDR 0xA0
#define MAX_EDID_BLOCKS 5

struct hdmi_edid_init_data {
	struct kobject *kobj;
	struct hdmi_util_ds_data *ds_data;
	u32 max_pclk_khz;
	u8 *buf;
	u32 buf_size;
};

int hdmi_edid_parser(void *edid_ctrl);
u32 hdmi_edid_get_raw_data(void *edid_ctrl, u8 *buf, u32 size);
u8 hdmi_edid_get_sink_scaninfo(void *edid_ctrl, u32 resolution);
u32 hdmi_edid_get_sink_mode(void *edid_ctrl);
bool hdmi_edid_get_sink_scrambler_support(void *input);
bool hdmi_edid_get_scdc_support(void *input);
int hdmi_edid_get_audio_blk(void *edid_ctrl,
	struct msm_hdmi_audio_edid_blk *blk);
void hdmi_edid_set_video_resolution(void *edid_ctrl, u32 resolution,
	bool reset);
void hdmi_edid_deinit(void *edid_ctrl);
void *hdmi_edid_init(struct hdmi_edid_init_data *init_data);
bool hdmi_edid_is_s3d_mode_supported(void *input,
	u32 video_mode, u32 s3d_mode);

#endif /* __HDMI_EDID_H__ */
