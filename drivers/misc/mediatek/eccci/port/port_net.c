// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2016 MediaTek Inc.
 */
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/ip.h>
#include <linux/tcp.h>
#include <linux/ipv6.h>
#include <net/ipv6.h>
#include <linux/skbuff.h>
#include <linux/module.h>
#include <linux/timer.h>
#include <linux/version.h>
#include <linux/sockios.h>
#include "mt-plat/mtk_ccci_common.h"
#include "ccci_config.h"
#include "ccci_common_config.h"
#include "ccci_core.h"
#include "ccci_bm.h"
#include "ccci_modem.h"
#include "port_net.h"
#include "ccci_hif.h"

#ifdef PORT_NET_TRACE
#define CREATE_TRACE_POINTS
#include "port_net_events.h"
#endif
#include "ccmni.h"

#define NET_ACK_TXQ_INDEX(p) ((p)->txq_exp_index&0x0F)
#define GET_CCMNI_IDX(p) ((p)->minor - CCCI_NET_MINOR_BASE)

/* now we only support MBIM Tx/Rx in CCMNI_U context */
static atomic_t mbim_ccmni_index[MAX_MD_NUM];

int ccci_get_ccmni_channel(int md_id, int ccmni_idx, struct ccmni_ch *channel)
{
	int ret = 0;

	switch (ccmni_idx) {
	case 0:
		channel->rx = CCCI_CCMNI1_RX;
		channel->rx_ack = 0xFF;
		channel->tx = CCCI_CCMNI1_TX;
		channel->tx_ack = 0xFF;
		channel->dl_ack = CCCI_CCMNI1_DL_ACK;
		channel->multiq = md_id == MD_SYS1 ? 1 : 0;
		break;
	case 1:
		channel->rx = CCCI_CCMNI2_RX;
		channel->rx_ack = 0xFF;
		channel->tx = CCCI_CCMNI2_TX;
		channel->tx_ack = 0xFF;
		channel->dl_ack = CCCI_CCMNI2_DL_ACK;
		channel->multiq = md_id == MD_SYS1 ? 1 : 0;
		break;
	case 2:
		channel->rx = CCCI_CCMNI3_RX;
		channel->rx_ack = 0xFF;
		channel->tx = CCCI_CCMNI3_TX;
		channel->tx_ack = 0xFF;
		channel->dl_ack = CCCI_CCMNI3_TX;
		channel->multiq = md_id == MD_SYS1 ? 1 : 0;
		break;
	case 3:
		channel->rx = CCCI_CCMNI4_RX;
		channel->rx_ack = 0xFF;
		channel->tx = CCCI_CCMNI4_TX;
		channel->tx_ack = 0xFF;
		channel->dl_ack = CCCI_CCMNI4_TX;
		channel->multiq = md_id == MD_SYS1 ? 1 : 0;
		break;
	case 4:
		channel->rx = CCCI_CCMNI5_RX;
		channel->rx_ack = 0xFF;
		channel->tx = CCCI_CCMNI5_TX;
		channel->tx_ack = 0xFF;
		channel->dl_ack = CCCI_CCMNI5_TX;
		channel->multiq = md_id == MD_SYS1 ? 1 : 0;
		break;
	case 5:
		channel->rx = CCCI_CCMNI6_RX;
		channel->rx_ack = 0xFF;
		channel->tx = CCCI_CCMNI6_TX;
		channel->tx_ack = 0xFF;
		channel->dl_ack = CCCI_CCMNI6_TX;
		channel->multiq = md_id == MD_SYS1 ? 1 : 0;
		break;
	case 6:
		channel->rx = CCCI_CCMNI7_RX;
		channel->rx_ack = 0xFF;
		channel->tx = CCCI_CCMNI7_TX;
		channel->tx_ack = 0xFF;
		channel->dl_ack = CCCI_CCMNI7_TX;
		channel->multiq = md_id == MD_SYS1 ? 1 : 0;
		break;
	case 7:
		channel->rx = CCCI_CCMNI8_RX;
		channel->rx_ack = 0xFF;
		channel->tx = CCCI_CCMNI8_TX;
		channel->tx_ack = 0xFF;
		channel->dl_ack = CCCI_CCMNI8_DLACK_RX;
		channel->multiq = md_id == MD_SYS1 ? 1 : 0;
		break;
	case 8: /* a replica for ccmni-lan, so should not be used */
		channel->rx = CCCI_INVALID_CH_ID;
		channel->rx_ack = 0xFF;
		channel->tx = CCCI_INVALID_CH_ID;
		channel->tx_ack = 0xFF;
		channel->dl_ack = CCCI_INVALID_CH_ID;
		channel->multiq = 0;
		break;
	case 9:
		channel->rx = CCCI_CCMNI10_RX;
		channel->rx_ack = 0xFF;
		channel->tx = CCCI_CCMNI10_TX;
		channel->tx_ack = 0xFF;
		channel->dl_ack = CCCI_CCMNI10_TX;
		channel->multiq = md_id == MD_SYS1 ? 1 : 0;
		break;
	case 10:
		channel->rx = CCCI_CCMNI11_RX;
		channel->rx_ack = 0xFF;
		channel->tx = CCCI_CCMNI11_TX;
		channel->tx_ack = 0xFF;
		channel->dl_ack = CCCI_CCMNI11_TX;
		channel->multiq = md_id == MD_SYS1 ? 1 : 0;
		break;
	case 11:
		channel->rx = CCCI_CCMNI12_RX;
		channel->rx_ack = 0xFF;
		channel->tx = CCCI_CCMNI12_TX;
		channel->tx_ack = 0xFF;
		channel->dl_ack = CCCI_CCMNI12_TX;
		channel->multiq = md_id == MD_SYS1 ? 1 : 0;
		break;
	case 12:
		channel->rx = CCCI_CCMNI13_RX;
		channel->rx_ack = 0xFF;
		channel->tx = CCCI_CCMNI13_TX;
		channel->tx_ack = 0xFF;
		channel->dl_ack = CCCI_CCMNI13_TX;
		channel->multiq = md_id == MD_SYS1 ? 1 : 0;
		break;
	case 13:
		channel->rx = CCCI_CCMNI14_RX;
		channel->rx_ack = 0xFF;
		channel->tx = CCCI_CCMNI14_TX;
		channel->tx_ack = 0xFF;
		channel->dl_ack = CCCI_CCMNI14_TX;
		channel->multiq = md_id == MD_SYS1 ? 1 : 0;
		break;
	case 14:
		channel->rx = CCCI_CCMNI15_RX;
		channel->rx_ack = 0xFF;
		channel->tx = CCCI_CCMNI15_TX;
		channel->tx_ack = 0xFF;
		channel->dl_ack = CCCI_CCMNI15_TX;
		channel->multiq = md_id == MD_SYS1 ? 1 : 0;
		break;
	case 15:
		channel->rx = CCCI_CCMNI16_RX;
		channel->rx_ack = 0xFF;
		channel->tx = CCCI_CCMNI16_TX;
		channel->tx_ack = 0xFF;
		channel->dl_ack = CCCI_CCMNI16_TX;
		channel->multiq = md_id == MD_SYS1 ? 1 : 0;
		break;
	case 16:
		channel->rx = CCCI_CCMNI17_RX;
		channel->rx_ack = 0xFF;
		channel->tx = CCCI_CCMNI17_TX;
		channel->tx_ack = 0xFF;
		channel->dl_ack = CCCI_CCMNI17_TX;
		channel->multiq = md_id == MD_SYS1 ? 1 : 0;
		break;
	case 17:
		channel->rx = CCCI_CCMNI18_RX;
		channel->rx_ack = 0xFF;
		channel->tx = CCCI_CCMNI18_TX;
		channel->tx_ack = 0xFF;
		channel->dl_ack = CCCI_CCMNI18_TX;
		channel->multiq = md_id == MD_SYS1 ? 1 : 0;
		break;
	case 18:
		channel->rx = CCCI_CCMNI19_RX;
		channel->rx_ack = 0xFF;
		channel->tx = CCCI_CCMNI19_TX;
		channel->tx_ack = 0xFF;
		channel->dl_ack = CCCI_CCMNI19_TX;
		channel->multiq = md_id == MD_SYS1 ? 1 : 0;
		break;
	case 19:
		channel->rx = CCCI_CCMNI20_RX;
		channel->rx_ack = 0xFF;
		channel->tx = CCCI_CCMNI20_TX;
		channel->tx_ack = 0xFF;
		channel->dl_ack = CCCI_CCMNI20_TX;
		channel->multiq = md_id == MD_SYS1 ? 1 : 0;
		break;
	case 20:
		channel->rx = CCCI_CCMNI21_RX;
		channel->rx_ack = 0xFF;
		channel->tx = CCCI_CCMNI21_TX;
		channel->tx_ack = 0xFF;
		channel->dl_ack = CCCI_CCMNI21_TX;
		channel->multiq = md_id == MD_SYS1 ? 1 : 0;
		break;
	case 21: /* CCMIN-LAN should always be the last one*/
		channel->rx = CCCI_CCMNILAN_RX;
		channel->rx_ack = 0xFF;
		channel->tx = CCCI_CCMNILAN_TX;
		channel->tx_ack = 0xFF;
		channel->dl_ack = CCCI_CCMNILAN_DLACK_RX;
		channel->multiq = 0;
		break;
	default:
		CCCI_ERROR_LOG(md_id, NET,
			"invalid ccmni index=%d\n", ccmni_idx);
		ret = -1;
		break;
	}

	return ret;
}

static struct port_t *find_net_port_by_ccmni_idx(int md_id, int ccmni_idx)
{
	return port_get_by_minor(md_id, ccmni_idx + CCCI_NET_MINOR_BASE);
}

int ccmni_send_pkt(int md_id, int ccmni_idx, void *data, int is_ack)
{
	struct port_t *port = NULL;
	/* struct ccci_request *req = NULL; */
	struct ccci_header *ccci_h = NULL;
	struct sk_buff *skb = (struct sk_buff *)data;
	struct ccmni_ch *channel = ccmni_ops.get_ch(md_id, ccmni_idx);
	int tx_ch = is_ack ? channel->dl_ack : channel->tx;
	int ret;
#ifdef PORT_NET_TRACE
	unsigned long long send_time = 0;
	unsigned long long get_port_time = 0;
	unsigned long long total_time = 0;

	total_time = sched_clock();
#endif

#ifdef PORT_NET_TRACE
	get_port_time = sched_clock();
#endif
	port = find_net_port_by_ccmni_idx(md_id, ccmni_idx);
#ifdef PORT_NET_TRACE
	get_port_time = sched_clock() - get_port_time;
#endif
	if (!port) {
		CCCI_ERROR_LOG(0, NET, "port is NULL for CCMNI%d\n", ccmni_idx);
		return CCMNI_ERR_TX_INVAL;
	}

	ccci_h = (struct ccci_header *)skb_push(skb,
		sizeof(struct ccci_header));
	ccci_h = (struct ccci_header *)skb->data;
	ccci_h->channel = tx_ch;
	ccci_h->data[0] = ccmni_idx;
	/* as skb->len already included ccci_header after skb_push */
	ccci_h->data[1] = skb->len;
	ccci_h->reserved = 0;

	CCCI_DEBUG_LOG(md_id, NET,
		"port %s send: %08X, %08X, %08X, %08X\n", port->name,
		ccci_h->data[0], ccci_h->data[1], ccci_h->channel,
		ccci_h->reserved);
#ifdef PORT_NET_TRACE
	send_time = sched_clock();
#endif
	ret = port_net_send_skb_to_md(port, is_ack, skb);
#ifdef PORT_NET_TRACE
	send_time = sched_clock() - send_time;
#endif
	if (ret) {
		skb_pull(skb, sizeof(struct ccci_header));
		/* undo header, in next retry,
		 * we'll reserve header again
		 */
		ret = CCMNI_ERR_TX_BUSY;
	} else {
		ret = CCMNI_ERR_TX_OK;
	}
#ifdef PORT_NET_TRACE
	if (ret == CCMNI_ERR_TX_OK) {
		total_time = sched_clock() - total_time;
		trace_port_net_tx(md_id, -1, tx_ch,
			(unsigned int)get_port_time,
			(unsigned int)send_time,
			(unsigned int)(total_time));
	} else {
		trace_port_net_error(port->md_id, -1, tx_ch,
			port->tx_busy_count, __LINE__);
	}
#endif
	return ret;
}

int ccmni_napi_poll(int md_id, int ccmni_idx,
	struct napi_struct *napi, int weight)
{
	CCCI_ERROR_LOG(md_id, NET,
		"ccmni%d NAPI is not supported\n", ccmni_idx);
	return -ENODEV;
}

struct ccmni_ccci_ops eccci_ccmni_ops = {
	.ccmni_ver = CCMNI_DRV_V0,
	.ccmni_num = 21,
	.name = "ccmni",
	.md_ability = MODEM_CAP_DATA_ACK_DVD | MODEM_CAP_CCMNI_MQ
		| MODEM_CAP_DIRECT_TETHERING, /* CCCI_KMODULE_ENABLE: todo */
	.irat_md_id = -1,
	.napi_poll_weigh = NAPI_POLL_WEIGHT,
	.send_pkt = ccmni_send_pkt,
	.napi_poll = ccmni_napi_poll,
	.get_ccmni_ch = ccci_get_ccmni_channel,
	.ccci_net_init = mtk_ccci_net_port_init,
	.ccci_handle_port_list = mtk_ccci_handle_port_list,
};

struct ccmni_ccci_ops eccci_cc3mni_ops = {
	.ccmni_ver = CCMNI_DRV_V0,
	.ccmni_num = 21,
	.name = "cc3mni",
	.md_ability = MODEM_CAP_CCMNI_IRAT | MODEM_CAP_TXBUSY_STOP,
	.irat_md_id = MD_SYS1,
	.napi_poll_weigh = 0,
	.send_pkt = ccmni_send_pkt,
	.napi_poll = ccmni_napi_poll,
	.get_ccmni_ch = ccci_get_ccmni_channel,
};

int ccmni_send_mbim_skb(int md_id, struct sk_buff *skb)
{
	int mbim_ccmni_current;
	int is_ack = 0;

	if (md_id < 0 || md_id >= MAX_MD_NUM) {
		CCCI_ERROR_LOG(md_id, NET, "invalid MD id=%d\n", md_id);
		return -EINVAL;
	}

	mbim_ccmni_current = atomic_read(&mbim_ccmni_index[md_id]);
	if (mbim_ccmni_current == -1)
		return -EPERM;

	is_ack = ccmni_ops.is_ack_skb(md_id, skb);

	return ccmni_send_pkt(md_id, mbim_ccmni_current, skb, is_ack);
}

void ccmni_update_mbim_interface(int md_id, int id)
{
	if (md_id < 0 || md_id >= MAX_MD_NUM) {
		CCCI_ERROR_LOG(md_id, NET, "invalid MD id=%d\n", md_id);
		return;
	}

	atomic_set(&mbim_ccmni_index[md_id], id);
	CCCI_NORMAL_LOG(md_id, NET, "MBIM interface id=%d\n", id);
}

static int port_net_init(struct port_t *port)
{
	int md_id = port->md_id;
	struct ccci_per_md *per_md_data = ccci_get_per_md_data(md_id);

	if (port->md_id < 0 || port->md_id >= MAX_MD_NUM) {
		CCCI_ERROR_LOG(md_id, NET, "invalid MD id=%d\n", port->md_id);
		return -EINVAL;
	}
	port->minor += CCCI_NET_MINOR_BASE;
	if (port->rx_ch == CCCI_CCMNI1_RX) {
		atomic_set(&mbim_ccmni_index[port->md_id], -1);

		eccci_ccmni_ops.md_ability |= per_md_data->md_capability;
#if defined CONFIG_MTK_MD3_SUPPORT
		CCCI_INIT_LOG(port->md_id, NET,
			"clear MODEM_CAP_SGIO flag for IRAT enable\n");
		eccci_ccmni_ops.md_ability &= (~(MODEM_CAP_SGIO));
#endif
		if (port->md_id == MD_SYS1)
			ccmni_ops.init(md_id, &eccci_ccmni_ops);
		else if (port->md_id == MD_SYS3)
			ccmni_ops.init(md_id, &eccci_cc3mni_ops);
	}
	return 0;
}

#ifdef CCCI_KMODULE_ENABLE
int mbim_start_xmit(struct sk_buff *skb, int ifid)
{
	pr_debug("[ccci/dummy] %s is not supported!\n", __func__);
	return 0;
}
#endif

static void recv_from_port_list(struct port_t *port)
{
	unsigned long flags;
	struct sk_buff *skb = NULL;

	spin_lock_irqsave(&port->port_rx_list.lock, flags);
	skb = __skb_dequeue(&port->port_rx_list);
	if (skb == NULL) {
		spin_unlock_irqrestore(&port->port_rx_list.lock, flags);
		return;
	}
	spin_unlock_irqrestore(&port->port_rx_list.lock, flags);
	port_ch_dump(port, 0, skb, skb->len);
	ccmni_ops.rx_callback(port->md_id, GET_CCMNI_IDX(port), skb, NULL);
}

int mtk_ccci_handle_port_list(int status, char *name)
{
	int ret = 0, channel;
	unsigned long flags;
	struct port_t *port = NULL;
	struct sk_buff *skb = NULL;

	channel = mtk_ccci_request_port(name);
	ret = find_port_by_channel(channel, &port);
	if (ret)
		return -1;
	if (status)
		atomic_set(&port->is_up, 1);
	else {
		atomic_set(&port->is_up, 0);
		spin_lock_irqsave(&port->port_rx_list.lock, flags);
		while ((skb = __skb_dequeue(&port->port_rx_list))
			!= NULL)
			dev_kfree_skb_any(skb);
		spin_unlock_irqrestore(&port->port_rx_list.lock, flags);
		return ret;
	}
	while (!skb_queue_empty(&port->port_rx_list))
		recv_from_port_list(port);
	return ret;
}

static void ccmni_queue_recv_skb(struct port_t *port, struct sk_buff *skb)
{
	unsigned long flags;

	if (atomic_read(&port->is_up)) {
		while (!skb_queue_empty(&port->port_rx_list))
			recv_from_port_list(port);

		/*The packet may be out of order when ccmni is up at the*/
		/* same time, it will be correctly handled by TCP stack.*/
		ccmni_ops.rx_callback(port->md_id, GET_CCMNI_IDX(port),
					skb, NULL);
	} else {
		port_ch_dump(port, 1, skb, skb->len);
		spin_lock_irqsave(&port->port_rx_list.lock, flags);
			__skb_queue_tail(&port->port_rx_list, skb);
		spin_unlock_irqrestore(&port->port_rx_list.lock, flags);
	}
}

static int port_net_recv_skb(struct port_t *port, struct sk_buff *skb)
{
	struct lhif_header *lhif_h = (struct lhif_header *)skb->data;

	int mbim_ccmni_current = 0;
#ifdef CCCI_SKB_TRACE
	struct ccci_per_md *per_md_data = ccci_get_per_md_data(port->md_id);
	unsigned long long *netif_rx_profile = per_md_data->netif_rx_profile;
	unsigned long long netif_time;
#endif
#ifdef PORT_NET_TRACE
	unsigned long long rx_cb_time;
	unsigned long long total_time;

	total_time = sched_clock();
#endif
	if (port->hif_id == MD1_NET_HIF) {
		skb_pull(skb, sizeof(struct lhif_header));
		CCCI_DEBUG_LOG(port->md_id, NET,
			"port %s recv: 0x%08X, 0x%08X, %08X, 0x%08X\n",
			port->name, lhif_h->netif, lhif_h->f, lhif_h->flow,
			lhif_h->pdcp_count);
	} else {
		struct ccci_header *ccci_h = (struct ccci_header *)skb->data;

		skb_pull(skb, sizeof(struct ccci_header));
		CCCI_DEBUG_LOG(port->md_id, NET,
			"port %s recv: 0x%08X, 0x%08X, %08X, 0x%08X\n",
			port->name,
			ccci_h->data[0], ccci_h->data[1], ccci_h->channel,
			ccci_h->reserved);
	}

	mbim_ccmni_current = atomic_read(&mbim_ccmni_index[port->md_id]);
	if (mbim_ccmni_current == GET_CCMNI_IDX(port)) {
		mbim_start_xmit(skb, mbim_ccmni_current);
		return 0;
	}

#ifdef PORT_NET_TRACE
	rx_cb_time = sched_clock();
#endif

#ifdef CCCI_SKB_TRACE
	netif_rx_profile[4] = sched_clock() -
		(unsigned long long)skb->tstamp;
	skb->tstamp = 0;
	netif_time = sched_clock();
#endif

	ccmni_queue_recv_skb(port, skb);

#ifdef CCCI_SKB_TRACE
	netif_rx_profile[3] = sched_clock() - netif_time;
	ccmni_ops.dump_rx_status(port->md_id, netif_rx_profile);
#endif
#ifdef PORT_NET_TRACE
	rx_cb_time = sched_clock() - rx_cb_time;
	total_time = sched_clock() - total_time;
	trace_port_net_rx(port->md_id, PORT_RXQ_INDEX(port), port->rx_ch,
		(unsigned int)rx_cb_time, (unsigned int)total_time);
#endif
	return 0;
}

static void port_net_queue_state_notify(struct port_t *port, int dir,
	int qno, unsigned int state)
{
	int is_ack = 0;
	unsigned long flags = 0;

	if (dir == OUT)
		spin_lock_irqsave(&port->flag_lock, flags);
	if (dir == OUT && qno == NET_ACK_TXQ_INDEX(port)
		&& port->txq_index != qno)
		is_ack = 1;

	if (port->md_id != MD_SYS3) {
		if (((state == TX_IRQ) &&
			((!is_ack && ((port->flags &
			PORT_F_TX_DATA_FULLED) == 0)) ||
			(is_ack && ((port->flags &
			PORT_F_TX_ACK_FULLED) == 0))))) {
			if (dir == OUT)
				spin_unlock_irqrestore(&port->flag_lock, flags);
			return;
		}
	}
	if (state == TX_FULL) {
		if (ccci_dpmaif_empty_query(qno) > 0) {
			if (dir == OUT)
				spin_unlock_irqrestore(&port->flag_lock, flags);
			return;
		}
	}
	ccmni_ops.queue_state_callback(port->md_id,
		GET_CCMNI_IDX(port), state, is_ack);

	switch (state) {
	case TX_IRQ:
		port->flags &= ~(is_ack ? PORT_F_TX_ACK_FULLED :
			PORT_F_TX_DATA_FULLED);
		break;
	case TX_FULL:
		port->flags |= (is_ack ? PORT_F_TX_ACK_FULLED :
			PORT_F_TX_DATA_FULLED);
		break;
	default:
		break;
	};
	if (dir == OUT)
		spin_unlock_irqrestore(&port->flag_lock, flags);
}

static void port_net_md_state_notify(struct port_t *port, unsigned int state)
{
	ccmni_ops.md_state_callback(port->md_id, GET_CCMNI_IDX(port), state);
}

void port_net_md_dump_info(struct port_t *port, unsigned int flag)
{
	if (port == NULL) {
		CCCI_ERROR_LOG(0, NET, "%s: port==NULL\n", __func__);
		return;
	}
	if (ccmni_ops.dump == NULL) {
		CCCI_ERROR_LOG(port->md_id, NET,
			"%s: ccmni_ops.dump== null\n", __func__);
		return;
	}
	ccmni_ops.dump(port->md_id, GET_CCMNI_IDX(port), 0);
}

void mtk_ccci_net_port_init(char *name)
{
	int ret = 0, channel;
	struct port_t *port = NULL;

	channel = mtk_ccci_request_port(name);
	if (channel < 0) {
		CCCI_ERROR_LOG(-1, NET,
		"Fail to init net port %s for channel %d\n", name, channel);
		return;
	}
	ret = find_port_by_channel(channel, &port);
	if (ret < 0) {
		CCCI_ERROR_LOG(-1, NET,
		"Cannot find channel %d for net port %s\n", channel, name);
		return;
	}
	port->flags |= PORT_F_NET_DUMP;
	skb_queue_head_init(&port->port_rx_list);
}

struct port_ops net_port_ops = {
	.init = &port_net_init,
	.recv_skb = &port_net_recv_skb,
	.queue_state_notify = &port_net_queue_state_notify,
	.md_state_notify = &port_net_md_state_notify,
	.dump_info = &port_net_md_dump_info,
};
