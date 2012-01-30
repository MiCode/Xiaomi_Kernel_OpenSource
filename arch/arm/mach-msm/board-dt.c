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

static void __init msm_dt_timer_init(void)
{
	struct device_node *node;
	struct resource res;
	int rc;

	node = of_find_compatible_node(NULL, NULL, "qcom,msm-qtimer");
	if (!node) {
		pr_err("no matching timer node found\n");
		return;
	}

	rc = of_irq_to_resource(node, 0, &res);
	if (rc < 0)
		pr_err("interrupt not specified in timer node\n");
	else
		arch_timer_register(&res, 1);

	of_node_put(node);
}

static struct sys_timer msm_dt_timer = {
	.init = msm_dt_timer_init
};

int __cpuinit local_timer_setup(struct clock_event_device *evt)
{
	return 0;
}

void local_timer_stop(void)
{
	return;
}

static void __init msm_dt_init_irq(void)
{
	if (machine_is_copper())
		msm_copper_init_irq();
}

static void __init msm_dt_map_io(void)
{
	if (early_machine_is_copper())
		msm_map_copper_io();
	if (socinfo_init() < 0)
		pr_err("%s: socinfo_init() failed\n", __func__);
}

static void __init msm_dt_init(void)
{
	struct of_dev_auxdata *adata = NULL;

	if (machine_is_copper())
		msm_copper_init(&adata);

	of_platform_populate(NULL, of_default_bus_match_table, adata, NULL);
	if (machine_is_copper())
		msm_copper_add_devices();
}

static const char *msm_dt_match[] __initdata = {
	"qcom,msmcopper",
	NULL
};

static void __init msm_dt_reserve(void)
{
	if (early_machine_is_copper())
		msm_copper_reserve();
}

static void __init msm_dt_init_very_early(void)
{
	if (early_machine_is_copper())
		msm_copper_very_early();
}

DT_MACHINE_START(MSM_DT, "Qualcomm MSM (Flattened Device Tree)")
	.map_io = msm_dt_map_io,
	.init_irq = msm_dt_init_irq,
	.init_machine = msm_dt_init,
	.handle_irq = gic_handle_irq,
	.timer = &msm_dt_timer,
	.dt_compat = msm_dt_match,
	.nr_irqs = -1,
	.reserve = msm_dt_reserve,
	.init_very_early = msm_dt_init_very_early,
MACHINE_END
