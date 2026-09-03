// SPDX-License-Identifier: GPL-2.0-only
/*
 * smccall.h - Unisoc platform header
 *
 * Copyright 2022 Unisoc(Shanghai) Technologies Co.Ltd
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#undef TRACE_SYSTEM
#define TRACE_SYSTEM smccall

#if !defined(_TRACE_SMCCALL_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_SMCCALL_H

#include <linux/tracepoint.h>

DECLARE_EVENT_CLASS(smccall,
	TP_PROTO(ulong arg0, ulong arg1, ulong arg2, ulong arg3),
	TP_ARGS(arg0, arg1, arg2, arg3),

	TP_STRUCT__entry(
		__field(ulong, arg0)
		__field(ulong, arg1)
		__field(ulong, arg2)
		__field(ulong, arg3)
	),

	TP_fast_assign(
		__entry->arg0 = arg0;
		__entry->arg1 = arg1;
		__entry->arg2 = arg2;
		__entry->arg3 = arg3;
		),

	TP_printk("%#lx, %#lx, %#lx, %#lx",
		 __entry->arg0,
		 __entry->arg1,
		 __entry->arg2,
		 __entry->arg3
		 )
);

DEFINE_EVENT(smccall, smc_entry,
	TP_PROTO(ulong arg0, ulong arg1, ulong arg2, ulong arg3),
	TP_ARGS(arg0, arg1, arg2, arg3)
);

DEFINE_EVENT(smccall, smc_exit,
	TP_PROTO(ulong arg0, ulong arg1, ulong arg2, ulong arg3),
	TP_ARGS(arg0, arg1, arg2, arg3)
);

#endif /* _TRACE_SMCCALL_H */

/* This part must be outside protection */
#include <trace/define_trace.h>
