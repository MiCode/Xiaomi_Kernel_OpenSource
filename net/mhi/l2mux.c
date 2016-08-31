/*
 * File: l2mux.c
 *
 * Modem-Host Interface (MHI) L2MUX layer
 *
 * Copyright (C) 2011 Renesas Mobile Corporation. All rights reserved.
 *
 * Author: Petri Mattila <petri.to.mattila@renesasmobile.com>
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
#include <linux/slab.h>
#include <linux/if_mhi.h>
#include <linux/mhi.h>
#include <linux/l2mux.h>

#include <net/af_mhi.h>

#ifdef CONFIG_MHI_DEBUG
# define DPRINTK(...)    printk(KERN_DEBUG "MHI/L2MUX: " __VA_ARGS__)
#else
# define DPRINTK(...)
#endif


/* Handle ONLY Non DIX types 0x00-0xff */
#define ETH_NON_DIX_NPROTO   0x0100


/* L2MUX master lock */
static DEFINE_SPINLOCK(l2mux_lock);

/* L3 ID -> RX function table */
static l2mux_skb_fn *l2mux_id2rx_tab[MHI_L3_NPROTO] __read_mostly;

/* Packet Type -> TX function table */
static l2mux_skb_fn *l2mux_pt2tx_tab[ETH_NON_DIX_NPROTO] __read_mostly;


int l2mux_netif_rx_register(int l3, l2mux_skb_fn *fn)
{
	int err = 0;

	DPRINTK("l2mux_netif_rx_register(l3:%d, fn:%p)\n", l3, fn);

	if (l3 < 0 || l3 >= MHI_L3_NPROTO)
		return -EINVAL;

	if (!fn)
		return -EINVAL;

	spin_lock(&l2mux_lock);
	{
		if (l2mux_id2rx_tab[l3] == NULL)
			l2mux_id2rx_tab[l3] = fn;
		else
			err = -EBUSY;
	}
	spin_unlock(&l2mux_lock);

	return err;
}
EXPORT_SYMBOL(l2mux_netif_rx_register);

int l2mux_netif_rx_unregister(int l3)
{
	int err = 0;

	DPRINTK("l2mux_netif_rx_unregister(l3:%d)\n", l3);

	if (l3 < 0 || l3 >= MHI_L3_NPROTO)
		return -EINVAL;

	spin_lock(&l2mux_lock);
	{
		if (l2mux_id2rx_tab[l3])
			l2mux_id2rx_tab[l3] = NULL;
		else
			err = -EPROTONOSUPPORT;
	}
	spin_unlock(&l2mux_lock);

	return err;
}
EXPORT_SYMBOL(l2mux_netif_rx_unregister);

int l2mux_netif_tx_register(int pt, l2mux_skb_fn *fn)
{
	int err = 0;

	DPRINTK("l2mux_netif_tx_register(pt:%d, fn:%p)\n", pt, fn);

	if (pt <= 0 || pt >= ETH_NON_DIX_NPROTO)
		return -EINVAL;

	if (!fn)
		return -EINVAL;

	spin_lock(&l2mux_lock);
	{
		if (l2mux_pt2tx_tab[pt] == NULL)
			l2mux_pt2tx_tab[pt] = fn;
		else
			err = -EBUSY;
	}
	spin_unlock(&l2mux_lock);

	return err;
}
EXPORT_SYMBOL(l2mux_netif_tx_register);

int l2mux_netif_tx_unregister(int pt)
{
	int err = 0;

	DPRINTK("l2mux_netif_tx_unregister(pt:%d)\n", pt);

	if (pt <= 0 || pt >= ETH_NON_DIX_NPROTO)
		return -EINVAL;

	spin_lock(&l2mux_lock);
	{
		if (l2mux_pt2tx_tab[pt])
			l2mux_pt2tx_tab[pt] = NULL;
		else
			err = -EPROTONOSUPPORT;
	}
	spin_unlock(&l2mux_lock);

	return err;
}
EXPORT_SYMBOL(l2mux_netif_tx_unregister);

int l2mux_skb_rx(struct sk_buff *skb, struct net_device *dev)
{
	struct l2muxhdr	 *l2hdr;
	unsigned          l3pid;
	unsigned	  l3len;
	l2mux_skb_fn     *rxfn;

	/* Set the device in the skb */
	skb->dev = dev;

	/* Set MAC header here */
	skb_reset_mac_header(skb);

	/* L2MUX header */
	l2hdr = l2mux_hdr(skb);

	/* proto id and length in L2 header */
	l3pid = l2mux_get_proto(l2hdr);
	l3len = l2mux_get_length(l2hdr);

	DPRINTK("L2MUX: RX dev:%d skb_len:%d l3_len:%d l3_pid:%d\n",
		       skb->dev->ifindex, skb->len, l3len, l3pid);

#ifdef CONFIG_MHI_DUMP_FRAMES
	{
		u8 *ptr = skb->data;
		int len = skb_headlen(skb);
		int i;

		printk(KERN_DEBUG "L2MUX: RX dev:%d skb_len:%d l3_len:%d l3_pid:%d\n",
		       dev->ifindex, skb->len, l3len, l3pid);

		for (i = 0; i < len; i++) {
			if (i%8 == 0)
				printk(KERN_DEBUG "L2MUX: RX [%04X] ", i);
			printk(" 0x%02X", ptr[i]);
			if (i%8 == 7 || i == len-1)
				printk("\n");
		}
	}
#endif
	/* check that the advertised length is correct */
	if (l3len != skb->len - L2MUX_HDR_SIZE) {
		printk(KERN_WARNING "L2MUX: l2mux_skb_rx: L3_id:%d - skb length mismatch L3:%d (+4) <> SKB:%d",
		       l3pid, l3len, skb->len);
		goto drop;
	}

	/* get RX function */
	rxfn = l2mux_id2rx_tab[l3pid];

	/* Not registered */
	if (!rxfn)
		goto drop;

	/* Call the receiver function */
	return rxfn(skb, dev);

drop:
	kfree_skb(skb);
	return NET_RX_DROP;
}
EXPORT_SYMBOL(l2mux_skb_rx);

int l2mux_skb_tx(struct sk_buff *skb, struct net_device *dev)
{
	l2mux_skb_fn *txfn;
	unsigned type;

	/* Packet type ETH_P_XXX */
	type = ntohs(skb->protocol);

#ifdef CONFIG_MHI_DUMP_FRAMES
	{
		u8 *ptr = skb->data;
		int len = skb_headlen(skb);
		int i;

		printk(KERN_DEBUG "L2MUX: TX dev:%d skb_len:%d ETH_P:%d\n",
		       dev->ifindex, skb->len, type);

		for (i = 0; i < len; i++) {
			if (i%8 == 0)
				printk(KERN_DEBUG "L2MUX: TX [%04X] ", i);
			printk(" 0x%02X", ptr[i]);
			if (i%8 == 7 || i == len-1)
				printk("\n");
		}
	}
#endif
	/* Only handling non DIX types */
	if (type <= 0 || type >= ETH_NON_DIX_NPROTO)
		return -EINVAL;

	/* TX function for this packet type */
	txfn = l2mux_pt2tx_tab[type];

	if (txfn)
		return txfn(skb, dev);

	return 0;
}
EXPORT_SYMBOL(l2mux_skb_tx);

static int __init l2mux_init(void)
{
	int i;

	DPRINTK("l2mux_init\n");

	for (i = 0; i < MHI_L3_NPROTO; i++)
		l2mux_id2rx_tab[i] = NULL;

	for (i = 0; i < ETH_NON_DIX_NPROTO; i++)
		l2mux_pt2tx_tab[i] = NULL;

	return 0;
}

static void __exit l2mux_exit(void)
{
	DPRINTK("l2mux_exit\n");
}

module_init(l2mux_init);
module_exit(l2mux_exit);

MODULE_AUTHOR("Petri Mattila <petri.to.mattila@renesasmobile.com>");
MODULE_DESCRIPTION("L2MUX for MHI Protocol Stack");
MODULE_LICENSE("GPL");

