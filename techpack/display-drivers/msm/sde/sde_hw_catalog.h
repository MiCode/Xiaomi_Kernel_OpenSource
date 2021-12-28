/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2015-2021, The Linux Foundation. All rights reserved.
 */

#ifndef _SDE_HW_CATALOG_H
#define _SDE_HW_CATALOG_H

#include <linux/kernel.h>
#include <linux/bug.h>
#include <linux/bitmap.h>
#include <linux/err.h>
#include <linux/of_fdt.h>
#include "sde_hw_mdss.h"

/**
 * Max hardware block count: For ex: max 12 SSPP pipes or
 * 5 ctl paths. In all cases, it can have max 12 hardware blocks
 * based on current design
 */
#define MAX_BLOCKS    12
#define MAX_REG_SIZE_ENTRIES 14

#define SDE_HW_VER(MAJOR, MINOR, STEP) ((u32)((MAJOR & 0xF) << 28)    |\
		((MINOR & 0xFFF) << 16)  |\
		(STEP & 0xFFFF))

#define SDE_HW_MAJOR(rev)		((rev) >> 28)
#define SDE_HW_MINOR(rev)		(((rev) >> 16) & 0xFFF)
#define SDE_HW_STEP(rev)		((rev) & 0xFFFF)
#define SDE_HW_MAJOR_MINOR(rev)		((rev) >> 16)

#define SDE_HW_VER_170	SDE_HW_VER(1, 7, 0) /* 8996 */
#define SDE_HW_VER_300	SDE_HW_VER(3, 0, 0) /* 8998 */
#define SDE_HW_VER_400	SDE_HW_VER(4, 0, 0) /* sdm845 */
#define SDE_HW_VER_410	SDE_HW_VER(4, 1, 0) /* sdm670 */
#define SDE_HW_VER_500	SDE_HW_VER(5, 0, 0) /* sm8150 */
#define SDE_HW_VER_510	SDE_HW_VER(5, 1, 0) /* sdmshrike */
#define SDE_HW_VER_520	SDE_HW_VER(5, 2, 0) /* sdmmagpie */
#define SDE_HW_VER_530	SDE_HW_VER(5, 3, 0) /* sm6150 */
#define SDE_HW_VER_540	SDE_HW_VER(5, 4, 0) /* sdmtrinket */
#define SDE_HW_VER_600	SDE_HW_VER(6, 0, 0) /* kona */
#define SDE_HW_VER_610	SDE_HW_VER(6, 1, 0) /* sm7250 */
#define SDE_HW_VER_630	SDE_HW_VER(6, 3, 0) /* bengal */
#define SDE_HW_VER_640	SDE_HW_VER(6, 4, 0) /* lagoon */
#define SDE_HW_VER_650	SDE_HW_VER(6, 5, 0) /* scuba */
#define SDE_HW_VER_660	SDE_HW_VER(6, 6, 0) /* holi */
#define SDE_HW_VER_670	SDE_HW_VER(6, 7, 0) /* shima */
#define SDE_HW_VER_700	SDE_HW_VER(7, 0, 0) /* lahaina */
#define SDE_HW_VER_720	SDE_HW_VER(7, 2, 0) /* yupik */
#define SDE_HW_VER_810	SDE_HW_VER(8, 1, 0) /* waipio */
#define SDE_HW_VER_820	SDE_HW_VER(8, 2, 0) /* diwali */

/* Avoid using below IS_XXX macros outside catalog, use feature bit instead */
#define IS_SDE_MAJOR_SAME(rev1, rev2)   \
		(SDE_HW_MAJOR((rev1)) == SDE_HW_MAJOR((rev2)))
#define IS_SDE_MAJOR_MINOR_SAME(rev1, rev2)   \
		(SDE_HW_MAJOR_MINOR((rev1)) == SDE_HW_MAJOR_MINOR((rev2)))

#define IS_MSM8996_TARGET(rev) IS_SDE_MAJOR_MINOR_SAME((rev), SDE_HW_VER_170)
#define IS_MSM8998_TARGET(rev) IS_SDE_MAJOR_MINOR_SAME((rev), SDE_HW_VER_300)
#define IS_SDM845_TARGET(rev) IS_SDE_MAJOR_MINOR_SAME((rev), SDE_HW_VER_400)
#define IS_SDM670_TARGET(rev) IS_SDE_MAJOR_MINOR_SAME((rev), SDE_HW_VER_410)
#define IS_SM8150_TARGET(rev) IS_SDE_MAJOR_MINOR_SAME((rev), SDE_HW_VER_500)
#define IS_SDMSHRIKE_TARGET(rev) IS_SDE_MAJOR_MINOR_SAME((rev), SDE_HW_VER_510)
#define IS_SDMMAGPIE_TARGET(rev) IS_SDE_MAJOR_MINOR_SAME((rev), SDE_HW_VER_520)
#define IS_SM6150_TARGET(rev) IS_SDE_MAJOR_MINOR_SAME((rev), SDE_HW_VER_530)
#define IS_SDMTRINKET_TARGET(rev) IS_SDE_MAJOR_MINOR_SAME((rev), SDE_HW_VER_540)
#define IS_KONA_TARGET(rev) IS_SDE_MAJOR_MINOR_SAME((rev), SDE_HW_VER_600)
#define IS_SAIPAN_TARGET(rev) IS_SDE_MAJOR_MINOR_SAME((rev), SDE_HW_VER_610)
#define IS_BENGAL_TARGET(rev) IS_SDE_MAJOR_MINOR_SAME((rev), SDE_HW_VER_630)
#define IS_LAGOON_TARGET(rev) IS_SDE_MAJOR_MINOR_SAME((rev), SDE_HW_VER_640)
#define IS_SCUBA_TARGET(rev) IS_SDE_MAJOR_MINOR_SAME((rev), SDE_HW_VER_650)
#define IS_HOLI_TARGET(rev) IS_SDE_MAJOR_MINOR_SAME((rev), SDE_HW_VER_660)
#define IS_SHIMA_TARGET(rev) IS_SDE_MAJOR_MINOR_SAME((rev), SDE_HW_VER_670)
#define IS_LAHAINA_TARGET(rev) IS_SDE_MAJOR_MINOR_SAME((rev), SDE_HW_VER_700)
#define IS_YUPIK_TARGET(rev) IS_SDE_MAJOR_MINOR_SAME((rev), SDE_HW_VER_720)
#define IS_WAIPIO_TARGET(rev) IS_SDE_MAJOR_MINOR_SAME((rev), SDE_HW_VER_810)
#define IS_DIWALI_TARGET(rev) IS_SDE_MAJOR_MINOR_SAME((rev), SDE_HW_VER_820)

#define SDE_HW_BLK_NAME_LEN	16

/* default size of valid register space for MDSS_HW block (offset 0) */
#define DEFAULT_MDSS_HW_BLOCK_SIZE 0x5C

#define MAX_IMG_WIDTH 0x3fff
#define MAX_IMG_HEIGHT 0x3fff

#define CRTC_DUAL_MIXERS_ONLY	2
#define MAX_MIXERS_PER_CRTC	4
#define MAX_MIXERS_PER_LAYOUT	2
#define MAX_LAYOUTS_PER_CRTC (MAX_MIXERS_PER_CRTC / MAX_MIXERS_PER_LAYOUT)

#define SDE_COLOR_PROCESS_VER(MAJOR, MINOR) \
		((((MAJOR) & 0xFFFF) << 16) | (((MINOR) & 0xFFFF)))
#define SDE_COLOR_PROCESS_MAJOR(version) (((version) & 0xFFFF0000) >> 16)
#define SDE_COLOR_PROCESS_MINOR(version) ((version) & 0xFFFF)

#define IS_SDE_CP_VER_1_0(version) \
	(version == SDE_COLOR_PROCESS_VER(0x1, 0x0))

#define MAX_XIN_COUNT 16
#define SSPP_SUBBLK_COUNT_MAX 2

#define SDE_CTL_CFG_VERSION_1_0_0       0x100
#define MAX_INTF_PER_CTL_V1                 2
#define MAX_DSC_PER_CTL_V1                  4
#define MAX_CWB_PER_CTL_V1                  2
#define MAX_MERGE_3D_PER_CTL_V1             2
#define MAX_WB_PER_CTL_V1                   1
#define MAX_CDM_PER_CTL_V1                  1
#define MAX_VDC_PER_CTL_V1                  1
#define IS_SDE_CTL_REV_100(rev) \
	((rev) == SDE_CTL_CFG_VERSION_1_0_0)

/**
 * True inline rotation supported versions
 */
#define SDE_INLINE_ROT_VERSION_1_0_0	0x100
#define SDE_INLINE_ROT_VERSION_2_0_0	0x200
#define SDE_INLINE_ROT_VERSION_2_0_1	0x201

#define IS_SDE_INLINE_ROT_REV_100(rev) \
	((rev) == SDE_INLINE_ROT_VERSION_1_0_0)
#define IS_SDE_INLINE_ROT_REV_200(rev) \
	((rev) == SDE_INLINE_ROT_VERSION_2_0_0)
#define IS_SDE_INLINE_ROT_REV_201(rev) \
	((rev) == SDE_INLINE_ROT_VERSION_2_0_1)


/*
 * UIDLE supported versions
 */
#define SDE_UIDLE_VERSION_1_0_0		0x100
#define SDE_UIDLE_VERSION_1_0_1		0x101
#define SDE_UIDLE_VERSION_1_0_2		0x102

#define IS_SDE_UIDLE_REV_100(rev) \
	((rev) == SDE_UIDLE_VERSION_1_0_0)
#define IS_SDE_UIDLE_REV_101(rev) \
	((rev) == SDE_UIDLE_VERSION_1_0_1)
#define IS_SDE_UIDLE_REV_102(rev) \
	((rev) == SDE_UIDLE_VERSION_1_0_2)

#define SDE_UIDLE_MAJOR(rev)		((rev) >> 8)

#define SDE_HW_UBWC_VER(rev) \
	SDE_HW_VER((((rev) >> 8) & 0xF), (((rev) >> 4) & 0xF), ((rev) & 0xF))

/**
 * Supported UBWC feature versions
 */
enum {
	SDE_HW_UBWC_VER_10 = SDE_HW_UBWC_VER(0x100),
	SDE_HW_UBWC_VER_20 = SDE_HW_UBWC_VER(0x200),
	SDE_HW_UBWC_VER_30 = SDE_HW_UBWC_VER(0x300),
	SDE_HW_UBWC_VER_40 = SDE_HW_UBWC_VER(0x400),
};
#define IS_UBWC_10_SUPPORTED(rev) \
		IS_SDE_MAJOR_MINOR_SAME((rev), SDE_HW_UBWC_VER_10)
#define IS_UBWC_20_SUPPORTED(rev) \
		IS_SDE_MAJOR_MINOR_SAME((rev), SDE_HW_UBWC_VER_20)
#define IS_UBWC_30_SUPPORTED(rev) \
		IS_SDE_MAJOR_MINOR_SAME((rev), SDE_HW_UBWC_VER_30)
#define IS_UBWC_40_SUPPORTED(rev) \
		IS_SDE_MAJOR_MINOR_SAME((rev), SDE_HW_UBWC_VER_40)

/**
 * Supported SSPP system cache settings
 */
#define SSPP_SYS_CACHE_EN_FLAG	BIT(0)
#define SSPP_SYS_CACHE_SCID		BIT(1)
#define SSPP_SYS_CACHE_OP_MODE	BIT(2)
#define SSPP_SYS_CACHE_OP_TYPE	BIT(3)
#define SSPP_SYS_CACHE_NO_ALLOC	BIT(4)

/**
 * sde_sys_cache_type: Types of system cache supported
 * SDE_SYS_CACHE_DISP: Static img system cache
 * SDE_SYS_CACHE_MAX:  Maximum number of sys cache users
 * SDE_SYS_CACHE_NONE: Sys cache not used
 */
enum sde_sys_cache_type {
	SDE_SYS_CACHE_DISP,
	SDE_SYS_CACHE_MAX,
	SDE_SYS_CACHE_NONE = SDE_SYS_CACHE_MAX
};

/**
 * All INTRs relevant for a specific target should be enabled via
 * _add_to_irq_offset_list()
 */
enum sde_intr_hwblk_type {
	SDE_INTR_HWBLK_TOP,
	SDE_INTR_HWBLK_INTF,
	SDE_INTR_HWBLK_AD4,
	SDE_INTR_HWBLK_INTF_TEAR,
	SDE_INTR_HWBLK_LTM,
	SDE_INTR_HWBLK_MAX
};

enum sde_intr_top_intr {
	SDE_INTR_TOP_INTR = 1,
	SDE_INTR_TOP_INTR2,
	SDE_INTR_TOP_HIST_INTR,
	SDE_INTR_TOP_MAX
};

struct sde_intr_irq_offsets {
	struct list_head list;
	enum sde_intr_hwblk_type type;
	u32 instance_idx;
	u32 base_offset;
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
 * @SDE_MDP_WD_TIMER      WD timer support
 * @SDE_MDP_DHDR_MEMPOOL   Dynamic HDR Metadata mempool present
 * @SDE_MDP_DHDR_MEMPOOL_4K Dynamic HDR mempool is 4k aligned
 * @SDE_MDP_PERIPH_TOP_REMOVED Indicates if periph top0 block is removed
 * @SDE_MDP_MAX            Maximum value

 */
enum {
	SDE_MDP_PANIC_PER_PIPE = 0x1,
	SDE_MDP_10BIT_SUPPORT,
	SDE_MDP_BWC,
	SDE_MDP_UBWC_1_0,
	SDE_MDP_UBWC_1_5,
	SDE_MDP_VSYNC_SEL,
	SDE_MDP_WD_TIMER,
	SDE_MDP_DHDR_MEMPOOL,
	SDE_MDP_DHDR_MEMPOOL_4K,
	SDE_MDP_PERIPH_TOP_0_REMOVED,
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
 * @SDE_SSPP_EXCL_RECT,      SSPP supports exclusion rect
 * @SDE_SSPP_SMART_DMA_V1,   SmartDMA 1.0 support
 * @SDE_SSPP_SMART_DMA_V2,   SmartDMA 2.0 support
 * @SDE_SSPP_SMART_DMA_V2p5, SmartDMA 2.5 support
 * @SDE_SSPP_VIG_IGC,        VIG 1D LUT IGC
 * @SDE_SSPP_VIG_GAMUT,      VIG 3D LUT Gamut
 * @SDE_SSPP_DMA_IGC,        DMA 1D LUT IGC
 * @SDE_SSPP_DMA_GC,         DMA 1D LUT GC
 * @SDE_SSPP_INVERSE_PMA     Alpha unmultiply (PMA) support
 * @SDE_SSPP_DGM_INVERSE_PMA Alpha unmultiply (PMA) support in DGM block
 * @SDE_SSPP_DGM_CSC         Support of color space conversion in DGM block
 * @SDE_SSPP_SEC_UI_ALLOWED   Allows secure-ui layers
 * @SDE_SSPP_BLOCK_SEC_UI    Blocks secure-ui layers
 * @SDE_SSPP_SCALER_QSEED3LITE Qseed3lite algorithm support
 * @SDE_SSPP_TRUE_INLINE_ROT Support of SSPP true inline rotation v1
 * @SDE_SSPP_MULTIRECT_ERROR SSPP has error based on RECT0 or RECT1
 * @SDE_SSPP_PREDOWNSCALE    Support pre-downscale X-direction by 2 for inline
 * @SDE_SSPP_PREDOWNSCALE_Y  Support pre-downscale Y-direction for inline
 * @SDE_SSPP_INLINE_CONST_CLR Inline rotation requires const clr disabled
 * @SDE_SSPP_FP16_IGC        FP16 IGC color processing block support
 * @SDE_SSPP_FP16_GC         FP16 GC color processing block support
 * @SDE_SSPP_FP16_CSC        FP16 CSC color processing block support
 * @SDE_SSPP_FP16_UNMULT     FP16 alpha unmult color processing block support
 * @SDE_SSPP_UBWC_STATS:     Support for ubwc stats
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
	SDE_SSPP_EXCL_RECT,
	SDE_SSPP_SMART_DMA_V1,
	SDE_SSPP_SMART_DMA_V2,
	SDE_SSPP_SMART_DMA_V2p5,
	SDE_SSPP_VIG_IGC,
	SDE_SSPP_VIG_GAMUT,
	SDE_SSPP_DMA_IGC,
	SDE_SSPP_DMA_GC,
	SDE_SSPP_INVERSE_PMA,
	SDE_SSPP_DGM_INVERSE_PMA,
	SDE_SSPP_DGM_CSC,
	SDE_SSPP_SEC_UI_ALLOWED,
	SDE_SSPP_BLOCK_SEC_UI,
	SDE_SSPP_SCALER_QSEED3LITE,
	SDE_SSPP_TRUE_INLINE_ROT,
	SDE_SSPP_MULTIRECT_ERROR,
	SDE_SSPP_PREDOWNSCALE,
	SDE_SSPP_PREDOWNSCALE_Y,
	SDE_SSPP_INLINE_CONST_CLR,
	SDE_SSPP_FP16_IGC,
	SDE_SSPP_FP16_GC,
	SDE_SSPP_FP16_CSC,
	SDE_SSPP_FP16_UNMULT,
	SDE_SSPP_UBWC_STATS,
	SDE_SSPP_MAX
};

/**
 * SDE performance features
 * @SDE_PERF_SSPP_QOS,            SSPP support QoS control, danger/safe/creq
 * @SDE_PERF_SSPP_QOS_8LVL,       SSPP support 8-level QoS control
 * @SDE_PERF_SSPP_TS_PREFILL      Supports prefill with traffic shaper
 * @SDE_PERF_SSPP_TS_PREFILL_REC1 Supports prefill with traffic shaper multirec
 * @SDE_PERF_SSPP_CDP             Supports client driven prefetch
 * @SDE_PERF_SSPP_SYS_CACHE,      SSPP supports system cache
 * @SDE_PERF_SSPP_UIDLE,          sspp supports uidle
 * @SDE_PERF_SSPP_MAX             Maximum value
 */
enum {
	SDE_PERF_SSPP_QOS = 0x1,
	SDE_PERF_SSPP_QOS_8LVL,
	SDE_PERF_SSPP_TS_PREFILL,
	SDE_PERF_SSPP_TS_PREFILL_REC1,
	SDE_PERF_SSPP_CDP,
	SDE_PERF_SSPP_SYS_CACHE,
	SDE_PERF_SSPP_UIDLE,
	SDE_PERF_SSPP_MAX
};

/*
 * MIXER sub-blocks/features
 * @SDE_MIXER_LAYER           Layer mixer layer blend configuration,
 * @SDE_MIXER_SOURCESPLIT     Layer mixer supports source-split configuration
 * @SDE_MIXER_GC              Gamma correction block
 * @SDE_DIM_LAYER             Layer mixer supports dim layer
 * @SDE_DISP_CWB_PREF         Layer mixer preferred for CWB
 * @SDE_DISP_DCWB_PREF        Layer mixer preferred for Dedicated CWB
 * @SDE_DISP_PRIMARY_PREF     Layer mixer preferred for primary display
 * @SDE_DISP_SECONDARY_PREF   Layer mixer preferred for secondary display
 * @SDE_MIXER_COMBINED_ALPHA  Layer mixer bg and fg alpha in single register
 * @SDE_MIXER_NOISE_LAYER     Layer mixer supports noise layer
 * @SDE_MIXER_MAX             maximum value
 */
enum {
	SDE_MIXER_LAYER = 0x1,
	SDE_MIXER_SOURCESPLIT,
	SDE_MIXER_GC,
	SDE_DIM_LAYER,
	SDE_DISP_PRIMARY_PREF,
	SDE_DISP_SECONDARY_PREF,
	SDE_DISP_CWB_PREF,
	SDE_DISP_DCWB_PREF,
	SDE_MIXER_COMBINED_ALPHA,
	SDE_MIXER_NOISE_LAYER,
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
 * @SDE_DSPP_GAMUT           Gamut block
 * @SDE_DSPP_DITHER          Dither block
 * @SDE_DSPP_HIST            Histogram block
 * @SDE_DSPP_VLUT            PA VLUT block
 * @SDE_DSPP_AD              AD block
 * @SDE_DSPP_LTM             LTM block
 * @SDE_DSPP_SPR             SPR block
 * @SDE_DSPP_DEMURA          Demura block
 * @SDE_DSPP_RC              RC block
 * @SDE_DSPP_SB              SB LUT DMA
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
	SDE_DSPP_LTM,
	SDE_DSPP_SPR,
	SDE_DSPP_DEMURA,
	SDE_DSPP_RC,
	SDE_DSPP_SB,
	SDE_DSPP_MAX
};

/**
 * LTM sub-features
 * @SDE_LTM_INIT             LTM INIT feature
 * @SDE_LTM_ROI              LTM ROI feature
 * @SDE_LTM_VLUT             LTM VLUT feature
 * @SDE_LTM_MAX              maximum value
 */
enum {
	SDE_LTM_INIT = 0x1,
	SDE_LTM_ROI,
	SDE_LTM_VLUT,
	SDE_LTM_MAX
};

/**
 * PINGPONG sub-blocks
 * @SDE_PINGPONG_TE              Tear check block
 * @SDE_PINGPONG_TE2             Additional tear check block for split pipes
 * @SDE_PINGPONG_SPLIT           PP block supports split fifo
 * @SDE_PINGPONG_SLAVE           PP block is a suitable slave for split fifo
 * @SDE_PINGPONG_DSC,            Display stream compression blocks
 * @SDE_PINGPONG_DITHER,         Dither blocks
 * @SDE_PINGPONG_DITHER_LUMA,    Dither sub-blocks and features
 * @SDE_PINGPONG_MERGE_3D,  Separate MERGE_3D block exists
 * @SDE_PINGPONG_CWB,           PP block supports CWB
 * @SDE_PINGPONG_CWB_DITHER,    PP block supports CWB dither
 * @SDE_PINGPONG_MAX
 */
enum {
	SDE_PINGPONG_TE = 0x1,
	SDE_PINGPONG_TE2,
	SDE_PINGPONG_SPLIT,
	SDE_PINGPONG_SLAVE,
	SDE_PINGPONG_DSC,
	SDE_PINGPONG_DITHER,
	SDE_PINGPONG_DITHER_LUMA,
	SDE_PINGPONG_MERGE_3D,
	SDE_PINGPONG_CWB,
	SDE_PINGPONG_CWB_DITHER,
	SDE_PINGPONG_MAX
};

/** DSC sub-blocks/features
 * @SDE_DSC_OUTPUT_CTRL         Supports the control of the pp id which gets
 *                              the pixel output from this DSC.
 * @SDE_DSC_HW_REV_1_1          dsc block supports dsc 1.1 only
 * @SDE_DSC_HW_REV_1_2          dsc block supports dsc 1.1 and 1.2
 * @SDE_DSC_NATIVE_422_EN,      Supports native422 and native420 encoding
 * @SDE_DSC_ENC,                DSC encoder sub block
 * @SDE_DSC_CTL,                DSC ctl sub block
 * @SDE_DSC_MAX
 */
enum {
	SDE_DSC_OUTPUT_CTRL = 0x1,
	SDE_DSC_HW_REV_1_1,
	SDE_DSC_HW_REV_1_2,
	SDE_DSC_NATIVE_422_EN,
	SDE_DSC_ENC,
	SDE_DSC_CTL,
	SDE_DSC_MAX
};

/** VDC sub-blocks/features
 * @SDE_VDC_HW_REV_1_2         vdc block supports vdc 1.2 only
 * @SDE_VDC_ENC                vdc encoder sub block
 * @SDE_VDC_CTL                 vdc ctl sub block
 * @SDE_VDC_MAX
 */
enum {
	SDE_VDC_HW_REV_1_2,
	SDE_VDC_ENC,
	SDE_VDC_CTL,
	SDE_VDC_MAX
};

/**
 * CTL sub-blocks
 * @SDE_CTL_SPLIT_DISPLAY       CTL supports video mode split display
 * @SDE_CTL_PINGPONG_SPLIT      CTL supports pingpong split
 * @SDE_CTL_PRIMARY_PREF        CTL preferred for primary display
 * @SDE_CTL_ACTIVE_CFG          CTL configuration is specified using active
 *                              blocks
 * @SDE_CTL_UIDLE               CTL supports uidle
 * @SDE_CTL_UNIFIED_DSPP_FLUSH  CTL supports only one flush bit for DSPP
 * @SDE_CTL_MAX
 */
enum {
	SDE_CTL_SPLIT_DISPLAY = 0x1,
	SDE_CTL_PINGPONG_SPLIT,
	SDE_CTL_PRIMARY_PREF,
	SDE_CTL_ACTIVE_CFG,
	SDE_CTL_UIDLE,
	SDE_CTL_UNIFIED_DSPP_FLUSH,
	SDE_CTL_MAX
};

/**
 * INTF sub-blocks
 * @SDE_INTF_INPUT_CTRL         Supports the setting of pp block from which
 *                              pixel data arrives to this INTF
 * @SDE_INTF_TE                 INTF block has TE configuration support
 * @SDE_INTF_TE_ALIGN_VSYNC     INTF block has POMS Align vsync support
 * @SDE_INTF_WD_TIMER          INTF block has WD Timer support
 * @SDE_INTF_STATUS             INTF block has INTF_STATUS register
 * @SDE_INTF_RESET_COUNTER      INTF block has frame/line counter reset support
 * @SDE_INTF_VSYNC_TIMESTAMP    INTF block has vsync timestamp logged
 * @SDE_INTF_AVR_STATUS         INTF block has AVR_STATUS field in AVR_CONTROL register
 * @SDE_INTF_MAX
 */
enum {
	SDE_INTF_INPUT_CTRL = 0x1,
	SDE_INTF_TE,
	SDE_INTF_TE_ALIGN_VSYNC,
	SDE_INTF_WD_TIMER,
	SDE_INTF_STATUS,
	SDE_INTF_RESET_COUNTER,
	SDE_INTF_VSYNC_TIMESTAMP,
	SDE_INTF_AVR_STATUS,
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
 * @SDE_WB_HAS_DCWB         Writeback block supports dedicated CWB
 * @SDE_WB_CROP             CWB supports cropping
 * @SDE_WB_CWB_CTRL         Separate CWB control is available for configuring
 * @SDE_WB_DCWB_CTRL        Separate DCWB control is available for configuring
 * @SDE_WB_CWB_DITHER_CTRL  CWB dither is available for configuring
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
	SDE_WB_HAS_DCWB,
	SDE_WB_CROP,
	SDE_WB_CWB_CTRL,
	SDE_WB_DCWB_CTRL,
	SDE_WB_CWB_DITHER_CTRL,
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
 * @SDE_VBIF_DISABLE_SHAREABLE: VBIF requires inner/outer shareables disabled
 * @SDE_VBIF_MAX              maximum value
 */
enum {
	SDE_VBIF_QOS_OTLIM = 0x1,
	SDE_VBIF_QOS_REMAP,
	SDE_VBIF_DISABLE_SHAREABLE,
	SDE_VBIF_MAX
};

/**
 * uidle features
 * @SDE_UIDLE_QACTIVE_OVERRIDE    uidle sends qactive signal
 * @SDE_UIDLE_MAX                 maximum value
 */
enum {
	SDE_UIDLE_QACTIVE_OVERRIDE = 0x1,
	SDE_UIDLE_MAX
};

/**
 * MACRO SDE_HW_BLK_INFO - information of HW blocks inside SDE
 * @name:              string name for debug purposes
 * @id:                enum identifying this block
 * @base:              register base offset to mdss
 * @len:               length of hardware block
 * @features           bit mask identifying sub-blocks/features
 * @perf_features   bit mask identifying performance sub-blocks/features
 */
#define SDE_HW_BLK_INFO \
	char name[SDE_HW_BLK_NAME_LEN]; \
	u32 id; \
	u32 base; \
	u32 len; \
	union { \
		unsigned long features; \
		u64 features_ext; \
	}; \
	unsigned long perf_features

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
 * @regdma_base: offset of this sub-block relative regdma top
 * @version: qseed block revision
 * @h_preload: horizontal preload
 * @v_preload: vertical preload
 */
struct sde_scaler_blk {
	SDE_HW_SUBBLK_INFO;
	u32 regdma_base;
	u32 version;
	u32 h_preload;
	u32 v_preload;
};

struct sde_csc_blk {
	SDE_HW_SUBBLK_INFO;
};

/**
 * struct sde_pp_blk : Pixel processing sub-blk information
 * @regdma_base: offset of this sub-block relative regdma top
 * @info:   HW register and features supported by this sub-blk
 * @version: HW Algorithm version
 */
struct sde_pp_blk {
	SDE_HW_SUBBLK_INFO;
	u32 regdma_base;
	u32 version;
};

/**
 * struct sde_dsc_blk : DSC Encoder sub-blk information
 * @info:   HW register and features supported by this sub-blk
 */
struct sde_dsc_blk {
	SDE_HW_SUBBLK_INFO;
};

/**
 * struct sde_vdc_blk : VDC Encoder sub-blk information
 * @info:   HW register and features supported by this sub-blk
 */
struct sde_vdc_blk {
	SDE_HW_SUBBLK_INFO;
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
	SDE_QOS_LUT_USAGE_CWB_TILE,
	SDE_QOS_LUT_USAGE_INLINE,
	SDE_QOS_LUT_USAGE_INLINE_RESTRICTED_FMTS,
	SDE_QOS_LUT_USAGE_MAX,
};

/**
 * enum sde_creq_lut_types - define creq LUT types possible for all use cases
 * This is second dimension to sde_qos_lut_usage enum.
 */
enum sde_creq_lut_types {
	SDE_CREQ_LUT_TYPE_NOQSEED,
	SDE_CREQ_LUT_TYPE_QSEED,
	SDE_CREQ_LUT_TYPE_MAX,
};

/**
 * struct sde_sspp_sub_blks : SSPP sub-blocks
 * @maxlinewidth: max source pipe line width support
 * @scaling_linewidth: max vig source pipe linewidth for scaling usecases
 * @maxdwnscale: max downscale ratio supported(without DECIMATION)
 * @maxupscale:  maxupscale ratio supported
 * @maxwidth:    max pixelwidth supported by this pipe
 * @creq_vblank: creq priority during vertical blanking
 * @danger_vblank: danger priority during vertical blanking
 * @pixel_ram_size: size of latency hiding and de-tiling buffer in bytes
 * @smart_dma_priority: hw priority of rect1 of multirect pipe
 * @max_per_pipe_bw: maximum allowable bandwidth of this pipe in kBps
 * @max_per_pipe_bw_high: maximum allowable bandwidth of this pipe in kBps
 *				in case of no VFE
 * @top_off: offset of the sub-block top register relative to sspp top
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
 * @num_fp16_igc_blk: number of FP16 IGC blocks
 * @fp16_igc_blk: FP16 IGC block array
 * @num_fp16_gc_blk: number of FP16 GC blocks
 * @fp16_gc_blk: FP16 GC block array
 * @num_fp16_csc_blk: number of FP16 CSC blocks
 * @fp16_csc_blk: FP16 CSC block array
 * @num_fp16_unmult_blk: number of FP16 UNMULT blocks
 * @fp16_unmult_blk: FP16 UNMULT block array
 * @unmult_offset: Unmult register offset
 * @format_list: Pointer to list of supported formats
 * @virt_format_list: Pointer to list of supported formats for virtual planes
 * @in_rot_format_list: Pointer to list of supported formats for inline rotation
 * @in_rot_maxdwnscale_rt_num: max downscale ratio for inline rotation
 *                                 rt clients - numerator
 * @in_rot_maxdwnscale_rt_denom: max downscale ratio for inline rotation
 *                                 rt clients - denominator
 * @in_rot_maxdwnscale_nrt: max downscale ratio for inline rotation nrt clients
 * @in_rot_maxdwnscale_rt_nopd_num: downscale threshold for when pre-downscale
 *                                    must be enabled on HW with this support.
 * @in_rot_maxdwnscale_rt_nopd_denom: downscale threshold for when pre-downscale
 *                                    must be enabled on HW with this support.
 * @in_rot_maxheight: max pre rotated height for inline rotation
 * @llcc_scid: scid for the system cache
 * @llcc_slice size: slice size of the system cache
 */
struct sde_sspp_sub_blks {
	u32 maxlinewidth;
	u32 scaling_linewidth;
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
	u32 top_off;
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
	u32 num_fp16_igc_blk;
	struct sde_pp_blk fp16_igc_blk[SSPP_SUBBLK_COUNT_MAX];
	u32 num_fp16_gc_blk;
	struct sde_pp_blk fp16_gc_blk[SSPP_SUBBLK_COUNT_MAX];
	u32 num_fp16_csc_blk;
	struct sde_pp_blk fp16_csc_blk[SSPP_SUBBLK_COUNT_MAX];
	u32 num_fp16_unmult_blk;
	struct sde_pp_blk fp16_unmult_blk[SSPP_SUBBLK_COUNT_MAX];
	u32 unmult_offset[SSPP_SUBBLK_COUNT_MAX];

	const struct sde_format_extended *format_list;
	const struct sde_format_extended *virt_format_list;
	const struct sde_format_extended *in_rot_format_list;
	u32 in_rot_maxdwnscale_rt_num;
	u32 in_rot_maxdwnscale_rt_denom;
	u32 in_rot_maxdwnscale_nrt;
	u32 in_rot_maxdwnscale_rt_nopd_num;
	u32 in_rot_maxdwnscale_rt_nopd_denom;
	u32 in_rot_maxheight;
	int llcc_scid;
	size_t llcc_slice_size;
};

/**
 * struct sde_lm_sub_blks:      information of mixer block
 * @maxwidth:               Max pixel width supported by this mixer
 * @maxblendstages:         Max number of blend-stages supported
 * @blendstage_base:        Blend-stage register base offset
 * @gc: gamma correction block
 * @nlayer: noise layer block
 */
struct sde_lm_sub_blks {
	u32 maxwidth;
	u32 maxblendstages;
	u32 blendstage_base[MAX_BLOCKS];
	struct sde_pp_blk gc;
	struct sde_pp_blk nlayer;
};

/**
 * struct sde_dspp_rc: Pixel processing rounded corner sub-blk information
 * @info: HW register and features supported by this sub-blk.
 * @version: HW Algorithm version.
 * @idx: HW block instance id.
 * @mem_total_size: data memory size.
 */
struct sde_dspp_rc {
	SDE_HW_SUBBLK_INFO;
	u32 version;
	u32 idx;
	u32 mem_total_size;
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
	struct sde_pp_blk ltm;
	struct sde_pp_blk spr;
	struct sde_pp_blk vlut;
	struct sde_dspp_rc rc;
	struct sde_pp_blk demura;
};

struct sde_pingpong_sub_blks {
	struct sde_pp_blk te;
	struct sde_pp_blk te2;
	struct sde_pp_blk dsc;
	struct sde_pp_blk dither;
};

/**
 * struct sde_dsc_sub_blks : DSC sub-blks
 *
 */
struct sde_dsc_sub_blks {
	struct sde_dsc_blk enc;
	struct sde_dsc_blk ctl;
};

/**
 * struct sde_vdc_sub_blks : VDC sub-blks
 *
 */
struct sde_vdc_sub_blks {
	struct sde_vdc_blk enc;
	struct sde_vdc_blk ctl;
};

struct sde_wb_sub_blocks {
	u32 maxlinewidth;
	u32 maxlinewidth_linear;
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
 * @clk_status         clock status register definition
 */
struct sde_mdp_cfg {
	SDE_HW_BLK_INFO;
	u32 highest_bank_bit;
	u32 ubwc_static;
	u32 ubwc_swizzle;
	bool has_dest_scaler;
	u32 smart_panel_align_mode;
	struct sde_clk_ctrl_reg clk_ctrls[SDE_CLK_CTRL_MAX];
	struct sde_clk_ctrl_reg clk_status[SDE_CLK_CTRL_MAX];
};

/* struct sde_uidle_cfg : MDP TOP-BLK instance info
 * @id:                     index identifying this block
 * @base:                   register base offset to mdss
 * @features:               bit mask identifying sub-blocks/features
 * @fal10_exit_cnt:         fal10 exit counter
 * @fal10_exit_danger:      fal10 exit danger level
 * @fal10_danger:           fal10 danger level
 * @fal10_target_idle_time: fal10 targeted time in uS
 * @fal1_target_idle_time:  fal1 targeted time in uS
 * @fal10_threshold:        fal10 threshold value
 * @fal1_max_threshold      fal1 maximum allowed threshold value
 * @max_downscale:          maximum downscaling ratio x1000.
 *	                    This ratio is multiplied x1000 to allow
 *	                    3 decimal precision digits.
 * @max_fps:                maximum fps to allow micro idle
 * @max_fal1_fps:           maximum fps to allow micro idle FAL1 only
 * @uidle_rev:              uidle revision supported by the target,
 *                          zero if no support
 * @debugfs_perf:           enable/disable performance counters and status
 *                          logging
 * @debugfs_ctrl:           uidle is enabled/disabled through debugfs
 * @perf_cntr_en:           performance counters are enabled/disabled
 */
struct sde_uidle_cfg {
	SDE_HW_BLK_INFO;
	/* global settings */
	u32 fal10_exit_cnt;
	u32 fal10_exit_danger;
	u32 fal10_danger;
	/* per-pipe settings */
	u32 fal10_target_idle_time;
	u32 fal1_target_idle_time;
	u32 fal10_threshold;
	u32 fal1_max_threshold;
	u32 max_dwnscale;
	u32 max_fps;
	u32 max_fal1_fps;
	u32 uidle_rev;
	u32 debugfs_perf;
	bool debugfs_ctrl;
	bool perf_cntr_en;
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
 * @dummy_mixer:       identifies dcwb mixer is considered dummy
 * @lm_pair_mask:      Bitmask of LMs that can be controlled by same CTL
 */
struct sde_lm_cfg {
	SDE_HW_BLK_INFO;
	struct sde_lm_sub_blks *sblk;
	u32 dspp;
	u32 pingpong;
	u32 ds;
	bool dummy_mixer;
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
	struct sde_dspp_sub_blks *sblk;
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
 * @dsc_pair_mask:     Bitmask of DSCs that can be controlled by same CTL
 */
struct sde_dsc_cfg {
	SDE_HW_BLK_INFO;
	DECLARE_BITMAP(dsc_pair_mask, DSC_MAX);
	struct sde_dsc_sub_blks *sblk;
};

/**
 * struct sde_vdc_cfg - information of VDC blocks
 * @id                 enum identifying this block
 * @base               register offset of this block
 * @len:               length of hardware block
 * @features           bit mask identifying sub-blocks/features
 * @enc                VDC encoder register offset(relative to VDC base)
 * @ctl                VDC Control register offset(relative to VDC base)
 */
struct sde_vdc_cfg {
	SDE_HW_BLK_INFO;
	struct sde_vdc_sub_blks *sblk;
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
 * @te_irq_offset:     Register offset for INTF TE IRQ block
 */
struct sde_intf_cfg  {
	SDE_HW_BLK_INFO;
	u32 type;   /* interface type*/
	u32 controller_id;
	u32 prog_fetch_lines_worst_case;
	u32 te_irq_offset;
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

/*
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
 * enum sde_reg_dma_type - defines reg dma block type
 * @REG_DMA_TYPE_DB: DB LUT DMA block
 * @REG_DMA_TYPE_SB: SB LUT DMA block
 * @REG_DMA_TYPE_MAX: invalid selection
 */
enum sde_reg_dma_type {
	REG_DMA_TYPE_DB,
	REG_DMA_TYPE_SB,
	REG_DMA_TYPE_MAX,
};

/**
 * struct sde_reg_dma_blk_info - definition of lut dma block.
 * @valid              bool indicating if the definiton is valid.
 * @base               register offset of this block.
 * @features           bit mask identifying sub-blocks/features.
 */
struct sde_reg_dma_blk_info {
	bool valid;
	u32 base;
	u32 features;
};

/**
 * struct sde_reg_dma_cfg - overall config struct of lut dma blocks.
 * @reg_dma_blks       Reg DMA blk info for each possible block type
 * @version            version of lutdma hw blocks
 * @trigger_sel_off    offset to trigger select registers of lutdma
 * @broadcast_disabled flag indicating if broadcast usage should be avoided
 * @xin_id             VBIF xin client-id for LUTDMA
 * @vbif_idx           VBIF id (RT/NRT)
 * @base_off           Base offset of LUTDMA from the MDSS root
 * @clk_ctrl           VBIF xin client clk-ctrl
 */
struct sde_reg_dma_cfg {
	struct sde_reg_dma_blk_info reg_dma_blks[REG_DMA_TYPE_MAX];
	u32 version;
	u32 trigger_sel_off;
	u32 broadcast_disabled;
	u32 xin_id;
	u32 vbif_idx;
	u32 base_off;
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
 * struct sde_sc_cfg - define system cache configuration
 * @has_sys_cache: true if system cache is enabled
 * @llcc_scid: scid for the system cache
 * @llcc_slice_size: slice size of the system cache
 */
struct sde_sc_cfg {
	bool has_sys_cache;
	int llcc_scid;
	size_t llcc_slice_size;
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
 * @danger_lut: liner, linear_qseed, macrotile, etc. danger luts
 * @sfe_lut: linear, macrotile, macrotile_qseed, etc. safe luts
 * @creq_lut: linear, macrotile, non_realtime, cwb, etc. creq luts
 * @qos_refresh_count: total refresh count for possible different luts
 * @qos_refresh_rate: different refresh rates for luts
 * @cdp_cfg            cdp use case configurations
 * @cpu_mask:          pm_qos cpu mask value
 * @cpu_mask_perf:     pm_qos cpu silver core mask value
 * @cpu_dma_latency:   pm_qos cpu dma latency value
 * @cpu_irq_latency:   pm_qos cpu irq latency value
 * @num_ddr_channels:  number of DDR channels
 * @dram_efficiency:     DRAM efficiency factor
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
	u64 *danger_lut;
	u64 *safe_lut;
	u64 *creq_lut;
	u32 qos_refresh_count;
	u32 *qos_refresh_rate;
	struct sde_perf_cdp_cfg cdp_cfg[SDE_PERF_CDP_USAGE_MAX];
	unsigned long cpu_mask;
	unsigned long cpu_mask_perf;
	u32 cpu_dma_latency;
	u32 cpu_irq_latency;
	u32 num_ddr_channels;
	u32 dram_efficiency;
	u32 axi_bus_width;
	u32 num_mnoc_ports;
};

/**
 * struct sde_mdss_cfg - information of MDSS HW
 * This is the main catalog data structure representing
 * this HW version. Contains number of instances,
 * register offsets, capabilities of the all MDSS HW sub-blocks.
 *
 * @trusted_vm_env	set to true, if the driver is executing in
 *			the trusted VM. false, otherwise.
 * @max_trusted_vm_displays	maximum number of concurrent trusted
 *				vm displays supported.
 * @tvm_reg_count		number of sub-driver register ranges that need to be included
 *						for trusted vm for accepting the resources
 * @tvm_reg				array of sub-driver register ranges entries that need to be
 *						included
 * @max_sspp_linewidth max source pipe line width support.
 * @vig_sspp_linewidth max vig source pipe line width support.
 * @scaling_linewidth max vig source pipe linewidth for scaling usecases
 * @max_mixer_width    max layer mixer line width support.
 * @max_dsc_width      max dsc line width support.
 * @max_mixer_blendstages max layer mixer blend stages or
 *                       supported z order
 * @max_wb_linewidth   max writeback line width support.
 * @max_wb_linewidth_linear   max writeback line width for linear formats.
 * @max_display_width   maximum display width support.
 * @max_display_height  maximum display height support.

 * @min_display_width   minimum display width support.
 * @min_display_height  minimum display height support.
 * @csc_type           csc or csc_10bit support.
 * @smart_dma_rev      Supported version of SmartDMA feature.
 * @ctl_rev            supported version of control path.
 * @has_src_split      source split feature status
 * @has_cdp            Client driven prefetch feature status
 * @has_wb_ubwc        UBWC feature supported on WB
 * @has_cwb_crop       CWB cropping is supported
 * @has_cwb_support    indicates if device supports primary capture through CWB
 * @has_dedicated_cwb_support    indicates if device supports dedicated path for CWB capture
 * @has_cwb_dither     indicates if device supports cwb dither feature
 * @cwb_blk_off        CWB offset address
 * @cwb_blk_stride     offset between each CWB blk
 * @ubwc_version       UBWC feature version (0x0 for not supported)
 * @ubwc_bw_calc_version indicate how UBWC BW has to be calculated
 * @skip_inline_rot_thresh    Skip inline rotation threshold
 * @has_idle_pc        indicate if idle power collapse feature is supported
 * @allowed_dsc_reservation_switch  intf to which dsc reservation switch is supported
 * @wakeup_with_touch  indicate early wake up display with input touch event
 * @has_hdr            HDR feature support
 * @has_hdr_plus       HDR10+ feature support
 * @dma_formats        Supported formats for dma pipe
 * @cursor_formats     Supported formats for cursor pipe
 * @vig_formats        Supported formats for vig pipe
 * @wb_formats         Supported formats for wb
 * @virt_vig_formats   Supported formats for virtual vig pipe
 * @vbif_qos_nlvl      number of vbif QoS priority level
 * @ts_prefill_rev     prefill traffic shaper feature revision
 * @true_inline_rot_rev	inline rotator feature revision
 * @macrotile_mode     UBWC parameter for macro tile channel distribution
 * @pipe_order_type    indicate if it is required to specify pipe order
 * @sspp_multirect_error flag to indicate whether ubwc and meta error by rect is supported
 * @delay_prg_fetch_start indicates if throttling the fetch start is required
 * @has_qsync	       Supports qsync feature
 * @has_3d_merge_reset Supports 3D merge reset
 * @has_decimation     Supports decimation
 * @has_trusted_vm_support	     Supported HW sharing with trusted VM
 * @has_avr_step       Supports AVR with vsync alignment to a set step rate
 * @rc_lm_flush_override        Support Rounded Corner using layer mixer flush
 * @has_mixer_combined_alpha     Mixer has single register for FG & BG alpha
 * @vbif_disable_inner_outer_shareable     VBIF requires disabling shareables
 * @inline_disable_const_clr     Disable constant color during inline rotate
 * @dither_luma_mode_support   Enables dither luma mode
 * @has_base_layer     Supports staging layer as base layer
 * @demura_supported   Demura pipe support flag(~0x00 - Not supported)
 * @qseed_sw_lib_rev	qseed sw library type supporting the qseed hw
 * @qseed_hw_version   qseed hw version of the target
 * @sc_cfg: system cache configuration
 * @syscache_supported  Flag to indicate if sys cache support is enabled
 * @uidle_cfg		Settings for uidle feature
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
 * @has_cursor    indicates if hardware cursor is supported
 * @has_vig_p010  indicates if vig pipe supports p010 format
 * @has_fp16      indicates if FP16 format is supported on SSPP pipes
 * @has_precise_vsync_ts  indicates if HW has vsyc timestamp logging capability
 * @has_ubwc_stats: indicates if ubwc stats feature is supported
 * @mdss_hw_block_size  Max offset of MDSS_HW block (0 offset), used for debug
 * @inline_rot_formats formats supported by the inline rotator feature
 * @irq_offset_list     list of sde_intr_irq_offsets to initialize irq table
 * @rc_count	number of rounded corner hardware instances
 * @demura_count number of demura hardware instances
 * @dcwb_count          number of dcwb hardware instances
 */
struct sde_mdss_cfg {
	u32 hwversion;
	bool trusted_vm_env;
	u32 max_trusted_vm_displays;
	u32 tvm_reg_count;
	struct resource tvm_reg[MAX_REG_SIZE_ENTRIES];

	u32 max_sspp_linewidth;
	u32 vig_sspp_linewidth;
	u32 scaling_linewidth;
	u32 max_mixer_width;
	u32 max_dsc_width;
	u32 max_mixer_blendstages;
	u32 max_wb_linewidth;
	u32 max_wb_linewidth_linear;

	u32 max_display_width;
	u32 max_display_height;
	u32 min_display_width;
	u32 min_display_height;

	u32 csc_type;
	u32 smart_dma_rev;
	u32 ctl_rev;
	bool has_src_split;
	bool has_cdp;
	bool has_dim_layer;
	bool has_wb_ubwc;
	bool has_cwb_crop;
	bool has_cwb_support;
	bool has_dedicated_cwb_support;
	bool has_cwb_dither;
	u32 cwb_blk_off;
	u32 cwb_blk_stride;
	u32 ubwc_version;
	u32 ubwc_bw_calc_version;
	bool skip_inline_rot_threshold;
	bool has_idle_pc;
	u32 allowed_dsc_reservation_switch;
	bool wakeup_with_touch;
	u32 vbif_qos_nlvl;
	u32 ts_prefill_rev;
	u32 true_inline_rot_rev;
	u32 macrotile_mode;
	u32 pipe_order_type;
	bool sspp_multirect_error;
	bool delay_prg_fetch_start;
	bool has_qsync;
	bool has_3d_merge_reset;
	bool has_decimation;
	bool has_mixer_combined_alpha;
	bool vbif_disable_inner_outer_shareable;
	bool inline_disable_const_clr;
	bool dither_luma_mode_support;
	bool has_base_layer;
	bool has_demura;
	bool has_trusted_vm_support;
	bool has_avr_step;
	bool rc_lm_flush_override;
	u32 demura_supported[SSPP_MAX][2];
	u32 qseed_sw_lib_rev;
	u32 qseed_hw_version;

	struct sde_sc_cfg sc_cfg[SDE_SYS_CACHE_MAX];
	bool syscache_supported;

	bool sui_misr_supported;
	u32 sui_block_xin_mask;

	u32 sec_sid_mask_count;
	u32 sec_sid_mask[MAX_BLOCKS];
	u32 sui_ns_allowed;
	u32 sui_supported_blendstage;
	bool has_sui_blendstage;

	bool has_hdr;
	bool has_hdr_plus;
	bool has_cursor;
	bool has_vig_p010;
	bool has_fp16;
	bool has_precise_vsync_ts;
	bool has_ubwc_stats;

	u32 mdss_hw_block_size;
	u32 mdss_count;
	struct sde_mdss_base_cfg mdss[MAX_BLOCKS];

	u32 mdp_count;
	struct sde_mdp_cfg mdp[MAX_BLOCKS];

	/* uidle is a singleton */
	struct sde_uidle_cfg uidle_cfg;

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

	u32 vdc_count;
	struct sde_vdc_cfg vdc[MAX_BLOCKS];

	u32 cdm_count;
	struct sde_cdm_cfg cdm[MAX_BLOCKS];

	u32 intf_count;
	struct sde_intf_cfg intf[MAX_BLOCKS];

	u32 wb_count;
	struct sde_wb_cfg wb[MAX_BLOCKS];

	u32 vbif_count;
	struct sde_vbif_cfg vbif[MAX_BLOCKS];

	u32 reg_dma_count;
	struct sde_reg_dma_cfg dma_cfg;

	u32 ad_count;
	u32 ltm_count;
	u32 rc_count;
	u32 spr_count;
	u32 demura_count;

	u32 merge_3d_count;
	struct sde_merge_3d_cfg merge_3d[MAX_BLOCKS];

	u32 qdss_count;
	struct sde_qdss_cfg qdss[MAX_BLOCKS];

	u32 dcwb_count;

	/* Add additional block data structures here */

	struct sde_perf_cfg perf;
	struct sde_format_extended *dma_formats;
	struct sde_format_extended *cursor_formats;
	struct sde_format_extended *vig_formats;
	struct sde_format_extended *wb_formats;
	struct sde_format_extended *virt_vig_formats;
	struct sde_format_extended *inline_rot_formats;
	struct sde_format_extended *inline_rot_restricted_formats;

	struct list_head irq_offset_list;
};

struct sde_mdss_hw_cfg_handler {
	u32 major;
	u32 minor;
	struct sde_mdss_cfg* (*cfg_init)(u32 data);
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
#define BLK_LTM(s) ((s)->ltm)
#define BLK_RC(s) ((s)->rc)

/**
 * sde_hw_set_preference: populate the individual hw lm preferences,
 *                        overwrite if exists
 * @sde_cfg:              pointer to sspp cfg
 * @num_lm:               num lms to set preference
 * @disp_type:            is the given display primary/secondary
 */
void sde_hw_mixer_set_preference(struct sde_mdss_cfg *sde_cfg, u32 num_lm,
		uint32_t disp_type);

/**
 * sde_hw_catalog_init - sde hardware catalog init API parses dtsi property
 * and stores all parsed offset, hardware capabilities in config structure.
 * @dev:          drm device node.
 *
 * Return: parsed sde config structure
 */
struct sde_mdss_cfg *sde_hw_catalog_init(struct drm_device *dev);

/**
 * sde_hw_catalog_deinit - sde hardware catalog cleanup
 * @sde_cfg:      pointer returned from init function
 */
void sde_hw_catalog_deinit(struct sde_mdss_cfg *sde_cfg);

/**
 * sde_hw_catalog_irq_offset_list_delete - delete the irq_offset_list
 *                                         maintained by the catalog
 * @head:      pointer to the catalog's irq_offset_list
 */
static inline void sde_hw_catalog_irq_offset_list_delete(
		struct list_head *head)
{
	struct sde_intr_irq_offsets *item, *tmp;

	list_for_each_entry_safe(item, tmp, head, list) {
		list_del(&item->list);
		kfree(item);
	}
}

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
#endif /* _SDE_HW_CATALOG_H */
