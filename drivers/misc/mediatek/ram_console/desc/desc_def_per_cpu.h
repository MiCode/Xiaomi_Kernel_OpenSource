/*
 * Copyright (C) 2015 MediaTek Inc.
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

	DF_PER_CPU(last_irq_enter, "  irq: enter(%d, "),
	DF_PER_CPU(jiffies_last_irq_enter, "%llu) "),
	DF_PER_CPU(last_irq_exit, "quit(%d, "),
	DF_PER_CPU(jiffies_last_irq_exit, "%llu)\n"),
	DF_PER_CPU(hotplug_footprint, "  hotplug: %d\n"),
	DF_PER_CPU(mtk_cpuidle_footprint, "  mtk_cpuidle_footprint: 0x%x\n"),
	DF_PER_CPU(mcdi_footprint, "  mcdi footprint: 0x%x\n"),
