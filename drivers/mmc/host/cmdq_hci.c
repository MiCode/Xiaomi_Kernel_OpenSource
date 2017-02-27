/* Copyright (c) 2015-2017 The Linux Foundation. All rights reserved.
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
#include "sdhci.h"
#include "sdhci-msm.h"

#define DCMD_SLOT 31
#define NUM_SLOTS 32

/* 10 sec */
#define HALT_TIMEOUT_MS 10000

static int cmdq_halt_poll(struct mmc_host *mmc, bool halt);
static int cmdq_halt(struct mmc_host *mmc, bool halt);

#ifdef CONFIG_PM_RUNTIME
static int cmdq_runtime_pm_get(struct cmdq_host *host)
{
	return pm_runtime_get_sync(host->mmc->parent);
}
static int cmdq_runtime_pm_put(struct cmdq_host *host)
{
	pm_runtime_mark_last_busy(host->mmc->parent);
	return pm_runtime_put_autosuspend(host->mmc->parent);
}
#else
static inline int cmdq_runtime_pm_get(struct cmdq_host *host)
{
	return 0;
}
static inline int cmdq_runtime_pm_put(struct cmdq_host *host)
{
	return 0;
}
#endif
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
		(cq_host->mmc->max_segs * tag *
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
	u32 ier;

	ier = cmdq_readl(cq_host, CQISTE);
	ier &= ~clear;
	ier |= set;
	cmdq_writel(cq_host, ier, CQISTE);
	cmdq_writel(cq_host, ier, CQISGE);
	/* ensure the writes are done */
	mb();
}


#define DRV_NAME "cmdq-host"

static void cmdq_dump_task_history(struct cmdq_host *cq_host)
{
	int i;

	if (likely(!cq_host->mmc->cmdq_thist_enabled))
		return;

	if (!cq_host->thist) {
		pr_err("%s: %s: CMDQ task history buffer not allocated\n",
			mmc_hostname(cq_host->mmc), __func__);
		return;
	}

	pr_err("---- Circular Task History ----\n");
	pr_err(DRV_NAME ": Last entry index: %d", cq_host->thist_idx - 1);

	for (i = 0; i < cq_host->num_slots; i++) {
		pr_err(DRV_NAME ": [%02d]%s Task: 0x%08x | Args: 0x%08x\n", i,
			(cq_host->thist[i].is_dcmd) ? "DCMD" : "DATA",
			lower_32_bits(cq_host->thist[i].task),
			upper_32_bits(cq_host->thist[i].task));
	}
	pr_err("-------------------------\n");
}

static void cmdq_dump_adma_mem(struct cmdq_host *cq_host)
{
	struct mmc_host *mmc = cq_host->mmc;
	dma_addr_t desc_dma;
	int tag = 0;
	unsigned long data_active_reqs =
		mmc->cmdq_ctx.data_active_reqs;
	unsigned long desc_size =
		(cq_host->mmc->max_segs * cq_host->trans_desc_len);

	for_each_set_bit(tag, &data_active_reqs, cq_host->num_slots) {
		desc_dma = get_trans_desc_dma(cq_host, tag);
		pr_err("%s: %s: tag = %d, trans_dma(phys) = %pad, trans_desc(virt) = 0x%p\n",
				mmc_hostname(mmc), __func__, tag,
				&desc_dma, get_trans_desc(cq_host, tag));
		print_hex_dump(KERN_ERR, "cmdq-adma:", DUMP_PREFIX_ADDRESS,
				32, 8, get_trans_desc(cq_host, tag),
				(desc_size), false);
	}
}

static void cmdq_dumpregs(struct cmdq_host *cq_host)
{
	struct mmc_host *mmc = cq_host->mmc;

	MMC_TRACE(mmc,
	"%s: 0x0C=0x%08x 0x10=0x%08x 0x14=0x%08x 0x18=0x%08x 0x28=0x%08x 0x2C=0x%08x 0x30=0x%08x 0x34=0x%08x 0x54=0x%08x 0x58=0x%08x 0x5C=0x%08x 0x48=0x%08x\n",
	__func__, cmdq_readl(cq_host, CQCTL), cmdq_readl(cq_host, CQIS),
	cmdq_readl(cq_host, CQISTE), cmdq_readl(cq_host, CQISGE),
	cmdq_readl(cq_host, CQTDBR), cmdq_readl(cq_host, CQTCN),
	cmdq_readl(cq_host, CQDQS), cmdq_readl(cq_host, CQDPT),
	cmdq_readl(cq_host, CQTERRI), cmdq_readl(cq_host, CQCRI),
	cmdq_readl(cq_host, CQCRA), cmdq_readl(cq_host, CQCRDCT));
	pr_err(DRV_NAME ": ========== REGISTER DUMP (%s)==========\n",
		mmc_hostname(mmc));

	pr_err(DRV_NAME ": Caps: 0x%08x		  | Version:  0x%08x\n",
		cmdq_readl(cq_host, CQCAP),
		cmdq_readl(cq_host, CQVER));
	pr_err(DRV_NAME ": Queing config: 0x%08x  | Queue Ctrl:  0x%08x\n",
		cmdq_readl(cq_host, CQCFG),
		cmdq_readl(cq_host, CQCTL));
	pr_err(DRV_NAME ": Int stat: 0x%08x	  | Int enab:  0x%08x\n",
		cmdq_readl(cq_host, CQIS),
		cmdq_readl(cq_host, CQISTE));
	pr_err(DRV_NAME ": Int sig: 0x%08x	  | Int Coal:  0x%08x\n",
		cmdq_readl(cq_host, CQISGE),
		cmdq_readl(cq_host, CQIC));
	pr_err(DRV_NAME ": TDL base: 0x%08x	  | TDL up32:  0x%08x\n",
		cmdq_readl(cq_host, CQTDLBA),
		cmdq_readl(cq_host, CQTDLBAU));
	pr_err(DRV_NAME ": Doorbell: 0x%08x	  | Comp Notif:  0x%08x\n",
		cmdq_readl(cq_host, CQTDBR),
		cmdq_readl(cq_host, CQTCN));
	pr_err(DRV_NAME ": Dev queue: 0x%08x	  | Dev Pend:  0x%08x\n",
		cmdq_readl(cq_host, CQDQS),
		cmdq_readl(cq_host, CQDPT));
	pr_err(DRV_NAME ": Task clr: 0x%08x	  | Send stat 1:  0x%08x\n",
		cmdq_readl(cq_host, CQTCLR),
		cmdq_readl(cq_host, CQSSC1));
	pr_err(DRV_NAME ": Send stat 2: 0x%08x	  | DCMD resp:  0x%08x\n",
		cmdq_readl(cq_host, CQSSC2),
		cmdq_readl(cq_host, CQCRDCT));
	pr_err(DRV_NAME ": Resp err mask: 0x%08x  | Task err:  0x%08x\n",
		cmdq_readl(cq_host, CQRMEM),
		cmdq_readl(cq_host, CQTERRI));
	pr_err(DRV_NAME ": Resp idx 0x%08x	  | Resp arg:  0x%08x\n",
		cmdq_readl(cq_host, CQCRI),
		cmdq_readl(cq_host, CQCRA));
	pr_err(DRV_NAME": Vendor cfg 0x%08x\n",
	       cmdq_readl(cq_host, CQ_VENDOR_CFG));
	pr_err(DRV_NAME ": ===========================================\n");

	cmdq_dump_task_history(cq_host);
	if (cq_host->ops->dump_vendor_regs)
		cq_host->ops->dump_vendor_regs(mmc);
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

	size_t desc_size;
	size_t data_size;
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

	desc_size = cq_host->slot_sz * cq_host->num_slots;

	data_size = cq_host->trans_desc_len * cq_host->mmc->max_segs *
		(cq_host->num_slots - 1);

	pr_info("%s: desc_size: %d data_sz: %d slot-sz: %d\n", __func__,
		(int)desc_size, (int)data_size, cq_host->slot_sz);

	/*
	 * allocate a dma-mapped chunk of memory for the descriptors
	 * allocate a dma-mapped chunk of memory for link descriptors
	 * setup each link-desc memory offset per slot-number to
	 * the descriptor table.
	 */
	cq_host->desc_base = dmam_alloc_coherent(mmc_dev(cq_host->mmc),
						 desc_size,
						 &cq_host->desc_dma_base,
						 GFP_KERNEL);
	cq_host->trans_desc_base = dmam_alloc_coherent(mmc_dev(cq_host->mmc),
					      data_size,
					      &cq_host->trans_desc_dma_base,
					      GFP_KERNEL);
	cq_host->thist = devm_kzalloc(mmc_dev(cq_host->mmc),
					(sizeof(*cq_host->thist) *
						cq_host->num_slots),
					GFP_KERNEL);
	if (!cq_host->desc_base || !cq_host->trans_desc_base)
		return -ENOMEM;

	pr_info("desc-base: 0x%p trans-base: 0x%p\n desc_dma 0x%llx trans_dma: 0x%llx\n",
		 cq_host->desc_base, cq_host->trans_desc_base,
		(unsigned long long)cq_host->desc_dma_base,
		(unsigned long long) cq_host->trans_desc_dma_base);

	for (; i < (cq_host->num_slots); i++)
		setup_trans_desc(cq_host, i);

	return 0;
}

static int cmdq_enable(struct mmc_host *mmc)
{
	int err = 0;
	u32 cqcfg;
	u32 cqcap = 0;
	bool dcmd_enable;
	struct cmdq_host *cq_host = mmc_cmdq_private(mmc);

	if (!cq_host || !mmc->card || !mmc_card_cmdq(mmc->card)) {
		err = -EINVAL;
		goto out;
	}

	if (cq_host->enabled)
		goto out;

	cmdq_runtime_pm_get(cq_host);
	cqcfg = cmdq_readl(cq_host, CQCFG);
	if (cqcfg & 0x1) {
		pr_info("%s: %s: cq_host is already enabled\n",
				mmc_hostname(mmc), __func__);
		WARN_ON(1);
		goto pm_ref_count;
	}

	if (cq_host->quirks & CMDQ_QUIRK_NO_DCMD)
		dcmd_enable = false;
	else
		dcmd_enable = true;

	cqcfg = ((cq_host->caps & CMDQ_TASK_DESC_SZ_128 ? CQ_TASK_DESC_SZ : 0) |
			(dcmd_enable ? CQ_DCMD : 0));

	cqcap = cmdq_readl(cq_host, CQCAP);
	if (cqcap & CQCAP_CS) {
		/*
		 * In case host controller supports cryptographic operations
		 * then, it uses 128bit task descriptor. Upper 64 bits of task
		 * descriptor would be used to pass crypto specific informaton.
		 */
		cq_host->caps |= CMDQ_CAP_CRYPTO_SUPPORT |
				 CMDQ_TASK_DESC_SZ_128;
		cqcfg |= CQ_ICE_ENABLE;
	}

	cmdq_writel(cq_host, cqcfg, CQCFG);
	/* enable CQ_HOST */
	cmdq_writel(cq_host, cmdq_readl(cq_host, CQCFG) | CQ_ENABLE,
		    CQCFG);

	if (!cq_host->desc_base ||
			!cq_host->trans_desc_base) {
		err = cmdq_host_alloc_tdl(cq_host);
		if (err)
			goto pm_ref_count;
	}

	cmdq_writel(cq_host, lower_32_bits(cq_host->desc_dma_base), CQTDLBA);
	cmdq_writel(cq_host, upper_32_bits(cq_host->desc_dma_base), CQTDLBAU);

	/*
	 * disable all vendor interrupts
	 * enable CMDQ interrupts
	 * enable the vendor error interrupts
	 */
	if (cq_host->ops->clear_set_irqs)
		cq_host->ops->clear_set_irqs(mmc, true);

	cmdq_clear_set_irqs(cq_host, 0x0, CQ_INT_ALL);

	/* cq_host would use this rca to address the card */
	cmdq_writel(cq_host, mmc->card->rca, CQSSC2);

	/* send QSR at lesser intervals than the default */
	cmdq_writel(cq_host, SEND_QSR_INTERVAL, CQSSC1);

	/* enable bkops exception indication */
	if (mmc_card_configured_manual_bkops(mmc->card) &&
	    !mmc_card_configured_auto_bkops(mmc->card))
		cmdq_writel(cq_host, cmdq_readl(cq_host, CQRMEM) | CQ_EXCEPTION,
				CQRMEM);

	/* ensure the writes are done before enabling CQE */
	mb();

	cq_host->enabled = true;
	mmc_host_clr_cq_disable(mmc);

	if (cq_host->ops->set_transfer_params)
		cq_host->ops->set_transfer_params(mmc);

	if (cq_host->ops->set_block_size)
		cq_host->ops->set_block_size(cq_host->mmc);

	if (cq_host->ops->set_data_timeout)
		cq_host->ops->set_data_timeout(mmc, 0xf);

	if (cq_host->ops->clear_set_dumpregs)
		cq_host->ops->clear_set_dumpregs(mmc, 1);

	if (cq_host->ops->enhanced_strobe_mask)
		cq_host->ops->enhanced_strobe_mask(mmc, true);

pm_ref_count:
	cmdq_runtime_pm_put(cq_host);
out:
	MMC_TRACE(mmc, "%s: CQ enabled err: %d\n", __func__, err);
	return err;
}

static void cmdq_disable_nosync(struct mmc_host *mmc, bool soft)
{
	struct cmdq_host *cq_host = (struct cmdq_host *)mmc_cmdq_private(mmc);

	if (soft) {
		cmdq_writel(cq_host, cmdq_readl(
				    cq_host, CQCFG) & ~(CQ_ENABLE),
			    CQCFG);
	}
	if (cq_host->ops->enhanced_strobe_mask)
		cq_host->ops->enhanced_strobe_mask(mmc, false);

	cq_host->enabled = false;
	mmc_host_set_cq_disable(mmc);
	MMC_TRACE(mmc, "%s: CQ disabled\n", __func__);
}

static void cmdq_disable(struct mmc_host *mmc, bool soft)
{
	struct cmdq_host *cq_host = (struct cmdq_host *)mmc_cmdq_private(mmc);

	cmdq_runtime_pm_get(cq_host);
	cmdq_disable_nosync(mmc, soft);
	cmdq_runtime_pm_put(cq_host);
}

static void cmdq_reset(struct mmc_host *mmc, bool soft)
{
	struct cmdq_host *cq_host = (struct cmdq_host *)mmc_cmdq_private(mmc);
	unsigned int cqcfg;
	unsigned int tdlba;
	unsigned int tdlbau;
	unsigned int rca;
	int ret;

	cmdq_runtime_pm_get(cq_host);
	cqcfg = cmdq_readl(cq_host, CQCFG);
	tdlba = cmdq_readl(cq_host, CQTDLBA);
	tdlbau = cmdq_readl(cq_host, CQTDLBAU);
	rca = cmdq_readl(cq_host, CQSSC2);

	cmdq_disable(mmc, true);

	if (cq_host->ops->reset) {
		ret = cq_host->ops->reset(mmc);
		if (ret) {
			pr_crit("%s: reset CMDQ controller: failed\n",
				mmc_hostname(mmc));
			BUG();
		}
	}

	cmdq_writel(cq_host, tdlba, CQTDLBA);
	cmdq_writel(cq_host, tdlbau, CQTDLBAU);

	if (cq_host->ops->clear_set_irqs)
		cq_host->ops->clear_set_irqs(mmc, true);

	cmdq_clear_set_irqs(cq_host, 0x0, CQ_INT_ALL);

	/* cq_host would use this rca to address the card */
	cmdq_writel(cq_host, rca, CQSSC2);

	/* ensure the writes are done before enabling CQE */
	mb();

	cmdq_writel(cq_host, cqcfg, CQCFG);
	cmdq_runtime_pm_put(cq_host);
	cq_host->enabled = true;
	mmc_host_clr_cq_disable(mmc);
}

static void cmdq_prep_task_desc(struct mmc_request *mrq,
					u64 *data, bool intr, bool qbr)
{
	struct mmc_cmdq_req *cmdq_req = mrq->cmdq_req;
	u32 req_flags = cmdq_req->cmdq_req_flags;

	pr_debug("%s: %s: data-tag: 0x%08x - dir: %d - prio: %d - cnt: 0x%08x -	addr: 0x%llx\n",
		 mmc_hostname(mrq->host), __func__,
		 !!(req_flags & DAT_TAG), !!(req_flags & DIR),
		 !!(req_flags & PRIO), cmdq_req->data.blocks,
		 (u64)mrq->cmdq_req->blk_addr);

	*data = VALID(1) |
		END(1) |
		INT(intr) |
		ACT(0x5) |
		FORCED_PROG(!!(req_flags & FORCED_PRG)) |
		CONTEXT(mrq->cmdq_req->ctx_id) |
		DATA_TAG(!!(req_flags & DAT_TAG)) |
		DATA_DIR(!!(req_flags & DIR)) |
		PRIORITY(!!(req_flags & PRIO)) |
		QBAR(qbr) |
		REL_WRITE(!!(req_flags & REL_WR)) |
		BLK_COUNT(mrq->cmdq_req->data.blocks) |
		BLK_ADDR((u64)mrq->cmdq_req->blk_addr);

	MMC_TRACE(mrq->host,
		"%s: Task: 0x%08x | Args: 0x%08x | cnt: 0x%08x\n", __func__,
		lower_32_bits(*data),
		upper_32_bits(*data),
		mrq->cmdq_req->data.blocks);
}

static int cmdq_dma_map(struct mmc_host *host, struct mmc_request *mrq)
{
	int sg_count;
	struct mmc_data *data = mrq->data;

	if (!data)
		return -EINVAL;

	sg_count = dma_map_sg(mmc_dev(host), data->sg,
			      data->sg_len,
			      (data->flags & MMC_DATA_WRITE) ?
			      DMA_TO_DEVICE : DMA_FROM_DEVICE);
	if (!sg_count) {
		pr_err("%s: sg-len: %d\n", __func__, data->sg_len);
		return -ENOMEM;
	}

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
	int i, sg_count, len;
	bool end = false;
	dma_addr_t addr;
	u8 *desc;
	struct scatterlist *sg;

	sg_count = cmdq_dma_map(mrq->host, mrq);
	if (sg_count < 0) {
		pr_err("%s: %s: unable to map sg lists, %d\n",
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

	pr_debug("%s: req: 0x%p tag: %d calc_trans_des: 0x%p sg-cnt: %d\n",
		__func__, mrq->req, tag, desc, sg_count);

	return 0;
}

static void cmdq_log_task_desc_history(struct cmdq_host *cq_host, u64 task,
					bool is_dcmd)
{
	if (likely(!cq_host->mmc->cmdq_thist_enabled))
		return;

	if (!cq_host->thist) {
		pr_err("%s: %s: CMDQ task history buffer not allocated\n",
			mmc_hostname(cq_host->mmc), __func__);
		return;
	}

	if (cq_host->thist_idx >= cq_host->num_slots)
		cq_host->thist_idx = 0;

	cq_host->thist[cq_host->thist_idx].is_dcmd = is_dcmd;
	memcpy(&cq_host->thist[cq_host->thist_idx++].task,
		&task, cq_host->task_desc_len);
}

static void cmdq_prep_dcmd_desc(struct mmc_host *mmc,
				   struct mmc_request *mrq)
{
	u64 *task_desc = NULL;
	u64 data = 0;
	u8 resp_type;
	u8 *desc;
	__le64 *dataddr;
	struct cmdq_host *cq_host = mmc_cmdq_private(mmc);
	u8 timing;

	if (!(mrq->cmd->flags & MMC_RSP_PRESENT)) {
		resp_type = 0x0;
		timing = 0x1;
	} else {
		if (mrq->cmd->flags & MMC_RSP_BUSY) {
			resp_type = 0x3;
			timing = 0x0;
		} else {
			resp_type = 0x2;
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
	pr_debug("cmdq: dcmd: cmd: %d timing: %d resp: %d\n",
		mrq->cmd->opcode, timing, resp_type);
	dataddr = (__le64 __force *)(desc + 4);
	dataddr[0] = cpu_to_le64((u64)mrq->cmd->arg);
	cmdq_log_task_desc_history(cq_host, *task_desc, true);
	MMC_TRACE(mrq->host,
		"%s: DCMD: Task: 0x%08x | Args: 0x%08x\n",
		__func__,
		lower_32_bits(*task_desc),
		upper_32_bits(*task_desc));
}

static inline
void cmdq_prep_crypto_desc(struct cmdq_host *cq_host, u64 *task_desc,
			u64 ice_ctx)
{
	u64 *ice_desc = NULL;

	if (cq_host->caps & CMDQ_CAP_CRYPTO_SUPPORT) {
		/*
		 * Get the address of ice context for the given task descriptor.
		 * ice context is present in the upper 64bits of task descriptor
		 * ice_conext_base_address = task_desc + 8-bytes
		 */
		ice_desc = (__le64 __force *)((u8 *)task_desc +
						CQ_TASK_DESC_TASK_PARAMS_SIZE);
		memset(ice_desc, 0, CQ_TASK_DESC_ICE_PARAMS_SIZE);

		/*
		 *  Assign upper 64bits data of task descritor with ice context
		 */
		if (ice_ctx)
			*ice_desc = cpu_to_le64(ice_ctx);
	}
}

static void cmdq_pm_qos_vote(struct sdhci_host *host, struct mmc_request *mrq)
{
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct sdhci_msm_host *msm_host = pltfm_host->priv;

	sdhci_msm_pm_qos_cpu_vote(host,
		msm_host->pdata->pm_qos_data.cmdq_latency, mrq->req->cpu);
}

static void cmdq_pm_qos_unvote(struct sdhci_host *host, struct mmc_request *mrq)
{
	/* use async as we're inside an atomic context (soft-irq) */
	sdhci_msm_pm_qos_cpu_unvote(host, mrq->req->cpu, true);
}

static int cmdq_request(struct mmc_host *mmc, struct mmc_request *mrq)
{
	int err = 0;
	u64 data = 0;
	u64 *task_desc = NULL;
	u32 tag = mrq->cmdq_req->tag;
	struct cmdq_host *cq_host = (struct cmdq_host *)mmc_cmdq_private(mmc);
	struct sdhci_host *host = mmc_priv(mmc);
	u64 ice_ctx = 0;

	if (!cq_host->enabled) {
		pr_err("%s: CMDQ host not enabled yet !!!\n",
		       mmc_hostname(mmc));
		err = -EINVAL;
		goto out;
	}

	cmdq_runtime_pm_get(cq_host);

	if (mrq->cmdq_req->cmdq_req_flags & DCMD) {
		cmdq_prep_dcmd_desc(mmc, mrq);
		cq_host->mrq_slot[DCMD_SLOT] = mrq;
		/* DCMD's are always issued on a fixed slot */
		tag = DCMD_SLOT;
		goto ring_doorbell;
	}

	if (cq_host->ops->crypto_cfg) {
		err = cq_host->ops->crypto_cfg(mmc, mrq, tag, &ice_ctx);
		if (err) {
			pr_err("%s: failed to configure crypto: err %d tag %d\n",
					mmc_hostname(mmc), err, tag);
			goto out;
		}
	}

	task_desc = (__le64 __force *)get_desc(cq_host, tag);

	cmdq_prep_task_desc(mrq, &data, 1,
			    (mrq->cmdq_req->cmdq_req_flags & QBR));
	*task_desc = cpu_to_le64(data);

	cmdq_prep_crypto_desc(cq_host, task_desc, ice_ctx);

	cmdq_log_task_desc_history(cq_host, *task_desc, false);

	err = cmdq_prep_tran_desc(mrq, cq_host, tag);
	if (err) {
		pr_err("%s: %s: failed to setup tx desc: %d\n",
		       mmc_hostname(mmc), __func__, err);
		goto out;
	}

	cq_host->mrq_slot[tag] = mrq;

	/* PM QoS */
	sdhci_msm_pm_qos_irq_vote(host);
	cmdq_pm_qos_vote(host, mrq);
ring_doorbell:
	/* Ensure the task descriptor list is flushed before ringing doorbell */
	wmb();
	if (cmdq_readl(cq_host, CQTDBR) & (1 << tag)) {
		cmdq_dumpregs(cq_host);
		BUG_ON(1);
	}
	MMC_TRACE(mmc, "%s: tag: %d\n", __func__, tag);
	cmdq_writel(cq_host, 1 << tag, CQTDBR);
	/* Commit the doorbell write immediately */
	wmb();

out:
	return err;
}

static void cmdq_finish_data(struct mmc_host *mmc, unsigned int tag)
{
	struct mmc_request *mrq;
	struct cmdq_host *cq_host = (struct cmdq_host *)mmc_cmdq_private(mmc);

	mrq = get_req_by_tag(cq_host, tag);
	if (tag == cq_host->dcmd_slot)
		mrq->cmd->resp[0] = cmdq_readl(cq_host, CQCRDCT);

	if (mrq->cmdq_req->cmdq_req_flags & DCMD)
		cmdq_writel(cq_host, cmdq_readl(cq_host, CQ_VENDOR_CFG) |
			    CMDQ_SEND_STATUS_TRIGGER, CQ_VENDOR_CFG);

	cmdq_runtime_pm_put(cq_host);
	if (!(cq_host->caps & CMDQ_CAP_CRYPTO_SUPPORT) &&
			cq_host->ops->crypto_cfg_reset)
		cq_host->ops->crypto_cfg_reset(mmc, tag);
	mrq->done(mrq);
}

irqreturn_t cmdq_irq(struct mmc_host *mmc, int err)
{
	u32 status;
	unsigned long tag = 0, comp_status;
	struct cmdq_host *cq_host = (struct cmdq_host *)mmc_cmdq_private(mmc);
	unsigned long err_info = 0;
	struct mmc_request *mrq;
	int ret;
	u32 dbr_set = 0;

	status = cmdq_readl(cq_host, CQIS);

	if (!status && !err)
		return IRQ_NONE;
	MMC_TRACE(mmc, "%s: CQIS: 0x%x err: %d\n",
		__func__, status, err);

	if (err || (status & CQIS_RED)) {
		err_info = cmdq_readl(cq_host, CQTERRI);
		pr_err("%s: err: %d status: 0x%08x task-err-info (0x%08lx)\n",
		       mmc_hostname(mmc), err, status, err_info);

		/*
		 * Need to halt CQE in case of error in interrupt context itself
		 * otherwise CQE may proceed with sending CMD to device even if
		 * CQE/card is in error state.
		 * CMDQ error handling will make sure that it is unhalted after
		 * handling all the errors.
		 */
		ret = cmdq_halt_poll(mmc, true);
		if (ret)
			pr_err("%s: %s: halt failed ret=%d\n",
					mmc_hostname(mmc), __func__, ret);

		/*
		 * Clear the CQIS after halting incase of error. This is done
		 * because if CQIS is cleared before halting, the CQ will
		 * continue with issueing commands for rest of requests with
		 * Doorbell rung. This will overwrite the Resp Arg register.
		 * So CQ must be halted first and then CQIS cleared incase
		 * of error
		 */
		cmdq_writel(cq_host, status, CQIS);

		cmdq_dumpregs(cq_host);

		if (!err_info) {
			/*
			 * It may so happen sometimes for few errors(like ADMA)
			 * that HW cannot give CQTERRI info.
			 * Thus below is a HW WA for recovering from such
			 * scenario.
			 * - To halt/disable CQE and do reset_all.
			 *   Since there is no way to know which tag would
			 *   have caused such error, so check for any first
			 *   bit set in doorbell and proceed with an error.
			 */
			dbr_set = cmdq_readl(cq_host, CQTDBR);
			if (!dbr_set) {
				pr_err("%s: spurious/force error interrupt\n",
						mmc_hostname(mmc));
				cmdq_halt_poll(mmc, false);
				mmc_host_clr_halt(mmc);
				return IRQ_HANDLED;
			}

			tag = ffs(dbr_set) - 1;
			pr_err("%s: error tag selected: tag = %lu\n",
					mmc_hostname(mmc), tag);
			mrq = get_req_by_tag(cq_host, tag);
			if (mrq->data)
				mrq->data->error = err;
			else
				mrq->cmd->error = err;
			/*
			 * Get ADMA descriptor memory in case of ADMA
			 * error for debug.
			 */
			if (err == -EIO)
				cmdq_dump_adma_mem(cq_host);
			goto skip_cqterri;
		}

		if (err_info & CQ_RMEFV) {
			tag = GET_CMD_ERR_TAG(err_info);
			pr_err("%s: CMD err tag: %lu\n", __func__, tag);

			mrq = get_req_by_tag(cq_host, tag);
			/* CMD44/45/46/47 will not have a valid cmd */
			if (mrq->cmd)
				mrq->cmd->error = err;
			else
				mrq->data->error = err;
		} else if (err_info & CQ_DTEFV) {
			tag = GET_DAT_ERR_TAG(err_info);
			pr_err("%s: Dat err  tag: %lu\n", __func__, tag);
			mrq = get_req_by_tag(cq_host, tag);
			mrq->data->error = err;
		}

skip_cqterri:
		/*
		 * If CQE halt fails then, disable CQE
		 * from processing any further requests
		 */
		if (ret) {
			cmdq_disable_nosync(mmc, true);
			/*
			 * Enable legacy interrupts as CQE halt has failed.
			 * This is needed to send legacy commands like status
			 * cmd as part of error handling work.
			 */
			if (cq_host->ops->clear_set_irqs)
				cq_host->ops->clear_set_irqs(mmc, false);
		}

		/*
		 * CQE detected a reponse error from device
		 * In most cases, this would require a reset.
		 */
		if (status & CQIS_RED) {
			/*
			 * will check if the RED error is due to a bkops
			 * exception once the queue is empty
			 */
			BUG_ON(!mmc->card);
			if (mmc_card_configured_manual_bkops(mmc->card) ||
			    mmc_card_configured_auto_bkops(mmc->card))
				mmc->card->bkops.needs_check = true;

			mrq->cmdq_req->resp_err = true;
			pr_err("%s: Response error (0x%08x) from card !!!",
				mmc_hostname(mmc), cmdq_readl(cq_host, CQCRA));

		} else {
			mrq->cmdq_req->resp_idx = cmdq_readl(cq_host, CQCRI);
			mrq->cmdq_req->resp_arg = cmdq_readl(cq_host, CQCRA);
		}

		cmdq_finish_data(mmc, tag);
	} else {
		cmdq_writel(cq_host, status, CQIS);
	}

	if (status & CQIS_TCC) {
		/* read CQTCN and complete the request */
		comp_status = cmdq_readl(cq_host, CQTCN);
		if (!comp_status)
			goto out;
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
			pr_debug("%s: completing tag -> %lu\n",
				 mmc_hostname(mmc), tag);
			MMC_TRACE(mmc, "%s: completing tag -> %lu\n",
				__func__, tag);
				cmdq_finish_data(mmc, tag);
		}
	}

	if (status & CQIS_HAC) {
		if (cq_host->ops->post_cqe_halt)
			cq_host->ops->post_cqe_halt(mmc);
		/* halt done: re-enable legacy interrupts */
		if (cq_host->ops->clear_set_irqs)
			cq_host->ops->clear_set_irqs(mmc, false);
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

	if (!halt) {
		if (cq_host->ops->set_data_timeout)
			cq_host->ops->set_data_timeout(mmc, 0xf);
		if (cq_host->ops->clear_set_irqs)
			cq_host->ops->clear_set_irqs(mmc, true);
		cmdq_writel(cq_host, cmdq_readl(cq_host, CQCTL) & ~HALT,
			    CQCTL);
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
				cq_host->ops->clear_set_irqs(mmc,
							false);
			mmc_host_set_halt(mmc);
			break;
		}
	}
	cmdq_set_halt_irq(cq_host, true);
	return retries ? 0 : -ETIMEDOUT;
}

/* May sleep */
static int cmdq_halt(struct mmc_host *mmc, bool halt)
{
	struct cmdq_host *cq_host = (struct cmdq_host *)mmc_cmdq_private(mmc);
	u32 ret = 0;
	u32 config = 0;
	int retries = 3;

	cmdq_runtime_pm_get(cq_host);
	if (halt) {
		while (retries) {
			cmdq_writel(cq_host, cmdq_readl(cq_host, CQCTL) | HALT,
				    CQCTL);
			ret = wait_for_completion_timeout(&cq_host->halt_comp,
					  msecs_to_jiffies(HALT_TIMEOUT_MS));
			if (!ret) {
				pr_warn("%s: %s: HAC int timeout\n",
					mmc_hostname(mmc), __func__);
				if ((cmdq_readl(cq_host, CQCTL) & HALT)) {
					/*
					 * Don't retry if CQE is halted but irq
					 * is not triggered in timeout period.
					 * And since we are returning error,
					 * un-halt CQE. Since irq was not fired
					 * yet, no need to set other params
					 */
					retries = 0;
					config = cmdq_readl(cq_host, CQCTL);
					config &= ~HALT;
					cmdq_writel(cq_host, config, CQCTL);
				} else {
					pr_warn("%s: %s: retryng halt (%d)\n",
						mmc_hostname(mmc), __func__,
						retries);
					retries--;
					continue;
				}
			} else {
				MMC_TRACE(mmc, "%s: halt done , retries: %d\n",
					__func__, retries);
				break;
			}
		}
		ret = retries ? 0 : -ETIMEDOUT;
	} else {
		if (cq_host->ops->set_transfer_params)
			cq_host->ops->set_transfer_params(mmc);
		if (cq_host->ops->set_block_size)
			cq_host->ops->set_block_size(mmc);
		if (cq_host->ops->set_data_timeout)
			cq_host->ops->set_data_timeout(mmc, 0xf);
		if (cq_host->ops->clear_set_irqs)
			cq_host->ops->clear_set_irqs(mmc, true);
		MMC_TRACE(mmc, "%s: unhalt done\n", __func__);
		cmdq_writel(cq_host, cmdq_readl(cq_host, CQCTL) & ~HALT,
			    CQCTL);
	}
	cmdq_runtime_pm_put(cq_host);
	return ret;
}

static void cmdq_post_req(struct mmc_host *mmc, int tag, int err)
{
	struct cmdq_host *cq_host;
	struct mmc_request *mrq;
	struct mmc_data *data;
	struct sdhci_host *sdhci_host = mmc_priv(mmc);

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

		/* we're in atomic context (soft-irq) so unvote async. */
		sdhci_msm_pm_qos_irq_unvote(sdhci_host, true);
		cmdq_pm_qos_unvote(sdhci_host, mrq);
	}
}

static void cmdq_dumpstate(struct mmc_host *mmc)
{
	struct cmdq_host *cq_host = (struct cmdq_host *)mmc_cmdq_private(mmc);
	cmdq_runtime_pm_get(cq_host);
	cmdq_dumpregs(cq_host);
	cmdq_runtime_pm_put(cq_host);
}

static int cmdq_late_init(struct mmc_host *mmc)
{
	struct sdhci_host *host = mmc_priv(mmc);
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct sdhci_msm_host *msm_host = pltfm_host->priv;

	/*
	 * TODO: This should basically move to something like "sdhci-cmdq-msm"
	 * for msm specific implementation.
	 */
	sdhci_msm_pm_qos_irq_init(host);

	if (msm_host->pdata->pm_qos_data.cmdq_valid)
		sdhci_msm_pm_qos_cpu_init(host,
			msm_host->pdata->pm_qos_data.cmdq_latency);
	return 0;
}

static const struct mmc_cmdq_host_ops cmdq_host_ops = {
	.init = cmdq_late_init,
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
	struct cmdq_host *cq_host;
	struct resource *cmdq_memres = NULL;

	/* check and setup CMDQ interface */
	cmdq_memres = platform_get_resource_byname(pdev, IORESOURCE_MEM,
						   "cmdq_mem");
	if (!cmdq_memres) {
		dev_dbg(&pdev->dev, "CMDQ not supported\n");
		return ERR_PTR(-EINVAL);
	}

	cq_host = kzalloc(sizeof(*cq_host), GFP_KERNEL);
	if (!cq_host) {
		dev_err(&pdev->dev, "failed to allocate memory for CMDQ\n");
		return ERR_PTR(-ENOMEM);
	}
	cq_host->mmio = devm_ioremap(&pdev->dev,
				     cmdq_memres->start,
				     resource_size(cmdq_memres));
	if (!cq_host->mmio) {
		dev_err(&pdev->dev, "failed to remap cmdq regs\n");
		kfree(cq_host);
		return ERR_PTR(-EBUSY);
	}
	dev_dbg(&pdev->dev, "CMDQ ioremap: done\n");

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
