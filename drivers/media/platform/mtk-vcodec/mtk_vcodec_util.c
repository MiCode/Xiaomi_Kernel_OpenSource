/*
* Copyright (c) 2015 MediaTek Inc.
* Author: PC Chen <pc.chen@mediatek.com>
*         Tiffany Lin <tiffany.lin@mediatek.com>
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License version 2 as
* published by the Free Software Foundation.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*/

#include <linux/module.h>

#include "mtk_vcodec_drv.h"
#include "mtk_vcodec_util.h"
#include "mtk_vpu.h"

bool mtk_vcodec_dbg = false;
int mtk_v4l2_dbg_level = 0;

module_param(mtk_v4l2_dbg_level, int, S_IRUGO | S_IWUSR);
module_param(mtk_vcodec_dbg, bool, S_IRUGO | S_IWUSR);

void __iomem *mtk_vcodec_get_reg_addr(void *data, unsigned int reg_idx)
{
	struct mtk_vcodec_ctx *ctx = (struct mtk_vcodec_ctx *)data;

	if (!data || reg_idx >= NUM_MAX_VCODEC_REG_BASE) {
		mtk_v4l2_err("Invalid arguments");
		return NULL;
	}
	return ctx->dev->reg_base[reg_idx];
}

int mtk_vcodec_mem_alloc(void *data, struct mtk_vcodec_mem *mem)
{
	unsigned long size = mem->size;
	struct mtk_vcodec_ctx *ctx = (struct mtk_vcodec_ctx *)data;
	struct device *dev = &ctx->dev->plat_dev->dev;

	mem->va = dma_alloc_coherent(dev, size, &mem->dma_addr, GFP_KERNEL);

	if (!mem->va) {
		mtk_v4l2_err("%s dma_alloc size=%ld failed!", dev_name(dev),
			     size);
		return -ENOMEM;
	}

	memset(mem->va, 0, size);

	mtk_v4l2_debug(3, "[%d]  - va      = %p", ctx->idx, mem->va);
	mtk_v4l2_debug(3, "[%d]  - dma     = 0x%lx", ctx->idx,
		       (unsigned long)mem->dma_addr);
	mtk_v4l2_debug(3, "[%d]    size = 0x%lx", ctx->idx, size);

	return 0;
}

void mtk_vcodec_mem_free(void *data, struct mtk_vcodec_mem *mem)
{
	unsigned long size = mem->size;
	struct mtk_vcodec_ctx *ctx = (struct mtk_vcodec_ctx *)data;
	struct device *dev = &ctx->dev->plat_dev->dev;

	dma_free_coherent(dev, size, mem->va, mem->dma_addr);
	mem->va = NULL;

	mtk_v4l2_debug(3, "[%d]  - va      = %p", ctx->idx, mem->va);
	mtk_v4l2_debug(3, "[%d]  - dma     = 0x%lx", ctx->idx,
		       (unsigned long)mem->dma_addr);
	mtk_v4l2_debug(3, "[%d]    size = 0x%lx", ctx->idx, size);
}

int mtk_vcodec_get_ctx_id(void *data)
{
	struct mtk_vcodec_ctx *ctx = (struct mtk_vcodec_ctx *)data;

	if (!ctx)
		return -1;

	return ctx->idx;
}

struct platform_device *mtk_vcodec_get_plat_dev(void *data)
{
	struct mtk_vcodec_ctx *ctx = (struct mtk_vcodec_ctx *)data;

	if (!ctx)
		return NULL;

	return vpu_get_plat_device(ctx->dev->plat_dev);
}

void mtk_vcodec_fmt2str(u32 fmt, char *str)
{
	char a = fmt & 0xFF;
	char b = (fmt >> 8) & 0xFF;
	char c = (fmt >> 16) & 0xFF;
	char d = (fmt >> 24) & 0xFF;

	sprintf(str, "%c%c%c%c", a, b, c, d);
}
