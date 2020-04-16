/*
 * Copyright (C) 2018 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */


#ifndef _PERF_TRACKER_INTERNAL_H
#define _PERF_TRACKER_INTERNAL_H

#ifdef CONFIG_MTK_GPU_SWPM_SUPPORT
extern void MTKGPUPower_model_start(unsigned int interval_ns);
extern void MTKGPUPower_model_stop(void);
extern void MTKGPUPower_model_suspend(void);
extern void MTKGPUPower_model_resume(void);
void perf_update_gpu_counter(unsigned int gpu_pmu[], unsigned int len);
#endif

#endif /* _PERF_TRACKER_INTERNAL_H */
