/* Copyright (c) 2012, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/export.h>
#include <linux/clocksource.h>
#include <linux/time.h>
#include "update_vsyscall_arm.h"
/*
 * See entry-armv.S for the offsets into the kernel user helper for
 * these fields.
 */
#define ARM_VSYSCALL_TIMER_TZ			0xf20
#define ARM_VSYSCALL_TIMER_SEQ			0xf28
#define ARM_VSYSCALL_TIMER_OFFSET		0xf30
#define ARM_VSYSCALL_TIMER_WTM_TV_SEC		0xf38
#define ARM_VSYSCALL_TIMER_WTM_TV_NSEC		0xf3c
#define ARM_VSYSCALL_TIMER_CYCLE_LAST		0xf40
#define ARM_VSYSCALL_TIMER_MASK			0xf48
#define ARM_VSYSCALL_TIMER_MULT			0xf50
#define ARM_VSYSCALL_TIMER_SHIFT		0xf54
#define ARM_VSYSCALL_TIMER_TV_SEC		0xf58
#define ARM_VSYSCALL_TIMER_TV_NSEC		0xf5c

struct kernel_gtod_t {
	u64  cycle_last;
	u64  mask;
	u32  mult;
	u32  shift;
	u32  tv_sec;
	u32  tv_nsec;
};

struct kernel_tz_t {
	u32  tz_minuteswest;
	u32  tz_dsttime;
};

struct kernel_wtm_t {
	u32  tv_sec;
	u32  tv_nsec;
};

/*
 * Updates the kernel user helper area with the current timespec
 * data, as well as additional fields needed to calculate
 * gettimeofday, clock_gettime, etc.
 */
void
update_vsyscall(struct timespec *ts, struct timespec *wtm,
	struct clocksource *c, u32 mult)
{
	unsigned long vectors = (unsigned long)vectors_page;
	unsigned long flags;
	unsigned *seqnum = (unsigned *)(vectors + ARM_VSYSCALL_TIMER_SEQ);
	struct kernel_gtod_t *dgtod = (struct kernel_gtod_t *)(vectors +
		ARM_VSYSCALL_TIMER_CYCLE_LAST);
	struct kernel_wtm_t *dgwtm = (struct kernel_wtm_t *)(vectors +
		ARM_VSYSCALL_TIMER_WTM_TV_SEC);

	write_seqlock_irqsave(&kuh_time_lock, flags);
	*seqnum = kuh_time_lock.sequence;
	dgtod->cycle_last = c->cycle_last;
	dgtod->mask = c->mask;
	dgtod->mult = c->mult;
	dgtod->shift = c->shift;
	dgtod->tv_sec = ts->tv_sec;
	dgtod->tv_nsec = ts->tv_nsec;
	dgwtm->tv_sec = wtm->tv_sec;
	dgwtm->tv_nsec = wtm->tv_nsec;
	*seqnum = kuh_time_lock.sequence + 1;
	write_sequnlock_irqrestore(&kuh_time_lock, flags);
}
EXPORT_SYMBOL(update_vsyscall);

void
update_vsyscall_tz(void)
{
	unsigned long vectors = (unsigned long)vectors_page;
	unsigned long flags;
	unsigned *seqnum = (unsigned *)(vectors + ARM_VSYSCALL_TIMER_SEQ);
	struct kernel_tz_t *dgtod = (struct kernel_tz_t *)(vectors +
		ARM_VSYSCALL_TIMER_TZ);

	write_seqlock_irqsave(&kuh_time_lock, flags);
	*seqnum = kuh_time_lock.sequence;
	dgtod->tz_minuteswest = sys_tz.tz_minuteswest;
	dgtod->tz_dsttime = sys_tz.tz_dsttime;
	*seqnum = kuh_time_lock.sequence + 1;
	write_sequnlock_irqrestore(&kuh_time_lock, flags);
}
EXPORT_SYMBOL(update_vsyscall_tz);
