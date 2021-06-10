/*
 * Copyright (C) 2017 MediaTek Inc.
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
#ifndef __MT6768_THERMAL_H__
#define __MT6768_THERMAL_H__

#include <linux/module.h>
#include <linux/types.h>
#include <linux/spinlock.h>
#include <linux/interrupt.h>

#include <linux/io.h>
#include <linux/uaccess.h>

#include "mt-plat/sync_write.h"
#include "mtk_thermal_typedefs.h"

#ifdef GPUFREQ_NOT_READY
struct mt_gpufreq_power_table_info {
	unsigned int gpufreq_khz;
	unsigned int gpufreq_volt;
	unsigned int gpufreq_power;
};
#else
#include "mtk_gpufreq.h"
#endif

/* chip dependent */

/* MT6768
 * Bank0        CA7LL           TSMCU4
 * Bank1        CA7BL           TSMCU5
 * Bank2        CCI             TSMCU4+5
 * Bank3        GPU             TSMCU2
 * Bank4        SoC+MD1         TSMCU1 + TSMCU3
 */
enum thermal_sensor {
	TS_MCU1 = 0,
	TS_MCU2,
	TS_MCU3,
	TS_MCU4,
	TS_MCU5,
	TS_MCU6,
	TS_MCU7,
	TS_MCU8,
	TS_ABB, /* TODO: double check if TS_ABB exists. */
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
extern int tscpu_get_temperature_range(void);
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

extern int get_immediate_cpuB_wrap(void);

extern int get_immediate_mcucci_wrap(void);
extern int get_immediate_gpu_wrap(void);
extern int get_immediate_mdla_wrap(void);
extern int get_immediate_vpu_wrap(void);
extern int get_immediate_top_wrap(void);
extern int get_immediate_md_wrap(void);

/* Added for DLPT. */
extern int tscpu_get_min_cpu_pwr(void);

extern int tscpu_get_min_gpu_pwr(void);
extern int tscpu_get_min_vpu_pwr(void);
extern int tscpu_get_min_mdla_pwr(void);


/* Five thermal sensors. */
enum mtk_thermal_sensor_cpu_id_met {
	MTK_THERMAL_SENSOR_TS1 = 0,
	MTK_THERMAL_SENSOR_TS2,
	MTK_THERMAL_SENSOR_TS3,
	MTK_THERMAL_SENSOR_TS4,
	MTK_THERMAL_SENSOR_TS5,
	MTK_THERMAL_SENSOR_TS6,
	MTK_THERMAL_SENSOR_TS7,
	MTK_THERMAL_SENSOR_TS8,
	MTK_THERMAL_SENSOR_TSABB,

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

extern int get_immediate_ts6_wrap(void);

extern int get_immediate_ts7_wrap(void);

extern int get_immediate_ts8_wrap(void);

extern int get_immediate_tsabb_wrap(void);

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

#endif /* __MT6768_THERMAL_H__ */
