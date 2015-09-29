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

#ifndef _SDE_HW_CATALOG_H
#define _SDE_HW_CATALOG_H

#include <linux/kernel.h>
#include <linux/bug.h>
#include <linux/bitmap.h>
#include <linux/err.h>

#define MAX_BLOCKS    8
#define MAX_LAYERS    12

#define SDE_HW_VER(MAJOR, MINOR, STEP) (((MAJOR & 0xF) << 28)    |\
		((MINOR & 0xFFF) << 16)  |\
		(STEP & 0xFFFF))

#define SDE_HW_MAJOR(rev)		((rev) >> 28)
#define SDE_HW_MINOR(rev)		.(((rev) >> 16) & 0xFFF)
#define SDE_HW_STEP(rev)		((rev) & 0xFFFF)
#define SDE_HW_MAJOR_MINOR(rev)		((rev) >> 16)

#define IS_SDE_MAJOR_MINOR_SAME(rev1, rev2)   \
	(SDE_HW_MAJOR_MINOR((rev1)) == SDE_HW_MAJOR_MINOR((rev2)))

#define SDE_HW_VER_170	SDE_HW_VER(1, 7, 0) /* 8996 v1.0 */
#define SDE_HW_VER_171	SDE_HW_VER(1, 7, 1) /* 8996 v2.0 */
#define SDE_HW_VER_172	SDE_HW_VER(1, 7, 2) /* 8996 v3.0 */
#define SDE_HW_VER_300	SDE_HW_VER(3, 0, 0) /* cobalt v1.0 */

/**
 * MDP TOP BLOCK features
 * @SDE_MDP_PANIC_PER_PIPE Panic configuration needs to be be done per pipe
 * @SDE_MDP_10BIT_SUPPORT, Chipset supports 10 bit pixel formats
 * @SDE_MDP_BWC,           MDSS HW supports Bandwidth compression.
 * @SDE_MDP_UBWC_1_0,      This chipsets supports Universal Bandwidth
 *                         compression initial revision
 * @SDE_MDP_UBWC_1_5,      Universal Bandwidth compression version 1.5
 * @SDE_MDP_CDP,           Client driven prefetch
 * @SDE_MDP_MAX            Maximum value

 */
enum {
	SDE_MDP_PANIC_PER_PIPE = 0x1,
	SDE_MDP_10BIT_SUPPORT,
	SDE_MDP_BWC,
	SDE_MDP_UBWC_1_0,
	SDE_MDP_UBWC_1_5,
	SDE_MDP_CDP,
	SDE_MDP_MAX
};

/**
 * SSPP sub-blocks/features
 * @SDE_SSPP_SRC             Src and fetch part of the pipes,
 * @SDE_SSPP_SCALAR_QSEED2,  QSEED2 algorithm support
 * @SDE_SSPP_SCALAR_QSEED3,  QSEED3 algorithm support
 * @SDE_SSPP_SCALAR_RGB,     RGB Scalar, supported by RGB pipes
 * @SDE_SSPP_CSC,            Support of Color space conversion
 * @SDE_SSPP_PA_V1,          Common op-mode register for PA blocks
 * @SDE_SSPP_HIST_V1         Histogram programming method V1
 * @SDE_SSPP_IGC,            Inverse gamma correction
 * @SDE_SSPP_PCC,            Color correction support
 * @SDE_SSPP_CURSOR,         SSPP can be used as a cursor layer
 * @SDE_SSPP_MAX             maximum value
 */
enum {
	SDE_SSPP_SRC = 0x1,
	SDE_SSPP_SCALAR_QSEED2,
	SDE_SSPP_SCALAR_QSEED3,
	SDE_SSPP_SCALAR_RGB,
	SDE_SSPP_CSC,
	SDE_SSPP_PA_V1, /* Common op-mode register for PA blocks */
	SDE_SSPP_HIST_V1,
	SDE_SSPP_IGC,
	SDE_SSPP_PCC,
	SDE_SSPP_CURSOR,
	SDE_SSPP_MAX
};

/*
 * MIXER sub-blocks/features
 * @SDE_MIXER_LAYER           Layer mixer layer blend configuration,
 * @SDE_MIXER_SOURCESPLIT     Layer mixer supports source-split configuration
 * @SDE_MIXER_GC              Gamma correction block
 * @SDE_MIXER_MAX             maximum value
 */
enum {
	SDE_MIXER_LAYER = 0x1,
	SDE_MIXER_SOURCESPLIT,
	SDE_MIXER_GC,
	SDE_MIXER_MAX
};

/**
 * DSPP sub-blocks
 * @SDE_DSPP_IGC             DSPP Inverse gamma correction block
 * @SDE_DSPP_PCC             Panel color correction block
 * @SDE_DSPP_GC              Gamma correction block
 * @SDE_DSPP_PA              Picture adjustment block
 * @SDE_DSPP_GAMUT           Gamut bloc
 * @SDE_DSPP_DITHER          Dither block
 * @SDE_DSPP_HIST            Histogram bloc
 * @SDE_DSPP_MAX             maximum value
 */
enum {
	SDE_DSPP_IGC = 0x1,
	SDE_DSPP_PCC,
	SDE_DSPP_GC,
	SDE_DSPP_PA,
	SDE_DSPP_GAMUT,
	SDE_DSPP_DITHER,
	SDE_DSPP_HIST,
	SDE_DSPP_MAX
};

/**
 * PINGPONG sub-blocks
 * @SDE_PINGPONG_TE         Tear check block
 * @SDE_PINGPONG_TE2        Additional tear check block for split pipes
 * @SDE_PINGPONG_SPLIT      PP block supports split fifo
 * @SDE_PINGPONG_DSC,       Display stream compression blocks
 * @SDE_PINGPONG_MAX
 */
enum {
	SDE_PINGPONG_TE = 0x1,
	SDE_PINGPONG_TE2,
	SDE_PINGPONG_SPLIT,
	SDE_PINGPONG_DSC,
	SDE_PINGPONG_MAX
};

/**
 * WB sub-blocks and features
 * @SDE_WB_LINE_MODE        Writeback module supports line/linear mode
 * @SDE_WB_BLOCK_MODE       Writeback module supports block mode read
 * @SDE_WB_ROTATE           rotation support,this is available if writeback
 *                          supports block mode read
 * @SDE_WB_CSC              Writeback color conversion block support
 * @SDE_WB_CHROMA_DOWN,     Writeback chroma down block,
 * @SDE_WB_DOWNSCALE,       Writeback integer downscaler,
 * @SDE_WB_DITHER,          Dither block
 * @SDE_WB_TRAFFIC_SHAPER,  Writeback traffic shaper bloc
 * @SDE_WB_UBWC_1_0,        Writeback Universal bandwidth compression 1.0
 *                          support
 * @SDE_WB_WBWC_1_5         UBWC 1.5 support
 * @SDE_WB_MAX              maximum value
 */
enum {
	SDE_WB_LINE_MODE = 0x1,
	SDE_WB_BLOCK_MODE,
	SDE_WB_ROTATE = SDE_WB_BLOCK_MODE,
	SDE_WB_CSC,
	SDE_WB_CHROMA_DOWN,
	SDE_WB_DOWNSCALE,
	SDE_WB_DITHER,
	SDE_WB_TRAFFIC_SHAPER,
	SDE_WB_UBWC_1_0,
	SDE_WB_MAX
};

/**
 * MACRO SDE_HW_BLK_INFO - information of HW blocks inside SDE
 * @id:                enum identifying this block
 * @base:              register base offset to mdss
 * @features           bit mask identifying sub-blocks/features
 */
#define SDE_HW_BLK_INFO \
	u32 id; \
	u32 base; \
	unsigned long features

/**
 * MACRO SDE_HW_SUBBLK_INFO - information of HW sub-block inside SDE
 * @id:                enum identifying this sub-block
 * @base:              offset of this sub-block relative to the block
 *                     offset
 * @len                register block length of this sub-block
 */
#define SDE_HW_SUBBLK_INFO \
	u32 id; \
	u32 base; \
	u32 len

/**
 * struct sde_src_blk: SSPP part of the source pipes
 * @info:   HW register and features supported by this sub-blk
 */
struct sde_src_blk {
	SDE_HW_SUBBLK_INFO;
};

/**
 * struct sde_scalar_info: Scalar information
 * @info:   HW register and features supported by this sub-blk
 */
struct sde_scalar_blk {
	SDE_HW_SUBBLK_INFO;
};

struct sde_csc_blk {
	SDE_HW_SUBBLK_INFO;
};

/**
 * struct sde_pp_blk : Pixel processing sub-blk information
 * @info:   HW register and features supported by this sub-blk
 * @version: HW Algorithm version
 */
struct sde_pp_blk {
	SDE_HW_SUBBLK_INFO;
	u32 version;
};

/**
 * struct sde_sspp_sub_blks : SSPP sub-blocks
 * @maxdwnscale: max downscale ratio supported(without DECIMATION)
 * @maxupscale:  maxupscale ratio supported
 * @maxwidth:    max pixelwidth supported by this pipe
 * @danger_lut:  LUT to generate danger signals
 * @safe_lut:    LUT to generate safe signals
 * @src_blk:
 * @scalar_blk:
 * @csc_blk:
 * @pa_blk:
 * @hist_lut:
 * @pcc_blk:
 */
struct sde_sspp_sub_blks {
	u32 maxlinewidth;
	u32 danger_lut;
	u32 safe_lut;
	u32 maxdwnscale;
	u32 maxupscale;
	struct sde_src_blk src_blk;
	struct sde_scalar_blk scalar_blk;
	struct sde_pp_blk csc_blk;
	struct sde_pp_blk pa_blk;
	struct sde_pp_blk hist_lut;
	struct sde_pp_blk pcc_blk;
};

/**
 * struct sde_lm_sub_blks:      information of mixer block
 * @maxwidth:               Max pixel width supported by this mixer
 * @maxblendstages:         Max number of blend-stages supported
 * @blendstage_base:        Blend-stage register base offset
 */
struct sde_lm_sub_blks {
	u32 maxwidth;
	u32 maxblendstages;
	u32 blendstage_base[MAX_BLOCKS];
};

struct sde_dspp_sub_blks {
	struct sde_pp_blk igc;
	struct sde_pp_blk pcc;
	struct sde_pp_blk gc;
	struct sde_pp_blk pa;
	struct sde_pp_blk gamut;
	struct sde_pp_blk dither;
	struct sde_pp_blk hist;
};

struct sde_pingpong_sub_blks {
	struct sde_pp_blk te;
	struct sde_pp_blk te2;
	struct sde_pp_blk dsc;
};

struct sde_wb_sub_blocks {
	u32 maxlinewidth;
};

/* struct sde_mdp_cfg : MDP TOP-BLK instance info
 * @id:                index identifying this block
 * @base:              register base offset to mdss
 * @features           bit mask identifying sub-blocks/features
 * @highest_bank_bit:  UBWC parameter
 */
struct sde_mdp_cfg {
	SDE_HW_BLK_INFO;
	u32 highest_bank_bit;
};

/* struct sde_mdp_cfg : MDP TOP-BLK instance info
 * @id:                index identifying this block
 * @base:              register base offset to mdss
 * @features           bit mask identifying sub-blocks/features
 */
struct sde_ctl_cfg {
	SDE_HW_BLK_INFO;
};

/**
 * struct sde_sspp_cfg - information of source pipes
 * @id:                index identifying this block
 * @base               register offset of this block
 * @features           bit mask identifying sub-blocks/features
 * @sblk:              Sub-blocks of SSPP
 */
struct sde_sspp_cfg {
	SDE_HW_BLK_INFO;
	const struct sde_sspp_sub_blks *sblk;
};

/**
 * struct sde_lm_cfg - information of layer mixer blocks
 * @id:                index identifying this block
 * @base               register offset of this block
 * @features           bit mask identifying sub-blocks/features
 * @sblk:              Sub-blocks of SSPP
 */
struct sde_lm_cfg {
	SDE_HW_BLK_INFO;
	const struct sde_lm_sub_blks *sblk;
};

/**
 * struct sde_dspp_cfg - information of DSPP blocks
 * @id                 enum identifying this block
 * @base               register offset of this block
 * @features           bit mask identifying sub-blocks/features
 *                     supported by this block
 * @sblk               sub-blocks information
 */
struct sde_dspp_cfg  {
	SDE_HW_BLK_INFO;
	const struct sde_dspp_sub_blks *sblk;
};

/**
 * struct sde_pingpong_cfg - information of PING-PONG blocks
 * @id                 enum identifying this block
 * @base               register offset of this block
 * @features           bit mask identifying sub-blocks/features
 * @sblk               sub-blocks information
 */
struct sde_pingpong_cfg  {
	SDE_HW_BLK_INFO;
	const struct sde_pingpong_sub_blks *sblk;
};

/**
 * struct sde_cdm_cfg - information of chroma down blocks
 * @id                 enum identifying this block
 * @base               register offset of this block
 * @features           bit mask identifying sub-blocks/features
 * @intf_connect       Connects to which interfaces
 * @wb_connect:        Connects to which writebacks
 */
struct sde_cdm_cfg   {
	SDE_HW_BLK_INFO;
	u32 intf_connect[MAX_BLOCKS];
	u32 wb_connect[MAX_BLOCKS];
};

/**
 * struct sde_intf_cfg - information of timing engine blocks
 * @id                 enum identifying this block
 * @base               register offset of this block
 * @features           bit mask identifying sub-blocks/features
 * @type:              Interface type(DSI, DP, HDMI)
 */
struct sde_intf_cfg  {
	SDE_HW_BLK_INFO;
	u32 type;   /* interface type*/
};

/**
 * struct sde_wb_cfg - information of writeback blocks
 * @id                 enum identifying this block
 * @base               register offset of this block
 * @features           bit mask identifying sub-blocks/features
 */
struct sde_wb_cfg {
	SDE_HW_BLK_INFO;
	struct sde_wb_sub_blocks *sblk;
};

/**
 * struct sde_ad_cfg - information of Assertive Display blocks
 * @id                 enum identifying this block
 * @base               register offset of this block
 * @features           bit mask identifying sub-blocks/features
 */
struct sde_ad_cfg {
	SDE_HW_BLK_INFO;
};

/**
 * struct sde_mdss_cfg - information of MDSS HW
 * This is the main catalog data structure representing
 * this HW version. Contains number of instances,
 * register offsets, capabilities of the all MDSS HW sub-blocks.
 */
struct sde_mdss_cfg {
	u32 hwversion;

	u32 mdp_count;
	struct sde_mdp_cfg mdp[MAX_BLOCKS];

	u32 ctl_count;
	struct sde_ctl_cfg ctl[MAX_BLOCKS];

	u32 sspp_count;
	struct sde_sspp_cfg sspp[MAX_LAYERS];

	u32 mixer_count;
	struct sde_lm_cfg mixer[MAX_BLOCKS];

	u32 dspp_count;
	struct sde_dspp_cfg dspp[MAX_BLOCKS];

	u32 pingpong_count;
	struct sde_pingpong_cfg pingpong[MAX_BLOCKS];

	u32 cdm_count;
	struct sde_cdm_cfg cdm[MAX_BLOCKS];

	u32 intf_count;
	struct sde_intf_cfg intf[MAX_BLOCKS];

	u32 wb_count;
	struct sde_wb_cfg wb[MAX_BLOCKS];

	u32 ad_count;
	struct sde_ad_cfg ad[MAX_BLOCKS];
	/* Add additional block data structures here */
};

struct sde_mdss_hw_cfg_handler {
	u32 major;
	u32 minor;
	struct sde_mdss_cfg* (*cfg_init)(u32);
};

/*
 * Access Macros
 */
#define BLK_MDP(s) ((s)->mdp)
#define BLK_CTL(s) ((s)->ctl)
#define BLK_VIG(s) ((s)->vig)
#define BLK_RGB(s) ((s)->rgb)
#define BLK_DMA(s) ((s)->dma)
#define BLK_CURSOR(s) ((s)->cursor)
#define BLK_MIXER(s) ((s)->mixer)
#define BLK_DSPP(s) ((s)->dspp)
#define BLK_PINGPONG(s) ((s)->pingpong)
#define BLK_CDM(s) ((s)->cdm)
#define BLK_INTF(s) ((s)->intf)
#define BLK_WB(s) ((s)->wb)
#define BLK_AD(s) ((s)->ad)

struct sde_mdss_cfg *sde_mdss_cfg_170_init(u32 step);
struct sde_mdss_cfg *sde_hw_catalog_init(u32 major, u32 minor, u32 step);

#endif /* _SDE_HW_CATALOG_H */
