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

#ifndef _SDE_HW_MDP_UTIL_H
#define _SDE_HW_MDP_UTIL_H

#include <linux/io.h>
#include <linux/slab.h>
#include "sde_hw_mdss.h"

/*
 * This is the common struct maintained by each sub block
 * for mapping the register offsets in this block to the
 * absoulute IO address
 * @base_off:     mdp register mapped offset
 * @blk_off:      pipe offset relative to mdss offset
 * @length        length of register block offset
 * @hwversion     mdss hw version number
 */
struct sde_hw_blk_reg_map {
	void __iomem *base_off;
	u32 blk_off;
	u32 length;
	u32 hwversion;
};

void sde_hw_reg_write(void __iomem *base, u32 blk_offset, u32 reg, u32 val);

u32 sde_hw_reg_read(void __iomem *base, u32 blk_offset, u32 reg);

static inline void SDE_REG_WRITE(struct sde_hw_blk_reg_map *c, u32 reg_off,
		u32 val)
{
	sde_hw_reg_write(c->base_off, c->blk_off, reg_off, val);
}

static inline int SDE_REG_READ(struct sde_hw_blk_reg_map *c, u32 reg_off)
{
	return sde_hw_reg_read(c->base_off, c->blk_off, reg_off);
}

void sde_hw_csc_setup(struct sde_hw_blk_reg_map  *c,
		u32 csc_reg_off,
		struct sde_csc_cfg *data);

#endif /* _SDE_HW_MDP_UTIL_H */

