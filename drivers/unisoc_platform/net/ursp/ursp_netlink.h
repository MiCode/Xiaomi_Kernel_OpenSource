/* SPDX-License-Identifier: GPL-2.0-only */
/* Copyright (C) 2019 Spreadtrum Communications Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/types.h>
#include "ursp.h"

#define NETLINK_ESTABLISH_REPORT 25

#define URSP_NL_GRP_EVENT 0x00000001

enum {
	ESTABLISH_EVENT = 0x0,
	CLOSE_EVENT = 0x1,
};

enum {
	URSP_NL_MSG_IPv4_TCP_ESTABLISH = 0x1,
	URSP_NL_MSG_IPv4_UDP_ESTABLISH,
	URSP_NL_MSG_IPv4_TCP_CLOSE,
	URSP_NL_MSG_IPv4_UDP_CLOSE,
	URSP_NL_MSG_IPv6_TCP_ESTABLISH,
	URSP_NL_MSG_IPv6_UDP_ESTABLISH,
	URSP_NL_MSG_IPv6_TCP_CLOSE,
	URSP_NL_MSG_IPv6_UDP_CLOSE,
};

struct nl_msg_report {
	union {
		__be32		addr;
		__be32		addr6[4];
	};
	__u16	port;
	__u8	protocol;
	__u32	uid;
	__u32	type;
};

int ursp_notifier_call(unsigned long val, void *v);
void ursp_notify(struct ursp_conn *ursp_ct,
		 unsigned long event);

