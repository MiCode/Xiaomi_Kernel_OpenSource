/*
 * arch/arm/mach-tegra/tegra_ism.h
 *
 * Copyright (c) 2013-2014, NVIDIA CORPORATION. All rights reserved.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef _MACH_TEGRA_ISM_H_
#define _MACH_TEGRA_ISM_H_

/* These functions return the RO frequency in MHz given the configuration */
u32 read_cpu0_ism(u32 mode, u32 duration, u32 div, u32 sel);
u32 read_gpu_ism(u32 mode, u32 duration, u32 div, u32 sel);
u32 read_ism(u32 mode, u32 duration, u32 div, u32 sel, u32 offset,
	     u8 chiplet, u16 len, u8 instr_id);
#endif /* _MACH_TEGRA_ISM_H_ */
