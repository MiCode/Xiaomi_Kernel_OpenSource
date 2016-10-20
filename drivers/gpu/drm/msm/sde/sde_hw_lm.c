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

#include "sde_hw_catalog.h"
#include "sde_hwio.h"
#include "sde_hw_lm.h"
#include "sde_hw_mdss.h"

#define LM_OP_MODE                        0x00
#define LM_OUT_SIZE                       0x04
#define LM_BORDER_COLOR_0                 0x08
#define LM_BORDER_COLOR_1                 0x010

/* These register are offset to mixer base + stage base */
#define LM_BLEND0_OP                     0x00
#define LM_BLEND0_CONST_ALPHA            0x04
#define LM_BLEND0_FG_ALPHA               0x04
#define LM_BLEND0_BG_ALPHA               0x08

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
		rc = sblk->blendstage_base[stage - 1];
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

static void sde_hw_lm_setup_blend_config_msmskunk(struct sde_hw_mixer *ctx,
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

static void _setup_mixer_ops(struct sde_mdss_cfg *m,
		struct sde_hw_lm_ops *ops,
		unsigned long cap)
{
	ops->setup_mixer_out = sde_hw_lm_setup_out;
	if (IS_MSMSKUNK_TARGET(m->hwversion))
		ops->setup_blend_config = sde_hw_lm_setup_blend_config_msmskunk;
	else
		ops->setup_blend_config = sde_hw_lm_setup_blend_config;
	ops->setup_alpha_out = sde_hw_lm_setup_color3;
	ops->setup_border_color = sde_hw_lm_setup_border_color;
	ops->setup_gc = sde_hw_lm_gc;
};

struct sde_hw_mixer *sde_hw_lm_init(enum sde_lm idx,
		void __iomem *addr,
		struct sde_mdss_cfg *m)
{
	struct sde_hw_mixer *c;
	struct sde_lm_cfg *cfg;

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

	/*
	 * Perform any default initialization for the sspp blocks
	 */
	return c;
}

void sde_hw_lm_destroy(struct sde_hw_mixer *lm)
{
	kfree(lm);
}
