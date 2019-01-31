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

#ifndef MTK_WDMA_H
#define MTK_WDMA_H

enum MTK_WDMA_HW_FORMAT {
	MTK_WDMA_HW_FORMAT_RGB565 = 0x00,
	MTK_WDMA_HW_FORMAT_RGB888 = 0x01,
	MTK_WDMA_HW_FORMAT_RGBA8888 = 0x02,
	MTK_WDMA_HW_FORMAT_ARGB8888 = 0x03,
	MTK_WDMA_HW_FORMAT_UYVY = 0x04,
	MTK_WDMA_HW_FORMAT_YUYV = 0x05,
	MTK_WDMA_HW_FORMAT_NV21 = 0x06,
	MTK_WDMA_HW_FORMAT_YV12 = 0x07,
	MTK_WDMA_HW_FORMAT_BGR565 = 0x08,
	MTK_WDMA_HW_FORMAT_BGR888 = 0x09,
	MTK_WDMA_HW_FORMAT_BGRA8888 = 0x0a,
	MTK_WDMA_HW_FORMAT_ABGR8888 = 0x0b,
	MTK_WDMA_HW_FORMAT_VYUY = 0x0c,
	MTK_WDMA_HW_FORMAT_YVYU = 0x0d,
	MTK_WDMA_HW_FORMAT_YONLY = 0x0e,
	MTK_WDMA_HW_FORMAT_NV12 = 0x0f,
	MTK_WDMA_HW_FORMAT_IYUV = 0x10,
	MTK_WDMA_HW_FORMAT_UNKNOWN = 0x100,
};

struct MTK_WDMA_HW_PARAM {
	enum MTK_WDMA_HW_FORMAT in_format;
	enum MTK_WDMA_HW_FORMAT out_format;
	unsigned char alpha;
	unsigned int use_specified_alpha;
	unsigned int src_width;
	unsigned int src_height;
	unsigned int clip_x;
	unsigned int clip_y;
	unsigned int clip_width;
	unsigned int clip_height;
	unsigned long addr_1st_plane;
	unsigned long addr_2nd_plane;
	unsigned long addr_3rd_plane;
};

extern int mtk_wdma_hw_set(
	void *reg_base, struct MTK_WDMA_HW_PARAM *pParam);
extern int mtk_wdma_hw_unset(void *reg_base);
extern int mtk_wdma_hw_irq_clear(void *reg_base);
extern int mtk_ovl_prepare_hw(void);
extern int mtk_ovl_unprepare_hw(void);

#endif /* MTK_WDMA_H */

