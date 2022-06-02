/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2020-2021, The Linux Foundation. All rights reserved.
 */

#ifndef __LINUX_USB_REDRIVER_H
#define __LINUX_USB_REDRIVER_H

enum plug_orientation {
	ORIENTATION_CC1,
	ORIENTATION_CC2,
	ORIENTATION_UNKNOWN,
};

#ifdef CONFIG_USB_REDRIVER

int redriver_release_usb_lanes(struct device_node *node);
int redriver_notify_connect(struct device_node *node, enum plug_orientation orientation);
int redriver_notify_disconnect(struct device_node *node);
int redriver_orientation_get(struct device_node *node);
int redriver_gadget_pullup_enter(struct device_node *node, int is_on);
int redriver_gadget_pullup_exit(struct device_node *node, int is_on);
int redriver_powercycle(struct device_node *node);

#else

static inline int redriver_release_usb_lanes(struct device_node *node)
{
	return 0;
}

static inline int redriver_notify_connect(struct device_node *node,
					enum plug_orientation orientation)
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

static inline int redriver_gadget_pullup_enter(struct device_node *node,
						int is_on)
{
	return 0;
}

static inline int redriver_gadget_pullup_exit(struct device_node *node,
						int is_on)
{
	return 0;
}

static inline int redriver_powercycle(struct device_node *node)
{
	return 0;
}

#endif

#endif /*__LINUX_USB_REDRIVER_H */
