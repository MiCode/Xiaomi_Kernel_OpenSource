/* Copyright (c) 2012-2014, 2018 The Linux Foundation. All rights reserved.
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

#ifndef DIAGFWD_HSIC_H
#define DIAGFWD_HSIC_H

#include <linux/list.h>
#ifdef CONFIG_DIAG_OVER_USB
#include <linux/usb/usbdiag.h>
#endif
#include <linux/usb/diag_bridge.h>

#define HSIC_1			0
#define HSIC_2			1
#define NUM_HSIC_DEV		2

#define DIAG_HSIC_NAME_SZ	24

struct diag_hsic_buf_tbl_t {
	struct list_head link;
	unsigned char *buf;
	int len;
};

struct diag_hsic_info {
	int id;
	int dev_id;
	int mempool;
	uint8_t opened;
	uint8_t enabled;
	uint8_t suspended;
	char name[DIAG_HSIC_NAME_SZ];
	struct work_struct read_work;
	struct work_struct read_complete_work;
	struct work_struct open_work;
	struct work_struct close_work;
	struct workqueue_struct *hsic_wq;
	spinlock_t lock;
	struct list_head buf_tbl;
};

extern struct diag_hsic_info diag_hsic[NUM_HSIC_DEV];

int diag_hsic_init(void);
void diag_hsic_exit(void);

#endif

