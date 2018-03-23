/*
 * Copyright (c) 2010-2013, 2018, The Linux Foundation. All rights reserved.
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
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/usb.h>
#include <linux/usb/ch11.h>

#define TEST_SE0_NAK_PID			0x0101
#define TEST_J_PID				0x0102
#define TEST_K_PID				0x0103
#define TEST_PACKET_PID				0x0104
#define TEST_HS_HOST_PORT_SUSPEND_RESUME	0x0106
#define TEST_SINGLE_STEP_GET_DEV_DESC		0x0107
#define TEST_SINGLE_STEP_SET_FEATURE		0x0108

static u8 numPorts;

static int ehset_get_port_num(struct device *dev, const char *buf,
							unsigned long *val)
{
	int ret;

	ret = kstrtoul(buf, 10, val);
	if (ret < 0) {
		dev_err(dev, "couldn't parse string %d\n", ret);
		return ret;
	}

	if (!*val || *val > numPorts) {
		dev_err(dev, "Invalid port num entered\n");
		return -EINVAL;
	}

	return 0;
}

static int ehset_clear_port_feature(struct usb_device *udev, int feature,
				int port1)
{
	return usb_control_msg(udev, usb_sndctrlpipe(udev, 0),
		USB_REQ_CLEAR_FEATURE, USB_RT_PORT, feature, port1,
		NULL, 0, 1000);
}

static int ehset_set_port_feature(struct usb_device *udev, int feature,
			int port1, int timeout)
{
	return usb_control_msg(udev, usb_sndctrlpipe(udev, 0),
		USB_REQ_SET_FEATURE, USB_RT_PORT, feature, port1,
		NULL, 0, timeout);
}

static int ehset_set_testmode(struct device *dev, struct usb_device *child_udev,
			struct usb_device *hub_udev, int test_id, int port)
{
	struct usb_device_descriptor *buf;
	int ret = -EINVAL;

	switch (test_id) {
	case TEST_SE0_NAK_PID:
		ret = ehset_set_port_feature(hub_udev, USB_PORT_FEAT_TEST,
					(TEST_SE0_NAK << 8) | port, 1000);
		break;
	case TEST_J_PID:
		ret = ehset_set_port_feature(hub_udev, USB_PORT_FEAT_TEST,
					(TEST_J << 8) | port, 1000);
		break;
	case TEST_K_PID:
		ret = ehset_set_port_feature(hub_udev, USB_PORT_FEAT_TEST,
					(TEST_K << 8) | port, 1000);
		break;
	case TEST_PACKET_PID:
		ret = ehset_set_port_feature(hub_udev, USB_PORT_FEAT_TEST,
					(TEST_PACKET << 8) | port, 1000);
		break;
	case TEST_HS_HOST_PORT_SUSPEND_RESUME:
		/* Test: wait for 15secs -> suspend -> 15secs delay -> resume */
		msleep(15 * 1000);
		ret = ehset_set_port_feature(hub_udev, USB_PORT_FEAT_SUSPEND,
							port, 1000);
		if (ret)
			break;

		msleep(15 * 1000);
		ret = ehset_clear_port_feature(hub_udev, USB_PORT_FEAT_SUSPEND,
							port);
		break;
	case TEST_SINGLE_STEP_GET_DEV_DESC:
		/* Test: wait for 15secs -> GetDescriptor request */
		msleep(15 * 1000);
		buf = kmalloc(USB_DT_DEVICE_SIZE, GFP_KERNEL);
		if (!buf) {
			ret = -ENOMEM;
			break;
		}

		ret = usb_control_msg(child_udev,
					usb_rcvctrlpipe(child_udev, 0),
					USB_REQ_GET_DESCRIPTOR, USB_DIR_IN,
					USB_DT_DEVICE << 8, 0,
					buf, USB_DT_DEVICE_SIZE,
					USB_CTRL_GET_TIMEOUT);
		kfree(buf);
		break;
	case TEST_SINGLE_STEP_SET_FEATURE:
		/*
		 * GetDescriptor SETUP request -> 15secs delay -> IN & STATUS
		 *
		 * Note, this test is only supported on root hubs since the
		 * SetPortFeature handling can only be done inside the HCD's
		 * hub_control callback function.
		 */
		if (hub_udev != child_udev->bus->root_hub) {
			dev_err(dev, "SINGLE_STEP_SET_FEATURE test only supported on root hub\n");
			break;
		}

		ret = ehset_set_port_feature(hub_udev, USB_PORT_FEAT_TEST,
						(6 << 8) | port, 60 * 1000);

		break;
	default:
		dev_err(dev, "%s: unsupported test ID: 0x%x\n",
			__func__, test_id);
	}

	return ret;
}

static ssize_t test_se0_nak_portnum_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct usb_interface *intf = to_usb_interface(dev);
	struct usb_device *udev = interface_to_usbdev(intf);
	unsigned long portnum;
	int ret;

	ret = ehset_get_port_num(dev, buf, &portnum);
	if (ret)
		return ret;

	usb_lock_device(udev);
	ret = ehset_set_testmode(dev, NULL, udev, TEST_SE0_NAK_PID, portnum);
	usb_unlock_device(udev);
	if (ret) {
		dev_err(dev, "Error %d while SE0_NAK test\n", ret);
		return ret;
	}

	return count;
}
static DEVICE_ATTR_WO(test_se0_nak_portnum);

static ssize_t test_j_portnum_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct usb_interface *intf = to_usb_interface(dev);
	struct usb_device *udev = interface_to_usbdev(intf);
	unsigned long portnum;
	int ret;

	ret = ehset_get_port_num(dev, buf, &portnum);
	if (ret)
		return ret;

	usb_lock_device(udev);
	ret = ehset_set_testmode(dev, NULL, udev, TEST_J_PID, portnum);
	usb_unlock_device(udev);
	if (ret) {
		dev_err(dev, "Error %d while J state test\n", ret);
		return ret;
	}

	return count;
}
static DEVICE_ATTR_WO(test_j_portnum);

static ssize_t test_k_portnum_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct usb_interface *intf = to_usb_interface(dev);
	struct usb_device *udev = interface_to_usbdev(intf);
	unsigned long portnum;
	int ret;

	ret = ehset_get_port_num(dev, buf, &portnum);
	if (ret)
		return ret;

	usb_lock_device(udev);
	ret = ehset_set_testmode(dev, NULL, udev, TEST_K_PID, portnum);
	usb_unlock_device(udev);
	if (ret) {
		dev_err(dev, "Error %d while K state test\n", ret);
		return ret;
	}

	return count;
}
static DEVICE_ATTR_WO(test_k_portnum);

static ssize_t test_packet_portnum_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct usb_interface *intf = to_usb_interface(dev);
	struct usb_device *udev = interface_to_usbdev(intf);
	unsigned long portnum;
	int ret;

	ret = ehset_get_port_num(dev, buf, &portnum);
	if (ret)
		return ret;

	usb_lock_device(udev);
	ret = ehset_set_testmode(dev, NULL, udev, TEST_PACKET_PID, portnum);
	usb_unlock_device(udev);
	if (ret) {
		dev_err(dev, "Error %d while sending test packets\n", ret);
		return ret;
	}

	return count;
}
static DEVICE_ATTR_WO(test_packet_portnum);

static ssize_t test_port_susp_resume_portnum_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct usb_interface *intf = to_usb_interface(dev);
	struct usb_device *udev = interface_to_usbdev(intf);
	unsigned long portnum;
	int ret;

	ret = ehset_get_port_num(dev, buf, &portnum);
	if (ret)
		return ret;

	usb_lock_device(udev);
	ret = ehset_set_testmode(dev, NULL, udev,
				TEST_HS_HOST_PORT_SUSPEND_RESUME, portnum);
	usb_unlock_device(udev);
	if (ret) {
		dev_err(dev, "Error %d while port suspend resume test\n", ret);
		return ret;
	}

	return count;
}
static DEVICE_ATTR_WO(test_port_susp_resume_portnum);

static struct attribute *ehset_attributes[] = {
	&dev_attr_test_se0_nak_portnum.attr,
	&dev_attr_test_j_portnum.attr,
	&dev_attr_test_k_portnum.attr,
	&dev_attr_test_packet_portnum.attr,
	&dev_attr_test_port_susp_resume_portnum.attr,
	NULL
};

static const struct attribute_group ehset_attr_group = {
	.attrs = ehset_attributes,
};

static int ehset_probe(struct usb_interface *intf,
		       const struct usb_device_id *id)
{
	int ret = -EINVAL;
	struct usb_device *dev = interface_to_usbdev(intf);
	struct usb_device *hub_udev = dev->parent;
	u8 portnum = dev->portnum;
	u16 test_pid = le16_to_cpu(dev->descriptor.idProduct);

	/*
	 * If an external hub does not support the EHSET test fixture, then user
	 * can forcefully unbind the external hub from the hub driver (to which
	 * an external hub gets bound by default) and bind it to this driver, so
	 * as to send test signals on any downstream port of the hub.
	 */
	if (dev->descriptor.bDeviceClass == USB_CLASS_HUB) {
		struct usb_hub_descriptor *descriptor;

		descriptor = kzalloc(sizeof(*descriptor), GFP_KERNEL);
		if (!descriptor)
			return -ENOMEM;

		ret = usb_control_msg(dev, usb_rcvctrlpipe(dev, 0),
				USB_REQ_GET_DESCRIPTOR, USB_DIR_IN | USB_RT_HUB,
				USB_DT_HUB << 8, 0, descriptor,
				USB_DT_HUB_NONVAR_SIZE, USB_CTRL_GET_TIMEOUT);
		if (ret < 0) {
			dev_err(&intf->dev, "%s: Failed to get hub desc %d\n",
								__func__, ret);
			kfree(descriptor);
			return ret;
		}

		numPorts = descriptor->bNbrPorts;
		ret = sysfs_create_group(&intf->dev.kobj, &ehset_attr_group);
		if (ret < 0)
			dev_err(&intf->dev, "%s: Failed to create sysfs nodes %d\n",
								__func__, ret);

		kfree(descriptor);
		return ret;
	}

	ret = ehset_set_testmode(&intf->dev, dev, hub_udev, test_pid, portnum);

	return (ret < 0) ? ret : 0;
}

static void ehset_disconnect(struct usb_interface *intf)
{
	struct usb_device *dev = interface_to_usbdev(intf);

	numPorts = 0;
	if (dev->descriptor.bDeviceClass == USB_CLASS_HUB)
		sysfs_remove_group(&intf->dev.kobj, &ehset_attr_group);
}

static const struct usb_device_id ehset_id_table[] = {
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

module_usb_driver(ehset_driver);

MODULE_DESCRIPTION("USB Driver for EHSET Test Fixture");
MODULE_LICENSE("GPL v2");
