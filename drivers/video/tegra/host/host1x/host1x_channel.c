/*
 * drivers/video/tegra/host/host1x/channel_host1x.c
 *
 * Tegra Graphics Host Channel
 *
 * Copyright (c) 2010-2012, NVIDIA Corporation.
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

#include "nvhost_channel.h"
#include "dev.h"
#include "nvhost_acm.h"
#include "nvhost_job.h"
#include "nvhost_hwctx.h"
#include <trace/events/nvhost.h>
#include <linux/slab.h>

#include "host1x_hwctx.h"
#include "nvhost_intr.h"
#include "class_ids.h"

#define NV_FIFO_READ_TIMEOUT 200000

static int host1x_drain_read_fifo(struct nvhost_channel *ch,
	u32 *ptr, unsigned int count, unsigned int *pending);

static void sync_waitbases(struct nvhost_channel *ch, u32 syncpt_val)
{
	unsigned long waitbase;
	struct nvhost_device_data *pdata = platform_get_drvdata(ch->dev);
	unsigned long int waitbase_mask = pdata->waitbases;
	if (pdata->waitbasesync) {
		waitbase = find_first_bit(&waitbase_mask, BITS_PER_LONG);
		nvhost_cdma_push(&ch->cdma,
			nvhost_opcode_setclass(NV_HOST1X_CLASS_ID,
				host1x_uclass_load_syncpt_base_r(),
				1),
				nvhost_class_host_load_syncpt_base(waitbase,
						syncpt_val));
	}
}

static void *pre_submit_ctxsave(struct nvhost_job *job,
		struct nvhost_hwctx *cur_ctx)
{
	struct nvhost_channel *ch = job->ch;
	void *ctxsave_waiter = NULL;

	/* Is a save needed? */
	if (!cur_ctx || ch->cur_ctx == job->hwctx)
		return NULL;

	if (cur_ctx->has_timedout) {
		dev_dbg(&ch->dev->dev,
			"%s: skip save of timed out context (0x%p)\n",
			__func__, ch->cur_ctx);

		return NULL;
	}

	/* Allocate save waiter if needed */
	if (ch->ctxhandler->save_service) {
		ctxsave_waiter = nvhost_intr_alloc_waiter();
		if (!ctxsave_waiter)
			return ERR_PTR(-ENOMEM);
	}

	return ctxsave_waiter;
}

static void submit_ctxsave(struct nvhost_job *job, void *ctxsave_waiter,
		struct nvhost_hwctx *cur_ctx)
{
	struct nvhost_master *host = nvhost_get_host(job->ch->dev);
	struct nvhost_channel *ch = job->ch;
	u32 syncval;
	int err;
	u32 save_thresh = 0;

	/* Is a save needed? */
	if (!cur_ctx || cur_ctx == job->hwctx || cur_ctx->has_timedout)
		return;

	/* Retrieve save threshold if we have a waiter */
	if (ctxsave_waiter)
		save_thresh =
			nvhost_syncpt_read_max(&host->syncpt, job->syncpt_id)
			+ to_host1x_hwctx(cur_ctx)->save_thresh;

	/* Adjust the syncpoint max */
	job->syncpt_incrs += to_host1x_hwctx(cur_ctx)->save_incrs;
	syncval = nvhost_syncpt_incr_max(&host->syncpt,
			job->syncpt_id,
			to_host1x_hwctx(cur_ctx)->save_incrs);

	/* Send the save to channel */
	cur_ctx->valid = true;
	ch->ctxhandler->save_push(cur_ctx, &ch->cdma);
	nvhost_job_get_hwctx(job, cur_ctx);

	/* Notify save service */
	if (ctxsave_waiter) {
		err = nvhost_intr_add_action(&host->intr,
			job->syncpt_id,
			save_thresh,
			NVHOST_INTR_ACTION_CTXSAVE, cur_ctx,
			ctxsave_waiter,
			NULL);
		ctxsave_waiter = NULL;
		WARN(err, "Failed to set ctx save interrupt");
	}

	trace_nvhost_channel_context_save(ch->dev->name, cur_ctx);
}

static void submit_ctxrestore(struct nvhost_job *job)
{
	struct nvhost_master *host = nvhost_get_host(job->ch->dev);
	struct nvhost_channel *ch = job->ch;
	u32 syncval;
	struct host1x_hwctx *ctx =
		job->hwctx ? to_host1x_hwctx(job->hwctx) : NULL;

	/* First check if we have a valid context to restore */
	if(ch->cur_ctx == job->hwctx || !job->hwctx || !job->hwctx->valid)
		return;

	/* Increment syncpt max */
	job->syncpt_incrs += ctx->restore_incrs;
	syncval = nvhost_syncpt_incr_max(&host->syncpt,
			job->syncpt_id,
			ctx->restore_incrs);

	/* Send restore buffer to channel */
	nvhost_cdma_push_gather(&ch->cdma,
		host->memmgr,
		ctx->restore,
		0,
		nvhost_opcode_gather(ctx->restore_size),
		ctx->restore_phys);

	trace_nvhost_channel_context_restore(ch->dev->name, &ctx->hwctx);
}

static void submit_nullkickoff(struct nvhost_job *job, int user_syncpt_incrs)
{
	struct nvhost_channel *ch = job->ch;
	int incr;
	u32 op_incr;
	struct nvhost_device_data *pdata = platform_get_drvdata(ch->dev);

	/* push increments that correspond to nulled out commands */
	op_incr = nvhost_opcode_imm_incr_syncpt(
			host1x_uclass_incr_syncpt_cond_op_done_v(),
			job->syncpt_id);
	for (incr = 0; incr < (user_syncpt_incrs >> 1); incr++)
		nvhost_cdma_push(&ch->cdma, op_incr, op_incr);
	if (user_syncpt_incrs & 1)
		nvhost_cdma_push(&ch->cdma, op_incr, NVHOST_OPCODE_NOOP);

	/* for 3d, waitbase needs to be incremented after each submit */
	if (pdata->class == NV_GRAPHICS_3D_CLASS_ID) {
		u32 waitbase = to_host1x_hwctx_handler(job->hwctx->h)->waitbase;
		nvhost_cdma_push(&ch->cdma,
			nvhost_opcode_setclass(
				NV_HOST1X_CLASS_ID,
				host1x_uclass_incr_syncpt_base_r(),
				1),
			nvhost_class_host_incr_syncpt_base(
				waitbase,
				user_syncpt_incrs));
	}
}

static inline u32 gather_regnum(u32 word)
{
	return (word >> 16) & 0xfff;
}

static inline  u32 gather_type(u32 word)
{
	return (word >> 28) & 1;
}

static inline u32 gather_count(u32 word)
{
	return word & 0x3fff;
}

static void submit_gathers(struct nvhost_job *job)
{
	/* push user gathers */
	int i;
	for (i = 0 ; i < job->num_gathers; i++) {
		struct nvhost_job_gather *g = &job->gathers[i];
		u32 op1;
		u32 op2;

		/* If register is specified, add a gather with incr/nonincr.
		 * This allows writing large amounts of data directly from
		 * memory to a register. */
		if (gather_regnum(g->words))
			op1 = nvhost_opcode_gather_insert(
					gather_regnum(g->words),
					gather_type(g->words),
					gather_count(g->words));
		else
			op1 = nvhost_opcode_gather(g->words);
		op2 = job->gathers[i].mem_base + g->offset;
		nvhost_cdma_push_gather(&job->ch->cdma,
				job->memmgr,
				g->ref,
				g->offset,
				op1, op2);
	}
}

static int host1x_channel_submit(struct nvhost_job *job)
{
	struct nvhost_channel *ch = job->ch;
	struct nvhost_syncpt *sp = &nvhost_get_host(job->ch->dev)->syncpt;
	u32 user_syncpt_incrs = job->syncpt_incrs;
	u32 prev_max = 0;
	u32 syncval;
	int err;
	void *completed_waiter = NULL, *ctxsave_waiter = NULL;
	struct nvhost_device_data *pdata = platform_get_drvdata(ch->dev);

	/* Bail out on timed out contexts */
	if (job->hwctx && job->hwctx->has_timedout)
		return -ETIMEDOUT;

	/* Turn on the client module and host1x */
	nvhost_module_busy(ch->dev);

	/* before error checks, return current max */
	prev_max = job->syncpt_end =
		nvhost_syncpt_read_max(sp, job->syncpt_id);

	/* get submit lock */
	err = mutex_lock_interruptible(&ch->submitlock);
	if (err) {
		nvhost_module_idle(ch->dev);
		goto error;
	}

	/* Do the needed allocations */
	ctxsave_waiter = pre_submit_ctxsave(job, ch->cur_ctx);
	if (IS_ERR(ctxsave_waiter)) {
		err = PTR_ERR(ctxsave_waiter);
		nvhost_module_idle(ch->dev);
		mutex_unlock(&ch->submitlock);
		goto error;
	}

	completed_waiter = nvhost_intr_alloc_waiter();
	if (!completed_waiter) {
		nvhost_module_idle(ch->dev);
		mutex_unlock(&ch->submitlock);
		err = -ENOMEM;
		goto error;
	}

	/* begin a CDMA submit */
	err = nvhost_cdma_begin(&ch->cdma, job);
	if (err) {
		mutex_unlock(&ch->submitlock);
		nvhost_module_idle(ch->dev);
		goto error;
	}

	if (pdata->serialize) {
		/* Force serialization by inserting a host wait for the
		 * previous job to finish before this one can commence. */
		nvhost_cdma_push(&ch->cdma,
				nvhost_opcode_setclass(NV_HOST1X_CLASS_ID,
					host1x_uclass_wait_syncpt_r(),
					1),
				nvhost_class_host_wait_syncpt(job->syncpt_id,
					nvhost_syncpt_read_max(sp,
						job->syncpt_id)));
	}

	submit_ctxsave(job, ctxsave_waiter, ch->cur_ctx);
	submit_ctxrestore(job);
	ch->cur_ctx = job->hwctx;

	syncval = nvhost_syncpt_incr_max(sp,
			job->syncpt_id, user_syncpt_incrs);

	job->syncpt_end = syncval;

	/* add a setclass for modules that require it */
	if (pdata->class)
		nvhost_cdma_push(&ch->cdma,
			nvhost_opcode_setclass(pdata->class, 0, 0),
			NVHOST_OPCODE_NOOP);

	if (job->null_kickoff)
		submit_nullkickoff(job, user_syncpt_incrs);
	else
		submit_gathers(job);

	sync_waitbases(ch, job->syncpt_end);

	/* end CDMA submit & stash pinned hMems into sync queue */
	nvhost_cdma_end(&ch->cdma, job);

	trace_nvhost_channel_submitted(ch->dev->name,
			prev_max, syncval);

	/* schedule a submit complete interrupt */
	err = nvhost_intr_add_action(&nvhost_get_host(ch->dev)->intr,
			job->syncpt_id, syncval,
			NVHOST_INTR_ACTION_SUBMIT_COMPLETE, ch,
			completed_waiter,
			NULL);
	completed_waiter = NULL;
	WARN(err, "Failed to set submit complete interrupt");

	mutex_unlock(&ch->submitlock);

	return 0;

error:
	kfree(ctxsave_waiter);
	kfree(completed_waiter);
	return err;
}

static int host1x_drain_read_fifo(struct nvhost_channel *ch,
	u32 *ptr, unsigned int count, unsigned int *pending)
{
	unsigned int entries = *pending;
	unsigned long timeout = jiffies + NV_FIFO_READ_TIMEOUT;
	void __iomem *chan_regs = ch->aperture;
	while (count) {
		unsigned int num;

		while (!entries && time_before(jiffies, timeout)) {
			/* query host for number of entries in fifo */
			entries = host1x_channel_fifostat_outfentries_v(
				readl(chan_regs + host1x_channel_fifostat_r()));
			if (!entries)
				cpu_relax();
		}

		/*  timeout -> return error */
		if (!entries)
			return -EIO;

		num = min(entries, count);
		entries -= num;
		count -= num;

		while (num & ~0x3) {
			u32 arr[4];
			arr[0] = readl(chan_regs + host1x_channel_inddata_r());
			arr[1] = readl(chan_regs + host1x_channel_inddata_r());
			arr[2] = readl(chan_regs + host1x_channel_inddata_r());
			arr[3] = readl(chan_regs + host1x_channel_inddata_r());
			memcpy(ptr, arr, 4*sizeof(u32));
			ptr += 4;
			num -= 4;
		}
		while (num--)
			*ptr++ = readl(chan_regs + host1x_channel_inddata_r());
	}
	*pending = entries;

	return 0;
}

static int host1x_save_context(struct nvhost_channel *ch)
{
	struct nvhost_hwctx *hwctx_to_save;
	DECLARE_WAIT_QUEUE_HEAD_ONSTACK(wq);
	u32 syncpt_incrs, syncpt_val;
	int err = 0;
	void *ref;
	void *ctx_waiter = NULL, *wakeup_waiter = NULL;
	struct nvhost_job *job;
	u32 syncpt_id;

	ctx_waiter = nvhost_intr_alloc_waiter();
	wakeup_waiter = nvhost_intr_alloc_waiter();
	if (!ctx_waiter || !wakeup_waiter) {
		err = -ENOMEM;
		goto done;
	}

	mutex_lock(&ch->submitlock);
	hwctx_to_save = ch->cur_ctx;
	if (!hwctx_to_save) {
		mutex_unlock(&ch->submitlock);
		goto done;
	}

	job = nvhost_job_alloc(ch, hwctx_to_save, 0, 0, 0,
			nvhost_get_host(ch->dev)->memmgr);
	if (IS_ERR_OR_NULL(job)) {
		err = PTR_ERR(job);
		mutex_unlock(&ch->submitlock);
		goto done;
	}

	hwctx_to_save->valid = true;
	ch->cur_ctx = NULL;
	syncpt_id = to_host1x_hwctx_handler(hwctx_to_save->h)->syncpt;

	syncpt_incrs = to_host1x_hwctx(hwctx_to_save)->save_incrs;
	syncpt_val = nvhost_syncpt_incr_max(&nvhost_get_host(ch->dev)->syncpt,
					syncpt_id, syncpt_incrs);

	job->syncpt_id = syncpt_id;
	job->syncpt_incrs = syncpt_incrs;
	job->syncpt_end = syncpt_val;

	err = nvhost_cdma_begin(&ch->cdma, job);
	if (err) {
		mutex_unlock(&ch->submitlock);
		goto done;
	}

	ch->ctxhandler->save_push(hwctx_to_save, &ch->cdma);
	nvhost_cdma_end(&ch->cdma, job);
	nvhost_job_put(job);
	job = NULL;

	err = nvhost_intr_add_action(&nvhost_get_host(ch->dev)->intr, syncpt_id,
			syncpt_val - syncpt_incrs +
				to_host1x_hwctx(hwctx_to_save)->save_thresh,
			NVHOST_INTR_ACTION_CTXSAVE, hwctx_to_save,
			ctx_waiter,
			NULL);
	ctx_waiter = NULL;
	WARN(err, "Failed to set context save interrupt");

	err = nvhost_intr_add_action(&nvhost_get_host(ch->dev)->intr,
			syncpt_id, syncpt_val,
			NVHOST_INTR_ACTION_WAKEUP, &wq,
			wakeup_waiter,
			&ref);
	wakeup_waiter = NULL;
	WARN(err, "Failed to set wakeup interrupt");
	wait_event(wq,
		nvhost_syncpt_is_expired(&nvhost_get_host(ch->dev)->syncpt,
				syncpt_id, syncpt_val));

	nvhost_intr_put_ref(&nvhost_get_host(ch->dev)->intr, syncpt_id, ref);

	nvhost_cdma_update(&ch->cdma);

	mutex_unlock(&ch->submitlock);

done:
	kfree(ctx_waiter);
	kfree(wakeup_waiter);
	return err;
}

static inline void __iomem *host1x_channel_aperture(void __iomem *p, int ndx)
{
	p += ndx * NV_HOST1X_CHANNEL_MAP_SIZE_BYTES;
	return p;
}

static inline int host1x_hwctx_handler_init(struct nvhost_channel *ch)
{
	int err = 0;

	struct nvhost_device_data *pdata = platform_get_drvdata(ch->dev);
	unsigned long syncpts = pdata->syncpts;
	unsigned long waitbases = pdata->waitbases;
	u32 syncpt = find_first_bit(&syncpts, BITS_PER_LONG);
	u32 waitbase = find_first_bit(&waitbases, BITS_PER_LONG);

	if (pdata->alloc_hwctx_handler) {
		ch->ctxhandler = pdata->alloc_hwctx_handler(syncpt,
				waitbase, ch);
		if (!ch->ctxhandler)
			err = -ENOMEM;
	}

	return err;
}

static int host1x_channel_init(struct nvhost_channel *ch,
	struct nvhost_master *dev, int index)
{
	ch->chid = index;
	mutex_init(&ch->reflock);
	mutex_init(&ch->submitlock);

	ch->aperture = host1x_channel_aperture(dev->aperture, index);

	return host1x_hwctx_handler_init(ch);
}

static const struct nvhost_channel_ops host1x_channel_ops = {
	.init = host1x_channel_init,
	.submit = host1x_channel_submit,
	.save_context = host1x_save_context,
	.drain_read_fifo = host1x_drain_read_fifo,
};
