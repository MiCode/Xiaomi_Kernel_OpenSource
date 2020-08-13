/* Copyright (c) 2011-2014, 2019-2020, Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/kernel.h>
#include <linux/interrupt.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/termios.h>
#include <linux/netdevice.h>
#include <linux/debugfs.h>
#include <linux/bitops.h>
#include <linux/termios.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/usb/usb_bridge.h>
#include <linux/usb/composite.h>

#define MDM_DATA_RX_Q_SIZE			10
#define MDM_DATA_RX_REQ_SIZE			2048
#define MDM_DATA_TX_INTR_THRESHOLD		10
#define MDM_DATA_TX_Q_SIZE			50

static unsigned int mdm_data_rx_q_size = MDM_DATA_RX_Q_SIZE;
module_param(mdm_data_rx_q_size, uint, 0644);

static unsigned int mdm_data_tx_q_size = MDM_DATA_TX_Q_SIZE;
module_param(mdm_data_tx_q_size, uint, 0644);

static unsigned int mdm_data_rx_req_size = MDM_DATA_RX_REQ_SIZE;
module_param(mdm_data_rx_req_size, uint, 0644);

static unsigned int mdm_data_tx_intr_thld = MDM_DATA_TX_INTR_THRESHOLD;
module_param(mdm_data_tx_intr_thld, uint, 0644);

/*flow ctrl*/
#define MDM_DATA_FLOW_CTRL_EN_THRESHOLD		100
#define MDM_DATA_FLOW_CTRL_DISABLE		60
#define MDM_DATA_FLOW_CTRL_SUPPORT		1
#define MDM_DATA_PENDLIMIT_WITH_BRIDGE		100

static unsigned int mdm_data_fctrl_support = MDM_DATA_FLOW_CTRL_SUPPORT;
module_param(mdm_data_fctrl_support, uint, 0644);

static unsigned int mdm_data_fctrl_en_thld = MDM_DATA_FLOW_CTRL_EN_THRESHOLD;
module_param(mdm_data_fctrl_en_thld, uint, 0644);

static unsigned int mdm_data_fctrl_dis_thld = MDM_DATA_FLOW_CTRL_DISABLE;
module_param(mdm_data_fctrl_dis_thld, uint, 0644);

static unsigned int mdm_data_pend_limit_with_bridge =
		MDM_DATA_PENDLIMIT_WITH_BRIDGE;
module_param(mdm_data_pend_limit_with_bridge, uint, 0644);

#define CH_OPENED 0
#define CH_READY 1

static struct usb_interface_descriptor intf_in_only_desc = {
	.bLength            =	sizeof(intf_in_only_desc),
	.bDescriptorType    =	USB_DT_INTERFACE,
	.bNumEndpoints      =	1,
	.bInterfaceClass    =	USB_CLASS_VENDOR_SPEC,
	.bInterfaceSubClass =	USB_SUBCLASS_VENDOR_SPEC,
	/* .bInterfaceProtocol = DYNAMIC */
};

static struct usb_interface_descriptor intf_desc = {
	.bLength            =	sizeof(intf_desc),
	.bDescriptorType    =	USB_DT_INTERFACE,
	.bNumEndpoints      =	2,
	.bInterfaceClass    =	USB_CLASS_VENDOR_SPEC,
	.bInterfaceSubClass =	USB_SUBCLASS_VENDOR_SPEC,
	/* .bInterfaceProtocol = DYNAMIC */
};

static struct usb_endpoint_descriptor hs_bulk_in_desc = {
	.bLength          =	USB_DT_ENDPOINT_SIZE,
	.bDescriptorType  =	USB_DT_ENDPOINT,
	.bEndpointAddress =	USB_DIR_IN,
	.bmAttributes     =	USB_ENDPOINT_XFER_BULK,
	.wMaxPacketSize   =	cpu_to_le16(512),
	.bInterval        =	0,
};
static struct usb_endpoint_descriptor fs_bulk_in_desc = {
	.bLength          =	USB_DT_ENDPOINT_SIZE,
	.bDescriptorType  =	USB_DT_ENDPOINT,
	.bEndpointAddress =	USB_DIR_IN,
	.bmAttributes     =	USB_ENDPOINT_XFER_BULK,
	.wMaxPacketSize   =	cpu_to_le16(64),
	.bInterval        =	0,
};

static struct usb_endpoint_descriptor hs_bulk_out_desc = {
	.bLength          =	USB_DT_ENDPOINT_SIZE,
	.bDescriptorType  =	USB_DT_ENDPOINT,
	.bEndpointAddress =	USB_DIR_OUT,
	.bmAttributes     =	USB_ENDPOINT_XFER_BULK,
	.wMaxPacketSize   =	cpu_to_le16(512),
	.bInterval        =	0,
};

static struct usb_endpoint_descriptor fs_bulk_out_desc = {
	.bLength          =	USB_DT_ENDPOINT_SIZE,
	.bDescriptorType  =	USB_DT_ENDPOINT,
	.bEndpointAddress =	USB_DIR_OUT,
	.bmAttributes     =	USB_ENDPOINT_XFER_BULK,
	.wMaxPacketSize   =	cpu_to_le16(64),
	.bInterval        =	0,
};

static struct usb_endpoint_descriptor ss_bulk_in_desc = {
	.bLength          =	USB_DT_ENDPOINT_SIZE,
	.bDescriptorType  =	USB_DT_ENDPOINT,
	.bEndpointAddress =	USB_DIR_IN,
	.bmAttributes     =	USB_ENDPOINT_XFER_BULK,
	.wMaxPacketSize   =	cpu_to_le16(1024),
};

static struct usb_ss_ep_comp_descriptor ss_bulk_in_comp_desc = {
	.bLength =		sizeof(ss_bulk_in_comp_desc),
	.bDescriptorType =	USB_DT_SS_ENDPOINT_COMP,

	/* the following 2 values can be tweaked if necessary */
	/* .bMaxBurst =		0, */
	/* .bmAttributes =	0, */
};

static struct usb_endpoint_descriptor ss_bulk_out_desc = {
	.bLength          =	USB_DT_ENDPOINT_SIZE,
	.bDescriptorType  =	USB_DT_ENDPOINT,
	.bEndpointAddress =	USB_DIR_OUT,
	.bmAttributes     =	USB_ENDPOINT_XFER_BULK,
	.wMaxPacketSize   =	cpu_to_le16(1024),
};

static struct usb_ss_ep_comp_descriptor ss_bulk_out_comp_desc = {
	.bLength =		sizeof(ss_bulk_out_comp_desc),
	.bDescriptorType =	USB_DT_SS_ENDPOINT_COMP,

	/* the following 2 values can be tweaked if necessary */
	/* .bMaxBurst =		0, */
	/* .bmAttributes =	0, */
};

static struct usb_descriptor_header *fs_in_only_desc[] = {
	(struct usb_descriptor_header *) &intf_in_only_desc,
	(struct usb_descriptor_header *) &fs_bulk_in_desc,
	NULL,
	};

static struct usb_descriptor_header *hs_in_only_desc[] = {
	(struct usb_descriptor_header *) &intf_in_only_desc,
	(struct usb_descriptor_header *) &hs_bulk_in_desc,
	NULL,
};

static struct usb_descriptor_header *ss_in_only_desc[] = {
	(struct usb_descriptor_header *) &intf_in_only_desc,
	(struct usb_descriptor_header *) &ss_bulk_in_desc,
	(struct usb_descriptor_header *) &ss_bulk_in_comp_desc,
	NULL,
};

static struct usb_descriptor_header *fs_desc[] = {
	(struct usb_descriptor_header *) &intf_desc,
	(struct usb_descriptor_header *) &fs_bulk_in_desc,
	(struct usb_descriptor_header *) &fs_bulk_out_desc,
	NULL,
	};
static struct usb_descriptor_header *hs_desc[] = {
	(struct usb_descriptor_header *) &intf_desc,
	(struct usb_descriptor_header *) &hs_bulk_in_desc,
	(struct usb_descriptor_header *) &hs_bulk_out_desc,
	NULL,
};

static struct usb_descriptor_header *ss_desc[] = {
	(struct usb_descriptor_header *) &intf_desc,
	(struct usb_descriptor_header *) &ss_bulk_in_desc,
	(struct usb_descriptor_header *) &ss_bulk_in_comp_desc,
	(struct usb_descriptor_header *) &ss_bulk_out_desc,
	(struct usb_descriptor_header *) &ss_bulk_out_comp_desc,
	NULL,
};

static struct usb_string mdm_data_string_defs[MAX_BRIDGE_DEVICES];

static struct usb_gadget_strings mdm_data_string_table = {
	.language =		0x0409, /* en-us */
	.strings =		mdm_data_string_defs,
};

static struct usb_gadget_strings *mdm_data_strings[] = {
	&mdm_data_string_table,
	NULL,
};

struct mdm_data_port {
	struct usb_function	function;
	struct usb_composite_dev *cdev;

	/* gadget */
	atomic_t		connected;
	struct usb_ep		*in;
	struct usb_ep		*out;
	bool			in_ep;
	bool			out_ep;

	/* data transfer queues */
	unsigned int		tx_q_size;
	struct list_head	tx_idle;
	struct sk_buff_head	tx_skb_q;
	spinlock_t		tx_lock;

	unsigned int		rx_q_size;
	struct list_head	rx_idle;
	struct sk_buff_head	rx_skb_q;
	spinlock_t		rx_lock;

	/* work */
	struct workqueue_struct	*wq;
	struct work_struct	connect_w;
	struct work_struct	disconnect_w;
	struct work_struct	write_tomdm_w;
	struct work_struct	write_tohost_w;
	struct platform_driver	pdrv;
	char			port_name[MAX_INST_NAME_LEN];

	struct bridge		brdg;

	/*bridge status*/
	unsigned long		bridge_sts;

	unsigned int		n_tx_req_queued;

	/*counters*/
	unsigned long		to_modem;
	unsigned long		to_host;
	unsigned int		rx_throttled_cnt;
	unsigned int		rx_unthrottled_cnt;
	unsigned int		tx_throttled_cnt;
	unsigned int		tx_unthrottled_cnt;
	unsigned int		tomodem_drp_cnt;
	unsigned int		unthrottled_pnd_skbs;
};

static struct mdm_data_port *mdm_data_ports[MAX_BRIDGE_DEVICES];

static inline struct mdm_data_port *func_to_port(struct usb_function *f)
{
	return container_of(f, struct mdm_data_port, function);
}

struct mdm_data_opts {
	struct usb_function_instance func_inst;
	struct mdm_data_port *ctxt;
	bool in_ep;
	bool out_ep;

	/*
	 * Protect the data form concurrent access by read/write
	 * and create symlink/remove symlink.
	 */
	 struct mutex lock;
	 int refcnt;
};

static unsigned int get_timestamp(void);
static void dbg_timestamp(char *, struct sk_buff *);
static void mdm_data_start_rx(struct mdm_data_port *port);

static void mdm_data_free_requests(struct usb_ep *ep, struct list_head *head)
{
	struct usb_request	*req;

	while (!list_empty(head)) {
		req = list_entry(head->next, struct usb_request, list);
		list_del(&req->list);
		usb_ep_free_request(ep, req);
	}
}

static int mdm_data_alloc_requests(struct usb_ep *ep, struct list_head *head,
		int num,
		void (*cb)(struct usb_ep *ep, struct usb_request *),
		spinlock_t *lock)
{
	int			i;
	struct usb_request	*req;
	unsigned long		flags;

	pr_debug("%s: ep:%s head:%pK num:%d cb:%pK", __func__,
			ep->name, head, num, cb);

	for (i = 0; i < num; i++) {
		req = usb_ep_alloc_request(ep, GFP_KERNEL);
		if (!req) {
			pr_debug("%s: req allocated:%d\n", __func__, i);
			return list_empty(head) ? -ENOMEM : 0;
		}
		req->complete = cb;
		spin_lock_irqsave(lock, flags);
		list_add(&req->list, head);
		spin_unlock_irqrestore(lock, flags);
	}

	return 0;
}

static void mdm_data_unthrottle_tx(void *ctx)
{
	struct mdm_data_port	*port = ctx;
	unsigned long		flags;

	if (!port || !atomic_read(&port->connected))
		return;

	spin_lock_irqsave(&port->rx_lock, flags);
	port->tx_unthrottled_cnt++;
	spin_unlock_irqrestore(&port->rx_lock, flags);

	queue_work(port->wq, &port->write_tomdm_w);
	pr_debug("%s: port name =%s unthrottled\n", __func__,
		port->port_name);
}

static void mdm_data_write_tohost(struct work_struct *w)
{
	unsigned long		flags;
	struct sk_buff		*skb;
	int			ret;
	struct usb_request	*req;
	struct usb_ep		*ep;
	struct mdm_data_port	*port;
	struct timestamp_info	*info;

	port = container_of(w, struct mdm_data_port, write_tohost_w);

	pr_debug("%s\n", __func__);
	if (!port)
		return;

	spin_lock_irqsave(&port->tx_lock, flags);
	ep = port->in;
	if (!ep) {
		spin_unlock_irqrestore(&port->tx_lock, flags);
		return;
	}

	while (!list_empty(&port->tx_idle)) {
		skb = __skb_dequeue(&port->tx_skb_q);
		if (!skb)
			break;
		pr_debug("%s: port:%pK toh:%lu pname:%s\n", __func__,
				port, port->to_host, port->port_name);
		req = list_first_entry(&port->tx_idle, struct usb_request,
				list);
		req->context = skb;
		req->buf = skb->data;
		req->length = skb->len;

		port->n_tx_req_queued++;
		if (port->n_tx_req_queued == mdm_data_tx_intr_thld) {
			req->no_interrupt = 0;
			port->n_tx_req_queued = 0;
		} else {
			req->no_interrupt = 1;
		}

		list_del(&req->list);

		info = (struct timestamp_info *)skb->cb;
		info->tx_queued = get_timestamp();
		spin_unlock_irqrestore(&port->tx_lock, flags);
		ret = usb_ep_queue(ep, req, GFP_KERNEL);
		spin_lock_irqsave(&port->tx_lock, flags);
		if (ret) {
			pr_err("%s: usb epIn failed\n", __func__);
			list_add(&req->list, &port->tx_idle);
			dev_kfree_skb_any(skb);
			break;
		}
		port->to_host++;
		if (mdm_data_fctrl_support &&
			port->tx_skb_q.qlen <= mdm_data_fctrl_dis_thld &&
			test_and_clear_bit(RX_THROTTLED, &port->brdg.flags)) {
			port->rx_unthrottled_cnt++;
			port->unthrottled_pnd_skbs = port->tx_skb_q.qlen;
			pr_debug_ratelimited("%s: disable flow ctrl: tx skbq len: %u\n",
					__func__, port->tx_skb_q.qlen);
			data_bridge_unthrottle_rx(port->brdg.ch_id);
		}
	}
	spin_unlock_irqrestore(&port->tx_lock, flags);
}

static int mdm_data_receive(void *p, void *data, size_t len)
{
	struct mdm_data_port	*port = p;
	unsigned long		flags;
	struct sk_buff		*skb = data;

	if (!port || !atomic_read(&port->connected)) {
		dev_kfree_skb_any(skb);
		return -ENOTCONN;
	}

	pr_debug("%s: p:%pK %s skb_len:%d\n", __func__,
			port, port->port_name, skb->len);

	spin_lock_irqsave(&port->tx_lock, flags);
	__skb_queue_tail(&port->tx_skb_q, skb);

	if (mdm_data_fctrl_support &&
			port->tx_skb_q.qlen >= mdm_data_fctrl_en_thld) {
		set_bit(RX_THROTTLED, &port->brdg.flags);
		port->rx_throttled_cnt++;
		pr_debug_ratelimited("%s: flow ctrl enabled: tx skbq len: %u\n",
					__func__, port->tx_skb_q.qlen);
		spin_unlock_irqrestore(&port->tx_lock, flags);
		queue_work(port->wq, &port->write_tohost_w);
		return -EBUSY;
	}

	spin_unlock_irqrestore(&port->tx_lock, flags);

	queue_work(port->wq, &port->write_tohost_w);

	return 0;
}

static void mdm_data_write_tomdm(struct work_struct *w)
{
	struct mdm_data_port	*port;
	struct sk_buff		*skb;
	struct timestamp_info	*info;
	unsigned long		flags;
	int			ret;

	port = container_of(w, struct mdm_data_port, write_tomdm_w);

	if (!port || !atomic_read(&port->connected))
		return;

	spin_lock_irqsave(&port->rx_lock, flags);
	if (test_bit(TX_THROTTLED, &port->brdg.flags)) {
		spin_unlock_irqrestore(&port->rx_lock, flags);
		goto start_rx;
	}

	while ((skb = __skb_dequeue(&port->rx_skb_q))) {
		pr_debug("%s: port:%pK tom:%lu pname:%s\n", __func__,
				port, port->to_modem, port->port_name);

		info = (struct timestamp_info *)skb->cb;
		info->rx_done_sent = get_timestamp();
		spin_unlock_irqrestore(&port->rx_lock, flags);
		ret = data_bridge_write(port->brdg.ch_id, skb);
		spin_lock_irqsave(&port->rx_lock, flags);
		if (ret < 0) {
			if (ret == -EBUSY) {
				/*flow control*/
				port->tx_throttled_cnt++;
				break;
			}
			pr_err_ratelimited("%s: write error:%d\n",
					__func__, ret);
			port->tomodem_drp_cnt++;
			dev_kfree_skb_any(skb);
			break;
		}
		port->to_modem++;
	}
	spin_unlock_irqrestore(&port->rx_lock, flags);
start_rx:
	mdm_data_start_rx(port);
}

static void mdm_data_epin_complete(struct usb_ep *ep, struct usb_request *req)
{
	struct mdm_data_port	*port = ep->driver_data;
	struct sk_buff		*skb = req->context;
	int			status = req->status;

	pr_debug("%s: skb_len: %d, status: %d", __func__, skb->len, status);
	switch (status) {
	case 0:
		/* successful completion */
		dbg_timestamp("DL", skb);
		break;
	case -ECONNRESET:
	case -ESHUTDOWN:
		/* connection gone */
		dev_kfree_skb_any(skb);
		req->buf = NULL;
		usb_ep_free_request(ep, req);
		return;
	default:
		pr_err("%s: data tx ep error %d\n", __func__, status);
		break;
	}

	dev_kfree_skb_any(skb);

	spin_lock(&port->tx_lock);
	list_add_tail(&req->list, &port->tx_idle);
	spin_unlock(&port->tx_lock);

	queue_work(port->wq, &port->write_tohost_w);
}

static void
mdm_data_epout_complete(struct usb_ep *ep, struct usb_request *req)
{
	struct mdm_data_port	*port = ep->driver_data;
	struct sk_buff		*skb = req->context;
	struct timestamp_info	*info = (struct timestamp_info *)skb->cb;
	int			status = req->status;
	bool			queue = false;

	pr_debug("%s: skb_len: %d, status: %d", __func__, skb->len, status);
	switch (status) {
	case 0:
		skb_put(skb, req->actual);
		queue = true;
		break;
	case -ECONNRESET:
	case -ESHUTDOWN:
		/* cable disconnection */
		dev_kfree_skb_any(skb);
		req->buf = NULL;
		usb_ep_free_request(ep, req);
		return;
	default:
		pr_err_ratelimited("%s: %s response error %d, %d/%d\n",
					__func__, ep->name, status,
				req->actual, req->length);
		dev_kfree_skb_any(skb);
		break;
	}

	spin_lock(&port->rx_lock);
	if (queue) {
		info->rx_done = get_timestamp();
		__skb_queue_tail(&port->rx_skb_q, skb);
		list_add_tail(&req->list, &port->rx_idle);
		queue_work(port->wq, &port->write_tomdm_w);
	}
	spin_unlock(&port->rx_lock);
}

static void mdm_data_start_rx(struct mdm_data_port *port)
{
	struct usb_request	*req;
	struct usb_ep		*ep;
	unsigned long		flags;
	int			ret;
	struct sk_buff		*skb;
	struct timestamp_info	*info;
	unsigned int		created;

	pr_debug("%s: port:%pK\n", __func__, port);
	if (!port)
		return;

	spin_lock_irqsave(&port->rx_lock, flags);
	ep = port->out;
	if (!ep) {
		spin_unlock_irqrestore(&port->rx_lock, flags);
		return;
	}

	if (!test_bit(CH_READY, &port->bridge_sts)) {
		while (!list_empty(&port->rx_idle)) {
			req = list_first_entry(&port->rx_idle,
						struct usb_request, list);
			list_del(&req->list);
			usb_ep_free_request(ep, req);
		}
	}

	while (atomic_read(&port->connected) && !list_empty(&port->rx_idle)) {
		if (port->rx_skb_q.qlen > mdm_data_pend_limit_with_bridge)
			break;

		req = list_first_entry(&port->rx_idle,
					struct usb_request, list);
		list_del(&req->list);
		spin_unlock_irqrestore(&port->rx_lock, flags);

		created = get_timestamp();
		skb = alloc_skb(mdm_data_rx_req_size, GFP_KERNEL);
		if (!skb) {
			spin_lock_irqsave(&port->rx_lock, flags);
			list_add(&req->list, &port->rx_idle);
			break;
		}
		info = (struct timestamp_info *)skb->cb;
		info->created = created;
		req->buf = skb->data;
		req->length = mdm_data_rx_req_size;
		req->context = skb;

		info->rx_queued = get_timestamp();
		ret = usb_ep_queue(ep, req, GFP_KERNEL);
		spin_lock_irqsave(&port->rx_lock, flags);
		if (ret) {
			dev_kfree_skb_any(skb);

			pr_err_ratelimited("%s: rx queue failed\n", __func__);

			if (atomic_read(&port->connected))
				list_add(&req->list, &port->rx_idle);
			else
				usb_ep_free_request(ep, req);
			break;
		}
	}
	spin_unlock_irqrestore(&port->rx_lock, flags);
}

static void mdm_data_start_io(struct mdm_data_port *port)
{
	unsigned long	flags;
	struct usb_ep	*ep_out, *ep_in;
	int		ret;

	pr_debug("%s: port:%pK\n", __func__, port);

	spin_lock_irqsave(&port->rx_lock, flags);
	ep_out = port->out;
	spin_unlock_irqrestore(&port->rx_lock, flags);

	if (ep_out) {
		pr_debug("%s: ep_out:%pK\n", __func__, ep_out);
		ret = mdm_data_alloc_requests(ep_out,
					&port->rx_idle,
					port->rx_q_size,
					mdm_data_epout_complete,
					&port->rx_lock);
		if (ret) {
			pr_err("%s: rx req allocation failed\n", __func__);
			return;
		}
	}

	spin_lock_irqsave(&port->tx_lock, flags);
	ep_in = port->in;
	spin_unlock_irqrestore(&port->tx_lock, flags);
	if (!ep_in) {
		spin_lock_irqsave(&port->rx_lock, flags);
		if (ep_out)
			mdm_data_free_requests(ep_out, &port->rx_idle);
		spin_unlock_irqrestore(&port->rx_lock, flags);
		return;
	}

	pr_debug("%s: ep_in:%pK\n", __func__, ep_in);
	ret = mdm_data_alloc_requests(ep_in, &port->tx_idle,
		port->tx_q_size, mdm_data_epin_complete, &port->tx_lock);
	if (ret) {
		pr_err("%s: tx req allocation failed\n", __func__);
		spin_lock_irqsave(&port->rx_lock, flags);
		if (ep_out)
			mdm_data_free_requests(ep_out, &port->rx_idle);
		spin_unlock_irqrestore(&port->rx_lock, flags);
		return;
	}

	/* queue out requests */
	mdm_data_start_rx(port);
}

static void mdm_data_connect_w(struct work_struct *w)
{
	struct mdm_data_port	*port =
		container_of(w, struct mdm_data_port, connect_w);
	int			ret;

	if (!port || !atomic_read(&port->connected) ||
		!test_bit(CH_READY, &port->bridge_sts))
		return;

	pr_debug("%s: port:%pK\n", __func__, port);

	ret = data_bridge_open(&port->brdg);
	if (ret) {
		pr_err("%s: unable open bridge ch:%d err:%d\n",
				__func__, port->brdg.ch_id, ret);
		return;
	}

	set_bit(CH_OPENED, &port->bridge_sts);

	mdm_data_start_io(port);
}

static void mdm_data_disconnect_w(struct work_struct *w)
{
	struct mdm_data_port	*port =
		container_of(w, struct mdm_data_port, disconnect_w);

	if (!test_bit(CH_OPENED, &port->bridge_sts))
		return;

	pr_debug("%s: port:%pK\n", __func__, port);

	data_bridge_close(port->brdg.ch_id);
	clear_bit(CH_OPENED, &port->bridge_sts);
}

static void mdm_data_free_buffers(struct mdm_data_port *port)
{
	struct sk_buff	*skb;
	unsigned long	flags;

	spin_lock_irqsave(&port->tx_lock, flags);
	if (!port->in) {
		spin_unlock_irqrestore(&port->tx_lock, flags);
		return;
	}

	mdm_data_free_requests(port->in, &port->tx_idle);

	while ((skb = __skb_dequeue(&port->tx_skb_q)))
		dev_kfree_skb_any(skb);
	spin_unlock_irqrestore(&port->tx_lock, flags);

	spin_lock_irqsave(&port->rx_lock, flags);
	if (!port->out) {
		spin_unlock_irqrestore(&port->rx_lock, flags);
		return;
	}

	mdm_data_free_requests(port->out, &port->rx_idle);

	while ((skb = __skb_dequeue(&port->rx_skb_q)))
		dev_kfree_skb_any(skb);
	spin_unlock_irqrestore(&port->rx_lock, flags);
}

static int mdm_data_probe(struct platform_device *pdev)
{
	struct mdm_data_port *port;
	int id;

	pr_debug("%s: name:%s\n", __func__, pdev->name);

	id = bridge_name_to_id(pdev->name);
	if (id < 0) {
		pr_err("%s: invalid port\n", __func__);
		return -EINVAL;
	}

	port = mdm_data_ports[id];
	set_bit(CH_READY, &port->bridge_sts);

	/* if usb is online, try opening bridge */
	if (atomic_read(&port->connected))
		queue_work(port->wq, &port->connect_w);

	return 0;
}

/* mdm disconnect */
static int mdm_data_remove(struct platform_device *pdev)
{
	struct mdm_data_port *port;
	struct usb_ep	*ep_in;
	struct usb_ep	*ep_out;
	int id;

	pr_debug("%s: name:%s\n", __func__, pdev->name);

	id = bridge_name_to_id(pdev->name);
	if (id < 0) {
		pr_err("%s: invalid port\n", __func__);
		return -EINVAL;
	}

	port = mdm_data_ports[id];

	ep_in = port->in;
	if (ep_in)
		usb_ep_fifo_flush(ep_in);

	ep_out = port->out;
	if (ep_out)
		usb_ep_fifo_flush(ep_out);

	/* cancel pending writes to MDM */
	cancel_work_sync(&port->write_tomdm_w);

	mdm_data_free_buffers(port);

	cancel_work_sync(&port->connect_w);
	if (test_and_clear_bit(CH_OPENED, &port->bridge_sts))
		data_bridge_close(port->brdg.ch_id);
	clear_bit(CH_READY, &port->bridge_sts);
	clear_bit(RX_THROTTLED, &port->brdg.flags);

	return 0;
}

static void mdm_data_port_deinit(struct mdm_data_port *port)
{
	struct platform_driver	*pdrv = &port->pdrv;

	if (pdrv)
		platform_driver_unregister(pdrv);

	destroy_workqueue(port->wq);
}

static int mdm_data_port_init(struct mdm_data_port *port, const char *port_name)
{
	struct platform_driver	*pdrv;

	strlcpy(port->port_name, port_name, MAX_INST_NAME_LEN);

	port->wq = create_singlethread_workqueue(port->port_name);
	if (!port->wq) {
		pr_err("%s: Unable to create workqueue:%s\n",
						__func__, port->port_name);
		return -ENOMEM;
	}

	/* port initialization */
	spin_lock_init(&port->rx_lock);
	spin_lock_init(&port->tx_lock);

	INIT_WORK(&port->connect_w, mdm_data_connect_w);
	INIT_WORK(&port->disconnect_w, mdm_data_disconnect_w);
	INIT_WORK(&port->write_tohost_w, mdm_data_write_tohost);
	INIT_WORK(&port->write_tomdm_w, mdm_data_write_tomdm);

	INIT_LIST_HEAD(&port->tx_idle);
	INIT_LIST_HEAD(&port->rx_idle);

	skb_queue_head_init(&port->tx_skb_q);
	skb_queue_head_init(&port->rx_skb_q);

	port->brdg.name = port->port_name;
	port->brdg.ctx = port;
	port->brdg.ops.send_pkt = mdm_data_receive;
	port->brdg.ops.unthrottle_tx = mdm_data_unthrottle_tx;

	pdrv = &port->pdrv;
	pdrv->probe = mdm_data_probe;
	pdrv->remove = mdm_data_remove;
	pdrv->driver.name = port->port_name;
	pdrv->driver.owner = THIS_MODULE;

	platform_driver_register(pdrv);
	return 0;
}

#if defined(CONFIG_DEBUG_FS)
#define DEBUG_DATA_BUF_SIZE 4096

static unsigned int record_timestamp;
module_param(record_timestamp, uint, 0644);

static struct timestamp_buf dbg_data = {
	.idx = 0,
	.lck = __RW_LOCK_UNLOCKED(lck)
};

/*get_timestamp - returns time of day in us */
static unsigned int get_timestamp(void)
{
	struct timeval	tval;
	unsigned int	stamp;

	if (!record_timestamp)
		return 0;

	do_gettimeofday(&tval);
	/* 2^32 = 4294967296. Limit to 4096s. */
	stamp = tval.tv_sec & 0xFFF;
	stamp = stamp * 1000000 + tval.tv_usec;
	return stamp;
}

static void dbg_inc(unsigned int *idx)
{
	*idx = (*idx + 1) % (DBG_DATA_MAX-1);
}

/*
 * dbg_timestamp - Stores timestamp values of a SKB life cycle to debug buffer
 * @event: "DL": Downlink Data
 * @skb: SKB used to store timestamp values to debug buffer
 */
static void dbg_timestamp(char *event, struct sk_buff *skb)
{
	unsigned long		flags;
	struct timestamp_info	*info = (struct timestamp_info *)skb->cb;

	if (!record_timestamp)
		return;

	write_lock_irqsave(&dbg_data.lck, flags);

	scnprintf(dbg_data.buf[dbg_data.idx], DBG_DATA_MSG,
		  "%pK %u[%s] %u %u %u %u %u %u\n",
		  skb, skb->len, event, info->created, info->rx_queued,
		  info->rx_done, info->rx_done_sent, info->tx_queued,
		  get_timestamp());

	dbg_inc(&dbg_data.idx);

	write_unlock_irqrestore(&dbg_data.lck, flags);
}

/* show_timestamp: displays the timestamp buffer */
static ssize_t show_timestamp(struct file *file, char __user *ubuf,
		size_t count, loff_t *ppos)
{
	unsigned long	flags;
	unsigned int	i;
	unsigned int	j = 0;
	char		*buf;
	int		ret = 0;

	if (!record_timestamp)
		return 0;

	buf = kzalloc(sizeof(char) * DEBUG_DATA_BUF_SIZE, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	read_lock_irqsave(&dbg_data.lck, flags);

	i = dbg_data.idx;
	for (dbg_inc(&i); i != dbg_data.idx; dbg_inc(&i)) {
		if (!strnlen(dbg_data.buf[i], DBG_DATA_MSG))
			continue;
		j += scnprintf(buf + j, DEBUG_DATA_BUF_SIZE - j,
			       "%s\n", dbg_data.buf[i]);
	}

	read_unlock_irqrestore(&dbg_data.lck, flags);

	ret = simple_read_from_buffer(ubuf, count, ppos, buf, j);

	kfree(buf);

	return ret;
}

static const struct file_operations usb_data_timestamp_ops = {
	.read = show_timestamp,
};

static ssize_t mdm_data_read_stats(struct file *file,
	char __user *ubuf, size_t count, loff_t *ppos)
{
	struct mdm_data_port	*port;
	struct platform_driver	*pdrv;
	char			*buf;
	unsigned long		flags;
	int			ret;
	int			i;
	int			temp = 0;

	buf = kzalloc(sizeof(char) * DEBUG_DATA_BUF_SIZE, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	for (i = 0; i < MAX_BRIDGE_DEVICES; i++) {
		port = mdm_data_ports[i];
		pdrv = &port->pdrv;

		spin_lock_irqsave(&port->rx_lock, flags);
		temp += scnprintf(buf + temp, DEBUG_DATA_BUF_SIZE - temp,
				"\nName:           %s\n"
				"#PORT:%d port#:   %pK\n"
				"data_ch_open:	   %d\n"
				"data_ch_ready:    %d\n"
				"\n******UL INFO*****\n\n"
				"dpkts_to_modem:   %lu\n"
				"tomodem_drp_cnt:  %u\n"
				"rx_buf_len:       %u\n"
				"tx thld cnt       %u\n"
				"tx unthld cnt     %u\n"
				"TX_THROTTLED      %d\n",
				pdrv->driver.name,
				i, port,
				test_bit(CH_OPENED, &port->bridge_sts),
				test_bit(CH_READY, &port->bridge_sts),
				port->to_modem,
				port->tomodem_drp_cnt,
				port->rx_skb_q.qlen,
				port->tx_throttled_cnt,
				port->tx_unthrottled_cnt,
				test_bit(TX_THROTTLED, &port->brdg.flags));
		spin_unlock_irqrestore(&port->rx_lock, flags);

		spin_lock_irqsave(&port->tx_lock, flags);
		temp += scnprintf(buf + temp, DEBUG_DATA_BUF_SIZE - temp,
				"\n******DL INFO******\n\n"
				"dpkts_to_usbhost: %lu\n"
				"tx_buf_len:	   %u\n"
				"rx thld cnt	   %u\n"
				"rx unthld cnt	   %u\n"
				"uthld pnd skbs    %u\n"
				"RX_THROTTLED	   %d\n",
				port->to_host,
				port->tx_skb_q.qlen,
				port->rx_throttled_cnt,
				port->rx_unthrottled_cnt,
				port->unthrottled_pnd_skbs,
				test_bit(RX_THROTTLED, &port->brdg.flags));
		spin_unlock_irqrestore(&port->tx_lock, flags);

	}

	ret = simple_read_from_buffer(ubuf, count, ppos, buf, temp);

	kfree(buf);

	return ret;
}

static ssize_t mdm_data_reset_stats(struct file *file,
	const char __user *buf, size_t count, loff_t *ppos)
{
	struct mdm_data_port	*port;
	int			i;
	unsigned long		flags;

	for (i = 0; i < MAX_BRIDGE_DEVICES; i++) {
		port = mdm_data_ports[i];
		spin_lock_irqsave(&port->rx_lock, flags);
		port->to_modem = 0;
		port->tomodem_drp_cnt = 0;
		port->tx_throttled_cnt = 0;
		port->tx_unthrottled_cnt = 0;
		spin_unlock_irqrestore(&port->rx_lock, flags);

		spin_lock_irqsave(&port->tx_lock, flags);
		port->to_host = 0;
		port->rx_throttled_cnt = 0;
		port->rx_unthrottled_cnt = 0;
		port->unthrottled_pnd_skbs = 0;
		spin_unlock_irqrestore(&port->tx_lock, flags);
	}
	return count;
}

static const struct file_operations mdm_stats_ops = {
	.read = mdm_data_read_stats,
	.write = mdm_data_reset_stats,
};

static struct dentry	*data_dent;

static void mdm_data_debugfs_init(void)
{
	static struct dentry	*data_dfile_stats;
	static struct dentry	*data_dfile_tstamp;

	data_dent = debugfs_create_dir("usb_mdm_data", NULL);
	if (IS_ERR(data_dent))
		return;

	data_dfile_stats = debugfs_create_file("status", 0644, data_dent,
				NULL, &mdm_stats_ops);
	if (!data_dfile_stats || IS_ERR(data_dfile_stats))
		goto error;

	data_dfile_tstamp = debugfs_create_file("timestamp", 0644, data_dent,
				NULL, &usb_data_timestamp_ops);
	if (!data_dfile_tstamp || IS_ERR(data_dfile_tstamp))
		goto error;

	return;

error:
	debugfs_remove_recursive(data_dent);
	data_dent = NULL;
}

static void mdm_data_debugfs_exit(void)
{
	debugfs_remove_recursive(data_dent);
	data_dent = NULL;
}

#else

static void mdm_data_debugfs_init(void) { }
static void mdm_data_debugfs_exit(void) { }
static void dbg_timestamp(char *event, struct sk_buff *skb) { }
static unsigned int get_timestamp(void)
{
	return 0;
}

#endif

static void mdm_data_unbind(struct usb_configuration *c, struct usb_function *f)
{
	struct mdm_data_port *port = func_to_port(f);

	pr_debug("%s: start unbinding port %pK\n", __func__, port);
	usb_free_all_descriptors(f);
}

static int mdm_data_bind(struct usb_configuration *c, struct usb_function *f)
{
	bool out_desc = true;
	struct usb_interface_descriptor *data_desc = &intf_desc;
	struct usb_descriptor_header **fs_desc_hdr = fs_desc;
	struct usb_descriptor_header **hs_desc_hdr = hs_desc;
	struct usb_descriptor_header **ss_desc_hdr = ss_desc;
	struct usb_composite_dev *cdev = c->cdev;
	struct mdm_data_port *port = func_to_port(f);
	struct usb_ep *ep;
	int status;
	int id;

	pr_debug("%s: start binding port %pK\n", __func__, port);
	port->cdev = c->cdev;

	if (port->in_ep && !port->out_ep) {
		out_desc = false;
		data_desc = &intf_in_only_desc;
		fs_desc_hdr = fs_in_only_desc;
		hs_desc_hdr = hs_in_only_desc;
		ss_desc_hdr = ss_in_only_desc;
	} else if (!port->in_ep) {
		pr_err("%s: IN endpoint absent, invalid usecase!\n", __func__);
		return -EINVAL;
	}

	id = bridge_name_to_id(port->port_name);
	if (id < 0) {
		pr_err("%s: Invalid port\n", __func__);
		return -EINVAL;
	}

	data_desc->bInterfaceNumber = usb_interface_id(c, f);

	status = bridge_id_to_protocol(id);
	if (status < 0) {
		pr_err("%s: Invalid port\n", __func__);
		return status;
	}

	data_desc->bInterfaceProtocol = status;

	status = usb_string_id(cdev);
	if (status < 0)
		return status;
	mdm_data_string_defs[id].id = status;
	data_desc->iInterface = status;
	mdm_data_string_defs[id].s = "MDM Data";

	status = -ENODEV;
	ep = usb_ep_autoconfig(cdev->gadget, &fs_bulk_in_desc);
	if (!ep)
		goto fail;

	port->in = ep;
	ep->driver_data = cdev;
	hs_bulk_in_desc.bEndpointAddress =
				fs_bulk_in_desc.bEndpointAddress;
	ss_bulk_in_desc.bEndpointAddress =
				fs_bulk_in_desc.bEndpointAddress;

	if (out_desc) {
		ep = usb_ep_autoconfig(cdev->gadget, &fs_bulk_out_desc);
		if (!ep)
			goto fail;

		port->out = ep;
		ep->driver_data = cdev;
		hs_bulk_out_desc.bEndpointAddress =
					fs_bulk_out_desc.bEndpointAddress;
		ss_bulk_out_desc.bEndpointAddress =
					fs_bulk_out_desc.bEndpointAddress;
	}

	status = usb_assign_descriptors(f, fs_desc_hdr, hs_desc_hdr,
						ss_desc_hdr, ss_desc_hdr);
	if (status)
		goto fail;

	return 0;

fail:
	if (port->out)
		port->out->driver_data = NULL;
	if (port->in)
		port->in->driver_data = NULL;

	pr_err("%s: can't bind, err %d\n", __func__, status);
	return status;
}

static void mdm_data_disable(struct usb_function *f)
{
	struct mdm_data_port *port = func_to_port(f);
	unsigned long flags;

	pr_debug("%s: Disabling\n", __func__);

	mdm_data_free_buffers(port);
	usb_ep_disable(port->in);
	port->in->driver_data = NULL;

	if (port->out) {
		usb_ep_disable(port->out);
		port->out->driver_data = NULL;
	}

	atomic_set(&port->connected, 0);

	spin_lock_irqsave(&port->tx_lock, flags);
	clear_bit(RX_THROTTLED, &port->brdg.flags);
	spin_unlock_irqrestore(&port->tx_lock, flags);

	spin_lock_irqsave(&port->rx_lock, flags);
	clear_bit(TX_THROTTLED, &port->brdg.flags);
	spin_unlock_irqrestore(&port->rx_lock, flags);

	queue_work(port->wq, &port->disconnect_w);
}

static int
mdm_data_set_alt(struct usb_function *f, unsigned int intf, unsigned int alt)
{
	struct mdm_data_port *port = func_to_port(f);
	struct usb_composite_dev *cdev = f->config->cdev;
	unsigned long flags;
	int ret = 0;

	pr_debug("%s: mdm_data_port: %pK\n", __func__, port);

	port->tx_q_size = mdm_data_tx_q_size;
	port->rx_q_size = mdm_data_rx_q_size;
	ret = config_ep_by_speed(cdev->gadget, f, port->in);
	if (ret) {
		port->in->desc = NULL;
		return -EINVAL;
	}

	port->in->driver_data = port;
	ret = usb_ep_enable(port->in);
	if (ret) {
		ERROR(port->cdev, "can't enable %s, result %d\n",
						port->in->name, ret);
		return ret;
	}

	if (port->out) {
		ret = config_ep_by_speed(cdev->gadget, f, port->out);
		if (ret) {
			usb_ep_disable(port->in);
			port->out->desc = NULL;
			return -EINVAL;
		}

		port->out->driver_data = port;
		ret = usb_ep_enable(port->out);
		if (ret) {
			usb_ep_disable(port->in);
			ERROR(port->cdev, "can't enable %s, result %d\n",
						port->out->name, ret);
			return ret;
		}
	}

	atomic_set(&port->connected, 1);

	spin_lock_irqsave(&port->tx_lock, flags);
	port->to_host = 0;
	port->rx_throttled_cnt = 0;
	port->rx_unthrottled_cnt = 0;
	port->unthrottled_pnd_skbs = 0;
	spin_unlock_irqrestore(&port->tx_lock, flags);

	spin_lock_irqsave(&port->rx_lock, flags);
	port->to_modem = 0;
	port->tomodem_drp_cnt = 0;
	port->tx_throttled_cnt = 0;
	port->tx_unthrottled_cnt = 0;
	spin_unlock_irqrestore(&port->rx_lock, flags);

	queue_work(port->wq, &port->connect_w);

	return ret;
}

static void mdm_data_free(struct usb_function *f)
{
	struct mdm_data_opts *opts;

	opts = container_of(f->fi, struct mdm_data_opts, func_inst);
	mutex_lock(&opts->lock);
	--opts->refcnt;
	mutex_unlock(&opts->lock);
}

static struct
usb_function *mdm_data_bind_config(struct usb_function_instance *fi)
{
	struct mdm_data_opts *opts;
	struct usb_function *f;

	opts = container_of(fi, struct mdm_data_opts, func_inst);
	f = &opts->ctxt->function;

	mutex_lock(&opts->lock);
	opts->ctxt->in_ep = opts->in_ep;
	opts->ctxt->out_ep = opts->out_ep;
	++opts->refcnt;
	mutex_unlock(&opts->lock);

	f->name = opts->ctxt->port_name;
	f->strings = mdm_data_strings;
	f->bind = mdm_data_bind;
	f->unbind = mdm_data_unbind;
	f->set_alt = mdm_data_set_alt;
	f->disable = mdm_data_disable;
	f->free_func = mdm_data_free;

	return f;
}

static inline struct mdm_data_opts *to_mdm_data_opts(struct config_item *item)
{
	return container_of(to_config_group(item), struct mdm_data_opts,
				func_inst.group);
}

static void mdm_data_opts_release(struct config_item *item)
{
	struct mdm_data_opts *opts = to_mdm_data_opts(item);

	usb_put_function_instance(&opts->func_inst);
}

static struct configfs_item_operations mdm_data_item_ops = {
	.release	= mdm_data_opts_release,
};

#define MDM_DATA_BOOL_ATTRIBUTE(name)					\
static ssize_t f_mdm_data_##name##_show(struct config_item *item, char *page)  \
{									\
	struct mdm_data_opts *opts = to_mdm_data_opts(item);		\
	int result;							\
									\
	mutex_lock(&opts->lock);					\
	result = snprintf(page, PAGE_SIZE, "%d\n", opts->name);		\
	mutex_unlock(&opts->lock);					\
									\
	return result;							\
}									\
									\
static ssize_t f_mdm_data_##name##_store(struct config_item *item,	\
					const char *page, size_t len)	\
{									\
	struct mdm_data_opts *opts = to_mdm_data_opts(item);		\
	bool present;							\
	int ret;							\
									\
	mutex_lock(&opts->lock);					\
	if (opts->refcnt) {						\
		ret = -EBUSY;						\
		goto end;						\
	}								\
									\
	ret = kstrtobool(page, &present);				\
	if (ret)							\
		goto end;						\
									\
	opts->name = present;						\
	ret = len;							\
									\
end:									\
	mutex_unlock(&opts->lock);					\
	return ret;							\
}									\
									\
CONFIGFS_ATTR(f_mdm_data_, name)

MDM_DATA_BOOL_ATTRIBUTE(in_ep);
MDM_DATA_BOOL_ATTRIBUTE(out_ep);

static struct configfs_attribute *mdm_data_attrs[] = {
	&f_mdm_data_attr_in_ep,
	&f_mdm_data_attr_out_ep,
	NULL,
};

static struct config_item_type mdm_data_func_type = {
	.ct_item_ops	= &mdm_data_item_ops,
	.ct_attrs	= mdm_data_attrs,
	.ct_owner	= THIS_MODULE,
};

static int
mdm_data_set_inst_name(struct usb_function_instance *fi, const char *name)
{
	struct mdm_data_opts *opts;
	int name_len, ret;
	int id;

	opts = container_of(fi, struct mdm_data_opts, func_inst);
	name_len = strlen(name) + 1;
	if (name_len > MAX_INST_NAME_LEN)
		return -ENAMETOOLONG;

	id = bridge_name_to_id(name);
	if (id < 0) {
		pr_err("%s: Failed to find gadget ID for %s instance\n",
							__func__, name);
		return -EINVAL;
	}

	opts->ctxt = mdm_data_ports[id];
	ret = mdm_data_port_init(opts->ctxt, name);
	if (ret) {
		pr_err("%s: Unable to initialize inst %s\n", __func__, name);
		return ret;
	}

	return 0;
}

static void mdm_data_free_inst(struct usb_function_instance *f)
{
	struct mdm_data_opts *opts;

	opts = container_of(f, struct mdm_data_opts, func_inst);
	mdm_data_port_deinit(opts->ctxt);
	kfree(opts->ctxt);
	mutex_destroy(&opts->lock);
	kfree(opts);
}

static struct usb_function_instance *mdm_data_alloc_inst(void)
{
	struct mdm_data_opts *opts;

	opts = kzalloc(sizeof(*opts), GFP_KERNEL);
	if (!opts)
		return ERR_PTR(-ENOMEM);

	mutex_init(&opts->lock);
	opts->in_ep = opts->out_ep = false;
	opts->func_inst.set_inst_name = mdm_data_set_inst_name;
	opts->func_inst.free_func_inst = mdm_data_free_inst;
	config_group_init_type_name(&opts->func_inst.group, "",
						&mdm_data_func_type);

	return &opts->func_inst;
}

static struct usb_function *mdm_data_alloc(struct usb_function_instance *fi)
{
	return mdm_data_bind_config(fi);
}

DECLARE_USB_FUNCTION(mdm_data, mdm_data_alloc_inst, mdm_data_alloc);

static int __init mdm_data_init(void)
{
	struct mdm_data_port *dev;
	int ret, i;
	int num_instances = 0;

	for (i = 0; i < MAX_BRIDGE_DEVICES; i++) {
		dev = kzalloc(sizeof(*dev), GFP_KERNEL);
		if (!dev) {
			ret = -ENOMEM;
			goto dev_free;
		}

		num_instances++;
		mdm_data_ports[i] = dev;
	}

	ret = usb_function_register(&mdm_datausb_func);
	if (ret) {
		pr_err("%s: failed to register mdm_data %d\n", __func__, ret);
		goto dev_free;
	}

	mdm_data_debugfs_init();

	return 0;

dev_free:
	for (i = 0; i < num_instances; i++) {
		dev = mdm_data_ports[i];
		kfree(dev);
		mdm_data_ports[i] = NULL;
	}

	return ret;
}

static void __exit mdm_data_exit(void)
{
	struct mdm_data_port *dev;
	int i;

	mdm_data_debugfs_exit();
	usb_function_unregister(&mdm_datausb_func);
	for (i = 0; i < MAX_BRIDGE_DEVICES; i++) {
		dev = mdm_data_ports[i];
		kfree(dev);
		mdm_data_ports[i] = NULL;
	}
}

module_init(mdm_data_init);
module_exit(mdm_data_exit);

MODULE_DESCRIPTION("USB Modem data function driver");
MODULE_LICENSE("GPL v2");
