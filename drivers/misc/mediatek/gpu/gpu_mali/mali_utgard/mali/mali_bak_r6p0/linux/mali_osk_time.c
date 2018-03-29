/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2008-2010, 2013-2015 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

/**
 * @file mali_osk_time.c
 * Implementation of the OS abstraction layer for the kernel device driver
 */

#include "mali_osk.h"
#include <linux/jiffies.h>
#include <linux/time.h>
#include <asm/delay.h>

mali_bool _mali_osk_time_after_eq(unsigned long ticka, unsigned long tickb)
{
	return time_after_eq(ticka, tickb) ?
	       MALI_TRUE : MALI_FALSE;
}

unsigned long _mali_osk_time_mstoticks(u32 ms)
{
	return msecs_to_jiffies(ms);
}

u32 _mali_osk_time_tickstoms(unsigned long ticks)
{
	return jiffies_to_msecs(ticks);
}

unsigned long _mali_osk_time_tickcount(void)
{
	return jiffies;
}

void _mali_osk_time_ubusydelay(u32 usecs)
{
	udelay(usecs);
}

u64 _mali_osk_time_get_ns(void)
{
	struct timespec tsval;
	getnstimeofday(&tsval);
	return (u64)timespec_to_ns(&tsval);
}

u64 _mali_osk_boot_time_get_ns(void)
{
	struct timespec tsval;
	get_monotonic_boottime(&tsval);
	return (u64)timespec_to_ns(&tsval);
}
