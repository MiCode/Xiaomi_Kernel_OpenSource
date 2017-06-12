/* Copyright (c) 2016-2017, The Linux Foundation. All rights reserved.
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

#ifndef DIAGFWD_GLINK_H
#define DIAGFWD_GLINK_H

#define DIAG_GLINK_NAME_SZ	24
#define GLINK_DRAIN_BUF_SIZE	4096

struct diag_glink_info {
	uint8_t peripheral;
	uint8_t type;
	uint8_t inited;
	atomic_t opened;
	atomic_t diag_state;
	uint32_t fifo_size;
	atomic_t tx_intent_ready;
	void *hdl;
	void *link_state_handle;
	char edge[DIAG_GLINK_NAME_SZ];
	char name[DIAG_GLINK_NAME_SZ];
	struct mutex lock;
	wait_queue_head_t read_wait_q;
	wait_queue_head_t wait_q;
	struct workqueue_struct *wq;
	struct work_struct open_work;
	struct work_struct close_work;
	struct work_struct read_work;
	struct work_struct connect_work;
	struct work_struct remote_disconnect_work;
	struct work_struct late_init_work;
	struct diagfwd_info *fwd_ctxt;
};

extern struct diag_glink_info glink_data[NUM_PERIPHERALS];
extern struct diag_glink_info glink_cntl[NUM_PERIPHERALS];
extern struct diag_glink_info glink_cmd[NUM_PERIPHERALS];
extern struct diag_glink_info glink_dci_cmd[NUM_PERIPHERALS];
extern struct diag_glink_info glink_dci[NUM_PERIPHERALS];

int diag_glink_init_peripheral(uint8_t peripheral);
void diag_glink_exit(void);
int diag_glink_init(void);
void diag_glink_early_exit(void);
void diag_glink_invalidate(void *ctxt, struct diagfwd_info *fwd_ctxt);
int diag_glink_check_state(void *ctxt);

#endif
