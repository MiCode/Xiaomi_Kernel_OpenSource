/* Copyright (c) 2011, Code Aurora Forum. All rights reserved.
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
#include <linux/platform_device.h>
#include <linux/usb.h>
#include <linux/usb/cdc.h>
#include <linux/usb/ch9.h>
#include <asm/unaligned.h>
#include <mach/usb_dun_bridge.h>

#define DRIVER_DESC "Qualcomm USB DUN bridge driver"
#define DRIVER_VERSION "1.0"

struct dun_bridge {
	struct usb_device	*udev;
	struct usb_interface	*intf;
	struct usb_anchor	submitted;
	u8			int_in_epaddr;
	unsigned		in, out; /* bulk in/out pipes */

	struct urb		*inturb;
	struct usb_ctrlrequest	cmd;
	u8			*ctrl_buf;

	struct kref		kref;
	struct platform_device	*pdev;

	struct dun_bridge_ops	*ops;
};

static struct dun_bridge *__dev;

/* This assumes that __dev has already been initialized by probe(). */
int dun_bridge_open(struct dun_bridge_ops *ops)
{
	struct dun_bridge *dev = __dev;
	int ret = 0;

	if (!dev) {
		err("%s: dev is null", __func__);
		return -ENODEV;
	}

	if (!ops || !ops->read_complete || !ops->write_complete)
		return -EINVAL;

	dev->ops = ops;
	if (ops->ctrl_status) {
		ret = usb_submit_urb(dev->inturb, GFP_KERNEL);
		if (ret)
			pr_err("%s: submitting int urb failed: %d\n",
				__func__, ret);
	}

	return ret;
}
EXPORT_SYMBOL(dun_bridge_open);

int dun_bridge_close(void)
{
	struct dun_bridge *dev = __dev;
	if (!dev)
		return -ENODEV;

	dev_dbg(&dev->udev->dev, "%s:", __func__);
	usb_unlink_anchored_urbs(&dev->submitted);
	usb_unlink_urb(dev->inturb);
	dev->ops = NULL;

	return 0;
}
EXPORT_SYMBOL(dun_bridge_close);

static void read_cb(struct urb *urb)
{
	struct dun_bridge *dev = urb->context;
	struct dun_bridge_ops *ops;

	if (!dev || !dev->intf) {
		pr_err("%s: device is disconnected\n", __func__);
		kfree(urb->transfer_buffer);
		return;
	}

	dev_dbg(&dev->udev->dev, "%s: status:%d actual:%d\n", __func__,
			urb->status, urb->actual_length);

	usb_autopm_put_interface(dev->intf);
	ops = dev->ops;
	if (ops)
		ops->read_complete(ops->ctxt,
				urb->transfer_buffer,
				urb->transfer_buffer_length,
				/* callback must check this value for error */
				urb->status < 0 ?
					urb->status : urb->actual_length);
	else {
		/* can't call back, free buffer on caller's behalf */
		dev_err(&dev->udev->dev, "cannot complete read callback\n");
		kfree(urb->transfer_buffer);
	}
}

int dun_bridge_read(void *data, int len)
{
	struct dun_bridge *dev = __dev;
	struct urb *urb;
	int ret;

	if (!dev || !dev->ops)
		return -ENODEV;

	if (!dev->intf) {
		pr_err("%s: device is disconnected\n", __func__);
		return -ENODEV;
	}

	if (!len) {
		dev_err(&dev->udev->dev, "%s: invalid len:%d\n", __func__, len);
		return -EINVAL;
	}

	urb = usb_alloc_urb(0, GFP_KERNEL);
	if (!urb) {
		dev_err(&dev->udev->dev, "%s: Unable to alloc urb\n", __func__);
		return -ENOMEM;
	}

	usb_fill_bulk_urb(urb, dev->udev, dev->in,
			data, len, read_cb, dev);
	usb_anchor_urb(urb, &dev->submitted);

	usb_autopm_get_interface(dev->intf);
	ret = usb_submit_urb(urb, GFP_KERNEL);
	if (ret) {
		dev_err(&dev->udev->dev, "%s: submit urb err:%d\n",
			__func__, ret);
		usb_unanchor_urb(urb);
		usb_autopm_put_interface(dev->intf);
	}

	usb_free_urb(urb);
	return ret;
}
EXPORT_SYMBOL(dun_bridge_read);

static void write_cb(struct urb *urb)
{
	struct dun_bridge *dev = urb->context;
	struct dun_bridge_ops *ops;

	if (!dev || !dev->intf) {
		pr_err("%s: device is disconnected\n", __func__);
		kfree(urb->transfer_buffer);
		return;
	}

	dev_dbg(&dev->udev->dev, "%s: status:%d actual:%d\n", __func__,
			urb->status, urb->actual_length);

	usb_autopm_put_interface(dev->intf);
	ops = dev->ops;
	if (ops)
		ops->write_complete(ops->ctxt,
				urb->transfer_buffer,
				urb->transfer_buffer_length,
				/* callback must check this value for error */
				urb->status < 0 ?
					urb->status : urb->actual_length);
	else {
		/* can't call back, free buffer on caller's behalf */
		dev_err(&dev->udev->dev, "cannot complete write callback\n");
		kfree(urb->transfer_buffer);
	}
}

int dun_bridge_write(void *data, int len)
{
	struct dun_bridge *dev = __dev;
	struct urb *urb;
	int ret;

	if (!dev || !dev->ops)
		return -ENODEV;

	if (!dev->intf) {
		pr_err("%s: device is disconnected\n", __func__);
		return -ENODEV;
	}

	if (!len) {
		dev_err(&dev->udev->dev, "%s: invalid len:%d\n", __func__, len);
		return -EINVAL;
	}

	urb = usb_alloc_urb(0, GFP_KERNEL);
	if (!urb) {
		dev_err(&dev->udev->dev, "%s: Unable to alloc urb\n", __func__);
		return -ENOMEM;
	}

	usb_fill_bulk_urb(urb, dev->udev, dev->out,
			data, len, write_cb, dev);
	usb_anchor_urb(urb, &dev->submitted);

	usb_autopm_get_interface(dev->intf);
	ret = usb_submit_urb(urb, GFP_KERNEL);
	if (ret) {
		dev_err(&dev->udev->dev, "%s: submit urb err:%d\n",
			__func__, ret);
		usb_unanchor_urb(urb);
		usb_autopm_put_interface(dev->intf);
	}

	usb_free_urb(urb);
	return ret;
}
EXPORT_SYMBOL(dun_bridge_write);

static void ctrl_cb(struct urb *urb)
{
	struct dun_bridge *dev = urb->context;
	usb_autopm_put_interface(dev->intf);
}

int dun_bridge_send_ctrl_bits(unsigned ctrl_bits)
{
	struct dun_bridge *dev = __dev;
	struct urb *urb = NULL;
	int ret;

	if (!dev || !dev->intf) {
		pr_err("%s: device is disconnected\n", __func__);
		return -ENODEV;
	}

	dev_dbg(&dev->udev->dev, "%s: %#x", __func__, ctrl_bits);

	dev->cmd.bRequestType = USB_TYPE_CLASS | USB_RECIP_INTERFACE;
	dev->cmd.bRequest = USB_CDC_REQ_SET_CONTROL_LINE_STATE;
	dev->cmd.wValue = cpu_to_le16(ctrl_bits);
	dev->cmd.wIndex = cpu_to_le16(dev->int_in_epaddr);
	dev->cmd.wLength = 0;

	urb = usb_alloc_urb(0, GFP_ATOMIC);
	if (!urb) {
		dev_err(&dev->udev->dev, "%s: Unable to alloc urb\n", __func__);
		return -ENOMEM;
	}

	usb_fill_control_urb(urb, dev->udev, usb_sndctrlpipe(dev->udev, 0),
			     (unsigned char *)&dev->cmd, NULL, 0,
			     ctrl_cb, dev);

	usb_autopm_get_interface(dev->intf);
	ret = usb_submit_urb(urb, GFP_ATOMIC);
	if (ret) {
		dev_err(&dev->udev->dev, "%s: submit urb err:%d\n",
			__func__, ret);
		usb_autopm_put_interface(dev->intf);
	}

	usb_free_urb(urb);
	return ret;
}
EXPORT_SYMBOL(dun_bridge_send_ctrl_bits);

static void int_cb(struct urb *urb)
{
	struct dun_bridge *dev = urb->context;
	struct usb_cdc_notification *dr = urb->transfer_buffer;
	unsigned char *data;
	unsigned int ctrl_bits;
	int status = urb->status;

	if (!dev || !dev->intf) {
		pr_err("%s: device is disconnected\n", __func__);
		return;
	}

	switch (status) {
	case 0:
		/* success */
		break;
	case -ECONNRESET:
	case -ENOENT:
	case -ESHUTDOWN:
		/* this urb is terminated, clean up */
		dev_err(&dev->udev->dev,
			"%s - urb shutting down with status: %d\n",
			__func__, status);
		return;
	default:
		dev_err(&dev->udev->dev,
			"%s - nonzero urb status received: %d\n",
			__func__, status);
		goto resubmit_urb;
	}

	data = (unsigned char *)(dr + 1);
	switch (dr->bNotificationType) {
	case USB_CDC_NOTIFY_NETWORK_CONNECTION:
		dev_dbg(&dev->udev->dev, "%s network\n", dr->wValue ?
					"connected to" : "disconnected from");
		break;

	case USB_CDC_NOTIFY_SERIAL_STATE:
		ctrl_bits = get_unaligned_le16(data);
		dev_dbg(&dev->udev->dev, "serial state: %d\n", ctrl_bits);
		if (dev->ops && dev->ops->ctrl_status)
			dev->ops->ctrl_status(dev->ops->ctxt, ctrl_bits);
		break;

	default:
		dev_err(&dev->udev->dev, "unknown notification %d received: "
			"index %d len %d data0 %d data1 %d\n",
			dr->bNotificationType, dr->wIndex,
			dr->wLength, data[0], data[1]);
		break;
	}
resubmit_urb:
	status = usb_submit_urb(dev->inturb, GFP_ATOMIC);
	if (status)
		dev_err(&dev->udev->dev, "%s: submit urb err:%d\n",
			__func__, status);
}

static void dun_bridge_delete(struct kref *kref)
{
	struct dun_bridge *dev = container_of(kref, struct dun_bridge, kref);

	__dev = NULL;
	usb_put_dev(dev->udev);
	usb_free_urb(dev->inturb);
	kfree(dev->ctrl_buf);
	kfree(dev);
}

static int
dun_bridge_probe(struct usb_interface *intf, const struct usb_device_id *id)
{
	struct dun_bridge *dev;
	struct usb_host_interface *iface_desc;
	struct usb_endpoint_descriptor *epd;
	__u8 iface_num;
	int i;
	int ctrlsize = 0;
	int ret = -ENOMEM;

	iface_desc = intf->cur_altsetting;
	iface_num = iface_desc->desc.bInterfaceNumber;

	/* is this interface supported? */
	if (iface_num != id->driver_info)
		return -ENODEV;

	dev = kzalloc(sizeof(*dev), GFP_KERNEL);
	if (!dev) {
		pr_err("%s: unable to allocate dev\n", __func__);
		goto error;
	}

	dev->pdev = platform_device_alloc("dun_bridge", 0);
	if (!dev->pdev) {
		pr_err("%s: unable to allocate platform device\n", __func__);
		kfree(dev);
		return -ENOMEM;
	}
	__dev = dev;

	kref_init(&dev->kref);
	dev->udev = usb_get_dev(interface_to_usbdev(intf));
	dev->intf = intf;

	init_usb_anchor(&dev->submitted);
	dev->inturb = usb_alloc_urb(0, GFP_KERNEL);
	if (!dev->inturb) {
		ret = -ENOMEM;
		goto error;
	}

	for (i = 0; i < iface_desc->desc.bNumEndpoints; ++i) {
		epd = &iface_desc->endpoint[i].desc;

		if (usb_endpoint_is_int_in(epd)) {
			dev->int_in_epaddr = epd->bEndpointAddress;
			ctrlsize = le16_to_cpu(epd->wMaxPacketSize);

			dev->ctrl_buf = kzalloc(ctrlsize, GFP_KERNEL);
			if (!dev->ctrl_buf) {
				ret = -ENOMEM;
				goto error;
			}

			usb_fill_int_urb(dev->inturb, dev->udev,
					 usb_rcvintpipe(dev->udev,
							dev->int_in_epaddr),
					 dev->ctrl_buf, ctrlsize,
					 int_cb, dev, epd->bInterval);

		} else if (usb_endpoint_is_bulk_in(epd))
			dev->in = usb_rcvbulkpipe(dev->udev,
						epd->bEndpointAddress &
						USB_ENDPOINT_NUMBER_MASK);

		else if (usb_endpoint_is_bulk_out(epd))
			dev->out = usb_sndbulkpipe(dev->udev,
						epd->bEndpointAddress &
						USB_ENDPOINT_NUMBER_MASK);
	}

	if (!dev->int_in_epaddr && !dev->in && !dev->out) {
		dev_err(&dev->udev->dev, "%s: could not find all endpoints\n",
					__func__);
		ret = -ENODEV;
		goto error;
	}

	usb_set_intfdata(intf, dev);
	platform_device_add(dev->pdev);
	return 0;
error:
	if (dev)
		kref_put(&dev->kref, dun_bridge_delete);
	return ret;
}

static void dun_bridge_disconnect(struct usb_interface *intf)
{
	struct dun_bridge *dev = usb_get_intfdata(intf);

	platform_device_del(dev->pdev);
	usb_set_intfdata(intf, NULL);
	dev->intf = NULL;

	kref_put(&dev->kref, dun_bridge_delete);

	pr_debug("%s: DUN Bridge now disconnected\n", __func__);
}

static int dun_bridge_suspend(struct usb_interface *intf, pm_message_t message)
{
	struct dun_bridge *dev = usb_get_intfdata(intf);

	dev_dbg(&dev->udev->dev, "%s:", __func__);
	usb_unlink_anchored_urbs(&dev->submitted);
	usb_unlink_urb(dev->inturb);

	return 0;
}

static int dun_bridge_resume(struct usb_interface *intf)
{
	struct dun_bridge *dev = usb_get_intfdata(intf);
	int ret = 0;

	if (dev->ops && dev->ops->ctrl_status) {
		ret = usb_submit_urb(dev->inturb, GFP_KERNEL);
		if (ret)
			dev_err(&dev->udev->dev, "%s: submit int urb err: %d\n",
				__func__, ret);
	}

	return ret;
}

#define VALID_INTERFACE_NUM	2
static const struct usb_device_id id_table[] = {
	{ USB_DEVICE(0x05c6, 0x9001),	/* Generic QC Modem device */
	.driver_info = VALID_INTERFACE_NUM },
	{ }				/* Terminating entry */
};
MODULE_DEVICE_TABLE(usb, id_table);

static struct usb_driver dun_bridge_driver = {
	.name			= "dun_usb_bridge",
	.probe			= dun_bridge_probe,
	.disconnect		= dun_bridge_disconnect,
	.id_table		= id_table,
	.suspend		= dun_bridge_suspend,
	.resume			= dun_bridge_resume,
	.supports_autosuspend	= true,
};

static int __init dun_bridge_init(void)
{
	int ret;

	ret = usb_register(&dun_bridge_driver);
	if (ret)
		pr_err("%s: unable to register dun_bridge_driver\n", __func__);

	return ret;
}

static void __exit dun_bridge_exit(void)
{
	usb_deregister(&dun_bridge_driver);
}

module_init(dun_bridge_init);
module_exit(dun_bridge_exit);

MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_LICENSE("GPL V2");
