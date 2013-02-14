/* Copyright (c) 2012, The Linux Foundation. All rights reserved.
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
#include <linux/smux.h>

#include <mach/usb_gadget_xport.h>

static unsigned int num_data_ports;

static const char *ghsuart_data_names[] = {
	"SMUX_DUN_DATA_HSUART",
	"SMUX_RMNET_DATA_HSUART"
};

#define DATA_BRIDGE_NAME_MAX_LEN		20

#define GHSUART_DATA_RMNET_RX_Q_SIZE		10
#define GHSUART_DATA_RMNET_TX_Q_SIZE		20
#define GHSUART_DATA_SERIAL_RX_Q_SIZE		5
#define GHSUART_DATA_SERIAL_TX_Q_SIZE		5
#define GHSUART_DATA_RX_REQ_SIZE		2048
#define GHSUART_DATA_TX_INTR_THRESHOLD		1

/* from cdc-acm.h */
#define ACM_CTRL_RTS		(1 << 1)	/* unused with full duplex */
#define ACM_CTRL_DTR		(1 << 0)	/* host is ready for data r/w */
#define ACM_CTRL_OVERRUN	(1 << 6)
#define ACM_CTRL_PARITY		(1 << 5)
#define ACM_CTRL_FRAMING	(1 << 4)
#define ACM_CTRL_RI		(1 << 3)
#define ACM_CTRL_BRK		(1 << 2)
#define ACM_CTRL_DSR		(1 << 1)
#define ACM_CTRL_DCD		(1 << 0)

static unsigned int ghsuart_data_rmnet_tx_q_size = GHSUART_DATA_RMNET_TX_Q_SIZE;
module_param(ghsuart_data_rmnet_tx_q_size, uint, S_IRUGO | S_IWUSR);

static unsigned int ghsuart_data_rmnet_rx_q_size = GHSUART_DATA_RMNET_RX_Q_SIZE;
module_param(ghsuart_data_rmnet_rx_q_size, uint, S_IRUGO | S_IWUSR);

static unsigned int ghsuart_data_serial_tx_q_size =
					GHSUART_DATA_SERIAL_TX_Q_SIZE;
module_param(ghsuart_data_serial_tx_q_size, uint, S_IRUGO | S_IWUSR);

static unsigned int ghsuart_data_serial_rx_q_size =
				GHSUART_DATA_SERIAL_RX_Q_SIZE;
module_param(ghsuart_data_serial_rx_q_size, uint, S_IRUGO | S_IWUSR);

static unsigned int ghsuart_data_rx_req_size = GHSUART_DATA_RX_REQ_SIZE;
module_param(ghsuart_data_rx_req_size, uint, S_IRUGO | S_IWUSR);

unsigned int ghsuart_data_tx_intr_thld = GHSUART_DATA_TX_INTR_THRESHOLD;
module_param(ghsuart_data_tx_intr_thld, uint, S_IRUGO | S_IWUSR);

#define CH_OPENED 0
#define CH_READY 1

struct ghsuart_data_port {
	/* port */
	unsigned		port_num;

	/* gadget */
	atomic_t		connected;
	struct usb_ep		*in;
	struct usb_ep		*out;

	enum gadget_type	gtype;
	spinlock_t		port_lock;
	void *port_usb;

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
	void *ctx;
	unsigned int ch_id;
	/* flow control bits */
	unsigned long flags;
	/* channel status */
	unsigned long		channel_sts;

	unsigned int		n_tx_req_queued;

	/* control bits */
	unsigned		cbits_tomodem;
	unsigned		cbits_tohost;

	/* counters */
	unsigned long		to_modem;
	unsigned long		to_host;
	unsigned int		tomodem_drp_cnt;
};

static struct {
	struct ghsuart_data_port	*port;
	struct platform_driver	pdrv;
} ghsuart_data_ports[NUM_HSUART_PORTS];

static void ghsuart_data_start_rx(struct ghsuart_data_port *port);

static void ghsuart_data_free_requests(struct usb_ep *ep,
				 struct list_head *head)
{
	struct usb_request	*req;

	while (!list_empty(head)) {
		req = list_entry(head->next, struct usb_request, list);
		list_del(&req->list);
		usb_ep_free_request(ep, req);
	}
}

static int ghsuart_data_alloc_requests(struct usb_ep *ep,
		struct list_head *head,
		int num,
		void (*cb)(struct usb_ep *ep, struct usb_request *),
		gfp_t flags)
{
	int			i;
	struct usb_request	*req;

	pr_debug("%s: ep:%s head:%p num:%d cb:%p", __func__,
			ep->name, head, num, cb);

	for (i = 0; i < num; i++) {
		req = usb_ep_alloc_request(ep, flags);
		if (!req) {
			pr_err("%s: req allocated:%d\n", __func__, i);
			return list_empty(head) ? -ENOMEM : 0;
		}
		req->complete = cb;
		list_add(&req->list, head);
	}

	return 0;
}

static void ghsuart_data_write_tohost(struct work_struct *w)
{
	unsigned long		flags;
	struct sk_buff		*skb;
	int			ret;
	struct usb_request	*req;
	struct usb_ep		*ep;
	struct ghsuart_data_port	*port;

	port = container_of(w, struct ghsuart_data_port, write_tohost_w);

	if (!port || !atomic_read(&port->connected))
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

		req = list_first_entry(&port->tx_idle, struct usb_request,
				list);
		req->context = skb;
		req->buf = skb->data;
		req->length = skb->len;

		port->n_tx_req_queued++;
		if (port->n_tx_req_queued == ghsuart_data_tx_intr_thld) {
			req->no_interrupt = 0;
			port->n_tx_req_queued = 0;
		} else {
			req->no_interrupt = 1;
		}

		list_del(&req->list);

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
	}
	spin_unlock_irqrestore(&port->tx_lock, flags);
}

static void ghsuart_data_write_tomdm(struct work_struct *w)
{
	struct ghsuart_data_port	*port;
	struct sk_buff		*skb;
	unsigned long		flags;
	int			ret;

	port = container_of(w, struct ghsuart_data_port, write_tomdm_w);

	if (!port || !atomic_read(&port->connected))
		return;

	spin_lock_irqsave(&port->rx_lock, flags);
	if (test_bit(TX_THROTTLED, &port->flags)) {
		spin_unlock_irqrestore(&port->rx_lock, flags);
		return;
	}

	while ((skb = __skb_dequeue(&port->rx_skb_q))) {
		pr_debug("%s: port:%p tom:%lu pno:%d\n", __func__,
				port, port->to_modem, port->port_num);

		ret = msm_smux_write(port->ch_id, skb, skb->data, skb->len);
		if (ret < 0) {
			if (ret == -EAGAIN) {
				/*flow control*/
				set_bit(TX_THROTTLED, &port->flags);
				__skb_queue_head(&port->rx_skb_q, skb);
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
	ghsuart_data_start_rx(port);
}

static void ghsuart_data_epin_complete(struct usb_ep *ep,
				struct usb_request *req)
{
	struct ghsuart_data_port	*port = ep->driver_data;
	struct sk_buff		*skb = req->context;
	int			status = req->status;

	switch (status) {
	case 0:
		/* successful completion */
		break;
	case -ECONNRESET:
	case -ESHUTDOWN:
		/* connection gone */
		dev_kfree_skb_any(skb);
		req->buf = 0;
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
ghsuart_data_epout_complete(struct usb_ep *ep, struct usb_request *req)
{
	struct ghsuart_data_port	*port = ep->driver_data;
	struct sk_buff		*skb = req->context;
	int			status = req->status;
	int			queue = 0;

	switch (status) {
	case 0:
		skb_put(skb, req->actual);
		queue = 1;
		break;
	case -ECONNRESET:
	case -ESHUTDOWN:
		/* cable disconnection */
		dev_kfree_skb_any(skb);
		req->buf = 0;
		usb_ep_free_request(ep, req);
		return;
	default:
		pr_err_ratelimited("%s: %s response error %d, %d/%d\n",
					__func__, ep->name, status,
				req->actual, req->length);
		dev_kfree_skb_any(skb);
		list_add_tail(&req->list, &port->rx_idle);
		return;
	}

	spin_lock(&port->rx_lock);
	if (queue) {
		__skb_queue_tail(&port->rx_skb_q, skb);
		list_add_tail(&req->list, &port->rx_idle);
		queue_work(port->wq, &port->write_tomdm_w);
	}
	spin_unlock(&port->rx_lock);
}

static void ghsuart_data_start_rx(struct ghsuart_data_port *port)
{
	struct usb_request	*req;
	struct usb_ep		*ep;
	unsigned long		flags;
	int			ret;
	struct sk_buff		*skb;

	pr_debug("%s: port:%p\n", __func__, port);
	if (!port)
		return;

	spin_lock_irqsave(&port->rx_lock, flags);
	ep = port->out;
	if (!ep) {
		spin_unlock_irqrestore(&port->rx_lock, flags);
		return;
	}

	if (test_bit(TX_THROTTLED, &port->flags)) {
		spin_unlock_irqrestore(&port->rx_lock, flags);
		return;
	}

	while (atomic_read(&port->connected) && !list_empty(&port->rx_idle)) {

		req = list_first_entry(&port->rx_idle,
					struct usb_request, list);

		skb = alloc_skb(ghsuart_data_rx_req_size, GFP_ATOMIC);
		if (!skb)
			break;
		list_del(&req->list);
		req->buf = skb->data;
		req->length = ghsuart_data_rx_req_size;
		req->context = skb;

		spin_unlock_irqrestore(&port->rx_lock, flags);
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

static void ghsuart_data_start_io(struct ghsuart_data_port *port)
{
	unsigned long	flags;
	struct usb_ep	*ep;
	int		ret;

	pr_debug("%s: port:%p\n", __func__, port);

	if (!port)
		return;

	spin_lock_irqsave(&port->rx_lock, flags);
	ep = port->out;
	if (!ep) {
		spin_unlock_irqrestore(&port->rx_lock, flags);
		return;
	}

	ret = ghsuart_data_alloc_requests(ep, &port->rx_idle,
		port->rx_q_size, ghsuart_data_epout_complete, GFP_ATOMIC);
	if (ret) {
		pr_err("%s: rx req allocation failed\n", __func__);
		spin_unlock_irqrestore(&port->rx_lock, flags);
		return;
	}
	spin_unlock_irqrestore(&port->rx_lock, flags);

	spin_lock_irqsave(&port->tx_lock, flags);
	ep = port->in;
	if (!ep) {
		spin_unlock_irqrestore(&port->tx_lock, flags);
		return;
	}

	ret = ghsuart_data_alloc_requests(ep, &port->tx_idle,
		port->tx_q_size, ghsuart_data_epin_complete, GFP_ATOMIC);
	if (ret) {
		pr_err("%s: tx req allocation failed\n", __func__);
		ghsuart_data_free_requests(ep, &port->rx_idle);
		spin_unlock_irqrestore(&port->tx_lock, flags);
		return;
	}
	spin_unlock_irqrestore(&port->tx_lock, flags);

	/* queue out requests */
	ghsuart_data_start_rx(port);
}

static void ghsuart_dunctrl_status(void *ctxt, unsigned int ctrl_bits)
{
	struct ghsuart_data_port  *port = ctxt;
	struct gserial          *gser;
	unsigned long	flags;

	pr_debug("%s - input control lines: dcd%c dsr%c break%c "
	"ring%c framing%c parity%c overrun%c\n", __func__,
	ctrl_bits & ACM_CTRL_DCD ? '+' : '-',
	ctrl_bits & ACM_CTRL_DSR ? '+' : '-',
	ctrl_bits & ACM_CTRL_BRK ? '+' : '-',
	ctrl_bits & ACM_CTRL_RI  ? '+' : '-',
	ctrl_bits & ACM_CTRL_FRAMING ? '+' : '-',
	ctrl_bits & ACM_CTRL_PARITY ? '+' : '-',
	ctrl_bits & ACM_CTRL_OVERRUN ? '+' : '-');

	spin_lock_irqsave(&port->port_lock, flags);
	port->cbits_tohost = ctrl_bits;
	gser = port->port_usb;
	spin_unlock_irqrestore(&port->port_lock, flags);
	if (gser && gser->send_modem_ctrl_bits)
		gser->send_modem_ctrl_bits(gser, ctrl_bits);
}

const char *event_string(int event_type)
{
	switch (event_type) {
	case SMUX_CONNECTED:
		return "SMUX_CONNECTED";
	case SMUX_DISCONNECTED:
		return "SMUX_DISCONNECTED";
	case SMUX_READ_DONE:
		return "SMUX_READ_DONE";
	case SMUX_READ_FAIL:
		return "SMUX_READ_FAIL";
	case SMUX_WRITE_DONE:
		return "SMUX_WRITE_DONE";
	case SMUX_WRITE_FAIL:
		return "SMUX_WRITE_FAIL";
	case SMUX_HIGH_WM_HIT:
		return "SMUX_HIGH_WM_HIT";
	case SMUX_LOW_WM_HIT:
		return "SMUX_LOW_WM_HIT";
	case SMUX_TIOCM_UPDATE:
		return "SMUX_TIOCM_UPDATE";
	default:
		return "UNDEFINED";
	}
}

static void ghsuart_notify_event(void *priv, int event_type,
				const void *metadata)
{
	struct ghsuart_data_port	*port = priv;
	struct smux_meta_write *meta_write =
				(struct smux_meta_write *) metadata;
	struct smux_meta_read *meta_read =
				(struct smux_meta_read *) metadata;
	struct sk_buff		*skb;
	unsigned long		flags;
	unsigned int		cbits;
	struct gserial		*gser;

	pr_debug("%s: event type: %s ", __func__, event_string(event_type));
	switch (event_type) {
	case SMUX_CONNECTED:
		set_bit(CH_OPENED, &port->channel_sts);
		if (port->gtype == USB_GADGET_SERIAL) {
			cbits = msm_smux_tiocm_get(port->ch_id);
			if (cbits & ACM_CTRL_DCD) {
				gser = port->port_usb;
				if (gser && gser->connect)
					gser->connect(gser);
			}
		}
		ghsuart_data_start_io(port);
		break;
	case SMUX_DISCONNECTED:
		clear_bit(CH_OPENED, &port->channel_sts);
		break;
	case SMUX_READ_DONE:
		skb = meta_read->pkt_priv;
		skb->data = meta_read->buffer;
		skb->len = meta_read->len;
		spin_lock_irqsave(&port->tx_lock, flags);
		__skb_queue_tail(&port->tx_skb_q, skb);
		spin_unlock_irqrestore(&port->tx_lock, flags);
		queue_work(port->wq, &port->write_tohost_w);
		break;
	case SMUX_WRITE_DONE:
		skb = meta_write->pkt_priv;
		skb->data = meta_write->buffer;
		dev_kfree_skb_any(skb);
		queue_work(port->wq, &port->write_tomdm_w);
		break;
	case SMUX_READ_FAIL:
		skb = meta_read->pkt_priv;
		skb->data = meta_read->buffer;
		dev_kfree_skb_any(skb);
		break;
	case SMUX_WRITE_FAIL:
		skb = meta_write->pkt_priv;
		skb->data = meta_write->buffer;
		dev_kfree_skb_any(skb);
		break;
	case SMUX_HIGH_WM_HIT:
		spin_lock_irqsave(&port->rx_lock, flags);
		set_bit(TX_THROTTLED, &port->flags);
		spin_unlock_irqrestore(&port->rx_lock, flags);
	case SMUX_LOW_WM_HIT:
		spin_lock_irqsave(&port->rx_lock, flags);
		clear_bit(TX_THROTTLED, &port->flags);
		spin_unlock_irqrestore(&port->rx_lock, flags);
		queue_work(port->wq, &port->write_tomdm_w);
		break;
	case SMUX_TIOCM_UPDATE:
		if (port->gtype == USB_GADGET_SERIAL) {
			cbits = msm_smux_tiocm_get(port->ch_id);
			ghsuart_dunctrl_status(port, cbits);
		}
		break;
	default:
		pr_err("%s:wrong event recieved\n", __func__);
	}
}

static int ghsuart_get_rx_buffer(void *priv, void **pkt_priv,
			void **buffer, int size)
{
	struct sk_buff		*skb;

	skb = alloc_skb(size, GFP_ATOMIC);
	if (!skb)
		return -ENOMEM;
	*pkt_priv = skb;
	*buffer = skb->data;

	return 0;
}

static void ghsuart_data_connect_w(struct work_struct *w)
{
	struct ghsuart_data_port	*port =
		container_of(w, struct ghsuart_data_port, connect_w);
	int			ret;

	if (!port || !atomic_read(&port->connected) ||
		!test_bit(CH_READY, &port->channel_sts))
		return;

	pr_debug("%s: port:%p\n", __func__, port);

	ret = msm_smux_open(port->ch_id, port, &ghsuart_notify_event,
				&ghsuart_get_rx_buffer);
	if (ret) {
		pr_err("%s: unable to open smux ch:%d err:%d\n",
				__func__, port->ch_id, ret);
		return;
	}
}

static void ghsuart_data_disconnect_w(struct work_struct *w)
{
	struct ghsuart_data_port	*port =
		container_of(w, struct ghsuart_data_port, disconnect_w);

	if (!test_bit(CH_OPENED, &port->channel_sts))
		return;

	msm_smux_close(port->ch_id);
	clear_bit(CH_OPENED, &port->channel_sts);
}

static void ghsuart_data_free_buffers(struct ghsuart_data_port *port)
{
	struct sk_buff	*skb;
	unsigned long	flags;

	if (!port)
		return;

	spin_lock_irqsave(&port->tx_lock, flags);
	if (!port->in) {
		spin_unlock_irqrestore(&port->tx_lock, flags);
		return;
	}

	ghsuart_data_free_requests(port->in, &port->tx_idle);

	while ((skb = __skb_dequeue(&port->tx_skb_q)))
		dev_kfree_skb_any(skb);
	spin_unlock_irqrestore(&port->tx_lock, flags);

	spin_lock_irqsave(&port->rx_lock, flags);
	if (!port->out) {
		spin_unlock_irqrestore(&port->rx_lock, flags);
		return;
	}

	ghsuart_data_free_requests(port->out, &port->rx_idle);

	while ((skb = __skb_dequeue(&port->rx_skb_q)))
		dev_kfree_skb_any(skb);
	spin_unlock_irqrestore(&port->rx_lock, flags);
}

static int ghsuart_data_probe(struct platform_device *pdev)
{
	struct ghsuart_data_port *port;

	pr_debug("%s: name:%s num_data_ports= %d\n",
		__func__, pdev->name, num_data_ports);

	if (pdev->id >= num_data_ports) {
		pr_err("%s: invalid port: %d\n", __func__, pdev->id);
		return -EINVAL;
	}

	port = ghsuart_data_ports[pdev->id].port;
	set_bit(CH_READY, &port->channel_sts);

	/* if usb is online, try opening bridge */
	if (atomic_read(&port->connected))
		queue_work(port->wq, &port->connect_w);

	return 0;
}

/* mdm disconnect */
static int ghsuart_data_remove(struct platform_device *pdev)
{
	struct ghsuart_data_port *port;
	struct usb_ep	*ep_in;
	struct usb_ep	*ep_out;
	int ret;
	struct gserial		*gser = NULL;
	unsigned long	flags;

	pr_debug("%s: name:%s\n", __func__, pdev->name);

	if (pdev->id >= num_data_ports) {
		pr_err("%s: invalid port: %d\n", __func__, pdev->id);
		return -EINVAL;
	}

	port = ghsuart_data_ports[pdev->id].port;

	ep_in = port->in;
	if (ep_in)
		usb_ep_fifo_flush(ep_in);

	ep_out = port->out;
	if (ep_out)
		usb_ep_fifo_flush(ep_out);

	ghsuart_data_free_buffers(port);

	if (port->gtype == USB_GADGET_SERIAL) {
		spin_lock_irqsave(&port->port_lock, flags);
		gser = port->port_usb;
		port->cbits_tohost = 0;
		spin_unlock_irqrestore(&port->port_lock, flags);
		if (gser && gser->disconnect)
			gser->disconnect(gser);
	}

	ret = msm_smux_close(port->ch_id);
	if (ret < 0)
		pr_err("%s:Unable to close smux channel: %d\n",
				__func__, port->ch_id);

	clear_bit(CH_READY, &port->channel_sts);
	clear_bit(CH_OPENED, &port->channel_sts);

	return 0;
}

static void ghsuart_data_port_free(int portno)
{
	struct ghsuart_data_port	*port = ghsuart_data_ports[portno].port;
	struct platform_driver	*pdrv = &ghsuart_data_ports[portno].pdrv;

	destroy_workqueue(port->wq);
	kfree(port);

	if (pdrv)
		platform_driver_unregister(pdrv);
}

static void
ghsuart_send_controlbits_tomodem(void *gptr, u8 portno, int cbits)
{
	struct ghsuart_data_port	*port;

	if (portno >= num_ctrl_ports || !gptr) {
		pr_err("%s: Invalid portno#%d\n", __func__, portno);
		return;
	}

	port = ghsuart_data_ports[portno].port;
	if (!port) {
		pr_err("%s: port is null\n", __func__);
		return;
	}

	if (cbits == port->cbits_tomodem)
		return;

	port->cbits_tomodem = cbits;

	if (!test_bit(CH_OPENED, &port->channel_sts))
		return;

	/* if DTR is high, update latest modem info to Host */
	if (port->cbits_tomodem & ACM_CTRL_DTR) {
		unsigned int i;

		i = msm_smux_tiocm_get(port->ch_id);
		ghsuart_dunctrl_status(port, i);
	}

	pr_debug("%s: ctrl_tomodem:%d\n", __func__, cbits);
	/* Send the control bits to the Modem */
	msm_smux_tiocm_set(port->ch_id, cbits, ~cbits);
}

static int ghsuart_data_port_alloc(unsigned port_num, enum gadget_type gtype)
{
	struct ghsuart_data_port	*port;
	struct platform_driver	*pdrv;

	port = kzalloc(sizeof(struct ghsuart_data_port), GFP_KERNEL);
	if (!port)
		return -ENOMEM;

	port->wq = create_singlethread_workqueue(ghsuart_data_names[port_num]);
	if (!port->wq) {
		pr_err("%s: Unable to create workqueue:%s\n",
			__func__, ghsuart_data_names[port_num]);
		kfree(port);
		return -ENOMEM;
	}
	port->port_num = port_num;

	/* port initialization */
	spin_lock_init(&port->port_lock);
	spin_lock_init(&port->rx_lock);
	spin_lock_init(&port->tx_lock);

	INIT_WORK(&port->connect_w, ghsuart_data_connect_w);
	INIT_WORK(&port->disconnect_w, ghsuart_data_disconnect_w);
	INIT_WORK(&port->write_tohost_w, ghsuart_data_write_tohost);
	INIT_WORK(&port->write_tomdm_w, ghsuart_data_write_tomdm);

	INIT_LIST_HEAD(&port->tx_idle);
	INIT_LIST_HEAD(&port->rx_idle);

	skb_queue_head_init(&port->tx_skb_q);
	skb_queue_head_init(&port->rx_skb_q);

	port->gtype = gtype;
	if (port->gtype == USB_GADGET_SERIAL)
		port->ch_id = SMUX_USB_DUN_0;
	else
		port->ch_id = SMUX_USB_RMNET_DATA_0;
	port->ctx = port;
	ghsuart_data_ports[port_num].port = port;

	pdrv = &ghsuart_data_ports[port_num].pdrv;
	pdrv->probe = ghsuart_data_probe;
	pdrv->remove = ghsuart_data_remove;
	pdrv->driver.name = ghsuart_data_names[port_num];
	pdrv->driver.owner = THIS_MODULE;

	platform_driver_register(pdrv);

	pr_debug("%s: port:%p portno:%d\n", __func__, port, port_num);

	return 0;
}

void ghsuart_data_disconnect(void *gptr, int port_num)
{
	struct ghsuart_data_port	*port;
	unsigned long		flags;
	struct gserial		*gser = NULL;

	pr_debug("%s: port#%d\n", __func__, port_num);

	port = ghsuart_data_ports[port_num].port;

	if (port_num > num_data_ports) {
		pr_err("%s: invalid portno#%d\n", __func__, port_num);
		return;
	}

	if (!gptr || !port) {
		pr_err("%s: port is null\n", __func__);
		return;
	}

	ghsuart_data_free_buffers(port);

	/* disable endpoints */
	if (port->in) {
		usb_ep_disable(port->in);
		port->in->driver_data = NULL;
	}

	if (port->out) {
		usb_ep_disable(port->out);
		port->out->driver_data = NULL;
	}
	atomic_set(&port->connected, 0);

	if (port->gtype == USB_GADGET_SERIAL) {
		gser = gptr;
		spin_lock_irqsave(&port->port_lock, flags);
		gser->notify_modem = 0;
		port->cbits_tomodem = 0;
		port->port_usb = 0;
		spin_unlock_irqrestore(&port->port_lock, flags);
	}

	spin_lock_irqsave(&port->tx_lock, flags);
	port->in = NULL;
	port->n_tx_req_queued = 0;
	clear_bit(RX_THROTTLED, &port->flags);
	spin_unlock_irqrestore(&port->tx_lock, flags);

	spin_lock_irqsave(&port->rx_lock, flags);
	port->out = NULL;
	clear_bit(TX_THROTTLED, &port->flags);
	spin_unlock_irqrestore(&port->rx_lock, flags);

	queue_work(port->wq, &port->disconnect_w);
}

int ghsuart_data_connect(void *gptr, int port_num)
{
	struct ghsuart_data_port		*port;
	struct gserial			*gser;
	struct grmnet			*gr;
	unsigned long			flags;
	int				ret = 0;

	pr_debug("%s: port#%d\n", __func__, port_num);

	port = ghsuart_data_ports[port_num].port;

	if (port_num > num_data_ports) {
		pr_err("%s: invalid portno#%d\n", __func__, port_num);
		return -ENODEV;
	}

	if (!gptr || !port) {
		pr_err("%s: port is null\n", __func__);
		return -ENODEV;
	}

	if (port->gtype == USB_GADGET_SERIAL) {
		gser = gptr;

		spin_lock_irqsave(&port->tx_lock, flags);
		port->in = gser->in;
		spin_unlock_irqrestore(&port->tx_lock, flags);

		spin_lock_irqsave(&port->rx_lock, flags);
		port->out = gser->out;
		spin_unlock_irqrestore(&port->rx_lock, flags);


		port->tx_q_size = ghsuart_data_serial_tx_q_size;
		port->rx_q_size = ghsuart_data_serial_rx_q_size;
		gser->in->driver_data = port;
		gser->out->driver_data = port;

		spin_lock_irqsave(&port->port_lock, flags);
		gser->notify_modem = ghsuart_send_controlbits_tomodem;
		port->port_usb = gptr;
		spin_unlock_irqrestore(&port->port_lock, flags);
	} else {
		gr = gptr;

		spin_lock_irqsave(&port->tx_lock, flags);
		port->in = gr->in;
		spin_unlock_irqrestore(&port->tx_lock, flags);

		spin_lock_irqsave(&port->rx_lock, flags);
		port->out = gr->out;
		spin_unlock_irqrestore(&port->rx_lock, flags);

		port->tx_q_size = ghsuart_data_rmnet_tx_q_size;
		port->rx_q_size = ghsuart_data_rmnet_rx_q_size;
		gr->in->driver_data = port;
		gr->out->driver_data = port;
	}

	ret = usb_ep_enable(port->in);
	if (ret) {
		pr_err("%s: usb_ep_enable failed eptype:IN ep:%p",
				__func__, port->in);
		goto fail;
	}

	ret = usb_ep_enable(port->out);
	if (ret) {
		pr_err("%s: usb_ep_enable failed eptype:OUT ep:%p",
				__func__, port->out);
		usb_ep_disable(port->in);
		goto fail;
	}

	atomic_set(&port->connected, 1);

	spin_lock_irqsave(&port->tx_lock, flags);
	port->to_host = 0;
	spin_unlock_irqrestore(&port->tx_lock, flags);

	spin_lock_irqsave(&port->rx_lock, flags);
	port->to_modem = 0;
	port->tomodem_drp_cnt = 0;
	spin_unlock_irqrestore(&port->rx_lock, flags);

	queue_work(port->wq, &port->connect_w);
fail:
	return ret;
}

#define DEBUG_BUF_SIZE 1024
static ssize_t ghsuart_data_read_stats(struct file *file,
	char __user *ubuf, size_t count, loff_t *ppos)
{
	struct ghsuart_data_port	*port;
	struct platform_driver	*pdrv;
	char			*buf;
	unsigned long		flags;
	int			ret;
	int			i;
	int			temp = 0;

	buf = kzalloc(sizeof(char) * DEBUG_BUF_SIZE, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	for (i = 0; i < num_data_ports; i++) {
		port = ghsuart_data_ports[i].port;
		if (!port)
			continue;
		pdrv = &ghsuart_data_ports[i].pdrv;

		spin_lock_irqsave(&port->rx_lock, flags);
		temp += scnprintf(buf + temp, DEBUG_BUF_SIZE - temp,
				"\nName:           %s\n"
				"#PORT:%d port#:   %p\n"
				"data_ch_open:	   %d\n"
				"data_ch_ready:    %d\n"
				"\n******UL INFO*****\n\n"
				"dpkts_to_modem:   %lu\n"
				"tomodem_drp_cnt:  %u\n"
				"rx_buf_len:       %u\n"
				"TX_THROTTLED      %d\n",
				pdrv->driver.name,
				i, port,
				test_bit(CH_OPENED, &port->channel_sts),
				test_bit(CH_READY, &port->channel_sts),
				port->to_modem,
				port->tomodem_drp_cnt,
				port->rx_skb_q.qlen,
				test_bit(TX_THROTTLED, &port->flags));
		spin_unlock_irqrestore(&port->rx_lock, flags);

		spin_lock_irqsave(&port->tx_lock, flags);
		temp += scnprintf(buf + temp, DEBUG_BUF_SIZE - temp,
				"\n******DL INFO******\n\n"
				"dpkts_to_usbhost: %lu\n"
				"tx_buf_len:	   %u\n"
				"RX_THROTTLED	   %d\n",
				port->to_host,
				port->tx_skb_q.qlen,
				test_bit(RX_THROTTLED, &port->flags));
		spin_unlock_irqrestore(&port->tx_lock, flags);

	}

	ret = simple_read_from_buffer(ubuf, count, ppos, buf, temp);

	kfree(buf);

	return ret;
}

static ssize_t ghsuart_data_reset_stats(struct file *file,
	const char __user *buf, size_t count, loff_t *ppos)
{
	struct ghsuart_data_port	*port;
	int			i;
	unsigned long		flags;

	for (i = 0; i < num_data_ports; i++) {
		port = ghsuart_data_ports[i].port;
		if (!port)
			continue;

		spin_lock_irqsave(&port->rx_lock, flags);
		port->to_modem = 0;
		port->tomodem_drp_cnt = 0;
		spin_unlock_irqrestore(&port->rx_lock, flags);

		spin_lock_irqsave(&port->tx_lock, flags);
		port->to_host = 0;
		spin_unlock_irqrestore(&port->tx_lock, flags);
	}
	return count;
}

const struct file_operations ghsuart_data_stats_ops = {
	.read = ghsuart_data_read_stats,
	.write = ghsuart_data_reset_stats,
};

static struct dentry	*ghsuart_data_dent;
static int ghsuart_data_debugfs_init(void)
{
	struct dentry	 *ghsuart_data_dfile;

	ghsuart_data_dent = debugfs_create_dir("ghsuart_data_xport", 0);
	if (!ghsuart_data_dent || IS_ERR(ghsuart_data_dent))
		return -ENODEV;

	ghsuart_data_dfile = debugfs_create_file("status", S_IRUGO | S_IWUSR,
				 ghsuart_data_dent, 0, &ghsuart_data_stats_ops);
	if (!ghsuart_data_dfile || IS_ERR(ghsuart_data_dfile)) {
		debugfs_remove(ghsuart_data_dent);
		return -ENODEV;
	}

	return 0;
}

static void ghsuart_data_debugfs_exit(void)
{
	debugfs_remove_recursive(ghsuart_data_dent);
}

int ghsuart_data_setup(unsigned num_ports, enum gadget_type gtype)
{
	int		first_port_id = num_data_ports;
	int		total_num_ports = num_ports + num_data_ports;
	int		ret = 0;
	int		i;

	if (!num_ports || total_num_ports > NUM_PORTS) {
		pr_err("%s: Invalid num of ports count:%d\n",
				__func__, num_ports);
		return -EINVAL;
	}
	pr_debug("%s: count: %d\n", __func__, num_ports);

	for (i = first_port_id; i < total_num_ports; i++) {

		/*probe can be called while port_alloc,so update no_data_ports*/
		num_data_ports++;
		ret = ghsuart_data_port_alloc(i, gtype);
		if (ret) {
			num_data_ports--;
			pr_err("%s: Unable to alloc port:%d\n", __func__, i);
			goto free_ports;
		}
	}

	/*return the starting index*/
	return first_port_id;

free_ports:
	for (i = first_port_id; i < num_data_ports; i++)
		ghsuart_data_port_free(i);
		num_data_ports = first_port_id;

	return ret;
}

static int __init ghsuart_data_init(void)
{
	int ret;

	ret = ghsuart_data_debugfs_init();
	if (ret) {
		pr_debug("mode debugfs file is not available");
		return ret;
	}

	return 0;
}
module_init(ghsuart_data_init);

static void __exit ghsuart_data_exit(void)
{
	ghsuart_data_debugfs_exit();
}
module_exit(ghsuart_data_exit);

MODULE_DESCRIPTION("hsuart data xport driver for DUN and RMNET");
MODULE_LICENSE("GPL v2");
