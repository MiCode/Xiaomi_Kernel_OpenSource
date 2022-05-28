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

#define SENSOR_FMT_MASK			0xFFFF
unsigned int sensor_mbus_to_ipi_fmt(unsigned int mbus_code);
unsigned int sensor_mbus_to_ipi_pixel_id(unsigned int mbus_code);

#endif //__MTKCAM_FMT_UTILS_H
