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


