/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 * Author: Dennis-YC Hsieh <dennis-yc.hsieh@mediatek.com>
 */
#ifndef __MTK_MML_RDMA_GOLDEN_H__
#define __MTK_MML_RDMA_GOLDEN_H__

#include "mtk-mml.h"

#define DMABUF_CON_CNT		4

#define GOLDEN_PIXEL_FHD	(2560 * 1088)
#define GOLDEN_PIXEL_2K		(2750 * 1440)
#define GOLDEN_PIXEL_4K		(3840 * 2176)

struct golden_setting {
	u32 pixel;
	struct threshold {
		u32 preultra;
		u32 ultra;
		u32 urgent;
	} plane[DMABUF_CON_CNT];
};

struct rdma_golden {
	/* golden settings by pixel, must order low to high (fhd->2k->4k) */
	const struct golden_setting *settings;
	u8 cnt;
};

/* Folling part is mt6983 racing mode golden settings */

/* 4K60 ARGB/YUYV 1 plane 4 bpp */
#define MT6983_ARGB_4K_PREULTRA		(828 << 16 | 736)
#define MT6983_ARGB_4K_ULTRA		(644 << 16 | 552)
#define MT6983_ARGB_4K_URGENT		(460 << 16 | 368)
/* 4K60 RGB 1 plane 3 bpp */
#define MT6983_RGB_4K_PREULTRA		(621 << 16 | 552)
#define MT6983_RGB_4K_ULTRA		(483 << 16 | 414)
#define MT6983_RGB_4K_URGENT		(345 << 16 | 276)
/* 4K60 YUV420 2 plane 1/0.5 bpp */
#define MT6983_YUV420_4K_PREULTRA_0	(207 << 16 | 184)
#define MT6983_YUV420_4K_ULTRA_0	(161 << 16 | 138)
#define MT6983_YUV420_4K_URGENT_0	(115 << 16 | 92)
#define MT6983_YUV420_4K_PREULTRA_1	(104 << 16 | 92)
#define MT6983_YUV420_4K_ULTRA_1	(80 << 16 | 69)
#define MT6983_YUV420_4K_URGENT_1	(57 << 16 | 46)

/* 2K120 ARGB/YUYV 1 plane 4 bpp */
#define MT6983_ARGB_2K_PREULTRA		(792 << 16 | 704)
#define MT6983_ARGB_2K_ULTRA		(616 << 16 | 528)
#define MT6983_ARGB_2K_URGENT		(440 << 16 | 352)
/* 2K120 RGB 1 plane 3 bpp */
#define MT6983_RGB_2K_PREULTRA		(594 << 16 | 528)
#define MT6983_RGB_2K_ULTRA		(462 << 16 | 396)
#define MT6983_RGB_2K_URGENT		(330 << 16 | 264)
/* 2K120 YUV420 2 plane 1/0.5 bpp */
#define MT6983_YUV420_2K_PREULTRA_0	(198 << 16 | 176)
#define MT6983_YUV420_2K_ULTRA_0	(154 << 16 | 132)
#define MT6983_YUV420_2K_URGENT_0	(110 << 16 | 88)
#define MT6983_YUV420_2K_PREULTRA_1	(99 << 16 | 88)
#define MT6983_YUV420_2K_ULTRA_1	(77 << 16 | 66)
#define MT6983_YUV420_2K_URGENT_1	(55 << 16 | 44)

/* FHD120 ARGB/YUYV 1 plane 4 bpp */
#define MT6983_ARGB_FHD_PREULTRA	(576 << 16 | 512)
#define MT6983_ARGB_FHD_ULTRA		(448 << 16 | 384)
#define MT6983_ARGB_FHD_URGENT		(320 << 16 | 256)
/* FHD120 RGB 1 plane 3 bpp */
#define MT6983_RGB_FHD_PREULTRA		(432 << 16 | 384)
#define MT6983_RGB_FHD_ULTRA		(336 << 16 | 288)
#define MT6983_RGB_FHD_URGENT		(240 << 16 | 192)
/* FHD120 YUV420 2 plane 1/0.5 bpp */
#define MT6983_YUV420_FHD_PREULTRA_0	(144 << 16 | 128)
#define MT6983_YUV420_FHD_ULTRA_0	(112 << 16 | 96)
#define MT6983_YUV420_FHD_URGENT_0	(80 << 16 | 64)
#define MT6983_YUV420_FHD_PREULTRA_1	(72 << 16 | 64)
#define MT6983_YUV420_FHD_ULTRA_1	(56 << 16 | 48)
#define MT6983_YUV420_FHD_URGENT_1	(40 << 16 | 32)

static const struct golden_setting th_argb_mt6983[] = {
	{
		.pixel = GOLDEN_PIXEL_FHD,
		.plane = {
			{
				.preultra	= MT6983_ARGB_FHD_PREULTRA,
				.ultra		= MT6983_ARGB_FHD_ULTRA,
				.urgent		= MT6983_ARGB_FHD_URGENT,
			},
		},
	}, {
		.pixel = GOLDEN_PIXEL_2K,
		.plane = {
			{
				.preultra	= MT6983_ARGB_2K_PREULTRA,
				.ultra		= MT6983_ARGB_2K_ULTRA,
				.urgent		= MT6983_ARGB_2K_URGENT,
			},
		},
	}, {
		.pixel = GOLDEN_PIXEL_4K,
		.plane = {
			{
				.preultra	= MT6983_ARGB_4K_PREULTRA,
				.ultra		= MT6983_ARGB_4K_ULTRA,
				.urgent		= MT6983_ARGB_4K_URGENT,
			},
		},
	},
};

static const struct golden_setting th_rgb_mt6983[] = {
	{
		.pixel = GOLDEN_PIXEL_FHD,
		.plane = {
			{
				.preultra	= MT6983_RGB_FHD_PREULTRA,
				.ultra		= MT6983_RGB_FHD_ULTRA,
				.urgent		= MT6983_RGB_FHD_URGENT,
			},
		},
	}, {
		.pixel = GOLDEN_PIXEL_2K,
		.plane = {
			{
				.preultra	= MT6983_RGB_2K_PREULTRA,
				.ultra		= MT6983_RGB_2K_ULTRA,
				.urgent		= MT6983_RGB_2K_URGENT,
			},
		},
	}, {
		.pixel = GOLDEN_PIXEL_4K,
		.plane = {
			{
				.preultra	= MT6983_RGB_4K_PREULTRA,
				.ultra		= MT6983_RGB_4K_ULTRA,
				.urgent		= MT6983_RGB_4K_URGENT,
			},
		},
	},
};

static const struct golden_setting th_yuv420_mt6983[] = {
	{
		.pixel = GOLDEN_PIXEL_FHD,
		.plane = {
			{
				.preultra	= MT6983_YUV420_FHD_PREULTRA_0,
				.ultra		= MT6983_YUV420_FHD_ULTRA_0,
				.urgent		= MT6983_YUV420_FHD_URGENT_0,
			}, {
				.preultra	= MT6983_YUV420_FHD_PREULTRA_1,
				.ultra		= MT6983_YUV420_FHD_ULTRA_1,
				.urgent		= MT6983_YUV420_FHD_URGENT_1,
			},
		},
	}, {
		.pixel = GOLDEN_PIXEL_2K,
		.plane = {
			{
				.preultra	= MT6983_YUV420_2K_PREULTRA_0,
				.ultra		= MT6983_YUV420_2K_ULTRA_0,
				.urgent		= MT6983_YUV420_2K_URGENT_0,
			}, {
				.preultra	= MT6983_YUV420_2K_PREULTRA_1,
				.ultra		= MT6983_YUV420_2K_ULTRA_1,
				.urgent		= MT6983_YUV420_2K_URGENT_1,
			},
		},
	}, {
		.pixel = GOLDEN_PIXEL_4K,
		.plane = {
			{
				.preultra	= MT6983_YUV420_4K_PREULTRA_0,
				.ultra		= MT6983_YUV420_4K_ULTRA_0,
				.urgent		= MT6983_YUV420_4K_URGENT_0,
			}, {
				.preultra	= MT6983_YUV420_4K_PREULTRA_1,
				.ultra		= MT6983_YUV420_4K_ULTRA_1,
				.urgent		= MT6983_YUV420_4K_URGENT_1,
			},
		},
	},

};

/* end of mt6983 */

#endif	/* __MTK_MML_RDMA_GOLDEN_H__ */
