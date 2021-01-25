/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright (c) 2017-2018, 2021, The Linux Foundation. All rights reserved.
 */

#ifndef DIAGFWD_RPMSG_H
#define DIAGFWD_RPMSG_H

#define DIAG_RPMSG_NAME_SZ	24
#define RPMSG_DRAIN_BUF_SIZE	4096

struct diag_rpmsg_info {
	uint8_t peripheral;
	uint8_t type;
	uint8_t inited;
	atomic_t opened;
	atomic_t diag_state;
	uint32_t fifo_size;
	struct rpmsg_device *hdl;
	char edge[DIAG_RPMSG_NAME_SZ];
	char name[DIAG_RPMSG_NAME_SZ];
	struct mutex lock;
	wait_queue_head_t read_wait_q;
	wait_queue_head_t wait_q;
	struct workqueue_struct *wq;
	struct work_struct open_work;
	struct work_struct close_work;
	struct work_struct read_work;
	struct work_struct late_init_work;
	struct diagfwd_info *fwd_ctxt;
	void *buf1;
	void *buf2;
};

extern struct diag_rpmsg_info rpmsg_data[NUM_PERIPHERALS];
extern struct diag_rpmsg_info rpmsg_cntl[NUM_PERIPHERALS];
extern struct diag_rpmsg_info rpmsg_cmd[NUM_PERIPHERALS];
extern struct diag_rpmsg_info rpmsg_dci_cmd[NUM_PERIPHERALS];
extern struct diag_rpmsg_info rpmsg_dci[NUM_PERIPHERALS];

int diag_rpmsg_init_peripheral(uint8_t peripheral);
void diag_rpmsg_exit(void);
int diag_rpmsg_init(void);
void diag_rpmsg_early_exit(void);
void diag_rpmsg_invalidate(void *ctxt, struct diagfwd_info *fwd_ctxt);
int diag_rpmsg_check_state(void *ctxt);
void rpmsg_mark_buffers_free(uint8_t peripheral, uint8_t type, int buf_num);

#endif
