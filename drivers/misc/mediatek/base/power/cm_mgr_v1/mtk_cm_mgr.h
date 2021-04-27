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

#ifndef __MTK_CM_MGR_H__
#define __MTK_CM_MGR_H__

#include <mtk_cm_mgr_platform.h>
#include <mtk_cm_mgr_platform_reg.h>

#include <linux/kernel.h>
#ifdef CONFIG_MTK_CPU_FREQ
#include <mtk_cpufreq_internal.h>
#endif /* CONFIG_MTK_CPU_FREQ */

struct cm_mgr_data {
	unsigned int cmd;
	unsigned int arg;
};

#define CM_MGR_D_LEN		(2)
#define IPI_CM_MGR_INIT 0
#define IPI_CM_MGR_ENABLE 1
#define IPI_CM_MGR_OPP_ENABLE 2
#define IPI_CM_MGR_SSPM_ENABLE 3
#define IPI_CM_MGR_BLANK 4
#define IPI_CM_MGR_DISABLE_FB 5
#define IPI_CM_MGR_DRAM_TYPE 6
#define IPI_CM_MGR_CPU_POWER_RATIO_UP 7
#define IPI_CM_MGR_CPU_POWER_RATIO_DOWN 8
#define IPI_CM_MGR_VCORE_POWER_RATIO_UP 9
#define IPI_CM_MGR_VCORE_POWER_RATIO_DOWN 10
#define IPI_CM_MGR_DEBOUNCE_UP 11
#define IPI_CM_MGR_DEBOUNCE_DOWN 12
#define IPI_CM_MGR_DEBOUNCE_TIMES_RESET_ADB 16
#define IPI_CM_MGR_DRAM_LEVEL 17
#define IPI_CM_MGR_LIGHT_LOAD_CPS 18
#define IPI_CM_MGR_LOADING_ENABLE 19
#define IPI_CM_MGR_LOADING_LEVEL 20
#define IPI_CM_MGR_EMI_DEMAND_CHECK 21
#define IPI_CM_MGR_OPP_FREQ_SET 22
#define IPI_CM_MGR_OPP_VOLT_SET 23
#define IPI_CM_MGR_BCPU_WEIGHT_MAX_SET 24
#define IPI_CM_MGR_BCPU_WEIGHT_MIN_SET 25
#define IPI_CM_MGR_BBCPU_WEIGHT_MAX_SET 26
#define IPI_CM_MGR_BBCPU_WEIGHT_MIN_SET 27
#define IPI_CM_MGR_DSU_DEBOUNCE_UP_SET 28
#define IPI_CM_MGR_DSU_DEBOUNCE_DOWN_SET 29
#define IPI_CM_MGR_DSU_DIFF_PWR_UP_SET 30
#define IPI_CM_MGR_DSU_DIFF_PWR_DOWN_SET 31
#define IPI_CM_MGR_DSU_PWR_RATIO_SET 31
#define IPI_CM_MGR_DSU_L_PWR_RATIO_SET 32
#define IPI_CM_MGR_DSU_B_PWR_RATIO_SET 33
#define IPI_CM_MGR_DSU_BB_PWR_RATIO_SET 34


extern spinlock_t cm_mgr_lock;
extern int cm_mgr_disable_fb;
extern int cm_mgr_blank_status;
extern unsigned int cpu_power_ratio_up[CM_MGR_EMI_OPP];
extern unsigned int cpu_power_ratio_down[CM_MGR_EMI_OPP];
extern unsigned int vcore_power_ratio_up[CM_MGR_EMI_OPP];
extern unsigned int vcore_power_ratio_down[CM_MGR_EMI_OPP];
extern unsigned int debounce_times_up_adb[CM_MGR_EMI_OPP];
extern unsigned int debounce_times_down_adb[CM_MGR_EMI_OPP];
extern int debounce_times_reset_adb;
extern int debounce_times_perf_down;
extern int debounce_times_perf_force_down;
extern int cm_mgr_enable;
extern int cm_mgr_perf_enable;
extern int cm_mgr_perf_timer_enable;
extern int cm_mgr_perf_force_enable;
extern int light_load_cps;
extern int cm_mgr_loading_level;
extern int cm_mgr_loading_enable;
extern int cm_mgr_emi_demand_check;
#ifdef USE_CPU_TO_DRAM_MAP
extern int cm_mgr_cpu_map_dram_enable;
#endif /* USE_CPU_TO_DRAM_MAP */
#ifdef USE_CPU_TO_DRAM_MAP_NEW
extern int cm_mgr_cpu_map_emi_opp;
extern int cm_mgr_cpu_map_skip_cpu_opp;
extern int cm_mgr_cpu_opp_to_dram[CM_MGR_CPU_OPP_SIZE];
extern void cm_mgr_cpu_map_update_table(void);
#endif /* USE_CPU_TO_DRAM_MAP_NEW */

extern void cm_mgr_perf_set_status(int status);
extern void cm_mgr_perf_set_force_status(int status);
extern void check_cm_mgr_status_internal(void);
extern void check_cm_mgr_status(unsigned int cluster, unsigned int freq);
extern void cm_mgr_enable_fn(int enable);
extern int cm_mgr_to_sspm_command(u32 cmd, int val);

enum mt_cpu_dvfs_id;

extern unsigned int mt_cpufreq_get_cur_phy_freq_no_lock(enum mt_cpu_dvfs_id id);

extern void sched_get_percpu_load2(int cpu, bool reset,
		unsigned int *rel_load, unsigned int *abs_load);

__attribute__((weak))
int dvfsrc_get_bw(int type) {return 0; };

__attribute__((weak))
int dvfsrc_get_emi_bw(int type) {return 0; };

#endif	/* __MTK_CM_MGR_H__ */
