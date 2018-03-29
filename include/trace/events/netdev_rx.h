/*
* Copyright (C) 2016 MediaTek Inc.
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License version 2 as
* published by the Free Software Foundation.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
* See http://www.gnu.org/licenses/gpl-2.0.html for more details.
*/

#undef TRACE_SYSTEM
#define TRACE_SYSTEM netdev_rx

#if !defined(_TRACE_NETDEV_RX_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_NETDEV_RX_H

#include <linux/tracepoint.h>

TRACE_EVENT(netd_skb_rx,

	TP_PROTO(unsigned long long *net_deta),

	TP_ARGS(net_deta),

	TP_STRUCT__entry(
		__array( unsigned long long,	net_deta, 8 )
	),

	TP_fast_assign(
		memcpy(__entry->net_deta, net_deta, 8*sizeof(unsigned long long));
	),

	TP_printk("	%llu	%04X	%llu	%llu	%llu	%llu	%llu	%llu", 
										   __entry->net_deta[0], (unsigned int)__entry->net_deta[1], __entry->net_deta[2],__entry->net_deta[3],
										   __entry->net_deta[4], __entry->net_deta[5],__entry->net_deta[6], __entry->net_deta[7])
);

TRACE_EVENT(rpsd_skb_rx,

	TP_PROTO(unsigned long long *dl_delay),

	TP_ARGS(dl_delay),

	TP_STRUCT__entry(
		__array(	unsigned long long,	dl_delay, 8)
	),

	TP_fast_assign(
		memcpy(__entry->dl_delay, dl_delay, 8*sizeof(unsigned long long));
	),

	TP_printk("	%llu	%04X	%llu	%llu	%llu	%llu	%llu	%llu", 
										__entry->dl_delay[0], (unsigned int)__entry->dl_delay[1], __entry->dl_delay[2], __entry->dl_delay[3],
										__entry->dl_delay[4], __entry->dl_delay[5], __entry->dl_delay[6], __entry->dl_delay[7])
);
#endif

/***** NOTICE! The #if protection ends here. *****/

/*
 * TRACE_INCLUDE_FILE is not needed if the filename and TRACE_SYSTEM are equal
 */

#include <trace/define_trace.h>
