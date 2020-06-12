/* SPDX-License-Identifier: GPL-2.0 */
#undef TRACE_SYSTEM
#define TRACE_SYSTEM udp

#if !defined(_TRACE_UDP_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_UDP_H

#include <linux/udp.h>
#include <linux/tracepoint.h>

TRACE_EVENT(udp_fail_queue_rcv_skb,

	TP_PROTO(int rc, struct sock *sk),

	TP_ARGS(rc, sk),

	TP_STRUCT__entry(
		__field(int, rc)
		__field(__u16, lport)
	),

	TP_fast_assign(
		__entry->rc = rc;
		__entry->lport = inet_sk(sk)->inet_num;
	),

	TP_printk("rc=%d port=%hu", __entry->rc, __entry->lport)
);

TRACE_EVENT(udpv4_fail_rcv_buf_errors,

	TP_PROTO(struct sk_buff *skb),

	TP_ARGS(skb),

	TP_STRUCT__entry(
		__field(void *, saddr)
		__field(void *, daddr)
		__field(__be16, sport)
		__field(__be16, dport)
	),

	TP_fast_assign(
		__entry->saddr = &ip_hdr(skb)->saddr;
		__entry->daddr = &ip_hdr(skb)->daddr;
		__entry->sport = ntohs(udp_hdr(skb)->source);
		__entry->dport = ntohs(udp_hdr(skb)->dest);
	),

	TP_printk("src %pI4:%u dst %pI4:%u", __entry->saddr,
		  __entry->sport, __entry->daddr, __entry->dport)
);

TRACE_EVENT(udpv6_fail_rcv_buf_errors,

	TP_PROTO(struct sk_buff *skb),

	TP_ARGS(skb),

	TP_STRUCT__entry(
		__field(void *, saddr)
		__field(void *, daddr)
		__field(__be16, sport)
		__field(__be16, dport)
	),

	TP_fast_assign(
		__entry->saddr = &ipv6_hdr(skb)->saddr;
		__entry->daddr = &ipv6_hdr(skb)->daddr;
		__entry->sport = ntohs(udp_hdr(skb)->source);
		__entry->dport = ntohs(udp_hdr(skb)->dest);
	),

	TP_printk("src %pI6:%u dst %pI6:%u", __entry->saddr,
		  __entry->sport, __entry->daddr, __entry->dport)
);

#endif /* _TRACE_UDP_H */

/* This part must be outside protection */
#include <trace/define_trace.h>
