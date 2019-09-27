/* Copyright (c) 2019 The Linux Foundation. All rights reserved.
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

#include <linux/module.h>

#include "ipa_eth_i.h"

static const char * const ipa_eth_device_events[IPA_ETH_DEV_EVENT_COUNT] = {
	[IPA_ETH_DEV_RESET_PREPARE] = "RESET_PREPARE",
	[IPA_ETH_DEV_RESET_COMPLETE] = "RESET_COMPLETE",
};

const char *ipa_eth_device_event_name(enum ipa_eth_device_event event)
{
	const char *name = "<unknown>";

	if (event < IPA_ETH_DEV_EVENT_COUNT && ipa_eth_device_events[event])
		name = ipa_eth_device_events[event];

	return name;
}

/* IPC Logging */

bool ipa_eth_ipc_logdbg = IPA_ETH_IPC_LOGDBG_DEFAULT;
module_param(ipa_eth_ipc_logdbg, bool, 0444);
MODULE_PARM_DESC(ipa_eth_ipc_logdbg, "Log debug IPC messages");

static void *ipa_eth_ipc_logbuf;

void *ipa_eth_get_ipc_logbuf(void)
{
	return ipa_eth_ipc_logbuf;
}
EXPORT_SYMBOL(ipa_eth_get_ipc_logbuf);

void *ipa_eth_get_ipc_logbuf_dbg(void)
{
	return ipa_eth_ipc_logdbg ? ipa_eth_ipc_logbuf : NULL;
}
EXPORT_SYMBOL(ipa_eth_get_ipc_logbuf_dbg);

#define IPA_ETH_IPC_LOG_PAGES 128

int ipa_eth_ipc_log_init(void)
{
	if (ipa_eth_ipc_logbuf)
		return 0;

	ipa_eth_ipc_logbuf = ipc_log_context_create(
				IPA_ETH_IPC_LOG_PAGES, IPA_ETH_SUBSYS, 0);

	return ipa_eth_ipc_logbuf ? 0 : -EFAULT;
}

void ipa_eth_ipc_log_cleanup(void)
{
	if (ipa_eth_ipc_logbuf) {
		ipc_log_context_destroy(ipa_eth_ipc_logbuf);
		ipa_eth_ipc_logbuf = NULL;
	}
}
