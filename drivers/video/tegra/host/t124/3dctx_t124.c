/*
 * drivers/video/tegra/host/t124/3dctx_t124.c
 *
 * Tegra Graphics Host 3d hardware context
 *
 * Copyright (c) 2011-2013, NVIDIA Corporation.  All rights reserved.
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

#include "../nvhost_hwctx.h"
#include "../dev.h"

#include "t124.h"
#include "hardware_t124.h"

#include "../gk20a/gk20a.h"

static void t124_ctx3d_free(struct kref *ref)
{
	struct nvhost_hwctx *ctx = container_of(ref, struct nvhost_hwctx, ref);

#if defined(CONFIG_TEGRA_GK20A)
	gk20a_free_channel(ctx, true);
#endif
	kfree(ctx);
}

struct nvhost_hwctx *t124_3dctx_alloc(struct nvhost_hwctx_handler *h,
			struct nvhost_channel *ch)
{
	struct nvhost_hwctx *ctx;

	nvhost_dbg_fn("");

	/* it seems odd to be allocating a channel here but the
	 * t20/t30 notion of a channel is mapped on top of gk20a's
	 * channel.  this works because there is only one module
	 * under gk20a's host (gr).
	 */
	/* call gk20a_channel_alloc */

	ctx = kzalloc(sizeof(*ctx), GFP_KERNEL);
	if (!ctx)
		return NULL;

	kref_init(&ctx->ref);

#if defined(CONFIG_TEGRA_GK20A)
	return gk20a_open_channel(ch, ctx);
#else
	return ctx;
#endif
}

void t124_3dctx_get(struct nvhost_hwctx *hwctx)
{
	nvhost_dbg_fn("");
	kref_get(&hwctx->ref);
}

void t124_3dctx_put(struct nvhost_hwctx *hwctx)
{
	nvhost_dbg_fn("");
	kref_put(&hwctx->ref, t124_ctx3d_free);
}

void t124_3dctx_save_push(struct nvhost_hwctx *ctx, struct nvhost_cdma *cdma)
{
	nvhost_dbg_fn("");
}

int __init t124_nvhost_3dctx_handler_init(struct nvhost_hwctx_handler *h)
{
	nvhost_dbg_fn("");

	h->alloc = t124_3dctx_alloc;
	h->get   = t124_3dctx_get;
	h->put   = t124_3dctx_put;
	h->save_push = t124_3dctx_save_push;

	return 0;
}

int __init t124_nvhost_mpectx_handler_init(struct nvhost_hwctx_handler *h)
{
	nvhost_dbg_fn("");
	return 0;
}
