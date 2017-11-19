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

#ifndef _SDE_HW_UTIL_H
#define _SDE_HW_UTIL_H

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
 * @xin_id        xin id
 * @hwversion     mdss hw version number
 */
struct sde_hw_blk_reg_map {
	void __iomem *base_off;
	u32 blk_off;
	u32 length;
	u32 xin_id;
	u32 hwversion;
	u32 log_mask;
};

u32 *sde_hw_util_get_log_mask_ptr(void);

void sde_reg_write(struct sde_hw_blk_reg_map *c,
		u32 reg_off,
		u32 val,
		const char *name);
int sde_reg_read(struct sde_hw_blk_reg_map *c, u32 reg_off);

#define SDE_REG_WRITE(c, off, val) sde_reg_write(c, off, val, #off)
#define SDE_REG_READ(c, off) sde_reg_read(c, off)

void *sde_hw_util_get_dir(void);

void sde_hw_csc_setup(struct sde_hw_blk_reg_map  *c,
		u32 csc_reg_off,
		struct sde_csc_cfg *data, bool csc10);

#endif /* _SDE_HW_UTIL_H */

