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

#if !IS_ENABLED(CONFIG_MTK_CM_IPI)
struct cm_mgr_data {
	unsigned int cmd;
	unsigned int arg;
};
#endif /* CONFIG_MTK_CM_IPI */

struct cm_mgr_hook {
	u32 (*cm_mgr_get_perfs)(int);
	void (*cm_mgr_perf_set_force_status)(int);
	void (*check_cm_mgr_status)(unsigned int, unsigned int, unsigned int);
	void (*cm_mgr_perf_platform_set_status)(int);
	void (*cm_mgr_perf_set_status)(int);
};

enum cm_mgr_display_ {
	CM_MGR_MAIN_SCREEN = 0,
	CM_MGR_SUB_SCREEN,
	CM_MGR_DISPLAY_MAX,
};

#if !IS_ENABLED(CONFIG_MTK_CM_IPI)
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
#endif /* CONFIG_MTK_CM_IPI */


/* common api */
#if IS_ENABLED(CONFIG_MTK_CM_IPI)
extern int get_cm_step_num(void);
extern int get_dsu_perf(void);
extern int cm_mgr_judge_perfs_dram_opp(int dram_opp);
#endif
extern void cm_mgr_update_dram_by_cpu_opp(int cpu_opp);
#if !IS_ENABLED(CONFIG_MTK_CM_IPI)
extern int cm_mgr_to_sspm_command(u32 cmd, int val);
#endif /* CONFIG_MTK_CM_IPI */
extern int cm_mgr_check_dts_setting(struct platform_device *pdev);
extern int cm_mgr_common_init(void);
extern void cm_mgr_common_exit(void);

extern void cm_mgr_set_pdev(struct platform_device *pdev);
extern int cm_mgr_get_num_array(void);
extern void cm_mgr_set_num_array(int num);
extern int cm_mgr_get_dram_opp_base(void);
extern void cm_mgr_set_dram_opp_base(int num);
extern int cm_mgr_get_num_perf(void);
extern void cm_mgr_set_num_perf(int num);
extern int cm_mgr_get_perf_enable(void);
extern int cm_mgr_get_perf_force_enable(void);
extern void cm_mgr_set_perf_force_enable(int enable);
extern int debounce_times_perf_down_get(void);
extern int debounce_times_perf_force_down_get(void);
extern int debounce_times_perf_down_local_get(void);
extern void debounce_times_perf_down_local_set(int num);
extern int debounce_times_perf_down_force_local_get(void);
extern void debounce_times_perf_down_force_local_set(int num);
extern int cm_mgr_get_blank_status(void);
extern int cm_mgr_get_disable_fb(void);
extern struct icc_path *cm_mgr_get_bw_path(void);
extern struct icc_path *cm_mgr_set_bw_path(struct icc_path *bw_path);
extern void cm_mgr_perf_set_status(int enable);
extern void cm_mgr_register_hook(struct cm_mgr_hook *hook);
extern void cm_mgr_unregister_hook(struct cm_mgr_hook *hook);
extern void cm_mgr_set_dram_opp_ceiling(int opp);
extern void cm_mgr_set_dram_opp_floor(int opp);

#endif	/* __MTK_CM_MGR_H__ */
