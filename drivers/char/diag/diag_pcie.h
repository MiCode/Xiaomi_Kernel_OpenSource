/* Copyright (c) 2018-2020, The Linux Foundation. All rights reserved.
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

#ifndef DIAGPCIE_H
#define DIAGPCIE_H

#include "diagchar.h"
#include "diag_mux.h"
#include <linux/msm_mhi_dev.h>

#define NUM_DIAG_PCIE_DEV	1
#define DIAG_PCIE_LOCAL	0
#define DIAG_PCIE_NAME_SZ	24
#define DIAG_PCIE_STRING_SZ	30
#define DIAG_MAX_PKT_SZ	16386
#define DIAG_MAX_PCIE_PKT_SZ	2048
enum mhi_chan_dir {
	MHI_DIR_INVALID = 0x0,
	MHI_DIR_OUT = 0x1,
	MHI_DIR_IN = 0x2,
	MHI_DIR__reserved = 0x80000000
};
struct diag_pcie_buf_tbl_t {
	struct list_head track;
	unsigned char *buf;
	uint32_t len;
	atomic_t ref_count;
	int ctxt;
};
struct chan_attr {
	/* SW maintained channel id */
	enum mhi_client_channel chan_id;
	/* maximum buffer size for this channel */
	size_t max_pkt_size;
	/* number of buffers supported in this channel */
	u32 nr_trbs;
	/* direction of the channel, see enum mhi_chan_dir */
	enum mhi_chan_dir dir;
	/* need to register mhi channel state change callback */
	bool register_cb;
	void *read_buffer;
	size_t read_buffer_size;
	/* Name of char device */
	char *device_name;
};
struct diag_pcie_context {
	struct diag_pcie_info *ch;
	int buf_ctxt;
	void *buf;
};
struct diag_pcie_info {
	int id;
	int dev_id;
	int mempool;
	int ctxt;
	int mempool_init;
	struct mutex in_chan_lock;
	struct mutex out_chan_lock;
	u32 out_chan;
	/* read channel - always even */
	u32 in_chan;
	struct mhi_dev_client *out_handle;
	struct mhi_dev_client *in_handle;
	struct chan_attr in_chan_attr;
	struct chan_attr out_chan_attr;
	atomic_t diag_state;
	atomic_t enabled;
	unsigned long read_cnt;
	unsigned long write_cnt;
	struct diag_mux_ops *ops;
	unsigned char *read_buf;
	struct list_head buf_tbl;
	spinlock_t write_lock;
	char name[DIAG_PCIE_NAME_SZ];
	struct work_struct read_work;
	struct work_struct open_work;
	struct work_struct close_work;
	struct workqueue_struct *wq;
	spinlock_t lock;
	void (*event_notifier)(struct mhi_dev_client_cb_reason *cb);
};
extern struct diag_pcie_info diag_pcie[NUM_DIAG_PCIE_DEV];
int diag_pcie_register(int id, int ctxt, struct diag_mux_ops *ops);
int diag_pcie_queue_read(int id);
int diag_pcie_write(int id, unsigned char *buf, int len, int ctxt);
void diag_pcie_connect_all(void);
void diag_pcie_disconnect_all(void);
void diag_pcie_connect_device(int id);
void diag_pcie_disconnect_device(int id);
void diag_pcie_exit(int id);
void diag_pcie_write_complete_cb(void *req);
void diag_pcie_read_work_fn(struct work_struct *work);
void diag_pcie_open_work_fn(struct work_struct *work);
void diag_pcie_close_work_fn(struct work_struct *work);
void diag_pcie_ready_cb(struct mhi_dev_client_cb_data *cb_data);
#endif
