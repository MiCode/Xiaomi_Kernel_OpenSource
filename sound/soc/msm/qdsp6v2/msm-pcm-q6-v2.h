/*
 * Copyright (C) 2008 Google, Inc.
 * Copyright (C) 2008 HTC Corporation
 * Copyright (c) 2012-2018 The Linux Foundation. All rights reserved.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * See the GNU General Public License for more details.
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, you can find it at http://www.fsf.org.
 */

#ifndef _MSM_PCM_H
#define _MSM_PCM_H
#include <sound/apr_audio-v2.h>
#include <sound/q6asm-v2.h>
#include "msm-pcm-routing-v2.h"

/* Support unconventional sample rates 12000, 24000 as well */
#define USE_RATE                \
			(SNDRV_PCM_RATE_8000_48000 | SNDRV_PCM_RATE_KNOT)

extern int copy_count;

struct buffer {
	void *data;
	unsigned size;
	unsigned used;
	unsigned addr;
};

struct buffer_rec {
	void *data;
	unsigned int size;
	unsigned int read;
	unsigned int addr;
};

struct audio_locks {
	spinlock_t event_lock;
	wait_queue_head_t read_wait;
	wait_queue_head_t write_wait;
	wait_queue_head_t eos_wait;
	wait_queue_head_t enable_wait;
	wait_queue_head_t flush_wait;
};

struct msm_audio_in_frame_info {
	uint32_t size;
	uint32_t offset;
};

#define PLAYBACK_MIN_NUM_PERIODS    2
#define PLAYBACK_MAX_NUM_PERIODS    8
#define PLAYBACK_MAX_PERIOD_SIZE    122880
#define PLAYBACK_MIN_PERIOD_SIZE    128
#define CAPTURE_MIN_NUM_PERIODS     2
#define CAPTURE_MAX_NUM_PERIODS     8
#define CAPTURE_MAX_PERIOD_SIZE     122880
#define CAPTURE_MIN_PERIOD_SIZE     320

struct msm_audio {
	struct snd_pcm_substream *substream;
	unsigned int pcm_size;
	unsigned int pcm_count;
	unsigned int pcm_irq_pos;       /* IRQ position */
	uint16_t source; /* Encoding source bit mask */

	struct audio_client *audio_client;

	uint16_t session_id;

	uint32_t samp_rate;
	uint32_t channel_mode;
	uint32_t dsp_cnt;

	int abort; /* set when error, like sample rate mismatch */

	bool reset_event;
	int enabled;
	int close_ack;
	int cmd_ack;
	/*
	 * cmd_ack doesn't tell if paticular command has been sent so can't
	 * determine if it needs to wait for completion.
	 * Use cmd_pending instead when checking whether a command is been
	 * sent or not.
	 */
	unsigned long cmd_pending;
	atomic_t start;
	atomic_t stop;
	atomic_t out_count;
	atomic_t in_count;
	atomic_t out_needed;
	atomic_t eos;
	int out_head;
	int periods;
	int mmap_flag;
	atomic_t pending_buffer;
	bool set_channel_map;
	char channel_map[PCM_FORMAT_MAX_NUM_CHANNEL_V2];
	int cmd_interrupt;
	bool meta_data_mode;
	uint32_t volume;
	bool compress_enable;
	/* array of frame info */
	struct msm_audio_in_frame_info in_frame_info[CAPTURE_MAX_NUM_PERIODS];
};

struct output_meta_data_st {
	uint32_t meta_data_length;
	uint32_t frame_size;
	uint32_t timestamp_lsw;
	uint32_t timestamp_msw;
	uint32_t reserved[12];
};

struct msm_plat_data {
	int perf_mode;
	bool avs_ver;
	struct snd_pcm *pcm;
	struct snd_pcm *pcm_device[MSM_FRONTEND_DAI_MM_SIZE];
	struct msm_pcm_channel_mixer chmixer_pspd[MSM_FRONTEND_DAI_MM_SIZE][2];
};

#endif /*_MSM_PCM_H*/
