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

#ifndef __MTK_CPUIDLE_H__
#define __MTK_CPUIDLE_H__

#include <asm/arch_timer.h>

#if defined(CONFIG_MACH_MT6757)
#include "mtk_cpuidle_mt6757.h"
#elif defined(CONFIG_MACH_MT6799)
#include "mtk_cpuidle_mt6799.h"
#elif defined(CONFIG_MACH_MT6759)
#include "mtk_cpuidle_mt6759.h"
#elif defined(CONFIG_MACH_MT6763)
#include "mtk_cpuidle_mt6763.h"
#elif defined(CONFIG_MACH_MT6758)
#include "mtk_cpuidle_mt6758.h"
#endif

enum mtk_cpuidle_mode {
	MTK_LEGACY_MCDI_MODE = 1,
	MTK_LEGACY_SODI_MODE,
	MTK_LEGACY_SODI3_MODE,
	MTK_LEGACY_DPIDLE_MODE,
	MTK_LEGACY_SUSPEND_MODE,
	MTK_MCDI_CPU_MODE,
	MTK_MCDI_CLUSTER_MODE,
	MTK_SODI_MODE,
	MTK_SODI3_MODE,
	MTK_DPIDLE_MODE,
	MTK_SUSPEND_MODE,
};

int mtk_cpuidle_init(void);
int mtk_enter_idle_state(int idx);


#ifdef CONFIG_MTK_RAM_CONSOLE

#define aee_addr(cpu) (mtk_cpuidle_aee_virt_addr + (cpu << 2))
#define mtk_cpuidle_footprint_log(cpu, idx) (				\
{									\
	writel_relaxed(readl_relaxed(aee_addr(cpu)) | (1 << idx),	\
		       aee_addr(cpu));					\
}									\
)
#define mtk_cpuidle_footprint_clr(cpu) (				\
{									\
	writel_relaxed(0, aee_addr(cpu));				\
}									\
)
#else
#define mtk_cpuidle_footprint(cpu, idx)
#define mtk_cpuidle_footprint_clr(cpu)
#endif


#define MTK_CPUIDLE_TIME_PROFILING 0

#if MTK_CPUIDLE_TIME_PROFILING
#define MTK_CPUIDLE_TIMESTAMP_COUNT 20

extern unsigned int mt_cpufreq_get_cur_freq(enum mt_cpu_dvfs_id id);
#define mtk_cpuidle_timestamp_log(cpu, idx) ({				\
	mtk_cpuidle_timestamp[cpu][idx] = arch_counter_get_cntvct();	\
})

struct mtk_cpuidle_time_profile {
	int count;

	unsigned int kernel_plat_backup;
	unsigned int kernel_to_atf;
	unsigned int atf_setup;
	unsigned int atf_l2_flush;
	unsigned int atf_spm_suspend;
	unsigned int atf_gic_backup;
	unsigned int atf_plat_backup;

	unsigned int atf_cpu_init;
	unsigned int atf_gic_restore;
	unsigned int atf_spm_suspend_finish;
	unsigned int atf_plat_restore;
	unsigned int atf_to_kernel;
	unsigned int kernel_plat_restore;
};
#else
#define mtk_cpuidle_timestamp_log(cpu, idx)
#endif

#define MTK_SUSPEND_FOOTPRINT_ENTER_CPUIDLE		0
#define MTK_SUSPEND_FOOTPRINT_BEFORE_ATF		1
#define MTK_SUSPEND_FOOTPRINT_ENTER_ATF			2
#define MTK_SUSPEND_FOOTPRINT_RESERVE_P1		3
#define MTK_SUSPEND_FOOTPRINT_RESERVE_P2		4
#define MTK_SUSPEND_FOOTPRINT_ENTER_SPM_SUSPEND		5
#define MTK_SUSPEND_FOOTPRINT_LEAVE_SPM_SUSPEND		6
#define MTK_SUSPEND_FOOTPRINT_BEFORE_WFI		7
#define MTK_SUSPEND_FOOTPRINT_AFTER_WFI			8
#define MTK_SUSPEND_FOOTPRINT_BEFORE_MMU		9
#define MTK_SUSPEND_FOOTPRINT_AFTER_MMU			10
#define MTK_SUSPEND_FOOTPRINT_ENTER_SPM_SUSPEND_FINISH	11
#define MTK_SUSPEND_FOOTPRINT_LEAVE_SPM_SUSPEND_FINISH	12
#define MTK_SUSPEND_FOOTPRINT_LEAVE_ATF			13
#define MTK_SUSPEND_FOOTPRINT_AFTER_ATF			14
#define MTK_SUSPEND_FOOTPRINT_LEAVE_CPUIDLE		15

#define MTK_SUSPEND_TIMESTAMP_ENTER_CPUIDLE		0
#define MTK_SUSPEND_TIMESTAMP_BEFORE_ATF		1
#define MTK_SUSPEND_TIMESTAMP_ENTER_ATF			2
#define MTK_SUSPEND_TIMESTAMP_BEFORE_L2_FLUSH		3
#define MTK_SUSPEND_TIMESTAMP_AFTER_L2_FLUSH		4
#define MTK_SUSPEND_TIMESTAMP_ENTER_SPM_SUSPEND		5
#define MTK_SUSPEND_TIMESTAMP_LEAVE_SPM_SUSPEND		6
#define MTK_SUSPEND_TIMESTAMP_GIC_P1			7
#define MTK_SUSPEND_TIMESTAMP_GIC_P2			8
#define MTK_SUSPEND_TIMESTAMP_BEFORE_WFI		9
#define MTK_SUSPEND_TIMESTAMP_AFTER_WFI			10
#define MTK_SUSPEND_TIMESTAMP_RESERVE_P1		11
#define MTK_SUSPEND_TIMESTAMP_RESERVE_P2		12
#define MTK_SUSPEND_TIMESTAMP_GIC_P3			13
#define MTK_SUSPEND_TIMESTAMP_GIC_P4			14
#define MTK_SUSPEND_TIMESTAMP_ENTER_SPM_SUSPEND_FINISH	15
#define MTK_SUSPEND_TIMESTAMP_LEAVE_SPM_SUSPEND_FINISH	16
#define MTK_SUSPEND_TIMESTAMP_LEAVE_ATF			17
#define MTK_SUSPEND_TIMESTAMP_AFTER_ATF			18
#define MTK_SUSPEND_TIMESTAMP_LEAVE_CPUIDLE		19

void __weak switch_armpll_ll_hwmode(int enable) { }
void __weak switch_armpll_l_hwmode(int enable) { }
void mt_save_generic_timer(unsigned int *container, int sw);
void mt_restore_generic_timer(unsigned int *container, int sw);
void write_cntpctl(int cntpctl);
int read_cntpctl(void);
extern unsigned long *aee_rr_rec_mtk_cpuidle_footprint_va(void);
extern unsigned long *aee_rr_rec_mtk_cpuidle_footprint_pa(void);

unsigned long * __weak
	mt_save_dbg_regs(unsigned long *p, unsigned int cpuid) { return 0; }
void __weak mt_restore_dbg_regs(unsigned long *p, unsigned int cpuid) { }
void __weak mt_copy_dbg_regs(int to, int from) { }

void __weak dpm_mcsi_mtcmos_on_flow(int on) { }

extern char *irq_match[];
extern unsigned int irq_nr[];
extern int wake_src_irq[];
extern int irq_offset[];

#endif
