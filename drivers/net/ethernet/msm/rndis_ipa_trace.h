/* Copyright (c) 2015-2016, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#undef TRACE_SYSTEM
#define TRACE_SYSTEM rndis_ipa
#define TRACE_INCLUDE_FILE rndis_ipa_trace

#if !defined(_RNDIS_IPA_TRACE_H) || defined(TRACE_HEADER_MULTI_READ)
#define _RNDIS_IPA_TRACE_H

#include <linux/tracepoint.h>

TRACE_EVENT(
	rndis_netif_ni,

	TP_PROTO(unsigned long proto),

	TP_ARGS(proto),

	TP_STRUCT__entry(
		__field(unsigned long,	proto)
	),

	TP_fast_assign(
		__entry->proto = proto;
	),

	TP_printk("proto =%lu\n", __entry->proto)
);

TRACE_EVENT(
	rndis_tx_dp,

	TP_PROTO(unsigned long proto),

	TP_ARGS(proto),

	TP_STRUCT__entry(
		__field(unsigned long,	proto)
	),

	TP_fast_assign(
		__entry->proto = proto;
	),

	TP_printk("proto =%lu\n", __entry->proto)
);

TRACE_EVENT(
	rndis_status_rcvd,

	TP_PROTO(unsigned long proto),

	TP_ARGS(proto),

	TP_STRUCT__entry(
		__field(unsigned long,	proto)
	),

	TP_fast_assign(
		__entry->proto = proto;
	),

	TP_printk("proto =%lu\n", __entry->proto)
);

#endif /* _RNDIS_IPA_TRACE_H */

/* This part must be outside protection */
#undef TRACE_INCLUDE_PATH
#define TRACE_INCLUDE_PATH .
#include <trace/define_trace.h>
