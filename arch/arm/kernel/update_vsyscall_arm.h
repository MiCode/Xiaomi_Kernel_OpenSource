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

extern void *vectors_page;
extern struct timezone sys_tz;

/*
 * This read-write spinlock protects us from races in SMP while
 * updating the kernel user helper-embedded time.
 */
__cacheline_aligned_in_smp DEFINE_SEQLOCK(kuh_time_lock);
