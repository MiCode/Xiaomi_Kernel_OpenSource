/*
 * Support Camera Imaging tracer core.
 *
 * Copyright (c) 2013 Intel Corporation. All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License version
 * 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 *
 */
#undef TRACE_SYSTEM
#define TRACE_SYSTEM atomisp

#include <linux/tracepoint.h>
#include <linux/string.h>
TRACE_EVENT(camera_meminfo,

	TP_PROTO(const char *name, int uptr_size, int counter, int sys_size,
		int sys_res_size, int cam_sys_use, int cam_dyc_use,
		int cam_res_use),

	TP_ARGS(name, uptr_size, counter, sys_size, sys_res_size, cam_sys_use,
		cam_dyc_use, cam_res_use),

	TP_STRUCT__entry(
		__array(char, name, 24)
		__field(int, uptr_size)
		__field(int, counter)
		__field(int, sys_size)
		__field(int, sys_res_size)
		__field(int, cam_res_use)
		__field(int, cam_dyc_use)
		__field(int, cam_sys_use)
	),

	TP_fast_assign(
		strlcpy(__entry->name, name, 24);
		__entry->uptr_size = uptr_size;
		__entry->counter = counter;
		__entry->sys_size = sys_size;
		__entry->sys_res_size = sys_res_size;
		__entry->cam_res_use = cam_res_use;
		__entry->cam_dyc_use = cam_dyc_use;
		__entry->cam_sys_use = cam_sys_use;
	),

	TP_printk(
		"<%s> User ptr memory:%d pages,\tISP private memory used:%d"
		" pages:\tsysFP system size:%d,\treserved size:%d"
		"\tcamFP sysUse:%d,\tdycUse:%d,\tresUse:%d.\n",
		__entry->name, __entry->uptr_size, __entry->counter,
		__entry->sys_size, __entry->sys_res_size, __entry->cam_sys_use,
		__entry->cam_dyc_use, __entry->cam_res_use)
);

TRACE_EVENT(camera_debug,

	TP_PROTO(const char *name, char *info, const int line),

	TP_ARGS(name, info, line),

	TP_STRUCT__entry(
		__array(char, name, 24)
		__array(char, info, 24)
		__field(int, line)
	),

	TP_fast_assign(
		strlcpy(__entry->name, name, 24);
		strlcpy(__entry->info, info, 24);
		__entry->line = line;
	),

	TP_printk("<%s>-<%d> %s\n", __entry->name, __entry->line,
		__entry->info)
);

#undef TRACE_INCLUDE_PATH
#undef TRACE_INCLUDE_FILE
#define TRACE_INCLUDE_PATH .
#define TRACE_INCLUDE_FILE   atomisp_trace_event
/* This part must be outside protection */
#include <trace/define_trace.h>
