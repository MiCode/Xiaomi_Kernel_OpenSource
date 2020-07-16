/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2015 MediaTek Inc.
 */

	DF_PER_CPU(last_irq_enter, "  irq: enter(%d, "),
	DF_PER_CPU(jiffies_last_irq_enter, "%llu) "),
	DF_PER_CPU(last_irq_exit, "quit(%d, "),
	DF_PER_CPU(jiffies_last_irq_exit, "%llu)\n"),
	DF_PER_CPU(hotplug_footprint, "  hotplug: %d\n"),
	DF_PER_CPU(mtk_cpuidle_footprint, "  mtk_cpuidle_footprint: 0x%x\n"),
	DF_PER_CPU(mcdi_footprint, "  mcdi footprint: 0x%x\n"),
