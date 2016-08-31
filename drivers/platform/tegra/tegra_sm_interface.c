/*
 * Trusted Little Kernel secure monitor interface
 *
 * Copyright (c) 2014, NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/mutex.h>

DEFINE_MUTEX(tegra_smc_lock);

static u32 __extended_smc(u32 *regs)
{
	register uint32_t r0 asm("r0") = (u32)regs;

	/* allows MAX_EXT_SMC_ARGS (r0-r11) to be passed in registers */
	asm volatile (
		__asmeq("%0", "r0")
		"stmfd	sp!, {r4-r12}	@ save reg state\n"
		"mov	r12, r0		@ reg ptr to r12\n"
		"ldmia	r12, {r0-r11}	@ load arg regs\n"
#ifdef REQUIRES_SEC
		".arch_extension sec\n"
#endif
		"smc	#0		@ switch to secure world\n"
		"ldmfd	sp!, {r4-r12}	@ restore saved regs\n"
		: "=r" (r0)
		: "r" (r0)
	);

	return r0;
}

u32 tegra_sm_extended(u32 *regs)
{
	return __extended_smc(regs);
}

static u32 __generic_smc(u32 arg0, u32 arg1, u32 arg2)
{
	register uint32_t r0 asm("r0") = arg0;
	register uint32_t r1 asm("r1") = arg1;
	register uint32_t r2 asm("r2") = arg2;

	asm volatile(
		__asmeq("%0", "r0")
		__asmeq("%1", "r0")
		__asmeq("%2", "r1")
		__asmeq("%3", "r2")
#ifdef REQUIRES_SEC
		".arch_extension sec\n"
#endif
		"smc	#0		@ switch to secure world\n"
		: "=r" (r0)
		: "r" (r0), "r" (r1), "r" (r2)
	);

	return r0;
}

u32 tegra_sm_generic(u32 arg0, u32 arg1, u32 arg2)
{
	return __generic_smc(arg0, arg1, arg2);
}

void tegra_sm_lock(void)
{
	mutex_lock(&tegra_smc_lock);
}

void tegra_sm_unlock(void)
{
	mutex_unlock(&tegra_smc_lock);
}

int tegra_sm_is_locked(void)
{
	return mutex_is_locked(&tegra_smc_lock);
}
