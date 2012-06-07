/* Copyright (c) 2010-2011, Code Aurora Forum. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */


#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/module.h>
#include <linux/usb.h>
#include <linux/slab.h>
#include <linux/usb/ch11.h>
#include <linux/usb/hcd.h>

#define TEST_SE0_NAK_PID		0x0101
#define TEST_J_PID			0x0102
#define TEST_K_PID			0x0103
#define TEST_PACKET_PID			0x0104
#define TEST_HS_HOST_PORT_SUSPEND_RESUME 0x0106
#define TEST_SINGLE_STEP_GET_DEV_DESC	0x0107
#define TEST_SINGLE_STEP_SET_FEATURE	0x0108

static int ehset_probe(struct usb_interface *intf,
		       const struct usb_device_id *id)
{
	int status = -1;
	struct usb_device *dev = interface_to_usbdev(intf);
	struct usb_device *rh_udev = dev->bus->root_hub;
	struct usb_device *hub_udev = dev->parent;
	int port1 = dev->portnum;
	int test_mode = le16_to_cpu(dev->descriptor.idProduct);

	switch (test_mode) {
	case TEST_SE0_NAK_PID:
		status = usb_control_msg(hub_udev, usb_sndctrlpipe(hub_udev, 0),
			USB_REQ_SET_FEATURE, USB_RT_PORT, USB_PORT_FEAT_TEST,
			(3 << 8) | port1, NULL, 0, 1000);
		break;
	case TEST_J_PID:
		status = usb_control_msg(hub_udev, usb_sndctrlpipe(hub_udev, 0),
			USB_REQ_SET_FEATURE, USB_RT_PORT, USB_PORT_FEAT_TEST,
			(1 << 8) | port1, NULL, 0, 1000);
		break;
	case TEST_K_PID:
		status = usb_control_msg(hub_udev, usb_sndctrlpipe(hub_udev, 0),
			USB_REQ_SET_FEATURE, USB_RT_PORT, USB_PORT_FEAT_TEST,
			(2 << 8) | port1, NULL, 0, 1000);
		break;
	case TEST_PACKET_PID:
		status = usb_control_msg(hub_udev, usb_sndctrlpipe(hub_udev, 0),
			USB_REQ_SET_FEATURE, USB_RT_PORT, USB_PORT_FEAT_TEST,
			(4 << 8) | port1, NULL, 0, 1000);
		break;
	case TEST_HS_HOST_PORT_SUSPEND_RESUME:
		/* Test: wait for 15secs -> suspend -> 15secs delay -> resume */
		msleep(15 * 1000);
		status = usb_control_msg(hub_udev, usb_sndctrlpipe(hub_udev, 0),
			USB_REQ_SET_FEATURE, USB_RT_PORT,
			USB_PORT_FEAT_SUSPEND, port1, NULL, 0, 1000);
		if (status < 0)
			break;
		msleep(15 * 1000);
		status = usb_control_msg(hub_udev, usb_sndctrlpipe(hub_udev, 0),
			USB_REQ_CLEAR_FEATURE, USB_RT_PORT,
			USB_PORT_FEAT_SUSPEND, port1, NULL, 0, 1000);
		break;
	case TEST_SINGLE_STEP_GET_DEV_DESC:
		/* Test: wait for 15secs -> GetDescriptor request */
		msleep(15 * 1000);
		{
			struct usb_device_descriptor *buf;
			buf = kmalloc(USB_DT_DEVICE_SIZE, GFP_KERNEL);
			if (!buf)
				return -ENOMEM;

			status = usb_control_msg(dev, usb_rcvctrlpipe(dev, 0),
				USB_REQ_GET_DESCRIPTOR, USB_DIR_IN,
				USB_DT_DEVICE << 8, 0,
				buf, USB_DT_DEVICE_SIZE,
				USB_CTRL_GET_TIMEOUT);
			kfree(buf);
		}
		break;
	case TEST_SINGLE_STEP_SET_FEATURE:
		/* GetDescriptor's SETUP request -> 15secs delay -> IN & STATUS
		 * Issue request to ehci root hub driver with portnum = 1
		 */
		status = usb_control_msg(rh_udev, usb_sndctrlpipe(rh_udev, 0),
			USB_REQ_SET_FEATURE, USB_RT_PORT, USB_PORT_FEAT_TEST,
			(6 << 8) | 1, NULL, 0, 60 * 1000);

		break;
	default:
		pr_err("%s: undefined test mode ( %X )\n", __func__, test_mode);
		return -EINVAL;
	}

	return (status < 0) ? status : 0;
}

static void ehset_disconnect(struct usb_interface *intf)
{
}

static struct usb_device_id ehset_id_table[] = {
	{ USB_DEVICE(0x1a0a, TEST_SE0_NAK_PID) },
	{ USB_DEVICE(0x1a0a, TEST_J_PID) },
	{ USB_DEVICE(0x1a0a, TEST_K_PID) },
	{ USB_DEVICE(0x1a0a, TEST_PACKET_PID) },
	{ USB_DEVICE(0x1a0a, TEST_HS_HOST_PORT_SUSPEND_RESUME) },
	{ USB_DEVICE(0x1a0a, TEST_SINGLE_STEP_GET_DEV_DESC) },
	{ USB_DEVICE(0x1a0a, TEST_SINGLE_STEP_SET_FEATURE) },
	{ }			/* Terminating entry */
};

MODULE_DEVICE_TABLE(usb, ehset_id_table);

static struct usb_driver ehset_driver = {
	.name =		"usb_ehset_test",
	.probe =	ehset_probe,
	.disconnect =	ehset_disconnect,
	.id_table =	ehset_id_table,
};

static int __init ehset_init(void)
{
	return usb_register(&ehset_driver);
}

static void __exit ehset_exit(void)
{
	usb_deregister(&ehset_driver);
}

module_init(ehset_init);
module_exit(ehset_exit);

MODULE_DESCRIPTION("USB Driver for EHSET Test Fixture");
MODULE_LICENSE("GPL v2");
