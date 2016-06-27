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

#include "sde_hw_color_processing_v1_7.h"

#define PA_HUE_OFF		0x110
#define PA_HUE_MASK		0xFFF
#define PA_SAT_OFF		0x114
#define PA_SAT_MASK		0xFFFF
#define PA_VAL_OFF		0x118
#define PA_VAL_MASK		0xFF
#define PA_CONT_OFF		0x11C
#define PA_CONT_MASK		0xFF

#define VIG_OP_PA_EN		BIT(4)
#define VIG_OP_PA_SKIN_EN	BIT(5)
#define VIG_OP_PA_FOL_EN	BIT(6)
#define VIG_OP_PA_SKY_EN	BIT(7)
#define VIG_OP_PA_HUE_EN	BIT(25)
#define VIG_OP_PA_SAT_EN	BIT(26)
#define VIG_OP_PA_VAL_EN	BIT(27)
#define VIG_OP_PA_CONT_EN	BIT(28)

#define PA_DISABLE_REQUIRED(x)	!((x) & \
				(VIG_OP_PA_SKIN_EN | VIG_OP_PA_SKY_EN | \
				VIG_OP_PA_FOL_EN | VIG_OP_PA_HUE_EN | \
				VIG_OP_PA_SAT_EN | VIG_OP_PA_VAL_EN | \
				VIG_OP_PA_CONT_EN))

void sde_setup_pipe_pa_hue_v1_7(struct sde_hw_pipe *ctx, void *cfg)
{
	uint32_t hue = *((uint32_t *)cfg);
	u32 base = ctx->cap->sblk->hsic_blk.base;
	u32 opmode = 0;

	SDE_REG_WRITE(&ctx->hw, base + PA_HUE_OFF, hue & PA_HUE_MASK);

	opmode = SDE_REG_READ(&ctx->hw, base);

	if (!hue) {
		opmode &= ~VIG_OP_PA_HUE_EN;
		if (PA_DISABLE_REQUIRED(opmode))
			opmode &= ~VIG_OP_PA_EN;
	} else {
		opmode |= VIG_OP_PA_HUE_EN | VIG_OP_PA_EN;
	}

	SDE_REG_WRITE(&ctx->hw, base, opmode);
}

void sde_setup_pipe_pa_sat_v1_7(struct sde_hw_pipe *ctx, void *cfg)
{
	uint32_t sat = *((uint32_t *)cfg);
	u32 base = ctx->cap->sblk->hsic_blk.base;
	u32 opmode = 0;

	SDE_REG_WRITE(&ctx->hw, base + PA_SAT_OFF, sat & PA_SAT_MASK);

	opmode = SDE_REG_READ(&ctx->hw, base);

	if (!sat) {
		opmode &= ~VIG_OP_PA_SAT_EN;
		if (PA_DISABLE_REQUIRED(opmode))
			opmode &= ~VIG_OP_PA_EN;
	} else {
		opmode |= VIG_OP_PA_SAT_EN | VIG_OP_PA_EN;
	}

	SDE_REG_WRITE(&ctx->hw, base, opmode);
}

void sde_setup_pipe_pa_val_v1_7(struct sde_hw_pipe *ctx, void *cfg)
{
	uint32_t value = *((uint32_t *)cfg);
	u32 base = ctx->cap->sblk->hsic_blk.base;
	u32 opmode = 0;

	SDE_REG_WRITE(&ctx->hw, base + PA_VAL_OFF, value & PA_VAL_MASK);

	opmode = SDE_REG_READ(&ctx->hw, base);

	if (!value) {
		opmode &= ~VIG_OP_PA_VAL_EN;
		if (PA_DISABLE_REQUIRED(opmode))
			opmode &= ~VIG_OP_PA_EN;
	} else {
		opmode |= VIG_OP_PA_VAL_EN | VIG_OP_PA_EN;
	}

	SDE_REG_WRITE(&ctx->hw, base, opmode);
}

void sde_setup_pipe_pa_cont_v1_7(struct sde_hw_pipe *ctx, void *cfg)
{
	uint32_t contrast = *((uint32_t *)cfg);
	u32 base = ctx->cap->sblk->hsic_blk.base;
	u32 opmode = 0;

	SDE_REG_WRITE(&ctx->hw, base + PA_CONT_OFF, contrast & PA_CONT_MASK);

	opmode = SDE_REG_READ(&ctx->hw, base);

	if (!contrast) {
		opmode &= ~VIG_OP_PA_CONT_EN;
		if (PA_DISABLE_REQUIRED(opmode))
			opmode &= ~VIG_OP_PA_EN;
	} else {
		opmode |= VIG_OP_PA_CONT_EN | VIG_OP_PA_EN;
	}

	SDE_REG_WRITE(&ctx->hw, base, opmode);
}
