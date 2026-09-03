/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef _IMSBR_CORE_H
#define _IMSBR_CORE_H

#include <linux/netfilter.h>
#include <linux/percpu.h>
#include <net/netfilter/nf_conntrack.h>

#include "imsbr_sipc.h"

/* NOTE: if the struct imsbr_msghdr{} changed, IMSBR_MSG_VERSION should
 * be changed also.
 */
#define IMSBR_MSG_VERSION	0

#define IMSBR_CMD_MAXSZ		20

/**
 * CAUTION: For compatibility reason, when you want to extend the current
 * control message payload, you must put them at the end! Otherwise, CP folks
 * will beat and kill you.
 */
struct imsbr_msghdr {
	/* If the version is not consistent between AP and CP, we will complain
	 * from time to time. But AFAICS, this is useless anymore.
	 */
	u16 imsbr_version;
	u16 imsbr_paylen;
	/**
	 * The commands triggered by AP:
	 *     "ho-lte2wifi"   [-payload-]: simcard(u32)
	 *     "ho-wifi2lte"   [-payload-]: simcard(u32)
	 *     "ho-finish"     [-payload-]: simcard(u32)
	 *     "aptuple-add"   [-payload-]: struct imsbr_tuple{}
	 *     "aptuple-del"   [-payload-]: struct imsbr_tuple{}
	 *     "aptuple-reset" [-payload-]: simcard(u32)
	 *     "vowifi-call"   [-payload-]: simcard(u32)
	 *     "volte-call"    [-payload-]: simcard(u32)
	 *     "call-end"      [-payload-]: simcard(u32)
	 *     "ltevideo-apsk" [-payload-]: boolean(u32)
	 *     "test-xxx"      [-payload-]: some test commands
	 * The commands triggered by CP:
	 *     "cptuple-add"   [-payload-]: struct imsbr_tuple{}
	 *     "cptuple-del"   [-payload-]: struct imsbr_tuple{}
	 *     "cptuple-reset" [-payload-]: simcard(u32)
	 * The commands triggered by BOTH:
	 *     "echo-ping"     [-payload-]: string
	 *     "echo-pong"     [-payload-]: string
	 * The commands reserved by ALL:
	 *     "unknown"
	 */
	char imsbr_cmd[IMSBR_CMD_MAXSZ];
	char imsbr_payload[0];
};

#define IMSBR_MSG_MAXLEN \
	(IMSBR_CTRL_BLKSZ - sizeof(struct imsbr_msghdr))

#define IMSBR_FLOW_MIN_NR		64

enum imsbr_flow_types {
	IMSBR_FLOW_CPTUPLE,
	IMSBR_FLOW_APTUPLE,
};

struct handover_state{
	int ho_type;
	u8	sim_card;
};

#define IMSBR_FLOW_HSIZE		512

struct imsbr_flow {
	struct hlist_node		hlist;
	struct nf_conntrack_tuple	nft_tuple;
	u16				flow_type;
	u8				media_type;
	u8				sim_card;
	u16				link_type;
	u16				socket_type;
	struct rcu_head			rcu;
};

struct imsbr_stat {
	u64 ip_output_fail;
	u64 ip_route_fail;
	u64 ip6_output_fail;
	u64 ip6_route_fail;
	u64 xfrm_lookup_fail;
	u64 sk_buff_fail;
	u64 flow_duplicated;

	u64 sipc_get_fail;
	u64 sipc_receive_fail;
	u64 sipc_send_fail;

	u64 nfct_get_fail;
	u64 nfct_untracked;
	u64 nfct_slow_path;

	u64 pkts_fromcp;
	u64 pkts_tocp;

	u64 frag_create;
	u64 frag_ok;
	u64 frag_fail;

	u64 reasm_request;
	u64 reasm_ok;
	u64 reasm_fail;
};

extern struct imsbr_stat __percpu *imsbr_stats;

#define IMSBR_STAT_INC(field)	this_cpu_inc(field)

struct imsbr_simcard {
	int		init_call;
	int		curr_call;
	atomic_t	ho_state;
};

extern struct imsbr_simcard imsbr_simcards[];

/* Currently, only support one sim card! */
#define IMSBR_SIMCARD_NUM	2

extern atomic_t imsbr_enabled;
extern unsigned int cur_lp_state;

enum imsbr_ho_types {
	IMSBR_HO_UNSPEC,
	IMSBR_HO_LTE2WIFI,
	IMSBR_HO_WIFI2LTE,
	IMSBR_HO_FINISH,
	__IMSBR_HO_MAX
};

static inline bool imsbr_in_wifi2lte(int simcard)
{
	if (atomic_read(&imsbr_simcards[simcard].ho_state) ==
	    IMSBR_HO_WIFI2LTE)
		return true;

	return false;
}

static inline bool imsbr_in_lte2wifi(int simcard)
{
	if (atomic_read(&imsbr_simcards[simcard].ho_state) ==
	    IMSBR_HO_LTE2WIFI)
		return true;

	return false;
}

#ifdef CONFIG_SPRD_IMS_BRIDGE_TEST

struct call_c_function {
	void (*cptuple_update) (struct imsbr_msghdr *msg, unsigned long add);
	void (*cp_sync_esp) (struct imsbr_msghdr *msg, unsigned long unused);
	void (*handover_state) (struct imsbr_msghdr *msg, unsigned long unused);
	void (*cptuple_reset) (struct imsbr_msghdr *msg, unsigned long unused);
	void (*cp_reset) (struct imsbr_msghdr *msg, unsigned long unused);
	void (*echo_ping) (struct imsbr_msghdr *msg, unsigned long unused);
	void (*echo_pong) (struct imsbr_msghdr *msg, unsigned long unused);
};
void call_core_function(struct call_c_function *ccf);

#endif

void imsbr_set_callstate(enum imsbr_call_state state, u32 simcard);

void imsbr_set_calltype(enum imsbr_lowpower_state lp_st);

bool imsbr_tuple_validate(const char *msg, struct imsbr_tuple *tuple);

void imsbr_flow_add(struct nf_conntrack_tuple *nft, u16 flow_type,
		    struct imsbr_tuple *tuple);
void imsbr_flow_del(struct nf_conntrack_tuple *nft, u16 flow_type,
		    struct imsbr_tuple *tuple);
struct imsbr_flow *imsbr_flow_match(struct nf_conntrack_tuple *nft);
void imsbr_flow_reset(u16 flow_type, u8 sim_card, bool quiet);

void imsbr_tuple2nftuple(struct imsbr_tuple *tuple,
			 struct nf_conntrack_tuple *nft,
			 bool invert);
void imsbr_tuple_dump(const char *prefix, struct imsbr_tuple *tuple);

void imsbr_process_msg(struct imsbr_sipc *sipc, struct sblock *blk,
		       bool freeit);

int imsbr_core_init(void);
void imsbr_core_exit(void);

//TODO temp modification
int imsbr_esp_update_esphs(char *esp);
void imsbr_esp_update_lp_st(int lp_st);

#endif
