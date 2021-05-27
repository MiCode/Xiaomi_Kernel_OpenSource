/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2021 MediaTek Inc.
 */

#ifndef __GPUFREQ_COMMON_H__
#define __GPUFREQ_COMMON_H__

#if IS_ENABLED(CONFIG_MTK_AEE_IPANIC) && IS_ENABLED(CONFIG_MTK_AEE_FEATURE)
#include <mt-plat/mboot_params.h>
#endif

/**************************************************
 * Shader Present Setting
 **************************************************/
#define T0C0  (1 <<  0)
#define T1C0  (1 <<  1)
#define T2C0  (1 <<  2)
#define T3C0  (1 <<  3)
#define T0C1  (1 <<  4)
#define T1C1  (1 <<  5)
#define T2C1  (1 <<  6)
#define T3C1  (1 <<  7)
#define T0C2  (1 <<  8)
#define T1C2  (1 <<  9)
#define T2C2  (1 << 10)
#define T3C2  (1 << 11)
#define T0C3  (1 << 12)
#define T1C3  (1 << 13)
#define T2C3  (1 << 14)
#define T3C3  (1 << 15)
#define T4C0  (1 << 16)
#define T5C0  (1 << 17)
#define T6C0  (1 << 18)
#define T7C0  (1 << 19)
#define T4C1  (1 << 20)
#define T5C1  (1 << 21)
#define T6C1  (1 << 22)
#define T7C1  (1 << 23)

/**************************************************
 * Enumeration
 **************************************************/
enum gpufreq_vgpu_step {
	GPUFREQ_VGPU_STEP_1 = 0x1,
	GPUFREQ_VGPU_STEP_2 = 0x2,
	GPUFREQ_VGPU_STEP_3 = 0x3,
	GPUFREQ_VGPU_STEP_4 = 0x4,
	GPUFREQ_VGPU_STEP_5 = 0x5,
	GPUFREQ_VGPU_STEP_6 = 0x6,
	GPUFREQ_VGPU_STEP_7 = 0x7,
	GPUFREQ_VGPU_STEP_8 = 0x8,
	GPUFREQ_VGPU_STEP_9 = 0x9,
	GPUFREQ_VGPU_STEP_A = 0xA,
	GPUFREQ_VGPU_STEP_B = 0xB,
	GPUFREQ_VGPU_STEP_C = 0xC,
	GPUFREQ_VGPU_STEP_D = 0xD,
	GPUFREQ_VGPU_STEP_E = 0xE,
	GPUFREQ_VGPU_STEP_F = 0xF,
};

/**************************************************
 * Function
 **************************************************/
static inline void __gpufreq_footprint_vgpu(enum gpufreq_vgpu_step step)
{
#if IS_ENABLED(CONFIG_MTK_AEE_IPANIC) && IS_ENABLED(CONFIG_MTK_AEE_FEATURE)
	aee_rr_rec_gpu_dvfs_vgpu(step |
		(aee_rr_curr_gpu_dvfs_vgpu() & 0xF0));
#else
	GPUFREQ_UNREFERENCED(step);
#endif
}

static inline void __gpufreq_footprint_vgpu_reset(void)
{
#if IS_ENABLED(CONFIG_MTK_AEE_IPANIC) && IS_ENABLED(CONFIG_MTK_AEE_FEATURE)
	aee_rr_rec_gpu_dvfs_vgpu(0);
#endif
}

static inline void __gpufreq_footprint_oppidx(int idx)
{
#if IS_ENABLED(CONFIG_MTK_AEE_IPANIC) && IS_ENABLED(CONFIG_MTK_AEE_FEATURE)
	aee_rr_rec_gpu_dvfs_oppidx(idx);
#else
	GPUFREQ_UNREFERENCED(idx);
#endif
}

static inline void __gpufreq_footprint_oppidx_reset(void)
{
#if IS_ENABLED(CONFIG_MTK_AEE_IPANIC) && IS_ENABLED(CONFIG_MTK_AEE_FEATURE)
	aee_rr_rec_gpu_dvfs_oppidx(0xFF);
#endif
}

static inline void __gpufreq_footprint_power_count(int count)
{
#if IS_ENABLED(CONFIG_MTK_AEE_IPANIC) && IS_ENABLED(CONFIG_MTK_AEE_FEATURE)
	aee_rr_rec_gpu_dvfs_power_count(count);
#else
	GPUFREQ_UNREFERENCED(count);
#endif
}

static inline void __gpufreq_footprint_power_count_reset(void)
{
#if IS_ENABLED(CONFIG_MTK_AEE_IPANIC) && IS_ENABLED(CONFIG_MTK_AEE_FEATURE)
	aee_rr_rec_gpu_dvfs_power_count(0);
#endif
}

/**************************************************
 * Platform Function Declaration
 **************************************************/
/* Common */
unsigned int __gpufreq_bringup(void);
unsigned int __gpufreq_power_ctrl_enable(void);
unsigned int __gpufreq_get_dvfs_state(void);
unsigned int __gpufreq_get_shader_present(void);
int __gpufreq_power_control(enum gpufreq_power_state power);
void __gpufreq_set_timestamp(void);
void __gpufreq_check_bus_idle(void);
void __gpufreq_dump_infra_status(void);
int __gpufreq_get_batt_oc_idx(int batt_oc_level);
int __gpufreq_get_batt_percent_idx(int batt_percent_level);
int __gpufreq_get_low_batt_idx(int low_batt_level);
void __gpufreq_set_stress_test(unsigned int mode);
int __gpufreq_set_enforced_aging(unsigned int mode);
/* GPU */
unsigned int __gpufreq_get_cur_fgpu(void);
unsigned int __gpufreq_get_max_fgpu(void);
unsigned int __gpufreq_get_min_fgpu(void);
unsigned int __gpufreq_get_cur_vgpu(void);
unsigned int __gpufreq_get_max_vgpu(void);
unsigned int __gpufreq_get_min_vgpu(void);
unsigned int __gpufreq_get_cur_pgpu(void);
unsigned int __gpufreq_get_max_pgpu(void);
unsigned int __gpufreq_get_min_pgpu(void);
int __gpufreq_get_cur_idx_gpu(void);
int __gpufreq_get_max_idx_gpu(void);
int __gpufreq_get_min_idx_gpu(void);
unsigned int __gpufreq_get_opp_num_gpu(void);
unsigned int __gpufreq_get_signed_opp_num_gpu(void);
unsigned int __gpufreq_get_fgpu_by_idx(int oppidx);
unsigned int __gpufreq_get_vgpu_by_idx(int oppidx);
unsigned int __gpufreq_get_pgpu_by_idx(int oppidx);
int __gpufreq_get_idx_by_fgpu(unsigned int freq);
int __gpufreq_get_idx_by_vgpu(unsigned int volt);
int __gpufreq_get_idx_by_pgpu(unsigned int power);
unsigned int __gpufreq_get_lkg_pgpu(unsigned int volt);
unsigned int __gpufreq_get_dyn_pgpu(unsigned int freq, unsigned int volt);
const struct gpufreq_opp_info *__gpufreq_get_working_table_gpu(void);
const struct gpufreq_opp_info *__gpufreq_get_signed_table_gpu(void);
struct gpufreq_debug_opp_info __gpufreq_get_debug_opp_info_gpu(void);
int __gpufreq_generic_commit_gpu(int target_oppidx, enum gpufreq_dvfs_state key);
int __gpufreq_fix_target_oppidx_gpu(int oppidx);
int __gpufreq_fix_custom_freq_volt_gpu(unsigned int freq, unsigned int volt);
/* SRAM */
unsigned int __gpufreq_get_cur_vsram_gpu(void);
unsigned int __gpufreq_get_cur_vsram_stack(void);
unsigned int __gpufreq_get_vsram_by_vgpu(unsigned int volt);
unsigned int __gpufreq_get_vsram_by_vstack(unsigned int volt);
/* GPUSTACK */
unsigned int __gpufreq_get_cur_fstack(void);
unsigned int __gpufreq_get_max_fstack(void);
unsigned int __gpufreq_get_min_fstack(void);
unsigned int __gpufreq_get_cur_vstack(void);
unsigned int __gpufreq_get_max_vstack(void);
unsigned int __gpufreq_get_min_vstack(void);
unsigned int __gpufreq_get_cur_pstack(void);
unsigned int __gpufreq_get_max_pstack(void);
unsigned int __gpufreq_get_min_pstack(void);
int __gpufreq_get_cur_idx_stack(void);
int __gpufreq_get_max_idx_stack(void);
int __gpufreq_get_min_idx_stack(void);
unsigned int __gpufreq_get_opp_num_stack(void);
unsigned int __gpufreq_get_signed_opp_num_stack(void);
unsigned int __gpufreq_get_fstack_by_idx(int oppidx);
unsigned int __gpufreq_get_vstack_by_idx(int oppidx);
unsigned int __gpufreq_get_pstack_by_idx(int oppidx);
int __gpufreq_get_idx_by_fstack(unsigned int freq);
int __gpufreq_get_idx_by_vstack(unsigned int volt);
int __gpufreq_get_idx_by_pstack(unsigned int power);
unsigned int __gpufreq_get_lkg_pstack(unsigned int volt);
unsigned int __gpufreq_get_dyn_pstack(unsigned int freq, unsigned int volt);
const struct gpufreq_opp_info *__gpufreq_get_working_table_stack(void);
const struct gpufreq_opp_info *__gpufreq_get_signed_table_stack(void);
struct gpufreq_debug_opp_info __gpufreq_get_debug_opp_info_stack(void);
int __gpufreq_generic_commit_stack(int target_oppidx, enum gpufreq_dvfs_state key);
int __gpufreq_fix_target_oppidx_stack(int oppidx);
int __gpufreq_fix_custom_freq_volt_stack(unsigned int freq, unsigned int volt);

#endif /* __GPUFREQ_COMMON_H__ */
