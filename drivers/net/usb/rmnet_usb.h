/* Copyright (c) 2011-2013, The Linux Foundation. All rights reserved.
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

#ifndef __RMNET_USB_H
#define __RMNET_USB_H

#include <linux/mutex.h>
#include <linux/usb.h>
#include <linux/cdev.h>
#include <linux/usb/ch9.h>
#include <linux/usb/cdc.h>

#define MAX_RMNET_DEVS		4
#define MAX_RMNET_INSTS_PER_DEV	17
#define TOTAL_RMNET_DEV_COUNT	(MAX_RMNET_DEVS * MAX_RMNET_INSTS_PER_DEV)

#define CTRL_DEV_MAX_LEN	10

#define RMNET_CTRL_DEV_OPEN	0
#define RMNET_CTRL_DEV_READY	1
#define RMNET_CTRL_DEV_MUX_EN	2

/*data MUX header bit mask*/
#define MUX_PAD_SHIFT	0x2

/*big endian format ctrl MUX header bit masks*/
#define MUX_CTRL_PADLEN_MASK	0x3F
#define MUX_CTRL_MASK	0x80

/*max padding bytes for n byte alignment*/
#define MAX_PAD_BYTES(n)	(n-1)

/*
 *MUX Header big endian Format
 *BIT 0 - 5 : Pad bytes
 *BIT 6: Reserved
 *BIT 7: Mux type 0: Data, 1: control
 *BIT 8-15: Mux ID
 *BIT 16-31: PACKET_LEN_WITH_PADDING (Bytes)
 */
struct mux_hdr {
	__u8	padding_info;
	__u8	mux_id;
	__u16	pkt_len_w_padding;
} __packed;

struct rmnet_ctrl_dev {

	/*for debugging purpose*/
	char			name[CTRL_DEV_MAX_LEN];

	struct cdev		cdev;
	struct device		*devicep;
	unsigned		ch_id;

	/*to identify the usb device*/
	unsigned		id;

	struct usb_interface	*intf;
	unsigned int		int_pipe;
	struct urb		*rcvurb;
	struct urb		*inturb;
	struct usb_anchor	tx_submitted;
	struct usb_anchor	rx_submitted;
	void			*rcvbuf;
	void			*intbuf;
	struct usb_ctrlrequest	*in_ctlreq;

	spinlock_t		rx_lock;
	struct mutex		dev_lock;
	struct list_head	rx_list;
	wait_queue_head_t	read_wait_queue;
	wait_queue_head_t	open_wait_queue;

	struct workqueue_struct	*wq;
	struct work_struct	get_encap_work;

	unsigned long		status;

	bool			claimed;

	unsigned int		mdm_wait_timeout;

	/*input control lines (DSR, CTS, CD, RI)*/
	unsigned int		cbits_tolocal;
	/*output control lines (DTR, RTS)*/
	unsigned int		cbits_tomdm;

	/*counters*/
	unsigned int		snd_encap_cmd_cnt;
	unsigned int		get_encap_resp_cnt;
	unsigned int		resp_avail_cnt;
	unsigned int		get_encap_failure_cnt;
	unsigned int		set_ctrl_line_state_cnt;
	unsigned int		tx_ctrl_err_cnt;
	unsigned int		zlp_cnt;
};

extern struct workqueue_struct	*usbnet_wq;

extern int rmnet_usb_ctrl_start_rx(struct rmnet_ctrl_dev *);
extern int rmnet_usb_ctrl_suspend(struct rmnet_ctrl_dev *dev);
extern int rmnet_usb_ctrl_init(int num_devs, int insts_per_dev);
extern void rmnet_usb_ctrl_exit(int num_devs, int insts_per_dev);
extern int rmnet_usb_ctrl_probe(struct usb_interface *intf,
				struct usb_host_endpoint *int_in,
				unsigned long rmnet_devnum,
				unsigned long *data);
extern void rmnet_usb_ctrl_disconnect(struct rmnet_ctrl_dev *);

#endif /* __RMNET_USB_H*/
