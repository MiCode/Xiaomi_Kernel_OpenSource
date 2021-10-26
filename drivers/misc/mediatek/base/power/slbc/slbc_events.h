/*
 * Copyright (C) 2017 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#undef TRACE_SYSTEM
#define TRACE_SYSTEM slbc_events

#if !defined(_TRACE_SLBC_EVENTS_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_SLBC_EVENTS_H

#include <linux/tracepoint.h>

TRACE_EVENT(slbc_api,
	TP_PROTO(char *_api_name,
		char *_name),
	TP_ARGS(_api_name,
		_name),
	TP_STRUCT__entry(
		__field(char*, _api_name)
		__field(char*, _name)
	),
	TP_fast_assign(
		__entry->_api_name = _api_name;
		__entry->_name = _name;
	),
	TP_printk("%s - %s",
		__entry->_api_name, __entry->_name)
);

TRACE_EVENT(slbc_data,
	TP_PROTO(char *_api_name,
		struct slbc_data *_data),
	TP_ARGS(_api_name,
		_data),
	TP_STRUCT__entry(
		__field(char*, _api_name)
		__field(struct slbc_data*, _data)
	),
	TP_fast_assign(
		__entry->_api_name = _api_name;
		__entry->_data = _data;
	),
	TP_printk("%s %d %x %ld %p %p %d %x %p %d %d",
		__entry->_api_name,
		__entry->_data->uid,
		__entry->_data->type,
		__entry->_data->size,
		__entry->_data->paddr,
		__entry->_data->vaddr,
		__entry->_data->sid,
		__entry->_data->slot_used,
		__entry->_data->config,
		__entry->_data->ref,
		__entry->_data->pwr_ref)
);

#endif /* _TRACE_SLBC_EVENTS_H */

/* This part must be outside protection */
#undef TRACE_INCLUDE_PATH
#define TRACE_INCLUDE_PATH ./
#undef TRACE_INCLUDE_FILE
#define TRACE_INCLUDE_FILE slbc_events
#include <trace/define_trace.h>
