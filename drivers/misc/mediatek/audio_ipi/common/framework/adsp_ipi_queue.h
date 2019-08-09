/*
 * Copyright (C) 2017 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef MTK_ADSP_IPI_QUEUE_H
#define MTK_ADSP_IPI_QUEUE_H

#include <linux/types.h>

#define ADSP_IPI_QUEUE_DEFAULT_WAIT_MS (20)



int scp_ipi_queue_init(uint32_t opendsp_id); /* enum opendsp_id */
bool is_scp_ipi_queue_init(const uint32_t opendsp_id);

int scp_flush_msg_queue(uint32_t opendsp_id);

int scp_send_msg_to_queue(
	uint32_t opendsp_id, /* enum opendsp_id */
	uint32_t ipi_id, /* enum adsp_ipi_id */
	void *buf,
	uint32_t len,
	uint32_t wait_ms);


int scp_dispatch_ipi_hanlder_to_queue(
	uint32_t opendsp_id, /* enum opendsp_id */
	uint32_t ipi_id, /* enum adsp_ipi_id */
	void *buf,
	uint32_t len,
	void (*ipi_handler)(int ipi_id, void *buf, unsigned int len));


#endif /* end of MTK_ADSP_IPI_QUEUE_H */



