/*
 * arch/arm/mach-tegra/timer.h
 *
 * Copyright (c) 2012-2013 NVIDIA Corporation. All rights reserved.
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

#define TIMER1_OFFSET		0x0
#define TIMER2_OFFSET		0x8
#define TIMER3_OFFSET 		0x50
#define TIMER4_OFFSET 		0x58
#define TIMER5_OFFSET 		0x60
#define TIMER6_OFFSET 		0x68
#define TIMER7_OFFSET		0x70
#define TIMER8_OFFSET		0x78
#define TIMER9_OFFSET		0x80
#define TIMER10_OFFSET		0x88

#define TIMER_PTV		0x0
#define TIMER_PCR		0x4

#ifdef CONFIG_ARCH_TEGRA_2x_SOC
void __init tegra20_init_timer(void);
#else
void __init tegra30_init_timer(void);
#endif

#ifdef CONFIG_TEGRA_LP0_IN_IDLE
void tegra_rtc_set_trigger(unsigned long cycles);
#else
static inline void tegra_rtc_set_trigger(unsigned long cycles) {}
#endif

struct tegra_twd_context {
	u32 twd_ctrl;
	u32 twd_load;
	u32 twd_cnt;
};

extern void __iomem *timer_reg_base;
#define timer_writel(value, reg) \
	__raw_writel(value, timer_reg_base + (reg))
#define timer_readl(reg) \
	__raw_readl(timer_reg_base + (reg))

#ifdef CONFIG_ARM_ARCH_TIMER
int __init tegra_init_arch_timer(void);
extern bool arch_timer_initialized;
#endif

void __init tegra_cpu_timer_init(void);
void __init tegra_init_late_timer(void);

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

#if !defined(CONFIG_ARM_ARCH_TIMER) && !defined(CONFIG_HAVE_ARM_TWD)
void tegra_cputimer_reset_irq_affinity(int cpu);
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

int hotplug_cpu_register(struct device_node *);
#endif /* _MACH_TEGRA_TIMER_H_ */
