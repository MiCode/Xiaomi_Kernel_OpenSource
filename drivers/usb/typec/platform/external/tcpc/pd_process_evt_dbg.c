/*
 * Copyright (C) 2020 Richtek Inc.
 *
 * Power Delivery Process Event For DBGACC
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include <linux/usb/tcpc/pd_core.h>
#include <linux/usb/tcpc/tcpci_event.h>
#include <linux/usb/tcpc/pd_process_evt.h>

#ifdef CONFIG_USB_PD_CUSTOM_DBGACC

bool pd_process_event_dbg(struct pd_port *pd_port, struct pd_event *pd_event)
{
	/* Don't need to handle any PD message, Keep VBUS 5V, and using VDM */
	return false;
}

#endif /* CONFIG_USB_PD_CUSTOM_DBGACC */
