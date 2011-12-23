/*
 * f_rmnet_smd_sdio.c -- RmNet SMD & SDIO function driver
 *
 * Copyright (C) 2003-2005,2008 David Brownell
 * Copyright (C) 2003-2004 Robert Schwebel, Benedikt Spranger
 * Copyright (C) 2003 Al Borchers (alborchers@steinerpoint.com)
 * Copyright (C) 2008 Nokia Corporation
 * Copyright (c) 2011 Code Aurora Forum. All rights reserved.
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

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/err.h>
#include <linux/list.h>
#include <linux/device.h>
#include <linux/workqueue.h>
#include <linux/netdevice.h>
#include <linux/interrupt.h>
#include <linux/ratelimit.h>

#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/uaccess.h>
#include <asm/ioctls.h>

#include <linux/usb/cdc.h>
#include <linux/usb/composite.h>
#include <linux/usb/ch9.h>
#include <linux/termios.h>
#include <linux/debugfs.h>

#include <mach/msm_smd.h>
#include <mach/sdio_cmux.h>
#include <mach/sdio_dmux.h>
#include <mach/usb_gadget_xport.h>

#ifdef CONFIG_RMNET_SMD_SDIO_CTL_CHANNEL
static uint32_t rmnet_mux_sdio_ctl_ch = CONFIG_RMNET_SMD_SDIO_CTL_CHANNEL;
#else
static uint32_t rmnet_mux_sdio_ctl_ch;
#endif
module_param(rmnet_mux_sdio_ctl_ch, uint, S_IRUGO);
MODULE_PARM_DESC(rmnet_mux_sdio_ctl_ch, "RmNetMUX control SDIO channel ID");

#ifdef CONFIG_RMNET_SMD_SDIO_DATA_CHANNEL
static uint32_t rmnet_mux_sdio_data_ch = CONFIG_RMNET_SMD_SDIO_DATA_CHANNEL;
#else
static uint32_t rmnet_mux_sdio_data_ch;
#endif
module_param(rmnet_mux_sdio_data_ch, uint, S_IRUGO);
MODULE_PARM_DESC(rmnet_mux_sdio_data_ch, "RmNetMUX data SDIO channel ID");

#ifdef CONFIG_RMNET_SDIO_SMD_DATA_CHANNEL
static char *rmnet_mux_smd_data_ch = CONFIG_RMNET_SDIO_SMD_DATA_CHANNEL;
#else
static char *rmnet_mux_smd_data_ch;
#endif
module_param(rmnet_mux_smd_data_ch, charp, S_IRUGO);
MODULE_PARM_DESC(rmnet_mux_smd_data_ch, "RmNetMUX data SMD channel");

#define RMNET_MUX_ACM_CTRL_DTR			(1 << 0)

#define RMNET_MUX_SDIO_HDR			8
#define RMNET_MUX_SDIO_NOTIFY_INTERVAL		5
#define RMNET_MUX_SDIO_MAX_NFY_SZE	sizeof(struct usb_cdc_notification)

#define RMNET_MUX_SDIO_RX_REQ_MAX		16
#define RMNET_MUX_SDIO_RX_REQ_SIZE		2048
#define RMNET_MUX_SDIO_TX_REQ_MAX		100

#define RMNET_MUX_SDIO_TX_LIMIT			1000
#define RMNET_MUX_SDIO_RX_ENABLE_LIMIT		1000
#define RMNET_MUX_SDIO_RX_DISABLE_LIMIT		500

static uint32_t mux_sdio_tx_pkt_drop_thld = RMNET_MUX_SDIO_TX_LIMIT;
module_param(mux_sdio_tx_pkt_drop_thld, uint, S_IRUGO | S_IWUSR);

static uint32_t mux_sdio_rx_fctrl_en_thld =
		RMNET_MUX_SDIO_RX_ENABLE_LIMIT;
module_param(mux_sdio_rx_fctrl_en_thld, uint, S_IRUGO | S_IWUSR);

static uint32_t mux_sdio_rx_fctrl_dis_thld = RMNET_MUX_SDIO_RX_DISABLE_LIMIT;
module_param(mux_sdio_rx_fctrl_dis_thld, uint, S_IRUGO | S_IWUSR);


#define RMNET_MUX_SMD_RX_REQ_MAX		8
#define RMNET_MUX_SMD_RX_REQ_SIZE		2048
#define RMNET_MUX_SMD_TX_REQ_MAX		8
#define RMNET_MUX_SMD_TX_REQ_SIZE		2048
#define RMNET_MUX_SMD_TXN_MAX			2048

struct rmnet_mux_ctrl_pkt {
	void *buf;
	int len;
	struct list_head list;
};

struct rmnet_mux_ctrl_dev {
	struct list_head tx_q;
	wait_queue_head_t tx_wait_q;
	unsigned long tx_len;

	struct list_head rx_q;
	unsigned long rx_len;

	unsigned long cbits_to_modem;

	unsigned	opened;
};

struct rmnet_mux_sdio_dev {
	/* Tx/Rx lists */
	struct list_head tx_idle;
	struct sk_buff_head    tx_skb_queue;
	struct list_head rx_idle;
	struct sk_buff_head    rx_skb_queue;



	struct work_struct data_rx_work;

	struct delayed_work open_work;
	atomic_t sdio_open;

	unsigned int dpkts_pending_atdmux;
};

/* Data SMD channel */
struct rmnet_mux_smd_info {
	struct smd_channel *ch;
	struct tasklet_struct tx_tlet;
	struct tasklet_struct rx_tlet;
#define RMNET_MUX_CH_OPENED 0
	unsigned long flags;
	/* pending rx packet length */
	atomic_t rx_pkt;
	/* wait for smd open event*/
	wait_queue_head_t wait;
};

struct rmnet_mux_smd_dev {
	/* Tx/Rx lists */
	struct list_head tx_idle;
	struct list_head rx_idle;
	struct list_head rx_queue;

	struct rmnet_mux_smd_info smd_data;
};

struct rmnet_mux_dev {
	struct usb_function function;
	struct usb_composite_dev *cdev;

	struct usb_ep *epout;
	struct usb_ep *epin;
	struct usb_ep *epnotify;
	struct usb_request *notify_req;

	struct rmnet_mux_smd_dev smd_dev;
	struct rmnet_mux_sdio_dev sdio_dev;
	struct rmnet_mux_ctrl_dev ctrl_dev;

	u8 ifc_id;
	enum transport_type xport;
	spinlock_t lock;
	atomic_t online;
	atomic_t notify_count;
	struct workqueue_struct *wq;
	struct work_struct disconnect_work;

	/* pkt counters */
	unsigned long dpkts_tomsm;
	unsigned long dpkts_tomdm;
	unsigned long dpkts_tolaptop;
	unsigned long tx_drp_cnt;
	unsigned long cpkts_tolaptop;
	unsigned long cpkts_tomdm;
	unsigned long cpkts_drp_cnt;
};

static struct rmnet_mux_dev *rmux_dev;

static struct usb_interface_descriptor rmnet_mux_interface_desc = {
	.bLength =              USB_DT_INTERFACE_SIZE,
	.bDescriptorType =      USB_DT_INTERFACE,
	.bNumEndpoints =        3,
	.bInterfaceClass =      USB_CLASS_VENDOR_SPEC,
	.bInterfaceSubClass =   USB_CLASS_VENDOR_SPEC,
	.bInterfaceProtocol =   USB_CLASS_VENDOR_SPEC,
};

/* Full speed support */
static struct usb_endpoint_descriptor rmnet_mux_fs_notify_desc = {
	.bLength =              USB_DT_ENDPOINT_SIZE,
	.bDescriptorType =      USB_DT_ENDPOINT,
	.bEndpointAddress =     USB_DIR_IN,
	.bmAttributes =         USB_ENDPOINT_XFER_INT,
	.wMaxPacketSize =	__constant_cpu_to_le16(
						RMNET_MUX_SDIO_MAX_NFY_SZE),
	.bInterval =            1 << RMNET_MUX_SDIO_NOTIFY_INTERVAL,
};

static struct usb_endpoint_descriptor rmnet_mux_fs_in_desc  = {
	.bLength =              USB_DT_ENDPOINT_SIZE,
	.bDescriptorType =      USB_DT_ENDPOINT,
	.bEndpointAddress =     USB_DIR_IN,
	.bmAttributes =         USB_ENDPOINT_XFER_BULK,
	.wMaxPacketSize   = __constant_cpu_to_le16(64),
};

static struct usb_endpoint_descriptor rmnet_mux_fs_out_desc = {
	.bLength =              USB_DT_ENDPOINT_SIZE,
	.bDescriptorType =      USB_DT_ENDPOINT,
	.bEndpointAddress =     USB_DIR_OUT,
	.bmAttributes =         USB_ENDPOINT_XFER_BULK,
	.wMaxPacketSize = __constant_cpu_to_le16(64),
};

static struct usb_descriptor_header *rmnet_mux_fs_function[] = {
	(struct usb_descriptor_header *) &rmnet_mux_interface_desc,
	(struct usb_descriptor_header *) &rmnet_mux_fs_notify_desc,
	(struct usb_descriptor_header *) &rmnet_mux_fs_in_desc,
	(struct usb_descriptor_header *) &rmnet_mux_fs_out_desc,
	NULL,
};

/* High speed support */
static struct usb_endpoint_descriptor rmnet_mux_hs_notify_desc  = {
	.bLength =              USB_DT_ENDPOINT_SIZE,
	.bDescriptorType =      USB_DT_ENDPOINT,
	.bEndpointAddress =     USB_DIR_IN,
	.bmAttributes =         USB_ENDPOINT_XFER_INT,
	.wMaxPacketSize =	__constant_cpu_to_le16(
						RMNET_MUX_SDIO_MAX_NFY_SZE),
	.bInterval =            RMNET_MUX_SDIO_NOTIFY_INTERVAL + 4,
};

static struct usb_endpoint_descriptor rmnet_mux_hs_in_desc = {
	.bLength =              USB_DT_ENDPOINT_SIZE,
	.bDescriptorType =      USB_DT_ENDPOINT,
	.bEndpointAddress =     USB_DIR_IN,
	.bmAttributes =         USB_ENDPOINT_XFER_BULK,
	.wMaxPacketSize =	__constant_cpu_to_le16(512),
};

static struct usb_endpoint_descriptor rmnet_mux_hs_out_desc = {
	.bLength =              USB_DT_ENDPOINT_SIZE,
	.bDescriptorType =      USB_DT_ENDPOINT,
	.bEndpointAddress =     USB_DIR_OUT,
	.bmAttributes =         USB_ENDPOINT_XFER_BULK,
	.wMaxPacketSize =	__constant_cpu_to_le16(512),
};

static struct usb_descriptor_header *rmnet_mux_hs_function[] = {
	(struct usb_descriptor_header *) &rmnet_mux_interface_desc,
	(struct usb_descriptor_header *) &rmnet_mux_hs_notify_desc,
	(struct usb_descriptor_header *) &rmnet_mux_hs_in_desc,
	(struct usb_descriptor_header *) &rmnet_mux_hs_out_desc,
	NULL,
};

/* String descriptors */

static struct usb_string rmnet_mux_string_defs[] = {
	[0].s = "RmNet",
	{  } /* end of list */
};

static struct usb_gadget_strings rmnet_mux_string_table = {
	.language =             0x0409, /* en-us */
	.strings =              rmnet_mux_string_defs,
};

static struct usb_gadget_strings *rmnet_mux_strings[] = {
	&rmnet_mux_string_table,
	NULL,
};

static struct rmnet_mux_ctrl_pkt *rmnet_mux_alloc_ctrl_pkt(unsigned len,
							   gfp_t flags)
{
	struct rmnet_mux_ctrl_pkt *cpkt;

	cpkt = kzalloc(sizeof(struct rmnet_mux_ctrl_pkt), flags);
	if (!cpkt)
		return 0;

	cpkt->buf = kzalloc(len, flags);
	if (!cpkt->buf) {
		kfree(cpkt);
		return 0;
	}

	cpkt->len = len;

	return cpkt;

}

static void rmnet_mux_free_ctrl_pkt(struct rmnet_mux_ctrl_pkt *cpkt)
{
	kfree(cpkt->buf);
	kfree(cpkt);
}

/*
 * Allocate a usb_request and its buffer.  Returns a pointer to the
 * usb_request or a pointer with an error code if there is an error.
 */
static struct usb_request *
rmnet_mux_alloc_req(struct usb_ep *ep, unsigned len, gfp_t kmalloc_flags)
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
static void rmnet_mux_free_req(struct usb_ep *ep, struct usb_request *req)
{
	kfree(req->buf);
	usb_ep_free_request(ep, req);
}

static int rmnet_mux_sdio_rx_submit(struct rmnet_mux_dev *dev,
				    struct usb_request *req, gfp_t gfp_flags)
{
	struct sk_buff *skb;
	int retval;

	skb = alloc_skb(RMNET_MUX_SDIO_RX_REQ_SIZE + RMNET_MUX_SDIO_HDR,
								gfp_flags);
	if (skb == NULL)
		return -ENOMEM;
	skb_reserve(skb, RMNET_MUX_SDIO_HDR);

	req->buf = skb->data;
	req->length = RMNET_MUX_SDIO_RX_REQ_SIZE;
	req->context = skb;

	retval = usb_ep_queue(dev->epout, req, gfp_flags);
	if (retval)
		dev_kfree_skb_any(skb);

	return retval;
}

static void rmnet_mux_sdio_start_rx(struct rmnet_mux_dev *dev)
{
	struct rmnet_mux_sdio_dev *sdio_dev = &dev->sdio_dev;
	struct usb_composite_dev *cdev = dev->cdev;
	int status;
	struct usb_request *req;
	struct list_head *pool;
	unsigned long flags;

	if (!atomic_read(&dev->online)) {
		pr_debug("%s: USB not connected\n", __func__);
		return;
	}

	spin_lock_irqsave(&dev->lock, flags);
	pool = &sdio_dev->rx_idle;
	while (!list_empty(pool)) {
		req = list_first_entry(pool, struct usb_request, list);
		list_del(&req->list);

		spin_unlock_irqrestore(&dev->lock, flags);
		status = rmnet_mux_sdio_rx_submit(dev, req, GFP_KERNEL);
		spin_lock_irqsave(&dev->lock, flags);

		if (status) {
			ERROR(cdev, "rmnet_mux data rx enqueue err %d\n",
								status);
			list_add_tail(&req->list, &sdio_dev->rx_idle);
			break;
		}
	}
	spin_unlock_irqrestore(&dev->lock, flags);
}

static void rmnet_mux_sdio_start_tx(struct rmnet_mux_dev *dev)
{
	unsigned long			flags;
	int				status;
	struct sk_buff			*skb;
	struct usb_request		*req;
	struct rmnet_mux_sdio_dev *sdio_dev = &dev->sdio_dev;
	struct usb_composite_dev	*cdev = dev->cdev;


	if (!atomic_read(&dev->online))
		return;

	spin_lock_irqsave(&dev->lock, flags);
	while (!list_empty(&sdio_dev->tx_idle)) {
		skb = __skb_dequeue(&sdio_dev->tx_skb_queue);
		if (!skb) {
			spin_unlock_irqrestore(&dev->lock, flags);
			return;
		}

		req = list_first_entry(&sdio_dev->tx_idle,
				struct usb_request, list);
		req->context = skb;
		req->buf = skb->data;
		req->length = skb->len;

		list_del(&req->list);
		spin_unlock(&dev->lock);
		status = usb_ep_queue(dev->epin, req, GFP_ATOMIC);
		spin_lock(&dev->lock);
		if (status) {
			/* USB still online, queue requests back */
			if (atomic_read(&dev->online)) {
				ERROR(cdev, "rmnet tx data enqueue err %d\n",
						status);
				list_add_tail(&req->list, &sdio_dev->tx_idle);
				__skb_queue_head(&sdio_dev->tx_skb_queue, skb);
			} else {
				req->buf = 0;
				rmnet_mux_free_req(dev->epin, req);
				dev_kfree_skb_any(skb);
			}
			break;
		}
		dev->dpkts_tolaptop++;
	}
	spin_unlock_irqrestore(&dev->lock, flags);
}

static void rmnet_mux_sdio_data_receive_cb(void *priv, struct sk_buff *skb)
{
	struct rmnet_mux_dev *dev = priv;
	struct rmnet_mux_sdio_dev *sdio_dev = &dev->sdio_dev;
	unsigned long flags;

	if (!skb)
		return;
	if (!atomic_read(&dev->online)) {
		dev_kfree_skb_any(skb);
		return;
	}
	spin_lock_irqsave(&dev->lock, flags);
	if (sdio_dev->tx_skb_queue.qlen > mux_sdio_tx_pkt_drop_thld) {
		pr_err_ratelimited("%s: tx pkt dropped: tx_drop_cnt:%lu\n",
			__func__, dev->tx_drp_cnt);
		dev->tx_drp_cnt++;
		spin_unlock_irqrestore(&dev->lock, flags);
		dev_kfree_skb_any(skb);
		return;
	}
	__skb_queue_tail(&sdio_dev->tx_skb_queue, skb);
	spin_unlock_irqrestore(&dev->lock, flags);
	rmnet_mux_sdio_start_tx(dev);
}

static void rmnet_mux_sdio_data_write_done(void *priv, struct sk_buff *skb)
{
	struct rmnet_mux_dev *dev = priv;
	struct rmnet_mux_sdio_dev *sdio_dev = &dev->sdio_dev;

	if (!skb)
		return;

	dev_kfree_skb_any(skb);
	/* this function is called from
	 * sdio mux from spin_lock_irqsave
	 */
	spin_lock(&dev->lock);
	sdio_dev->dpkts_pending_atdmux--;

	if (sdio_dev->dpkts_pending_atdmux >= mux_sdio_rx_fctrl_dis_thld) {
		spin_unlock(&dev->lock);
		return;
	}
	spin_unlock(&dev->lock);

	rmnet_mux_sdio_start_rx(dev);
}

static void rmnet_mux_sdio_data_rx_work(struct work_struct *w)
{
	struct rmnet_mux_dev *dev = container_of(w, struct rmnet_mux_dev,
			sdio_dev.data_rx_work);
	struct rmnet_mux_sdio_dev *sdio_dev = &dev->sdio_dev;
	struct usb_composite_dev *cdev = dev->cdev;

	struct sk_buff *skb;
	int ret;
	unsigned long flags;

	spin_lock_irqsave(&dev->lock, flags);
	while ((skb = __skb_dequeue(&sdio_dev->rx_skb_queue))) {
		spin_unlock_irqrestore(&dev->lock, flags);
		ret = msm_sdio_dmux_write(rmnet_mux_sdio_data_ch, skb);
		spin_lock_irqsave(&dev->lock, flags);
		if (ret < 0) {
			ERROR(cdev, "rmnet_mux SDIO data write failed\n");
			dev_kfree_skb_any(skb);
		} else {
			dev->dpkts_tomdm++;
			sdio_dev->dpkts_pending_atdmux++;
		}
	}
	spin_unlock_irqrestore(&dev->lock, flags);
}

static void
rmnet_mux_sdio_complete_epout(struct usb_ep *ep, struct usb_request *req)
{
	struct rmnet_mux_dev *dev = ep->driver_data;
	struct rmnet_mux_sdio_dev *sdio_dev = &dev->sdio_dev;
	struct usb_composite_dev *cdev = dev->cdev;
	struct sk_buff *skb = req->context;
	int status = req->status;
	int queue = 0;

	if (dev->xport == USB_GADGET_XPORT_UNDEF) {
		dev_kfree_skb_any(skb);
		req->buf = 0;
		rmnet_mux_free_req(ep, req);
		return;
	}

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
		rmnet_mux_free_req(ep, req);
		return;
	default:
		/* unexpected failure */
		ERROR(cdev, "RMNET_MUX %s response error %d, %d/%d\n",
			ep->name, status,
			req->actual, req->length);
		dev_kfree_skb_any(skb);
		break;
	}

	spin_lock(&dev->lock);
	if (queue) {
		__skb_queue_tail(&sdio_dev->rx_skb_queue, skb);
		queue_work(dev->wq, &sdio_dev->data_rx_work);
	}

	if (sdio_dev->dpkts_pending_atdmux >= mux_sdio_rx_fctrl_en_thld) {
		list_add_tail(&req->list, &sdio_dev->rx_idle);
		spin_unlock(&dev->lock);
		return;
	}
	spin_unlock(&dev->lock);

	status = rmnet_mux_sdio_rx_submit(dev, req, GFP_ATOMIC);
	if (status) {
		ERROR(cdev, "rmnet_mux data rx enqueue err %d\n", status);
		list_add_tail(&req->list, &sdio_dev->rx_idle);
	}
}

static void
rmnet_mux_sdio_complete_epin(struct usb_ep *ep, struct usb_request *req)
{
	struct rmnet_mux_dev *dev = ep->driver_data;
	struct rmnet_mux_sdio_dev *sdio_dev = &dev->sdio_dev;
	struct sk_buff  *skb = req->context;
	struct usb_composite_dev *cdev = dev->cdev;
	int status = req->status;

	if (dev->xport == USB_GADGET_XPORT_UNDEF) {
		dev_kfree_skb_any(skb);
		req->buf = 0;
		rmnet_mux_free_req(ep, req);
		return;
	}

	switch (status) {
	case 0:
		/* successful completion */
	case -ECONNRESET:
	case -ESHUTDOWN:
		/* connection gone */
		break;
	default:
		ERROR(cdev, "rmnet_mux data tx ep error %d\n", status);
		break;
	}

	spin_lock(&dev->lock);
	list_add_tail(&req->list, &sdio_dev->tx_idle);
	spin_unlock(&dev->lock);
	dev_kfree_skb_any(skb);

	rmnet_mux_sdio_start_tx(dev);
}

static int rmnet_mux_sdio_enable(struct rmnet_mux_dev *dev)
{
	struct rmnet_mux_sdio_dev *sdio_dev = &dev->sdio_dev;
	int i;
	struct usb_request *req;

	/*
	 * If the memory allocation fails, all the allocated
	 * requests will be freed upon cable disconnect.
	 */
	for (i = 0; i < RMNET_MUX_SDIO_RX_REQ_MAX; i++) {
		req = rmnet_mux_alloc_req(dev->epout, 0, GFP_KERNEL);
		if (IS_ERR(req))
			return PTR_ERR(req);
		req->complete = rmnet_mux_sdio_complete_epout;
		list_add_tail(&req->list, &sdio_dev->rx_idle);
	}
	for (i = 0; i < RMNET_MUX_SDIO_TX_REQ_MAX; i++) {
		req = rmnet_mux_alloc_req(dev->epin, 0, GFP_KERNEL);
		if (IS_ERR(req))
			return PTR_ERR(req);
		req->complete = rmnet_mux_sdio_complete_epin;
		list_add_tail(&req->list, &sdio_dev->tx_idle);
	}

	rmnet_mux_sdio_start_rx(dev);
	return 0;
}

static void rmnet_mux_smd_start_rx(struct rmnet_mux_dev *dev)
{
	struct usb_composite_dev *cdev = dev->cdev;
	struct rmnet_mux_smd_dev *smd_dev = &dev->smd_dev;
	int status;
	struct usb_request *req;
	struct list_head *pool = &smd_dev->rx_idle;
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

static void rmnet_mux_smd_data_tx_tlet(unsigned long arg)
{
	struct rmnet_mux_dev *dev = (struct rmnet_mux_dev *) arg;
	struct rmnet_mux_smd_dev *smd_dev = &dev->smd_dev;
	struct usb_composite_dev *cdev = dev->cdev;
	struct usb_request *req;
	int status;
	int sz;
	unsigned long flags;

	while (1) {
		if (!atomic_read(&dev->online))
			break;
		sz = smd_cur_packet_size(smd_dev->smd_data.ch);
		if (sz == 0)
			break;
		if (smd_read_avail(smd_dev->smd_data.ch) < sz)
			break;

		spin_lock_irqsave(&dev->lock, flags);
		if (list_empty(&smd_dev->tx_idle)) {
			spin_unlock_irqrestore(&dev->lock, flags);
			DBG(cdev, "rmnet_mux data Tx buffers full\n");
			break;
		}
		req = list_first_entry(&smd_dev->tx_idle,
				struct usb_request, list);
		list_del(&req->list);
		spin_unlock_irqrestore(&dev->lock, flags);

		req->length = smd_read(smd_dev->smd_data.ch, req->buf, sz);
		status = usb_ep_queue(dev->epin, req, GFP_ATOMIC);
		if (status) {
			ERROR(cdev, "rmnet tx data enqueue err %d\n", status);
			spin_lock_irqsave(&dev->lock, flags);
			list_add_tail(&req->list, &smd_dev->tx_idle);
			spin_unlock_irqrestore(&dev->lock, flags);
			break;
		}
		dev->dpkts_tolaptop++;
	}

}

static void rmnet_mux_smd_data_rx_tlet(unsigned long arg)
{
	struct rmnet_mux_dev *dev = (struct rmnet_mux_dev *) arg;
	struct rmnet_mux_smd_dev *smd_dev = &dev->smd_dev;
	struct usb_composite_dev *cdev = dev->cdev;
	struct usb_request *req;
	int ret;
	unsigned long flags;

	spin_lock_irqsave(&dev->lock, flags);
	while (1) {
		if (!atomic_read(&dev->online))
			break;
		if (list_empty(&smd_dev->rx_queue)) {
			atomic_set(&smd_dev->smd_data.rx_pkt, 0);
			break;
		}
		req = list_first_entry(&smd_dev->rx_queue,
			struct usb_request, list);
		if (smd_write_avail(smd_dev->smd_data.ch) < req->actual) {
			atomic_set(&smd_dev->smd_data.rx_pkt, req->actual);
			DBG(cdev, "rmnet_mux SMD data channel full\n");
			break;
		}

		list_del(&req->list);
		spin_unlock_irqrestore(&dev->lock, flags);
		ret = smd_write(smd_dev->smd_data.ch, req->buf, req->actual);
		spin_lock_irqsave(&dev->lock, flags);
		if (ret != req->actual) {
			ERROR(cdev, "rmnet_mux SMD data write failed\n");
			break;
		}
		dev->dpkts_tomsm++;
		list_add_tail(&req->list, &smd_dev->rx_idle);
	}
	spin_unlock_irqrestore(&dev->lock, flags);

	/* We have free rx data requests. */
	rmnet_mux_smd_start_rx(dev);
}

/* If SMD has enough room to accommodate a data rx packet,
 * write into SMD directly. Otherwise enqueue to rx_queue.
 * We will not write into SMD directly untill rx_queue is
 * empty to strictly follow the ordering requests.
 */
static void
rmnet_mux_smd_complete_epout(struct usb_ep *ep, struct usb_request *req)
{
	struct rmnet_mux_dev *dev = req->context;
	struct rmnet_mux_smd_dev *smd_dev = &dev->smd_dev;
	struct usb_composite_dev *cdev = dev->cdev;
	int status = req->status;
	int ret;

	if (dev->xport == USB_GADGET_XPORT_UNDEF) {
		rmnet_mux_free_req(ep, req);
		return;
	}

	switch (status) {
	case 0:
		/* normal completion */
		break;
	case -ECONNRESET:
	case -ESHUTDOWN:
		/* connection gone */
		spin_lock(&dev->lock);
		list_add_tail(&req->list, &smd_dev->rx_idle);
		spin_unlock(&dev->lock);
		return;
	default:
		/* unexpected failure */
		ERROR(cdev, "RMNET_MUX %s response error %d, %d/%d\n",
			ep->name, status,
			req->actual, req->length);
		spin_lock(&dev->lock);
		list_add_tail(&req->list, &smd_dev->rx_idle);
		spin_unlock(&dev->lock);
		return;
	}

	spin_lock(&dev->lock);
	if (!atomic_read(&smd_dev->smd_data.rx_pkt)) {
		if (smd_write_avail(smd_dev->smd_data.ch) < req->actual) {
			atomic_set(&smd_dev->smd_data.rx_pkt, req->actual);
			goto queue_req;
		}
		spin_unlock(&dev->lock);
		ret = smd_write(smd_dev->smd_data.ch, req->buf, req->actual);
		/* This should never happen */
		if (ret != req->actual)
			ERROR(cdev, "rmnet_mux data smd write failed\n");
		/* Restart Rx */
		dev->dpkts_tomsm++;
		spin_lock(&dev->lock);
		list_add_tail(&req->list, &smd_dev->rx_idle);
		spin_unlock(&dev->lock);
		rmnet_mux_smd_start_rx(dev);
		return;
	}
queue_req:
	list_add_tail(&req->list, &smd_dev->rx_queue);
	spin_unlock(&dev->lock);
}

static void rmnet_mux_smd_complete_epin(struct usb_ep *ep,
					struct usb_request *req)
{
	struct rmnet_mux_dev *dev = req->context;
	struct rmnet_mux_smd_dev *smd_dev = &dev->smd_dev;
	struct usb_composite_dev *cdev = dev->cdev;
	int status = req->status;
	int schedule = 0;

	if (dev->xport == USB_GADGET_XPORT_UNDEF) {
		rmnet_mux_free_req(ep, req);
		return;
	}

	switch (status) {
	case -ECONNRESET:
	case -ESHUTDOWN:
		/* connection gone */
		spin_lock(&dev->lock);
		list_add_tail(&req->list, &smd_dev->tx_idle);
		spin_unlock(&dev->lock);
		break;
	default:
		ERROR(cdev, "rmnet_mux data tx ep error %d\n", status);
		/* FALLTHROUGH */
	case 0:
		spin_lock(&dev->lock);
		if (list_empty(&smd_dev->tx_idle))
			schedule = 1;
		list_add_tail(&req->list, &smd_dev->tx_idle);

		if (schedule)
			tasklet_schedule(&smd_dev->smd_data.tx_tlet);
		spin_unlock(&dev->lock);
		break;
	}

}


static void rmnet_mux_smd_notify(void *priv, unsigned event)
{
	struct rmnet_mux_dev *dev = priv;
	struct rmnet_mux_smd_info *smd_info = &dev->smd_dev.smd_data;
	int len = atomic_read(&smd_info->rx_pkt);

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
		set_bit(RMNET_MUX_CH_OPENED, &smd_info->flags);
		wake_up(&smd_info->wait);
		break;
	case SMD_EVENT_CLOSE:
		/* We will never come here.
		 * reset flags after closing smd channel
		 * */
		clear_bit(RMNET_MUX_CH_OPENED, &smd_info->flags);
		break;
	}
}

static int rmnet_mux_smd_enable(struct rmnet_mux_dev *dev)
{
	struct usb_composite_dev *cdev = dev->cdev;
	struct rmnet_mux_smd_dev *smd_dev = &dev->smd_dev;
	int i, ret;
	struct usb_request *req;

	if (test_bit(RMNET_MUX_CH_OPENED, &smd_dev->smd_data.flags))
		goto smd_alloc_req;

	ret = smd_open(rmnet_mux_smd_data_ch, &smd_dev->smd_data.ch,
			dev, rmnet_mux_smd_notify);
	if (ret) {
		ERROR(cdev, "Unable to open data smd channel\n");
		return ret;
	}

	wait_event(smd_dev->smd_data.wait, test_bit(RMNET_MUX_CH_OPENED,
				&smd_dev->smd_data.flags));

	/* Allocate bulk in/out requests for data transfer.
	 * If the memory allocation fails, all the allocated
	 * requests will be freed upon cable disconnect.
	 */
smd_alloc_req:
	for (i = 0; i < RMNET_MUX_SMD_RX_REQ_MAX; i++) {
		req = rmnet_mux_alloc_req(dev->epout, RMNET_MUX_SMD_RX_REQ_SIZE,
				GFP_KERNEL);
		if (IS_ERR(req))
			return PTR_ERR(req);
		req->length = RMNET_MUX_SMD_TXN_MAX;
		req->context = dev;
		req->complete = rmnet_mux_smd_complete_epout;
		list_add_tail(&req->list, &smd_dev->rx_idle);
	}

	for (i = 0; i < RMNET_MUX_SMD_TX_REQ_MAX; i++) {
		req = rmnet_mux_alloc_req(dev->epin, RMNET_MUX_SMD_TX_REQ_SIZE,
				GFP_KERNEL);
		if (IS_ERR(req))
			return PTR_ERR(req);
		req->context = dev;
		req->complete = rmnet_mux_smd_complete_epin;
		list_add_tail(&req->list, &smd_dev->tx_idle);
	}

	rmnet_mux_smd_start_rx(dev);
	return 0;
}

static void rmnet_mux_notify_complete(struct usb_ep *ep,
					 struct usb_request *req)
{
	struct rmnet_mux_dev *dev = req->context;
	struct usb_composite_dev *cdev = dev->cdev;
	int status = req->status;

	switch (status) {
	case -ECONNRESET:
	case -ESHUTDOWN:
		/* connection gone */
		atomic_set(&dev->notify_count, 0);
		break;
	default:
		ERROR(cdev, "rmnet_mux notifyep error %d\n", status);
		/* FALLTHROUGH */
	case 0:

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

static void ctrl_response_available(struct rmnet_mux_dev *dev)
{
	struct usb_composite_dev *cdev = dev->cdev;
	struct usb_request              *req = dev->notify_req;
	struct usb_cdc_notification     *event = req->buf;
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
		ERROR(cdev, "rmnet_mux notify ep enqueue error %d\n", status);
	}
}

#define MAX_CTRL_PKT_SIZE	4096

static void rmnet_mux_response_complete(struct usb_ep *ep,
					struct usb_request *req)
{
	struct rmnet_mux_dev *dev = req->context;
	struct usb_composite_dev *cdev = dev->cdev;

	switch (req->status) {
	case -ECONNRESET:
	case -ESHUTDOWN:
	case 0:
		return;
	default:
		INFO(cdev, "rmnet_mux %s response error %d, %d/%d\n",
			ep->name, req->status,
			req->actual, req->length);
	}
}

static void rmnet_mux_command_complete(struct usb_ep *ep,
					struct usb_request *req)
{
	struct rmnet_mux_dev		*dev = req->context;
	struct usb_composite_dev	*cdev = dev->cdev;
	struct rmnet_mux_ctrl_dev		*ctrl_dev = &dev->ctrl_dev;
	struct rmnet_mux_ctrl_pkt		*cpkt;
	int				len = req->actual;

	if (req->status < 0) {
		ERROR(cdev, "rmnet_mux command error %d\n", req->status);
		return;
	}

	cpkt = rmnet_mux_alloc_ctrl_pkt(len, GFP_ATOMIC);
	if (!cpkt) {
		ERROR(cdev, "unable to allocate memory for ctrl req\n");
		return;
	}

	spin_lock(&dev->lock);
	if (!ctrl_dev->opened) {
		spin_unlock(&dev->lock);
		rmnet_mux_free_ctrl_pkt(cpkt);
		dev->cpkts_drp_cnt++;
		pr_err_ratelimited(
			"%s: ctrl pkts dropped: cpkts_drp_cnt: %lu\n",
			__func__, dev->cpkts_drp_cnt);
		return;
	}

	memcpy(cpkt->buf, req->buf, len);

	list_add_tail(&cpkt->list, &ctrl_dev->tx_q);
	ctrl_dev->tx_len++;
	spin_unlock(&dev->lock);

	/* wakeup read thread */
	wake_up(&ctrl_dev->tx_wait_q);
}

static int
rmnet_mux_setup(struct usb_function *f, const struct usb_ctrlrequest *ctrl)
{
	struct rmnet_mux_dev *dev = container_of(f, struct rmnet_mux_dev,
								 function);
	struct rmnet_mux_ctrl_dev *ctrl_dev = &dev->ctrl_dev;
	struct usb_composite_dev *cdev = f->config->cdev;
	struct usb_request      *req = cdev->req;
	int                     ret = -EOPNOTSUPP;
	u16                     w_index = le16_to_cpu(ctrl->wIndex);
	u16                     w_value = le16_to_cpu(ctrl->wValue);
	u16                     w_length = le16_to_cpu(ctrl->wLength);
	struct rmnet_mux_ctrl_pkt	*cpkt;

	if (!atomic_read(&dev->online))
		return -ENOTCONN;

	switch ((ctrl->bRequestType << 8) | ctrl->bRequest) {

	case ((USB_DIR_OUT | USB_TYPE_CLASS | USB_RECIP_INTERFACE) << 8)
			| USB_CDC_SEND_ENCAPSULATED_COMMAND:
		ret = w_length;
		req->complete = rmnet_mux_command_complete;
		req->context = dev;
		break;


	case ((USB_DIR_IN | USB_TYPE_CLASS | USB_RECIP_INTERFACE) << 8)
			| USB_CDC_GET_ENCAPSULATED_RESPONSE:
		if (w_value)
			goto invalid;
		else {
			unsigned len;

			spin_lock(&dev->lock);
			if (list_empty(&ctrl_dev->rx_q)) {
				DBG(cdev, "ctrl resp queue empty"
					" %02x.%02x v%04x i%04x l%d\n",
					ctrl->bRequestType, ctrl->bRequest,
					w_value, w_index, w_length);
				spin_unlock(&dev->lock);
				goto invalid;

			}
			cpkt = list_first_entry(&ctrl_dev->rx_q,
					struct rmnet_mux_ctrl_pkt, list);
			list_del(&cpkt->list);
			ctrl_dev->rx_len--;
			spin_unlock(&dev->lock);

			len = min_t(unsigned, w_length, cpkt->len);
			memcpy(req->buf, cpkt->buf, len);
			ret = len;
			req->complete = rmnet_mux_response_complete;
			req->context = dev;
			rmnet_mux_free_ctrl_pkt(cpkt);

			dev->cpkts_tolaptop++;
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
		if (w_value & RMNET_MUX_ACM_CTRL_DTR)
			ctrl_dev->cbits_to_modem |= TIOCM_DTR;
		else
			ctrl_dev->cbits_to_modem &= ~TIOCM_DTR;

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
		VDBG(cdev, "rmnet_mux req%02x.%02x v%04x i%04x l%d\n",
			ctrl->bRequestType, ctrl->bRequest,
			w_value, w_index, w_length);
		req->zero = (ret < w_length);
		req->length = ret;
		ret = usb_ep_queue(cdev->gadget->ep0, req, GFP_ATOMIC);
		if (ret < 0)
			ERROR(cdev, "rmnet_mux ep0 enqueue err %d\n", ret);
	}

	return ret;
}

static void rmnet_mux_free_buf(struct rmnet_mux_dev *dev)
{
	struct rmnet_mux_sdio_dev *sdio_dev = &dev->sdio_dev;
	struct rmnet_mux_ctrl_dev *ctrl_dev = &dev->ctrl_dev;
	struct rmnet_mux_smd_dev *smd_dev = &dev->smd_dev;
	struct rmnet_mux_ctrl_pkt *cpkt;
	struct usb_request *req;
	struct list_head *pool;
	struct sk_buff *skb;
	unsigned long flags;

	spin_lock_irqsave(&dev->lock, flags);
	/* free all usb requests in SDIO tx pool */
	pool = &sdio_dev->tx_idle;
	while (!list_empty(pool)) {
		req = list_first_entry(pool, struct usb_request, list);
		list_del(&req->list);
		req->buf = NULL;
		rmnet_mux_free_req(dev->epout, req);
	}

	pool = &sdio_dev->rx_idle;
	/* free all usb requests in SDIO rx pool */
	while (!list_empty(pool)) {
		req = list_first_entry(pool, struct usb_request, list);
		list_del(&req->list);
		req->buf = NULL;
		rmnet_mux_free_req(dev->epin, req);
	}

	while ((skb = __skb_dequeue(&sdio_dev->tx_skb_queue)))
		dev_kfree_skb_any(skb);

	while ((skb = __skb_dequeue(&sdio_dev->rx_skb_queue)))
		dev_kfree_skb_any(skb);

	/* free all usb requests in SMD tx pool */
	pool = &smd_dev->tx_idle;
	while (!list_empty(pool)) {
		req = list_first_entry(pool, struct usb_request, list);
		list_del(&req->list);
		rmnet_mux_free_req(dev->epout, req);
	}

	pool = &smd_dev->rx_idle;
	/* free all usb requests in SMD rx pool */
	while (!list_empty(pool)) {
		req = list_first_entry(pool, struct usb_request, list);
		list_del(&req->list);
		rmnet_mux_free_req(dev->epin, req);
	}

	/* free all usb requests in SMD rx queue */
	pool = &smd_dev->rx_queue;
	while (!list_empty(pool)) {
		req = list_first_entry(pool, struct usb_request, list);
		list_del(&req->list);
		rmnet_mux_free_req(dev->epin, req);
	}

	pool = &ctrl_dev->tx_q;
	while (!list_empty(pool)) {
		cpkt = list_first_entry(pool, struct rmnet_mux_ctrl_pkt, list);
		list_del(&cpkt->list);
		rmnet_mux_free_ctrl_pkt(cpkt);
		ctrl_dev->tx_len--;
	}

	pool = &ctrl_dev->rx_q;
	while (!list_empty(pool)) {
		cpkt = list_first_entry(pool, struct rmnet_mux_ctrl_pkt, list);
		list_del(&cpkt->list);
		rmnet_mux_free_ctrl_pkt(cpkt);
		ctrl_dev->rx_len--;
	}
	spin_unlock_irqrestore(&dev->lock, flags);
}

static void rmnet_mux_disconnect_work(struct work_struct *w)
{
	struct rmnet_mux_dev *dev = container_of(w, struct rmnet_mux_dev,
			disconnect_work);
	struct rmnet_mux_smd_dev *smd_dev = &dev->smd_dev;
	struct rmnet_mux_ctrl_dev *ctrl_dev = &dev->ctrl_dev;

	if (dev->xport == USB_GADGET_XPORT_SMD) {
		tasklet_kill(&smd_dev->smd_data.rx_tlet);
		tasklet_kill(&smd_dev->smd_data.tx_tlet);
	}

	rmnet_mux_free_buf(dev);
	dev->xport = 0;

	/* wakeup read thread */
	wake_up(&ctrl_dev->tx_wait_q);
}

static void rmnet_mux_suspend(struct usb_function *f)
{
	struct rmnet_mux_dev *dev = container_of(f, struct rmnet_mux_dev,
								function);
	struct rmnet_mux_ctrl_dev *ctrl_dev = &dev->ctrl_dev;

	if (!atomic_read(&dev->online))
		return;
	/* This is a workaround for Windows Host bug during suspend.
	 * Windows 7/xp Hosts are suppose to drop DTR, when Host suspended.
	 * Since it is not being done, Hence exclusively dropping the DTR
	 * from function driver suspend.
	 */
	ctrl_dev->cbits_to_modem &= ~TIOCM_DTR;
}

static void rmnet_mux_disable(struct usb_function *f)
{
	struct rmnet_mux_dev *dev = container_of(f, struct rmnet_mux_dev,
								function);
	struct rmnet_mux_ctrl_dev *ctrl_dev = &dev->ctrl_dev;

	if (!atomic_read(&dev->online))
		return;

	atomic_set(&dev->online, 0);

	usb_ep_fifo_flush(dev->epnotify);
	usb_ep_disable(dev->epnotify);
	rmnet_mux_free_req(dev->epnotify, dev->notify_req);

	usb_ep_fifo_flush(dev->epout);
	usb_ep_disable(dev->epout);

	usb_ep_fifo_flush(dev->epin);
	usb_ep_disable(dev->epin);

	/* cleanup work */
	ctrl_dev->cbits_to_modem = 0;
	queue_work(dev->wq, &dev->disconnect_work);
}

#define SDIO_OPEN_RETRY_DELAY	msecs_to_jiffies(2000)
#define SDIO_OPEN_MAX_RETRY	90
static void rmnet_mux_open_sdio_work(struct work_struct *w)
{
	struct rmnet_mux_dev *dev =
		container_of(w, struct rmnet_mux_dev, sdio_dev.open_work.work);
	struct rmnet_mux_sdio_dev *sdio_dev = &dev->sdio_dev;
	struct usb_composite_dev *cdev = dev->cdev;
	int ret;
	static int retry_cnt;

	/* Data channel for network packets */
	ret = msm_sdio_dmux_open(rmnet_mux_sdio_data_ch, dev,
				rmnet_mux_sdio_data_receive_cb,
				rmnet_mux_sdio_data_write_done);
	if (ret) {
		if (retry_cnt > SDIO_OPEN_MAX_RETRY) {
			ERROR(cdev, "Unable to open SDIO DATA channel\n");
			return;
		}
		retry_cnt++;
		queue_delayed_work(dev->wq, &sdio_dev->open_work,
					SDIO_OPEN_RETRY_DELAY);
		return;
	}


	atomic_set(&sdio_dev->sdio_open, 1);
	pr_info("%s: usb rmnet_mux sdio channels are open retry_cnt:%d\n",
				__func__, retry_cnt);
	retry_cnt = 0;
	return;
}

static int rmnet_mux_set_alt(struct usb_function *f,
			unsigned intf, unsigned alt)
{
	struct rmnet_mux_dev *dev = container_of(f, struct rmnet_mux_dev,
								function);
	struct rmnet_mux_sdio_dev *sdio_dev = &dev->sdio_dev;
	struct usb_composite_dev *cdev = dev->cdev;

	/* allocate notification */
	dev->notify_req = rmnet_mux_alloc_req(dev->epnotify,
				RMNET_MUX_SDIO_MAX_NFY_SZE, GFP_ATOMIC);

	if (IS_ERR(dev->notify_req))
		return PTR_ERR(dev->notify_req);

	dev->notify_req->complete = rmnet_mux_notify_complete;
	dev->notify_req->context = dev;
	dev->notify_req->length = RMNET_MUX_SDIO_MAX_NFY_SZE;
	usb_ep_enable(dev->epnotify, ep_choose(cdev->gadget,
				&rmnet_mux_hs_notify_desc,
				&rmnet_mux_fs_notify_desc));

	dev->epin->driver_data = dev;
	usb_ep_enable(dev->epin, ep_choose(cdev->gadget,
				&rmnet_mux_hs_in_desc,
				&rmnet_mux_fs_in_desc));
	dev->epout->driver_data = dev;
	usb_ep_enable(dev->epout, ep_choose(cdev->gadget,
				&rmnet_mux_hs_out_desc,
				&rmnet_mux_fs_out_desc));

	dev->dpkts_tolaptop = 0;
	dev->cpkts_tolaptop = 0;
	dev->cpkts_tomdm = 0;
	dev->dpkts_tomdm = 0;
	dev->dpkts_tomsm = 0;
	dev->tx_drp_cnt = 0;
	dev->cpkts_drp_cnt = 0;
	sdio_dev->dpkts_pending_atdmux = 0;
	atomic_set(&dev->online, 1);

	return 0;
}

static ssize_t transport_store(
		struct device *device, struct device_attribute *attr,
		const char *buf, size_t size)
{
	struct rmnet_mux_dev *dev =  rmux_dev;
	int value;
	enum transport_type given_xport;
	enum transport_type t;
	struct rmnet_mux_smd_dev *smd_dev = &dev->smd_dev;
	struct rmnet_mux_sdio_dev *sdio_dev = &dev->sdio_dev;
	struct list_head *pool;
	struct sk_buff_head *skb_pool;
	struct sk_buff *skb;
	struct usb_request *req;
	unsigned long flags;

	if (!atomic_read(&dev->online)) {
		pr_err("%s: usb cable is not connected\n", __func__);
		return -EINVAL;
	}

	sscanf(buf, "%d", &value);
	if (value)
		given_xport = USB_GADGET_XPORT_SDIO;
	else
		given_xport = USB_GADGET_XPORT_SMD;

	if (given_xport == dev->xport) {
		pr_err("%s: given_xport:%s cur_xport:%s doing nothing\n",
				__func__, xport_to_str(given_xport),
				xport_to_str(dev->xport));
		return 0;
	}

	pr_debug("usb_rmnet_mux: TransportRequested: %s\n",
			xport_to_str(given_xport));

	/* prevent any other pkts to/from usb  */
	t = dev->xport;
	dev->xport = USB_GADGET_XPORT_UNDEF;
	if (t != USB_GADGET_XPORT_UNDEF) {
		usb_ep_fifo_flush(dev->epin);
		usb_ep_fifo_flush(dev->epout);
	}

	switch (t) {
	case USB_GADGET_XPORT_SDIO:
		spin_lock_irqsave(&dev->lock, flags);
		/* tx_idle */

		sdio_dev->dpkts_pending_atdmux = 0;

		pool = &sdio_dev->tx_idle;
		while (!list_empty(pool)) {
			req = list_first_entry(pool, struct usb_request, list);
			list_del(&req->list);
			req->buf = NULL;
			rmnet_mux_free_req(dev->epout, req);
		}

		/* rx_idle */
		pool = &sdio_dev->rx_idle;
		/* free all usb requests in SDIO rx pool */
		while (!list_empty(pool)) {
			req = list_first_entry(pool, struct usb_request, list);
			list_del(&req->list);
			req->buf = NULL;
			rmnet_mux_free_req(dev->epin, req);
		}

		/* tx_skb_queue */
		skb_pool = &sdio_dev->tx_skb_queue;
		while ((skb = __skb_dequeue(skb_pool)))
			dev_kfree_skb_any(skb);
		/* rx_skb_queue */
		skb_pool = &sdio_dev->rx_skb_queue;
		while ((skb = __skb_dequeue(skb_pool)))
			dev_kfree_skb_any(skb);

		spin_unlock_irqrestore(&dev->lock, flags);
		break;
	case USB_GADGET_XPORT_SMD:
		/* close smd xport */
		tasklet_kill(&smd_dev->smd_data.rx_tlet);
		tasklet_kill(&smd_dev->smd_data.tx_tlet);

		spin_lock_irqsave(&dev->lock, flags);
		/* free all usb requests in SMD tx pool */
		pool = &smd_dev->tx_idle;
		while (!list_empty(pool)) {
			req = list_first_entry(pool, struct usb_request, list);
			list_del(&req->list);
			rmnet_mux_free_req(dev->epout, req);
		}

		pool = &smd_dev->rx_idle;
		/* free all usb requests in SMD rx pool */
		while (!list_empty(pool)) {
			req = list_first_entry(pool, struct usb_request, list);
			list_del(&req->list);
			rmnet_mux_free_req(dev->epin, req);
		}

		/* free all usb requests in SMD rx queue */
		pool = &smd_dev->rx_queue;
		while (!list_empty(pool)) {
			req = list_first_entry(pool, struct usb_request, list);
			list_del(&req->list);
			rmnet_mux_free_req(dev->epin, req);
		}

		spin_unlock_irqrestore(&dev->lock, flags);
		break;
	default:
		pr_debug("%s: undefined xport, do nothing\n", __func__);
	}

	dev->xport = given_xport;

	switch (dev->xport) {
	case USB_GADGET_XPORT_SDIO:
		rmnet_mux_sdio_enable(dev);
		break;
	case USB_GADGET_XPORT_SMD:
		rmnet_mux_smd_enable(dev);
		break;
	default:
		/* we should never come here */
		pr_err("%s: undefined transport\n", __func__);
	}

	return size;
}
static DEVICE_ATTR(transport, S_IRUGO | S_IWUSR, NULL, transport_store);

static int rmnet_mux_bind(struct usb_configuration *c, struct usb_function *f)
{
	struct usb_composite_dev *cdev = c->cdev;
	struct rmnet_mux_dev *dev = container_of(f, struct rmnet_mux_dev,
								function);
	struct rmnet_mux_sdio_dev *sdio_dev = &dev->sdio_dev;
	int id;
	struct usb_ep *ep;

	dev->cdev = cdev;

	/* allocate interface ID */
	id = usb_interface_id(c, f);
	if (id < 0)
		return id;
	dev->ifc_id = id;
	rmnet_mux_interface_desc.bInterfaceNumber = id;

	ep = usb_ep_autoconfig(cdev->gadget, &rmnet_mux_fs_in_desc);
	if (!ep)
		goto out;
	ep->driver_data = cdev; /* claim endpoint */
	dev->epin = ep;

	ep = usb_ep_autoconfig(cdev->gadget, &rmnet_mux_fs_out_desc);
	if (!ep)
		goto out;
	ep->driver_data = cdev; /* claim endpoint */
	dev->epout = ep;

	ep = usb_ep_autoconfig(cdev->gadget, &rmnet_mux_fs_notify_desc);
	if (!ep)
		goto out;
	ep->driver_data = cdev; /* claim endpoint */
	dev->epnotify = ep;

	/* support all relevant hardware speeds... we expect that when
	 * hardware is dual speed, all bulk-capable endpoints work at
	 * both speeds
	 */
	if (gadget_is_dualspeed(c->cdev->gadget)) {
		rmnet_mux_hs_in_desc.bEndpointAddress =
			rmnet_mux_fs_in_desc.bEndpointAddress;
		rmnet_mux_hs_out_desc.bEndpointAddress =
			rmnet_mux_fs_out_desc.bEndpointAddress;
		rmnet_mux_hs_notify_desc.bEndpointAddress =
			rmnet_mux_fs_notify_desc.bEndpointAddress;
	}

	queue_delayed_work(dev->wq, &sdio_dev->open_work, 0);

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

static void rmnet_mux_smd_init(struct rmnet_mux_smd_dev *smd_dev)
{
	struct rmnet_mux_dev *dev = container_of(smd_dev,
			struct rmnet_mux_dev, smd_dev);

	atomic_set(&smd_dev->smd_data.rx_pkt, 0);
	tasklet_init(&smd_dev->smd_data.rx_tlet, rmnet_mux_smd_data_rx_tlet,
					(unsigned long) dev);
	tasklet_init(&smd_dev->smd_data.tx_tlet, rmnet_mux_smd_data_tx_tlet,
					(unsigned long) dev);

	init_waitqueue_head(&smd_dev->smd_data.wait);

	INIT_LIST_HEAD(&smd_dev->rx_idle);
	INIT_LIST_HEAD(&smd_dev->rx_queue);
	INIT_LIST_HEAD(&smd_dev->tx_idle);
}

static void rmnet_mux_sdio_init(struct rmnet_mux_sdio_dev *sdio_dev)
{
	INIT_WORK(&sdio_dev->data_rx_work, rmnet_mux_sdio_data_rx_work);

	INIT_DELAYED_WORK(&sdio_dev->open_work, rmnet_mux_open_sdio_work);

	INIT_LIST_HEAD(&sdio_dev->rx_idle);
	INIT_LIST_HEAD(&sdio_dev->tx_idle);
	skb_queue_head_init(&sdio_dev->tx_skb_queue);
	skb_queue_head_init(&sdio_dev->rx_skb_queue);
}

static void
rmnet_mux_unbind(struct usb_configuration *c, struct usb_function *f)
{
	struct rmnet_mux_dev *dev = container_of(f, struct rmnet_mux_dev,
								function);
	struct rmnet_mux_smd_dev *smd_dev = &dev->smd_dev;

	smd_dev->smd_data.flags = 0;
}

#if defined(CONFIG_DEBUG_FS)
#define DEBUG_BUF_SIZE	1024
static ssize_t rmnet_mux_read_stats(struct file *file, char __user *ubuf,
		size_t count, loff_t *ppos)
{
	struct rmnet_mux_dev *dev = file->private_data;
	struct rmnet_mux_sdio_dev *sdio_dev = &dev->sdio_dev;
	struct rmnet_mux_ctrl_dev *ctrl_dev = &dev->ctrl_dev;
	char *debug_buf;
	unsigned long flags;
	int ret;

	debug_buf = kmalloc(sizeof(char) * DEBUG_BUF_SIZE, GFP_KERNEL);
	if (!debug_buf)
		return -ENOMEM;

	spin_lock_irqsave(&dev->lock, flags);
	ret = scnprintf(debug_buf, DEBUG_BUF_SIZE,
			"dpkts_tomsm:  %lu\n"
			"dpkts_tomdm: %lu\n"
			"cpkts_tomdm: %lu\n"
			"dpkts_tolaptop: %lu\n"
			"cpkts_tolaptop:  %lu\n"
			"cbits_to_modem: %lu\n"
			"tx skb size:     %u\n"
			"rx_skb_size:     %u\n"
			"dpkts_pending_at_dmux: %u\n"
			"tx drp cnt: %lu\n"
			"cpkts_drp_cnt: %lu\n"
			"cpkt_tx_qlen: %lu\n"
			"cpkt_rx_qlen_to_modem: %lu\n"
			"xport: %s\n"
			"ctr_ch_opened:	%d\n",
			dev->dpkts_tomsm, dev->dpkts_tomdm,
			dev->cpkts_tomdm, dev->dpkts_tolaptop,
			dev->cpkts_tolaptop, ctrl_dev->cbits_to_modem,
			sdio_dev->tx_skb_queue.qlen,
			sdio_dev->rx_skb_queue.qlen,
			sdio_dev->dpkts_pending_atdmux, dev->tx_drp_cnt,
			dev->cpkts_drp_cnt,
			ctrl_dev->tx_len, ctrl_dev->rx_len,
			xport_to_str(dev->xport), ctrl_dev->opened);

	spin_unlock_irqrestore(&dev->lock, flags);

	ret = simple_read_from_buffer(ubuf, count, ppos, debug_buf, ret);

	kfree(debug_buf);

	return ret;
}

static ssize_t rmnet_mux_reset_stats(struct file *file, const char __user *buf,
				 size_t count, loff_t *ppos)
{
	struct rmnet_mux_dev *dev = file->private_data;
	struct rmnet_mux_sdio_dev *sdio_dev = &dev->sdio_dev;

	dev->dpkts_tolaptop = 0;
	dev->cpkts_tolaptop = 0;
	dev->cpkts_tomdm = 0;
	dev->dpkts_tomdm = 0;
	dev->dpkts_tomsm = 0;
	sdio_dev->dpkts_pending_atdmux = 0;
	dev->tx_drp_cnt = 0;
	dev->cpkts_drp_cnt = 0;
	return count;
}

static int dbg_rmnet_mux_open(struct inode *inode, struct file *file)
{
	file->private_data = inode->i_private;

	return 0;
}

const struct file_operations rmnet_mux_svlte_debug_stats_ops = {
	.open = dbg_rmnet_mux_open,
	.read = rmnet_mux_read_stats,
	.write = rmnet_mux_reset_stats,
};

struct dentry *dent_rmnet_mux;

static void rmnet_mux_debugfs_init(struct rmnet_mux_dev *dev)
{

	dent_rmnet_mux = debugfs_create_dir("usb_rmnet_mux", 0);
	if (IS_ERR(dent_rmnet_mux))
		return;

	debugfs_create_file("status", 0444, dent_rmnet_mux, dev,
			&rmnet_mux_svlte_debug_stats_ops);
}
#else
static void rmnet_mux_debugfs_init(struct rmnet_mux_dev *dev) {}
#endif

int usb_rmnet_mux_ctrl_open(struct inode *inode, struct file *fp)
{
	struct rmnet_mux_dev *dev =  rmux_dev;
	struct rmnet_mux_ctrl_dev *ctrl_dev = &dev->ctrl_dev;
	unsigned long flags;

	spin_lock_irqsave(&dev->lock, flags);
	if (ctrl_dev->opened) {
		spin_unlock_irqrestore(&dev->lock, flags);
		pr_err("%s: device is already opened\n", __func__);
		return -EBUSY;
	}

	ctrl_dev->opened = 1;
	fp->private_data = dev;
	spin_unlock_irqrestore(&dev->lock, flags);

	return 0;
}


int usb_rmnet_mux_ctrl_release(struct inode *inode, struct file *fp)
{
	struct rmnet_mux_dev *dev = fp->private_data;
	struct rmnet_mux_ctrl_dev *ctrl_dev = &dev->ctrl_dev;
	unsigned long flags;

	spin_lock_irqsave(&dev->lock, flags);
	ctrl_dev->opened = 0;
	fp->private_data = 0;
	spin_unlock_irqrestore(&dev->lock, flags);

	return 0;
}

ssize_t usb_rmnet_mux_ctrl_read(struct file *fp,
		      char __user *buf,
		      size_t count,
		      loff_t *ppos)
{
	struct rmnet_mux_dev *dev = fp->private_data;
	struct rmnet_mux_ctrl_dev *ctrl_dev = &dev->ctrl_dev;
	struct rmnet_mux_ctrl_pkt *cpkt;
	unsigned long flags;
	int ret = 0;

ctrl_read:
	if (!atomic_read(&dev->online)) {
		pr_debug("%s: USB cable not connected\n", __func__);
		return -ENODEV;
	}

	spin_lock_irqsave(&dev->lock, flags);
	if (list_empty(&ctrl_dev->tx_q)) {
		spin_unlock_irqrestore(&dev->lock, flags);
		/* Implement sleep and wakeup here */
		ret = wait_event_interruptible(ctrl_dev->tx_wait_q,
					!list_empty(&ctrl_dev->tx_q) ||
					!atomic_read(&dev->online));
		if (ret < 0)
			return ret;

		goto ctrl_read;
	}

	cpkt = list_first_entry(&ctrl_dev->tx_q, struct rmnet_mux_ctrl_pkt,
							list);
	if (cpkt->len > count) {
		spin_unlock_irqrestore(&dev->lock, flags);
		pr_err("%s: cpkt size:%d > buf size:%d\n",
				__func__, cpkt->len, count);
		return -ENOMEM;
	}
	list_del(&cpkt->list);
	ctrl_dev->tx_len--;
	spin_unlock_irqrestore(&dev->lock, flags);

	count = cpkt->len;

	ret = copy_to_user(buf, cpkt->buf, count);
	dev->cpkts_tomdm++;

	rmnet_mux_free_ctrl_pkt(cpkt);

	if (ret)
		return ret;

	return count;
}

ssize_t usb_rmnet_mux_ctrl_write(struct file *fp,
		       const char __user *buf,
		       size_t count,
		       loff_t *ppos)
{
	struct rmnet_mux_dev *dev = fp->private_data;
	struct rmnet_mux_ctrl_dev *ctrl_dev = &dev->ctrl_dev;
	struct rmnet_mux_ctrl_pkt *cpkt;
	unsigned long flags;
	int ret = 0;

	if (!atomic_read(&dev->online)) {
		pr_debug("%s: USB cable not connected\n", __func__);
		return -ENODEV;
	}

	if (!count) {
		pr_err("%s: zero length ctrl pkt\n", __func__);
		return -ENODEV;
	}

	if (count > MAX_CTRL_PKT_SIZE) {
		pr_err("%s: max_pkt_size:%d given_pkt_size:%d\n",
				__func__, MAX_CTRL_PKT_SIZE, count);
		return -ENOMEM;
	}

	cpkt = rmnet_mux_alloc_ctrl_pkt(count, GFP_KERNEL);
	if (!cpkt) {
		pr_err("%s: cannot allocate rmnet_mux ctrl pkt\n", __func__);
		return -ENOMEM;
	}

	ret = copy_from_user(cpkt->buf, buf, count);
	if (ret) {
		pr_err("%s: copy_from_user failed err:%d\n",
				__func__, ret);
		rmnet_mux_free_ctrl_pkt(cpkt);
		return ret;
	}

	spin_lock_irqsave(&dev->lock, flags);
	ctrl_dev->rx_len++;
	list_add(&cpkt->list, &ctrl_dev->rx_q);
	spin_unlock_irqrestore(&dev->lock, flags);

	ctrl_response_available(dev);

	return count;
}


#define RMNET_MUX_CTRL_GET_DTR	_IOR(0xFE, 0, int)
static long
usb_rmnet_mux_ctrl_ioctl(struct file *fp, unsigned c, unsigned long value)
{
	struct rmnet_mux_dev *dev = fp->private_data;
	struct rmnet_mux_ctrl_dev *ctrl_dev = &dev->ctrl_dev;
	unsigned long *temp = (unsigned long *)value;
	int ret = 0;

	if (c != RMNET_MUX_CTRL_GET_DTR)
		return -ENODEV;

	ret = copy_to_user(temp,
			&ctrl_dev->cbits_to_modem,
			sizeof(*temp));
	if (ret)
		return ret;

	return 0;
}

static const struct file_operations rmnet_mux_ctrl_fops = {
	.owner		= THIS_MODULE,
	.open		= usb_rmnet_mux_ctrl_open,
	.release	= usb_rmnet_mux_ctrl_release,
	.read		= usb_rmnet_mux_ctrl_read,
	.write		= usb_rmnet_mux_ctrl_write,
	.unlocked_ioctl	= usb_rmnet_mux_ctrl_ioctl,
};

static struct miscdevice rmnet_mux_ctrl_dev = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "rmnet_mux_ctrl",
	.fops = &rmnet_mux_ctrl_fops,
};

static int rmnet_mux_ctrl_device_init(struct rmnet_mux_dev *dev)
{
	int ret;
	struct rmnet_mux_ctrl_dev *ctrl_dev = &dev->ctrl_dev;

	INIT_LIST_HEAD(&ctrl_dev->tx_q);
	INIT_LIST_HEAD(&ctrl_dev->rx_q);
	init_waitqueue_head(&ctrl_dev->tx_wait_q);

	ret = misc_register(&rmnet_mux_ctrl_dev);
	if (ret) {
		pr_err("%s: failed to register misc device\n", __func__);
		return ret;
	}

	return 0;
}

static int rmnet_smd_sdio_function_add(struct usb_configuration *c)
{
	struct rmnet_mux_dev *dev = rmux_dev;

	if (!dev)
		return -ENODEV;

	pr_debug("rmnet_smd_sdio_function_add\n");

	dev->function.name = "rmnet_smd_sdio";
	dev->function.strings = rmnet_mux_strings;
	dev->function.descriptors = rmnet_mux_fs_function;
	dev->function.hs_descriptors = rmnet_mux_hs_function;
	dev->function.bind = rmnet_mux_bind;
	dev->function.unbind = rmnet_mux_unbind;
	dev->function.setup = rmnet_mux_setup;
	dev->function.set_alt = rmnet_mux_set_alt;
	dev->function.disable = rmnet_mux_disable;
	dev->function.suspend = rmnet_mux_suspend;

	return usb_add_function(c, &dev->function);
}

static int rmnet_smd_sdio_init(void)
{
	struct rmnet_mux_dev *dev;
	int ret;

	dev = kzalloc(sizeof(*dev), GFP_KERNEL);
	if (!dev)
		return -ENOMEM;

	rmux_dev = dev;

	dev->wq = create_singlethread_workqueue("k_rmnet_mux_work");
	if (!dev->wq) {
		ret = -ENOMEM;
		goto free_dev;
	}

	spin_lock_init(&dev->lock);
	atomic_set(&dev->notify_count, 0);
	atomic_set(&dev->online, 0);
	INIT_WORK(&dev->disconnect_work, rmnet_mux_disconnect_work);
	rmnet_mux_smd_init(&dev->smd_dev);
	rmnet_mux_sdio_init(&dev->sdio_dev);

	ret = rmnet_mux_ctrl_device_init(dev);
	if (ret) {
		pr_debug("%s: rmnet_mux_ctrl_device_init failed, err:%d\n",
				__func__, ret);
		goto free_wq;
	}

	rmnet_mux_debugfs_init(dev);

	return 0;

free_wq:
	destroy_workqueue(dev->wq);
free_dev:
	kfree(dev);

	return ret;
}

static void rmnet_smd_sdio_cleanup(void)
{
	struct rmnet_mux_dev *dev = rmux_dev;
	struct rmnet_mux_smd_dev *smd_dev = &dev->smd_dev;

	debugfs_remove_recursive(dent_rmnet_mux);
	misc_deregister(&rmnet_mux_ctrl_dev);
	smd_close(smd_dev->smd_data.ch);
	destroy_workqueue(dev->wq);
	kfree(dev);
}
