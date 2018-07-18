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

#define pr_fmt(fmt)	"[drm:%s:%d] " fmt, __func__, __LINE__
#include <linux/slab.h>
#include <linux/of_address.h>
#include <linux/platform_device.h>
#include <linux/soc/qcom/llcc-qcom.h>
#include <linux/pm_qos.h>

#include "sde_hw_mdss.h"
#include "sde_hw_catalog.h"
#include "sde_hw_catalog_format.h"
#include "sde_kms.h"

/*************************************************************
 * MACRO DEFINITION
 *************************************************************/

/**
 * Max hardware block in certain hardware. For ex: sspp pipes
 * can have QSEED, pcc, igc, pa, csc, qos entries, etc. This count is
 * 64 based on software design. It should be increased if any of the
 * hardware block has more subblocks.
 */
#define MAX_SDE_HW_BLK  64

/* each entry will have register address and bit offset in that register */
#define MAX_BIT_OFFSET 2

/* default line width for sspp, mixer, ds (input), wb */
#define DEFAULT_SDE_LINE_WIDTH 2048

/* default output line width for ds */
#define DEFAULT_SDE_OUTPUT_LINE_WIDTH 2560

/* max mixer blend stages */
#define DEFAULT_SDE_MIXER_BLENDSTAGES 7

/* max bank bit for macro tile and ubwc format */
#define DEFAULT_SDE_HIGHEST_BANK_BIT 15

/* default ubwc version */
#define DEFAULT_SDE_UBWC_VERSION SDE_HW_UBWC_VER_10

/* default ubwc static config register value */
#define DEFAULT_SDE_UBWC_STATIC 0x0

/* default ubwc swizzle register value */
#define DEFAULT_SDE_UBWC_SWIZZLE 0x0

/* default ubwc macrotile mode value */
#define DEFAULT_SDE_UBWC_MACROTILE_MODE 0x0

/* default hardware block size if dtsi entry is not present */
#define DEFAULT_SDE_HW_BLOCK_LEN 0x100

/* total number of intf - dp, dsi, hdmi */
#define INTF_COUNT			3

#define MAX_UPSCALE_RATIO		20
#define MAX_DOWNSCALE_RATIO		4
#define SSPP_UNITY_SCALE		1

#define MAX_HORZ_DECIMATION		4
#define MAX_VERT_DECIMATION		4

#define MAX_SPLIT_DISPLAY_CTL		2
#define MAX_PP_SPLIT_DISPLAY_CTL	1

#define MDSS_BASE_OFFSET		0x0

#define ROT_LM_OFFSET			3
#define LINE_LM_OFFSET			5
#define LINE_MODE_WB_OFFSET		2

/**
 * these configurations are decided based on max mdp clock. It accounts
 * for max and min display resolution based on virtual hardware resource
 * support.
 */
#define MAX_DISPLAY_HEIGHT_WITH_DECIMATION		2160
#define MAX_DISPLAY_HEIGHT				5120
#define MIN_DISPLAY_HEIGHT				0
#define MIN_DISPLAY_WIDTH				0
#define MAX_LM_PER_DISPLAY				2

/* maximum XIN halt timeout in usec */
#define VBIF_XIN_HALT_TIMEOUT		0x4000

#define DEFAULT_PIXEL_RAM_SIZE		(50 * 1024)

/* access property value based on prop_type and hardware index */
#define PROP_VALUE_ACCESS(p, i, j)		((p + i)->value[j])

/*
 * access element within PROP_TYPE_BIT_OFFSET_ARRAYs based on prop_type,
 * hardware index and offset array index
 */
#define PROP_BITVALUE_ACCESS(p, i, j, k)	((p + i)->bit_value[j][k])

#define DEFAULT_SBUF_HEADROOM		(20)
#define DEFAULT_SBUF_PREFILL		(128)

/*
 * Default parameter values
 */
#define DEFAULT_MAX_BW_HIGH			7000000
#define DEFAULT_MAX_BW_LOW			7000000
#define DEFAULT_UNDERSIZED_PREFILL_LINES	2
#define DEFAULT_XTRA_PREFILL_LINES		2
#define DEFAULT_DEST_SCALE_PREFILL_LINES	3
#define DEFAULT_MACROTILE_PREFILL_LINES		4
#define DEFAULT_YUV_NV12_PREFILL_LINES		8
#define DEFAULT_LINEAR_PREFILL_LINES		1
#define DEFAULT_DOWNSCALING_PREFILL_LINES	1
#define DEFAULT_CORE_IB_FF			"6.0"
#define DEFAULT_CORE_CLK_FF			"1.0"
#define DEFAULT_COMP_RATIO_RT \
		"NV12/5/1/1.23 AB24/5/1/1.23 XB24/5/1/1.23"
#define DEFAULT_COMP_RATIO_NRT \
		"NV12/5/1/1.25 AB24/5/1/1.25 XB24/5/1/1.25"
#define DEFAULT_MAX_PER_PIPE_BW			2400000
#define DEFAULT_AMORTIZABLE_THRESHOLD		25
#define DEFAULT_CPU_MASK			0
#define DEFAULT_CPU_DMA_LATENCY			PM_QOS_DEFAULT_VALUE

/*************************************************************
 *  DTSI PROPERTY INDEX
 *************************************************************/
enum {
	HW_OFF,
	HW_LEN,
	HW_DISP,
	HW_PROP_MAX,
};

enum sde_prop {
	SDE_OFF,
	SDE_LEN,
	SSPP_LINEWIDTH,
	MIXER_LINEWIDTH,
	MIXER_BLEND,
	WB_LINEWIDTH,
	BANK_BIT,
	UBWC_VERSION,
	UBWC_STATIC,
	UBWC_SWIZZLE,
	QSEED_TYPE,
	CSC_TYPE,
	PANIC_PER_PIPE,
	SRC_SPLIT,
	DIM_LAYER,
	SMART_DMA_REV,
	IDLE_PC,
	DEST_SCALER,
	SMART_PANEL_ALIGN_MODE,
	MACROTILE_MODE,
	UBWC_BW_CALC_VERSION,
	PIPE_ORDER_VERSION,
	SEC_SID_MASK,
	SDE_PROP_MAX,
};

enum {
	PERF_MAX_BW_LOW,
	PERF_MAX_BW_HIGH,
	PERF_MIN_CORE_IB,
	PERF_MIN_LLCC_IB,
	PERF_MIN_DRAM_IB,
	PERF_CORE_IB_FF,
	PERF_CORE_CLK_FF,
	PERF_COMP_RATIO_RT,
	PERF_COMP_RATIO_NRT,
	PERF_UNDERSIZED_PREFILL_LINES,
	PERF_DEST_SCALE_PREFILL_LINES,
	PERF_MACROTILE_PREFILL_LINES,
	PERF_YUV_NV12_PREFILL_LINES,
	PERF_LINEAR_PREFILL_LINES,
	PERF_DOWNSCALING_PREFILL_LINES,
	PERF_XTRA_PREFILL_LINES,
	PERF_AMORTIZABLE_THRESHOLD,
	PERF_DANGER_LUT,
	PERF_SAFE_LUT_LINEAR,
	PERF_SAFE_LUT_MACROTILE,
	PERF_SAFE_LUT_NRT,
	PERF_SAFE_LUT_CWB,
	PERF_QOS_LUT_LINEAR,
	PERF_QOS_LUT_MACROTILE,
	PERF_QOS_LUT_NRT,
	PERF_QOS_LUT_CWB,
	PERF_CDP_SETTING,
	PERF_CPU_MASK,
	PERF_CPU_DMA_LATENCY,
	PERF_QOS_LUT_MACROTILE_QSEED,
	PERF_SAFE_LUT_MACROTILE_QSEED,
	PERF_PROP_MAX,
};

enum {
	SSPP_OFF,
	SSPP_SIZE,
	SSPP_TYPE,
	SSPP_XIN,
	SSPP_CLK_CTRL,
	SSPP_CLK_STATUS,
	SSPP_SCALE_SIZE,
	SSPP_VIG_BLOCKS,
	SSPP_RGB_BLOCKS,
	SSPP_DMA_BLOCKS,
	SSPP_EXCL_RECT,
	SSPP_SMART_DMA,
	SSPP_MAX_PER_PIPE_BW,
	SSPP_PROP_MAX,
};

enum {
	VIG_QSEED_OFF,
	VIG_QSEED_LEN,
	VIG_CSC_OFF,
	VIG_HSIC_PROP,
	VIG_MEMCOLOR_PROP,
	VIG_PCC_PROP,
	VIG_GAMUT_PROP,
	VIG_IGC_PROP,
	VIG_INVERSE_PMA,
	VIG_PROP_MAX,
};

enum {
	RGB_SCALER_OFF,
	RGB_SCALER_LEN,
	RGB_PCC_PROP,
	RGB_PROP_MAX,
};

enum {
	DMA_IGC_PROP,
	DMA_GC_PROP,
	DMA_DGM_INVERSE_PMA,
	DMA_CSC_OFF,
	DMA_PROP_MAX,
};

enum {
	INTF_OFF,
	INTF_LEN,
	INTF_PREFETCH,
	INTF_TYPE,
	INTF_PROP_MAX,
};

enum {
	PP_OFF,
	PP_LEN,
	TE_OFF,
	TE_LEN,
	TE2_OFF,
	TE2_LEN,
	PP_SLAVE,
	DITHER_OFF,
	DITHER_LEN,
	DITHER_VER,
	PP_MERGE_3D_ID,
	PP_PROP_MAX,
};

enum {
	DSC_OFF,
	DSC_LEN,
	DSC_PROP_MAX,
};

enum {
	DS_TOP_OFF,
	DS_TOP_LEN,
	DS_TOP_INPUT_LINEWIDTH,
	DS_TOP_OUTPUT_LINEWIDTH,
	DS_TOP_PROP_MAX,
};

enum {
	DS_OFF,
	DS_LEN,
	DS_PROP_MAX,
};

enum {
	DSPP_TOP_OFF,
	DSPP_TOP_SIZE,
	DSPP_TOP_PROP_MAX,
};

enum {
	DSPP_OFF,
	DSPP_SIZE,
	DSPP_BLOCKS,
	DSPP_PROP_MAX,
};

enum {
	DSPP_IGC_PROP,
	DSPP_PCC_PROP,
	DSPP_GC_PROP,
	DSPP_HSIC_PROP,
	DSPP_MEMCOLOR_PROP,
	DSPP_SIXZONE_PROP,
	DSPP_GAMUT_PROP,
	DSPP_DITHER_PROP,
	DSPP_HIST_PROP,
	DSPP_VLUT_PROP,
	DSPP_BLOCKS_PROP_MAX,
};

enum {
	AD_OFF,
	AD_VERSION,
	AD_PROP_MAX,
};

enum {
	MIXER_OFF,
	MIXER_LEN,
	MIXER_PAIR_MASK,
	MIXER_BLOCKS,
	MIXER_DISP,
	MIXER_PROP_MAX,
};

enum {
	MIXER_GC_PROP,
	MIXER_BLOCKS_PROP_MAX,
};

enum {
	MIXER_BLEND_OP_OFF,
	MIXER_BLEND_PROP_MAX,
};

enum {
	WB_OFF,
	WB_LEN,
	WB_ID,
	WB_XIN_ID,
	WB_CLK_CTRL,
	WB_PROP_MAX,
};

enum {
	VBIF_OFF,
	VBIF_LEN,
	VBIF_ID,
	VBIF_DEFAULT_OT_RD_LIMIT,
	VBIF_DEFAULT_OT_WR_LIMIT,
	VBIF_DYNAMIC_OT_RD_LIMIT,
	VBIF_DYNAMIC_OT_WR_LIMIT,
	VBIF_QOS_RT_REMAP,
	VBIF_QOS_NRT_REMAP,
	VBIF_MEMTYPE_0,
	VBIF_MEMTYPE_1,
	VBIF_PROP_MAX,
};

enum {
	REG_DMA_OFF,
	REG_DMA_VERSION,
	REG_DMA_TRIGGER_OFF,
	REG_DMA_PROP_MAX
};

enum {
	INLINE_ROT_XIN,
	INLINE_ROT_XIN_TYPE,
	INLINE_ROT_CLK_CTRL,
	INLINE_ROT_PROP_MAX
};

/*************************************************************
 * dts property definition
 *************************************************************/
enum prop_type {
	PROP_TYPE_BOOL,
	PROP_TYPE_U32,
	PROP_TYPE_U32_ARRAY,
	PROP_TYPE_STRING,
	PROP_TYPE_STRING_ARRAY,
	PROP_TYPE_BIT_OFFSET_ARRAY,
	PROP_TYPE_NODE,
};

struct sde_prop_type {
	/* use property index from enum property for readability purpose */
	u8 id;
	/* it should be property name based on dtsi documentation */
	char *prop_name;
	/**
	 * if property is marked mandatory then it will fail parsing
	 * when property is not present
	 */
	u32  is_mandatory;
	/* property type based on "enum prop_type"  */
	enum prop_type type;
};

struct sde_prop_value {
	u32 value[MAX_SDE_HW_BLK];
	u32 bit_value[MAX_SDE_HW_BLK][MAX_BIT_OFFSET];
};

/*************************************************************
 * dts property list
 *************************************************************/
static struct sde_prop_type sde_prop[] = {
	{SDE_OFF, "qcom,sde-off", true, PROP_TYPE_U32},
	{SDE_LEN, "qcom,sde-len", false, PROP_TYPE_U32},
	{SSPP_LINEWIDTH, "qcom,sde-sspp-linewidth", false, PROP_TYPE_U32},
	{MIXER_LINEWIDTH, "qcom,sde-mixer-linewidth", false, PROP_TYPE_U32},
	{MIXER_BLEND, "qcom,sde-mixer-blendstages", false, PROP_TYPE_U32},
	{WB_LINEWIDTH, "qcom,sde-wb-linewidth", false, PROP_TYPE_U32},
	{BANK_BIT, "qcom,sde-highest-bank-bit", false, PROP_TYPE_U32},
	{UBWC_VERSION, "qcom,sde-ubwc-version", false, PROP_TYPE_U32},
	{UBWC_STATIC, "qcom,sde-ubwc-static", false, PROP_TYPE_U32},
	{UBWC_SWIZZLE, "qcom,sde-ubwc-swizzle", false, PROP_TYPE_U32},
	{QSEED_TYPE, "qcom,sde-qseed-type", false, PROP_TYPE_STRING},
	{CSC_TYPE, "qcom,sde-csc-type", false, PROP_TYPE_STRING},
	{PANIC_PER_PIPE, "qcom,sde-panic-per-pipe", false, PROP_TYPE_BOOL},
	{SRC_SPLIT, "qcom,sde-has-src-split", false, PROP_TYPE_BOOL},
	{DIM_LAYER, "qcom,sde-has-dim-layer", false, PROP_TYPE_BOOL},
	{SMART_DMA_REV, "qcom,sde-smart-dma-rev", false, PROP_TYPE_STRING},
	{IDLE_PC, "qcom,sde-has-idle-pc", false, PROP_TYPE_BOOL},
	{DEST_SCALER, "qcom,sde-has-dest-scaler", false, PROP_TYPE_BOOL},
	{SMART_PANEL_ALIGN_MODE, "qcom,sde-smart-panel-align-mode",
			false, PROP_TYPE_U32},
	{MACROTILE_MODE, "qcom,sde-macrotile-mode", false, PROP_TYPE_U32},
	{UBWC_BW_CALC_VERSION, "qcom,sde-ubwc-bw-calc-version", false,
			PROP_TYPE_U32},
	{PIPE_ORDER_VERSION, "qcom,sde-pipe-order-version", false,
			PROP_TYPE_U32},
	{SEC_SID_MASK, "qcom,sde-secure-sid-mask", false, PROP_TYPE_U32_ARRAY},
};

static struct sde_prop_type sde_perf_prop[] = {
	{PERF_MAX_BW_LOW, "qcom,sde-max-bw-low-kbps", false, PROP_TYPE_U32},
	{PERF_MAX_BW_HIGH, "qcom,sde-max-bw-high-kbps", false, PROP_TYPE_U32},
	{PERF_MIN_CORE_IB, "qcom,sde-min-core-ib-kbps", false, PROP_TYPE_U32},
	{PERF_MIN_LLCC_IB, "qcom,sde-min-llcc-ib-kbps", false, PROP_TYPE_U32},
	{PERF_MIN_DRAM_IB, "qcom,sde-min-dram-ib-kbps", false, PROP_TYPE_U32},
	{PERF_CORE_IB_FF, "qcom,sde-core-ib-ff", false, PROP_TYPE_STRING},
	{PERF_CORE_CLK_FF, "qcom,sde-core-clk-ff", false, PROP_TYPE_STRING},
	{PERF_COMP_RATIO_RT, "qcom,sde-comp-ratio-rt", false,
			PROP_TYPE_STRING},
	{PERF_COMP_RATIO_NRT, "qcom,sde-comp-ratio-nrt", false,
			PROP_TYPE_STRING},
	{PERF_UNDERSIZED_PREFILL_LINES, "qcom,sde-undersizedprefill-lines",
			false, PROP_TYPE_U32},
	{PERF_DEST_SCALE_PREFILL_LINES, "qcom,sde-dest-scaleprefill-lines",
			false, PROP_TYPE_U32},
	{PERF_MACROTILE_PREFILL_LINES, "qcom,sde-macrotileprefill-lines",
			false, PROP_TYPE_U32},
	{PERF_YUV_NV12_PREFILL_LINES, "qcom,sde-yuv-nv12prefill-lines",
			false, PROP_TYPE_U32},
	{PERF_LINEAR_PREFILL_LINES, "qcom,sde-linearprefill-lines",
			false, PROP_TYPE_U32},
	{PERF_DOWNSCALING_PREFILL_LINES, "qcom,sde-downscalingprefill-lines",
			false, PROP_TYPE_U32},
	{PERF_XTRA_PREFILL_LINES, "qcom,sde-xtra-prefill-lines",
			false, PROP_TYPE_U32},
	{PERF_AMORTIZABLE_THRESHOLD, "qcom,sde-amortizable-threshold",
			false, PROP_TYPE_U32},
	{PERF_DANGER_LUT, "qcom,sde-danger-lut", false, PROP_TYPE_U32_ARRAY},
	{PERF_SAFE_LUT_LINEAR, "qcom,sde-safe-lut-linear", false,
			PROP_TYPE_U32_ARRAY},
	{PERF_SAFE_LUT_MACROTILE, "qcom,sde-safe-lut-macrotile", false,
			PROP_TYPE_U32_ARRAY},
	{PERF_SAFE_LUT_NRT, "qcom,sde-safe-lut-nrt", false,
			PROP_TYPE_U32_ARRAY},
	{PERF_SAFE_LUT_CWB, "qcom,sde-safe-lut-cwb", false,
			PROP_TYPE_U32_ARRAY},
	{PERF_QOS_LUT_LINEAR, "qcom,sde-qos-lut-linear", false,
			PROP_TYPE_U32_ARRAY},
	{PERF_QOS_LUT_MACROTILE, "qcom,sde-qos-lut-macrotile", false,
			PROP_TYPE_U32_ARRAY},
	{PERF_QOS_LUT_NRT, "qcom,sde-qos-lut-nrt", false,
			PROP_TYPE_U32_ARRAY},
	{PERF_QOS_LUT_CWB, "qcom,sde-qos-lut-cwb", false,
			PROP_TYPE_U32_ARRAY},

	{PERF_CDP_SETTING, "qcom,sde-cdp-setting", false,
			PROP_TYPE_U32_ARRAY},
	{PERF_CPU_MASK, "qcom,sde-qos-cpu-mask", false, PROP_TYPE_U32},
	{PERF_CPU_DMA_LATENCY, "qcom,sde-qos-cpu-dma-latency", false,
			PROP_TYPE_U32},
	{PERF_QOS_LUT_MACROTILE_QSEED, "qcom,sde-qos-lut-macrotile-qseed",
			false, PROP_TYPE_U32_ARRAY},
	{PERF_SAFE_LUT_MACROTILE_QSEED, "qcom,sde-safe-lut-macrotile-qseed",
			false, PROP_TYPE_U32_ARRAY},
};

static struct sde_prop_type sspp_prop[] = {
	{SSPP_OFF, "qcom,sde-sspp-off", true, PROP_TYPE_U32_ARRAY},
	{SSPP_SIZE, "qcom,sde-sspp-src-size", false, PROP_TYPE_U32},
	{SSPP_TYPE, "qcom,sde-sspp-type", true, PROP_TYPE_STRING_ARRAY},
	{SSPP_XIN, "qcom,sde-sspp-xin-id", true, PROP_TYPE_U32_ARRAY},
	{SSPP_CLK_CTRL, "qcom,sde-sspp-clk-ctrl", false,
		PROP_TYPE_BIT_OFFSET_ARRAY},
	{SSPP_CLK_STATUS, "qcom,sde-sspp-clk-status", false,
		PROP_TYPE_BIT_OFFSET_ARRAY},
	{SSPP_SCALE_SIZE, "qcom,sde-sspp-scale-size", false, PROP_TYPE_U32},
	{SSPP_VIG_BLOCKS, "qcom,sde-sspp-vig-blocks", false, PROP_TYPE_NODE},
	{SSPP_RGB_BLOCKS, "qcom,sde-sspp-rgb-blocks", false, PROP_TYPE_NODE},
	{SSPP_DMA_BLOCKS, "qcom,sde-sspp-dma-blocks", false, PROP_TYPE_NODE},
	{SSPP_EXCL_RECT, "qcom,sde-sspp-excl-rect", false, PROP_TYPE_U32_ARRAY},
	{SSPP_SMART_DMA, "qcom,sde-sspp-smart-dma-priority", false,
		PROP_TYPE_U32_ARRAY},
	{SSPP_MAX_PER_PIPE_BW, "qcom,sde-max-per-pipe-bw-kbps", false,
		PROP_TYPE_U32_ARRAY},
};

static struct sde_prop_type vig_prop[] = {
	{VIG_QSEED_OFF, "qcom,sde-vig-qseed-off", false, PROP_TYPE_U32},
	{VIG_QSEED_LEN, "qcom,sde-vig-qseed-size", false, PROP_TYPE_U32},
	{VIG_CSC_OFF, "qcom,sde-vig-csc-off", false, PROP_TYPE_U32},
	{VIG_HSIC_PROP, "qcom,sde-vig-hsic", false, PROP_TYPE_U32_ARRAY},
	{VIG_MEMCOLOR_PROP, "qcom,sde-vig-memcolor", false,
		PROP_TYPE_U32_ARRAY},
	{VIG_PCC_PROP, "qcom,sde-vig-pcc", false, PROP_TYPE_U32_ARRAY},
	{VIG_GAMUT_PROP, "qcom,sde-vig-gamut", false, PROP_TYPE_U32_ARRAY},
	{VIG_IGC_PROP, "qcom,sde-vig-igc", false, PROP_TYPE_U32_ARRAY},
	{VIG_INVERSE_PMA, "qcom,sde-vig-inverse-pma", false, PROP_TYPE_BOOL},
};

static struct sde_prop_type rgb_prop[] = {
	{RGB_SCALER_OFF, "qcom,sde-rgb-scaler-off", false, PROP_TYPE_U32},
	{RGB_SCALER_LEN, "qcom,sde-rgb-scaler-size", false, PROP_TYPE_U32},
	{RGB_PCC_PROP, "qcom,sde-rgb-pcc", false, PROP_TYPE_U32_ARRAY},
};

static struct sde_prop_type dma_prop[] = {
	{DMA_IGC_PROP, "qcom,sde-dma-igc", false, PROP_TYPE_U32_ARRAY},
	{DMA_GC_PROP, "qcom,sde-dma-gc", false, PROP_TYPE_U32_ARRAY},
	{DMA_DGM_INVERSE_PMA, "qcom,sde-dma-inverse-pma", false,
		PROP_TYPE_BOOL},
	{DMA_CSC_OFF, "qcom,sde-dma-csc-off", false, PROP_TYPE_U32},
};

static struct sde_prop_type ctl_prop[] = {
	{HW_OFF, "qcom,sde-ctl-off", true, PROP_TYPE_U32_ARRAY},
	{HW_LEN, "qcom,sde-ctl-size", false, PROP_TYPE_U32},
	{HW_DISP, "qcom,sde-ctl-display-pref", false, PROP_TYPE_STRING_ARRAY},
};

struct sde_prop_type mixer_blend_prop[] = {
	{MIXER_BLEND_OP_OFF, "qcom,sde-mixer-blend-op-off", true,
		PROP_TYPE_U32_ARRAY},
};

static struct sde_prop_type mixer_prop[] = {
	{MIXER_OFF, "qcom,sde-mixer-off", true, PROP_TYPE_U32_ARRAY},
	{MIXER_LEN, "qcom,sde-mixer-size", false, PROP_TYPE_U32},
	{MIXER_PAIR_MASK, "qcom,sde-mixer-pair-mask", true,
		PROP_TYPE_U32_ARRAY},
	{MIXER_BLOCKS, "qcom,sde-mixer-blocks", false, PROP_TYPE_NODE},
	{MIXER_DISP, "qcom,sde-mixer-display-pref", false,
		PROP_TYPE_STRING_ARRAY},
};

static struct sde_prop_type mixer_blocks_prop[] = {
	{MIXER_GC_PROP, "qcom,sde-mixer-gc", false, PROP_TYPE_U32_ARRAY},
};

static struct sde_prop_type dspp_top_prop[] = {
	{DSPP_TOP_OFF, "qcom,sde-dspp-top-off", true, PROP_TYPE_U32},
	{DSPP_TOP_SIZE, "qcom,sde-dspp-top-size", false, PROP_TYPE_U32},
};

static struct sde_prop_type dspp_prop[] = {
	{DSPP_OFF, "qcom,sde-dspp-off", true, PROP_TYPE_U32_ARRAY},
	{DSPP_SIZE, "qcom,sde-dspp-size", false, PROP_TYPE_U32},
	{DSPP_BLOCKS, "qcom,sde-dspp-blocks", false, PROP_TYPE_NODE},
};

static struct sde_prop_type dspp_blocks_prop[] = {
	{DSPP_IGC_PROP, "qcom,sde-dspp-igc", false, PROP_TYPE_U32_ARRAY},
	{DSPP_PCC_PROP, "qcom,sde-dspp-pcc", false, PROP_TYPE_U32_ARRAY},
	{DSPP_GC_PROP, "qcom,sde-dspp-gc", false, PROP_TYPE_U32_ARRAY},
	{DSPP_HSIC_PROP, "qcom,sde-dspp-hsic", false, PROP_TYPE_U32_ARRAY},
	{DSPP_MEMCOLOR_PROP, "qcom,sde-dspp-memcolor", false,
		PROP_TYPE_U32_ARRAY},
	{DSPP_SIXZONE_PROP, "qcom,sde-dspp-sixzone", false,
		PROP_TYPE_U32_ARRAY},
	{DSPP_GAMUT_PROP, "qcom,sde-dspp-gamut", false, PROP_TYPE_U32_ARRAY},
	{DSPP_DITHER_PROP, "qcom,sde-dspp-dither", false, PROP_TYPE_U32_ARRAY},
	{DSPP_HIST_PROP, "qcom,sde-dspp-hist", false, PROP_TYPE_U32_ARRAY},
	{DSPP_VLUT_PROP, "qcom,sde-dspp-vlut", false, PROP_TYPE_U32_ARRAY},
};

static struct sde_prop_type ad_prop[] = {
	{AD_OFF, "qcom,sde-dspp-ad-off", false, PROP_TYPE_U32_ARRAY},
	{AD_VERSION, "qcom,sde-dspp-ad-version", false, PROP_TYPE_U32},
};

static struct sde_prop_type ds_top_prop[] = {
	{DS_TOP_OFF, "qcom,sde-dest-scaler-top-off", false, PROP_TYPE_U32},
	{DS_TOP_LEN, "qcom,sde-dest-scaler-top-size", false, PROP_TYPE_U32},
	{DS_TOP_INPUT_LINEWIDTH, "qcom,sde-max-dest-scaler-input-linewidth",
		false, PROP_TYPE_U32},
	{DS_TOP_OUTPUT_LINEWIDTH, "qcom,sde-max-dest-scaler-output-linewidth",
		false, PROP_TYPE_U32},
};

static struct sde_prop_type ds_prop[] = {
	{DS_OFF, "qcom,sde-dest-scaler-off", false, PROP_TYPE_U32_ARRAY},
	{DS_LEN, "qcom,sde-dest-scaler-size", false, PROP_TYPE_U32},
};

static struct sde_prop_type pp_prop[] = {
	{PP_OFF, "qcom,sde-pp-off", true, PROP_TYPE_U32_ARRAY},
	{PP_LEN, "qcom,sde-pp-size", false, PROP_TYPE_U32},
	{TE_OFF, "qcom,sde-te-off", false, PROP_TYPE_U32_ARRAY},
	{TE_LEN, "qcom,sde-te-size", false, PROP_TYPE_U32},
	{TE2_OFF, "qcom,sde-te2-off", false, PROP_TYPE_U32_ARRAY},
	{TE2_LEN, "qcom,sde-te2-size", false, PROP_TYPE_U32},
	{PP_SLAVE, "qcom,sde-pp-slave", false, PROP_TYPE_U32_ARRAY},
	{DITHER_OFF, "qcom,sde-dither-off", false, PROP_TYPE_U32_ARRAY},
	{DITHER_LEN, "qcom,sde-dither-size", false, PROP_TYPE_U32},
	{DITHER_VER, "qcom,sde-dither-version", false, PROP_TYPE_U32},
	{PP_MERGE_3D_ID, "qcom,sde-pp-merge-3d-id", false, PROP_TYPE_U32_ARRAY},
};

static struct sde_prop_type dsc_prop[] = {
	{DSC_OFF, "qcom,sde-dsc-off", false, PROP_TYPE_U32_ARRAY},
	{DSC_LEN, "qcom,sde-dsc-size", false, PROP_TYPE_U32},
};

static struct sde_prop_type cdm_prop[] = {
	{HW_OFF, "qcom,sde-cdm-off", false, PROP_TYPE_U32_ARRAY},
	{HW_LEN, "qcom,sde-cdm-size", false, PROP_TYPE_U32},
};

static struct sde_prop_type intf_prop[] = {
	{INTF_OFF, "qcom,sde-intf-off", true, PROP_TYPE_U32_ARRAY},
	{INTF_LEN, "qcom,sde-intf-size", false, PROP_TYPE_U32},
	{INTF_PREFETCH, "qcom,sde-intf-max-prefetch-lines", false,
						PROP_TYPE_U32_ARRAY},
	{INTF_TYPE, "qcom,sde-intf-type", false, PROP_TYPE_STRING_ARRAY},
};

static struct sde_prop_type wb_prop[] = {
	{WB_OFF, "qcom,sde-wb-off", true, PROP_TYPE_U32_ARRAY},
	{WB_LEN, "qcom,sde-wb-size", false, PROP_TYPE_U32},
	{WB_ID, "qcom,sde-wb-id", true, PROP_TYPE_U32_ARRAY},
	{WB_XIN_ID, "qcom,sde-wb-xin-id", false, PROP_TYPE_U32_ARRAY},
	{WB_CLK_CTRL, "qcom,sde-wb-clk-ctrl", false,
		PROP_TYPE_BIT_OFFSET_ARRAY},
};

static struct sde_prop_type vbif_prop[] = {
	{VBIF_OFF, "qcom,sde-vbif-off", true, PROP_TYPE_U32_ARRAY},
	{VBIF_LEN, "qcom,sde-vbif-size", false, PROP_TYPE_U32},
	{VBIF_ID, "qcom,sde-vbif-id", false, PROP_TYPE_U32_ARRAY},
	{VBIF_DEFAULT_OT_RD_LIMIT, "qcom,sde-vbif-default-ot-rd-limit", false,
		PROP_TYPE_U32},
	{VBIF_DEFAULT_OT_WR_LIMIT, "qcom,sde-vbif-default-ot-wr-limit", false,
		PROP_TYPE_U32},
	{VBIF_DYNAMIC_OT_RD_LIMIT, "qcom,sde-vbif-dynamic-ot-rd-limit", false,
		PROP_TYPE_U32_ARRAY},
	{VBIF_DYNAMIC_OT_WR_LIMIT, "qcom,sde-vbif-dynamic-ot-wr-limit", false,
		PROP_TYPE_U32_ARRAY},
	{VBIF_QOS_RT_REMAP, "qcom,sde-vbif-qos-rt-remap", false,
		PROP_TYPE_U32_ARRAY},
	{VBIF_QOS_NRT_REMAP, "qcom,sde-vbif-qos-nrt-remap", false,
		PROP_TYPE_U32_ARRAY},
	{VBIF_MEMTYPE_0, "qcom,sde-vbif-memtype-0", false, PROP_TYPE_U32_ARRAY},
	{VBIF_MEMTYPE_1, "qcom,sde-vbif-memtype-1", false, PROP_TYPE_U32_ARRAY},
};

static struct sde_prop_type reg_dma_prop[REG_DMA_PROP_MAX] = {
	[REG_DMA_OFF] =  {REG_DMA_OFF, "qcom,sde-reg-dma-off", false,
		PROP_TYPE_U32},
	[REG_DMA_VERSION] = {REG_DMA_VERSION, "qcom,sde-reg-dma-version",
		false, PROP_TYPE_U32},
	[REG_DMA_TRIGGER_OFF] = {REG_DMA_TRIGGER_OFF,
		"qcom,sde-reg-dma-trigger-off", false,
		PROP_TYPE_U32},
};

static struct sde_prop_type merge_3d_prop[] = {
	{HW_OFF, "qcom,sde-merge-3d-off", false, PROP_TYPE_U32_ARRAY},
	{HW_LEN, "qcom,sde-merge-3d-size", false, PROP_TYPE_U32},
};

static struct sde_prop_type inline_rot_prop[INLINE_ROT_PROP_MAX] = {
	{INLINE_ROT_XIN, "qcom,sde-inline-rot-xin", false,
							PROP_TYPE_U32_ARRAY},
	{INLINE_ROT_XIN_TYPE, "qcom,sde-inline-rot-xin-type", false,
							PROP_TYPE_STRING_ARRAY},
	{INLINE_ROT_CLK_CTRL, "qcom,sde-inline-rot-clk-ctrl", false,
						PROP_TYPE_BIT_OFFSET_ARRAY},
};

/*************************************************************
 * static API list
 *************************************************************/

static int _parse_dt_u32_handler(struct device_node *np,
	char *prop_name, u32 *offsets, int len, bool mandatory)
{
	int rc = -EINVAL;

	if (len > MAX_SDE_HW_BLK) {
		SDE_ERROR(
			"prop: %s tries out of bound access for u32 array read len: %d\n",
				prop_name, len);
		return -E2BIG;
	}

	rc = of_property_read_u32_array(np, prop_name, offsets, len);
	if (rc && mandatory)
		SDE_ERROR("mandatory prop: %s u32 array read len:%d\n",
				prop_name, len);
	else if (rc)
		SDE_DEBUG("optional prop: %s u32 array read len:%d\n",
				prop_name, len);

	return rc;
}

static int _parse_dt_bit_offset(struct device_node *np,
	char *prop_name, struct sde_prop_value *prop_value, u32 prop_index,
	u32 count, bool mandatory)
{
	int rc = 0, len, i, j;
	const u32 *arr;

	arr = of_get_property(np, prop_name, &len);
	if (arr) {
		len /= sizeof(u32);
		len &= ~0x1;

		if (len > (MAX_SDE_HW_BLK * MAX_BIT_OFFSET)) {
			SDE_ERROR(
				"prop: %s len: %d will lead to out of bound access\n",
				prop_name, len / MAX_BIT_OFFSET);
			return -E2BIG;
		}

		for (i = 0, j = 0; i < len; j++) {
			PROP_BITVALUE_ACCESS(prop_value, prop_index, j, 0) =
				be32_to_cpu(arr[i]);
			i++;
			PROP_BITVALUE_ACCESS(prop_value, prop_index, j, 1) =
				be32_to_cpu(arr[i]);
			i++;
		}
	} else {
		if (mandatory) {
			SDE_ERROR("error mandatory property '%s' not found\n",
				prop_name);
			rc = -EINVAL;
		} else {
			SDE_DEBUG("error optional property '%s' not found\n",
				prop_name);
		}
	}

	return rc;
}

static int _validate_dt_entry(struct device_node *np,
	struct sde_prop_type *sde_prop, u32 prop_size, int *prop_count,
	int *off_count)
{
	int rc = 0, i, val;
	struct device_node *snp = NULL;

	if (off_count) {
		*off_count = of_property_count_u32_elems(np,
				sde_prop[0].prop_name);
		if ((*off_count > MAX_BLOCKS) || (*off_count < 0)) {
			if (sde_prop[0].is_mandatory) {
				SDE_ERROR(
					"invalid hw offset prop name:%s count: %d\n",
					sde_prop[0].prop_name, *off_count);
				rc = -EINVAL;
			}
			*off_count = 0;
			memset(prop_count, 0, sizeof(int) * prop_size);
			return rc;
		}
	}

	for (i = 0; i < prop_size; i++) {
		switch (sde_prop[i].type) {
		case PROP_TYPE_U32:
			rc = of_property_read_u32(np, sde_prop[i].prop_name,
				&val);
			break;
		case PROP_TYPE_U32_ARRAY:
			prop_count[i] = of_property_count_u32_elems(np,
				sde_prop[i].prop_name);
			if (prop_count[i] < 0)
				rc = prop_count[i];
			break;
		case PROP_TYPE_STRING_ARRAY:
			prop_count[i] = of_property_count_strings(np,
				sde_prop[i].prop_name);
			if (prop_count[i] < 0)
				rc = prop_count[i];
			break;
		case PROP_TYPE_BIT_OFFSET_ARRAY:
			of_get_property(np, sde_prop[i].prop_name, &val);
			prop_count[i] = val / (MAX_BIT_OFFSET * sizeof(u32));
			break;
		case PROP_TYPE_NODE:
			snp = of_get_child_by_name(np,
					sde_prop[i].prop_name);
			if (!snp)
				rc = -EINVAL;
			break;
		default:
			SDE_DEBUG("invalid property type:%d\n",
							sde_prop[i].type);
			break;
		}
		SDE_DEBUG(
			"prop id:%d prop name:%s prop type:%d prop_count:%d\n",
			i, sde_prop[i].prop_name,
			sde_prop[i].type, prop_count[i]);

		if (rc && sde_prop[i].is_mandatory &&
		   ((sde_prop[i].type == PROP_TYPE_U32) ||
		    (sde_prop[i].type == PROP_TYPE_NODE))) {
			SDE_ERROR("prop:%s not present\n",
						sde_prop[i].prop_name);
			goto end;
		} else if (sde_prop[i].type == PROP_TYPE_U32 ||
			sde_prop[i].type == PROP_TYPE_BOOL ||
			sde_prop[i].type == PROP_TYPE_NODE) {
			rc = 0;
			continue;
		}

		if (off_count && (prop_count[i] != *off_count) &&
				sde_prop[i].is_mandatory) {
			SDE_ERROR(
				"prop:%s count:%d is different compared to offset array:%d\n",
				sde_prop[i].prop_name,
				prop_count[i], *off_count);
			rc = -EINVAL;
			goto end;
		} else if (off_count && prop_count[i] != *off_count) {
			SDE_DEBUG(
				"prop:%s count:%d is different compared to offset array:%d\n",
				sde_prop[i].prop_name,
				prop_count[i], *off_count);
			rc = 0;
			prop_count[i] = 0;
		}
		if (prop_count[i] < 0) {
			prop_count[i] = 0;
			if (sde_prop[i].is_mandatory) {
				SDE_ERROR("prop:%s count:%d is negative\n",
					sde_prop[i].prop_name, prop_count[i]);
				rc = -EINVAL;
			} else {
				rc = 0;
				SDE_DEBUG("prop:%s count:%d is negative\n",
					sde_prop[i].prop_name, prop_count[i]);
			}
		}
	}

end:
	return rc;
}

static int _read_dt_entry(struct device_node *np,
	struct sde_prop_type *sde_prop, u32 prop_size, int *prop_count,
	bool *prop_exists,
	struct sde_prop_value *prop_value)
{
	int rc = 0, i, j;

	for (i = 0; i < prop_size; i++) {
		prop_exists[i] = true;
		switch (sde_prop[i].type) {
		case PROP_TYPE_U32:
			rc = of_property_read_u32(np, sde_prop[i].prop_name,
				&PROP_VALUE_ACCESS(prop_value, i, 0));
			SDE_DEBUG(
				"prop id:%d prop name:%s prop type:%d value:0x%x\n",
				i, sde_prop[i].prop_name,
				sde_prop[i].type,
				PROP_VALUE_ACCESS(prop_value, i, 0));
			if (rc)
				prop_exists[i] = false;
			break;
		case PROP_TYPE_BOOL:
			PROP_VALUE_ACCESS(prop_value, i, 0) =
				of_property_read_bool(np,
					sde_prop[i].prop_name);
			SDE_DEBUG(
				"prop id:%d prop name:%s prop type:%d value:0x%x\n",
				i, sde_prop[i].prop_name,
				sde_prop[i].type,
				PROP_VALUE_ACCESS(prop_value, i, 0));
			break;
		case PROP_TYPE_U32_ARRAY:
			rc = _parse_dt_u32_handler(np, sde_prop[i].prop_name,
				&PROP_VALUE_ACCESS(prop_value, i, 0),
				prop_count[i], sde_prop[i].is_mandatory);
			if (rc && sde_prop[i].is_mandatory) {
				SDE_ERROR(
					"%s prop validation success but read failed\n",
					sde_prop[i].prop_name);
				prop_exists[i] = false;
				goto end;
			} else {
				if (rc)
					prop_exists[i] = false;
				/* only for debug purpose */
				SDE_DEBUG("prop id:%d prop name:%s prop \"\
					type:%d", i, sde_prop[i].prop_name,
					sde_prop[i].type);
				for (j = 0; j < prop_count[i]; j++)
					SDE_DEBUG(" value[%d]:0x%x ", j,
						PROP_VALUE_ACCESS(prop_value, i,
								j));
				SDE_DEBUG("\n");
			}
			break;
		case PROP_TYPE_BIT_OFFSET_ARRAY:
			rc = _parse_dt_bit_offset(np, sde_prop[i].prop_name,
				prop_value, i, prop_count[i],
				sde_prop[i].is_mandatory);
			if (rc && sde_prop[i].is_mandatory) {
				SDE_ERROR(
					"%s prop validation success but read failed\n",
					sde_prop[i].prop_name);
				prop_exists[i] = false;
				goto end;
			} else {
				if (rc)
					prop_exists[i] = false;
				SDE_DEBUG(
					"prop id:%d prop name:%s prop type:%d",
					i, sde_prop[i].prop_name,
					sde_prop[i].type);
				for (j = 0; j < prop_count[i]; j++)
					SDE_DEBUG(
					"count[%d]: bit:0x%x off:0x%x\n", j,
					PROP_BITVALUE_ACCESS(prop_value,
						i, j, 0),
					PROP_BITVALUE_ACCESS(prop_value,
						i, j, 1));
				SDE_DEBUG("\n");
			}
			break;
		case PROP_TYPE_NODE:
			/* Node will be parsed in calling function */
			rc = 0;
			break;
		default:
			SDE_DEBUG("invalid property type:%d\n",
							sde_prop[i].type);
			break;
		}
		rc = 0;
	}

end:
	return rc;
}

static void _sde_sspp_setup_vig(struct sde_mdss_cfg *sde_cfg,
	struct sde_sspp_cfg *sspp, struct sde_sspp_sub_blks *sblk,
	bool *prop_exists, struct sde_prop_value *prop_value, u32 *vig_count)
{
	sblk->maxupscale = MAX_UPSCALE_RATIO;
	sblk->maxdwnscale = MAX_DOWNSCALE_RATIO;
	sspp->id = SSPP_VIG0 + *vig_count;
	snprintf(sspp->name, SDE_HW_BLK_NAME_LEN, "sspp_%u",
			sspp->id - SSPP_VIG0);
	sspp->clk_ctrl = SDE_CLK_CTRL_VIG0 + *vig_count;
	sspp->type = SSPP_TYPE_VIG;
	set_bit(SDE_SSPP_QOS, &sspp->features);
	if (sde_cfg->vbif_qos_nlvl == 8)
		set_bit(SDE_SSPP_QOS_8LVL, &sspp->features);
	(*vig_count)++;

	if (!prop_value)
		return;

	if (sde_cfg->qseed_type == SDE_SSPP_SCALER_QSEED2) {
		set_bit(SDE_SSPP_SCALER_QSEED2, &sspp->features);
		sblk->scaler_blk.id = SDE_SSPP_SCALER_QSEED2;
		sblk->scaler_blk.base = PROP_VALUE_ACCESS(prop_value,
			VIG_QSEED_OFF, 0);
		sblk->scaler_blk.len = PROP_VALUE_ACCESS(prop_value,
			VIG_QSEED_LEN, 0);
		snprintf(sblk->scaler_blk.name, SDE_HW_BLK_NAME_LEN,
				"sspp_scaler%u", sspp->id - SSPP_VIG0);
	} else if (sde_cfg->qseed_type == SDE_SSPP_SCALER_QSEED3) {
		set_bit(SDE_SSPP_SCALER_QSEED3, &sspp->features);
		sblk->scaler_blk.id = SDE_SSPP_SCALER_QSEED3;
		sblk->scaler_blk.base = PROP_VALUE_ACCESS(prop_value,
			VIG_QSEED_OFF, 0);
		sblk->scaler_blk.len = PROP_VALUE_ACCESS(prop_value,
			VIG_QSEED_LEN, 0);
		snprintf(sblk->scaler_blk.name, SDE_HW_BLK_NAME_LEN,
			"sspp_scaler%u", sspp->id - SSPP_VIG0);
	}

	if (sde_cfg->has_sbuf)
		set_bit(SDE_SSPP_SBUF, &sspp->features);

	sblk->csc_blk.id = SDE_SSPP_CSC;
	snprintf(sblk->csc_blk.name, SDE_HW_BLK_NAME_LEN,
			"sspp_csc%u", sspp->id - SSPP_VIG0);
	if (sde_cfg->csc_type == SDE_SSPP_CSC) {
		set_bit(SDE_SSPP_CSC, &sspp->features);
		sblk->csc_blk.base = PROP_VALUE_ACCESS(prop_value,
							VIG_CSC_OFF, 0);
	} else if (sde_cfg->csc_type == SDE_SSPP_CSC_10BIT) {
		set_bit(SDE_SSPP_CSC_10BIT, &sspp->features);
		sblk->csc_blk.base = PROP_VALUE_ACCESS(prop_value,
							VIG_CSC_OFF, 0);
	}

	sblk->hsic_blk.id = SDE_SSPP_HSIC;
	snprintf(sblk->hsic_blk.name, SDE_HW_BLK_NAME_LEN,
			"sspp_hsic%u", sspp->id - SSPP_VIG0);
	if (prop_exists[VIG_HSIC_PROP]) {
		sblk->hsic_blk.base = PROP_VALUE_ACCESS(prop_value,
			VIG_HSIC_PROP, 0);
		sblk->hsic_blk.version = PROP_VALUE_ACCESS(prop_value,
			VIG_HSIC_PROP, 1);
		sblk->hsic_blk.len = 0;
		set_bit(SDE_SSPP_HSIC, &sspp->features);
	}

	sblk->memcolor_blk.id = SDE_SSPP_MEMCOLOR;
	snprintf(sblk->memcolor_blk.name, SDE_HW_BLK_NAME_LEN,
			"sspp_memcolor%u", sspp->id - SSPP_VIG0);
	if (prop_exists[VIG_MEMCOLOR_PROP]) {
		sblk->memcolor_blk.base = PROP_VALUE_ACCESS(prop_value,
			VIG_MEMCOLOR_PROP, 0);
		sblk->memcolor_blk.version = PROP_VALUE_ACCESS(prop_value,
			VIG_MEMCOLOR_PROP, 1);
		sblk->memcolor_blk.len = 0;
		set_bit(SDE_SSPP_MEMCOLOR, &sspp->features);
	}

	sblk->pcc_blk.id = SDE_SSPP_PCC;
	snprintf(sblk->pcc_blk.name, SDE_HW_BLK_NAME_LEN,
			"sspp_pcc%u", sspp->id - SSPP_VIG0);
	if (prop_exists[VIG_PCC_PROP]) {
		sblk->pcc_blk.base = PROP_VALUE_ACCESS(prop_value,
			VIG_PCC_PROP, 0);
		sblk->pcc_blk.version = PROP_VALUE_ACCESS(prop_value,
			VIG_PCC_PROP, 1);
		sblk->pcc_blk.len = 0;
		set_bit(SDE_SSPP_PCC, &sspp->features);
	}

	if (prop_exists[VIG_GAMUT_PROP]) {
		sblk->gamut_blk.id = SDE_SSPP_VIG_GAMUT;
		snprintf(sblk->gamut_blk.name, SDE_HW_BLK_NAME_LEN,
			"sspp_vig_gamut%u", sspp->id - SSPP_VIG0);
		sblk->gamut_blk.base = PROP_VALUE_ACCESS(prop_value,
			VIG_GAMUT_PROP, 0);
		sblk->gamut_blk.version = PROP_VALUE_ACCESS(prop_value,
			VIG_GAMUT_PROP, 1);
		sblk->gamut_blk.len = 0;
		set_bit(SDE_SSPP_VIG_GAMUT, &sspp->features);
	}

	if (prop_exists[VIG_IGC_PROP]) {
		sblk->igc_blk[0].id = SDE_SSPP_VIG_IGC;
		snprintf(sblk->igc_blk[0].name, SDE_HW_BLK_NAME_LEN,
			"sspp_vig_igc%u", sspp->id - SSPP_VIG0);
		sblk->igc_blk[0].base = PROP_VALUE_ACCESS(prop_value,
			VIG_IGC_PROP, 0);
		sblk->igc_blk[0].version = PROP_VALUE_ACCESS(prop_value,
			VIG_IGC_PROP, 1);
		sblk->igc_blk[0].len = 0;
		set_bit(SDE_SSPP_VIG_IGC, &sspp->features);
	}

	if (PROP_VALUE_ACCESS(prop_value, VIG_INVERSE_PMA, 0))
		set_bit(SDE_SSPP_INVERSE_PMA, &sspp->features);

	sblk->format_list = sde_cfg->vig_formats;
	sblk->virt_format_list = sde_cfg->virt_vig_formats;
}

static void _sde_sspp_setup_rgb(struct sde_mdss_cfg *sde_cfg,
	struct sde_sspp_cfg *sspp, struct sde_sspp_sub_blks *sblk,
	bool *prop_exists, struct sde_prop_value *prop_value, u32 *rgb_count)
{
	sblk->maxupscale = MAX_UPSCALE_RATIO;
	sblk->maxdwnscale = MAX_DOWNSCALE_RATIO;
	sspp->id = SSPP_RGB0 + *rgb_count;
	snprintf(sspp->name, SDE_HW_BLK_NAME_LEN, "sspp_%u",
			sspp->id - SSPP_VIG0);
	sspp->clk_ctrl = SDE_CLK_CTRL_RGB0 + *rgb_count;
	sspp->type = SSPP_TYPE_RGB;
	set_bit(SDE_SSPP_QOS, &sspp->features);
	if (sde_cfg->vbif_qos_nlvl == 8)
		set_bit(SDE_SSPP_QOS_8LVL, &sspp->features);
	(*rgb_count)++;

	if (!prop_value)
		return;

	if (sde_cfg->qseed_type == SDE_SSPP_SCALER_QSEED2) {
		set_bit(SDE_SSPP_SCALER_RGB, &sspp->features);
		sblk->scaler_blk.id = SDE_SSPP_SCALER_QSEED2;
		sblk->scaler_blk.base = PROP_VALUE_ACCESS(prop_value,
			RGB_SCALER_OFF, 0);
		sblk->scaler_blk.len = PROP_VALUE_ACCESS(prop_value,
			RGB_SCALER_LEN, 0);
		snprintf(sblk->scaler_blk.name, SDE_HW_BLK_NAME_LEN,
			"sspp_scaler%u", sspp->id - SSPP_VIG0);
	} else if (sde_cfg->qseed_type == SDE_SSPP_SCALER_QSEED3) {
		set_bit(SDE_SSPP_SCALER_RGB, &sspp->features);
		sblk->scaler_blk.id = SDE_SSPP_SCALER_QSEED3;
		sblk->scaler_blk.base = PROP_VALUE_ACCESS(prop_value,
			RGB_SCALER_LEN, 0);
		sblk->scaler_blk.len = PROP_VALUE_ACCESS(prop_value,
			SSPP_SCALE_SIZE, 0);
		snprintf(sblk->scaler_blk.name, SDE_HW_BLK_NAME_LEN,
			"sspp_scaler%u", sspp->id - SSPP_VIG0);
	}

	sblk->pcc_blk.id = SDE_SSPP_PCC;
	if (prop_exists[RGB_PCC_PROP]) {
		sblk->pcc_blk.base = PROP_VALUE_ACCESS(prop_value,
			RGB_PCC_PROP, 0);
		sblk->pcc_blk.version = PROP_VALUE_ACCESS(prop_value,
			RGB_PCC_PROP, 1);
		sblk->pcc_blk.len = 0;
		set_bit(SDE_SSPP_PCC, &sspp->features);
	}

	sblk->format_list = sde_cfg->dma_formats;
	sblk->virt_format_list = NULL;
}

static void _sde_sspp_setup_cursor(struct sde_mdss_cfg *sde_cfg,
	struct sde_sspp_cfg *sspp, struct sde_sspp_sub_blks *sblk,
	struct sde_prop_value *prop_value, u32 *cursor_count)
{
	if (!IS_SDE_MAJOR_MINOR_SAME(sde_cfg->hwversion, SDE_HW_VER_300))
		SDE_ERROR("invalid sspp type %d, xin id %d\n",
				sspp->type, sspp->xin_id);
	set_bit(SDE_SSPP_CURSOR, &sspp->features);
	sblk->maxupscale = SSPP_UNITY_SCALE;
	sblk->maxdwnscale = SSPP_UNITY_SCALE;
	sblk->format_list = sde_cfg->cursor_formats;
	sblk->virt_format_list = NULL;
	sspp->id = SSPP_CURSOR0 + *cursor_count;
	snprintf(sspp->name, SDE_HW_BLK_NAME_LEN, "sspp_%u",
			sspp->id - SSPP_VIG0);
	sspp->clk_ctrl = SDE_CLK_CTRL_CURSOR0 + *cursor_count;
	sspp->type = SSPP_TYPE_CURSOR;
	(*cursor_count)++;
}

static void _sde_sspp_setup_dma(struct sde_mdss_cfg *sde_cfg,
	struct sde_sspp_cfg *sspp, struct sde_sspp_sub_blks *sblk,
	bool prop_exists[][DMA_PROP_MAX], struct sde_prop_value *prop_value,
	u32 *dma_count, u32 dgm_count)
{
	u32 i = 0;

	sblk->maxupscale = SSPP_UNITY_SCALE;
	sblk->maxdwnscale = SSPP_UNITY_SCALE;
	sblk->format_list = sde_cfg->dma_formats;
	sblk->virt_format_list = sde_cfg->dma_formats;
	sspp->id = SSPP_DMA0 + *dma_count;
	sspp->clk_ctrl = SDE_CLK_CTRL_DMA0 + *dma_count;
	snprintf(sspp->name, SDE_HW_BLK_NAME_LEN, "sspp_%u",
			sspp->id - SSPP_VIG0);
	sspp->type = SSPP_TYPE_DMA;
	set_bit(SDE_SSPP_QOS, &sspp->features);
	if (sde_cfg->vbif_qos_nlvl == 8)
		set_bit(SDE_SSPP_QOS_8LVL, &sspp->features);
	(*dma_count)++;

	if (!prop_value)
		return;

	sblk->num_igc_blk = dgm_count;
	sblk->num_gc_blk = dgm_count;
	sblk->num_dgm_csc_blk = dgm_count;
	for (i = 0; i < dgm_count; i++) {
		if (prop_exists[i][DMA_IGC_PROP]) {
			sblk->igc_blk[i].id = SDE_SSPP_DMA_IGC;
			snprintf(sblk->igc_blk[i].name, SDE_HW_BLK_NAME_LEN,
				"sspp_dma_igc%u", sspp->id - SSPP_DMA0);
			sblk->igc_blk[i].base = PROP_VALUE_ACCESS(
				&prop_value[i * DMA_PROP_MAX], DMA_IGC_PROP, 0);
			sblk->igc_blk[i].version = PROP_VALUE_ACCESS(
				&prop_value[i * DMA_PROP_MAX], DMA_IGC_PROP, 1);
			sblk->igc_blk[i].len = 0;
			set_bit(SDE_SSPP_DMA_IGC, &sspp->features);
		}

		if (prop_exists[i][DMA_GC_PROP]) {
			sblk->gc_blk[i].id = SDE_SSPP_DMA_GC;
			snprintf(sblk->gc_blk[0].name, SDE_HW_BLK_NAME_LEN,
				"sspp_dma_gc%u", sspp->id - SSPP_DMA0);
			sblk->gc_blk[i].base = PROP_VALUE_ACCESS(
				&prop_value[i * DMA_PROP_MAX], DMA_GC_PROP, 0);
			sblk->gc_blk[i].version = PROP_VALUE_ACCESS(
				&prop_value[i * DMA_PROP_MAX], DMA_GC_PROP, 1);
			sblk->gc_blk[i].len = 0;
			set_bit(SDE_SSPP_DMA_GC, &sspp->features);
		}

		if (PROP_VALUE_ACCESS(&prop_value[i * DMA_PROP_MAX],
			DMA_DGM_INVERSE_PMA, 0))
			set_bit(SDE_SSPP_DGM_INVERSE_PMA, &sspp->features);

		if (prop_exists[i][DMA_CSC_OFF]) {
			sblk->dgm_csc_blk[i].id = SDE_SSPP_DGM_CSC;
			snprintf(sblk->csc_blk.name, SDE_HW_BLK_NAME_LEN,
				"sspp_dgm_csc%u", sspp->id - SSPP_DMA0);
			set_bit(SDE_SSPP_DGM_CSC, &sspp->features);
			sblk->dgm_csc_blk[i].base = PROP_VALUE_ACCESS(
				&prop_value[i * DMA_PROP_MAX], DMA_CSC_OFF, 0);
		}
	}
}

static int sde_dgm_parse_dt(struct device_node *np, u32 index,
	struct sde_prop_value *prop_value, bool *prop_exists)
{
	int rc = 0;
	u32 child_idx = 0;
	int prop_count[DMA_PROP_MAX] = {0};
	struct device_node *dgm_snp = NULL;

	for_each_child_of_node(np, dgm_snp) {
		if (index != child_idx++)
			continue;
		rc = _validate_dt_entry(dgm_snp, dma_prop, ARRAY_SIZE(dma_prop),
				prop_count, NULL);
		if (rc)
			return rc;
		rc = _read_dt_entry(dgm_snp, dma_prop, ARRAY_SIZE(dma_prop),
				prop_count, prop_exists,
				prop_value);
	}

	return rc;
}

static int sde_sspp_parse_dt(struct device_node *np,
	struct sde_mdss_cfg *sde_cfg)
{
	int rc, prop_count[SSPP_PROP_MAX], off_count, i, j;
	int vig_prop_count[VIG_PROP_MAX], rgb_prop_count[RGB_PROP_MAX];
	bool prop_exists[SSPP_PROP_MAX], vig_prop_exists[VIG_PROP_MAX];
	bool rgb_prop_exists[RGB_PROP_MAX];
	bool dgm_prop_exists[SSPP_SUBBLK_COUNT_MAX][DMA_PROP_MAX];
	struct sde_prop_value *prop_value = NULL;
	struct sde_prop_value *vig_prop_value = NULL, *rgb_prop_value = NULL;
	struct sde_prop_value *dgm_prop_value = NULL;
	const char *type;
	struct sde_sspp_cfg *sspp;
	struct sde_sspp_sub_blks *sblk;
	u32 vig_count = 0, dma_count = 0, rgb_count = 0, cursor_count = 0;
	u32 dgm_count = 0;
	struct device_node *snp = NULL;

	prop_value = kcalloc(SSPP_PROP_MAX,
			sizeof(struct sde_prop_value), GFP_KERNEL);
	if (!prop_value) {
		rc = -ENOMEM;
		goto end;
	}

	rc = _validate_dt_entry(np, sspp_prop, ARRAY_SIZE(sspp_prop),
		prop_count, &off_count);
	if (rc)
		goto end;

	rc = _read_dt_entry(np, sspp_prop, ARRAY_SIZE(sspp_prop), prop_count,
					prop_exists, prop_value);
	if (rc)
		goto end;

	sde_cfg->sspp_count = off_count;

	/* get vig feature dt properties if they exist */
	snp = of_get_child_by_name(np, sspp_prop[SSPP_VIG_BLOCKS].prop_name);
	if (snp) {
		vig_prop_value = kcalloc(VIG_PROP_MAX,
			sizeof(struct sde_prop_value), GFP_KERNEL);
		if (!vig_prop_value) {
			rc = -ENOMEM;
			goto end;
		}
		rc = _validate_dt_entry(snp, vig_prop, ARRAY_SIZE(vig_prop),
			vig_prop_count, NULL);
		if (rc)
			goto end;
		rc = _read_dt_entry(snp, vig_prop, ARRAY_SIZE(vig_prop),
				vig_prop_count, vig_prop_exists,
				vig_prop_value);
	}

	/* get rgb feature dt properties if they exist */
	snp = of_get_child_by_name(np, sspp_prop[SSPP_RGB_BLOCKS].prop_name);
	if (snp) {
		rgb_prop_value = kcalloc(RGB_PROP_MAX,
					sizeof(struct sde_prop_value),
					GFP_KERNEL);
		if (!rgb_prop_value) {
			rc = -ENOMEM;
			goto end;
		}
		rc = _validate_dt_entry(snp, rgb_prop, ARRAY_SIZE(rgb_prop),
			rgb_prop_count, NULL);
		if (rc)
			goto end;
		rc = _read_dt_entry(snp, rgb_prop, ARRAY_SIZE(rgb_prop),
				rgb_prop_count, rgb_prop_exists,
				rgb_prop_value);
	}

	/* get dma feature dt properties if they exist */
	snp = of_get_child_by_name(np, sspp_prop[SSPP_DMA_BLOCKS].prop_name);
	if (snp) {
		dgm_count = of_get_child_count(snp);
		if (dgm_count > 0 && dgm_count <= SSPP_SUBBLK_COUNT_MAX) {
			dgm_prop_value = kzalloc(dgm_count * DMA_PROP_MAX *
					sizeof(struct sde_prop_value),
					GFP_KERNEL);
			if (!dgm_prop_value) {
				rc = -ENOMEM;
				goto end;
			}
			for (i = 0; i < dgm_count; i++)
				sde_dgm_parse_dt(snp, i,
					&dgm_prop_value[i * DMA_PROP_MAX],
					&dgm_prop_exists[i][0]);
		}
	}

	for (i = 0; i < off_count; i++) {
		sspp = sde_cfg->sspp + i;
		sblk = kzalloc(sizeof(*sblk), GFP_KERNEL);
		if (!sblk) {
			rc = -ENOMEM;
			/* catalog deinit will release the allocated blocks */
			goto end;
		}
		sspp->sblk = sblk;

		sspp->base = PROP_VALUE_ACCESS(prop_value, SSPP_OFF, i);
		sspp->len = PROP_VALUE_ACCESS(prop_value, SSPP_SIZE, 0);
		sblk->maxlinewidth = sde_cfg->max_sspp_linewidth;

		set_bit(SDE_SSPP_SRC, &sspp->features);

		if (sde_cfg->has_cdp)
			set_bit(SDE_SSPP_CDP, &sspp->features);

		if (sde_cfg->ts_prefill_rev == 1) {
			set_bit(SDE_SSPP_TS_PREFILL, &sspp->features);
		} else if (sde_cfg->ts_prefill_rev == 2) {
			set_bit(SDE_SSPP_TS_PREFILL, &sspp->features);
			set_bit(SDE_SSPP_TS_PREFILL_REC1, &sspp->features);
		}

		sblk->smart_dma_priority =
			PROP_VALUE_ACCESS(prop_value, SSPP_SMART_DMA, i);

		if (sblk->smart_dma_priority && sde_cfg->smart_dma_rev)
			set_bit(sde_cfg->smart_dma_rev, &sspp->features);

		sblk->src_blk.id = SDE_SSPP_SRC;

		of_property_read_string_index(np,
				sspp_prop[SSPP_TYPE].prop_name, i, &type);
		if (!strcmp(type, "vig")) {
			_sde_sspp_setup_vig(sde_cfg, sspp, sblk,
				vig_prop_exists, vig_prop_value, &vig_count);
		} else if (!strcmp(type, "rgb")) {
			_sde_sspp_setup_rgb(sde_cfg, sspp, sblk,
				rgb_prop_exists, rgb_prop_value, &rgb_count);
		} else if (!strcmp(type, "cursor")) {
			/* No prop values for cursor pipes */
			_sde_sspp_setup_cursor(sde_cfg, sspp, sblk, NULL,
								&cursor_count);
		} else if (!strcmp(type, "dma")) {
			_sde_sspp_setup_dma(sde_cfg, sspp, sblk,
				dgm_prop_exists, dgm_prop_value, &dma_count,
				dgm_count);
		} else {
			SDE_ERROR("invalid sspp type:%s\n", type);
			rc = -EINVAL;
			goto end;
		}

		snprintf(sblk->src_blk.name, SDE_HW_BLK_NAME_LEN, "sspp_src_%u",
				sspp->id - SSPP_VIG0);

		if (sspp->clk_ctrl >= SDE_CLK_CTRL_MAX) {
			SDE_ERROR("%s: invalid clk ctrl: %d\n",
					sblk->src_blk.name, sspp->clk_ctrl);
			rc = -EINVAL;
			goto end;
		}

		sblk->maxhdeciexp = MAX_HORZ_DECIMATION;
		sblk->maxvdeciexp = MAX_VERT_DECIMATION;

		sspp->xin_id = PROP_VALUE_ACCESS(prop_value, SSPP_XIN, i);
		sblk->pixel_ram_size = DEFAULT_PIXEL_RAM_SIZE;
		sblk->src_blk.len = PROP_VALUE_ACCESS(prop_value, SSPP_SIZE, 0);

		if (PROP_VALUE_ACCESS(prop_value, SSPP_EXCL_RECT, i) == 1)
			set_bit(SDE_SSPP_EXCL_RECT, &sspp->features);

		if (prop_exists[SSPP_MAX_PER_PIPE_BW])
			sblk->max_per_pipe_bw = PROP_VALUE_ACCESS(prop_value,
					SSPP_MAX_PER_PIPE_BW, i);
		else
			sblk->max_per_pipe_bw = DEFAULT_MAX_PER_PIPE_BW;

		for (j = 0; j < sde_cfg->mdp_count; j++) {
			sde_cfg->mdp[j].clk_ctrls[sspp->clk_ctrl].reg_off =
				PROP_BITVALUE_ACCESS(prop_value,
						SSPP_CLK_CTRL, i, 0);
			sde_cfg->mdp[j].clk_ctrls[sspp->clk_ctrl].bit_off =
				PROP_BITVALUE_ACCESS(prop_value,
						SSPP_CLK_CTRL, i, 1);
		}

		SDE_DEBUG(
			"xin:%d ram:%d clk%d:%x/%d\n",
			sspp->xin_id,
			sblk->pixel_ram_size,
			sspp->clk_ctrl,
			sde_cfg->mdp[0].clk_ctrls[sspp->clk_ctrl].reg_off,
			sde_cfg->mdp[0].clk_ctrls[sspp->clk_ctrl].bit_off);
	}

end:
	kfree(prop_value);
	kfree(vig_prop_value);
	kfree(rgb_prop_value);
	kfree(dgm_prop_value);
	return rc;
}

static int sde_ctl_parse_dt(struct device_node *np,
		struct sde_mdss_cfg *sde_cfg)
{
	int rc, prop_count[HW_PROP_MAX], i;
	bool prop_exists[HW_PROP_MAX];
	struct sde_prop_value *prop_value = NULL;
	struct sde_ctl_cfg *ctl;
	u32 off_count;

	if (!sde_cfg) {
		SDE_ERROR("invalid argument input param\n");
		rc = -EINVAL;
		goto end;
	}

	prop_value = kzalloc(HW_PROP_MAX *
			sizeof(struct sde_prop_value), GFP_KERNEL);
	if (!prop_value) {
		rc = -ENOMEM;
		goto end;
	}

	rc = _validate_dt_entry(np, ctl_prop, ARRAY_SIZE(ctl_prop), prop_count,
		&off_count);
	if (rc)
		goto end;

	sde_cfg->ctl_count = off_count;

	rc = _read_dt_entry(np, ctl_prop, ARRAY_SIZE(ctl_prop), prop_count,
		prop_exists, prop_value);
	if (rc)
		goto end;

	for (i = 0; i < off_count; i++) {
		const char *disp_pref = NULL;

		ctl = sde_cfg->ctl + i;
		ctl->base = PROP_VALUE_ACCESS(prop_value, HW_OFF, i);
		ctl->len = PROP_VALUE_ACCESS(prop_value, HW_LEN, 0);
		ctl->id = CTL_0 + i;
		snprintf(ctl->name, SDE_HW_BLK_NAME_LEN, "ctl_%u",
				ctl->id - CTL_0);

		of_property_read_string_index(np,
				ctl_prop[HW_DISP].prop_name, i, &disp_pref);
		if (disp_pref && !strcmp(disp_pref, "primary"))
			set_bit(SDE_CTL_PRIMARY_PREF, &ctl->features);
		if (i < MAX_SPLIT_DISPLAY_CTL)
			set_bit(SDE_CTL_SPLIT_DISPLAY, &ctl->features);
		if (i < MAX_PP_SPLIT_DISPLAY_CTL)
			set_bit(SDE_CTL_PINGPONG_SPLIT, &ctl->features);
		if (sde_cfg->has_sbuf)
			set_bit(SDE_CTL_SBUF, &ctl->features);
		if (IS_SDE_CTL_REV_100(sde_cfg->ctl_rev))
			set_bit(SDE_CTL_ACTIVE_CFG, &ctl->features);
	}
end:
	kfree(prop_value);
	return rc;
}

static int sde_mixer_parse_dt(struct device_node *np,
						struct sde_mdss_cfg *sde_cfg)
{
	int rc, prop_count[MIXER_PROP_MAX], i, j;
	int blocks_prop_count[MIXER_BLOCKS_PROP_MAX];
	int blend_prop_count[MIXER_BLEND_PROP_MAX];
	bool prop_exists[MIXER_PROP_MAX];
	bool blocks_prop_exists[MIXER_BLOCKS_PROP_MAX];
	bool blend_prop_exists[MIXER_BLEND_PROP_MAX];
	struct sde_prop_value *prop_value = NULL, *blocks_prop_value = NULL;
	struct sde_prop_value *blend_prop_value = NULL;
	u32 off_count, blend_off_count, max_blendstages, lm_pair_mask;
	struct sde_lm_cfg *mixer;
	struct sde_lm_sub_blks *sblk;
	int pp_count, dspp_count, ds_count, mixer_count;
	u32 pp_idx, dspp_idx, ds_idx;
	u32 mixer_base;
	struct device_node *snp = NULL;

	if (!sde_cfg) {
		SDE_ERROR("invalid argument input param\n");
		rc = -EINVAL;
		goto end;
	}
	max_blendstages = sde_cfg->max_mixer_blendstages;

	prop_value = kcalloc(MIXER_PROP_MAX,
			sizeof(struct sde_prop_value), GFP_KERNEL);
	if (!prop_value) {
		rc = -ENOMEM;
		goto end;
	}

	rc = _validate_dt_entry(np, mixer_prop, ARRAY_SIZE(mixer_prop),
		prop_count, &off_count);
	if (rc)
		goto end;

	rc = _read_dt_entry(np, mixer_prop, ARRAY_SIZE(mixer_prop), prop_count,
		prop_exists, prop_value);
	if (rc)
		goto end;

	pp_count = sde_cfg->pingpong_count;
	dspp_count = sde_cfg->dspp_count;
	ds_count = sde_cfg->ds_count;

	/* get mixer feature dt properties if they exist */
	snp = of_get_child_by_name(np, mixer_prop[MIXER_BLOCKS].prop_name);
	if (snp) {
		blocks_prop_value = kzalloc(MIXER_BLOCKS_PROP_MAX *
				MAX_SDE_HW_BLK * sizeof(struct sde_prop_value),
				GFP_KERNEL);
		if (!blocks_prop_value) {
			rc = -ENOMEM;
			goto end;
		}
		rc = _validate_dt_entry(snp, mixer_blocks_prop,
			ARRAY_SIZE(mixer_blocks_prop), blocks_prop_count, NULL);
		if (rc)
			goto end;
		rc = _read_dt_entry(snp, mixer_blocks_prop,
				ARRAY_SIZE(mixer_blocks_prop),
				blocks_prop_count, blocks_prop_exists,
				blocks_prop_value);
	}

	/* get the blend_op register offsets */
	blend_prop_value = kzalloc(MIXER_BLEND_PROP_MAX *
			sizeof(struct sde_prop_value), GFP_KERNEL);
	if (!blend_prop_value) {
		rc = -ENOMEM;
		goto end;
	}
	rc = _validate_dt_entry(np, mixer_blend_prop,
		ARRAY_SIZE(mixer_blend_prop), blend_prop_count,
		&blend_off_count);
	if (rc)
		goto end;

	rc = _read_dt_entry(np, mixer_blend_prop, ARRAY_SIZE(mixer_blend_prop),
		blend_prop_count, blend_prop_exists, blend_prop_value);
	if (rc)
		goto end;

	for (i = 0, mixer_count = 0, pp_idx = 0, dspp_idx = 0,
			ds_idx = 0; i < off_count; i++) {
		const char *disp_pref = NULL;

		mixer_base = PROP_VALUE_ACCESS(prop_value, MIXER_OFF, i);
		if (!mixer_base)
			continue;

		mixer = sde_cfg->mixer + mixer_count;

		sblk = kzalloc(sizeof(*sblk), GFP_KERNEL);
		if (!sblk) {
			rc = -ENOMEM;
			/* catalog deinit will release the allocated blocks */
			goto end;
		}
		mixer->sblk = sblk;

		mixer->base = mixer_base;
		mixer->len = PROP_VALUE_ACCESS(prop_value, MIXER_LEN, 0);
		mixer->id = LM_0 + i;
		snprintf(mixer->name, SDE_HW_BLK_NAME_LEN, "lm_%u",
				mixer->id - LM_0);

		if (!prop_exists[MIXER_LEN])
			mixer->len = DEFAULT_SDE_HW_BLOCK_LEN;

		lm_pair_mask = PROP_VALUE_ACCESS(prop_value,
				MIXER_PAIR_MASK, i);
		if (lm_pair_mask)
			mixer->lm_pair_mask = 1 << lm_pair_mask;

		sblk->maxblendstages = max_blendstages;
		sblk->maxwidth = sde_cfg->max_mixer_width;

		for (j = 0; j < blend_off_count; j++)
			sblk->blendstage_base[j] =
				PROP_VALUE_ACCESS(blend_prop_value,
						MIXER_BLEND_OP_OFF, j);

		if (sde_cfg->has_src_split)
			set_bit(SDE_MIXER_SOURCESPLIT, &mixer->features);
		if (sde_cfg->has_dim_layer)
			set_bit(SDE_DIM_LAYER, &mixer->features);

		of_property_read_string_index(np,
			mixer_prop[MIXER_DISP].prop_name, i, &disp_pref);
		if (disp_pref && !strcmp(disp_pref, "primary"))
			set_bit(SDE_DISP_PRIMARY_PREF, &mixer->features);

		mixer->pingpong = pp_count > 0 ? pp_idx + PINGPONG_0
							: PINGPONG_MAX;
		mixer->dspp = dspp_count > 0 ? dspp_idx + DSPP_0
							: DSPP_MAX;
		mixer->ds = ds_count > 0 ? ds_idx + DS_0 : DS_MAX;
		pp_count--;
		dspp_count--;
		ds_count--;
		pp_idx++;
		dspp_idx++;
		ds_idx++;

		mixer_count++;

		sblk->gc.id = SDE_MIXER_GC;
		if (blocks_prop_value && blocks_prop_exists[MIXER_GC_PROP]) {
			sblk->gc.base = PROP_VALUE_ACCESS(blocks_prop_value,
					MIXER_GC_PROP, 0);
			sblk->gc.version = PROP_VALUE_ACCESS(blocks_prop_value,
					MIXER_GC_PROP, 1);
			sblk->gc.len = 0;
			set_bit(SDE_MIXER_GC, &mixer->features);
		}
	}
	sde_cfg->mixer_count = mixer_count;

end:
	kfree(prop_value);
	kfree(blocks_prop_value);
	kfree(blend_prop_value);
	return rc;
}

static int sde_intf_parse_dt(struct device_node *np,
						struct sde_mdss_cfg *sde_cfg)
{
	int rc, prop_count[INTF_PROP_MAX], i;
	struct sde_prop_value *prop_value = NULL;
	bool prop_exists[INTF_PROP_MAX];
	u32 off_count;
	u32 dsi_count = 0, none_count = 0, hdmi_count = 0, dp_count = 0;
	const char *type;
	struct sde_intf_cfg *intf;

	if (!sde_cfg) {
		SDE_ERROR("invalid argument\n");
		rc = -EINVAL;
		goto end;
	}

	prop_value = kzalloc(INTF_PROP_MAX *
			sizeof(struct sde_prop_value), GFP_KERNEL);
	if (!prop_value) {
		rc = -ENOMEM;
		goto end;
	}

	rc = _validate_dt_entry(np, intf_prop, ARRAY_SIZE(intf_prop),
		prop_count, &off_count);
	if (rc)
		goto end;

	sde_cfg->intf_count = off_count;

	rc = _read_dt_entry(np, intf_prop, ARRAY_SIZE(intf_prop), prop_count,
		prop_exists, prop_value);
	if (rc)
		goto end;

	for (i = 0; i < off_count; i++) {
		intf = sde_cfg->intf + i;
		intf->base = PROP_VALUE_ACCESS(prop_value, INTF_OFF, i);
		intf->len = PROP_VALUE_ACCESS(prop_value, INTF_LEN, 0);
		intf->id = INTF_0 + i;
		snprintf(intf->name, SDE_HW_BLK_NAME_LEN, "intf_%u",
				intf->id - INTF_0);

		if (!prop_exists[INTF_LEN])
			intf->len = DEFAULT_SDE_HW_BLOCK_LEN;

		intf->prog_fetch_lines_worst_case =
				!prop_exists[INTF_PREFETCH] ?
				sde_cfg->perf.min_prefill_lines :
				PROP_VALUE_ACCESS(prop_value, INTF_PREFETCH, i);

		of_property_read_string_index(np,
				intf_prop[INTF_TYPE].prop_name, i, &type);
		if (!strcmp(type, "dsi")) {
			intf->type = INTF_DSI;
			intf->controller_id = dsi_count;
			dsi_count++;
		} else if (!strcmp(type, "hdmi")) {
			intf->type = INTF_HDMI;
			intf->controller_id = hdmi_count;
			hdmi_count++;
		} else if (!strcmp(type, "dp")) {
			intf->type = INTF_DP;
			intf->controller_id = dp_count;
			dp_count++;
		} else {
			intf->type = INTF_NONE;
			intf->controller_id = none_count;
			none_count++;
		}

		if (sde_cfg->has_sbuf)
			set_bit(SDE_INTF_ROT_START, &intf->features);
		if (IS_SDE_CTL_REV_100(sde_cfg->ctl_rev))
			set_bit(SDE_INTF_INPUT_CTRL, &intf->features);

		if (IS_SDE_MAJOR_SAME((sde_cfg->hwversion),
				SDE_HW_VER_500))
			set_bit(SDE_INTF_TE, &intf->features);
	}

end:
	kfree(prop_value);
	return rc;
}

static int sde_wb_parse_dt(struct device_node *np, struct sde_mdss_cfg *sde_cfg)
{
	int rc, prop_count[WB_PROP_MAX], i, j;
	struct sde_prop_value *prop_value = NULL;
	bool prop_exists[WB_PROP_MAX];
	u32 off_count;
	struct sde_wb_cfg *wb;
	struct sde_wb_sub_blocks *sblk;

	if (!sde_cfg) {
		SDE_ERROR("invalid argument\n");
		rc = -EINVAL;
		goto end;
	}

	prop_value = kzalloc(WB_PROP_MAX *
			sizeof(struct sde_prop_value), GFP_KERNEL);
	if (!prop_value) {
		rc = -ENOMEM;
		goto end;
	}

	rc = _validate_dt_entry(np, wb_prop, ARRAY_SIZE(wb_prop), prop_count,
		&off_count);
	if (rc)
		goto end;

	sde_cfg->wb_count = off_count;

	rc = _read_dt_entry(np, wb_prop, ARRAY_SIZE(wb_prop), prop_count,
		prop_exists, prop_value);
	if (rc)
		goto end;

	for (i = 0; i < off_count; i++) {
		wb = sde_cfg->wb + i;
		sblk = kzalloc(sizeof(*sblk), GFP_KERNEL);
		if (!sblk) {
			rc = -ENOMEM;
			/* catalog deinit will release the allocated blocks */
			goto end;
		}
		wb->sblk = sblk;

		wb->base = PROP_VALUE_ACCESS(prop_value, WB_OFF, i);
		wb->id = WB_0 + PROP_VALUE_ACCESS(prop_value, WB_ID, i);
		snprintf(wb->name, SDE_HW_BLK_NAME_LEN, "wb_%u",
				wb->id - WB_0);
		wb->clk_ctrl = SDE_CLK_CTRL_WB0 +
			PROP_VALUE_ACCESS(prop_value, WB_ID, i);
		wb->xin_id = PROP_VALUE_ACCESS(prop_value, WB_XIN_ID, i);

		if (wb->clk_ctrl >= SDE_CLK_CTRL_MAX) {
			SDE_ERROR("%s: invalid clk ctrl: %d\n",
					wb->name, wb->clk_ctrl);
			rc = -EINVAL;
			goto end;
		}

		if (IS_SDE_MAJOR_MINOR_SAME((sde_cfg->hwversion),
				SDE_HW_VER_170))
			wb->vbif_idx = VBIF_NRT;
		else
			wb->vbif_idx = VBIF_RT;

		wb->len = PROP_VALUE_ACCESS(prop_value, WB_LEN, 0);
		if (!prop_exists[WB_LEN])
			wb->len = DEFAULT_SDE_HW_BLOCK_LEN;
		sblk->maxlinewidth = sde_cfg->max_wb_linewidth;

		if (wb->id >= LINE_MODE_WB_OFFSET)
			set_bit(SDE_WB_LINE_MODE, &wb->features);
		else
			set_bit(SDE_WB_BLOCK_MODE, &wb->features);
		set_bit(SDE_WB_TRAFFIC_SHAPER, &wb->features);
		set_bit(SDE_WB_YUV_CONFIG, &wb->features);

		if (sde_cfg->has_cdp)
			set_bit(SDE_WB_CDP, &wb->features);

		set_bit(SDE_WB_QOS, &wb->features);
		if (sde_cfg->vbif_qos_nlvl == 8)
			set_bit(SDE_WB_QOS_8LVL, &wb->features);

		if (sde_cfg->has_wb_ubwc)
			set_bit(SDE_WB_UBWC, &wb->features);

		set_bit(SDE_WB_XY_ROI_OFFSET, &wb->features);

		if (IS_SDE_CTL_REV_100(sde_cfg->ctl_rev))
			set_bit(SDE_WB_INPUT_CTRL, &wb->features);

		if (sde_cfg->has_cwb_support) {
			set_bit(SDE_WB_HAS_CWB, &wb->features);
			if (IS_SDE_CTL_REV_100(sde_cfg->ctl_rev))
				set_bit(SDE_WB_CWB_CTRL, &wb->features);
		}

		for (j = 0; j < sde_cfg->mdp_count; j++) {
			sde_cfg->mdp[j].clk_ctrls[wb->clk_ctrl].reg_off =
				PROP_BITVALUE_ACCESS(prop_value,
						WB_CLK_CTRL, i, 0);
			sde_cfg->mdp[j].clk_ctrls[wb->clk_ctrl].bit_off =
				PROP_BITVALUE_ACCESS(prop_value,
						WB_CLK_CTRL, i, 1);
		}

		wb->format_list = sde_cfg->wb_formats;

		SDE_DEBUG(
			"wb:%d xin:%d vbif:%d clk%d:%x/%d\n",
			wb->id - WB_0,
			wb->xin_id,
			wb->vbif_idx,
			wb->clk_ctrl,
			sde_cfg->mdp[0].clk_ctrls[wb->clk_ctrl].reg_off,
			sde_cfg->mdp[0].clk_ctrls[wb->clk_ctrl].bit_off);
	}

end:
	kfree(prop_value);
	return rc;
}

static void _sde_dspp_setup_blocks(struct sde_mdss_cfg *sde_cfg,
	struct sde_dspp_cfg *dspp, struct sde_dspp_sub_blks *sblk,
	bool *prop_exists, struct sde_prop_value *prop_value)
{
	sblk->igc.id = SDE_DSPP_IGC;
	if (prop_exists[DSPP_IGC_PROP]) {
		sblk->igc.base = PROP_VALUE_ACCESS(prop_value,
			DSPP_IGC_PROP, 0);
		sblk->igc.version = PROP_VALUE_ACCESS(prop_value,
			DSPP_IGC_PROP, 1);
		sblk->igc.len = 0;
		set_bit(SDE_DSPP_IGC, &dspp->features);
	}

	sblk->pcc.id = SDE_DSPP_PCC;
	if (prop_exists[DSPP_PCC_PROP]) {
		sblk->pcc.base = PROP_VALUE_ACCESS(prop_value,
			DSPP_PCC_PROP, 0);
		sblk->pcc.version = PROP_VALUE_ACCESS(prop_value,
			DSPP_PCC_PROP, 1);
		sblk->pcc.len = 0;
		set_bit(SDE_DSPP_PCC, &dspp->features);
	}

	sblk->gc.id = SDE_DSPP_GC;
	if (prop_exists[DSPP_GC_PROP]) {
		sblk->gc.base = PROP_VALUE_ACCESS(prop_value, DSPP_GC_PROP, 0);
		sblk->gc.version = PROP_VALUE_ACCESS(prop_value,
			DSPP_GC_PROP, 1);
		sblk->gc.len = 0;
		set_bit(SDE_DSPP_GC, &dspp->features);
	}

	sblk->gamut.id = SDE_DSPP_GAMUT;
	if (prop_exists[DSPP_GAMUT_PROP]) {
		sblk->gamut.base = PROP_VALUE_ACCESS(prop_value,
			DSPP_GAMUT_PROP, 0);
		sblk->gamut.version = PROP_VALUE_ACCESS(prop_value,
			DSPP_GAMUT_PROP, 1);
		sblk->gamut.len = 0;
		set_bit(SDE_DSPP_GAMUT, &dspp->features);
	}

	sblk->dither.id = SDE_DSPP_DITHER;
	if (prop_exists[DSPP_DITHER_PROP]) {
		sblk->dither.base = PROP_VALUE_ACCESS(prop_value,
			DSPP_DITHER_PROP, 0);
		sblk->dither.version = PROP_VALUE_ACCESS(prop_value,
			DSPP_DITHER_PROP, 1);
		sblk->dither.len = 0;
		set_bit(SDE_DSPP_DITHER, &dspp->features);
	}

	sblk->hist.id = SDE_DSPP_HIST;
	if (prop_exists[DSPP_HIST_PROP]) {
		sblk->hist.base = PROP_VALUE_ACCESS(prop_value,
			DSPP_HIST_PROP, 0);
		sblk->hist.version = PROP_VALUE_ACCESS(prop_value,
			DSPP_HIST_PROP, 1);
		sblk->hist.len = 0;
		set_bit(SDE_DSPP_HIST, &dspp->features);
	}

	sblk->hsic.id = SDE_DSPP_HSIC;
	if (prop_exists[DSPP_HSIC_PROP]) {
		sblk->hsic.base = PROP_VALUE_ACCESS(prop_value,
			DSPP_HSIC_PROP, 0);
		sblk->hsic.version = PROP_VALUE_ACCESS(prop_value,
			DSPP_HSIC_PROP, 1);
		sblk->hsic.len = 0;
		set_bit(SDE_DSPP_HSIC, &dspp->features);
	}

	sblk->memcolor.id = SDE_DSPP_MEMCOLOR;
	if (prop_exists[DSPP_MEMCOLOR_PROP]) {
		sblk->memcolor.base = PROP_VALUE_ACCESS(prop_value,
			DSPP_MEMCOLOR_PROP, 0);
		sblk->memcolor.version = PROP_VALUE_ACCESS(prop_value,
			DSPP_MEMCOLOR_PROP, 1);
		sblk->memcolor.len = 0;
		set_bit(SDE_DSPP_MEMCOLOR, &dspp->features);
	}

	sblk->sixzone.id = SDE_DSPP_SIXZONE;
	if (prop_exists[DSPP_SIXZONE_PROP]) {
		sblk->sixzone.base = PROP_VALUE_ACCESS(prop_value,
			DSPP_SIXZONE_PROP, 0);
		sblk->sixzone.version = PROP_VALUE_ACCESS(prop_value,
			DSPP_SIXZONE_PROP, 1);
		sblk->sixzone.len = 0;
		set_bit(SDE_DSPP_SIXZONE, &dspp->features);
	}

	sblk->vlut.id = SDE_DSPP_VLUT;
	if (prop_exists[DSPP_VLUT_PROP]) {
		sblk->vlut.base = PROP_VALUE_ACCESS(prop_value,
			DSPP_VLUT_PROP, 0);
		sblk->vlut.version = PROP_VALUE_ACCESS(prop_value,
			DSPP_VLUT_PROP, 1);
		sblk->sixzone.len = 0;
		set_bit(SDE_DSPP_VLUT, &dspp->features);
	}
}

static void _sde_inline_rot_parse_dt(struct device_node *np,
		struct sde_mdss_cfg *sde_cfg, struct sde_rot_cfg *rot)
{
	int rc, prop_count[INLINE_ROT_PROP_MAX], i, j, index;
	struct sde_prop_value *prop_value = NULL;
	bool prop_exists[INLINE_ROT_PROP_MAX];
	u32 off_count, sspp_count = 0, wb_count = 0;
	const char *type;

	prop_value = kcalloc(INLINE_ROT_PROP_MAX,
			sizeof(struct sde_prop_value), GFP_KERNEL);
	if (!prop_value)
		return;

	rc = _validate_dt_entry(np, inline_rot_prop,
			ARRAY_SIZE(inline_rot_prop), prop_count, &off_count);
	if (rc)
		goto end;

	rc = _read_dt_entry(np, inline_rot_prop, ARRAY_SIZE(inline_rot_prop),
			prop_count, prop_exists, prop_value);
	if (rc)
		goto end;

	for (i = 0; i < off_count; i++) {
		rot->vbif_cfg[i].xin_id = PROP_VALUE_ACCESS(prop_value,
							INLINE_ROT_XIN, i);
		of_property_read_string_index(np,
				inline_rot_prop[INLINE_ROT_XIN_TYPE].prop_name,
				i, &type);

		if (!strcmp(type, "sspp")) {
			rot->vbif_cfg[i].num = INLINE_ROT0_SSPP + sspp_count;
			rot->vbif_cfg[i].is_read = true;
			rot->vbif_cfg[i].clk_ctrl =
					SDE_CLK_CTRL_INLINE_ROT0_SSPP
					+ sspp_count;
			sspp_count++;
		} else if (!strcmp(type, "wb")) {
			rot->vbif_cfg[i].num = INLINE_ROT0_WB + wb_count;
			rot->vbif_cfg[i].is_read = false;
			rot->vbif_cfg[i].clk_ctrl =
					SDE_CLK_CTRL_INLINE_ROT0_WB
					+ wb_count;
			wb_count++;
		} else {
			SDE_ERROR("invalid rotator vbif type:%s\n", type);
			goto end;
		}

		index = rot->vbif_cfg[i].clk_ctrl;
		if (index < 0 || index >= SDE_CLK_CTRL_MAX) {
			SDE_ERROR("invalid clk_ctrl enum:%d\n", index);
			goto end;
		}

		for (j = 0; j < sde_cfg->mdp_count; j++) {
			sde_cfg->mdp[j].clk_ctrls[index].reg_off =
				PROP_BITVALUE_ACCESS(prop_value,
						INLINE_ROT_CLK_CTRL, i, 0);
			sde_cfg->mdp[j].clk_ctrls[index].bit_off =
				PROP_BITVALUE_ACCESS(prop_value,
						INLINE_ROT_CLK_CTRL, i, 1);
		}

		SDE_DEBUG("rot- xin:%d, num:%d, rd:%d, clk:%d:0x%x/%d\n",
				rot->vbif_cfg[i].xin_id,
				rot->vbif_cfg[i].num,
				rot->vbif_cfg[i].is_read,
				rot->vbif_cfg[i].clk_ctrl,
				sde_cfg->mdp[0].clk_ctrls[index].reg_off,
				sde_cfg->mdp[0].clk_ctrls[index].bit_off);
	}

	rot->vbif_idx = VBIF_RT;
	rot->xin_count = off_count;

end:
	kfree(prop_value);
}

static int sde_rot_parse_dt(struct device_node *np,
		struct sde_mdss_cfg *sde_cfg)
{
	struct sde_rot_cfg *rot;
	struct platform_device *pdev;
	struct of_phandle_args phargs;
	struct llcc_slice_desc *slice;
	int rc = 0, i;

	if (!sde_cfg) {
		SDE_ERROR("invalid argument\n");
		rc = -EINVAL;
		goto end;
	}

	for (i = 0; i < ROT_MAX; i++) {
		rot = sde_cfg->rot + sde_cfg->rot_count;
		rot->base = 0;
		rot->len = 0;

		rc = of_parse_phandle_with_args(np,
				"qcom,sde-inline-rotator", "#list-cells",
				i, &phargs);
		if (rc) {
			rc = 0;
			break;
		} else if (!phargs.np || !phargs.args_count) {
			rc = -EINVAL;
			break;
		}

		rot->id = ROT_0 + phargs.args[0];

		pdev = of_find_device_by_node(phargs.np);
		if (pdev) {
			slice = llcc_slice_getd(&pdev->dev, "rotator");
			if (IS_ERR_OR_NULL(slice)) {
				rot->pdev = NULL;
				SDE_ERROR("failed to get system cache %ld\n",
						PTR_ERR(slice));
			} else {
				rot->scid = llcc_get_slice_id(slice);
				rot->slice_size = llcc_get_slice_size(slice);
				rot->pdev = pdev;
				llcc_slice_putd(slice);
				SDE_DEBUG("rot:%d scid:%d slice_size:%zukb\n",
						rot->id, rot->scid,
						rot->slice_size);
				_sde_inline_rot_parse_dt(np, sde_cfg, rot);
				sde_cfg->rot_count++;
			}
		} else {
			rot->pdev = NULL;
			SDE_ERROR("invalid sde rotator node\n");
		}

		of_node_put(phargs.np);
	}

	if (sde_cfg->rot_count) {
		sde_cfg->has_sbuf = true;
		sde_cfg->sbuf_headroom = DEFAULT_SBUF_HEADROOM;
		sde_cfg->sbuf_prefill = DEFAULT_SBUF_PREFILL;
	}

end:
	return rc;
}

static int sde_dspp_top_parse_dt(struct device_node *np,
		struct sde_mdss_cfg *sde_cfg)
{
	int rc, prop_count[DSPP_TOP_PROP_MAX];
	bool prop_exists[DSPP_TOP_PROP_MAX];
	struct sde_prop_value *prop_value = NULL;
	u32 off_count;

	if (!sde_cfg) {
		SDE_ERROR("invalid argument\n");
		rc = -EINVAL;
		goto end;
	}

	prop_value = kzalloc(DSPP_TOP_PROP_MAX *
			sizeof(struct sde_prop_value), GFP_KERNEL);
	if (!prop_value) {
		rc = -ENOMEM;
		goto end;
	}

	rc = _validate_dt_entry(np, dspp_top_prop, ARRAY_SIZE(dspp_top_prop),
		prop_count, &off_count);
	if (rc)
		goto end;

	rc = _read_dt_entry(np, dspp_top_prop, ARRAY_SIZE(dspp_top_prop),
		prop_count, prop_exists, prop_value);
	if (rc)
		goto end;

	if (off_count != 1) {
		SDE_ERROR("invalid dspp_top off_count:%d\n", off_count);
		rc = -EINVAL;
		goto end;
	}

	sde_cfg->dspp_top.base =
		PROP_VALUE_ACCESS(prop_value, DSPP_TOP_OFF, 0);
	sde_cfg->dspp_top.len =
		PROP_VALUE_ACCESS(prop_value, DSPP_TOP_SIZE, 0);
	snprintf(sde_cfg->dspp_top.name, SDE_HW_BLK_NAME_LEN, "dspp_top");

end:
	kfree(prop_value);
	return rc;
}

static int sde_dspp_parse_dt(struct device_node *np,
						struct sde_mdss_cfg *sde_cfg)
{
	int rc, prop_count[DSPP_PROP_MAX], i;
	int ad_prop_count[AD_PROP_MAX];
	bool prop_exists[DSPP_PROP_MAX], ad_prop_exists[AD_PROP_MAX];
	bool blocks_prop_exists[DSPP_BLOCKS_PROP_MAX];
	struct sde_prop_value *ad_prop_value = NULL;
	int blocks_prop_count[DSPP_BLOCKS_PROP_MAX];
	struct sde_prop_value *prop_value = NULL, *blocks_prop_value = NULL;
	u32 off_count, ad_off_count;
	struct sde_dspp_cfg *dspp;
	struct sde_dspp_sub_blks *sblk;
	struct device_node *snp = NULL;

	if (!sde_cfg) {
		SDE_ERROR("invalid argument\n");
		rc = -EINVAL;
		goto end;
	}

	prop_value = kzalloc(DSPP_PROP_MAX *
			sizeof(struct sde_prop_value), GFP_KERNEL);
	if (!prop_value) {
		rc = -ENOMEM;
		goto end;
	}

	rc = _validate_dt_entry(np, dspp_prop, ARRAY_SIZE(dspp_prop),
		prop_count, &off_count);
	if (rc)
		goto end;

	sde_cfg->dspp_count = off_count;

	rc = _read_dt_entry(np, dspp_prop, ARRAY_SIZE(dspp_prop), prop_count,
		prop_exists, prop_value);
	if (rc)
		goto end;

	/* Parse AD dtsi entries */
	ad_prop_value = kcalloc(AD_PROP_MAX,
			sizeof(struct sde_prop_value), GFP_KERNEL);
	if (!ad_prop_value) {
		rc = -ENOMEM;
		goto end;
	}
	rc = _validate_dt_entry(np, ad_prop, ARRAY_SIZE(ad_prop),
		ad_prop_count, &ad_off_count);
	if (rc)
		goto end;
	rc = _read_dt_entry(np, ad_prop, ARRAY_SIZE(ad_prop), ad_prop_count,
		ad_prop_exists, ad_prop_value);
	if (rc)
		goto end;

	/* get DSPP feature dt properties if they exist */
	snp = of_get_child_by_name(np, dspp_prop[DSPP_BLOCKS].prop_name);
	if (snp) {
		blocks_prop_value = kzalloc(DSPP_BLOCKS_PROP_MAX *
				MAX_SDE_HW_BLK * sizeof(struct sde_prop_value),
				GFP_KERNEL);
		if (!blocks_prop_value) {
			rc = -ENOMEM;
			goto end;
		}
		rc = _validate_dt_entry(snp, dspp_blocks_prop,
			ARRAY_SIZE(dspp_blocks_prop), blocks_prop_count, NULL);
		if (rc)
			goto end;
		rc = _read_dt_entry(snp, dspp_blocks_prop,
			ARRAY_SIZE(dspp_blocks_prop), blocks_prop_count,
			blocks_prop_exists, blocks_prop_value);
		if (rc)
			goto end;
	}

	for (i = 0; i < off_count; i++) {
		dspp = sde_cfg->dspp + i;
		dspp->base = PROP_VALUE_ACCESS(prop_value, DSPP_OFF, i);
		dspp->len = PROP_VALUE_ACCESS(prop_value, DSPP_SIZE, 0);
		dspp->id = DSPP_0 + i;
		snprintf(dspp->name, SDE_HW_BLK_NAME_LEN, "dspp_%u",
				dspp->id - DSPP_0);

		sblk = kzalloc(sizeof(*sblk), GFP_KERNEL);
		if (!sblk) {
			rc = -ENOMEM;
			/* catalog deinit will release the allocated blocks */
			goto end;
		}
		dspp->sblk = sblk;

		if (blocks_prop_value)
			_sde_dspp_setup_blocks(sde_cfg, dspp, sblk,
					blocks_prop_exists, blocks_prop_value);

		sblk->ad.id = SDE_DSPP_AD;
		sde_cfg->ad_count = ad_off_count;
		if (ad_prop_value && (i < ad_off_count) &&
		    ad_prop_exists[AD_OFF]) {
			sblk->ad.base = PROP_VALUE_ACCESS(ad_prop_value,
				AD_OFF, i);
			sblk->ad.version = PROP_VALUE_ACCESS(ad_prop_value,
				AD_VERSION, 0);
			set_bit(SDE_DSPP_AD, &dspp->features);
		}
	}

end:
	kfree(prop_value);
	kfree(ad_prop_value);
	kfree(blocks_prop_value);
	return rc;
}

static int sde_ds_parse_dt(struct device_node *np,
			struct sde_mdss_cfg *sde_cfg)
{
	int rc, prop_count[DS_PROP_MAX], top_prop_count[DS_TOP_PROP_MAX], i;
	struct sde_prop_value *prop_value = NULL, *top_prop_value = NULL;
	bool prop_exists[DS_PROP_MAX], top_prop_exists[DS_TOP_PROP_MAX];
	u32 off_count = 0, top_off_count = 0;
	struct sde_ds_cfg *ds;
	struct sde_ds_top_cfg *ds_top = NULL;

	if (!sde_cfg) {
		SDE_ERROR("invalid argument\n");
		rc = -EINVAL;
		goto end;
	}

	if (!sde_cfg->mdp[0].has_dest_scaler) {
		SDE_DEBUG("dest scaler feature not supported\n");
		rc = 0;
		goto end;
	}

	/* Parse the dest scaler top register offset and capabilities */
	top_prop_value = kzalloc(DS_TOP_PROP_MAX *
			sizeof(struct sde_prop_value), GFP_KERNEL);
	if (!top_prop_value) {
		rc = -ENOMEM;
		goto end;
	}

	rc = _validate_dt_entry(np, ds_top_prop,
				ARRAY_SIZE(ds_top_prop),
				top_prop_count, &top_off_count);
	if (rc)
		goto end;

	rc = _read_dt_entry(np, ds_top_prop,
			ARRAY_SIZE(ds_top_prop), top_prop_count,
			top_prop_exists, top_prop_value);
	if (rc)
		goto end;

	/* Parse the offset of each dest scaler block */
	prop_value = kcalloc(DS_PROP_MAX,
			sizeof(struct sde_prop_value), GFP_KERNEL);
	if (!prop_value) {
		rc = -ENOMEM;
		goto end;
	}

	rc = _validate_dt_entry(np, ds_prop, ARRAY_SIZE(ds_prop), prop_count,
		&off_count);
	if (rc)
		goto end;

	sde_cfg->ds_count = off_count;

	rc = _read_dt_entry(np, ds_prop, ARRAY_SIZE(ds_prop), prop_count,
		prop_exists, prop_value);
	if (rc)
		goto end;

	if (!off_count)
		goto end;

	ds_top = kzalloc(sizeof(struct sde_ds_top_cfg), GFP_KERNEL);
	if (!ds_top) {
		rc = -ENOMEM;
		goto end;
	}

	ds_top->id = DS_TOP;
	snprintf(ds_top->name, SDE_HW_BLK_NAME_LEN, "ds_top_%u",
		ds_top->id - DS_TOP);
	ds_top->base = PROP_VALUE_ACCESS(top_prop_value, DS_TOP_OFF, 0);
	ds_top->len = PROP_VALUE_ACCESS(top_prop_value, DS_TOP_LEN, 0);
	ds_top->maxupscale = MAX_UPSCALE_RATIO;

	ds_top->maxinputwidth = PROP_VALUE_ACCESS(top_prop_value,
			DS_TOP_INPUT_LINEWIDTH, 0);
	if (!top_prop_exists[DS_TOP_INPUT_LINEWIDTH])
		ds_top->maxinputwidth = DEFAULT_SDE_LINE_WIDTH;

	ds_top->maxoutputwidth = PROP_VALUE_ACCESS(top_prop_value,
			DS_TOP_OUTPUT_LINEWIDTH, 0);
	if (!top_prop_exists[DS_TOP_OUTPUT_LINEWIDTH])
		ds_top->maxoutputwidth = DEFAULT_SDE_OUTPUT_LINE_WIDTH;

	for (i = 0; i < off_count; i++) {
		ds = sde_cfg->ds + i;
		ds->top = ds_top;
		ds->base = PROP_VALUE_ACCESS(prop_value, DS_OFF, i);
		ds->id = DS_0 + i;
		ds->len = PROP_VALUE_ACCESS(prop_value, DS_LEN, 0);
		snprintf(ds->name, SDE_HW_BLK_NAME_LEN, "ds_%u",
			ds->id - DS_0);

		if (!prop_exists[DS_LEN])
			ds->len = DEFAULT_SDE_HW_BLOCK_LEN;

		if (sde_cfg->qseed_type == SDE_SSPP_SCALER_QSEED3)
			set_bit(SDE_SSPP_SCALER_QSEED3, &ds->features);
	}

end:
	kfree(top_prop_value);
	kfree(prop_value);
	return rc;
};

static int sde_dsc_parse_dt(struct device_node *np,
			struct sde_mdss_cfg *sde_cfg)
{
	int rc, prop_count[MAX_BLOCKS], i;
	struct sde_prop_value *prop_value = NULL;
	bool prop_exists[DSC_PROP_MAX];
	u32 off_count;
	struct sde_dsc_cfg *dsc;

	if (!sde_cfg) {
		SDE_ERROR("invalid argument\n");
		rc = -EINVAL;
		goto end;
	}

	prop_value = kzalloc(DSC_PROP_MAX *
			sizeof(struct sde_prop_value), GFP_KERNEL);
	if (!prop_value) {
		rc = -ENOMEM;
		goto end;
	}

	rc = _validate_dt_entry(np, dsc_prop, ARRAY_SIZE(dsc_prop), prop_count,
		&off_count);
	if (rc)
		goto end;

	sde_cfg->dsc_count = off_count;

	rc = _read_dt_entry(np, dsc_prop, ARRAY_SIZE(dsc_prop), prop_count,
		prop_exists, prop_value);
	if (rc)
		goto end;

	for (i = 0; i < off_count; i++) {
		dsc = sde_cfg->dsc + i;
		dsc->base = PROP_VALUE_ACCESS(prop_value, DSC_OFF, i);
		dsc->id = DSC_0 + i;
		dsc->len = PROP_VALUE_ACCESS(prop_value, DSC_LEN, 0);
		snprintf(dsc->name, SDE_HW_BLK_NAME_LEN, "dsc_%u",
				dsc->id - DSC_0);

		if (!prop_exists[DSC_LEN])
			dsc->len = DEFAULT_SDE_HW_BLOCK_LEN;

		if (IS_SDE_CTL_REV_100(sde_cfg->ctl_rev))
			set_bit(SDE_DSC_OUTPUT_CTRL, &dsc->features);
	}

end:
	kfree(prop_value);
	return rc;
};

static int sde_cdm_parse_dt(struct device_node *np,
				struct sde_mdss_cfg *sde_cfg)
{
	int rc, prop_count[HW_PROP_MAX], i;
	struct sde_prop_value *prop_value = NULL;
	bool prop_exists[HW_PROP_MAX];
	u32 off_count;
	struct sde_cdm_cfg *cdm;

	if (!sde_cfg) {
		SDE_ERROR("invalid argument\n");
		rc = -EINVAL;
		goto end;
	}

	prop_value = kzalloc(HW_PROP_MAX *
			sizeof(struct sde_prop_value), GFP_KERNEL);
	if (!prop_value) {
		rc = -ENOMEM;
		goto end;
	}

	rc = _validate_dt_entry(np, cdm_prop, ARRAY_SIZE(cdm_prop), prop_count,
		&off_count);
	if (rc)
		goto end;

	sde_cfg->cdm_count = off_count;

	rc = _read_dt_entry(np, cdm_prop, ARRAY_SIZE(cdm_prop), prop_count,
		prop_exists, prop_value);
	if (rc)
		goto end;

	for (i = 0; i < off_count; i++) {
		cdm = sde_cfg->cdm + i;
		cdm->base = PROP_VALUE_ACCESS(prop_value, HW_OFF, i);
		cdm->id = CDM_0 + i;
		snprintf(cdm->name, SDE_HW_BLK_NAME_LEN, "cdm_%u",
				cdm->id - CDM_0);
		cdm->len = PROP_VALUE_ACCESS(prop_value, HW_LEN, 0);

		/* intf3 and wb2 for cdm block */
		cdm->wb_connect = sde_cfg->wb_count ? BIT(WB_2) : BIT(31);
		cdm->intf_connect = sde_cfg->intf_count ? BIT(INTF_3) : BIT(31);

		if (IS_SDE_CTL_REV_100(sde_cfg->ctl_rev))
			set_bit(SDE_CDM_INPUT_CTRL, &cdm->features);
	}

end:
	kfree(prop_value);
	return rc;
}

static int sde_vbif_parse_dt(struct device_node *np,
				struct sde_mdss_cfg *sde_cfg)
{
	int rc, prop_count[VBIF_PROP_MAX], i, j, k;
	struct sde_prop_value *prop_value = NULL;
	bool prop_exists[VBIF_PROP_MAX];
	u32 off_count, vbif_len;
	struct sde_vbif_cfg *vbif;

	if (!sde_cfg) {
		SDE_ERROR("invalid argument\n");
		rc = -EINVAL;
		goto end;
	}

	prop_value = kzalloc(VBIF_PROP_MAX *
			sizeof(struct sde_prop_value), GFP_KERNEL);
	if (!prop_value) {
		rc = -ENOMEM;
		goto end;
	}

	rc = _validate_dt_entry(np, vbif_prop, ARRAY_SIZE(vbif_prop),
			prop_count, &off_count);
	if (rc)
		goto end;

	rc = _validate_dt_entry(np, &vbif_prop[VBIF_DYNAMIC_OT_RD_LIMIT], 1,
			&prop_count[VBIF_DYNAMIC_OT_RD_LIMIT], NULL);
	if (rc)
		goto end;

	rc = _validate_dt_entry(np, &vbif_prop[VBIF_DYNAMIC_OT_WR_LIMIT], 1,
			&prop_count[VBIF_DYNAMIC_OT_WR_LIMIT], NULL);
	if (rc)
		goto end;

	rc = _validate_dt_entry(np, &vbif_prop[VBIF_QOS_RT_REMAP], 1,
			&prop_count[VBIF_QOS_RT_REMAP], NULL);
	if (rc)
		goto end;

	rc = _validate_dt_entry(np, &vbif_prop[VBIF_QOS_NRT_REMAP], 1,
			&prop_count[VBIF_QOS_NRT_REMAP], NULL);
	if (rc)
		goto end;

	rc = _validate_dt_entry(np, &vbif_prop[VBIF_MEMTYPE_0], 1,
			&prop_count[VBIF_MEMTYPE_0], NULL);
	if (rc)
		goto end;

	rc = _validate_dt_entry(np, &vbif_prop[VBIF_MEMTYPE_1], 1,
			&prop_count[VBIF_MEMTYPE_1], NULL);
	if (rc)
		goto end;

	sde_cfg->vbif_count = off_count;

	rc = _read_dt_entry(np, vbif_prop, ARRAY_SIZE(vbif_prop), prop_count,
		prop_exists, prop_value);
	if (rc)
		goto end;

	vbif_len = PROP_VALUE_ACCESS(prop_value, VBIF_LEN, 0);
	if (!prop_exists[VBIF_LEN])
		vbif_len = DEFAULT_SDE_HW_BLOCK_LEN;

	for (i = 0; i < off_count; i++) {
		vbif = sde_cfg->vbif + i;
		vbif->base = PROP_VALUE_ACCESS(prop_value, VBIF_OFF, i);
		vbif->len = vbif_len;
		vbif->id = VBIF_0 + PROP_VALUE_ACCESS(prop_value, VBIF_ID, i);
		snprintf(vbif->name, SDE_HW_BLK_NAME_LEN, "vbif_%u",
				vbif->id - VBIF_0);

		SDE_DEBUG("vbif:%d\n", vbif->id - VBIF_0);

		vbif->xin_halt_timeout = VBIF_XIN_HALT_TIMEOUT;

		vbif->default_ot_rd_limit = PROP_VALUE_ACCESS(prop_value,
				VBIF_DEFAULT_OT_RD_LIMIT, 0);
		SDE_DEBUG("default_ot_rd_limit=%u\n",
				vbif->default_ot_rd_limit);

		vbif->default_ot_wr_limit = PROP_VALUE_ACCESS(prop_value,
				VBIF_DEFAULT_OT_WR_LIMIT, 0);
		SDE_DEBUG("default_ot_wr_limit=%u\n",
				vbif->default_ot_wr_limit);

		vbif->dynamic_ot_rd_tbl.count =
				prop_count[VBIF_DYNAMIC_OT_RD_LIMIT] / 2;
		SDE_DEBUG("dynamic_ot_rd_tbl.count=%u\n",
				vbif->dynamic_ot_rd_tbl.count);
		if (vbif->dynamic_ot_rd_tbl.count) {
			vbif->dynamic_ot_rd_tbl.cfg = kcalloc(
				vbif->dynamic_ot_rd_tbl.count,
				sizeof(struct sde_vbif_dynamic_ot_cfg),
				GFP_KERNEL);
			if (!vbif->dynamic_ot_rd_tbl.cfg) {
				rc = -ENOMEM;
				goto end;
			}
		}

		for (j = 0, k = 0; j < vbif->dynamic_ot_rd_tbl.count; j++) {
			vbif->dynamic_ot_rd_tbl.cfg[j].pps = (u64)
				PROP_VALUE_ACCESS(prop_value,
				VBIF_DYNAMIC_OT_RD_LIMIT, k++);
			vbif->dynamic_ot_rd_tbl.cfg[j].ot_limit =
				PROP_VALUE_ACCESS(prop_value,
				VBIF_DYNAMIC_OT_RD_LIMIT, k++);
			SDE_DEBUG("dynamic_ot_rd_tbl[%d].cfg=<%llu %u>\n", j,
				vbif->dynamic_ot_rd_tbl.cfg[j].pps,
				vbif->dynamic_ot_rd_tbl.cfg[j].ot_limit);
		}

		vbif->dynamic_ot_wr_tbl.count =
				prop_count[VBIF_DYNAMIC_OT_WR_LIMIT] / 2;
		SDE_DEBUG("dynamic_ot_wr_tbl.count=%u\n",
				vbif->dynamic_ot_wr_tbl.count);
		if (vbif->dynamic_ot_wr_tbl.count) {
			vbif->dynamic_ot_wr_tbl.cfg = kcalloc(
				vbif->dynamic_ot_wr_tbl.count,
				sizeof(struct sde_vbif_dynamic_ot_cfg),
				GFP_KERNEL);
			if (!vbif->dynamic_ot_wr_tbl.cfg) {
				rc = -ENOMEM;
				goto end;
			}
		}

		for (j = 0, k = 0; j < vbif->dynamic_ot_wr_tbl.count; j++) {
			vbif->dynamic_ot_wr_tbl.cfg[j].pps = (u64)
				PROP_VALUE_ACCESS(prop_value,
				VBIF_DYNAMIC_OT_WR_LIMIT, k++);
			vbif->dynamic_ot_wr_tbl.cfg[j].ot_limit =
				PROP_VALUE_ACCESS(prop_value,
				VBIF_DYNAMIC_OT_WR_LIMIT, k++);
			SDE_DEBUG("dynamic_ot_wr_tbl[%d].cfg=<%llu %u>\n", j,
				vbif->dynamic_ot_wr_tbl.cfg[j].pps,
				vbif->dynamic_ot_wr_tbl.cfg[j].ot_limit);
		}

		if (vbif->default_ot_rd_limit || vbif->default_ot_wr_limit ||
				vbif->dynamic_ot_rd_tbl.count ||
				vbif->dynamic_ot_wr_tbl.count)
			set_bit(SDE_VBIF_QOS_OTLIM, &vbif->features);

		vbif->qos_rt_tbl.npriority_lvl =
				prop_count[VBIF_QOS_RT_REMAP];
		SDE_DEBUG("qos_rt_tbl.npriority_lvl=%u\n",
				vbif->qos_rt_tbl.npriority_lvl);
		if (vbif->qos_rt_tbl.npriority_lvl == sde_cfg->vbif_qos_nlvl) {
			vbif->qos_rt_tbl.priority_lvl = kcalloc(
				vbif->qos_rt_tbl.npriority_lvl, sizeof(u32),
				GFP_KERNEL);
			if (!vbif->qos_rt_tbl.priority_lvl) {
				rc = -ENOMEM;
				goto end;
			}
		} else if (vbif->qos_rt_tbl.npriority_lvl) {
			vbif->qos_rt_tbl.npriority_lvl = 0;
			vbif->qos_rt_tbl.priority_lvl = NULL;
			SDE_ERROR("invalid qos rt table\n");
		}

		for (j = 0; j < vbif->qos_rt_tbl.npriority_lvl; j++) {
			vbif->qos_rt_tbl.priority_lvl[j] =
				PROP_VALUE_ACCESS(prop_value,
						VBIF_QOS_RT_REMAP, j);
			SDE_DEBUG("lvl[%d]=%u\n", j,
					vbif->qos_rt_tbl.priority_lvl[j]);
		}

		vbif->qos_nrt_tbl.npriority_lvl =
				prop_count[VBIF_QOS_NRT_REMAP];
		SDE_DEBUG("qos_nrt_tbl.npriority_lvl=%u\n",
				vbif->qos_nrt_tbl.npriority_lvl);

		if (vbif->qos_nrt_tbl.npriority_lvl == sde_cfg->vbif_qos_nlvl) {
			vbif->qos_nrt_tbl.priority_lvl = kcalloc(
				vbif->qos_nrt_tbl.npriority_lvl, sizeof(u32),
				GFP_KERNEL);
			if (!vbif->qos_nrt_tbl.priority_lvl) {
				rc = -ENOMEM;
				goto end;
			}
		} else if (vbif->qos_nrt_tbl.npriority_lvl) {
			vbif->qos_nrt_tbl.npriority_lvl = 0;
			vbif->qos_nrt_tbl.priority_lvl = NULL;
			SDE_ERROR("invalid qos nrt table\n");
		}

		for (j = 0; j < vbif->qos_nrt_tbl.npriority_lvl; j++) {
			vbif->qos_nrt_tbl.priority_lvl[j] =
				PROP_VALUE_ACCESS(prop_value,
						VBIF_QOS_NRT_REMAP, j);
			SDE_DEBUG("lvl[%d]=%u\n", j,
					vbif->qos_nrt_tbl.priority_lvl[j]);
		}

		if (vbif->qos_rt_tbl.npriority_lvl ||
				vbif->qos_nrt_tbl.npriority_lvl)
			set_bit(SDE_VBIF_QOS_REMAP, &vbif->features);

		vbif->memtype_count = prop_count[VBIF_MEMTYPE_0] +
					prop_count[VBIF_MEMTYPE_1];
		if (vbif->memtype_count > MAX_XIN_COUNT) {
			vbif->memtype_count = 0;
			SDE_ERROR("too many memtype defs, ignoring entries\n");
		}
		for (j = 0, k = 0; j < prop_count[VBIF_MEMTYPE_0]; j++)
			vbif->memtype[k++] = PROP_VALUE_ACCESS(
					prop_value, VBIF_MEMTYPE_0, j);
		for (j = 0; j < prop_count[VBIF_MEMTYPE_1]; j++)
			vbif->memtype[k++] = PROP_VALUE_ACCESS(
					prop_value, VBIF_MEMTYPE_1, j);
	}

end:
	kfree(prop_value);
	return rc;
}

static int sde_pp_parse_dt(struct device_node *np, struct sde_mdss_cfg *sde_cfg)
{
	int rc, prop_count[PP_PROP_MAX], i;
	struct sde_prop_value *prop_value = NULL;
	bool prop_exists[PP_PROP_MAX];
	u32 off_count, major_version;
	struct sde_pingpong_cfg *pp;
	struct sde_pingpong_sub_blks *sblk;

	if (!sde_cfg) {
		SDE_ERROR("invalid argument\n");
		rc = -EINVAL;
		goto end;
	}

	prop_value = kzalloc(PP_PROP_MAX *
			sizeof(struct sde_prop_value), GFP_KERNEL);
	if (!prop_value) {
		rc = -ENOMEM;
		goto end;
	}

	rc = _validate_dt_entry(np, pp_prop, ARRAY_SIZE(pp_prop), prop_count,
		&off_count);
	if (rc)
		goto end;

	sde_cfg->pingpong_count = off_count;

	rc = _read_dt_entry(np, pp_prop, ARRAY_SIZE(pp_prop), prop_count,
		prop_exists, prop_value);
	if (rc)
		goto end;

	for (i = 0; i < off_count; i++) {
		pp = sde_cfg->pingpong + i;
		sblk = kzalloc(sizeof(*sblk), GFP_KERNEL);
		if (!sblk) {
			rc = -ENOMEM;
			/* catalog deinit will release the allocated blocks */
			goto end;
		}
		pp->sblk = sblk;

		pp->base = PROP_VALUE_ACCESS(prop_value, PP_OFF, i);
		pp->id = PINGPONG_0 + i;
		snprintf(pp->name, SDE_HW_BLK_NAME_LEN, "pingpong_%u",
				pp->id - PINGPONG_0);
		pp->len = PROP_VALUE_ACCESS(prop_value, PP_LEN, 0);

		sblk->te.base = PROP_VALUE_ACCESS(prop_value, TE_OFF, i);
		sblk->te.id = SDE_PINGPONG_TE;
		snprintf(sblk->te.name, SDE_HW_BLK_NAME_LEN, "te_%u",
				pp->id - PINGPONG_0);

		major_version = SDE_HW_MAJOR(sde_cfg->hwversion);
		if (major_version < SDE_HW_MAJOR(SDE_HW_VER_500))
			set_bit(SDE_PINGPONG_TE, &pp->features);

		sblk->te2.base = PROP_VALUE_ACCESS(prop_value, TE2_OFF, i);
		if (sblk->te2.base) {
			sblk->te2.id = SDE_PINGPONG_TE2;
			snprintf(sblk->te2.name, SDE_HW_BLK_NAME_LEN, "te2_%u",
					pp->id - PINGPONG_0);
			set_bit(SDE_PINGPONG_TE2, &pp->features);
			set_bit(SDE_PINGPONG_SPLIT, &pp->features);
		}

		if (PROP_VALUE_ACCESS(prop_value, PP_SLAVE, i))
			set_bit(SDE_PINGPONG_SLAVE, &pp->features);

		sblk->dsc.base = PROP_VALUE_ACCESS(prop_value, DSC_OFF, i);
		if (sblk->dsc.base) {
			sblk->dsc.id = SDE_PINGPONG_DSC;
			snprintf(sblk->dsc.name, SDE_HW_BLK_NAME_LEN, "dsc_%u",
					pp->id - PINGPONG_0);
			set_bit(SDE_PINGPONG_DSC, &pp->features);
		}

		sblk->dither.base = PROP_VALUE_ACCESS(prop_value, DITHER_OFF,
							i);
		if (sblk->dither.base) {
			sblk->dither.id = SDE_PINGPONG_DITHER;
			snprintf(sblk->dither.name, SDE_HW_BLK_NAME_LEN,
					"dither_%u", pp->id);
			set_bit(SDE_PINGPONG_DITHER, &pp->features);
		}
		sblk->dither.len = PROP_VALUE_ACCESS(prop_value, DITHER_LEN, 0);
		sblk->dither.version = PROP_VALUE_ACCESS(prop_value, DITHER_VER,
								0);

		if (prop_exists[PP_MERGE_3D_ID]) {
			set_bit(SDE_PINGPONG_MERGE_3D, &pp->features);
			pp->merge_3d_id = PROP_VALUE_ACCESS(prop_value,
					PP_MERGE_3D_ID, i) + 1;
		}
	}

end:
	kfree(prop_value);
	return rc;
}

static int sde_parse_dt(struct device_node *np, struct sde_mdss_cfg *cfg)
{
	int rc, i, dma_rc, len, prop_count[SDE_PROP_MAX];
	struct sde_prop_value *prop_value = NULL;
	bool prop_exists[SDE_PROP_MAX];
	const char *type;
	u32 major_version;

	if (!cfg) {
		SDE_ERROR("invalid argument\n");
		rc = -EINVAL;
		goto end;
	}

	prop_value = kzalloc(SDE_PROP_MAX *
			sizeof(struct sde_prop_value), GFP_KERNEL);
	if (!prop_value) {
		rc = -ENOMEM;
		goto end;
	}

	rc = _validate_dt_entry(np, sde_prop, ARRAY_SIZE(sde_prop), prop_count,
		&len);
	if (rc)
		goto end;

	rc = _validate_dt_entry(np, &sde_prop[SEC_SID_MASK], 1,
				&prop_count[SEC_SID_MASK], NULL);
	if (rc)
		goto end;

	rc = _read_dt_entry(np, sde_prop, ARRAY_SIZE(sde_prop), prop_count,
		prop_exists, prop_value);
	if (rc)
		goto end;

	cfg->mdss_count = 1;
	cfg->mdss[0].base = MDSS_BASE_OFFSET;
	cfg->mdss[0].id = MDP_TOP;
	snprintf(cfg->mdss[0].name, SDE_HW_BLK_NAME_LEN, "mdss_%u",
			cfg->mdss[0].id - MDP_TOP);

	cfg->mdp_count = 1;
	cfg->mdp[0].id = MDP_TOP;
	snprintf(cfg->mdp[0].name, SDE_HW_BLK_NAME_LEN, "top_%u",
		cfg->mdp[0].id - MDP_TOP);
	cfg->mdp[0].base = PROP_VALUE_ACCESS(prop_value, SDE_OFF, 0);
	cfg->mdp[0].len = PROP_VALUE_ACCESS(prop_value, SDE_LEN, 0);
	if (!prop_exists[SDE_LEN])
		cfg->mdp[0].len = DEFAULT_SDE_HW_BLOCK_LEN;

	cfg->max_sspp_linewidth = PROP_VALUE_ACCESS(prop_value,
			SSPP_LINEWIDTH, 0);
	if (!prop_exists[SSPP_LINEWIDTH])
		cfg->max_sspp_linewidth = DEFAULT_SDE_LINE_WIDTH;

	cfg->max_mixer_width = PROP_VALUE_ACCESS(prop_value,
			MIXER_LINEWIDTH, 0);
	if (!prop_exists[MIXER_LINEWIDTH])
		cfg->max_mixer_width = DEFAULT_SDE_LINE_WIDTH;

	cfg->max_mixer_blendstages = PROP_VALUE_ACCESS(prop_value,
			MIXER_BLEND, 0);
	if (!prop_exists[MIXER_BLEND])
		cfg->max_mixer_blendstages = DEFAULT_SDE_MIXER_BLENDSTAGES;

	cfg->max_wb_linewidth = PROP_VALUE_ACCESS(prop_value, WB_LINEWIDTH, 0);
	if (!prop_exists[WB_LINEWIDTH])
		cfg->max_wb_linewidth = DEFAULT_SDE_LINE_WIDTH;

	cfg->mdp[0].highest_bank_bit = PROP_VALUE_ACCESS(prop_value,
			BANK_BIT, 0);
	if (!prop_exists[BANK_BIT])
		cfg->mdp[0].highest_bank_bit = DEFAULT_SDE_HIGHEST_BANK_BIT;

	cfg->ubwc_version = SDE_HW_UBWC_VER(PROP_VALUE_ACCESS(prop_value,
			UBWC_VERSION, 0));
	if (!prop_exists[UBWC_VERSION])
		cfg->ubwc_version = DEFAULT_SDE_UBWC_VERSION;

	cfg->macrotile_mode = PROP_VALUE_ACCESS(prop_value, MACROTILE_MODE, 0);
	if (!prop_exists[MACROTILE_MODE])
		cfg->macrotile_mode = DEFAULT_SDE_UBWC_MACROTILE_MODE;

	cfg->ubwc_bw_calc_version =
		PROP_VALUE_ACCESS(prop_value, UBWC_BW_CALC_VERSION, 0);

	cfg->mdp[0].ubwc_static = PROP_VALUE_ACCESS(prop_value, UBWC_STATIC, 0);
	if (!prop_exists[UBWC_STATIC])
		cfg->mdp[0].ubwc_static = DEFAULT_SDE_UBWC_STATIC;

	cfg->mdp[0].ubwc_swizzle = PROP_VALUE_ACCESS(prop_value,
			UBWC_SWIZZLE, 0);
	if (!prop_exists[UBWC_SWIZZLE])
		cfg->mdp[0].ubwc_swizzle = DEFAULT_SDE_UBWC_SWIZZLE;

	cfg->mdp[0].has_dest_scaler =
		PROP_VALUE_ACCESS(prop_value, DEST_SCALER, 0);

	cfg->mdp[0].smart_panel_align_mode =
		PROP_VALUE_ACCESS(prop_value, SMART_PANEL_ALIGN_MODE, 0);

	major_version = SDE_HW_MAJOR(cfg->hwversion);
	if (major_version < SDE_HW_MAJOR(SDE_HW_VER_500))
		set_bit(SDE_MDP_VSYNC_SEL, &cfg->mdp[0].features);

	if (prop_exists[SEC_SID_MASK]) {
		cfg->sec_sid_mask_count = prop_count[SEC_SID_MASK];
		for (i = 0; i < cfg->sec_sid_mask_count; i++)
			cfg->sec_sid_mask[i] =
				PROP_VALUE_ACCESS(prop_value, SEC_SID_MASK, i);
	}

	rc = of_property_read_string(np, sde_prop[QSEED_TYPE].prop_name, &type);
	if (!rc && !strcmp(type, "qseedv3")) {
		cfg->qseed_type = SDE_SSPP_SCALER_QSEED3;
	} else if (!rc && !strcmp(type, "qseedv2")) {
		cfg->qseed_type = SDE_SSPP_SCALER_QSEED2;
	} else if (rc) {
		SDE_DEBUG("invalid QSEED configuration\n");
		rc = 0;
	}

	rc = of_property_read_string(np, sde_prop[CSC_TYPE].prop_name, &type);
	if (!rc && !strcmp(type, "csc")) {
		cfg->csc_type = SDE_SSPP_CSC;
	} else if (!rc && !strcmp(type, "csc-10bit")) {
		cfg->csc_type = SDE_SSPP_CSC_10BIT;
	} else if (rc) {
		SDE_DEBUG("invalid csc configuration\n");
		rc = 0;
	}

	/*
	 * Current SDE support only Smart DMA 2.0-2.5.
	 * No support for Smart DMA 1.0 yet.
	 */
	cfg->smart_dma_rev = 0;
	dma_rc = of_property_read_string(np, sde_prop[SMART_DMA_REV].prop_name,
			&type);
	if (dma_rc) {
		SDE_DEBUG("invalid SMART_DMA_REV node in device tree: %d\n",
				dma_rc);
	} else if (!strcmp(type, "smart_dma_v2p5")) {
		cfg->smart_dma_rev = SDE_SSPP_SMART_DMA_V2p5;
	} else if (!strcmp(type, "smart_dma_v2")) {
		cfg->smart_dma_rev = SDE_SSPP_SMART_DMA_V2;
	} else if (!strcmp(type, "smart_dma_v1")) {
		SDE_ERROR("smart dma 1.0 is not supported in SDE\n");
	} else {
		SDE_DEBUG("unknown smart dma version\n");
	}

	cfg->has_src_split = PROP_VALUE_ACCESS(prop_value, SRC_SPLIT, 0);
	cfg->has_dim_layer = PROP_VALUE_ACCESS(prop_value, DIM_LAYER, 0);
	cfg->has_idle_pc = PROP_VALUE_ACCESS(prop_value, IDLE_PC, 0);
	cfg->pipe_order_type = PROP_VALUE_ACCESS(prop_value,
		PIPE_ORDER_VERSION, 0);
end:
	kfree(prop_value);
	return rc;
}

static int sde_parse_reg_dma_dt(struct device_node *np,
		struct sde_mdss_cfg *sde_cfg)
{
	u32 val;
	int rc = 0;
	int i = 0;

	sde_cfg->reg_dma_count = 0;
	for (i = 0; i < REG_DMA_PROP_MAX; i++) {
		rc = of_property_read_u32(np, reg_dma_prop[i].prop_name,
				&val);
		if (rc)
			break;
		switch (i) {
		case REG_DMA_OFF:
			sde_cfg->dma_cfg.base = val;
			break;
		case REG_DMA_VERSION:
			sde_cfg->dma_cfg.version = val;
			break;
		case REG_DMA_TRIGGER_OFF:
			sde_cfg->dma_cfg.trigger_sel_off = val;
			break;
		default:
			break;
		}
	}
	if (!rc && i == REG_DMA_PROP_MAX)
		sde_cfg->reg_dma_count = 1;
	/* reg dma is optional feature hence return 0 */
	return 0;
}

static int sde_perf_parse_dt(struct device_node *np, struct sde_mdss_cfg *cfg)
{
	int rc, len, prop_count[PERF_PROP_MAX];
	struct sde_prop_value *prop_value = NULL;
	bool prop_exists[PERF_PROP_MAX];
	const char *str = NULL;
	int j, k;

	if (!cfg) {
		SDE_ERROR("invalid argument\n");
		rc = -EINVAL;
		goto end;
	}

	prop_value = kzalloc(PERF_PROP_MAX *
			sizeof(struct sde_prop_value), GFP_KERNEL);
	if (!prop_value) {
		rc = -ENOMEM;
		goto end;
	}

	rc = _validate_dt_entry(np, sde_perf_prop, ARRAY_SIZE(sde_perf_prop),
			prop_count, &len);
	if (rc)
		goto freeprop;

	rc = _validate_dt_entry(np, &sde_perf_prop[PERF_DANGER_LUT], 1,
			&prop_count[PERF_DANGER_LUT], NULL);
	if (rc)
		goto freeprop;

	rc = _validate_dt_entry(np, &sde_perf_prop[PERF_SAFE_LUT_LINEAR], 1,
			&prop_count[PERF_SAFE_LUT_LINEAR], NULL);
	if (rc)
		goto freeprop;

	rc = _validate_dt_entry(np, &sde_perf_prop[PERF_SAFE_LUT_MACROTILE], 1,
			&prop_count[PERF_SAFE_LUT_MACROTILE], NULL);
	if (rc)
		goto freeprop;

	rc = _validate_dt_entry(np, &sde_perf_prop[PERF_SAFE_LUT_NRT], 1,
			&prop_count[PERF_SAFE_LUT_NRT], NULL);
	if (rc)
		goto freeprop;

	rc = _validate_dt_entry(np, &sde_perf_prop[PERF_SAFE_LUT_CWB], 1,
			&prop_count[PERF_SAFE_LUT_CWB], NULL);
	if (rc)
		goto freeprop;

	rc = _validate_dt_entry(np, &sde_perf_prop[PERF_QOS_LUT_LINEAR], 1,
			&prop_count[PERF_QOS_LUT_LINEAR], NULL);
	if (rc)
		goto freeprop;

	rc = _validate_dt_entry(np, &sde_perf_prop[PERF_QOS_LUT_MACROTILE], 1,
			&prop_count[PERF_QOS_LUT_MACROTILE], NULL);
	if (rc)
		goto freeprop;

	rc = _validate_dt_entry(np, &sde_perf_prop[PERF_QOS_LUT_NRT], 1,
			&prop_count[PERF_QOS_LUT_NRT], NULL);
	if (rc)
		goto freeprop;

	rc = _validate_dt_entry(np, &sde_perf_prop[PERF_QOS_LUT_CWB], 1,
			&prop_count[PERF_QOS_LUT_CWB], NULL);
	if (rc)
		goto freeprop;

	rc = _validate_dt_entry(np, &sde_perf_prop[PERF_CDP_SETTING], 1,
			&prop_count[PERF_CDP_SETTING], NULL);
	if (rc)
		goto freeprop;

	rc = _validate_dt_entry(np,
			&sde_perf_prop[PERF_QOS_LUT_MACROTILE_QSEED], 1,
			&prop_count[PERF_QOS_LUT_MACROTILE_QSEED], NULL);
	if (rc)
		goto freeprop;

	rc = _validate_dt_entry(np,
			&sde_perf_prop[PERF_SAFE_LUT_MACROTILE_QSEED], 1,
			&prop_count[PERF_SAFE_LUT_MACROTILE_QSEED], NULL);
	if (rc)
		goto freeprop;

	rc = _read_dt_entry(np, sde_perf_prop, ARRAY_SIZE(sde_perf_prop),
			prop_count, prop_exists, prop_value);
	if (rc)
		goto freeprop;

	cfg->perf.max_bw_low =
			prop_exists[PERF_MAX_BW_LOW] ?
			PROP_VALUE_ACCESS(prop_value, PERF_MAX_BW_LOW, 0) :
			DEFAULT_MAX_BW_LOW;
	cfg->perf.max_bw_high =
			prop_exists[PERF_MAX_BW_HIGH] ?
			PROP_VALUE_ACCESS(prop_value, PERF_MAX_BW_HIGH, 0) :
			DEFAULT_MAX_BW_HIGH;
	cfg->perf.min_core_ib =
			prop_exists[PERF_MIN_CORE_IB] ?
			PROP_VALUE_ACCESS(prop_value, PERF_MIN_CORE_IB, 0) :
			DEFAULT_MAX_BW_LOW;
	cfg->perf.min_llcc_ib =
			prop_exists[PERF_MIN_LLCC_IB] ?
			PROP_VALUE_ACCESS(prop_value, PERF_MIN_LLCC_IB, 0) :
			DEFAULT_MAX_BW_LOW;
	cfg->perf.min_dram_ib =
			prop_exists[PERF_MIN_DRAM_IB] ?
			PROP_VALUE_ACCESS(prop_value, PERF_MIN_DRAM_IB, 0) :
			DEFAULT_MAX_BW_LOW;

	/*
	 * The following performance parameters (e.g. core_ib_ff) are
	 * mapped directly as device tree string constants.
	 */
	rc = of_property_read_string(np,
			sde_perf_prop[PERF_CORE_IB_FF].prop_name, &str);
	cfg->perf.core_ib_ff = rc ? DEFAULT_CORE_IB_FF : str;
	rc = of_property_read_string(np,
			sde_perf_prop[PERF_CORE_CLK_FF].prop_name, &str);
	cfg->perf.core_clk_ff = rc ? DEFAULT_CORE_CLK_FF : str;
	rc = of_property_read_string(np,
			sde_perf_prop[PERF_COMP_RATIO_RT].prop_name, &str);
	cfg->perf.comp_ratio_rt = rc ? DEFAULT_COMP_RATIO_RT : str;
	rc = of_property_read_string(np,
			sde_perf_prop[PERF_COMP_RATIO_NRT].prop_name, &str);
	cfg->perf.comp_ratio_nrt = rc ? DEFAULT_COMP_RATIO_NRT : str;
	rc = 0;

	cfg->perf.undersized_prefill_lines =
			prop_exists[PERF_UNDERSIZED_PREFILL_LINES] ?
			PROP_VALUE_ACCESS(prop_value,
					PERF_UNDERSIZED_PREFILL_LINES, 0) :
			DEFAULT_UNDERSIZED_PREFILL_LINES;
	cfg->perf.xtra_prefill_lines =
			prop_exists[PERF_XTRA_PREFILL_LINES] ?
			PROP_VALUE_ACCESS(prop_value,
					PERF_XTRA_PREFILL_LINES, 0) :
			DEFAULT_XTRA_PREFILL_LINES;
	cfg->perf.dest_scale_prefill_lines =
			prop_exists[PERF_DEST_SCALE_PREFILL_LINES] ?
			PROP_VALUE_ACCESS(prop_value,
					PERF_DEST_SCALE_PREFILL_LINES, 0) :
			DEFAULT_DEST_SCALE_PREFILL_LINES;
	cfg->perf.macrotile_prefill_lines =
			prop_exists[PERF_MACROTILE_PREFILL_LINES] ?
			PROP_VALUE_ACCESS(prop_value,
					PERF_MACROTILE_PREFILL_LINES, 0) :
			DEFAULT_MACROTILE_PREFILL_LINES;
	cfg->perf.yuv_nv12_prefill_lines =
			prop_exists[PERF_YUV_NV12_PREFILL_LINES] ?
			PROP_VALUE_ACCESS(prop_value,
					PERF_YUV_NV12_PREFILL_LINES, 0) :
			DEFAULT_YUV_NV12_PREFILL_LINES;
	cfg->perf.linear_prefill_lines =
			prop_exists[PERF_LINEAR_PREFILL_LINES] ?
			PROP_VALUE_ACCESS(prop_value,
					PERF_LINEAR_PREFILL_LINES, 0) :
			DEFAULT_LINEAR_PREFILL_LINES;
	cfg->perf.downscaling_prefill_lines =
			prop_exists[PERF_DOWNSCALING_PREFILL_LINES] ?
			PROP_VALUE_ACCESS(prop_value,
					PERF_DOWNSCALING_PREFILL_LINES, 0) :
			DEFAULT_DOWNSCALING_PREFILL_LINES;
	cfg->perf.amortizable_threshold =
			prop_exists[PERF_AMORTIZABLE_THRESHOLD] ?
			PROP_VALUE_ACCESS(prop_value,
					PERF_AMORTIZABLE_THRESHOLD, 0) :
			DEFAULT_AMORTIZABLE_THRESHOLD;

	if (prop_exists[PERF_DANGER_LUT] && prop_count[PERF_DANGER_LUT] <=
			SDE_QOS_LUT_USAGE_MAX) {
		for (j = 0; j < prop_count[PERF_DANGER_LUT]; j++) {
			cfg->perf.danger_lut_tbl[j] =
					PROP_VALUE_ACCESS(prop_value,
						PERF_DANGER_LUT, j);
			SDE_DEBUG("danger usage:%d lut:0x%x\n",
					j, cfg->perf.danger_lut_tbl[j]);
		}
	}

	for (j = 0; j < SDE_QOS_LUT_USAGE_MAX; j++) {
		static const u32 safe_key[SDE_QOS_LUT_USAGE_MAX] = {
			[SDE_QOS_LUT_USAGE_LINEAR] =
					PERF_SAFE_LUT_LINEAR,
			[SDE_QOS_LUT_USAGE_MACROTILE] =
					PERF_SAFE_LUT_MACROTILE,
			[SDE_QOS_LUT_USAGE_NRT] =
					PERF_SAFE_LUT_NRT,
			[SDE_QOS_LUT_USAGE_CWB] =
					PERF_SAFE_LUT_CWB,
			[SDE_QOS_LUT_USAGE_MACROTILE_QSEED] =
					PERF_SAFE_LUT_MACROTILE_QSEED,
		};
		const u32 entry_size = 2;
		int m, count;
		int key = safe_key[j];

		if (!prop_exists[key])
			continue;

		count = prop_count[key] / entry_size;

		cfg->perf.sfe_lut_tbl[j].entries = kcalloc(count,
			sizeof(struct sde_qos_lut_entry), GFP_KERNEL);
		if (!cfg->perf.sfe_lut_tbl[j].entries) {
			rc = -ENOMEM;
			goto freeprop;
		}

		for (k = 0, m = 0; k < count; k++, m += entry_size) {
			u64 lut_lo;

			cfg->perf.sfe_lut_tbl[j].entries[k].fl =
					PROP_VALUE_ACCESS(prop_value, key, m);
			lut_lo = PROP_VALUE_ACCESS(prop_value, key, m + 1);
			cfg->perf.sfe_lut_tbl[j].entries[k].lut = lut_lo;
			SDE_DEBUG("safe usage:%d.%d fl:%d lut:0x%llx\n",
				j, k,
				cfg->perf.sfe_lut_tbl[j].entries[k].fl,
				cfg->perf.sfe_lut_tbl[j].entries[k].lut);
		}
		cfg->perf.sfe_lut_tbl[j].nentry = count;
	}

	for (j = 0; j < SDE_QOS_LUT_USAGE_MAX; j++) {
		static const u32 prop_key[SDE_QOS_LUT_USAGE_MAX] = {
			[SDE_QOS_LUT_USAGE_LINEAR] =
					PERF_QOS_LUT_LINEAR,
			[SDE_QOS_LUT_USAGE_MACROTILE] =
					PERF_QOS_LUT_MACROTILE,
			[SDE_QOS_LUT_USAGE_NRT] =
					PERF_QOS_LUT_NRT,
			[SDE_QOS_LUT_USAGE_CWB] =
					PERF_QOS_LUT_CWB,
			[SDE_QOS_LUT_USAGE_MACROTILE_QSEED] =
					PERF_QOS_LUT_MACROTILE_QSEED,
		};
		const u32 entry_size = 3;
		int m, count;
		int key = prop_key[j];

		if (!prop_exists[key])
			continue;

		count = prop_count[key] / entry_size;

		cfg->perf.qos_lut_tbl[j].entries = kcalloc(count,
			sizeof(struct sde_qos_lut_entry), GFP_KERNEL);
		if (!cfg->perf.qos_lut_tbl[j].entries) {
			rc = -ENOMEM;
			goto freeprop;
		}

		for (k = 0, m = 0; k < count; k++, m += entry_size) {
			u64 lut_hi, lut_lo;

			cfg->perf.qos_lut_tbl[j].entries[k].fl =
					PROP_VALUE_ACCESS(prop_value, key, m);
			lut_hi = PROP_VALUE_ACCESS(prop_value, key, m + 1);
			lut_lo = PROP_VALUE_ACCESS(prop_value, key, m + 2);
			cfg->perf.qos_lut_tbl[j].entries[k].lut =
					(lut_hi << 32) | lut_lo;
			SDE_DEBUG("usage:%d.%d fl:%d lut:0x%llx\n",
				j, k,
				cfg->perf.qos_lut_tbl[j].entries[k].fl,
				cfg->perf.qos_lut_tbl[j].entries[k].lut);
		}
		cfg->perf.qos_lut_tbl[j].nentry = count;
	}

	if (prop_exists[PERF_CDP_SETTING]) {
		const u32 prop_size = 2;
		u32 count = prop_count[PERF_CDP_SETTING] / prop_size;

		count = min_t(u32, count, SDE_PERF_CDP_USAGE_MAX);

		for (j = 0; j < count; j++) {
			cfg->perf.cdp_cfg[j].rd_enable =
					PROP_VALUE_ACCESS(prop_value,
					PERF_CDP_SETTING, j * prop_size);
			cfg->perf.cdp_cfg[j].wr_enable =
					PROP_VALUE_ACCESS(prop_value,
					PERF_CDP_SETTING, j * prop_size + 1);
			SDE_DEBUG("cdp usage:%d rd:%d wr:%d\n",
				j, cfg->perf.cdp_cfg[j].rd_enable,
				cfg->perf.cdp_cfg[j].wr_enable);
		}

		cfg->has_cdp = true;
	}

	cfg->perf.cpu_mask =
			prop_exists[PERF_CPU_MASK] ?
			PROP_VALUE_ACCESS(prop_value, PERF_CPU_MASK, 0) :
			DEFAULT_CPU_MASK;
	cfg->perf.cpu_dma_latency =
			prop_exists[PERF_CPU_DMA_LATENCY] ?
			PROP_VALUE_ACCESS(prop_value, PERF_CPU_DMA_LATENCY, 0) :
			DEFAULT_CPU_DMA_LATENCY;

freeprop:
	kfree(prop_value);
end:
	return rc;
}

static int sde_parse_merge_3d_dt(struct device_node *np,
		struct sde_mdss_cfg *sde_cfg)
{
	int rc, prop_count[HW_PROP_MAX], off_count, i;
	struct sde_prop_value *prop_value = NULL;
	bool prop_exists[HW_PROP_MAX];
	struct sde_merge_3d_cfg *merge_3d;

	prop_value = kcalloc(HW_PROP_MAX, sizeof(struct sde_prop_value),
			GFP_KERNEL);
	if (!prop_value) {
		rc = -ENOMEM;
		goto fail;
	}

	rc = _validate_dt_entry(np, merge_3d_prop, ARRAY_SIZE(merge_3d_prop),
		prop_count, &off_count);
	if (rc)
		goto error;

	sde_cfg->merge_3d_count = off_count;

	rc = _read_dt_entry(np, merge_3d_prop, ARRAY_SIZE(merge_3d_prop),
			prop_count,
			prop_exists, prop_value);
	if (rc)
		goto error;

	for (i = 0; i < off_count; i++) {
		merge_3d = sde_cfg->merge_3d + i;
		merge_3d->base = PROP_VALUE_ACCESS(prop_value, HW_OFF, i);
		merge_3d->id = MERGE_3D_0 + i;
		snprintf(merge_3d->name, SDE_HW_BLK_NAME_LEN, "merge_3d_%u",
				merge_3d->id -  MERGE_3D_0);
		merge_3d->len = PROP_VALUE_ACCESS(prop_value, HW_LEN, 0);
	}

	return 0;
error:
	sde_cfg->merge_3d_count = 0;
	kfree(prop_value);
fail:
	return rc;
}

static int sde_hardware_format_caps(struct sde_mdss_cfg *sde_cfg,
	uint32_t hw_rev)
{
	int rc = 0;
	uint32_t dma_list_size, vig_list_size, wb2_list_size;
	uint32_t virt_vig_list_size;
	uint32_t cursor_list_size = 0;
	uint32_t index = 0;

	if (IS_SDE_MAJOR_MINOR_SAME((hw_rev), SDE_HW_VER_300)) {
		cursor_list_size = ARRAY_SIZE(cursor_formats);
		sde_cfg->cursor_formats = kcalloc(cursor_list_size,
			sizeof(struct sde_format_extended), GFP_KERNEL);
		if (!sde_cfg->cursor_formats) {
			rc = -ENOMEM;
			goto end;
		}
		index = sde_copy_formats(sde_cfg->cursor_formats,
			cursor_list_size, 0, cursor_formats,
			ARRAY_SIZE(cursor_formats));
	}

	dma_list_size = ARRAY_SIZE(plane_formats);
	vig_list_size = ARRAY_SIZE(plane_formats_yuv);
	virt_vig_list_size = ARRAY_SIZE(plane_formats);
	wb2_list_size = ARRAY_SIZE(wb2_formats);

	dma_list_size += ARRAY_SIZE(rgb_10bit_formats);
	vig_list_size += ARRAY_SIZE(rgb_10bit_formats)
		+ ARRAY_SIZE(tp10_ubwc_formats)
		+ ARRAY_SIZE(p010_formats);
	virt_vig_list_size += ARRAY_SIZE(rgb_10bit_formats);
	if (IS_SDE_MAJOR_MINOR_SAME((hw_rev), SDE_HW_VER_400) ||
		(IS_SDE_MAJOR_MINOR_SAME((hw_rev), SDE_HW_VER_410)) ||
		(IS_SDE_MAJOR_SAME((hw_rev), SDE_HW_VER_500)))
		vig_list_size += ARRAY_SIZE(p010_ubwc_formats);

	wb2_list_size += ARRAY_SIZE(rgb_10bit_formats)
		+ ARRAY_SIZE(tp10_ubwc_formats);

	sde_cfg->dma_formats = kcalloc(dma_list_size,
		sizeof(struct sde_format_extended), GFP_KERNEL);
	if (!sde_cfg->dma_formats) {
		rc = -ENOMEM;
		goto end;
	}

	sde_cfg->vig_formats = kcalloc(vig_list_size,
		sizeof(struct sde_format_extended), GFP_KERNEL);
	if (!sde_cfg->vig_formats) {
		rc = -ENOMEM;
		goto end;
	}

	sde_cfg->virt_vig_formats = kcalloc(virt_vig_list_size,
		sizeof(struct sde_format_extended), GFP_KERNEL);
	if (!sde_cfg->virt_vig_formats) {
		rc = -ENOMEM;
		goto end;
	}

	sde_cfg->wb_formats = kcalloc(wb2_list_size,
		sizeof(struct sde_format_extended), GFP_KERNEL);
	if (!sde_cfg->wb_formats) {
		SDE_ERROR("failed to allocate wb format list\n");
		rc = -ENOMEM;
		goto end;
	}

	if (IS_SDE_MAJOR_MINOR_SAME((hw_rev), SDE_HW_VER_300) ||
	    IS_SDE_MAJOR_MINOR_SAME((hw_rev), SDE_HW_VER_400) ||
	    IS_SDE_MAJOR_MINOR_SAME((hw_rev), SDE_HW_VER_410) ||
	    IS_SDE_MAJOR_SAME((hw_rev), SDE_HW_VER_500))
		sde_cfg->has_hdr = true;

	index = sde_copy_formats(sde_cfg->dma_formats, dma_list_size,
		0, plane_formats, ARRAY_SIZE(plane_formats));
	index += sde_copy_formats(sde_cfg->dma_formats, dma_list_size,
		index, rgb_10bit_formats,
		ARRAY_SIZE(rgb_10bit_formats));

	index = sde_copy_formats(sde_cfg->vig_formats, vig_list_size,
		0, plane_formats_yuv, ARRAY_SIZE(plane_formats_yuv));
	index += sde_copy_formats(sde_cfg->vig_formats, vig_list_size,
		index, rgb_10bit_formats,
		ARRAY_SIZE(rgb_10bit_formats));
	index += sde_copy_formats(sde_cfg->vig_formats, vig_list_size,
		index, p010_formats, ARRAY_SIZE(p010_formats));
	if (IS_SDE_MAJOR_MINOR_SAME((hw_rev), SDE_HW_VER_400) ||
		(IS_SDE_MAJOR_MINOR_SAME((hw_rev), SDE_HW_VER_410)) ||
		(IS_SDE_MAJOR_SAME((hw_rev), SDE_HW_VER_500)))
		index += sde_copy_formats(sde_cfg->vig_formats,
			vig_list_size, index, p010_ubwc_formats,
			ARRAY_SIZE(p010_ubwc_formats));
	index += sde_copy_formats(sde_cfg->vig_formats, vig_list_size,
		index, tp10_ubwc_formats,
		ARRAY_SIZE(tp10_ubwc_formats));

	index = sde_copy_formats(sde_cfg->virt_vig_formats, virt_vig_list_size,
		0, plane_formats, ARRAY_SIZE(plane_formats));
	index += sde_copy_formats(sde_cfg->virt_vig_formats, virt_vig_list_size,
		index, rgb_10bit_formats,
		ARRAY_SIZE(rgb_10bit_formats));

	index = sde_copy_formats(sde_cfg->wb_formats, wb2_list_size,
		0, wb2_formats, ARRAY_SIZE(wb2_formats));
	index += sde_copy_formats(sde_cfg->wb_formats, wb2_list_size,
		index, rgb_10bit_formats,
		ARRAY_SIZE(rgb_10bit_formats));
	index += sde_copy_formats(sde_cfg->wb_formats, wb2_list_size,
		index, tp10_ubwc_formats,
		ARRAY_SIZE(tp10_ubwc_formats));
end:
	return rc;
}

static int _sde_hardware_pre_caps(struct sde_mdss_cfg *sde_cfg, uint32_t hw_rev)
{
	int rc = 0;

	if (!sde_cfg)
		return -EINVAL;

	rc = sde_hardware_format_caps(sde_cfg, hw_rev);

	if (IS_MSM8996_TARGET(hw_rev)) {
		sde_cfg->perf.min_prefill_lines = 21;
	} else if (IS_MSM8998_TARGET(hw_rev)) {
		sde_cfg->has_wb_ubwc = true;
		sde_cfg->perf.min_prefill_lines = 25;
		sde_cfg->vbif_qos_nlvl = 4;
		sde_cfg->ts_prefill_rev = 1;
	} else if (IS_SDM845_TARGET(hw_rev)) {
		sde_cfg->has_wb_ubwc = true;
		sde_cfg->has_cwb_support = true;
		sde_cfg->perf.min_prefill_lines = 24;
		sde_cfg->vbif_qos_nlvl = 8;
		sde_cfg->ts_prefill_rev = 2;
		sde_cfg->sui_misr_supported = true;
		sde_cfg->sui_block_xin_mask = 0x3F71;
	} else if (IS_SDM670_TARGET(hw_rev)) {
		sde_cfg->has_wb_ubwc = true;
		sde_cfg->perf.min_prefill_lines = 24;
		sde_cfg->vbif_qos_nlvl = 8;
		sde_cfg->ts_prefill_rev = 2;
	} else if (IS_SM8150_TARGET(hw_rev)) {
		sde_cfg->has_cwb_support = true;
		sde_cfg->has_wb_ubwc = true;
		sde_cfg->has_qsync = true;
		sde_cfg->perf.min_prefill_lines = 24;
		sde_cfg->vbif_qos_nlvl = 8;
		sde_cfg->ts_prefill_rev = 2;
		sde_cfg->ctl_rev = SDE_CTL_CFG_VERSION_1_0_0;
		sde_cfg->delay_prg_fetch_start = true;
		sde_cfg->sui_ns_allowed = true;
		sde_cfg->sui_misr_supported = true;
		sde_cfg->sui_block_xin_mask = 0x3F71;
	} else if (IS_SDMSHRIKE_TARGET(hw_rev)) {
		sde_cfg->has_wb_ubwc = true;
		sde_cfg->perf.min_prefill_lines = 24;
		sde_cfg->vbif_qos_nlvl = 8;
		sde_cfg->ts_prefill_rev = 2;
		sde_cfg->ctl_rev = SDE_CTL_CFG_VERSION_1_0_0;
		sde_cfg->delay_prg_fetch_start = true;
	} else {
		SDE_ERROR("unsupported chipset id:%X\n", hw_rev);
		sde_cfg->perf.min_prefill_lines = 0xffff;
		rc = -ENODEV;
	}

	return rc;
}

static int _sde_hardware_post_caps(struct sde_mdss_cfg *sde_cfg,
	uint32_t hw_rev)
{
	int rc = 0, i;
	u32 max_horz_deci = 0, max_vert_deci = 0;

	if (!sde_cfg)
		return -EINVAL;

	if (IS_SM8150_TARGET(hw_rev)) {
		sde_cfg->sui_supported_blendstage =
			sde_cfg->max_mixer_blendstages - SDE_STAGE_0;

		for (i = 0; i < sde_cfg->sspp_count; i++)
			set_bit(SDE_SSPP_QOS_FL_NOCALC,
					&sde_cfg->sspp[i].features);
	}

	for (i = 0; i < sde_cfg->sspp_count; i++) {
		if (sde_cfg->sspp[i].sblk) {
			max_horz_deci = max(max_horz_deci,
				sde_cfg->sspp[i].sblk->maxhdeciexp);
			max_vert_deci = max(max_vert_deci,
				sde_cfg->sspp[i].sblk->maxvdeciexp);
		}

		/*
		 * set sec-ui blocked SSPP feature flag based on blocked
		 * xin-mask if sec-ui-misr feature is enabled;
		 */
		if (sde_cfg->sui_misr_supported
				&& (sde_cfg->sui_block_xin_mask
					& BIT(sde_cfg->sspp[i].xin_id)))
			set_bit(SDE_SSPP_BLOCK_SEC_UI,
					&sde_cfg->sspp[i].features);
	}

	/* this should be updated based on HW rev in future */
	sde_cfg->max_lm_per_display = MAX_LM_PER_DISPLAY;

	if (max_horz_deci)
		sde_cfg->max_display_width = sde_cfg->max_sspp_linewidth *
			max_horz_deci;
	else
		sde_cfg->max_display_width = sde_cfg->max_mixer_width *
			sde_cfg->max_lm_per_display;

	if (max_vert_deci)
		sde_cfg->max_display_height =
			MAX_DISPLAY_HEIGHT_WITH_DECIMATION * max_vert_deci;
	else
		sde_cfg->max_display_height = MAX_DISPLAY_HEIGHT;

	sde_cfg->min_display_height = MIN_DISPLAY_HEIGHT;
	sde_cfg->min_display_width = MIN_DISPLAY_WIDTH;

	return rc;
}

void sde_hw_catalog_deinit(struct sde_mdss_cfg *sde_cfg)
{
	int i;

	if (!sde_cfg)
		return;

	for (i = 0; i < sde_cfg->sspp_count; i++)
		kfree(sde_cfg->sspp[i].sblk);

	for (i = 0; i < sde_cfg->mixer_count; i++)
		kfree(sde_cfg->mixer[i].sblk);

	for (i = 0; i < sde_cfg->wb_count; i++)
		kfree(sde_cfg->wb[i].sblk);

	for (i = 0; i < sde_cfg->dspp_count; i++)
		kfree(sde_cfg->dspp[i].sblk);

	if (sde_cfg->ds_count)
		kfree(sde_cfg->ds[0].top);

	for (i = 0; i < sde_cfg->pingpong_count; i++)
		kfree(sde_cfg->pingpong[i].sblk);

	for (i = 0; i < sde_cfg->vbif_count; i++) {
		kfree(sde_cfg->vbif[i].dynamic_ot_rd_tbl.cfg);
		kfree(sde_cfg->vbif[i].dynamic_ot_wr_tbl.cfg);
		kfree(sde_cfg->vbif[i].qos_rt_tbl.priority_lvl);
		kfree(sde_cfg->vbif[i].qos_nrt_tbl.priority_lvl);
	}

	for (i = 0; i < SDE_QOS_LUT_USAGE_MAX; i++)
		kfree(sde_cfg->perf.qos_lut_tbl[i].entries);

	kfree(sde_cfg->dma_formats);
	kfree(sde_cfg->cursor_formats);
	kfree(sde_cfg->vig_formats);
	kfree(sde_cfg->wb_formats);
	kfree(sde_cfg->virt_vig_formats);

	kfree(sde_cfg);
}

/*************************************************************
 * hardware catalog init
 *************************************************************/
struct sde_mdss_cfg *sde_hw_catalog_init(struct drm_device *dev, u32 hw_rev)
{
	int rc;
	struct sde_mdss_cfg *sde_cfg;
	struct device_node *np = dev->dev->of_node;

	sde_cfg = kzalloc(sizeof(*sde_cfg), GFP_KERNEL);
	if (!sde_cfg)
		return ERR_PTR(-ENOMEM);

	sde_cfg->hwversion = hw_rev;

	rc = _sde_hardware_pre_caps(sde_cfg, hw_rev);
	if (rc)
		goto end;

	rc = sde_parse_dt(np, sde_cfg);
	if (rc)
		goto end;

	rc = sde_perf_parse_dt(np, sde_cfg);
	if (rc)
		goto end;

	rc = sde_rot_parse_dt(np, sde_cfg);
	if (rc)
		goto end;

	rc = sde_ctl_parse_dt(np, sde_cfg);
	if (rc)
		goto end;

	rc = sde_sspp_parse_dt(np, sde_cfg);
	if (rc)
		goto end;

	rc = sde_dspp_top_parse_dt(np, sde_cfg);
	if (rc)
		goto end;

	rc = sde_dspp_parse_dt(np, sde_cfg);
	if (rc)
		goto end;

	rc = sde_ds_parse_dt(np, sde_cfg);
	if (rc)
		goto end;

	rc = sde_dsc_parse_dt(np, sde_cfg);
	if (rc)
		goto end;

	rc = sde_pp_parse_dt(np, sde_cfg);
	if (rc)
		goto end;

	/* mixer parsing should be done after dspp,
	 * ds and pp for mapping setup
	 */
	rc = sde_mixer_parse_dt(np, sde_cfg);
	if (rc)
		goto end;

	rc = sde_intf_parse_dt(np, sde_cfg);
	if (rc)
		goto end;

	rc = sde_wb_parse_dt(np, sde_cfg);
	if (rc)
		goto end;

	/* cdm parsing should be done after intf and wb for mapping setup */
	rc = sde_cdm_parse_dt(np, sde_cfg);
	if (rc)
		goto end;

	rc = sde_vbif_parse_dt(np, sde_cfg);
	if (rc)
		goto end;

	rc = sde_parse_reg_dma_dt(np, sde_cfg);
	if (rc)
		goto end;

	rc = sde_parse_merge_3d_dt(np, sde_cfg);
	if (rc)
		goto end;

	rc = _sde_hardware_post_caps(sde_cfg, hw_rev);
	if (rc)
		goto end;

	return sde_cfg;

end:
	sde_hw_catalog_deinit(sde_cfg);
	return NULL;
}
