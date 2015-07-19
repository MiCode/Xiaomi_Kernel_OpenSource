/* Copyright (c) 2015, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef DIAGIPCLOG_H
#define DIAGIPCLOG_H

#include <linux/ipc_logging.h>

#define DIAG_IPC_LOG_PAGES	50

#define DIAG_DEBUG_USERSPACE	0x0001
#define DIAG_DEBUG_MUX		0x0002
#define DIAG_DEBUG_DCI		0x0004
#define DIAG_DEBUG_PERIPHERALS	0x0008
#define DIAG_DEBUG_MASKS	0x0010
#define DIAG_DEBUG_POWER	0x0020
#define DIAG_DEBUG_BRIDGE	0x0040

#define DIAG_DEBUG

#ifdef DIAG_DEBUG
extern uint16_t diag_debug_mask;
extern void *diag_ipc_log;

#define DIAG_LOG(log_lvl, msg, ...)					\
	do {								\
		if (diag_ipc_log && (log_lvl & diag_debug_mask)) {	\
			ipc_log_string(diag_ipc_log,			\
				"[%s] " msg, __func__, ##__VA_ARGS__);	\
		}							\
	} while (0)
#else
#define DIAG_LOG(log_lvl, msg, ...)
#endif

#endif
