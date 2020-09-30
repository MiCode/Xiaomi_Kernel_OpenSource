/* SPDX-License-Identifier: GPL-2.0 */
/*
 * adsp_ipi_queue.h --  Mediatek ADSP IPI with queue
 *
 * Copyright (c) 2018 MediaTek Inc.
 */

#ifndef MTK_ADSP_IPI_QUEUE_H
#define MTK_ADSP_IPI_QUEUE_H

#include <linux/types.h>

#define ADSP_IPI_QUEUE_DEFAULT_WAIT_MS (20)

void ipi_queue_init(void);

int dsp_flush_msg_queue(uint32_t dsp_id);

extern int dsp_send_msg_to_queue(
	uint32_t dsp_id, /* enum dsp_id */
	uint32_t ipi_id, /* enum adsp_ipi_id */
	void *buf,
	uint32_t len,
	uint32_t wait_ms);


extern int dsp_dispatch_ipi_hanlder_to_queue(
	uint32_t dsp_id, /* enum dsp_id */
	uint32_t ipi_id, /* enum adsp_ipi_id */
	void *buf,
	uint32_t len,
	void (*ipi_handler)(int ipi_id, void *buf, unsigned int len));

#endif /* end of MTK_ADSP_IPI_QUEUE_H */

