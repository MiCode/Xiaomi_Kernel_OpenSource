/*
 * Copyright (C) 2017 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#ifndef _MTK_CPUIDLE_PRIVATE_H_
#define _MTK_CPUIDLE_PRIVATE_H_

unsigned long *aee_rr_rec_mtk_cpuidle_footprint_va(void);
unsigned long *aee_rr_rec_mtk_cpuidle_footprint_pa(void);
int request_uart_to_wakeup(void);

unsigned long * __weak mt_save_dbg_regs(unsigned long *p, unsigned int cpuid)
{
	return 0;
}

void __weak mt_restore_dbg_regs(unsigned long *p, unsigned int cpuid)
{
}

void __weak dpm_mcsi_mtcmos_on_flow(int on)
{
}

#define CPUIDLE_FP_ENTER_CPUIDLE		0
#define CPUIDLE_FP_BEFORE_ATF			1
#define CPUIDLE_FP_ENTER_ATF			2
#define CPUIDLE_FP_RESERVE_P1			3
#define CPUIDLE_FP_RESERVE_P2			4
#define CPUIDLE_FP_ENTER_SPM_SUSPEND		5
#define CPUIDLE_FP_LEAVE_SPM_SUSPEND		6
#define CPUIDLE_FP_BEFORE_WFI			7
#define CPUIDLE_FP_AFTER_WFI			8
#define CPUIDLE_FP_BEFORE_MMU			9
#define CPUIDLE_FP_AFTER_MMU			10
#define CPUIDLE_FP_ENTER_SPM_SUSPEND_FINISH	11
#define CPUIDLE_FP_LEAVE_SPM_SUSPEND_FINISH	12
#define CPUIDLE_FP_LEAVE_ATF			13
#define CPUIDLE_FP_AFTER_ATF			14
#define CPUIDLE_FP_LEAVE_CPUIDLE		15


#define CPUIDLE_TS_ENTER_CPUIDLE		0
#define CPUIDLE_TS_BEFORE_ATF			1
#define CPUIDLE_TS_ENTER_ATF			2
#define CPUIDLE_TS_BEFORE_L2_FLUSH		3
#define CPUIDLE_TS_AFTER_L2_FLUSH		4
#define CPUIDLE_TS_ENTER_SPM_SUSPEND		5
#define CPUIDLE_TS_LEAVE_SPM_SUSPEND		6
#define CPUIDLE_TS_GIC_P1			7
#define CPUIDLE_TS_GIC_P2			8
#define CPUIDLE_TS_BEFORE_WFI			9
#define CPUIDLE_TS_AFTER_WFI			10
#define CPUIDLE_TS_RESERVE_P1			11
#define CPUIDLE_TS_RESERVE_P2			12
#define CPUIDLE_TS_GIC_P3			13
#define CPUIDLE_TS_GIC_P4			14
#define CPUIDLE_TS_ENTER_SPM_SUSPEND_FINISH	15
#define CPUIDLE_TS_LEAVE_SPM_SUSPEND_FINISH	16
#define CPUIDLE_TS_LEAVE_ATF			17
#define CPUIDLE_TS_AFTER_ATF			18
#define CPUIDLE_TS_LEAVE_CPUIDLE		19
#define NR_CPUIDLE_TS				20

struct cpuidle_perf_time {
	unsigned int count;
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

	unsigned int k2atf;
	unsigned int atf2wfi;
	unsigned int wfi2k;
	unsigned int k2leave;
};

#endif
