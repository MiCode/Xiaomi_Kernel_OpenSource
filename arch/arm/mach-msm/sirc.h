/* arch/arm/mach-msm/sirc.h
 *
 * Copyright (C) 2009 Google, Inc.
 * Copyright (c) 2011, Code Aurora Forum. All rights reserved.
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

#ifndef _ARCH_ARM_MACH_MSM_SIRC_H
#define _ARCH_ARM_MACH_MSM_SIRC_H

#if defined(CONFIG_ARCH_MSM_SCORPION) && !defined(CONFIG_MSM_SMP)
void sirc_fiq_select(int irq, bool enable);
#else
static inline void sirc_fiq_select(int irq, bool enable) {}
#endif

#if defined(CONFIG_ARCH_QSD8X50) || defined(CONFIG_ARCH_FSM9XXX)
void __init msm_init_sirc(void);
void msm_sirc_enter_sleep(void);
void msm_sirc_exit_sleep(void);
#else
static inline void __init msm_init_sirc(void) {}
static inline void msm_sirc_enter_sleep(void) { }
static inline void msm_sirc_exit_sleep(void) { }
#endif

#endif
