// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2022, Xiaomi, Inc. All rights reserved.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <uapi/linux/pci_regs.h>
#include <linux/io.h>
#include <linux/kthread.h>
#include <linux/sched.h>
#include <uapi/linux/sched/types.h>
#include "ispv4_pcie_hdma.h"
#include "ispv4_debugfs.h"
#include "media/ispv4_defs.h"

MODULE_IMPORT_NS(DMA_BUF);

static inline uint32_t ispv4_pcie_hdma_reg_read(struct pcie_hdma *hdma,
						uint32_t reg_addr)
{
	return readl_relaxed(hdma->base + reg_addr);
}

static inline void ispv4_pcie_hdma_reg_write(struct pcie_hdma *hdma,
					     uint32_t reg_addr,
					     uint32_t val)
{
	writel_relaxed(val, hdma->base + reg_addr);
}

static void ispv4_hdma_abort_handler(struct pcie_hdma *hdma,
				     struct pcie_hdma_chan_ctrl *chan_ctrl)
{
	struct device *dev = &hdma->pci_dev->dev;

	dev_info(dev, "pcie hdma transfer abort\n");
}

static void ispv4_hdma_watermark_handler(struct pcie_hdma *hdma,
					 struct pcie_hdma_chan_ctrl *chan_ctrl)
{
	struct device *dev = &hdma->pci_dev->dev;

	dev_info(dev, "pcie hdma ll transfer done\n");
}

static void ispv4_hdma_irq_comman_handler(struct pcie_hdma *hdma,
					 struct pcie_hdma_chan_ctrl *chan_ctrl)
{
	struct device *dev = &hdma->pci_dev->dev;

	if (chan_ctrl->int_status & HDMA_INT_STATUS_ABORT)
		ispv4_hdma_abort_handler(hdma, chan_ctrl);
	if (chan_ctrl->int_status & HDMA_INT_STATUS_WATERMARK)
		ispv4_hdma_watermark_handler(hdma, chan_ctrl);
	if (chan_ctrl->int_status & HDMA_INT_STATUS_STOP)
		dev_info(dev, "pcie hdma transfer done\n");
}

static irqreturn_t ispv4_hdma_irq_rd_ch_handler(int irq, void *dev_id)
{
	struct pcie_hdma *hdma = dev_id;
	struct pcie_hdma_chan_ctrl *chan_ctrl;
	enum pcie_hdma_dir dir = HDMA_FROM_DEVICE;
	uint8_t chan_id;

	for (chan_id = 0; chan_id < PCIE_HDMA_CHAN_NUM; chan_id++) {
		chan_ctrl = &hdma->r_chans[chan_id];

		chan_ctrl->int_status = ispv4_pcie_hdma_reg_read(hdma,
							HDMA_INT_STATUS(chan_id, dir));
		if (!chan_ctrl->int_status)
			continue;

		ispv4_hdma_irq_comman_handler(hdma, chan_ctrl);

		complete(&chan_ctrl->comp);
	}

	return IRQ_HANDLED;
}

#define DUMP_DMA_BUF
static irqreturn_t ispv4_hdma_irq_wr_ch_handler(int irq, void *dev_id)
{
	struct pcie_hdma *hdma = dev_id;
	struct pcie_hdma_chan_ctrl *chan_ctrl;
	enum pcie_hdma_dir dir = HDMA_TO_DEVICE;
	uint8_t chan_id;
#ifdef DUMP_DMA_BUF
	void *va = hdma->va + 0x10000;
	int i;

	for (i = 0; i < 40; i += 4)
		dev_info(&hdma->pci_dev->dev, "%x ", *(uint32_t *)(hdma->va + i));

	for (i = 0; i < 40; i += 4)
		dev_info(&hdma->pci_dev->dev, "%x ", *(uint32_t *)(va + i));
#endif

	for (chan_id = 0; chan_id < PCIE_HDMA_CHAN_NUM; chan_id++) {
		chan_ctrl = &hdma->w_chans[chan_id];

		chan_ctrl->int_status = ispv4_pcie_hdma_reg_read(hdma,
								 HDMA_INT_STATUS(chan_id, dir));
		if (!chan_ctrl->int_status)
			continue;

		ispv4_hdma_irq_comman_handler(hdma, chan_ctrl);

		complete(&chan_ctrl->comp);
	}

	return IRQ_HANDLED;
}

static int ispv4_hdma_alloc_ll_mem(struct pcie_hdma *hdma,
				   struct pcie_hdma_mm_blocks *mm_blocks,
				   enum pcie_hdma_dir dir)
{
	unsigned int i, elmnt_len;

	if (mm_blocks->blk_cnt > PCIE_HDMA_MAX_XFER_ELMNT_CNT)
		return -EINVAL;

	elmnt_len = ISPV4_HDMA_BUF_SIZE / ISPV4_LL_BLOCK_CNT;
	for (i = 0; i < mm_blocks->blk_cnt; i++) {
		if (dir == HDMA_TO_DEVICE) {
			mm_blocks->blks[i].src_addr = ISPV4_HDMA_EP_BUF_ADDR + (i * elmnt_len);
			mm_blocks->blks[i].dst_addr = hdma->dma + (i * elmnt_len);
		} else {
			mm_blocks->blks[i].src_addr = hdma->dma + (i * elmnt_len);
			mm_blocks->blks[i].dst_addr = ISPV4_HDMA_EP_BUF_ADDR + (i * elmnt_len);
		}
		mm_blocks->blks[i].size = elmnt_len;
	}

	return 0;
}

int ispv4_hdma_ll_transfer(struct pcie_hdma *hdma, enum pcie_hdma_dir dir)
{
	struct pcie_hdma_chan_ctrl *chan_ctrl;
	struct device *dev = &hdma->pci_dev->dev;
	struct pcie_hdma_mm_blocks mm_blocks;
	int ret = 0;

	chan_ctrl = ispv4_hdma_request_chan(hdma, dir);
	if (IS_ERR(chan_ctrl)) {
		ret = PTR_ERR(chan_ctrl);
		dev_err(dev, "request hdma channel failed: %d\n", ret);
		return ret;
	}

	memset(hdma->va, 0xbc, ISPV4_HDMA_BUF_SIZE);
	flush_cache_all();
	//dma_cache_sync(hdma->dev, hdma->va, ISPV4_HDMA_BUF_SIZE, DMA_BIDIRECTIONAL);

	mm_blocks.blk_cnt = ISPV4_LL_BLOCK_CNT;
	ret = ispv4_hdma_alloc_ll_mem(hdma, &mm_blocks, dir);
	if (ret) {
		dev_err(dev, "alloc ll mem failed: %d\n", ret);
		goto release_chan;
	}

	ret = ispv4_hdma_xfer_add_ll_blocks(chan_ctrl, &mm_blocks);
	if (ret) {
		dev_err(dev, "add ll blocks failed: %d\n", ret);
		goto release_chan;
	}

	ret = ispv4_pcie_hdma_start_and_wait_end(hdma, chan_ctrl);
	if (ret) {
		dev_err(dev, "hdma transfer failed: %d\n", ret);
		goto release_chan;
	}

release_chan:
	ispv4_hdma_release_chan(chan_ctrl);

	return ret;
}
EXPORT_SYMBOL_GPL(ispv4_hdma_ll_transfer);

int ispv4_hdma_single_transfer(struct pcie_hdma *hdma, enum pcie_hdma_dir dir)
{
	struct pcie_hdma_chan_ctrl *chan_ctrl = NULL;
	struct device *dev = &hdma->pci_dev->dev;
	int ret = 0;

	chan_ctrl = ispv4_hdma_request_chan(hdma, dir);
	if (IS_ERR(chan_ctrl)) {
		ret = PTR_ERR(chan_ctrl);
		dev_err(dev, "request hdma channel failed: %d\n", ret);
		return ret;
	}

	memset(hdma->va, 0xbc, ISPV4_HDMA_BUF_SIZE);
	flush_cache_all();
	//dma_cache_sync(hdma->dev, hdma->va, ISPV4_HDMA_BUF_SIZE, DMA_BIDIRECTIONAL);

	if (dir == HDMA_TO_DEVICE) {
		ret = ispv4_hdma_xfer_add_block(chan_ctrl, ISPV4_HDMA_EP_BUF_ADDR,
						hdma->dma, ISPV4_HDMA_BUF_SIZE);
		if (ret) {
			dev_err(dev, "add read xfer block failed: %d\n", ret);
			goto release_chan;
		}
	} else {
		ret = ispv4_hdma_xfer_add_block(chan_ctrl, hdma->dma, ISPV4_HDMA_EP_BUF_ADDR,
						ISPV4_HDMA_BUF_SIZE);
		if (ret) {
			dev_err(dev, "add write xfer block failed: %d\n", ret);
			goto release_chan;
		}
	}

	ret = ispv4_pcie_hdma_start_and_wait_end(hdma, chan_ctrl);
	if (ret) {
		dev_err(dev, "hdma transfer failed: %d\n", ret);
		goto release_chan;
	}

release_chan:
	ispv4_hdma_release_chan(chan_ctrl);

	return ret;
}
EXPORT_SYMBOL_GPL(ispv4_hdma_single_transfer);

int ispv4_hdma_single_trans(struct pcie_hdma *hdma, enum pcie_hdma_dir dir,
			    void* data, int len, u32 ep_addr)
{
	struct pcie_hdma_chan_ctrl *chan_ctrl = NULL;
	struct device *dev = &hdma->pci_dev->dev;
	dma_addr_t da;
	void *va;
	int ret = 0;

	va = dma_alloc_coherent(dev, len, &da, GFP_KERNEL);
	if (va == NULL) {
		dev_err(dev, "alloc dma coherent failed.\n");
		ret = -ENOMEM;
		return ret;
	}

	//TODO: Cached? flush?
	memcpy(va, data, len);

	chan_ctrl = ispv4_hdma_request_chan(hdma, dir);
	if (IS_ERR(chan_ctrl)) {
		ret = PTR_ERR(chan_ctrl);
		dev_err(dev, "request hdma channel failed: %d\n", ret);
		return ret;
	}

	if (dir == HDMA_TO_DEVICE) {
		ret = ispv4_hdma_xfer_add_block(chan_ctrl, ep_addr,
						da, len);
		if (ret) {
			dev_err(dev, "add read xfer block failed: %d\n", ret);
			goto release_chan;
		}
	} else {
		ret = ispv4_hdma_xfer_add_block(chan_ctrl, da, ep_addr,
						len);
		if (ret) {
			dev_err(dev, "add write xfer block failed: %d\n", ret);
			goto release_chan;
		}
	}

	ret = ispv4_pcie_hdma_start_and_wait_end(hdma, chan_ctrl);
	if (ret) {
		dev_err(dev, "hdma transfer failed: %d\n", ret);
		goto release_chan;
	}
	if (dir == HDMA_TO_DEVICE) {
		//TODO: Invalidate
		memcpy(data, va, len);
	}
release_chan:
	ispv4_hdma_release_chan(chan_ctrl);

	return ret;
}
EXPORT_SYMBOL_GPL(ispv4_hdma_single_trans);

#ifdef PCIE_HDMA_AP
static int ispv4_pcie_hdma_thread(void *data)
{
	struct pcie_hdma *hdma = data;

	while (!kthread_should_stop()) {
		atomic_set(&hdma->hdma_wakeup, 0);

		/* dma test */
		ispv4_hdma_tx_test(hdma);
		set_current_state(TASK_INTERRUPTIBLE);

		if (!atomic_read(&hdma->hdma_wakeup))
			schedule();
	}

	hdma->hdma_thread = NULL;

	return 0;
}

static int ispv4_pcie_hdma_thread_setup(struct pcie_hdma *hdma)
{
	struct device *dev = &hdma->pci_dev->dev;
	int ret = 0;

	struct sched_param param = {
		.sched_priority = 3 * MAX_USER_RT_PRIO / 4,
	};

	hdma->hdma_thread = kthread_create(ispv4_pcie_hdma_thread, hdma,
					   dev_name(dev));


	if (IS_ERR(hdma->hdma_thread)) {
		ret = PTR_ERR(hdma->hdma_thread);
		dev_err(dev, "kthread_create failed: %d\n", ret);
		return ret;
	}

	sched_setscheduler(hdma->hdma_thread, SCHED_FIFO, &param);

	return ret;
}
#endif

static void ispv4_hdma_single_xfer_config(struct pcie_hdma *hdma, uint8_t chan_id,
					  enum pcie_hdma_dir dir,
					  struct pcie_hdma_xfer_cfg *xfer_cfg)
{
	uint32_t src_addr = xfer_cfg->ll_elmnt[0].sar_low;
	uint32_t dst_addr = xfer_cfg->ll_elmnt[0].dar_low;
	uint32_t size = xfer_cfg->ll_elmnt[0].tx_size;
	uint32_t val;

	/* Enable chan */
	ispv4_pcie_hdma_reg_write(hdma, HDMA_EN(chan_id, dir), HDMA_CHAN_EN);

	/* Configure transfer size*/
	ispv4_pcie_hdma_reg_write(hdma, HDMA_XFERSIZE(chan_id, dir), size);

	/* Configure src addr */
	ispv4_pcie_hdma_reg_write(hdma, HDMA_SAR_LOW(chan_id, dir), src_addr);
	ispv4_pcie_hdma_reg_write(hdma, HDMA_SAR_HIGH(chan_id, dir), 0);

	/* Configure dst addr */
	ispv4_pcie_hdma_reg_write(hdma, HDMA_DAR_LOW(chan_id, dir), dst_addr);
	ispv4_pcie_hdma_reg_write(hdma, HDMA_DAR_HIGH(chan_id, dir), 0);

	/* Disable linked list mode */
	val = ispv4_pcie_hdma_reg_read(hdma, HDMA_CONTROL1(chan_id, dir));
	val &= ~HDMA_CONTROL1_LLEN;
	ispv4_pcie_hdma_reg_write(hdma, HDMA_DAR_HIGH(chan_id, dir), val);
}

static void ispv4_hdma_ll_xfer_config(struct pcie_hdma *hdma, uint8_t chan_id,
				      enum pcie_hdma_dir dir,
				      struct pcie_hdma_xfer_cfg *cfg)
{
	struct pcie_hdma_ll_link_elmnt *link_elmnt =
			(struct pcie_hdma_ll_link_elmnt *)&cfg->ll_elmnt[cfg->ll_elmnt_cnt];
	uint32_t val;

	/* Enable chan */
	ispv4_pcie_hdma_reg_write(hdma, HDMA_EN(chan_id, dir), HDMA_CHAN_EN);

	ispv4_pcie_hdma_reg_write(hdma, HDMA_LLP_LOW(chan_id, dir), link_elmnt->llp_low);
	ispv4_pcie_hdma_reg_write(hdma, HDMA_LLP_HIGH(chan_id, dir), link_elmnt->llp_high);

	/* Enable linked list mode */
	val = ispv4_pcie_hdma_reg_read(hdma, HDMA_CONTROL1(chan_id, dir));
	val |= HDMA_CONTROL1_LLEN;
	ispv4_pcie_hdma_reg_write(hdma, HDMA_DAR_HIGH(chan_id, dir), val);

	/* Setup CCS and CB */
	val = ispv4_pcie_hdma_reg_read(hdma, HDMA_CYCLE(chan_id, dir));
	val = HDMA_CYCLE_CYCLE_STATE | HDMA_CYCLE_CYCLE_BIT;
	ispv4_pcie_hdma_reg_write(hdma, HDMA_CYCLE(chan_id, dir), val);
}

static void ispv4_pcie_hdma_chan_request_init(struct pcie_hdma_chan_ctrl *chan_ctrl)
{
	chan_ctrl->timeout_ms = PCIE_HDMA_DEF_TIMEOUT_MS;
	chan_ctrl->wait_type = INTERRUPT;
}

static void ispv4_pcie_hdma_chan_reset(struct pcie_hdma_chan_ctrl *chan_ctrl)
{
	chan_ctrl->inuse = false;
	chan_ctrl->xfer_cfg.ll_elmnt_cnt = 0;
	chan_ctrl->xfer_cfg.total_len = 0;
}

static bool ispv4_hdma_ll_elmnt_validate(struct pcie_hdma_ll_data_elmnt *elmnt)
{
	if (elmnt->sar_low == elmnt->dar_low)
		return false;

	if (!elmnt->tx_size)
		return false;

	return true;
}

static bool ispv4_hdma_single_blk_xfer_cfg_validate(struct pcie_hdma_xfer_cfg *xfer_cfg)
{
	if (xfer_cfg->ll_elmnt_cnt != 1)
		return false;

	if (!ispv4_hdma_ll_elmnt_validate(&xfer_cfg->ll_elmnt[0]))
		return false;

	return true;
}

static bool ispv4_hdma_ll_xfer_cfg_validate(struct pcie_hdma_xfer_cfg *xfer_cfg)
{
	uint32_t i;

	if (xfer_cfg->ll_elmnt_cnt <= 1)
		return false;

	for (i = 0; i < xfer_cfg->ll_elmnt_cnt; i++) {
		if (ispv4_hdma_ll_elmnt_validate(&xfer_cfg->ll_elmnt[i]))
			return false;
	}

	return true;
}

static bool ispv4_pcie_hdma_xfer_cfg_validate(struct pcie_hdma_chan_ctrl *chan_ctrl)
{
	if (chan_ctrl->xfer_mode == HDMA_SINGLE_BLK_MODE)
		return ispv4_hdma_single_blk_xfer_cfg_validate(&chan_ctrl->xfer_cfg);

	if (chan_ctrl->xfer_mode == HDMA_LL_MODE)
		return ispv4_hdma_ll_xfer_cfg_validate(&chan_ctrl->xfer_cfg);

	return false;
}

static bool ispv4_hdma_chan_partial_validate(struct pcie_hdma_chan_ctrl *chan_ctrl)
{
	if (!chan_ctrl->inuse)
		return false;

	if (chan_ctrl->dir != HDMA_TO_DEVICE && chan_ctrl->dir != HDMA_FROM_DEVICE)
		return false;

	if (chan_ctrl->id >= PCIE_HDMA_CHAN_NUM)
		return false;

	if (chan_ctrl->wait_type != POLLING && chan_ctrl->wait_type != INTERRUPT)
		return false;

	return true;
}

static bool ispv4_hdma_chan_full_validate(struct pcie_hdma_chan_ctrl *chan_ctrl)
{
	if (!ispv4_hdma_chan_partial_validate(chan_ctrl))
		return false;

	if (!ispv4_pcie_hdma_xfer_cfg_validate(chan_ctrl))
		return false;

	return true;
}

static void ispv4_hdma_xfer_config(struct pcie_hdma *hdma,
				   struct pcie_hdma_chan_ctrl *chan_ctrl)
{
	if (chan_ctrl->xfer_mode == HDMA_SINGLE_BLK_MODE)
		ispv4_hdma_single_xfer_config(hdma, chan_ctrl->id, chan_ctrl->dir,
					      &chan_ctrl->xfer_cfg);
	else
		ispv4_hdma_ll_xfer_config(hdma, chan_ctrl->id, chan_ctrl->dir,
					  &chan_ctrl->xfer_cfg);
}

static void ispv4_hdma_add_link_elmnt(struct pcie_hdma_chan_ctrl *chan_ctrl)
{
	struct pcie_hdma_xfer_cfg *xfer_cfg = &chan_ctrl->xfer_cfg;
	struct pcie_hdma_ll_link_elmnt *link_elment = NULL;
	uint32_t link_elemnt_idx = xfer_cfg->ll_elmnt_cnt;

	link_elment = (struct pcie_hdma_ll_link_elmnt *)&xfer_cfg->ll_elmnt[link_elemnt_idx];
	link_elment->cfg = HDMA_LINK_ELMNT_LLP | HDMA_LINK_ELMNT_TCB;
	//link_elment->llp_low = (uint32_t)(&xfer_cfg->ll_elmnt[0]);
	link_elment->llp_high = 0;

	xfer_cfg->ll_elmnt_cnt++; /* add count for the link elmnt */
}

/****************************************************************************
 * Name: ispv4_hdma_request_chan
 *
 * Description:
 *	Request an HDMA channel according to dir.
 *
 ****************************************************************************/
struct pcie_hdma_chan_ctrl *ispv4_hdma_request_chan(struct pcie_hdma *hdma,
						    enum pcie_hdma_dir dir)
{
	struct pcie_hdma_chan_ctrl *chan_ctrl = NULL;
	struct device *dev = &hdma->pci_dev->dev;
	unsigned long flags;
	uint32_t i;

	if (dir != HDMA_FROM_DEVICE && dir != HDMA_TO_DEVICE) {
		dev_err(dev, "Invalid HDMA direction\n");
		return NULL;
	}

	for (i = 0; i < PCIE_HDMA_CHAN_NUM; i++) {
		chan_ctrl = (dir == HDMA_FROM_DEVICE) ? &hdma->r_chans[i] : &hdma->w_chans[i];
		spin_lock_irqsave(&chan_ctrl->chan_lock, flags);
		if (!chan_ctrl->inuse) {
			chan_ctrl->inuse = true;
			ispv4_pcie_hdma_chan_request_init(chan_ctrl);
			spin_unlock_irqrestore(&chan_ctrl->chan_lock, flags);
			break;
		}
		spin_unlock_irqrestore(&chan_ctrl->chan_lock, flags);
	}

	return chan_ctrl;
}
EXPORT_SYMBOL_GPL(ispv4_hdma_request_chan);

void ispv4_hdma_release_chan(struct pcie_hdma_chan_ctrl *chan_ctrl)
{
	unsigned long flags;

	if (!chan_ctrl->inuse)
		return;

	spin_lock_irqsave(&chan_ctrl->chan_lock, flags);
	ispv4_pcie_hdma_chan_reset(chan_ctrl);
	spin_unlock_irqrestore(&chan_ctrl->chan_lock, flags);
}
EXPORT_SYMBOL_GPL(ispv4_hdma_release_chan);

/* NOT thread safe! */
int32_t ispv4_hdma_xfer_add_block(struct pcie_hdma_chan_ctrl *chan_ctrl,
				  uint32_t src_addr, uint32_t dst_addr,
				  uint32_t size)
{
	struct pcie_hdma_xfer_cfg *xfer_cfg = NULL;
	uint32_t first_slot;

	if (!chan_ctrl->inuse)
		return -EINVAL;

	xfer_cfg = &chan_ctrl->xfer_cfg;
	if (xfer_cfg->ll_elmnt_cnt + 1 > PCIE_HDMA_MAX_XFER_ELMNT_CNT)
		return -EINVAL;

	first_slot = xfer_cfg->ll_elmnt_cnt;
	xfer_cfg->ll_elmnt[first_slot].cfg |= HDMA_CYCLE_CYCLE_BIT;
	xfer_cfg->ll_elmnt[first_slot].tx_size = size;
	xfer_cfg->ll_elmnt[first_slot].sar_low = src_addr;
	xfer_cfg->ll_elmnt[first_slot].sar_high = 0;
	xfer_cfg->ll_elmnt[first_slot].dar_low = dst_addr;
	xfer_cfg->ll_elmnt[first_slot].dar_high = 0;

#ifdef ISPV4_PCIE_PERDORMANCE
	xfer_cfg->total_len += size;
#endif
	xfer_cfg->ll_elmnt_cnt += 1;
	if (xfer_cfg->ll_elmnt_cnt == 1)
		chan_ctrl->xfer_mode = HDMA_SINGLE_BLK_MODE;
	else
		chan_ctrl->xfer_mode = HDMA_LL_MODE;

	return 0;
}
EXPORT_SYMBOL_GPL(ispv4_hdma_xfer_add_block);

/* NOT thread safe! */
int32_t ispv4_hdma_xfer_add_ll_blocks(struct pcie_hdma_chan_ctrl *chan_ctrl,
				      struct pcie_hdma_mm_blocks *blocks)
{
	struct pcie_hdma_xfer_cfg *xfer_cfg = NULL;
	uint32_t first_slot;
	uint32_t i;

	if (!chan_ctrl || !blocks)
		return -EINVAL;

	if (!blocks->blk_cnt)
		return -EINVAL;

	if (!chan_ctrl->inuse)
		return -EBUSY;

	xfer_cfg = &chan_ctrl->xfer_cfg;
	if (blocks->blk_cnt + xfer_cfg->ll_elmnt_cnt > PCIE_HDMA_MAX_XFER_ELMNT_CNT)
		return -EINVAL;

	first_slot = xfer_cfg->ll_elmnt_cnt;
	for (i = 0; i < blocks->blk_cnt; i++) {
#ifdef ISPV4_PCIE_PERDORMANCE
		xfer_cfg->total_len += blocks->blks[i].size;
#endif
		xfer_cfg->ll_elmnt[first_slot + i].cfg |= HDMA_CYCLE_CYCLE_BIT;
		xfer_cfg->ll_elmnt[first_slot + i].tx_size = blocks->blks[i].size;
		xfer_cfg->ll_elmnt[first_slot + i].sar_low = blocks->blks[i].src_addr;
		xfer_cfg->ll_elmnt[first_slot + i].sar_high = 0;
		xfer_cfg->ll_elmnt[first_slot + i].dar_low = blocks->blks[i].dst_addr;
		xfer_cfg->ll_elmnt[first_slot + i].dar_high = 0;
	}

	xfer_cfg->ll_elmnt_cnt += blocks->blk_cnt;
	if (xfer_cfg->ll_elmnt_cnt > 1)
		chan_ctrl->xfer_mode = HDMA_LL_MODE;
	else
		chan_ctrl->xfer_mode = HDMA_SINGLE_BLK_MODE;

	return 0;
}
EXPORT_SYMBOL_GPL(ispv4_hdma_xfer_add_ll_blocks);

static int ispv4_hdma_do_complete(struct pcie_hdma *hdma,
				  struct pcie_hdma_chan_ctrl *chan_ctrl)
{
	struct device *dev = &hdma->pci_dev->dev;
	uint32_t int_status;
#ifdef ISPV4_PCIE_PERDORMANCE
	uint64_t cost_ns;
#endif

	int_status = ispv4_pcie_hdma_reg_read(hdma, HDMA_STATUS(chan_ctrl->id, chan_ctrl->dir));

	if (int_status == HDMA_STATUS_STOPPED) {
#ifdef ISPV4_PCIE_PERDORMANCE
		chan_ctrl->end_ns = do_get_timeofday();
		cost_ns = chan_ctrl->end_ns - chan_ctrl->start_ns;

		dev_info(dev, "\tlen: %luBytes, cost %luns, speed: %lluMB/s\n",
			 chan_ctrl->xfer_cfg.total_len, cost_ns,
			 (chan_ctrl->xfer_cfg.total_len * NSEC_PER_SEC) / (cost_ns * 1024 * 1024));
#endif
		dev_info(dev, "%d Finished!\n", __LINE__);

		return 0;
	}

	if (int_status == HDMA_STATUS_ABORTED) {
		int_status = ispv4_pcie_hdma_reg_read(hdma,
						HDMA_INT_STATUS(chan_ctrl->id, chan_ctrl->dir));
		dev_info(dev, "%d Aborted! INT_STATUS: 0x%lx\n", __LINE__, int_status);
	} else
		dev_info(dev, "%d Unknown HDMA status\n", __LINE__);

	return -EINVAL;
}

static int32_t ispv4_hdma_wait_for_complete(struct pcie_hdma *hdma,
					    struct pcie_hdma_chan_ctrl *chan_ctrl)
{
	struct device *dev = &hdma->pci_dev->dev;
	enum pcie_hdma_dir dir;
#ifdef ISPV4_PCIE_PERDORMANCE
	uint64_t wait_start_ms;
#endif
	uint8_t chan_id;
	uint32_t ret = 0;

	chan_id = chan_ctrl->id;
	dir = chan_ctrl->dir;

#ifdef ISPV4_PCIE_PERDORMANCE
	if (chan_ctrl->wait_type == POLLING) {
		wait_start_ms = do_get_timeofday();
		while (ispv4_pcie_hdma_reg_read(hdma, HDMA_STATUS(chan_id, dir)) == HDMA_STATUS_RUNNING) {
			if (do_get_timeofday() - wait_start_ms > chan_ctrl->timeout_ms) {
				//TODO: stop HDMA
				dev_info(dev, "%s timeout!\n", chan_ctrl->chan_name);
				return -EINVAL;
			}
		}

	} else {
#endif /* ISPV4_PCIE_PERDORMANCE */
		ret = wait_for_completion_timeout(&chan_ctrl->comp,
				msecs_to_jiffies(1000));
		if (!ret) {
			dev_info(dev, "wait for completion timeout!");
			ret = -ETIMEDOUT;
			return ret;
		}

#ifdef ISPV4_PCIE_PERDORMANCE
	}
#endif

	return ispv4_hdma_do_complete(hdma, chan_ctrl);
}

static int ispv4_hdma_msi_conf(struct pcie_hdma *hdma, struct pcie_hdma_chan_ctrl *chan_ctrl)
{
	struct pci_dev *pci_dev = hdma->pci_dev;
	unsigned int msi_addr_low, msi_data;
	unsigned short ctl_val;

	pci_read_config_word(pci_dev, pci_dev->msi_cap + PCI_MSI_FLAGS, &ctl_val);

	pci_read_config_dword(pci_dev, pci_dev->msi_cap + PCI_MSI_ADDRESS_LO, &msi_addr_low);

	if (!(ctl_val & PCI_MSI_FLAGS_ENABLE))
		return -EINVAL;

	if (ctl_val & PCI_MSI_FLAGS_64BIT)
		pci_read_config_dword(pci_dev, pci_dev->msi_cap + PCI_MSI_DATA_64, &msi_data);
	else
		pci_read_config_dword(pci_dev, pci_dev->msi_cap + PCI_MSI_DATA_32, &msi_data);

	ispv4_pcie_hdma_reg_write(hdma, HDMA_MSI_STOP_LOW(chan_ctrl->id, chan_ctrl->dir),
				  msi_addr_low);
	ispv4_pcie_hdma_reg_write(hdma, HDMA_MSI_STOP_HIGH(chan_ctrl->id, chan_ctrl->dir),
				  0x00);

	ispv4_pcie_hdma_reg_write(hdma, HDMA_MSI_WATERMARK_LOW(chan_ctrl->id, chan_ctrl->dir),
				  msi_addr_low);
	ispv4_pcie_hdma_reg_write(hdma, HDMA_MSI_WATERMARK_HIGH(chan_ctrl->id, chan_ctrl->dir),
				  0x00);

	ispv4_pcie_hdma_reg_write(hdma, HDMA_MSI_ABORT_LOW(chan_ctrl->id, chan_ctrl->dir),
				  msi_addr_low);
	ispv4_pcie_hdma_reg_write(hdma, HDMA_MSI_ABORT_HIGH(chan_ctrl->id, chan_ctrl->dir),
				  0x00);

	if (chan_ctrl->dir == HDMA_TO_DEVICE)
		msi_data += PCIE_HDMA_MSI_NUM;
	else
		msi_data += PCIE_HDMA_MSI_NUM + 1;

	ispv4_pcie_hdma_reg_write(hdma, HDMA_MSI_MSGD(chan_ctrl->id, chan_ctrl->dir),
				  msi_data);

	return 0;
}

static void ispv4_pcie_hdma_start(struct pcie_hdma *hdma,
				  struct pcie_hdma_chan_ctrl *chan_ctrl)
{
	uint32_t enabled_int, val;

	ispv4_hdma_xfer_config(hdma, chan_ctrl);

#ifdef ISPV4_PCIE_PERDORMANCE
	//chan_ctrl->start_ns = do_get_timeofday();
#endif
	enabled_int = HDMA_INT_LAIE | HDMA_INT_RAIE | HDMA_INT_LSIE | HDMA_INT_RSIE;
	ispv4_pcie_hdma_reg_write(hdma, HDMA_INT_SETUP(chan_ctrl->id, chan_ctrl->dir), enabled_int);

	val = ispv4_pcie_hdma_reg_read(hdma, HDMA_DOORBELL(chan_ctrl->id, chan_ctrl->dir));
	val |= HDMA_DB_START;
	ispv4_pcie_hdma_reg_write(hdma, HDMA_DOORBELL(chan_ctrl->id, chan_ctrl->dir), val);
}

int32_t ispv4_pcie_hdma_start_and_wait_end(struct pcie_hdma *hdma,
					   struct pcie_hdma_chan_ctrl *chan_ctrl)
{
	struct device *dev;
	int32_t ret = 0;

	if (!hdma) {
		pr_err("ispv4 hdma is NULL !\n");
		return -EINVAL;
	}

	dev = &hdma->pci_dev->dev;

	ret = ispv4_hdma_chan_full_validate(chan_ctrl);
	if (!ret) {
		dev_err(dev, "hdma channel validate: %d\n", ret);
		return ret;
	}

	ret = ispv4_hdma_msi_conf(hdma, chan_ctrl);
	if (ret) {
		dev_err(dev, "hdma msi configure failed: %d\n", ret);
		return ret;
	}

	if (chan_ctrl->xfer_mode == HDMA_LL_MODE)
		ispv4_hdma_add_link_elmnt(chan_ctrl);

	ispv4_pcie_hdma_start(hdma, chan_ctrl);

	ret = ispv4_hdma_wait_for_complete(hdma, chan_ctrl);

	return ret;
}
EXPORT_SYMBOL_GPL(ispv4_pcie_hdma_start_and_wait_end);

static void ispv4_pcie_hdma_chan_init(struct pcie_hdma *hdma)
{
	struct pcie_hdma_chan_ctrl *chan_ctrl;
	uint32_t i;

	for (i = 0; i < PCIE_HDMA_CHAN_NUM; i++) {
		chan_ctrl = &hdma->r_chans[i];
		chan_ctrl->id = i;
		chan_ctrl->dir = HDMA_FROM_DEVICE;
		spin_lock_init(&chan_ctrl->chan_lock);
		init_completion(&chan_ctrl->comp);
		ispv4_pcie_hdma_chan_reset(chan_ctrl);

		chan_ctrl = &hdma->w_chans[i];
		chan_ctrl->id = i;
		chan_ctrl->dir = HDMA_TO_DEVICE;
		init_completion(&chan_ctrl->comp);
		spin_lock_init(&chan_ctrl->chan_lock);
		ispv4_pcie_hdma_chan_reset(chan_ctrl);
	}
}

struct pcie_hdma *ispv4_pcie_hdma_init(struct ispv4_data *priv)
{
	struct device *dev = &priv->pci->dev;
	struct pcie_hdma *hdma;
	uint16_t ctl;
	int ret = 0;

	hdma = devm_kzalloc(dev, sizeof(struct pcie_hdma), GFP_KERNEL);
	if (!hdma)
		return ERR_PTR(-ENOMEM);

	hdma->base = priv->base_bar[3];
	hdma->pci_dev = priv->pci;

	ret = pcie_capability_read_word(priv->pci, PCI_EXP_DEVCTL, &ctl);
	if (ret)
		return ERR_PTR(ret);

	if (ctl & PCI_EXP_DEVCTL_NOSNOOP_EN) {
		ctl &= ~PCI_EXP_DEVCTL_NOSNOOP_EN;
		pcie_capability_write_word(priv->pci, PCI_EXP_DEVCTL, ctl);
	}

	hdma->va = dma_alloc_coherent(dev, ISPV4_HDMA_BUF_SIZE, &hdma->dma, GFP_KERNEL);
	if (!hdma->va)
		return ERR_PTR(-ENOMEM);

	ret = devm_request_threaded_irq(dev, priv->pci->irq + PCIE_HDMA_MSI_NUM,
					NULL, ispv4_hdma_irq_wr_ch_handler,
					IRQF_TRIGGER_HIGH | IRQF_ONESHOT,
					"ispv4_hdma_wr_ch", hdma);
	if (ret) {
		dev_err(dev, "request hdma write channel msi irq failed!\n");
		return ERR_PTR(ret);
	}

	ret = devm_request_threaded_irq(dev, priv->pci->irq + PCIE_HDMA_MSI_NUM + 1,
					NULL, ispv4_hdma_irq_rd_ch_handler,
					IRQF_TRIGGER_HIGH | IRQF_ONESHOT,
					"ispv4_hdma_rd_ch", hdma);
	if (ret) {
		dev_err(dev, "request hdma read channel msi irq failed!\n");
		return ERR_PTR(ret);
	}

	ispv4_pcie_hdma_chan_init(hdma);

#ifdef ISPV4_DEBUGFS
	ispv4_debugfs_add_pcie_hdma(hdma);
#endif

	return hdma;
}
EXPORT_SYMBOL_GPL(ispv4_pcie_hdma_init);

void ispv4_pcie_hdma_exit(struct pcie_hdma *hdma)
{
#ifdef HDMA_EXIT
	struct pcie_hdma *hdma;

	hdma = container_of(hdma, struct pcie_hdma, pdata);

	kthread_stop(hdma->hdma_thread);
#endif
	dma_free_coherent(&hdma->pci_dev->dev, ISPV4_HDMA_BUF_SIZE, hdma->va, hdma->dma);
}

MODULE_LICENSE("GPL");
