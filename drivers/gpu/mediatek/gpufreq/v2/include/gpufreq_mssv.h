/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2021 MediaTek Inc.
 */

#ifndef __GPUFREQ_MSSV_H__
#define __GPUFREQ_MSSV_H__

/**************************************************
 * Definition
 **************************************************/
#define GPUFREQ_MSSV_TEST_MODE          (0)

/**************************************************
 * Enumeration
 **************************************************/
enum gpufreq_mssv_target {
	TARGET_MSSV_FGPU = 0,
	TARGET_MSSV_VGPU,
	TARGET_MSSV_FSTACK,
	TARGET_MSSV_VSTACK,
	TARGET_MSSV_VSRAM,
	TARGET_MSSV_STACK_SEL,
	TARGET_MSSV_DEL_SEL,
	TARGET_MSSV_INVALID,
};

/**************************************************
 * Structure
 **************************************************/


#endif /* __GPUFREQ_MSSV_H__ */
