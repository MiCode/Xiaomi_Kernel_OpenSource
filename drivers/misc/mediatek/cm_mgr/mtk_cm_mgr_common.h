/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __MTK_CM_MGR_H__
#define __MTK_CM_MGR_H__

#include <linux/kernel.h>

struct mtk_cm_mgr {
	struct device *dev;
	int num_perf;
	int idx;
	void __iomem *base;
	int *buf;
};

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

extern struct platform_device *cm_mgr_pdev;
extern void __iomem *cm_mgr_base;
extern int cm_mgr_num_perf;
extern int *cm_mgr_perfs;
extern int cm_mgr_num_array;

extern int cm_mgr_blank_status;
extern int cm_mgr_disable_fb;
extern int cm_mgr_emi_demand_check;
extern int cm_mgr_enable;
extern int cm_mgr_loading_enable;
extern int cm_mgr_loading_level;
extern int cm_mgr_opp_enable;
extern int cm_mgr_perf_enable;
extern int cm_mgr_perf_force_enable;
#if defined(CONFIG_MTK_TINYSYS_SSPM_SUPPORT)
extern int cm_mgr_sspm_enable;
#endif /* CONFIG_MTK_TINYSYS_SSPM_SUPPORT */
extern int *cpu_power_ratio_down;
extern int *cpu_power_ratio_up;
extern int *debounce_times_down_adb;
extern int debounce_times_perf_down;
extern int debounce_times_perf_force_down;
extern int debounce_times_reset_adb;
extern int *debounce_times_up_adb;
extern int light_load_cps;
extern int *vcore_power_ratio_down;
extern int *vcore_power_ratio_up;

extern int debounce_times_perf_down_local;
extern int debounce_times_perf_down_force_local;
extern int pm_qos_update_request_status;
extern int cm_mgr_dram_opp_base;
extern int cm_mgr_dram_opp;

extern int cm_mgr_loop_count;
extern int total_bw_value;

/* common api */
extern void cm_mgr_perf_set_status(int status);
extern void cm_mgr_perf_set_force_status(int status);
extern void cm_mgr_enable_fn(int enable);
extern int cm_mgr_to_sspm_command(u32 cmd, int val);
extern int cm_mgr_common_init(void);
extern void cm_mgr_common_exit(void);

/* platform api */
extern void cm_mgr_perf_platform_set_status(int enable);
extern void cm_mgr_perf_platform_set_force_status(int enable);

#endif	/* __MTK_CM_MGR_H__ */
