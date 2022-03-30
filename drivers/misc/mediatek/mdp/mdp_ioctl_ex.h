/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef __MDP_IOCTL_EX_H__
#define __MDP_IOCTL_EX_H__

#include <linux/kernel.h>
#include <linux/platform_device.h>

extern int gMdpRegMSBSupport;

s32 mdp_ioctl_async_exec(struct file *pf, unsigned long param);
s32 mdp_ioctl_async_wait(unsigned long param);
s32 mdp_ioctl_alloc_readback_slots(void *fp, unsigned long param);
s32 mdp_ioctl_free_readback_slots(void *fp, unsigned long param);
s32 mdp_ioctl_read_readback_slots(unsigned long param);
s32 mdp_ioctl_simulate(unsigned long param);
void mdp_ioctl_free_job_by_node(void *node);
void mdp_ioctl_free_readback_slots_by_node(void *fp);
int mdp_limit_dev_create(struct platform_device *device);
void mdpsyscon_init(void);
void mdpsyscon_deinit(void);

#endif				/* __MDP_IOCTL_EX_H__ */
