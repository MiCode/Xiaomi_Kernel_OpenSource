/*
 * Copyright (C) 2017 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef _MUSBFSH_HOST_H
#define _MUSBFSH_HOST_H

#include <linux/scatterlist.h>

#ifdef MTK_USB_RUNTIME_SUPPORT
extern void mt_eint_unmask(unsigned int line);
extern void mt_eint_mask(unsigned int line);
#endif
#ifdef MTK_ICUSB_SUPPORT
extern struct usb_interface *stor_intf;
#endif
#ifdef MTK_USB_RUNTIME_SUPPORT
extern void mt_eint_unmask(unsigned int line);
extern void mt_eint_mask(unsigned int line);
extern void request_wakeup_md_timeout(unsigned int dev_id,
					unsigned int dev_sub_id);
#endif

#ifdef MTK_ICUSB_SUPPORT
extern struct my_attr skip_mac_init_attr;
#endif

static inline struct usb_hcd *musbfsh_to_hcd(struct musbfsh *musb)
{
	return container_of((void *) musb, struct usb_hcd, hcd_priv);
}

static inline struct musbfsh *hcd_to_musbfsh(struct usb_hcd *hcd)
{
	return (struct musbfsh *) (hcd->hcd_priv);
}

/* stored in "usb_host_endpoint.hcpriv" for scheduled endpoints */
struct musbfsh_qh {
	struct usb_host_endpoint *hep;		/* usbcore info */
	struct usb_device	*dev;
	struct musbfsh_hw_ep	*hw_ep;		/* current binding */

	struct list_head	ring;		/* of musbfsh_qh */
	/* struct musbfsh_qh		*next; */	/* for periodic tree */
	u8			mux;		/* qh multiplexed to hw_ep */

	unsigned int offset;		/* in urb->transfer_buffer */
	unsigned int segsize;	/* current xfer fragment */

	u8			type_reg;	/* {rx,tx} type register */
	u8			intv_reg;	/* {rx,tx} interval register */
	u8			addr_reg;	/* device address register */
	u8			h_addr_reg;	/* hub address register */
	u8			h_port_reg;	/* hub port register */

	u8			is_ready;	/* safe to modify hw_ep */
	u8			type;		/* XFERTYPE_* */
	u8			epnum;
	u8			hb_mult;	/* high bandwidth pkts per uf */
	u16			maxpacket;
	u16			frame;		/* for periodic schedule */
	unsigned int iso_idx;	/* in urb->iso_frame_desc[] */
	struct sg_mapping_iter sg_miter;	/* for highmem in PIO mode */
};

/* map from control or bulk queue head to the first qh on that ring */
static inline struct musbfsh_qh *first_qh(struct list_head *q)
{
	if (list_empty(q))
		return NULL;
	return list_entry(q->next, struct musbfsh_qh, ring);
}


extern void musbfsh_root_disconnect(struct musbfsh *musb);

struct usb_hcd;

extern int musbfsh_hub_status_data(struct usb_hcd *hcd, char *buf);
extern int musbfsh_hub_control(struct usb_hcd *hcd,
			u16 typeReq, u16 wValue, u16 wIndex,
			char *buf, u16 wLength);

extern struct hc_driver musbfsh_hc_driver;

static inline struct urb *next_urb(struct musbfsh_qh *qh)
{
	struct list_head	*queue;

	if (!qh)
		return NULL;
	queue = &qh->hep->urb_list;
	if (list_empty(queue))
		return NULL;
	return list_entry(queue->next, struct urb, urb_list);
}

#endif				/* _MUSBFSH_HOST_H */
