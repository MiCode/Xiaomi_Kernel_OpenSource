/* Copyright (c) 2011-2012, The Linux Foundation. All rights reserved.
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

#ifndef __MSM_HDMI_AUDIO_H
#define __MSM_HDMI_AUDIO_H

/* Supported HDMI Audio channels */
#define MSM_HDMI_AUDIO_CHANNEL_2		0
#define MSM_HDMI_AUDIO_CHANNEL_4		1
#define MSM_HDMI_AUDIO_CHANNEL_6		2
#define MSM_HDMI_AUDIO_CHANNEL_8		3

#define TRUE   1
#define FALSE  0

enum hdmi_supported_sample_rates {
	HDMI_SAMPLE_RATE_32KHZ,
	HDMI_SAMPLE_RATE_44_1KHZ,
	HDMI_SAMPLE_RATE_48KHZ,
	HDMI_SAMPLE_RATE_88_2KHZ,
	HDMI_SAMPLE_RATE_96KHZ,
	HDMI_SAMPLE_RATE_176_4KHZ,
	HDMI_SAMPLE_RATE_192KHZ
};

int hdmi_audio_enable(bool on , u32 fifo_water_mark);
int hdmi_audio_packet_enable(bool on);
void hdmi_msm_audio_sample_rate_reset(int rate);
int hdmi_msm_audio_get_sample_rate(void);

#ifdef CONFIG_FB_MSM_HDMI_MSM_PANEL
int hdmi_msm_audio_info_setup(bool enabled, u32 num_of_channels,
	u32 channel_allocation, u32 level_shift, bool down_mix);
#else
static inline int hdmi_msm_audio_info_setup(bool enabled,
	u32 num_of_channels, u32 channel_allocation, u32 level_shift,
	bool down_mix)
{
	return 0;
}
#endif
#endif /* __MSM_HDMI_AUDIO_H*/
