/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2015 MediaTek Inc.
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
void mdp_ioctl_free_readback_slots_by_node(void *fp);

#if IS_ENABLED(CONFIG_TRUSTONIC_TEE_SUPPORT) || IS_ENABLED(CONFIG_MICROTRUST_TEE_SUPPORT)
int m4u_sec_init(void);
#endif
#if defined(CONFIG_MTK_CAM_GENIEZONE_SUPPORT)
int m4u_gz_sec_init(int mtk_iommu_sec_id);
#endif

#endif				/* __MDP_IOCTL_EX_H__ */
