/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __MTKCAM_FMT_UTILS_H
#define __MTKCAM_FMT_UTILS_H

#include <linux/types.h>

#define FMT_FOURCC		"%u(%c%c%c%c%s)"
#define MEMBER_FOURCC(pfmt)			\
	(pfmt),					\
	(char)((pfmt) & 0x7f),			\
	(char)(((pfmt) >> 8) & 0x7f),		\
	(char)(((pfmt) >> 16) & 0x7f),		\
	(char)(((pfmt) >> 24) & 0x7f),		\
	((pfmt) & (1UL << 32)) ? "-BE" : ""

struct v4l2_fmtdesc;
void fill_ext_mtkcam_fmtdesc(struct v4l2_fmtdesc *f);

unsigned int mtk_cam_get_pixel_bits(unsigned int ipi_fmt);

int mtk_cam_dmao_xsize(int w, unsigned int ipi_fmt, int pixel_mode_shift);

/* TODO: rename */
unsigned int mtk_cam_get_img_fmt(unsigned int fourcc);

int is_yuv_ufo(unsigned int pixelformat);
int is_raw_ufo(unsigned int pixelformat);
int is_fullg_rb(unsigned int pixelformat);

struct mtk_format_info {
	u32 format;
	u8 mem_planes;
	u8 comp_planes;
	u8 bitpp[4]; /* bits per plane */
	u8 hdiv;
	u8 vdiv;
	u8 bus_align; /* in bytes */
	//u8 block_w[4]; /* in pixels */
	//u8 block_h[4];
};

const struct mtk_format_info *mtk_format_info(u32 format);

unsigned int mtk_format_calc_stride(const struct mtk_format_info *info,
				    unsigned int i,
				    unsigned int w,
				    unsigned int stride0 /* may be 0 */);
unsigned int mtk_format_calc_planesize(const struct mtk_format_info *info,
				       unsigned int i,
				       unsigned int h,
				       unsigned int stride);

struct v4l2_format_info;
unsigned int v4l2_format_calc_stride(const struct v4l2_format_info *info,
				     unsigned int i,
				     unsigned int w,
				     unsigned int stride0 /* may be 0 */);
unsigned int v4l2_format_calc_planesize(const struct v4l2_format_info *info,
					unsigned int i,
					unsigned int h,
					unsigned int stride);

#define SENSOR_FMT_MASK			0xFFFF
unsigned int sensor_mbus_to_ipi_fmt(unsigned int mbus_code);
unsigned int sensor_mbus_to_ipi_pixel_id(unsigned int mbus_code);

#endif //__MTKCAM_FMT_UTILS_H
