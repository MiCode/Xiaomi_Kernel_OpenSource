/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2017 MediaTek Inc.
 */

#ifndef _MUSB_HOST_H
#define _MUSB_HOST_H

#include <linux/scatterlist.h>

static inline struct usb_hcd *musb_to_hcd(struct musb *musb)
{
	return container_of((void *)musb, struct usb_hcd, hcd_priv);
}

static inline struct musb *hcd_to_musb(struct usb_hcd *hcd)
{
	return (struct musb *)(hcd->hcd_priv);
}

/* stored in "usb_host_endpoint.hcpriv" for scheduled endpoints */
struct musb_qh {
	struct usb_host_endpoint *hep;	/* usbcore info */
	struct usb_device *dev;
	struct musb_hw_ep *hw_ep;	/* current binding */

	struct list_head ring;	/* of musb_qh */
	/* struct musb_qh               *next; *//* for periodic tree */
	u8 mux;			/* qh multiplexed to hw_ep */

	unsigned int offset;	/* in urb->transfer_buffer */
	unsigned int segsize;	/* current xfer fragment */

	u8 type_reg;		/* {rx,tx} type register */
	u8 intv_reg;		/* {rx,tx} interval register */
	u8 addr_reg;		/* device address register */
	u8 h_addr_reg;		/* hub address register */
	u8 h_port_reg;		/* hub port register */

	u8 is_ready;		/* safe to modify hw_ep */
	u8 type;		/* XFERTYPE_* */
	u8 epnum;
	u8 hb_mult;		/* high bandwidth pkts per uf */
	u16 maxpacket;
	u16 frame;		/* for periodic schedule */
	unsigned int iso_idx;	/* in urb->iso_frame_desc[] */
	struct sg_mapping_iter sg_miter;	/* for highmem in PIO mode */
#ifdef CONFIG_MTK_MUSB_QMU_SUPPORT
	u8 is_use_qmu;
#endif
	bool db_used;
};

/* map from control or bulk queue head to the first qh on that ring */
static inline struct musb_qh *first_qh(struct list_head *q)
{
	if (list_empty(q))
		return NULL;
	return list_entry(q->next, struct musb_qh, ring);
}

void musb_h_pre_disable(struct musb *musb);

extern void musb_root_disconnect(struct musb *musb);

struct usb_hcd;

extern int musb_hub_status_data(struct usb_hcd *hcd, char *buf);
extern int musb_hub_control(struct usb_hcd *hcd,
			    u16 typeReq, u16 wValue,
			    u16 wIndex, char *buf,
				u16 wLength);

extern const struct hc_driver musb_hc_driver;

static inline struct urb *next_urb(struct musb_qh *qh)
{
	struct list_head *queue;

	if (!qh)
		return NULL;
	queue = &qh->hep->urb_list;
	if (list_empty(queue))
		return NULL;
	return list_entry(queue->next, struct urb, urb_list);
}
extern u16 musb_h_flush_rxfifo(struct musb_hw_ep *hw_ep, u16 csr);

#ifdef CONFIG_MTK_MUSB_QMU_SUPPORT
extern void musb_ep_set_qh(struct musb_hw_ep *ep, int isRx, struct musb_qh *qh);
extern struct musb_qh *musb_ep_get_qh(struct musb_hw_ep *ep, int isRx);
extern void musb_advance_schedule(struct musb *musb, struct urb *urb,
				  struct musb_hw_ep *hw_ep, int is_in);
#endif
enum {
	QH_FREE_RESCUE_INTERRUPT,
	QH_FREE_RESCUE_EP_DISABLE,
};
#endif				/* _MUSB_HOST_H */
