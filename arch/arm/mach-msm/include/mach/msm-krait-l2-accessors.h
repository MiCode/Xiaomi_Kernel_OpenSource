#ifndef __ASM_ARCH_MSM_MSM_KRAIT_L2_ACCESSORS_H
#define __ASM_ARCH_MSM_MSM_KRAIT_L2_ACCESSORS_H

/*
 * Copyright (c) 2011, Code Aurora Forum. All rights reserved.
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
extern void set_l2_indirect_reg(u32 reg_addr, u32 val);
extern u32 get_l2_indirect_reg(u32 reg_addr);
extern u32 set_get_l2_indirect_reg(u32 reg_addr, u32 val);

#endif
