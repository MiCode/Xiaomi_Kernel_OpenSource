/* Copyright (c) 2012-2013, The Linux Foundation. All rights reserved.
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

#ifndef __MSM_HDMI_AUDIO_CODEC_H__
#define __MSM_HDMI_AUDIO_CODEC_H__

#include <linux/device.h>
#include <linux/platform_device.h>

struct msm_hdmi_audio_edid_blk {
	u8 *audio_data_blk;
	unsigned int audio_data_blk_size; /* in bytes */
	u8 *spk_alloc_data_blk;
	unsigned int spk_alloc_data_blk_size; /* in bytes */
};

struct msm_hdmi_audio_codec_ops {
	int (*audio_info_setup)(struct platform_device *pdev,
		u32 sample_rate, u32 num_of_channels,
		u32 channel_allocation, u32 level_shift,
		bool down_mix);
	int (*get_audio_edid_blk) (struct platform_device *pdev,
		struct msm_hdmi_audio_edid_blk *blk);
	int (*hdmi_cable_status) (struct platform_device *pdev, u32 vote);
};

int msm_hdmi_register_audio_codec(struct platform_device *pdev,
	struct msm_hdmi_audio_codec_ops *ops);

#endif /* __MSM_HDMI_AUDIO_CODEC_H__ */
