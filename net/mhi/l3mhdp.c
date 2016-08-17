/*
 * File: l3mhdp.c
 *
 * MHDP - Modem Host Data Protocol for MHI protocol family.
 *
 * Copyright (C) 2011 Renesas Mobile Corporation. All rights reserved.
 *
 * Author:	Sugnan Prabhu S <sugnan.prabhu@renesasmobile.com>
 *		Petri Mattila <petri.to.mattila@renesasmobile.com>
 *
 * Based on work by: Sam Lantinga (slouken@cs.ucdavis.edu)
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

#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/version.h>

#include <linux/skbuff.h>
#include <linux/netdevice.h>
#include <linux/if_arp.h>
#include <linux/l2mux.h>
#include <linux/etherdevice.h>
#include <linux/pkt_sched.h>

#ifdef CONFIG_MHDP_BONDING_SUPPORT
#define MHDP_BONDING_SUPPORT
#endif

/*#define MHDP_USE_NAPI*/

#ifdef MHDP_BONDING_SUPPORT
#include <linux/etherdevice.h>
#endif /* MHDP_BONDING_SUPPORT */

#include <net/netns/generic.h>
#include <net/mhi/mhdp.h>


/* MHDP device MTU limits */
#define MHDP_MTU_MAX		0x2400
#define MHDP_MTU_MIN		0x44
#define MAX_MHDP_FRAME_SIZE	16000

/* MHDP device names */
#define MHDP_IFNAME		"rmnet%d"
#define MHDP_CTL_IFNAME		"rmnetctl"

/* Print every MHDP SKB content */
/*#define MHDP_DEBUG_SKB*/


#define EPRINTK(...)    printk(KERN_DEBUG "MHI/MHDP: " __VA_ARGS__)

#ifdef CONFIG_MHI_DEBUG
# define DPRINTK(...)    printk(KERN_DEBUG "MHI/MHDP: " __VA_ARGS__)
#else
# define DPRINTK(...)
#endif

#ifdef MHDP_DEBUG_SKB
# define SKBPRINT(a, b)    __print_skb_content(a, b)
#else
# define SKBPRINT(a, b)
#endif

/* IPv6 support */
#define VER_IPv4 0x04
#define VER_IPv6 0x06
#define ETH_IP_TYPE(x) (((0x00|(x>>4)) == VER_IPv4) ? ETH_P_IP : ETH_P_IPV6)

int sysctl_mhdp_concat_nb_pkt __read_mostly;
EXPORT_SYMBOL(sysctl_mhdp_concat_nb_pkt);

/*** Type definitions ***/
#ifdef MHDP_USE_NAPI
#define NAPI_WEIGHT 64
#endif /*MHDP_USE_NAPI*/

#define MAX_MHDPHDR_SIZE 18

struct mhdp_tunnel {
	struct mhdp_tunnel	*next;
	struct net_device	*dev;
	struct net_device	*master_dev;
	struct sk_buff		*skb;
	int pdn_id;
	int			free_pdn;
	struct timer_list tx_timer;
	struct sk_buff *skb_to_free[MAX_MHDPHDR_SIZE];
	spinlock_t timer_lock;
};

struct mhdp_net {
	struct mhdp_tunnel	*tunnels;
	struct net_device	*ctl_dev;
#ifdef MHDP_USE_NAPI
	struct net_device	*dev;
	struct napi_struct	napi;
	struct sk_buff_head	skb_list;
#endif /*#ifdef MHDP_USE_NAPI*/
};

struct packet_info {
	uint32_t pdn_id;
	uint32_t packet_offset;
	uint32_t packet_length;
};

struct mhdp_hdr {
	uint32_t packet_count;
	struct packet_info info[MAX_MHDPHDR_SIZE];
};


/*** Prototypes ***/

static void mhdp_netdev_setup(struct net_device *dev);

static void mhdp_submit_queued_skb(struct mhdp_tunnel *tunnel, int force_send);

static int mhdp_netdev_event(struct notifier_block *this,
			     unsigned long event, void *ptr);

static void tx_timer_timeout(unsigned long arg);

#ifdef MHDP_USE_NAPI

static int mhdp_poll(struct napi_struct *napi, int budget);

#endif /*MHDP_USE_NAPI*/

/*** Global Variables ***/

static int  mhdp_net_id __read_mostly;

static struct notifier_block mhdp_netdev_notifier = {
	.notifier_call = mhdp_netdev_event,
};

/*** Funtions ***/

#ifdef MHDP_DEBUG_SKB
static void
__print_skb_content(struct sk_buff *skb, const char *tag)
{
	struct page *page;
	skb_frag_t *frag;
	int len;
	int i, j;
	u8 *ptr;

	/* Main SKB buffer */
	ptr = (u8 *)skb->data;
	len = skb_headlen(skb);

	printk(KERN_DEBUG "MHDP: SKB buffer lenght %02u\n", len);
	for (i = 0; i < len; i++) {
		if (i%8 == 0)
			printk(KERN_DEBUG "%s DATA: ", tag);
		printk(" 0x%02X", ptr[i]);
		if (i%8 == 7 || i == len - 1)
			printk("\n");
	}

	/* SKB fragments */
	for (i = 0; i < (skb_shinfo(skb)->nr_frags); i++) {
		frag = &skb_shinfo(skb)->frags[i];
		page = skb_frag_page(frag);

		ptr = page_address(page);

		for (j = 0; j < frag->size; j++) {
			if (j%8 == 0)
				printk(KERN_DEBUG "%s FRAG[%d]: ", tag, i);
			printk(" 0x%02X", ptr[frag->page_offset + j]);
			if (j%8 == 7 || j == frag->size - 1)
				printk("\n");
		}
	}
}
#endif


static inline struct mhdp_net *
mhdp_net_dev(struct net_device *dev)
{
	return net_generic(dev_net(dev), mhdp_net_id);
}

static void
mhdp_tunnel_init(struct net_device *dev,
		 struct mhdp_tunnel_parm *parms,
		 struct net_device *master_dev)
{
	struct mhdp_net *mhdpn = mhdp_net_dev(dev);
	struct mhdp_tunnel *tunnel = netdev_priv(dev);

	DPRINTK("mhdp_tunnel_init: dev:%s", dev->name);

	tunnel->next = mhdpn->tunnels;
	mhdpn->tunnels = tunnel;

	tunnel->dev         = dev;
	tunnel->master_dev  = master_dev;
	tunnel->skb         = NULL;
	tunnel->pdn_id      = parms->pdn_id;
	tunnel->free_pdn    = 0;

	init_timer(&tunnel->tx_timer);
	spin_lock_init(&tunnel->timer_lock);
}

static void
mhdp_tunnel_destroy(struct net_device *dev)
{
	DPRINTK("mhdp_tunnel_destroy: dev:%s", dev->name);

	unregister_netdevice(dev);
}

static void
mhdp_destroy_tunnels(struct mhdp_net *mhdpn)
{
	struct mhdp_tunnel *tunnel;

	for (tunnel = mhdpn->tunnels; (tunnel); tunnel = tunnel->next)
		mhdp_tunnel_destroy(tunnel->dev);

	mhdpn->tunnels = NULL;
}

static struct mhdp_tunnel *
mhdp_locate_tunnel(struct mhdp_net *mhdpn, int pdn_id)
{
	struct mhdp_tunnel *tunnel;

	for (tunnel = mhdpn->tunnels; tunnel; tunnel = tunnel->next)
		if (tunnel->pdn_id == pdn_id)
			return tunnel;

	return NULL;
}

static struct net_device *
mhdp_add_tunnel(struct net *net, struct mhdp_tunnel_parm *parms)
{
	struct net_device *mhdp_dev, *master_dev;

	DPRINTK("mhdp_add_tunnel: adding a tunnel to %s\n", parms->master);

	master_dev = dev_get_by_name(net, parms->master);
	if (!master_dev)
		goto err_alloc_dev;

	mhdp_dev = alloc_netdev(sizeof(struct mhdp_tunnel),
				MHDP_IFNAME, mhdp_netdev_setup);
	if (!mhdp_dev)
		goto err_alloc_dev;

	dev_net_set(mhdp_dev, net);

	if (dev_alloc_name(mhdp_dev, MHDP_IFNAME) < 0)
		goto err_reg_dev;

	strcpy(parms->name, mhdp_dev->name);

	if (register_netdevice(mhdp_dev)) {
		printk(KERN_ERR "MHDP: register_netdev failed\n");
		goto err_reg_dev;
	}

	dev_hold(mhdp_dev);

	mhdp_tunnel_init(mhdp_dev, parms, master_dev);

	dev_put(master_dev);

	return mhdp_dev;

err_reg_dev:
	free_netdev(mhdp_dev);
err_alloc_dev:
	return NULL;
}


static int
mhdp_netdev_ioctl(struct net_device *dev, struct ifreq *ifr, int cmd)
{
	struct net *net = dev_net(dev);
	struct mhdp_net *mhdpn = mhdp_net_dev(dev);
	struct mhdp_tunnel *tunnel, *pre_dev;
	struct mhdp_tunnel_parm __user *u_parms;
	struct mhdp_tunnel_parm k_parms;

	int err = 0;

	DPRINTK("mhdp tunnel ioctl %X", cmd);

	switch (cmd) {

	case SIOCADDPDNID:
		u_parms = (struct mhdp_tunnel_parm *)ifr->ifr_data;
		if (copy_from_user(&k_parms, u_parms,
				   sizeof(struct mhdp_tunnel_parm))) {
			DPRINTK("Error: Failed to copy data from user space");
			return -EFAULT;
		}

		DPRINTK("pdn_id:%d master_device:%s",
				k_parms.pdn_id,
				k_parms.master);
		tunnel = mhdp_locate_tunnel(mhdpn, k_parms.pdn_id);

		if (NULL == tunnel) {
			if (mhdp_add_tunnel(net, &k_parms)) {
				if (copy_to_user(u_parms, &k_parms,
					 sizeof(struct mhdp_tunnel_parm)))
					err = -EINVAL;
			} else {
				err = -EINVAL;
			}

		} else if (1 == tunnel->free_pdn) {

			tunnel->free_pdn = 0;

			strcpy((char *) &k_parms.name, tunnel->dev->name);

			if (copy_to_user(u_parms, &k_parms,
					sizeof(struct mhdp_tunnel_parm)))
					err = -EINVAL;
		} else {
			err = -EBUSY;
		}
		break;

	case SIOCDELPDNID:
		u_parms = (struct mhdp_tunnel_parm *)ifr->ifr_data;
		if (copy_from_user(&k_parms, u_parms,
					sizeof(struct mhdp_tunnel_parm))) {
			DPRINTK("Error: Failed to copy data from user space");
			return -EFAULT;
		}

		DPRINTK("pdn_id:%d", k_parms.pdn_id);

		for (tunnel = mhdpn->tunnels, pre_dev = NULL;
			tunnel;
			pre_dev = tunnel, tunnel = tunnel->next) {
			if (tunnel->pdn_id == k_parms.pdn_id)
				tunnel->free_pdn = 1;
		}
		break;

	case SIOCRESETMHDP:
		mhdp_destroy_tunnels(mhdpn);
		break;

	default:
		err = -EINVAL;
	}

	return err;
}

static int
mhdp_netdev_change_mtu(struct net_device *dev, int new_mtu)
{
	if (new_mtu < MHDP_MTU_MIN || new_mtu > MHDP_MTU_MAX)
		return -EINVAL;

	dev->mtu = new_mtu;

	return 0;
}

static void
mhdp_netdev_uninit(struct net_device *dev)
{
	dev_put(dev);
}


static void
mhdp_submit_queued_skb(struct mhdp_tunnel *tunnel, int force_send)
{
	struct sk_buff *skb = tunnel->skb;
	struct l2muxhdr	*l2hdr;
	struct mhdp_hdr *mhdpHdr;
	int i, nb_frags;

	BUG_ON(!tunnel->master_dev);

	if (skb) {
		mhdpHdr = (struct mhdp_hdr *)tunnel->skb->data;
		nb_frags = mhdpHdr->packet_count;

		skb->protocol = htons(ETH_P_MHDP);
		skb->priority = 1;

		skb->dev = tunnel->master_dev;

		skb_reset_network_header(skb);

		skb_push(skb, L2MUX_HDR_SIZE);
		skb_reset_mac_header(skb);

		l2hdr = l2mux_hdr(skb);
		l2mux_set_proto(l2hdr, MHI_L3_MHDP_UL);
		l2mux_set_length(l2hdr, skb->len - L2MUX_HDR_SIZE);

		SKBPRINT(skb, "MHDP: TX");

		tunnel->dev->stats.tx_packets++;
		tunnel->skb = NULL;

		dev_queue_xmit(skb);

		for (i = 0; i < nb_frags; i++)
			if (tunnel->skb_to_free[i])
				dev_kfree_skb(tunnel->skb_to_free[i]);
			else
				EPRINTK("submit_q_skb: error no skb to free\n");
	}
}

static int
mhdp_netdev_rx(struct sk_buff *skb, struct net_device *dev)
{
	skb_frag_t *frag = NULL;
	struct page *page = NULL;
	struct sk_buff *newskb;
	struct mhdp_hdr *mhdpHdr;
	int offset, length;
	int err = 0, i, pdn_id;
	int mhdp_header_len;
	struct mhdp_tunnel *tunnel = NULL;
	int start = 0;
	int has_frag = skb_shinfo(skb)->nr_frags;
	uint32_t packet_count;
	unsigned char ip_ver;

	if (has_frag) {
		frag = &skb_shinfo(skb)->frags[0];
		page = skb_frag_page(frag);
	}

	if (skb_headlen(skb) > L2MUX_HDR_SIZE)
		skb_pull(skb, L2MUX_HDR_SIZE);
	else if (has_frag)
		frag->page_offset += L2MUX_HDR_SIZE;

	packet_count = *((unsigned char *)skb->data);

	mhdp_header_len = sizeof(packet_count) +
		(packet_count * sizeof(struct packet_info));

	if (mhdp_header_len > skb_headlen(skb)) {
		int skbheadlen = skb_headlen(skb);

		DPRINTK("mhdp header length: %d, skb_headerlen: %d",
				mhdp_header_len, skbheadlen);

		mhdpHdr = kmalloc(mhdp_header_len, GFP_ATOMIC);
		if (mhdpHdr == NULL) {
			printk(KERN_ERR "%s: kmalloc failed.\n", __func__);
			return err;
		}

		if (skbheadlen == 0) {
			memcpy((__u8 *)mhdpHdr,	page_address(page) +
						frag->page_offset,
						mhdp_header_len);

		} else {
			memcpy((__u8 *)mhdpHdr, skb->data, skbheadlen);

			memcpy((__u8 *)mhdpHdr + skbheadlen,
			       page_address(page) +
			       frag->page_offset,
			       mhdp_header_len - skbheadlen);

			start = mhdp_header_len - skbheadlen;
		}

		DPRINTK("page start: %d", start);
	} else {
		DPRINTK("skb->data has whole mhdp header");
		mhdpHdr = (struct mhdp_hdr *)(((__u8 *)skb->data));
	}

	DPRINTK("MHDP PACKET COUNT : %d",  mhdpHdr->packet_count);

	rcu_read_lock();

	for (i = 0; i < mhdpHdr->packet_count; i++) {

		DPRINTK(" packet_info[%d] - PDNID:%d, packet_offset: %d,\
			packet_length: %d\n", i, mhdpHdr->info[i].pdn_id,
			mhdpHdr->info[i].packet_offset,
			mhdpHdr->info[i].packet_length);

		pdn_id = mhdpHdr->info[i].pdn_id;
		offset = mhdpHdr->info[i].packet_offset;
		length = mhdpHdr->info[i].packet_length;

		if (skb_headlen(skb) > (mhdp_header_len + offset)) {

			newskb = skb_clone(skb, GFP_ATOMIC);
			if (unlikely(!newskb))
				goto error;

			skb_pull(newskb, mhdp_header_len + offset);
			ip_ver = (u8)*newskb->data;

		} else if (has_frag) {

			newskb = netdev_alloc_skb(dev, skb_headlen(skb));

			if (unlikely(!newskb))
				goto error;

			get_page(page);
			skb_add_rx_frag(newskb,
				skb_shinfo(newskb)->nr_frags,
				page,
				frag->page_offset +
				((mhdp_header_len - skb_headlen(skb)) + offset),
				length,
				PAGE_SIZE);

			ip_ver = *((unsigned long *)page_address(page) +
					(frag->page_offset +
					((mhdp_header_len - skb_headlen(skb)) +
					offset)));
			if ((ip_ver>>4) != VER_IPv4 &&
				(ip_ver>>4) != VER_IPv6)
				goto error;

		} else {
			DPRINTK("Error in the data received");
			goto error;
		}

		skb_reset_network_header(newskb);

		/* IPv6 Support - Check the IP version */
		/* and set ETH_P_IP or ETH_P_IPv6 for received packets */

		newskb->protocol = htons(ETH_IP_TYPE(ip_ver));

		newskb->pkt_type = PACKET_HOST;

		skb_tunnel_rx(newskb, dev);

		tunnel = mhdp_locate_tunnel(mhdp_net_dev(dev), pdn_id);
		if (tunnel) {
			struct net_device_stats *stats = &tunnel->dev->stats;
			stats->rx_packets++;
			newskb->dev = tunnel->dev;
			SKBPRINT(newskb, "NEWSKB: RX");
#ifdef MHDP_USE_NAPI
			netif_receive_skb(newskb);
#else
			netif_rx(newskb);
#endif /*#ifdef MHDP_USE_NAPI*/
		}
	}
	rcu_read_unlock();

	if (mhdp_header_len > skb_headlen(skb))
		kfree(mhdpHdr);

	dev_kfree_skb(skb);

	return err;

error:
	if (mhdp_header_len > skb_headlen(skb))
		kfree(mhdpHdr);

	dev_kfree_skb(skb);

	return err;
}

#ifdef MHDP_USE_NAPI
/*
static int mhdp_poll(struct napi_struct *napi, int budget)
function called through napi to read current ip frame received
*/
static int mhdp_poll(struct napi_struct *napi, int budget)
{
	struct mhdp_net *mhdpn = container_of(napi, struct mhdp_net, napi);
	int err = 0;
	struct sk_buff *skb;

	while (!skb_queue_empty(&mhdpn->skb_list)) {

		skb = skb_dequeue(&mhdpn->skb_list);
		err = mhdp_netdev_rx(skb, mhdpn->dev);
	}

	napi_complete(napi);

	return err;
}
/*l2mux callback*/
static int
mhdp_netdev_rx_napi(struct sk_buff *skb, struct net_device *dev)
{
	struct mhdp_net *mhdpn = mhdp_net_dev(dev);


	if (mhdpn) {

		mhdpn->dev = dev;
		skb_queue_tail(&mhdpn->skb_list, skb);

		napi_schedule(&mhdpn->napi);

	} else {
		EPRINTK("mhdp_netdev_rx_napi-MHDP driver init not correct\n");
	}

	return 0;
}
#endif /*MHDP_USE_NAPI*/

static void tx_timer_timeout(unsigned long arg)
{
	struct mhdp_tunnel *tunnel = (struct mhdp_tunnel *) arg;

	spin_lock(&tunnel->timer_lock);

	mhdp_submit_queued_skb(tunnel, 1);

	spin_unlock(&tunnel->timer_lock);
}

static int
mhdp_netdev_xmit(struct sk_buff *skb, struct net_device *dev)
{
	struct mhdp_hdr *mhdpHdr;
	struct mhdp_tunnel *tunnel = netdev_priv(dev);
	struct net_device_stats *stats = &tunnel->dev->stats;
	struct page *page = NULL;
	int i;
	int packet_count, offset, len;

	spin_lock(&tunnel->timer_lock);

	SKBPRINT(skb, "SKB: TX");

	if (timer_pending(&tunnel->tx_timer))
		del_timer(&tunnel->tx_timer);

#if 0
	{
		int i;
		int len = skb->len;
		u8 *ptr = skb->data;

		for (i = 0; i < len; i++) {
			if (i%8 == 0)
				printk(KERN_DEBUG
					"MHDP mhdp_netdev_xmit: TX [%04X] ", i);
			printk(" 0x%02X", ptr[i]);
			if (i%8 == 7 || i == len-1)
				printk("\n");
		}
	}
#endif
xmit_again:

	if (tunnel->skb == NULL) {
		tunnel->skb = netdev_alloc_skb(dev,
			L2MUX_HDR_SIZE + sizeof(struct mhdp_hdr));

		if (!tunnel->skb) {
			EPRINTK("mhdp_netdev_xmit error1");
			BUG();
		}

		/* Place holder for the mhdp packet count */
		len = skb_headroom(tunnel->skb) - L2MUX_HDR_SIZE - ETH_HLEN;

		skb_push(tunnel->skb, len);
		len -= 4;

		memset(tunnel->skb->data, 0, len);

		/*
		 * Need to replace following logic, with something better like
		 * __pskb_pull_tail or pskb_may_pull(tunnel->skb, len);
		 */
		{
			tunnel->skb->tail -= len;
			tunnel->skb->len  -= len;
		}


		mhdpHdr = (struct mhdp_hdr *)tunnel->skb->data;
		mhdpHdr->packet_count = 0;
	}

	/*This new frame is to big for the current mhdp frame,
	* send the frame first*/
	if (tunnel->skb->len + skb->len >= MAX_MHDP_FRAME_SIZE) {

		mhdp_submit_queued_skb(tunnel, 1);

		goto xmit_again;

	} else {

	/*
	 * skb_put cannot be called as the (data_len != 0)
	 */
		tunnel->skb->tail += sizeof(struct packet_info);
		tunnel->skb->len  += sizeof(struct packet_info);

		DPRINTK("new - skb->tail:%lu skb->end:%lu skb->data_len:%lu",
				(unsigned long)tunnel->skb->tail,
				(unsigned long)tunnel->skb->end,
				(unsigned long)tunnel->skb->data_len);

		mhdpHdr = (struct mhdp_hdr *)tunnel->skb->data;

		tunnel->skb_to_free[mhdpHdr->packet_count] = skb;

		packet_count = mhdpHdr->packet_count;
		mhdpHdr->info[packet_count].pdn_id = tunnel->pdn_id;
		if (packet_count == 0) {
			mhdpHdr->info[packet_count].packet_offset = 0;
		} else {
			mhdpHdr->info[packet_count].packet_offset =
				mhdpHdr->info[packet_count - 1].packet_offset +
				mhdpHdr->info[packet_count - 1].packet_length;
		}

		mhdpHdr->info[packet_count].packet_length = skb->len;
		mhdpHdr->packet_count++;

		page = virt_to_page(skb->data);

		if (page == NULL) {
			EPRINTK("kmap_atomic_to_page returns NULL");
			goto tx_error;
		}

		get_page(page);

		offset = ((unsigned long)skb->data -
			  (unsigned long)page_address(page));

		skb_add_rx_frag(tunnel->skb, skb_shinfo(tunnel->skb)->nr_frags,
			page, offset, skb_headlen(skb), PAGE_SIZE);

		if (skb_shinfo(skb)->nr_frags) {
			for (i = 0; i < skb_shinfo(skb)->nr_frags; i++) {
				skb_frag_t *frag =
					&skb_shinfo(tunnel->skb)->frags[i];

				get_page(skb_frag_page(frag));
				skb_add_rx_frag(tunnel->skb,
					skb_shinfo(tunnel->skb)->nr_frags,
					skb_frag_page(frag),
					frag->page_offset,
					frag->size,
					PAGE_SIZE);
			}
		}

		if (mhdpHdr->packet_count >= MAX_MHDPHDR_SIZE) {
			mhdp_submit_queued_skb(tunnel, 1);
		} else {
			tunnel->tx_timer.function = &tx_timer_timeout;
			tunnel->tx_timer.data     = (unsigned long) tunnel;
			tunnel->tx_timer.expires =
				jiffies + ((HZ + 999) / 1000);
			add_timer(&tunnel->tx_timer);
		}
	}

	spin_unlock(&tunnel->timer_lock);
	return NETDEV_TX_OK;

tx_error:
	spin_unlock(&tunnel->timer_lock);
	stats->tx_errors++;
	dev_kfree_skb(skb);
	return NETDEV_TX_OK;
}


static int
mhdp_netdev_event(struct notifier_block *this, unsigned long event, void *ptr)
{
	struct net_device *event_dev = (struct net_device *)ptr;

	DPRINTK("event_dev: %s, event: %lx\n",
		event_dev ? event_dev->name : "None", event);

	switch (event) {
	case NETDEV_UNREGISTER:
	{
		struct mhdp_net *mhdpn = mhdp_net_dev(event_dev);
		struct mhdp_tunnel *iter, *prev;

		DPRINTK("event_dev: %s, event: %lx\n",
			event_dev ? event_dev->name : "None", event);

		for (iter = mhdpn->tunnels, prev = NULL;
			iter; prev = iter, iter = iter->next) {
			if (event_dev == iter->master_dev) {
				if (!prev)
					mhdpn->tunnels =
						mhdpn->tunnels->next;
				else
					prev->next = iter->next;
				mhdp_tunnel_destroy(iter->dev);
			}
		}
	}
	break;
	}

	return NOTIFY_DONE;
}

#ifdef MHDP_BONDING_SUPPORT
static void
cdma_netdev_uninit(struct net_device *dev)
{
	dev_put(dev);
}

static void mhdp_ethtool_get_drvinfo(struct net_device *dev,
				struct ethtool_drvinfo *drvinfo)
{
	strncpy(drvinfo->driver, dev->name, 32);
}

static const struct ethtool_ops mhdp_ethtool_ops = {
	.get_drvinfo		= mhdp_ethtool_get_drvinfo,
	.get_link		= ethtool_op_get_link,
};
#endif /* MHDP_BONDING_SUPPORT */

static const struct net_device_ops mhdp_netdev_ops = {
	.ndo_uninit	= mhdp_netdev_uninit,
	.ndo_start_xmit	= mhdp_netdev_xmit,
	.ndo_do_ioctl	= mhdp_netdev_ioctl,
	.ndo_change_mtu	= mhdp_netdev_change_mtu,
};

static void mhdp_netdev_setup(struct net_device *dev)
{
	dev->netdev_ops		= &mhdp_netdev_ops;
#ifdef MHDP_BONDING_SUPPORT
	dev->ethtool_ops = &mhdp_ethtool_ops;
#endif /* MHDP_BONDING_SUPPORT */

	dev->destructor		= free_netdev;

#ifdef truc /* MHDP_BONDING_SUPPORT */
	ether_setup(dev);
	dev->flags		|= IFF_NOARP;
	dev->iflink		= 0;
	dev->features	|= (NETIF_F_NETNS_LOCAL | NETIF_F_FRAGLIST);
#else
	dev->type		= ARPHRD_TUNNEL;
	dev->hard_header_len	= L2MUX_HDR_SIZE + sizeof(struct mhdp_hdr);
	dev->mtu		= ETH_DATA_LEN;
	dev->flags		= IFF_NOARP;
	dev->iflink		= 0;
	dev->addr_len		= 4;
	dev->features	       |= (NETIF_F_NETNS_LOCAL | NETIF_F_FRAGLIST);
#endif /* MHDP_BONDING_SUPPORT */
}

static int __net_init mhdp_init_net(struct net *net)
{
	struct mhdp_net *mhdpn = net_generic(net, mhdp_net_id);
	int err;

	mhdpn->tunnels = NULL;

	mhdpn->ctl_dev = alloc_netdev(sizeof(struct mhdp_tunnel),
				      MHDP_CTL_IFNAME,
				      mhdp_netdev_setup);
	if (!mhdpn->ctl_dev)
		return -ENOMEM;

	dev_net_set(mhdpn->ctl_dev, net);
	dev_hold(mhdpn->ctl_dev);

	err = register_netdev(mhdpn->ctl_dev);
	if (err) {
		printk(KERN_ERR MHDP_CTL_IFNAME " register failed");
		free_netdev(mhdpn->ctl_dev);
		return err;
	}

#ifdef MHDP_USE_NAPI
	netif_napi_add(mhdpn->ctl_dev, &mhdpn->napi, mhdp_poll, NAPI_WEIGHT);
	napi_enable(&mhdpn->napi);
	skb_queue_head_init(&mhdpn->skb_list);
#endif /*#ifdef MHDP_USE_NAPI*/
	return 0;
}

static void __net_exit mhdp_exit_net(struct net *net)
{
	struct mhdp_net *mhdpn = net_generic(net, mhdp_net_id);

	rtnl_lock();
	mhdp_destroy_tunnels(mhdpn);
	unregister_netdevice(mhdpn->ctl_dev);
	rtnl_unlock();
}

static struct pernet_operations mhdp_net_ops = {
	.init = mhdp_init_net,
	.exit = mhdp_exit_net,
	.id   = &mhdp_net_id,
	.size = sizeof(struct mhdp_net),
};


static int __init mhdp_init(void)
{
	int err;

#ifdef MHDP_USE_NAPI
	err = l2mux_netif_rx_register(MHI_L3_MHDP_DL, mhdp_netdev_rx_napi);
#else
	err = l2mux_netif_rx_register(MHI_L3_MHDP_DL, mhdp_netdev_rx);
#endif /* MHDP_USE_NAPI */
	if (err)
		goto rollback0;

	err = register_pernet_device(&mhdp_net_ops);
	if (err < 0)
		goto rollback1;

	err = register_netdevice_notifier(&mhdp_netdev_notifier);
	if (err < 0)
		goto rollback2;

	return 0;

rollback2:
	unregister_pernet_device(&mhdp_net_ops);
rollback1:
	l2mux_netif_rx_unregister(MHI_L3_MHDP_DL);
rollback0:
	return err;
}

static void __exit mhdp_exit(void)
{
	l2mux_netif_rx_unregister(MHI_L3_MHDP_DL);
	unregister_netdevice_notifier(&mhdp_netdev_notifier);
	unregister_pernet_device(&mhdp_net_ops);
}


module_init(mhdp_init);
module_exit(mhdp_exit);

MODULE_AUTHOR("Sugnan Prabhu S <sugnan.prabhu@renesasmobile.com>");
MODULE_DESCRIPTION("Modem Host Data Protocol for MHI");
MODULE_LICENSE("GPL");

