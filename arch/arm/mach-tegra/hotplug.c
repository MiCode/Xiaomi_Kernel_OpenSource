/*
 *
 *  Copyright (C) 2002 ARM Ltd.
 *  All Rights Reserved
 *  Copyright (C) 2010-2013 NVIDIA Corporation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/kernel.h>
#include <linux/smp.h>
#include <linux/clk/tegra.h>
#include <linux/cpu_pm.h>
#include <linux/clk/tegra.h>
#include <linux/irqchip/tegra.h>

#include <asm/cacheflush.h>
#include <asm/smp_plat.h>

#include "sleep.h"

static void (*tegra_hotplug_shutdown)(void);

int tegra_cpu_kill(unsigned int cpu)
{
	cpu = cpu_logical_map(cpu);

	tegra_wait_cpu_in_reset(cpu);

	tegra_disable_cpu_clock(cpu);

	return 1;
}

/*
 * platform-specific code to shutdown a CPU
 *
 * Called with IRQs disabled
 */
void tegra_cpu_die(unsigned int cpu)
{
	cpu = cpu_logical_map(cpu);

#ifndef CONFIG_ARCH_TEGRA_2x_SOC
	/* Disable GIC CPU interface for this CPU. */
	tegra_gic_cpu_disable(false);
#endif

	/* Flush the L1 data cache. */
	tegra_flush_l1_cache();

	/* Shut down the current CPU. */
	tegra_hotplug_shutdown();

	/* Clock gate the CPU */
	tegra_wait_cpu_in_reset(cpu);
	tegra_disable_cpu_clock(cpu);

	/* Should never return here. */
	BUG();
}

#ifdef CONFIG_ARCH_TEGRA_2x_SOC
extern void tegra20_hotplug_shutdown(void);
void __init tegra20_hotplug_init(void)
{
	tegra_hotplug_shutdown = tegra20_hotplug_shutdown;
}
#endif

#if defined(CONFIG_ARCH_TEGRA_3x_SOC) || \
    defined(CONFIG_ARCH_TEGRA_11x_SOC) || \
    defined(CONFIG_ARCH_TEGRA_12x_SOC) || \
    defined(CONFIG_ARCH_TEGRA_14x_SOC)
extern void tegra30_hotplug_shutdown(void);
void __init tegra30_hotplug_init(void)
{
	tegra_hotplug_shutdown = tegra30_hotplug_shutdown;
}
#endif
