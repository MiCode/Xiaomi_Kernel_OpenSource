/* Copyright (c) 2012, The Linux Foundation. All rights reserved.
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

#ifndef __LINUX_USB_QDSS_H
#define __LINUX_USB_QDSS_H

#include <linux/kernel.h>

struct qdss_request {
	char *buf;
	int length;
	int actual;
	int status;
	void *context;
};

struct usb_qdss_ch {
	const char *name;
	struct list_head list;
	void (*notify)(void *priv, unsigned event, struct qdss_request *d_req,
		struct usb_qdss_ch *);
	void *priv;
	void *priv_usb;
	int app_conn;
};

enum qdss_state {
	USB_QDSS_CONNECT,
	USB_QDSS_DISCONNECT,
	USB_QDSS_CTRL_READ_DONE,
	USB_QDSS_DATA_WRITE_DONE,
	USB_QDSS_CTRL_WRITE_DONE,
};

struct usb_qdss_ch *usb_qdss_open(const char *name, void *priv,
	void (*notify)(void *, unsigned, struct qdss_request *,
		struct usb_qdss_ch *));
void usb_qdss_close(struct usb_qdss_ch *ch);
int usb_qdss_alloc_req(struct usb_qdss_ch *ch, int n_write, int n_read);
void usb_qdss_free_req(struct usb_qdss_ch *ch);
int usb_qdss_read(struct usb_qdss_ch *ch, struct qdss_request *d_req);
int usb_qdss_write(struct usb_qdss_ch *ch, struct qdss_request *d_req);
int usb_qdss_ctrl_write(struct usb_qdss_ch *ch, struct qdss_request *d_req);
int usb_qdss_ctrl_read(struct usb_qdss_ch *ch, struct qdss_request *d_req);

#endif
