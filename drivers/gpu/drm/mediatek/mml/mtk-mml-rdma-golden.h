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
	/* golden settings by pixel, must order low to high (fhd->2K->4k) */
	const struct golden_setting *settings;
	u8 cnt;
};

/* begin of mt6983 racing mode golden settings */

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


/* begin of mt6985 racing mode golden settings */

/* 4K60 ARGB/YUYV 1 plane 4 bpp */
#define MT6985_ARGB_4K_URGENT		(315 << 16 | 252)
#define MT6985_ARGB_4K_ULTRA		(441 << 16 | 378)
#define MT6985_ARGB_4K_PREULTRA		(567 << 16 | 504)
/* 4K60 RGB 1 plane 3 bpp */
#define MT6985_RGB_4K_URGENT		(236 << 16 | 189)
#define MT6985_RGB_4K_ULTRA		(330 << 16 | 283)
#define MT6985_RGB_4K_PREULTRA		(425 << 16 | 378)
/* 4K60 YUV420 2 plane 1/0.5 bpp */
#define MT6985_YUV420_4K_URGENT_0	(78 << 16 | 63)
#define MT6985_YUV420_4K_ULTRA_0	(110 << 16 | 94)
#define MT6985_YUV420_4K_PREULTRA_0	(141 << 16 | 126)
#define MT6985_YUV420_4K_URGENT_1	(39 << 16 | 31)
#define MT6985_YUV420_4K_ULTRA_1	(55 << 16 | 47)
#define MT6985_YUV420_4K_PREULTRA_1	(70 << 16 | 63)
/* 4K60 YV12 3 plane 1 bpp */
#define MT6985_YV12_4K_URGENT		(78 << 16 | 63)
#define MT6985_YV12_4K_ULTRA		(110 << 16 | 94)
#define MT6985_YV12_4K_PREULTRA		(141 << 16 | 126)
#define MT6985_YV12_4K_URGENT_1		(19 << 16 | 15)
#define MT6985_YV12_4K_ULTRA_1		(27 << 16 | 23)
#define MT6985_YV12_4K_PREULTRA_1	(35 << 16 | 31)
#define MT6985_YV12_4K_URGENT_2		(19 << 16 | 15)
#define MT6985_YV12_4K_ULTRA_2		(27 << 16 | 23)
#define MT6985_YV12_4K_PREULTRA_2	(35 << 16 | 31)
/* 4K60 AFBC 4 plane 1 bpp */
#define MT6985_AFBC_4K_URGENT		(315 << 16 | 252)
#define MT6985_AFBC_4K_ULTRA		(441 << 16 | 378)
#define MT6985_AFBC_4K_PREULTRA		(567 << 16 | 504)
#define MT6985_AFBC_4K_URGENT_1		(0 << 16 | 0)
#define MT6985_AFBC_4K_ULTRA_1		(0 << 16 | 0)
#define MT6985_AFBC_4K_PREULTRA_1	(0 << 16 | 0)
#define MT6985_AFBC_4K_URGENT_2		(2 << 16 | 1)
#define MT6985_AFBC_4K_ULTRA_2		(3 << 16 | 2)
#define MT6985_AFBC_4K_PREULTRA_2	(4 << 16 | 3)
#define MT6985_AFBC_4K_URGENT_3		(0 << 16 | 0)
#define MT6985_AFBC_4K_ULTRA_3		(0 << 16 | 0)
#define MT6985_AFBC_4K_PREULTRA_3	(0 << 16 | 0)
/* 4K60 HYFBC 4 plane 1 bpp */
#define MT6985_HYFBC_4K_URGENT		(78 << 16 | 63)
#define MT6985_HYFBC_4K_ULTRA		(110 << 16 | 94)
#define MT6985_HYFBC_4K_PREULTRA	(141 << 16 | 126)
#define MT6985_HYFBC_4K_URGENT_1	(39 << 16 | 31)
#define MT6985_HYFBC_4K_ULTRA_1		(55 << 16 | 47)
#define MT6985_HYFBC_4K_PREULTRA_1	(70 << 16 | 63)
#define MT6985_HYFBC_4K_URGENT_2	(2 << 16 | 1)
#define MT6985_HYFBC_4K_ULTRA_2		(2 << 16 | 1)
#define MT6985_HYFBC_4K_PREULTRA_2	(3 << 16 | 1)
#define MT6985_HYFBC_4K_URGENT_3	(1 << 16 | 0)
#define MT6985_HYFBC_4K_ULTRA_3		(2 << 16 | 1)
#define MT6985_HYFBC_4K_PREULTRA_3	(2 << 16 | 1)

/* 2K120 ARGB/YUYV 1 plane 4 bpp */
#define MT6985_ARGB_2K_URGENT		(329 << 16 | 263)
#define MT6985_ARGB_2K_ULTRA		(461 << 16 | 395)
#define MT6985_ARGB_2K_PREULTRA		(593 << 16 | 527)
/* 2K120 RGB 1 plane 3 bpp */
#define MT6985_RGB_2K_URGENT		(247 << 16 | 197)
#define MT6985_RGB_2K_ULTRA		(346 << 16 | 296)
#define MT6985_RGB_2K_PREULTRA		(445 << 16 | 395)
/* 2K120 YUV420 2 plane 1/0.5 bpp */
#define MT6985_YUV420_2K_URGENT_0	(82 << 16 | 65)
#define MT6985_YUV420_2K_ULTRA_0	(115 << 16 | 98)
#define MT6985_YUV420_2K_PREULTRA_0	(148 << 16 | 131)
#define MT6985_YUV420_2K_URGENT_1	(41 << 16 | 32)
#define MT6985_YUV420_2K_ULTRA_1	(57 << 16 | 49)
#define MT6985_YUV420_2K_PREULTRA_1	(74 << 16 | 65)
/* 2K120 YV12 3 plane 1 bpp */
#define MT6985_YV12_2K_URGENT		(82 << 16 | 65)
#define MT6985_YV12_2K_ULTRA		(115 << 16 | 98)
#define MT6985_YV12_2K_PREULTRA		(148 << 16 | 131)
#define MT6985_YV12_2K_URGENT_1		(20 << 16 | 16)
#define MT6985_YV12_2K_ULTRA_1		(28 << 16 | 24)
#define MT6985_YV12_2K_PREULTRA_1	(37 << 16 | 32)
#define MT6985_YV12_2K_URGENT_2		(20 << 16 | 16)
#define MT6985_YV12_2K_ULTRA_2		(28 << 16 | 24)
#define MT6985_YV12_2K_PREULTRA_2	(37 << 16 | 32)
/* 2K120 AFBC 4 plane 1 bpp*/
#define MT6985_AFBC_2K_URGENT		(329 << 16 | 263)
#define MT6985_AFBC_2K_ULTRA		(461 << 16 | 395)
#define MT6985_AFBC_2K_PREULTRA		(593 << 16 | 527)
#define MT6985_AFBC_2K_URGENT_1		(0 << 16 | 0)
#define MT6985_AFBC_2K_ULTRA_1		(0 << 16 | 0)
#define MT6985_AFBC_2K_PREULTRA_1	(0 << 16 | 0)
#define MT6985_AFBC_2K_URGENT_2		(1 << 16 | 0)
#define MT6985_AFBC_2K_ULTRA_2		(1 << 16 | 0)
#define MT6985_AFBC_2K_PREULTRA_2	(2 << 16 | 1)
#define MT6985_AFBC_2K_URGENT_3		(0 << 16 | 0)
#define MT6985_AFBC_2K_ULTRA_3		(0 << 16 | 0)
#define MT6985_AFBC_2K_PREULTRA_3	(0 << 16 | 0)
/* 2K120 HYFBC 4 plane 1 bpp*/
#define MT6985_HYFBC_2K_URGENT		(82 << 16 | 65)
#define MT6985_HYFBC_2K_ULTRA		(115 << 16 | 98)
#define MT6985_HYFBC_2K_PREULTRA	(148 << 16 | 131)
#define MT6985_HYFBC_2K_URGENT_1	(41 << 16 | 32)
#define MT6985_HYFBC_2K_ULTRA_1		(57 << 16 | 49)
#define MT6985_HYFBC_2K_PREULTRA_1	(74 << 16 | 65)
#define MT6985_HYFBC_2K_URGENT_2	(1 << 16 | 0)
#define MT6985_HYFBC_2K_ULTRA_2		(2 << 16 | 1)
#define MT6985_HYFBC_2K_PREULTRA_2	(3 << 16 | 2)
#define MT6985_HYFBC_2K_URGENT_3	(1 << 16 | 0)
#define MT6985_HYFBC_2K_ULTRA_3		(2 << 16 | 1)
#define MT6985_HYFBC_2K_PREULTRA_3	(3 << 16 | 2)

/* FHD120 ARGB/YUYV 1 plane 4 bpp */
#define MT6985_ARGB_FHD_URGENT		(231 << 16 | 185)
#define MT6985_ARGB_FHD_ULTRA		(324 << 16 | 278)
#define MT6985_ARGB_FHD_PREULTRA	(417 << 16 | 370)
/* FHD120 RGB 1 plane 3 bpp */
#define MT6985_RGB_FHD_URGENT		(173 << 16 | 139)
#define MT6985_RGB_FHD_ULTRA		(243 << 16 | 208)
#define MT6985_RGB_FHD_PREULTRA		(313 << 16 | 278)
/* FHD120 YUV420 2 plane 1/0.5 bpp */
#define MT6985_YUV420_FHD_URGENT_0	(57 << 16 | 46)
#define MT6985_YUV420_FHD_ULTRA_0	(81 << 16 | 69)
#define MT6985_YUV420_FHD_PREULTRA_0	(104 << 16 | 92)
#define MT6985_YUV420_FHD_URGENT_1	(28 << 16 | 23)
#define MT6985_YUV420_FHD_ULTRA_1	(40 << 16 | 34)
#define MT6985_YUV420_FHD_PREULTRA_1	(52 << 16 | 46)
/* FHD120 YV12 3 plane 1 bpp */
#define MT6985_YV12_FHD_URGENT		(57 << 16 | 46)
#define MT6985_YV12_FHD_ULTRA		(81 << 16 | 69)
#define MT6985_YV12_FHD_PREULTRA	(104 << 16 | 92)
#define MT6985_YV12_FHD_URGENT_1	(15 << 16 | 11)
#define MT6985_YV12_FHD_ULTRA_1		(21 << 16 | 17)
#define MT6985_YV12_FHD_PREULTRA_1	(27 << 16 | 23)
#define MT6985_YV12_FHD_URGENT_2	(15 << 16 | 11)
#define MT6985_YV12_FHD_ULTRA_2		(21 << 16 | 17)
#define MT6985_YV12_FHD_PREULTRA_2	(27 << 16 | 23)
/* FHD120 AFBC 4 plane 1 bpp*/
#define MT6985_AFBC_FHD_URGENT		(231 << 16 | 185)
#define MT6985_AFBC_FHD_ULTRA		(324 << 16 | 278)
#define MT6985_AFBC_FHD_PREULTRA	(417 << 16 | 370)
#define MT6985_AFBC_FHD_URGENT_1	(0 << 16 | 0)
#define MT6985_AFBC_FHD_ULTRA_1		(0 << 16 | 0)
#define MT6985_AFBC_FHD_PREULTRA_1	(0 << 16 | 0)
#define MT6985_AFBC_FHD_URGENT_2	(1 << 16 | 0)
#define MT6985_AFBC_FHD_ULTRA_2		(1 << 16 | 0)
#define MT6985_AFBC_FHD_PREULTRA_2	(2 << 16 | 1)
#define MT6985_AFBC_FHD_URGENT_3	(0 << 16 | 0)
#define MT6985_AFBC_FHD_ULTRA_3		(0 << 16 | 0)
#define MT6985_AFBC_FHD_PREULTRA_3	(0 << 16 | 0)
/* FHD120 HYFBC 4 plane 1 bpp*/
#define MT6985_HYFBC_FHD_URGENT		(57 << 16 | 46)
#define MT6985_HYFBC_FHD_ULTRA		(81 << 16 | 69)
#define MT6985_HYFBC_FHD_PREULTRA	(104 << 16 | 92)
#define MT6985_HYFBC_FHD_URGENT_1	(28 << 16 | 23)
#define MT6985_HYFBC_FHD_ULTRA_1	(40 << 16 | 34)
#define MT6985_HYFBC_FHD_PREULTRA_1	(52 << 16 | 46)
#define MT6985_HYFBC_FHD_URGENT_2	(1 << 16 | 0)
#define MT6985_HYFBC_FHD_ULTRA_2	(2 << 16 | 1)
#define MT6985_HYFBC_FHD_PREULTRA_2	(3 << 16 | 2)
#define MT6985_HYFBC_FHD_URGENT_3	(1 << 16 | 0)
#define MT6985_HYFBC_FHD_ULTRA_3	(2 << 16 | 1)
#define MT6985_HYFBC_FHD_PREULTRA_3	(3 << 16 | 2)

static const struct golden_setting th_argb_mt6985[] = {
	{
		.pixel = GOLDEN_PIXEL_FHD,
		.plane = {
			{
				.preultra	= MT6985_ARGB_FHD_PREULTRA,
				.ultra		= MT6985_ARGB_FHD_ULTRA,
				.urgent		= MT6985_ARGB_FHD_URGENT,
			},
		},
	}, {
		.pixel = GOLDEN_PIXEL_2K,
		.plane = {
			{
				.preultra	= MT6985_ARGB_2K_PREULTRA,
				.ultra		= MT6985_ARGB_2K_ULTRA,
				.urgent		= MT6985_ARGB_2K_URGENT,
			},
		},
	}, {
		.pixel = GOLDEN_PIXEL_4K,
		.plane = {
			{
				.preultra	= MT6985_ARGB_4K_PREULTRA,
				.ultra		= MT6985_ARGB_4K_ULTRA,
				.urgent		= MT6985_ARGB_4K_URGENT,
			},
		},
	},
};

static const struct golden_setting th_rgb_mt6985[] = {
	{
		.pixel = GOLDEN_PIXEL_FHD,
		.plane = {
			{
				.preultra	= MT6985_RGB_FHD_PREULTRA,
				.ultra		= MT6985_RGB_FHD_ULTRA,
				.urgent		= MT6985_RGB_FHD_URGENT,
			},
		},
	}, {
		.pixel = GOLDEN_PIXEL_2K,
		.plane = {
			{
				.preultra	= MT6985_RGB_2K_PREULTRA,
				.ultra		= MT6985_RGB_2K_ULTRA,
				.urgent		= MT6985_RGB_2K_URGENT,
			},
		},
	}, {
		.pixel = GOLDEN_PIXEL_4K,
		.plane = {
			{
				.preultra	= MT6985_RGB_4K_PREULTRA,
				.ultra		= MT6985_RGB_4K_ULTRA,
				.urgent		= MT6985_RGB_4K_URGENT,
			},
		},
	},
};

static const struct golden_setting th_yuv420_mt6985[] = {
	{
		.pixel = GOLDEN_PIXEL_FHD,
		.plane = {
			{
				.preultra	= MT6985_YUV420_FHD_PREULTRA_0,
				.ultra		= MT6985_YUV420_FHD_ULTRA_0,
				.urgent		= MT6985_YUV420_FHD_URGENT_0,
			}, {
				.preultra	= MT6985_YUV420_FHD_PREULTRA_1,
				.ultra		= MT6985_YUV420_FHD_ULTRA_1,
				.urgent		= MT6985_YUV420_FHD_URGENT_1,
			},
		},
	}, {
		.pixel = GOLDEN_PIXEL_2K,
		.plane = {
			{
				.preultra	= MT6985_YUV420_2K_PREULTRA_0,
				.ultra		= MT6985_YUV420_2K_ULTRA_0,
				.urgent		= MT6985_YUV420_2K_URGENT_0,
			}, {
				.preultra	= MT6985_YUV420_2K_PREULTRA_1,
				.ultra		= MT6985_YUV420_2K_ULTRA_1,
				.urgent		= MT6985_YUV420_2K_URGENT_1,
			},
		},
	}, {
		.pixel = GOLDEN_PIXEL_4K,
		.plane = {
			{
				.preultra	= MT6985_YUV420_4K_PREULTRA_0,
				.ultra		= MT6985_YUV420_4K_ULTRA_0,
				.urgent		= MT6985_YUV420_4K_URGENT_0,
			}, {
				.preultra	= MT6985_YUV420_4K_PREULTRA_1,
				.ultra		= MT6985_YUV420_4K_ULTRA_1,
				.urgent		= MT6985_YUV420_4K_URGENT_1,
			},
		},
	},
};

static const struct golden_setting th_yv12_mt6985[] = {
	{
		.pixel = GOLDEN_PIXEL_FHD,
		.plane = {
			{
				.preultra	= MT6985_YV12_FHD_PREULTRA,
				.ultra		= MT6985_YV12_FHD_ULTRA,
				.urgent		= MT6985_YV12_FHD_URGENT,
			}, {
				.preultra	= MT6985_YV12_FHD_PREULTRA_1,
				.ultra		= MT6985_YV12_FHD_ULTRA_1,
				.urgent		= MT6985_YV12_FHD_URGENT_1,
			}, {
				.preultra	= MT6985_YV12_FHD_PREULTRA_2,
				.ultra		= MT6985_YV12_FHD_ULTRA_2,
				.urgent		= MT6985_YV12_FHD_URGENT_2,
			},

		},
	}, {
		.pixel = GOLDEN_PIXEL_2K,
		.plane = {
			{
				.preultra	= MT6985_YV12_2K_PREULTRA,
				.ultra		= MT6985_YV12_2K_ULTRA,
				.urgent		= MT6985_YV12_2K_URGENT,
			}, {
				.preultra	= MT6985_YV12_2K_PREULTRA_1,
				.ultra		= MT6985_YV12_2K_ULTRA_1,
				.urgent		= MT6985_YV12_2K_URGENT_1,
			}, {
				.preultra	= MT6985_YV12_2K_PREULTRA_2,
				.ultra		= MT6985_YV12_2K_ULTRA_2,
				.urgent		= MT6985_YV12_2K_URGENT_2,
			},

		},
	}, {
		.pixel = GOLDEN_PIXEL_4K,
		.plane = {
			{
				.preultra	= MT6985_YV12_4K_PREULTRA,
				.ultra		= MT6985_YV12_4K_ULTRA,
				.urgent		= MT6985_YV12_4K_URGENT,
			}, {
				.preultra	= MT6985_YV12_4K_PREULTRA_1,
				.ultra		= MT6985_YV12_4K_ULTRA_1,
				.urgent		= MT6985_YV12_4K_URGENT_1,
			}, {
				.preultra	= MT6985_YV12_4K_PREULTRA_2,
				.ultra		= MT6985_YV12_4K_ULTRA_2,
				.urgent		= MT6985_YV12_4K_URGENT_2,
			},

		},
	},
};

static const struct golden_setting th_afbc_mt6985[] = {
	{
		.pixel = GOLDEN_PIXEL_FHD,
		.plane = {
			{
				.preultra	= MT6985_AFBC_FHD_PREULTRA,
				.ultra		= MT6985_AFBC_FHD_ULTRA,
				.urgent		= MT6985_AFBC_FHD_URGENT,
			}, {
				.preultra	= MT6985_AFBC_FHD_PREULTRA_1,
				.ultra		= MT6985_AFBC_FHD_ULTRA_1,
				.urgent		= MT6985_AFBC_FHD_URGENT_1,
			}, {
				.preultra	= MT6985_AFBC_FHD_PREULTRA_2,
				.ultra		= MT6985_AFBC_FHD_ULTRA_2,
				.urgent		= MT6985_AFBC_FHD_URGENT_2,
			}, {
				.preultra	= MT6985_AFBC_FHD_PREULTRA_3,
				.ultra		= MT6985_AFBC_FHD_ULTRA_3,
				.urgent		= MT6985_AFBC_FHD_URGENT_3,
			},

		},
	}, {
		.pixel = GOLDEN_PIXEL_2K,
		.plane = {
			{
				.preultra	= MT6985_AFBC_2K_PREULTRA,
				.ultra		= MT6985_AFBC_2K_ULTRA,
				.urgent		= MT6985_AFBC_2K_URGENT,
			}, {
				.preultra	= MT6985_AFBC_2K_PREULTRA_1,
				.ultra		= MT6985_AFBC_2K_ULTRA_1,
				.urgent		= MT6985_AFBC_2K_URGENT_1,
			}, {
				.preultra	= MT6985_AFBC_2K_PREULTRA_2,
				.ultra		= MT6985_AFBC_2K_ULTRA_2,
				.urgent		= MT6985_AFBC_2K_URGENT_2,
			}, {
				.preultra	= MT6985_AFBC_2K_PREULTRA_3,
				.ultra		= MT6985_AFBC_2K_ULTRA_3,
				.urgent		= MT6985_AFBC_2K_URGENT_3,
			},

		},
	}, {
		.pixel = GOLDEN_PIXEL_4K,
		.plane = {
			{
				.preultra	= MT6985_AFBC_4K_PREULTRA,
				.ultra		= MT6985_AFBC_4K_ULTRA,
				.urgent		= MT6985_AFBC_4K_URGENT,
			}, {
				.preultra	= MT6985_AFBC_4K_PREULTRA_1,
				.ultra		= MT6985_AFBC_4K_ULTRA_1,
				.urgent		= MT6985_AFBC_4K_URGENT_1,
			}, {
				.preultra	= MT6985_AFBC_4K_PREULTRA_2,
				.ultra		= MT6985_AFBC_4K_ULTRA_2,
				.urgent		= MT6985_AFBC_4K_URGENT_2,
			}, {
				.preultra	= MT6985_AFBC_4K_PREULTRA_3,
				.ultra		= MT6985_AFBC_4K_ULTRA_3,
				.urgent		= MT6985_AFBC_4K_URGENT_3,
			},
		},
	},
};

static const struct golden_setting th_hyfbc_mt6985[] = {
	{
		.pixel = GOLDEN_PIXEL_FHD,
		.plane = {
			{
				.preultra	= MT6985_HYFBC_FHD_PREULTRA,
				.ultra		= MT6985_HYFBC_FHD_ULTRA,
				.urgent		= MT6985_HYFBC_FHD_URGENT,
			}, {
				.preultra	= MT6985_HYFBC_FHD_PREULTRA_1,
				.ultra		= MT6985_HYFBC_FHD_ULTRA_1,
				.urgent		= MT6985_HYFBC_FHD_URGENT_1,
			}, {
				.preultra	= MT6985_HYFBC_FHD_PREULTRA_2,
				.ultra		= MT6985_HYFBC_FHD_ULTRA_2,
				.urgent		= MT6985_HYFBC_FHD_URGENT_2,
			}, {
				.preultra	= MT6985_HYFBC_FHD_PREULTRA_3,
				.ultra		= MT6985_HYFBC_FHD_ULTRA_3,
				.urgent		= MT6985_HYFBC_FHD_URGENT_3,
			},

		},
	}, {
		.pixel = GOLDEN_PIXEL_2K,
		.plane = {
			{
				.preultra	= MT6985_HYFBC_2K_PREULTRA,
				.ultra		= MT6985_HYFBC_2K_ULTRA,
				.urgent		= MT6985_HYFBC_2K_URGENT,
			}, {
				.preultra	= MT6985_HYFBC_2K_PREULTRA_1,
				.ultra		= MT6985_HYFBC_2K_ULTRA_1,
				.urgent		= MT6985_HYFBC_2K_URGENT_1,
			}, {
				.preultra	= MT6985_HYFBC_2K_PREULTRA_2,
				.ultra		= MT6985_HYFBC_2K_ULTRA_2,
				.urgent		= MT6985_HYFBC_2K_URGENT_2,
			}, {
				.preultra	= MT6985_HYFBC_2K_PREULTRA_3,
				.ultra		= MT6985_HYFBC_2K_ULTRA_3,
				.urgent		= MT6985_HYFBC_2K_URGENT_3,
			},

		},
	}, {
		.pixel = GOLDEN_PIXEL_4K,
		.plane = {
			{
				.preultra	= MT6985_HYFBC_4K_PREULTRA,
				.ultra		= MT6985_HYFBC_4K_ULTRA,
				.urgent		= MT6985_HYFBC_4K_URGENT,
			}, {
				.preultra	= MT6985_HYFBC_4K_PREULTRA_1,
				.ultra		= MT6985_HYFBC_4K_ULTRA_1,
				.urgent		= MT6985_HYFBC_4K_URGENT_1,
			}, {
				.preultra	= MT6985_HYFBC_4K_PREULTRA_2,
				.ultra		= MT6985_HYFBC_4K_ULTRA_2,
				.urgent		= MT6985_HYFBC_4K_URGENT_2,
			}, {
				.preultra	= MT6985_HYFBC_4K_PREULTRA_3,
				.ultra		= MT6985_HYFBC_4K_ULTRA_3,
				.urgent		= MT6985_HYFBC_4K_URGENT_3,
			},
		},
	},
};
/* end of mt6985 */

/* begin of mt6886 racing mode golden settings */

/* ARGB/YUYV 1 plane 4 bpp */
#define MT6886_ARGB_FHD_PREULTRA	(990 << 16 | 880)
#define MT6886_ARGB_FHD_ULTRA		(770 << 16 | 660)
#define MT6886_ARGB_FHD_URGENT		(550 << 16 | 440)
/* RGB 1 plane 3 bpp */
#define MT6886_RGB_FHD_PREULTRA		(990 << 16 | 880)
#define MT6886_RGB_FHD_ULTRA		(770 << 16 | 660)
#define MT6886_RGB_FHD_URGENT		(550 << 16 | 440)
/* YUV420/NV21/NV12 2 plane 1 bpp */
#define MT6886_YUV420_FHD_PREULTRA_0	(660 << 16 | 586)
#define MT6886_YUV420_FHD_ULTRA_0	(513 << 16 | 440)
#define MT6886_YUV420_FHD_URGENT_0	(367 << 16 | 293)
#define MT6886_YUV420_FHD_PREULTRA_1	(330 << 16 | 294)
#define MT6886_YUV420_FHD_ULTRA_1	(257 << 16 | 220)
#define MT6886_YUV420_FHD_URGENT_1	(184 << 16 | 147)
/* YV12 3 plane 1 bpp */
#define MT6886_YV12_FHD_PREULTRA	(660 << 16 | 586)
#define MT6886_YV12_FHD_ULTRA		(513 << 16 | 440)
#define MT6886_YV12_FHD_URGENT		(367 << 16 | 293)
#define MT6886_YV12_FHD_PREULTRA_1	(165 << 16 | 147)
#define MT6886_YV12_FHD_ULTRA_1		(128 << 16 | 110)
#define MT6886_YV12_FHD_URGENT_1	(92 << 16 | 73)
#define MT6886_YV12_FHD_PREULTRA_2	(165 << 16 | 147)
#define MT6886_YV12_FHD_ULTRA_2		(128 << 16 | 110)
#define MT6886_YV12_FHD_URGENT_2	(92 << 16 | 73)
/* AFBC 4 plane 1 bpp */
#define MT6886_AFBC_FHD_PREULTRA	(961 << 16 | 854)
#define MT6886_AFBC_FHD_ULTRA		(748 << 16 | 641)
#define MT6886_AFBC_FHD_URGENT		(534 << 16 | 427)
#define MT6886_AFBC_FHD_PREULTRA_1	(0 << 16 | 0)
#define MT6886_AFBC_FHD_ULTRA_1		(0 << 16 | 0)
#define MT6886_AFBC_FHD_URGENT_1	(0 << 16 | 0)
#define MT6886_AFBC_FHD_PREULTRA_2	(8 << 16 | 16)
#define MT6886_AFBC_FHD_ULTRA_2		(0 << 16 | 0)
#define MT6886_AFBC_FHD_URGENT_2	(0 << 16 | 0)
#define MT6886_AFBC_FHD_PREULTRA_3	(0 << 16 | 0)
#define MT6886_AFBC_FHD_ULTRA_3		(0 << 16 | 0)
#define MT6886_AFBC_FHD_URGENT_3	(0 << 16 | 0)

static const struct golden_setting th_argb_mt6886[] = {
	{
		.pixel = GOLDEN_PIXEL_FHD,
		.plane = {
			{
				.preultra	= MT6886_ARGB_FHD_PREULTRA,
				.ultra		= MT6886_ARGB_FHD_ULTRA,
				.urgent		= MT6886_ARGB_FHD_URGENT,
			},
		},
	},
};

static const struct golden_setting th_rgb_mt6886[] = {
	{
		.pixel = GOLDEN_PIXEL_FHD,
		.plane = {
			{
				.preultra	= MT6886_RGB_FHD_PREULTRA,
				.ultra		= MT6886_RGB_FHD_ULTRA,
				.urgent		= MT6886_RGB_FHD_URGENT,
			},
		},
	},
};

static const struct golden_setting th_yuv420_mt6886[] = {
	{
		.pixel = GOLDEN_PIXEL_FHD,
		.plane = {
			{
				.preultra	= MT6886_YUV420_FHD_PREULTRA_0,
				.ultra		= MT6886_YUV420_FHD_ULTRA_0,
				.urgent		= MT6886_YUV420_FHD_URGENT_0,
			}, {
				.preultra	= MT6886_YUV420_FHD_PREULTRA_1,
				.ultra		= MT6886_YUV420_FHD_ULTRA_1,
				.urgent		= MT6886_YUV420_FHD_URGENT_1,
			},
		},
	},
};

static const struct golden_setting th_yv12_mt6886[] = {
	{
		.pixel = GOLDEN_PIXEL_FHD,
		.plane = {
			{
				.preultra	= MT6886_YV12_FHD_PREULTRA,
				.ultra		= MT6886_YV12_FHD_ULTRA,
				.urgent		= MT6886_YV12_FHD_URGENT,
			}, {
				.preultra	= MT6886_YV12_FHD_PREULTRA_1,
				.ultra		= MT6886_YV12_FHD_ULTRA_1,
				.urgent		= MT6886_YV12_FHD_URGENT_1,
			}, {
				.preultra	= MT6886_YV12_FHD_PREULTRA_2,
				.ultra		= MT6886_YV12_FHD_ULTRA_2,
				.urgent		= MT6886_YV12_FHD_URGENT_2,
			},

		},
	},
};

static const struct golden_setting th_afbc_mt6886[] = {
	{
		.pixel = GOLDEN_PIXEL_FHD,
		.plane = {
			{
				.preultra	= MT6886_AFBC_FHD_PREULTRA,
				.ultra		= MT6886_AFBC_FHD_ULTRA,
				.urgent		= MT6886_AFBC_FHD_URGENT,
			}, {
				.preultra	= MT6886_AFBC_FHD_PREULTRA_1,
				.ultra		= MT6886_AFBC_FHD_ULTRA_1,
				.urgent		= MT6886_AFBC_FHD_URGENT_1,
			}, {
				.preultra	= MT6886_AFBC_FHD_PREULTRA_2,
				.ultra		= MT6886_AFBC_FHD_ULTRA_2,
				.urgent		= MT6886_AFBC_FHD_URGENT_2,
			}, {
				.preultra	= MT6886_AFBC_FHD_PREULTRA_3,
				.ultra		= MT6886_AFBC_FHD_ULTRA_3,
				.urgent		= MT6886_AFBC_FHD_URGENT_3,
			},

		},
	},
};
/* end of mt6886 */

#endif	/* __MTK_MML_RDMA_GOLDEN_H__ */
