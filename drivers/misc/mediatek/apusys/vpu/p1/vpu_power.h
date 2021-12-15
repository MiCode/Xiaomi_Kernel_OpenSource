// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef __VPU_POWER_H__
#define __VPU_POWER_H__

#include <linux/platform_device.h>
#include "vpu_cfg.h"
#include "vpu_cmn.h"

#define VPU_PWR_NO_BOOST    0xff

int vpu_init_dev_pwr(struct platform_device *pdev, struct vpu_device *vd);
void vpu_exit_dev_pwr(struct platform_device *pdev, struct vpu_device *vd);

int vpu_pwr_up(struct vpu_device *vd, uint8_t boost, uint32_t off_timer);
void vpu_pwr_down(struct vpu_device *vd);

int vpu_pwr_up_locked(struct vpu_device *vd, uint8_t boost, uint32_t off_timer);
void vpu_pwr_down_locked(struct vpu_device *vd);
void vpu_pwr_suspend_locked(struct vpu_device *vd);

int vpu_pwr_get_locked(struct vpu_device *vd, uint8_t boost);
void vpu_pwr_put_locked(struct vpu_device *vd, uint8_t boost);

static inline
int vpu_pwr_get_locked_nb(struct vpu_device *vd)
{
	return vpu_pwr_get_locked(vd, VPU_PWR_NO_BOOST);
}
static inline
void vpu_pwr_put_locked_nb(struct vpu_device *vd)
{
	vpu_pwr_put_locked(vd, VPU_PWR_NO_BOOST);
}


static inline
int vpu_pwr_cnt(struct vpu_device *vd)
{
	return kref_read(&vd->pw_ref);
}

#endif

