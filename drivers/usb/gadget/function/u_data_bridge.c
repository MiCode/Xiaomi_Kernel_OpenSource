/*
 * Copyright (c) 2011, 2013-2018, The Linux Foundation. All rights reserved.
 * Linux Foundation chooses to take subject only to the GPLv2 license terms,
 * and distributes only under these terms.
 *
 * This code also borrows from drivers/usb/gadget/u_serial.c, which is
 * Copyright (C) 2000 - 2003 Al Borchers (alborchers@steinerpoint.com)
 * Copyright (C) 2008 David Brownell
 * Copyright (C) 2008 by Nokia Corporation
 * Copyright (C) 1999 - 2002 Greg Kroah-Hartman (greg@kroah.com)
 * Copyright (C) 2000 Peter Berger (pberger@brimson.com)
 *
 * gbridge_port_read() API implementation is using borrowed code from
 * drivers/usb/gadget/legacy/printer.c, which is
 * Copyright (C) 2003-2005 David Brownell
 * Copyright (C) 2006 Craig W. Nadler
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

#include <linux/wait.h>
#include <linux/poll.h>
#include <linux/kernel.h>
#include <linux/interrupt.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/debugfs.h>
#include <linux/spinlock.h>
#include <asm/ioctls.h>

#define DEVICE_NAME "at_usb"
#define MODULE_NAME "msm_usb_bridge"
#define num_of_instance 2

#define BRIDGE_RX_QUEUE_SIZE	8
#define BRIDGE_RX_BUF_SIZE	2048
#define BRIDGE_TX_QUEUE_SIZE	8
#define BRIDGE_TX_BUF_SIZE	2048

struct gbridge_port {
	struct cdev		gbridge_cdev;
	struct device		*dev;
	unsigned		port_num;
	char			name[sizeof(DEVICE_NAME) + 2];

	spinlock_t		port_lock;

	wait_queue_head_t	open_wq;
	wait_queue_head_t	read_wq;

	struct list_head	read_pool;
	struct list_head	read_queued;
	struct list_head	write_pool;

	/* current active USB RX request */
	struct usb_request	*current_rx_req;
	/* number of pending bytes */
	size_t			pending_rx_bytes;
	/* current USB RX buffer */
	u8			*current_rx_buf;

	struct gserial		*port_usb;

	unsigned		cbits_to_modem;
	bool			cbits_updated;

	bool			is_connected;
	bool			port_open;

	unsigned long           nbytes_from_host;
	unsigned long		nbytes_to_host;
	unsigned long           nbytes_to_port_bridge;
	unsigned long		nbytes_from_port_bridge;
};

struct gbridge_port *ports[num_of_instance];
struct class *gbridge_classp;
static dev_t gbridge_number;
static struct workqueue_struct *gbridge_wq;
static unsigned n_bridge_ports;
static void gbridge_read_complete(struct usb_ep *ep, struct usb_request *req);
static void gbridge_free_req(struct usb_ep *ep, struct usb_request *req)
{
	kfree(req->buf);
	usb_ep_free_request(ep, req);
}

static void gbridge_free_requests(struct usb_ep *ep, struct list_head *head)
{
	struct usb_request	*req;

	while (!list_empty(head)) {
		req = list_entry(head->next, struct usb_request, list);
		list_del_init(&req->list);
		gbridge_free_req(ep, req);
	}
}

static struct usb_request *
gbridge_alloc_req(struct usb_ep *ep, unsigned len, gfp_t flags)
{
	struct usb_request *req;

	req = usb_ep_alloc_request(ep, flags);
	if (!req) {
		pr_err("usb alloc request failed\n");
		return 0;
	}

	req->length = len;
	req->buf = kmalloc(len, flags);
	if (!req->buf) {
		pr_err("request buf allocation failed\n");
		usb_ep_free_request(ep, req);
		return 0;
	}

	return req;
}

static int gbridge_alloc_requests(struct usb_ep *ep, struct list_head *head,
		int num, int size,
		void (*cb)(struct usb_ep *ep, struct usb_request *))
{
	int i;
	struct usb_request *req;

	pr_debug("ep:%pK head:%pK num:%d size:%d cb:%pK",
				ep, head, num, size, cb);

	for (i = 0; i < num; i++) {
		req = gbridge_alloc_req(ep, size, GFP_ATOMIC);
		if (!req) {
			pr_debug("req allocated:%d\n", i);
			return list_empty(head) ? -ENOMEM : 0;
		}
		req->complete = cb;
		list_add_tail(&req->list, head);
	}

	return 0;
}

static void gbridge_start_rx(struct gbridge_port *port)
{
	struct list_head	*pool;
	struct usb_ep		*ep;
	unsigned long		flags;
	int ret;

	pr_debug("start RX(USB OUT)\n");
	if (!port) {
		pr_err("port is null\n");
		return;
	}

	spin_lock_irqsave(&port->port_lock, flags);
	if (!(port->is_connected && port->port_open)) {
		spin_unlock_irqrestore(&port->port_lock, flags);
		pr_debug("can't start rx.\n");
		return;
	}

	pool = &port->read_pool;
	ep = port->port_usb->out;

	while (!list_empty(pool)) {
		struct usb_request	*req;

		req = list_entry(pool->next, struct usb_request, list);
		list_del_init(&req->list);
		req->length = BRIDGE_RX_BUF_SIZE;
		req->complete = gbridge_read_complete;
		spin_unlock_irqrestore(&port->port_lock, flags);
		ret = usb_ep_queue(ep, req, GFP_KERNEL);
		spin_lock_irqsave(&port->port_lock, flags);
		if (ret) {
			pr_err("port(%d):%pK usb ep(%s) queue failed\n",
					port->port_num, port, ep->name);
			list_add(&req->list, pool);
			break;
		}
	}

	spin_unlock_irqrestore(&port->port_lock, flags);
}

static void gbridge_read_complete(struct usb_ep *ep, struct usb_request *req)
{
	struct gbridge_port *port = ep->driver_data;
	unsigned long flags;

	pr_debug("ep:(%pK)(%s) port:%pK req_status:%d req->actual:%u\n",
			ep, ep->name, port, req->status, req->actual);
	if (!port) {
		pr_err("port is null\n");
		return;
	}

	spin_lock_irqsave(&port->port_lock, flags);
	if (!port->port_open || req->status || !req->actual) {
		list_add_tail(&req->list, &port->read_pool);
		spin_unlock_irqrestore(&port->port_lock, flags);
		return;
	}

	port->nbytes_from_host += req->actual;
	list_add_tail(&req->list, &port->read_queued);
	spin_unlock_irqrestore(&port->port_lock, flags);

	wake_up(&port->read_wq);
	return;
}

static void gbridge_write_complete(struct usb_ep *ep, struct usb_request *req)
{
	unsigned long flags;
	struct gbridge_port *port = ep->driver_data;

	pr_debug("ep:(%pK)(%s) port:%pK req_stats:%d\n",
			ep, ep->name, port, req->status);

	spin_lock_irqsave(&port->port_lock, flags);
	if (!port) {
		spin_unlock_irqrestore(&port->port_lock, flags);
		pr_err("port is null\n");
		return;
	}

	port->nbytes_to_host += req->actual;
	list_add_tail(&req->list, &port->write_pool);

	switch (req->status) {
	default:
		pr_debug("unexpected %s status %d\n", ep->name, req->status);
		/* FALL THROUGH */
	case 0:
		/* normal completion */
		break;

	case -ESHUTDOWN:
		/* disconnect */
		pr_debug("%s shutdown\n", ep->name);
		break;
	}

	spin_unlock_irqrestore(&port->port_lock, flags);
	return;
}

static void gbridge_start_io(struct gbridge_port *port)
{
	int ret = -ENODEV;
	unsigned long	flags;

	pr_debug("port: %pK\n", port);

	spin_lock_irqsave(&port->port_lock, flags);
	if (!port->port_usb)
		goto start_io_out;

	port->current_rx_req = NULL;
	port->pending_rx_bytes = 0;
	port->current_rx_buf = NULL;

	ret = gbridge_alloc_requests(port->port_usb->out,
				&port->read_pool,
				BRIDGE_RX_QUEUE_SIZE, BRIDGE_RX_BUF_SIZE,
				gbridge_read_complete);
	if (ret) {
		pr_err("unable to allocate out requests\n");
		goto start_io_out;
	}

	ret = gbridge_alloc_requests(port->port_usb->in,
				&port->write_pool,
				BRIDGE_TX_QUEUE_SIZE, BRIDGE_TX_BUF_SIZE,
				gbridge_write_complete);
	if (ret) {
		gbridge_free_requests(port->port_usb->out, &port->read_pool);
		pr_err("unable to allocate IN requests\n");
		goto start_io_out;
	}

start_io_out:
	spin_unlock_irqrestore(&port->port_lock, flags);
	if (ret)
		return;

	gbridge_start_rx(port);
}

static void gbridge_stop_io(struct gbridge_port *port)
{
	struct usb_ep	*in;
	struct usb_ep	*out;
	unsigned long	flags;

	pr_debug("port:%pK\n", port);
	spin_lock_irqsave(&port->port_lock, flags);
	if (!port->port_usb) {
		spin_unlock_irqrestore(&port->port_lock, flags);
		return;
	}
	in = port->port_usb->in;
	out = port->port_usb->out;
	spin_unlock_irqrestore(&port->port_lock, flags);

	/* disable endpoints, aborting down any active I/O */
	usb_ep_disable(out);
	out->driver_data = NULL;
	usb_ep_disable(in);
	in->driver_data = NULL;

	spin_lock_irqsave(&port->port_lock, flags);
	if (port->current_rx_req != NULL) {
		kfree(port->current_rx_req->buf);
		usb_ep_free_request(out, port->current_rx_req);
	}

	port->pending_rx_bytes = 0;
	port->current_rx_buf = NULL;
	gbridge_free_requests(out, &port->read_queued);
	gbridge_free_requests(out, &port->read_pool);
	gbridge_free_requests(in, &port->write_pool);
	spin_unlock_irqrestore(&port->port_lock, flags);
}

int gbridge_port_open(struct inode *inode, struct file *file)
{
	int ret;
	unsigned long flags;
	struct gbridge_port *port;

	port = container_of(inode->i_cdev, struct gbridge_port,
							gbridge_cdev);
	if (!port) {
		pr_err("Port is NULL.\n");
		return -EINVAL;
	}

	if (port && port->port_open) {
		pr_err("port is already opened.\n");
		return -EBUSY;
	}

	file->private_data = port;
	pr_debug("opening port(%pK)\n", port);
	ret = wait_event_interruptible(port->open_wq,
					port->is_connected);
	if (ret) {
		pr_debug("open interrupted.\n");
		return ret;
	}

	spin_lock_irqsave(&port->port_lock, flags);
	port->port_open = true;
	spin_unlock_irqrestore(&port->port_lock, flags);
	gbridge_start_rx(port);

	pr_debug("port(%pK) open is success\n", port);

	return 0;
}

int gbridge_port_release(struct inode *inode, struct file *file)
{
	unsigned long flags;
	struct gbridge_port *port;

	port = file->private_data;
	if (!port) {
		pr_err("port is NULL.\n");
		return -EINVAL;
	}

	pr_debug("closing port(%pK)\n", port);
	spin_lock_irqsave(&port->port_lock, flags);
	port->port_open = false;
	port->cbits_updated = false;
	spin_unlock_irqrestore(&port->port_lock, flags);
	pr_debug("port(%pK) is closed.\n", port);

	return 0;
}

ssize_t gbridge_port_read(struct file *file,
		       char __user *buf,
		       size_t count,
		       loff_t *ppos)
{
	unsigned long flags;
	struct gbridge_port *port;
	struct usb_request *req;
	struct list_head *pool;
	struct usb_request *current_rx_req;
	size_t pending_rx_bytes, bytes_copied = 0, size;
	u8 *current_rx_buf;

	port = file->private_data;
	if (!port) {
		pr_err("port is NULL.\n");
		return -EINVAL;
	}

	pr_debug("read on port(%pK) count:%zu\n", port, count);
	spin_lock_irqsave(&port->port_lock, flags);
	current_rx_req = port->current_rx_req;
	pending_rx_bytes = port->pending_rx_bytes;
	current_rx_buf = port->current_rx_buf;
	port->current_rx_req = NULL;
	port->current_rx_buf = NULL;
	port->pending_rx_bytes = 0;
	bytes_copied = 0;

	if (list_empty(&port->read_queued) && !pending_rx_bytes) {
		spin_unlock_irqrestore(&port->port_lock, flags);
		pr_debug("%s(): read_queued list is empty.\n", __func__);
		goto start_rx;
	}

	/*
	 * Consider below cases:
	 * 1. If available read buffer size (i.e. count value) is greater than
	 * available data as part of one USB OUT request buffer, then consider
	 * copying multiple USB OUT request buffers until read buffer is filled.
	 * 2. If available read buffer size (i.e. count value) is smaller than
	 * available data as part of one USB OUT request buffer, then copy this
	 * buffer data across multiple read() call until whole USB OUT request
	 * buffer is copied.
	 */
	while ((pending_rx_bytes || !list_empty(&port->read_queued)) && count) {
		if (pending_rx_bytes == 0) {
			pool = &port->read_queued;
			req = list_first_entry(pool, struct usb_request, list);
			list_del_init(&req->list);
			current_rx_req = req;
			pending_rx_bytes = req->actual;
			current_rx_buf = req->buf;
		}

		spin_unlock_irqrestore(&port->port_lock, flags);
		size = count;
		if (size > pending_rx_bytes)
			size = pending_rx_bytes;

		pr_debug("pending_rx_bytes:%zu count:%zu size:%zu\n",
					pending_rx_bytes, count, size);
		size -= copy_to_user(buf, current_rx_buf, size);
		port->nbytes_to_port_bridge += size;
		bytes_copied += size;
		count -= size;
		buf += size;

		spin_lock_irqsave(&port->port_lock, flags);
		if (!port->is_connected) {
			list_add_tail(&current_rx_req->list, &port->read_pool);
			spin_unlock_irqrestore(&port->port_lock, flags);
			return -EAGAIN;
		}

		/*
		 * partial data available, then update pending_rx_bytes,
		 * otherwise add USB request back to read_pool for next data.
		 */
		if (size < pending_rx_bytes) {
			pending_rx_bytes -= size;
			current_rx_buf += size;
		} else {
			list_add_tail(&current_rx_req->list, &port->read_pool);
			pending_rx_bytes = 0;
			current_rx_req = NULL;
			current_rx_buf = NULL;
		}
	}

	port->pending_rx_bytes = pending_rx_bytes;
	port->current_rx_buf = current_rx_buf;
	port->current_rx_req = current_rx_req;
	spin_unlock_irqrestore(&port->port_lock, flags);

start_rx:
	gbridge_start_rx(port);
	return bytes_copied;
}

ssize_t gbridge_port_write(struct file *file,
		       const char __user *buf,
		       size_t count,
		       loff_t *ppos)
{
	int ret;
	unsigned long flags;
	struct gbridge_port *port;
	struct usb_request *req;
	struct list_head *pool;
	unsigned xfer_size;
	struct usb_ep *in;

	port = file->private_data;
	if (!port) {
		pr_err("port is NULL.\n");
		return -EINVAL;
	}

	spin_lock_irqsave(&port->port_lock, flags);
	pr_debug("write on port(%pK)\n", port);

	if (!port->is_connected || !port->port_usb) {
		spin_unlock_irqrestore(&port->port_lock, flags);
		pr_err("%s: cable is disconnected.\n", __func__);
		return -ENODEV;
	}

	if (list_empty(&port->write_pool)) {
		spin_unlock_irqrestore(&port->port_lock, flags);
		pr_debug("%s: Request list is empty.\n", __func__);
		return 0;
	}

	in = port->port_usb->in;
	pool = &port->write_pool;
	req = list_first_entry(pool, struct usb_request, list);
	list_del_init(&req->list);
	spin_unlock_irqrestore(&port->port_lock, flags);

	pr_debug("%s: write buf size:%zu\n", __func__, count);
	if (count > BRIDGE_TX_BUF_SIZE)
		xfer_size = BRIDGE_TX_BUF_SIZE;
	else
		xfer_size = count;

	ret = copy_from_user(req->buf, buf, xfer_size);
	if (ret) {
		pr_err("copy_from_user failed: err %d\n", ret);
		ret = -EFAULT;
	} else {
		req->length = xfer_size;
		ret = usb_ep_queue(in, req, GFP_KERNEL);
		if (ret) {
			pr_err("EP QUEUE failed:%d\n", ret);
			ret = -EIO;
			goto err_exit;
		}
		spin_lock_irqsave(&port->port_lock, flags);
		port->nbytes_from_port_bridge += req->length;
		spin_unlock_irqrestore(&port->port_lock, flags);
	}

err_exit:
	if (ret) {
		spin_lock_irqsave(&port->port_lock, flags);
		/* USB cable is connected, add it back otherwise free request */
		if (port->is_connected)
			list_add(&req->list, &port->write_pool);
		else
			gbridge_free_req(in, req);
		spin_unlock_irqrestore(&port->port_lock, flags);
		return ret;
	}

	return xfer_size;
}

static unsigned int gbridge_port_poll(struct file *file, poll_table *wait)
{
	unsigned int mask = 0;
	struct gbridge_port *port;
	unsigned long flags;

	port = file->private_data;
	if (port && port->is_connected) {
		poll_wait(file, &port->read_wq, wait);
		spin_lock_irqsave(&port->port_lock, flags);
		if (!list_empty(&port->read_queued)) {
			mask |= POLLIN | POLLRDNORM;
			pr_debug("sets POLLIN for gbridge_port\n");
		}

		if (port->cbits_updated) {
			mask |= POLLPRI;
			pr_debug("sets POLLPRI for gbridge_port\n");
		}
		spin_unlock_irqrestore(&port->port_lock, flags);
	} else {
		pr_err("Failed due to NULL device or disconnected.\n");
		mask = POLLERR;
	}

	return mask;
}

static int gbridge_port_tiocmget(struct gbridge_port *port)
{
	struct gserial	*gser;
	unsigned int result = 0;
	unsigned long flags;

	if (!port) {
		pr_err("port is NULL.\n");
		return -ENODEV;
	}

	spin_lock_irqsave(&port->port_lock, flags);
	gser = port->port_usb;
	if (!gser) {
		pr_err("gser is null.\n");
		result = -ENODEV;
		goto fail;
	}

	if (gser->get_dtr)
		result |= (gser->get_dtr(gser) ? TIOCM_DTR : 0);

	if (gser->get_rts)
		result |= (gser->get_rts(gser) ? TIOCM_RTS : 0);

	if (gser->serial_state & TIOCM_CD)
		result |= TIOCM_CD;

	if (gser->serial_state & TIOCM_RI)
		result |= TIOCM_RI;

	if (gser->serial_state & TIOCM_DSR)
		result |= TIOCM_DSR;

	if (gser->serial_state & TIOCM_CTS)
		result |= TIOCM_CTS;
fail:
	spin_unlock_irqrestore(&port->port_lock, flags);
	return result;
}

static int gbridge_port_tiocmset(struct gbridge_port *port,
			unsigned int set, unsigned int clear)
{
	struct gserial *gser;
	int status = 0;
	unsigned long flags;

	if (!port) {
		pr_err("port is NULL.\n");
		return -ENODEV;
	}

	spin_lock_irqsave(&port->port_lock, flags);
	gser = port->port_usb;
	if (!gser) {
		pr_err("gser is NULL.\n");
		status = -ENODEV;
		goto fail;
	}

	if (set & TIOCM_RI) {
		if (gser->send_ring_indicator) {
			gser->serial_state |= TIOCM_RI;
			status = gser->send_ring_indicator(gser, 1);
		}
	}
	if (clear & TIOCM_RI) {
		if (gser->send_ring_indicator) {
			gser->serial_state &= ~TIOCM_RI;
			status = gser->send_ring_indicator(gser, 0);
		}
	}
	if (set & TIOCM_CD) {
		if (gser->send_carrier_detect) {
			gser->serial_state |= TIOCM_CD;
			status = gser->send_carrier_detect(gser, 1);
		}
	}
	if (clear & TIOCM_CD) {
		if (gser->send_carrier_detect) {
			gser->serial_state &= ~TIOCM_CD;
			status = gser->send_carrier_detect(gser, 0);
		}
	}
	if (set & TIOCM_DSR)
		gser->serial_state |= TIOCM_DSR;
	if (clear & TIOCM_DSR)
		gser->serial_state &= ~TIOCM_DSR;
	if (set & TIOCM_CTS) {
		if (gser->send_break) {
			gser->serial_state |= TIOCM_CTS;
			status = gser->send_break(gser, 0);
		}
	}
	if (clear & TIOCM_CTS) {
		if (gser->send_break) {
			gser->serial_state &= ~TIOCM_CTS;
			status = gser->send_break(gser, 1);
		}
	}
fail:
	spin_unlock_irqrestore(&port->port_lock, flags);
	return status;
}

static long gbridge_port_ioctl(struct file *fp, unsigned cmd,
						unsigned long arg)
{
	long ret = 0;
	int i = 0;
	uint32_t val;
	struct gbridge_port *port;

	port = fp->private_data;
	if (!port) {
		pr_err("port is null.\n");
		return POLLERR;
	}

	switch (cmd) {
	case TIOCMBIC:
	case TIOCMBIS:
	case TIOCMSET:
		pr_debug("TIOCMSET on port:%pK\n", port);
		i = get_user(val, (uint32_t *)arg);
		if (i) {
			pr_err("Error getting TIOCMSET value\n");
			return i;
		}
		ret = gbridge_port_tiocmset(port, val, ~val);
		break;
	case TIOCMGET:
		pr_debug("TIOCMGET on port:%pK\n", port);
		ret = gbridge_port_tiocmget(port);
		if (ret >= 0) {
			ret = put_user(ret, (uint32_t *)arg);
			port->cbits_updated = false;
		}
		break;
	default:
		pr_err("Received cmd:%d not supported\n", cmd);
		ret = -ENOIOCTLCMD;
		break;
	}

	return ret;
}

static void gbridge_notify_modem(void *gptr, u8 portno, int ctrl_bits)
{
	struct gbridge_port *port;
	int temp;
	struct gserial *gser = gptr;
	unsigned long flags;

	pr_debug("portno:%d ctrl_bits:%x\n", portno, ctrl_bits);
	if (!gser) {
		pr_err("gser is null\n");
		return;
	}

	port = ports[portno];
	spin_lock_irqsave(&port->port_lock, flags);
	temp = convert_acm_sigs_to_uart(ctrl_bits);

	if (temp == port->cbits_to_modem) {
		spin_unlock_irqrestore(&port->port_lock, flags);
		return;
	}

	port->cbits_to_modem = temp;
	port->cbits_updated = true;
	spin_unlock_irqrestore(&port->port_lock, flags);
	/* if DTR is high, update latest modem info to laptop */
	if (port->cbits_to_modem & TIOCM_DTR) {
		unsigned int result;
		unsigned cbits_to_laptop;

		result = gbridge_port_tiocmget(port);
		cbits_to_laptop = convert_uart_sigs_to_acm(result);
		if (gser->send_modem_ctrl_bits)
			gser->send_modem_ctrl_bits(
					port->port_usb, cbits_to_laptop);
	}

	wake_up(&port->read_wq);
}

#if defined(CONFIG_DEBUG_FS)
static ssize_t debug_gbridge_read_stats(struct file *file, char __user *ubuf,
		size_t count, loff_t *ppos)
{
	struct gbridge_port *port;
	char *buf;
	unsigned long flags;
	int temp = 0;
	int i;
	int ret;

	buf = kzalloc(sizeof(char) * 512, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	for (i = 0; i < n_bridge_ports; i++) {
		port = ports[i];
		spin_lock_irqsave(&port->port_lock, flags);
		temp += scnprintf(buf + temp, 512 - temp,
				"###PORT:%d###\n"
				"nbytes_to_host: %lu\n"
				"nbytes_from_host: %lu\n"
				"nbytes_to_port_bridge:  %lu\n"
				"nbytes_from_port_bridge: %lu\n"
				"cbits_to_modem:  %u\n"
				"Port Opened: %s\n",
				i, port->nbytes_to_host,
				port->nbytes_from_host,
				port->nbytes_to_port_bridge,
				port->nbytes_from_port_bridge,
				port->cbits_to_modem,
				(port->port_open ? "Opened" : "Closed"));
		spin_unlock_irqrestore(&port->port_lock, flags);
	}

	ret = simple_read_from_buffer(ubuf, count, ppos, buf, temp);
	kfree(buf);

	return ret;
}

static ssize_t debug_gbridge_reset_stats(struct file *file,
				const char __user *buf,
				size_t count, loff_t *ppos)
{
	struct gbridge_port *port;
	unsigned long flags;
	int i;

	for (i = 0; i < n_bridge_ports; i++) {
		port = ports[i];
		spin_lock_irqsave(&port->port_lock, flags);
		port->nbytes_to_host = port->nbytes_from_host = 0;
		port->nbytes_to_port_bridge = port->nbytes_from_port_bridge = 0;
		spin_unlock_irqrestore(&port->port_lock, flags);
	}

	return count;
}

static ssize_t gbridge_rw_write(struct file *file, const char __user *ubuf,
					size_t count, loff_t *ppos)
{
	struct gbridge_port *ui_dev = ports[0];
	struct gserial *gser;
	struct usb_function *func;
	struct usb_gadget   *gadget;

	if (!ui_dev) {
		pr_err("%s ui_dev is NULL\n", __func__);
		return -EINVAL;
	}

	gser = ui_dev->port_usb;
	if (!gser) {
		pr_err("%s gser is NULL\n", __func__);
		return -EINVAL;
	}

	func = &gser->func;
	if (!func) {
		pr_err("%s func is NULL\n", __func__);
		return -EINVAL;
	}

	gadget = gser->func.config->cdev->gadget;
	if ((gadget->speed == USB_SPEED_SUPER) && (func->func_is_suspended)) {
		pr_debug("%s Calling usb_func_wakeup\n", __func__);
		usb_func_wakeup(func);
	}

	return count;
}

static int debug_gbridge_open(struct inode *inode, struct file *file)
{
	return 0;
}

static const struct file_operations debug_gbridge_ops = {
	.open = debug_gbridge_open,
	.read = debug_gbridge_read_stats,
	.write = debug_gbridge_reset_stats,
};

const struct file_operations gbridge_rem_wakeup_fops = {
	.open = debug_gbridge_open,
	.write = gbridge_rw_write,
};

static void gbridge_debugfs_init(void)
{
	struct dentry *dent;

	dent = debugfs_create_dir("usb_gbridge", 0);
	if (IS_ERR(dent))
		return;

	debugfs_create_file("status", 0444, dent, 0, &debug_gbridge_ops);
	debugfs_create_file("remote_wakeup", S_IWUSR,
				dent, 0, &gbridge_rem_wakeup_fops);
}

#else
static void gbridge_debugfs_init(void) {}
#endif

int gbridge_setup(void *gptr, u8 no_ports)
{
	pr_debug("gptr:%pK, no_bridge_ports:%d\n", gptr, no_ports);
	if (no_ports > num_of_instance) {
		pr_err("More ports are requested\n");
		return -EINVAL;
	}

	n_bridge_ports = no_ports;
	gbridge_debugfs_init();
	return 0;
}

int gbridge_connect(void *gptr, u8 portno)
{
	unsigned long flags;
	int ret;
	struct gserial *gser;
	struct gbridge_port *port;

	if (!gptr) {
		pr_err("gptr is null\n");
		return -EINVAL;
	}

	pr_debug("gbridge:%pK portno:%u\n", gptr, portno);
	port = ports[portno];
	gser = gptr;

	spin_lock_irqsave(&port->port_lock, flags);
	port->port_usb = gser;
	gser->notify_modem = gbridge_notify_modem;
	spin_unlock_irqrestore(&port->port_lock, flags);

	ret = usb_ep_enable(gser->in);
	if (ret) {
		pr_err("usb_ep_enable failed eptype:IN ep:%pK, err:%d",
					gser->in, ret);
		port->port_usb = 0;
		return ret;
	}
	gser->in->driver_data = port;

	ret = usb_ep_enable(gser->out);
	if (ret) {
		pr_err("usb_ep_enable failed eptype:OUT ep:%pK, err: %d",
					gser->out, ret);
		port->port_usb = 0;
		gser->in->driver_data = 0;
		return ret;
	}
	gser->out->driver_data = port;

	spin_lock_irqsave(&port->port_lock, flags);
	port->is_connected = true;
	spin_unlock_irqrestore(&port->port_lock, flags);

	gbridge_start_io(port);
	wake_up(&port->open_wq);
	return 0;
}

void gbridge_disconnect(void *gptr, u8 portno)
{
	unsigned long flags;
	struct gserial *gser;
	struct gbridge_port *port;

	if (!gptr) {
		pr_err("gptr is null\n");
		return;
	}

	pr_debug("gptr:%pK portno:%u\n", gptr, portno);
	if (portno >= num_of_instance) {
		pr_err("Wrong port no %d\n", portno);
		return;
	}

	port = ports[portno];
	gser = gptr;

	gbridge_stop_io(port);

	/* lower DTR to modem */
	gbridge_notify_modem(gser, portno, 0);

	spin_lock_irqsave(&port->port_lock, flags);
	port->is_connected = false;
	gser->notify_modem = NULL;
	port->port_usb = NULL;
	port->nbytes_from_host = port->nbytes_to_host = 0;
	port->nbytes_to_port_bridge = 0;
	spin_unlock_irqrestore(&port->port_lock, flags);
}

static void gbridge_port_free(int portno)
{
	if (portno >= num_of_instance) {
		pr_err("Wrong portno %d\n", portno);
		return;
	}

	kfree(ports[portno]);
}
static int gbridge_port_alloc(int portno)
{
	int ret;

	ports[portno] = kzalloc(sizeof(struct gbridge_port), GFP_KERNEL);
	if (!ports[portno]) {
		pr_err("Unable to allocate memory for port(%d)\n", portno);
		ret = -ENOMEM;
		return  ret;
	}

	ports[portno]->port_num = portno;
	snprintf(ports[portno]->name, sizeof(ports[portno]->name),
			"%s%d", DEVICE_NAME, portno);
	spin_lock_init(&ports[portno]->port_lock);

	init_waitqueue_head(&ports[portno]->open_wq);
	init_waitqueue_head(&ports[portno]->read_wq);
	INIT_LIST_HEAD(&ports[portno]->read_pool);
	INIT_LIST_HEAD(&ports[portno]->read_queued);
	INIT_LIST_HEAD(&ports[portno]->write_pool);
	pr_debug("port:%pK portno:%d\n", ports[portno], portno);
	return 0;
}

static const struct file_operations gbridge_port_fops = {
	.owner = THIS_MODULE,
	.open = gbridge_port_open,
	.release = gbridge_port_release,
	.read = gbridge_port_read,
	.write = gbridge_port_write,
	.poll = gbridge_port_poll,
	.unlocked_ioctl = gbridge_port_ioctl,
	.compat_ioctl = gbridge_port_ioctl,
};

static void gbridge_chardev_deinit(void)
{
	int i;

	for (i = 0; i < num_of_instance; i++) {
		cdev_del(&ports[i]->gbridge_cdev);
		gbridge_port_free(i);
	}

	if (!IS_ERR_OR_NULL(gbridge_classp))
		class_destroy(gbridge_classp);
	unregister_chrdev_region(MAJOR(gbridge_number), num_of_instance);
}

static int gbridge_alloc_chardev_region(void)
{
	int ret;

	ret = alloc_chrdev_region(&gbridge_number,
			       0,
			       num_of_instance,
			       MODULE_NAME);
	if (IS_ERR_VALUE(ret)) {
		pr_err("alloc_chrdev_region() failed ret:%i\n", ret);
		return ret;
	}

	gbridge_classp = class_create(THIS_MODULE, MODULE_NAME);
	if (IS_ERR(gbridge_classp)) {
		pr_err("class_create() failed ENOMEM\n");
		ret = -ENOMEM;
	}

	return 0;
}

static int __init gbridge_init(void)
{
	int ret, i;
	struct device *devicep;
	struct gbridge_port *cur_port;

	gbridge_wq = create_singlethread_workqueue("k_gbridge");
	if (!gbridge_wq) {
		pr_err("Unable to create workqueue gbridge_wq\n");
		return -ENOMEM;
	}

	ret = gbridge_alloc_chardev_region();
	if (ret) {
		pr_err("gbridge_alloc_chardev_region() failed ret:%d\n", ret);
		destroy_workqueue(gbridge_wq);
		return ret;
	}

	for (i = 0; i < num_of_instance; i++) {
		gbridge_port_alloc(i);
		cur_port = ports[i];
		cdev_init(&cur_port->gbridge_cdev, &gbridge_port_fops);
		cur_port->gbridge_cdev.owner = THIS_MODULE;

		ret = cdev_add(&cur_port->gbridge_cdev, gbridge_number + i, 1);
		if (IS_ERR_VALUE(ret)) {
			pr_err("cdev_add() failed ret:%d\n", ret);
			unregister_chrdev_region(MAJOR(gbridge_number),
							num_of_instance);
			return ret;
		}

		devicep = device_create(gbridge_classp,	NULL,
					gbridge_number + i, cur_port->dev,
					cur_port->name);
		if (IS_ERR_OR_NULL(devicep)) {
			pr_err("device_create() failed for port(%d)\n", i);
			ret = -ENOMEM;
			cdev_del(&cur_port->gbridge_cdev);
			return ret;
		}
	}

	pr_info("gbridge_init successs.\n");
	return 0;
}
module_init(gbridge_init);

static void __exit gbridge_exit(void)
{
	gbridge_chardev_deinit();
}
module_exit(gbridge_exit);
MODULE_DESCRIPTION("Port Bridge DUN character Driver");
MODULE_LICENSE("GPL v2");
