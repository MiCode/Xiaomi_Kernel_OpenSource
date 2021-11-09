// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include "inc/pd_core.h"
#include "inc/tcpci_event.h"
#include "inc/pd_process_evt.h"

#if CONFIG_USB_PD_CUSTOM_DBGACC

bool pd_process_event_dbg(struct pd_port *pd_port, struct pd_event *pd_event)
{
	/* Don't need to handle any PD message, Keep VBUS 5V, and using VDM */
	return false;
}

#endif /* CONFIG_USB_PD_CUSTOM_DBGACC */
