// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2015 MediaTek Inc.
 */

/*****************************************************************************
 *
 * Filename:
 * ---------
 *   ccmni.c
 *
 * Project:
 * --------
 *
 *
 * Description:
 * ------------
 *   Cross Chip Modem Network Interface
 *
 * Author:
 * -------
 *   Anny.Hu(mtk80401)
 *
 ****************************************************************************/
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/ip.h>
#include <linux/tcp.h>
#include <linux/ipv6.h>
#include <net/ipv6.h>
#include <net/sch_generic.h>
#include <linux/skbuff.h>
#include <linux/module.h>
#include <linux/timer.h>
#include <linux/version.h>
#include <linux/sockios.h>
#include <linux/device.h>
#include <linux/debugfs.h>
#include <linux/preempt.h>
#include <linux/stacktrace.h>
#include "ccmni.h"
#include "ccci_debug.h"
#include "rps_perf.h"
#if defined(CCMNI_MET_DEBUG)
#include <mt-plat/met_drv.h>
#endif


struct ccmni_ctl_block *ccmni_ctl_blk;

/* Time in ns. This number must be less than 500ms. */
#ifdef ENABLE_WQ_GRO
long gro_flush_timer __read_mostly = 2000000L;
#else
long gro_flush_timer;
#endif

/*VIP_MARK is defined as highest priority */
#define APP_VIP_MARK		0x80000000
#define APP_VIP_MARK2		0x40000000

#define DEV_OPEN                1
#define DEV_CLOSE               0

static unsigned long timeout_flush_num, clear_flush_num;

static u64 g_cur_dl_speed;
static u32 g_tcp_is_need_gro = 1;

/*
 * Register the sysctl to set tcp_pacing_shift.
 */
static int sysctl_tcp_pacing_shift;
static struct ctl_table tcp_pacing_table[] = {
	{
		.procname	= "tcp_pacing_shift",
		.data		= &sysctl_tcp_pacing_shift,
		.maxlen		= sizeof(int),
		.mode		= 0644,
		.proc_handler	= proc_dointvec_minmax,
	},
	{}
};
static struct ctl_table tcp_pacing_sysctl_root[] = {
	{
		.procname	= "net",
		.mode		= 0555,
		.child		= tcp_pacing_table,
	},
	{}
};

static struct ctl_table_header *sysctl_header;
static int register_tcp_pacing_sysctl(void)
{
	sysctl_header = register_sysctl_table(tcp_pacing_sysctl_root);
	if (sysctl_header == NULL) {
		pr_info("CCCI:CCMNI:register tcp_pacing failed\n");
		return -1;
	}
	return 0;
}

static void unregister_tcp_pacing_sysctl(void)
{
	unregister_sysctl_table(sysctl_header);
}

void set_ccmni_rps(unsigned long value)
{
	int i = 0;

	if (ccmni_ctl_blk == NULL) {
		pr_info("%s: invalid ctlb\n", __func__);
		return;
	}

	for (i = 0; i < ccmni_ctl_blk->ccci_ops->ccmni_num; i++)
		set_rps_map(ccmni_ctl_blk->ccmni_inst[i]->dev->_rx, value);
}
EXPORT_SYMBOL(set_ccmni_rps);

void ccmni_set_cur_speed(u64 cur_dl_speed)
{
	g_cur_dl_speed = cur_dl_speed;
}
EXPORT_SYMBOL(ccmni_set_cur_speed);

void ccmni_set_tcp_is_need_gro(u32 tcp_is_need_gro)
{
	g_tcp_is_need_gro = tcp_is_need_gro;
}
EXPORT_SYMBOL(ccmni_set_tcp_is_need_gro);

static inline int is_ack_skb(struct sk_buff *skb)
{
	struct tcphdr *tcph;
	struct iphdr *iph;
	struct ipv6hdr *ip6h;
	u8 nexthdr;
	__be16 frag_off;
	u32 l4_off, total_len, packet_type;
	int ret = 0;
	struct md_tag_packet *tag = NULL;
	unsigned int count = 0;

	tag = (struct md_tag_packet *)skb->head;
	if (tag->guard_pattern == MDT_TAG_PATTERN)
		count = sizeof(tag->info);

	packet_type = skb->data[0] & 0xF0;
	if (packet_type == IPV6_VERSION) {
		ip6h = (struct ipv6hdr *)skb->data;
		total_len = sizeof(struct ipv6hdr) +
			    ntohs(ip6h->payload_len);

		/* copy md tag into skb->tail and
		 * skb->len > 128B(md recv buf size)
		 */
		/* this case will cause md EE */
		if (total_len <= 128 - sizeof(struct ccci_header) - count) {
			nexthdr = ip6h->nexthdr;
			l4_off = ipv6_skip_exthdr(skb,
				sizeof(struct ipv6hdr),
				&nexthdr, &frag_off);

			if (nexthdr == IPPROTO_TCP) {
				tcph = (struct tcphdr *)(skb->data + l4_off);

				if (tcph->syn)
					ret = 1;
				else if (!tcph->fin && !tcph->rst &&
					((total_len - l4_off) ==
						(tcph->doff << 2)))
					ret = 1;
			}
		}
	} else if (packet_type == IPV4_VERSION) {
		iph = (struct iphdr *)skb->data;

		if (ntohs(iph->tot_len) <=
				128 - sizeof(struct ccci_header) - count) {

			if (iph->protocol == IPPROTO_TCP) {
				tcph = (struct tcphdr *)(skb->data + (iph->ihl << 2));

				if (tcph->syn)
					ret = 1;
				else if (!tcph->fin && !tcph->rst &&
					ntohs(iph->tot_len) ==
					(iph->ihl << 2) + (tcph->doff << 2)) {
					ret = 1;
				}
			}
		}
	}

	return ret;
}

#ifdef ENABLE_WQ_GRO
static int is_skb_gro(struct sk_buff *skb)
{
	u32 packet_type;
	u32 protocol = 0xFFFFFFFF;

	packet_type = skb->data[0] & 0xF0;

	if (packet_type == IPV4_VERSION)
		protocol = ip_hdr(skb)->protocol;
	else if (packet_type == IPV6_VERSION)
		protocol = ipv6_hdr(skb)->nexthdr;

	if (protocol == IPPROTO_TCP) {
		return g_tcp_is_need_gro;
	} else if (protocol == IPPROTO_UDP) {
		if (g_cur_dl_speed > 500000000LL) //>500Mbps
			return 1;
	}

	return 0;
}

void ccmni_clr_flush_timer(void)
{
	int i = 0;
	struct ccmni_ctl_block *ctlb = ccmni_ctl_blk;

	if (ctlb == NULL)
		return;

	for (i = 0; i < ctlb->ccci_ops->ccmni_num; i++)
		if (ctlb->ccmni_inst[i] && ctlb->ccmni_inst[i]->dev &&
		    (ctlb->ccmni_inst[i]->dev->flags & IFF_UP))
			ktime_get_real_ts64(&ctlb->ccmni_inst[i]->flush_time);

}
EXPORT_SYMBOL(ccmni_clr_flush_timer);

static inline void napi_gro_list_flush(struct ccmni_instance *ccmni)
{
	struct napi_struct *napi;

	napi = ccmni->napi;
	napi_gro_flush(napi, false);
	if (napi->rx_count) {
		netif_receive_skb_list(&napi->rx_list);
		INIT_LIST_HEAD(&napi->rx_list);
		napi->rx_count = 0;
	}
}

static void ccmni_gro_flush(struct ccmni_instance *ccmni)
{
	struct timespec64 curr_time, diff;

	if (!gro_flush_timer)
		return;

	ktime_get_real_ts64(&curr_time);
	diff = timespec64_sub(curr_time, ccmni->flush_time);
	if ((diff.tv_sec > 0) || (diff.tv_nsec > gro_flush_timer)) {
		napi_gro_list_flush(ccmni);
		timeout_flush_num++;
		ktime_get_real_ts64(&ccmni->flush_time);
	}
}
#endif

static inline int ccmni_forward_rx(struct ccmni_instance *ccmni,
	struct sk_buff *skb)
{
	bool flt_ok = false;
	bool flt_flag = true;
	unsigned int pkt_type;
	struct iphdr *iph;
	struct ipv6hdr *ip6h;
	struct ccmni_fwd_filter flt_tmp;
	unsigned int i, j;
	u16 mask;
	u32 *addr1, *addr2;

	if (ccmni->flt_cnt) {
		for (i = 0; i < CCMNI_FLT_NUM; i++) {
			flt_tmp = ccmni->flt_tbl[i];
			pkt_type = skb->data[0] & 0xF0;
			if (!flt_tmp.ver || (flt_tmp.ver != pkt_type))
				continue;

			if (pkt_type == IPV4_VERSION) {
				iph = (struct iphdr *)skb->data;
				mask = flt_tmp.s_pref;
				addr1 = &iph->saddr;
				addr2 = &flt_tmp.ipv4.saddr;
				flt_flag = true;
				for (j = 0; flt_flag && j < 2; j++) {
					if (mask &&
						(addr1[0] >> (32 - mask) !=
						addr2[0] >> (32 - mask))) {
						flt_flag = false;
						break;
					}
					mask = flt_tmp.d_pref;
					addr1 = &iph->daddr;
					addr2 = &flt_tmp.ipv4.daddr;
				}
			} else if (pkt_type == IPV6_VERSION) {
				ip6h = (struct ipv6hdr *)skb->data;
				mask = flt_tmp.s_pref;
				addr1 = ip6h->saddr.s6_addr32;
				addr2 = flt_tmp.ipv6.saddr;
				flt_flag = true;
				for (j = 0; flt_flag && j < 2; j++) {
					if (mask == 0) {
						mask = flt_tmp.d_pref;
						addr1 = ip6h->daddr.s6_addr32;
						addr2 = flt_tmp.ipv6.daddr;
						continue;
					}
					if (mask <= 32 &&
						(addr1[0] >> (32 - mask) !=
						addr2[0] >> (32 - mask))) {
						flt_flag = false;
						break;
					}
					if (mask <= 64 &&
						(addr1[0] != addr2[0] ||
						addr1[1] >> (64 - mask) !=
						addr2[1] >> (64 - mask))) {
						flt_flag = false;
						break;
					}
					if (mask <= 96 &&
						(addr1[0] != addr2[0] ||
						addr1[1] != addr2[1] ||
						addr1[2] >> (96 - mask) !=
						addr2[2] >> (96 - mask))) {
						flt_flag = false;
						break;
					}
					if (mask <= 128 &&
						(addr1[0] != addr2[0] ||
						addr1[1] != addr2[1] ||
						addr1[2] != addr2[2] ||
						addr1[3] >> (128 - mask) !=
						addr2[3] >> (128 - mask))) {
						flt_flag = false;
						break;
					}
					mask = flt_tmp.d_pref;
					addr1 = ip6h->daddr.s6_addr32;
					addr2 = flt_tmp.ipv6.daddr;
				}
			}
			if (flt_flag) {
				flt_ok = true;
				break;
			}
		}

		if (flt_ok) {
			skb->ip_summed = CHECKSUM_NONE;
			skb_set_mac_header(skb, -ETH_HLEN);

			if (!in_interrupt())
				netif_rx_ni(skb);
			else
				netif_rx(skb);
			return NETDEV_TX_OK;
		}
	}

	return -1;
}


/********************netdev register function********************/
static u16 ccmni_select_queue(struct net_device *dev, struct sk_buff *skb,
	struct net_device *sb_dev/*, select_queue_fallback_t fallback */)
{
	struct ccmni_instance *ccmni =
		(struct ccmni_instance *)netdev_priv(dev);

	if (unlikely(ccmni_ctl_blk == NULL)) {
		netdev_err(dev, "%s : invalid ccmni_ctl_blk\n", __func__);
		return CCMNI_TXQ_NORMAL;
	}
	if (ccmni_ctl_blk->ccci_ops->md_ability & MODEM_CAP_DATA_ACK_DVD) {
		if ((skb->mark & APP_VIP_MARK) || (skb->mark & APP_VIP_MARK2))
			return CCMNI_TXQ_FAST;

		if (ccmni->ack_prio_en && is_ack_skb(skb))
			return CCMNI_TXQ_FAST;
		else
			return CCMNI_TXQ_NORMAL;
	} else
		return CCMNI_TXQ_NORMAL;
}

static int s_call_times;

static int ccmni_open(struct net_device *dev)
{
	struct ccmni_instance *ccmni =
		(struct ccmni_instance *)netdev_priv(dev);
	struct ccmni_instance *ccmni_tmp = NULL;
	int usage_cnt = 0;

	if (ccmni->index < 0) {
		netdev_err(dev, "%s : invalid ccmni index = %d\n",
			   __func__, ccmni->index);
		return -1;
	}

	if (unlikely(ccmni_ctl_blk == NULL)) {
		netdev_err(dev, "%s : invalid ccmni_ctl_blk\n", __func__);
		return -1;
	}

	if (gro_flush_timer)
		ktime_get_real_ts64(&ccmni->flush_time);

	netif_carrier_on(dev);

	netif_tx_start_all_queues(dev);

	if (unlikely(ccmni_ctl_blk->ccci_ops->md_ability & MODEM_CAP_NAPI)) {
		napi_enable(ccmni->napi);
		napi_schedule(ccmni->napi);
	}

	atomic_inc(&ccmni->usage);
	ccmni_tmp = ccmni_ctl_blk->ccmni_inst[ccmni->index];
	if (ccmni != ccmni_tmp) {
		usage_cnt = atomic_read(&ccmni->usage);
		atomic_set(&ccmni_tmp->usage, usage_cnt);
	}
	queue_delayed_work(ccmni->worker,
				&ccmni->pkt_queue_work,
				msecs_to_jiffies(500));

	netdev_info(dev,
		"%s_Open:cnt=(%d,%d), md_ab=0x%X, gro=(%llx, %ld), flt_cnt=%d\n",
		dev->name, atomic_read(&ccmni->usage),
		atomic_read(&ccmni_tmp->usage),
		ccmni_ctl_blk->ccci_ops->md_ability,
		dev->features, gro_flush_timer, ccmni->flt_cnt);

	if (s_call_times == 0)
		set_ccmni_rps(0x70);
	s_call_times++;

	return 0;
}

static int ccmni_close(struct net_device *dev)
{
	struct ccmni_instance *ccmni =
		(struct ccmni_instance *)netdev_priv(dev);
	struct ccmni_instance *ccmni_tmp = NULL;
	int usage_cnt = 0, ret = 0;

	if (ccmni->index < 0) {
		netdev_err(dev, "%s : invalid ccmni idx:%d\n", __func__, ccmni->index);
		return -1;
	}

	if (unlikely(ccmni_ctl_blk == NULL)) {
		netdev_err(dev, "%s : invalid ccmni_ctl_blk\n", __func__);
		return -1;
	}

	cancel_delayed_work(&ccmni->pkt_queue_work);
	flush_delayed_work(&ccmni->pkt_queue_work);

	atomic_dec(&ccmni->usage);
	ccmni_tmp = ccmni_ctl_blk->ccmni_inst[ccmni->index];
	if (ccmni != ccmni_tmp) {
		usage_cnt = atomic_read(&ccmni->usage);
		atomic_set(&ccmni_tmp->usage, usage_cnt);
	}

	netif_tx_disable(dev);

	if (unlikely(ccmni_ctl_blk->ccci_ops->md_ability & MODEM_CAP_NAPI))
		napi_disable(ccmni->napi);

	ret = ccmni_ctl_blk->ccci_ops->ccci_handle_port_list(DEV_CLOSE, dev->name);
	netdev_info(dev, "%s_Close:cnt=(%d, %d)\n",
		    dev->name, atomic_read(&ccmni->usage),
		    atomic_read(&ccmni_tmp->usage));

	return 0;
}

static netdev_tx_t ccmni_start_xmit(struct sk_buff *skb, struct net_device *dev)
{
	int ret = 0;
	int skb_len = skb->len;
	struct ccmni_instance *ccmni =
		(struct ccmni_instance *)netdev_priv(dev);
	unsigned int is_ack = 0;
	unsigned int priority_lev = 0;
	struct md_tag_packet *tag = NULL;
	unsigned int count = 0;
	struct iphdr *iph;

#if defined(CCMNI_MET_DEBUG)
	char tag_name[32] = { '\0' };
	unsigned int tag_id = 0;
#endif

	if (ccmni_forward_rx(ccmni, skb) == NETDEV_TX_OK)
		return NETDEV_TX_OK;

	/* dev->mtu is changed  if dev->mtu is changed by upper layer */
	if (unlikely(skb->len > dev->mtu)) {
		netdev_err(dev,
			   "CCMNI%d write fail: len(0x%x)>MTU(0x%x, 0x%x)\n",
			   ccmni->index, skb->len, CCMNI_MTU, dev->mtu);
		dev_kfree_skb(skb);
		dev->stats.tx_dropped++;
		return NETDEV_TX_OK;
	}

	if (unlikely(skb_headroom(skb) < sizeof(struct ccci_header))) {
		netdev_err(dev,
			   "CCMNI%d write fail: header room(%d) < ccci_header(%d)\n",
			   ccmni->index, skb_headroom(skb), dev->hard_header_len);
		dev_kfree_skb(skb);
		dev->stats.tx_dropped++;
		return NETDEV_TX_OK;
	}

	tag = (struct md_tag_packet *)skb->head;
	if (tag->guard_pattern == MDT_TAG_PATTERN) {
		count = sizeof(tag->info);
		memcpy(skb_tail_pointer(skb), &(tag->info), count);
		skb->len += count;
	}

	if (ccmni_ctl_blk->ccci_ops->md_ability & MODEM_CAP_DATA_ACK_DVD) {
		iph = (struct iphdr *)skb_network_header(skb);
		if (skb->mark & APP_VIP_MARK) {
			is_ack = 1;
			priority_lev = PRIORITY_2; /* highest priority */
		} else if (skb->mark & APP_VIP_MARK2) {
			is_ack = 1;
			priority_lev = PRIORITY_1;
		} else if (ccmni->ack_prio_en) {
			is_ack = is_ack_skb(skb);
			if (is_ack)
				priority_lev = PRIORITY_2;
			else
				priority_lev = PRIORITY_0;
		}
	}
	sk_pacing_shift_update(skb->sk, sysctl_tcp_pacing_shift);
	ret = ccmni_ctl_blk->ccci_ops->send_pkt(ccmni->index, skb, priority_lev);
	if (ret == CCMNI_ERR_MD_NO_READY || ret == CCMNI_ERR_TX_INVAL) {
		dev_kfree_skb(skb);
		dev->stats.tx_dropped++;
		ccmni->tx_busy_cnt[is_ack] = 0;
		netdev_dbg(dev,
			   "[TX] CCMNI%d send tx_pkt=%ld(ack=%d) fail: %d\n",
			   ccmni->index, (dev->stats.tx_packets + 1),
			   is_ack, ret);
		return NETDEV_TX_OK;
	} else if (ret == CCMNI_ERR_TX_BUSY)
		goto tx_busy;

	dev->stats.tx_packets++;
	dev->stats.tx_bytes += skb_len;
	if (ccmni->tx_busy_cnt[is_ack] > 10) {
		netdev_dbg(dev,
			   "[TX] CCMNI%d TX busy: tx_pkt=%ld(ack=%d) retry %ld times done\n",
			   ccmni->index, dev->stats.tx_packets,
			   is_ack, ccmni->tx_busy_cnt[is_ack]);
	}
	ccmni->tx_busy_cnt[is_ack] = 0;

#if defined(CCMNI_MET_DEBUG)
	if (ccmni->tx_met_time == 0) {
		ccmni->tx_met_time = jiffies;
		ccmni->tx_met_bytes = dev->stats.tx_bytes;
	} else if (time_after_eq(jiffies,
		ccmni->tx_met_time + msecs_to_jiffies(MET_LOG_TIMER))) {
		scnprintf(tag_name, 32, "%s_tx_bytes", dev->name);
		tag_id = CCMNI_TX_MET_ID + ccmni->index;
		met_tag_oneshot(tag_id, tag_name,
		(dev->stats.tx_bytes - ccmni->tx_met_bytes));
		ccmni->tx_met_bytes = dev->stats.tx_bytes;
		ccmni->tx_met_time = jiffies;
	}
#endif

	return NETDEV_TX_OK;

tx_busy:
	if (unlikely(!(ccmni_ctl_blk->ccci_ops->md_ability & MODEM_CAP_TXBUSY_STOP))) {
		if ((ccmni->tx_busy_cnt[is_ack]++) % 100 == 0)
			netdev_dbg(dev,
				"[TX]CCMNI%d TX busy: tx_pkt=%ld(ack=%d) retry_times=%ld\n",
				ccmni->index, dev->stats.tx_packets,
				is_ack, ccmni->tx_busy_cnt[is_ack]);
	} else
		ccmni->tx_busy_cnt[is_ack]++;

	return NETDEV_TX_BUSY;
}

static int ccmni_change_mtu(struct net_device *dev, int new_mtu)
{
	struct ccmni_instance *ccmni =
		(struct ccmni_instance *)netdev_priv(dev);

	if (new_mtu > CCMNI_MTU)
		return -EINVAL;

	dev->mtu = new_mtu;
	netdev_dbg(dev,
		"CCMNI%d change mtu_siz=%d\n", ccmni->index, new_mtu);
	return 0;
}

static void ccmni_tx_timeout(struct net_device *dev, unsigned int txqueue)
{
	struct ccmni_instance *ccmni =
		(struct ccmni_instance *)netdev_priv(dev);

	netdev_dbg(dev,
		   "ccmni%d_tx_timeout: usage_cnt=%d, timeout=%ds\n",
		   ccmni->index, atomic_read(&ccmni->usage),
		   (ccmni->dev->watchdog_timeo/HZ));

	dev->stats.tx_errors++;
	if (atomic_read(&ccmni->usage) > 0)
		netif_tx_wake_all_queues(dev);
}

static int ccmni_ioctl(struct net_device *dev, struct ifreq *ifr, int cmd)
{
	int usage_cnt;
	struct ccmni_instance *ccmni =
		(struct ccmni_instance *)netdev_priv(dev);
	struct ccmni_instance *ccmni_tmp = NULL;
	struct ccmni_ctl_block *ctlb = ccmni_ctl_blk;
	unsigned int timeout = 0;
	struct ccmni_fwd_filter flt_tmp;
	struct ccmni_flt_act flt_act;
	unsigned int i;
	unsigned int cmp_len;

	if (ccmni->index < 0) {
		netdev_err(dev, "%s : invalid ccmni index = %d\n",
			   __func__, ccmni->index);
		return -EINVAL;
	}

	switch (cmd) {
	case SIOCSTXQSTATE:
		/* ifru_ivalue[3~0]:   start/stop
		 * ifru_ivalue[7~4]:   reserve
		 * ifru_ivalue[15~8]:  user id, bit8=rild, bit9=thermal
		 * ifru_ivalue[31~16]: watchdog timeout value
		 */
		if ((ifr->ifr_ifru.ifru_ivalue & 0xF) == 0) {
			/*ignore txq stop/resume if dev is not running */
			if (atomic_read(&ccmni->usage) > 0 &&
				netif_running(dev)) {
				atomic_dec(&ccmni->usage);

				netif_tx_disable(dev);
				/* stop queue won't stop Tx
				 * watchdog (ndo_tx_timeout)
				 */
				timeout = (ifr->ifr_ifru.ifru_ivalue &
					0xFFFF0000) >> 16;
				if (timeout == 0)
					dev->watchdog_timeo = 60 * HZ;
				else
					dev->watchdog_timeo = timeout*HZ;

				ccmni_tmp = ctlb->ccmni_inst[ccmni->index];
				/* iRAT ccmni */
				if (ccmni_tmp != ccmni) {
					usage_cnt = atomic_read(&ccmni->usage);
					atomic_set(&ccmni_tmp->usage,
								usage_cnt);
				}
			}
		} else {
			if (atomic_read(&ccmni->usage) <= 0 &&
					netif_running(dev)) {
				netif_tx_wake_all_queues(dev);
				dev->watchdog_timeo = CCMNI_NETDEV_WDT_TO;
				atomic_inc(&ccmni->usage);

				ccmni_tmp = ctlb->ccmni_inst[ccmni->index];
				/* iRAT ccmni */
				if (ccmni_tmp != ccmni) {
					usage_cnt = atomic_read(&ccmni->usage);
					atomic_set(&ccmni_tmp->usage,
								usage_cnt);
				}
			}
		}
		if (likely(ccmni_tmp != NULL)) {
			netdev_dbg(dev,
				"SIOCSTXQSTATE: %s_state=0x%x, cnt=(%d, %d)\n",
				dev->name, ifr->ifr_ifru.ifru_ivalue,
				atomic_read(&ccmni->usage),
				atomic_read(&ccmni_tmp->usage));
		} else {
			netdev_dbg(dev,
				"SIOCSTXQSTATE: %s_state=0x%x, cnt=%d\n",
				dev->name, ifr->ifr_ifru.ifru_ivalue,
				atomic_read(&ccmni->usage));
		}
		break;

	case SIOCFWDFILTER:
		if (copy_from_user(&flt_act, ifr->ifr_ifru.ifru_data,
				sizeof(struct ccmni_flt_act))) {
			netdev_info(dev,
				"SIOCFWDFILTER: %s copy data from user fail\n",
				dev->name);
			return -EFAULT;
		}

		flt_tmp = flt_act.flt;
		if (flt_tmp.ver != 0x40 && flt_tmp.ver != 0x60) {
			netdev_info(dev,
				"SIOCFWDFILTER[%d]: %s invalid flt(%x, %x, %x, %x, %x)(%d)\n",
				flt_act.action, dev->name,
				flt_tmp.ver, flt_tmp.s_pref,
				flt_tmp.d_pref, flt_tmp.ipv4.saddr,
				flt_tmp.ipv4.daddr, ccmni->flt_cnt);
			return -EINVAL;
		}

		if (flt_act.action == CCMNI_FLT_ADD) { /* add new filter */
			if (ccmni->flt_cnt >= CCMNI_FLT_NUM) {
				netdev_info(dev,
					"SIOCFWDFILTER[ADD]: %s flt table full\n",
					dev->name);
				return -ENOMEM;
			}
			for (i = 0; i < CCMNI_FLT_NUM; i++) {
				if (ccmni->flt_tbl[i].ver == 0)
					break;
			}
			if (i < CCMNI_FLT_NUM) {
				memcpy(&ccmni->flt_tbl[i], &flt_tmp,
					sizeof(struct ccmni_fwd_filter));
				ccmni->flt_cnt++;
			}
			netdev_info(dev,
				"SIOCFWDFILTER[ADD]: %s add flt%d(%x, %x, %x, %x, %x)(%d)\n",
				dev->name, i, flt_tmp.ver,
				flt_tmp.s_pref, flt_tmp.d_pref,
				flt_tmp.ipv4.saddr, flt_tmp.ipv4.daddr,
				ccmni->flt_cnt);
		} else if (flt_act.action == CCMNI_FLT_DEL) {
			if (flt_tmp.ver == IPV4_VERSION)
				cmp_len = offsetof(struct ccmni_fwd_filter,
							ipv4.daddr) + 4;
			else
				cmp_len = sizeof(struct ccmni_fwd_filter);
			for (i = 0; i < CCMNI_FLT_NUM; i++) {
				if (ccmni->flt_tbl[i].ver == 0)
					continue;
				if (!memcmp(&ccmni->flt_tbl[i],
						&flt_tmp, cmp_len)) {
					netdev_info(dev,
						"SIOCFWDFILTER[DEL]: %s del flt%d(%x, %x, %x, %x, %x)(%d)\n",
						dev->name, i, flt_tmp.ver,
						flt_tmp.s_pref, flt_tmp.d_pref,
						flt_tmp.ipv4.saddr,
						flt_tmp.ipv4.daddr,
						ccmni->flt_cnt);
					memset(
						&ccmni->flt_tbl[i],
						0,
						sizeof(struct ccmni_fwd_filter)
					);
					ccmni->flt_cnt--;
					break;
				}
			}
			if (i >= CCMNI_FLT_NUM) {
				netdev_info(dev,
					"SIOCFWDFILTER[DEL]: %s no match flt(%x, %x, %x, %x, %x)(%d)\n",
					dev->name, flt_tmp.ver,
					flt_tmp.s_pref, flt_tmp.d_pref,
					flt_tmp.ipv4.saddr,
					flt_tmp.ipv4.daddr,
					ccmni->flt_cnt);
				return -ENXIO;
			}
		} else if (flt_act.action == CCMNI_FLT_FLUSH) {
			ccmni->flt_cnt = 0;
			memset(ccmni->flt_tbl, 0,
			CCMNI_FLT_NUM*sizeof(struct ccmni_fwd_filter));
			netdev_info(dev,
				"SIOCFWDFILTER[FLUSH]: %s flush filter\n",
				dev->name);
		}
		break;

	case SIOCACKPRIO:
		/* ifru_ivalue[3~0]: enable/disable ack prio feature  */
		if ((ifr->ifr_ifru.ifru_ivalue & 0xF) == 0) {
			for (i = 0; i < ctlb->ccci_ops->ccmni_num; i++) {
				ccmni_tmp = ctlb->ccmni_inst[i];
				ccmni_tmp->ack_prio_en = 0;
			}
		} else {
			for (i = 0; i < ctlb->ccci_ops->ccmni_num; i++) {
				ccmni_tmp = ctlb->ccmni_inst[i];
				ccmni_tmp->ack_prio_en = 1;
			}
		}
		netdev_info(dev,
			"SIOCACKPRIO: ack_prio_en=%d, ccmni0_ack_en=%d\n",
			ifr->ifr_ifru.ifru_ivalue,
			ccmni_tmp->ack_prio_en);
		break;

	case SIOPUSHPENDING:
		netdev_info(dev, "%s SIOPUSHPENDING called\n", ccmni->dev->name);
		cancel_delayed_work(&ccmni->pkt_queue_work);
		flush_delayed_work(&ccmni->pkt_queue_work);
		if (ctlb->ccci_ops->ccci_handle_port_list(DEV_OPEN, ccmni->dev->name))
			netdev_info(dev,
				"%s is failed to handle port list\n",
				ccmni->dev->name);
		break;

	default:
		netdev_dbg(dev,
			"%s: unknown ioctl cmd=%x\n", dev->name, cmd);
		break;
	}

	return 0;
}

static int ccmni_private_ioctl(struct net_device *dev, struct ifreq *ifr,
								void __user *data, int cmd)
{
	return ccmni_ioctl(dev, ifr, cmd);
}

static const struct net_device_ops ccmni_netdev_ops = {
	.ndo_open		= ccmni_open,
	.ndo_stop		= ccmni_close,
	.ndo_start_xmit	= ccmni_start_xmit,
	.ndo_tx_timeout	= ccmni_tx_timeout,
	.ndo_do_ioctl   = ccmni_ioctl,
	.ndo_siocdevprivate = ccmni_private_ioctl,
	.ndo_change_mtu = ccmni_change_mtu,
	.ndo_select_queue = ccmni_select_queue,
};

static int ccmni_napi_poll(struct napi_struct *napi, int budget)
{
#ifdef ENABLE_WQ_GRO
	return 0;
#else
	struct ccmni_instance *ccmni =
		(struct ccmni_instance *)netdev_priv(napi->dev);

	del_timer(ccmni->timer);

	if (ccmni_ctl_blk->ccci_ops->napi_poll)
		return ccmni_ctl_blk->ccci_ops->napi_poll(ccmni->index,
					napi, budget);
	else
		return 0;
#endif
}

//static void ccmni_napi_poll_timeout(unsigned long data)
static void ccmni_napi_poll_timeout(struct timer_list *t)
{
	//struct ccmni_instance *ccmni = (struct ccmni_instance *)data;
	//struct ccmni_instance *ccmni = from_timer(ccmni, t, timer);

	//netdev_dbg(dev,
	//	"CCMNI%d lost NAPI polling\n", ccmni->index);
}

static void get_queued_pkts(struct work_struct *work)
{
	struct delayed_work *dwork = to_delayed_work(work);
	struct ccmni_instance *ccmni =
		container_of(dwork, struct ccmni_instance, pkt_queue_work);

	if (ccmni_ctl_blk == NULL) {
		pr_info("%s : invalid ctlb\n", __func__);
		return;
	}
	if (ccmni_ctl_blk->ccci_ops->ccci_handle_port_list(DEV_OPEN, ccmni->dev->name))
		pr_info("%s is failed to handle port list\n", ccmni->dev->name);
}

/********************ccmni driver register  ccci function********************/
static inline int ccmni_inst_init(struct ccmni_instance *ccmni, struct net_device *dev)
{
	int ret = 0;

	ret = ccmni_ctl_blk->ccci_ops->get_ccmni_ch(ccmni->index, &ccmni->ch);
	if (ret) {
		netdev_dbg(dev,
			"get ccmni%d channel fail\n",
			ccmni->index);
		return ret;
	}

	ccmni->dev = dev;
	ccmni->ctlb = ccmni_ctl_blk;
	ccmni->napi = kzalloc(sizeof(struct napi_struct), GFP_KERNEL);
	if (ccmni->napi == NULL) {
		netdev_err(dev, "%s kzalloc ccmni->napi fail\n", __func__);
		return -1;
	}
	ccmni->timer = kzalloc(sizeof(struct timer_list), GFP_KERNEL);
	if (ccmni->timer == NULL) {
		netdev_err(dev, "%s kzalloc ccmni->timer fail\n", __func__);
		return -1;
	}
	ccmni->spinlock = kzalloc(sizeof(spinlock_t), GFP_KERNEL);
	if (ccmni->spinlock == NULL) {
		netdev_err(dev, "%s kzalloc ccmni->spinlock fail\n", __func__);
		return -1;
	}

	ccmni->ack_prio_en = 1;

	/* register napi device */
	if (dev && (ccmni_ctl_blk->ccci_ops->md_ability & MODEM_CAP_NAPI)) {
		//init_timer(ccmni->timer);
		//ccmni->timer->function = ccmni_napi_poll_timeout;
		//ccmni->timer->data = (unsigned long)ccmni;
		timer_setup(ccmni->timer, ccmni_napi_poll_timeout, 0);
		netif_napi_add(dev, ccmni->napi, ccmni_napi_poll,
			ccmni_ctl_blk->ccci_ops->napi_poll_weigh);
	}
#ifdef ENABLE_WQ_GRO
	if (dev)
		netif_napi_add(dev, ccmni->napi, ccmni_napi_poll,
			ccmni_ctl_blk->ccci_ops->napi_poll_weigh);
#endif

	atomic_set(&ccmni->usage, 0);
	spin_lock_init(ccmni->spinlock);

	ccmni->worker = alloc_workqueue("ccmni%d_rx_q_worker",
		WQ_UNBOUND | WQ_MEM_RECLAIM, 1, ccmni->index);
	if (!ccmni->worker) {
		netdev_info(dev, "%s alloc queue worker fail\n",
			__func__);
		return -1;
	}
	INIT_DELAYED_WORK(&ccmni->pkt_queue_work, get_queued_pkts);

	return ret;
}

static inline void ccmni_dev_init(struct net_device *dev)
{
	dev->header_ops = NULL; /* No Header */
	dev->mtu = CCMNI_MTU;
	dev->tx_queue_len = CCMNI_TX_QUEUE;
	dev->watchdog_timeo = CCMNI_NETDEV_WDT_TO;
	/* ccmni is a pure IP device */
	dev->flags = IFF_NOARP &
			(~IFF_BROADCAST & ~IFF_MULTICAST);
	/* not support VLAN */
	dev->features = NETIF_F_VLAN_CHALLENGED;
	dev->features |= NETIF_F_GRO_FRAGLIST;
	if (ccmni_ctl_blk->ccci_ops->md_ability & MODEM_CAP_HWTXCSUM) {
		pr_info("checksum_dbg %s MODEM_CAP_HWTXCSUM", __func__);
		dev->features |= NETIF_F_HW_CSUM;
	}
	if (ccmni_ctl_blk->ccci_ops->md_ability & MODEM_CAP_SGIO) {
		dev->features |= NETIF_F_SG;
		dev->hw_features |= NETIF_F_SG;
	}
	if (ccmni_ctl_blk->ccci_ops->md_ability & MODEM_CAP_NAPI) {
#ifdef ENABLE_NAPI_GRO
		dev->features |= NETIF_F_GRO;
		dev->hw_features |= NETIF_F_GRO;
#endif
	} else {
#ifdef ENABLE_WQ_GRO
		dev->features |= NETIF_F_GRO;
		dev->hw_features |= NETIF_F_GRO;
#endif
	}
	/* check gro_list_prepare
	 * when skb hasn't ethernet header,
	 * GRO needs hard_header_len == 0.
	 */
	dev->hard_header_len = 0;
	dev->addr_len = 0;        /* hasn't ethernet header */
	dev->priv_destructor = free_netdev;
	dev->netdev_ops = &ccmni_netdev_ops;
	random_ether_addr((u8 *) dev->dev_addr);
}

#ifdef CCCI_CCMNI_MODULE
const struct header_ops ccmni_eth_header_ops ____cacheline_aligned = {
	.create		= eth_header,
	.parse		= eth_header_parse,
	.cache		= eth_header_cache,
	.cache_update	= eth_header_cache_update,
};
#endif

static int ccmni_init(struct ccmni_ccci_ops *ccci_info)
{
	int i = 0, j = 0, ret = 0;
	struct ccmni_ctl_block *ctlb = NULL;
	struct ccmni_instance *ccmni = NULL;
	struct net_device *dev = NULL;

	if (register_tcp_pacing_sysctl() == -1)
		return 0;
	sysctl_tcp_pacing_shift = 6;

	if (unlikely(ccci_info->md_ability & MODEM_CAP_CCMNI_DISABLE)) {
		netdev_err(dev, "no need init ccmni: md_ability=0x%08X\n",
			ccci_info->md_ability);
		return 0;
	}

	ctlb = kzalloc(sizeof(struct ccmni_ctl_block), GFP_KERNEL);
	if (unlikely(ctlb == NULL)) {
		netdev_err(dev, "alloc ccmni ctl struct fail\n");
		return -ENOMEM;
	}

	ctlb->ccci_ops = kzalloc(sizeof(struct ccmni_ccci_ops), GFP_KERNEL);
	if (unlikely(ctlb->ccci_ops == NULL)) {
		netdev_err(dev, "alloc ccmni_ccci_ops struct fail\n");
		ret = -ENOMEM;
		goto alloc_mem_fail;
	}

#if defined(CCMNI_MET_DEBUG)
	if (met_tag_init() != 0)
		netdev_info(dev, "%s:met tag init fail\n", __func__);
#endif

	ccmni_ctl_blk = ctlb;

	memcpy(ctlb->ccci_ops, ccci_info, sizeof(struct ccmni_ccci_ops));

	for (i = 0; i < ctlb->ccci_ops->ccmni_num; i++) {
		/* allocate netdev */
		if (ctlb->ccci_ops->md_ability & MODEM_CAP_CCMNI_MQ)
			/* alloc multiple tx queue, 2 txq and 1 rxq */
			dev =
			alloc_etherdev_mqs(
					sizeof(struct ccmni_instance),
					2,
					1);
		else
			dev =
			alloc_etherdev(sizeof(struct ccmni_instance));
		if (unlikely(dev == NULL)) {
			netdev_dbg(dev, "alloc netdev fail\n");
			ret = -ENOMEM;
			goto alloc_netdev_fail;
		}

		/* init net device */
		ccmni_dev_init(dev);

		/* The purpose of changing the ccmni device type to ARPHRD_NONE
		 * is used to support automatically adding an ipv6 mroute and
		 * support for clat eBPF and tethering eBPF offload
		 */
		dev->type = ARPHRD_NONE;

		scnprintf(dev->name, sizeof(dev->name), "%s%d", ctlb->ccci_ops->name, i);

		/* init private structure of netdev */
		ccmni = netdev_priv(dev);
		ccmni->index = i;
		ret = ccmni_inst_init(ccmni, dev);
		if (ret) {
			netdev_info(dev,
				"initial ccmni instance fail\n");
			goto alloc_netdev_fail;
		}
		ctlb->ccmni_inst[i] = ccmni;

		/* register net device */
		ret = register_netdev(dev);
		if (ret)
			goto alloc_netdev_fail;
		ctlb->ccci_ops->ccci_net_init(dev->name);
	}

	scnprintf(ctlb->wakelock_name, sizeof(ctlb->wakelock_name),
			"ccmni_md1");
	ctlb->ccmni_wakelock = wakeup_source_register(NULL,
		ctlb->wakelock_name);
	if (!ctlb->ccmni_wakelock) {
		netdev_info(dev, "%s %d: init wakeup source fail!",
			__func__, __LINE__);
		return -1;
	}

	return 0;

alloc_netdev_fail:
	if (dev) {
		free_netdev(dev);
		ctlb->ccmni_inst[i] = NULL;
	}

	for (j = i - 1; j >= 0; j--) {
		ccmni = ctlb->ccmni_inst[j];
		unregister_netdev(ccmni->dev);
		/* free_netdev(ccmni->dev); */
		ctlb->ccmni_inst[j] = NULL;
	}

alloc_mem_fail:
	kfree(ctlb->ccci_ops);
	kfree(ctlb);

	ccmni_ctl_blk = NULL;
	return ret;
}

static void ccmni_exit(void)
{
	int i = 0;
	struct ccmni_ctl_block *ctlb = NULL;
	struct ccmni_instance *ccmni = NULL;

	unregister_tcp_pacing_sysctl();

	ctlb = ccmni_ctl_blk;
	if (ctlb) {
		if (ctlb->ccci_ops == NULL)
			goto ccmni_exit_ret;

		for (i = 0; i < ctlb->ccci_ops->ccmni_num; i++) {
			ccmni = ctlb->ccmni_inst[i];
			if (!ccmni)
				continue;

			pr_debug("%s: unregister ccmni%d dev\n",
				__func__, i);
			unregister_netdev(ccmni->dev);
			/* free_netdev(ccmni->dev); */
			ctlb->ccmni_inst[i] = NULL;
		}

		kfree(ctlb->ccci_ops);

ccmni_exit_ret:
		kfree(ctlb);
		ccmni_ctl_blk = NULL;
	}
}

static int ccmni_rx_callback(int ccmni_idx, struct sk_buff *skb,
		void *priv_data)
{
	struct ccmni_ctl_block *ctlb = NULL;
	/* struct ccci_header *ccci_h = (struct ccci_header*)skb->data; */
	struct ccmni_instance *ccmni = NULL;
	struct net_device *dev = NULL;
	int pkt_type, skb_len;
#if defined(CCCI_SKB_TRACE)
	struct iphdr *iph;
#endif
#if defined(CCMNI_MET_DEBUG)
	char tag_name[32] = { '\0' };
	unsigned int tag_id = 0;
#endif

	if (ccmni_idx < 0) {
		pr_err("%s : invalid ccmni index = %d\n",
			__func__, ccmni_idx);
		return -1;
	}
	ctlb = ccmni_ctl_blk;
	if (unlikely(ctlb == NULL || ctlb->ccci_ops == NULL)) {
		pr_debug("invalid CCMNI%d ctrl/ops struct\n",
			ccmni_idx);
		dev_kfree_skb(skb);
		return -1;
	}

	ccmni = ctlb->ccmni_inst[ccmni_idx];
	dev = ccmni->dev;

	pkt_type = skb->data[0] & 0xF0;

	skb_reset_transport_header(skb);
	skb_reset_network_header(skb);
	skb_set_mac_header(skb, 0);
	skb_reset_mac_len(skb);

	skb->dev = dev;
	if (pkt_type == 0x60)
		skb->protocol  = htons(ETH_P_IPV6);
	else
		skb->protocol  = htons(ETH_P_IP);

	//skb->ip_summed = CHECKSUM_NONE;
	skb_len = skb->len;

#if defined(NETDEV_TRACE) && defined(NETDEV_DL_TRACE)
	skb->dbg_flag = 0x1;
#endif

#if defined(CCCI_SKB_TRACE)
	iph = (struct iphdr *)skb->data;
	ctlb->net_rx_delay[2] = iph->id;
	ctlb->net_rx_delay[0] = dev->stats.rx_bytes + skb_len;
	ctlb->net_rx_delay[1] = dev->stats.tx_bytes;
#endif

	if (likely(ctlb->ccci_ops->md_ability & MODEM_CAP_NAPI)) {
#ifdef ENABLE_NAPI_GRO
		napi_gro_receive(ccmni->napi, skb);
#else
		netif_receive_skb(skb);
#endif
	} else {
#ifdef ENABLE_WQ_GRO
		if (is_skb_gro(skb)) {
			spin_lock_bh(ccmni->spinlock);
			napi_gro_receive(ccmni->napi, skb);
			ccmni_gro_flush(ccmni);
			spin_unlock_bh(ccmni->spinlock);
		} else {
			netif_rx_ni(skb);
		}
#else
		if (!in_interrupt())
			netif_rx_ni(skb);
		else
			netif_rx(skb);
#endif
	}
	dev->stats.rx_packets++;
	dev->stats.rx_bytes += skb_len;

#if defined(CCMNI_MET_DEBUG)
	if (ccmni->rx_met_time == 0) {
		ccmni->rx_met_time = jiffies;
		ccmni->rx_met_bytes = dev->stats.rx_bytes;
	} else if (time_after_eq(jiffies,
		ccmni->rx_met_time + msecs_to_jiffies(MET_LOG_TIMER))) {
		scnprintf(tag_name, 32, "%s_rx_bytes", dev->name);
		tag_id = CCMNI_RX_MET_ID + ccmni_idx;
		met_tag_oneshot(tag_id, tag_name,
			(dev->stats.rx_bytes - ccmni->rx_met_bytes));
		ccmni->rx_met_bytes = dev->stats.rx_bytes;
		ccmni->rx_met_time = jiffies;
	}
#endif

	__pm_wakeup_event(ctlb->ccmni_wakelock, jiffies_to_msecs(HZ));

	return 0;
}

static void ccmni_queue_state_callback(int ccmni_idx,
	enum HIF_STATE state, int is_ack)
{
	struct ccmni_ctl_block *ctlb = NULL;
	struct ccmni_instance *ccmni = NULL;
	struct ccmni_instance *ccmni_tmp = NULL;
	struct net_device *dev = NULL;
	struct netdev_queue *net_queue = NULL;

	if (ccmni_idx < 0) {
		pr_err("%s : invalid ccmni index = %d\n",
			__func__, ccmni_idx);
		return;
	}
	ctlb = ccmni_ctl_blk;
	if (unlikely(ctlb == NULL)) {
		pr_err("invalid ccmni ctrl when ccmni%d_hif_sta=%d\n",
		       ccmni_idx, state);
		return;
	}

	ccmni_tmp = ctlb->ccmni_inst[ccmni_idx];
	dev = ccmni_tmp->dev;
	ccmni = (struct ccmni_instance *)netdev_priv(dev);

	switch (state) {
#ifdef ENABLE_WQ_GRO
	case RX_FLUSH:
		spin_lock_bh(ccmni->spinlock);
		ccmni->rx_gro_cnt++;
		napi_gro_list_flush(ccmni);
		spin_unlock_bh(ccmni->spinlock);
		break;
#else
	case RX_IRQ:
		mod_timer(ccmni->timer, jiffies + HZ);
		napi_schedule(ccmni->napi);
		__pm_wakeup_event(ctlb->ccmni_wakelock, jiffies_to_msecs(HZ));
		break;
#endif

	case TX_IRQ:
		if (netif_running(ccmni->dev) &&
				atomic_read(&ccmni->usage) > 0) {
			if (likely(ctlb->ccci_ops->md_ability &
					MODEM_CAP_CCMNI_MQ)) {
				if (is_ack)
					net_queue =
						netdev_get_tx_queue(ccmni->dev,
							CCMNI_TXQ_FAST);
				else
					net_queue =
						netdev_get_tx_queue(ccmni->dev,
							CCMNI_TXQ_NORMAL);
				if (netif_tx_queue_stopped(net_queue))
					netif_tx_wake_queue(net_queue);
			} else {
				is_ack = 0;
				if (netif_queue_stopped(ccmni->dev))
					netif_wake_queue(ccmni->dev);
			}
			ccmni->tx_irq_cnt[is_ack]++;
			if ((ccmni->flags[is_ack] & CCMNI_TX_PRINT_F) ||
				time_after(jiffies,
					ccmni->tx_irq_tick[is_ack] + 2)) {
				ccmni->flags[is_ack] &= ~CCMNI_TX_PRINT_F;
				pr_info("%s(%d), idx=%d, md_sta=TX_IRQ, ack=%d, cnt(%u, %u), time=%lu\n",
					ccmni->dev->name,
					atomic_read(&ccmni->usage),
					ccmni->index,
					is_ack, ccmni->tx_full_cnt[is_ack],
					ccmni->tx_irq_cnt[is_ack],
					(jiffies - ccmni->tx_irq_tick[is_ack]));
			}
		}
		break;

	case TX_FULL:
		if (atomic_read(&ccmni->usage) > 0) {
			if (ctlb->ccci_ops->md_ability & MODEM_CAP_CCMNI_MQ) {
				if (is_ack)
					net_queue =
						netdev_get_tx_queue(ccmni->dev,
							CCMNI_TXQ_FAST);
				else
					net_queue =
						netdev_get_tx_queue(ccmni->dev,
							CCMNI_TXQ_NORMAL);
				netif_tx_stop_queue(net_queue);
			} else {
				is_ack = 0;
				netif_stop_queue(ccmni->dev);
			}
			ccmni->tx_full_cnt[is_ack]++;
			ccmni->tx_irq_tick[is_ack] = jiffies;
			if (time_after(jiffies,
					ccmni->tx_full_tick[is_ack] + 4)) {
				ccmni->tx_full_tick[is_ack] = jiffies;
				ccmni->flags[is_ack] |= CCMNI_TX_PRINT_F;
				pr_debug(
					"%s(%d), idx=%d, hif_sta=TX_FULL, ack=%d, cnt(%u, %u)\n",
					ccmni->dev->name, atomic_read(&ccmni->usage),
					ccmni->index, is_ack, ccmni->tx_full_cnt[is_ack],
					ccmni->tx_irq_cnt[is_ack]);
			}
		}
		break;
	default:
		break;
	}
}

static void ccmni_md_state_callback(int ccmni_idx, enum MD_STATE state)
{
	struct ccmni_instance *ccmni = NULL;
	struct ccmni_instance *ccmni_tmp = NULL;
	struct net_device *dev = NULL;
	int i = 0;
	unsigned long flags;

	if (ccmni_idx < 0) {
		pr_err("%s : invalid ccmni index = %d\n",
			__func__, ccmni_idx);
		return;
	}

	if (unlikely(ccmni_ctl_blk == NULL)) {
		pr_err("invalid ccmni ctrl when ccmni%d_md_sta=%d\n",
		       ccmni_idx, state);
		return;
	}
	ccmni_tmp = ccmni_ctl_blk->ccmni_inst[ccmni_idx];
	dev = ccmni_tmp->dev;
	ccmni = (struct ccmni_instance *)netdev_priv(dev);
	if (atomic_read(&ccmni->usage) > 0)
		netdev_dbg(dev, "md_state_cb: CCMNI%d, md_sta=%d, usage=%d\n",
			   ccmni_idx, state, atomic_read(&ccmni->usage));
	switch (state) {
	case READY:
		for (i = 0; i < 2; i++) {
			ccmni->tx_seq_num[i] = 0;
			ccmni->tx_full_cnt[i] = 0;
			ccmni->tx_irq_cnt[i] = 0;
			ccmni->tx_full_tick[i] = 0;
			ccmni->flags[i] &= ~CCMNI_TX_PRINT_F;
		}
		ccmni->rx_seq_num = 0;
		spin_lock_irqsave(ccmni->spinlock, flags);
		ccmni->rx_gro_cnt = 0;
		spin_unlock_irqrestore(ccmni->spinlock, flags);
		break;

	case EXCEPTION:
	case RESET:
	case WAITING_TO_STOP:
		netif_tx_disable(ccmni->dev);
		netif_carrier_off(ccmni->dev);
		break;
	default:
		break;
	}
}

static void ccmni_dump(int ccmni_idx, unsigned int flag)
{
	struct ccmni_ctl_block *ctlb = NULL;
	struct ccmni_instance *ccmni = NULL;
	struct ccmni_instance *ccmni_tmp = NULL;
	struct net_device *dev = NULL;
	struct netdev_queue *dev_queue = NULL;
	struct netdev_queue *ack_queue = NULL;
	struct Qdisc *qdisc = NULL;
	struct Qdisc *ack_qdisc = NULL;

	if (ccmni_idx < 0) {
		pr_err("%s : invalid ccmni index = %d\n",
			__func__, ccmni_idx);
		return;
	}
	ctlb = ccmni_ctl_blk;
	if (ctlb == NULL) {
		pr_err("invalid ctlb\n");
		return;
	}


	ccmni_tmp = ctlb->ccmni_inst[ccmni_idx];
	if (unlikely(ccmni_tmp == NULL))
		return;

	if ((ccmni_tmp->dev->stats.rx_packets == 0) &&
			(ccmni_tmp->dev->stats.tx_packets == 0))
		return;

	dev = ccmni_tmp->dev;
	/* ccmni diff from ccmni_tmp for MD IRAT */
	ccmni = (struct ccmni_instance *)netdev_priv(dev);
	dev_queue = netdev_get_tx_queue(dev, 0);
	netdev_info(dev, "to:clr(%lu:%lu)\r\n", timeout_flush_num, clear_flush_num);
	if (ctlb->ccci_ops->md_ability & MODEM_CAP_CCMNI_MQ) {
		ack_queue = netdev_get_tx_queue(dev, CCMNI_TXQ_FAST);
		qdisc = dev_queue->qdisc;
		ack_qdisc = ack_queue->qdisc;
		/* stats.rx_dropped is dropped in ccmni,
		 * dev->rx_dropped is dropped in net device layer
		 */
		/* stats.tx_packets is count by ccmni, bstats.
		 * packets is count by qdisc in net device layer
		 */
		netdev_info(dev,
			"%s(%d,%d), rx=(%ld,%ld,%d), tx=(%ld,%d,%lld), txq_len=(%d,%d), tx_drop=(%ld,%d,%d), rx_drop=(%ld,%ld), tx_busy=(%ld,%ld), sta=(0x%lx,0x%x,0x%lx,0x%lx)\n",
				  dev->name,
				  atomic_read(&ccmni->usage),
				  atomic_read(&ccmni_tmp->usage),
			      dev->stats.rx_packets,
				  dev->stats.rx_bytes,
				  ccmni->rx_gro_cnt,
			      dev->stats.tx_packets, qdisc->bstats.packets,
				  ack_qdisc->bstats.packets,
			      qdisc->q.qlen, ack_qdisc->q.qlen,
			      dev->stats.tx_dropped, qdisc->qstats.drops,
				  ack_qdisc->qstats.drops,
			      dev->stats.rx_dropped,
				  atomic_long_read(&dev->rx_dropped),
			      ccmni->tx_busy_cnt[0], ccmni->tx_busy_cnt[1],
			      dev->state, dev->flags, dev_queue->state,
				  ack_queue->state);
	} else
		netdev_info(dev,
			"%s(%d,%d), rx=(%ld,%ld,%d), tx=(%ld,%ld), txq_len=%d, tx_drop=(%ld,%d), rx_drop=(%ld,%ld), tx_busy=(%ld,%ld), sta=(0x%lx,0x%x,0x%lx)\n",
			      dev->name, atomic_read(&ccmni->usage),
				  atomic_read(&ccmni_tmp->usage),
			      dev->stats.rx_packets, dev->stats.rx_bytes,
				  ccmni->rx_gro_cnt,
			      dev->stats.tx_packets, dev->stats.tx_bytes,
			      dev->qdisc->q.qlen, dev->stats.tx_dropped,
				  dev->qdisc->qstats.drops,
			      dev->stats.rx_dropped,
				  atomic_long_read(&dev->rx_dropped),
				  ccmni->tx_busy_cnt[0],
			      ccmni->tx_busy_cnt[1], dev->state, dev->flags,
				  dev_queue->state);
}

static void ccmni_dump_rx_status(unsigned long long *status)
{
	if (ccmni_ctl_blk == NULL) {
		pr_err("%s : invalid ctlb\n", __func__);
		return;
	}
	status[0] = ccmni_ctl_blk->net_rx_delay[0];
	status[1] = ccmni_ctl_blk->net_rx_delay[1];
	status[2] = ccmni_ctl_blk->net_rx_delay[2];
}

static struct ccmni_ch *ccmni_get_ch(int ccmni_idx)
{
	if (ccmni_idx < 0) {
		pr_err("%s : invalid ccmni index = %d\n",
			__func__, ccmni_idx);
		return NULL;
	}

	if (ccmni_ctl_blk == NULL) {
		pr_info("%s : invalid ctlb\n", __func__);
		return NULL;
	}

	return &ccmni_ctl_blk->ccmni_inst[ccmni_idx]->ch;
}

struct ccmni_dev_ops ccmni_ops = {
	.skb_alloc_size = 1600,
	.init = &ccmni_init,
	.rx_callback = &ccmni_rx_callback,
	.md_state_callback = &ccmni_md_state_callback,
	.queue_state_callback = &ccmni_queue_state_callback,
	.exit = ccmni_exit,
	.dump = ccmni_dump,
	.dump_rx_status = ccmni_dump_rx_status,
	.get_ch = ccmni_get_ch,
	.is_ack_skb = is_ack_skb,
};
EXPORT_SYMBOL(ccmni_ops);

MODULE_AUTHOR("MTK CCCI");
MODULE_DESCRIPTION("CCCI ccmni driver v0.1");
MODULE_LICENSE("GPL");
