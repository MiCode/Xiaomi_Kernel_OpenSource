/* Copyright (c) 2015, The Linux Foundation. All rights reserved.
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

#include <linux/delay.h>
#include <linux/highmem.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/dma-mapping.h>
#include <linux/slab.h>
#include <linux/scatterlist.h>
#include <linux/platform_device.h>
#include <linux/blkdev.h>

#include <linux/mmc/mmc.h>
#include <linux/mmc/host.h>
#include <linux/mmc/card.h>
#include <linux/pm_runtime.h>
#include <linux/workqueue.h>

#include "cmdq_hci.h"
#include "mtk_sd.h"
#include "dbg.h"

#define DCMD_SLOT 31
#define NUM_SLOTS 32

/* 1 sec */
#define HALT_TIMEOUT_MS 1000

static int cmdq_halt_poll(struct mmc_host *mmc, bool halt);
static int cmdq_halt(struct mmc_host *mmc, bool halt);

static inline struct mmc_request *get_req_by_tag(struct cmdq_host *cq_host,
					  unsigned int tag)
{
	return cq_host->mrq_slot[tag];
}

static inline u8 *get_desc(struct cmdq_host *cq_host, u8 tag)
{
	return cq_host->desc_base + (tag * cq_host->slot_sz);
}

static inline u8 *get_link_desc(struct cmdq_host *cq_host, u8 tag)
{
	u8 *desc = get_desc(cq_host, tag);

	return desc + cq_host->task_desc_len;
}

static inline dma_addr_t get_trans_desc_dma(struct cmdq_host *cq_host, u8 tag)
{
	return cq_host->trans_desc_dma_base +
		(u32)(cq_host->mmc->max_segs * tag *
		 cq_host->trans_desc_len);
}

static inline u8 *get_trans_desc(struct cmdq_host *cq_host, u8 tag)
{
	return cq_host->trans_desc_base +
		(cq_host->trans_desc_len * cq_host->mmc->max_segs * tag);
}

static void setup_trans_desc(struct cmdq_host *cq_host, u8 tag)
{
	u8 *link_temp;
	dma_addr_t trans_temp;

	link_temp = get_link_desc(cq_host, tag);
	trans_temp = get_trans_desc_dma(cq_host, tag);

	memset(link_temp, 0, cq_host->link_desc_len);
	if (cq_host->link_desc_len > 8)
		*(link_temp + 8) = 0;

	if (tag == DCMD_SLOT) {
		*link_temp = VALID(0) | ACT(0) | END(1);
		return;
	}

	*link_temp = VALID(1) | ACT(0x6) | END(0);

	if (cq_host->dma64) {
		__le64 *data_addr = (__le64 __force *)(link_temp + 4);

		data_addr[0] = cpu_to_le64(trans_temp);
	} else {
		__le32 *data_addr = (__le32 __force *)(link_temp + 4);

		data_addr[0] = cpu_to_le32(trans_temp);
	}
}

static void cmdq_set_halt_irq(struct cmdq_host *cq_host, bool enable)
{
	u32 ier;

	ier = cmdq_readl(cq_host, CQISTE);
	if (enable) {
		cmdq_writel(cq_host, ier | HALT, CQISTE);
		cmdq_writel(cq_host, ier | HALT, CQISGE);
	} else {
		cmdq_writel(cq_host, ier & ~HALT, CQISTE);
		cmdq_writel(cq_host, ier & ~HALT, CQISGE);
	}
}

static void cmdq_clear_set_irqs(struct cmdq_host *cq_host, u32 clear, u32 set)
{
	u32 ier = 0;

	ier = cmdq_readl(cq_host, CQISTE);
	ier &= ~clear;
	ier |= set;
	cmdq_writel(cq_host, ier, CQISTE);
	cmdq_writel(cq_host, ier, CQISGE);
	/* ensure the writes are done */
	mb();
}

#define DRV_NAME "cmdq-host"

void cmdq_dumpregs(struct cmdq_host *cq_host)
{
	struct mmc_host *mmc = cq_host->mmc;

	pr_notice(DRV_NAME ": ========== REGISTER DUMP (%s)==========\n",
		mmc_hostname(mmc));

	pr_notice(DRV_NAME ": Caps: 0x%08x		  | Version:  0x%08x\n",
		cmdq_readl(cq_host, CQCAP),
		cmdq_readl(cq_host, CQVER));
	pr_notice(DRV_NAME ": Queing config: 0x%08x  | Queue Ctrl:  0x%08x\n",
		cmdq_readl(cq_host, CQCFG),
		cmdq_readl(cq_host, CQCTL));
	pr_notice(DRV_NAME ": Int stat: 0x%08x	  | Int enab:  0x%08x\n",
		cmdq_readl(cq_host, CQIS),
		cmdq_readl(cq_host, CQISTE));
	pr_notice(DRV_NAME ": Int sig: 0x%08x	  | Int Coal:  0x%08x\n",
		cmdq_readl(cq_host, CQISGE),
		cmdq_readl(cq_host, CQIC));
	pr_notice(DRV_NAME ": TDL base: 0x%08x	  | TDL up32:  0x%08x\n",
		cmdq_readl(cq_host, CQTDLBA),
		cmdq_readl(cq_host, CQTDLBAU));
	pr_notice(DRV_NAME ": Doorbell: 0x%08x	  | Comp Notif:  0x%08x\n",
		cmdq_readl(cq_host, CQTDBR),
		cmdq_readl(cq_host, CQTCN));
	pr_notice(DRV_NAME ": Dev queue: 0x%08x	  | Dev Pend:  0x%08x\n",
		cmdq_readl(cq_host, CQDQS),
		cmdq_readl(cq_host, CQDPT));
	pr_notice(DRV_NAME ": Task clr: 0x%08x	  | Send stat 1:  0x%08x\n",
		cmdq_readl(cq_host, CQTCLR),
		cmdq_readl(cq_host, CQSSC1));
	pr_notice(DRV_NAME ": Send stat 2: 0x%08x	  | DCMD resp:  0x%08x\n",
		cmdq_readl(cq_host, CQSSC2),
		cmdq_readl(cq_host, CQCRDCT));
	pr_notice(DRV_NAME ": Resp err mask: 0x%08x  | Task err:  0x%08x\n",
		cmdq_readl(cq_host, CQRMEM),
		cmdq_readl(cq_host, CQTERRI));
	pr_notice(DRV_NAME ": Resp idx 0x%08x	  | Resp arg:  0x%08x\n",
		cmdq_readl(cq_host, CQCRI),
		cmdq_readl(cq_host, CQCRA));
	pr_notice(DRV_NAME": Vendor cfg 0x%08x\n",
	       cmdq_readl(cq_host, CQ_VENDOR_CFG));
	pr_notice(DRV_NAME ": ===========================================\n");

}

/**
 * The allocated descriptor table for task, link & transfer descritors
 * looks like:
 * |----------|
 * |task desc |  |->|----------|
 * |----------|  |  |trans desc|
 * |link desc-|->|  |----------|
 * |----------|          .
 *      .                .
 *  no. of slots      max-segs
 *      .           |----------|
 * |----------|
 * The idea here is to create the [task+trans] table and mark & point the
 * link desc to the transfer desc table on a per slot basis.
 */
static int cmdq_host_alloc_tdl(struct cmdq_host *cq_host)
{

	size_t desc_size = 0;
	size_t data_size = 0;
	int i = 0;

	/* task descriptor can be 64/128 bit irrespective of arch */
	if (cq_host->caps & CMDQ_TASK_DESC_SZ_128) {
		cmdq_writel(cq_host, cmdq_readl(cq_host, CQCFG) |
			       CQ_TASK_DESC_SZ, CQCFG);
		cq_host->task_desc_len = 16;
	} else {
		cq_host->task_desc_len = 8;
	}

	/*
	 * 96 bits length of transfer desc instead of 128 bits which means
	 * ADMA would expect next valid descriptor at the 96th bit
	 * or 128th bit
	 */
	if (cq_host->dma64) {
		if (cq_host->quirks & CMDQ_QUIRK_SHORT_TXFR_DESC_SZ)
			cq_host->trans_desc_len = 12;
		else
			cq_host->trans_desc_len = 16;
		cq_host->link_desc_len = 16;
	} else {
		cq_host->trans_desc_len = 8;
		cq_host->link_desc_len = 8;
	}

	/* total size of a slot: 1 task & 1 transfer (link) */
	cq_host->slot_sz = cq_host->task_desc_len + cq_host->link_desc_len;

	desc_size = cq_host->slot_sz * (u32)cq_host->num_slots;

	data_size = cq_host->trans_desc_len * cq_host->mmc->max_segs *
		((u32)cq_host->num_slots - 1);

#ifdef MMC_CQHCI_DEBUG
	pr_debug("%s: desc_size: %d data_sz: %d slot-sz: %d\n", __func__,
		(int)desc_size, (int)data_size, cq_host->slot_sz);
#endif
	/*
	 * allocate a dma-mapped chunk of memory for the descriptors
	 * allocate a dma-mapped chunk of memory for link descriptors
	 * setup each link-desc memory offset per slot-number to
	 * the descriptor table.
	 */
	cq_host->desc_base = dma_alloc_coherent(mmc_dev(cq_host->mmc),
						 desc_size,
						 &cq_host->desc_dma_base,
						 GFP_KERNEL);
	cq_host->trans_desc_base = dma_alloc_coherent(mmc_dev(cq_host->mmc),
					      data_size,
					      &cq_host->trans_desc_dma_base,
					      GFP_KERNEL);

	if (!cq_host->desc_base || !cq_host->trans_desc_base)
		return -ENOMEM;

#ifdef MMC_CQHCI_DEBUG
	pr_debug("desc-base: 0x%p trans-base: 0x%p\n desc_dma 0x%llx trans_dma: 0x%llx\n",
		 cq_host->desc_base, cq_host->trans_desc_base,
		(unsigned long long) cq_host->desc_dma_base,
		(unsigned long long) cq_host->trans_desc_dma_base);
#endif

	for (; i < (cq_host->num_slots); i++)
		setup_trans_desc(cq_host, i);

	return 0;
}

static int cmdq_enable(struct mmc_host *mmc)
{
	int err = 0;
	u32 cqcfg = 0;
	bool dcmd_enable = FALSE;
	struct cmdq_host *cq_host = mmc_cmdq_private(mmc);

	if (!cq_host || !mmc->card || !mmc_card_cmdq(mmc->card)) {
		err = -EINVAL;
		goto out;
	}

	if (cq_host->enabled)
		goto out;

	cqcfg = cmdq_readl(cq_host, CQCFG);
	if (cqcfg & 0x1) {
		pr_notice("%s: %s: cq_host is already enabled\n",
				mmc_hostname(mmc), __func__);
		WARN_ON(1);
		goto out;
	}

	/* pre-setting for cqe */
	if (cq_host->ops->pre_cqe_enable)
		cq_host->ops->pre_cqe_enable(mmc, true);

	if (cq_host->quirks & CMDQ_QUIRK_NO_DCMD)
		dcmd_enable = false;
	else
		dcmd_enable = true;

	cqcfg = ((cq_host->caps & CMDQ_TASK_DESC_SZ_128 ? CQ_TASK_DESC_SZ : 0) |
			(dcmd_enable ? CQ_DCMD : 0));

	cmdq_writel(cq_host, cqcfg, CQCFG);
	/* enable CQ_HOST */
	cmdq_writel(cq_host,
		cmdq_readl(cq_host, CQCFG) | CQ_CRYPTO_ENABLE | CQ_ENABLE,
		CQCFG);

	if (!cq_host->desc_base ||
			!cq_host->trans_desc_base) {
		err = cmdq_host_alloc_tdl(cq_host);
		if (err) {
			pr_notice("cmdq_host_alloc_tdl fail\n");
			goto out;
		}
	}
	cmdq_writel(cq_host, 0x40, CQSSC1);
	cmdq_writel(cq_host, lower_32_bits(cq_host->desc_dma_base), CQTDLBA);
	cmdq_writel(cq_host, upper_32_bits(cq_host->desc_dma_base), CQTDLBAU);

	/*
	 * disable all vendor interrupts
	 * enable CMDQ interrupts
	 * enable the vendor error interrupts
	 */
	cmdq_clear_set_irqs(cq_host, 0x0, CQ_INT_ALL);

	/* cq_host would use this rca to address the card */
	cmdq_writel(cq_host, mmc->card->rca, CQSSC2);

	/* send QSR at lesser intervals than the default */
	cmdq_writel(cq_host, cmdq_readl(cq_host, CQSSC1) | SEND_QSR_INTERVAL,
				CQSSC1);

	/* ensure the writes are done before enabling CQE */
	mb();

	cq_host->enabled = true;
	mmc_host_clr_cq_disable(mmc);
out:
	return err;
}

static void cmdq_disable(struct mmc_host *mmc, bool soft)
{
	struct cmdq_host *cq_host = (struct cmdq_host *)mmc_cmdq_private(mmc);

	if (soft) {
		cmdq_writel(cq_host, cmdq_readl(
				    cq_host, CQCFG) & ~(CQ_ENABLE),
			    CQCFG);
	}

	/* restore settings for normal mode */
	if (cq_host->ops->pre_cqe_enable)
		cq_host->ops->pre_cqe_enable(mmc, false);

	mmc_host_set_cq_disable(mmc);
	cq_host->enabled = false;
}

static void cmdq_reset(struct mmc_host *mmc, bool soft)
{
	struct cmdq_host *cq_host = (struct cmdq_host *)mmc_cmdq_private(mmc);
	struct mmc_request *mrq = mmc->err_mrq;
	unsigned int cqcfg = 0;
	unsigned int tdlba = 0;
	unsigned int tdlbau = 0;
	unsigned int rca = 0;
	int ret;

	pr_notice("%s: %s\n",
			mmc_hostname(mmc), __func__);

	cqcfg = cmdq_readl(cq_host, CQCFG);
	tdlba = cmdq_readl(cq_host, CQTDLBA);
	tdlbau = cmdq_readl(cq_host, CQTDLBAU);
	rca = cmdq_readl(cq_host, CQSSC2);

	cmdq_disable(mmc, true);

	if (cq_host->ops->reset && !mrq->cmdq_req->skip_reset) {
		ret = cq_host->ops->reset(mmc);
		if (ret) {
			pr_notice("%s: reset CMDQ controller: failed\n",
				mmc_hostname(mmc));
			WARN_ON(1); /*bug*/
		}
	}

	cmdq_writel(cq_host, tdlba, CQTDLBA);
	cmdq_writel(cq_host, tdlbau, CQTDLBAU);

	cmdq_clear_set_irqs(cq_host, 0x0, CQ_INT_ALL);

	/* cq_host would use this rca to address the card */
	cmdq_writel(cq_host, rca, CQSSC2);

	/* ensure the writes are done before enabling CQE */
	mb();

	cmdq_writel(cq_host, cqcfg, CQCFG);
	cq_host->enabled = true;
	mmc_host_clr_cq_disable(mmc);
}

static void cmdq_prep_task_desc(struct mmc_request *mrq,
					u64 *data, bool intr, bool qbr)
{
	struct mmc_cmdq_req *cmdq_req = mrq->cmdq_req;
	u32 req_flags = cmdq_req->cmdq_req_flags;
	struct mmc_host *mmc = mrq->host;
	struct cmdq_host *cq_host = mmc_cmdq_private(mmc);
	u32 prio = !!(req_flags & PRIO);

	if (cq_host->quirks & CMDQ_QUIRK_PRIO_READ)
		prio |= (!!(req_flags & DIR) ? 1 : 0);

#ifdef MMC_CQHCI_DEBUG
	pr_debug("%s: %s: data-tag: %d - dir: %d - prio: %d - cnt: 0x%08x - addr: 0x%08x\n",
		 mmc_hostname(mrq->host), __func__,
		 !!(req_flags & DAT_TAG), !!(req_flags & DIR),
		 prio, cmdq_req->data.blocks,
		 cmdq_req->blk_addr);
#endif

	/* set force programming, if can not apply cache during write */
	/* when DIR is set means read, so check if DIR is unset here */
	if (!(req_flags & DIR) &&
		!msdc_can_apply_cache(
		(u64)mrq->cmdq_req->blk_addr,
		mrq->cmdq_req->data.blocks)) {
		req_flags |= FORCED_PRG;
	}

	*data = VALID(1) |
		END(1) |
		INT(intr) |
		ACT(0x5) |
		FORCED_PROG(!!(req_flags & FORCED_PRG)) |
		CONTEXT(mrq->cmdq_req->ctx_id) |
		DATA_TAG(!!(req_flags & DAT_TAG)) |
		DATA_DIR(!!(req_flags & DIR)) |
		PRIORITY(prio) |
		QBAR(qbr) |
		REL_WRITE(!!(req_flags & REL_WR)) |
		BLK_COUNT(mrq->cmdq_req->data.blocks) |
		BLK_ADDR((u64)mrq->cmdq_req->blk_addr);

}

static int cmdq_dma_map(struct mmc_host *host, struct mmc_request *mrq)
{
	int sg_count = 0;
	struct mmc_data *data = mrq->data;

	if (!data)
		return -EINVAL;

	sg_count = dma_map_sg(mmc_dev(host), data->sg,
			      data->sg_len,
			      (data->flags & MMC_DATA_WRITE) ?
			      DMA_TO_DEVICE : DMA_FROM_DEVICE);
	if (!sg_count) {
		pr_notice("%s: sg-len: %d\n", __func__, data->sg_len);
		return -ENOMEM;
	}

	/*check*/
	WARN_ON(sg_count != data->sg_len);

	return sg_count;
}

static void cmdq_set_tran_desc(u8 *desc, dma_addr_t addr, int len,
				bool end, bool is_dma64)
{
	__le32 *attr = (__le32 __force *)desc;

	*attr = (VALID(1) |
		 END(end ? 1 : 0) |
		 INT(0) |
		 ACT(0x4) |
		 DAT_LENGTH(len));

	if (is_dma64) {
		__le64 *dataddr = (__le64 __force *)(desc + 4);

		dataddr[0] = cpu_to_le64(addr);
	} else {
		__le32 *dataddr = (__le32 __force *)(desc + 4);

		dataddr[0] = cpu_to_le32(addr);
	}
}

static int cmdq_prep_tran_desc(struct mmc_request *mrq,
			       struct cmdq_host *cq_host, int tag)
{
	struct mmc_data *data = mrq->data;
	int i = 0, sg_count = 0, len = 0;
	bool end = false;
	dma_addr_t addr = 0;
	u8 *desc = NULL;
	struct scatterlist *sg = NULL;

	sg_count = cmdq_dma_map(mrq->host, mrq);
	if (sg_count < 0) {
		pr_notice("%s: %s: unable to map sg lists, %d\n",
				mmc_hostname(mrq->host), __func__, sg_count);
		return sg_count;
	}

	desc = get_trans_desc(cq_host, tag);
	memset(desc, 0, cq_host->trans_desc_len * cq_host->mmc->max_segs);

	for_each_sg(data->sg, sg, sg_count, i) {
		addr = sg_dma_address(sg);
		len = sg_dma_len(sg);

		if ((i+1) == sg_count)
			end = true;

		cmdq_set_tran_desc(desc, addr, len, end, cq_host->dma64);
		desc += cq_host->trans_desc_len;
	}

#ifdef MMC_CQHCI_DEBUG
	pr_debug("%s: req: 0x%p tag: %d calc_trans_des: 0x%p sg-cnt: %d\n",
		__func__, mrq->req, tag, desc, sg_count);
#endif

	return 0;
}

static void cmdq_prep_dcmd_desc(struct mmc_host *mmc,
				   struct mmc_request *mrq)
{
	u64 *task_desc = NULL;
	u64 data = 0;
	u8 resp_type = 0;
	u8 *desc = NULL;
	__le64 *dataddr = NULL;
	struct cmdq_host *cq_host = mmc_cmdq_private(mmc);
	u8 timing = 0;

	if (!(mrq->cmd->flags & MMC_RSP_PRESENT)) {
		resp_type = 0x0; /* no response */
		timing = 0x1;
	} else {
		if (mrq->cmd->flags & MMC_RSP_BUSY) {
			resp_type = 0x3; /* r1b */
			timing = 0x0;
		} else {
			resp_type = 0x2; /* r1,r4,r5 */
			timing = 0x1;
		}
	}

	task_desc = (__le64 __force *)get_desc(cq_host, cq_host->dcmd_slot);
	memset(task_desc, 0, cq_host->task_desc_len);
	data |= (VALID(1) |
		 END(1) |
		 INT(1) |
		 QBAR(1) |
		 ACT(0x5) |
		 CMD_INDEX(mrq->cmd->opcode) |
		 CMD_TIMING(timing) | RESP_TYPE(resp_type));
	*task_desc |= data;
	desc = (u8 *)task_desc;

#ifdef MMC_CQHCI_DEBUG
	pr_debug("cmdq: dcmd: cmd: %d arg: 0x%x timing: %d resp: %d\n",
		mrq->cmd->opcode, mrq->cmd->arg, timing, resp_type);
#endif

	dataddr = (__le64 __force *)(desc + 4);
	dataddr[0] = cpu_to_le64((u64)mrq->cmd->arg);
	/* make sure data was written to memory */
	mb();
}

static inline
void cmdq_prep_crypto_desc(struct cmdq_host *cq_host, u64 *task_desc,
			u64 hci_ce_ctx)
{
	u64 *hci_ce_desc = NULL;

	if (cq_host->caps & CMDQ_CAP_CRYPTO_SUPPORT) {
		/*
		 * Get the address of ce context for the given task descriptor.
		 * ice context is present in the upper 64bits of task descriptor
		 */
		hci_ce_desc = (__le64 __force *)((u8 *)task_desc +
						CQ_TASK_DESC_TASK_PARAMS_SIZE);
		memset(hci_ce_desc, 0, CQ_TASK_DESC_CE_PARAMS_SIZE);

		/*
		 *  Assign upper 64bits data of task descritor with ce context
		 */
		if (hci_ce_desc)
			*hci_ce_desc = cpu_to_le64(hci_ce_ctx);
	}
}

static int cmdq_request(struct mmc_host *mmc, struct mmc_request *mrq)
{
	int err = 0;
	u64 data = 0;
	u64 *task_desc = NULL;
	u32 tag = mrq->cmdq_req->tag;
	struct cmdq_host *cq_host = (struct cmdq_host *)mmc_cmdq_private(mmc);
	u64 hci_ce_ctx = 0;

	struct msdc_host *msdc_host = mmc_priv(mmc);

	MVG_EMMC_DECLARE_INT32(delay_ns);
	MVG_EMMC_DECLARE_INT32(delay_us);
	MVG_EMMC_DECLARE_INT32(delay_ms);

	(void)msdc_host; /* prevent build error */

	if (!cq_host->enabled) {
		pr_notice("%s: CMDQ host not enabled yet !!!\n",
		       mmc_hostname(mmc));
		err = -EINVAL;
		goto out;
	}

	if (mrq->cmdq_req->cmdq_req_flags & DCMD) {
		cmdq_prep_dcmd_desc(mmc, mrq);
		cq_host->mrq_slot[DCMD_SLOT] = mrq;
		/* DCMD's are always issued on a fixed slot */
		tag = DCMD_SLOT;
		dbg_add_host_log(mmc, MAGIC_CQHCI_DBG_TYPE_DCMD,
			mrq->cmd->opcode,
			mrq->cmd->arg);
		goto ring_doorbell;
	}

	if (cq_host->ops->crypto_cfg) {
		err = cq_host->ops->crypto_cfg(mmc, mrq, tag, &hci_ce_ctx);
		if (err) {
			pr_notice("%s: failed to configure crypto: err %d tag %d\n",
					mmc_hostname(mmc), err, tag);
			goto out;
		}
	}

	task_desc = (__le64 __force *)get_desc(cq_host, tag);

	cmdq_prep_task_desc(mrq, &data, 1,
			    (mrq->cmdq_req->cmdq_req_flags & QBR));
	*task_desc = cpu_to_le64(data);

	cmdq_prep_crypto_desc(cq_host, task_desc, hci_ce_ctx);

	err = cmdq_prep_tran_desc(mrq, cq_host, tag);
	if (err) {
		pr_notice("%s: %s: failed to setup tx desc: %d\n",
		       mmc_hostname(mmc), __func__, err);
		return err;
	}

	WARN_ON(cmdq_readl(cq_host, CQTDBR) & (1 << tag));  /*bug*/

	cq_host->mrq_slot[tag] = mrq;

	/* EN_CQHCI_IRQ, msdc host base + 0x10 */
	cmdq_writel_normal(cq_host, (1 << 28), 0x10);

	dbg_add_host_log(mmc, MAGIC_CQHCI_DBG_TYPE,
		MAGIC_CQHCI_DBG_NUM_L + tag,
		lower_32_bits(*task_desc));
	dbg_add_host_log(mmc, MAGIC_CQHCI_DBG_TYPE,
		MAGIC_CQHCI_DBG_NUM_U + tag,
		upper_32_bits(*task_desc));

ring_doorbell:
#ifdef MMC_CQHCI_DEBUG
	pr_debug("%s: %s, mrq->req = 0x%p, tag = %d, mrq->cmdq_req->tag = %d issued!!!\n",
		   mmc_hostname(mmc), __func__,
		   mrq->req, tag, mrq->cmdq_req->tag);
#endif

	if (mrq->cmdq_req->cmdq_req_flags & DCMD)
		MVG_EMMC_ERASE_MATCH(msdc_host,
			mrq->cmd->arg, delay_ms, delay_us,
			delay_ns, mrq->cmd->opcode);

	/* Ensure the task descriptor list is flushed before ringing doorbell */
	wmb();
	cmdq_writel(cq_host, 1 << tag, CQTDBR);
	/* Commit the doorbell write immediately */
	wmb();

	if (mrq->cmdq_req->cmdq_req_flags & DCMD)
		MVG_EMMC_ERASE_RESET(delay_ms,
			delay_us,
			mrq->cmd->opcode);
	else
		MVG_EMMC_WRITE_MATCH(msdc_host,
			(u64)mrq->cmdq_req->blk_addr,
			delay_ms, delay_us, delay_ns,
			(mrq->cmdq_req->cmdq_req_flags & DIR) ?
			(46 + tag * 100) : (47 + tag * 100),
			mrq->cmdq_req->data.blocks << 9);

out:

	return err;
}

static void cmdq_finish_data(struct mmc_host *mmc, unsigned int tag)
{
	struct mmc_request *mrq = NULL;
	struct cmdq_host *cq_host = (struct cmdq_host *)mmc_cmdq_private(mmc);

	mrq = get_req_by_tag(cq_host, tag);
	if (tag == cq_host->dcmd_slot) {
		mrq->cmd->resp[0] = cmdq_readl(cq_host, CQCRDCT);
		dbg_add_host_log(mmc, (MAGIC_CQHCI_DBG_TYPE_DCMD + 1),
			mrq->cmd->opcode,
			mrq->cmd->resp[0]);
	} else {
		dbg_add_host_log(mmc, MAGIC_CQHCI_DBG_TYPE,
			MAGIC_CQHCI_DBG_NUM_RI + tag,
			cmdq_readl(cq_host, CQCRA));
	}
	mrq->done(mrq);
}

irqreturn_t cmdq_irq(struct mmc_host *mmc, int err)
{
	u32 status = 0;
	unsigned long tag = 0, comp_status = 0, cmd_idx = 0;
	struct cmdq_host *cq_host = (struct cmdq_host *)mmc_cmdq_private(mmc);
	unsigned long err_info = 0;
	struct mmc_request *mrq = NULL;
	int ret;

	status = cmdq_readl(cq_host, CQIS);
	cmdq_writel(cq_host, status, CQIS);

	if (!status && !err)
		return IRQ_NONE;

_err:
	if (err || (status & CQIS_RED)) {
		err_info = cmdq_readl(cq_host, CQTERRI);
		pr_notice("%s: err: %d status: 0x%08x task-err-info (0x%08lx)\n",
		       mmc_hostname(mmc), err, status, err_info);

		if (mmc->cmdq_ops->dumpstate)
			mmc->cmdq_ops->dumpstate(mmc, false);

		/*
		 * Need to halt CQE in case of error in interrupt context itself
		 * otherwise CQE may proceed with sending CMD to device even if
		 * CQE/card is in error state.
		 * CMDQ error handling will make sure that it is unhalted after
		 * handling all the errors.
		 */
		ret = cmdq_halt_poll(mmc, true);
		if (ret)
			pr_notice("%s: %s: halt failed ret = %d\n",
					mmc_hostname(mmc), __func__, ret);

		/*
		 * Clear the CQIS after halting incase of error. This is done
		 * because if CQIS is cleared before halting, the CQ will
		 * continue with issueing commands for rest of requests with
		 * Doorbell rung. This will overwrite the Resp Arg register.
		 * So CQ must be halted first and then CQIS cleared in case
		 * of error
		 */
#ifdef MMC_CQHCI_DEBUG
		pr_debug("%s: %s: CQIS = 0x%x(last 0x%x), CQTCN = 0x%x after halt\n",
				mmc_hostname(mmc), __func__,
				cmdq_readl(cq_host, CQIS),
				status,
				cmdq_readl(cq_host, CQTCN));
#endif
		cmdq_writel(cq_host, status, CQIS);

		if (err_info & CQ_RMEFV) {
			cmd_idx = GET_CMD_ERR_CMDIDX(err_info);
			if (cmd_idx == MMC_SEND_STATUS) {
				u32 task_mask;
				/*
				 * since CMD13 does not belong to
				 * any tag, just find an active
				 * task from CQTCN or CQTDBR register
				 * and trigger error.
				 */
				task_mask = cmdq_readl(cq_host, CQTCN);
				if (!task_mask)
					task_mask = cmdq_readl(cq_host, CQTDBR);
				tag = uffs(task_mask) - 1;
				pr_notice("%s: cmd%lu err tag: %lu\n",
					__func__, cmd_idx, tag);
			} else {
				tag = GET_CMD_ERR_TAG(err_info);
				pr_notice("%s: cmd err tag: %lu\n",
					__func__, tag);
			}
			mrq = get_req_by_tag(cq_host, tag);
			/* CMD44/45/46/47 will not have a valid cmd */
			if (mrq->cmd)
				mrq->cmd->error = err;
			else
				mrq->data->error = err;
		} else if (err_info & CQ_DTEFV) {
			tag = GET_DAT_ERR_TAG(err_info);
			pr_notice("%s: dat err  tag: %lu\n", __func__, tag);

			mrq = get_req_by_tag(cq_host, tag);
			mrq->data->error = err;
		}

		/*
		 * CQE detected a response error from device
		 * In most cases, this would require a reset.
		 */
		if (status & CQIS_RED) {
			/*
			 * will check if the RED error is due to a bkops
			 * exception once the queue is empty
			 */
			WARN_ON(!mmc->card); /*bug*/
			if (mrq && mrq->cmdq_req) {
				mrq->cmdq_req->resp_err = true;
				if (cmdq_readl(cq_host, CQCRA) & (0x1 << 26)) {
				/* WP violation: shrink log & don't run autok */
					mrq->cmdq_req->skip_dump = true;
					mrq->cmdq_req->skip_reset = true;
				}
			}
			pr_notice("%s: Response error (0x%08x) from card !!!\n",
				mmc_hostname(mmc), cmdq_readl(cq_host, CQCRA));
		} else {
			if (mrq && mrq->cmdq_req) {
				mrq->cmdq_req->resp_idx =
					cmdq_readl(cq_host, CQCRI);
				mrq->cmdq_req->resp_arg =
					cmdq_readl(cq_host, CQCRA);
			}
		}

		/*
		 * When the error finish and set CMDQ_STATE_ERR error state,
		 * the non-error completed request finish afterwards will
		 * trigger BUG at mmc_blk_cmdq_complete_rq(). So don't
		 * finish non-error request when error occurs.
		 */
		status &= ~CQIS_TCC;
		/*
		 * We don't complete non-error request when error occurs,
		 * so clear the controller status here.
		 */
		cmdq_writel(cq_host, cmdq_readl(cq_host, CQTCN), CQTCN);

		cmdq_finish_data(mmc, tag);
	}

	if (status & CQIS_TCC) {
		/* read CQTCN and complete the request */
		comp_status = cmdq_readl(cq_host, CQTCN);
		if (!comp_status)
			goto out;
		/*
		 * Error interrupt may assert after
		 * normal task completion, so check here
		 * before complete task.
		 */
		if (cq_host->ops->pre_irq_complete)
			err = cq_host->ops->pre_irq_complete(mmc, err);
		if (err)
			goto _err;
		/*
		 * The CQTCN must be cleared before notifying req completion
		 * to upper layers to avoid missing completion notification
		 * of new requests with the same tag.
		 */
		cmdq_writel(cq_host, comp_status, CQTCN);
		/*
		 * A write memory barrier is necessary to guarantee that CQTCN
		 * gets cleared first before next doorbell for the same tag is
		 * set but that is already achieved by the barrier present
		 * before setting doorbell, hence one is not needed here.
		 */
		for_each_set_bit(tag, &comp_status, cq_host->num_slots) {
			/* complete the corresponding mrq */
#ifdef MMC_CQHCI_DEBUG
			pr_debug("%s: %s completing tag -> %lu\n",
				 mmc_hostname(mmc), __func__, tag);
#endif
			cmdq_finish_data(mmc, tag);
		}
	}

	if (status & CQIS_HAC) {
		if (cq_host->ops->post_cqe_halt)
			cq_host->ops->post_cqe_halt(mmc);
		/* halt is completed, wakeup waiting thread */
		complete(&cq_host->halt_comp);
	}

out:
	return IRQ_HANDLED;
}
EXPORT_SYMBOL(cmdq_irq);

/* cmdq_halt_poll - Halting CQE using polling method.
 * @mmc: struct mmc_host
 * @halt: bool halt
 * This is used mainly from interrupt context to halt/unhalt
 * CQE engine.
 */
static int cmdq_halt_poll(struct mmc_host *mmc, bool halt)
{
	struct cmdq_host *cq_host = (struct cmdq_host *)mmc_cmdq_private(mmc);
	int retries = 100;

	pr_notice("%s: %s halt = %d\n",
		mmc_hostname(mmc), __func__, halt);

	if (!halt) {
		if (cq_host->ops->clear_set_irqs)
			cq_host->ops->clear_set_irqs(mmc, true);
		cmdq_writel(cq_host, cmdq_readl(cq_host, CQCTL) & ~HALT,
			    CQCTL);
		mmc_host_clr_halt(mmc);
		return 0;
	}

	cmdq_set_halt_irq(cq_host, false);
	cmdq_writel(cq_host, cmdq_readl(cq_host, CQCTL) | HALT, CQCTL);
	while (retries) {
		if (!(cmdq_readl(cq_host, CQCTL) & HALT)) {
			udelay(5);
			retries--;
			continue;
		} else {
			if (cq_host->ops->post_cqe_halt)
				cq_host->ops->post_cqe_halt(mmc);
			/* halt done: re-enable legacy interrupts */
			if (cq_host->ops->clear_set_irqs)
				cq_host->ops->clear_set_irqs(mmc, false);
			mmc_host_set_halt(mmc);
			break;
		}
	}
	cmdq_set_halt_irq(cq_host, true);

	if (!retries) {
		pr_notice("%s: %s err! retries = %d\n",
			 mmc_hostname(mmc), __func__, retries);
	}

	return retries ? 0 : -ETIMEDOUT;
}

/* May sleep */
static int cmdq_halt(struct mmc_host *mmc, bool halt)
{
	struct cmdq_host *cq_host = (struct cmdq_host *)mmc_cmdq_private(mmc);
	u32 ret = 0;
	int retries = 3;

	if (halt) {
		while (retries) {
			cmdq_writel(cq_host, cmdq_readl(cq_host, CQCTL) | HALT,
				    CQCTL);
			ret = wait_for_completion_timeout(&cq_host->halt_comp,
					  msecs_to_jiffies(HALT_TIMEOUT_MS));
			if (!ret && !(cmdq_readl(cq_host, CQCTL) & HALT)) {
				pr_notice("%s: %s: HAC int timeout, retryng halt (%d)\n",
					mmc_hostname(mmc), __func__, retries);
				retries--;
				continue;
			} else {
				/* halt done: re-enable legacy interrupts */
				break;
			}
		}
		ret = retries ? 0 : -ETIMEDOUT;
	} else {
		cmdq_writel(cq_host, cmdq_readl(cq_host, CQCTL) & ~HALT,
			    CQCTL);
		/*
		 * Since driver take over control the HW settings
		 * maybe changed by SW, so restore pre-setting here
		 * after un-halt.
		 */
		if (cq_host->ops->pre_cqe_enable)
			cq_host->ops->pre_cqe_enable(mmc, true);
	}

	return ret;
}

static void cmdq_post_req(struct mmc_host *mmc, int tag, int err)
{
	struct cmdq_host *cq_host = NULL;
	struct mmc_request *mrq = NULL;
	struct mmc_data *data = NULL;

	if (WARN_ON(!mmc))
		return;

	cq_host = (struct cmdq_host *)mmc_cmdq_private(mmc);
	mrq = get_req_by_tag(cq_host, tag);
	data = mrq->data;

	if (data) {
		data->error = err;
		dma_unmap_sg(mmc_dev(mmc), data->sg, data->sg_len,
			     (data->flags & MMC_DATA_READ) ?
			     DMA_FROM_DEVICE : DMA_TO_DEVICE);
		if (err)
			data->bytes_xfered = 0;
		else
			data->bytes_xfered = blk_rq_bytes(mrq->req);
	}
}

static void cmdq_dumpstate(struct mmc_host *mmc, bool more)
{
	struct cmdq_host *cq_host;

	cq_host = (struct cmdq_host *)mmc_cmdq_private(mmc);

	cmdq_dumpregs(cq_host);
	if (more)
		msdc_dump_info(NULL, 0, NULL, 0);
}

static const struct mmc_cmdq_host_ops cmdq_host_ops = {
	.enable = cmdq_enable,
	.disable = cmdq_disable,
	.request = cmdq_request,
	.post_req = cmdq_post_req,
	.halt = cmdq_halt,
	.reset	= cmdq_reset,
	.dumpstate = cmdq_dumpstate,
};

struct cmdq_host *cmdq_pltfm_init(struct platform_device *pdev)
{
	struct cmdq_host *cq_host = NULL;
	/* check and setup CMDQ interface */
	cq_host = kzalloc(sizeof(*cq_host), GFP_KERNEL);
	if (!cq_host) {
		/* mark for check patch */
		/* dev_notice(&pdev->dev,
		 *	"alloc mem failed for CMDQ\n");
		 */
		return ERR_PTR(-ENOMEM);
	}

	return cq_host;
}
EXPORT_SYMBOL(cmdq_pltfm_init);

int cmdq_init(struct cmdq_host *cq_host, struct mmc_host *mmc,
	      bool dma64)
{
	int err = 0;

	cq_host->dma64 = dma64;
	cq_host->mmc = mmc;
	cq_host->mmc->cmdq_private = cq_host;

	cq_host->num_slots = NUM_SLOTS;
	cq_host->dcmd_slot = DCMD_SLOT;

	mmc->cmdq_ops = &cmdq_host_ops;
	mmc->num_cq_slots = NUM_SLOTS;
	mmc->dcmd_cq_slot = DCMD_SLOT;

	cq_host->mrq_slot = kzalloc(sizeof(cq_host->mrq_slot) *
				    cq_host->num_slots, GFP_KERNEL);
	if (!cq_host->mrq_slot)
		return -ENOMEM;

	init_completion(&cq_host->halt_comp);
	return err;
}
EXPORT_SYMBOL(cmdq_init);
