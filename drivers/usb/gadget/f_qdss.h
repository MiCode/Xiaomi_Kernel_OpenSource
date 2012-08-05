/*
 * Copyright (c) 2012, The Linux Foundation. All rights reserved.

 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details
 */

#ifndef _F_QDSS_H
#define _F_QDSS_H

#include <linux/kernel.h>
#include <linux/usb/ch9.h>
#include <linux/usb/gadget.h>

/* struct f_qdss - USB qdss function driver private structure */
struct f_qdss {
	struct usb_function function;
	struct usb_composite_dev *cdev;
	u8 ctrl_iface_id;
	u8 data_iface_id;
	int usb_connected;
	struct usb_request *endless_req;
	struct usb_ep *ctrl_out;
	struct usb_ep *ctrl_in;
	struct usb_ep *data;
	struct usb_qdss_ch ch;
	struct list_head ctrl_read_pool;
	struct list_head ctrl_write_pool;
	struct work_struct qdss_work;
	spinlock_t lock;
	unsigned int data_enabled:1;
	unsigned int ctrl_in_enabled:1;
	unsigned int ctrl_out_enabled:1;
};

#endif

