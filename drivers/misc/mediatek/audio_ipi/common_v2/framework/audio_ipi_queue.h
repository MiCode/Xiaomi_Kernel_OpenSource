// SPDX-License-Identifier: GPL-2.0
//
// audio_ipi_queue.h
//
// Copyright (c) 2018 MediaTek Inc.

#ifndef AUDIO_IPI_MSG_QUEUE_H
#define AUDIO_IPI_MSG_QUEUE_H

#include <linux/types.h>


/*
 * =============================================================================
 *                     ref struct
 * =============================================================================
 */

struct ipi_msg_t;


/*
 * =============================================================================
 *                     struct definition
 * =============================================================================
 */

struct ipi_queue_handler_t {
	/* set void to prevent get/set attributes from outside */
	void *msg_queue; /* struct msg_queue_t */
};


/*
 * =============================================================================
 *                     public function
 * =============================================================================
 */

struct ipi_queue_handler_t *create_ipi_queue_handler(const uint8_t task_scene);
void destroy_ipi_queue_handler(struct ipi_queue_handler_t *handler);

struct ipi_queue_handler_t *get_ipi_queue_handler(const uint8_t task_scene);

void disable_ipi_queue_handler(struct ipi_queue_handler_t *handler);

int flush_ipi_queue_handler(struct ipi_queue_handler_t *handler);

int send_message(struct ipi_queue_handler_t *handler,
		 struct ipi_msg_t *p_ipi_msg);
int send_message_ack(struct ipi_queue_handler_t *handler,
		     struct ipi_msg_t *p_ipi_msg_ack);

#endif /* end of AUDIO_IPI_MSG_QUEUE_H */


