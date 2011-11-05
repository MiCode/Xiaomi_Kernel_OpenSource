/* Copyright (c) 2007-2009, Code Aurora Forum. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef _ARCH_ARM_MACH_MSM_IDLE_H_
#define _ARCH_ARM_MACH_MSM_IDLE_H_

int msm_arch_idle(void);
int msm_pm_collapse(void);
void msm_pm_collapse_exit(void);
void msm_warmboot_entry(void);

#ifdef CONFIG_CPU_V7
void msm_pm_boot_entry(void);
void msm_pm_write_boot_vector(unsigned int cpu, unsigned long address);
void msm_pm_set_l2_flush_flag(unsigned int flag);
extern unsigned long msm_pm_pc_pgd;
#else
static inline void msm_pm_set_l2_flush_flag(unsigned int flag)
{
	/* empty */
}
#endif


#endif
