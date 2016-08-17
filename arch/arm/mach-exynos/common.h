/*
 * Copyright (c) 2011 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * Common Header for EXYNOS machines
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __ARCH_ARM_MACH_EXYNOS_COMMON_H
#define __ARCH_ARM_MACH_EXYNOS_COMMON_H

#include <asm/soc.h>

extern struct sys_timer exynos4_timer;

void exynos_init_io(struct map_desc *mach_desc, int size);
void exynos4_init_irq(void);
void exynos5_init_irq(void);
void exynos4_restart(char mode, const char *cmd);
void exynos5_restart(char mode, const char *cmd);

#ifdef CONFIG_ARCH_EXYNOS4
void exynos4_register_clocks(void);
void exynos4_setup_clocks(void);

#else
#define exynos4_register_clocks()
#define exynos4_setup_clocks()
#endif

#ifdef CONFIG_ARCH_EXYNOS5
void exynos5_register_clocks(void);
void exynos5_setup_clocks(void);

#else
#define exynos5_register_clocks()
#define exynos5_setup_clocks()
#endif

#ifdef CONFIG_CPU_EXYNOS4210
void exynos4210_register_clocks(void);
#else
#define exynos4210_register_clocks()
#endif

#ifdef CONFIG_SOC_EXYNOS4212
void exynos4212_register_clocks(void);
#else
#define exynos4212_register_clocks()
#endif

extern struct arm_soc_smp_init_ops	exynos4_soc_smp_init_ops;
extern struct arm_soc_smp_ops		exynos4_soc_smp_ops;
extern struct arm_soc_desc		exynos4_soc_desc;

extern void exynos4_cpu_die(unsigned int cpu);

#ifdef CONFIG_ARCH_EXYNOS
extern  int exynos_init(void);
extern void exynos4_map_io(void);
extern void exynos4_init_clocks(int xtal);
extern void exynos4_init_uarts(struct s3c2410_uartcfg *cfg, int no);
#endif

#endif /* __ARCH_ARM_MACH_EXYNOS_COMMON_H */
