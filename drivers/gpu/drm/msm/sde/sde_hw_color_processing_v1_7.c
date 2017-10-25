/* Copyright (c) 2016-2017, The Linux Foundation. All rights reserved.
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
#include "sde_hw_ctl.h"

#define REG_MASK_SHIFT(n, shift) ((REG_MASK(n)) << (shift))

#define PA_HUE_VIG_OFF		0x110
#define PA_SAT_VIG_OFF		0x114
#define PA_VAL_VIG_OFF		0x118
#define PA_CONT_VIG_OFF		0x11C

#define PA_HUE_DSPP_OFF		0x1c
#define PA_SAT_DSPP_OFF		0x20
#define PA_VAL_DSPP_OFF		0x24
#define PA_CONT_DSPP_OFF	0x28

#define PA_HIST_CTRL_DSPP_OFF	0x4
#define PA_HIST_DATA_DSPP_OFF	0x400

#define PA_LUTV_DSPP_OFF	0x1400
#define PA_LUT_SWAP_OFF		0x234

#define PA_LUTV_DSPP_CTRL_OFF	0x4c
#define PA_LUTV_DSPP_SWAP_OFF	0x18

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
#define DSPP_OP_PA_HIST_EN	BIT(16)
#define DSPP_OP_PA_SKIN_EN	BIT(5)
#define DSPP_OP_PA_FOL_EN	BIT(6)
#define DSPP_OP_PA_SKY_EN	BIT(7)

#define DSPP_SZ_ADJ_CURVE_P1_OFF	0x4
#define DSPP_SZ_THRESHOLDS_OFF	0x8
#define DSPP_PA_PWL_HOLD_OFF	0x40

#define DSPP_MEMCOL_SIZE0	0x14
#define DSPP_MEMCOL_SIZE1	0x8
#define DSPP_MEMCOL_PWL0_OFF	0x0
#define DSPP_MEMCOL_PWL2_OFF	0x3C
#define DSPP_MEMCOL_HOLD_SIZE	0x4

#define DSPP_MEMCOL_PROT_VAL_EN BIT(24)
#define DSPP_MEMCOL_PROT_SAT_EN BIT(23)
#define DSPP_MEMCOL_PROT_HUE_EN BIT(22)
#define DSPP_MEMCOL_PROT_CONT_EN BIT(18)
#define DSPP_MEMCOL_PROT_SIXZONE_EN BIT(17)
#define DSPP_MEMCOL_PROT_BLEND_EN BIT(3)

#define DSPP_MEMCOL_MASK \
	(DSPP_OP_PA_SKIN_EN | DSPP_OP_PA_SKY_EN | DSPP_OP_PA_FOL_EN)

#define DSPP_MEMCOL_PROT_MASK \
	(DSPP_MEMCOL_PROT_HUE_EN | DSPP_MEMCOL_PROT_SAT_EN | \
	DSPP_MEMCOL_PROT_VAL_EN | DSPP_MEMCOL_PROT_CONT_EN | \
	DSPP_MEMCOL_PROT_SIXZONE_EN | DSPP_MEMCOL_PROT_BLEND_EN)

#define PA_VIG_DISABLE_REQUIRED(x) \
			!((x) & (VIG_OP_PA_SKIN_EN | VIG_OP_PA_SKY_EN | \
			VIG_OP_PA_FOL_EN | VIG_OP_PA_HUE_EN | \
			VIG_OP_PA_SAT_EN | VIG_OP_PA_VAL_EN | \
			VIG_OP_PA_CONT_EN))

#define PA_DSPP_DISABLE_REQUIRED(x) \
			!((x) & (DSPP_OP_PA_SKIN_EN | DSPP_OP_PA_SKY_EN | \
			DSPP_OP_PA_FOL_EN | DSPP_OP_PA_HUE_EN | \
			DSPP_OP_PA_SAT_EN | DSPP_OP_PA_VAL_EN | \
			DSPP_OP_PA_CONT_EN | DSPP_OP_PA_HIST_EN | \
			DSPP_OP_SZ_HUE_EN | DSPP_OP_SZ_SAT_EN | \
			DSPP_OP_SZ_VAL_EN))

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

#define PGC_C0_OFF 0x4
#define PGC_C0_INDEX_OFF 0x8
#define PGC_8B_ROUND_EN BIT(1)
#define PGC_EN BIT(0)
#define PGC_TBL_NUM 3
#define PGC_LUT_SWAP_OFF 0x1c


static void __setup_pa_hue(struct sde_hw_blk_reg_map *hw,
		const struct sde_pp_blk *blk, u32 hue, int loc)
{
	u32 base = blk->base;
	u32 offset = (loc == DSPP) ? PA_HUE_DSPP_OFF : PA_HUE_VIG_OFF;
	u32 op_hue_en = (loc == DSPP) ? DSPP_OP_PA_HUE_EN : VIG_OP_PA_HUE_EN;
	u32 op_pa_en = (loc == DSPP) ? DSPP_OP_PA_EN : VIG_OP_PA_EN;
	u32 disable_req;
	u32 opmode;

	opmode = SDE_REG_READ(hw, base);
	SDE_REG_WRITE(hw, base + offset, hue & PA_HUE_MASK);

	if (!hue) {
		opmode &= ~op_hue_en;
		disable_req = (loc == DSPP) ?
			PA_DSPP_DISABLE_REQUIRED(opmode) :
			PA_VIG_DISABLE_REQUIRED(opmode);
		if (disable_req)
			opmode &= ~op_pa_en;
	} else {
		opmode |= (op_hue_en | op_pa_en);
	}

	SDE_REG_WRITE(hw, base, opmode);
}

void sde_setup_pipe_pa_hue_v1_7(struct sde_hw_pipe *ctx, void *cfg)
{
	uint32_t hue = *((uint32_t *)cfg);

	__setup_pa_hue(&ctx->hw, &ctx->cap->sblk->hsic_blk, hue, SSPP);
}

static void __setup_pa_sat(struct sde_hw_blk_reg_map *hw,
		const struct sde_pp_blk *blk, u32 sat, int loc)
{
	u32 base = blk->base;
	u32 offset = (loc == DSPP) ? PA_SAT_DSPP_OFF : PA_SAT_VIG_OFF;
	u32 op_sat_en = (loc == DSPP) ? DSPP_OP_PA_SAT_EN : VIG_OP_PA_SAT_EN;
	u32 op_pa_en = (loc == DSPP) ? DSPP_OP_PA_EN : VIG_OP_PA_EN;
	u32 disable_req;
	u32 opmode;

	opmode = SDE_REG_READ(hw, base);
	SDE_REG_WRITE(hw, base + offset, sat & PA_SAT_MASK);

	if (!sat) {
		opmode &= ~op_sat_en;
		disable_req = (loc == DSPP) ?
			PA_DSPP_DISABLE_REQUIRED(opmode) :
			PA_VIG_DISABLE_REQUIRED(opmode);
		if (disable_req)
			opmode &= ~op_pa_en;
	} else {
		opmode |= (op_sat_en | op_pa_en);
	}

	SDE_REG_WRITE(hw, base, opmode);
}

void sde_setup_pipe_pa_sat_v1_7(struct sde_hw_pipe *ctx, void *cfg)
{
	uint32_t sat = *((uint32_t *)cfg);

	__setup_pa_sat(&ctx->hw, &ctx->cap->sblk->hsic_blk, sat, SSPP);
}

static void __setup_pa_val(struct sde_hw_blk_reg_map *hw,
		const struct sde_pp_blk *blk, u32 value, int loc)
{
	u32 base = blk->base;
	u32 offset = (loc == DSPP) ? PA_VAL_DSPP_OFF : PA_VAL_VIG_OFF;
	u32 op_val_en = (loc == DSPP) ? DSPP_OP_PA_VAL_EN : VIG_OP_PA_VAL_EN;
	u32 op_pa_en = (loc == DSPP) ? DSPP_OP_PA_EN : VIG_OP_PA_EN;
	u32 disable_req;
	u32 opmode;

	opmode = SDE_REG_READ(hw, base);
	SDE_REG_WRITE(hw, base + offset, value & PA_VAL_MASK);

	if (!value) {
		opmode &= ~op_val_en;
		disable_req = (loc == DSPP) ?
			PA_DSPP_DISABLE_REQUIRED(opmode) :
			PA_VIG_DISABLE_REQUIRED(opmode);
		if (disable_req)
			opmode &= ~op_pa_en;
	} else {
		opmode |= (op_val_en | op_pa_en);
	}

	SDE_REG_WRITE(hw, base, opmode);
}

void sde_setup_pipe_pa_val_v1_7(struct sde_hw_pipe *ctx, void *cfg)
{
	uint32_t value = *((uint32_t *)cfg);

	__setup_pa_val(&ctx->hw, &ctx->cap->sblk->hsic_blk, value, SSPP);
}

static void __setup_pa_cont(struct sde_hw_blk_reg_map *hw,
		const struct sde_pp_blk *blk, u32 contrast, int loc)
{
	u32 base = blk->base;
	u32 offset = (loc == DSPP) ? PA_CONT_DSPP_OFF : PA_CONT_VIG_OFF;
	u32 op_cont_en = (loc == DSPP) ?
		DSPP_OP_PA_CONT_EN : VIG_OP_PA_CONT_EN;
	u32 op_pa_en = (loc == DSPP) ? DSPP_OP_PA_EN : VIG_OP_PA_EN;
	u32 disable_req;
	u32 opmode;

	opmode = SDE_REG_READ(hw, base);
	SDE_REG_WRITE(hw, base + offset, contrast & PA_CONT_MASK);

	if (!contrast) {
		opmode &= ~op_cont_en;
		disable_req = (loc == DSPP) ?
			PA_DSPP_DISABLE_REQUIRED(opmode) :
			PA_VIG_DISABLE_REQUIRED(opmode);
		if (disable_req)
			opmode &= ~op_pa_en;
	} else {
		opmode |= (op_cont_en | op_pa_en);
	}

	SDE_REG_WRITE(hw, base, opmode);
}

void sde_setup_pipe_pa_cont_v1_7(struct sde_hw_pipe *ctx, void *cfg)
{
	uint32_t contrast = *((uint32_t *)cfg);

	__setup_pa_cont(&ctx->hw, &ctx->cap->sblk->hsic_blk, contrast, SSPP);
}

void sde_setup_dspp_pa_hsic_v17(struct sde_hw_dspp *ctx, void *cfg)
{
	struct sde_hw_cp_cfg *hw_cfg = cfg;
	struct drm_msm_pa_hsic *hsic_cfg;
	u32 hue = 0;
	u32 sat = 0;
	u32 val = 0;
	u32 cont = 0;

	if (!ctx || !cfg) {
		DRM_ERROR("invalid param ctx %pK cfg %pK\n", ctx, cfg);
		return;
	}

	if (hw_cfg->payload &&
		(hw_cfg->len != sizeof(struct drm_msm_pa_hsic))) {
		DRM_ERROR("invalid size of payload len %d exp %zd\n",
			hw_cfg->len, sizeof(struct drm_msm_pa_hsic));
		return;
	}

	if (!hw_cfg->payload) {
		DRM_DEBUG_DRIVER("disable pa hsic feature\n");
	} else {
		hsic_cfg = hw_cfg->payload;
		if (hsic_cfg->flags & PA_HSIC_HUE_ENABLE)
			hue = hsic_cfg->hue;
		if (hsic_cfg->flags & PA_HSIC_SAT_ENABLE)
			sat = hsic_cfg->saturation;
		if (hsic_cfg->flags & PA_HSIC_VAL_ENABLE)
			val = hsic_cfg->value;
		if (hsic_cfg->flags & PA_HSIC_CONT_ENABLE)
			cont = hsic_cfg->contrast;
	}

	__setup_pa_hue(&ctx->hw, &ctx->cap->sblk->hsic, hue, DSPP);
	__setup_pa_sat(&ctx->hw, &ctx->cap->sblk->hsic, sat, DSPP);
	__setup_pa_val(&ctx->hw, &ctx->cap->sblk->hsic, val, DSPP);
	__setup_pa_cont(&ctx->hw, &ctx->cap->sblk->hsic, cont, DSPP);
}

void sde_setup_dspp_sixzone_v17(struct sde_hw_dspp *ctx, void *cfg)
{
	struct sde_hw_cp_cfg *hw_cfg = cfg;
	struct drm_msm_sixzone *sixzone;
	u32 opcode = 0, local_opcode = 0;
	u32 reg = 0, hold = 0, local_hold = 0;
	u32 addr = 0;
	int i = 0;

	if (!ctx || !cfg) {
		DRM_ERROR("invalid param ctx %pK cfg %pK\n", ctx, cfg);
		return;
	}

	opcode = SDE_REG_READ(&ctx->hw, ctx->cap->sblk->hsic.base);

	if (!hw_cfg->payload) {
		DRM_DEBUG_DRIVER("disable sixzone feature\n");
		opcode &= ~(DSPP_OP_SZ_HUE_EN | DSPP_OP_SZ_SAT_EN |
			DSPP_OP_SZ_VAL_EN);
		if (PA_DSPP_DISABLE_REQUIRED(opcode))
			opcode &= ~DSPP_OP_PA_EN;
		SDE_REG_WRITE(&ctx->hw, ctx->cap->sblk->hsic.base, opcode);
		return;
	}

	if (hw_cfg->len != sizeof(struct drm_msm_sixzone)) {
		DRM_ERROR("invalid size of payload len %d exp %zd\n",
			hw_cfg->len, sizeof(struct drm_msm_sixzone));
		return;
	}

	sixzone = hw_cfg->payload;

	reg = BIT(26);
	SDE_REG_WRITE(&ctx->hw, ctx->cap->sblk->sixzone.base, reg);

	addr = ctx->cap->sblk->sixzone.base + DSPP_SZ_ADJ_CURVE_P1_OFF;
	for (i = 0; i < SIXZONE_LUT_SIZE; i++) {
		SDE_REG_WRITE(&ctx->hw, addr, sixzone->curve[i].p1);
		SDE_REG_WRITE(&ctx->hw, (addr - 4), sixzone->curve[i].p0);
	}

	addr = ctx->cap->sblk->sixzone.base + DSPP_SZ_THRESHOLDS_OFF;
	SDE_REG_WRITE(&ctx->hw, addr, sixzone->threshold);
	SDE_REG_WRITE(&ctx->hw, (addr + 4), sixzone->adjust_p0);
	SDE_REG_WRITE(&ctx->hw, (addr + 8), sixzone->adjust_p1);

	hold = SDE_REG_READ(&ctx->hw,
		(ctx->cap->sblk->hsic.base + DSPP_PA_PWL_HOLD_OFF));
	local_hold = ((sixzone->sat_hold & REG_MASK(2)) << 12);
	local_hold |= ((sixzone->val_hold & REG_MASK(2)) << 14);
	hold &= ~REG_MASK_SHIFT(4, 12);
	hold |= local_hold;
	SDE_REG_WRITE(&ctx->hw,
		(ctx->cap->sblk->hsic.base + DSPP_PA_PWL_HOLD_OFF),
		hold);

	if (sixzone->flags & SIXZONE_HUE_ENABLE)
		local_opcode |= DSPP_OP_SZ_HUE_EN;
	if (sixzone->flags & SIXZONE_SAT_ENABLE)
		local_opcode |= DSPP_OP_SZ_SAT_EN;
	if (sixzone->flags & SIXZONE_VAL_ENABLE)
		local_opcode |= DSPP_OP_SZ_VAL_EN;

	if (local_opcode)
		local_opcode |= DSPP_OP_PA_EN;

	opcode &= ~REG_MASK_SHIFT(3, 29);
	opcode |= local_opcode;
	SDE_REG_WRITE(&ctx->hw, ctx->cap->sblk->hsic.base, opcode);
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

static void __setup_dspp_memcol(struct sde_hw_dspp *ctx,
		enum sde_memcolor_type type,
		struct drm_msm_memcol *memcolor)
{
	u32 addr = 0, offset = 0, idx = 0;
	u32 hold = 0, local_hold = 0, hold_shift = 0;

	switch (type) {
	case MEMCOLOR_SKIN:
		idx = 0;
		break;
	case MEMCOLOR_SKY:
		idx = 1;
		break;
	case MEMCOLOR_FOLIAGE:
		idx = 2;
		break;
	default:
		DRM_ERROR("Invalid memory color type %d\n", type);
		return;
	}

	offset = DSPP_MEMCOL_PWL0_OFF + (idx * DSPP_MEMCOL_SIZE0);
	addr = ctx->cap->sblk->memcolor.base + offset;
	hold_shift = idx * DSPP_MEMCOL_HOLD_SIZE;

	SDE_REG_WRITE(&ctx->hw, addr, memcolor->color_adjust_p0);
	addr += 4;
	SDE_REG_WRITE(&ctx->hw, addr, memcolor->color_adjust_p1);
	addr += 4;
	SDE_REG_WRITE(&ctx->hw, addr, memcolor->hue_region);
	addr += 4;
	SDE_REG_WRITE(&ctx->hw, addr, memcolor->sat_region);
	addr += 4;
	SDE_REG_WRITE(&ctx->hw, addr, memcolor->val_region);

	offset = DSPP_MEMCOL_PWL2_OFF + (idx * DSPP_MEMCOL_SIZE1);
	addr = ctx->cap->sblk->memcolor.base + offset;

	SDE_REG_WRITE(&ctx->hw, addr, memcolor->color_adjust_p2);
	addr += 4;
	SDE_REG_WRITE(&ctx->hw, addr, memcolor->blend_gain);

	addr = ctx->cap->sblk->hsic.base + DSPP_PA_PWL_HOLD_OFF;
	hold = SDE_REG_READ(&ctx->hw, addr);
	local_hold = ((memcolor->sat_hold & REG_MASK(2)) << hold_shift);
	local_hold |=
		((memcolor->val_hold & REG_MASK(2)) << (hold_shift + 2));
	hold &= ~REG_MASK_SHIFT(4, hold_shift);
	hold |= local_hold;
	SDE_REG_WRITE(&ctx->hw, addr, hold);
}

void sde_setup_dspp_memcol_skin_v17(struct sde_hw_dspp *ctx, void *cfg)
{
	struct sde_hw_cp_cfg *hw_cfg = cfg;
	struct drm_msm_memcol *memcolor;
	u32 opcode = 0;

	if (!ctx || !cfg) {
		DRM_ERROR("invalid param ctx %pK cfg %pK\n", ctx, cfg);
		return;
	}

	opcode = SDE_REG_READ(&ctx->hw, ctx->cap->sblk->hsic.base);

	if (!hw_cfg->payload) {
		DRM_DEBUG_DRIVER("disable memcolor skin feature\n");
		opcode &= ~(DSPP_OP_PA_SKIN_EN);
		if (PA_DSPP_DISABLE_REQUIRED(opcode))
			opcode &= ~DSPP_OP_PA_EN;
		SDE_REG_WRITE(&ctx->hw, ctx->cap->sblk->hsic.base, opcode);
		return;
	}

	if (hw_cfg->len != sizeof(struct drm_msm_memcol)) {
		DRM_ERROR("invalid size of payload len %d exp %zd\n",
			hw_cfg->len, sizeof(struct drm_msm_memcol));
		return;
	}

	memcolor = hw_cfg->payload;

	__setup_dspp_memcol(ctx, MEMCOLOR_SKIN, memcolor);

	opcode |= (DSPP_OP_PA_SKIN_EN | DSPP_OP_PA_EN);
	SDE_REG_WRITE(&ctx->hw, ctx->cap->sblk->hsic.base, opcode);
}

void sde_setup_dspp_memcol_sky_v17(struct sde_hw_dspp *ctx, void *cfg)
{
	struct sde_hw_cp_cfg *hw_cfg = cfg;
	struct drm_msm_memcol *memcolor;
	u32 opcode = 0;

	if (!ctx || !cfg) {
		DRM_ERROR("invalid param ctx %pK cfg %pK\n", ctx, cfg);
		return;
	}

	opcode = SDE_REG_READ(&ctx->hw, ctx->cap->sblk->hsic.base);

	if (!hw_cfg->payload) {
		DRM_DEBUG_DRIVER("disable memcolor sky feature\n");
		opcode &= ~(DSPP_OP_PA_SKY_EN);
		if (PA_DSPP_DISABLE_REQUIRED(opcode))
			opcode &= ~DSPP_OP_PA_EN;
		SDE_REG_WRITE(&ctx->hw, ctx->cap->sblk->hsic.base, opcode);
		return;
	}

	if (hw_cfg->len != sizeof(struct drm_msm_memcol)) {
		DRM_ERROR("invalid size of payload len %d exp %zd\n",
			hw_cfg->len, sizeof(struct drm_msm_memcol));
		return;
	}

	memcolor = hw_cfg->payload;

	__setup_dspp_memcol(ctx, MEMCOLOR_SKY, memcolor);

	opcode |= (DSPP_OP_PA_SKY_EN | DSPP_OP_PA_EN);
	SDE_REG_WRITE(&ctx->hw, ctx->cap->sblk->hsic.base, opcode);
}

void sde_setup_dspp_memcol_foliage_v17(struct sde_hw_dspp *ctx, void *cfg)
{
	struct sde_hw_cp_cfg *hw_cfg = cfg;
	struct drm_msm_memcol *memcolor;
	u32 opcode = 0;

	if (!ctx || !cfg) {
		DRM_ERROR("invalid param ctx %pK cfg %pK\n", ctx, cfg);
		return;
	}

	opcode = SDE_REG_READ(&ctx->hw, ctx->cap->sblk->hsic.base);

	if (!hw_cfg->payload) {
		DRM_DEBUG_DRIVER("disable memcolor foliage feature\n");
		opcode &= ~(DSPP_OP_PA_FOL_EN);
		if (PA_DSPP_DISABLE_REQUIRED(opcode))
			opcode &= ~DSPP_OP_PA_EN;
		SDE_REG_WRITE(&ctx->hw, ctx->cap->sblk->hsic.base, opcode);
		return;
	}

	if (hw_cfg->len != sizeof(struct drm_msm_memcol)) {
		DRM_ERROR("invalid size of payload len %d exp %zd\n",
			hw_cfg->len, sizeof(struct drm_msm_memcol));
		return;
	}

	memcolor = hw_cfg->payload;

	__setup_dspp_memcol(ctx, MEMCOLOR_FOLIAGE, memcolor);

	opcode |= (DSPP_OP_PA_FOL_EN | DSPP_OP_PA_EN);
	SDE_REG_WRITE(&ctx->hw, ctx->cap->sblk->hsic.base, opcode);
}

void sde_setup_dspp_memcol_prot_v17(struct sde_hw_dspp *ctx, void *cfg)
{
	struct sde_hw_cp_cfg *hw_cfg = cfg;
	struct drm_msm_memcol *memcolor;
	u32 opcode = 0, local_opcode = 0;

	if (!ctx || !cfg) {
		DRM_ERROR("invalid param ctx %pK cfg %pK\n", ctx, cfg);
		return;
	}

	opcode = SDE_REG_READ(&ctx->hw, ctx->cap->sblk->hsic.base);

	if (!hw_cfg->payload) {
		DRM_DEBUG_DRIVER("disable memcolor prot feature\n");
		opcode &= ~(DSPP_MEMCOL_PROT_MASK);
		if (PA_DSPP_DISABLE_REQUIRED(opcode))
			opcode &= ~DSPP_OP_PA_EN;
		SDE_REG_WRITE(&ctx->hw, ctx->cap->sblk->hsic.base, opcode);
		return;
	}

	if (hw_cfg->len != sizeof(struct drm_msm_memcol)) {
		DRM_ERROR("invalid size of payload len %d exp %zd\n",
			hw_cfg->len, sizeof(struct drm_msm_memcol));
		return;
	}

	memcolor = hw_cfg->payload;

	if (memcolor->prot_flags) {
		if (memcolor->prot_flags & MEMCOL_PROT_HUE)
			local_opcode |= DSPP_MEMCOL_PROT_HUE_EN;
		if (memcolor->prot_flags & MEMCOL_PROT_SAT)
			local_opcode |= DSPP_MEMCOL_PROT_SAT_EN;
		if (memcolor->prot_flags & MEMCOL_PROT_VAL)
			local_opcode |= DSPP_MEMCOL_PROT_VAL_EN;
		if (memcolor->prot_flags & MEMCOL_PROT_CONT)
			local_opcode |= DSPP_MEMCOL_PROT_CONT_EN;
		if (memcolor->prot_flags & MEMCOL_PROT_SIXZONE)
			local_opcode |= DSPP_MEMCOL_PROT_SIXZONE_EN;
		if (memcolor->prot_flags & MEMCOL_PROT_BLEND)
			local_opcode |= DSPP_MEMCOL_PROT_BLEND_EN;
	}

	if (local_opcode) {
		local_opcode |= DSPP_OP_PA_EN;
		opcode &= ~(DSPP_MEMCOL_PROT_MASK);
		opcode |= local_opcode;
		SDE_REG_WRITE(&ctx->hw, ctx->cap->sblk->hsic.base, opcode);
	}
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

void sde_setup_dspp_pa_vlut_v1_8(struct sde_hw_dspp *ctx, void *cfg)
{
	struct drm_msm_pa_vlut *payload = NULL;
	struct sde_hw_cp_cfg *hw_cfg = cfg;
	struct sde_hw_ctl *ctl = NULL;
	u32 vlut_base, pa_hist_base;
	u32 ctrl_off, swap_off;
	u32 tmp = 0;
	int i = 0, j = 0;
	u32 flush_mask = 0;

	if (!ctx) {
		DRM_ERROR("invalid input parameter NULL ctx\n");
		return;
	}

	if (!hw_cfg || (hw_cfg->payload && hw_cfg->len !=
			sizeof(struct drm_msm_pa_vlut))) {
		DRM_ERROR("hw %pK payload %pK payloadsize %d exp size %zd\n",
			  hw_cfg, ((hw_cfg) ? hw_cfg->payload : NULL),
			  ((hw_cfg) ? hw_cfg->len : 0),
			  sizeof(struct drm_msm_pa_vlut));
		return;
	}

	ctl = hw_cfg->ctl;
	vlut_base = ctx->cap->sblk->vlut.base;
	pa_hist_base = ctx->cap->sblk->hist.base;
	ctrl_off = pa_hist_base + PA_LUTV_DSPP_CTRL_OFF;
	swap_off = pa_hist_base + PA_LUTV_DSPP_SWAP_OFF;

	if (!hw_cfg->payload) {
		DRM_DEBUG_DRIVER("Disable vlut feature\n");
		SDE_REG_WRITE(&ctx->hw, ctrl_off, 0);
		goto exit;
	}

	payload = hw_cfg->payload;
	DRM_DEBUG_DRIVER("Enable vlut feature flags %llx\n", payload->flags);
	for (i = 0, j = 0; i < ARRAY_SIZE(payload->val); i += 2, j += 4) {
		tmp = (payload->val[i] & REG_MASK(10)) |
			((payload->val[i + 1] & REG_MASK(10)) << 16);
		SDE_REG_WRITE(&ctx->hw, (vlut_base + j), tmp);
	}
	SDE_REG_WRITE(&ctx->hw, ctrl_off, 1);
	SDE_REG_WRITE(&ctx->hw, swap_off, 1);

exit:
	/* update flush bit */
	if (ctl && ctl->ops.get_bitmask_dspp_pavlut) {
		ctl->ops.get_bitmask_dspp_pavlut(ctl, &flush_mask, ctx->idx);
		if (ctl->ops.update_pending_flush)
			ctl->ops.update_pending_flush(ctl, flush_mask);
	}
}

void sde_setup_dspp_gc_v1_7(struct sde_hw_dspp *ctx, void *cfg)
{
	struct drm_msm_pgc_lut *payload = NULL;
	struct sde_hw_cp_cfg *hw_cfg = cfg;
	u32 c0_off, c1_off, c2_off, i;

	if (!hw_cfg || (hw_cfg->payload && hw_cfg->len !=
			sizeof(struct drm_msm_pgc_lut))) {
		DRM_ERROR("hw %pK payload %pK payloadsize %d exp size %zd\n",
			  hw_cfg, ((hw_cfg) ? hw_cfg->payload : NULL),
			  ((hw_cfg) ? hw_cfg->len : 0),
			  sizeof(struct drm_msm_pgc_lut));
		return;
	}

	if (!hw_cfg->payload) {
		DRM_DEBUG_DRIVER("Disable pgc feature\n");
		SDE_REG_WRITE(&ctx->hw, ctx->cap->sblk->gc.base, 0);
		return;
	}
	payload = hw_cfg->payload;

	/* Initialize index offsets */
	c0_off = ctx->cap->sblk->gc.base + PGC_C0_INDEX_OFF;
	c1_off = c0_off + (sizeof(u32) * 2);
	c2_off = c1_off + (sizeof(u32) * 2);
	SDE_REG_WRITE(&ctx->hw, c0_off, 0);
	SDE_REG_WRITE(&ctx->hw, c1_off, 0);
	SDE_REG_WRITE(&ctx->hw, c2_off, 0);

	/* Initialize table offsets */
	c0_off = ctx->cap->sblk->gc.base + PGC_C0_OFF;
	c1_off = c0_off + (sizeof(u32) * 2);
	c2_off = c1_off + (sizeof(u32) * 2);

	for (i = 0; i < PGC_TBL_LEN; i++) {
		SDE_REG_WRITE(&ctx->hw, c0_off, payload->c0[i]);
		SDE_REG_WRITE(&ctx->hw, c1_off, payload->c1[i]);
		SDE_REG_WRITE(&ctx->hw, c2_off, payload->c2[i]);
	}
	SDE_REG_WRITE(&ctx->hw, ctx->cap->sblk->gc.base + PGC_LUT_SWAP_OFF,
			BIT(0));
	i = BIT(0) | ((payload->flags & PGC_8B_ROUND) ? BIT(1) : 0);
	SDE_REG_WRITE(&ctx->hw, ctx->cap->sblk->gc.base, i);
}

void sde_setup_dspp_hist_v1_7(struct sde_hw_dspp *ctx, void *cfg)
{
	u32 base, offset;
	u32 op_mode;
	bool feature_enabled;

	if (!ctx || !cfg) {
		DRM_ERROR("invalid parameters ctx %pK cfg %pK", ctx, cfg);
		return;
	}

	feature_enabled = *(bool *)cfg;
	base = ctx->cap->sblk->hist.base;
	offset = base + PA_HIST_CTRL_DSPP_OFF;

	op_mode = SDE_REG_READ(&ctx->hw, base);
	if (!feature_enabled) {
		op_mode &= ~DSPP_OP_PA_HIST_EN;
		if (PA_DSPP_DISABLE_REQUIRED(op_mode))
			op_mode &= ~DSPP_OP_PA_EN;
	} else {
		op_mode |= DSPP_OP_PA_HIST_EN | DSPP_OP_PA_EN;
	}

	SDE_REG_WRITE(&ctx->hw, offset, 0);
	SDE_REG_WRITE(&ctx->hw, base, op_mode);
}

void sde_read_dspp_hist_v1_7(struct sde_hw_dspp *ctx, void *cfg)
{
	struct drm_msm_hist *hist_data;
	u32 offset, offset_ctl;
	u32 i;

	if (!ctx || !cfg) {
		DRM_ERROR("invalid parameters ctx %pK cfg %pK", ctx, cfg);
		return;
	}

	hist_data = (struct drm_msm_hist *)cfg;
	offset = ctx->cap->sblk->hist.base + PA_HIST_DATA_DSPP_OFF;
	offset_ctl = ctx->cap->sblk->hist.base + PA_HIST_CTRL_DSPP_OFF;

	for (i = 0; i < HIST_V_SIZE; i++)
		hist_data->data[i] = SDE_REG_READ(&ctx->hw, offset + i * 4) &
					REG_MASK(24);

	/* unlock hist buffer */
	SDE_REG_WRITE(&ctx->hw, offset_ctl, 0);
}

void sde_lock_dspp_hist_v1_7(struct sde_hw_dspp *ctx, void *cfg)
{
	u32 offset_ctl;

	if (!ctx) {
		DRM_ERROR("invalid parameters ctx %pK", ctx);
		return;
	}

	offset_ctl = ctx->cap->sblk->hist.base + PA_HIST_CTRL_DSPP_OFF;

	/* lock hist buffer */
	SDE_REG_WRITE(&ctx->hw, offset_ctl, 1);
}
