/*
 * File: l3phonet.c
 *
 * L2 PHONET channel to AF_PHONET binding.
 *
 * Copyright (C) 2011 Renesas Mobile Corporation. All rights reserved.
 *
 * Author: Petri To Mattila <petri.to.mattila@renesasmobile.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 *
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/socket.h>
#include <linux/mhi.h>
#include <linux/l2mux.h>


/* Functions */

static int
mhi_pn_netif_rx(struct sk_buff *skb, struct net_device *dev)
{
	/* Set Protocol Family */
	skb->protocol = htons(ETH_P_PHONET);

	/* Remove L2MUX header and Phonet media byte */
	skb_pull(skb, L2MUX_HDR_SIZE + 1);

	/* Pass upwards to the Procotol Family */
	return netif_rx(skb);
}

static int
mhi_pn_netif_tx(struct sk_buff *skb, struct net_device *dev)
{
	struct l2muxhdr *l2hdr;
	int l3len;
	u8  *ptr;

	/* Add media byte */
	ptr = skb_push(skb, 1);

	/* Set media byte */
	ptr[0] = dev->dev_addr[0];

	/* L3 length */
	l3len = skb->len;

	/* Add L2MUX header */
	skb_push(skb, L2MUX_HDR_SIZE);

	/* Mac header starts here */
	skb_reset_mac_header(skb);

	/* L2MUX header pointer */
	l2hdr = l2mux_hdr(skb);

	/* L3 Proto ID */
	l2mux_set_proto(l2hdr, MHI_L3_PHONET);

	/* L3 payload length */
	l2mux_set_length(l2hdr, l3len);

	return 0;
}


/* Module registration */

int __init mhi_pn_init(void)
{
	int err;

	err = l2mux_netif_rx_register(MHI_L3_PHONET, mhi_pn_netif_rx);
	if (err)
		goto err1;

	err = l2mux_netif_tx_register(ETH_P_PHONET, mhi_pn_netif_tx);
	if (err)
		goto err2;

	return 0;

err2:
	l2mux_netif_rx_unregister(MHI_L3_PHONET);
err1:
	return err;
}

void __exit mhi_pn_exit(void)
{
	l2mux_netif_rx_unregister(MHI_L3_PHONET);
	l2mux_netif_tx_unregister(ETH_P_PHONET);
}


module_init(mhi_pn_init);
module_exit(mhi_pn_exit);

MODULE_AUTHOR("Petri Mattila <petri.to.mattila@renesasmobile.com>");
MODULE_DESCRIPTION("MHI Phonet protocol family bridge");
MODULE_LICENSE("GPL");

