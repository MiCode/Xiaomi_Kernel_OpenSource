/*
 * Tegra Graphics Host Hardware Context Interface
 *
 * Copyright (c) 2013, NVIDIA Corporation.  All rights reserved.
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
#include <linux/nvhost_ioctl.h>
#include "user_hwctx.h"
#include "nvhost_cdma.h"
#include "nvhost_memmgr.h"
#include "nvhost_channel.h"
#include "host1x/host1x.h"
#include "host1x/host1x01_hardware.h"

static void user_hwctx_save_push(struct nvhost_hwctx *nctx,
		struct nvhost_cdma *cdma)
{
	struct user_hwctx *ctx = to_user_hwctx(nctx);
	nvhost_cdma_push_gather(cdma,
			nctx->memmgr,
			ctx->save_buf,
			ctx->save_offset,
			nvhost_opcode_gather(ctx->save_size),
			nvhost_memmgr_dma_addr(ctx->save_sgt));
}

static void user_hwctx_restore_push(struct nvhost_hwctx *nctx,
		struct nvhost_cdma *cdma)
{
	struct user_hwctx *ctx = to_user_hwctx(nctx);
	nvhost_cdma_push_gather(cdma,
			nctx->memmgr,
			ctx->restore,
			ctx->restore_offset,
			nvhost_opcode_gather(ctx->restore_size),
			nvhost_memmgr_dma_addr(ctx->restore_sgt));
}

static void user_hwctx_free(struct kref *ref)
{
	struct nvhost_hwctx *hwctx =
		container_of(ref, struct nvhost_hwctx, ref);
	struct user_hwctx *uhwctx = to_user_hwctx(hwctx);

	user_ctxhandler_free(hwctx->h);

	if (uhwctx->save_sgt)
		nvhost_memmgr_unpin(hwctx->memmgr, uhwctx->save_buf,
				&hwctx->channel->dev->dev, uhwctx->save_sgt);
	if (uhwctx->restore_sgt)
		nvhost_memmgr_unpin(hwctx->memmgr, uhwctx->restore,
				&hwctx->channel->dev->dev, uhwctx->restore_sgt);

	if (uhwctx->save_buf)
		nvhost_memmgr_put(hwctx->memmgr, uhwctx->save_buf);

	if (uhwctx->restore)
		nvhost_memmgr_put(hwctx->memmgr, uhwctx->restore);

	nvhost_memmgr_put_mgr(hwctx->memmgr);
	kfree(uhwctx);
}

static void user_hwctx_put(struct nvhost_hwctx *ctx)
{
	kref_put(&ctx->ref, user_hwctx_free);
}

static void user_hwctx_get(struct nvhost_hwctx *ctx)
{
	kref_get(&ctx->ref);
}

static struct nvhost_hwctx *user_hwctx_alloc(struct nvhost_hwctx_handler *h,
		struct nvhost_channel *ch)
{
	struct user_hwctx *hwctx;

	hwctx = kzalloc(sizeof(*hwctx), GFP_KERNEL);

	if (!hwctx)
		return NULL;

	kref_init(&hwctx->hwctx.ref);
	hwctx->hwctx.h = h;
	hwctx->hwctx.channel = ch;
	hwctx->hwctx.valid = false;
	hwctx->hwctx.save_slots = 1;

	return &hwctx->hwctx;
}

int user_hwctx_set_save(struct user_hwctx *ctx,
		ulong mem, u32 offset, u32 words, struct nvhost_reloc *reloc)
{
	struct mem_handle *buf;
	struct sg_table *sgt;
	void *page_addr;

	/* First the restore buffer is set, then the save buffer */
	if (!ctx->restore || !ctx->restore_sgt)
		return -EINVAL;

	buf = nvhost_memmgr_get(ctx->hwctx.memmgr,
			mem, ctx->hwctx.channel->dev);
	if (IS_ERR(buf))
		return PTR_ERR(buf);

	sgt = nvhost_memmgr_pin(ctx->hwctx.memmgr, buf,
			&ctx->hwctx.channel->dev->dev, mem_flag_none);
	if (IS_ERR(sgt))
		return PTR_ERR(sgt);

	ctx->save_offset = offset;
	ctx->save_size = words;
	ctx->save_buf = buf;
	ctx->save_sgt = sgt;

	/* Patch restore buffer address into save buffer */
	page_addr = nvhost_memmgr_kmap(ctx->save_buf,
			reloc->cmdbuf_offset >> PAGE_SHIFT);
	if (!page_addr)
		return -ENOMEM;

	__raw_writel(nvhost_memmgr_dma_addr(ctx->restore_sgt) + offset,
			page_addr + (reloc->cmdbuf_offset & ~PAGE_MASK));
	nvhost_memmgr_kunmap(ctx->save_buf,
			reloc->cmdbuf_offset >> PAGE_SHIFT,
			page_addr);

	return 0;
}

int user_hwctx_set_restore(struct user_hwctx *ctx,
		ulong mem, u32 offset, u32 words)
{
	struct mem_handle *buf;
	struct sg_table *sgt;

	buf = nvhost_memmgr_get(ctx->hwctx.memmgr,
			mem, ctx->hwctx.channel->dev);
	if (IS_ERR(buf))
		return PTR_ERR(buf);

	sgt = nvhost_memmgr_pin(ctx->hwctx.memmgr, buf,
			&ctx->hwctx.channel->dev->dev, mem_flag_none);
	if (IS_ERR(sgt))
		return PTR_ERR(sgt);

	ctx->restore_offset = offset;
	ctx->restore_size = words;
	ctx->restore = buf;
	ctx->restore_sgt = sgt;

	return 0;
}

struct nvhost_hwctx_handler *user_ctxhandler_init(u32 syncpt,
		u32 waitbase, struct nvhost_channel *ch)
{
	struct user_hwctx_handler *p;

	p = kzalloc(sizeof(*p), GFP_KERNEL);
	if (!p)
		return NULL;

	p->h.syncpt = syncpt;
	p->h.waitbase = waitbase;

	p->h.alloc = user_hwctx_alloc;
	p->h.save_push = user_hwctx_save_push;
	p->h.restore_push = user_hwctx_restore_push;
	p->h.get = user_hwctx_get;
	p->h.put = user_hwctx_put;

	return &p->h;
}

void user_ctxhandler_free(struct nvhost_hwctx_handler *h)
{
	kfree(to_user_hwctx_handler(h));
}
