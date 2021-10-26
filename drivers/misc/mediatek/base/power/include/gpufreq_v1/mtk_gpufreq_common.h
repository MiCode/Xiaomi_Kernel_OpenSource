/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef __MTK_GPUFREQ_COMMON_H__
#define __MTK_GPUFREQ_COMMON_H__

// GPU_OPP_PTPOD_SLOPE's owner is ptpod driver
enum g_exception_enum  {
	GPU_FREQ_EXCEPTION,
	GPU_DFD_PROBE_TRIGGERED,
	GPU_OPP_PTPOD_SLOPE,
};

static const char * const g_exception_string[] = {
	"GPU_FREQ_EXCEPTION",
	"GPU_DFD_PROBE_TRIGGERED",
	"GPU_OPP_PTPOD_SLOPE",
};

struct gpu_assert_info {
	enum g_exception_enum exception_type;
	char exception_string[1024];
};

// to check if aee service is running
#ifdef CONFIG_MTK_AEE_AED
extern int aee_mode;
#endif

void gpu_assert(bool cond, enum g_exception_enum except_type,
	const char *except_str, ...);
void check_pending_info(void);

#endif /* __MTK_GPUFREQ_COMMON_H__ */

