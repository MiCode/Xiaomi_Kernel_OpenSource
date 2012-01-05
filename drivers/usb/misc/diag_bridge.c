/*
 * Copyright (c) 2011-2012, Code Aurora Forum. All rights reserved.
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
#include <linux/uaccess.h>
#include <linux/usb.h>
#include <mach/diag_bridge.h>

#define DRIVER_DESC	"USB host diag bridge driver"
#define DRIVER_VERSION	"1.0"

struct diag_bridge {
	struct usb_device	*udev;
	struct usb_interface	*ifc;
	struct usb_anchor	submitted;
	__u8			in_epAddr;
	__u8			out_epAddr;
	int			err;
	struct kref		kref;
	struct diag_bridge_ops	*ops;
	struct platform_device	*pdev;
};
struct diag_bridge *__dev;

int diag_bridge_open(struct diag_bridge_ops *ops)
{
	struct diag_bridge	*dev = __dev;

	if (!dev) {
		err("dev is null");
		return -ENODEV;
	}

	dev->ops = ops;
	dev->err = 0;

	return 0;
}
EXPORT_SYMBOL(diag_bridge_open);

void diag_bridge_close(void)
{
	struct diag_bridge	*dev = __dev;

	dev_dbg(&dev->udev->dev, "%s:\n", __func__);

	usb_kill_anchored_urbs(&dev->submitted);

	dev->ops = 0;
}
EXPORT_SYMBOL(diag_bridge_close);

static void diag_bridge_read_cb(struct urb *urb)
{
	struct diag_bridge	*dev = urb->context;
	struct diag_bridge_ops	*cbs = dev->ops;

	dev_dbg(&dev->udev->dev, "%s: status:%d actual:%d\n", __func__,
			urb->status, urb->actual_length);

	if (urb->status == -EPROTO) {
		/* save error so that subsequent read/write returns ESHUTDOWN */
		dev->err = urb->status;
		return;
	}

	cbs->read_complete_cb(cbs->ctxt,
			urb->transfer_buffer,
			urb->transfer_buffer_length,
			urb->status < 0 ? urb->status : urb->actual_length);
}

int diag_bridge_read(char *data, int size)
{
	struct urb		*urb = NULL;
	unsigned int		pipe;
	struct diag_bridge	*dev = __dev;
	int			ret;

	dev_dbg(&dev->udev->dev, "%s:\n", __func__);

	if (!size) {
		dev_err(&dev->udev->dev, "invalid size:%d\n", size);
		return -EINVAL;
	}

	if (!dev->ifc) {
		dev_err(&dev->udev->dev, "device is disconnected\n");
		return -ENODEV;
	}

	/* if there was a previous unrecoverable error, just quit */
	if (dev->err)
		return -ESHUTDOWN;

	urb = usb_alloc_urb(0, GFP_ATOMIC);
	if (!urb) {
		dev_err(&dev->udev->dev, "unable to allocate urb\n");
		return -ENOMEM;
	}

	pipe = usb_rcvbulkpipe(dev->udev, dev->in_epAddr);
	usb_fill_bulk_urb(urb, dev->udev, pipe, data, size,
				diag_bridge_read_cb, dev);
	usb_anchor_urb(urb, &dev->submitted);

	ret = usb_submit_urb(urb, GFP_ATOMIC);
	if (ret) {
		dev_err(&dev->udev->dev, "submitting urb failed err:%d\n", ret);
		usb_unanchor_urb(urb);
		usb_free_urb(urb);
		return ret;
	}

	usb_free_urb(urb);

	return 0;
}
EXPORT_SYMBOL(diag_bridge_read);

static void diag_bridge_write_cb(struct urb *urb)
{
	struct diag_bridge	*dev = urb->context;
	struct diag_bridge_ops	*cbs = dev->ops;

	dev_dbg(&dev->udev->dev, "%s:\n", __func__);

	if (urb->status == -EPROTO) {
		/* save error so that subsequent read/write returns ESHUTDOWN */
		dev->err = urb->status;
		return;
	}

	cbs->write_complete_cb(cbs->ctxt,
			urb->transfer_buffer,
			urb->transfer_buffer_length,
			urb->status < 0 ? urb->status : urb->actual_length);
}

int diag_bridge_write(char *data, int size)
{
	struct urb		*urb = NULL;
	unsigned int		pipe;
	struct diag_bridge	*dev = __dev;
	int			ret;

	dev_dbg(&dev->udev->dev, "%s:\n", __func__);

	if (!size) {
		dev_err(&dev->udev->dev, "invalid size:%d\n", size);
		return -EINVAL;
	}

	if (!dev->ifc) {
		dev_err(&dev->udev->dev, "device is disconnected\n");
		return -ENODEV;
	}

	/* if there was a previous unrecoverable error, just quit */
	if (dev->err)
		return -ESHUTDOWN;

	urb = usb_alloc_urb(0, GFP_ATOMIC);
	if (!urb) {
		err("unable to allocate urb");
		return -ENOMEM;
	}

	pipe = usb_sndbulkpipe(dev->udev, dev->out_epAddr);
	usb_fill_bulk_urb(urb, dev->udev, pipe, data, size,
				diag_bridge_write_cb, dev);
	usb_anchor_urb(urb, &dev->submitted);

	ret = usb_submit_urb(urb, GFP_ATOMIC);
	if (ret) {
		err("submitting urb failed err:%d", ret);
		usb_unanchor_urb(urb);
		usb_free_urb(urb);
		return ret;
	}

	usb_free_urb(urb);

	return 0;
}
EXPORT_SYMBOL(diag_bridge_write);

static void diag_bridge_delete(struct kref *kref)
{
	struct diag_bridge *dev =
		container_of(kref, struct diag_bridge, kref);

	usb_put_dev(dev->udev);
	__dev = 0;
	kfree(dev);
}

static int
diag_bridge_probe(struct usb_interface *ifc, const struct usb_device_id *id)
{
	struct diag_bridge		*dev;
	struct usb_host_interface	*ifc_desc;
	struct usb_endpoint_descriptor	*ep_desc;
	int				i;
	int				ret = -ENOMEM;
	__u8				ifc_num;

	dbg("%s: id:%lu", __func__, id->driver_info);

	ifc_num = ifc->cur_altsetting->desc.bInterfaceNumber;

	/* is this interface supported ? */
	if (ifc_num != id->driver_info)
		return -ENODEV;

	dev = kzalloc(sizeof(*dev), GFP_KERNEL);
	if (!dev) {
		pr_err("%s: unable to allocate dev\n", __func__);
		return -ENOMEM;
	}
	dev->pdev = platform_device_alloc("diag_bridge", -1);
	if (!dev->pdev) {
		pr_err("%s: unable to allocate platform device\n", __func__);
		kfree(dev);
		return -ENOMEM;
	}
	__dev = dev;

	dev->udev = usb_get_dev(interface_to_usbdev(ifc));
	dev->ifc = ifc;
	kref_init(&dev->kref);
	init_usb_anchor(&dev->submitted);

	ifc_desc = ifc->cur_altsetting;
	for (i = 0; i < ifc_desc->desc.bNumEndpoints; i++) {
		ep_desc = &ifc_desc->endpoint[i].desc;

		if (!dev->in_epAddr && usb_endpoint_is_bulk_in(ep_desc))
			dev->in_epAddr = ep_desc->bEndpointAddress;

		if (!dev->out_epAddr && usb_endpoint_is_bulk_out(ep_desc))
			dev->out_epAddr = ep_desc->bEndpointAddress;
	}

	if (!(dev->in_epAddr && dev->out_epAddr)) {
		err("could not find bulk in and bulk out endpoints");
		ret = -ENODEV;
		goto error;
	}

	usb_set_intfdata(ifc, dev);

	platform_device_add(dev->pdev);

	dev_dbg(&dev->udev->dev, "%s: complete\n", __func__);

	return 0;

error:
	if (dev)
		kref_put(&dev->kref, diag_bridge_delete);

	return ret;
}

static void diag_bridge_disconnect(struct usb_interface *ifc)
{
	struct diag_bridge	*dev = usb_get_intfdata(ifc);

	dev_dbg(&dev->udev->dev, "%s:\n", __func__);

	platform_device_del(dev->pdev);
	kref_put(&dev->kref, diag_bridge_delete);
	usb_set_intfdata(ifc, NULL);
}


#define VALID_INTERFACE_NUM	0
static const struct usb_device_id diag_bridge_ids[] = {
	{ USB_DEVICE(0x5c6, 0x9001),
	.driver_info = VALID_INTERFACE_NUM, },
	{ USB_DEVICE(0x5c6, 0x9034),
	.driver_info = VALID_INTERFACE_NUM, },
	{ USB_DEVICE(0x5c6, 0x9048),
	.driver_info = VALID_INTERFACE_NUM, },

	{} /* terminating entry */
};
MODULE_DEVICE_TABLE(usb, diag_bridge_ids);

static struct usb_driver diag_bridge_driver = {
	.name =		"diag_bridge",
	.probe =	diag_bridge_probe,
	.disconnect =	diag_bridge_disconnect,
	.id_table =	diag_bridge_ids,
};

static int __init diag_bridge_init(void)
{
	int ret;

	ret = usb_register(&diag_bridge_driver);
	if (ret) {
		err("%s: unable to register diag driver",
				__func__);
		return ret;
	}

	return 0;
}

static void __exit diag_bridge_exit(void)
{
	usb_deregister(&diag_bridge_driver);
}

module_init(diag_bridge_init);
module_exit(diag_bridge_exit);

MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_VERSION(DRIVER_VERSION);
MODULE_LICENSE("GPL v2");
