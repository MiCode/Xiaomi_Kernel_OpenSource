/*
 * rmnet.c -- RmNet function driver
 *
 * Copyright (C) 2003-2005,2008 David Brownell
 * Copyright (C) 2003-2004 Robert Schwebel, Benedikt Spranger
 * Copyright (C) 2003 Al Borchers (alborchers@steinerpoint.com)
 * Copyright (C) 2008 Nokia Corporation
 * Copyright (c) 2010, Code Aurora Forum. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/err.h>
#include <linux/interrupt.h>
#include <linux/kfifo.h>

#include <mach/msm_smd.h>
#include <linux/usb/cdc.h>

#include "usb_function.h"

static char *rmnet_ctl_ch = CONFIG_RMNET_SMD_CTL_CHANNEL;
module_param(rmnet_ctl_ch, charp, S_IRUGO);
MODULE_PARM_DESC(rmnet_ctl_ch, "RmNet control SMD channel");

static char *rmnet_data_ch = CONFIG_RMNET_SMD_DATA_CHANNEL;
module_param(rmnet_data_ch, charp, S_IRUGO);
MODULE_PARM_DESC(rmnet_data_ch, "RmNet data SMD channel");

#define RMNET_NOTIFY_INTERVAL	5
#define RMNET_MAX_NOTIFY_SIZE	sizeof(struct usb_cdc_notification)

#define QMI_REQ_MAX		4
#define QMI_REQ_SIZE		2048
#define QMI_RESP_MAX		8
#define QMI_RESP_SIZE		2048

#define RX_REQ_MAX		8
#define RX_REQ_SIZE		2048
#define TX_REQ_MAX		8
#define TX_REQ_SIZE		2048

#define TXN_MAX 		2048

static struct usb_interface_descriptor rmnet_interface_desc = {
	.bLength =		USB_DT_INTERFACE_SIZE,
	.bDescriptorType =	USB_DT_INTERFACE,
	/* .bInterfaceNumber = DYNAMIC */
	.bNumEndpoints =	3,
	.bInterfaceClass =	USB_CLASS_VENDOR_SPEC,
	.bInterfaceSubClass =	USB_CLASS_VENDOR_SPEC,
	.bInterfaceProtocol =	USB_CLASS_VENDOR_SPEC,
	/* .iInterface = DYNAMIC */
};

/* Full speed support */
static struct usb_endpoint_descriptor rmnet_fs_notify_desc = {
	.bLength =		USB_DT_ENDPOINT_SIZE,
	.bDescriptorType =	USB_DT_ENDPOINT,
	.bEndpointAddress =	USB_DIR_IN,
	.bmAttributes =		USB_ENDPOINT_XFER_INT,
	.wMaxPacketSize =	__constant_cpu_to_le16(RMNET_MAX_NOTIFY_SIZE),
	.bInterval =		1 << RMNET_NOTIFY_INTERVAL,
};

static struct usb_endpoint_descriptor rmnet_fs_in_desc  = {
	.bLength =		USB_DT_ENDPOINT_SIZE,
	.bDescriptorType =	USB_DT_ENDPOINT,
	.bEndpointAddress =	USB_DIR_IN,
	.bmAttributes =		USB_ENDPOINT_XFER_BULK,
	.wMaxPacketSize   = __constant_cpu_to_le16(64),
};

static struct usb_endpoint_descriptor rmnet_fs_out_desc = {
	.bLength =		USB_DT_ENDPOINT_SIZE,
	.bDescriptorType =	USB_DT_ENDPOINT,
	.bEndpointAddress =	USB_DIR_OUT,
	.bmAttributes =		USB_ENDPOINT_XFER_BULK,
	.wMaxPacketSize   = __constant_cpu_to_le16(64),
};

/* High speed support */
static struct usb_endpoint_descriptor rmnet_hs_notify_desc  = {
	.bLength =		USB_DT_ENDPOINT_SIZE,
	.bDescriptorType =	USB_DT_ENDPOINT,
	.bEndpointAddress =	USB_DIR_IN,
	.bmAttributes =		USB_ENDPOINT_XFER_INT,
	.wMaxPacketSize =	__constant_cpu_to_le16(RMNET_MAX_NOTIFY_SIZE),
	.bInterval =		RMNET_NOTIFY_INTERVAL + 4,
};

static struct usb_endpoint_descriptor rmnet_hs_in_desc = {
	.bLength =		USB_DT_ENDPOINT_SIZE,
	.bDescriptorType =	USB_DT_ENDPOINT,
	.bEndpointAddress =	USB_DIR_IN,
	.bmAttributes =		USB_ENDPOINT_XFER_BULK,
	.wMaxPacketSize =	__constant_cpu_to_le16(512),
};

static struct usb_endpoint_descriptor rmnet_hs_out_desc = {
	.bLength =		USB_DT_ENDPOINT_SIZE,
	.bDescriptorType =	USB_DT_ENDPOINT,
	.bEndpointAddress =	USB_DIR_OUT,
	.bmAttributes =		USB_ENDPOINT_XFER_BULK,
	.wMaxPacketSize =	__constant_cpu_to_le16(512),
};

/* QMI requests & responses buffer*/
struct qmi_buf {
	void *buf;
	int len;
	struct list_head list;
};

/* Control & data SMD channel private data */
struct rmnet_smd_info {
	struct smd_channel 	*ch;
	struct tasklet_struct	tx_tlet;
	struct tasklet_struct	rx_tlet;
#define CH_OPENED	0
	unsigned long		flags;
	/* pending rx packet length */
	atomic_t		rx_pkt;
	/* wait for smd open event*/
	wait_queue_head_t	wait;
};

struct rmnet_dev {
	struct usb_endpoint	*epout;
	struct usb_endpoint	*epin;
	struct usb_endpoint	*epnotify;
	struct usb_request 	*notify_req;

	u8			ifc_id;
	/* QMI lists */
	struct list_head	qmi_req_pool;
	struct list_head	qmi_resp_pool;
	struct list_head	qmi_req_q;
	struct list_head	qmi_resp_q;
	/* Tx/Rx lists */
	struct list_head 	tx_idle;
	struct list_head 	rx_idle;
	struct list_head	rx_queue;

	spinlock_t		lock;
	atomic_t		online;
	atomic_t		notify_count;

	struct rmnet_smd_info	smd_ctl;
	struct rmnet_smd_info	smd_data;

	struct workqueue_struct *wq;
	struct work_struct connect_work;
	struct work_struct disconnect_work;
};

static struct usb_function rmnet_function;

struct qmi_buf *
rmnet_alloc_qmi(unsigned len, gfp_t kmalloc_flags)
{
	struct qmi_buf *qmi;

	qmi = kmalloc(sizeof(struct qmi_buf), kmalloc_flags);
	if (qmi != NULL) {
		qmi->buf = kmalloc(len, kmalloc_flags);
		if (qmi->buf == NULL) {
			kfree(qmi);
			qmi = NULL;
		}
	}

	return qmi ? qmi : ERR_PTR(-ENOMEM);
}

void rmnet_free_qmi(struct qmi_buf *qmi)
{
	kfree(qmi->buf);
	kfree(qmi);
}
/*
 * Allocate a usb_request and its buffer.  Returns a pointer to the
 * usb_request or NULL if there is an error.
 */
struct usb_request *
rmnet_alloc_req(struct usb_endpoint *ep, unsigned len, gfp_t kmalloc_flags)
{
	struct usb_request *req;

	req = usb_ept_alloc_req(ep, 0);

	if (req != NULL) {
		req->length = len;
		req->buf = kmalloc(len, kmalloc_flags);
		if (req->buf == NULL) {
			usb_ept_free_req(ep, req);
			req = NULL;
		}
	}

	return req ? req : ERR_PTR(-ENOMEM);
}

/*
 * Free a usb_request and its buffer.
 */
void rmnet_free_req(struct usb_endpoint *ep, struct usb_request *req)
{
	kfree(req->buf);
	usb_ept_free_req(ep, req);
}

static void rmnet_notify_complete(struct usb_endpoint *ep,
		struct usb_request *req)
{
	struct rmnet_dev *dev = req->context;
	int status = req->status;

	switch (status) {
	case -ECONNRESET:
	case -ESHUTDOWN:
	case -ENODEV:
		/* connection gone */
		atomic_set(&dev->notify_count, 0);
		break;
	default:
		pr_err("%s: rmnet notify ep error %d\n", __func__, status);
		/* FALLTHROUGH */
	case 0:
		if (ep != dev->epnotify)
			break;

		/* handle multiple pending QMI_RESPONSE_AVAILABLE
		 * notifications by resending until we're done
		 */
		if (atomic_dec_and_test(&dev->notify_count))
			break;

		status = usb_ept_queue_xfer(dev->epnotify, dev->notify_req);
		if (status) {
			atomic_dec(&dev->notify_count);
			pr_err("%s: rmnet notify ep enqueue error %d\n",
					__func__, status);
		}
		break;
	}
}

static void qmi_response_available(struct rmnet_dev *dev)
{
	struct usb_request		*req = dev->notify_req;
	struct usb_cdc_notification	*event = req->buf;
	int status;

	/* Response will be sent later */
	if (atomic_inc_return(&dev->notify_count) != 1)
		return;

	event->bmRequestType = USB_DIR_IN | USB_TYPE_CLASS
			| USB_RECIP_INTERFACE;
	event->bNotificationType = USB_CDC_NOTIFY_RESPONSE_AVAILABLE;
	event->wValue = cpu_to_le16(0);
	event->wIndex = cpu_to_le16(dev->ifc_id);
	event->wLength = cpu_to_le16(0);

	status = usb_ept_queue_xfer(dev->epnotify, dev->notify_req);
	if (status < 0) {
		atomic_dec(&dev->notify_count);
		pr_err("%s: rmnet notify ep enqueue error %d\n",
				__func__, status);
	}
}

/* TODO
 * handle modem restart events
 */
static void rmnet_smd_notify(void *priv, unsigned event)
{
	struct rmnet_smd_info *smd_info = priv;
	int len = atomic_read(&smd_info->rx_pkt);

	switch (event) {
	case SMD_EVENT_DATA: {

		if (len && (smd_write_avail(smd_info->ch) >= len))
			tasklet_schedule(&smd_info->rx_tlet);

		if (smd_read_avail(smd_info->ch))
			tasklet_schedule(&smd_info->tx_tlet);

		break;
	}
	case SMD_EVENT_OPEN:
		/* usb endpoints are not enabled untill smd channels
		 * are opened. wake up worker thread to continue
		 * connection processing
		 */
		set_bit(CH_OPENED, &smd_info->flags);
		wake_up(&smd_info->wait);
		break;
	case SMD_EVENT_CLOSE:
		/* We will never come here.
		 * reset flags after closing smd channel
		 * */
		clear_bit(CH_OPENED, &smd_info->flags);
		break;
	}
}

static void rmnet_control_tx_tlet(unsigned long arg)
{
	struct rmnet_dev *dev = (struct rmnet_dev *) arg;
	struct qmi_buf *qmi_resp;
	int sz;
	unsigned long flags;

	while (1) {
		sz = smd_cur_packet_size(dev->smd_ctl.ch);
		if (sz == 0)
			break;
		if (smd_read_avail(dev->smd_ctl.ch) < sz)
			break;

		spin_lock_irqsave(&dev->lock, flags);
		if (list_empty(&dev->qmi_resp_pool)) {
			pr_err("%s: rmnet QMI Tx buffers full\n", __func__);
			spin_unlock_irqrestore(&dev->lock, flags);
			break;
		}
		qmi_resp = list_first_entry(&dev->qmi_resp_pool,
				struct qmi_buf, list);
		list_del(&qmi_resp->list);
		spin_unlock_irqrestore(&dev->lock, flags);

		qmi_resp->len = smd_read(dev->smd_ctl.ch, qmi_resp->buf, sz);

		spin_lock_irqsave(&dev->lock, flags);
		list_add_tail(&qmi_resp->list, &dev->qmi_resp_q);
		spin_unlock_irqrestore(&dev->lock, flags);

		qmi_response_available(dev);
	}

}

static void rmnet_control_rx_tlet(unsigned long arg)
{
	struct rmnet_dev *dev = (struct rmnet_dev *) arg;
	struct qmi_buf *qmi_req;
	int ret;
	unsigned long flags;

	spin_lock_irqsave(&dev->lock, flags);
	while (1) {

		if (list_empty(&dev->qmi_req_q)) {
			atomic_set(&dev->smd_ctl.rx_pkt, 0);
			break;
		}
		qmi_req = list_first_entry(&dev->qmi_req_q,
				struct qmi_buf, list);
		if (smd_write_avail(dev->smd_ctl.ch) < qmi_req->len) {
			atomic_set(&dev->smd_ctl.rx_pkt, qmi_req->len);
			pr_debug("%s: rmnet control smd channel full\n",
					__func__);
			break;
		}

		list_del(&qmi_req->list);
		spin_unlock_irqrestore(&dev->lock, flags);
		ret = smd_write(dev->smd_ctl.ch, qmi_req->buf, qmi_req->len);
		spin_lock_irqsave(&dev->lock, flags);
		if (ret != qmi_req->len) {
			pr_err("%s: rmnet control smd write failed\n",
					__func__);
			break;
		}

		list_add_tail(&qmi_req->list, &dev->qmi_req_pool);
	}
	spin_unlock_irqrestore(&dev->lock, flags);
}

static void rmnet_command_complete(struct usb_endpoint *ep,
		struct usb_request *req)
{
	struct rmnet_dev *dev = req->context;
	struct usb_function *func = &rmnet_function;
	struct usb_request *in_req;
	struct qmi_buf *qmi_req;
	int ret;

	if (req->status < 0) {
		pr_err("%s: rmnet command error %d\n", __func__, req->status);
		return;
	}

	spin_lock(&dev->lock);
	/* no pending control rx packet */
	if (!atomic_read(&dev->smd_ctl.rx_pkt)) {
		if (smd_write_avail(dev->smd_ctl.ch) < req->actual) {
			atomic_set(&dev->smd_ctl.rx_pkt, req->actual);
			goto queue_req;
		}
		spin_unlock(&dev->lock);
		ret = smd_write(dev->smd_ctl.ch, req->buf, req->actual);
		/* This should never happen */
		if (ret != req->actual)
			pr_err("%s: rmnet control smd write failed\n",
					__func__);
		goto ep0_ack;
	}
queue_req:
	if (list_empty(&dev->qmi_req_pool)) {
		spin_unlock(&dev->lock);
		pr_err("%s: rmnet QMI pool is empty\n", __func__);
		return;
	}

	qmi_req = list_first_entry(&dev->qmi_req_pool, struct qmi_buf, list);
	list_del(&qmi_req->list);
	spin_unlock(&dev->lock);
	memcpy(qmi_req->buf, req->buf, req->actual);
	qmi_req->len = req->actual;
	spin_lock(&dev->lock);
	list_add_tail(&qmi_req->list, &dev->qmi_req_q);
	spin_unlock(&dev->lock);
ep0_ack:
	/* Send ACK on EP0 IN */
	in_req = func->ep0_in_req;
	in_req->length = 0;
	in_req->complete = 0;
	usb_ept_queue_xfer(func->ep0_in, in_req);
}

static int rmnet_setup(struct usb_ctrlrequest *ctrl, void *buf,
				int len, void *context)
{
	struct rmnet_dev *dev = context;
	struct usb_request *req = rmnet_function.ep0_out_req;
	int			ret = -EOPNOTSUPP;
	u16			w_index = le16_to_cpu(ctrl->wIndex);
	u16			w_value = le16_to_cpu(ctrl->wValue);
	u16			w_length = le16_to_cpu(ctrl->wLength);
	struct qmi_buf *resp;
	int schedule = 0;

	if (!atomic_read(&dev->online))
		return -ENOTCONN;

	switch ((ctrl->bRequestType << 8) | ctrl->bRequest) {

	case ((USB_DIR_OUT | USB_TYPE_CLASS | USB_RECIP_INTERFACE) << 8)
			| USB_CDC_SEND_ENCAPSULATED_COMMAND:
		if (w_value || w_index != dev->ifc_id)
			goto invalid;
		ret = w_length;
		req->complete = rmnet_command_complete;
		req->context = dev;
		break;


	case ((USB_DIR_IN | USB_TYPE_CLASS | USB_RECIP_INTERFACE) << 8)
			| USB_CDC_GET_ENCAPSULATED_RESPONSE:
		if (w_value || w_index != dev->ifc_id)
			goto invalid;
		else {
			spin_lock(&dev->lock);
			resp = list_first_entry(&dev->qmi_resp_q,
					struct qmi_buf, list);
			list_del(&resp->list);
			spin_unlock(&dev->lock);
			memcpy(buf, resp->buf, resp->len);
			ret = resp->len;
			spin_lock(&dev->lock);

			if (list_empty(&dev->qmi_resp_pool))
				schedule = 1;
			list_add_tail(&resp->list, &dev->qmi_resp_pool);

			if (schedule)
				tasklet_schedule(&dev->smd_ctl.tx_tlet);
			spin_unlock(&dev->lock);
		}
		break;
	default:

invalid:
		pr_debug("%s: invalid control req%02x.%02x v%04x i%04x l%d\n",
			__func__, ctrl->bRequestType, ctrl->bRequest,
			w_value, w_index, w_length);
	}

	return ret;
}

static void rmnet_start_rx(struct rmnet_dev *dev)
{
	int status;
	struct usb_request *req;
	struct list_head *act, *tmp;
	unsigned long flags;

	spin_lock_irqsave(&dev->lock, flags);
	list_for_each_safe(act, tmp, &dev->rx_idle) {
		req = list_entry(act, struct usb_request, list);
		list_del(&req->list);

		spin_unlock_irqrestore(&dev->lock, flags);
		status = usb_ept_queue_xfer(dev->epout, req);
		spin_lock_irqsave(&dev->lock, flags);

		if (status) {
			pr_err("%s: rmnet data rx enqueue err %d\n",
					__func__, status);
			list_add_tail(&req->list, &dev->rx_idle);
			break;
		}
	}
	spin_unlock_irqrestore(&dev->lock, flags);
}

static void rmnet_data_tx_tlet(unsigned long arg)
{
	struct rmnet_dev *dev = (struct rmnet_dev *) arg;
	struct usb_request *req;
	int status;
	int sz;
	unsigned long flags;

	while (1) {

		sz = smd_cur_packet_size(dev->smd_data.ch);
		if (sz == 0)
			break;
		if (smd_read_avail(dev->smd_data.ch) < sz)
			break;

		spin_lock_irqsave(&dev->lock, flags);
		if (list_empty(&dev->tx_idle)) {
			spin_unlock_irqrestore(&dev->lock, flags);
			pr_debug("%s: rmnet data Tx buffers full\n", __func__);
			break;
		}
		req = list_first_entry(&dev->tx_idle, struct usb_request, list);
		list_del(&req->list);
		spin_unlock_irqrestore(&dev->lock, flags);

		req->length = smd_read(dev->smd_data.ch, req->buf, sz);
		status = usb_ept_queue_xfer(dev->epin, req);
		if (status) {
			pr_err("%s: rmnet tx data enqueue err %d\n",
					__func__, status);
			spin_lock_irqsave(&dev->lock, flags);
			list_add_tail(&req->list, &dev->tx_idle);
			spin_unlock_irqrestore(&dev->lock, flags);
			break;
		}
	}

}

static void rmnet_data_rx_tlet(unsigned long arg)
{
	struct rmnet_dev *dev = (struct rmnet_dev *) arg;
	struct usb_request *req;
	int ret;
	unsigned long flags;

	spin_lock_irqsave(&dev->lock, flags);
	while (1) {
		if (list_empty(&dev->rx_queue)) {
			atomic_set(&dev->smd_data.rx_pkt, 0);
			break;
		}
		req = list_first_entry(&dev->rx_queue,
			struct usb_request, list);
		if (smd_write_avail(dev->smd_data.ch) < req->actual) {
			atomic_set(&dev->smd_data.rx_pkt, req->actual);
			pr_debug("%s: rmnet SMD data channel full\n", __func__);
			break;
		}

		list_del(&req->list);
		spin_unlock_irqrestore(&dev->lock, flags);
		ret = smd_write(dev->smd_data.ch, req->buf, req->actual);
		spin_lock_irqsave(&dev->lock, flags);
		if (ret != req->actual) {
			pr_err("%s: rmnet SMD data write failed\n", __func__);
			break;
		}
		list_add_tail(&req->list, &dev->rx_idle);
	}
	spin_unlock_irqrestore(&dev->lock, flags);

	/* We have free rx data requests. */
	rmnet_start_rx(dev);
}

/* If SMD has enough room to accommodate a data rx packet,
 * write into SMD directly. Otherwise enqueue to rx_queue.
 * We will not write into SMD directly untill rx_queue is
 * empty to strictly follow the ordering requests.
 */
static void rmnet_complete_epout(struct usb_endpoint *ep,
		struct usb_request *req)
{
	struct rmnet_dev *dev = req->context;
	int status = req->status;
	int ret;

	switch (status) {
	case 0:
		/* normal completion */
		break;
	case -ECONNRESET:
	case -ESHUTDOWN:
	case -ENODEV:
		/* connection gone */
		spin_lock(&dev->lock);
		list_add_tail(&req->list, &dev->rx_idle);
		spin_unlock(&dev->lock);
		return;
	default:
		/* unexpected failure */
		pr_err("%s: response error %d, %d/%d\n",
			__func__, status, req->actual,
			req->length);
		spin_lock(&dev->lock);
		list_add_tail(&req->list, &dev->rx_idle);
		spin_unlock(&dev->lock);
		return;
	}

	spin_lock(&dev->lock);
	if (!atomic_read(&dev->smd_data.rx_pkt)) {
		if (smd_write_avail(dev->smd_data.ch) < req->actual) {
			atomic_set(&dev->smd_data.rx_pkt, req->actual);
			goto queue_req;
		}
		spin_unlock(&dev->lock);
		ret = smd_write(dev->smd_data.ch, req->buf, req->actual);
		/* This should never happen */
		if (ret != req->actual)
			pr_err("%s: rmnet data smd write failed\n", __func__);
		/* Restart Rx */
		spin_lock(&dev->lock);
		list_add_tail(&req->list, &dev->rx_idle);
		spin_unlock(&dev->lock);
		rmnet_start_rx(dev);
		return;
	}
queue_req:
	list_add_tail(&req->list, &dev->rx_queue);
	spin_unlock(&dev->lock);
}

static void rmnet_complete_epin(struct usb_endpoint *ep,
		struct usb_request *req)
{
	struct rmnet_dev *dev = req->context;
	int status = req->status;
	int schedule = 0;

	switch (status) {
	case -ECONNRESET:
	case -ESHUTDOWN:
	case -ENODEV:
		/* connection gone */
		spin_lock(&dev->lock);
		list_add_tail(&req->list, &dev->tx_idle);
		spin_unlock(&dev->lock);
		break;
	default:
		pr_err("%s: rmnet data tx ep error %d\n", __func__, status);
		/* FALLTHROUGH */
	case 0:
		spin_lock(&dev->lock);
		if (list_empty(&dev->tx_idle))
			schedule = 1;
		list_add_tail(&req->list, &dev->tx_idle);

		if (schedule)
			tasklet_schedule(&dev->smd_data.tx_tlet);
		spin_unlock(&dev->lock);
		break;
	}

}

static void rmnet_disconnect_work(struct work_struct *w)
{
	struct qmi_buf *qmi;
	struct usb_request *req;
	struct list_head *act, *tmp;
	struct rmnet_dev *dev = container_of(w, struct rmnet_dev,
					disconnect_work);

	atomic_set(&dev->notify_count, 0);

	tasklet_kill(&dev->smd_ctl.rx_tlet);
	tasklet_kill(&dev->smd_ctl.tx_tlet);
	tasklet_kill(&dev->smd_data.rx_tlet);
	tasklet_kill(&dev->smd_data.rx_tlet);

	list_for_each_safe(act, tmp, &dev->rx_queue) {
		req = list_entry(act, struct usb_request, list);
		list_del(&req->list);
		list_add_tail(&req->list, &dev->rx_idle);
	}

	list_for_each_safe(act, tmp, &dev->qmi_req_q) {
		qmi = list_entry(act, struct qmi_buf, list);
		list_del(&qmi->list);
		list_add_tail(&qmi->list, &dev->qmi_req_pool);
	}

	list_for_each_safe(act, tmp, &dev->qmi_resp_q) {
		qmi = list_entry(act, struct qmi_buf, list);
		list_del(&qmi->list);
		list_add_tail(&qmi->list, &dev->qmi_resp_pool);
	}

	smd_close(dev->smd_ctl.ch);
	dev->smd_ctl.flags = 0;

	smd_close(dev->smd_data.ch);
	dev->smd_data.flags = 0;
}

static void rmnet_connect_work(struct work_struct *w)
{
	struct rmnet_dev *dev = container_of(w, struct rmnet_dev, connect_work);
	int ret;

	/* Control channel for QMI messages */
	ret = smd_open(rmnet_ctl_ch, &dev->smd_ctl.ch,
			&dev->smd_ctl, rmnet_smd_notify);
	if (ret) {
		pr_err("%s: Unable to open control smd channel\n", __func__);
		return;
	}
	wait_event(dev->smd_ctl.wait, test_bit(CH_OPENED,
				&dev->smd_ctl.flags));

	/* Data channel for network packets */
	ret = smd_open(rmnet_data_ch, &dev->smd_data.ch,
			&dev->smd_data, rmnet_smd_notify);
	if (ret) {
		pr_err("%s: Unable to open data smd channel\n", __func__);
		smd_close(dev->smd_ctl.ch);
	}
	wait_event(dev->smd_data.wait, test_bit(CH_OPENED,
				&dev->smd_data.flags));

	if (usb_msm_get_speed() == USB_SPEED_HIGH) {
		usb_configure_endpoint(dev->epin, &rmnet_hs_in_desc);
		usb_configure_endpoint(dev->epout, &rmnet_hs_out_desc);
		usb_configure_endpoint(dev->epnotify, &rmnet_hs_notify_desc);
	} else {
		usb_configure_endpoint(dev->epin, &rmnet_fs_in_desc);
		usb_configure_endpoint(dev->epout, &rmnet_fs_out_desc);
		usb_configure_endpoint(dev->epnotify, &rmnet_fs_notify_desc);
	}

	usb_ept_enable(dev->epin,  1);
	usb_ept_enable(dev->epout, 1);
	usb_ept_enable(dev->epnotify, 1);

	atomic_set(&dev->online, 1);
	/* Queue Rx data requests */
	rmnet_start_rx(dev);
}

static void rmnet_configure(int configured, void *context)

{
	struct rmnet_dev *dev = context;

	if (configured) {
		queue_work(dev->wq, &dev->connect_work);
	} else {
		/* all pending requests will be canceled */
		if (!atomic_read(&dev->online))
			return;

		atomic_set(&dev->online, 0);

		usb_ept_fifo_flush(dev->epnotify);
		usb_ept_enable(dev->epnotify, 0);

		usb_ept_fifo_flush(dev->epout);
		usb_ept_enable(dev->epout, 0);

		usb_ept_fifo_flush(dev->epin);
		usb_ept_enable(dev->epin, 0);

		/* cleanup work */
		queue_work(dev->wq, &dev->disconnect_work);
	}

}

static void rmnet_free_buf(struct rmnet_dev *dev)
{
	struct qmi_buf *qmi;
	struct usb_request *req;
	struct list_head *act, *tmp;

	/* free all usb requests in tx pool */
	list_for_each_safe(act, tmp, &dev->tx_idle) {
		req = list_entry(act, struct usb_request, list);
		list_del(&req->list);
		rmnet_free_req(dev->epout, req);
	}

	/* free all usb requests in rx pool */
	list_for_each_safe(act, tmp, &dev->rx_idle) {
		req = list_entry(act, struct usb_request, list);
		list_del(&req->list);
		rmnet_free_req(dev->epin, req);
	}

	/* free all buffers in qmi request pool */
	list_for_each_safe(act, tmp, &dev->qmi_req_pool) {
		qmi = list_entry(act, struct qmi_buf, list);
		list_del(&qmi->list);
		rmnet_free_qmi(qmi);
	}

	/* free all buffers in qmi request pool */
	list_for_each_safe(act, tmp, &dev->qmi_resp_pool) {
		qmi = list_entry(act, struct qmi_buf, list);
		list_del(&qmi->list);
		rmnet_free_qmi(qmi);
	}

	rmnet_free_req(dev->epnotify, dev->notify_req);
}

static void rmnet_bind(void *context)
{
	struct rmnet_dev *dev = context;
	int i, ret;
	struct usb_request *req;
	struct qmi_buf *qmi;

	dev->ifc_id = usb_msm_get_next_ifc_number(&rmnet_function);
	rmnet_interface_desc.bInterfaceNumber = dev->ifc_id;

	/*Configuring IN Endpoint*/
	dev->epin = usb_alloc_endpoint(USB_DIR_IN);
	if (!dev->epin)
		return;

	rmnet_hs_in_desc.bEndpointAddress = USB_DIR_IN |
					dev->epin->num;
	rmnet_fs_in_desc.bEndpointAddress = USB_DIR_IN |
					dev->epin->num;

	/*Configuring OUT Endpoint*/
	dev->epout = usb_alloc_endpoint(USB_DIR_OUT);
	if (!dev->epout)
		goto free_epin;

	rmnet_hs_out_desc.bEndpointAddress = USB_DIR_OUT |
					dev->epout->num;
	rmnet_fs_out_desc.bEndpointAddress = USB_DIR_OUT |
					dev->epout->num;

	/*Configuring NOTIFY Endpoint*/
	dev->epnotify = usb_alloc_endpoint(USB_DIR_IN);
	if (!dev->epnotify)
		goto free_epout;

	rmnet_hs_notify_desc.bEndpointAddress = USB_DIR_IN |
				dev->epnotify->num;
	rmnet_fs_notify_desc.bEndpointAddress = USB_DIR_IN |
				dev->epnotify->num;

	dev->notify_req = usb_ept_alloc_req(dev->epnotify, 0);
	if (!dev->notify_req)
		goto free_epnotify;

	dev->notify_req->buf = kmalloc(RMNET_MAX_NOTIFY_SIZE, GFP_KERNEL);
	if (!dev->notify_req->buf)
		goto free_buf;;

	dev->notify_req->complete = rmnet_notify_complete;
	dev->notify_req->context = dev;
	dev->notify_req->length = RMNET_MAX_NOTIFY_SIZE;

	/* Allocate the qmi request and response buffers */
	for (i = 0; i < QMI_REQ_MAX; i++) {
		qmi = rmnet_alloc_qmi(QMI_REQ_SIZE, GFP_KERNEL);
		if (IS_ERR(qmi)) {
			ret = PTR_ERR(qmi);
			goto free_buf;
		}
		list_add_tail(&qmi->list, &dev->qmi_req_pool);
	}

	for (i = 0; i < QMI_RESP_MAX; i++) {
		qmi = rmnet_alloc_qmi(QMI_RESP_SIZE, GFP_KERNEL);
		if (IS_ERR(qmi)) {
			ret = PTR_ERR(qmi);
			goto free_buf;
		}
		list_add_tail(&qmi->list, &dev->qmi_resp_pool);
	}

	/* Allocate bulk in/out requests for data transfer */
	for (i = 0; i < RX_REQ_MAX; i++) {
		req = rmnet_alloc_req(dev->epout, RX_REQ_SIZE, GFP_KERNEL);
		if (IS_ERR(req)) {
			ret = PTR_ERR(req);
			goto free_buf;
		}
		req->length = TXN_MAX;
		req->context = dev;
		req->complete = rmnet_complete_epout;
		list_add_tail(&req->list, &dev->rx_idle);
	}

	for (i = 0; i < TX_REQ_MAX; i++) {
		req = rmnet_alloc_req(dev->epout, TX_REQ_SIZE, GFP_KERNEL);
		if (IS_ERR(req)) {
			ret = PTR_ERR(req);
			goto free_buf;
		}
		req->context = dev;
		req->complete = rmnet_complete_epin;
		list_add_tail(&req->list, &dev->tx_idle);
	}


	pr_info("Rmnet function bind completed\n");

	return;

free_buf:
	rmnet_free_buf(dev);
free_epnotify:
	usb_free_endpoint(dev->epnotify);
free_epout:
	usb_free_endpoint(dev->epout);
free_epin:
	usb_free_endpoint(dev->epin);

}

static void rmnet_unbind(void *context)
{
	struct rmnet_dev *dev = context;

	tasklet_kill(&dev->smd_ctl.rx_tlet);
	tasklet_kill(&dev->smd_ctl.tx_tlet);
	tasklet_kill(&dev->smd_data.rx_tlet);
	tasklet_kill(&dev->smd_data.rx_tlet);
	flush_workqueue(dev->wq);

	rmnet_free_buf(dev);
	usb_free_endpoint(dev->epin);
	usb_free_endpoint(dev->epout);
	usb_free_endpoint(dev->epnotify);

	kfree(dev);

}
static struct usb_function rmnet_function = {
	.bind = rmnet_bind,
	.configure = rmnet_configure,
	.unbind = rmnet_unbind,
	.setup  = rmnet_setup,
	.name = "rmnet",
};

struct usb_descriptor_header *rmnet_hs_descriptors[5];
struct usb_descriptor_header *rmnet_fs_descriptors[5];
static int __init rmnet_init(void)
{
	struct rmnet_dev *dev;
	int ret;

	dev = kzalloc(sizeof(*dev), GFP_KERNEL);
	if (!dev)
		return -ENOMEM;

	dev->wq = create_singlethread_workqueue("k_rmnet_work");
	if (!dev->wq) {
		ret = -ENOMEM;
		goto free_dev;
	}

	spin_lock_init(&dev->lock);
	atomic_set(&dev->notify_count, 0);
	atomic_set(&dev->online, 0);
	atomic_set(&dev->smd_ctl.rx_pkt, 0);
	atomic_set(&dev->smd_data.rx_pkt, 0);

	INIT_WORK(&dev->connect_work, rmnet_connect_work);
	INIT_WORK(&dev->disconnect_work, rmnet_disconnect_work);

	tasklet_init(&dev->smd_ctl.rx_tlet, rmnet_control_rx_tlet,
					(unsigned long) dev);
	tasklet_init(&dev->smd_ctl.tx_tlet, rmnet_control_tx_tlet,
					(unsigned long) dev);
	tasklet_init(&dev->smd_data.rx_tlet, rmnet_data_rx_tlet,
					(unsigned long) dev);
	tasklet_init(&dev->smd_data.tx_tlet, rmnet_data_tx_tlet,
					(unsigned long) dev);

	init_waitqueue_head(&dev->smd_ctl.wait);
	init_waitqueue_head(&dev->smd_data.wait);

	INIT_LIST_HEAD(&dev->qmi_req_pool);
	INIT_LIST_HEAD(&dev->qmi_req_q);
	INIT_LIST_HEAD(&dev->qmi_resp_pool);
	INIT_LIST_HEAD(&dev->qmi_resp_q);
	INIT_LIST_HEAD(&dev->rx_idle);
	INIT_LIST_HEAD(&dev->rx_queue);
	INIT_LIST_HEAD(&dev->tx_idle);

	rmnet_hs_descriptors[0] =
		(struct usb_descriptor_header *)&rmnet_interface_desc;
	rmnet_hs_descriptors[1] =
		(struct usb_descriptor_header *)&rmnet_hs_in_desc;
	rmnet_hs_descriptors[2] =
		(struct usb_descriptor_header *)&rmnet_hs_out_desc;
	rmnet_hs_descriptors[3] =
		(struct usb_descriptor_header *)&rmnet_hs_notify_desc;
	rmnet_hs_descriptors[4] = NULL;

	rmnet_fs_descriptors[0] =
		(struct usb_descriptor_header *)&rmnet_interface_desc;
	rmnet_fs_descriptors[1] =
		(struct usb_descriptor_header *)&rmnet_fs_in_desc;
	rmnet_fs_descriptors[2] =
		(struct usb_descriptor_header *)&rmnet_fs_out_desc;
	rmnet_fs_descriptors[3] =
		(struct usb_descriptor_header *)&rmnet_fs_notify_desc;
	rmnet_fs_descriptors[4] = NULL;

	rmnet_function.hs_descriptors = rmnet_hs_descriptors;
	rmnet_function.fs_descriptors = rmnet_fs_descriptors;
	rmnet_function.context = dev;

	ret = usb_function_register(&rmnet_function);
	if (ret)
		goto free_wq;

	return 0;

free_wq:
	destroy_workqueue(dev->wq);
free_dev:
	kfree(dev);

	return ret;
}

static void __exit rmnet_exit(void)
{
	usb_function_unregister(&rmnet_function);
}

module_init(rmnet_init);
module_exit(rmnet_exit);
MODULE_DESCRIPTION("RmNet usb function driver");
MODULE_VERSION("1.0");
MODULE_LICENSE("GPL v2");
