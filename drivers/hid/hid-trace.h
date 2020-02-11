/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2018-2020, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#if !defined(_HID_TRACE_H) || defined(TRACE_HEADER_MULTI_READ)
#define _HID_TRACE_H

#undef TRACE_SYSTEM
#define TRACE_SYSTEM hid
#undef TRACE_INCLUDE_PATH
#define TRACE_INCLUDE_PATH .
#undef TRACE_INCLUDE_FILE
#define TRACE_INCLUDE_FILE hid-trace

#include <linux/tracepoint.h>

TRACE_EVENT(qvr_recv_sensor,
	TP_PROTO(char *sensor, uint64_t ts, s32 x, s32 y, s32 z),
	TP_ARGS(sensor, ts, x, y, z),
	TP_STRUCT__entry(
		__field(char *, sensor)
		__field(uint64_t, ts)
		__field(int, x)
		__field(int, y)
		__field(int, z)
		),
	TP_fast_assign(
		__entry->sensor = sensor;
		__entry->ts = ts;
		__entry->x = x;
		__entry->y = y;
		__entry->z = z;
		),
	TP_printk(
		"%s - ts=%llu x=%d y=%d z=%d",
		__entry->sensor,
		__entry->ts,
		__entry->x,
		__entry->y,
		__entry->z
		)
	);

#endif /* _HID_TRACE_H */

/* This part must be outside protection */
#include <trace/define_trace.h>
