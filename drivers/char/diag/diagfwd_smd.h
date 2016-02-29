/* Copyright (c) 2015, The Linux Foundation. All rights reserved.
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

#ifndef DIAGFWD_SMD_H
#define DIAGFWD_SMD_H

#define DIAG_SMD_NAME_SZ	24
#define SMD_DRAIN_BUF_SIZE	4096

struct diag_smd_info {
	uint8_t peripheral;
	uint8_t type;
	uint8_t inited;
	atomic_t opened;
	atomic_t diag_state;
	uint32_t fifo_size;
	smd_channel_t *hdl;
	char name[DIAG_SMD_NAME_SZ];
	struct mutex lock;
	wait_queue_head_t read_wait_q;
	struct workqueue_struct *wq;
	struct work_struct open_work;
	struct work_struct close_work;
	struct work_struct read_work;
	struct diagfwd_info *fwd_ctxt;
};

extern struct diag_smd_info smd_data[NUM_PERIPHERALS];
extern struct diag_smd_info smd_cntl[NUM_PERIPHERALS];
extern struct diag_smd_info smd_dci[NUM_PERIPHERALS];
extern struct diag_smd_info smd_cmd[NUM_PERIPHERALS];
extern struct diag_smd_info smd_dci_cmd[NUM_PERIPHERALS];

int diag_smd_init_peripheral(uint8_t peripheral);
void diag_smd_exit(void);
int diag_smd_init(void);
void diag_smd_early_exit(void);
void diag_smd_invalidate(void *ctxt, struct diagfwd_info *fwd_ctxt);
int diag_smd_check_state(void *ctxt);

#endif
