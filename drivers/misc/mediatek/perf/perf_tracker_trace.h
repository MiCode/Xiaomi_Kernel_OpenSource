/*
 * Copyright (C) 2019 MediaTek Inc.
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


#if !defined(_PERF_TRACKER_TRACE_H) || defined(TRACE_HEADER_MULTI_READ)
#define _PERF_TRACKER_TRACE_H

#include <linux/tracepoint.h>

#undef TRACE_SYSTEM
#define TRACE_SYSTEM perf_tracker
#define TRACE_INCLUDE_FILE perf_tracker_trace

TRACE_EVENT(fuel_gauge,
	TP_PROTO(
		int cur,
		int volt
	),

	TP_ARGS(cur, volt),

	TP_STRUCT__entry(
		__field(int, cur)
		__field(int, volt)
	),

	TP_fast_assign(
		__entry->cur = cur;
		__entry->volt = volt;
	),

	TP_printk("cur=%d, vol=%d",
		__entry->cur,
		__entry->volt
	)
);

TRACE_EVENT(perf_index_gpu,
	TP_PROTO(u32 *gpu_data, u32 lens),
	TP_ARGS(gpu_data, lens),
	TP_STRUCT__entry(
		__dynamic_array(u32, gpu_data, lens)
		__field(u32, lens)
	),
	TP_fast_assign(
		memcpy(__get_dynamic_array(gpu_data), gpu_data,
			lens * sizeof(u32));
		__entry->lens = lens;
	),
	TP_printk("data=%s", __print_array(__get_dynamic_array(gpu_data),
		__entry->lens, sizeof(u32)))
);

TRACE_EVENT(perf_index_sbin,
	TP_PROTO(u32 *raw_data, u32 lens),
	TP_ARGS(raw_data, lens),
	TP_STRUCT__entry(
		__dynamic_array(u32, raw_data, lens)
		__field(u32, lens)
	),
	TP_fast_assign(
		memcpy(__get_dynamic_array(raw_data), raw_data,
			lens * sizeof(u32));
		__entry->lens = lens;
	),
	TP_printk("data=%s", __print_array(__get_dynamic_array(raw_data),
		__entry->lens, sizeof(u32)))
);

TRACE_EVENT(socket_packet,

	TP_PROTO(
		unsigned long sk_uid,
		int proto,
		int dport,
		int sport,
		int saddr,
		int len,
		int copied,
		int seq),

	TP_ARGS(sk_uid, proto, dport, sport, saddr, len, copied, seq),

	TP_STRUCT__entry(
		__field(unsigned long, sk_uid)
		__field(int, proto)
		__field(int, dport)
		__field(int, sport)
		__field(int, saddr)
		__field(int, len)
		__field(int, copied)
		__field(int, seq)
	),

	TP_fast_assign(
		__entry->sk_uid = sk_uid;
		__entry->proto = proto;
		__entry->dport = dport;
		__entry->sport = sport;
		__entry->saddr = saddr;
		__entry->len = len;
		__entry->copied = copied;
		__entry->seq = seq;
	),

	TP_printk(
	"uid=%ld proto=%d dport=%d sport=%d addr=0x%x len=%d seq=0x%x copied=%d",
		__entry->sk_uid, __entry->proto, __entry->dport,
		__entry->sport, __entry->saddr, __entry->len,
		__entry->seq, __entry->copied)
);

TRACE_EVENT(tcp_rtt,

	TP_PROTO(
		unsigned long uid,
		int dport,
		int sport,
		int family,
		int daddr,
		long rtt),

	TP_ARGS(uid, dport, sport, family, daddr, rtt),

	TP_STRUCT__entry(
		__field(unsigned long, uid)
		__field(int, dport)
		__field(int, sport)
		__field(int, family)
		__field(int, daddr)
		__field(long, rtt)
	),

	TP_fast_assign(
		__entry->uid = uid;
		__entry->dport = dport;
		__entry->sport = sport;
		__entry->family = family;
		__entry->daddr = daddr;
		__entry->rtt = rtt;
	),

	TP_printk("uid=%ld dport=%d sport=%d family=%d daddr=%x RTT=%lu",
		__entry->uid, __entry->dport, __entry->sport,
		__entry->family, __entry->daddr, __entry->rtt)
);

#endif /*_PERF_TRACKER_TRACE_H */

#undef TRACE_INCLUDE_PATH
#define TRACE_INCLUDE_PATH .
#undef TRACE_INCLUDE_FILE
#define TRACE_INCLUDE_FILE perf_tracker_trace
/* This part must be outside protection */
#include <trace/define_trace.h>
