/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
*/

#ifndef __MTK_EEMGPU_PRJ_CONFIG_H__
#define __MTK_EEMGPU_PRJ_CONFIG_H__

/*
 * ##########################
 * build env control
 * ############################
 */
#define ENABLE_GPU (1)
#define EN_EEMGPU (1) /* enable/disable EEM (SW) */
#define DVT	(0)
#define EARLY_PORTING	(0)

/*
 * ##########################
 * debug log control
 * ############################
 */
#define CONFIG_EEMG_SHOWLOG (0)
#define EN_ISR_LOG (0)
#define DUMP_DATA_TO_DE	(0)
//#define CTP_EEMG_DUMP

/*
 * ##########################
 * EFUSE feature
 * ############################
 */
#define EEMG_FAKE_EFUSE		(0)

/*
 * ##########################
 * PTP feature
 * ############################
 */
#define ENABLE_LOO_G (1)
#define ENABLE_LOO (1)
#define SUPPORT_GPU_VB	(1)	/* gpu voltage bin */
#define EEMG_DISABLE_DRCC (0)

/*
 * ##########################
 * GPU DVFS feature
 * ############################
 */
//#define EARLY_PORTING_GPU
#define NR_FREQ 16
#define NR_FREQ_GPU 16
#define GPU_FREQ_BASE 1000000
#define GPU_M_FREQ_BASE 700000

/*
 * ##########################
 * PMIC feature
 * ############################
 */
//#define EARLY_PORTING_PMIC
#define SET_PMIC_VOLT		(1)
#define SET_PMIC_VOLT_TO_DVFS	(1)

/*
 * ##########################
 * Thermal feature
 * ############################
 */
//#define EARLY_PORTING_THERMAL
#define LVTS_THERMAL (1)
#define GPU_THERMAL_BANK THERMAL_BANK2 /* defined in mtk_thermal.h */

/*
 * ##########################
 * Temperature feature
 * ############################
 */
#define LOW_TEMP_VAL		(25000)
#define EXTRA_LOW_TEMP_VAL	(10000)
#define HIGH_TEMP_VAL		(85000)

/*
 * ##########################
 * phase out define
 * ############################
 */
/* PTP feature */
#define ENABLE_CPU (0)
#define ENABLE_MDLA (0)
#define ENABLE_VPU (0)
#define ENABLE_LOO_B (0)

#endif

