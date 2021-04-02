/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2021 MediaTek Inc.
 */

#ifndef __GPUFREQ_V2_H__
#define __GPUFREQ_V2_H__

/**************************************************
 * Definition
 **************************************************/
#define GPUFREQ_TRACE_ENABLE            (0)
#define GPUFREQ_TRACE_TAG "[GPU/TRACE]"

/**************************************************
 * GPUFREQ Log Setting
 **************************************************/
#define GPUFERQ_TAG "[GPU/FREQ]"
#define GPUFREQ_LOGE(fmt, args...) \
	pr_err(GPUFERQ_TAG"[ERROR]@%s: "fmt"\n", __func__, ##args)
#define GPUFREQ_LOGW(fmt, args...) \
	pr_debug(GPUFERQ_TAG"[WARN]@%s: "fmt"\n", __func__, ##args)
#define GPUFREQ_LOGI(fmt, args...) \
	pr_info(GPUFERQ_TAG"[INFO]@%s: "fmt"\n", __func__, ##args)
#define GPUFREQ_LOGD(fmt, args...) \
	pr_debug(GPUFERQ_TAG"[DEBUG]@%s: "fmt"\n", __func__, ##args)

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
 * Misc. Definition
 **************************************************/
#define GPUFREQ_UNREFERENCED(param) ((void)(param))

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
 * External Function
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

/**************************************************
 * Platform Implementation
 **************************************************/
/* Common */
extern unsigned int __gpufreq_bringup(void);
extern unsigned int __gpufreq_power_ctrl_enable(void);
extern unsigned int __gpufreq_get_dvfs_state(void);
extern unsigned int __gpufreq_get_shader_present(void);
extern int __gpufreq_power_control(
	enum gpufreq_power_state power, enum gpufreq_cg_state cg,
	enum gpufreq_mtcmos_state mtcmos, enum gpufreq_buck_state buck);
extern void __gpufreq_set_timestamp(void);
extern void __gpufreq_check_bus_idle(void);
extern void __gpufreq_dump_infra_status(void);
extern void __gpufreq_resume_dvfs(void);
extern int __gpufreq_pause_dvfs(void);
extern int __gpufreq_map_avs_idx(int avsidx);
extern void __gpufreq_adjust_volt_by_avs(
	unsigned int avs_volt[], unsigned int array_size);
extern int __gpufreq_get_batt_oc_idx(int batt_oc_level);
extern int __gpufreq_get_batt_percent_idx(int batt_percent_level);
extern int __gpufreq_get_low_batt_idx(int low_batt_level);
extern void __gpufreq_set_stress_test(bool mode);
extern int __gpufreq_set_enforced_aging(bool mode);

/* GPU */
extern unsigned int __gpufreq_get_cur_fgpu(void);
extern unsigned int __gpufreq_get_cur_vgpu(void);
extern int __gpufreq_get_cur_idx_gpu(void);
extern int __gpufreq_get_max_idx_gpu(void);
extern int __gpufreq_get_min_idx_gpu(void);
extern unsigned int __gpufreq_get_opp_num_gpu(void);
extern unsigned int __gpufreq_get_signed_opp_num_gpu(void);
extern unsigned int __gpufreq_get_fgpu_by_idx(int oppidx);
extern unsigned int __gpufreq_get_vgpu_by_idx(int oppidx);
extern unsigned int __gpufreq_get_pgpu_by_idx(int oppidx);
extern int __gpufreq_get_idx_by_fgpu(unsigned int freq);
extern int __gpufreq_get_idx_by_pgpu(unsigned int power);
extern unsigned int __gpufreq_get_lkg_pgpu(unsigned int volt);
extern unsigned int __gpufreq_get_dyn_pgpu(
	unsigned int freq, unsigned int volt);
extern const struct gpufreq_opp_info *__gpufreq_get_working_table_gpu(void);
extern const struct gpufreq_opp_info *__gpufreq_get_signed_table_gpu(void);
extern const struct gpufreq_sb_info *__gpufreq_get_sb_table_gpu(void);
extern struct gpufreq_debug_opp_info __gpufreq_get_debug_opp_info_gpu(void);
extern int __gpufreq_commit_gpu(
	int target_oppidx, enum gpufreq_dvfs_state key);
extern int __gpufreq_fix_target_oppidx_gpu(int oppidx);
extern int __gpufreq_fix_custom_freq_volt_gpu(
	unsigned int freq, unsigned int volt);
extern void __gpufreq_restore_opp_gpu(void);

/* SRAM */
extern unsigned int __gpufreq_get_cur_vsram(void);
extern unsigned int __gpufreq_get_vsram_by_idx(int oppidx);
extern unsigned int __gpufreq_get_vsram_by_vgpu(unsigned int vgpu);

/* GPUSTACK */
extern unsigned int __gpufreq_get_cur_fgstack(void);
extern unsigned int __gpufreq_get_cur_vgstack(void);
extern int __gpufreq_get_cur_idx_gstack(void);
extern int __gpufreq_get_max_idx_gstack(void);
extern int __gpufreq_get_min_idx_gstack(void);
extern unsigned int __gpufreq_get_opp_num_gstack(void);
extern unsigned int __gpufreq_get_signed_opp_num_gstack(void);
extern unsigned int __gpufreq_get_fgstack_by_idx(int oppidx);
extern unsigned int __gpufreq_get_vgstack_by_idx(int oppidx);
extern unsigned int __gpufreq_get_pgstack_by_idx(int oppidx);
extern int __gpufreq_get_idx_by_fgstack(unsigned int freq);
extern int __gpufreq_get_idx_by_pgstack(unsigned int power);
extern unsigned int __gpufreq_get_lkg_pgstack(unsigned int volt);
extern unsigned int __gpufreq_get_dyn_pgstack(
	unsigned int freq, unsigned int volt);
extern const struct gpufreq_opp_info *__gpufreq_get_working_table_gstack(void);
extern const struct gpufreq_opp_info *__gpufreq_get_signed_table_gstack(void);
extern const struct gpufreq_sb_info *__gpufreq_get_sb_table_gstack(void);
extern struct gpufreq_debug_opp_info __gpufreq_get_debug_opp_info_gstack(void);
extern int __gpufreq_commit_gstack(
	int target_oppidx, enum gpufreq_dvfs_state key);
extern int __gpufreq_fix_target_oppidx_gstack(int oppidx);
extern int __gpufreq_fix_custom_freq_volt_gstack(
	unsigned int freq, unsigned int volt);
extern void __gpufreq_restore_opp_gstack(void);

#endif /* __GPUFREQ_V2_H__ */
