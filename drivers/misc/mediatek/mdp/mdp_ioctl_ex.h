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

#ifndef __MDP_IOCTL_EX_H__
#define __MDP_IOCTL_EX_H__

#include <linux/kernel.h>
#include <linux/platform_device.h>

int mdp_limit_late_init(void);
int mdp_limit_dev_create(struct platform_device *device);
void mdp_limit_dev_destroy(void);
s32 mdp_ioctl_async_exec(struct file *pf, unsigned long param);
s32 mdp_ioctl_async_wait(unsigned long param);
s32 mdp_ioctl_alloc_readback_slots(void *fp, unsigned long param);
s32 mdp_ioctl_free_readback_slots(void *fp, unsigned long param);
s32 mdp_ioctl_read_readback_slots(unsigned long param);
s32 mdp_ioctl_simulate(unsigned long param);
void mdp_ioctl_free_job_by_node(void *node);
void mdp_ioctl_free_readback_slots_by_node(void *fp);


#if (IS_ENABLED(CONFIG_TRUSTONIC_TEE_SUPPORT) || \
	IS_ENABLED(CONFIG_MICROTRUST_TEE_SUPPORT)) && \
	IS_ENABLED(CONFIG_MTK_TEE_GP_SUPPORT)
#if (IS_ENABLED(CONFIG_MTK_SEC_VIDEO_PATH_SUPPORT) && \
	!IS_ENABLED(CONFIG_MTK_SVP_ON_MTEE_SUPPORT)) || \
	IS_ENABLED(CONFIG_MTK_CAM_SECURITY_SUPPORT) || \
	IS_ENABLED(CONFIG_MTK_GZ_SUPPORT_SDSP)
#define MDP_M4U_TEE_SUPPORT
int m4u_sec_init(void);
#endif
#endif

#if IS_ENABLED(CONFIG_MTK_CAM_GENIEZONE_SUPPORT)
#define MDP_M4U_MTEE_SEC_CAM_SUPPORT
int m4u_gz_sec_init(int mtk_iommu_sec_id);
#endif

#if IS_ENABLED(CONFIG_MTK_SVP_ON_MTEE_SUPPORT)
#define MDP_M4U_MTEE_SVP_SUPPORT
int m4u_gz_sec_init(int mtk_iommu_sec_id);
#endif
#endif				/* __MDP_IOCTL_EX_H__ */
