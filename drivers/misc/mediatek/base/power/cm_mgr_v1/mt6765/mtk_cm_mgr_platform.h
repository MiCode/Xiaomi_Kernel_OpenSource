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

#ifdef CONFIG_MTK_DRAMC
#include <mtk_dramc.h>
#endif /* CONFIG_MTK_DRAMC */

/* #define ATF_SECURE_SMC */
#define PER_CPU_STALL_RATIO
#define LIGHT_LOAD
/* #define USE_AVG_PMU */
/* #define DEBUG_CM_MGR */
#define USE_TIMER_CHECK
/* #define USE_IDLE_NOTIFY */
#define USE_NEW_CPU_OPP

#define CM_MGR_EMI_OPP	2
#define CM_MGR_LOWER_OPP 9
#define CM_MGR_LOWER_OPP_1 12
#define CM_MGR_CPU_CLUSTER 2
#define CM_MGR_CPU_COUNT 8

#define VCORE_ARRAY_SIZE CM_MGR_EMI_OPP
#define CM_MGR_CPU_ARRAY_SIZE (CM_MGR_CPU_CLUSTER * CM_MGR_EMI_OPP)
#define RATIO_COUNT (100 / 5 - 1)
#define IS_UP 1
#define IS_DOWN 0
#define USE_TIMER_CHECK_TIME msecs_to_jiffies(500)

enum {
	CM_MGR_LP4X_2CH_3200 = 0,
	CM_MGR_LP3_1CH_1866,
	CM_MGR_MAX,
};

extern spinlock_t sw_zq_tx_lock;

extern void __iomem *mcucfg_mp0_counter_base;

extern int cm_mgr_abs_load;
extern int cm_mgr_rel_load;

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

#endif	/* __MTK_CM_MGR_PLATFORM_H__ */
