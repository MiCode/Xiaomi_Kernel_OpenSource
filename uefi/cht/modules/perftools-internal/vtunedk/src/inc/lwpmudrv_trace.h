/*
    Copyright (C) 2005-2014 Intel Corporation.  All Rights Reserved.

    This file is part of SEP Development Kit

    SEP Development Kit is free software; you can redistribute it
    and/or modify it under the terms of the GNU General Public License
    version 2 as published by the Free Software Foundation.

    SEP Development Kit is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with SEP Development Kit; if not, write to the Free Software
    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

    As a special exception, you may use this file as part of a free software
    library without restriction.  Specifically, if other files instantiate
    templates or use macros or inline functions from this file, or you compile
    this file and link it with other files to produce an executable, this
    file does not by itself cause the resulting executable to be covered by
    the GNU General Public License.  This exception does not however
    invalidate any other reasons why the executable file might be covered by
    the GNU General Public License.
*/


#undef TRACE_SYSTEM
#define TRACE_SYSTEM lwpmudrv

#if !defined(_LWPMUDRV_TRACE_H_) || defined(TRACE_HEADER_MULTI_READ)
#define _LWPMUDRV_TRACE_H_
#include <linux/tracepoint.h>

TRACE_EVENT(lwpmudrv_marker,

    TP_PROTO(const char *origin, unsigned long long timestamp),

    TP_ARGS(origin, timestamp),

    TP_STRUCT__entry(
        __string(            origin,  origin)
        __field( unsigned long long,  timestamp)
    ),

    TP_fast_assign(
        __assign_str(origin, origin);
        __entry->timestamp = timestamp;
    ),

    TP_printk("%s tsc=%llx", __get_str(origin), __entry->timestamp)
);

#endif

/* This part must be outside protection */
#undef  TRACE_INCLUDE_PATH
#define TRACE_INCLUDE_PATH .
#define TRACE_INCLUDE_FILE lwpmudrv_trace
#include <trace/define_trace.h>
