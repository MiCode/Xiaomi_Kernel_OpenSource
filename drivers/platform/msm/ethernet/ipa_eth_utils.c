// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2019-2020, The Linux Foundation. All rights reserved
 */

#include <linux/module.h>
#include <linux/netdevice.h>

#include "ipa_eth_i.h"

#ifdef CONFIG_IPA_ETH_DEBUG
bool ipa_eth_ipc_logdbg = true;
#else
bool ipa_eth_ipc_logdbg;
#endif

static const char * const ipa_eth_device_events[IPA_ETH_DEV_EVENT_COUNT] = {
	[IPA_ETH_DEV_RESET_PREPARE] = "RESET_PREPARE",
	[IPA_ETH_DEV_RESET_COMPLETE] = "RESET_COMPLETE",
	[IPA_ETH_DEV_ADD_MACSEC_IF] = "ADD_MACSEC_IF",
	[IPA_ETH_DEV_DEL_MACSEC_IF] = "DEL_MACSEC_IF",
};

const char *ipa_eth_device_event_name(enum ipa_eth_device_event event)
{
	const char *name = "<unknown>";

	if (event < IPA_ETH_DEV_EVENT_COUNT && ipa_eth_device_events[event])
		name = ipa_eth_device_events[event];

	return name;
}

static const char * const
		ipa_eth_net_device_events[IPA_ETH_NET_DEVICE_MAX_EVENTS] = {
	[NETDEV_REGISTER] = "REGISTER",
	[NETDEV_UNREGISTER] = "UNREGISTER",
	[NETDEV_CHANGENAME] = "CHANGE_NAME",
	[NETDEV_PRE_UP] = "PRE_UP",
	[NETDEV_UP] = "UP",
	[NETDEV_GOING_DOWN] = "GOING_DOWN",
	[NETDEV_DOWN] = "DOWN",
	[NETDEV_CHANGE] = "CHANGE",
	[NETDEV_CHANGELOWERSTATE] = "CHANGE_LOWER_STATE",
	[NETDEV_PRECHANGEUPPER] = "PRE_CHANGE_UPPER",
	[NETDEV_CHANGEUPPER] = "CHANGE_UPPER",
	[NETDEV_JOIN] = "JOIN",
};

const char *ipa_eth_net_device_event_name(unsigned long event)
{
	const char *name = "<unknown>";

	if (event < IPA_ETH_NET_DEVICE_MAX_EVENTS &&
			ipa_eth_net_device_events[event])
		name = ipa_eth_net_device_events[event];

	return name;
}

static const char * const
		ipa_eth_pm_notifier_events[IPA_ETH_PM_NOTIFIER_MAX_EVENTS] = {
	[PM_HIBERNATION_PREPARE] = "HIBERNATION_PREPARE",
	[PM_POST_HIBERNATION] = "POST_HIBERNATION",
	[PM_SUSPEND_PREPARE] = "SUSPEND_PREPARE",
	[PM_POST_SUSPEND] = "POST_SUSPEND",
	[PM_RESTORE_PREPARE] = "RESTORE_PREPARE",
	[PM_POST_RESTORE] = "POST_RESTORE",
};

const char *ipa_eth_pm_notifier_event_name(unsigned long event)
{
	const char *name = "<unknown>";

	if (event < IPA_ETH_PM_NOTIFIER_MAX_EVENTS &&
			ipa_eth_pm_notifier_events[event])
		name = ipa_eth_pm_notifier_events[event];

	return name;
}

/* IPC Logging */
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
