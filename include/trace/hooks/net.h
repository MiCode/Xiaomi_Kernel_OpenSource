/* SPDX-License-Identifier: GPL-2.0 */

#undef TRACE_SYSTEM
#define TRACE_SYSTEM net
#define TRACE_INCLUDE_PATH trace/hooks

#if !defined(_TRACE_HOOK_NET_VH_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_HOOK_NET_VH_H
#include <trace/hooks/vendor_hooks.h>

#ifndef TCP_STATE_CHANGE_REASON_H
#define TCP_STATE_CHANGE_REASON_H
enum tcp_state_change_reason {
	TCP_STATE_CHANGE_REASON_NORMAL,
	TCP_STATE_CHANGE_REASON_SYN_RST,
	TCP_STATE_CHANGE_REASON_SYN_TIMEOUT,
	TCP_STATE_CHANGE_REASON_RETRANSMIT
};
#endif

struct packet_type;
struct list_head;
DECLARE_HOOK(android_vh_ptype_head,
	TP_PROTO(const struct packet_type *pt, struct list_head *vendor_pt),
	TP_ARGS(pt, vendor_pt));

struct sock;
struct msghdr;
struct sk_buff;
struct sockaddr_in6;
struct net_device;
DECLARE_RESTRICTED_HOOK(android_rvh_tcp_sendmsg,
	TP_PROTO(struct sock *sk, struct msghdr *msg, size_t len),
	TP_ARGS(sk, msg, len), 1);
DECLARE_RESTRICTED_HOOK(android_rvh_tcp_recvmsg,
	TP_PROTO(struct sock *sk, struct msghdr *msg, size_t len, int flags, int *addr_len),
	TP_ARGS(sk, msg, len, flags, addr_len), 1);
DECLARE_RESTRICTED_HOOK(android_rvh_udp_sendmsg,
	TP_PROTO(struct sock *sk, struct msghdr *msg, size_t len),
	TP_ARGS(sk, msg, len), 1);
DECLARE_RESTRICTED_HOOK(android_rvh_udp_recvmsg,
	TP_PROTO(struct sock *sk, struct msghdr *msg, size_t len, int flags, int *addr_len),
	TP_ARGS(sk, msg, len, flags, addr_len), 1);
DECLARE_RESTRICTED_HOOK(android_rvh_udpv6_sendmsg,
	TP_PROTO(struct sock *sk, struct msghdr *msg, size_t len),
	TP_ARGS(sk, msg, len), 1);
DECLARE_RESTRICTED_HOOK(android_rvh_udpv6_recvmsg,
	TP_PROTO(struct sock *sk, struct msghdr *msg, size_t len, int flags, int *addr_len),
	TP_ARGS(sk, msg, len, flags, addr_len), 1);
DECLARE_RESTRICTED_HOOK(android_rvh_tcp_select_window,
	TP_PROTO(struct sock *sk, u32 *new_win), TP_ARGS(sk, new_win), 1);
DECLARE_RESTRICTED_HOOK(android_rvh_inet_sock_create,
	TP_PROTO(struct sock *sk), TP_ARGS(sk), 1);
DECLARE_RESTRICTED_HOOK(android_rvh_inet_sock_release,
	TP_PROTO(struct sock *sk), TP_ARGS(sk), 1);
DECLARE_RESTRICTED_HOOK(android_rvh_bpf_skb_load_bytes,
	TP_PROTO(const struct sk_buff *skb, u32 offset, void *to, u32 len,
		int *handled, int *err),
	TP_ARGS(skb, offset, to, len, handled, err), 1);
DECLARE_RESTRICTED_HOOK(android_rvh_tcp_rcv_spurious_retrans,
	TP_PROTO(struct sock *sk), TP_ARGS(sk), 1);
DECLARE_HOOK(android_vh_tcp_sock_error,
	TP_PROTO(struct sock *sk), TP_ARGS(sk));
DECLARE_HOOK(android_vh_tcp_fastsyn,
	TP_PROTO(struct sock *sk), TP_ARGS(sk));
DECLARE_HOOK(android_vh_tcp_select_window,
	TP_PROTO(struct sock *sk, uint32_t *win), TP_ARGS(sk, win));
DECLARE_HOOK(android_vh_tcp_state_change,
	TP_PROTO(struct sock *sk, enum tcp_state_change_reason reason, int state),
	TP_ARGS(sk, reason, state));
DECLARE_HOOK(android_vh_tcp_update_rtt,
	TP_PROTO(struct sock *sk, long rtt), TP_ARGS(sk, rtt));
DECLARE_HOOK(android_vh_dc_send_copy,
	TP_PROTO(struct sk_buff *skb, struct net_device *dev), TP_ARGS(skb, dev));
DECLARE_HOOK(android_vh_dc_receive,
	TP_PROTO(struct sk_buff *skb, int *flag), TP_ARGS(skb, flag));
DECLARE_HOOK(android_vh_tcp_v4_connect,
	TP_PROTO(struct sock *sk, struct sockaddr *uaddr), TP_ARGS(sk, uaddr));
DECLARE_HOOK(android_vh_tcp_v6_connect,
	TP_PROTO(struct sock *sk, struct sockaddr *uaddr), TP_ARGS(sk, uaddr));
DECLARE_HOOK(android_vh_udp_v4_connect,
	TP_PROTO(struct sock *sk, __be32 daddr, __be16 dport, uint16_t family),
	TP_ARGS(sk, daddr, dport, family));
DECLARE_HOOK(android_vh_udp_v6_connect,
	TP_PROTO(struct sock *sk, struct sockaddr_in6 *sin6), TP_ARGS(sk, sin6));
DECLARE_HOOK(android_vh_inet_create,
	TP_PROTO(struct sock *sk, bool err), TP_ARGS(sk, err));
DECLARE_HOOK(android_vh_uplink_send_msg,
	TP_PROTO(struct sock *sk), TP_ARGS(sk));
DECLARE_HOOK(android_vh_sock_create,
	TP_PROTO(struct sock *sk), TP_ARGS(sk));
DECLARE_HOOK(android_vh_tcp_rtt_estimator,
	TP_PROTO(struct sock *sk, long mrtt_us), TP_ARGS(sk, mrtt_us));
DECLARE_HOOK(android_vh_udp_enqueue_schedule_skb,
	TP_PROTO(struct sock *sk, struct sk_buff *skb), TP_ARGS(sk, skb));
DECLARE_HOOK(android_vh_build_skb_around,
	TP_PROTO(struct sk_buff *skb), TP_ARGS(skb));
DECLARE_HOOK(android_vh_tcp_write_timeout_estab_retrans,
        TP_PROTO(struct sock *sk), TP_ARGS(sk));
DECLARE_HOOK(android_vh_tcp_connect,
	TP_PROTO(struct sk_buff *skb), TP_ARGS(skb));
DECLARE_HOOK(android_vh_sk_alloc,
        TP_PROTO(struct sock *sk), TP_ARGS(sk));
DECLARE_HOOK(android_vh_sk_free,
        TP_PROTO(struct sock *sk), TP_ARGS(sk));
DECLARE_HOOK(android_vh_sk_clone_lock,
	TP_PROTO(struct sock *nsk), TP_ARGS(nsk));
struct request_sock;
DECLARE_HOOK(android_vh_inet_csk_clone_lock,
	TP_PROTO(struct sock *newsk, const struct request_sock *req), TP_ARGS(newsk, req));
DECLARE_HOOK(android_vh_tcp_clean_rtx_queue,
	TP_PROTO(struct sock *sk, int flag, long seq_rtt_us),
	TP_ARGS(sk, flag, seq_rtt_us));
struct inet_connection_sock;
DECLARE_HOOK(android_vh_tcp_rcv_synack,
	TP_PROTO(struct inet_connection_sock *icsk), TP_ARGS(icsk));
DECLARE_HOOK(android_vh_udp_unicast_rcv_skb,
	TP_PROTO(struct sk_buff *skb, struct sock *sk),
	TP_ARGS(skb, sk));
DECLARE_HOOK(android_vh_udp6_unicast_rcv_skb,
	TP_PROTO(struct sk_buff *skb, struct sock *sk),
	TP_ARGS(skb, sk));
DECLARE_HOOK(android_vh_tcp_rcv_established_fast_path,
	TP_PROTO(struct sock *sk), TP_ARGS(sk));
DECLARE_HOOK(android_vh_tcp_rcv_established_slow_path,
	TP_PROTO(struct sock *sk), TP_ARGS(sk));
/* macro versions of hooks are no longer required */

#endif /* _TRACE_HOOK_NET_VH_H */
/* This part must be outside protection */
#include <trace/define_trace.h>
