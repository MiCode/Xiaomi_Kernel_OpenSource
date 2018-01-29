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
 *
 */

#define pr_fmt(fmt)	"%s: " fmt, __func__

#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/file.h>
#include <linux/sync.h>
#include <linux/delay.h>
#include <linux/debugfs.h>
#include <linux/interrupt.h>
#include <linux/dma-mapping.h>
#include <linux/dma-buf.h>
#include <linux/msm_ion.h>

#include "sde_rotator_core.h"
#include "sde_rotator_util.h"
#include "sde_rotator_smmu.h"
#include "sde_rotator_r3.h"
#include "sde_rotator_r3_internal.h"
#include "sde_rotator_r3_hwio.h"
#include "sde_rotator_r3_debug.h"
#include "sde_rotator_trace.h"

/* XIN mapping */
#define XIN_SSPP		0
#define XIN_WRITEBACK		1

/* wait for at most 2 vsync for lowest refresh rate (24hz) */
#define KOFF_TIMEOUT msecs_to_jiffies(42 * 32)

/* Macro for constructing the REGDMA command */
#define SDE_REGDMA_WRITE(p, off, data) \
	do { \
		*p++ = REGDMA_OP_REGWRITE | \
			((off) & REGDMA_ADDR_OFFSET_MASK); \
		*p++ = (data); \
	} while (0)

#define SDE_REGDMA_MODIFY(p, off, mask, data) \
	do { \
		*p++ = REGDMA_OP_REGMODIFY | \
			((off) & REGDMA_ADDR_OFFSET_MASK); \
		*p++ = (mask); \
		*p++ = (data); \
	} while (0)

#define SDE_REGDMA_BLKWRITE_INC(p, off, len) \
	do { \
		*p++ = REGDMA_OP_BLKWRITE_INC | \
			((off) & REGDMA_ADDR_OFFSET_MASK); \
		*p++ = (len); \
	} while (0)

#define SDE_REGDMA_BLKWRITE_DATA(p, data) \
	do { \
		*(p) = (data); \
		(p)++; \
	} while (0)

/* Macro for directly accessing mapped registers */
#define SDE_ROTREG_WRITE(base, off, data) \
	writel_relaxed(data, (base + (off)))

#define SDE_ROTREG_READ(base, off) \
	readl_relaxed(base + (off))

/**
 * sde_hw_rotator_get_ctx(): Retrieve rotator context from rotator HW based
 * on provided session_id. Each rotator has a different session_id.
 */
static struct sde_hw_rotator_context *sde_hw_rotator_get_ctx(
		struct sde_hw_rotator *rot, u32 session_id,
		enum sde_rot_queue_prio q_id)
{
	int i;
	struct sde_hw_rotator_context  *ctx = NULL;

	for (i = 0; i < SDE_HW_ROT_REGDMA_TOTAL_CTX; i++) {
		ctx = rot->rotCtx[q_id][i];

		if (ctx && (ctx->session_id == session_id)) {
			SDEROT_DBG(
				"rotCtx sloti[%d][%d] ==> ctx:%p | session-id:%d\n",
				q_id, i, ctx, ctx->session_id);
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
		dma_buf_begin_cpu_access(dbgbuf->dmabuf, 0, dbgbuf->buflen,
				DMA_FROM_DEVICE);
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
		dma_buf_end_cpu_access(dbgbuf->dmabuf, 0, dbgbuf->buflen,
				DMA_FROM_DEVICE);
	}

	dbgbuf->vaddr  = NULL;
	dbgbuf->dmabuf = NULL;
	dbgbuf->buflen = 0;
	dbgbuf->width  = 0;
	dbgbuf->height = 0;
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
	u32 *wrptr;

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
	SDE_REGDMA_BLKWRITE_INC(wrptr, ROT_WB_DST_FORMAT, 4);
	SDE_REGDMA_BLKWRITE_DATA(wrptr, 0x000037FF);
	SDE_REGDMA_BLKWRITE_DATA(wrptr, 0);
	SDE_REGDMA_BLKWRITE_DATA(wrptr, 0x03020100);
	SDE_REGDMA_BLKWRITE_DATA(wrptr, ctx->ts_addr);
	SDE_REGDMA_WRITE(wrptr, ROT_WB_DST_YSTRIDE0, 4);
	SDE_REGDMA_WRITE(wrptr, ROT_WB_OUT_SIZE, 0x00010001);
	SDE_REGDMA_WRITE(wrptr, ROT_WB_OUT_IMG_SIZE, 0x00010001);
	SDE_REGDMA_WRITE(wrptr, ROT_WB_OUT_XY, 0);
	SDE_REGDMA_WRITE(wrptr, ROTTOP_DNSC, 0);
	SDE_REGDMA_WRITE(wrptr, ROTTOP_OP_MODE, 1);
	SDE_REGDMA_MODIFY(wrptr, REGDMA_TIMESTAMP_REG, mask, swts);
	SDE_REGDMA_WRITE(wrptr, ROTTOP_START_CTRL, 1);

	sde_hw_rotator_put_regdma_segment(ctx, wrptr);
}

/*
 * sde_hw_rotator_setup_fetchengine - setup fetch engine
 * @ctx: Pointer to rotator context
 * @queue_id: Priority queue identifier
 * @cfg: Fetch configuration
 * @danger_lut: real-time QoS LUT for danger setting (not used)
 * @safe_lut: real-time QoS LUT for safe setting (not used)
 * @flags: Control flag
 */
static void sde_hw_rotator_setup_fetchengine(struct sde_hw_rotator_context *ctx,
		enum sde_rot_queue_prio queue_id,
		struct sde_hw_rot_sspp_cfg *cfg, u32 danger_lut, u32 safe_lut,
		u32 flags)
{
	struct sde_hw_rotator *rot = ctx->rot;
	struct sde_mdp_format_params *fmt;
	struct sde_mdp_data *data;
	u32 *wrptr;
	u32 opmode = 0;
	u32 chroma_samp = 0;
	u32 src_format = 0;
	u32 unpack = 0;
	u32 width = cfg->img_width;
	u32 height = cfg->img_height;
	u32 fetch_blocksize = 0;
	int i;

	if (ROT_REGDMA_ON == ctx->rot->mode) {
		SDE_ROTREG_WRITE(rot->mdss_base, REGDMA_CSR_REGDMA_INT_EN,
				REGDMA_INT_MASK);
		SDE_ROTREG_WRITE(rot->mdss_base, REGDMA_CSR_REGDMA_OP_MODE,
				REGDMA_EN);
	}

	wrptr = sde_hw_rotator_get_regdma_segment(ctx);

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
	if (sde_mdp_is_tp10_format(fmt))
		fetch_blocksize = SDE_ROT_SSPP_FETCH_BLOCKSIZE_96;
	else
		fetch_blocksize = SDE_ROT_SSPP_FETCH_BLOCKSIZE_128;
	SDE_REGDMA_WRITE(wrptr, ROT_SSPP_FETCH_CONFIG,
			fetch_blocksize |
			SDE_ROT_SSPP_FETCH_CONFIG_RESET_VALUE |
			((rot->highest_bank & 0x3) << 18));

	/* setup source buffer plane security status */
	if (flags & SDE_ROT_FLAG_SECURE_OVERLAY_SESSION) {
		SDE_REGDMA_WRITE(wrptr, ROT_SSPP_SRC_ADDR_SW_STATUS, 0xF);
		ctx->is_secure = true;
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
	struct sde_mdp_format_params *fmt;
	u32 *wrptr;
	u32 pack = 0;
	u32 dst_format = 0;
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
	if (fmt->bits[C3_ALPHA] || fmt->alpha_enable) {
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

	/*
	 * setup Downscale factor
	 */
	SDE_REGDMA_WRITE(wrptr, ROTTOP_DNSC,
			cfg->v_downscale_factor |
			(cfg->h_downscale_factor << 16));

	/* write config setup for bank configration */
	SDE_REGDMA_WRITE(wrptr, ROT_WB_DST_WRITE_CONFIG,
			(ctx->rot->highest_bank & 0x3) << 8);

	if (flags & SDE_ROT_FLAG_ROT_90)
		SDE_REGDMA_WRITE(wrptr, ROTTOP_OP_MODE, 0x3);
	else
		SDE_REGDMA_WRITE(wrptr, ROTTOP_OP_MODE, 0x1);

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
	u32 *wrptr;
	u32 *rdptr;
	u8 *addr;
	u32 mask;
	u32 blksize;

	rdptr = sde_hw_rotator_get_regdma_segment_base(ctx);
	wrptr = sde_hw_rotator_get_regdma_segment(ctx);

	if (rot->irq_num >= 0) {
		SDE_REGDMA_WRITE(wrptr, ROTTOP_INTR_EN, 1);
		SDE_REGDMA_WRITE(wrptr, ROTTOP_INTR_CLEAR, 1);
		reinit_completion(&ctx->rot_comp);
		enable_irq(rot->irq_num);
	}

	SDE_REGDMA_WRITE(wrptr, ROTTOP_START_CTRL, 1);

	/* Update command queue write ptr */
	sde_hw_rotator_put_regdma_segment(ctx, wrptr);

	SDEROT_DBG("BEGIN %d\n", ctx->timestamp);
	/* Write all command stream to Rotator blocks */
	/* Rotator will start right away after command stream finish writing */
	while (rdptr < wrptr) {
		u32 op = REGDMA_OP_MASK & *rdptr;

		switch (op) {
		case REGDMA_OP_NOP:
			SDEROT_DBG("NOP\n");
			rdptr++;
			break;
		case REGDMA_OP_REGWRITE:
			SDEROT_DBG("REGW %6.6x %8.8x\n",
					rdptr[0] & REGDMA_ADDR_OFFSET_MASK,
					rdptr[1]);
			addr =  rot->mdss_base +
				(*rdptr++ & REGDMA_ADDR_OFFSET_MASK);
			writel_relaxed(*rdptr++, addr);
			break;
		case REGDMA_OP_REGMODIFY:
			SDEROT_DBG("REGM %6.6x %8.8x %8.8x\n",
					rdptr[0] & REGDMA_ADDR_OFFSET_MASK,
					rdptr[1], rdptr[2]);
			addr =  rot->mdss_base +
				(*rdptr++ & REGDMA_ADDR_OFFSET_MASK);
			mask = *rdptr++;
			writel_relaxed((readl_relaxed(addr) & mask) | *rdptr++,
					addr);
			break;
		case REGDMA_OP_BLKWRITE_SINGLE:
			SDEROT_DBG("BLKWS %6.6x %6.6x\n",
					rdptr[0] & REGDMA_ADDR_OFFSET_MASK,
					rdptr[1]);
			addr =  rot->mdss_base +
				(*rdptr++ & REGDMA_ADDR_OFFSET_MASK);
			blksize = *rdptr++;
			while (blksize--) {
				SDEROT_DBG("DATA %8.8x\n", rdptr[0]);
				writel_relaxed(*rdptr++, addr);
			}
			break;
		case REGDMA_OP_BLKWRITE_INC:
			SDEROT_DBG("BLKWI %6.6x %6.6x\n",
					rdptr[0] & REGDMA_ADDR_OFFSET_MASK,
					rdptr[1]);
			addr =  rot->mdss_base +
				(*rdptr++ & REGDMA_ADDR_OFFSET_MASK);
			blksize = *rdptr++;
			while (blksize--) {
				SDEROT_DBG("DATA %8.8x\n", rdptr[0]);
				writel_relaxed(*rdptr++, addr);
				addr += 4;
			}
			break;
		default:
			/* Other not supported OP mode
			 * Skip data for now for unregonized OP mode
			 */
			SDEROT_DBG("UNDEFINED\n");
			rdptr++;
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
	struct sde_hw_rotator *rot = ctx->rot;
	u32 *wrptr;
	u32  regdmaSlot;
	u32  offset;
	long length;
	long ts_length;
	u32  enableInt;
	u32  swts = 0;
	u32  mask = 0;

	wrptr = sde_hw_rotator_get_regdma_segment(ctx);

	if (rot->irq_num >= 0)
		reinit_completion(&ctx->regdma_comp);

	/* enable IRQ for first regdma submission from idle */
	if (atomic_read(&rot->regdma_submit_count) ==
				atomic_read(&rot->regdma_done_count)) {
		SDEROT_DBG("Enable IRQ! regdma submitcnt==donecnt -> %d\n",
				atomic_read(&rot->regdma_submit_count));
		enable_irq(rot->irq_num);
	}

	/*
	 * Last ROT command must be ROT_START before REGDMA start
	 */
	SDE_REGDMA_WRITE(wrptr, ROTTOP_START_CTRL, 1);
	sde_hw_rotator_put_regdma_segment(ctx, wrptr);

	/*
	 * Start REGDMA with command offset and size
	*/
	regdmaSlot = sde_hw_rotator_get_regdma_ctxidx(ctx);
	length = ((long)wrptr - (long)ctx->regdma_base) / 4;
	offset = (u32)(ctx->regdma_base - (u32 *)(rot->mdss_base +
				REGDMA_RAM_REGDMA_CMD_RAM));
	enableInt = ((ctx->timestamp & 1) + 1) << 30;

	SDEROT_DBG(
		"regdma(%d)[%d] <== INT:0x%X|length:%ld|offset:0x%X, ts:%X\n",
		queue_id, regdmaSlot, enableInt, length, offset,
		ctx->timestamp);

	/* ensure the command packet is issued before the submit command */
	wmb();

	/* REGDMA submission for current context */
	if (ROT_QUEUE_HIGH_PRIORITY == queue_id) {
		SDE_ROTREG_WRITE(rot->mdss_base,
				REGDMA_CSR_REGDMA_QUEUE_0_SUBMIT,
				(length << 14) | offset);
		swts = ctx->timestamp;
		mask = ~SDE_REGDMA_SWTS_MASK;
	} else {
		SDE_ROTREG_WRITE(rot->mdss_base,
				REGDMA_CSR_REGDMA_QUEUE_1_SUBMIT,
				(length << 14) | offset);
		swts = ctx->timestamp << SDE_REGDMA_SWTS_SHIFT;
		mask = ~(SDE_REGDMA_SWTS_MASK << SDE_REGDMA_SWTS_SHIFT);
	}

	/* Write timestamp after previous rotator job finished */
	sde_hw_rotator_setup_timestamp_packet(ctx, mask, swts);
	offset += length;
	ts_length = sde_hw_rotator_get_regdma_segment(ctx) - wrptr;
	BUG_ON((length + ts_length) > SDE_HW_ROT_REGDMA_SEG_SIZE);

	/* ensure command packet is issue before the submit command */
	wmb();

	if (ROT_QUEUE_HIGH_PRIORITY == queue_id) {
		SDE_ROTREG_WRITE(rot->mdss_base,
				REGDMA_CSR_REGDMA_QUEUE_0_SUBMIT,
				enableInt | (ts_length << 14) | offset);
	} else {
		SDE_ROTREG_WRITE(rot->mdss_base,
				REGDMA_CSR_REGDMA_QUEUE_1_SUBMIT,
				enableInt | (ts_length << 14) | offset);
	}

	/* Update REGDMA submit count */
	atomic_inc(&rot->regdma_submit_count);

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
					KOFF_TIMEOUT);

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

			disable_irq_nosync(rot->irq_num);
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
	u32 status;
	u32 last_isr;
	u32 last_ts;
	u32 int_id;
	u32 sts = 0;
	u32 d_count;
	unsigned long flags;

	if (rot->irq_num >= 0) {
		SDEROT_DBG("Wait for REGDMA completion, ctx:%p, ts:%X\n",
				ctx, ctx->timestamp);
		rc = wait_for_completion_timeout(&ctx->regdma_comp,
				KOFF_TIMEOUT);

		spin_lock_irqsave(&rot->rotisr_lock, flags);

		last_isr = ctx->last_regdma_isr_status;
		last_ts  = ctx->last_regdma_timestamp;
		status   = last_isr & REGDMA_INT_MASK;
		int_id   = last_ts & 1;
		SDEROT_DBG("INT status:0x%X, INT id:%d, timestamp:0x%X\n",
				status, int_id, last_ts);

		if (rc == 0 || (status & REGDMA_INT_ERR_MASK)) {
			SDEROT_ERR(
				"Timeout wait for regdma interrupt status, ts:%X\n",
				ctx->timestamp);

			if (status & REGDMA_WATCHDOG_INT)
				SDEROT_ERR("REGDMA watchdog interrupt\n");
			else if (status & REGDMA_INVALID_DESCRIPTOR)
				SDEROT_ERR("REGDMA invalid descriptor\n");
			else if (status & REGDMA_INCOMPLETE_CMD)
				SDEROT_ERR("REGDMA incomplete command\n");
			else if (status & REGDMA_INVALID_CMD)
				SDEROT_ERR("REGDMA invalid command\n");

			status = ROT_ERROR_BIT;
		} else if (queue_id == ROT_QUEUE_HIGH_PRIORITY) {
			/* Got to match exactly with interrupt ID */
			int_id = REGDMA_QUEUE0_INT0 << int_id;

			SDE_ROTREG_WRITE(rot->mdss_base,
					REGDMA_CSR_REGDMA_INT_CLEAR,
					int_id);

			status = 0;
		} else if (queue_id == ROT_QUEUE_LOW_PRIORITY) {
			/* Matching interrupt ID */
			int_id = REGDMA_QUEUE1_INT0 << int_id;

			SDE_ROTREG_WRITE(rot->mdss_base,
					REGDMA_CSR_REGDMA_INT_CLEAR,
					int_id);

			status = 0;
		}

		/* regardless success or timeout, update done count */
		d_count = atomic_inc_return(&rot->regdma_done_count);

		/* disable IRQ if no more regdma submission in queue */
		if (d_count == atomic_read(&rot->regdma_submit_count)) {
			SDEROT_DBG(
				"Disable IRQ!! regdma donecnt==submitcnt -> %d\n",
				d_count);
			disable_irq_nosync(rot->irq_num);
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

		SDE_ROTREG_WRITE(rot->mdss_base, REGDMA_CSR_REGDMA_INT_CLEAR,
				0xFFFF);
	}

	sts = (status & ROT_ERROR_BIT) ? -ENODEV : 0;

	return sts;
}

/*
 * setup_rotator_ops - setup callback functions for the low-level HAL
 * @ops: Pointer to low-level ops callback
 * @mode: Operation mode (non-regdma or regdma)
 */
static void setup_rotator_ops(struct sde_hw_rotator_ops *ops,
		enum sde_rotator_regdma_mode mode)
{
	ops->setup_rotator_fetchengine = sde_hw_rotator_setup_fetchengine;
	ops->setup_rotator_wbengine = sde_hw_rotator_setup_wbengine;
	if (ROT_REGDMA_ON == mode) {
		ops->start_rotator = sde_hw_rotator_start_regdma;
		ops->wait_rotator_done = sde_hw_rotator_wait_done_regdma;
	} else {
		ops->start_rotator = sde_hw_rotator_start_no_regdma;
		ops->wait_rotator_done = sde_hw_rotator_wait_done_no_regdma;
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
	struct ion_handle *handle;
	struct sde_mdp_img_data *data;
	u32 bufsize = sizeof(int) * SDE_HW_ROT_REGDMA_TOTAL_CTX * 2;

	rot->iclient = msm_ion_client_create(rot->pdev->name);
	if (IS_ERR_OR_NULL(rot->iclient)) {
		SDEROT_ERR("msm_ion_client_create() return error (%p)\n",
				rot->iclient);
		return -EINVAL;
	}

	handle = ion_alloc(rot->iclient, bufsize, SZ_4K,
			ION_HEAP(ION_SYSTEM_HEAP_ID), 0);
	if (IS_ERR_OR_NULL(handle)) {
		SDEROT_ERR("ion memory allocation failed\n");
		return -ENOMEM;
	}

	data = &rot->swts_buf;
	data->len = bufsize;
	data->srcp_dma_buf = ion_share_dma_buf(rot->iclient, handle);
	if (IS_ERR(data->srcp_dma_buf)) {
		SDEROT_ERR("ion_dma_buf setup failed\n");
		rc = -ENOMEM;
		goto imap_err;
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
	if (IS_ERR_VALUE(rc)) {
		SDEROT_ERR("smmu_map_dma_buf failed: (%d)\n", rc);
		goto err_unmap;
	}

	dma_buf_begin_cpu_access(data->srcp_dma_buf, 0, data->len,
			DMA_FROM_DEVICE);
	rot->swts_buffer = dma_buf_kmap(data->srcp_dma_buf, 0);
	if (IS_ERR_OR_NULL(rot->swts_buffer)) {
		SDEROT_ERR("ion kernel memory mapping failed\n");
		rc = IS_ERR(rot->swts_buffer);
		goto kmap_err;
	}

	data->mapped = true;
	SDEROT_DBG("swts buffer mapped: %pad/%lx va:%p\n", &data->addr,
			data->len, rot->swts_buffer);

	ion_free(rot->iclient, handle);

	sde_smmu_ctrl(0);

	return rc;
kmap_err:
	sde_smmu_unmap_dma_buf(data->srcp_table, SDE_IOMMU_DOMAIN_ROT_UNSECURE,
			DMA_FROM_DEVICE, data->srcp_dma_buf);
err_unmap:
	dma_buf_unmap_attachment(data->srcp_attachment, data->srcp_table,
			DMA_FROM_DEVICE);
err_detach:
	dma_buf_detach(data->srcp_dma_buf, data->srcp_attachment);
err_put:
	dma_buf_put(data->srcp_dma_buf);
	data->srcp_dma_buf = NULL;
imap_err:
	ion_free(rot->iclient, handle);

	return rc;
}

/*
 * sde_hw_rotator_swtc_destroy - destroy software timestamp buffer
 * @rot: Pointer to rotator hw
 */
static void sde_hw_rotator_swtc_destroy(struct sde_hw_rotator *rot)
{
	struct sde_mdp_img_data *data;

	data = &rot->swts_buf;

	dma_buf_end_cpu_access(data->srcp_dma_buf, 0, data->len,
			DMA_FROM_DEVICE);
	dma_buf_kunmap(data->srcp_dma_buf, 0, rot->swts_buffer);

	sde_smmu_unmap_dma_buf(data->srcp_table, SDE_IOMMU_DOMAIN_ROT_UNSECURE,
			DMA_FROM_DEVICE, data->srcp_dma_buf);
	dma_buf_unmap_attachment(data->srcp_attachment, data->srcp_table,
			DMA_FROM_DEVICE);
	dma_buf_detach(data->srcp_dma_buf, data->srcp_attachment);
	dma_buf_put(data->srcp_dma_buf);
	data->srcp_dma_buf = NULL;
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

	if (ROT_REGDMA_ON == rot->mode)
		sde_hw_rotator_swtc_destroy(rot);

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
	if (ROT_REGDMA_OFF == resinfo->rot->mode)
		resinfo->hw.max_active = 1;
	else {
		resinfo->hw.max_active = SDE_HW_ROT_REGDMA_TOTAL_CTX - 1;

		if (resinfo->rot->iclient == NULL)
			sde_hw_rotator_swts_create(resinfo->rot);
	}

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

	devm_kfree(&mgr->pdev->dev, resinfo);
}

/*
 * sde_hw_rotator_alloc_rotctx - allocate rotator context
 * @rot: Pointer to rotator hw
 * @hw: Pointer to rotator resource
 * @session_id: Session identifier of this context
 *
 * This function allocates a new rotator context for the given session id.
 */
static struct sde_hw_rotator_context *sde_hw_rotator_alloc_rotctx(
		struct sde_hw_rotator *rot,
		struct sde_rot_hw_resource *hw,
		u32    session_id)
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
	ctx->hwres      = hw;
	ctx->timestamp  = atomic_add_return(1, &rot->timestamp[ctx->q_id]);
	ctx->timestamp &= SDE_REGDMA_SWTS_MASK;
	ctx->is_secure  = false;

	ctx->regdma_base  = rot->cmd_wr_ptr[ctx->q_id]
		[sde_hw_rotator_get_regdma_ctxidx(ctx)];
	ctx->regdma_wrptr = ctx->regdma_base;
	ctx->ts_addr      = (dma_addr_t)((u32 *)rot->swts_buf.addr +
		ctx->q_id * SDE_HW_ROT_REGDMA_TOTAL_CTX +
		sde_hw_rotator_get_regdma_ctxidx(ctx));

	init_completion(&ctx->rot_comp);
	init_completion(&ctx->regdma_comp);

	/* Store rotator context for lookup purpose */
	sde_hw_rotator_put_ctx(ctx);

	SDEROT_DBG(
		"New rot CTX:%p, ctxidx:%d, session-id:%d, prio:%d, timestamp:%X, active:%d\n",
		ctx, sde_hw_rotator_get_regdma_ctxidx(ctx), ctx->session_id,
		ctx->q_id, ctx->timestamp,
		atomic_read(&ctx->hwres->num_active));

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
		"Free rot CTX:%p, ctxidx:%d, session-id:%d, prio:%d, timestamp:%X, active:%d\n",
		ctx, sde_hw_rotator_get_regdma_ctxidx(ctx), ctx->session_id,
		ctx->q_id, ctx->timestamp,
		atomic_read(&ctx->hwres->num_active));

	rot->rotCtx[ctx->q_id][sde_hw_rotator_get_regdma_ctxidx(ctx)] = NULL;

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
	struct sde_rotation_item *item;

	if (!hw || !entry) {
		SDEROT_ERR("null hw resource/entry\n");
		return -EINVAL;
	}

	resinfo = container_of(hw, struct sde_hw_rotator_resource_info, hw);
	rot = resinfo->rot;
	item = &entry->item;

	ctx = sde_hw_rotator_alloc_rotctx(rot, hw, item->session_id);
	if (!ctx) {
		SDEROT_ERR("Failed allocating rotator context!!\n");
		return -EINVAL;
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

	sspp_cfg.img_width = item->input.width;
	sspp_cfg.img_height = item->input.height;
	sspp_cfg.fmt = sde_get_format_params(item->input.format);
	if (!sspp_cfg.fmt) {
		SDEROT_ERR("null format\n");
		return -EINVAL;
	}
	sspp_cfg.src_rect = &item->src_rect;
	sspp_cfg.data = &entry->src_buf;
	sde_mdp_get_plane_sizes(sspp_cfg.fmt, item->input.width,
			item->input.height, &sspp_cfg.src_plane,
			0, /* No bwc_mode */
			(flags & SDE_ROT_FLAG_SOURCE_ROTATED_90) ?
					true : false);

	rot->ops.setup_rotator_fetchengine(ctx, ctx->q_id,
			&sspp_cfg, danger_lut, safe_lut, flags);

	wb_cfg.img_width = item->output.width;
	wb_cfg.img_height = item->output.height;
	wb_cfg.fmt = sde_get_format_params(item->output.format);
	wb_cfg.dst_rect = &item->dst_rect;
	wb_cfg.data = &entry->dst_buf;
	sde_mdp_get_plane_sizes(wb_cfg.fmt, item->output.width,
			item->output.height, &wb_cfg.dst_plane,
			0, /* No bwc_mode */
			(flags & SDE_ROT_FLAG_ROT_90) ? true : false);

	wb_cfg.v_downscale_factor = entry->dnsc_factor_h;
	wb_cfg.h_downscale_factor = entry->dnsc_factor_w;

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

	if (mdata->default_ot_rd_limit) {
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
		ot_params.fmt = entry->perf->config.input.format;
		sde_mdp_set_ot_limit(&ot_params);
	}

	if (mdata->default_ot_wr_limit) {
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
		ot_params.fmt = entry->perf->config.input.format;
		sde_mdp_set_ot_limit(&ot_params);
	}

	if (test_bit(SDE_QOS_PER_PIPE_LUT, mdata->sde_qos_map))	{
		u32 qos_lut = 0; /* low priority for nrt read client */

		trace_rot_perf_set_qos_luts(XIN_SSPP, sspp_cfg.fmt->format,
			qos_lut, sde_mdp_is_linear_format(sspp_cfg.fmt));

		SDE_ROTREG_WRITE(rot->mdss_base, ROT_SSPP_CREQ_LUT, qos_lut);
	}

	if (mdata->npriority_lvl > 0) {
		u32 mask, reg_val, i, vbif_qos;

		for (i = 0; i < mdata->npriority_lvl; i++) {
			reg_val = SDE_VBIF_READ(mdata,
					MMSS_VBIF_NRT_VBIF_QOS_REMAP_00 + i*4);
			mask = 0x3 << (XIN_SSPP * 2);
			reg_val &= ~(mask);
			vbif_qos = mdata->vbif_nrt_qos[i];
			reg_val |= vbif_qos << (XIN_SSPP * 2);
			/* ensure write is issued after the read operation */
			mb();
			SDE_VBIF_WRITE(mdata,
					MMSS_VBIF_NRT_VBIF_QOS_REMAP_00 + i*4,
					reg_val);
		}
	}

	/* Enable write gather for writeback to remove write gaps, which
	 * may hang AXI/BIMC/SDE.
	 */
	SDE_VBIF_WRITE(mdata, MMSS_VBIF_NRT_VBIF_WRITE_GATHTER_EN,
			BIT(XIN_WRITEBACK));

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
	int ret = 0;

	if (!hw || !entry) {
		SDEROT_ERR("null hw resource/entry\n");
		return -EINVAL;
	}

	resinfo = container_of(hw, struct sde_hw_rotator_resource_info, hw);
	rot = resinfo->rot;

	/* Lookup rotator context from session-id */
	ctx = sde_hw_rotator_get_ctx(rot, entry->item.session_id, hw->wb_id);
	if (!ctx) {
		SDEROT_ERR("Cannot locate rotator ctx from sesison id:%d\n",
				entry->item.session_id);
	}
	BUG_ON(ctx == NULL);

	ret = sde_smmu_ctrl(1);
	if (IS_ERR_VALUE(ret)) {
		SDEROT_ERR("IOMMU attach failed\n");
		return ret;
	}

	rot->ops.start_rotator(ctx, ctx->q_id);

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
	ctx = sde_hw_rotator_get_ctx(rot, entry->item.session_id, hw->wb_id);
	if (!ctx) {
		SDEROT_ERR("Cannot locate rotator ctx from sesison id:%d\n",
				entry->item.session_id);
	}
	BUG_ON(ctx == NULL);

	ret = rot->ops.wait_rotator_done(ctx, ctx->q_id, 0);

	sde_smmu_ctrl(0);

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
	clear_bit(SDE_QOS_CDP, mdata->sde_qos_map);
	set_bit(SDE_QOS_OTLIM, mdata->sde_qos_map);
	set_bit(SDE_QOS_PER_PIPE_LUT, mdata->sde_qos_map);
	clear_bit(SDE_QOS_SIMPLIFIED_PREFILL, mdata->sde_qos_map);

	set_bit(SDE_CAPS_R3_WB, mdata->sde_caps_map);

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
			disable_irq_nosync(rot->irq_num);
		SDEROT_DBG("Notify rotator complete\n");

		/* Normal rotator only 1 session, no need to lookup */
		ctx = rot->rotCtx[0][0];
		BUG_ON(ctx == NULL);
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
	struct sde_hw_rotator *rot = ptr;
	struct sde_hw_rotator_context *ctx;
	irqreturn_t ret = IRQ_NONE;
	u32 isr;
	u32 ts;
	u32 q_id;

	isr = SDE_ROTREG_READ(rot->mdss_base, REGDMA_CSR_REGDMA_INT_STATUS);
	ts  = SDE_ROTREG_READ(rot->mdss_base, REGDMA_TIMESTAMP_REG);

	SDEROT_DBG("intr_status = %8.8x, sw_TS:%X\n", isr, ts);

	/* Any REGDMA status, including error and watchdog timer, should
	 * trigger and wake up waiting thread */
	if (isr & (REGDMA_INT_HIGH_MASK | REGDMA_INT_LOW_MASK)) {
		spin_lock(&rot->rotisr_lock);

		/*
		 * Obtain rotator context based on timestamp from regdma
		 * and low/high interrupt status
		 */
		if (isr & REGDMA_INT_HIGH_MASK) {
			q_id = ROT_QUEUE_HIGH_PRIORITY;
			ts   = ts & SDE_REGDMA_SWTS_MASK;
		} else if (isr & REGDMA_INT_LOW_MASK) {
			q_id = ROT_QUEUE_LOW_PRIORITY;
			ts   = (ts >> SDE_REGDMA_SWTS_SHIFT) &
				SDE_REGDMA_SWTS_MASK;
		}

		ctx = rot->rotCtx[q_id][ts & SDE_HW_ROT_REGDMA_SEG_MASK];
		BUG_ON(ctx == NULL);

		/*
		 * Wake up all waiting context from the current and previous
		 * SW Timestamp.
		 */
		do {
			ctx->last_regdma_isr_status = isr;
			ctx->last_regdma_timestamp  = ts;
			SDEROT_DBG(
				"regdma complete: ctx:%p, ts:%X, dcount:%X\n",
				ctx, ts, atomic_read(&rot->regdma_done_count));
			complete_all(&ctx->regdma_comp);

			ts  = (ts - 1) & SDE_REGDMA_SWTS_MASK;
			ctx = rot->rotCtx[q_id]
				[ts & SDE_HW_ROT_REGDMA_SEG_MASK];
		} while (ctx && (ctx->last_regdma_timestamp == 0));

		/*
		 * Clear corresponding regdma interrupt because it is a level
		 * interrupt
		 */
		SDE_ROTREG_WRITE(rot->mdss_base, REGDMA_CSR_REGDMA_INT_CLEAR,
				isr);

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
					ctx->last_regdma_isr_status = isr;
					ctx->last_regdma_timestamp  = ts;
					complete_all(&ctx->regdma_comp);
					SDEROT_DBG("Wakeup rotctx[%d][%d]:%p\n",
							i, j, ctx);
				}
			}
		}

		SDE_ROTREG_WRITE(rot->mdss_base, REGDMA_CSR_REGDMA_INT_CLEAR,
				isr);

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
	int ret = 0;
	u16 src_w, src_h, dst_w, dst_h;
	struct sde_rotation_item *item = &entry->item;
	struct sde_mdp_format_params *fmt;

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

	if ((src_w != dst_w) || (src_h != dst_h)) {
		if ((src_w % dst_w) || (src_h % dst_h)) {
			SDEROT_DBG("non integral scale not support\n");
			ret = -EINVAL;
			goto dnsc_err;
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
	/* Tiled format downscale support not applied to AYUV tiled */
	if (sde_mdp_is_tilea5x_format(fmt) && (entry->dnsc_factor_h > 4)) {
		SDEROT_DBG("max downscale for tiled format is 4\n");
		ret = -EINVAL;
		goto dnsc_err;
	}
	if (sde_mdp_is_ubwc_format(fmt)	&& (entry->dnsc_factor_h > 2)) {
		SDEROT_DBG("downscale with ubwc cannot be more than 2\n");
		ret = -EINVAL;
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
	int cnt = 0;

	if (!mgr || !buf)
		return 0;

	hw_data = mgr->hw_data;

#define SPRINT(fmt, ...) \
		(cnt += scnprintf(buf + cnt, len - cnt, fmt, ##__VA_ARGS__))

	/* insert capabilities here */

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

	/* Assign ops */
	mgr->ops_hw_destroy = sde_hw_rotator_destroy;
	mgr->ops_hw_alloc = sde_hw_rotator_alloc_ext;
	mgr->ops_hw_free = sde_hw_rotator_free_ext;
	mgr->ops_config_hw = sde_hw_rotator_config;
	mgr->ops_kickoff_entry = sde_hw_rotator_kickoff;
	mgr->ops_wait_for_entry = sde_hw_rotator_wait4done;
	mgr->ops_hw_validate_entry = sde_hw_rotator_validate_entry;
	mgr->ops_hw_show_caps = sde_hw_rotator_show_caps;
	mgr->ops_hw_show_state = sde_hw_rotator_show_state;
	mgr->ops_hw_create_debugfs = sde_rotator_r3_create_debugfs;

	ret = sde_hw_rotator_parse_dt(mgr->hw_data, mgr->pdev);
	if (ret)
		goto error_parse_dt;

	rot->irq_num = platform_get_irq(mgr->pdev, 0);
	if (rot->irq_num < 0) {
		SDEROT_ERR("fail to get rotator irq\n");
	} else {
		if (ROT_REGDMA_OFF == rot->mode)
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

	setup_rotator_ops(&rot->ops, rot->mode);

	spin_lock_init(&rot->rotctx_lock);
	spin_lock_init(&rot->rotisr_lock);

	/* REGDMA initialization */
	if (ROT_REGDMA_OFF == rot->mode) {
		for (i = 0; i < SDE_HW_ROT_REGDMA_TOTAL_CTX; i++)
			rot->cmd_wr_ptr[0][i] = &rot->cmd_queue[
				SDE_HW_ROT_REGDMA_SEG_SIZE * i];
	} else {
		for (i = 0; i < SDE_HW_ROT_REGDMA_TOTAL_CTX; i++)
			rot->cmd_wr_ptr[ROT_QUEUE_HIGH_PRIORITY][i] =
				(u32 *)(rot->mdss_base +
					REGDMA_RAM_REGDMA_CMD_RAM +
					SDE_HW_ROT_REGDMA_SEG_SIZE * 4 * i);

		for (i = 0; i < SDE_HW_ROT_REGDMA_TOTAL_CTX; i++)
			rot->cmd_wr_ptr[ROT_QUEUE_LOW_PRIORITY][i] =
				(u32 *)(rot->mdss_base +
					REGDMA_RAM_REGDMA_CMD_RAM +
					SDE_HW_ROT_REGDMA_SEG_SIZE * 4 *
					(i + SDE_HW_ROT_REGDMA_TOTAL_CTX));
	}

	atomic_set(&rot->timestamp[0], 0);
	atomic_set(&rot->timestamp[1], 0);
	atomic_set(&rot->regdma_submit_count, 0);
	atomic_set(&rot->regdma_done_count, 0);

	ret = sde_rotator_hw_rev_init(rot);
	if (ret)
		goto error_hw_rev_init;

	return 0;
error_hw_rev_init:
	if (rot->irq_num >= 0)
		devm_free_irq(&mgr->pdev->dev, rot->irq_num, mdata);
	devm_kfree(&mgr->pdev->dev, mgr->hw_data);
error_parse_dt:
	return ret;
}
