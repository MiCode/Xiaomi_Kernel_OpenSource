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
#define XHCI_EXT_CAPS_INTEL_SSIC	196
#define XHCI_EXT_CAPS_INTEL_SSIC_PROFILE	197

/* register definition */
#define PMCTRL			0x34
#define INT_PME_FLAG_CLR	(1 << 28)

#define DUAL_ROLE_CFG0		0x68
#define SW_VBUS_VALID		(1 << 24)
#define SW_IDPIN_EN		(1 << 21)
#define SW_IDPIN		(1 << 20)

#define DUAL_ROLE_CFG1		0x6c
#define SW_MODE			(1 << 29)

#define SSIC_PORT_NUM		2

/* SSIC Configuration Register 2
 * Address Offset: 0Ch-0Fh
 * Port 1 ... N : 0Ch, 3Ch, ... ,(0Ch + (NumSSICPorts-1)*30h)
 */
#define SSIC_PORT_CFG2		0xc
#define SSIC_PORT_CFG2_OFFSET	0x30
#define PROG_DONE		(1 << 30)
#define SSIC_PORT_UNUSED	(1 << 31)

/* SSIC Port N Register Access Control
 * Address Offset: 04h – 07h,
 * Port 1 ... N : 04h, 114h, 224h
 */
#define SSIC_ACCESS_CTRL	0x4
#define SSIC_ACCESS_CTRL_OFFSET	0x110
#define SSIC_ACCESS_CTRL_REGISTER_BANK_VALID	(1 << 25)


#define DUAL_ROLE_CFG1_POLL_TIMEOUT	1000

extern int xhci_intel_vendor_cap_init(struct xhci_hcd *xhci);
extern int xhci_intel_phy_vbus_valid(struct xhci_hcd *xhci, int vbus_valid);
extern int xhci_intel_phy_mux_switch(struct xhci_hcd *xhci, int is_device_on);
extern void xhci_intel_clr_internal_pme_flag(struct xhci_hcd *xhci);
extern void xhci_intel_ssic_port_unused(struct xhci_hcd *xhci, bool unused);
