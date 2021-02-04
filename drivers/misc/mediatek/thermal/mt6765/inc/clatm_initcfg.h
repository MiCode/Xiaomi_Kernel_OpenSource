/*
 * Copyright (C) 2018 MediaTek Inc.
 * Copyright (C) 2021 XiaoMi, Inc.
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

#ifndef __CLATM_INITCFG_H__
#define __CLATM_INITCFG_H__

#define CLATM_SET_INIT_CFG			(1)

#define CLATM_INIT_CFG_0_TARGET_TJ		(75000)
#define CLATM_INIT_CFG_0_EXIT_POINT		(10000)
#define CLATM_INIT_CFG_0_FIRST_STEP		(3960)
#define CLATM_INIT_CFG_0_THETA_RISE		(2)
#define CLATM_INIT_CFG_0_THETA_FALL		(8)
#define CLATM_INIT_CFG_0_MIN_BUDGET_CHG		(1)
#define CLATM_INIT_CFG_0_MIN_CPU_PWR		(500)
#define CLATM_INIT_CFG_0_MAX_CPU_PWR		(3960)
#define CLATM_INIT_CFG_0_MIN_GPU_PWR		(300)
#define CLATM_INIT_CFG_0_MAX_GPU_PWR		(2000)

#define CLATM_INIT_CFG_1_TARGET_TJ		(65000)
#define CLATM_INIT_CFG_1_EXIT_POINT		(10000)
#define CLATM_INIT_CFG_1_FIRST_STEP		(3000)
#define CLATM_INIT_CFG_1_THETA_RISE		(2)
#define CLATM_INIT_CFG_1_THETA_FALL		(8)
#define CLATM_INIT_CFG_1_MIN_BUDGET_CHG		(1)
#define CLATM_INIT_CFG_1_MIN_CPU_PWR		(300)
#define CLATM_INIT_CFG_1_MAX_CPU_PWR		(3000)
#define CLATM_INIT_CFG_1_MIN_GPU_PWR		(800)
#define CLATM_INIT_CFG_1_MAX_GPU_PWR		(2000)

#define CLATM_INIT_CFG_2_TARGET_TJ		(75000)
#define CLATM_INIT_CFG_2_EXIT_POINT		(10000)
#define CLATM_INIT_CFG_2_FIRST_STEP		(3960)
#define CLATM_INIT_CFG_2_THETA_RISE		(2)
#define CLATM_INIT_CFG_2_THETA_FALL		(8)
#define CLATM_INIT_CFG_2_MIN_BUDGET_CHG		(1)
#define CLATM_INIT_CFG_2_MIN_CPU_PWR		(600)
#define CLATM_INIT_CFG_2_MAX_CPU_PWR		(3960)
#define CLATM_INIT_CFG_2_MIN_GPU_PWR		(800)
#define CLATM_INIT_CFG_2_MAX_GPU_PWR		(2000)

#define CLATM_INIT_CFG_ACTIVE_ATM_COOLER	(0)

#define CLATM_INIT_CFG_CATM			(2)

#define CLATM_INIT_CFG_PHPB_CPU_TT		(10)
#define CLATM_INIT_CFG_PHPB_CPU_TP		(10)

#define CLATM_INIT_CFG_PHPB_GPU_TT		(80)
#define CLATM_INIT_CFG_PHPB_GPU_TP		(80)

#define CLATM_INIT_HRTIMER_POLLING_DELAY	(50)

#define CLATM_USE_MIN_CPU_OPP			(1)

#define CLCTM_TARGET_TJ				(90000)
#define CLCTM_TPCB_1				(47000)
#define CLCTM_TPCB_2				(51000)
#define CLCTM_EXIT_TJ				(CLCTM_TARGET_TJ - 10000)
#define CLCTM_AE				(CLCTM_TARGET_TJ)
#define CLCTM_BE				(0)
#define CLCTM_AX				(CLCTM_EXIT_TJ)
#define CLCTM_BX				(0)
#define CLCTM_TT_HIGH				(500)
#define CLCTM_TT_LOW				(500)
#define CLCTM_STEADY_TTJ_DELTA			(13500)

#endif	/* __CLATM_INITCFG_H__ */
