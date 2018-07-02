/* Copyright (c) 2018, The Linux Foundation. All rights reserved.
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

#ifndef _NET_CNSS_GENETLINK_H_
#define _NET_CNSS_GENETLINK_H_

#define CLD80211_MAX_COMMANDS 40
#define CLD80211_MAX_NL_DATA  4096

/**
 * enum cld80211_attr - Driver/Application embeds the data in nlmsg with the
 *			help of below attributes
 *
 * @CLD80211_ATTR_VENDOR_DATA: Embed all other attributes in this nested
 *	attribute.
 * @CLD80211_ATTR_DATA: Embed complete data in this attribute
 * @CLD80211_ATTR_META_DATA: Embed meta data for above data. This will help
 * wlan driver to peek into request message packet without opening up definition
 * of complete request message.
 *
 * Any new message in future can be added as another attribute
 */
enum cld80211_attr {
	CLD80211_ATTR_VENDOR_DATA = 1,
	CLD80211_ATTR_DATA,
	CLD80211_ATTR_META_DATA,
	/* add new attributes above here */

	__CLD80211_ATTR_AFTER_LAST,
	CLD80211_ATTR_MAX = __CLD80211_ATTR_AFTER_LAST - 1
};

/**
 * enum cld80211_multicast_groups - List of multicast groups supported
 *
 * @CLD80211_MCGRP_SVC_MSGS: WLAN service message will be sent to this group.
 *	Ex: Status ind messages
 * @CLD80211_MCGRP_HOST_LOGS: All logging related messages from driver will be
 *	sent to this multicast group
 * @CLD80211_MCGRP_FW_LOGS: Firmware logging messages will be sent to this group
 * @CLD80211_MCGRP_PER_PKT_STATS: Messages related packet stats debugging infra
 *	will be sent to this group
 * @CLD80211_MCGRP_DIAG_EVENTS: Driver/Firmware status logging diag events will
 *	be sent to this group
 * @CLD80211_MCGRP_FATAL_EVENTS: Any fatal message generated in driver/firmware
 *	will be sent to this group
 * @CLD80211_MCGRP_OEM_MSGS: All OEM message will be sent to this group
 *	Ex: LOWI messages
 */
enum cld80211_multicast_groups {
	CLD80211_MCGRP_SVC_MSGS,
	CLD80211_MCGRP_HOST_LOGS,
	CLD80211_MCGRP_FW_LOGS,
	CLD80211_MCGRP_PER_PKT_STATS,
	CLD80211_MCGRP_DIAG_EVENTS,
	CLD80211_MCGRP_FATAL_EVENTS,
	CLD80211_MCGRP_OEM_MSGS,
};

/**
 * typedef cld80211_cb - Callback to be called when an nlmsg is received with
 *			 the registered cmd_id command from userspace
 * @data: Payload of the message to be sent to driver
 * @data_len: Length of the payload
 * @cb_ctx: callback context to be returned to driver when the callback
 *	 is called
 * @pid: process id of the sender
 */
typedef void (*cld80211_cb)(const void *data, int data_len,
			    void *cb_ctx, int pid);

/**
 * register_cld_cmd_cb() - Allows cld driver to register for commands with
 *	callback
 * @cmd_id: Command to be registered. Valid range [1, CLD80211_MAX_COMMANDS]
 * @cb: Callback to be called when an nlmsg is received with cmd_id command
 *       from userspace
 * @cb_ctx: context provided by driver; Send this as cb_ctx of func()
 *         to driver
 */
int register_cld_cmd_cb(u8 cmd_id, cld80211_cb cb, void *cb_ctx);

/**
 * deregister_cld_cmd_cb() - Allows cld driver to de-register the command it
 *	has already registered
 * @cmd_id: Command to be deregistered.
 */
int deregister_cld_cmd_cb(u8 cmd_id);

/**
 * cld80211_get_genl_family() - Returns current netlink family context
 */
struct genl_family *cld80211_get_genl_family(void);

#endif /* _NET_CNSS_GENETLINK_H_ */
