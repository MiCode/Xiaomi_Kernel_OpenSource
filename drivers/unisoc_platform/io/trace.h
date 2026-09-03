/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * limit task's buffer write in cgroup.
 *
 * Copyright 2023 Unisoc(Shanghai) Technologies Co.Ltd
 *
 * Licensed under the Unisoc General Software License, version 1.0 (the
 * License);
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 * https://www.unisoc.com/en_us/license/UNISOC_GENERAL_LICENSE_V1.0-EN_US
 * Software distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OF ANY KIND, either express or implied.
 * See the Unisoc General Software License, version 1.0 for more details.
 */

//This file has been modified by Unisoc (Tianjin) Technologies Co., Ltd in 2023.

#undef TRACE_SYSTEM
#define TRACE_SYSTEM unisoc_io

#if !defined(_TRACE_UNISOC_IO_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_UNISOC_IO_H

#include <linux/tracepoint.h>

TRACE_EVENT(iolimit_write_control,
	TP_PROTO(unsigned long delta),

	TP_ARGS(delta),

	TP_STRUCT__entry(
		__field(pid_t, tgid)
		__field(pid_t, pid)
		__array(char, comm, TASK_COMM_LEN)
		__field(unsigned long, delta)
	),

	TP_fast_assign(
		__entry->tgid = current->tgid;
		__entry->pid  = current->pid;
		memcpy(__entry->comm, current->comm, TASK_COMM_LEN);
		__entry->delta = delta * 1000 / HZ;
	),

	TP_printk("tgid:%d pid:%d comm=%s delta=%lu\n",
		__entry->tgid,
		__entry->pid,
		__entry->comm,
		__entry->delta
	)
);

#endif /* _TRACE_UNISOC_IO_H */

#undef TRACE_INCLUDE_PATH
#define TRACE_INCLUDE_PATH .
#define TRACE_INCLUDE_FILE trace

#include <trace/define_trace.h>
