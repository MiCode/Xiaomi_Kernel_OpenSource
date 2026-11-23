// SPDX-License-Identifier: GPL-2.0
/*
 * mimon.c - Xiaomi CPU monitor kernel module
 *
 * Copyright (c) 2026 Xiaomi, Inc., All rights reserved.
 */

#include <linux/module.h>
#include "cpu_load_monitor.h"
#include "cpu_netlink.h"

#define CREATE_TRACE_POINTS
#include "mimon_trace.h"

void mimon_trace_format(const char *fmt, ...)
{
	va_list args;
	char buf[MAX_TRACE_STR_LEN];

	if (trace_mimon_trace_enabled()) {
		va_start(args, fmt);
		vscnprintf(buf, MAX_TRACE_STR_LEN, fmt, args);
		va_end(args);
		trace_mimon_trace(buf);
	}
}

static int __init mimon_init(void)
{
	int ret;

	ret = cpu_netlink_init();
	if (ret) {
		pr_err("mimon: failed to init netlink\n");
		return ret;
	}

	ret = cpu_load_monitor_init();
	if (ret) {
		pr_err("mimon: failed to init load monitor\n");
		cpu_netlink_exit();
		return ret;
	}

	return 0;
}

static void __exit mimon_exit(void)
{
	cpu_load_monitor_exit();
	cpu_netlink_exit();
}

module_init(mimon_init);
module_exit(mimon_exit);

MODULE_LICENSE("GPL");
