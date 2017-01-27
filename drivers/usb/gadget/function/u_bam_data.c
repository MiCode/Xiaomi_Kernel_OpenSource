/* Copyright (c) 2012-2016, The Linux Foundation. All rights reserved.
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

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) "%s: " fmt, __func__

#include <linux/kernel.h>
#include <linux/interrupt.h>
#include <linux/device.h>
#include <linux/bitops.h>
#include <linux/usb/gadget.h>

#include <linux/usb_bam.h>

#include "u_bam_data.h"

#define BAM_DATA_RX_Q_SIZE	128
#define BAM_DATA_MUX_RX_REQ_SIZE  2048   /* Must be 1KB aligned */
#define BAM_DATA_PENDING_LIMIT	220

#define SYS_BAM_RX_PKT_FLOW_CTRL_SUPPORT	1
#define SYS_BAM_RX_PKT_FCTRL_EN_TSHOLD		500
#define SYS_BAM_RX_PKT_FCTRL_DIS_TSHOLD		300

static unsigned int bam_ipa_rx_fctrl_support = SYS_BAM_RX_PKT_FLOW_CTRL_SUPPORT;
module_param(bam_ipa_rx_fctrl_support, uint, S_IRUGO | S_IWUSR);

static unsigned int bam_ipa_rx_fctrl_en_thld = SYS_BAM_RX_PKT_FCTRL_EN_TSHOLD;
module_param(bam_ipa_rx_fctrl_en_thld, uint, S_IRUGO | S_IWUSR);

static unsigned int bam_ipa_rx_fctrl_dis_thld = SYS_BAM_RX_PKT_FCTRL_DIS_TSHOLD;
module_param(bam_ipa_rx_fctrl_dis_thld, uint, S_IRUGO | S_IWUSR);

static struct workqueue_struct *bam_data_wq;
static int n_bam2bam_data_ports;

unsigned int bam_data_rx_q_size = BAM_DATA_RX_Q_SIZE;
module_param(bam_data_rx_q_size, uint, S_IRUGO | S_IWUSR);

static unsigned int bam_data_mux_rx_req_size = BAM_DATA_MUX_RX_REQ_SIZE;
module_param(bam_data_mux_rx_req_size, uint, S_IRUGO | S_IWUSR);

#define SPS_PARAMS_SPS_MODE		BIT(5)
#define SPS_PARAMS_TBE		        BIT(6)
#define MSM_VENDOR_ID			BIT(16)

struct rndis_data_ch_info {
	/* this provides downlink (device->host i.e host) side configuration*/
	u32 dl_max_transfer_size;
	/* this provides uplink (host->device i.e device) side configuration */
	u32 ul_max_transfer_size;
	u32 ul_max_packets_number;
	bool ul_aggregation_enable;
	u32 prod_clnt_hdl;
	u32 cons_clnt_hdl;
	void *priv;
};

struct sys2ipa_sw_data {
	void		*teth_priv;
	ipa_notify_cb	teth_cb;
};

struct bam_data_ch_info {
	unsigned long		flags;
	unsigned		id;

	struct bam_data_port	*port;
	struct work_struct	write_tobam_w;

	struct usb_request	*rx_req;
	struct usb_request	*tx_req;

	u32			src_pipe_idx;
	u32			dst_pipe_idx;
	u8			src_connection_idx;
	u8			dst_connection_idx;
	enum usb_ctrl		usb_bam_type;

	enum function_type			func_type;
	enum transport_type			trans;
	struct usb_bam_connect_ipa_params	ipa_params;

	/* UL workaround parameters */
	struct sys2ipa_sw_data	ul_params;
	struct list_head	rx_idle;
	struct sk_buff_head	rx_skb_q;
	int			total_skb;
	int			freed_skb;
	int			freed_rx_reqs;
	int			alloc_rx_reqs;
	struct sk_buff_head	rx_skb_idle;
	enum usb_bam_pipe_type	src_pipe_type;
	enum usb_bam_pipe_type	dst_pipe_type;
	unsigned int		pending_with_bam;
	int			rx_buffer_size;

	unsigned int		rx_flow_control_disable;
	unsigned int		rx_flow_control_enable;
	unsigned int		rx_flow_control_triggered;
	/*
	 * used for RNDIS/ECM network interface based design
	 * to indicate ecm/rndis pipe connect notifiaction is sent
	 * to ecm_ipa/rndis_ipa.
	 */
	atomic_t		pipe_connect_notified;
	bool			tx_req_dequeued;
	bool			rx_req_dequeued;
};

enum u_bam_data_event_type {
	U_BAM_DATA_DISCONNECT_E = 0,
	U_BAM_DATA_CONNECT_E,
	U_BAM_DATA_SUSPEND_E,
	U_BAM_DATA_RESUME_E
};

struct bam_data_port {
	bool                            is_ipa_connected;
	enum u_bam_data_event_type	last_event;
	unsigned			port_num;
	spinlock_t			port_lock;
	unsigned int                    ref_count;
	struct data_port		*port_usb;
	struct usb_gadget		*gadget;
	struct bam_data_ch_info		data_ch;

	struct work_struct		connect_w;
	struct work_struct		disconnect_w;
	struct work_struct		suspend_w;
	struct work_struct		resume_w;
};
struct  usb_bam_data_connect_info {
	u32 usb_bam_pipe_idx;
	u32 peer_pipe_idx;
	u32 usb_bam_handle;
};

struct bam_data_port *bam2bam_data_ports[BAM2BAM_DATA_N_PORTS];
static struct rndis_data_ch_info rndis_data;

static void bam2bam_data_suspend_work(struct work_struct *w);
static void bam2bam_data_resume_work(struct work_struct *w);
static void bam_data_free_reqs(struct bam_data_port *port);

/*----- sys2bam towards the IPA (UL workaround) --------------- */

static int bam_data_alloc_requests(struct usb_ep *ep, struct list_head *head,
		int num,
		void (*cb)(struct usb_ep *ep, struct usb_request *),
		gfp_t flags)
{
	int i;
	struct bam_data_port	*port = ep->driver_data;
	struct bam_data_ch_info	*d = &port->data_ch;
	struct usb_request *req;

	pr_debug("%s: ep:%pK head:%pK num:%d cb:%pK", __func__,
			ep, head, num, cb);

	if (d->alloc_rx_reqs) {
		pr_err("%s(): reqs are already allocated.\n", __func__);
		WARN_ON(1);
		return -EINVAL;
	}

	for (i = 0; i < num; i++) {
		req = usb_ep_alloc_request(ep, flags);
		if (!req) {
			pr_err("%s: req allocated:%d\n", __func__, i);
			return list_empty(head) ? -ENOMEM : 0;
		}
		d->alloc_rx_reqs++;
		req->complete = cb;
		list_add_tail(&req->list, head);
	}

	return 0;
}

static inline dma_addr_t bam_data_get_dma_from_skb(struct sk_buff *skb)
{
	return *((dma_addr_t *)(skb->cb));
}

/* This function should be called with port_lock lock taken */
static struct sk_buff *bam_data_alloc_skb_from_pool(
	struct bam_data_port *port)
{
	struct bam_data_ch_info *d;
	struct sk_buff *skb = NULL;
	dma_addr_t      skb_buf_dma_addr;
	struct data_port  *data_port;
	struct usb_gadget *gadget;

	if (!port)
		return NULL;
	d = &port->data_ch;
	if (!d)
		return NULL;

	if (d->rx_skb_idle.qlen == 0) {
		/*
		 * In case skb idle pool is empty, we allow to allocate more
		 * skbs so we dynamically enlarge the pool size when needed.
		 * Therefore, in steady state this dynamic allocation will
		 * stop when the pool will arrive to its optimal size.
		 */
		pr_debug("%s: allocate skb\n", __func__);
		skb = alloc_skb(d->rx_buffer_size + BAM_MUX_HDR, GFP_ATOMIC);
		if (!skb) {
			pr_err("%s: alloc skb failed\n", __func__);
			goto alloc_exit;
		}

		d->total_skb++;
		skb_reserve(skb, BAM_MUX_HDR);

		data_port = port->port_usb;
		if (data_port && data_port->cdev && data_port->cdev->gadget) {
			gadget = data_port->cdev->gadget;

			skb_buf_dma_addr =
				dma_map_single(&gadget->dev, skb->data,
					d->rx_buffer_size, DMA_BIDIRECTIONAL);

			if (dma_mapping_error(&gadget->dev, skb_buf_dma_addr)) {
				pr_err("%s: Could not DMA map SKB buffer\n",
					__func__);
				skb_buf_dma_addr = DMA_ERROR_CODE;
			}
		} else {
			pr_err("%s: Could not DMA map SKB buffer\n", __func__);
			skb_buf_dma_addr = DMA_ERROR_CODE;
		}

		memcpy(skb->cb, &skb_buf_dma_addr,
			sizeof(skb_buf_dma_addr));

	} else {
		pr_debug("%s: pull skb from pool\n", __func__);
		skb = __skb_dequeue(&d->rx_skb_idle);
	}

alloc_exit:
	return skb;
}

static void bam_data_free_skb_to_pool(
	struct bam_data_port *port,
	struct sk_buff *skb)
{
	struct bam_data_ch_info *d;

	if (!port) {
		dev_kfree_skb_any(skb);
		return;
	}
	d = &port->data_ch;
	if (!d) {
		dev_kfree_skb_any(skb);
		return;
	}

	skb->len = 0;
	skb_reset_tail_pointer(skb);
	__skb_queue_tail(&d->rx_skb_idle, skb);
}

static void bam_data_write_done(void *p, struct sk_buff *skb)
{
	struct bam_data_port	*port = p;
	struct bam_data_ch_info	*d = &port->data_ch;
	unsigned long flags;

	if (!skb)
		return;

	spin_lock_irqsave(&port->port_lock, flags);
	bam_data_free_skb_to_pool(port, skb);

	d->pending_with_bam--;

	pr_debug("%s: port:%pK d:%pK pbam:%u, pno:%d\n", __func__,
			port, d, d->pending_with_bam, port->port_num);

	spin_unlock_irqrestore(&port->port_lock, flags);

	queue_work(bam_data_wq, &d->write_tobam_w);
}

static void bam_data_ipa_sys2bam_notify_cb(void *priv,
		enum ipa_dp_evt_type event, unsigned long data)
{
	struct sys2ipa_sw_data *ul = (struct sys2ipa_sw_data *)priv;
	struct bam_data_port	*port;
	struct bam_data_ch_info	*d;

	switch (event) {
	case IPA_WRITE_DONE:
		d = container_of(ul, struct bam_data_ch_info, ul_params);
		port = container_of(d, struct bam_data_port, data_ch);
		/* call into bam_demux functionality that'll recycle the data */
		bam_data_write_done(port, (struct sk_buff *)(data));
		break;
	case IPA_RECEIVE:
		/* call the callback given by tethering driver init function
		 * (and was given to ipa_connect)
		 */
		if (ul->teth_cb)
			ul->teth_cb(ul->teth_priv, event, data);
		break;
	default:
		/* unexpected event */
		pr_err("%s: unexpected event %d\n", __func__, event);
		break;
	}
}


static void bam_data_start_rx(struct bam_data_port *port)
{
	struct usb_request		*req;
	struct bam_data_ch_info		*d;
	struct usb_ep			*ep;
	int				ret;
	struct sk_buff			*skb;
	unsigned long			flags;

	if (!port->port_usb) {
		return;
	}

	d = &port->data_ch;
	ep = port->port_usb->out;

	spin_lock_irqsave(&port->port_lock, flags);
	while (port->port_usb && !list_empty(&d->rx_idle)) {

		if (bam_ipa_rx_fctrl_support &&
			d->rx_skb_q.qlen >= bam_ipa_rx_fctrl_en_thld)
			break;

		req = list_first_entry(&d->rx_idle, struct usb_request, list);
		skb = bam_data_alloc_skb_from_pool(port);
		if (!skb)
			break;
		list_del(&req->list);
		req->buf = skb->data;
		req->dma = bam_data_get_dma_from_skb(skb);
		req->length = d->rx_buffer_size;

		if (req->dma != DMA_ERROR_CODE)
			req->dma_pre_mapped = true;
		else
			req->dma_pre_mapped = false;

		req->context = skb;
		spin_unlock_irqrestore(&port->port_lock, flags);
		ret = usb_ep_queue(ep, req, GFP_ATOMIC);
		spin_lock_irqsave(&port->port_lock, flags);
		if (ret) {
			bam_data_free_skb_to_pool(port, skb);

			pr_err("%s: rx queue failed %d\n", __func__, ret);

			if (port->port_usb)
				list_add(&req->list, &d->rx_idle);
			else
				usb_ep_free_request(ep, req);
			break;
		}
	}
	spin_unlock_irqrestore(&port->port_lock, flags);
}

static void bam_data_epout_complete(struct usb_ep *ep, struct usb_request *req)
{
	struct bam_data_port	*port = ep->driver_data;
	struct bam_data_ch_info	*d = &port->data_ch;
	struct sk_buff		*skb = req->context;
	int			status = req->status;
	int			queue = 0;
	unsigned long		flags;

	switch (status) {
	case 0:
		skb_put(skb, req->actual);
		queue = 1;
		break;
	case -ECONNRESET:
	case -ESHUTDOWN:
		/* cable disconnection */
		spin_lock_irqsave(&port->port_lock, flags);
		bam_data_free_skb_to_pool(port, skb);
		d->freed_rx_reqs++;
		spin_unlock_irqrestore(&port->port_lock, flags);
		req->buf = 0;
		usb_ep_free_request(ep, req);
		return;
	default:
		pr_err("%s: %s response error %d, %d/%d\n", __func__,
			ep->name, status, req->actual, req->length);
		spin_lock_irqsave(&port->port_lock, flags);
		bam_data_free_skb_to_pool(port, skb);
		spin_unlock_irqrestore(&port->port_lock, flags);
		break;
	}

	spin_lock(&port->port_lock);
	if (queue) {
		__skb_queue_tail(&d->rx_skb_q, skb);
		if (!usb_bam_get_prod_granted(d->usb_bam_type,
					d->dst_connection_idx)) {
			list_add_tail(&req->list, &d->rx_idle);
			spin_unlock(&port->port_lock);
			pr_err_ratelimited("usb bam prod is not granted.\n");
			return;
		} else
			queue_work(bam_data_wq, &d->write_tobam_w);
	}

	if (bam_mux_rx_fctrl_support &&
		d->rx_skb_q.qlen >= bam_ipa_rx_fctrl_en_thld) {
		if (!d->rx_flow_control_triggered) {
			d->rx_flow_control_triggered = 1;
			d->rx_flow_control_enable++;
		}
		list_add_tail(&req->list, &d->rx_idle);
		spin_unlock(&port->port_lock);
		return;
	}

	skb = bam_data_alloc_skb_from_pool(port);
	if (!skb) {
		list_add_tail(&req->list, &d->rx_idle);
		spin_unlock(&port->port_lock);
		return;
	}
	spin_unlock(&port->port_lock);

	req->buf = skb->data;
	req->dma = bam_data_get_dma_from_skb(skb);
	req->length = d->rx_buffer_size;

	if (req->dma != DMA_ERROR_CODE)
		req->dma_pre_mapped = true;
	else
		req->dma_pre_mapped = false;

	req->context = skb;

	status = usb_ep_queue(ep, req, GFP_ATOMIC);
	if (status) {
		pr_err_ratelimited("%s: data rx enqueue err %d\n",
						__func__, status);
		spin_lock(&port->port_lock);
		bam_data_free_skb_to_pool(port, skb);
		list_add_tail(&req->list, &d->rx_idle);
		spin_unlock(&port->port_lock);
	}
}
/* It should be called with port_lock acquire. */
static int bam_data_sys2bam_alloc_req(struct bam_data_port *port, bool in)
{
	int			ret;
	struct usb_ep		*ep;
	struct list_head	*idle;
	unsigned		queue_size;
	void		(*ep_complete)(struct usb_ep *, struct usb_request *);

	if (!port->port_usb)
		return -EBUSY;
	if (in)
		return -ENODEV;

	ep = port->port_usb->out;
	idle = &port->data_ch.rx_idle;
	queue_size = bam_data_rx_q_size;
	ep_complete = bam_data_epout_complete;

	ret = bam_data_alloc_requests(ep, idle, queue_size, ep_complete,
			GFP_ATOMIC);
	if (ret) {
		pr_err("%s: allocation failed\n", __func__);
	}

	return ret;
}

static void bam_data_write_toipa(struct work_struct *w)
{
	struct bam_data_port	*port;
	struct bam_data_ch_info	*d;
	struct sk_buff		*skb;
	int			ret;
	int			qlen;
	unsigned long		flags;
	dma_addr_t		skb_dma_addr;
	struct ipa_tx_meta	ipa_meta = {0x0};

	d = container_of(w, struct bam_data_ch_info, write_tobam_w);
	port = d->port;

	spin_lock_irqsave(&port->port_lock, flags);
	if (!port->port_usb) {
		spin_unlock_irqrestore(&port->port_lock, flags);
		return;
	}

	while (d->pending_with_bam < BAM_PENDING_PKTS_LIMIT &&
	       usb_bam_get_prod_granted(d->usb_bam_type,
					d->dst_connection_idx)) {
		skb =  __skb_dequeue(&d->rx_skb_q);
		if (!skb)
			break;

		d->pending_with_bam++;

		pr_debug("%s: port:%pK d:%pK pbam:%u pno:%d\n", __func__,
				port, d, d->pending_with_bam, port->port_num);

		spin_unlock_irqrestore(&port->port_lock, flags);

		skb_dma_addr = bam_data_get_dma_from_skb(skb);
		if (skb_dma_addr != DMA_ERROR_CODE) {
			ipa_meta.dma_address = skb_dma_addr;
			ipa_meta.dma_address_valid = true;
		}

		ret = ipa_tx_dp(IPA_CLIENT_USB_PROD, skb, &ipa_meta);

		spin_lock_irqsave(&port->port_lock, flags);
		if (ret) {
			pr_debug_ratelimited("%s: write error:%d\n",
							__func__, ret);
			d->pending_with_bam--;
			bam_data_free_skb_to_pool(port, skb);
			break;
		}
	}

	qlen = d->rx_skb_q.qlen;
	spin_unlock_irqrestore(&port->port_lock, flags);

	if (qlen < bam_ipa_rx_fctrl_dis_thld) {
		if (d->rx_flow_control_triggered) {
			d->rx_flow_control_disable++;
			d->rx_flow_control_triggered = 0;
		}
		bam_data_start_rx(port);
	}

}

/*------------data_path----------------------------*/

static void bam_data_endless_rx_complete(struct usb_ep *ep,
					 struct usb_request *req)
{
	int status = req->status;

	pr_debug("%s: status: %d\n", __func__, status);
}

static void bam_data_endless_tx_complete(struct usb_ep *ep,
					 struct usb_request *req)
{
	int status = req->status;

	pr_debug("%s: status: %d\n", __func__, status);
}

static void bam_data_start_endless_rx(struct bam_data_port *port)
{
	struct bam_data_ch_info *d = &port->data_ch;
	struct usb_ep *ep;
	unsigned long flags;
	int status;

	spin_lock_irqsave(&port->port_lock, flags);
	if (!port->port_usb || !d->rx_req) {
		spin_unlock_irqrestore(&port->port_lock, flags);
		return;
	}
	ep = port->port_usb->out;
	spin_unlock_irqrestore(&port->port_lock, flags);

	pr_debug("%s: enqueue\n", __func__);
	status = usb_ep_queue(ep, d->rx_req, GFP_ATOMIC);
	if (status)
		pr_err("error enqueuing transfer, %d\n", status);
}

static void bam_data_start_endless_tx(struct bam_data_port *port)
{
	struct bam_data_ch_info *d = &port->data_ch;
	struct usb_ep *ep;
	unsigned long flags;
	int status;

	spin_lock_irqsave(&port->port_lock, flags);
	if (!port->port_usb || !d->tx_req) {
		spin_unlock_irqrestore(&port->port_lock, flags);
		return;
	}
	ep = port->port_usb->in;
	spin_unlock_irqrestore(&port->port_lock, flags);

	pr_debug("%s: enqueue\n", __func__);
	status = usb_ep_queue(ep, d->tx_req, GFP_ATOMIC);
	if (status)
		pr_err("error enqueuing transfer, %d\n", status);
}

static void bam_data_stop_endless_rx(struct bam_data_port *port)
{
	struct bam_data_ch_info *d = &port->data_ch;
	unsigned long flags;
	int status;

	spin_lock_irqsave(&port->port_lock, flags);
	if (!port->port_usb) {
		spin_unlock_irqrestore(&port->port_lock, flags);
		return;
	}

	d->rx_req_dequeued = true;

	pr_debug("%s: dequeue\n", __func__);
	status = usb_ep_dequeue(port->port_usb->out, d->rx_req);
	if (status)
		pr_err("%s: error dequeuing transfer, %d\n", __func__, status);

	spin_unlock_irqrestore(&port->port_lock, flags);
}

static void bam_data_stop_endless_tx(struct bam_data_port *port)
{
	struct bam_data_ch_info *d = &port->data_ch;
	struct usb_ep *ep;
	unsigned long flags;
	int status;

	spin_lock_irqsave(&port->port_lock, flags);
	if (!port->port_usb) {
		spin_unlock_irqrestore(&port->port_lock, flags);
		return;
	}
	ep = port->port_usb->in;
	d->tx_req_dequeued = true;
	spin_unlock_irqrestore(&port->port_lock, flags);

	pr_debug("%s: dequeue\n", __func__);
	status = usb_ep_dequeue(ep, d->tx_req);
	if (status)
		pr_err("%s: error dequeuing transfer, %d\n", __func__, status);
}

static void bam2bam_free_rx_skb_idle_list(struct bam_data_port *port)
{
	struct bam_data_ch_info *d;
	struct sk_buff *skb;
	dma_addr_t dma_addr;
	struct usb_gadget *gadget = NULL;

	if (!port) {
		pr_err("%s(): Port is NULL.\n", __func__);
		return;
	}

	d = &port->data_ch;
	if (!d) {
		pr_err("%s(): port->data_ch is NULL.\n", __func__);
		return;
	}

	if (!port->port_usb) {
		pr_err("%s(): port->port_usb is NULL.\n", __func__);
		return;
	}

	if (!port->port_usb->cdev) {
		pr_err("port->port_usb->cdev is NULL");
		return;
	}

	gadget = port->port_usb->cdev->gadget;
	if (!gadget) {
		pr_err("%s(): gadget is NULL.\n", __func__);
		return;
	}

	while (d->rx_skb_idle.qlen > 0) {
		skb = __skb_dequeue(&d->rx_skb_idle);
		dma_addr = gbam_get_dma_from_skb(skb);

		if (gadget && dma_addr != DMA_ERROR_CODE) {
			dma_unmap_single(&gadget->dev, dma_addr,
				bam_mux_rx_req_size, DMA_BIDIRECTIONAL);
			dma_addr = DMA_ERROR_CODE;
			memcpy(skb->cb, &dma_addr, sizeof(dma_addr));
		}
		dev_kfree_skb_any(skb);
		d->freed_skb++;
	}

	pr_debug("%s(): Freed %d SKBs from rx_skb_idle queue\n", __func__,
							d->freed_skb);
}

/*
 * bam_data_ipa_disconnect()- Perform USB IPA function level disconnect
 * struct bam_data_ch_info - Per USB IPA port data structure
 *
 * Make sure to call IPA rndis/ecm/mbim related disconnect APIs() only
 * if those APIs init counterpart is already performed.
 * MBIM: teth_bridge_connect() is NO_OPS and teth_bridge_init() is
 * being called with atomic context on cable connect, hence there is no
 * need to consider for this check. pipe_connect_notified is being used
 * for RNDIS/ECM driver due to its different design with usage of
 * network interface created by IPA driver.
 */
static void bam_data_ipa_disconnect(struct bam_data_ch_info *d)
{
	pr_debug("%s(): pipe_connect_notified:%d\n",
		__func__, atomic_read(&d->pipe_connect_notified));
	/*
	 * Check if pipe_connect_notified is set to 1, then perform disconnect
	 * part and set pipe_connect_notified to zero.
	 */
	if (atomic_xchg(&d->pipe_connect_notified, 0) == 1) {
		void *priv;

		if (d->func_type == USB_FUNC_ECM) {
			priv = ecm_qc_get_ipa_priv();
			ecm_ipa_disconnect(priv);
		} else if (d->func_type == USB_FUNC_RNDIS) {
			priv = rndis_qc_get_ipa_priv();
			rndis_ipa_pipe_disconnect_notify(priv);
		}
		pr_debug("%s(): net interface is disconnected.\n", __func__);
	}

	if (d->func_type == USB_FUNC_MBIM) {
		pr_debug("%s(): teth_bridge() disconnected\n", __func__);
		teth_bridge_disconnect(d->ipa_params.src_client);
	}
}

static void bam2bam_data_disconnect_work(struct work_struct *w)
{
	struct bam_data_port *port =
			container_of(w, struct bam_data_port, disconnect_w);
	struct bam_data_ch_info *d;
	unsigned long flags;
	int ret;

	spin_lock_irqsave(&port->port_lock, flags);

	if (!port->is_ipa_connected) {
		pr_debug("%s: Already disconnected. Bailing out.\n", __func__);
		spin_unlock_irqrestore(&port->port_lock, flags);
		return;
	}

	d = &port->data_ch;

	/*
	 * Unlock the port here and not at the end of this work,
	 * because we do not want to activate usb_bam, ipa and
	 * tethe bridge logic in atomic context and wait uneeded time.
	 * Either way other works will not fire until end of this work
	 * and event functions (as bam_data_connect) will not influance
	 * while lower layers connect pipes, etc.
	*/
	spin_unlock_irqrestore(&port->port_lock, flags);

	ret = usb_bam_disconnect_ipa(d->usb_bam_type, &d->ipa_params);
	if (ret)
		pr_err("usb_bam_disconnect_ipa failed: err:%d\n", ret);
	usb_bam_free_fifos(d->usb_bam_type, d->src_connection_idx);
	usb_bam_free_fifos(d->usb_bam_type, d->dst_connection_idx);

	/*
	 * NOTE: it is required to disconnect USB and IPA BAM related pipes
	 * before calling IPA tethered function related disconnect API. IPA
	 * tethered function related disconnect API delete depedency graph
	 * with IPA RM which would results into IPA not pulling data although
	 * there is pending data on USB BAM producer pipe.
	 */
	bam_data_ipa_disconnect(d);
	spin_lock_irqsave(&port->port_lock, flags);
	port->is_ipa_connected = false;

	/*
	 * Decrement usage count which was incremented
	 * upon cable connect or cable disconnect in suspended state.
	 */
	usb_gadget_autopm_put_async(port->gadget);
	spin_unlock_irqrestore(&port->port_lock, flags);

	pr_debug("Disconnect workqueue done (port %pK)\n", port);
}
/*
 * This function configured data fifo based on index passed to get bam2bam
 * configuration.
 */
static void configure_usb_data_fifo(enum usb_ctrl bam_type,
		u8 idx, struct usb_ep *ep, enum usb_bam_pipe_type pipe_type)
{
	struct u_bam_data_connect_info bam_info;
	struct sps_mem_buffer data_fifo = {0};

	if (pipe_type == USB_BAM_PIPE_BAM2BAM) {
		get_bam2bam_connection_info(bam_type, idx,
					&bam_info.usb_bam_pipe_idx,
					NULL, &data_fifo, NULL);

		msm_data_fifo_config(ep,
					data_fifo.phys_base,
					data_fifo.size,
					bam_info.usb_bam_pipe_idx);
	}
}

/* Start RX transfers according to pipe_type */
static inline void bam_data_start_rx_transfers(struct bam_data_ch_info *d,
				struct bam_data_port *port)
{
	if (d->src_pipe_type == USB_BAM_PIPE_BAM2BAM)
		bam_data_start_endless_rx(port);
	else
		bam_data_start_rx(port);
}

static void bam2bam_data_connect_work(struct work_struct *w)
{
	struct bam_data_port *port = container_of(w, struct bam_data_port,
						  connect_w);
	struct teth_bridge_connect_params connect_params;
	struct teth_bridge_init_params teth_bridge_params;
	struct bam_data_ch_info *d;
	struct data_port	*d_port;
	struct usb_gadget	*gadget = NULL;
	u32			sps_params;
	int			ret;
	unsigned long		flags;

	pr_debug("%s: Connect workqueue started", __func__);

	spin_lock_irqsave(&port->port_lock, flags);

	d = &port->data_ch;
	d_port = port->port_usb;

	if (port->last_event == U_BAM_DATA_DISCONNECT_E) {
		pr_debug("%s: Port is about to disconnect. Bail out.\n",
			__func__);
		spin_unlock_irqrestore(&port->port_lock, flags);
		return;
	}

	if (d_port && d_port->cdev)
		gadget = d_port->cdev->gadget;

	if (!gadget) {
		pr_err("%s: NULL gadget\n", __func__);
		spin_unlock_irqrestore(&port->port_lock, flags);
		return;
	}

	if (!port->port_usb) {
		pr_err("port_usb is NULL");
		spin_unlock_irqrestore(&port->port_lock, flags);
		return;
	}

	if (!port->port_usb->out) {
		pr_err("port_usb->out (bulk out ep) is NULL");
		spin_unlock_irqrestore(&port->port_lock, flags);
		return;
	}

	/*
	 * check if connect_w got called two times during RNDIS resume as
	 * explicit flow control is called to start data transfers after
	 * bam_data_connect()
	 */
	if (port->is_ipa_connected) {
		pr_debug("IPA connect is already done & Transfers started\n");
		spin_unlock_irqrestore(&port->port_lock, flags);
		usb_gadget_autopm_put_async(port->gadget);
		return;
	}

	d->ipa_params.usb_connection_speed = gadget->speed;
	d->ipa_params.cons_clnt_hdl = -1;
	d->ipa_params.prod_clnt_hdl = -1;

	if (d->dst_pipe_type != USB_BAM_PIPE_BAM2BAM) {
		spin_unlock_irqrestore(&port->port_lock, flags);
		pr_err("%s: no software preparation for DL not using bam2bam\n",
				__func__);
		return;
	}

	spin_unlock_irqrestore(&port->port_lock, flags);

	usb_bam_alloc_fifos(d->usb_bam_type, d->src_connection_idx);
	usb_bam_alloc_fifos(d->usb_bam_type, d->dst_connection_idx);

	spin_lock_irqsave(&port->port_lock, flags);
	if (!port->port_usb) {
		pr_err("Disconnected.port_usb is NULL\n");
		spin_unlock_irqrestore(&port->port_lock, flags);
		goto free_fifos;
	}

	if (gadget_is_dwc3(gadget)) {
		/* Configure RX */
		configure_usb_data_fifo(d->usb_bam_type,
				d->src_connection_idx,
				port->port_usb->out, d->src_pipe_type);
		sps_params = MSM_SPS_MODE | MSM_DISABLE_WB
			| MSM_PRODUCER | d->src_pipe_idx;
		d->rx_req->length = 32*1024;
		d->rx_req->udc_priv = sps_params;
		msm_ep_config(port->port_usb->out, d->rx_req);

		/* Configure TX */
		configure_usb_data_fifo(d->usb_bam_type,
				d->dst_connection_idx,
				port->port_usb->in, d->dst_pipe_type);
		sps_params = MSM_SPS_MODE | MSM_DISABLE_WB
					| d->dst_pipe_idx;
		d->tx_req->length = 32*1024;
		d->tx_req->udc_priv = sps_params;
		msm_ep_config(port->port_usb->in, d->tx_req);

	} else {
		/* Configure RX */
		get_bam2bam_connection_info(d->usb_bam_type,
				d->src_connection_idx,
				&d->src_pipe_idx,
				NULL, NULL, NULL);
		sps_params = (SPS_PARAMS_SPS_MODE | d->src_pipe_idx |
			MSM_VENDOR_ID) & ~SPS_PARAMS_TBE;
		d->rx_req->udc_priv = sps_params;

		/* Configure TX */
		get_bam2bam_connection_info(d->usb_bam_type,
				d->dst_connection_idx,
				&d->dst_pipe_idx,
				NULL, NULL, NULL);
		sps_params = (SPS_PARAMS_SPS_MODE | d->dst_pipe_idx |
			MSM_VENDOR_ID) & ~SPS_PARAMS_TBE;
		d->tx_req->udc_priv = sps_params;
	}

	if (d->func_type == USB_FUNC_MBIM) {
		teth_bridge_params.client = d->ipa_params.src_client;
		ret = teth_bridge_init(&teth_bridge_params);
		if (ret) {
			spin_unlock_irqrestore(&port->port_lock, flags);
			pr_err("%s:teth_bridge_init() failed\n",
			      __func__);
			goto free_fifos;
		}
		d->ipa_params.notify =
			teth_bridge_params.usb_notify_cb;
		d->ipa_params.priv =
			teth_bridge_params.private_data;
		d->ipa_params.ipa_ep_cfg.mode.mode = IPA_BASIC;
		d->ipa_params.skip_ep_cfg =
			teth_bridge_params.skip_ep_cfg;
	}
	d->ipa_params.dir = USB_TO_PEER_PERIPHERAL;
	if (d->func_type == USB_FUNC_ECM) {
		d->ipa_params.notify = ecm_qc_get_ipa_rx_cb();
		d->ipa_params.priv = ecm_qc_get_ipa_priv();
		d->ipa_params.skip_ep_cfg = ecm_qc_get_skip_ep_config();
	}

	if (d->func_type == USB_FUNC_RNDIS) {
		d->ipa_params.notify = rndis_qc_get_ipa_rx_cb();
		d->ipa_params.priv = rndis_qc_get_ipa_priv();
		d->ipa_params.skip_ep_cfg =
			rndis_qc_get_skip_ep_config();
	}

	/* Support for UL using system-to-IPA */
	if (d->src_pipe_type == USB_BAM_PIPE_SYS2BAM) {
		d->ul_params.teth_cb = d->ipa_params.notify;
		d->ipa_params.notify =
			bam_data_ipa_sys2bam_notify_cb;
		d->ul_params.teth_priv = d->ipa_params.priv;
		d->ipa_params.priv = &d->ul_params;
		d->ipa_params.reset_pipe_after_lpm = false;
	} else {
		d->ipa_params.reset_pipe_after_lpm =
			(gadget_is_dwc3(gadget) &&
			msm_dwc3_reset_ep_after_lpm(gadget));
	}

	spin_unlock_irqrestore(&port->port_lock, flags);
	ret = usb_bam_connect_ipa(d->usb_bam_type, &d->ipa_params);
	if (ret) {
		pr_err("%s: usb_bam_connect_ipa failed: err:%d\n",
			__func__, ret);
		goto free_fifos;
	}

	spin_lock_irqsave(&port->port_lock, flags);
	if (port->last_event ==  U_BAM_DATA_DISCONNECT_E) {
		spin_unlock_irqrestore(&port->port_lock, flags);
		pr_err("%s:%d: Port is being disconnected.\n",
					__func__, __LINE__);
		goto disconnect_ipa;
	}

	d_port->ipa_consumer_ep = d->ipa_params.ipa_cons_ep_idx;

	/* Remove support for UL using system-to-IPA towards DL */
	if (d->src_pipe_type == USB_BAM_PIPE_SYS2BAM) {
		d->ipa_params.notify = d->ul_params.teth_cb;
		d->ipa_params.priv = d->ul_params.teth_priv;
	}

	d->ipa_params.dir = PEER_PERIPHERAL_TO_USB;
	if (d->func_type == USB_FUNC_ECM) {
		d->ipa_params.notify = ecm_qc_get_ipa_tx_cb();
		d->ipa_params.priv = ecm_qc_get_ipa_priv();
		d->ipa_params.skip_ep_cfg = ecm_qc_get_skip_ep_config();
	}
	if (d->func_type == USB_FUNC_RNDIS) {
		d->ipa_params.notify = rndis_qc_get_ipa_tx_cb();
		d->ipa_params.priv = rndis_qc_get_ipa_priv();
		d->ipa_params.skip_ep_cfg =
			rndis_qc_get_skip_ep_config();
	}

	if (d->dst_pipe_type == USB_BAM_PIPE_BAM2BAM) {
		d->ipa_params.reset_pipe_after_lpm =
			(gadget_is_dwc3(gadget) &&
			 msm_dwc3_reset_ep_after_lpm(gadget));
	} else {
		d->ipa_params.reset_pipe_after_lpm = false;
	}
	spin_unlock_irqrestore(&port->port_lock, flags);
	ret = usb_bam_connect_ipa(d->usb_bam_type, &d->ipa_params);
	if (ret) {
		pr_err("%s: usb_bam_connect_ipa failed: err:%d\n",
			__func__, ret);
		goto disconnect_ipa;
	}

	/*
	 * Cable might have been disconnected after releasing the
	 * spinlock and re-enabling IRQs. Hence check again.
	 */
	spin_lock_irqsave(&port->port_lock, flags);
	if (port->last_event ==  U_BAM_DATA_DISCONNECT_E) {
		spin_unlock_irqrestore(&port->port_lock, flags);
		pr_err("%s:%d: port is beind disconnected.\n",
					__func__, __LINE__);
		goto disconnect_ipa;
	}

	port->is_ipa_connected = true;

	d_port->ipa_producer_ep = d->ipa_params.ipa_prod_ep_idx;
	pr_debug("%s(): ipa_producer_ep:%d ipa_consumer_ep:%d\n",
			__func__, d_port->ipa_producer_ep,
			d_port->ipa_consumer_ep);
	spin_unlock_irqrestore(&port->port_lock, flags);

	if (d->func_type == USB_FUNC_MBIM) {
		connect_params.ipa_usb_pipe_hdl =
			d->ipa_params.prod_clnt_hdl;
		connect_params.usb_ipa_pipe_hdl =
			d->ipa_params.cons_clnt_hdl;
		connect_params.tethering_mode =
			TETH_TETHERING_MODE_MBIM;
		connect_params.client_type = d->ipa_params.src_client;
		ret = teth_bridge_connect(&connect_params);
		if (ret) {
			pr_err("%s:teth_bridge_connect() failed\n",
			      __func__);
			return;
		}
	}

	if (d->func_type == USB_FUNC_ECM) {
		ret = ecm_ipa_connect(d->ipa_params.cons_clnt_hdl,
			d->ipa_params.prod_clnt_hdl,
			d->ipa_params.priv);
		if (ret) {
			pr_err("%s: failed to connect IPA: err:%d\n",
				__func__, ret);
			return;
		}
	}

	if (d->func_type == USB_FUNC_RNDIS) {
		rndis_data.prod_clnt_hdl =
			d->ipa_params.prod_clnt_hdl;
		rndis_data.cons_clnt_hdl =
			d->ipa_params.cons_clnt_hdl;
		rndis_data.priv = d->ipa_params.priv;

		pr_debug("ul_max_transfer_size:%d\n",
				rndis_data.ul_max_transfer_size);
		pr_debug("ul_max_packets_number:%d\n",
				rndis_data.ul_max_packets_number);
		pr_debug("dl_max_transfer_size:%d\n",
				rndis_data.dl_max_transfer_size);

		ret = rndis_ipa_pipe_connect_notify(
			rndis_data.cons_clnt_hdl,
			rndis_data.prod_clnt_hdl,
			rndis_data.ul_max_transfer_size,
			rndis_data.ul_max_packets_number,
			rndis_data.dl_max_transfer_size,
			rndis_data.priv);
		if (ret) {
			pr_err("%s: failed to connect IPA: err:%d\n",
				__func__, ret);
			return;
		}
	}
	atomic_set(&d->pipe_connect_notified, 1);

	/* Don't queue the transfers yet, only after network stack is up */
	if (d->func_type == USB_FUNC_RNDIS || d->func_type == USB_FUNC_ECM) {
		pr_debug("%s: Not starting now, waiting for network notify",
			__func__);
		return;
	}

	/* queue in & out requests */
	bam_data_start_rx_transfers(d, port);
	bam_data_start_endless_tx(port);

	pr_debug("Connect workqueue done (port %pK)", port);
	return;

disconnect_ipa:
	/* let disconnect work take care of ipa disconnect */
	port->is_ipa_connected = true;
	return;

free_fifos:
	usb_bam_free_fifos(d->usb_bam_type, d->src_connection_idx);
	usb_bam_free_fifos(d->usb_bam_type, d->dst_connection_idx);
}

/*
 * Called when IPA triggers us that the network interface is up.
 *  Starts the transfers on bulk endpoints.
 * (optimization reasons, the pipes and bam with IPA are already connected)
 */
void bam_data_start_rx_tx(u8 port_num)
{
	struct bam_data_port	*port;
	struct bam_data_ch_info	*d;
	unsigned long flags;

	pr_debug("%s: Triggered: starting tx, rx", __func__);

	/* queue in & out requests */
	port = bam2bam_data_ports[port_num];
	if (!port) {
		pr_err("%s: port is NULL, can't start tx, rx", __func__);
		return;
	}

	spin_lock_irqsave(&port->port_lock, flags);
	d = &port->data_ch;

	if (!port->port_usb || !port->port_usb->in->driver_data
		|| !port->port_usb->out->driver_data) {
		pr_err("%s: Can't start tx, rx, ep not enabled", __func__);
		goto out;
	}

	if (!d->rx_req || !d->tx_req) {
		pr_err("%s: No request d->rx_req=%pK, d->tx_req=%pK", __func__,
			d->rx_req, d->tx_req);
		goto out;
	}
	if (!port->is_ipa_connected) {
		pr_debug("%s: pipes are disconnected", __func__);
		goto out;
	}

	spin_unlock_irqrestore(&port->port_lock, flags);

	/* queue in & out requests */
	pr_debug("%s: Starting rx", __func__);
	bam_data_start_rx_transfers(d, port);

	pr_debug("%s: Starting tx", __func__);
	bam_data_start_endless_tx(port);

	return;
out:
	spin_unlock_irqrestore(&port->port_lock, flags);
}

inline int u_bam_data_func_to_port(enum function_type func, u8 func_port)
{
	if (func >= USB_NUM_FUNCS || func_port >= PORTS_PER_FUNC) {
		pr_err("func=%d and func_port=%d are an illegal combination\n",
			func, func_port);
		return -EINVAL;
	}
	return (PORTS_PER_FUNC * func) + func_port;
}

static int bam2bam_data_port_alloc(int portno)
{
	struct bam_data_port    *port;
	struct bam_data_ch_info *d;

	if (bam2bam_data_ports[portno] != NULL) {
		pr_debug("port %d already allocated.\n", portno);
		return 0;
	}

	port = kzalloc(sizeof(struct bam_data_port), GFP_KERNEL);
	if (!port) {
		pr_err("no memory to allocate port %d\n", portno);
		return -ENOMEM;
	}

	bam2bam_data_ports[portno] = port;
	d = &port->data_ch;
	d->port = port;

	spin_lock_init(&port->port_lock);

	INIT_WORK(&port->connect_w, bam2bam_data_connect_work);
	INIT_WORK(&port->disconnect_w, bam2bam_data_disconnect_work);
	INIT_WORK(&port->suspend_w, bam2bam_data_suspend_work);
	INIT_WORK(&port->resume_w, bam2bam_data_resume_work);
	INIT_WORK(&d->write_tobam_w, bam_data_write_toipa);
	return 0;
}

void u_bam_data_start_rndis_ipa(void)
{
	int port_num;
	struct bam_data_port *port;
	struct bam_data_ch_info *d;

	pr_debug("%s\n", __func__);

	port_num = u_bam_data_func_to_port(USB_FUNC_RNDIS,
					RNDIS_QC_ACTIVE_PORT);
	port = bam2bam_data_ports[port_num];
	if (!port) {
		pr_err("%s: port is NULL", __func__);
		return;
	}

	d = &port->data_ch;

	if (!atomic_read(&d->pipe_connect_notified)) {
		/*
		 * Increment usage count upon cable connect. Decrement after IPA
		 * handshake is done in disconnect work due to cable disconnect
		 * or in suspend work.
		 */
		usb_gadget_autopm_get_noresume(port->gadget);
		queue_work(bam_data_wq, &port->connect_w);
	} else {
		pr_debug("%s: Transfers already started?\n", __func__);
	}
}

void u_bam_data_stop_rndis_ipa(void)
{
	int port_num;
	struct bam_data_port *port;
	struct bam_data_ch_info *d;
	unsigned long flags;

	pr_debug("%s\n", __func__);

	port_num = u_bam_data_func_to_port(USB_FUNC_RNDIS,
					RNDIS_QC_ACTIVE_PORT);
	port = bam2bam_data_ports[port_num];
	if (!port) {
		pr_err("%s: port is NULL", __func__);
		return;
	}

	d = &port->data_ch;

	if (atomic_read(&d->pipe_connect_notified)) {
		rndis_ipa_reset_trigger();
		bam_data_stop_endless_tx(port);
		bam_data_stop_endless_rx(port);
		if (gadget_is_dwc3(port->gadget)) {
			spin_lock_irqsave(&port->port_lock, flags);
			/* check if USB cable is disconnected or not */
			if (port->port_usb) {
				msm_ep_unconfig(port->port_usb->in);
				msm_ep_unconfig(port->port_usb->out);
			}
			spin_unlock_irqrestore(&port->port_lock, flags);
		}
		queue_work(bam_data_wq, &port->disconnect_w);
	}
}

void bam_data_flow_control_enable(bool enable)
{
	if (enable)
		u_bam_data_stop_rndis_ipa();
	else
		u_bam_data_start_rndis_ipa();
}

static void bam_data_free_reqs(struct bam_data_port *port)
{

	struct list_head *head;
	struct usb_request *req;

	if (port->data_ch.src_pipe_type != USB_BAM_PIPE_SYS2BAM)
		return;

	head = &port->data_ch.rx_idle;

	while (!list_empty(head)) {
		req = list_entry(head->next, struct usb_request, list);
		list_del(&req->list);
		usb_ep_free_request(port->port_usb->out, req);
		port->data_ch.freed_rx_reqs++;
	}
}

void bam_data_disconnect(struct data_port *gr, enum function_type func,
		u8 dev_port_num)
{
	struct bam_data_port *port;
	struct bam_data_ch_info	*d;
	struct sk_buff *skb = NULL;
	unsigned long flags;
	int port_num;

	port_num = u_bam_data_func_to_port(func, dev_port_num);
	if (port_num < 0) {
		pr_err("invalid bam2bam portno#%d\n", port_num);
		return;
	}

	pr_debug("dev:%pK port number:%d\n", gr, port_num);

	if (!gr) {
		pr_err("data port is null\n");
		return;
	}

	port = bam2bam_data_ports[port_num];

	if (!port) {
		pr_err("port %u is NULL", port_num);
		return;
	}

	spin_lock_irqsave(&port->port_lock, flags);

	d = &port->data_ch;

	/* Already disconnected due to suspend with remote wake disabled */
	if (port->last_event == U_BAM_DATA_DISCONNECT_E) {
		spin_unlock_irqrestore(&port->port_lock, flags);
		return;
	}

	/*
	 * Suspend with remote wakeup enabled. Increment usage
	 * count when disconnect happens in suspended state.
	 * Corresponding decrement happens in the end of this
	 * function if IPA handshake is already done or it is done
	 * in disconnect work after finishing IPA handshake.
	 * In case of RNDIS, if connect_w by rndis_flow_control is not triggered
	 * yet then don't perform pm_runtime_get as suspend_w would have bailed
	 * w/o runtime_get.
	 * And restrict check to only RNDIS to handle cases where connect_w is
	 * already scheduled but execution is pending which must be rare though.
	 */
	if (port->last_event == U_BAM_DATA_SUSPEND_E &&
		     (d->func_type != USB_FUNC_RNDIS || port->is_ipa_connected))
		usb_gadget_autopm_get_noresume(port->gadget);

	if (port->port_usb) {
		port->port_usb->ipa_consumer_ep = -1;
		port->port_usb->ipa_producer_ep = -1;

		if (port->port_usb->in && port->port_usb->in->driver_data) {

			/*
			 * Disable endpoints.
			 * Unlocking is needed since disabling the eps might
			 * stop active transfers and therefore the request
			 * complete function will be called, where we try
			 * to obtain the spinlock as well.
			 */
			spin_unlock_irqrestore(&port->port_lock, flags);
			usb_ep_disable(port->port_usb->in);
			if (d->tx_req) {
				usb_ep_free_request(port->port_usb->in,
								d->tx_req);
				d->tx_req = NULL;
			}

			usb_ep_disable(port->port_usb->out);
			if (d->rx_req) {
				usb_ep_free_request(port->port_usb->out,
								d->rx_req);
				d->rx_req = NULL;
			}

			spin_lock_irqsave(&port->port_lock, flags);

			/* Only for SYS2BAM mode related UL workaround */
			if (d->src_pipe_type == USB_BAM_PIPE_SYS2BAM) {

				pr_debug("SKBs_RX_Q: freed:%d\n",
							d->rx_skb_q.qlen);
				while ((skb = __skb_dequeue(&d->rx_skb_q)))
					dev_kfree_skb_any(skb);

				bam2bam_free_rx_skb_idle_list(port);
				pr_debug("SKBs: allocated:%d freed:%d\n",
						d->total_skb, d->freed_skb);
				pr_debug("rx_reqs: allocated:%d freed:%d\n",
					d->alloc_rx_reqs, d->freed_rx_reqs);

				/* reset all skb/reqs related statistics */
				d->total_skb = 0;
				d->freed_skb = 0;
				d->freed_rx_reqs = 0;
				d->alloc_rx_reqs = 0;
			}

			/*
			 * Set endless flag to false as USB Endpoint
			 * is already disable.
			 */
			if (d->dst_pipe_type == USB_BAM_PIPE_BAM2BAM)
				port->port_usb->in->endless = false;

			if (d->src_pipe_type == USB_BAM_PIPE_BAM2BAM)
				port->port_usb->out->endless = false;

			port->port_usb->in->driver_data = NULL;
			port->port_usb->out->driver_data = NULL;

			port->port_usb = NULL;
		}
	}

	port->last_event = U_BAM_DATA_DISCONNECT_E;
	/* Disable usb irq for CI gadget. It will be enabled in
	 * usb_bam_disconnect_pipe() after disconnecting all pipes
	 * and USB BAM reset is done.
	 */
	if (!gadget_is_dwc3(port->gadget))
		msm_usb_irq_disable(true);

	queue_work(bam_data_wq, &port->disconnect_w);

	spin_unlock_irqrestore(&port->port_lock, flags);
}

int bam_data_connect(struct data_port *gr, enum transport_type trans,
		u8 dev_port_num, enum function_type func)
{
	struct bam_data_port	*port;
	struct bam_data_ch_info	*d;
	int			ret, port_num;
	unsigned long		flags;
	u8			src_connection_idx, dst_connection_idx;
	enum usb_ctrl		usb_bam_type;

	if (!gr) {
		pr_err("data port is null\n");
		return -ENODEV;
	}

	port_num = u_bam_data_func_to_port(func, dev_port_num);
	if (port_num < 0) {
		pr_err("invalid portno#%d\n", port_num);
		return -EINVAL;
	}

	if (trans != USB_GADGET_XPORT_BAM2BAM_IPA) {
		pr_err("invalid xport#%d\n", trans);
		return -EINVAL;
	}

	pr_debug("dev:%pK port#%d\n", gr, port_num);

	usb_bam_type = usb_bam_get_bam_type(gr->cdev->gadget->name);

	src_connection_idx = usb_bam_get_connection_idx(usb_bam_type,
			IPA_P_BAM, USB_TO_PEER_PERIPHERAL, USB_BAM_DEVICE,
			dev_port_num);
	dst_connection_idx = usb_bam_get_connection_idx(usb_bam_type,
			IPA_P_BAM, PEER_PERIPHERAL_TO_USB, USB_BAM_DEVICE,
			dev_port_num);
	if (src_connection_idx < 0 || dst_connection_idx < 0) {
		pr_err("%s: usb_bam_get_connection_idx failed\n", __func__);
		return ret;
	}

	port = bam2bam_data_ports[port_num];

	spin_lock_irqsave(&port->port_lock, flags);

	port->port_usb = gr;
	port->gadget = gr->cdev->gadget;
	d = &port->data_ch;
	d->src_connection_idx = src_connection_idx;
	d->dst_connection_idx = dst_connection_idx;
	d->usb_bam_type = usb_bam_type;

	d->trans = trans;
	d->func_type = func;
	d->rx_buffer_size = (gr->rx_buffer_size ? gr->rx_buffer_size :
					bam_mux_rx_req_size);

	if (usb_bam_type == HSIC_CTRL) {
		d->ipa_params.src_client = IPA_CLIENT_HSIC1_PROD;
		d->ipa_params.dst_client = IPA_CLIENT_HSIC1_CONS;
	} else {
		d->ipa_params.src_client = IPA_CLIENT_USB_PROD;
		d->ipa_params.dst_client = IPA_CLIENT_USB_CONS;
	}

	pr_debug("%s(): rx_buffer_size:%d\n", __func__, d->rx_buffer_size);
	d->ipa_params.src_pipe = &(d->src_pipe_idx);
	d->ipa_params.dst_pipe = &(d->dst_pipe_idx);
	d->ipa_params.src_idx = src_connection_idx;
	d->ipa_params.dst_idx = dst_connection_idx;
	d->rx_flow_control_disable = 0;
	d->rx_flow_control_enable = 0;
	d->rx_flow_control_triggered = 0;

	/*
	 * Query pipe type using IPA src/dst index with
	 * usbbam driver. It is being set either as
	 * BAM2BAM or SYS2BAM.
	 */
	if (usb_bam_get_pipe_type(usb_bam_type, d->ipa_params.src_idx,
				  &d->src_pipe_type) ||
	    usb_bam_get_pipe_type(usb_bam_type, d->ipa_params.dst_idx,
				  &d->dst_pipe_type)) {
		pr_err("usb_bam_get_pipe_type() failed\n");
		ret = -EINVAL;
		goto exit;
	}

	/*
	 * Check for pipe_type. If it is BAM2BAM, then it is required
	 * to disable Xfer complete and Xfer not ready interrupts for
	 * that particular endpoint. Hence it set endless flag based
	 * it which is considered into UDC driver while enabling
	 * USB Endpoint.
	 */
	if (d->dst_pipe_type == USB_BAM_PIPE_BAM2BAM)
		port->port_usb->in->endless = true;

	if (d->src_pipe_type == USB_BAM_PIPE_BAM2BAM)
		port->port_usb->out->endless = true;

	ret = usb_ep_enable(gr->in);
	if (ret) {
		pr_err("usb_ep_enable failed eptype:IN ep:%pK", gr->in);
		goto exit;
	}

	gr->in->driver_data = port;

	ret = usb_ep_enable(gr->out);
	if (ret) {
		pr_err("usb_ep_enable failed eptype:OUT ep:%pK", gr->out);
		goto disable_in_ep;
	}

	gr->out->driver_data = port;

	if (d->src_pipe_type == USB_BAM_PIPE_SYS2BAM) {

		/* UL workaround requirements */
		skb_queue_head_init(&d->rx_skb_q);
		skb_queue_head_init(&d->rx_skb_idle);
		INIT_LIST_HEAD(&d->rx_idle);

		ret = bam_data_sys2bam_alloc_req(port, false);
		if (ret) {
			pr_err("%s: sys2bam_alloc_req failed(%d)",
							__func__, ret);
			goto disable_out_ep;
		}
	}

	d->rx_req = usb_ep_alloc_request(port->port_usb->out,
							GFP_ATOMIC);
	if (!d->rx_req) {
		pr_err("%s: failed to allocate rx_req\n", __func__);
		goto bam_data_free;
	}
	d->rx_req->context = port;
	d->rx_req->complete = bam_data_endless_rx_complete;
	d->rx_req->length = 0;
	d->rx_req->no_interrupt = 1;

	d->tx_req = usb_ep_alloc_request(port->port_usb->in,
							GFP_ATOMIC);
	if (!d->tx_req) {
		pr_err("%s: failed to allocate tx_req\n", __func__);
		goto ep_out_req_free;
	}

	d->tx_req->context = port;
	d->tx_req->complete = bam_data_endless_tx_complete;
	d->tx_req->length = 0;
	d->tx_req->no_interrupt = 1;

	gr->out->driver_data = port;

	port->last_event = U_BAM_DATA_CONNECT_E;

	/* Wait for host to enable flow_control */
	if (d->func_type == USB_FUNC_RNDIS) {
		ret = 0;
		goto exit;
	}

	/*
	 * Increment usage count upon cable connect. Decrement after IPA
	 * handshake is done in disconnect work (due to cable disconnect)
	 * or in suspend work.
	 */
	usb_gadget_autopm_get_noresume(port->gadget);

	queue_work(bam_data_wq, &port->connect_w);
	spin_unlock_irqrestore(&port->port_lock, flags);
	return 0;

ep_out_req_free:
	usb_ep_free_request(port->port_usb->out, d->rx_req);
bam_data_free:
	bam_data_free_reqs(port);
disable_out_ep:
	gr->out->driver_data = 0;
	usb_ep_disable(gr->out);
disable_in_ep:
	gr->in->driver_data = 0;
	usb_ep_disable(gr->in);
exit:
	spin_unlock_irqrestore(&port->port_lock, flags);
	return ret;
}

int bam_data_setup(enum function_type func, unsigned int no_bam2bam_port)
{
	int	i;
	int	ret;

	pr_debug("requested %d BAM2BAM ports", no_bam2bam_port);

	if (!no_bam2bam_port || no_bam2bam_port > PORTS_PER_FUNC ||
		func >= USB_NUM_FUNCS) {
		pr_err("Invalid num of ports count:%d or function type:%d\n",
			no_bam2bam_port, func);
		return -EINVAL;
	}

	for (i = 0; i < no_bam2bam_port; i++) {
		n_bam2bam_data_ports++;
		ret = bam2bam_data_port_alloc(u_bam_data_func_to_port(func, i));
		if (ret) {
			n_bam2bam_data_ports--;
			pr_err("Failed to alloc port:%d\n", i);
			goto free_bam_ports;
		}
	}

	pr_debug("n_bam2bam_data_ports:%d\n", n_bam2bam_data_ports);

	if (bam_data_wq) {
		pr_debug("bam_data is already setup.");
		return 0;
	}

	bam_data_wq = alloc_workqueue("k_bam_data",
				WQ_UNBOUND | WQ_MEM_RECLAIM, 1);
	if (!bam_data_wq) {
		pr_err("Failed to create workqueue\n");
		ret = -ENOMEM;
		goto free_bam_ports;
	}

	return 0;

free_bam_ports:
	for (i = 0; i < n_bam2bam_data_ports; i++) {
		kfree(bam2bam_data_ports[i]);
		bam2bam_data_ports[i] = NULL;
		if (bam_data_wq) {
			destroy_workqueue(bam_data_wq);
			bam_data_wq = NULL;
		}
	}

	return ret;
}

static int bam_data_wake_cb(void *param)
{
	int ret;
	struct bam_data_port *port = (struct bam_data_port *)param;
	struct data_port *d_port = port->port_usb;
	struct usb_gadget *gadget;
	struct usb_function *func;

	pr_debug("%s: woken up by peer\n", __func__);

	if (!d_port) {
		pr_err("FAILED: d_port == NULL");
		return -ENODEV;
	}

	if (!d_port->cdev) {
		pr_err("FAILED: d_port->cdev == NULL");
		return -ENODEV;
	}

	gadget = d_port->cdev->gadget;
	if (!gadget) {
		pr_err("FAILED: d_port->cdev->gadget == NULL");
		return -ENODEV;
	}

	func = d_port->func;

	/*
	 * In Super-Speed mode, remote wakeup is not allowed for suspended
	 * functions which have been disallowed by the host to issue Funtion
	 * Remote Wakeup.
	 * Note - We deviate here from the USB 3.0 spec and allow
	 * non-suspended functions to issue remote-wakeup even if they were not
	 * allowed to do so by the host. This is done in order to support non
	 * fully USB 3.0 compatible hosts.
	 */
	if ((gadget->speed == USB_SPEED_SUPER) && (func->func_is_suspended))
		ret = usb_func_wakeup(func);
	else
		ret = usb_gadget_wakeup(gadget);

	if ((ret == -EBUSY) || (ret == -EAGAIN))
		pr_debug("Remote wakeup is delayed due to LPM exit.\n");
	else if (ret)
		pr_err("Failed to wake up the USB core. ret=%d.\n", ret);

	return ret;
}

static void bam_data_start(void *param, enum usb_bam_pipe_dir dir)
{
	struct bam_data_port *port = param;
	struct data_port *d_port = port->port_usb;
	struct bam_data_ch_info *d = &port->data_ch;
	struct usb_gadget *gadget;

	if (!d_port || !d_port->cdev || !d_port->cdev->gadget) {
		pr_err("%s:d_port,cdev or gadget is  NULL\n", __func__);
		return;
	}
	if (port->last_event != U_BAM_DATA_RESUME_E) {
		pr_err("%s: Port state changed since resume. Bail out.\n",
			__func__);
		return;
	}

	gadget = d_port->cdev->gadget;

	if (dir == USB_TO_PEER_PERIPHERAL) {
		if (port->data_ch.src_pipe_type == USB_BAM_PIPE_BAM2BAM)
			bam_data_start_endless_rx(port);
		else {
			bam_data_start_rx(port);
			queue_work(bam_data_wq, &d->write_tobam_w);
		}
	} else {
		if (gadget_is_dwc3(gadget) &&
		    msm_dwc3_reset_ep_after_lpm(gadget)) {
			configure_data_fifo(d->usb_bam_type,
				d->dst_connection_idx,
				port->port_usb->in, d->dst_pipe_type);
		}
		bam_data_start_endless_tx(port);
	}

}

static void bam_data_stop(void *param, enum usb_bam_pipe_dir dir)
{
	struct bam_data_port *port = param;

	if (dir == USB_TO_PEER_PERIPHERAL) {
		/*
		 * Only handling BAM2BAM, as there is no equivelant to
		 * bam_data_stop_endless_rx() for the SYS2BAM use case
		 */
		if (port->data_ch.src_pipe_type == USB_BAM_PIPE_BAM2BAM)
			bam_data_stop_endless_rx(port);
	} else {
		bam_data_stop_endless_tx(port);
	}
}

void bam_data_suspend(struct data_port *port_usb, u8 dev_port_num,
		enum function_type func, bool remote_wakeup_enabled)
{
	struct bam_data_port *port;
	unsigned long flags;
	int port_num;

	port_num = u_bam_data_func_to_port(func, dev_port_num);
	if (port_num < 0) {
		pr_err("invalid bam2bam portno#%d\n", port_num);
		return;
	}

	pr_debug("%s: suspended port %d\n", __func__, port_num);

	port = bam2bam_data_ports[port_num];
	if (!port) {
		pr_err("%s(): Port is NULL.\n", __func__);
		return;
	}

	/* suspend with remote wakeup disabled */
	if (!remote_wakeup_enabled) {
		/*
		 * When remote wakeup is disabled, IPA BAM is disconnected
		 * because it cannot send new data until the USB bus is resumed.
		 * Endpoint descriptors info is saved before it gets reset by
		 * the BAM disconnect API. This lets us restore this info when
		 * the USB bus is resumed.
		 */
		port_usb->in_ep_desc_backup = port_usb->in->desc;
		port_usb->out_ep_desc_backup = port_usb->out->desc;

		pr_debug("in_ep_desc_backup = %pK, out_ep_desc_backup = %pK",
			port_usb->in_ep_desc_backup,
			port_usb->out_ep_desc_backup);

		bam_data_disconnect(port_usb, func, dev_port_num);
		return;
	}

	spin_lock_irqsave(&port->port_lock, flags);
	port->last_event = U_BAM_DATA_SUSPEND_E;
	queue_work(bam_data_wq, &port->suspend_w);
	spin_unlock_irqrestore(&port->port_lock, flags);
}

void bam_data_resume(struct data_port *port_usb, u8 dev_port_num,
		enum function_type func, bool remote_wakeup_enabled)
{
	struct bam_data_port *port;
	unsigned long flags;
	int port_num;

	port_num = u_bam_data_func_to_port(func, dev_port_num);
	if (port_num < 0) {
		pr_err("invalid bam2bam portno#%d\n", port_num);
		return;
	}

	pr_debug("%s: resumed port %d\n", __func__, port_num);

	port = bam2bam_data_ports[port_num];
	if (!port) {
		pr_err("%s(): Port is NULL.\n", __func__);
		return;
	}

	/* resume with remote wakeup disabled */
	if (!remote_wakeup_enabled) {
		/* Restore endpoint descriptors info. */
		port_usb->in->desc = port_usb->in_ep_desc_backup;
		port_usb->out->desc = port_usb->out_ep_desc_backup;

		pr_debug("in_ep_desc_backup = %pK, out_ep_desc_backup = %pK",
			port_usb->in_ep_desc_backup,
			port_usb->out_ep_desc_backup);

		bam_data_connect(port_usb, port->data_ch.trans,
			dev_port_num, func);
		return;
	}

	spin_lock_irqsave(&port->port_lock, flags);
	port->last_event = U_BAM_DATA_RESUME_E;

	/*
	 * Increment usage count here to disallow gadget
	 * parent suspend. This counter will decrement
	 * after IPA handshake is done in disconnect work
	 * (due to cable disconnect) or in bam_data_disconnect
	 * in suspended state.
	 */
	usb_gadget_autopm_get_noresume(port->gadget);
	queue_work(bam_data_wq, &port->resume_w);
	spin_unlock_irqrestore(&port->port_lock, flags);
}

void bam_data_flush_workqueue(void)
{
	pr_debug("%s(): Flushing workqueue\n", __func__);
	flush_workqueue(bam_data_wq);
}

static void bam2bam_data_suspend_work(struct work_struct *w)
{
	struct bam_data_port *port =
			container_of(w, struct bam_data_port, suspend_w);
	struct bam_data_ch_info *d;
	int ret;
	unsigned long flags;

	pr_debug("%s: suspend work started\n", __func__);

	spin_lock_irqsave(&port->port_lock, flags);

	d = &port->data_ch;

	/* In case of RNDIS, host enables flow_control invoking connect_w. If it
	 * is delayed then we may end up having suspend_w run before connect_w.
	 * In this scenario, connect_w may or may not at all start if cable gets
	 * disconnected or if host changes configuration e.g. RNDIS --> MBIM
	 * For these cases don't do runtime_put as there was no _get yet, and
	 * detect this condition on disconnect to not do extra pm_runtme_get
	 * for SUSPEND --> DISCONNECT scenario.
	 */
	if (!port->is_ipa_connected) {
		pr_err("%s: Not yet connected. SUSPEND pending.\n", __func__);
		spin_unlock_irqrestore(&port->port_lock, flags);
		return;
	}

	if ((port->last_event == U_BAM_DATA_DISCONNECT_E) ||
	    (port->last_event == U_BAM_DATA_RESUME_E)) {
		pr_debug("%s: Port is about to disconnect/resume. Bail out.\n",
			__func__);
		goto exit;
	}

	ret = usb_bam_register_wake_cb(d->usb_bam_type, d->dst_connection_idx,
					bam_data_wake_cb, port);
	if (ret) {
		pr_err("%s(): Failed to register BAM wake callback.\n",
			__func__);
		goto exit;
	}

	usb_bam_register_start_stop_cbs(d->usb_bam_type, d->dst_connection_idx,
					bam_data_start, bam_data_stop,
					port);

	/*
	 * release lock here because bam_data_start() or
	 * bam_data_stop() called from usb_bam_suspend()
	 * re-acquires port lock.
	 */
	spin_unlock_irqrestore(&port->port_lock, flags);
	usb_bam_suspend(d->usb_bam_type, &d->ipa_params);
	spin_lock_irqsave(&port->port_lock, flags);

exit:
	/*
	 * Decrement usage count after IPA handshake is done
	 * to allow gadget parent to go to lpm. This counter was
	 * incremented upon cable connect.
	 */
	usb_gadget_autopm_put_async(port->gadget);

	spin_unlock_irqrestore(&port->port_lock, flags);
}

static void bam2bam_data_resume_work(struct work_struct *w)
{
	struct bam_data_port *port =
			container_of(w, struct bam_data_port, resume_w);
	struct bam_data_ch_info *d;
	struct data_port *d_port;
	struct usb_gadget *gadget;
	int ret;
	unsigned long flags;

	spin_lock_irqsave(&port->port_lock, flags);

	if (!port->port_usb) {
		pr_err("port->port_usb is NULL");
		goto exit;
	}

	if (!port->port_usb->cdev) {
		pr_err("!port->port_usb->cdev is NULL");
		goto exit;
	}

	if (!port->port_usb->cdev->gadget) {
		pr_err("!port->port_usb->cdev->gadget is NULL");
		goto exit;
	}

	d = &port->data_ch;
	d_port = port->port_usb;
	gadget = d_port->cdev->gadget;

	pr_debug("%s: resume work started\n", __func__);

	if (port->last_event == U_BAM_DATA_DISCONNECT_E) {
		pr_debug("%s: Port is about to disconnect. Bail out.\n",
			__func__);
		goto exit;
	}

	ret = usb_bam_register_wake_cb(d->usb_bam_type, d->dst_connection_idx,
					NULL, NULL);
	if (ret) {
		pr_err("%s(): Failed to un-register BAM wake callback.\n",
			__func__);
		goto exit;
	}

	/*
	 * If usb_req was dequeued as part of bus suspend then
	 * corresponding DBM IN and OUT EPs should also be reset.
	 * There is a possbility that usb_bam may not have dequeued the
	 * request in case of quick back to back usb bus suspend resume.
	 */
	if (gadget_is_dwc3(gadget) &&
		msm_dwc3_reset_ep_after_lpm(gadget)) {
		if (d->tx_req_dequeued) {
			configure_usb_data_fifo(d->usb_bam_type,
				d->dst_connection_idx,
				port->port_usb->in, d->dst_pipe_type);
			spin_unlock_irqrestore(&port->port_lock, flags);
			msm_dwc3_reset_dbm_ep(port->port_usb->in);
			spin_lock_irqsave(&port->port_lock, flags);
		}
		if (d->rx_req_dequeued) {
			configure_usb_data_fifo(d->usb_bam_type,
				d->src_connection_idx,
				port->port_usb->out, d->src_pipe_type);
			spin_unlock_irqrestore(&port->port_lock, flags);
			msm_dwc3_reset_dbm_ep(port->port_usb->out);
			spin_lock_irqsave(&port->port_lock, flags);
		}
	}
	d->tx_req_dequeued = false;
	d->rx_req_dequeued = false;
	usb_bam_resume(d->usb_bam_type, &d->ipa_params);
exit:
	spin_unlock_irqrestore(&port->port_lock, flags);
}

void u_bam_data_set_dl_max_xfer_size(u32 max_transfer_size)
{

	if (!max_transfer_size) {
		pr_err("%s: invalid parameters\n", __func__);
		return;
	}
	rndis_data.dl_max_transfer_size = max_transfer_size;
	pr_debug("%s(): dl_max_xfer_size:%d\n", __func__, max_transfer_size);
}

void u_bam_data_set_ul_max_pkt_num(u8 max_packets_number)

{
	if (!max_packets_number) {
		pr_err("%s: invalid parameters\n", __func__);
		return;
	}

	rndis_data.ul_max_packets_number = max_packets_number;

	if (max_packets_number > 1)
		rndis_data.ul_aggregation_enable = true;
	else
		rndis_data.ul_aggregation_enable = false;

	pr_debug("%s(): ul_aggregation enable:%d\n", __func__,
				rndis_data.ul_aggregation_enable);
	pr_debug("%s(): ul_max_packets_number:%d\n", __func__,
				max_packets_number);
}

void u_bam_data_set_ul_max_xfer_size(u32 max_transfer_size)
{
	if (!max_transfer_size) {
		pr_err("%s: invalid parameters\n", __func__);
		return;
	}
	rndis_data.ul_max_transfer_size = max_transfer_size;
	pr_debug("%s(): ul_max_xfer_size:%d\n", __func__, max_transfer_size);
}
