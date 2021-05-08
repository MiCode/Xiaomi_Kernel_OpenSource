/*
 * Copyright (C) 2018 MediaTek Inc.
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
#define TRACE_SYSTEM spm_events

#if !defined(_TRACE_MTK_SPM_EVENTS_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_MTK_SPM_EVENTS_H

#include <linux/tracepoint.h>

TRACE_EVENT(SPM__lp_ratio_0,
	TP_PROTO(unsigned int ratio, unsigned int ap_ratio,
		unsigned int md_ratio),
	TP_ARGS(ratio, ap_ratio, md_ratio),
	TP_STRUCT__entry(__field(unsigned int, ratio)
		__field(unsigned int, ap_ratio)
		__field(unsigned int, md_ratio)
	),
	TP_fast_assign(__entry->ratio = ratio;
		__entry->ap_ratio = ap_ratio;
		__entry->md_ratio = md_ratio;
	),
	TP_printk("%d, %d, %d",
		__entry->ratio, __entry->ap_ratio,
		__entry->md_ratio)
);

TRACE_EVENT(SPM__resource_req_0,
	TP_PROTO(unsigned int md, unsigned int conn, unsigned int scp,
		unsigned int adsp, unsigned int ufs, unsigned int disp,
		unsigned int apu, unsigned int spm),
	TP_ARGS(md, conn, scp, adsp, ufs, disp, apu, spm),
	TP_STRUCT__entry(__field(unsigned int, md)
		__field(unsigned int, conn)
		__field(unsigned int, scp)
		__field(unsigned int, adsp)
		__field(unsigned int, ufs)
		__field(unsigned int, disp)
		__field(unsigned int, apu)
		__field(unsigned int, spm)
	),
	TP_fast_assign(__entry->md = md;
		__entry->conn = conn;
		__entry->scp = scp;
		__entry->adsp = adsp;
		__entry->ufs = ufs;
		__entry->disp = disp;
		__entry->apu = apu;
		__entry->spm = spm;
	),
	TP_printk("%d, %d, %d, %d, %d, %d, %d, %d",
		__entry->md, __entry->conn,
		__entry->scp, __entry->adsp,
		__entry->ufs, __entry->disp,
		__entry->apu, __entry->spm)
);

TRACE_EVENT(Network__traffic_0,
	TP_PROTO(u64 rx_bytes, u64 rx_packets, u64 tx_bytes, u64 tx_packets),
	TP_ARGS(rx_bytes, rx_packets, tx_bytes, tx_packets),
	TP_STRUCT__entry(__field(u64, rx_bytes)
		__field(u64, rx_packets)
		__field(u64, tx_bytes)
		__field(u64, tx_packets)
	),
	TP_fast_assign(__entry->rx_bytes = rx_bytes;
		__entry->rx_packets = rx_packets;
		__entry->tx_bytes = tx_bytes;
		__entry->tx_packets = tx_packets;
	),
	TP_printk("%llu, %llu, %llu, %llu",
		__entry->rx_bytes, __entry->rx_packets,
		__entry->tx_bytes, __entry->tx_packets)
);

#endif /* _TRACE_MTK_SPM_EVENTS_H */

/* This part must be outside protection */
#undef TRACE_INCLUDE_PATH
#define TRACE_INCLUDE_PATH ./
#undef TRACE_INCLUDE_FILE
#define TRACE_INCLUDE_FILE mtk_spm_events
#include <trace/define_trace.h>

