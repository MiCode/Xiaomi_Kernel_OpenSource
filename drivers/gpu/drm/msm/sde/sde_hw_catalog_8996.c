/* Copyright (c) 2015-2016, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include "sde_hw_catalog.h"
#include "sde_hw_mdss.h"
#include "sde_hwio.h"

/* VIG layer capability */
#define VIG_17X_MASK \
	(BIT(SDE_SSPP_SRC) | BIT(SDE_SSPP_SCALER_QSEED2) |\
	BIT(SDE_SSPP_CSC) | BIT(SDE_SSPP_PA_V1) |\
	BIT(SDE_SSPP_HIST_V1) | BIT(SDE_SSPP_PCC) |\
	BIT(SDE_SSPP_IGC))

/* RGB layer capability */
#define RGB_17X_MASK \
	(BIT(SDE_SSPP_SRC) | BIT(SDE_SSPP_SCALER_RGB) |\
	BIT(SDE_SSPP_PCC) | BIT(SDE_SSPP_IGC))

/* DMA layer capability */
#define DMA_17X_MASK \
	(BIT(SDE_SSPP_SRC) | BIT(SDE_SSPP_PA_V1) |\
	BIT(SDE_SSPP_PCC) | BIT(SDE_SSPP_IGC))

/* Cursor layer capability */
#define CURSOR_17X_MASK  (BIT(SDE_SSPP_SRC) | BIT(SDE_SSPP_CURSOR))

#define MIXER_17X_MASK (BIT(SDE_MIXER_SOURCESPLIT) |\
	BIT(SDE_MIXER_GC))

#define DSPP_17X_MASK \
	(BIT(SDE_DSPP_IGC) | BIT(SDE_DSPP_PCC) |\
	BIT(SDE_DSPP_GC) | BIT(SDE_DSPP_PA) | BIT(SDE_DSPP_GAMUT) |\
	BIT(SDE_DSPP_DITHER) | BIT(SDE_DSPP_HIST))

#define PINGPONG_17X_MASK \
	(BIT(SDE_PINGPONG_TE) | BIT(SDE_PINGPONG_DSC))

#define PINGPONG_17X_SPLIT_MASK \
	(PINGPONG_17X_MASK | BIT(SDE_PINGPONG_SPLIT) |\
	BIT(SDE_PINGPONG_TE2))

#define WB01_17X_MASK \
	(BIT(SDE_WB_LINE_MODE) | BIT(SDE_WB_BLOCK_MODE) |\
	BIT(SDE_WB_CSC) | BIT(SDE_WB_CHROMA_DOWN) | BIT(SDE_WB_DOWNSCALE) |\
	BIT(SDE_WB_DITHER) | BIT(SDE_WB_TRAFFIC_SHAPER) |\
	BIT(SDE_WB_UBWC_1_0) | BIT(SDE_WB_YUV_CONFIG))

#define WB2_17X_MASK \
	(BIT(SDE_WB_LINE_MODE) | BIT(SDE_WB_TRAFFIC_SHAPER) |\
	BIT(SDE_WB_YUV_CONFIG))

#define DECIMATION_17X_MAX_H	4
#define DECIMATION_17X_MAX_V	4

static const struct sde_format_extended plane_formats[] = {
	{DRM_FORMAT_ARGB8888, 0},
	{DRM_FORMAT_ABGR8888, 0},
	{DRM_FORMAT_RGBA8888, 0},
	{DRM_FORMAT_RGBA8888, DRM_FORMAT_MOD_QCOM_COMPRESSED},
	{DRM_FORMAT_BGRA8888, 0},
	{DRM_FORMAT_XRGB8888, 0},
	{DRM_FORMAT_RGBX8888, 0},
	{DRM_FORMAT_RGBX8888, DRM_FORMAT_MOD_QCOM_COMPRESSED},
	{DRM_FORMAT_RGB888, 0},
	{DRM_FORMAT_BGR888, 0},
	{DRM_FORMAT_RGB565, 0},
	{DRM_FORMAT_RGB565, DRM_FORMAT_MOD_QCOM_COMPRESSED},
	{DRM_FORMAT_BGR565, 0},
	{DRM_FORMAT_ARGB1555, 0},
	{DRM_FORMAT_ABGR1555, 0},
	{DRM_FORMAT_RGBA5551, 0},
	{DRM_FORMAT_BGRA5551, 0},
	{DRM_FORMAT_XRGB1555, 0},
	{DRM_FORMAT_XBGR1555, 0},
	{DRM_FORMAT_RGBX5551, 0},
	{DRM_FORMAT_BGRX5551, 0},
	{DRM_FORMAT_ARGB4444, 0},
	{DRM_FORMAT_ABGR4444, 0},
	{DRM_FORMAT_RGBA4444, 0},
	{DRM_FORMAT_BGRA4444, 0},
	{DRM_FORMAT_XRGB4444, 0},
	{DRM_FORMAT_XBGR4444, 0},
	{DRM_FORMAT_RGBX4444, 0},
	{DRM_FORMAT_BGRX4444, 0},
	{0, 0},
};

static const struct sde_format_extended plane_formats_yuv[] = {
	{DRM_FORMAT_ARGB8888, 0},
	{DRM_FORMAT_ABGR8888, 0},
	{DRM_FORMAT_RGBA8888, 0},
	{DRM_FORMAT_RGBA8888, DRM_FORMAT_MOD_QCOM_COMPRESSED},
	{DRM_FORMAT_BGRA8888, 0},
	{DRM_FORMAT_XRGB8888, 0},
	{DRM_FORMAT_RGBX8888, 0},
	{DRM_FORMAT_RGBX8888, DRM_FORMAT_MOD_QCOM_COMPRESSED},
	{DRM_FORMAT_RGB888, 0},
	{DRM_FORMAT_BGR888, 0},
	{DRM_FORMAT_RGB565, 0},
	{DRM_FORMAT_RGB565, DRM_FORMAT_MOD_QCOM_COMPRESSED},
	{DRM_FORMAT_BGR565, 0},
	{DRM_FORMAT_ARGB1555, 0},
	{DRM_FORMAT_ABGR1555, 0},
	{DRM_FORMAT_RGBA5551, 0},
	{DRM_FORMAT_BGRA5551, 0},
	{DRM_FORMAT_XRGB1555, 0},
	{DRM_FORMAT_XBGR1555, 0},
	{DRM_FORMAT_RGBX5551, 0},
	{DRM_FORMAT_BGRX5551, 0},
	{DRM_FORMAT_ARGB4444, 0},
	{DRM_FORMAT_ABGR4444, 0},
	{DRM_FORMAT_RGBA4444, 0},
	{DRM_FORMAT_BGRA4444, 0},
	{DRM_FORMAT_XRGB4444, 0},
	{DRM_FORMAT_XBGR4444, 0},
	{DRM_FORMAT_RGBX4444, 0},
	{DRM_FORMAT_BGRX4444, 0},

	{DRM_FORMAT_NV12, 0},
	{DRM_FORMAT_NV12, DRM_FORMAT_MOD_QCOM_COMPRESSED},
	{DRM_FORMAT_NV21, 0},
	{DRM_FORMAT_NV16, 0},
	{DRM_FORMAT_NV61, 0},
	{DRM_FORMAT_VYUY, 0},
	{DRM_FORMAT_UYVY, 0},
	{DRM_FORMAT_YUYV, 0},
	{DRM_FORMAT_YVYU, 0},
	{DRM_FORMAT_YUV420, 0},
	{DRM_FORMAT_YVU420, 0},
	{0, 0},
};

static const struct sde_format_extended wb0_formats[] = {
	{DRM_FORMAT_RGB565, 0},
	{DRM_FORMAT_RGB888, 0},
	{DRM_FORMAT_ARGB8888, 0},
	{DRM_FORMAT_RGBA8888, 0},
	{DRM_FORMAT_XRGB8888, 0},
	{DRM_FORMAT_RGBX8888, 0},
	{DRM_FORMAT_ARGB1555, 0},
	{DRM_FORMAT_RGBA5551, 0},
	{DRM_FORMAT_XRGB1555, 0},
	{DRM_FORMAT_RGBX5551, 0},
	{DRM_FORMAT_ARGB4444, 0},
	{DRM_FORMAT_RGBA4444, 0},
	{DRM_FORMAT_RGBX4444, 0},
	{DRM_FORMAT_XRGB4444, 0},

	{DRM_FORMAT_BGR565, 0},
	{DRM_FORMAT_BGR888, 0},
	{DRM_FORMAT_ABGR8888, 0},
	{DRM_FORMAT_BGRA8888, 0},
	{DRM_FORMAT_BGRX8888, 0},
	{DRM_FORMAT_ABGR1555, 0},
	{DRM_FORMAT_BGRA5551, 0},
	{DRM_FORMAT_XBGR1555, 0},
	{DRM_FORMAT_BGRX5551, 0},
	{DRM_FORMAT_ABGR4444, 0},
	{DRM_FORMAT_BGRA4444, 0},
	{DRM_FORMAT_BGRX4444, 0},
	{DRM_FORMAT_XBGR4444, 0},

	{DRM_FORMAT_RGBX8888, DRM_FORMAT_MOD_QCOM_COMPRESSED},
	{DRM_FORMAT_RGBA8888, DRM_FORMAT_MOD_QCOM_COMPRESSED},
	{DRM_FORMAT_RGB565, DRM_FORMAT_MOD_QCOM_COMPRESSED},

	{DRM_FORMAT_YUV420, 0},
	{DRM_FORMAT_NV12, 0},
	{DRM_FORMAT_NV16, 0},
	{DRM_FORMAT_YUYV, 0},

	{DRM_FORMAT_NV12, DRM_FORMAT_MOD_QCOM_COMPRESSED},
	{DRM_FORMAT_AYUV, DRM_FORMAT_MOD_QCOM_COMPRESSED},

	{0, 0},
};

static const struct sde_format_extended wb2_formats[] = {
	{DRM_FORMAT_RGB565, 0},
	{DRM_FORMAT_RGB888, 0},
	{DRM_FORMAT_ARGB8888, 0},
	{DRM_FORMAT_RGBA8888, 0},
	{DRM_FORMAT_XRGB8888, 0},
	{DRM_FORMAT_RGBX8888, 0},
	{DRM_FORMAT_ARGB1555, 0},
	{DRM_FORMAT_RGBA5551, 0},
	{DRM_FORMAT_XRGB1555, 0},
	{DRM_FORMAT_RGBX5551, 0},
	{DRM_FORMAT_ARGB4444, 0},
	{DRM_FORMAT_RGBA4444, 0},
	{DRM_FORMAT_RGBX4444, 0},
	{DRM_FORMAT_XRGB4444, 0},

	{DRM_FORMAT_BGR565, 0},
	{DRM_FORMAT_BGR888, 0},
	{DRM_FORMAT_ABGR8888, 0},
	{DRM_FORMAT_BGRA8888, 0},
	{DRM_FORMAT_BGRX8888, 0},
	{DRM_FORMAT_ABGR1555, 0},
	{DRM_FORMAT_BGRA5551, 0},
	{DRM_FORMAT_XBGR1555, 0},
	{DRM_FORMAT_BGRX5551, 0},
	{DRM_FORMAT_ABGR4444, 0},
	{DRM_FORMAT_BGRA4444, 0},
	{DRM_FORMAT_BGRX4444, 0},
	{DRM_FORMAT_XBGR4444, 0},

	{DRM_FORMAT_YUV420, 0},
	{DRM_FORMAT_NV12, 0},
	{DRM_FORMAT_NV16, 0},
	{DRM_FORMAT_YUYV, 0},

	{0, 0},
};

/**
 * set_cfg_1xx_init(): populate sde sub-blocks reg offsets and instance counts
 */
static inline int set_cfg_1xx_init(struct sde_mdss_cfg *cfg)
{

	/* Layer capability */
	static const struct sde_sspp_sub_blks vig_layer = {
		.maxlinewidth = 2560,
		.danger_lut = 0xFFFF,
		.safe_lut = 0xFF00,
		.maxdwnscale = 4, .maxupscale = 20,
		.maxhdeciexp = DECIMATION_17X_MAX_H,
		.maxvdeciexp = DECIMATION_17X_MAX_V,
		.src_blk = {.id = SDE_SSPP_SRC,
			.base = 0x00, .len = 0x150,},
		.scaler_blk = {.id = SDE_SSPP_SCALER_QSEED2,
			.base = 0x200, .len = 0x70,},
		.csc_blk = {.id = SDE_SSPP_CSC,
			.base = 0x320, .len = 0x44,},
		.pa_blk = {.id = SDE_SSPP_PA_V1,
			.base = 0x200, .len = 0x0,},
		.hist_lut = {.id = SDE_SSPP_HIST_V1,
			.base = 0xA00, .len = 0x400,},
		.pcc_blk = {.id = SDE_SSPP_PCC,
			.base = 0x1780, .len = 0x64,},
		.format_list = plane_formats_yuv,
	};

	static const struct sde_sspp_sub_blks layer = {
		.maxlinewidth = 2560,
		.danger_lut = 0xFFFF,
		.safe_lut = 0xFF00,
		.maxdwnscale = 4, .maxupscale = 20,
		.maxhdeciexp = DECIMATION_17X_MAX_H,
		.maxvdeciexp = DECIMATION_17X_MAX_V,
		.src_blk = {.id = SDE_SSPP_SRC,
			.base = 0x00, .len = 0x150,},
		.scaler_blk = {.id = SDE_SSPP_SCALER_QSEED2,
			.base = 0x200, .len = 0x70,},
		.csc_blk = {.id = SDE_SSPP_CSC,
			.base = 0x320, .len = 0x44,},
		.pa_blk = {.id = SDE_SSPP_PA_V1,
			.base = 0x200, .len = 0x0,},
		.hist_lut = {.id = SDE_SSPP_HIST_V1,
			.base = 0xA00, .len = 0x400,},
		.pcc_blk = {.id = SDE_SSPP_PCC,
			.base = 0x1780, .len = 0x64,},
		.format_list = plane_formats,
	};

	static const struct sde_sspp_sub_blks dma = {
		.maxlinewidth = 2560,
		.danger_lut = 0xFFFF,
		.safe_lut = 0xFF00,
		.maxdwnscale = 1, .maxupscale = 1,
		.maxhdeciexp = DECIMATION_17X_MAX_H,
		.maxvdeciexp = DECIMATION_17X_MAX_V,
		.src_blk = {.id = SDE_SSPP_SRC, .base = 0x00, .len = 0x150,},
		.scaler_blk = {.id = 0, .base = 0x00, .len = 0x0,},
		.csc_blk = {.id = 0, .base = 0x00, .len = 0x0,},
		.pa_blk = {.id = 0, .base = 0x200, .len = 0x0,},
		.hist_lut = {.id = 0, .base = 0xA00, .len = 0x0,},
		.pcc_blk = {.id = SDE_SSPP_PCC, .base = 0x01780, .len = 0x64,},
		.format_list = plane_formats,
	};

	static const struct sde_sspp_sub_blks cursor = {
		.maxlinewidth = 128,
		.danger_lut = 0xFFFF,
		.safe_lut = 0xFF00,
		.maxdwnscale = 1, .maxupscale = 1,
		.maxhdeciexp = DECIMATION_17X_MAX_H,
		.maxvdeciexp = DECIMATION_17X_MAX_V,
		.src_blk = {.id = SDE_SSPP_SRC, .base = 0x00, .len = 0x150,},
		.scaler_blk = {.id = 0, .base = 0x00, .len = 0x0,},
		.csc_blk = {.id = 0, .base = 0x00, .len = 0x0,},
		.pa_blk = {.id = 0, .base = 0x00, .len = 0x0,},
		.hist_lut = {.id = 0, .base = 0x00, .len = 0x0,},
		.pcc_blk = {.id = 0, .base = 0x00, .len = 0x0,},
		.format_list = plane_formats,
	};

	/* MIXER capability */
	static const struct sde_lm_sub_blks lm = {
		.maxwidth = 2560,
		.maxblendstages = 7, /* excluding base layer */
		.blendstage_base = { /* offsets relative to mixer base */
			0x20, 0x50, 0x80, 0xB0, 0x230, 0x260, 0x290 }
	};

	/* DSPP capability */
	static const struct sde_dspp_sub_blks dspp = {
			.igc = {.id = SDE_DSPP_GC, .base = 0x17c0, .len = 0x0,
				.version = 0x1},
		.pcc = {.id = SDE_DSPP_PCC, .base = 0x00, .len = 0x0,
			.version = 0x1},
		.gamut = {.id = SDE_DSPP_GAMUT, .base = 0x01600, .len = 0x0,
			.version = 0x1},
		.dither = {.id = SDE_DSPP_DITHER, .base = 0x00, .len = 0x0,
			.version = 0x1},
		.pa = {.id = SDE_DSPP_PA, .base = 0x00, .len = 0x0,
			.version = 0x1},
		.hist = {.id = SDE_DSPP_HIST, .base = 0x00, .len = 0x0,
			.version = 0x1},
	};

	/* PINGPONG capability */
	static const struct sde_pingpong_sub_blks pingpong = {
		.te = {.id = SDE_PINGPONG_TE, .base = 0x0000, .len = 0x0,
			.version = 0x1},
		.te2 = {.id = SDE_PINGPONG_TE2, .base = 0x2000, .len = 0x0,
			.version = 0x1},
		.dsc = {.id = SDE_PINGPONG_DSC, .base = 0x10000, .len = 0x0,
			.version = 0x1},
	};

	/* Writeback 0/1 capability */
	static const struct sde_wb_sub_blocks wb0 = {
		.maxlinewidth = 2048,
	};

	/* Writeback 2 capability */
	static const struct sde_wb_sub_blocks wb2 = {
		.maxlinewidth = 4096,
	};

	/* Setup Register maps and defaults */
	*cfg = (struct sde_mdss_cfg){
		.mdss_count = 1,
		.mdss = {
			{.id = MDP_TOP, .base = 0x00000000, .features = 0}
		},
		.mdp_count = 1,
		.mdp = {
			{.id = MDP_TOP, .base = 0x00001000, .features = 0,
				.highest_bank_bit = 0x2},
		},
		.ctl_count = 5,
		.ctl = {
			{.id = CTL_0,
				.base = 0x00002000,
				.features = BIT(SDE_CTL_SPLIT_DISPLAY) |
					BIT(SDE_CTL_PINGPONG_SPLIT) },
			{.id = CTL_1,
				.base = 0x00002200,
				.features = BIT(SDE_CTL_SPLIT_DISPLAY) },
			{.id = CTL_2,
				.base = 0x00002400},
			{.id = CTL_3,
				.base = 0x00002600},
			{.id = CTL_4,
				.base = 0x00002800},
		},
			/* 4 VIG, + 4 RGB + 2 DMA + 2 CURSOR */
		.sspp_count = 12,
		.sspp = {
			{.id = SSPP_VIG0, .base = 0x00005000,
			.features = VIG_17X_MASK, "vig0", .sblk = &vig_layer},
			{.id = SSPP_VIG1, .base = 0x00007000,
			.features = VIG_17X_MASK, "vig1", .sblk = &vig_layer},
			{.id = SSPP_VIG2, .base = 0x00009000,
			.features = VIG_17X_MASK, "vig2", .sblk = &vig_layer},
			{.id = SSPP_VIG3, .base = 0x0000b000,
			.features = VIG_17X_MASK, "vig3", .sblk = &vig_layer},

			{.id = SSPP_RGB0, .base = 0x00015000,
			.features = RGB_17X_MASK, "rgb0", .sblk = &layer},
			{.id = SSPP_RGB1, .base = 0x00017000,
			.features = RGB_17X_MASK, "rgb1", .sblk = &layer},
			{.id = SSPP_RGB2, .base = 0x00019000,
			.features = RGB_17X_MASK, "rgb2", .sblk = &layer},
			{.id = SSPP_RGB3, .base = 0x0001B000,
			.features = RGB_17X_MASK, "rgb3", .sblk = &layer},

			{.id = SSPP_DMA0, .base = 0x00025000,
			.features = DMA_17X_MASK, "dma0", .sblk = &dma},
			{.id = SSPP_DMA1, .base = 0x00027000,
			.features = DMA_17X_MASK, "dma1", .sblk = &dma},

			{.id = SSPP_CURSOR0, .base = 0x00035000,
			.features = CURSOR_17X_MASK, "cursor0",
			.sblk = &cursor},
			{.id = SSPP_CURSOR1, .base = 0x00037000,
			.features = CURSOR_17X_MASK, "cursor1",
			.sblk = &cursor},
		},
		.mixer_count = 6,
		.mixer = {
			{.id = LM_0, .base = 0x00045000,
				.features = MIXER_17X_MASK,
				.sblk = &lm,
				.dspp = DSPP_0,
				.pingpong = PINGPONG_0,
				.lm_pair_mask = (1 << LM_1) },
			{.id = LM_1, .base = 0x00046000,
				.features = MIXER_17X_MASK,
				.sblk = &lm,
				.dspp = DSPP_1,
				.pingpong = PINGPONG_1,
				.lm_pair_mask = (1 << LM_0) },
			{.id = LM_2, .base = 0x00047000,
				.features = MIXER_17X_MASK,
				.sblk = &lm,
				.dspp = DSPP_MAX,
				.pingpong = PINGPONG_2,
				.lm_pair_mask = (1 << LM_5) },
			{.id = LM_3, .base = 0x00048000,
				.features = MIXER_17X_MASK,
				.sblk = &lm,
				.dspp = DSPP_MAX,
				.pingpong = PINGPONG_MAX},
			{.id = LM_4, .base = 0x00049000,
				.features = MIXER_17X_MASK,
				.sblk = &lm,
				.dspp = DSPP_MAX,
				.pingpong = PINGPONG_MAX},
			{.id = LM_5, .base = 0x0004a000,
				.features = MIXER_17X_MASK,
				.sblk = &lm,
				.dspp = DSPP_MAX,
				.pingpong = PINGPONG_3,
				.lm_pair_mask = (1 << LM_2) },
		},
		.dspp_count = 2,
		.dspp = {
			{.id = DSPP_0, .base = 0x00055000,
			.features = DSPP_17X_MASK,
				.sblk = &dspp},
			{.id = DSPP_1, .base = 0x00057000,
			.features = DSPP_17X_MASK,
				.sblk = &dspp},
		},
		.pingpong_count = 4,
		.pingpong = {
			{.id = PINGPONG_0, .base = 0x00071000,
				.features = PINGPONG_17X_SPLIT_MASK,
				.sblk = &pingpong},
			{.id = PINGPONG_1, .base = 0x00071800,
				.features = PINGPONG_17X_SPLIT_MASK,
				.sblk = &pingpong},
			{.id = PINGPONG_2, .base = 0x00072000,
				.features = PINGPONG_17X_MASK,
				.sblk = &pingpong},
			{.id = PINGPONG_3, .base = 0x00072800,
				.features = PINGPONG_17X_MASK,
				.sblk = &pingpong},
		},
		.cdm_count = 1,
		.cdm = {
			{.id = CDM_0, .base = 0x0007A200, .features = 0,
				.intf_connect = BIT(INTF_3),
				.wb_connect = BIT(WB_2),}
		},
		.intf_count = 4,
		.intf = {
			{.id = INTF_0, .base = 0x0006B000,
				.type = INTF_NONE, .controller_id = 0,
				.prog_fetch_lines_worst_case = 21},
			{.id = INTF_1, .base = 0x0006B800,
				.type = INTF_DSI, .controller_id = 0,
				.prog_fetch_lines_worst_case = 21},
			{.id = INTF_2, .base = 0x0006C000,
				.type = INTF_DSI, .controller_id = 1,
				.prog_fetch_lines_worst_case = 21},
			{.id = INTF_3, .base = 0x0006C800,
				.type = INTF_HDMI, .controller_id = 0,
				.prog_fetch_lines_worst_case = 21},
		},
		.wb_count = 3,
		.wb = {
			{.id = WB_0, .base = 0x00065000,
				.features = WB01_17X_MASK,
				.sblk = &wb0,
				.format_list = wb0_formats},
			{.id = WB_1, .base = 0x00065800,
				.features = WB01_17X_MASK,
				.sblk = &wb0,
				.format_list = wb0_formats},
			{.id = WB_2, .base = 0x00066000,
				.features = WB2_17X_MASK,
				.sblk = &wb2,
				.format_list = wb2_formats},
		},
		.ad_count = 2,
		.ad = {
			{.id = AD_0, .base = 0x00079000},
			{.id = AD_1, .base = 0x00079800},
		},
	};
	return 0;
}

/**
 * sde_mdp_cfg_170_init(): Populate the sde sub-blocks catalog information
 */
struct sde_mdss_cfg *sde_mdss_cfg_170_init(u32 step)
{
	struct sde_mdss_cfg *m = NULL;

	/*
	 * This function, for each sub-block sets,
	 * instance count, IO regions,
	 * default capabilities and this version capabilities,
	 * Additional catalog items
	 */

	m = kzalloc(sizeof(*m), GFP_KERNEL);
	if (!m)
		return NULL;

	set_cfg_1xx_init(m);
	m->hwversion = SDE_HW_VER(1, 7, step);

	return m;
}
