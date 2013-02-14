/* Copyright (c) 2008-2009, 2011-2012 The Linux Foundation. All rights reserved.
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

#ifndef _ARCH_ARM_MACH_MSM_TIMER_H_
#define _ARCH_ARM_MACH_MSM_TIMER_H_

extern struct sys_timer msm_timer;

uint32_t msm_timer_get_sclk_ticks(void);
int msm_timer_init_time_sync(void (*timeout)(void));
#ifndef CONFIG_ARM_ARCH_TIMER
void __iomem *msm_timer_get_timer0_base(void);
int64_t msm_timer_enter_idle(void);
void msm_timer_exit_idle(int low_power);
int64_t msm_timer_get_sclk_time(int64_t *period);
#else
static inline int64_t msm_timer_enter_idle(void) { return 0; }
static inline void msm_timer_exit_idle(int low_power) { return; }
static inline int64_t msm_timer_get_sclk_time(int64_t *period) { return 0; }
static inline void __iomem *msm_timer_get_timer0_base(void) { return NULL; }
#endif
#endif
