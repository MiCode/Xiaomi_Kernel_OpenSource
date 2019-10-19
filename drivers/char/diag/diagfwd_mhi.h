/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright (c) 2014-2019, The Linux Foundation. All rights reserved.
 */

#ifndef DIAGFWD_MHI_H
#define DIAGFWD_MHI_H

#include "diagchar.h"
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
#include <linux/mhi.h>

#define MHI_1			0
#define MHI_DCI_1		1
#define NUM_MHI_DEV		2
#define NUM_MHI_CHAN		2

#define TYPE_MHI_READ_CH	0
#define TYPE_MHI_WRITE_CH	1

#define DIAG_MHI_NAME_SZ	24

/* Below mhi  device ids are from mhi controller */
#define MHI_DEV_ID_1 0x306
#define MHI_DEV_ID_2 0x1101
#define MHI_DEV_ID_3 0x1103

struct diag_mhi_buf_tbl_t {
	struct list_head link;
	unsigned char *buf;
	int len;
};

struct diag_mhi_ch_t {
	uint8_t type;
	spinlock_t lock;
	atomic_t opened;
	struct list_head buf_tbl;
};

struct diag_mhi_info {
	int id;
	int dev_id;
	int mempool;
	int mempool_init;
	int num_read;
	uint8_t enabled;
	struct mhi_device *mhi_dev;
	char name[DIAG_MHI_NAME_SZ];
	struct work_struct read_work;
	struct list_head read_done_list;
	struct work_struct read_done_work;
	struct work_struct open_work;
	struct work_struct close_work;
	struct workqueue_struct *mhi_wq;
	wait_queue_head_t mhi_wait_q;
	struct diag_mhi_ch_t read_ch;
	struct diag_mhi_ch_t write_ch;
	struct mutex ch_mutex;
	spinlock_t lock;
};

extern struct diag_mhi_info diag_mhi[NUM_MHI_DEV][NUM_MHI_CHAN];
int diag_mhi_init(void);
void diag_mhi_exit(void);
void diag_register_with_mhi(void);
#endif
