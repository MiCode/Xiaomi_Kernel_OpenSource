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
 *
 */

#define pr_fmt(fmt)	"%s:%d: " fmt, __func__, __LINE__

#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/file.h>
#include <linux/delay.h>
#include <linux/debugfs.h>
#include <linux/interrupt.h>
#include <linux/dma-mapping.h>
#include <linux/dma-buf.h>
#include <linux/clk.h>
#include <linux/clk/qcom.h>

#include "sde_rotator_core.h"
#include "sde_rotator_util.h"
#include "sde_rotator_smmu.h"
#include "sde_rotator_r3.h"
#include "sde_rotator_r3_internal.h"
#include "sde_rotator_r3_hwio.h"
#include "sde_rotator_r3_debug.h"
#include "sde_rotator_trace.h"
#include "sde_rotator_debug.h"

#define RES_UHD              (3840*2160)
#define MS_TO_US(t) ((t) * USEC_PER_MSEC)

/* traffic shaping clock ticks = finish_time x 19.2MHz */
#define TRAFFIC_SHAPE_CLKTICK_14MS   268800
#define TRAFFIC_SHAPE_CLKTICK_12MS   230400
#define TRAFFIC_SHAPE_VSYNC_CLK      19200000

/* XIN mapping */
#define XIN_SSPP		0
#define XIN_WRITEBACK		1

/* wait for at most 2 vsync for lowest refresh rate (24hz) */
#define KOFF_TIMEOUT		(42 * 8)

/*
 * When in sbuf mode, select a much longer wait, to allow the other driver
 * to detect timeouts and abort if necessary.
 */
#define KOFF_TIMEOUT_SBUF	(10000)

/* default stream buffer headroom in lines */
#define DEFAULT_SBUF_HEADROOM	20
#define DEFAULT_UBWC_MALSIZE	0
#define DEFAULT_UBWC_SWIZZLE	0

#define DEFAULT_MAXLINEWIDTH	4096

/* stride alignment requirement for avoiding partial writes */
#define PARTIAL_WRITE_ALIGNMENT	0x1F

/* Macro for constructing the REGDMA command */
#define SDE_REGDMA_WRITE(p, off, data) \
	do { \
		SDEROT_DBG("SDEREG.W:[%s:0x%X] <= 0x%X\n", #off, (off),\
				(u32)(data));\
		writel_relaxed_no_log( \
				(REGDMA_OP_REGWRITE | \
				 ((off) & REGDMA_ADDR_OFFSET_MASK)), \
				p); \
		p += sizeof(u32); \
		writel_relaxed_no_log(data, p); \
		p += sizeof(u32); \
	} while (0)

#define SDE_REGDMA_MODIFY(p, off, mask, data) \
	do { \
		SDEROT_DBG("SDEREG.M:[%s:0x%X] <= 0x%X\n", #off, (off),\
				(u32)(data));\
		writel_relaxed_no_log( \
				(REGDMA_OP_REGMODIFY | \
				 ((off) & REGDMA_ADDR_OFFSET_MASK)), \
				p); \
		p += sizeof(u32); \
		writel_relaxed_no_log(mask, p); \
		p += sizeof(u32); \
		writel_relaxed_no_log(data, p); \
		p += sizeof(u32); \
	} while (0)

#define SDE_REGDMA_BLKWRITE_INC(p, off, len) \
	do { \
		SDEROT_DBG("SDEREG.B:[%s:0x%X:0x%X]\n", #off, (off),\
				(u32)(len));\
		writel_relaxed_no_log( \
				(REGDMA_OP_BLKWRITE_INC | \
				 ((off) & REGDMA_ADDR_OFFSET_MASK)), \
				p); \
		p += sizeof(u32); \
		writel_relaxed_no_log(len, p); \
		p += sizeof(u32); \
	} while (0)

#define SDE_REGDMA_BLKWRITE_DATA(p, data) \
	do { \
		SDEROT_DBG("SDEREG.I:[:] <= 0x%X\n", (u32)(data));\
		writel_relaxed_no_log(data, p); \
		p += sizeof(u32); \
	} while (0)

#define SDE_REGDMA_READ(p, data) \
	do { \
		data = readl_relaxed_no_log(p); \
		p += sizeof(u32); \
	} while (0)

/* Macro for directly accessing mapped registers */
#define SDE_ROTREG_WRITE(base, off, data) \
	do { \
		SDEROT_DBG("SDEREG.D:[%s:0x%X] <= 0x%X\n", #off, (off)\
				, (u32)(data));\
		writel_relaxed(data, (base + (off))); \
	} while (0)

#define SDE_ROTREG_READ(base, off) \
	readl_relaxed(base + (off))

#define SDE_ROTTOP_IN_OFFLINE_MODE(_rottop_op_mode_) \
	(((_rottop_op_mode_) & ROTTOP_OP_MODE_ROT_OUT_MASK) == 0)

static const u32 sde_hw_rotator_v3_inpixfmts[] = {
	SDE_PIX_FMT_XRGB_8888,
	SDE_PIX_FMT_ARGB_8888,
	SDE_PIX_FMT_ABGR_8888,
	SDE_PIX_FMT_RGBA_8888,
	SDE_PIX_FMT_BGRA_8888,
	SDE_PIX_FMT_RGBX_8888,
	SDE_PIX_FMT_BGRX_8888,
	SDE_PIX_FMT_XBGR_8888,
	SDE_PIX_FMT_RGBA_5551,
	SDE_PIX_FMT_ARGB_1555,
	SDE_PIX_FMT_ABGR_1555,
	SDE_PIX_FMT_BGRA_5551,
	SDE_PIX_FMT_BGRX_5551,
	SDE_PIX_FMT_RGBX_5551,
	SDE_PIX_FMT_XBGR_1555,
	SDE_PIX_FMT_XRGB_1555,
	SDE_PIX_FMT_ARGB_4444,
	SDE_PIX_FMT_RGBA_4444,
	SDE_PIX_FMT_BGRA_4444,
	SDE_PIX_FMT_ABGR_4444,
	SDE_PIX_FMT_RGBX_4444,
	SDE_PIX_FMT_XRGB_4444,
	SDE_PIX_FMT_BGRX_4444,
	SDE_PIX_FMT_XBGR_4444,
	SDE_PIX_FMT_RGB_888,
	SDE_PIX_FMT_BGR_888,
	SDE_PIX_FMT_RGB_565,
	SDE_PIX_FMT_BGR_565,
	SDE_PIX_FMT_Y_CB_CR_H2V2,
	SDE_PIX_FMT_Y_CR_CB_H2V2,
	SDE_PIX_FMT_Y_CR_CB_GH2V2,
	SDE_PIX_FMT_Y_CBCR_H2V2,
	SDE_PIX_FMT_Y_CRCB_H2V2,
	SDE_PIX_FMT_Y_CBCR_H1V2,
	SDE_PIX_FMT_Y_CRCB_H1V2,
	SDE_PIX_FMT_Y_CBCR_H2V1,
	SDE_PIX_FMT_Y_CRCB_H2V1,
	SDE_PIX_FMT_YCBYCR_H2V1,
	SDE_PIX_FMT_Y_CBCR_H2V2_VENUS,
	SDE_PIX_FMT_Y_CRCB_H2V2_VENUS,
	SDE_PIX_FMT_RGBA_8888_UBWC,
	SDE_PIX_FMT_RGBX_8888_UBWC,
	SDE_PIX_FMT_RGB_565_UBWC,
	SDE_PIX_FMT_Y_CBCR_H2V2_UBWC,
	SDE_PIX_FMT_RGBA_1010102,
	SDE_PIX_FMT_RGBX_1010102,
	SDE_PIX_FMT_ARGB_2101010,
	SDE_PIX_FMT_XRGB_2101010,
	SDE_PIX_FMT_BGRA_1010102,
	SDE_PIX_FMT_BGRX_1010102,
	SDE_PIX_FMT_ABGR_2101010,
	SDE_PIX_FMT_XBGR_2101010,
	SDE_PIX_FMT_RGBA_1010102_UBWC,
	SDE_PIX_FMT_RGBX_1010102_UBWC,
	SDE_PIX_FMT_Y_CBCR_H2V2_P010,
	SDE_PIX_FMT_Y_CBCR_H2V2_TP10,
	SDE_PIX_FMT_Y_CBCR_H2V2_TP10_UBWC,
};

static const u32 sde_hw_rotator_v3_outpixfmts[] = {
	SDE_PIX_FMT_XRGB_8888,
	SDE_PIX_FMT_ARGB_8888,
	SDE_PIX_FMT_ABGR_8888,
	SDE_PIX_FMT_RGBA_8888,
	SDE_PIX_FMT_BGRA_8888,
	SDE_PIX_FMT_RGBX_8888,
	SDE_PIX_FMT_BGRX_8888,
	SDE_PIX_FMT_XBGR_8888,
	SDE_PIX_FMT_RGBA_5551,
	SDE_PIX_FMT_ARGB_1555,
	SDE_PIX_FMT_ABGR_1555,
	SDE_PIX_FMT_BGRA_5551,
	SDE_PIX_FMT_BGRX_5551,
	SDE_PIX_FMT_RGBX_5551,
	SDE_PIX_FMT_XBGR_1555,
	SDE_PIX_FMT_XRGB_1555,
	SDE_PIX_FMT_ARGB_4444,
	SDE_PIX_FMT_RGBA_4444,
	SDE_PIX_FMT_BGRA_4444,
	SDE_PIX_FMT_ABGR_4444,
	SDE_PIX_FMT_RGBX_4444,
	SDE_PIX_FMT_XRGB_4444,
	SDE_PIX_FMT_BGRX_4444,
	SDE_PIX_FMT_XBGR_4444,
	SDE_PIX_FMT_RGB_888,
	SDE_PIX_FMT_BGR_888,
	SDE_PIX_FMT_RGB_565,
	SDE_PIX_FMT_BGR_565,
	/* SDE_PIX_FMT_Y_CB_CR_H2V2 */
	/* SDE_PIX_FMT_Y_CR_CB_H2V2 */
	/* SDE_PIX_FMT_Y_CR_CB_GH2V2 */
	SDE_PIX_FMT_Y_CBCR_H2V2,
	SDE_PIX_FMT_Y_CRCB_H2V2,
	SDE_PIX_FMT_Y_CBCR_H1V2,
	SDE_PIX_FMT_Y_CRCB_H1V2,
	SDE_PIX_FMT_Y_CBCR_H2V1,
	SDE_PIX_FMT_Y_CRCB_H2V1,
	/* SDE_PIX_FMT_YCBYCR_H2V1 */
	SDE_PIX_FMT_Y_CBCR_H2V2_VENUS,
	SDE_PIX_FMT_Y_CRCB_H2V2_VENUS,
	SDE_PIX_FMT_RGBA_8888_UBWC,
	SDE_PIX_FMT_RGBX_8888_UBWC,
	SDE_PIX_FMT_RGB_565_UBWC,
	SDE_PIX_FMT_Y_CBCR_H2V2_UBWC,
	SDE_PIX_FMT_RGBA_1010102,
	SDE_PIX_FMT_RGBX_1010102,
	/* SDE_PIX_FMT_ARGB_2101010 */
	/* SDE_PIX_FMT_XRGB_2101010 */
	SDE_PIX_FMT_BGRA_1010102,
	SDE_PIX_FMT_BGRX_1010102,
	/* SDE_PIX_FMT_ABGR_2101010 */
	/* SDE_PIX_FMT_XBGR_2101010 */
	SDE_PIX_FMT_RGBA_1010102_UBWC,
	SDE_PIX_FMT_RGBX_1010102_UBWC,
	SDE_PIX_FMT_Y_CBCR_H2V2_P010,
	SDE_PIX_FMT_Y_CBCR_H2V2_TP10,
	SDE_PIX_FMT_Y_CBCR_H2V2_TP10_UBWC,
};

static const u32 sde_hw_rotator_v4_inpixfmts[] = {
	SDE_PIX_FMT_XRGB_8888,
	SDE_PIX_FMT_ARGB_8888,
	SDE_PIX_FMT_ABGR_8888,
	SDE_PIX_FMT_RGBA_8888,
	SDE_PIX_FMT_BGRA_8888,
	SDE_PIX_FMT_RGBX_8888,
	SDE_PIX_FMT_BGRX_8888,
	SDE_PIX_FMT_XBGR_8888,
	SDE_PIX_FMT_RGBA_5551,
	SDE_PIX_FMT_ARGB_1555,
	SDE_PIX_FMT_ABGR_1555,
	SDE_PIX_FMT_BGRA_5551,
	SDE_PIX_FMT_BGRX_5551,
	SDE_PIX_FMT_RGBX_5551,
	SDE_PIX_FMT_XBGR_1555,
	SDE_PIX_FMT_XRGB_1555,
	SDE_PIX_FMT_ARGB_4444,
	SDE_PIX_FMT_RGBA_4444,
	SDE_PIX_FMT_BGRA_4444,
	SDE_PIX_FMT_ABGR_4444,
	SDE_PIX_FMT_RGBX_4444,
	SDE_PIX_FMT_XRGB_4444,
	SDE_PIX_FMT_BGRX_4444,
	SDE_PIX_FMT_XBGR_4444,
	SDE_PIX_FMT_RGB_888,
	SDE_PIX_FMT_BGR_888,
	SDE_PIX_FMT_RGB_565,
	SDE_PIX_FMT_BGR_565,
	SDE_PIX_FMT_Y_CB_CR_H2V2,
	SDE_PIX_FMT_Y_CR_CB_H2V2,
	SDE_PIX_FMT_Y_CR_CB_GH2V2,
	SDE_PIX_FMT_Y_CBCR_H2V2,
	SDE_PIX_FMT_Y_CRCB_H2V2,
	SDE_PIX_FMT_Y_CBCR_H1V2,
	SDE_PIX_FMT_Y_CRCB_H1V2,
	SDE_PIX_FMT_Y_CBCR_H2V1,
	SDE_PIX_FMT_Y_CRCB_H2V1,
	SDE_PIX_FMT_YCBYCR_H2V1,
	SDE_PIX_FMT_Y_CBCR_H2V2_VENUS,
	SDE_PIX_FMT_Y_CRCB_H2V2_VENUS,
	SDE_PIX_FMT_RGBA_8888_UBWC,
	SDE_PIX_FMT_RGBX_8888_UBWC,
	SDE_PIX_FMT_RGB_565_UBWC,
	SDE_PIX_FMT_Y_CBCR_H2V2_UBWC,
	SDE_PIX_FMT_RGBA_1010102,
	SDE_PIX_FMT_RGBX_1010102,
	SDE_PIX_FMT_ARGB_2101010,
	SDE_PIX_FMT_XRGB_2101010,
	SDE_PIX_FMT_BGRA_1010102,
	SDE_PIX_FMT_BGRX_1010102,
	SDE_PIX_FMT_ABGR_2101010,
	SDE_PIX_FMT_XBGR_2101010,
	SDE_PIX_FMT_RGBA_1010102_UBWC,
	SDE_PIX_FMT_RGBX_1010102_UBWC,
	SDE_PIX_FMT_Y_CBCR_H2V2_P010,
	SDE_PIX_FMT_Y_CBCR_H2V2_P010_VENUS,
	SDE_PIX_FMT_Y_CBCR_H2V2_TP10,
	SDE_PIX_FMT_Y_CBCR_H2V2_TP10_UBWC,
	SDE_PIX_FMT_Y_CBCR_H2V2_P010_UBWC,
	SDE_PIX_FMT_Y_CBCR_H2V2_P010_TILE,
	SDE_PIX_FMT_Y_CBCR_H2V2_TILE,
	SDE_PIX_FMT_Y_CRCB_H2V2_TILE,
	SDE_PIX_FMT_XRGB_8888_TILE,
	SDE_PIX_FMT_ARGB_8888_TILE,
	SDE_PIX_FMT_ABGR_8888_TILE,
	SDE_PIX_FMT_XBGR_8888_TILE,
	SDE_PIX_FMT_RGBA_8888_TILE,
	SDE_PIX_FMT_BGRA_8888_TILE,
	SDE_PIX_FMT_RGBX_8888_TILE,
	SDE_PIX_FMT_BGRX_8888_TILE,
	SDE_PIX_FMT_RGBA_1010102_TILE,
	SDE_PIX_FMT_RGBX_1010102_TILE,
	SDE_PIX_FMT_ARGB_2101010_TILE,
	SDE_PIX_FMT_XRGB_2101010_TILE,
	SDE_PIX_FMT_BGRA_1010102_TILE,
	SDE_PIX_FMT_BGRX_1010102_TILE,
	SDE_PIX_FMT_ABGR_2101010_TILE,
	SDE_PIX_FMT_XBGR_2101010_TILE,
};

static const u32 sde_hw_rotator_v4_outpixfmts[] = {
	SDE_PIX_FMT_XRGB_8888,
	SDE_PIX_FMT_ARGB_8888,
	SDE_PIX_FMT_ABGR_8888,
	SDE_PIX_FMT_RGBA_8888,
	SDE_PIX_FMT_BGRA_8888,
	SDE_PIX_FMT_RGBX_8888,
	SDE_PIX_FMT_BGRX_8888,
	SDE_PIX_FMT_XBGR_8888,
	SDE_PIX_FMT_RGBA_5551,
	SDE_PIX_FMT_ARGB_1555,
	SDE_PIX_FMT_ABGR_1555,
	SDE_PIX_FMT_BGRA_5551,
	SDE_PIX_FMT_BGRX_5551,
	SDE_PIX_FMT_RGBX_5551,
	SDE_PIX_FMT_XBGR_1555,
	SDE_PIX_FMT_XRGB_1555,
	SDE_PIX_FMT_ARGB_4444,
	SDE_PIX_FMT_RGBA_4444,
	SDE_PIX_FMT_BGRA_4444,
	SDE_PIX_FMT_ABGR_4444,
	SDE_PIX_FMT_RGBX_4444,
	SDE_PIX_FMT_XRGB_4444,
	SDE_PIX_FMT_BGRX_4444,
	SDE_PIX_FMT_XBGR_4444,
	SDE_PIX_FMT_RGB_888,
	SDE_PIX_FMT_BGR_888,
	SDE_PIX_FMT_RGB_565,
	SDE_PIX_FMT_BGR_565,
	/* SDE_PIX_FMT_Y_CB_CR_H2V2 */
	/* SDE_PIX_FMT_Y_CR_CB_H2V2 */
	/* SDE_PIX_FMT_Y_CR_CB_GH2V2 */
	SDE_PIX_FMT_Y_CBCR_H2V2,
	SDE_PIX_FMT_Y_CRCB_H2V2,
	SDE_PIX_FMT_Y_CBCR_H1V2,
	SDE_PIX_FMT_Y_CRCB_H1V2,
	SDE_PIX_FMT_Y_CBCR_H2V1,
	SDE_PIX_FMT_Y_CRCB_H2V1,
	/* SDE_PIX_FMT_YCBYCR_H2V1 */
	SDE_PIX_FMT_Y_CBCR_H2V2_VENUS,
	SDE_PIX_FMT_Y_CRCB_H2V2_VENUS,
	SDE_PIX_FMT_RGBA_8888_UBWC,
	SDE_PIX_FMT_RGBX_8888_UBWC,
	SDE_PIX_FMT_RGB_565_UBWC,
	SDE_PIX_FMT_Y_CBCR_H2V2_UBWC,
	SDE_PIX_FMT_RGBA_1010102,
	SDE_PIX_FMT_RGBX_1010102,
	SDE_PIX_FMT_ARGB_2101010,
	SDE_PIX_FMT_XRGB_2101010,
	SDE_PIX_FMT_BGRA_1010102,
	SDE_PIX_FMT_BGRX_1010102,
	SDE_PIX_FMT_ABGR_2101010,
	SDE_PIX_FMT_XBGR_2101010,
	SDE_PIX_FMT_RGBA_1010102_UBWC,
	SDE_PIX_FMT_RGBX_1010102_UBWC,
	SDE_PIX_FMT_Y_CBCR_H2V2_P010,
	SDE_PIX_FMT_Y_CBCR_H2V2_P010_VENUS,
	SDE_PIX_FMT_Y_CBCR_H2V2_TP10,
	SDE_PIX_FMT_Y_CBCR_H2V2_TP10_UBWC,
	SDE_PIX_FMT_Y_CBCR_H2V2_P010_UBWC,
	SDE_PIX_FMT_Y_CBCR_H2V2_P010_TILE,
	SDE_PIX_FMT_Y_CBCR_H2V2_TILE,
	SDE_PIX_FMT_Y_CRCB_H2V2_TILE,
	SDE_PIX_FMT_XRGB_8888_TILE,
	SDE_PIX_FMT_ARGB_8888_TILE,
	SDE_PIX_FMT_ABGR_8888_TILE,
	SDE_PIX_FMT_XBGR_8888_TILE,
	SDE_PIX_FMT_RGBA_8888_TILE,
	SDE_PIX_FMT_BGRA_8888_TILE,
	SDE_PIX_FMT_RGBX_8888_TILE,
	SDE_PIX_FMT_BGRX_8888_TILE,
	SDE_PIX_FMT_RGBA_1010102_TILE,
	SDE_PIX_FMT_RGBX_1010102_TILE,
	SDE_PIX_FMT_ARGB_2101010_TILE,
	SDE_PIX_FMT_XRGB_2101010_TILE,
	SDE_PIX_FMT_BGRA_1010102_TILE,
	SDE_PIX_FMT_BGRX_1010102_TILE,
	SDE_PIX_FMT_ABGR_2101010_TILE,
	SDE_PIX_FMT_XBGR_2101010_TILE,
};

static const u32 sde_hw_rotator_v4_inpixfmts_sbuf[] = {
	SDE_PIX_FMT_Y_CBCR_H2V2_P010,
	SDE_PIX_FMT_Y_CBCR_H2V2,
	SDE_PIX_FMT_Y_CRCB_H2V2,
	SDE_PIX_FMT_Y_CBCR_H2V2_TP10_UBWC,
	SDE_PIX_FMT_Y_CBCR_H2V2_P010_UBWC,
	SDE_PIX_FMT_Y_CBCR_H2V2_UBWC,
	SDE_PIX_FMT_Y_CBCR_H2V2_TP10,
	SDE_PIX_FMT_Y_CBCR_H2V2_P010_TILE,
	SDE_PIX_FMT_Y_CBCR_H2V2_TILE,
};

static const u32 sde_hw_rotator_v4_outpixfmts_sbuf[] = {
	SDE_PIX_FMT_Y_CBCR_H2V2_TP10,
	SDE_PIX_FMT_Y_CBCR_H2V2_P010_TILE,
	SDE_PIX_FMT_Y_CBCR_H2V2_TILE,
};

static struct sde_rot_vbif_debug_bus nrt_vbif_dbg_bus_r3[] = {
	{0x214, 0x21c, 16, 1, 0x200}, /* arb clients main */
	{0x214, 0x21c, 0, 12, 0x13}, /* xin blocks - axi side */
	{0x21c, 0x214, 0, 12, 0xc}, /* xin blocks - clock side */
};

static struct sde_rot_debug_bus rot_dbgbus_r3[] = {
	/*
	 * rottop - 0xA8850
	 */
	/* REGDMA */
	{ 0XA8850, 0, 0 },
	{ 0XA8850, 0, 1 },
	{ 0XA8850, 0, 2 },
	{ 0XA8850, 0, 3 },
	{ 0XA8850, 0, 4 },

	/* ROT_WB */
	{ 0XA8850, 1, 0 },
	{ 0XA8850, 1, 1 },
	{ 0XA8850, 1, 2 },
	{ 0XA8850, 1, 3 },
	{ 0XA8850, 1, 4 },
	{ 0XA8850, 1, 5 },
	{ 0XA8850, 1, 6 },
	{ 0XA8850, 1, 7 },

	/* UBWC_DEC */
	{ 0XA8850, 2, 0 },

	/* UBWC_ENC */
	{ 0XA8850, 3, 0 },

	/* ROT_FETCH_0 */
	{ 0XA8850, 4, 0 },
	{ 0XA8850, 4, 1 },
	{ 0XA8850, 4, 2 },
	{ 0XA8850, 4, 3 },
	{ 0XA8850, 4, 4 },
	{ 0XA8850, 4, 5 },
	{ 0XA8850, 4, 6 },
	{ 0XA8850, 4, 7 },

	/* ROT_FETCH_1 */
	{ 0XA8850, 5, 0 },
	{ 0XA8850, 5, 1 },
	{ 0XA8850, 5, 2 },
	{ 0XA8850, 5, 3 },
	{ 0XA8850, 5, 4 },
	{ 0XA8850, 5, 5 },
	{ 0XA8850, 5, 6 },
	{ 0XA8850, 5, 7 },

	/* ROT_FETCH_2 */
	{ 0XA8850, 6, 0 },
	{ 0XA8850, 6, 1 },
	{ 0XA8850, 6, 2 },
	{ 0XA8850, 6, 3 },
	{ 0XA8850, 6, 4 },
	{ 0XA8850, 6, 5 },
	{ 0XA8850, 6, 6 },
	{ 0XA8850, 6, 7 },

	/* ROT_FETCH_3 */
	{ 0XA8850, 7, 0 },
	{ 0XA8850, 7, 1 },
	{ 0XA8850, 7, 2 },
	{ 0XA8850, 7, 3 },
	{ 0XA8850, 7, 4 },
	{ 0XA8850, 7, 5 },
	{ 0XA8850, 7, 6 },
	{ 0XA8850, 7, 7 },

	/* ROT_FETCH_4 */
	{ 0XA8850, 8, 0 },
	{ 0XA8850, 8, 1 },
	{ 0XA8850, 8, 2 },
	{ 0XA8850, 8, 3 },
	{ 0XA8850, 8, 4 },
	{ 0XA8850, 8, 5 },
	{ 0XA8850, 8, 6 },
	{ 0XA8850, 8, 7 },

	/* ROT_UNPACK_0*/
	{ 0XA8850, 9, 0 },
	{ 0XA8850, 9, 1 },
	{ 0XA8850, 9, 2 },
	{ 0XA8850, 9, 3 },
};

static struct sde_rot_regdump sde_rot_r3_regdump[] = {
	{ "SDEROT_ROTTOP", SDE_ROT_ROTTOP_OFFSET, 0x100, SDE_ROT_REGDUMP_READ },
	{ "SDEROT_SSPP", SDE_ROT_SSPP_OFFSET, 0x200, SDE_ROT_REGDUMP_READ },
	{ "SDEROT_WB", SDE_ROT_WB_OFFSET, 0x300, SDE_ROT_REGDUMP_READ },
	{ "SDEROT_REGDMA_CSR", SDE_ROT_REGDMA_OFFSET, 0x100,
		SDE_ROT_REGDUMP_READ },
	/*
	 * Need to perform a SW reset to REGDMA in order to access the
	 * REGDMA RAM especially if REGDMA is waiting for Rotator IDLE.
	 * REGDMA RAM should be dump at last.
	 */
	{ "SDEROT_REGDMA_RESET", ROTTOP_SW_RESET_OVERRIDE, 1,
		SDE_ROT_REGDUMP_WRITE, 1 },
	{ "SDEROT_REGDMA_RAM", SDE_ROT_REGDMA_RAM_OFFSET, 0x2000,
		SDE_ROT_REGDUMP_READ },
	{ "SDEROT_VBIF_NRT", SDE_ROT_VBIF_NRT_OFFSET, 0x590,
		SDE_ROT_REGDUMP_VBIF },
	{ "SDEROT_REGDMA_RESET", ROTTOP_SW_RESET_OVERRIDE, 1,
		SDE_ROT_REGDUMP_WRITE, 0 },
};

struct sde_rot_cdp_params {
	bool enable;
	struct sde_mdp_format_params *fmt;
	u32 offset;
};

/* Invalid software timestamp value for initialization */
#define SDE_REGDMA_SWTS_INVALID	(~0)

/**
 * __sde_hw_rotator_get_timestamp - obtain rotator current timestamp
 * @rot: rotator context
 * @q_id: regdma queue id (low/high)
 * @return: current timestmap
 */
static u32 __sde_hw_rotator_get_timestamp(struct sde_hw_rotator *rot, u32 q_id)
{
	struct sde_rot_data_type *mdata = sde_rot_get_mdata();
	u32 ts;

	if (test_bit(SDE_CAPS_HW_TIMESTAMP, mdata->sde_caps_map)) {
		if (q_id == ROT_QUEUE_HIGH_PRIORITY)
			ts = SDE_ROTREG_READ(rot->mdss_base, ROTTOP_ROT_CNTR_0);
		else
			ts = SDE_ROTREG_READ(rot->mdss_base, ROTTOP_ROT_CNTR_1);
	} else {
		ts = SDE_ROTREG_READ(rot->mdss_base, REGDMA_TIMESTAMP_REG);
		if (q_id == ROT_QUEUE_LOW_PRIORITY)
			ts >>= SDE_REGDMA_SWTS_SHIFT;
	}

	return ts & SDE_REGDMA_SWTS_MASK;
}

/**
 * sde_hw_rotator_elapsed_swts - Find difference of 2 software timestamps
 * @ts_curr: current software timestamp
 * @ts_prev: previous software timestamp
 * @return: the amount ts_curr is ahead of ts_prev
 */
static int sde_hw_rotator_elapsed_swts(u32 ts_curr, u32 ts_prev)
{
	u32 diff = (ts_curr - ts_prev) & SDE_REGDMA_SWTS_MASK;

	return sign_extend32(diff, (SDE_REGDMA_SWTS_SHIFT - 1));
}

/**
 * sde_hw_rotator_pending_hwts - Check if the given context is still pending
 * @rot: Pointer to hw rotator
 * @ctx: Pointer to rotator context
 * @phwts: Pointer to returned reference hw timestamp, optional
 * @return: true if context has pending requests
 */
static int sde_hw_rotator_pending_hwts(struct sde_hw_rotator *rot,
		struct sde_hw_rotator_context *ctx, u32 *phwts)
{
	u32 hwts;
	int ts_diff;
	bool pending;

	if (ctx->last_regdma_timestamp == SDE_REGDMA_SWTS_INVALID) {
		if (ctx->q_id == ROT_QUEUE_LOW_PRIORITY)
			hwts = SDE_ROTREG_READ(rot->mdss_base,
					ROTTOP_ROT_CNTR_1);
		else
			hwts = SDE_ROTREG_READ(rot->mdss_base,
					ROTTOP_ROT_CNTR_0);
	} else {
		hwts = ctx->last_regdma_timestamp;
	}

	hwts &= SDE_REGDMA_SWTS_MASK;

	ts_diff = sde_hw_rotator_elapsed_swts(ctx->timestamp, hwts);

	if (phwts)
		*phwts = hwts;

	pending = (ts_diff > 0) ? true : false;

	SDEROT_DBG("ts:0x%x, queue_id:%d, hwts:0x%x, pending:%d\n",
		ctx->timestamp, ctx->q_id, hwts, pending);
	SDEROT_EVTLOG(ctx->timestamp, hwts, ctx->q_id, ts_diff);
	return pending;
}

/**
 * sde_hw_rotator_update_hwts - update hw timestamp with given value
 * @rot: Pointer to hw rotator
 * @q_id: rotator queue id
 * @hwts: new hw timestamp
 */
static void sde_hw_rotator_update_hwts(struct sde_hw_rotator *rot,
		u32 q_id, u32 hwts)
{
	if (q_id == ROT_QUEUE_LOW_PRIORITY)
		SDE_ROTREG_WRITE(rot->mdss_base, ROTTOP_ROT_CNTR_1, hwts);
	else
		SDE_ROTREG_WRITE(rot->mdss_base, ROTTOP_ROT_CNTR_0, hwts);
}

/**
 * sde_hw_rotator_pending_swts - Check if the given context is still pending
 * @rot: Pointer to hw rotator
 * @ctx: Pointer to rotator context
 * @pswts: Pointer to returned reference software timestamp, optional
 * @return: true if context has pending requests
 */
static int sde_hw_rotator_pending_swts(struct sde_hw_rotator *rot,
		struct sde_hw_rotator_context *ctx, u32 *pswts)
{
	u32 swts;
	int ts_diff;
	bool pending;

	if (ctx->last_regdma_timestamp == SDE_REGDMA_SWTS_INVALID)
		swts = SDE_ROTREG_READ(rot->mdss_base, REGDMA_TIMESTAMP_REG);
	else
		swts = ctx->last_regdma_timestamp;

	if (ctx->q_id == ROT_QUEUE_LOW_PRIORITY)
		swts >>= SDE_REGDMA_SWTS_SHIFT;

	swts &= SDE_REGDMA_SWTS_MASK;

	ts_diff = sde_hw_rotator_elapsed_swts(ctx->timestamp, swts);

	if (pswts)
		*pswts = swts;

	pending = (ts_diff > 0) ? true : false;

	SDEROT_DBG("ts:0x%x, queue_id:%d, swts:0x%x, pending:%d\n",
		ctx->timestamp, ctx->q_id, swts, pending);
	SDEROT_EVTLOG(ctx->timestamp, swts, ctx->q_id, ts_diff);
	return pending;
}

/**
 * sde_hw_rotator_update_swts - update software timestamp with given value
 * @rot: Pointer to hw rotator
 * @q_id: rotator queue id
 * @swts: new software timestamp
 */
static void sde_hw_rotator_update_swts(struct sde_hw_rotator *rot,
		u32 q_id, u32 swts)
{
	u32 mask = SDE_REGDMA_SWTS_MASK;

	swts &= SDE_REGDMA_SWTS_MASK;
	if (q_id == ROT_QUEUE_LOW_PRIORITY) {
		swts <<= SDE_REGDMA_SWTS_SHIFT;
		mask <<= SDE_REGDMA_SWTS_SHIFT;
	}

	swts |= (SDE_ROTREG_READ(rot->mdss_base, REGDMA_TIMESTAMP_REG) & ~mask);
	SDE_ROTREG_WRITE(rot->mdss_base, REGDMA_TIMESTAMP_REG, swts);
}

/**
 * sde_hw_rotator_enable_irq - Enable hw rotator interrupt with ref. count
 *				Also, clear rotator/regdma irq status.
 * @rot: Pointer to hw rotator
 */
static void sde_hw_rotator_enable_irq(struct sde_hw_rotator *rot)
{
	SDEROT_DBG("irq_num:%d enabled:%d\n", rot->irq_num,
		atomic_read(&rot->irq_enabled));

	if (!atomic_read(&rot->irq_enabled)) {
		if (rot->mode == ROT_REGDMA_OFF)
			SDE_ROTREG_WRITE(rot->mdss_base, ROTTOP_INTR_CLEAR,
				ROT_DONE_MASK);
		else
			SDE_ROTREG_WRITE(rot->mdss_base,
				REGDMA_CSR_REGDMA_INT_CLEAR, REGDMA_INT_MASK);

		enable_irq(rot->irq_num);
	}
	atomic_inc(&rot->irq_enabled);
}

/**
 * sde_hw_rotator_disable_irq - Disable hw rotator interrupt with ref. count
 *				Also, clear rotator/regdma irq enable masks.
 * @rot: Pointer to hw rotator
 */
static void sde_hw_rotator_disable_irq(struct sde_hw_rotator *rot)
{
	SDEROT_DBG("irq_num:%d enabled:%d\n", rot->irq_num,
		atomic_read(&rot->irq_enabled));

	if (!atomic_read(&rot->irq_enabled)) {
		SDEROT_ERR("irq %d is already disabled\n", rot->irq_num);
		return;
	}

	if (!atomic_dec_return(&rot->irq_enabled)) {
		if (rot->mode == ROT_REGDMA_OFF)
			SDE_ROTREG_WRITE(rot->mdss_base, ROTTOP_INTR_EN, 0);
		else
			SDE_ROTREG_WRITE(rot->mdss_base,
				REGDMA_CSR_REGDMA_INT_EN, 0);
		/* disable irq after last pending irq is handled, if any */
		synchronize_irq(rot->irq_num);
		disable_irq_nosync(rot->irq_num);
	}
}

static void sde_hw_rotator_halt_vbif_xin_client(void)
{
	struct sde_mdp_vbif_halt_params halt_params;

	memset(&halt_params, 0, sizeof(struct sde_mdp_vbif_halt_params));
	halt_params.xin_id = XIN_SSPP;
	halt_params.reg_off_mdp_clk_ctrl = MMSS_VBIF_NRT_VBIF_CLK_FORCE_CTRL0;
	halt_params.bit_off_mdp_clk_ctrl =
		MMSS_VBIF_NRT_VBIF_CLK_FORCE_CTRL0_XIN0;
	sde_mdp_halt_vbif_xin(&halt_params);

	memset(&halt_params, 0, sizeof(struct sde_mdp_vbif_halt_params));
	halt_params.xin_id = XIN_WRITEBACK;
	halt_params.reg_off_mdp_clk_ctrl = MMSS_VBIF_NRT_VBIF_CLK_FORCE_CTRL0;
	halt_params.bit_off_mdp_clk_ctrl =
		MMSS_VBIF_NRT_VBIF_CLK_FORCE_CTRL0_XIN1;
	sde_mdp_halt_vbif_xin(&halt_params);
}

/**
 * sde_hw_rotator_reset - Reset rotator hardware
 * @rot: pointer to hw rotator
 * @ctx: pointer to current rotator context during the hw hang (optional)
 */
static int sde_hw_rotator_reset(struct sde_hw_rotator *rot,
		struct sde_hw_rotator_context *ctx)
{
	struct sde_hw_rotator_context *rctx = NULL;
	u32 int_mask = (REGDMA_INT_0_MASK | REGDMA_INT_1_MASK |
			REGDMA_INT_2_MASK);
	u32 last_ts[ROT_QUEUE_MAX] = {0,};
	u32 latest_ts, opmode;
	int elapsed_time, t;
	int i, j;
	unsigned long flags;

	if (!rot) {
		SDEROT_ERR("NULL rotator\n");
		return -EINVAL;
	}

	/* sw reset the hw rotator */
	SDE_ROTREG_WRITE(rot->mdss_base, ROTTOP_SW_RESET_OVERRIDE, 1);
	/* ensure write is issued to the rotator HW */
	wmb();
	usleep_range(MS_TO_US(10), MS_TO_US(20));

	/* force rotator into offline mode */
	opmode = SDE_ROTREG_READ(rot->mdss_base, ROTTOP_OP_MODE);
	SDE_ROTREG_WRITE(rot->mdss_base, ROTTOP_OP_MODE,
			opmode & ~(BIT(5) | BIT(4) | BIT(1) | BIT(0)));

	SDE_ROTREG_WRITE(rot->mdss_base, ROTTOP_SW_RESET_OVERRIDE, 0);

	/* halt vbif xin client to ensure no pending transaction */
	sde_hw_rotator_halt_vbif_xin_client();

	/* if no ctx is specified, skip ctx wake up */
	if (!ctx)
		return 0;

	if (ctx->q_id >= ROT_QUEUE_MAX) {
		SDEROT_ERR("context q_id out of range: %d\n", ctx->q_id);
		return -EINVAL;
	}

	spin_lock_irqsave(&rot->rotisr_lock, flags);

	/* update timestamp register with current context */
	last_ts[ctx->q_id] = ctx->timestamp;
	rot->ops.update_ts(rot, ctx->q_id, ctx->timestamp);
	SDEROT_EVTLOG(ctx->timestamp);

	/*
	 * Search for any pending rot session, and look for last timestamp
	 * per hw queue.
	 */
	for (i = 0; i < ROT_QUEUE_MAX; i++) {
		latest_ts = atomic_read(&rot->timestamp[i]);
		latest_ts &= SDE_REGDMA_SWTS_MASK;
		elapsed_time = sde_hw_rotator_elapsed_swts(latest_ts,
			last_ts[i]);

		for (j = 0; j < SDE_HW_ROT_REGDMA_TOTAL_CTX; j++) {
			rctx = rot->rotCtx[i][j];
			if (rctx && rctx != ctx) {
				rctx->last_regdma_isr_status = int_mask;
				rctx->last_regdma_timestamp  = rctx->timestamp;

				t = sde_hw_rotator_elapsed_swts(latest_ts,
							rctx->timestamp);
				if (t < elapsed_time) {
					elapsed_time = t;
					last_ts[i] = rctx->timestamp;
					rot->ops.update_ts(rot, i, last_ts[i]);
				}

				SDEROT_DBG("rotctx[%d][%d], ts:%d\n",
						i, j, rctx->timestamp);
				SDEROT_EVTLOG(i, j, rctx->timestamp,
						last_ts[i]);
			}
		}
	}

	/* Finally wakeup all pending rotator context in queue */
	for (i = 0; i < ROT_QUEUE_MAX; i++) {
		for (j = 0; j < SDE_HW_ROT_REGDMA_TOTAL_CTX; j++) {
			rctx = rot->rotCtx[i][j];
			if (rctx && rctx != ctx)
				wake_up_all(&rctx->regdma_waitq);
		}
	}

	spin_unlock_irqrestore(&rot->rotisr_lock, flags);

	return 0;
}

/**
 * _sde_hw_rotator_dump_status - Dump hw rotator status on error
 * @rot: Pointer to hw rotator
 */
static void _sde_hw_rotator_dump_status(struct sde_hw_rotator *rot,
		u32 *ubwcerr)
{
	struct sde_rot_data_type *mdata = sde_rot_get_mdata();
	u32 reg = 0;

	SDEROT_ERR(
		"op_mode = %x, int_en = %x, int_status = %x\n",
		SDE_ROTREG_READ(rot->mdss_base,
			REGDMA_CSR_REGDMA_OP_MODE),
		SDE_ROTREG_READ(rot->mdss_base,
			REGDMA_CSR_REGDMA_INT_EN),
		SDE_ROTREG_READ(rot->mdss_base,
			REGDMA_CSR_REGDMA_INT_STATUS));

	SDEROT_ERR(
		"ts0/ts1 = %x/%x, q0_status = %x, q1_status = %x, block_status = %x\n",
		__sde_hw_rotator_get_timestamp(rot, ROT_QUEUE_HIGH_PRIORITY),
		__sde_hw_rotator_get_timestamp(rot, ROT_QUEUE_LOW_PRIORITY),
		SDE_ROTREG_READ(rot->mdss_base,
			REGDMA_CSR_REGDMA_QUEUE_0_STATUS),
		SDE_ROTREG_READ(rot->mdss_base,
			REGDMA_CSR_REGDMA_QUEUE_1_STATUS),
		SDE_ROTREG_READ(rot->mdss_base,
			REGDMA_CSR_REGDMA_BLOCK_STATUS));

	SDEROT_ERR(
		"invalid_cmd_offset = %x, fsm_state = %x\n",
		SDE_ROTREG_READ(rot->mdss_base,
			REGDMA_CSR_REGDMA_INVALID_CMD_RAM_OFFSET),
		SDE_ROTREG_READ(rot->mdss_base,
			REGDMA_CSR_REGDMA_FSM_STATE));

	SDEROT_ERR("rottop: op_mode = %x, status = %x, clk_status = %x\n",
		SDE_ROTREG_READ(rot->mdss_base, ROTTOP_OP_MODE),
		SDE_ROTREG_READ(rot->mdss_base, ROTTOP_STATUS),
		SDE_ROTREG_READ(rot->mdss_base, ROTTOP_CLK_STATUS));

	reg = SDE_ROTREG_READ(rot->mdss_base, ROT_SSPP_UBWC_ERROR_STATUS);
	if (ubwcerr)
		*ubwcerr = reg;
	SDEROT_ERR(
		"UBWC decode status = %x, UBWC encode status = %x\n", reg,
		SDE_ROTREG_READ(rot->mdss_base, ROT_WB_UBWC_ERROR_STATUS));

	SDEROT_ERR("VBIF XIN HALT status = %x VBIF AXI HALT status = %x\n",
		SDE_VBIF_READ(mdata, MMSS_VBIF_XIN_HALT_CTRL1),
		SDE_VBIF_READ(mdata, MMSS_VBIF_AXI_HALT_CTRL1));

	SDEROT_ERR("sspp unpack wr: plane0 = %x, plane1 = %x, plane2 = %x\n",
			SDE_ROTREG_READ(rot->mdss_base,
				ROT_SSPP_FETCH_SMP_WR_PLANE0),
			SDE_ROTREG_READ(rot->mdss_base,
				ROT_SSPP_FETCH_SMP_WR_PLANE1),
			SDE_ROTREG_READ(rot->mdss_base,
				ROT_SSPP_FETCH_SMP_WR_PLANE2));
	SDEROT_ERR("sspp unpack rd: plane0 = %x, plane1 = %x, plane2 = %x\n",
			SDE_ROTREG_READ(rot->mdss_base,
					ROT_SSPP_SMP_UNPACK_RD_PLANE0),
			SDE_ROTREG_READ(rot->mdss_base,
					ROT_SSPP_SMP_UNPACK_RD_PLANE1),
			SDE_ROTREG_READ(rot->mdss_base,
					ROT_SSPP_SMP_UNPACK_RD_PLANE2));
	SDEROT_ERR("sspp: unpack_ln = %x, unpack_blk = %x, fill_lvl = %x\n",
			SDE_ROTREG_READ(rot->mdss_base,
				ROT_SSPP_UNPACK_LINE_COUNT),
			SDE_ROTREG_READ(rot->mdss_base,
				ROT_SSPP_UNPACK_BLK_COUNT),
			SDE_ROTREG_READ(rot->mdss_base,
				ROT_SSPP_FILL_LEVELS));

	SDEROT_ERR("wb: sbuf0 = %x, sbuf1 = %x, sys_cache = %x\n",
			SDE_ROTREG_READ(rot->mdss_base,
				ROT_WB_SBUF_STATUS_PLANE0),
			SDE_ROTREG_READ(rot->mdss_base,
				ROT_WB_SBUF_STATUS_PLANE1),
			SDE_ROTREG_READ(rot->mdss_base,
				ROT_WB_SYS_CACHE_MODE));
}

/**
 * sde_hw_rotator_get_ctx(): Retrieve rotator context from rotator HW based
 * on provided session_id. Each rotator has a different session_id.
 * @rot: Pointer to rotator hw
 * @session_id: Identifier for rotator session
 * @sequence_id: Identifier for rotation request within the session
 * @q_id: Rotator queue identifier
 */
static struct sde_hw_rotator_context *sde_hw_rotator_get_ctx(
		struct sde_hw_rotator *rot, u32 session_id, u32 sequence_id,
		enum sde_rot_queue_prio q_id)
{
	int i;
	struct sde_hw_rotator_context  *ctx = NULL;

	for (i = 0; i < SDE_HW_ROT_REGDMA_TOTAL_CTX; i++) {
		ctx = rot->rotCtx[q_id][i];

		if (ctx && (ctx->session_id == session_id) &&
				(ctx->sequence_id == sequence_id)) {
			SDEROT_DBG(
				"rotCtx sloti[%d][%d] ==> ctx:%p | session-id:%d | sequence-id:%d\n",
				q_id, i, ctx, ctx->session_id,
				ctx->sequence_id);
			return ctx;
		}
	}

	return NULL;
}

/*
 * sde_hw_rotator_map_vaddr - map the debug buffer to kernel space
 * @dbgbuf: Pointer to debug buffer
 * @buf: Pointer to layer buffer structure
 * @data: Pointer to h/w mapped buffer structure
 */
static void sde_hw_rotator_map_vaddr(struct sde_dbg_buf *dbgbuf,
		struct sde_layer_buffer *buf, struct sde_mdp_data *data)
{
	dbgbuf->dmabuf = data->p[0].srcp_dma_buf;
	dbgbuf->buflen = data->p[0].srcp_dma_buf->size;

	dbgbuf->vaddr  = NULL;
	dbgbuf->width  = buf->width;
	dbgbuf->height = buf->height;

	if (dbgbuf->dmabuf && (dbgbuf->buflen > 0)) {
		dma_buf_begin_cpu_access(dbgbuf->dmabuf, DMA_FROM_DEVICE);
		dbgbuf->vaddr = dma_buf_kmap(dbgbuf->dmabuf, 0);
		SDEROT_DBG("vaddr mapping: 0x%p/%ld w:%d/h:%d\n",
				dbgbuf->vaddr, dbgbuf->buflen,
				dbgbuf->width, dbgbuf->height);
	}
}

/*
 * sde_hw_rotator_unmap_vaddr - unmap the debug buffer from kernel space
 * @dbgbuf: Pointer to debug buffer
 */
static void sde_hw_rotator_unmap_vaddr(struct sde_dbg_buf *dbgbuf)
{
	if (dbgbuf->vaddr) {
		dma_buf_kunmap(dbgbuf->dmabuf, 0, dbgbuf->vaddr);
		dma_buf_end_cpu_access(dbgbuf->dmabuf, DMA_FROM_DEVICE);
	}

	dbgbuf->vaddr  = NULL;
	dbgbuf->dmabuf = NULL;
	dbgbuf->buflen = 0;
	dbgbuf->width  = 0;
	dbgbuf->height = 0;
}

/*
 * sde_hw_rotator_vbif_setting - helper function to set vbif QoS remapper
 * levels, enable write gather enable and avoid clk gating setting for
 * debug purpose.
 *
 * @rot: Pointer to rotator hw
 */
static void sde_hw_rotator_vbif_setting(struct sde_hw_rotator *rot)
{
	u32 i, mask, vbif_qos, reg_val = 0;
	struct sde_rot_data_type *mdata = sde_rot_get_mdata();

	/* VBIF_ROT QoS remapper setting */
	switch (mdata->npriority_lvl) {

	case SDE_MDP_VBIF_4_LEVEL_REMAPPER:
		for (i = 0; i < mdata->npriority_lvl; i++) {
			reg_val = SDE_VBIF_READ(mdata,
					MMSS_VBIF_NRT_VBIF_QOS_REMAP_00 + i*4);
			mask = 0x3 << (XIN_SSPP * 2);
			vbif_qos = mdata->vbif_nrt_qos[i];
			reg_val |= vbif_qos << (XIN_SSPP * 2);
			/* ensure write is issued after the read operation */
			mb();
			SDE_VBIF_WRITE(mdata,
					MMSS_VBIF_NRT_VBIF_QOS_REMAP_00 + i*4,
					reg_val);
		}
		break;

	case SDE_MDP_VBIF_8_LEVEL_REMAPPER:
		mask = mdata->npriority_lvl - 1;
		for (i = 0; i < mdata->npriority_lvl; i++) {
			/* RD and WR client */
			reg_val |= (mdata->vbif_nrt_qos[i] & mask)
							<< (XIN_SSPP * 4);
			reg_val |= (mdata->vbif_nrt_qos[i] & mask)
							<< (XIN_WRITEBACK * 4);

			SDE_VBIF_WRITE(mdata,
				MMSS_VBIF_NRT_VBIF_QOS_RP_REMAP_000 + i*8,
				reg_val);
			SDE_VBIF_WRITE(mdata,
				MMSS_VBIF_NRT_VBIF_QOS_LVL_REMAP_000 + i*8,
				reg_val);
		}
		break;

	default:
		SDEROT_DBG("invalid vbif remapper levels\n");
	}

	/* Enable write gather for writeback to remove write gaps, which
	 * may hang AXI/BIMC/SDE.
	 */
	SDE_VBIF_WRITE(mdata, MMSS_VBIF_NRT_VBIF_WRITE_GATHTER_EN,
			BIT(XIN_WRITEBACK));

	/*
	 * For debug purpose, disable clock gating, i.e. Clocks always on
	 */
	if (mdata->clk_always_on) {
		SDE_VBIF_WRITE(mdata, MMSS_VBIF_CLKON, 0x3);
		SDE_VBIF_WRITE(mdata, MMSS_VBIF_NRT_VBIF_CLK_FORCE_CTRL0, 0x3);
		SDE_VBIF_WRITE(mdata, MMSS_VBIF_NRT_VBIF_CLK_FORCE_CTRL1,
				0xFFFF);
		SDE_ROTREG_WRITE(rot->mdss_base, ROTTOP_CLK_CTRL, 1);
	}
}

/*
 * sde_hw_rotator_setup_timestamp_packet - setup timestamp writeback command
 * @ctx: Pointer to rotator context
 * @mask: Bit mask location of the timestamp
 * @swts: Software timestamp
 */
static void sde_hw_rotator_setup_timestamp_packet(
		struct sde_hw_rotator_context *ctx, u32 mask, u32 swts)
{
	char __iomem *wrptr;

	wrptr = sde_hw_rotator_get_regdma_segment(ctx);

	/*
	 * Create a dummy packet write out to 1 location for timestamp
	 * generation.
	 */
	SDE_REGDMA_BLKWRITE_INC(wrptr, ROT_SSPP_SRC_SIZE, 6);
	SDE_REGDMA_BLKWRITE_DATA(wrptr, 0x00010001);
	SDE_REGDMA_BLKWRITE_DATA(wrptr, 0);
	SDE_REGDMA_BLKWRITE_DATA(wrptr, 0);
	SDE_REGDMA_BLKWRITE_DATA(wrptr, 0x00010001);
	SDE_REGDMA_BLKWRITE_DATA(wrptr, 0);
	SDE_REGDMA_BLKWRITE_DATA(wrptr, ctx->ts_addr);
	SDE_REGDMA_WRITE(wrptr, ROT_SSPP_SRC_YSTRIDE0, 4);
	SDE_REGDMA_BLKWRITE_INC(wrptr, ROT_SSPP_SRC_FORMAT, 4);
	SDE_REGDMA_BLKWRITE_DATA(wrptr, 0x004037FF);
	SDE_REGDMA_BLKWRITE_DATA(wrptr, 0x03020100);
	SDE_REGDMA_BLKWRITE_DATA(wrptr, 0x80000000);
	SDE_REGDMA_BLKWRITE_DATA(wrptr, ctx->timestamp);
	/*
	 * Must clear secure buffer setting for SW timestamp because
	 * SW timstamp buffer allocation is always non-secure region.
	 */
	if (ctx->is_secure) {
		SDE_REGDMA_WRITE(wrptr, ROT_SSPP_SRC_ADDR_SW_STATUS, 0);
		SDE_REGDMA_WRITE(wrptr, ROT_WB_DST_ADDR_SW_STATUS, 0);
	}
	SDE_REGDMA_BLKWRITE_INC(wrptr, ROT_WB_DST_FORMAT, 4);
	SDE_REGDMA_BLKWRITE_DATA(wrptr, 0x000037FF);
	SDE_REGDMA_BLKWRITE_DATA(wrptr, 0);
	SDE_REGDMA_BLKWRITE_DATA(wrptr, 0x03020100);
	SDE_REGDMA_BLKWRITE_DATA(wrptr, ctx->ts_addr);
	SDE_REGDMA_WRITE(wrptr, ROT_WB_DST_YSTRIDE0, 4);
	SDE_REGDMA_WRITE(wrptr, ROT_WB_OUT_SIZE, 0x00010001);
	SDE_REGDMA_WRITE(wrptr, ROT_WB_OUT_IMG_SIZE, 0x00010001);
	SDE_REGDMA_WRITE(wrptr, ROT_WB_OUT_XY, 0);
	SDE_REGDMA_WRITE(wrptr, ROT_WB_DST_WRITE_CONFIG,
			(ctx->rot->highest_bank & 0x3) << 8);
	SDE_REGDMA_WRITE(wrptr, ROTTOP_DNSC, 0);
	SDE_REGDMA_WRITE(wrptr, ROTTOP_OP_MODE, 1);
	SDE_REGDMA_MODIFY(wrptr, REGDMA_TIMESTAMP_REG, mask, swts);
	SDE_REGDMA_WRITE(wrptr, ROTTOP_START_CTRL, 1);

	sde_hw_rotator_put_regdma_segment(ctx, wrptr);
}

/*
 * sde_hw_rotator_cdp_configs - configures the CDP registers
 * @ctx: Pointer to rotator context
 * @params: Pointer to parameters needed for CDP configs
 */
static void sde_hw_rotator_cdp_configs(struct sde_hw_rotator_context *ctx,
		struct sde_rot_cdp_params *params)
{
	int reg_val;
	char __iomem *wrptr = sde_hw_rotator_get_regdma_segment(ctx);

	if (!params->enable) {
		SDE_REGDMA_WRITE(wrptr, params->offset, 0x0);
		goto end;
	}

	reg_val = BIT(0); /* enable cdp */

	if (sde_mdp_is_ubwc_format(params->fmt))
		reg_val |= BIT(1); /* enable UBWC meta cdp */

	if (sde_mdp_is_ubwc_format(params->fmt)
			|| sde_mdp_is_tilea4x_format(params->fmt)
			|| sde_mdp_is_tilea5x_format(params->fmt))
		reg_val |= BIT(2); /* enable tile amortize */

	reg_val |= BIT(3); /* enable preload addr ahead cnt 64 */

	SDE_REGDMA_WRITE(wrptr, params->offset, reg_val);

end:
	sde_hw_rotator_put_regdma_segment(ctx, wrptr);
}

/*
 * sde_hw_rotator_setup_qos_lut_wr - Set QoS LUT/Danger LUT/Safe LUT configs
 * for the WRITEBACK rotator for inline and offline rotation.
 *
 * @ctx: Pointer to rotator context
 */
static void sde_hw_rotator_setup_qos_lut_wr(struct sde_hw_rotator_context *ctx)
{
	struct sde_rot_data_type *mdata = sde_rot_get_mdata();
	char __iomem *wrptr = sde_hw_rotator_get_regdma_segment(ctx);

	/* Offline rotation setting */
	if (!ctx->sbuf_mode) {
		/* QOS LUT WR setting */
		if (test_bit(SDE_QOS_LUT, mdata->sde_qos_map)) {
			SDE_REGDMA_WRITE(wrptr, ROT_WB_CREQ_LUT_0,
					mdata->lut_cfg[SDE_ROT_WR].creq_lut_0);
			SDE_REGDMA_WRITE(wrptr, ROT_WB_CREQ_LUT_1,
					mdata->lut_cfg[SDE_ROT_WR].creq_lut_1);
		}

		/* Danger LUT WR setting */
		if (test_bit(SDE_QOS_DANGER_LUT, mdata->sde_qos_map))
			SDE_REGDMA_WRITE(wrptr, ROT_WB_DANGER_LUT,
					mdata->lut_cfg[SDE_ROT_WR].danger_lut);

		/* Safe LUT WR setting */
		if (test_bit(SDE_QOS_SAFE_LUT, mdata->sde_qos_map))
			SDE_REGDMA_WRITE(wrptr, ROT_WB_SAFE_LUT,
					mdata->lut_cfg[SDE_ROT_WR].safe_lut);

	/* Inline rotation setting */
	} else {
		/* QOS LUT WR setting */
		if (test_bit(SDE_INLINE_QOS_LUT, mdata->sde_inline_qos_map)) {
			SDE_REGDMA_WRITE(wrptr, ROT_WB_CREQ_LUT_0,
				mdata->inline_lut_cfg[SDE_ROT_WR].creq_lut_0);
			SDE_REGDMA_WRITE(wrptr, ROT_WB_CREQ_LUT_1,
				mdata->inline_lut_cfg[SDE_ROT_WR].creq_lut_1);
		}

		/* Danger LUT WR setting */
		if (test_bit(SDE_INLINE_QOS_DANGER_LUT,
					mdata->sde_inline_qos_map))
			SDE_REGDMA_WRITE(wrptr, ROT_WB_DANGER_LUT,
				mdata->inline_lut_cfg[SDE_ROT_WR].danger_lut);

		/* Safe LUT WR setting */
		if (test_bit(SDE_INLINE_QOS_SAFE_LUT,
					mdata->sde_inline_qos_map))
			SDE_REGDMA_WRITE(wrptr, ROT_WB_SAFE_LUT,
				mdata->inline_lut_cfg[SDE_ROT_WR].safe_lut);
	}

	/* Update command queue write ptr */
	sde_hw_rotator_put_regdma_segment(ctx, wrptr);
}

/*
 * sde_hw_rotator_setup_qos_lut_rd - Set QoS LUT/Danger LUT/Safe LUT configs
 * for the SSPP rotator for inline and offline rotation.
 *
 * @ctx: Pointer to rotator context
 */
static void sde_hw_rotator_setup_qos_lut_rd(struct sde_hw_rotator_context *ctx)
{
	struct sde_rot_data_type *mdata = sde_rot_get_mdata();
	char __iomem *wrptr = sde_hw_rotator_get_regdma_segment(ctx);

	/* Offline rotation setting */
	if (!ctx->sbuf_mode) {
		/* QOS LUT RD setting */
		if (test_bit(SDE_QOS_LUT, mdata->sde_qos_map)) {
			SDE_REGDMA_WRITE(wrptr, ROT_SSPP_CREQ_LUT_0,
					mdata->lut_cfg[SDE_ROT_RD].creq_lut_0);
			SDE_REGDMA_WRITE(wrptr, ROT_SSPP_CREQ_LUT_1,
					mdata->lut_cfg[SDE_ROT_RD].creq_lut_1);
		}

		/* Danger LUT RD setting */
		if (test_bit(SDE_QOS_DANGER_LUT, mdata->sde_qos_map))
			SDE_REGDMA_WRITE(wrptr, ROT_SSPP_DANGER_LUT,
					mdata->lut_cfg[SDE_ROT_RD].danger_lut);

		/* Safe LUT RD setting */
		if (test_bit(SDE_QOS_SAFE_LUT, mdata->sde_qos_map))
			SDE_REGDMA_WRITE(wrptr, ROT_SSPP_SAFE_LUT,
					mdata->lut_cfg[SDE_ROT_RD].safe_lut);

	/* inline rotation setting */
	} else {
		/* QOS LUT RD setting */
		if (test_bit(SDE_INLINE_QOS_LUT, mdata->sde_inline_qos_map)) {
			SDE_REGDMA_WRITE(wrptr, ROT_SSPP_CREQ_LUT_0,
				mdata->inline_lut_cfg[SDE_ROT_RD].creq_lut_0);
			SDE_REGDMA_WRITE(wrptr, ROT_SSPP_CREQ_LUT_1,
				mdata->inline_lut_cfg[SDE_ROT_RD].creq_lut_1);
		}

		/* Danger LUT RD setting */
		if (test_bit(SDE_INLINE_QOS_DANGER_LUT,
					mdata->sde_inline_qos_map))
			SDE_REGDMA_WRITE(wrptr, ROT_SSPP_DANGER_LUT,
				mdata->inline_lut_cfg[SDE_ROT_RD].danger_lut);

		/* Safe LUT RD setting */
		if (test_bit(SDE_INLINE_QOS_SAFE_LUT,
					mdata->sde_inline_qos_map))
			SDE_REGDMA_WRITE(wrptr, ROT_SSPP_SAFE_LUT,
				mdata->inline_lut_cfg[SDE_ROT_RD].safe_lut);
	}

	/* Update command queue write ptr */
	sde_hw_rotator_put_regdma_segment(ctx, wrptr);
}

/*
 * sde_hw_rotator_setup_fetchengine - setup fetch engine
 * @ctx: Pointer to rotator context
 * @queue_id: Priority queue identifier
 * @cfg: Fetch configuration
 * @danger_lut: real-time QoS LUT for danger setting (not used)
 * @safe_lut: real-time QoS LUT for safe setting (not used)
 * @dnsc_factor_w: downscale factor for width
 * @dnsc_factor_h: downscale factor for height
 * @flags: Control flag
 */
static void sde_hw_rotator_setup_fetchengine(struct sde_hw_rotator_context *ctx,
		enum sde_rot_queue_prio queue_id,
		struct sde_hw_rot_sspp_cfg *cfg, u32 danger_lut, u32 safe_lut,
		u32 dnsc_factor_w, u32 dnsc_factor_h, u32 flags)
{
	struct sde_hw_rotator *rot = ctx->rot;
	struct sde_mdp_format_params *fmt;
	struct sde_mdp_data *data;
	struct sde_rot_cdp_params cdp_params = {0};
	struct sde_rot_data_type *mdata = sde_rot_get_mdata();
	char __iomem *wrptr;
	u32 opmode = 0;
	u32 chroma_samp = 0;
	u32 src_format = 0;
	u32 unpack = 0;
	u32 width = cfg->img_width;
	u32 height = cfg->img_height;
	u32 fetch_blocksize = 0;
	int i;

	if (ctx->rot->mode == ROT_REGDMA_ON) {
		if (rot->irq_num >= 0)
			SDE_ROTREG_WRITE(rot->mdss_base,
					REGDMA_CSR_REGDMA_INT_EN,
					REGDMA_INT_MASK);
		SDE_ROTREG_WRITE(rot->mdss_base, REGDMA_CSR_REGDMA_OP_MODE,
				REGDMA_EN);
	}

	wrptr = sde_hw_rotator_get_regdma_segment(ctx);

	/*
	 * initialize start control trigger selection first
	 */
	if (test_bit(SDE_CAPS_SBUF_1, mdata->sde_caps_map)) {
		if (ctx->sbuf_mode)
			SDE_REGDMA_WRITE(wrptr, ROTTOP_START_CTRL,
					ctx->start_ctrl);
		else
			SDE_REGDMA_WRITE(wrptr, ROTTOP_START_CTRL, 0);
	}

	/* source image setup */
	if ((flags & SDE_ROT_FLAG_DEINTERLACE)
			&& !(flags & SDE_ROT_FLAG_SOURCE_ROTATED_90)) {
		for (i = 0; i < cfg->src_plane.num_planes; i++)
			cfg->src_plane.ystride[i] *= 2;
		width *= 2;
		height /= 2;
	}

	/*
	 * REGDMA BLK write from SRC_SIZE to OP_MODE, total 15 registers
	 */
	SDE_REGDMA_BLKWRITE_INC(wrptr, ROT_SSPP_SRC_SIZE, 15);

	/* SRC_SIZE, SRC_IMG_SIZE, SRC_XY, OUT_SIZE, OUT_XY */
	SDE_REGDMA_BLKWRITE_DATA(wrptr,
			cfg->src_rect->w | (cfg->src_rect->h << 16));
	SDE_REGDMA_BLKWRITE_DATA(wrptr, 0); /* SRC_IMG_SIZE unused */
	SDE_REGDMA_BLKWRITE_DATA(wrptr,
			cfg->src_rect->x | (cfg->src_rect->y << 16));
	SDE_REGDMA_BLKWRITE_DATA(wrptr,
			cfg->src_rect->w | (cfg->src_rect->h << 16));
	SDE_REGDMA_BLKWRITE_DATA(wrptr,
			cfg->src_rect->x | (cfg->src_rect->y << 16));

	/* SRC_ADDR [0-3], SRC_YSTRIDE [0-1] */
	data = cfg->data;
	for (i = 0; i < SDE_ROT_MAX_PLANES; i++)
		SDE_REGDMA_BLKWRITE_DATA(wrptr, data->p[i].addr);
	SDE_REGDMA_BLKWRITE_DATA(wrptr, cfg->src_plane.ystride[0] |
			(cfg->src_plane.ystride[1] << 16));
	SDE_REGDMA_BLKWRITE_DATA(wrptr, cfg->src_plane.ystride[2] |
			(cfg->src_plane.ystride[3] << 16));

	/* UNUSED, write 0 */
	SDE_REGDMA_BLKWRITE_DATA(wrptr, 0);

	/* setup source format */
	fmt = cfg->fmt;

	chroma_samp = fmt->chroma_sample;
	if (flags & SDE_ROT_FLAG_SOURCE_ROTATED_90) {
		if (chroma_samp == SDE_MDP_CHROMA_H2V1)
			chroma_samp = SDE_MDP_CHROMA_H1V2;
		else if (chroma_samp == SDE_MDP_CHROMA_H1V2)
			chroma_samp = SDE_MDP_CHROMA_H2V1;
	}

	src_format = (chroma_samp << 23)   |
		(fmt->fetch_planes << 19)  |
		(fmt->bits[C3_ALPHA] << 6) |
		(fmt->bits[C2_R_Cr] << 4)  |
		(fmt->bits[C1_B_Cb] << 2)  |
		(fmt->bits[C0_G_Y] << 0);

	if (fmt->alpha_enable &&
			(fmt->fetch_planes == SDE_MDP_PLANE_INTERLEAVED))
		src_format |= BIT(8); /* SRCC3_EN */

	src_format |= ((fmt->unpack_count - 1) << 12) |
			(fmt->unpack_tight << 17)       |
			(fmt->unpack_align_msb << 18)   |
			((fmt->bpp - 1) << 9)           |
			((fmt->frame_format & 3) << 30);

	if (flags & SDE_ROT_FLAG_ROT_90)
		src_format |= BIT(11);	/* ROT90 */

	if (sde_mdp_is_ubwc_format(fmt))
		opmode |= BIT(0); /* BWC_DEC_EN */

	/* if this is YUV pixel format, enable CSC */
	if (sde_mdp_is_yuv_format(fmt))
		src_format |= BIT(15); /* SRC_COLOR_SPACE */

	if (fmt->pixel_mode == SDE_MDP_PIXEL_10BIT)
		src_format |= BIT(14); /* UNPACK_DX_FORMAT */

	if (rot->solid_fill)
		src_format |= BIT(22); /* SOLID_FILL */

	/* SRC_FORMAT */
	SDE_REGDMA_BLKWRITE_DATA(wrptr, src_format);

	/* setup source unpack pattern */
	unpack = (fmt->element[3] << 24) | (fmt->element[2] << 16) |
		 (fmt->element[1] << 8)  | (fmt->element[0] << 0);

	/* SRC_UNPACK_PATTERN */
	SDE_REGDMA_BLKWRITE_DATA(wrptr, unpack);

	/* setup source op mode */
	if (flags & SDE_ROT_FLAG_FLIP_LR)
		opmode |= BIT(13); /* FLIP_MODE L/R horizontal flip */
	if (flags & SDE_ROT_FLAG_FLIP_UD)
		opmode |= BIT(14); /* FLIP_MODE U/D vertical flip */
	opmode |= BIT(31); /* MDSS_MDP_OP_PE_OVERRIDE */

	/* SRC_OP_MODE */
	SDE_REGDMA_BLKWRITE_DATA(wrptr, opmode);

	/* setup source fetch config, TP10 uses different block size */
	if (test_bit(SDE_CAPS_R3_1P5_DOWNSCALE, mdata->sde_caps_map) &&
			(dnsc_factor_w == 1) && (dnsc_factor_h == 1)) {
		if (sde_mdp_is_tp10_format(fmt))
			fetch_blocksize = SDE_ROT_SSPP_FETCH_BLOCKSIZE_144_EXT;
		else
			fetch_blocksize = SDE_ROT_SSPP_FETCH_BLOCKSIZE_192_EXT;
	} else {
		if (sde_mdp_is_tp10_format(fmt))
			fetch_blocksize = SDE_ROT_SSPP_FETCH_BLOCKSIZE_96;
		else
			fetch_blocksize = SDE_ROT_SSPP_FETCH_BLOCKSIZE_128;
	}

	if (rot->solid_fill)
		SDE_REGDMA_WRITE(wrptr, ROT_SSPP_SRC_CONSTANT_COLOR,
				rot->constant_color);

	SDE_REGDMA_WRITE(wrptr, ROT_SSPP_FETCH_CONFIG,
			fetch_blocksize |
			SDE_ROT_SSPP_FETCH_CONFIG_RESET_VALUE |
			((rot->highest_bank & 0x3) << 18));

	if (test_bit(SDE_CAPS_UBWC_2, mdata->sde_caps_map))
		SDE_REGDMA_WRITE(wrptr, ROT_SSPP_UBWC_STATIC_CTRL, BIT(31) |
				((ctx->rot->ubwc_malsize & 0x3) << 8) |
				((ctx->rot->highest_bank & 0x3) << 4) |
				((ctx->rot->ubwc_swizzle & 0x1) << 0));
	else if (test_bit(SDE_CAPS_UBWC_3, mdata->sde_caps_map))
		SDE_REGDMA_WRITE(wrptr, ROT_SSPP_UBWC_STATIC_CTRL, BIT(30));

	/* setup source buffer plane security status */
	if (flags & (SDE_ROT_FLAG_SECURE_OVERLAY_SESSION |
			SDE_ROT_FLAG_SECURE_CAMERA_SESSION)) {
		SDE_REGDMA_WRITE(wrptr, ROT_SSPP_SRC_ADDR_SW_STATUS, 0xF);
		ctx->is_secure = true;
	} else {
		SDE_REGDMA_WRITE(wrptr, ROT_SSPP_SRC_ADDR_SW_STATUS, 0);
		ctx->is_secure = false;
	}

	/* Update command queue write ptr */
	sde_hw_rotator_put_regdma_segment(ctx, wrptr);

	/* CDP register RD setting */
	cdp_params.enable = test_bit(SDE_QOS_CDP, mdata->sde_qos_map) ?
					 mdata->enable_cdp[SDE_ROT_RD] : false;
	cdp_params.fmt = fmt;
	cdp_params.offset = ROT_SSPP_CDP_CNTL;
	sde_hw_rotator_cdp_configs(ctx, &cdp_params);

	/* QOS LUT/ Danger LUT/ Safe Lut WR setting */
	sde_hw_rotator_setup_qos_lut_rd(ctx);

	wrptr = sde_hw_rotator_get_regdma_segment(ctx);

	/*
	 * Determine if traffic shaping is required. Only enable traffic
	 * shaping when content is 4k@30fps. The actual traffic shaping
	 * bandwidth calculation is done in output setup.
	 */
	if (((!ctx->sbuf_mode)
			&& (cfg->src_rect->w * cfg->src_rect->h) >= RES_UHD)
			&& (cfg->fps <= 30)) {
		SDEROT_DBG("Enable Traffic Shaper\n");
		ctx->is_traffic_shaping = true;
	} else {
		SDEROT_DBG("Disable Traffic Shaper\n");
		ctx->is_traffic_shaping = false;
	}

	/* Update command queue write ptr */
	sde_hw_rotator_put_regdma_segment(ctx, wrptr);
}

/*
 * sde_hw_rotator_setup_wbengine - setup writeback engine
 * @ctx: Pointer to rotator context
 * @queue_id: Priority queue identifier
 * @cfg: Writeback configuration
 * @flags: Control flag
 */
static void sde_hw_rotator_setup_wbengine(struct sde_hw_rotator_context *ctx,
		enum sde_rot_queue_prio queue_id,
		struct sde_hw_rot_wb_cfg *cfg,
		u32 flags)
{
	struct sde_rot_data_type *mdata = sde_rot_get_mdata();
	struct sde_mdp_format_params *fmt;
	struct sde_rot_cdp_params cdp_params = {0};
	char __iomem *wrptr;
	u32 pack = 0;
	u32 dst_format = 0;
	u32 no_partial_writes = 0;
	int i;

	wrptr = sde_hw_rotator_get_regdma_segment(ctx);

	fmt = cfg->fmt;

	/* setup WB DST format */
	dst_format |= (fmt->chroma_sample << 23) |
			(fmt->fetch_planes << 19)  |
			(fmt->bits[C3_ALPHA] << 6) |
			(fmt->bits[C2_R_Cr] << 4)  |
			(fmt->bits[C1_B_Cb] << 2)  |
			(fmt->bits[C0_G_Y] << 0);

	/* alpha control */
	if (fmt->alpha_enable || (!fmt->is_yuv && (fmt->unpack_count == 4))) {
		dst_format |= BIT(8);
		if (!fmt->alpha_enable) {
			dst_format |= BIT(14);
			SDE_REGDMA_WRITE(wrptr, ROT_WB_DST_ALPHA_X_VALUE, 0);
		}
	}

	dst_format |= ((fmt->unpack_count - 1) << 12)	|
			(fmt->unpack_tight << 17)	|
			(fmt->unpack_align_msb << 18)	|
			((fmt->bpp - 1) << 9)		|
			((fmt->frame_format & 3) << 30);

	if (sde_mdp_is_yuv_format(fmt))
		dst_format |= BIT(15);

	if (fmt->pixel_mode == SDE_MDP_PIXEL_10BIT)
		dst_format |= BIT(21); /* PACK_DX_FORMAT */

	/*
	 * REGDMA BLK write, from DST_FORMAT to DST_YSTRIDE 1, total 9 regs
	 */
	SDE_REGDMA_BLKWRITE_INC(wrptr, ROT_WB_DST_FORMAT, 9);

	/* DST_FORMAT */
	SDE_REGDMA_BLKWRITE_DATA(wrptr, dst_format);

	/* DST_OP_MODE */
	if (sde_mdp_is_ubwc_format(fmt))
		SDE_REGDMA_BLKWRITE_DATA(wrptr, BIT(0));
	else
		SDE_REGDMA_BLKWRITE_DATA(wrptr, 0);

	/* DST_PACK_PATTERN */
	pack = (fmt->element[3] << 24) | (fmt->element[2] << 16) |
		(fmt->element[1] << 8) | (fmt->element[0] << 0);
	SDE_REGDMA_BLKWRITE_DATA(wrptr, pack);

	/* DST_ADDR [0-3], DST_YSTRIDE [0-1] */
	for (i = 0; i < SDE_ROT_MAX_PLANES; i++)
		SDE_REGDMA_BLKWRITE_DATA(wrptr, cfg->data->p[i].addr);
	SDE_REGDMA_BLKWRITE_DATA(wrptr, cfg->dst_plane.ystride[0] |
			(cfg->dst_plane.ystride[1] << 16));
	SDE_REGDMA_BLKWRITE_DATA(wrptr, cfg->dst_plane.ystride[2] |
			(cfg->dst_plane.ystride[3] << 16));

	/* setup WB out image size and ROI */
	SDE_REGDMA_WRITE(wrptr, ROT_WB_OUT_IMG_SIZE,
			cfg->img_width | (cfg->img_height << 16));
	SDE_REGDMA_WRITE(wrptr, ROT_WB_OUT_SIZE,
			cfg->dst_rect->w | (cfg->dst_rect->h << 16));
	SDE_REGDMA_WRITE(wrptr, ROT_WB_OUT_XY,
			cfg->dst_rect->x | (cfg->dst_rect->y << 16));

	if (flags & (SDE_ROT_FLAG_SECURE_OVERLAY_SESSION |
			SDE_ROT_FLAG_SECURE_CAMERA_SESSION))
		SDE_REGDMA_WRITE(wrptr, ROT_WB_DST_ADDR_SW_STATUS, 0x1);
	else
		SDE_REGDMA_WRITE(wrptr, ROT_WB_DST_ADDR_SW_STATUS, 0);

	/*
	 * setup Downscale factor
	 */
	SDE_REGDMA_WRITE(wrptr, ROTTOP_DNSC,
			cfg->v_downscale_factor |
			(cfg->h_downscale_factor << 16));

	/* partial write check */
	if (test_bit(SDE_CAPS_PARTIALWR, mdata->sde_caps_map)) {
		no_partial_writes = BIT(10);

		/*
		 * For simplicity, don't disable partial writes if
		 * the ROI does not span the entire width of the
		 * output image, and require the total stride to
		 * also be properly aligned.
		 *
		 * This avoids having to determine the memory access
		 * alignment of the actual horizontal ROI on a per
		 * color format basis.
		 */
		if (sde_mdp_is_ubwc_format(fmt)) {
			no_partial_writes = 0x0;
		} else if (cfg->dst_rect->x ||
				cfg->dst_rect->w != cfg->img_width) {
			no_partial_writes = 0x0;
		} else {
			for (i = 0; i < SDE_ROT_MAX_PLANES; i++)
				if (cfg->dst_plane.ystride[i] &
						PARTIAL_WRITE_ALIGNMENT)
					no_partial_writes = 0x0;
		}
	}

	/* write config setup for bank configuration */
	SDE_REGDMA_WRITE(wrptr, ROT_WB_DST_WRITE_CONFIG, no_partial_writes |
			(ctx->rot->highest_bank & 0x3) << 8);

	if (test_bit(SDE_CAPS_UBWC_2, mdata->sde_caps_map))
		SDE_REGDMA_WRITE(wrptr, ROT_WB_UBWC_STATIC_CTRL,
				((ctx->rot->ubwc_malsize & 0x3) << 8) |
				((ctx->rot->highest_bank & 0x3) << 4) |
				((ctx->rot->ubwc_swizzle & 0x1) << 0));

	if (test_bit(SDE_CAPS_SBUF_1, mdata->sde_caps_map))
		SDE_REGDMA_WRITE(wrptr, ROT_WB_SYS_CACHE_MODE,
				ctx->sys_cache_mode);

	SDE_REGDMA_WRITE(wrptr, ROTTOP_OP_MODE, ctx->op_mode |
			(flags & SDE_ROT_FLAG_ROT_90 ? BIT(1) : 0) | BIT(0));

	sde_hw_rotator_put_regdma_segment(ctx, wrptr);

	/* CDP register WR setting */
	cdp_params.enable = test_bit(SDE_QOS_CDP, mdata->sde_qos_map) ?
					mdata->enable_cdp[SDE_ROT_WR] : false;
	cdp_params.fmt = fmt;
	cdp_params.offset = ROT_WB_CDP_CNTL;
	sde_hw_rotator_cdp_configs(ctx, &cdp_params);

	/* QOS LUT/ Danger LUT/ Safe LUT WR setting */
	sde_hw_rotator_setup_qos_lut_wr(ctx);

	wrptr = sde_hw_rotator_get_regdma_segment(ctx);

	/* setup traffic shaper for 4k 30fps content or if prefill_bw is set */
	if (ctx->is_traffic_shaping || cfg->prefill_bw) {
		u32 bw;

		/*
		 * Target to finish in 12ms, and we need to set number of bytes
		 * per clock tick for traffic shaping.
		 * Each clock tick run @ 19.2MHz, so we need we know total of
		 * clock ticks in 14ms, i.e. 12ms/(1/19.2MHz) ==> 23040
		 * Finally, calcualte the byte count per clock tick based on
		 * resolution, bpp and compression ratio.
		 */
		bw = cfg->dst_rect->w * cfg->dst_rect->h;

		if (fmt->chroma_sample == SDE_MDP_CHROMA_420)
			bw = (bw * 3) / 2;
		else
			bw *= fmt->bpp;

		bw /= TRAFFIC_SHAPE_CLKTICK_12MS;

		/* use prefill bandwidth instead if specified */
		if (cfg->prefill_bw)
			bw = DIV_ROUND_UP_SECTOR_T(cfg->prefill_bw,
					TRAFFIC_SHAPE_VSYNC_CLK);

		if (bw > 0xFF)
			bw = 0xFF;
		else if (bw == 0)
			bw = 1;

		SDE_REGDMA_WRITE(wrptr, ROT_WB_TRAFFIC_SHAPER_WR_CLIENT,
				BIT(31) | (cfg->prefill_bw ? BIT(27) : 0) | bw);
		SDEROT_DBG("Enable ROT_WB Traffic Shaper:%d\n", bw);
	} else {
		SDE_REGDMA_WRITE(wrptr, ROT_WB_TRAFFIC_SHAPER_WR_CLIENT, 0);
		SDEROT_DBG("Disable ROT_WB Traffic Shaper\n");
	}

	/* Update command queue write ptr */
	sde_hw_rotator_put_regdma_segment(ctx, wrptr);
}

/*
 * sde_hw_rotator_start_no_regdma - start non-regdma operation
 * @ctx: Pointer to rotator context
 * @queue_id: Priority queue identifier
 */
static u32 sde_hw_rotator_start_no_regdma(struct sde_hw_rotator_context *ctx,
		enum sde_rot_queue_prio queue_id)
{
	struct sde_hw_rotator *rot = ctx->rot;
	char __iomem *wrptr;
	char __iomem *mem_rdptr;
	char __iomem *addr;
	u32 mask;
	u32 cmd0, cmd1, cmd2;
	u32 blksize;

	/*
	 * when regdma is not using, the regdma segment is just a normal
	 * DRAM, and not an iomem.
	 */
	mem_rdptr = sde_hw_rotator_get_regdma_segment_base(ctx);
	wrptr = sde_hw_rotator_get_regdma_segment(ctx);

	if (rot->irq_num >= 0) {
		SDE_REGDMA_WRITE(wrptr, ROTTOP_INTR_EN, 1);
		SDE_REGDMA_WRITE(wrptr, ROTTOP_INTR_CLEAR, 1);
		reinit_completion(&ctx->rot_comp);
		sde_hw_rotator_enable_irq(rot);
	}

	SDE_REGDMA_WRITE(wrptr, ROTTOP_START_CTRL, ctx->start_ctrl);

	/* Update command queue write ptr */
	sde_hw_rotator_put_regdma_segment(ctx, wrptr);

	SDEROT_DBG("BEGIN %d\n", ctx->timestamp);
	/* Write all command stream to Rotator blocks */
	/* Rotator will start right away after command stream finish writing */
	while (mem_rdptr < wrptr) {
		u32 op = REGDMA_OP_MASK & readl_relaxed_no_log(mem_rdptr);

		switch (op) {
		case REGDMA_OP_NOP:
			SDEROT_DBG("NOP\n");
			mem_rdptr += sizeof(u32);
			break;
		case REGDMA_OP_REGWRITE:
			SDE_REGDMA_READ(mem_rdptr, cmd0);
			SDE_REGDMA_READ(mem_rdptr, cmd1);
			SDEROT_DBG("REGW %6.6x %8.8x\n",
					cmd0 & REGDMA_ADDR_OFFSET_MASK,
					cmd1);
			addr =  rot->mdss_base +
				(cmd0 & REGDMA_ADDR_OFFSET_MASK);
			writel_relaxed(cmd1, addr);
			break;
		case REGDMA_OP_REGMODIFY:
			SDE_REGDMA_READ(mem_rdptr, cmd0);
			SDE_REGDMA_READ(mem_rdptr, cmd1);
			SDE_REGDMA_READ(mem_rdptr, cmd2);
			SDEROT_DBG("REGM %6.6x %8.8x %8.8x\n",
					cmd0 & REGDMA_ADDR_OFFSET_MASK,
					cmd1, cmd2);
			addr =  rot->mdss_base +
				(cmd0 & REGDMA_ADDR_OFFSET_MASK);
			mask = cmd1;
			writel_relaxed((readl_relaxed(addr) & mask) | cmd2,
					addr);
			break;
		case REGDMA_OP_BLKWRITE_SINGLE:
			SDE_REGDMA_READ(mem_rdptr, cmd0);
			SDE_REGDMA_READ(mem_rdptr, cmd1);
			SDEROT_DBG("BLKWS %6.6x %6.6x\n",
					cmd0 & REGDMA_ADDR_OFFSET_MASK,
					cmd1);
			addr =  rot->mdss_base +
				(cmd0 & REGDMA_ADDR_OFFSET_MASK);
			blksize = cmd1;
			while (blksize--) {
				SDE_REGDMA_READ(mem_rdptr, cmd0);
				SDEROT_DBG("DATA %8.8x\n", cmd0);
				writel_relaxed(cmd0, addr);
			}
			break;
		case REGDMA_OP_BLKWRITE_INC:
			SDE_REGDMA_READ(mem_rdptr, cmd0);
			SDE_REGDMA_READ(mem_rdptr, cmd1);
			SDEROT_DBG("BLKWI %6.6x %6.6x\n",
					cmd0 & REGDMA_ADDR_OFFSET_MASK,
					cmd1);
			addr =  rot->mdss_base +
				(cmd0 & REGDMA_ADDR_OFFSET_MASK);
			blksize = cmd1;
			while (blksize--) {
				SDE_REGDMA_READ(mem_rdptr, cmd0);
				SDEROT_DBG("DATA %8.8x\n", cmd0);
				writel_relaxed(cmd0, addr);
				addr += 4;
			}
			break;
		default:
			/* Other not supported OP mode
			 * Skip data for now for unregonized OP mode
			 */
			SDEROT_DBG("UNDEFINED\n");
			mem_rdptr += sizeof(u32);
			break;
		}
	}
	SDEROT_DBG("END %d\n", ctx->timestamp);

	return ctx->timestamp;
}

/*
 * sde_hw_rotator_start_regdma - start regdma operation
 * @ctx: Pointer to rotator context
 * @queue_id: Priority queue identifier
 */
static u32 sde_hw_rotator_start_regdma(struct sde_hw_rotator_context *ctx,
		enum sde_rot_queue_prio queue_id)
{
	struct sde_rot_data_type *mdata = sde_rot_get_mdata();
	struct sde_hw_rotator *rot = ctx->rot;
	char __iomem *wrptr;
	u32  regdmaSlot;
	u32  offset;
	u32  length;
	u32  ts_length;
	u32  enableInt;
	u32  swts = 0;
	u32  mask = 0;
	u32  trig_sel;
	bool int_trigger = false;

	wrptr = sde_hw_rotator_get_regdma_segment(ctx);

	/* Enable HW timestamp if supported in rotator */
	if (test_bit(SDE_CAPS_HW_TIMESTAMP, mdata->sde_caps_map)) {
		SDE_REGDMA_MODIFY(wrptr, ROTTOP_ROT_CNTR_CTRL,
				~BIT(queue_id), BIT(queue_id));
		int_trigger = true;
	} else if (ctx->sbuf_mode) {
		int_trigger = true;
	}

	/*
	 * Last ROT command must be ROT_START before REGDMA start
	 */
	SDE_REGDMA_WRITE(wrptr, ROTTOP_START_CTRL, ctx->start_ctrl);

	sde_hw_rotator_put_regdma_segment(ctx, wrptr);

	/*
	 * Start REGDMA with command offset and size
	 */
	regdmaSlot = sde_hw_rotator_get_regdma_ctxidx(ctx);
	length = (wrptr - ctx->regdma_base) / 4;
	offset = (ctx->regdma_base - (rot->mdss_base +
				REGDMA_RAM_REGDMA_CMD_RAM)) / sizeof(u32);
	enableInt = ((ctx->timestamp & 1) + 1) << 30;
	trig_sel = ctx->sbuf_mode ? REGDMA_CMD_TRIG_SEL_MDP_FLUSH :
			REGDMA_CMD_TRIG_SEL_SW_START;

	SDEROT_DBG(
		"regdma(%d)[%d] <== INT:0x%X|length:%d|offset:0x%X, ts:%X\n",
		queue_id, regdmaSlot, enableInt, length, offset,
		ctx->timestamp);

	/* ensure the command packet is issued before the submit command */
	wmb();

	/* REGDMA submission for current context */
	if (queue_id == ROT_QUEUE_HIGH_PRIORITY) {
		SDE_ROTREG_WRITE(rot->mdss_base,
				REGDMA_CSR_REGDMA_QUEUE_0_SUBMIT,
				(int_trigger ? enableInt : 0) | trig_sel |
				((length & 0x3ff) << 14) | offset);
		swts = ctx->timestamp;
		mask = ~SDE_REGDMA_SWTS_MASK;
	} else {
		SDE_ROTREG_WRITE(rot->mdss_base,
				REGDMA_CSR_REGDMA_QUEUE_1_SUBMIT,
				(int_trigger ? enableInt : 0) | trig_sel |
				((length & 0x3ff) << 14) | offset);
		swts = ctx->timestamp << SDE_REGDMA_SWTS_SHIFT;
		mask = ~(SDE_REGDMA_SWTS_MASK << SDE_REGDMA_SWTS_SHIFT);
	}

	SDEROT_EVTLOG(ctx->timestamp, queue_id, length, offset, ctx->sbuf_mode);

	/* sw timestamp update can only be used in offline multi-context mode */
	if (!int_trigger) {
		/* Write timestamp after previous rotator job finished */
		sde_hw_rotator_setup_timestamp_packet(ctx, mask, swts);
		offset += length;
		ts_length = sde_hw_rotator_get_regdma_segment(ctx) - wrptr;
		ts_length /= sizeof(u32);
		WARN_ON((length + ts_length) > SDE_HW_ROT_REGDMA_SEG_SIZE);

		/* ensure command packet is issue before the submit command */
		wmb();

		SDEROT_EVTLOG(queue_id, enableInt, ts_length, offset);

		if (queue_id == ROT_QUEUE_HIGH_PRIORITY) {
			SDE_ROTREG_WRITE(rot->mdss_base,
					REGDMA_CSR_REGDMA_QUEUE_0_SUBMIT,
					enableInt | (ts_length << 14) | offset);
		} else {
			SDE_ROTREG_WRITE(rot->mdss_base,
					REGDMA_CSR_REGDMA_QUEUE_1_SUBMIT,
					enableInt | (ts_length << 14) | offset);
		}
	}

	/* Update command queue write ptr */
	sde_hw_rotator_put_regdma_segment(ctx, wrptr);

	return ctx->timestamp;
}

/*
 * sde_hw_rotator_wait_done_no_regdma - wait for non-regdma completion
 * @ctx: Pointer to rotator context
 * @queue_id: Priority queue identifier
 * @flags: Option flag
 */
static u32 sde_hw_rotator_wait_done_no_regdma(
		struct sde_hw_rotator_context *ctx,
		enum sde_rot_queue_prio queue_id, u32 flag)
{
	struct sde_hw_rotator *rot = ctx->rot;
	int rc = 0;
	u32 sts = 0;
	u32 status;
	unsigned long flags;

	if (rot->irq_num >= 0) {
		SDEROT_DBG("Wait for Rotator completion\n");
		rc = wait_for_completion_timeout(&ctx->rot_comp,
				ctx->sbuf_mode ?
				msecs_to_jiffies(KOFF_TIMEOUT_SBUF) :
				msecs_to_jiffies(rot->koff_timeout));

		spin_lock_irqsave(&rot->rotisr_lock, flags);
		status = SDE_ROTREG_READ(rot->mdss_base, ROTTOP_STATUS);
		if (rc == 0) {
			/*
			 * Timeout, there might be error,
			 * or rotator still busy
			 */
			if (status & ROT_BUSY_BIT)
				SDEROT_ERR(
					"Timeout waiting for rotator done\n");
			else if (status & ROT_ERROR_BIT)
				SDEROT_ERR(
					"Rotator report error status\n");
			else
				SDEROT_WARN(
					"Timeout waiting, but rotator job is done!!\n");

			sde_hw_rotator_disable_irq(rot);
		}
		spin_unlock_irqrestore(&rot->rotisr_lock, flags);
	} else {
		int cnt = 200;

		do {
			udelay(500);
			status = SDE_ROTREG_READ(rot->mdss_base, ROTTOP_STATUS);
			cnt--;
		} while ((cnt > 0) && (status & ROT_BUSY_BIT)
				&& ((status & ROT_ERROR_BIT) == 0));

		if (status & ROT_ERROR_BIT)
			SDEROT_ERR("Rotator error\n");
		else if (status & ROT_BUSY_BIT)
			SDEROT_ERR("Rotator busy\n");

		SDE_ROTREG_WRITE(rot->mdss_base, ROTTOP_INTR_CLEAR,
				ROT_DONE_CLEAR);
	}

	sts = (status & ROT_ERROR_BIT) ? -ENODEV : 0;

	return sts;
}

/*
 * sde_hw_rotator_wait_done_regdma - wait for regdma completion
 * @ctx: Pointer to rotator context
 * @queue_id: Priority queue identifier
 * @flags: Option flag
 */
static u32 sde_hw_rotator_wait_done_regdma(
		struct sde_hw_rotator_context *ctx,
		enum sde_rot_queue_prio queue_id, u32 flag)
{
	struct sde_hw_rotator *rot = ctx->rot;
	int rc = 0;
	bool abort;
	u32 status;
	u32 last_isr;
	u32 last_ts;
	u32 int_id;
	u32 swts;
	u32 sts = 0;
	u32 ubwcerr = 0;
	unsigned long flags;

	if (rot->irq_num >= 0) {
		SDEROT_DBG("Wait for REGDMA completion, ctx:%p, ts:%X\n",
				ctx, ctx->timestamp);
		rc = wait_event_timeout(ctx->regdma_waitq,
				!rot->ops.get_pending_ts(rot, ctx, &swts),
				ctx->sbuf_mode ?
				msecs_to_jiffies(KOFF_TIMEOUT_SBUF) :
				msecs_to_jiffies(rot->koff_timeout));

		ATRACE_INT("sde_rot_done", 0);
		spin_lock_irqsave(&rot->rotisr_lock, flags);

		last_isr = ctx->last_regdma_isr_status;
		last_ts  = ctx->last_regdma_timestamp;
		abort    = ctx->abort;
		status   = last_isr & REGDMA_INT_MASK;
		int_id   = last_ts & 1;
		SDEROT_DBG("INT status:0x%X, INT id:%d, timestamp:0x%X\n",
				status, int_id, last_ts);

		if (rc == 0 || (status & REGDMA_INT_ERR_MASK) || abort) {
			bool pending;

			pending = rot->ops.get_pending_ts(rot, ctx, &swts);
			SDEROT_ERR(
				"Timeout wait for regdma interrupt status, ts:0x%X/0x%X, pending:%d, abort:%d\n",
				ctx->timestamp, swts, pending, abort);

			if (status & REGDMA_WATCHDOG_INT)
				SDEROT_ERR("REGDMA watchdog interrupt\n");
			else if (status & REGDMA_INVALID_DESCRIPTOR)
				SDEROT_ERR("REGDMA invalid descriptor\n");
			else if (status & REGDMA_INCOMPLETE_CMD)
				SDEROT_ERR("REGDMA incomplete command\n");
			else if (status & REGDMA_INVALID_CMD)
				SDEROT_ERR("REGDMA invalid command\n");

			_sde_hw_rotator_dump_status(rot, &ubwcerr);

			if (ubwcerr || abort) {
				/*
				 * Perform recovery for ROT SSPP UBWC decode
				 * error.
				 * - SW reset rotator hw block
				 * - reset TS logic so all pending rotation
				 *   in hw queue got done signalled
				 */
				spin_unlock_irqrestore(&rot->rotisr_lock,
						flags);
				if (!sde_hw_rotator_reset(rot, ctx))
					status = REGDMA_INCOMPLETE_CMD;
				else
					status = ROT_ERROR_BIT;
				spin_lock_irqsave(&rot->rotisr_lock, flags);
			} else {
				status = ROT_ERROR_BIT;
			}
		} else {
			if (rc == 1)
				SDEROT_WARN(
					"REGDMA done but no irq, ts:0x%X/0x%X\n",
					ctx->timestamp, swts);
			status = 0;
		}

		spin_unlock_irqrestore(&rot->rotisr_lock, flags);
	} else {
		int cnt = 200;
		bool pending;

		do {
			udelay(500);
			last_isr = SDE_ROTREG_READ(rot->mdss_base,
					REGDMA_CSR_REGDMA_INT_STATUS);
			pending = rot->ops.get_pending_ts(rot, ctx, &swts);
			cnt--;
		} while ((cnt > 0) && pending &&
				((last_isr & REGDMA_INT_ERR_MASK) == 0));

		if (last_isr & REGDMA_INT_ERR_MASK) {
			SDEROT_ERR("Rotator error, ts:0x%X/0x%X status:%x\n",
				ctx->timestamp, swts, last_isr);
			_sde_hw_rotator_dump_status(rot, NULL);
			status = ROT_ERROR_BIT;
		} else if (pending) {
			SDEROT_ERR("Rotator timeout, ts:0x%X/0x%X status:%x\n",
				ctx->timestamp, swts, last_isr);
			_sde_hw_rotator_dump_status(rot, NULL);
			status = ROT_ERROR_BIT;
		} else {
			status = 0;
		}

		SDE_ROTREG_WRITE(rot->mdss_base, REGDMA_CSR_REGDMA_INT_CLEAR,
				last_isr);
	}

	sts = (status & (ROT_ERROR_BIT | REGDMA_INCOMPLETE_CMD)) ? -ENODEV : 0;

	if (status & ROT_ERROR_BIT)
		SDEROT_EVTLOG_TOUT_HANDLER("rot", "rot_dbg_bus",
				"vbif_dbg_bus", "panic");

	return sts;
}

/*
 * setup_rotator_ops - setup callback functions for the low-level HAL
 * @ops: Pointer to low-level ops callback
 * @mode: Operation mode (non-regdma or regdma)
 * @use_hwts: HW timestamp support mode
 */
static void setup_rotator_ops(struct sde_hw_rotator_ops *ops,
		enum sde_rotator_regdma_mode mode,
		bool use_hwts)
{
	ops->setup_rotator_fetchengine = sde_hw_rotator_setup_fetchengine;
	ops->setup_rotator_wbengine = sde_hw_rotator_setup_wbengine;
	if (mode == ROT_REGDMA_ON) {
		ops->start_rotator = sde_hw_rotator_start_regdma;
		ops->wait_rotator_done = sde_hw_rotator_wait_done_regdma;
	} else {
		ops->start_rotator = sde_hw_rotator_start_no_regdma;
		ops->wait_rotator_done = sde_hw_rotator_wait_done_no_regdma;
	}

	if (use_hwts) {
		ops->get_pending_ts = sde_hw_rotator_pending_hwts;
		ops->update_ts = sde_hw_rotator_update_hwts;
	} else {
		ops->get_pending_ts = sde_hw_rotator_pending_swts;
		ops->update_ts = sde_hw_rotator_update_swts;
	}
}

/*
 * sde_hw_rotator_swts_create - create software timestamp buffer
 * @rot: Pointer to rotator hw
 *
 * This buffer is used by regdma to keep track of last completed command.
 */
static int sde_hw_rotator_swts_create(struct sde_hw_rotator *rot)
{
	int rc = 0;
	struct sde_mdp_img_data *data;
	u32 bufsize = sizeof(int) * SDE_HW_ROT_REGDMA_TOTAL_CTX * 2;

	if (bufsize < SZ_4K)
		bufsize = SZ_4K;

	data = &rot->swts_buf;
	data->len = bufsize;
	data->srcp_dma_buf = sde_rot_get_dmabuf(data);
	if (!data->srcp_dma_buf) {
		SDEROT_ERR("Fail dmabuf create\n");
		return -ENOMEM;
	}

	sde_smmu_ctrl(1);

	data->srcp_attachment = sde_smmu_dma_buf_attach(data->srcp_dma_buf,
			&rot->pdev->dev, SDE_IOMMU_DOMAIN_ROT_UNSECURE);
	if (IS_ERR_OR_NULL(data->srcp_attachment)) {
		SDEROT_ERR("sde_smmu_dma_buf_attach error\n");
		rc = -ENOMEM;
		goto err_put;
	}

	data->srcp_table = dma_buf_map_attachment(data->srcp_attachment,
			DMA_BIDIRECTIONAL);
	if (IS_ERR_OR_NULL(data->srcp_table)) {
		SDEROT_ERR("dma_buf_map_attachment error\n");
		rc = -ENOMEM;
		goto err_detach;
	}

	rc = sde_smmu_map_dma_buf(data->srcp_dma_buf, data->srcp_table,
			SDE_IOMMU_DOMAIN_ROT_UNSECURE, &data->addr,
			&data->len, DMA_BIDIRECTIONAL);
	if (rc < 0) {
		SDEROT_ERR("smmu_map_dma_buf failed: (%d)\n", rc);
		goto err_unmap;
	}

	data->mapped = true;
	SDEROT_DBG("swts buffer mapped: %pad/%lx va:%p\n", &data->addr,
			data->len, rot->swts_buffer);

	sde_smmu_ctrl(0);

	return rc;
err_unmap:
	dma_buf_unmap_attachment(data->srcp_attachment, data->srcp_table,
			DMA_FROM_DEVICE);
err_detach:
	dma_buf_detach(data->srcp_dma_buf, data->srcp_attachment);
err_put:
	data->srcp_dma_buf = NULL;

	sde_smmu_ctrl(0);
	return rc;
}

/*
 * sde_hw_rotator_swts_destroy - destroy software timestamp buffer
 * @rot: Pointer to rotator hw
 */
static void sde_hw_rotator_swts_destroy(struct sde_hw_rotator *rot)
{
	struct sde_mdp_img_data *data;

	data = &rot->swts_buf;

	sde_smmu_unmap_dma_buf(data->srcp_table, SDE_IOMMU_DOMAIN_ROT_UNSECURE,
			DMA_FROM_DEVICE, data->srcp_dma_buf);
	dma_buf_unmap_attachment(data->srcp_attachment, data->srcp_table,
			DMA_FROM_DEVICE);
	dma_buf_detach(data->srcp_dma_buf, data->srcp_attachment);
	dma_buf_put(data->srcp_dma_buf);
	data->addr = 0;
	data->srcp_dma_buf = NULL;
	data->srcp_attachment = NULL;
	data->mapped = false;
}

/*
 * sde_hw_rotator_pre_pmevent - SDE rotator core will call this before a
 *                              PM event occurs
 * @mgr: Pointer to rotator manager
 * @pmon: Boolean indicate an on/off power event
 */
void sde_hw_rotator_pre_pmevent(struct sde_rot_mgr *mgr, bool pmon)
{
	struct sde_hw_rotator *rot;
	u32 l_ts, h_ts, l_hwts, h_hwts;
	u32 rotsts, regdmasts, rotopmode;

	/*
	 * Check last HW timestamp with SW timestamp before power off event.
	 * If there is a mismatch, that will be quite possible the rotator HW
	 * is either hang or not finishing last submitted job. In that case,
	 * it is best to do a timeout eventlog to capture some good events
	 * log data for analysis.
	 */
	if (!pmon && mgr && mgr->hw_data) {
		rot = mgr->hw_data;
		h_ts = atomic_read(&rot->timestamp[ROT_QUEUE_HIGH_PRIORITY]) &
				SDE_REGDMA_SWTS_MASK;
		l_ts = atomic_read(&rot->timestamp[ROT_QUEUE_LOW_PRIORITY]) &
				SDE_REGDMA_SWTS_MASK;

		/* Need to turn on clock to access rotator register */
		sde_rotator_clk_ctrl(mgr, true);
		l_hwts = __sde_hw_rotator_get_timestamp(rot,
				ROT_QUEUE_LOW_PRIORITY);
		h_hwts = __sde_hw_rotator_get_timestamp(rot,
				ROT_QUEUE_HIGH_PRIORITY);
		regdmasts = SDE_ROTREG_READ(rot->mdss_base,
				REGDMA_CSR_REGDMA_BLOCK_STATUS);
		rotsts = SDE_ROTREG_READ(rot->mdss_base, ROTTOP_STATUS);
		rotopmode = SDE_ROTREG_READ(rot->mdss_base, ROTTOP_OP_MODE);

		SDEROT_DBG(
			"swts(l/h):0x%x/0x%x, hwts(l/h):0x%x/0x%x, regdma-sts:0x%x, rottop-sts:0x%x\n",
				l_ts, h_ts, l_hwts, h_hwts,
				regdmasts, rotsts);
		SDEROT_EVTLOG(l_ts, h_ts, l_hwts, h_hwts, regdmasts, rotsts);

		if (((l_ts != l_hwts) || (h_ts != h_hwts)) &&
				((regdmasts & REGDMA_BUSY) ||
				 (rotsts & ROT_STATUS_MASK))) {
			SDEROT_ERR(
				"Mismatch SWTS with HWTS: swts(l/h):0x%x/0x%x, hwts(l/h):0x%x/0x%x, regdma-sts:0x%x, rottop-sts:0x%x\n",
				l_ts, h_ts, l_hwts, h_hwts,
				regdmasts, rotsts);
			_sde_hw_rotator_dump_status(rot, NULL);
			SDEROT_EVTLOG_TOUT_HANDLER("rot", "rot_dbg_bus",
					"vbif_dbg_bus", "panic");
		} else if (!SDE_ROTTOP_IN_OFFLINE_MODE(rotopmode) &&
				((regdmasts & REGDMA_BUSY) ||
						(rotsts & ROT_BUSY_BIT))) {
			/*
			 * rotator can stuck in inline while mdp is detached
			 */
			SDEROT_WARN(
				"Inline Rot busy: regdma-sts:0x%x, rottop-sts:0x%x, rottop-opmode:0x%x\n",
				regdmasts, rotsts, rotopmode);
			sde_hw_rotator_reset(rot, NULL);
		} else if ((regdmasts & REGDMA_BUSY) ||
				(rotsts & ROT_BUSY_BIT)) {
			_sde_hw_rotator_dump_status(rot, NULL);
			SDEROT_EVTLOG_TOUT_HANDLER("rot", "rot_dbg_bus",
					"vbif_dbg_bus", "panic");
			sde_hw_rotator_reset(rot, NULL);
		}

		/* Turn off rotator clock after checking rotator registers */
		sde_rotator_clk_ctrl(mgr, false);
	}
}

/*
 * sde_hw_rotator_post_pmevent - SDE rotator core will call this after a
 *                               PM event occurs
 * @mgr: Pointer to rotator manager
 * @pmon: Boolean indicate an on/off power event
 */
void sde_hw_rotator_post_pmevent(struct sde_rot_mgr *mgr, bool pmon)
{
	struct sde_hw_rotator *rot;
	u32 l_ts, h_ts;

	/*
	 * After a power on event, the rotator HW is reset to default setting.
	 * It is necessary to synchronize the SW timestamp with the HW.
	 */
	if (pmon && mgr && mgr->hw_data) {
		rot = mgr->hw_data;
		h_ts = atomic_read(&rot->timestamp[ROT_QUEUE_HIGH_PRIORITY]);
		l_ts = atomic_read(&rot->timestamp[ROT_QUEUE_LOW_PRIORITY]);

		SDEROT_DBG("h_ts:0x%x, l_ts;0x%x\n", h_ts, l_ts);
		SDEROT_EVTLOG(h_ts, l_ts);
		rot->reset_hw_ts = true;
		rot->last_hwts[ROT_QUEUE_LOW_PRIORITY] =
				l_ts & SDE_REGDMA_SWTS_MASK;
		rot->last_hwts[ROT_QUEUE_HIGH_PRIORITY] =
				h_ts & SDE_REGDMA_SWTS_MASK;
	}
}

/*
 * sde_hw_rotator_destroy - Destroy hw rotator and free allocated resources
 * @mgr: Pointer to rotator manager
 */
static void sde_hw_rotator_destroy(struct sde_rot_mgr *mgr)
{
	struct sde_rot_data_type *mdata = sde_rot_get_mdata();
	struct sde_hw_rotator *rot;

	if (!mgr || !mgr->pdev || !mgr->hw_data) {
		SDEROT_ERR("null parameters\n");
		return;
	}

	rot = mgr->hw_data;
	if (rot->irq_num >= 0)
		devm_free_irq(&mgr->pdev->dev, rot->irq_num, mdata);

	if (!test_bit(SDE_CAPS_HW_TIMESTAMP, mdata->sde_caps_map) &&
			rot->mode == ROT_REGDMA_ON)
		sde_hw_rotator_swts_destroy(rot);

	devm_kfree(&mgr->pdev->dev, mgr->hw_data);
	mgr->hw_data = NULL;
}

/*
 * sde_hw_rotator_alloc_ext - allocate rotator resource from rotator hw
 * @mgr: Pointer to rotator manager
 * @pipe_id: pipe identifier (not used)
 * @wb_id: writeback identifier/priority queue identifier
 *
 * This function allocates a new hw rotator resource for the given priority.
 */
static struct sde_rot_hw_resource *sde_hw_rotator_alloc_ext(
		struct sde_rot_mgr *mgr, u32 pipe_id, u32 wb_id)
{
	struct sde_rot_data_type *mdata = sde_rot_get_mdata();
	struct sde_hw_rotator_resource_info *resinfo;

	if (!mgr || !mgr->hw_data) {
		SDEROT_ERR("null parameters\n");
		return NULL;
	}

	/*
	 * Allocate rotator resource info. Each allocation is per
	 * HW priority queue
	 */
	resinfo = devm_kzalloc(&mgr->pdev->dev, sizeof(*resinfo), GFP_KERNEL);
	if (!resinfo) {
		SDEROT_ERR("Failed allocation HW rotator resource info\n");
		return NULL;
	}

	resinfo->rot = mgr->hw_data;
	resinfo->hw.wb_id = wb_id;
	atomic_set(&resinfo->hw.num_active, 0);
	init_waitqueue_head(&resinfo->hw.wait_queue);

	/* For non-regdma, only support one active session */
	if (resinfo->rot->mode == ROT_REGDMA_OFF)
		resinfo->hw.max_active = 1;
	else {
		resinfo->hw.max_active = SDE_HW_ROT_REGDMA_TOTAL_CTX - 1;

		if (!test_bit(SDE_CAPS_HW_TIMESTAMP, mdata->sde_caps_map) &&
				resinfo->rot->swts_buf.mapped == false)
			sde_hw_rotator_swts_create(resinfo->rot);
	}

	if (resinfo->rot->irq_num >= 0)
		sde_hw_rotator_enable_irq(resinfo->rot);

	SDEROT_DBG("New rotator resource:%p, priority:%d\n",
			resinfo, wb_id);

	return &resinfo->hw;
}

/*
 * sde_hw_rotator_free_ext - free the given rotator resource
 * @mgr: Pointer to rotator manager
 * @hw: Pointer to rotator resource
 */
static void sde_hw_rotator_free_ext(struct sde_rot_mgr *mgr,
		struct sde_rot_hw_resource *hw)
{
	struct sde_hw_rotator_resource_info *resinfo;

	if (!mgr || !mgr->hw_data)
		return;

	resinfo = container_of(hw, struct sde_hw_rotator_resource_info, hw);

	SDEROT_DBG(
		"Free rotator resource:%p, priority:%d, active:%d, pending:%d\n",
		resinfo, hw->wb_id, atomic_read(&hw->num_active),
		hw->pending_count);

	if (resinfo->rot->irq_num >= 0)
		sde_hw_rotator_disable_irq(resinfo->rot);

	devm_kfree(&mgr->pdev->dev, resinfo);
}

/*
 * sde_hw_rotator_alloc_rotctx - allocate rotator context
 * @rot: Pointer to rotator hw
 * @hw: Pointer to rotator resource
 * @session_id: Session identifier of this context
 * @sequence_id: Sequence identifier of this request
 * @sbuf_mode: true if stream buffer is requested
 *
 * This function allocates a new rotator context for the given session id.
 */
static struct sde_hw_rotator_context *sde_hw_rotator_alloc_rotctx(
		struct sde_hw_rotator *rot,
		struct sde_rot_hw_resource *hw,
		u32    session_id,
		u32    sequence_id,
		bool   sbuf_mode)
{
	struct sde_hw_rotator_context *ctx;

	/* Allocate rotator context */
	ctx = devm_kzalloc(&rot->pdev->dev, sizeof(*ctx), GFP_KERNEL);
	if (!ctx) {
		SDEROT_ERR("Failed allocation HW rotator context\n");
		return NULL;
	}

	ctx->rot        = rot;
	ctx->q_id       = hw->wb_id;
	ctx->session_id = session_id;
	ctx->sequence_id = sequence_id;
	ctx->hwres      = hw;
	ctx->timestamp  = atomic_add_return(1, &rot->timestamp[ctx->q_id]);
	ctx->timestamp &= SDE_REGDMA_SWTS_MASK;
	ctx->is_secure  = false;
	ctx->sbuf_mode  = sbuf_mode;
	INIT_LIST_HEAD(&ctx->list);

	ctx->regdma_base  = rot->cmd_wr_ptr[ctx->q_id]
		[sde_hw_rotator_get_regdma_ctxidx(ctx)];
	ctx->regdma_wrptr = ctx->regdma_base;
	ctx->ts_addr      = (dma_addr_t)((u32 *)rot->swts_buf.addr +
		ctx->q_id * SDE_HW_ROT_REGDMA_TOTAL_CTX +
		sde_hw_rotator_get_regdma_ctxidx(ctx));

	ctx->last_regdma_timestamp = SDE_REGDMA_SWTS_INVALID;

	init_completion(&ctx->rot_comp);
	init_waitqueue_head(&ctx->regdma_waitq);

	/* Store rotator context for lookup purpose */
	sde_hw_rotator_put_ctx(ctx);

	SDEROT_DBG(
		"New rot CTX:%p, ctxidx:%d, session-id:%d, prio:%d, timestamp:%X, active:%d sbuf:%d\n",
		ctx, sde_hw_rotator_get_regdma_ctxidx(ctx), ctx->session_id,
		ctx->q_id, ctx->timestamp,
		atomic_read(&ctx->hwres->num_active),
		ctx->sbuf_mode);

	return ctx;
}

/*
 * sde_hw_rotator_free_rotctx - free the given rotator context
 * @rot: Pointer to rotator hw
 * @ctx: Pointer to rotator context
 */
static void sde_hw_rotator_free_rotctx(struct sde_hw_rotator *rot,
		struct sde_hw_rotator_context *ctx)
{
	if (!rot || !ctx)
		return;

	SDEROT_DBG(
		"Free rot CTX:%p, ctxidx:%d, session-id:%d, prio:%d, timestamp:%X, active:%d sbuf:%d\n",
		ctx, sde_hw_rotator_get_regdma_ctxidx(ctx), ctx->session_id,
		ctx->q_id, ctx->timestamp,
		atomic_read(&ctx->hwres->num_active),
		ctx->sbuf_mode);

	/* Clear rotator context from lookup purpose */
	sde_hw_rotator_clr_ctx(ctx);

	devm_kfree(&rot->pdev->dev, ctx);
}

/*
 * sde_hw_rotator_config - configure hw for the given rotation entry
 * @hw: Pointer to rotator resource
 * @entry: Pointer to rotation entry
 *
 * This function setup the fetch/writeback/rotator blocks, as well as VBIF
 * based on the given rotation entry.
 */
static int sde_hw_rotator_config(struct sde_rot_hw_resource *hw,
		struct sde_rot_entry *entry)
{
	struct sde_rot_data_type *mdata = sde_rot_get_mdata();
	struct sde_hw_rotator *rot;
	struct sde_hw_rotator_resource_info *resinfo;
	struct sde_hw_rotator_context *ctx;
	struct sde_hw_rot_sspp_cfg sspp_cfg;
	struct sde_hw_rot_wb_cfg wb_cfg;
	u32 danger_lut = 0;	/* applicable for realtime client only */
	u32 safe_lut = 0;	/* applicable for realtime client only */
	u32 flags = 0;
	u32 rststs = 0;
	struct sde_rotation_item *item;
	int ret;

	if (!hw || !entry) {
		SDEROT_ERR("null hw resource/entry\n");
		return -EINVAL;
	}

	resinfo = container_of(hw, struct sde_hw_rotator_resource_info, hw);
	rot = resinfo->rot;
	item = &entry->item;

	ctx = sde_hw_rotator_alloc_rotctx(rot, hw, item->session_id,
			item->sequence_id, item->output.sbuf);
	if (!ctx) {
		SDEROT_ERR("Failed allocating rotator context!!\n");
		return -EINVAL;
	}

	/* save entry for debugging purposes */
	ctx->last_entry = entry;

	if (test_bit(SDE_CAPS_SBUF_1, mdata->sde_caps_map)) {
		if (entry->dst_buf.sbuf) {
			u32 op_mode;

			if (entry->item.trigger ==
					SDE_ROTATOR_TRIGGER_COMMAND)
				ctx->start_ctrl = (rot->cmd_trigger << 4);
			else if (entry->item.trigger ==
					SDE_ROTATOR_TRIGGER_VIDEO)
				ctx->start_ctrl = (rot->vid_trigger << 4);
			else
				ctx->start_ctrl = 0;

			ctx->sys_cache_mode = BIT(15) |
					((item->output.scid & 0x1f) << 8) |
					(item->output.writeback ? 0x5 : 0);

			ctx->op_mode = BIT(4) |
				((ctx->rot->sbuf_headroom & 0xff) << 8);

			/* detect transition to inline mode */
			op_mode = (SDE_ROTREG_READ(rot->mdss_base,
					ROTTOP_OP_MODE) >> 4) & 0x3;
			if (!op_mode) {
				u32 status;

				status = SDE_ROTREG_READ(rot->mdss_base,
						ROTTOP_STATUS);
				if (status & BIT(0)) {
					SDEROT_ERR("rotator busy 0x%x\n",
							status);
					_sde_hw_rotator_dump_status(rot, NULL);
					SDEROT_EVTLOG_TOUT_HANDLER("rot",
							"vbif_dbg_bus",
							"panic");
				}
			}

		} else {
			ctx->start_ctrl = BIT(0);
			ctx->sys_cache_mode = 0;
			ctx->op_mode = 0;
		}
	} else  {
		ctx->start_ctrl = BIT(0);
	}

	SDEROT_EVTLOG(ctx->start_ctrl, ctx->sys_cache_mode, ctx->op_mode);

	/*
	 * if Rotator HW is reset, but missing PM event notification, we
	 * need to init the SW timestamp automatically.
	 */
	rststs = SDE_ROTREG_READ(rot->mdss_base, REGDMA_RESET_STATUS_REG);
	if (!rot->reset_hw_ts && rststs) {
		u32 l_ts, h_ts, l_hwts, h_hwts;

		h_hwts = __sde_hw_rotator_get_timestamp(rot,
				ROT_QUEUE_HIGH_PRIORITY);
		l_hwts = __sde_hw_rotator_get_timestamp(rot,
				ROT_QUEUE_LOW_PRIORITY);
		h_ts = atomic_read(&rot->timestamp[ROT_QUEUE_HIGH_PRIORITY]);
		l_ts = atomic_read(&rot->timestamp[ROT_QUEUE_LOW_PRIORITY]);
		SDEROT_EVTLOG(0xbad0, rststs, l_hwts, h_hwts, l_ts, h_ts);

		if (ctx->q_id == ROT_QUEUE_HIGH_PRIORITY) {
			h_ts = (h_ts - 1) & SDE_REGDMA_SWTS_MASK;
			l_ts &= SDE_REGDMA_SWTS_MASK;
		} else {
			l_ts = (l_ts - 1) & SDE_REGDMA_SWTS_MASK;
			h_ts &= SDE_REGDMA_SWTS_MASK;
		}

		SDEROT_DBG("h_ts:0x%x, l_ts;0x%x\n", h_ts, l_ts);
		SDEROT_EVTLOG(0x900d, h_ts, l_ts);
		rot->last_hwts[ROT_QUEUE_LOW_PRIORITY] = l_ts;
		rot->last_hwts[ROT_QUEUE_HIGH_PRIORITY] = h_ts;

		rot->ops.update_ts(rot, ROT_QUEUE_HIGH_PRIORITY, h_ts);
		rot->ops.update_ts(rot, ROT_QUEUE_LOW_PRIORITY, l_ts);
		SDE_ROTREG_WRITE(rot->mdss_base, REGDMA_RESET_STATUS_REG, 0);

		/* ensure write is issued to the rotator HW */
		wmb();
	}

	if (rot->reset_hw_ts) {
		SDEROT_EVTLOG(rot->last_hwts[ROT_QUEUE_LOW_PRIORITY],
				rot->last_hwts[ROT_QUEUE_HIGH_PRIORITY]);
		rot->ops.update_ts(rot, ROT_QUEUE_HIGH_PRIORITY,
				rot->last_hwts[ROT_QUEUE_HIGH_PRIORITY]);
		rot->ops.update_ts(rot, ROT_QUEUE_LOW_PRIORITY,
				rot->last_hwts[ROT_QUEUE_LOW_PRIORITY]);
		SDE_ROTREG_WRITE(rot->mdss_base, REGDMA_RESET_STATUS_REG, 0);

		/* ensure write is issued to the rotator HW */
		wmb();
		rot->reset_hw_ts = false;
	}

	flags = (item->flags & SDE_ROTATION_FLIP_LR) ?
			SDE_ROT_FLAG_FLIP_LR : 0;
	flags |= (item->flags & SDE_ROTATION_FLIP_UD) ?
			SDE_ROT_FLAG_FLIP_UD : 0;
	flags |= (item->flags & SDE_ROTATION_90) ?
			SDE_ROT_FLAG_ROT_90 : 0;
	flags |= (item->flags & SDE_ROTATION_DEINTERLACE) ?
			SDE_ROT_FLAG_DEINTERLACE : 0;
	flags |= (item->flags & SDE_ROTATION_SECURE) ?
			SDE_ROT_FLAG_SECURE_OVERLAY_SESSION : 0;
	flags |= (item->flags & SDE_ROTATION_SECURE_CAMERA) ?
			SDE_ROT_FLAG_SECURE_CAMERA_SESSION : 0;


	sspp_cfg.img_width = item->input.width;
	sspp_cfg.img_height = item->input.height;
	sspp_cfg.fps = entry->perf->config.frame_rate;
	sspp_cfg.bw = entry->perf->bw;
	sspp_cfg.fmt = sde_get_format_params(item->input.format);
	if (!sspp_cfg.fmt) {
		SDEROT_ERR("null format\n");
		ret = -EINVAL;
		goto error;
	}
	sspp_cfg.src_rect = &item->src_rect;
	sspp_cfg.data = &entry->src_buf;
	sde_mdp_get_plane_sizes(sspp_cfg.fmt, item->input.width,
			item->input.height, &sspp_cfg.src_plane,
			0, /* No bwc_mode */
			(flags & SDE_ROT_FLAG_SOURCE_ROTATED_90) ?
					true : false);

	rot->ops.setup_rotator_fetchengine(ctx, ctx->q_id,
			&sspp_cfg, danger_lut, safe_lut,
			entry->dnsc_factor_w, entry->dnsc_factor_h, flags);

	wb_cfg.img_width = item->output.width;
	wb_cfg.img_height = item->output.height;
	wb_cfg.fps = entry->perf->config.frame_rate;
	wb_cfg.bw = entry->perf->bw;
	wb_cfg.fmt = sde_get_format_params(item->output.format);
	if (!wb_cfg.fmt) {
		SDEROT_ERR("null format\n");
		ret = -EINVAL;
		goto error;
	}

	wb_cfg.dst_rect = &item->dst_rect;
	wb_cfg.data = &entry->dst_buf;
	sde_mdp_get_plane_sizes(wb_cfg.fmt, item->output.width,
			item->output.height, &wb_cfg.dst_plane,
			0, /* No bwc_mode */
			(flags & SDE_ROT_FLAG_ROT_90) ? true : false);

	wb_cfg.v_downscale_factor = entry->dnsc_factor_h;
	wb_cfg.h_downscale_factor = entry->dnsc_factor_w;
	wb_cfg.prefill_bw = item->prefill_bw;

	rot->ops.setup_rotator_wbengine(ctx, ctx->q_id, &wb_cfg, flags);

	/* setup VA mapping for debugfs */
	if (rot->dbgmem) {
		sde_hw_rotator_map_vaddr(&ctx->src_dbgbuf,
				&item->input,
				&entry->src_buf);

		sde_hw_rotator_map_vaddr(&ctx->dst_dbgbuf,
				&item->output,
				&entry->dst_buf);
	}

	SDEROT_EVTLOG(ctx->timestamp, flags,
			item->input.width, item->input.height,
			item->output.width, item->output.height,
			entry->src_buf.p[0].addr, entry->dst_buf.p[0].addr,
			item->input.format, item->output.format,
			entry->perf->config.frame_rate);

	/* initialize static vbif setting */
	sde_mdp_init_vbif();

	if (!ctx->sbuf_mode && mdata->default_ot_rd_limit) {
		struct sde_mdp_set_ot_params ot_params;

		memset(&ot_params, 0, sizeof(struct sde_mdp_set_ot_params));
		ot_params.xin_id = XIN_SSPP;
		ot_params.num = 0; /* not used */
		ot_params.width = entry->perf->config.input.width;
		ot_params.height = entry->perf->config.input.height;
		ot_params.fps = entry->perf->config.frame_rate;
		ot_params.reg_off_vbif_lim_conf = MMSS_VBIF_RD_LIM_CONF;
		ot_params.reg_off_mdp_clk_ctrl =
				MMSS_VBIF_NRT_VBIF_CLK_FORCE_CTRL0;
		ot_params.bit_off_mdp_clk_ctrl =
				MMSS_VBIF_NRT_VBIF_CLK_FORCE_CTRL0_XIN0;
		ot_params.fmt = ctx->is_traffic_shaping ?
			SDE_PIX_FMT_ABGR_8888 :
			entry->perf->config.input.format;
		ot_params.rotsts_base = rot->mdss_base + ROTTOP_STATUS;
		ot_params.rotsts_busy_mask = ROT_BUSY_BIT;
		sde_mdp_set_ot_limit(&ot_params);
	}

	if (!ctx->sbuf_mode && mdata->default_ot_wr_limit) {
		struct sde_mdp_set_ot_params ot_params;

		memset(&ot_params, 0, sizeof(struct sde_mdp_set_ot_params));
		ot_params.xin_id = XIN_WRITEBACK;
		ot_params.num = 0; /* not used */
		ot_params.width = entry->perf->config.input.width;
		ot_params.height = entry->perf->config.input.height;
		ot_params.fps = entry->perf->config.frame_rate;
		ot_params.reg_off_vbif_lim_conf = MMSS_VBIF_WR_LIM_CONF;
		ot_params.reg_off_mdp_clk_ctrl =
				MMSS_VBIF_NRT_VBIF_CLK_FORCE_CTRL0;
		ot_params.bit_off_mdp_clk_ctrl =
				MMSS_VBIF_NRT_VBIF_CLK_FORCE_CTRL0_XIN1;
		ot_params.fmt = ctx->is_traffic_shaping ?
			SDE_PIX_FMT_ABGR_8888 :
			entry->perf->config.input.format;
		ot_params.rotsts_base = rot->mdss_base + ROTTOP_STATUS;
		ot_params.rotsts_busy_mask = ROT_BUSY_BIT;
		sde_mdp_set_ot_limit(&ot_params);
	}

	if (test_bit(SDE_QOS_PER_PIPE_LUT, mdata->sde_qos_map))	{
		u32 qos_lut = 0; /* low priority for nrt read client */

		trace_rot_perf_set_qos_luts(XIN_SSPP, sspp_cfg.fmt->format,
			qos_lut, sde_mdp_is_linear_format(sspp_cfg.fmt));

		SDE_ROTREG_WRITE(rot->mdss_base, ROT_SSPP_CREQ_LUT, qos_lut);
	}

	/* VBIF QoS and other settings */
	if (!ctx->sbuf_mode)
		sde_hw_rotator_vbif_setting(rot);

	return 0;

error:
	sde_hw_rotator_free_rotctx(rot, ctx);
	return ret;
}

/*
 * sde_hw_rotator_cancel - cancel hw configuration for the given rotation entry
 * @hw: Pointer to rotator resource
 * @entry: Pointer to rotation entry
 *
 * This function cancels a previously configured rotation entry.
 */
static int sde_hw_rotator_cancel(struct sde_rot_hw_resource *hw,
		struct sde_rot_entry *entry)
{
	struct sde_hw_rotator *rot;
	struct sde_hw_rotator_resource_info *resinfo;
	struct sde_hw_rotator_context *ctx;
	unsigned long flags;

	if (!hw || !entry) {
		SDEROT_ERR("null hw resource/entry\n");
		return -EINVAL;
	}

	resinfo = container_of(hw, struct sde_hw_rotator_resource_info, hw);
	rot = resinfo->rot;

	/* Lookup rotator context from session-id */
	ctx = sde_hw_rotator_get_ctx(rot, entry->item.session_id,
			entry->item.sequence_id, hw->wb_id);
	if (!ctx) {
		SDEROT_ERR("Cannot locate rotator ctx from sesison id:%d\n",
				entry->item.session_id);
		return -EINVAL;
	}

	spin_lock_irqsave(&rot->rotisr_lock, flags);
	rot->ops.update_ts(rot, ctx->q_id, ctx->timestamp);
	spin_unlock_irqrestore(&rot->rotisr_lock, flags);

	SDEROT_EVTLOG(entry->item.session_id, ctx->timestamp);

	if (rot->dbgmem) {
		sde_hw_rotator_unmap_vaddr(&ctx->src_dbgbuf);
		sde_hw_rotator_unmap_vaddr(&ctx->dst_dbgbuf);
	}

	/* Current rotator context job is finished, time to free up */
	sde_hw_rotator_free_rotctx(rot, ctx);

	return 0;
}

/*
 * sde_hw_rotator_kickoff - kickoff processing on the given entry
 * @hw: Pointer to rotator resource
 * @entry: Pointer to rotation entry
 */
static int sde_hw_rotator_kickoff(struct sde_rot_hw_resource *hw,
		struct sde_rot_entry *entry)
{
	struct sde_hw_rotator *rot;
	struct sde_hw_rotator_resource_info *resinfo;
	struct sde_hw_rotator_context *ctx;

	if (!hw || !entry) {
		SDEROT_ERR("null hw resource/entry\n");
		return -EINVAL;
	}

	resinfo = container_of(hw, struct sde_hw_rotator_resource_info, hw);
	rot = resinfo->rot;

	/* Lookup rotator context from session-id */
	ctx = sde_hw_rotator_get_ctx(rot, entry->item.session_id,
			entry->item.sequence_id, hw->wb_id);
	if (!ctx) {
		SDEROT_ERR("Cannot locate rotator ctx from sesison id:%d\n",
				entry->item.session_id);
		return -EINVAL;
	}

	rot->ops.start_rotator(ctx, ctx->q_id);

	return 0;
}

static int sde_hw_rotator_abort_kickoff(struct sde_rot_hw_resource *hw,
		struct sde_rot_entry *entry)
{
	struct sde_hw_rotator *rot;
	struct sde_hw_rotator_resource_info *resinfo;
	struct sde_hw_rotator_context *ctx;
	unsigned long flags;

	if (!hw || !entry) {
		SDEROT_ERR("null hw resource/entry\n");
		return -EINVAL;
	}

	resinfo = container_of(hw, struct sde_hw_rotator_resource_info, hw);
	rot = resinfo->rot;

	/* Lookup rotator context from session-id */
	ctx = sde_hw_rotator_get_ctx(rot, entry->item.session_id,
			entry->item.sequence_id, hw->wb_id);
	if (!ctx) {
		SDEROT_ERR("Cannot locate rotator ctx from sesison id:%d\n",
				entry->item.session_id);
		return -EINVAL;
	}

	spin_lock_irqsave(&rot->rotisr_lock, flags);
	rot->ops.update_ts(rot, ctx->q_id, ctx->timestamp);
	ctx->abort = true;
	wake_up_all(&ctx->regdma_waitq);
	spin_unlock_irqrestore(&rot->rotisr_lock, flags);

	SDEROT_EVTLOG(entry->item.session_id, ctx->timestamp);

	return 0;
}

/*
 * sde_hw_rotator_wait4done - wait for completion notification
 * @hw: Pointer to rotator resource
 * @entry: Pointer to rotation entry
 *
 * This function blocks until the given entry is complete, error
 * is detected, or timeout.
 */
static int sde_hw_rotator_wait4done(struct sde_rot_hw_resource *hw,
		struct sde_rot_entry *entry)
{
	struct sde_hw_rotator *rot;
	struct sde_hw_rotator_resource_info *resinfo;
	struct sde_hw_rotator_context *ctx;
	int ret;

	if (!hw || !entry) {
		SDEROT_ERR("null hw resource/entry\n");
		return -EINVAL;
	}

	resinfo = container_of(hw, struct sde_hw_rotator_resource_info, hw);
	rot = resinfo->rot;

	/* Lookup rotator context from session-id */
	ctx = sde_hw_rotator_get_ctx(rot, entry->item.session_id,
			entry->item.sequence_id, hw->wb_id);
	if (!ctx) {
		SDEROT_ERR("Cannot locate rotator ctx from sesison id:%d\n",
				entry->item.session_id);
		return -EINVAL;
	}

	ret = rot->ops.wait_rotator_done(ctx, ctx->q_id, 0);

	if (rot->dbgmem) {
		sde_hw_rotator_unmap_vaddr(&ctx->src_dbgbuf);
		sde_hw_rotator_unmap_vaddr(&ctx->dst_dbgbuf);
	}

	/* Current rotator context job is finished, time to free up*/
	sde_hw_rotator_free_rotctx(rot, ctx);

	return ret;
}

/*
 * sde_rotator_hw_rev_init - setup feature and/or capability bitmask
 * @rot: Pointer to hw rotator
 *
 * This function initializes feature and/or capability bitmask based on
 * h/w version read from the device.
 */
static int sde_rotator_hw_rev_init(struct sde_hw_rotator *rot)
{
	struct sde_rot_data_type *mdata = sde_rot_get_mdata();
	u32 hw_version;

	if (!mdata) {
		SDEROT_ERR("null rotator data\n");
		return -EINVAL;
	}

	hw_version = SDE_ROTREG_READ(rot->mdss_base, ROTTOP_HW_VERSION);
	SDEROT_DBG("hw version %8.8x\n", hw_version);

	clear_bit(SDE_QOS_PER_PIPE_IB, mdata->sde_qos_map);
	set_bit(SDE_QOS_OVERHEAD_FACTOR, mdata->sde_qos_map);
	set_bit(SDE_QOS_OTLIM, mdata->sde_qos_map);
	set_bit(SDE_QOS_PER_PIPE_LUT, mdata->sde_qos_map);
	clear_bit(SDE_QOS_SIMPLIFIED_PREFILL, mdata->sde_qos_map);

	set_bit(SDE_CAPS_R3_WB, mdata->sde_caps_map);

	/* features exposed via rotator top h/w version */
	if (hw_version != SDE_ROT_TYPE_V1_0) {
		SDEROT_DBG("Supporting 1.5 downscale for SDE Rotator\n");
		set_bit(SDE_CAPS_R3_1P5_DOWNSCALE,  mdata->sde_caps_map);
	}

	set_bit(SDE_CAPS_SEC_ATTACH_DETACH_SMMU, mdata->sde_caps_map);

	mdata->nrt_vbif_dbg_bus = nrt_vbif_dbg_bus_r3;
	mdata->nrt_vbif_dbg_bus_size =
			ARRAY_SIZE(nrt_vbif_dbg_bus_r3);

	mdata->rot_dbg_bus = rot_dbgbus_r3;
	mdata->rot_dbg_bus_size = ARRAY_SIZE(rot_dbgbus_r3);

	mdata->regdump = sde_rot_r3_regdump;
	mdata->regdump_size = ARRAY_SIZE(sde_rot_r3_regdump);
	SDE_ROTREG_WRITE(rot->mdss_base, REGDMA_TIMESTAMP_REG, 0);

	/* features exposed via mdss h/w version */
	if (IS_SDE_MAJOR_MINOR_SAME(mdata->mdss_version, SDE_MDP_HW_REV_500)) {
		SDEROT_DBG("Supporting sys cache inline rotation\n");
		set_bit(SDE_CAPS_SBUF_1,  mdata->sde_caps_map);
		set_bit(SDE_CAPS_UBWC_3,  mdata->sde_caps_map);
		set_bit(SDE_CAPS_PARTIALWR,  mdata->sde_caps_map);
		set_bit(SDE_CAPS_HW_TIMESTAMP, mdata->sde_caps_map);
		rot->inpixfmts[SDE_ROTATOR_MODE_OFFLINE] =
				sde_hw_rotator_v4_inpixfmts;
		rot->num_inpixfmt[SDE_ROTATOR_MODE_OFFLINE] =
				ARRAY_SIZE(sde_hw_rotator_v4_inpixfmts);
		rot->outpixfmts[SDE_ROTATOR_MODE_OFFLINE] =
				sde_hw_rotator_v4_outpixfmts;
		rot->num_outpixfmt[SDE_ROTATOR_MODE_OFFLINE] =
				ARRAY_SIZE(sde_hw_rotator_v4_outpixfmts);
		rot->inpixfmts[SDE_ROTATOR_MODE_SBUF] =
				sde_hw_rotator_v4_inpixfmts_sbuf;
		rot->num_inpixfmt[SDE_ROTATOR_MODE_SBUF] =
				ARRAY_SIZE(sde_hw_rotator_v4_inpixfmts_sbuf);
		rot->outpixfmts[SDE_ROTATOR_MODE_SBUF] =
				sde_hw_rotator_v4_outpixfmts_sbuf;
		rot->num_outpixfmt[SDE_ROTATOR_MODE_SBUF] =
				ARRAY_SIZE(sde_hw_rotator_v4_outpixfmts_sbuf);
		rot->downscale_caps =
			"LINEAR/1.5/2/4/8/16/32/64 TILE/1.5/2/4 TP10/1.5/2";
	} else if (IS_SDE_MAJOR_MINOR_SAME(mdata->mdss_version,
				SDE_MDP_HW_REV_400) ||
			IS_SDE_MAJOR_MINOR_SAME(mdata->mdss_version,
				SDE_MDP_HW_REV_410)) {
		SDEROT_DBG("Supporting sys cache inline rotation\n");
		set_bit(SDE_CAPS_SBUF_1,  mdata->sde_caps_map);
		set_bit(SDE_CAPS_UBWC_2,  mdata->sde_caps_map);
		set_bit(SDE_CAPS_PARTIALWR,  mdata->sde_caps_map);
		rot->inpixfmts[SDE_ROTATOR_MODE_OFFLINE] =
				sde_hw_rotator_v4_inpixfmts;
		rot->num_inpixfmt[SDE_ROTATOR_MODE_OFFLINE] =
				ARRAY_SIZE(sde_hw_rotator_v4_inpixfmts);
		rot->outpixfmts[SDE_ROTATOR_MODE_OFFLINE] =
				sde_hw_rotator_v4_outpixfmts;
		rot->num_outpixfmt[SDE_ROTATOR_MODE_OFFLINE] =
				ARRAY_SIZE(sde_hw_rotator_v4_outpixfmts);
		rot->inpixfmts[SDE_ROTATOR_MODE_SBUF] =
				sde_hw_rotator_v4_inpixfmts_sbuf;
		rot->num_inpixfmt[SDE_ROTATOR_MODE_SBUF] =
				ARRAY_SIZE(sde_hw_rotator_v4_inpixfmts_sbuf);
		rot->outpixfmts[SDE_ROTATOR_MODE_SBUF] =
				sde_hw_rotator_v4_outpixfmts_sbuf;
		rot->num_outpixfmt[SDE_ROTATOR_MODE_SBUF] =
				ARRAY_SIZE(sde_hw_rotator_v4_outpixfmts_sbuf);
		rot->downscale_caps =
			"LINEAR/1.5/2/4/8/16/32/64 TILE/1.5/2/4 TP10/1.5/2";
	} else {
		rot->inpixfmts[SDE_ROTATOR_MODE_OFFLINE] =
				sde_hw_rotator_v3_inpixfmts;
		rot->num_inpixfmt[SDE_ROTATOR_MODE_OFFLINE] =
				ARRAY_SIZE(sde_hw_rotator_v3_inpixfmts);
		rot->outpixfmts[SDE_ROTATOR_MODE_OFFLINE] =
				sde_hw_rotator_v3_outpixfmts;
		rot->num_outpixfmt[SDE_ROTATOR_MODE_OFFLINE] =
				ARRAY_SIZE(sde_hw_rotator_v3_outpixfmts);
		rot->downscale_caps = (hw_version == SDE_ROT_TYPE_V1_0) ?
			"LINEAR/2/4/8/16/32/64 TILE/2/4 TP10/2" :
			"LINEAR/1.5/2/4/8/16/32/64 TILE/1.5/2/4 TP10/1.5/2";
	}

	return 0;
}

/*
 * sde_hw_rotator_rotirq_handler - non-regdma interrupt handler
 * @irq: Interrupt number
 * @ptr: Pointer to private handle provided during registration
 *
 * This function services rotator interrupt and wakes up waiting client
 * with pending rotation requests already submitted to h/w.
 */
static irqreturn_t sde_hw_rotator_rotirq_handler(int irq, void *ptr)
{
	struct sde_hw_rotator *rot = ptr;
	struct sde_hw_rotator_context *ctx;
	irqreturn_t ret = IRQ_NONE;
	u32 isr;

	isr = SDE_ROTREG_READ(rot->mdss_base, ROTTOP_INTR_STATUS);

	SDEROT_DBG("intr_status = %8.8x\n", isr);

	if (isr & ROT_DONE_MASK) {
		if (rot->irq_num >= 0)
			sde_hw_rotator_disable_irq(rot);
		SDEROT_DBG("Notify rotator complete\n");

		/* Normal rotator only 1 session, no need to lookup */
		ctx = rot->rotCtx[0][0];
		WARN_ON(ctx == NULL);
		complete_all(&ctx->rot_comp);

		spin_lock(&rot->rotisr_lock);
		SDE_ROTREG_WRITE(rot->mdss_base, ROTTOP_INTR_CLEAR,
				ROT_DONE_CLEAR);
		spin_unlock(&rot->rotisr_lock);
		ret = IRQ_HANDLED;
	}

	return ret;
}

/*
 * sde_hw_rotator_regdmairq_handler - regdma interrupt handler
 * @irq: Interrupt number
 * @ptr: Pointer to private handle provided during registration
 *
 * This function services rotator interrupt, decoding the source of
 * events (high/low priority queue), and wakes up all waiting clients
 * with pending rotation requests already submitted to h/w.
 */
static irqreturn_t sde_hw_rotator_regdmairq_handler(int irq, void *ptr)
{
	struct sde_rot_data_type *mdata = sde_rot_get_mdata();
	struct sde_hw_rotator *rot = ptr;
	struct sde_hw_rotator_context *ctx, *tmp;
	irqreturn_t ret = IRQ_NONE;
	u32 isr, isr_tmp;
	u32 ts;
	u32 q_id;

	isr = SDE_ROTREG_READ(rot->mdss_base, REGDMA_CSR_REGDMA_INT_STATUS);
	/* acknowledge interrupt before reading latest timestamp */
	SDE_ROTREG_WRITE(rot->mdss_base, REGDMA_CSR_REGDMA_INT_CLEAR, isr);

	SDEROT_DBG("intr_status = %8.8x\n", isr);

	/* Any REGDMA status, including error and watchdog timer, should
	 * trigger and wake up waiting thread
	 */
	if (isr & (REGDMA_INT_HIGH_MASK | REGDMA_INT_LOW_MASK)) {
		spin_lock(&rot->rotisr_lock);

		/*
		 * Obtain rotator context based on timestamp from regdma
		 * and low/high interrupt status
		 */
		if (isr & REGDMA_INT_HIGH_MASK) {
			q_id = ROT_QUEUE_HIGH_PRIORITY;
		} else if (isr & REGDMA_INT_LOW_MASK) {
			q_id = ROT_QUEUE_LOW_PRIORITY;
		} else {
			SDEROT_ERR("unknown ISR status: isr=0x%X\n", isr);
			goto done_isr_handle;
		}

		ts = __sde_hw_rotator_get_timestamp(rot, q_id);

		/*
		 * Timestamp packet is not available in sbuf mode.
		 * Simulate timestamp update in the handler instead.
		 */
		if (test_bit(SDE_CAPS_HW_TIMESTAMP, mdata->sde_caps_map) ||
				list_empty(&rot->sbuf_ctx[q_id]))
			goto skip_sbuf;

		ctx = NULL;
		isr_tmp = isr;
		list_for_each_entry(tmp, &rot->sbuf_ctx[q_id], list) {
			u32 mask;

			mask = tmp->timestamp & 0x1 ? REGDMA_INT_1_MASK :
				REGDMA_INT_0_MASK;
			if (isr_tmp & mask) {
				isr_tmp &= ~mask;
				ctx = tmp;
				ts = ctx->timestamp;
				rot->ops.update_ts(rot, ctx->q_id, ts);
				SDEROT_DBG("update swts:0x%X\n", ts);
			}
			SDEROT_EVTLOG(isr, tmp->timestamp);
		}
		if (ctx == NULL)
			SDEROT_ERR("invalid swts ctx\n");
skip_sbuf:
		ctx = rot->rotCtx[q_id][ts & SDE_HW_ROT_REGDMA_SEG_MASK];

		/*
		 * Wake up all waiting context from the current and previous
		 * SW Timestamp.
		 */
		while (ctx &&
			sde_hw_rotator_elapsed_swts(ctx->timestamp, ts) >= 0) {
			ctx->last_regdma_isr_status = isr;
			ctx->last_regdma_timestamp  = ts;
			SDEROT_DBG(
				"regdma complete: ctx:%p, ts:%X\n", ctx, ts);
			wake_up_all(&ctx->regdma_waitq);

			ts  = (ts - 1) & SDE_REGDMA_SWTS_MASK;
			ctx = rot->rotCtx[q_id]
				[ts & SDE_HW_ROT_REGDMA_SEG_MASK];
		};

done_isr_handle:
		spin_unlock(&rot->rotisr_lock);
		ret = IRQ_HANDLED;
	} else if (isr & REGDMA_INT_ERR_MASK) {
		/*
		 * For REGDMA Err, we save the isr info and wake up
		 * all waiting contexts
		 */
		int i, j;

		SDEROT_ERR(
			"regdma err isr:%X, wake up all waiting contexts\n",
			isr);

		spin_lock(&rot->rotisr_lock);

		for (i = 0; i < ROT_QUEUE_MAX; i++) {
			for (j = 0; j < SDE_HW_ROT_REGDMA_TOTAL_CTX; j++) {
				ctx = rot->rotCtx[i][j];
				if (ctx && ctx->last_regdma_isr_status == 0) {
					ts = __sde_hw_rotator_get_timestamp(
							rot, i);
					ctx->last_regdma_isr_status = isr;
					ctx->last_regdma_timestamp  = ts;
					wake_up_all(&ctx->regdma_waitq);
					SDEROT_DBG("Wakeup rotctx[%d][%d]:%p\n",
							i, j, ctx);
				}
			}
		}

		spin_unlock(&rot->rotisr_lock);
		ret = IRQ_HANDLED;
	}

	return ret;
}

/*
 * sde_hw_rotator_validate_entry - validate rotation entry
 * @mgr: Pointer to rotator manager
 * @entry: Pointer to rotation entry
 *
 * This function validates the given rotation entry and provides possible
 * fixup (future improvement) if available.  This function returns 0 if
 * the entry is valid, and returns error code otherwise.
 */
static int sde_hw_rotator_validate_entry(struct sde_rot_mgr *mgr,
		struct sde_rot_entry *entry)
{
	struct sde_rot_data_type *mdata = sde_rot_get_mdata();
	struct sde_hw_rotator *hw_data;
	int ret = 0;
	u16 src_w, src_h, dst_w, dst_h;
	struct sde_rotation_item *item = &entry->item;
	struct sde_mdp_format_params *fmt;

	if (!mgr || !entry || !mgr->hw_data) {
		SDEROT_ERR("invalid parameters\n");
		return -EINVAL;
	}

	hw_data = mgr->hw_data;

	if (hw_data->maxlinewidth < item->src_rect.w) {
		SDEROT_ERR("invalid src width %u\n", item->src_rect.w);
		return -EINVAL;
	}

	src_w = item->src_rect.w;
	src_h = item->src_rect.h;

	if (item->flags & SDE_ROTATION_90) {
		dst_w = item->dst_rect.h;
		dst_h = item->dst_rect.w;
	} else {
		dst_w = item->dst_rect.w;
		dst_h = item->dst_rect.h;
	}

	entry->dnsc_factor_w = 0;
	entry->dnsc_factor_h = 0;

	if (item->output.sbuf &&
			!test_bit(SDE_CAPS_SBUF_1, mdata->sde_caps_map)) {
		SDEROT_ERR("stream buffer not supported\n");
		return -EINVAL;
	}

	if ((src_w != dst_w) || (src_h != dst_h)) {
		if (!dst_w || !dst_h) {
			SDEROT_DBG("zero output width/height not support\n");
			ret = -EINVAL;
			goto dnsc_err;
		}
		if ((src_w % dst_w) || (src_h % dst_h)) {
			SDEROT_DBG("non integral scale not support\n");
			ret = -EINVAL;
			goto dnsc_1p5_check;
		}
		entry->dnsc_factor_w = src_w / dst_w;
		if ((entry->dnsc_factor_w & (entry->dnsc_factor_w - 1)) ||
				(entry->dnsc_factor_w > 64)) {
			SDEROT_DBG("non power-of-2 w_scale not support\n");
			ret = -EINVAL;
			goto dnsc_err;
		}
		entry->dnsc_factor_h = src_h / dst_h;
		if ((entry->dnsc_factor_h & (entry->dnsc_factor_h - 1)) ||
				(entry->dnsc_factor_h > 64)) {
			SDEROT_DBG("non power-of-2 h_scale not support\n");
			ret = -EINVAL;
			goto dnsc_err;
		}
	}

	fmt = sde_get_format_params(item->output.format);
	/*
	 * Rotator downscale support max 4 times for UBWC format and
	 * max 2 times for TP10/TP10_UBWC format
	 */
	if (sde_mdp_is_ubwc_format(fmt) && (entry->dnsc_factor_h > 4)) {
		SDEROT_DBG("max downscale for UBWC format is 4\n");
		ret = -EINVAL;
		goto dnsc_err;
	}
	if (sde_mdp_is_tp10_format(fmt) && (entry->dnsc_factor_h > 2)) {
		SDEROT_DBG("downscale with TP10 cannot be more than 2\n");
		ret = -EINVAL;
	}
	goto dnsc_err;

dnsc_1p5_check:
	/* Check for 1.5 downscale that only applies to V2 HW */
	if (test_bit(SDE_CAPS_R3_1P5_DOWNSCALE, mdata->sde_caps_map)) {
		entry->dnsc_factor_w = src_w / dst_w;
		if ((entry->dnsc_factor_w != 1) ||
				((dst_w * 3) != (src_w * 2))) {
			SDEROT_DBG(
				"No supporting non 1.5 downscale width ratio, src_w:%d, dst_w:%d\n",
				src_w, dst_w);
			ret = -EINVAL;
			goto dnsc_err;
		}

		entry->dnsc_factor_h = src_h / dst_h;
		if ((entry->dnsc_factor_h != 1) ||
				((dst_h * 3) != (src_h * 2))) {
			SDEROT_DBG(
				"Not supporting non 1.5 downscale height ratio, src_h:%d, dst_h:%d\n",
				src_h, dst_h);
			ret = -EINVAL;
			goto dnsc_err;
		}
		ret = 0;
	}

dnsc_err:
	/* Downscaler does not support asymmetrical dnsc */
	if (entry->dnsc_factor_w != entry->dnsc_factor_h) {
		SDEROT_DBG("asymmetric downscale not support\n");
		ret = -EINVAL;
	}

	if (ret) {
		entry->dnsc_factor_w = 0;
		entry->dnsc_factor_h = 0;
	}
	return ret;
}

/*
 * sde_hw_rotator_show_caps - output capability info to sysfs 'caps' file
 * @mgr: Pointer to rotator manager
 * @attr: Pointer to device attribute interface
 * @buf: Pointer to output buffer
 * @len: Length of output buffer
 */
static ssize_t sde_hw_rotator_show_caps(struct sde_rot_mgr *mgr,
		struct device_attribute *attr, char *buf, ssize_t len)
{
	struct sde_hw_rotator *hw_data;
	struct sde_rot_data_type *mdata = sde_rot_get_mdata();
	int cnt = 0;

	if (!mgr || !buf)
		return 0;

	hw_data = mgr->hw_data;

#define SPRINT(fmt, ...) \
		(cnt += scnprintf(buf + cnt, len - cnt, fmt, ##__VA_ARGS__))

	/* insert capabilities here */
	if (test_bit(SDE_CAPS_R3_1P5_DOWNSCALE, mdata->sde_caps_map))
		SPRINT("min_downscale=1.5\n");
	else
		SPRINT("min_downscale=2.0\n");

	SPRINT("downscale_compression=1\n");

	if (hw_data->downscale_caps)
		SPRINT("downscale_ratios=%s\n", hw_data->downscale_caps);

#undef SPRINT
	return cnt;
}

/*
 * sde_hw_rotator_show_state - output state info to sysfs 'state' file
 * @mgr: Pointer to rotator manager
 * @attr: Pointer to device attribute interface
 * @buf: Pointer to output buffer
 * @len: Length of output buffer
 */
static ssize_t sde_hw_rotator_show_state(struct sde_rot_mgr *mgr,
		struct device_attribute *attr, char *buf, ssize_t len)
{
	struct sde_hw_rotator *rot;
	struct sde_hw_rotator_context *ctx;
	int cnt = 0;
	int num_active = 0;
	int i, j;

	if (!mgr || !buf) {
		SDEROT_ERR("null parameters\n");
		return 0;
	}

	rot = mgr->hw_data;

#define SPRINT(fmt, ...) \
		(cnt += scnprintf(buf + cnt, len - cnt, fmt, ##__VA_ARGS__))

	if (rot) {
		SPRINT("rot_mode=%d\n", rot->mode);
		SPRINT("irq_num=%d\n", rot->irq_num);

		if (rot->mode == ROT_REGDMA_OFF) {
			SPRINT("max_active=1\n");
			SPRINT("num_active=%d\n", rot->rotCtx[0][0] ? 1 : 0);
		} else {
			for (i = 0; i < ROT_QUEUE_MAX; i++) {
				for (j = 0; j < SDE_HW_ROT_REGDMA_TOTAL_CTX;
						j++) {
					ctx = rot->rotCtx[i][j];

					if (ctx) {
						SPRINT(
							"rotCtx[%d][%d]:%p\n",
							i, j, ctx);
						++num_active;
					}
				}
			}

			SPRINT("max_active=%d\n", SDE_HW_ROT_REGDMA_TOTAL_CTX);
			SPRINT("num_active=%d\n", num_active);
		}
	}

#undef SPRINT
	return cnt;
}

/*
 * sde_hw_rotator_get_pixfmt - get the indexed pixel format
 * @mgr: Pointer to rotator manager
 * @index: index of pixel format
 * @input: true for input port; false for output port
 * @mode: operating mode
 */
static u32 sde_hw_rotator_get_pixfmt(struct sde_rot_mgr *mgr,
		int index, bool input, u32 mode)
{
	struct sde_hw_rotator *rot;

	if (!mgr || !mgr->hw_data) {
		SDEROT_ERR("null parameters\n");
		return 0;
	}

	rot = mgr->hw_data;

	if (mode >= SDE_ROTATOR_MODE_MAX) {
		SDEROT_ERR("invalid rotator mode %d\n", mode);
		return 0;
	}

	if (input) {
		if ((index < rot->num_inpixfmt[mode]) && rot->inpixfmts[mode])
			return rot->inpixfmts[mode][index];
		else
			return 0;
	} else {
		if ((index < rot->num_outpixfmt[mode]) && rot->outpixfmts[mode])
			return rot->outpixfmts[mode][index];
		else
			return 0;
	}
}

/*
 * sde_hw_rotator_is_valid_pixfmt - verify if the given pixel format is valid
 * @mgr: Pointer to rotator manager
 * @pixfmt: pixel format to be verified
 * @input: true for input port; false for output port
 * @mode: operating mode
 */
static int sde_hw_rotator_is_valid_pixfmt(struct sde_rot_mgr *mgr, u32 pixfmt,
		bool input, u32 mode)
{
	struct sde_hw_rotator *rot;
	const u32 *pixfmts;
	u32 num_pixfmt;
	int i;

	if (!mgr || !mgr->hw_data) {
		SDEROT_ERR("null parameters\n");
		return false;
	}

	rot = mgr->hw_data;

	if (mode >= SDE_ROTATOR_MODE_MAX) {
		SDEROT_ERR("invalid rotator mode %d\n", mode);
		return false;
	}

	if (input) {
		pixfmts = rot->inpixfmts[mode];
		num_pixfmt = rot->num_inpixfmt[mode];
	} else {
		pixfmts = rot->outpixfmts[mode];
		num_pixfmt = rot->num_outpixfmt[mode];
	}

	if (!pixfmts || !num_pixfmt) {
		SDEROT_ERR("invalid pixel format tables\n");
		return false;
	}

	for (i = 0; i < num_pixfmt; i++)
		if (pixfmts[i] == pixfmt)
			return true;

	return false;
}

/*
 * sde_hw_rotator_get_downscale_caps - get scaling capability string
 * @mgr: Pointer to rotator manager
 * @caps: Pointer to capability string buffer; NULL to return maximum length
 * @len: length of capability string buffer
 * return: length of capability string
 */
static int sde_hw_rotator_get_downscale_caps(struct sde_rot_mgr *mgr,
		char *caps, int len)
{
	struct sde_hw_rotator *rot;
	int rc = 0;

	if (!mgr || !mgr->hw_data) {
		SDEROT_ERR("null parameters\n");
		return -EINVAL;
	}

	rot = mgr->hw_data;

	if (rot->downscale_caps) {
		if (caps)
			rc = snprintf(caps, len, "%s", rot->downscale_caps);
		else
			rc = strlen(rot->downscale_caps);
	}

	return rc;
}

/*
 * sde_hw_rotator_get_maxlinewidth - get maximum line width supported
 * @mgr: Pointer to rotator manager
 * return: maximum line width supported by hardware
 */
static int sde_hw_rotator_get_maxlinewidth(struct sde_rot_mgr *mgr)
{
	struct sde_hw_rotator *rot;

	if (!mgr || !mgr->hw_data) {
		SDEROT_ERR("null parameters\n");
		return -EINVAL;
	}

	rot = mgr->hw_data;

	return rot->maxlinewidth;
}

/*
 * sde_hw_rotator_dump_status - dump status to debug output
 * @mgr: Pointer to rotator manager
 * return: none
 */
static void sde_hw_rotator_dump_status(struct sde_rot_mgr *mgr)
{
	if (!mgr || !mgr->hw_data) {
		SDEROT_ERR("null parameters\n");
		return;
	}

	_sde_hw_rotator_dump_status(mgr->hw_data, NULL);
}

/*
 * sde_hw_rotator_parse_dt - parse r3 specific device tree settings
 * @hw_data: Pointer to rotator hw
 * @dev: Pointer to platform device
 */
static int sde_hw_rotator_parse_dt(struct sde_hw_rotator *hw_data,
		struct platform_device *dev)
{
	int ret = 0;
	u32 data;

	if (!hw_data || !dev)
		return -EINVAL;

	ret = of_property_read_u32(dev->dev.of_node, "qcom,mdss-rot-mode",
			&data);
	if (ret) {
		SDEROT_DBG("default to regdma off\n");
		ret = 0;
		hw_data->mode = ROT_REGDMA_OFF;
	} else if (data < ROT_REGDMA_MAX) {
		SDEROT_DBG("set to regdma mode %d\n", data);
		hw_data->mode = data;
	} else {
		SDEROT_ERR("regdma mode out of range. default to regdma off\n");
		hw_data->mode = ROT_REGDMA_OFF;
	}

	ret = of_property_read_u32(dev->dev.of_node,
			"qcom,mdss-highest-bank-bit", &data);
	if (ret) {
		SDEROT_DBG("default to A5X bank\n");
		ret = 0;
		hw_data->highest_bank = 2;
	} else {
		SDEROT_DBG("set highest bank bit to %d\n", data);
		hw_data->highest_bank = data;
	}

	ret = of_property_read_u32(dev->dev.of_node,
			"qcom,sde-ubwc-malsize", &data);
	if (ret) {
		ret = 0;
		hw_data->ubwc_malsize = DEFAULT_UBWC_MALSIZE;
	} else {
		SDEROT_DBG("set ubwc malsize to %d\n", data);
		hw_data->ubwc_malsize = data;
	}

	ret = of_property_read_u32(dev->dev.of_node,
			"qcom,sde-ubwc_swizzle", &data);
	if (ret) {
		ret = 0;
		hw_data->ubwc_swizzle = DEFAULT_UBWC_SWIZZLE;
	} else {
		SDEROT_DBG("set ubwc swizzle to %d\n", data);
		hw_data->ubwc_swizzle = data;
	}

	ret = of_property_read_u32(dev->dev.of_node,
			"qcom,mdss-sbuf-headroom", &data);
	if (ret) {
		ret = 0;
		hw_data->sbuf_headroom = DEFAULT_SBUF_HEADROOM;
	} else {
		SDEROT_DBG("set sbuf headroom to %d\n", data);
		hw_data->sbuf_headroom = data;
	}

	ret = of_property_read_u32(dev->dev.of_node,
			"qcom,mdss-rot-linewidth", &data);
	if (ret) {
		ret = 0;
		hw_data->maxlinewidth = DEFAULT_MAXLINEWIDTH;
	} else {
		SDEROT_DBG("set mdss-rot-linewidth to %d\n", data);
		hw_data->maxlinewidth = data;
	}

	return ret;
}

/*
 * sde_rotator_r3_init - initialize the r3 module
 * @mgr: Pointer to rotator manager
 *
 * This function setup r3 callback functions, parses r3 specific
 * device tree settings, installs r3 specific interrupt handler,
 * as well as initializes r3 internal data structure.
 */
int sde_rotator_r3_init(struct sde_rot_mgr *mgr)
{
	struct sde_hw_rotator *rot;
	struct sde_rot_data_type *mdata = sde_rot_get_mdata();
	int i;
	int ret;

	rot = devm_kzalloc(&mgr->pdev->dev, sizeof(*rot), GFP_KERNEL);
	if (!rot)
		return -ENOMEM;

	mgr->hw_data = rot;
	mgr->queue_count = ROT_QUEUE_MAX;

	rot->mdss_base = mdata->sde_io.base;
	rot->pdev      = mgr->pdev;
	rot->koff_timeout = KOFF_TIMEOUT;
	rot->vid_trigger = ROTTOP_START_CTRL_TRIG_SEL_MDP;
	rot->cmd_trigger = ROTTOP_START_CTRL_TRIG_SEL_MDP;

	/* Assign ops */
	mgr->ops_hw_destroy = sde_hw_rotator_destroy;
	mgr->ops_hw_alloc = sde_hw_rotator_alloc_ext;
	mgr->ops_hw_free = sde_hw_rotator_free_ext;
	mgr->ops_config_hw = sde_hw_rotator_config;
	mgr->ops_cancel_hw = sde_hw_rotator_cancel;
	mgr->ops_abort_hw = sde_hw_rotator_abort_kickoff;
	mgr->ops_kickoff_entry = sde_hw_rotator_kickoff;
	mgr->ops_wait_for_entry = sde_hw_rotator_wait4done;
	mgr->ops_hw_validate_entry = sde_hw_rotator_validate_entry;
	mgr->ops_hw_show_caps = sde_hw_rotator_show_caps;
	mgr->ops_hw_show_state = sde_hw_rotator_show_state;
	mgr->ops_hw_create_debugfs = sde_rotator_r3_create_debugfs;
	mgr->ops_hw_get_pixfmt = sde_hw_rotator_get_pixfmt;
	mgr->ops_hw_is_valid_pixfmt = sde_hw_rotator_is_valid_pixfmt;
	mgr->ops_hw_pre_pmevent = sde_hw_rotator_pre_pmevent;
	mgr->ops_hw_post_pmevent = sde_hw_rotator_post_pmevent;
	mgr->ops_hw_get_downscale_caps = sde_hw_rotator_get_downscale_caps;
	mgr->ops_hw_get_maxlinewidth = sde_hw_rotator_get_maxlinewidth;
	mgr->ops_hw_dump_status = sde_hw_rotator_dump_status;

	ret = sde_hw_rotator_parse_dt(mgr->hw_data, mgr->pdev);
	if (ret)
		goto error_parse_dt;

	rot->irq_num = platform_get_irq(mgr->pdev, 0);
	if (rot->irq_num == -EPROBE_DEFER) {
		SDEROT_INFO("irq master master not ready, defer probe\n");
		return -EPROBE_DEFER;
	} else if (rot->irq_num < 0) {
		SDEROT_ERR("fail to get rotator irq, fallback to polling\n");
	} else {
		if (rot->mode == ROT_REGDMA_OFF)
			ret = devm_request_threaded_irq(&mgr->pdev->dev,
					rot->irq_num,
					sde_hw_rotator_rotirq_handler,
					NULL, 0, "sde_rotator_r3", rot);
		else
			ret = devm_request_threaded_irq(&mgr->pdev->dev,
					rot->irq_num,
					sde_hw_rotator_regdmairq_handler,
					NULL, 0, "sde_rotator_r3", rot);
		if (ret) {
			SDEROT_ERR("fail to request irq r:%d\n", ret);
			rot->irq_num = -1;
		} else {
			disable_irq(rot->irq_num);
		}
	}
	atomic_set(&rot->irq_enabled, 0);

	ret = sde_rotator_hw_rev_init(rot);
	if (ret)
		goto error_hw_rev_init;

	setup_rotator_ops(&rot->ops, rot->mode,
			test_bit(SDE_CAPS_HW_TIMESTAMP, mdata->sde_caps_map));

	spin_lock_init(&rot->rotctx_lock);
	spin_lock_init(&rot->rotisr_lock);

	/* REGDMA initialization */
	if (rot->mode == ROT_REGDMA_OFF) {
		for (i = 0; i < SDE_HW_ROT_REGDMA_TOTAL_CTX; i++)
			rot->cmd_wr_ptr[0][i] = (char __iomem *)(
					&rot->cmd_queue[
					SDE_HW_ROT_REGDMA_SEG_SIZE * i]);
	} else {
		for (i = 0; i < SDE_HW_ROT_REGDMA_TOTAL_CTX; i++)
			rot->cmd_wr_ptr[ROT_QUEUE_HIGH_PRIORITY][i] =
				rot->mdss_base +
					REGDMA_RAM_REGDMA_CMD_RAM +
					SDE_HW_ROT_REGDMA_SEG_SIZE * 4 * i;

		for (i = 0; i < SDE_HW_ROT_REGDMA_TOTAL_CTX; i++)
			rot->cmd_wr_ptr[ROT_QUEUE_LOW_PRIORITY][i] =
				rot->mdss_base +
					REGDMA_RAM_REGDMA_CMD_RAM +
					SDE_HW_ROT_REGDMA_SEG_SIZE * 4 *
					(i + SDE_HW_ROT_REGDMA_TOTAL_CTX);
	}

	for (i = 0; i < ROT_QUEUE_MAX; i++) {
		atomic_set(&rot->timestamp[i], 0);
		INIT_LIST_HEAD(&rot->sbuf_ctx[i]);
	}

	/* set rotator CBCR to shutoff memory/periphery on clock off.*/
	clk_set_flags(mgr->rot_clk[SDE_ROTATOR_CLK_MDSS_ROT].clk,
			CLKFLAG_NORETAIN_MEM);
	clk_set_flags(mgr->rot_clk[SDE_ROTATOR_CLK_MDSS_ROT].clk,
			CLKFLAG_NORETAIN_PERIPH);

	mdata->sde_rot_hw = rot;
	return 0;
error_hw_rev_init:
	if (rot->irq_num >= 0)
		devm_free_irq(&mgr->pdev->dev, rot->irq_num, mdata);
	devm_kfree(&mgr->pdev->dev, mgr->hw_data);
error_parse_dt:
	return ret;
}
