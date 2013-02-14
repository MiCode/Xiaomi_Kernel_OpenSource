/*arch/arm/mach-msm/qdsp5audpp.h
 *
 * Copyright (C) 2008 Google, Inc.
 * Copyright (c) 2012, The Linux Foundation. All rights reserved.
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

#ifndef _MACH_QDSP5_AUDPP_H
#define _MACH_QDSP5_AUDPP_H

#include <mach/qdsp5/qdsp5audppcmdi.h>

typedef void (*audpp_event_func)(void *private, unsigned id, uint16_t *msg);

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

#endif
