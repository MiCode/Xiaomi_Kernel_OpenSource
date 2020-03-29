/* Copyright (c) 2019-2020 The Linux Foundation. All rights reserved.
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
#include <linux/netdevice.h>

#include "ipa_eth_i.h"

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

static void __ipa_eth_free_msg(void *buff, u32 len, u32 type) {}

static int ipa_eth_send_ecm_msg(struct net_device *net_dev,
		enum ipa_ecm_event ecm_event)
{
	struct ipa_msg_meta msg_meta;
	struct ipa_ecm_msg ecm_msg;

	if (!net_dev)
		return -EFAULT;

	memset(&msg_meta, 0, sizeof(msg_meta));
	memset(&ecm_msg, 0, sizeof(ecm_msg));

	ecm_msg.ifindex = net_dev->ifindex;
	strlcpy(ecm_msg.name, net_dev->name, IPA_RESOURCE_NAME_MAX);

	msg_meta.msg_type = ecm_event;
	msg_meta.msg_len = sizeof(struct ipa_ecm_msg);

	return ipa_send_msg(&msg_meta, &ecm_msg, __ipa_eth_free_msg);
}

int ipa_eth_send_msg_connect(struct net_device *net_dev)
{
	ipa_eth_log("Sending ECM_CONNECT for %s", net_dev->name);
	return ipa_eth_send_ecm_msg(net_dev, ECM_CONNECT);
}

int ipa_eth_send_msg_disconnect(struct net_device *net_dev)
{
	ipa_eth_log("Sending ECM_DISCONNECT for %s", net_dev->name);
	return ipa_eth_send_ecm_msg(net_dev, ECM_DISCONNECT);
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
