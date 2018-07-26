/* Copyright (c) 2018, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

/* This file was originally distributed by Qualcomm Atheros, Inc.
 * before Copyright ownership was assigned to the Linux Foundation.
 */

#ifndef _HIF_INTERNAL_H_
#define _HIF_INTERNAL_H_

#include "hif.h"
#include "hif_sdio_common.h"

/* Make this large enough to avoid ever failing due to lack of bus requests.
 * A number that accounts for the total number of credits on the Target plus
 * outstanding register requests is good.
 *
 * FUTURE: could dyanamically allocate busrequest structs as needed.
 * FUTURE: would be nice for HIF to use HTCA's htca_request. Seems
 * wasteful to use multiple structures -- one for HTCA and another
 * for HIF -- and to copy info from one to the other. Maybe should
 * semi-merge these layers?
 */
#define BUS_REQUEST_MAX_NUM 128

#define SDIO_CLOCK_FREQUENCY_DEFAULT 25000000 /* TBD: Can support 50000000
					       * on real HW?
					       */
#define SDWLAN_ENABLE_DISABLE_TIMEOUT 20
#define FLAGS_CARD_ENAB 0x02
#define FLAGS_CARD_IRQ_UNMSK 0x04

/* The block size is an attribute of the SDIO function which is
 * shared by all four mailboxes. We cannot support per-mailbox
 * block sizes over SDIO.
 */
#define HIF_MBOX_BLOCK_SIZE HIF_DEFAULT_IO_BLOCK_SIZE
#define HIF_MBOX0_BLOCK_SIZE HIF_MBOX_BLOCK_SIZE
#define HIF_MBOX1_BLOCK_SIZE HIF_MBOX_BLOCK_SIZE
#define HIF_MBOX2_BLOCK_SIZE HIF_MBOX_BLOCK_SIZE
#define HIF_MBOX3_BLOCK_SIZE HIF_MBOX_BLOCK_SIZE

struct bus_request {
	/*struct bus_request*/ void *next; /* link list of available requests */
	struct completion comp_req;
	u32 address; /* request data */
	u8 *buffer;
	u32 length;
	u32 req_type;
	void *context;
	int status;
};

struct hif_device {
	struct sdio_func *func;

	/* Main HIF task */
	struct task_struct *hif_task; /* task to handle SDIO requests */
	wait_queue_head_t hif_wait;
	int hif_task_work;	    /* Signals HIFtask that there is work */
	int hif_shutdown;	    /* signals HIFtask to stop */
	struct completion hif_exit; /* HIFtask completion */

	/* HIF Completion task */
	/* task to handle SDIO completions */
	struct task_struct *completion_task;
	wait_queue_head_t completion_wait;
	int completion_work;
	int completion_shutdown;
	struct completion completion_exit;

	/* pending request queue */
	spinlock_t req_qlock;
	struct bus_request *req_qhead; /* head of request queue */
	struct bus_request *req_qtail; /* tail of request queue */

	/* completed request queue */
	spinlock_t compl_qlock;
	struct bus_request *compl_qhead;
	struct bus_request *compl_qtail;

	/* request free list */
	spinlock_t req_free_qlock;
	struct bus_request *bus_req_free_qhead; /* free queue */

	/* Space for requests, initially queued to busRequestFreeQueue */
	struct bus_request bus_request[BUS_REQUEST_MAX_NUM];

	void *claimed_context;
	struct cbs_from_hif
	    cbs_from_hif; /* Callbacks made from HIF to caller */
	bool is_enabled;  /* device is currently enabled? */
	bool is_intr_enb; /* interrupts are currently unmasked at
			   * Host - dbg only
			   */
	int irq_handling; /* currently processing interrupts */
	const struct sdio_device_id *id;
	struct mmc_host *host;
	void *context;
	bool ctrl_response_timeout;
	/* for debug; links hif device back to caller (e.g.HTCA target) */
	void *caller_handle;
};

#define CMD53_FIXED_ADDRESS 1
#define CMD53_INCR_ADDRESS 2

#endif /* _HIF_INTERNAL_H_ */
