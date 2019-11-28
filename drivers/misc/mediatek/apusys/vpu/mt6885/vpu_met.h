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

#ifndef __VPU_MET_H__
#define __VPU_MET_H__

#include "vpu_cmn.h"

#ifdef CONFIG_MTK_APUSYS_VPU_DEBUG
int vpu_init_dev_met(struct platform_device *pdev,
	struct vpu_device *vd);
void vpu_exit_dev_met(struct platform_device *pdev,
	struct vpu_device *vd);
int vpu_met_set_ftrace(struct vpu_device *vd);
void vpu_met_isr(struct vpu_device *vd);
#else
static inline
int vpu_init_dev_met(struct platform_device *pdev,
	struct vpu_device *vd)
{
	return 0;
}
static inline
void vpu_exit_dev_met(struct platform_device *pdev,
	struct vpu_device *vd)
{
}
static inline
int vpu_met_set_ftrace(struct vpu_device *vd)
{
	return 0;
}
static inline
void vpu_met_isr(struct vpu_device *vd)
{
}
#endif

#endif
