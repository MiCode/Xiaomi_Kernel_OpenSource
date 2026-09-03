/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) 2019 Spreadtrum Communications Inc.
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
#ifndef _SLA_NET_STATS_NETLINK_H
#define _SLA_NET_STATS_NETLINK_H

#include <linux/types.h>
#include "sla.h"

#define NETLINK_SLA		31	//netilink unit for sla

#define SLA_NL_GRP_EVENT	0x00000001	//sla netlink group

enum {
	SLA_NL_NET_STATS_MSG = 0x1,		//report rtt >= 50ms and has  lasted 5s
	SLA_NL_NET_STATS_GOOD_RTT = 0x2,	//report  rtt < 100ms and has lasted 30s
	SLA_NL_NET_STATS_GOOD_RTT_2 = 0x3,	//report  rtt < 50ms and has lasted  60s
	SLA_NL_NET_STATS_ENABLE = 0x13,		//enable start measuring the rate and rtt
};

int sla_netlink_notify(unsigned long msgtype, void *data);
int sla_nl_init(void);
void sla_nl_exit(void);

#endif /*_SLA_NET_STATS_NETLINK_H*/

