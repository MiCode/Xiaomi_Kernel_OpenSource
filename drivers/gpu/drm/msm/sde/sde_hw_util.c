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
#define pr_fmt(fmt)	"[drm:%s:%d] " fmt, __func__, __LINE__
#include "msm_drv.h"
#include "sde_kms.h"
#include "sde_hw_mdss.h"
#include "sde_hw_util.h"

/* using a file static variables for debugfs access */
static u32 sde_hw_util_log_mask = SDE_DBG_MASK_NONE;

void sde_reg_write(struct sde_hw_blk_reg_map *c,
		u32 reg_off,
		u32 val,
		const char *name)
{
	/* don't need to mutex protect this */
	if (c->log_mask & sde_hw_util_log_mask)
		SDE_DEBUG_DRIVER("[%s:0x%X] <= 0x%X\n",
				name, c->blk_off + reg_off, val);
	writel_relaxed(val, c->base_off + c->blk_off + reg_off);
}

int sde_reg_read(struct sde_hw_blk_reg_map *c, u32 reg_off)
{
	return readl_relaxed(c->base_off + c->blk_off + reg_off);
}

u32 *sde_hw_util_get_log_mask_ptr(void)
{
	return &sde_hw_util_log_mask;
}

void sde_hw_csc_setup(struct sde_hw_blk_reg_map *c,
		u32 csc_reg_off,
		struct sde_csc_cfg *data)
{
	static const u32 matrix_shift = 7;
	u32 val;

	/* matrix coeff - convert S15.16 to S4.9 */
	val = ((data->csc_mv[0] >> matrix_shift) & 0x1FFF) |
		(((data->csc_mv[1] >> matrix_shift) & 0x1FFF) << 16);
	SDE_REG_WRITE(c, csc_reg_off, val);
	val = ((data->csc_mv[2] >> matrix_shift) & 0x1FFF) |
		(((data->csc_mv[3] >> matrix_shift) & 0x1FFF) << 16);
	SDE_REG_WRITE(c, csc_reg_off + 0x4, val);
	val = ((data->csc_mv[4] >> matrix_shift) & 0x1FFF) |
		(((data->csc_mv[5] >> matrix_shift) & 0x1FFF) << 16);
	SDE_REG_WRITE(c, csc_reg_off + 0x8, val);
	val = ((data->csc_mv[6] >> matrix_shift) & 0x1FFF) |
		(((data->csc_mv[7] >> matrix_shift) & 0x1FFF) << 16);
	SDE_REG_WRITE(c, csc_reg_off + 0xc, val);
	val = (data->csc_mv[8] >> matrix_shift) & 0x1FFF;
	SDE_REG_WRITE(c, csc_reg_off + 0x10, val);

	/* Pre clamp */
	val = (data->csc_pre_lv[0] << 8) | data->csc_pre_lv[1];
	SDE_REG_WRITE(c, csc_reg_off + 0x14, val);
	val = (data->csc_pre_lv[2] << 8) | data->csc_pre_lv[3];
	SDE_REG_WRITE(c, csc_reg_off + 0x18, val);
	val = (data->csc_pre_lv[4] << 8) | data->csc_pre_lv[5];
	SDE_REG_WRITE(c, csc_reg_off + 0x1c, val);

	/* Post clamp */
	val = (data->csc_post_lv[0] << 8) | data->csc_post_lv[1];
	SDE_REG_WRITE(c, csc_reg_off + 0x20, val);
	val = (data->csc_post_lv[2] << 8) | data->csc_post_lv[3];
	SDE_REG_WRITE(c, csc_reg_off + 0x24, val);
	val = (data->csc_post_lv[4] << 8) | data->csc_post_lv[5];
	SDE_REG_WRITE(c, csc_reg_off + 0x28, val);

	/* Pre-Bias */
	SDE_REG_WRITE(c, csc_reg_off + 0x2c, data->csc_pre_bv[0]);
	SDE_REG_WRITE(c, csc_reg_off + 0x30, data->csc_pre_bv[1]);
	SDE_REG_WRITE(c, csc_reg_off + 0x34, data->csc_pre_bv[2]);

	/* Post-Bias */
	SDE_REG_WRITE(c, csc_reg_off + 0x38, data->csc_post_bv[0]);
	SDE_REG_WRITE(c, csc_reg_off + 0x3c, data->csc_post_bv[1]);
	SDE_REG_WRITE(c, csc_reg_off + 0x40, data->csc_post_bv[2]);
}

