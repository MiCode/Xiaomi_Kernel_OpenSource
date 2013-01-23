/*
 * f_rmnet_sdio.c -- RmNet SDIO function driver
 *
 * Copyright (C) 2003-2005,2008 David Brownell
 * Copyright (C) 2003-2004 Robert Schwebel, Benedikt Spranger
 * Copyright (C) 2003 Al Borchers (alborchers@steinerpoint.com)
 * Copyright (C) 2008 Nokia Corporation
 * Copyright (c) 2010-2011, The Linux Foundation. All rights reserved.
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
#include <linux/list.h>
#include <linux/device.h>
#include <linux/workqueue.h>
#include <linux/netdevice.h>

#include <linux/usb/cdc.h>
#include <linux/usb/composite.h>
#include <linux/usb/ch9.h>
#include <linux/termios.h>
#include <linux/debugfs.h>

#include <mach/sdio_cmux.h>
#include <mach/sdio_dmux.h>

#ifdef CONFIG_RMNET_SDIO_CTL_CHANNEL
static uint32_t rmnet_sdio_ctl_ch = CONFIG_RMNET_SDIO_CTL_CHANNEL;
#else
static uint32_t rmnet_sdio_ctl_ch;
#endif
module_param(rmnet_sdio_ctl_ch, uint, S_IRUGO);
MODULE_PARM_DESC(rmnet_sdio_ctl_ch, "RmNet control SDIO channel ID");

#ifdef CONFIG_RMNET_SDIO_DATA_CHANNEL
static uint32_t rmnet_sdio_data_ch = CONFIG_RMNET_SDIO_DATA_CHANNEL;
#else
static uint32_t rmnet_sdio_data_ch;
#endif
module_param(rmnet_sdio_data_ch, uint, S_IRUGO);
MODULE_PARM_DESC(rmnet_sdio_data_ch, "RmNet data SDIO channel ID");

#define ACM_CTRL_DTR				(1 << 0)

#define SDIO_MUX_HDR				8
#define RMNET_SDIO_NOTIFY_INTERVAL		5
#define RMNET_SDIO_MAX_NFY_SZE  sizeof(struct usb_cdc_notification)

#define RMNET_SDIO_RX_REQ_MAX			16
#define RMNET_SDIO_RX_REQ_SIZE			2048
#define RMNET_SDIO_TX_REQ_MAX			200

#define TX_PKT_DROP_THRESHOLD			1000
#define RX_PKT_FLOW_CTRL_EN_THRESHOLD		1000
#define RX_PKT_FLOW_CTRL_DISABLE		500

unsigned int sdio_tx_pkt_drop_thld = TX_PKT_DROP_THRESHOLD;
module_param(sdio_tx_pkt_drop_thld, uint, S_IRUGO | S_IWUSR);

unsigned int sdio_rx_fctrl_en_thld = RX_PKT_FLOW_CTRL_EN_THRESHOLD;
module_param(sdio_rx_fctrl_en_thld, uint, S_IRUGO | S_IWUSR);

unsigned int sdio_rx_fctrl_dis_thld = RX_PKT_FLOW_CTRL_DISABLE;
module_param(sdio_rx_fctrl_dis_thld, uint, S_IRUGO | S_IWUSR);

/* QMI requests & responses buffer*/
struct rmnet_sdio_qmi_buf {
	void *buf;
	int len;
	struct list_head list;
};

struct rmnet_sdio_dev {
	struct usb_function function;
	struct usb_composite_dev *cdev;

	struct usb_ep           *epout;
	struct usb_ep           *epin;
	struct usb_ep           *epnotify;
	struct usb_request      *notify_req;

	u8                      ifc_id;
	/* QMI lists */
	struct list_head        qmi_req_q;
	unsigned int		qreq_q_len;
	struct list_head        qmi_resp_q;
	unsigned int		qresp_q_len;
	/* Tx/Rx lists */
	struct list_head        tx_idle;
	unsigned int		tx_idle_len;
	struct sk_buff_head	tx_skb_queue;
	struct list_head        rx_idle;
	unsigned int		rx_idle_len;
	struct sk_buff_head	rx_skb_queue;

	spinlock_t              lock;
	atomic_t                online;
	atomic_t                notify_count;

	struct workqueue_struct *wq;
	struct work_struct disconnect_work;

	struct work_struct ctl_rx_work;
	struct work_struct data_rx_work;

	struct delayed_work sdio_open_work;
	struct work_struct sdio_close_work;
#define RMNET_SDIO_CH_OPEN	1
	unsigned long	data_ch_status;
	unsigned long	ctrl_ch_status;

	unsigned int dpkts_pending_atdmux;
	int cbits_to_modem;
	struct work_struct set_modem_ctl_bits_work;

	/* pkt logging dpkt - data pkt; cpkt - control pkt*/
	struct dentry *dent;
	unsigned long dpkt_tolaptop;
	unsigned long dpkt_tomodem;
	unsigned long tx_drp_cnt;
	unsigned long cpkt_tolaptop;
	unsigned long cpkt_tomodem;
};

static struct usb_interface_descriptor rmnet_sdio_interface_desc = {
	.bLength =              USB_DT_INTERFACE_SIZE,
	.bDescriptorType =      USB_DT_INTERFACE,
	/* .bInterfaceNumber = DYNAMIC */
	.bNumEndpoints =        3,
	.bInterfaceClass =      USB_CLASS_VENDOR_SPEC,
	.bInterfaceSubClass =   USB_CLASS_VENDOR_SPEC,
	.bInterfaceProtocol =   USB_CLASS_VENDOR_SPEC,
	/* .iInterface = DYNAMIC */
};

/* Full speed support */
static struct usb_endpoint_descriptor rmnet_sdio_fs_notify_desc = {
	.bLength =              USB_DT_ENDPOINT_SIZE,
	.bDescriptorType =      USB_DT_ENDPOINT,
	.bEndpointAddress =     USB_DIR_IN,
	.bmAttributes =         USB_ENDPOINT_XFER_INT,
	.wMaxPacketSize =	__constant_cpu_to_le16(RMNET_SDIO_MAX_NFY_SZE),
	.bInterval =            1 << RMNET_SDIO_NOTIFY_INTERVAL,
};

static struct usb_endpoint_descriptor rmnet_sdio_fs_in_desc  = {
	.bLength =              USB_DT_ENDPOINT_SIZE,
	.bDescriptorType =      USB_DT_ENDPOINT,
	.bEndpointAddress =     USB_DIR_IN,
	.bmAttributes =         USB_ENDPOINT_XFER_BULK,
	.wMaxPacketSize   = __constant_cpu_to_le16(64),
};

static struct usb_endpoint_descriptor rmnet_sdio_fs_out_desc = {
	.bLength =              USB_DT_ENDPOINT_SIZE,
	.bDescriptorType =      USB_DT_ENDPOINT,
	.bEndpointAddress =     USB_DIR_OUT,
	.bmAttributes =         USB_ENDPOINT_XFER_BULK,
	.wMaxPacketSize = __constant_cpu_to_le16(64),
};

static struct usb_descriptor_header *rmnet_sdio_fs_function[] = {
	(struct usb_descriptor_header *) &rmnet_sdio_interface_desc,
	(struct usb_descriptor_header *) &rmnet_sdio_fs_notify_desc,
	(struct usb_descriptor_header *) &rmnet_sdio_fs_in_desc,
	(struct usb_descriptor_header *) &rmnet_sdio_fs_out_desc,
	NULL,
};

/* High speed support */
static struct usb_endpoint_descriptor rmnet_sdio_hs_notify_desc  = {
	.bLength =              USB_DT_ENDPOINT_SIZE,
	.bDescriptorType =      USB_DT_ENDPOINT,
	.bEndpointAddress =     USB_DIR_IN,
	.bmAttributes =         USB_ENDPOINT_XFER_INT,
	.wMaxPacketSize =	__constant_cpu_to_le16(RMNET_SDIO_MAX_NFY_SZE),
	.bInterval =            RMNET_SDIO_NOTIFY_INTERVAL + 4,
};

static struct usb_endpoint_descriptor rmnet_sdio_hs_in_desc = {
	.bLength =              USB_DT_ENDPOINT_SIZE,
	.bDescriptorType =      USB_DT_ENDPOINT,
	.bEndpointAddress =     USB_DIR_IN,
	.bmAttributes =         USB_ENDPOINT_XFER_BULK,
	.wMaxPacketSize =	__constant_cpu_to_le16(512),
};

static struct usb_endpoint_descriptor rmnet_sdio_hs_out_desc = {
	.bLength =              USB_DT_ENDPOINT_SIZE,
	.bDescriptorType =      USB_DT_ENDPOINT,
	.bEndpointAddress =     USB_DIR_OUT,
	.bmAttributes =         USB_ENDPOINT_XFER_BULK,
	.wMaxPacketSize =	__constant_cpu_to_le16(512),
};

static struct usb_descriptor_header *rmnet_sdio_hs_function[] = {
	(struct usb_descriptor_header *) &rmnet_sdio_interface_desc,
	(struct usb_descriptor_header *) &rmnet_sdio_hs_notify_desc,
	(struct usb_descriptor_header *) &rmnet_sdio_hs_in_desc,
	(struct usb_descriptor_header *) &rmnet_sdio_hs_out_desc,
	NULL,
};

/* String descriptors */

static struct usb_string rmnet_sdio_string_defs[] = {
	[0].s = "QMI RmNet",
	{  } /* end of list */
};

static struct usb_gadget_strings rmnet_sdio_string_table = {
	.language =             0x0409, /* en-us */
	.strings =              rmnet_sdio_string_defs,
};

static struct usb_gadget_strings *rmnet_sdio_strings[] = {
	&rmnet_sdio_string_table,
	NULL,
};

static struct rmnet_sdio_qmi_buf *
rmnet_sdio_alloc_qmi(unsigned len, gfp_t kmalloc_flags)

{
	struct rmnet_sdio_qmi_buf *qmi;

	qmi = kmalloc(sizeof(struct rmnet_sdio_qmi_buf), kmalloc_flags);
	if (qmi != NULL) {
		qmi->buf = kmalloc(len, kmalloc_flags);
		if (qmi->buf == NULL) {
			kfree(qmi);
			qmi = NULL;
		}
	}

	return qmi ? qmi : ERR_PTR(-ENOMEM);
}

static void rmnet_sdio_free_qmi(struct rmnet_sdio_qmi_buf *qmi)
{
	kfree(qmi->buf);
	kfree(qmi);
}
/*
 * Allocate a usb_request and its buffer.  Returns a pointer to the
 * usb_request or a pointer with an error code if there is an error.
 */
static struct usb_request *
rmnet_sdio_alloc_req(struct usb_ep *ep, unsigned len, gfp_t kmalloc_flags)
{
	struct usb_request *req;

	req = usb_ep_alloc_request(ep, kmalloc_flags);

	if (len && req != NULL) {
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
static void rmnet_sdio_free_req(struct usb_ep *ep, struct usb_request *req)
{
	kfree(req->buf);
	usb_ep_free_request(ep, req);
}

static void rmnet_sdio_notify_complete(struct usb_ep *ep,
					struct usb_request *req)
{
	struct rmnet_sdio_dev *dev = req->context;
	struct usb_composite_dev *cdev = dev->cdev;
	int status = req->status;

	switch (status) {
	case -ECONNRESET:
	case -ESHUTDOWN:
		/* connection gone */
		atomic_set(&dev->notify_count, 0);
		break;
	default:
		ERROR(cdev, "rmnet notifyep error %d\n", status);
		/* FALLTHROUGH */
	case 0:

		if (!test_bit(RMNET_SDIO_CH_OPEN, &dev->ctrl_ch_status))
			return;

		/* handle multiple pending QMI_RESPONSE_AVAILABLE
		 * notifications by resending until we're done
		 */
		if (atomic_dec_and_test(&dev->notify_count))
			break;

		status = usb_ep_queue(dev->epnotify, req, GFP_ATOMIC);
		if (status) {
			atomic_dec(&dev->notify_count);
			ERROR(cdev, "rmnet notify ep enq error %d\n", status);
		}
		break;
	}
}

static void rmnet_sdio_qmi_resp_available(struct rmnet_sdio_dev *dev)
{
	struct usb_composite_dev *cdev = dev->cdev;
	struct usb_cdc_notification     *event;
	int status;
	unsigned long flags;

	/* Response will be sent later */
	if (atomic_inc_return(&dev->notify_count) != 1)
		return;

	spin_lock_irqsave(&dev->lock, flags);

	if (!atomic_read(&dev->online)) {
		spin_unlock_irqrestore(&dev->lock, flags);
		return;
	}

	event = dev->notify_req->buf;

	event->bmRequestType = USB_DIR_IN | USB_TYPE_CLASS
			| USB_RECIP_INTERFACE;
	event->bNotificationType = USB_CDC_NOTIFY_RESPONSE_AVAILABLE;
	event->wValue = cpu_to_le16(0);
	event->wIndex = cpu_to_le16(dev->ifc_id);
	event->wLength = cpu_to_le16(0);
	spin_unlock_irqrestore(&dev->lock, flags);

	status = usb_ep_queue(dev->epnotify, dev->notify_req, GFP_ATOMIC);
	if (status < 0) {
		if (atomic_read(&dev->online))
			atomic_dec(&dev->notify_count);
		ERROR(cdev, "rmnet notify ep enqueue error %d\n", status);
	}
}

#define SDIO_MAX_CTRL_PKT_SIZE	4096
static void rmnet_sdio_ctl_receive_cb(void *data, int size, void *priv)
{
	struct rmnet_sdio_dev *dev = priv;
	struct usb_composite_dev *cdev = dev->cdev;
	struct rmnet_sdio_qmi_buf *qmi_resp;
	unsigned long flags;

	if (!data) {
		pr_info("%s: cmux_ch close event\n", __func__);
		if (test_bit(RMNET_SDIO_CH_OPEN, &dev->ctrl_ch_status) &&
		    test_bit(RMNET_SDIO_CH_OPEN, &dev->data_ch_status)) {
			clear_bit(RMNET_SDIO_CH_OPEN, &dev->ctrl_ch_status);
			clear_bit(RMNET_SDIO_CH_OPEN, &dev->data_ch_status);
			queue_work(dev->wq, &dev->sdio_close_work);
		}
		return;
	}

	if (!size || !test_bit(RMNET_SDIO_CH_OPEN, &dev->ctrl_ch_status))
		return;


	if (size > SDIO_MAX_CTRL_PKT_SIZE) {
		ERROR(cdev, "ctrl pkt size:%d exceeds max pkt size:%d\n",
				size, SDIO_MAX_CTRL_PKT_SIZE);
		return;
	}

	if (!atomic_read(&dev->online)) {
		DBG(cdev, "USB disconnected\n");
		return;
	}

	qmi_resp = rmnet_sdio_alloc_qmi(size, GFP_KERNEL);
	if (IS_ERR(qmi_resp)) {
		DBG(cdev, "unable to allocate memory for QMI resp\n");
		return;
	}
	memcpy(qmi_resp->buf, data, size);
	qmi_resp->len = size;
	spin_lock_irqsave(&dev->lock, flags);
	list_add_tail(&qmi_resp->list, &dev->qmi_resp_q);
	dev->qresp_q_len++;
	spin_unlock_irqrestore(&dev->lock, flags);

	rmnet_sdio_qmi_resp_available(dev);
}

static void rmnet_sdio_ctl_write_done(void *data, int size, void *priv)
{
	struct rmnet_sdio_dev *dev = priv;
	struct usb_composite_dev *cdev = dev->cdev;

	VDBG(cdev, "rmnet control write done = %d bytes\n", size);
}

static void rmnet_sdio_sts_callback(int id, void *priv)
{
	struct rmnet_sdio_dev *dev = priv;
	struct usb_composite_dev *cdev = dev->cdev;

	DBG(cdev, "rmnet_sdio_sts_callback: id: %d\n", id);
}

static void rmnet_sdio_control_rx_work(struct work_struct *w)
{
	struct rmnet_sdio_dev *dev = container_of(w, struct rmnet_sdio_dev,
						 ctl_rx_work);
	struct usb_composite_dev *cdev = dev->cdev;
	struct rmnet_sdio_qmi_buf *qmi_req;
	unsigned long flags;
	int ret;

	while (1) {
		spin_lock_irqsave(&dev->lock, flags);
		if (list_empty(&dev->qmi_req_q))
			goto unlock;

		qmi_req = list_first_entry(&dev->qmi_req_q,
					struct rmnet_sdio_qmi_buf, list);
		list_del(&qmi_req->list);
		dev->qreq_q_len--;
		spin_unlock_irqrestore(&dev->lock, flags);

		ret = sdio_cmux_write(rmnet_sdio_ctl_ch, qmi_req->buf,
					qmi_req->len);
		if (ret != qmi_req->len) {
			ERROR(cdev, "rmnet control SDIO write failed\n");
			return;
		}

		dev->cpkt_tomodem++;

		/*
		 * cmux_write API copies the buffer and gives it to sdio_al.
		 * Hence freeing the memory before write is completed.
		 */
		rmnet_sdio_free_qmi(qmi_req);
	}
unlock:
	spin_unlock_irqrestore(&dev->lock, flags);
}

static void rmnet_sdio_response_complete(struct usb_ep *ep,
					 struct usb_request *req)
{
	struct rmnet_sdio_dev *dev = req->context;
	struct usb_composite_dev *cdev = dev->cdev;

	switch (req->status) {
	case -ECONNRESET:
	case -ESHUTDOWN:
	case 0:
		return;
	default:
		INFO(cdev, "rmnet %s response error %d, %d/%d\n",
			ep->name, req->status,
			req->actual, req->length);
	}
}

static void rmnet_sdio_command_complete(struct usb_ep *ep,
					struct usb_request *req)
{
	struct rmnet_sdio_dev *dev = req->context;
	struct usb_composite_dev *cdev = dev->cdev;
	struct rmnet_sdio_qmi_buf *qmi_req;
	int len = req->actual;

	if (req->status < 0) {
		ERROR(cdev, "rmnet command error %d\n", req->status);
		return;
	}

	/* discard the packet if sdio is not available */
	if (!test_bit(RMNET_SDIO_CH_OPEN, &dev->ctrl_ch_status))
		return;

	qmi_req = rmnet_sdio_alloc_qmi(len, GFP_ATOMIC);
	if (IS_ERR(qmi_req)) {
		ERROR(cdev, "unable to allocate memory for QMI req\n");
		return;
	}
	memcpy(qmi_req->buf, req->buf, len);
	qmi_req->len = len;
	spin_lock(&dev->lock);
	list_add_tail(&qmi_req->list, &dev->qmi_req_q);
	dev->qreq_q_len++;
	spin_unlock(&dev->lock);
	queue_work(dev->wq, &dev->ctl_rx_work);
}

static int
rmnet_sdio_setup(struct usb_function *f, const struct usb_ctrlrequest *ctrl)
{
	struct rmnet_sdio_dev *dev = container_of(f, struct rmnet_sdio_dev,
							 function);
	struct usb_composite_dev *cdev = f->config->cdev;
	struct usb_request      *req = cdev->req;
	int                     ret = -EOPNOTSUPP;
	u16                     w_index = le16_to_cpu(ctrl->wIndex);
	u16                     w_value = le16_to_cpu(ctrl->wValue);
	u16                     w_length = le16_to_cpu(ctrl->wLength);
	struct rmnet_sdio_qmi_buf *resp;

	if (!atomic_read(&dev->online))
		return -ENOTCONN;

	switch ((ctrl->bRequestType << 8) | ctrl->bRequest) {

	case ((USB_DIR_OUT | USB_TYPE_CLASS | USB_RECIP_INTERFACE) << 8)
			| USB_CDC_SEND_ENCAPSULATED_COMMAND:
		ret = w_length;
		req->complete = rmnet_sdio_command_complete;
		req->context = dev;
		break;


	case ((USB_DIR_IN | USB_TYPE_CLASS | USB_RECIP_INTERFACE) << 8)
			| USB_CDC_GET_ENCAPSULATED_RESPONSE:
		if (w_value)
			goto invalid;
		else {
			unsigned len;

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
				struct rmnet_sdio_qmi_buf, list);
			list_del(&resp->list);
			dev->qresp_q_len--;
			spin_unlock(&dev->lock);

			len = min_t(unsigned, w_length, resp->len);
			memcpy(req->buf, resp->buf, len);
			ret = len;
			req->context = dev;
			req->complete = rmnet_sdio_response_complete;
			rmnet_sdio_free_qmi(resp);

			/* check if its the right place to add */
			dev->cpkt_tolaptop++;
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
		if (w_value & ACM_CTRL_DTR)
			dev->cbits_to_modem |= TIOCM_DTR;
		else
			dev->cbits_to_modem &= ~TIOCM_DTR;
		queue_work(dev->wq, &dev->set_modem_ctl_bits_work);

		ret = 0;

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
		req->zero = (ret < w_length);
		req->length = ret;
		ret = usb_ep_queue(cdev->gadget->ep0, req, GFP_ATOMIC);
		if (ret < 0)
			ERROR(cdev, "rmnet ep0 enqueue err %d\n", ret);
	}

	return ret;
}

static int
rmnet_sdio_rx_submit(struct rmnet_sdio_dev *dev, struct usb_request *req,
						 gfp_t gfp_flags)
{
	struct sk_buff *skb;
	int retval;

	skb = alloc_skb(RMNET_SDIO_RX_REQ_SIZE + SDIO_MUX_HDR, gfp_flags);
	if (skb == NULL)
		return -ENOMEM;
	skb_reserve(skb, SDIO_MUX_HDR);

	req->buf = skb->data;
	req->length = RMNET_SDIO_RX_REQ_SIZE;
	req->context = skb;

	retval = usb_ep_queue(dev->epout, req, gfp_flags);
	if (retval)
		dev_kfree_skb_any(skb);

	return retval;
}

static void rmnet_sdio_start_rx(struct rmnet_sdio_dev *dev)
{
	struct usb_composite_dev *cdev = dev->cdev;
	int status;
	struct usb_request *req;
	unsigned long flags;

	if (!atomic_read(&dev->online)) {
		pr_err("%s: USB not connected\n", __func__);
		return;
	}

	spin_lock_irqsave(&dev->lock, flags);
	while (!list_empty(&dev->rx_idle)) {
		req = list_first_entry(&dev->rx_idle, struct usb_request, list);
		list_del(&req->list);
		dev->rx_idle_len--;

		spin_unlock_irqrestore(&dev->lock, flags);
		status = rmnet_sdio_rx_submit(dev, req, GFP_ATOMIC);
		spin_lock_irqsave(&dev->lock, flags);

		if (status) {
			ERROR(cdev, "rmnet data rx enqueue err %d\n", status);
			list_add_tail(&req->list, &dev->rx_idle);
			dev->rx_idle_len++;
			break;
		}
	}
	spin_unlock_irqrestore(&dev->lock, flags);
}

static void rmnet_sdio_start_tx(struct rmnet_sdio_dev *dev)
{
	unsigned long			flags;
	int				status;
	struct sk_buff			*skb;
	struct usb_request		*req;
	struct usb_composite_dev	*cdev = dev->cdev;

	if (!atomic_read(&dev->online))
		return;

	if (!test_bit(RMNET_SDIO_CH_OPEN, &dev->data_ch_status))
		return;

	spin_lock_irqsave(&dev->lock, flags);
	while (!list_empty(&dev->tx_idle)) {
		skb = __skb_dequeue(&dev->tx_skb_queue);
		if (!skb) {
			spin_unlock_irqrestore(&dev->lock, flags);
			return;
		}

		req = list_first_entry(&dev->tx_idle, struct usb_request, list);
		req->context = skb;
		req->buf = skb->data;
		req->length = skb->len;

		list_del(&req->list);
		dev->tx_idle_len--;
		spin_unlock(&dev->lock);
		status = usb_ep_queue(dev->epin, req, GFP_ATOMIC);
		spin_lock(&dev->lock);
		if (status) {
			/* USB still online, queue requests back */
			if (atomic_read(&dev->online)) {
				ERROR(cdev, "rmnet tx data enqueue err %d\n",
						status);
				list_add_tail(&req->list, &dev->tx_idle);
				dev->tx_idle_len++;
				__skb_queue_head(&dev->tx_skb_queue, skb);
			} else {
				req->buf = 0;
				rmnet_sdio_free_req(dev->epin, req);
				dev_kfree_skb_any(skb);
			}
			break;
		}
		dev->dpkt_tolaptop++;
	}
	spin_unlock_irqrestore(&dev->lock, flags);
}

static void rmnet_sdio_data_receive_cb(void *priv, struct sk_buff *skb)
{
	struct rmnet_sdio_dev *dev = priv;
	unsigned long flags;

	/* SDIO mux sends NULL SKB when link state changes */
	if (!skb) {
		pr_info("%s: dmux_ch close event\n", __func__);
		if (test_bit(RMNET_SDIO_CH_OPEN, &dev->ctrl_ch_status) &&
		    test_bit(RMNET_SDIO_CH_OPEN, &dev->data_ch_status)) {
			clear_bit(RMNET_SDIO_CH_OPEN, &dev->ctrl_ch_status);
			clear_bit(RMNET_SDIO_CH_OPEN, &dev->data_ch_status);
			queue_work(dev->wq, &dev->sdio_close_work);
		}
		return;
	}

	if (!atomic_read(&dev->online)) {
		dev_kfree_skb_any(skb);
		return;
	}

	spin_lock_irqsave(&dev->lock, flags);

	if (dev->tx_skb_queue.qlen > sdio_tx_pkt_drop_thld) {
		if (printk_ratelimit())
			pr_err("%s: tx pkt dropped: tx_drop_cnt:%lu\n",
					__func__, dev->tx_drp_cnt);
		dev->tx_drp_cnt++;
		spin_unlock_irqrestore(&dev->lock, flags);
		dev_kfree_skb_any(skb);
		return;
	}

	__skb_queue_tail(&dev->tx_skb_queue, skb);
	spin_unlock_irqrestore(&dev->lock, flags);

	rmnet_sdio_start_tx(dev);
}

static void rmnet_sdio_data_write_done(void *priv, struct sk_buff *skb)
{
	struct rmnet_sdio_dev *dev = priv;

	/* SDIO mux sends NULL SKB when link state changes */
	if (!skb) {
		pr_info("%s: dmux_ch open event\n", __func__);
		queue_delayed_work(dev->wq, &dev->sdio_open_work, 0);
		return;
	}

	dev_kfree_skb_any(skb);
	/* this function is called from
	 * sdio mux from spin_lock_irqsave
	 */
	spin_lock(&dev->lock);
	dev->dpkts_pending_atdmux--;

	if (!test_bit(RMNET_SDIO_CH_OPEN, &dev->data_ch_status) ||
			dev->dpkts_pending_atdmux >= sdio_rx_fctrl_dis_thld) {
		spin_unlock(&dev->lock);
		return;
	}
	spin_unlock(&dev->lock);

	rmnet_sdio_start_rx(dev);
}

static void rmnet_sdio_data_rx_work(struct work_struct *w)
{
	struct rmnet_sdio_dev *dev = container_of(w, struct rmnet_sdio_dev,
							data_rx_work);
	struct usb_composite_dev *cdev = dev->cdev;
	struct sk_buff *skb;
	int ret;
	unsigned long flags;

	if (!test_bit(RMNET_SDIO_CH_OPEN, &dev->data_ch_status)) {
		pr_info("%s: sdio data ch not open\n", __func__);
		return;
	}

	spin_lock_irqsave(&dev->lock, flags);
	while ((skb = __skb_dequeue(&dev->rx_skb_queue))) {
		spin_unlock_irqrestore(&dev->lock, flags);
		ret = msm_sdio_dmux_write(rmnet_sdio_data_ch, skb);
		spin_lock_irqsave(&dev->lock, flags);
		if (ret < 0) {
			ERROR(cdev, "rmnet SDIO data write failed\n");
			dev_kfree_skb_any(skb);
			break;
		} else {
			dev->dpkt_tomodem++;
			dev->dpkts_pending_atdmux++;
		}
	}
	spin_unlock_irqrestore(&dev->lock, flags);
}

static void rmnet_sdio_complete_epout(struct usb_ep *ep,
					struct usb_request *req)
{
	struct rmnet_sdio_dev *dev = ep->driver_data;
	struct usb_composite_dev *cdev = dev->cdev;
	struct sk_buff *skb = req->context;
	int status = req->status;
	int queue = 0;

	switch (status) {
	case 0:
		/* successful completion */
		skb_put(skb, req->actual);
		queue = 1;
		break;
	case -ECONNRESET:
	case -ESHUTDOWN:
		/* connection gone */
		dev_kfree_skb_any(skb);
		req->buf = 0;
		rmnet_sdio_free_req(ep, req);
		return;
	default:
		/* unexpected failure */
		ERROR(cdev, "RMNET %s response error %d, %d/%d\n",
			ep->name, status,
			req->actual, req->length);
		dev_kfree_skb_any(skb);
		break;
	}

	if (!test_bit(RMNET_SDIO_CH_OPEN, &dev->data_ch_status)) {
		pr_info("%s: sdio data ch not open\n", __func__);
		dev_kfree_skb_any(skb);
		req->buf = 0;
		rmnet_sdio_free_req(ep, req);
		return;
	}

	spin_lock(&dev->lock);
	if (queue) {
		__skb_queue_tail(&dev->rx_skb_queue, skb);
		queue_work(dev->wq, &dev->data_rx_work);
	}

	if (dev->dpkts_pending_atdmux >= sdio_rx_fctrl_en_thld) {
		list_add_tail(&req->list, &dev->rx_idle);
		dev->rx_idle_len++;
		spin_unlock(&dev->lock);
		return;
	}
	spin_unlock(&dev->lock);

	status = rmnet_sdio_rx_submit(dev, req, GFP_ATOMIC);
	if (status) {
		ERROR(cdev, "rmnet data rx enqueue err %d\n", status);
		list_add_tail(&req->list, &dev->rx_idle);
		dev->rx_idle_len++;
	}
}

static void rmnet_sdio_complete_epin(struct usb_ep *ep, struct usb_request *req)
{
	struct rmnet_sdio_dev *dev = ep->driver_data;
	struct sk_buff  *skb = req->context;
	struct usb_composite_dev *cdev = dev->cdev;
	int status = req->status;

	switch (status) {
	case 0:
		/* successful completion */
	case -ECONNRESET:
	case -ESHUTDOWN:
		/* connection gone */
		break;
	default:
		ERROR(cdev, "rmnet data tx ep error %d\n", status);
		break;
	}

	spin_lock(&dev->lock);
	list_add_tail(&req->list, &dev->tx_idle);
	dev->tx_idle_len++;
	spin_unlock(&dev->lock);
	dev_kfree_skb_any(skb);

	rmnet_sdio_start_tx(dev);
}

static void rmnet_sdio_free_buf(struct rmnet_sdio_dev *dev)
{
	struct rmnet_sdio_qmi_buf *qmi;
	struct usb_request *req;
	struct list_head *act, *tmp;
	struct sk_buff *skb;
	unsigned long flags;


	spin_lock_irqsave(&dev->lock, flags);

	dev->dpkt_tolaptop = 0;
	dev->dpkt_tomodem = 0;
	dev->cpkt_tolaptop = 0;
	dev->cpkt_tomodem = 0;
	dev->dpkts_pending_atdmux = 0;
	dev->tx_drp_cnt = 0;

	/* free all usb requests in tx pool */
	list_for_each_safe(act, tmp, &dev->tx_idle) {
		req = list_entry(act, struct usb_request, list);
		list_del(&req->list);
		dev->tx_idle_len--;
		req->buf = NULL;
		rmnet_sdio_free_req(dev->epout, req);
	}

	/* free all usb requests in rx pool */
	list_for_each_safe(act, tmp, &dev->rx_idle) {
		req = list_entry(act, struct usb_request, list);
		list_del(&req->list);
		dev->rx_idle_len--;
		req->buf = NULL;
		rmnet_sdio_free_req(dev->epin, req);
	}

	/* free all buffers in qmi request pool */
	list_for_each_safe(act, tmp, &dev->qmi_req_q) {
		qmi = list_entry(act, struct rmnet_sdio_qmi_buf, list);
		list_del(&qmi->list);
		dev->qreq_q_len--;
		rmnet_sdio_free_qmi(qmi);
	}

	/* free all buffers in qmi request pool */
	list_for_each_safe(act, tmp, &dev->qmi_resp_q) {
		qmi = list_entry(act, struct rmnet_sdio_qmi_buf, list);
		list_del(&qmi->list);
		dev->qresp_q_len--;
		rmnet_sdio_free_qmi(qmi);
	}

	while ((skb = __skb_dequeue(&dev->tx_skb_queue)))
		dev_kfree_skb_any(skb);

	while ((skb = __skb_dequeue(&dev->rx_skb_queue)))
		dev_kfree_skb_any(skb);

	rmnet_sdio_free_req(dev->epnotify, dev->notify_req);

	spin_unlock_irqrestore(&dev->lock, flags);
}

static void rmnet_sdio_set_modem_cbits_w(struct work_struct *w)
{
	struct rmnet_sdio_dev *dev;

	dev = container_of(w, struct rmnet_sdio_dev, set_modem_ctl_bits_work);

	if (!test_bit(RMNET_SDIO_CH_OPEN, &dev->ctrl_ch_status))
		return;

	pr_debug("%s: cbits_to_modem:%d\n",
			__func__, dev->cbits_to_modem);

	sdio_cmux_tiocmset(rmnet_sdio_ctl_ch,
			dev->cbits_to_modem,
			~dev->cbits_to_modem);
}

static void rmnet_sdio_disconnect_work(struct work_struct *w)
{
	/* REVISIT: Push all the data to sdio if anythign is pending */
}
static void rmnet_sdio_suspend(struct usb_function *f)
{
	struct rmnet_sdio_dev *dev = container_of(f, struct rmnet_sdio_dev,
								function);

	if (!atomic_read(&dev->online))
		return;
	/* This is a workaround for Windows Host bug during suspend.
	 * Windows 7/xp Hosts are suppose to drop DTR, when Host suspended.
	 * Since it is not beind done, Hence exclusively dropping the DTR
	 * from function driver suspend.
	 */
	dev->cbits_to_modem &= ~TIOCM_DTR;
	queue_work(dev->wq, &dev->set_modem_ctl_bits_work);
}
static void rmnet_sdio_disable(struct usb_function *f)
{
	struct rmnet_sdio_dev *dev = container_of(f, struct rmnet_sdio_dev,
								 function);

	if (!atomic_read(&dev->online))
		return;

	usb_ep_disable(dev->epnotify);
	usb_ep_disable(dev->epout);
	usb_ep_disable(dev->epin);

	atomic_set(&dev->online, 0);
	atomic_set(&dev->notify_count, 0);
	rmnet_sdio_free_buf(dev);

	/* cleanup work */
	queue_work(dev->wq, &dev->disconnect_work);
	dev->cbits_to_modem = 0;
	queue_work(dev->wq, &dev->set_modem_ctl_bits_work);
}

static void rmnet_close_sdio_work(struct work_struct *w)
{
	struct rmnet_sdio_dev		*dev;
	unsigned long			flags;
	struct usb_cdc_notification     *event;
	int				status;
	struct rmnet_sdio_qmi_buf	*qmi;
	struct usb_request		*req;
	struct sk_buff			*skb;

	pr_debug("%s:\n", __func__);

	dev = container_of(w, struct rmnet_sdio_dev, sdio_close_work);

	if (!atomic_read(&dev->online))
		return;

	usb_ep_fifo_flush(dev->epnotify);

	spin_lock_irqsave(&dev->lock, flags);
	event = dev->notify_req->buf;

	event->bmRequestType = USB_DIR_IN | USB_TYPE_CLASS
			| USB_RECIP_INTERFACE;
	event->bNotificationType = USB_CDC_NOTIFY_NETWORK_CONNECTION;
	event->wValue = cpu_to_le16(0);
	event->wIndex = cpu_to_le16(dev->ifc_id);
	event->wLength = cpu_to_le16(0);
	spin_unlock_irqrestore(&dev->lock, flags);

	status = usb_ep_queue(dev->epnotify, dev->notify_req, GFP_KERNEL);
	if (status < 0) {
		if (!atomic_read(&dev->online))
			return;
		pr_err("%s: rmnet notify ep enqueue error %d\n",
				__func__, status);
	}

	usb_ep_fifo_flush(dev->epout);
	usb_ep_fifo_flush(dev->epin);
	cancel_work_sync(&dev->data_rx_work);

	spin_lock_irqsave(&dev->lock, flags);

	if (!atomic_read(&dev->online)) {
		spin_unlock_irqrestore(&dev->lock, flags);
		return;
	}

	/* free all usb requests in tx pool */
	while (!list_empty(&dev->tx_idle)) {
		req = list_first_entry(&dev->tx_idle, struct usb_request, list);
		list_del(&req->list);
		dev->tx_idle_len--;
		req->buf = NULL;
		rmnet_sdio_free_req(dev->epout, req);
	}

	/* free all usb requests in rx pool */
	while (!list_empty(&dev->rx_idle)) {
		req = list_first_entry(&dev->rx_idle, struct usb_request, list);
		list_del(&req->list);
		dev->rx_idle_len--;
		req->buf = NULL;
		rmnet_sdio_free_req(dev->epin, req);
	}

	/* free all buffers in qmi request pool */
	while (!list_empty(&dev->qmi_req_q)) {
		qmi = list_first_entry(&dev->qmi_req_q,
				struct rmnet_sdio_qmi_buf, list);
		list_del(&qmi->list);
		dev->qreq_q_len--;
		rmnet_sdio_free_qmi(qmi);
	}

	/* free all buffers in qmi response pool */
	while (!list_empty(&dev->qmi_resp_q)) {
		qmi = list_first_entry(&dev->qmi_resp_q,
				struct rmnet_sdio_qmi_buf, list);
		list_del(&qmi->list);
		dev->qresp_q_len--;
		rmnet_sdio_free_qmi(qmi);
	}
	atomic_set(&dev->notify_count, 0);

	pr_info("%s: setting notify count to zero\n", __func__);


	while ((skb = __skb_dequeue(&dev->tx_skb_queue)))
		dev_kfree_skb_any(skb);

	while ((skb = __skb_dequeue(&dev->rx_skb_queue)))
		dev_kfree_skb_any(skb);
	spin_unlock_irqrestore(&dev->lock, flags);
}

static int rmnet_sdio_start_io(struct rmnet_sdio_dev *dev)
{
	struct usb_request *req;
	int ret, i;
	unsigned long flags;

	spin_lock_irqsave(&dev->lock, flags);
	if (!atomic_read(&dev->online)) {
		spin_unlock_irqrestore(&dev->lock, flags);
		return 0;
	}

	if (!test_bit(RMNET_SDIO_CH_OPEN, &dev->data_ch_status) ||
			!test_bit(RMNET_SDIO_CH_OPEN, &dev->ctrl_ch_status)) {
		spin_unlock_irqrestore(&dev->lock, flags);
		return 0;
	}

	for (i = 0; i < RMNET_SDIO_RX_REQ_MAX; i++) {
		req = rmnet_sdio_alloc_req(dev->epout, 0, GFP_ATOMIC);
		if (IS_ERR(req)) {
			ret = PTR_ERR(req);
			spin_unlock_irqrestore(&dev->lock, flags);
			goto free_buf;
		}
		req->complete = rmnet_sdio_complete_epout;
		list_add_tail(&req->list, &dev->rx_idle);
		dev->rx_idle_len++;
	}
	for (i = 0; i < RMNET_SDIO_TX_REQ_MAX; i++) {
		req = rmnet_sdio_alloc_req(dev->epin, 0, GFP_ATOMIC);
		if (IS_ERR(req)) {
			ret = PTR_ERR(req);
			spin_unlock_irqrestore(&dev->lock, flags);
			goto free_buf;
		}
		req->complete = rmnet_sdio_complete_epin;
		list_add_tail(&req->list, &dev->tx_idle);
		dev->tx_idle_len++;
	}
	spin_unlock_irqrestore(&dev->lock, flags);

	/* Queue Rx data requests */
	rmnet_sdio_start_rx(dev);

	return 0;

free_buf:
	rmnet_sdio_free_buf(dev);
	dev->epout = dev->epin = dev->epnotify = NULL; /* release endpoints */
	return ret;
}


#define RMNET_SDIO_OPEN_RETRY_DELAY	msecs_to_jiffies(2000)
#define SDIO_SDIO_OPEN_MAX_RETRY	90
static void rmnet_open_sdio_work(struct work_struct *w)
{
	struct rmnet_sdio_dev *dev =
			container_of(w, struct rmnet_sdio_dev,
						sdio_open_work.work);
	struct usb_composite_dev *cdev = dev->cdev;
	int ret;
	static int retry_cnt;

	if (!test_bit(RMNET_SDIO_CH_OPEN, &dev->ctrl_ch_status)) {
		/* Control channel for QMI messages */
		ret = sdio_cmux_open(rmnet_sdio_ctl_ch,
				rmnet_sdio_ctl_receive_cb,
				rmnet_sdio_ctl_write_done,
				rmnet_sdio_sts_callback, dev);
		if (!ret)
			set_bit(RMNET_SDIO_CH_OPEN, &dev->ctrl_ch_status);
	}

	if (!test_bit(RMNET_SDIO_CH_OPEN, &dev->data_ch_status)) {
		/* Data channel for network packets */
		ret = msm_sdio_dmux_open(rmnet_sdio_data_ch, dev,
				rmnet_sdio_data_receive_cb,
				rmnet_sdio_data_write_done);
		if (!ret)
			set_bit(RMNET_SDIO_CH_OPEN, &dev->data_ch_status);
	}

	if (test_bit(RMNET_SDIO_CH_OPEN, &dev->data_ch_status) &&
			test_bit(RMNET_SDIO_CH_OPEN, &dev->ctrl_ch_status)) {

		rmnet_sdio_start_io(dev);

		/* if usb cable is connected, update DTR status to modem */
		if (atomic_read(&dev->online))
			queue_work(dev->wq, &dev->set_modem_ctl_bits_work);

		pr_info("%s: usb rmnet sdio channels are open retry_cnt:%d\n",
				__func__, retry_cnt);
		retry_cnt = 0;
		return;
	}

	retry_cnt++;
	pr_debug("%s: usb rmnet sdio open retry_cnt:%d\n",
			__func__, retry_cnt);

	if (retry_cnt > SDIO_SDIO_OPEN_MAX_RETRY) {
		if (!test_bit(RMNET_SDIO_CH_OPEN, &dev->ctrl_ch_status))
			ERROR(cdev, "Unable to open control SDIO channel\n");

		if (!test_bit(RMNET_SDIO_CH_OPEN, &dev->data_ch_status))
			ERROR(cdev, "Unable to open DATA SDIO channel\n");

	} else {
		queue_delayed_work(dev->wq, &dev->sdio_open_work,
				RMNET_SDIO_OPEN_RETRY_DELAY);
	}
}

static int rmnet_sdio_set_alt(struct usb_function *f,
			unsigned intf, unsigned alt)
{
	struct rmnet_sdio_dev *dev = container_of(f, struct rmnet_sdio_dev,
								function);
	struct usb_composite_dev *cdev = dev->cdev;
	int ret = 0;

	/* Enable epin */
	dev->epin->driver_data = dev;
	ret = config_ep_by_speed(cdev->gadget, f, dev->epin);
	if (ret) {
		dev->epin->desc = NULL;
		ERROR(cdev, "config_ep_by_speed failes for ep %s, result %d\n",
			dev->epin->name, ret);
		return ret;
	}
	ret = usb_ep_enable(dev->epin);
	if (ret) {
		ERROR(cdev, "can't enable %s, result %d\n",
			dev->epin->name, ret);
		return ret;
	}

	/* Enable epout */
	dev->epout->driver_data = dev;
	ret = config_ep_by_speed(cdev->gadget, f, dev->epout);
	if (ret) {
		dev->epout->desc = NULL;
		ERROR(cdev, "config_ep_by_speed failes for ep %s, result %d\n",
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

	/* Enable epnotify */
	ret = config_ep_by_speed(cdev->gadget, f, dev->epnotify);
	if (ret) {
		dev->epnotify->desc = NULL;
		ERROR(cdev, "config_ep_by_speed failes for ep %s, result %d\n",
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

	/* allocate notification */
	dev->notify_req = rmnet_sdio_alloc_req(dev->epnotify,
				RMNET_SDIO_MAX_NFY_SZE, GFP_ATOMIC);

	if (IS_ERR(dev->notify_req)) {
		ret = PTR_ERR(dev->notify_req);
		pr_err("%s: unable to allocate memory for notify ep\n",
				__func__);
		return ret;
	}
	dev->notify_req->complete = rmnet_sdio_notify_complete;
	dev->notify_req->context = dev;
	dev->notify_req->length = RMNET_SDIO_MAX_NFY_SZE;

	atomic_set(&dev->online, 1);

	ret = rmnet_sdio_start_io(dev);

	return ret;

}

static int rmnet_sdio_bind(struct usb_configuration *c, struct usb_function *f)
{
	struct usb_composite_dev *cdev = c->cdev;
	struct rmnet_sdio_dev *dev = container_of(f, struct rmnet_sdio_dev,
								function);
	int id;
	struct usb_ep *ep;

	dev->cdev = cdev;

	/* allocate interface ID */
	id = usb_interface_id(c, f);
	if (id < 0)
		return id;
	dev->ifc_id = id;
	rmnet_sdio_interface_desc.bInterfaceNumber = id;

	ep = usb_ep_autoconfig(cdev->gadget, &rmnet_sdio_fs_in_desc);
	if (!ep)
		goto out;
	ep->driver_data = cdev; /* claim endpoint */
	dev->epin = ep;

	ep = usb_ep_autoconfig(cdev->gadget, &rmnet_sdio_fs_out_desc);
	if (!ep)
		goto out;
	ep->driver_data = cdev; /* claim endpoint */
	dev->epout = ep;

	ep = usb_ep_autoconfig(cdev->gadget, &rmnet_sdio_fs_notify_desc);
	if (!ep)
		goto out;
	ep->driver_data = cdev; /* claim endpoint */
	dev->epnotify = ep;

	/* support all relevant hardware speeds... we expect that when
	 * hardware is dual speed, all bulk-capable endpoints work at
	 * both speeds
	 */
	if (gadget_is_dualspeed(c->cdev->gadget)) {
		rmnet_sdio_hs_in_desc.bEndpointAddress =
			rmnet_sdio_fs_in_desc.bEndpointAddress;
		rmnet_sdio_hs_out_desc.bEndpointAddress =
			rmnet_sdio_fs_out_desc.bEndpointAddress;
		rmnet_sdio_hs_notify_desc.bEndpointAddress =
			rmnet_sdio_fs_notify_desc.bEndpointAddress;
	}

	queue_delayed_work(dev->wq, &dev->sdio_open_work, 0);

	return 0;

out:
	if (dev->epnotify)
		dev->epnotify->driver_data = NULL;
	if (dev->epout)
		dev->epout->driver_data = NULL;
	if (dev->epin)
		dev->epin->driver_data = NULL;

	return -ENODEV;
}

static void
rmnet_sdio_unbind(struct usb_configuration *c, struct usb_function *f)
{
	struct rmnet_sdio_dev *dev = container_of(f, struct rmnet_sdio_dev,
								function);

	cancel_delayed_work_sync(&dev->sdio_open_work);
	destroy_workqueue(dev->wq);

	dev->epout = dev->epin = dev->epnotify = NULL; /* release endpoints */

	if (test_bit(RMNET_SDIO_CH_OPEN, &dev->data_ch_status)) {
		msm_sdio_dmux_close(rmnet_sdio_data_ch);
		clear_bit(RMNET_SDIO_CH_OPEN, &dev->data_ch_status);
	}

	if (test_bit(RMNET_SDIO_CH_OPEN, &dev->ctrl_ch_status)) {
		sdio_cmux_close(rmnet_sdio_ctl_ch);
		clear_bit(RMNET_SDIO_CH_OPEN, &dev->ctrl_ch_status);
	}

	debugfs_remove_recursive(dev->dent);

	kfree(dev);
}

#if defined(CONFIG_DEBUG_FS)
static ssize_t rmnet_sdio_read_stats(struct file *file, char __user *ubuf,
		size_t count, loff_t *ppos)
{
	struct rmnet_sdio_dev *dev = file->private_data;
	char *buf;
	unsigned long flags;
	int ret;

	buf = kzalloc(sizeof(char) * 1024, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	spin_lock_irqsave(&dev->lock, flags);
	ret = scnprintf(buf, PAGE_SIZE,
			"-*-DATA-*-\n"
			"dpkts_tohost:%lu epInPool:%u tx_size:%u drp_cnt:%lu\n"
			"dpkts_tomodem:%lu epOutPool:%u rx_size:%u pending:%u\n"
			"-*-QMI-*-\n"
			"cpkts_tomodem:%lu  qmi_req_q:%u cbits:%d\n"
			"cpkts_tolaptop:%lu qmi_resp_q:%u notify_cnt:%d\n"
			"-*-MISC-*-\n"
			"data_ch_status: %lu ctrl_ch_status: %lu\n",
			/* data */
			dev->dpkt_tolaptop, dev->tx_idle_len,
			dev->tx_skb_queue.qlen, dev->tx_drp_cnt,
			dev->dpkt_tomodem, dev->rx_idle_len,
			dev->rx_skb_queue.qlen, dev->dpkts_pending_atdmux,
			/* qmi */
			dev->cpkt_tomodem, dev->qreq_q_len,
			dev->cbits_to_modem,
			dev->cpkt_tolaptop, dev->qresp_q_len,
			atomic_read(&dev->notify_count),
			/* misc */
			dev->data_ch_status, dev->ctrl_ch_status);

	spin_unlock_irqrestore(&dev->lock, flags);

	ret = simple_read_from_buffer(ubuf, count, ppos, buf, ret);

	kfree(buf);

	return ret;
}

static ssize_t rmnet_sdio_reset_stats(struct file *file, const char __user *buf,
				 size_t count, loff_t *ppos)
{
	struct rmnet_sdio_dev *dev = file->private_data;

	dev->dpkt_tolaptop = 0;
	dev->dpkt_tomodem = 0;
	dev->cpkt_tolaptop = 0;
	dev->cpkt_tomodem = 0;
	dev->dpkts_pending_atdmux = 0;
	dev->tx_drp_cnt = 0;

	/* TBD: How do we reset skb qlen
	 * it might have side effects
	 */

	return count;
}

static int debug_rmnet_sdio_open(struct inode *inode, struct file *file)
{
	file->private_data = inode->i_private;

	return 0;
}

const struct file_operations debug_rmnet_sdio_stats_ops = {
	.open  = debug_rmnet_sdio_open,
	.read  = rmnet_sdio_read_stats,
	.write = rmnet_sdio_reset_stats,
};

static void rmnet_sdio_debugfs_init(struct rmnet_sdio_dev *dev)
{
	dev->dent = debugfs_create_dir("usb_rmnet_sdio", 0);
	if (IS_ERR(dev->dent))
		return;

	debugfs_create_file("status", 0444, dev->dent, dev,
					&debug_rmnet_sdio_stats_ops);
}
#else
static void rmnet_sdio_debugfs_init(struct rmnet_sdio_dev *dev)
{
	return;
}
#endif

int rmnet_sdio_function_add(struct usb_configuration *c)
{
	struct rmnet_sdio_dev *dev;
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

	INIT_WORK(&dev->disconnect_work, rmnet_sdio_disconnect_work);
	INIT_WORK(&dev->set_modem_ctl_bits_work, rmnet_sdio_set_modem_cbits_w);

	INIT_WORK(&dev->ctl_rx_work, rmnet_sdio_control_rx_work);
	INIT_WORK(&dev->data_rx_work, rmnet_sdio_data_rx_work);

	INIT_DELAYED_WORK(&dev->sdio_open_work, rmnet_open_sdio_work);
	INIT_WORK(&dev->sdio_close_work, rmnet_close_sdio_work);

	INIT_LIST_HEAD(&dev->qmi_req_q);
	INIT_LIST_HEAD(&dev->qmi_resp_q);

	INIT_LIST_HEAD(&dev->rx_idle);
	INIT_LIST_HEAD(&dev->tx_idle);
	skb_queue_head_init(&dev->tx_skb_queue);
	skb_queue_head_init(&dev->rx_skb_queue);

	dev->function.name = "rmnet_sdio";
	dev->function.strings = rmnet_sdio_strings;
	dev->function.fs_descriptors = rmnet_sdio_fs_function;
	dev->function.hs_descriptors = rmnet_sdio_hs_function;
	dev->function.bind = rmnet_sdio_bind;
	dev->function.unbind = rmnet_sdio_unbind;
	dev->function.setup = rmnet_sdio_setup;
	dev->function.set_alt = rmnet_sdio_set_alt;
	dev->function.disable = rmnet_sdio_disable;
	dev->function.suspend = rmnet_sdio_suspend;

	ret = usb_add_function(c, &dev->function);
	if (ret)
		goto free_wq;

	rmnet_sdio_debugfs_init(dev);

       return 0;

free_wq:
       destroy_workqueue(dev->wq);
free_dev:
       kfree(dev);

       return ret;
}
