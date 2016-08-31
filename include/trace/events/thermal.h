/*
 * Copyright (c) 2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#undef TRACE_SYSTEM
#define TRACE_SYSTEM thermal

#if !defined(_TRACE_THERMAL_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_THERMAL_H

#include <linux/string.h>
#include <linux/thermal.h>
#include <linux/tracepoint.h>

TRACE_EVENT(thermal_trip,

	    TP_PROTO(const char *type, int temp),

	    TP_ARGS(type, temp),

	    TP_STRUCT__entry(
			     __array(char, type, THERMAL_NAME_LENGTH)
			     __field(int, temp)
			     ),

	    TP_fast_assign(
			   memcpy(__entry->type, type, THERMAL_NAME_LENGTH);
			   __entry->temp = temp;
			   ),

	    TP_printk("%s = %d", __entry->type, __entry->temp)
	    );

TRACE_EVENT(cooling_device_update,

	    TP_PROTO(const char *type, long target),

	    TP_ARGS(type, target),

	    TP_STRUCT__entry(
			     __array(char, type, THERMAL_NAME_LENGTH)
			     __field(long, target)
			     ),

	    TP_fast_assign(
			   memcpy(__entry->type, type, THERMAL_NAME_LENGTH);
			   __entry->target = target;
			   ),

	    TP_printk("%s -> %ld", __entry->type, __entry->target)
	    );


#endif /* _TRACE_THERMAL_H */

/* This part must be outside protection */
#include <trace/define_trace.h>
