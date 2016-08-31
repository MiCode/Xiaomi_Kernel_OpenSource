/*
 * arch/arm/mach-tegra/board-dt-tegra148.c
 *
 * NVIDIA Tegra148 device tree board support
 *
 * Copyright (C) 2012-2013 NVIDIA Corporation. All rights reserved.
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

#include <linux/kernel.h>
#include <linux/of.h>
#include <linux/irqchip.h>

#include <asm/mach/arch.h>

#include "board.h"
#include "clock.h"
#include "common.h"

#ifdef CONFIG_USE_OF

static void __init tegra148_dt_init(void)
{
}

static const char * const tegra148_dt_board_compat[] = {
	"nvidia,tegra148",
	NULL
};

DT_MACHINE_START(TEGRA148_DT, "NVIDIA Tegra148 (Flattened Device Tree)")
	.smp		= smp_ops(tegra_smp_ops),
	.map_io		= tegra_map_common_io,
	.init_early	= tegra14x_init_early,
	.init_irq	= irqchip_init,
	.init_time	= tegra_init_timer,
	.init_machine	= tegra148_dt_init,
	.restart	= tegra_assert_system_reset,
	.dt_compat	= tegra148_dt_board_compat,
MACHINE_END

#endif
