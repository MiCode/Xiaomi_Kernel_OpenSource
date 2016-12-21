/* Copyright (c) 2015-2017, The Linux Foundation. All rights reserved.
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

#ifndef _SDE_ROTATOR_R3_INTERNAL_H
#define _SDE_ROTATOR_R3_INTERNAL_H

#include "sde_rotator_core.h"

struct sde_hw_rotator;
struct sde_hw_rotator_context;

/**
 * Flags
 */
#define SDE_ROT_FLAG_SECURE_OVERLAY_SESSION 0x1
#define SDE_ROT_FLAG_FLIP_LR                0x2
#define SDE_ROT_FLAG_FLIP_UD                0x4
#define SDE_ROT_FLAG_SOURCE_ROTATED_90      0x8
#define SDE_ROT_FLAG_ROT_90                 0x10
#define SDE_ROT_FLAG_DEINTERLACE            0x20
#define SDE_ROT_FLAG_SECURE_CAMERA_SESSION  0x40

/**
 * General defines
 */
#define SDE_HW_ROT_REGDMA_RAM_SIZE      1024
#define SDE_HW_ROT_REGDMA_TOTAL_CTX     8
#define SDE_HW_ROT_REGDMA_SEG_MASK      (SDE_HW_ROT_REGDMA_TOTAL_CTX - 1)
#define SDE_HW_ROT_REGDMA_SEG_SIZE \
	(SDE_HW_ROT_REGDMA_RAM_SIZE / SDE_HW_ROT_REGDMA_TOTAL_CTX)
#define SDE_REGDMA_SWTS_MASK            0x00000FFF
#define SDE_REGDMA_SWTS_SHIFT           12

enum sde_rot_queue_prio {
	ROT_QUEUE_HIGH_PRIORITY,
	ROT_QUEUE_LOW_PRIORITY,
	ROT_QUEUE_MAX
};

enum sde_rot_angle {
	ROT_ANGLE_0,
	ROT_ANGLE_90,
	ROT_ANGEL_MAX
};

enum sde_rotator_regdma_mode {
	ROT_REGDMA_OFF,
	ROT_REGDMA_ON,
	ROT_REGDMA_MAX
};

/**
 * struct sde_hw_rot_sspp_cfg: Rotator SSPP Configration description
 * @src:       source surface information
 * @src_rect:  src ROI, caller takes into account the different operations
 *             such as decimation, flip etc to program this field
 * @addr:      source surface address
 */
struct sde_hw_rot_sspp_cfg {
	struct sde_mdp_format_params *fmt;
	struct sde_mdp_plane_sizes    src_plane;
	struct sde_rect              *src_rect;
	struct sde_mdp_data          *data;
	u32                           img_width;
	u32                           img_height;
	u32                           fps;
	u64                           bw;
};



/**
 *  struct sde_hw_rot_wb_cfg: Rotator WB Configration description
 *  @dest:      destination surface information
 *  @dest_rect: dest ROI, caller takes into account the different operations
 *              such as decimation, flip etc to program this field
 *  @addr:      destination surface address
 */
struct sde_hw_rot_wb_cfg {
	struct sde_mdp_format_params   *fmt;
	struct sde_mdp_plane_sizes      dst_plane;
	struct sde_rect                *dst_rect;
	struct sde_mdp_data            *data;
	u32                             img_width;
	u32                             img_height;
	u32                             v_downscale_factor;
	u32                             h_downscale_factor;
	u32                             fps;
	u64                             bw;
};



/**
 *
 * struct sde_hw_rotator_ops: Interface to the Rotator Hw driver functions
 *
 * Pre-requsises:
 *  - Caller must call the init function to get the rotator context
 *  - These functions will be called after clocks are enabled
 */
struct sde_hw_rotator_ops {
	/**
	 * setup_rotator_fetchengine():
	 *    Setup Source format
	 *    Setup Source dimension/cropping rectangle (ROI)
	 *    Setup Source surface base address and stride
	 *    Setup fetch engine op mode (linear/tiled/compression/...)
	 * @ctx:        Rotator context created in sde_hw_rotator_config
	 * @queue_id:   Select either low / high priority queue
	 * @cfg:        Rotator Fetch engine configuration parameters
	 * @danger_lut: Danger LUT setting
	 * @safe_lut:   Safe LUT setting
	 * @dnsc_factor_w: Downscale factor for width
	 * @dnsc_factor_h: Downscale factor for height
	 * @flags:      Specific config flag, see SDE_ROT_FLAG_ for details
	 */
	void (*setup_rotator_fetchengine)(
			struct sde_hw_rotator_context  *ctx,
			enum   sde_rot_queue_prio       queue_id,
			struct sde_hw_rot_sspp_cfg     *cfg,
			u32                             danger_lut,
			u32                             safe_lut,
			u32                             dnsc_factor_w,
			u32                             dnsc_factor_h,
			u32                             flags);

	/**
	 * setup_rotator_wbengine():
	 *     Setup destination formats
	 *     Setup destination dimension/cropping rectangle (ROI)
	 *     Setup destination surface base address and strides
	 *     Setup writeback engine op mode (linear/tiled/compression)
	 * @ctx:        Rotator context created in sde_hw_rotator_config
	 * @queue_id:   Select either low / high priority queue
	 * @cfg:        Rotator WriteBack engine configuration parameters
	 * @flags:      Specific config flag, see SDE_ROT_FLAG_ for details
	 */
	void (*setup_rotator_wbengine)(
			struct sde_hw_rotator_context *ctx,
			enum   sde_rot_queue_prio      queue_id,
			struct sde_hw_rot_wb_cfg      *cfg,
			u32                            flags);

	/**
	 * start_rotator():
	 *     Kick start rotator operation based on cached setup parameters
	 *     REGDMA commands will get generated at this points
	 * @ctx:      Rotator context
	 * @queue_id: Select either low / high priority queue
	 * Returns:   unique job timestamp per submit. Used for tracking
	 *            rotator finished job.
	 */
	u32 (*start_rotator)(
			struct sde_hw_rotator_context  *ctx,
			enum   sde_rot_queue_prio       queue_id);

	/**
	 * wait_rotator_done():
	 *     Notify Rotator HAL layer previously submitted job finished.
	 *     A job timestamp will return to caller.
	 * @ctx:    Rotator context
	 * @flags:  Reserved
	 * Returns: job timestamp for tracking purpose
	 *
	 */
	u32 (*wait_rotator_done)(
			struct sde_hw_rotator_context  *ctx,
			enum   sde_rot_queue_prio       queue_id,
			u32                             flags);
};

/**
 * struct sde_dbg_buf : Debug buffer used by debugfs
 * @vaddr:        VA address mapped from dma buffer
 * @dmabuf:       DMA buffer
 * @buflen:       Length of DMA buffer
 * @width:        pixel width of buffer
 * @height:       pixel height of buffer
 */
struct sde_dbg_buf {
	void *vaddr;
	struct dma_buf *dmabuf;
	unsigned long buflen;
	u32 width;
	u32 height;
};

/**
 * struct sde_hw_rotator_context : Each rotator context ties to each priority
 * queue. Max number of concurrent contexts in regdma is limited to regdma
 * ram segment size allocation. Each rotator context can be any priority. A
 * incrementatl timestamp is used to identify and assigne to each context.
 */
struct sde_hw_rotator_context {
	struct sde_hw_rotator *rot;
	struct sde_rot_hw_resource *hwres;
	enum   sde_rot_queue_prio q_id;
	u32    session_id;
	u32    *regdma_base;
	u32    *regdma_wrptr;
	u32    timestamp;
	struct completion rot_comp;
	wait_queue_head_t regdma_waitq;
	struct sde_dbg_buf src_dbgbuf;
	struct sde_dbg_buf dst_dbgbuf;
	u32    last_regdma_isr_status;
	u32    last_regdma_timestamp;
	dma_addr_t ts_addr;
	bool   is_secure;
	bool   is_traffic_shaping;
};

/**
 * struct sde_hw_rotator_resource_info : Each rotator resource ties to each
 * priority queue
 */
struct sde_hw_rotator_resource_info {
	struct sde_hw_rotator      *rot;
	struct sde_rot_hw_resource  hw;
};

/**
 * struct sde_hw_rotator : Rotator description
 * @hw:           mdp register mapped offset
 * @ops:          pointer to operations possible for the rotator HW
 */
struct sde_hw_rotator {
	/* base */
	char __iomem *mdss_base;

	/* Platform device from upper manager */
	struct platform_device *pdev;

	/* Ops */
	struct sde_hw_rotator_ops ops;

	/* Cmd Queue */
	u32    cmd_queue[SDE_HW_ROT_REGDMA_RAM_SIZE];

	/* Cmd Queue Write Ptr */
	u32   *cmd_wr_ptr[ROT_QUEUE_MAX][SDE_HW_ROT_REGDMA_TOTAL_CTX];

	/* Rotator Context */
	struct sde_hw_rotator_context
		*rotCtx[ROT_QUEUE_MAX][SDE_HW_ROT_REGDMA_TOTAL_CTX];

	/* Cmd timestamp sequence for different priority*/
	atomic_t timestamp[ROT_QUEUE_MAX];

	/* regdma mode */
	enum   sde_rotator_regdma_mode mode;

	/* logical interrupt number */
	int    irq_num;
	atomic_t irq_enabled;

	/* internal ION memory for SW timestamp */
	struct ion_client *iclient;
	struct sde_mdp_img_data swts_buf;
	void *swts_buffer;

	u32    highest_bank;

	spinlock_t rotctx_lock;
	spinlock_t rotisr_lock;

	bool    dbgmem;
	bool reset_hw_ts;
	u32 last_hw_ts;
};

/**
 * sde_hw_rotator_get_regdma_ctxidx(): regdma segment index is based on
 * timestamp. For non-regdma, just return 0 (i.e. first index)
 * @ctx: Rotator Context
 * return: regdma segment index
 */
static inline u32 sde_hw_rotator_get_regdma_ctxidx(
		struct sde_hw_rotator_context *ctx)
{
	if (ctx->rot->mode == ROT_REGDMA_OFF)
		return 0;
	else
		return ctx->timestamp & SDE_HW_ROT_REGDMA_SEG_MASK;
}

/**
 * sde_hw_rotator_get_regdma_segment_base: return the base pointe of current
 * regdma command buffer
 * @ctx: Rotator Context
 * return: base segment address
 */
static inline u32 *sde_hw_rotator_get_regdma_segment_base(
		struct sde_hw_rotator_context *ctx)
{
	SDEROT_DBG("regdma base @slot[%d]: %p\n",
			sde_hw_rotator_get_regdma_ctxidx(ctx),
			ctx->regdma_base);

	return ctx->regdma_base;
}

/**
 * sde_hw_rotator_get_regdma_segment(): return current regdma command buffer
 * pointer for current regdma segment.
 * @ctx: Rotator Context
 * return: segment address
 */
static inline u32 *sde_hw_rotator_get_regdma_segment(
		struct sde_hw_rotator_context *ctx)
{
	u32 idx = sde_hw_rotator_get_regdma_ctxidx(ctx);
	u32 *addr = ctx->regdma_wrptr;

	SDEROT_DBG("regdma slot[%d] ==> %p\n", idx, addr);
	return addr;
}

/**
 * sde_hw_rotator_put_regdma_segment(): update current regdma command buffer
 * pointer for current regdma segment
 * @ctx: Rotator Context
 * @wrptr: current regdma segment location
 */
static inline void sde_hw_rotator_put_regdma_segment(
		struct sde_hw_rotator_context *ctx,
		u32 *wrptr)
{
	u32 idx = sde_hw_rotator_get_regdma_ctxidx(ctx);

	ctx->regdma_wrptr = wrptr;
	SDEROT_DBG("regdma slot[%d] <== %p\n", idx, wrptr);
}

/**
 * sde_hw_rotator_put_ctx(): Storing rotator context according to its
 * timestamp.
 */
static inline void sde_hw_rotator_put_ctx(struct sde_hw_rotator_context *ctx)
{
	 struct sde_hw_rotator *rot = ctx->rot;
	 u32 idx = sde_hw_rotator_get_regdma_ctxidx(ctx);
	 unsigned long flags;

	 spin_lock_irqsave(&rot->rotisr_lock, flags);
	 rot->rotCtx[ctx->q_id][idx] = ctx;
	 spin_unlock_irqrestore(&rot->rotisr_lock, flags);

	 SDEROT_DBG("rotCtx[%d][%d] <== ctx:%p | session-id:%d\n",
			 ctx->q_id, idx, ctx, ctx->session_id);
}

/**
 * sde_hw_rotator_clr_ctx(): Clearing rotator context according to its
 * timestamp.
 */
static inline void sde_hw_rotator_clr_ctx(struct sde_hw_rotator_context *ctx)
{
	 struct sde_hw_rotator *rot = ctx->rot;
	 u32 idx = sde_hw_rotator_get_regdma_ctxidx(ctx);
	 unsigned long flags;

	 spin_lock_irqsave(&rot->rotisr_lock, flags);
	 rot->rotCtx[ctx->q_id][idx] = NULL;
	 spin_unlock_irqrestore(&rot->rotisr_lock, flags);

	 SDEROT_DBG("rotCtx[%d][%d] <== null | session-id:%d\n",
			 ctx->q_id, idx, ctx->session_id);
}

#endif /*_SDE_ROTATOR_R3_INTERNAL_H */
