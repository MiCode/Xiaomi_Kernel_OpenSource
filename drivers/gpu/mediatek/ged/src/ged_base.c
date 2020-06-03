// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include "ged_base.h"
#include <asm/page.h>
#include <linux/version.h>
#include <linux/vmalloc.h>
#include <linux/slab.h>
#include <linux/sched/clock.h>
#include <linux/interrupt.h>

#include <linux/uaccess.h>

unsigned long ged_copy_to_user(void __user *pvTo, const void *pvFrom,
	unsigned long ulBytes)
{
	if (access_ok(pvTo, ulBytes))
		return __copy_to_user(pvTo, pvFrom, ulBytes);
	return ulBytes;
}

unsigned long ged_copy_from_user(void *pvTo, const void __user *pvFrom,
	unsigned long ulBytes)
{
	if (access_ok(pvFrom, ulBytes))
		return __copy_from_user(pvTo, pvFrom, ulBytes);
	return ulBytes;
}

void *ged_alloc(int i32Size)
{
	void *pvBuf;

	if (i32Size <= PAGE_SIZE)
		pvBuf = kmalloc(i32Size, GFP_KERNEL);
	else
		pvBuf = vmalloc(i32Size);

	return pvBuf;
}

void *ged_alloc_atomic(int i32Size)
{
	void *pvBuf;

	if (i32Size <= PAGE_SIZE)
		pvBuf = kmalloc(i32Size, GFP_ATOMIC);
	else
		pvBuf = vmalloc(i32Size);

	return pvBuf;
}

void ged_free(void *pvBuf, int i32Size)
{
	if (pvBuf) {
		if (i32Size <= PAGE_SIZE)
			kfree(pvBuf);
		else
			vfree(pvBuf);
	}
}

long ged_get_pid(void)
{
	if (in_interrupt())
		return 0xffffffffL;
	return (long)task_tgid_nr(current);
}

unsigned long long ged_get_time(void)
{
	unsigned long long temp;

	preempt_disable();
	temp = cpu_clock(smp_processor_id());
	preempt_enable();

	return temp;
}

