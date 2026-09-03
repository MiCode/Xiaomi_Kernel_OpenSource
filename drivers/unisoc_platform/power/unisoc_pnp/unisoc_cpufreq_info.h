/* SPDX-License-Identifier: GPL-2.0 */
//
//Copyright (C) 2016-2024 Unisoc (Shanghai) Technologies Co., Ltd

#ifndef __SPRD_PNP_CPUINFO_H__
#define __SPRD_PNP_CPUINFO_H__

#include "unisoc_pnp_common.h"

extern ssize_t cpu_en_read(struct file *file, char __user *in_buf, size_t count, loff_t *ppos);
extern ssize_t cpu_en_write(struct file *file, const char __user *user_buf,
								size_t count, loff_t *ppos);
extern __poll_t cpu_en_poll(struct file *file, poll_table *wait);
extern int unregister_cpufreq_notify_info(struct sprd_pnp_platform_info *pnp_data);
extern int unregister_cpuidle_trace_info(struct sprd_pnp_platform_info *pnp_data);
extern int sprd_cpu_freq_proc_open(struct inode *inode, struct file *file);
extern int sprd_cpu_idle_proc_open(struct inode *inode, struct file *file);

#endif /* __SPRD_PNP_CPUINFO_H__ */
