/*
 * Copyright (c) 2016 MediaTek Inc.
 * Author: Qing Li <qing.li@mediatek.com>
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

#ifndef MTK_OVL_H
#define MTK_OVL_H

#define MTK_OVL_CLK_COUNT 10

enum OVL_LAYER_SOURCE {
	OVL_LAYER_SOURCE_MEM = 0,
	OVL_LAYER_SOURCE_RESERVED = 1,
	OVL_LAYER_SOURCE_SCL = 2,
	OVL_LAYER_SOURCE_PQ = 3,
};

enum OVL_INPUT_FORMAT {
	OVL_INPUT_FORMAT_BGR565 = 0,
	OVL_INPUT_FORMAT_RGB888 = 1,
	OVL_INPUT_FORMAT_RGBA8888 = 2,
	OVL_INPUT_FORMAT_ARGB8888 = 3,
	OVL_INPUT_FORMAT_VYUY = 4,
	OVL_INPUT_FORMAT_YVYU = 5,
	OVL_INPUT_FORMAT_RGB565 = 6,
	OVL_INPUT_FORMAT_BGR888 = 7,
	OVL_INPUT_FORMAT_BGRA8888 = 8,
	OVL_INPUT_FORMAT_ABGR8888 = 9,
	OVL_INPUT_FORMAT_UYVY = 10,
	OVL_INPUT_FORMAT_YUYV = 11,
	OVL_INPUT_FORMAT_UNKNOWN = 32,
};

struct MTK_OVL_HW_PARAM_LAYER {
	enum OVL_LAYER_SOURCE source;
	enum OVL_INPUT_FORMAT fmt;
	unsigned int ovl_index;
	unsigned int layer;
	unsigned int layer_en;
	unsigned long addr;
	unsigned long vaddr;
	unsigned int src_x;
	unsigned int src_y;
	unsigned int src_w;
	unsigned int src_h;
	unsigned int src_pitch;
	unsigned int dst_x;
	unsigned int dst_y;
	unsigned int dst_w;
	unsigned int dst_h;	/* clip region */
	unsigned int keyEn;
	unsigned int key;
	unsigned int aen;
	unsigned char alpha;

	unsigned int sur_aen;
	unsigned int src_alpha;
	unsigned int dst_alpha;

	unsigned int isTdshp;
	unsigned int isDirty;

	unsigned int buff_idx;
	unsigned int identity;
	unsigned int connected_type;
	unsigned int security;
	unsigned int yuv_range;
	unsigned int src_ori_x;
};

struct MTK_OVL_HW_PARAM_ALL {
	struct MTK_OVL_HW_PARAM_LAYER ovl_config[4];
	unsigned int dst_w;
	unsigned int dst_h;
	struct clk *clk[MTK_OVL_CLK_COUNT];
};

extern int mtk_ovl_hw_set(
	void *reg_base, struct MTK_OVL_HW_PARAM_ALL *pConfig);
extern int mtk_ovl_hw_unset(void *reg_base);

#endif /* MTK_OVL_H */

