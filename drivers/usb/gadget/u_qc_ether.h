/*
 * u_qc_ether.h -- interface to USB gadget "ethernet link" utilities
 *
 * Copyright (C) 2003-2005,2008 David Brownell
 * Copyright (C) 2003-2004 Robert Schwebel, Benedikt Spranger
 * Copyright (C) 2008 Nokia Corporation
 * Copyright (c) 2012-2013, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef __U_QC_ETHER_H
#define __U_QC_ETHER_H

#include <linux/err.h>
#include <linux/if_ether.h>
#include <linux/usb/composite.h>
#include <linux/usb/cdc.h>

#include "gadget_chips.h"


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
 *
 * This function is based on Ethernet-over-USB link layer utilities and
 * contains MSM specific implementation.
 */

struct qc_gether {
	struct usb_function		func;

	/* updated by gether_{connect,disconnect} */
	struct eth_qc_dev		*ioport;

	/* endpoints handle full and/or high speeds */
	struct usb_ep			*in_ep;
	struct usb_ep			*out_ep;

	bool				is_zlp_ok;

	u16				cdc_filter;

	/* hooks for added framing, as needed for RNDIS and EEM. */
	u32				header_len;

	struct sk_buff			*(*wrap)(struct qc_gether *port,
						struct sk_buff *skb);
	int				(*unwrap)(struct qc_gether *port,
						struct sk_buff *skb,
						struct sk_buff_head *list);

	/* called on network open/close */
	void				(*open)(struct qc_gether *);
	void				(*close)(struct qc_gether *);
};

/* netdev setup/teardown as directed by the gadget driver */
int gether_qc_setup(struct usb_gadget *g, u8 ethaddr[ETH_ALEN]);
void gether_qc_cleanup_name(const char *netname);
/* variant of gether_setup that allows customizing network device name */
int gether_qc_setup_name(struct usb_gadget *g, u8 ethaddr[ETH_ALEN],
		const char *netname);

/* connect/disconnect is handled by individual functions */
struct net_device *gether_qc_connect_name(struct qc_gether *link,
		const char *netname, bool netif_enable);
void gether_qc_disconnect_name(struct qc_gether *link, const char *netname);

/* each configuration may bind one instance of an ethernet link */
int ecm_qc_bind_config(struct usb_configuration *c, u8 ethaddr[ETH_ALEN],
				char *xport_name);

int
rndis_qc_bind_config_vendor(struct usb_configuration *c, u8 ethaddr[ETH_ALEN],
					 u32 vendorID, const char *manufacturer,
					 u8 maxPktPerXfer);

void gether_qc_get_macs(u8 dev_mac[ETH_ALEN], u8 host_mac[ETH_ALEN]);

#endif /* __U_QC_ETHER_H */
