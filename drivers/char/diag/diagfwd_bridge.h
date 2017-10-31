/* Copyright (c) 2012-2014, 2017 The Linux Foundation. All rights reserved.
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

#ifndef DIAGFWD_BRIDGE_H
#define DIAGFWD_BRIDGE_H

/*
 * Add Data channels at the top half and the DCI channels at the
 * bottom half of this list.
 */
#define DIAGFWD_MDM		0
#define DIAGFWD_SMUX		1
#define NUM_REMOTE_DATA_DEV	2
#define DIAGFWD_MDM_DCI		NUM_REMOTE_DATA_DEV
#define NUM_REMOTE_DCI_DEV	(DIAGFWD_MDM_DCI - NUM_REMOTE_DATA_DEV + 1)
#define NUM_REMOTE_DEV		(NUM_REMOTE_DATA_DEV + NUM_REMOTE_DCI_DEV)

#define DIAG_BRIDGE_NAME_SZ	24
#define DIAG_BRIDGE_GET_NAME(x)	(bridge_info[x].name)

struct diag_remote_dev_ops {
	int (*open)(int id);
	int (*close)(int id);
	int (*queue_read)(int id);
	int (*write)(int id, unsigned char *buf, int len, int ctxt);
	int (*fwd_complete)(int id, unsigned char *buf, int len, int ctxt);
};

struct diagfwd_bridge_info {
	int id;
	int type;
	int inited;
	int ctxt;
	char name[DIAG_BRIDGE_NAME_SZ];
	struct diag_remote_dev_ops *dev_ops;
	/* DCI related variables. These would be NULL for data channels */
	void *dci_read_ptr;
	unsigned char *dci_read_buf;
	int dci_read_len;
	struct workqueue_struct *dci_wq;
	struct work_struct dci_read_work;
};

extern struct diagfwd_bridge_info bridge_info[NUM_REMOTE_DEV];
int diagfwd_bridge_init(void);
void diagfwd_bridge_exit(void);
int diagfwd_bridge_close(int id);
int diagfwd_bridge_write(int id, unsigned char *buf, int len);
uint16_t diag_get_remote_device_mask(void);

/* The following functions must be called by Diag remote devices only. */
int diagfwd_bridge_register(int id, int ctxt, struct diag_remote_dev_ops *ops);
int diag_remote_dev_open(int id);
void diag_remote_dev_close(int id);
int diag_remote_dev_read_done(int id, unsigned char *buf, int len);
int diag_remote_dev_write_done(int id, unsigned char *buf, int len, int ctxt);

#endif
