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
	VPU_DBG_PWR = 0x10,
	VPU_DBG_PEF = 0x20,
	VPU_DBG_MET = 0x40,
};

#ifdef CONFIG_MTK_APUSYS_VPU_DEBUG
extern u32 vpu_klog;

static inline
int vpu_debug_on(int mask)
{
	return (vpu_klog & mask);
}

#define vpu_debug(mask, ...) do { if (vpu_debug_on(mask)) \
		pr_info(__VA_ARGS__); \
	} while (0)

int vpu_init_debug(void);
void vpu_exit_debug(void);
int vpu_init_dev_debug(struct platform_device *pdev, struct vpu_device *vd);
void vpu_exit_dev_debug(struct platform_device *pdev, struct vpu_device *vd);
int vpu_mesg_seq(struct seq_file *s, struct vpu_device *vd);
int vpu_debug_state_seq(struct seq_file *s, uint32_t vs, uint32_t ds, int b);
int vpu_debug_cmd_seq(struct seq_file *s, struct vpu_device *vd, int prio,
	int prio_max, int active, struct vpu_cmd_ctl *c, uint64_t timeout);
const char *vpu_debug_cmd_str(int cmd);
void vpu_seq_time(struct seq_file *s, uint64_t t);
void vpu_seq_boost(struct seq_file *s, int boost);

#else

#define vpu_debug(mask, ...)

static inline
int vpu_init_debug(void) { return 0; }

static inline
void vpu_exit_debug(void) { }

static inline
int vpu_init_dev_debug(struct platform_device *pdev,
	struct vpu_device *vd)
{
	return 0;
}

static inline
void vpu_exit_dev_debug(struct platform_device *pdev,
	struct vpu_device *vd)
{
}

static inline
int vpu_debug_on(int mask)
{
	return 0;
}

static inline
int vpu_mesg_seq(struct seq_file *s, struct vpu_device *vd)
{
	return 0;
}

static inline
int vpu_debug_state_seq(struct seq_file *s, uint32_t vs, uint32_t ds, int b)
{
	return 0;
}

static inline
int vpu_debug_cmd_seq(struct seq_file *s, struct vpu_device *vd, int prio,
	int prio_max, int active, struct vpu_cmd_ctl *c, uint64_t timeout)
{
	return 0;
}

static inline
const char *vpu_debug_cmd_str(int cmd)
{
	return "";
}

static inline
void vpu_seq_time(struct seq_file *s, uint64_t t)
{
}

static inline
void vpu_seq_boost(struct seq_file *s, int boost)
{
}

#endif

#define vpu_drv_debug(...) vpu_debug(VPU_DBG_DRV, __VA_ARGS__)
#define vpu_mem_debug(...) vpu_debug(VPU_DBG_MEM, __VA_ARGS__)
#define vpu_cmd_debug(...) vpu_debug(VPU_DBG_CMD, __VA_ARGS__)
#define vpu_alg_debug(...) vpu_debug(VPU_DBG_ALG, __VA_ARGS__)
#define vpu_pwr_debug(...) vpu_debug(VPU_DBG_PWR, __VA_ARGS__)
#define vpu_pef_debug(...) vpu_debug(VPU_DBG_PEF, __VA_ARGS__)
#define vpu_met_debug(...) vpu_debug(VPU_DBG_MET, __VA_ARGS__)

#endif

