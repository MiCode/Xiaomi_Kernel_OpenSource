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

#ifndef _SDE_HW_MDSS_H
#define _SDE_HW_MDSS_H

#include <linux/kernel.h>
#include <linux/err.h>

#define SDE_CSC_MATRIX_COEFF_SIZE	9
#define SDE_CSC_CLAMP_SIZE		6
#define SDE_CSC_BIAS_SIZE		3

#define SDE_MAX_PLANES			4
#define PIPES_PER_STAGE			2
#define VALID_ROT_WB_FORMAT		BIT(0)

enum sde_mdp {
	MDP_TOP = 0x1,
	MDP_MAX,
};

enum sde_sspp {
	SSPP_NONE,
	SSPP_VIG0,
	SSPP_VIG1,
	SSPP_VIG2,
	SSPP_VIG3,
	SSPP_RGB0,
	SSPP_RGB1,
	SSPP_RGB2,
	SSPP_RGB3,
	SSPP_DMA0,
	SSPP_DMA1,
	SSPP_DMA2,
	SSPP_DMA3,
	SSPP_CURSOR0,
	SSPP_CURSOR1,
	SSPP_MAX
};

enum sde_sspp_type {
	SSPP_TYPE_VIG,
	SSPP_TYPE_RGB,
	SSPP_TYPE_DMA,
	SSPP_TYPE_CURSOR,
	SSPP_TYPE_MAX
};

enum sde_lm {
	LM_0 = 0,
	LM_1,
	LM_2,
	LM_3,
	LM_4,
	LM_5,
	LM_6,
	LM_MAX
};

enum sde_stage {
	SDE_STAGE_BASE = 0,
	SDE_STAGE_0,
	SDE_STAGE_1,
	SDE_STAGE_2,
	SDE_STAGE_3,
	SDE_STAGE_4,
	SDE_STAGE_5,
	SDE_STAGE_6,
	SDE_STAGE_MAX
};
enum sde_dspp {
	DSPP_0 = 0,
	DSPP_1,
	DSPP_2,
	DSPP_3,
	DSPP_MAX
};

enum sde_ctl {
	CTL_0 = 0,
	CTL_1,
	CTL_2,
	CTL_3,
	CTL_4,
	CTL_MAX
};

enum sde_cdm {
	CDM_0 = 0,
	CDM_1,
	CDM_MAX
};

enum sde_pingpong {
	PINGPONG_0 = 0,
	PINGPONG_1,
	PINGPONG_2,
	PINGPONG_3,
	PINGPONG_4,
	PINGPONG_MAX
};

enum sde_intf {
	INTF_0 = 0,
	INTF_1,
	INTF_2,
	INTF_3,
	INTF_4,
	INTF_5,
	INTF_6,
	INTF_MAX
};

enum sde_intf_type {
	INTF_NONE = 0x0,
	INTF_DSI = 0x1,
	INTF_HDMI = 0x3,
	INTF_LCDC = 0x5,
	INTF_EDP = 0x9,
	INTF_TYPE_MAX
};

enum sde_intf_mode {
	INTF_MODE_NONE = 0,
	INTF_MODE_CMD,
	INTF_MODE_VIDEO,
	INTF_MODE_WB_BLOCK,
	INTF_MODE_WB_LINE,
	INTF_MODE_MAX
};

enum sde_wb {
	WB_0 = 1,
	WB_1,
	WB_2,
	WB_3,
	WB_MAX
};

enum sde_ad {
	AD_0 = 0x1,
	AD_1,
	AD_MAX
};

/**
 * MDP HW,Component order color map
 */
enum {
	C0_G_Y = 0,
	C1_B_Cb = 1,
	C2_R_Cr = 2,
	C3_ALPHA = 3
};

/**
 * enum sde_mdp_plane_type - defines how the color component pixel packing
 * @SDE_MDP_PLANE_INTERLEAVED   : Color components in single plane
 * @SDE_MDP_PLANE_PLANAR        : Color component in separate planes
 * @SDE_MDP_PLANE_PSEUDO_PLANAR : Chroma components interleaved in separate
 *                                plane
 */
enum sde_mdp_plane_type {
	SDE_MDP_PLANE_INTERLEAVED,
	SDE_MDP_PLANE_PLANAR,
	SDE_MDP_PLANE_PSEUDO_PLANAR,
};

/**
 * enum sde_mdp_chroma_samp_type - chroma sub-samplng type
 * @SDE_MDP_CHROMA_RGB   : no chroma subsampling
 * @SDE_MDP_CHROMA_H2V1  : chroma pixels are horizontally subsampled
 * @SDE_MDP_CHROMA_H1V2  : chroma pixels are vertically subsampled
 * @SDE_MDP_CHROMA_420   : 420 subsampling
 */
enum sde_mdp_chroma_samp_type {
	SDE_MDP_CHROMA_RGB,
	SDE_MDP_CHROMA_H2V1,
	SDE_MDP_CHROMA_H1V2,
	SDE_MDP_CHROMA_420
};

/**
 * enum sde_mdp_fetch_type - format id, used by drm-driver only to map drm forcc
 * Defines How MDP HW fetches data
 * @SDE_MDP_FETCH_LINEAR   : fetch is line by line
 * @SDE_MDP_FETCH_TILE     : fetches data in Z order from a tile
 * @SDE_MDP_FETCH_UBWC     : fetch and decompress data
 */
enum sde_mdp_fetch_type {
	SDE_MDP_FETCH_LINEAR,
	SDE_MDP_FETCH_TILE,
	SDE_MDP_FETCH_UBWC
};

/**
 * Value of enum chosen to fit the number of bits
 * expected by the HW programming.
 */
enum {
	COLOR_4BIT,
	COLOR_5BIT,
	COLOR_6BIT,
	COLOR_8BIT,
	COLOR_ALPHA_1BIT = 0,
	COLOR_ALPHA_4BIT = 1,
};

enum sde_alpha_blend_type {
	ALPHA_FG_CONST = 0,
	ALPHA_BG_CONST,
	ALPHA_FG_PIXEL,
	ALPHA_BG_PIXEL,
	ALPHA_MAX
};

struct addr_info {
	u32 plane[SDE_MAX_PLANES];
};

/**
 * struct sde_mdp_format_params - defines the format configuration which
 * allows MDP HW to correctly fetch and decode the format
 * @format : format id, used by drm-driver only to map drm forcc
 * @flag
 * @chroma_sample
 * @fetch_planes
 * @unpack_align_msb
 * @unpack_tight
 * @unpack_count
 * @bpp
 * @alpha_enable
 * @fetch_mode
 * @bits
 * @element
 */
struct sde_mdp_format_params {
	u32 format;
	enum sde_mdp_plane_type fetch_planes;
	u8 element[SDE_MAX_PLANES];
	u8 bits[SDE_MAX_PLANES];
	enum sde_mdp_chroma_samp_type chroma_sample;
	u8 unpack_align_msb;	/* 0 to LSB, 1 to MSB */
	u8 unpack_tight;	/* 0 for loose, 1 for tight */
	u8 unpack_count;	/* 0 = 1 component, 1 = 2 component ... */
	u8 bpp;                 /* Bytes per pixel */
	u8 alpha_enable;	/*  source has alpha */
	enum sde_mdp_fetch_type fetch_mode;
	u8 is_yuv;
	u32 flag;
};

/**
 * struct sde_hw_source_info - format information of the source pixel data
 * @format : pixel format parameters
 * @width : image width @height: image height
 * @num_planes : number of planes including the meta data planes for the
 * compressed formats @plane: per plane information
 */
struct sde_hw_source_info {
	struct sde_mdp_format_params *format;
	u32 width;
	u32 height;
	u32 num_planes;
	u32 ystride[SDE_MAX_PLANES];
};

struct sde_rect {
	u16 x;
	u16 y;
	u16 w;
	u16 h;
};

struct sde_hw_alpha_cfg {
	u32 const_alpha;
	enum sde_alpha_blend_type alpha_sel;
	u8 inv_alpha_sel;
	u8 mod_alpha;
	u8 inv_mode_alpha;
};

struct sde_hw_blend_cfg {
	struct sde_hw_alpha_cfg fg;
	struct sde_hw_alpha_cfg bg;
};

struct sde_csc_cfg {
	uint32_t csc_mv[SDE_CSC_MATRIX_COEFF_SIZE];
	uint32_t csc_pre_bv[SDE_CSC_BIAS_SIZE];
	uint32_t csc_post_bv[SDE_CSC_BIAS_SIZE];
	uint32_t csc_pre_lv[SDE_CSC_CLAMP_SIZE];
	uint32_t csc_post_lv[SDE_CSC_CLAMP_SIZE];
};

/**
 * struct sde_mdss_color - mdss color description
 * color 0 : green
 * color 1 : blue
 * color 2 : red
 * color 3 : alpha
 */
struct sde_mdss_color {
	u32 color_0;
	u32 color_1;
	u32 color_2;
	u32 color_3;
};

#endif  /* _SDE_HW_MDSS_H */
