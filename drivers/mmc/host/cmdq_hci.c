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

#include "cmdq_hci.h"

#define DCMD_SLOT 31
#define NUM_SLOTS 32

/* 1 sec */
#define HALT_TIMEOUT_MS 1000

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

static void cmdq_dump_debug_ram(struct cmdq_host *cq_host)
{
	int i = 0;

	pr_err("---- Debug RAM dump ----\n");
	pr_err(DRV_NAME ": Debug RAM wrap-around: 0x%08x | Debug RAM overlap: 0x%08x\n",
	       cmdq_readl(cq_host, CQ_CMD_DBG_RAM_WA),
	       cmdq_readl(cq_host, CQ_CMD_DBG_RAM_OL));

	while (i < 16) {
		pr_err(DRV_NAME ": Debug RAM dump [%d]: 0x%08x\n", i,
		       cmdq_readl(cq_host, CQ_CMD_DBG_RAM + (0x4 * i)));
		i++;
	}
	pr_err("-------------------------\n");
}

static void cmdq_dumpregs(struct cmdq_host *cq_host)
{
	struct mmc_host *mmc = cq_host->mmc;

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
	pr_err(DRV_NAME ": ===========================================\n");

	cmdq_dump_debug_ram(cq_host);
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
	bool dcmd_enable;
	struct cmdq_host *cq_host = mmc_cmdq_private(mmc);

	if (!cq_host || !mmc->card || !mmc_card_cmdq(mmc->card)) {
		err = -EINVAL;
		goto out;
	}

	if (cq_host->enabled)
		goto out;

	cqcfg = cmdq_readl(cq_host, CQCFG);
	if (cqcfg & 0x1) {
		pr_info("%s: %s: cq_host is already enabled\n",
				mmc_hostname(mmc), __func__);
		WARN_ON(1);
		goto out;
	}

	if (cq_host->quirks & CMDQ_QUIRK_NO_DCMD)
		dcmd_enable = false;
	else
		dcmd_enable = true;

	cqcfg = ((cq_host->caps & CMDQ_TASK_DESC_SZ_128 ? CQ_TASK_DESC_SZ : 0) |
			(dcmd_enable ? CQ_DCMD : 0));

	cmdq_writel(cq_host, cqcfg, CQCFG);
	/* enable CQ_HOST */
	cmdq_writel(cq_host, cmdq_readl(cq_host, CQCFG) | CQ_ENABLE,
		    CQCFG);

	if (!cq_host->desc_base ||
			!cq_host->trans_desc_base) {
		err = cmdq_host_alloc_tdl(cq_host);
		if (err)
			goto out;
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
	cmdq_writel(cq_host, cmdq_readl(cq_host, CQSSC1) | SEND_QSR_INTERVAL,
				CQSSC1);

	/* ensure the writes are done before enabling CQE */
	mb();

	cq_host->enabled = true;

	if (cq_host->ops->set_block_size)
		cq_host->ops->set_block_size(cq_host->mmc);

	if (cq_host->ops->set_data_timeout)
		cq_host->ops->set_data_timeout(mmc, 0xf);

	if (cq_host->ops->clear_set_dumpregs)
		cq_host->ops->clear_set_dumpregs(mmc, 1);

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

	cq_host->enabled = false;
}

static void cmdq_reset(struct mmc_host *mmc, bool soft)
{
	struct cmdq_host *cq_host = (struct cmdq_host *)mmc_cmdq_private(mmc);
	unsigned int cqcfg;
	unsigned int tdlba;
	unsigned int tdlbau;
	unsigned int rca;
	int ret;

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
	cq_host->enabled = true;
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

static void cmdq_set_tran_desc(u8 *desc,
				 dma_addr_t addr, int len, bool end)
{
	__le64 *dataddr = (__le64 __force *)(desc + 4);
	__le32 *attr = (__le32 __force *)desc;

	*attr = (VALID(1) |
		 END(end ? 1 : 0) |
		 INT(0) |
		 ACT(0x4) |
		 DAT_LENGTH(len));

	dataddr[0] = cpu_to_le64(addr);
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
		cmdq_set_tran_desc(desc, addr, len, end);
		desc += cq_host->trans_desc_len;
	}

	pr_debug("%s: req: 0x%p tag: %d calc_trans_des: 0x%p sg-cnt: %d\n",
		__func__, mrq->req, tag, desc, sg_count);

	return 0;
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

}

static int cmdq_request(struct mmc_host *mmc, struct mmc_request *mrq)
{
	int err;
	u64 data = 0;
	u64 *task_desc = NULL;
	u32 tag = mrq->cmdq_req->tag;
	struct cmdq_host *cq_host = (struct cmdq_host *)mmc_cmdq_private(mmc);

	if (!cq_host->enabled) {
		pr_err("%s: CMDQ host not enabled yet !!!\n",
		       mmc_hostname(mmc));
		err = -EINVAL;
		goto out;
	}

	if (mrq->cmdq_req->cmdq_req_flags & DCMD) {
		cmdq_prep_dcmd_desc(mmc, mrq);
		cq_host->mrq_slot[DCMD_SLOT] = mrq;
		if (cq_host->ops->pm_qos_update)
			cq_host->ops->pm_qos_update(mmc, NULL, true);
		cmdq_writel(cq_host, 1 << DCMD_SLOT, CQTDBR);
		return 0;
	}

	if (cq_host->ops->crypto_cfg) {
		err = cq_host->ops->crypto_cfg(mmc, mrq, tag);
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

	err = cmdq_prep_tran_desc(mrq, cq_host, tag);
	if (err) {
		pr_err("%s: %s: failed to setup tx desc: %d\n",
		       mmc_hostname(mmc), __func__, err);
		return err;
	}

	if (cq_host->ops->pm_qos_update)
		cq_host->ops->pm_qos_update(mmc, NULL, true);

	BUG_ON(cmdq_readl(cq_host, CQTDBR) & (1 << tag));

	cq_host->mrq_slot[tag] = mrq;
	if (cq_host->ops->set_tranfer_params)
		cq_host->ops->set_tranfer_params(mmc);

	cmdq_writel(cq_host, 1 << tag, CQTDBR);

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

	mrq->done(mrq);
}

irqreturn_t cmdq_irq(struct mmc_host *mmc, int err)
{
	u32 status;
	unsigned long tag = 0, comp_status;
	struct cmdq_host *cq_host = (struct cmdq_host *)mmc_cmdq_private(mmc);
	unsigned long err_info = 0;
	struct mmc_request *mrq;

	status = cmdq_readl(cq_host, CQIS);
	cmdq_writel(cq_host, status, CQIS);

	if (!status && !err)
		return IRQ_NONE;

	if (err || (status & CQIS_RED)) {
		err_info = cmdq_readl(cq_host, CQTERRI);
		pr_err("%s: err: %d status: 0x%08x task-err-info (0x%08lx)\n",
		       mmc_hostname(mmc), err, status, err_info);

		cmdq_dumpregs(cq_host);

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

		tag = 0;
		/*
		 * CQE detected a reponse error from device
		 * In most cases, this would require a reset.
		 */
		if (status & CQIS_RED) {
			mrq->cmdq_req->resp_err = true;
			pr_err("%s: Response error (0x%08x) from card !!!",
					mmc_hostname(mmc), status);
		} else {
			mrq->cmdq_req->resp_idx = cmdq_readl(cq_host, CQCRI);
			mrq->cmdq_req->resp_arg = cmdq_readl(cq_host, CQCRA);
		}

		mmc->err_mrq = mrq;

		if (cq_host->ops->pm_qos_update)
			cq_host->ops->pm_qos_update(mmc, NULL, false);

		cmdq_finish_data(mmc, tag);
	}

	if (status & CQIS_TCC) {
		/* read QCTCN and complete the request */
		comp_status = cmdq_readl(cq_host, CQTCN);
		if (!comp_status)
			goto out;

		/*
		 * pm-qos for cmdq is removed only when there is no cmdq
		 * request been processed.
		 * Check if comp_status matches with the number of active_reqs.
		 * This means that all reqs got actually completed and there
		 * was no DCMD.
		 * But in case of DCMD, active_reqs mask has a bit set for DCMD
		 * as well, so ensure that the when comp_status bit is set
		 * for DCMD then there should not be any data_active_reqs in
		 * flight (which can happen if DCMD is not set with QBR)
		 */
		if (((mmc->cmdq_ctx).active_reqs == comp_status) ||
			       (((1 << 31) & comp_status) &&
				!((mmc->cmdq_ctx).data_active_reqs))) {
			if (cq_host->ops->pm_qos_update)
				cq_host->ops->pm_qos_update(mmc, NULL, false);
		}

		for_each_set_bit(tag, &comp_status, cq_host->num_slots) {
			/* complete the corresponding mrq */
			pr_debug("%s: completing tag -> %lu\n",
				 mmc_hostname(mmc), tag);
			cmdq_finish_data(mmc, tag);
		}
		cmdq_writel(cq_host, comp_status, CQTCN);
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

/* May sleep */
static int cmdq_halt(struct mmc_host *mmc, bool halt)
{
	struct cmdq_host *cq_host = (struct cmdq_host *)mmc_cmdq_private(mmc);
	u32 val;

	if (halt) {
		cmdq_writel(cq_host, cmdq_readl(cq_host, CQCTL) | HALT,
			    CQCTL);
		val = wait_for_completion_timeout(&cq_host->halt_comp,
					  msecs_to_jiffies(HALT_TIMEOUT_MS));
		/* halt done: re-enable legacy interrupts */
		if (cq_host->ops->clear_set_irqs)
			cq_host->ops->clear_set_irqs(mmc, false);

		return val ? 0 : -ETIMEDOUT;
	} else {
		if (cq_host->ops->set_data_timeout)
			cq_host->ops->set_data_timeout(mmc, 0xf);
		if (cq_host->ops->clear_set_irqs)
			cq_host->ops->clear_set_irqs(mmc, true);
		cmdq_writel(cq_host, cmdq_readl(cq_host, CQCTL) & ~HALT,
			    CQCTL);
	}

	return 0;
}

static void cmdq_post_req(struct mmc_host *host, struct mmc_request *mrq,
			  int err)
{
	struct mmc_data *data = mrq->data;

	if (data) {
		data->error = err;
		dma_unmap_sg(mmc_dev(host), data->sg, data->sg_len,
			     (data->flags & MMC_DATA_READ) ?
			     DMA_FROM_DEVICE : DMA_TO_DEVICE);
		if (err)
			data->bytes_xfered = 0;
		else
			data->bytes_xfered = blk_rq_bytes(mrq->req);
	}
}

static void cmdq_dumpstate(struct mmc_host *mmc)
{
	struct cmdq_host *cq_host = (struct cmdq_host *)mmc_cmdq_private(mmc);

	cmdq_dumpregs(cq_host);
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

	cq_host->mrq_slot = kzalloc(sizeof(cq_host->mrq_slot) *
				    cq_host->num_slots, GFP_KERNEL);
	if (!cq_host->mrq_slot)
		return -ENOMEM;

	init_completion(&cq_host->halt_comp);
	return err;
}
EXPORT_SYMBOL(cmdq_init);
