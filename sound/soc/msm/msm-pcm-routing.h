/* Copyright (c) 2011-2013, The Linux Foundation. All rights reserved.
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

#define LPASS_BE_PRI_I2S_RX "PRIMARY_I2S_RX"
#define LPASS_BE_PRI_I2S_TX "PRIMARY_I2S_TX"
#define LPASS_BE_SLIMBUS_0_RX "SLIMBUS_0_RX"
#define LPASS_BE_SLIMBUS_0_TX "SLIMBUS_0_TX"
#define LPASS_BE_HDMI "HDMI"
#define LPASS_BE_PSEUDO "PSEUDO"
#define LPASS_BE_INT_BT_SCO_RX "INT_BT_SCO_RX"
#define LPASS_BE_INT_BT_SCO_TX "INT_BT_SCO_TX"
#define LPASS_BE_INT_FM_RX "INT_FM_RX"
#define LPASS_BE_INT_FM_TX "INT_FM_TX"
#define LPASS_BE_AFE_PCM_RX "RT_PROXY_DAI_001_RX"
#define LPASS_BE_AFE_PCM_TX "RT_PROXY_DAI_002_TX"
#define LPASS_BE_AUXPCM_RX "AUX_PCM_RX"
#define LPASS_BE_AUXPCM_TX "AUX_PCM_TX"
#define LPASS_BE_SEC_AUXPCM_RX "SEC_AUX_PCM_RX"
#define LPASS_BE_SEC_AUXPCM_TX "SEC_AUX_PCM_TX"
#define LPASS_BE_VOICE_PLAYBACK_TX "VOICE_PLAYBACK_TX"
#define LPASS_BE_INCALL_RECORD_RX "INCALL_RECORD_TX"
#define LPASS_BE_INCALL_RECORD_TX "INCALL_RECORD_RX"
#define LPASS_BE_SEC_I2S_RX "SECONDARY_I2S_RX"
#define LPASS_BE_SEC_I2S_TX "SECONDARY_I2S_TX"

#define LPASS_BE_MI2S_RX "MI2S_RX"
#define LPASS_BE_MI2S_TX "MI2S_TX"
#define LPASS_BE_STUB_RX "STUB_RX"
#define LPASS_BE_STUB_TX "STUB_TX"
#define LPASS_BE_SLIMBUS_1_RX "SLIMBUS_1_RX"
#define LPASS_BE_SLIMBUS_1_TX "SLIMBUS_1_TX"
#define LPASS_BE_STUB_1_TX "STUB_1_TX"
#define LPASS_BE_SLIMBUS_3_RX "SLIMBUS_3_RX"
#define LPASS_BE_SLIMBUS_3_TX "SLIMBUS_3_TX"
#define LPASS_BE_SLIMBUS_4_RX "SLIMBUS_4_RX"
#define LPASS_BE_SLIMBUS_4_TX "SLIMBUS_4_TX"

/* For multimedia front-ends, asm session is allocated dynamically.
 * Hence, asm session/multimedia front-end mapping has to be maintained.
 * Due to this reason, additional multimedia front-end must be placed before
 * non-multimedia front-ends.
 */

enum {
	MSM_FRONTEND_DAI_MULTIMEDIA1 = 0,
	MSM_FRONTEND_DAI_MULTIMEDIA2,
	MSM_FRONTEND_DAI_MULTIMEDIA3,
	MSM_FRONTEND_DAI_MULTIMEDIA4,
	MSM_FRONTEND_DAI_MULTIMEDIA5,
	MSM_FRONTEND_DAI_MULTIMEDIA6,
	MSM_FRONTEND_DAI_MULTIMEDIA7,
	MSM_FRONTEND_DAI_MULTIMEDIA8,
	MSM_FRONTEND_DAI_PSEUDO,
	MSM_FRONTEND_DAI_CS_VOICE,
	MSM_FRONTEND_DAI_VOIP,
	MSM_FRONTEND_DAI_AFE_RX,
	MSM_FRONTEND_DAI_AFE_TX,
	MSM_FRONTEND_DAI_VOICE_STUB,
	MSM_FRONTEND_DAI_VOLTE,
	MSM_FRONTEND_DAI_VOICE2,
	MSM_FRONTEND_DAI_DTMF_RX,
	MSM_FRONTEND_DAI_VOLTE_STUB,
	MSM_FRONTEND_DAI_VOICE2_STUB,
	MSM_FRONTEND_DAI_MAX,
};

#define MSM_FRONTEND_DAI_MM_SIZE (MSM_FRONTEND_DAI_PSEUDO + 1)
#define MSM_FRONTEND_DAI_MM_MAX_ID MSM_FRONTEND_DAI_PSEUDO

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
	MSM_BACKEND_DAI_AFE_PCM_RX,
	MSM_BACKEND_DAI_AFE_PCM_TX,
	MSM_BACKEND_DAI_AUXPCM_RX,
	MSM_BACKEND_DAI_AUXPCM_TX,
	MSM_BACKEND_DAI_VOICE_PLAYBACK_TX,
	MSM_BACKEND_DAI_INCALL_RECORD_RX,
	MSM_BACKEND_DAI_INCALL_RECORD_TX,
	MSM_BACKEND_DAI_MI2S_RX,
	MSM_BACKEND_DAI_MI2S_TX,
	MSM_BACKEND_DAI_SEC_I2S_RX,
	MSM_BACKEND_DAI_SEC_I2S_TX,
	MSM_BACKEND_DAI_SLIMBUS_1_RX,
	MSM_BACKEND_DAI_SLIMBUS_1_TX,
	MSM_BACKEND_DAI_SLIMBUS_4_RX,
	MSM_BACKEND_DAI_SLIMBUS_4_TX,
	MSM_BACKEND_DAI_SLIMBUS_3_RX,
	MSM_BACKEND_DAI_SLIMBUS_3_TX,
	MSM_BACKEND_DAI_EXTPROC_RX,
	MSM_BACKEND_DAI_EXTPROC_TX,
	MSM_BACKEND_DAI_EXTPROC_EC_TX,
	MSM_BACKEND_DAI_SEC_AUXPCM_RX,
	MSM_BACKEND_DAI_SEC_AUXPCM_TX,
	MSM_BACKEND_DAI_PSEUDO_PORT,
	MSM_BACKEND_DAI_MAX,
};

enum msm_pcm_routing_event {
	MSM_PCM_RT_EVT_BUF_RECFG,
	MSM_PCM_RT_EVT_DEVSWITCH,
	MSM_PCM_RT_EVT_MAX,
};
/* dai_id: front-end ID,
 * dspst_id:  DSP audio stream ID
 * stream_type: playback or capture
 */
void msm_pcm_routing_reg_phy_stream(int fedai_id, bool perf_mode,
				int dspst_id, int stream_type);
void msm_pcm_routing_reg_psthr_stream(int fedai_id, int dspst_id,
		int stream_type, int enable);

void msm_pcm_routing_reg_pseudo_stream(int fedai_id, bool perf_mode,
				int dspst_id, int stream_type, int sample_rate,
				int channels);

void msm_pcm_routing_dereg_pseudo_stream(int fedai_id, int dspst_id);

struct msm_pcm_routing_evt {
	void (*event_func)(enum msm_pcm_routing_event, void *);
	void *priv_data;
};

void msm_pcm_routing_reg_phy_stream_v2(int fedai_id, bool perf_mode,
				       int dspst_id, int stream_type,
				       struct msm_pcm_routing_evt event_info);

void msm_pcm_routing_dereg_phy_stream(int fedai_id, int stream_type);

int lpa_set_volume(unsigned volume);

int msm_routing_check_backend_enabled(int fedai_id);

int multi_ch_pcm_set_volume(unsigned volume);

int compressed_set_volume(unsigned volume);

void multi_ch_pcm_set_channel_map(char *channel_mapping);

#endif /*_MSM_PCM_H*/
