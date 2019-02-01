/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2015-2019, The Linux Foundation. All rights reserved.
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
#define TRACE_INCLUDE_PATH ../../drivers/platform/msm/ipa/ipa_clients
#include <trace/define_trace.h>
