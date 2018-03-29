/*
 * Copyright (c) 2014 MediaTek Inc.
 * Author: Jie Qiu <jie.qiu@mediatek.com>
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
#ifndef _MTK_DPI_H
#define _MTK_DPI_H

#include <linux/types.h>
#include <drm/drm_crtc.h>

#include "mtk_drm_ddp_comp.h"

struct device;
struct clk;

enum mtk_dpi_out_bit_num {
	MTK_DPI_OUT_BIT_NUM_8BITS,
	MTK_DPI_OUT_BIT_NUM_10BITS,
	MTK_DPI_OUT_BIT_NUM_12BITS,
	MTK_DPI_OUT_BIT_NUM_16BITS
};

enum mtk_dpi_out_yc_map {
	MTK_DPI_OUT_YC_MAP_RGB,
	MTK_DPI_OUT_YC_MAP_CYCY,
	MTK_DPI_OUT_YC_MAP_YCYC,
	MTK_DPI_OUT_YC_MAP_CY,
	MTK_DPI_OUT_YC_MAP_YC
};

enum mtk_dpi_out_channel_swap {
	MTK_DPI_OUT_CHANNEL_SWAP_RGB,
	MTK_DPI_OUT_CHANNEL_SWAP_GBR,
	MTK_DPI_OUT_CHANNEL_SWAP_BRG,
	MTK_DPI_OUT_CHANNEL_SWAP_RBG,
	MTK_DPI_OUT_CHANNEL_SWAP_GRB,
	MTK_DPI_OUT_CHANNEL_SWAP_BGR
};

enum mtk_dpi_out_color_format {
	MTK_DPI_COLOR_FORMAT_RGB,
	MTK_DPI_COLOR_FORMAT_RGB_FULL,
	MTK_DPI_COLOR_FORMAT_YCBCR_444,
	MTK_DPI_COLOR_FORMAT_YCBCR_422,
	MTK_DPI_COLOR_FORMAT_XV_YCC,
	MTK_DPI_COLOR_FORMAT_YCBCR_444_FULL,
	MTK_DPI_COLOR_FORMAT_YCBCR_422_FULL
};

struct mtk_dpi {
	struct mtk_ddp_comp ddp_comp;
	struct drm_encoder encoder;
	void __iomem *regs;
	struct device *dev;
	struct clk *engine_clk;
	struct clk *pixel_clk;
	struct clk *sel;
	struct clk *tvd_div1;
	struct clk *tvd_div2;
	struct clk *tvd_div4;
	struct clk *tvd_div8;
	int irq;
	struct drm_display_mode mode;
	enum mtk_dpi_out_color_format color_format;
	enum mtk_dpi_out_yc_map yc_map;
	enum mtk_dpi_out_bit_num bit_num;
	enum mtk_dpi_out_channel_swap channel_swap;
	int refcount;
	void *data;
};

static inline struct mtk_dpi *mtk_dpi_from_encoder(struct drm_encoder *e)
{
	return container_of(e, struct mtk_dpi, encoder);
}

#endif /* _MTK_DPI_H */
