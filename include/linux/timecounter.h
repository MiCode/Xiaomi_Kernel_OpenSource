/*
 * linux/include/linux/timecounter.h
 *
 * based on code that migrated away from
 * linux/include/linux/clocksource.h
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#ifndef _LINUX_TIMECOUNTER_H
#define _LINUX_TIMECOUNTER_H
#include <linux/types.h>
struct cyclecounter {
	cycle_t (*read)(const struct cyclecounter *cc);
	cycle_t mask;
	u32 mult;
	u32 shift;
};
struct timecounter {
	const struct cyclecounter *cc;
	cycle_t cycle_last;
	u64 nsec;
};
static inline u64 cyclecounter_cyc2ns(const struct cyclecounter *cc,
				      cycle_t cycles)
{
	u64 ret = (u64)cycles;
	ret = (ret * cc->mult) >> cc->shift;
	return ret;
}
extern void timecounter_init(struct timecounter *tc,
			     const struct cyclecounter *cc,
			     u64 start_tstamp);
extern u64 timecounter_read(struct timecounter *tc);
extern u64 timecounter_cyc2time(struct timecounter *tc,
				cycle_t cycle_tstamp);
#endif
