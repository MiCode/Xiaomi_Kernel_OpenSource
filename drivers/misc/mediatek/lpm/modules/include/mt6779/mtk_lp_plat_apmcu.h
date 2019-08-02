/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */
#ifndef __MTK_LP_PLAT_APMCU_H__
#define __MTK_LP_PLAT_APMCU_H__

#include <linux/types.h>

#define nr_cluster_ids                  (1)
#define get_physical_cluster_id(cpu)    (0)

#define LP_PM_SYSRAM_INFO_OFS  0x8
#define LP_PM_SYSRAM_SIZE      0x800

extern void __iomem *cpu_pm_syssram_base;
/* 0x11B000 */
#define MT_CPU_PM_KERNEL_SRAMBASE	(cpu_pm_syssram_base + 0x100)
#define MT_CPU_PM_KERNEL_SRAM(slot)	(MT_CPU_PM_KERNEL_SRAMBASE + (slot<<2))


#define cpu_pm_sync_writel(v, a)				\
do {								\
	__raw_writel((v), (void __force __iomem *)((a)));	\
	mb(); /*make sure register access in order */		\
} while (0)

#define MT_CPU_PM_KERNEL_SRAM_WR(slot, val) ({\
		unsigned int uval = 0;\
		if (cpu_pm_syssram_base)\
			cpu_pm_sync_writel(val,\
				MT_CPU_PM_KERNEL_SRAM(slot)); uval; })


#define MT_CPU_PM_KERNEL_SRAM_RD(slot) ({\
		unsigned int uval = 0;\
		if (cpu_pm_syssram_base)\
			uval = __raw_readl(MT_CPU_PM_KERNEL_SRAM(slot));\
		uval; })

void mtk_lp_plat_set_mcusys_off(int cpu);
void mtk_lp_plat_clr_mcusys_off(int cpu);

void mtk_lp_plat_set_cluster_off(int cpu);
void mtk_lp_plat_clr_cluster_off(int cpu);

bool mtk_lp_plat_is_mcusys_off(void);
bool mtk_lp_plat_is_cluster_off(int cpu);

int mtk_lp_plat_apmcu_init(void);
int mtk_lp_plat_apmcu_early_init(void);

#endif
