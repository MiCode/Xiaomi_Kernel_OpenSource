/*
 * Copyright (C) 2018 MediaTek Inc.
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
#define CLATM_INIT_CFG_0_MIN_CPU_PWR		(300)
#define CLATM_INIT_CFG_0_MAX_CPU_PWR		(3960)
#define CLATM_INIT_CFG_0_MIN_GPU_PWR		(800)
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

#define CLATM_INIT_CFG_CATM			(0)

#define CLATM_INIT_CFG_PHPB_CPU_TT		(10)
#define CLATM_INIT_CFG_PHPB_CPU_TP		(50)

#define CLATM_INIT_CFG_PHPB_GPU_TT		(80)
#define CLATM_INIT_CFG_PHPB_GPU_TP		(80)

#define CLATM_INIT_HRTIMER_POLLING_DELAY	(50)

#define CLATM_USE_MIN_CPU_OPP			(1)

#define CLATM_CONFIGURABLE_TIMER



#define  POLLING_TRIP_TEMP0 55000
#define  POLLING_TRIP_TEMP1 45000
#define  POLLING_TRIP_TEMP2 40000

#define POLLING_FACTOR0 10
#define POLLING_FACTOR1 2
#define POLLING_FACTOR2 4


#endif	/* __CLATM_INITCFG_H__ */
