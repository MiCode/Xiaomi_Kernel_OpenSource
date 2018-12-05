/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright (c) 2012-2014, 2017-2018 The Linux Foundation. All rights reserved.
 */

#ifndef DIAGFWD_HSIC_H
#define DIAGFWD_HSIC_H
#ifdef CONFIG_DIAG_OVER_USB
#include <linux/usb/usbdiag.h>
#endif
#include <linux/usb/diag_bridge.h>

#define HSIC_1			0
#define HSIC_2			1
#define NUM_HSIC_DEV		2

#define DIAG_HSIC_NAME_SZ	24

struct diag_hsic_info {
	int id;
	int dev_id;
	int mempool;
	uint8_t opened;
	uint8_t enabled;
	uint8_t suspended;
	char name[DIAG_HSIC_NAME_SZ];
	struct work_struct read_work;
	struct work_struct open_work;
	struct work_struct close_work;
	struct workqueue_struct *hsic_wq;
	spinlock_t lock;
};

extern struct diag_hsic_info diag_hsic[NUM_HSIC_DEV];

int diag_hsic_init(void);
void diag_hsic_exit(void);

#endif

