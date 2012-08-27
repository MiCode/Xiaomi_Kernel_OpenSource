/* Copyright (c) 2011-2012, Code Aurora Forum. All rights reserved.
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

#ifndef QDSP5AUDPREPROC_H
#define _QDSP5AUDPREPROC_H

#include <mach/qdsp5/qdsp5audpreproccmdi.h>
#include <mach/qdsp5/qdsp5audpreprocmsg.h>

#define MSM_AUD_ENC_MODE_TUNNEL  0x00000100
#define MSM_AUD_ENC_MODE_NONTUNNEL  0x00000200

#define AUDPREPROC_CODEC_MASK 0x00FF
#define AUDPREPROC_MODE_MASK 0xFF00

#define MSM_ADSP_ENC_MODE_TUNNEL 24
#define MSM_ADSP_ENC_MODE_NON_TUNNEL 25

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

int audpreproc_unregister_event_callback(struct audpreproc_event_callback *ecb);

int audpreproc_register_event_callback(struct audpreproc_event_callback *ecb);

int audpreproc_update_audrec_info(struct audrec_session_info
						*audrec_session_info);
int get_audrec_session_info(struct audrec_session_info *info);

int audpreproc_dsp_set_agc(audpreproc_cmd_cfg_agc_params *agc,
	unsigned len);
int audpreproc_dsp_set_ns(audpreproc_cmd_cfg_ns_params *ns,
	unsigned len);
int audpreproc_dsp_set_iir(audpreproc_cmd_cfg_iir_tuning_filter_params *iir,
	unsigned len);

int audpreproc_send_preproccmdqueue(void *cmd, unsigned len);
typedef void (*audrec_event_func)(void *private, unsigned id, uint16_t *msg);
int audrectask_enable(unsigned enc_type, audrec_event_func func, void *private);
void audrectask_disable(unsigned enc_type, void *private);

int audrectask_send_cmdqueue(void *cmd, unsigned len);
int audrectask_send_bitstreamqueue(void *cmd, unsigned len);

#endif /* QDSP5AUDPREPROC_H */
