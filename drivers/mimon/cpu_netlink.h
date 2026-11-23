// SPDX-License-Identifier: GPL-2.0
/*
 * cpu_netlink.h - CPU monitor netlink interface
 *
 * Copyright (c) 2026 Xiaomi, Inc., All rights reserved.
 */

#ifndef CPU_NETLINK_H
#define CPU_NETLINK_H

#include <linux/types.h>

enum sock_no {
	CPU_HIGH_LOAD	= 1,
	PROC_LOAD	= 5,
};

int cpu_netlink_init(void);
void cpu_netlink_exit(void);
int send_to_user(int sock_no, size_t len, const int *data);

#endif /* CPU_NETLINK_H */
