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

#include "sde_hw_mdp_util.h"

void sde_hw_reg_write(void  __iomem *base, u32 blk_off, u32 reg_off, u32 val)
{
	writel_relaxed(val, base + blk_off + reg_off);
}

u32 sde_hw_reg_read(void  __iomem *base, u32 blk_off, u32 reg_off)
{
	return readl_relaxed(base + blk_off + reg_off);
}

void sde_hw_csc_setup(struct sde_hw_blk_reg_map  *c,
		u32 csc_reg_off,
		struct sde_csc_cfg *data)
{
	u32 val;

	/* Matrix coeff */
	val = (data->csc_mv[0] & 0x1FF) |
		((data->csc_mv[1] & 0x1FF) << 16);
	SDE_REG_WRITE(c, csc_reg_off,  val);
	val = (data->csc_mv[2] & 0x1FF) |
		((data->csc_mv[3] & 0x1FF) << 16);
	SDE_REG_WRITE(c, csc_reg_off + 0x4, val);
	val = (data->csc_mv[4] & 0x1FF) |
		((data->csc_mv[5] & 0x1FF) >> 16);
	SDE_REG_WRITE(c, csc_reg_off + 0x8, val);
	val = (data->csc_mv[6] & 0x1FF) |
		((data->csc_mv[7] & 0x1FF) << 16);
	SDE_REG_WRITE(c, csc_reg_off + 0xc, val);
	val = data->csc_mv[8] & 0x1FF;
	SDE_REG_WRITE(c, csc_reg_off + 0x10, val);

	/* Pre clamp */
	val = (data->csc_pre_lv[0] << 8) | data->csc_pre_lv[1];
	SDE_REG_WRITE(c, csc_reg_off + 0x14,  val);
	val = (data->csc_pre_lv[2] << 8) | data->csc_pre_lv[3];
	SDE_REG_WRITE(c, csc_reg_off  + 0x18, val);
	val = (data->csc_pre_lv[4] << 8) | data->csc_pre_lv[5];
	SDE_REG_WRITE(c, csc_reg_off  + 0x1c, val);

	/* Post clamp */
	val = (data->csc_post_lv[0] << 8) | data->csc_post_lv[1];
	SDE_REG_WRITE(c, csc_reg_off + 0x20,  val);
	val = (data->csc_post_lv[2] << 8) | data->csc_post_lv[3];
	SDE_REG_WRITE(c, csc_reg_off  + 0x24, val);
	val = (data->csc_post_lv[4] << 8) | data->csc_post_lv[5];
	SDE_REG_WRITE(c, csc_reg_off  + 0x28, val);

	/* Pre-Bias */
	SDE_REG_WRITE(c, csc_reg_off + 0x2c,  data->csc_pre_bv[0]);
	SDE_REG_WRITE(c, csc_reg_off + 0x30, data->csc_pre_bv[1]);
	SDE_REG_WRITE(c, csc_reg_off + 0x34, data->csc_pre_bv[2]);

	/* Post-Bias */
	SDE_REG_WRITE(c, csc_reg_off + 0x38,  data->csc_post_bv[0]);
	SDE_REG_WRITE(c, csc_reg_off + 0x3c, data->csc_post_bv[1]);
	SDE_REG_WRITE(c, csc_reg_off + 0x40, data->csc_post_bv[2]);
}

