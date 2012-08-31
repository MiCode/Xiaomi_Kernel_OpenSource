/* Copyright (c) 2011-2012, Code Aurora Forum. All rights reserved.
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
#include <linux/kref.h>
#include <linux/debugfs.h>
#include <linux/platform_device.h>
#include <linux/uaccess.h>
#include <linux/ratelimit.h>
#include <linux/usb/ch9.h>
#include <linux/usb/cdc.h>
#include <linux/termios.h>
#include <asm/unaligned.h>
#include <mach/usb_bridge.h>

static const char *ctrl_bridge_names[] = {
	"dun_ctrl_hsic0",
	"rmnet_ctrl_hsic0"
};

/* polling interval for Interrupt ep */
#define HS_INTERVAL		7
#define FS_LS_INTERVAL		3

#define ACM_CTRL_DTR		(1 << 0)
#define DEFAULT_READ_URB_LENGTH	4096

#define SUSPENDED		BIT(0)

struct ctrl_bridge {
	struct usb_device	*udev;
	struct usb_interface	*intf;

	unsigned int		int_pipe;
	struct urb		*inturb;
	void			*intbuf;

	struct urb		*readurb;
	void			*readbuf;

	struct usb_anchor	tx_submitted;
	struct usb_anchor	tx_deferred;
	struct usb_ctrlrequest	*in_ctlreq;

	struct bridge		*brdg;
	struct platform_device	*pdev;

	unsigned long		flags;

	/* input control lines (DSR, CTS, CD, RI) */
	unsigned int		cbits_tohost;

	/* output control lines (DTR, RTS) */
	unsigned int		cbits_tomdm;

	/* counters */
	unsigned int		snd_encap_cmd;
	unsigned int		get_encap_res;
	unsigned int		resp_avail;
	unsigned int		set_ctrl_line_sts;
	unsigned int		notify_ser_state;
};

static struct ctrl_bridge	*__dev[MAX_BRIDGE_DEVICES];

/* counter used for indexing ctrl bridge devices */
static int	ch_id;

unsigned int ctrl_bridge_get_cbits_tohost(unsigned int id)
{
	struct ctrl_bridge	*dev;

	if (id >= MAX_BRIDGE_DEVICES)
		return -EINVAL;

	dev = __dev[id];
	if (!dev)
		return -ENODEV;

	return dev->cbits_tohost;
}
EXPORT_SYMBOL(ctrl_bridge_get_cbits_tohost);

int ctrl_bridge_set_cbits(unsigned int id, unsigned int cbits)
{
	struct ctrl_bridge	*dev;
	struct bridge		*brdg;
	int			retval;

	if (id >= MAX_BRIDGE_DEVICES)
		return -EINVAL;

	dev = __dev[id];
	if (!dev)
		return -ENODEV;

	pr_debug("%s: dev[id] =%u cbits : %u\n", __func__, id, cbits);

	brdg = dev->brdg;
	if (!brdg)
		return -ENODEV;

	dev->cbits_tomdm = cbits;

	retval = ctrl_bridge_write(id, NULL, 0);

	/* if DTR is high, update latest modem info to host */
	if (brdg && (cbits & ACM_CTRL_DTR) && brdg->ops.send_cbits)
		brdg->ops.send_cbits(brdg->ctx, dev->cbits_tohost);

	return retval;
}
EXPORT_SYMBOL(ctrl_bridge_set_cbits);

static void resp_avail_cb(struct urb *urb)
{
	struct ctrl_bridge	*dev = urb->context;
	int			status = 0;
	int			resubmit_urb = 1;
	struct bridge		*brdg = dev->brdg;

	switch (urb->status) {
	case 0:
		/*success*/
		dev->get_encap_res++;
		if (brdg && brdg->ops.send_pkt)
			brdg->ops.send_pkt(brdg->ctx, urb->transfer_buffer,
				urb->actual_length);
		break;

	/*do not resubmit*/
	case -ESHUTDOWN:
	case -ENOENT:
	case -ECONNRESET:
		/* unplug */
	case -EPROTO:
		/*babble error*/
		resubmit_urb = 0;
	/*resubmit*/
	case -EOVERFLOW:
	default:
		dev_dbg(&dev->intf->dev, "%s: non zero urb status = %d\n",
			__func__, urb->status);
	}

	if (resubmit_urb) {
		/*re- submit int urb to check response available*/
		usb_anchor_urb(dev->inturb, &dev->tx_submitted);
		status = usb_submit_urb(dev->inturb, GFP_ATOMIC);
		if (status) {
			dev_err(&dev->intf->dev,
				"%s: Error re-submitting Int URB %d\n",
				__func__, status);
			usb_unanchor_urb(dev->inturb);
		}
	}
}

static void notification_available_cb(struct urb *urb)
{
	int				status;
	struct usb_cdc_notification	*ctrl;
	struct ctrl_bridge		*dev = urb->context;
	struct bridge			*brdg = dev->brdg;
	unsigned int			ctrl_bits;
	unsigned char			*data;

	switch (urb->status) {
	case 0:
		/*success*/
		break;
	case -ESHUTDOWN:
	case -ENOENT:
	case -ECONNRESET:
	case -EPROTO:
		 /* unplug */
		 return;
	case -EPIPE:
		dev_err(&dev->intf->dev,
			"%s: stall on int endpoint\n", __func__);
		/* TBD : halt to be cleared in work */
	case -EOVERFLOW:
	default:
		pr_debug_ratelimited("%s: non zero urb status = %d\n",
					__func__, urb->status);
		goto resubmit_int_urb;
	}

	ctrl = (struct usb_cdc_notification *)urb->transfer_buffer;
	data = (unsigned char *)(ctrl + 1);

	switch (ctrl->bNotificationType) {
	case USB_CDC_NOTIFY_RESPONSE_AVAILABLE:
		dev->resp_avail++;
		usb_fill_control_urb(dev->readurb, dev->udev,
					usb_rcvctrlpipe(dev->udev, 0),
					(unsigned char *)dev->in_ctlreq,
					dev->readbuf,
					DEFAULT_READ_URB_LENGTH,
					resp_avail_cb, dev);

		usb_anchor_urb(dev->readurb, &dev->tx_submitted);
		status = usb_submit_urb(dev->readurb, GFP_ATOMIC);
		if (status) {
			dev_err(&dev->intf->dev,
				"%s: Error submitting Read URB %d\n",
				__func__, status);
			usb_unanchor_urb(dev->readurb);
			goto resubmit_int_urb;
		}
		return;
	case USB_CDC_NOTIFY_NETWORK_CONNECTION:
		dev_dbg(&dev->intf->dev, "%s network\n", ctrl->wValue ?
					"connected to" : "disconnected from");
		break;
	case USB_CDC_NOTIFY_SERIAL_STATE:
		dev->notify_ser_state++;
		ctrl_bits = get_unaligned_le16(data);
		dev_dbg(&dev->intf->dev, "serial state: %d\n", ctrl_bits);
		dev->cbits_tohost = ctrl_bits;
		if (brdg && brdg->ops.send_cbits)
			brdg->ops.send_cbits(brdg->ctx, ctrl_bits);
		break;
	default:
		dev_err(&dev->intf->dev, "%s: unknown notification %d received:"
			"index %d len %d data0 %d data1 %d",
			__func__, ctrl->bNotificationType, ctrl->wIndex,
			ctrl->wLength, data[0], data[1]);
	}

resubmit_int_urb:
	usb_anchor_urb(urb, &dev->tx_submitted);
	status = usb_submit_urb(urb, GFP_ATOMIC);
	if (status) {
		dev_err(&dev->intf->dev, "%s: Error re-submitting Int URB %d\n",
		__func__, status);
		usb_unanchor_urb(urb);
	}
}

static int ctrl_bridge_start_read(struct ctrl_bridge *dev)
{
	int	retval = 0;

	if (!dev->inturb) {
		dev_err(&dev->intf->dev, "%s: inturb is NULL\n", __func__);
		return -ENODEV;
	}

	if (!dev->inturb->anchor) {
		usb_anchor_urb(dev->inturb, &dev->tx_submitted);
		retval = usb_submit_urb(dev->inturb, GFP_KERNEL);
		if (retval < 0) {
			dev_err(&dev->intf->dev,
				"%s error submitting int urb %d\n",
				__func__, retval);
			usb_unanchor_urb(dev->inturb);
		}
	}

	return retval;
}

int ctrl_bridge_open(struct bridge *brdg)
{
	struct ctrl_bridge	*dev;

	if (!brdg) {
		err("bridge is null\n");
		return -EINVAL;
	}

	if (brdg->ch_id >= MAX_BRIDGE_DEVICES)
		return -EINVAL;

	dev = __dev[brdg->ch_id];
	if (!dev) {
		err("dev is null\n");
		return -ENODEV;
	}

	dev->brdg = brdg;
	dev->snd_encap_cmd = 0;
	dev->get_encap_res = 0;
	dev->resp_avail = 0;
	dev->set_ctrl_line_sts = 0;
	dev->notify_ser_state = 0;

	if (brdg->ops.send_cbits)
		brdg->ops.send_cbits(brdg->ctx, dev->cbits_tohost);

	return 0;
}
EXPORT_SYMBOL(ctrl_bridge_open);

void ctrl_bridge_close(unsigned int id)
{
	struct ctrl_bridge	*dev;

	if (id >= MAX_BRIDGE_DEVICES)
		return;

	dev  = __dev[id];
	if (!dev || !dev->brdg)
		return;

	dev_dbg(&dev->intf->dev, "%s:\n", __func__);

	ctrl_bridge_set_cbits(dev->brdg->ch_id, 0);
	usb_unlink_anchored_urbs(&dev->tx_submitted);

	dev->brdg = NULL;
}
EXPORT_SYMBOL(ctrl_bridge_close);

static void ctrl_write_callback(struct urb *urb)
{
	struct ctrl_bridge	*dev = urb->context;

	if (urb->status) {
		pr_debug("Write status/size %d/%d\n",
			urb->status, urb->actual_length);
	}

	kfree(urb->transfer_buffer);
	kfree(urb->setup_packet);
	usb_free_urb(urb);
	usb_autopm_put_interface_async(dev->intf);
}

int ctrl_bridge_write(unsigned int id, char *data, size_t size)
{
	int			result;
	struct urb		*writeurb;
	struct usb_ctrlrequest	*out_ctlreq;
	struct ctrl_bridge	*dev;

	if (id >= MAX_BRIDGE_DEVICES) {
		result = -EINVAL;
		goto free_data;
	}

	dev = __dev[id];

	if (!dev) {
		result = -ENODEV;
		goto free_data;
	}

	dev_dbg(&dev->intf->dev, "%s:[id]:%u: write (%d bytes)\n",
		__func__, id, size);

	writeurb = usb_alloc_urb(0, GFP_ATOMIC);
	if (!writeurb) {
		dev_err(&dev->intf->dev, "%s: error allocating read urb\n",
			__func__);
		result = -ENOMEM;
		goto free_data;
	}

	out_ctlreq = kmalloc(sizeof(*out_ctlreq), GFP_ATOMIC);
	if (!out_ctlreq) {
		dev_err(&dev->intf->dev,
			"%s: error allocating setup packet buffer\n",
			__func__);
		result = -ENOMEM;
		goto free_urb;
	}

	/* CDC Send Encapsulated Request packet */
	out_ctlreq->bRequestType = (USB_DIR_OUT | USB_TYPE_CLASS |
				 USB_RECIP_INTERFACE);
	if (!data && !size) {
		out_ctlreq->bRequest = USB_CDC_REQ_SET_CONTROL_LINE_STATE;
		out_ctlreq->wValue = dev->cbits_tomdm;
		dev->set_ctrl_line_sts++;
	} else {
		out_ctlreq->bRequest = USB_CDC_SEND_ENCAPSULATED_COMMAND;
		out_ctlreq->wValue = 0;
		dev->snd_encap_cmd++;
	}
	out_ctlreq->wIndex =
		dev->intf->cur_altsetting->desc.bInterfaceNumber;
	out_ctlreq->wLength = cpu_to_le16(size);

	usb_fill_control_urb(writeurb, dev->udev,
				 usb_sndctrlpipe(dev->udev, 0),
				 (unsigned char *)out_ctlreq,
				 (void *)data, size,
				 ctrl_write_callback, dev);

	result = usb_autopm_get_interface_async(dev->intf);
	if (result < 0) {
		dev_dbg(&dev->intf->dev, "%s: unable to resume interface: %d\n",
			__func__, result);

		/*
		  * Revisit: if (result == -EPERM)
		  * bridge_suspend(dev->intf, PMSG_SUSPEND);
		  */

		goto free_ctrlreq;
	}

	if (test_bit(SUSPENDED, &dev->flags)) {
		usb_anchor_urb(writeurb, &dev->tx_deferred);
		goto deferred;
	}

	usb_anchor_urb(writeurb, &dev->tx_submitted);
	result = usb_submit_urb(writeurb, GFP_ATOMIC);
	if (result < 0) {
		dev_err(&dev->intf->dev, "%s: submit URB error %d\n",
			__func__, result);
		usb_autopm_put_interface_async(dev->intf);
		goto unanchor_urb;
	}
deferred:
	return size;

unanchor_urb:
	usb_unanchor_urb(writeurb);
free_ctrlreq:
	kfree(out_ctlreq);
free_urb:
	usb_free_urb(writeurb);
free_data:
	kfree(data);

	return result;
}
EXPORT_SYMBOL(ctrl_bridge_write);

int ctrl_bridge_suspend(unsigned int id)
{
	struct ctrl_bridge	*dev;

	if (id >= MAX_BRIDGE_DEVICES)
		return -EINVAL;

	dev = __dev[id];
	if (!dev)
		return -ENODEV;

	set_bit(SUSPENDED, &dev->flags);
	usb_kill_anchored_urbs(&dev->tx_submitted);

	return 0;
}

int ctrl_bridge_resume(unsigned int id)
{
	struct ctrl_bridge	*dev;
	struct urb		*urb;

	if (id >= MAX_BRIDGE_DEVICES)
		return -EINVAL;

	dev = __dev[id];
	if (!dev)
		return -ENODEV;

	if (!test_and_clear_bit(SUSPENDED, &dev->flags))
		return 0;

	/* submit pending write requests */
	while ((urb = usb_get_from_anchor(&dev->tx_deferred))) {
		int ret;
		usb_anchor_urb(urb, &dev->tx_submitted);
		ret = usb_submit_urb(urb, GFP_ATOMIC);
		if (ret < 0) {
			usb_unanchor_urb(urb);
			kfree(urb->setup_packet);
			kfree(urb->transfer_buffer);
			usb_free_urb(urb);
			usb_autopm_put_interface_async(dev->intf);
		}
	}

	return ctrl_bridge_start_read(dev);
}

#if defined(CONFIG_DEBUG_FS)
#define DEBUG_BUF_SIZE	1024
static ssize_t ctrl_bridge_read_stats(struct file *file, char __user *ubuf,
		size_t count, loff_t *ppos)
{
	struct ctrl_bridge	*dev;
	char			*buf;
	int			ret;
	int			i;
	int			temp = 0;

	buf = kzalloc(sizeof(char) * DEBUG_BUF_SIZE, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	for (i = 0; i < ch_id; i++) {
		dev = __dev[i];
		if (!dev)
			continue;

		temp += scnprintf(buf + temp, DEBUG_BUF_SIZE - temp,
				"\nName#%s dev %p\n"
				"snd encap cmd cnt: %u\n"
				"get encap res cnt: %u\n"
				"res available cnt: %u\n"
				"set ctrlline sts cnt: %u\n"
				"notify ser state cnt: %u\n"
				"cbits_tomdm: %d\n"
				"cbits_tohost: %d\n"
				"suspended: %d\n",
				dev->pdev->name, dev,
				dev->snd_encap_cmd,
				dev->get_encap_res,
				dev->resp_avail,
				dev->set_ctrl_line_sts,
				dev->notify_ser_state,
				dev->cbits_tomdm,
				dev->cbits_tohost,
				test_bit(SUSPENDED, &dev->flags));
	}

	ret = simple_read_from_buffer(ubuf, count, ppos, buf, temp);

	kfree(buf);

	return ret;
}

static ssize_t ctrl_bridge_reset_stats(struct file *file,
	const char __user *buf, size_t count, loff_t *ppos)
{
	struct ctrl_bridge	*dev;
	int			i;

	for (i = 0; i < ch_id; i++) {
		dev = __dev[i];
		if (!dev)
			continue;

		dev->snd_encap_cmd = 0;
		dev->get_encap_res = 0;
		dev->resp_avail = 0;
		dev->set_ctrl_line_sts = 0;
		dev->notify_ser_state = 0;
	}
	return count;
}

const struct file_operations ctrl_stats_ops = {
	.read = ctrl_bridge_read_stats,
	.write = ctrl_bridge_reset_stats,
};

struct dentry	*ctrl_dent;
struct dentry	*ctrl_dfile;
static void ctrl_bridge_debugfs_init(void)
{
	ctrl_dent = debugfs_create_dir("ctrl_hsic_bridge", 0);
	if (IS_ERR(ctrl_dent))
		return;

	ctrl_dfile =
		debugfs_create_file("status", 0644, ctrl_dent, 0,
			&ctrl_stats_ops);
	if (!ctrl_dfile || IS_ERR(ctrl_dfile))
		debugfs_remove(ctrl_dent);
}

static void ctrl_bridge_debugfs_exit(void)
{
	debugfs_remove(ctrl_dfile);
	debugfs_remove(ctrl_dent);
}

#else
static void ctrl_bridge_debugfs_init(void) { }
static void ctrl_bridge_debugfs_exit(void) { }
#endif

int
ctrl_bridge_probe(struct usb_interface *ifc, struct usb_host_endpoint *int_in,
		int id)
{
	struct ctrl_bridge		*dev;
	struct usb_device		*udev;
	struct usb_endpoint_descriptor	*ep;
	u16				wMaxPacketSize;
	int				retval = 0;
	int				interval;

	udev = interface_to_usbdev(ifc);

	dev = kzalloc(sizeof(*dev), GFP_KERNEL);
	if (!dev) {
		dev_err(&ifc->dev, "%s: unable to allocate dev\n",
			__func__);
		return -ENOMEM;
	}
	dev->pdev = platform_device_alloc(ctrl_bridge_names[id], id);
	if (!dev->pdev) {
		dev_err(&ifc->dev, "%s: unable to allocate platform device\n",
			__func__);
		retval = -ENOMEM;
		goto nomem;
	}

	dev->udev = udev;
	dev->int_pipe = usb_rcvintpipe(udev,
		int_in->desc.bEndpointAddress & USB_ENDPOINT_NUMBER_MASK);
	dev->intf = ifc;

	init_usb_anchor(&dev->tx_submitted);
	init_usb_anchor(&dev->tx_deferred);

	/*use max pkt size from ep desc*/
	ep = &dev->intf->cur_altsetting->endpoint[0].desc;

	dev->inturb = usb_alloc_urb(0, GFP_KERNEL);
	if (!dev->inturb) {
		dev_err(&ifc->dev, "%s: error allocating int urb\n", __func__);
		retval = -ENOMEM;
		goto pdev_del;
	}

	wMaxPacketSize = le16_to_cpu(ep->wMaxPacketSize);

	dev->intbuf = kmalloc(wMaxPacketSize, GFP_KERNEL);
	if (!dev->intbuf) {
		dev_err(&ifc->dev, "%s: error allocating int buffer\n",
			__func__);
		retval = -ENOMEM;
		goto free_inturb;
	}

	interval =
		(udev->speed == USB_SPEED_HIGH) ? HS_INTERVAL : FS_LS_INTERVAL;

	usb_fill_int_urb(dev->inturb, udev, dev->int_pipe,
				dev->intbuf, wMaxPacketSize,
				notification_available_cb, dev, interval);

	dev->readurb = usb_alloc_urb(0, GFP_KERNEL);
	if (!dev->readurb) {
		dev_err(&ifc->dev, "%s: error allocating read urb\n",
			__func__);
		retval = -ENOMEM;
		goto free_intbuf;
	}

	dev->readbuf = kmalloc(DEFAULT_READ_URB_LENGTH, GFP_KERNEL);
	if (!dev->readbuf) {
		dev_err(&ifc->dev, "%s: error allocating read buffer\n",
			__func__);
		retval = -ENOMEM;
		goto free_rurb;
	}

	dev->in_ctlreq = kmalloc(sizeof(*dev->in_ctlreq), GFP_KERNEL);
	if (!dev->in_ctlreq) {
		dev_err(&ifc->dev, "%s:error allocating setup packet buffer\n",
			__func__);
		retval = -ENOMEM;
		goto free_rbuf;
	}

	dev->in_ctlreq->bRequestType =
			(USB_DIR_IN | USB_TYPE_CLASS | USB_RECIP_INTERFACE);
	dev->in_ctlreq->bRequest  = USB_CDC_GET_ENCAPSULATED_RESPONSE;
	dev->in_ctlreq->wValue = 0;
	dev->in_ctlreq->wIndex =
		dev->intf->cur_altsetting->desc.bInterfaceNumber;
	dev->in_ctlreq->wLength = cpu_to_le16(DEFAULT_READ_URB_LENGTH);

	__dev[id] = dev;

	platform_device_add(dev->pdev);

	ch_id++;

	return ctrl_bridge_start_read(dev);

free_rbuf:
	kfree(dev->readbuf);
free_rurb:
	usb_free_urb(dev->readurb);
free_intbuf:
	kfree(dev->intbuf);
free_inturb:
	usb_free_urb(dev->inturb);
pdev_del:
	platform_device_unregister(dev->pdev);
nomem:
	kfree(dev);

	return retval;
}

void ctrl_bridge_disconnect(unsigned int id)
{
	struct ctrl_bridge	*dev = __dev[id];

	dev_dbg(&dev->intf->dev, "%s:\n", __func__);

	platform_device_unregister(dev->pdev);

	kfree(dev->in_ctlreq);
	kfree(dev->readbuf);
	kfree(dev->intbuf);

	usb_free_urb(dev->readurb);
	usb_free_urb(dev->inturb);

	__dev[id] = NULL;
	ch_id--;

	kfree(dev);
}

static int __init ctrl_bridge_init(void)
{
	ctrl_bridge_debugfs_init();

	return 0;
}
module_init(ctrl_bridge_init);

static void __exit ctrl_bridge_exit(void)
{
	ctrl_bridge_debugfs_exit();
}
module_exit(ctrl_bridge_exit);

MODULE_DESCRIPTION("Qualcomm modem control bridge driver");
MODULE_LICENSE("GPL v2");
