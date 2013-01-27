/* Copyright (c) 2012-2013, The Linux Foundation. All rights reserved.
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

#include <mach/diag_bridge.h>

#define N_MDM_WRITE	8
#define N_MDM_READ	1
#define NUM_HSIC_BUF_TBL_ENTRIES N_MDM_WRITE
#define MAX_HSIC_CH	4
int diagfwd_write_complete_hsic(struct diag_request *, int index);
int diagfwd_cancel_hsic(void);
void diag_read_usb_hsic_work_fn(struct work_struct *work);
void diag_usb_read_complete_hsic_fn(struct work_struct *w);
extern struct diag_bridge_ops hsic_diag_bridge_ops[MAX_HSIC_CH];
extern struct platform_driver msm_hsic_ch_driver;
void diag_hsic_close(int);

/* Diag-HSIC structure, n HSIC bridges can be used at same time
 * for instance HSIC(0), HS-USB(1) working at same time
 */
struct diag_hsic_dev {
	int id;
	int hsic_ch;
	int hsic_inited;
	int hsic_device_enabled;
	int hsic_device_opened;
	int hsic_suspend;
	int in_busy_hsic_read_on_device;
	int in_busy_hsic_write;
	struct work_struct diag_read_hsic_work;
	int count_hsic_pool;
	int count_hsic_write_pool;
	unsigned int poolsize_hsic;
	unsigned int poolsize_hsic_write;
	unsigned int itemsize_hsic;
	unsigned int itemsize_hsic_write;
	mempool_t *diag_hsic_pool;
	mempool_t *diag_hsic_write_pool;
	int num_hsic_buf_tbl_entries;
	struct diag_write_device *hsic_buf_tbl;
	spinlock_t hsic_spinlock;
};

#endif
