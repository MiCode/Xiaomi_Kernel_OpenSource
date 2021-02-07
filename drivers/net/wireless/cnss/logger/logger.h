/* SPDX-License-Identifier: GPL-2.0-only */
/* Copyright (c) 2016-2021, The Linux Foundation. All rights reserved. */

#ifndef _LOGGER_H_
#define _LOGGER_H_

#include <linux/module.h>
#include <linux/list.h>
#include <net/sock.h>
#include <linux/netlink.h>
#include <linux/skbuff.h>

#define CNSS_LOGGER_NL_MCAST_GRP_ID	0x01
#define CNSS_LOGGER_NL_MAX_PAYLOAD	256
#define CNSS_LOGGER_BROADCAST_ID	255

/**
 * struct aninlmsg - the wireless service message header
 * @nlh:	the netlink message header
 * @radio:	the radio index of this message
 * @wmsg:	the pointer to the wireless message data
 */
struct aninlmsg {
	struct  nlmsghdr *nlh;
	int radio;
	void *wmsg;
};

/**
 * struct logger_event_handler - the logger event handler structure
 * @list:	the event list associated to the same device
 * @event:	the event number
 * @radio_idx:	the callback handler
 */
struct logger_event_handler {
	struct list_head list;

	int event;
	int (*cb)(struct sk_buff *skb);
};

/**
 * struct logger_device - the logger device structure
 * @list:	the device list registered to logger module
 * @event_list:	the event list registered to this device
 * @ctx:	the pointer to the logger context
 * @wiphy:	the wiphy that associated to the device
 * @name:	the name of the device driver module
 * @radio_idx:	the radio index assigned to this device
 */
struct logger_device {
	struct list_head list;
	struct list_head event_list;

	struct logger_context *ctx;
	struct wiphy *wiphy;
	char name[MODULE_NAME_LEN];
	int radio_idx;
};

/**
 * struct logger_context - the main context block for logger module
 * @dev_list:	this is the list to maintain the devices that registered
 *		to use the logger module feature
 * @nl_sock:	the netlink socket to share accros the module
 * @con_mutex:	the mutex to protect concurrent access
 * @data_lock:	the lock to protect shared data
 * @radio_mask: this mask would maintain the radio index assign and release
 */
struct logger_context {
	struct list_head dev_list;

	struct sock *nl_sock;
	struct mutex con_mutex; /* concurrent access mutex */
	spinlock_t data_lock;
	unsigned long radio_mask; /* support up to 4 drivers registration? */
#ifdef CONFIG_DEBUG_FS
	struct dentry *debugfs_entry;
#endif
};

int logger_netlink_init(struct logger_context *ctx);
int logger_netlink_deinit(struct logger_context *ctx);
struct logger_context *logger_get_ctx(void);

#ifdef CONFIG_DEBUG_FS
void logger_debugfs_init(struct logger_context *ctx);
void logger_debugfs_remove(struct logger_context *ctx);
#else
static inline void logger_debugfs_init(struct logger_context *ctx) {}
static inline void logger_debugfs_remove(struct logger_context *ctx) {}
#endif

#endif /* _LOGGER_H_ */
