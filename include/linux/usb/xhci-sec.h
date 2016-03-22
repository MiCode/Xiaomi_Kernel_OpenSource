/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * xHCI secondary ring APIs
 *
 * Copyright (c) 2019, The Linux Foundation. All rights reserved.
 */

#ifndef __LINUX_XHCI_SEC_H
#define __LINUX_XHCI_SEC_H

struct usb_device;

#if IS_ENABLED(CONFIG_USB_XHCI_HCD)
int xhci_sec_event_ring_setup(struct usb_device *udev, unsigned int intr_num);
int xhci_sec_event_ring_cleanup(struct usb_device *udev, unsigned int intr_num);
#else
static inline int xhci_sec_event_ring_setup(struct usb_device *udev,
		unsigned int intr_num);
{
	return -ENODEV;
}

static inline int xhci_sec_event_ring_cleanup(struct usb_device *udev,
		unsigned int intr_num)
{
	return -ENODEV;
}
#endif

#endif /* __LINUX_XHCI_SEC_H */
