/* SPDX-License-Identifier: GPL-2.0 */
//
//Copyright (C) 2016-2024 Unisoc (Shanghai) Technologies Co., Ltd

#ifndef __SPRD_PNP_CPUIDLE_H__
#define __SPRD_PNP_CPUIDLE_H__

#include "unisoc_pnp_common.h"

extern int register_cpuidle_trace_info(struct sprd_pnp_platform_info *pnp_data);
extern int unregister_cpuidle_trace_info(struct sprd_pnp_platform_info *pnp_data);
extern int sprd_cpu_idle_proc_open(struct inode *inode, struct file *file);

#endif /* __SPRD_PNP_CPUINFO_H__ */
