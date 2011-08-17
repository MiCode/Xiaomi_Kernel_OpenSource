/* Copyright (c) 2009-2011, Code Aurora Forum. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef _MACH_QDSP5_V2_AUDPREPROC_H
#define _MACH_QDSP5_V2_AUDPREPROC_H

#include <mach/qdsp5v2/qdsp5audpreproccmdi.h>
#include <mach/qdsp5v2/qdsp5audpreprocmsg.h>

#define MAX_ENC_COUNT 3

#define MSM_ADSP_ENC_CODEC_WAV 0
#define MSM_ADSP_ENC_CODEC_AAC 1
#define MSM_ADSP_ENC_CODEC_SBC 2
#define MSM_ADSP_ENC_CODEC_AMRNB 3
#define MSM_ADSP_ENC_CODEC_EVRC 4
#define MSM_ADSP_ENC_CODEC_QCELP 5
#define MSM_ADSP_ENC_CODEC_EXT_WAV (15)

#define MSM_ADSP_ENC_MODE_TUNNEL 24
#define MSM_ADSP_ENC_MODE_NON_TUNNEL 25

#define AUDPREPROC_CODEC_MASK 0x00FF
#define AUDPREPROC_MODE_MASK 0xFF00

#define MSM_AUD_ENC_MODE_TUNNEL  0x00000100
#define MSM_AUD_ENC_MODE_NONTUNNEL  0x00000200

#define SOURCE_PIPE_1	0x0001
#define SOURCE_PIPE_0	0x0000

/* event callback routine prototype*/
typedef void (*audpreproc_event_func)(void *private, unsigned id, void *msg);

struct audpreproc_event_callback {
	audpreproc_event_func fn;
	void *private;
};

/*holds audrec information*/
struct audrec_session_info {
	int session_id;
	int sampling_freq;
};

/* Exported common api's from audpreproc layer */
int audpreproc_aenc_alloc(unsigned enc_type, const char **module_name,
		unsigned *queue_id);
void audpreproc_aenc_free(int enc_id);

int audpreproc_enable(int enc_id, audpreproc_event_func func, void *private);
void audpreproc_disable(int enc_id, void *private);

int audpreproc_send_audreccmdqueue(void *cmd, unsigned len);

int audpreproc_send_preproccmdqueue(void *cmd, unsigned len);

int audpreproc_dsp_set_agc(struct audpreproc_cmd_cfg_agc_params *agc,
	unsigned len);
int audpreproc_dsp_set_agc2(struct audpreproc_cmd_cfg_agc_params_2 *agc2,
	unsigned len);
int audpreproc_dsp_set_ns(struct audpreproc_cmd_cfg_ns_params *ns,
	unsigned len);
int audpreproc_dsp_set_iir(
struct audpreproc_cmd_cfg_iir_tuning_filter_params *iir, unsigned len);

int audpreproc_dsp_set_agc(struct audpreproc_cmd_cfg_agc_params *agc,
 unsigned int len);

int audpreproc_dsp_set_iir(
struct audpreproc_cmd_cfg_iir_tuning_filter_params *iir, unsigned int len);

int audpreproc_update_audrec_info(struct audrec_session_info
						*audrec_session_info);
int audpreproc_unregister_event_callback(struct audpreproc_event_callback *ecb);

int audpreproc_register_event_callback(struct audpreproc_event_callback *ecb);

int audpreproc_dsp_set_gain_tx(
	struct audpreproc_cmd_cfg_cal_gain *calib_gain_tx, unsigned len);

void get_audrec_session_info(int id, struct audrec_session_info *info);

int audpreproc_dsp_set_lvnv(
	struct audpreproc_cmd_cfg_lvnv_param *preproc_lvnv, unsigned len);
#endif /* _MACH_QDSP5_V2_AUDPREPROC_H */
