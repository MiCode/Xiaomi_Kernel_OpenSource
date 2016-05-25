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
 */
#include "drm/msm_drm_pp.h"
#include "sde_hw_mdss.h"
#include "sde_hwio.h"
#include "sde_hw_catalog.h"
#include "sde_hw_dspp.h"

#define PCC_ENABLE BIT(0)
#define PCC_OP_MODE_OFF 0
#define PCC_CONST_COEFF_OFF 4
#define PCC_R_COEFF_OFF 0x10
#define PCC_G_COEFF_OFF 0x1C
#define PCC_B_COEFF_OFF 0x28
#define PCC_RG_COEFF_OFF 0x34
#define PCC_RB_COEFF_OFF 0x40
#define PCC_GB_COEFF_OFF 0x4C
#define PCC_RGB_COEFF_OFF 0x58
#define PCC_CONST_COEFF_MASK 0xFFFF
#define PCC_COEFF_MASK 0x3FFFF

static struct sde_dspp_cfg *_dspp_offset(enum sde_dspp dspp,
		struct sde_mdss_cfg *m,
		void __iomem *addr,
		struct sde_hw_blk_reg_map *b)
{
	int i;

	for (i = 0; i < m->dspp_count; i++) {
		if (dspp == m->dspp[i].id) {
			b->base_off = addr;
			b->blk_off = m->dspp[i].base;
			b->hwversion = m->hwversion;
			b->log_mask = SDE_DBG_MASK_DSPP;
			return &m->dspp[i];
		}
	}

	return ERR_PTR(-EINVAL);
}

void sde_dspp_setup_histogram(struct sde_hw_dspp *ctx, void *cfg)
{
}

void sde_dspp_read_histogram(struct sde_hw_dspp *ctx, void *cfg)
{
}

void sde_dspp_update_igc(struct sde_hw_dspp *ctx, void *cfg)
{
}

void sde_dspp_setup_pa(struct sde_hw_dspp *dspp, void *cfg)
{
}

void sde_dspp_setup_hue(struct sde_hw_dspp *dspp, void *cfg)
{
}

void sde_dspp_setup_pcc(struct sde_hw_dspp *ctx, void *cfg)
{
	struct sde_hw_cp_cfg *hw_cfg = cfg;
	struct drm_msm_pcc *pcc;
	void  __iomem *base;

	if (!hw_cfg  || (hw_cfg->len != sizeof(*pcc)  && hw_cfg->payload)) {
		DRM_ERROR(
			"hw_cfg %pK payload %pK payload size %d exp size %zd\n",
			hw_cfg, (hw_cfg ? hw_cfg->payload : NULL),
			((hw_cfg) ? hw_cfg->len : 0), sizeof(*pcc));
		return;
	}
	base = ctx->hw.base_off + ctx->cap->base;

	/* Turn off feature */
	if (!hw_cfg->payload) {
		SDE_REG_WRITE(&ctx->hw, ctx->cap->sblk->pcc.base,
			      PCC_OP_MODE_OFF);
		return;
	}
	DRM_DEBUG_DRIVER("Enable PCC feature\n");
	pcc = hw_cfg->payload;

	SDE_REG_WRITE(&ctx->hw, ctx->cap->sblk->pcc.base + PCC_CONST_COEFF_OFF,
				  pcc->r.c & PCC_CONST_COEFF_MASK);
	SDE_REG_WRITE(&ctx->hw,
		      ctx->cap->sblk->pcc.base + PCC_CONST_COEFF_OFF + 4,
		      pcc->g.c & PCC_CONST_COEFF_MASK);
	SDE_REG_WRITE(&ctx->hw,
		      ctx->cap->sblk->pcc.base + PCC_CONST_COEFF_OFF + 8,
		      pcc->b.c & PCC_CONST_COEFF_MASK);

	SDE_REG_WRITE(&ctx->hw, ctx->cap->sblk->pcc.base + PCC_R_COEFF_OFF,
				  pcc->r.r & PCC_COEFF_MASK);
	SDE_REG_WRITE(&ctx->hw, ctx->cap->sblk->pcc.base + PCC_R_COEFF_OFF + 4,
				  pcc->g.r & PCC_COEFF_MASK);
	SDE_REG_WRITE(&ctx->hw, ctx->cap->sblk->pcc.base + PCC_R_COEFF_OFF + 8,
				  pcc->b.r & PCC_COEFF_MASK);

	SDE_REG_WRITE(&ctx->hw, ctx->cap->sblk->pcc.base + PCC_G_COEFF_OFF,
				  pcc->r.g & PCC_COEFF_MASK);
	SDE_REG_WRITE(&ctx->hw, ctx->cap->sblk->pcc.base + PCC_G_COEFF_OFF + 4,
				  pcc->g.g & PCC_COEFF_MASK);
	SDE_REG_WRITE(&ctx->hw, ctx->cap->sblk->pcc.base + PCC_G_COEFF_OFF + 8,
				  pcc->b.g & PCC_COEFF_MASK);

	SDE_REG_WRITE(&ctx->hw, ctx->cap->sblk->pcc.base + PCC_B_COEFF_OFF,
				  pcc->r.b & PCC_COEFF_MASK);
	SDE_REG_WRITE(&ctx->hw, ctx->cap->sblk->pcc.base + PCC_B_COEFF_OFF + 4,
				  pcc->g.b & PCC_COEFF_MASK);
	SDE_REG_WRITE(&ctx->hw, ctx->cap->sblk->pcc.base + PCC_B_COEFF_OFF + 8,
				  pcc->b.b & PCC_COEFF_MASK);


	SDE_REG_WRITE(&ctx->hw, ctx->cap->sblk->pcc.base + PCC_RG_COEFF_OFF,
				  pcc->r.rg & PCC_COEFF_MASK);
	SDE_REG_WRITE(&ctx->hw, ctx->cap->sblk->pcc.base + PCC_RG_COEFF_OFF + 4,
				  pcc->g.rg & PCC_COEFF_MASK);
	SDE_REG_WRITE(&ctx->hw, ctx->cap->sblk->pcc.base + PCC_RG_COEFF_OFF + 8,
				  pcc->b.rg & PCC_COEFF_MASK);

	SDE_REG_WRITE(&ctx->hw, ctx->cap->sblk->pcc.base + PCC_RB_COEFF_OFF,
				  pcc->r.rb & PCC_COEFF_MASK);
	SDE_REG_WRITE(&ctx->hw, ctx->cap->sblk->pcc.base + PCC_RB_COEFF_OFF + 4,
				  pcc->g.rb & PCC_COEFF_MASK);
	SDE_REG_WRITE(&ctx->hw, ctx->cap->sblk->pcc.base + PCC_RB_COEFF_OFF + 8,
				  pcc->b.rb & PCC_COEFF_MASK);


	SDE_REG_WRITE(&ctx->hw, ctx->cap->sblk->pcc.base + PCC_GB_COEFF_OFF,
				  pcc->r.gb & PCC_COEFF_MASK);
	SDE_REG_WRITE(&ctx->hw, ctx->cap->sblk->pcc.base + PCC_GB_COEFF_OFF + 4,
				  pcc->g.gb & PCC_COEFF_MASK);
	SDE_REG_WRITE(&ctx->hw, ctx->cap->sblk->pcc.base + PCC_GB_COEFF_OFF + 8,
				  pcc->b.gb & PCC_COEFF_MASK);

	SDE_REG_WRITE(&ctx->hw, ctx->cap->sblk->pcc.base + PCC_RGB_COEFF_OFF,
				  pcc->r.rgb & PCC_COEFF_MASK);
	SDE_REG_WRITE(&ctx->hw,
		      ctx->cap->sblk->pcc.base + PCC_RGB_COEFF_OFF + 4,
		      pcc->g.rgb & PCC_COEFF_MASK);
	SDE_REG_WRITE(&ctx->hw,
		      ctx->cap->sblk->pcc.base + PCC_RGB_COEFF_OFF + 8,
		      pcc->b.rgb & PCC_COEFF_MASK);
	SDE_REG_WRITE(&ctx->hw, ctx->cap->sblk->pcc.base, PCC_ENABLE);
}

void sde_dspp_setup_sharpening(struct sde_hw_dspp *ctx, void *cfg)
{
}

void sde_dspp_setup_pa_memcolor(struct sde_hw_dspp *ctx, void *cfg)
{
}

void sde_dspp_setup_sixzone(struct sde_hw_dspp *dspp)
{
}

void sde_dspp_setup_danger_safe(struct sde_hw_dspp *ctx, void *cfg)
{
}

void sde_dspp_setup_dither(struct sde_hw_dspp *ctx, void *cfg)
{
}

static void _setup_dspp_ops(struct sde_hw_dspp *c, unsigned long features)
{
	int i = 0;

	for (i = 0; i < SDE_DSPP_MAX; i++) {
		if (!test_bit(i, &features))
			continue;
		switch (i) {
		case SDE_DSPP_PCC:
			if (c->cap->sblk->pcc.version ==
				(SDE_COLOR_PROCESS_VER(0x1, 0x0)))
				c->ops.setup_pcc = sde_dspp_setup_pcc;
			break;
		case SDE_DSPP_HSIC:
			if (c->cap->sblk->hsic.version ==
				(SDE_COLOR_PROCESS_VER(0x1, 0x0)))
				c->ops.setup_hue = sde_dspp_setup_hue;
			break;
		default:
			break;
		}

	}
}

struct sde_hw_dspp *sde_hw_dspp_init(enum sde_dspp idx,
			void __iomem *addr,
			struct sde_mdss_cfg *m)
{
	struct sde_hw_dspp *c;
	struct sde_dspp_cfg *cfg;

	c = kzalloc(sizeof(*c), GFP_KERNEL);
	if (!c)
		return ERR_PTR(-ENOMEM);

	cfg = _dspp_offset(idx, m, addr, &c->hw);
	if (IS_ERR_OR_NULL(cfg)) {
		kfree(c);
		return ERR_PTR(-EINVAL);
	}

	/* Assign ops */
	c->idx = idx;
	c->cap = cfg;
	_setup_dspp_ops(c, c->cap->features);

	return c;
}

void sde_hw_dspp_destroy(struct sde_hw_dspp *dspp)
{
	kfree(dspp);
}
