/* SPDX-License-Identifier: GPL-2.0 */
/*
 * u_ether.c -- Ethernet-over-USB link layer utilities for Gadget stack
 *
 * Copyright (C) 2003-2005,2008 David Brownell
 * Copyright (C) 2003-2004 Robert Schwebel, Benedikt Spranger
 * Copyright (C) 2008 Nokia Corporation
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
#include <linux/debugfs.h>
#include <uapi/linux/sched/types.h>
#include "sprd_u_ether.h"
#include "sprd_rndis.h"


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

static struct workqueue_struct	*uether_tx_wq;
static int tx_start_threshold = 1500;
static int tx_stop_threshold = 2000;

/* this refers to max number sgs per transfer
 * which includes headers/data packets
 */
#define DL_MAX_PKTS_PER_XFER	20

struct eth_dev {
	/* lock is held while accessing port_usb
	 */
	spinlock_t		lock;
	struct gether		*port_usb;

	struct net_device	*net;
	struct usb_gadget	*gadget;

	spinlock_t		req_lock;	/* guard {rx,tx}_reqs */
	struct list_head	tx_reqs, rx_reqs;
	u32			tx_qlen;
	/* Minimum number of TX USB request queued to UDC */
#define TX_REQ_THRESHOLD	5
	int			tx_work_status;
	int			tx_req_status;
	int			no_tx_req_used;
	int			tx_skb_hold_count;
	u32			tx_req_bufsize;
	struct sk_buff_head	tx_skb_q;

	struct sk_buff_head	rx_frames;

	unsigned		qmult;

	unsigned		header_len;
	u32			ul_max_pkts_per_xfer;
	u32			dl_max_pkts_per_xfer;
	u32			dl_max_xfer_size;
	struct sk_buff		*(*wrap)(struct gether *, struct sk_buff *skb);
	int			(*unwrap)(struct gether *,
						struct sk_buff *skb,
						struct sk_buff_head *list);

	struct work_struct	work;
	struct task_struct	*rx_thread;
	struct work_struct	tx_work;

	unsigned long		todo;
#define	WORK_RX_MEMORY		0

	bool			zlp;
	bool			no_skb_reserve;
	u8			host_mac[ETH_ALEN];
	u8			dev_mac[ETH_ALEN];

	/* stats */
	unsigned long		tx_throttle;
	unsigned int		tx_aggr_cnt[DL_MAX_PKTS_PER_XFER];
	unsigned int		tx_pkts_rcvd;
	unsigned int		loop_brk_cnt;
	struct dentry		*uether_dent;
	struct dentry		*uether_dfile;

	bool			sg_enabled;
};

/* when sg is enabled, sg_ctx is used to track skb each usb request will
 * xfer
 */
struct sg_ctx {
	struct sk_buff_head	skbs;
};
static void uether_debugfs_init(struct eth_dev *dev, const char *n);
static void uether_debugfs_exit(struct eth_dev *dev);
/*-------------------------------------------------------------------------*/

#define RX_EXTRA	20	/* bytes guarding against rx overflows */

#define DEFAULT_QLEN	2	/* double buffering by default */

/*
 * Usually downlink rates are higher than uplink rates and it
 * deserve higher number of requests. For CAT-6 data rates of
 * 300Mbps (~30 packets per milli-sec) 40 usb request may not
 * be sufficient. At this rate and with interrupt moderation
 * of interconnect, data can be very bursty. tx_qmult is the
 * additional multipler on qmult.
 */
static u32 tx_qmult = 1;
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
	printk(level "%s: " fmt, (d)->net->name, ## args)

#ifdef DEBUG
#undef DEBUG
#define DBG(dev, fmt, args...) \
	xprintk(dev, KERN_DEBUG, fmt, ## args)
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
	xprintk(dev, KERN_ERR, fmt, ## args)
#define INFO(dev, fmt, args...) \
	xprintk(dev, KERN_INFO, fmt, ## args)

/*-------------------------------------------------------------------------*/

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
#ifdef CONFIG_USB_PAM
	if (dev->port_usb &&
	    dev->port_usb->out_ep &&
	    (dev->port_usb->out_ep->android_kabi_reserved1 == 0xff))
		return;
#endif

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

	if (dev->port_usb) {
		out = dev->port_usb->out_ep;

		/* Padding up to RX_EXTRA handles minor disagreements with host.
		 * Normally we use the USB "terminate on short read" convention;
		 * so allow up to (N*maxpacket), since that memory is normally
		 * already allocated.  Some hardware doesn't deal well with
		 * short reads (e.g. DMA must be N*maxpacket), so for now don't
		 * trim a byte off the end (force hardware errors on overflow).
		 *
		 * RNDIS uses internal framing, and explicitly allows senders to
		 * pad to end-of-packet.  That's potentially nice for speed, but
		 * means receivers can't recover lost synch on their own
		 * (because new packets don't only start after a short RX).
		 */
		size += sizeof(struct ethhdr) + dev->net->mtu + RX_EXTRA;
		size += dev->port_usb->header_len;
		size += out->maxpacket - 1;
		size -= size % out->maxpacket;
		if (dev->ul_max_pkts_per_xfer)
			size *= dev->ul_max_pkts_per_xfer;
		if (dev->port_usb->is_fixed)
			size = max_t(size_t, size,
					dev->port_usb->fixed_out_len);
	} else {
		out = NULL;
	}

	spin_unlock_irqrestore(&dev->lock, flags);

#ifdef CONFIG_USB_PAM
	if (!out || !dev->port_usb || (out->android_kabi_reserved1 == 0xff)) {
		INFO(dev, "usb %s:size: %zd return \n", __func__, size);
		return -ENOTCONN;
	}
#endif
	DBG(dev, "%s: size: %zd\n", __func__, size);
	skb = alloc_skb(size + NET_IP_ALIGN, gfp_flags);
	if (skb == NULL) {
		dev->net->stats.rx_over_errors++;
		ERROR(dev, "alloc rx skb failed\n");
		goto enomem;
	}
	/* Some platforms perform better when IP packets are aligned,
	 * but on at least one, checksumming fails otherwise.  Note:
	 * RNDIS headers involve variable numbers of LE32 values.
	 */
	skb_reserve(skb, NET_IP_ALIGN);

	req->buf = skb->data;
	req->length = size;
	req->complete = rx_complete;
	req->context = skb;

	if (out)
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
	spin_lock(&dev->req_lock);
	list_add(&req->list, &dev->rx_reqs);
	spin_unlock(&dev->req_lock);

	if (queue)
		wake_up_process(dev->rx_thread);
}

static int prealloc_sg(struct list_head *list, struct usb_ep *ep, u32 n,
		bool sg_supported, int hlen)
{
	u32			i;
	struct usb_request	*req;
	struct sg_ctx		*sg_ctx;

	if (!n)
		return -ENOMEM;

	/* queue/recycle up to N requests */
	i = n;
	list_for_each_entry(req, list, list) {
		if (i-- == 0)
			goto extra;
	}
	while (i--) {
		req = usb_ep_alloc_request(ep, GFP_ATOMIC);
		if (!req)
			return list_empty(list) ? -ENOMEM : 0;

		list_add(&req->list, list);
		req->complete = tx_complete;
		if (!sg_supported)
			continue;
		req->sg = kmalloc(
				DL_MAX_PKTS_PER_XFER *
				sizeof(struct scatterlist),
				GFP_ATOMIC);
		if (!req->sg)
			goto extra;
		sg_ctx = kmalloc(sizeof(*sg_ctx), GFP_ATOMIC);
		if (!sg_ctx)
			goto extra;
		req->context = sg_ctx;
		req->buf = kzalloc(DL_MAX_PKTS_PER_XFER * hlen,
					GFP_ATOMIC);
		if (!req->buf)
			goto extra;
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

		if (!sg_supported)
			continue;
		if (!req->sg)
			kfree(req->sg);
		if (!req->context)
			kfree(req->context);
		if (!req->buf)
			kfree(req->buf);
	}
	return -ENOMEM;
}

static int prealloc(struct list_head *list, struct usb_ep *ep, unsigned n)
{
	unsigned		i;
	struct usb_request	*req;

	if (!n)
		return -ENOMEM;

	/* queue/recycle up to N requests */
	i = n;
	list_for_each_entry(req, list, list) {
		if (i-- == 0)
			goto extra;
	}
	while (i--) {
		req = usb_ep_alloc_request(ep, GFP_ATOMIC);
		if (!req)
			return list_empty(list) ? -ENOMEM : 0;
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
	status = prealloc_sg(&dev->tx_reqs, link->in_ep, n * tx_qmult,
				dev->sg_enabled,
				dev->header_len);
	if (status < 0)
		goto fail;
	status = prealloc(&dev->rx_reqs, link->out_ep, n);
	if (status < 0)
		goto fail;
	goto done;
fail:
	DBG(dev, "can't alloc requests\n");
done:
	spin_unlock(&dev->req_lock);
	return status;
}

static void rx_fill(struct eth_dev *dev, gfp_t gfp_flags)
{
	struct usb_request	*req;
	unsigned long		flags;
	int			req_cnt = 0;
	int			ret;

	/* fill unused rxq slots with some skb */
	spin_lock_irqsave(&dev->req_lock, flags);
	while (!list_empty(&dev->rx_reqs)) {
		/* break the nexus of continuous completion and re-submission*/
		if (++req_cnt > qlen(dev->gadget, dev->qmult))
			break;

		req = list_first_entry(&dev->rx_reqs, struct usb_request, list);
		list_del_init(&req->list);
		spin_unlock_irqrestore(&dev->req_lock, flags);
		ret = rx_submit(dev, req, gfp_flags);
		if (ret < 0) {
			spin_lock_irqsave(&dev->req_lock, flags);
			list_add(&req->list, &dev->rx_reqs);
			spin_unlock_irqrestore(&dev->req_lock, flags);
			if (ret != -ESHUTDOWN && dev->port_usb)
				defer_kevent(dev, WORK_RX_MEMORY);
			return;
		}

		spin_lock_irqsave(&dev->req_lock, flags);
	}
	spin_unlock_irqrestore(&dev->req_lock, flags);
}

static int process_rx_w(void *data)
{
	struct eth_dev	*dev = (struct eth_dev *)data;
	struct sk_buff	*skb;
	int		status = 0;

	set_user_nice(current, -20);
	while (!kthread_should_stop()) {
		skb = skb_dequeue(&dev->rx_frames);
		if (!skb) {
			set_current_state(TASK_INTERRUPTIBLE);
			schedule_timeout(HZ/5);
			set_current_state(TASK_RUNNING);
			continue;
		}
		if (!dev->port_usb)
			continue;

		do {
			if (status < 0 || ETH_HLEN > skb->len
			    || skb->len > ETH_FRAME_LEN) {
				dev->net->stats.rx_errors++;
				dev->net->stats.rx_length_errors++;
				DBG(dev, "rx length %d\n", skb->len);
				dev_kfree_skb_any(skb);
				status = 0;
				continue;
			}

			skb->protocol = eth_type_trans(skb, dev->net);
			dev->net->stats.rx_packets++;
			dev->net->stats.rx_bytes += skb->len;

			local_bh_disable();
			status = netif_receive_skb(skb);
			local_bh_enable();
		} while ((skb = skb_dequeue(&dev->rx_frames)));

		if (netif_running(dev->net))
			rx_fill(dev, GFP_KERNEL);
	}
	return 0;
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

static void do_tx_queue_work(struct eth_dev *dev)
{
	unsigned long		flags;
	u32			max_num_pkts;

	spin_lock_irqsave(&dev->lock, flags);
	max_num_pkts = dev->dl_max_pkts_per_xfer;
	if (!max_num_pkts)
		max_num_pkts = 1;

	if ((dev->tx_skb_q.qlen && !dev->tx_req_status)
		|| (!dev->tx_work_status
		&& dev->tx_skb_q.qlen >= max_num_pkts)) {
		spin_unlock_irqrestore(&dev->lock, flags);
		queue_work(uether_tx_wq, &dev->tx_work);
		return;
	}
	spin_unlock_irqrestore(&dev->lock, flags);
}

static void tx_complete(struct usb_ep *ep, struct usb_request *req)
{
	struct sk_buff	*skb = req->context;
	struct eth_dev	*dev = ep->driver_data;
	struct usb_request *new_req;
	struct usb_ep *in;
	int n = 1;
	int length;
	int retval;

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

	if (req->num_sgs) {
		struct sg_ctx *sg_ctx = req->context;
		struct sk_buff *tmp;
		struct sk_buff_head *list;

		n = skb_queue_len(&sg_ctx->skbs);
		dev->tx_aggr_cnt[n-1]++;

		/* sg_ctx is only accessible here, can use lock-free version */
		list = &sg_ctx->skbs;
		while ((tmp = __skb_dequeue(list)) != NULL)
			dev_kfree_skb_any(tmp);
	}

	dev->net->stats.tx_packets++;

	spin_lock(&dev->req_lock);
	list_add_tail(&req->list, &dev->tx_reqs);

	if (req->num_sgs) {
		if (dev->tx_req_status > 0)
			dev->tx_req_status--;
		if (!req->status)
			do_tx_queue_work(dev);

		spin_unlock(&dev->req_lock);
		return;
	}

	if (dev->port_usb->multi_pkt_xfer) {
		dev->no_tx_req_used--;
		req->length = 0;
		in = dev->port_usb->in_ep;

		if (!list_empty(&dev->tx_reqs)) {
			new_req = list_first_entry(&dev->tx_reqs, struct usb_request, list);

			if (new_req->length > 0) {
				list_del(&new_req->list);
				spin_unlock(&dev->req_lock);
				length = new_req->length;

				/* NCM requires no zlp if transfer is
				 * dwNtbInMaxSize
				 */
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
					DBG(dev, "tx queue err %d\n", retval);
					break;
				case 0:
					spin_lock(&dev->req_lock);
					dev->no_tx_req_used++;
					spin_unlock(&dev->req_lock);
				}
			} else {
				spin_unlock(&dev->req_lock);
			}
		} else {
			spin_unlock(&dev->req_lock);
		}
	} else {
		spin_unlock(&dev->req_lock);
		dev_kfree_skb_any(skb);
	}

	if (netif_carrier_ok(dev->net))
		netif_wake_queue(dev->net);
}

static inline int is_promisc(u16 cdc_filter)
{
	return cdc_filter & USB_CDC_PACKET_TYPE_PROMISCUOUS;
}

static void alloc_tx_buffer(struct eth_dev *dev)
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
		if (!req->buf)
			req->buf = kmalloc(dev->tx_req_bufsize,
						GFP_ATOMIC);
	}
}

static void process_tx_w(struct work_struct *w)
{
	struct eth_dev		*dev = container_of(w, struct eth_dev, tx_work);
	struct net_device	*net = NULL;
	struct sk_buff		*skb = NULL;
	struct sg_ctx		*sg_ctx;
	struct usb_request	*req;
	struct usb_ep		*in = NULL;
	int			ret, count, hlen = 0, hdr_offset;
	u32			max_size = 0;
	u32			max_num_pkts = 1;
	unsigned long		flags;
	bool			header_on = false;
	int			req_cnt = 0;
	bool			port_usb_active;

	spin_lock_irqsave(&dev->lock, flags);
	dev->tx_work_status = 1;
	if (dev->port_usb) {
		in = dev->port_usb->in_ep;
		max_size = dev->dl_max_xfer_size;
		max_num_pkts = dev->dl_max_pkts_per_xfer;
		if (!max_num_pkts)
			max_num_pkts = 1;
		hlen = dev->header_len;
		net = dev->net;
	}
	spin_unlock_irqrestore(&dev->lock, flags);

	spin_lock_irqsave(&dev->req_lock, flags);
	while (in && !list_empty(&dev->tx_reqs) &&
			(skb = skb_dequeue(&dev->tx_skb_q))) {
		req = list_first_entry(&dev->tx_reqs, struct usb_request,
				list);
		list_del(&req->list);
		spin_unlock_irqrestore(&dev->req_lock, flags);

		req->num_sgs = 0;
		req->zero = 1;
		req->length = 0;
		sg_ctx = req->context;
		skb_queue_head_init(&sg_ctx->skbs);
		sg_init_table(req->sg, DL_MAX_PKTS_PER_XFER);

		hdr_offset = 0;
		count = 1;

		do {
			/* spinlock can be avoided if buffer can passed
			 * wrap callback argument. However, it requires
			 * changes to all existing clients
			 */
			hlen = dev->header_len;
			spin_lock_irqsave(&dev->lock, flags);
			if (!dev->port_usb) {
				spin_unlock_irqrestore(&dev->lock, flags);
				skb_queue_purge(&sg_ctx->skbs);
				kfree(req->sg);
				kfree(req->context);
				kfree(req->buf);
				usb_ep_free_request(in, req);
				dev->tx_work_status = 0;
				return;
			}

			if (hlen && dev->wrap) {
				dev->port_usb->header = req->buf + hdr_offset;
				skb = dev->wrap(dev->port_usb, skb);
				header_on = true;
			}

			spin_unlock_irqrestore(&dev->lock, flags);

			if (!skb) {
				skb_queue_purge(&sg_ctx->skbs);
				kfree(req->sg);
				kfree(req->context);
				kfree(req->buf);
				usb_ep_free_request(in, req);
				dev->tx_work_status = 0;
				return;
			}

			if (header_on) {
				sg_set_buf(&req->sg[req->num_sgs],
					req->buf + hdr_offset, hlen);
				req->num_sgs++;
				hdr_offset += hlen;
				req->length += hlen;
			}

			/* skb processing */
			sg_set_buf(&req->sg[req->num_sgs], skb->data, skb->len);
			req->num_sgs++;

			req->length += skb->len;
			skb_queue_tail(&sg_ctx->skbs, skb);

			skb = skb_dequeue(&dev->tx_skb_q);
			if (!skb)
				break;
			if ((req->length + skb->len + hlen) >= max_size ||
					count >= max_num_pkts) {
				skb_queue_head(&dev->tx_skb_q, skb);
				break;
			}
			count++;
		} while (true);
		sg_mark_end(&req->sg[req->num_sgs - 1]);

		spin_lock_irqsave(&dev->lock, flags);
		if (dev->port_usb) {
			in = dev->port_usb->in_ep;
			port_usb_active = 1;
		} else {
			port_usb_active = 0;
		}
		spin_unlock_irqrestore(&dev->lock, flags);

		if (!port_usb_active) {
			__skb_queue_purge(&sg_ctx->skbs);
			kfree(req->sg);
			kfree(req->context);
			kfree(req->buf);
			usb_ep_free_request(in, req);
			dev->tx_work_status = 0;
			return;
		}

		spin_lock_irqsave(&dev->req_lock, flags);
		dev->tx_req_status++;
		spin_unlock_irqrestore(&dev->req_lock, flags);
		ret = usb_ep_queue(in, req, GFP_KERNEL);
		spin_lock_irqsave(&dev->req_lock, flags);
		switch (ret) {
		default:
			dev->net->stats.tx_dropped +=
				skb_queue_len(&sg_ctx->skbs);

			__skb_queue_purge(&sg_ctx->skbs);
			list_add_tail(&req->list, &dev->tx_reqs);
			if (dev->tx_req_status > 0)
				dev->tx_req_status--;
			break;
		case 0:
			break;
		}

		/* break the loop after processing 10 packets
		 * otherwise wd may kick in
		 */
		if (ret || ++req_cnt > 10) {
			dev->loop_brk_cnt++;
			break;
		}

		if (dev->tx_skb_q.qlen <  tx_start_threshold)
			netif_start_queue(net);

	}
	spin_unlock_irqrestore(&dev->req_lock, flags);
	dev->tx_work_status = 0;
}

static netdev_tx_t eth_start_xmit(struct sk_buff *skb,
					struct net_device *net)
{
	struct eth_dev		*dev = netdev_priv(net);
	int			length = 0;
	int			retval;
	struct usb_request	*req = NULL;
	unsigned long		flags;
	struct usb_ep		*in;
	u16			cdc_filter;

	spin_lock_irqsave(&dev->lock, flags);
	if (dev->port_usb) {
		in = dev->port_usb->in_ep;
		cdc_filter = dev->port_usb->cdc_filter;
	} else {
		in = NULL;
		cdc_filter = 0;
	}
	spin_unlock_irqrestore(&dev->lock, flags);

	if (skb && !in) {
		dev_kfree_skb_any(skb);
		return NETDEV_TX_OK;
	}
#ifdef CONFIG_USB_PAM
	if (in && (in->android_kabi_reserved1 == 0xff)) {
		if (skb)
			dev_kfree_skb_any(skb);
		return NETDEV_TX_OK;
	}
#endif
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
				return NETDEV_TX_OK;
			}
		}
		/* ignores USB_CDC_PACKET_TYPE_DIRECTED */
	}

	dev->tx_pkts_rcvd++;
	if (dev->sg_enabled) {
		skb_queue_tail(&dev->tx_skb_q, skb);
		if (dev->tx_skb_q.qlen > tx_stop_threshold) {
			dev->tx_throttle++;
			netif_stop_queue(net);
		}
		do_tx_queue_work(dev);
		return NETDEV_TX_OK;
	}

	/* Allocate memory for tx_reqs to support multi packet transfer */
	if (dev->port_usb && dev->port_usb->multi_pkt_xfer
		&& !dev->tx_req_bufsize)
		alloc_tx_buffer(dev);

	spin_lock_irqsave(&dev->req_lock, flags);
	/*
	 * this freelist can be empty if an interrupt triggered disconnect()
	 * and reconfigured the gadget (shutting down this queue) after the
	 * network stack decided to xmit but before we got the spinlock.
	 */
	if (list_empty(&dev->tx_reqs)) {
		spin_unlock_irqrestore(&dev->req_lock, flags);
		return NETDEV_TX_BUSY;
	}

	req = list_first_entry(&dev->tx_reqs, struct usb_request, list);
	list_del(&req->list);

	/* temporarily stop TX queue when the freelist empties */
	if (list_empty(&dev->tx_reqs))
		netif_stop_queue(net);
	spin_unlock_irqrestore(&dev->req_lock, flags);

	/* no buffer copies needed, unless the network stack did it
	 * or the hardware can't use skb buffers.
	 * or there's not enough space for extra headers we need
	 */
	if (dev->wrap) {
		unsigned long	flags;

		spin_lock_irqsave(&dev->lock, flags);
		if (dev->port_usb)
			skb = dev->wrap(dev->port_usb, skb);
		spin_unlock_irqrestore(&dev->lock, flags);
		if (!skb) {
			/* Multi frame CDC protocols may store the frame for
			 * later which is not a dropped frame.
			 */
			if (dev->port_usb->supports_multi_frame)
				goto multiframe;
			goto drop;
		}
	}

	spin_lock_irqsave(&dev->req_lock, flags);
	dev->tx_skb_hold_count++;
	spin_unlock_irqrestore(&dev->req_lock, flags);

	if (dev->port_usb && dev->port_usb->multi_pkt_xfer) {
		memcpy(req->buf + req->length, skb->data, skb->len);
		req->length = req->length + skb->len;
		length = req->length;
		dev_kfree_skb_any(skb);

		spin_lock_irqsave(&dev->req_lock, flags);
		if (dev->tx_skb_hold_count < dev->dl_max_pkts_per_xfer) {
			if (dev->no_tx_req_used > TX_REQ_THRESHOLD) {
				list_add(&req->list, &dev->tx_reqs);
				spin_unlock_irqrestore(&dev->req_lock, flags);
				goto success;
			}
		}

		dev->no_tx_req_used++;
		spin_unlock_irqrestore(&dev->req_lock, flags);

		spin_lock_irqsave(&dev->lock, flags);
		dev->tx_skb_hold_count = 0;
		spin_unlock_irqrestore(&dev->lock, flags);
	} else {
		if (skb) {
			length = skb->len;
			req->buf = skb->data;
			req->context = skb;
		}
	}

	req->complete = tx_complete;

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

	/* throttle highspeed IRQ rate back slightly */
	if (gadget_is_dualspeed(dev->gadget) &&
			 (dev->gadget->speed == USB_SPEED_HIGH)) {
		dev->tx_qlen++;
		if (dev->tx_qlen == (dev->qmult/2)) {
			req->no_interrupt = 0;
			dev->tx_qlen = 0;
		} else {
			req->no_interrupt = 1;
		}
	} else {
		req->no_interrupt = 0;
	}

	retval = usb_ep_queue(in, req, GFP_ATOMIC);
	switch (retval) {
	default:
		DBG(dev, "tx queue err %d\n", retval);
		break;
	case 0:
		break;
	}

	if (retval) {
		if (!dev->port_usb->multi_pkt_xfer)
			dev_kfree_skb_any(skb);
drop:
		dev->net->stats.tx_dropped++;
multiframe:
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
	DBG(dev, "%s\n", __func__);

	/* fill the rx queue */
	rx_fill(dev, gfp_flags);

	/* and open the tx floodgates */
	dev->tx_qlen = 0;
	netif_wake_queue(dev->net);
}

static int eth_open(struct net_device *net)
{
	struct eth_dev	*dev = netdev_priv(net);
	struct gether	*link;

	DBG(dev, "%s\n", __func__);
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

	VDBG(dev, "%s\n", __func__);
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

		/* it maybe crash if in/out is null since abnormal disconnect */
		if (!in || !out) {
			spin_unlock_irqrestore(&dev->lock, flags);
			return 0;
		}

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
			dev_addr[i] = num;
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

static const struct net_device_ops eth_netdev_ops = {
	.ndo_open		= eth_open,
	.ndo_stop		= eth_stop,
	.ndo_start_xmit		= eth_start_xmit,
	.ndo_change_mtu		= ueth_change_mtu,
	.ndo_set_mac_address	= eth_mac_addr,
	.ndo_validate_addr	= eth_validate_addr,
};

static struct device_type gadget_type = {
	.name	= "gadget",
};

/**
 * gether_setup_name - initialize one ethernet-over-usb link
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
struct eth_dev *sprd_gether_setup_name(struct usb_gadget *g,
		const char *dev_addr, const char *host_addr,
		u8 ethaddr[ETH_ALEN], unsigned qmult, const char *netname)
{
	struct eth_dev		*dev;
	struct net_device	*net;
	int			status;

	net = alloc_etherdev(sizeof *dev);
	if (!net)
		return ERR_PTR(-ENOMEM);

	dev = netdev_priv(net);
	spin_lock_init(&dev->lock);
	spin_lock_init(&dev->req_lock);
	INIT_WORK(&dev->work, eth_work);
	INIT_LIST_HEAD(&dev->tx_reqs);
	INIT_LIST_HEAD(&dev->rx_reqs);

	skb_queue_head_init(&dev->rx_frames);
	skb_queue_head_init(&dev->tx_skb_q);

	/* network device setup */
	dev->net = net;
	dev->qmult = qmult;
	snprintf(net->name, sizeof(net->name), "%s%%d", netname);

	INFO(dev, "%s netdev name: %s \n", __func__, netname);

	if (get_ether_addr(dev_addr, net->dev_addr))
		dev_warn(&g->dev,
			"using random %s ethernet address\n", "self");
	if (get_ether_addr(host_addr, dev->host_mac))
		dev_warn(&g->dev,
			"using random %s ethernet address\n", "host");

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
		uether_debugfs_init(dev, netname);
	}

	return dev;
}
EXPORT_SYMBOL_GPL(sprd_gether_setup_name);

struct net_device *sprd_gether_setup_name_default(const char *netname)
{
	struct net_device	*net;
	struct eth_dev		*dev;

	net = alloc_etherdev(sizeof(*dev));
	if (!net)
		return ERR_PTR(-ENOMEM);

	dev = netdev_priv(net);
	spin_lock_init(&dev->lock);
	spin_lock_init(&dev->req_lock);
	INIT_WORK(&dev->work, eth_work);
	INIT_WORK(&dev->tx_work, process_tx_w);
	INIT_LIST_HEAD(&dev->tx_reqs);
	INIT_LIST_HEAD(&dev->rx_reqs);

	skb_queue_head_init(&dev->rx_frames);
	skb_queue_head_init(&dev->tx_skb_q);

	/* network device setup */
	dev->net = net;
	dev->qmult = QMULT_DEFAULT;
	snprintf(net->name, sizeof(net->name), "%s%%d", netname);

	eth_random_addr(dev->dev_mac);
	pr_warn("using random %s ethernet address\n", "self");
	eth_random_addr(dev->host_mac);
	pr_warn("using random %s ethernet address\n", "host");

	net->netdev_ops = &eth_netdev_ops;

	net->ethtool_ops = &ops;
	SET_NETDEV_DEVTYPE(net, &gadget_type);
	uether_debugfs_init(dev, netname);

	return net;
}
EXPORT_SYMBOL_GPL(sprd_gether_setup_name_default);

int sprd_gether_register_netdev(struct net_device *net)
{
	struct eth_dev *dev;
	struct usb_gadget *g;
	struct sockaddr sa;
	int status;

	if (!net->dev.parent)
		return -EINVAL;
	dev = netdev_priv(net);
	g = dev->gadget;

	dev->rx_thread = kthread_create(process_rx_w, dev, "uether_rx");
	if (IS_ERR(dev->rx_thread)) {
		ERROR(dev, "failed to create uether_rx (0x%lx)\n", PTR_ERR(dev->rx_thread));
		return -EINVAL;
	}
	wake_up_process(dev->rx_thread);

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
EXPORT_SYMBOL_GPL(sprd_gether_register_netdev);

void sprd_gether_set_gadget(struct net_device *net, struct usb_gadget *g)
{
	struct eth_dev *dev;

	dev = netdev_priv(net);
	dev->gadget = g;
	SET_NETDEV_DEV(net, &g->dev);
}
EXPORT_SYMBOL_GPL(sprd_gether_set_gadget);

int sprd_gether_set_dev_addr(struct net_device *net, const char *dev_addr)
{
	struct eth_dev *dev;
	u8 new_addr[ETH_ALEN];

	dev = netdev_priv(net);
	if (get_ether_addr(dev_addr, new_addr))
		return -EINVAL;
	memcpy(dev->dev_mac, new_addr, ETH_ALEN);
	return 0;
}
EXPORT_SYMBOL_GPL(sprd_gether_set_dev_addr);

int sprd_gether_get_dev_addr(struct net_device *net, char *dev_addr, int len)
{
	struct eth_dev *dev;
	int ret;

	dev = netdev_priv(net);
	ret = get_ether_addr_str(dev->dev_mac, dev_addr, len);
	if (ret + 1 < len) {
		dev_addr[ret++] = '\n';
		dev_addr[ret] = '\0';
	}

	return ret;
}
EXPORT_SYMBOL_GPL(sprd_gether_get_dev_addr);

int sprd_gether_set_host_addr(struct net_device *net, const char *host_addr)
{
	struct eth_dev *dev;
	u8 new_addr[ETH_ALEN];

	dev = netdev_priv(net);
	if (get_ether_addr(host_addr, new_addr))
		return -EINVAL;
	memcpy(dev->host_mac, new_addr, ETH_ALEN);
	return 0;
}
EXPORT_SYMBOL_GPL(sprd_gether_set_host_addr);

int sprd_gether_get_host_addr(struct net_device *net, char *host_addr, int len)
{
	struct eth_dev *dev;
	int ret;

	dev = netdev_priv(net);
	ret = get_ether_addr_str(dev->host_mac, host_addr, len);
	if (ret + 1 < len) {
		host_addr[ret++] = '\n';
		host_addr[ret] = '\0';
	}

	return ret;
}
EXPORT_SYMBOL_GPL(sprd_gether_get_host_addr);

int sprd_gether_get_host_addr_cdc(struct net_device *net, char *host_addr, int len)
{
	struct eth_dev *dev;

	if (len < 13)
		return -EINVAL;

	dev = netdev_priv(net);
	snprintf(host_addr, len, "%pm", dev->host_mac);

	return strlen(host_addr);
}
EXPORT_SYMBOL_GPL(sprd_gether_get_host_addr_cdc);

void sprd_gether_get_host_addr_u8(struct net_device *net, u8 host_mac[ETH_ALEN])
{
	struct eth_dev *dev;

	dev = netdev_priv(net);
	memcpy(host_mac, dev->host_mac, ETH_ALEN);
}
EXPORT_SYMBOL_GPL(sprd_gether_get_host_addr_u8);

void sprd_gether_set_qmult(struct net_device *net, unsigned qmult)
{
	struct eth_dev *dev;

	dev = netdev_priv(net);
	dev->qmult = qmult;
}
EXPORT_SYMBOL_GPL(sprd_gether_set_qmult);

unsigned sprd_gether_get_qmult(struct net_device *net)
{
	struct eth_dev *dev;

	dev = netdev_priv(net);
	return dev->qmult;
}
EXPORT_SYMBOL_GPL(sprd_gether_get_qmult);

int sprd_gether_get_ifname(struct net_device *net, char *name, int len)
{
	int ret;

	rtnl_lock();
	ret = scnprintf(name, len, "%s\n", netdev_name(net));
	rtnl_unlock();
	return ret;
}
EXPORT_SYMBOL_GPL(sprd_gether_get_ifname);

/**
 * gether_cleanup - remove Ethernet-over-USB device
 * Context: may sleep
 *
 * This is called to free all resources allocated by @gether_setup().
 */
void sprd_gether_cleanup(struct eth_dev *dev)
{
	if (!dev)
		return;

	kthread_stop(dev->rx_thread);
	uether_debugfs_exit(dev);
	unregister_netdev(dev->net);
	flush_work(&dev->work);
	free_netdev(dev->net);
}
EXPORT_SYMBOL_GPL(sprd_gether_cleanup);

void gether_update_dl_max_xfer_size(struct gether *link, u32 s)
{
	struct eth_dev		*dev = link->ioport;

	dev->dl_max_xfer_size = s;
}
EXPORT_SYMBOL_GPL(gether_update_dl_max_xfer_size);

void gether_enable_sg(struct gether *link, bool enable)
{
	struct eth_dev		*dev = link->ioport;

	/* fixme: enable rndis sg mode. best way to be set by sg_supported */
	dev->sg_enabled = true;
}
EXPORT_SYMBOL_GPL(gether_enable_sg);

bool gether_is_sg_enabled(struct gether *link)
{
	struct eth_dev		*dev = link->ioport;

	return dev->sg_enabled;
}
EXPORT_SYMBOL_GPL(gether_is_sg_enabled);

void gether_update_dl_max_pkts_per_xfer(struct gether *link, u32 n)
{
	struct eth_dev		*dev = link->ioport;

	if (n > DL_MAX_PKTS_PER_XFER)
		n = DL_MAX_PKTS_PER_XFER;

	dev->dl_max_pkts_per_xfer = n;
}
EXPORT_SYMBOL_GPL(gether_update_dl_max_pkts_per_xfer);

/**
 * gether_connect - notify network layer that USB link is active
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
struct net_device *sprd_gether_connect(struct gether *link)
{
	struct eth_dev		*dev = link->ioport;
	int			result = 0;

	if (!dev)
		return ERR_PTR(-EINVAL);

	/* if scatter/gather or sg is supported then headers can be part of
	 * req->buf which is allocated later
	 */
	if (!dev->sg_enabled) {
		/* size of rndis_packet_msg_type */
		link->header = kzalloc(sizeof(struct rndis_packet_msg_type),
				       GFP_ATOMIC);
		if (!link->header) {
			pr_err("RNDIS header memory allocation failed.\n");
			result = -ENOMEM;
			return ERR_PTR(result);
		}
	}
#ifdef CONFIG_USB_PAM
	link->in_ep->android_kabi_reserved1 = 0xff;
	link->out_ep->android_kabi_reserved1 = 0xff;
#endif

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

	dev->header_len = link->header_len;
	dev->unwrap = link->unwrap;
	dev->wrap = link->wrap;
	dev->ul_max_pkts_per_xfer = link->ul_max_pkts_per_xfer;
	dev->dl_max_pkts_per_xfer = link->dl_max_pkts_per_xfer;
	dev->dl_max_xfer_size = link->dl_max_xfer_size;

	if (result == 0)
		result = alloc_requests(dev, link, qlen(dev->gadget,
					dev->qmult));

	if (result == 0) {
		dev->zlp = link->is_zlp_ok;
		DBG(dev, "qlen %d\n", qlen(dev->gadget, dev->qmult));

		spin_lock(&dev->lock);
		dev->tx_skb_hold_count = 0;
		dev->tx_work_status = 0;
		dev->tx_req_status = 0;
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
fail0:
	/* caller is responsible for cleanup on error */
	if (result < 0) {
		if (!dev->sg_enabled)
			kfree(link->header);
		return ERR_PTR(result);
	}
	return dev->net;
}
EXPORT_SYMBOL_GPL(sprd_gether_connect);

/**
 * gether_disconnect - notify network layer that USB link is inactive
 * @link: the USB link, on which gether_connect() was called
 * Context: irqs blocked
 *
 * This is called to deactivate endpoints and let the network layer know
 * the connection went inactive ("no carrier").
 *
 * On return, the state is as if gether_connect() had never been called.
 * The endpoints are inactive, and accordingly without active USB I/O.
 * Pointers to endpoint descriptors and endpoint private data are nulled.
 */
void sprd_gether_disconnect(struct gether *link)
{
	struct eth_dev		*dev = link->ioport;
	struct usb_request	*req;
	struct sk_buff		*skb;

	WARN_ON(!dev);
	if (!dev)
		return;

	DBG(dev, "%s\n", __func__);

	netif_stop_queue(dev->net);
	netif_carrier_off(dev->net);

	/* disable endpoints, forcing (synchronous) completion
	 * of all pending i/o.  then free the request objects
	 * and forget about the endpoints.
	 */
	usb_ep_disable(link->in_ep);
	spin_lock(&dev->req_lock);
	while (!list_empty(&dev->tx_reqs)) {
		req = list_first_entry(&dev->tx_reqs, struct usb_request, list);
		list_del(&req->list);

		spin_unlock(&dev->req_lock);
		if (link->multi_pkt_xfer ||
				dev->sg_enabled)
			kfree(req->buf);
		if (dev->sg_enabled) {
			kfree(req->context);
			kfree(req->sg);
		}

		usb_ep_free_request(link->in_ep, req);
		spin_lock(&dev->req_lock);
	}
	/* Free rndis header buffer memory */
	if (!dev->sg_enabled)
		kfree(link->header);
	link->header = NULL;
	spin_unlock(&dev->req_lock);

	skb_queue_purge(&dev->tx_skb_q);

	link->in_ep->desc = NULL;

	usb_ep_disable(link->out_ep);
	spin_lock(&dev->req_lock);
	while (!list_empty(&dev->rx_reqs)) {
		req = list_first_entry(&dev->rx_reqs, struct usb_request, list);
		list_del(&req->list);

		spin_unlock(&dev->req_lock);
		usb_ep_free_request(link->out_ep, req);
		spin_lock(&dev->req_lock);
	}
	spin_unlock(&dev->req_lock);

	spin_lock(&dev->rx_frames.lock);
	while ((skb = __skb_dequeue(&dev->rx_frames)))
		dev_kfree_skb_any(skb);
	spin_unlock(&dev->rx_frames.lock);

	link->out_ep->desc = NULL;

#ifdef CONFIG_USB_PAM
	link->in_ep->android_kabi_reserved1 = 0;
	link->out_ep->android_kabi_reserved1 = 0;
#endif

	/* finish forgetting about this USB link episode */
	dev->header_len = 0;
	dev->unwrap = NULL;
	dev->wrap = NULL;

	spin_lock(&dev->lock);
	dev->port_usb = NULL;
	spin_unlock(&dev->lock);
}
EXPORT_SYMBOL_GPL(sprd_gether_disconnect);

static int uether_stat_show(struct seq_file *s, void *unused)
{
	struct eth_dev *dev = s->private;
	int i;

	if (dev) {
		seq_printf(s, "tx_qlen=%u tx_throttle = %lu\n aggr count:",
			   dev->tx_skb_q.qlen,
			   dev->tx_throttle);
		for (i = 0; i < DL_MAX_PKTS_PER_XFER; i++)
			seq_printf(s, "%u\t", dev->tx_aggr_cnt[i]);

		seq_printf(s, "\nloop_brk_cnt = %u\n tx_pkts_rcvd=%u\n",
			   dev->loop_brk_cnt,
			   dev->tx_pkts_rcvd);
	}

	return 0;
}

static int uether_open(struct inode *inode, struct file *file)
{
	return single_open(file, uether_stat_show, inode->i_private);
}

static ssize_t uether_stat_reset(struct file *file,
		const char __user *ubuf, size_t count, loff_t *ppos)
{
	struct seq_file *s = file->private_data;
	struct eth_dev *dev = s->private;
	unsigned long flags;

	spin_lock_irqsave(&dev->lock, flags);
	/* Reset tx_throttle */
	dev->tx_throttle = 0;
	/*dev->rx_throttle = 0;*/
	spin_unlock_irqrestore(&dev->lock, flags);
	return count;
}

const struct file_operations uether_stats_ops = {
	.open = uether_open,
	.read = seq_read,
	.write = uether_stat_reset,
};

static void uether_debugfs_init(struct eth_dev *dev, const char *name)
{
	struct dentry *uether_dent;
	struct dentry *uether_dfile;

	uether_dent = debugfs_create_dir(name, 0);
	if (IS_ERR(uether_dent))
		return;
	dev->uether_dent = uether_dent;

	uether_dfile = debugfs_create_file("status", S_IRUGO | S_IWUSR,
				uether_dent, dev, &uether_stats_ops);
	if (!uether_dfile || IS_ERR(uether_dfile))
		debugfs_remove(uether_dent);
	dev->uether_dfile = uether_dfile;
}

static void uether_debugfs_exit(struct eth_dev *dev)
{
	debugfs_remove(dev->uether_dfile);
	debugfs_remove(dev->uether_dent);
	dev->uether_dent = NULL;
	dev->uether_dfile = NULL;
}

static int __init gether_init(void)
{
	uether_tx_wq = alloc_workqueue("uether_tx",
				WQ_CPU_INTENSIVE | WQ_UNBOUND, 1);
	if (!uether_tx_wq) {
		pr_err("%s: Unable to create workqueue: uether\n", __func__);
		return -ENOMEM;
	}

	return 0;
}
module_init(gether_init);

static void __exit gether_exit(void)
{
	destroy_workqueue(uether_tx_wq);
}
module_exit(gether_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("David Brownell");
