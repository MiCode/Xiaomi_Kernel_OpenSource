/* Copyright (c) 2012, Code Aurora Forum. All rights reserved.
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
#ifndef _MSM_PCM_VOICE_H
#define _MSM_PCM_VOICE_H
#include <sound/apr_audio.h>

enum {
	VOICE_SESSION_INDEX,
	VOLTE_SESSION_INDEX,
	VOICE_SESSION_INDEX_MAX,
};

struct msm_voice {
	struct snd_pcm_substream *playback_substream;
	struct snd_pcm_substream *capture_substream;

	int instance;

	struct mutex lock;

	uint32_t samp_rate;
	uint32_t channel_mode;

	int playback_start;
	int capture_start;
};

#endif /*_MSM_PCM_VOICE_H*/
