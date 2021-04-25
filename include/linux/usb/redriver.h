/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2020-2021, The Linux Foundation. All rights reserved.
 */

#ifndef __LINUX_USB_REDRIVER_H
#define __LINUX_USB_REDRIVER_H

#ifdef CONFIG_USB_REDRIVER

int redriver_release_usb_lanes(struct device_node *node);
int redriver_notify_connect(struct device_node *node);
int redriver_notify_disconnect(struct device_node *node);
int redriver_orientation_get(struct device_node *node);
int redriver_gadget_pullup(struct device_node *node, int is_on);

#else

static inline int redriver_release_usb_lanes(struct device_node *node)
{
	return 0;
}

static inline int redriver_notify_connect(struct device_node *node)
{
	return 0;
}

static inline int redriver_notify_disconnect(struct device_node *node)
{
	return 0;
}

static inline int redriver_orientation_get(struct device_node *node)
{
	return -ENODEV;
}

static inline int redriver_gadget_pullup(struct device_node *node, int is_on)
{
	return 0;
}

#endif

#endif /*__LINUX_USB_REDRIVER_H */
