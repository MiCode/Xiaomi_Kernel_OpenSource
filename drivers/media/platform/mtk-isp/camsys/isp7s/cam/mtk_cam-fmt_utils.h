/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __MTKCAM_FMT_UTILS_H
#define __MTKCAM_FMT_UTILS_H

struct v4l2_fmtdesc;
void fill_ext_mtkcam_fmtdesc(struct v4l2_fmtdesc *f);

/* TODO: remove from header later */
int mtk_cam_yuv_dma_bus_size(int bpp, int pixel_mode_shift);
unsigned int mtk_cam_get_pixel_bits(unsigned int ipi_fmt);

int mtk_cam_dmao_xsize(int w, unsigned int ipi_fmt, int pixel_mode_shift);

/* TODO: rename */
unsigned int mtk_cam_get_img_fmt(unsigned int fourcc);

int is_mtk_format(unsigned int pixelformat);
int is_yuv_ufo(unsigned int pixelformat);
int is_raw_ufo(unsigned int pixelformat);
int is_fullg_rb(unsigned int pixelformat);

#endif //__MTKCAM_FMT_UTILS_H
