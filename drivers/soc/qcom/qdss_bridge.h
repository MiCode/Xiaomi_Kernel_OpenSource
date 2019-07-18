/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2017-2019, The Linux Foundation. All rights reserved.
 */

#ifndef _QDSS_BRIDGE_H
#define _QDSS_BRIDGE_H

struct qdss_buf_tbl_lst {
	struct list_head link;
	unsigned char *buf;
	struct qdss_request *usb_req;
	atomic_t available;
};

struct qdss_mhi_buf_tbl_t {
	struct list_head link;
	unsigned char *buf;
	size_t len;
};

enum mhi_transfer_mode {
	MHI_TRANSFER_TYPE_USB,
	MHI_TRANSFER_TYPE_UCI,
};

enum open_status {
	DISABLE,
	ENABLE,
	SSR,
};

enum mhi_ch {
	QDSS,
	QDSS_HW,
	EMPTY,
};

struct qdss_bridge_drvdata {
	int alias;
	int nr_trbs;
	enum open_status opened;
	struct completion completion;
	size_t mtu;
	enum mhi_transfer_mode mode;
	spinlock_t lock;
	struct device *dev;
	struct cdev cdev;
	struct mhi_device *mhi_dev;
	struct work_struct read_work;
	struct work_struct read_done_work;
	struct work_struct open_work;
	struct work_struct close_work;
	struct workqueue_struct *mhi_wq;
	struct mhi_client_handle *hdl;
	struct mhi_client_info_t *client_info;
	struct list_head buf_tbl;
	struct list_head mhi_buf_tbl;
	struct list_head read_done_list;
	struct usb_qdss_ch *usb_ch;
	struct qdss_mhi_buf_tbl_t *cur_buf;
	wait_queue_head_t uci_wq;
	size_t rx_size;
};

#endif
