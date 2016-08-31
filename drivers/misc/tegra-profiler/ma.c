/*
 * drivers/misc/tegra-profiler/ma.c
 *
 * Copyright (c) 2014, NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/module.h>
#include <linux/mm.h>
#include <linux/timer.h>
#include <linux/sched.h>

#include <linux/tegra_profiler.h>

#include "ma.h"
#include "quadd.h"
#include "hrt.h"
#include "comm.h"
#include "debug.h"

static void make_sample(struct quadd_hrt_ctx *hrt_ctx,
			pid_t pid, unsigned long vm_size,
			unsigned long rss_size)
{
	struct quadd_record_data record;
	struct quadd_ma_data *ma = &record.ma;
	struct quadd_comm_data_interface *comm = hrt_ctx->quadd_ctx->comm;

	record.record_type = QUADD_RECORD_TYPE_MA;

	ma->pid = pid;
	ma->time = quadd_get_time();

	ma->vm_size = vm_size << (PAGE_SHIFT-10);
	ma->rss_size = rss_size << (PAGE_SHIFT-10);

	comm->put_sample(&record, NULL, 0);
}

static void check_ma(struct quadd_hrt_ctx *hrt_ctx)
{
	pid_t pid;
	struct pid *pid_s;
	struct task_struct *task = NULL;
	struct mm_struct *mm;
	struct quadd_ctx *quadd_ctx = hrt_ctx->quadd_ctx;
	unsigned long vm_size, rss_size;

	pid = quadd_ctx->param.pids[0];

	rcu_read_lock();
	pid_s = find_vpid(pid);
	if (pid_s)
		task = pid_task(pid_s, PIDTYPE_PID);
	rcu_read_unlock();
	if (!task)
		return;

	mm = task->mm;
	if (!mm)
		return;

	vm_size = mm->total_vm;
	rss_size = get_mm_rss(mm);

	if (vm_size != hrt_ctx->vm_size_prev ||
	    rss_size != hrt_ctx->rss_size_prev) {
		make_sample(hrt_ctx, pid, vm_size, rss_size);
		hrt_ctx->vm_size_prev = vm_size;
		hrt_ctx->rss_size_prev = rss_size;
	}
}

static void timer_interrupt(unsigned long data)
{
	struct quadd_hrt_ctx *hrt_ctx = (struct quadd_hrt_ctx *)data;
	struct timer_list *timer = &hrt_ctx->ma_timer;

	if (hrt_ctx->active == 0)
		return;

	check_ma(hrt_ctx);

	timer->expires = jiffies + msecs_to_jiffies(hrt_ctx->ma_period);
	add_timer(timer);
}

void quadd_ma_start(struct quadd_hrt_ctx *hrt_ctx)
{
	struct timer_list *timer = &hrt_ctx->ma_timer;

	if (hrt_ctx->ma_period == 0) {
		pr_info("QuadD MA is disabled\n");
		return;
	}
	pr_info("QuadD MA is started, interval: %u msec\n",
		hrt_ctx->ma_period);

	hrt_ctx->vm_size_prev = 0;
	hrt_ctx->rss_size_prev = 0;

	init_timer(timer);
	timer->function = timer_interrupt;
	timer->expires = jiffies + msecs_to_jiffies(hrt_ctx->ma_period);
	timer->data = (unsigned long)hrt_ctx;
	add_timer(timer);
}

void quadd_ma_stop(struct quadd_hrt_ctx *hrt_ctx)
{
	if (hrt_ctx->ma_period > 0) {
		pr_info("QuadD MA is stopped\n");
		del_timer_sync(&hrt_ctx->ma_timer);
	}
}
