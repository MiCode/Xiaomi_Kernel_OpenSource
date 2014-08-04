/*
 * Copyright (C) 2014 Intel Corp.
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
 * You should have received a copy of the GNU General Public License
 * along with this program;
 */

/* Extended capability IDs for Intel Vendor Defined */
#define XHCI_EXT_CAPS_INTEL_HOST_CAP	192

/* register definition */
#define DUAL_ROLE_CFG0		0x68
#define SW_VBUS_VALID		(1 << 24)
#define SW_IDPIN_EN		(1 << 21)
#define SW_IDPIN		(1 << 20)

#define DUAL_ROLE_CFG1		0x6c
#define SW_MODE			(1 << 29)

#define DUAL_ROLE_CFG1_POLL_TIMEOUT	1000

extern int xhci_intel_vendor_cap_init(struct xhci_hcd *xhci);
extern int xhci_intel_phy_mux_switch(struct xhci_hcd *xhci, int is_device_on);
