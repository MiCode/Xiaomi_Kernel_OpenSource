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
 *
*/


/* For Decoders */
#ifndef __Q6_AUDIO_COMMON_H__
#define __Q6_AUDIO_COMMON_H__

#if defined(CONFIG_ARCH_MSM8974) || defined(CONFIG_ARCH_MSM9625) \
	|| defined(CONFIG_ARCH_MSM8226) || defined(CONFIG_ARCH_MSM8610)

#include <sound/apr_audio-v2.h>
#include <sound/q6asm-v2.h>
#else
#include <sound/apr_audio.h>
#include <sound/q6asm.h>
#endif

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
