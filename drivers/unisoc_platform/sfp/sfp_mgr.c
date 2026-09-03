// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2019 Spreadtrum Communications Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/version.h>
#include <linux/netdevice.h>
#include <linux/inetdevice.h>
#include <linux/etherdevice.h>
#include <linux/skbuff.h>
#include <linux/ip.h>
#include <linux/in.h>
#include <linux/tcp.h>
#include <linux/icmp.h>
#include <linux/icmpv6.h>
#include <linux/device.h>
#include <net/ip6_route.h>
#include <net/route.h>
#include <net/addrconf.h>
#include <net/arp.h>
#include <linux/proc_fs.h>
#include <linux/netfilter/x_tables.h>
#include <linux/rculist.h>
#include <net/netfilter/nf_nat.h>
#include <net/netfilter/nf_conntrack.h>
#include <net/netfilter/nf_conntrack_tuple.h>
#include <net/netfilter/nf_conntrack_l4proto.h>
#include <net/netfilter/nf_conntrack_core.h>
#include <linux/netfilter/nf_conntrack_common.h>
#include <net/net_namespace.h>
#include <linux/timer.h>
#include <linux/skbuff.h>
#include <linux/spinlock.h>
#include <linux/string.h>
#include <linux/platform_device.h>
#include <linux/of_device.h>
#include "sfp.h"
#include "sfp_hash.h"

#define FTP_CTRL_PORT 21
#define DHCP_PORT 67
#define DNS_PORT 53

#if IS_ENABLED(CONFIG_UNISOC_SIPA_V3)
#define CHK_FWD_ENTRY_SIZE (sizeof(struct fwd_entry) != 120)
#else
#define CHK_FWD_ENTRY_SIZE (sizeof(struct fwd_entry) != 96)
#endif
#define CHK_HASH_TBL_SIZE (sizeof(struct hd_hash_tbl) != 8)

spinlock_t mgr_lock;/* Spinlock for sfp */
/* Entry in manager */
struct hlist_head mgr_fwd_entries[SFP_ENTRIES_HASH_SIZE];
static const char * const sfp_netdev[] = {
				    "wlan",
				    "usb",
				    "seth",
				    "sipa_usb",
				    "sipa_eth",
				    NULL
				  };

#define IPA_TERM_MAX 32

static char *ipa_netdev[IPA_TERM_MAX] = {
				    [1] = "sipa_usb",
				    [6] = "sipa_eth",
				    [25] = "usb",
				  };

static bool ipa_pamwifi_enable;

static void set_sfp_ipa_pamwifi_enable(bool enable)
{
	ipa_pamwifi_enable = enable;

	FP_PRT_DBG(FP_PRT_INFO, "set ipa_pamwifi_enable to %d\n", ipa_pamwifi_enable);

	if (ipa_pamwifi_enable)
		ipa_netdev[2] = "wlan";
}

#define IPA_BAN_MAX 4
static const char * const ipa_banned_netdev[IPA_BAN_MAX] = {
				    "wlan", "bt-pan",
				  };

static bool is_relevant_protocol(u8 proto, struct nf_conntrack_tuple *tuple)
{
	/* For now we only knw how to handle NAT rules for TCP/UDP protocols */
	/* So we disregard everything else */
	switch (proto) {
	case IP_L4_PROTO_TCP:
	case IP_L4_PROTO_UDP:
		return true;
	case IP_L4_PROTO_ICMP:
		if (tuple->dst.u.icmp.type == ICMP_ECHO ||
		    tuple->dst.u.icmp.type == ICMP_ECHOREPLY)
			return true;
		break;
	case IP_L4_PROTO_ICMP6:
		if (tuple->dst.u.icmp.type == ICMPV6_ECHO_REQUEST ||
		    tuple->dst.u.icmp.type == ICMPV6_ECHO_REPLY)
			return true;
		break;
	default:
		return false;
	}
	return false;
}

static bool sfp_prot_check(u8 proto, struct nf_conntrack_tuple *tuple)
{
	return is_relevant_protocol(proto, tuple);
}

/*
 * If the port need to be passed to linux,return 1.
 */
static bool is_filter_port(u8 l4proto, u16 src_port, u16 dst_port)
{
	/* We want to pass FTP control stream packets to Linux */
	if (l4proto == IPPROTO_TCP) {
		if (src_port == FTP_CTRL_PORT ||
		    dst_port == FTP_CTRL_PORT)
			return true;
	} else if (l4proto == IPPROTO_UDP) {
		if (src_port == DHCP_PORT ||
		    dst_port == DHCP_PORT)
			return true;
		else if ((src_port == DNS_PORT) ||
			 (dst_port == DNS_PORT))
			return true;
	}
	return false;
}

/* When sfp_ct is created, the default value of sfp_ct->used is 2,
 * because sfp_ct is associated with tuplehash[IP_CT_DIR_ORIGINAL]
 * and tuplehash[IP_CT_DIR_REPLY]. The value of sfp_ct->used is 1,
 * when we delete tuplehash[IP_CT_DIR_ORIGINAL], so it will delete
 * sfp_ct timer before free sfp_ct. When the value becomes 0, sfp_ct
 * will be freed.
 */
static void sfp_mgr_fwd_entry_free(struct rcu_head *head)
{
	struct sfp_mgr_fwd_tuple_hash *sfp_hash;
	struct sfp_conn *sfp_ct;

	sfp_hash = container_of(head, struct sfp_mgr_fwd_tuple_hash, rcu);
	sfp_ct = sfp_ct_tuplehash_to_ctrack(sfp_hash);
	if (sfp_ct && refcount_dec_and_test(&sfp_ct->used)) {
		FP_PRT_DBG(FP_PRT_WARN, "sfp_ct free %p\n", sfp_ct);
		kfree(sfp_ct);
	} else if (sfp_ct) {
		FP_PRT_DBG(FP_PRT_WARN, "del sfp_ct timer\n");
		del_timer_sync(&sfp_ct->timeout);
	}
}

static void sfp_mgr_fwd_death_by_timeout(struct timer_list *t)
{
	int cur_tether_scheme;
	struct sfp_conn *sfp_entry = from_timer(sfp_entry, t, timeout);
	//sfp_entry = (struct sfp_conn *)ul_fwd_entry_conn;
	struct sfp_mgr_fwd_tuple_hash *tuplehash_original;
	struct sfp_mgr_fwd_tuple_hash *tuplehash_reply;

	if (!sfp_entry) {
		FP_PRT_DBG(FP_PRT_ERR, "sfp_entry was free when time out.\n");
		return;
	}

	tuplehash_original = &sfp_entry->tuplehash[IP_CT_DIR_ORIGINAL];
	tuplehash_reply = &sfp_entry->tuplehash[IP_CT_DIR_REPLY];

	FP_PRT_DBG(FP_PRT_DEBUG, "SFP:<<check death by timeout(%p)>>.\n",
		   sfp_entry);

	cur_tether_scheme = get_sfp_tether_scheme();

	switch (cur_tether_scheme) {
	case SFP_HARD_PATH:
		spin_lock_bh(&fwd_tbl.sp_lock);
		if (!sfp_ipa_tbl_timeout(sfp_entry)) {
			spin_unlock_bh(&fwd_tbl.sp_lock);
			return;
		}

		FP_PRT_DBG(FP_PRT_DEBUG, "time out: delete ipa entry %p\n",
			   sfp_entry);
		sfp_ipa_fwd_delete(&sfp_entry->tuplehash[IP_CT_DIR_ORIGINAL],
				   sfp_entry->hash[IP_CT_DIR_ORIGINAL]);
		sfp_ipa_fwd_delete(&sfp_entry->tuplehash[IP_CT_DIR_REPLY],
				   sfp_entry->hash[IP_CT_DIR_REPLY]);
		spin_unlock_bh(&fwd_tbl.sp_lock);
		spin_lock_bh(&mgr_lock);
		break;
	case SFP_SOFT_PATH:
		spin_lock_bh(&mgr_lock);
		delete_in_sfp_fwd_table(&sfp_entry->tuplehash[IP_CT_DIR_ORIGINAL]);
		delete_in_sfp_fwd_table(&sfp_entry->tuplehash[IP_CT_DIR_REPLY]);
		break;
	case SFP_HALF_PATH:
		spin_lock_bh(&fwd_tbl.sp_lock);
		if (!sfp_ipa_tbl_timeout(sfp_entry)) {
			spin_unlock_bh(&fwd_tbl.sp_lock);
			return;
		}

		FP_PRT_DBG(FP_PRT_DEBUG, "time out: delete ipa entry %p\n",
			   sfp_entry);
		sfp_ipa_fwd_delete(&sfp_entry->tuplehash[IP_CT_DIR_ORIGINAL],
				   sfp_entry->hash[IP_CT_DIR_ORIGINAL]);
		sfp_ipa_fwd_delete(&sfp_entry->tuplehash[IP_CT_DIR_REPLY],
				   sfp_entry->hash[IP_CT_DIR_REPLY]);
		spin_unlock_bh(&fwd_tbl.sp_lock);
		spin_lock_bh(&mgr_lock);
		delete_in_sfp_fwd_table(&sfp_entry->tuplehash[IP_CT_DIR_ORIGINAL]);
		delete_in_sfp_fwd_table(&sfp_entry->tuplehash[IP_CT_DIR_REPLY]);
		break;
	};

	fp_prt_tuple_info(FP_PRT_DEBUG,
			   &sfp_entry->tuplehash[IP_CT_DIR_ORIGINAL].tuple);
	fp_prt_tuple_info(FP_PRT_DEBUG,
			   &sfp_entry->tuplehash[IP_CT_DIR_REPLY].tuple);

	rcu_read_lock_bh();

	if (tuplehash_original->entry_lst.pprev != LIST_POISON2) {
		hlist_del_rcu(&sfp_entry->tuplehash[IP_CT_DIR_ORIGINAL].entry_lst);
		call_rcu(&sfp_entry->tuplehash[IP_CT_DIR_ORIGINAL].rcu,
			 sfp_mgr_fwd_entry_free);
	}

	if (tuplehash_reply->entry_lst.pprev != LIST_POISON2) {
		hlist_del_rcu(&sfp_entry->tuplehash[IP_CT_DIR_REPLY].entry_lst);
		call_rcu(&sfp_entry->tuplehash[IP_CT_DIR_REPLY].rcu,
			 sfp_mgr_fwd_entry_free);
	}

	rcu_read_unlock_bh();
	spin_unlock_bh(&mgr_lock);
}

static int sfp_tuple_to_fwd_entries(struct nf_conntrack_tuple *tuple,
				    struct nf_conntrack_tuple *target_tuple,
				    struct sfp_mgr_fwd_tuple *fwd_entry)
{
	if (!tuple || !target_tuple || !fwd_entry)
		return 1;

	fwd_entry->orig_info.src_ip.all[0] = tuple->src.u3.all[0];
	fwd_entry->orig_info.src_ip.all[1] = tuple->src.u3.all[1];
	fwd_entry->orig_info.src_ip.all[2] = tuple->src.u3.all[2];
	fwd_entry->orig_info.src_ip.all[3] = tuple->src.u3.all[3];
	fwd_entry->orig_info.dst_ip.all[0]  = tuple->dst.u3.all[0];
	fwd_entry->orig_info.dst_ip.all[1]  = tuple->dst.u3.all[1];
	fwd_entry->orig_info.dst_ip.all[2]  = tuple->dst.u3.all[2];
	fwd_entry->orig_info.dst_ip.all[3]  = tuple->dst.u3.all[3];
	fwd_entry->orig_info.l3_proto = tuple->src.l3num;
	fwd_entry->trans_info.l3_proto = tuple->src.l3num;
	fwd_entry->orig_info.l4_proto = tuple->dst.protonum;
	fwd_entry->trans_info.l4_proto = tuple->dst.protonum;
	fwd_entry->orig_info.src_l4_info.all = tuple->src.u.all;
	fwd_entry->orig_info.dst_l4_info.all = tuple->dst.u.all;
	/*Transform*/
	fwd_entry->trans_info.src_ip.all[0] = target_tuple->dst.u3.all[0];
	fwd_entry->trans_info.src_ip.all[1] = target_tuple->dst.u3.all[1];
	fwd_entry->trans_info.src_ip.all[2] = target_tuple->dst.u3.all[2];
	fwd_entry->trans_info.src_ip.all[3] = target_tuple->dst.u3.all[3];
	fwd_entry->trans_info.dst_ip.all[0] = target_tuple->src.u3.all[0];
	fwd_entry->trans_info.dst_ip.all[1] = target_tuple->src.u3.all[1];
	fwd_entry->trans_info.dst_ip.all[2] = target_tuple->src.u3.all[2];
	fwd_entry->trans_info.dst_ip.all[3] = target_tuple->src.u3.all[3];

	fwd_entry->trans_info.src_l4_info.all = target_tuple->dst.u.all;
	fwd_entry->trans_info.dst_l4_info.all = target_tuple->src.u.all;

	return 0;
}

static int sfp_get_ip_version(struct sk_buff *skb)
{
	struct iphdr *iphdr = ip_hdr(skb);

	if (iphdr->version == 0x04)
		return 1;
	else
		return 0;
}

static struct neighbour *
sfp_dst_get_neighbour(struct dst_entry *dst, void *daddr, int is_v4)
{
	struct neighbour *neigh;
	const struct in6_addr *nexthop6;
	u32 nexthop4;

	if (is_v4) {
		nexthop4 =
			(__force u32)rt_nexthop((struct rtable *)dst,
			*(__be32 *)daddr);
		neigh = __ipv4_neigh_lookup_noref(dst->dev, nexthop4);
	} else {
		nexthop6 =
			rt6_nexthop((struct rt6_info *)dst,
				    (struct in6_addr *)daddr);
		neigh = __ipv6_neigh_lookup_noref(dst->dev, nexthop6);
	}
	if (neigh)
		neigh_hold(neigh);

	return neigh;
}

static bool sfp_get_mac_by_ipaddr(union nf_inet_addr *addr,
				  u8 *mac_addr, int is_v4)
{
	struct neighbour *neigh;
	struct rtable *rt;
	struct rt6_info *rt6;
	struct dst_entry *dst;
	struct net_device *mac_dev;

	/*
	 * Look up the rtable entry for the IP address then get the hardware
	 * address from its neighbour structure.  This means this work when the
	 * neighbours are routers too.
	 */
	if (likely(is_v4)) {
		rt = ip_route_output(&init_net, addr->ip, 0, 0, 0);
		if (IS_ERR(rt))
			goto ret_fail;

		dst = (struct dst_entry *)rt;
	} else {
		rt6 = rt6_lookup(&init_net, &addr->in6, NULL, 0, NULL, 0);
		if (!rt6)
			goto ret_fail;

		dst = (struct dst_entry *)rt6;
	}

	rcu_read_lock();
	neigh = sfp_dst_get_neighbour(dst, addr, is_v4);
	if (unlikely(!neigh)) {
		rcu_read_unlock();
		dst_release(dst);
		goto ret_fail;
	}

	if (unlikely(!(neigh->nud_state & NUD_VALID))) {
		rcu_read_unlock();
		neigh_release(neigh);
		dst_release(dst);
		goto ret_fail;
	}

	mac_dev = neigh->dev;
	if (!mac_dev) {
		rcu_read_unlock();
		neigh_release(neigh);
		dst_release(dst);
		goto ret_fail;
	}

	memcpy(mac_addr, neigh->ha, (size_t)mac_dev->addr_len);

	rcu_read_unlock();
	neigh_release(neigh);
	dst_release(dst);

	return true;

ret_fail:
	if (is_v4) {
		FP_PRT_DBG(FP_PRT_DEBUG,
			   "failed to find MAC address for IP: %pI4\n",
			   &addr->ip);
	} else {
		FP_PRT_DBG(FP_PRT_DEBUG,
			   "failed to find MAC address for IP: %pI6\n",
			   addr->ip6);
	}

	return false;
}

static bool sfp_ct_mac_init(struct sk_buff *skb,
			    struct nf_conn *ct,
			    struct sfp_conn *sfp_ct)
{
	enum ip_conntrack_info ctinfo;
	int dir;
	u8 orig_src[ETH_ALEN];
	u8 orig_dst[ETH_ALEN];
	u8 reply_src[ETH_ALEN];
	u8 reply_dst[ETH_ALEN];
	u8 *in_src, *in_dst, *out_src, *out_dst;
	char *skb_mac = skb_mac_header(skb);
	int is_v4 = sfp_get_ip_version(skb);
	char *def_mac = "000000";
	struct rtable *rt = skb_rtable(skb);
	struct sfp_mgr_fwd_tuple_hash *thptr;

	nf_ct_get(skb, &ctinfo);
	dir = CTINFO2DIR(ctinfo);

	if (dir == IP_CT_DIR_REPLY) {
		in_src = reply_src;
		in_dst = reply_dst;
		out_src = orig_src;
		out_dst = orig_dst;
	} else {
		in_src = orig_src;
		in_dst = orig_dst;
		out_src = reply_src;
		out_dst = reply_dst;
	}

	mac_addr_copy(in_src, skb_mac + 6);
	mac_addr_copy(in_dst, skb_mac);

	if (rt->dst.dev->flags & IFF_NOARP) {
		FP_PRT_DBG(FP_PRT_DEBUG, "noarp iface\n");
		mac_addr_copy(out_dst, rt->dst.dev->dev_addr);
		mac_addr_copy(out_src, (u8 *)def_mac);
	} else {
		if (!sfp_get_mac_by_ipaddr(&ct->tuplehash[!dir].tuple.src.u3,
					   out_src, is_v4)) {
			FP_PRT_DBG(FP_PRT_WARN, "get orig_src_mac fail\n");
			return false;
		}

		mac_addr_copy(out_dst, rt->dst.dev->dev_addr);
	}

	FP_PRT_DBG(FP_PRT_DEBUG,
		   "orig src mac: %02x.%02x.%02x.%02x.%02x.%02x\n",
		   orig_src[0], orig_src[1], orig_src[2],
		   orig_src[3], orig_src[4], orig_src[5]);
	FP_PRT_DBG(FP_PRT_DEBUG,
		   "orig dst mac: %02x.%02x.%02x.%02x.%02x.%02x\n",
		   orig_dst[0], orig_dst[1], orig_dst[2],
		   orig_dst[3], orig_dst[4], orig_dst[5]);
	FP_PRT_DBG(FP_PRT_DEBUG,
		   "reply src mac: %02x.%02x.%02x.%02x.%02x.%02x\n",
		   reply_src[0], reply_src[1], reply_src[2],
		   reply_src[3], reply_src[4], reply_src[5]);
	FP_PRT_DBG(FP_PRT_DEBUG,
		   "reply dst mac: %02x.%02x.%02x.%02x.%02x.%02x\n",
		   reply_dst[0], reply_dst[1], reply_dst[2],
		   reply_dst[3], reply_dst[4], reply_dst[5]);

	thptr = &sfp_ct->tuplehash[IP_CT_DIR_ORIGINAL];
	mac_addr_copy(thptr->ssfp_fwd_tuple.orig_mac_info.src_mac,
		      orig_src);
	mac_addr_copy(thptr->ssfp_fwd_tuple.orig_mac_info.dst_mac,
		      orig_dst);
	mac_addr_copy(thptr->ssfp_fwd_tuple.trans_mac_info.src_mac,
		      reply_dst);
	mac_addr_copy(thptr->ssfp_fwd_tuple.trans_mac_info.dst_mac,
		      reply_src);

	thptr = &sfp_ct->tuplehash[IP_CT_DIR_REPLY];
	mac_addr_copy(thptr->ssfp_fwd_tuple.orig_mac_info.src_mac,
		      reply_src);
	mac_addr_copy(thptr->ssfp_fwd_tuple.orig_mac_info.dst_mac,
		      reply_dst);
	mac_addr_copy(thptr->ssfp_fwd_tuple.trans_mac_info.src_mac,
		      orig_dst);
	mac_addr_copy(thptr->ssfp_fwd_tuple.trans_mac_info.dst_mac,
		      orig_src);

	return true;
}

int sfp_ct_init(struct nf_conn *ct, struct sfp_conn *sfp_ct)
{
	struct nf_conntrack_tuple *tuple1;
	struct nf_conntrack_tuple *tuple2;
	struct nf_conntrack_tuple *tuple;
	struct sfp_mgr_fwd_tuple *fwd_tuple1;
	struct sfp_mgr_fwd_tuple *fwd_tuple2;

	tuple = &ct->tuplehash[IP_CT_DIR_ORIGINAL].tuple;

	rcu_read_lock_bh();
	sfp_ct->tuplehash[IP_CT_DIR_ORIGINAL].tuple =
		ct->tuplehash[IP_CT_DIR_ORIGINAL].tuple;
	sfp_ct->tuplehash[IP_CT_DIR_REPLY].tuple =
		ct->tuplehash[IP_CT_DIR_REPLY].tuple;

	tuple1 = &sfp_ct->tuplehash[IP_CT_DIR_ORIGINAL].tuple;
	tuple2 = &sfp_ct->tuplehash[IP_CT_DIR_REPLY].tuple;

	fwd_tuple1 = &sfp_ct->tuplehash[IP_CT_DIR_ORIGINAL].ssfp_fwd_tuple;
	sfp_tuple_to_fwd_entries(tuple1, tuple2, fwd_tuple1);
	fwd_tuple2 = &sfp_ct->tuplehash[IP_CT_DIR_REPLY].ssfp_fwd_tuple;
	sfp_tuple_to_fwd_entries(tuple2, tuple1, fwd_tuple2);

	timer_setup(&sfp_ct->timeout,
		    sfp_mgr_fwd_death_by_timeout,
		    0);

	if (tuple1->dst.protonum == IP_L4_PROTO_TCP)
		sfp_ct->timeout.expires = jiffies + sysctl_tcp_aging_time;
	else
		sfp_ct->timeout.expires = jiffies + sysctl_udp_aging_time;

	refcount_set(&sfp_ct->used, 2);
	sfp_ct->sfp_status |= SFP_CT_FLAG_WHOLE;
	sfp_ct->insert_flag = 1;
	add_timer(&sfp_ct->timeout);

	rcu_read_unlock_bh();

	fp_prt_tuple_info(FP_PRT_WARN,
			   &sfp_ct->tuplehash[IP_CT_DIR_ORIGINAL].tuple);
	fp_prt_tuple_info(FP_PRT_WARN,
			   &sfp_ct->tuplehash[IP_CT_DIR_REPLY].tuple);

	return 0;
}

static bool sfp_check_filter_port(struct nf_conntrack_tuple *tuple, u8 l4proto)
{
	u16 src_port = 0;
	u16 dst_port = 0;

	if (l4proto == IPPROTO_TCP) {
		src_port = ntohs(tuple->src.u.tcp.port);
		dst_port = ntohs(tuple->dst.u.tcp.port);
	} else if (l4proto == IPPROTO_UDP) {
		src_port = ntohs(tuple->src.u.udp.port);
		dst_port = ntohs(tuple->dst.u.udp.port);
	}

	return is_filter_port(l4proto, src_port, dst_port);
}

static int create_mgr_fwd_entries_in_forward(struct sk_buff *skb,
					     struct nf_conn *ct,
					     int in_ifindex,
					     int out_ifindex,
					     int in_ipaifindex,
					     int out_ipaifindex)
{
	struct sfp_conn *new_sfp_ct;
	struct sfp_mgr_fwd_tuple_hash *tuple_hash;
	struct nf_conntrack_tuple *tuple =
		&ct->tuplehash[IP_CT_DIR_REPLY].tuple;
	struct sfp_mgr_fwd_tuple_hash *orig_hashtuple;
	struct sfp_mgr_fwd_tuple_hash *reply_hashtuple;
	u8 l4proto = tuple->dst.protonum;
	u32 hash, hash_inv;

	/* Check whether is created */
	rcu_read_lock_bh();
	hash = sfp_hash_conntrack(tuple);
	hlist_for_each_entry_rcu(tuple_hash,
				 &mgr_fwd_entries[hash],
				 entry_lst) {
		if (sfp_ct_tuple_equal(tuple_hash, tuple)) {
			FP_PRT_DBG(FP_PRT_DEBUG, "tuple hash exist, ignore.\n");
			rcu_read_unlock_bh();
			return 1;
		}
	}

	new_sfp_ct = kmalloc(sizeof(*new_sfp_ct), GFP_ATOMIC);

	if (!new_sfp_ct) {
		rcu_read_unlock_bh();
		return -ENOMEM;
	}

	FP_PRT_DBG(FP_PRT_DEBUG, "new sfp struct (%p)\n", new_sfp_ct);
	memset(new_sfp_ct, 0, sizeof(struct sfp_conn));

	if (!sfp_ct_mac_init(skb, ct, new_sfp_ct)) {
		FP_PRT_DBG(FP_PRT_ERR,
			   "sfp init mac fail, delete (%p)\n", new_sfp_ct);
		kfree(new_sfp_ct);
		rcu_read_unlock_bh();
		return 1;
	}

	sfp_ct_init(ct, new_sfp_ct);
	/* Out net device id */
	tuple_hash = &new_sfp_ct->tuplehash[IP_CT_DIR_ORIGINAL];
	tuple_hash->ssfp_fwd_tuple.in_ifindex = in_ifindex;
	tuple_hash->ssfp_fwd_tuple.out_ifindex = out_ifindex;
	tuple_hash->ssfp_fwd_tuple.in_ipaifindex = in_ipaifindex;
	tuple_hash->ssfp_fwd_tuple.out_ipaifindex = out_ipaifindex;

	tuple_hash = &new_sfp_ct->tuplehash[IP_CT_DIR_REPLY];
	tuple_hash->ssfp_fwd_tuple.in_ifindex = out_ifindex;
	tuple_hash->ssfp_fwd_tuple.out_ifindex = in_ifindex;
	tuple_hash->ssfp_fwd_tuple.in_ipaifindex = out_ipaifindex;
	tuple_hash->ssfp_fwd_tuple.out_ipaifindex = in_ipaifindex;

	orig_hashtuple = &new_sfp_ct->tuplehash[IP_CT_DIR_ORIGINAL];
	reply_hashtuple = &new_sfp_ct->tuplehash[IP_CT_DIR_REPLY];

	hash = sfp_hash_conntrack(&orig_hashtuple->tuple);
	new_sfp_ct->hash[IP_CT_DIR_ORIGINAL] = hash;
	hlist_add_head_rcu(&orig_hashtuple->entry_lst,
			   &mgr_fwd_entries[hash]);

	hash_inv = sfp_hash_conntrack(&reply_hashtuple->tuple);
	new_sfp_ct->hash[IP_CT_DIR_REPLY] = hash_inv;
	hlist_add_head_rcu(&reply_hashtuple->entry_lst,
			   &mgr_fwd_entries[hash_inv]);

	FP_PRT_DBG(FP_PRT_DEBUG,
		   "add mgr tbl, orig_hash[%u], reply_hash[%u]\n",
		   hash, hash_inv);
	rcu_read_unlock_bh();

	if (l4proto == IP_L4_PROTO_UDP ||
	    l4proto == IP_L4_PROTO_ICMP || l4proto == IP_L4_PROTO_ICMP6) {
		int cur_tether_scheme = get_sfp_tether_scheme();

		switch (cur_tether_scheme) {
		case SFP_HARD_PATH:
			sfp_ipa_hash_add(new_sfp_ct);
			break;
		case SFP_SOFT_PATH:
			sfp_fwd_hash_add(new_sfp_ct);
			break;
		case SFP_HALF_PATH:
			sfp_ipa_hash_add(new_sfp_ct);
			sfp_fwd_hash_add(new_sfp_ct);
			break;
		};
	}

	if (l4proto == IP_L4_PROTO_TCP) {
		ct->proto.tcp.seen[0].flags |= IP_CT_TCP_FLAG_BE_LIBERAL;
		ct->proto.tcp.seen[1].flags |= IP_CT_TCP_FLAG_BE_LIBERAL;
	}

	return 0;
}

static int get_hw_iface_by_dev(struct net_device *dev)
{
	int i;

	if (dev->name[0] == '\0') {
		FP_PRT_DBG(FP_PRT_ERR, "Invalid dev name!\n");
		return 0;
	}

	dev_hold(dev);

	for (i = 0; i < IPA_TERM_MAX; i++) {
		if (ipa_netdev[i] &&
		    strncasecmp(dev->name, ipa_netdev[i],
				strlen(ipa_netdev[i])) == 0) {
			dev_put(dev);
			return i;
		}
	}
	dev_put(dev);

	return 0;
}

bool is_banned_ipa_netdev(struct net_device *dev)
{
	int i;

	dev_hold(dev);
	for (i = 0; i < IPA_BAN_MAX; i++) {
		if (ipa_banned_netdev[i] &&
		    strncasecmp(dev->name, ipa_banned_netdev[i],
				strlen(ipa_banned_netdev[i])) == 0) {
			dev_put(dev);
			return true;
		}
	}
	dev_put(dev);

	return false;
}

static bool sfp_clatd_dev_check(struct net_device *dev)
{
	if (strncasecmp(dev->name, "v4-", 3) == 0)
		return true;

	return false;
}

/*
 * Hook 1: in FORWARD
 * create a new mgr forward entry
 */
int sfp_filter_mgr_fwd_create_entries(u8 pf, struct sk_buff *skb)
{
	enum ip_conntrack_info ctinfo;
	struct net *net;
	struct nf_conn *ct;
	struct rtable *rt;
	struct nf_conntrack_tuple *tuple;
	int in_ifindex;
	int out_ifindex;
	int out_ipaifindex, in_ipaifindex;
	u8  l4proto;
	int dir;
	int cur_tether_scheme;

	if (!get_sfp_enable())
		return 0;

	ct = nf_ct_get(skb, &ctinfo);
	if (!ct)
		return 0;

	if (!test_bit(IPS_CONFIRMED_BIT, &ct->status))
		return 0;

	tuple = &ct->tuplehash[IP_CT_DIR_ORIGINAL].tuple;

	/*
	 * we may fail to get l4proto from a fragment pkt,
	 * we better pass frags to the Network.
	 */
	l4proto = tuple->dst.protonum;
	dir = CTINFO2DIR(ctinfo);

	if (l4proto == IPPROTO_TCP && sfp_tcp_flag_chk(ct))
		return 0;

	net = nf_ct_net(ct);
	rt = skb_rtable(skb);

	/* wifi/bt-pan does not support IPA due to their hardware drawback */
	cur_tether_scheme = get_sfp_tether_scheme();
	if (!(cur_tether_scheme == SFP_SOFT_PATH)) {
		if ((is_banned_ipa_netdev(skb->dev) ||
		     is_banned_ipa_netdev(rt->dst.dev)) &&
		     !ipa_pamwifi_enable) {
			FP_PRT_DBG(FP_PRT_DEBUG,
				   "dev is filtered, won't create sfp\n");
			return 0;
		}
	}

	if (dir == IP_CT_DIR_ORIGINAL) {
		if (l4proto == IPPROTO_TCP &&
		    test_bit(IPS_ASSURED_BIT, &ct->status))
			sfp_mgr_fwd_ct_tcp_sure(ct);

		if (l4proto != IPPROTO_UDP)
			return 0;
	}

	net = nf_ct_net(ct);
	rt = skb_rtable(skb);

	if (!net || !rt) {
		FP_PRT_DBG(FP_PRT_DEBUG,
			   "invalid net or rt, won't create sfp\n");
		return 0;
	}

	if (sfp_clatd_dev_check(rt->dst.dev)) {
		FP_PRT_DBG(FP_PRT_DEBUG,
			   "clatd check failed, won't create sfp\n");
		return 0;
	}

	if (!sfp_prot_check(l4proto, tuple)) {
		FP_PRT_DBG(FP_PRT_DEBUG,
			   "protocol not supported, won't create sfp\n");
		return 0;
	}

	if (sfp_check_filter_port(tuple, l4proto)) {
		FP_PRT_DBG(FP_PRT_DEBUG,
			   "port filted, won't create sfp\n");
		return 0;
	}

	if (dir == IP_CT_DIR_REPLY) {
		out_ifindex = skb->dev->ifindex;
		in_ifindex = rt->dst.dev->ifindex;
		out_ipaifindex = get_hw_iface_by_dev(skb->dev);
		in_ipaifindex = get_hw_iface_by_dev(rt->dst.dev);
	} else {
		in_ifindex = skb->dev->ifindex;
		out_ifindex = rt->dst.dev->ifindex;
		in_ipaifindex = get_hw_iface_by_dev(skb->dev);
		out_ipaifindex = get_hw_iface_by_dev(rt->dst.dev);
	}

	FP_PRT_DBG(FP_PRT_DEBUG, "iface info [%s %d, %s %d] [%d, %d], dir %d\n",
		   rt->dst.dev->name, in_ifindex, skb->dev->name,
		   out_ifindex, in_ipaifindex, out_ipaifindex, dir);
	create_mgr_fwd_entries_in_forward(skb, ct,
					  in_ifindex, out_ifindex,
					  in_ipaifindex, out_ipaifindex);

	return 0;
}

/*
 * In PREROUTING hook.
 * 1. confirm the udp fwd entry
 * 2. using the skb to update the entries in mac info.
 */
int sfp_update_mgr_fwd_entries(struct sk_buff *skb,
			       struct nf_conntrack_tuple *tuple)
{
	u32 hash;
	struct sfp_mgr_fwd_tuple_hash *curr_fwd_hash;
	struct sfp_mgr_fwd_tuple_hash *target_hash;
	struct sfp_mgr_fwd_tuple *f_tuple;
	struct sfp_conn *sfp_ct;
	u8 macaddrs[12];
	u8 *dstmac;
	u8 *srcmac;
	u8 dir;

	if (!get_sfp_enable())
		return 0;

	if (!is_relevant_protocol(tuple->dst.protonum, tuple))
		return 2;

	hash = sfp_hash_conntrack(tuple);
	rcu_read_lock_bh();
	hlist_for_each_entry_rcu(curr_fwd_hash,
				 &mgr_fwd_entries[hash],
				 entry_lst) {
		if (sfp_ct_tuple_equal(curr_fwd_hash, tuple)) {
			sfp_ct = sfp_ct_tuplehash_to_ctrack(curr_fwd_hash);
			dir = curr_fwd_hash->tuple.dst.dir;
			f_tuple = &sfp_ct->tuplehash[dir].ssfp_fwd_tuple;
			target_hash = &sfp_ct->tuplehash[!dir];
			if (skb->mac_header) {
				memcpy(macaddrs, skb_mac_header(skb), 12);
				dir = curr_fwd_hash->tuple.dst.dir;
				dstmac = f_tuple->orig_mac_info.dst_mac;
				mac_addr_copy(dstmac, macaddrs);
				srcmac = f_tuple->orig_mac_info.src_mac;
				mac_addr_copy(srcmac, macaddrs + 6);
				dir = !dir;
				dstmac = f_tuple->trans_mac_info.dst_mac;
				mac_addr_copy(dstmac, macaddrs + 6);
				srcmac = f_tuple->trans_mac_info.src_mac;
				mac_addr_copy(srcmac, macaddrs);
			}
			sfp_ct->sfp_status |= SFP_CT_FLAG_PREROUTING;
			if (sfp_ct->sfp_status == SFP_CT_FLAG_WHOLE) {
				sfp_tuple_to_fwd_entries(&curr_fwd_hash->tuple,
							 &target_hash->tuple,
							 &curr_fwd_hash->ssfp_fwd_tuple);
				sfp_tuple_to_fwd_entries(&target_hash->tuple,
							 &curr_fwd_hash->tuple,
							 &target_hash->ssfp_fwd_tuple);
				FP_PRT_DBG(FP_PRT_DEBUG,
					   "PREROUTING:sfp need update mac and transform info[%u]\n",
					    hash);
				FP_PRT_DBG(FP_PRT_DEBUG, "---ORIGINAL---\n");
				fp_prt_tuple_info(FP_PRT_DEBUG,
						   &sfp_ct->tuplehash[IP_CT_DIR_ORIGINAL].tuple);
				FP_PRT_DBG(FP_PRT_DEBUG, "---NEW REPLY---\n");
				fp_prt_tuple_info(FP_PRT_DEBUG,
						   &sfp_ct->tuplehash[IP_CT_DIR_REPLY].tuple);
				FP_PRT_DBG(FP_PRT_DEBUG, " OUT-index = %d\n",
					   curr_fwd_hash->ssfp_fwd_tuple.out_ifindex);
				FP_PRT_DBG(FP_PRT_DEBUG,
					   "PREROUTING:the sfp [%u] has whole info.\n",
					   hash);
				/*Add udp ct to fwd table*/
				if (curr_fwd_hash->tuple.dst.protonum ==
					IP_L4_PROTO_UDP ||
					curr_fwd_hash->tuple.dst.protonum ==
					IP_L4_PROTO_ICMP ||
					curr_fwd_hash->tuple.dst.protonum ==
					IP_L4_PROTO_ICMP6) {
					add_in_sfp_fwd_table(curr_fwd_hash,
							     sfp_ct);
					add_in_sfp_fwd_table(target_hash,
							     sfp_ct);
				}
			}
			break;
		}
	}
	rcu_read_unlock_bh();
	return 0;
}

/*
 * For tcp connection, after three-way handshake,
 * the ct is sure,and the sfp also need sure.
 */
int sfp_mgr_fwd_ct_tcp_sure(struct nf_conn *ct)
{
	struct nf_conntrack_tuple *tuple;
	struct sfp_mgr_fwd_tuple_hash *curr_fwd_hash;
	struct sfp_mgr_fwd_tuple_hash *target_hash;
	struct sfp_conn *sfp_ct;
	u32 hash;
	u8 dir;
	int cur_tether_scheme;

	tuple =
	(struct nf_conntrack_tuple *)&ct->tuplehash[IP_CT_DIR_ORIGINAL].tuple;
	hash = sfp_hash_conntrack(tuple);
	rcu_read_lock_bh();
	hlist_for_each_entry_rcu(curr_fwd_hash,
				 &mgr_fwd_entries[hash],
				 entry_lst) {
		if (sfp_ct_tuple_equal(curr_fwd_hash, tuple)) {
			/*Get sfp_conn in sfp*/
			sfp_ct = sfp_ct_tuplehash_to_ctrack(curr_fwd_hash);
			if (!sfp_ct->tcp_sure_flag) {
				dir = curr_fwd_hash->tuple.dst.dir;
				target_hash = &sfp_ct->tuplehash[!dir];

				mod_timer(&sfp_ct->timeout,
					  jiffies + SFP_TCP_ESTABLISHED_TIME);
				FP_PRT_DBG(FP_PRT_DEBUG,
					   "insert new fwd entry [%u]\n", hash);
				fp_prt_tuple_info(FP_PRT_DEBUG,
						   &curr_fwd_hash->tuple);
				fp_prt_tuple_info(FP_PRT_DEBUG,
						   &target_hash->tuple);
				sfp_ct->tcp_sure_flag = 1;

				cur_tether_scheme = get_sfp_tether_scheme();

				switch (cur_tether_scheme) {
				case SFP_HARD_PATH:
					sfp_ipa_hash_add(sfp_ct);
					break;
				case SFP_SOFT_PATH:
					sfp_fwd_hash_add(sfp_ct);
					break;
				case SFP_HALF_PATH:
					sfp_ipa_hash_add(sfp_ct);
					sfp_fwd_hash_add(sfp_ct);
					break;
				};

			}
			break;
		}
	}
	rcu_read_unlock_bh();
	return 0;
}

int sfp_mgr_entry_ct_confirmed(struct nf_conntrack_tuple  *tuple)
{
	int hash;
	int confirm = 0;
	struct sfp_mgr_fwd_tuple_hash *tuple_hash;

	rcu_read_lock_bh();
	hash = sfp_hash_conntrack(tuple);
	hlist_for_each_entry_rcu(tuple_hash,
				 &mgr_fwd_entries[hash],
				 entry_lst) {
		if (sfp_ct_tuple_equal(tuple_hash, tuple))
			confirm = 1;
	}
	rcu_read_unlock_bh();
	fp_prt_tuple_info(FP_PRT_DEBUG, tuple);
	return confirm;
}

static int sfp_check_netdevice_change(struct net_device *dev)
{
	int i = 0;
	int ifindex = 0;

	dev_hold(dev);
	while (sfp_netdev[i]) {
		if ((dev->name[0] != '\0') &&
		    (strncasecmp(dev->name, sfp_netdev[i],
				 strlen(sfp_netdev[i])) == 0)
		   ) {
			ifindex = dev->ifindex;
			break;
		}
		i++;
	}
	dev_put(dev);
	return ifindex;
}

void sfp_clear_fwd_table(int ifindex)
{
	struct sfp_mgr_fwd_tuple_hash *tuple_hash;
	int i;

	if (ifindex == 0)
		return;

	spin_lock_bh(&mgr_lock);
	rcu_read_lock_bh();
	for (i = 0; i < SFP_ENTRIES_HASH_SIZE; i++) {
		hlist_for_each_entry_rcu(tuple_hash,
					 &mgr_fwd_entries[i],
					 entry_lst) {
			if (tuple_hash->ssfp_fwd_tuple.in_ifindex == ifindex ||
			    tuple_hash->ssfp_fwd_tuple.out_ifindex == ifindex) {
				hlist_del_rcu(&tuple_hash->entry_lst);
				delete_in_sfp_fwd_table(tuple_hash);
				call_rcu(&tuple_hash->rcu,
					 sfp_mgr_fwd_entry_free);
			}
		}
	}
	rcu_read_unlock_bh();
	spin_unlock_bh(&mgr_lock);
}

/* Clear the whole sfp entries hash table. */
void clear_sfp_mgr_table(void)
{
	struct sfp_mgr_fwd_tuple_hash *tuple_hash;
	int i;

	FP_PRT_DBG(FP_PRT_DEBUG, "SFP: Clearing all forward entries\n");
	rcu_read_lock_bh();
	for (i = 0; i < SFP_ENTRIES_HASH_SIZE; i++) {
		hlist_for_each_entry_rcu(tuple_hash,
					 &mgr_fwd_entries[i],
					 entry_lst) {
			hlist_del_rcu(&tuple_hash->entry_lst);
			call_rcu(&tuple_hash->rcu, sfp_mgr_fwd_entry_free);
		}
	}
	rcu_read_unlock_bh();
}

static int sfp_if_netdev_event_handler(struct notifier_block *nb,
				       unsigned long event, void *ptr)
{
	int ifindex;
	struct net_device *dev = netdev_notifier_info_to_dev(ptr);

	FP_PRT_DBG(FP_PRT_DEBUG,
		   "iface_stat: netdev_event():ev=0x%lx name=%s\n",
		   event, dev ? dev->name : "");

	switch (event) {
	case NETDEV_DOWN:
		/*Do something for sfp tables*/
		ifindex = sfp_check_netdevice_change(dev);
		FP_PRT_DBG(FP_PRT_WARN,
			   "iface_stat: DOWN dev ifindex = %d\n",
			   ifindex);
		sfp_clear_fwd_table(ifindex);
		break;
	case NETDEV_REGISTER:
		if (sfp_clatd_dev_check(dev)) {
			sysctl_net_sfp_enable = 0;
			if (!get_sfp_tether_scheme())
				sfp_ipa_fwd_clear();
			else
				clear_sfp_fwd_table();
			clear_sfp_mgr_table();
			FP_PRT_DBG(FP_PRT_WARN,
				   "clatd check succeeded, won't enable sfp\n");
		}
		break;
	case NETDEV_UNREGISTER:
		if (sfp_clatd_dev_check(dev)) {
			sysctl_net_sfp_enable = 1;
			FP_PRT_DBG(FP_PRT_WARN,
				   "clatd check failed, re-enable sfp\n");
		}
		break;
	}

	return NOTIFY_DONE;
}

static struct notifier_block sfp_if_netdev_notifier_blk = {
	.notifier_call = sfp_if_netdev_event_handler,
};

int sfp_mgr_proc_enable(void)
{
	int err;

	FP_PRT_DBG(FP_PRT_DEBUG, "Network Fast Processing Enabled\n");
	err = register_netdevice_notifier(&sfp_if_netdev_notifier_blk);
	FP_PRT_DBG(FP_PRT_DEBUG, "Register netdev notifier, ret %d\n", err);
	return err;
}

void sfp_mgr_proc_disable(void)
{
	int cur_tether_scheme;
	unregister_netdevice_notifier(&sfp_if_netdev_notifier_blk);
	FP_PRT_DBG(FP_PRT_DEBUG, "Network Fast Processing Disabled\n");

	cur_tether_scheme = get_sfp_tether_scheme();

	switch (cur_tether_scheme) {
	case SFP_HARD_PATH:
		sfp_ipa_fwd_clear();
		break;
	case SFP_SOFT_PATH:
		/*Clear fwd table*/
		clear_sfp_fwd_table();
		break;
	case SFP_HALF_PATH:
		sfp_ipa_fwd_clear();
		clear_sfp_fwd_table();
		break;
	};

	/*Clear mgr table*/
	clear_sfp_mgr_table();
}

/* Initialize sfp entries hash table. */
static void sfp_entries_hash_init(void)
{
	int i;
	int cur_tether_scheme;

	FP_PRT_DBG(FP_PRT_DEBUG, "initializing sfp entries hash table.\n");
	for (i = 0; i < SFP_ENTRIES_HASH_SIZE; i++) {
		INIT_HLIST_HEAD(&mgr_fwd_entries[i]);
		cur_tether_scheme = get_sfp_tether_scheme();

		switch (cur_tether_scheme) {
		case SFP_HARD_PATH:
			INIT_HLIST_HEAD(&fwd_tbl.sfp_fwd_entries[i]);
			break;
		case SFP_SOFT_PATH:
			INIT_HLIST_HEAD(&sfp_fwd_entries[i]);
			break;
		case SFP_HALF_PATH:
			INIT_HLIST_HEAD(&fwd_tbl.sfp_fwd_entries[i]);
			INIT_HLIST_HEAD(&sfp_fwd_entries[i]);
			break;
		};
	}
}

static struct device sfp_ipa_dev;

struct device *sfp_get_ipa_dev(void)
{
	return &sfp_ipa_dev;
}

static int sfp_parse_dt(struct device *dev)
{
	int ret;
	u32 data;
	bool pamwifi_enable = false;
	struct device_node *np = dev->of_node;

	ret = of_property_read_u32(np, "tether_scheme", &data);
	if (ret) {
		dev_err(dev, "fail to read tether_scheme, %d\n", ret);
		return ret;
	}

	set_sfp_tether_scheme(data);

	ret = of_property_read_u32(np, "enable", &data);
	if (ret) {
		dev_err(dev, "fail to read enable, %d\n", ret);
		return ret;
	}

	set_sfp_enable(data);

	if (of_property_read_bool(np, "sfp,enable-pamwifi")) {
		pamwifi_enable = true;
		set_sfp_ipa_pamwifi_enable(pamwifi_enable);
	}

	return 0;
}

static int sfp_ipa_plat_drv_probe(struct platform_device *pdev)
{
	int ret;

	if (pdev->dev.of_node) {
		ret = sfp_parse_dt(&pdev->dev);
		if (ret) {
			dev_err(&pdev->dev, "failed to parse device tree, ret=%d\n", ret);
			return ret;
		}
		memset(&sfp_ipa_dev, 0, sizeof(struct device));
		sfp_ipa_dev = pdev->dev;
		sfp_ipa_init();
	}

	return 0;
}

static const struct of_device_id sfp_ipa_plat_drv_match[] = {
	{ .compatible = "sprd,sfp",},
	{}
};

static struct platform_driver sfp_ipa_plat_drv = {
	.probe = sfp_ipa_plat_drv_probe,
	.driver = {
		.name = SFP_IFACE_PREF,
		.owner = THIS_MODULE,
		.of_match_table = sfp_ipa_plat_drv_match,
	},
};

static int __init init_sfp_module(void)
{
	int ret;

	spin_lock_init(&mgr_lock);
	sfp_entries_hash_init();

	ret = platform_driver_register(&sfp_ipa_plat_drv);
	if (ret) {
		pr_err("Failed to register sfp_ipa_plat_drv: %d\n", ret);
		return 0;
	}
	sfp_proc_create();
	nf_sfp_conntrack_init();
	sfp_netlink_init();
	sysctl_sfp_init();
	if (sysctl_net_sfp_enable == 1)
		sfp_mgr_proc_enable();

	return 0;
}

static void __exit exit_sfp_module(void)
{
	sfp_mgr_disable();
	nfp_proc_exit();
	sfp_netlink_exit();
	sysctl_sfp_exit();
	platform_driver_unregister(&sfp_ipa_plat_drv);
}

late_initcall(init_sfp_module);
module_exit(exit_sfp_module);
MODULE_ALIAS("platform:SPRD IPA driver.");
MODULE_DESCRIPTION("IP Application Accelerator (IPA) accelerates the delivery of IP based applications.");
MODULE_AUTHOR("Junjie Wang <junjie.wang@spreadtrum.com>");
MODULE_LICENSE("GPL v2");
MODULE_VERSION("1.0");
