/* Copyright (c) 2011-2012, Code Aurora Forum. All rights reserved.
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
	USB_GADGET_XPORT_BAM2BAM,
	USB_GADGET_XPORT_HSIC,
	USB_GADGET_XPORT_HSUART,
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
	case USB_GADGET_XPORT_BAM2BAM:
		return "BAM2BAM";
	case USB_GADGET_XPORT_HSIC:
		return "HSIC";
	case USB_GADGET_XPORT_HSUART:
		return "HSUART";
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
	if (!strncasecmp("BAM2BAM", name, XPORT_STR_LEN))
		return USB_GADGET_XPORT_BAM2BAM;
	if (!strncasecmp("HSIC", name, XPORT_STR_LEN))
		return USB_GADGET_XPORT_HSIC;
	if (!strncasecmp("HSUART", name, XPORT_STR_LEN))
		return USB_GADGET_XPORT_HSUART;
	if (!strncasecmp("", name, XPORT_STR_LEN))
		return USB_GADGET_XPORT_NONE;

	return USB_GADGET_XPORT_UNDEF;
}

enum gadget_type {
	USB_GADGET_SERIAL,
	USB_GADGET_RMNET,
};

#define NUM_RMNET_HSIC_PORTS 1
#define NUM_DUN_HSIC_PORTS 1
#define NUM_PORTS (NUM_RMNET_HSIC_PORTS \
	+ NUM_DUN_HSIC_PORTS)

#define NUM_RMNET_HSUART_PORTS 1
#define NUM_DUN_HSUART_PORTS 1
#define NUM_HSUART_PORTS (NUM_RMNET_HSUART_PORTS \
	+ NUM_DUN_HSUART_PORTS)

int ghsic_ctrl_connect(void *, int);
void ghsic_ctrl_disconnect(void *, int);
int ghsic_ctrl_setup(unsigned int, enum gadget_type);
int ghsic_data_connect(void *, int);
void ghsic_data_disconnect(void *, int);
int ghsic_data_setup(unsigned int, enum gadget_type);

int ghsuart_ctrl_connect(void *, int);
void ghsuart_ctrl_disconnect(void *, int);
int ghsuart_ctrl_setup(unsigned int, enum gadget_type);
int ghsuart_data_connect(void *, int);
void ghsuart_data_disconnect(void *, int);
int ghsuart_data_setup(unsigned int, enum gadget_type);
#endif
