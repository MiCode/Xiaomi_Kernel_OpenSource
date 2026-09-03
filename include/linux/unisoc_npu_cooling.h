/* SPDX-License-Identifier: GPL-2.0
 * Copyright (C) 2020 Spreadtrum Communications Inc.
 */

#ifndef _UNISOC_NPU_COOLING_H_
#define _UNISOC_NPU_COOLING_H_

#if IS_ENABLED(CONFIG_UNISOC_NPU_COOLING_DEVICE)
int npu_cooling_device_register(struct devfreq *npudev);
int npu_cooling_device_unregister(void);
#else
static inline int npu_cooling_device_register(struct devfreq *npudev)
{
	return 0;
}
static inline int npu_cooling_device_unregister(void)
{
	return 0;
}
#endif

#endif
