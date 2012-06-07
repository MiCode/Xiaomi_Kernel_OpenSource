/*arch/arm/mach-msm/qdsp5iv2/audpp.h
 *
 * Copyright (C) 2008 Google, Inc.
 * Copyright (c) 2008-2011, Code Aurora Forum. All rights reserved.
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

#ifndef _MACH_QDSP5_V2_AUDPP_H
#define _MACH_QDSP5_V2_AUDPP_H

#include <mach/qdsp5v2/qdsp5audppcmdi.h>

typedef void (*audpp_event_func)(void *private, unsigned id, uint16_t *msg);

/* worst case delay of 1sec for response */
#define MSM_AUD_DECODER_WAIT_MS 1000
#define MSM_AUD_MODE_TUNNEL  0x00000100
#define MSM_AUD_MODE_NONTUNNEL  0x00000200
#define MSM_AUD_MODE_LP  0x00000400
#define MSM_AUD_DECODER_MASK  0x0000FFFF
#define MSM_AUD_OP_MASK  0xFFFF0000

/* read call timeout for error cases */
#define MSM_AUD_BUFFER_UPDATE_WAIT_MS 2000

/* stream info error message mask */
#define AUDPLAY_STREAM_INFO_MSG_MASK 0xFFFF0000
#define AUDPLAY_ERROR_THRESHOLD_ENABLE 0xFFFFFFFF

#define NON_TUNNEL_MODE_PLAYBACK 1
#define TUNNEL_MODE_PLAYBACK 0

#define AUDPP_MIXER_ICODEC AUDPP_CMD_CFG_DEV_MIXER_DEV_0
#define AUDPP_MIXER_1 AUDPP_CMD_CFG_DEV_MIXER_DEV_1
#define AUDPP_MIXER_2 AUDPP_CMD_CFG_DEV_MIXER_DEV_2
#define AUDPP_MIXER_3 AUDPP_CMD_CFG_DEV_MIXER_DEV_3
#define AUDPP_MIXER_HLB AUDPP_CMD_CFG_DEV_MIXER_DEV_4
#define AUDPP_MIXER_NONHLB (AUDPP_CMD_CFG_DEV_MIXER_DEV_0 | \
			AUDPP_CMD_CFG_DEV_MIXER_DEV_1 | \
			AUDPP_CMD_CFG_DEV_MIXER_DEV_2 | \
			AUDPP_CMD_CFG_DEV_MIXER_DEV_3)
#define AUDPP_MIXER_UPLINK_RX		AUDPP_CMD_CFG_DEV_MIXER_DEV_5
#define AUDPP_MAX_COPP_DEVICES		6

enum obj_type {
	COPP,
	POPP
};

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

void audpp_route_stream(unsigned short dec_id, unsigned short mixer_mask);

int audpp_set_volume_and_pan(unsigned id, unsigned volume, int pan,
					enum obj_type objtype);
int audpp_pause(unsigned id, int pause);
int audpp_flush(unsigned id);
int audpp_query_avsync(int id);
int audpp_restore_avsync(int id, uint16_t *avsync);

int audpp_dsp_set_eq(unsigned id, unsigned enable,
	struct audpp_cmd_cfg_object_params_eqalizer *eq,
			enum obj_type objtype);

int audpp_dsp_set_spa(unsigned id,
	struct audpp_cmd_cfg_object_params_spectram *spa,
			enum obj_type objtype);

int audpp_dsp_set_stf(unsigned id, unsigned enable,
     struct audpp_cmd_cfg_object_params_sidechain *stf,
			enum obj_type objtype);

int audpp_dsp_set_vol_pan(unsigned id,
	struct audpp_cmd_cfg_object_params_volume *vol_pan,
			enum obj_type objtype);

int audpp_dsp_set_mbadrc(unsigned id, unsigned enable,
	struct audpp_cmd_cfg_object_params_mbadrc *mbadrc,
	enum obj_type objtype);

int audpp_dsp_set_qconcert_plus(unsigned id, unsigned enable,
	struct audpp_cmd_cfg_object_params_qconcert *qconcert_plus,
	enum obj_type objtype);

int audpp_dsp_set_rx_iir(unsigned id, unsigned enable,
	struct audpp_cmd_cfg_object_params_pcm *iir,
	enum obj_type objtype);

int audpp_dsp_set_gain_rx(unsigned id,
	struct audpp_cmd_cfg_cal_gain *calib_gain_rx,
	enum obj_type objtype);
int audpp_dsp_set_pbe(unsigned id, unsigned enable,
	struct audpp_cmd_cfg_pbe *pbe_block,
	enum obj_type objtype);
#endif
