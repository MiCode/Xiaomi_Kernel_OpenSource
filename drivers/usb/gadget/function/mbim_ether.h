/*
 * u_ether.h -- interface to USB gadget "ethernet link" utilities
 *
 * Copyright (C) 2003-2005,2008 David Brownell
 * Copyright (C) 2003-2004 Robert Schwebel, Benedikt Spranger
 * Copyright (C) 2008 Nokia Corporation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef __MBIM_ETHER_H
#define __MBIM_ETHER_H

#include <linux/err.h>
#include <linux/if_ether.h>
#include <linux/usb/composite.h>
#include <linux/usb/cdc.h>

#include "gadget_chips.h"

struct mbim_eth_dev;

/*
 * This represents the USB side of an "ethernet" link, managed by a USB
 * function which provides control and (maybe) framing.  Two functions
 * in different configurations could share the same ethernet link/netdev,
 * using different host interaction models.
 *
 * There is a current limitation that only one instance of this link may
 * be present in any given configuration.  When that's a problem, network
 * layer facilities can be used to package multiple logical links on this
 * single "physical" one.
 */
struct mbim_gether {
	struct usb_function		func;

	/* updated by gether_{connect,disconnect} */
	struct mbim_eth_dev			*ioport;

	/* endpoints handle full and/or high speeds */
	struct usb_ep			*in_ep;
	struct usb_ep			*out_ep;

	bool			is_zlp_ok;

	u16				cdc_filter;

	/* hooks for added framing, as needed for RNDIS and EEM. */
	u32				header_len;
	/* NCM requires fixed size bundles */
	bool			is_fixed;
	u32				fixed_out_len;
	u32				fixed_in_len;

	unsigned		ul_max_pkts_per_xfer;
	unsigned		dl_max_pkts_per_xfer;
	unsigned		dl_max_transfer_len;
	bool			multi_pkt_xfer;
	struct sk_buff	*(*wrap)(struct mbim_gether *port,
				struct sk_buff *skb, int ifid);
	int		(*unwrap)(struct mbim_gether *port,
				struct sk_buff *skb,
				struct sk_buff_head *list);

	/* called on network open/close */
	void				(*open)(struct mbim_gether *);
	void				(*close)(struct mbim_gether *);
	struct rndis_packet_msg_type	*header;
};



/*#define NIPQUAD(addr) \
*			((unsigned char *)&addr)[0], \
*			((unsigned char *)&addr)[1], \
*			((unsigned char *)&addr)[2], \
*			((unsigned char *)&addr)[3]
*
*#define NIPQUAD_FMT "%u.%u.%u.%u"
*/


/* variant of gether_setup that allows customizing network device name */
struct mbim_eth_dev *mbim_ether_setup_name(struct usb_gadget *g);

void mbim_ether_cleanup(struct mbim_eth_dev *dev);

/* connect/disconnect is handled by individual functions */
struct net_device *mbim_connect(struct mbim_gether *);
void mbim_disconnect(struct mbim_gether *);

#ifdef CONFIG_MTK_NET_CCMNI
extern void ccmni_update_mbim_interface(int md_id, int id);
extern int ccmni_send_mbim_skb(int md_id, struct sk_buff *skb);
#endif
#endif /* __MBIM_ETHER_H */
