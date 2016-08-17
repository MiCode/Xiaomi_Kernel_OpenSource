/*
 * arch/arm/mach-tegra/include/mach/gic.h
 *
 * Copyright (C) 2010-2012 NVIDIA Corporation
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

#ifndef _MACH_TEGRA_GIC_H_
#define _MACH_TEGRA_GIC_H_

#if defined(CONFIG_HOTPLUG_CPU) || defined(CONFIG_PM_SLEEP)

void tegra_gic_cpu_disable(bool disable_pass_through);
void tegra_gic_cpu_enable(void);

#endif

#define	GIC_V1	1
#define	GIC_V2	2

#if defined(CONFIG_PM_SLEEP)

int tegra_gic_pending_interrupt(void);

#ifndef CONFIG_ARCH_TEGRA_2x_SOC

void tegra_gic_dist_disable(void);
void tegra_gic_dist_enable(void);

void tegra_gic_disable_affinity(void);
void tegra_gic_restore_affinity(void);
void tegra_gic_affinity_to_cpu0(void);

#endif
#endif

u32 tegra_gic_version(void);
void __init tegra_gic_init(bool is_dt);

#endif /* _MACH_TEGRA_GIC_H_ */
