/*
 * Copyright (C) 2018 MediaTek Inc.
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
#ifndef __MT6785_THERMAL_H__
#define __MT6785_THERMAL_H__

#include <linux/module.h>
#include <linux/types.h>
#include <linux/spinlock.h>
#include <linux/interrupt.h>

#include <linux/io.h>
#include <linux/uaccess.h>

#include "mt-plat/sync_write.h"

#include "mtk_thermal_typedefs.h"

/* TODO: remove this! */
/*#define GPUFREQ_NOT_READY*/

#ifdef GPUFREQ_NOT_READY
struct mt_gpufreq_power_table_info {
	unsigned int gpufreq_khz;
	unsigned int gpufreq_volt;
	unsigned int gpufreq_power;
};
#else
#include "mtk_gpufreq.h"
#endif

/*=============================================================
 * LVTS SW Configs
 *=============================================================
 */
#define CFG_THERM_LVTS				(1)
#define CFG_THERM_NO_AUXADC			(1)

#if CFG_THERM_LVTS
#define	CFG_LVTS_DOMINATOR			(1)
#define	LVTS_THERMAL_CONTROLLER_HW_FILTER	(1) /* 1, 2, 4, 8, 16 */
#define	LVTS_DEVICE_AUTO_RCK			(0)
/*Use bootup "count RC", no need to get "count RC" again after resume*/
#define CFG_THERM_USE_BOOTUP_COUNT_RC
#else
#define	CFG_LVTS_DOMINATOR			(0)
#define	LVTS_THERMAL_CONTROLLER_HW_FILTER	(0)
#define	LVTS_DEVICE_AUTO_RCK			(0)
#endif

/*
 *There is no idle code in kernel since mt6885(big sw).
 *Thus, kernel only can use "cpu pm notifier" to do idle scenario things.
 *
 *Release LVTS in thermal kernel driver
 *1. SPM will pause LVTS thermal controllers before closing 26M
 *2. After leaving SODI3, SPM will release LVTS thermal controllers
 *    if controllers were paused properly.
 *3. After leaving SODI3, Thermal driver will release LVTS thermal
 *    controllers if SPM didn't release controller successfully
 */
#define LVTS_CPU_PM_NTFY_CALLBACK

#if defined(LVTS_CPU_PM_NTFY_CALLBACK)
#define CFG_THERM_SODI3_RELEASE
//#define LVTS_CPU_PM_NTFY_PROFILE
#endif

/* public thermal sensor enum */

enum thermal_sensor {
#if CFG_THERM_LVTS
	TS_LVTS1_0 = 0,	/* LVTS1-0 Little */
	TS_LVTS1_1,		/* LVTS1-1 Little */
	TS_LVTS1_2,		/* LVTS1-2 Little */
	TS_LVTS1_3,		/* LVTS1-3 Little */
	TS_LVTS2_0,		/* LVTS2-0 Big */
	TS_LVTS2_1,		/* LVTS2-1 Big */
	TS_LVTS3_0,		/* LVTS3-0 GPU */
	TS_LVTS3_1,		/* LVTS3-1 CAM */
	TS_LVTS3_2,		/* LVTS3-2 VP */
	TS_LVTS3_3,		/* LVTS3-3 VP */
	TS_LVTS4_0,		/* LVTS4-0 VPU */
	TS_LVTS5_0,		/* LVTS5-0 MD-4G */
	TS_LVTS5_1,		/* LVTS5-1 MD-5G */
	TS_LVTS5_2,		/* LVTS5-2 MD-3G */

#endif
	TS_ENUM_MAX
};

enum thermal_bank_name {
	THERMAL_BANK0 = 0,
	THERMAL_BANK1,
	THERMAL_BANK2,
	THERMAL_BANK3,
	THERMAL_BANK4,
	THERMAL_BANK5,
	THERMAL_BANK_NUM
};

struct TS_PTPOD {
	unsigned int ts_MTS;
	unsigned int ts_BTS;
};

extern int mtktscpu_limited_dmips;
extern int tscpu_get_temperature_range(void);

/* Valid if it returns 1, invalid if it returns 0. */
extern int tscpu_is_temp_valid(void);

extern void get_thermal_slope_intercept(
	struct TS_PTPOD *ts_info, enum thermal_bank_name ts_bank);

#if CFG_THERM_LVTS
extern void get_lvts_slope_intercept(
		struct TS_PTPOD *ts_info, enum thermal_bank_name ts_bank);
#endif

extern void set_taklking_flag(bool flag);

extern int tscpu_get_cpu_temp(void);

extern int tscpu_get_temp_by_bank(enum thermal_bank_name ts_bank);

#define THERMAL_WRAP_WR32(val, addr)	\
	mt_reg_sync_writel((val), ((void *)addr))

extern int get_immediate_cpuL_wrap(void);
extern int get_immediate_cpuB_wrap(void);
extern int get_immediate_mcucci_wrap(void);
extern int get_immediate_gpu_wrap(void);
extern int get_immediate_vpu_wrap(void);
extern int get_immediate_top_wrap(void);
extern int get_immediate_md_wrap(void);

/* Added for DLPT/EARA */
extern int tscpu_get_min_cpu_pwr(void);
extern int tscpu_get_min_gpu_pwr(void);
extern int tscpu_get_min_vpu_pwr(void);
extern int tscpu_get_min_mdla_pwr(void);

extern void lvts_ipi_send_efuse_data(void);
extern void lvts_ipi_send_sspm_thermal_thtottle(void);
extern void lvts_ipi_send_sspm_thermal_suspend_resume(int is_suspend);

/* Five thermal sensors. */
enum mtk_thermal_sensor_cpu_id_met {
#if CFG_THERM_LVTS
	MTK_THERMAL_SENSOR_LVTS1_0,
	MTK_THERMAL_SENSOR_LVTS1_1,
	MTK_THERMAL_SENSOR_LVTS1_2,
	MTK_THERMAL_SENSOR_LVTS1_3,
	MTK_THERMAL_SENSOR_LVTS2_0,
	MTK_THERMAL_SENSOR_LVTS2_1,
	MTK_THERMAL_SENSOR_LVTS3_0,
	MTK_THERMAL_SENSOR_LVTS3_1,
	MTK_THERMAL_SENSOR_LVTS3_2,
	MTK_THERMAL_SENSOR_LVTS3_3,
	MTK_THERMAL_SENSOR_LVTS4_0,
	MTK_THERMAL_SENSOR_LVTS5_0,
	MTK_THERMAL_SENSOR_LVTS5_1,
	MTK_THERMAL_SENSOR_LVTS5_2,
#endif
	ATM_CPU_LIMIT,
	ATM_GPU_LIMIT,

	MTK_THERMAL_SENSOR_CPU_COUNT
};

extern int tscpu_get_cpu_temp_met(enum mtk_thermal_sensor_cpu_id_met id);

typedef void (*met_thermalsampler_funcMET)(void);

extern void mt_thermalsampler_registerCB(met_thermalsampler_funcMET pCB);

extern void mtkTTimer_cancel_timer(void);

extern void mtkTTimer_start_timer(void);

extern int mtkts_bts_get_hw_temp(void);

extern int get_immediate_ts0_wrap(void);
extern int get_immediate_ts1_wrap(void);
extern int get_immediate_ts2_wrap(void);
extern int get_immediate_ts3_wrap(void);
extern int get_immediate_ts4_wrap(void);
extern int get_immediate_ts5_wrap(void);
extern int get_immediate_ts6_wrap(void);
extern int get_immediate_ts7_wrap(void);
extern int get_immediate_ts8_wrap(void);
extern int get_immediate_ts9_wrap(void);

#if CFG_THERM_LVTS
extern int get_immediate_tslvts1_0_wrap(void);
extern int get_immediate_tslvts1_1_wrap(void);
extern int get_immediate_tslvts1_2_wrap(void);
extern int get_immediate_tslvts1_3_wrap(void);
extern int get_immediate_tslvts2_0_wrap(void);
extern int get_immediate_tslvts2_1_wrap(void);
extern int get_immediate_tslvts3_0_wrap(void);
extern int get_immediate_tslvts3_1_wrap(void);
extern int get_immediate_tslvts3_2_wrap(void);
extern int get_immediate_tslvts3_3_wrap(void);
extern int get_immediate_tslvts4_0_wrap(void);
extern int get_immediate_tslvts5_0_wrap(void);
extern int get_immediate_tslvts5_1_wrap(void);
extern int get_immediate_tslvts5_2_wrap(void);
#endif

extern int get_immediate_tsabb_wrap(void);

extern int (*get_immediate_tsX[TS_ENUM_MAX])(void);

extern int is_cpu_power_unlimit(void);	/* in mtk_ts_cpu.c */

extern int is_cpu_power_min(void);	/* in mtk_ts_cpu.c */

extern int get_cpu_target_tj(void);

extern int get_cpu_target_offset(void);

extern int mtk_gpufreq_register(
	struct mt_gpufreq_power_table_info *freqs, int num);


extern int get_target_tj(void);

extern int mtk_thermal_get_tpcb_target(void);

extern void thermal_set_big_core_speed(
unsigned int tempMonCtl1, unsigned int tempMonCtl2, unsigned int tempAhbPoll);

/*
 * return value(1): cooler of abcct/abcct_lcmoff is deactive,
 * and no thermal current limit.
 */
extern int mtk_cooler_is_abcct_unlimit(void);

extern int tscpu_kernel_status(void);
#endif /* __MT6785_THERMAL_H__ */
