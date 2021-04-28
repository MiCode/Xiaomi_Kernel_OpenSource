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
	VA_STATE_PREPROCESSING,
	VA_STATE_AEC,
	VA_STATE_KEYWORD,
	VA_STATE_UPLOAD,
};

/* Voice Assitant type*/
enum {
	VA_RECORD = 0,
	VA_VAD,
	VA_AEC,
	VA_KEYWORD,
	VA_PREPROCESSING,
};

enum {
	VA_NOTIFY_VAD_PASS,
	VA_NOTIFY_WAKEWORD_PASS,
};

/*Voice Assistant State Machine*/
#define VA_STATE(x)			(1 << x)

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

enum {
	AFE_MEM_TYPE_AFE_SRAM = 0,
	AFE_MEM_TYPE_LP,
	AFE_MEM_TYPE_DRAM,
};

struct audio_dma_mem {
	uint32_t mem_type;
	uint32_t dma_paddr;
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
		struct audio_dma_mem adsp_dma;
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
		struct audio_dma_mem adsp_dma;
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

struct ipc_clk_mode_params {
	uint8_t clk_mode;
};

struct dsp_ipc_va_notify {
	uint32_t type;
	char wakeword[32];
};

enum DUMP_POSITION {
	DSP_DUMP1,
	DSP_DUMP2,
	DSP_DUMP3,
	DSP_DUMP4,
	DSP_DATA_DUMP_NUM,
};

struct host_debug_param {
	uint16_t inited;
	uint16_t dump_position;
	uint32_t request_bytes;
	uint32_t period_size;
	uint32_t period_cnt;
	uint32_t shared_base;
	uint32_t chnum;
	uint32_t bitwidth;
	uint32_t buffer_size;
};


struct dsp_debug_irq_param {
	uint16_t dump_position;
	uint32_t irq_notify_done;
};

struct host_load_model_init_param {
	uint16_t inited;
	uint32_t shared_base;
	uint32_t model_size;
};

struct dsp_info_notify {
	uint32_t max_pll;
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

#define AUDIO_COPY_VA_PARAMS_SRC2DST(src, dst) \
	memcpy((void *)dst, (void *)src, \
	sizeof(struct ipc_va_params))

#define AUDIO_COPY_IPC_RESERVE_MEM(src, dst) \
	memcpy((void *)dst, (void *)src, \
	sizeof(struct ipc_reserve_memory))

#define DAI_HOSTLESS_MASK			0x01000000
#define DAI_HOSTLESS			0x01000000
#define DAI_NON_HOSTLESS	0x00000000

#define DAI_TYPE_MASK			0x00F00000
#define DAI_TYPE_POS			20
#define DAI_VA_RECORD_TYPE			0x00000000
#define DAI_MIC_RECORD_TYPE			0x00000001
#define DAI_AEC_RECORD_TYPE			0x00000002
#define DAI_WW_RECORD_TYPE			0x00000003

#define DAI_META_MASK			(DAI_HOSTLESS_MASK|DAI_TYPE_MASK)

#define DAI_IS_HOSTLESS(dai)            ((dai&DAI_HOSTLESS_MASK) != 0)
#define DAI_GET_TYPE(dai)               (((dai&DAI_TYPE_MASK)>>DAI_TYPE_POS))
#define DAI_UNPACK_ID(dai)              (dai&(~DAI_META_MASK))
#define DAI_UNPACK_HOSTLESS_ID(dai)     (dai&(~DAI_HOSTLESS_MASK))
#define DAI_PACK_ID(dai, type, hostless) \
	((dai)|(type<<DAI_TYPE_POS)|(hostless))

//if the host can't wait, it should not ack.
enum {
	MSG_TO_DSP_CREATE_VA_T = 0,	//create voice assistant task
	MSG_TO_DSP_DESTROY_VA_T,	//destroy voice assistant task
	MSG_TO_DSP_AP_SUSPEND_T,	//ap suspend event
	MSG_TO_DSP_AP_RESUME_T,	 //ap resume event
	MSG_TO_DSP_SCENE_VA_VAD,	//Enable Voice Activity Detection
	MSG_TO_DSP_SCENE_VA_KEYWORD,	//Enable Keyword detection
	MSG_TO_DSP_SCENE_VA_AEC,		//Enable AEC
	MSG_TO_DSP_SCENE_VA_PREPROCESSING,	// Enable Pre-processing
	MSG_TO_DSP_SCENE_VA_RECORD,		// Do Record
	MSG_TO_DSP_SCENE_VA_VOICE_UPLOAD_DONE,
	MSG_TO_DSP_SCENE_VA_BEAMFORMING,
	MSG_TO_DSP_SCENE_VA_VAD_FORCE_OK,
	MSG_TO_DSP_SCENE_VA_KEYWORD_FORCE_OK,
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
	MSG_TO_DSP_DEBUG_START,
	MSG_TO_DSP_DEBUG_STOP,
	MSG_TO_DSP_CLK_MODE_SET,
	MSG_TO_DSP_LOAD_MODEL_OPEN,
	MSG_TO_DSP_LOAD_MODEL_CLOSE,
	MSG_TO_DSP_LOAD_MODEL_WRITE,
	MSG_TO_DSP_SCENE_VA_WW_ARCH,	// Get wakeword arch info,
	MSG_TO_DSP_NUM,
	// message from DSP to host
	MSG_TO_HOST_DSP_IRQUL = 0x1000,			// Uplink IRQ
	MSG_TO_HOST_DSP_IRQDL,			// Downlink IRQ
	MSG_TO_HOST_VA_NOTIFY,	// notify Host keyword/vad detection pass
	MSG_TO_HOST_DSP_AUDIO_READY,	// DSP notify Host that DSP is ready.
	MSG_TO_HOST_IRQ,				//should not ack
	MSG_TO_HOST_DSP_DEBUG_IRQ,
};

#endif // end of __AUDIO_SHARED_INFO_H__

