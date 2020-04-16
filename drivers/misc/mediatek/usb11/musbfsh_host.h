/*
 * MUSB OTG driver host defines
 *
 * Copyright 2005 Mentor Graphics Corporation
 * Copyright (C) 2005-2006 by Texas Instruments
 * Copyright (C) 2006-2007 Nokia Corporation
 *
 * Copyright 2015 Mediatek Inc.
 *	Marvin Lin <marvin.lin@mediatek.com>
 *	Arvin Wang <arvin.wang@mediatek.com>
 *	Vincent Fan <vincent.fan@mediatek.com>
 *	Bryant Lu <bryant.lu@mediatek.com>
 *	Yu-Chang Wang <yu-chang.wang@mediatek.com>
 *	Macpaul Lin <macpaul.lin@mediatek.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.
 *
 * THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN
 * NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
 * USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#ifndef _MUSBFSH_HOST_H
#define _MUSBFSH_HOST_H

#include <linux/scatterlist.h>

static inline struct usb_hcd *musbfsh_to_hcd(struct musbfsh *musb)
{
	return container_of((void *)musb, struct usb_hcd, hcd_priv);
}

static inline struct musbfsh *hcd_to_musbfsh(struct usb_hcd *hcd)
{
	return (struct musbfsh *)(hcd->hcd_priv);
}

/* stored in "usb_host_endpoint.hcpriv" for scheduled endpoints */
struct musbfsh_qh {
	struct usb_host_endpoint *hep;	/* usbcore info */
	struct usb_device *dev;
	struct musbfsh_hw_ep *hw_ep;	/* current binding */

	struct list_head ring;	/* of musbfsh_qh */
	/* struct musbfsh_qh            *next; *//* for periodic tree */
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
#ifdef CONFIG_MTK_MUSBFSH_QMU_SUPPORT
	u8 is_use_qmu;
#endif
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
			       u16 typeReq, u16 wValue, u16 wIndex, char *buf,
			       u16 wLength);

extern struct hc_driver musbfsh_hc_driver;

static inline struct urb *next_urb(struct musbfsh_qh *qh)
{
	struct list_head *queue;

	if (!qh)
		return NULL;
	queue = &qh->hep->urb_list;
	if (list_empty(queue))
		return NULL;
	return list_entry(queue->next, struct urb, urb_list);
}

/* file include host.h for get icusb related struct */
#ifdef CONFIG_MTK_ICUSB_SUPPORT
#include "musbfsh_icusb.h"
#endif

#ifdef CONFIG_MTK_DT_USB_SUPPORT
extern int musbfsh_connect_flag;

/* This should be in platform musbfsh_mt65xx.h */
#endif

#ifdef MTK_USB_RUNTIME_SUPPORT
#include <cust_eint.h>
extern void mt_eint_unmask(unsigned int line);
extern void mt_eint_mask(unsigned int line);
extern void request_wakeup_md_timeout(unsigned int dev_id,
				      unsigned int dev_sub_id);
#endif

#ifdef CONFIG_MTK_DT_USB_SUPPORT
extern void request_wakeup_md_timeout(unsigned int dev_id,
	unsigned int dev_sub_id);
extern int musbfsh_skip_port_suspend;
extern int musbfsh_skip_port_resume;
#endif

#ifdef CONFIG_MTK_MUSBFSH_QMU_SUPPORT
extern void musbfsh_ep_set_qh(struct musbfsh_hw_ep *ep,
	int isRx, struct musbfsh_qh *qh);
extern struct musbfsh_qh *musbfsh_ep_get_qh(struct musbfsh_hw_ep *ep, int isRx);
extern void musbfsh_advance_schedule(struct musbfsh *musb, struct urb *urb,
				  struct musbfsh_hw_ep *hw_ep, int is_in);
extern u16 musbfsh_h_flush_rxfifo(struct musbfsh_hw_ep *hw_ep, u16 csr);
extern void musbfsh_h_tx_flush_fifo(struct musbfsh_hw_ep *ep);
#endif
#endif				/* _MUSBFSH_HOST_H */
