/* SPDX-License-Identifier: GPL-2.0-only */
/* Copyright (c) 2012-2015, The Linux Foundation. All rights reserved.
 */
#ifndef _MSM_PCM_VOICE_H
#define _MSM_PCM_VOICE_H
#include <dsp/apr_audio-v2.h>

enum {
	VOICE_SESSION_INDEX,
	VOLTE_SESSION_INDEX,
	VOICE2_SESSION_INDEX,
	QCHAT_SESSION_INDEX,
	VOWLAN_SESSION_INDEX,
	VOICEMMODE1_INDEX,
	VOICEMMODE2_INDEX,
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
