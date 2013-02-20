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

	int state;

	wait_queue_head_t wait;
	wait_queue_head_t cmd_wait;

	struct dal_client *client;

	int cb_status;
	uint32_t flags;
	void *apr;
	int ref_count;
};

#endif
