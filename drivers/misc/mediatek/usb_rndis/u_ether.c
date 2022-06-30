// SPDX-License-Identifier: GPL-2.0+
/*
 * u_ether.c -- Ethernet-over-USB link layer utilities for Gadget stack
 *
 * Copyright (C) 2003-2005,2008 David Brownell
 * Copyright (C) 2003-2004 Robert Schwebel, Benedikt Spranger
 * Copyright (C) 2008 Nokia Corporation
 * Copyright (c) 2018 MediaTek Inc.
 */

/* #define VERBOSE_DEBUG */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/gfp.h>
#include <linux/device.h>
#include <linux/ctype.h>
#include <linux/etherdevice.h>
#include <linux/ethtool.h>
#include <linux/if_vlan.h>
#include <net/sch_generic.h>
#include <linux/ip.h>
#include <linux/ktime.h>

#include "u_ether.h"
#include "rndis.h"

#if IS_ENABLED(CONFIG_MTK_NET_RPS)
#include "rps_perf.h"
#endif

/*
 * This component encapsulates the Ethernet link glue needed to provide
 * one (!) network link through the USB gadget stack, normally "usb0".
 *
 * The control and data models are handled by the function driver which
 * connects to this code; such as CDC Ethernet (ECM or EEM),
 * "CDC Subset", or RNDIS.  That includes all descriptor and endpoint
 * management.
 *
 * Link level addressing is handled by this component using module
 * parameters; if no such parameters are provided, random link level
 * addresses are used.  Each end of the link uses one address.  The
 * host end address is exported in various ways, and is often recorded
 * in configuration databases.
 *
 * The driver which assembles each configuration using such a link is
 * responsible for ensuring that each configuration includes at most one
 * instance of is network link.  (The network layer provides ways for
 * this single "physical" link to be used by multiple virtual links.)
 */

#define UETH__VERSION	"29-May-2008"

/* Experiments show that both Linux and Windows hosts allow up to 16k
 * frame sizes. Set the max size to 15k+52 to prevent allocating 32k
 * blocks and still have efficient handling. */
#define GETHER_MAX_ETH_FRAME_LEN 15412

static struct workqueue_struct	*uether_wq;
static struct workqueue_struct	*uether_wq1;
static struct workqueue_struct  *uether_rps_wq;

USB_ETHERNET_MODULE_PARAMETERS();

/*-------------------------------------------------------------------------*/

#define RX_EXTRA	20	/* bytes guarding against rx overflows */

#define DEFAULT_QLEN	2	/* double buffering by default */

static unsigned int tx_wakeup_threshold = 13;
module_param(tx_wakeup_threshold, uint, 0644);
MODULE_PARM_DESC(tx_wakeup_threshold, "tx wakeup threshold value");

static int u_ether_tx_req_threshold = 1;
module_param(u_ether_tx_req_threshold, uint, 0644);
MODULE_PARM_DESC(u_ether_tx_req_threshold, "u_ether_tx_req_threshold value");

#define U_ETHER_RX_PENDING_TSHOLD 100
static unsigned int u_ether_rx_pending_thld = U_ETHER_RX_PENDING_TSHOLD;
module_param(u_ether_rx_pending_thld, uint, 0644);

static int rndis_mtu;
module_param(rndis_mtu, uint, 0644);
MODULE_PARM_DESC(rndis_mtu, "Set RNDIS MTU value");

static int rndis_gso;
module_param(rndis_gso, uint, 0644);
MODULE_PARM_DESC(rndis_gso, "Enable RNDIS GSO");

static int uether_debug;
module_param(uether_debug, uint, 0644);
MODULE_PARM_DESC(uether_debug, "Enable U_Ether Debug");

static int ip_debug;
module_param(ip_debug, uint, 0644);
MODULE_PARM_DESC(ip_debug, "Enable IPID Debug");

static int ip_debug_ratelimit;
module_param(ip_debug_ratelimit, uint, 0644);
MODULE_PARM_DESC(ip_debug_ratelimit, "Enable IP Debug ratelimit");

static int uether_tx_profile;
module_param(uether_tx_profile, uint, 0644);
MODULE_PARM_DESC(uether_tx_profile, "Profile TX XMIT");

static bool tx_profile_start;
static ktime_t starttime;
static unsigned long long rec_data_len;

/* for dual-speed hardware, use deeper queues at high/super speed */
static inline int qlen(struct usb_gadget *gadget, unsigned qmult)
{
	if (gadget_is_dualspeed(gadget) && (gadget->speed == USB_SPEED_HIGH ||
					    gadget->speed >= USB_SPEED_SUPER))
		return qmult * DEFAULT_QLEN;
	else
		return DEFAULT_QLEN;
}

/*-------------------------------------------------------------------------*/

/* REVISIT there must be a better way than having two sets
 * of debug calls ...
 */

#undef DBG
#undef VDBG
#undef ERROR
#undef INFO

#define xprintk(d, level, fmt, args...) \
	printk(level "%s: " fmt , (d)->net->name , ## args)

#ifdef DEBUG
#undef DEBUG
#define DBG(dev, fmt, args...) \
	xprintk(dev , KERN_DEBUG , fmt , ## args)
#else
#define DBG(dev, fmt, args...) \
	do { } while (0)
#endif /* DEBUG */

#ifdef VERBOSE_DEBUG
#define VDBG	DBG
#else
#define VDBG(dev, fmt, args...) \
	do { } while (0)
#endif /* DEBUG */

#define ERROR(dev, fmt, args...) \
	xprintk(dev , KERN_ERR , fmt , ## args)
#define INFO(dev, fmt, args...) \
	xprintk(dev , KERN_INFO , fmt , ## args)

/*-------------------------------------------------------------------------*/
unsigned int rndis_test_last_resp_id;
unsigned int rndis_test_last_msg_id;
EXPORT_SYMBOL_GPL(rndis_test_last_msg_id);

unsigned long rndis_test_reset_msg_cnt;
EXPORT_SYMBOL_GPL(rndis_test_reset_msg_cnt);

unsigned long rndis_test_rx_usb_in;
unsigned long rndis_test_rx_net_out;
unsigned long rndis_test_rx_nomem;
unsigned long rndis_test_rx_error;

unsigned long rndis_test_tx_net_in;
unsigned long rndis_test_tx_busy;
unsigned long rndis_test_tx_stop;

unsigned long rndis_test_tx_usb_out;
unsigned long rndis_test_tx_complete;

#define U_ETHER_DBG(fmt, args...) do { \
		if (uether_debug) \
			pr_info("U_ETHER,%s, " fmt, __func__, ## args); \
		} while (0)

/* NETWORK DRIVER HOOKUP (to the layer above this driver) */

static int ueth_change_mtu(struct net_device *net, int new_mtu)
{
	struct eth_dev	*dev = netdev_priv(net);
	unsigned long	flags;
	int		status = 0;

	/* don't change MTU on "live" link (peer won't know) */
	spin_lock_irqsave(&dev->lock, flags);
	if (dev->port_usb)
		status = -EBUSY;
	else if (new_mtu <= ETH_HLEN || new_mtu > GETHER_MAX_ETH_FRAME_LEN)
		status = -ERANGE;
	else
		net->mtu = new_mtu;
	spin_unlock_irqrestore(&dev->lock, flags);
	U_ETHER_DBG("mtu to %d, status is %d\n", new_mtu, status);

	return status;
}

static void eth_get_drvinfo(struct net_device *net, struct ethtool_drvinfo *p)
{
	struct eth_dev *dev = netdev_priv(net);

	strlcpy(p->driver, "g_ether", sizeof(p->driver));
	strlcpy(p->version, UETH__VERSION, sizeof(p->version));
	strlcpy(p->fw_version, dev->gadget->name, sizeof(p->fw_version));
	strlcpy(p->bus_info, dev_name(&dev->gadget->dev), sizeof(p->bus_info));
}

/* REVISIT can also support:
 *   - WOL (by tracking suspends and issuing remote wakeup)
 *   - msglevel (implies updated messaging)
 *   - ... probably more ethtool ops
 */

static const struct ethtool_ops ops = {
	.get_drvinfo = eth_get_drvinfo,
	.get_link = ethtool_op_get_link,
};

static void defer_kevent(struct eth_dev *dev, int flag)
{
	if (test_and_set_bit(flag, &dev->todo))
		return;
	if (!schedule_work(&dev->work))
		ERROR(dev, "kevent %d may have been dropped\n", flag);
	else
		DBG(dev, "kevent %d scheduled\n", flag);
}

static void rx_complete(struct usb_ep *ep, struct usb_request *req);
static void tx_complete(struct usb_ep *ep, struct usb_request *req);

static int
rx_submit(struct eth_dev *dev, struct usb_request *req, gfp_t gfp_flags)
{
	struct sk_buff	*skb;
	int		retval = -ENOMEM;
	size_t		size = 0;
	struct usb_ep	*out;
	unsigned long	flags;

	spin_lock_irqsave(&dev->lock, flags);
	if (dev->port_usb)
		out = dev->port_usb->out_ep;
	else
		out = NULL;

	if (!out)
	{
		spin_unlock_irqrestore(&dev->lock, flags);
		return -ENOTCONN;
	}

	/* Padding up to RX_EXTRA handles minor disagreements with host.
	 * Normally we use the USB "terminate on short read" convention;
	 * so allow up to (N*maxpacket), since that memory is normally
	 * already allocated.  Some hardware doesn't deal well with short
	 * reads (e.g. DMA must be N*maxpacket), so for now don't trim a
	 * byte off the end (to force hardware errors on overflow).
	 *
	 * RNDIS uses internal framing, and explicitly allows senders to
	 * pad to end-of-packet.  That's potentially nice for speed, but
	 * means receivers can't recover lost synch on their own (because
	 * new packets don't only start after a short RX).
	 */
	size += sizeof(struct ethhdr) + dev->net->mtu + RX_EXTRA;
	size += dev->port_usb->header_len;
	size += out->maxpacket - 1;
	size -= size % out->maxpacket;

	if (dev->ul_max_pkts_per_xfer)
		size *= dev->ul_max_pkts_per_xfer;

	if (dev->port_usb->is_fixed)
		size = max_t(size_t, size, dev->port_usb->fixed_out_len);
	spin_unlock_irqrestore(&dev->lock, flags);

	skb = alloc_skb(size + NET_IP_ALIGN, gfp_flags);
	if (skb == NULL) {
		U_ETHER_DBG("no rx skb\n");
		rndis_test_rx_nomem++;
		goto enomem;
	}

	/* Some platforms perform better when IP packets are aligned,
	 * but on at least one, checksumming fails otherwise.  Note:
	 * RNDIS headers involve variable numbers of LE32 values.
	 */
		skb_reserve(skb, NET_IP_ALIGN);

	req->buf = skb->data;
	req->length = size;
	req->context = skb;

	retval = usb_ep_queue(out, req, gfp_flags);
	if (retval == -ENOMEM)
enomem:
		defer_kevent(dev, WORK_RX_MEMORY);
	if (retval) {
		DBG(dev, "rx submit --> %d\n", retval);
		if (skb)
			dev_kfree_skb_any(skb);
	}
	return retval;
}

static void rx_complete(struct usb_ep *ep, struct usb_request *req)
{
	struct sk_buff	*skb = req->context;
	struct eth_dev	*dev = ep->driver_data;
	int		status = req->status;
	bool		queue = 0;

	switch (status) {

	/* normal completion */
	case 0:
	U_ETHER_DBG("len(%d)\n", req->actual);
		skb_put(skb, req->actual);

		if (dev->unwrap) {
			unsigned long	flags;

			spin_lock_irqsave(&dev->lock, flags);
			if (dev->port_usb) {
				status = dev->unwrap(dev->port_usb,
							skb,
							&dev->rx_frames);
				if (status == -EINVAL)
					dev->net->stats.rx_errors++;
				else if (status == -EOVERFLOW)
					dev->net->stats.rx_over_errors++;
			} else {
				dev_kfree_skb_any(skb);
				status = -ENOTCONN;
			}
			spin_unlock_irqrestore(&dev->lock, flags);
		} else {
			skb_queue_tail(&dev->rx_frames, skb);
		}
		if (!status)
			queue = 1;
		rndis_test_rx_usb_in++;
		break;

	/* software-driven interface shutdown */
	case -ECONNRESET:		/* unlink */
	case -ESHUTDOWN:		/* disconnect etc */
		VDBG(dev, "rx shutdown, code %d\n", status);
		goto quiesce;

	/* for hardware automagic (such as pxa) */
	case -ECONNABORTED:		/* endpoint reset */
		DBG(dev, "rx %s reset\n", ep->name);
		defer_kevent(dev, WORK_RX_MEMORY);
quiesce:
		dev_kfree_skb_any(skb);
		goto clean;

	/* data overrun */
	case -EOVERFLOW:
		dev->net->stats.rx_over_errors++;
		fallthrough;

	default:
		queue = 1;
		dev_kfree_skb_any(skb);
		dev->net->stats.rx_errors++;
		DBG(dev, "rx status %d\n", status);
		break;
	}

clean:
	if (queue && dev->rx_frames.qlen <= u_ether_rx_pending_thld) {
		if (rx_submit(dev, req, GFP_ATOMIC) < 0) {
			spin_lock(&dev->reqrx_lock);
		list_add(&req->list, &dev->rx_reqs);
			spin_unlock(&dev->reqrx_lock);
	}
	} else {
		spin_lock(&dev->reqrx_lock);
	list_add(&req->list, &dev->rx_reqs);
		spin_unlock(&dev->reqrx_lock);
	}


	if (queue) {
		queue_work(uether_wq, &dev->rx_work);
		queue_work(uether_wq1, &dev->rx_work1);
	}

}

static int prealloc(struct list_head *list, struct usb_ep *ep, unsigned n)
{
	unsigned		i;
	struct usb_request	*req;
	bool			usb_in;

	if (!n)
		return -ENOMEM;

	/* queue/recycle up to N requests */
	i = n;
	list_for_each_entry(req, list, list) {
		if (i-- == 0)
			goto extra;
	}

	if (ep->desc->bEndpointAddress & USB_DIR_IN)
		usb_in = true;
	else
		usb_in = false;

	while (i--) {
		req = usb_ep_alloc_request(ep, GFP_ATOMIC);
		if (!req)
			return list_empty(list) ? -ENOMEM : 0;
	/* update completion handler */
		if (usb_in)
			req->complete = tx_complete;
		else
			req->complete = rx_complete;
		list_add(&req->list, list);
	}
	return 0;

extra:
	/* free extras */
	for (;;) {
		struct list_head	*next;

		next = req->list.next;
		list_del(&req->list);
		usb_ep_free_request(ep, req);

		if (next == list)
			break;

		req = container_of(next, struct usb_request, list);
	}
	return 0;
}

static int alloc_requests(struct eth_dev *dev, struct gether *link, unsigned n)
{
	int	status;

	spin_lock(&dev->req_lock);
	status = prealloc(&dev->tx_reqs, link->in_ep, n);
	if (status < 0) {
		spin_unlock(&dev->req_lock);
		U_ETHER_DBG("can't alloc tx requests\n");
		return status;
	}
	spin_unlock(&dev->req_lock);

	spin_lock(&dev->reqrx_lock);
	status = prealloc(&dev->rx_reqs, link->out_ep, n);
	if (status < 0) {
		spin_unlock(&dev->reqrx_lock);
		U_ETHER_DBG("can't alloc rx requests\n");
		return status;
	}
	spin_unlock(&dev->reqrx_lock);

	return status;
}

void rx_fill(struct eth_dev *dev, gfp_t gfp_flags)
{
	struct usb_request	*req;
	unsigned long		flags;
	int			req_cnt = 0;

	/* fill unused rxq slots with some skb */
	spin_lock_irqsave(&dev->reqrx_lock, flags);
	while (!list_empty(&dev->rx_reqs)) {
		/* break the nexus of continuous completion and re-submission*/
		if (++req_cnt > qlen(dev->gadget, dev->qmult))
			break;

		req = container_of(dev->rx_reqs.next,
				struct usb_request, list);
		list_del_init(&req->list);
		spin_unlock_irqrestore(&dev->reqrx_lock, flags);

		if (rx_submit(dev, req, gfp_flags) < 0) {
			spin_lock_irqsave(&dev->reqrx_lock, flags);
			list_add(&req->list, &dev->rx_reqs);
			spin_unlock_irqrestore(&dev->reqrx_lock, flags);
			defer_kevent(dev, WORK_RX_MEMORY);
			return;
		}

		spin_lock_irqsave(&dev->reqrx_lock, flags);
	}
	spin_unlock_irqrestore(&dev->reqrx_lock, flags);
}

static void process_rx_w(struct work_struct *work)
{
	struct eth_dev	*dev = container_of(work, struct eth_dev, rx_work);
	struct sk_buff	*skb;
	int		status = 0;

	if (!dev->port_usb)
		return;

	while ((skb = skb_dequeue(&dev->rx_frames))) {
		if (status < 0
				|| ETH_HLEN > skb->len
				|| skb->len > ETH_FRAME_LEN) {
			dev->net->stats.rx_errors++;
			dev->net->stats.rx_length_errors++;
			rndis_test_rx_error++;
			DBG(dev, "rx length %d\n", skb->len);
			dev_kfree_skb_any(skb);
			continue;
		}
		skb->protocol = eth_type_trans(skb, dev->net);
		dev->net->stats.rx_packets++;
		dev->net->stats.rx_bytes += skb->len;
#if defined(NETDEV_TRACE) && defined(NETDEV_UL_TRACE)
		skb->dbg_flag = 0x4;
#endif

		rndis_test_rx_net_out++;
		status = netif_rx_ni(skb);
	}

    /* move to another workthread */
#if 0
	if (netif_running(dev->net))
		rx_fill(dev, GFP_KERNEL);
#endif
}

static void eth_work(struct work_struct *work)
{
	struct eth_dev	*dev = container_of(work, struct eth_dev, work);

	if (test_and_clear_bit(WORK_RX_MEMORY, &dev->todo)) {
		if (netif_running(dev->net))
			rx_fill(dev, GFP_KERNEL);
	}

	if (dev->todo)
		DBG(dev, "work done, flags = 0x%lx\n", dev->todo);
}

static void process_rx_w1(struct work_struct *work)
{
	struct eth_dev	*dev = container_of(work, struct eth_dev, rx_work1);

	if (!dev->port_usb)
		return;

	if (netif_running(dev->net))
		rx_fill(dev, GFP_KERNEL);
}

static void tx_complete(struct usb_ep *ep, struct usb_request *req)
{
	struct sk_buff	*skb;
	struct eth_dev	*dev;
	struct net_device *net;
	struct usb_request *new_req;
	struct usb_ep *in;
	int length;
	int retval;

	if (!ep->driver_data) {
		usb_ep_free_request(ep, req);
		return;
	}

	dev = ep->driver_data;
	net = dev->net;

	if (!dev->port_usb) {
		usb_ep_free_request(ep, req);
		return;
	}

	switch (req->status) {
	default:
		dev->net->stats.tx_errors++;
		VDBG(dev, "tx err %d\n", req->status);
		fallthrough;
	case -ECONNRESET:		/* unlink */
	case -ESHUTDOWN:		/* disconnect etc */
		break;
	case 0:
		if (!req->zero)
			dev->net->stats.tx_bytes += req->length-1;
		else
			dev->net->stats.tx_bytes += req->length;
	}
	dev->net->stats.tx_packets++;
	rndis_test_tx_complete++;

	spin_lock(&dev->req_lock);
	list_add_tail(&req->list, &dev->tx_reqs);

	if (dev->port_usb->multi_pkt_xfer && !req->context) {
		dev->no_tx_req_used--;
		req->length = 0;
		in = dev->port_usb->in_ep;

		new_req = container_of(dev->tx_reqs.next,
				struct usb_request, list);

		list_del(&new_req->list);
		if (new_req->length > 0) {
			dev->tx_skb_hold_count = 0;
			dev->no_tx_req_used++;
			spin_unlock(&dev->req_lock);

			length = new_req->length;

			/* NCM requires no zlp if transfer is dwNtbInMaxSize */
			if (dev->port_usb->is_fixed &&
					length == dev->port_usb->fixed_in_len &&
					(length % in->maxpacket) == 0)
				new_req->zero = 0;
			else
				new_req->zero = 1;

			/* use zlp framing on tx for strict CDC-Ether
			 * conformance, though any robust network rx
			 * path ignores extra padding. and some hardware
			 * doesn't like to write zlps.
			 */
			if (new_req->zero && !dev->zlp &&
					(length % in->maxpacket) == 0) {
				new_req->zero = 0;
				length++;
			}

			new_req->length = length;
			retval = usb_ep_queue(in, new_req, GFP_ATOMIC);
			switch (retval) {
			default:
				U_ETHER_DBG("%s - tx qerr %d\n", __func__, retval);
				new_req->length = 0;
				spin_lock(&dev->req_lock);
				/* TX OUT OF ORDER FIX */
				dev->no_tx_req_used--;
				list_add(&new_req->list,
						&dev->tx_reqs);
				spin_unlock(&dev->req_lock);
				break;
			case 0:
				/* TX OUT OF ORDER FIX */
				break;
			}
		} else {
			/*
			 * Put the idle request at the back of the
			 * queue. The xmit function will put the
			 * unfinished request at the beginning of the
			 * queue.
			 */
			list_add_tail(&new_req->list, &dev->tx_reqs);
			spin_unlock(&dev->req_lock);
		}
	} else {
		skb = req->context;
		/* Is aggregation already enabled and buffers allocated ? */
		if (dev->port_usb->multi_pkt_xfer && dev->tx_req_bufsize) {
#if IS_ENABLED(CONFIG_64BIT) && IS_ENABLED(CONFIG_MTK_LM_MODE)
			req->buf = kzalloc(dev->tx_req_bufsize,
						GFP_ATOMIC | GFP_DMA);
#else
			req->buf = kzalloc(dev->tx_req_bufsize, GFP_ATOMIC);
#endif
			req->context = NULL;
		} else {
			req->buf = NULL;
		}

		spin_unlock(&dev->req_lock);
		dev_kfree_skb_any(skb);
	}
	atomic_dec(&dev->tx_qlen);
	if (netif_carrier_ok(dev->net)) {
		spin_lock(&dev->req_lock);
		if (dev->no_tx_req_used < tx_wakeup_threshold)
			netif_wake_queue(dev->net);
		spin_unlock(&dev->req_lock);
	}

}

static inline int is_promisc(u16 cdc_filter)
{
	return cdc_filter & USB_CDC_PACKET_TYPE_PROMISCUOUS;
}

static int alloc_tx_buffer(struct eth_dev *dev)
{
	struct list_head	*act;
	struct usb_request	*req;

	dev->tx_req_bufsize = (dev->dl_max_pkts_per_xfer *
				(dev->net->mtu
				+ sizeof(struct ethhdr)
				/* size of rndis_packet_msg_type */
				+ 44
				+ 22));

	list_for_each(act, &dev->tx_reqs) {
		req = container_of(act, struct usb_request, list);
		if (!req->buf) {
#if IS_ENABLED(CONFIG_64BIT) && IS_ENABLED(CONFIG_MTK_LM_MODE)
			req->buf = kzalloc(dev->tx_req_bufsize,
						GFP_ATOMIC | GFP_DMA);
#else
			req->buf = kzalloc(dev->tx_req_bufsize,
						GFP_ATOMIC);
#endif

			if (!req->buf)
				goto free_buf;

		}
		/* req->context is not used for multi_pkt_xfers */
		req->context = NULL;
		}
	return 0;

free_buf:
	/* tx_req_bufsize = 0 retries mem alloc on next eth_start_xmit */
	dev->tx_req_bufsize = 0;
	list_for_each(act, &dev->tx_reqs) {
		req = container_of(act, struct usb_request, list);
		kfree(req->buf);
	req->buf = NULL;
	}
	return -ENOMEM;
}

static netdev_tx_t eth_start_xmit(struct sk_buff *skb,
					struct net_device *net)
{
	struct eth_dev		*dev = netdev_priv(net);
	int			length = 0;
	int			retval;
	struct usb_request	*req = NULL;
	unsigned long		flags;
	struct usb_ep		*in = NULL;
	u16			cdc_filter = 0;
	bool			multi_pkt_xfer = false;
	uint32_t		max_size = 0;
	static unsigned long okCnt, busyCnt;
	static DEFINE_RATELIMIT_STATE(ratelimit1, 1 * HZ, 2);
	static DEFINE_RATELIMIT_STATE(ratelimit2, 1 * HZ, 2);
	static DEFINE_RATELIMIT_STATE(ratelimit3, 1 * HZ, 2);
	struct iphdr *iph;
	ktime_t endtime, deltatime;
	unsigned long long duration;
	struct netdev_queue *dev_queue = NULL;
	struct Qdisc *qdisc;
	struct skb_shared_info *pinfo;
	skb_frag_t *frag;
	unsigned int frag_cnt = 0, frag_idx = 0, frag_data_len = 0, frag_total_len = 0;
	char *frag_data_addr;

	if (ip_debug)
		if ((ip_debug_ratelimit && __ratelimit(&ratelimit3)) ||
			!ip_debug_ratelimit) {
			iph = skb->encapsulation ? inner_ip_hdr(skb) : ip_hdr(skb);
			pr_info("len=0x%x, id=0x%x\n",
				iph->tot_len, iph->id);
		}

	if (uether_tx_profile) {
		#if IS_ENABLED(CONFIG_64BIT)
		if (!tx_profile_start) {
			starttime = ktime_get();
			tx_profile_start = true;
		}

		dev_queue = netdev_get_tx_queue(net, 0);
		qdisc = dev_queue->qdisc;
		endtime = ktime_get();
		deltatime = ktime_sub(endtime, starttime);
		duration = (unsigned long long) ktime_to_ns(deltatime) >> 10;
		rec_data_len += skb->len;

		if (duration > 1000000) {
			pr_info("rndis rec_data_len=%llu duration=%llu,\n"
				"speed=%llu mbps, txq_len =%d\n",
				rec_data_len, duration,
				((rec_data_len/1000000)/(duration/1000000))*8,
				qdisc->q.qlen);
			tx_profile_start = false;
			rec_data_len = 0;
		}
		#endif
	}

	spin_lock_irqsave(&dev->lock, flags);
	if (dev->port_usb) {
		in = dev->port_usb->in_ep;
		cdc_filter = dev->port_usb->cdc_filter;
		multi_pkt_xfer = dev->port_usb->multi_pkt_xfer;
		max_size = dev->dl_max_xfer_size;
	} else {
		in = NULL;
		cdc_filter = 0;
	}
	spin_unlock_irqrestore(&dev->lock, flags);

	if (skb && !in) {
		dev_kfree_skb_any(skb);
		U_ETHER_DBG("%s - wrong direction!\n", __func__);
		return NETDEV_TX_OK;
	}

	/* apply outgoing CDC or RNDIS filters */
	if (skb && !is_promisc(cdc_filter)) {
		u8		*dest = skb->data;

		if (is_multicast_ether_addr(dest)) {
			u16	type;

			/* ignores USB_CDC_PACKET_TYPE_MULTICAST and host
			 * SET_ETHERNET_MULTICAST_FILTERS requests
			 */
			if (is_broadcast_ether_addr(dest))
				type = USB_CDC_PACKET_TYPE_BROADCAST;
			else
				type = USB_CDC_PACKET_TYPE_ALL_MULTICAST;
			if (!(cdc_filter & type)) {
				dev_kfree_skb_any(skb);
				U_ETHER_DBG("%s - filter/type no match\n", __func__);
				return NETDEV_TX_OK;
			}
		}
		/* ignores USB_CDC_PACKET_TYPE_DIRECTED */
	}

	spin_lock_irqsave(&dev->req_lock, flags);

    /* Allocate memory for tx_reqs to support multi packet transfer */
	if (multi_pkt_xfer && !dev->tx_req_bufsize) {
		retval = alloc_tx_buffer(dev);
		if (retval < 0) {
			spin_unlock_irqrestore(&dev->req_lock, flags);
			U_ETHER_DBG("%s - alloc_tx_buffer failed - NOMEM\n", __func__);
			return -ENOMEM;
		}
	}
	if (__ratelimit(&ratelimit1)) {
#if IS_ENABLED(CONFIG_MEDIATEK_SOLUTION)
		usb_boost();
#endif
		U_ETHER_DBG("COM[%d,%d,%x,%x,%lu]\n",
			dev->gadget->speed, max_size, rndis_test_last_msg_id,
			rndis_test_last_resp_id, rndis_test_reset_msg_cnt);

		U_ETHER_DBG("RX[%lu,%lu,%lu,%lu] TX[%lu,%lu,%lu,%lu,%lu]\n",
			rndis_test_rx_usb_in, rndis_test_rx_net_out,
			rndis_test_rx_nomem, rndis_test_rx_error,
			rndis_test_tx_net_in, rndis_test_tx_usb_out,
			rndis_test_tx_busy, rndis_test_tx_stop,
			rndis_test_tx_complete);
	}
	rndis_test_tx_net_in++;
	/*
	 * this freelist can be empty if an interrupt triggered disconnect()
	 * and reconfigured the gadget (shutting down this queue) after the
	 * network stack decided to xmit but before we got the spinlock.
	 */
	if (list_empty(&dev->tx_reqs)) {
		busyCnt++;
		if (__ratelimit(&ratelimit2))
			U_ETHER_DBG("okCnt : %lu, busyCnt : %lu\n",
					okCnt, busyCnt);
		spin_unlock_irqrestore(&dev->req_lock, flags);
		rndis_test_tx_busy++;
		U_ETHER_DBG("%s - TXBUSY\n", __func__);
		return NETDEV_TX_BUSY;
	}
	okCnt++;

	req = container_of(dev->tx_reqs.next, struct usb_request, list);
	list_del(&req->list);

	/* temporarily stop TX queue when the freelist empties */
	if (list_empty(&dev->tx_reqs)) {
		rndis_test_tx_stop++;
		netif_stop_queue(net);
	}
	spin_unlock_irqrestore(&dev->req_lock, flags);

	if (dev->port_usb == NULL) {
		dev_kfree_skb_any(skb);
		U_ETHER_DBG("port_usb NULL\n");
		return NETDEV_TX_OK;
	}

	/*
	 * No buffer copies needed, unless the network stack did it
	 * or the hardware can't use skb buffers or there's not enough
	 * space for extra headers we need.
	 */
	spin_lock_irqsave(&dev->lock, flags);
	if (dev->wrap && dev->port_usb)
		skb = dev->wrap(dev->port_usb, skb);
	spin_unlock_irqrestore(&dev->lock, flags);

	if (!skb) {
		if (!dev->port_usb->supports_multi_frame)
			dev->net->stats.tx_dropped++;
		/* no error code for dropped packets */
		U_ETHER_DBG("%s - skb dropped\n", __func__);
		return NETDEV_TX_OK;
	}

	if (multi_pkt_xfer) {
		U_ETHER_DBG("req->length:%d header_len:%u\n"
				" skb->len:%d skb->data_len:%d\n",
				req->length, dev->header_len,
				skb->len, skb->data_len);
		/* Add RNDIS Header */
		memcpy(req->buf + req->length, dev->port_usb->header,
						dev->header_len);
		/* Increment req length by header size */
		req->length += dev->header_len;
		/* Copy received IP data from SKB */

		if (net->features & NETIF_F_GSO) {
			pinfo = skb_shinfo(skb);
			frag_cnt = pinfo->nr_frags;
		}

		if (frag_cnt == 0) {
			memcpy(req->buf + req->length, skb->data, skb->len);
			/* Increment req length by skb data length */
			req->length += skb->len;
		} else {
			memcpy(req->buf + req->length, skb->data, skb->len - skb->data_len);
			req->length += skb->len - skb->data_len;

			for (frag_idx = 0; frag_idx < frag_cnt; frag_idx++) {
				frag = pinfo->frags + frag_idx;
				frag_data_len = skb_frag_size(frag);
				frag_data_addr = skb_frag_address(frag);

				memcpy(req->buf + req->length, frag_data_addr, frag_data_len);
				frag_total_len += frag_data_len;
				frag_data_addr += frag_data_len;
				req->length += frag_data_len;
			}
		}
		length = req->length;
		dev_kfree_skb_any(skb);

		spin_lock_irqsave(&dev->req_lock, flags);
		dev->tx_skb_hold_count++;
		/* if (dev->tx_skb_hold_count < dev->dl_max_pkts_per_xfer) { */
		if ((dev->tx_skb_hold_count < dev->dl_max_pkts_per_xfer)
				&& (length < (max_size - dev->net->mtu))) {
			if (dev->no_tx_req_used > u_ether_tx_req_threshold) {
				list_add(&req->list, &dev->tx_reqs);
				spin_unlock_irqrestore(&dev->req_lock, flags);
				goto success;
		}
	}

		dev->no_tx_req_used++;
		dev->tx_skb_hold_count = 0;
		spin_unlock_irqrestore(&dev->req_lock, flags);

	} else {
		length = skb->len;
		req->buf = skb->data;
		req->context = skb;
	}


	/* NCM requires no zlp if transfer is dwNtbInMaxSize */
	if (dev->port_usb->is_fixed &&
	    length == dev->port_usb->fixed_in_len &&
	    (length % in->maxpacket) == 0)
		req->zero = 0;
	else
		req->zero = 1;

	/* use zlp framing on tx for strict CDC-Ether conformance,
	 * though any robust network rx path ignores extra padding.
	 * and some hardware doesn't like to write zlps.
	 */
	if (req->zero && !dev->zlp && (length % in->maxpacket) == 0) {
		req->zero = 0;
		length++;
	}

	req->length = length;
#if 0
	/* throttle high/super speed IRQ rate back slightly */
	if (gadget_is_dualspeed(dev->gadget))
		req->no_interrupt = (((dev->gadget->speed == USB_SPEED_HIGH ||
					dev->gadget->speed >= USB_SPEED_SUPER))
					&& !list_empty(&dev->tx_reqs))
			? ((atomic_read(&dev->tx_qlen) % dev->qmult) != 0)
			: 0;
#endif
	retval = usb_ep_queue(in, req, GFP_ATOMIC);
	switch (retval) {
	default:
		U_ETHER_DBG("%s - tx queue err %d\n", __func__, retval);
		dev->no_tx_req_used--;
		break;
	case 0:
		rndis_test_tx_usb_out++;
		atomic_inc(&dev->tx_qlen);
	}

	if (retval) {
		if (!multi_pkt_xfer)
			dev_kfree_skb_any(skb);
		else
			req->length = 0;

		dev->net->stats.tx_dropped++;

		spin_lock_irqsave(&dev->req_lock, flags);
		if (list_empty(&dev->tx_reqs))
			netif_start_queue(net);
		list_add(&req->list, &dev->tx_reqs);
		spin_unlock_irqrestore(&dev->req_lock, flags);
	}
success:
	return NETDEV_TX_OK;
}

/*-------------------------------------------------------------------------*/

static void eth_start(struct eth_dev *dev, gfp_t gfp_flags)
{
	U_ETHER_DBG("\n");

	pr_info("%s - queue_work set_rps_map\n", __func__);
	queue_work(uether_rps_wq, &dev->rps_map_work);

	/* fill the rx queue */
	rx_fill(dev, gfp_flags);

	/* and open the tx floodgates */
	atomic_set(&dev->tx_qlen, 0);
	netif_wake_queue(dev->net);
}

static int eth_open(struct net_device *net)
{
	struct eth_dev	*dev = netdev_priv(net);
	struct gether	*link;

	U_ETHER_DBG("\n");
	if (netif_carrier_ok(dev->net))
		eth_start(dev, GFP_KERNEL);

	spin_lock_irq(&dev->lock);
	link = dev->port_usb;
	if (link && link->open)
		link->open(link);
	spin_unlock_irq(&dev->lock);

	return 0;
}

static int eth_stop(struct net_device *net)
{
	struct eth_dev	*dev = netdev_priv(net);
	unsigned long	flags;

	U_ETHER_DBG("\n");
	pr_info("%s, START !!!!\n", __func__);
	netif_stop_queue(net);

	DBG(dev, "stop stats: rx/tx %ld/%ld, errs %ld/%ld\n",
		dev->net->stats.rx_packets, dev->net->stats.tx_packets,
		dev->net->stats.rx_errors, dev->net->stats.tx_errors
		);

	/* ensure there are no more active requests */
	spin_lock_irqsave(&dev->lock, flags);
	if (dev->port_usb) {
		struct gether	*link = dev->port_usb;
		const struct usb_endpoint_descriptor *in;
		const struct usb_endpoint_descriptor *out;

		if (link->close)
			link->close(link);

		/* NOTE:  we have no abort-queue primitive we could use
		 * to cancel all pending I/O.  Instead, we disable then
		 * reenable the endpoints ... this idiom may leave toggle
		 * wrong, but that's a self-correcting error.
		 *
		 * REVISIT:  we *COULD* just let the transfers complete at
		 * their own pace; the network stack can handle old packets.
		 * For the moment we leave this here, since it works.
		 */
		in = link->in_ep->desc;
		out = link->out_ep->desc;
		usb_ep_disable(link->in_ep);
		usb_ep_disable(link->out_ep);
		if (netif_carrier_ok(net)) {
			DBG(dev, "host still using in/out endpoints\n");
			link->in_ep->desc = in;
			link->out_ep->desc = out;
			usb_ep_enable(link->in_ep);
			usb_ep_enable(link->out_ep);
		}
	}
	spin_unlock_irqrestore(&dev->lock, flags);
	pr_info("%s, END !!!!\n", __func__);

	return 0;
}

/*-------------------------------------------------------------------------*/


static int get_ether_addr(const char *str, u8 *dev_addr)
{
	if (str) {
		unsigned	i;

		for (i = 0; i < 6; i++) {
			unsigned char num;

			if ((*str == '.') || (*str == ':'))
				str++;
			num = hex_to_bin(*str++) << 4;
			num |= hex_to_bin(*str++);
			dev_addr [i] = num;
		}
		if (is_valid_ether_addr(dev_addr))
			return 0;
	}
	eth_random_addr(dev_addr);
	return 1;
}

static int get_ether_addr_str(u8 dev_addr[ETH_ALEN], char *str, int len)
{
	if (len < 18)
		return -EINVAL;

	snprintf(str, len, "%pM", dev_addr);
	return 18;
}

static void set_rps_map_work(struct work_struct *work)
{
	struct eth_dev	*dev = container_of(work, struct eth_dev, rps_map_work);

	if (!dev->port_usb)
		return;

#if IS_ENABLED(CONFIG_MTK_NET_RPS)
	pr_info("%s - set rps to 0xff\n", __func__);
	set_rps_map(dev->net->_rx, 0xff);
#else
	pr_info("%s - cannot set rps, CONFIG_MTK_NET_RPS is not set\n", __func__);
#endif
}

/* defined but not used due to MAC customization */
#if 0
static u8 host_ethaddr[ETH_ALEN];

static int get_host_ether_addr(u8 *str, u8 *dev_addr)
{
	/* memcpy(dev_addr, str, ETH_ALEN); */
	ether_addr_copy(dev_addr, str);
	if (is_valid_ether_addr(dev_addr))
		return 0;

	random_ether_addr(dev_addr);
	ether_addr_copy(dev_addr, str);
	/* memcpy(str, dev_addr, ETH_ALEN); */
	return 1;
}
#endif
static const struct net_device_ops eth_netdev_ops = {
	.ndo_open		= eth_open,
	.ndo_stop		= eth_stop,
	.ndo_start_xmit		= eth_start_xmit,
	.ndo_change_mtu		= ueth_change_mtu,
	.ndo_set_mac_address 	= eth_mac_addr,
	.ndo_validate_addr	= eth_validate_addr,
};

static struct device_type gadget_type = {
	.name	= "gadget",
};

/**
 * mtk_gether_setup_name - initialize one ethernet-over-usb link
 * @g: gadget to associated with these links
 * @ethaddr: NULL, or a buffer in which the ethernet address of the
 *	host side of the link is recorded
 * @netname: name for network device (for example, "usb")
 * Context: may sleep
 *
 * This sets up the single network link that may be exported by a
 * gadget driver using this framework.  The link layer addresses are
 * set up using module parameters.
 *
 * Returns an eth_dev pointer on success, or an ERR_PTR on failure.
 */
struct eth_dev *mtk_gether_setup_name(struct usb_gadget *g,
		const char *dev_addr, const char *host_addr,
		u8 ethaddr[ETH_ALEN], unsigned qmult, const char *netname)
{
	struct eth_dev		*dev;
	struct net_device	*net;
	int			status;
	static unsigned char a[6] = {0x06, 0x16, 0x26, 0x36, 0x46, 0x56};

	net = alloc_etherdev(sizeof *dev);
	if (!net)
		return ERR_PTR(-ENOMEM);

	dev = netdev_priv(net);
	spin_lock_init(&dev->lock);
	spin_lock_init(&dev->req_lock);
	spin_lock_init(&dev->reqrx_lock);
	INIT_WORK(&dev->work, eth_work);
	INIT_WORK(&dev->rx_work, process_rx_w);
	INIT_WORK(&dev->rx_work1, process_rx_w1);
	INIT_WORK(&dev->rps_map_work, set_rps_map_work);
	INIT_LIST_HEAD(&dev->tx_reqs);
	INIT_LIST_HEAD(&dev->rx_reqs);

	skb_queue_head_init(&dev->rx_frames);

	/* network device setup */
	dev->net = net;
	dev->qmult = qmult;
	snprintf(net->name, sizeof(net->name), "%s%%d", netname);

#if 0
	if (get_ether_addr(dev_addr, net->dev_addr))
		dev_info(&g->dev, "using random %s ethernet address\n", "self");

	if (get_ether_addr(host_addr, dev->host_mac))
		dev_info(&g->dev, "using random %s ethernet address\n", "host");
#else
	if (get_ether_addr(dev_addr, net->dev_addr))
		dev_info(&g->dev,
			"using random %s ethernet address\n", "self");

	ether_addr_copy(dev->host_mac, a);
	pr_debug("%s, rndis: %x:%x:%x:%x:%x:%x\n", __func__,
		   dev->host_mac[0], dev->host_mac[1],
		   dev->host_mac[2], dev->host_mac[3],
		   dev->host_mac[4], dev->host_mac[5]);
#endif

	if (ethaddr)
		memcpy(ethaddr, dev->host_mac, ETH_ALEN);

	net->netdev_ops = &eth_netdev_ops;

	net->ethtool_ops = &ops;

	dev->gadget = g;
	SET_NETDEV_DEV(net, &g->dev);
	SET_NETDEV_DEVTYPE(net, &gadget_type);

	status = register_netdev(net);
	if (status < 0) {
		dev_dbg(&g->dev, "register_netdev failed, %d\n", status);
		free_netdev(net);
		dev = ERR_PTR(status);
	} else {
		INFO(dev, "MAC %pM\n", net->dev_addr);
		INFO(dev, "HOST MAC %pM\n", dev->host_mac);

		/*
		 * two kinds of host-initiated state changes:
		 *  - iff DATA transfer is active, carrier is "on"
		 *  - tx queueing enabled if open *and* carrier is "on"
		 */
		netif_carrier_off(net);
	}

	return dev;
}
EXPORT_SYMBOL_GPL(mtk_gether_setup_name);

struct net_device *mtk_gether_setup_name_default(const char *netname)
{
	struct net_device	*net;
	struct eth_dev		*dev;

	net = alloc_etherdev(sizeof(*dev));
	if (!net)
		return ERR_PTR(-ENOMEM);

	dev = netdev_priv(net);
	spin_lock_init(&dev->lock);
	spin_lock_init(&dev->req_lock);
	spin_lock_init(&dev->reqrx_lock);
	INIT_WORK(&dev->work, eth_work);
	INIT_WORK(&dev->rx_work, process_rx_w);
	INIT_WORK(&dev->rx_work1, process_rx_w1);
	INIT_WORK(&dev->rps_map_work, set_rps_map_work);
	INIT_LIST_HEAD(&dev->tx_reqs);
	INIT_LIST_HEAD(&dev->rx_reqs);

	skb_queue_head_init(&dev->rx_frames);

	/* network device setup */
	if (rndis_gso) {
		net->features |= NETIF_F_GSO | NETIF_F_SG;
		net->hw_features |= NETIF_F_GSO | NETIF_F_SG;
	}

	if (rndis_mtu)
		net->mtu = rndis_mtu;

	dev->net = net;
	dev->qmult = qmult;
	pr_info("%s - qmult:%d\n", __func__, qmult);
	snprintf(net->name, sizeof(net->name), "%s%%d", netname);

	eth_random_addr(dev->dev_mac);
	pr_info("using random %s ethernet address\n", "self");
	eth_random_addr(dev->host_mac);
	pr_info("using random %s ethernet address\n", "host");

	net->netdev_ops = &eth_netdev_ops;

	net->ethtool_ops = &ops;
	SET_NETDEV_DEVTYPE(net, &gadget_type);

	return net;
}
EXPORT_SYMBOL_GPL(mtk_gether_setup_name_default);

int mtk_gether_register_netdev(struct net_device *net)
{
	struct eth_dev *dev;
	struct usb_gadget *g;
	struct sockaddr sa;
	int status;

	if (!net->dev.parent)
		return -EINVAL;
	dev = netdev_priv(net);
	g = dev->gadget;
	status = register_netdev(net);
	if (status < 0) {
		dev_dbg(&g->dev, "register_netdev failed, %d\n", status);
		return status;
	} else {
		INFO(dev, "HOST MAC %pM\n", dev->host_mac);

		/* two kinds of host-initiated state changes:
		 *  - iff DATA transfer is active, carrier is "on"
		 *  - tx queueing enabled if open *and* carrier is "on"
		 */
		netif_carrier_off(net);
	}
	sa.sa_family = net->type;
	memcpy(sa.sa_data, dev->dev_mac, ETH_ALEN);
	rtnl_lock();
	status = dev_set_mac_address(net, &sa, NULL);
	rtnl_unlock();
	if (status)
		pr_warn("cannot set self ethernet address: %d\n", status);
	else
		INFO(dev, "MAC %pM\n", dev->dev_mac);

	return status;
}
EXPORT_SYMBOL_GPL(mtk_gether_register_netdev);

void mtk_gether_set_gadget(struct net_device *net, struct usb_gadget *g)
{
	struct eth_dev *dev;

	dev = netdev_priv(net);
	dev->gadget = g;
	SET_NETDEV_DEV(net, &g->dev);
}
EXPORT_SYMBOL_GPL(mtk_gether_set_gadget);

int mtk_gether_set_dev_addr(struct net_device *net, const char *dev_addr)
{
	struct eth_dev *dev;
	u8 new_addr[ETH_ALEN];

	memset(new_addr, 0, ETH_ALEN);
	dev = netdev_priv(net);
	if (get_ether_addr(dev_addr, new_addr))
		return -EINVAL;
	memcpy(dev->dev_mac, new_addr, ETH_ALEN);
	return 0;
}
EXPORT_SYMBOL_GPL(mtk_gether_set_dev_addr);

int mtk_gether_get_dev_addr(struct net_device *net, char *dev_addr, int len)
{
	struct eth_dev *dev;

	dev = netdev_priv(net);
	return get_ether_addr_str(dev->dev_mac, dev_addr, len);
}
EXPORT_SYMBOL_GPL(mtk_gether_get_dev_addr);

int mtk_gether_set_host_addr(struct net_device *net, const char *host_addr)
{
	struct eth_dev *dev;
	u8 new_addr[ETH_ALEN];

	memset(new_addr, 0, ETH_ALEN);
	dev = netdev_priv(net);
	if (get_ether_addr(host_addr, new_addr))
		return -EINVAL;
	memcpy(dev->host_mac, new_addr, ETH_ALEN);
	return 0;
}
EXPORT_SYMBOL_GPL(mtk_gether_set_host_addr);

int mtk_gether_get_host_addr(struct net_device *net, char *host_addr, int len)
{
	struct eth_dev *dev;

	dev = netdev_priv(net);
	return get_ether_addr_str(dev->host_mac, host_addr, len);
}
EXPORT_SYMBOL_GPL(mtk_gether_get_host_addr);

int mtk_gether_get_host_addr_cdc(struct net_device *net, char *host_addr, int len)
{
	struct eth_dev *dev;

	if (len < 13)
		return -EINVAL;

	dev = netdev_priv(net);
	snprintf(host_addr, len, "%pm", dev->host_mac);

	return strlen(host_addr);
}
EXPORT_SYMBOL_GPL(mtk_gether_get_host_addr_cdc);

void mtk_gether_get_host_addr_u8(struct net_device *net, u8 host_mac[ETH_ALEN])
{
	struct eth_dev *dev;

	dev = netdev_priv(net);
	memcpy(host_mac, dev->host_mac, ETH_ALEN);
}
EXPORT_SYMBOL_GPL(mtk_gether_get_host_addr_u8);

void mtk_gether_set_qmult(struct net_device *net, unsigned qmult)
{
	struct eth_dev *dev;

	dev = netdev_priv(net);
	dev->qmult = qmult;
}
EXPORT_SYMBOL_GPL(mtk_gether_set_qmult);

unsigned mtk_gether_get_qmult(struct net_device *net)
{
	struct eth_dev *dev;

	dev = netdev_priv(net);
	return dev->qmult;
}
EXPORT_SYMBOL_GPL(mtk_gether_get_qmult);

int mtk_gether_get_ifname(struct net_device *net, char *name, int len)
{
	rtnl_lock();
	strlcpy(name, netdev_name(net), len);
	rtnl_unlock();
	return strlen(name);
}
EXPORT_SYMBOL_GPL(mtk_gether_get_ifname);

void gether_update_dl_max_xfer_size(struct gether *link, uint32_t s)
{
	struct eth_dev		*dev = link->ioport;
	unsigned long flags;

	spin_lock_irqsave(&dev->lock, flags);
	dev->dl_max_xfer_size = s;
	spin_unlock_irqrestore(&dev->lock, flags);
}
EXPORT_SYMBOL_GPL(gether_update_dl_max_xfer_size);

/**
 * mtk_gether_cleanup - remove Ethernet-over-USB device
 * Context: may sleep
 *
 * This is called to free all resources allocated by @gether_setup().
 */
void mtk_gether_cleanup(struct eth_dev *dev)
{
	if (!dev)
		return;

	unregister_netdev(dev->net);
	flush_work(&dev->work);
	free_netdev(dev->net);
}
EXPORT_SYMBOL_GPL(mtk_gether_cleanup);

/**
 * mtk_gether_connect - notify network layer that USB link is active
 * @link: the USB link, set up with endpoints, descriptors matching
 *	current device speed, and any framing wrapper(s) set up.
 * Context: irqs blocked
 *
 * This is called to activate endpoints and let the network layer know
 * the connection is active ("carrier detect").  It may cause the I/O
 * queues to open and start letting network packets flow, but will in
 * any case activate the endpoints so that they respond properly to the
 * USB host.
 *
 * Verify net_device pointer returned using IS_ERR().  If it doesn't
 * indicate some error code (negative errno), ep->driver_data values
 * have been overwritten.
 */
struct net_device *mtk_gether_connect(struct gether *link)
{
	struct eth_dev		*dev = link->ioport;
	int			result = 0;

	if (!dev)
		return ERR_PTR(-EINVAL);
	link->header = kzalloc(sizeof(struct rndis_packet_msg_type),
			GFP_ATOMIC);
	if (!link->header) {
		result = -ENOMEM;
		goto fail;
	}

	U_ETHER_DBG("\n");
	link->in_ep->driver_data = dev;
	result = usb_ep_enable(link->in_ep);
	if (result != 0) {
		DBG(dev, "enable %s --> %d\n",
			link->in_ep->name, result);
		goto fail0;
	}

	link->out_ep->driver_data = dev;
	result = usb_ep_enable(link->out_ep);
	if (result != 0) {
		DBG(dev, "enable %s --> %d\n",
			link->out_ep->name, result);
		goto fail1;
	}

	if (result == 0)
		result = alloc_requests(dev, link, qlen(dev->gadget,
					dev->qmult));

	if (result == 0) {
		dev->zlp = link->is_zlp_ok;
		DBG(dev, "qlen %d\n", qlen(dev->gadget, dev->qmult));

		dev->header_len = link->header_len;
		dev->unwrap = link->unwrap;
		dev->wrap = link->wrap;
		dev->ul_max_pkts_per_xfer = link->ul_max_pkts_per_xfer;
		dev->dl_max_pkts_per_xfer = link->dl_max_pkts_per_xfer;
		dev->dl_max_xfer_size = link->dl_max_transfer_len;

		spin_lock(&dev->lock);
		dev->tx_skb_hold_count = 0;
		dev->no_tx_req_used = 0;
		dev->tx_req_bufsize = 0;
		dev->port_usb = link;
		if (netif_running(dev->net)) {
			if (link->open)
				link->open(link);
		} else {
			if (link->close)
				link->close(link);
		}
		spin_unlock(&dev->lock);

		netif_carrier_on(dev->net);
		if (netif_running(dev->net))
			eth_start(dev, GFP_ATOMIC);

	/* on error, disable any endpoints  */
	} else {
		(void) usb_ep_disable(link->out_ep);
fail1:
		(void) usb_ep_disable(link->in_ep);
	}

	/* caller is responsible for cleanup on error */
	if (result < 0) {
fail0:
		kfree(link->header);
fail:
		return ERR_PTR(result);
	}
	return dev->net;
}
EXPORT_SYMBOL_GPL(mtk_gether_connect);

/**
 * mtk_gether_disconnect - notify network layer that USB link is inactive
 * @link: the USB link, on which mtk_gether_connect() was called
 * Context: irqs blocked
 *
 * This is called to deactivate endpoints and let the network layer know
 * the connection went inactive ("no carrier").
 *
 * On return, the state is as if mtk_gether_connect() had never been called.
 * The endpoints are inactive, and accordingly without active USB I/O.
 * Pointers to endpoint descriptors and endpoint private data are nulled.
 */
void mtk_gether_disconnect(struct gether *link)
{
	struct eth_dev		*dev = link->ioport;
	struct usb_request	*req;
	struct sk_buff		*skb;

	WARN_ON(!dev);
	if (!dev)
		return;

	U_ETHER_DBG("\n");

	rndis_test_rx_usb_in = 0;
	rndis_test_rx_net_out = 0;
	rndis_test_rx_nomem = 0;
	rndis_test_rx_error = 0;

	rndis_test_tx_net_in = 0;
	rndis_test_tx_busy = 0;
	rndis_test_tx_stop = 0;

	rndis_test_tx_usb_out = 0;
	rndis_test_tx_complete = 0;

	netif_stop_queue(dev->net);
	netif_carrier_off(dev->net);

	/* disable endpoints, forcing (synchronous) completion
	 * of all pending i/o.  then free the request objects
	 * and forget about the endpoints.
	 */
	usb_ep_disable(link->in_ep);
	spin_lock(&dev->req_lock);
	while (!list_empty(&dev->tx_reqs)) {
		req = container_of(dev->tx_reqs.next,
					struct usb_request, list);
		list_del(&req->list);

		spin_unlock(&dev->req_lock);
		if (link->multi_pkt_xfer) {
			kfree(req->buf);
			req->buf = NULL;
		}
		usb_ep_free_request(link->in_ep, req);
		spin_lock(&dev->req_lock);
	}
	kfree(link->header);
	link->header = NULL;
	spin_unlock(&dev->req_lock);
	link->in_ep->desc = NULL;

	usb_ep_disable(link->out_ep);
	spin_lock(&dev->reqrx_lock);
	while (!list_empty(&dev->rx_reqs)) {
		req = container_of(dev->rx_reqs.next,
					struct usb_request, list);
		list_del(&req->list);

		spin_unlock(&dev->reqrx_lock);
		usb_ep_free_request(link->out_ep, req);
		spin_lock(&dev->reqrx_lock);
	}
	spin_unlock(&dev->reqrx_lock);

	spin_lock(&dev->rx_frames.lock);
	while ((skb = __skb_dequeue(&dev->rx_frames)))
		dev_kfree_skb_any(skb);
	spin_unlock(&dev->rx_frames.lock);

	link->out_ep->desc = NULL;

	/* finish forgetting about this USB link episode */
	dev->header_len = 0;
	dev->unwrap = NULL;
	dev->wrap = NULL;

	spin_lock(&dev->lock);
	dev->port_usb = NULL;
	spin_unlock(&dev->lock);
}
EXPORT_SYMBOL_GPL(mtk_gether_disconnect);

static int __init gether_init(void)
{
	uether_wq  = create_singlethread_workqueue("uether");
	if (!uether_wq) {
		pr_info("%s: create workqueue fail: uether\n", __func__);
		return -ENOMEM;
	}
	uether_wq1  = create_singlethread_workqueue("uether_rx1");
	if (!uether_wq1) {
		destroy_workqueue(uether_wq);
		pr_info("%s: create workqueue fail: uether_rx1\n", __func__);
		return -ENOMEM;
	}
	uether_rps_wq  = create_singlethread_workqueue("uether_rps");
	if (!uether_rps_wq)
		pr_info("%s: create workqueue fail: uether_rps\n", __func__);
	return 0;
}
module_init(gether_init);

static void __exit gether_exit(void)
{
	destroy_workqueue(uether_wq);
	destroy_workqueue(uether_wq1);
	destroy_workqueue(uether_rps_wq);
}
module_exit(gether_exit);
MODULE_AUTHOR("David Brownell");
MODULE_DESCRIPTION("ethernet over USB driver");
MODULE_LICENSE("GPL v2");
