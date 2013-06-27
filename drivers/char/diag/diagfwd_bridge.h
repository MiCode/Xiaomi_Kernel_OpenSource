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

#ifndef DIAGFWD_BRIDGE_H
#define DIAGFWD_BRIDGE_H

#include "diagfwd.h"

#define MAX_BRIDGES	5
#define HSIC	0
#define HSIC_2	1
#define SMUX	4

int diagfwd_connect_bridge(int);
void connect_bridge(int, uint8_t);
int diagfwd_disconnect_bridge(int);
void diagfwd_bridge_init(int index);
void diagfwd_bridge_exit(void);
int diagfwd_read_complete_bridge(struct diag_request *diag_read_ptr);

/* Diag-Bridge structure, n bridges can be used at same time
 * for instance SMUX, HSIC working at same time
 */
struct diag_bridge_dev {
	int id;
	char name[20];
	int enabled;
	struct mutex bridge_mutex;
	int usb_connected;
	int read_len;
	int write_len;
	unsigned char *usb_buf_out;
	struct usb_diag_ch *ch;
	struct workqueue_struct *wq;
	struct work_struct diag_read_work;
	struct diag_request *usb_read_ptr;
	struct work_struct usb_read_complete_work;
};

#endif
