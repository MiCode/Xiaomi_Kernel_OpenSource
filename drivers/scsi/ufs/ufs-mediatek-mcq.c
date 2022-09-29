// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2022 MediaTek Inc.
 * Authors:
 *	Eddie Huang <eddie.huang@mediatek.com>
 */

#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/tracepoint.h>

#include "ufshcd.h"
#include "ufshcd-crypto.h"
#include "ufs-mediatek.h"

#include <trace/hooks/ufshcd.h>

enum {
	/* one for empty, one for devman command*/
	UFSHCD_MCQ_NUM_RESERVED = 2,
};

enum {
	UFSHCD_UIC_DL_PA_INIT_ERROR = (1 << 0),
};

#define MTK_MCQ_INVALID_IRQ	0xFFFF

#define ufs_mtk_mcq_hex_dump(prefix_str, buf, len) do {                       \
	size_t __len = (len);                                            \
	print_hex_dump(KERN_ERR, prefix_str,                             \
		       __len > 4 ? DUMP_PREFIX_OFFSET : DUMP_PREFIX_NONE,\
		       16, 4, buf, __len, false);                        \
} while (0)

static unsigned int mtk_mcq_irq[UFSHCD_MAX_Q_NR];

static void ufs_mtk_mcq_map_tag(void *data, struct ufs_hba *hba, int index, int *tag);

static void ufs_mtk_mcq_print_trs_tag(struct ufs_hba *hba, int tag, bool pr_prdt)
{
	struct ufshcd_lrb *lrbp;
	int prdt_length;

	lrbp = &hba->lrb[tag];

	dev_err(hba->dev, "UPIU[%d] - issue time %lld us\n",
			tag, ktime_to_us(lrbp->issue_time_stamp));
	dev_err(hba->dev, "UPIU[%d] - complete time %lld us\n",
			tag, ktime_to_us(lrbp->compl_time_stamp));
	dev_err(hba->dev,
		"UPIU[%d] - Transfer Request Descriptor phys@0x%llx\n",
		tag, (u64)lrbp->utrd_dma_addr);

	ufs_mtk_mcq_hex_dump("UPIU TRD: ", lrbp->utr_descriptor_ptr,
			sizeof(struct utp_transfer_req_desc));
	dev_err(hba->dev, "UPIU[%d] - Request UPIU phys@0x%llx\n", tag,
		(u64)lrbp->ucd_req_dma_addr);
	ufs_mtk_mcq_hex_dump("UPIU REQ: ", lrbp->ucd_req_ptr,
			sizeof(struct utp_upiu_req));
	dev_err(hba->dev, "UPIU[%d] - Response UPIU phys@0x%llx\n", tag,
		(u64)lrbp->ucd_rsp_dma_addr);
	ufs_mtk_mcq_hex_dump("UPIU RSP: ", lrbp->ucd_rsp_ptr,
			sizeof(struct utp_upiu_rsp));

	prdt_length = le16_to_cpu(
		lrbp->utr_descriptor_ptr->prd_table_length);
	if (hba->quirks & UFSHCD_QUIRK_PRDT_BYTE_GRAN)
		prdt_length /= hba->sg_entry_size;

	dev_err(hba->dev,
		"UPIU[%d] - PRDT - %d entries  phys@0x%llx\n",
		tag, prdt_length,
		(u64)lrbp->ucd_prdt_dma_addr);

	if (pr_prdt)
		ufs_mtk_mcq_hex_dump("UPIU PRDT: ", lrbp->ucd_prdt_ptr,
			hba->sg_entry_size * prdt_length);
}

static void ufs_mtk_mcq_print_trs(void *data, struct ufs_hba *hba, bool pr_prdt)
{
	int tag;
	struct ufs_hba_private *hba_priv = (struct ufs_hba_private *)hba->android_vendor_data1;
	unsigned long *bitmap = hba_priv->outstanding_mcq_reqs;

	for_each_set_bit(tag, bitmap, UFSHCD_MAX_TAG) {
		ufs_mtk_mcq_print_trs_tag(hba, tag, pr_prdt);
	}
}

static u32 ufs_mtk_q_entry_offset(struct ufs_queue *q, union utp_q_entry *ptr)
{
	return (ptr - q->q_base_addr);
}

static dma_addr_t ufs_mtk_q_virt_to_dma(struct ufs_queue *q, union utp_q_entry *entry)
{
	unsigned long q_offset;

	if (!q || !entry || entry < q->q_base_addr) {
		//TBD: error message
		return 0;
	}
	q_offset = entry - q->q_base_addr;
	if (q_offset > q->q_depth) {
		//TBD: error message
		return 0;
	}
	return (q_offset * SQE_SIZE);
}

static bool ufs_mtk_is_sq_full(struct ufs_hba *hba, struct ufs_queue *q)
{
	u32 head_offset;
	u32 tail_offset;

	head_offset = ufshcd_readl(hba, MCQ_ADDR(REG_UFS_SQ_HEAD, q->qid)) / SQE_SIZE;
	tail_offset = ufs_mtk_q_entry_offset(q, q->tail);

	if (((tail_offset + 1) % q->q_depth) == head_offset)
		return true;
	else
		return false;
}

static inline void ufs_mtk_write_sq_tail(struct ufs_hba *hba, struct ufs_queue *sq_ptr)
{
	ufshcd_writel(hba, ufs_mtk_q_virt_to_dma(sq_ptr, sq_ptr->tail),
		MCQ_ADDR(REG_UFS_SQ_TAIL, sq_ptr->qid));
	sq_ptr->tail_written = sq_ptr->tail;
}


static bool ufs_mtk_is_cq_empty(struct ufs_hba *hba, struct ufs_queue *q)
{
	unsigned long tail_offset;
	unsigned long head_offset;

	tail_offset = ufshcd_readl(hba, MCQ_ADDR(REG_UFS_CQ_TAIL, q->qid));
	head_offset = ufs_mtk_q_entry_offset(q, q->head) * CQE_SIZE;

	if (head_offset == tail_offset)
		return true;
	else
		return false;
}

static void ufs_mtk_inc_cq_head(struct ufs_hba *hba, struct ufs_queue *q)
{
	u8 head_offset;
	u8 head_next_offset;
	u32 head_next_offset_addr;

	head_offset = ufs_mtk_q_entry_offset(q, q->head);
	head_next_offset = (head_offset+1) % q->q_depth;
	q->head = &q->q_base_addr[head_next_offset];
	head_next_offset_addr = head_next_offset * CQE_SIZE;

	ufshcd_writel(hba, head_next_offset_addr, MCQ_ADDR(REG_UFS_CQ_HEAD, q->qid));
}

static void ufs_mtk_transfer_req_compl_handler(struct ufs_hba *hba,
					bool retry_requests, int index)
{
	struct ufshcd_lrb *lrbp;
	struct scsi_cmnd *cmd;
	int result;
	bool update_scaling = false;

	lrbp = &hba->lrb[index];
	lrbp->compl_time_stamp = ktime_get();
	cmd = lrbp->cmd;
	if (cmd) {
		trace_android_vh_ufs_compl_command(hba, lrbp);
		ufshcd_add_command_trace(hba, index, UFS_CMD_COMP);
		result = retry_requests ? DID_BUS_BUSY << 16 :
			ufshcd_transfer_rsp_status(hba, lrbp);
		scsi_dma_unmap(cmd);
		cmd->result = result;
		ufshcd_crypto_clear_prdt(hba, lrbp);
		/* Mark completed command as NULL in LRB */
		lrbp->cmd = NULL;
		/* Do not touch lrbp after scsi done */
		cmd->scsi_done(cmd);
		ufshcd_release(hba);
		update_scaling = true;
	} else if (lrbp->command_type == UTP_CMD_TYPE_DEV_MANAGE ||
		lrbp->command_type == UTP_CMD_TYPE_UFS_STORAGE) {
		if (hba->dev_cmd.complete) {
			trace_android_vh_ufs_compl_command(hba, lrbp);
			ufshcd_add_command_trace(hba, index,
						 UFS_DEV_COMP);
			complete(hba->dev_cmd.complete);
			update_scaling = true;
		}
	}

	if (update_scaling)
		ufshcd_clk_scaling_update_busy(hba);
}

static irqreturn_t ufs_mtk_mcq_cq_ring_handler(struct ufs_hba *hba, int hwq)
{
	struct ufs_queue *q;
	struct utp_cq_entry	*entry;
	struct ufshcd_lrb *lrb;
	dma_addr_t ucdl_dma_addr;
	u8 tag_offset;
	u32 mcq_cqe_ocs;
	unsigned long flags;
	struct ufs_hba_private *hba_priv = (struct ufs_hba_private *)hba->android_vendor_data1;

	q = &hba_priv->mcq_q_cfg.cq[hwq];
	while (!ufs_mtk_is_cq_empty(hba, q)) {
		entry = &q->head->cq;
		ucdl_dma_addr = entry->UCD_base & UCD_BASE_ADD_MASK;
		tag_offset = (ucdl_dma_addr - hba->ucdl_dma_addr) /
						sizeof_utp_transfer_cmd_desc(hba);
		/* Write OCS value back to UTRD, as legacy controller did */
		lrb = &hba->lrb[tag_offset];
		mcq_cqe_ocs = (le32_to_cpu(entry->dword_4) & UTRD_OCS_MASK);
		lrb->utr_descriptor_ptr->header.dword_2 = cpu_to_le32(mcq_cqe_ocs);
		ufs_mtk_inc_cq_head(hba, q);
		spin_lock_irqsave(&hba->outstanding_lock, flags);
		__clear_bit(tag_offset, hba_priv->outstanding_mcq_reqs);
		spin_unlock_irqrestore(&hba->outstanding_lock, flags);
		ufs_mtk_transfer_req_compl_handler(hba, false, tag_offset);
	}

	return IRQ_HANDLED;
}

static irqreturn_t ufs_mtk_mcq_cq_handler(struct ufs_hba *hba)
{
	int i;
	unsigned long cq_is, flags;
	irqreturn_t ret = IRQ_HANDLED;
	struct ufs_hba_private *hba_priv = (struct ufs_hba_private *)hba->android_vendor_data1;

	spin_lock_irqsave(&hba->outstanding_lock, flags);
	cq_is = ufshcd_readl(hba, REG_UFS_MMIO_CQ_IS);
	ufshcd_writel(hba, cq_is, REG_UFS_MMIO_CQ_IS);
	spin_unlock_irqrestore(&hba->outstanding_lock, flags);

	for (i = 0; i < hba_priv->mcq_nr_hw_queue; i++) {
		if (test_bit(i, &cq_is))
			ret |= ufs_mtk_mcq_cq_ring_handler(hba, i);
	}

	return ret;
}

static irqreturn_t ufs_mtk_mcq_sq_ring_handler(struct ufs_hba *hba, int hwq)
{
	// Do not handle SQ now, TBD
	return IRQ_HANDLED;
}

static irqreturn_t ufs_mtk_mcq_sq_handler(struct ufs_hba *hba)
{
	struct ufs_hba_private *hba_priv = (struct ufs_hba_private *)hba->android_vendor_data1;
	unsigned long sq_is, flags;
	int i;
	irqreturn_t ret = IRQ_NONE;

	spin_lock_irqsave(&hba->outstanding_lock, flags);
	sq_is = ufshcd_readl(hba, REG_UFS_MMIO_SQ_IS);
	ufshcd_writel(hba, sq_is, REG_UFS_MMIO_SQ_IS);
	spin_unlock_irqrestore(&hba->outstanding_lock, flags);

	for (i = 0; i < hba_priv->mcq_nr_hw_queue; i++) {
		if (test_bit(i, &sq_is))
			ret |= ufs_mtk_mcq_sq_ring_handler(hba, i);
	}

	return ret;
}

static irqreturn_t ufs_mtk_mcq_intr(int irq, void *__intr_info)
{
	struct ufs_mcq_intr_info *mcq_intr_info = __intr_info;
	struct ufs_hba *hba = mcq_intr_info->hba;
	struct ufs_hba_private *hba_priv = (struct ufs_hba_private *)hba->android_vendor_data1;
	int hwq;
	irqreturn_t retval = IRQ_NONE;

	if (hba_priv->mcq_nr_intr == 0)
		return IRQ_NONE;

	hwq = mcq_intr_info->qid;

	ufshcd_writel(hba, (1 << hwq), REG_UFS_MMIO_CQ_IS);
	retval = ufs_mtk_mcq_cq_ring_handler(hba, hwq);

	return retval;
}

static void ufs_mtk_legacy_mcq_handler(void *data, struct ufs_hba *hba,
								u32 intr_status, irqreturn_t *retval)
{
	struct ufs_hba_private *hba_priv =
					(struct ufs_hba_private *)hba->android_vendor_data1;

	if (!hba_priv->is_mcq_enabled) {
		return;
	}

	/* If enable multiple CQ interrupt, legacy interrupt should not handle
	 * CQ/SQ interrupt
	 * If enbale CQ interrupt, this function should not be called
	 */
	if (hba_priv->mcq_nr_intr > 0) {
		*retval |= IRQ_HANDLED;
		return;
	}

	if (intr_status & CQ_INT)
		ufs_mtk_mcq_cq_handler(hba);

	if (intr_status & SQ_INT)
		ufs_mtk_mcq_sq_handler(hba);

	*retval |= IRQ_HANDLED;
}

static void ufs_mtk_mcq_send_hw_cmd(struct ufs_hba *hba, unsigned int task_tag)
{
	struct ufs_hba_private *hba_priv = (struct ufs_hba_private *)hba->android_vendor_data1;
	struct ufshcd_lrb *lrbp = &hba->lrb[task_tag];
	int q_index = lrbp->android_vendor_data1;
	struct ufs_queue *sq_ptr = &hba_priv->mcq_q_cfg.sq[q_index];
	unsigned long flags;

	hba_priv->mcq_q_cfg.sent_cmd_count[q_index]++;

	lrbp->issue_time_stamp = ktime_get();
	lrbp->compl_time_stamp = ktime_set(0, 0);

	trace_android_vh_ufs_send_command(hba, lrbp);
	ufshcd_add_command_trace(hba, task_tag, UFS_CMD_SEND);
	ufshcd_clk_scaling_start_busy(hba);

	spin_lock_irqsave(&sq_ptr->q_lock, flags);
	if (hba->vops && hba->vops->setup_xfer_req)
		hba->vops->setup_xfer_req(hba, task_tag, (lrbp->cmd ? true : false));
	//SQ full happens when CQ speed <<< SQ speed,
	if (ufs_mtk_is_sq_full(hba, sq_ptr)) {
		dev_err(hba->dev, "qid: %d FULL", sq_ptr->qid);
		goto finalize;
	}

	__set_bit(task_tag, hba_priv->outstanding_mcq_reqs);

	/* Copy the modified UTRD to the corresponding SQ entry */
	memcpy(sq_ptr->tail, lrbp->utr_descriptor_ptr, SQE_SIZE);
	if (ufs_mtk_q_entry_offset(sq_ptr, ++sq_ptr->tail) == sq_ptr->q_depth)
		sq_ptr->tail = sq_ptr->q_base_addr;

	/* Make sure SQE copy finish */
	wmb();

	ufs_mtk_write_sq_tail(hba, sq_ptr);

	/* Make sure sq tail update */
	wmb();

finalize:
	spin_unlock_irqrestore(&sq_ptr->q_lock, flags);
}

static void ufs_mtk_mcq_send_command(void *data, struct ufs_hba *hba, unsigned int task_tag)
{
	ufs_mtk_mcq_send_hw_cmd(hba, task_tag);
}

static void ufs_mtk_mcq_config_queue(struct ufs_hba *hba, u8 qid,
			u8 enable, u8 q_type, u16 q_depth, u8 priority, u8 mapping)
{
	struct ufs_hba_private *hba_priv = (struct ufs_hba_private *)hba->android_vendor_data1;
	struct ufs_queue_config *mcq_q_cfg = &hba_priv->mcq_q_cfg;
	struct ufs_queue *queue;

	if (q_type == MCQ_Q_TYPE_SQ) {
		queue = &mcq_q_cfg->sq[qid];
		mcq_q_cfg->sq_cq_map[qid] = mapping;
	} else {
		queue = &mcq_q_cfg->cq[qid];
	}

	//set depth and priority to 0, if Q is not enabled
	queue->qid = qid;
	queue->q_enable = enable;
	queue->q_type = q_type;
	queue->q_depth = (enable ? q_depth : 0);
	queue->priority = (enable ? priority : 0);

	dev_info(hba->dev, "q_type: %d, qid: %d, enable: %d, q_depth: %d, priority: %d, mapping: %d\n",
		queue->q_type, queue->qid, queue->q_enable, queue->q_depth, queue->priority,
		mcq_q_cfg->sq_cq_map[qid]);
}

static int ufs_mtk_mcq_init_queue(struct ufs_hba *hba)
{
	struct ufs_hba_private *hba_priv = (struct ufs_hba_private *)hba->android_vendor_data1;
	struct ufs_queue_config *mcq_q_cfg = &hba_priv->mcq_q_cfg;
	int i;
	u8 sq_nr, cq_nr, q_depth, q_priority = 0;
	u32 cnt;
	struct ufs_queue *queue;

	// queue depth should be the same for every SQ, blk_mq_init_queue
	q_depth = hba_priv->mcq_nr_q_depth;
	sq_nr = cq_nr = hba_priv->mcq_nr_hw_queue;
	for (i = 0; i < sq_nr; i++) {
		ufs_mtk_mcq_config_queue(hba, i, true, MCQ_Q_TYPE_SQ, q_depth, q_priority, i);
		ufs_mtk_mcq_config_queue(hba, i, true, MCQ_Q_TYPE_CQ, q_depth, q_priority, i);

		mcq_q_cfg->sent_cmd_count[i] = 0;
	}

	// check if tag is enough
	for (i = 0, cnt = 0; i < sq_nr; i++) {
		queue = &mcq_q_cfg->sq[i];
		cnt += queue->q_depth;
		if (cnt > UFSHCD_MAX_TAG)
			goto err;
	}

	for (i = 0, cnt = 0; i < cq_nr; i++) {
		queue = &mcq_q_cfg->cq[i];
		cnt += queue->q_depth;
		if (cnt > UFSHCD_MAX_TAG)
			goto err;
	}

	mcq_q_cfg->sq_nr = sq_nr;
	mcq_q_cfg->cq_nr = cq_nr;
	dev_info(hba->dev, "SQ NR = %d\n", mcq_q_cfg->sq_nr);
	dev_info(hba->dev, "CQ NR = %d\n", mcq_q_cfg->cq_nr);

	return 0;

err:
	dev_err(hba->dev, "MCQ configuration not applied. ONLY %d tag is allowed", UFSHCD_MAX_TAG);
	return -EINVAL;
}

static void ufs_mtk_mcq_host_memory_configure(struct ufs_hba *hba)
{
	int i;
	struct ufs_hba_private *hba_priv = (struct ufs_hba_private *)hba->android_vendor_data1;
	u8 q_size = hba_priv->mcq_nr_hw_queue;
	u16 sq_offset = 0;
	u16 sq_offset_1k = 0;
	u16 cq_offset = 0;
	u16 cq_offset_1k = 0;
	struct ufs_queue *sq_ptr;
	struct ufs_queue *cq_ptr;


	for (i = 0; i < q_size; i++) {
		sq_ptr = &hba_priv->mcq_q_cfg.sq[i];

		sq_ptr->q_base_addr = (void *)hba_priv->usel_base_addr + sq_offset_1k * SQE_SIZE;
		sq_ptr->q_dma_addr = hba_priv->usel_dma_addr + sq_offset_1k * SQE_SIZE;

		sq_offset += sq_ptr->q_depth;
		sq_offset_1k += DIV_ROUND_UP(sq_ptr->q_depth, SQE_NUM_1K) * SQE_NUM_1K; //for 1K alignment constraint
		spin_lock_init(&(sq_ptr->q_lock));

		dev_info(hba->dev, "SQ[%d], depth: %d, sq_offset: %d, sq_offset_1k: %d",
			i, sq_ptr->q_depth, sq_offset, sq_offset_1k);
	}

	for (i = 0; i < q_size; i++) {
		cq_ptr = &hba_priv->mcq_q_cfg.cq[i];

		cq_ptr->q_base_addr = (void *)hba_priv->ucel_base_addr + cq_offset_1k * CQE_SIZE;
		cq_ptr->q_dma_addr = hba_priv->ucel_dma_addr + cq_offset_1k * CQE_SIZE;

		cq_offset += cq_ptr->q_depth;
		cq_offset_1k += DIV_ROUND_UP(cq_ptr->q_depth, CQE_NUM_1K) * CQE_NUM_1K; //for 1K alignment constraint
		spin_lock_init(&(cq_ptr->q_lock));

		dev_info(hba->dev, "CQ[%d], depth: %d, cq_offset: %d, cq_offset_1k: %d",
			i, cq_ptr->q_depth, cq_offset, cq_offset_1k);
	}
}

static inline void ufs_mtk_mcq_reset_ptr(struct ufs_queue *q)
{
	q->head = q->tail = q->tail_written = q->q_base_addr;
}

static void ufs_mtk_mcq_enable(struct ufs_hba *hba)
{
	int i;
	u32 cq_ie, sq_ie;
	u32 val;
	u64 addr;
	struct ufs_queue *q;
	struct ufs_hba_private *hba_priv =
				(struct ufs_hba_private *)hba->android_vendor_data1;

	// enable AH8 + MCQ
	ufshcd_rmwl(hba, MCQ_AH8, MCQ_AH8, REG_UFS_MMIO_OPT_CTRL_0);

	// set queue type: MCQ
	ufshcd_rmwl(hba, MCQCFG_TYPE, MCQCFG_TYPE, REG_UFS_MCQCFG);

	// set arbitration scheme, check capability SP & RRP
	ufshcd_rmwl(hba, MCQCFG_ARB_SCHEME, MCQCFG_ARB_RRP, REG_UFS_MCQCFG);

	cq_ie = 0;
	for (i = 0; i < hba_priv->mcq_nr_hw_queue; i++) {
		q = &hba_priv->mcq_q_cfg.cq[i];

		if (q->q_enable) {
			val = (q->q_depth * CQE_SIZE / 4) & 0xffff; //Q_SIZE in unit of DWORD
			val |= Q_ENABLE;
			addr = q->q_dma_addr;

			cq_ie |= (1 << i);
		} else {
			val = 0;
			addr = 0;
		}

		ufshcd_writel(hba, lower_32_bits(addr), MCQ_ADDR(REG_UFS_CQ_LBA, i));
		ufshcd_writel(hba, upper_32_bits(addr), MCQ_ADDR(REG_UFS_CQ_UBA, i));
		ufshcd_writel(hba, val, MCQ_ADDR(REG_UFS_CQ_ATTR, i));

		ufs_mtk_mcq_reset_ptr(q);
	}

	sq_ie = 0;
	for (i = 0; i < hba_priv->mcq_nr_hw_queue; i++) {
		q = &hba_priv->mcq_q_cfg.sq[i];

		if (q->q_enable) {
			val = (q->q_depth * SQE_SIZE / 4) & 0xffff; //Q_SIZE in unit of DWORD
			val |= (hba_priv->mcq_q_cfg.sq_cq_map[i] << 16);
			val |= (q->priority << 28);
			val |= Q_ENABLE;
			addr = q->q_dma_addr;

			sq_ie |= (1 << i);
		} else {
			val = 0;
			addr = 0;
		}

		ufshcd_writel(hba, lower_32_bits(addr), MCQ_ADDR(REG_UFS_SQ_LBA, i));
		ufshcd_writel(hba, upper_32_bits(addr), MCQ_ADDR(REG_UFS_SQ_UBA, i));
		ufshcd_writel(hba, val, MCQ_ADDR(REG_UFS_SQ_ATTR, i));

		ufs_mtk_mcq_reset_ptr(q);
	}

	ufshcd_writel(hba, cq_ie, REG_UFS_MMIO_CQ_IE);

	/* Enable SQ interrupt */
	//ufshcd_writel(hba, sq_ie, REG_UFS_MMIO_SQ_IE);

	if (hba_priv->mcq_nr_intr > 0)
		ufshcd_rmwl(hba, MCQ_INTR_EN_MSK, MCQ_MULTI_INTR_EN, REG_UFS_MMIO_OPT_CTRL_0);

}

static void ufs_mtk_mcq_enable_intr(struct ufs_hba *hba, u32 intrs)
{
	u32 set = ufshcd_readl(hba, REG_INTERRUPT_ENABLE);

	if (hba->ufs_version == ufshci_version(1, 0)) {
		u32 rw;
		rw = set & INTERRUPT_MASK_RW_VER_10;
		set = rw | ((set ^ intrs) & intrs);
	} else {
		set |= intrs;
	}

	ufshcd_writel(hba, set, REG_INTERRUPT_ENABLE);
}

static void ufs_mtk_mcq_make_hba_operational(void *data, struct ufs_hba *hba, int *err)
{
	struct ufs_hba_private *hba_priv = (struct ufs_hba_private *)hba->android_vendor_data1;
	u32 reg;
	*err = 0;

	if (hba_priv->mcq_nr_intr > 0)
		ufs_mtk_mcq_enable_intr(hba, UFSHCD_ENABLE_INTRS_MCQ_SEPARATE);
	else
		ufs_mtk_mcq_enable_intr(hba, UFSHCD_ENABLE_INTRS_MCQ);

	ufs_mtk_mcq_enable(hba);

	/* Configure UTMRL base address registers */
	ufshcd_writel(hba, lower_32_bits(hba->utmrdl_dma_addr),
			REG_UTP_TASK_REQ_LIST_BASE_L);
	ufshcd_writel(hba, upper_32_bits(hba->utmrdl_dma_addr),
			REG_UTP_TASK_REQ_LIST_BASE_H);

	/*
	 * Make sure base address and interrupt setup are updated before
	 * enabling the run/stop registers below.
	 */
	wmb();

	/*
	 * UCRDY, UTMRLDY and UTRLRDY bits must be 1
	 */
	reg = ufshcd_readl(hba, REG_CONTROLLER_STATUS);
	if ((reg & UFSHCD_STATUS_READY) == UFSHCD_STATUS_READY) {
		ufshcd_writel(hba, UTP_TASK_REQ_LIST_RUN_STOP_BIT,
				  REG_UTP_TASK_REQ_LIST_RUN_STOP);
	} else {
		dev_err(hba->dev,
			"Host controller not ready to process requests");
		*err = -EIO;
	}
}

static void ufs_mtk_mcq_use_mcq_hooks(void *data, struct ufs_hba *hba, bool *use_mcq)
{
	struct ufs_hba_private *hba_priv = (struct ufs_hba_private *)hba->android_vendor_data1;

	*use_mcq = hba_priv->is_mcq_enabled;
}

static void ufs_mtk_mcq_hba_capabilities(void *data, struct ufs_hba *hba, int *err)
{
	struct ufs_hba_private *hba_priv = (struct ufs_hba_private *)hba->android_vendor_data1;
	int nr;

	if (!hba_priv->is_mcq_enabled)
		return;

	hba->reserved_slot = UFSHCD_MAX_TAG - 1;

	/* Get MCQ number */
	hba_priv->mcq_cap = ufshcd_readl(hba, REG_UFS_MCQCAP);
	hba_priv->max_q = FIELD_GET(MAX_Q, hba_priv->mcq_cap);

	/* Sanity check */
	if (hba_priv->mcq_nr_hw_queue > hba_priv->max_q) {
		dev_err(hba->dev, "HCI maxq %d is less than %d\n",
				hba_priv->max_q, hba_priv->mcq_nr_hw_queue);
		*err = -EINVAL;
		return;
	}

	if (hba_priv->mcq_nr_hw_queue == 0) {
		dev_err(hba->dev, "Enable MCQ but set hw queue to zero\n");
		*err = -EINVAL;
		return;
	}

	nr = hba_priv->mcq_nr_hw_queue * hba_priv->mcq_nr_q_depth;
	if (nr > UFSHCD_MAX_TAG) {
		dev_err(hba->dev, "MCQ tag %d is larger than %d\n",
					nr, UFSHCD_MAX_TAG);
		*err = -EINVAL;
		return;
	}
}

static void ufs_mtk_mcq_print_evt(struct ufs_hba *hba, u32 id,
			     char *err_name)
{
	int i;
	bool found = false;
	struct ufs_event_hist *e;

	if (id >= UFS_EVT_CNT)
		return;

	e = &hba->ufs_stats.event[id];

	for (i = 0; i < UFS_EVENT_HIST_LENGTH; i++) {
		int p = (i + e->pos) % UFS_EVENT_HIST_LENGTH;

		if (e->tstamp[p] == 0)
			continue;
		dev_err(hba->dev, "%s[%d] = 0x%x at %lld us\n", err_name, p,
			e->val[p], ktime_to_us(e->tstamp[p]));
		found = true;
	}

	if (!found)
		dev_err(hba->dev, "No record of %s\n", err_name);
	else
		dev_err(hba->dev, "%s: total cnt=%llu\n", err_name, e->cnt);
}

static void ufs_mtk_mcq_print_evt_hist(struct ufs_hba *hba)
{
	ufshcd_dump_regs(hba, 0, UFSHCI_REG_SPACE_SIZE, "host_regs: ");

	ufs_mtk_mcq_print_evt(hba, UFS_EVT_PA_ERR, "pa_err");
	ufs_mtk_mcq_print_evt(hba, UFS_EVT_DL_ERR, "dl_err");
	ufs_mtk_mcq_print_evt(hba, UFS_EVT_NL_ERR, "nl_err");
	ufs_mtk_mcq_print_evt(hba, UFS_EVT_TL_ERR, "tl_err");
	ufs_mtk_mcq_print_evt(hba, UFS_EVT_DME_ERR, "dme_err");
	ufs_mtk_mcq_print_evt(hba, UFS_EVT_AUTO_HIBERN8_ERR,
			 "auto_hibern8_err");
	ufs_mtk_mcq_print_evt(hba, UFS_EVT_FATAL_ERR, "fatal_err");
	ufs_mtk_mcq_print_evt(hba, UFS_EVT_LINK_STARTUP_FAIL,
			 "link_startup_fail");
	ufs_mtk_mcq_print_evt(hba, UFS_EVT_RESUME_ERR, "resume_fail");
	ufs_mtk_mcq_print_evt(hba, UFS_EVT_SUSPEND_ERR,
			 "suspend_fail");
	ufs_mtk_mcq_print_evt(hba, UFS_EVT_DEV_RESET, "dev_reset");
	ufs_mtk_mcq_print_evt(hba, UFS_EVT_HOST_RESET, "host_reset");
	ufs_mtk_mcq_print_evt(hba, UFS_EVT_ABORT, "task_abort");

	ufshcd_vops_dbg_register_dump(hba);
}

static void ufs_mtk_mcq_print_clk_freqs(struct ufs_hba *hba)
{
	struct ufs_clk_info *clki;
	struct list_head *head = &hba->clk_list_head;

	if (list_empty(head))
		return;

	list_for_each_entry(clki, head, list) {
		if (!IS_ERR_OR_NULL(clki->clk) && clki->min_freq &&
				clki->max_freq)
			dev_err(hba->dev, "clk: %s, rate: %u\n",
					clki->name, clki->curr_freq);
	}
}

static void ufs_mtk_mcq_print_host_state(struct ufs_hba *hba)
{
	struct scsi_device *sdev_ufs = hba->sdev_ufs_device;

	dev_err(hba->dev, "UFS Host state=%d\n", hba->ufshcd_state);
	dev_err(hba->dev, "outstanding reqs=0x%lx tasks=0x%lx\n",
		hba->outstanding_reqs, hba->outstanding_tasks);
	dev_err(hba->dev, "saved_err=0x%x, saved_uic_err=0x%x\n",
		hba->saved_err, hba->saved_uic_err);
	dev_err(hba->dev, "Device power mode=%d, UIC link state=%d\n",
		hba->curr_dev_pwr_mode, hba->uic_link_state);
	dev_err(hba->dev, "PM in progress=%d, sys. suspended=%d\n",
		hba->pm_op_in_progress, hba->is_sys_suspended);
	dev_err(hba->dev, "Auto BKOPS=%d, Host self-block=%d\n",
		hba->auto_bkops_enabled, hba->host->host_self_blocked);
	dev_err(hba->dev, "Clk gate=%d\n", hba->clk_gating.state);
	dev_err(hba->dev,
		"last_hibern8_exit_tstamp at %lld us, hibern8_exit_cnt=%d\n",
		ktime_to_us(hba->ufs_stats.last_hibern8_exit_tstamp),
		hba->ufs_stats.hibern8_exit_cnt);
	dev_err(hba->dev, "last intr at %lld us, last intr status=0x%x\n",
		ktime_to_us(hba->ufs_stats.last_intr_ts),
		hba->ufs_stats.last_intr_status);
	dev_err(hba->dev, "error handling flags=0x%x, req. abort count=%d\n",
		hba->eh_flags, hba->req_abort_count);
	dev_err(hba->dev, "hba->ufs_version=0x%x, Host capabilities=0x%x, caps=0x%x\n",
		hba->ufs_version, hba->capabilities, hba->caps);
	dev_err(hba->dev, "quirks=0x%x, dev. quirks=0x%x\n", hba->quirks,
		hba->dev_quirks);
	if (sdev_ufs)
		dev_err(hba->dev, "UFS dev info: %.8s %.16s rev %.4s\n",
			sdev_ufs->vendor, sdev_ufs->model, sdev_ufs->rev);

	ufs_mtk_mcq_print_clk_freqs(hba);
}

static void ufs_mtk_mcq_print_pwr_info(struct ufs_hba *hba)
{
	static const char * const names[] = {
		"INVALID MODE",
		"FAST MODE",
		"SLOW_MODE",
		"INVALID MODE",
		"FASTAUTO_MODE",
		"SLOWAUTO_MODE",
		"INVALID MODE",
	};

	/*
	 * Using dev_dbg to avoid messages during runtime PM to avoid
	 * never-ending cycles of messages written back to storage by user space
	 * causing runtime resume, causing more messages and so on.
	 */
	dev_dbg(hba->dev, "%s:[RX, TX]: gear=[%d, %d], lane[%d, %d], pwr[%s, %s], rate = %d\n",
		 __func__,
		 hba->pwr_info.gear_rx, hba->pwr_info.gear_tx,
		 hba->pwr_info.lane_rx, hba->pwr_info.lane_tx,
		 names[hba->pwr_info.pwr_rx],
		 names[hba->pwr_info.pwr_tx],
		 hba->pwr_info.hs_rate);
}

static inline bool ufs_mtk_mcq_is_saved_err_fatal(struct ufs_hba *hba)
{
	return (hba->saved_uic_err & UFSHCD_UIC_DL_PA_INIT_ERROR) ||
	       (hba->saved_err & (INT_FATAL_ERRORS | UFSHCD_UIC_HIBERN8_MASK));
}

static inline void ufs_mtk_mcq_schedule_eh_work(struct ufs_hba *hba)
{
	/* handle fatal errors only when link is not in error state */
	if (hba->ufshcd_state != UFSHCD_STATE_ERROR) {
		if (hba->force_reset || ufshcd_is_link_broken(hba) ||
		    ufs_mtk_mcq_is_saved_err_fatal(hba))
			hba->ufshcd_state = UFSHCD_STATE_EH_SCHEDULED_FATAL;
		else
			hba->ufshcd_state = UFSHCD_STATE_EH_SCHEDULED_NON_FATAL;
		queue_work(hba->eh_wq, &hba->eh_work);
	}
}

static void ufs_mtk_mcq_set_req_abort_skip(struct ufs_hba *hba, unsigned long *bitmap)
{
	struct ufshcd_lrb *lrbp;
	int tag;

	for_each_set_bit(tag, bitmap, UFSHCD_MAX_TAG) {
		lrbp = &hba->lrb[tag];
		lrbp->req_abort_skip = true;
	}
}

static int ufs_mtk_try_to_abort_task(struct ufs_hba *hba, int tag)
{
	/* Not implement MCQ stop and clean command
	 * If implement MCQ stop and clean, need export ufshcd_issue_tm_cmd()
	 * to call UFS_QUERY_TASK and UFS_ABORT_TASK
	 */
	return 0;
}

static void ufs_mtk_mcq_abort(void *data, struct scsi_cmnd *cmd, int *ret)
{
	struct Scsi_Host *host = cmd->device->host;
	struct ufs_hba *hba = shost_priv(host);
	struct ufs_hba_private *hba_priv = (struct ufs_hba_private *)hba->android_vendor_data1;
	int tag = scsi_cmd_to_rq(cmd)->tag;
	struct ufshcd_lrb *lrbp;
	unsigned long flags;
	int err;

	*ret = FAILED;

	ufs_mtk_mcq_map_tag(NULL, hba, scsi_cmd_to_rq(cmd)->mq_hctx->queue_num, &tag);
	lrbp = &hba->lrb[tag];

	ufshcd_hold(hba, false);

	/* If command is already aborted/completed, return FAILED. */
	if (!(test_bit(tag, hba_priv->outstanding_mcq_reqs))) {
		dev_err(hba->dev,
			"%s: cmd at tag %d already completed, outstanding=0x%lx\n",
			__func__, tag, hba->outstanding_reqs);
		goto release;
	}

	/* Print Transfer Request of aborted task */
	dev_info(hba->dev, "%s: Deivce abort at hwq:%d, tag:%d", __func__,
						scsi_cmd_to_rq(cmd)->mq_hctx->queue_num, tag);

	/*
	 * Print detailed info about aborted request.
	 * As more than one request might get aborted at the same time,
	 * print full information only for the first aborted request in order
	 * to reduce repeated printouts. For other aborted requests only print
	 * basic details.
	 */
	scsi_print_command(cmd);
	if (!hba->req_abort_count) {
		ufshcd_update_evt_hist(hba, UFS_EVT_ABORT, tag);
		ufs_mtk_mcq_print_evt_hist(hba);
		ufs_mtk_mcq_print_host_state(hba);
		ufs_mtk_mcq_print_pwr_info(hba);
		ufs_mtk_mcq_print_trs_tag(hba, tag, true);
	} else {
		ufs_mtk_mcq_print_trs_tag(hba, tag, false);
	}
	hba->req_abort_count++;

	/*
	 * Task abort to the device W-LUN is illegal. When this command
	 * will fail, due to spec violation, scsi err handling next step
	 * will be to send LU reset which, again, is a spec violation.
	 * To avoid these unnecessary/illegal steps, first we clean up
	 * the lrb taken by this cmd and re-set it in outstanding_reqs,
	 * then queue the eh_work and bail.
	 */
	if (lrbp->lun == UFS_UPIU_UFS_DEVICE_WLUN) {
		ufshcd_update_evt_hist(hba, UFS_EVT_ABORT, lrbp->lun);

		spin_lock_irqsave(host->host_lock, flags);
		hba->force_reset = true;
		ufs_mtk_mcq_schedule_eh_work(hba);
		spin_unlock_irqrestore(host->host_lock, flags);
		goto release;
	}

	/* Skip task abort in case previous aborts failed and report failure */
	if (lrbp->req_abort_skip) {
		dev_err(hba->dev, "%s: skipping abort\n", __func__);
		ufs_mtk_mcq_set_req_abort_skip(hba, hba_priv->outstanding_mcq_reqs);
		goto release;
	}

	err = ufs_mtk_try_to_abort_task(hba, tag);
	if (err) {
		dev_err(hba->dev, "%s: failed with err %d\n", __func__, err);
		ufs_mtk_mcq_set_req_abort_skip(hba, hba_priv->outstanding_mcq_reqs);
		*ret = FAILED;
		goto release;
	}

	lrbp->cmd = NULL;

	/* No clean SQ */
	/* *ret = SUCCESS; */

release:
	/* Matches the ufshcd_hold() call at the start of this function. */
	ufshcd_release(hba);
	return;
}

static void ufs_mtk_mcq_config(void *data, struct ufs_hba *hba, int *err)
{
	struct Scsi_Host *host = hba->host;
	struct ufs_hba_private *hba_priv = (struct ufs_hba_private *)hba->android_vendor_data1;

	*err = ufs_mtk_mcq_init_queue(hba);
	if (*err)
		return;

	ufs_mtk_mcq_host_memory_configure(hba);

	/* Reserve one depth to judge empty or full */
	host->can_queue = hba_priv->mcq_nr_q_depth - UFSHCD_MCQ_NUM_RESERVED;
	host->cmd_per_lun = hba_priv->mcq_nr_q_depth - UFSHCD_MCQ_NUM_RESERVED;
	host->nr_hw_queues = hba_priv->mcq_nr_hw_queue;
}

static void ufs_mtk_mcq_max_tag(void *data, struct ufs_hba *hba, int *max_tag)
{
	struct ufs_hba_private *hba_priv = (struct ufs_hba_private *)hba->android_vendor_data1;

	if (hba_priv->is_mcq_enabled)
		*max_tag = UFSHCD_MAX_TAG;
	else
		*max_tag = hba->nutrs;
}

static void ufs_mtk_mcq_has_oustanding_reqs(void *data, struct ufs_hba *hba, bool *ret)
{
	struct ufs_hba_private *hba_priv = (struct ufs_hba_private *)hba->android_vendor_data1;
	int i;

	if (!hba_priv->is_mcq_enabled) {
		*ret = hba->outstanding_reqs != 0 ? true : false;
		return;
	}

	for (i = 0; i < BITMAP_TAGS_LEN; i++) {
		if (hba_priv->outstanding_mcq_reqs[i]) {
			*ret = true;
			return;
		}
	}

	*ret = false;
}

static void ufs_mtk_mcq_get_outstanding_reqs(void *data, struct ufs_hba *hba,
								unsigned long **outstanding_reqs, int *nr_tag)
{
	struct ufs_hba_private *hba_priv = (struct ufs_hba_private *)hba->android_vendor_data1;

	if (hba_priv->is_mcq_enabled) {
		*outstanding_reqs = hba_priv->outstanding_mcq_reqs;
		if (nr_tag)
			*nr_tag = UFSHCD_MAX_TAG;
	} else {
		*outstanding_reqs = &hba->outstanding_reqs;
		if (nr_tag)
			*nr_tag = hba->nutrs;
	}
}

static void ufs_mtk_mcq_clear_cmd(void *data, struct ufs_hba *hba, int tag, int *ret)
{
	// Mediatek MCQ not imiplement SQ stop and clean
	*ret = FAILED;
}

static void ufs_mtk_mcq_clear_pending(void *data, struct ufs_hba *hba, int *ret)
{
	// Mediatek MCQ not imiplement SQ stop and clean
	*ret = FAILED;
}

static void ufs_mtk_mcq_map_tag(void *data, struct ufs_hba *hba, int index, int *tag)
{
	struct ufs_hba_private *hba_priv = (struct ufs_hba_private *)hba->android_vendor_data1;
	int oldtag = *tag;

	if (!hba_priv->is_mcq_enabled)
		return;

	*tag = oldtag + index * hba_priv->mcq_nr_q_depth;
}

static void ufs_mtk_mcq_set_sqid(void *data, struct ufs_hba *hba, int index,
								struct ufshcd_lrb *lrbp)
{
	struct ufs_hba_private *hba_priv = (struct ufs_hba_private *)hba->android_vendor_data1;

	if (!hba_priv->is_mcq_enabled)
		return;

	/* Set hwq index to lrb */
	lrbp->android_vendor_data1 = index;
}

int ufs_mtk_mcq_alloc_priv(struct ufs_hba *hba)
{
	struct ufs_hba_private *hba_priv;

	hba_priv = kzalloc(sizeof(struct ufs_hba_private), GFP_KERNEL);
	if (!hba_priv) {
		dev_err(hba->dev, "Allocate ufs private struct fail\n");
		return -1;
	}

	hba->android_vendor_data1 = (u64)hba_priv;

	return 0;
}

void ufs_mtk_mcq_host_dts(struct ufs_hba *hba)
{
	struct ufs_hba_private *hba_priv = (struct ufs_hba_private *)hba->android_vendor_data1;
	struct device_node *np = hba->dev->of_node;
	int i;

	if (of_property_read_bool(np, "mediatek,ufs-mcq-enabled"))
		hba_priv->is_mcq_enabled = true;
	else
		hba_priv->is_mcq_enabled = false;

	if (hba_priv->is_mcq_enabled) {
		hba_priv->mcq_nr_hw_queue = 1;
		of_property_read_u32(np, "mediatek,ufs-mcq-hwq-count", &hba_priv->mcq_nr_hw_queue);

		hba_priv->mcq_nr_q_depth = 32;
		of_property_read_u32(np, "mediatek,ufs-mcq-q-depth", &hba_priv->mcq_nr_q_depth);

		hba_priv->mcq_nr_intr = 0;
		if (of_property_read_u32(np, "mediatek,ufs-mcq-intr-count", &hba_priv->mcq_nr_intr) == 0) {
			if (hba_priv->mcq_nr_intr > 0) {
				for (i = 0; i < hba_priv->mcq_nr_intr; i++) {
					hba_priv->mcq_intr_info[i].qid = i;
					hba_priv->mcq_intr_info[i].intr = mtk_mcq_irq[i];
					hba_priv->mcq_intr_info[i].hba = hba;
					dev_info(hba->dev, "Set MCQ interrupt: %d, %d\n", i,
									hba_priv->mcq_intr_info[i].intr);
				}
			}
		}

		dev_info(hba->dev, "%s, mcq hwq: %d, mcq q-depth: %d, mcq_nr_intr: %d\n",
					__func__, hba_priv->mcq_nr_hw_queue,
					hba_priv->mcq_nr_q_depth, hba_priv->mcq_nr_intr);
	}

	dev_info(hba->dev, "MCQ enabled: %s\n",
						hba_priv->is_mcq_enabled ? "yes" : "no");
}

void ufs_mtk_mcq_get_irq(struct platform_device *pdev)
{
	int i, irq;

	for (i = 0; i < UFSHCD_MAX_Q_NR; i++)
		mtk_mcq_irq[i] = MTK_MCQ_INVALID_IRQ;

	for (i = 0; i < UFSHCD_MAX_Q_NR; i++) {
		/* irq index 0 is ufshcd system irq, sq, cq irq start from index 1 */
		irq = platform_get_irq(pdev, i + 1);
		if (irq < 0) {
			dev_info(&pdev->dev, "Get platform interrupt fail: %d\n", i);
			break;
		}
		mtk_mcq_irq[i] = irq;
		dev_info(&pdev->dev, "Get platform interrupt: %d, %d\n", i, irq);
	}
}

void ufs_mtk_mcq_request_irq(struct ufs_hba *hba)
{
	struct ufs_hba_private *hba_priv = (struct ufs_hba_private *)hba->android_vendor_data1;
	unsigned int irq, i;
	int ret;

	if (!hba_priv->is_mcq_enabled)
		return;

	if (!hba_priv->mcq_nr_intr)
		return;

	// Disable irq option register
	ufshcd_rmwl(hba, MCQ_INTR_EN_MSK, 0, REG_UFS_MMIO_OPT_CTRL_0);

	for (i = 0; i < hba_priv->mcq_nr_intr; i++) {
		irq = hba_priv->mcq_intr_info[i].intr;
		if (irq == MTK_MCQ_INVALID_IRQ) {
			dev_info(hba->dev, "mcq_intr: %d is invalid\n", i);
			break;
		}

		ret = devm_request_irq(hba->dev, irq, ufs_mtk_mcq_intr, 0, UFSHCD,
									&hba_priv->mcq_intr_info[i]);
		if (ret) {
			dev_err(hba->dev, "request irq %d failed\n", irq);
			return;
		}
		dev_info(hba->dev, "request_irq: %d\n", irq);
	}
}

void ufs_mtk_mcq_set_irq_affinity(struct ufs_hba *hba)
{
	struct ufs_hba_private *hba_priv = (struct ufs_hba_private *)hba->android_vendor_data1;
	struct blk_mq_tag_set *tag_set = &hba->host->tag_set;
	struct blk_mq_queue_map	*map = &tag_set->map[HCTX_TYPE_DEFAULT];
	unsigned int nr = map->nr_queues;
	unsigned int q_index, cpu, irq;
	int ret;

	if (!hba_priv->is_mcq_enabled)
		return;

	if (hba_priv->mcq_nr_intr == 0)
		return;

	/* Set affinity */
	for (cpu = 0; cpu < nr; cpu++) {
		if (map->mq_map[cpu] >= 0) {
			q_index = map->mq_map[cpu];
			irq = hba_priv->mcq_intr_info[q_index].intr;
			ret = irq_set_affinity(irq, cpumask_of(cpu));
			if (ret) {
				dev_err(hba->dev, "mcq: irq_set_affinity irq %d on CPU %d failed\n",
									irq, cpu);
				return;
			}
			dev_info(hba->dev, "Set irq %d to CPU: %d\n", irq, cpu);
		}
	}
}

int ufs_mtk_mcq_memory_alloc(struct ufs_hba *hba)
{
	size_t usel_size, ucel_size;
	struct ufs_hba_private *hba_priv =
			(struct ufs_hba_private *)hba->android_vendor_data1;
	int q_size = hba_priv->mcq_nr_hw_queue;
	int pool_size;

	if (!hba_priv->is_mcq_enabled)
		return 0;

	pool_size = UFSHCD_MAX_TAG;

	/*
	 * assume SQi/CQi depth is multiple of 32, and
	 * sum(SQi depth) = sum(CQi depth) = UFSHCD_MAX_TAG
	 * if SQi/CQi may not be multple of 32, need to prepare 8 * SQE_NUM_1K
	 * extra size for the maximum possible fragments
	 */
	//SQ entry
	usel_size = SQE_SIZE * pool_size;
	hba_priv->usel_base_addr = dmam_alloc_coherent(hba->dev,
						   usel_size,
						   &hba_priv->usel_dma_addr,
						   GFP_KERNEL);
	if (!hba_priv->usel_base_addr ||
	    WARN_ON(hba_priv->usel_dma_addr & (PAGE_SIZE - 1))) {
		dev_err(hba->dev,
			"SQ Entry Memory allocation failed\n");
		goto out;
	}

	//CQ entry
	ucel_size = CQE_SIZE * pool_size;
	hba_priv->ucel_base_addr = dmam_alloc_coherent(hba->dev,
						   ucel_size,
						   &hba_priv->ucel_dma_addr,
						   GFP_KERNEL);
	if (!hba_priv->ucel_base_addr ||
	    WARN_ON(hba_priv->ucel_dma_addr & (PAGE_SIZE - 1))) {
		dev_err(hba->dev,
			"CQ Entry Memory allocation failed\n");
		goto out;
	}

	//SQ structure
	hba_priv->mcq_q_cfg.sq = devm_kcalloc(hba->dev,
						   q_size,
						   sizeof(struct ufs_queue),
						   GFP_KERNEL);

	//CQ structure
	hba_priv->mcq_q_cfg.cq = devm_kcalloc(hba->dev,
						   q_size,
						   sizeof(struct ufs_queue),
						   GFP_KERNEL);
	return 0;
out:
	return -ENOMEM;
}

struct tracepoints_table {
	const char *name;
	void *func;
	struct tracepoint *tp;
	bool init;
};

static struct tracepoints_table mcq_interests[] = {
	{
		.name = "android_vh_ufs_use_mcq_hooks",
		.func = ufs_mtk_mcq_use_mcq_hooks
	},
	{
		.name = "android_vh_ufs_mcq_max_tag",
		.func = ufs_mtk_mcq_max_tag
	},
	{
		.name = "android_vh_ufs_mcq_map_tag",
		.func = ufs_mtk_mcq_map_tag
	},
	{
		.name = "android_vh_ufs_mcq_set_sqid",
		.func = ufs_mtk_mcq_set_sqid
	},
	{
		.name = "android_vh_ufs_mcq_handler",
		.func = ufs_mtk_legacy_mcq_handler
	},
	{
		.name = "android_vh_ufs_mcq_make_hba_operational",
		.func = ufs_mtk_mcq_make_hba_operational
	},
	{
		.name = "android_vh_ufs_mcq_hba_capabilities",
		.func = ufs_mtk_mcq_hba_capabilities
	},
	{
		.name = "android_vh_ufs_mcq_print_trs",
		.func = ufs_mtk_mcq_print_trs
	},
	{
		.name = "android_vh_ufs_mcq_send_command",
		.func = ufs_mtk_mcq_send_command
	},
	{
		.name = "android_vh_ufs_mcq_config",
		.func = ufs_mtk_mcq_config
	},
	{
		.name = "android_vh_ufs_mcq_has_oustanding_reqs",
		.func = ufs_mtk_mcq_has_oustanding_reqs
	},
	{
		.name = "android_vh_ufs_mcq_get_outstanding_reqs",
		.func = ufs_mtk_mcq_get_outstanding_reqs
	},
	{
		.name = "android_vh_ufs_mcq_abort",
		.func = ufs_mtk_mcq_abort
	},
	{
		.name = "android_vh_ufs_mcq_clear_cmd",
		.func = ufs_mtk_mcq_clear_cmd
	},
	{
		.name = "android_vh_ufs_mcq_clear_pending",
		.func = ufs_mtk_mcq_clear_pending
	},
};

#define FOR_EACH_INTEREST(i) \
	for (i = 0; i < sizeof(mcq_interests) / sizeof(struct tracepoints_table); \
	i++)

static void ufs_mtk_mcq_lookup_tracepoints(struct tracepoint *tp,
					   void *ignore)
{
	int i;

	FOR_EACH_INTEREST(i) {
		if (strcmp(mcq_interests[i].name, tp->name) == 0)
			mcq_interests[i].tp = tp;
	}
}

void ufs_mtk_mcq_uninstall_tracepoints(void)
{
	int i;

	FOR_EACH_INTEREST(i) {
		if (mcq_interests[i].init) {
			tracepoint_probe_unregister(mcq_interests[i].tp,
							mcq_interests[i].func,
							NULL);
		}
	}
}

int ufs_mtk_mcq_install_tracepoints(void)
{
	int i;

	/* Install the tracepoints */
	for_each_kernel_tracepoint(ufs_mtk_mcq_lookup_tracepoints, NULL);

	FOR_EACH_INTEREST(i) {
		if (mcq_interests[i].tp == NULL) {
			pr_info("Error: tracepoint %s not found\n", mcq_interests[i].name);
			continue;
		}
		tracepoint_probe_register(mcq_interests[i].tp,
					  mcq_interests[i].func,
					  NULL);
		mcq_interests[i].init = true;
	}

	return 0;
}

