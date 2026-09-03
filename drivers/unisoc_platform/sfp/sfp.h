/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) 2020 Spreadtrum Communications Inc.
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
#ifndef __SFP_H__
#define __SFP_H__

/* Includes */
#include <linux/kernel.h>
#include <linux/timer.h>
#include <linux/ip.h>
#include <net/tcp.h>
#include <net/udp.h>
#include <net/genetlink.h>
#include <linux/sipa.h>
#include <linux/types.h>
#include <linux/kern_levels.h>
#include <net/netfilter/nf_nat.h>
#include <net/netfilter/nf_conntrack_tuple.h>
#include <linux/jhash.h>
#include <linux/atomic.h>
#include <linux/refcount.h>
#include <linux/netdevice.h>
#include <linux/of_device.h>
#include <linux/soc/sprd/sfp.h>
#include "sfp_hash.h"
#define SFP_OK		0x00  /* Operation succeeded */
#define SFP_FAIL		0x01   /* Operation failed */

#define SFP_RST_FLAG 3

#define MAC_ADDR_SIZE 6

#define SFP_TCP_CT_WAITING (10 * HZ)

#define SFP_IFACE_PREF "sfp"

extern unsigned int sfp_stats_bytes;

enum {
	SFP_HARD_PATH,
	SFP_SOFT_PATH,
	SFP_HALF_PATH
};

enum {
	IP_L4_PROTO_NULL = 0,
	IP_L4_PROTO_ICMP = 1,
	IP_L4_PROTO_TCP	= 6,    /* Transmission Control Protocol        */
	IP_L4_PROTO_UDP	= 17,   /* User Datagram Protocol               */
	IP_L4_PROTO_ICMP6 = 58,
	IP_L4_PROTO_RAW	= 255,  /* Raw IP packets                       */
	IP_L4_PROTO_MAX
};

enum {
	SET_FP_FORWARD = 1,	/* Add forward info */
	SET_FP_ROUTING,	/* Add routing info */
};

#define SFP_FLAG_ENTRY_INFO		BIT(1)
#define SFP_FLAG_ROUTING_INFO		BIT(2)
#define SFP_FLAG_WHOLE		(SFP_FLAG_ENTRY_INFO | SFP_FLAG_ROUTING_INFO)

enum {
	SFP_CT_FLAG_FORWARD = 1,
	SFP_CT_FLAG_POSTROUTING = SFP_FLAG_ENTRY_INFO,
	SFP_CT_FLAG_PREROUTING = SFP_FLAG_ROUTING_INFO,
	SFP_CT_FLAG_WHOLE =
	(SFP_CT_FLAG_FORWARD | SFP_CT_FLAG_POSTROUTING | SFP_CT_FLAG_PREROUTING)
};

/* Declare structs */
struct sfp_mgmt_info {
	int status;
	int count;
};

enum sfp_tcp_flag_set {
	TCP_SYN_SET,
	TCP_SYNACK_SET,
	TCP_FIN_SET,
	TCP_ACK_SET,
	TCP_RST_SET,
	TCP_NONE_SET,
};

struct pskb_header {
	void *data_header;
	void *ip_header;
	unsigned int tot_len;
	unsigned short offset;
};

#define SFP_ENTRY_NEW_SRC_IP  NF_NAT_MANIP_SRC
#define SFP_ENTRY_NEW_DST_IP  NF_NAT_MANIP_DST

struct mac_info {
	u8 dst_mac[6]; /* Destination mac */
	u8 src_mac[6]; /* Dource mac */
} __packed;

union sfp_inet_addr {
	__be32 ip;
	__be32 ip6[4];
	__u32 all[4];
};

union sfp_l4_info {
	/* Add other protocols here. */
	__be16 all;
	struct {
		__be16 port;
	} tcp;
	struct {
		__be16 port;
	} udp;
	struct {
		u8 type, code;
	} icmp;
};

struct pkt_tuple_info {
	union sfp_inet_addr dst_ip;    /* Destination ip */
	union sfp_inet_addr src_ip;    /* Source ip */
	union sfp_l4_info src_l4_info;
	union sfp_l4_info dst_l4_info;
	u8 l3_proto;   /* Ipv4 or ipv6 */
	u8 l4_proto;   /* Tcp or udp */
} __packed;

struct sfp_entry_info {
	/* Basic info from skb */
	u8 src_mac[6]; /* Source mac */
	u8 dst_mac[6]; /* Destination mac */
	u32 src_ip;    /* Source ip */
	u32 dst_ip;    /* Destination ip */
	u8 proto;
	u16 sport;
	u16 dport;
	/* Get info from ct */
	u32  new_ip;
	u16  new_port;
	u8   nf_flags;
	/* Get info from routing */
	u8 in_ifindex;
	u8 out_ifindex;
};

struct sfp_trans_tuple {
	struct mac_info trans_mac_info;
	struct pkt_tuple_info trans_info;
	u8 fwd_flags;
	u32 count;
	u8 in_ifindex;
	u8 out_ifindex;
	u8 in_ipaifindex;
	u8 out_ipaifindex;
};

struct sfp_mgr_fwd_tuple {
	struct mac_info orig_mac_info;
	struct mac_info trans_mac_info;
	struct pkt_tuple_info orig_info;
	struct pkt_tuple_info trans_info;
	u32 count;
	u8 in_ifindex;
	u8 out_ifindex;
	u8 in_ipaifindex;
	u8 out_ipaifindex;
	/*
	 * 1:nat;2:drop;3:bypass;
	 * 4:do fwd,but the pkt will be back to software network
	 */
	u8 fwd_flags;
};

struct sfp_fwd_entry {
	struct hlist_node entry_lst;
	struct nf_conntrack_tuple tuple;
	struct sfp_trans_tuple ssfp_trans_tuple;
	struct rcu_head	 rcu;
	struct sfp_conn *sfp_ct;
};

#if IS_ENABLED(CONFIG_UNISOC_SIPA_V3)
/* sizeof fwd_entry is 120 bytes */
struct fwd_entry {
	struct pkt_tuple_info orig_info;
	struct pkt_tuple_info trans_info;
	struct mac_info trans_mac_info;
	u8 out_ifindex;
	u8 fwd_flags;
	u8 mac_info_opts;
	u8 reserve1;
	u32 pkt_drop_th;
	u32 pkt_current_idx; /* ip stream index, read-only for software */
	u32 pkt_total_cnt; /* ip stream totol pkt count */
	u32 pkt_current_cnt; /* ip stream pkt count currently */
	u32 pkt_drop_cnt; /* ip stream total pkt drop count */
	__be32 time_stamp;
	u32 reserve2;
} __packed;
#else
/* sizeof fwd_entry is 96 bytes */
struct fwd_entry {
	struct pkt_tuple_info orig_info;
	struct pkt_tuple_info trans_info;
	struct mac_info trans_mac_info;
	u8 out_ifindex;
	u8 fwd_flags;
	__be32 time_stamp;
	u16 reserve;
} __packed;
#endif

struct hd_hash_tbl {
	u8 pad1;
	__be16 num;
	u8 pad2;
	__be32 haddr;
} __packed;

struct sfp_mgr_fwd_tuple_hash {
	struct hlist_node entry_lst;
	struct nf_conntrack_tuple tuple;
	struct sfp_mgr_fwd_tuple ssfp_fwd_tuple;
	struct rcu_head	 rcu;
};

struct sfp_conn {
	refcount_t used;
	atomic_t del;
	struct sfp_mgr_fwd_tuple_hash tuplehash[IP_CT_DIR_MAX];
	u32 hash[IP_CT_DIR_MAX];
	unsigned long status;
	unsigned int sfp_status;
	unsigned int insert_flag;
	unsigned int fin_rst_flag;
	unsigned int tcp_sure_flag;
	/* Timer function; drops refcnt when it goes off. */
	struct timer_list timeout;
	u32 ts;
	int expire;
};

enum sfp_attrs {
	SFP_A_UNSPEC,
	SFP_A_FILTER,
	SFP_A_STATS,
	__SFP_A_MAX
};

#define SFP_A_MAX (__SFP_A_MAX - 1)

enum sfp_commands {
	__SFP_CMD_UNSPEC,
	SFP_NL_CMD_APPEND,
	SFP_NL_CMD_INSERT,
	SFP_NL_CMD_DELETE,
	SFP_NL_CMD_FLUSH,
	SFP_NL_CMD_LIST,
	SFP_NL_CMD_STATS,
	SFP_CMD_MAX,
};

struct sfp_routing_info {
	u32 src_ip;
	u32 dst_ip;
	/* Get info from routing */
	u8 in_ifindex;
	u8 out_ifindex;
};

struct sfp_route_entry {
	struct hlist_node entry_lst;
	struct sfp_routing_info routing_info;
	atomic_t used;
	struct rcu_head	 rcu;
};

struct sfpintf_iter_state {
	int bucket;
};

struct sfp_mgr_fwd_iter_state {
	int bucket;
};

struct sfpfwd_iter_state {
	int bucket;
	int count;
};

enum {
	T0 = 0,
	T1 = 1,
	SFP_IPA_TBL_MAX,
};

struct sfp_ipa_addr {
	size_t sz;
	u8 *v_addr;
	dma_addr_t handle;
	size_t len;
};

struct sfp_ipa_hash_tbl {
	int alloc_sz;
	struct sfp_ipa_addr h_tbl;
	struct sipa_hash_table sipa_tbl;
};

struct sfp_ipa_tbl_mgr {
	struct sfp_ipa_hash_tbl tbl[SFP_IPA_TBL_MAX];
};

struct sfp_ipa_table {
	u8 *v_addr;
	dma_addr_t handle;
	struct sipa_hash_table ipa_tbl;
};

struct sfp_fwd_hash_tbl {
	spinlock_t sp_lock;	/* Spin lock for ipa fwd */
	atomic_t entry_cnt;
	int op_flag;
	int append_cnt;
	u32 hash_lst[IP_CT_DIR_MAX];
	struct sfp_ipa_tbl_mgr ipa_tbl_mgr;
	struct timer_list recycle_timer;
	struct hlist_head sfp_fwd_entries[SFP_ENTRIES_HASH_SIZE];
};

/* The golden ration: an arbitrary value */
#define SP_JHASH_GOLDEN_RATIO           0x9e3779b9

static inline u32 sprd_sfp_hash(u32 a, u32 b, u32 c, u32 initval)
{
	a += SP_JHASH_GOLDEN_RATIO;
	b += SP_JHASH_GOLDEN_RATIO;
	c += initval;

	a -= b; a -= c; a ^= (c >> 13);
	b -= c; b -= a; b ^= (a << 8);
	c -= a; c -= b; c ^= (b >> 13);
	a -= b; a -= c; a ^= (c >> 12);
	b -= c; b -= a; b ^= (a << 16);
	c -= a; c -= b; c ^= (b >> 5);
	a -= b; a -= c; a ^= (c >> 3);
	b -= c; b -= a; b ^= (a << 10);
	c -= a; c -= b; c ^= (b >> 15);
	return c;
}

#define IP_SFT(ip, x) (((ip) >> (x)) & 0xFF)

/* Debug control */
#define FP_DEBUG

#define PF_PRT_NO		0x0000
#define FP_PRT_ERR		0x0001
#define FP_PRT_WARN		0x0002
#define FP_PRT_DEBUG		0x0004
#define FP_PRT_INFO		0x0008
#define FP_PRT_DETAIL		0x0010
#define FP_PRT_ALL		0x3

#ifdef FP_DEBUG
extern unsigned int fp_dbg_lvl;
#define SFP_LOG_TAG  "SFP"
#define FP_PRT_DBG(FLG, fmt, arg...) {\
	if (fp_dbg_lvl & (FLG))\
		pr_info("SFP:" fmt, ##arg);\
	}
#else
#define FP_PRT_DBG(FLG, fmt, arg...)
#endif

#define IPID "id(%x)"
#define TCP_FMT "seq: %x, ack: %x, %d -> %d"

extern struct hlist_head mgr_fwd_entries[SFP_ENTRIES_HASH_SIZE];
extern struct hlist_head sfp_fwd_entries[SFP_ENTRIES_HASH_SIZE];
extern struct sfp_ipa_tbl_mgr ipa_tbl_mgr;
extern struct net init_net;
extern int sysctl_net_sfp_enable;
extern int sysctl_net_sfp_tether_scheme;
extern int sysctl_tcp_aging_time;
extern int sysctl_udp_aging_time;
extern spinlock_t mgr_lock;
extern struct sfp_fwd_hash_tbl fwd_tbl;

extern int test_count;
extern int test_result;
/* Copy 6 bytes. Warning - doesn't perform any checks on memory, just copies */
static inline void mac_addr_copy(u8 *dst, const u8 *src)
{
	memcpy(dst, src, 6);
}

static inline void fp_prt_tuple_info(int dbg_lvl,
				     const struct nf_conntrack_tuple *tuple)
{
	if (tuple->src.l3num == NFPROTO_IPV4) {
		u32 src = tuple->src.u3.ip;
		u32 dst = tuple->dst.u3.ip;

		FP_PRT_DBG(dbg_lvl,
			   "\t[%u]-%pI4-%pI4-proto[%u][%u][%u]\n",
			   tuple->dst.dir, &src, &dst,
			   tuple->dst.protonum,
			   ntohs(tuple->src.u.all),
			   ntohs(tuple->dst.u.all));
	} else {
		struct in6_addr src = tuple->src.u3.in6;
		struct in6_addr dst = tuple->dst.u3.in6;

		FP_PRT_DBG(dbg_lvl,
			   "\t\t[%u]-%pI6-%pI6-proto[%u][%u][%u]\n",
			   tuple->dst.dir, &src, &dst,
			   tuple->dst.protonum,
			   ntohs(tuple->src.u.all),
			   ntohs(tuple->dst.u.all));
	}
}

#define TCPF_SYN			0X02
#define TCPF_RST			0X04
#define TCPF_ACK			0X10
#define TCPF_FINACK			0X11
#define TCPF_SYNACK			0X12
#define TCPF_RSTACK			0X14
#define TCPF_PUSHACK			0X18
#define TCPF_FINPUSHACK		0X19

static inline char *get_tcp_flag(struct tcphdr *hp)
{
	static char s_tcp_state[9][16] = {
		"UNKNOWN",
		"TCP_SYN",
		"TCP_RST",
		"TCP_ACK",
		"TCP_FINACK",
		"TCP_SYNACK",
		"TCP_RSTACK",
		"TCP_PUSHACK",
		"TCP_FINPUSHACK",
	};
	char *p = (char *)hp;
	char flag = *(p + 13) & 0x1f;

	switch (flag) {
	case TCPF_SYN:
		return s_tcp_state[1];
	case TCPF_RST:
		return s_tcp_state[2];
	case TCPF_ACK:
		return s_tcp_state[3];
	case TCPF_FINACK:
		return s_tcp_state[4];
	case TCPF_SYNACK:
		return s_tcp_state[5];
	case TCPF_RSTACK:
		return s_tcp_state[6];
	case TCPF_PUSHACK:
		return s_tcp_state[7];
	case TCPF_FINPUSHACK:
		return s_tcp_state[8];
	default:
		return s_tcp_state[0];
	}

	return s_tcp_state[0];
}

static inline bool sfp_ct_tuple_equal(const struct sfp_mgr_fwd_tuple_hash *t1,
				      const struct nf_conntrack_tuple *t2)
{
	return __nf_ct_tuple_src_equal(&t1->tuple, t2) &&
	       __nf_ct_tuple_dst_equal(&t1->tuple, t2);
}

static inline bool sfp_ct_nf_tuple_equal(const struct nf_conntrack_tuple *t1,
					 const struct nf_conntrack_tuple *t2)
{
	return __nf_ct_tuple_src_equal(t1, t2) &&
	       __nf_ct_tuple_dst_equal(t1, t2);
}

static inline struct sfp_conn *
sfp_ct_tuplehash_to_ctrack(const struct sfp_mgr_fwd_tuple_hash *hash)
{
	return container_of(hash, struct sfp_conn,
			    tuplehash[hash->tuple.dst.dir]);
}

static inline bool sfp_tcp_rst_chk(struct nf_conn *ct)
{
	return (ct->proto.tcp.last_index == TCP_RST_SET) ? true : false;
}

static inline bool sfp_tcp_fin_chk(struct nf_conn *ct)
{
	return (ct->proto.tcp.last_index == TCP_FIN_SET) ? true : false;
}

static inline bool sfp_tcp_flag_chk(struct nf_conn *ct)
{
	return (ct->proto.tcp.last_index == TCP_RST_SET ||
		ct->proto.tcp.last_index == TCP_FIN_SET) ?
		true : false;
}

void sfp_mgr_disable(void);
int sfp_mgr_proc_enable(void);
void sfp_mgr_proc_disable(void);

int sfp_proc_create(void);
void nfp_proc_exit(void);

void sfp_fwd_entry_table_clear(void);
int check_sfp_fwd_table(struct nf_conntrack_tuple *tuple,
			struct sfp_trans_tuple *ret_info);
bool sfp_pkt_to_tuple(void *data,
		      u32 offset,
		      struct nf_conntrack_tuple *tuple,
		      u32 *l4offsetp);
void sfp_update_checksum(void *ipheader,
			 int  pkt_totlen,
			 u16 l3proto,
			 u32 l3offset,
			 u8 l4proto,
			 u32 l4offset);
int get_sfp_enable(void);
int get_sfp_tether_scheme(void);
int add_in_sfp_fwd_table(const struct sfp_mgr_fwd_tuple_hash *hash,
			 struct sfp_conn *sfp_ct);
void sfp_fwd_hash_add(struct sfp_conn *sfp_ct);
void sfp_ipa_init(void);

int sysctl_sfp_init(void);
void sysctl_sfp_exit(void);

int sfp_netlink_init(void);
void sfp_netlink_exit(void);

int get_sfp_fwd_entry_count(struct sfp_mgr_fwd_tuple_hash *fwd_hash_entry);
int delete_in_sfp_fwd_table(const struct sfp_mgr_fwd_tuple_hash *hash);
void clear_sfp_fwd_table(void);
int sfp_sync_with_nfl_ct(const struct nf_conntrack_tuple *tuple);
int nf_sfp_conntrack_init(void);
bool sfp_ipa_tbl_timeout(struct sfp_conn *sfp_ct);
void sfp_ipa_swap_tbl_new(void);
void ipa_tbl_check(void);

int sfp_ct_init(struct nf_conn *ct, struct sfp_conn *sfp_ct);
void clear_sfp_mgr_table(void);
void print_hash_tbl(char *p, int len);

int sfp_ipa_fwd_add(enum ip_conntrack_dir dir, struct sfp_conn *sfp_ct);
int sfp_ipa_hash_add(struct sfp_conn *sfp_ct);
int sfp_ipa_fwd_delete(const struct sfp_mgr_fwd_tuple_hash *t_hash, u32 hash);
void sfp_ipa_fwd_clear(void);
void sfp_ipa_swap_tbl(void);
void sfp_clear_all_ipa_tbl(void);
int sfp_tbl_id(void);
void sfp_clear_fwd_table(int ifindex);
bool is_banned_ipa_netdev(struct net_device *dev);

int sfp_mgr_fwd_entry_delete(const struct nf_conntrack_tuple *tuple);
void sfp_ipa_entry_delete(u32 hash);
void sfp_destroy_ipa_tbl(void);
bool sfp_ipa_ipv6_check(const struct sk_buff *skb,
			u16 *first, u8 *nexthdr, u32 *off);

struct device *sfp_get_ipa_dev(void);
u32 hash_conntrack(const struct nf_conntrack_tuple *tuple);

int sfp_test_init(int count);

int get_sfp_tether_scheme(void);
void set_sfp_tether_scheme(int tether_scheme);
void set_sfp_enable(int enable);

#endif
