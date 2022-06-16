/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2022 MediaTek Inc.
 */

#ifndef __GPUFREQ_HISTORY_MT6985_H__
#define __GPUFREQ_HISTORY_MT6985_H__

/**************************************************
 * Definition
 **************************************************/
#define GPUFREQ_HISTORY_SIZE \
	sizeof(struct gpu_dvfs_history_log)

/* Log Record to SYS SRAM           */
/* 1 log = 4(bytes) * 8 = 32(bytes) */
/* 32(bytes) * 100 = 3200 = C80h    */
#define GPUFREQ_HISTORY_LOG_NUM         100
#define GPUFREQ_HISTORY_LOG_ENTRY       8
#define GPUFREQ_HISTORY_SYSRAM_BASE     0x00118800
#define GPUFREQ_HISTORY_SYSRAM_SIZE    ((GPUFREQ_HISTORY_LOG_NUM * GPUFREQ_HISTORY_LOG_ENTRY) << 2)
#define GPUFREQ_HISTORY_OFFS_LOG_S     (GPUFREQ_HISTORY_SYSRAM_BASE)
#define GPUFREQ_HISTORY_OFFS_LOG_E     (GPUFREQ_HISTORY_OFFS_LOG_S + GPUFREQ_HISTORY_SYSRAM_SIZE)

/**************************************************
 * Structure
 **************************************************/
struct gpu_dvfs_source {
	unsigned int cur_volt:20;
	int cur_oppidx:6;
	int target_oppidx:6;
	unsigned int cur_vsram:20;
	unsigned int park_flag:12;
	// (sel:1 + sram_delsel:1 + park_flag:10) or (temperature:8 + reserve:4)
	unsigned int cur_freq:12;
	int ceiling_oppidx:6;
	int floor_oppidx:6;
	unsigned int c_limiter:4;
	unsigned int f_limiter:4;
};

struct gpu_dvfs_history_log {
	unsigned int time_stamp_h_log:32;
	unsigned int time_stamp_l_log:32;
	struct gpu_dvfs_source gpu_db_top;
	struct gpu_dvfs_source gpu_db_stack;
};

/**************************************************
 * Common Function
 **************************************************/
void __gpufreq_record_history_entry(enum gpufreq_history_state history_state);
void __gpufreq_history_memory_init(void);
void __gpufreq_history_memory_reset(void);
void __gpufreq_history_memory_uninit(void);
void __gpufreq_set_sel_bit(unsigned int sel);
unsigned int __gpufreq_get_sel_bit(void);
void __gpufreq_set_delsel_bit(unsigned int delsel);
unsigned int __gpufreq_get_delsel_bit(void);
/**************************************************
 * Variable
 **************************************************/

#endif /* __GPUFREQ_HISTORY_MT6985_H__ */
