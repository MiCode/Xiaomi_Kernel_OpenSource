/*
 * Copyright (C) 2019 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include <linux/err.h>
#include <linux/module.h>

#include <drm/drmP.h>
#include <drm/drm_atomic_helper.h>
#include <drm/drm_crtc_helper.h>
#include <drm/drm_mipi_dsi.h>

#include <drm/drm_panel.h>

#include "mtk_panel_ext.h"

static DEFINE_MUTEX(panel_ext_lock);
static LIST_HEAD(panel_ext_list);

void mtk_panel_init(struct mtk_panel_ctx *ctx)
{
	INIT_LIST_HEAD(&ctx->list);
}

void mtk_panel_add(struct mtk_panel_ctx *ctx)
{
	mutex_lock(&panel_ext_lock);
	list_add_tail(&ctx->list, &panel_ext_list);
	mutex_unlock(&panel_ext_lock);
}

void mtk_panel_remove(struct mtk_panel_ctx *ctx)
{
	mutex_lock(&panel_ext_lock);
	list_del_init(&ctx->list);
	mutex_unlock(&panel_ext_lock);
}

int mtk_panel_attach(struct mtk_panel_ctx *ctx, struct drm_panel *panel)
{
	if (ctx->panel)
		return -EBUSY;

	ctx->panel = panel;

	return 0;
}

int mtk_panel_detach(struct mtk_panel_ctx *ctx)
{
	ctx->panel = NULL;

	return 0;
}

int mtk_panel_ext_create(struct device *dev,
			 struct mtk_panel_params *ext_params,
			 struct mtk_panel_funcs *ext_funcs,
			 struct drm_panel *panel)
{
	struct mtk_panel_ctx *ext_ctx;
	struct mtk_panel_ext *ext;

	ext_ctx = devm_kzalloc(dev, sizeof(struct mtk_panel_ctx), GFP_KERNEL);
	if (!ext_ctx)
		return -ENOMEM;

	ext = devm_kzalloc(dev, sizeof(struct mtk_panel_ext), GFP_KERNEL);
	if (!ext)
		return -ENOMEM;

	mtk_panel_init(ext_ctx);
	ext->params = ext_params;
	ext->funcs = ext_funcs;
	ext_ctx->ext = ext;

	mtk_panel_add(ext_ctx);
	mtk_panel_attach(ext_ctx, panel);

	return 0;
}

struct mtk_panel_ext *find_panel_ext(struct drm_panel *panel)
{
	//	struct mtk_panel_ext *ext;
	struct mtk_panel_ctx *ctx;

	mutex_lock(&panel_ext_lock);

	list_for_each_entry(ctx, &panel_ext_list, list) {
		if (ctx->panel == panel) {
			mutex_unlock(&panel_ext_lock);
			return ctx->ext;
		}
	}

	mutex_unlock(&panel_ext_lock);
	return NULL;
}
