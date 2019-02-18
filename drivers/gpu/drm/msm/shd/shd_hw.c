/* Copyright (c) 2015-2019, The Linux Foundation. All rights reserved.
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

#define pr_fmt(fmt)	"[drm-shd:%s:%d] " fmt, __func__, __LINE__

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/debugfs.h>
#include <linux/component.h>
#include <linux/of_irq.h>
#include <linux/kthread.h>
#include <uapi/linux/sched/types.h>
#include <drm/drm_atomic_helper.h>
#include <drm/drm_atomic.h>
#include <drm/drm_crtc.h>
#include <drm/drm_vblank.h>
#include "msm_drv.h"
#include "msm_kms.h"
#include "sde_connector.h"
#include "sde_encoder.h"
#include "sde_crtc.h"
#include "sde_plane.h"
#include "shd_drm.h"
#include "shd_hw.h"

#define CTL_SSPP_FLUSH_MASK              0x3041807

#define CTL_MIXER_FLUSH_MASK             0x00207C0

#define   CTL_LAYER(lm)                 \
		(((lm) == LM_5) ? (0x024) : (((lm) - LM_0) * 0x004))
#define   CTL_LAYER_EXT(lm)             \
		(0x40 + (((lm) - LM_0) * 0x004))
#define   CTL_LAYER_EXT2(lm)             \
		(0x70 + (((lm) - LM_0) * 0x004))
#define   CTL_LAYER_EXT3(lm)             \
		(0xA0 + (((lm) - LM_0) * 0x004))

#define CTL_MIXER_BORDER_OUT            BIT(24)

#define CTL_FLUSH_MASK                   0x090

#define LM_BLEND0_OP                     0x00

#define CTL_NUM_EXT			4

#define CTL_SSPP_MAX_RECTS		2

static DEFINE_SPINLOCK(hw_ctl_lock);

/**
 * struct ctl_sspp_stage_reg_map: Describes bit layout for a sspp stage cfg
 * @ext: Index to indicate LAYER_x_EXT id for given sspp
 * @start: Start position of blend stage bits for given sspp
 * @bits: Number of bits from @start assigned for given sspp
 * @sec_bit_mask: Bitmask to add to LAYER_x_EXT1 for missing bit of given sspp
 */
struct ctl_sspp_stage_reg_map {
	u32 ext;
	u32 start;
	u32 bits;
	u32 sec_bit_mask;
};

/**
 * list of ctl_sspp_stage_reg_map for all the sppp
 */
static const struct ctl_sspp_stage_reg_map
	sspp_reg_cfg_tbl[SSPP_MAX][CTL_SSPP_MAX_RECTS] = {
	/* SSPP_NONE */{ {0, 0, 0, 0}, {0, 0, 0, 0} },
	/* SSPP_VIG0 */{ {0, 0, 3, BIT(0)}, {3, 0, 4, 0} },
	/* SSPP_VIG1 */{ {0, 3, 3, BIT(2)}, {3, 4, 4, 0} },
	/* SSPP_VIG2 */{ {0, 6, 3, BIT(4)}, {3, 8, 4, 0} },
	/* SSPP_VIG3 */{ {0, 26, 3, BIT(6)}, {3, 12, 4, 0} },
	/* SSPP_RGB0 */{ {0, 9, 3, BIT(8)}, {0, 0, 0, 0} },
	/* SSPP_RGB1 */{ {0, 12, 3, BIT(10)}, {0, 0, 0, 0} },
	/* SSPP_RGB2 */{ {0, 15, 3, BIT(12)}, {0, 0, 0, 0} },
	/* SSPP_RGB3 */{ {0, 29, 3, BIT(14)}, {0, 0, 0, 0} },
	/* SSPP_DMA0 */{ {0, 18, 3, BIT(16)}, {2, 8, 4, 0} },
	/* SSPP_DMA1 */{ {0, 21, 3, BIT(18)}, {2, 12, 4, 0} },
	/* SSPP_DMA2 */{ {2, 0, 4, 0}, {2, 16, 4, 0} },
	/* SSPP_DMA3 */{ {2, 4, 4, 0}, {2, 20, 4, 0} },
	/* SSPP_CURSOR0 */ { {1, 20, 4, 0}, {0, 0, 0, 0} },
	/* SSPP_CURSOR1 */ { {1, 26, 4, 0}, {0, 0, 0, 0} }
};

static void _sde_shd_hw_ctl_clear_blendstages_in_range(
	struct sde_shd_hw_ctl *hw_ctl, enum sde_lm lm)
{
	struct sde_hw_ctl *ctx = &hw_ctl->base;
	struct sde_hw_blk_reg_map *c = &ctx->hw;
	u32 mixercfg[4];
	u32 mixermask[4] = {0, 0, 0, 0};
	u32 start = hw_ctl->range.start + SDE_STAGE_0;
	u32 end = start + hw_ctl->range.size;
	int pipes_per_stage;
	int i, j;
	u32 value, mask;
	const struct ctl_sspp_stage_reg_map *sspp_cfg;

	mixercfg[0] = SDE_REG_READ(c, CTL_LAYER(lm));
	mixercfg[1] = SDE_REG_READ(c, CTL_LAYER_EXT(lm));
	mixercfg[2] = SDE_REG_READ(c, CTL_LAYER_EXT2(lm));
	mixercfg[3] = SDE_REG_READ(c, CTL_LAYER_EXT3(lm));

	if (!((mixercfg[0] & ~CTL_MIXER_BORDER_OUT) |
		mixercfg[1] | mixercfg[2] | mixercfg[3]))
		goto end;

	if (test_bit(SDE_MIXER_SOURCESPLIT,
		&ctx->mixer_hw_caps->features))
		pipes_per_stage = PIPES_PER_STAGE;
	else
		pipes_per_stage = 1;

	for (i = 1; i < SSPP_MAX; i++) {
		for (j = 0 ; j < pipes_per_stage; j++) {
			sspp_cfg = &sspp_reg_cfg_tbl[i][j];
			if (!sspp_cfg->bits)
				continue;

			mask = (1 << sspp_cfg->bits) - 1;
			value = (mixercfg[sspp_cfg->ext] >> sspp_cfg->start);
			value &= mask;
			if (mixercfg[1] & sspp_cfg->sec_bit_mask)
				value |= (1 << sspp_cfg->bits);

			if (value > start && value <= end) {
				mixermask[sspp_cfg->ext] |=
					(mask << sspp_cfg->start);
				mixermask[1] |= sspp_cfg->sec_bit_mask;
			}
		}
	}

end:
	hw_ctl->mixer_cfg[lm].mixercfg_mask = mixermask[0];
	hw_ctl->mixer_cfg[lm].mixercfg_ext_mask = mixermask[1];
	hw_ctl->mixer_cfg[lm].mixercfg_ext2_mask = mixermask[2];
	hw_ctl->mixer_cfg[lm].mixercfg_ext3_mask = mixermask[3];
}

static inline
void _sde_shd_hw_ctl_clear_all_blendstages(struct sde_hw_ctl *ctx)
{
	struct sde_shd_hw_ctl *hw_ctl;
	int i;

	if (!ctx)
		return;

	hw_ctl = container_of(ctx, struct sde_shd_hw_ctl, base);

	for (i = 0; i < ctx->mixer_count; i++) {
		int mixer_id = ctx->mixer_hw_caps[i].id;

		_sde_shd_hw_ctl_clear_blendstages_in_range(hw_ctl, mixer_id);
	}
}

static inline int _stage_offset(struct sde_hw_mixer *ctx, enum sde_stage stage)
{
	const struct sde_lm_sub_blks *sblk = ctx->cap->sblk;
	int rc;

	if (stage == SDE_STAGE_BASE)
		rc = -EINVAL;
	else if (stage <= sblk->maxblendstages)
		rc = sblk->blendstage_base[stage - SDE_STAGE_0];
	else
		rc = -EINVAL;

	return rc;
}

static void _sde_shd_hw_ctl_setup_blendstage(struct sde_hw_ctl *ctx,
	enum sde_lm lm, struct sde_hw_stage_cfg *stage_cfg)
{
	struct sde_shd_hw_ctl *hw_ctl;
	int i, j;
	int stages;
	int pipes_per_stage;
	int stage_offset = 0;
	u32 pipe_idx, rect_idx;
	const struct ctl_sspp_stage_reg_map *sspp_cfg;
	u32 mixercfg[CTL_NUM_EXT] = {CTL_MIXER_BORDER_OUT, 0, 0, 0};
	u32 mixermask[CTL_NUM_EXT] = {0, 0, 0, 0};
	u32 value, mask, stage_value;

	if (!ctx)
		return;

	hw_ctl = container_of(ctx, struct sde_shd_hw_ctl, base);

	stages = hw_ctl->range.size - 1;
	if (stages < 0)
		return;

	if (test_bit(SDE_MIXER_SOURCESPLIT,
		&ctx->mixer_hw_caps->features))
		pipes_per_stage = PIPES_PER_STAGE;
	else
		pipes_per_stage = 1;

	_sde_shd_hw_ctl_clear_blendstages_in_range(hw_ctl, lm);

	if (!stage_cfg)
		goto exit;

	stage_offset = hw_ctl->range.start;

	for (i = 0; i <= stages; i++) {
		for (j = 0 ; j < pipes_per_stage; j++) {
			pipe_idx = stage_cfg->stage[i][j];
			if (!pipe_idx || pipe_idx >= SSPP_MAX)
				continue;

			rect_idx = (stage_cfg->multirect_index[i][j]
					== SDE_SSPP_RECT_1);

			sspp_cfg = &sspp_reg_cfg_tbl[pipe_idx][rect_idx];
			if (!sspp_cfg->bits)
				continue;

			stage_value = i + stage_offset + 1;
			mask = (1 << sspp_cfg->bits) - 1;
			value = mask & stage_value;
			mixercfg[sspp_cfg->ext] |= (value << sspp_cfg->start);
			if (stage_value > mask)
				mixercfg[1] |= sspp_cfg->sec_bit_mask;

			mixermask[sspp_cfg->ext] |= (mask << sspp_cfg->start);
			mixermask[1] |= sspp_cfg->sec_bit_mask;
		}
	}

	hw_ctl->mixer_cfg[lm].mixercfg_mask |= mixermask[0];
	hw_ctl->mixer_cfg[lm].mixercfg_ext_mask |= mixermask[1];
	hw_ctl->mixer_cfg[lm].mixercfg_ext2_mask |= mixermask[2];
	hw_ctl->mixer_cfg[lm].mixercfg_ext3_mask |= mixermask[3];
exit:
	hw_ctl->mixer_cfg[lm].mixercfg = mixercfg[0];
	hw_ctl->mixer_cfg[lm].mixercfg_ext = mixercfg[1];
	hw_ctl->mixer_cfg[lm].mixercfg_ext2 = mixercfg[2];
	hw_ctl->mixer_cfg[lm].mixercfg_ext3 = mixercfg[3];
}

static void _sde_shd_flush_hw_ctl(struct sde_hw_ctl *ctx)
{
	struct sde_shd_hw_ctl *hw_ctl;
	struct sde_hw_blk_reg_map *c;
	u32 mixercfg, mixercfg_ext;
	u32 mixercfg_ext2, mixercfg_ext3;
	int i;

	hw_ctl = container_of(ctx, struct sde_shd_hw_ctl, base);

	hw_ctl->flush_mask = ctx->flush.pending_flush_mask;

	hw_ctl->flush_mask &= CTL_SSPP_FLUSH_MASK;

	c = &ctx->hw;

	for (i = 0; i < ctx->mixer_count; i++) {
		int lm = ctx->mixer_hw_caps[i].id;

		mixercfg = SDE_REG_READ(c, CTL_LAYER(lm));
		mixercfg_ext = SDE_REG_READ(c, CTL_LAYER_EXT(lm));
		mixercfg_ext2 = SDE_REG_READ(c, CTL_LAYER_EXT2(lm));
		mixercfg_ext3 = SDE_REG_READ(c, CTL_LAYER_EXT3(lm));

		mixercfg &= ~hw_ctl->mixer_cfg[lm].mixercfg_mask;
		mixercfg_ext &= ~hw_ctl->mixer_cfg[lm].mixercfg_ext_mask;
		mixercfg_ext2 &= ~hw_ctl->mixer_cfg[lm].mixercfg_ext2_mask;
		mixercfg_ext3 &= ~hw_ctl->mixer_cfg[lm].mixercfg_ext3_mask;

		mixercfg |= hw_ctl->mixer_cfg[lm].mixercfg;
		mixercfg_ext |= hw_ctl->mixer_cfg[lm].mixercfg_ext;
		mixercfg_ext2 |= hw_ctl->mixer_cfg[lm].mixercfg_ext2;
		mixercfg_ext3 |= hw_ctl->mixer_cfg[lm].mixercfg_ext3;

		SDE_REG_WRITE(c, CTL_LAYER(lm), mixercfg);
		SDE_REG_WRITE(c, CTL_LAYER_EXT(lm), mixercfg_ext);
		SDE_REG_WRITE(c, CTL_LAYER_EXT2(lm), mixercfg_ext2);
		SDE_REG_WRITE(c, CTL_LAYER_EXT3(lm), mixercfg_ext3);
	}
}

static void _sde_shd_setup_blend_config(struct sde_hw_mixer *ctx,
		uint32_t stage,
		uint32_t fg_alpha, uint32_t bg_alpha, uint32_t blend_op)
{
	struct sde_shd_hw_mixer *hw_lm;
	struct sde_shd_mixer_cfg *cfg;

	if (!ctx)
		return;

	hw_lm = container_of(ctx, struct sde_shd_hw_mixer, base);

	cfg = &hw_lm->cfg[stage + hw_lm->range.start];

	cfg->fg_alpha = fg_alpha;
	cfg->bg_alpha = bg_alpha;
	cfg->blend_op = blend_op;
	cfg->dirty = true;
}

static void _sde_shd_setup_dim_layer(struct sde_hw_mixer *ctx,
		struct sde_hw_dim_layer *dim_layer)
{
	struct sde_shd_hw_mixer *hw_lm;
	struct sde_hw_dim_layer dim_layer2;

	if (!ctx)
		return;

	hw_lm = container_of(ctx, struct sde_shd_hw_mixer, base);

	dim_layer2 = *dim_layer;
	dim_layer2.stage += hw_lm->range.start;
	dim_layer2.rect.x += hw_lm->roi.x;
	dim_layer2.rect.y += hw_lm->roi.y;

	hw_lm->cfg[dim_layer2.stage].dim_layer = dim_layer2;
	hw_lm->cfg[dim_layer2.stage].dim_layer_enable = true;
}

static void _sde_shd_clear_dim_layer(struct sde_hw_mixer *ctx)
{
	struct sde_shd_hw_mixer *hw_lm;
	int i;

	if (!ctx)
		return;

	hw_lm = container_of(ctx, struct sde_shd_hw_mixer, base);

	for (i = SDE_STAGE_0; i < SDE_STAGE_MAX; i++)
		hw_lm->cfg[i].dim_layer_enable = false;
}

static inline
void _sde_shd_setup_mixer_out(struct sde_hw_mixer *ctx,
		struct sde_hw_mixer_cfg *cfg)
{
	/* do nothing */
}

static void _sde_shd_flush_hw_lm(struct sde_hw_mixer *ctx)
{
	struct sde_shd_hw_mixer *hw_lm;
	struct sde_hw_blk_reg_map *c = &ctx->hw;
	int stage_off, i;
	u32 reset = BIT(16), val;
	int start, end;

	if (!ctx)
		return;

	hw_lm = container_of(ctx, struct sde_shd_hw_mixer, base);

	start = SDE_STAGE_0 + hw_lm->range.start;
	end = start + hw_lm->range.size;
	reset = ~reset;
	for (i = start; i < end; i++) {
		stage_off = _stage_offset(ctx, i);
		if (WARN_ON(stage_off < 0))
			return;

		if (hw_lm->cfg[i].dim_layer_enable) {
			hw_lm->orig->ops.setup_dim_layer(ctx,
				&hw_lm->cfg[i].dim_layer);
		} else {
			val = SDE_REG_READ(c, LM_BLEND0_OP + stage_off);
			val &= reset;
			SDE_REG_WRITE(c, LM_BLEND0_OP + stage_off, val);
		}

		if (hw_lm->cfg[i].dirty) {
			hw_lm->orig->ops.setup_blend_config(ctx, i,
				hw_lm->cfg[i].fg_alpha,
				hw_lm->cfg[i].bg_alpha,
				hw_lm->cfg[i].blend_op);
			hw_lm->cfg[i].dirty = false;
		}
	}
}

void sde_shd_hw_flush(struct sde_hw_ctl *ctl_ctx,
	struct sde_hw_mixer *lm_ctx[CRTC_DUAL_MIXERS], int lm_num)
{
	struct sde_hw_blk_reg_map *c;
	unsigned long lock_flags;
	int i;

	c = &ctl_ctx->hw;

	spin_lock_irqsave(&hw_ctl_lock, lock_flags);

	SDE_REG_WRITE(c, CTL_FLUSH_MASK, CTL_MIXER_FLUSH_MASK);

	_sde_shd_flush_hw_ctl(ctl_ctx);

	for (i = 0; i < lm_num; i++)
		_sde_shd_flush_hw_lm(lm_ctx[i]);

	if (ctl_ctx->ops.trigger_flush)
		ctl_ctx->ops.trigger_flush(ctl_ctx);

	SDE_REG_WRITE(c, CTL_FLUSH_MASK, 0);

	spin_unlock_irqrestore(&hw_ctl_lock, lock_flags);
}

void sde_shd_hw_ctl_init_op(struct sde_hw_ctl *ctx)
{
	ctx->ops.clear_all_blendstages =
		_sde_shd_hw_ctl_clear_all_blendstages;

	ctx->ops.setup_blendstage =
		_sde_shd_hw_ctl_setup_blendstage;

	ctx->ops.setup_intf_cfg_v1 = NULL;
}

void sde_shd_hw_lm_init_op(struct sde_hw_mixer *ctx)
{
	ctx->ops.setup_blend_config =
			_sde_shd_setup_blend_config;

	ctx->ops.setup_dim_layer =
			_sde_shd_setup_dim_layer;

	ctx->ops.setup_mixer_out =
			_sde_shd_setup_mixer_out;

	ctx->ops.clear_dim_layer =
			_sde_shd_clear_dim_layer;
}

