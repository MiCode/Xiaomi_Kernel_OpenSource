/*
 * Copyright (C) 2018 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */
/* This file define the shared information between Host and DSP, need to
 * synchronize this file with host
 */
#ifndef __AUDIO_SHARED_INFO_H__
#define __AUDIO_SHARED_INFO_H__
enum {
	TASK_SCENE_AUDIO_CONTROLLER = 0,
	TASK_SCENE_VA,			// Voice Assistant
	TASK_SCENE_SIZE,
	TASK_SCENE_INVALID
};

enum {
	VA_STATE_IDLE,
	VA_STATE_VAD,
	VA_STATE_KEYWORD,
	VA_STATE_UPLOAD,
};

enum {
	RING_BUF_TYPE_RECORD,
	RING_BUF_TYPE_PLAYBACK,
};

struct io_ipc_ring_buf_shared {
	uint32_t start_addr;
	uint32_t size_bytes;
	uint32_t ptr_to_hw_offset_bytes;
	uint32_t ptr_to_appl_offset_bytes;

	//ring_buffer_type: record or playback
	uint32_t ring_buffer_dir;

	/* hw_offset_flag:
	 * We treat hw_offset==appl_offset as buffer empty when record.
	 * If the buffer is full the hw_offset will be one bytes behind the
	 * appl_offset and the hw_offset_flag will set to one.
	 * When playback, hw_offset==appl_offset will be treated as full. If
	 * the buffer is empty, the hw_offset will be one bytes behind the
	 * appl_offset and the hw_offset_flag will set to one.
	 */
	uint32_t hw_offset_flag;
};

/* information struct from host */
struct host_ipc_msg_hw_param {
	uint32_t dai_id;
	uint32_t sample_rate;
	uint8_t channel_num;
	uint8_t bitwidth; /* 16bits or 32bits */
	uint32_t period_size; /* in frames */
	uint32_t period_count;
	union {
		uint32_t afe_dma_paddr;
	};
};

struct host_ipc_msg_hw_free {
	uint32_t dai_id;
};

struct host_ipc_msg_trigger {
	uint32_t dai_id;
};

/* information struct to host */
struct dsp_ipc_msg_hw_param {
	uint32_t dai_id;
	uint32_t sample_rate;
	uint8_t channel_num;
	uint8_t bitwidth; /* 16bits or 32bits */
	uint32_t period_size; /* in frames */
	uint32_t period_count;
	union {
		struct io_ipc_ring_buf_shared SharedRingBuffer;
		uint32_t adsp_dma_control_paddr;
	};
};

struct dsp_ipc_msg_irq {
	uint32_t dai_id;
	struct io_ipc_ring_buf_shared share_ring_buf;
};

struct ipc_va_params {
	uint32_t va_type;
	uint8_t enable_flag;
};

struct ipc_reserve_memory {
	uint32_t start_addr;
	uint32_t size_bytes;
};

#define AUDIO_IPC_COPY_DSP_HW_PARAM(src, dst) \
	memcpy((void *)dst, (void *)src, sizeof(struct dsp_ipc_msg_hw_param))

#define AUDIO_IPC_COPY_HOST_HW_PARAM(src, dst) \
	memcpy((void *)dst, (void *)src, sizeof(struct host_ipc_msg_hw_param))

#define AUDIO_IPC_COPY_HOST_HW_FREE(src, dst) \
	memcpy((void *)dst, (void *)src, sizeof(struct host_ipc_msg_hw_free))

#define AUDIO_IPC_COPY_HOST_TRIGGER(src, dst) \
	memcpy((void *)dst, (void *)src, sizeof(struct host_ipc_msg_trigger))

#define AUDIO_COPY_SHARED_BUFFER_INFO(src, dst) \
	memcpy((void *)dst, (void *)src, \
	sizeof(struct io_ipc_ring_buf_shared))

#define AUDIO_COPY_IPC_RESERVE_MEM(src, dst) \
	memcpy((void *)dst, (void *)src, \
	sizeof(struct ipc_reserve_memory))

#define DAI_HOSTLESS_MASK			0x01000000
#define IS_HOSTLESS_DAI(a)			((a&DAI_HOSTLESS_MASK) != 0)
#define UNPACK_HOSTLESS_DAI(a)			(a&(~DAI_HOSTLESS_MASK))
#define PACK_HOSTLESS_DAI(a)			(a|DAI_HOSTLESS_MASK)

//if the host can't wait, it should not ack.
enum {
	MSG_TO_DSP_CREATE_VA_T = 0,	//create voice assistant task
	MSG_TO_DSP_DESTROY_VA_T,	//destroy voice assistant task
	MSG_TO_DSP_SCENE_VA_VAD,	//Enable Voice Activity Detection
	MSG_TO_DSP_SCENE_VA_KEYWORD,	//Enable Keyword detection
	MSG_TO_DSP_SCENE_VA_AEC,		//Enable AEC
	MSG_TO_DSP_SCENE_VA_PREPROCESSING,	// Enable Pre-processing
	MSG_TO_DSP_SCENE_VA_NORMAL_REC,		// Normal Recording
	MSG_TO_DSP_RESERVE_MEM,		//Set task reserve memory
	MSG_TO_DSP_HOST_PORT_STARTUP,	//should ack
	MSG_TO_DSP_HOST_HW_PARAMS,	//should ack
	MSG_TO_DSP_HOST_PREPARE,	//should ack
	MSG_TO_DSP_HOST_TRIGGER_START,	//should not ack
	MSG_TO_DSP_HOST_TRIGGER_STOP,	//should not ack
	MSG_TO_DSP_HOST_HW_FREE,	//should ack
	MSG_TO_DSP_HOST_CLOSE,		//should ack
	MSG_TO_DSP_DSP_PORT_STARTUP,	//should ack
	MSG_TO_DSP_DSP_HW_PARAMS,	//should ack
	MSG_TO_DSP_DSP_PREPARE,		//should ack
	MSG_TO_DSP_DSP_TRIGGER_START,	//should not ack
	MSG_TO_DSP_DSP_TRIGGER_STOP,	//should not ack
	MSG_TO_DSP_DSP_HW_FREE,		//should ack
	MSG_TO_DSP_DSP_CLOSE,		//should ack
	MSG_TO_DSP_NUM,
	// message from DSP to host
	MSG_TO_HOST_DSP_IRQUL,			// Uplink IRQ
	MSG_TO_HOST_DSP_IRQDL,			// Downlink IRQ
	MSG_TO_HOST_VA_KEYWORD_PASS,	// notify Host keyword detection pass
	MSG_TO_HOST_DSP_AUDIO_READY,	// DSP notify Host that DSP is ready.
	MSG_TO_HOST_IRQ,				//should not ack
};

#endif // end of __AUDIO_SHARED_INFO_H__

