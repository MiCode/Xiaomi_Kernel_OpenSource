/* Copyright (C) 2017 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include "mt2712_yheader.h"
#include "mt2712_yapphdr.h"
#include "mt2712_drv.h"
#include "mt2712_yregacc.h"

static int GSTATUS;

static int q_op_mode[MAX_TX_QUEUE_CNT] = {
	Q_GENERIC,
	Q_GENERIC,
	Q_GENERIC,
	Q_GENERIC,
	Q_GENERIC,
	Q_GENERIC,
	Q_GENERIC,
	Q_GENERIC
};

void disable_all_ch_rx_interrpt(struct prv_data *pdata)
{
	struct hw_if_struct *hw_if = &pdata->hw_if;
	unsigned int q_inx;

	for (q_inx = 0; q_inx < RX_QUEUE_CNT; q_inx++)
		hw_if->disable_rx_interrupt(q_inx);
}

static unsigned int get_tx_hwtstamp(
	struct prv_data *pdata,
	struct s_TX_NORMAL_DESC *txdesc,
	struct sk_buff *skb)
{
	struct hw_if_struct *hw_if = &pdata->hw_if;
	struct skb_shared_hwtstamps shhwtstamp;
	u64 ns;

	if (hw_if->drop_tx_status_enabled() == 0) {
		/* check tx tstamp status */
		if (!hw_if->get_tx_tstamp_status(txdesc)) {
			pr_err("tx timestamp is not captured for this packet\n");
			return 0;
		}

		/* get the valid tstamp */
		ns = hw_if->get_tx_tstamp(txdesc);
	} else {
		/* drop tx status mode is enabled, hence read time
		 * stamp from register instead of descriptor
		 */

		/* check tx tstamp status */
		if (!hw_if->get_tx_tstamp_status_via_reg()) {
			pr_err("tx timestamp is not captured for this packet\n");
			return 0;
		}

		/* get the valid tstamp */
		ns = hw_if->get_tx_tstamp_via_reg();
	}

	pdata->xstats.tx_timestamp_captured_n++;
	memset(&shhwtstamp, 0, sizeof(struct skb_shared_hwtstamps));
	shhwtstamp.hwtstamp = ns_to_ktime(ns);
	/* pass tstamp to stack */
	skb_tstamp_tx(skb, &shhwtstamp);

	return 1;
}

static unsigned char get_rx_hwtstamp(
	struct prv_data *pdata,
	struct sk_buff *skb,
	struct rx_wrapper_descriptor *desc_data,
	unsigned int q_inx)
{
	struct s_RX_CONTEXT_DESC *rx_context_desc = NULL;
	struct hw_if_struct *hw_if = &pdata->hw_if;
	struct skb_shared_hwtstamps *shhwtstamp = NULL;
	u64 ns;
	int retry, ret;

	desc_data->dirty_rx++;
	INCR_RX_DESC_INDEX(desc_data->cur_rx, 1);
	rx_context_desc = GET_RX_DESC_PTR(q_inx, desc_data->cur_rx);

	/* check rx tsatmp */
	for (retry = 0; retry < 10; retry++) {
		ret = hw_if->get_rx_tstamp_status(rx_context_desc);
		if (ret == 1) {
			/* time stamp is valid */
			break;
		} else if (ret == 0) {
			pr_err("Device has not yet updated the context desc to hold Rx time stamp(retry = %d)\n",
			       retry);
		} else {
			pr_err("Error: Rx time stamp is corrupted(retry = %d)\n", retry);
			return 2;
		}
	}

	if (retry == 10) {
		pr_err("Device has not yet updated the context desc to hold Rx time stamp(retry = %d)\n", retry);
		desc_data->dirty_rx--;
		DECR_RX_DESC_INDEX(desc_data->cur_rx);
		return 0;
	}

	pdata->xstats.rx_timestamp_captured_n++;
	/* get valid tstamp */
	ns = hw_if->get_rx_tstamp(rx_context_desc);

	shhwtstamp = skb_hwtstamps(skb);
	memset(shhwtstamp, 0, sizeof(struct skb_shared_hwtstamps));
	shhwtstamp->hwtstamp = ns_to_ktime(ns);

	return 1;
}

static void tx_interrupt(struct net_device *dev, struct prv_data *pdata, unsigned int q_inx)
{
	struct tx_wrapper_descriptor *desc_data =
	    GET_TX_WRAPPER_DESC(q_inx);
	struct s_TX_NORMAL_DESC *txptr = NULL;
	struct tx_buffer *buffer = NULL;
	struct hw_if_struct *hw_if = &pdata->hw_if;
	struct desc_if_struct *desc_if = &pdata->desc_if;
	int err_incremented;
	unsigned int tstamp_taken = 0;
	unsigned long flags;

	spin_lock_irqsave(&pdata->tx_lock, flags);

	pdata->xstats.tx_clean_n[q_inx]++;

	while (desc_data->tx_pkt_queued > 0) {
		txptr = GET_TX_DESC_PTR(q_inx, desc_data->dirty_tx);
		buffer = GET_TX_BUF_PTR(q_inx, desc_data->dirty_tx);
		tstamp_taken = 0;

		if (!hw_if->tx_complete(txptr))
			break;

		/* update the tx error if any by looking at last segment
		 * for NORMAL descriptors
		 */
		if ((hw_if->get_tx_desc_ls(txptr)) && !(hw_if->get_tx_desc_ctxt(txptr))) {
			if (buffer->skb) {
				/* check whether skb support hw tstamp */
				if ((skb_shinfo(buffer->skb)->tx_flags & SKBTX_IN_PROGRESS))
					tstamp_taken = get_tx_hwtstamp(pdata, txptr, buffer->skb);
			}

			err_incremented = 0;

			if (hw_if->tx_aborted_error) {
				if (hw_if->tx_aborted_error(txptr)) {
					err_incremented = 1;
					dev->stats.tx_aborted_errors++;
				}
			}
			if (hw_if->tx_carrier_lost_error) {
				if (hw_if->tx_carrier_lost_error(txptr)) {
					err_incremented = 1;
					dev->stats.tx_carrier_errors++;
				}
			}
			if (hw_if->tx_fifo_underrun) {
				if (hw_if->tx_fifo_underrun(txptr)) {
					err_incremented = 1;
					dev->stats.tx_fifo_errors++;
				}
			}

			if (err_incremented == 1)
				dev->stats.tx_errors++;

			pdata->xstats.q_tx_pkt_n[q_inx]++;
			pdata->xstats.tx_pkt_n++;
			dev->stats.tx_packets++;
		}

		dev->stats.tx_bytes += buffer->len;
		dev->stats.tx_bytes += buffer->len2;
		desc_if->unmap_tx_skb(pdata, buffer);

		/* reset the descriptor so that driver/host can reuse it */
		hw_if->tx_desc_reset(desc_data->dirty_tx, pdata, q_inx);

		INCR_TX_DESC_INDEX(desc_data->dirty_tx, 1);
		desc_data->free_desc_cnt++;
		desc_data->tx_pkt_queued--;
	}

	if ((desc_data->queue_stopped == 1) && (desc_data->free_desc_cnt > 0)) {
		desc_data->queue_stopped = 0;
		netif_wake_subqueue(dev, q_inx);
	}

	spin_unlock_irqrestore(&pdata->tx_lock, flags);
}

static int alloc_rx_buf(struct prv_data *pdata, struct rx_buffer *buffer, gfp_t gfp)
{
	struct sk_buff *skb = buffer->skb;

	if (skb) {
		skb_trim(skb, 0);
		goto map_skb;
	}

	skb = __netdev_alloc_skb_ip_align(pdata->dev, pdata->rx_buffer_len, gfp);
	if (!skb) {
		pr_err("Failed to allocate skb\n");
		return -ENOMEM;
	}
	buffer->skb = skb;
	buffer->len = pdata->rx_buffer_len;
 map_skb:
	buffer->dma = dma_map_single(&pdata->pdev->dev, skb->data,
				     pdata->rx_buffer_len, DMA_FROM_DEVICE);
	if (dma_mapping_error(&pdata->pdev->dev, buffer->dma))
		pr_err("failed to do the RX dma map\n");

	return 0;
}

/* pass skb to upper layer */
static void receive_skb(struct prv_data *pdata, struct net_device *dev, struct sk_buff *skb, unsigned int q_inx)
{
	skb_record_rx_queue(skb, q_inx);
	skb->dev = dev;
	skb->protocol = eth_type_trans(skb, dev);
	netif_receive_skb(skb);
}

void update_rx_errors(struct net_device *dev, unsigned int rx_status)
{
	/* received pkt with crc error */
	if ((rx_status & 0x1000000))
		dev->stats.rx_crc_errors++;

	/* received frame alignment */
	if ((rx_status & 0x100000))
		dev->stats.rx_frame_errors++;

	/* receiver fifo overrun */
	if ((rx_status & 0x200000))
		dev->stats.rx_fifo_errors++;
}

static void delay_packet_handle(
		struct prv_data *pdata,
		struct sk_buff *skb,
		struct rx_buffer *buffer)
{
	buffer->skb = skb;
	buffer->dma = dma_map_single(&pdata->pdev->dev, skb->data,
				     pdata->rx_buffer_len, DMA_FROM_DEVICE);
	if (dma_mapping_error(&pdata->pdev->dev, buffer->dma))
		pr_err("failed to do the RX dma map\n");
}

static int clean_rx_irq(struct prv_data *pdata, int quota, unsigned int q_inx)
{
	struct rx_wrapper_descriptor *desc_data =
	    GET_RX_WRAPPER_DESC(q_inx);
	struct net_device *dev = pdata->dev;
	struct desc_if_struct *desc_if = &pdata->desc_if;
	struct hw_if_struct *hw_if = &pdata->hw_if;
	struct sk_buff *skb = NULL;
	int received = 0;
	struct rx_buffer *buffer = NULL;
	struct s_RX_NORMAL_DESC *RX_NORMAL_DESC = NULL;
	unsigned int pkt_len;
	int ret;

	while (received < quota) {
		buffer = GET_RX_BUF_PTR(q_inx, desc_data->cur_rx);
		RX_NORMAL_DESC = GET_RX_DESC_PTR(q_inx, desc_data->cur_rx);

		/* check for data availability */
		if (!(RX_NORMAL_DESC->RDES3 & RDESC3_OWN)) {
			/* assign it to new skb */
			skb = buffer->skb;
			buffer->skb = NULL;
			dma_unmap_single(&pdata->pdev->dev, buffer->dma,
					 pdata->rx_buffer_len, DMA_FROM_DEVICE);
			buffer->dma = 0;

			/* get the packet length */
			pkt_len =
			    (RX_NORMAL_DESC->RDES3 & RDESC3_PL);

			/* check for bad/oversized packet,
			 * error is valid only for last descriptor (OWN + LD bit set).
			 */
			if (!(RX_NORMAL_DESC->RDES3 & RDESC3_ES) &&
			    (RX_NORMAL_DESC->RDES3 & RDESC3_LD)) {
				/* pkt_len = pkt_len - 4; */ /* CRC stripping */

				/* code added for copybreak, this should improve
				 * performance for small pkts with large amount
				 * of reassembly being done in the stack
				 */
				if (pkt_len < COPYBREAK_DEFAULT) {
					struct sk_buff *new_skb =
					    netdev_alloc_skb_ip_align(dev,
								      pkt_len);
					if (new_skb) {
						skb_copy_to_linear_data_offset(
							new_skb,
							-NET_IP_ALIGN,
							(skb->data - NET_IP_ALIGN),
							(pkt_len + NET_IP_ALIGN));
						/* recycle actual desc skb */
						buffer->skb = skb;
						skb = new_skb;
					} else {
						/* just continue with the old skb */
					}
				}
				skb_put(skb, pkt_len);

				/* get rx tstamp if available */
				if ((pdata->hw_feat.tsstssel) && (pdata->hwts_rx_en) &&
				    hw_if->rx_tstamp_available(RX_NORMAL_DESC)) {
					ret = get_rx_hwtstamp(pdata, skb, desc_data, q_inx);
					if (ret == 0) {
						/* device has not yet updated the CONTEXT desc to hold the
						 * time stamp, hence delay the packet reception
						 */
						delay_packet_handle(pdata, skb, buffer);
						goto rx_tstmp_failed;
					}
				}

				dev->last_rx = jiffies;
				/* update the statistics */
				dev->stats.rx_packets++;
				dev->stats.rx_bytes += skb->len;
				receive_skb(pdata, dev, skb, q_inx);
				received++;
			} else {
				if (!(RX_NORMAL_DESC->RDES3 & RDESC3_LD))
					pr_err("Received oversized pkt, spanned across multiple desc\n");

				/* recycle skb */
				buffer->skb = skb;
				dev->stats.rx_errors++;
				update_rx_errors(dev, RX_NORMAL_DESC->RDES3);
			}

			desc_data->dirty_rx++;
			if (desc_data->dirty_rx >= desc_data->skb_realloc_threshold)
				desc_if->realloc_skb(pdata, q_inx);

			INCR_RX_DESC_INDEX(desc_data->cur_rx, 1);
		} else {
			/* no more data to read */
			break;
		}
	}

rx_tstmp_failed:

	if (desc_data->dirty_rx)
		desc_if->realloc_skb(pdata, q_inx);

	return received;
}

static void configure_rx_fun_ptr(struct prv_data *pdata)
{
	pdata->rx_buffer_len = MTK_ETH_FRAME_LEN;
	pdata->clean_rx = clean_rx_irq;
	pdata->alloc_rx_buf = alloc_rx_buf;
}

static void default_common_confs(struct prv_data *pdata)
{
	pdata->flow_ctrl = MTK_FLOW_CTRL_TX_RX;
	pdata->oldflow_ctrl = MTK_FLOW_CTRL_TX_RX;
	pdata->hwts_tx_en = 0;
	pdata->hwts_rx_en = 0;
	pdata->l2_filtering_mode = !!pdata->hw_feat.hash_tbl_sz;
	pdata->one_nsec_accuracy = 1;
}

static void restart_phy(struct prv_data *pdata)
{
	pdata->oldlink = 0;
	pdata->speed = 0;
	pdata->oldduplex = -1;

	if (pdata->phydev)
		phy_start_aneg(pdata->phydev);
}

static void default_tx_confs_single_q(
		struct prv_data *pdata,
		unsigned int q_inx)
{
	struct tx_queue *queue_data = GET_TX_QUEUE_PTR(q_inx);

	queue_data->q_op_mode = q_op_mode[q_inx];
}

static void default_rx_confs_single_q(
		struct prv_data *pdata,
		unsigned int q_inx)
{
}

static void restart_dev(struct prv_data *pdata, unsigned int q_inx)
{
	struct desc_if_struct *desc_if = &pdata->desc_if;
	struct hw_if_struct *hw_if = &pdata->hw_if;
	struct rx_queue *rx_queue = GET_RX_QUEUE_PTR(q_inx);

	netif_stop_subqueue(pdata->dev, q_inx);
	napi_disable(&rx_queue->napi);

	/* stop DMA TX/RX */
	hw_if->stop_dma_tx(q_inx);
	hw_if->stop_dma_rx(q_inx);

	/* free tx skb's */
	desc_if->tx_skb_free_mem_single_q(pdata, q_inx);
	/* free rx skb's */
	desc_if->rx_skb_free_mem_single_q(pdata, q_inx);

	if ((TX_QUEUE_CNT == 0) && (RX_QUEUE_CNT == 0)) {
		/* issue software reset to device */
		hw_if->exit();

		configure_rx_fun_ptr(pdata);
		default_common_confs(pdata);
	}
	/* reset all variables */
	default_tx_confs_single_q(pdata, q_inx);
	default_rx_confs_single_q(pdata, q_inx);

	/* reinit descriptor */
	desc_if->wrapper_tx_desc_init_single_q(pdata, q_inx);
	desc_if->wrapper_rx_desc_init_single_q(pdata, q_inx);

	napi_enable(&rx_queue->napi);

	/* initializes MAC and DMA
	 * NOTE : Do we need to init only one channel
	 * which generate FBE
	 */
	hw_if->init(pdata);

	restart_phy(pdata);

	netif_wake_subqueue(pdata->dev, q_inx);
}

irqreturn_t ISR_SW_ETH(int irq, void *device_id)
{
	unsigned long DMA_ISR;
	unsigned long DMA_SR;
	unsigned long MAC_ISR;
	unsigned long MAC_IMR;
	unsigned long DMA_IER;
	struct prv_data *pdata =
	    (struct prv_data *)device_id;
	struct net_device *dev = pdata->dev;
	struct hw_if_struct *hw_if = &pdata->hw_if;
	unsigned int q_inx;
	int napi_sched = 0;
	struct rx_queue *rx_queue = NULL;
	unsigned long MAC_PCS = 0;
	unsigned long DMA_SR_MERGE = 0;

	DMA_ISR_REG_RD(DMA_ISR);
	if (DMA_ISR == 0x0)
		return IRQ_NONE;

	MAC_ISR_REG_RD(MAC_ISR);

	/* Handle MAC interrupts */
	if (GET_VALUE(DMA_ISR, DMA_ISR_MACIS_LPOS, DMA_ISR_MACIS_HPOS) & 1) {
		/* handle only those MAC interrupts which are enabled */
		MAC_IMR_REG_RD(MAC_IMR);
		MAC_ISR = (MAC_ISR & MAC_IMR);

		/* RGMII/SMII interrupt */
		if (GET_VALUE(MAC_ISR, MAC_ISR_RGSMIIS_LPOS, MAC_ISR_RGSMIIS_HPOS) & 1) {
			MAC_PCS_REG_RD(MAC_PCS);
			pr_err("RGMII/SMII interrupt: MAC_PCS = %#lx\n", MAC_PCS);
			if ((MAC_PCS & 0x80000) == 0x80000) {
				pdata->pcs_link = 1;
				netif_carrier_on(dev);
				if ((MAC_PCS & 0x10000) == 0x10000) {
					pdata->pcs_duplex = 1;
					hw_if->set_full_duplex();
				} else {
					pdata->pcs_duplex = 0;
					hw_if->set_half_duplex();
				}

				if ((MAC_PCS & 0x60000) == 0x0) {
					pdata->pcs_speed = SPEED_10;
					hw_if->set_mii_speed_10();
				} else if ((MAC_PCS & 0x60000) == 0x20000) {
					pdata->pcs_speed = SPEED_100;
					hw_if->set_mii_speed_100();
				} else if ((MAC_PCS & 0x60000) == 0x40000) {
					pdata->pcs_speed = SPEED_1000;
					hw_if->set_gmii_speed();
				}
				pr_err("Link is UP:%dMbps & %s duplex\n",
				       pdata->pcs_speed, pdata->pcs_duplex ? "Full" : "Half");
			} else {
				pr_err("Link is Down\n");
				pdata->pcs_link = 0;
				netif_carrier_off(dev);
			}
		}
	}

dma_again:
	/* Handle DMA interrupts */
	for (q_inx = 0; q_inx < TX_QUEUE_CNT; q_inx++) {
		rx_queue = GET_RX_QUEUE_PTR(q_inx);

		DMA_SR_REG_RD(q_inx, DMA_SR);

		/* clear interrupts */
		DMA_SR_REG_WR(q_inx, DMA_SR);

		DMA_IER_REG_RD(q_inx, DMA_IER);
		/* handle only those DMA interrupts which are enabled */
		DMA_SR = (DMA_SR & DMA_IER);

		if (DMA_SR == 0)
			continue;

		if ((GET_VALUE(DMA_SR, DMA_SR_RI_LPOS, DMA_SR_RI_HPOS) & 1) ||
		    (GET_VALUE(DMA_SR, DMA_SR_RBU_LPOS, DMA_SR_RBU_HPOS) & 1)) {
			if (!napi_sched) {
				napi_sched = 1;
				if (likely(napi_schedule_prep(&rx_queue->napi))) {
					disable_all_ch_rx_interrpt(pdata);
					__napi_schedule(&rx_queue->napi);
				} else {
					pr_err("driver bug! Rx interrupt while in poll\n");
					disable_all_ch_rx_interrpt(pdata);
				}

				if ((GET_VALUE(DMA_SR, DMA_SR_RI_LPOS, DMA_SR_RI_HPOS) & 1))
					pdata->xstats.rx_normal_irq_n[q_inx]++;
				else
					pdata->xstats.rx_buf_unavailable_irq_n[q_inx]++;
			}
		}
		if (GET_VALUE(DMA_SR, DMA_SR_TI_LPOS, DMA_SR_TI_HPOS) & 1) {
			pdata->xstats.tx_normal_irq_n[q_inx]++;
			tx_interrupt(dev, pdata, q_inx);
		}
		if (GET_VALUE(DMA_SR, DMA_SR_TPS_LPOS, DMA_SR_TPS_HPOS) & 1) {
			pdata->xstats.tx_process_stopped_irq_n[q_inx]++;
			GSTATUS = -E_DMA_SR_TPS;
		}
		if (GET_VALUE(DMA_SR, DMA_SR_TBU_LPOS, DMA_SR_TBU_HPOS) & 1) {
			pdata->xstats.tx_buf_unavailable_irq_n[q_inx]++;
			GSTATUS = -E_DMA_SR_TBU;
		}
		if (GET_VALUE(DMA_SR, DMA_SR_RPS_LPOS, DMA_SR_RPS_HPOS) & 1) {
			pdata->xstats.rx_process_stopped_irq_n[q_inx]++;
			GSTATUS = -E_DMA_SR_RPS;
		}
		if (GET_VALUE(DMA_SR, DMA_SR_RWT_LPOS, DMA_SR_RWT_HPOS) & 1) {
			pdata->xstats.rx_watchdog_irq_n++;
			GSTATUS = -S_DMA_SR_RWT;
		}
		if (GET_VALUE(DMA_SR, DMA_SR_FBE_LPOS, DMA_SR_FBE_HPOS) & 1) {
			pdata->xstats.fatal_bus_error_irq_n++;
			GSTATUS = -E_DMA_SR_FBE;
			restart_dev(pdata, q_inx);
		}
	}

	/* DMA may transfer frame to mac during DMA INTR handle */
	DMA_SR_MERGE = 0;
	for (q_inx = 0; q_inx < TX_QUEUE_CNT; q_inx++) {
		DMA_SR_REG_RD(q_inx, DMA_SR);
		DMA_SR_MERGE |= DMA_SR;
	}

	if (DMA_SR_MERGE)
		goto dma_again;

	return IRQ_HANDLED;
}

static void default_tx_confs(struct prv_data *pdata)
{
	unsigned int q_inx;

	for (q_inx = 0; q_inx < TX_QUEUE_CNT; q_inx++)
		default_tx_confs_single_q(pdata, q_inx);
}

static void default_rx_confs(struct prv_data *pdata)
{
	unsigned int q_inx;

	for (q_inx = 0; q_inx < RX_QUEUE_CNT; q_inx++)
		default_rx_confs_single_q(pdata, q_inx);
}

static int prepare_mc_list(struct net_device *dev)
{
	struct prv_data *pdata = netdev_priv(dev);
	struct hw_if_struct *hw_if = &pdata->hw_if;
	u32 mc_filter[HTR_CNT];
	struct netdev_hw_addr *ha = NULL;
	int crc32_val = 0;
	int ret = 0, i = 1;

	if (pdata->l2_filtering_mode) {
		ret = 1;
		memset(mc_filter, 0, sizeof(mc_filter));

		if (pdata->max_hash_table_size == 64) {
			netdev_for_each_mc_addr(ha, dev) {
				/* The upper 6 bits of the calculated CRC are used to
				 * index the content of the Hash Table Reg 0 and 1.
				 */
				crc32_val =
					(bitrev32(~crc32_le(~0, ha->addr, 6)) >> 26);
				/* The most significant bit determines the register
				 * to use (Hash Table Reg X, X = 0 and 1) while the
				 * other 5(0x1F) bits determines the bit within the
				 * selected register
				 */
				mc_filter[crc32_val >> 5] |= (1 << (crc32_val & 0x1F));
			}
		} else if (pdata->max_hash_table_size == 128) {
			netdev_for_each_mc_addr(ha, dev) {
				/* The upper 7 bits of the calculated CRC are used to
				 * index the content of the Hash Table Reg 0,1,2 and 3.
				 */
				crc32_val =
					(bitrev32(~crc32_le(~0, ha->addr, 6)) >> 25);

				pr_err("crc_le = %#x, crc_be = %#x\n",
				       bitrev32(~crc32_le(~0, ha->addr, 6)),
				       bitrev32(~crc32_be(~0, ha->addr, 6)));

				/* The most significant 2 bits determines the register
				 * to use (Hash Table Reg X, X = 0,1,2 and 3) while the
				 * other 5(0x1F) bits determines the bit within the
				 * selected register
				 */
				mc_filter[crc32_val >> 5] |= (1 << (crc32_val & 0x1F));
			}
		} else if (pdata->max_hash_table_size == 256) {
			netdev_for_each_mc_addr(ha, dev) {
				/* The upper 8 bits of the calculated CRC are used to
				 * index the content of the Hash Table Reg 0,1,2,3,4,
				 * 5,6, and 7.
				 */
				crc32_val =
					(bitrev32(~crc32_le(~0, ha->addr, 6)) >> 24);
				/* The most significant 3 bits determines the register
				 * to use (Hash Table Reg X, X = 0,1,2,3,4,5,6 and 7) while
				 * the other 5(0x1F) bits determines the bit within the
				 * selected register
				 */
				mc_filter[crc32_val >> 5] |= (1 << (crc32_val & 0x1F));
			}
		}

		for (i = 0; i < HTR_CNT; i++)
			hw_if->update_hash_table_reg(i, mc_filter[i]);

	} else {
		netdev_for_each_mc_addr(ha, dev) {
			if (i < 32)
				hw_if->update_mac_addr1_31_low_high_reg(i, ha->addr);
			else
				hw_if->update_mac_addr32_127_low_high_reg(i, ha->addr);
			i++;
		}
	}

	return ret;
}

static int prepare_uc_list(struct net_device *dev)
{
	struct prv_data *pdata = netdev_priv(dev);
	struct hw_if_struct *hw_if = &pdata->hw_if;
	u32 uc_filter[HTR_CNT];
	struct netdev_hw_addr *ha = NULL;
	int crc32_val = 0;
	int ret = 0, i = 1;

	if (pdata->l2_filtering_mode) {
		ret = 1;
		memset(uc_filter, 0, sizeof(uc_filter));

		if (pdata->max_hash_table_size == 64) {
			netdev_for_each_uc_addr(ha, dev) {
				crc32_val =
					(bitrev32(~crc32_le(~0, ha->addr, 6)) >> 26);
				uc_filter[crc32_val >> 5] |= (1 << (crc32_val & 0x1F));
			}
		} else if (pdata->max_hash_table_size == 128) {
			netdev_for_each_uc_addr(ha, dev) {
				crc32_val =
					(bitrev32(~crc32_le(~0, ha->addr, 6)) >> 25);
				uc_filter[crc32_val >> 5] |= (1 << (crc32_val & 0x1F));
			}
		} else if (pdata->max_hash_table_size == 256) {
			netdev_for_each_uc_addr(ha, dev) {
				crc32_val =
					(bitrev32(~crc32_le(~0, ha->addr, 6)) >> 24);
				uc_filter[crc32_val >> 5] |= (1 << (crc32_val & 0x1F));
			}
		}

		/* configure hash value of real/default interface also */

		if (pdata->max_hash_table_size == 64) {
			crc32_val =
				(bitrev32(~crc32_le(~0, dev->dev_addr, 6)) >> 26);
			uc_filter[crc32_val >> 5] |= (1 << (crc32_val & 0x1F));
		} else if (pdata->max_hash_table_size == 128) {
			crc32_val =
				(bitrev32(~crc32_le(~0, dev->dev_addr, 6)) >> 25);
			uc_filter[crc32_val >> 5] |= (1 << (crc32_val & 0x1F));

		} else if (pdata->max_hash_table_size == 256) {
			crc32_val =
				(bitrev32(~crc32_le(~0, dev->dev_addr, 6)) >> 24);
			uc_filter[crc32_val >> 5] |= (1 << (crc32_val & 0x1F));
		}

		for (i = 0; i < HTR_CNT; i++)
			hw_if->update_hash_table_reg(i, uc_filter[i]);

	} else {
		netdev_for_each_uc_addr(ha, dev) {
			if (i < 32)
				hw_if->update_mac_addr1_31_low_high_reg(i, ha->addr);
			else
				hw_if->update_mac_addr32_127_low_high_reg(i, ha->addr);
			i++;
		}
	}

	return ret;
}

void set_rx_mode(struct net_device *dev)
{
	struct prv_data *pdata = netdev_priv(dev);
	struct hw_if_struct *hw_if = &pdata->hw_if;
	unsigned long flags;
	unsigned char pr_mode = 0;
	unsigned char huc_mode = 0;
	unsigned char hmc_mode = 0;
	unsigned char pm_mode = 0;
	unsigned char hpf_mode = 0;
	int mode, i;

	spin_lock_irqsave(&pdata->lock, flags);

	if (dev->flags & IFF_PROMISC) {
		pr_mode = 1;
	} else if ((dev->flags & IFF_ALLMULTI) ||
		   (netdev_mc_count(dev) > (pdata->max_hash_table_size))) {
		pm_mode = 1;
		if (pdata->max_hash_table_size) {
			for (i = 0; i < HTR_CNT; i++)
				hw_if->update_hash_table_reg(i, 0xffffffff);
		}
	} else if (!netdev_mc_empty(dev)) {
		if ((netdev_mc_count(dev) > (pdata->max_addr_reg_cnt - 1)) &&
		    (!pdata->max_hash_table_size)) {
			/* switch to PROMISCUOUS mode */
			pr_mode = 1;
		} else {
			mode = prepare_mc_list(dev);
			if (mode) {
				/* Hash filtering for multicast */
				hmc_mode = 1;
			} else {
				/* Perfect filtering for multicast */
				hmc_mode = 0;
				hpf_mode = 1;
			}
		}
	}

	/* Handle multiple unicast addresses */
	if ((netdev_uc_count(dev) > (pdata->max_addr_reg_cnt - 1)) &&
	    (!pdata->max_hash_table_size)) {
		/* switch to PROMISCUOUS mode */
		pr_mode = 1;
	} else if (!netdev_uc_empty(dev)) {
		mode = prepare_uc_list(dev);
		if (mode) {
			/* Hash filtering for unicast */
			huc_mode = 1;
		} else {
			/* Perfect filtering for unicast */
			huc_mode = 0;
			hpf_mode = 1;
		}
	}

	hw_if->config_mac_pkt_filter_reg(pr_mode, huc_mode,
					 hmc_mode, pm_mode, hpf_mode);

	spin_unlock_irqrestore(&pdata->lock, flags);
}

static void mmc_setup(struct prv_data *pdata)
{
	if (pdata->hw_feat.mmc_sel)
		memset(&pdata->mmc, 0, sizeof(struct mmc_counters));
	else
		pr_err("No MMC/RMON module available in the HW\n");
}

void napi_enable_mq(struct prv_data *pdata)
{
	struct rx_queue *rx_queue = NULL;
	int q_inx;

	for (q_inx = 0; q_inx < RX_QUEUE_CNT; q_inx++) {
		rx_queue = GET_RX_QUEUE_PTR(q_inx);
		napi_enable(&rx_queue->napi);
	}
}

static int open(struct net_device *dev)
{
	struct prv_data *pdata = netdev_priv(dev);
	int ret = Y_SUCCESS;
	struct hw_if_struct *hw_if = &pdata->hw_if;
	struct desc_if_struct *desc_if = &pdata->desc_if;

	pdata->irq_number = dev->irq;

	ret = request_irq(pdata->irq_number, ISR_SW_ETH,
			  0, DEV_NAME, pdata);

	if (ret != 0) {
		pr_err("Unable to register IRQ %d, ret %d\n",
		       pdata->irq_number, ret);
		ret = -EBUSY;
		goto err_irq_0;
	}
	ret = desc_if->alloc_buff_and_desc(pdata);
	if (ret < 0) {
		pr_err("failed to allocate buffer/descriptor memory\n");
		ret = -ENOMEM;
		goto err_out_desc_buf_alloc_failed;
	}

	/* default configuration */

	default_common_confs(pdata);
	default_tx_confs(pdata);
	default_rx_confs(pdata);
	configure_rx_fun_ptr(pdata);

	napi_enable_mq(pdata);

	set_rx_mode(dev);
	desc_if->wrapper_tx_desc_init(pdata);
	desc_if->wrapper_rx_desc_init(pdata);

	mmc_setup(pdata);

	/* initializes MAC and DMA */
	hw_if->init(pdata);

	if (pdata->phydev) {
		phy_start(pdata->phydev);
		netif_tx_start_all_queues(dev);
		}

	return ret;

 err_out_desc_buf_alloc_failed:
	free_irq(pdata->irq_number, pdata);
	pdata->irq_number = 0;

 err_irq_0:
	return ret;
}

void all_ch_napi_disable(struct prv_data *pdata)
{
	struct rx_queue *rx_queue = NULL;
	int q_inx;

	for (q_inx = 0; q_inx < RX_QUEUE_CNT; q_inx++) {
		rx_queue = GET_RX_QUEUE_PTR(q_inx);
		napi_disable(&rx_queue->napi);
	}
}

static int close(struct net_device *dev)
{
	struct prv_data *pdata = netdev_priv(dev);
	struct hw_if_struct *hw_if = &pdata->hw_if;
	struct desc_if_struct *desc_if = &pdata->desc_if;

	if (pdata->phydev)
		phy_stop(pdata->phydev);

	netif_tx_disable(dev);
	all_ch_napi_disable(pdata);

	/* issue software reset to device */
	hw_if->exit();
	desc_if->tx_free_mem(pdata);
	desc_if->rx_free_mem(pdata);
	if (pdata->irq_number != 0) {
		free_irq(pdata->irq_number, pdata);
		pdata->irq_number = 0;
	}

	return Y_SUCCESS;
}

unsigned int get_total_desc_cnt(struct prv_data *pdata, struct sk_buff *skb, unsigned int q_inx)
{
	unsigned int count = 0, size = 0;
	int length = 0;

	/* descriptors required based on data limit per descriptor */
	length = (skb->len - skb->data_len);
	while (length) {
		size = min(length, MAX_DATA_PER_TXD);
		count++;
		length = length - size;
	}

	return count;
}

int start_xmit(struct sk_buff *skb, struct net_device *dev)
{
	struct prv_data *pdata = netdev_priv(dev);
	unsigned int q_inx;
	struct tx_wrapper_descriptor *desc_data;
	struct s_tx_pkt_features *tx_pkt_features = GET_TX_PKT_FEATURES_PTR;
	unsigned long flags;
	unsigned int desc_count = 0;
	unsigned int count = 0;
	struct hw_if_struct *hw_if = &pdata->hw_if;
	struct desc_if_struct *desc_if = &pdata->desc_if;
	int retval = NETDEV_TX_OK;

	/* only queue0 support half duplex */
	if (!pdata->phydev->duplex)
		skb_set_queue_mapping(skb, 0);

	q_inx = skb_get_queue_mapping(skb);
	desc_data = GET_TX_WRAPPER_DESC(q_inx);

	spin_lock_irqsave(&pdata->tx_lock, flags);

	if (skb->len <= 0) {
		dev_kfree_skb_any(skb);
		pr_err("%s : Empty skb received from stack\n", dev->name);
		goto tx_netdev_return;
	}

	if ((skb_shinfo(skb)->gso_size == 0) && (skb->len > MTK_ETH_FRAME_LEN)) {
		pr_err("%s : big packet = %d\n", dev->name, (u16)skb->len);
		dev_kfree_skb_any(skb);
		dev->stats.tx_dropped++;
		goto tx_netdev_return;
	}

	memset(&pdata->tx_pkt_features, 0, sizeof(pdata->tx_pkt_features));

	/* check total number of desc required for current xfer */
	desc_count = get_total_desc_cnt(pdata, skb, q_inx);
	if (desc_data->free_desc_cnt < desc_count) {
		desc_data->queue_stopped = 1;
		netif_stop_subqueue(dev, q_inx);
		pr_err("stopped TX queue(%d) since there are no sufficient descriptor available for the current transfer, free(%d) require(%d)\n",
		       q_inx, desc_data->free_desc_cnt, desc_count);
		retval = NETDEV_TX_BUSY;
		goto tx_netdev_return;
	}

	/* check for hw tstamping */
	if (pdata->hw_feat.tsstssel && pdata->hwts_tx_en) {
		if (skb_shinfo(skb)->tx_flags & SKBTX_HW_TSTAMP) {
			/* declare that device is doing timestamping */
			skb_shinfo(skb)->tx_flags |= SKBTX_IN_PROGRESS;
			TX_PKT_FEATURES_PKT_ATTRIBUTES_PTP_ENABLE_MLF_WR(tx_pkt_features->pkt_attributes, 1);
		}
	}

	count = desc_if->map_tx_skb(dev, skb);
	if (count == 0) {
		dev_kfree_skb_any(skb);
		retval = NETDEV_TX_OK;
		goto tx_netdev_return;
	}

	desc_data->packet_count = count;

	desc_data->free_desc_cnt -= count;
	desc_data->tx_pkt_queued += count;

	/* fallback to software time stamping if core doesn't
	 * support hardware time stamping
	 */
	if ((pdata->hw_feat.tsstssel == 0) || (pdata->hwts_tx_en == 0))
		skb_tx_timestamp(skb);

	/* configure required descriptor fields for transmission */
	hw_if->pre_xmit(pdata, q_inx);

tx_netdev_return:
	spin_unlock_irqrestore(&pdata->tx_lock, flags);

	return retval;
}

static struct net_device_stats *get_stats(struct net_device *dev)
{
	return &dev->stats;
}

#ifdef CONFIG_NET_POLL_CONTROLLER

/* \brief API to receive packets in polling mode.
 *
 * \details This is polling receive function used by netconsole and other
 * diagnostic tool to allow network i/o with interrupts disabled.
 *
 * \param[in] dev - pointer to net_device structure
 *
 * \return void
 */
static void poll_controller(struct net_device *dev)
{
	struct prv_data *pdata = netdev_priv(dev);

	disable_irq(pdata->irq_number);
	ISR_SW_ETH(pdata->irq_number, pdata);
	enable_irq(pdata->irq_number);
}

#endif	/*end of CONFIG_NET_POLL_CONTROLLER */

static int handle_hwtstamp_ioctl(struct prv_data *pdata, struct ifreq *ifr)
{
	struct hw_if_struct *hw_if = &pdata->hw_if;
	struct hwtstamp_config config;
	u32 ptp_v2 = 0;
	u32 tstamp_all = 0;
	u32 ptp_over_ethernet = 0;
	u32 snap_type_sel = 0;
	u32 ts_master_en = 0;
	u32 ts_event_en = 0;
	u32 av_8021asm_en = 0;
	u32 MAC_TCR = 0;
	u64 temp = 0;
	struct timespec now;

	if (!pdata->hw_feat.tsstssel) {
		pr_err("No hw timestamping is available in this core\n");
		return -EOPNOTSUPP;
	}

	if (copy_from_user(&config, ifr->ifr_data, sizeof(struct hwtstamp_config)))
		return -EFAULT;

	pr_err("config.flags = %#x, tx_type = %#x, rx_filter = %#x\n",
	       config.flags, config.tx_type, config.rx_filter);

	/* reserved for future extensions */
	if (config.flags)
		return -EINVAL;

	switch (config.tx_type) {
	case HWTSTAMP_TX_OFF:
		pdata->hwts_tx_en = 0;
		break;
	case HWTSTAMP_TX_ON:
		pdata->hwts_tx_en = 1;
		break;
	default:
		return -ERANGE;
	}

	switch (config.rx_filter) {
	/* time stamp no incoming packet at all */
	case HWTSTAMP_FILTER_NONE:
		config.rx_filter = HWTSTAMP_FILTER_NONE;
		break;

	/* PTP v2/802.AS1, any layer, any kind of event packet */
	case HWTSTAMP_FILTER_PTP_V2_EVENT:
		config.rx_filter = HWTSTAMP_FILTER_PTP_V2_EVENT;
		ptp_v2 = MAC_TCR_TSVER2ENA;
		/* take time stamp for all event messages */
		snap_type_sel = MAC_TCR_SNAPTYPSEL_1;

		ptp_over_ethernet = MAC_TCR_TSIPENA;
		av_8021asm_en = MAC_TCR_AV8021ASMEN;
		break;

	/* PTP v2/802.AS1, any layer, Sync packet */
	case HWTSTAMP_FILTER_PTP_V2_SYNC:
		config.rx_filter = HWTSTAMP_FILTER_PTP_V2_SYNC;
		ptp_v2 = MAC_TCR_TSVER2ENA;
		/* take time stamp for SYNC messages only */
		ts_event_en = MAC_TCR_TSEVENTENA;

		ptp_over_ethernet = MAC_TCR_TSIPENA;
		av_8021asm_en = MAC_TCR_AV8021ASMEN;
		break;

	/* PTP v2/802.AS1, any layer, Delay_req packet */
	case HWTSTAMP_FILTER_PTP_V2_DELAY_REQ:
		config.rx_filter = HWTSTAMP_FILTER_PTP_V2_DELAY_REQ;
		ptp_v2 = MAC_TCR_TSVER2ENA;
		/* take time stamp for Delay_Req messages only */
		ts_master_en = MAC_TCR_TSMASTERENA;
		ts_event_en = MAC_TCR_TSEVENTENA;

		ptp_over_ethernet = MAC_TCR_TSIPENA;
		av_8021asm_en = MAC_TCR_AV8021ASMEN;
		break;

	/* time stamp any incoming packet */
	case HWTSTAMP_FILTER_ALL:
		config.rx_filter = HWTSTAMP_FILTER_ALL;
		tstamp_all = MAC_TCR_TSENALL;
		break;

	default:
		return -ERANGE;
	}
	pdata->hwts_rx_en = ((config.rx_filter == HWTSTAMP_FILTER_NONE) ? 0 : 1);

	if (!pdata->hwts_tx_en && !pdata->hwts_rx_en) {
		/* disable hw time stamping */
		hw_if->config_hw_time_stamping(MAC_TCR);
	} else {
		MAC_TCR = (MAC_TCR_TSENA | MAC_TCR_TSCFUPDT | MAC_TCR_TSCTRLSSR |
			   tstamp_all | ptp_v2 | ptp_over_ethernet | ts_event_en | ts_master_en |
			   snap_type_sel | av_8021asm_en);

		if (!pdata->one_nsec_accuracy)
			MAC_TCR &= ~MAC_TCR_TSCTRLSSR;

		hw_if->config_hw_time_stamping(MAC_TCR);

		/* program Sub Second Increment Reg */
		hw_if->config_sub_second_increment(SYSCLOCK);

		/* formula is :
		 * addend = 2^32/freq_div_ratio;
		 *
		 * where, freq_div_ratio = SYSCLOCK/50MHz
		 *
		 * hence, addend = ((2^32) * 50MHz)/SYSCLOCK;
		 *
		 * NOTE: SYSCLOCK should be >= 50MHz to
		 *       achive 20ns accuracy.
		 *
		 * 2^x * y == (y << x), hence
		 * 2^32 * 50000000 ==> (50000000 << 32)
		 */
		temp = (u64)(50000000ULL << 32);
		pdata->default_addend = div_u64(temp, SYSCLOCK);
		hw_if->config_addend(pdata->default_addend);

		/* initialize system time */
		getnstimeofday(&now);
		hw_if->init_systime(now.tv_sec, now.tv_nsec);
	}

	return (copy_to_user(ifr->ifr_data, &config, sizeof(struct hwtstamp_config))) ? -EFAULT : 0;
}

static int FRAME_PATTERN_CH[8] = {
	0x11111111,
	0x22222222,
	0x33333333,
	0x44444444,
	0x55555555,
	0x66666666,
	0x77777777,
	0x88888888,
};

static int frame_hdrs[8][4] = {
	/* for channel 0 : Non tagged header
	 * Dst addr : 0x00:0x0D:0x56:0x73:0xD0:0xF3
	 * Src addr : 0x00:0x55:0x7B:0xB5:0x7D:0xF7
	 * Type/Length : 0x800
	 */
	{0xFFFFFFFF, 0x5500FFFF, 0xF77DB57B, 0x00000008},

	/* for channel 1 : VLAN tagged header with priority 1
	 * Dst addr : 0x00:0x0D:0x56:0x73:0xD0:0xF3
	 * Src addr : 0x00:0x55:0x7B:0xB5:0x7D:0xF7
	 * Type/Length : 0x8100
	 */
	{0xFFFFFFFF, 0x5500FFFF, 0xF77DB57B, 0x64200081},

	/* for channel 2 : VLAN tagged header with priority 2
	 * Dst addr : 0x00:0x0D:0x56:0x73:0xD0:0xF3
	 * Src addr : 0x00:0x55:0x7B:0xB5:0x7D:0xF7
	 * Type/Length : 0x8100
	 */
	{0xFFFFFFFF, 0x5500FFFF, 0xF77DB57B, 0x64400081},

	/* for channel 3 : VLAN tagged header with priority 3
	 * Dst addr : 0x00:0x0D:0x56:0x73:0xD0:0xF3
	 * Src addr : 0x00:0x55:0x7B:0xB5:0x7D:0xF7
	 * Type/Length : 0x8100
	 */
	{0x73560D00, 0x5500F3D0, 0xF77DB57B, 0x64600081},

	/* for channel 4 : VLAN tagged header with priority 4
	 * Dst addr : 0x00:0x0D:0x56:0x73:0xD0:0xF3
	 * Src addr : 0x00:0x55:0x7B:0xB5:0x7D:0xF7
	 * Type/Length : 0x8100
	 */
	{0x73560D00, 0x5500F3D0, 0xF77DB57B, 0x64800081},

	/* for channel 5 : VLAN tagged header with priority 5
	 * Dst addr : 0x00:0x0D:0x56:0x73:0xD0:0xF3
	 * Src addr : 0x00:0x55:0x7B:0xB5:0x7D:0xF7
	 * Type/Length : 0x8100
	 */
	{0x73560D00, 0x5500F3D0, 0xF77DB57B, 0x64A00081},

	/* for channel 6 : VLAN tagged header with priority 6
	 * Dst addr : 0x00:0x0D:0x56:0x73:0xD0:0xF3
	 * Src addr : 0x00:0x55:0x7B:0xB5:0x7D:0xF7
	 * Type/Length : 0x8100
	 */
	{0x73560D00, 0x5500F3D0, 0xF77DB57B, 0x64C00081},

	/* for channel 7 : VLAN tagged header with priority 7
	 * Dst addr : 0x00:0x0D:0x56:0x73:0xD0:0xF3
	 * Src addr : 0x00:0x55:0x7B:0xB5:0x7D:0xF7
	 * Type/Length : 0x8100
	 */
	{0x73560D00, 0x5500F3D0, 0xF77DB57B, 0x64E00081},
};

int send_frame(struct prv_data *pdata, int num)
{
	struct sk_buff *skb = NULL;
	unsigned int *skb_data = NULL;
	int payload_cnt = 0;
	int i, j;
	unsigned int frame_size = 1500;
	unsigned long tx_framecount, tx_octetcount;
	unsigned long rx_framecount, rx_octetcount;

	pdata->mmc.mmc_tx_framecount_gb += mtk_eth_reg_read(MMC_TXPACKETCOUNT_GB_REG_ADDR);
	pdata->mmc.mmc_tx_octetcount_gb += mtk_eth_reg_read(MMC_TXOCTETCOUNT_GB_REG_ADDR);
	pdata->mmc.mmc_rx_framecount_gb += mtk_eth_reg_read(MMC_RXPACKETCOUNT_GB_REG_ADDR);
	pdata->mmc.mmc_rx_octetcount_gb += mtk_eth_reg_read(MMC_RXOCTETCOUNT_GB_REG_ADDR);
	usleep_range(1000, 2000);
	for (j = 0; j < TX_QUEUE_CNT; j++) {
		for (i = 0; i < num; i++) {
			payload_cnt = 0;
			skb = dev_alloc_skb(MTK_ETH_FRAME_LEN);
			if (!skb) {
				pr_err("Failed to allocate tx skb\n");
				return -CONFIG_FAIL;
			}

			skb_set_queue_mapping(skb, j); /*  map skb to queue0 */
			skb_data = (unsigned int *)skb->data;
			/* Add Ethernet header */
			*skb_data++ = frame_hdrs[j][0];
			*skb_data++ = frame_hdrs[j][1];
			*skb_data++ = frame_hdrs[j][2];
			*skb_data++ = frame_hdrs[j][3];
			/* Add payload */
			for (payload_cnt = 0; payload_cnt < frame_size;) {
				*skb_data++ = FRAME_PATTERN_CH[j];
				/* increment by 4 since we are writing
				 * one dword at a time
				 */
				payload_cnt += 4;
			}
			skb->len = frame_size;
			start_xmit(skb, pdata->dev);
		}
	}

	usleep_range(1000, 2000);
	tx_framecount = mtk_eth_reg_read(MMC_TXPACKETCOUNT_GB_REG_ADDR);
	tx_octetcount = mtk_eth_reg_read(MMC_TXOCTETCOUNT_GB_REG_ADDR);
	pdata->mmc.mmc_tx_framecount_gb += tx_framecount;
	pdata->mmc.mmc_tx_octetcount_gb += tx_octetcount;

	rx_framecount = mtk_eth_reg_read(MMC_RXPACKETCOUNT_GB_REG_ADDR);
	rx_octetcount = mtk_eth_reg_read(MMC_RXOCTETCOUNT_GB_REG_ADDR);
	pdata->mmc.mmc_rx_framecount_gb += rx_framecount;
	pdata->mmc.mmc_rx_octetcount_gb += rx_octetcount;

	if ((tx_framecount == rx_framecount) &&
	    (tx_octetcount == rx_octetcount) &&
	    (tx_framecount == TX_QUEUE_CNT * num)) {
		pr_err("loop back success:\ntx_framecount:%lu\t tx_octetcount:%lu\nrx_framecount:%lu\t rx_octetcount:%lu\n",
		       tx_framecount, tx_octetcount,
		       rx_framecount, rx_octetcount);
		return CONFIG_SUCCESS;
		}
	else {
		pr_err("loop back fail:\ntx_framecount:%lu\t tx_octetcount:%lu\nrx_framecount:%lu\t rx_octetcount:%lu\n",
		       tx_framecount, tx_octetcount,
		       rx_framecount, rx_octetcount);
		return CONFIG_FAIL;
	}
}

static int handle_prv_ioctl(struct prv_data *pdata, struct ifr_data_struct *req)
{
	struct hw_if_struct *hw_if = &pdata->hw_if;
	int ret = 0;

	switch (req->cmd) {
	case MAC_READ_CMD:
		(req->num) = ioread32((void *)(BASE_ADDRESS + req->q_inx));
		ret = CONFIG_SUCCESS;
		break;
	case MAC_WRITE_CMD:
		iowrite32(req->num, (void *)(BASE_ADDRESS + req->q_inx));
		ret = CONFIG_SUCCESS;
		break;
	case PHY_WRITE_CMD:
		ret = hw_if->write_phy_regs(pdata->phyaddr, req->q_inx, req->num);
		if (ret == 0)
			ret = CONFIG_SUCCESS;
		else
			ret = CONFIG_FAIL;
		break;

	case PHY_READ_CMD:
		hw_if->read_phy_regs(pdata->phyaddr, req->q_inx, &req->num);
		if (ret == 0)
			ret = CONFIG_SUCCESS;
		else
			ret = CONFIG_FAIL;
		break;
	case SEND_FRAME_CMD:
		ret = send_frame(pdata, req->num);
		break;

	default:
		ret = -EOPNOTSUPP;
		pr_err("Unsupported command call\n");
	}

	return ret;
}

static int ioctl(struct net_device *dev, struct ifreq *ifr, int cmd)
{
	struct prv_data *pdata = netdev_priv(dev);
	struct ifr_data_struct *req = ifr->ifr_ifru.ifru_data;
	struct mii_ioctl_data *data = if_mii(ifr);
	unsigned int reg_val = 0;
	int ret = 0;

	switch (cmd) {
	case SIOCGMIIPHY:
		data->phy_id = pdata->phyaddr;
		pr_err("PHY ID: SIOCGMIIPHY\n");
		break;

	case SIOCGMIIREG:
		ret =
		    mdio_read_direct(pdata, pdata->phyaddr,
				     (data->reg_num & 0x1F), &reg_val);
		if (ret)
			ret = -EIO;

		data->val_out = reg_val;
		pr_err("PHY ID: SIOCGMIIREG reg:%#x reg_val:%#x\n",
		       (data->reg_num & 0x1F), reg_val);
		break;

	case SIOCSMIIREG:
		pr_err("PHY ID: SIOCSMIIPHY\n");
		break;

	case PRV_IOCTL:
		ret = handle_prv_ioctl(pdata, req);
		break;

	case SIOCSHWTSTAMP:
		ret = handle_hwtstamp_ioctl(pdata, ifr);
		break;

	default:
		ret = -EOPNOTSUPP;
		pr_err("Unsupported IOCTL call\n");
	}

	return ret;
}

static const struct net_device_ops netdev_ops = {
	.ndo_open = open,
	.ndo_stop = close,
	.ndo_start_xmit = start_xmit,
	.ndo_get_stats = get_stats,
	.ndo_set_rx_mode = set_rx_mode,
#ifdef CONFIG_NET_POLL_CONTROLLER
	.ndo_poll_controller = poll_controller,
#endif
	.ndo_do_ioctl = ioctl,
	.ndo_set_mac_address = eth_mac_addr,
	.ndo_validate_addr = eth_validate_addr,
	.ndo_change_mtu = eth_change_mtu,
};

const struct net_device_ops *get_netdev_ops(void)
{
	return (const struct net_device_ops *)&netdev_ops;
}

phy_interface_t get_phy_interface(struct prv_data *pdata)
{
	phy_interface_t ret = PHY_INTERFACE_MODE_MII;

	if (pdata->hw_feat.act_phy_sel == GMII_MII) {
		if (pdata->hw_feat.gmii_sel)
			ret = PHY_INTERFACE_MODE_GMII;
		else if (pdata->hw_feat.mii_sel)
			ret = PHY_INTERFACE_MODE_MII;
	} else if (pdata->hw_feat.act_phy_sel == RGMII) {
		ret = PHY_INTERFACE_MODE_RGMII;
	} else if (pdata->hw_feat.act_phy_sel == SGMII) {
		ret = PHY_INTERFACE_MODE_SGMII;
	} else if (pdata->hw_feat.act_phy_sel == TBI) {
		ret = PHY_INTERFACE_MODE_TBI;
	} else if (pdata->hw_feat.act_phy_sel == RMII) {
		ret = PHY_INTERFACE_MODE_RMII;
	} else if (pdata->hw_feat.act_phy_sel == RTBI) {
		ret = PHY_INTERFACE_MODE_RTBI;
	} else if (pdata->hw_feat.act_phy_sel == SMII) {
		ret = PHY_INTERFACE_MODE_SMII;
	} else {
		pr_err("Missing interface support between PHY and MAC\n\n");
		ret = PHY_INTERFACE_MODE_NA;
	}

	return ret;
}

void enable_all_ch_rx_interrpt(
			struct prv_data *pdata)
{
	struct hw_if_struct *hw_if = &pdata->hw_if;
	unsigned int q_inx;

	for (q_inx = 0; q_inx < RX_QUEUE_CNT; q_inx++)
		hw_if->enable_rx_interrupt(q_inx);
}

int poll_mq(struct napi_struct *napi, int budget)
{
	struct rx_queue *rx_queue =
		container_of(napi, struct rx_queue, napi);
	struct prv_data *pdata = rx_queue->pdata;
	/* divide the budget evenly among all the queues */
	int per_q_budget = budget / RX_QUEUE_CNT;
	int q_inx = 0;
	int received = 0, per_q_received = 0;

	pdata->xstats.napi_poll_n++;

	for (q_inx = 0; q_inx < RX_QUEUE_CNT; q_inx++) {
		rx_queue = GET_RX_QUEUE_PTR(q_inx);

		per_q_received = pdata->clean_rx(pdata, per_q_budget, q_inx);

		received += per_q_received;
		pdata->xstats.rx_pkt_n += per_q_received;
		pdata->xstats.q_rx_pkt_n[q_inx] += per_q_received;
	}

	/* If we processed all pkts, we are done;
	 * tell the kernel & re-enable interrupt
	 */
	if (received < budget) {
		unsigned long flags;

		spin_lock_irqsave(&pdata->lock, flags);
		__napi_complete(napi);
		/* Enable all ch RX interrupt */
		enable_all_ch_rx_interrpt(pdata);
		spin_unlock_irqrestore(&pdata->lock, flags);
	}

	return received;
}

inline unsigned int mtk_eth_reg_read(unsigned long addr)
{
		return ioread32((void *)addr);
}

void mmc_read(struct mmc_counters *mmc)
{
	/* MMC TX counter registers */
	mmc->mmc_tx_octetcount_gb += mtk_eth_reg_read(MMC_TXOCTETCOUNT_GB_REG_ADDR);
	mmc->mmc_tx_framecount_gb += mtk_eth_reg_read(MMC_TXPACKETCOUNT_GB_REG_ADDR);
	mmc->mmc_tx_broadcastframe_g += mtk_eth_reg_read(MMC_TXBROADCASTPACKETS_G_REG_ADDR);
	mmc->mmc_tx_multicastframe_g += mtk_eth_reg_read(MMC_TXMULTICASTPACKETS_G_REG_ADDR);
	mmc->mmc_tx_64_octets_gb += mtk_eth_reg_read(MMC_TX64OCTETS_GB_REG_ADDR);
	mmc->mmc_tx_65_to_127_octets_gb += mtk_eth_reg_read(MMC_TX65TO127OCTETS_GB_REG_ADDR);
	mmc->mmc_tx_128_to_255_octets_gb += mtk_eth_reg_read(MMC_TX128TO255OCTETS_GB_REG_ADDR);
	mmc->mmc_tx_256_to_511_octets_gb += mtk_eth_reg_read(MMC_TX256TO511OCTETS_GB_REG_ADDR);
	mmc->mmc_tx_512_to_1023_octets_gb += mtk_eth_reg_read(MMC_TX512TO1023OCTETS_GB_REG_ADDR);
	mmc->mmc_tx_1024_to_max_octets_gb += mtk_eth_reg_read(MMC_TX1024TOMAXOCTETS_GB_REG_ADDR);
	mmc->mmc_tx_unicast_gb += mtk_eth_reg_read(MMC_TXUNICASTPACKETS_GB_REG_ADDR);
	mmc->mmc_tx_multicast_gb += mtk_eth_reg_read(MMC_TXMULTICASTPACKETS_GB_REG_ADDR);
	mmc->mmc_tx_broadcast_gb += mtk_eth_reg_read(MMC_TXBROADCASTPACKETS_GB_REG_ADDR);
	mmc->mmc_tx_underflow_error += mtk_eth_reg_read(MMC_TXUNDERFLOWERROR_REG_ADDR);
	mmc->mmc_tx_singlecol_g += mtk_eth_reg_read(MMC_TXSINGLECOL_G_REG_ADDR);
	mmc->mmc_tx_multicol_g += mtk_eth_reg_read(MMC_TXMULTICOL_G_REG_ADDR);
	mmc->mmc_tx_deferred += mtk_eth_reg_read(MMC_TXDEFERRED_REG_ADDR);
	mmc->mmc_tx_latecol += mtk_eth_reg_read(MMC_TXLATECOL_REG_ADDR);
	mmc->mmc_tx_exesscol += mtk_eth_reg_read(MMC_TXEXESSCOL_REG_ADDR);
	mmc->mmc_tx_carrier_error += mtk_eth_reg_read(MMC_TXCARRIERERROR_REG_ADDR);
	mmc->mmc_tx_octetcount_g += mtk_eth_reg_read(MMC_TXOCTETCOUNT_G_REG_ADDR);
	mmc->mmc_tx_framecount_g += mtk_eth_reg_read(MMC_TXPACKETSCOUNT_G_REG_ADDR);
	mmc->mmc_tx_excessdef += mtk_eth_reg_read(MMC_TXEXCESSDEF_REG_ADDR);
	mmc->mmc_tx_pause_frame += mtk_eth_reg_read(MMC_TXPAUSEPACKETS_REG_ADDR);
	mmc->mmc_tx_vlan_frame_g += mtk_eth_reg_read(MMC_TXVLANPACKETS_G_REG_ADDR);
	mmc->mmc_tx_osize_frame_g += mtk_eth_reg_read(MMC_TXOVERSIZE_G_REG_ADDR);

	/* MMC RX counter registers */
	mmc->mmc_rx_framecount_gb += mtk_eth_reg_read(MMC_RXPACKETCOUNT_GB_REG_ADDR);
	mmc->mmc_rx_octetcount_gb += mtk_eth_reg_read(MMC_RXOCTETCOUNT_GB_REG_ADDR);
	mmc->mmc_rx_octetcount_g += mtk_eth_reg_read(MMC_RXOCTETCOUNT_G_REG_ADDR);
	mmc->mmc_rx_broadcastframe_g += mtk_eth_reg_read(MMC_RXBROADCASTPACKETS_G_REG_ADDR);
	mmc->mmc_rx_multicastframe_g += mtk_eth_reg_read(MMC_RXMULTICASTPACKETS_G_REG_ADDR);
	mmc->mmc_rx_crc_errror += mtk_eth_reg_read(MMC_RXCRCERROR_REG_ADDR);
	mmc->mmc_rx_align_error += mtk_eth_reg_read(MMC_RXALIGNMENTERROR_REG_ADDR);
	mmc->mmc_rx_run_error += mtk_eth_reg_read(MMC_RXRUNTERROR_REG_ADDR);
	mmc->mmc_rx_jabber_error += mtk_eth_reg_read(MMC_RXJABBERERROR_REG_ADDR);
	mmc->mmc_rx_undersize_g += mtk_eth_reg_read(MMC_RXUNDERSIZE_G_REG_ADDR);
	mmc->mmc_rx_oversize_g += mtk_eth_reg_read(MMC_RXOVERSIZE_G_REG_ADDR);
	mmc->mmc_rx_64_octets_gb += mtk_eth_reg_read(MMC_RX64OCTETS_GB_REG_ADDR);
	mmc->mmc_rx_65_to_127_octets_gb += mtk_eth_reg_read(MMC_RX65TO127OCTETS_GB_REG_ADDR);
	mmc->mmc_rx_128_to_255_octets_gb += mtk_eth_reg_read(MMC_RX128TO255OCTETS_GB_REG_ADDR);
	mmc->mmc_rx_256_to_511_octets_gb += mtk_eth_reg_read(MMC_RX256TO511OCTETS_GB_REG_ADDR);
	mmc->mmc_rx_512_to_1023_octets_gb += mtk_eth_reg_read(MMC_RX512TO1023OCTETS_GB_REG_ADDR);
	mmc->mmc_rx_1024_to_max_octets_gb += mtk_eth_reg_read(MMC_RX1024TOMAXOCTETS_GB_REG_ADDR);
	mmc->mmc_rx_unicast_g += mtk_eth_reg_read(MMC_RXUNICASTPACKETS_G_REG_ADDR);
	mmc->mmc_rx_length_error += mtk_eth_reg_read(MMC_RXLENGTHERROR_REG_ADDR);
	mmc->mmc_rx_outofrangetype += mtk_eth_reg_read(MMC_RXOUTOFRANGETYPE_REG_ADDR);
	mmc->mmc_rx_pause_frames += mtk_eth_reg_read(MMC_RXPAUSEPACKETS_REG_ADDR);
	mmc->mmc_rx_fifo_overflow += mtk_eth_reg_read(MMC_RXFIFOOVERFLOW_REG_ADDR);
	mmc->mmc_rx_vlan_frames_gb += mtk_eth_reg_read(MMC_RXVLANPACKETS_GB_REG_ADDR);
	mmc->mmc_rx_watchdog_error += mtk_eth_reg_read(MMC_RXWATCHDOGERROR_REG_ADDR);
	mmc->mmc_rx_receive_error += mtk_eth_reg_read(MMC_RXRCVERROR_REG_ADDR);
	mmc->mmc_rx_ctrl_frames_g += mtk_eth_reg_read(MMC_RXCTRLPACKETS_G_REG_ADDR);

	/* IPC */
	mmc->mmc_rx_ipc_intr_mask += mtk_eth_reg_read(MMC_IPC_INTR_MASK_RX_REG_ADDR);
	mmc->mmc_rx_ipc_intr += mtk_eth_reg_read(MMC_IPC_INTR_RX_REG_ADDR);

	/* IPv4 */
	mmc->mmc_rx_ipv4_gd += mtk_eth_reg_read(MMC_RXIPV4_GD_PKTS_REG_ADDR);
	mmc->mmc_rx_ipv4_hderr += mtk_eth_reg_read(MMC_RXIPV4_HDRERR_PKTS_REG_ADDR);
	mmc->mmc_rx_ipv4_nopay += mtk_eth_reg_read(MMC_RXIPV4_NOPAY_PKTS_REG_ADDR);
	mmc->mmc_rx_ipv4_frag += mtk_eth_reg_read(MMC_RXIPV4_FRAG_PKTS_REG_ADDR);
	mmc->mmc_rx_ipv4_udsbl += mtk_eth_reg_read(MMC_RXIPV4_UBSBL_PKTS_REG_ADDR);

	/* IPV6 */
	mmc->mmc_rx_ipv6_gd += mtk_eth_reg_read(MMC_RXIPV6_GD_PKTS_REG_ADDR);
	mmc->mmc_rx_ipv6_hderr += mtk_eth_reg_read(MMC_RXIPV6_HDRERR_PKTS_REG_ADDR);
	mmc->mmc_rx_ipv6_nopay += mtk_eth_reg_read(MMC_RXIPV6_NOPAY_PKTS_REG_ADDR);

	/* Protocols */
	mmc->mmc_rx_udp_gd += mtk_eth_reg_read(MMC_RXUDP_GD_PKTS_REG_ADDR);
	mmc->mmc_rx_udp_err += mtk_eth_reg_read(MMC_RXUDP_ERR_PKTS_REG_ADDR);
	mmc->mmc_rx_tcp_gd += mtk_eth_reg_read(MMC_RXTCP_GD_PKTS_REG_ADDR);
	mmc->mmc_rx_tcp_err += mtk_eth_reg_read(MMC_RXTCP_ERR_PKTS_REG_ADDR);
	mmc->mmc_rx_icmp_gd += mtk_eth_reg_read(MMC_RXICMP_GD_PKTS_REG_ADDR);
	mmc->mmc_rx_icmp_err += mtk_eth_reg_read(MMC_RXICMP_ERR_PKTS_REG_ADDR);

	/* IPv4 */
	mmc->mmc_rx_ipv4_gd_octets += mtk_eth_reg_read(MMC_RXIPV4_GD_OCTETS_REG_ADDR);
	mmc->mmc_rx_ipv4_hderr_octets += mtk_eth_reg_read(MMC_RXIPV4_HDRERR_OCTETS_REG_ADDR);
	mmc->mmc_rx_ipv4_nopay_octets += mtk_eth_reg_read(MMC_RXIPV4_NOPAY_OCTETS_REG_ADDR);
	mmc->mmc_rx_ipv4_frag_octets += mtk_eth_reg_read(MMC_RXIPV4_FRAG_OCTETS_REG_ADDR);
	mmc->mmc_rx_ipv4_udsbl_octets += mtk_eth_reg_read(MMC_RXIPV4_UDSBL_OCTETS_REG_ADDR);

	/* IPV6 */
	mmc->mmc_rx_ipv6_gd_octets += mtk_eth_reg_read(MMC_RXIPV6_GD_OCTETS_REG_ADDR);
	mmc->mmc_rx_ipv6_hderr_octets += mtk_eth_reg_read(MMC_RXIPV6_HDRERR_OCTETS_REG_ADDR);
	mmc->mmc_rx_ipv6_nopay_octets += mtk_eth_reg_read(MMC_RXIPV6_NOPAY_OCTETS_REG_ADDR);

	/* Protocols */
	mmc->mmc_rx_udp_gd_octets += mtk_eth_reg_read(MMC_RXUDP_GD_OCTETS_REG_ADDR);
	mmc->mmc_rx_udp_err_octets += mtk_eth_reg_read(MMC_RXUDP_ERR_OCTETS_REG_ADDR);
	mmc->mmc_rx_tcp_gd_octets += mtk_eth_reg_read(MMC_RXTCP_GD_OCTETS_REG_ADDR);
	mmc->mmc_rx_tcp_err_octets += mtk_eth_reg_read(MMC_RXTCP_ERR_OCTETS_REG_ADDR);
	mmc->mmc_rx_icmp_gd_octets += mtk_eth_reg_read(MMC_RXICMP_GD_OCTETS_REG_ADDR);
	mmc->mmc_rx_icmp_err_octets += mtk_eth_reg_read(MMC_RXICMP_ERR_OCTETS_REG_ADDR);
}

void get_all_hw_features(struct prv_data *pdata)
{
	unsigned int MAC_HFR0;
	unsigned int MAC_HFR1;
	unsigned int MAC_HFR2;

	MAC_HFR0_REG_RD(MAC_HFR0);
	MAC_HFR1_REG_RD(MAC_HFR1);
	MAC_HFR2_REG_RD(MAC_HFR2);

	memset(&pdata->hw_feat, 0, sizeof(pdata->hw_feat));
	pdata->hw_feat.mii_sel = ((MAC_HFR0 >> 0) & MAC_HFR0_MIISEL_MASK);
	pdata->hw_feat.gmii_sel = ((MAC_HFR0 >> 1) & MAC_HFR0_GMIISEL_MASK);
	pdata->hw_feat.hd_sel = ((MAC_HFR0 >> 2) & MAC_HFR0_HDSEL_MASK);
	pdata->hw_feat.pcs_sel = ((MAC_HFR0 >> 3) & MAC_HFR0_PCSSEL_MASK);
	pdata->hw_feat.vlan_hash_en =
	    ((MAC_HFR0 >> 4) & MAC_HFR0_VLANHASEL_MASK);
	pdata->hw_feat.sma_sel = ((MAC_HFR0 >> 5) & MAC_HFR0_SMASEL_MASK);
	pdata->hw_feat.rwk_sel = ((MAC_HFR0 >> 6) & MAC_HFR0_RWKSEL_MASK);
	pdata->hw_feat.mgk_sel = ((MAC_HFR0 >> 7) & MAC_HFR0_MGKSEL_MASK);
	pdata->hw_feat.mmc_sel = ((MAC_HFR0 >> 8) & MAC_HFR0_MMCSEL_MASK);
	pdata->hw_feat.arp_offld_en =
	    ((MAC_HFR0 >> 9) & MAC_HFR0_ARPOFFLDEN_MASK);
	pdata->hw_feat.ts_sel =
	    ((MAC_HFR0 >> 12) & MAC_HFR0_TSSSEL_MASK);
	pdata->hw_feat.eee_sel = ((MAC_HFR0 >> 13) & MAC_HFR0_EEESEL_MASK);
	pdata->hw_feat.tx_coe_sel =
	    ((MAC_HFR0 >> 14) & MAC_HFR0_TXCOESEL_MASK);
	pdata->hw_feat.rx_coe_sel =
	    ((MAC_HFR0 >> 16) & MAC_HFR0_RXCOE_MASK);
	pdata->hw_feat.mac_addr16_sel =
	    ((MAC_HFR0 >> 18) & MAC_HFR0_ADDMACADRSEL_MASK);
	pdata->hw_feat.mac_addr32_sel =
	    ((MAC_HFR0 >> 23) & MAC_HFR0_MACADR32SEL_MASK);
	pdata->hw_feat.mac_addr64_sel =
	    ((MAC_HFR0 >> 24) & MAC_HFR0_MACADR64SEL_MASK);
	pdata->hw_feat.tsstssel =
	    ((MAC_HFR0 >> 25) & MAC_HFR0_TSINTSEL_MASK);
	pdata->hw_feat.sa_vlan_ins =
	    ((MAC_HFR0 >> 27) & MAC_HFR0_SAVLANINS_MASK);
	pdata->hw_feat.act_phy_sel =
	    ((MAC_HFR0 >> 28) & MAC_HFR0_ACTPHYSEL_MASK);

	pdata->hw_feat.rx_fifo_size =
	    ((MAC_HFR1 >> 0) & MAC_HFR1_RXFIFOSIZE_MASK);
	pdata->hw_feat.tx_fifo_size =
	    ((MAC_HFR1 >> 6) & MAC_HFR1_TXFIFOSIZE_MASK);
	pdata->hw_feat.adv_ts_hword =
	    ((MAC_HFR1 >> 13) & MAC_HFR1_ADVTHWORD_MASK);
	pdata->hw_feat.dcb_en = ((MAC_HFR1 >> 16) & MAC_HFR1_DCBEN_MASK);
	pdata->hw_feat.sph_en = ((MAC_HFR1 >> 17) & MAC_HFR1_SPHEN_MASK);
	pdata->hw_feat.tso_en = ((MAC_HFR1 >> 18) & MAC_HFR1_TSOEN_MASK);
	pdata->hw_feat.dma_debug_gen =
	    ((MAC_HFR1 >> 19) & MAC_HFR1_DMADEBUGEN_MASK);
	pdata->hw_feat.av_sel = ((MAC_HFR1 >> 20) & MAC_HFR1_AVSEL_MASK);
	pdata->hw_feat.lp_mode_en =
	    ((MAC_HFR1 >> 23) & MAC_HFR1_LPMODEEN_MASK);
	pdata->hw_feat.hash_tbl_sz =
	    ((MAC_HFR1 >> 24) & MAC_HFR1_HASHTBLSZ_MASK);
	pdata->hw_feat.l3l4_filter_num =
	    ((MAC_HFR1 >> 27) & MAC_HFR1_L3L4FILTERNUM_MASK);

	pdata->hw_feat.rx_q_cnt = ((MAC_HFR2 >> 0) & MAC_HFR2_RXQCNT_MASK);
	pdata->hw_feat.tx_q_cnt = ((MAC_HFR2 >> 6) & MAC_HFR2_TXQCNT_MASK);
	pdata->hw_feat.rx_ch_cnt =
	    ((MAC_HFR2 >> 12) & MAC_HFR2_RXCHCNT_MASK);
	pdata->hw_feat.tx_ch_cnt =
	    ((MAC_HFR2 >> 18) & MAC_HFR2_TXCHCNT_MASK);
	pdata->hw_feat.pps_out_num =
	    ((MAC_HFR2 >> 24) & MAC_HFR2_PPSOUTNUM_MASK);
	pdata->hw_feat.aux_snap_num =
	    ((MAC_HFR2 >> 28) & MAC_HFR2_AUXSNAPNUM_MASK);
}

/* \brief API to print all hw features.
 *
 * \details This function is used to print all the device feature.
 *
 * \param[in] pdata - pointer to driver private structure
 *
 * \return none
 */
void print_all_hw_features(struct prv_data *pdata)
{
	char *str = NULL;

	pr_err("\n");
	pr_err("=====================================================/\n");
	pr_err("\n");
	pr_err("10/100 Mbps Support                         : %s\n",
	       pdata->hw_feat.mii_sel ? "YES" : "NO");
	pr_err("1000 Mbps Support                           : %s\n",
	       pdata->hw_feat.gmii_sel ? "YES" : "NO");
	pr_err("Half-duplex Support                         : %s\n",
	       pdata->hw_feat.hd_sel ? "YES" : "NO");
	pr_err("PCS Registers(TBI/SGMII/RTBI PHY interface) : %s\n",
	       pdata->hw_feat.pcs_sel ? "YES" : "NO");
	pr_err("VLAN Hash Filter Selected                   : %s\n",
	       pdata->hw_feat.vlan_hash_en ? "YES" : "NO");
	pdata->vlan_hash_filtering = pdata->hw_feat.vlan_hash_en;
	pr_err("SMA (MDIO) Interface                        : %s\n",
	       pdata->hw_feat.sma_sel ? "YES" : "NO");
	pr_err("PMT Remote Wake-up Packet Enable            : %s\n",
	       pdata->hw_feat.rwk_sel ? "YES" : "NO");
	pr_err("PMT Magic Packet Enable                     : %s\n",
	       pdata->hw_feat.mgk_sel ? "YES" : "NO");
	pr_err("RMON/MMC Module Enable                      : %s\n",
	       pdata->hw_feat.mmc_sel ? "YES" : "NO");
	pr_err("ARP Offload Enabled                         : %s\n",
	       pdata->hw_feat.arp_offld_en ? "YES" : "NO");
	pr_err("IEEE 1588-2008 Timestamp Enabled            : %s\n",
	       pdata->hw_feat.ts_sel ? "YES" : "NO");
	pr_err("Energy Efficient Ethernet Enabled           : %s\n",
	       pdata->hw_feat.eee_sel ? "YES" : "NO");
	pr_err("Transmit Checksum Offload Enabled           : %s\n",
	       pdata->hw_feat.tx_coe_sel ? "YES" : "NO");
	pr_err("Receive Checksum Offload Enabled            : %s\n",
	       pdata->hw_feat.rx_coe_sel ? "YES" : "NO");
	pr_err("MAC Addresses 16–31 Selected                : %s\n",
	       pdata->hw_feat.mac_addr16_sel ? "YES" : "NO");
	pr_err("MAC Addresses 32–63 Selected                : %s\n",
	       pdata->hw_feat.mac_addr32_sel ? "YES" : "NO");
	pr_err("MAC Addresses 64–127 Selected               : %s\n",
	       pdata->hw_feat.mac_addr64_sel ? "YES" : "NO");

	if (pdata->hw_feat.mac_addr64_sel)
		pdata->max_addr_reg_cnt = 128;
	else if (pdata->hw_feat.mac_addr32_sel)
		pdata->max_addr_reg_cnt = 64;
	else if (pdata->hw_feat.mac_addr16_sel)
		pdata->max_addr_reg_cnt = 32;
	else
		pdata->max_addr_reg_cnt = 1;

	switch (pdata->hw_feat.tsstssel) {
	case 0:
		str = "RESERVED";
		break;
	case 1:
		str = "INTERNAL";
		break;
	case 2:
		str = "EXTERNAL";
		break;
	case 3:
		str = "BOTH";
		break;
	}
	pr_err("Timestamp System Time Source                : %s\n",
	       str);
	pr_err("Source Address or VLAN Insertion Enable     : %s\n",
	       pdata->hw_feat.sa_vlan_ins ? "YES" : "NO");

	switch (pdata->hw_feat.act_phy_sel) {
	case 0:
		str = "GMII/MII";
		break;
	case 1:
		str = "RGMII";
		break;
	case 2:
		str = "SGMII";
		break;
	case 3:
		str = "TBI";
		break;
	case 4:
		str = "RMII";
		break;
	case 5:
		str = "RTBI";
		break;
	case 6:
		str = "SMII";
		break;
	case 7:
		str = "RevMII";
		break;
	default:
		str = "RESERVED";
	}
	pr_err("Active PHY Selected                         : %s\n",
	       str);

	switch (pdata->hw_feat.rx_fifo_size) {
	case 0:
		str = "128 bytes";
		break;
	case 1:
		str = "256 bytes";
		break;
	case 2:
		str = "512 bytes";
		break;
	case 3:
		str = "1 KBytes";
		break;
	case 4:
		str = "2 KBytes";
		break;
	case 5:
		str = "4 KBytes";
		break;
	case 6:
		str = "8 KBytes";
		break;
	case 7:
		str = "16 KBytes";
		break;
	case 8:
		str = "32 kBytes";
		break;
	case 9:
		str = "64 KBytes";
		break;
	case 10:
		str = "128 KBytes";
		break;
	case 11:
		str = "256 KBytes";
		break;
	default:
		str = "RESERVED";
	}
	pr_err("MTL Receive FIFO Size                       : %s\n",
	       str);

	switch (pdata->hw_feat.tx_fifo_size) {
	case 0:
		str = "128 bytes";
		break;
	case 1:
		str = "256 bytes";
		break;
	case 2:
		str = "512 bytes";
		break;
	case 3:
		str = "1 KBytes";
		break;
	case 4:
		str = "2 KBytes";
		break;
	case 5:
		str = "4 KBytes";
		break;
	case 6:
		str = "8 KBytes";
		break;
	case 7:
		str = "16 KBytes";
		break;
	case 8:
		str = "32 kBytes";
		break;
	case 9:
		str = "64 KBytes";
		break;
	case 10:
		str = "128 KBytes";
		break;
	case 11:
		str = "256 KBytes";
		break;
	default:
		str = "RESERVED";
	}
	pr_err("MTL Transmit FIFO Size                       : %s\n",
	       str);
	pr_err("IEEE 1588 High Word Register Enable          : %s\n",
	       pdata->hw_feat.adv_ts_hword ? "YES" : "NO");
	pr_err("DCB Feature Enable                           : %s\n",
	       pdata->hw_feat.dcb_en ? "YES" : "NO");
	pr_err("Split Header Feature Enable                  : %s\n",
	       pdata->hw_feat.sph_en ? "YES" : "NO");
	pr_err("TCP Segmentation Offload Enable              : %s\n",
	       pdata->hw_feat.tso_en ? "YES" : "NO");
	pr_err("DMA Debug Registers Enabled                  : %s\n",
	       pdata->hw_feat.dma_debug_gen ? "YES" : "NO");
	pr_err("AV Feature Enabled                           : %s\n",
	       pdata->hw_feat.av_sel ? "YES" : "NO");
	pr_err("Low Power Mode Enabled                       : %s\n",
	       pdata->hw_feat.lp_mode_en ? "YES" : "NO");

	switch (pdata->hw_feat.hash_tbl_sz) {
	case 0:
		str = "No hash table selected";
		pdata->max_hash_table_size = 0;
		break;
	case 1:
		str = "64";
		pdata->max_hash_table_size = 64;
		break;
	case 2:
		str = "128";
		pdata->max_hash_table_size = 128;
		break;
	case 3:
		str = "256";
		pdata->max_hash_table_size = 256;
		break;
	}
	pr_err("Hash Table Size                              : %s\n",
	       str);
	pr_err("Total number of L3 or L4 Filters             : %d L3/L4 Filter\n",
	       pdata->hw_feat.l3l4_filter_num);
	pr_err("Number of MTL Receive Queues                 : %d\n",
	       (pdata->hw_feat.rx_q_cnt + 1));
	pr_err("Number of MTL Transmit Queues                : %d\n",
	       (pdata->hw_feat.tx_q_cnt + 1));
	pr_err("Number of DMA Receive Channels               : %d\n",
	       (pdata->hw_feat.rx_ch_cnt + 1));
	pr_err("Number of DMA Transmit Channels              : %d\n",
	       (pdata->hw_feat.tx_ch_cnt + 1));

	switch (pdata->hw_feat.pps_out_num) {
	case 0:
		str = "No PPS output";
		break;
	case 1:
		str = "1 PPS output";
		break;
	case 2:
		str = "2 PPS output";
		break;
	case 3:
		str = "3 PPS output";
		break;
	case 4:
		str = "4 PPS output";
		break;
	default:
		str = "RESERVED";
	}
	pr_err("Number of PPS Outputs                        : %s\n", str);

	switch (pdata->hw_feat.aux_snap_num) {
	case 0:
		str = "No auxiliary input";
		break;
	case 1:
		str = "1 auxiliary input";
		break;
	case 2:
		str = "2 auxiliary input";
		break;
	case 3:
		str = "3 auxiliary input";
		break;
	case 4:
		str = "4 auxiliary input";
		break;
	default:
		str = "RESERVED";
	}
	pr_err("Number of Auxiliary Snapshot Inputs          : %s", str);

	pr_err("\n");
	pr_err("=====================================================/\n");
}
