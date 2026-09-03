// SPDX-License-Identifier: GPL-2.0-only
/* Copyright (C) 2016 Spreadtrum Communications Inc.
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

#define pr_fmt(fmt) "sprd-imsbr: " fmt

#include <linux/init.h>
#include <linux/jhash.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/mempool.h>
#include <linux/net.h>
#include <linux/proc_fs.h>
#include <linux/random.h>
#include <linux/spinlock.h>
#include <net/netfilter/nf_conntrack_core.h>
#include <uapi/linux/ims_bridge/ims_bridge.h>
#include <net/esp.h>

#include "imsbr_core.h"
#include "imsbr_packet.h"
#include "imsbr_hooks.h"

static DEFINE_MUTEX(imsbr_mutex);

atomic_t imsbr_enabled __read_mostly;

struct imsbr_simcard imsbr_simcards[IMSBR_SIMCARD_NUM] __read_mostly;

struct imsbr_stat __percpu *imsbr_stats;
unsigned int cur_lp_state;

static struct kmem_cache *imsbr_flow_cache;
static mempool_t *imsbr_flow_pool;

static struct hlist_head *imsbr_flow_bucket;

static u32 imsbr_hash_rnd __read_mostly;

static void imsbr_cptuple_update(struct imsbr_msghdr *msg, unsigned long add);
static void imsbr_cptuple_reset(struct imsbr_msghdr *msg, unsigned long unused);
static void imsbr_cp_reset(struct imsbr_msghdr *msg, unsigned long unused);
static void imsbr_echo_ping(struct imsbr_msghdr *msg, unsigned long unused);
static void imsbr_echo_pong(struct imsbr_msghdr *msg, unsigned long unused);
static void imsbr_cp_sync_esp(struct imsbr_msghdr *msg, unsigned long unused);
static void imsbr_handover_state(struct imsbr_msghdr *msg, unsigned long unused);

static struct imsbr_msglist {
	const char *name;
	void (*doit)(struct imsbr_msghdr *msg, unsigned long arg);
	unsigned long arg;
	unsigned int req_len;
} imsbr_msglists[] = {
	{
		.name = "cptuple-add",
		.doit = imsbr_cptuple_update,
		.arg = 1,
		.req_len = sizeof(struct imsbr_tuple),
	},
	{
		.name = "cptuple-del",
		.doit = imsbr_cptuple_update,
		.arg = 0,
		.req_len = sizeof(struct imsbr_tuple),
	},
	{
		.name = "cptuple-reset",
		.doit = imsbr_cptuple_reset,
		.req_len = sizeof(u32),
	},
	{
		.name = "cp-reset",
		.doit = imsbr_cp_reset,
		.req_len = sizeof('\0'),
	},
	{
		.name = "echo-ping",
		.doit = imsbr_echo_ping,
		.req_len = sizeof('\0'),
	},
	{
		.name = "echo-pong",
		.doit = imsbr_echo_pong,
		.req_len = sizeof('\0'),
	},
	{
		.name = "cp-sync-esp",
		.doit = imsbr_cp_sync_esp,
		.req_len = sizeof('\0'), /* size not fixed */
	},
	{
		.name = "handover-state",
		.doit = imsbr_handover_state,
		.req_len = sizeof('\0'),
	},
};

static u32 imsbr_tuple_hash(struct nf_conntrack_tuple *nft)
{
	u32 src = 0, dst = 0;
	int i;

	for (i = 0; i < 4; i++) {
		src += nft->src.u3.all[i];
		dst += nft->dst.u3.all[i];
	}

	src += nft->src.l3num;
	dst += nft->dst.protonum;

	return jhash_2words(src, dst, imsbr_hash_rnd) % IMSBR_FLOW_HSIZE;
}

static struct imsbr_flow *
imsbr_flow_find(struct nf_conntrack_tuple *nft, u16 flow_type, u32 h)
{
	struct imsbr_flow *flow;

	hlist_for_each_entry_rcu(flow, &imsbr_flow_bucket[h], hlist) {
		if (flow->flow_type == flow_type &&
		    nf_ct_tuple_equal(&flow->nft_tuple, nft))
			return flow;
	}

	return NULL;
}

bool imsbr_tuple_validate(const char *msg, struct imsbr_tuple *tuple)
{
	if (tuple->sim_card >= IMSBR_SIMCARD_NUM) {
		pr_err("%s simcard %d is out of range!\n", msg,
		       tuple->sim_card);
		return false;
	}

	if (tuple->media_type == IMSBR_MEDIA_UNSPEC ||
	    tuple->media_type >= __IMSBR_MEDIA_MAX) {
		pr_err("%s media_type %d is invalid!\n", msg,
		       tuple->media_type);
		return false;
	}

	if (tuple->link_type == IMSBR_LINK_UNSPEC ||
	    tuple->link_type >= __IMSBR_LINK_MAX) {
		pr_err("%s link_type %d is invalid!\n", msg,
		       tuple->link_type);
		return false;
	}

	if (tuple->socket_type == IMSBR_SOCKET_UNSPEC ||
	    tuple->socket_type >= __IMSBR_SOCKET_MAX) {
		pr_err("%s socket_type %d is invalid!\n", msg,
		       tuple->socket_type);
		return false;
	}

	return true;
}

/* must be called with rcu_read_lock hold */
struct imsbr_flow *imsbr_flow_match(struct nf_conntrack_tuple *nft)
{
	struct imsbr_flow *flow = NULL;
	u32 h;

	h = imsbr_tuple_hash(nft);

	hlist_for_each_entry_rcu(flow, &imsbr_flow_bucket[h], hlist) {
		struct nf_conntrack_tuple *t = &flow->nft_tuple;

		if (nf_inet_addr_cmp(&t->src.u3, &nft->src.u3) &&
		    nf_inet_addr_cmp(&t->dst.u3, &nft->dst.u3) &&
		    t->src.l3num == nft->src.l3num &&
		    t->dst.protonum == nft->dst.protonum) {
			if (t->src.u.all != 0 &&
			    t->src.u.all != nft->src.u.all)
				continue;
			if (t->dst.u.all != 0 &&
			    t->dst.u.all != nft->dst.u.all)
				continue;

			break;
		}
	}

	return flow;
}

void imsbr_flow_add(struct nf_conntrack_tuple *nft, u16 flow_type,
		    struct imsbr_tuple *tuple)
{
	struct imsbr_flow *flow;
	u32 h;

	h = imsbr_tuple_hash(nft);

	mutex_lock(&imsbr_mutex);
	flow = imsbr_flow_find(nft, flow_type, h);
	if (flow) {
		pr_debug("flow duplicated!\n");
		IMSBR_STAT_INC(imsbr_stats->flow_duplicated);
		mutex_unlock(&imsbr_mutex);
		return;
	}

	flow = mempool_alloc(imsbr_flow_pool, GFP_KERNEL);
	if (!flow) {
		pr_err("mempool alloc fail!\n");
		mutex_unlock(&imsbr_mutex);
		return;
	}

	memset(flow, 0, sizeof(*flow));
	flow->nft_tuple = *nft;
	flow->flow_type = flow_type;
	flow->media_type = tuple->media_type;
	flow->link_type = tuple->link_type;
	flow->socket_type = tuple->socket_type;
	flow->sim_card = tuple->sim_card;

	/* Insert at the front, so if tuple is same between volte and vowifi,
	 * the later has the higher priority!
	 */
	hlist_add_head_rcu(&flow->hlist, &imsbr_flow_bucket[h]);
	atomic_inc(&imsbr_enabled);

	mutex_unlock(&imsbr_mutex);
}

static void imsbr_flow_free(struct rcu_head *head)
{
	struct imsbr_flow *flow;

	flow = container_of(head, struct imsbr_flow, rcu);
	mempool_free(flow, imsbr_flow_pool);
}

static void imsbr_flow_destroy(struct imsbr_flow *flow)
{
	atomic_dec(&imsbr_enabled);
	hlist_del_rcu(&flow->hlist);
	call_rcu(&flow->rcu, imsbr_flow_free);
}

void imsbr_flow_del(struct nf_conntrack_tuple *nft, u16 flow_type,
		    struct imsbr_tuple *tuple)
{
	struct imsbr_flow *flow;
	u32 h;

	h = imsbr_tuple_hash(nft);

	mutex_lock(&imsbr_mutex);
	flow = imsbr_flow_find(nft, flow_type, h);
	if (flow)
		imsbr_flow_destroy(flow);

	mutex_unlock(&imsbr_mutex);
}

void imsbr_flow_reset(u16 flow_type, u8 sim_card, bool quiet)
{
	struct imsbr_flow *flow;
	struct hlist_node *n;
	int i, cnt = 0;

	mutex_lock(&imsbr_mutex);
	for (i = 0; i < IMSBR_FLOW_HSIZE; i++) {
		hlist_for_each_entry_safe(flow, n,
					  &imsbr_flow_bucket[i], hlist) {
			if (flow->flow_type == flow_type &&
			    flow->sim_card == sim_card) {
				imsbr_flow_destroy(flow);
				cnt++;
			}
		}
	}
	mutex_unlock(&imsbr_mutex);

	if (!quiet) {
		pr_info("reset %d %stuple flows\n", cnt,
			flow_type == IMSBR_FLOW_CPTUPLE ? "cp" : "ap");
	}
}

void imsbr_tuple2nftuple(struct imsbr_tuple *tuple,
			 struct nf_conntrack_tuple *nft,
			 bool invert)
{
	memset(nft, 0, sizeof(*nft));

	if (tuple->l3proto == IPPROTO_IP)
		nft->src.l3num = AF_INET;
	else
		nft->src.l3num = AF_INET6;
	nft->dst.protonum = tuple->l4proto;

	if (invert) {
		memcpy(&nft->src.u3, &tuple->peer_addr,
		       sizeof(union nf_inet_addr));
		memcpy(&nft->dst.u3, &tuple->local_addr,
		       sizeof(union nf_inet_addr));

		nft->src.u.all = tuple->peer_port;
		nft->dst.u.all = tuple->local_port;
	} else {
		memcpy(&nft->src.u3, &tuple->local_addr,
		       sizeof(union nf_inet_addr));
		memcpy(&nft->dst.u3, &tuple->peer_addr,
		       sizeof(union nf_inet_addr));

		nft->src.u.all = tuple->local_port;
		nft->dst.u.all = tuple->peer_port;
	}
}

static char *imsbr_media_type2str(u8 media_type)
{
	switch (media_type) {
	case IMSBR_MEDIA_SIP:
		return "sip";
	case IMSBR_MEDIA_RTP_AUDIO:
		return "rtp-audio";
	case IMSBR_MEDIA_RTP_VIDEO:
		return "rtp-video";
	case IMSBR_MEDIA_RTCP_AUDIO:
		return "rtcp-audio";
	case IMSBR_MEDIA_RTCP_VIDEO:
		return "rtcp-video";
	case IMSBR_MEDIA_IKE:
		return "ike";
	default:
		return "unknown";
	}
}

static char *imsbr_link_type2str(u8 link_type)
{
	switch (link_type) {
	case IMSBR_LINK_AP:
		return ":apln";
	case IMSBR_LINK_CP:
		return ":cpln";
	default:
		return ":uknln";
	}
}

static char *imsbr_socket_type2str(u8 socket_type)
{
	switch (socket_type) {
	case IMSBR_SOCKET_AP:
		return "@apsk";
	case IMSBR_SOCKET_CP:
		return "@cpsk";
	default:
		return "@uknsk";
	}
}

void imsbr_tuple_dump(const char *prefix, struct imsbr_tuple *tuple)
{
	if (tuple->l3proto == IPPROTO_IP) {
		pr_debug("%s(%s) l3=%u l4=%u %pI4 %u -> %pI4 %u sim%d%s%s tnum=%d\n",
			prefix, imsbr_media_type2str(tuple->media_type),
			tuple->l3proto, tuple->l4proto,
			&tuple->local_addr.ip, ntohs(tuple->local_port),
			&tuple->peer_addr.ip, ntohs(tuple->peer_port),
			tuple->sim_card,
			imsbr_link_type2str(tuple->link_type),
			imsbr_socket_type2str(tuple->socket_type),
			atomic_read(&imsbr_enabled));
	} else {
		pr_debug("%s(%s) l3=%u l4=%u %pI6c %u -> %pI6c %u sim%d%s%s tnum=%d\n",
			prefix, imsbr_media_type2str(tuple->media_type),
			tuple->l3proto, tuple->l4proto,
			tuple->local_addr.ip6, ntohs(tuple->local_port),
			tuple->peer_addr.ip6, ntohs(tuple->peer_port),
			tuple->sim_card,
			imsbr_link_type2str(tuple->link_type),
			imsbr_socket_type2str(tuple->socket_type),
			atomic_read(&imsbr_enabled));
	}
}

static void imsbr_cptuple_update(struct imsbr_msghdr *msg, unsigned long add)
{
	struct nf_conntrack_tuple nft, nft_inv;
	struct imsbr_tuple *tuple;

	tuple = (struct imsbr_tuple *)msg->imsbr_payload;

	imsbr_tuple_dump(msg->imsbr_cmd, tuple);
	if (!imsbr_tuple_validate(msg->imsbr_cmd, tuple))
		return;

	imsbr_tuple2nftuple(tuple, &nft, false);
	imsbr_tuple2nftuple(tuple, &nft_inv, true);

	if (add) {
		imsbr_flow_add(&nft, IMSBR_FLOW_CPTUPLE, tuple);
		imsbr_flow_add(&nft_inv, IMSBR_FLOW_CPTUPLE, tuple);
	} else {
		imsbr_flow_del(&nft, IMSBR_FLOW_CPTUPLE, tuple);
		imsbr_flow_del(&nft_inv, IMSBR_FLOW_CPTUPLE, tuple);
	}
}

static void imsbr_cp_sync_esp(struct imsbr_msghdr *msg, unsigned long unused)
{
	int i, j;
	struct espheader *esph =
		(struct espheader *)(msg->imsbr_payload + sizeof(unsigned int));
	unsigned int esp_num = *((unsigned int *)msg->imsbr_payload);

	for (i = 0; i < esp_num; i++) {
		esph = (struct espheader *)((char *)esph +
			sizeof(*esph) * i);
		for (j = 0; j < MAX_ESPS; j++) {
			if (esphs[j].spi == esph->spi) {
				esphs[j].seq = esph->seq;
				break;
			}
		}
	}

	/* TODO GKI scan, we may need to abort this way */
	//imsbr_esp_update_esphs((char *)esphs);
}

static void imsbr_handover_state(struct imsbr_msghdr *msg, unsigned long unused)
{
	struct handover_state *state;
	int new_ho = 0;
	u32 simcard;

	state = (struct handover_state *)msg->imsbr_payload;

	if (state->ho_type == IMSBR_HO_LTE2WIFI) {
		new_ho = IMSBR_HO_LTE2WIFI;
	} else if (state->ho_type == IMSBR_HO_WIFI2LTE) {
		new_ho = IMSBR_HO_WIFI2LTE;
	} else if (state->ho_type == IMSBR_HO_FINISH) {
		new_ho = IMSBR_HO_FINISH;
	} else {
		pr_info("state from cp: %d", state->ho_type);
	}
	simcard = state->sim_card;

	pr_info("state from cp: %d sim%d", new_ho, simcard);
	atomic_set(&imsbr_simcards[simcard].ho_state, new_ho);
}

static void imsbr_cptuple_reset(struct imsbr_msghdr *msg, unsigned long unused)
{
	u32 *sim_card;

	sim_card = (u32 *)msg->imsbr_payload;
	imsbr_flow_reset(IMSBR_FLOW_CPTUPLE, *sim_card, false);
}

static void imsbr_cp_reset(struct imsbr_msghdr *msg, unsigned long unused)
{
	imsbr_notify_ltevideo_apsk();
}

static void imsbr_echo_ping(struct imsbr_msghdr *msg, unsigned long unused)
{
	struct sblock blk;
	char *echostr;
	int echolen;
	int echolen_tmp;

	echostr = msg->imsbr_payload;
	echolen_tmp = strlen(echostr) + 1;
	echolen = min_t(int, strlen(echostr) + 1, IMSBR_MSG_MAXLEN);

	if (!imsbr_build_cmd("echo-pong", &blk, echostr, echolen))
		imsbr_sblock_send(&imsbr_ctrl, &blk, echolen);
}

static void imsbr_echo_pong(struct imsbr_msghdr *msg, unsigned long unused)
{
	pr_info("pong from cp: %s\n", msg->imsbr_payload);
}

#ifdef CONFIG_SPRD_IMS_BRIDGE_TEST

void call_core_function(struct call_c_function *ccf)
{
	ccf->cptuple_update = imsbr_cptuple_update;
	ccf->cp_sync_esp = imsbr_cp_sync_esp;
	ccf->handover_state = imsbr_handover_state;
	ccf->cptuple_reset = imsbr_cptuple_reset;
	ccf->cp_reset = imsbr_cp_reset;
	ccf->echo_ping = imsbr_echo_ping;
	ccf->echo_pong = imsbr_echo_pong;
}

#endif

static bool imsbr_msg_invalid(struct imsbr_msghdr *msghdr, int msglen)
{
	if (msghdr->imsbr_version != IMSBR_MSG_VERSION) {
		pr_err("version conflict! AP is %u but CP is %u!\n",
		       IMSBR_MSG_VERSION, msghdr->imsbr_version);
		return true;
	}

	if (msglen != msghdr->imsbr_paylen) {
		pr_err("format error, msglen=%d but payload length=%d\n",
		       msglen, msghdr->imsbr_paylen);
		return true;
	}

	return false;
}

void imsbr_process_msg(struct imsbr_sipc *sipc, struct sblock *blk,
		       bool freeit)
{
	struct imsbr_msghdr *msghdr;
	struct imsbr_msglist *list;
	char msgbuff[IMSBR_CTRL_BLKSZ];
	int msglen, blklen;
	char *cmd;
	int i;

	/* blk->length had been checked in imsbr_kthread */
	blklen = blk->length;
	unalign_memcpy(msgbuff, blk->addr, blklen);
	msghdr = (struct imsbr_msghdr *)msgbuff;
	msglen = blklen - sizeof(struct imsbr_msghdr);

	if (likely(freeit))
		imsbr_sblock_release(sipc, blk);

	if (imsbr_msg_invalid(msghdr, msglen))
		return;

	cmd = msghdr->imsbr_cmd;
	pr_debug("msghdr cmd=%s len=%d\n", cmd, msglen);

	for (i = 0; i < ARRAY_SIZE(imsbr_msglists); i++) {
		list = &imsbr_msglists[i];
		if (!strcmp(cmd, list->name)) {
			if (list->req_len > msghdr->imsbr_paylen) {
				int sz = list->req_len - msghdr->imsbr_paylen;

				memset(&msgbuff[blklen], 0, sz);
				pr_debug("req_len is %d, but paylen is %d, padding...\n",
					 list->req_len, msghdr->imsbr_paylen);
			}

			list->doit(msghdr, list->arg);
			break;
		}
	}

	if (i == ARRAY_SIZE(imsbr_msglists))
		pr_err("cmd %s is not supported now!\n", cmd);
}

static char *imsbr_ho_type2cmd(enum imsbr_ho_types type)
{
	switch (type) {
	case IMSBR_HO_LTE2WIFI:
		return "ho-lte2wifi";
	case IMSBR_HO_WIFI2LTE:
		return "ho-wifi2lte";
	case IMSBR_HO_FINISH:
		return "ho-finish";
	default:
		return "unknown";
	}
}

static char *imsbr_calls2cmd(enum imsbr_call_state state)
{
	switch (state) {
	case IMSBR_CALLS_END:
		return "call-end";
	case IMSBR_CALLS_VOWIFI:
		return "vowifi-call";
	case IMSBR_CALLS_VOLTE:
		return "volte-call";
	default:
		return "unknown";
	}
}

static void imsbr_trigger_handover(enum imsbr_ho_types type, u32 simcard)
{
	struct sblock blk;
	char *cmd;

	cmd = imsbr_ho_type2cmd(type);
	if (imsbr_build_cmd(cmd, &blk, &simcard, sizeof(simcard)))
		return;

	imsbr_sblock_send(&imsbr_ctrl, &blk, sizeof(simcard));

	pr_info("trigger handover [%s] sim%d\n", cmd, simcard);
}

static void imsbr_notify_callstate(enum imsbr_call_state state, u32 simcard)
{
	struct sblock blk;
	char *cmd;

	cmd = imsbr_calls2cmd(state);
	if (imsbr_build_cmd(cmd, &blk, &simcard, sizeof(simcard)))
		return;

	imsbr_sblock_send(&imsbr_ctrl, &blk, sizeof(simcard));
}

void imsbr_set_callstate(enum imsbr_call_state state, u32 simcard)
{
	int curr_ho, new_ho;

	mutex_lock(&imsbr_mutex);
	curr_ho = atomic_read(&imsbr_simcards[simcard].ho_state);

	if (state == IMSBR_CALLS_END) {
		/* Back to the primitive society :) */
		imsbr_simcards[simcard].init_call = state;
		imsbr_simcards[simcard].curr_call = state;
		new_ho = IMSBR_HO_FINISH;

		imsbr_notify_callstate(state, simcard);
	} else {
		if (imsbr_simcards[simcard].init_call == IMSBR_CALLS_END) {
			imsbr_simcards[simcard].init_call = state;
			imsbr_simcards[simcard].curr_call = state;
			new_ho = IMSBR_HO_FINISH;

			/* CP only need to know the init call state */
			imsbr_notify_callstate(state, simcard);
		} else {
			/**
			 * Imagine the following handover scenario:
			 * VoLTE->VoWiFi->VoLTE->VoWIFI->VoLTE->VoWIFI ...
			 * Let the ims bridge relax, handover becomes lte2wifi->
			 * finish->lte2wifi->finish->lte2wifi ...
			 */
			imsbr_simcards[simcard].curr_call = state;

			if (imsbr_simcards[simcard].init_call ==
			    imsbr_simcards[simcard].curr_call) {
				new_ho = IMSBR_HO_FINISH;
			} else {
				if (imsbr_simcards[simcard].init_call ==
				    IMSBR_CALLS_VOLTE)
					new_ho = IMSBR_HO_LTE2WIFI;
				else
					new_ho = IMSBR_HO_WIFI2LTE;
			}
		}
	}

	if (new_ho != curr_ho)
		imsbr_trigger_handover(new_ho, simcard);

	atomic_set(&imsbr_simcards[simcard].ho_state, new_ho);
	mutex_unlock(&imsbr_mutex);

	pr_info("call switch to [%s] init=%s curr=%s ho=%s->%s sim%d\n",
		imsbr_calls2cmd(state),
		imsbr_calls2cmd(imsbr_simcards[simcard].init_call),
		imsbr_calls2cmd(imsbr_simcards[simcard].curr_call),
		imsbr_ho_type2cmd(curr_ho),
		imsbr_ho_type2cmd(new_ho),
		simcard);
}

#ifdef CONFIG_PROC_FS
struct imsbr_flow_iter {
	struct seq_net_private p;
	unsigned int bucket;
};

static struct hlist_node *imsbr_flow_get_first(struct seq_file *seq)
{
	struct imsbr_flow_iter *iter = seq->private;
	struct hlist_node *n;

	for (iter->bucket = 0;
	     iter->bucket < IMSBR_FLOW_HSIZE;
	     iter->bucket++) {
		n = rcu_dereference(hlist_first_rcu(&imsbr_flow_bucket[iter->bucket]));
		if (n)
			return n;
	}

	return NULL;
}

static struct hlist_node *imsbr_flow_get_next(struct seq_file *seq,
					      struct hlist_node *head)
{
	struct imsbr_flow_iter *iter = seq->private;

	head = rcu_dereference(hlist_next_rcu(head));
	while (!head) {
		if (++iter->bucket >= IMSBR_FLOW_HSIZE)
			return NULL;

		head = rcu_dereference(hlist_first_rcu(&imsbr_flow_bucket[iter->bucket]));
	}

	return head;
}

static struct hlist_node *imsbr_flow_get_idx(struct seq_file *seq, loff_t pos)
{
	struct hlist_node *head = imsbr_flow_get_first(seq);

	if (head)
		while (pos && (head = imsbr_flow_get_next(seq, head)))
			pos--;

	return pos ? NULL : head;
}

static void *imsbr_flow_seq_start(struct seq_file *seq, loff_t *pos)
	__acquires(RCU)
{
	rcu_read_lock();
	return imsbr_flow_get_idx(seq, *pos);
}

static void *imsbr_flow_seq_next(struct seq_file *s, void *v, loff_t *pos)
{
	(*pos)++;
	return imsbr_flow_get_next(s, v);
}

static void imsbr_flow_seq_stop(struct seq_file *s, void *v)
	__releases(RCU)
{
	rcu_read_unlock();
}

static char *get_l3_name(u_int16_t l3num)
{
	switch (l3num) {
	case AF_INET:
		return "ipv4";
	case AF_INET6:
		return "ipv6";
	default:
		return "ukn";
	}
}

static char *get_l4_name(u_int8_t protonum)
{
	switch (protonum) {
	case IPPROTO_UDP:
		return "udp";
	case IPPROTO_TCP:
		return "tcp";
	case IPPROTO_ICMP:
		return "icmp";
	case IPPROTO_ESP:
		return "esp";
	case IPPROTO_AH:
		return "ah";
	default:
		return "ukn";
	}
}

static int imsbr_flow_seq_show(struct seq_file *s, void *v)
{
	struct nf_conntrack_tuple *nft;
	struct imsbr_flow *flow;
	u16 l3num;
	u8 protonum;

	flow = hlist_entry(v, struct imsbr_flow, hlist);
	nft = &flow->nft_tuple;

	l3num = nft->src.l3num;
	protonum = nft->dst.protonum;

	if (flow->flow_type == IMSBR_FLOW_CPTUPLE)
		seq_puts(s, "cptuple ");
	else
		seq_puts(s, "aptuple ");

	seq_printf(s, "%-5s %-2u %-5s %-2u ", get_l3_name(l3num), l3num,
		   get_l4_name(protonum), protonum);

	if (l3num == AF_INET) {
		seq_printf(s, "src=%pI4 dst=%pI4 ", &nft->src.u3.ip,
			   &nft->dst.u3.ip);
	} else {
		seq_printf(s, "src=%pI6c dst=%pI6c ", nft->src.u3.ip6,
			   nft->dst.u3.ip6);
	}

	seq_printf(s, "sport=%hu dport=%hu ", ntohs(nft->src.u.all),
		   ntohs(nft->dst.u.all));

	seq_printf(s, "%s sim%d%s%s\n",
		   imsbr_media_type2str(flow->media_type),
		   flow->sim_card,
		   imsbr_link_type2str(flow->link_type),
		   imsbr_socket_type2str(flow->socket_type));

	return 0;
}

static const struct seq_operations imsbr_flow_seq_ops = {
	.start = imsbr_flow_seq_start,
	.next  = imsbr_flow_seq_next,
	.stop  = imsbr_flow_seq_stop,
	.show  = imsbr_flow_seq_show
};

static void *imsbr_stat_start(struct seq_file *seq, loff_t *pos)
	__acquires(RCU)
{
	rcu_read_lock();
	return imsbr_flow_get_idx(seq, *pos);
}

static void *imsbr_stat_next(struct seq_file *s, void *v, loff_t *pos)
{
	(*pos)++;
	return imsbr_flow_get_next(s, v);
}

static void imsbr_stat_stop(struct seq_file *s, void *v)
	__releases(RCU)
{
	rcu_read_unlock();
}

static int imsbr_flow_open(struct inode *inode, struct file *file)
{
	return seq_open(file, &imsbr_flow_seq_ops);
}

static const struct file_operations imsbr_flow_fops = {
	.owner   = THIS_MODULE,
	.open    = imsbr_flow_open,
	.read    = seq_read,
	.llseek  = seq_lseek,
	.release = seq_release,
};

static int imsbr_stat_show(struct seq_file *s, void *v)
{
	struct imsbr_stat st, *ptr;
	int ho_s, i_call, c_call;
	int cpu, simcard;

	for (simcard = 0; simcard < IMSBR_SIMCARD_NUM; simcard++) {
		ho_s = atomic_read(&imsbr_simcards[simcard].ho_state);
		i_call = imsbr_simcards[simcard].init_call;
		c_call = imsbr_simcards[simcard].curr_call;
		seq_printf(s, "Sim%d call state info:\n", simcard);
		seq_printf(s, "  initcall=%s\n", imsbr_calls2cmd(i_call));
		seq_printf(s, "  currcall=%s\n", imsbr_calls2cmd(c_call));
		seq_printf(s, "  handover=%s\n", imsbr_ho_type2cmd(ho_s));
	}

	memset(&st, 0, sizeof(st));
	for (cpu = 0; cpu < nr_cpu_ids; cpu++) {
		if (!cpu_possible(cpu))
			continue;

		ptr = per_cpu_ptr(imsbr_stats, cpu);

		st.ip_output_fail += ptr->ip_output_fail;
		st.ip_route_fail += ptr->ip_route_fail;
		st.ip6_output_fail += ptr->ip6_output_fail;
		st.ip6_route_fail += ptr->ip6_route_fail;
		st.xfrm_lookup_fail += ptr->xfrm_lookup_fail;
		st.sk_buff_fail += ptr->sk_buff_fail;
		st.flow_duplicated += ptr->flow_duplicated;

		st.sipc_get_fail += ptr->sipc_get_fail;
		st.sipc_receive_fail += ptr->sipc_receive_fail;
		st.sipc_send_fail += ptr->sipc_send_fail;

		st.nfct_get_fail += ptr->nfct_get_fail;
		st.nfct_untracked += ptr->nfct_untracked;
		st.nfct_slow_path += ptr->nfct_slow_path;

		st.pkts_fromcp += ptr->pkts_fromcp;
		st.pkts_tocp += ptr->pkts_tocp;

		st.frag_create += ptr->frag_create;
		st.frag_ok += ptr->frag_ok;
		st.frag_fail += ptr->frag_fail;

		st.reasm_request += ptr->reasm_request;
		st.reasm_ok += ptr->reasm_ok;
		st.reasm_fail += ptr->reasm_fail;
	}

	seq_puts(s, "\nIms bridge stat info:\n");
	seq_printf(s, "  ip_output_fail=%llu\n", st.ip_output_fail);
	seq_printf(s, "  ip_route_fail=%llu\n", st.ip_route_fail);
	seq_printf(s, "  ip6_output_fail=%llu\n", st.ip6_output_fail);
	seq_printf(s, "  ip6_route_fail=%llu\n", st.ip6_route_fail);
	seq_printf(s, "  xfrm_lookup_fail=%llu\n", st.xfrm_lookup_fail);
	seq_printf(s, "  sk_buff_fail=%llu\n", st.sk_buff_fail);
	seq_printf(s, "  flow_duplicated=%llu\n", st.flow_duplicated);

	seq_printf(s, "  sipc_get_fail=%llu\n", st.sipc_get_fail);
	seq_printf(s, "  sipc_receive_fail=%llu\n", st.sipc_receive_fail);
	seq_printf(s, "  sipc_send_fail=%llu\n", st.sipc_send_fail);

	seq_printf(s, "  nfct_get_fail=%llu\n", st.nfct_get_fail);
	seq_printf(s, "  nfct_untracked=%llu\n", st.nfct_untracked);
	seq_printf(s, "  nfct_slow_path=%llu\n", st.nfct_slow_path);

	seq_printf(s, "  pkts_fromcp=%llu\n", st.pkts_fromcp);
	seq_printf(s, "  pkts_tocp=%llu\n", st.pkts_tocp);

	seq_printf(s, "  frag_create=%llu\n", st.frag_create);
	seq_printf(s, "  frag_ok=%llu\n", st.frag_ok);
	seq_printf(s, "  frag_fail=%llu\n", st.frag_fail);

	seq_printf(s, "  reasm_request=%llu\n", st.reasm_request);
	seq_printf(s, "  reasm_ok=%llu\n", st.reasm_ok);
	seq_printf(s, "  reasm_fail=%llu\n", st.reasm_fail);

	return 0;
}

static const struct seq_operations imsbr_stat_ops = {
	.start = imsbr_stat_start,
	.next  = imsbr_stat_next,
	.stop  = imsbr_stat_stop,
	.show  = imsbr_stat_show
};

static int imsbr_stat_open(struct inode *inode, struct file *file)
{
	return single_open(file, imsbr_stat_show, NULL);
}

static const struct file_operations imsbr_stat_fops = {
	.owner	 = THIS_MODULE,
	.open	 = imsbr_stat_open,
	.read	 = seq_read,
	.llseek	 = seq_lseek,
	.release = single_release,
};

static int imsbr_init_proc(struct net *net)
{
	if (!proc_create_net("imsbr_flow", 0444, net->proc_net,
			     &imsbr_flow_seq_ops,
			     sizeof(struct seq_net_private))) {
		pr_err("proc create imsbr_flow fail!\n");
		goto err_flow;
	}

	if (!proc_create_net("imsbr_stat", 0444, net->proc_net,
			     &imsbr_stat_ops,
			     sizeof(struct seq_net_private))) {
		pr_err("proc create imsbr_flow fail!\n");
		goto err_stat;
	}

	return 0;

err_stat:
	remove_proc_entry("imsbr_flow", net->proc_net);
err_flow:
	return -ENOMEM;
}

static void imsbr_fini_proc(struct net *net)
{
	remove_proc_entry("imsbr_stat", net->proc_net);
	remove_proc_entry("imsbr_flow", net->proc_net);
}
#else
static int imsbr_init_proc(struct net *net)
{
	return 0;
}

static void imsbr_fini_proc(struct net *net)
{
}
#endif /* CONFIG_PROC_FS */

static int __init imsbr_mempool_init(void)
{
	imsbr_flow_cache = kmem_cache_create("imsbr_flow",
					     sizeof(struct imsbr_flow),
					     0, 0, NULL);
	if (!imsbr_flow_cache) {
		pr_err("kmem_cache_create fail\n");
		return -ENOMEM;
	}

	imsbr_flow_pool = mempool_create(IMSBR_FLOW_MIN_NR, mempool_alloc_slab,
					 mempool_free_slab, imsbr_flow_cache);
	if (!imsbr_flow_pool) {
		kmem_cache_destroy(imsbr_flow_cache);
		pr_err("mempool_create fail\n");
		return -ENOMEM;
	}

	return 0;
}

static void imsbr_mempool_exit(void)
{
	mempool_destroy(imsbr_flow_pool);
	kmem_cache_destroy(imsbr_flow_cache);
}

int __init imsbr_core_init(void)
{
	int i;

	imsbr_hash_rnd = prandom_u32();
	atomic_set(&imsbr_enabled, 0);

	for (i = 0; i < IMSBR_SIMCARD_NUM; i++) {
		imsbr_simcards[i].init_call = IMSBR_CALLS_END;
		imsbr_simcards[i].curr_call = IMSBR_CALLS_END;

		atomic_set(&imsbr_simcards[i].ho_state, IMSBR_HO_FINISH);
	}

	imsbr_flow_bucket = kmalloc(IMSBR_FLOW_HSIZE *
			sizeof(struct hlist_head), GFP_KERNEL);
	if (!imsbr_flow_bucket)
		goto err_bucket;
	for (i = 0; i < IMSBR_FLOW_HSIZE; i++)
		INIT_HLIST_HEAD(&imsbr_flow_bucket[i]);

	imsbr_stats = alloc_percpu(struct imsbr_stat);
	if (!imsbr_stats)
		goto err_percpu;
	if (imsbr_mempool_init())
		goto err_mempool;

	if (imsbr_init_proc(&init_net))
		goto err_proc;

	return 0;

err_proc:
	imsbr_mempool_exit();
err_mempool:
	free_percpu(imsbr_stats);
err_percpu:
	kfree(imsbr_flow_bucket);
err_bucket:
	return -ENOMEM;
}

void imsbr_core_exit(void)
{
	int i;

	imsbr_fini_proc(&init_net);

	for (i = 0; i < IMSBR_SIMCARD_NUM; i++) {
		imsbr_flow_reset(IMSBR_FLOW_CPTUPLE, i, true);
		imsbr_flow_reset(IMSBR_FLOW_APTUPLE, i, true);
	}

	/* Wait for completion of call_rcu()'s */
	rcu_barrier();

	imsbr_mempool_exit();
	free_percpu(imsbr_stats);
	kfree(imsbr_flow_bucket);
}
