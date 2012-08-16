/* arch/arm/mach-msm/qdsp5/audmgr.h
 *
 * Copyright (C) 2008 Google, Inc.
 * Copyright (c) 2008-2009, 2012 The Linux Foundation. All rights reserved.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef _AUDIO_RPC_H_
#define _AUDIO_RPC_H_

#include <mach/qdsp5/qdsp5audppcmdi.h>

enum rpc_aud_def_sample_rate_type {
	RPC_AUD_DEF_SAMPLE_RATE_NONE,
	RPC_AUD_DEF_SAMPLE_RATE_8000,
	RPC_AUD_DEF_SAMPLE_RATE_11025,
	RPC_AUD_DEF_SAMPLE_RATE_12000,
	RPC_AUD_DEF_SAMPLE_RATE_16000,
	RPC_AUD_DEF_SAMPLE_RATE_22050,
	RPC_AUD_DEF_SAMPLE_RATE_24000,
	RPC_AUD_DEF_SAMPLE_RATE_32000,
	RPC_AUD_DEF_SAMPLE_RATE_44100,
	RPC_AUD_DEF_SAMPLE_RATE_48000,
	RPC_AUD_DEF_SAMPLE_RATE_MAX,
};

enum rpc_aud_def_method_type {
	RPC_AUD_DEF_METHOD_NONE,
	RPC_AUD_DEF_METHOD_KEY_BEEP,
	RPC_AUD_DEF_METHOD_PLAYBACK,
	RPC_AUD_DEF_METHOD_VOICE,
	RPC_AUD_DEF_METHOD_RECORD,
	RPC_AUD_DEF_METHOD_HOST_PCM,
	RPC_AUD_DEF_METHOD_MIDI_OUT,
	RPC_AUD_DEF_METHOD_RECORD_SBC,
	RPC_AUD_DEF_METHOD_DTMF_RINGER,
	RPC_AUD_DEF_METHOD_MAX,
};

enum rpc_aud_def_codec_type {
	RPC_AUD_DEF_CODEC_NONE,
	RPC_AUD_DEF_CODEC_DTMF,
	RPC_AUD_DEF_CODEC_MIDI,
	RPC_AUD_DEF_CODEC_MP3,
	RPC_AUD_DEF_CODEC_PCM,
	RPC_AUD_DEF_CODEC_AAC,
	RPC_AUD_DEF_CODEC_WMA,
	RPC_AUD_DEF_CODEC_RA,
	RPC_AUD_DEF_CODEC_ADPCM,
	RPC_AUD_DEF_CODEC_GAUDIO,
	RPC_AUD_DEF_CODEC_VOC_EVRC,
	RPC_AUD_DEF_CODEC_VOC_13K,
	RPC_AUD_DEF_CODEC_VOC_4GV_NB,
	RPC_AUD_DEF_CODEC_VOC_AMR,
	RPC_AUD_DEF_CODEC_VOC_EFR,
	RPC_AUD_DEF_CODEC_VOC_FR,
	RPC_AUD_DEF_CODEC_VOC_HR,
	RPC_AUD_DEF_CODEC_VOC_CDMA,
	RPC_AUD_DEF_CODEC_VOC_CDMA_WB,
	RPC_AUD_DEF_CODEC_VOC_UMTS,
	RPC_AUD_DEF_CODEC_VOC_UMTS_WB,
	RPC_AUD_DEF_CODEC_SBC,
	RPC_AUD_DEF_CODEC_VOC_PCM,
	RPC_AUD_DEF_CODEC_AMR_WB,
	RPC_AUD_DEF_CODEC_AMR_WB_PLUS,
	RPC_AUD_DEF_CODEC_AAC_BSAC,
	RPC_AUD_DEF_CODEC_MAX,
	RPC_AUD_DEF_CODEC_AMR_NB,
	RPC_AUD_DEF_CODEC_13K,
	RPC_AUD_DEF_CODEC_EVRC,
	RPC_AUD_DEF_CODEC_AC3,
	RPC_AUD_DEF_CODEC_MAX_002,
};

enum rpc_snd_method_type {
	RPC_SND_METHOD_VOICE = 0,
	RPC_SND_METHOD_KEY_BEEP,
	RPC_SND_METHOD_MESSAGE,
	RPC_SND_METHOD_RING,
	RPC_SND_METHOD_MIDI,
	RPC_SND_METHOD_AUX,
	RPC_SND_METHOD_MAX,
};

enum rpc_voc_codec_type {
	RPC_VOC_CODEC_DEFAULT,
	RPC_VOC_CODEC_ON_CHIP_0 = RPC_VOC_CODEC_DEFAULT,
	RPC_VOC_CODEC_ON_CHIP_1,
	RPC_VOC_CODEC_STEREO_HEADSET,
	RPC_VOC_CODEC_ON_CHIP_AUX,
	RPC_VOC_CODEC_BT_OFF_BOARD,
	RPC_VOC_CODEC_BT_A2DP,
	RPC_VOC_CODEC_OFF_BOARD,
	RPC_VOC_CODEC_SDAC,
	RPC_VOC_CODEC_RX_EXT_SDAC_TX_INTERNAL,
	RPC_VOC_CODEC_IN_STEREO_SADC_OUT_MONO_HANDSET,
	RPC_VOC_CODEC_IN_STEREO_SADC_OUT_STEREO_HEADSET,
	RPC_VOC_CODEC_TX_INT_SADC_RX_EXT_AUXPCM,
	RPC_VOC_CODEC_EXT_STEREO_SADC_OUT_MONO_HANDSET,
	RPC_VOC_CODEC_EXT_STEREO_SADC_OUT_STEREO_HEADSET,
	RPC_VOC_CODEC_TTY_ON_CHIP_1,
	RPC_VOC_CODEC_TTY_OFF_BOARD,
	RPC_VOC_CODEC_TTY_VCO,
	RPC_VOC_CODEC_TTY_HCO,
	RPC_VOC_CODEC_ON_CHIP_0_DUAL_MIC,
	RPC_VOC_CODEC_MAX,
	RPC_VOC_CODEC_NONE,
};

enum rpc_audmgr_status_type {
	RPC_AUDMGR_STATUS_READY,
	RPC_AUDMGR_STATUS_CODEC_CONFIG,
	RPC_AUDMGR_STATUS_PENDING,
	RPC_AUDMGR_STATUS_SUSPEND,
	RPC_AUDMGR_STATUS_FAILURE,
	RPC_AUDMGR_STATUS_VOLUME_CHANGE,
	RPC_AUDMGR_STATUS_DISABLED,
	RPC_AUDMGR_STATUS_ERROR,
};

struct rpc_audmgr_enable_client_args {
	uint32_t set_to_one;
	uint32_t tx_sample_rate;
	uint32_t rx_sample_rate;
	uint32_t def_method;
	uint32_t codec_type;
	uint32_t snd_method;

	uint32_t cb_func;
	uint32_t client_data;
};
	
#define AUDMGR_ENABLE_CLIENT			2
#define AUDMGR_DISABLE_CLIENT			3
#define AUDMGR_SUSPEND_EVENT_RSP		4
#define AUDMGR_REGISTER_OPERATION_LISTENER	5
#define AUDMGR_UNREGISTER_OPERATION_LISTENER	6
#define AUDMGR_REGISTER_CODEC_LISTENER		7
#define AUDMGR_GET_RX_SAMPLE_RATE		8
#define AUDMGR_GET_TX_SAMPLE_RATE		9
#define AUDMGR_SET_DEVICE_MODE			10

#define AUDMGR_PROG_VERS "rs30000013:0x7feccbff"
#define AUDMGR_PROG 0x30000013
#define AUDMGR_VERS 0x7feccbff
#define AUDMGR_VERS_COMP 0x00010001
#define AUDMGR_VERS_COMP_VER2 0x00020001
#define AUDMGR_VERS_COMP_VER3 0x00030001

struct rpc_audmgr_cb_func_ptr {
	uint32_t cb_id; /* cb_func */
	uint32_t status; /* Audmgr status */
	uint32_t set_to_one;  /* Pointer status (1 = valid, 0  = invalid) */
	uint32_t disc;
	/* disc = AUDMGR_STATUS_READY => data=handle
	   disc = AUDMGR_STATUS_CODEC_CONFIG => data = volume
	   disc = AUDMGR_STATUS_DISABLED => data =status_disabled
	   disc = AUDMGR_STATUS_VOLUME_CHANGE => data = volume_change */
	union {
		uint32_t handle;
		uint32_t volume;
		uint32_t status_disabled;
		uint32_t volume_change;
	} u;
	uint32_t client_data;
};

#define AUDMGR_CB_FUNC_PTR			1
#define AUDMGR_OPR_LSTNR_CB_FUNC_PTR		2
#define AUDMGR_CODEC_LSTR_FUNC_PTR		3

#define AUDMGR_CB_PROG_VERS "rs31000013:0xf8e3e2d9"
#define AUDMGR_CB_PROG 0x31000013
#define AUDMGR_CB_VERS 0xf8e3e2d9

struct audmgr {
	wait_queue_head_t wait;
	uint32_t handle;
	int state;
};

struct audmgr_config {
	uint32_t tx_rate;
	uint32_t rx_rate;
	uint32_t def_method;
	uint32_t codec;
	uint32_t snd_method;
};

int audmgr_open(struct audmgr *am);
int audmgr_close(struct audmgr *am);
int audmgr_enable(struct audmgr *am, struct audmgr_config *cfg);
int audmgr_disable(struct audmgr *am);

typedef void (*audpp_event_func)(void *private, unsigned id, uint16_t *msg);
typedef void (*audrec_event_func)(void *private, unsigned id, uint16_t *msg);

/* worst case delay of 1sec for response */
#define MSM_AUD_DECODER_WAIT_MS 1000
#define MSM_AUD_MODE_TUNNEL  0x00000100
#define MSM_AUD_MODE_NONTUNNEL  0x00000200
#define MSM_AUD_DECODER_MASK  0x0000FFFF
#define MSM_AUD_OP_MASK  0xFFFF0000

/*Playback mode*/
#define NON_TUNNEL_MODE_PLAYBACK 1
#define TUNNEL_MODE_PLAYBACK 0

enum msm_aud_decoder_state {
	MSM_AUD_DECODER_STATE_NONE = 0,
	MSM_AUD_DECODER_STATE_FAILURE = 1,
	MSM_AUD_DECODER_STATE_SUCCESS = 2,
	MSM_AUD_DECODER_STATE_CLOSE = 3,
};

int audpp_adec_alloc(unsigned dec_attrb, const char **module_name,
			unsigned *queueid);
void audpp_adec_free(int decid);

struct audpp_event_callback {
	audpp_event_func fn;
	void *private;
};

int audpp_register_event_callback(struct audpp_event_callback *eh);
int audpp_unregister_event_callback(struct audpp_event_callback *eh);
int is_audpp_enable(void);

int audpp_enable(int id, audpp_event_func func, void *private);
void audpp_disable(int id, void *private);

int audpp_send_queue1(void *cmd, unsigned len);
int audpp_send_queue2(void *cmd, unsigned len);
int audpp_send_queue3(void *cmd, unsigned len);

int audpp_set_volume_and_pan(unsigned id, unsigned volume, int pan);
int audpp_pause(unsigned id, int pause);
int audpp_flush(unsigned id);
void audpp_avsync(int id, unsigned rate);
unsigned audpp_avsync_sample_count(int id);
unsigned audpp_avsync_byte_count(int id);
int audpp_dsp_set_mbadrc(unsigned id, unsigned enable,
			audpp_cmd_cfg_object_params_mbadrc *mbadrc);
int audpp_dsp_set_eq(unsigned id, unsigned enable,
			audpp_cmd_cfg_object_params_eqalizer *eq);
int audpp_dsp_set_rx_iir(unsigned id, unsigned enable,
				audpp_cmd_cfg_object_params_pcm *iir);

int audpp_dsp_set_rx_srs_trumedia_g
	(struct audpp_cmd_cfg_object_params_srstm_g *srstm);
int audpp_dsp_set_rx_srs_trumedia_w
	(struct audpp_cmd_cfg_object_params_srstm_w *srstm);
int audpp_dsp_set_rx_srs_trumedia_c
	(struct audpp_cmd_cfg_object_params_srstm_c *srstm);
int audpp_dsp_set_rx_srs_trumedia_h
	(struct audpp_cmd_cfg_object_params_srstm_h *srstm);
int audpp_dsp_set_rx_srs_trumedia_p
	(struct audpp_cmd_cfg_object_params_srstm_p *srstm);
int audpp_dsp_set_rx_srs_trumedia_l
	(struct audpp_cmd_cfg_object_params_srstm_l *srstm);

int audpp_dsp_set_vol_pan(unsigned id,
				audpp_cmd_cfg_object_params_volume *vol_pan);
int audpp_dsp_set_qconcert_plus(unsigned id, unsigned enable,
			audpp_cmd_cfg_object_params_qconcert *qconcert_plus);
int audrectask_enable(unsigned enc_type, audrec_event_func func, void *private);
void audrectask_disable(unsigned enc_type, void *private);

int audrectask_send_cmdqueue(void *cmd, unsigned len);
int audrectask_send_bitstreamqueue(void *cmd, unsigned len);

#endif
