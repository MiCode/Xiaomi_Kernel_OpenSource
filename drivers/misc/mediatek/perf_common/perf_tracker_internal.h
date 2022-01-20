/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2019 MediaTek Inc.
 */


#ifndef _PERF_TRACKER_INTERNAL_H
#define _PERF_TRACKER_INTERNAL_H

#ifdef CONFIG_MTK_GPU_SWPM_SUPPORT
extern void MTKGPUPower_model_start(unsigned int interval_ns);
extern void MTKGPUPower_model_stop(void);
extern void MTKGPUPower_model_suspend(void);
extern void MTKGPUPower_model_resume(void);
void perf_update_gpu_counter(unsigned int gpu_pmu[], unsigned int len) { };
#endif

#endif /* _PERF_TRACKER_INTERNAL_H */
