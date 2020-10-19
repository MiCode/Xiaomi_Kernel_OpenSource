/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2019 MediaTek Inc.
 */

#ifndef _PERF_TRACKER_H
#define _PERF_TRACKER_H

#if IS_ENABLED(CONFIG_MTK_GPU_SWPM_SUPPORT)
void perf_update_gpu_counter(unsigned int gpu_pmu[], unsigned int len);
#endif

#endif /* _PERF_TRACKER_H */
