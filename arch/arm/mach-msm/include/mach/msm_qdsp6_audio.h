/* arch/arm/mach-msm/include/mach/msm_qdsp6_audio.h
 *
 * Copyright (C) 2009 Google, Inc.
 * Author: Brian Swetland <swetland@google.com>
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

#ifndef _MACH_MSM_QDSP6_Q6AUDIO_
#define _MACH_MSM_QDSP6_Q6AUDIO_

#define AUDIO_FLAG_READ		0
#define AUDIO_FLAG_WRITE	1
#define AUDIO_FLAG_INCALL_MIXED	2

#include <linux/wait.h>

enum {
	DEVICE_UNMUTE = 0,
	DEVICE_MUTE,
	STREAM_UNMUTE,
	STREAM_MUTE,
};

struct audio_buffer {
	dma_addr_t phys;
	void *data;
	uint32_t size;
	uint32_t used;	/* 1 = CPU is waiting for DSP to consume this buf */
	uint32_t actual_size; /* actual number of bytes read by DSP */
};

struct audio_client {
	struct audio_buffer buf[2];
	int cpu_buf;	/* next buffer the CPU will touch */
	int dsp_buf;	/* next buffer the DSP will touch */
	int running;
	int session;

	wait_queue_head_t wait;
	struct dal_client *client;

	int cb_status;
	uint32_t flags;
};

/* Obtain a 16bit signed, interleaved audio channel of the specified
 * rate (Hz) and channels (1 or 2), with two buffers of bufsz bytes.
 */
struct audio_client *q6audio_open_pcm(uint32_t bufsz, uint32_t rate,
				      uint32_t channels, uint32_t flags,
				      uint32_t acdb_id);

struct audio_client *q6audio_open_auxpcm(uint32_t rate, uint32_t channels,
					uint32_t flags, uint32_t acdb_id);

struct audio_client *q6voice_open(uint32_t flags);

struct audio_client *q6audio_open_mp3(uint32_t bufsz, uint32_t rate,
				      uint32_t channels, uint32_t acdb_id);

struct audio_client *q6audio_open_dtmf(uint32_t rate, uint32_t channels,
							uint32_t acdb_id);
int q6audio_play_dtmf(struct audio_client *ac, uint16_t dtmf_hi,
			uint16_t dtmf_low, uint16_t duration, uint16_t rx_gain);

struct audio_client *q6audio_open_aac(uint32_t bufsz, uint32_t samplerate,
					uint32_t channels, uint32_t bitrate,
					uint32_t stream_format, uint32_t flags,
					uint32_t acdb_id);

struct audio_client *q6audio_open_qcp(uint32_t bufsz, uint32_t min_rate,
					uint32_t max_rate, uint32_t flags,
					uint32_t format, uint32_t acdb_id);

struct audio_client *q6audio_open_amrnb(uint32_t bufsz, uint32_t enc_mode,
					uint32_t dtx_enable, uint32_t flags,
					uint32_t acdb_id);

int q6audio_close(struct audio_client *ac);
int q6audio_auxpcm_close(struct audio_client *ac);
int q6voice_close(struct audio_client *ac);
int q6audio_mp3_close(struct audio_client *ac);

int q6audio_read(struct audio_client *ac, struct audio_buffer *ab);
int q6audio_write(struct audio_client *ac, struct audio_buffer *ab);
int q6audio_async(struct audio_client *ac);

int q6audio_do_routing(uint32_t route, uint32_t acdb_id);
int q6audio_set_tx_mute(int mute);
int q6audio_reinit_acdb(char* filename);
int q6audio_update_acdb(uint32_t id_src, uint32_t id_dst);
int q6audio_set_rx_volume(int level);
int q6audio_set_stream_volume(struct audio_client *ac, int vol);
int q6audio_set_stream_eq_pcm(struct audio_client *ac, void *eq_config);

struct q6audio_analog_ops {
	void (*init)(void);
	void (*speaker_enable)(int en);
	void (*headset_enable)(int en);
	void (*receiver_enable)(int en);
	void (*bt_sco_enable)(int en);
	void (*int_mic_enable)(int en);
	void (*ext_mic_enable)(int en);
};

void q6audio_register_analog_ops(struct q6audio_analog_ops *ops);

/* signal non-recoverable DSP error so we can log and/or panic */
void q6audio_dsp_not_responding(void);

#endif
