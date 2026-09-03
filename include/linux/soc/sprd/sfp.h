/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef __SPRD_SFP_H__
#define __SPRD_SFP_H__

/* includes */
#include <linux/kernel.h>
#include <linux/timer.h>
#include <linux/ip.h>
#include <linux/types.h>
#include <linux/kern_levels.h>
#include <net/netfilter/nf_nat.h>
#include <net/netfilter/nf_conntrack.h>
#include <net/netfilter/nf_conntrack_tuple.h>
#include <linux/netdevice.h>

#define DEFAULT_SFP_AGING_TIME	(20 * HZ)
#define DEFAULT_SFP_TCP_AGING_TIME	(DEFAULT_SFP_AGING_TIME * 6)
#define SFP_TCP_ESTABLISHED_TIME  (5 * 24 * 3600 * HZ)
#define SFP_TCP_TIME_WAIT  (120 * HZ)
#define SFP_TCP_CLOSE_TIME  (10 * HZ)
#define DEFAULT_SFP_UDP_AGING_TIME	(DEFAULT_SFP_AGING_TIME * 2)

enum{
	SFP_INTERFACE_LTE = 0,
	SFP_INTERFACE_USB,
	SFP_INTERFACE_WIFI
};

void sfp_mgr_entries_hash_init(void);
int sfp_update_mgr_fwd_entries(struct sk_buff *skb,
			       struct nf_conntrack_tuple *tuple);
struct net_device *netdev_get_by_index(int ifindex);
int sfp_filter_mgr_fwd_create_entries(u8 pf, struct sk_buff *skb);
int sfp_mgr_fwd_ct_tcp_sure(struct nf_conn *ct);
int soft_fastpath_process(int in_if, void *data_header,
			  u16 *offset, int *plen, int *out_if);
int sfp_mgr_entry_ct_confirmed(struct nf_conntrack_tuple  *tuple);
#endif
