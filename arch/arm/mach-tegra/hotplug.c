/*
 *  arch/arm/mach-tegra/hotplug.c
 *
 *  Copyright (C) 2010-2012 NVIDIA Corporation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/kernel.h>
#include <linux/io.h>
#include <linux/smp.h>
#include <linux/cpu_pm.h>

#include <asm/cacheflush.h>
#include <asm/cp15.h>
#include <asm/smp_plat.h>

#include <mach/iomap.h>

#include "gic.h"
#include "sleep.h"

#define CPU_CLOCK(cpu) (0x1<<(8+cpu))

#define CLK_RST_CONTROLLER_CLK_CPU_CMPLX \
	(IO_ADDRESS(TEGRA_CLK_RESET_BASE) + 0x4c)
#define CLK_RST_CONTROLLER_RST_CPU_CMPLX_SET \
	(IO_ADDRESS(TEGRA_CLK_RESET_BASE) + 0x340)
#define CLK_RST_CONTROLLER_RST_CPU_CMPLX_CLR \
	(IO_ADDRESS(TEGRA_CLK_RESET_BASE) + 0x344)

#ifdef CONFIG_ARCH_TEGRA_2x_SOC
/* For Tegra2 use the software-written value of the reset register for status.*/
#define CLK_RST_CONTROLLER_CPU_CMPLX_STATUS CLK_RST_CONTROLLER_RST_CPU_CMPLX_SET
#else
#define CLK_RST_CONTROLLER_CPU_CMPLX_STATUS \
	(IO_ADDRESS(TEGRA_CLK_RESET_BASE) + 0x470)
#endif

int tegra_cpu_kill(unsigned int cpu)
{
	unsigned int reg;

	cpu = cpu_logical_map(cpu);

	do {
		reg = readl(CLK_RST_CONTROLLER_CPU_CMPLX_STATUS);
		cpu_relax();
	} while (!(reg & (1<<cpu)));

#if defined(CONFIG_ARCH_TEGRA_2x_SOC) || defined(CONFIG_ARCH_TEGRA_3x_SOC)
	reg = readl(CLK_RST_CONTROLLER_CLK_CPU_CMPLX);
	writel(reg | CPU_CLOCK(cpu), CLK_RST_CONTROLLER_CLK_CPU_CMPLX);
#endif

	return 1;
}

/*
 * platform-specific code to shutdown a CPU
 *
 * Called with IRQs disabled
 */
void tegra_cpu_die(unsigned int cpu)
{
#ifdef CONFIG_ARCH_TEGRA_2x_SOC
	/* Flush the L1 data cache. */
	flush_cache_all();

	/* Place the current CPU in reset. */
	tegra2_hotplug_shutdown();
#else

	/* Disable GIC CPU interface for this CPU. */
	tegra_gic_cpu_disable(false);

	/* Flush the L1 data cache. */
	tegra_flush_l1_cache();

	/* Shut down the current CPU. */
	tegra3_hotplug_shutdown();
#endif

	/* Should never return here. */
	BUG();
}
