/*
 * drivers/video/tegra/host/nvhost_job.c
 *
 * Tegra Graphics Host Job
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

#include <linux/slab.h>
#include <linux/kref.h>
#include <linux/err.h>
#include <linux/vmalloc.h>
#include <linux/scatterlist.h>
#include <trace/events/nvhost.h>
#include "nvhost_channel.h"
#include "nvhost_job.h"
#include "nvhost_hwctx.h"
#include "nvhost_syncpt.h"
#include "dev.h"
#include "nvhost_memmgr.h"
#include "chip_support.h"

/* Magic to use to fill freed handle slots */
#define BAD_MAGIC 0xdeadbeef

static size_t job_size(u32 num_cmdbufs, u32 num_relocs, u32 num_waitchks)
{
	s64 num_unpins = num_cmdbufs + num_relocs;
	s64 total;

	total = sizeof(struct nvhost_job)
			+ num_relocs * sizeof(struct nvhost_reloc)
			+ num_relocs * sizeof(struct nvhost_reloc_shift)
			+ num_unpins * sizeof(struct nvhost_job_unpin)
			+ num_waitchks * sizeof(struct nvhost_waitchk)
			+ num_cmdbufs * sizeof(struct nvhost_job_gather)
			+ num_unpins * sizeof(dma_addr_t)
			+ num_unpins * sizeof(u32 *);

	if(total > ULONG_MAX)
		return 0;
	return (size_t)total;
}


static void init_fields(struct nvhost_job *job,
		u32 num_cmdbufs, u32 num_relocs, u32 num_waitchks)
{
	int num_unpins = num_cmdbufs + num_relocs;
	void *mem = job;

	/* First init state to zero */

	/*
	 * Redistribute memory to the structs.
	 * Overflows and negative conditions have
	 * already been checked in job_alloc().
	 */
	mem += sizeof(struct nvhost_job);
	job->relocarray = num_relocs ? mem : NULL;
	mem += num_relocs * sizeof(struct nvhost_reloc);
	job->relocshiftarray = num_relocs ? mem : NULL;
	mem += num_relocs * sizeof(struct nvhost_reloc_shift);
	job->unpins = num_unpins ? mem : NULL;
	mem += num_unpins * sizeof(struct nvhost_job_unpin);
	job->waitchk = num_waitchks ? mem : NULL;
	mem += num_waitchks * sizeof(struct nvhost_waitchk);
	job->gathers = num_cmdbufs ? mem : NULL;
	mem += num_cmdbufs * sizeof(struct nvhost_job_gather);
	job->addr_phys = num_unpins ? mem : NULL;
	mem += num_unpins * sizeof(dma_addr_t);
	job->pin_ids = num_unpins ? mem : NULL;

	job->reloc_addr_phys = job->addr_phys;
	job->gather_addr_phys = &job->addr_phys[num_relocs];
}

struct nvhost_job *nvhost_job_alloc(struct nvhost_channel *ch,
		struct nvhost_hwctx *hwctx,
		int num_cmdbufs, int num_relocs, int num_waitchks,
		struct mem_mgr *memmgr)
{
	struct nvhost_job *job = NULL;
	size_t size = job_size(num_cmdbufs, num_relocs, num_waitchks);

	if(!size)
		return NULL;
	job = vzalloc(size);
	if (!job)
		return NULL;

	kref_init(&job->ref);
	job->ch = ch;
	job->hwctx = hwctx;
	if (hwctx)
		hwctx->h->get(hwctx);
	job->memmgr = memmgr ? mem_op().get_mgr(memmgr) : NULL;

	init_fields(job, num_cmdbufs, num_relocs, num_waitchks);

	return job;
}

void nvhost_job_get(struct nvhost_job *job)
{
	kref_get(&job->ref);
}

static void job_free(struct kref *ref)
{
	struct nvhost_job *job = container_of(ref, struct nvhost_job, ref);

	if (job->hwctxref)
		job->hwctxref->h->put(job->hwctxref);
	if (job->hwctx)
		job->hwctx->h->put(job->hwctx);
	if (job->memmgr)
		mem_op().put_mgr(job->memmgr);
	vfree(job);
}

/* Acquire reference to a hardware context. Used for keeping saved contexts in
 * memory. */
void nvhost_job_get_hwctx(struct nvhost_job *job, struct nvhost_hwctx *hwctx)
{
	BUG_ON(job->hwctxref);

	job->hwctxref = hwctx;
	hwctx->h->get(hwctx);
}

void nvhost_job_put(struct nvhost_job *job)
{
	kref_put(&job->ref, job_free);
}

void nvhost_job_add_gather(struct nvhost_job *job,
		u32 mem_id, u32 words, u32 offset)
{
	struct nvhost_job_gather *cur_gather =
			&job->gathers[job->num_gathers];

	cur_gather->words = words;
	cur_gather->mem_id = mem_id;
	cur_gather->offset = offset;
	job->num_gathers += 1;
}

/*
 * Check driver supplied waitchk structs for syncpt thresholds
 * that have already been satisfied and NULL the comparison (to
 * avoid a wrap condition in the HW).
 */
static int do_waitchks(struct nvhost_job *job, struct nvhost_syncpt *sp,
		u32 patch_mem, struct mem_handle *h)
{
	int i;

	/* compare syncpt vs wait threshold */
	for (i = 0; i < job->num_waitchk; i++) {
		struct nvhost_waitchk *wait = &job->waitchk[i];

		/* validate syncpt id */
		if (wait->syncpt_id > nvhost_syncpt_nb_pts(sp))
			continue;

		/* skip all other gathers */
		if (patch_mem != wait->mem)
			continue;

		trace_nvhost_syncpt_wait_check(wait->mem, wait->offset,
				wait->syncpt_id, wait->thresh,
				nvhost_syncpt_read(sp, wait->syncpt_id));
		if (nvhost_syncpt_is_expired(sp,
					wait->syncpt_id, wait->thresh)) {
			void *patch_addr = NULL;

			/*
			 * NULL an already satisfied WAIT_SYNCPT host method,
			 * by patching its args in the command stream. The
			 * method data is changed to reference a reserved
			 * (never given out or incr) NVSYNCPT_GRAPHICS_HOST
			 * syncpt with a matching threshold value of 0, so
			 * is guaranteed to be popped by the host HW.
			 */
			dev_dbg(&syncpt_to_dev(sp)->dev->dev,
			    "drop WAIT id %d (%s) thresh 0x%x, min 0x%x\n",
			    wait->syncpt_id,
			    syncpt_op().name(sp, wait->syncpt_id),
			    wait->thresh,
			    nvhost_syncpt_read_min(sp, wait->syncpt_id));

			/* patch the wait */
			patch_addr = mem_op().kmap(h,
					wait->offset >> PAGE_SHIFT);
			if (patch_addr) {
				nvhost_syncpt_patch_wait(sp,
					(patch_addr +
					 (wait->offset & ~PAGE_MASK)));
				mem_op().kunmap(h,
						wait->offset >> PAGE_SHIFT,
						patch_addr);
			} else {
				pr_err("Couldn't map cmdbuf for wait check\n");
			}
		}

		wait->mem = 0;
	}
	return 0;
}


static int pin_job_mem(struct nvhost_job *job)
{
	int i;
	int count = 0;
	int result;

	for (i = 0; i < job->num_relocs; i++) {
		struct nvhost_reloc *reloc = &job->relocarray[i];
		job->pin_ids[count] = reloc->target;
		count++;
	}

	for (i = 0; i < job->num_gathers; i++) {
		struct nvhost_job_gather *g = &job->gathers[i];
		job->pin_ids[count] = g->mem_id;
		count++;
	}

	/* validate array and pin unique ids, get refs for unpinning */
	result = mem_op().pin_array_ids(job->memmgr, job->ch->dev,
		job->pin_ids, job->addr_phys,
		count,
		job->unpins);

	if (result > 0)
		job->num_unpins = result;

	return result;
}

static int do_relocs(struct nvhost_job *job,
		u32 cmdbuf_mem, struct mem_handle *h)
{
	int i = 0;
	int last_page = -1;
	void *cmdbuf_page_addr = NULL;

	/* pin & patch the relocs for one gather */
	while (i < job->num_relocs) {
		struct nvhost_reloc *reloc = &job->relocarray[i];
		struct nvhost_reloc_shift *shift = &job->relocshiftarray[i];

		/* skip all other gathers */
		if (cmdbuf_mem != reloc->cmdbuf_mem) {
			i++;
			continue;
		}

		if (last_page != reloc->cmdbuf_offset >> PAGE_SHIFT) {
			if (cmdbuf_page_addr)
				mem_op().kunmap(h, last_page, cmdbuf_page_addr);

			cmdbuf_page_addr = mem_op().kmap(h,
					reloc->cmdbuf_offset >> PAGE_SHIFT);
			last_page = reloc->cmdbuf_offset >> PAGE_SHIFT;

			if (unlikely(!cmdbuf_page_addr)) {
				pr_err("Couldn't map cmdbuf for relocation\n");
				return -ENOMEM;
			}
		}

		__raw_writel(
			(job->reloc_addr_phys[i] +
				reloc->target_offset) >> shift->shift,
			(cmdbuf_page_addr +
				(reloc->cmdbuf_offset & ~PAGE_MASK)));

		/* remove completed reloc from the job */
		if (i != job->num_relocs - 1) {
			struct nvhost_reloc *reloc_last =
				&job->relocarray[job->num_relocs - 1];
			struct nvhost_reloc_shift *shift_last =
				&job->relocshiftarray[job->num_relocs - 1];
			reloc->cmdbuf_mem	= reloc_last->cmdbuf_mem;
			reloc->cmdbuf_offset	= reloc_last->cmdbuf_offset;
			reloc->target		= reloc_last->target;
			reloc->target_offset	= reloc_last->target_offset;
			shift->shift		= shift_last->shift;
			job->reloc_addr_phys[i] =
				job->reloc_addr_phys[job->num_relocs - 1];
			job->num_relocs--;
		} else {
			break;
		}
	}

	if (cmdbuf_page_addr)
		mem_op().kunmap(h, last_page, cmdbuf_page_addr);

	return 0;
}


int nvhost_job_pin(struct nvhost_job *job, struct nvhost_syncpt *sp)
{
	int err = 0, i = 0, j = 0;
	unsigned long waitchk_mask[nvhost_syncpt_nb_pts(sp) / BITS_PER_LONG];

	memset(&waitchk_mask[0], 0, sizeof(waitchk_mask));
	for (i = 0; i < job->num_waitchk; i++) {
		u32 syncpt_id = job->waitchk[i].syncpt_id;
		if (syncpt_id < nvhost_syncpt_nb_pts(sp))
			waitchk_mask[BIT_WORD(syncpt_id)]
				|= BIT_MASK(syncpt_id);
	}

	/* get current syncpt values for waitchk */
	for_each_set_bit(i, &waitchk_mask[0], sizeof(waitchk_mask))
		nvhost_syncpt_update_min(sp, i);

	/* pin memory */
	err = pin_job_mem(job);
	if (err <= 0)
		goto fail;

	/* patch gathers */
	for (i = 0; i < job->num_gathers; i++) {
		struct nvhost_job_gather *g = &job->gathers[i];

		/* process each gather mem only once */
		if (!g->ref) {
			g->ref = mem_op().get(job->memmgr,
				g->mem_id, job->ch->dev);
			if (IS_ERR(g->ref)) {
				err = PTR_ERR(g->ref);
				g->ref = NULL;
				break;
			}

			g->mem_base = job->gather_addr_phys[i];

			for (j = 0; j < job->num_gathers; j++) {
				struct nvhost_job_gather *tmp =
					&job->gathers[j];
				if (!tmp->ref && tmp->mem_id == g->mem_id) {
					tmp->ref = g->ref;
					tmp->mem_base = g->mem_base;
				}
			}
			err = do_relocs(job, g->mem_id,  g->ref);
			if (!err)
				err = do_waitchks(job, sp,
						g->mem_id, g->ref);
			mem_op().put(job->memmgr, g->ref);
			if (err)
				break;
		}
	}
fail:
	wmb();

	return err;
}

/*
 * Fast unpin, only for nvmap
 */
void nvhost_job_unpin(struct nvhost_job *job)
{
	int i;

	for (i = 0; i < job->num_unpins; i++) {
		struct nvhost_job_unpin *unpin = &job->unpins[i];
		mem_op().unpin(job->memmgr, unpin->h, unpin->mem);
		mem_op().put(job->memmgr, unpin->h);
	}
	job->num_unpins = 0;
}

/**
 * Debug routine used to dump job entries
 */
void nvhost_job_dump(struct device *dev, struct nvhost_job *job)
{
	dev_info(dev, "    SYNCPT_ID   %d\n",
		job->syncpt_id);
	dev_info(dev, "    SYNCPT_VAL  %d\n",
		job->syncpt_end);
	dev_info(dev, "    FIRST_GET   0x%x\n",
		job->first_get);
	dev_info(dev, "    TIMEOUT     %d\n",
		job->timeout);
	dev_info(dev, "    CTX 0x%p\n",
		job->hwctx);
	dev_info(dev, "    NUM_SLOTS   %d\n",
		job->num_slots);
	dev_info(dev, "    NUM_HANDLES %d\n",
		job->num_unpins);
}
