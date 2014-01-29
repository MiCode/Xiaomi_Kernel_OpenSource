/*
 * Copyright (c) 2013,2014, The Linux Foundation. All rights reserved.
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

#ifndef _USB_TRACE_H_
#define _USB_TRACE_H_

#include <linux/tracepoint.h>

DECLARE_TRACE(usb_daytona_invalid_access,
	TP_PROTO(unsigned int ebi_addr,
	 unsigned int ebi_apacket0, unsigned int ebi_apacket1),
	TP_ARGS(ebi_addr, ebi_apacket0, ebi_apacket1));

#endif

