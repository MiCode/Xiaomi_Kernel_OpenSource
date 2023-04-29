/*
 * Copyright (c) Xiaomi Technologies Co., Ltd. 2021. All rights reserved.
 * Copyright (C) 2021 XiaoMi, Inc.
 *
 * File name: turbo_sched.c
 * Descrviption: turbo sched
 * Author: guchao1@xiaomi.com
 * Version: 3.0
 * Date:  2021/05/01
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#define pr_fmt(fmt) "turbo-sched: " fmt

#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/uaccess.h>
#include <linux/sched/signal.h>
#include <trace/hooks/sched.h>
#include <trace/hooks/cgroup.h>
#include <linux/kthread.h>
#include <linux/cpu.h>
#include <linux/sched.h>
#include <linux/moduleparam.h>
#include <linux/security.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/cpumask.h>
#include <linux/cpuset.h>
#include <linux/gfp.h>
#include <linux/cred.h>
#include <linux/pkg_stat.h>
#include <linux/lsm_hooks.h>
#include "../include/mi_module.h"

unsigned int tsched_debug;
module_param(tsched_debug, uint, 0644);

void mi_cpuset_fork(void *nouse, struct task_struct *tsk, int *forbidden)
{
	if (!tsched_debug)
		return;

	pr_info ("forbidden : %d", forbidden);
}

void mi_sched_setaffinity(void *nouse, struct task_struct *p,
		const struct cpumask * in_mask, int *skip)
{
	if (!tsched_debug)
		return;

	printk("debug_setaffinity: current : %s %d %d, setaffinity target : %s %d %d\n",
			current->comm, task_uid(current).val ,current->pid,
			p->comm, task_uid(p).val ,p->pid);
	printk("debug_setaffinity: cpumask is : %d \n", *in_mask);
}

void mi_set_cpus_allowed_comm(void *nouse, struct task_struct *p,
		const struct cpumask * in_mask)
{
	if (!tsched_debug)
		return;

	pr_info ("p->pid : %d", p->pid);
}

static int __init turbo_sched_init(void)
{
	register_trace_android_rvh_cpuset_fork(mi_cpuset_fork, NULL);
	register_trace_android_vh_sched_setaffinity_early(mi_sched_setaffinity, NULL);
	register_trace_android_rvh_set_cpus_allowed_comm(mi_set_cpus_allowed_comm, NULL);

	return 0;
}

static void __exit turbo_sched_exit(void)
{
	return;
}

module_init(turbo_sched_init);
module_exit(turbo_sched_exit);
MODULE_LICENSE("GPL");
