/*
 * Copyright (C) 2016 MediaTek Inc.
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

#ifndef AUDIO_IPI_MESSAGE_H
#define AUDIO_IPI_MESSAGE_H


#include <linux/types.h>


/*
 * =============================================================================
 *                     MACRO
 * =============================================================================
 */

#define IPI_MSG_HEADER_SIZE      (16)
#define MAX_IPI_MSG_PAYLOAD_SIZE (32)
#define MAX_IPI_MSG_BUF_SIZE    (IPI_MSG_HEADER_SIZE + MAX_IPI_MSG_PAYLOAD_SIZE)

#define IPI_MSG_MAGIC_NUMBER     (0x8888)


/*
 * =============================================================================
 *                     typedef
 * =============================================================================
 */

enum { /* audio_ipi_msg_layer_t */
	AUDIO_IPI_LAYER_HAL_TO_KERNEL,         /* HAL    -> kernel */
	AUDIO_IPI_LAYER_HAL_TO_SCP,            /* HAL    -> SCP */

	AUDIO_IPI_LAYER_KERNEL_TO_HAL,         /* kernel -> HAL */
	AUDIO_IPI_LAYER_KERNEL_TO_SCP,         /* kernel -> SCP */
	AUDIO_IPI_LAYER_KERNEL_TO_SCP_ATOMIC,  /* kernel -> SCP ATOMIC */
};

enum { /* audio_ipi_msg_data_t */
	/* param1: defined by user,       param2: defined by user */
	AUDIO_IPI_MSG_ONLY,
	/* param1: payload length (<=32), param2: defined by user */
	AUDIO_IPI_PAYLOAD,
	/* param1: dma data length,       param2: defined by user */
	AUDIO_IPI_DMA,
};

enum {
	AUDIO_IPI_MSG_BYPASS_ACK    = 0,
	AUDIO_IPI_MSG_NEED_ACK      = 1,
	AUDIO_IPI_MSG_ACK_BACK      = 2,
	AUDIO_IPI_MSG_DIRECT_SEND   = 3,
	AUDIO_IPI_MSG_CANCELED      = 8
};

/*
 * =============================================================================
 *                     struct definition
 * =============================================================================
 */

struct ipi_msg_t {
	uint16_t magic;      /* IPI_MSG_MAGIC_NUMBER */
	uint8_t  task_scene; /* see task_scene_t */
	uint8_t  msg_layer;  /* see audio_ipi_msg_layer_t */
	uint8_t  data_type;  /* see audio_ipi_msg_data_t */
	uint8_t  ack_type;   /* see audio_ipi_msg_ack_t */
	uint16_t msg_id;     /* defined by user */
	uint32_t param1;     /* see audio_ipi_msg_data_t */
	uint32_t param2;     /* see audio_ipi_msg_data_t */
	union {
		char payload[MAX_IPI_MSG_PAYLOAD_SIZE];
		char *dma_addr;  /* for AUDIO_IPI_DMA only */
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

void dump_msg(const struct ipi_msg_t *p_ipi_msg);

void check_msg_format(const struct ipi_msg_t *p_ipi_msg, unsigned int len);

void print_msg_info(
	const char *func_name,
	const char *description,
	const struct ipi_msg_t *p_ipi_msg);


#endif /* end of AUDIO_IPI_MESSAGE_H */


