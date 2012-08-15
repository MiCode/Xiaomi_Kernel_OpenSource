/* Copyright (c) 2011-2012, Code Aurora Forum. All rights reserved.
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
#include <linux/errno.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_platform.h>
#include <linux/of_fdt.h>
#include <linux/of_irq.h>
#include <asm/hardware/gic.h>
#include <asm/arch_timer.h>
#include <asm/mach/arch.h>
#include <asm/mach/time.h>
#include <mach/socinfo.h>
#include <mach/board.h>
#include <mach/restart.h>

static void __init msm_dt_timer_init(void)
{
	arch_timer_of_register();
}

static struct sys_timer msm_dt_timer = {
	.init = msm_dt_timer_init
};

static void __init msm_dt_init_irq(void)
{
	if (machine_is_msm8974())
		msm_8974_init_irq();
}

static void __init msm_dt_map_io(void)
{
	if (early_machine_is_msm8974())
		msm_map_8974_io();
	if (socinfo_init() < 0)
		pr_err("%s: socinfo_init() failed\n", __func__);
}

static void __init msm_dt_init(void)
{
	struct of_dev_auxdata *adata = NULL;

	if (machine_is_msm8974())
		msm_8974_init(&adata);

	of_platform_populate(NULL, of_default_bus_match_table, adata, NULL);
	if (machine_is_msm8974()) {
		msm_8974_add_devices();
		msm_8974_add_drivers();
	}
}

static const char *msm_dt_match[] __initconst = {
	"qcom,msm8974",
	NULL
};

static void __init msm_dt_reserve(void)
{
	if (early_machine_is_msm8974())
		msm_8974_reserve();
}

static void __init msm_dt_init_very_early(void)
{
	if (early_machine_is_msm8974())
		msm_8974_very_early();
}

DT_MACHINE_START(MSM_DT, "Qualcomm MSM (Flattened Device Tree)")
	.map_io = msm_dt_map_io,
	.init_irq = msm_dt_init_irq,
	.init_machine = msm_dt_init,
	.handle_irq = gic_handle_irq,
	.timer = &msm_dt_timer,
	.dt_compat = msm_dt_match,
	.reserve = msm_dt_reserve,
	.init_very_early = msm_dt_init_very_early,
	.restart = msm_restart,
MACHINE_END
