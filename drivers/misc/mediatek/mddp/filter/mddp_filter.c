// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include <linux/netfilter.h>
#include <linux/netfilter_ipv4.h>
#include <linux/netfilter_ipv6.h>
#include <net/arp.h>
#include <net/ip6_route.h>
#include <net/ip.h>
#include <net/ip6_checksum.h>
#include <net/ipv6.h>
#include <net/netfilter/nf_conntrack.h>
#include <net/netfilter/nf_conntrack_extend.h>
#include <net/route.h>
#include <linux/in6.h>
#include <linux/timer.h>

#include "mddp_ctrl.h"
#include "mddp_debug.h"
#include "mddp_dev.h"
#include "mddp_filter.h"
#include "mddp_f_config.h"
#include "mddp_f_desc.h"
#include "mddp_f_dev.h"
#include "mddp_f_proto.h"
#include "mddp_ipc.h"
#include "mtk_ccci_common.h"


#define USED_TIMEOUT 1

#define TRACK_TABLE_INIT_LOCK(TABLE) \
		spin_lock_init((&(TABLE).lock))
#define TRACK_TABLE_LOCK(TABLE, flags) \
		spin_lock_irqsave((&(TABLE).lock), (flags))
#define TRACK_TABLE_UNLOCK(TABLE, flags) \
		spin_unlock_irqrestore((&(TABLE).lock), (flags))

enum mddp_f_rule_tag_info_e {
	MDDP_RULE_TAG_NORMAL_PACKET = 0,
	MDDP_RULE_TAG_FAKE_DL_NAT_PACKET,
};

struct mddp_f_cb {
	struct net_device *wan;
	struct net_device *lan;
	u_int32_t src[4];	/* IPv4 use src[0] */
	u_int32_t dst[4];	/* IPv4 use dst[0] */
	u_int16_t sport;
	u_int16_t dport;
	u_int8_t proto;
	u_int8_t ip_ver;
	bool is_uplink;
};

#define MDDP_F_MAX_TRACK_NUM 512
#define MDDP_F_MAX_TRACK_TABLE_LIST 16
#define MDDP_F_TABLE_BUFFER_NUM 3000

static uint32_t mddp_netfilter_is_hook;
static struct net_device *mddp_wan_netdev;
static const struct net_device_ops *mddp_wan_netdev_ops_save;

//------------------------------------------------------------------------------
// Struct definition.
// -----------------------------------------------------------------------------
struct mddp_f_set_ct_timeout_req_t {
	uint32_t                udp_ct_timeout;
	uint32_t                tcp_ct_timeout;
	uint8_t                 rsv[4];
};

struct mddp_f_set_ct_timeout_rsp_t {
	uint32_t                udp_ct_timeout;
	uint32_t                tcp_ct_timeout;
	uint8_t                 result;
	uint8_t                 rsv[3];
};

//------------------------------------------------------------------------------
// Function prototype.
//------------------------------------------------------------------------------
static uint32_t mddp_nfhook_postrouting_v4
(void *priv, struct sk_buff *skb, const struct nf_hook_state *state);
static void mddp_nfhook_postrouting_v6(struct sk_buff *skb);

static int32_t mddp_f_init_nat_tuple(void);
static void mddp_f_uninit_nat_tuple(void);
static int32_t mddp_f_init_router_tuple(void);
static void mddp_f_uninit_router_tuple(void);
//------------------------------------------------------------------------------
// Registered callback function.
//------------------------------------------------------------------------------
static struct nf_hook_ops mddp_nf_ops[] __read_mostly = {
	{
		.hook           = mddp_nfhook_postrouting_v4,
		.pf             = NFPROTO_IPV4,
		.hooknum        = NF_INET_POST_ROUTING,
		.priority       = NF_IP_PRI_LAST,
	},
};

//------------------------------------------------------------------------------
// Private functions.
//------------------------------------------------------------------------------
/*
 *	Return Value: added extension tag length.
 */
static int mddp_f_e_tag_packet(
	struct sk_buff *skb,
	struct mddp_f_cb *cb,
	unsigned int hit_cnt)
{
	struct mddp_f_tag_packet_t *skb_tag;
	struct mddp_f_e_tag_common_t *skb_e_tag;
	struct mddp_f_e_tag_mac_t e_tag_mac;
	struct neighbour *neigh = NULL;
	int tag_len, etag_len;

	if (cb->ip_ver == 4)
		neigh = neigh_lookup(&arp_tbl, &ip_hdr(skb)->daddr, cb->lan);
	if (cb->ip_ver == 6)
		neigh = neigh_lookup(&nd_tbl, &ipv6_hdr(skb)->daddr, cb->lan);

	if ((neigh == NULL) || !(neigh->nud_state & NUD_VALID)) {
		MDDP_F_LOG(MDDP_LL_WARN,
			"%s: Add MDDP Etag Fail, neigh is null or nud_state is not VALID\n",
			__func__);
		if (neigh)
			neigh_release(neigh);
		return -1;
	}

	neigh_ha_snapshot(e_tag_mac.mac_addr, neigh, neigh->dev);
	if (neigh)
		neigh_release(neigh);
	e_tag_mac.access_cnt = hit_cnt;

	tag_len = sizeof(struct mddp_f_tag_packet_t);
	etag_len = sizeof(struct mddp_f_e_tag_common_t) +
			sizeof(struct mddp_f_e_tag_mac_t);

	/* extension tag for MAC address */
	skb_e_tag = (struct mddp_f_e_tag_common_t *)
		(skb_tail_pointer(skb) + tag_len);
	skb_e_tag->type = MDDP_E_TAG_MAC;
	skb_e_tag->len  = etag_len;

	memcpy(skb_e_tag->value, &e_tag_mac, sizeof(struct mddp_f_e_tag_mac_t));

	MDDP_F_LOG(MDDP_LL_INFO,
			"%s: Add MDDP Etag,  mac[%02x:%02x:%02x:%02x:%02x:%02x], access_cnt[%d]\n",
			__func__, e_tag_mac.mac_addr[0], e_tag_mac.mac_addr[1],
			e_tag_mac.mac_addr[2], e_tag_mac.mac_addr[3],
			e_tag_mac.mac_addr[4], e_tag_mac.mac_addr[5],
			e_tag_mac.access_cnt);

	skb_tag = (struct mddp_f_tag_packet_t *) skb_tail_pointer(skb);
	skb_tag->guard_pattern = MDDP_TAG_PATTERN;
	skb_tag->version = __MDDP_VERSION__;
	skb_tag->tag_len = tag_len + etag_len;
	skb_tag->v2.tag_info = MDDP_RULE_TAG_FAKE_DL_NAT_PACKET;
	skb_tag->v2.lan_netif_id = mddp_f_dev_to_netif_id(cb->lan);
	if (cb->is_uplink == true) {
		skb_tag->v2.port = cb->sport;
		skb_tag->v2.ip = cb->src[0];  /* Don't care IPv6 IP */
		MDDP_F_LOG(MDDP_LL_NOTICE,
				"%s: Add MDDP UL tag, lan_netif_id[%x], port[%x], ip[%x], skb[%p].\n",
				__func__, skb_tag->v2.lan_netif_id,
				skb_tag->v2.port, skb_tag->v2.ip, skb);
	} else {
		skb_tag->v2.port = cb->dport;
		skb_tag->v2.ip = cb->dst[0];  /* Don't care IPv6 IP */
		MDDP_F_LOG(MDDP_LL_NOTICE,
				"%s: Add MDDP DL tag, lan_netif_id[%x], port[%x], ip[%x], skb[%p].\n",
				__func__, skb_tag->v2.lan_netif_id,
				skb_tag->v2.port, skb_tag->v2.ip, skb);
	}
	__skb_put(skb, skb_tag->tag_len);

	mddp_enqueue_dstate(MDDP_DSTATE_ID_NEW_TAG,
				skb_tag->v2.ip, skb_tag->v2.port);

	return 0;
}

static struct sk_buff *mddp_f_skb_tag(struct sk_buff *skb, struct mddp_f_cb *cb)
{
	uint16_t port;
	int len, ntail;
	struct sk_buff *fake_skb;
	struct tcphdr *tcph;
	struct udphdr *udph;

	ntail = sizeof(struct mddp_f_tag_packet_t) +
		sizeof(struct mddp_f_e_tag_common_t) +
		sizeof(struct mddp_f_e_tag_mac_t);

	fake_skb = netdev_alloc_skb(cb->wan, cb->wan->mtu);
	if (!fake_skb)
		return NULL;
	len = skb->len;
	if (skb->len > (cb->wan->mtu - ntail))
		len = cb->wan->mtu - ntail;
	__skb_put_data(fake_skb, skb_network_header(skb), len);
	skb_reset_mac_header(fake_skb);
	skb_reset_network_header(fake_skb);

	if (cb->ip_ver == 4) {
		struct iphdr *iph;

		iph = ip_hdr(fake_skb);
		if (cb->is_uplink) {
			iph->daddr = cb->src[0];
			cb->src[0] = iph->saddr;
			iph->saddr = cb->dst[0];
		}
		iph->tot_len = htons(fake_skb->len);
		iph->check = 0;
		len = iph->ihl * 4;
		skb_set_transport_header(fake_skb, len);

		if (cb->proto == IPPROTO_UDP) {
			udph = (void *)(skb_network_header(fake_skb) + len);
			if (cb->is_uplink) {
				port = udph->dest;
				udph->dest = cb->sport;
				cb->sport = udph->source;
				udph->source = port;
			}
			udph->len = htons(fake_skb->len - len);
		}
		if (cb->proto == IPPROTO_TCP) {
			tcph = (void *)(skb_network_header(fake_skb) + len);
			if (cb->is_uplink) {
				port = tcph->dest;
				tcph->dest = cb->sport;
				cb->sport = tcph->source;
				tcph->source = port;
			}
		}
	}
	if (cb->ip_ver == 6) {
		struct in6_addr addr;
		struct ipv6hdr *iph;

		iph = ipv6_hdr(fake_skb);
		if (cb->is_uplink) {
			addr = iph->saddr;
			iph->saddr = iph->daddr;
			iph->daddr = addr;
		}
		len = sizeof(struct ipv6hdr);
		iph->payload_len = htons(fake_skb->len - len);
		skb_set_transport_header(fake_skb, len);

		if (cb->proto == IPPROTO_UDP) {
			udph = (void *)(skb_network_header(fake_skb) + len);
			if (cb->is_uplink) {
				port = udph->source;
				udph->source = udph->dest;
				udph->dest = port;
			}
			udph->len = htons(fake_skb->len - len);
		}

		if (cb->proto == IPPROTO_TCP) {
			tcph = (void *)(skb_network_header(fake_skb) + len);
			if (cb->is_uplink) {
				port = tcph->source;
				tcph->source = tcph->dest;
				tcph->dest = port;
			}
		}
	}

	fake_skb->priority = skb->priority;
	fake_skb->protocol = skb->protocol;
	fake_skb->pkt_type = skb->pkt_type;

	return fake_skb;
}

static int mddp_f_tag_packet(
	struct sk_buff *skb,
	struct mddp_f_cb *cb,
	unsigned int hit_cnt)
{
	struct sk_buff *fake_skb;
	int ret;

	fake_skb = mddp_f_skb_tag(skb, cb);
	if (fake_skb == NULL) {
		MDDP_F_LOG(MDDP_LL_NOTICE, "%s: skb_copy() failed\n", __func__);
		return -ENOMEM;
	}

	/* Add Extension tag */
	ret = mddp_f_e_tag_packet(fake_skb, cb, hit_cnt);
	if (ret < 0) {
		dev_kfree_skb(fake_skb);
		return -EFAULT;
	}

	if (cb->wan->netdev_ops->ndo_start_xmit(fake_skb, cb->wan) != NETDEV_TX_OK)
		dev_kfree_skb(fake_skb);

	return 0;
}

static int32_t mddp_ct_update(void *buf, uint32_t buf_len)
{
	struct mddp_ct_timeout_ind_t   *ct_ind;
	struct mddp_ct_nat_table_t     *entry;
	uint32_t                        read_cnt = 0;
	uint32_t                        i;

	ct_ind = (struct mddp_ct_timeout_ind_t *)buf;
	read_cnt = sizeof(ct_ind->entry_num);

	for (i = 0; i < ct_ind->entry_num; i++) {
		entry = &(ct_ind->nat_table[i]);
		read_cnt += sizeof(struct mddp_ct_nat_table_t);
		if (read_cnt > buf_len) {
			MDDP_F_LOG(MDDP_LL_NOTICE,
					"%s: Invalid buf_len(%u), i(%u), read_cnt(%u)!\n",
					__func__, buf_len, i, read_cnt);
			break;
		}

		MDDP_F_LOG(MDDP_LL_INFO,
				"%s: Update conntrack private(%u.%u.%u.%u:%u), target(%u.%u.%u.%u:%u), public(%u.%u.%u.%u:%u)\n",
				__func__,
				entry->private_ip[0], entry->private_ip[1],
				entry->private_ip[2], entry->private_ip[3],
				entry->private_port,
				entry->target_ip[0], entry->target_ip[1],
				entry->target_ip[2], entry->target_ip[3],
				entry->target_port,
				entry->public_ip[0], entry->public_ip[1],
				entry->public_ip[2], entry->public_ip[3],
				entry->public_port);

		// Send IND to upper module.
		mddp_dev_response(MDDP_APP_TYPE_ALL,
			MDDP_CMCMD_CT_IND,
			true,
			(uint8_t *)entry,
			sizeof(struct mddp_ct_nat_table_t));
	}

	return 0;
}

//------------------------------------------------------------------------------
// Public functions.
//------------------------------------------------------------------------------
void mddp_f_wan_netdev_set(struct net_device *netdev)
{
	mddp_wan_netdev = netdev;
}

int32_t mddp_f_msg_hdlr(uint32_t msg_id, void *buf, uint32_t buf_len)
{
	int32_t                                 ret = 0;

	switch (msg_id) {
	case IPC_MSG_ID_DPFM_CT_TIMEOUT_IND:
		mddp_ct_update(buf, buf_len);
		ret = 0;
		break;

	case IPC_MSG_ID_DPFM_SET_CT_TIMEOUT_VALUE_RSP:
		ret = 0;
		break;

	default:
		MDDP_F_LOG(MDDP_LL_NOTICE,
				"%s: Unaccepted msg_id(%d)!\n",
				__func__, msg_id);
		ret = -EINVAL;
		break;
	}

	return ret;
}

int32_t mddp_f_set_ct_value(uint8_t *buf, uint32_t buf_len)
{
	uint32_t                                md_status;
	struct mddp_md_msg_t                   *md_msg;
	struct mddp_dev_req_set_ct_value_t     *in_req;
	struct mddp_f_set_ct_timeout_req_t      ct_req;
	struct mddp_app_t *app;

	if (buf_len != sizeof(struct mddp_dev_req_set_ct_value_t)) {
		MDDP_F_LOG(MDDP_LL_NOTICE,
				"%s: Invalid parameter, buf_len(%d)!\n",
				__func__, buf_len);
		return -EINVAL;
	}

	md_status = exec_ccci_kern_func_by_md_id(0, ID_GET_MD_STATE, NULL, 0);

	if (md_status != MD_STATE_READY) {
		MDDP_F_LOG(MDDP_LL_NOTICE,
				"%s: Invalid state, md_status(%d)!\n",
				__func__, md_status);
		return -ENODEV;
	}

	md_msg = kzalloc(sizeof(struct mddp_md_msg_t) + sizeof(ct_req),
			GFP_ATOMIC);
	if (unlikely(!md_msg))
		return -EAGAIN;

	in_req = (struct mddp_dev_req_set_ct_value_t *)buf;

	memset(&ct_req, 0, sizeof(ct_req));
	ct_req.tcp_ct_timeout = in_req->tcp_ct_timeout;
	ct_req.udp_ct_timeout = in_req->udp_ct_timeout;

	md_msg->msg_id = IPC_MSG_ID_DPFM_SET_CT_TIMEOUT_VALUE_REQ;
	md_msg->data_len = sizeof(ct_req);
	memcpy(md_msg->data, &ct_req, sizeof(ct_req));
	app = mddp_get_app_inst(MDDP_APP_TYPE_WH);
	mddp_ipc_send_md(app, md_msg, MDFPM_USER_ID_DPFM);

	return 0;
}

//------------------------------------------------------------------------------
// Kernel functions.
//------------------------------------------------------------------------------
static int mddp_netops_open(struct net_device *dev)
{
	return mddp_wan_netdev_ops_save->ndo_open(dev);
}

static int mddp_netops_stop(struct net_device *dev)
{
	return mddp_wan_netdev_ops_save->ndo_stop(dev);
}

static netdev_tx_t mddp_netops_start_xmit(struct sk_buff *skb, struct net_device *dev)
{
	if (skb->protocol == htons(ETH_P_IPV6))
		mddp_nfhook_postrouting_v6(skb);

	return mddp_wan_netdev_ops_save->ndo_start_xmit(skb, dev);
}

static void mddp_netops_tx_timeout(struct net_device *dev)
{
	mddp_wan_netdev_ops_save->ndo_tx_timeout(dev);
}

static int mddp_netops_do_ioctl(struct net_device *dev, struct ifreq *ifr, int cmd)
{
	return mddp_wan_netdev_ops_save->ndo_do_ioctl(dev, ifr, cmd);
}

static int mddp_netops_change_mtu(struct net_device *dev, int new_mtu)
{
	return mddp_wan_netdev_ops_save->ndo_change_mtu(dev, new_mtu);
}

static u16 mddp_netops_select_queue(struct net_device *dev, struct sk_buff *skb,
				    struct net_device *sb_dev, select_queue_fallback_t fallback)
{
	return mddp_wan_netdev_ops_save->ndo_select_queue(dev, skb, sb_dev, fallback);
}

static const struct net_device_ops mddp_wan_netdev_ops = {
	.ndo_open	= mddp_netops_open,
	.ndo_stop	= mddp_netops_stop,
	.ndo_start_xmit	= mddp_netops_start_xmit,
	.ndo_tx_timeout	= mddp_netops_tx_timeout,
	.ndo_do_ioctl	= mddp_netops_do_ioctl,
	.ndo_change_mtu	= mddp_netops_change_mtu,
	.ndo_select_queue = mddp_netops_select_queue,
};

static int __net_init mddp_nf_register(struct net *net)
{
	return nf_register_net_hooks(net, mddp_nf_ops,
					ARRAY_SIZE(mddp_nf_ops));
}

static void __net_exit mddp_nf_unregister(struct net *net)
{
	nf_unregister_net_hooks(net, mddp_nf_ops,
					ARRAY_SIZE(mddp_nf_ops));
}

static struct pernet_operations mddp_net_ops = {
	.init = mddp_nf_register,
	.exit = mddp_nf_unregister,
};


void mddp_netfilter_hook(void)
{
	if (mddp_netfilter_is_hook == 0) {
		int ret = 0;

		ret = register_pernet_subsys(&mddp_net_ops);
		if (ret < 0) {
			MDDP_F_LOG(MDDP_LL_NOTICE,
					"%s: Cannot register hooks(%d)!\n",
					__func__, ret);
		} else {
			netif_tx_disable(mddp_wan_netdev);
			mddp_wan_netdev_ops_save = mddp_wan_netdev->netdev_ops;
			mddp_wan_netdev->netdev_ops = &mddp_wan_netdev_ops;
			netif_tx_wake_all_queues(mddp_wan_netdev);
			mddp_netfilter_is_hook = 1;
		}
	}
}

void mddp_netfilter_unhook(void)
{
	if (mddp_netfilter_is_hook == 1) {
		unregister_pernet_subsys(&mddp_net_ops);
		netif_tx_disable(mddp_wan_netdev);
		mddp_wan_netdev->netdev_ops = mddp_wan_netdev_ops_save;
		netif_tx_wake_all_queues(mddp_wan_netdev);
		mddp_netfilter_is_hook = 0;
	}
}

int32_t mddp_filter_init(void)
{
	int ret = 0;

	ret = mddp_f_init_nat_tuple();
	if (ret < 0) {
		MDDP_F_LOG(MDDP_LL_NOTICE,
				"%s: Cannot init nat tuple(%d)!\n",
				__func__, ret);
		return ret;
	}

	ret = mddp_f_init_router_tuple();
	if (ret < 0) {
		mddp_f_uninit_nat_tuple();
		MDDP_F_LOG(MDDP_LL_NOTICE,
				"%s: Cannot init router tuple(%d)!\n",
				__func__, ret);
		return ret;
	}

	return 0;
}

static atomic_t mddp_filter_quit = ATOMIC_INIT(0);
void mddp_filter_uninit(void)
{
	mddp_netfilter_unhook();
	atomic_set(&mddp_filter_quit, 1);
	mddp_f_uninit_nat_tuple();
	mddp_f_uninit_router_tuple();
}

#include "mddp_filter_v4.c"
#include "mddp_filter_v6.c"
