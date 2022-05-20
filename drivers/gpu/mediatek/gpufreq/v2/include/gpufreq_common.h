/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2021 MediaTek Inc.
 */

#ifndef __GPUFREQ_COMMON_H__
#define __GPUFREQ_COMMON_H__

#include <linux/bits.h>
#if IS_ENABLED(CONFIG_MTK_AEE_IPANIC) && IS_ENABLED(CONFIG_MTK_AEE_FEATURE)
#include <mt-plat/mboot_params.h>
#endif

/**************************************************
 * Misc Definition
 **************************************************/
#define GPUOP(_freq, _volt, _vsram, _posdiv, _margin, _power) \
	{                                  \
		.freq = _freq,                 \
		.volt = _volt,                 \
		.vsram = _vsram,               \
		.posdiv = _posdiv,             \
		.margin = _margin,             \
		.power = _power                \
	}
#define ADJOP(_oppidx, _freq, _volt, _vsram) \
	{                                  \
		.oppidx = _oppidx,             \
		.freq = _freq,                 \
		.volt = _volt,                 \
		.vsram = _vsram,               \
	}
#define REGOP(_addr, _val) \
	{                                  \
		.addr = _addr,                 \
		.val = _val,                   \
	}
#ifndef MAX
#define MAX(x, y) (((x) < (y)) ? (y) : (x))
#endif
#ifndef MIN
#define MIN(x, y) (((x) < (y)) ? (x) : (y))
#endif

/**************************************************
 * Shader Present Setting
 **************************************************/
#define T0C0  (BIT(0))
#define T1C0  (BIT(1))
#define T2C0  (BIT(2))
#define T3C0  (BIT(3))
#define T0C1  (BIT(4))
#define T1C1  (BIT(5))
#define T2C1  (BIT(6))
#define T3C1  (BIT(7))
#define T0C2  (BIT(8))
#define T1C2  (BIT(9))
#define T2C2  (BIT(10))
#define T3C2  (BIT(11))
#define T0C3  (BIT(12))
#define T1C3  (BIT(13))
#define T2C3  (BIT(14))
#define T3C3  (BIT(15))
#define T4C0  (BIT(16))
#define T5C0  (BIT(17))
#define T6C0  (BIT(18))
#define T7C0  (BIT(19))
#define T4C1  (BIT(20))
#define T5C1  (BIT(21))
#define T6C1  (BIT(22))
#define T7C1  (BIT(23))

/**************************************************
 * Enumeration
 **************************************************/


/**************************************************
 * Structure
 **************************************************/


/**************************************************
 * Platform Function Declaration
 **************************************************/
/* Common */
unsigned int __gpufreq_bringup(void);
unsigned int __gpufreq_power_ctrl_enable(void);
unsigned int __gpufreq_active_idle_ctrl_enable(void);
unsigned int __gpufreq_get_power_state(void);
unsigned int __gpufreq_get_dvfs_state(void);
unsigned int __gpufreq_get_shader_present(void);
int __gpufreq_power_control(enum gpufreq_power_state power);
int __gpufreq_active_idle_control(enum gpufreq_power_state power);
void __gpufreq_set_timestamp(void);
void __gpufreq_check_bus_idle(void);
void __gpufreq_dump_infra_status(void);
int __gpufreq_get_batt_oc_idx(int batt_oc_level);
int __gpufreq_get_batt_percent_idx(int batt_percent_level);
int __gpufreq_get_low_batt_idx(int low_batt_level);
void __gpufreq_set_margin_mode(enum gpufreq_feat_mode mode);
void __gpufreq_set_gpm_mode(unsigned int version, enum gpufreq_feat_mode mode);
void __gpufreq_set_dfd_mode(enum gpufreq_feat_mode mode);
struct gpufreq_core_mask_info *__gpufreq_get_core_mask_table(void);
unsigned int __gpufreq_get_core_num(void);
void __gpufreq_pdca_config(enum gpufreq_power_state power);
void __gpufreq_fake_mtcmos_control(enum gpufreq_power_state power);
void __gpufreq_update_debug_opp_info(void);
void __gpufreq_set_shared_status(struct gpufreq_shared_status *shared_status);
int __gpufreq_mssv_commit(unsigned int target, unsigned int val);
/* GPU */
unsigned int __gpufreq_get_cur_fgpu(void);
unsigned int __gpufreq_get_cur_vgpu(void);
unsigned int __gpufreq_get_cur_pgpu(void);
unsigned int __gpufreq_get_max_pgpu(void);
unsigned int __gpufreq_get_min_pgpu(void);
int __gpufreq_get_cur_idx_gpu(void);
int __gpufreq_get_opp_num_gpu(void);
int __gpufreq_get_signed_opp_num_gpu(void);
unsigned int __gpufreq_get_fgpu_by_idx(int oppidx);
unsigned int __gpufreq_get_pgpu_by_idx(int oppidx);
int __gpufreq_get_idx_by_fgpu(unsigned int freq);
int __gpufreq_get_idx_by_vgpu(unsigned int volt);
int __gpufreq_get_idx_by_pgpu(unsigned int power);
unsigned int __gpufreq_get_lkg_pgpu(unsigned int volt);
unsigned int __gpufreq_get_dyn_pgpu(unsigned int freq, unsigned int volt);
int __gpufreq_generic_commit_gpu(int target_oppidx, enum gpufreq_dvfs_state key);
int __gpufreq_fix_target_oppidx_gpu(int oppidx);
int __gpufreq_fix_custom_freq_volt_gpu(unsigned int freq, unsigned int volt);
/* SRAM */
unsigned int __gpufreq_get_cur_vsram_gpu(void);
unsigned int __gpufreq_get_cur_vsram_stack(void);
/* GPUSTACK */
unsigned int __gpufreq_get_cur_fstack(void);
unsigned int __gpufreq_get_cur_vstack(void);
unsigned int __gpufreq_get_cur_pstack(void);
unsigned int __gpufreq_get_max_pstack(void);
unsigned int __gpufreq_get_min_pstack(void);
int __gpufreq_get_cur_idx_stack(void);
int __gpufreq_get_opp_num_stack(void);
int __gpufreq_get_signed_opp_num_stack(void);
unsigned int __gpufreq_get_fstack_by_idx(int oppidx);
unsigned int __gpufreq_get_pstack_by_idx(int oppidx);
int __gpufreq_get_idx_by_fstack(unsigned int freq);
int __gpufreq_get_idx_by_vstack(unsigned int volt);
int __gpufreq_get_idx_by_pstack(unsigned int power);
unsigned int __gpufreq_get_lkg_pstack(unsigned int volt);
unsigned int __gpufreq_get_dyn_pstack(unsigned int freq, unsigned int volt);
int __gpufreq_generic_commit_stack(int target_oppidx, enum gpufreq_dvfs_state key);
int __gpufreq_fix_target_oppidx_stack(int oppidx);
int __gpufreq_fix_custom_freq_volt_stack(unsigned int freq, unsigned int volt);

/**************************************************
 * Function
 **************************************************/
static inline void __gpufreq_reset_footprint(void)
{
#if IS_ENABLED(CONFIG_MTK_AEE_IPANIC) && IS_ENABLED(CONFIG_MTK_AEE_FEATURE)
	aee_rr_rec_gpu_dvfs_vgpu(0);
	aee_rr_rec_gpu_dvfs_power_count(0);
	aee_rr_rec_gpu_dvfs_oppidx(GENMASK(7, 0));
#endif /* CONFIG_MTK_AEE_IPANIC && CONFIG_MTK_AEE_FEATURE */
}

static inline void __gpufreq_footprint_power_step(unsigned int step)
{
#if IS_ENABLED(CONFIG_MTK_AEE_IPANIC) && IS_ENABLED(CONFIG_MTK_AEE_FEATURE)
	aee_rr_rec_gpu_dvfs_vgpu(step & GENMASK(7, 0));
#else
	GPUFREQ_UNREFERENCED(step);
#endif /* CONFIG_MTK_AEE_IPANIC && CONFIG_MTK_AEE_FEATURE */
}

static inline void __gpufreq_footprint_oppidx(int oppidx)
{
#if IS_ENABLED(CONFIG_MTK_AEE_IPANIC) && IS_ENABLED(CONFIG_MTK_AEE_FEATURE)
	aee_rr_rec_gpu_dvfs_oppidx(oppidx);
#else
	GPUFREQ_UNREFERENCED(oppidx);
#endif /* CONFIG_MTK_AEE_IPANIC && CONFIG_MTK_AEE_FEATURE */
}

static inline void __gpufreq_footprint_power_count(int power_count)
{
#if IS_ENABLED(CONFIG_MTK_AEE_IPANIC) && IS_ENABLED(CONFIG_MTK_AEE_FEATURE)
	aee_rr_rec_gpu_dvfs_power_count(power_count);
#else
	GPUFREQ_UNREFERENCED(power_count);
#endif /* CONFIG_MTK_AEE_IPANIC && CONFIG_MTK_AEE_FEATURE */
}

static inline void __gpufreq_abort(const char *exception_string, ...)
{
	va_list args;
	int cx = 0;
	char tmp_string[1024];

	va_start(args, exception_string);
	cx = vsnprintf(tmp_string, sizeof(tmp_string), exception_string, args);
	va_end(args);

	GPUFREQ_LOGE("[ABORT]: %s", tmp_string);
	__gpufreq_dump_infra_status();

	BUG_ON(1);
}

#endif /* __GPUFREQ_COMMON_H__ */
