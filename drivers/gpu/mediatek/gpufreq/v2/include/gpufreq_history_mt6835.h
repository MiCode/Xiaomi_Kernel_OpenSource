/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2022 MediaTek Inc.
 */

#ifndef __GPUFREQ_HISTORY_MT6835_H__
#define __GPUFREQ_HISTORY_MT6835_H__

/**************************************************
 * Definition
 **************************************************/
#define GPUFREQ_HISTORY_SIZE \
	sizeof(struct gpu_dvfs_history_log)

/* Log Record to SYS SRAM           */
/* 1 log = 4(bytes) * 9 = 36(bytes) */
/* 36(bytes) * 100 = 3600 = E10h    */
#define GPUFREQ_HISTORY_LOG_NUM         100
#define GPUFREQ_HISTORY_LOG_ENTRY       9
#define GPUFREQ_HISTORY_SYSRAM_BASE     0x00111000
#define GPUFREQ_HISTORY_SYSRAM_SIZE    ((GPUFREQ_HISTORY_LOG_NUM * GPUFREQ_HISTORY_LOG_ENTRY) << 2)
#define GPUFREQ_HISTORY_OFFS_LOG_S     (GPUFREQ_HISTORY_SYSRAM_BASE)
#define GPUFREQ_HISTORY_OFFS_LOG_E     (GPUFREQ_HISTORY_OFFS_LOG_S + GPUFREQ_HISTORY_SYSRAM_SIZE)

#define GPUFREQ_POWER_INIT_LOG_ENTRY             (1)
#define GPUFREQ_POWER_ONESHOT_SIZE               (GPUFREQ_POWER_INIT_LOG_ENTRY << 2)
#define GPUFREQ_POWER_HISTORY_LOG_NUM            (20)
#define GPUFREQ_POWER_HISTORY_LOG_ENTRY          (3)
#define GPUFREQ_POWER_HISTORY_SYSRAM_BASE        (GPUFREQ_HISTORY_OFFS_LOG_E)
#define GPUFREQ_POWER_HISTORY_SYSRAM_SIZE        \
	((GPUFREQ_POWER_HISTORY_LOG_NUM * GPUFREQ_POWER_HISTORY_LOG_ENTRY) << 2)
#define GPUFREQ_POWER_HISTORY_OFFS_LOG_S         (GPUFREQ_POWER_HISTORY_SYSRAM_BASE)
#define GPUFREQ_POWER_HISTORY_OFFS_LOG_E         \
	(GPUFREQ_POWER_HISTORY_OFFS_LOG_S + GPUFREQ_POWER_ONESHOT_SIZE + \
		GPUFREQ_POWER_HISTORY_SYSRAM_SIZE)
/**************************************************
 * Structure
 **************************************************/
struct gpu_dvfs_source {
	unsigned int cur_volt:20;
	int cur_oppidx:6;
	int target_oppidx:6;
	unsigned int cur_vsram:20;
	unsigned int cur_freq:12;
	unsigned int cur_vcore:20;
	unsigned int park_flag:12;
	// (temperature:8 + reserve:4) or (sel:1 + sram_delsel:1 + park_flag:10)
};

struct gpu_dvfs_limiter {
	int ceiling_oppidx:6;
	int floor_oppidx:6;
	unsigned int c_limiter:4;
	unsigned int f_limiter:4;
	unsigned int reserve:12;
};

struct gpu_dvfs_history_log {
	unsigned int time_stamp_h_log:32;
	unsigned int time_stamp_l_log:32;
	struct gpu_dvfs_source gpu_db_top;
	struct gpu_dvfs_source gpu_db_stack;
	struct gpu_dvfs_limiter gpu_db_limiter;
};

struct gpu_power_init_log {
	unsigned int start_idx:32;
};

struct gpu_power_history_log {
	unsigned int time_stamp_h_log:32;
	unsigned int time_stamp_l_log:32;
	unsigned int power_on_off:1;
	unsigned int buck_vcore:1;
	unsigned int buck_vsram:1;
	unsigned int buck_vtop:1;
	unsigned int buck_vstack:1;
	unsigned int mfg_0:1;
	unsigned int mfg_1:1;
	unsigned int mfg_2:1;
	unsigned int mfg_3:1;
	unsigned int mfg_4_19:15;
	unsigned int reserve:8;
};

struct gpu_history_buck_info {
	unsigned int buck_vcore;
	unsigned int buck_vsram;
	unsigned int buck_vtop;
	unsigned int buck_vstack;
};

struct gpu_history_mfg_info {
	unsigned int mfg_0;
	unsigned int mfg_1;
	unsigned int mfg_2;
	unsigned int mfg_3;
};

/**************************************************
 * Enumeration
 **************************************************/
enum gpu_history_buck_status {
	HISTORY_BUCK_VCORE = 0,
	HISTORY_BUCK_VSRAM,
	HISTORY_BUCK_VTOP,
	HISTORY_BUCK_VSTACK,
};

enum gpu_history_mfg_status {
	HISTORY_MFG_0 = 0,
	HISTORY_MFG_1,
	HISTORY_MFG_2,
	HISTORY_MFG_3,
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

void __gpufreq_power_history_init_entry(void);
void __gpufreq_power_history_entry(void);
void __gpufreq_power_history_memory_init(void);
void __gpufreq_power_history_memory_reset(void);
void __gpufreq_power_history_memory_uninit(void);
void __gpufreq_set_power_buck(enum gpu_history_buck_status target, unsigned int power_status);
void __gpufreq_set_power_mfg(enum gpu_history_mfg_status target, unsigned int power_mfg);
unsigned int __gpufreq_get_power_buck(enum gpu_history_buck_status target);
unsigned int __gpufreq_get_power_mfg(enum gpu_history_mfg_status target);

/**************************************************
 * Variable
 **************************************************/

#endif /* __GPUFREQ_HISTORY_MT6835_H__ */
