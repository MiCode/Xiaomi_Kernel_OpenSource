/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef LINUX_URSP_H__
#define LINUX_URSP_H__

/* includes */
#include <linux/ip.h>
#include <net/netfilter/nf_conntrack.h>
#include <linux/types.h>

#define DEFAULT_URSP_AGING_TIME		(20 * HZ)
#define DEFAULT_URSP_TCP_AGING_TIME	(DEFAULT_URSP_AGING_TIME * 6)
#define DEFAULT_URSP_UDP_AGING_TIME	(DEFAULT_URSP_AGING_TIME * 2)
#define URSP_TCP_ESTABLISHED_TIME	(5 * 24 * 3600 * HZ)
#define URSP_TCP_TIME_WAIT		(120 * HZ)
#define URSP_TCP_CLOSE_TIME		(10 * HZ)

int ursp_filter_fwd_create_entries(u8 pf, struct sk_buff *skb,
				   const struct nf_hook_state *s);
int ursp_ct_tcp_sure(u8 pf, struct sk_buff *skb, struct nf_conn *ct);
int nl_ursp_init(void);
void nl_ursp_exit(void);
#endif
