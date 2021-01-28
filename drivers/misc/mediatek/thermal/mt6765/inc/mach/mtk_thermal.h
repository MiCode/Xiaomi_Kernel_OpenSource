/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2019 MediaTek Inc.
 */

#ifndef __MT6765_THERMAL_H__
#define __MT6765_THERMAL_H__

#include <linux/module.h>
#include <linux/types.h>
#include <linux/spinlock.h>
#include <linux/interrupt.h>

#include <linux/io.h>
#include <linux/uaccess.h>

#include "mt-plat/sync_write.h"

#include "mtk_gpufreq.h"

/*
 * Bank0: CPU-L		(TS_MCU1)
 * Bank1: CPU-LL	(TS_MCU2)
 * Bank2: CCI		(TS_MCU1 + TS_MCU2)
 * Bank3: GPU		(TS_MCU3)
 * Bank4: SoC		(TS_MCU4 + TS_MCU5)
 */
enum thermal_sensor {
	TS_MCU1 = 0,
	TS_MCU2,
	TS_MCU3,
	TS_MCU4,
	TS_MCU5,
	TS_ENUM_MAX,
};

enum thermal_bank_name {
	THERMAL_BANK0     = 0,
	THERMAL_BANK1     = 1,
	THERMAL_BANK2     = 2,
	THERMAL_BANK3     = 3,
	THERMAL_BANK4     = 4,
	THERMAL_BANK_NUM
};

struct TS_PTPOD {
	unsigned int ts_MTS;
	unsigned int ts_BTS;
};

extern int mtktscpu_limited_dmips;

/* Valid if it returns 1, invalid if it returns 0. */
extern int tscpu_is_temp_valid(void);

extern void get_thermal_slope_intercept
(struct TS_PTPOD *ts_info, enum thermal_bank_name ts_bank);

extern void set_taklking_flag(bool flag);

extern int tscpu_get_cpu_temp(void);

extern int tscpu_get_temp_by_bank(enum thermal_bank_name ts_bank);

#define THERMAL_WRAP_WR32(val, addr)	\
	mt_reg_sync_writel((val), ((void *)addr))

extern int get_immediate_gpu_wrap(void);

extern int get_immediate_cpuL_wrap(void);

extern int get_immediate_cpuLL_wrap(void);

extern int get_immediate_mcucci_wrap(void);

/* Added for DLPT. */
extern int tscpu_get_min_cpu_pwr(void);

extern int tscpu_get_min_gpu_pwr(void);

/* Five thermal sensors. */
enum mtk_thermal_sensor_cpu_id_met {
	MTK_THERMAL_SENSOR_TS1 = 0,
	MTK_THERMAL_SENSOR_TS2,
	MTK_THERMAL_SENSOR_TS3,
	MTK_THERMAL_SENSOR_TS4,
	MTK_THERMAL_SENSOR_TS5,

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

extern int get_immediate_ts1_wrap(void);

extern int get_immediate_ts2_wrap(void);

extern int get_immediate_ts3_wrap(void);

extern int get_immediate_ts4_wrap(void);

extern int get_immediate_ts5_wrap(void);


extern int (*get_immediate_tsX[TS_ENUM_MAX])(void);

extern int is_cpu_power_unlimit(void);	/* in mtk_ts_cpu.c */

extern int is_cpu_power_min(void);	/* in mtk_ts_cpu.c */

extern int get_cpu_target_tj(void);

extern int get_cpu_target_offset(void);

extern int mtk_gpufreq_register
(struct mt_gpufreq_power_table_info *freqs, int num);

extern int get_target_tj(void);

extern int mtk_thermal_get_tpcb_target(void);

extern void thermal_set_big_core_speed
(unsigned int tempMonCtl1, unsigned int tempMonCtl2, unsigned int tempAhbPoll);

/* return value(1): cooler of abcct/abcct_lcmoff is deactive,
 * and no thermal current limit.
 */
extern int mtk_cooler_is_abcct_unlimit(void);

#endif /* __MT6765_THERMAL_H__ */
