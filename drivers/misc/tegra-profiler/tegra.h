/*
 * drivers/misc/tegra-profiler/tegra.h
 *
 * Copyright (c) 2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 */

#ifndef __QUADD_TEGRA_H
#define __QUADD_TEGRA_H

#include <linux/smp.h>

#ifdef CONFIG_TEGRA_CLUSTER_CONTROL
#include <linux/io.h>
#include <../../mach-tegra/pm.h>
#endif

static inline int quadd_get_processor_id(void)
{
	int cpu_id = smp_processor_id();

#ifdef CONFIG_TEGRA_CLUSTER_CONTROL
	if (is_lp_cluster())
		cpu_id |= QM_TEGRA_POWER_CLUSTER_LP;
#endif

	return cpu_id;
}

static inline int quadd_is_cpu_with_lp_cluster(void)
{
#ifdef CONFIG_TEGRA_CLUSTER_CONTROL
	return 1;
#else
	return 0;
#endif
}

#endif  /* __QUADD_TEGRA_H */
