/* Copyright (c) 2015-2018, The Linux Foundation. All rights reserved.
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
#include <linux/msm-bus.h>
#include <drm/drmP.h>

/**
 * Max hardware block count: For ex: max 12 SSPP pipes or
 * 5 ctl paths. In all cases, it can have max 12 hardware blocks
 * based on current design
 */
#define MAX_BLOCKS    12

#define SDE_HW_VER(MAJOR, MINOR, STEP) (((MAJOR & 0xF) << 28)    |\
		((MINOR & 0xFFF) << 16)  |\
		(STEP & 0xFFFF))

#define SDE_HW_MAJOR(rev)		((rev) >> 28)
#define SDE_HW_MINOR(rev)		(((rev) >> 16) & 0xFFF)
#define SDE_HW_STEP(rev)		((rev) & 0xFFFF)
#define SDE_HW_MAJOR_MINOR(rev)		((rev) >> 16)

#define IS_SDE_MAJOR_MINOR_SAME(rev1, rev2)   \
	(SDE_HW_MAJOR_MINOR((rev1)) == SDE_HW_MAJOR_MINOR((rev2)))

#define SDE_HW_VER_170	SDE_HW_VER(1, 7, 0) /* 8996 v1.0 */
#define SDE_HW_VER_171	SDE_HW_VER(1, 7, 1) /* 8996 v2.0 */
#define SDE_HW_VER_172	SDE_HW_VER(1, 7, 2) /* 8996 v3.0 */
#define SDE_HW_VER_300	SDE_HW_VER(3, 0, 0) /* 8998 v1.0 */
#define SDE_HW_VER_301	SDE_HW_VER(3, 0, 1) /* 8998 v1.1 */
#define SDE_HW_VER_400	SDE_HW_VER(4, 0, 0) /* sdm845 v1.0 */

#define IS_MSMSKUNK_TARGET(rev) IS_SDE_MAJOR_MINOR_SAME((rev), SDE_HW_VER_400)

#define SDE_HW_BLK_NAME_LEN	16

#define MAX_IMG_WIDTH 0x3fff
#define MAX_IMG_HEIGHT 0x3fff

#define CRTC_DUAL_MIXERS	2

#define SDE_COLOR_PROCESS_VER(MAJOR, MINOR) \
		((((MAJOR) & 0xFFFF) << 16) | (((MINOR) & 0xFFFF)))
#define SDE_COLOR_PROCESS_MAJOR(version) (((version) & 0xFFFF0000) >> 16)
#define SDE_COLOR_PROCESS_MINOR(version) ((version) & 0xFFFF)

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
 * @SDE_SSPP_SCALER_QSEED2,  QSEED2 algorithm support
 * @SDE_SSPP_SCALER_QSEED3,  QSEED3 alogorithm support
 * @SDE_SSPP_SCALER_RGB,     RGB Scaler, supported by RGB pipes
 * @SDE_SSPP_CSC,            Support of Color space converion
 * @SDE_SSPP_CSC_10BIT,      Support of 10-bit Color space conversion
 * @SDE_SSPP_HSIC,           Global HSIC control
 * @SDE_SSPP_MEMCOLOR        Memory Color Support
 * @SDE_SSPP_IGC,            Inverse gamma correction
 * @SDE_SSPP_PCC,            Color correction support
 * @SDE_SSPP_CURSOR,         SSPP can be used as a cursor layer
 * @SDE_SSPP_QOS,            SSPP support QoS control, danger/safe/creq
 * @SDE_SSPP_MAX             maximum value
 */
enum {
	SDE_SSPP_SRC = 0x1,
	SDE_SSPP_SCALER_QSEED2,
	SDE_SSPP_SCALER_QSEED3,
	SDE_SSPP_SCALER_RGB,
	SDE_SSPP_CSC,
	SDE_SSPP_CSC_10BIT,
	SDE_SSPP_HSIC,
	SDE_SSPP_MEMCOLOR,
	SDE_SSPP_IGC,
	SDE_SSPP_PCC,
	SDE_SSPP_CURSOR,
	SDE_SSPP_QOS,
	SDE_SSPP_MAX
};

/*
 * MIXER sub-blocks/features
 * @SDE_MIXER_LAYER           Layer mixer layer blend configuration,
 * @SDE_MIXER_SOURCESPLIT     Layer mixer supports source-split configuration
 * @SDE_MIXER_GC              Gamma correction block
 * @SDE_DISP_PRIMARY_PREF     Primary display prefers this mixer
 * @SDE_DISP_SECONDARY_PREF   Secondary display prefers this mixer
 * @SDE_DISP_TERTIARY_PREF    Tertiary display prefers this mixer
 * @SDE_MIXER_MAX             maximum value
 */
enum {
	SDE_MIXER_LAYER = 0x1,
	SDE_MIXER_SOURCESPLIT,
	SDE_MIXER_GC,
	SDE_DISP_PRIMARY_PREF,
	SDE_DISP_SECONDARY_PREF,
	SDE_DISP_TERTIARY_PREF,
	SDE_MIXER_MAX
};

/**
 * DSPP sub-blocks
 * @SDE_DSPP_IGC             DSPP Inverse gamma correction block
 * @SDE_DSPP_PCC             Panel color correction block
 * @SDE_DSPP_GC              Gamma correction block
 * @SDE_DSPP_HSIC            Global HSIC block
 * @SDE_DSPP_MEMCOLOR        Memory Color block
 * @SDE_DSPP_SIXZONE         Six zone block
 * @SDE_DSPP_GAMUT           Gamut bloc
 * @SDE_DSPP_DITHER          Dither block
 * @SDE_DSPP_HIST            Histogram block
 * @SDE_DSPP_VLUT            PA VLUT block
 * @SDE_DSPP_AD              AD block
 * @SDE_DSPP_MAX             maximum value
 */
enum {
	SDE_DSPP_IGC = 0x1,
	SDE_DSPP_PCC,
	SDE_DSPP_GC,
	SDE_DSPP_HSIC,
	SDE_DSPP_MEMCOLOR,
	SDE_DSPP_SIXZONE,
	SDE_DSPP_GAMUT,
	SDE_DSPP_DITHER,
	SDE_DSPP_HIST,
	SDE_DSPP_VLUT,
	SDE_DSPP_AD,
	SDE_DSPP_MAX
};

/**
 * PINGPONG sub-blocks
 * @SDE_PINGPONG_TE         Tear check block
 * @SDE_PINGPONG_TE2        Additional tear check block for split pipes
 * @SDE_PINGPONG_SPLIT      PP block supports split fifo
 * @SDE_PINGPONG_SLAVE      PP block is a suitable slave for split fifo
 * @SDE_PINGPONG_DSC,       Display stream compression blocks
 * @SDE_PINGPONG_MAX
 */
enum {
	SDE_PINGPONG_TE = 0x1,
	SDE_PINGPONG_TE2,
	SDE_PINGPONG_SPLIT,
	SDE_PINGPONG_SLAVE,
	SDE_PINGPONG_DSC,
	SDE_PINGPONG_MAX
};

/**
 * CTL sub-blocks
 * @SDE_CTL_SPLIT_DISPLAY       CTL supports video mode split display
 * @SDE_CTL_PINGPONG_SPLIT      CTL supports pingpong split
 * @SDE_CTL_PRIMARY_PREF        Primary display perfers this CTL
 * @SDE_CTL_SECONDARY_PREF      Secondary display perfers this CTL
 * @SDE_CTL_TERTIARY_PREF       Tertiary display perfers this CTL
 * @SDE_CTL_MAX
 */
enum {
	SDE_CTL_SPLIT_DISPLAY = 0x1,
	SDE_CTL_PINGPONG_SPLIT,
	SDE_CTL_PRIMARY_PREF,
	SDE_CTL_SECONDARY_PREF,
	SDE_CTL_TERTIARY_PREF,
	SDE_CTL_MAX
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
 * @SDE_WB_UBWC_1_5         UBWC 1.5 support
 * @SDE_WB_YUV_CONFIG       Writeback supports output of YUV colorspace
 * @SDE_WB_PIPE_ALPHA       Writeback supports pipe alpha
 * @SDE_WB_XY_ROI_OFFSET    Writeback supports x/y-offset of out ROI in
 *                          the destination image
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
	SDE_WB_YUV_CONFIG,
	SDE_WB_PIPE_ALPHA,
	SDE_WB_XY_ROI_OFFSET,
	SDE_WB_MAX
};

/**
 * VBIF sub-blocks and features
 * @SDE_VBIF_QOS_OTLIM        VBIF supports OT Limit
 * @SDE_VBIF_MAX              maximum value
 */
enum {
	SDE_VBIF_QOS_OTLIM = 0x1,
	SDE_VBIF_MAX
};

/**
 * MACRO SDE_HW_BLK_INFO - information of HW blocks inside SDE
 * @name:              string name for debug purposes
 * @id:                enum identifying this block
 * @base:              register base offset to mdss
 * @len:               length of hardware block
 * @features           bit mask identifying sub-blocks/features
 */
#define SDE_HW_BLK_INFO \
	char name[SDE_HW_BLK_NAME_LEN]; \
	u32 id; \
	u32 base; \
	u32 len; \
	unsigned long features; \

/**
 * MACRO SDE_HW_SUBBLK_INFO - information of HW sub-block inside SDE
 * @name:              string name for debug purposes
 * @id:                enum identifying this sub-block
 * @base:              offset of this sub-block relative to the block
 *                     offset
 * @len                register block length of this sub-block
 */
#define SDE_HW_SUBBLK_INFO \
	char name[SDE_HW_BLK_NAME_LEN]; \
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
 * struct sde_scaler_blk: Scaler information
 * @info:   HW register and features supported by this sub-blk
 * @version: qseed block revision
 */
struct sde_scaler_blk {
	SDE_HW_SUBBLK_INFO;
	u32 version;
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
 * struct sde_format_extended - define sde specific pixel format+modifier
 * @fourcc_format: Base FOURCC pixel format code
 * @modifier: 64-bit drm format modifier, same modifier must be applied to all
 *            framebuffer planes
 */
struct sde_format_extended {
	uint32_t fourcc_format;
	uint64_t modifier;
};

/**
 * struct sde_sspp_sub_blks : SSPP sub-blocks
 * @maxdwnscale: max downscale ratio supported(without DECIMATION)
 * @maxupscale:  maxupscale ratio supported
 * @maxwidth:    max pixelwidth supported by this pipe
 * @danger_lut_linear: LUT to generate danger signals for linear format
 * @safe_lut_linear: LUT to generate safe signals for linear format
 * @danger_lut_tile: LUT to generate danger signals for tile format
 * @safe_lut_tile: LUT to generate safe signals for tile format
 * @danger_lut_nrt: LUT to generate danger signals for non-realtime use case
 * @safe_lut_nrt: LUT to generate safe signals for non-realtime use case
 * @creq_lut_nrt: LUT to generate creq signals for non-realtime use case
 * @creq_vblank: creq priority during vertical blanking
 * @danger_vblank: danger priority during vertical blanking
 * @pixel_ram_size: size of latency hiding and de-tiling buffer in bytes
 * @src_blk:
 * @scaler_blk:
 * @csc_blk:
 * @hsic:
 * @memcolor:
 * @pcc_blk:
 * @igc_blk:
 * @format_list: Pointer to list of supported formats
 */
struct sde_sspp_sub_blks {
	u32 maxlinewidth;
	u32 danger_lut_linear;
	u32 safe_lut_linear;
	u32 danger_lut_tile;
	u32 safe_lut_tile;
	u32 danger_lut_nrt;
	u32 safe_lut_nrt;
	u32 creq_lut_nrt;
	u32 creq_vblank;
	u32 danger_vblank;
	u32 pixel_ram_size;
	u32 maxdwnscale;
	u32 maxupscale;
	u32 maxhdeciexp; /* max decimation is 2^value */
	u32 maxvdeciexp; /* max decimation is 2^value */
	struct sde_src_blk src_blk;
	struct sde_scaler_blk scaler_blk;
	struct sde_pp_blk csc_blk;
	struct sde_pp_blk hsic_blk;
	struct sde_pp_blk memcolor_blk;
	struct sde_pp_blk pcc_blk;
	struct sde_pp_blk igc_blk;

	const struct sde_format_extended *format_list;
};

/**
 * struct sde_lm_sub_blks:      information of mixer block
 * @maxwidth:               Max pixel width supported by this mixer
 * @maxblendstages:         Max number of blend-stages supported
 * @blendstage_base:        Blend-stage register base offset
 * @gc: gamma correction block
 */
struct sde_lm_sub_blks {
	u32 maxwidth;
	u32 maxblendstages;
	u32 blendstage_base[MAX_BLOCKS];
	struct sde_pp_blk gc;
};

struct sde_dspp_sub_blks {
	struct sde_pp_blk igc;
	struct sde_pp_blk pcc;
	struct sde_pp_blk gc;
	struct sde_pp_blk hsic;
	struct sde_pp_blk memcolor;
	struct sde_pp_blk sixzone;
	struct sde_pp_blk gamut;
	struct sde_pp_blk dither;
	struct sde_pp_blk hist;
	struct sde_pp_blk ad;
	struct sde_pp_blk vlut;
};

struct sde_pingpong_sub_blks {
	struct sde_pp_blk te;
	struct sde_pp_blk te2;
	struct sde_pp_blk dsc;
};

struct sde_wb_sub_blocks {
	u32 maxlinewidth;
};

struct sde_mdss_base_cfg {
	SDE_HW_BLK_INFO;
};

/**
 * sde_clk_ctrl_type - Defines top level clock control signals
 */
enum sde_clk_ctrl_type {
	SDE_CLK_CTRL_NONE,
	SDE_CLK_CTRL_VIG0,
	SDE_CLK_CTRL_VIG1,
	SDE_CLK_CTRL_VIG2,
	SDE_CLK_CTRL_VIG3,
	SDE_CLK_CTRL_VIG4,
	SDE_CLK_CTRL_RGB0,
	SDE_CLK_CTRL_RGB1,
	SDE_CLK_CTRL_RGB2,
	SDE_CLK_CTRL_RGB3,
	SDE_CLK_CTRL_DMA0,
	SDE_CLK_CTRL_DMA1,
	SDE_CLK_CTRL_CURSOR0,
	SDE_CLK_CTRL_CURSOR1,
	SDE_CLK_CTRL_WB0,
	SDE_CLK_CTRL_WB1,
	SDE_CLK_CTRL_WB2,
	SDE_CLK_CTRL_MAX,
};

/* struct sde_clk_ctrl_reg : Clock control register
 * @reg_off:           register offset
 * @bit_off:           bit offset
 */
struct sde_clk_ctrl_reg {
	u32 reg_off;
	u32 bit_off;
};

/* struct sde_mdp_cfg : MDP TOP-BLK instance info
 * @id:                index identifying this block
 * @base:              register base offset to mdss
 * @features           bit mask identifying sub-blocks/features
 * @highest_bank_bit:  UBWC parameter
 * @clk_ctrls          clock control register definition
 */
struct sde_mdp_cfg {
	SDE_HW_BLK_INFO;
	u32 highest_bank_bit;
	struct sde_clk_ctrl_reg clk_ctrls[SDE_CLK_CTRL_MAX];
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
 * @sblk:              SSPP sub-blocks information
 * @xin_id:            bus client identifier
 * @clk_ctrl           clock control identifier
 * @type               sspp type identifier
 */
struct sde_sspp_cfg {
	SDE_HW_BLK_INFO;
	const struct sde_sspp_sub_blks *sblk;
	u32 xin_id;
	enum sde_clk_ctrl_type clk_ctrl;
	u32 type;
};

/**
 * struct sde_lm_cfg - information of layer mixer blocks
 * @id:                index identifying this block
 * @base               register offset of this block
 * @features           bit mask identifying sub-blocks/features
 * @sblk:              LM Sub-blocks information
 * @dspp:              ID of connected DSPP, DSPP_MAX if unsupported
 * @pingpong:          ID of connected PingPong, PINGPONG_MAX if unsupported
 * @lm_pair_mask:      Bitmask of LMs that can be controlled by same CTL
 */
struct sde_lm_cfg {
	SDE_HW_BLK_INFO;
	const struct sde_lm_sub_blks *sblk;
	u32 dspp;
	u32 pingpong;
	unsigned long lm_pair_mask;
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
 * @intf_connect       Bitmask of INTF IDs this CDM can connect to
 * @wb_connect:        Bitmask of Writeback IDs this CDM can connect to
 */
struct sde_cdm_cfg   {
	SDE_HW_BLK_INFO;
	unsigned long intf_connect;
	unsigned long wb_connect;
};

/**
 * struct sde_intf_cfg - information of timing engine blocks
 * @id                 enum identifying this block
 * @base               register offset of this block
 * @features           bit mask identifying sub-blocks/features
 * @type:              Interface type(DSI, DP, HDMI)
 * @controller_id:     Controller Instance ID in case of multiple of intf type
 * @prog_fetch_lines_worst_case	Worst case latency num lines needed to prefetch
 */
struct sde_intf_cfg  {
	SDE_HW_BLK_INFO;
	u32 type;   /* interface type*/
	u32 controller_id;
	u32 prog_fetch_lines_worst_case;
};

/**
 * struct sde_wb_cfg - information of writeback blocks
 * @id                 enum identifying this block
 * @base               register offset of this block
 * @features           bit mask identifying sub-blocks/features
 * @sblk               sub-block information
 * @format_list: Pointer to list of supported formats
 * @vbif_idx           vbif identifier
 * @xin_id             client interface identifier
 * @clk_ctrl           clock control identifier
 */
struct sde_wb_cfg {
	SDE_HW_BLK_INFO;
	const struct sde_wb_sub_blocks *sblk;
	const struct sde_format_extended *format_list;
	u32 vbif_idx;
	u32 xin_id;
	enum sde_clk_ctrl_type clk_ctrl;
};

/**
 * struct sde_vbif_dynamic_ot_cfg - dynamic OT setting
 * @pps                pixel per seconds
 * @ot_limit           OT limit to use up to specified pixel per second
 */
struct sde_vbif_dynamic_ot_cfg {
	u64 pps;
	u32 ot_limit;
};

/**
 * struct sde_vbif_dynamic_ot_tbl - dynamic OT setting table
 * @count              length of cfg
 * @cfg                pointer to array of configuration settings with
 *                     ascending requirements
 */
struct sde_vbif_dynamic_ot_tbl {
	u32 count;
	struct sde_vbif_dynamic_ot_cfg *cfg;
};

/**
 * struct sde_vbif_cfg - information of VBIF blocks
 * @id                 enum identifying this block
 * @base               register offset of this block
 * @features           bit mask identifying sub-blocks/features
 * @ot_rd_limit        default OT read limit
 * @ot_wr_limit        default OT write limit
 * @xin_halt_timeout   maximum time (in usec) for xin to halt
 * @dynamic_ot_rd_tbl  dynamic OT read configuration table
 * @dynamic_ot_wr_tbl  dynamic OT write configuration table
 */
struct sde_vbif_cfg {
	SDE_HW_BLK_INFO;
	u32 default_ot_rd_limit;
	u32 default_ot_wr_limit;
	u32 xin_halt_timeout;
	struct sde_vbif_dynamic_ot_tbl dynamic_ot_rd_tbl;
	struct sde_vbif_dynamic_ot_tbl dynamic_ot_wr_tbl;
};

/**
 * struct sde_perf_cfg - performance control settings
 * @max_bw_low         low threshold of maximum bandwidth (kbps)
 * @max_bw_high        high threshold of maximum bandwidth (kbps)
 */
struct sde_perf_cfg {
	u32 max_bw_low;
	u32 max_bw_high;
};

/**
* struct sde_vp_sub_blks - Virtual Plane sub-blocks
* @pipeid_list             list for hw pipe id
* @sspp_id                 SSPP ID, refer to enum sde_sspp.
*/
struct sde_vp_sub_blks {
	struct list_head pipeid_list;
	u32 sspp_id;
};

/**
* struct sde_vp_cfg - information of Virtual Plane SW blocks
* @id                 enum identifying this block
* @sub_blks           list head for virtual plane sub blocks
* @plane_type         plane type, such as primary, overlay or cursor
* @display_type       which display the plane bound to, such as primary,
*                     secondary or tertiary
*/
struct sde_vp_cfg {
	u32 id;
	struct list_head sub_blks;
	const char *plane_type;
	const char *display_type;
};

/**
 * struct sde_mdss_cfg - information of MDSS HW
 * This is the main catalog data structure representing
 * this HW version. Contains number of instances,
 * register offsets, capabilities of the all MDSS HW sub-blocks.
 *
 * @max_sspp_linewidth max source pipe line width support.
 * @max_mixer_width    max layer mixer line width support.
 * @max_mixer_blendstages max layer mixer blend stages or
 *                       supported z order
 * @max_wb_linewidth   max writeback line width support.
 * @highest_bank_bit   highest memory bit setting for tile buffers.
 * @qseed_type         qseed2 or qseed3 support.
 * @csc_type           csc or csc_10bit support.
 * @has_src_split      source split feature status
 * @has_cdp            Client driver prefetch feature status
 * @has_hdr            HDR feature support
 * @dma_formats        Supported formats for dma pipe
 * @cursor_formats     Supported formats for cursor pipe
 * @vig_formats        Supported formats for vig pipe
 * @wb_formats         Supported formats for wb
 */
struct sde_mdss_cfg {
	u32 hwversion;

	u32 max_sspp_linewidth;
	u32 max_mixer_width;
	u32 max_mixer_blendstages;
	u32 max_wb_linewidth;
	u32 highest_bank_bit;
	u32 qseed_type;
	u32 csc_type;
	bool has_src_split;
	bool has_cdp;
	bool has_hdr;
	u32 mdss_count;
	struct sde_mdss_base_cfg mdss[MAX_BLOCKS];

	u32 mdp_count;
	struct sde_mdp_cfg mdp[MAX_BLOCKS];

	u32 ctl_count;
	struct sde_ctl_cfg ctl[MAX_BLOCKS];

	u32 sspp_count;
	struct sde_sspp_cfg sspp[MAX_BLOCKS];

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

	u32 vbif_count;
	struct sde_vbif_cfg vbif[MAX_BLOCKS];
	/* Add additional block data structures here */

	struct sde_perf_cfg perf;

	u32 vp_count;
	struct sde_vp_cfg vp[MAX_BLOCKS];

	struct sde_format_extended *dma_formats;
	struct sde_format_extended *cursor_formats;
	struct sde_format_extended *vig_formats;
	struct sde_format_extended *wb_formats;
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

/**
 * sde_hw_catalog_init - sde hardware catalog init API parses dtsi property
 * and stores all parsed offset, hardware capabilities in config structure.
 * @dev:          drm device node.
 * @hw_rev:       caller needs provide the hardware revision before parsing.
 *
 * Return: parsed sde config structure
 */
struct sde_mdss_cfg *sde_hw_catalog_init(struct drm_device *dev, u32 hw_rev);

/**
 * sde_hw_catalog_deinit - sde hardware catalog cleanup
 * @sde_cfg:      pointer returned from init function
 */
void sde_hw_catalog_deinit(struct sde_mdss_cfg *sde_cfg);

#endif /* _SDE_HW_CATALOG_H */
