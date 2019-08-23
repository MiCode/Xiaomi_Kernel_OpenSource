/* Copyright (c) 2015-2019 The Linux Foundation. All rights reserved.
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

#define IS_SDE_MAJOR_SAME(rev1, rev2)   \
	(SDE_HW_MAJOR((rev1)) == SDE_HW_MAJOR((rev2)))

#define IS_SDE_MAJOR_MINOR_SAME(rev1, rev2)   \
	(SDE_HW_MAJOR_MINOR((rev1)) == SDE_HW_MAJOR_MINOR((rev2)))

#define SDE_HW_VER_170	SDE_HW_VER(1, 7, 0) /* 8996 v1.0 */
#define SDE_HW_VER_171	SDE_HW_VER(1, 7, 1) /* 8996 v2.0 */
#define SDE_HW_VER_172	SDE_HW_VER(1, 7, 2) /* 8996 v3.0 */
#define SDE_HW_VER_300	SDE_HW_VER(3, 0, 0) /* 8998 v1.0 */
#define SDE_HW_VER_301	SDE_HW_VER(3, 0, 1) /* 8998 v1.1 */
#define SDE_HW_VER_400	SDE_HW_VER(4, 0, 0) /* sdm845 v1.0 */
#define SDE_HW_VER_401	SDE_HW_VER(4, 0, 1) /* sdm845 v2.0 */
#define SDE_HW_VER_410	SDE_HW_VER(4, 1, 0) /* sdm670 v1.0 */
#define SDE_HW_VER_500	SDE_HW_VER(5, 0, 0) /* sm8150 v1.0 */
#define SDE_HW_VER_501	SDE_HW_VER(5, 0, 1) /* sm8150 v2.0 */
#define SDE_HW_VER_510	SDE_HW_VER(5, 1, 0) /* sdmshrike v1.0 */
#define SDE_HW_VER_520	SDE_HW_VER(5, 2, 0) /* sdmmagpie v1.0 */
#define SDE_HW_VER_530	SDE_HW_VER(5, 3, 0) /* sm6150 v1.0 */
#define SDE_HW_VER_540	SDE_HW_VER(5, 4, 0) /* sdmtrinket v1.0 */
#define SDE_HW_VER_620	SDE_HW_VER(6, 2, 0) /* atoll*/

#define IS_MSM8996_TARGET(rev) IS_SDE_MAJOR_MINOR_SAME((rev), SDE_HW_VER_170)
#define IS_MSM8998_TARGET(rev) IS_SDE_MAJOR_MINOR_SAME((rev), SDE_HW_VER_300)
#define IS_SDM845_TARGET(rev) IS_SDE_MAJOR_MINOR_SAME((rev), SDE_HW_VER_400)
#define IS_SDM670_TARGET(rev) IS_SDE_MAJOR_MINOR_SAME((rev), SDE_HW_VER_410)
#define IS_SM8150_TARGET(rev) IS_SDE_MAJOR_MINOR_SAME((rev), SDE_HW_VER_500)
#define IS_SDMSHRIKE_TARGET(rev) IS_SDE_MAJOR_MINOR_SAME((rev), SDE_HW_VER_510)
#define IS_SDMMAGPIE_TARGET(rev) IS_SDE_MAJOR_MINOR_SAME((rev), SDE_HW_VER_520)
#define IS_SM6150_TARGET(rev) IS_SDE_MAJOR_MINOR_SAME((rev), SDE_HW_VER_530)
#define IS_SDMTRINKET_TARGET(rev) IS_SDE_MAJOR_MINOR_SAME((rev), SDE_HW_VER_540)
#define IS_ATOLL_TARGET(rev) IS_SDE_MAJOR_MINOR_SAME((rev), SDE_HW_VER_620)

#define SDE_HW_BLK_NAME_LEN	16

#define MAX_IMG_WIDTH 0x3fff
#define MAX_IMG_HEIGHT 0x3fff

#define CRTC_DUAL_MIXERS	2

#define SDE_COLOR_PROCESS_VER(MAJOR, MINOR) \
		((((MAJOR) & 0xFFFF) << 16) | (((MINOR) & 0xFFFF)))
#define SDE_COLOR_PROCESS_MAJOR(version) (((version) & 0xFFFF0000) >> 16)
#define SDE_COLOR_PROCESS_MINOR(version) ((version) & 0xFFFF)

#define MAX_XIN_COUNT 16
#define SSPP_SUBBLK_COUNT_MAX 2

#define SDE_CTL_CFG_VERSION_1_0_0       0x100
#define MAX_INTF_PER_CTL_V1                 2
#define MAX_DSC_PER_CTL_V1                  2
#define MAX_CWB_PER_CTL_V1                  2
#define MAX_MERGE_3D_PER_CTL_V1             2
#define MAX_WB_PER_CTL_V1                   1
#define MAX_CDM_PER_CTL_V1                  1
#define IS_SDE_CTL_REV_100(rev) \
	((rev) == SDE_CTL_CFG_VERSION_1_0_0)

#define SDE_HW_UBWC_VER(rev) \
	SDE_HW_VER((((rev) >> 8) & 0xF), (((rev) >> 4) & 0xF), ((rev) & 0xF))

/**
 * Supported UBWC feature versions
 */
enum {
	SDE_HW_UBWC_VER_10 = SDE_HW_UBWC_VER(0x100),
	SDE_HW_UBWC_VER_20 = SDE_HW_UBWC_VER(0x200),
	SDE_HW_UBWC_VER_30 = SDE_HW_UBWC_VER(0x300),
};
#define IS_UBWC_10_SUPPORTED(rev) \
		IS_SDE_MAJOR_MINOR_SAME((rev), SDE_HW_UBWC_VER_10)
#define IS_UBWC_20_SUPPORTED(rev) \
		IS_SDE_MAJOR_MINOR_SAME((rev), SDE_HW_UBWC_VER_20)
#define IS_UBWC_30_SUPPORTED(rev) \
		IS_SDE_MAJOR_MINOR_SAME((rev), SDE_HW_UBWC_VER_30)

/**
 * SDE INTERRUPTS - maintains the possible hw irq's allowed by HW
 * The order in this enum must match the order of the irqs defined
 * by 'sde_irq_map'
 */
enum sde_intr_enum {
	MDSS_INTR_SSPP_TOP0_INTR,
	MDSS_INTR_SSPP_TOP0_INTR2,
	MDSS_INTR_SSPP_TOP0_HIST_INTR,
	MDSS_INTR_INTF_0_INTR,
	MDSS_INTR_INTF_1_INTR,
	MDSS_INTR_INTF_2_INTR,
	MDSS_INTR_INTF_3_INTR,
	MDSS_INTR_INTF_4_INTR,
	MDSS_INTR_AD4_0_INTR,
	MDSS_INTR_AD4_1_INTR,
	MDSS_INTF_TEAR_1_INTR,
	MDSS_INTF_TEAR_2_INTR,
	MDSS_INTR_MAX
};

/**
 * MDP TOP BLOCK features
 * @SDE_MDP_PANIC_PER_PIPE Panic configuration needs to be be done per pipe
 * @SDE_MDP_10BIT_SUPPORT, Chipset supports 10 bit pixel formats
 * @SDE_MDP_BWC,           MDSS HW supports Bandwidth compression.
 * @SDE_MDP_UBWC_1_0,      This chipsets supports Universal Bandwidth
 *                         compression initial revision
 * @SDE_MDP_UBWC_1_5,      Universal Bandwidth compression version 1.5
 * @SDE_MDP_VSYNC_SEL      Vsync selection for command mode panels
 * @SDE_MDP_MAX            Maximum value

 */
enum {
	SDE_MDP_PANIC_PER_PIPE = 0x1,
	SDE_MDP_10BIT_SUPPORT,
	SDE_MDP_BWC,
	SDE_MDP_UBWC_1_0,
	SDE_MDP_UBWC_1_5,
	SDE_MDP_VSYNC_SEL,
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
 * @SDE_SSPP_PCC,            Color correction support
 * @SDE_SSPP_CURSOR,         SSPP can be used as a cursor layer
 * @SDE_SSPP_QOS,            SSPP support QoS control, danger/safe/creq
 * @SDE_SSPP_QOS_8LVL,       SSPP support 8-level QoS control
 * @SDE_SSPP_EXCL_RECT,      SSPP supports exclusion rect
 * @SDE_SSPP_SMART_DMA_V1,   SmartDMA 1.0 support
 * @SDE_SSPP_SMART_DMA_V2,   SmartDMA 2.0 support
 * @SDE_SSPP_SMART_DMA_V2p5, SmartDMA 2.5 support
 * @SDE_SSPP_SBUF,           SSPP support inline stream buffer
 * @SDE_SSPP_TS_PREFILL      Supports prefill with traffic shaper
 * @SDE_SSPP_TS_PREFILL_REC1 Supports prefill with traffic shaper multirec
 * @SDE_SSPP_CDP             Supports client driven prefetch
 * @SDE_SSPP_VIG_IGC,        VIG 1D LUT IGC
 * @SDE_SSPP_VIG_GAMUT,      VIG 3D LUT Gamut
 * @SDE_SSPP_DMA_IGC,        DMA 1D LUT IGC
 * @SDE_SSPP_DMA_GC,         DMA 1D LUT GC
 * @SDE_SSPP_INVERSE_PMA     Alpha unmultiply (PMA) support
 * @SDE_SSPP_DGM_INVERSE_PMA Alpha unmultiply (PMA) support in DGM block
 * @SDE_SSPP_DGM_CSC         Support of color space conversion in DGM block
 * @SDE_SSPP_SEC_UI_ALLOWED   Allows secure-ui layers
 * @SDE_SSPP_BLOCK_SEC_UI    Blocks secure-ui layers
 * @SDE_SSPP_QOS_FL_NOCALC   Avoid fill level calculation for QoS/danger/safe
 * @SDE_SSPP_SCALER_QSEED3LITE Qseed3lite algorithm support
 * @SDE_SSPP_LINE_INSERTION  Line insertion support
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
	SDE_SSPP_PCC,
	SDE_SSPP_CURSOR,
	SDE_SSPP_QOS,
	SDE_SSPP_QOS_8LVL,
	SDE_SSPP_EXCL_RECT,
	SDE_SSPP_SMART_DMA_V1,
	SDE_SSPP_SMART_DMA_V2,
	SDE_SSPP_SMART_DMA_V2p5,
	SDE_SSPP_SBUF,
	SDE_SSPP_TS_PREFILL,
	SDE_SSPP_TS_PREFILL_REC1,
	SDE_SSPP_CDP,
	SDE_SSPP_VIG_IGC,
	SDE_SSPP_VIG_GAMUT,
	SDE_SSPP_DMA_IGC,
	SDE_SSPP_DMA_GC,
	SDE_SSPP_INVERSE_PMA,
	SDE_SSPP_DGM_INVERSE_PMA,
	SDE_SSPP_DGM_CSC,
	SDE_SSPP_SEC_UI_ALLOWED,
	SDE_SSPP_BLOCK_SEC_UI,
	SDE_SSPP_QOS_FL_NOCALC,
	SDE_SSPP_SCALER_QSEED3LITE,
	SDE_SSPP_LINE_INSERTION,
	SDE_SSPP_MAX
};

/*
 * MIXER sub-blocks/features
 * @SDE_MIXER_LAYER           Layer mixer layer blend configuration,
 * @SDE_MIXER_SOURCESPLIT     Layer mixer supports source-split configuration
 * @SDE_MIXER_GC              Gamma correction block
 * @SDE_DIM_LAYER             Layer mixer supports dim layer
 * @SDE_DISP_CWB_PREF         Layer mixer preferred for CWB
 * @SDE_DISP_PRIMARY_PREF     Layer mixer preferred for primary display
 * @SDE_MIXER_MAX             maximum value
 */
enum {
	SDE_MIXER_LAYER = 0x1,
	SDE_MIXER_SOURCESPLIT,
	SDE_MIXER_GC,
	SDE_DIM_LAYER,
	SDE_DISP_PRIMARY_PREF,
	SDE_DISP_CWB_PREF,
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
 * @SDE_PINGPONG_DITHER,    Dither blocks
 * @SDE_PINGPONG_MERGE_3D,  Separate MERGE_3D block exists
 * @SDE_PINGPONG_MAX
 */
enum {
	SDE_PINGPONG_TE = 0x1,
	SDE_PINGPONG_TE2,
	SDE_PINGPONG_SPLIT,
	SDE_PINGPONG_SLAVE,
	SDE_PINGPONG_DSC,
	SDE_PINGPONG_DITHER,
	SDE_PINGPONG_MERGE_3D,
	SDE_PINGPONG_MAX
};

/** DSC sub-blocks
 * @SDE_DSC_OUTPUT_CTRL         Supports the control of the pp id which gets
 *                              the pixel output from this DSC.
 * @SDE_DSC_MAX
 */
enum {
	SDE_DSC_OUTPUT_CTRL = 0x1,
	SDE_DSC_MAX
};

/**
 * CTL sub-blocks
 * @SDE_CTL_SPLIT_DISPLAY       CTL supports video mode split display
 * @SDE_CTL_PINGPONG_SPLIT      CTL supports pingpong split
 * @SDE_CTL_SBUF                CTL supports inline stream buffer
 * @SDE_CTL_PRIMARY_PREF        CTL preferred for primary display
 * @SDE_CTL_ACTIVE_CFG          CTL configuration is specified using active
 *                              blocks
 * @SDE_CTL_MAX
 */
enum {
	SDE_CTL_SPLIT_DISPLAY = 0x1,
	SDE_CTL_PINGPONG_SPLIT,
	SDE_CTL_SBUF,
	SDE_CTL_PRIMARY_PREF,
	SDE_CTL_ACTIVE_CFG,
	SDE_CTL_MAX
};

/**
 * INTF sub-blocks
 * @SDE_INTF_ROT_START          INTF supports rotator start trigger
 * @SDE_INTF_INPUT_CTRL         Supports the setting of pp block from which
 *                              pixel data arrives to this INTF
 * @SDE_INTF_TE                 INTF block has TE configuration support
 * @SDE_INTF_MAX
 */
enum {
	SDE_INTF_ROT_START = 0x1,
	SDE_INTF_INPUT_CTRL,
	SDE_INTF_TE,
	SDE_INTF_MAX
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
 * @SDE_WB_UBWC,            Writeback Universal bandwidth compression
 * @SDE_WB_YUV_CONFIG       Writeback supports output of YUV colorspace
 * @SDE_WB_PIPE_ALPHA       Writeback supports pipe alpha
 * @SDE_WB_XY_ROI_OFFSET    Writeback supports x/y-offset of out ROI in
 *                          the destination image
 * @SDE_WB_QOS,             Writeback supports QoS control, danger/safe/creq
 * @SDE_WB_QOS_8LVL,        Writeback supports 8-level QoS control
 * @SDE_WB_CDP              Writeback supports client driven prefetch
 * @SDE_WB_INPUT_CTRL       Writeback supports from which pp block input pixel
 *                          data arrives.
 * @SDE_WB_HAS_CWB          Writeback block supports concurrent writeback
 * @SDE_WB_CWB_CTRL         Separate CWB control is available for configuring
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
	SDE_WB_UBWC,
	SDE_WB_YUV_CONFIG,
	SDE_WB_PIPE_ALPHA,
	SDE_WB_XY_ROI_OFFSET,
	SDE_WB_QOS,
	SDE_WB_QOS_8LVL,
	SDE_WB_CDP,
	SDE_WB_INPUT_CTRL,
	SDE_WB_HAS_CWB,
	SDE_WB_CWB_CTRL,
	SDE_WB_MAX
};

/* CDM features
 * @SDE_CDM_INPUT_CTRL     CDM supports from which pp block intput pixel data
 *                         arrives
 * @SDE_CDM_MAX            maximum value
 */
enum {
	SDE_CDM_INPUT_CTRL = 0x1,
	SDE_CDM_MAX
};

/**
 * VBIF sub-blocks and features
 * @SDE_VBIF_QOS_OTLIM        VBIF supports OT Limit
 * @SDE_VBIF_QOS_REMAP        VBIF supports QoS priority remap
 * @SDE_VBIF_MAX              maximum value
 */
enum {
	SDE_VBIF_QOS_OTLIM = 0x1,
	SDE_VBIF_QOS_REMAP,
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
	unsigned long features

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
 * @h_preload: horizontal preload
 * @v_preload: vertical preload
 */
struct sde_scaler_blk {
	SDE_HW_SUBBLK_INFO;
	u32 version;
	u32 h_preload;
	u32 v_preload;
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
 * enum sde_qos_lut_usage - define QoS LUT use cases
 */
enum sde_qos_lut_usage {
	SDE_QOS_LUT_USAGE_LINEAR,
	SDE_QOS_LUT_USAGE_MACROTILE,
	SDE_QOS_LUT_USAGE_NRT,
	SDE_QOS_LUT_USAGE_CWB,
	SDE_QOS_LUT_USAGE_MACROTILE_QSEED,
	SDE_QOS_LUT_USAGE_MAX,
};

/**
 * struct sde_qos_lut_entry - define QoS LUT table entry
 * @fl: fill level, or zero on last entry to indicate default lut
 * @lut: lut to use if equal to or less than fill level
 */
struct sde_qos_lut_entry {
	u32 fl;
	u64 lut;
};

/**
 * struct sde_qos_lut_tbl - define QoS LUT table
 * @nentry: number of entry in this table
 * @entries: Pointer to table entries
 */
struct sde_qos_lut_tbl {
	u32 nentry;
	struct sde_qos_lut_entry *entries;
};

/**
 * struct sde_sspp_sub_blks : SSPP sub-blocks
 * @maxdwnscale: max downscale ratio supported(without DECIMATION)
 * @maxupscale:  maxupscale ratio supported
 * @maxwidth:    max pixelwidth supported by this pipe
 * @creq_vblank: creq priority during vertical blanking
 * @danger_vblank: danger priority during vertical blanking
 * @pixel_ram_size: size of latency hiding and de-tiling buffer in bytes
 * @smart_dma_priority: hw priority of rect1 of multirect pipe
 * @max_per_pipe_bw: maximum allowable bandwidth of this pipe in kBps
 * @max_per_pipe_bw_high: maximum allowable bandwidth of this pipe in kBps
 *                           in case of no VFE
 * @src_blk:
 * @scaler_blk:
 * @csc_blk:
 * @hsic:
 * @memcolor:
 * @pcc_blk:
 * @gamut_blk: 3D LUT gamut block
 * @num_igc_blk: number of IGC block
 * @igc_blk: 1D LUT IGC block
 * @num_gc_blk: number of GC block
 * @gc_blk: 1D LUT GC block
 * @num_dgm_csc_blk: number of DGM CSC blocks
 * @dgm_csc_blk: DGM CSC blocks
 * @format_list: Pointer to list of supported formats
 * @virt_format_list: Pointer to list of supported formats for virtual planes
 */
struct sde_sspp_sub_blks {
	u32 maxlinewidth;
	u32 creq_vblank;
	u32 danger_vblank;
	u32 pixel_ram_size;
	u32 maxdwnscale;
	u32 maxupscale;
	u32 maxhdeciexp; /* max decimation is 2^value */
	u32 maxvdeciexp; /* max decimation is 2^value */
	u32 smart_dma_priority;
	u32 max_per_pipe_bw;
	u32 max_per_pipe_bw_high;
	struct sde_src_blk src_blk;
	struct sde_scaler_blk scaler_blk;
	struct sde_pp_blk csc_blk;
	struct sde_pp_blk hsic_blk;
	struct sde_pp_blk memcolor_blk;
	struct sde_pp_blk pcc_blk;
	struct sde_pp_blk gamut_blk;
	u32 num_igc_blk;
	struct sde_pp_blk igc_blk[SSPP_SUBBLK_COUNT_MAX];
	u32 num_gc_blk;
	struct sde_pp_blk gc_blk[SSPP_SUBBLK_COUNT_MAX];
	u32 num_dgm_csc_blk;
	struct sde_pp_blk dgm_csc_blk[SSPP_SUBBLK_COUNT_MAX];

	const struct sde_format_extended *format_list;
	const struct sde_format_extended *virt_format_list;
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
	struct sde_pp_blk dither;
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
	SDE_CLK_CTRL_INLINE_ROT0_SSPP,
	SDE_CLK_CTRL_INLINE_ROT0_WB,
	SDE_CLK_CTRL_LUTDMA,
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
 * @ubwc_static:       ubwc static configuration
 * @ubwc_swizzle:      ubwc default swizzle setting
 * @has_dest_scaler:   indicates support of destination scaler
 * @smart_panel_align_mode: split display smart panel align modes
 * @clk_ctrls          clock control register definition
 */
struct sde_mdp_cfg {
	SDE_HW_BLK_INFO;
	u32 highest_bank_bit;
	u32 ubwc_static;
	u32 ubwc_swizzle;
	bool has_dest_scaler;
	u32 smart_panel_align_mode;
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
	struct sde_sspp_sub_blks *sblk;
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
 * @ds:                ID of connected DS, DS_MAX if unsupported
 * @lm_pair_mask:      Bitmask of LMs that can be controlled by same CTL
 */
struct sde_lm_cfg {
	SDE_HW_BLK_INFO;
	const struct sde_lm_sub_blks *sblk;
	u32 dspp;
	u32 pingpong;
	u32 ds;
	unsigned long lm_pair_mask;
};

/**
 * struct sde_dspp_cfg - information of DSPP top block
 * @id                 enum identifying this block
 * @base               register offset of this block
 * @features           bit mask identifying sub-blocks/features
 *                     supported by this block
 */
struct sde_dspp_top_cfg  {
	SDE_HW_BLK_INFO;
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
 * struct sde_ds_top_cfg - information of dest scaler top
 * @id               enum identifying this block
 * @base             register offset of this block
 * @features         bit mask identifying features
 * @version          hw version of dest scaler
 * @maxinputwidth    maximum input line width
 * @maxoutputwidth   maximum output line width
 * @maxupscale       maximum upscale ratio
 */
struct sde_ds_top_cfg {
	SDE_HW_BLK_INFO;
	u32 version;
	u32 maxinputwidth;
	u32 maxoutputwidth;
	u32 maxupscale;
};

/**
 * struct sde_ds_cfg - information of dest scaler blocks
 * @id          enum identifying this block
 * @base        register offset wrt DS top offset
 * @features    bit mask identifying features
 * @version     hw version of the qseed block
 * @top         DS top information
 */
struct sde_ds_cfg {
	SDE_HW_BLK_INFO;
	u32 version;
	const struct sde_ds_top_cfg *top;
};

/**
 * struct sde_pingpong_cfg - information of PING-PONG blocks
 * @id                 enum identifying this block
 * @base               register offset of this block
 * @features           bit mask identifying sub-blocks/features
 * @sblk               sub-blocks information
 * @merge_3d_id        merge_3d block id
 */
struct sde_pingpong_cfg  {
	SDE_HW_BLK_INFO;
	const struct sde_pingpong_sub_blks *sblk;
	int merge_3d_id;
};

/**
 * struct sde_dsc_cfg - information of DSC blocks
 * @id                 enum identifying this block
 * @base               register offset of this block
 * @len:               length of hardware block
 * @features           bit mask identifying sub-blocks/features
 */
struct sde_dsc_cfg {
	SDE_HW_BLK_INFO;
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
 * struct sde_merge_3d_cfg - information of merge_3d blocks
 * @id                 enum identifying this block
 * @base               register offset of this block
 * @len:               length of hardware block
 * @features           bit mask identifying sub-blocks/features
 */
struct sde_merge_3d_cfg {
	SDE_HW_BLK_INFO;
};

/**
 * struct sde_qdss_cfg - information of qdss blocks
 * @id                 enum identifying this block
 * @base               register offset of this block
 * @len:               length of hardware block
 * @features           bit mask identifying sub-blocks/features
 */
struct sde_qdss_cfg {
	SDE_HW_BLK_INFO;
};

/**
 * struct sde_rot_vbif_cfg - inline rotator vbif configs
 * @xin_id             xin client id
 * @num                enum identifying this block
 * @is_read            indicates read/write client
 * @clk_ctrl           index to clk control
 */
struct sde_rot_vbif_cfg {
	u32 xin_id;
	u32 num;
	bool is_read;
	enum sde_clk_ctrl_type clk_ctrl;
};

/**
 * struct sde_rot_cfg - information of rotator blocks
 * @id                 enum identifying this block
 * @base               register offset of this block
 * @len                length of hardware block
 * @features           bit mask identifying sub-blocks/features
 * @pdev               private device handle
 * @scid               subcache identifier
 * @slice_size         subcache slice size
 * @vbif_idx           vbif identifier
 * @xin_count          number of xin clients
 * @vbif_cfg           vbif settings related to rotator
 */
struct sde_rot_cfg {
	SDE_HW_BLK_INFO;
	void *pdev;
	int scid;
	size_t slice_size;
	u32 vbif_idx;

	u32 xin_count;
	struct sde_rot_vbif_cfg vbif_cfg[MAX_BLOCKS];
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
 * struct sde_vbif_qos_tbl - QoS priority table
 * @npriority_lvl      num of priority level
 * @priority_lvl       pointer to array of priority level in ascending order
 */
struct sde_vbif_qos_tbl {
	u32 npriority_lvl;
	u32 *priority_lvl;
};

/**
 * enum sde_vbif_client_type
 * @VBIF_RT_CLIENT: real time client
 * @VBIF_NRT_CLIENT: non-realtime clients like writeback
 * @VBIF_CWB_CLIENT: concurrent writeback client
 * @VBIF_LUTDMA_CLIENT: LUTDMA client
 * @VBIF_MAX_CLIENT: max number of clients
 */
enum sde_vbif_client_type {
	VBIF_RT_CLIENT,
	VBIF_NRT_CLIENT,
	VBIF_CWB_CLIENT,
	VBIF_LUTDMA_CLIENT,
	VBIF_MAX_CLIENT
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
 * @qos_tbl            Array of QoS priority table
 * @memtype_count      number of defined memtypes
 * @memtype            array of xin memtype definitions
 */
struct sde_vbif_cfg {
	SDE_HW_BLK_INFO;
	u32 default_ot_rd_limit;
	u32 default_ot_wr_limit;
	u32 xin_halt_timeout;
	struct sde_vbif_dynamic_ot_tbl dynamic_ot_rd_tbl;
	struct sde_vbif_dynamic_ot_tbl dynamic_ot_wr_tbl;
	struct sde_vbif_qos_tbl qos_tbl[VBIF_MAX_CLIENT];
	u32 memtype_count;
	u32 memtype[MAX_XIN_COUNT];
};
/**
 * struct sde_reg_dma_cfg - information of lut dma blocks
 * @id                 enum identifying this block
 * @base               register offset of this block
 * @features           bit mask identifying sub-blocks/features
 * @version            version of lutdma hw block
 * @trigger_sel_off    offset to trigger select registers of lutdma
 * @xin_id             VBIF xin client-id for LUTDMA
 * @vbif_idx           VBIF id (RT/NRT)
 * @clk_ctrl           VBIF xin client clk-ctrl
 */
struct sde_reg_dma_cfg {
	SDE_HW_BLK_INFO;
	u32 version;
	u32 trigger_sel_off;
	u32 xin_id;
	u32 vbif_idx;
	enum sde_clk_ctrl_type clk_ctrl;
};

/**
 * Define CDP use cases
 * @SDE_PERF_CDP_UDAGE_RT: real-time use cases
 * @SDE_PERF_CDP_USAGE_NRT: non real-time use cases such as WFD
 */
enum {
	SDE_PERF_CDP_USAGE_RT,
	SDE_PERF_CDP_USAGE_NRT,
	SDE_PERF_CDP_USAGE_MAX
};

/**
 * struct sde_perf_cdp_cfg - define CDP use case configuration
 * @rd_enable: true if read pipe CDP is enabled
 * @wr_enable: true if write pipe CDP is enabled
 */
struct sde_perf_cdp_cfg {
	bool rd_enable;
	bool wr_enable;
};

/**
 * struct sde_perf_cfg - performance control settings
 * @max_bw_low         low threshold of maximum bandwidth (kbps)
 * @max_bw_high        high threshold of maximum bandwidth (kbps)
 * @min_core_ib        minimum bandwidth for core (kbps)
 * @min_core_ib        minimum mnoc ib vote in kbps
 * @min_llcc_ib        minimum llcc ib vote in kbps
 * @min_dram_ib        minimum dram ib vote in kbps
 * @core_ib_ff         core instantaneous bandwidth fudge factor
 * @core_clk_ff        core clock fudge factor
 * @comp_ratio_rt      string of 0 or more of <fourcc>/<ven>/<mod>/<comp ratio>
 * @comp_ratio_nrt     string of 0 or more of <fourcc>/<ven>/<mod>/<comp ratio>
 * @undersized_prefill_lines   undersized prefill in lines
 * @xtra_prefill_lines         extra prefill latency in lines
 * @dest_scale_prefill_lines   destination scaler latency in lines
 * @macrotile_perfill_lines    macrotile latency in lines
 * @yuv_nv12_prefill_lines     yuv_nv12 latency in lines
 * @linear_prefill_lines       linear latency in lines
 * @downscaling_prefill_lines  downscaling latency in lines
 * @amortizable_theshold minimum y position for traffic shaping prefill
 * @min_prefill_lines  minimum pipeline latency in lines
 * @danger_lut_tbl: LUT tables for danger signals
 * @sfe_lut_tbl: LUT tables for safe signals
 * @qos_lut_tbl: LUT tables for QoS signals
 * @cdp_cfg            cdp use case configurations
 * @cpu_mask:          pm_qos cpu mask value
 * @cpu_dma_latency:   pm_qos cpu dma latency value
 * @axi_bus_width:     axi bus width value in bytes
 * @num_mnoc_ports:    number of mnoc ports
 */
struct sde_perf_cfg {
	u32 max_bw_low;
	u32 max_bw_high;
	u32 min_core_ib;
	u32 min_llcc_ib;
	u32 min_dram_ib;
	const char *core_ib_ff;
	const char *core_clk_ff;
	const char *comp_ratio_rt;
	const char *comp_ratio_nrt;
	u32 undersized_prefill_lines;
	u32 xtra_prefill_lines;
	u32 dest_scale_prefill_lines;
	u32 macrotile_prefill_lines;
	u32 yuv_nv12_prefill_lines;
	u32 linear_prefill_lines;
	u32 downscaling_prefill_lines;
	u32 amortizable_threshold;
	u32 min_prefill_lines;
	u32 danger_lut_tbl[SDE_QOS_LUT_USAGE_MAX];
	struct sde_qos_lut_tbl sfe_lut_tbl[SDE_QOS_LUT_USAGE_MAX];
	struct sde_qos_lut_tbl qos_lut_tbl[SDE_QOS_LUT_USAGE_MAX];
	struct sde_perf_cdp_cfg cdp_cfg[SDE_PERF_CDP_USAGE_MAX];
	u32 cpu_mask;
	u32 cpu_dma_latency;
	u32 axi_bus_width;
	u32 num_mnoc_ports;
};

/**
 * struct sde_mdss_cfg - information of MDSS HW
 * This is the main catalog data structure representing
 * this HW version. Contains number of instances,
 * register offsets, capabilities of the all MDSS HW sub-blocks.
 *
 * @max_sspp_linewidth max source pipe line width support.
 * @vig_sspp_linewidth max vig source pipe line width support.
 * @max_mixer_width    max layer mixer line width support.
 * @max_mixer_blendstages max layer mixer blend stages or
 *                       supported z order
 * @max_wb_linewidth   max writeback line width support.
 * @max_display_width   maximum display width support.
 * @max_display_height  maximum display height support.
 * @max_lm_per_display  maximum layer mixer per display
 * @min_display_width   minimum display width support.
 * @min_display_height  minimum display height support.
 * @qseed_type         qseed2 or qseed3 support.
 * @csc_type           csc or csc_10bit support.
 * @smart_dma_rev      Supported version of SmartDMA feature.
 * @ctl_rev            supported version of control path.
 * @has_src_split      source split feature status
 * @has_cdp            Client driven prefetch feature status
 * @has_wb_ubwc        UBWC feature supported on WB
 * @has_cwb_support    indicates if device supports primary capture through CWB
 * @ubwc_version       UBWC feature version (0x0 for not supported)
 * @ubwc_bw_calc_version indicate how UBWC BW has to be calculated
 * @has_sbuf           indicate if stream buffer is available
 * @sbuf_headroom      stream buffer headroom in lines
 * @sbuf_prefill       stream buffer prefill default in lines
 * @has_idle_pc        indicate if idle power collapse feature is supported
 * @has_hdr            HDR feature support
 * @dma_formats        Supported formats for dma pipe
 * @cursor_formats     Supported formats for cursor pipe
 * @vig_formats        Supported formats for vig pipe
 * @wb_formats         Supported formats for wb
 * @virt_vig_formats   Supported formats for virtual vig pipe
 * @vbif_qos_nlvl      number of vbif QoS priority level
 * @ts_prefill_rev     prefill traffic shaper feature revision
 * @macrotile_mode     UBWC parameter for macro tile channel distribution
 * @pipe_order_type    indicate if it is required to specify pipe order
 * @delay_prg_fetch_start indicates if throttling the fetch start is required
 * @has_qsync	       Supports qsync feature
 * @has_3d_merge_reset Supports 3D merge reset
 * @has_qos_fl_nocalc  flag to indicate QoS fill level needs no calculation
 * @has_decimation     Supports decimation
 * @sui_misr_supported  indicate if secure-ui-misr is supported
 * @sui_block_xin_mask  mask of all the xin-clients to be blocked during
 *                         secure-ui when secure-ui-misr feature is supported
 * @sec_sid_mask_count  number of SID masks
 * @sec_sid_mask        SID masks used during the scm_call for transition
 *                         between secure/non-secure sessions
 * @sui_ns_allowed      flag to indicate non-secure context banks are allowed
 *                         during secure-ui session
 * @sui_supported_blendstage  secure-ui supported blendstage
 * @has_sui_blendstage  flag to indicate secure-ui has a blendstage restriction
 * @mdss_irqs	  bitmap with the irqs supported by the target
 */
struct sde_mdss_cfg {
	u32 hwversion;

	u32 max_sspp_linewidth;
	u32 vig_sspp_linewidth;
	u32 max_mixer_width;
	u32 max_mixer_blendstages;
	u32 max_wb_linewidth;

	u32 max_display_width;
	u32 max_display_height;
	u32 min_display_width;
	u32 min_display_height;
	u32 max_lm_per_display;

	u32 qseed_type;
	u32 csc_type;
	u32 smart_dma_rev;
	u32 ctl_rev;
	bool has_src_split;
	bool has_cdp;
	bool has_dim_layer;
	bool has_wb_ubwc;
	bool has_cwb_support;
	u32 ubwc_version;
	u32 ubwc_bw_calc_version;
	bool has_sbuf;
	u32 sbuf_headroom;
	u32 sbuf_prefill;
	bool has_idle_pc;
	u32 vbif_qos_nlvl;
	u32 ts_prefill_rev;
	u32 macrotile_mode;
	u32 pipe_order_type;
	bool delay_prg_fetch_start;
	bool has_qsync;
	bool has_3d_merge_reset;
	bool has_line_insertion;
	bool has_qos_fl_nocalc;
	bool has_decimation;

	bool sui_misr_supported;
	u32 sui_block_xin_mask;

	u32 sec_sid_mask_count;
	u32 sec_sid_mask[MAX_BLOCKS];
	u32 sui_ns_allowed;
	u32 sui_supported_blendstage;
	bool has_sui_blendstage;

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

	struct sde_dspp_top_cfg dspp_top;

	u32 dspp_count;
	struct sde_dspp_cfg dspp[MAX_BLOCKS];

	u32 ds_count;
	struct sde_ds_cfg ds[MAX_BLOCKS];

	u32 pingpong_count;
	struct sde_pingpong_cfg pingpong[MAX_BLOCKS];

	u32 dsc_count;
	struct sde_dsc_cfg dsc[MAX_BLOCKS];

	u32 cdm_count;
	struct sde_cdm_cfg cdm[MAX_BLOCKS];

	u32 intf_count;
	struct sde_intf_cfg intf[MAX_BLOCKS];

	u32 wb_count;
	struct sde_wb_cfg wb[MAX_BLOCKS];

	u32 rot_count;
	struct sde_rot_cfg rot[MAX_BLOCKS];

	u32 vbif_count;
	struct sde_vbif_cfg vbif[MAX_BLOCKS];

	u32 reg_dma_count;
	struct sde_reg_dma_cfg dma_cfg;

	u32 ad_count;

	u32 merge_3d_count;
	struct sde_merge_3d_cfg merge_3d[MAX_BLOCKS];

	u32 qdss_count;
	struct sde_qdss_cfg qdss[MAX_BLOCKS];

	/* Add additional block data structures here */

	struct sde_perf_cfg perf;
	struct sde_format_extended *dma_formats;
	struct sde_format_extended *cursor_formats;
	struct sde_format_extended *vig_formats;
	struct sde_format_extended *wb_formats;
	struct sde_format_extended *virt_vig_formats;

	DECLARE_BITMAP(mdss_irqs, MDSS_INTR_MAX);
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
#define BLK_DS(s) ((s)->ds)
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

/**
 * sde_hw_sspp_multirect_enabled - check multirect enabled for the sspp
 * @cfg:          pointer to sspp cfg
 */
static inline bool sde_hw_sspp_multirect_enabled(const struct sde_sspp_cfg *cfg)
{
	return test_bit(SDE_SSPP_SMART_DMA_V1, &cfg->features) ||
			 test_bit(SDE_SSPP_SMART_DMA_V2, &cfg->features) ||
			 test_bit(SDE_SSPP_SMART_DMA_V2p5, &cfg->features);
}

static inline bool sde_hw_intf_te_supported(const struct sde_mdss_cfg *sde_cfg)
{
	return test_bit(SDE_INTF_TE, &(sde_cfg->intf[0].features));
}
#endif /* _SDE_HW_CATALOG_H */
