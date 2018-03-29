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

/**
 * @file mt_gpufreq.h
 * @brief GPU DVFS driver interface
 */

#ifndef _MT_GPUFREQ_H
#define _MT_GPUFREQ_H

#include <linux/module.h>
#include <linux/clk.h>


/*********************
* GPU Frequency List
**********************/
#define GPU_DVFS_FREQ1                  (698000) /* KHz */
#define GPU_DVFS_FREQ2                  (598000) /* KHz */
#define GPU_DVFS_FREQ3                  (494000) /* KHz */
#define GPU_DVFS_FREQ4                  (455000) /* KHz */
#define GPU_DVFS_FREQ5                  (396500) /* KHz */
#define GPU_DVFS_FREQ6                  (299000) /* KHz */
#define GPU_DVFS_FREQ7                  (253500) /* KHz */

/* Used for fixed freq-volt setting maximum boundary */
#define GPU_DVFS_MAX_FREQ               (700000) /* KHz */
#define GPU_DVFS_MIN_FREQ               (GPU_DVFS_FREQ7)

#define GPU_DVFS_VOLT1                  (1130)   /* mV */
#define GPU_DVFS_VOLT2                  (1000)   /* mV */

/* us, (DA9212 externel buck) I2C command delay 100us, Buck 10mv/us */
#define GPU_DVFS_VOLT_SETTLE_TIME(volt_old, volt_new) (((((volt_old) > (volt_new)) ? \
			((volt_old) - (volt_new)) : ((volt_new) - (volt_old))) + 10 - 1) / 10 + 100)

/* mV -> uV */
#define GPU_VOLT_TO_EXTBUCK_VAL(volt)       ((volt)*1000)
#define GPU_VOLT_TO_EXTBUCK_MAXVAL(volt)    (GPU_VOLT_TO_EXTBUCK_VAL(volt)+10000-1)

/* register val -> mV */
#define GPU_VOLT_TO_MV(volt)            (((volt)*625)/100+700)

struct mt_gpufreq_table_info {
	unsigned int gpufreq_khz;
	unsigned int gpufreq_volt;
	unsigned int gpufreq_idx;
};

struct mt_gpufreq_power_table_info {
	unsigned int gpufreq_khz;
	unsigned int gpufreq_power;
};

/* Operate Point Definition */
#define GPUOP_FREQ_TBL(khz, volt, idx) {    \
	.gpufreq_khz = khz,                     \
	.gpufreq_volt = volt,                   \
	.gpufreq_idx = idx,                     \
}

/*****************
* extern function
******************/
extern bool         mt_gpucore_ready(void);
extern bool         mt_gpufreq_dvfs_ready(void);
extern int          mt_gpufreq_state_set(int enabled);
extern unsigned int mt_gpufreq_get_cur_freq_index(void);
extern unsigned int mt_gpufreq_get_cur_freq(void);
extern unsigned int mt_gpufreq_get_cur_volt(void);
extern unsigned int mt_gpufreq_get_dvfs_table_num(void);
extern unsigned int mt_gpufreq_target(unsigned int idx);
extern unsigned int mt_gpufreq_voltage_enable_set(unsigned int enable);
extern unsigned int mt_gpufreq_voltage_set_by_ptpod(unsigned int volt[], unsigned int array_size);
extern unsigned int mt_gpufreq_get_freq_by_idx(unsigned int idx);
extern void         mt_gpufreq_return_default_DVS_by_ptpod(void);
extern void         mt_gpufreq_enable_by_ptpod(void);
extern void         mt_gpufreq_disable_by_ptpod(void);
extern unsigned int mt_gpufreq_get_thermal_limit_index(void);
extern unsigned int mt_gpufreq_get_thermal_limit_freq(void);
extern void         mt_gpufreq_thermal_protect(unsigned int limited_power);

/*****************
* power limit notification
******************/
typedef void (*gpufreq_power_limit_notify)(unsigned int);
extern void mt_gpufreq_power_limit_notify_registerCB(gpufreq_power_limit_notify pCB);

/*****************
* touch boost notification
******************/
typedef void (*gpufreq_input_boost_notify)(unsigned int);
extern void mt_gpufreq_input_boost_notify_registerCB(gpufreq_input_boost_notify pCB);

/*****************
* profiling purpose
******************/
typedef void (*sampler_func) (unsigned int);
extern void mt_gpufreq_setfreq_registerCB(sampler_func pCB);
extern void mt_gpufreq_setvolt_registerCB(sampler_func pCB);

/******************************
* Extern Function Declaration
*******************************/
u32 get_devinfo_with_index(u32 index);

typedef void (*gpufreq_mfgclock_notify)(void);
extern void mt_gpufreq_mfgclock_notify_registerCB(
		gpufreq_mfgclock_notify pEnableCB, gpufreq_mfgclock_notify pDisableCB);

#endif
