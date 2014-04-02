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

#ifndef DIAGFWD_BRIDGE_H
#define DIAGFWD_BRIDGE_H

#include "diagfwd.h"

#define MAX_BRIDGES_DATA	3
#define MAX_BRIDGES_DCI		2
#define HSIC_DATA_CH		0
#define HSIC_DATA_CH_2		1
#define HSIC_DCI_CH		0
#define HSIC_DCI_CH_2		1
#define SMUX			2

#define DIAG_DATA_BRIDGE_IDX	0
#define DIAG_DCI_BRIDGE_IDX	1
#define DIAG_DATA_BRIDGE_IDX_2	2
#define DIAG_DCI_BRIDGE_IDX_2	3

int diagfwd_bridge_init(int index);
int diagfwd_bridge_dci_init(int index);
void diagfwd_bridge_exit(void);
void diagfwd_bridge_dci_exit(void);

/* Diag-Bridge structure, n bridges can be used at same time
 * for instance SMUX, HSIC working at same time
 */
struct diag_bridge_dev {
	int id;
	int usb_id;
	char name[20];
	int enabled;
	struct mutex bridge_mutex;
	int usb_connected;
	int read_len;
	struct workqueue_struct *wq;
};

struct diag_bridge_dci_dev {
	int id;
	char name[20];
	int enabled;
	struct mutex bridge_mutex;
	int read_len;
	int write_len;
	struct workqueue_struct *wq;
	struct work_struct read_complete_work;
};

#endif
