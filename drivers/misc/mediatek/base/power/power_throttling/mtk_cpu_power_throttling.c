/*
 * Copyright (C) 2019 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include <linux/module.h>
#include <mach/upmu_sw.h>
#include <mtk_cpu_power_throttling.h>
#include <mach/mtk_pmic.h>

#if !defined(DISABLE_LOW_BATTERY_PROTECT) && defined(LOW_BATTERY_PT_SETTING_V2)
static void cpu_pt_low_battery_cb(enum LOW_BATTERY_LEVEL_TAG level)
{
	switch (level) {
	case LOW_BATTERY_LEVEL_0:
		//TODO: throttle CPU
		break;
	case LOW_BATTERY_LEVEL_1:
		//TODO: throttle CPU
		break;
	case LOW_BATTERY_LEVEL_2:
		//TODO: throttle CPU
		break;
	case LOW_BATTERY_LEVEL_3:
		//TODO: throttle CPU
		break;
	default:
		pr_info("%s invalid input: %d\n", level);
	}
}
#endif

static int __init mtk_cpu_power_throttling_module_init(void)
{
#if !defined(DISABLE_LOW_BATTERY_PROTECT) && defined(LOW_BATTERY_PT_SETTING_V2)
	register_low_battery_notify(
		&cpu_pt_low_battery_cb, LOW_BATTERY_PRIO_CPU_B);
#endif
	return 0;
}

static void __exit mtk_cpu_power_throttling_module_exit(void)
{

}

module_init(mtk_cpu_power_throttling_module_init);
module_exit(mtk_cpu_power_throttling_module_exit);

MODULE_DESCRIPTION("CPU power throttle interface");
