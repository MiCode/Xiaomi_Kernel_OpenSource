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

#include "sde_kms.h"
#include "sde_hw_catalog.h"
#include "sde_hwio.h"
#include "sde_hw_lm.h"
#include "sde_hw_mdss.h"
#include "sde_dbg.h"
#include "sde_kms.h"

#define LM_OP_MODE                        0x00
#define LM_OUT_SIZE                       0x04
#define LM_BORDER_COLOR_0                 0x08
#define LM_BORDER_COLOR_1                 0x010

/* These register are offset to mixer base + stage base */
#define LM_BLEND0_OP                     0x00
#define LM_BLEND0_CONST_ALPHA            0x04
#define LM_FG_COLOR_FILL_COLOR_0         0x08
#define LM_FG_COLOR_FILL_COLOR_1         0x0C
#define LM_FG_COLOR_FILL_SIZE            0x10
#define LM_FG_COLOR_FILL_XY              0x14

#define LM_BLEND0_FG_ALPHA               0x04
#define LM_BLEND0_BG_ALPHA               0x08

#define LM_MISR_CTRL			0x310
#define LM_MISR_SIGNATURE		0x314

static struct sde_lm_cfg *_lm_offset(enum sde_lm mixer,
		struct sde_mdss_cfg *m,
		void __iomem *addr,
		struct sde_hw_blk_reg_map *b)
{
	int i;

	for (i = 0; i < m->mixer_count; i++) {
		if (mixer == m->mixer[i].id) {
			b->base_off = addr;
			b->blk_off = m->mixer[i].base;
			b->length = m->mixer[i].len;
			b->hwversion = m->hwversion;
			b->log_mask = SDE_DBG_MASK_LM;
			return &m->mixer[i];
		}
	}

	return ERR_PTR(-ENOMEM);
}

/**
 * _stage_offset(): returns the relative offset of the blend registers
 * for the stage to be setup
 * @c:     mixer ctx contains the mixer to be programmed
 * @stage: stage index to setup
 */
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

static void sde_hw_lm_setup_out(struct sde_hw_mixer *ctx,
		struct sde_hw_mixer_cfg *mixer)
{
	struct sde_hw_blk_reg_map *c = &ctx->hw;
	u32 outsize;
	u32 op_mode;

	op_mode = SDE_REG_READ(c, LM_OP_MODE);

	outsize = mixer->out_height << 16 | mixer->out_width;
	SDE_REG_WRITE(c, LM_OUT_SIZE, outsize);

	/* SPLIT_LEFT_RIGHT */
	if (mixer->right_mixer)
		op_mode |= BIT(31);
	else
		op_mode &= ~BIT(31);
	SDE_REG_WRITE(c, LM_OP_MODE, op_mode);
}

static void sde_hw_lm_setup_border_color(struct sde_hw_mixer *ctx,
		struct sde_mdss_color *color,
		u8 border_en)
{
	struct sde_hw_blk_reg_map *c = &ctx->hw;

	if (border_en) {
		SDE_REG_WRITE(c, LM_BORDER_COLOR_0,
			(color->color_0 & 0xFFF) |
			((color->color_1 & 0xFFF) << 0x10));
		SDE_REG_WRITE(c, LM_BORDER_COLOR_1,
			(color->color_2 & 0xFFF) |
			((color->color_3 & 0xFFF) << 0x10));
	}
}

static void sde_hw_lm_setup_blend_config_sdm845(struct sde_hw_mixer *ctx,
	u32 stage, u32 fg_alpha, u32 bg_alpha, u32 blend_op)
{
	struct sde_hw_blk_reg_map *c = &ctx->hw;
	int stage_off;
	u32 const_alpha;

	if (stage == SDE_STAGE_BASE)
		return;

	stage_off = _stage_offset(ctx, stage);
	if (WARN_ON(stage_off < 0))
		return;

	const_alpha = (bg_alpha & 0xFF) | ((fg_alpha & 0xFF) << 16);
	SDE_REG_WRITE(c, LM_BLEND0_CONST_ALPHA + stage_off, const_alpha);
	SDE_REG_WRITE(c, LM_BLEND0_OP + stage_off, blend_op);
}

static void sde_hw_lm_setup_blend_config(struct sde_hw_mixer *ctx,
	u32 stage, u32 fg_alpha, u32 bg_alpha, u32 blend_op)
{
	struct sde_hw_blk_reg_map *c = &ctx->hw;
	int stage_off;

	if (stage == SDE_STAGE_BASE)
		return;

	stage_off = _stage_offset(ctx, stage);
	if (WARN_ON(stage_off < 0))
		return;

	SDE_REG_WRITE(c, LM_BLEND0_FG_ALPHA + stage_off, fg_alpha);
	SDE_REG_WRITE(c, LM_BLEND0_BG_ALPHA + stage_off, bg_alpha);
	SDE_REG_WRITE(c, LM_BLEND0_OP + stage_off, blend_op);
}

static void sde_hw_lm_setup_color3(struct sde_hw_mixer *ctx,
	uint32_t mixer_op_mode)
{
	struct sde_hw_blk_reg_map *c = &ctx->hw;
	int op_mode;

	/* read the existing op_mode configuration */
	op_mode = SDE_REG_READ(c, LM_OP_MODE);

	op_mode = (op_mode & (BIT(31) | BIT(30))) | mixer_op_mode;

	SDE_REG_WRITE(c, LM_OP_MODE, op_mode);
}

static void sde_hw_lm_gc(struct sde_hw_mixer *mixer,
			void *cfg)
{
}

static void sde_hw_lm_clear_dim_layer(struct sde_hw_mixer *ctx)
{
	struct sde_hw_blk_reg_map *c = &ctx->hw;
	const struct sde_lm_sub_blks *sblk = ctx->cap->sblk;
	int stage_off, i;
	u32 reset = BIT(16), val;

	reset = ~reset;
	for (i = SDE_STAGE_0; i <= sblk->maxblendstages; i++) {
		stage_off = _stage_offset(ctx, i);
		if (WARN_ON(stage_off < 0))
			return;

		/*
		 * read the existing blendn_op register and clear only DIM layer
		 * bit (color_fill bit)
		 */
		val = SDE_REG_READ(c, LM_BLEND0_OP + stage_off);
		val &= reset;
		SDE_REG_WRITE(c, LM_BLEND0_OP + stage_off, val);
	}
}

static void sde_hw_lm_setup_dim_layer(struct sde_hw_mixer *ctx,
		struct sde_hw_dim_layer *dim_layer)
{
	struct sde_hw_blk_reg_map *c = &ctx->hw;
	int stage_off;
	u32 val = 0, alpha = 0;

	stage_off = _stage_offset(ctx, dim_layer->stage);
	if (stage_off < 0) {
		SDE_ERROR("invalid stage_off:%d for dim layer\n", stage_off);
		return;
	}

	alpha = dim_layer->color_fill.color_3 & 0xFF;
	val = ((dim_layer->color_fill.color_1 << 2) & 0xFFF) << 16 |
			((dim_layer->color_fill.color_0 << 2) & 0xFFF);
	SDE_REG_WRITE(c, LM_FG_COLOR_FILL_COLOR_0 + stage_off, val);

	val = (alpha << 4) << 16 |
			((dim_layer->color_fill.color_2 << 2) & 0xFFF);
	SDE_REG_WRITE(c, LM_FG_COLOR_FILL_COLOR_1 + stage_off, val);

	val = dim_layer->rect.h << 16 | dim_layer->rect.w;
	SDE_REG_WRITE(c, LM_FG_COLOR_FILL_SIZE + stage_off, val);

	val = dim_layer->rect.y << 16 | dim_layer->rect.x;
	SDE_REG_WRITE(c, LM_FG_COLOR_FILL_XY + stage_off, val);

	val = BIT(16); /* enable dim layer */
	val |= SDE_BLEND_FG_ALPHA_FG_CONST | SDE_BLEND_BG_ALPHA_BG_CONST;
	if (dim_layer->flags & SDE_DRM_DIM_LAYER_EXCLUSIVE)
		val |= BIT(17);
	else
		val &= ~BIT(17);
	SDE_REG_WRITE(c, LM_BLEND0_OP + stage_off, val);
	val = (alpha << 16) | (0xff - alpha);
	SDE_REG_WRITE(c, LM_BLEND0_CONST_ALPHA + stage_off, val);
}

static void sde_hw_lm_setup_misr(struct sde_hw_mixer *ctx,
				bool enable, u32 frame_count)
{
	struct sde_hw_blk_reg_map *c = &ctx->hw;
	u32 config = 0;

	SDE_REG_WRITE(c, LM_MISR_CTRL, MISR_CTRL_STATUS_CLEAR);
	/* clear misr data */
	wmb();

	if (enable)
		config = (frame_count & MISR_FRAME_COUNT_MASK) |
			MISR_CTRL_ENABLE | INTF_MISR_CTRL_FREE_RUN_MASK;

	SDE_REG_WRITE(c, LM_MISR_CTRL, config);
}

static u32 sde_hw_lm_collect_misr(struct sde_hw_mixer *ctx)
{
	struct sde_hw_blk_reg_map *c = &ctx->hw;

	return SDE_REG_READ(c, LM_MISR_SIGNATURE);
}

static void _setup_mixer_ops(struct sde_mdss_cfg *m,
		struct sde_hw_lm_ops *ops,
		unsigned long features)
{
	ops->setup_mixer_out = sde_hw_lm_setup_out;
	if (IS_SDM845_TARGET(m->hwversion) || IS_SDM670_TARGET(m->hwversion) ||
			IS_SM8150_TARGET(m->hwversion) ||
			IS_SDMSHRIKE_TARGET(m->hwversion) ||
			IS_SM6150_TARGET(m->hwversion) ||
			IS_SDMMAGPIE_TARGET(m->hwversion) ||
			IS_SDMTRINKET_TARGET(m->hwversion))
		ops->setup_blend_config = sde_hw_lm_setup_blend_config_sdm845;
	else
		ops->setup_blend_config = sde_hw_lm_setup_blend_config;
	ops->setup_alpha_out = sde_hw_lm_setup_color3;
	ops->setup_border_color = sde_hw_lm_setup_border_color;
	ops->setup_gc = sde_hw_lm_gc;
	ops->setup_misr = sde_hw_lm_setup_misr;
	ops->collect_misr = sde_hw_lm_collect_misr;

	if (test_bit(SDE_DIM_LAYER, &features)) {
		ops->setup_dim_layer = sde_hw_lm_setup_dim_layer;
		ops->clear_dim_layer = sde_hw_lm_clear_dim_layer;
	}
};

static struct sde_hw_blk_ops sde_hw_ops = {
	.start = NULL,
	.stop = NULL,
};

struct sde_hw_mixer *sde_hw_lm_init(enum sde_lm idx,
		void __iomem *addr,
		struct sde_mdss_cfg *m)
{
	struct sde_hw_mixer *c;
	struct sde_lm_cfg *cfg;
	int rc;

	c = kzalloc(sizeof(*c), GFP_KERNEL);
	if (!c)
		return ERR_PTR(-ENOMEM);

	cfg = _lm_offset(idx, m, addr, &c->hw);
	if (IS_ERR_OR_NULL(cfg)) {
		kfree(c);
		return ERR_PTR(-EINVAL);
	}

	/* Assign ops */
	c->idx = idx;
	c->cap = cfg;
	_setup_mixer_ops(m, &c->ops, c->cap->features);

	rc = sde_hw_blk_init(&c->base, SDE_HW_BLK_LM, idx, &sde_hw_ops);
	if (rc) {
		SDE_ERROR("failed to init hw blk %d\n", rc);
		goto blk_init_error;
	}

	sde_dbg_reg_register_dump_range(SDE_DBG_NAME, cfg->name, c->hw.blk_off,
			c->hw.blk_off + c->hw.length, c->hw.xin_id);

	return c;

blk_init_error:
	kzfree(c);

	return ERR_PTR(rc);
}

void sde_hw_lm_destroy(struct sde_hw_mixer *lm)
{
	if (lm)
		sde_hw_blk_destroy(&lm->base);
	kfree(lm);
}
