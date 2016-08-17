/*
 * arch/arm/mach-tegra/timer.h
 *
 * Copyright (C) 2010-2013 NVIDIA Corporation
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

#ifndef _MACH_TEGRA_TIMER_H_
#define _MACH_TEGRA_TIMER_H_

#define RTC_SECONDS		0x08
#define RTC_SHADOW_SECONDS	0x0c
#define RTC_MILLISECONDS	0x10

#define TIMERUS_CNTR_1US	0x10
#define TIMERUS_USEC_CFG	0x14
#define TIMERUS_CNTR_FREEZE	0x4c

#define TIMER1_BASE		0x0
#define TIMER2_BASE		0x8
#define TIMER3_BASE		0x50
#define TIMER4_BASE		0x58

#define TIMER_PTV		0x0
#define TIMER_PCR		0x4

void __init tegra_init_timer(void);

#ifdef CONFIG_ARCH_TEGRA_2x_SOC
void __init tegra20_init_timer(void);
#else
void __init tegra30_init_timer(void);
#endif

struct tegra_twd_context {
	u32 twd_ctrl;
	u32 twd_load;
	u32 twd_cnt;
};

void __init tegra_cpu_timer_init(void);
int tegra_get_linear_age(void);

#ifdef CONFIG_HAVE_ARM_TWD
int tegra_twd_get_state(struct tegra_twd_context *context);
void tegra_twd_suspend(struct tegra_twd_context *context);
void tegra_twd_resume(struct tegra_twd_context *context);
#else
static inline int tegra_twd_get_state(struct tegra_twd_context *context)
{ return -ENODEV; }
static inline void tegra_twd_suspend(struct tegra_twd_context *context) {}
static inline void tegra_twd_resume(struct tegra_twd_context *context) {}
#endif

#if defined(CONFIG_ARM_ARCH_TIMER) && defined(CONFIG_PM_SLEEP)
void tegra_tsc_suspend(void);
void tegra_tsc_resume(void);
void tegra_tsc_wait_for_suspend(void);
void tegra_tsc_wait_for_resume(void);
int tegra_cpu_timer_get_remain(s64 *time);
#else
static inline void tegra_tsc_suspend(void) {}
static inline void tegra_tsc_resume(void) {}
static inline void tegra_tsc_wait_for_suspend(void) {};
static inline void tegra_tsc_wait_for_resume(void) {};
#endif

u64 tegra_rtc_read_ms(void);

#endif /* _MACH_TEGRA_TIMER_H_ */
