/*
 * linux/kernel/time/timecounter.c
 *
 * based on code that migrated away from
 * linux/kernel/time/clocksource.c
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

#include <linux/export.h>
#include <linux/timecounter.h>

void timecounter_init(struct timecounter *tc,
		const struct cyclecounter *cc,
		u64 start_tstamp)
{
	tc->cc = cc;
	tc->cycle_last = cc->read(cc);
	tc->nsec = start_tstamp;
}
EXPORT_SYMBOL_GPL(timecounter_init);
static u64 timecounter_read_delta(struct timecounter *tc)
{
	cycle_t cycle_now, cycle_delta;
	u64 ns_offset;

	cycle_now = tc->cc->read(tc->cc);

	cycle_delta = (cycle_now - tc->cycle_last) & tc->cc->mask;

	ns_offset = cyclecounter_cyc2ns(tc->cc, cycle_delta);

	tc->cycle_last = cycle_now;

	return ns_offset;
}

u64 timecounter_read(struct timecounter *tc)
{
	u64 nsec;

	nsec = timecounter_read_delta(tc);
	nsec += tc->nsec;
	tc->nsec = nsec;

	return nsec;
}
EXPORT_SYMBOL_GPL(timecounter_read);

u64 timecounter_cyc2time(struct timecounter *tc,
		cycle_t cycle_tstamp)
{
	u64 cycle_delta = (cycle_tstamp - tc->cycle_last) & tc->cc->mask;
	u64 nsec;

	if (cycle_delta > tc->cc->mask / 2) {
		cycle_delta = (tc->cycle_last - cycle_tstamp) & tc->cc->mask;
		nsec = tc->nsec - cyclecounter_cyc2ns(tc->cc, cycle_delta);
	} else {
		nsec = cyclecounter_cyc2ns(tc->cc, cycle_delta) + tc->nsec;
	}

	return nsec;
}
EXPORT_SYMBOL_GPL(timecounter_cyc2time);
