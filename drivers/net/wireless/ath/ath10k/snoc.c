/* Copyright (c) 2005-2011 Atheros Communications Inc.
 * Copyright (c) 2011-2013 Qualcomm Atheros, Inc.
 * Copyright (c) 2017, The Linux Foundation. All rights reserved.
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

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/interrupt.h>
#include <linux/spinlock.h>
#include <linux/bitops.h>

#include "core.h"
#include "debug.h"
#include "hif.h"
#include "htc.h"
#include "ce.h"
#include "snoc.h"
#include <soc/qcom/icnss.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#define WCN3990_MAX_IRQ 12
const char *ce_name[WCN3990_MAX_IRQ] = {
	"WLAN_CE_0",
	"WLAN_CE_1",
	"WLAN_CE_2",
	"WLAN_CE_3",
	"WLAN_CE_4",
	"WLAN_CE_5",
	"WLAN_CE_6",
	"WLAN_CE_7",
	"WLAN_CE_8",
	"WLAN_CE_9",
	"WLAN_CE_10",
	"WLAN_CE_11",
};

#define ATH10K_SNOC_TARGET_WAIT 3000
#define ATH10K_SNOC_NUM_WARM_RESET_ATTEMPTS 3
#define SNOC_HIF_POWER_DOWN_DELAY 30

static void ath10k_snoc_buffer_cleanup(struct ath10k *ar);
static int ath10k_snoc_request_irq(struct ath10k *ar);
static void ath10k_snoc_free_irq(struct ath10k *ar);
static void ath10k_snoc_htc_tx_cb(struct ath10k_ce_pipe *ce_state);
static void ath10k_snoc_htc_rx_cb(struct ath10k_ce_pipe *ce_state);
static void ath10k_snoc_htt_tx_cb(struct ath10k_ce_pipe *ce_state);
static void ath10k_snoc_htt_rx_cb(struct ath10k_ce_pipe *ce_state);
static void ath10k_snoc_htt_htc_rx_cb(struct ath10k_ce_pipe *ce_state);

static struct ce_attr host_ce_config_wlan[] = {
	/* CE0: host->target HTC control streams */
	{
		.flags = CE_ATTR_FLAGS,
		.src_nentries = 16,
		.src_sz_max = 2048,
		.dest_nentries = 0,
		.send_cb = ath10k_snoc_htc_tx_cb,
	},

	/* CE1: target->host HTT + HTC control */
	{
		.flags = CE_ATTR_FLAGS,
		.src_nentries = 0,
		.src_sz_max = 2048,
		.dest_nentries = 512,
		.recv_cb = ath10k_snoc_htt_htc_rx_cb,
	},

	/* CE2: target->host WMI */
	{
		.flags = CE_ATTR_FLAGS,
		.src_nentries = 0,
		.src_sz_max = 2048,
		.dest_nentries = 64,
		.recv_cb = ath10k_snoc_htc_rx_cb,
	},

	/* CE3: host->target WMI */
	{
		.flags = CE_ATTR_FLAGS,
		.src_nentries = 32,
		.src_sz_max = 2048,
		.dest_nentries = 0,
		.send_cb = ath10k_snoc_htc_tx_cb,
	},

	/* CE4: host->target HTT */
	{
		.flags = CE_ATTR_FLAGS | CE_ATTR_DIS_INTR,
		.src_nentries = 256,
		.src_sz_max = 256,
		.dest_nentries = 0,
		.send_cb = ath10k_snoc_htt_tx_cb,
	},

	/* CE5: target->host HTT (ipa_uc->target ) */
	{
		.flags = CE_ATTR_FLAGS,
		.src_nentries = 0,
		.src_sz_max = 512,
		.dest_nentries = 512,
		.recv_cb = ath10k_snoc_htt_rx_cb,
	},

	/* CE6: target autonomous hif_memcpy */
	{
		.flags = CE_ATTR_FLAGS,
		.src_nentries = 0,
		.src_sz_max = 0,
		.dest_nentries = 0,
	},

	/* CE7: ce_diag, the Diagnostic Window */
	{
		.flags = CE_ATTR_FLAGS,
		.src_nentries = 2,
		.src_sz_max = 2048,
		.dest_nentries = 2,
	},

	/* CE8: Target to uMC */
	{
		.flags = CE_ATTR_FLAGS,
		.src_nentries = 0,
		.src_sz_max = 2048,
		.dest_nentries = 128,
	},

	/* CE9 target->host HTT */
	{
		.flags = CE_ATTR_FLAGS,
		.src_nentries = 0,
		.src_sz_max = 2048,
		.dest_nentries = 512,
		.recv_cb = ath10k_snoc_htt_htc_rx_cb,
	},

	/* CE10: target->host HTT */
	{
		.flags = CE_ATTR_FLAGS,
		.src_nentries = 0,
		.src_sz_max = 2048,
		.dest_nentries = 512,
		.recv_cb = ath10k_snoc_htt_htc_rx_cb,
	},

	/* CE11: target -> host PKTLOG */
	{
		.flags = CE_ATTR_FLAGS,
		.src_nentries = 0,
		.src_sz_max = 2048,
		.dest_nentries = 512,
		.recv_cb = ath10k_snoc_htt_htc_rx_cb,
	},
};

static struct ce_pipe_config target_ce_config_wlan[] = {
	/* CE0: host->target HTC control and raw streams */
	{
		.pipenum = __cpu_to_le32(0),
		.pipedir = __cpu_to_le32(PIPEDIR_OUT),
		.nentries = __cpu_to_le32(32),
		.nbytes_max = __cpu_to_le32(2048),
		.flags = __cpu_to_le32(CE_ATTR_FLAGS),
		.reserved = __cpu_to_le32(0),
	},

	/* CE1: target->host HTT + HTC control */
	{
		.pipenum = __cpu_to_le32(1),
		.pipedir = __cpu_to_le32(PIPEDIR_IN),
		.nentries = __cpu_to_le32(32),
		.nbytes_max = __cpu_to_le32(2048),
		.flags = __cpu_to_le32(CE_ATTR_FLAGS),
		.reserved = __cpu_to_le32(0),
	},

	/* CE2: target->host WMI */
	{
		.pipenum = __cpu_to_le32(2),
		.pipedir = __cpu_to_le32(PIPEDIR_IN),
		.nentries = __cpu_to_le32(64),
		.nbytes_max = __cpu_to_le32(2048),
		.flags = __cpu_to_le32(CE_ATTR_FLAGS),
		.reserved = __cpu_to_le32(0),
	},

	/* CE3: host->target WMI */
	{
		.pipenum = __cpu_to_le32(3),
		.pipedir = __cpu_to_le32(PIPEDIR_OUT),
		.nentries = __cpu_to_le32(32),
		.nbytes_max = __cpu_to_le32(2048),
		.flags = __cpu_to_le32(CE_ATTR_FLAGS),
		.reserved = __cpu_to_le32(0),
	},

	/* CE4: host->target HTT */
	{
		.pipenum = __cpu_to_le32(4),
		.pipedir = __cpu_to_le32(PIPEDIR_OUT),
		.nentries = __cpu_to_le32(256),
		.nbytes_max = __cpu_to_le32(256),
		.flags = __cpu_to_le32(CE_ATTR_FLAGS | CE_ATTR_DIS_INTR),
		.reserved = __cpu_to_le32(0),
	},

	/* CE5: target->host HTT (HIF->HTT) */
	{
		.pipenum = __cpu_to_le32(5),
		.pipedir = __cpu_to_le32(PIPEDIR_OUT),
		.nentries = __cpu_to_le32(1024),
		.nbytes_max = __cpu_to_le32(64),
		.flags = __cpu_to_le32(CE_ATTR_FLAGS | CE_ATTR_DIS_INTR),
		.reserved = __cpu_to_le32(0),
	},

	/* CE6: Reserved for target autonomous hif_memcpy */
	{
		.pipenum = __cpu_to_le32(6),
		.pipedir = __cpu_to_le32(PIPEDIR_INOUT),
		.nentries = __cpu_to_le32(32),
		.nbytes_max = __cpu_to_le32(16384),
		.flags = __cpu_to_le32(CE_ATTR_FLAGS),
		.reserved = __cpu_to_le32(0),
	},

	/* CE7 used only by Host */
	{
		.pipenum = __cpu_to_le32(7),
		.pipedir = __cpu_to_le32(4),
		.nentries = __cpu_to_le32(0),
		.nbytes_max = __cpu_to_le32(0),
		.flags = __cpu_to_le32(CE_ATTR_FLAGS | CE_ATTR_DIS_INTR),
		.reserved = __cpu_to_le32(0),
	},

	/* CE8 Target to uMC */
	{
		.pipenum = __cpu_to_le32(8),
		.pipedir = __cpu_to_le32(PIPEDIR_IN),
		.nentries = __cpu_to_le32(32),
		.nbytes_max = __cpu_to_le32(2048),
		.flags = __cpu_to_le32(0),
		.reserved = __cpu_to_le32(0),
	},

	/* CE9 target->host HTT */
	{
		.pipenum = __cpu_to_le32(9),
		.pipedir = __cpu_to_le32(PIPEDIR_IN),
		.nentries = __cpu_to_le32(32),
		.nbytes_max = __cpu_to_le32(2048),
		.flags = __cpu_to_le32(CE_ATTR_FLAGS),
		.reserved = __cpu_to_le32(0),
	},

	/* CE10 target->host HTT */
	{
		.pipenum = __cpu_to_le32(10),
		.pipedir = __cpu_to_le32(PIPEDIR_IN),
		.nentries = __cpu_to_le32(32),
		.nbytes_max = __cpu_to_le32(2048),
		.flags = __cpu_to_le32(CE_ATTR_FLAGS),
		.reserved = __cpu_to_le32(0),
	},

	/* CE11 target autonomous qcache memcpy */
	{
		.pipenum = __cpu_to_le32(11),
		.pipedir = __cpu_to_le32(PIPEDIR_IN),
		.nentries = __cpu_to_le32(32),
		.nbytes_max = __cpu_to_le32(2048),
		.flags = __cpu_to_le32(CE_ATTR_FLAGS),
		.reserved = __cpu_to_le32(0),
	},
};

static struct service_to_pipe target_service_to_ce_map_wlan[] = {
	{
		__cpu_to_le32(ATH10K_HTC_SVC_ID_WMI_DATA_VO),
		__cpu_to_le32(PIPEDIR_OUT),	/* out = UL = host -> target */
		__cpu_to_le32(3),
	},
	{
		__cpu_to_le32(ATH10K_HTC_SVC_ID_WMI_DATA_VO),
		__cpu_to_le32(PIPEDIR_IN),	/* in = DL = target -> host */
		__cpu_to_le32(2),
	},
	{
		__cpu_to_le32(ATH10K_HTC_SVC_ID_WMI_DATA_BK),
		__cpu_to_le32(PIPEDIR_OUT),	/* out = UL = host -> target */
		__cpu_to_le32(3),
	},
	{
		__cpu_to_le32(ATH10K_HTC_SVC_ID_WMI_DATA_BK),
		__cpu_to_le32(PIPEDIR_IN),	/* in = DL = target -> host */
		__cpu_to_le32(2),
	},
	{
		__cpu_to_le32(ATH10K_HTC_SVC_ID_WMI_DATA_BE),
		__cpu_to_le32(PIPEDIR_OUT),	/* out = UL = host -> target */
		__cpu_to_le32(3),
	},
	{
		__cpu_to_le32(ATH10K_HTC_SVC_ID_WMI_DATA_BE),
		__cpu_to_le32(PIPEDIR_IN),	/* in = DL = target -> host */
		__cpu_to_le32(2),
	},
	{
		__cpu_to_le32(ATH10K_HTC_SVC_ID_WMI_DATA_VI),
		__cpu_to_le32(PIPEDIR_OUT),	/* out = UL = host -> target */
		__cpu_to_le32(3),
	},
	{
		__cpu_to_le32(ATH10K_HTC_SVC_ID_WMI_DATA_VI),
		__cpu_to_le32(PIPEDIR_IN),	/* in = DL = target -> host */
		__cpu_to_le32(2),
	},
	{
		__cpu_to_le32(ATH10K_HTC_SVC_ID_WMI_CONTROL),
		__cpu_to_le32(PIPEDIR_OUT),	/* out = UL = host -> target */
		__cpu_to_le32(3),
	},
	{
		__cpu_to_le32(ATH10K_HTC_SVC_ID_WMI_CONTROL),
		__cpu_to_le32(PIPEDIR_IN),	/* in = DL = target -> host */
		__cpu_to_le32(2),
	},
	{
		__cpu_to_le32(ATH10K_HTC_SVC_ID_RSVD_CTRL),
		__cpu_to_le32(PIPEDIR_OUT),	/* out = UL = host -> target */
		__cpu_to_le32(0),
	},
	{
		__cpu_to_le32(ATH10K_HTC_SVC_ID_RSVD_CTRL),
		__cpu_to_le32(PIPEDIR_IN),	/* in = DL = target -> host */
		__cpu_to_le32(2),
	},
	{ /* not used */
		__cpu_to_le32(ATH10K_HTC_SVC_ID_TEST_RAW_STREAMS),
		__cpu_to_le32(PIPEDIR_OUT),	/* out = UL = host -> target */
		__cpu_to_le32(0),
	},
	{ /* not used */
		__cpu_to_le32(ATH10K_HTC_SVC_ID_TEST_RAW_STREAMS),
		__cpu_to_le32(PIPEDIR_IN),	/* in = DL = target -> host */
		__cpu_to_le32(2),
	},
	{
		__cpu_to_le32(ATH10K_HTC_SVC_ID_HTT_DATA_MSG),
		__cpu_to_le32(PIPEDIR_OUT),	/* out = UL = host -> target */
		__cpu_to_le32(4),
	},
	{
		__cpu_to_le32(ATH10K_HTC_SVC_ID_HTT_DATA_MSG),
		__cpu_to_le32(PIPEDIR_IN),	/* in = DL = target -> host */
		__cpu_to_le32(1),
	},
	{
		__cpu_to_le32(ATH10K_HTC_SVC_ID_HTT_IPA_MSG),
		__cpu_to_le32(PIPEDIR_OUT),/* IPA service */
		__cpu_to_le32(5),
	},
	{ /* in = DL = target -> host */
		__cpu_to_le32(ATH10K_HTC_SVC_ID_HTT_DATA2_MSG),
		__cpu_to_le32(PIPEDIR_IN),	/* in = DL = target -> host */
		__cpu_to_le32(9),
	},
	{ /* in = DL = target -> host */
		__cpu_to_le32(ATH10K_HTC_SVC_ID_HTT_DATA3_MSG),
		__cpu_to_le32(PIPEDIR_IN),	/* in = DL = target -> host */
		__cpu_to_le32(10),
	},
	{ /* in = DL = target -> host pktlog */
		__cpu_to_le32(ATH10K_HTC_SVC_ID_HTT_LOG_MSG),
		__cpu_to_le32(PIPEDIR_IN),	/* in = DL = target -> host */
		__cpu_to_le32(11),
	},
	/* (Additions here) */

	{ /* must be last */
		__cpu_to_le32(0),
		__cpu_to_le32(0),
		__cpu_to_le32(0),
	},
};

#define WCN3990_SRC_WR_INDEX_OFFSET 0x3C
#define WCN3990_DST_WR_INDEX_OFFSET 0x40

static struct ath10k_shadow_reg_cfg target_shadow_reg_cfg_map[] = {
		{ 0, WCN3990_SRC_WR_INDEX_OFFSET},
		{ 3, WCN3990_SRC_WR_INDEX_OFFSET},
		{ 4, WCN3990_SRC_WR_INDEX_OFFSET},
		{ 5, WCN3990_SRC_WR_INDEX_OFFSET},
		{ 7, WCN3990_SRC_WR_INDEX_OFFSET},
		{ 1, WCN3990_DST_WR_INDEX_OFFSET},
		{ 2, WCN3990_DST_WR_INDEX_OFFSET},
		{ 7, WCN3990_DST_WR_INDEX_OFFSET},
		{ 8, WCN3990_DST_WR_INDEX_OFFSET},
		{ 9, WCN3990_DST_WR_INDEX_OFFSET},
		{ 10, WCN3990_DST_WR_INDEX_OFFSET},
		{ 11, WCN3990_DST_WR_INDEX_OFFSET},
};

void ath10k_snoc_write32(struct ath10k *ar, u32 offset, u32 value)
{
	struct ath10k_snoc *ar_snoc = ath10k_snoc_priv(ar);

	if (!ar_snoc)
		return;

	iowrite32(value, ar_snoc->mem + offset);
}

u32 ath10k_snoc_read32(struct ath10k *ar, u32 offset)
{
	struct ath10k_snoc *ar_snoc = ath10k_snoc_priv(ar);
	u32 val;

	if (!ar_snoc)
		return -EINVAL;

	val = ioread32(ar_snoc->mem + offset);

	return val;
}

static int __ath10k_snoc_rx_post_buf(struct ath10k_snoc_pipe *pipe)
{
	struct ath10k *ar = pipe->hif_ce_state;
	struct ath10k_snoc *ar_snoc = ath10k_snoc_priv(ar);
	struct ath10k_ce_pipe *ce_pipe = pipe->ce_hdl;
	struct sk_buff *skb;
	dma_addr_t paddr;
	int ret;

	skb = dev_alloc_skb(pipe->buf_sz);
	if (!skb)
		return -ENOMEM;

	WARN_ONCE((unsigned long)skb->data & 3, "unaligned skb");

	paddr = dma_map_single(ar->dev, skb->data,
			       skb->len + skb_tailroom(skb),
			       DMA_FROM_DEVICE);
	if (unlikely(dma_mapping_error(ar->dev, paddr))) {
		ath10k_warn(ar, "failed to dma map snoc rx buf\n");
		dev_kfree_skb_any(skb);
		return -EIO;
	}

	ATH10K_SKB_RXCB(skb)->paddr = paddr;

	spin_lock_bh(&ar_snoc->opaque_ctx.ce_lock);
	ret = __ath10k_ce_rx_post_buf(ce_pipe, skb, paddr);
	spin_unlock_bh(&ar_snoc->opaque_ctx.ce_lock);
	if (ret) {
		dma_unmap_single(ar->dev, paddr, skb->len + skb_tailroom(skb),
				 DMA_FROM_DEVICE);
		dev_kfree_skb_any(skb);
		return ret;
	}

	return 0;
}

static void ath10k_snoc_rx_post_pipe(struct ath10k_snoc_pipe *pipe)
{
	struct ath10k *ar = pipe->hif_ce_state;
	struct ath10k_snoc *ar_snoc = ath10k_snoc_priv(ar);
	struct ath10k_ce_pipe *ce_pipe = pipe->ce_hdl;
	int ret, num;

	if (pipe->buf_sz == 0)
		return;

	if (!ce_pipe->dest_ring)
		return;

	spin_lock_bh(&ar_snoc->opaque_ctx.ce_lock);
	num = __ath10k_ce_rx_num_free_bufs(ce_pipe);
	spin_unlock_bh(&ar_snoc->opaque_ctx.ce_lock);
	while (num--) {
		ret = __ath10k_snoc_rx_post_buf(pipe);
		if (ret) {
			if (ret == -ENOSPC)
				break;
			ath10k_warn(ar, "failed to post rx buf: %d\n", ret);
			mod_timer(&ar_snoc->rx_post_retry, jiffies +
				  ATH10K_SNOC_RX_POST_RETRY_MS);
			break;
		}
	}
}

static void ath10k_snoc_rx_post(struct ath10k *ar)
{
	struct ath10k_snoc *ar_snoc = ath10k_snoc_priv(ar);
	int i;

	for (i = 0; i < CE_COUNT; i++)
		ath10k_snoc_rx_post_pipe(&ar_snoc->pipe_info[i]);
}

static void ath10k_snoc_rx_replenish_retry(unsigned long ptr)
{
	struct ath10k *ar = (void *)ptr;
	ath10k_snoc_rx_post(ar);
}

static void ath10k_snoc_htc_tx_cb(struct ath10k_ce_pipe *ce_state)
{
	struct ath10k *ar = ce_state->ar;
	struct sk_buff_head list;
	struct sk_buff *skb;

	__skb_queue_head_init(&list);
	while (ath10k_ce_completed_send_next(ce_state, (void **)&skb) == 0) {
		if (!skb)
			continue;

		__skb_queue_tail(&list, skb);
	}

	while ((skb = __skb_dequeue(&list)))
		ath10k_htc_tx_completion_handler(ar, skb);
}

static void ath10k_snoc_process_rx_cb(struct ath10k_ce_pipe *ce_state,
				      void (*callback)(struct ath10k *ar,
						       struct sk_buff *skb))
{
	struct ath10k *ar = ce_state->ar;
	struct ath10k_snoc *ar_snoc = ath10k_snoc_priv(ar);
	struct ath10k_snoc_pipe *pipe_info =  &ar_snoc->pipe_info[ce_state->id];
	struct sk_buff *skb;
	struct sk_buff_head list;
	void *transfer_context;
	unsigned int nbytes, max_nbytes;

	__skb_queue_head_init(&list);
	while (ath10k_ce_completed_recv_next(ce_state, &transfer_context,
					     &nbytes) == 0) {
		skb = transfer_context;
		max_nbytes = skb->len + skb_tailroom(skb);
		dma_unmap_single(ar->dev, ATH10K_SKB_RXCB(skb)->paddr,
				 max_nbytes, DMA_FROM_DEVICE);

		if (unlikely(max_nbytes < nbytes)) {
			ath10k_warn(ar, "rxed more than expected (nbytes %d, max %d)",
				    nbytes, max_nbytes);
			dev_kfree_skb_any(skb);
			continue;
		}

		skb_put(skb, nbytes);
		__skb_queue_tail(&list, skb);
	}

	while ((skb = __skb_dequeue(&list))) {
		ath10k_dbg(ar, ATH10K_DBG_SNOC, "snoc rx ce pipe %d len %d\n",
			   ce_state->id, skb->len);

		callback(ar, skb);
	}

	ath10k_snoc_rx_post_pipe(pipe_info);
}

static void ath10k_snoc_htc_rx_cb(struct ath10k_ce_pipe *ce_state)
{
	ath10k_snoc_process_rx_cb(ce_state, ath10k_htc_rx_completion_handler);
}

static void ath10k_snoc_htt_htc_rx_cb(struct ath10k_ce_pipe *ce_state)
{
	/* CE4 polling needs to be done whenever CE pipe which transports
	 * HTT Rx (target->host) is processed.
	 */
	ath10k_ce_per_engine_service(ce_state->ar, CE_POLL_PIPE);

	ath10k_snoc_process_rx_cb(ce_state, ath10k_htc_rx_completion_handler);
}

static void ath10k_snoc_htt_tx_cb(struct ath10k_ce_pipe *ce_state)
{
	struct ath10k *ar = ce_state->ar;
	struct sk_buff *skb;

	while (ath10k_ce_completed_send_next(ce_state, (void **)&skb) == 0) {
		if (!skb)
			continue;

		dma_unmap_single(ar->dev, ATH10K_SKB_CB(skb)->paddr,
				 skb->len, DMA_TO_DEVICE);
		ath10k_htt_hif_tx_complete(ar, skb);
	}
}

static void ath10k_snoc_htt_rx_deliver(struct ath10k *ar, struct sk_buff *skb)
{
	skb_pull(skb, sizeof(struct ath10k_htc_hdr));
	ath10k_htt_t2h_msg_handler(ar, skb);
}

static void ath10k_snoc_htt_rx_cb(struct ath10k_ce_pipe *ce_state)
{
	ath10k_ce_per_engine_service(ce_state->ar, CE_POLL_PIPE);
	ath10k_snoc_process_rx_cb(ce_state, ath10k_snoc_htt_rx_deliver);
}

static int ath10k_snoc_hif_tx_sg(struct ath10k *ar, u8 pipe_id,
				 struct ath10k_hif_sg_item *items, int n_items)
{
	struct ath10k_snoc *ar_snoc = ath10k_snoc_priv(ar);
	struct ath10k_snoc_pipe *snoc_pipe;
	struct ath10k_ce_pipe *ce_pipe;
	struct ath10k_ce_ring *src_ring;
	unsigned int nentries_mask;
	unsigned int sw_index;
	unsigned int write_index;
	int err, i = 0;

	if (!ar_snoc)
		return  -EINVAL;

	snoc_pipe = &ar_snoc->pipe_info[pipe_id];
	ce_pipe = snoc_pipe->ce_hdl;
	src_ring = ce_pipe->src_ring;
	spin_lock_bh(&ar_snoc->opaque_ctx.ce_lock);

	nentries_mask = src_ring->nentries_mask;
	sw_index = src_ring->sw_index;
	write_index = src_ring->write_index;

	if (unlikely(CE_RING_DELTA(nentries_mask,
				   write_index, sw_index - 1) < n_items)) {
		err = -ENOBUFS;
		goto err;
	}

	for (i = 0; i < n_items - 1; i++) {
		ath10k_dbg(ar, ATH10K_DBG_SNOC,
			   "snoc tx item %d paddr %pad len %d n_items %d\n",
			   i, &items[i].paddr, items[i].len, n_items);

		err = ath10k_ce_send_nolock(ce_pipe,
					    items[i].transfer_context,
					    items[i].paddr,
					    items[i].len,
					    items[i].transfer_id,
					    CE_SEND_FLAG_GATHER);
		if (err)
			goto err;
	}

	ath10k_dbg(ar, ATH10K_DBG_SNOC,
		   "snoc tx item %d paddr %pad len %d n_items %d\n",
		   i, &items[i].paddr, items[i].len, n_items);

	err = ath10k_ce_send_nolock(ce_pipe,
				    items[i].transfer_context,
				    items[i].paddr,
				    items[i].len,
				    items[i].transfer_id,
				    0);
	if (err)
		goto err;

	spin_unlock_bh(&ar_snoc->opaque_ctx.ce_lock);
	return 0;

err:
	for (; i > 0; i--)
		__ath10k_ce_send_revert(ce_pipe);

	spin_unlock_bh(&ar_snoc->opaque_ctx.ce_lock);
	return err;
}

static u16 ath10k_snoc_hif_get_free_queue_number(struct ath10k *ar, u8 pipe)
{
	struct ath10k_snoc *ar_snoc = ath10k_snoc_priv(ar);

	ath10k_dbg(ar, ATH10K_DBG_SNOC, "hif get free queue number\n");

	return ath10k_ce_num_free_src_entries(ar_snoc->pipe_info[pipe].ce_hdl);
}

static void ath10k_snoc_hif_send_complete_check(struct ath10k *ar, u8 pipe,
						int force)
{
	int resources;

	ath10k_dbg(ar, ATH10K_DBG_SNOC, "snoc hif send complete check\n");

	if (!force) {
		resources = ath10k_snoc_hif_get_free_queue_number(ar, pipe);

		if (resources > (host_ce_config_wlan[pipe].src_nentries >> 1))
			return;
	}
	ath10k_ce_per_engine_service(ar, pipe);
}

static int ath10k_snoc_hif_map_service_to_pipe(struct ath10k *ar,
					       u16 service_id,
					       u8 *ul_pipe, u8 *dl_pipe)
{
	const struct service_to_pipe *entry;
	bool ul_set = false, dl_set = false;
	int i;

	ath10k_dbg(ar, ATH10K_DBG_SNOC, "snoc hif map service\n");

	for (i = 0; i < ARRAY_SIZE(target_service_to_ce_map_wlan); i++) {
		entry = &target_service_to_ce_map_wlan[i];

		if (__le32_to_cpu(entry->service_id) != service_id)
			continue;

		switch (__le32_to_cpu(entry->pipedir)) {
		case PIPEDIR_NONE:
			break;
		case PIPEDIR_IN:
			WARN_ON(dl_set);
			*dl_pipe = __le32_to_cpu(entry->pipenum);
			dl_set = true;
			break;
		case PIPEDIR_OUT:
			WARN_ON(ul_set);
			*ul_pipe = __le32_to_cpu(entry->pipenum);
			ul_set = true;
			break;
		case PIPEDIR_INOUT:
			WARN_ON(dl_set);
			WARN_ON(ul_set);
			*dl_pipe = __le32_to_cpu(entry->pipenum);
			*ul_pipe = __le32_to_cpu(entry->pipenum);
			dl_set = true;
			ul_set = true;
			break;
		}
	}

	if (WARN_ON(!ul_set || !dl_set))
		return -ENOENT;

	return 0;
}

static void ath10k_snoc_hif_get_default_pipe(struct ath10k *ar,
					     u8 *ul_pipe, u8 *dl_pipe)
{
	ath10k_dbg(ar, ATH10K_DBG_SNOC, "snoc hif get default pipe\n");

	(void)ath10k_snoc_hif_map_service_to_pipe(ar,
						 ATH10K_HTC_SVC_ID_RSVD_CTRL,
						 ul_pipe, dl_pipe);
}

static void ath10k_snoc_irq_disable(struct ath10k *ar)
{
	ath10k_ce_disable_interrupts(ar);
}

static void ath10k_snoc_irq_enable(struct ath10k *ar)
{
	ath10k_ce_enable_interrupts(ar);
}

static void ath10k_snoc_rx_pipe_cleanup(struct ath10k_snoc_pipe *snoc_pipe)
{
	struct ath10k *ar;
	struct ath10k_ce_pipe *ce_pipe;
	struct ath10k_ce_ring *ce_ring;
	struct sk_buff *skb;
	int i;

	ar = snoc_pipe->hif_ce_state;
	ce_pipe = snoc_pipe->ce_hdl;
	ce_ring = ce_pipe->dest_ring;

	if (!ce_ring)
		return;

	if (!snoc_pipe->buf_sz)
		return;

	for (i = 0; i < ce_ring->nentries; i++) {
		skb = ce_ring->per_transfer_context[i];
		if (!skb)
			continue;

		ce_ring->per_transfer_context[i] = NULL;

		dma_unmap_single(ar->dev, ATH10K_SKB_RXCB(skb)->paddr,
				 skb->len + skb_tailroom(skb),
				 DMA_FROM_DEVICE);
		dev_kfree_skb_any(skb);
	}
}

static void ath10k_snoc_tx_pipe_cleanup(struct ath10k_snoc_pipe *snoc_pipe)
{
	struct ath10k *ar;
	struct ath10k_snoc *ar_snoc;
	struct ath10k_ce_pipe *ce_pipe;
	struct ath10k_ce_ring *ce_ring;
	struct sk_buff *skb;
	int i;

	ar = snoc_pipe->hif_ce_state;
	ar_snoc = ath10k_snoc_priv(ar);
	ce_pipe = snoc_pipe->ce_hdl;
	ce_ring = ce_pipe->src_ring;

	if (!ce_ring)
		return;

	if (!snoc_pipe->buf_sz)
		return;

	for (i = 0; i < ce_ring->nentries; i++) {
		skb = ce_ring->per_transfer_context[i];
		if (!skb)
			continue;

		ce_ring->per_transfer_context[i] = NULL;

		ath10k_htc_tx_completion_handler(ar, skb);
	}
}

static void ath10k_snoc_buffer_cleanup(struct ath10k *ar)
{
	struct ath10k_snoc *ar_snoc = ath10k_snoc_priv(ar);
	int pipe_num;

	del_timer_sync(&ar_snoc->rx_post_retry);
	for (pipe_num = 0; pipe_num < CE_COUNT; pipe_num++) {
		struct ath10k_snoc_pipe *pipe_info;

		pipe_info = &ar_snoc->pipe_info[pipe_num];
		ath10k_snoc_rx_pipe_cleanup(pipe_info);
		ath10k_snoc_tx_pipe_cleanup(pipe_info);
	}
}

static void ath10k_snoc_flush(struct ath10k *ar)
{
	ath10k_snoc_buffer_cleanup(ar);
}

static void ath10k_snoc_hif_stop(struct ath10k *ar)
{
	if (!ar)
		return;
	ath10k_dbg(ar, ATH10K_DBG_BOOT, "boot hif stop\n");
	ath10k_snoc_irq_disable(ar);
	ath10k_snoc_flush(ar);
	napi_synchronize(&ar->napi);
	napi_disable(&ar->napi);
}

static int ath10k_snoc_alloc_pipes(struct ath10k *ar)
{
	struct ath10k_snoc *ar_snoc = ath10k_snoc_priv(ar);
	struct ath10k_snoc_pipe *pipe;
	int i, ret;

	for (i = 0; i < CE_COUNT; i++) {
		pipe = &ar_snoc->pipe_info[i];
		pipe->ce_hdl = &ar_snoc->opaque_ctx.ce_states[i];
		pipe->pipe_num = i;
		pipe->hif_ce_state = ar;

		ret = ath10k_ce_alloc_pipe(ar, i, &host_ce_config_wlan[i]);
		if (ret) {
			ath10k_err(ar, "failed to allocate copy engine pipe %d: %d\n",
				   i, ret);
			return ret;
		}

		pipe->buf_sz = (size_t)(host_ce_config_wlan[i].src_sz_max);
	}

	return 0;
}

static void ath10k_snoc_free_pipes(struct ath10k *ar)
{
	int i;

	for (i = 0; i < CE_COUNT; i++)
		ath10k_ce_free_pipe(ar, i);
}

static void ath10k_snoc_release_resource(struct ath10k *ar)
{
	netif_napi_del(&ar->napi);
	ath10k_snoc_free_pipes(ar);
}

static int ath10k_snoc_init_pipes(struct ath10k *ar)
{
	int i, ret;

	for (i = 0; i < CE_COUNT; i++) {
		ret = ath10k_ce_init_pipe(ar, i, &host_ce_config_wlan[i]);
		if (ret) {
			ath10k_err(ar, "failed to initialize copy engine pipe %d: %d\n",
				   i, ret);
			return ret;
		}
	}

	return 0;
}

static void ath10k_snoc_hif_power_down(struct ath10k *ar)
{
	ath10k_dbg(ar, ATH10K_DBG_BOOT, "boot hif power down\n");
	msleep(SNOC_HIF_POWER_DOWN_DELAY);
	icnss_wlan_disable(ICNSS_OFF);
}

int ath10k_snoc_get_ce_id(struct ath10k *ar, int irq)
{
	int i;
	struct ath10k_snoc *ar_snoc = ath10k_snoc_priv(ar);

	for (i = 0; i < CE_COUNT_MAX; i++) {
		if (ar_snoc->ce_irqs[i] == irq)
		return i;
	}
	ath10k_err(ar, "No matching CE id for irq %d\n", irq);

	return -EINVAL;
}

static irqreturn_t ath10k_snoc_per_engine_handler(int irq, void *arg)
{
	struct ath10k *ar = arg;
	struct ath10k_snoc *ar_snoc = ath10k_snoc_priv(ar);
	int ce_id = ath10k_snoc_get_ce_id(ar, irq);

	if (ce_id < 0 || ce_id >= ARRAY_SIZE(ar_snoc->pipe_info)) {
		ath10k_warn(ar, "unexpected/invalid irq %d ce_id %d\n", irq,
			    ce_id);
		return IRQ_HANDLED;
	}

	ath10k_snoc_irq_disable(ar);
	napi_schedule(&ar->napi);

	return IRQ_HANDLED;
}

static int ath10k_snoc_request_irq(struct ath10k *ar)
{
	int ret, id;
	struct ath10k_snoc *ar_snoc = ath10k_snoc_priv(ar);
	int irqflags = IRQF_TRIGGER_RISING;

	for (id = 0; id < CE_COUNT_MAX; id++) {
		ret = request_irq(ar_snoc->ce_irqs[id],
				  ath10k_snoc_per_engine_handler,
				  irqflags, ce_name[id], ar);
		if (ret) {
			ath10k_err(ar,
				   "%s: cannot register CE %d irq handler, ret = %d",
				   __func__, id, ret);
			free_irq(ar_snoc->ce_irqs[id], ar);
			return ret;
		}
	}

	return 0;
}

static void ath10k_snoc_free_irq(struct ath10k *ar)
{
	int id;
	struct ath10k_snoc *ar_snoc = ath10k_snoc_priv(ar);

	for (id = 0; id < CE_COUNT_MAX; id++)
		free_irq(ar_snoc->ce_irqs[id], ar);
}


static int ath10k_snoc_get_soc_info(struct ath10k *ar)
{
	int ret;
	struct icnss_soc_info soc_info;
	struct ath10k_snoc *ar_snoc = ath10k_snoc_priv(ar);

	memset(&soc_info, 0, sizeof(soc_info));

	ret = icnss_get_soc_info(&soc_info);
	if (ret < 0) {
		ath10k_err(ar, "%s: icnss_get_soc_info error = %d",
			   __func__, ret);
		return ret;
	}

	ar_snoc->mem = soc_info.v_addr;
	ar_snoc->mem_pa = soc_info.p_addr;

	ar_snoc->target_info.soc_version = soc_info.soc_id;
	ar_snoc->target_info.target_version = soc_info.soc_id;
	ar_snoc->target_info.target_revision = 0;

	ath10k_dbg(ar, ATH10K_DBG_SNOC,
		   "%s: mem = %pS mem_pa = %pad soc ver=%x tgt ver=%x\n",
		   __func__, ar_snoc->mem, &ar_snoc->mem_pa,
		   ar_snoc->target_info.soc_version,
		   ar_snoc->target_info.target_version);

	return 0;
}

static int ath10k_snoc_wlan_enable(struct ath10k *ar)
{
	struct icnss_wlan_enable_cfg cfg;
	int pipe_num, i;
	struct ath10k_ce_tgt_pipe_cfg tgt_cfg[CE_COUNT_MAX];
	struct ce_tgt_pipe_cfg *tmp_tgt_cfg;
	struct ce_svc_pipe_cfg *tmp_svc_cfg;

	for (pipe_num = 0; pipe_num < CE_COUNT_MAX; pipe_num++) {
		tgt_cfg[pipe_num].pipe_num =
				target_ce_config_wlan[pipe_num].pipenum;
		tgt_cfg[pipe_num].pipe_dir =
				target_ce_config_wlan[pipe_num].pipedir;
		tgt_cfg[pipe_num].nentries =
				target_ce_config_wlan[pipe_num].nentries;
		tgt_cfg[pipe_num].nbytes_max =
				target_ce_config_wlan[pipe_num].nbytes_max;
		tgt_cfg[pipe_num].flags =
				target_ce_config_wlan[pipe_num].flags;
		tgt_cfg[pipe_num].reserved = 0;
	}

	cfg.num_ce_tgt_cfg = sizeof(target_ce_config_wlan) /
				sizeof(struct ce_tgt_pipe_cfg);
	cfg.ce_tgt_cfg = (struct ce_tgt_pipe_cfg *)
		&tgt_cfg;
	cfg.num_ce_svc_pipe_cfg = sizeof(target_service_to_ce_map_wlan) /
				  sizeof(struct ce_svc_pipe_cfg);
	cfg.ce_svc_cfg = (struct ce_svc_pipe_cfg *)
		&target_service_to_ce_map_wlan;
	cfg.num_shadow_reg_cfg = sizeof(target_shadow_reg_cfg_map) /
					sizeof(struct icnss_shadow_reg_cfg);
	cfg.shadow_reg_cfg = (struct icnss_shadow_reg_cfg *)
		&target_shadow_reg_cfg_map;

	for (i = 0; i < cfg.num_ce_tgt_cfg; i++)
		tmp_tgt_cfg = cfg.ce_tgt_cfg + i;

	for (i = 0; i < cfg.num_ce_svc_pipe_cfg; i++)
		tmp_svc_cfg = cfg.ce_svc_cfg + i;

	return icnss_wlan_enable(&cfg, ICNSS_MISSION, "5.1.0.26N");
}

static int ath10k_snoc_bus_configure(struct ath10k *ar)
{
	int ret;

	ret = ath10k_snoc_wlan_enable(ar);
	if (ret < 0) {
		ath10k_err(ar, "%s: ath10k_snoc_bus_configure error = %d",
			   __func__, ret);
		return ret;
	}

	return 0;
}

static int ath10k_snoc_hif_start(struct ath10k *ar)
{
	ath10k_dbg(ar, ATH10K_DBG_BOOT, "boot hif start\n");
	ath10k_snoc_irq_enable(ar);
	ath10k_snoc_rx_post(ar);
	return 0;
}

static int ath10k_snoc_claim(struct ath10k *ar)
{
	struct ath10k_snoc *ar_snoc = ath10k_snoc_priv(ar);

	ath10k_snoc_get_soc_info(ar);
	ath10k_dbg(ar, ATH10K_DBG_BOOT, "boot snoc_mem 0x%p\n", ar_snoc->mem);

	return 0;
}

static int ath10k_snoc_hif_power_up(struct ath10k *ar)
{
	int ret;

	ath10k_dbg(ar, ATH10K_DBG_SNOC, "%s:WCN3990 driver state = %d\n",
		   __func__, ar->state);

	if (ar->state == ATH10K_STATE_ON) {
		ret = ath10k_snoc_bus_configure(ar);
		if (ret)
			ath10k_err(ar, "failed to configure bus: %d\n", ret);
	}
	ret = ath10k_snoc_init_pipes(ar);
	if (ret) {
		ath10k_err(ar, "failed to initialize CE: %d\n", ret);
		goto err_sleep;
	}

	napi_enable(&ar->napi);
	return 0;

err_sleep:
	return ret;
}

static int ath10k_snoc_napi_poll(struct napi_struct *ctx, int budget)
{
	struct ath10k *ar = container_of(ctx, struct ath10k, napi);
	int done = 0;

	ath10k_ce_per_engine_service_any(ar);

	done = ath10k_htt_txrx_compl_task(ar, budget);

	if (done < budget) {
		napi_complete(ctx);
		ath10k_snoc_irq_enable(ar);
	}

	return done;
}

static int ath10k_snoc_resource_init(struct ath10k *ar)
{
	int i, ret = 0;
	struct resource *res;
	struct ath10k_snoc *ar_snoc = ath10k_snoc_priv(ar);

	for (i = 0; i < CE_COUNT; i++) {
		res = platform_get_resource(ar_snoc->dev, IORESOURCE_IRQ, i);
		if (!res) {
			ath10k_err(ar, "Fail to get IRQ-%d\n", i);
			ret = -ENODEV;
			goto out;
		} else {
			ar_snoc->ce_irqs[i] = res->start;
		}
	}

out:
	return ret;
}

static const struct ath10k_hif_ops ath10k_snoc_hif_ops = {
	.tx_sg			= ath10k_snoc_hif_tx_sg,
	.start			= ath10k_snoc_hif_start,
	.stop			= ath10k_snoc_hif_stop,
	.map_service_to_pipe	= ath10k_snoc_hif_map_service_to_pipe,
	.get_default_pipe	= ath10k_snoc_hif_get_default_pipe,
	.send_complete_check	= ath10k_snoc_hif_send_complete_check,
	.get_free_queue_number	= ath10k_snoc_hif_get_free_queue_number,
	.power_up		= ath10k_snoc_hif_power_up,
	.power_down		= ath10k_snoc_hif_power_down,
	.read32			= ath10k_snoc_read32,
	.write32		= ath10k_snoc_write32,
};

static const struct ath10k_bus_ops ath10k_snoc_bus_ops = {
	.read32		= ath10k_snoc_read32,
	.write32	= ath10k_snoc_write32,
};

static int ath10k_snoc_probe(struct platform_device *pdev)
{
	int ret;
	struct ath10k *ar;
	struct ath10k_snoc *ar_snoc;
	enum ath10k_hw_rev hw_rev;
	struct device *dev;
	u32 chip_id;

	dev = &pdev->dev;
	hw_rev = ATH10K_HW_WCN3990;
	ret = dma_set_mask_and_coherent(dev, DMA_BIT_MASK(37));
	ar = ath10k_core_create(sizeof(*ar_snoc), dev, ATH10K_BUS_SNOC,
				hw_rev, &ath10k_snoc_hif_ops);
	if (!ar) {
		dev_err(dev, "failed to allocate core\n");
		return -ENOMEM;
	}

	ar_snoc = ath10k_snoc_priv(ar);
	if (!ar_snoc)
		return  -EINVAL;
	ar_snoc->dev = pdev;
	platform_set_drvdata(pdev, ar);
	ar_snoc->ar = ar;

	spin_lock_init(&ar_snoc->opaque_ctx.ce_lock);
	ar_snoc->opaque_ctx.bus_ops = &ath10k_snoc_bus_ops;
	ath10k_snoc_resource_init(ar);

	ar->target_version = ATH10K_HW_WCN3990;
	ar->hw->wiphy->hw_version = ATH10K_HW_WCN3990;
	setup_timer(&ar_snoc->rx_post_retry, ath10k_snoc_rx_replenish_retry,
		    (unsigned long)ar);

	ret = ath10k_snoc_claim(ar);
	if (ret) {
		ath10k_err(ar, "failed to claim device: %d\n", ret);
		goto err_core_destroy;
	}
	ret = ath10k_snoc_bus_configure(ar);
	if (ret) {
		ath10k_err(ar, "failed to configure bus: %d\n", ret);
		goto err_core_destroy;
	}

	ret = ath10k_snoc_alloc_pipes(ar);
	if (ret) {
		ath10k_err(ar, "failed to allocate copy engine pipes: %d\n",
			   ret);
		goto err_core_destroy;
	}

	netif_napi_add(&ar->napi_dev, &ar->napi, ath10k_snoc_napi_poll,
		       ATH10K_NAPI_BUDGET);

	ret = ath10k_snoc_request_irq(ar);
	if (ret) {
		ath10k_warn(ar, "failed to request irqs: %d\n", ret);
		goto err_free_pipes;
	}

	chip_id = ar_snoc->target_info.soc_version;
	/* chip id needs to be retrieved from platform driver */
	ret = ath10k_core_register(ar, chip_id);
	if (ret) {
		ath10k_err(ar, "failed to register driver core: %d\n", ret);
		goto err_free_irq;
	}
	ath10k_dbg(ar, ATH10K_DBG_SNOC, "%s:WCN3990 probed\n", __func__);

	return 0;

err_free_irq:
	ath10k_snoc_free_irq(ar);

err_free_pipes:
	ath10k_snoc_free_pipes(ar);

err_core_destroy:
	ath10k_core_destroy(ar);

	return ret;
}

static int ath10k_snoc_remove(struct platform_device *pdev)
{
	struct ath10k *ar = platform_get_drvdata(pdev);
	struct ath10k_snoc *ar_snoc = ath10k_snoc_priv(ar);

	if (!ar)
		return -EINVAL;

	if (!ar_snoc)
		return -EINVAL;

	ath10k_core_unregister(ar);
	ath10k_snoc_free_irq(ar);
	ath10k_snoc_release_resource(ar);
	ath10k_snoc_free_pipes(ar);
	ath10k_core_destroy(ar);

	ath10k_dbg(ar, ATH10K_DBG_SNOC, "%s:WCN3990 removed\n", __func__);

	return 0;
}

static const struct of_device_id ath10k_snoc_dt_match[] = {
	{.compatible = "qcom,wcn3990-wifi"},
	{}
};
MODULE_DEVICE_TABLE(of, ath10k_snoc_dt_match);

static struct platform_driver ath10k_snoc_driver = {
		.probe  = ath10k_snoc_probe,
		.remove = ath10k_snoc_remove,
		.driver = {
			.name   = "ath10k_snoc",
			.owner = THIS_MODULE,
			.of_match_table = ath10k_snoc_dt_match,
		},
};

static int __init ath10k_snoc_init(void)
{
	int ret;

	if (!icnss_is_fw_ready()) {
		pr_err("failed to get fw ready indication\n");
		return -EAGAIN;
	}
	ret = platform_driver_register(&ath10k_snoc_driver);
	if (ret)
		pr_err("failed to register ath10k snoc driver: %d\n",
		       ret);

	return ret;
}
module_init(ath10k_snoc_init);

static void __exit ath10k_snoc_exit(void)
{
	platform_driver_unregister(&ath10k_snoc_driver);
}
module_exit(ath10k_snoc_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Driver support for Atheros WCN3990 SNOC devices");
