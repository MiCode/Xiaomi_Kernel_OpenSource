/* Copyright (c) 2011, Code Aurora Forum. All rights reserved.
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
#ifndef _MSM_PCM_ROUTING_H
#define _MSM_PCM_ROUTING_H
#include <sound/apr_audio.h>

#define LPASS_BE_PRI_I2S_RX "(Backend) PRIMARY_I2S_RX"
#define LPASS_BE_PRI_I2S_TX "(Backend) PRIMARY_I2S_TX"
#define LPASS_BE_SLIMBUS_0_RX "(Backend) SLIMBUS_0_RX"
#define LPASS_BE_SLIMBUS_0_TX "(Backend) SLIMBUS_0_TX"
#define LPASS_BE_HDMI "(Backend) HDMI"
#define LPASS_BE_INT_BT_SCO_RX "(Backend) INT_BT_SCO_RX"
#define LPASS_BE_INT_BT_SCO_TX "(Backend) INT_BT_SCO_TX"
#define LPASS_BE_INT_FM_RX "(Backend) INT_FM_RX"
#define LPASS_BE_INT_FM_TX "(Backend) INT_FM_TX"

enum {
	MSM_FRONTEND_DAI_MULTIMEDIA1 = 0,
	MSM_FRONTEND_DAI_MULTIMEDIA2,
	MSM_FRONTEND_DAI_CS_VOICE,
	MSM_FRONTEND_DAI_VOIP,
	MSM_FRONTEND_DAI_MULTIMEDIA3,
	MSM_FRONTEND_DAI_MAX,
};

enum {
	MSM_BACKEND_DAI_PRI_I2S_RX = 0,
	MSM_BACKEND_DAI_PRI_I2S_TX,
	MSM_BACKEND_DAI_SLIMBUS_0_RX,
	MSM_BACKEND_DAI_SLIMBUS_0_TX,
	MSM_BACKEND_DAI_HDMI_RX,
	MSM_BACKEND_DAI_INT_BT_SCO_RX,
	MSM_BACKEND_DAI_INT_BT_SCO_TX,
	MSM_BACKEND_DAI_INT_FM_RX,
	MSM_BACKEND_DAI_INT_FM_TX,
	MSM_BACKEND_DAI_MAX,
};

struct voice_mixer_data {
	u32 port_id;
	unsigned long dai_sessions;
	u32 mixer_type;
};

enum {
	VOICE_MIXER_PRI_I2S_RX = 0,
	VOICE_MIXER_SLIMBUS_0_RX,
	VOICE_MIXER_PRI_I2S_TX,
	VOICE_MIXER_SLIMBUS_0_TX,
	VOICE_MIXER_INT_BT_SCO_RX,
	VOICE_MIXER_INT_BT_SCO_TX,
	VOICE_MIXER_MAX,
};

enum {
	VOICE_PRI_I2S_RX = 0,
	VOICE_PRI_I2S_TX,
	VOICE_SLIMBUS_0_RX,
	VOICE_SLIMBUS_0_TX,
	VOICE_INT_BT_SCO_RX = 5,
	VOICE_INT_BT_SCO_TX,
};

/* dai_id: front-end ID,
 * dspst_id:  DSP audio stream ID
 * stream_type: playback or capture
 */
void msm_pcm_routing_reg_phy_stream(int fedai_id, int dspst_id,
	int stream_type);
void msm_pcm_routing_dereg_phy_stream(int fedai_id, int stream_type);

#endif /*_MSM_PCM_H*/
