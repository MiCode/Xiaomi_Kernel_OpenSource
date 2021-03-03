/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * xHCI secondary ring APIs
 *
 * Copyright (c) 2019,2021 The Linux Foundation. All rights reserved.
 */

#ifndef __LINUX_XHCI_SEC_H
#define __LINUX_XHCI_SEC_H

#include <linux/usb.h>

#if IS_ENABLED(CONFIG_USB_XHCI_HCD)
int xhci_sec_event_ring_setup(struct usb_device *udev, unsigned int intr_num);
int xhci_sec_event_ring_cleanup(struct usb_device *udev, unsigned int intr_num);
phys_addr_t xhci_get_sec_event_ring_phys_addr(struct usb_device *udev,
		unsigned int intr_num, dma_addr_t *dma);
phys_addr_t xhci_get_xfer_ring_phys_addr(struct usb_device *udev,
		struct usb_host_endpoint *ep, dma_addr_t *dma);
int xhci_stop_endpoint(struct usb_device *udev, struct usb_host_endpoint *ep);
#else
static inline int xhci_sec_event_ring_setup(struct usb_device *udev,
		unsigned int intr_num)
{
	return -ENODEV;
}

static inline int xhci_sec_event_ring_cleanup(struct usb_device *udev,
		unsigned int intr_num)
{
	return -ENODEV;
}

static inline phys_addr_t xhci_get_sec_event_ring_phys_addr(
		struct usb_device *udev, unsigned int intr_num,
		dma_addr_t *dma)
{
	return 0;
}

static inline phys_addr_t xhci_get_xfer_ring_phys_addr(struct usb_device *udev,
		struct usb_host_endpoint *ep, dma_addr_t *dma)
{
	return 0;
}

static inline int xhci_stop_endpoint(struct usb_device *udev,
		struct usb_host_endpoint *ep)
{
	return -ENODEV;
}
#endif

#endif /* __LINUX_XHCI_SEC_H */
