/* Copyright (c) 2011, Code Aurora Forum. All rights reserved.
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

#ifndef __LINUX_USB_GADGET_XPORT_H__
#define __LINUX_USB_GADGET_XPORT_H__

enum transport_type {
	USB_GADGET_XPORT_UNDEF,
	USB_GADGET_XPORT_TTY,
	USB_GADGET_XPORT_SDIO,
	USB_GADGET_XPORT_SMD,
	USB_GADGET_XPORT_BAM,
	USB_GADGET_XPORT_NONE,
};

#define XPORT_STR_LEN	10

static char *xport_to_str(enum transport_type t)
{
	switch (t) {
	case USB_GADGET_XPORT_TTY:
		return "TTY";
	case USB_GADGET_XPORT_SDIO:
		return "SDIO";
	case USB_GADGET_XPORT_SMD:
		return "SMD";
	case USB_GADGET_XPORT_BAM:
		return "BAM";
	case USB_GADGET_XPORT_NONE:
		return "NONE";
	default:
		return "UNDEFINED";
	}
}

static enum transport_type str_to_xport(const char *name)
{
	if (!strncasecmp("TTY", name, XPORT_STR_LEN))
		return USB_GADGET_XPORT_TTY;
	if (!strncasecmp("SDIO", name, XPORT_STR_LEN))
		return USB_GADGET_XPORT_SDIO;
	if (!strncasecmp("SMD", name, XPORT_STR_LEN))
		return USB_GADGET_XPORT_SMD;
	if (!strncasecmp("BAM", name, XPORT_STR_LEN))
		return USB_GADGET_XPORT_BAM;
	if (!strncasecmp("", name, XPORT_STR_LEN))
		return USB_GADGET_XPORT_NONE;

	return USB_GADGET_XPORT_UNDEF;
}

#endif
