/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2021 MediaTek Inc.
 */

#ifndef __MTK_GPUFREQ_V2_H__
#define __MTK_GPUFREQ_V2_H__

/**************************************************
 * Definition
 **************************************************/
#define GPUFREQ_TRACE_ENABLE            (0)
#define GPUFREQ_TRACE_TAG "[GPU/TRACE]"

#if GPUFREQ_TRACE_ENABLE
	#define GPUFREQ_TRACE_START(fmt, args...) \
		pr_err(GPUFREQ_TRACE_TAG" + %s("fmt")\n", __func__, ##args)
	#define GPUFREQ_TRACE_END(fmt, args...) \
		pr_err(GPUFREQ_TRACE_TAG" - %s("fmt")\n", __func__, ##args)
#else
	#define GPUFREQ_TRACE_START(fmt, args...) \
		pr_debug(GPUFREQ_TRACE_TAG" + %s("fmt")\n", __func__, ##args)
	#define GPUFREQ_TRACE_END(fmt, args...) \
		pr_debug(GPUFREQ_TRACE_TAG" - %s("fmt")\n", __func__, ##args)
#endif

/**************************************************
 * Enumeration
 **************************************************/
enum gpufreq_return {
	GPUFREQ_HW_LIMIT = 1,
	GPUFREQ_SUCCESS = 0,
	GPUFREQ_EINVAL = -1,
	GPUFREQ_ENOMEM,
	GPUFREQ_ENOENT,
};

enum gpufreq_postdiv {
	POSDIV_POWER_1 = 0,
	POSDIV_POWER_2,
	POSDIV_POWER_4,
	POSDIV_POWER_8,
	POSDIV_POWER_16,
};

enum gpufreq_dvfs_state {
	DVFS_FREE = 0,            // 0000
	DVFS_DISABLE = 1 << 0,    // 0001
	DVFS_POWEROFF = 1 << 1,   // 0010
	DVFS_DEBUG_KEEP = 1 << 2, // 0100
	DVFS_AVS_KEEP = 1 << 3,   // 1000
};

enum gpufreq_target {
#if defined(GPUFREQ_DUAL_BUCK)
	TARGET_GPUSTACK = 0,
	TARGET_GPU = 1,
	TARGET_INVALID = 2,
	TARGET_DEFAULT = TARGET_GPUSTACK,
#else
	TARGET_GPU = 0,
	TARGET_INVALID = 1,
	TARGET_GPUSTACK = 2,
	TARGET_DEFAULT = TARGET_GPU,
#endif
};

enum gpufreq_power_state {
	POWER_OFF = 0,
	POWER_ON,
};

enum gpufreq_cg_state {
	CG_OFF = 0,
	CG_ON,
};

enum gpufreq_mtcmos_state {
	MTCMOS_OFF = 0,
	MTCMOS_ON,
};

enum gpufreq_buck_state {
	BUCK_OFF = 0,
	BUCK_ON,
};

enum gpuppm_reserved_idx {
	GPUPPM_DEFAULT_IDX = -1,
	GPUPPM_RESET_IDX = -2,
	GPUPPM_KEEP_IDX = -3,
};

enum gpuppm_limiter {
	LIMIT_SEGMENT = 0,
	LIMIT_DEBUG = 1,
	LIMIT_THERMAL,
	LIMIT_SRAMRC,
	LIMIT_BATT_OC,
	LIMIT_BATT_PERCENT,
	LIMIT_LOW_BATT,
	LIMIT_PBM,
	LIMIT_APIBOOST,
	LIMIT_FPSGO,
	LIMIT_NUM,
};

enum gpuppm_limit_type {
	GPUPPM_CEILING = 0,
	GPUPPM_FLOOR = 1,
	GPUPPM_INVALID,
};

/**************************************************
 * Structure
 **************************************************/
/* todo: remove power table struct */
struct mt_gpufreq_power_table_info {
	unsigned int gpufreq_khz;
	unsigned int gpufreq_vgpu;
	unsigned int gpufreq_power;
};

struct gpufreq_opp_info {
	unsigned int freq;
	unsigned int volt;
	unsigned int vsram;
	enum gpufreq_postdiv postdiv;
	unsigned int vaging;
	unsigned int power;
};

struct gpufreq_sb_info {
	int up;
	int down;
};

/**************************************************
 * Function
 **************************************************/
unsigned int gpufreq_bringup(void);
unsigned int gpufreq_power_ctrl_enable(void);
unsigned int gpufreq_custom_init_enable(void);
unsigned int gpufreq_get_dvfs_state(void);
unsigned int gpufreq_get_shader_present(void);
void gpufreq_set_timestamp(void);
void gpufreq_check_bus_idle(void);
void gpufreq_dump_infra_status(void);
void gpufreq_resume_dvfs(void);
int gpufreq_pause_dvfs(void);
int gpufreq_map_avs_idx(int avsidx);
void gpufreq_adjust_volt_by_avs(
	unsigned int avs_volt[], unsigned int array_size);
void gpufreq_restore_opp(enum gpufreq_target target);

unsigned int gpufreq_get_cur_freq(enum gpufreq_target target);
unsigned int gpufreq_get_cur_volt(enum gpufreq_target target);
int gpufreq_get_cur_oppidx(enum gpufreq_target target);
int gpufreq_get_max_oppidx(enum gpufreq_target target);
int gpufreq_get_min_oppidx(enum gpufreq_target target);
unsigned int gpufreq_get_opp_num(enum gpufreq_target target);
unsigned int gpufreq_get_freq_by_idx(
	enum gpufreq_target target, int oppidx);
unsigned int gpufreq_get_volt_by_idx(
	enum gpufreq_target target, int oppidx);
unsigned int gpufreq_get_power_by_idx(
	enum gpufreq_target target, int oppidx);
int gpufreq_get_oppidx_by_freq(
	enum gpufreq_target target, unsigned int freq);
int gpufreq_get_oppidx_by_power(
	enum gpufreq_target target, unsigned int power);
unsigned int gpufreq_get_leakage_power(
	enum gpufreq_target target, unsigned int volt);
unsigned int gpufreq_get_dynamic_power(
	enum gpufreq_target target, unsigned int freq, unsigned int volt);
int gpufreq_get_cur_limit_idx(
	enum gpufreq_target target, enum gpuppm_limit_type limit);
unsigned int gpufreq_get_cur_limiter(
	enum gpufreq_target target, enum gpuppm_limit_type limit);

int gpufreq_power_control(
	enum gpufreq_power_state power, enum gpufreq_cg_state cg,
	enum gpufreq_mtcmos_state mtcmos, enum gpufreq_buck_state buck);
int gpufreq_commit(enum gpufreq_target target, int oppidx);
int gpufreq_set_limit(
	enum gpufreq_target target, unsigned int limiter,
	int ceiling, int floor);

#endif /* __MTK_GPUFREQ_V2_H__ */
