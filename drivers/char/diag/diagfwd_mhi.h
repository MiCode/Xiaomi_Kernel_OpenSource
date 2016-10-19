/* Copyright (c) 2014-2016, The Linux Foundation. All rights reserved.
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

#ifndef DIAGFWD_MHI_H
#define DIAGFWD_MHI_H

#include "diagchar.h"
#include <linux/msm_mhi.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/completion.h>
#include <linux/fs.h>
#include <linux/types.h>
#include <linux/cdev.h>
#include <linux/list.h>
#include <linux/wait.h>
#include <linux/pci.h>
#include <linux/vmalloc.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/poll.h>
#include <linux/sched.h>
#include <linux/platform_device.h>
#include <linux/tty.h>
#include <linux/delay.h>
#include <linux/ipc_logging.h>
#include <linux/msm_mhi.h>

#define MHI_1			0
#define MHI_DCI_1		1
#define NUM_MHI_DEV		2

#define TYPE_MHI_READ_CH	0
#define TYPE_MHI_WRITE_CH	1

#define DIAG_MHI_NAME_SZ	24

struct diag_mhi_buf_tbl_t {
	struct list_head link;
	unsigned char *buf;
	int len;
};

struct diag_mhi_ch_t {
	uint8_t type;
	u32 channel;
	enum MHI_CLIENT_CHANNEL chan;
	atomic_t opened;
	spinlock_t lock;
	struct mhi_client_info_t client_info;
	struct mhi_client_handle *hdl;
	struct list_head buf_tbl;
};

struct diag_mhi_info {
	int id;
	int dev_id;
	int mempool;
	int mempool_init;
	int num_read;
	uint8_t enabled;
	char name[DIAG_MHI_NAME_SZ];
	struct work_struct read_work;
	struct work_struct read_done_work;
	struct work_struct open_work;
	struct work_struct close_work;
	struct workqueue_struct *mhi_wq;
	wait_queue_head_t mhi_wait_q;
	struct diag_mhi_ch_t read_ch;
	struct diag_mhi_ch_t write_ch;
	spinlock_t lock;
};

extern struct diag_mhi_info diag_mhi[NUM_MHI_DEV];

int diag_mhi_init(void);
void diag_mhi_exit(void);

#endif
