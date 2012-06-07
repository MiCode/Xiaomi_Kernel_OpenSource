/* Copyright (c) 2010, Code Aurora Forum. All rights reserved.
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
#ifndef __ARCH_ARM_MACH_MSM_GPIOMUX_8X60_H
#define __ARCH_ARM_MACH_MSM_GPIOMUX_8X60_H

void __init msm8x60_init_gpiomux(struct msm_gpiomux_configs *cfgs);

extern struct msm_gpiomux_configs msm8x60_surf_ffa_gpiomux_cfgs[] __initdata;
extern struct msm_gpiomux_configs msm8x60_fluid_gpiomux_cfgs[] __initdata;
extern struct msm_gpiomux_configs msm8x60_charm_gpiomux_cfgs[] __initdata;
extern struct msm_gpiomux_configs msm8x60_dragon_gpiomux_cfgs[] __initdata;

#endif
