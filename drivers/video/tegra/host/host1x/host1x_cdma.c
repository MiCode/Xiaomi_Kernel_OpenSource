/*
 * drivers/video/tegra/host/host1x/host1x_cdma.c
 *
 * Tegra Graphics Host Command DMA
 *
 * Copyright (c) 2010-2013, NVIDIA Corporation.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <linux/slab.h>
#include <linux/scatterlist.h>
#include "nvhost_acm.h"
#include "nvhost_cdma.h"
#include "nvhost_channel.h"
#include "debug.h"
#include "dev.h"
#include "chip_support.h"
#include "nvhost_memmgr.h"

#include "host1x_cdma.h"
#include "host1x_hwctx.h"

static inline u32 host1x_channel_dmactrl(int stop, int get_rst, int init_get)
{
	return host1x_channel_dmactrl_dmastop_f(stop)
		| host1x_channel_dmactrl_dmagetrst_f(get_rst)
		| host1x_channel_dmactrl_dmainitget_f(init_get);
}

static void cdma_timeout_handler(struct work_struct *work);

/*
 * push_buffer
 *
 * The push buffer is a circular array of words to be fetched by command DMA.
 * Note that it works slightly differently to the sync queue; fence == cur
 * means that the push buffer is full, not empty.
 */


/**
 * Reset to empty push buffer
 */
static void push_buffer_reset(struct push_buffer *pb)
{
	pb->fence = PUSH_BUFFER_SIZE - 8;
	pb->cur = 0;
}

/**
 * Init push buffer resources
 */
static void push_buffer_destroy(struct push_buffer *pb);
static int push_buffer_init(struct push_buffer *pb)
{
	struct nvhost_cdma *cdma = pb_to_cdma(pb);
	struct mem_mgr *mgr = cdma_to_memmgr(cdma);
	pb->mem = NULL;
	pb->mapped = NULL;
	pb->phys = 0;
	pb->client_handle = NULL;

	BUG_ON(!cdma_pb_op().reset);
	cdma_pb_op().reset(pb);

	/* allocate and map pushbuffer memory */
	pb->mem = mem_op().alloc(mgr, PUSH_BUFFER_SIZE + 4, 32,
			      mem_mgr_flag_write_combine);
	if (IS_ERR_OR_NULL(pb->mem)) {
		pb->mem = NULL;
		goto fail;
	}
	pb->mapped = mem_op().mmap(pb->mem);
	if (IS_ERR_OR_NULL(pb->mapped)) {
		pb->mapped = NULL;
		goto fail;
	}

	/* pin pushbuffer and get physical address */
	pb->sgt = mem_op().pin(mgr, pb->mem);
	if (IS_ERR(pb->sgt)) {
		pb->sgt = 0;
		goto fail;
	}
	pb->phys = sg_dma_address(pb->sgt->sgl);

	/* memory for storing nvmap client and handles for each opcode pair */
	pb->client_handle = kzalloc(NVHOST_GATHER_QUEUE_SIZE *
				sizeof(struct mem_mgr_handle),
			GFP_KERNEL);
	if (!pb->client_handle)
		goto fail;

	/* put the restart at the end of pushbuffer memory */
	*(pb->mapped + (PUSH_BUFFER_SIZE >> 2)) =
		nvhost_opcode_restart(pb->phys);

	return 0;

fail:
	push_buffer_destroy(pb);
	return -ENOMEM;
}

/**
 * Clean up push buffer resources
 */
static void push_buffer_destroy(struct push_buffer *pb)
{
	struct nvhost_cdma *cdma = pb_to_cdma(pb);
	struct mem_mgr *mgr = cdma_to_memmgr(cdma);
	if (pb->mapped)
		mem_op().munmap(pb->mem, pb->mapped);

	if (pb->phys != 0)
		mem_op().unpin(mgr, pb->mem, pb->sgt);

	if (pb->mem)
		mem_op().put(mgr, pb->mem);

	kfree(pb->client_handle);

	pb->mem = NULL;
	pb->mapped = NULL;
	pb->phys = 0;
	pb->client_handle = 0;
}

/**
 * Push two words to the push buffer
 * Caller must ensure push buffer is not full
 */
static void push_buffer_push_to(struct push_buffer *pb,
		struct mem_mgr *client, struct mem_handle *handle,
		u32 op1, u32 op2)
{
	u32 cur = pb->cur;
	u32 *p = (u32 *)((u32)pb->mapped + cur);
	u32 cur_nvmap = (cur/8) & (NVHOST_GATHER_QUEUE_SIZE - 1);
	BUG_ON(cur == pb->fence);
	*(p++) = op1;
	*(p++) = op2;
	pb->client_handle[cur_nvmap].client = client;
	pb->client_handle[cur_nvmap].handle = handle;
	pb->cur = (cur + 8) & (PUSH_BUFFER_SIZE - 1);
}

/**
 * Pop a number of two word slots from the push buffer
 * Caller must ensure push buffer is not empty
 */
static void push_buffer_pop_from(struct push_buffer *pb,
		unsigned int slots)
{
	/* Clear the nvmap references for old items from pb */
	unsigned int i;
	u32 fence_nvmap = pb->fence/8;
	for (i = 0; i < slots; i++) {
		int cur_fence_nvmap = (fence_nvmap+i)
				& (NVHOST_GATHER_QUEUE_SIZE - 1);
		struct mem_mgr_handle *h = &pb->client_handle[cur_fence_nvmap];
		h->client = NULL;
		h->handle = NULL;
	}
	/* Advance the next write position */
	pb->fence = (pb->fence + slots * 8) & (PUSH_BUFFER_SIZE - 1);
}

/**
 * Return the number of two word slots free in the push buffer
 */
static u32 push_buffer_space(struct push_buffer *pb)
{
	return ((pb->fence - pb->cur) & (PUSH_BUFFER_SIZE - 1)) / 8;
}

static u32 push_buffer_putptr(struct push_buffer *pb)
{
	return pb->phys + pb->cur;
}

/*
 * The syncpt incr buffer is filled with methods to increment syncpts, which
 * is later GATHER-ed into the mainline PB. It's used when a timed out context
 * is interleaved with other work, so needs to inline the syncpt increments
 * to maintain the count (but otherwise does no work).
 */

/**
 * Init timeout resources
 */
static int cdma_timeout_init(struct nvhost_cdma *cdma,
				 u32 syncpt_id)
{
	if (syncpt_id == NVSYNCPT_INVALID)
		return -EINVAL;

	INIT_DELAYED_WORK(&cdma->timeout.wq, cdma_timeout_handler);
	cdma->timeout.initialized = true;

	return 0;
}

/**
 * Clean up timeout resources
 */
static void cdma_timeout_destroy(struct nvhost_cdma *cdma)
{
	if (cdma->timeout.initialized)
		cancel_delayed_work(&cdma->timeout.wq);
	cdma->timeout.initialized = false;
}

/**
 * Increment timedout buffer's syncpt via CPU.
 */
static void cdma_timeout_cpu_incr(struct nvhost_cdma *cdma, u32 getptr,
				u32 syncpt_incrs, u32 syncval, u32 nr_slots,
				u32 waitbases)
{
	struct nvhost_master *dev = cdma_to_dev(cdma);
	struct push_buffer *pb = &cdma->push_buffer;
	u32 i, getidx;

	for (i = 0; i < syncpt_incrs; i++)
		nvhost_syncpt_cpu_incr(&dev->syncpt, cdma->timeout.syncpt_id);

	/* after CPU incr, ensure shadow is up to date */
	nvhost_syncpt_update_min(&dev->syncpt, cdma->timeout.syncpt_id);

	/* Synchronize wait bases. 2D wait bases are synchronized with
	 * syncpoint 19. Hence wait bases are not updated when syncptid=18. */

	if (cdma->timeout.syncpt_id != NVSYNCPT_2D_0 && waitbases) {
		void __iomem *p;
		p = dev->sync_aperture + host1x_sync_syncpt_base_0_r() +
				(__ffs(waitbases) * sizeof(u32));
		writel(syncval, p);
		dev->syncpt.base_val[__ffs(waitbases)] = syncval;
	}

	/* NOP all the PB slots */
	getidx = getptr - pb->phys;
	while (nr_slots--) {
		u32 *p = (u32 *)((u32)pb->mapped + getidx);
		*(p++) = NVHOST_OPCODE_NOOP;
		*(p++) = NVHOST_OPCODE_NOOP;
		dev_dbg(&dev->dev->dev, "%s: NOP at 0x%x\n",
			__func__, pb->phys + getidx);
		getidx = (getidx + 8) & (PUSH_BUFFER_SIZE - 1);
	}
	wmb();
}

/**
 * Start channel DMA
 */
static void cdma_start(struct nvhost_cdma *cdma)
{
	void __iomem *chan_regs = cdma_to_channel(cdma)->aperture;

	if (cdma->running)
		return;

	BUG_ON(!cdma_pb_op().putptr);
	cdma->last_put = cdma_pb_op().putptr(&cdma->push_buffer);

	writel(host1x_channel_dmactrl(true, false, false),
		chan_regs + host1x_channel_dmactrl_r());

	/* set base, put, end pointer (all of memory) */
	writel(0, chan_regs + host1x_channel_dmastart_r());
	writel(cdma->last_put, chan_regs + host1x_channel_dmaput_r());
	writel(0xFFFFFFFF, chan_regs + host1x_channel_dmaend_r());

	/* reset GET */
	writel(host1x_channel_dmactrl(true, true, true),
		chan_regs + host1x_channel_dmactrl_r());

	/* start the command DMA */
	writel(host1x_channel_dmactrl(false, false, false),
		chan_regs + host1x_channel_dmactrl_r());

	cdma->running = true;
}

/**
 * Similar to cdma_start(), but rather than starting from an idle
 * state (where DMA GET is set to DMA PUT), on a timeout we restore
 * DMA GET from an explicit value (so DMA may again be pending).
 */
static void cdma_timeout_restart(struct nvhost_cdma *cdma, u32 getptr)
{
	struct nvhost_master *dev = cdma_to_dev(cdma);
	void __iomem *chan_regs = cdma_to_channel(cdma)->aperture;

	if (cdma->running)
		return;

	BUG_ON(!cdma_pb_op().putptr);
	cdma->last_put = cdma_pb_op().putptr(&cdma->push_buffer);

	writel(host1x_channel_dmactrl(true, false, false),
		chan_regs + host1x_channel_dmactrl_r());

	/* set base, end pointer (all of memory) */
	writel(0, chan_regs + host1x_channel_dmastart_r());
	writel(0xFFFFFFFF, chan_regs + host1x_channel_dmaend_r());

	/* set GET, by loading the value in PUT (then reset GET) */
	writel(getptr, chan_regs + host1x_channel_dmaput_r());
	writel(host1x_channel_dmactrl(true, true, true),
		chan_regs + host1x_channel_dmactrl_r());

	dev_dbg(&dev->dev->dev,
		"%s: DMA GET 0x%x, PUT HW 0x%x / shadow 0x%x\n",
		__func__,
		readl(chan_regs + host1x_channel_dmaget_r()),
		readl(chan_regs + host1x_channel_dmaput_r()),
		cdma->last_put);

	/* deassert GET reset and set PUT */
	writel(host1x_channel_dmactrl(true, false, false),
		chan_regs + host1x_channel_dmactrl_r());
	writel(cdma->last_put, chan_regs + host1x_channel_dmaput_r());

	/* start the command DMA */
	writel(host1x_channel_dmactrl(false, false, false),
		chan_regs + host1x_channel_dmactrl_r());

	cdma->running = true;
}

/**
 * Kick channel DMA into action by writing its PUT offset (if it has changed)
 */
static void cdma_kick(struct nvhost_cdma *cdma)
{
	u32 put;
	BUG_ON(!cdma_pb_op().putptr);

	put = cdma_pb_op().putptr(&cdma->push_buffer);

	if (put != cdma->last_put) {
		void __iomem *chan_regs = cdma_to_channel(cdma)->aperture;
		wmb();
		writel(put, chan_regs + host1x_channel_dmaput_r());
		cdma->last_put = put;
	}
}

static void cdma_stop(struct nvhost_cdma *cdma)
{
	void __iomem *chan_regs = cdma_to_channel(cdma)->aperture;

	mutex_lock(&cdma->lock);
	if (cdma->running) {
		nvhost_cdma_wait_locked(cdma, CDMA_EVENT_SYNC_QUEUE_EMPTY);
		writel(host1x_channel_dmactrl(true, false, false),
			chan_regs + host1x_channel_dmactrl_r());
		cdma->running = false;
	}
	mutex_unlock(&cdma->lock);
}

/**
 * Stops both channel's command processor and CDMA immediately.
 * Also, tears down the channel and resets corresponding module.
 */
static void cdma_timeout_teardown_begin(struct nvhost_cdma *cdma)
{
	struct nvhost_master *dev = cdma_to_dev(cdma);
	struct nvhost_channel *ch = cdma_to_channel(cdma);
	u32 cmdproc_stop;

	BUG_ON(cdma->torndown);

	dev_dbg(&dev->dev->dev,
		"begin channel teardown (channel id %d)\n", ch->chid);

	cmdproc_stop = readl(dev->sync_aperture + host1x_sync_cmdproc_stop_r());
	cmdproc_stop |= BIT(ch->chid);
	writel(cmdproc_stop, dev->sync_aperture + host1x_sync_cmdproc_stop_r());

	dev_dbg(&dev->dev->dev,
		"%s: DMA GET 0x%x, PUT HW 0x%x / shadow 0x%x\n",
		__func__,
		readl(ch->aperture + host1x_channel_dmaget_r()),
		readl(ch->aperture + host1x_channel_dmaput_r()),
		cdma->last_put);

	writel(host1x_channel_dmactrl(true, false, false),
		ch->aperture + host1x_channel_dmactrl_r());

	writel(BIT(ch->chid), dev->sync_aperture + host1x_sync_ch_teardown_r());
	nvhost_module_reset(ch->dev);

	cdma->running = false;
	cdma->torndown = true;
}

static void cdma_timeout_teardown_end(struct nvhost_cdma *cdma, u32 getptr)
{
	struct nvhost_master *dev = cdma_to_dev(cdma);
	struct nvhost_channel *ch = cdma_to_channel(cdma);
	u32 cmdproc_stop;

	BUG_ON(!cdma->torndown || cdma->running);

	dev_dbg(&dev->dev->dev,
		"end channel teardown (id %d, DMAGET restart = 0x%x)\n",
		ch->chid, getptr);

	cmdproc_stop = readl(dev->sync_aperture + host1x_sync_cmdproc_stop_r());
	cmdproc_stop &= ~(BIT(ch->chid));
	writel(cmdproc_stop, dev->sync_aperture + host1x_sync_cmdproc_stop_r());

	cdma->torndown = false;
	cdma_timeout_restart(cdma, getptr);
}

/**
 * If this timeout fires, it indicates the current sync_queue entry has
 * exceeded its TTL and the userctx should be timed out and remaining
 * submits already issued cleaned up (future submits return an error).
 */
static void cdma_timeout_handler(struct work_struct *work)
{
	struct nvhost_cdma *cdma;
	struct nvhost_master *dev;
	struct nvhost_syncpt *sp;
	struct nvhost_channel *ch;

	u32 syncpt_val;

	u32 prev_cmdproc, cmdproc_stop;

	cdma = container_of(to_delayed_work(work), struct nvhost_cdma,
			    timeout.wq);
	dev = cdma_to_dev(cdma);
	sp = &dev->syncpt;
	ch = cdma_to_channel(cdma);

	if (nvhost_debug_force_timeout_dump ||
		cdma->timeout.timeout_debug_dump)
		nvhost_debug_dump(cdma_to_dev(cdma));

	mutex_lock(&cdma->lock);

	if (!cdma->timeout.clientid) {
		dev_dbg(&dev->dev->dev,
			 "cdma_timeout: expired, but has no clientid\n");
		mutex_unlock(&cdma->lock);
		return;
	}

	/* stop processing to get a clean snapshot */
	prev_cmdproc = readl(dev->sync_aperture + host1x_sync_cmdproc_stop_r());
	cmdproc_stop = prev_cmdproc | BIT(ch->chid);
	writel(cmdproc_stop, dev->sync_aperture + host1x_sync_cmdproc_stop_r());

	dev_dbg(&dev->dev->dev, "cdma_timeout: cmdproc was 0x%x is 0x%x\n",
		prev_cmdproc, cmdproc_stop);

	syncpt_val = nvhost_syncpt_update_min(&dev->syncpt,
			cdma->timeout.syncpt_id);

	/* has buffer actually completed? */
	if ((s32)(syncpt_val - cdma->timeout.syncpt_val) >= 0) {
		dev_dbg(&dev->dev->dev,
			 "cdma_timeout: expired, but buffer had completed\n");
		/* restore */
		cmdproc_stop = prev_cmdproc & ~(BIT(ch->chid));
		writel(cmdproc_stop,
			dev->sync_aperture + host1x_sync_cmdproc_stop_r());
		mutex_unlock(&cdma->lock);
		return;
	}

	dev_warn(&dev->dev->dev,
		"%s: timeout: %d (%s) ctx 0x%p, HW thresh %d, done %d\n",
		__func__,
		cdma->timeout.syncpt_id,
		syncpt_op().name(sp, cdma->timeout.syncpt_id),
		cdma->timeout.ctx,
		syncpt_val, cdma->timeout.syncpt_val);

	/* stop HW, resetting channel/module */
	cdma_op().timeout_teardown_begin(cdma);

	nvhost_cdma_update_sync_queue(cdma, sp, ch->dev);
	mutex_unlock(&cdma->lock);
}

static const struct nvhost_cdma_ops host1x_cdma_ops = {
	.start = cdma_start,
	.stop = cdma_stop,
	.kick = cdma_kick,

	.timeout_init = cdma_timeout_init,
	.timeout_destroy = cdma_timeout_destroy,
	.timeout_teardown_begin = cdma_timeout_teardown_begin,
	.timeout_teardown_end = cdma_timeout_teardown_end,
	.timeout_cpu_incr = cdma_timeout_cpu_incr,
};

static const struct nvhost_pushbuffer_ops host1x_pushbuffer_ops = {
	.reset = push_buffer_reset,
	.init = push_buffer_init,
	.destroy = push_buffer_destroy,
	.push_to = push_buffer_push_to,
	.pop_from = push_buffer_pop_from,
	.space = push_buffer_space,
	.putptr = push_buffer_putptr,
};

