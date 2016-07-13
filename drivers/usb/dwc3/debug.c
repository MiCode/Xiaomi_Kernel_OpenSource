/**
 * debug.c - DesignWare USB3 DRD Controller Debug/Trace Support
 *
 * Copyright (C) 2014 Texas Instruments Incorporated - http://www.ti.com
 *
 * Author: Felipe Balbi <balbi@ti.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2  of
 * the License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include "debug.h"

#include <linux/moduleparam.h>

static unsigned int ep_addr_rxdbg_mask = 1;
module_param(ep_addr_rxdbg_mask, uint, 0644);
static unsigned int ep_addr_txdbg_mask = 1;
module_param(ep_addr_txdbg_mask, uint, 0644);

void dwc3_trace(void (*trace)(struct va_format *), const char *fmt, ...)
{
	struct va_format vaf;
	va_list args;

	va_start(args, fmt);
	vaf.fmt = fmt;
	vaf.va = &args;

	trace(&vaf);

	va_end(args);
}

static int allow_dbg_print(u8 ep_num)
{
	int dir, num;

	/* allow bus wide events */
	if (ep_num == 0xff)
		return 1;

	dir = ep_num & 0x1;
	num = ep_num >> 1;
	num = 1 << num;

	if (dir && (num & ep_addr_txdbg_mask))
		return 1;
	if (!dir && (num & ep_addr_rxdbg_mask))
		return 1;

	return 0;
}

/**
 * dwc3_dbg_print:  prints the common part of the event
 * @addr:   endpoint address
 * @name:   event name
 * @status: status
 * @extra:  extra information
 * @dwc3: pointer to struct dwc3
 */
void dwc3_dbg_print(struct dwc3 *dwc, u8 ep_num, const char *name,
			int status, const char *extra)
{
	if (!allow_dbg_print(ep_num))
		return;

	if (name == NULL)
		return;

	ipc_log_string(dwc->dwc_ipc_log_ctxt, "%02X %-25.25s %4i ?\t%s",
			ep_num, name, status, extra);
}

/**
 * dwc3_dbg_done: prints a DONE event
 * @addr:   endpoint address
 * @td:     transfer descriptor
 * @status: status
 * @dwc3: pointer to struct dwc3
 */
void dwc3_dbg_done(struct dwc3 *dwc, u8 ep_num,
		const u32 count, int status)
{
	if (!allow_dbg_print(ep_num))
		return;

	ipc_log_string(dwc->dwc_ipc_log_ctxt, "%02X %-25.25s %4i ?\t%d",
			ep_num, "DONE", status, count);
}

/**
 * dwc3_dbg_event: prints a generic event
 * @addr:   endpoint address
 * @name:   event name
 * @status: status
 */
void dwc3_dbg_event(struct dwc3 *dwc, u8 ep_num, const char *name, int status)
{
	if (!allow_dbg_print(ep_num))
		return;

	if (name != NULL)
		dwc3_dbg_print(dwc, ep_num, name, status, "");
}

/*
 * dwc3_dbg_queue: prints a QUEUE event
 * @addr:   endpoint address
 * @req:    USB request
 * @status: status
 */
void dwc3_dbg_queue(struct dwc3 *dwc, u8 ep_num,
		const struct usb_request *req, int status)
{
	if (!allow_dbg_print(ep_num))
		return;

	if (req != NULL) {
		ipc_log_string(dwc->dwc_ipc_log_ctxt,
			"%02X %-25.25s %4i ?\t%d %d", ep_num, "QUEUE", status,
			!req->no_interrupt, req->length);
	}
}

/**
 * dwc3_dbg_setup: prints a SETUP event
 * @addr: endpoint address
 * @req:  setup request
 */
void dwc3_dbg_setup(struct dwc3 *dwc, u8 ep_num,
		const struct usb_ctrlrequest *req)
{
	if (!allow_dbg_print(ep_num))
		return;

	if (req != NULL) {
		ipc_log_string(dwc->dwc_ipc_log_ctxt,
			"%02X %-25.25s ?\t%02X %02X %04X %04X %d",
			ep_num, "SETUP", req->bRequestType,
			req->bRequest, le16_to_cpu(req->wValue),
			le16_to_cpu(req->wIndex), le16_to_cpu(req->wLength));
	}
}

/**
 * dwc3_dbg_print_reg: prints a reg value
 * @name:   reg name
 * @reg: reg value to be printed
 */
void dwc3_dbg_print_reg(struct dwc3 *dwc, const char *name, int reg)
{
	if (name == NULL)
		return;

	ipc_log_string(dwc->dwc_ipc_log_ctxt, "%s = 0x%08x", name, reg);
}
