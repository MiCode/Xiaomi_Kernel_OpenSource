/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2021 MediaTek Inc.
 */

#ifndef __MT_GPUFREQ_CORE_H__
#define __MT_GPUFREQ_CORE_H__

/**************************************************
 * Function
 **************************************************/
/* Common */
unsigned int __gpufreq_bringup(void);
unsigned int __gpufreq_power_ctrl_enable(void);
unsigned int __gpufreq_get_dvfs_state(void);
unsigned int __gpufreq_get_shader_present(void);
int __gpufreq_power_control(
	enum gpufreq_power_state power, enum gpufreq_cg_state cg,
	enum gpufreq_mtcmos_state mtcmos, enum gpufreq_buck_state buck);
void __gpufreq_set_timestamp(void);
void __gpufreq_check_bus_idle(void);
void __gpufreq_resume_dvfs(void);
int __gpufreq_pause_dvfs(void);
int __gpufreq_map_avs_idx(int avsidx);
void __gpufreq_adjust_volt_by_avs(
	unsigned int avs_volt[], unsigned int array_size);
int __gpufreq_get_batt_oc_idx(int batt_oc_level);
int __gpufreq_get_batt_percent_idx(int batt_percent_level);
int __gpufreq_get_low_batt_idx(int low_batt_level);
void __gpufreq_set_stress_test(bool mode);
int __gpufreq_set_enforced_aging(bool mode);

/* GPU */
unsigned int __gpufreq_get_cur_fgpu(void);
unsigned int __gpufreq_get_cur_vgpu(void);
int __gpufreq_get_cur_idx_gpu(void);
int __gpufreq_get_max_idx_gpu(void);
int __gpufreq_get_min_idx_gpu(void);
unsigned int __gpufreq_get_opp_num_gpu(void);
unsigned int __gpufreq_get_signed_opp_num_gpu(void);
unsigned int __gpufreq_get_fgpu_by_idx(int oppidx);
unsigned int __gpufreq_get_vgpu_by_idx(int oppidx);
unsigned int __gpufreq_get_pgpu_by_idx(int oppidx);
int __gpufreq_get_idx_by_fgpu(unsigned int freq);
int __gpufreq_get_idx_by_pgpu(unsigned int power);
unsigned int __gpufreq_get_lkg_pgpu(unsigned int volt);
unsigned int __gpufreq_get_dyn_pgpu(
	unsigned int freq, unsigned int volt);
const struct gpufreq_opp_info *__gpufreq_get_working_table_gpu(void);
const struct gpufreq_opp_info *__gpufreq_get_signed_table_gpu(void);
const struct gpufreq_sb_info *__gpufreq_get_sb_table_gpu(void);
struct gpufreq_debug_opp_info __gpufreq_get_debug_opp_info_gpu(void);
int __gpufreq_commit_gpu(
	int target_oppidx, enum gpufreq_dvfs_state key);
int __gpufreq_fix_target_oppidx_gpu(int oppidx);
int __gpufreq_fix_custom_freq_volt_gpu(
	unsigned int freq, unsigned int volt);
void __gpufreq_restore_opp_gpu(void);

/* SRAM */
unsigned int __gpufreq_get_cur_vsram(void);
unsigned int __gpufreq_get_vsram_by_idx(int oppidx);
unsigned int __gpufreq_get_vsram_by_vgpu(unsigned int vgpu);

/* GPUSTACK */
unsigned int __gpufreq_get_cur_fgstack(void);
unsigned int __gpufreq_get_cur_vgstack(void);
int __gpufreq_get_cur_idx_gstack(void);
int __gpufreq_get_max_idx_gstack(void);
int __gpufreq_get_min_idx_gstack(void);
unsigned int __gpufreq_get_opp_num_gstack(void);
unsigned int __gpufreq_get_signed_opp_num_gstack(void);
unsigned int __gpufreq_get_fgstack_by_idx(int oppidx);
unsigned int __gpufreq_get_vgstack_by_idx(int oppidx);
unsigned int __gpufreq_get_pgstack_by_idx(int oppidx);
int __gpufreq_get_idx_by_fgstack(unsigned int freq);
int __gpufreq_get_idx_by_pgstack(unsigned int power);
unsigned int __gpufreq_get_lkg_pgstack(unsigned int volt);
unsigned int __gpufreq_get_dyn_pgstack(
	unsigned int freq, unsigned int volt);
const struct gpufreq_opp_info *__gpufreq_get_working_table_gstack(void);
const struct gpufreq_opp_info *__gpufreq_get_signed_table_gstack(void);
const struct gpufreq_sb_info *__gpufreq_get_sb_table_gstack(void);
struct gpufreq_debug_opp_info __gpufreq_get_debug_opp_info_gstack(void);
int __gpufreq_commit_gstack(
	int target_oppidx, enum gpufreq_dvfs_state key);
int __gpufreq_fix_target_oppidx_gstack(int oppidx);
int __gpufreq_fix_custom_freq_volt_gstack(
	unsigned int freq, unsigned int volt);
void __gpufreq_restore_opp_gstack(void);

#endif /* __MT_GPUFREQ_CORE_H__ */
