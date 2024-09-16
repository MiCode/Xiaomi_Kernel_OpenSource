/*  SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */
#ifndef _CONNDUMP_NETLINK_H_
#define _CONNDUMP_NETLINK_H_

#include <linux/types.h>
#include <linux/compiler.h>

#include "coredump/connsys_coredump.h"

/*******************************************************************************
*                                 M A C R O S
********************************************************************************
*/

/*******************************************************************************
*                             D A T A   T Y P E S
********************************************************************************
*/

struct netlink_event_cb {
	void (*coredump_end)(void*);
};

/*******************************************************************************
*                  F U N C T I O N   D E C L A R A T I O N S
********************************************************************************
*/
int conndump_netlink_init(int conn_type, void* dump_ctx, struct netlink_event_cb* cb);
int conndump_netlink_send_to_native(int conn_type, char* tag, char* buf, unsigned int length);


#endif /*_CONNDUMP_NETLINK_H_ */
