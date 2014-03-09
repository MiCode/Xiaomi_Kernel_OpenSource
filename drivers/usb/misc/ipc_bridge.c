/* Copyright (c) 2011-2014, The Linux Foundation. All rights reserved.
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
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/platform_device.h>
#include <linux/uaccess.h>
#include <linux/usb.h>
#include <linux/usb/ch9.h>
#include <linux/usb/cdc.h>
#include <linux/debugfs.h>

#include <mach/ipc_bridge.h>

enum ipc_bridge_rx_state {
	RX_IDLE, /* inturb is not queued */
	RX_WAIT, /* inturb is queued and waiting for data */
	RX_BUSY, /* inturb is completed. processing RX */
};

struct ctl_pkt {
	u32 len;
	void *buf;
	struct list_head list;
};

struct ipc_bridge {
	struct usb_device *udev;
	struct usb_interface *intf;
	struct urb *inturb;
	struct urb *readurb;
	struct urb *writeurb;
	struct usb_ctrlrequest *in_ctlreq;
	struct usb_ctrlrequest *out_ctlreq;
	void *readbuf;
	void *intbuf;

	spinlock_t lock;
	struct list_head rx_list;
	enum ipc_bridge_rx_state rx_state;

	struct platform_device *pdev;
	struct mutex open_mutex;
	struct mutex read_mutex;
	struct mutex write_mutex;
	bool opened;
	struct completion write_done;
	int write_result;
	wait_queue_head_t read_wait_q;

	unsigned int snd_encap_cmd;
	unsigned int get_encap_resp;
	unsigned int susp_fail_cnt;
};

#define IPC_BRIDGE_MAX_READ_SZ		(8 * 1024)
#define IPC_BRIDGE_MAX_WRITE_SZ		(8 * 1024)

static struct ipc_bridge *__ipc_bridge_dev;

static int ipc_bridge_submit_inturb(struct ipc_bridge *dev, gfp_t gfp_flags)
{
	int ret;
	unsigned long flags;

	ret = usb_submit_urb(dev->inturb, gfp_flags);
	if (ret < 0 && ret != -EPERM)
		dev_err(&dev->intf->dev, "int urb submit err %d\n", ret);

	spin_lock_irqsave(&dev->lock, flags);
	if (ret)
		dev->rx_state = RX_IDLE;
	else
		dev->rx_state = RX_WAIT;
	spin_unlock_irqrestore(&dev->lock, flags);

	return ret;
}

static void ipc_bridge_write_cb(struct urb *urb)
{
	struct ipc_bridge *dev = urb->context;

	usb_autopm_put_interface_async(dev->intf);

	if (urb->dev->state == USB_STATE_NOTATTACHED)
		dev->write_result = -ENODEV;
	else if (urb->status < 0)
		dev->write_result = urb->status;
	else
		dev->write_result = urb->actual_length;

	complete(&dev->write_done);
}

static void ipc_bridge_read_cb(struct urb *urb)
{
	struct ipc_bridge *dev = urb->context;
	bool resubmit = true;
	struct ctl_pkt *pkt;
	unsigned long flags;

	usb_autopm_put_interface_async(dev->intf);
	if (urb->dev->state == USB_STATE_NOTATTACHED) {
		wake_up(&dev->read_wait_q);
		return;
	}

	switch (urb->status) {
	case 0:
		break;

	case -ENOENT:
	case -ESHUTDOWN:
	case -ECONNRESET:
	case -EPROTO:
	case -EPIPE:
		resubmit = false;
		goto done;

	case -EOVERFLOW:
	default:
		goto done;
	}

	if (!urb->actual_length)
		goto done;

	pkt = kmalloc(sizeof(*pkt), GFP_ATOMIC);
	if (!pkt) {
		dev_err(&dev->intf->dev, "fail to allocate pkt\n");
		resubmit = false;
		goto done;
	}
	pkt->len = urb->actual_length;
	pkt->buf = kmalloc(pkt->len, GFP_ATOMIC);
	if (!pkt->buf) {
		kfree(pkt);
		dev_err(&dev->intf->dev, "fail to allocate pkt buffer\n");
		resubmit = false;
		goto done;
	}

	memcpy(pkt->buf, urb->transfer_buffer, pkt->len);

	spin_lock_irqsave(&dev->lock, flags);
	list_add_tail(&pkt->list, &dev->rx_list);
	spin_unlock_irqrestore(&dev->lock, flags);
	wake_up(&dev->read_wait_q);

done:
	if (resubmit) {
		ipc_bridge_submit_inturb(dev, GFP_ATOMIC);
	} else {
		spin_lock_irqsave(&dev->lock, flags);
		dev->rx_state = RX_IDLE;
		spin_unlock_irqrestore(&dev->lock, flags);
	}
}

static void ipc_bridge_int_cb(struct urb *urb)
{
	struct ipc_bridge *dev = urb->context;
	struct usb_cdc_notification *ctl;
	int status;
	unsigned long flags;

	if (urb->dev->state == USB_STATE_NOTATTACHED)
		return;

	spin_lock_irqsave(&dev->lock, flags);
	dev->rx_state = RX_IDLE;
	spin_unlock_irqrestore(&dev->lock, flags);

	switch (urb->status) {
	case 0:
	case -ENOENT:
		break;

	case -ESHUTDOWN:
	case -ECONNRESET:
	case -EPROTO:
	case -EPIPE:
		return;

	case -EOVERFLOW:
	default:
		ipc_bridge_submit_inturb(dev, GFP_ATOMIC);
		return;
	}

	if (!urb->actual_length)
		return;

	ctl = urb->transfer_buffer;

	switch (ctl->bNotificationType) {
	case USB_CDC_NOTIFY_RESPONSE_AVAILABLE:

		spin_lock_irqsave(&dev->lock, flags);
		dev->rx_state = RX_BUSY;
		spin_unlock_irqrestore(&dev->lock, flags);

		usb_fill_control_urb(dev->readurb, dev->udev,
				usb_rcvctrlpipe(dev->udev, 0),
				(unsigned char *)dev->in_ctlreq,
				dev->readbuf, IPC_BRIDGE_MAX_READ_SZ,
				ipc_bridge_read_cb, dev);

		status = usb_submit_urb(dev->readurb, GFP_ATOMIC);
		if (status) {
			dev_err(&dev->intf->dev, "read urb submit err %d\n",
					status);
			goto resubmit_int_urb;
		}
		dev->get_encap_resp++;
		/* Tell runtime pm core that we are busy */
		usb_autopm_get_interface_no_resume(dev->intf);
		return;
	default:
		dev_err(&dev->intf->dev, "unknown data on int ep\n");
	}

resubmit_int_urb:
	ipc_bridge_submit_inturb(dev, GFP_ATOMIC);
}

static int ipc_bridge_open(struct platform_device *pdev)
{
	struct ipc_bridge *dev = __ipc_bridge_dev;

	if (dev->pdev != pdev)
		return -EINVAL;

	mutex_lock(&dev->open_mutex);
	if (dev->opened) {
		mutex_unlock(&dev->open_mutex);
		dev_dbg(&dev->intf->dev, "bridge already opened\n");
		return -EBUSY;
	}
	dev->opened = true;
	mutex_unlock(&dev->open_mutex);
	return 0;
}

static int ipc_bridge_rx_list_empty(struct ipc_bridge *dev)
{
	int ret;
	unsigned long flags;

	spin_lock_irqsave(&dev->lock, flags);
	ret = list_empty(&dev->rx_list);
	spin_unlock_irqrestore(&dev->lock, flags);

	return ret;
}

static int
ipc_bridge_read(struct platform_device *pdev, char *buf, unsigned int count)
{
	struct ipc_bridge *dev = __ipc_bridge_dev;
	struct ctl_pkt *pkt;
	int ret;
	unsigned long flags;

	if (dev->pdev != pdev)
		return -EINVAL;
	if (!dev->opened)
		return -EPERM;
	if (count > IPC_BRIDGE_MAX_READ_SZ)
		return -ENOSPC;

	mutex_lock(&dev->read_mutex);

	wait_event(dev->read_wait_q, (!ipc_bridge_rx_list_empty(dev) ||
			(dev->udev->state == USB_STATE_NOTATTACHED)));

	if (dev->udev->state == USB_STATE_NOTATTACHED) {
		ret = -ENODEV;
		goto done;
	}

	spin_lock_irqsave(&dev->lock, flags);
	pkt = list_first_entry(&dev->rx_list, struct ctl_pkt, list);
	if (pkt->len > count) {
		spin_unlock_irqrestore(&dev->lock, flags);
		dev_err(&dev->intf->dev, "large RX packet\n");
		ret = -ENOSPC;
		goto done;
	}
	list_del(&pkt->list);
	spin_unlock_irqrestore(&dev->lock, flags);

	memcpy(buf, pkt->buf, pkt->len);
	ret = pkt->len;
	kfree(pkt->buf);
	kfree(pkt);
done:
	mutex_unlock(&dev->read_mutex);
	return ret;
}

static int
ipc_bridge_write(struct platform_device *pdev, char *buf, unsigned int count)
{
	struct ipc_bridge *dev = __ipc_bridge_dev;
	int ret;

	if (dev->pdev != pdev)
		return -EINVAL;
	if (!dev->opened)
		return -EPERM;
	if (count > IPC_BRIDGE_MAX_WRITE_SZ)
		return -EINVAL;

	mutex_lock(&dev->write_mutex);

	dev->out_ctlreq->wLength = cpu_to_le16(count);
	usb_fill_control_urb(dev->writeurb, dev->udev,
				 usb_sndctrlpipe(dev->udev, 0),
				 (unsigned char *)dev->out_ctlreq,
				 (void *)buf, count,
				 ipc_bridge_write_cb, dev);

	ret = usb_autopm_get_interface(dev->intf);
	if (ret < 0) {
		dev_err(&dev->intf->dev, "write auto pm fail %d\n", ret);
		goto done;
	}
	ret = usb_submit_urb(dev->writeurb, GFP_KERNEL);
	if (ret < 0) {
		dev_err(&dev->intf->dev, "write urb submit err %d\n", ret);
		usb_autopm_put_interface_async(dev->intf);
		goto done;
	}
	dev->snd_encap_cmd++;
	wait_for_completion(&dev->write_done);
	ret = dev->write_result;
done:
	mutex_unlock(&dev->write_mutex);
	return ret;
}

static void ipc_bridge_close(struct platform_device *pdev)
{
	struct ipc_bridge *dev = __ipc_bridge_dev;

	WARN_ON(dev->pdev != pdev);
	WARN_ON(dev->udev->state != USB_STATE_NOTATTACHED);

	mutex_lock(&dev->open_mutex);
	if (!dev->opened) {
		mutex_unlock(&dev->open_mutex);
		dev_dbg(&dev->intf->dev, "bridge not opened\n");
		return;
	}
	dev->opened = false;
	mutex_unlock(&dev->open_mutex);
}

static const struct ipc_bridge_platform_data ipc_bridge_pdata = {
	.max_read_size = IPC_BRIDGE_MAX_READ_SZ,
	.max_write_size = IPC_BRIDGE_MAX_WRITE_SZ,
	.open = ipc_bridge_open,
	.read = ipc_bridge_read,
	.write = ipc_bridge_write,
	.close = ipc_bridge_close,
};

static int ipc_bridge_suspend(struct usb_interface *intf, pm_message_t message)
{
	struct ipc_bridge *dev = usb_get_intfdata(intf);
	unsigned long flags;
	int ret = 0;

	spin_lock_irqsave(&dev->lock, flags);
	if (dev->rx_state == RX_BUSY) {
		dev->susp_fail_cnt++;
		ret = -EBUSY;
		goto done;
	}
	spin_unlock_irqrestore(&dev->lock, flags);

	usb_kill_urb(dev->inturb);

	spin_lock_irqsave(&dev->lock, flags);
	if (dev->rx_state != RX_IDLE) {
		dev->susp_fail_cnt++;
		ret = -EBUSY;
		goto done;
	}
done:
	spin_unlock_irqrestore(&dev->lock, flags);
	return ret;
}

static int ipc_bridge_resume(struct usb_interface *intf)
{
	struct ipc_bridge *dev = usb_get_intfdata(intf);

	return ipc_bridge_submit_inturb(dev, GFP_KERNEL);
}

#define DEBUG_BUF_SIZE	512
static ssize_t ipc_bridge_read_stats(struct file *file, char __user *ubuf,
			size_t count, loff_t *ppos)
{
	struct ipc_bridge *dev = __ipc_bridge_dev;
	char *buf;
	int ret;

	buf = kzalloc(DEBUG_BUF_SIZE, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	ret = scnprintf(buf, DEBUG_BUF_SIZE,
			"ch opened: %d\n"
			"encap cmd sent: %u\n"
			"encap resp recvd: %u\n"
			"suspend fail cnt: %u\n",
			dev->opened,
			dev->snd_encap_cmd,
			dev->get_encap_resp,
			dev->susp_fail_cnt
			);

	ret = simple_read_from_buffer(ubuf, count, ppos, buf, ret);
	kfree(buf);
	return ret;
}

const struct file_operations ipc_stats_ops = {
	.read = ipc_bridge_read_stats,
};

static struct dentry *dir;

static void ipc_bridge_debugfs_init(void)
{
	struct dentry *dfile;

	dir = debugfs_create_dir("ipc_bridge", 0);
	if (IS_ERR_OR_NULL(dir))
		return;

	dfile = debugfs_create_file("status", 0444, dir, 0, &ipc_stats_ops);
	if (IS_ERR_OR_NULL(dfile))
		debugfs_remove(dir);
}

static void ipc_bridge_debugfs_cleanup(void)
{
	debugfs_remove_recursive(dir);
}

static int
ipc_bridge_probe(struct usb_interface *intf, const struct usb_device_id *id)
{
	struct ipc_bridge *dev;
	struct usb_device *udev = interface_to_usbdev(intf);
	struct usb_host_interface *intf_desc;
	struct usb_endpoint_descriptor *ep;
	u16 wMaxPacketSize;
	int ret;

	intf_desc = intf->cur_altsetting;
	if (intf_desc->desc.bNumEndpoints != 1 || !usb_endpoint_is_int_in(
				&intf_desc->endpoint[0].desc)) {
		dev_err(&intf->dev, "driver expects only 1 int ep\n");
		return -ENODEV;
	}

	dev = kzalloc(sizeof(*dev), GFP_KERNEL);
	if (!dev) {
		dev_err(&intf->dev, "fail to allocate dev\n");
		return -ENOMEM;
	}
	__ipc_bridge_dev = dev;

	dev->inturb = usb_alloc_urb(0, GFP_KERNEL);
	if (!dev->inturb) {
		dev_err(&intf->dev, "fail to allocate int urb\n");
		ret = -ENOMEM;
		goto free_dev;
	}

	ep = &intf->cur_altsetting->endpoint[0].desc;
	wMaxPacketSize = le16_to_cpu(ep->wMaxPacketSize);

	dev->intbuf = kmalloc(wMaxPacketSize, GFP_KERNEL);
	if (!dev->intbuf) {
		dev_err(&intf->dev, "%s: error allocating int buffer\n",
			__func__);
		ret = -ENOMEM;
		goto free_inturb;
	}

	usb_fill_int_urb(dev->inturb, udev,
			usb_rcvintpipe(udev, ep->bEndpointAddress),
			dev->intbuf, wMaxPacketSize,
			ipc_bridge_int_cb, dev, ep->bInterval);

	dev->in_ctlreq = kmalloc(sizeof(*dev->in_ctlreq), GFP_KERNEL);
	if (!dev->in_ctlreq) {
		dev_err(&intf->dev, "error allocating IN control req\n");
		ret = -ENOMEM;
		goto free_intbuf;
	}

	dev->in_ctlreq->bRequestType =
			(USB_DIR_IN | USB_TYPE_CLASS | USB_RECIP_INTERFACE);
	dev->in_ctlreq->bRequest  = USB_CDC_GET_ENCAPSULATED_RESPONSE;
	dev->in_ctlreq->wValue = 0;
	dev->in_ctlreq->wIndex = intf->cur_altsetting->desc.bInterfaceNumber;
	dev->in_ctlreq->wLength = cpu_to_le16(IPC_BRIDGE_MAX_READ_SZ);

	dev->readurb = usb_alloc_urb(0, GFP_KERNEL);
	if (!dev->readurb) {
		dev_err(&intf->dev, "fail to allocate read urb\n");
		ret = -ENOMEM;
		goto free_in_ctlreq;
	}

	dev->readbuf = kmalloc(IPC_BRIDGE_MAX_READ_SZ, GFP_KERNEL);
	if (!dev->readbuf) {
		dev_err(&intf->dev, "fail to allocate read buffer\n");
		ret = -ENOMEM;
		goto free_readurb;
	}

	dev->out_ctlreq = kmalloc(sizeof(*dev->out_ctlreq), GFP_KERNEL);
	if (!dev->out_ctlreq) {
		dev_err(&intf->dev, "error allocating OUT control req\n");
		ret = -ENOMEM;
		goto free_readbuf;
	}

	dev->out_ctlreq->bRequestType =
			(USB_DIR_OUT | USB_TYPE_CLASS | USB_RECIP_INTERFACE);
	dev->out_ctlreq->bRequest  = USB_CDC_SEND_ENCAPSULATED_COMMAND;
	dev->out_ctlreq->wValue = 0;
	dev->out_ctlreq->wIndex = intf->cur_altsetting->desc.bInterfaceNumber;

	dev->writeurb = usb_alloc_urb(0, GFP_KERNEL);
	if (!dev->writeurb) {
		dev_err(&intf->dev, "fail to allocate write urb\n");
		ret = -ENOMEM;
		goto free_out_ctlreq;
	}

	dev->udev = usb_get_dev(interface_to_usbdev(intf));
	dev->intf = intf;
	spin_lock_init(&dev->lock);
	init_completion(&dev->write_done);
	init_waitqueue_head(&dev->read_wait_q);
	INIT_LIST_HEAD(&dev->rx_list);
	mutex_init(&dev->open_mutex);
	mutex_init(&dev->read_mutex);
	mutex_init(&dev->write_mutex);
	usb_set_intfdata(intf, dev);
	usb_enable_autosuspend(udev);

	dev->pdev = platform_device_alloc("ipc_bridge", -1);
	if (!dev->pdev) {
		dev_err(&intf->dev, "fail to allocate pdev\n");
		ret = -ENOMEM;
		goto destroy_mutex;
	}

	ret = platform_device_add_data(dev->pdev, &ipc_bridge_pdata,
				sizeof(struct ipc_bridge_platform_data));
	if (ret) {
		dev_err(&intf->dev, "fail to add pdata\n");
		goto put_pdev;
	}

	ret = platform_device_add(dev->pdev);
	if (ret) {
		dev_err(&intf->dev, "fail to add pdev\n");
		goto put_pdev;
	}

	ret = ipc_bridge_submit_inturb(dev, GFP_KERNEL);
	if (ret) {
		dev_err(&intf->dev, "fail to start reading\n");
		goto del_pdev;
	}

	ipc_bridge_debugfs_init();
	return 0;

del_pdev:
	platform_device_del(dev->pdev);
put_pdev:
	platform_device_put(dev->pdev);
destroy_mutex:
	usb_disable_autosuspend(udev);
	mutex_destroy(&dev->write_mutex);
	mutex_destroy(&dev->read_mutex);
	mutex_destroy(&dev->open_mutex);
	usb_put_dev(dev->udev);
	usb_free_urb(dev->writeurb);
free_out_ctlreq:
	kfree(dev->out_ctlreq);
free_readbuf:
	kfree(dev->readbuf);
free_readurb:
	usb_free_urb(dev->readurb);
free_in_ctlreq:
	kfree(dev->in_ctlreq);
free_intbuf:
	kfree(dev->intbuf);
free_inturb:
	usb_free_urb(dev->inturb);
free_dev:
	kfree(dev);
	__ipc_bridge_dev = NULL;

	return ret;
}

static void ipc_bridge_disconnect(struct usb_interface *intf)
{
	struct ipc_bridge *dev = usb_get_intfdata(intf);
	struct ctl_pkt *pkt;
	unsigned long flags;

	ipc_bridge_debugfs_cleanup();

	usb_kill_urb(dev->writeurb);
	usb_kill_urb(dev->inturb);
	usb_kill_urb(dev->readurb);

	/*
	 * The readurb may not be active at the time of
	 * unlink.  Wake up the reader explicitly before
	 * unregistering the platform device.
	 */
	wake_up(&dev->read_wait_q);
	platform_device_unregister(dev->pdev);
	WARN_ON(dev->opened);

	spin_lock_irqsave(&dev->lock, flags);
	while (!list_empty(&dev->rx_list)) {
		pkt = list_first_entry(&dev->rx_list, struct ctl_pkt, list);
		list_del(&pkt->list);
		kfree(pkt->buf);
		kfree(pkt);
	}
	spin_unlock_irqrestore(&dev->lock, flags);

	mutex_destroy(&dev->open_mutex);
	mutex_destroy(&dev->read_mutex);
	mutex_destroy(&dev->write_mutex);
	usb_free_urb(dev->writeurb);
	kfree(dev->out_ctlreq);
	usb_free_urb(dev->readurb);
	kfree(dev->in_ctlreq);
	kfree(dev->intbuf);
	usb_free_urb(dev->inturb);
	usb_put_dev(dev->udev);
	kfree(dev);
	__ipc_bridge_dev = NULL;
}

static const struct usb_device_id ipc_bridge_ids[] = {
	{ USB_DEVICE_INTERFACE_NUMBER(0x5c6, 0x908A, 7) },
	{ USB_DEVICE_INTERFACE_NUMBER(0x5c6, 0x908E, 9) },
	{ USB_DEVICE_INTERFACE_NUMBER(0x5c6, 0x909D, 5) },
	{ USB_DEVICE_INTERFACE_NUMBER(0x5c6, 0x909E, 7) },
	{ USB_DEVICE_INTERFACE_NUMBER(0x5c6, 0x90A0, 7) },
	{ USB_DEVICE_INTERFACE_NUMBER(0x5c6, 0x90A4, 9) },

	{} /* terminating entry */
};
MODULE_DEVICE_TABLE(usb, ipc_bridge_ids);

static struct usb_driver ipc_bridge_driver = {
	.name = "ipc_bridge",
	.probe = ipc_bridge_probe,
	.disconnect = ipc_bridge_disconnect,
	.suspend = ipc_bridge_suspend,
	.resume = ipc_bridge_resume,
	.reset_resume = ipc_bridge_resume,
	.id_table = ipc_bridge_ids,
	.supports_autosuspend = 1,
};

module_usb_driver(ipc_bridge_driver);

MODULE_DESCRIPTION("USB IPC bridge driver");
MODULE_LICENSE("GPL v2");
