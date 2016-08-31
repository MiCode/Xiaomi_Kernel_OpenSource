/*
 * drivers/video/tegra/host/host1x/host1x_cdma.c
 *
 * Tegra Graphics Host Command DMA
 *
 * Copyright (c) 2010-2013, NVIDIA Corporation. All rights reserved.
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
#include "class_ids.h"
#include "chip_support.h"
#include "nvhost_job.h"

#include "host1x_cdma.h"

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
	int err = 0;
	pb->mapped = NULL;
	pb->dma_addr = 0;
	pb->client_handle = NULL;

	cdma_pb_op().reset(pb);

	/* allocate the pushbuffer memory */
	pb->mapped = dma_alloc_writecombine(&cdma_to_dev(cdma)->dev->dev,
					PUSH_BUFFER_SIZE + 4,
					&pb->dma_addr,
					GFP_KERNEL);
	if (!pb->mapped) {
		err = -ENOMEM;
		pb->mapped = NULL;
		goto fail;
	}

	/* memory for storing nvmap client and handles for each opcode pair */
	pb->client_handle = kzalloc(NVHOST_GATHER_QUEUE_SIZE *
				sizeof(struct mem_mgr_handle),
			GFP_KERNEL);
	if (!pb->client_handle) {
		err = -ENOMEM;
		goto fail;
	}

	/* put the restart at the end of pushbuffer memory */
	*(pb->mapped + (PUSH_BUFFER_SIZE >> 2)) =
		nvhost_opcode_restart(pb->dma_addr);

	return 0;

fail:
	push_buffer_destroy(pb);
	return err;
}

/**
 * Clean up push buffer resources
 */
static void push_buffer_destroy(struct push_buffer *pb)
{
	struct nvhost_cdma *cdma = pb_to_cdma(pb);
	if (pb->mapped)
		dma_free_writecombine(&cdma_to_dev(cdma)->dev->dev,
					PUSH_BUFFER_SIZE + 4,
					pb->mapped,
					pb->dma_addr);

	kfree(pb->client_handle);

	pb->mapped = NULL;
	pb->dma_addr = 0;
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
	u32 *p = (u32 *)((uintptr_t)pb->mapped + cur);
	u32 cur_nvmap = (cur/8) & (NVHOST_GATHER_QUEUE_SIZE - 1);
	WARN_ON(cur == pb->fence);
	*(p++) = op1;
	*(p++) = op2;
	pb->client_handle[cur_nvmap].client = client;
	pb->client_handle[cur_nvmap].handle = handle;
	pb->cur = (cur + 8) & (PUSH_BUFFER_SIZE - 1);
}

static void _push_buffer_push_to(struct push_buffer *pb,
				dma_addr_t iova,
				u32 op1, u32 op2)
{
	u32 cur = pb->cur;
	u32 *p = (u32 *)((uintptr_t)pb->mapped + cur);
	u32 cur_nvmap = (cur/8) & (NVHOST_GATHER_QUEUE_SIZE - 1);
	WARN_ON(cur == pb->fence);
	*(p++) = op1;
	*(p++) = op2;
	pb->client_handle[cur_nvmap].iova = iova;
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
		h->iova = 0;
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
	return pb->dma_addr + pb->cur;
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
static void cdma_timeout_pb_cleanup(struct nvhost_cdma *cdma, u32 getptr,
				u32 nr_slots)
{
	struct nvhost_master *dev = cdma_to_dev(cdma);
	struct push_buffer *pb = &cdma->push_buffer;
	u32 getidx;

	/* NOP all the PB slots */
	getidx = getptr - pb->dma_addr;
	while (nr_slots--) {
		u32 *p = (u32 *)((uintptr_t)pb->mapped + getidx);
		*(p++) = NVHOST_OPCODE_NOOP;
		*(p++) = NVHOST_OPCODE_NOOP;
		dev_dbg(&dev->dev->dev, "%s: NOP at 0x%llx\n",
			__func__, (u64)(pb->dma_addr + getidx));
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

	/* prevent using setclass inside gathers */
	nvhost_channel_init_gather_filter(cdma_to_channel(cdma));

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

	/* reinitialise gather filter for the channel */
	nvhost_channel_init_gather_filter(cdma_to_channel(cdma));

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

	put = cdma_pb_op().putptr(&cdma->push_buffer);

	if (put != cdma->last_put) {
		void __iomem *chan_regs = cdma_to_channel(cdma)->aperture;
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

	if (cdma->torndown && !cdma->running) {
		dev_warn(&dev->dev->dev, "Already torn down\n");
		return;
	}

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

static void cdma_timeout_release_mlocks(struct nvhost_cdma *cdma)
{
	struct nvhost_master *dev = cdma_to_dev(cdma);
	struct nvhost_syncpt *syncpt = &dev->syncpt;
	unsigned int chid = cdma_to_channel(cdma)->chid;
	int i;

	for (i = 0; i < nvhost_syncpt_nb_mlocks(syncpt); i++) {
		unsigned int owner;
		bool ch_own, cpu_own;
		syncpt_op().mutex_owner(syncpt, i, &cpu_own, &ch_own, &owner);

		if (!(ch_own && owner == chid))
			continue;

		syncpt_op().mutex_unlock(&dev->syncpt, i);
		dev_dbg(&dev->dev->dev, "released mlock %d\n", i);
	}

}

static void cdma_timeout_teardown_end(struct nvhost_cdma *cdma, u32 getptr)
{
	struct nvhost_master *dev = cdma_to_dev(cdma);
	struct nvhost_channel *ch = cdma_to_channel(cdma);
	u32 cmdproc_stop;

	dev_dbg(&dev->dev->dev,
		"end channel teardown (id %d, DMAGET restart = 0x%x)\n",
		ch->chid, getptr);

	cmdproc_stop = readl(dev->sync_aperture + host1x_sync_cmdproc_stop_r());
	cmdproc_stop &= ~(BIT(ch->chid));
	writel(cmdproc_stop, dev->sync_aperture + host1x_sync_cmdproc_stop_r());

	cdma_timeout_release_mlocks(cdma);

	cdma->torndown = false;
	cdma_timeout_restart(cdma, getptr);
}

static bool cdma_check_dependencies(struct nvhost_cdma *cdma)
{
	struct nvhost_channel *ch = cdma_to_channel(cdma);
	struct nvhost_master *dev = cdma_to_dev(cdma);
	u32 cbstat = readl(dev->sync_aperture +
		host1x_sync_cbstat_0_r() + 4 * ch->chid);
	u32 cbread = readl(dev->sync_aperture +
		host1x_sync_cbread0_r() + 4 * ch->chid);
	u32 waiting = cbstat & 0x00010008;
	u32 syncpt_id = cbread >> 24;
	int i;

	if (!waiting)
		return false;

	for (i = 0; i < cdma->timeout.num_syncpts; ++i)
		if (cdma->timeout.sp[i].id == syncpt_id)
			return false;

	return true;
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
	int ret;
	bool completed;
	int i;

	u32 syncpt_val;

	u32 prev_cmdproc, cmdproc_stop;

	cdma = container_of(to_delayed_work(work), struct nvhost_cdma,
			    timeout.wq);
	dev = cdma_to_dev(cdma);
	sp = &dev->syncpt;
	ch = cdma_to_channel(cdma);

	ret = mutex_trylock(&cdma->lock);
	if (!ret) {
		schedule_delayed_work(&cdma->timeout.wq, msecs_to_jiffies(10));
		return;
	}

	if (nvhost_debug_force_timeout_dump ||
		cdma->timeout.timeout_debug_dump)
		nvhost_debug_dump_locked(cdma_to_dev(cdma), ch->chid);

	/* is this submit dependent with submits on other channels? */
	if (cdma->timeout.allow_dependency && cdma_check_dependencies(cdma)) {
		dev_dbg(&dev->dev->dev,
			"cdma_timeout: timeout handler rescheduled\n");
		cdma->timeout.allow_dependency = false;
		schedule_delayed_work(&cdma->timeout.wq,
				      msecs_to_jiffies(cdma->timeout.timeout));
		mutex_unlock(&cdma->lock);
		return;
	}

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

	completed = true;
	for (i = 0; completed && i < cdma->timeout.num_syncpts; ++i) {
		syncpt_val = nvhost_syncpt_update_min(&dev->syncpt,
				cdma->timeout.sp[i].id);

		if (!nvhost_syncpt_is_expired(&dev->syncpt,
			cdma->timeout.sp[i].id, cdma->timeout.sp[i].fence))
			completed = false;
	}

	/* has buffer actually completed? */
	if (completed) {
		dev_dbg(&dev->dev->dev,
			 "cdma_timeout: expired, but buffer had completed\n");
		/* restore */
		cmdproc_stop = prev_cmdproc & ~(BIT(ch->chid));
		writel(cmdproc_stop,
			dev->sync_aperture + host1x_sync_cmdproc_stop_r());
		mutex_unlock(&cdma->lock);
		return;
	}

	for (i = 0; i < cdma->timeout.num_syncpts; ++i) {
		syncpt_val = nvhost_syncpt_read_min(&dev->syncpt,
				cdma->timeout.sp[i].id);
		dev_warn(&dev->dev->dev,
			"%s: timeout: %d (%s) ctx 0x%p, HW thresh %d, done %d\n",
			__func__, cdma->timeout.sp[i].id,
			syncpt_op().name(sp, cdma->timeout.sp[i].id),
			cdma->timeout.ctx, syncpt_val,
			cdma->timeout.sp[i].fence);
	}

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
	.timeout_pb_cleanup = cdma_timeout_pb_cleanup,
};

static const struct nvhost_pushbuffer_ops host1x_pushbuffer_ops = {
	.reset = push_buffer_reset,
	.init = push_buffer_init,
	.destroy = push_buffer_destroy,
	.push_to = push_buffer_push_to,
	._push_to = _push_buffer_push_to,
	.pop_from = push_buffer_pop_from,
	.space = push_buffer_space,
	.putptr = push_buffer_putptr,
};

