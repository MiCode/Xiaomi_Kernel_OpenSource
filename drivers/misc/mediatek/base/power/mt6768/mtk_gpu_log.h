/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
*/

#ifndef _MT_GPU_LOG_H_
#define _MT_GPU_LOG_H_

#include <ged_log.h>
#include "mtk_gpufreq_core.h"

extern GED_LOG_BUF_HANDLE _mtk_gpu_log_hnd;

#if 0
#define GPULOG(fmt, ...) \
	ged_log_buf_print2(_mtk_gpu_log_hnd, \
		GED_LOG_ATTR_TIME, fmt, ##__VA_ARGS__)
#define GPULOG2(fmt, ...) do { \
	ged_log_buf_print2(_mtk_gpu_log_hnd, \
		GED_LOG_ATTR_TIME, fmt, ##__VA_ARGS__); \
	gpufreq_pr_info(fmt "\n", ##__VA_ARGS__); \
} while (0)
#endif

#define GPULOG(fmt, ...) do { } while (0)
#define GPULOG2(fmt, ...) do { } while (0)

void mtk_gpu_log_init(void);
void mtk_gpu_log_trigger_aee(const char *msg);

#endif

