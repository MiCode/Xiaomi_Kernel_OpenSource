/* Copyright (c) 2016, The Linux Foundation. All rights reserved.
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

#include <drm/msm_drm_pp.h>
#include "sde_hw_color_processing_v1_7.h"

#define PA_HUE_VIG_OFF		0x110
#define PA_SAT_VIG_OFF		0x114
#define PA_VAL_VIG_OFF		0x118
#define PA_CONT_VIG_OFF		0x11C

#define PA_HUE_DSPP_OFF		0x238
#define PA_SAT_DSPP_OFF		0x23C
#define PA_VAL_DSPP_OFF		0x240
#define PA_CONT_DSPP_OFF	0x244

#define PA_LUTV_DSPP_OFF	0x1400
#define PA_LUT_SWAP_OFF		0x234

#define PA_HUE_MASK		0xFFF
#define PA_SAT_MASK		0xFFFF
#define PA_VAL_MASK		0xFF
#define PA_CONT_MASK		0xFF

#define MEMCOL_PWL0_OFF		0x88
#define MEMCOL_PWL0_MASK	0xFFFF07FF
#define MEMCOL_PWL1_OFF		0x8C
#define MEMCOL_PWL1_MASK	0xFFFFFFFF
#define MEMCOL_HUE_REGION_OFF	0x90
#define MEMCOL_HUE_REGION_MASK	0x7FF07FF
#define MEMCOL_SAT_REGION_OFF	0x94
#define MEMCOL_SAT_REGION_MASK	0xFFFFFF
#define MEMCOL_VAL_REGION_OFF	0x98
#define MEMCOL_VAL_REGION_MASK	0xFFFFFF
#define MEMCOL_P0_LEN		0x14
#define MEMCOL_P1_LEN		0x8
#define MEMCOL_PWL2_OFF		0x218
#define MEMCOL_PWL2_MASK	0xFFFFFFFF
#define MEMCOL_BLEND_GAIN_OFF	0x21C
#define MEMCOL_PWL_HOLD_OFF	0x214

#define VIG_OP_PA_EN		BIT(4)
#define VIG_OP_PA_SKIN_EN	BIT(5)
#define VIG_OP_PA_FOL_EN	BIT(6)
#define VIG_OP_PA_SKY_EN	BIT(7)
#define VIG_OP_PA_HUE_EN	BIT(25)
#define VIG_OP_PA_SAT_EN	BIT(26)
#define VIG_OP_PA_VAL_EN	BIT(27)
#define VIG_OP_PA_CONT_EN	BIT(28)

#define DSPP_OP_SZ_VAL_EN	BIT(31)
#define DSPP_OP_SZ_SAT_EN	BIT(30)
#define DSPP_OP_SZ_HUE_EN	BIT(29)
#define DSPP_OP_PA_HUE_EN	BIT(25)
#define DSPP_OP_PA_SAT_EN	BIT(26)
#define DSPP_OP_PA_VAL_EN	BIT(27)
#define DSPP_OP_PA_CONT_EN	BIT(28)
#define DSPP_OP_PA_EN		BIT(20)
#define DSPP_OP_PA_LUTV_EN	BIT(19)
#define DSPP_OP_PA_SKIN_EN	BIT(5)
#define DSPP_OP_PA_FOL_EN	BIT(6)
#define DSPP_OP_PA_SKY_EN	BIT(7)

#define REG_MASK(n) ((BIT(n)) - 1)

#define PA_VIG_DISABLE_REQUIRED(x) \
			!((x) & (VIG_OP_PA_SKIN_EN | VIG_OP_PA_SKY_EN | \
			VIG_OP_PA_FOL_EN | VIG_OP_PA_HUE_EN | \
			VIG_OP_PA_SAT_EN | VIG_OP_PA_VAL_EN | \
			VIG_OP_PA_CONT_EN))


#define PA_DSPP_DISABLE_REQUIRED(x) \
			!((x) & (DSPP_OP_PA_SKIN_EN | DSPP_OP_PA_SKY_EN | \
			DSPP_OP_PA_FOL_EN | DSPP_OP_PA_HUE_EN | \
			DSPP_OP_PA_SAT_EN | DSPP_OP_PA_VAL_EN | \
			DSPP_OP_PA_CONT_EN | DSPP_OP_PA_LUTV_EN))

#define DSPP_OP_PCC_ENABLE	BIT(0)
#define PCC_OP_MODE_OFF		0
#define PCC_CONST_COEFF_OFF	4
#define PCC_R_COEFF_OFF		0x10
#define PCC_G_COEFF_OFF		0x1C
#define PCC_B_COEFF_OFF		0x28
#define PCC_RG_COEFF_OFF	0x34
#define PCC_RB_COEFF_OFF	0x40
#define PCC_GB_COEFF_OFF	0x4C
#define PCC_RGB_COEFF_OFF	0x58
#define PCC_CONST_COEFF_MASK	0xFFFF
#define PCC_COEFF_MASK		0x3FFFF

#define SSPP	0
#define DSPP	1

static void __setup_pa_hue(struct sde_hw_blk_reg_map *hw,
			const struct sde_pp_blk *blk, uint32_t hue,
			int location)
{
	u32 base = blk->base;
	u32 offset = (location == DSPP) ? PA_HUE_DSPP_OFF : PA_HUE_VIG_OFF;
	u32 op_hue_en = (location == DSPP) ? DSPP_OP_PA_HUE_EN :
					VIG_OP_PA_HUE_EN;
	u32 op_pa_en = (location == DSPP) ? DSPP_OP_PA_EN : VIG_OP_PA_EN;
	u32 disable_req;
	u32 opmode;

	SDE_REG_WRITE(hw, base + offset, hue & PA_HUE_MASK);

	opmode = SDE_REG_READ(hw, base);

	if (!hue) {
		opmode &= ~op_hue_en;
		disable_req = (location == DSPP) ?
			PA_DSPP_DISABLE_REQUIRED(opmode) :
			PA_VIG_DISABLE_REQUIRED(opmode);
		if (disable_req)
			opmode &= ~op_pa_en;
	} else {
		opmode |= op_hue_en | op_pa_en;
	}

	SDE_REG_WRITE(hw, base, opmode);
}

void sde_setup_pipe_pa_hue_v1_7(struct sde_hw_pipe *ctx, void *cfg)
{
	uint32_t hue = *((uint32_t *)cfg);

	__setup_pa_hue(&ctx->hw, &ctx->cap->sblk->hsic_blk, hue, SSPP);
}

void sde_setup_dspp_pa_hue_v1_7(struct sde_hw_dspp *ctx, void *cfg)
{
	uint32_t hue = *((uint32_t *)cfg);

	__setup_pa_hue(&ctx->hw, &ctx->cap->sblk->hsic, hue, DSPP);
}

static void __setup_pa_sat(struct sde_hw_blk_reg_map *hw,
			const struct sde_pp_blk *blk, uint32_t sat,
			int location)
{
	u32 base = blk->base;
	u32 offset = (location == DSPP) ? PA_SAT_DSPP_OFF : PA_SAT_VIG_OFF;
	u32 op_sat_en = (location == DSPP) ?
			DSPP_OP_PA_SAT_EN : VIG_OP_PA_SAT_EN;
	u32 op_pa_en = (location == DSPP) ? DSPP_OP_PA_EN : VIG_OP_PA_EN;
	u32 disable_req;
	u32 opmode;

	SDE_REG_WRITE(hw, base + offset, sat & PA_SAT_MASK);

	opmode = SDE_REG_READ(hw, base);

	if (!sat) {
		opmode &= ~op_sat_en;
		disable_req = (location == DSPP) ?
			PA_DSPP_DISABLE_REQUIRED(opmode) :
			PA_VIG_DISABLE_REQUIRED(opmode);
		if (disable_req)
			opmode &= ~op_pa_en;
	} else {
		opmode |= op_sat_en | op_pa_en;
	}

	SDE_REG_WRITE(hw, base, opmode);
}

void sde_setup_pipe_pa_sat_v1_7(struct sde_hw_pipe *ctx, void *cfg)
{
	uint32_t sat = *((uint32_t *)cfg);

	__setup_pa_sat(&ctx->hw, &ctx->cap->sblk->hsic_blk, sat, SSPP);
}

static void __setup_pa_val(struct sde_hw_blk_reg_map *hw,
			const struct sde_pp_blk *blk, uint32_t value,
			int location)
{
	u32 base = blk->base;
	u32 offset = (location == DSPP) ? PA_VAL_DSPP_OFF : PA_VAL_VIG_OFF;
	u32 op_val_en = (location == DSPP) ?
			DSPP_OP_PA_VAL_EN : VIG_OP_PA_VAL_EN;
	u32 op_pa_en = (location == DSPP) ? DSPP_OP_PA_EN : VIG_OP_PA_EN;
	u32 disable_req;
	u32 opmode;

	SDE_REG_WRITE(hw, base + offset, value & PA_VAL_MASK);

	opmode = SDE_REG_READ(hw, base);

	if (!value) {
		opmode &= ~op_val_en;
		disable_req = (location == DSPP) ?
			PA_DSPP_DISABLE_REQUIRED(opmode) :
			PA_VIG_DISABLE_REQUIRED(opmode);
		if (disable_req)
			opmode &= ~op_pa_en;
	} else {
		opmode |= op_val_en | op_pa_en;
	}

	SDE_REG_WRITE(hw, base, opmode);
}

void sde_setup_pipe_pa_val_v1_7(struct sde_hw_pipe *ctx, void *cfg)
{
	uint32_t value = *((uint32_t *)cfg);

	__setup_pa_val(&ctx->hw, &ctx->cap->sblk->hsic_blk, value, SSPP);
}

static void __setup_pa_cont(struct sde_hw_blk_reg_map *hw,
			const struct sde_pp_blk *blk, uint32_t contrast,
			int location)
{
	u32 base = blk->base;
	u32 offset = (location == DSPP) ? PA_CONT_DSPP_OFF : PA_CONT_VIG_OFF;
	u32 op_cont_en = (location == DSPP) ? DSPP_OP_PA_CONT_EN :
					VIG_OP_PA_CONT_EN;
	u32 op_pa_en = (location == DSPP) ? DSPP_OP_PA_EN : VIG_OP_PA_EN;
	u32 disable_req;
	u32 opmode;

	SDE_REG_WRITE(hw, base + offset, contrast & PA_CONT_MASK);

	opmode = SDE_REG_READ(hw, base);

	if (!contrast) {
		opmode &= ~op_cont_en;
		disable_req = (location == DSPP) ?
			PA_DSPP_DISABLE_REQUIRED(opmode) :
			PA_VIG_DISABLE_REQUIRED(opmode);
		if (disable_req)
			opmode &= ~op_pa_en;
	} else {
		opmode |= op_cont_en | op_pa_en;
	}

	SDE_REG_WRITE(hw, base, opmode);
}

void sde_setup_pipe_pa_cont_v1_7(struct sde_hw_pipe *ctx, void *cfg)
{
	uint32_t contrast = *((uint32_t *)cfg);

	__setup_pa_cont(&ctx->hw, &ctx->cap->sblk->hsic_blk, contrast, SSPP);
}

void sde_setup_pipe_pa_memcol_v1_7(struct sde_hw_pipe *ctx,
				   enum sde_memcolor_type type,
				   void *cfg)
{
	struct drm_msm_memcol *mc = cfg;
	u32 base = ctx->cap->sblk->memcolor_blk.base;
	u32 off, op, mc_en, hold = 0;
	u32 mc_i = 0;

	switch (type) {
	case MEMCOLOR_SKIN:
		mc_en = VIG_OP_PA_SKIN_EN;
		mc_i = 0;
		break;
	case MEMCOLOR_SKY:
		mc_en = VIG_OP_PA_SKY_EN;
		mc_i = 1;
		break;
	case MEMCOLOR_FOLIAGE:
		mc_en = VIG_OP_PA_FOL_EN;
		mc_i = 2;
		break;
	default:
		DRM_ERROR("Invalid memory color type %d\n", type);
		return;
	}

	op = SDE_REG_READ(&ctx->hw, base);
	if (!mc) {
		op &= ~mc_en;
		if (PA_VIG_DISABLE_REQUIRED(op))
			op &= ~VIG_OP_PA_EN;
		SDE_REG_WRITE(&ctx->hw, base, op);
		return;
	}

	off = base + (mc_i * MEMCOL_P0_LEN);
	SDE_REG_WRITE(&ctx->hw, (off + MEMCOL_PWL0_OFF),
		      mc->color_adjust_p0 & MEMCOL_PWL0_MASK);
	SDE_REG_WRITE(&ctx->hw, (off + MEMCOL_PWL1_OFF),
		      mc->color_adjust_p1 & MEMCOL_PWL1_MASK);
	SDE_REG_WRITE(&ctx->hw, (off + MEMCOL_HUE_REGION_OFF),
		      mc->hue_region & MEMCOL_HUE_REGION_MASK);
	SDE_REG_WRITE(&ctx->hw, (off + MEMCOL_SAT_REGION_OFF),
		      mc->sat_region & MEMCOL_SAT_REGION_MASK);
	SDE_REG_WRITE(&ctx->hw, (off + MEMCOL_VAL_REGION_OFF),
		      mc->val_region & MEMCOL_VAL_REGION_MASK);

	off = base + (mc_i * MEMCOL_P1_LEN);
	SDE_REG_WRITE(&ctx->hw, (off + MEMCOL_PWL2_OFF),
		      mc->color_adjust_p2 & MEMCOL_PWL2_MASK);
	SDE_REG_WRITE(&ctx->hw, (off + MEMCOL_BLEND_GAIN_OFF), mc->blend_gain);

	hold = SDE_REG_READ(&ctx->hw, off + MEMCOL_PWL_HOLD_OFF);
	hold &= ~(0xF << (mc_i * 4));
	hold |= ((mc->sat_hold & 0x3) << (mc_i * 4));
	hold |= ((mc->val_hold & 0x3) << ((mc_i * 4) + 2));
	SDE_REG_WRITE(&ctx->hw, (off + MEMCOL_PWL_HOLD_OFF), hold);

	op |= VIG_OP_PA_EN | mc_en;
	SDE_REG_WRITE(&ctx->hw, base, op);
}

void sde_setup_dspp_pcc_v1_7(struct sde_hw_dspp *ctx, void *cfg)
{
	struct sde_hw_cp_cfg *hw_cfg = cfg;
	struct drm_msm_pcc *pcc;
	void  __iomem *base;

	if (!hw_cfg  || (hw_cfg->len != sizeof(*pcc)  && hw_cfg->payload)) {
		DRM_ERROR("invalid params hw %p payload %p payloadsize %d \"\
			  exp size %zd\n",
			   hw_cfg, ((hw_cfg) ? hw_cfg->payload : NULL),
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
	SDE_REG_WRITE(&ctx->hw, ctx->cap->sblk->pcc.base, DSPP_OP_PCC_ENABLE);
}

void sde_setup_dspp_pa_vlut_v1_7(struct sde_hw_dspp *ctx, void *cfg)
{
	struct drm_msm_pa_vlut *payload = NULL;
	struct sde_hw_cp_cfg *hw_cfg = cfg;
	u32 base = ctx->cap->sblk->vlut.base;
	u32 offset = base + PA_LUTV_DSPP_OFF;
	u32 op_mode, tmp;
	int i = 0, j = 0;

	if (!hw_cfg || (hw_cfg->payload && hw_cfg->len !=
			sizeof(struct drm_msm_pa_vlut))) {
		DRM_ERROR("hw %pK payload %pK payloadsize %d exp size %zd\n",
			  hw_cfg, ((hw_cfg) ? hw_cfg->payload : NULL),
			  ((hw_cfg) ? hw_cfg->len : 0),
			  sizeof(struct drm_msm_pa_vlut));
		return;
	}
	op_mode = SDE_REG_READ(&ctx->hw, base);
	if (!hw_cfg->payload) {
		DRM_DEBUG_DRIVER("Disable vlut feature\n");
		/**
		 * In the PA_VLUT disable case, remove PA_VLUT enable bit(19)
		 * first, then check whether any other PA sub-features are
		 * enabled or not. If none of the sub-features are enabled,
		 * remove the PA global enable bit(20).
		 */
		op_mode &= ~((u32)DSPP_OP_PA_LUTV_EN);
		if (PA_DSPP_DISABLE_REQUIRED(op_mode))
			op_mode &= ~((u32)DSPP_OP_PA_EN);
		SDE_REG_WRITE(&ctx->hw, base, op_mode);
		return;
	}
	payload = hw_cfg->payload;
	DRM_DEBUG_DRIVER("Enable vlut feature flags %llx\n", payload->flags);
	for (i = 0, j = 0; i < ARRAY_SIZE(payload->val); i += 2, j += 4) {
		tmp = (payload->val[i] & REG_MASK(10)) |
			((payload->val[i + 1] & REG_MASK(10)) << 16);
		SDE_REG_WRITE(&ctx->hw, (offset + j),
			     tmp);
	}
	SDE_REG_WRITE(&ctx->hw, (base + PA_LUT_SWAP_OFF), 1);
	op_mode |= DSPP_OP_PA_EN | DSPP_OP_PA_LUTV_EN;
	SDE_REG_WRITE(&ctx->hw, base, op_mode);
}
