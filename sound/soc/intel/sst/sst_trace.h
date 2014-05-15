/*
 *  sst_trace.h - Intel SST Driver tracing support
 *
 *  Copyright (C) 2013	Intel Corp
 *  Authors: Omair Mohammed Abdullah <omair.m.abdullah@linux.intel.com>
 *  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; version 2 of the License.
 *
 *  This program is distributed in the hope that it will be useful, but
 *  WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  General Public License for more details.
 *
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 */

#undef TRACE_SYSTEM
#define TRACE_SYSTEM sst

#if !defined(_TRACE_SST_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_SST_H

#include <linux/types.h>
#include <linux/ktime.h>
#include <linux/tracepoint.h>

TRACE_EVENT(sst_ipc,

	TP_PROTO(const char *msg, u32 header_high, u32 header_low, int pvt_id),

	TP_ARGS(msg, header_high, header_low, pvt_id),

	TP_STRUCT__entry(
		__string(info_msg,	msg)
		__field(unsigned int,	val_l)
		__field(unsigned int,	val_h)
		__field(unsigned int,	id)
	),

	TP_fast_assign(
		__assign_str(info_msg, msg);
		__entry->val_l = header_low;
		__entry->val_h = header_high;
		__entry->id = pvt_id;
	),

	TP_printk("\t%s\t [%2u] = %#8.8x:%.4x", __get_str(info_msg),
		  (unsigned int)__entry->id,
		  (unsigned int)__entry->val_h, (unsigned int)__entry->val_l)

);

TRACE_EVENT(sst_stream,

	TP_PROTO(const char *msg, int str_id, int pipe_id),

	TP_ARGS(msg, str_id, pipe_id),

	TP_STRUCT__entry(
		__string(info_msg,	msg)
		__field(unsigned int,	str_id)
		__field(unsigned int,	pipe_id)
	),

	TP_fast_assign(
		__assign_str(info_msg, msg);
		__entry->str_id = str_id;
		__entry->pipe_id = pipe_id;
	),

	TP_printk("\t%s\t str  = %2u, pipe = %#x", __get_str(info_msg),
		  (unsigned int)__entry->str_id, (unsigned int)__entry->pipe_id)
);

TRACE_EVENT(sst_ipc_mailbox,

	TP_PROTO(const char *mailbox, int mbox_len),

	TP_ARGS(mailbox, mbox_len),

	TP_STRUCT__entry(
		__dynamic_array(char,	mbox,	(3 * mbox_len))
	),

	TP_fast_assign(
		sst_dump_to_buffer(mailbox, mbox_len,
				   __get_dynamic_array(mbox));
	),

	TP_printk("  %s", __get_str(mbox))

);

TRACE_EVENT(sst_lib_download,

	TP_PROTO(const char *msg, const char *lib_name),

	TP_ARGS(msg, lib_name),

	TP_STRUCT__entry(
		__string(info_msg, msg)
		__string(info_lib_name, lib_name)
	),

	TP_fast_assign(
		__assign_str(info_msg, msg);
		__assign_str(info_lib_name, lib_name);
	),

	TP_printk("\t%s %s", __get_str(info_msg),
			__get_str(info_lib_name))
);

TRACE_EVENT(sst_fw_download,

	TP_PROTO(const char *msg, int fw_state),

	TP_ARGS(msg, fw_state),

	TP_STRUCT__entry(
		__string(info_msg, msg)
		__field(unsigned int,   fw_state)
	),

	TP_fast_assign(
		__assign_str(info_msg, msg);
		__entry->fw_state = fw_state;
	),

	TP_printk("\t%s\tFW state = %d", __get_str(info_msg),
				(unsigned int)__entry->fw_state)
);

#endif /* _TRACE_SST_H */

/* This part must be outside protection */
#undef TRACE_INCLUDE_PATH
#define TRACE_INCLUDE_PATH .
#define TRACE_INCLUDE_FILE sst_trace
#include <trace/define_trace.h>
