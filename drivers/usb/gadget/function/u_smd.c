/*
 * u_smd.c - utilities for USB gadget serial over smd
 *
 * Copyright (c) 2011, 2013-2015, The Linux Foundation. All rights reserved.
 *
 * This code also borrows from drivers/usb/gadget/u_serial.c, which is
 * Copyright (C) 2000 - 2003 Al Borchers (alborchers@steinerpoint.com)
 * Copyright (C) 2008 David Brownell
 * Copyright (C) 2008 by Nokia Corporation
 * Copyright (C) 1999 - 2002 Greg Kroah-Hartman (greg@kroah.com)
 * Copyright (C) 2000 Peter Berger (pberger@brimson.com)
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
#include <soc/qcom/smd.h>
#include <linux/debugfs.h>

#include "u_serial.h"

#define SMD_RX_QUEUE_SIZE		8
#define SMD_RX_BUF_SIZE			2048

#define SMD_TX_QUEUE_SIZE		8
#define SMD_TX_BUF_SIZE			2048

static struct workqueue_struct *gsmd_wq;

#define SMD_N_PORTS	2
#define CH_OPENED	0
#define CH_READY	1
struct smd_port_info {
	struct smd_channel	*ch;
	char			*name;
	unsigned long		flags;
};

struct smd_port_info smd_pi[SMD_N_PORTS] = {
	{
		.name = "DS",
	},
	{
		.name = "UNUSED",
	},
};

struct gsmd_port {
	unsigned		port_num;
	spinlock_t		port_lock;

	unsigned		n_read;
	struct list_head	read_pool;
	struct list_head	read_queue;
	struct work_struct	push;

	struct list_head	write_pool;
	struct work_struct	pull;

	struct gserial		*port_usb;

	struct smd_port_info	*pi;
	struct delayed_work	connect_work;
	struct work_struct	disconnect_work;

	/* At present, smd does not notify
	 * control bit change info from modem
	 */
	struct work_struct	update_modem_ctrl_sig;

#define SMD_ACM_CTRL_DTR		0x01
#define SMD_ACM_CTRL_RTS		0x02
	unsigned		cbits_to_modem;

#define SMD_ACM_CTRL_DCD		0x01
#define SMD_ACM_CTRL_DSR		0x02
#define SMD_ACM_CTRL_BRK		0x04
#define SMD_ACM_CTRL_RI		0x08
	unsigned		cbits_to_laptop;

	/* pkt counters */
	unsigned long		nbytes_tomodem;
	unsigned long		nbytes_tolaptop;
};

static struct smd_portmaster {
	struct mutex lock;
	struct gsmd_port *port;
	struct platform_driver pdrv;
} smd_ports[SMD_N_PORTS];
static unsigned n_smd_ports;
u32			extra_sz;

static void gsmd_free_req(struct usb_ep *ep, struct usb_request *req)
{
	kfree(req->buf);
	usb_ep_free_request(ep, req);
}

static void gsmd_free_requests(struct usb_ep *ep, struct list_head *head)
{
	struct usb_request	*req;

	while (!list_empty(head)) {
		req = list_entry(head->next, struct usb_request, list);
		list_del(&req->list);
		gsmd_free_req(ep, req);
	}
}

static struct usb_request *
gsmd_alloc_req(struct usb_ep *ep, unsigned len, size_t extra_sz, gfp_t flags)
{
	struct usb_request *req;

	req = usb_ep_alloc_request(ep, flags);
	if (!req) {
		pr_err("%s: usb alloc request failed\n", __func__);
		return 0;
	}

	req->length = len;
	req->buf = kmalloc(len + extra_sz, flags);
	if (!req->buf) {
		pr_err("%s: request buf allocation failed\n", __func__);
		usb_ep_free_request(ep, req);
		return 0;
	}

	return req;
}

static int gsmd_alloc_requests(struct usb_ep *ep, struct list_head *head,
		int num, int size, size_t extra_sz,
		void (*cb)(struct usb_ep *ep, struct usb_request *))
{
	int i;
	struct usb_request *req;

	pr_debug("%s: ep:%p head:%p num:%d size:%d cb:%p", __func__,
			ep, head, num, size, cb);

	for (i = 0; i < num; i++) {
		req = gsmd_alloc_req(ep, size, extra_sz, GFP_ATOMIC);
		if (!req) {
			pr_debug("%s: req allocated:%d\n", __func__, i);
			return list_empty(head) ? -ENOMEM : 0;
		}
		req->complete = cb;
		list_add(&req->list, head);
	}

	return 0;
}

static void gsmd_start_rx(struct gsmd_port *port)
{
	struct list_head	*pool;
	struct usb_ep		*out;
	unsigned long	flags;
	int ret;

	if (!port) {
		pr_err("%s: port is null\n", __func__);
		return;
	}

	spin_lock_irqsave(&port->port_lock, flags);

	if (!port->port_usb) {
		pr_debug("%s: USB disconnected\n", __func__);
		goto start_rx_end;
	}

	pool = &port->read_pool;
	out = port->port_usb->out;

	while (test_bit(CH_OPENED, &port->pi->flags) && !list_empty(pool)) {
		struct usb_request	*req;

		req = list_entry(pool->next, struct usb_request, list);
		list_del(&req->list);
		req->length = SMD_RX_BUF_SIZE;

		spin_unlock_irqrestore(&port->port_lock, flags);
		ret = usb_ep_queue(out, req, GFP_KERNEL);
		spin_lock_irqsave(&port->port_lock, flags);
		if (ret) {
			pr_err("%s: usb ep out queue failed"
					"port:%p, port#%d\n",
					 __func__, port, port->port_num);
			list_add_tail(&req->list, pool);
			break;
		}
	}
start_rx_end:
	spin_unlock_irqrestore(&port->port_lock, flags);
}

static void gsmd_rx_push(struct work_struct *w)
{
	struct gsmd_port *port = container_of(w, struct gsmd_port, push);
	struct smd_port_info *pi = port->pi;
	struct list_head *q;

	pr_debug("%s: port:%p port#%d", __func__, port, port->port_num);

	spin_lock_irq(&port->port_lock);

	q = &port->read_queue;
	while (pi->ch && !list_empty(q)) {
		struct usb_request *req;
		int avail;

		req = list_first_entry(q, struct usb_request, list);

		switch (req->status) {
		case -ESHUTDOWN:
			pr_debug("%s: req status shutdown portno#%d port:%p\n",
					__func__, port->port_num, port);
			goto rx_push_end;
		default:
			pr_warning("%s: port:%p port#%d"
					" Unexpected Rx Status:%d\n", __func__,
					port, port->port_num, req->status);
		case 0:
			/* normal completion */
			break;
		}

		avail = smd_write_avail(pi->ch);
		if (!avail)
			goto rx_push_end;

		if (req->actual) {
			char		*packet = req->buf;
			unsigned	size = req->actual;
			unsigned	n;
			int		count;

			n = port->n_read;
			if (n) {
				packet += n;
				size -= n;
			}

			count = smd_write(pi->ch, packet, size);
			if (count < 0) {
				pr_err("%s: smd write failed err:%d\n",
						__func__, count);
				goto rx_push_end;
			}

			if (count != size) {
				port->n_read += count;
				goto rx_push_end;
			}

			port->nbytes_tomodem += count;
		}

		port->n_read = 0;
		list_move(&req->list, &port->read_pool);
	}

rx_push_end:
	spin_unlock_irq(&port->port_lock);

	gsmd_start_rx(port);
}

static void gsmd_read_pending(struct gsmd_port *port)
{
	int avail;

	if (!port || !port->pi->ch)
		return;

	/* passing null buffer discards the data */
	while ((avail = smd_read_avail(port->pi->ch)))
		smd_read(port->pi->ch, 0, avail);

	return;
}

static void gsmd_tx_pull(struct work_struct *w)
{
	struct gsmd_port *port = container_of(w, struct gsmd_port, pull);
	struct list_head *pool = &port->write_pool;
	struct smd_port_info *pi = port->pi;
	struct usb_ep *in;

	pr_debug("%s: port:%p port#%d pool:%p\n", __func__,
			port, port->port_num, pool);

	spin_lock_irq(&port->port_lock);

	if (!port->port_usb) {
		pr_debug("%s: usb is disconnected\n", __func__);
		spin_unlock_irq(&port->port_lock);
		gsmd_read_pending(port);
		return;
	}

	in = port->port_usb->in;
	while (pi->ch && !list_empty(pool)) {
		struct usb_request *req;
		int avail;
		int ret;

		avail = smd_read_avail(pi->ch);
		if (!avail)
			break;

		avail = avail > SMD_TX_BUF_SIZE ? SMD_TX_BUF_SIZE : avail;

		req = list_entry(pool->next, struct usb_request, list);
		list_del(&req->list);
		req->length = smd_read(pi->ch, req->buf, avail);
		req->zero = 1;

		spin_unlock_irq(&port->port_lock);
		ret = usb_ep_queue(in, req, GFP_KERNEL);
		spin_lock_irq(&port->port_lock);
		if (ret) {
			pr_err("%s: usb ep in queue failed"
					"port:%p, port#%d err:%d\n",
					__func__, port, port->port_num, ret);
			/* could be usb disconnected */
			if (!port->port_usb)
				gsmd_free_req(in, req);
			else
				list_add(&req->list, pool);
			goto tx_pull_end;
		}

		port->nbytes_tolaptop += req->length;
	}

tx_pull_end:
	/* TBD: Check how code behaves on USB bus suspend */
	if (port->port_usb && smd_read_avail(port->pi->ch) && !list_empty(pool))
		queue_work(gsmd_wq, &port->pull);

	spin_unlock_irq(&port->port_lock);

	return;
}

static void gsmd_read_complete(struct usb_ep *ep, struct usb_request *req)
{
	struct gsmd_port *port = ep->driver_data;

	pr_debug("%s: ep:%p port:%p\n", __func__, ep, port);

	if (!port) {
		pr_err("%s: port is null\n", __func__);
		return;
	}

	spin_lock(&port->port_lock);
	if (!test_bit(CH_OPENED, &port->pi->flags) ||
			req->status == -ESHUTDOWN) {
		spin_unlock(&port->port_lock);
		gsmd_free_req(ep, req);
		return;
	}

	list_add_tail(&req->list, &port->read_queue);
	queue_work(gsmd_wq, &port->push);
	spin_unlock(&port->port_lock);

	return;
}

static void gsmd_write_complete(struct usb_ep *ep, struct usb_request *req)
{
	struct gsmd_port *port = ep->driver_data;

	pr_debug("%s: ep:%p port:%p\n", __func__, ep, port);

	if (!port) {
		pr_err("%s: port is null\n", __func__);
		return;
	}

	spin_lock(&port->port_lock);
	if (!test_bit(CH_OPENED, &port->pi->flags) ||
			req->status == -ESHUTDOWN) {
		spin_unlock(&port->port_lock);
		gsmd_free_req(ep, req);
		return;
	}

	if (req->status)
		pr_warning("%s: port:%p port#%d unexpected %s status %d\n",
				__func__, port, port->port_num,
				ep->name, req->status);

	list_add(&req->list, &port->write_pool);
	queue_work(gsmd_wq, &port->pull);
	spin_unlock(&port->port_lock);

	return;
}

static void gsmd_start_io(struct gsmd_port *port)
{
	int		ret = -ENODEV;

	pr_debug("%s: port: %p\n", __func__, port);

	spin_lock(&port->port_lock);

	if (!port->port_usb)
		goto start_io_out;

	smd_tiocmset_from_cb(port->pi->ch,
			port->cbits_to_modem,
			~port->cbits_to_modem);

	ret = gsmd_alloc_requests(port->port_usb->out,
				&port->read_pool,
				SMD_RX_QUEUE_SIZE, SMD_RX_BUF_SIZE, 0,
				gsmd_read_complete);
	if (ret) {
		pr_err("%s: unable to allocate out requests\n",
				__func__);
		goto start_io_out;
	}

	ret = gsmd_alloc_requests(port->port_usb->in,
				&port->write_pool,
				SMD_TX_QUEUE_SIZE, SMD_TX_BUF_SIZE, extra_sz,
				gsmd_write_complete);
	if (ret) {
		gsmd_free_requests(port->port_usb->out, &port->read_pool);
		pr_err("%s: unable to allocate IN requests\n",
				__func__);
		goto start_io_out;
	}

start_io_out:
	spin_unlock(&port->port_lock);

	if (ret)
		return;

	gsmd_start_rx(port);
}

static unsigned int convert_uart_sigs_to_acm(unsigned uart_sig)
{
	unsigned int acm_sig = 0;

	/* should this needs to be in calling functions ??? */
	uart_sig &= (TIOCM_RI | TIOCM_CD | TIOCM_DSR);

	if (uart_sig & TIOCM_RI)
		acm_sig |= SMD_ACM_CTRL_RI;
	if (uart_sig & TIOCM_CD)
		acm_sig |= SMD_ACM_CTRL_DCD;
	if (uart_sig & TIOCM_DSR)
		acm_sig |= SMD_ACM_CTRL_DSR;

	return acm_sig;
}

static unsigned int convert_acm_sigs_to_uart(unsigned acm_sig)
{
	unsigned int uart_sig = 0;

	/* should this needs to be in calling functions ??? */
	acm_sig &= (SMD_ACM_CTRL_DTR | SMD_ACM_CTRL_RTS);

	if (acm_sig & SMD_ACM_CTRL_DTR)
		uart_sig |= TIOCM_DTR;
	if (acm_sig & SMD_ACM_CTRL_RTS)
		uart_sig |= TIOCM_RTS;

	return uart_sig;
}


static void gsmd_stop_io(struct gsmd_port *port)
{
	struct usb_ep	*in;
	struct usb_ep	*out;
	unsigned long	flags;

	spin_lock_irqsave(&port->port_lock, flags);
	if (!port->port_usb) {
		spin_unlock_irqrestore(&port->port_lock, flags);
		return;
	}
	in = port->port_usb->in;
	out = port->port_usb->out;
	spin_unlock_irqrestore(&port->port_lock, flags);

	usb_ep_fifo_flush(in);
	usb_ep_fifo_flush(out);

	spin_lock(&port->port_lock);
	if (port->port_usb) {
		gsmd_free_requests(out, &port->read_pool);
		gsmd_free_requests(out, &port->read_queue);
		gsmd_free_requests(in, &port->write_pool);
		port->n_read = 0;
		port->cbits_to_laptop = 0;
	}

	if (port->port_usb->send_modem_ctrl_bits)
		port->port_usb->send_modem_ctrl_bits(
					port->port_usb,
					port->cbits_to_laptop);
	spin_unlock(&port->port_lock);

}

static void gsmd_notify(void *priv, unsigned event)
{
	struct gsmd_port *port = priv;
	struct smd_port_info *pi = port->pi;
	int i;

	switch (event) {
	case SMD_EVENT_DATA:
		pr_debug("%s: Event data\n", __func__);
		if (smd_read_avail(pi->ch))
			queue_work(gsmd_wq, &port->pull);
		if (smd_write_avail(pi->ch))
			queue_work(gsmd_wq, &port->push);
		break;
	case SMD_EVENT_OPEN:
		pr_debug("%s: Event Open\n", __func__);
		set_bit(CH_OPENED, &pi->flags);
		gsmd_start_io(port);
		break;
	case SMD_EVENT_CLOSE:
		pr_debug("%s: Event Close\n", __func__);
		clear_bit(CH_OPENED, &pi->flags);
		gsmd_stop_io(port);
		break;
	case SMD_EVENT_STATUS:
		i = smd_tiocmget(port->pi->ch);
		port->cbits_to_laptop = convert_uart_sigs_to_acm(i);
		if (port->port_usb && port->port_usb->send_modem_ctrl_bits)
			port->port_usb->send_modem_ctrl_bits(port->port_usb,
						port->cbits_to_laptop);
		break;
	}
}

static void gsmd_connect_work(struct work_struct *w)
{
	struct gsmd_port *port;
	struct smd_port_info *pi;
	int ret;

	port = container_of(w, struct gsmd_port, connect_work.work);
	pi = port->pi;

	pr_debug("%s: port:%p port#%d\n", __func__, port, port->port_num);

	if (!test_bit(CH_READY, &pi->flags))
		return;

	ret = smd_named_open_on_edge(pi->name, SMD_APPS_MODEM,
				&pi->ch, port, gsmd_notify);
	if (ret) {
		if (ret == -EAGAIN) {
			/* port not ready  - retry */
			pr_debug("%s: SMD port not ready - rescheduling:%s err:%d\n",
					__func__, pi->name, ret);
			queue_delayed_work(gsmd_wq, &port->connect_work,
				msecs_to_jiffies(250));
		} else {
			pr_err("%s: unable to open smd port:%s err:%d\n",
					__func__, pi->name, ret);
		}
	}
}

static void gsmd_disconnect_work(struct work_struct *w)
{
	struct gsmd_port *port;
	struct smd_port_info *pi;

	port = container_of(w, struct gsmd_port, disconnect_work);
	pi = port->pi;

	pr_debug("%s: port:%p port#%d\n", __func__, port, port->port_num);

	smd_close(port->pi->ch);
	port->pi->ch = NULL;
}

static void gsmd_notify_modem(void *gptr, u8 portno, int ctrl_bits)
{
	struct gsmd_port *port;
	int temp;
	struct gserial *gser = gptr;

	if (portno >= n_smd_ports) {
		pr_err("%s: invalid portno#%d\n", __func__, portno);
		return;
	}

	if (!gser) {
		pr_err("%s: gser is null\n", __func__);
		return;
	}

	port = smd_ports[portno].port;

	temp = convert_acm_sigs_to_uart(ctrl_bits);

	if (temp == port->cbits_to_modem)
		return;

	port->cbits_to_modem = temp;

	/* usb could send control signal before smd is ready */
	if (!test_bit(CH_OPENED, &port->pi->flags))
		return;

	pr_debug("%s: ctrl_tomodem:%d DTR:%d  RST:%d\n", __func__, ctrl_bits,
		ctrl_bits & SMD_ACM_CTRL_DTR ? 1 : 0,
		ctrl_bits & SMD_ACM_CTRL_RTS ? 1 : 0);
	/* if DTR is high, update latest modem info to laptop */
	if (port->cbits_to_modem & TIOCM_DTR) {
		unsigned i;

		i = smd_tiocmget(port->pi->ch);
		port->cbits_to_laptop = convert_uart_sigs_to_acm(i);

		pr_debug("%s - input control lines: cbits_to_host:%x DCD:%c DSR:%c BRK:%c RING:%c\n",
			__func__, port->cbits_to_laptop,
			port->cbits_to_laptop & SMD_ACM_CTRL_DCD ? '1' : '0',
			port->cbits_to_laptop & SMD_ACM_CTRL_DSR ? '1' : '0',
			port->cbits_to_laptop & SMD_ACM_CTRL_BRK ? '1' : '0',
			port->cbits_to_laptop & SMD_ACM_CTRL_RI  ? '1' : '0');
		if (gser->send_modem_ctrl_bits)
			gser->send_modem_ctrl_bits(
					port->port_usb,
					port->cbits_to_laptop);
	}

	smd_tiocmset(port->pi->ch,
			port->cbits_to_modem,
			~port->cbits_to_modem);
}

int gsmd_connect(struct gserial *gser, u8 portno)
{
	unsigned long flags;
	int ret;
	struct gsmd_port *port;

	pr_debug("%s: gserial:%p portno:%u\n", __func__, gser, portno);

	if (portno >= n_smd_ports) {
		pr_err("%s: Invalid port no#%d", __func__, portno);
		return -EINVAL;
	}

	if (!gser) {
		pr_err("%s: gser is null\n", __func__);
		return -EINVAL;
	}

	port = smd_ports[portno].port;

	spin_lock_irqsave(&port->port_lock, flags);
	port->port_usb = gser;
	gser->notify_modem = gsmd_notify_modem;
	port->nbytes_tomodem = 0;
	port->nbytes_tolaptop = 0;
	spin_unlock_irqrestore(&port->port_lock, flags);

	ret = usb_ep_enable(gser->in);
	if (ret) {
		pr_err("%s: usb_ep_enable failed eptype:IN ep:%p, err:%d",
				__func__, gser->in, ret);
		port->port_usb = 0;
		return ret;
	}
	gser->in->driver_data = port;

	ret = usb_ep_enable(gser->out);
	if (ret) {
		pr_err("%s: usb_ep_enable failed eptype:OUT ep:%p, err: %d",
				__func__, gser->out, ret);
		port->port_usb = 0;
		gser->in->driver_data = 0;
		return ret;
	}
	gser->out->driver_data = port;

	queue_delayed_work(gsmd_wq, &port->connect_work, msecs_to_jiffies(0));

	return 0;
}

void gsmd_disconnect(struct gserial *gser, u8 portno)
{
	unsigned long flags;
	struct gsmd_port *port;

	pr_debug("%s: gserial:%p portno:%u\n", __func__, gser, portno);

	if (portno >= n_smd_ports) {
		pr_err("%s: invalid portno#%d\n", __func__, portno);
		return;
	}

	if (!gser) {
		pr_err("%s: gser is null\n", __func__);
		return;
	}

	port = smd_ports[portno].port;

	spin_lock_irqsave(&port->port_lock, flags);
	port->port_usb = 0;
	spin_unlock_irqrestore(&port->port_lock, flags);

	/* disable endpoints, aborting down any active I/O */
	usb_ep_disable(gser->out);
	gser->out->driver_data = NULL;
	usb_ep_disable(gser->in);
	gser->in->driver_data = NULL;

	spin_lock_irqsave(&port->port_lock, flags);
	gsmd_free_requests(gser->out, &port->read_pool);
	gsmd_free_requests(gser->out, &port->read_queue);
	gsmd_free_requests(gser->in, &port->write_pool);
	port->n_read = 0;
	spin_unlock_irqrestore(&port->port_lock, flags);

	if (test_and_clear_bit(CH_OPENED, &port->pi->flags)) {
		/* lower the dtr */
		port->cbits_to_modem = 0;
		smd_tiocmset(port->pi->ch,
				port->cbits_to_modem,
				~port->cbits_to_modem);
	}

	gser->notify_modem = NULL;

	if (port->pi->ch)
		queue_work(gsmd_wq, &port->disconnect_work);
}

#define SMD_CH_MAX_LEN	20
static int gsmd_ch_probe(struct platform_device *pdev)
{
	struct gsmd_port *port;
	struct smd_port_info *pi;
	int i;
	unsigned long flags;

	pr_debug("%s: name:%s\n", __func__, pdev->name);

	for (i = 0; i < n_smd_ports; i++) {
		port = smd_ports[i].port;
		pi = port->pi;

		if (!strncmp(pi->name, pdev->name, SMD_CH_MAX_LEN)) {
			set_bit(CH_READY, &pi->flags);
			spin_lock_irqsave(&port->port_lock, flags);
			if (port->port_usb)
				queue_delayed_work(gsmd_wq, &port->connect_work,
					msecs_to_jiffies(0));
			spin_unlock_irqrestore(&port->port_lock, flags);
			break;
		}
	}
	return 0;
}

static int gsmd_ch_remove(struct platform_device *pdev)
{
	struct gsmd_port *port;
	struct smd_port_info *pi;
	int i;

	pr_debug("%s: name:%s\n", __func__, pdev->name);

	for (i = 0; i < n_smd_ports; i++) {
		port = smd_ports[i].port;
		pi = port->pi;

		if (!strncmp(pi->name, pdev->name, SMD_CH_MAX_LEN)) {
			clear_bit(CH_READY, &pi->flags);
			clear_bit(CH_OPENED, &pi->flags);
			if (pi->ch) {
				smd_close(pi->ch);
				pi->ch = NULL;
			}
			break;
		}
	}
	return 0;
}

static void gsmd_port_free(int portno)
{
	struct gsmd_port *port = smd_ports[portno].port;

	if (!port)
		kfree(port);
}

static int gsmd_port_alloc(int portno, struct usb_cdc_line_coding *coding)
{
	struct gsmd_port *port;
	struct platform_driver *pdrv;

	port = kzalloc(sizeof(struct gsmd_port), GFP_KERNEL);
	if (!port)
		return -ENOMEM;

	port->port_num = portno;
	port->pi = &smd_pi[portno];

	spin_lock_init(&port->port_lock);

	INIT_LIST_HEAD(&port->read_pool);
	INIT_LIST_HEAD(&port->read_queue);
	INIT_WORK(&port->push, gsmd_rx_push);

	INIT_LIST_HEAD(&port->write_pool);
	INIT_WORK(&port->pull, gsmd_tx_pull);

	INIT_DELAYED_WORK(&port->connect_work, gsmd_connect_work);
	INIT_WORK(&port->disconnect_work, gsmd_disconnect_work);

	smd_ports[portno].port = port;
	pdrv = &smd_ports[portno].pdrv;
	pdrv->probe = gsmd_ch_probe;
	pdrv->remove = gsmd_ch_remove;
	pdrv->driver.name = port->pi->name;
	pdrv->driver.owner = THIS_MODULE;
	platform_driver_register(pdrv);

	pr_debug("%s: port:%p portno:%d\n", __func__, port, portno);

	return 0;
}

#if defined(CONFIG_DEBUG_FS)
static ssize_t debug_smd_read_stats(struct file *file, char __user *ubuf,
		size_t count, loff_t *ppos)
{
	struct gsmd_port *port;
	struct smd_port_info *pi;
	char *buf;
	unsigned long flags;
	int temp = 0;
	int i;
	int ret;

	buf = kzalloc(sizeof(char) * 512, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	for (i = 0; i < n_smd_ports; i++) {
		port = smd_ports[i].port;
		pi = port->pi;
		spin_lock_irqsave(&port->port_lock, flags);
		temp += scnprintf(buf + temp, 512 - temp,
				"###PORT:%d###\n"
				"nbytes_tolaptop: %lu\n"
				"nbytes_tomodem:  %lu\n"
				"cbits_to_modem:  %u\n"
				"cbits_to_laptop: %u\n"
				"n_read: %u\n"
				"smd_read_avail: %d\n"
				"smd_write_avail: %d\n"
				"CH_OPENED: %d\n"
				"CH_READY: %d\n",
				i, port->nbytes_tolaptop, port->nbytes_tomodem,
				port->cbits_to_modem, port->cbits_to_laptop,
				port->n_read,
				pi->ch ? smd_read_avail(pi->ch) : 0,
				pi->ch ? smd_write_avail(pi->ch) : 0,
				test_bit(CH_OPENED, &pi->flags),
				test_bit(CH_READY, &pi->flags));
		spin_unlock_irqrestore(&port->port_lock, flags);
	}

	ret = simple_read_from_buffer(ubuf, count, ppos, buf, temp);

	kfree(buf);

	return ret;

}

static ssize_t debug_smd_reset_stats(struct file *file, const char __user *buf,
				 size_t count, loff_t *ppos)
{
	struct gsmd_port *port;
	unsigned long flags;
	int i;

	for (i = 0; i < n_smd_ports; i++) {
		port = smd_ports[i].port;

		spin_lock_irqsave(&port->port_lock, flags);
		port->nbytes_tolaptop = 0;
		port->nbytes_tomodem = 0;
		spin_unlock_irqrestore(&port->port_lock, flags);
	}

	return count;
}

static int debug_smd_open(struct inode *inode, struct file *file)
{
	return 0;
}

static const struct file_operations debug_gsmd_ops = {
	.open = debug_smd_open,
	.read = debug_smd_read_stats,
	.write = debug_smd_reset_stats,
};

static void gsmd_debugfs_init(void)
{
	struct dentry *dent;

	dent = debugfs_create_dir("usb_gsmd", 0);
	if (IS_ERR(dent))
		return;

	debugfs_create_file("status", 0444, dent, 0, &debug_gsmd_ops);
}
#else
static void gsmd_debugfs_init(void) {}
#endif

int gsmd_setup(struct usb_gadget *g, unsigned count)
{
	struct usb_cdc_line_coding	coding;
	int ret;
	int i;

	pr_debug("%s: g:%p count: %d\n", __func__, g, count);

	if (!count || count > SMD_N_PORTS) {
		pr_err("%s: Invalid num of ports count:%d gadget:%p\n",
				__func__, count, g);
		return -EINVAL;
	}

	coding.dwDTERate = cpu_to_le32(9600);
	coding.bCharFormat = 8;
	coding.bParityType = USB_CDC_NO_PARITY;
	coding.bDataBits = USB_CDC_1_STOP_BITS;

	gsmd_wq = create_singlethread_workqueue("k_gsmd");
	if (!gsmd_wq) {
		pr_err("%s: Unable to create workqueue gsmd_wq\n",
				__func__);
		return -ENOMEM;
	}
	extra_sz = g->extra_buf_alloc;

	for (i = 0; i < count; i++) {
		mutex_init(&smd_ports[i].lock);
		n_smd_ports++;
		ret = gsmd_port_alloc(i, &coding);
		if (ret) {
			n_smd_ports--;
			pr_err("%s: Unable to alloc port:%d\n", __func__, i);
			goto free_smd_ports;
		}
	}

	gsmd_debugfs_init();

	return 0;
free_smd_ports:
	for (i = 0; i < n_smd_ports; i++)
		gsmd_port_free(i);

	destroy_workqueue(gsmd_wq);

	return ret;
}

void gsmd_cleanup(struct usb_gadget *g, unsigned count)
{
	/* TBD */
}

int gsmd_write(u8 portno, char *buf, unsigned int size)
{
	int count, avail;
	struct gsmd_port const *port = smd_ports[portno].port;

	if (portno > SMD_N_PORTS)
		return -EINVAL;

	avail = smd_write_avail(port->pi->ch);
	if (avail < size)
		return -EAGAIN;

	count = smd_write(port->pi->ch, buf, size);
	return count;
}

