/* SPDX-License-Identifier: GPL-2.0
 * Copyright (C) 2020 Spreadtrum Communications Inc.
 */

#ifndef _UNISOC_GPU_COOLING_H_
#define _UNISOC_GPU_COOLING_H_

#if IS_ENABLED(CONFIG_UNISOC_GPU_COOLING_DEVICE)
int create_gpu_cooling_device(struct devfreq *gpudev, u64 *mask);
int destroy_gpu_cooling_device(void);
#else
static inline int create_gpu_cooling_device(struct devfreq *gpudev, u64 *mask)
{
	return 0;
}
static inline int destroy_gpu_cooling_device(void)
{
	return 0;
}
#endif

#endif
