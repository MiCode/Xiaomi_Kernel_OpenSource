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

#ifndef _MT_GPU_LOG_H_
#define _MT_GPU_LOG_H_

#include <ged_log.h>
#include "mtk_gpufreq_core.h"

extern GED_LOG_BUF_HANDLE _mtk_gpu_log_hnd;

#define GPULOG(fmt, ...) ged_log_buf_print2(_mtk_gpu_log_hnd, \
	GED_LOG_ATTR_TIME, fmt, ##__VA_ARGS__)
#define GPULOG2(fmt, ...) do { \
	ged_log_buf_print2(_mtk_gpu_log_hnd, \
	GED_LOG_ATTR_TIME, fmt, ##__VA_ARGS__); \
	gpufreq_pr_info(fmt "\n", ##__VA_ARGS__); \
} while (0)

void mtk_gpu_log_init(void);
void mtk_gpu_log_trigger_aee(const char *msg);

#endif

