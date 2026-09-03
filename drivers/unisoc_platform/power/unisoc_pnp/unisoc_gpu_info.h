/* SPDX-License-Identifier: GPL-2.0 */
//
//Copyright (C) 2016-2024 Unisoc (Shanghai) Technologies Co., Ltd

#ifndef __SPRD_PNP_GPUINFO_H__
#define __SPRD_PNP_GPUINFO_H__

#include "unisoc_pnp_common.h"

extern int unregister_gpufreq_trace_info(struct sprd_pnp_platform_info *pnp_data);
extern ssize_t gpu_en_read(struct file *file, char __user *in_buf, size_t count, loff_t *ppos);
extern ssize_t gpu_en_write(struct file *file, const char __user *user_buf,
				size_t count, loff_t *ppos);
extern int sprd_gpu_freq_proc_open(struct inode *inode, struct file *file);

#endif /* __SPRD_PNP_GPUINFO_H__ */
