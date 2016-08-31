/* Copyright (c) 2016, The Linux Foundation. All rights reserved.
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

#include <linux/err.h>

#include "debug.h"

#define CNSS_IPC_LOG_PAGES		32

void *cnss_ipc_log_context;

int cnss_debug_init(void)
{
	cnss_ipc_log_context = ipc_log_context_create(CNSS_IPC_LOG_PAGES,
						      "cnss", 0);
	if (!cnss_ipc_log_context) {
		cnss_pr_err("Unable to create IPC log context!\n");
		return -EINVAL;
	}

	return 0;
}

void cnss_debug_deinit(void)
{
	if (cnss_ipc_log_context) {
		ipc_log_context_destroy(cnss_ipc_log_context);
		cnss_ipc_log_context = NULL;
	}
}
