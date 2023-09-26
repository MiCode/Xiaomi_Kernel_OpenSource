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

#ifndef __MTK_CM_MGR_PLATFORM_H__
#define __MTK_CM_MGR_PLATFORM_H__

#ifdef CONFIG_MEDIATEK_DRAMC
#include <dramc.h>
#endif /* CONFIG_MEDIATEK_DRAMC */

#define PER_CPU_STALL_RATIO
#define LIGHT_LOAD
/* #define USE_AVG_PMU */
/* #define DEBUG_CM_MGR */
/* #define USE_TIMER_CHECK */
/* #define USE_IDLE_NOTIFY */
#define USE_NEW_CPU_OPP
#define USE_CM_MGR_AT_SSPM
/* #define USE_SINGLE_CLUSTER */
/* #define USE_CPU_TO_DRAM_MAP */
/* #define USE_CPU_TO_DRAM_MAP_NEW */
#define USE_BCPU_WEIGHT
#define USE_CM_USER_MODE /* POWER_HAL setting */

#define CM_BCPU_MIN_OPP_WEIGHT
#define CM_MGR_EMI_OPP 6
#define CM_MGR_LOWER_OPP 7
#define CM_MGR_CPU_CLUSTER 2
#define CM_MGR_CPU_COUNT 8
#define CM_MGR_CPU_LIMIT 6

#define CLUSTER0_MASK   0xff

#define CM_MGR_CPU_OPP_SIZE 16

#define VCORE_ARRAY_SIZE CM_MGR_EMI_OPP
#define CM_MGR_CPU_ARRAY_SIZE (CM_MGR_CPU_CLUSTER * CM_MGR_EMI_OPP)
#define RATIO_COUNT (100 / 5 - 1)
#define IS_UP 1
#define IS_DOWN 0
#define USE_TIMER_CHECK_TIME msecs_to_jiffies(50)
#define CM_MGR_INIT_DELAY_MS 1
#define CM_MGR_BW_VALUE 0
#define CM_MGR_VCORE_OPP_COUNT 30

enum {
	CM_MGR_LP4 = 0,
	CM_MGR_LP5,
	CM_MGR_MAX,
};

#ifdef USE_BCPU_WEIGHT
extern int cpu_power_bcpu_weight_max;
extern int cpu_power_bcpu_weight_min;
extern int cpu_power_bcpu_weight_max0;
extern int cpu_power_bcpu_weight_min0;
extern int cpu_power_bcpu_weight_max1;
extern int cpu_power_bcpu_weight_min1;
#endif /* USE_BCPU_WEIGHT */

#ifdef CM_BCPU_MIN_OPP_WEIGHT
extern unsigned int cm_mgr_bcpu_min_opp_weight;
extern unsigned int cm_mgr_bcpu_low_opp_weight;
extern unsigned int cm_mgr_bcpu_low_opp_bound;
extern unsigned int cm_mgr_bcpu_min_opp_weight0;
extern unsigned int cm_mgr_bcpu_low_opp_weight0;
extern unsigned int cm_mgr_bcpu_low_opp_bound0;
extern unsigned int cm_mgr_bcpu_min_opp_weight1;
extern unsigned int cm_mgr_bcpu_low_opp_weight1;
extern unsigned int cm_mgr_bcpu_low_opp_bound1;
#endif /* CM_BCPU_MIN_OPP_WEIGHT */

#ifdef USE_CM_USER_MODE
extern unsigned int cm_user_mode;
extern unsigned int cm_user_active;
extern void cm_mgr_user_mode_set(unsigned int mode);
extern void cm_mgr_user_mode_cmd(int reset, char *cmd, unsigned int val_1, unsigned int val_2);
#endif

extern void __iomem *mcucfg_mp0_counter_base;

extern unsigned int cpu_power_up_array[CM_MGR_CPU_CLUSTER];
extern unsigned int cpu_power_down_array[CM_MGR_CPU_CLUSTER];
extern unsigned int cpu_power_up[CM_MGR_CPU_CLUSTER];
extern unsigned int cpu_power_down[CM_MGR_CPU_CLUSTER];
extern unsigned int v2f[CM_MGR_CPU_CLUSTER];
extern unsigned int cpu_power_ratio_up0[CM_MGR_EMI_OPP];
extern unsigned int cpu_power_ratio_down0[CM_MGR_EMI_OPP];
extern unsigned int debounce_times_up_adb0[CM_MGR_EMI_OPP];
extern unsigned int debounce_times_down_adb0[CM_MGR_EMI_OPP];
extern unsigned int cpu_power_ratio_up1[CM_MGR_EMI_OPP];
extern unsigned int cpu_power_ratio_down1[CM_MGR_EMI_OPP];
extern unsigned int debounce_times_up_adb1[CM_MGR_EMI_OPP];
extern unsigned int debounce_times_down_adb1[CM_MGR_EMI_OPP];
extern int vcore_power_up;
extern int vcore_power_down;
extern int cpu_opp_cur[CM_MGR_CPU_CLUSTER];
extern int ratio_max[CM_MGR_CPU_CLUSTER];
extern int ratio[CM_MGR_CPU_COUNT];
extern int count[CM_MGR_CPU_CLUSTER];
extern int count_ack[CM_MGR_CPU_CLUSTER];
extern int vcore_dram_opp;
extern int vcore_dram_opp_cur;
extern int cm_mgr_abs_load;
extern int cm_mgr_rel_load;
extern int total_bw;
extern int cps_valid;
extern int debounce_times_up;
extern int debounce_times_down;
extern int ratio_scale[CM_MGR_CPU_CLUSTER];
extern int max_load[CM_MGR_CPU_CLUSTER];
extern int cpu_load[NR_CPUS];
extern int loading_acc[NR_CPUS];
extern int loading_cnt;

#define DEBUG_CM_PERF_OPP
extern int debounce_times_perf_down_local;
extern int debounce_times_perf_down_force_local;
extern int pm_qos_update_request_status;
extern int cm_mgr_dram_opp_base;
extern int cm_mgr_dram_opp;

#define USE_STEP_PERF_OPP
extern int cm_mgr_dram_perf_opp;
extern int cm_mgr_dram_step_opp;

extern void cm_mgr_update_met(void);
extern int cm_mgr_get_idx(void);
extern int cm_mgr_get_stall_ratio(int cpu);
extern int cm_mgr_get_cpu_count(int cluster);
extern int cm_mgr_get_max_stall_ratio(int cluster);
extern int cm_mgr_check_stall_ratio(int mp0, int mp1);
extern void cm_mgr_ratio_timer_en(int enable);
extern void cm_mgr_perf_platform_set_status(int enable);
extern void cm_mgr_perf_platform_set_force_status(int enable);
extern int cm_mgr_register_init(void);
extern int cm_mgr_platform_init(void);
extern void cm_mgr_set_dram_level(int level);
extern int cm_mgr_get_dram_opp(void);
extern int cm_mgr_check_bw_status(void);
extern int cm_mgr_get_bw(void);
extern void cm_mgr_ddr_setting_init(void);
extern void dbg_cm_mgr_platform_show(struct seq_file *m);
extern void dbg_cm_mgr_platform_write(int len, const char *cmd, u32 val_1,
	u32 val_2);
extern int x_ratio_enable;
extern int cm_mgr_camera_enable;
extern unsigned int cpu_power_ratio_up_x_camera[CM_MGR_EMI_OPP];
extern unsigned int cpu_power_ratio_up_x[CM_MGR_EMI_OPP];

#endif	/* __MTK_CM_MGR_PLATFORM_H__ */
