/*
 * Copyright (C) 2019 MediaTek Inc.
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

#ifndef __VPU_DEBUG_H__
#define __VPU_DEBUG_H__

#include <linux/printk.h>
#include "vpu_cmn.h"

enum VPU_DEBUG_MASK {
	VPU_DBG_DRV = 0x01,
	VPU_DBG_MEM = 0x02,
	VPU_DBG_ALG = 0x04,
	VPU_DBG_CMD = 0x08,
	VPU_DBG_PMU = 0x10,
	VPU_DBG_PERF = 0x20,
	VPU_DBG_QOS = 0x40,
	VPU_DBG_TIMEOUT = 0x80,
	VPU_DBG_DVFS = 0x100,
};

#ifdef CONFIG_MTK_VPU_DEBUG

extern u32 vpu_klog;
#define vpu_debug(mask, ...) do { if (vpu_klog & mask) \
		pr_debug(__VA_ARGS__); \
	} while (0)

int vpu_init_debug(void);
void vpu_exit_debug(void);
int vpu_init_dev_debug(struct platform_device *pdev, struct vpu_device *dev);
void vpu_exit_dev_debug(struct platform_device *pdev, struct vpu_device *dev);

#else

#define vpu_debug(mask, ...)

static inline
int vpu_init_debug(void) { return 0; }

static inline
void vpu_exit_debug(void) { }

static inline
int vpu_init_dev_debug(struct platform_device *pdev,
	struct vpu_device *dev)
{
	return 0;
}

static inline
void vpu_exit_dev_debug(struct platform_device *pdev,
	struct vpu_device *dev)
{
}

#endif

#define vpu_drv_debug(...) vpu_debug(VPU_DBG_DRV, __VA_ARGS__)
#define vpu_mem_debug(...) vpu_debug(VPU_DBG_MEM, __VA_ARGS__)
#define vpu_cmd_debug(...) vpu_debug(VPU_DBG_CMD, __VA_ARGS__)
#define vpu_alg_debug(...) vpu_debug(VPU_DBG_ALG, __VA_ARGS__)
#define vpu_pmu_debug(...) vpu_debug(VPU_DBG_PMU, __VA_ARGS__)
#define vpu_perf_debug(...) vpu_debug(VPU_DBG_PERF, __VA_ARGS__)
#define vpu_qos_debug(...) vpu_debug(VPU_DBG_QOS, __VA_ARGS__)
#define vpu_timeout_debug(...) vpu_debug(VPU_DBG_TIMEOUT, __VA_ARGS__)
#define vpu_dvfs_debug(...) vpu_debug(VPU_DBG_DVFS, __VA_ARGS__)

#endif

