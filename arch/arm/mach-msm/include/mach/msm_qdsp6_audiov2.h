/* arch/arm/mach-msm/include/mach/msm_qdsp6_audio.h
 *
 * Copyright (C) 2009 Google, Inc.
 * Copyright (c) 2009, Code Aurora Forum. All rights reserved.
 *
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

extern char *audio_data;
extern int32_t audio_phys;
extern uint32_t tx_clk_freq;

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

struct audio_client *q6voice_open(void);
int q6voice_setup(void);
int q6voice_teardown(void);
int q6voice_close(struct audio_client *ac);


struct audio_client *q6audio_open(uint32_t bufsz, uint32_t flags);
int q6audio_start(struct audio_client *ac, void *rpc, uint32_t len);

int q6audio_close(struct audio_client *ac);
int q6audio_read(struct audio_client *ac, struct audio_buffer *ab);
int q6audio_write(struct audio_client *ac, struct audio_buffer *ab);
int q6audio_async(struct audio_client *ac);

int q6audio_do_routing(uint32_t route);
int q6audio_set_tx_mute(int mute);
int q6audio_update_acdb(uint32_t id_src, uint32_t id_dst);
int q6audio_set_rx_volume(int level);
int q6audio_set_route(const char *name);

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

#endif
