/*
 * Copyright (c) 2012-2017, The Linux Foundation. All rights reserved.
 *
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
#include <linux/usb/msm_hsusb.h>
#include <linux/usb/composite.h>
#include <linux/usb/usb_qdss.h>

#include "u_rmnet.h"

struct usb_qdss_bam_connect_info {
	u32 usb_bam_pipe_idx;
	u32 peer_pipe_idx;
	unsigned long usb_bam_handle;
	struct sps_mem_buffer *data_fifo;
};

struct gqdss {
	struct usb_function function;
	struct usb_ep *ctrl_out;
	struct usb_ep *ctrl_in;
	struct usb_ep *data;
	int (*send_encap_cmd)(enum qti_port_type qport, void *buf, size_t len);
	void (*notify_modem)(void *g, enum qti_port_type qport, int cbits);
};

/* struct f_qdss - USB qdss function driver private structure */
struct f_qdss {
	struct gqdss port;
	struct usb_qdss_bam_connect_info bam_info;
	struct usb_gadget *gadget;
	short int port_num;
	u8 ctrl_iface_id;
	u8 data_iface_id;
	int usb_connected;
	bool debug_inface_enabled;
	struct usb_request *endless_req;
	struct usb_qdss_ch ch;
	struct list_head ctrl_read_pool;
	struct list_head ctrl_write_pool;
	struct work_struct connect_w;
	struct work_struct disconnect_w;
	spinlock_t lock;
	unsigned int data_enabled:1;
	unsigned int ctrl_in_enabled:1;
	unsigned int ctrl_out_enabled:1;
	struct workqueue_struct *wq;
};

struct usb_qdss_opts {
	struct usb_function_instance func_inst;
	struct f_qdss *usb_qdss;
	char *channel_name;
};

int uninit_data(struct usb_ep *ep);
int set_qdss_data_connection(struct usb_gadget *gadget,
	struct usb_ep *data_ep, u8 data_addr, int enable);
#endif
