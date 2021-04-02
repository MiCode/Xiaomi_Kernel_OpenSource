/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2021 MediaTek Inc.
 */

#ifndef __MTK_GPUFREQ_COMMON_H__
#define __MTK_GPUFREQ_COMMON_H__

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

/**************************************************
 * Register Manipulations
 **************************************************/
#define READ_REGISTER_UINT32(reg) \
	(*(unsigned int * const)(reg))
#define WRITE_REGISTER_UINT32(reg, val) \
	((*(unsigned int * const)(reg)) = (val))
#define INREG32(x) \
	READ_REGISTER_UINT32((unsigned int *)((void *)(x)))
#define OUTREG32(x, y) \
	WRITE_REGISTER_UINT32((unsigned int *)((void *)(x)), (unsigned int)(y))
#define SETREG32(x, y) \
	OUTREG32(x, INREG32(x)|(y))
#define CLRREG32(x, y) \
	OUTREG32(x, INREG32(x)&~(y))
#define MASKREG32(x, y, z) \
	OUTREG32(x, (INREG32(x)&~(y))|(z))
#define DRV_Reg32(addr)             INREG32(addr)
#define DRV_WriteReg32(addr, data)  OUTREG32(addr, data)
#define DRV_SetReg32(addr, data)    SETREG32(addr, data)
#define DRV_ClrReg32(addr, data)    CLRREG32(addr, data)

/**************************************************
 * Misc. Definition
 **************************************************/
#define GPUFREQ_UNREFERENCED(param) ((void)(param))

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

#endif /* __MTK_GPUFREQ_COMMON_H__ */
