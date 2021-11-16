/* SPDX-License-Identifier: GPL-2.0-only */
/* Copyright (c) 2012-2014, 2017, 2020 The Linux Foundation. All rights reserved.
 */


/* For Decoders */
#ifndef __Q6_AUDIO_COMMON_H__
#define __Q6_AUDIO_COMMON_H__

#include <dsp/apr_audio-v2.h>
#include <dsp/q6asm-v2.h>
extern spinlock_t enc_dec_lock;

void q6_audio_cb(uint32_t opcode, uint32_t token,
		uint32_t *payload, void *priv);

void audio_aio_cb(uint32_t opcode, uint32_t token,
			uint32_t *payload,  void *audio);


/* For Encoders */
void q6asm_in_cb(uint32_t opcode, uint32_t token,
		uint32_t *payload, void *priv);

void  audio_in_get_dsp_frames(void *audio,
		uint32_t token,	uint32_t *payload);

#endif /*__Q6_AUDIO_COMMON_H__*/
