/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2012-2020, The Linux Foundation. All rights reserved.
 */

#ifndef _F_QDSS_H
#define _F_QDSS_H

#include <linux/kernel.h>
#include <linux/ipc_logging.h>
#include <linux/usb/ch9.h>
#include <linux/usb/gadget.h>
#include <linux/usb/composite.h>
#include <linux/usb/usb_qdss.h>

enum qti_port_type {
	QTI_PORT_RMNET,
	QTI_PORT_DPL,
	QTI_NUM_PORTS
};

struct usb_qdss_bam_connect_info {
	u32 usb_bam_pipe_idx;
	u32 peer_pipe_idx;
	unsigned long usb_bam_handle;
	struct sps_mem_buffer *data_fifo;
	unsigned long qdss_bam_iova;
	phys_addr_t qdss_bam_phys;
	u32 qdss_bam_size;
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
	struct list_head ctrl_write_pool;

	/* for mdm channel SW path */
	struct list_head data_write_pool;
	struct list_head queued_data_pool;

	struct work_struct connect_w;
	struct work_struct disconnect_w;
	spinlock_t lock;
	unsigned int data_enabled:1;
	unsigned int ctrl_in_enabled:1;
	unsigned int ctrl_out_enabled:1;
	struct workqueue_struct *wq;
	bool qdss_close;
};

static void *_qdss_ipc_log;

#define NUM_PAGES	10 /* # of pages for ipc logging */

#ifdef CONFIG_DYNAMIC_DEBUG
#define qdss_log(fmt, ...) do { \
	ipc_log_string(_qdss_ipc_log, "%s: " fmt,  __func__, ##__VA_ARGS__); \
	dynamic_pr_debug("%s: " fmt, __func__, ##__VA_ARGS__); \
} while (0)
#else
#define qdss_log(fmt, ...) \
	ipc_log_string(_qdss_ipc_log, "%s: " fmt,  __func__, ##__VA_ARGS__)
#endif

struct usb_qdss_opts {
	struct usb_function_instance func_inst;
	struct f_qdss *usb_qdss;
	char *channel_name;
};

int uninit_data(struct usb_ep *ep);
int set_qdss_data_connection(struct f_qdss *qdss, int enable);
#endif
