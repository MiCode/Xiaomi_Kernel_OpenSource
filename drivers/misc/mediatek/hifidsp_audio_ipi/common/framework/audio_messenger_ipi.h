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

#ifndef AUDIO_MESSENGER_IPI_H
#define AUDIO_MESSENGER_IPI_H

#include <linux/types.h>

#include <audio_ipi_dma.h>



/*
 * =============================================================================
 *                     MACRO
 * =============================================================================
 */

#define MAX_IPI_MSG_BUF_SIZE     (272) /* SHARE_BUF_SIZE - 16 */
#define IPI_MSG_HEADER_SIZE      (16)
#define MAX_IPI_MSG_PAYLOAD_SIZE (MAX_IPI_MSG_BUF_SIZE - IPI_MSG_HEADER_SIZE)

#define IPI_MSG_MAGIC_NUMBER     (0x8888)


/*
 * =============================================================================
 *                     typedef
 * =============================================================================
 */

enum { /* audio_ipi_msg_source_layer_t */
	AUDIO_IPI_LAYER_FROM_HAL,
	AUDIO_IPI_LAYER_FROM_KERNEL,
	AUDIO_IPI_LAYER_FROM_DSP,

	AUDIO_IPI_LAYER_FROM_SIZE,
	AUDIO_IPI_LAYER_FROM_MAX = 15 /* 4-bit only */
};

enum { /* audio_ipi_msg_target_layer_t */
	AUDIO_IPI_LAYER_TO_HAL,
	AUDIO_IPI_LAYER_TO_KERNEL,
	AUDIO_IPI_LAYER_TO_DSP,

	AUDIO_IPI_LAYER_TO_SIZE,
	AUDIO_IPI_LAYER_TO_MAX = 15 /* 4-bit only */
};

enum { /* audio_ipi_msg_data_t */
	AUDIO_IPI_MSG_ONLY,
	AUDIO_IPI_PAYLOAD,
	AUDIO_IPI_DMA,

	AUDIO_IPI_TYPE_SIZE
};

enum { /* audio_ipi_msg_ack_t */
	/* bypass ack, but still send to audio queue */
	AUDIO_IPI_MSG_BYPASS_ACK    = 0,

	/* need ack, and block in audio queue until ack back */
	AUDIO_IPI_MSG_NEED_ACK      = 1,
	AUDIO_IPI_MSG_ACK_BACK      = 2,

	/* bypass audio queue, but still send to ipc queue */
	AUDIO_IPI_MSG_DIRECT_SEND   = 3,

	AUDIO_IPI_MSG_CANCELED      = 8
};

/*
 * =============================================================================
 *                     struct definition
 * =============================================================================
 */

struct aud_data_t {
	uint32_t memory_size;           /* buffer size (memory) */
	uint32_t data_size;             /* 0 <= data_size <= memory_size */
	union {
		void    *addr;          /* memory address */
		unsigned long addr_val; /* the value of address */

		uint32_t dummy[2];      /* work between 32/64 bit environment */
	};
};

struct ipi_msg_dma_info_t {
	struct aud_data_t reserve;      /* TODO: remove later */

	struct aud_data_t hal_buf;      /* source data buffer */

	uint32_t rw_idx;                /* region r/w index */
	uint32_t data_size;             /* region data size */

	struct aud_data_t wb_dram;      /* allow target to write data back */
};
#define IPI_MSG_DMA_INFO_SIZE (sizeof(struct ipi_msg_dma_info_t))


struct ipi_msg_t {
	/* header: 16 bytes */
	uint16_t magic;             /* IPI_MSG_MAGIC_NUMBER */
	uint8_t  task_scene;        /* see task_scene_t */
	uint8_t  source_layer: 4;   /* see audio_ipi_msg_source_layer_t */
	uint8_t  target_layer: 4;   /* see audio_ipi_msg_target_layer_t */

	uint8_t  data_type;         /* see audio_ipi_msg_data_t */
	uint8_t  ack_type;          /* see audio_ipi_msg_ack_t */
	uint16_t msg_id;            /* defined by user */

	union {
		uint32_t param1;
		uint32_t payload_size;  /* payload */
		uint32_t scp_ret;
	};

	uint32_t param2;

	/* data: 256 bytes */
	union {
		char payload[MAX_IPI_MSG_PAYLOAD_SIZE]; /* payload only */
		struct ipi_msg_dma_info_t dma_info;     /* dma only */
		char *dma_addr;  /* TODO: remove later */
	};

};


/*
 * =============================================================================
 *                     hook function
 * =============================================================================
 */

typedef void (*recv_message_t)(struct ipi_msg_t *p_ipi_msg);


/*
 * =============================================================================
 *                     public function
 * =============================================================================
 */

uint16_t get_message_buf_size(const struct ipi_msg_t *p_ipi_msg);

int check_msg_format(const struct ipi_msg_t *p_ipi_msg, unsigned int len);

bool check_print_msg_info(const struct ipi_msg_t *p_ipi_msg);


#define DUMP_IPI_MSG(description, p_ipi_msg) \
	do { \
		if (description == NULL || (p_ipi_msg) == NULL) \
			break; \
		if ((p_ipi_msg)->magic != IPI_MSG_MAGIC_NUMBER) { \
			pr_info("%s, but magic 0x%x fail", \
				description, (p_ipi_msg)->magic); \
			break; \
		} \
		if ((p_ipi_msg)->data_type == AUDIO_IPI_MSG_ONLY) { \
			pr_info("%s, task: %d, msg_id: 0x%x, ack_type: %d, " \
				"p1: 0x%x, p2: 0x%x", \
				description, \
				(p_ipi_msg)->task_scene, \
				(p_ipi_msg)->msg_id, \
				(p_ipi_msg)->ack_type, \
				(p_ipi_msg)->param1, \
				(p_ipi_msg)->param2); \
		} else if ((p_ipi_msg)->data_type == AUDIO_IPI_PAYLOAD) { \
			pr_info("%s, task: %d, msg_id: 0x%x, ack_type: %d, " \
				"payload_size: 0x%x, p2: 0x%x", \
				description, \
				(p_ipi_msg)->task_scene, \
				(p_ipi_msg)->msg_id, \
				(p_ipi_msg)->ack_type, \
				(p_ipi_msg)->payload_size, \
				(p_ipi_msg)->param2); \
		} else if ((p_ipi_msg)->data_type == AUDIO_IPI_DMA) { \
			pr_info("%s, task: %d, msg_id: 0x%x, ack_type: %d, " \
				"p1: 0x%x, p2: 0x%x, dma sz: %u, idx: %u, " \
				"hal sz: %u", \
				description, \
				(p_ipi_msg)->task_scene, \
				(p_ipi_msg)->msg_id, \
				(p_ipi_msg)->ack_type, \
				(p_ipi_msg)->param1, \
				(p_ipi_msg)->param2, \
				(p_ipi_msg)->dma_info.data_size, \
				(p_ipi_msg)->dma_info.rw_idx, \
				(p_ipi_msg)->dma_info.hal_buf.data_size); \
		} \
	} while (0)


/*
 * =============================================================================
 *                     public functions - declaration
 * =============================================================================
 */

void audio_messenger_ipi_init(void);

void audio_reg_recv_message(uint8_t task_scene, recv_message_t recv_message);



int audio_send_ipi_msg(
	struct ipi_msg_t *p_ipi_msg,
	uint8_t task_scene, /* task_scene_t */
	uint8_t target_layer, /* audio_ipi_msg_target_layer_t */
	uint8_t data_type, /* audio_ipi_msg_data_t */
	uint8_t ack_type, /* audio_ipi_msg_ack_t */
	uint16_t msg_id,
	uint32_t param1, /* data_size for payload & dma */
	uint32_t param2,
	void    *data_buffer); /* buffer for payload & dma */

int audio_send_ipi_filled_msg(struct ipi_msg_t *p_ipi_msg);

int send_message_to_scp(const struct ipi_msg_t *p_ipi_msg);




#endif /* end of AUDIO_MESSENGER_IPI_H */

