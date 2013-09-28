/* Copyright (c) 2013, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

/* MSM EMAC Ethernet Controller driver.
 */

#include <linux/if_ether.h>
#include <linux/if_vlan.h>
#include <linux/interrupt.h>
#include <linux/ip.h>
#include <linux/ipv6.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/tcp.h>
#include <linux/io.h>

#include "emac.h"
#include "emac_hw.h"
#include "emac_ptp.h"

#define DRV_VERSION "1.0.0.0"

char emac_drv_name[] = "msm_emac";
const char emac_drv_description[] = "Qualcomm EMAC Ethernet Driver";
const char emac_drv_version[] = DRV_VERSION;

#define EMAC_MSG_DEFAULT (NETIF_MSG_DRV | NETIF_MSG_PROBE | NETIF_MSG_LINK |  \
		NETIF_MSG_TIMER | NETIF_MSG_IFDOWN | NETIF_MSG_IFUP |         \
		NETIF_MSG_RX_ERR | NETIF_MSG_TX_ERR | NETIF_MSG_TX_QUEUED |   \
		NETIF_MSG_INTR | NETIF_MSG_TX_DONE | NETIF_MSG_RX_STATUS |    \
		NETIF_MSG_PKTDATA | NETIF_MSG_HW | NETIF_MSG_WOL)

#define EMAC_RRDESC_SIZE      4
#define EMAC_TS_RRDESC_SIZE   6
#define EMAC_TPDESC_SIZE      4
#define EMAC_RFDESC_SIZE      2

#define EMAC_RSS_IDT_SIZE     256

static int msm_emac_msglvl = -1;
module_param_named(msglvl, msm_emac_msglvl, int, S_IRUGO | S_IWUSR | S_IWGRP);

static int emac_up(struct emac_adapter *adpt);
static void emac_down(struct emac_adapter *adpt, u32 ctrl);
static irqreturn_t emac_interrupt(int irq, void *data);

/* EMAC HW has an issue with interrupt assignment because of which
   following receive rss queue to interrupt mapping is used.
   intr      rss-queue
   core0       0, 1
   core1       2
   core2       3
   core3       x (unsigned)
*/
static struct emac_irq_info emac_irq[EMAC_NUM_CORE_IRQ] = {
	{ 0, "emac_core0_irq", emac_interrupt, EMAC_INT_STATUS,
	  EMAC_INT_MASK, IMR_NORMAL_MASK | RX_PKT_INT1, NULL, NULL },
	{ 0, "emac_core1_irq", emac_interrupt, EMAC_INT1_STATUS,
	  EMAC_INT1_MASK, RX_PKT_INT2, NULL, NULL },
	{ 0, "emac_core2_irq", emac_interrupt, EMAC_INT2_STATUS,
	  EMAC_INT2_MASK, RX_PKT_INT3, NULL, NULL },
	{ 0, "emac_core3_irq", emac_interrupt, EMAC_INT3_STATUS,
	  EMAC_INT3_MASK, 0, NULL, NULL },
};

/* reinitialize */
void emac_reinit_locked(struct emac_adapter *adpt)
{
	WARN_ON(in_interrupt());

	while (CHK_AND_SET_ADPT_FLAG(STATE_RESETTING))
		msleep(20); /* Reset might take few 10s of ms */

	if (CHK_ADPT_FLAG(STATE_DOWN)) {
		CLI_ADPT_FLAG(STATE_RESETTING);
		return;
	}

	emac_down(adpt, EMAC_HW_CTRL_RESET_MAC);
	emac_up(adpt);

	CLI_ADPT_FLAG(STATE_RESETTING);
}

static void emac_task_schedule(struct emac_adapter *adpt)
{
	if (!CHK_ADPT_FLAG(STATE_DOWN) &&
	    !CHK_ADPT_FLAG(STATE_WATCH_DOG)) {
		SET_ADPT_FLAG(STATE_WATCH_DOG);
		schedule_work(&adpt->emac_task);
	}
}

static void emac_check_lsc(struct emac_adapter *adpt)
{
	SET_ADPT_FLAG(TASK_LSC_REQ);
	adpt->link_jiffies = jiffies + EMAC_TRY_LINK_TIMEOUT;

	if (!CHK_ADPT_FLAG(STATE_DOWN))
		emac_task_schedule(adpt);
}

/* Respond to a TX hang */
static void emac_tx_timeout(struct net_device *netdev)
{
	struct emac_adapter *adpt = netdev_priv(netdev);

	if (!CHK_ADPT_FLAG(STATE_DOWN)) {
		SET_ADPT_FLAG(TASK_REINIT_REQ);
		emac_task_schedule(adpt);
	}
}

/* Configure Multicast and Promiscuous modes */
static void emac_set_rx_mode(struct net_device *netdev)
{
	struct emac_adapter *adpt = netdev_priv(netdev);
	struct emac_hw *hw = &adpt->hw;
	struct netdev_hw_addr *ha;

	/* Check for Promiscuous and All Multicast modes */
	if (netdev->flags & IFF_PROMISC) {
		SET_HW_FLAG(PROMISC_EN);
	} else if (netdev->flags & IFF_ALLMULTI) {
		SET_HW_FLAG(MULTIALL_EN);
		CLI_HW_FLAG(PROMISC_EN);
	} else {
		CLI_HW_FLAG(MULTIALL_EN);
		CLI_HW_FLAG(PROMISC_EN);
	}
	emac_hw_config_mac_ctrl(hw);

	/* update multicast address filtering */
	emac_hw_clear_mc_addr(hw);
	netdev_for_each_mc_addr(ha, netdev)
		emac_hw_set_mc_addr(hw, ha->addr);
}

/* Change MAC address */
static int emac_set_mac_address(struct net_device *netdev, void *p)
{
	struct emac_adapter *adpt = netdev_priv(netdev);
	struct emac_hw *hw = &adpt->hw;
	struct sockaddr *addr = p;

	if (!is_valid_ether_addr(addr->sa_data))
		return -EADDRNOTAVAIL;

	if (netif_running(netdev))
		return -EBUSY;

	memcpy(netdev->dev_addr, addr->sa_data, netdev->addr_len);
	memcpy(hw->mac_addr, addr->sa_data, netdev->addr_len);

	emac_hw_set_mac_addr(hw, hw->mac_addr);
	return 0;
}

/* Push the received skb to upper layers */
static void emac_receive_skb(struct emac_rx_queue *rxque,
			     struct sk_buff *skb,
			     u16 vlan_tag, bool vlan_flag)
{
	if (vlan_flag) {
		u16 vlan;
		EMAC_TAG_TO_VLAN(vlan_tag, vlan);
		__vlan_hwaccel_put_tag(skb, vlan);
	}

	napi_gro_receive(&rxque->napi, skb);
}

/* Consume next received packet descriptor */
static bool emac_get_rrdesc(struct emac_rx_queue *rxque,
			    union emac_sw_rrdesc *srrd)
{
	struct emac_adapter *adpt = netdev_priv(rxque->netdev);
	u32 *hrrd = EMAC_RRD(rxque, adpt->rrdesc_size,
			     rxque->rrd.consume_idx);
	u8 i = 0;

	/* If time stamping is enabled, it will be added in the beginning of
	 * the hw rrd (hrrd). In sw rrd (srrd), dwords 4 & 5 are reserved for
	 * the time stamp; hence the conversion.
	 */
	if (adpt->tstamp_en) {
		srrd->dfmt.dw[4] = readl_relaxed(hrrd + i); i++;
		srrd->dfmt.dw[5] = readl_relaxed(hrrd + i); i++;
	} else {
		srrd->dfmt.dw[4] = 0;
		srrd->dfmt.dw[5] = 0;
	}

	srrd->dfmt.dw[0] = readl_relaxed(hrrd + i); i++;
	srrd->dfmt.dw[1] = readl_relaxed(hrrd + i); i++;
	srrd->dfmt.dw[2] = readl_relaxed(hrrd + i); i++;
	srrd->dfmt.dw[3] = readl_relaxed(hrrd + i);

	if (!srrd->genr.update)
		return false;

	emac_dbg(adpt, rx_status, "RX[%d]:SRRD[%x]: %x:%x:%x:%x:%x:%x\n",
		 rxque->que_idx, rxque->rrd.consume_idx, srrd->dfmt.dw[0],
		 srrd->dfmt.dw[1], srrd->dfmt.dw[2], srrd->dfmt.dw[3],
		 srrd->dfmt.dw[4], srrd->dfmt.dw[5]);

	if (unlikely(srrd->genr.nor != 1)) {
		/* multiple rfd not supported */
		emac_err(adpt, "Multi rfd not support yet! nor = %d\n",
			 srrd->genr.nor);
	}

	/* mark rrd as processed */
	srrd->genr.update = 0;
	writel_relaxed(srrd->dfmt.dw[3], hrrd + i);

	if (++rxque->rrd.consume_idx == rxque->rrd.count)
		rxque->rrd.consume_idx = 0;

	return true;
}

/* Produce new receive free descriptor */
static bool emac_set_rfdesc(struct emac_rx_queue *rxque,
			    union emac_sw_rfdesc *srfd)
{
	struct emac_adapter *adpt = netdev_priv(rxque->netdev);
	u32 *hrfd = EMAC_RFD(rxque, adpt->rfdesc_size,
			     rxque->rfd.produce_idx);

	writel_relaxed(srfd->dfmt.dw[0], hrfd);
	writel_relaxed(srfd->dfmt.dw[1], hrfd + 1);

	if (++rxque->rfd.produce_idx == rxque->rfd.count)
		rxque->rfd.produce_idx = 0;

	return true;
}

/* Produce new transmit descriptor */
static bool emac_set_tpdesc(struct emac_tx_queue *txque,
			    union emac_sw_tpdesc *stpd)
{
	struct emac_adapter *adpt = netdev_priv(txque->netdev);
	u32 *htpd;

	txque->tpd.last_produce_idx = txque->tpd.produce_idx;
	htpd = EMAC_TPD(txque, adpt->tpdesc_size, txque->tpd.produce_idx);

	if (++txque->tpd.produce_idx == txque->tpd.count)
		txque->tpd.produce_idx = 0;

	writel_relaxed(stpd->dfmt.dw[0], htpd);
	writel_relaxed(stpd->dfmt.dw[1], htpd + 1);
	writel_relaxed(stpd->dfmt.dw[2], htpd + 2);
	writel_relaxed(stpd->dfmt.dw[3], htpd + 3);

	emac_dbg(adpt, tx_done, "TX[%d]:STPD[%x]: %x:%x:%x:%x\n",
		 txque->que_idx, txque->tpd.last_produce_idx, stpd->dfmt.dw[0],
		 stpd->dfmt.dw[1], stpd->dfmt.dw[2], stpd->dfmt.dw[3]);

	return true;
}

/* Mark the last transmit descriptor as such (for the transmit packet) */
static void emac_set_tpdesc_lastfrag(struct emac_tx_queue *txque)
{
	struct emac_adapter *adpt = netdev_priv(txque->netdev);
	u32 tmp_tpd;
	u32 *htpd = EMAC_TPD(txque, adpt->tpdesc_size,
			     txque->tpd.last_produce_idx);

	tmp_tpd = readl_relaxed(htpd + 1);
	tmp_tpd |= EMAC_TPD_LAST_FRAGMENT;
	writel_relaxed(tmp_tpd, htpd + 1);
}

void emac_set_tpdesc_tstamp_sav(struct emac_tx_queue *txque)
{
	struct emac_adapter *adpt = netdev_priv(txque->netdev);
	u32 tmp_tpd;
	u32 *htpd = EMAC_TPD(txque, adpt->tpdesc_size,
			     txque->tpd.last_produce_idx);

	tmp_tpd = readl_relaxed(htpd + 3);
	tmp_tpd |= EMAC_TPD_TSTAMP_SAVE;
	writel_relaxed(tmp_tpd, htpd + 3);
	wmb();
}

/* Fill up receive queue's RFD with preallocated receive buffers */
static int emac_refresh_rx_buffer(struct emac_rx_queue *rxque)
{
	struct emac_adapter *adpt = netdev_priv(rxque->netdev);
	struct emac_hw *hw = &adpt->hw;
	struct emac_buffer *curr_rxbuf;
	struct emac_buffer *next_rxbuf;
	union emac_sw_rfdesc srfd;
	struct sk_buff *skb;
	void *skb_data = NULL;
	u16 count = 0;
	u16 next_produce_idx;

	next_produce_idx = rxque->rfd.produce_idx;
	if (++next_produce_idx == rxque->rfd.count)
		next_produce_idx = 0;
	curr_rxbuf = GET_RFD_BUFFER(rxque, rxque->rfd.produce_idx);
	next_rxbuf = GET_RFD_BUFFER(rxque, next_produce_idx);

	/* this always has a blank rx_buffer*/
	while (next_rxbuf->dma == 0) {
		skb = dev_alloc_skb(adpt->rxbuf_size + NET_IP_ALIGN);
		if (unlikely(!skb)) {
			emac_err(adpt, "alloc rx buffer failed\n");
			break;
		}

		/* Make buffer alignment 2 beyond a 16 byte boundary
		 * this will result in a 16 byte aligned IP header after
		 * the 14 byte MAC header is removed
		 */
		skb_reserve(skb, NET_IP_ALIGN);
		skb_data = skb->data;
		curr_rxbuf->skb = skb;
		curr_rxbuf->length = adpt->rxbuf_size;
		curr_rxbuf->dma = dma_map_single(rxque->dev, skb_data,
						 curr_rxbuf->length,
						 DMA_FROM_DEVICE);
		srfd.genr.addr = curr_rxbuf->dma;
		emac_set_rfdesc(rxque, &srfd);
		next_produce_idx = rxque->rfd.produce_idx;
		if (++next_produce_idx == rxque->rfd.count)
			next_produce_idx = 0;

		curr_rxbuf = GET_RFD_BUFFER(rxque, rxque->rfd.produce_idx);
		next_rxbuf = GET_RFD_BUFFER(rxque, next_produce_idx);
		count++;
	}

	if (count) {
		u32 prod_idx = (rxque->rfd.produce_idx << rxque->produce_shft) &
				rxque->produce_mask;
		wmb(); /* ensure that the descriptors are properly set */
		emac_reg_update32(hw, EMAC, rxque->produce_reg,
				  rxque->produce_mask, prod_idx);
		wmb();
		emac_dbg(adpt, rx_status, "RX[%d]: prod idx 0x%x\n",
			 rxque->que_idx, rxque->rfd.produce_idx);
	}

	return count;
}

static void emac_clean_rfdesc(struct emac_rx_queue *rxque,
			      union emac_sw_rrdesc *srrd)
{
	struct emac_buffer *rfbuf = rxque->rfd.rfbuff;
	u16 consume_idx = srrd->genr.si;
	u16 i;

	for (i = 0; i < srrd->genr.nor; i++) {
		rfbuf[consume_idx].skb = NULL;
		if (++consume_idx == rxque->rfd.count)
			consume_idx = 0;
	}

	rxque->rfd.consume_idx = consume_idx;
	rxque->rfd.process_idx = consume_idx;
}

static void emac_read_tx_tstamp_fifo(struct emac_hw *hw,
				     struct emac_tx_queue *txque)
{
	struct emac_buffer *tpbuf;
	u32 ts_idx = 0;
	u32 sec, ns;

	while (1) {
		ts_idx = emac_reg_r32(hw, EMAC_CSR,
				      EMAC_EMAC_WRAPPER_TX_TS_INX);
		if (ts_idx & EMAC_WRAPPER_TX_TS_EMPTY)
			break;

		ns = emac_reg_r32(hw, EMAC_CSR, EMAC_EMAC_WRAPPER_TX_TS_LO);
		sec = emac_reg_r32(hw, EMAC_CSR, EMAC_EMAC_WRAPPER_TX_TS_HI);

		ts_idx &= EMAC_WRAPPER_TX_TS_INX_BMSK;
		if ((ts_idx < txque->tpd.consume_idx) ||
		    (ts_idx > txque->tpd.last_produce_idx)) {
			emac_warn(hw->adpt, tx_done,
				  "zombie timestamp desc idx %d\n", ts_idx);
			continue;
		}

		tpbuf  = GET_TPD_BUFFER(txque, ts_idx);

		if (tpbuf->skb &&
		    (skb_shinfo(tpbuf->skb)->tx_flags & SKBTX_HW_TSTAMP)) {
			struct skb_shared_hwtstamps ts;

			ts.hwtstamp = ktime_set(sec, ns);
			skb_tstamp_tx(tpbuf->skb, &ts);
		}
	}
}

/* Process receive event */
static void emac_handle_rx(struct emac_adapter *adpt,
			   struct emac_rx_queue *rxque,
			   int *num_pkts, int max_pkts)
{
	struct emac_hw *hw = &adpt->hw;
	struct net_device *netdev  = adpt->netdev;

	union emac_sw_rrdesc srrd;
	struct emac_buffer *rfbuf;
	struct sk_buff *skb;

	u16 hw_consume_idx, num_consume_pkts;
	u16 count = 0;
	u32 proc_idx;

	hw_consume_idx = emac_reg_field_r32(hw, EMAC, rxque->consume_reg,
					    rxque->consume_mask,
					    rxque->consume_shft);
	num_consume_pkts = (hw_consume_idx >= rxque->rrd.consume_idx) ?
		(hw_consume_idx -  rxque->rrd.consume_idx) :
		(hw_consume_idx + rxque->rrd.count - rxque->rrd.consume_idx);

	while (1) {
		if (!num_consume_pkts)
			break;

		if (!emac_get_rrdesc(rxque, &srrd))
			break;

		if (likely(srrd.genr.nor == 1)) {
			/* good receive */
			rfbuf = GET_RFD_BUFFER(rxque, srrd.genr.si);
			dma_unmap_single(rxque->dev, rfbuf->dma, rfbuf->length,
					 DMA_FROM_DEVICE);
			rfbuf->dma = 0;
			skb = rfbuf->skb;
		} else {
			/* multi rfd not supported */
			emac_err(adpt, "Multi rfd not support yet!\n");
			break;
		}
		emac_clean_rfdesc(rxque, &srrd);
		num_consume_pkts--;
		count++;

		if (srrd.genr.res || srrd.genr.lene) {
			dev_kfree_skb(rfbuf->skb);
			emac_warn(adpt, rx_err, "received packet has errors\n");
			continue;
		}

		skb_put(skb, srrd.genr.pkt_len - ETH_FCS_LEN);
		skb->ip_summed = CHECKSUM_NONE;
		skb->dev = netdev;
		skb->protocol = eth_type_trans(skb, skb->dev);

		if (CHK_HW_FLAG(TS_RX_EN)) {
			struct skb_shared_hwtstamps *hwts = skb_hwtstamps(skb);

			hwts->hwtstamp = ktime_set(srrd.genr.ts_high,
						   srrd.genr.ts_low);
		}

		emac_receive_skb(rxque, skb, (u16)srrd.genr.cvlan_tag,
				 (bool)srrd.genr.cvlan_flag);

		netdev->last_rx = jiffies;
		(*num_pkts)++;
		if (*num_pkts >= max_pkts)
			break;
	}

	if (count) {
		proc_idx = (rxque->rfd.process_idx << rxque->process_shft) &
				rxque->process_mask;
		wmb(); /* ensure that the descriptors are properly cleared */
		emac_reg_update32(hw, EMAC, rxque->process_reg,
				  rxque->process_mask, proc_idx);
		wmb();
		emac_dbg(adpt, rx_status, "RX[%d]: proc idx 0x%x\n",
			 rxque->que_idx, rxque->rfd.process_idx);

		emac_refresh_rx_buffer(rxque);
	}
}

/* Process transmit event */
static void emac_handle_tx(struct emac_adapter *adpt,
			   struct emac_tx_queue *txque)
{
	struct emac_hw *hw = &adpt->hw;
	struct emac_buffer *tpbuf;
	u16 hw_consume_idx;

	hw_consume_idx = emac_reg_field_r32(hw, EMAC, txque->consume_reg,
					    txque->consume_mask,
					    txque->consume_shft);
	emac_dbg(adpt, tx_done, "TX[%d]: cons idx 0x%x\n",
		 txque->que_idx, hw_consume_idx);

	while (txque->tpd.consume_idx != hw_consume_idx) {
		emac_read_tx_tstamp_fifo(hw, txque);
		tpbuf = GET_TPD_BUFFER(txque, txque->tpd.consume_idx);
		if (tpbuf->dma) {
			dma_unmap_single(txque->dev, tpbuf->dma, tpbuf->length,
					 DMA_TO_DEVICE);
			tpbuf->dma = 0;
		}

		if (tpbuf->skb) {
			dev_kfree_skb_irq(tpbuf->skb);
			tpbuf->skb = NULL;
		}

		if (++txque->tpd.consume_idx == txque->tpd.count)
			txque->tpd.consume_idx = 0;
	}

	if (netif_queue_stopped(adpt->netdev) &&
	    netif_carrier_ok(adpt->netdev)) {
		netif_wake_queue(adpt->netdev);
	}
}

/* NAPI */
static int emac_napi_rtx(struct napi_struct *napi, int budget)
{
	struct emac_rx_queue *rxque = container_of(napi, struct emac_rx_queue,
						   napi);
	struct emac_adapter *adpt = netdev_priv(rxque->netdev);
	struct emac_irq_info *irq_info = rxque->irq_info;
	struct emac_hw *hw = &adpt->hw;
	int work_done = 0;

	/* Keep link state information with original netdev */
	if (!netif_carrier_ok(adpt->netdev))
		goto quit_polling;

	emac_handle_rx(adpt, rxque, &work_done, budget);

	if (work_done < budget) {
quit_polling:
		napi_complete(napi);

		irq_info->mask |= rxque->intr;
		emac_reg_w32(hw, EMAC, irq_info->mask_reg, irq_info->mask);
		wmb();
	}

	return work_done;
}

/* Check if enough transmit descriptors available */
static bool emac_check_num_tpdescs(struct emac_tx_queue *txque,
				   const struct sk_buff *skb)
{
	u16 num_required = 1;
	u16 num_available = 0;
	u16 produce_idx = txque->tpd.produce_idx;
	u16 consume_idx = txque->tpd.consume_idx;
	u16 i = 0;

	u16 proto_hdr_len = 0;
	if (skb_is_gso(skb)) {
		proto_hdr_len = skb_transport_offset(skb) + tcp_hdrlen(skb);
		if (proto_hdr_len < skb_headlen(skb))
			num_required++;
		if (skb_shinfo(skb)->gso_type & SKB_GSO_TCPV6)
			num_required++;
	}

	for (i = 0; i < skb_shinfo(skb)->nr_frags; i++)
		num_required++;

	num_available = (u16)(consume_idx > produce_idx) ?
		(consume_idx - produce_idx - 1) :
		(txque->tpd.count + consume_idx - produce_idx - 1);

	return num_required < num_available;
}

/* Fill up transmit descriptors with TSO and Checksum offload information */
static int emac_tso_csum(struct emac_adapter *adpt,
			 struct emac_tx_queue *txque,
			 struct sk_buff *skb,
			 union emac_sw_tpdesc *stpd)
{
	u8  hdr_len;
	int retval;

	if (skb_is_gso(skb)) {
		if (skb_header_cloned(skb)) {
			retval = pskb_expand_head(skb, 0, 0, GFP_ATOMIC);
			if (unlikely(retval))
				return retval;
		}

		if (skb->protocol == htons(ETH_P_IP)) {
			u32 pkt_len =
				((unsigned char *)ip_hdr(skb) - skb->data) +
				ntohs(ip_hdr(skb)->tot_len);
			if (skb->len > pkt_len)
				pskb_trim(skb, pkt_len);
		}

		hdr_len = skb_transport_offset(skb) + tcp_hdrlen(skb);
		if (unlikely(skb->len == hdr_len)) {
			/* we only need to do csum */
			emac_warn(adpt, tx_err,
				  "tso not needed for packet with 0 data\n");
			goto do_csum;
		}

		if (skb_shinfo(skb)->gso_type & SKB_GSO_TCPV4) {
			ip_hdr(skb)->check = 0;
			tcp_hdr(skb)->check = ~csum_tcpudp_magic(
						ip_hdr(skb)->saddr,
						ip_hdr(skb)->daddr,
						0, IPPROTO_TCP, 0);
			stpd->genr.ipv4 = 1;
		}

		if (skb_shinfo(skb)->gso_type & SKB_GSO_TCPV6) {
			/* ipv6 tso need an extra tpd */
			union emac_sw_tpdesc extra_tpd;

			memset(stpd, 0, sizeof(union emac_sw_tpdesc));
			memset(&extra_tpd, 0, sizeof(union emac_sw_tpdesc));

			ipv6_hdr(skb)->payload_len = 0;
			tcp_hdr(skb)->check = ~csum_ipv6_magic(
						&ipv6_hdr(skb)->saddr,
						&ipv6_hdr(skb)->daddr,
						0, IPPROTO_TCP, 0);
			extra_tpd.tso.pkt_len = skb->len;
			extra_tpd.tso.lso = 0x1;
			extra_tpd.tso.lso_v2 = 0x1;
			emac_set_tpdesc(txque, &extra_tpd);
			stpd->tso.lso_v2 = 0x1;
		}

		stpd->tso.lso = 0x1;
		stpd->tso.tcphdr_offset = skb_transport_offset(skb);
		stpd->tso.mss = skb_shinfo(skb)->gso_size;
		return 0;
	}

do_csum:
	if (likely(skb->ip_summed == CHECKSUM_PARTIAL)) {
		u8 css, cso;

		cso = skb_transport_offset(skb);
		if (unlikely(cso & 0x1)) {
			emac_err(adpt, "payload offset should be even\n");
			return -EINVAL;
		} else {
			css = cso + skb->csum_offset;

			stpd->csum.payld_offset = cso >> 1;
			stpd->csum.cxsum_offset = css >> 1;
			stpd->csum.c_csum = 0x1;
		}
	}

	return 0;
}

/* Fill up transmit descriptors */
static void emac_tx_map(struct emac_adapter *adpt,
			struct emac_tx_queue *txque,
			struct sk_buff *skb,
			union emac_sw_tpdesc *stpd)
{
	struct emac_hw  *hw = &adpt->hw;
	struct emac_buffer *tpbuf = NULL;
	u16 nr_frags = skb_shinfo(skb)->nr_frags;
	u32 len = skb_headlen(skb);
	u16 map_len = 0;
	u16 mapped_len = 0;
	u16 hdr_len = 0;
	u16 i;
	u32 tso = stpd->tso.lso;

	if (tso) {
		map_len = hdr_len = skb_transport_offset(skb) + tcp_hdrlen(skb);

		tpbuf = GET_TPD_BUFFER(txque, txque->tpd.produce_idx);
		tpbuf->length = map_len;
		tpbuf->dma = dma_map_single(txque->dev,	skb->data,
					    hdr_len, DMA_TO_DEVICE);
		mapped_len += map_len;
		stpd->genr.addr_lo = EMAC_DMA_ADDR_LO(tpbuf->dma);
		stpd->genr.addr_hi = EMAC_DMA_ADDR_HI(tpbuf->dma);
		stpd->genr.buffer_len = tpbuf->length;
		emac_set_tpdesc(txque, stpd);
	}

	if (mapped_len < len) {
		tpbuf = GET_TPD_BUFFER(txque, txque->tpd.produce_idx);
		tpbuf->length = len - mapped_len;
		tpbuf->dma = dma_map_single(txque->dev, skb->data + mapped_len,
					    tpbuf->length, DMA_TO_DEVICE);
		stpd->genr.addr_lo = EMAC_DMA_ADDR_LO(tpbuf->dma);
		stpd->genr.addr_hi = EMAC_DMA_ADDR_HI(tpbuf->dma);
		stpd->genr.buffer_len  = tpbuf->length;
		emac_set_tpdesc(txque, stpd);
	}

	for (i = 0; i < nr_frags; i++) {
		struct skb_frag_struct *frag;

		frag = &skb_shinfo(skb)->frags[i];

		tpbuf = GET_TPD_BUFFER(txque, txque->tpd.produce_idx);
		tpbuf->length = frag->size;
		tpbuf->dma = dma_map_page(txque->dev, frag->page.p,
					  frag->page_offset,
					  tpbuf->length,
					  DMA_TO_DEVICE);
		stpd->genr.addr_lo = EMAC_DMA_ADDR_LO(tpbuf->dma);
		stpd->genr.addr_hi = EMAC_DMA_ADDR_HI(tpbuf->dma);
		stpd->genr.buffer_len  = tpbuf->length;
		emac_set_tpdesc(txque, stpd);
	}

	/* The last tpd */
	emac_set_tpdesc_lastfrag(txque);

	if (CHK_HW_FLAG(TS_TX_EN) &&
	    (skb_shinfo(skb)->tx_flags & SKBTX_HW_TSTAMP)) {
		emac_set_tpdesc_tstamp_sav(txque);
		skb_shinfo(skb)->tx_flags |= SKBTX_IN_PROGRESS;
	}

	/* The last buffer info contain the skb address,
	 * so it will be freed after unmap
	 */
	tpbuf->skb = skb;
}

/* Transmit the packet using specified transmit queue */
static int emac_start_xmit_frame(struct emac_adapter *adpt,
				 struct emac_tx_queue *txque,
				 struct sk_buff *skb)
{
	struct emac_hw  *hw = &adpt->hw;
	union emac_sw_tpdesc stpd;
	u32 prod_idx;

	if (CHK_ADPT_FLAG(STATE_DOWN)) {
		dev_kfree_skb_any(skb);
		return NETDEV_TX_OK;
	}

	if (!emac_check_num_tpdescs(txque, skb)) {
		/* not enough descriptors, just stop queue */
		netif_stop_queue(adpt->netdev);
		return NETDEV_TX_BUSY;
	}

	memset(&stpd, 0, sizeof(union emac_sw_tpdesc));

	if (emac_tso_csum(adpt, txque, skb, &stpd) != 0) {
		dev_kfree_skb_any(skb);
		return NETDEV_TX_OK;
	}

	if (vlan_tx_tag_present(skb)) {
		u16 vlan = vlan_tx_tag_get(skb);
		u16 tag;
		EMAC_VLAN_TO_TAG(vlan, tag);
		stpd.genr.cvlan_tag = tag;
		stpd.genr.ins_cvtag = 0x1;
	}

	if (skb_network_offset(skb) != ETH_HLEN)
		stpd.genr.type = 0x1;

	emac_tx_map(adpt, txque, skb, &stpd);

	/* update produce idx */
	prod_idx = (txque->tpd.produce_idx << txque->produce_shft) &
			txque->produce_mask;
	wmb(); /* ensure that the descriptors are properly set */
	emac_reg_update32(hw, EMAC, txque->produce_reg,
			  txque->produce_mask, prod_idx);
	wmb();
	emac_dbg(adpt, tx_queued, "TX[%d]: prod idx 0x%x\n",
		 txque->que_idx, txque->tpd.produce_idx);

	return NETDEV_TX_OK;
}

/* Transmit the packet */
static int emac_start_xmit(struct sk_buff *skb,
			   struct net_device *netdev)
{
	struct emac_adapter *adpt = netdev_priv(netdev);
	struct emac_tx_queue *txque;

	txque = &adpt->tx_queue[EMAC_ACTIVE_TXQ];
	return emac_start_xmit_frame(adpt, txque, skb);
}

/* ISR */
static irqreturn_t emac_interrupt(int irq, void *data)
{
	struct emac_irq_info *irq_info = data;
	struct emac_adapter *adpt = irq_info->adpt;
	struct emac_hw *hw = &adpt->hw;
	int max_ints = EMAC_MAX_HANDLED_INTRS;
	u32 isr, status;

	do {
		isr = emac_reg_r32(hw, EMAC, irq_info->status_reg);
		status = isr & irq_info->mask;

		if (status == 0) {
			emac_reg_w32(hw, EMAC, irq_info->status_reg, 0);
			wmb();
			if (max_ints != EMAC_MAX_HANDLED_INTRS)
				return IRQ_HANDLED;
			return IRQ_NONE;
		}

		/* ack PHY interrupt */
		if (status & ISR_GPHY_LINK)
			emac_hw_ack_phy_intr(hw);

		/* Ack MAC interrupt and disable the interrupt */
		emac_reg_w32(hw, EMAC, irq_info->status_reg, status | DIS_INT);
		wmb();

		if (status & ISR_ERROR) {
			emac_warn(adpt, intr, "isr error status 0x%x\n",
				  status & ISR_ERROR);
			/* reset MAC */
			SET_ADPT_FLAG(TASK_REINIT_REQ);
			emac_task_schedule(adpt);
		}

		/* Schedule the napi for receive queue with interrupt
		 * status bit set
		 */
		if ((status & irq_info->rxque->intr)) {
			if (napi_schedule_prep(&irq_info->rxque->napi)) {
				irq_info->mask &= ~irq_info->rxque->intr;
				emac_reg_w32(hw, EMAC, irq_info->mask_reg,
					     irq_info->mask);
				__napi_schedule(&irq_info->rxque->napi);
			}
		}

		if (status & ISR_TX_PKT) {
			if (status & TX_PKT_INT)
				emac_handle_tx(adpt, &adpt->tx_queue[0]);
			if (status & TX_PKT_INT1)
				emac_handle_tx(adpt, &adpt->tx_queue[1]);
			if (status & TX_PKT_INT2)
				emac_handle_tx(adpt, &adpt->tx_queue[2]);
			if (status & TX_PKT_INT3)
				emac_handle_tx(adpt, &adpt->tx_queue[3]);
		}

		if (status & ISR_OVER) {
			emac_err(adpt, "TX/RX overflow status 0x%x\n",
				 status & ISR_OVER);
		}

		/* link event */
		if (status & (ISR_GPHY_LINK | SW_MAN_INT)) {
			emac_check_lsc(adpt);
			break;
		}

	} while (--max_ints > 0);

	/* enable the interrupt */
	emac_reg_w32(hw, EMAC, irq_info->status_reg, 0);
	wmb();
	return IRQ_HANDLED;
}

/* Enable interrupts */
static inline void emac_enable_intr(struct emac_adapter *adpt)
{
	struct emac_hw *hw = &adpt->hw;

	emac_hw_enable_intr(hw);
}

/* Disable interrupts */
static inline void emac_disable_intr(struct emac_adapter *adpt)
{
	struct emac_hw *hw = &adpt->hw;
	int i;

	emac_hw_disable_intr(hw);
	for (i = 0; i < EMAC_NUM_CORE_IRQ; i++)
		synchronize_irq(adpt->irq_info[i].irq);
}

/* Configure VLAN tag strip/insert feature */
static int emac_set_features(struct net_device *netdev,
			     netdev_features_t features)
{
	struct emac_adapter *adpt = netdev_priv(netdev);
	struct emac_hw *hw = &adpt->hw;
	netdev_features_t changed = features ^ netdev->features;

	if (!(changed & (NETIF_F_HW_VLAN_TX | NETIF_F_HW_VLAN_RX)))
		return 0;

	netdev->features = features;
	if (netdev->features & NETIF_F_HW_VLAN_RX)
		SET_HW_FLAG(VLANSTRIP_EN);
	else
		CLI_HW_FLAG(VLANSTRIP_EN);

	if (netif_running(netdev))
		emac_reinit_locked(adpt);

	return 0;
}

static void emac_napi_enable_all(struct emac_adapter *adpt)
{
	u8 i;

	for (i = 0; i < adpt->num_rxques; i++)
		napi_enable(&adpt->rx_queue[i].napi);
}

static void emac_napi_disable_all(struct emac_adapter *adpt)
{
	u8 i;

	for (i = 0; i < adpt->num_rxques; i++)
		napi_disable(&adpt->rx_queue[i].napi);
}

/* Free all descriptors of given transmit queue */
static void emac_clean_tx_queue(struct emac_tx_queue *txque)
{
	struct device *dev = txque->dev;
	unsigned long size;
	u16 i;

	/* ring already cleared, nothing to do */
	if (!txque->tpd.tpbuff)
		return;

	for (i = 0; i < txque->tpd.count; i++) {
		struct emac_buffer *tpbuf;
		tpbuf = GET_TPD_BUFFER(txque, i);
		if (tpbuf->dma) {
			dma_unmap_single(dev, tpbuf->dma, tpbuf->length,
					 DMA_TO_DEVICE);
			tpbuf->dma = 0;
		}
		if (tpbuf->skb) {
			dev_kfree_skb_any(tpbuf->skb);
			tpbuf->skb = NULL;
		}
	}

	size = sizeof(struct emac_buffer) * txque->tpd.count;
	memset(txque->tpd.tpbuff, 0, size);

	/* clear the descriptor ring */
	memset(txque->tpd.tpdesc, 0, txque->tpd.size);

	txque->tpd.consume_idx = 0;
	txque->tpd.produce_idx = 0;
}

static void emac_clean_all_tx_queues(struct emac_adapter *adpt)
{
	u8 i;

	for (i = 0; i < adpt->num_txques; i++)
		emac_clean_tx_queue(&adpt->tx_queue[i]);
}

/* Free all descriptors of given receive queue */
static void emac_clean_rx_queue(struct emac_rx_queue *rxque)
{
	struct device *dev = rxque->dev;
	unsigned long size;
	u16 i;

	/* ring already cleared, nothing to do */
	if (!rxque->rfd.rfbuff)
		return;

	for (i = 0; i < rxque->rfd.count; i++) {
		struct emac_buffer *rfbuf;
		rfbuf = GET_RFD_BUFFER(rxque, i);
		if (rfbuf->dma) {
			dma_unmap_single(dev, rfbuf->dma, rfbuf->length,
					 DMA_FROM_DEVICE);
			rfbuf->dma = 0;
		}
		if (rfbuf->skb) {
			dev_kfree_skb(rfbuf->skb);
			rfbuf->skb = NULL;
		}
	}

	size =  sizeof(struct emac_buffer) * rxque->rfd.count;
	memset(rxque->rfd.rfbuff, 0, size);

	/* clear the descriptor rings */
	memset(rxque->rrd.rrdesc, 0, rxque->rrd.size);
	rxque->rrd.produce_idx = 0;
	rxque->rrd.consume_idx = 0;

	memset(rxque->rfd.rfdesc, 0, rxque->rfd.size);
	rxque->rfd.produce_idx = 0;
	rxque->rfd.consume_idx = 0;
}

static void emac_clean_all_rx_queues(struct emac_adapter *adpt)
{
	u8 i;

	for (i = 0; i < adpt->num_rxques; i++)
		emac_clean_rx_queue(&adpt->rx_queue[i]);
}

/* Free all buffers associated with given transmit queue */
static void emac_free_tx_descriptor(struct emac_tx_queue *txque)
{
	emac_clean_tx_queue(txque);

	kfree(txque->tpd.tpbuff);
	txque->tpd.tpbuff = NULL;
	txque->tpd.tpdesc = NULL;
	txque->tpd.tpdma = 0;
	txque->tpd.size = 0;
}

static void emac_free_all_tx_descriptor(struct emac_adapter *adpt)
{
	u8 i;

	for (i = 0; i < adpt->num_txques; i++)
		emac_free_tx_descriptor(&adpt->tx_queue[i]);
}

/* Allocate TX descriptor ring for the given transmit queue */
static int emac_alloc_tx_descriptor(struct emac_adapter *adpt,
				    struct emac_tx_queue *txque)
{
	struct emac_ring_header *ring_header = &adpt->ring_header;
	unsigned long size;

	size = sizeof(struct emac_buffer) * txque->tpd.count;
	txque->tpd.tpbuff = kzalloc(size, GFP_KERNEL);
	if (!txque->tpd.tpbuff)
		goto err_alloc_tpq_buffer;

	txque->tpd.size = txque->tpd.count * (adpt->tpdesc_size * 4);
	txque->tpd.tpdma = ring_header->dma + ring_header->used;
	txque->tpd.tpdesc = ring_header->desc + ring_header->used;
	ring_header->used += ALIGN(txque->tpd.size, 8);
	txque->tpd.produce_idx = 0;
	txque->tpd.consume_idx = 0;
	return 0;

err_alloc_tpq_buffer:
	emac_err(adpt, "Unable to allocate memory for the Tx descriptor\n");
	return -ENOMEM;
}

static int emac_alloc_all_tx_descriptor(struct emac_adapter *adpt)
{
	int retval = 0;
	u8 i;

	for (i = 0; i < adpt->num_txques; i++) {
		retval = emac_alloc_tx_descriptor(adpt, &adpt->tx_queue[i]);
		if (retval)
			break;
	}

	if (retval) {
		emac_err(adpt, "Allocation for Tx Queue %u failed\n", i);
		for (i--; i > 0; i--)
			emac_free_tx_descriptor(&adpt->tx_queue[i]);
	}

	return retval;
}

/* Free all buffers associated with given transmit queue */
static void emac_free_rx_descriptor(struct emac_rx_queue *rxque)
{
	emac_clean_rx_queue(rxque);

	kfree(rxque->rfd.rfbuff);
	rxque->rfd.rfbuff = NULL;
	rxque->rrd.rrdesc = rxque->rfd.rfdesc = NULL;
	rxque->rrd.rrdma = rxque->rfd.rfdma = 0;
	rxque->rrd.size = rxque->rfd.size = 0;
}

static void emac_free_all_rx_descriptor(struct emac_adapter *adpt)
{
	u8 i;

	for (i = 0; i < adpt->num_rxques; i++)
		emac_free_rx_descriptor(&adpt->rx_queue[i]);
}

/* Allocate RX descriptor rings for the given receive queue */
static int emac_alloc_rx_descriptor(struct emac_adapter *adpt,
				    struct emac_rx_queue *rxque)
{
	struct emac_ring_header *ring_header = &adpt->ring_header;
	unsigned long size;

	size = sizeof(struct emac_buffer) * rxque->rfd.count;
	rxque->rfd.rfbuff = kzalloc(size, GFP_KERNEL);
	if (!rxque->rfd.rfbuff)
		goto err_alloc_rfq_buffer;

	rxque->rrd.size = rxque->rrd.count * (adpt->rrdesc_size * 4);
	rxque->rfd.size = rxque->rfd.count * (adpt->rfdesc_size * 4);

	rxque->rrd.rrdma = ring_header->dma + ring_header->used;
	rxque->rrd.rrdesc = ring_header->desc + ring_header->used;
	ring_header->used += ALIGN(rxque->rrd.size, 8);

	rxque->rfd.rfdma = ring_header->dma + ring_header->used;
	rxque->rfd.rfdesc = ring_header->desc + ring_header->used;
	ring_header->used += ALIGN(rxque->rfd.size, 8);

	rxque->rrd.produce_idx = 0;
	rxque->rrd.consume_idx = 0;

	rxque->rfd.produce_idx = 0;
	rxque->rfd.consume_idx = 0;

	return 0;

err_alloc_rfq_buffer:
	emac_err(adpt, "Unable to allocate memory for the Rx descriptor\n");
	return -ENOMEM;
}

static int emac_alloc_all_rx_descriptor(struct emac_adapter *adpt)
{
	int retval = 0;
	u8 i;

	for (i = 0; i < adpt->num_rxques; i++) {
		retval = emac_alloc_rx_descriptor(adpt, &adpt->rx_queue[i]);
		if (retval)
			break;
	}

	if (retval) {
		emac_err(adpt, "Allocation for Rx Queue %u failed\n", i);
		for (i--; i > 0; i--)
			emac_free_rx_descriptor(&adpt->rx_queue[i]);
	}

	return retval;
}

/* Allocate all TX and RX descriptor rings */
static int emac_alloc_all_rtx_descriptor(struct emac_adapter *adpt)
{
	struct emac_ring_header *ring_header = &adpt->ring_header;
	int num_tques = adpt->num_txques;
	int num_rques = adpt->num_rxques;
	unsigned int num_tx_descs = adpt->num_txdescs;
	unsigned int num_rx_descs = adpt->num_rxdescs;
	struct device *dev = adpt->rx_queue[0].dev;
	int retval;

	/* Ring DMA buffer. Each ring may need up to 8 bytes for alignment,
	 * hence the additional padding bytes are allocated.
	 */
	ring_header->size =
		num_tques * num_tx_descs * (adpt->tpdesc_size * 4) +
		num_rques * num_rx_descs * (adpt->rfdesc_size * 4) +
		num_rques * num_rx_descs * (adpt->rrdesc_size * 4) +
		num_tques * 8 + num_rques * 2 * 8;

	emac_info(adpt, ifup, "TX queues %d, TX descriptors %d\n",
		  num_tques, num_tx_descs);
	emac_info(adpt, ifup, "RX queues %d, Rx descriptors %d\n",
		  num_rques, num_rx_descs);

	ring_header->used = 0;
	ring_header->desc = dma_alloc_coherent(dev, ring_header->size,
					&ring_header->dma, GFP_KERNEL);
	if (!ring_header->desc) {
		emac_err(adpt, "dma_alloc_coherent failed\n");
		retval = -ENOMEM;
		goto err_alloc_dma;
	}
	memset(ring_header->desc, 0, ring_header->size);
	ring_header->used = ALIGN(ring_header->dma, 8) - ring_header->dma;

	retval = emac_alloc_all_tx_descriptor(adpt);
	if (retval)
		goto err_alloc_tx;

	retval = emac_alloc_all_rx_descriptor(adpt);
	if (retval)
		goto err_alloc_rx;

	return 0;

err_alloc_rx:
	emac_free_all_tx_descriptor(adpt);
err_alloc_tx:
	dma_free_coherent(dev, ring_header->size,
			  ring_header->desc, ring_header->dma);

	ring_header->desc = NULL;
	ring_header->dma = 0;
	ring_header->size = ring_header->used = 0;
err_alloc_dma:
	return retval;
}

/* Free all TX and RX descriptor rings */
static void emac_free_all_rtx_descriptor(struct emac_adapter *adpt)
{
	struct emac_ring_header *ring_header = &adpt->ring_header;
	struct device *dev = adpt->rx_queue[0].dev;

	emac_free_all_tx_descriptor(adpt);
	emac_free_all_rx_descriptor(adpt);

	dma_free_coherent(dev, ring_header->size,
			  ring_header->desc, ring_header->dma);

	ring_header->desc = NULL;
	ring_header->dma = 0;
	ring_header->size = ring_header->used = 0;
}

/* Initialize descriptor rings */
static void emac_init_ring_ptrs(struct emac_adapter *adpt)
{
	int i, j;

	for (i = 0; i < adpt->num_txques; i++) {
		struct emac_tx_queue *txque = &adpt->tx_queue[i];
		struct emac_buffer *tpbuf = txque->tpd.tpbuff;
		txque->tpd.produce_idx = 0;
		txque->tpd.consume_idx = 0;
		for (j = 0; j < txque->tpd.count; j++)
			tpbuf[j].dma = 0;
	}

	for (i = 0; i < adpt->num_rxques; i++) {
		struct emac_rx_queue *rxque = &adpt->rx_queue[i];
		struct emac_buffer *rfbuf = rxque->rfd.rfbuff;
		rxque->rrd.produce_idx = 0;
		rxque->rrd.consume_idx = 0;
		rxque->rfd.produce_idx = 0;
		rxque->rfd.consume_idx = 0;
		for (j = 0; j < rxque->rfd.count; j++)
			rfbuf[j].dma = 0;
	}
}

/* Configure Receive Side Scaling (RSS) */
static void emac_config_rss(struct emac_adapter *adpt)
{
	static const u8 key[40] = {
		0x6D, 0x5A, 0x56, 0xDA, 0x25, 0x5B, 0x0E, 0xC2,
		0x41, 0x67, 0x25, 0x3D, 0x43, 0xA3, 0x8F, 0xB0,
		0xD0, 0xCA, 0x2B, 0xCB, 0xAE, 0x7B, 0x30, 0xB4,
		0x77, 0xCB, 0x2D, 0xA3, 0x80, 0x30, 0xF2, 0x0C,
		0x6A, 0x42, 0xB7, 0x3B, 0xBE, 0xAC, 0x01, 0xFA};

	struct emac_hw *hw = &adpt->hw;
	u32 reta = 0;
	u16 i, j;

	if (adpt->num_rxques == 1)
		return;

	if (!hw->rss_initialized) {
		hw->rss_initialized = true;
		/* initialize rss hash type and idt table size */
		hw->rss_hstype = EMAC_RSS_HSTYP_ALL_EN;
		hw->rss_idt_size = EMAC_RSS_IDT_SIZE;

		/* Fill out RSS key */
		memcpy(hw->rss_key, key, sizeof(hw->rss_key));

		/* Fill out redirection table */
		memset(hw->rss_idt, 0x0, sizeof(hw->rss_idt));
		for (i = 0, j = 0; i < EMAC_RSS_IDT_SIZE; i++, j++) {
			if (j == adpt->num_rxques)
				j = 0;
			if (j > 1)
				reta |= (j << ((i & 7) * 4));
			if ((i & 7) == 7) {
				hw->rss_idt[i>>3] = reta;
				reta = 0;
			}
		}
	}

	emac_hw_config_rss(hw);
}

/* Change the Maximum Transfer Unit (MTU) */
static int emac_change_mtu(struct net_device *netdev, int new_mtu)
{
	struct emac_adapter *adpt = netdev_priv(netdev);
	int old_mtu   = netdev->mtu;
	int max_frame = new_mtu + ETH_HLEN + ETH_FCS_LEN + VLAN_HLEN;

	if ((max_frame < EMAC_MIN_ETH_FRAME_SIZE) ||
	    (max_frame > EMAC_MAX_ETH_FRAME_SIZE)) {
		emac_err(adpt, "invalid MTU setting\n");
		return -EINVAL;
	}

	if ((old_mtu != new_mtu) && netif_running(netdev)) {
		emac_info(adpt, hw, "changing MTU from %d to %d\n",
			  netdev->mtu, new_mtu);
		netdev->mtu = new_mtu;
		adpt->hw.mtu = new_mtu;
		adpt->rxbuf_size = new_mtu > EMAC_DEF_RX_BUF_SIZE ?
			ALIGN(max_frame, 8) : EMAC_DEF_RX_BUF_SIZE;
		emac_reinit_locked(adpt);
	}

	return 0;
}

/* Bringup the interface/HW */
static int emac_up(struct emac_adapter *adpt)
{
	struct emac_hw *hw = &adpt->hw;
	struct net_device *netdev = adpt->netdev;
	int retval = 0;
	int i;

	emac_init_ring_ptrs(adpt);
	emac_set_rx_mode(netdev);

	emac_hw_config_mac(hw);
	emac_config_rss(adpt);

	for (i = 0; i < adpt->num_rxques; i++)
		emac_refresh_rx_buffer(&adpt->rx_queue[i]);

	for (i = 0; i < EMAC_NUM_CORE_IRQ; i++) {
		struct emac_irq_info *irq_info = &adpt->irq_info[i];
		retval = request_irq(irq_info->irq, irq_info->handler,
				     0, irq_info->name, irq_info);
		if (retval) {
			emac_err(adpt, "Unable to allocate interrupt %d: %d\n",
				 irq_info->irq, retval);
			while (--i >= 0)
				free_irq(adpt->irq_info[i].irq,
					 &adpt->irq_info[i]);
			goto err_request_irq;
		}
	}

	emac_napi_enable_all(adpt);
	emac_enable_intr(adpt);

	netif_start_queue(netdev);
	CLI_ADPT_FLAG(STATE_DOWN);

	/* check link status */
	SET_ADPT_FLAG(TASK_LSC_REQ);
	adpt->link_jiffies = jiffies + EMAC_TRY_LINK_TIMEOUT;
	mod_timer(&adpt->emac_timer, jiffies);

	return retval;

err_request_irq:
	emac_clean_all_rx_queues(adpt);
	return retval;
}

/* Bring down the interface/HW */
static void emac_down(struct emac_adapter *adpt, u32 ctrl)
{
	struct net_device *netdev = adpt->netdev;
	struct emac_hw *hw = &adpt->hw;
	int i;

	SET_ADPT_FLAG(STATE_DOWN);
	netif_stop_queue(netdev);

	netif_carrier_off(netdev);

	emac_disable_intr(adpt);
	emac_napi_disable_all(adpt);
	for (i = 0; i < EMAC_NUM_CORE_IRQ; i++)
		free_irq(adpt->irq_info[i].irq, &adpt->irq_info[i]);

	CLI_ADPT_FLAG(TASK_LSC_REQ);
	CLI_ADPT_FLAG(TASK_REINIT_REQ);
	del_timer_sync(&adpt->emac_timer);

	if (ctrl & EMAC_HW_CTRL_RESET_MAC)
		emac_hw_reset_mac(hw);

	adpt->hw.link_speed = EMAC_LINK_SPEED_UNKNOWN;
	emac_clean_all_tx_queues(adpt);
	emac_clean_all_rx_queues(adpt);
}

/* Called when the network interface is made active */
static int emac_open(struct net_device *netdev)
{
	struct emac_adapter *adpt = netdev_priv(netdev);
	int retval;

	netif_carrier_off(netdev);

	/* allocate rx/tx dma buffer & descriptors */
	retval = emac_alloc_all_rtx_descriptor(adpt);
	if (retval) {
		emac_err(adpt, "error in emac_alloc_all_rtx_descriptor\n");
		goto err_alloc_rtx;
	}

	retval = emac_up(adpt);
	if (retval)
		goto err_up;

	return retval;

err_up:
	emac_free_all_rtx_descriptor(adpt);
err_alloc_rtx:
	return retval;
}

/* Called when the network interface is disabled */
static int emac_close(struct net_device *netdev)
{
	struct emac_adapter *adpt = netdev_priv(netdev);
	struct emac_hw *hw = &adpt->hw;

	/* ensure no task is running and no reset is in progress */
	while (CHK_AND_SET_ADPT_FLAG(STATE_RESETTING))
		msleep(20); /* Reset might take few 10s of ms */

	if (!CHK_ADPT_FLAG(STATE_DOWN))
		emac_down(adpt, EMAC_HW_CTRL_RESET_MAC);
	else
		emac_hw_reset_mac(hw);

	if (CHK_HW_FLAG(PTP_CAP))
		emac_ptp_stop(hw);

	emac_free_all_rtx_descriptor(adpt);

	CLI_ADPT_FLAG(STATE_RESETTING);
	return 0;
}

/* PHY related IOCTLs */
static int emac_mii_ioctl(struct net_device *netdev,
			  struct ifreq *ifr, int cmd)
{
	struct emac_adapter *adpt = netdev_priv(netdev);
	struct emac_hw *hw = &adpt->hw;
	struct mii_ioctl_data *data = if_mii(ifr);
	int retval = 0;

	if (!netif_running(netdev))
		return -EINVAL;

	switch (cmd) {
	case SIOCGMIIPHY:
		data->phy_id = 0;
		break;

	case SIOCGMIIREG:
		if (!capable(CAP_NET_ADMIN)) {
			retval = -EPERM;
			break;
		}

		if (data->reg_num & ~(0x1F)) {
			retval = -EFAULT;
			break;
		}

		retval = emac_read_phy_reg(hw, data->reg_num, &data->val_out);
		break;

	case SIOCSMIIREG:
		if (!capable(CAP_NET_ADMIN)) {
			retval = -EPERM;
			break;
		}

		if (data->reg_num & ~(0x1F)) {
			retval = -EFAULT;
			break;
		}

		retval = emac_write_phy_reg(hw, data->reg_num, data->val_in);
		break;
	}

	return retval;

}

/* IOCTL support for the interface */
static int emac_ioctl(struct net_device *netdev, struct ifreq *ifr, int cmd)
{
	switch (cmd) {
	case SIOCGMIIPHY:
	case SIOCGMIIREG:
	case SIOCSMIIREG:
		return emac_mii_ioctl(netdev, ifr, cmd);
	case SIOCSHWTSTAMP:
		return emac_tstamp_ioctl(netdev, ifr, cmd);
	default:
		return -EOPNOTSUPP;
	}
}

/* Read statistics information from the HW */
void emac_update_hw_stats(struct emac_adapter *adpt)
{
	u16 hw_reg_addr = 0;
	u64 *stats_item = NULL;
	u32 val;

	/* update rx status */
	hw_reg_addr = REG_MAC_RX_STATUS_BIN;
	stats_item = &adpt->hw_stats.rx_ok;

	while (hw_reg_addr <= REG_MAC_RX_STATUS_END) {
		val = emac_reg_r32(&adpt->hw, EMAC, hw_reg_addr);
		*stats_item += val;
		stats_item++;
		hw_reg_addr += sizeof(u32);
	}

	/* additional rx status */
	val = emac_reg_r32(&adpt->hw, EMAC, EMAC_RXMAC_STATC_REG23);
	adpt->hw_stats.rx_crc_allign += val;
	val = emac_reg_r32(&adpt->hw, EMAC, EMAC_RXMAC_STATC_REG24);
	adpt->hw_stats.rx_jubbers += val;

	/* update tx status */
	hw_reg_addr = REG_MAC_TX_STATUS_BIN;
	stats_item = &adpt->hw_stats.tx_ok;

	while (hw_reg_addr <= REG_MAC_TX_STATUS_END) {
		val = emac_reg_r32(&adpt->hw, EMAC, hw_reg_addr);
		*stats_item += val;
		stats_item++;
		hw_reg_addr += sizeof(u32);
	}

	/* additional tx status */
	val = emac_reg_r32(&adpt->hw, EMAC, EMAC_TXMAC_STATC_REG25);
	adpt->hw_stats.tx_col += val;
}

/* Provide network statistics info for the interface */
struct rtnl_link_stats64 *emac_get_stats64(struct net_device *netdev,
					   struct rtnl_link_stats64 *net_stats)
{
	struct emac_adapter *adpt = netdev_priv(netdev);
	struct emac_hw_stats *hw_stats = &adpt->hw_stats;

	emac_update_hw_stats(adpt);
	net_stats->rx_packets = hw_stats->rx_ok;
	net_stats->tx_packets = hw_stats->tx_ok;
	net_stats->rx_bytes = hw_stats->rx_byte_cnt;
	net_stats->tx_bytes = hw_stats->tx_byte_cnt;
	net_stats->multicast = hw_stats->rx_mcast;
	net_stats->collisions = hw_stats->tx_1_col +
	    hw_stats->tx_2_col * 2 +
	    hw_stats->tx_late_col + hw_stats->tx_abort_col;

	net_stats->rx_errors = hw_stats->rx_frag + hw_stats->rx_fcs_err +
	    hw_stats->rx_len_err + hw_stats->rx_sz_ov +
	    hw_stats->rx_align_err;
	net_stats->rx_fifo_errors = hw_stats->rx_rxf_ov;
	net_stats->rx_length_errors = hw_stats->rx_len_err;
	net_stats->rx_crc_errors = hw_stats->rx_fcs_err;
	net_stats->rx_frame_errors = hw_stats->rx_align_err;
	net_stats->rx_over_errors = hw_stats->rx_rxf_ov;
	net_stats->rx_missed_errors = hw_stats->rx_rxf_ov;

	net_stats->tx_errors = hw_stats->tx_late_col + hw_stats->tx_abort_col +
	    hw_stats->tx_underrun + hw_stats->tx_trunc;
	net_stats->tx_fifo_errors = hw_stats->tx_underrun;
	net_stats->tx_aborted_errors = hw_stats->tx_abort_col;
	net_stats->tx_window_errors = hw_stats->tx_late_col;

	return net_stats;
}

static const struct net_device_ops emac_netdev_ops = {
	.ndo_open		= &emac_open,
	.ndo_stop		= &emac_close,
	.ndo_validate_addr	= &eth_validate_addr,
	.ndo_start_xmit		= &emac_start_xmit,
	.ndo_set_mac_address	= &emac_set_mac_address,
	.ndo_change_mtu		= &emac_change_mtu,
	.ndo_do_ioctl		= &emac_ioctl,
	.ndo_tx_timeout		= &emac_tx_timeout,
	.ndo_get_stats64	= &emac_get_stats64,
	.ndo_set_features       = emac_set_features,
	.ndo_set_rx_mode        = emac_set_rx_mode,
};

/* Reinitialize the interface/HW if required */
static void emac_reinit_task_routine(struct emac_adapter *adpt)
{
	if (!CHK_ADPT_FLAG(TASK_REINIT_REQ))
		return;
	CLI_ADPT_FLAG(TASK_REINIT_REQ);

	if (CHK_ADPT_FLAG(STATE_DOWN) || CHK_ADPT_FLAG(STATE_RESETTING))
		return;

	emac_reinit_locked(adpt);
}
static inline char *emac_get_link_speed_desc(u32 speed)
{
	switch (speed) {
	case EMAC_LINK_SPEED_1GB_FULL:
		return  "1 Gbps Duplex Full";
	case EMAC_LINK_SPEED_100_FULL:
		return "100 Mbps Duplex Full";
	case EMAC_LINK_SPEED_100_HALF:
		return "100 Mbps Duplex Half";
	case EMAC_LINK_SPEED_10_FULL:
		return "10 Mbps Duplex Full";
	case EMAC_LINK_SPEED_10_HALF:
		return "10 Mbps Duplex HALF";
	default:
		return "unknown speed";
	}
}

/* Check link status and handle link state changes */
static void emac_link_task_routine(struct emac_adapter *adpt)
{
	struct net_device *netdev = adpt->netdev;
	struct emac_hw *hw = &adpt->hw;
	char *link_desc;

	if (!CHK_ADPT_FLAG(TASK_LSC_REQ))
		return;
	CLI_ADPT_FLAG(TASK_LSC_REQ);

	/* ensure that no reset is in progess while link task is running */
	while (CHK_AND_SET_ADPT_FLAG(STATE_RESETTING))
		msleep(20); /* Reset might take few 10s of ms */

	if (CHK_ADPT_FLAG(STATE_DOWN))
		goto link_task_done;

	emac_check_phy_link(hw, &hw->link_speed, &hw->link_up);
	link_desc = emac_get_link_speed_desc(hw->link_speed);

	if (!hw->link_up && time_after(adpt->link_jiffies, jiffies))
		SET_ADPT_FLAG(TASK_LSC_REQ);

	if (hw->link_up) {
		if (netif_carrier_ok(netdev))
			goto link_task_done;

		emac_info(adpt, timer, "NIC Link is Up %s\n", link_desc);

		emac_hw_start_mac(hw);
		netif_carrier_on(netdev);
		netif_wake_queue(netdev);
	} else {
		/* only continue if link was up previously */
		if (!netif_carrier_ok(netdev))
			goto link_task_done;

		hw->link_speed = 0;
		emac_info(adpt, timer, "NIC Link is Down\n");
		netif_stop_queue(netdev);
		netif_carrier_off(netdev);

		emac_hw_stop_mac(hw);
	}

link_task_done:
	CLI_ADPT_FLAG(STATE_RESETTING);
}

/* Watchdog task routine */
static void emac_task_routine(struct work_struct *work)
{
	struct emac_adapter *adpt = container_of(work, struct emac_adapter,
						 emac_task);

	if (!CHK_ADPT_FLAG(STATE_WATCH_DOG))
		emac_warn(adpt, timer, "flag STATE_WATCH_DOG doesn't set\n");

	emac_reinit_task_routine(adpt);

	emac_link_task_routine(adpt);

	CLI_ADPT_FLAG(STATE_WATCH_DOG);
}

/* Timer routine */
static void emac_timer_routine(unsigned long data)
{
	struct emac_adapter *adpt = (struct emac_adapter *)data;
	unsigned long delay;

	/* poll faster when waiting for link */
	if (CHK_ADPT_FLAG(TASK_LSC_REQ))
		delay = HZ / 10;
	else
		delay = 2 * HZ;

	/* Reset the timer */
	mod_timer(&adpt->emac_timer, delay + jiffies);

	emac_task_schedule(adpt);
}

/* Initialize all queue data structures */
static void emac_init_rtx_queues(struct platform_device *pdev,
				 struct emac_adapter *adpt)
{
	int que_idx;

	adpt->num_txques = EMAC_DEF_TX_QUEUES;
	adpt->num_rxques = EMAC_DEF_RX_QUEUES;

	for (que_idx = 0; que_idx < adpt->num_txques; que_idx++) {
		struct emac_tx_queue *txque = &adpt->tx_queue[que_idx];

		txque->tpd.count = adpt->num_txdescs;
		txque->que_idx = que_idx;
		txque->netdev = adpt->netdev;
		txque->dev = &(pdev->dev);
	}

	for (que_idx = 0; que_idx < adpt->num_rxques; que_idx++) {
		struct emac_rx_queue *rxque = &adpt->rx_queue[que_idx];

		rxque->rrd.count = adpt->num_rxdescs;
		rxque->rfd.count = adpt->num_rxdescs;
		rxque->que_idx = que_idx;
		rxque->netdev = adpt->netdev;
		rxque->dev = &(pdev->dev);
	}

	switch (adpt->num_rxques) {
	case 4:
		adpt->rx_queue[3].produce_reg = EMAC_MAILBOX_13;
		adpt->rx_queue[3].produce_mask = RFD3_PROD_IDX_BMSK;
		adpt->rx_queue[3].produce_shft = RFD3_PROD_IDX_SHFT;

		adpt->rx_queue[3].process_reg = EMAC_MAILBOX_13;
		adpt->rx_queue[3].process_mask = RFD3_PROC_IDX_BMSK;
		adpt->rx_queue[3].process_shft = RFD3_PROC_IDX_SHFT;

		adpt->rx_queue[3].consume_reg = EMAC_MAILBOX_8;
		adpt->rx_queue[3].consume_mask = RFD3_CONS_IDX_BMSK;
		adpt->rx_queue[3].consume_shft = RFD3_CONS_IDX_SHFT;

		adpt->rx_queue[3].intr = RX_PKT_INT3;
		adpt->rx_queue[3].irq_info = &adpt->irq_info[3];
	case 3:
		adpt->rx_queue[2].produce_reg = EMAC_MAILBOX_6;
		adpt->rx_queue[2].produce_mask = RFD2_PROD_IDX_BMSK;
		adpt->rx_queue[2].produce_shft = RFD2_PROD_IDX_SHFT;

		adpt->rx_queue[2].process_reg = EMAC_MAILBOX_6;
		adpt->rx_queue[2].process_mask = RFD2_PROC_IDX_BMSK;
		adpt->rx_queue[2].process_shft = RFD2_PROC_IDX_SHFT;

		adpt->rx_queue[2].consume_reg = EMAC_MAILBOX_7;
		adpt->rx_queue[2].consume_mask = RFD2_CONS_IDX_BMSK;
		adpt->rx_queue[2].consume_shft = RFD2_CONS_IDX_SHFT;

		adpt->rx_queue[2].intr = RX_PKT_INT2;
		adpt->rx_queue[2].irq_info = &adpt->irq_info[2];
	case 2:
		adpt->rx_queue[1].produce_reg = EMAC_MAILBOX_5;
		adpt->rx_queue[1].produce_mask = RFD1_PROD_IDX_BMSK;
		adpt->rx_queue[1].produce_shft = RFD1_PROD_IDX_SHFT;

		adpt->rx_queue[1].process_reg = EMAC_MAILBOX_5;
		adpt->rx_queue[1].process_mask = RFD1_PROC_IDX_BMSK;
		adpt->rx_queue[1].process_shft = RFD1_PROC_IDX_SHFT;

		adpt->rx_queue[1].consume_reg = EMAC_MAILBOX_7;
		adpt->rx_queue[1].consume_mask = RFD1_CONS_IDX_BMSK;
		adpt->rx_queue[1].consume_shft = RFD1_CONS_IDX_SHFT;

		adpt->rx_queue[1].intr = RX_PKT_INT1;
		adpt->rx_queue[1].irq_info = &adpt->irq_info[1];
	case 1:
		adpt->rx_queue[0].produce_reg = EMAC_MAILBOX_0;
		adpt->rx_queue[0].produce_mask = RFD0_PROD_IDX_BMSK;
		adpt->rx_queue[0].produce_shft = RFD0_PROD_IDX_SHFT;

		adpt->rx_queue[0].process_reg = EMAC_MAILBOX_0;
		adpt->rx_queue[0].process_mask = RFD0_PROC_IDX_BMSK;
		adpt->rx_queue[0].process_shft = RFD0_PROC_IDX_SHFT;

		adpt->rx_queue[0].consume_reg = EMAC_MAILBOX_3;
		adpt->rx_queue[0].consume_mask = RFD0_CONS_IDX_BMSK;
		adpt->rx_queue[0].consume_shft = RFD0_CONS_IDX_SHFT;

		adpt->rx_queue[0].intr = RX_PKT_INT0;
		adpt->rx_queue[0].irq_info = &adpt->irq_info[0];
		break;
	}

	switch (adpt->num_txques) {
	case 4:
		adpt->tx_queue[3].produce_reg = EMAC_MAILBOX_11;
		adpt->tx_queue[3].produce_mask = H3TPD_PROD_IDX_BMSK;
		adpt->tx_queue[3].produce_shft = H3TPD_PROD_IDX_SHFT;

		adpt->tx_queue[3].consume_reg = EMAC_MAILBOX_12;
		adpt->tx_queue[3].consume_mask = H3TPD_CONS_IDX_BMSK;
		adpt->tx_queue[3].consume_shft = H3TPD_CONS_IDX_SHFT;
	case 3:
		adpt->tx_queue[2].produce_reg = EMAC_MAILBOX_9;
		adpt->tx_queue[2].produce_mask = H2TPD_PROD_IDX_BMSK;
		adpt->tx_queue[2].produce_shft = H2TPD_PROD_IDX_SHFT;

		adpt->tx_queue[2].consume_reg = EMAC_MAILBOX_10;
		adpt->tx_queue[2].consume_mask = H2TPD_CONS_IDX_BMSK;
		adpt->tx_queue[2].consume_shft = H2TPD_CONS_IDX_SHFT;
	case 2:
		adpt->tx_queue[1].produce_reg = EMAC_MAILBOX_16;
		adpt->tx_queue[1].produce_mask = H1TPD_PROD_IDX_BMSK;
		adpt->tx_queue[1].produce_shft = H1TPD_PROD_IDX_SHFT;

		adpt->tx_queue[1].consume_reg = EMAC_MAILBOX_10;
		adpt->tx_queue[1].consume_mask = H1TPD_CONS_IDX_BMSK;
		adpt->tx_queue[1].consume_shft = H1TPD_CONS_IDX_SHFT;
	case 1:
		adpt->tx_queue[0].produce_reg = EMAC_MAILBOX_15;
		adpt->tx_queue[0].produce_mask = NTPD_PROD_IDX_BMSK;
		adpt->tx_queue[0].produce_shft = NTPD_PROD_IDX_SHFT;

		adpt->tx_queue[0].consume_reg = EMAC_MAILBOX_2;
		adpt->tx_queue[0].consume_mask = NTPD_CONS_IDX_BMSK;
		adpt->tx_queue[0].consume_shft = NTPD_CONS_IDX_SHFT;
		break;
	}
}

/* Initialize various data structures  */
static void emac_init_adapter(struct emac_adapter *adpt)
{
	struct emac_hw *hw   = &adpt->hw;
	int max_frame;

	/* ids */
	hw->devid = (u16)emac_reg_field_r32(hw, EMAC, EMAC_DMA_MAS_CTRL,
					    DEV_ID_NUM_BMSK, DEV_ID_NUM_SHFT);
	hw->revid = (u16)emac_reg_field_r32(hw, EMAC, EMAC_DMA_MAS_CTRL,
					    DEV_REV_NUM_BMSK, DEV_REV_NUM_SHFT);

	/* descriptors */
	adpt->num_txdescs = EMAC_DEF_TX_DESCS;
	adpt->num_rxdescs = EMAC_DEF_RX_DESCS;

	/* mtu */
	adpt->netdev->mtu = ETH_DATA_LEN;
	hw->mtu = adpt->netdev->mtu;
	max_frame = adpt->netdev->mtu + ETH_HLEN + ETH_FCS_LEN + VLAN_HLEN;
	adpt->rxbuf_size = adpt->netdev->mtu > EMAC_DEF_RX_BUF_SIZE ?
			   ALIGN(max_frame, 8) : EMAC_DEF_RX_BUF_SIZE;

	/* dma */
	hw->dma_order = emac_dma_ord_out;
	hw->dmar_block = emac_dma_req_4096;
	hw->dmaw_block = emac_dma_req_128;
	hw->dmar_dly_cnt = DMAR_DLY_CNT_DEF;
	hw->dmaw_dly_cnt = DMAW_DLY_CNT_DEF;
	hw->tpd_burst = TXQ0_NUM_TPD_PREF_DEF;
	hw->rfd_burst = RXQ0_NUM_RFD_PREF_DEF;

	/* link */
	hw->link_up = false;
	hw->link_speed = EMAC_LINK_SPEED_UNKNOWN;

	/* flow control */
	hw->req_fc_mode = emac_fc_full;
	hw->cur_fc_mode = emac_fc_full;
	hw->disable_fc_autoneg = false;

	/* rss */
	hw->rss_initialized = false;
	hw->rss_hstype = 0;
	hw->rss_idt_size = 0;
	hw->rss_base_cpu = 0;
	memset(hw->rss_idt, 0x0, sizeof(hw->rss_idt));
	memset(hw->rss_key, 0x0, sizeof(hw->rss_key));

	/* others */
	hw->preamble = EMAC_PREAMBLE_DEF;
	adpt->wol = EMAC_WOL_MAGIC | EMAC_WOL_PHY;

	/* phy */
	emac_hw_init_phy(hw);

	if (CHK_HW_FLAG(PTP_CAP))
		emac_ptp_init(hw);
}

#ifdef CONFIG_PM
static int emac_shutdown_internal(struct platform_device *pdev, bool *wakeup)
{
	struct net_device *netdev = dev_get_drvdata(&pdev->dev);
	struct emac_adapter *adpt = netdev_priv(netdev);
	struct emac_hw *hw = &adpt->hw;
	u32 wufc = adpt->wol;
	u16 lpa, i;
	u32 speed, adv_speed;
	bool link_up;
	int retval = 0;

	netif_device_detach(netdev);
	if (netif_running(netdev)) {
		/* ensure no task is running and no reset is in progress */
		while (CHK_AND_SET_ADPT_FLAG(STATE_RESETTING))
			msleep(20); /* Reset might take few 10s of ms */

		emac_down(adpt, 0);

		CLI_ADPT_FLAG(STATE_RESETTING);
	}

	emac_check_phy_link(hw, &speed, &link_up);

	if (link_up) {
		retval = emac_read_phy_reg(hw, MII_LPA, &lpa);
		if (retval)
			return retval;

		adv_speed = EMAC_LINK_SPEED_10_HALF;
		if (lpa & LPA_10FULL)
			adv_speed = EMAC_LINK_SPEED_10_FULL;
		else if (lpa & LPA_10HALF)
			adv_speed = EMAC_LINK_SPEED_10_HALF;
		else if (lpa & LPA_100FULL)
			adv_speed = EMAC_LINK_SPEED_100_FULL;
		else if (lpa & LPA_100HALF)
			adv_speed = EMAC_LINK_SPEED_100_HALF;

		retval = emac_setup_phy_link(hw, adv_speed, true,
					     !hw->disable_fc_autoneg);
		if (retval)
			return retval;

		for (i = 0; i < EMAC_MAX_SETUP_LNK_CYCLE; i++) {
			retval = emac_check_phy_link(hw, &speed, &link_up);
			if ((!retval) && link_up)
				break;

			/* link can take upto few seconds to come up */
			msleep(100);
		}
	} else {
		speed = EMAC_LINK_SPEED_10_HALF;
		link_up = false;
	}
	hw->link_speed = speed;
	hw->link_up = link_up;

	retval = emac_hw_config_wol(hw, wufc);
	if (retval)
		return retval;

	/* clear phy interrupt */
	retval = emac_hw_ack_phy_intr(hw);
	if (retval)
		return retval;

	retval = emac_hw_config_pow_save(hw, adpt->hw.link_speed,
			(wufc ? true : false), false,
			(wufc & EMAC_WOL_MAGIC ? true : false));
	if (retval)
		return retval;

	*wakeup = wufc ? true : false;
	return 0;
}

static int emac_suspend(struct platform_device *pdev, pm_message_t state)
{
	bool wakeup;

	return emac_shutdown_internal(pdev, &wakeup);
}

static int emac_resume(struct platform_device *pdev)
{
	struct net_device *netdev = dev_get_drvdata(&pdev->dev);
	struct emac_adapter *adpt = netdev_priv(netdev);
	struct emac_hw *hw = &adpt->hw;
	u32 retval;

	emac_hw_reset_phy(hw);
	emac_hw_reset_mac(hw);
	retval = emac_setup_phy_link(hw, hw->autoneg_advertised, true,
				     !hw->disable_fc_autoneg);
	if (retval)
		return retval;

	retval = emac_hw_config_wol(hw, 0);
	if (retval)
		return retval;

	if (netif_running(netdev)) {
		retval = emac_up(adpt);
		if (retval)
			return retval;
	}

	netif_device_attach(netdev);
	return 0;
}
#endif

/* Get the resources */
static int emac_get_resources(struct platform_device *pdev,
			      struct emac_adapter *adpt)
{
	int retval = 0;
	u8 i;
	struct resource *res;
	struct net_device *netdev = adpt->netdev;
	void *reg_base[NUM_EMAC_REG_BASES] = { 0 };
	char *res_name[NUM_EMAC_REG_BASES] = {"emac", "emac_csr", "emac_1588"};

	for (i = 0; i < NUM_EMAC_REG_BASES; i++) {
		if ((i == EMAC_1588) && !adpt->tstamp_en)
			continue;

		res = platform_get_resource_byname(pdev, IORESOURCE_MEM,
						   res_name[i]);
		if (!res) {
			emac_err(adpt, "can't get %s resource\n", res_name[i]);
			retval = -ENOMEM;
			break;
		}

		reg_base[i] = ioremap(res->start, resource_size(res));
		if (!reg_base[i]) {
			emac_err(adpt, "can't remap %s\n", res_name[i]);
			retval = -ENOMEM;
			break;
		}
	}

	if (retval) {
		while (i--)
			if (reg_base[i])
				iounmap(reg_base[i]);
	} else {
		for (i = 0; i < NUM_EMAC_REG_BASES; i++)
			adpt->hw.reg_addr[i] = reg_base[i];

		netdev->base_addr = (unsigned long)adpt->hw.reg_addr[EMAC];
	}

	return retval;
}

/* Release resources */
static void emac_release_resources(struct emac_adapter *adpt)
{
	u8 i;

	for (i = 0; i < NUM_EMAC_REG_BASES; i++) {
		if (adpt->hw.reg_addr[i])
			iounmap(adpt->hw.reg_addr[i]);
	}
}

/* Probe function */
static int emac_probe(struct platform_device *pdev)
{
	struct net_device *netdev;
	struct emac_adapter *adpt;
	struct emac_hw *hw;
	int retval;
	u8 i;
	uint32_t hw_ver;

	netdev = alloc_etherdev(sizeof(struct emac_adapter));
	if (netdev == NULL) {
		dev_err(&pdev->dev, "etherdev alloc failed\n");
		retval = -ENOMEM;
		goto err_alloc_netdev;
	}

	dev_set_drvdata(&pdev->dev, netdev);
	SET_NETDEV_DEV(netdev, &pdev->dev);

	adpt = netdev_priv(netdev);
	adpt->netdev = netdev;
	hw = &adpt->hw;
	hw->adpt = adpt;
	adpt->msg_enable = netif_msg_init(msm_emac_msglvl, EMAC_MSG_DEFAULT);

	adpt->dma_mask = DMA_BIT_MASK(32);
	pdev->dev.dma_mask = &adpt->dma_mask;
	pdev->dev.dma_parms = &adpt->dma_parms;
	pdev->dev.coherent_dma_mask = DMA_BIT_MASK(32);

	dma_set_max_seg_size(&pdev->dev, 65536);
	dma_set_seg_boundary(&pdev->dev, 0xffffffff);

	adpt->tstamp_en = true;
	retval = emac_get_resources(pdev, adpt);
	if (retval)
		goto err_res;

	adpt->irq_info = emac_irq;
	for (i = 0; i < EMAC_NUM_CORE_IRQ; i++) {
		adpt->irq_info[i].adpt = adpt;
		adpt->irq_info[i].rxque = &adpt->rx_queue[i];
	}

	hw_ver = emac_reg_r32(hw, EMAC, EMAC_CORE_HW_VERSION);

	netdev->watchdog_timeo = EMAC_WATCHDOG_TIME;
	netdev->irq = adpt->irq_info[0].irq;

	if (adpt->tstamp_en)
		adpt->rrdesc_size = EMAC_TS_RRDESC_SIZE;
	else
		adpt->rrdesc_size = EMAC_RRDESC_SIZE;

	adpt->tpdesc_size = EMAC_TPDESC_SIZE;
	adpt->rfdesc_size = EMAC_RFDESC_SIZE;

	if (adpt->tstamp_en) {
		hw->rtc_ref_clkrate = DEFAULT_RTC_REF_CLKRATE;
		SET_HW_FLAG(PTP_CAP);
	}

	/* init netdev */
	netdev->netdev_ops = &emac_netdev_ops;

	emac_set_ethtool_ops(netdev);

	/* init adapter */
	emac_init_adapter(adpt);

	/* reset phy */
	emac_hw_reset_phy(hw);

	/* reset mac */
	emac_hw_reset_mac(hw);

	/* setup link to put it in a known good starting state */
	retval = emac_setup_phy_link(hw, hw->autoneg_advertised, true,
				     !hw->disable_fc_autoneg);
	if (retval)
		goto err_phy_link;

	/* set mac address */
	emac_hw_get_mac_addr(hw, hw->mac_perm_addr);
	memcpy(adpt->hw.mac_addr, hw->mac_perm_addr, netdev->addr_len);
	memcpy(netdev->dev_addr, adpt->hw.mac_addr, netdev->addr_len);
	emac_hw_set_mac_addr(hw, hw->mac_addr);

	/* set hw features */
	netdev->features = NETIF_F_SG | NETIF_F_HW_CSUM | NETIF_F_RXCSUM |
			NETIF_F_TSO | NETIF_F_TSO6 | NETIF_F_HW_VLAN_RX |
			NETIF_F_HW_VLAN_TX;
	netdev->hw_features = netdev->features;

	netdev->vlan_features |= NETIF_F_SG | NETIF_F_HW_CSUM |
				 NETIF_F_TSO | NETIF_F_TSO6;

	netdev->flags |= IFF_PROMISC;

	setup_timer(&adpt->emac_timer, &emac_timer_routine,
		    (unsigned long)adpt);
	INIT_WORK(&adpt->emac_task, emac_task_routine);

	/* Initialize queues */
	emac_init_rtx_queues(pdev, adpt);

	for (i = 0; i < adpt->num_rxques; i++)
		netif_napi_add(netdev, &adpt->rx_queue[i].napi,
			       emac_napi_rtx, 64);

	SET_HW_FLAG(VLANSTRIP_EN);
	SET_ADPT_FLAG(STATE_DOWN);
	strlcpy(netdev->name, "eth%d", sizeof(netdev->name));

	retval = register_netdev(netdev);
	if (retval) {
		emac_err(adpt, "register netdevice failed\n");
		goto err_register_netdev;
	}

	pr_info("%s - version %s\n", emac_drv_description, emac_drv_version);
	emac_dbg(adpt, probe, "EMAC HW ID %d.%d\n", hw->devid, hw->revid);
	emac_dbg(adpt, probe, "EMAC HW version %d.%d.%d\n",
		 (hw_ver & MAJOR_BMSK) >> MAJOR_SHFT,
		 (hw_ver & MINOR_BMSK) >> MINOR_SHFT,
		 (hw_ver & STEP_BMSK) >> STEP_SHFT);
	return 0;

err_register_netdev:
err_phy_link:
	emac_release_resources(adpt);
err_res:
	free_netdev(netdev);
err_alloc_netdev:
	return retval;
}

static int emac_remove(struct platform_device *pdev)
{
	struct net_device *netdev = dev_get_drvdata(&pdev->dev);
	struct emac_adapter *adpt = netdev_priv(netdev);

	pr_info("exiting %s\n", emac_drv_name);

	unregister_netdev(netdev);
	emac_release_resources(adpt);
	free_netdev(netdev);
	dev_set_drvdata(&pdev->dev, NULL);

	return 0;
}

static struct platform_driver emac_platform_driver = {
	.probe   = emac_probe,
	.remove  = __devexit_p(emac_remove),
#ifdef CONFIG_PM
	.suspend = emac_suspend,
	.resume  = emac_resume,
#endif
	.driver = {
		.owner = THIS_MODULE,
		.name	= "msm_emac"
	},
};

static int __init emac_init_module(void)
{
	return platform_driver_register(&emac_platform_driver);
}

static void __exit emac_exit_module(void)
{
	platform_driver_unregister(&emac_platform_driver);
}

module_init(emac_init_module);
module_exit(emac_exit_module);

MODULE_LICENSE("GPL");
