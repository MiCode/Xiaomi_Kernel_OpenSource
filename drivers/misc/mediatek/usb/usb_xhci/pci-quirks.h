/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2022 MediaTek Inc.
 */
#ifndef __LINUX_USB_PCI_QUIRKS_H
#define __LINUX_USB_PCI_QUIRKS_H

struct pci_dev;
static inline void usb_amd_quirk_pll_disable(void) {}
static inline void usb_amd_quirk_pll_enable(void) {}
static inline void usb_asmedia_modifyflowcontrol(struct pci_dev *pdev) {}
static inline void usb_amd_dev_put(void) {}
static inline void usb_disable_xhci_ports(struct pci_dev *xhci_pdev) {}
static inline void sb800_prefetch(struct device *dev, int on) {}
static inline bool usb_amd_pt_check_port(struct device *device, int port)
{
	return false;
}

#endif  /*  __LINUX_USB_PCI_QUIRKS_H  */
