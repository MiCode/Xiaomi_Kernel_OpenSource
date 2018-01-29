/**
 * debug.h - DesignWare USB3 DRD Controller Debug Header
 *
 * Copyright (C) 2010-2011 Texas Instruments Incorporated - http://www.ti.com
 *
 * Authors: Felipe Balbi <balbi@ti.com>,
 *	    Sebastian Andrzej Siewior <bigeasy@linutronix.de>
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

#ifndef __DWC3_DEBUG_H
#define __DWC3_DEBUG_H

#include "core.h"

/*
 * NOTE: Make sure to have dwc as local variable in function before using
 * below macros.
 */
#define dbg_event(ep_num, name, status) \
	dwc3_dbg_print(dwc, ep_num, name, status, "")

#define dbg_print(ep_num, name, status, extra) \
	dwc3_dbg_print(dwc, ep_num, name, status, extra)

#define dbg_print_reg(name, reg) \
	dwc3_dbg_print_reg(dwc, name, reg)

#define dbg_done(ep_num, count, status) \
	dwc3_dbg_done(dwc, ep_num, count, status)

#define dbg_queue(ep_num, req, status) \
	dwc3_dbg_queue(dwc, ep_num, req, status)

#define dbg_setup(ep_num, req) \
	dwc3_dbg_setup(dwc, ep_num, req)

/**
 * dwc3_gadget_ep_cmd_string - returns endpoint command string
 * @cmd: command code
 */
static inline const char *
dwc3_gadget_ep_cmd_string(u8 cmd)
{
	switch (cmd) {
	case DWC3_DEPCMD_DEPSTARTCFG:
		return "Start New Configuration";
	case DWC3_DEPCMD_ENDTRANSFER:
		return "End Transfer";
	case DWC3_DEPCMD_UPDATETRANSFER:
		return "Update Transfer";
	case DWC3_DEPCMD_STARTTRANSFER:
		return "Start Transfer";
	case DWC3_DEPCMD_CLEARSTALL:
		return "Clear Stall";
	case DWC3_DEPCMD_SETSTALL:
		return "Set Stall";
	case DWC3_DEPCMD_GETEPSTATE:
		return "Get Endpoint State";
	case DWC3_DEPCMD_SETTRANSFRESOURCE:
		return "Set Endpoint Transfer Resource";
	case DWC3_DEPCMD_SETEPCONFIG:
		return "Set Endpoint Configuration";
	default:
		return "UNKNOWN command";
	}
}

/**
 * dwc3_gadget_generic_cmd_string - returns generic command string
 * @cmd: command code
 */
static inline const char *
dwc3_gadget_generic_cmd_string(u8 cmd)
{
	switch (cmd) {
	case DWC3_DGCMD_SET_LMP:
		return "Set LMP";
	case DWC3_DGCMD_SET_PERIODIC_PAR:
		return "Set Periodic Parameters";
	case DWC3_DGCMD_XMIT_FUNCTION:
		return "Transmit Function Wake Device Notification";
	case DWC3_DGCMD_SET_SCRATCHPAD_ADDR_LO:
		return "Set Scratchpad Buffer Array Address Lo";
	case DWC3_DGCMD_SET_SCRATCHPAD_ADDR_HI:
		return "Set Scratchpad Buffer Array Address Hi";
	case DWC3_DGCMD_SELECTED_FIFO_FLUSH:
		return "Selected FIFO Flush";
	case DWC3_DGCMD_ALL_FIFO_FLUSH:
		return "All FIFO Flush";
	case DWC3_DGCMD_SET_ENDPOINT_NRDY:
		return "Set Endpoint NRDY";
	case DWC3_DGCMD_RUN_SOC_BUS_LOOPBACK:
		return "Run SoC Bus Loopback Test";
	default:
		return "UNKNOWN";
	}
}

/**
 * dwc3_gadget_link_string - returns link name
 * @link_state: link state code
 */
static inline const char *
dwc3_gadget_link_string(enum dwc3_link_state link_state)
{
	switch (link_state) {
	case DWC3_LINK_STATE_U0:
		return "U0";
	case DWC3_LINK_STATE_U1:
		return "U1";
	case DWC3_LINK_STATE_U2:
		return "U2";
	case DWC3_LINK_STATE_U3:
		return "U3";
	case DWC3_LINK_STATE_SS_DIS:
		return "SS.Disabled";
	case DWC3_LINK_STATE_RX_DET:
		return "RX.Detect";
	case DWC3_LINK_STATE_SS_INACT:
		return "SS.Inactive";
	case DWC3_LINK_STATE_POLL:
		return "Polling";
	case DWC3_LINK_STATE_RECOV:
		return "Recovery";
	case DWC3_LINK_STATE_HRESET:
		return "Hot Reset";
	case DWC3_LINK_STATE_CMPLY:
		return "Compliance";
	case DWC3_LINK_STATE_LPBK:
		return "Loopback";
	case DWC3_LINK_STATE_RESET:
		return "Reset";
	case DWC3_LINK_STATE_RESUME:
		return "Resume";
	default:
		return "UNKNOWN link state\n";
	}
}

/**
 * dwc3_gadget_event_string - returns event name
 * @event: the event code
 */
static inline const char *dwc3_gadget_event_string(u8 event)
{
	switch (event) {
	case DWC3_DEVICE_EVENT_DISCONNECT:
		return "Disconnect";
	case DWC3_DEVICE_EVENT_RESET:
		return "Reset";
	case DWC3_DEVICE_EVENT_CONNECT_DONE:
		return "Connection Done";
	case DWC3_DEVICE_EVENT_LINK_STATUS_CHANGE:
		return "Link Status Change";
	case DWC3_DEVICE_EVENT_WAKEUP:
		return "WakeUp";
	case DWC3_DEVICE_EVENT_EOPF:
		return "End-Of-Frame";
	case DWC3_DEVICE_EVENT_SOF:
		return "Start-Of-Frame";
	case DWC3_DEVICE_EVENT_ERRATIC_ERROR:
		return "Erratic Error";
	case DWC3_DEVICE_EVENT_CMD_CMPL:
		return "Command Complete";
	case DWC3_DEVICE_EVENT_OVERFLOW:
		return "Overflow";
	}

	return "UNKNOWN";
}

/**
 * dwc3_ep_event_string - returns event name
 * @event: then event code
 */
static inline const char *dwc3_ep_event_string(u8 event)
{
	switch (event) {
	case DWC3_DEPEVT_XFERCOMPLETE:
		return "Transfer Complete";
	case DWC3_DEPEVT_XFERINPROGRESS:
		return "Transfer In-Progress";
	case DWC3_DEPEVT_XFERNOTREADY:
		return "Transfer Not Ready";
	case DWC3_DEPEVT_RXTXFIFOEVT:
		return "FIFO";
	case DWC3_DEPEVT_STREAMEVT:
		return "Stream";
	case DWC3_DEPEVT_EPCMDCMPLT:
		return "Endpoint Command Complete";
	}

	return "UNKNOWN";
}

/**
 * dwc3_gadget_event_type_string - return event name
 * @event: the event code
 */
static inline const char *dwc3_gadget_event_type_string(u8 event)
{
	switch (event) {
	case DWC3_DEVICE_EVENT_DISCONNECT:
		return "Disconnect";
	case DWC3_DEVICE_EVENT_RESET:
		return "Reset";
	case DWC3_DEVICE_EVENT_CONNECT_DONE:
		return "Connect Done";
	case DWC3_DEVICE_EVENT_LINK_STATUS_CHANGE:
		return "Link Status Change";
	case DWC3_DEVICE_EVENT_WAKEUP:
		return "Wake-Up";
	case DWC3_DEVICE_EVENT_HIBER_REQ:
		return "Hibernation";
	case DWC3_DEVICE_EVENT_EOPF:
		return "End of Periodic Frame";
	case DWC3_DEVICE_EVENT_SOF:
		return "Start of Frame";
	case DWC3_DEVICE_EVENT_ERRATIC_ERROR:
		return "Erratic Error";
	case DWC3_DEVICE_EVENT_CMD_CMPL:
		return "Command Complete";
	case DWC3_DEVICE_EVENT_OVERFLOW:
		return "Overflow";
	default:
		return "UNKNOWN";
	}
}

void dwc3_trace(void (*trace)(struct va_format *), const char *fmt, ...);

#ifdef CONFIG_DEBUG_FS
extern void dwc3_dbg_print(struct dwc3 *, u8, const char*, int, const char*);
extern void dwc3_dbg_done(struct dwc3 *, u8, const u32, int);
extern void dwc3_dbg_queue(struct dwc3 *, u8, const struct usb_request*, int);
extern void dwc3_dbg_setup(struct dwc3 *, u8, const struct usb_ctrlrequest*);
extern int dwc3_debugfs_init(struct dwc3 *);
extern void dwc3_debugfs_exit(struct dwc3 *);
extern void dwc3_dbg_print_reg(struct dwc3 *, const char *name, int reg);
#else
static inline void dwc3_dbg_print(struct dwc3 *dwc, u8 ep_num, const char *name,
		int status, const char *extra)
{  }
static inline void dwc3_dbg_done(struct dwc3 *dwc, u8 ep_num,
		const u32 count, int status)
{  }
static inline void dwc3_dbg_queue(struct dwc3 *dwc, u8 ep_num,
		const struct usb_request *req, int status)
{  }
static inline void dwc3_dbg_setup(struct dwc3 *dwc, u8 ep_num,
		const struct usb_ctrlrequest *req)
{  }
static inline void dwc3_dbg_print_reg(struct dwc3 *dwc, const char *name,
				int reg)
{  }
static inline int dwc3_debugfs_init(struct dwc3 *d)
{  return 0;  }
static inline void dwc3_debugfs_exit(struct dwc3 *d)
{  }
#endif
#endif /* __DWC3_DEBUG_H */
