/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#ifndef _AOLTEST_NETLINK_H_
#define _AOLTEST_NETLINK_H_

#include <linux/types.h>
#include <linux/compiler.h>

struct test_info {
	unsigned int wifi_enabled;
	unsigned int wifi_scan_intvl;
	unsigned int wifi_cb_intvl;
	unsigned int bt_enabled;
	unsigned int bt_scan_intvl;
	unsigned int bt_cb_intvl;
	unsigned int gps_enabled;
	unsigned int gps_scan_intvl;
	unsigned int gps_cb_intvl;
};

enum aoltest_cmd_type {
	AOLTEST_CMD_DEFAULT = 0,
	AOLTEST_CMD_START_TEST = 1,
	AOLTEST_CMD_STOP_TEST = 2,
	AOLTEST_CMD_START_DATA_TRANS = 3,
	AOLTEST_CMD_STOP_DATA_TRANS = 4,
	AOLTEST_CMD_MAX
};

struct netlink_event_cb {
	int (*aoltest_bind)(void);
	int (*aoltest_unbind)(void);
	int (*aoltest_handler)(int cmd, void *data);
};

int aoltest_netlink_init(struct netlink_event_cb *cb);
void aoltest_netlink_deinit(void);
int aoltest_netlink_send_to_native(char *tag, unsigned int msg_id, char *buf, unsigned int length);

#endif /*_AOLTEST_NETLINK_H_ */
