/*
 * f_rmnet.c -- RmNet function driver
 *
 * Copyright (C) 2003-2005,2008 David Brownell
 * Copyright (C) 2003-2004 Robert Schwebel, Benedikt Spranger
 * Copyright (C) 2003 Al Borchers (alborchers@steinerpoint.com)
 * Copyright (C) 2008 Nokia Corporation
 * Copyright (c) 2010-2012, The Linux Foundation. All rights reserved.
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
#include <linux/list.h>
#include <linux/device.h>
#include <linux/wait.h>
#include <linux/workqueue.h>
#include <linux/bitops.h>
#include <linux/termios.h>
#include <linux/debugfs.h>

#include <mach/msm_smd.h>
#include <linux/usb/cdc.h>
#include <linux/usb/composite.h>
#include <linux/usb/ch9.h>

#include "gadget_chips.h"

#ifndef CONFIG_MSM_SMD
#define CONFIG_RMNET_SMD_CTL_CHANNEL	""
#define CONFIG_RMNET_SMD_DATA_CHANNEL	""
#endif

static char *rmnet_ctl_ch = CONFIG_RMNET_SMD_CTL_CHANNEL;
module_param(rmnet_ctl_ch, charp, S_IRUGO);
MODULE_PARM_DESC(rmnet_ctl_ch, "RmNet control SMD channel");

static char *rmnet_data_ch = CONFIG_RMNET_SMD_DATA_CHANNEL;
module_param(rmnet_data_ch, charp, S_IRUGO);
MODULE_PARM_DESC(rmnet_data_ch, "RmNet data SMD channel");

#define RMNET_SMD_ACM_CTRL_DTR	(1 << 0)

#define RMNET_SMD_NOTIFY_INTERVAL	5
#define RMNET_SMD_MAX_NOTIFY_SIZE	sizeof(struct usb_cdc_notification)

#define QMI_REQ_MAX			4
#define QMI_REQ_SIZE			2048
#define QMI_RESP_MAX			8
#define QMI_RESP_SIZE			2048

#define RMNET_RX_REQ_MAX		8
#define RMNET_RX_REQ_SIZE		2048
#define RMNET_TX_REQ_MAX		8
#define RMNET_TX_REQ_SIZE		2048

#define RMNET_TXN_MAX	 		2048

/* QMI requests & responses buffer*/
struct qmi_buf {
	void *buf;
	int len;
	struct list_head list;
};

/* Control & data SMD channel private data */
struct rmnet_smd_ch_info {
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

struct rmnet_smd_dev {
	struct usb_function function;
	struct usb_composite_dev *cdev;

	struct usb_ep		*epout;
	struct usb_ep		*epin;
	struct usb_ep		*epnotify;
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

	struct platform_driver		pdrv;
	u8				is_pdrv_used;
	struct rmnet_smd_ch_info	smd_ctl;
	struct rmnet_smd_ch_info	smd_data;

	struct workqueue_struct *wq;
	struct work_struct connect_work;
	struct work_struct disconnect_work;

	unsigned long	dpkts_to_host;
	unsigned long	dpkts_from_modem;
	unsigned long	dpkts_from_host;
	unsigned long	dpkts_to_modem;

	unsigned long	cpkts_to_host;
	unsigned long	cpkts_from_modem;
	unsigned long	cpkts_from_host;
	unsigned long	cpkts_to_modem;
};

static struct rmnet_smd_dev *rmnet_smd;

static struct usb_interface_descriptor rmnet_smd_interface_desc = {
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
static struct usb_endpoint_descriptor rmnet_smd_fs_notify_desc = {
	.bLength =		USB_DT_ENDPOINT_SIZE,
	.bDescriptorType =	USB_DT_ENDPOINT,
	.bEndpointAddress =	USB_DIR_IN,
	.bmAttributes =		USB_ENDPOINT_XFER_INT,
	.wMaxPacketSize =	__constant_cpu_to_le16(
						RMNET_SMD_MAX_NOTIFY_SIZE),
	.bInterval =		1 << RMNET_SMD_NOTIFY_INTERVAL,
};

static struct usb_endpoint_descriptor rmnet_smd_fs_in_desc  = {
	.bLength =		USB_DT_ENDPOINT_SIZE,
	.bDescriptorType =	USB_DT_ENDPOINT,
	.bEndpointAddress =	USB_DIR_IN,
	.bmAttributes =		USB_ENDPOINT_XFER_BULK,
	.wMaxPacketSize   = __constant_cpu_to_le16(64),
};

static struct usb_endpoint_descriptor rmnet_smd_fs_out_desc = {
	.bLength =		USB_DT_ENDPOINT_SIZE,
	.bDescriptorType =	USB_DT_ENDPOINT,
	.bEndpointAddress =	USB_DIR_OUT,
	.bmAttributes =		USB_ENDPOINT_XFER_BULK,
	.wMaxPacketSize   = __constant_cpu_to_le16(64),
};

static struct usb_descriptor_header *rmnet_smd_fs_function[] = {
	(struct usb_descriptor_header *) &rmnet_smd_interface_desc,
	(struct usb_descriptor_header *) &rmnet_smd_fs_notify_desc,
	(struct usb_descriptor_header *) &rmnet_smd_fs_in_desc,
	(struct usb_descriptor_header *) &rmnet_smd_fs_out_desc,
	NULL,
};

/* High speed support */
static struct usb_endpoint_descriptor rmnet_smd_hs_notify_desc  = {
	.bLength =		USB_DT_ENDPOINT_SIZE,
	.bDescriptorType =	USB_DT_ENDPOINT,
	.bEndpointAddress =	USB_DIR_IN,
	.bmAttributes =		USB_ENDPOINT_XFER_INT,
	.wMaxPacketSize =	__constant_cpu_to_le16(
						RMNET_SMD_MAX_NOTIFY_SIZE),
	.bInterval =		RMNET_SMD_NOTIFY_INTERVAL + 4,
};

static struct usb_endpoint_descriptor rmnet_smd_hs_in_desc = {
	.bLength =		USB_DT_ENDPOINT_SIZE,
	.bDescriptorType =	USB_DT_ENDPOINT,
	.bEndpointAddress =	USB_DIR_IN,
	.bmAttributes =		USB_ENDPOINT_XFER_BULK,
	.wMaxPacketSize =	__constant_cpu_to_le16(512),
};

static struct usb_endpoint_descriptor rmnet_smd_hs_out_desc = {
	.bLength =		USB_DT_ENDPOINT_SIZE,
	.bDescriptorType =	USB_DT_ENDPOINT,
	.bEndpointAddress =	USB_DIR_OUT,
	.bmAttributes =		USB_ENDPOINT_XFER_BULK,
	.wMaxPacketSize =	__constant_cpu_to_le16(512),
};

static struct usb_descriptor_header *rmnet_smd_hs_function[] = {
	(struct usb_descriptor_header *) &rmnet_smd_interface_desc,
	(struct usb_descriptor_header *) &rmnet_smd_hs_notify_desc,
	(struct usb_descriptor_header *) &rmnet_smd_hs_in_desc,
	(struct usb_descriptor_header *) &rmnet_smd_hs_out_desc,
	NULL,
};

/* String descriptors */

static struct usb_string rmnet_smd_string_defs[] = {
	[0].s = "QMI RmNet",
	{  } /* end of list */
};

static struct usb_gadget_strings rmnet_smd_string_table = {
	.language =		0x0409,	/* en-us */
	.strings =		rmnet_smd_string_defs,
};

static struct usb_gadget_strings *rmnet_smd_strings[] = {
	&rmnet_smd_string_table,
	NULL,
};

static struct qmi_buf *
rmnet_smd_alloc_qmi(unsigned len, gfp_t kmalloc_flags)
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

static void rmnet_smd_free_qmi(struct qmi_buf *qmi)
{
	kfree(qmi->buf);
	kfree(qmi);
}
/*
 * Allocate a usb_request and its buffer.  Returns a pointer to the
 * usb_request or a error code if there is an error.
 */
static struct usb_request *
rmnet_smd_alloc_req(struct usb_ep *ep, unsigned len, gfp_t kmalloc_flags)
{
	struct usb_request *req;

	req = usb_ep_alloc_request(ep, kmalloc_flags);

	if (req != NULL) {
		req->length = len;
		req->buf = kmalloc(len, kmalloc_flags);
		if (req->buf == NULL) {
			usb_ep_free_request(ep, req);
			req = NULL;
		}
	}

	return req ? req : ERR_PTR(-ENOMEM);
}

/*
 * Free a usb_request and its buffer.
 */
static void rmnet_smd_free_req(struct usb_ep *ep, struct usb_request *req)
{
	kfree(req->buf);
	usb_ep_free_request(ep, req);
}

static void rmnet_smd_notify_complete(struct usb_ep *ep,
					 struct usb_request *req)
{
	struct rmnet_smd_dev *dev = req->context;
	struct usb_composite_dev *cdev = dev->cdev;
	int status = req->status;

	switch (status) {
	case -ECONNRESET:
	case -ESHUTDOWN:
		/* connection gone */
		atomic_set(&dev->notify_count, 0);
		break;
	default:
		ERROR(cdev, "rmnet notify ep error %d\n", status);
		/* FALLTHROUGH */
	case 0:
		if (ep != dev->epnotify)
			break;

		/* handle multiple pending QMI_RESPONSE_AVAILABLE
		 * notifications by resending until we're done
		 */
		if (atomic_dec_and_test(&dev->notify_count))
			break;

		status = usb_ep_queue(dev->epnotify, req, GFP_ATOMIC);
		if (status) {
			atomic_dec(&dev->notify_count);
			ERROR(cdev, "rmnet notify ep enqueue error %d\n",
					status);
		}
		break;
	}
}

static void qmi_smd_response_available(struct rmnet_smd_dev *dev)
{
	struct usb_composite_dev *cdev = dev->cdev;
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

	status = usb_ep_queue(dev->epnotify, dev->notify_req, GFP_ATOMIC);
	if (status < 0) {
		atomic_dec(&dev->notify_count);
		ERROR(cdev, "rmnet notify ep enqueue error %d\n", status);
	}
}

/* TODO
 * handle modem restart events
 */
static void rmnet_smd_event_notify(void *priv, unsigned event)
{
	struct rmnet_smd_ch_info *smd_info = priv;
	int len = atomic_read(&smd_info->rx_pkt);
	struct rmnet_smd_dev *dev =
				(struct rmnet_smd_dev *) smd_info->tx_tlet.data;

	switch (event) {
	case SMD_EVENT_DATA: {
		if (!atomic_read(&dev->online))
			break;
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
	struct rmnet_smd_dev *dev = (struct rmnet_smd_dev *) arg;
	struct usb_composite_dev *cdev = dev->cdev;
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
			ERROR(cdev, "rmnet QMI Tx buffers full\n");
			spin_unlock_irqrestore(&dev->lock, flags);
			break;
		}
		qmi_resp = list_first_entry(&dev->qmi_resp_pool,
				struct qmi_buf, list);
		list_del(&qmi_resp->list);
		spin_unlock_irqrestore(&dev->lock, flags);

		qmi_resp->len = smd_read(dev->smd_ctl.ch, qmi_resp->buf, sz);

		spin_lock_irqsave(&dev->lock, flags);
		dev->cpkts_from_modem++;
		list_add_tail(&qmi_resp->list, &dev->qmi_resp_q);
		spin_unlock_irqrestore(&dev->lock, flags);

		qmi_smd_response_available(dev);
	}

}

static void rmnet_control_rx_tlet(unsigned long arg)
{
	struct rmnet_smd_dev *dev = (struct rmnet_smd_dev *) arg;
	struct usb_composite_dev *cdev = dev->cdev;
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
			DBG(cdev, "rmnet control smd channel full\n");
			break;
		}

		list_del(&qmi_req->list);
		dev->cpkts_from_host++;
		spin_unlock_irqrestore(&dev->lock, flags);
		ret = smd_write(dev->smd_ctl.ch, qmi_req->buf, qmi_req->len);
		spin_lock_irqsave(&dev->lock, flags);
		if (ret != qmi_req->len) {
			ERROR(cdev, "rmnet control smd write failed\n");
			break;
		}
		dev->cpkts_to_modem++;
		list_add_tail(&qmi_req->list, &dev->qmi_req_pool);
	}
	spin_unlock_irqrestore(&dev->lock, flags);
}

static void rmnet_smd_command_complete(struct usb_ep *ep,
					struct usb_request *req)
{
	struct rmnet_smd_dev *dev = req->context;
	struct usb_composite_dev *cdev = dev->cdev;
	struct qmi_buf *qmi_req;
	int ret;

	if (req->status < 0) {
		ERROR(cdev, "rmnet command error %d\n", req->status);
		return;
	}

	spin_lock(&dev->lock);
	dev->cpkts_from_host++;
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
			ERROR(cdev, "rmnet control smd write failed\n");
		spin_lock(&dev->lock);
		dev->cpkts_to_modem++;
		spin_unlock(&dev->lock);
		return;
	}
queue_req:
	if (list_empty(&dev->qmi_req_pool)) {
		spin_unlock(&dev->lock);
		ERROR(cdev, "rmnet QMI pool is empty\n");
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
}
static void rmnet_txcommand_complete(struct usb_ep *ep, struct usb_request *req)
{
	struct rmnet_smd_dev *dev = req->context;

	spin_lock(&dev->lock);
	dev->cpkts_to_host++;
	spin_unlock(&dev->lock);
}

static int
rmnet_smd_setup(struct usb_function *f, const struct usb_ctrlrequest *ctrl)
{
	struct rmnet_smd_dev *dev = container_of(f, struct rmnet_smd_dev,
								function);
	struct usb_composite_dev *cdev = f->config->cdev;
	struct usb_request	*req = cdev->req;
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
		ret = w_length;
		req->complete = rmnet_smd_command_complete;
		req->context = dev;
		break;


	case ((USB_DIR_IN | USB_TYPE_CLASS | USB_RECIP_INTERFACE) << 8)
			| USB_CDC_GET_ENCAPSULATED_RESPONSE:
		if (w_value)
			goto invalid;
		else {
			spin_lock(&dev->lock);
			if (list_empty(&dev->qmi_resp_q)) {
				INFO(cdev, "qmi resp empty "
					" req%02x.%02x v%04x i%04x l%d\n",
					ctrl->bRequestType, ctrl->bRequest,
					w_value, w_index, w_length);
				spin_unlock(&dev->lock);
				goto invalid;
			}
			resp = list_first_entry(&dev->qmi_resp_q,
					struct qmi_buf, list);
			list_del(&resp->list);
			spin_unlock(&dev->lock);
			memcpy(req->buf, resp->buf, resp->len);
			ret = resp->len;
			spin_lock(&dev->lock);

			if (list_empty(&dev->qmi_resp_pool))
				schedule = 1;
			list_add_tail(&resp->list, &dev->qmi_resp_pool);

			if (schedule)
				tasklet_schedule(&dev->smd_ctl.tx_tlet);
			spin_unlock(&dev->lock);
			req->complete = rmnet_txcommand_complete;
			req->context = dev;
		}
		break;
	case ((USB_DIR_OUT | USB_TYPE_CLASS | USB_RECIP_INTERFACE) << 8)
			| USB_CDC_REQ_SET_CONTROL_LINE_STATE:
		/* This is a workaround for RmNet and is borrowed from the
		 * CDC/ACM standard. The host driver will issue the above ACM
		 * standard request to the RmNet interface in the following
		 * scenario: Once the network adapter is disabled from device
		 * manager, the above request will be sent from the qcusbnet
		 * host driver, with DTR being '0'. Once network adapter is
		 * enabled from device manager (or during enumeration), the
		 * request will be sent with DTR being '1'.
		 */
		if (w_value & RMNET_SMD_ACM_CTRL_DTR)
			ret = smd_tiocmset(dev->smd_ctl.ch, TIOCM_DTR, 0);
		else
			ret = smd_tiocmset(dev->smd_ctl.ch, 0, TIOCM_DTR);

		break;
	default:

invalid:
		DBG(cdev, "invalid control req%02x.%02x v%04x i%04x l%d\n",
			ctrl->bRequestType, ctrl->bRequest,
			w_value, w_index, w_length);
	}

	/* respond with data transfer or status phase? */
	if (ret >= 0) {
		VDBG(cdev, "rmnet req%02x.%02x v%04x i%04x l%d\n",
			ctrl->bRequestType, ctrl->bRequest,
			w_value, w_index, w_length);
		req->zero = 0;
		req->length = ret;
		ret = usb_ep_queue(cdev->gadget->ep0, req, GFP_ATOMIC);
		if (ret < 0)
			ERROR(cdev, "rmnet ep0 enqueue err %d\n", ret);
	}

	return ret;
}

static void rmnet_smd_start_rx(struct rmnet_smd_dev *dev)
{
	struct usb_composite_dev *cdev = dev->cdev;
	int status;
	struct usb_request *req;
	struct list_head *pool = &dev->rx_idle;
	unsigned long flags;

	spin_lock_irqsave(&dev->lock, flags);
	while (!list_empty(pool)) {
		req = list_entry(pool->next, struct usb_request, list);
		list_del(&req->list);

		spin_unlock_irqrestore(&dev->lock, flags);
		status = usb_ep_queue(dev->epout, req, GFP_ATOMIC);
		spin_lock_irqsave(&dev->lock, flags);

		if (status) {
			ERROR(cdev, "rmnet data rx enqueue err %d\n", status);
			list_add_tail(&req->list, pool);
			break;
		}
	}
	spin_unlock_irqrestore(&dev->lock, flags);
}

static void rmnet_data_tx_tlet(unsigned long arg)
{
	struct rmnet_smd_dev *dev = (struct rmnet_smd_dev *) arg;
	struct usb_composite_dev *cdev = dev->cdev;
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
			DBG(cdev, "rmnet data Tx buffers full\n");
			break;
		}
		req = list_first_entry(&dev->tx_idle, struct usb_request, list);
		list_del(&req->list);
		spin_unlock_irqrestore(&dev->lock, flags);

		req->length = smd_read(dev->smd_data.ch, req->buf, sz);
		status = usb_ep_queue(dev->epin, req, GFP_ATOMIC);
		if (status) {
			ERROR(cdev, "rmnet tx data enqueue err %d\n", status);
			spin_lock_irqsave(&dev->lock, flags);
			list_add_tail(&req->list, &dev->tx_idle);
			spin_unlock_irqrestore(&dev->lock, flags);
			break;
		}
		spin_lock_irqsave(&dev->lock, flags);
		dev->dpkts_from_modem++;
		spin_unlock_irqrestore(&dev->lock, flags);
	}

}

static void rmnet_data_rx_tlet(unsigned long arg)
{
	struct rmnet_smd_dev *dev = (struct rmnet_smd_dev *) arg;
	struct usb_composite_dev *cdev = dev->cdev;
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
			DBG(cdev, "rmnet SMD data channel full\n");
			break;
		}

		list_del(&req->list);
		spin_unlock_irqrestore(&dev->lock, flags);
		ret = smd_write(dev->smd_data.ch, req->buf, req->actual);
		spin_lock_irqsave(&dev->lock, flags);
		if (ret != req->actual) {
			ERROR(cdev, "rmnet SMD data write failed\n");
			break;
		}
		dev->dpkts_to_modem++;
		list_add_tail(&req->list, &dev->rx_idle);
	}
	spin_unlock_irqrestore(&dev->lock, flags);

	/* We have free rx data requests. */
	rmnet_smd_start_rx(dev);
}

/* If SMD has enough room to accommodate a data rx packet,
 * write into SMD directly. Otherwise enqueue to rx_queue.
 * We will not write into SMD directly untill rx_queue is
 * empty to strictly follow the ordering requests.
 */
static void rmnet_smd_complete_epout(struct usb_ep *ep, struct usb_request *req)
{
	struct rmnet_smd_dev *dev = req->context;
	struct usb_composite_dev *cdev = dev->cdev;
	int status = req->status;
	int ret;

	switch (status) {
	case 0:
		/* normal completion */
		break;
	case -ECONNRESET:
	case -ESHUTDOWN:
		/* connection gone */
		spin_lock(&dev->lock);
		list_add_tail(&req->list, &dev->rx_idle);
		spin_unlock(&dev->lock);
		return;
	default:
		/* unexpected failure */
		ERROR(cdev, "RMNET %s response error %d, %d/%d\n",
			ep->name, status,
			req->actual, req->length);
		spin_lock(&dev->lock);
		list_add_tail(&req->list, &dev->rx_idle);
		spin_unlock(&dev->lock);
		return;
	}

	spin_lock(&dev->lock);
	dev->dpkts_from_host++;
	if (!atomic_read(&dev->smd_data.rx_pkt)) {
		if (smd_write_avail(dev->smd_data.ch) < req->actual) {
			atomic_set(&dev->smd_data.rx_pkt, req->actual);
			goto queue_req;
		}
		spin_unlock(&dev->lock);
		ret = smd_write(dev->smd_data.ch, req->buf, req->actual);
		/* This should never happen */
		if (ret != req->actual)
			ERROR(cdev, "rmnet data smd write failed\n");
		/* Restart Rx */
		spin_lock(&dev->lock);
		dev->dpkts_to_modem++;
		list_add_tail(&req->list, &dev->rx_idle);
		spin_unlock(&dev->lock);
		rmnet_smd_start_rx(dev);
		return;
	}
queue_req:
	list_add_tail(&req->list, &dev->rx_queue);
	spin_unlock(&dev->lock);
}

static void rmnet_smd_complete_epin(struct usb_ep *ep, struct usb_request *req)
{
	struct rmnet_smd_dev *dev = req->context;
	struct usb_composite_dev *cdev = dev->cdev;
	int status = req->status;
	int schedule = 0;

	switch (status) {
	case -ECONNRESET:
	case -ESHUTDOWN:
		/* connection gone */
		spin_lock(&dev->lock);
		list_add_tail(&req->list, &dev->tx_idle);
		spin_unlock(&dev->lock);
		break;
	default:
		ERROR(cdev, "rmnet data tx ep error %d\n", status);
		/* FALLTHROUGH */
	case 0:
		spin_lock(&dev->lock);
		if (list_empty(&dev->tx_idle))
			schedule = 1;
		list_add_tail(&req->list, &dev->tx_idle);
		dev->dpkts_to_host++;
		if (schedule)
			tasklet_schedule(&dev->smd_data.tx_tlet);
		spin_unlock(&dev->lock);
		break;
	}

}

static void rmnet_smd_disconnect_work(struct work_struct *w)
{
	struct qmi_buf *qmi;
	struct usb_request *req;
	struct list_head *act, *tmp;
	struct rmnet_smd_dev *dev = container_of(w, struct rmnet_smd_dev,
					disconnect_work);

	tasklet_kill(&dev->smd_ctl.rx_tlet);
	tasklet_kill(&dev->smd_ctl.tx_tlet);
	tasklet_kill(&dev->smd_data.rx_tlet);
	tasklet_kill(&dev->smd_data.tx_tlet);

	smd_close(dev->smd_ctl.ch);
	dev->smd_ctl.flags = 0;

	smd_close(dev->smd_data.ch);
	dev->smd_data.flags = 0;

	atomic_set(&dev->notify_count, 0);

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

	if (dev->is_pdrv_used) {
		platform_driver_unregister(&dev->pdrv);
		dev->is_pdrv_used = 0;
	}
}

/* SMD close may sleep
 * schedule a work to close smd channels
 */
static void rmnet_smd_disable(struct usb_function *f)
{
	struct rmnet_smd_dev *dev = container_of(f, struct rmnet_smd_dev,
								function);

	atomic_set(&dev->online, 0);

	usb_ep_fifo_flush(dev->epnotify);
	usb_ep_disable(dev->epnotify);
	usb_ep_fifo_flush(dev->epout);
	usb_ep_disable(dev->epout);

	usb_ep_fifo_flush(dev->epin);
	usb_ep_disable(dev->epin);

	/* cleanup work */
	queue_work(dev->wq, &dev->disconnect_work);
}

static void rmnet_smd_connect_work(struct work_struct *w)
{
	struct rmnet_smd_dev *dev = container_of(w, struct rmnet_smd_dev,
								connect_work);
	struct usb_composite_dev *cdev = dev->cdev;
	int ret = 0;

	/* Control channel for QMI messages */
	ret = smd_open(rmnet_ctl_ch, &dev->smd_ctl.ch,
			&dev->smd_ctl, rmnet_smd_event_notify);
	if (ret) {
		ERROR(cdev, "Unable to open control smd channel: %d\n", ret);
		/*
		 * Register platform driver to be notified in case SMD channels
		 * later becomes ready to be opened.
		 */
		if (!dev->is_pdrv_used) {
			ret = platform_driver_register(&dev->pdrv);
			if (ret)
				ERROR(cdev, "pdrv %s register failed %d\n",
						dev->pdrv.driver.name, ret);
			else
				dev->is_pdrv_used = 1;
		}
		return;
	}
	wait_event(dev->smd_ctl.wait, test_bit(CH_OPENED,
				&dev->smd_ctl.flags));

	/* Data channel for network packets */
	ret = smd_open(rmnet_data_ch, &dev->smd_data.ch,
			&dev->smd_data, rmnet_smd_event_notify);
	if (ret) {
		ERROR(cdev, "Unable to open data smd channel\n");
		smd_close(dev->smd_ctl.ch);
		return;
	}
	wait_event(dev->smd_data.wait, test_bit(CH_OPENED,
				&dev->smd_data.flags));

	atomic_set(&dev->online, 1);
	/* Queue Rx data requests */
	rmnet_smd_start_rx(dev);
}

static int rmnet_smd_ch_probe(struct platform_device *pdev)
{
	DBG(rmnet_smd->cdev, "Probe called for device: %s\n", pdev->name);

	queue_work(rmnet_smd->wq, &rmnet_smd->connect_work);

	return 0;
}

/* SMD open may sleep.
 * Schedule a work to open smd channels and enable
 * endpoints if smd channels are opened successfully.
 */
static int rmnet_smd_set_alt(struct usb_function *f,
		unsigned intf, unsigned alt)
{
	struct rmnet_smd_dev *dev = container_of(f, struct rmnet_smd_dev,
								function);
	struct usb_composite_dev *cdev = dev->cdev;
	int ret = 0;

	/* Enable epin endpoint */
	ret = config_ep_by_speed(cdev->gadget, f, dev->epin);
	if (ret) {
		dev->epin->desc = NULL;
		ERROR(cdev, "config_ep_by_speed failed for ep %s, result %d\n",
			dev->epin->name, ret);
		return ret;
	}
	ret = usb_ep_enable(dev->epin);
	if (ret) {
		ERROR(cdev, "can't enable %s, result %d\n",
					dev->epin->name, ret);
		return ret;
	}

	/* Enable epout endpoint */
	ret = config_ep_by_speed(cdev->gadget, f, dev->epout);
	if (ret) {
		dev->epout->desc = NULL;
		ERROR(cdev, "config_ep_by_speed failed for ep %s, result %d\n",
					dev->epout->name, ret);
		usb_ep_disable(dev->epin);
		return ret;
	}
	ret = usb_ep_enable(dev->epout);

	if (ret) {
		ERROR(cdev, "can't enable %s, result %d\n",
					dev->epout->name, ret);
		usb_ep_disable(dev->epin);
		return ret;
	}

	/* Enable epnotify endpoint */
	ret = config_ep_by_speed(cdev->gadget, f, dev->epnotify);
	if (ret) {
		dev->epnotify->desc = NULL;
		ERROR(cdev, "config_ep_by_speed failed for ep %s, result %d\n",
			dev->epnotify->name, ret);
		usb_ep_disable(dev->epin);
		usb_ep_disable(dev->epout);
		return ret;
	}
	ret = usb_ep_enable(dev->epnotify);
	if (ret) {
		ERROR(cdev, "can't enable %s, result %d\n",
					dev->epnotify->name, ret);
		usb_ep_disable(dev->epin);
		usb_ep_disable(dev->epout);
		return ret;
	}

	queue_work(dev->wq, &dev->connect_work);
	return 0;
}

static void rmnet_smd_free_buf(struct rmnet_smd_dev *dev)
{
	struct qmi_buf *qmi;
	struct usb_request *req;
	struct list_head *act, *tmp;

	dev->dpkts_to_host = 0;
	dev->dpkts_from_modem = 0;
	dev->dpkts_from_host = 0;
	dev->dpkts_to_modem = 0;

	dev->cpkts_to_host = 0;
	dev->cpkts_from_modem = 0;
	dev->cpkts_from_host = 0;
	dev->cpkts_to_modem = 0;
	/* free all usb requests in tx pool */
	list_for_each_safe(act, tmp, &dev->tx_idle) {
		req = list_entry(act, struct usb_request, list);
		list_del(&req->list);
		rmnet_smd_free_req(dev->epout, req);
	}

	/* free all usb requests in rx pool */
	list_for_each_safe(act, tmp, &dev->rx_idle) {
		req = list_entry(act, struct usb_request, list);
		list_del(&req->list);
		rmnet_smd_free_req(dev->epin, req);
	}

	/* free all buffers in qmi request pool */
	list_for_each_safe(act, tmp, &dev->qmi_req_pool) {
		qmi = list_entry(act, struct qmi_buf, list);
		list_del(&qmi->list);
		rmnet_smd_free_qmi(qmi);
	}

	/* free all buffers in qmi request pool */
	list_for_each_safe(act, tmp, &dev->qmi_resp_pool) {
		qmi = list_entry(act, struct qmi_buf, list);
		list_del(&qmi->list);
		rmnet_smd_free_qmi(qmi);
	}

	rmnet_smd_free_req(dev->epnotify, dev->notify_req);
}
static int rmnet_smd_bind(struct usb_configuration *c, struct usb_function *f)
{
	struct usb_composite_dev *cdev = c->cdev;
	struct rmnet_smd_dev *dev = container_of(f, struct rmnet_smd_dev,
								function);
	int i, id, ret;
	struct qmi_buf *qmi;
	struct usb_request *req;
	struct usb_ep *ep;

	dev->cdev = cdev;

	/* allocate interface ID */
	id = usb_interface_id(c, f);
	if (id < 0)
		return id;
	dev->ifc_id = id;
	rmnet_smd_interface_desc.bInterfaceNumber = id;

	ep = usb_ep_autoconfig(cdev->gadget, &rmnet_smd_fs_in_desc);
	if (!ep)
		return -ENODEV;
	ep->driver_data = cdev; /* claim endpoint */
	dev->epin = ep;

	ep = usb_ep_autoconfig(cdev->gadget, &rmnet_smd_fs_out_desc);
	if (!ep)
		return -ENODEV;
	ep->driver_data = cdev; /* claim endpoint */
	dev->epout = ep;

	ep = usb_ep_autoconfig(cdev->gadget, &rmnet_smd_fs_notify_desc);
	if (!ep)
		return -ENODEV;
	ep->driver_data = cdev; /* clain endpoint */
	dev->epnotify = ep;

	/* support all relevant hardware speeds... we expect that when
	 * hardware is dual speed, all bulk-capable endpoints work at
	 * both speeds
	 */
	if (gadget_is_dualspeed(c->cdev->gadget)) {
		rmnet_smd_hs_in_desc.bEndpointAddress =
				rmnet_smd_fs_in_desc.bEndpointAddress;
		rmnet_smd_hs_out_desc.bEndpointAddress =
				rmnet_smd_fs_out_desc.bEndpointAddress;
		rmnet_smd_hs_notify_desc.bEndpointAddress =
				rmnet_smd_fs_notify_desc.bEndpointAddress;

	}

	/* allocate notification */
	dev->notify_req = rmnet_smd_alloc_req(dev->epnotify,
					RMNET_SMD_MAX_NOTIFY_SIZE, GFP_KERNEL);
	if (IS_ERR(dev->notify_req))
		return PTR_ERR(dev->notify_req);

	dev->notify_req->complete = rmnet_smd_notify_complete;
	dev->notify_req->context = dev;
	dev->notify_req->length = RMNET_SMD_MAX_NOTIFY_SIZE;

	/* Allocate the qmi request and response buffers */
	for (i = 0; i < QMI_REQ_MAX; i++) {
		qmi = rmnet_smd_alloc_qmi(QMI_REQ_SIZE, GFP_KERNEL);
		if (IS_ERR(qmi)) {
			ret = PTR_ERR(qmi);
			goto free_buf;
		}
		list_add_tail(&qmi->list, &dev->qmi_req_pool);
	}

	for (i = 0; i < QMI_RESP_MAX; i++) {
		qmi = rmnet_smd_alloc_qmi(QMI_RESP_SIZE, GFP_KERNEL);
		if (IS_ERR(qmi)) {
			ret = PTR_ERR(qmi);
			goto free_buf;
		}
		list_add_tail(&qmi->list, &dev->qmi_resp_pool);
	}

	/* Allocate bulk in/out requests for data transfer */
	for (i = 0; i < RMNET_RX_REQ_MAX; i++) {
		req = rmnet_smd_alloc_req(dev->epout, RMNET_RX_REQ_SIZE,
								 GFP_KERNEL);
		if (IS_ERR(req)) {
			ret = PTR_ERR(req);
			goto free_buf;
		}
		req->length = RMNET_TXN_MAX;
		req->context = dev;
		req->complete = rmnet_smd_complete_epout;
		list_add_tail(&req->list, &dev->rx_idle);
	}

	for (i = 0; i < RMNET_TX_REQ_MAX; i++) {
		req = rmnet_smd_alloc_req(dev->epin, RMNET_TX_REQ_SIZE,
							GFP_KERNEL);
		if (IS_ERR(req)) {
			ret = PTR_ERR(req);
			goto free_buf;
		}
		req->context = dev;
		req->complete = rmnet_smd_complete_epin;
		list_add_tail(&req->list, &dev->tx_idle);
	}

	return 0;

free_buf:
	rmnet_smd_free_buf(dev);
	dev->epout = dev->epin = dev->epnotify = NULL; /* release endpoints */
	return ret;
}

#if defined(CONFIG_DEBUG_FS)
static ssize_t rmnet_smd_debug_read_stats(struct file *file, char __user *ubuf,
		size_t count, loff_t *ppos)
{
	struct rmnet_smd_dev *dev = file->private_data;
	struct rmnet_smd_ch_info smd_ctl_info = dev->smd_ctl;
	struct rmnet_smd_ch_info smd_data_info = dev->smd_data;
	char *buf;
	unsigned long flags;
	int ret;

	buf = kzalloc(sizeof(char) * 512, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	spin_lock_irqsave(&dev->lock, flags);
	ret = scnprintf(buf, 512,
			"smd_control_ch_opened: %lu\n"
			"smd_data_ch_opened: %lu\n"
			"usb online : %d\n"
			"dpkts_from_modem: %lu\n"
			"dpkts_to_host: %lu\n"
			"pending_dpkts_to_host: %lu\n"
			"dpkts_from_host: %lu\n"
			"dpkts_to_modem: %lu\n"
			"pending_dpkts_to_modem: %lu\n"
			"cpkts_from_modem: %lu\n"
			"cpkts_to_host: %lu\n"
			"pending_cpkts_to_host: %lu\n"
			"cpkts_from_host: %lu\n"
			"cpkts_to_modem: %lu\n"
			"pending_cpkts_to_modem: %lu\n"
			"smd_read_avail_ctrl: %d\n"
			"smd_write_avail_ctrl: %d\n"
			"smd_read_avail_data: %d\n"
			"smd_write_avail_data: %d\n",
			smd_ctl_info.flags, smd_data_info.flags,
			atomic_read(&dev->online),
			dev->dpkts_from_modem, dev->dpkts_to_host,
			(dev->dpkts_from_modem - dev->dpkts_to_host),
			dev->dpkts_from_host, dev->dpkts_to_modem,
			(dev->dpkts_from_host - dev->dpkts_to_modem),
			dev->cpkts_from_modem, dev->cpkts_to_host,
			(dev->cpkts_from_modem - dev->cpkts_to_host),
			dev->cpkts_from_host, dev->cpkts_to_modem,
			(dev->cpkts_from_host - dev->cpkts_to_modem),
			smd_read_avail(dev->smd_ctl.ch),
			smd_write_avail(dev->smd_ctl.ch),
			smd_read_avail(dev->smd_data.ch),
			smd_write_avail(dev->smd_data.ch));

	spin_unlock_irqrestore(&dev->lock, flags);

	ret = simple_read_from_buffer(ubuf, count, ppos, buf, ret);

	kfree(buf);

	return ret;
}

static ssize_t rmnet_smd_debug_reset_stats(struct file *file,
					const char __user *buf,
					size_t count, loff_t *ppos)
{
	struct rmnet_smd_dev *dev = file->private_data;
	unsigned long flags;

	spin_lock_irqsave(&dev->lock, flags);

	dev->dpkts_to_host = 0;
	dev->dpkts_from_modem = 0;
	dev->dpkts_from_host = 0;
	dev->dpkts_to_modem = 0;

	dev->cpkts_to_host = 0;
	dev->cpkts_from_modem = 0;
	dev->cpkts_from_host = 0;
	dev->cpkts_to_modem = 0;

	spin_unlock_irqrestore(&dev->lock, flags);

	return count;
}

static int rmnet_smd_debug_open(struct inode *inode, struct file *file)
{
	file->private_data = inode->i_private;

	return 0;
}

const struct file_operations rmnet_smd_debug_stats_ops = {
	.open = rmnet_smd_debug_open,
	.read = rmnet_smd_debug_read_stats,
	.write = rmnet_smd_debug_reset_stats,
};

struct dentry *dent_smd;
struct dentry *dent_smd_status;

static void rmnet_smd_debugfs_init(struct rmnet_smd_dev *dev)
{

	dent_smd = debugfs_create_dir("usb_rmnet_smd", 0);
	if (IS_ERR(dent_smd))
		return;

	dent_smd_status = debugfs_create_file("status", 0444, dent_smd, dev,
			&rmnet_smd_debug_stats_ops);

	if (!dent_smd_status) {
		debugfs_remove(dent_smd);
		dent_smd = NULL;
		return;
	}

	return;
}
#else
static void rmnet_smd_debugfs_init(struct rmnet_smd_dev *dev) {}
#endif

static void
rmnet_smd_unbind(struct usb_configuration *c, struct usb_function *f)
{
	struct rmnet_smd_dev *dev = container_of(f, struct rmnet_smd_dev,
								function);

	tasklet_kill(&dev->smd_ctl.rx_tlet);
	tasklet_kill(&dev->smd_ctl.tx_tlet);
	tasklet_kill(&dev->smd_data.rx_tlet);
	tasklet_kill(&dev->smd_data.tx_tlet);

	flush_workqueue(dev->wq);
	rmnet_smd_free_buf(dev);
	dev->epout = dev->epin = dev->epnotify = NULL; /* release endpoints */

	destroy_workqueue(dev->wq);
	debugfs_remove_recursive(dent_smd);
	kfree(dev);

}

int rmnet_smd_bind_config(struct usb_configuration *c)
{
	struct rmnet_smd_dev *dev;
	int ret;

	dev = kzalloc(sizeof(*dev), GFP_KERNEL);
	if (!dev)
		return -ENOMEM;

	rmnet_smd = dev;

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

	INIT_WORK(&dev->connect_work, rmnet_smd_connect_work);
	INIT_WORK(&dev->disconnect_work, rmnet_smd_disconnect_work);

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

	dev->pdrv.probe = rmnet_smd_ch_probe;
	dev->pdrv.driver.name = CONFIG_RMNET_SMD_CTL_CHANNEL;
	dev->pdrv.driver.owner = THIS_MODULE;

	INIT_LIST_HEAD(&dev->qmi_req_pool);
	INIT_LIST_HEAD(&dev->qmi_req_q);
	INIT_LIST_HEAD(&dev->qmi_resp_pool);
	INIT_LIST_HEAD(&dev->qmi_resp_q);
	INIT_LIST_HEAD(&dev->rx_idle);
	INIT_LIST_HEAD(&dev->rx_queue);
	INIT_LIST_HEAD(&dev->tx_idle);

	dev->function.name = "rmnet";
	dev->function.strings = rmnet_smd_strings;
	dev->function.fs_descriptors = rmnet_smd_fs_function;
	dev->function.hs_descriptors = rmnet_smd_hs_function;
	dev->function.bind = rmnet_smd_bind;
	dev->function.unbind = rmnet_smd_unbind;
	dev->function.setup = rmnet_smd_setup;
	dev->function.set_alt = rmnet_smd_set_alt;
	dev->function.disable = rmnet_smd_disable;

	ret = usb_add_function(c, &dev->function);
	if (ret)
		goto free_wq;

	rmnet_smd_debugfs_init(dev);

	return 0;

free_wq:
	destroy_workqueue(dev->wq);
free_dev:
	kfree(dev);

	return ret;
}
