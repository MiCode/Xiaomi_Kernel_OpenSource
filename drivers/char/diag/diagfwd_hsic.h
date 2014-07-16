/* Copyright (c) 2012-2014, The Linux Foundation. All rights reserved.
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

#include <linux/usb/diag_bridge.h>

#define N_MDM_WRITE	8
#define N_MDM_READ	1
#define NUM_HSIC_BUF_TBL_ENTRIES N_MDM_WRITE
#define REOPEN_HSIC 1
#define DONT_REOPEN_HSIC 0
#define HSIC_DATA_TYPE		0
#define HSIC_DCI_TYPE		1

int diagfwd_write_complete_hsic(struct diag_request *, int index);
int diagfwd_cancel_hsic(int reopen);
void diag_read_usb_hsic_work_fn(struct work_struct *work);
void diag_usb_read_complete_hsic_fn(struct work_struct *w);
extern struct diag_bridge_ops hsic_diag_bridge_ops[MAX_HSIC_DATA_CH];
extern struct diag_bridge_ops hsic_diag_dci_bridge_ops[MAX_HSIC_DCI_CH];
extern struct platform_driver msm_hsic_ch_driver;
void diag_hsic_close(int);
void diag_hsic_dci_close(int);

/*
 * Structure to hold the HSIC channel mapping information.
 * @dev_id: platform device ID
 * @type: HSIC type - HSIC_DATA_TYPE or HSIC_DCI_TYPE
 * @struct_idx: Index to the corresponding HSIC structure
 * @bridge_idx: Index to the corresponding Bridge Entry
 */
struct diag_hsic_bridge_map {
	uint8_t dev_id;
	uint8_t type;
	uint8_t struct_idx;
	uint8_t bridge_idx;
};

extern int hsic_data_bridge_map[MAX_HSIC_DATA_CH];
extern int hsic_dci_bridge_map[MAX_HSIC_DCI_CH];

/*
 * Diag-HSIC structure, n HSIC bridges can be used at same time
 * This structure is used for HSIC data channels.
 */
struct diag_hsic_dev {
	int id;
	int hsic_ch;
	int hsic_inited;
	int hsic_device_enabled;
	int hsic_device_opened;
	int hsic_suspend;
	int hsic_data_requested;
	int in_busy_hsic_read_on_device;
	int in_busy_hsic_write;
	struct work_struct diag_read_hsic_work;
	unsigned int poolsize_hsic_write;
	int num_hsic_buf_tbl_entries;
	struct diag_write_device *hsic_buf_tbl;
	spinlock_t hsic_spinlock;
};

/*
 * Diag-HSIC DCI structure, n HSIC bridges can be used at same time
 * This structure is used for HSIC DCI channels.
 */
struct diag_hsic_dci_dev {
	int id;
	int hsic_ch;
	int hsic_inited;
	int hsic_device_enabled;
	int hsic_device_opened;
	int hsic_suspend;
	int in_busy_hsic_write;
	struct work_struct diag_read_hsic_work;
	unsigned char *data;
	unsigned char *data_buf;
	uint32_t data_len;
	struct work_struct diag_process_hsic_work;
};

#endif
