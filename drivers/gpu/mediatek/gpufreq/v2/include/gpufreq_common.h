/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2021 MediaTek Inc.
 */

#ifndef __GPUFREQ_COMMON_H__
#define __GPUFREQ_COMMON_H__

#if IS_ENABLED(CONFIG_MTK_AEE_IPANIC) && IS_ENABLED(CONFIG_MTK_AEE_FEATURE)
#include <mt-plat/mboot_params.h>
#endif

#include <gpufreq_v2.h>
#if IS_ENABLED(CONFIG_MTK_AEE_AED)
#include <aed.h>
#endif

/**************************************************
 * Misc Definition
 **************************************************/
#ifndef MAX
#define MAX(x, y) (((x) < (y)) ? (y) : (x))
#endif
#ifndef MIN
#define MIN(x, y) (((x) < (y)) ? (x) : (y))
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
enum gpufreq_power_step {
	GPUFREQ_POWER_STEP_01 = 0x1,
	GPUFREQ_POWER_STEP_02 = 0x2,
	GPUFREQ_POWER_STEP_03 = 0x3,
	GPUFREQ_POWER_STEP_04 = 0x4,
	GPUFREQ_POWER_STEP_05 = 0x5,
	GPUFREQ_POWER_STEP_06 = 0x6,
	GPUFREQ_POWER_STEP_07 = 0x7,
	GPUFREQ_POWER_STEP_08 = 0x8,
	GPUFREQ_POWER_STEP_09 = 0x9,
	GPUFREQ_POWER_STEP_0A = 0xA,
	GPUFREQ_POWER_STEP_0B = 0xB,
	GPUFREQ_POWER_STEP_0C = 0xC,
	GPUFREQ_POWER_STEP_0D = 0xD,
	GPUFREQ_POWER_STEP_0E = 0xE,
	GPUFREQ_POWER_STEP_0F = 0xF,
	GPUFREQ_POWER_STEP_10 = 0x10,
	GPUFREQ_POWER_STEP_11 = 0x11,
	GPUFREQ_POWER_STEP_12 = 0x12,
	GPUFREQ_POWER_STEP_13 = 0x13,
	GPUFREQ_POWER_STEP_14 = 0x14,
	GPUFREQ_POWER_STEP_15 = 0x15,
	GPUFREQ_POWER_STEP_16 = 0x16,
	GPUFREQ_POWER_STEP_17 = 0x17,
	GPUFREQ_POWER_STEP_18 = 0x18,
	GPUFREQ_POWER_STEP_19 = 0x19,
	GPUFREQ_POWER_STEP_1A = 0x1A,
	GPUFREQ_POWER_STEP_1B = 0x1B,
	GPUFREQ_POWER_STEP_1C = 0x1C,
	GPUFREQ_POWER_STEP_1D = 0x1D,
	GPUFREQ_POWER_STEP_1E = 0x1E,
	GPUFREQ_POWER_STEP_1F = 0x1F,
};

enum gpufreq_exception {
	GPUFREQ_GPU_EXCEPTION,
	GPUFREQ_PMIC_EXCEPTION,
	GPUFREQ_CCF_EXCEPTION,
	GPUFREQ_FHCTL_EXCEPTION,
	NUM_EXCEPTION,
};

/**************************************************
 * Structure
 **************************************************/
struct gpufreq_exception_info {
	enum gpufreq_exception exception_type;
	char exception_string[1024];
};

/**************************************************
 * Local Variable Definition
 **************************************************/
static struct gpufreq_exception_info g_pending_aee;
static bool g_have_pending_aee;

static const char * const g_exception_string[] = {
	"GPUFREQ_GPU_EXCEPTION",
	"GPUFREQ_PMIC_EXCEPTION",
	"GPUFREQ_CCF_EXCEPTION",
	"GPUFREQ_FHCTL_EXCEPTION",
};

/**************************************************
 * Platform Function Declaration
 **************************************************/
/* Common */
unsigned int __gpufreq_bringup(void);
unsigned int __gpufreq_power_ctrl_enable(void);
unsigned int __gpufreq_get_power_state(void);
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
int __gpufreq_set_aging_mode(unsigned int mode);
void __gpufreq_set_gpm_mode(unsigned int mode);
struct gpufreq_asensor_info __gpufreq_get_asensor_info(void);
struct gpufreq_core_mask_info *__gpufreq_get_core_mask_table(void);
unsigned int __gpufreq_get_core_num(void);
void __gpufreq_get_critical_volt(const struct gpufreq_opp_info *opp_table);
void __gpufreq_pdc_control(enum gpufreq_power_state power);
void __gpufreq_fake_spm_mtcmos_control(enum gpufreq_power_state power);
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
const struct gpufreq_opp_info *__gpufreq_get_working_table_stack(void);
const struct gpufreq_opp_info *__gpufreq_get_signed_table_stack(void);
struct gpufreq_debug_opp_info __gpufreq_get_debug_opp_info_stack(void);
int __gpufreq_generic_commit_stack(int target_oppidx, enum gpufreq_dvfs_state key);
int __gpufreq_fix_target_oppidx_stack(int oppidx);
int __gpufreq_fix_custom_freq_volt_stack(unsigned int freq, unsigned int volt);

/**************************************************
 * Function
 **************************************************/
static inline void __gpufreq_footprint_power_step(enum gpufreq_power_step step)
{
#if IS_ENABLED(CONFIG_MTK_AEE_IPANIC) && IS_ENABLED(CONFIG_MTK_AEE_FEATURE)
	aee_rr_rec_gpu_dvfs_vgpu(step);
#else
	GPUFREQ_UNREFERENCED(step);
#endif
}

static inline void __gpufreq_footprint_power_step_reset(void)
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

static inline void __gpufreq_dump_exception(enum gpufreq_exception except_type,
	char *exception_string)
{
#if IS_ENABLED(CONFIG_MTK_AEE_AED)
	if (!exception_string) {
		GPUFREQ_LOGE("null exception string");
		return;
	}

	if (except_type < 0 || except_type >= NUM_EXCEPTION) {
		GPUFREQ_LOGE("invalid exception type");
		return;
	}

	if (aee_get_mode() != AEE_MODE_NOT_INIT) {
		aee_kernel_warning("GPUFREQ", "\n%s\nCRDISPATCH_KEY: %s\n",
			exception_string, g_exception_string[except_type]);
	} else if (!g_have_pending_aee) {
		/*
		 * if AEE driver is not ready when bring up
		 * we will keep the exception log pending.
		 * When GPU power off, we will check if there is pending
		 * log need aee dump and trigger the AEE dump.
		 */
		g_pending_aee.exception_type = except_type;
		strncpy(g_pending_aee.exception_string, exception_string, 1023);
		g_have_pending_aee = true;
	}
#else
	GPUFREQ_UNREFERENCED(except_type);
	GPUFREQ_UNREFERENCED(exception_string);
#endif
}

static inline void __gpufreq_abort(enum gpufreq_exception except_type,
	const char *exception_string, ...)
{
	va_list args;
	int cx = 0;
#ifdef __aarch64__
	char tmp_string[1024];
#else
	char tmp_string[512];
#endif

	va_start(args, exception_string);
	cx = vsnprintf(tmp_string, sizeof(tmp_string), exception_string, args);
	va_end(args);

	GPUFREQ_LOGE("[ABORT]: %s", tmp_string);
	__gpufreq_dump_infra_status();

	if (cx >= 0)
		__gpufreq_dump_exception(except_type, tmp_string);
	BUG_ON(1);
}

/*
 * Check if there is pending exception log
 */
static inline void __gpufreq_check_pending_exception(void)
{
	if (g_have_pending_aee) {
		g_have_pending_aee = false;
		__gpufreq_dump_exception(g_pending_aee.exception_type,
			g_pending_aee.exception_string);
	}
}

#endif /* __GPUFREQ_COMMON_H__ */
