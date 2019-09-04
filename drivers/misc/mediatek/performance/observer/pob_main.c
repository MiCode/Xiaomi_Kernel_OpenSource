/*
 * Copyright (C) 2018 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#include <linux/ktime.h>
#include <linux/hrtimer.h>
#include <linux/sched.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/rbtree.h>
#include <linux/preempt.h>
#include <linux/proc_fs.h>
#include <linux/trace_events.h>
#include <linux/debugfs.h>
#include <linux/spinlock.h>
#include <linux/vmalloc.h>
#include <linux/module.h>

#include <asm/page.h>
#include <linux/vmalloc.h>
#include <linux/slab.h>
#include <linux/mm.h>

#define CREATE_TRACE_POINTS
#include <trace/events/pob.h>

#include "pob_int.h"

#define TRACELOG_SIZE 512

void *pob_alloc_atomic(int i32Size)
{
	void *pvBuf;

	if (i32Size <= PAGE_SIZE)
		pvBuf = kmalloc(i32Size, GFP_ATOMIC);
	else
		pvBuf = vmalloc(i32Size);

	return pvBuf;
}

void pob_free(void *pvBuf)
{
	kvfree(pvBuf);
}

void pob_trace(const char *fmt, ...)
{
	char log[TRACELOG_SIZE];
	va_list args;

	va_start(args, fmt);
	vsnprintf(log, sizeof(log), fmt, args);
	va_end(args);

	preempt_disable();
	trace_pob_log(log);
	preempt_enable();
}

static int __init pob_init(void)
{
	struct dentry *pob_debugfs_dir = NULL;

	pob_debugfs_dir = debugfs_create_dir("pob", NULL);

	if (!pob_debugfs_dir)
		return -ENODEV;

	pob_qos_init(pob_debugfs_dir);

	return 0;
}

static void __exit pob_exit(void)
{

}

module_init(pob_init);
module_exit(pob_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("MediaTek Performance Observer");
MODULE_AUTHOR("MediaTek Inc.");

