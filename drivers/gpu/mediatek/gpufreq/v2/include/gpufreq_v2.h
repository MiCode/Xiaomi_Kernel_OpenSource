/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2021 MediaTek Inc.
 */

#ifndef __GPUFREQ_V2_H__
#define __GPUFREQ_V2_H__

#include <uapi/asm-generic/errno-base.h>

/**************************************************
 * Definition
 **************************************************/
#define GPUFREQ_UNREFERENCED(param)     ((void)(param))
#define GPUFREQ_DEBUG_ENABLE            (0)
#define GPUFREQ_TRACE_ENABLE            (0)
#define GPUFERQ_TAG                     "[GPU/FREQ]"
#define GPUFREQ_TRACE_TAG               "[GPU/TRACE]"
#define GPUFREQ_STATUS_MEM_SZ           (0x400) // 1KB
#define GPUFREQ_FORCE_WDT_ENABLE        (1)

/**************************************************
 * GPUFREQ Log Setting
 **************************************************/
#define GPUFREQ_LOGE(fmt, args...) \
	pr_err(GPUFERQ_TAG"[ERROR]@%s: "fmt"\n", __func__, ##args)
#define GPUFREQ_LOGW(fmt, args...) \
	pr_debug(GPUFERQ_TAG"[WARN]@%s: "fmt"\n", __func__, ##args)
#define GPUFREQ_LOGI(fmt, args...) \
	pr_info(GPUFERQ_TAG"[INFO]@%s: "fmt"\n", __func__, ##args)

#if GPUFREQ_DEBUG_ENABLE
	#define GPUFREQ_LOGD(fmt, args...) \
		pr_info(GPUFERQ_TAG"[DEBUG]@%s: "fmt"\n", __func__, ##args)
#else
	#define GPUFREQ_LOGD(fmt, args...) {}
#endif /* GPUFREQ_DEBUG_ENABLE */

#if GPUFREQ_TRACE_ENABLE
	#define GPUFREQ_TRACE_START(fmt, args...) \
		pr_info(GPUFREQ_TRACE_TAG" + %s("fmt")\n", __func__, ##args)
	#define GPUFREQ_TRACE_END(fmt, args...) \
		pr_info(GPUFREQ_TRACE_TAG" - %s("fmt")\n", __func__, ##args)
#else
	#define GPUFREQ_TRACE_START(fmt, args...) {}
	#define GPUFREQ_TRACE_END(fmt, args...) {}
#endif /* GPUFREQ_TRACE_ENABLE */

/**************************************************
 * Enumeration
 **************************************************/
enum gpufreq_return {
	GPUFREQ_HW_LIMIT = 1,
	GPUFREQ_SUCCESS = 0,
	GPUFREQ_EINVAL = -EINVAL,  // -22
	GPUFREQ_ENOMEM = -ENOMEM,  // -12
	GPUFREQ_ENOENT = -ENOENT,  // -2
	GPUFREQ_ENODEV = -ENODEV,  // -19
};

enum gpufreq_posdiv {
	POSDIV_POWER_1 = 0,
	POSDIV_POWER_2 = 1,
	POSDIV_POWER_4,
	POSDIV_POWER_8,
	POSDIV_POWER_16,
};

enum gpufreq_dvfs_state {
	DVFS_FREE = 0,              // 0000 0000
	DVFS_DISABLE = 1 << 0,      // 0000 0001
	DVFS_POWEROFF = 1 << 1,     // 0000 0010
	DVFS_DEBUG_KEEP = 1 << 2,   // 0000 0100
	DVFS_AVS_KEEP = 1 << 3,     // 0000 1000
	DVFS_AGING_KEEP = 1 << 4,   // 0001 0000
};

enum gpufreq_target {
	TARGET_DEFAULT = 0,
	TARGET_GPU = 1,
	TARGET_STACK,
	TARGET_INVALID,
};

enum gpufreq_power_state {
	POWER_OFF = 0,
	POWER_ON,
};

enum gpuppm_reserved_idx {
	GPUPPM_DEFAULT_IDX = -1,
	GPUPPM_RESET_IDX = -2,
	GPUPPM_KEEP_IDX = -3,
};

enum gpuppm_limiter {
	LIMIT_SEGMENT = 0,
	LIMIT_DEBUG = 1,
	LIMIT_THERMAL_AP,
	LIMIT_THERMAL_EB,
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
struct gpufreq_opp_info {
	unsigned int freq;            /* KHz */
	unsigned int volt;            /* mV x 100 */
	unsigned int vsram;           /* mV x 100 */
	enum gpufreq_posdiv posdiv;
	unsigned int vaging;          /* mV x 100 */
	unsigned int power;           /* mW */
};

struct gpufreq_core_mask_info {
	unsigned int num;
	unsigned int mask;
};

struct gpufreq_sb_info {
	int up;
	int down;
};

struct gpuppm_limit_info {
	unsigned int limiter;
	char name[20];
	unsigned int priority;
	int ceiling;
	unsigned int c_enable;
	int floor;
	unsigned int f_enable;
};

/**************************************************
 * Platform Implementation
 **************************************************/
struct gpufreq_platform_fp {
	/* Common */
	unsigned int (*bringup)(void);
	unsigned int (*power_ctrl_enable)(void);
	unsigned int (*get_power_state)(void);
	unsigned int (*get_dvfs_state)(void);
	unsigned int (*get_shader_present)(void);
	int (*power_control)(enum gpufreq_power_state power);
	void (*set_timestamp)(void);
	void (*check_bus_idle)(void);
	void (*dump_infra_status)(void);
	void (*set_stress_test)(unsigned int mode);
	int (*set_aging_mode)(unsigned int mode);
	void (*set_gpm_mode)(unsigned int mode);
	struct gpufreq_asensor_info (*get_asensor_info)(void);
	struct gpufreq_core_mask_info *(*get_core_mask_table)(void);
	unsigned int (*get_core_num)(void);
	void (*get_critical_volt)(const struct gpufreq_opp_info *opp_table);
	void (*pdc_control)(enum gpufreq_power_state power);
	void (*fake_spm_mtcmos_control)(enum gpufreq_power_state power);
	/* GPU */
	unsigned int (*get_cur_fgpu)(void);
	unsigned int (*get_cur_vgpu)(void);
	unsigned int (*get_cur_pgpu)(void);
	unsigned int (*get_max_pgpu)(void);
	unsigned int (*get_min_pgpu)(void);
	int (*get_cur_idx_gpu)(void);
	int (*get_opp_num_gpu)(void);
	int (*get_signed_opp_num_gpu)(void);
	unsigned int (*get_fgpu_by_idx)(int oppidx);
	unsigned int (*get_pgpu_by_idx)(int oppidx);
	int (*get_idx_by_fgpu)(unsigned int freq);
	unsigned int (*get_lkg_pgpu)(unsigned int volt);
	unsigned int (*get_dyn_pgpu)(unsigned int freq, unsigned int volt);
	const struct gpufreq_opp_info *(*get_working_table_gpu)(void);
	const struct gpufreq_opp_info *(*get_signed_table_gpu)(void);
	struct gpufreq_debug_opp_info (*get_debug_opp_info_gpu)(void);
	int (*generic_commit_gpu)(int target_oppidx, enum gpufreq_dvfs_state key);
	int (*fix_target_oppidx_gpu)(int oppidx);
	int (*fix_custom_freq_volt_gpu)(unsigned int freq, unsigned int volt);
	/* SRAM */
	unsigned int (*get_cur_vsram_gpu)(void);
	unsigned int (*get_cur_vsram_stack)(void);
	/* STACK */
	unsigned int (*get_cur_fstack)(void);
	unsigned int (*get_cur_vstack)(void);
	unsigned int (*get_cur_pstack)(void);
	unsigned int (*get_max_pstack)(void);
	unsigned int (*get_min_pstack)(void);
	int (*get_cur_idx_stack)(void);
	int (*get_opp_num_stack)(void);
	int (*get_signed_opp_num_stack)(void);
	unsigned int (*get_fstack_by_idx)(int oppidx);
	unsigned int (*get_pstack_by_idx)(int oppidx);
	int (*get_idx_by_fstack)(unsigned int freq);
	unsigned int (*get_lkg_pstack)(unsigned int volt);
	unsigned int (*get_dyn_pstack)(unsigned int freq, unsigned int volt);
	const struct gpufreq_opp_info *(*get_working_table_stack)(void);
	const struct gpufreq_opp_info *(*get_signed_table_stack)(void);
	struct gpufreq_debug_opp_info (*get_debug_opp_info_stack)(void);
	int (*generic_commit_stack)(int target_oppidx, enum gpufreq_dvfs_state key);
	int (*fix_target_oppidx_stack)(int oppidx);
	int (*fix_custom_freq_volt_stack)(unsigned int freq, unsigned int volt);
};

struct gpuppm_platform_fp {
	int (*limited_commit)(enum gpufreq_target target, int oppidx);
	int (*set_limit)(enum gpufreq_target target, enum gpuppm_limiter limiter,
		int ceiling_info, int floor_info);
	int (*switch_limit)(enum gpufreq_target target, enum gpuppm_limiter limiter,
		int c_enable, int f_enable);
	int (*get_ceiling)(enum gpufreq_target target);
	int (*get_floor)(enum gpufreq_target target);
	unsigned int (*get_c_limiter)(enum gpufreq_target target);
	unsigned int (*get_f_limiter)(enum gpufreq_target target);
	const struct gpuppm_limit_info *(*get_limit_table)(enum gpufreq_target target);
	struct gpufreq_debug_limit_info (*get_debug_limit_info)(enum gpufreq_target target);
};

/**************************************************
 * GPU HAL Interface
 **************************************************/
extern int (*mtk_get_gpu_limit_index_fp)(enum gpufreq_target target,
	enum gpuppm_limit_type limit);
extern unsigned int (*mtk_get_gpu_limiter_fp)(enum gpufreq_target target,
	enum gpuppm_limit_type limit);
extern unsigned int (*mtk_get_gpu_cur_freq_fp)(enum gpufreq_target target);
extern int (*mtk_get_gpu_cur_oppidx_fp)(enum gpufreq_target target);

/**************************************************
 * External Function
 **************************************************/
/* Common */
unsigned int gpufreq_bringup(void);
unsigned int gpufreq_power_ctrl_enable(void);
unsigned int gpufreq_get_power_state(void);
unsigned int gpufreq_get_dvfs_state(void);
unsigned int gpufreq_get_shader_present(void);
void gpufreq_set_timestamp(void);
void gpufreq_check_bus_idle(void);
void gpufreq_dump_infra_status(void);
unsigned int gpufreq_get_cur_freq(enum gpufreq_target target);
unsigned int gpufreq_get_cur_volt(enum gpufreq_target target);
unsigned int gpufreq_get_cur_vsram(enum gpufreq_target target);
unsigned int gpufreq_get_cur_power(enum gpufreq_target target);
unsigned int gpufreq_get_max_power(enum gpufreq_target target);
unsigned int gpufreq_get_min_power(enum gpufreq_target target);
int gpufreq_get_cur_oppidx(enum gpufreq_target target);
int gpufreq_get_opp_num(enum gpufreq_target target);
unsigned int gpufreq_get_freq_by_idx(enum gpufreq_target target, int oppidx);
unsigned int gpufreq_get_power_by_idx(enum gpufreq_target target, int oppidx);
int gpufreq_get_oppidx_by_freq(enum gpufreq_target target, unsigned int freq);
unsigned int gpufreq_get_leakage_power(enum gpufreq_target target, unsigned int volt);
unsigned int gpufreq_get_dynamic_power(enum gpufreq_target target,
	unsigned int freq, unsigned int volt);
int gpufreq_set_limit(enum gpufreq_target target,
	enum gpuppm_limiter limiter, int ceiling_info, int floor_info);
int gpufreq_get_cur_limit_idx(enum gpufreq_target target,enum gpuppm_limit_type limit);
unsigned int gpufreq_get_cur_limiter(enum gpufreq_target target, enum gpuppm_limit_type limit);
int gpufreq_power_control(enum gpufreq_power_state power);
int gpufreq_commit(enum gpufreq_target target, int oppidx);
struct gpufreq_core_mask_info *gpufreq_get_core_mask_table(void);
unsigned int gpufreq_get_core_num(void);
void gpufreq_pdc_control(enum gpufreq_power_state power);
void gpufreq_fake_spm_mtcmos_control(enum gpufreq_power_state power);
void gpufreq_register_gpufreq_fp(struct gpufreq_platform_fp *platform_fp);
void gpufreq_register_gpuppm_fp(struct gpuppm_platform_fp *platform_fp);

/* Debug */
struct gpufreq_debug_opp_info gpufreq_get_debug_opp_info(enum gpufreq_target target);
struct gpufreq_debug_limit_info gpufreq_get_debug_limit_info(enum gpufreq_target target);
const struct gpufreq_opp_info *gpufreq_get_working_table(enum gpufreq_target target);
const struct gpufreq_opp_info *gpufreq_get_signed_table(enum gpufreq_target target);
const struct gpuppm_limit_info *gpufreq_get_limit_table(enum gpufreq_target target);
int gpufreq_switch_limit(enum gpufreq_target target,
	enum gpuppm_limiter limiter, int c_enable, int f_enable);
int gpufreq_fix_target_oppidx(enum gpufreq_target target, int oppidx);
int gpufreq_fix_custom_freq_volt(enum gpufreq_target target,
	unsigned int freq, unsigned int volt);
int gpufreq_set_stress_test(unsigned int mode);
int gpufreq_set_aging_mode(unsigned int mode);
int gpufreq_set_gpm_mode(unsigned int mode);
int gpufreq_set_test_mode(unsigned int mode);
struct gpufreq_asensor_info gpufreq_get_asensor_info(void);

#endif /* __GPUFREQ_V2_H__ */
