/* Copyright (c) 2017-2018, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/kernel.h>
#include "board-dt.h"
#include <asm/mach/map.h>
#include <asm/mach/arch.h>
#include <linux/of_platform.h>

static const char *msm8953_dt_match[] __initconst = {
	"qcom,msm8953",
	"qcom,apq8053",
	NULL
};

static void __init msm8953_init(void)
{
	if (IS_ENABLED(CONFIG_ARCH_MSM8953_BOOT_ORDERING))
		return;
	board_dt_populate(NULL);
}

#ifdef CONFIG_ARCH_MSM8953_BOOT_ORDERING
static int __init msm8953_dt_populate(void)
{
	of_platform_populate(NULL, of_default_bus_match_table, NULL, NULL);

	/* Explicitly parent the /soc devices to the root node to preserve
	 * the kernel ABI (sysfs structure, etc) until userspace is updated
	 */
	return of_platform_populate(of_find_node_by_path("/soc"),
				of_default_bus_match_table, NULL, NULL);
}
late_initcall(msm8953_dt_populate);
#endif

DT_MACHINE_START(MSM8953_DT,
	"Qualcomm Technologies, Inc. MSM8953 (Flattened Device Tree)")
	.init_machine		= msm8953_init,
	.dt_compat		= msm8953_dt_match,
MACHINE_END
