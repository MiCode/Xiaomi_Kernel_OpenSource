/*
 * drivers/regulator/tegra-regulator.c
 *
 * Copyright (c) 2010 Google, Inc
 * Copyright (C) 2011 NVIDIA Corporation.
 *
 * Author:
 *	Colin Cross <ccross@google.com>
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

#ifndef _MACH_TEGRA_POWERGATE_H_
#define _MACH_TEGRA_POWERGATE_H_

#if defined(CONFIG_ARCH_TEGRA_2x_SOC) || defined(CONFIG_ARCH_TEGRA_3x_SOC)
#define TEGRA_POWERGATE_CPU0	0
#else
#define TEGRA_POWERGATE_CRAIL	0
#endif
#define TEGRA_POWERGATE_3D	1
#define TEGRA_POWERGATE_3D0	TEGRA_POWERGATE_3D
#define TEGRA_POWERGATE_VENC	2
#define TEGRA_POWERGATE_PCIE	3
#define TEGRA_POWERGATE_VDEC	4
#define TEGRA_POWERGATE_L2	5
#define TEGRA_POWERGATE_MPE	6
#define TEGRA_POWERGATE_HEG	7
#define TEGRA_POWERGATE_SATA	8
#define TEGRA_POWERGATE_CPU1	9
#define TEGRA_POWERGATE_CPU2	10
#define TEGRA_POWERGATE_CPU3	11
#define TEGRA_POWERGATE_CELP	12
#define TEGRA_POWERGATE_3D1	13
#if !defined(CONFIG_ARCH_TEGRA_2x_SOC) && !defined(CONFIG_ARCH_TEGRA_3x_SOC)
#define TEGRA_POWERGATE_CPU0	14
#endif
#define TEGRA_POWERGATE_C0NC	15
#define TEGRA_POWERGATE_C1NC	16
#define TEGRA_POWERGATE_DISA	18
#define TEGRA_POWERGATE_DISB	19
#define TEGRA_POWERGATE_XUSBA	20
#define TEGRA_POWERGATE_XUSBB	21
#define TEGRA_POWERGATE_XUSBC	22

#define TEGRA_POWERGATE_CPU	TEGRA_POWERGATE_CPU0

#if defined(CONFIG_ARCH_TEGRA_2x_SOC)
#define TEGRA_NUM_POWERGATE	7
#define TEGRA_CPU_POWERGATE_ID(cpu)	(TEGRA_POWERGATE_CPU)
#define TEGRA_IS_CPU_POWERGATE_ID(id)	((id) == TEGRA_POWERGATE_CPU)
#else
#if defined(CONFIG_ARCH_TEGRA_3x_SOC)
#define TEGRA_NUM_POWERGATE	14
#else
#define TEGRA_NUM_POWERGATE	23
#endif
#define TEGRA_CPU_POWERGATE_ID(cpu)	((cpu == 0) ? TEGRA_POWERGATE_CPU0 : \
						(cpu + TEGRA_POWERGATE_CPU1 - 1))
#define TEGRA_IS_CPU_POWERGATE_ID(id)  (((id) == TEGRA_POWERGATE_CPU0) || \
					((id) == TEGRA_POWERGATE_CPU1) || \
					((id) == TEGRA_POWERGATE_CPU2) || \
					((id) == TEGRA_POWERGATE_CPU3))
#endif

struct clk;

int  __init tegra_powergate_init(void);

int tegra_cpu_powergate_id(int cpuid);
bool tegra_powergate_is_powered(int id);
int tegra_powergate_power_on(int id);
int tegra_powergate_power_off(int id);
int tegra_powergate_mc_disable(int id);
int tegra_powergate_mc_enable(int id);
int tegra_powergate_mc_flush(int id);
int tegra_powergate_mc_flush_done(int id);
int tegra_powergate_remove_clamping(int id);
const char *tegra_powergate_get_name(int id);

/*
 * Functions to powergate/un-powergate partitions.
 * Handle clk management in the API's.
 *
 * tegra_powergate_partition_with_clk_off() can be called with
 * clks ON. It disables all required clks.
 *
 * tegra_unpowergate_partition_with_clk_on() can be called with
 * all required clks OFF. Returns with all clks ON.
 *
 * Warning: In general drivers should take care of the module
 * clks and use tegra_powergate_partition() &
 * tegra_unpowergate_partition() API's.
 */
int tegra_powergate_partition_with_clk_off(int id);
int tegra_unpowergate_partition_with_clk_on(int id);

/*
 * Functions to powergate un-powergate partitions.
 * Drivers are responsible for clk enable-disable
 *
 * tegra_powergate_partition() should be called with all
 * required clks OFF. Drivers should disable clks BEFORE
 * calling this fucntion
 *
 * tegra_unpowergate_partition should be called with all
 * required clks OFF. Returns with all clks OFF. Drivers
 * should enable all clks AFTER this function
 */
int tegra_powergate_partition(int id);
int tegra_unpowergate_partition(int id);

bool tegra_powergate_check_clamping(int id);
#endif /* _MACH_TEGRA_POWERGATE_H_ */
