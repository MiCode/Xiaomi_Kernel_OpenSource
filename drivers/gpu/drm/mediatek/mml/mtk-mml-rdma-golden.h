/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 * Author: Dennis-YC Hsieh <dennis-yc.hsieh@mediatek.com>
 */
#ifndef __MTK_MML_RDMA_GOLDEN_H__
#define __MTK_MML_RDMA_GOLDEN_H__

#include "mtk-mml.h"

#define GOLDEN_PIXEL_FHD	(2560 * 1080)
#define GOLDEN_PIXEL_4K		(3840 * 2160)

enum rdma_golden_res {
	RDMA_GOLDEN_FHD = 0,
	RDMA_GOLDEN_4K,
	RDMA_GOLDEN_TOTAL
};

struct golden_setting {
	u32 pixel;
	struct threshold {
		u32 preultra;
		u32 ultra;
		u32 urgent;
	} plane[MML_MAX_PLANES];
};

struct rdma_golden {
	/* golden settings by pixel, must order low to high (fhd->2k->4k) */
	const struct golden_setting *settings;
	u8 cnt;
};

/* Folling part is mt6983 racing mode golden settings */

/* 4K60 ARGB */
#define MT6983_ARGB_4K_PREULTRA		(1079 << 16 | 1037)
#define MT6983_ARGB_4K_ULTRA		(1037 << 16 | 954)
#define MT6983_ARGB_4K_URGENT		(498 << 16 | 457)
/* FHD60 ARGB */
#define MT6983_ARGB_FHD_PREULTRA	(360 << 16 | 346)
#define MT6983_ARGB_FHD_ULTRA		(346 << 16 | 318)
#define MT6983_ARGB_FHD_URGENT		(166 << 16 | 153)
/* 4K60 YUV420 */
#define MT6983_YUV420_4K_PREULTRA_0	(180 << 16 | 173)
#define MT6983_YUV420_4K_ULTRA_0	(173 << 16 | 159)
#define MT6983_YUV420_4K_URGENT_0	(83 << 16 | 76)
#define MT6983_YUV420_4K_PREULTRA_1	(91 << 16 | 87)
#define MT6983_YUV420_4K_ULTRA_1	(87 << 16 | 80)
#define MT6983_YUV420_4K_URGENT_1	(42 << 16 | 39)
/* FHD60 YUV420 */
#define MT6983_YUV420_FHD_PREULTRA_0	(60 << 16 | 58)
#define MT6983_YUV420_FHD_ULTRA_0	(58 << 16 | 53)
#define MT6983_YUV420_FHD_URGENT_0	(28 << 16 | 26)
#define MT6983_YUV420_FHD_PREULTRA_1	(31 << 16 | 30)
#define MT6983_YUV420_FHD_ULTRA_1	(30 << 16 | 27)
#define MT6983_YUV420_FHD_URGENT_1	(14 << 16 | 13)

static const struct golden_setting th_argb_mt6983[RDMA_GOLDEN_TOTAL] = {
	[RDMA_GOLDEN_FHD] = {
		.pixel = GOLDEN_PIXEL_FHD,
		.plane = {
			[0] = {
				.preultra	= MT6983_ARGB_FHD_PREULTRA,
				.ultra		= MT6983_ARGB_FHD_ULTRA,
				.urgent		= MT6983_ARGB_FHD_URGENT,
			},
		},
	},
	[RDMA_GOLDEN_4K] = {
		.pixel = GOLDEN_PIXEL_4K,
		.plane = {
			[0] = {
				.preultra	= MT6983_ARGB_4K_PREULTRA,
				.ultra		= MT6983_ARGB_4K_ULTRA,
				.urgent		= MT6983_ARGB_4K_URGENT,
			},
		},
	},
};

static const struct golden_setting th_yuv420_mt6983[RDMA_GOLDEN_TOTAL] = {
	[RDMA_GOLDEN_FHD] = {
		.pixel = GOLDEN_PIXEL_FHD,
		.plane = {
			[0] = {
				.preultra	= MT6983_YUV420_FHD_PREULTRA_0,
				.ultra		= MT6983_YUV420_FHD_ULTRA_0,
				.urgent		= MT6983_YUV420_FHD_URGENT_0,
			},
			[1] = {
				.preultra	= MT6983_YUV420_FHD_PREULTRA_1,
				.ultra		= MT6983_YUV420_FHD_ULTRA_1,
				.urgent		= MT6983_YUV420_FHD_URGENT_1,
			},
		},
	},
	[RDMA_GOLDEN_4K] = {
		.pixel = GOLDEN_PIXEL_4K,
		.plane = {
			[0] = {
				.preultra	= MT6983_YUV420_FHD_PREULTRA_1,
				.ultra		= MT6983_YUV420_FHD_PREULTRA_1,
				.urgent		= MT6983_YUV420_FHD_PREULTRA_1,
			},
			[1] = {
				.preultra	= MT6983_YUV420_4K_PREULTRA_1,
				.ultra		= MT6983_YUV420_4K_PREULTRA_1,
				.urgent		= MT6983_YUV420_4K_PREULTRA_1,
			},
		},
	},

};

/* end of mt6983 */

#endif	/* __MTK_MML_RDMA_GOLDEN_H__ */
