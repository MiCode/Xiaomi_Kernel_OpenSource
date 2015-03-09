/*
 * usb port device code
 *
 * Copyright (C) 2012 Intel Corp
 *
 * Author: Lan Tianyu <tianyu.lan@intel.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 */

#include <linux/slab.h>
#include <linux/pm_qos.h>

#include "hub.h"

static const struct attribute_group *port_dev_group[];

static ssize_t connect_type_show(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	struct usb_port *port_dev = to_usb_port(dev);
	char *result;

	switch (port_dev->connect_type) {
	case USB_PORT_CONNECT_TYPE_HOT_PLUG:
		result = "hotplug";
		break;
	case USB_PORT_CONNECT_TYPE_HARD_WIRED:
		result = "hardwired";
		break;
	case USB_PORT_NOT_USED:
		result = "not used";
		break;
	default:
		result = "unknown";
		break;
	}

	return sprintf(buf, "%s\n", result);
}
static DEVICE_ATTR_RO(connect_type);

static ssize_t usb3_lpm_show(struct device *dev,
			      struct device_attribute *attr, char *buf)
{
	struct usb_port *port_dev = to_usb_port(dev);
	const char *p;

	if (port_dev->u1_is_enabled) {
		if (port_dev->u2_is_enabled)
			p = "u1_u2";
		else
			p = "u1";
	} else {
		if (port_dev->u2_is_enabled)
			p = "u2";
		else
			p = "0";
	}

	return sprintf(buf, "%s\n", p);
}

static ssize_t usb3_lpm_store(struct device *dev,
			       struct device_attribute *attr,
			       const char *buf, size_t count)
{
	struct usb_port *port_dev = to_usb_port(dev);
	struct usb_device *udev = port_dev->child;
	struct usb_hcd *hcd;

	if (!strncmp(buf, "u1_u2", 5)) {
		port_dev->u1_is_enabled = true;
		port_dev->u2_is_enabled = true;

	} else if (!strncmp(buf, "u1", 2)) {
		port_dev->u1_is_enabled = true;
		port_dev->u2_is_enabled = false;

	} else if (!strncmp(buf, "u2", 2)) {
		port_dev->u1_is_enabled = false;
		port_dev->u2_is_enabled = true;

	} else if (!strncmp(buf, "0", 1)) {
		port_dev->u1_is_enabled = false;
		port_dev->u2_is_enabled = false;
	} else
		return -EINVAL;

	/* If device is connected to the port, disable & enable lpm
	 * to make new u1 u2 setting take effect immediately
	 */
	if (udev) {
		hcd = bus_to_hcd(udev->bus);
		if (!hcd)
			return -EINVAL;
		usb_lock_device(udev);
		mutex_lock(hcd->bandwidth_mutex);
		if (!usb_disable_lpm(udev))
			usb_enable_lpm(udev);
		mutex_unlock(hcd->bandwidth_mutex);
		usb_unlock_device(udev);
	}

	return count;
}
static DEVICE_ATTR_RW(usb3_lpm);

static struct attribute *port_dev_attrs[] = {
	&dev_attr_connect_type.attr,
	NULL,
};

static struct attribute_group port_dev_attr_grp = {
	.attrs = port_dev_attrs,
};

static const struct attribute_group *port_dev_group[] = {
	&port_dev_attr_grp,
	NULL,
};

static struct attribute *port_dev_usb3_attrs[] = {
	&dev_attr_usb3_lpm.attr,
	NULL,
};

static struct attribute_group port_dev_usb3_attr_grp = {
	.attrs = port_dev_usb3_attrs,
};

static const struct attribute_group *port_dev_usb3_group[] = {
	&port_dev_attr_grp,
	&port_dev_usb3_attr_grp,
	NULL,
};

static void usb_port_device_release(struct device *dev)
{
	struct usb_port *port_dev = to_usb_port(dev);

	kfree(port_dev);
}

#ifdef CONFIG_PM_RUNTIME
static int usb_port_runtime_resume(struct device *dev)
{
	struct usb_port *port_dev = to_usb_port(dev);
	struct usb_device *hdev = to_usb_device(dev->parent->parent);
	struct usb_interface *intf = to_usb_interface(dev->parent);
	struct usb_hub *hub = usb_hub_to_struct_hub(hdev);
	int port1 = port_dev->portnum;
	int retval;

	if (!hub)
		return -EINVAL;

	usb_autopm_get_interface(intf);
	retval = usb_hub_set_port_power(hdev, hub, port1, true);
	if (port_dev->child && !retval) {
		/*
		 * Attempt to wait for usb hub port to be reconnected in order
		 * to make the resume procedure successful.  The device may have
		 * disconnected while the port was powered off, so ignore the
		 * return status.
		 */
		retval = hub_port_debounce_be_connected(hub, port1);
		if (retval < 0)
			dev_dbg(&port_dev->dev, "can't get reconnection after setting port  power on, status %d\n",
					retval);
		usb_clear_port_feature(hdev, port1, USB_PORT_FEAT_C_ENABLE);
		retval = 0;
	}

	usb_autopm_put_interface(intf);
	return retval;
}

static int usb_port_runtime_suspend(struct device *dev)
{
	struct usb_port *port_dev = to_usb_port(dev);
	struct usb_device *hdev = to_usb_device(dev->parent->parent);
	struct usb_interface *intf = to_usb_interface(dev->parent);
	struct usb_hub *hub = usb_hub_to_struct_hub(hdev);
	int port1 = port_dev->portnum;
	int retval;

	if (!hub)
		return -EINVAL;

	if (dev_pm_qos_flags(&port_dev->dev, PM_QOS_FLAG_NO_POWER_OFF)
			== PM_QOS_FLAGS_ALL)
		return -EAGAIN;

	usb_autopm_get_interface(intf);
	retval = usb_hub_set_port_power(hdev, hub, port1, false);
	usb_clear_port_feature(hdev, port1, USB_PORT_FEAT_C_CONNECTION);
	usb_clear_port_feature(hdev, port1, USB_PORT_FEAT_C_ENABLE);
	usb_autopm_put_interface(intf);
	return retval;
}
#endif

static const struct dev_pm_ops usb_port_pm_ops = {
#ifdef CONFIG_PM_RUNTIME
	.runtime_suspend =	usb_port_runtime_suspend,
	.runtime_resume =	usb_port_runtime_resume,
#endif
};

struct device_type usb_port_device_type = {
	.name =		"usb_port",
	.release =	usb_port_device_release,
	.pm =		&usb_port_pm_ops,
};

int usb_hub_create_port_device(struct usb_hub *hub, int port1)
{
	struct usb_port *port_dev = NULL;
	struct usb_device *hdev = hub->hdev;
	int retval;

	port_dev = kzalloc(sizeof(*port_dev), GFP_KERNEL);
	if (!port_dev) {
		retval = -ENOMEM;
		goto exit;
	}

	hub->ports[port1 - 1] = port_dev;
	port_dev->portnum = port1;
	port_dev->power_is_on = true;
	port_dev->dev.parent = hub->intfdev;
	if (hub_is_superspeed(hdev)) {
		port_dev->u1_is_enabled = true;
		port_dev->u2_is_enabled = true;
		port_dev->dev.groups = port_dev_usb3_group;
	} else
		port_dev->dev.groups = port_dev_group;
	port_dev->dev.type = &usb_port_device_type;
	dev_set_name(&port_dev->dev, "port%d", port1);
	mutex_init(&port_dev->status_lock);
	retval = device_register(&port_dev->dev);
	if (retval)
		goto error_register;

	pm_runtime_set_active(&port_dev->dev);

	/* It would be dangerous if user space couldn't
	 * prevent usb device from being powered off. So don't
	 * enable port runtime pm if failed to expose port's pm qos.
	 */
	if (!dev_pm_qos_expose_flags(&port_dev->dev,
			PM_QOS_FLAG_NO_POWER_OFF))
		pm_runtime_enable(&port_dev->dev);

	device_enable_async_suspend(&port_dev->dev);
	return 0;

error_register:
	put_device(&port_dev->dev);
exit:
	return retval;
}

void usb_hub_remove_port_device(struct usb_hub *hub,
				       int port1)
{
	device_unregister(&hub->ports[port1 - 1]->dev);
}

