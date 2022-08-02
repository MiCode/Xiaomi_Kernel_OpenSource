// SPDX-License-Identifier: GPL-2.0
/*
 * xHCI host controller driver
 *
 * Copyright (C) 2013 Xenia Ragiadakou
 * Copyright (C) 2022 MediaTek Inc.
 *
 * Author: Xenia Ragiadakou
 * Email : burzalodowa@gmail.com
 */

#define CREATE_TRACE_POINTS
#include "xhci-trace.h"

EXPORT_TRACEPOINT_SYMBOL_GPL(xhci_dbg_quirks_);
EXPORT_TRACEPOINT_SYMBOL_GPL(xhci_urb_enqueue_);
EXPORT_TRACEPOINT_SYMBOL_GPL(xhci_handle_transfer_);
EXPORT_TRACEPOINT_SYMBOL_GPL(xhci_urb_giveback_);
