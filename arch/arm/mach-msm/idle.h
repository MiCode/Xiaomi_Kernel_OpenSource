/* Copyright (c) 2007-2009,2012 The Linux Foundation. All rights reserved.
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

/* 11 general purpose registers (r4-r14), 10 cp15 registers */
#define CPU_SAVED_STATE_SIZE (4 * 11 + 4 * 10)

#define ON	1
#define OFF	0
#define TARGET_IS_8625	1
#define POWER_COLLAPSED 1

#ifndef __ASSEMBLY__

int msm_arch_idle(void);
int msm_pm_collapse(void);
int msm_pm_pc_hotplug(void);
void msm_pm_collapse_exit(void);
extern void *msm_saved_state;
extern void (*msm_pm_disable_l2_fn)(void);
extern void (*msm_pm_enable_l2_fn)(void);
extern void (*msm_pm_flush_l2_fn)(void);
extern unsigned long msm_saved_state_phys;

#ifdef CONFIG_CPU_V7
void msm_pm_boot_entry(void);
void msm_pm_set_l2_flush_flag(unsigned int flag);
int msm_pm_get_l2_flush_flag(void);
extern unsigned long msm_pm_pc_pgd;
extern unsigned long msm_pm_boot_vector[NR_CPUS];
extern uint32_t target_type;
extern uint32_t apps_power_collapse;
extern uint32_t *l2x0_base_addr;
#else
static inline void msm_pm_set_l2_flush_flag(unsigned int flag)
{
	/* empty */
}
static inline void msm_pm_boot_entry(void)
{
	/* empty */
}
static inline void msm_pm_write_boot_vector(unsigned int cpu,
						unsigned long address)
{
	/* empty */
}
#endif
#endif
#endif
